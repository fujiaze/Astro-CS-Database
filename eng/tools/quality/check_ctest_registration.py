#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""check_ctest_registration.py — CTest 目标 → CI 检查项注册闭包校验器（CI-REG-002）。

控制包依据：05_FINDINGS_REGISTER_20260911.md STD-F7「本轮新增测试目标未被 CI 显式登记」
处置 4「validators 增加负向检查（发现未注册的新 add_test 目标即 FAIL）」。

契约（fail-closed，任一 C 规则违规 → exit 1）：
  C1  扫描活动 CTest 面全部 CMake 源（名为 CMakeLists.txt 或 *.cmake），解析
      add_test(NAME <target> ...) 目标名；出现非 NAME 形式 add_test 亦判违规
      （无法静态枚举 = 无法证明已注册）。扫描排除 run/ build/ out/ artifacts/
      .git/ third_party/，以及路径含 archive/superseded 的归档旧树（AGENTS.md 目录规范）。
  C2  每个目标必须被「显式注册」覆盖，二者其一：
        (a) eng/ci/checks.json 中某检查项的 ctest_targets 模式（精确名或 glob）匹配；或
        (b) eng/ci/ctest_baseline.json 冻结存量清单命中（CI-REG-002 建立时点一次性收编的
            存量目标）。
  C3  未覆盖目标 → FAIL，逐条输出 target <- source（新增/改名测试必须同提交注册）。
  C4  反向完整性：ctest_targets 模式匹配不到任何现存目标 → FAIL（陈旧注册）。
  C5  基线漂移：ctest_baseline.json 中已不存在于 CMake 源的目标 → FAIL
      （删除测试必须同提交收缩基线，防基线无限膨胀）。
  C6  防「登记但未真跑」：无 glob 字符的 ctest_targets 名必须出现在该检查项 command
      的某个参数里（如 ctest -R ^p1001_real_nodes$），否则 FAIL。

负例（--selftest，全部在内存 fixture 上跑，零副作用）：
  S1  新增未注册 add_test → FAIL（C3）；
  S2  同目标由 ctest_targets 显式登记且 command 携带 → PASS；
  S3  同目标由冻结基线覆盖 → PASS；
  S4  ctest_targets 指向不存在的目标 → FAIL（C4）；
  S5  ctest_targets 精确名不在 command 中 → FAIL（C6）；
  S6  基线含已消失目标 → FAIL（C5）；
  S7  主仓库真实三件套（源/注册表/基线）→ PASS（回归保护，现场漂移即红）。

用法:
  python3 eng/tools/quality/check_ctest_registration.py                     # 校验（CI 检查面）
  python3 eng/tools/quality/check_ctest_registration.py --output run/ci/... # 同时落证据 JSON
  python3 eng/tools/quality/check_ctest_registration.py --write-baseline    # 维护面：重算存量基线
  python3 eng/tools/quality/check_ctest_registration.py --selftest          # 负例自检

