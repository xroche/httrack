#!/usr/bin/env python3
"""Run a command on a pty, resize the pty mid-run, check the display repaints.

Usage: pty-resize.py <resize-after-s> <deadline-s> <capture-file> <cmd> [args...]
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
RUNNING = b"Bytes saved"


def set_size(fd, rows, cols):
    fcntl.ioctl(fd, termios.TIOCSWINSZ, struct.pack("HHHH", rows, cols, 0, 0))


def main():
    resize_after = float(sys.argv[1])
    deadline = float(sys.argv[2])
    capture = sys.argv[3]
    argv = sys.argv[4:]

    master, slave = pty.openpty()
    set_size(master, 24, 80)
    proc = subprocess.Popen(
        argv, stdin=slave, stdout=slave, stderr=slave, start_new_session=True
    )
    os.close(slave)

    buf = bytearray()
    resized_at = None
    start = time.monotonic()
    while True:
        now = time.monotonic() - start
        if now > deadline:
            break
        if resized_at is None and now >= resize_after:
            set_size(master, 40, 100)
            resized_at = len(buf)
        ready, _, _ = select.select([master], [], [], 0.2)
        if ready:
            try:
                data = os.read(master, 65536)
            except OSError:
                break
            if not data:
                break
            buf += data
        elif proc.poll() is not None:
            break

    if proc.poll() is None:
        os.killpg(proc.pid, signal.SIGKILL)
    proc.wait()
    os.close(master)

    with open(capture, "wb") as fp:
        fp.write(buf)

    if resized_at is None:
        print("FAIL: process ended before the resize")
        return 1
    before, after = bytes(buf[:resized_at]), bytes(buf[resized_at:])
    # Control: without a live display the redraw check would pass vacuously.
    if RUNNING not in before:
        print("FAIL: no progress display before the resize (%d bytes)" % len(before))
        return 1
    if CLEAR not in after:
        print("FAIL: no full redraw after the resize (%d bytes)" % len(after))
        return 1
    print("ok: redraw seen %d bytes after the resize" % after.index(CLEAR))
    return 0


if __name__ == "__main__":
    sys.exit(main())
