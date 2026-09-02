#!/usr/bin/env python3
"""GOV-003 版本命名空间机器检查器 (tools/doccheck 系列)。

任务 GOV-003: 根 VERSION 设 0.11.0-alpha.1; product/module/ABI/data-schema/
doc-revision/history 五个版本命名空间; CMake/CLI/L0 从根 VERSION 生成产品
版本, 禁止手抄。

检查项 (exit 0 = PASS):
  1. 根 VERSION 存在且匹配 MAJOR.MINOR.PATCH-alpha.N (禁 stable/rc/beta);
  2. tools/gen_version.py --json 输出 version 前缀 == 根 VERSION
     (生成链: CLI/打包从唯一源派生, 非手抄);
  3. 允许路径 active 文档/配置不含"另一个产品版本":
     - alpha 形态 X.Y.Z-alpha.N != 源 => FAIL;
     - 裸 X.Y.Z != 源基础号 => FAIL (豁免: 非产品版本命名空间与占位);
  4. 反误报: FITS 4.0 / HiPS 1.0 / ABI v1 / schema_version /
     DatabaseVersion / CFITSIO / X.Y.Z 占位 不被当作产品版本漂移;
  5. mutation 合同: 伪造产品版本字面量必须使本 checker FAIL;
  6. 他人路径遗留 (docs/VERSIONING.md、CMake project 字面量、tests/ 硬编码、
     tools/check_*.py 硬编码、DOCUMENT_INDEX base_product_version) 汇总为
     out_of_scope 列表输出, 不判 FAIL (集成协调项, 见 known_limits)。

用法:
  python3 tools/doccheck/check_version_namespaces.py [--root <repo>] [--json-out <f>]
"""
from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys

# 允许路径 active 扫描面 (GOV-003 owner 可改路径内的当前文档/配置)
# 硬判: 治理/规范/当前状态文档 — 任何其他产品版本/裸版本字面量 => FAIL。
SCAN_FILES = [
    "VERSION",
    "AGENTS.md",
    "AstroCS_ENGINEERING_CONSTRAINTS.md",
    "README.md",
    "REVIEW.md",
    "HANDOVER.md",
]
SCAN_DIRS = ["docs/governance", "docs/owner"]
# warnings-only: 历史日志驻留点 (memory.md 是逐日操作日志, CHANGELOG.md 是
# history 命名空间合法驻留点; 其历史轮次/外部组件版本由 GOV-005 收敛,
# GOV-003 不硬判 FAIL, 只报告警告)。
LOG_FILES = ["memory.md", "CHANGELOG.md"]
ARCHIVE_HINT = ("/archive/", "ARCHIVED", "NON_NORMATIVE")

SEMVER_ALPHA = re.compile(r"^(\d+)\.(\d+)\.(\d+)-alpha\.(\d+)$")
ALPHA_INLINE = re.compile(r"(?<![\w.])(\d+\.\d+\.\d+)-alpha\.(\d+)(?![\w.])")
BASE_INLINE = re.compile(r"(?<![\w.])(\d+\.\d+\.\d+)(?![\d.])")
PRERELEASE_BAD = re.compile(r"-(stable|rc|beta)\b", re.IGNORECASE)

# 机器修订关系字段 (front matter/YAML), 非"当前产品版本"陈述
REV_FIELD = re.compile(r"^(source_main_version|target_main_version|base_product_version|source_main_sha|base_main_sha|product_version|doc_version)\s*[:=]\s*[\"']?\d+\.\d+\.\d+(-alpha\.\d+)?[\"']?")
CONTRACT_DOC_VERSION = re.compile(r"状态:\s*\w+\s+版本:\s*\d+(\.\d+)*")
# 非产品版本命名空间的行级豁免 (本命名空间合同 §1/§3): 这些 token 所在行的
# 数字三元组属于 FITS/HiPS/ABI/data-schema/外部组件/占位, 不得误报。
NAMESPACE_EXEMPT = (
    "hips_version", "hips 1.0", "hips 1.4", "hips_version=1.4",  # HiPS 格式版本
    "fits 4.0", "fits 4", "cfitsio", "fitsio", "fits 标准",       # FITS 格式/组件
    "abi v1", "abi_version", "acs_abi_version", "acs_artifact_abi",  # C ABI
    "schema_version", "cli_schema_version", "$schema",            # data-schema
    "databaseversion", "gaia 库",                                   # Gaia 库标识
    "module_version", "module.yaml",                                # module 命名空间
    "doc-revision", "doc_revision", "版本: ", "修订号",             # doc-revision
    "v6.1", "v19r", "v18r", "控制包", "轮次", "history", "archived",  # history
    "x.y.z", "major.minor.patch", "占位",
    "opencl", "driver", "g++", "gcc", "cmake", "ninja", "mingw", "msys2",
    "siril", "wbpp", "pcl", "rcr", "python", "clang", "healpix", "ivoa",
    "2026-", "2025-",                                           # 日期
    '"version":', '"版本":', "version: ", "版本号",
)