只读（除 --write-baseline/--output 显式请求）；仅 stdlib。
"""
from __future__ import annotations

import argparse
import datetime as _dt
import fnmatch
import json
import pathlib
import re
import subprocess
import sys

REPO = pathlib.Path(__file__).resolve().parents[3]
REGISTRY_REL = "eng/ci/checks.json"

# 口径：**已显式登记进 eng/ci/checks.json 的 ctest_targets
# ⇒ 不再进本冻结基线**。冻结基线是 CI-REG-002 建立时点**存量目标**的一次性过渡收编
# 机制；显式登记强于冻结（登记会被 C4/C6 双向校验，冻结只保证不漂移）。所以看到
# 某个 add_test 目标不在本基线里，先查它是不是已被显式登记 —— 那是**正常**的，
# 不是遗漏。
BASELINE_REL = "eng/ci/ctest_baseline.json"
SCHEMA_VERSION = 1

# 扫描排除（AGENTS.md 目录规范：run/ 临时面、build/out 构建面、artifacts 证据面，
# 归档控制包旧树不是活动 CTest 面）。
SKIP_DIR_NAMES = {
    "run", "build", "out", "artifacts", ".git", "third_party", "node_modules",
    ".venv", "__pycache__", "AstroCS.wiki",
}
SKIP_PATH_SUBSTR = ("archive", "superseded")

ADD_TEST_NAME_RE = re.compile(r"add_test\s*\(\s*NAME\s+([^\s()#]+)")
ADD_TEST_ANY_RE = re.compile(r"(?<![A-Za-z0-9_.])add_test\s*\(")
GLOB_CHARS = "*?["
PARSER_MODES = ("grammar", "legacy", "auto")
# 只属于"正则面求不出 foreach 头"的判红标记：grammar 面用更强解析器取代它们，
# 合并结构判据时必须滤掉，否则对 grammar 已解析出的循环重复判红。
_FOREACH_FACE_MARKERS = ("foreach 头", "目标名位置残留未求值变量", "foreach 嵌套深度")
# 完备枚举面（C1 的首选事实源）：与「配置期注册一致性」判据共用同一套 CMake 解析器，
# 它按命令/块结构求值（foreach 头变量、变量派生、跨目录作用域、条件上下文）而不是按文本
# 正则取字面量。见 C1 说明与 docstring 的「C1 枚举完备性」一节。
COND_TOOL_REL = "eng/tools/quality/check_ctest_reg_condition.py"


class GrammarUnavailable(RuntimeError):
    """完备枚举面的解析器不可用 —— 由 --parser 决定 fallback 还是 fail-closed。"""


_grammar_mod = None


def load_grammar():
    """惰性加载完备枚举面的 CMake 解析器（保持被依赖方为叶子模块，无循环 import）。"""
    global _grammar_mod
    if _grammar_mod is not None:
        return _grammar_mod
    path = REPO / COND_TOOL_REL
    if not path.is_file():
        raise GrammarUnavailable("完备枚举面工具 %s 不存在" % COND_TOOL_REL)
    import importlib.util
    spec = importlib.util.spec_from_file_location("ctest_reg_grammar", path)
    mod = importlib.util.module_from_spec(spec)
    sys.modules["ctest_reg_grammar"] = mod
    try:
        spec.loader.exec_module(mod)
    except Exception as exc:  # noqa: BLE001 —— 依赖不可用一律 fail-closed，不吞不猜
        raise GrammarUnavailable(
            "完备枚举面工具 %s 加载失败（%s: %s）"
            % (COND_TOOL_REL, exc.__class__.__name__, exc)) from None
    _grammar_mod = mod
    return mod


def parse_targets_grammar(sources: dict):
    """完备枚举面：复用 CMake 解析器求值 add_test(NAME …) 的名字集合。

    返回 (targets, errors)。名字位置不可静态枚举时该工具**已经** fail-closed（它的 C6），
    此处把它的 errors 原样并入本门的 C1 判红面；名字位置不可能返回带 ${…} 的假名。
    """
    mod = load_grammar()
    an = mod.analyze(sources, root=REPO)
    reg, _ref, _vio, errors, _summary, _runtime = mod.judge(an, root=REPO)
    targets = {}
    for name in sorted(reg):
        occ = reg[name]
        targets[name] = occ[0].path if occ else "?"
    # 只并入**名字枚举不可信**的那类错误（该工具 C6 的子集），不并它自己的引用面
    # 非退化守卫（C7）：那条是"引用面为空"的自守，与名字枚举完备性无关，并进来会
    # 把没有引用面的最小复现 fixture 判成红（假红）。
    keep = ("名字", "未解析变量", "不可静态求值", "未闭合", "结构不可信",
            "扫描面为空", "元素过多", "括号未闭合", "foreach 变量名")
    name_errors = [e for e in errors
                   if e.startswith("C6") and any(k in e for k in keep)]
    if not targets and not name_errors:
        name_errors.append("C1 完备枚举面未解析出任何 add_test(NAME …) 名字 —— ")
    return targets, ["C1 " + e for e in name_errors]


def parse_targets_legacy(sources: dict):
    """遗留路径：文本级正则 + foreach 头求值（保留供 --parser legacy 复现与对照）。"""
    targets, errors = _parse_targets_regex(sources)
    return targets, errors


def parse_targets(sources: dict, parser: str = "grammar"):
    """C1 名字枚举：parser=grammar（默认，完备）| legacy（正则）| auto（grammar 失败即回退）。"""
    if parser not in PARSER_MODES:
        raise ValueError("未知 parser 模式 %r（可选 %s）" % (parser, "/".join(PARSER_MODES)))
    if parser == "legacy":
        return parse_targets_legacy(sources)
    try:
        targets, gerrs = parse_targets_grammar(sources)
    except GrammarUnavailable as exc:
        if parser == "auto":
            targets, errors = parse_targets_legacy(sources)
            return targets, errors + [
                "C1 完备枚举面不可用，已回退遗留正则面（%s）—— 回退面可能有枚举盲点，"
                "CI 不得使用 auto：%s" % (exc.__class__.__name__, exc)]
        raise
    # 名字枚举面换成完备解析器，**房规结构判据不放松**：正则面里与 foreach 头求值无关的
    # 结构违规（非 NAME 形式的 add_test、括号结构等）继续并入 C1 判红面。只滤掉
    # 「foreach 头不可静态枚举」这一类 —— 那一类正是 grammar 面用更强解析器取代的部分，
    # 留着会把 grammar 已解析出来的循环重复判红（假红）。
    _t, lerrs = parse_targets_legacy(sources)
    structural = [e for e in lerrs if not any(m in e for m in _FOREACH_FACE_MARKERS)]
    return targets, gerrs + structural


def _utc_now() -> str:
    return _dt.datetime.now(_dt.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


# --------------------------------------------------------------------------- 扫描 ----

def _is_skipped(rel: pathlib.Path) -> bool:
    if set(rel.parts) & SKIP_DIR_NAMES:
        return True
    low = str(rel).replace("\\", "/")
    return any(s in low for s in SKIP_PATH_SUBSTR)


class GitUnavailable(RuntimeError):
    """git 不可用 / 非 git 工作树 / 版本库面为空 —— CTest 面（= 版本库面）无法枚举。"""


def git_tracked_set(repo: pathlib.Path) -> set:
    """git 已跟踪文件集（仓库相对、'/' 分隔）。

    CTest 面 = **版本库面**。并发写者（其它任务）在工作区留下的
    **未跟踪** CMake 源不属于本仓库的任何检出，若纳入判据，本门就会在
    别人写到一半的目录上判红（实测：lib/algorithms/projection/tests/p3wcs/ 的
    未跟踪 CMakeLists 重复注册 p3_wcs ⇒ C1 红），把「CI 注册闭包」变成
    「工作区快照」判据。故判定面收敛到 git ls-files；未跟踪源单独计数并在
    证据 JSON 的 untracked_cmake_sources 字段留痕（不静默丢弃）。

    GITDECOUPLE-02（原缺陷）：git 不可用时**返回空集**会被 discover_sources 当成
    "全部源都未跟踪" ⇒ targets=0 ⇒ 全部 CMake 源被判 untracked、全部
    ctest_targets 被判 C4「陈旧注册」、全部基线目标被判 C5「已消失」（实测真仓库
    形态的镜像树：282 条错误，语义全部指向「target 消失」而真因是「git 不可用」）。
    依赖不可用 ⇒ 抛 GitUnavailable，由 main 显式点名 + fail-closed（rc=2；
    docs/ci/01_CHECKS.md §1 fail-closed，CI_SPEC §2.2「仓库不可用 ⇒ runner error」）。
    """
    try:
        out = subprocess.run(["git", "-C", str(repo), "ls-files", "-z"],
                             capture_output=True, text=True, timeout=120)
    except (OSError, subprocess.SubprocessError) as exc:
        raise GitUnavailable(f"git 调用失败（{exc.__class__.__name__}: {exc}）") from None
    if out.returncode != 0:
        last = (out.stderr.strip().splitlines() or [""])[-1]
        raise GitUnavailable(f"git ls-files rc={out.returncode}: {last[:140]}")
    tracked = {p for p in out.stdout.split("\0") if p}
    if not tracked:
        raise GitUnavailable(
            f"git ls-files 在 {repo} 返回空版本库面 —— CTest 面无法枚举"
            f"（空面不得当成「全部源未跟踪」）")
    return tracked


def discover_sources(repo: pathlib.Path, *, tracked_only: bool = False) -> tuple:
    """活动 CTest 面 CMake 源；返回 (sources, untracked_sources)。

    tracked_only=True 时排除未跟踪源（CI 面）；False 时全收（工作区快照/自测用）。
    """
    found: set[pathlib.Path] = set()
    for pattern in ("CMakeLists.txt", "*.cmake"):
        for path in repo.rglob(pattern):
            if _is_skipped(path.relative_to(repo)):
                continue
            found.add(path)
    tracked = git_tracked_set(repo) if tracked_only else None
    sources, untracked = [], []
    for path in sorted(found):
        rel = str(path.relative_to(repo)).replace("\\", "/")
        if tracked is not None and rel not in tracked:
            untracked.append(rel)
            continue
        sources.append(path)
    return sources, untracked


def _visible_text(text: str) -> str:
    """剥离整行注释（行首 # 行），避免注释里的 add_test 伪目标。"""
    return "\n".join(l for l in text.splitlines() if not l.lstrip().startswith("#"))


