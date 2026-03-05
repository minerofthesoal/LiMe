#!/usr/bin/env python3
"""Minimal LiMe custom API v1 server."""

from http.server import BaseHTTPRequestHandler, HTTPServer
import json
import os
from datetime import datetime


class LiMeAPIHandler(BaseHTTPRequestHandler):
    def _json(self, code: int, payload: dict):
        data = json.dumps(payload).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        if self.path == "/api/v1/health":
            self._json(200, {"status": "ok", "version": "0.1.1-prealpha", "time": datetime.utcnow().isoformat()})
            return
        if self.path == "/api/v1/keys":
            keys = {"hf": bool(os.getenv("HF_TOKEN")), "openai": bool(os.getenv("OPENAI_API_KEY"))}
            self._json(200, {"configured": keys})
            return
        self._json(404, {"error": "not_found"})


def main(host: str = "0.0.0.0", port: int = 8081):
    server = HTTPServer((host, port), LiMeAPIHandler)
    print(f"LiMe API v1 listening on http://{host}:{port}")
    server.serve_forever()


if __name__ == "__main__":
    main()
