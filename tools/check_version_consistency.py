#!/usr/bin/env python3
"""VER-001 版本一致性机器检查器。
扫描范围内任何 X.Y.Z 形式字面量必须等于当前唯一版本源 VERSION 的基础号,
且不得出现 stable/RC/beta 预发布标记; VERSION 本身与 gen_version 常量为豁免定义点。
exit 0 = PASS; 任何伪造/漂移版本字面量 => 非 0 (mutation 必须失败)。

标准条款号豁免 (P2/CI-VER-CHK-001, 裁决 R-08): FITS WCS Paper I/II 与 IVOA HiPS
的条款引用 (§2.1.1 / §4.2.1 / §4.4.1 / §6.3.1 等) 是科学可追溯锚, 不是产品版本;
识别口径按"标准条款号形态"收窄 (见 mask_standard_clause_numbers)。
口径只准更精确、不准更宽松: 只挖条款号本身, 同行真实版本字面量仍须 FAIL。

合同生命周期边界字段豁免 (W4-A3): 合同/对象注册表 JSON 里的
`"retire_after": "X.Y.Z"` 是**前向生命周期边界**(该字段的语义就是"在此版本之后退出"),
不是"产品当前版本 = X.Y.Z"的声明 ⇒ 与唯一版本源比较是错口径 —— 现行实测
`docs/contracts/unified_object_registry.json`(20 处) 与
`contracts/data/unified_object_compatibility_map_v1.json`(17 处) 的
`retire_after: "0.12.0"` 使本扫描器在真仓恒 FAIL(49 条), 遮蔽了
`UT-VERSION` 的 test_04/test_13。口径仍只准更精确: 只挖 **JSON 字段形态**
`"<生命周期键>": "<X.Y.Z>"` 的**值本身**, 同文件内任何其它版本字面量
(含同一 JSON 对象里的 `"product_version"`/`"doc_version"` 等产品版本声明)
照旧必须等于唯一源基础号(见 tests/version/test_version_consistency.py 的
lifecycle boundary 正/负例)。
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
    # W4-A3 实测残余: 硬件探针默认 build 串 (0.0.0-alpha.0+g000000000000) 与
    # tests/config/fixtures/** 的 cpu_profile 负例/正例合成数据 (0.1.0-alpha.1) ——
    # 都是"旧世代合成 token", 不是活动文档里的产品版本声明。
    os.path.join("tests", "backend", "test_hardware_inspect.py"),
}
TEST_FIXTURE_DIRS = (  # 目录级合成数据面 (同上口径)
    os.path.join("tests", "config", "fixtures"),
)
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
          "hipsgen", "votable", 'version "', "harness",
          # W4-A3 实测残余 16 条(全在 docs/science|algorithms|references 的外部引用面,
          # 与产品版本无关; 原口径把外部软件版本/文献卷页/数据文件名当产品版本):
          #   astropy/WCSLIB 7.0.1(可执行标准)、photutils 1.6.0(Zenodo DOI)、
          #   gdr3sp-1.0.0-*.xpsd(数据文件名)、MNRAS 214, 575(卷, 页, 被逗号分隔成三元组)、
          #   DOI 10.5281/zenodo.XXXXXXX 与版本串同行。
          "astropy", "wcs", "zenodo", "doi", "mnras", "xpsd", "photutils",
          "gaiaxpy", "readthedocs", "文献", "表 ", "卷")
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

# ── 合同生命周期边界字段口径 (W4-A3) ────────────────────────────────────────
# 事由: 合同/对象注册表 JSON 的 "retire_after": "0.12.0" 是前向生命周期边界,
#       被 BASE_RE 当"未知版本字面量", 真仓恒 49 条 FAIL。
# 依据: DATA-001 统一对象合同的兼容映射字段语义(该字段定义"在此版本之后退出",
#       本质就是尚未到达的版本); 与 R-08 同款——修口径、不许改合同数据来迎合检查器。
# 形态收窄: 只挖 JSON 字段形态 ""<键>": "<X.Y.Z>"" 的值; 非 JSON、无引号、
#       同对象内其它字段形态一律不挖(负例见 test_version_consistency.LifecycleBoundary)。
LIFECYCLE_KEYS = ("retire_after", "退役窗口", "introduced_in", "deprecated_in",
                  "removed_in", "since", "until")
# 生命周期边界列的合法取值：显式登记的前向边界(由合同 owner 维护)。空集 = 该列
# 任何版本字面量都判红 —— fail-closed，不得把生命周期表当"免检区"。
LIFECYCLE_BOUNDARY_VALUES = {"0.12.0"}
LIFECYCLE_BOUNDARY_RE = re.compile(
    r'"(?:' + "|".join(LIFECYCLE_KEYS) + r')"\s*:\s*"(\d+\.\d+\.\d+)"')


def mask_lifecycle_boundaries(line):
    """把生命周期边界字段的版本值挖成等长空白后返回, 供版本字面量扫描使用。

    只挖值本身(保留键名与引号长度), 同行其它版本字面量仍会被抓 ——
    `"product_version": "0.12.0", "retire_after": "0.99.0"` 中的前者必须 FAIL。
    """
    def repl(m):
        return " " * (m.end(1) - m.start(1))
    return LIFECYCLE_BOUNDARY_RE.sub(repl, line)


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
                    full = os.path.join(dirpath, fn)
                    # W4-A3：跳过符号链接 —— 链接不是"第二权威"，同一份文件被扫两次
                    # 会把目标文件里的历史版本字面量重复计数（实测：facade 链接到
                    # gaia zlib 测试后，1.3.1/1.3.2 被当成未知产品版本字面量）。
                    if os.path.islink(full):
                        continue
                    yield full
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
    for _d in TEST_FIXTURE_DIRS:
        if rel == _d or rel.startswith(_d + os.sep):
            return  # 目录级合成数据面(tests/config/fixtures/**)
    alpha_full = re.compile(r"(\d+\.\d+\.\d+)-alpha\.(\d+)")
    # 生命周期列口径 (W4-A3): 合同文档里 "退役窗口 / retire_after" 表格列同样是
    # **前向边界**语义(该列的取值定义"何时退出", 天然 != 当前基础号)。表头命中
    # LIFECYCLE_KEYS 即对其后的连续表格数据行启用豁免; 表格结束(非 '|' 行)即复位。
    lifecycle_table = False
    with open(path, encoding="utf-8", errors="replace") as f:
        for i, line in enumerate(f, 1):
            if line.lstrip().startswith("|"):
                if any(k in line for k in LIFECYCLE_KEYS):
                    lifecycle_table = True
            else:
                lifecycle_table = False
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
            # R-08 / W4-A3: 未知版本字面量扫描前先挖掉标准条款号与合同生命周期
            # 边界值。alpha/prerelease 判定仍跑在原始行上 —— 口径只收窄未知字面量
            # 误报面, 不放宽漂移判定。
            if lifecycle_table:
                allowed = LIFECYCLE_BOUNDARY_VALUES
                scan_line = mask_standard_clause_numbers(line)
            else:
                allowed = {base_num}
                scan_line = mask_lifecycle_boundaries(mask_standard_clause_numbers(line))
            for m in BASE_RE.finditer(scan_line):
                if m.group(1) not in allowed:
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
