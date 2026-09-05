#!/usr/bin/env python3
"""Minimal FTP server for the crawl tests: PASV only, binary RETR, LIST.

Prints "PORT <n>" once bound, like local-server.py. --mode-file is re-read
before every transfer, so a test can flip one path's behaviour between passes
without restarting the server and moving the port (which names the mirror
directory). Each line is "<path> <mode>...", path "*" matching everything:

  truncate  send a prefix of the body, then drop the data connection
  empty     open the data connection and send nothing
  stall     send a prefix, then hold the data connection open and go quiet
  trickle   send the body in small chunks, slowly enough to stay in flight
  norest    answer REST with 500, so the client re-fetches from scratch
  nomdtm    answer MDTM with 500, like a server predating RFC 3659

--require-pass answers USER with 331, the only way the client's PASS is sent.
--mute accepts the control connection and never speaks, not even the greeting.
"""

import argparse
import os
import socket
import sys
import threading
import time


# Longer than any bound a test asserts, so going quiet can never be what ends
# the run under test.
QUIET_SECONDS = 900

TRICKLE_CHUNK = 8192
TRICKLE_PAUSE = 0.2


def reply(conn, text):
    conn.sendall((text + "\r\n").encode("utf-8", "replace"))


class Session(threading.Thread):
    def __init__(self, conn, root, mode_file, log, require_pass=False, mute=False):
        threading.Thread.__init__(self, daemon=True)
        self.conn = conn
        self.root = root
        self.mode_file = mode_file
        self.log = log
        self.require_pass = require_pass
        self.mute = mute
        self.pasv = None
        self.rest = 0
        self.path = "/"  # named by SIZE/RETR; REST carries no path

    def modes(self):
        if not self.mode_file:
            return set()
        found = set()
        try:
            with open(self.mode_file, encoding="utf-8") as fp:
                for line in fp:
                    words = line.split()
                    if words and words[0] in ("*", self.path):
                        found |= set(words[1:])
        except OSError:
            pass
        return found

    # Resolve an FTP path argument under the root, refusing escapes.
    def resolve(self, arg):
        arg = arg.strip().strip('"')
        self.path = "/" + arg.lstrip("/")
        path = os.path.normpath(os.path.join(self.root, arg.lstrip("/")))
        if path != self.root and not path.startswith(self.root + os.sep):
            return None
        return path

    def open_pasv(self):
        srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        srv.bind(("127.0.0.1", 0))
        srv.listen(1)
        self.pasv = srv
        port = srv.getsockname()[1]
        return "227 Entering Passive Mode (127,0,0,1,%d,%d)" % (port >> 8, port & 0xFF)

    def accept_data(self):
        if self.pasv is None:
            return None
        self.pasv.settimeout(30)
        try:
            data, _ = self.pasv.accept()
        except (OSError, socket.timeout):
            data = None
        self.pasv.close()
        self.pasv = None
        return data

    def send_body(self, body, modes):
        data = self.accept_data()
        if data is None:
            reply(self.conn, "425 no data connection")
            return
        try:
            if "empty" in modes:
                pass
            elif "truncate" in modes:
                data.sendall(body[: max(1, len(body) // 8)])
            elif "stall" in modes:
                # A fraction, never the whole body: a completed transfer would
                # end the wait the tests are timing.
                data.sendall(body[: max(1, len(body) // 8)])
                time.sleep(QUIET_SECONDS)
            elif "trickle" in modes:
                for off in range(0, len(body), TRICKLE_CHUNK):
                    data.sendall(body[off : off + TRICKLE_CHUNK])
                    time.sleep(TRICKLE_PAUSE)
            else:
                data.sendall(body)
        except OSError:
            pass
        data.close()
        if modes & {"empty", "truncate", "stall"}:
            reply(self.conn, "426 transfer aborted")
        else:
            reply(self.conn, "226 Transfer complete")

    def do_retr(self, arg):
        path = self.resolve(arg)
        if path is None or not os.path.isfile(path):
            reply(self.conn, "550 no such file")
            return
        with open(path, "rb") as fp:
            body = fp.read()
        modes = self.modes()
        reply(self.conn, "150 opening binary mode")
        self.send_body(body[self.rest :], modes)
        self.rest = 0

    def do_list(self, arg):
        if arg.startswith("-A"):
            arg = arg[2:].strip()
        path = self.resolve(arg or "/")
        if path is None or not os.path.isdir(path):
            reply(self.conn, "550 no such directory")
            return
        lines = []
        for name in sorted(os.listdir(path)):
            full = os.path.join(path, name)
            if os.path.isdir(full):
                lines.append("drwxr-xr-x 2 ftp ftp 4096 Jan 01 00:00 %s" % name)
            else:
                lines.append(
                    "-rw-r--r-- 1 ftp ftp %d Jan 01 00:00 %s"
                    % (os.path.getsize(full), name)
                )
        reply(self.conn, "150 opening ASCII mode")
        self.send_body(("\r\n".join(lines) + "\r\n").encode("utf-8"), set())

    def dispatch(self, verb, arg):
        conn = self.conn
        if verb == "USER" and self.require_pass:
            reply(conn, "331 password required")  # the client sends PASS only on a 3xx
        elif verb in ("USER", "PASS", "TYPE", "NOOP"):
            reply(conn, "200 ok")
        elif verb == "SYST":
            reply(conn, "215 UNIX Type: L8")
        elif verb == "PWD":
            reply(conn, '257 "/"')
        elif verb == "CWD":
            reply(conn, "250 ok")
        elif verb == "PASV":
            reply(conn, self.open_pasv())
        elif verb == "EPSV":
            reply(conn, "500 not supported")
        elif verb == "SIZE":
            path = self.resolve(arg)
            if path and os.path.isfile(path):
                reply(conn, "213 %d" % os.path.getsize(path))
            else:
                reply(conn, "550 no such file")
        elif verb == "MDTM":
            path = self.resolve(arg)
            if "nomdtm" in self.modes():
                reply(conn, "500 MDTM not understood")
            elif path and os.path.isfile(path):
                stamp = time.gmtime(os.path.getmtime(path))
                reply(conn, "213 " + time.strftime("%Y%m%d%H%M%S", stamp))
            else:
                reply(conn, "550 no such file")
        elif verb == "REST":
            if "norest" in self.modes():
                reply(conn, "500 REST not understood")
            else:
                try:
                    self.rest = int(arg)
                except ValueError:
                    self.rest = 0
                reply(conn, "350 restarting")
        elif verb == "RETR":
            self.do_retr(arg)
        elif verb in ("LIST", "NLST"):
            self.do_list(arg)
        elif verb == "QUIT":
            reply(conn, "221 bye")
            return False
        else:
            reply(conn, "500 unknown command")
        return True

    def run(self):
        conn = self.conn
        buf = b""
        try:
            if self.mute:
                time.sleep(QUIET_SECONDS)  # connected, but not even a greeting
                return
            reply(conn, "220 httrack test ftp")
            while True:
                while b"\r\n" not in buf:
                    chunk = conn.recv(4096)
                    if not chunk:
                        return
                    buf += chunk
                line, buf = buf.split(b"\r\n", 1)
                line = line.decode("utf-8", "replace")
                self.log(line)
                verb, _, arg = line.partition(" ")
                if not self.dispatch(verb.upper(), arg):
                    return
        except OSError:
            return
        finally:
            if self.pasv is not None:
                self.pasv.close()
            conn.close()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", required=True)
    ap.add_argument("--mode-file")
    ap.add_argument("--log")
    ap.add_argument("--require-pass", action="store_true")
    ap.add_argument("--mute", action="store_true")
    args = ap.parse_args()

    # newline="\n": the shell greps these lines and Windows text mode adds a CR.
    logfp = open(args.log, "a", encoding="utf-8", newline="\n") if args.log else None
    log_lock = threading.Lock()

    def log(line):
        if logfp:
            with log_lock:
                logfp.write(line + "\n")
                logfp.flush()

    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("127.0.0.1", 0))
    srv.listen(16)
    sys.stdout.reconfigure(newline="\n")  # the launcher parses PORT, CRLF breaks it
    sys.stdout.write("PORT %d\n" % srv.getsockname()[1])
    sys.stdout.flush()

    root = os.path.abspath(args.root)
    while True:
        conn, _ = srv.accept()
        Session(conn, root, args.mode_file, log, args.require_pass, args.mute).start()


if __name__ == "__main__":
    main()
