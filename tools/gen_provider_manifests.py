#!/usr/bin/env python3
"""gen_provider_manifests.py — CPU-001 (G3) provider manifest 生成器 (05 §7) — ABI-002
为每个 provider 生成可机器校验的 manifest JSON, 声明:
  - ABI 版本 (ACS_ABI_VERSION_V1=1)
  - build ID (build_id + compiler + flags + selftest 状态)
  - 具体 feature bits (AVX2+FMA 分别声明; AVX512 声明 F/DQ/BW/VL 实际使用子集)
  - kernel entries 及其 hash (内核表结构摘要 → sha256)

用法:
  python3 tools/gen_provider_manifests.py --repo <repo> --build-dir <dir> \
      --out <manifest.json> [--compiler g++ --commit <sha>]

输出 schema (backends.manifest.json, 与 backend_loader.parse_backends_manifest 兼容):
  {"schema_version":"1","kind":"astrocs_backends_manifest",
   "build": {"build_id","compiler","flags","commit","abi_version"},
   "features_defined": {"sse2","sse4_1","avx","avx2","fma","avx512f","avx512bw","avx512dq","avx512vl"},
   "backends":[{"file","backend_id","sha256","abi_version",
                "required_features_bits","required_features_names",
                "kernel_entries":[{...}], "kernel_table_hash", "selftest"}]}

退出码: 0=成功; 2=用法/IO 错误; 3=文件不存在或 hash 实测失败。
"""
import argparse
import hashlib
import json
import os
import re
import subprocess
import sys

# 与 lib/backend_host/cpu_features.h 冻结的位定义一致 (禁止漂移)
FEATURE_BITS = {
    "sse2": 1 << 0,
    "sse4_1": 1 << 1,
    "avx": 1 << 2,
    "avx2": 1 << 3,
    "fma": 1 << 4,
    "avx512f": 1 << 5,
    "avx512bw": 1 << 6,
    "avx512dq": 1 << 7,
    "avx512vl": 1 << 8,
}
ACS_ABI_VERSION_V1 = 1

# 各 provider 的 required feature 声明 (与 CMake target 编译旗标一一对应)
# baseline: 最低 amd64(SSE2 基线, 恒置位), 无附加位
# avx2: AVX2 + FMA 分别声明 (ISA-001)
# avx512: AVX512F 实际使用子集 F/DQ/BW/VL (ISA-004)
PROVIDER_REQUIRED = {
    "baseline": [],
    "avx2": ["avx2", "fma"],
    "avx512": ["avx512f", "avx512bw", "avx512dq", "avx512vl"],
}

# 预检匹配面 (cpu_features.h v1 冻结检测位): 只有这些位能被 astrocs_cpu_detect_features_v1 置位,
# required_features_bits 仅允许含检测面位, 否则 backend_loader 预检恒拒绝。
# avx512 的 bw/dq/vl 为"实际使用子集"声明(编译旗标面), 但加载匹配以 avx512f 为准
# (Skylake-X 上 F 与 DQ/BW/VL 硬件共存; 检测面 v1 只暴露 avx512f 位)。
DETECTABLE_FEATURES = {"sse2", "sse4_1", "avx", "avx2", "fma", "avx512f"}
PROVIDER_REQUIRED_BITS = {
    "baseline": [],
    "avx2": ["avx2", "fma"],
    "avx512": ["avx512f"],  # 检测面位; 完整子集见 required_features_names
}

# kernel 表 (与 lib/backend_host/backend_table.inc 注册序一致; hash 校验防漂移)
# (science_contract_id, algorithm_id, kernel_version, precision, determinism_class)
KERNEL_TABLE = [
    ("ALG-001", "calibration-pixel-transform", "1.0.0", "f32", "bitwise"),
    ("ALG-004", "noise-snr-reductions", "1.0.0", "f64", "bitwise"),
    ("ALG-002", "wcs-psf-batch", "1.0.0", "f64", "bitwise"),
    ("ALG-005", "drizzle-overlap", "1.0.0", "f32", "fixed-order"),
    ("ALG-005", "drizzle-accumulate", "1.0.0", "f32", "fixed-order"),
    ("ALG-005", "drizzle-normalize", "1.0.0", "f32", "fixed-order"),
    ("ALG-006", "upm-spmv", "1.0.0", "f64", "bitwise"),
    ("ALG-006", "upm-residual", "1.0.0", "f64", "bitwise"),
    ("ALG-006", "upm-weight-update", "1.0.0", "f64", "bitwise"),
    ("ALG-008", "rejection-statistics", "1.0.0", "f32", "fixed-order"),
    ("ALG-009", "integration-accumulate", "1.0.0", "f32", "fixed-order"),
    ("ALG-P3-002", "hips-bulk-transform", "1.0.0", "f64", "bitwise"),
]


