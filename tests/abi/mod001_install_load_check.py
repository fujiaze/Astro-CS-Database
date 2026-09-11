#!/usr/bin/env python3
"""MOD-001 科学 DLL 安装加载验证与产品清单 — 机器验收编排（Linux amd64）。

任务: ASTROCS-CONSTITUTION-ALIGNMENT-V1 / MOD-001（宪章 §8.4 科学模块独立
DLL/SO + §8.5 基建构建单元 + §8.1 CLI 产品清单 + §18.4 只加载签名清单官方模块;
依赖 AIO-001 F-AIO-003 生产接线归属 MOD-001）。

被测事实链（本脚本逐段断言, 任一失败 → 非零退出, 绝不静默 PASS）:
  S0 安全 loader 自检（sha256 FIPS 向量 + 布局断言）;
  S1 科学模块 DLL/平台 SHARED 构建在位（build 树 7 模块 .so + astrocs exe）;
  S2 cmake --install 产生白名单安装树（cmake/install_layout.cmake 唯一 install 源）;
  S3 packaging/verify_install_tree.py: required 全在 + product manifest units 全在;
  S4 产品清单完整性: astrocs.product.json 与 install-tree.contract.json 同步登记
     6 个科学模块 DLL（astrocs.catalog.gaia / astrocs.p1.drizzle /
     astrocs.p1.calibration / astrocs.p1.cosmetic / astrocs.p1.hips_writer /
     astrocs.p1.noise）, manifest rel_path ↔ contract install_path ↔ 安装树文件
     三方一致; sha256=null 合法（BLD-003 骨架约定, 打包期填充; 本脚本在 S6 以
     实测 sha256 传入 loader 验证 hash 核对路径）;
  S5 F-AIO-003 生产接线: lib/astro_image_io/src/aio_abi.cpp 符号（aio_abi_query_v1）
     在生产 target libastrocs_aio.a 内; 直链生产库的探针 aio_abi_query_v1 握手
     abi_version=1/status_count=71 成功;
  S6 安全 loader 加载验证（探针 = tests/abi/abi003_loader_probe.c, 合同 =
     runtime/module_loader/secure_loader.h; 绝无静态 fallback）:
     正路径 — manifest 声明的每个 kind=module unit（noop + 6 科学 DLL）按
       安装树绝对路径 + 实测 sha256 + module_id + allowed_root=安装树 加载成功,
       describe 回读 module_id 与 manifest 登记逐一一致（签名清单官方模块语义）;
     负路径 — sha256 篡改 → ACS_LOADER_EC_HASH_MISMATCH(5);
       module_id 错配 → ACS_LOADER_EC_MODULE_ID_MISMATCH(15);
       allowed_root 越界 → ACS_LOADER_EC_PATH_ESCAPE(4);
       文件缺失 → ACS_LOADER_EC_FILE_MISSING(6)。全部必败（fail-closed）;
  S7 CLI 联动（安装树内 exe）: modules list --json verdict=PASS units=11;
     modules verify rc=0; selftest --module <科学模块> 逐模块装配 PASS;
     selftest --module astrocs.not.in.manifest 必败 rc!=0;
  S8 安装树完整性负向（fail-closed 破坏性注入, 最后执行）: 删除
     modules/astrocs_p1_calibration.so → verify_install_tree 明确失败
     （MISSING REQUIRED）+ CLI modules verify 非零（无静态 fallback 证明）。

用法:
  python3 tests/abi/mod001_install_load_check.py [--build-dir <dir>] [--keep]
  退出码 0 = 全部通过; 1 = 任一失败。ASTROCS_MOD001_BUILD_DIR 可指定构建树。

测试有牙齿: S6 负路径与 S7/S8 注入在本脚本内直接证明"断链/篡改/缺失必败",
不依赖外部变异。
"""
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import time

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
INC = os.path.join(REPO, "include")
LOADER_DIR = os.path.join(REPO, "runtime", "module_loader")
PROBE_C = os.path.join(REPO, "tests", "abi", "abi003_loader_probe.c")
VERIFY_SCRIPT = os.path.join(REPO, "packaging", "verify_install_tree.py")
CC = os.environ.get("CC", "gcc")
TIMEOUT = 900

