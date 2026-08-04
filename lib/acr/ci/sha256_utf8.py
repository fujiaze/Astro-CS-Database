#!/usr/bin/env python3
"""
SHA-256 UTF-8 安全清单生成与验证工具
解决审计 #9：sha256sum 对中文路径编码处理错误

用法：
  生成清单：python sha256_utf8.py generate <目录> <输出文件.sha256>
  验证清单：python sha256_utf8.py verify <清单文件.sha256>
  生成+验证：python sha256_utf8.py generate-and-verify <目录> <输出文件.sha256>

清单格式：每行 <sha256hex> *<utf-8-path>
路径以 * 前缀标记二进制安全模式，路径本身为 UTF-8 编码。
"""
import hashlib
import os
import sys
import json
from pathlib import Path
from typing import List, Tuple, Optional


def compute_sha256(filepath: str) -> str:
    """计算文件的 SHA-256 哈希值"""
    h = hashlib.sha256()
    with open(filepath, 'rb') as f:
        while True:
            chunk = f.read(65536)
            if not chunk:
                break
            h.update(chunk)
    return h.hexdigest()


def collect_files(root_dir: str) -> List[str]:
    """递归收集目录下所有文件，返回 UTF-8 编码的相对路径列表"""
    files = []
    root = Path(root_dir)
    for dirpath, dirnames, filenames in os.walk(root):
        # 跳过 .git 目录
        if '.git' in dirnames:
            dirnames.remove('.git')
        for fname in filenames:
            full_path = os.path.join(dirpath, fname)
            rel_path = os.path.relpath(full_path, root_dir)
            # 确保路径是 UTF-8 字符串
            if isinstance(rel_path, bytes):
                rel_path = rel_path.decode('utf-8', errors='replace')
            files.append((rel_path, full_path))
    files.sort(key=lambda x: x[0])
    return files


def generate_manifest(root_dir: str, output_file: str) -> dict:
    """生成 SHA-256 清单文件"""
    files = collect_files(root_dir)
    entries = []
    stats = {
        'total_files': 0,
        'total_bytes': 0,
        'errors': [],
    }

    for rel_path, full_path in files:
        try:
            sha = compute_sha256(full_path)
            size = os.path.getsize(full_path)
            entries.append({
                'path': rel_path,
                'sha256': sha,
                'size': size,
            })
            stats['total_files'] += 1
            stats['total_bytes'] += size
        except Exception as e:
            stats['errors'].append({'path': rel_path, 'error': str(e)})

    # 写入 JSON 格式（UTF-8 安全）
    manifest = {
        'root': os.path.abspath(root_dir),
        'total_files': stats['total_files'],
        'total_bytes': stats['total_bytes'],
        'error_count': len(stats['errors']),
        'entries': entries,
    }

    with open(output_file, 'w', encoding='utf-8') as f:
        json.dump(manifest, f, ensure_ascii=False, indent=2)

    # 同时写入文本格式（兼容 sha256sum -c 风格，但用 UTF-8）
    text_file = output_file.replace('.json', '.txt') if output_file.endswith('.json') else output_file + '.txt'
    with open(text_file, 'w', encoding='utf-8') as f:
        for entry in entries:
            f.write(f"{entry['sha256']}  {entry['path']}\n")

    stats['output_json'] = output_file
    stats['output_text'] = text_file
    return stats


def verify_manifest(manifest_file: str) -> dict:
    """验证 SHA-256 清单"""
    with open(manifest_file, 'r', encoding='utf-8') as f:
        manifest = json.load(f)

    root = manifest['root']
    entries = manifest['entries']
    results = {
        'total': len(entries),
        'passed': 0,
        'failed': 0,
        'missing': 0,
        'mismatches': [],
        'missing_files': [],
    }

    for entry in entries:
        rel_path = entry['path']
        expected_sha = entry['sha256']
        full_path = os.path.join(root, rel_path)

        if not os.path.exists(full_path):
            results['missing'] += 1
            results['missing_files'].append(rel_path)
            continue

        actual_sha = compute_sha256(full_path)
        if actual_sha == expected_sha:
            results['passed'] += 1
        else:
            results['failed'] += 1
            results['mismatches'].append({
                'path': rel_path,
                'expected': expected_sha,
                'actual': actual_sha,
            })

    return results


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(1)

    mode = sys.argv[1]
    if mode == 'generate':
        root_dir = sys.argv[2]
        output_file = sys.argv[3] if len(sys.argv) > 3 else 'sha256_manifest.json'
        stats = generate_manifest(root_dir, output_file)
        print(f"Generated: {stats['total_files']} files, {stats['total_bytes']} bytes")
        print(f"Errors: {stats['error_count']}")
        print(f"Output: {stats['output_json']} + {stats['output_text']}")
        if stats['errors']:
            for e in stats['errors'][:5]:
                print(f"  ERROR: {e['path']}: {e['error']}")
        sys.exit(0 if stats['error_count'] == 0 else 1)

    elif mode == 'verify':
        manifest_file = sys.argv[2]
        results = verify_manifest(manifest_file)
        print(f"Total: {results['total']}")
        print(f"Passed: {results['passed']}")
        print(f"Failed: {results['failed']}")
        print(f"Missing: {results['missing']}")
        if results['mismatches']:
            print("\nMismatches:")
            for m in results['mismatches'][:10]:
                print(f"  {m['path']}")
                print(f"    expected: {m['expected']}")
                print(f"    actual:   {m['actual']}")
        if results['missing_files']:
            print("\nMissing files:")
            for f in results['missing_files'][:10]:
                print(f"  {f}")
        sys.exit(0 if results['failed'] == 0 and results['missing'] == 0 else 1)

    elif mode == 'generate-and-verify':
        root_dir = sys.argv[2]
        output_file = sys.argv[3] if len(sys.argv) > 3 else 'sha256_manifest.json'
        stats = generate_manifest(root_dir, output_file)
        print(f"Generated: {stats['total_files']} files")
        results = verify_manifest(stats['output_json'])
        print(f"Verified: {results['passed']}/{results['total']} passed")
        if results['failed'] > 0 or results['missing'] > 0:
            print(f"FAILED: {results['failed']} mismatches, {results['missing']} missing")
            sys.exit(1)
        print("ALL PASSED")
        sys.exit(0)

    else:
        print(f"Unknown mode: {mode}")
        print(__doc__)
        sys.exit(1)


if __name__ == '__main__':
    main()