FOREACH_RE = re.compile(r'foreach\s*\(\s*([A-Za-z_][A-Za-z0-9_]*)\s+([^)]*?)\s*\)(.*?)endforeach\s*\(', re.S)
# 循环头的**值列表可以来自变量**（`set(NAME a b c)` / `set(NAME "a;b;c")`）。
# 这里只做「set(字面量 …) 的迭代求值」这一最小静态面；其它派生（list(APPEND …)、
# function 参数、宏体）不求值，走 fail-closed 点名，**不再**把 ${VAR} 当成一个目标名收下。
SET_VAR_RE = re.compile(
    r'(?<![A-Za-z0-9_])set\s*\(\s*([A-Za-z_][A-Za-z0-9_]*)[ \t]+([^()]*?)\s*\)')
_VAR_REF_RE = re.compile(r'\$\{([A-Za-z_][A-Za-z0-9_]*)\}')
_INLINE_COMMENT_RE = re.compile(r'(?:^|[ \t])#[^\n]*')
_MAX_VAR_PASSES = 8


def _split_values(tok):
    """CMake 值列表 → 逐值：空白与分号皆分隔；去引号；丢空串；保序去重。"""
    out = []
    for part in re.split(r'[ \t\r\n;]+', tok.strip()):
        part = part.strip().strip('"')
        if part and part not in out:
            out.append(part)
    return out


def set_vars(text):
    """源内 `set(VAR value…)` → {VAR: 值列表}；同名后写覆盖前写（CMake 同作用域语义）。"""
    found = {}
    for var, body in SET_VAR_RE.findall(_INLINE_COMMENT_RE.sub(chr(39)+chr(39), text)):
        found[var] = _split_values(body)
    return found


def _dir_parent(rel):
    """仓库相对路径 → 父目录（Toposix，根为 ""）。"""
    d = str(pathlib.PurePosixPath(rel).parent)
    return "" if d == "." else d


def _unique_value(v):
    """变量值列表 → 唯一取值（多值即为不同取值，返回 None）。"""
    if len(v) != 1:
        return None
    return v[0]


def _scope_values(var, rel, file_vars, scope_vars):
    """变量在 `rel` 处的取值列表：本源后写覆盖为最高优先，其次向外逐级目录（CMake 目录作用域）。

    跨目录求值只用于**补足本源无法定值的循环头**（同仓实证：lib/algorithms/drizzle/CMakeLists.txt:68
    的 `foreach(_f ${ASTROCS_CFITSIO_SOURCES})`，其值由 eng/cmake/cfitsio_sources.cmake:3 提供、
    经根 CMakeLists.txt:66 的 include 进入根作用域）。本源已能定值的名字**不**看外层，
    避免把子目录里合法的同名遮蔽误判成同名多值。
    """
    own_dir = _dir_parent(rel)
    vals = [file_vars[var]] if var in file_vars else []
    d = own_dir
    first = True
    while True:
        if not (first and var in file_vars):   # 本源已收过自己目录的 set，不重复计
            vals += scope_vars.get((d, var), [])
        first = False
        if d == "":
            break
        d = _dir_parent(d)
    return vals


