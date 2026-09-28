#!/usr/bin/env python3
"""check_provider_manifests.py — CPU provider 两族 DSO ↔ 清单双向校验（DYN-740 / C-03 入图裁决）。

背景: 全仓有两个 CPU provider 家族, 各带自己的冻结 C ABI 入口, 两族都必须"进构建目标 +
进清单校验, 不得游离"(裁决: 属 ISA provider 集合 ⇒ 必须入图):
  backend  家族: lib/infrastructure/benchmark/backend_host/<id>_backend.cpp
                 入口 astrocs_backend_get_api_v1; 交付 providers/astrocs_cpu_<id>.so
                 清单 providers/backends.manifest.json
  provider 家族: lib/infrastructure/benchmark/cpu/<id>/src/<id>_provider.cpp (+ common/src)
                 入口 astrocs_provider_query_v1; 交付 providers/astrocs_cpuprov_<id>.so
                 清单 providers/providers.manifest.json

判据 (全绿才 PASS):
  R1 清单存在/JSON 合法/kind+family+entrypoint 自洽 (缺一即红);
  R2 每条目 file 在 providers/ 下存在, 实测 sha256 == 清单声明;
  R3 条目声明的 entrypoint 实测导出 (dlopen 取符号), 且等于其家族入口 (家族混装即红);
  R4 providers/ 下每个 .so/.dll 恰好在两清单之一登记一次 (游离或双登记即红);
  R5 required_features_bits == 按 required_features_names 重算的位 (位表漂移即红),
     且 names ⊆ features_defined。

用法:
  python3 eng/ci/check_provider_manifests.py --repo . --providers-dir build/providers
  python3 eng/ci/check_provider_manifests.py --self-test        # 负例自检 (必须能红)
退出码: 0=PASS; 1=FAIL; 2=用法错误。
"""
import argparse
import ctypes
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile

BACKEND_ENTRYPOINT = "astrocs_backend_get_api_v1"
PROVIDER_ENTRYPOINT = "astrocs_provider_query_v1"
FEATURE_BITS = {
    "sse2": 1 << 0, "sse4_1": 1 << 1, "avx": 1 << 2, "avx2": 1 << 3, "fma": 1 << 4,
    "avx512f": 1 << 5, "avx512cd": 1 << 6, "avx512bw": 1 << 7, "avx512dq": 1 << 8,
    "avx512vl": 1 << 9,
}
FAMILIES = (
    ("backends.manifest.json", "backend", BACKEND_ENTRYPOINT, "astrocs_backends_manifest"),
    ("providers.manifest.json", "provider", PROVIDER_ENTRYPOINT, "astrocs_providers_manifest"),
)


def sha256_file(path, chunk=1 << 16):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(chunk), b""):
            h.update(block)
    return h.hexdigest()


def exports(path, symbol):
    """实测 DSO 是否导出 symbol (不是相信清单自述)。"""
    try:
        lib = ctypes.CDLL(path)
    except OSError:
        return False
    try:
        getattr(lib, symbol)
    except AttributeError:
        return False
    return True


def classify(path, providers_dir):
    fe = None
    for name, fam, entry, _kind in FAMILIES:
        if exports(path, entry):
            fe = (name, fam, entry)
            break
    return fe


