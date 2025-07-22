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

app = Flask(__name__)

DEVICE_KEYS = {
    "nrf9160-dev001": b"sensingteam",
    "nrf9160-dev002": b"haralab",
    "nrf52840": b"greatkey",
}

UPLOAD_DIR = "uploads"
os.makedirs(UPLOAD_DIR, exist_ok=True)

def verify_hmac(device_id: str, timestamp: int, data_bytes: bytes, recv_hmac: str) -> bool:
    key = DEVICE_KEYS.get(device_id)
    if not key:
        app.logger.warning(f"Unknown device_id: {device_id}")
        return False

    message = str(timestamp).encode() + device_id.encode() + data_bytes
    expected_digest = hmac.new(key, message, hashlib.sha256).digest()
    expected_hmac = base64.b64encode(expected_digest).decode()

    app.logger.info(f"Expected HMAC: {expected_hmac}")
    app.logger.info(f"Received HMAC: {recv_hmac}")

    return hmac.compare_digest(expected_hmac, recv_hmac)

def deserialize(binary):
    data = []
    timestamp = 0
    for i in range(0, len(binary), 3):
        delta_t, a = struct.unpack(">Hb", binary[i:i+3])
        timestamp += delta_t
        data.append({
            "t": timestamp,
            "a": None if a == -1 else a
        })
    return data

def save_to_csv(data, path):
    with open(path, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=["t", "a"])
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

    data_bytes = base64.b64decode(data_b64)

    serial_data = heatshrink2.decompress(data_bytes, window_sz2=4, lookahead_sz2=3)
    raw_data = deserialize(serial_data)

    if not verify_hmac(device_id, timestamp, data_bytes, recv_hmac):
        return jsonify({"error": "Invalid HMAC"}), 403

    app.logger.info(f"Valid payload from {device_id} → First 20 bytes: {serial_data[:20]}")

    # 保存 csv 文件
    filename = os.path.join(UPLOAD_DIR, f"{device_id}_{timestamp}.zip")
    save_to_csv(raw_data, filename)

    return jsonify({"status": "OK"}), 200

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=80, debug=True)