def _resolve_loop_values(tok, file_vars, scope_vars, rel, errors, line):
    """foreach 头的值列表记号 → 字面值列表；不可静态枚举返回 None（由调用方判红）。"""
    cur = _split_values(tok)
    for _ in range(_MAX_VAR_PASSES):
        refs = []
        for tokv in cur:
            refs += _VAR_REF_RE.findall(tokv)
        if not refs:
            return cur
        nxt = []
        for tokv in cur:
            parts = [tokv]
            for ref in refs:
                vals = _scope_values(ref, rel, file_vars, scope_vars)
                if not vals:
                    continue
                if _unique_value(vals) is None:
                    # 同名多值 ⇒ 生效值取决于求值顺序，不得猜：fail-closed 点名
                    errors.append(
                        "C1 %s:%d: foreach 头变量 ${%s} 有 %d 个不同取值（%s）—— "
                        "同名 set 的生效值取决于扫描顺序，无法静态定名 ⇒ fail-closed"
                        % (rel, line, ref, len(vals), " | ".join(" ".join(v) for v in vals[:4])))
                    return None
                prevals = vals[0]
                newparts = []
                for pt in parts:
                    if "${" + ref + "}" in pt:
                        newparts += [pt.replace("${" + ref + "}", v) for v in prevals]
                    else:
                        newparts.append(pt)
                parts = newparts
            nxt += parts
        if nxt == cur:
            break
        cur = nxt
    for tokv in cur:
        if _VAR_REF_RE.search(tokv):
            # 残留未求值变量：取值集合不可静态枚举。**不在此处报错** —— 报不报红
            # 取决于循环体是否含 add_test(NAME …)（见 expand_foreach.repl），
            # 由那里按块上下文点名，免得对与 CTest 面无关的循环产生假红。
            return None
    return cur


def expand_foreach(text, depth=0, errors=None, rel="?", scope_vars=None, file_vars=None):
    """把 foreach(var a b c) ... endforeach() 展开成逐值副本。

    解析器原先只取字面量名，于是
    lib/infrastructure/pipeline/orchestrator/cpp/tests/CMakeLists.txt 的
    foreach(orch_test logger checkpoint) 生成的 orchestrator_logger_units /
    orchestrator_checkpoint_units 两条真实 ctest 在扫描面上不可见 —— 这正是
    空/不透明扫描面那一类失效（看不见的东西永远不会被判红）。

    **循环头来自变量**是同一类失效里更隐蔽的一种：eng/tests/unit/p1_psfw/CMakeLists.txt:51
    的 `foreach(g ${V6_P1_PSFW_GROUPS})`（列表在 :50 的 set 里）原先只替换 ${g}、
    把 ${V6_P1_PSFW_GROUPS} 原样留在目标名里 ⇒ 判定面出现一个**不存在的目标名**
    `p1_psfw_${V6_P1_PSFW_GROUPS}`（假条目虚增面，还被 glob 结构性命中、随基线冻结固化），
    而 8 个**真实**注册名 p1_psfw_{anea,winfo,oracle,components,common,gates,record,negative}
    从未进入判定面（真名若被删/改名，本门永不判红）。现按 `set(VAR …)` 求值循环头；
    求不出即 fail-closed 点名（C1），绝不把带 ${…} 的字面量当目标名。
    """
    if depth > 4:
        if errors is not None:
            errors.append('C1 %s: foreach 嵌套深度 > 4，展开不可控 ⇒ fail-closed' % rel)
        return text

    if file_vars is None:
        file_vars = set_vars(text)
    if scope_vars is None:
        scope_vars = {}

    def repl(m):
        var, tok, body = m.group(1), m.group(2), m.group(3)
        line = text[:m.start()].count(chr(10)) + 1
        before = len(errors) if errors is not None else 0
        original = m.group(0)
        vals = _resolve_loop_values(tok, file_vars, scope_vars, rel, errors, line)
        if vals is None:
            # 求不出循环头取值 ⇒ 不产出任何目标名（绝不产出假名）。而只有**循环体里
            # 真有 add_test(NAME …)** 时才判红：本门的判定面只由 add_test(NAME …) 构成，
            # 循环体只做 add_executable/源列表拼接的（同仓实证：
            # lib/algorithms/drizzle/**/CMakeLists.txt 与
            # lib/algorithms/integration/phase2_integrate/ 的 ${ASTROCS_CFITSIO_SOURCES}
            # 循环、DRZ_EXTRA_TESTS / DRZ_PROBE_TESTS 循环）与 CTest 目标集合无关，
            # 对它判红是假红 —— 那会把门变成噪声门。
            if ADD_TEST_NAME_RE.search(body):
                if errors is not None and len(errors) == before:
                    errors.append(
                        'C1 %s: foreach 头 %s 的值集合无法静态枚举，且循环体含 '
                        'add_test(NAME …) —— 该 ctest 目标名不可枚举：既不得占位收下，'
                        '也不得静默丢弃 ⇒ fail-closed（循环头须可在本源或目录作用域内静态求值）'
                        % (rel, tok.strip()))
                return original
            # 体与 CTest 目标集合无关：原样保留（不删体），免得把里面的 add_test 静默丢出扫描面。
            return body
        return chr(10).join(body.replace('${' + var + '}', v) for v in vals)

    expanded = FOREACH_RE.sub(repl, text)
    if expanded != text:
        return expand_foreach(expanded, depth + 1, errors, rel, scope_vars, file_vars)
    return expanded


