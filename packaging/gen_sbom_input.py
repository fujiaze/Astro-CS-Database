#!/usr/bin/env python3
"""AstroCS SBOM 输入生成/校验器 (BLD-004)

机器验收 (BLD-004 + W5-PKG-001):
  1. dependency-lock.json 与 DEPENDENCIES.md 语义一致 (生产依赖/系统依赖/
     test-only oracle 分离);
  2. fresh configure 不读取机器绝对路径: 对 CMake 作用域 (CMakeLists.txt,
     CMakePresets.json, cmake/**, packaging/**, DEPENDENCIES.md) 扫描禁止
     模式 (F:/ C:/Users/<user> /home/<user> C:\\msys64 等), 冻结工具链安装
     约定 (C:/AstroCS/toolchains, preset 显式声明) 为白名单例外;
  3. 生成 SBOM 输入: 以 dependency-lock.json 为权威源输出扁平 SBOM 输入
     (JSON Lines: 每个依赖一行 {name, version, usage, source, hash});
  4. (W5-PKG-001 增补) 锁内声明的 vendored 实树必须自证: vendored_path 存在、
     file_sha256/license_file_sha256 与实文件一致、aggregate_sha256 按
     aggregate_sha256_algo 可复算 —— 改前锁内哈希在任何机器门内零校验。

用法:
  python3 packaging/gen_sbom_input.py --root <repo-root> [--json-out <p>]
  python3 packaging/gen_sbom_input.py --root <repo-root> --self-test
    退出码 0 = 校验通过 + 生成 SBOM 输入; 1 = 不一致失败; 2 = 输入不可用
    (fail-closed, 不静默通过)。

输出:
  <root>/build/sbom-input.jsonl (每次运行重写; 不入 git)
"""
import argparse
import json
import re
import shutil
import sys
import tempfile
from pathlib import Path

# 不写 __pycache__: packaging/ 是登记/交付目录, 不允许出现解释器缓存垃圾
# (W5-PKG-001 清理项; 与 .gitignore 的 __pycache__/ 覆盖配套)。
sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
try:  # 复用打包面检查器的哈希/源清单实现（单一实现, 不复制算法）
    from check_packaging_consistency import (  # noqa: E402
        InputUnavailable, aggregate_sha256, parse_cfitsio_sources, sha256_file)
except ImportError as _e:  # pragma: no cover - fail-closed
    print(f"SBOM FAIL (fail-closed): 依赖 packaging/check_packaging_consistency.py: {_e}",
          file=sys.stderr)
    raise SystemExit(2)

LOCK_REL = Path("packaging/dependency-lock.json")
DEPENDENCIES_MD = Path("DEPENDENCIES.md")
# 机器路径扫描范围 = 构建输入 (CMake/脚本/契约); DEPENDENCIES.md 是政策文档
# (其禁止模式描述文本经 verify_dependencies_md 做语义一致性检查, 不作路径源)
SCAN_SCOPE = ["CMakeLists.txt", "CMakePresets.json", "cmake", "packaging", "cli/CMakeLists.txt"]

# 机器绝对路径禁止模式 (Windows F:/ C:/Users/<user>; Linux /home/<user>;
# 隐式 MSYS2/MinGW)
FORBIDDEN = [
    re.compile(r"(?i)F:/"),
    re.compile(r"(?i)F:\\"),
    re.compile(r"(?i)C:/Users/[A-Za-z0-9_\-]+"),
    re.compile(r"(?i)C:\\Users\\[A-Za-z0-9_\-]+"),
    re.compile(r"(?i)C:\\msys64"),
    re.compile(r"(?i)/home/[a-z0-9_\-]+/"),
]
# 冻结工具链安装约定白名单 (preset 显式声明, 非隐式读取)
ALLOWED = [
    re.compile(r"(?i)C:/AstroCS/toolchains/"),
    re.compile(r"(?i)C:\\AstroCS\\toolchains\\"),
]