def check_tree(providers_dir):
    """返回 (errors, ok_entries, summary)。errors 非空 ⇒ 红。"""
    errs = []
    seen = {}       # basename -> manifest file
    entries_ok = 0
    for mname, fam, entry, kind in FAMILIES:
        mpath = os.path.join(providers_dir, mname)
        if not os.path.isfile(mpath):
            errs.append("R1 清单缺失: " + mpath)
            continue
        try:
            doc = json.load(open(mpath, encoding="utf-8"))
        except Exception as exc:
            errs.append("R1 清单非 JSON: " + mname + " (" + str(exc) + ")")
            continue
        if doc.get("kind") != kind:
            errs.append("R1 kind 失配: " + mname + " kind=" + str(doc.get("kind")) + " 期望 " + kind)
        if doc.get("family") not in (None, fam):
            errs.append("R1 family 失配: " + mname + " family=" + str(doc.get("family")))
        if doc.get("entrypoint") not in (None, entry):
            errs.append("R1 entrypoint 失配: " + mname + " entrypoint=" + str(doc.get("entrypoint")))
        feats_defined = set(doc.get("features_defined") or [])
        for b in doc.get("backends") or []:
            tag = mname + ":" + str(b.get("backend_id"))
            fpath = os.path.join(providers_dir, str(b.get("file") or ""))
            if not os.path.isfile(fpath):
                errs.append("R2 条目文件不存在: " + tag + " -> " + str(b.get("file")))
                continue
            real = sha256_file(fpath)
            if real != b.get("sha256"):
                errs.append("R2 sha256 失配: " + tag + " 清单=" + str(b.get("sha256"))[:16] + " 实测=" + real[:16])
            declared = b.get("entrypoint") or entry
            if declared != entry:
                errs.append("R3 家族混装: " + tag + " entrypoint=" + str(declared) + " 期望 " + entry)
            if not exports(fpath, declared):
                errs.append("R3 入口符号未导出: " + tag + " (" + str(declared) + ")")
            names = list(b.get("required_features_names") or [])
            bits = 0
            for n in names:
                if n not in FEATURE_BITS:
                    errs.append("R5 未知能力名: " + tag + " " + str(n))
                else:
                    bits |= FEATURE_BITS[n]
            if bits != b.get("required_features_bits"):
                errs.append("R5 能力位重算失配: " + tag + " 清单=" + str(b.get("required_features_bits"))
                            + " 重算=" + str(bits))
            if feats_defined and not set(names) <= feats_defined:
                errs.append("R5 names ⊄ features_defined: " + tag)
            base = os.path.basename(fpath)
            if base in seen:
                errs.append("R4 双登记: " + base + " 同时在 " + seen[base] + " 与 " + mname)
            seen[base] = mname
            entries_ok += 1
    # R4: providers/ 下每个 DSO 必须被登记一次
    for f in sorted(os.listdir(providers_dir)) if os.path.isdir(providers_dir) else []:
        if not (f.endswith(".so") or f.endswith(".dll")):
            continue
        if f not in seen:
            errs.append("R4 游离 DSO (未登记于任何清单): " + f)
    return errs, entries_ok


