from flask import Flask, request, jsonify
import hmac
import hashlib
import base64
import os
import zipfile
import io
import heatshrink2
from datetime import datetime

import struct
import csv

from Crypto.Cipher import AES
from Crypto.Util.Padding import unpad

app = Flask(__name__)

DEVICE_KEYS = {
    "nrf9160-dev001": b"sensingteam",
    "nrf9160-dev002": b"haralab",
    "nrf52840": b"greatkey",
}

AES_KEY = b'secretkey1234567'

UPLOAD_DIR = "uploads"
os.makedirs(UPLOAD_DIR, exist_ok=True)

def verify_hmac(device_id: str, timestamp: int, data_bytes: str, recv_hmac: str) -> bool:
    key = DEVICE_KEYS.get(device_id)
    if not key:
        app.logger.warning(f"Unknown device_id: {device_id}")
        return False

    # # 拼出来的 message
    # ts_part = str(timestamp).encode()
    # id_part = device_id.encode()
    # data_part = base64.b64decode(data_bytes)
    # message = ts_part + id_part + data_part

    # # 打印长度
    # app.logger.info(f"Service-side ts_part length: {len(ts_part)}")
    # app.logger.info(f"Service-side id_part length: {len(id_part)}")
    # app.logger.info(f"Service-side data_part length: {len(data_part)}")
    # app.logger.info(f"Service-side total message length: {len(message)}")

    message = str(timestamp).encode() + device_id.encode() + base64.b64decode(data_bytes)
    expected_digest = hmac.new(key, message, hashlib.sha256).digest()
    expected_hmac = base64.b64encode(expected_digest).decode()

    app.logger.info(f"Expected HMAC: {expected_hmac}")
    app.logger.info(f"Received HMAC: {recv_hmac}")

    return hmac.compare_digest(expected_hmac, recv_hmac)
    
def aes_decrypt(encrypted: str) -> bytes:
    data = base64.b64decode(encrypted)
    iv = data[:16]
    ciphertext = data[16:]
    cipher = AES.new(AES_KEY, AES.MODE_CBC, iv)
    return unpad(cipher.decrypt(ciphertext), AES.block_size)

def deserialize(binary):
    assert (len(binary) - 4) % 3 == 0
    start_ts, = struct.unpack(">I", binary[:4])
    data = []
    timestamp = start_ts
    seq_bytes = binary[4:]

    for i in range(0, len(seq_bytes), 3):
        delta_t, a = struct.unpack(">Hb", seq_bytes[i:i+3])
        timestamp += delta_t
        data.append({
            "t": timestamp,
            "delta_t": delta_t,
            "a": a
        })

    # More compact
    # for i in range(0, len(seq_bytes), 3):
    #     packed = (seq_bytes[i] << 16) | (seq_bytes[i+1] << 8) | seq_bytes[i+2]
    #     a = packed & 0x07
    #     delta_t = packed >> 3
    #     timestamp += delta_t
    #     data.append({
    #         "t": timestamp,
    #         "delta_t": delta_t,
    #         "a": a
    #     })
    return data

def save_to_csv(data, path):
    with open(path, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=["t", "delta_t", "a"])
        writer.writeheader()
        for row in data:
            writer.writerow(row)

@app.route("/data", methods=["POST"])
def receive_data():
    payload = request.get_json()
    if not payload:
        return jsonify({"error": "Invalid JSON"}), 400

    device_id = payload.get("device_id")
    timestamp = payload.get("timestamp")
    data_b64 = payload.get("data")
    recv_hmac = payload.get("hmac")

    if not all([device_id, timestamp, data_b64, recv_hmac]):
        return jsonify({"error": "Missing fields"}), 400

    ts_format = datetime.fromtimestamp(timestamp).strftime("%Y-%m-%d %H:%M:%S")
    app.logger.info(f"Hear from device: {device_id} at {ts_format} ({timestamp})")

    if not verify_hmac(device_id, timestamp, data_b64, recv_hmac):
        return jsonify({"error": "Invalid HMAC"}), 403

    data_plain = aes_decrypt(data_b64)
    serial_data = heatshrink2.decompress(data_plain, window_sz2=4, lookahead_sz2=3)
    raw_data = deserialize(serial_data)

    app.logger.info(f"Valid payload from {device_id} → First 20 bytes: {raw_data[:20]}")

    # 保存 csv 文件
    filename = os.path.join(UPLOAD_DIR, f"{device_id}_{timestamp}.csv")
    save_to_csv(raw_data, filename)
    app.logger.info(f"Saved to {filename}")

    return jsonify({"status": "success", "message": "Data received"}), 200

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=80, debug=True)