# 产品清单合同锚（MOD-001: 6 个科学模块 DLL 必须登记且可加载; 唯一事实源 =
# lib/<mod>/module.yaml 的 module_id + CMake SHARED target OUTPUT_NAME）
SCIENCE_MODULES = [
    ("MOD-CAT-GAIA",   "astrocs.catalog.gaia",    "astrocs_catalog_gaia"),
    ("MOD-P1-DRIZZLE", "astrocs.p1.drizzle",      "astrocs_p1_drizzle"),
    ("MOD-P1-CAL",     "astrocs.p1.calibration",  "astrocs_p1_calibration"),
    ("MOD-P1-COS",     "astrocs.p1.cosmetic",     "astrocs_p1_cosmetic"),
    ("MOD-P1-HIPSW",   "astrocs.p1.hips_writer",  "astrocs_p1_hips_writer"),
    ("MOD-P1-NOISE",   "astrocs.p1.noise",        "astrocs_p1_noise"),
]
SCIENCE_TARGETS = [t[2] for t in SCIENCE_MODULES]
PLATFORM_TARGETS = ["astrocs", "astrocs_runtime", "astrocs_io", "astrocs_noop",
                    "astrocs_cpu_baseline"]

FAILURES = []
CHECKS_TOTAL = [0]


def check(name, cond, detail=""):
    tag = "PASS" if cond else "FAIL"
    print(f"[{tag}] {name}" + (f"  {detail}" if detail else ""), flush=True)
    CHECKS_TOTAL[0] += 1
    if not cond:
        FAILURES.append(name)


def run(cmd, cwd=None, env=None, timeout=TIMEOUT):
    t0 = time.time()
    r = subprocess.run(cmd, capture_output=True, text=True, cwd=cwd, env=env,
                       timeout=timeout)
    r.elapsed = time.time() - t0
    return r


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 16), b""):
            h.update(chunk)
    return h.hexdigest()


def compile_probe(work):
    loader_o = os.path.join(work, "secure_loader.o")
    r = run([CC, "-std=c11", "-Wall", "-fno-exceptions", "-fPIC", "-c",
             f"-I{INC}", f"-I{LOADER_DIR}",
             os.path.join(LOADER_DIR, "secure_loader.c"), "-o", loader_o])
    check("compile secure_loader.c (loader 合同源)", r.returncode == 0,
          r.stderr[-400:] if r.returncode else "")
    if r.returncode != 0:
        return None
    probe = os.path.join(work, "abi003_probe")
    r = run([CC, "-std=c11", "-Wall", "-fno-exceptions",
             f"-I{INC}", f"-I{LOADER_DIR}",
             PROBE_C, loader_o, "-ldl", "-o", probe])
    check("compile abi003_loader_probe", r.returncode == 0,
          r.stderr[-400:] if r.returncode else "")
    if r.returncode != 0:
        return None
    return probe


def compile_aio_abi_probe(work, build):
    """S5: 直链生产 libastrocs_aio.a（F-AIO-003 接线后含 aio_abi.o）的握手探针。"""
    src = os.path.join(work, "mod001_aio_abi_probe.cpp")
    with open(src, "w", encoding="utf-8") as f:
        f.write(r"""// MOD-001 S5: aio_abi 生产接线探针 — 直链生产 astrocs_aio 静态库
// 调用 aio_abi_query_v1 握手（F-AIO-003: aio_abi.cpp 编入 astrocs_aio target）。
#include "astrocs/io/aio_abi_v1.h"
#include <cstdio>
int main() {
    aio_abi_info_v1 info;
    info.struct_size = (uint32_t)sizeof(aio_abi_info_v1);
    info.abi_version = AIO_ABI_VERSION_V1;
    int st = aio_abi_query_v1(&info);
    printf("AIO_ABI_QUERY status=%d abi=%u status_count=%u\n",
           st, (unsigned)info.abi_version, (unsigned)info.status_count);
    return (st == AIO_OK && info.abi_version == AIO_ABI_VERSION_V1) ? 0 : 1;
}
""")
    exe = os.path.join(work, "mod001_aio_abi_probe")
    aio_lib = os.path.join(build, "libastrocs_aio.a")
    common_lib = os.path.join(build, "libastrocs_common.a")
    r = run(["g++", "-std=c++17", f"-I{INC}", src, aio_lib, common_lib,
             "-o", exe])
    check("compile aio_abi probe (直链生产 libastrocs_aio.a)", r.returncode == 0,
          r.stderr[-500:] if r.returncode else "")
    if r.returncode != 0:
        return None
    return exe


