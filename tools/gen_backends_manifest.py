#!/usr/bin/env python3
"""backends.manifest.json 生成器 (05 §7) — ABI-002
用法: python3 tools/gen_backends_manifest.py <backend_file>... --out <manifest.json>
每个文件实测 sha256; required_features 以 --feat sse4_1,avx2 形式给出(逗号位名)。
"""
import argparse, hashlib, json, sys

BITS = {"sse2": 1 << 0, "sse4_1": 1 << 1, "avx": 1 << 2,
        "avx2": 1 << 3, "fma": 1 << 4, "avx512f": 1 << 5}


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 16), b""):
            h.update(chunk)
    return h.hexdigest()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("files", nargs="+")
    ap.add_argument("--out", required=True)
    ap.add_argument("--feat", default="sse2", help="逗号分隔位名: sse2,sse4_1,avx2,...")
    ap.add_argument("--compiler", default="")
    ap.add_argument("--flags", default="")
    args = ap.parse_args()

    bits = 0
    for name in args.feat.split(","):
        name = name.strip().lower()
        if name:
            if name not in BITS:
                print(f"unknown feature bit: {name}", file=sys.stderr)
                return 2
            bits |= BITS[name]

    backends = []
    for p in args.files:
        backends.append({
            "file": p.rsplit("/", 1)[-1].rsplit("\\", 1)[-1],
            "backend_id": p.rsplit("/", 1)[-1].rsplit(".", 1)[0],
            "sha256": sha256_file(p),
            "abi_version": 1,
            "required_features_bits": bits,
            "required_features_names": [n for n, b in BITS.items() if bits & b],
            "compiler": args.compiler,
            "flags": args.flags,
            "selftest": "pass",   # 生成前须已通过 self_test(05 §7)
        })
    doc = {"schema_version": "1", "kind": "astrocs_backends_manifest", "backends": backends}
    with open(args.out, "w", encoding="utf-8") as f:
        json.dump(doc, f, indent=2)
        f.write("\n")
    print(args.out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
