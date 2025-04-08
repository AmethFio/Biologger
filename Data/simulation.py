import cbor2
import zstandard as zstd
import random
import time
import numpy as np
# What
# Simulate 24 hours of activity recognition data
def generate_24h_data(start_time=None, avg_interval=30):
    """Generate simulated activity data for 24 hours.
    
    - `start_time`: Start timestamp (default: now).
    - `avg_interval`: Average time between events in seconds.
    """
    if start_time is None:
        start_time = int(time.time()) - 86400  # Default to 24 hours ago
    
    activities = [0, 1, 2, 3, 4]
    data = []
    
    timestamp = start_time
    while timestamp < start_time + 86400:  # 24 hours worth of data
        label = random.choice(activities)
        timestamp += max(1, int(random.gauss(avg_interval, 10)))  # Gaussian distribution around avg_interval
        data.append({"t": timestamp, "a": label})  # Only record the starting timestamp
    
    return data

def RLE_data(data):
    re_data = []
    tmp_act = None
    tmp_start = 0
    tmp_duration = 0
    first_entry = True
    for d in data:
        if first_entry:
            re_data.append({"t": d.t, "a": None})
            tmp_act = d.a
            tmp_start = d.t
            first_entry = False

        if d.a == tmp_act:
            tmp_duration = d.t - tmp_start
        else:
            if tmp_duration < 256:
                tmp_duration = np.uint8(tmp_duration)
            re_data.append({"t": tmp_duration, "a": tmp_act})
            tmp_act = d.a
            tmp_start = d.t
            tmp_duration = 0

# Compress data with CBOR and Zstandard
def compress_data(data):
    cbor_encoded = cbor2.dumps(data)  # Convert data to CBOR format
    compressor = zstd.ZstdCompressor(level=3)  # Adjust level for compression ratio vs. speed
    compressed_data = compressor.compress(cbor_encoded)
    return compressed_data

# Decompress and decode
def decompress_data(compressed_data):
    decompressor = zstd.ZstdDecompressor()
    cbor_decoded = decompressor.decompress(compressed_data)
    return cbor2.loads(cbor_decoded)

def show_max_delta(re_data):
    return max(re_data, key=lambda x: x["t"])

def show_double_byte(re_data):
    return sum(1 for d in re_data[1:] if d["t"] > 255)

def show_single_byte(re_data):
    return sum(1 for d in re_data[1:] if d["t"] <= 255)

# Example usage
simulated_data = generate_24h_data(avg_interval=10)  # Avg 10s interval between events
re_data = RLE_data(simulated_data)
compressed_data = compress_data(re_data)
decompressed_data = decompress_data(compressed_data)

# Print size reduction
print(f"Total events: {len(re_data) - 1}")
print(f"Original Size: {len(cbor2.dumps(re_data))} bytes")
print(f"Compressed Size: {len(compressed_data)} bytes")
print(f"Compression Ratio: {len(compressed_data) / len(cbor2.dumps(simulated_data)):.2%}")
print(f"First 5 entries in decompressed data: {decompressed_data[:5]}")
print(f"Longest duration: {show_max_delta(re_data)}")
print(f"Deltas > 255: {show_double_byte(re_data)} entries")
print(f"Deltas <= 255: {show_single_byte(re_data)} entries")
