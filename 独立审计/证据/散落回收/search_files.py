#!/usr/bin/env python3
import os
from pathlib import Path

evidence_dir = Path("/workspace/Astro CS Database/独立审计/证据")

# List all files with 05, ①, and 科学 in name
print("=== Files containing '05', '①', AND '科学' ===\n")

science_1_files = []
for f in evidence_dir.iterdir():
    if f.is_file() and f.suffix == '.md':
        name = f.name
        if '05' in name and '①' in name and '科学性' in name:
            science_1_files.append(name)

for i, f in enumerate(sorted(science_1_files), 1):
    full_path = evidence_dir / f
    print(f"{i}. {f}")
    print(f"   Full path: {full_path.absolute()}")
    print(f"   Exists: {full_path.exists()}")
    print()

if science_1_files:
    # Read the first file found
    target_file = sorted(science_1_files)[0]
    target_path = evidence_dir / target_file
    
    print("=" * 80)
    print(f"READING FILE: {target_file}")
    print("=" * 80)
    
    try:
        content = target_path.read_text(encoding='utf-8')
        print(content)
    except Exception as e:
        print(f"Error reading file: {e}")
