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
SCAN_ROOTS = ["docs", "schemas", "launch", "tests"]
# CHANGELOG.md 是 history 命名空间驻留点 (GOV-003 §2/§4, 与 ci/check_version.py
# [5] 口径一致): 条目记录"当时的版本号"属天然历史事实, 不进本检查。
SCAN_FILES = ["build.sh", "toolchain.ps1", "README.md", "VERSION",
              "tools/gen_version.py"]
SELF_FIXTURE = os.path.join("tests", "version", "test_version_consistency.py")  # mutation 样本自身
PROBE_FIXTURES = {  # 版本探针工具：内含 '0.1.0' 等被扫描 token，属扫描器自身而非产品
    os.path.join("tools", "quality", "known_failures_baseline.py"),
}
TEST_FIXTURES = {  # 单元测试合成数据文件: 内含 semver 解析/比较/取代逻辑的合成 token
    os.path.join("tests", "artifact", "test_provenance.py"),  # parse_version/version_gt 等合成版本
    os.path.join("tests", "abi", "test_secure_loader.py"),  # loader 探针 fixture 0.0.0-test 等非法版本样本
}
# 行内豁免: 非产品版本的数字三元组(外部工具/格式版本/协议版本/示例占位)
EXEMPT = ("hips_version", "DatabaseVersion", "schema_version", "cap.version", "driver",
          "X.Y.Z", "MAJOR.MINOR.PATCH", "healpix", "cfitsio", "fitsio", "opencl", "example",
          "g++", "gcc", "cmake", "ninja", "mingw", "msys2", "siril", "wbpp", "pcl", "rcr",
          "python", '"version":', '"版本":', "version: ", "clang", "ivoa",
          # 外部工具链版本表 (git/zstd/xz): GOV-003 §3 豁免表"外部组件版本"同口径
          "git", "zstd", "xz")
# 合同文档 front matter 的 "状态: ACTIVE  版本: 1.0.0" 是文档修订号，不是产品版本
CONTRACT_DOC_VERSION = re.compile(r"状态:\s*\w+\s+版本:\s*\d+\.\d+\.\d+")
SKIP_DIRS = {".git", "build", "run", "reports", "archive", "testdata", "工程控制",
             "BASS DR3", "lib", "AstroCS.wiki", "__pycache__"}

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
            # 归档命名空间 (docs/archive/ 等) 是历史记录, 天然记录旧版本号,
            # 绝不可改成当前版本 —— 目录名命中 SKIP_DIRS 即整树剪枝。
            dirnames[:] = [d for d in dirnames
                           if d not in SKIP_DIRS and d != "__pycache__"]
            for fn in filenames:
                if fn.endswith((".py", ".md", ".json", ".sh", ".ps1")):
                    yield os.path.join(dirpath, fn)
    for fn in SCAN_FILES:
        p = os.path.join(REPO, fn)
        if os.path.isfile(p):
            yield p

def _rel_to_repo(path):
    """path 相对仓库根的显示路径。

    Windows 下临时文件与仓库可能在不同盘符 (C:\\Users\\...\\Temp vs D:\\repo),
    os.path.relpath 会抛 ValueError: path is on mount 'C:', start on mount 'D:'
    (UT-VERSION run 5e457d425fc8 test_05 实证)。此时回退 os.path.abspath 原样:
    仓库外文件的绝对路径不命中任何豁免表条目, mutation 合同语义不变。
    """
    try:
        return os.path.relpath(path, REPO)
    except ValueError:
        return os.path.abspath(path)


def check_file(path, base_num, alpha_n, errors):
    rel = _rel_to_repo(path)
    if rel == "VERSION" or rel == os.path.join("tools", "gen_version.py"):
        return  # 唯一定义点自身豁免
    if rel == SELF_FIXTURE or rel in PROBE_FIXTURES or rel in TEST_FIXTURES:
        return  # mutation/probe/合成数据测试样本文件(内含故意伪造或合成版本 token)
    alpha_full = re.compile(r"(\d+\.\d+\.\d+)-alpha\.(\d+)")
    with open(path, encoding="utf-8", errors="replace") as f:
        for i, line in enumerate(f, 1):
            if CONTRACT_DOC_VERSION.search(line):
                continue  # L1 合同 front matter 的文档修订号(非产品版本)
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