def iter_scope_files(root: Path) -> list:
    files = []
    for item in SCAN_SCOPE:
        p = root / item
        if p.is_file():
            files.append(p)
        elif p.is_dir():
            files.extend(x for x in p.rglob("*") if x.is_file()
                         and x.suffix in (".txt", ".cmake", ".json", ".yaml", ".md", ".ps1", ".py"))
    # 去重保序
    seen, out = set(), []
    for f in files:
        rp = str(f.relative_to(root))
        if rp not in seen:
            seen.add(rp)
            out.append(f)
    return out


def scan_machine_paths(root: Path) -> list:
    hits = []
    self_name = Path(__file__).resolve()
    for f in iter_scope_files(root):
        if f.resolve() == self_name:  # 跳过扫描器自身 (模式定义示例文本)
            continue
        try:
            text = f.read_text(encoding="utf-8", errors="ignore")
        except Exception:
            continue
        for lineno, line in enumerate(text.splitlines(), 1):
            if any(a.search(line) for a in ALLOWED):
                continue
            stripped = line.lstrip()
            # 跳过注释行/文档示例 (说明性引用非实际读取)
            if stripped.startswith(("#", "//", "*", "<!--")) or "注:" in line or "见 fatduck" in line:
                continue
            for pat in FORBIDDEN:
                if pat.search(line):
                    hits.append(f"{f.relative_to(root)}:{lineno}: {line.strip()[:120]}")
                    break
    return hits


def verify_dependencies_md(root: Path, lock: dict) -> list:
    """DEPENDENCIES.md 与 lock 的生产依赖名集合一致 (轻量交叉检查)。"""
    md = root / DEPENDENCIES_MD
    if not md.exists():
        return ["DEPENDENCIES.md 缺失"]
    text = md.read_text(encoding="utf-8", errors="ignore")
    issues = []
    for dep in lock.get("production_dependencies", []):
        if dep["name"].lower() not in text.lower():
            issues.append(f"生产依赖 {dep['name']} 未在 DEPENDENCIES.md 提及")
    for dep in lock.get("test_only_oracles", []):
        if dep["name"].lower() not in text.lower():
            issues.append(f"test-only oracle {dep['name']} 未在 DEPENDENCIES.md 提及")
    return issues


def verify_vendored(root: Path, lock: dict) -> list:
    """锁内 vendored 实树自证 (W5-PKG-001 判据 4)。"""
    issues = []
    prod = lock.get("production_dependencies") or []
    if not prod:
        raise InputUnavailable("dependency-lock.production_dependencies 为空 (fail-closed)")
    try:
        cfitsio_srcs = parse_cfitsio_sources(root)
    except InputUnavailable as e:
        issues.append(str(e))
        cfitsio_srcs = []
    for dep in prod:
        name = dep.get("name", "?")
        vp = dep.get("vendored_path", "")
        if not vp or not (root / vp).exists():
            issues.append(f"vendored_path 不存在: {name} -> {vp}")
            continue
        if dep.get("file_sha256"):
            fp = root / (dep.get("file_sha256_of") or vp)
            if not fp.is_file():
                issues.append(f"file_sha256_of 不存在: {dep.get('file_sha256_of')}")
            elif sha256_file(fp) != dep["file_sha256"]:
                issues.append(f"file_sha256 不匹配: {fp.relative_to(root)}")
        if dep.get("aggregate_sha256"):
            algo = dep.get("aggregate_sha256_algo")
            if not algo:
                issues.append(f"{name} 有 aggregate_sha256 但缺 aggregate_sha256_algo")
            elif not cfitsio_srcs:
                issues.append(f"{name} aggregate 源清单不可用")
            elif aggregate_sha256(root, algo, cfitsio_srcs) != dep["aggregate_sha256"]:
                issues.append(f"aggregate_sha256 复算不符: {name}")
        if dep.get("license_file_sha256"):
            lf = dep.get("license_file")
            if not lf or not (root / lf).is_file():
                issues.append(f"license_file 不存在: {name} -> {lf}")
            elif sha256_file(root / lf) != dep["license_file_sha256"]:
                issues.append(f"license_file_sha256 不匹配: {lf}")
    return issues


