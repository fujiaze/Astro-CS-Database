#!/usr/bin/env python3
"""VER-001 版本一致性机器检查器。
扫描范围内任何 X.Y.Z 形式字面量必须等于当前唯一版本源 VERSION 的基础号,
且不得出现 stable/RC/beta 预发布标记; VERSION 本身与 gen_version 常量为豁免定义点。
exit 0 = PASS; 任何伪造/漂移版本字面量 => 非 0 (mutation 必须失败)。

标准条款号豁免 (P2/CI-VER-CHK-001, 裁决 R-08): FITS WCS Paper I/II 与 IVOA HiPS
的条款引用 (§2.1.1 / §4.2.1 / §4.4.1 / §6.3.1 等) 是科学可追溯锚, 不是产品版本;
识别口径按"标准条款号形态"收窄 (见 mask_standard_clause_numbers)。
口径只准更精确、不准更宽松: 只挖条款号本身, 同行真实版本字面量仍须 FAIL。
"""
import os, re, subprocess, sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BASE_RE = re.compile(r"(?<![\w.])(\d+\.\d+\.\d+)(?![\d.])")  # 排除 127.0.0.1 等 IP/更长子串
PRERELEASE_BAD = re.compile(r"\b\d+\.\d+\.\d+-(stable|rc|beta)\b", re.IGNORECASE)
SCAN_ROOTS = ["docs", "contracts/schemas", "tests"]
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
          "git", "zstd", "xz",
          # 外部标准/工具版本引用 (非产品版本): Hipsgen LINT 工具版本、
          # VOTable 标准版本、C ABI 接口版本串 `version "1.0.0"` (测试探针断言该串)、
          # 外部 oracle harness 版本引用 (R13 4b6eb26d 实证: PHASE2_REJECTION.md:282
          # "…未修改 Siril\n1.4.3 官方 harness…" 跨行断开 siril 豁免词 → 漏报;
          # 'harness' 一词在仓库仅出现于外部 oracle 工具上下文, 与 siril/rcr 同口径)
          "hipsgen", "votable", 'version "', "harness")
# 合同文档 front matter 的 "状态: ACTIVE  版本: 1.0.0" 是文档修订号，不是产品版本
CONTRACT_DOC_VERSION = re.compile(r"状态:\s*\w+\s+版本:\s*\d+\.\d+\.\d+")
SKIP_DIRS = {".git", "build", "run", "reports", "archive", "testdata", "工程控制",
             "BASS DR3", "lib", "AstroCS.wiki", "__pycache__"}

# ── 标准条款号口径 (P2/CI-VER-CHK-001, 裁决 R-08) ──────────────────────────
# 事由: docs/standards/STANDARDS_REGISTRY.md 的 FITS WCS Paper I/II 与 IVOA HiPS
#       条款引用 (§2.1.1 / §4.2.1 / §4.4.1 / §6.3.1) 被旧口径误判为"未知版本
#       字面量", 19 条 findings 全部落在该文件。
# 依据: 宪章 §19 基础科学与格式参考 + §7.3「以标准为基础, 而不是根据现有代码反推」;
#       裁决 R-08 —— 修检查器口径, 严禁为过检查改写标准条款号; 检查器只准更精确、
#       不准更宽松 (必须带"真实版本漂移仍 FAIL"的正向守卫用例)。
# 形态: ① § 前缀条款号 (§2.1.1), 含其枚举续项 (§4.1/4.2.1/4.4.1);
#       ② 标准名后紧跟的裸条款号 (Paper I 2.1.1 / HiPS 4.2.1 / SIP 2.1.1)。
CLAUSE_ANCHOR_RE = re.compile(r"§\s*\d+(?:\.\d+)*")            # §2.1.1 / §3 / §12.1
CLAUSE_BARE_RE = re.compile(r"(?<![\w.])(\d+(?:\.\d+){2,})(?![\d.])")  # 裸 X.Y.Z
# 条款枚举中相邻两项之间只允许: 空白、括号注记、分隔符 (/、,; 和 与 及 ~)
CLAUSE_GAP_RE = re.compile(
    r"^(?:\s*(?:[（(][^（）()]*[）)])?\s*(?:[/、,，;；]|和|与|及|~|～)\s*)+$")
STANDARD_NAME_CLAUSE_RE = re.compile(
    r"(?:Paper\s+(?:I|II|III|IV|V)|HiPS|SIP|IVOA\s+HiPS|HEALPix|Drizzle|"
    r"FITS\s+(?:WCS\s+)?Standard)\s+(\d+(?:\.\d+){2,})(?![\d.])")

def clause_number_spans(line):
    """行内标准条款号的字符下标区段 [(start, end), ...] (含 § 锚点及其枚举续项)。"""
    spans = []
    pos = 0
    while True:
        m = CLAUSE_ANCHOR_RE.search(line, pos)
        if not m:
            break
        end = m.end()
        while True:  # 枚举续项: §4.1/4.2.1/4.4.1 中后两项无 § 前缀
            b = CLAUSE_BARE_RE.search(line, end)
            if not b or not CLAUSE_GAP_RE.match(line[end:b.start()]):
                break
            end = b.end()
        spans.append((m.start(), end))
        pos = end
    spans.extend(m.span(1) for m in STANDARD_NAME_CLAUSE_RE.finditer(line))
    return spans


def mask_standard_clause_numbers(line):
    """把标准条款号挖成等长空白后返回, 供版本字面量扫描使用。

    只挖条款号本身, 不动同一行的产品版本字面量 ——
    `| §4.2.1（properties） | 版本 1.2.3 |` 中的 `1.2.3` 仍会被抓 (R-08)。
    """
    spans = clause_number_spans(line)
    if not spans:
        return line
    chars = list(line)
    for s, e in spans:
        chars[s:e] = [" "] * (e - s)
    return "".join(chars)

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
            # R-08: 未知版本字面量扫描前先挖掉标准条款号。alpha/prerelease 判定
            # 仍跑在原始行上 —— 口径只收窄未知字面量误报面, 不放宽漂移判定。
            for m in BASE_RE.finditer(mask_standard_clause_numbers(line)):
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