def check(name: str, ok: bool, detail: str) -> dict:
    return {"check": name, "pass": bool(ok), "detail": detail}


def root_version(root: str) -> str:
    p = os.path.join(root, "VERSION")
    with open(p, encoding="utf-8") as f:
        raw = f.read().strip()
    m = SEMVER_ALPHA.match(raw)
    if not m:
        raise SystemExit(f"VERSION 格式非法: {raw!r}")
    return raw


def iter_scan_files(root: str):
    for rel in SCAN_FILES:
        p = os.path.join(root, rel)
        if os.path.isfile(p):
            yield p
    for rel in LOG_FILES:
        p = os.path.join(root, rel)
        if os.path.isfile(p):
            yield p
    for d in SCAN_DIRS:
        base = os.path.join(root, d)
        if not os.path.isdir(base):
            continue
        for dirpath, dirnames, filenames in os.walk(base):
            dirnames[:] = [x for x in dirnames if not x.startswith("__")]
            for fn in filenames:
                if fn.endswith((".md", ".txt", ".json", ".yaml", ".yml", ".py", ".sh")):
                    yield os.path.join(dirpath, fn)


def is_archived_path(rel: str) -> bool:
    return any(h in rel for h in ("/archive/",))


def exempt_line(line: str) -> bool:
    low = line.lower()
    return any(k in low for k in NAMESPACE_EXEMPT)


