import os
path = "/workspace/Astro CS Database/独立审计/证据/"
files = os.listdir(path)
for f in sorted(files):
    if '审查' in f and '05' in f and '①' in f and '科学性' in f:
        print(f"File: {f}")
        print(f"Bytes: {f.encode('utf-8')}")
        full_path = os.path.join(path, f)
        size = os.path.getsize(full_path)
        print(f"Size: {size} bytes")
        print("---")