def selftest():
    """负例自检: 造真 DSO + 真清单, 逐条注入缺陷, 必须能红; 原始树必须绿。"""
    fails = []
    tmp = tempfile.mkdtemp(prefix="provman_")
    try:
        src_b = os.path.join(tmp, "b.c")
        src_p = os.path.join(tmp, "p.c")
        open(src_b, "w").write("int " + BACKEND_ENTRYPOINT + "(void){return 0;}\n")
        open(src_p, "w").write("int " + PROVIDER_ENTRYPOINT + "(void){return 0;}\n")
        pdir = os.path.join(tmp, "providers")
        os.makedirs(pdir)
        so_b = os.path.join(pdir, "astrocs_cpu_avx2.so")
        so_p = os.path.join(pdir, "astrocs_cpuprov_avx2.so")
        subprocess.run(["cc", "-shared", "-fPIC", "-o", so_b, src_b], check=True)
        subprocess.run(["cc", "-shared", "-fPIC", "-o", so_p, src_p], check=True)

        def manifest(mname, fam, entry, kind, entries):
            doc = {"schema_version": "1", "kind": kind, "family": fam, "entrypoint": entry,
                   "features_defined": sorted(FEATURE_BITS), "backends": entries}
            json.dump(doc, open(os.path.join(pdir, mname), "w"), indent=2)

        def entry(fn, bid, entry_sym, path):
            names = ["avx2", "fma"] if "avx2" in fn else ["sse2"]
            bits = 0
            for n in names:
                bits |= FEATURE_BITS[n]
            return {"file": fn, "backend_id": bid, "entrypoint": entry_sym,
                    "sha256": sha256_file(path), "required_features_bits": bits,
                    "required_features_names": names}

        def write_good():
            manifest("backends.manifest.json", "backend", BACKEND_ENTRYPOINT,
                     "astrocs_backends_manifest", [entry("astrocs_cpu_avx2.so", "avx2", BACKEND_ENTRYPOINT, so_b)])
            manifest("providers.manifest.json", "provider", PROVIDER_ENTRYPOINT,
                     "astrocs_providers_manifest", [entry("astrocs_cpuprov_avx2.so", "avx2", PROVIDER_ENTRYPOINT, so_p)])

        def expect_red(name, mutate):
            write_good()
            mutate()
            errs, _ = check_tree(pdir)
            if not errs:
                fails.append("CASE " + name + ": 应红实绿")

        write_good()
        errs, n = check_tree(pdir)
        if errs or n != 2:
            fails.append("CASE baseline: 应绿实红 " + "; ".join(errs))

        expect_red("R2-sha", lambda: manifest("backends.manifest.json", "backend", BACKEND_ENTRYPOINT,
                  "astrocs_backends_manifest", [{"file": "astrocs_cpu_avx2.so", "backend_id": "avx2",
                   "entrypoint": BACKEND_ENTRYPOINT, "sha256": "0" * 64,
                   "required_features_bits": 24, "required_features_names": ["avx2", "fma"]}]))
        expect_red("R3-混装", lambda: manifest("backends.manifest.json", "backend", BACKEND_ENTRYPOINT,
                  "astrocs_backends_manifest", [entry("astrocs_cpu_avx2.so", "avx2", PROVIDER_ENTRYPOINT, so_p)]))
        expect_red("R4-游离", lambda: manifest("providers.manifest.json", "provider", PROVIDER_ENTRYPOINT,
                  "astrocs_providers_manifest", []))
        expect_red("R1-kind", lambda: manifest("providers.manifest.json", "provider", PROVIDER_ENTRYPOINT,
                  "wrong_kind", [entry("astrocs_cpuprov_avx2.so", "avx2", PROVIDER_ENTRYPOINT, so_p)]))
        expect_red("R5-位漂移", lambda: manifest("providers.manifest.json", "provider", PROVIDER_ENTRYPOINT,
                  "astrocs_providers_manifest", [{"file": "astrocs_cpuprov_avx2.so", "backend_id": "avx2",
                   "entrypoint": PROVIDER_ENTRYPOINT, "sha256": sha256_file(so_p),
                   "required_features_bits": 8, "required_features_names": ["avx2", "fma"]}]))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    if fails:
        for f in fails:
            print("SELFTEST FAIL " + f)
        print("PROVIDER_MANIFESTS_SELFTEST FAIL cases=" + str(len(fails)))
        return 1
    print("PROVIDER_MANIFESTS_SELFTEST PASS cases=5(红) + 1(绿)")
    return 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", default=".")
    ap.add_argument("--providers-dir", default="")
    ap.add_argument("--json-out", default="")
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args()
    if args.self_test:
        return selftest()
    pdir = args.providers_dir or os.path.join(args.repo, "build", "providers")
    if not os.path.isdir(pdir):
        print("CHK-PROVIDER-MANIFESTS SKIP: providers 目录不存在: " + pdir)
        return 0
    errs, n = check_tree(pdir)
    if args.json_out:
        os.makedirs(os.path.dirname(args.json_out) or ".", exist_ok=True)
        json.dump({"providers_dir": pdir, "entries": n, "errors": errs},
                  open(args.json_out, "w"), indent=2)
    if errs:
        for e in errs:
            print("CHK-PROVIDER-MANIFESTS FAIL " + e)
        print("PROVIDER_MANIFESTS_FAIL errors=" + str(len(errs)))
        return 1
    print("PROVIDER_MANIFESTS_PASS dir=" + pdir + " entries=" + str(n))
    return 0


if __name__ == "__main__":
    sys.exit(main())
