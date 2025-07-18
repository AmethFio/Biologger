#!/usr/bin/env python3

from http.server import HTTPServer, BaseHTTPRequestHandler
import ssl

class SimpleHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        self.send_response(200)
        self.send_header("Content-type", "text/plain; charset=utf-8")
        self.end_headers()
        self.wfile.write(b"Hello from HTTPS server!\n")

def run(port=4443):
    server_address = ('', port)
    httpd = HTTPServer(server_address, SimpleHandler)

    # 创建 SSLContext
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(certfile="cert.pem", keyfile="key.pem")

    # 把 socket wrap 一下
    httpd.socket = context.wrap_socket(httpd.socket, server_side=True)

    print(f"Serving on https://0.0.0.0:{port}")
    httpd.serve_forever()

if __name__ == "__main__":
    run()