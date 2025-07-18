import requests
import time
import hmac
import hashlib
import base64
import json
import tempfile
import zipfile
import os

# ======= 配置 =======
DEVICE_ID = "nrf9160-dev002"
SECRET_KEY = b"haralab"

SERVER_URL = "http://54.253.68.173:80/data"

def generate_zip():
    tmp_zip = tempfile.NamedTemporaryFile(delete=False, suffix=".zip")
    with zipfile.ZipFile(tmp_zip.name, 'w') as zf:
        # 写入一些随机内容
        for i in range(5):
            zf.writestr(f"file_{i}.txt", str(i) * 2048)  # 每个文件约 2KB
    return tmp_zip.name

def compute_hmac(timestamp: int, device_id: str, payload_bytes: bytes) -> str:
    message = str(timestamp).encode() + device_id.encode() + payload_bytes
    digest = hmac.new(SECRET_KEY, message, hashlib.sha256).digest()
    return base64.b64encode(digest).decode()

def send_post():
    timestamp = int(time.time())
    zip_path = generate_zip()
    with open(zip_path, "rb") as f:
        payload_bytes = f.read()

    hmac_value = compute_hmac(timestamp, DEVICE_ID, payload_bytes)

    body = {
        "device_id": DEVICE_ID,
        "timestamp": timestamp,
        "payload": base64.b64encode(payload_bytes).decode(),
        "hmac": hmac_value
    }

    print(f"Sent: 10KB zip with HMAC: {hmac_value}")
    resp = requests.post(SERVER_URL, json=body)

    print("Return:", resp.status_code)
    print("Response:", resp.text)

    os.unlink(zip_path)  # 删除临时文件

if __name__ == "__main__":
    send_post()