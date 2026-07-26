import zstandard
import struct
import sys
import os

# 读取原始 NESTED HISS 文件
src = sys.argv[1]
dst = sys.argv[2]

with open(src, 'rb') as f:
    data = f.read()

# 解析 HISS 头
magic = data[0:4]
assert magic == b'HISS', f'Not HISS file: {magic}'
uncomp_len, comp_len = struct.unpack('<II', data[4:12])
comp_json = data[12:12+comp_len]

# 解压 JSON 头
dctx = zstandard.ZstdDecompressor()
json_str = dctx.decompress(comp_json).decode('utf-8')
print(f'Original JSON (first 200 chars): {json_str[:200]}')

# 修改 nested: true -> false (NESTED -> RING)
new_json = json_str.replace('"nested":true', '"nested":false')
assert new_json != json_str, 'nested field not found or not modified'
print(f'Modified JSON (first 200 chars): {new_json[:200]}')

# 重新压缩
cctx = zstandard.ZstdCompressor(level=5)
new_comp = cctx.compress(new_json.encode('utf-8'))
new_comp_len = len(new_comp)
new_uncomp_len = len(new_json)
print(f'JSON: {uncomp_len} -> {new_uncomp_len} bytes, compressed: {comp_len} -> {new_comp_len} bytes')

# 写入新文件 (Magic + uncomp_len + comp_len + comp_json + 剩余数据)
with open(dst, 'wb') as f:
    f.write(b'HISS')
    f.write(struct.pack('<II', new_uncomp_len, new_comp_len))
    f.write(new_comp)
    # 写入剩余数据 (ipix + pixel + snr)
    f.write(data[12+comp_len:])

print(f'Written: {dst} ({os.path.getsize(dst)} bytes)')
