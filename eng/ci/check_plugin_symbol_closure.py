#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CHK-PLUGIN-SYMBOL-CLOSURE：交付共享对象（plugin .so）符号闭包机器门。

上游依据：
- ENGINEERING_SPEC.md §7（模块 DLL/SO 是交付单元，安装树落 modules/）；
- docs/ci/01_CHECKS.md §1（每项检查必须能绿能红、fail-closed、锚存活）；
- ASTROCS_DESIGN.md §10（aio 是文件级唯一 I/O 边界）与模块 DLL 的自持闭包约定：
  模块 .so 从源文件独立重编译其依赖闭包（静态库对象非 PIC 不可链），因此
  **每个交付 .so 必须自持其符号闭包**，不得依赖宿主进程导出符号。

判据（任一不满足 ⇒ 非零退出；全部机器可执行）：
  C0 锚存活: 产品清单与构建图存在且可解析；缺失以 ANCHOR_STALE 显式判红。
  C1 登记面闭合: 产品清单里每个 kind != exe 的 unit（运行期 dlopen 载入的共享对象）
     都能在构建图里定位到其 .so 产物；缺一判红。
  C2 闭包解析（静态判据）: 每个 .so 的**强未定义符号**必须能在其依赖闭包内解析。
     依赖闭包 = DT_NEEDED 传递闭包（ld.so 解析出的绝对路径）∪ 宿主基线库
     {libc, libm, libstdc++, libgcc_s, libdl, libpthread, librt, libatomic}。
     宿主基线库是任何 dlopen 调用方都必然已加载的运行时，故计入闭包。
     弱未定义符号（_ITM_registerTMCloneTable、__gmon_start__ 等）按 ELF 语义
     解析为 0，不计入判红，但计入统计输出。
  C3 DT_NEEDED 可解析: 每个 DT_NEEDED 必须能被动态链接器定位（ldd 无 not found）。
  C4 动态确认: dlopen(RTLD_NOW|RTLD_LOCAL) 必须成功；在隔离子进程内执行，
     避免本进程已加载符号掩盖缺口。
  C5 非退化: 被扫描 unit 数为 0 ⇒ 判红（空集不得当通过）。

用法:
  python3 eng/ci/check_plugin_symbol_closure.py [--build-dir DIR] [--json-out FILE]
                                               [--so PATH ...] [--no-dlopen] [--quiet]
  python3 eng/ci/check_plugin_symbol_closure.py --self-test [--json-out FILE]