def find_built_so(build, name):
    """在构建树定位模块 .so 实际输出路径（Linux 默认 build/<subdir>/;
    排除 asan/tsan 并行构建树）。"""
    skip = ("/asan/", "/tsan/")
    for root, _dirs, files in os.walk(build):
        if any(s in (root + "/") for s in skip):
            continue
        if name + ".so" in files:
            return os.path.join(root, name + ".so")
    return None


def load_module(probe, abs_path, module_id, sha, root, expect_ok, tag):
    """探针 load 一发; expect_ok=True 断言 LOAD_OK+module_id 回读一致;
    expect_ok='fail:<detail>' 断言 LOAD_FAIL detail 匹配。"""
    r = run([probe, "load", "module", abs_path,
             module_id if module_id else "-", sha if sha else "-",
             "-", root, "1"])
    out = (r.stdout or "") + (r.stderr or "")
    if expect_ok is True:
        ok = r.returncode == 0 and out.startswith("LOAD_OK")
        mid_ok = ok and f"module_id={module_id} " in out + " "
        check(tag, ok and mid_ok, out.strip()[:220])
        return out
    want = int(expect_ok.split(":", 1)[1])
    ok = out.startswith("LOAD_FAIL") and f"detail={want}" in out
    check(tag + f" (detail={want} 必败)", ok, out.strip()[:220])
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--build-dir", default=os.environ.get("ASTROCS_MOD001_BUILD_DIR")
                    or os.path.join(REPO, "build"))
    ap.add_argument("--keep", action="store_true", help="保留安装树（证据复核）")
    args = ap.parse_args()
    build = os.path.abspath(args.build_dir)
    stamp = time.strftime("%Y%m%dT%H%M%SZ", time.gmtime())
    work = os.path.join(REPO, "run", "mod001_install_load", stamp)
    os.makedirs(work, exist_ok=True)
    print(f"repo={REPO}\nbuild={build}\nwork={work}", flush=True)

    if not os.path.isdir(build):
        print(f"FATAL: build dir missing: {build}（先 cmake -S . -B {build}）")
        return 1

    # ── S0: loader 自检 ──
    probe = compile_probe(work)
    if probe is None:
        return 1
    r = run([probe, "selftest"])
    check("S0 loader selftest (sha256 FIPS 向量+布局)", r.returncode == 0
          and "SELFTEST_OK" in r.stdout, (r.stdout + r.stderr).strip()[:200])

    # ── S1: 构建在位（缺失则构建, rc 判定; 幂等） ──
    targets = PLATFORM_TARGETS + SCIENCE_TARGETS
    missing = [t for t in SCIENCE_TARGETS
               if not os.path.isfile(os.path.join(build, t + ".so"))]
    if missing:
        r = run(["cmake", "--build", build, "--target"] + targets
                + ["-j", str(os.cpu_count() or 2)])
        check("S1 build science module DLLs " + (str(missing) if missing else ""),
              r.returncode == 0, r.stderr[-600:] if r.returncode else "")
    so_paths = {t: find_built_so(build, t) for t in SCIENCE_TARGETS}
    missing_paths = [t for t, p in so_paths.items() if not p]
    check("S1 build tree science .so 全在（构建树实际输出位）",
          not missing_paths, f"missing: {missing_paths}" if missing_paths else "")
    check("S1 build tree astrocs exe 在", os.path.isfile(os.path.join(build, "astrocs")))

    # ── S2: 安装树 ──
    prefix = os.path.join(work, "install")
    r = run(["cmake", "--install", build, "--prefix", prefix])
    check("S2 cmake --install rc=0", r.returncode == 0, r.stderr[-500:] if r.returncode else "")
    if r.returncode != 0:
        return 1

    # ── S3: verify_install_tree.py ──
    r = run([sys.executable, VERIFY_SCRIPT, "--prefix", prefix,
             "--json-out", os.path.join(work, "install_tree_report.json")])
    check("S3 verify_install_tree rc=0", r.returncode == 0,
          (r.stdout + r.stderr)[-600:] if r.returncode else "")

    # ── S4: 产品清单完整性 ──
    manifest_path = os.path.join(prefix, "astrocs.product.json")
    contract_path = os.path.join(REPO, "packaging", "install-tree.contract.json")
    try:
        with open(manifest_path, encoding="utf-8") as f:
            manifest = json.load(f)
        manifest_ok = True
    except Exception as e:  # noqa: BLE001
        manifest, manifest_ok = {}, False
        check("S4 manifest 可解析", False, str(e))
    with open(contract_path, encoding="utf-8") as f:
        contract = json.load(f)
    if manifest_ok:
        munits = {u.get("unit_id"): u for u in manifest.get("units", [])
                  if isinstance(u, dict)}
        cunits = {u.get("unit_id"): u for u in contract.get("units", [])
                  if isinstance(u, dict)}
        check("S4 manifest units=11 (5 平台 + 6 科学)", len(munits) == 11,
              f"got {len(munits)}: {sorted(munits)}")
        for uid, mid, so in SCIENCE_MODULES:
            mu = munits.get(uid)
            cu = cunits.get(uid)
            rel = f"modules/{so}.so"
            check(f"S4 manifest[{uid}] kind/rel/module_id/status",
                  bool(mu) and mu.get("kind") == "module"
                  and mu.get("rel_path") == rel
                  and mu.get("module_id") == mid
                  and mu.get("status") == "IMPLEMENTED"
                  and mu.get("sha256", "missing") in (None, "missing"),
                  json.dumps(mu, ensure_ascii=False)[:200] if mu else "unit missing")
            check(f"S4 contract[{uid}] module/required/一致",
                  bool(cu) and cu.get("kind") == "module"
                  and cu.get("install_path") == rel
                  and cu.get("required") is True
                  and (mu is None or mu.get("rel_path") == cu.get("install_path")),
                  json.dumps(cu, ensure_ascii=False)[:200] if cu else "unit missing")
            check(f"S4 安装树文件在位 {rel}",
                  os.path.isfile(os.path.join(prefix, rel)), rel)

    # ── S5: F-AIO-003 生产接线 ──
    aio_lib = os.path.join(build, "libastrocs_aio.a")
    r = run(["nm", "--defined-only", aio_lib])
    sym_ok = r.returncode == 0 and "aio_abi_query_v1" in r.stdout \
        and "aio_content_hash_buffer_v1" in r.stdout
    check("S5 libastrocs_aio.a 含 aio_abi_* 生产符号 (F-AIO-003)", sym_ok,
          f"rc={r.returncode}")
    aio_probe = compile_aio_abi_probe(work, build)
    if aio_probe:
        r = run([aio_probe])
        check("S5 aio_abi_query_v1 生产库握手 abi=1", r.returncode == 0
              and "status=0" in r.stdout and "abi=1" in r.stdout,
              (r.stdout + r.stderr).strip()[:200])

    # ── S6: 安全 loader 逐 unit 加载（manifest 声明即签名清单语义） ──
    module_units = []
    if manifest_ok:
        module_units = [u for u in manifest.get("units", [])
                        if isinstance(u, dict) and u.get("kind") == "module"]
    check("S6 manifest kind=module units=7 (noop+6 科学)", len(module_units) == 7,
          f"got {len(module_units)}")
    for u in module_units:
        rel = u.get("rel_path", "")
        mid = u.get("module_id") or ""
        abs_path = os.path.join(prefix, rel)
        real = sha256_file(abs_path) if os.path.isfile(abs_path) else ""
        check(f"S6 实测 sha256 可计算 {rel}", len(real) == 64)
        load_module(probe, abs_path, mid, real, prefix, True,
                    f"S6 LOAD_OK {mid} (sha256+module_id+root 校验)")
        # 负1: sha256 篡改 → HASH_MISMATCH(5)
        bad_sha = ("0" if real[0] != "0" else "1") + real[1:]
        load_module(probe, abs_path, mid, bad_sha, prefix, "fail:5",
                    f"S6 注入: sha256 篡改 {mid}")
        # 负2: module_id 错配 → MODULE_ID_MISMATCH(15)
        load_module(probe, abs_path, "astrocs.wrong.id", real, prefix, "fail:15",
                    f"S6 注入: module_id 错配 {mid}")
    # 负3: allowed_root 越界（build 树文件不在安装树 root 内）→ PATH_ESCAPE(4)
    build_so = so_paths.get("astrocs_p1_calibration")
    if build_so:
        load_module(probe, build_so, "-", "-", prefix, "fail:4",
                    "S6 注入: root 越界 (build 树路径 vs 安装树 root)")
    # 负4: 文件缺失 → FILE_MISSING(6)
    load_module(probe, os.path.join(prefix, "modules", "astrocs_missing_mod.so"),
                "astrocs.missing", "-", prefix, "fail:6",
                "S6 注入: 文件缺失")

    # ── S7: CLI 联动（安装树内） ──
    exe = os.path.join(prefix, "astrocs")
    env = dict(os.environ)
    r = run([exe, "modules", "list", "--json"], cwd=prefix, env=env)
    try:
        doc = json.loads(r.stdout)
    except Exception:  # noqa: BLE001
        doc = {}
    check("S7 modules list --json verdict=PASS units=11",
          r.returncode == 0 and doc.get("verdict") == "PASS"
          and len(doc.get("units", [])) == 11,
          (r.stdout + r.stderr)[-400:] if r.returncode or not doc else "")
    r = run([exe, "modules", "verify"], cwd=prefix, env=env)
    check("S7 modules verify rc=0", r.returncode == 0
          and "modules verify OK (11 units" in r.stdout,
          (r.stdout + r.stderr)[-300:])
    for _, mid, _so in SCIENCE_MODULES:
        r = run([exe, "selftest", "--module", mid, "--json"], cwd=prefix, env=env)
        ok = r.returncode == 0 and f'"module_assembly:{mid}"' in r.stdout \
            and '"status": "pass"' in r.stdout
        check(f"S7 selftest --module {mid} 装配 PASS", ok,
              (r.stdout + r.stderr)[-300:] if not ok else "")
    r = run([exe, "selftest", "--module", "astrocs.not.in.manifest", "--json"],
            cwd=prefix, env=env)
    check("S7 selftest 未登记 module 必败 rc!=0", r.returncode != 0,
          f"rc={r.returncode}")

    # ── S8: 破坏性注入（最后执行; 安装树一次性） ──
    victim = os.path.join(prefix, "modules", "astrocs_p1_calibration.so")
    if os.path.isfile(victim):
        os.remove(victim)
        r = run([sys.executable, VERIFY_SCRIPT, "--prefix", prefix])
        check("S8 注入: 删科学 DLL → verify_install_tree 必败(MISSING REQUIRED)",
              r.returncode != 0 and "MISSING REQUIRED" in (r.stdout + r.stderr),
              (r.stdout + r.stderr)[-300:])
        r = run([exe, "modules", "verify"], cwd=prefix, env=env)
        check("S8 注入: 删科学 DLL → CLI modules verify 必败(无静态 fallback)",
              r.returncode != 0 and "missing_unit_file" in r.stdout,
              f"rc={r.returncode}")
    else:
        check("S8 注入: 删科学 DLL", False, f"victim missing: {victim}")

    total = CHECKS_TOTAL[0]
    verdict = "PASS" if not FAILURES else "FAIL"
    print(f"\nMOD001_INSTALL_LOAD_CHECK {verdict}: {total - len(FAILURES)}/{total} checks")
    if FAILURES:
        print("failed: " + "; ".join(FAILURES[:20]))
    if args.keep:
        print(f"install tree kept: {prefix}")
    elif os.path.isdir(work):
        shutil.rmtree(work, ignore_errors=True)
    return 0 if not FAILURES else 1


if __name__ == "__main__":
    sys.exit(main())