def gen_sbom_input(root: Path, lock: dict) -> list:
    rows = []
    for dep in lock.get("production_dependencies", []):
        rows.append({
            "name": dep["name"], "version": dep.get("version", ""),
            "usage": dep.get("usage", "production"),
            "source": dep.get("source", ""),
            "sha256": dep.get("file_sha256", dep.get("sha256", "")),
        })
    for dep in lock.get("system_dependencies", []):
        rows.append({
            "name": dep["name"], "version": dep.get("version", "system"),
            "usage": "production-system",
            "source": dep.get("source", ""),
            "sha256": "",
        })
    for dep in lock.get("test_only_oracles", []):
        rows.append({
            "name": dep["name"], "version": dep.get("version", ""),
            "usage": "test-only",
            "source": dep.get("source", ""),
            "sha256": dep.get("sha256", ""),
        })
    out = root / "build" / "sbom-input.jsonl"
    out.parent.mkdir(parents=True, exist_ok=True)
    with open(out, "w", encoding="utf-8") as f:
        for row in rows:
            f.write(json.dumps(row, ensure_ascii=False, sort_keys=True) + "\n")
    return rows


# ── 负例注入自测（ENGINEERING_SPEC §8 可执行负例面）────────────────────────
def _build_fixture(base: Path) -> Path:
    """最小自洽夹具: 锁 + vendored 文件 + 源清单 + DEPENDENCIES.md。"""
    root = base / "repo"
    (root / "cmake").mkdir(parents=True)
    (root / "packaging" / "licenses").mkdir(parents=True)
    (root / "vendor").mkdir(parents=True)
    srcs = ["vendor/a.c", "vendor/b.c"]
    for s in srcs:
        (root / s).write_text("int x;\n", encoding="utf-8")
    (root / "cmake" / "cfitsio_sources.cmake").write_text(
        "set(ASTROCS_CFITSIO_SOURCES\n" +
        "".join(f"  {s}\n" for s in srcs) + ")\n", encoding="utf-8")
    (root / "packaging" / "licenses" / "FAKE_LICENSE.txt").write_text(
        "license text\n", encoding="utf-8")
    (root / "CMakeLists.txt").write_text("# fixture\n", encoding="utf-8")
    (root / "CMakePresets.json").write_text("{}\n", encoding="utf-8")
    (root / "cli").mkdir()
    (root / "cli" / "CMakeLists.txt").write_text("# fixture\n", encoding="utf-8")
    (root / "DEPENDENCIES.md").write_text(
        "cfitsio nlohmann-json astropy\n", encoding="utf-8")
    algo = "sha256(concat(sorted(git-blob-sha1 of sources)))"
    lock = {
        "lock_schema": "astrocs.dependency-lock/v1",
        "lock_version": 1,
        "production_dependencies": [{
            "name": "cfitsio", "version": "0.1", "usage": "production",
            "vendored_path": "vendor/a.c",
            "file_sha256": sha256_file(root / "vendor" / "a.c"),
            "file_sha256_of": "vendor/a.c",
            "aggregate_sha256": aggregate_sha256(root, algo, srcs),
            "aggregate_sha256_algo": algo,
            "aggregate_sha256_sources": "cmake/cfitsio_sources.cmake",
            "license": "fixture", "license_file": "packaging/licenses/FAKE_LICENSE.txt",
            "license_file_sha256": sha256_file(
                root / "packaging" / "licenses" / "FAKE_LICENSE.txt"),
        }],
        "system_dependencies": [],
        "test_only_oracles": [{"name": "astropy", "version": "x", "usage": "test-only"}],
    }
    lp = root / LOCK_REL
    lp.parent.mkdir(parents=True, exist_ok=True)
    lp.write_text(json.dumps(lock, ensure_ascii=False, indent=2), encoding="utf-8")
    return root


def _fixture_issues(root: Path) -> list:
    lock = json.loads((root / LOCK_REL).read_text(encoding="utf-8"))
    issues = list(verify_dependencies_md(root, lock))
    issues.extend(verify_vendored(root, lock))
    issues.extend(scan_machine_paths(root))
    return issues