def _parse_targets_regex(sources: dict) -> tuple:
    """返回 (target -> 仓库相对源路径, 结构违规列表)。

    循环头变量的求值需要两个作用域面（CMake 目录作用域）：`file_vars` = 本源 `set()`
    的后写覆盖结果；`scope_vars` = 逐目录的 `set()` 集合（跨文件形态，例如
    eng/cmake/cfitsio_sources.cmake 提供、根 CMakeLists.txt include 后被子目录引用）。
    """
    targets: dict = {}
    errors: list = []
    scope_vars: dict = {}
    for rel, raw in sorted(sources.items()):
        for var, vals in set_vars(_visible_text(raw)).items():
            entries = scope_vars.setdefault((_dir_parent(rel), var), [])
            if vals not in entries:
                entries.append(vals)
    for rel, raw in sorted(sources.items()):
        text = _visible_text(raw)
        text = expand_foreach(text, errors=errors, rel=rel, scope_vars=scope_vars,
                              file_vars=set_vars(text))
        names = ADD_TEST_NAME_RE.findall(text)
        total = len(ADD_TEST_ANY_RE.findall(text))
        if total != len(names):
            errors.append(
                "C1 %s: add_test 调用 %d 处但 NAME 形式仅 %d 处"
                "（非 NAME 形式无法静态枚举 → fail-closed）" % (rel, total, len(names)))
        for name in names:
            name = name.strip().strip('"')
            if not name:
                continue
            if '${' in name or '$(' in name or '$<' in name:
                # 假名守卫：名字位置残留变量/生成器表达式 ⇒ 它**不是一个目标名**。
                # 不得当成目标收下（假条目虚增判定面、被 glob 结构性命中、并随
                # ctest_baseline 冻结固化），也不得静默丢弃 ⇒ fail-closed 点名。
                errors.append(
                    "C1 %s: 目标名位置残留未求值变量/生成器表达式 %r —— "
                    "它不是任何真实 ctest 目标；既不得当假条目收下，也不得静默"
                    "丢弃 ⇒ fail-closed（如为 foreach 头变量，须可在本源内静态求值）"
                    % (rel, name))
                continue
            prev = targets.get(name)
            if prev is not None and prev != rel:
                errors.append("C1 目标 %s 在多个源中重复注册：%s 与 %s" % (name, prev, rel))
                continue
            targets[name] = rel
    return targets, errors


# -------------------------------------------------------------------- 注册表/基线 ----