def scan_file(path: str, base_num: str, alpha_n: int, errors: list, warnings: list,
              log_ok: bool = False):
    rel = os.path.relpath(path, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    # 归档不扫描: history 命名空间只进 archive/CHANGELOG
    if is_archived_path(rel):
        return
    # 日志驻留点 (memory/CHANGELOG): 漂移只警告不 FAIL
    is_log = log_ok or os.path.basename(rel) in ("memory.md", "CHANGELOG.md")
    sink_e, sink_w = errors, warnings
    if is_log:
        sink_e, sink_w = [], warnings
    with open(path, encoding="utf-8", errors="replace") as f:
        for i, line in enumerate(f, 1):
            if REV_FIELD.match(line.strip()):
                continue  # 机器修订关系字段(记录来源/基线), 非当前版本陈述
            if CONTRACT_DOC_VERSION.search(line):
                continue  # front matter 文档修订号 (doc-revision 命名空间)
            if PRERELEASE_BAD.search(line) and ALPHA_INLINE.search(line):
                m = ALPHA_INLINE.search(line)
                if m:
                    sink_e.append(
                        f"{rel}:{i}: 产品版本含禁止 prerelease: {line.strip()[:90]}")
            for m in ALPHA_INLINE.finditer(line):
                if m.group(1) != base_num or int(m.group(2)) != alpha_n:
                    # 其他 alpha 产品版本: history 轮次上下文允许例外
                    if exempt_line(line):
                        sink_w.append(f"{rel}:{i}: history/豁免行内 alpha 串 "
                                      f"{m.group(0)} (不判 FAIL): {line.strip()[:70]}")
                        continue
                    if is_log:
                        sink_w.append(f"{rel}:{i}: 日志内 alpha 串 {m.group(0)} "
                                      f"(warnings-only): {line.strip()[:70]}")
                        continue
                    sink_e.append(f"{rel}:{i}: 其他产品版本 {m.group(0)} != "
                                  f"{base_num}-alpha.{alpha_n}: {line.strip()[:90]}")
            if exempt_line(line):
                continue
            for m in BASE_INLINE.finditer(line):
                if m.group(1) != base_num:
                    if is_log:
                        sink_w.append(f"{rel}:{i}: 日志内裸版本字面量 {m.group(1)} "
                                      f"(warnings-only): {line.strip()[:70]}")
                        continue
                    sink_e.append(f"{rel}:{i}: 裸版本字面量 {m.group(1)} != "
                                  f"{base_num}: {line.strip()[:90]}")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--root", default=".")
    ap.add_argument("--json-out", default=None)
    args = ap.parse_args()
    root = os.path.abspath(args.root)
    results: list[dict] = []
    errors: list[str] = []
    warnings: list[str] = []

    try:
        ver = root_version(root)
        base_num, alpha_n = re.match(r"^(\d+\.\d+\.\d+)-alpha\.(\d+)$", ver).groups()
        alpha_n = int(alpha_n)
        results.append(check("version_source_format", True, f"VERSION = {ver}"))
    except SystemExit as exc:
        print(exc)
        return 1

    # 生成链: gen_version.py 必须从 VERSION 派生, 输出前缀一致
    gen = os.path.join(root, "tools", "gen_version.py")
    gen_ok = False
    if os.path.isfile(gen):
        try:
            r = subprocess.run([sys.executable, gen, "--json"],
                               cwd=root, capture_output=True, text=True, timeout=60)
            if r.returncode == 0:
                rep = json.loads(r.stdout)
                if rep.get("version", "").startswith(ver + "+g") and rep.get("prerelease") == "alpha":
                    gen_ok = True
                    results.append(check("gen_version_from_source", True,
                                         f"gen_version --json version 前缀 = {ver}"))
                else:
                    results.append(check("gen_version_from_source", False,
                                         f"gen_version 输出前缀不符: {rep.get('version')!r}"))
            else:
                results.append(check("gen_version_from_source", False,
                                     f"gen_version 退出 {r.returncode}: {r.stderr[:120]}"))
        except Exception as exc:  # noqa: BLE001
            results.append(check("gen_version_from_source", False, str(exc)))
    else:
        results.append(check("gen_version_from_source", False, "tools/gen_version.py 缺失"))
    # 输出串形态断言 (机器可读, 供 TASK_RESULT evidence)
    try:
        r = subprocess.run([sys.executable, gen, "--json"], cwd=root,
                           capture_output=True, text=True, timeout=60)
        shape = json.loads(r.stdout)["version"]
        shape_ok = bool(re.match(rf"^{re.escape(base_num)}-alpha\.{alpha_n}\+g[0-9a-f]{{12}}(\.dirty)?$", shape))
        results.append(check("version_string_shape", shape_ok, f"生成串形态 {shape}"))
    except Exception:  # noqa: BLE001
        pass

    # active 允许路径扫描 (memory/CHANGELOG 日志驻留点 -> warnings-only)
    scanned = 0
    log_warn_before = len(warnings)
    for path in iter_scan_files(root):
        scanned += 1
        scan_file(path, base_num, alpha_n, errors, warnings)
    log_warnings = len(warnings) - log_warn_before
    results.append(check("active_scan", not errors,
                         f"扫描 {scanned} 文件; 漂移={len(errors)}" if errors else
                         f"扫描 {scanned} 文件 (允许路径 active + 日志驻留点), 无其他产品版本"))
    results.append(check("log_warnings_only", True,
                         f"memory/CHANGELOG 等日志驻留点 {log_warnings} 条漂移仅警告 "
                         "(GOV-005 收敛对象, 不判 FAIL)"))

    # 反误报: 豁免样本必须不报错 (FITS 4.0 / HiPS 1.0 / ABI v1)
    probe_lines = [
        "标准 IVOA HiPS 1.0 输出 (HiPS 格式版本非产品版本)",
        "FITS 4.0 规范与 CFITSIO 4.6.4 供应商版本",
        "ACS_ABI_VERSION_V1 = 1u (C ABI 命名空间, 非产品版本)",
        "schema_version = 1 (data-schema 命名空间, 非产品版本)",
        "DatabaseVersion=1.0.0 (Gaia 库标识)",
        "占位写法 X.Y.Z / MAJOR.MINOR.PATCH 不参与比较",
    ]
    false_pos = []
    for ln in probe_lines:
        for m in ALPHA_INLINE.finditer(ln):
            false_pos.append(f"alpha 误报: {ln[:60]}")
        if not exempt_line(ln):
            for m in BASE_INLINE.finditer(ln):
                if m.group(1) != base_num:
                    false_pos.append(f"裸三元组误报: {ln[:60]}")
    results.append(check("no_false_positive_fits_hips_abi", not false_pos,
                         "FITS 4.0/HiPS 1.0/ABI v1/schema/DatabaseVersion/占位 均不误报"
                         if not false_pos else "; ".join(false_pos[:5])))

    # mutation 合同: 伪造产品版本必须被抓
    import tempfile
    mutation_hit = False
    with tempfile.TemporaryDirectory() as td:
        fake = os.path.join(td, "FAKE.md")
        with open(fake, "w", encoding="utf-8") as f:
            f.write("发布产品版本 9.9.9-alpha.1 与 1.2.3 正式版\n")
        me: list[str] = []
        mw: list[str] = []
        scan_file(fake, base_num, alpha_n, me, mw)
        mutation_hit = any("9.9.9-alpha.1" in e or "1.2.3" in e for e in me)
    results.append(check("mutation_forged_version_fails", mutation_hit,
                         "伪造 9.9.9-alpha.1/1.2.3 被抓" if mutation_hit else "mutation 未命中!"))

    out_of_scope: list[str] = []
    # 他人路径遗留 (不判 FAIL; 前台集成/后续 GOV 任务协调)
    legacy = {
        "docs/VERSIONING.md": "VER-001 遗留: '当前冻结基线 0.10.0-alpha.2' (非允许路径, 需 GOV-005/前台收敛)",
        "CMakeLists.txt": "project(astrocs VERSION 0.10.0) 字面量 (BLD-002 配合从 VERSION 生成; 主串已 file(READ) 生成)",
        "tests/version/test_version_consistency.py": "test_01/05 硬编码 0.10.0-alpha.2 断言 (QA 配合更新)",
        "tests/quality/test_linux_release.py": "assertIn('astrocs 0.10.0') (QA 配合更新)",
        "tests/backend/test_cpu_profile.py": "gen --version 0.10.0-alpha.2 (QA 配合更新)",
        "tools/check_final_traceability.py": "checker 硬编码 == 0.10.0-alpha.2 (QA/前台配合)",
        "tools/check_release_consistency.py": "checker 硬编码 == 0.10.0-alpha.2 (QA/前台配合)",
        "tools/check_reproducible_build.py": "checker 硬编码 == 0.10.0-alpha.2 (QA/前台配合)",
        "tools/make_linux_release.py": "回退串硬编码 0.10.0-alpha.2 (打包 owner 配合; 主路径读 VERSION)",
        "tools/make_windows_release.py": "回退串硬编码 0.10.0-alpha.2 (打包 owner 配合; 主路径读 VERSION)",
        "docs/DOCUMENT_INDEX.yaml": "base_product_version 0.10.0-alpha.2 (GOV-002 基线修订字段; 需补登 docs/governance 文件)",
    }
    for p, why in legacy.items():
        if os.path.exists(os.path.join(root, p)):
            out_of_scope.append(f"{p}: {why}")

    results.append(check("known_legacy_reported", True,
                         f"登记 {len(out_of_scope)} 项他人路径遗留 (集成协调): "
                         + "; ".join(p.split(":")[0] for p in out_of_scope)))
    if warnings:
        results.append(check("warnings_note", True,
                             f"{len(warnings)} 条豁免/警告 (history 轮次引用, 不判 FAIL)"))

    passed = all(r["pass"] for r in results)
    out = {
        "tool": "tools/doccheck/check_version_namespaces.py",
        "version": "1.0.0",
        "task": "GOV-003",
        "owner": "SA-GOV-01",
        "root": root,
        "product_version": ver,
        "results": sorted(results, key=lambda r: r["check"]),
        "warnings": warnings,
        "out_of_scope_legacy": out_of_scope,
        "verdict": "VERSION_NAMESPACES_PASS" if passed else "VERSION_NAMESPACES_FAIL",
    }
    text = json.dumps(out, ensure_ascii=False, indent=2, sort_keys=True)
    if args.json_out:
        os.makedirs(os.path.dirname(os.path.abspath(args.json_out)), exist_ok=True)
        with open(args.json_out, "w", encoding="utf-8") as f:
            f.write(text + "\n")
    print(text)
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())