exit 0 = 全部通过；1 = 判据不满足；2 = 输入不可用（fail-closed）。
"""
from __future__ import annotations

import argparse
import json
import os
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile

REPO = pathlib.Path(__file__).resolve().parent.parent.parent
PRODUCT_ANCHOR = "eng/packaging/astrocs.product.json"
DEFAULT_BUILD_DIRS = ("build", "run/ci/build-gcc-release", "build/linux-control")
BUILD_GRAPH_FILE = "build.ninja"

# 宿主基线库：任何 dlopen 调用方（CLI/加载器/测试进程）都必然已加载的运行时。
BASELINE_HOST_LIBS = (
    "libc.so.6", "libm.so.6", "libstdc++.so.6", "libgcc_s.so.1",
    "libdl.so.2", "libpthread.so.0", "librt.so.1", "libatomic.so.1",
)
# 弱未定义符号在 ELF 语义下解析为 0，不构成载入期失败。
WEAK_TYPES = ("w", "v")
TOOL_TIMEOUT = 60

PASS, FAIL, INPUT = 0, 1, 2


def _run(cmd: list) -> subprocess.CompletedProcess:
    return subprocess.run(cmd, capture_output=True, text=True, timeout=TOOL_TIMEOUT)


def _require_tools() -> list:
    return [t for t in ("nm", "readelf", "ldd") if shutil.which(t) is None]


def dyn_defined(path: str) -> set:
    """动态符号表里已定义的符号名集合（版本后缀已剥离）。"""
    out = _run(["nm", "-D", "--defined-only", path]).stdout
    syms = set()
    for line in out.splitlines():
        parts = line.split()
        if len(parts) >= 3:
            syms.add(parts[-1].split("@")[0])
    return syms


def dyn_undefined(path: str) -> tuple:
    """动态符号表里的未定义符号 -> (强未定义集合, 弱未定义集合)。"""
    out = _run(["nm", "-D", "--undefined-only", path]).stdout
    strong, weak = set(), set()
    for line in out.splitlines():
        parts = line.split()
        if len(parts) < 2:
            continue
        sym_type, name = parts[0], parts[-1].split("@")[0]
        (weak if sym_type in WEAK_TYPES else strong).add(name)
    return strong, weak


def needed_libs(path: str) -> list:
    out = _run(["readelf", "-dW", path]).stdout
    return re.findall(r"\(NEEDED\)\s+Shared library: \[([^\]]+)\]", out)


def loader_closure(path: str) -> tuple:
    """经动态链接器解析出的依赖闭包绝对路径集合 + 未解析的 DT_NEEDED 列表。"""
    res = _run(["ldd", path])
    paths, missing = set(), []
    for line in (res.stdout + res.stderr).splitlines():
        m = re.match(r"\s*(\S+)\s+=>\s+(\S+)", line)
        if m:
            if m.group(2).startswith("/"):
                paths.add(os.path.realpath(m.group(2)))
            else:
                missing.append(m.group(1))
            continue
        if "not found" in line:
            missing.append(line.strip().split()[0])
            continue
        m2 = re.match(r"\s*(/\S+)\s+\(0x", line)
        if m2:
            paths.add(os.path.realpath(m2.group(1)))
    if os.path.exists(path):
        paths.add(os.path.realpath(path))
    for lib in BASELINE_HOST_LIBS:
        p = shutil.which(lib)
        if p:
            paths.add(os.path.realpath(p))
    return paths, sorted(set(missing))


def dlopen_probe(path: str) -> tuple:
    """隔离子进程 dlopen(RTLD_NOW|RTLD_LOCAL)：返回 (ok, 诊断)。"""
    code = ("import ctypes, os, sys\n"
            "ctypes.CDLL(sys.argv[1], mode=os.RTLD_NOW | os.RTLD_LOCAL)\n")
    try:
        res = subprocess.run([sys.executable, "-S", "-c", code, path],
                             capture_output=True, text=True, timeout=TOOL_TIMEOUT)
    except subprocess.TimeoutExpired:
        return False, "dlopen 探针超时"
    if res.returncode == 0:
        return True, ""
    detail = (res.stderr or res.stdout).strip().splitlines()
    return False, detail[-1] if detail else "dlopen 失败"


def demangle(names: list) -> dict:
    if not names or shutil.which("c++filt") is None:
        return {}
    res = _run(["c++filt"] + list(names))
    return dict(zip(names, res.stdout.splitlines()))


def analyze_so(path: str, *, probe: bool = True) -> dict:
    strong, weak = dyn_undefined(path)
    closure, missing_needed = loader_closure(path)
    defined = set()
    for dep in sorted(closure):
        if os.path.exists(dep):
            defined |= dyn_defined(dep)
    gaps = sorted(strong - defined)
    entry = {
        "so": path,
        "dt_needed": needed_libs(path),
        "closure_size": len(closure),
        "undefined_strong": len(strong),
        "undefined_weak": sorted(weak),
        "gaps": gaps,
        "gaps_demangled": demangle(gaps),
        "missing_needed": missing_needed,
        "dlopen_ok": None,
        "dlopen_error": "",
    }
    if probe:
        ok, err = dlopen_probe(path)
        entry["dlopen_ok"] = ok
        entry["dlopen_error"] = err
    return entry


def phony_targets(build_dir: pathlib.Path) -> dict:
    """构建图里 target 名 -> 产物相对路径（CMake 生成的 phony 别名表）。"""
    graph = build_dir / BUILD_GRAPH_FILE
    if not graph.is_file():
        return {}
    mapping = {}
    with open(graph, encoding="utf-8", errors="replace") as fh:
        for line in fh:
            m = re.match(r"^build (\S+): phony (\S+)\s*$", line)
            if m:
                mapping.setdefault(m.group(1), m.group(2))
    return mapping


def target_candidates(rel_path: str) -> list:
    name = os.path.basename(rel_path)
    stem = name[:-3] if name.endswith(".so") else name
    cands = [stem, name]
    if stem.startswith("lib"):
        cands += [stem[3:], stem[3:] + ".so"]
    return cands


def collect_units(repo: pathlib.Path, build_dir: pathlib.Path) -> tuple:
    """产品清单登记的非 exe unit -> [(unit_id, rel_path, 产物绝对路径 或 None)]。"""
    product = json.loads((repo / PRODUCT_ANCHOR).read_text(encoding="utf-8"))
    units = [u for u in product.get("units", []) if u.get("kind") != "exe"]
    mapping = phony_targets(build_dir)
    out = []
    for unit in units:
        rel = str(unit.get("rel_path", ""))
        found = None
        for cand in target_candidates(rel):
            if cand in mapping:
                found = str(build_dir / mapping[cand])
                break
        if found is None and (build_dir / rel).is_file():
            found = str(build_dir / rel)
        out.append((unit.get("unit_id", "?"), rel, found))
    return out, len(mapping)


def verdict_of(entry: dict) -> list:
    reasons = []
    if entry["missing_needed"]:
        reasons.append("MISSING_NEEDED " + ",".join(entry["missing_needed"]))
    if entry["gaps"]:
        shown = entry["gaps"][:8]
        reasons.append("UNRESOLVED_SYMBOL " + ",".join(shown))
    if entry["dlopen_ok"] is False:
        reasons.append("DLOPEN_FAILED " + entry["dlopen_error"])
    return reasons


def report(results: list, skipped: list, *, quiet: bool) -> bool:
    ok = True
    for unit_id, rel, path, entry in results:
        reasons = verdict_of(entry)
        tag = "PASS" if not reasons else "FAIL"
        line = ("[" + tag + "] PLUGIN-CLOSURE " + unit_id + " " + rel
                + " undefined_strong=" + str(entry["undefined_strong"])
                + " gaps=" + str(len(entry["gaps"]))
                + " dlopen=" + str(entry["dlopen_ok"]))
        if not quiet or reasons:
            print(line, flush=True)
        for reason in reasons:
            ok = False
            print("        " + reason, flush=True)
            for sym in entry["gaps"][:8]:
                print("          " + sym + "  ->  "
                      + entry["gaps_demangled"].get(sym, ""), flush=True)
    for unit_id, rel, why in skipped:
        ok = False
        print("[FAIL] PLUGIN-CLOSURE " + unit_id + " " + rel + "  " + why, flush=True)
    return ok


def run_real(repo: pathlib.Path, build_dir: pathlib.Path, *, probe: bool,
             quiet: bool, extra_so: list) -> tuple:
    if not (repo / PRODUCT_ANCHOR).is_file():
        print("ANCHOR_STALE: PRODUCT_MANIFEST " + PRODUCT_ANCHOR, flush=True)
        return INPUT, {"error": "anchor_stale", "anchor": PRODUCT_ANCHOR}
    if not (build_dir / BUILD_GRAPH_FILE).is_file():
        print("ANCHOR_STALE: BUILD_GRAPH " + str(build_dir / BUILD_GRAPH_FILE), flush=True)
        return INPUT, {"error": "build_graph_missing", "build_dir": str(build_dir)}
    units, graph_targets = collect_units(repo, build_dir)
    if graph_targets == 0:
        print("ANCHOR_STALE: BUILD_GRAPH_EMPTY " + str(build_dir / BUILD_GRAPH_FILE), flush=True)
        return INPUT, {"error": "build_graph_empty", "build_dir": str(build_dir)}
    if not units:
        print("[FAIL] PLUGIN-CLOSURE 产品清单未登记任何非 exe unit（空集不得当通过）", flush=True)
        return FAIL, {"error": "empty_unit_set"}

    results, skipped = [], []
    for unit_id, rel, path in units:
        if path is None or not os.path.exists(path):
            skipped.append((unit_id, rel, "NO_ARTIFACT 构建图未产出该 unit 的 .so"))
            continue
        results.append((unit_id, rel, path, analyze_so(path, probe=probe)))
    for path in extra_so:
        if not os.path.exists(path):
            skipped.append(("EXTRA", path, "NO_ARTIFACT 指定路径不存在"))
            continue
        results.append(("EXTRA", os.path.basename(path), path,
                        analyze_so(path, probe=probe)))
    ok = report(results, skipped, quiet=quiet)
    payload = {
        "build_dir": str(build_dir),
        "units_total": len(units),
        "units_analyzed": len(results),
        "verdict": "PASS" if ok else "FAIL",
        "results": [{"unit_id": u, "rel_path": r, "so": p, **e}
                    for u, r, p, e in results],
        "skipped": [{"unit_id": u, "rel_path": r, "reason": w} for u, r, w in skipped],
    }
    if not ok:
        return FAIL, payload
    print("PLUGIN_SYMBOL_CLOSURE: PASS (" + str(len(results)) + "/" + str(len(units))
          + " units, build_dir=" + str(build_dir) + ")", flush=True)
    return PASS, payload


# ── 非退化自检面（--self-test）：正例 + 三类负例，全部机器可执行 ──
FIXTURE_GREEN = "int fx_ok(void) { return 1; }\n"
FIXTURE_DEP = "int fx_dep(void) { return 7; }\n"
FIXTURE_ORPHAN = ("extern int fx_dep(void);\n"
                  "int fx_use(void) { return fx_dep(); }\n")


def _cc() -> str:
    return os.environ.get("CC", "cc")


def _compile(args: list, work: pathlib.Path) -> tuple:
    res = subprocess.run([_cc()] + args, capture_output=True, text=True,
                         cwd=str(work), timeout=TOOL_TIMEOUT)
    return res.returncode == 0, (res.stderr or "").strip()[-400:]


def run_self_test() -> tuple:
    if _require_tools():
        print("SELFTEST [FAIL] 缺外部工具: " + ",".join(_require_tools()), flush=True)
        return INPUT, {"verdict": "FAIL"}
    cases = []

    def case(name: str, ok: bool, detail: str = "") -> None:
        cases.append({"name": name, "ok": bool(ok), "detail": detail})
        print("SELFTEST [" + ("PASS" if ok else "FAIL") + "] " + name
              + (("  " + detail) if detail else ""), flush=True)

    with tempfile.TemporaryDirectory(prefix="pscc-selftest-") as tmp:
        work = pathlib.Path(tmp)
        (work / "green.c").write_text(FIXTURE_GREEN, encoding="utf-8")
        (work / "dep.c").write_text(FIXTURE_DEP, encoding="utf-8")
        (work / "orphan.c").write_text(FIXTURE_ORPHAN, encoding="utf-8")

        built, err = _compile(["-shared", "-fPIC", "dep.c", "-o", "libfxdep.so"], work)
        case("fixture 编译: 依赖库 libfxdep.so", built, err)
        built, err = _compile(["-shared", "-fPIC", "green.c", "-o", "green.so"], work)
        case("fixture 编译: 自持闭包 .so", built, err)
        # 负例 1: 调用者未把定义者纳入 DT_NEEDED —— 同目录存在 libfxdep.so 仍必须判红
        built, err = _compile(["-shared", "-fPIC", "orphan.c", "-o", "orphan.so"], work)
        case("fixture 编译: 未纳入闭包的符号引用", built, err)
        # 正例: 显式 -lfxdep ⇒ 定义者进入闭包
        built, err = _compile(["-shared", "-fPIC", "orphan.c", "-L.", "-lfxdep",
                               "-Wl,-rpath,$ORIGIN", "-o", "needed.so"], work)
        case("fixture 编译: 已纳入闭包的符号引用", built, err)

        green = analyze_so(str(work / "green.so"))
        case("正例绿: 自持闭包 .so 零缺口", not verdict_of(green),
             "gaps=" + str(len(green["gaps"])))
        orphan = analyze_so(str(work / "orphan.so"))
        case("负例红: 未纳入闭包的符号引用被点名",
             orphan["gaps"] == ["fx_dep"], "gaps=" + str(orphan["gaps"]))
        needed = analyze_so(str(work / "needed.so"))
        case("正例绿: 定义者已在 DT_NEEDED 闭包内", not verdict_of(needed),
             "gaps=" + str(needed["gaps"]))
        os.rename(str(work / "libfxdep.so"), str(work / "libfxdep.so.off"))
        broken = analyze_so(str(work / "needed.so"))
        case("负例红: DT_NEEDED 不可解析",
             bool(broken["missing_needed"]) and "fx_dep" in broken["gaps"],
             "missing=" + str(broken["missing_needed"]))
        os.rename(str(work / "libfxdep.so.off"), str(work / "libfxdep.so"))

    rc_empty, _ = run_real(REPO, pathlib.Path(tempfile.mkdtemp(prefix="pscc-nograph-")),
                           probe=False, quiet=True, extra_so=[])
    case("负例红: 构建图缺失 fail-closed", rc_empty == INPUT, "rc=" + str(rc_empty))

    passed = sum(1 for c in cases if c["ok"])
    ok = passed == len(cases)
    print("PLUGIN_SYMBOL_CLOSURE_SELFTEST: " + ("PASS" if ok else "FAIL")
          + " (" + str(passed) + "/" + str(len(cases)) + ")", flush=True)
    return (PASS if ok else FAIL), {"verdict": "PASS" if ok else "FAIL", "cases": cases}


def pick_build_dir(repo: pathlib.Path, explicit: str | None) -> pathlib.Path:
    if explicit:
        return pathlib.Path(explicit)
    for cand in DEFAULT_BUILD_DIRS:
        path = repo / cand
        if (path / BUILD_GRAPH_FILE).is_file():
            return path
    return repo / DEFAULT_BUILD_DIRS[0]


def main(argv: list | None = None) -> int:
    parser = argparse.ArgumentParser(description="plugin .so 符号闭包机器门")
    parser.add_argument("--build-dir", default=None,
                        help="构建树（默认按 " + ",".join(DEFAULT_BUILD_DIRS) + " 顺序取首个已配置树）")
    parser.add_argument("--json-out", default=None)
    parser.add_argument("--so", action="append", default=[], metavar="PATH",
                        help="额外分析的 .so（可重复；用于定点复核）")
    parser.add_argument("--no-dlopen", action="store_true", help="跳过动态确认 C4")
    parser.add_argument("--quiet", action="store_true")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args(argv)

    missing_tools = _require_tools()
    if missing_tools:
        print("ANCHOR_STALE: EXTERNAL_TOOL " + ",".join(missing_tools), flush=True)
        return INPUT

    if args.self_test:
        rc, payload = run_self_test()
    else:
        build_dir = pick_build_dir(REPO, args.build_dir)
        if not args.quiet:
            print("build_dir=" + str(build_dir), flush=True)
        rc, payload = run_real(REPO, build_dir, probe=not args.no_dlopen,
                               quiet=args.quiet, extra_so=list(args.so))
    if args.json_out:
        out = pathlib.Path(args.json_out)
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8")
    return rc


if __name__ == "__main__":
    sys.exit(main())
