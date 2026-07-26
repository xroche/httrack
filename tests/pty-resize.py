#!/usr/bin/env python3
"""Drive httrack on a pty and check the animated display follows the terminal.

Usage: pty-resize.py <capture-file> <long-url-prefix> -- <cmd> [args...]
"""
import fcntl
import os
import pty
import select
import signal
import struct
import subprocess
import sys
import termios
import time

CLEAR = b"\033[2J"
FRAME = b"\033[1;1f"  # every refresh homes here before painting the stats
LIVE = b"Bytes saved"


class Term:
    def __init__(self, argv, capture):
        self.capture = capture
        self.buf = bytearray()
        self.master, slave = pty.openpty()
        self.resize(24, 80)
        self.proc = subprocess.Popen(
            argv, stdin=slave, stdout=slave, stderr=slave, start_new_session=True
        )
        os.close(slave)

    def resize(self, rows, cols):
        fcntl.ioctl(
            self.master, termios.TIOCSWINSZ, struct.pack("HHHH", rows, cols, 0, 0)
        )

    def _read(self, timeout):
        ready, _, _ = select.select([self.master], [], [], timeout)
        if not ready:
            return b""
        try:
            return os.read(self.master, 65536)
        except OSError:
            return b""

    def pump(self, seconds):
        """Read for the given window; return only the bytes read during it."""
        mark = len(self.buf)
        end = time.monotonic() + seconds
        while time.monotonic() < end:
            self.buf += self._read(0.1)
        return bytes(self.buf[mark:])

    def wait_for(self, pattern, timeout):
        mark = len(self.buf)
        end = time.monotonic() + timeout
        while time.monotonic() < end:
            self.buf += self._read(0.1)
            if pattern in bytes(self.buf[mark:]):
                return True
            if self.proc.poll() is not None:
                break
        return False

    def alive(self):
        return self.proc.poll() is None

    def close(self):
        if self.alive():
            os.killpg(self.proc.pid, signal.SIGKILL)
        self.proc.wait()
        os.close(self.master)
        with open(self.capture, "wb") as fp:
            fp.write(self.buf)


def frame_heights(chunk):
    """Newline count of each complete frame in the chunk."""
    return [f.count(b"\n") for f in chunk.split(FRAME)[1:-1]]


def main():
    capture, long_url = sys.argv[1], sys.argv[2].encode()
    argv = sys.argv[sys.argv.index("--") + 1 :]
    term = Term(argv, capture)
    failures = []

    def check(cond, msg):
        if not cond:
            failures.append(msg)
        return cond

    try:
        if not check(term.wait_for(LIVE, 15), "the display never started"):
            return 1

        # Steady state: a repaint here would mask any resize-driven one below.
        idle = term.pump(1.0)
        check(CLEAR not in idle, "repaint without a resize")
        check(long_url not in idle, "long URL not truncated at 80 columns")

        term.resize(24, 120)
        check(term.wait_for(CLEAR, 3), "no repaint after a width-only resize")
        check(CLEAR not in term.pump(1.0), "repaint kept firing after the resize")

        term.resize(10, 120)
        check(term.wait_for(CLEAR, 3), "no repaint after a height-only resize")
        heights = frame_heights(term.pump(1.0))
        check(heights, "no frame after the height change")
        check(
            all(h <= 9 for h in heights),
            "frame overflows a 10-row terminal: %s" % heights,
        )

        term.resize(24, 200)
        check(term.wait_for(CLEAR, 3), "no repaint after growing the terminal")
        check(long_url in term.pump(1.0), "long URL still truncated at 200 columns")

        check(term.alive(), "httrack died mid-run (exit %s)" % term.proc.poll())
    finally:
        term.close()

    for msg in failures:
        print("FAIL: %s" % msg)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
