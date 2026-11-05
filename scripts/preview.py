#!/usr/bin/env python3
"""Rebuild native previews on source changes and serve a live, local viewer."""

import argparse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import subprocess
import threading
import time
from urllib.parse import urlsplit
import uuid
import webbrowser

PROJECT = Path(__file__).resolve().parent.parent
OUTPUT = PROJECT / "build/preview"
BUILD = OUTPUT / "native"
LAYOUTS = ("morning", "afternoon", "test")


def source_signature():
    paths = {PROJECT / "CMakeLists.txt", PROJECT / ".clang-format",
             PROJECT / "scripts/format.sh", PROJECT / "dashboard.example.yaml",
             PROJECT / "dashboard.yaml"}
    for folder in ("src", "tests", "cmake"):
        paths.update(path for path in (PROJECT / folder).rglob("*") if path.is_file())
    signature = []
    for path in sorted(paths):
        try:
            info = path.stat()
        except FileNotFoundError:
            continue
        signature.append((str(path), info.st_mtime_ns, info.st_size))
    return tuple(signature)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument("--no-open", action="store_true", help="Do not open a browser")
    args = parser.parse_args()
    OUTPUT.mkdir(parents=True, exist_ok=True)
    lock = threading.Lock()
    stop = threading.Event()
    state = {"revision": "", "building": True, "error": False}
    images = {}

    def run(command, quiet=False):
        with subprocess.Popen(command, cwd=PROJECT,
                              stdout=subprocess.DEVNULL if quiet else None) as process:
            while process.poll() is None:
                if stop.wait(0.1):
                    process.terminate()
                    try:
                        process.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        process.kill()
                    raise InterruptedError()
            if process.returncode:
                raise subprocess.CalledProcessError(process.returncode, command)

    def watch():
        previous = None
        previous_minute = None
        configured = False
        while not stop.is_set():
            signature = source_signature()
            minute = int(time.time() // 60)
            source_changed = signature != previous
            with lock:
                clock_due = minute != previous_minute and not state["error"]
            if source_changed or clock_due:
                previous = signature
                previous_minute = minute
                with lock:
                    state.update(building=True, error=False)
                try:
                    if not configured:
                        run(["cmake", "-S", str(PROJECT), "-B", str(BUILD), "-G", "Ninja",
                             "-DCMAKE_CXX_COMPILER=" + os.environ.get("CXX", "clang++")])
                        configured = True
                    if source_changed:
                        run(["cmake", "--build", str(BUILD)])
                    updated = {}
                    for layout in LAYOUTS:
                        pbm = OUTPUT / f"{layout}.pbm"
                        png = OUTPUT / f"{layout}.png"
                        option = "--preview" if layout == "test" else f"--preview-{layout}"
                        config = PROJECT / "dashboard.yaml"
                        if not config.exists():
                            config = PROJECT / "dashboard.example.yaml"
                        run([str(BUILD / "family-dashboard"), option, str(pbm),
                             "--config", str(config)])
                        run(["sips", "-s", "format", "png", str(pbm), "--out", str(png)], quiet=True)
                        updated[layout] = png.read_bytes()
                    with lock:
                        images.update(updated)
                        state.update(revision=uuid.uuid4().hex, building=False, error=False)
                    print("Preview updated. Watching source files and the clock...", flush=True)
                except InterruptedError:
                    return
                except (OSError, subprocess.CalledProcessError) as error:
                    with lock:
                        state.update(building=False, error=True)
                    print(f"Preview build failed: {error}\nFix the error and save to retry.", flush=True)
            stop.wait(0.5)

    class Handler(BaseHTTPRequestHandler):
        def do_GET(self):
            path = urlsplit(self.path).path
            if path == "/":
                self.respond(200, "text/html; charset=utf-8",
                             (PROJECT / "scripts/preview.html").read_bytes())
            elif path == "/status":
                with lock:
                    body = json.dumps(state).encode()
                self.respond(200, "application/json", body)
            elif path in (f"/{layout}.png" for layout in LAYOUTS):
                with lock:
                    body = images.get(path[1:-4])
                self.respond(200 if body else 503, "image/png", body or b"")
            else:
                self.respond(404, "text/plain", b"Not found")

        def respond(self, code, content_type, body):
            self.send_response(code)
            self.send_header("Content-Type", content_type)
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            try:
                self.wfile.write(body)
            except (BrokenPipeError, ConnectionResetError):
                pass

        def log_message(self, *_):
            pass

    # Bind only to loopback; this is a development tool for the local machine.
    server = ThreadingHTTPServer(("127.0.0.1", args.port), Handler)
    url = f"http://127.0.0.1:{server.server_port}/"
    worker = threading.Thread(target=watch, daemon=True)
    worker.start()
    print(f"Live preview: {url}\nSave C++ sources to rebuild. Ctrl+C stops the preview.", flush=True)
    if not args.no_open:
        webbrowser.open(url)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        stop.set()
        server.server_close()
        worker.join()


if __name__ == "__main__":
    main()
