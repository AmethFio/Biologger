import random
import time
import numpy as np

activities = [0, 1, 2, 3, 4]
prior = [0.2, 0.1, 0.4, 0.2, 0.1]
gauss_prarams = [
    (50, 20), (100, 30), (30, 10), (70, 25), (40, 15)
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
    data.append({"t": start_time, "a": None})

    timestamp = start_time
    while timestamp < start_time + 86400:  # 24 hours worth of data
        label = sample_activity()
        delta = max(1, int(random.gauss(*gauss_prarams[label])))  # Gaussian distribution around avg_interval
        timestamp += delta
        data.append({"t": delta, "a": label})  # Only record the starting timestamp
    
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

