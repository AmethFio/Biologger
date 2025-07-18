import cbor2
import zstandard as zstd
import random
import time
import numpy as np
# What
# Simulate 24 hours of activity recognition data
def generate_24h_data(start_time=None, avg_interval=150):
    """Generate simulated activity data for 24 hours.
    
    - `start_time`: Start timestamp (default: now).
    - `avg_interval`: Average time between events in seconds.
    """
    if start_time is None:
        start_time = int(time.time()) - 86400  # Default to 24 hours ago
    
    activities = [0, 1, 2, 3, 4]
    data = []
    data.append({"t": start_time, "a": None})

    timestamp = start_time
    tmp_act = None
    tmp_delta = 0
    while timestamp < start_time + 86400:  # 24 hours worth of data
        label = random.choice(activities)
        delta = max(1, int(random.gauss(avg_interval, 140)))  # Gaussian distribution around avg_interval
        timestamp += delta
        if label != tmp_act:
            if tmp_delta == 0:
                tmp_delta = delta
            data.append({"t": tmp_delta, "a": label})  # Only record the starting timestamp
            tmp_act = label
            tmp_delta = 0
        else:
            tmp_delta += delta
    
    return data

def single_byte_data(data):
    for i, d in enumerate(data):
        if d["t"] < 256:
            data[i]["t"] = np.uint8(d["t"])

    return data

# Function to recursively convert numpy types to native Python types
def to_native(obj):
    if isinstance(obj, np.generic):
        return obj.item()  # Convert numpy scalar to native type
    elif isinstance(obj, dict):
        return {k: to_native(v) for k, v in obj.items()}  # Recursively convert dict
    elif isinstance(obj, list):
        return [to_native(i) for i in obj]  # Recursively convert list
    else:
        return obj  # Base case: return obj as is if it's a normal type


# Compress data with CBOR and Zstandard
def compress_data(data):
    data = to_native(data)
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

def show_double_byte(re_data, th=23):
    return sum(1 for d in re_data[1:] if d["t"] > th)

def show_single_byte(re_data, th=23):
    return sum(1 for d in re_data[1:] if d["t"] <= th)

# Example usage
simulated_data = generate_24h_data(avg_interval=10)  # Avg 10s interval between events
re_data = single_byte_data(simulated_data)
compressed_data = compress_data(simulated_data)
decompressed_data = decompress_data(compressed_data)
re_compressed_data = compress_data(re_data)

pr = '\n'.join('t: '+str(d["t"])+' a: ' + str(d["a"]) for d in decompressed_data[:5])

# Print size reduction
print(f"Total events: {len(simulated_data) - 1}")
print(f"First 5 entries in decompressed data:\n{pr}")
print(f"Longest duration: {show_max_delta(simulated_data[1:])}")

print(f"Deltas > 23: {show_double_byte(simulated_data[1:])} entries")
print(f"Deltas <= 23: {show_single_byte(simulated_data[1:])} entries")
print(f"Original Size: {len(cbor2.dumps(to_native(simulated_data)))} bytes")
print(f"CBOR2 Compressed Size: {len(compressed_data)} bytes")
print(f"Compression Ratio: {len(compressed_data) / len(cbor2.dumps(to_native(simulated_data))):.2%}")

print(f"Deltas > 255: {show_double_byte(simulated_data[1:], 255)} entries")
print(f"Deltas <= 255: {show_single_byte(simulated_data[1:], 255)} entries")
print(f"Uint8 + CBOR2 Compressed Size: {len(re_compressed_data)} bytes")
print(f"Compression Ratio: {len(re_compressed_data) / len(cbor2.dumps(to_native(simulated_data))):.2%}")

