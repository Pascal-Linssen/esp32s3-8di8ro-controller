import argparse
import http.client
import json
import os
import sys
import urllib.parse


def _eprint(*args: object) -> None:
    print(*args, file=sys.stderr)


def post_firmware(url: str, bin_path: str, ota_key: str, timeout_s: int) -> int:
    parsed = urllib.parse.urlparse(url)
    if parsed.scheme not in ("http", ""):
        raise ValueError("Only http:// URLs are supported")

    host = parsed.hostname or parsed.path  # allow passing just "192.168.1.50"
    if not host:
        raise ValueError("Missing host/IP")

    port = parsed.port or 80
    path = parsed.path if parsed.scheme else "/api/ota"
    if parsed.scheme and (path == "" or path == "/"):
        path = "/api/ota"

    if not os.path.isfile(bin_path):
        raise FileNotFoundError(bin_path)

    with open(bin_path, "rb") as f:
        data = f.read()

    headers = {
        "Content-Type": "application/octet-stream",
        "Content-Length": str(len(data)),
        "X-OTA-Key": ota_key,
        "Connection": "close",
    }

    conn = http.client.HTTPConnection(host, port, timeout=timeout_s)
    try:
        conn.request("POST", path, body=data, headers=headers)
        resp = conn.getresponse()
        body = resp.read() or b""

        # Try JSON decode, but keep raw text as fallback
        text = body.decode("utf-8", errors="replace")
        ok = 0
        try:
            payload = json.loads(text) if text.strip() else {}
            ok = 1 if payload.get("ok") == 1 else 0
        except Exception:
            payload = None

        print(f"HTTP {resp.status} {resp.reason}")
        if payload is not None:
            print(json.dumps(payload, ensure_ascii=False, indent=2))
        else:
            print(text)

        return 0 if (200 <= resp.status < 300 and ok == 1) else 2
    finally:
        conn.close()


def main() -> int:
    p = argparse.ArgumentParser(description="Upload firmware.bin to ESP32 via HTTP OTA (/api/ota)")
    p.add_argument("--ip", required=True, help="ESP IP or URL (e.g. 192.168.1.50 or http://192.168.1.50)")
    p.add_argument("--key", required=True, help="OTA key (same as configured on the device)")
    p.add_argument(
        "--bin",
        default=os.path.join(".pio", "build", "esp32s3", "firmware.bin"),
        help="Path to firmware.bin",
    )
    p.add_argument("--timeout", type=int, default=30, help="HTTP timeout (seconds)")
    args = p.parse_args()

    url = args.ip
    if not url.startswith("http://") and not url.startswith("https://"):
        # allow raw IP
        url = f"http://{url}/api/ota"

    try:
        return post_firmware(url=url, bin_path=args.bin, ota_key=args.key, timeout_s=args.timeout)
    except Exception as ex:
        _eprint(f"ERROR: {ex}")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
