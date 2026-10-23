#!/usr/bin/env python3
"""Serves VMX World's Xbox build to its network loader (xbox/loader/main.c, docs/XBOX.md); copied from FOLD's.

    tools/xbox_serve.py            # port 8771; serves xbox/build/xbox/VMXWORLD (tools/build_xbox.sh)

GET /version  a short hash of the build (the XBE, assets, catalog); the loader compares it with world.ver
GET /pack     the whole build in one answer: "FOLDPACK <n>\\n", "<size> <path>\\n" per file, then the files;
              the folder's default.xbe travels as world.xbe (the loader itself is default.xbe on the Xbox)
The folder is read again on every request, so a new build is picked up without a restart. Leave it running
while starting VMX World on the Xbox; each start prints what the loader asked for.
"""
import hashlib
import http.server
import os
import sys
import time

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "xbox", "build", "xbox", "VMXWORLD")
PORT = 8771


def pack():
    files = [("world.xbe", os.path.join(ROOT, "default.xbe"))]
    for sub in ("assets", "catalog"):
        d = os.path.join(ROOT, sub)
        for name in sorted(os.listdir(d)) if os.path.isdir(d) else []:
            files.append((sub + "/" + name, os.path.join(d, name)))
    blobs = [(name, open(path, "rb").read()) for name, path in files]
    head = "FOLDPACK %d\n" % len(blobs) + "".join("%d %s\n" % (len(b), n) for n, b in blobs)
    body = head.encode() + b"".join(b for _, b in blobs)
    return hashlib.sha1(body).hexdigest()[:12], body, len(blobs)


class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.0"

    def send(self, body, kind="application/octet-stream"):
        self.send_response(200)
        self.send_header("Content-Type", kind)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        if not os.path.isfile(os.path.join(ROOT, "default.xbe")):
            self.send_error(404, "no build: run tools/build_xbox.sh")
            return
        ver, body, n = pack()
        if self.path.startswith("/version"):
            self.send((ver + "\n").encode(), "text/plain")
        elif self.path.startswith("/pack"):
            self.send(body)
            print("  sent build %s: %d files, %.1f MB" % (ver, n, len(body) / 1e6), flush=True)
        else:
            self.send_error(404)

    def log_message(self, fmt, *args):
        print("%s %s %s" % (time.strftime("%H:%M:%S"), self.client_address[0], fmt % args), flush=True)


if __name__ == "__main__":
    if len(sys.argv) > 1:
        PORT = int(sys.argv[1])
    print("serving %s on port %d (Ctrl+C stops)" % (os.path.normpath(ROOT), PORT), flush=True)
    http.server.ThreadingHTTPServer(("", PORT), Handler).serve_forever()