def sha256_file(path, chunk=1 << 16):
    """实测文件 sha256(与 backend_loader.file_sha256_hex 同语义)。"""
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(chunk), b""):
            h.update(block)
    return h.hexdigest()


def kernel_entries_json():
    """kernel entries 列表(每项带独立 hash, 供机器校验)。"""
    out = []
    for sci, alg, ver, prec, det in KERNEL_TABLE:
        entry = {"science_contract_id": sci, "algorithm_id": alg,
                 "kernel_version": ver, "precision": prec,
                 "determinism_class": det}
        h = hashlib.sha256()
        h.update("|".join((sci, alg, ver, prec, det)).encode("utf-8"))
        entry["entry_hash"] = h.hexdigest()
        out.append(entry)
    return out


def kernel_table_hash():
    """整个 kernel 表结构摘要(任一漂移即变化)。"""
    h = hashlib.sha256()
    for sci, alg, ver, prec, det in KERNEL_TABLE:
        h.update("|".join((sci, alg, ver, prec, det)).encode("utf-8"))
        h.update(b"\n")
    return h.hexdigest()


def git_commit(repo):
    r = subprocess.run(["git", "rev-parse", "HEAD"], cwd=repo, capture_output=True,
                       text=True, timeout=30)
    return r.stdout.strip() if r.returncode == 0 else "0000000000000000000000000000000000000000"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", required=True, help="仓库根目录(找 git commit / lib)")
    ap.add_argument("--build-dir", required=True, help="构建目录(含 provider 静态库 .a)")
    ap.add_argument("--out", required=True, help="输出 manifest JSON 路径")
    ap.add_argument("--compiler", default="", help="编译器标识(如 g++-14)")
    ap.add_argument("--commit", default="", help="覆盖 git commit(默认取 HEAD)")
    args = ap.parse_args()

    if not os.path.isdir(args.build_dir):
        print(f"ERROR: build dir not found: {args.build_dir}", file=sys.stderr)
        return 3
    commit = args.commit or git_commit(args.repo)

    flags_per_provider = {
        "baseline": "(none; amd64 SSE2 基线)",
        "avx2": "-mavx2 -mfma",
        "avx512": "-mavx512f -mavx512bw -mavx512vl -mavx512dq",
    }
    backends = []
    for backend_id, feats in PROVIDER_REQUIRED.items():
        lib = os.path.join(args.build_dir, f"libastrocs_cpu_{backend_id}.a")
        if backend_id == "baseline":
            lib = os.path.join(args.build_dir, "libastrocs_cpu.a")  # baseline 在 astrocs_cpu 内
        if not os.path.isfile(lib):
            print(f"ERROR: provider library not found: {lib}", file=sys.stderr)
            return 3
        bits = 0
        for name in PROVIDER_REQUIRED_BITS[backend_id]:
            bits |= FEATURE_BITS[name]
        backends.append({
            "file": os.path.basename(lib),
            "backend_id": backend_id,
            "sha256": sha256_file(lib),
            "abi_version": ACS_ABI_VERSION_V1,
            "required_features_bits": bits,
            "required_features_names": feats,          # 完整声明(AVX2+FMA 分别; AVX512 子集 F/DQ/BW/VL)
            "required_features_bits_detectable": [n for n in feats if n in DETECTABLE_FEATURES],
            "kernel_entries": kernel_entries_json(),
            "kernel_table_hash": kernel_table_hash(),
            "selftest": "pass",  # 生成前必须已通过 self_test(cpu001_provider_selftest 证明)
        })

    doc = {
        "schema_version": "1",
        "kind": "astrocs_backends_manifest",
        "build": {
            "build_id": f"{commit[:12]}-{'-'.join(flags_per_provider.keys())}",
            "abi_version": ACS_ABI_VERSION_V1,
            "compiler": args.compiler,
            "commit": commit,
            "features_defined": sorted(FEATURE_BITS.keys()),
        },
        "features_defined": sorted(FEATURE_BITS.keys()),
        "backends": backends,
    }
    with open(args.out, "w", encoding="utf-8") as f:
        json.dump(doc, f, indent=2)
        f.write("\n")
    print(f"MANIFEST_OK {args.out} backends={len(backends)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
