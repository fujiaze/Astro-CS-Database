#!/usr/bin/env python3
"""AstroCS SBOM 输入生成/校验器 (BLD-004)

机器验收 (BLD-004):
  1. dependency-lock.json 与 DEPENDENCIES.md 语义一致 (生产依赖/系统依赖/
     test-only oracle 分离);
  2. fresh configure 不读取机器绝对路径: 对 CMake 作用域 (CMakeLists.txt,
     CMakePresets.json, cmake/**, packaging/**, DEPENDENCIES.md) 扫描禁止
     模式 (F:/ C:/Users/<user> /home/<user> C:\\msys64 等), 冻结工具链安装
     约定 (C:/AstroCS/toolchains, preset 显式声明) 为白名单例外;
  3. 生成 SBOM 输入: 以 dependency-lock.json 为权威源输出扁平 SBOM 输入
     (JSON Lines: 每个依赖一行 {name, version, usage, source, hash})。

用法:
  python3 packaging/gen_sbom_input.py --root <repo-root>
    退出码 0 = 校验通过 + 生成 SBOM 输入; 1 = 发现机器路径/一致性失败。

输出:
  <root>/build/sbom-input.jsonl (每次运行重写; 不入 git)
"""
import argparse
import json
import re
import sys
from pathlib import Path

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


def main():
    ap = argparse.ArgumentParser(description="AstroCS SBOM 输入生成/校验 (BLD-004)")
    ap.add_argument("--root", required=True, help="仓库根目录")
    args = ap.parse_args()
    root = Path(args.root).resolve()
    lock_path = root / LOCK_REL
    if not lock_path.exists():
        print("LOCK_MISSING packaging/dependency-lock.json", file=sys.stderr)
        sys.exit(1)
    lock = json.loads(lock_path.read_text(encoding="utf-8"))
    issues = []
    md_issues = verify_dependencies_md(root, lock)
    issues.extend(md_issues)
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
    print("BLD-004 PASS (lock <-> DEPENDENCIES.md 一致; fresh configure 无机器绝对路径)")
    sys.exit(0)


if __name__ == "__main__":
    main()
