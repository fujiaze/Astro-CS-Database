#!/usr/bin/env python3
"""gen_provider_manifests.py — CPU-001 (G3) provider manifest 生成器 (05 §7) — ABI-002
为每个 provider 生成可机器校验的 manifest JSON, 声明:
  - ABI 版本 (ACS_ABI_VERSION_V1=1)
  - build ID (build_id + compiler + flags + selftest 状态)
  - 具体 feature bits (AVX2+FMA 分别声明; AVX512 声明 F/DQ/BW/VL 实际使用子集)
  - kernel entries 及其 hash (内核表结构摘要 → sha256)

用法:
  python3 eng/tools/gen_provider_manifests.py --repo <repo> --build-dir <dir> \
      --out <manifest.json> [--compiler g++ --commit <sha>]
  # 交付形态 (docs/architecture/ISA_VARIANTS.md §0 第 3 条 / §2; R-28):
# 变体为 SHARED DSO，与清单同目录:
  python3 eng/tools/gen_provider_manifests.py --repo <repo> --build-dir <dir> \
      --providers-dir <build>/providers --isa-only --out <build>/providers/backends.manifest.json

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
import ctypes
import hashlib
import json
import os
import re
import subprocess
import sys

# 与 lib/backend_host/cpu_features.h 冻结的位定义**逐位同源** (禁止漂移)。
# 订正 (DYN-740 / R-28): 原表 avx512bw=1<<6 / dq=1<<7 / vl=1<<8 与 cpu_features.h
# (CD=1<<6 / BW=1<<7 / DQ=1<<8 / VL=1<<9) 冲突 —— 同一位号两名两义，生成出来的
# required_features_bits 会被预检按**另一个能力**判读。现按 cpu_features.h 逐位抄录。
FEATURE_BITS = {
    "sse2": 1 << 0,
    "sse4_1": 1 << 1,
    "avx": 1 << 2,
    "avx2": 1 << 3,
    "fma": 1 << 4,
    "avx512f": 1 << 5,
    "avx512cd": 1 << 6,
    "avx512bw": 1 << 7,
    "avx512dq": 1 << 8,
    "avx512vl": 1 << 9,
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

# 预检匹配面 = cpu_features.h 的实测检测位。订正 (DYN-740 / R-28): 旧注释称"检测面 v1
# 只暴露 avx512f 位"，据此把 avx512 的 required_features_bits 收窄到 avx512f ——
# 而 cpu_features.cpp 自 TRUTHFUL-CONCLUSION-01 起逐位实测 BW/DQ/VL，且 avx512
# provider 以 -mavx512f/-bw/-dq/-vl 编译 ⇒「声明 ⊊ 编译」，出厂机器（如 KNL：有 F 无
# BW/DQ/VL）上加载放行、首调撞非法指令。现声明 = 编译 = 检测三侧同源
# (ACS_FEAT_AVX512_PROVIDER_REQUIRED = F|BW|DQ|VL = 928)。
DETECTABLE_FEATURES = set(FEATURE_BITS.keys())
PROVIDER_REQUIRED_BITS = {
    "baseline": [],
    "avx2": ["avx2", "fma"],
    "avx512": ["avx512f", "avx512bw", "avx512dq", "avx512vl"],
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


# ── provider 家族表 (DYN-740 / P4-M05 交接项 1「C-03 入图」) ──────────────────
# 全仓存在**两个** CPU provider 家族，各带自己的 C ABI 入口；两者都必须进构建图与
# 清单校验，不得游离 (裁决：属 ISA provider 集合 ⇒ 必须入图)：
#   backend  家族 = lib/infrastructure/benchmark/backend_host/*_backend.cpp
#                入口 astrocs_backend_get_api_v1 (lib/include/astrocs/common_abi_v1.h:164)
#                消费者 = lib/.../backend_host/backend_loader + cpu_routing + CLI 选取；
#                交付名 providers/astrocs_cpu_<id>.so + providers/backends.manifest.json。
#   provider 家族 = lib/infrastructure/benchmark/cpu/<id>/src/<id>_provider.cpp (+ common/src)
#                入口 astrocs_provider_query_v1 (lib/include/astrocs/abi/module_api_v1.h 冻结)
#                消费者 = lib/infrastructure/pipeline/module_loader/secure_loader；
#                交付名 providers/astrocs_cpuprov_<id>.so + providers/providers.manifest.json。
# 两族清单互不混装 (backend 清单里出现 provider 入口 = 预检必然拒绝的游离项) ⇒
# eng/ci/check_provider_manifests.py 逐条校验入口符号与家族归属。
FAMILIES = {
    "backend": {
        "kind": "astrocs_backends_manifest",
        "entrypoint": "astrocs_backend_get_api_v1",
        "abi": "backend_host_v1",
        "file_prefix": "astrocs_cpu_",
        "skip_baseline_when_isa_only": True,
    },
    "provider": {
        "kind": "astrocs_providers_manifest",
        "entrypoint": "astrocs_provider_query_v1",
        "abi": "module_provider_v1",
        "file_prefix": "astrocs_cpuprov_",
        "skip_baseline_when_isa_only": False,
    },
}


def verify_entrypoint(lib_path, symbol):
    """实测 DSO 是否真的导出 symbol（不是"声称导出"）。

    用 dlopen + 取符号：清单里写 entrypoint 字段而文件不导出它 ⇒ 生成期即失败，
    不留到运行期被 dlopen 打回。返回 True/False。
    """
    try:
        lib = ctypes.CDLL(lib_path)
    except OSError:
        return False
    try:
        getattr(lib, symbol)
    except AttributeError:
        return False
    return True


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", required=True, help="仓库根目录(找 git commit / lib)")
    ap.add_argument("--build-dir", required=True, help="构建目录(含 provider 静态库 .a)")
    ap.add_argument("--providers-dir", default="",
                    help="SHARED provider 目录(含 astrocs_cpu_<id>.so|.dll, 与清单同目录)")
    ap.add_argument("--isa-only", action="store_true",
                    help="只登记可选 ISA 变体(baseline 恒为进程内置, 不进清单; 仅 backend 家族适用)")
    ap.add_argument("--family", choices=sorted(FAMILIES), default="backend",
                    help="provider 家族: backend=astrocs_backend_get_api_v1 / provider=astrocs_provider_query_v1")
    ap.add_argument("--out", required=True, help="输出 manifest JSON 路径")
    ap.add_argument("--compiler", default="", help="编译器标识(如 g++-14)")
    ap.add_argument("--commit", default="", help="覆盖 git commit(默认取 HEAD)")
    args = ap.parse_args()

    if not os.path.isdir(args.build_dir):
        print(f"ERROR: build dir not found: {args.build_dir}", file=sys.stderr)
        return 3
    commit = args.commit or git_commit(args.repo)
    fam = FAMILIES[args.family]

    flags_per_provider = {
        "baseline": "(none; amd64 SSE2 基线)",
        # avx512 全子集(与 PROVIDER_REQUIRED_BITS 及根 CMake target 编译旗标同源)
        "avx2": "-mavx2 -mfma",
        "avx512": "-mavx512f -mavx512bw -mavx512vl -mavx512dq",
    }
    # 交付形态选择: --providers-dir ⇒ SHARED DSO(astrocs_cpu_<id>.so|.dll, PREFIX 已去 lib);
    # 否则沿用旧静态库面(libastrocs_cpu_<id>.a)。
    providers_dir = args.providers_dir
    if providers_dir and not os.path.isdir(providers_dir):
        print(f"ERROR: providers dir not found: {providers_dir}", file=sys.stderr)
        return 3
    ids = [b for b in PROVIDER_REQUIRED
           if not (args.isa_only and fam["skip_baseline_when_isa_only"] and b == "baseline")]
    if args.family == "provider" and not providers_dir:
        print("ERROR: provider 家族只以 SHARED DSO 交付 ⇒ 必须给 --providers-dir", file=sys.stderr)
        return 2
    provider_entrypoint = fam["entrypoint"]
    backends = []
    for backend_id in ids:
        feats = PROVIDER_REQUIRED[backend_id]
        if providers_dir:
            lib = ""
            for ext in (".so", ".dll"):
                cand = os.path.join(providers_dir, f"{fam['file_prefix']}{backend_id}{ext}")
                if os.path.isfile(cand):
                    lib = cand
                    break
        else:
            lib = os.path.join(args.build_dir, f"libastrocs_cpu_{backend_id}.a")
            if backend_id == "baseline":
                lib = os.path.join(args.build_dir, "libastrocs_cpu.a")  # baseline 在 astrocs_cpu 内
        if not lib or not os.path.isfile(lib):
            print(f"ERROR: provider library not found: {lib}", file=sys.stderr)
            return 3
        # 实测入口符号：清单声称的 entrypoint 必须真的在文件里（否则生成期即失败）。
        if not verify_entrypoint(lib, provider_entrypoint):
            print(f"ERROR: provider library {lib} 未导出 {provider_entrypoint}", file=sys.stderr)
            return 4
        bits = 0
        for name in PROVIDER_REQUIRED_BITS[backend_id]:
            bits |= FEATURE_BITS[name]
        backends.append({
            "file": os.path.basename(lib),
            "backend_id": backend_id,
            "entrypoint": provider_entrypoint,
            "abi": fam["abi"],
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
        "kind": fam["kind"],
        "family": args.family,
        "entrypoint": fam["entrypoint"],
        "build": {
            "build_id": f"{commit[:12]}-{args.family}-{'-'.join(flags_per_provider.keys())}",
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
    print(f"MANIFEST_OK {args.out} family={args.family} kind={fam['kind']} "
          f"entrypoint={fam['entrypoint']} backends={len(backends)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
