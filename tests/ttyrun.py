#!/usr/bin/env python3
"""Run a command with a pty for stdout/stderr and capture what it paints.

Usage: ttyrun.py <capture-file> [--stdin tty|eof] [--enter] -- <cmd> [args...]

--stdin tty (default) hands the child the pty, which never goes readable;
--stdin eof gives it /dev/null. --enter types one ENTER once the child paints.
"""
import os
import subprocess
import sys
import termios

capture = sys.argv[1]
opts = sys.argv[2 : sys.argv.index("--")]
argv = sys.argv[sys.argv.index("--") + 1 :]
stdin_mode = opts[opts.index("--stdin") + 1] if "--stdin" in opts else "tty"
send_enter = "--enter" in opts

master, slave = os.openpty()
attrs = termios.tcgetattr(slave)
attrs[1] &= ~termios.ONLCR  # or the pty manufactures the CRs a spinner check counts
termios.tcsetattr(slave, termios.TCSANOW, attrs)
devnull = os.open(os.devnull, os.O_RDONLY) if stdin_mode == "eof" else None
p = subprocess.Popen(
    argv, stdin=slave if devnull is None else devnull, stdout=slave, stderr=slave
)
os.close(slave)
if devnull is not None:
    os.close(devnull)

with open(capture, "wb") as f:
    while True:
        try:
            data = os.read(master, 4096)
        except OSError:  # the slave closed: EIO on Linux, empty read on BSD
            break
        if not data:
            break
        f.write(data)
        if send_enter:
            os.write(master, b"\n")
            send_enter = False
os.close(master)
sys.exit(p.wait())
