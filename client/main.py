import os
from http.server import BaseHTTPRequestHandler, HTTPServer

LISTEN_ADDR = os.getenv("LISTEN_ADDR", "0.0.0.0")
LISTEN_PORT = int(os.getenv("LISTEN_PORT", "8080"))

RESPONSE_BODY = b"hello ns3"


class Handler(BaseHTTPRequestHandler):
    def do_POST(self):
        content_length = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(content_length)
        print(f"Received {content_length} bytes: {body!r}")

        self.send_response(200)
        self.send_header("Content-Type", "text/plain")
        self.send_header("Content-Length", str(len(RESPONSE_BODY)))
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(RESPONSE_BODY)

    def log_message(self, format, *args):
        print(f"[{self.client_address[0]}] {format % args}")


def main():
    server = HTTPServer((LISTEN_ADDR, LISTEN_PORT), Handler)
    print(f"Listening on {LISTEN_ADDR}:{LISTEN_PORT} ...")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
