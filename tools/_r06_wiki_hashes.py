#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""计算 Wiki 目录所有 .md 文件的 SHA-256 哈希"""
import hashlib
import os

wiki_dir = 'AstroCS.wiki'
hashes = []
for fname in sorted(os.listdir(wiki_dir)):
    if fname.endswith('.md'):
        fpath = os.path.join(wiki_dir, fname)
        h = hashlib.sha256()
        with open(fpath, 'rb') as f:
            for chunk in iter(lambda: f.read(1 << 20), b''):
                h.update(chunk)
        hashes.append(f"{h.hexdigest()}  {fname}")

out_path = os.path.join(wiki_dir, 'WIKI_FILE_HASHES.sha256')
with open(out_path, 'w', encoding='utf-8') as f:
    for line in hashes:
        f.write(line + '\n')

for line in hashes:
    print(line)
