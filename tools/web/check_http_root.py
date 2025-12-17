import socket
import re

HOST = "192.168.1.50"
PORT = 80

req = (
    "GET / HTTP/1.1\r\n"
    f"Host: {HOST}\r\n"
    "Connection: close\r\n"
    "\r\n"
).encode("ascii")

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.settimeout(5)
s.connect((HOST, PORT))
s.sendall(req)

data = b""
while True:
    try:
        chunk = s.recv(4096)
    except socket.timeout:
        break
    if not chunk:
        break
    data += chunk
s.close()

headers, _, body = data.partition(b"\r\n\r\n")
headers_txt = headers.decode("iso-8859-1", errors="replace")

m = re.search(r"(?im)^Content-Length:\s*(\d+)\s*$", headers_txt)
cl = int(m.group(1)) if m else None

print("--- headers ---")
print(headers_txt)
print("--- sizes ---")
print("Content-Length:", cl)
print("Received body bytes:", len(body))
print("Total bytes:", len(data))
print("Body starts with:", body[:60])
