#!/usr/bin/env python3
"""Local listening page for A/B comparisons of renders.

    python3 beads/test/listen.py            # http://127.0.0.1:8765
    python3 beads/test/listen.py --port 9000

Serves beads/test/listen.html, a JSON list of every WAV under renders/ and
audio_samples/ at /api/renders, and the WAVs themselves under /files/. Only
those two folders are served, and only on 127.0.0.1.

Names and descriptions come from beads/test/listen_notes.json (committed,
since renders/ is git-ignored), re-read on every request.

Open a comparison directly with a hash of comma-separated paths, e.g.
    http://127.0.0.1:8765/#renders/stock/beads_processor.wav,renders/04-wet-gain/beads_processor.wav
"""

import argparse
import json
import os
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import unquote, urlparse

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
PAGE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "listen.html")
AUDIO_DIRS = ("renders", "audio_samples")
NOTES = os.path.join(os.path.dirname(os.path.abspath(__file__)), "listen_notes.json")


def load_notes():
    try:
        with open(NOTES) as f:
            notes = json.load(f)
    except (OSError, ValueError) as e:
        print("listen_notes.json not read: %s" % e, flush=True)
        notes = {}
    return notes.get("folders", {}), notes.get("renders", {})


def list_wavs():
    folders, renders = load_notes()
    out = []
    for top in AUDIO_DIRS:
        base = os.path.join(ROOT, top)
        for dirpath, _, files in os.walk(base):
            for f in sorted(files):
                if f.lower().endswith(".wav"):
                    full = os.path.join(dirpath, f)
                    rel = os.path.relpath(full, ROOT)
                    group = os.path.relpath(dirpath, ROOT)
                    folder = folders.get(group, {})
                    out.append({
                        "path": rel,
                        "group": group,
                        "name": f,
                        "mtime": os.path.getmtime(full),
                        "label": folder.get("label", ""),
                        "description": folder.get("description", ""),
                        "about": renders.get(f, ""),
                    })
    out.sort(key=lambda w: (w["group"], w["name"]))
    return out


class Handler(SimpleHTTPRequestHandler):
    def log_message(self, fmt, *args):
        pass

    def send_bytes(self, body, ctype, cache=False):
        self.send_response(200)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "max-age=0, must-revalidate" if cache else "no-store")
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        path = unquote(urlparse(self.path).path)
        if path in ("/", "/index.html"):
            with open(PAGE, "rb") as f:
                return self.send_bytes(f.read(), "text/html; charset=utf-8")
        if path == "/api/renders":
            return self.send_bytes(json.dumps(list_wavs()).encode(), "application/json")
        if path.startswith("/files/"):
            rel = path[len("/files/"):]
            full = os.path.abspath(os.path.join(ROOT, rel))
            allowed = any(full.startswith(os.path.join(ROOT, d) + os.sep) for d in AUDIO_DIRS)
            if allowed and full.lower().endswith(".wav") and os.path.isfile(full):
                with open(full, "rb") as f:
                    return self.send_bytes(f.read(), "audio/wav", cache=True)
        self.send_error(404)


def main():
    p = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    p.add_argument("--port", type=int, default=8765)
    args = p.parse_args()
    server = ThreadingHTTPServer(("127.0.0.1", args.port), Handler)
    print("Listening page at http://127.0.0.1:%d" % args.port, flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
