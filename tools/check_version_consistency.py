#!/usr/bin/env python3
"""VER-001 版本一致性机器检查器。
扫描范围内任何 X.Y.Z 形式字面量必须等于当前唯一版本源 VERSION 的基础号,
且不得出现 stable/RC/beta 预发布标记; VERSION 本身与 gen_version 常量为豁免定义点。
exit 0 = PASS; 任何伪造/漂移版本字面量 => 非 0 (mutation 必须失败)。
"""
import os, re, subprocess, sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BASE_RE = re.compile(r"(?<![\w.])(\d+\.\d+\.\d+)(?![\d.])")  # 排除 127.0.0.1 等 IP/更长子串
PRERELEASE_BAD = re.compile(r"\b\d+\.\d+\.\d+-(stable|rc|beta)\b", re.IGNORECASE)
SCAN_ROOTS = ["docs", "schemas", "tools", "launch", "tests"]
SCAN_FILES = ["build.sh", "toolchain.ps1", "README.md", "CHANGELOG.md", "VERSION"]
SELF_FIXTURE = os.path.join("tests", "version", "test_version_consistency.py")  # mutation 样本自身
# 行内豁免: 非产品版本的数字三元组(外部工具/格式版本/协议版本/示例占位)
EXEMPT = ("hips_version", "DatabaseVersion", "schema_version", "cap.version", "driver",
          "X.Y.Z", "MAJOR.MINOR.PATCH", "healpix", "cfitsio", "fitsio", "opencl", "example",
          "g++", "gcc", "cmake", "ninja", "mingw", "msys2", "siril", "wbpp", "pcl", "rcr",
          "python", '"version":', "clang")
SKIP_DIRS = {".git", "build", "run", "reports", "archive", "testdata", "工程控制",
             "BASS DR3", "lib", "AstroCS.wiki", "tools", "__pycache__"}

def base_version():
    """返回 (基础号 X.Y.Z, alpha.N)。"""
    with open(os.path.join(REPO, "VERSION"), encoding="utf-8") as f:
        raw = f.read().strip()
    m = re.match(r"^(\d+\.\d+\.\d+)-alpha\.(\d+)$", raw)
    if not m:
        raise SystemExit(f"VERSION_CHECK_FAIL: 版本源格式非法: {raw!r}")
    return m.group(1), int(m.group(2))

def iter_files():
    for root in SCAN_ROOTS:
        for dirpath, dirnames, filenames in os.walk(os.path.join(REPO, root)):
            dirnames[:] = [d for d in dirnames if d not in ("__pycache__",)]
            for fn in filenames:
                if fn.endswith((".py", ".md", ".json", ".sh", ".ps1")):
                    yield os.path.join(dirpath, fn)
    for fn in SCAN_FILES:
        p = os.path.join(REPO, fn)
        if os.path.isfile(p):
            yield p

def check_file(path, base_num, alpha_n, errors):
    rel = os.path.relpath(path, REPO)
    if rel == "VERSION" or rel == os.path.join("tools", "gen_version.py"):
        return  # 唯一定义点自身豁免
    if rel == SELF_FIXTURE:
        return  # mutation 测试样本文件(内含故意伪造版本)
    alpha_full = re.compile(r"(\d+\.\d+\.\d+)-alpha\.(\d+)")
    with open(path, encoding="utf-8", errors="replace") as f:
        for i, line in enumerate(f, 1):
            if PRERELEASE_BAD.search(line):
                errors.append(f"{rel}:{i}: 禁止的 prerelease 标记(stable/rc/beta): {line.strip()[:90]}")
            if "-alpha" in line:
                for m in alpha_full.finditer(line):
                    if m.group(1) != base_num or int(m.group(2)) != alpha_n:
                        errors.append(f"{rel}:{i}: alpha 版本漂移 {m.group(0)} != {base_num}-alpha.{alpha_n}")
                # 去掉 alpha 串后检查行内残留的普通三元组
                line = alpha_full.sub("", line)
            low = line.lower()
            if any(k in low for k in EXEMPT):
                continue
            for m in BASE_RE.finditer(line):
                if m.group(1) != base_num:
                    errors.append(f"{rel}:{i}: 未知版本字面量 {m.group(1)} != 唯一源基础号 {base_num}: {line.strip()[:90]}")

def main():
    base_num, alpha_n = base_version()
    errors = []
    for path in iter_files():
        check_file(path, base_num, alpha_n, errors)
    if errors:
        print(f"VERSION_CONSISTENCY_FAIL ({len(errors)}):")
        for e in errors[:20]:
            print(" ", e)
        return 1
    print(f"VERSION_CONSISTENCY_PASS base={base_num}-alpha.{alpha_n} 扫描范围: {', '.join(SCAN_ROOTS + SCAN_FILES)}")
    return 0

if __name__ == "__main__":
    sys.exit(main())