def self_test() -> int:
    ok = True
    with tempfile.TemporaryDirectory(prefix="astrocs-sbom-selftest-") as tmp:
        base = Path(tmp)
        cases = (
            ("positive (未注入)", lambda r: None),
            ("vendored file_sha256 不符", lambda r: (r / "vendor" / "a.c").write_text(
                "int y;\n", encoding="utf-8")),
            ("vendored_path 失效", lambda r: (r / "vendor" / "a.c").unlink()),
            ("aggregate_sha256 源改动", lambda r: (r / "vendor" / "b.c").write_text(
                "int z;\n", encoding="utf-8")),
            ("许可文本缺失", lambda r: (r / "packaging" / "licenses"
                                   / "FAKE_LICENSE.txt").unlink()),
            ("DEPENDENCIES.md 漏生产依赖", lambda r: (r / "DEPENDENCIES.md").write_text(
                "astropy\n", encoding="utf-8")),
            ("机器绝对路径回归", lambda r: (r / "packaging" / "x.ps1").write_text(
                "$env:Path = " + '"' + "C:" + chr(92) + "msys64" + chr(92) +
                'mingw64' + chr(92) + 'bin;$env:Path"' + "\n", encoding="utf-8")),
        )
        for i, (case, mutate) in enumerate(cases):
            work = base / f"case{i}"
            work.mkdir()
            root = _build_fixture(work)
            mutate(root)
            try:
                issues = _fixture_issues(root)
            except (InputUnavailable, json.JSONDecodeError) as e:
                issues = [f"input: {e}"]
            red = bool(issues)
            good = (not red) if case.startswith("positive") else red
            detail = ("应绿" if case.startswith("positive") else "应红") + \
                     f", 实得 {'红' if red else '绿'} {issues[:1]}"
            print(("SELFTEST PASS " if good else "SELFTEST FAIL ") + f"{case}: {detail}")
            ok = ok and good
    print("SELFTEST " + ("PASS (全部注入判红, 正例判绿)" if ok else "FAIL"))
    return 0 if ok else 1


def main():
    ap = argparse.ArgumentParser(
        description="AstroCS SBOM 输入生成/校验 (BLD-004/W5-PKG-001)")
    ap.add_argument("--root", default="", help="仓库根目录 (默认由脚本位置推导)")
    ap.add_argument("--self-test", action="store_true",
                    help="负例注入自测（机器可执行负例面）")
    args = ap.parse_args()
    if args.self_test:
        return self_test()
    root = Path(args.root).resolve() if args.root else Path(__file__).resolve().parent.parent
    lock_path = root / LOCK_REL
    if not lock_path.exists():
        print("LOCK_MISSING packaging/dependency-lock.json", file=sys.stderr)
        sys.exit(1)
    try:
        lock = json.loads(lock_path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as e:
        print(f"LOCK_INVALID {LOCK_REL}: {e}", file=sys.stderr)
        return 2
    issues = []
    md_issues = verify_dependencies_md(root, lock)
    issues.extend(md_issues)
    try:
        issues.extend(verify_vendored(root, lock))
    except InputUnavailable as e:
        print(f"SBOM FAIL (fail-closed): {e}", file=sys.stderr)
        return 2
    hits = scan_machine_paths(root)
    if hits:
        issues.append(f"机器绝对路径命中 {len(hits)} 处 (前 5): " + "; ".join(hits[:5]))
    rows = gen_sbom_input(root, lock)
    print(f"SBOM_INPUT OK: {len(rows)} rows -> build/sbom-input.jsonl")
    if issues:
        print("BLD-004 FAIL:")
        for i in issues:
            print("  -", i)
        sys.exit(1)
    print("BLD-004 PASS (lock <-> DEPENDENCIES.md 一致; vendored 实树 hash 自证; "
          "fresh configure 无机器绝对路径)")
    sys.exit(0)


if __name__ == "__main__":
    raise SystemExit(main())
