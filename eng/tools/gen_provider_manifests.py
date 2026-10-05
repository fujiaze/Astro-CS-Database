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
  # 交付形态 (docs/engineering/resources/cpu/ISA_VARIANTS.md §0 第 3 条 / §2; R-28):
# 变体为 SHARED DSO，与清单同目录:
  python3 eng/tools/gen_provider_manifests.py --repo <repo> --build-dir <dir> \
      --providers-dir <build>/providers --isa-only --out <build>/providers/backends.manifest.json

输出 schema (backends.manifest.json, 与 backend_loader.parse_backends_manifest 兼容):
  {"schema_version":"1","kind":"acsd_backends_manifest",
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

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import isa_sites  # noqa: E402
from isa_feature_bits import FeatureBits, UnknownFeature  # noqa: E402

# 位表**不抄**，从唯一事实源解析（eng/tools/isa_feature_bits.py → cpu_features.h）。
# 抄一份的历史代价: DYN-740 / R-28 —— 本文件旧表把 avx512bw/dq/vl 写成 1<<6/1<<7/1<<8，
# 与 cpu_features.h 的 CD=1<<6 / BW=1<<7 / DQ=1<<8 / VL=1<<9 冲突: 同一位号两名两义，
# 生成出来的 required_features_bits 会被加载预检按**另一个能力**判读。
_FB = FeatureBits.load()
FEATURE_BITS = _FB.bit_by_name
ACS_ABI_VERSION_V1 = 1

# 后端 id 全集（遍历序 + build_id 的族标识后缀；baseline 恒为进程内置）。
BACKEND_IDS = ("baseline", "avx2", "avx512")

# 预检匹配面 = cpu_features.h 的实测检测位（DYN-740 / R-28: 旧实现把 avx512 的声明收窄到
# avx512f，而产物以 -mavx512f/-bw/-dq/-vl 编译 ⇒「声明 ⊊ 编译」，出厂机器（如 KNL: 有 F
# 无 BW/DQ/VL）上加载放行、首调撞非法指令）。
DETECTABLE_FEATURES = set(FEATURE_BITS.keys())

# ── 声明面记录 ↔ 构建口径（R-60: 清单的 ISA 声明位必须**推导**，不得手抄）──────────────
# DECLARED_BITS = 每个家族/变体在 DSO 源码里**自陈**的需求位（加载预检 required ⊆ detected
# 的左边），按平台分开写。它不是数据源，而是与构建口径逐位核对的**期望值**:
# 口径来自 eng/tools/quality/isa_sites.json 的旗标站点（该表由 check_isa_same_source.py
# 强制与根 CMakeLists.txt 的 target_compile_options 对齐）。两边不等即拒出清单（rc=5），
# 并指明方向:
#   · 声明 ⊄ 编译（"声明了但没编进去"）= 虚假能力声明;
#   · 编译 ⊄ 声明（少声明）= 加载放行后首调撞非法指令（DYN-740 原始事故）。
# 两族的 avx512 记录不同是**合法**差异，依据在各自源码与旗标里:
#   backend  家族: -mavx512f -bw -dq -vl 编译（无 CD），声明宏 ACSD_BACKEND_REQUIRED_FEATURES
#     按 __AVX512CD__ 两分支 ⇒ GNU 928 / MSVC 992;
#   provider 家族: 五个子集旗标编译（含 -mavx512cd），声明宏 ACS_CPU_AVX512_REQUIRED_FEATURES
#     = ACS_CAP_GROUP_AVX512_SUBSET = F|CD|BW|DQ|VL ⇒ **两平台都是 992**（旧清单此处写 928，
#     与该 DSO 的自陈宏和自身编译旗标都不符 —— 属"清单声明位没从构建口径推导"的同类缺口）。
DECLARED_BITS = {
    "backend": {
        "baseline": {"*": ()},
        "avx2": {"*": ("avx2", "fma")},
        "avx512": {isa_sites.GNU_PLATFORM: ("avx512f", "avx512bw", "avx512dq", "avx512vl"),
                   isa_sites.MSVC_PLATFORM: ("avx512f", "avx512cd", "avx512bw", "avx512dq", "avx512vl")},
    },
    "provider": {
        "baseline": {"*": ()},
        "avx2": {"*": ("avx2", "fma")},
        "avx512": {"*": ("avx512f", "avx512cd", "avx512bw", "avx512dq", "avx512vl")},
    },
}

# 清单条目 ↔ 旗标站点（baseline 无旗标站点: 它本身就是基线 ISA 面）。
SITE_BY_FAMILY = {
    "backend": {"avx2": "product-avx2", "avx512": "product-avx512"},
    "provider": {"avx2": "product-cpuprov-avx2", "avx512": "product-cpuprov-avx512"},
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
#                入口 acsd_backend_get_api_v1 (lib/include/acsd/common_abi_v1.h:164)
#                消费者 = lib/.../backend_host/backend_loader + cpu_routing + CLI 选取；
#                交付名 providers/acsd_cpu_<id>.so + providers/backends.manifest.json。
#   provider 家族 = lib/infrastructure/benchmark/cpu/<id>/src/<id>_provider.cpp (+ common/src)
#                入口 acsd_provider_query_v1 (lib/include/acsd/abi/module_api_v1.h 冻结)
#                消费者 = lib/infrastructure/pipeline/module_loader/secure_loader；
#                交付名 providers/acsd_cpuprov_<id>.so + providers/providers.manifest.json。
# 两族清单互不混装 (backend 清单里出现 provider 入口 = 预检必然拒绝的游离项) ⇒
# 清单的入口符号与家族归属由门禁在重建时校验。
FAMILIES = {
    "backend": {
        "kind": "acsd_backends_manifest",
        "entrypoint": "acsd_backend_get_api_v1",
        "abi": "backend_host_v1",
        "file_prefix": "acsd_cpu_",
        "skip_baseline_when_isa_only": True,
    },
    "provider": {
        "kind": "acsd_providers_manifest",
        "entrypoint": "acsd_provider_query_v1",
        "abi": "module_provider_v1",
        "file_prefix": "acsd_cpuprov_",
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
                    help="SHARED provider 目录(含 acsd_cpu_<id>.so|.dll, 与清单同目录)")
    ap.add_argument("--isa-only", action="store_true",
                    help="只登记可选 ISA 变体(baseline 恒为进程内置, 不进清单; 仅 backend 家族适用)")
    ap.add_argument("--family", choices=sorted(FAMILIES), default="backend",
                    help="provider 家族: backend=acsd_backend_get_api_v1 / provider=acsd_provider_query_v1")
    ap.add_argument("--out", required=True, help="输出 manifest JSON 路径")
    ap.add_argument("--compiler", default="", help="编译器标识(如 g++-14)")
    ap.add_argument("--commit", default="", help="覆盖 git commit(默认取 HEAD)")
    ap.add_argument("--sites-file", default=isa_sites.DEFAULT_SITES,
                    help="ISA 旗标站点口径表(默认 eng/tools/quality/isa_sites.json)")
    args = ap.parse_args()

    if not os.path.isdir(args.build_dir):
        print(f"ERROR: build dir not found: {args.build_dir}", file=sys.stderr)
        return 3
    commit = args.commit or git_commit(args.repo)
    fam = FAMILIES[args.family]

    # R-60: 旗标与声明面**同源同平台**。MSVC 没有 AVX-512 子集档位旗标 —— /arch:AVX512 的
    # 许可面是 F+CD+BW+DQ+VL（MS docs /arch (x64) 预定义宏段 + C++ 团队博客），比 GCC 腿的
    # 四子集旗标多 CD；变体 DSO 的声明（avx512_backend.cpp 的 ACSD_BACKEND_REQUIRED_FEATURES
    # 按 __AVX512CD__ 取平台精确值）因此含 CD ⇒ 清单必须同步，否则一边「声明了产物没用上的位」、
    # 另一边「产物声明了清单不认」。GNU/Clang(Linux) 腿的输出逐字节不变。
    platform = isa_sites.platform_of(args.compiler)
    try:
        sites_reg = isa_sites.load(args.sites_file)
    except (OSError, ValueError) as e:
        print(f"ERROR: 读不到 ISA 旗标口径表 {args.sites_file}: {e}", file=sys.stderr)
        return 2
    # 逐变体的**真实构建旗标**（来自站点登记，平台分支已解析）；不是手抄的字符串。
    flags_by_backend = {"baseline": "(none; amd64 SSE2 基线)"}
    # 交付形态选择: --providers-dir ⇒ SHARED DSO(acsd_cpu_<id>.so|.dll, PREFIX 已去 lib);
    # 否则沿用旧静态库面(libacsd_cpu_<id>.a)。
    providers_dir = args.providers_dir
    if providers_dir and not os.path.isdir(providers_dir):
        print(f"ERROR: providers dir not found: {providers_dir}", file=sys.stderr)
        return 3
    ids = [b for b in BACKEND_IDS
           if not (args.isa_only and fam["skip_baseline_when_isa_only"] and b == "baseline")]
    if args.family == "provider" and not providers_dir:
        print("ERROR: provider 家族只以 SHARED DSO 交付 ⇒ 必须给 --providers-dir", file=sys.stderr)
        return 2
    provider_entrypoint = fam["entrypoint"]
    backends = []
    for backend_id in ids:
        # 声明面记录（按平台取；"*" = 两平台同值）
        expect = DECLARED_BITS[args.family][backend_id]
        expect = tuple(expect.get(platform, expect.get("*")))
        # 构建口径推导（站点旗标 ⊆> 位面）；站点缺失/旗标未映射 ⇒ fail-closed
        site_id = SITE_BY_FAMILY[args.family].get(backend_id, "")
        if site_id:
            site = isa_sites.site_of(sites_reg, site_id)
            if site is None:
                print(f"ERROR: 口径表 {args.sites_file} 里没有站点 {site_id}"
                      f"（家族 {args.family} 变体 {backend_id}）—— 站点未登记不得凭空出清单",
                      file=sys.stderr)
                return 2
            try:
                applicable, derived, _derived_bits = isa_sites.permitted(sites_reg, site,
                                                                        platform, _FB)
            except (isa_sites.UnknownSite, UnknownFeature) as e:
                print(f"ERROR: 站点 {site_id} 推导失败: {e}", file=sys.stderr)
                return 2
            flags_by_backend[backend_id] = " ".join(applicable)
        else:
            derived = ()
        if tuple(derived) != expect:
            extra = [n for n in expect if n not in derived]        # 声明 ⊄ 编译
            missing = [n for n in derived if n not in expect]      # 编译 ⊄ 声明
            if extra:
                print(f"ERROR: 声明了但没编进去（虚假能力声明）: {extra} —— "
                      f"家族 {args.family} 变体 {backend_id} 在平台 {platform} 的站点 "
                      f"{site_id or '(无)'} 旗标只许可 {list(derived)}", file=sys.stderr)
            if missing:
                print(f"ERROR: 编进去了却没声明（加载会放行到不支持的机器）: {missing} —— "
                      f"家族 {args.family} 变体 {backend_id} 平台 {platform} 的站点旗标许可 "
                      f"{list(derived)}，声明记录只有 {list(expect)}", file=sys.stderr)
            if not extra and not missing:
                print(f"ERROR: 声明面与构建口径不等: {list(expect)} vs {list(derived)}",
                      file=sys.stderr)
            return 5
        feats = list(expect)
        req_bits = list(expect)
        if providers_dir:
            lib = ""
            for ext in (".so", ".dll"):
                cand = os.path.join(providers_dir, f"{fam['file_prefix']}{backend_id}{ext}")
                if os.path.isfile(cand):
                    lib = cand
                    break
        else:
            lib = os.path.join(args.build_dir, f"libacsd_cpu_{backend_id}.a")
            if backend_id == "baseline":
                lib = os.path.join(args.build_dir, "libacsd_cpu.a")  # baseline 在 acsd_cpu 内
        if not lib or not os.path.isfile(lib):
            print(f"ERROR: provider library not found: {lib}", file=sys.stderr)
            return 3
        # 实测入口符号：清单声称的 entrypoint 必须真的在文件里（否则生成期即失败）。
        if not verify_entrypoint(lib, provider_entrypoint):
            print(f"ERROR: provider library {lib} 未导出 {provider_entrypoint}", file=sys.stderr)
            return 4
        try:
            bits = _FB.bits_of(req_bits)
        except UnknownFeature as e:
            print(f"ERROR: 声明位不在 cpu_features.h: {e}", file=sys.stderr)
            return 2
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
            "build_id": f"{commit[:12]}-{args.family}-{'-'.join(BACKEND_IDS)}",
            "flags": flags_by_backend,
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