def load_json(path: pathlib.Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def command_surface(check: dict) -> list:
    """聚合项 + 其各 step 的 command 全文（C6 口径订正）。

    CI-001 ID 收敛后注册表是**两层**结构：聚合项（CHK-*）持有 ctest_targets，
    而真正调用 ctest 的是其 steps[].command（deep_ci_driver.py ctest-target
    --target <名>）。C6 的判据是「登记了就必须真跑」，故运行面 = 聚合项 command
    ∪ 全部 step command；只看聚合项 command 会把**确实在跑**的目标误判为
    「登记但未真跑」（实测 56 条误红）。
    """
    cmd = list(check.get("command", []) or [])
    for step in check.get("steps", []) or []:
        if isinstance(step, dict):
            cmd += list(step.get("command", []) or [])
    return cmd


def registry_patterns(registry: dict) -> list:
    """返回 [(check_id, pattern, command_surface)]。"""
    out: list = []
    for check in registry.get("checks", []):
        if not isinstance(check, dict):
            continue
        for pat in check.get("ctest_targets", []) or []:
            if isinstance(pat, str) and pat:
                out.append((check["id"], pat, command_surface(check)))
    return out


def baseline_targets(baseline: dict) -> set:
    return {t for t in baseline.get("targets", []) if isinstance(t, str) and t}


# ------------------------------------------------------------------------- 判定 ----

def evaluate(targets: dict, registry: dict, baseline: dict) -> dict:
    """核心判定（纯函数，供主流程与 --selftest 共用）。"""
    errors: list = []
    patterns = registry_patterns(registry)
    base = baseline_targets(baseline)

    # C0 扫描面非空（fail-closed；docs/ci/01_CHECKS.md §1「scanned == 0 ⇒ rc != 0」）：
    # 空目标集下 C3/C4/C5 都可能"恰好"不触发（注册表无 ctest_targets 且基线为空），
    # 于是"什么都没扫到"会静默判绿 —— 那是恒真门，必须点名判红。
    if not targets:
        errors.append(
            "C0 扫描面为空：版本库面内未解析出任何 add_test(NAME …) 目标 —— "
            "fail-closed 拒绝空扫描判绿（若确实无测试，请显式登记豁免面）")

    # C7 假名守卫（裁定一）：目标名里带未求值变量/生成器表达式 ⇒ 它**不是**任何真实
    # ctest 目标。假条目的危害不是"多一条"，而是**覆盖面虚增**：一个不存在的东西
    # 会被名字通配结构性命中（本仓实证：CHK-UNIT 的 `p1_psfw_*` 与 CHK-NWORKER 的
    # `p1_*` 双双命中 `p1_psfw_${V6_P1_PSFW_GROUPS}`），于是"已覆盖"是假的、
    # 还可能随 ctest_baseline 冻结固化。判定面必须由真实名字构成，故一律判红。
    for name in sorted(targets):
        if "${" in name or "$(" in name or "$<" in name:
            errors.append(
                "C7 目标名 %r 含未求值变量/生成器表达式（源 %s）—— 它不是真实 ctest "
                "目标；名字通配会把它当成已覆盖，覆盖面虚增：不得收下，也不得静默丢弃"
                % (name, targets[name]))

    explicit: dict = {}
    dangling: list = []
    not_in_command: list = []
    for cid, pat, command in patterns:
        hits = [t for t in targets if fnmatch.fnmatchcase(t, pat)]
        if not hits:
            dangling.append("%s:%s" % (cid, pat))
        for target in hits:
            explicit.setdefault(target, cid)
        if not any(ch in pat for ch in GLOB_CHARS):
            if pat not in "\n".join(command):
                not_in_command.append("%s:%s" % (cid, pat))

    registered = set(explicit) | base
    unregistered = sorted(t for t in targets if t not in registered)
    baseline_only = sorted(t for t in targets if t in base and t not in explicit)
    stale_baseline = sorted(base - set(targets))

    if unregistered:
        errors.append(
            "C3 未注册的 add_test 目标 %d 个（新增/改名测试必须同提交在 eng/ci/checks.json "
            "登记显式检查项，或经 eng/ci/ctest_baseline.json 冻结收编）：" % len(unregistered))
        for target in unregistered:
            errors.append("C3   %s <- %s" % (target, targets[target]))
    for item in dangling:
        errors.append("C4 ctest_targets 模式匹配不到任何现存目标（陈旧注册）：%s" % item)
    for item in stale_baseline:
        errors.append(
            "C5 ctest_baseline.json 目标已不在 CMake 源中（删除测试须同步收缩基线）：%s" % item)
    for item in not_in_command:
        errors.append(
            "C6 ctest_targets 精确名未出现在该检查 command 中（登记但未真跑）：%s" % item)

    return {
        "targets_total": len(targets),
        "sources_total": len(set(targets.values())),
        "registered_explicit": sorted(explicit),
        "registered_baseline_only": baseline_only,
        "baseline_total": len(base),
        "unregistered": unregistered,
        "stale_baseline": stale_baseline,
        "dangling_patterns": sorted(dangling),
        "pattern_not_in_command": sorted(not_in_command),
        "errors": errors,
    }


# --------------------------------------------------------------------- 真实数据面 ----

def collect_real(repo: pathlib.Path, *, tracked_only: bool = True, parser: str = "grammar") -> tuple:
    """真实数据面：(targets, structural_errors, untracked_cmake_sources)。

    tracked_only=True（默认，CI 面）= 只判**版本库**内的 CMake 源；未跟踪的
    工作区源在第三个返回值里留痕。自测可用 tracked_only=False 复现工作区快照。
    """
    paths, untracked = discover_sources(repo, tracked_only=tracked_only)
    sources = {}
    for path in paths:
        rel = str(path.relative_to(repo)).replace("\\", "/")
        sources[rel] = path.read_text(encoding="utf-8", errors="replace")
    targets, errors = parse_targets(sources, parser=parser)
    return targets, errors, untracked


def write_baseline(repo: pathlib.Path, targets: dict, explicit: set, previous: dict = None) -> pathlib.Path:
    """重算存量基线：现存目标 − 显式登记目标（显式登记者不进基线）。"""
    entries = sorted(t for t in targets
                     if t not in explicit and "${" not in t and "$(" not in t and "$<" not in t)
    try:
        sha = subprocess.run(["git", "-C", str(repo), "rev-parse", "HEAD"],
                             capture_output=True, text=True, timeout=30).stdout.strip()
    except (OSError, subprocess.SubprocessError):
        sha = ""
    data = {
        "schema_version": SCHEMA_VERSION,
        "purpose": ("CI-REG-002 存量 CTest 目标冻结清单：建立时点已存在的 add_test 目标"
                    "一次性收编（避免 150+ 存量目标逐个登记）；此后新增/改名目标必须显式"
                    "登记 eng/ci/checks.json 的 ctest_targets，删除目标必须同步收缩本清单"
                    "（eng/tools/quality/check_ctest_registration.py C5 fail-closed）。"
                    "维护命令：--write-baseline。"),
        "base_commit": sha,
        "generated_utc": _utc_now(),
        "sources": sorted(set(targets.values())),
        "targets": entries,
    }
    path = repo / BASELINE_REL
    if previous is not None and previous.get("targets") == entries:
        return path
    path.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return path


# ------------------------------------------------------------------------- 自检 ----

FIXTURE_CMAKE = (
    "add_executable(demo_test demo_test.cpp)\n"
    "add_test(NAME demo_units COMMAND demo_test units)\n"
    "add_test(NAME brand_new_target COMMAND demo_test new)\n"
)
FIXTURE_CMAKE_STALE = "add_test(NAME demo_units COMMAND demo_test units)\n"
# S9/S10：循环头来自变量（实证形态 eng/tests/unit/p1_psfw/CMakeLists.txt:50-54）。
# 旧解析器只替换循环变量、把头变量原样留在名字里 ⇒ 产出一个假名（**不存在的目标**），
# 而 2 个真名从未进入判定面。
FIXTURE_LOOP_VAR = (
    "set(DEMO_GROUPS anea winfo)\n"
    "foreach(g " + __import__("builtins").chr(36) + "{DEMO_GROUPS})\n"
    "  add_test(NAME demo_" + __import__("builtins").chr(36) + "{g} COMMAND demo_test " + __import__("builtins").chr(36) + "{g})\n"
    "endforeach()\n")
FIXTURE_LOOP_VAR_UNRESOLVED = (
    "foreach(g " + __import__("builtins").chr(36) + "{DEMO_MISSING_GROUPS})\n"
    "  add_test(NAME demo_" + __import__("builtins").chr(36) + "{g} COMMAND demo_test " + __import__("builtins").chr(36) + "{g})\n"
    "endforeach()\n")
FIXTURE_FAKE_NAME = "add_test(NAME demo_" + __import__("builtins").chr(36) + "{LATE_VAR} COMMAND demo_test)\n"

FIXTURE_CMAKE_ONE = "add_test(NAME demo_units COMMAND demo_test units)\n"


def _fixture_registry(target=None, command_name=None, *, via_step=False) -> dict:
    checks = [{
        "id": "DEMO-BASE", "profiles": ["fast"], "platform": "any",
        "command": ["python3", "eng/tools/quality/check_ctest_registration.py"],
        "timeout_seconds": 60, "heavy": False, "mutates_workspace": False,
        "outputs": [], "waivable": False,
    }]
    if target is not None:
        entry = {
            "id": "DEMO-CTEST", "profiles": ["linux-main"], "platform": "any",
            "command": ["python3", "eng/ci/run_checks.py", "--check", "DEMO-CTEST", "--quiet"],
            "timeout_seconds": 300, "heavy": False, "mutates_workspace": False,
            "outputs": [], "waivable": False, "ctest_targets": [target],
        }
        if via_step:
            # 两层结构：聚合项持有 ctest_targets，真正跑 ctest 的是 step command
            entry["steps"] = [{
                "id": "DEMO-CTEST-STEP", "profiles": ["linux-main"], "platform": "linux",
                "command": ["python3", "eng/tools/quality/deep_ci_driver.py", "ctest-target",
                            "--build-dir", "run/ci/build", "--target", target,
                            "--output", "run/ci/ctest/%s.json" % target],
                "timeout_seconds": 300, "heavy": False, "mutates_workspace": False,
                "outputs": [], "waivable": False,
            }]
        else:
            entry["command"] = ["ctest", "--test-dir", "run/ci/build", "-R",
                                "^%s$" % (command_name or target), "--output-on-failure"]
        checks.append(entry)
    return {"schema_version": 1, "checks": checks}


def run_selftest() -> int:
    results: list = []

    def case(name: str, sources: dict, registry: dict, baseline: dict, expect_pass: bool,
             parser: str = "legacy") -> None:
        targets, structural = parse_targets(sources, parser=parser)
        verdict = evaluate(targets, registry, baseline)
        errs = structural + verdict["errors"]
        ok = (not errs) if expect_pass else bool(errs)
        results.append({"case": name, "expect": "PASS" if expect_pass else "FAIL",
                        "actual": "PASS" if not errs else "FAIL", "ok": ok, "errors": errs})

    src = {"CMakeLists.txt": FIXTURE_CMAKE}
    empty_reg = _fixture_registry()
    case("S1_unregistered_new_target", src, empty_reg, {"targets": []}, False)
    case("S2_explicit_registration", {"CMakeLists.txt": FIXTURE_CMAKE_ONE},
         _fixture_registry("demo_units"), {"targets": []}, True)
    case("S3_baseline_covered", src, empty_reg,
         {"targets": ["demo_units", "brand_new_target"]}, True)
    case("S4_dangling_pattern", src, _fixture_registry("ghost_target"), {"targets": []}, False)
    case("S5_pattern_not_in_command", src, _fixture_registry("demo_units", "other_name"),
         {"targets": []}, False)
    case("S6_stale_baseline", {"CMakeLists.txt": FIXTURE_CMAKE_STALE}, empty_reg,
         {"targets": ["removed_target"]}, False)
    # S8：两层注册表 —— 目标由 step command 真跑（聚合项 command 只是转发），
    #     不得判「登记但未真跑」（C6 口径订正的正例面）。
    case("S8_pattern_in_step_command", {"CMakeLists.txt": FIXTURE_CMAKE_ONE},
         _fixture_registry("demo_units", via_step=True), {"targets": []}, True)

    # S9：循环头来自变量的循环（"+" 号面）—— legacy 与 grammar 两条路径都必须枚举出两个真名。
    for _p in ("legacy", "grammar"):
        _t, _e = parse_targets({"CMakeLists.txt": FIXTURE_LOOP_VAR}, parser=_p)
        _ok = (sorted(_t) == ["demo_anea", "demo_winfo"]
               and not any("${" in n for n in _t) and not _e)
        results.append({"case": "S9_loop_header_variable_%s" % _p, "expect": "PASS",
                        "actual": "PASS" if _ok else "FAIL", "ok": _ok,
                        "errors": [] if _ok else ["targets=%r errors=%r" % (sorted(_t), _e)]})
    # S10：循环头变量**不可静态枚举** ⇒ 该处名字集合看不见 ⇒ 必须判红（不得静默丢）。
    for _p in ("legacy", "grammar"):
        _t, _e = parse_targets({"CMakeLists.txt": FIXTURE_LOOP_VAR_UNRESOLVED}, parser=_p)
        _ok = bool(_e) and not _t
        results.append({"case": "S10_loop_header_unresolved_%s" % _p, "expect": "FAIL",
                        "actual": "FAIL" if _ok else "PASS", "ok": _ok,
                        "errors": _e or ["targets=%r（未被判红）" % sorted(_t)]})
    # S11：名字位置残留变量 ⇒ 假名不得被当成目标（C7）。
    for _p in ("legacy", "grammar"):
        _t, _e = parse_targets({"CMakeLists.txt": FIXTURE_FAKE_NAME}, parser=_p)
        _v = evaluate(_t, empty_reg, {"targets": []})
        _errs = _e + _v["errors"]
        _ok = bool(_errs) and not any("${" in n for n in _t)
        results.append({"case": "S11_fake_name_rejected_%s" % _p, "expect": "FAIL",
                        "actual": "FAIL" if _errs else "PASS", "ok": _ok,
                        "errors": _errs})
    # S12：假名若混进目标集，**结构性命中**（glob）不得把它洗成"已覆盖"（C7 必红）。
    _v = evaluate({"demo_" + __import__("builtins").chr(36) + "{LATE_VAR}": "CMakeLists.txt"},
                  _fixture_registry("demo_*"), {"targets": []})
    _ok = any("C7" in e for e in _v["errors"])
    results.append({"case": "S12_glob_must_not_whitewash_fake", "expect": "FAIL",
                    "actual": "FAIL" if _ok else "PASS", "ok": _ok,
                    "errors": _v["errors"]})

    targets, structural, untracked_real = collect_real(REPO, parser="grammar")
    verdict = evaluate(targets, load_json(REPO / REGISTRY_REL),
                       load_json(REPO / BASELINE_REL))
    errs = structural + verdict["errors"]
    results.append({"case": "S7_real_repo", "expect": "PASS",
                    "actual": "PASS" if not errs else "FAIL", "ok": not errs, "errors": errs})

    failed = [r for r in results if not r["ok"]]
    print(json.dumps({"tool": "check_ctest_registration.py", "mode": "selftest",
                      "cases": results, "failed": len(failed),
                      "verdict": "PASS" if not failed else "FAIL"},
                     ensure_ascii=False, indent=2))
    return 0 if not failed else 1


# --------------------------------------------------------------------------- CLI ----

def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="CTest 目标 → CI 注册闭包校验（CI-REG-002）")
    ap.add_argument("--repo", default=str(REPO))
    ap.add_argument("--registry", default=REGISTRY_REL)
    ap.add_argument("--baseline", default=BASELINE_REL)
    ap.add_argument("--output", default=None, help="证据 JSON 落盘路径（run/ 下）")
    ap.add_argument("--write-baseline", action="store_true",
                    help="维护面：按当前源码重算 eng/ci/ctest_baseline.json（CI 不调用）")
    ap.add_argument("--selftest", action="store_true", help="负例自检（内存 fixture）")
    ap.add_argument("--parser", choices=list(PARSER_MODES), default="grammar",
                    help="名字枚举面：grammar=完备（默认，复用 CMake 解析器）；"
                         "legacy=遗留正则；auto=grammar 不可用即回退 legacy（仅诊断用）")
    args = ap.parse_args(argv)

    if args.selftest:
        return run_selftest()

    repo = pathlib.Path(args.repo).resolve()
    try:
        targets, structural, untracked = collect_real(repo, parser=args.parser)
    except GitUnavailable as exc:
        # 依赖不可用 ⇒ 点名 + fail-closed（rc=2）。**不得**退化成"空版本库面 ⇒ 全部目标
        # 未注册/陈旧"（原缺陷：282 条错误全部指向「target 消失」而真因是「git 不可用」）。
        print("CTEST-REG-FAIL: GIT_UNAVAILABLE %s" % exc)
        print("  CTest 面 = 版本库面（口径）；git 面不可用 ⇒ 不给出注册闭包结论，"
              "fail-closed rc=2（空版本库面不等于「target 消失/陈旧注册」）")
        if args.output:
            out = pathlib.Path(args.output)
            if not out.is_absolute():
                out = repo / out
            out.parent.mkdir(parents=True, exist_ok=True)
            out.write_text(json.dumps({
                "tool": "check_ctest_registration.py",
                "rule": "CI-REG-002 / STD-F7 处置 4",
                "generated_utc": _utc_now(),
                "registry": args.registry,
                "baseline": args.baseline,
                "git_unavailable": str(exc),
                "verdict": "GIT_UNAVAILABLE",
                "error_count": 1,
                "errors": ["GIT_UNAVAILABLE %s" % exc],
            }, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        return 2
    registry = load_json(repo / args.registry)
    baseline_path = repo / args.baseline
    if not baseline_path.is_file():
        # GAP-027 fail-closed（CI-001）：缺 C5 基线即静默回落空基线（漂移门失去基线）
        print("CTEST-REG-FAIL: 缺 ctest 基线 %s（C5 基线漂移门无基线可比）——"
              "fail-closed 判 FAIL；确需重建请显式 --write-baseline" % baseline_path)
        return 1
    baseline = load_json(baseline_path)

    if args.write_baseline:
        explicit = {t for _cid, pat, _cmd in registry_patterns(registry)
                    for t in targets if fnmatch.fnmatchcase(t, pat)}
        path = write_baseline(repo, targets, explicit, previous=baseline)
        # conclusion-anchor: --write-baseline 分支的回执；verdict 指「基线写入动作完成」，
        # 被测 ctest 注册面的判定在同文件 evaluate() 之后，不在本分支。
        print(json.dumps({"tool": "check_ctest_registration.py", "mode": "write-baseline",
                          "baseline": str(path.relative_to(repo)).replace("\\", "/"),
                          "targets": len(targets), "explicit": len(explicit),
                          "baseline_entries": len(targets) - len(explicit),
                          "verdict": "PASS"}, ensure_ascii=False, indent=2))
        return 0

    verdict = evaluate(targets, registry, baseline)
    errors = structural + verdict["errors"]
    summary = {
        "tool": "check_ctest_registration.py",
        "rule": "CI-REG-002 / STD-F7 处置 4",
        "generated_utc": _utc_now(),
        "registry": args.registry,
        "baseline": args.baseline,
        "baseline_base_commit": baseline.get("base_commit", ""),
        "targets_total": verdict["targets_total"],
        "sources_total": verdict["sources_total"],
        "registered_explicit": verdict["registered_explicit"],
        "registered_baseline_only_count": len(verdict["registered_baseline_only"]),
        "unregistered": verdict["unregistered"],
        "stale_baseline": verdict["stale_baseline"],
        "dangling_patterns": verdict["dangling_patterns"],
        "pattern_not_in_command": verdict["pattern_not_in_command"],
        "untracked_cmake_sources": untracked,
        "untracked_note": ("未跟踪的 CMake 源不计入 CTest 面（本门判"
                           "版本库，不判并发写者的工作区快照）；此处留痕不静默丢弃"),
        "error_count": len(errors),
        "errors": errors,
        "verdict": "PASS" if not errors else "FAIL",
    }
    text = json.dumps(summary, ensure_ascii=False, indent=2)
    if args.output:
        out = pathlib.Path(args.output)
        if not out.is_absolute():
            out = repo / out
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(text + "\n", encoding="utf-8")
    # 人类可读摘要恒落 stdout（run.py V_EMPTY_OUTPUT 防线：非 waivable 检查必须留痕）
    print("[check_ctest_registration] targets=%d sources=%d explicit=%d baseline_only=%d "
          "verdict=%s" % (verdict["targets_total"], verdict["sources_total"],
                          len(verdict["registered_explicit"]),
                          len(verdict["registered_baseline_only"]), summary["verdict"]))
    for line in errors:
        print(line)
    print(text)
    return 0 if not errors else 1


if __name__ == "__main__":
    sys.exit(main())
