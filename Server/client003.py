import requests
import time
import hmac
import hashlib
import base64
import json
import tempfile
import zipfile
import os

import struct
import heatshrink2
import numpy as np

# ======= 配置 =======
DEVICE_ID = "nrf9160-dev001"
SECRET_KEY = b"sensingteam"

SERVER_URL = "http://54.253.68.173:80/data"


activities = [1, 2, 3, 4, 5]
prior = [0.2, 0.1, 0.4, 0.2, 0.1]
# gauss_prarams = [
#     (50, 20), (100, 30), (30, 10), (70, 25), (40, 15)
# ]
# gauss_params_std = [
#     (50, 4.47), (100, 5.48), (30, 3.16), (70, 5), (40, 3.87)
# ]
uniform_params = [
    (5, 25), (10, 60), (1, 15), (10, 31), (5, 95)
]

# Sample activity
def sample_activity():
    r = np.random.uniform(0, 1)
    prob = 0.
    for i in range(5):
        prob += prior[i]
        if r <= prob:
            return i
    return i

# Simulate 24 hours of activity recognition data
def generate_24h_data(start_time=None):
    """Generate simulated activity data for 24 hours.
    
    - `start_time`: Start timestamp (default: now).
    - `avg_interval`: Average time between events in seconds.
    """
    if start_time is None:
        start_time = int(time.time()) - 86400  # Default to 24 hours ago
    
    data = []
    data.append({"t": 0, "a": 0})

    total_elapsed = 0
    while total_elapsed < 86400:  # 24 hours worth of data
        label = sample_activity()
        # delta = max(1, int(np.random.normal(*gauss_params_std[label])))  # Gaussian distribution around avg_interval # May cause overflow
        delta = int(np.random.uniform(*uniform_params[label]))
        data.append({"t": delta, "a": label})  # Only record the starting timestamp
        total_elapsed += delta
    
    return start_time, data

def serialize_data(start_ts, data):
    seq = [(item["t"], item["a"]) for item in data]

    print(f"First 20 bytes of serialized data: {seq[:20]}")

    binary = struct.pack(">I", start_ts)  # 4 字节起始时间
    binary += b''.join(
        struct.pack(">Hb", delta_t, a) for delta_t, a in seq
    )

    # More compact
    # for delta_t, a in seq:
    #     # 编码 24 位
    #     packed = (delta_t << 3) | (a & 0x07)
    #     binary += bytes([
    #         (packed >> 16) & 0xFF,
    #         (packed >> 8) & 0xFF,
    #         packed & 0xFF
    #     ])
    return binary

def compress_data(data):
    compressed_data = heatshrink2.compress(data, window_sz2=4, lookahead_sz2=3)
    return compressed_data

def compute_hmac(timestamp: int, device_id: str, payload_bytes: bytes) -> str:
    message = str(timestamp).encode() + device_id.encode() + payload_bytes
    digest = hmac.new(SECRET_KEY, message, hashlib.sha256).digest()
    return base64.b64encode(digest).decode()

def send_post():
    timestamp = int(time.time())

    start_time, simu_data = generate_24h_data()
    serial_data = serialize_data(start_time, simu_data)
    data_bytes = compress_data(serial_data)

    hmac_value = compute_hmac(timestamp, DEVICE_ID, data_bytes)

    body = {
        "device_id": DEVICE_ID,
        "timestamp": timestamp,
        "data": base64.b64encode(data_bytes).decode(),
        "hmac": hmac_value
    }

    print(f"Total Entries: {len(simu_data)}")
    print(f"Original Size: {len(serial_data)} bytes")
    print(f"Compressed Size: {len(data_bytes)} bytes")
    print(f"Compression Ratio: {len(data_bytes) / len(serial_data):.2%}")
    print(f"Sent: HMAC: {hmac_value}")

    resp = requests.post(SERVER_URL, json=body)

    print("Return:", resp.status_code)
    print("Response:", resp.text)
    return simu_data

if __name__ == "__main__":
    send_post()
    