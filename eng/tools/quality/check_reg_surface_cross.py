#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""check_reg_surface_cross.py — 注册面三面交叉判据（配置期注册 / 既有门枚举 / 按名引用）。

目的（本判据存在的理由）：**让盲区不可能存在而不被发现**。同一件事此前有三个各自自洽的
名字面，任一面的盲点都只有它自己看不见：

  面 A `check_ctest_reg_condition.py` 的**注册面**：解析活动 CMake 面全部 add_test(NAME …)，
       按命令/块结构求值（foreach 头变量、变量派生、跨目录作用域）—— 本仓实测 446 个真名。
  面 B `check_ctest_registration.py`（CI-REG-002）的**枚举面**：登记闭包门自己枚举出的目标名，
       并与 eng/ci/checks.json 的 ctest_targets 与 eng/ci/ctest_baseline.json 对账。
  面 C **按名引用面**：所有**不通过 CMake 注册**而按名字引用 ctest 目标的地方 ——
       eng/ci/checks.json 的 step command（`--target` / `ctest -R` / `--expect`）、
       ctest_baseline.json、ctest_skip_register.json、mutation_gates.json、
       runtime_oracle.py 的 anchor_names 等。

真实事故（本判据的立据事实）：面 B 的枚举面靠文本正则取 add_test 名字，不解析 foreach 头里的
变量。eng/tests/unit/p1_psfw/CMakeLists.txt:50-54 的 `set(V6_P1_PSFW_GROUPS …)` +
`foreach(g ${V6_P1_PSFW_GROUPS})` + `add_test(NAME p1_psfw_${g} …)` 于是被读成**一个不存在的
目标名** `p1_psfw_${V6_P1_PSFW_GROUPS}`，而 8 个真名 p1_psfw_{anea,winfo,oracle,components,
common,gates,record,negative} 从未进入面 B。后果有两层：
  ① 真名被删/改名时面 B 不判红（**看不见的东西永远不会被判红**）；
  ② 那个假名被名字通配（CHK-UNIT `p1_psfw_*` / CHK-NWORKER `p1_*`）**结构性命中**，
     于是"已覆盖"是假的 —— 覆盖面虚增，还可能随 ctest_baseline.json 冻结固化。

判据（任一面出现的名字，在其余面上要么有对应、要么有明确归属；缺一即判红；空面 fail-closed）：

  X1 面 B 不得含**假名**：面 B 枚举出的名字里不得残留未求值变量/生成器表达式
     （`${…}` / `$(` / `$<`）。假名不是"多一条"，而是覆盖面虚增。
  X2 面 B ⊆ 面 A：面 B 枚举出的每个名字都必须能在面 A 的注册面找到对应 ——
     面 B 多出来的名字只有两种身份：假名（X1 已判）或**实际不存在的目标**。
  X3 面 A ⊆ 面 B：面 A 注册的每个名字都必须出现在面 B 的枚举面上（否则面 B 对它失明，
     删掉/改名它时没有任何门会红）。两侧差集必须为空。
  X4 面 C 的名字 ⊆ 面 A ∩ 面 B：**每个被按名引用的名字都必须是一个真实注册的 ctest 目标，
     且两个枚举面都看得见它**。引用面点名了一个枚举面看不见的目标 ⇒ 该面引用会落空
     （驱动找不到目标）而枚举面不会判红 —— 这正是本判据要消灭的组合。
  X5 面 B 的"注册闭包"不得有**空的按名通路**：ctest_targets 里每个**精确名**条目必须
     出现在面 A 的注册面上（对应 C6 的"登记但未真跑"之外的另一半：登记却不存在的目标）；
     每个非精确名（glob）条目至少要匹配到面 A 的一个真名（否则是空覆盖）。
  X6 空面 fail-closed：任一面为空（0 个名字）即判红 —— 空面不得静默判绿。
  X7 不设白名单、不设豁免面：本判据没有 allowlist / waiver / baseline 参数。

产出：每个差异名逐条给归属（面 A 位置 / 面 B 位置 / 面 C 引用处 文件:行），供报告逐名对账。

用法:
  python3 eng/tools/quality/check_reg_surface_cross.py                 # 校验（CI 检查面）
  python3 eng/tools/quality/check_reg_surface_cross.py --json-out run/ci/reg-surface-cross.json
  python3 eng/tools/quality/check_reg_surface_cross.py --self-test     # 内存 fixture 正/负例
  python3 eng/tools/quality/check_reg_surface_cross.py --inventory     # 三面规模与差集清单
  python3 eng/tools/quality/check_reg_surface_cross.py --fault-inject all   # 真仓注入（逐例必红）

只读（除 --json-out 显式请求）；仅 stdlib。
"""
from __future__ import annotations

import argparse
import datetime as _dt
import fnmatch
import importlib.util
import json
import pathlib
import re
import sys

REPO = pathlib.Path(__file__).resolve().parents[3]
COND_TOOL_REL = "eng/tools/quality/check_ctest_reg_condition.py"
REG_TOOL_REL = "eng/tools/quality/check_ctest_registration.py"
REGISTRY_REL = "eng/ci/checks.json"
BASELINE_REL = "eng/ci/ctest_baseline.json"

# 面 C 的扫描面：非构建脚本里**按名引用 ctest 目标**的文件（版本库面内）。
# 只列文件，不列名字 —— 名字一律从文件里解析出来，不写死清单（写死清单＝白名单）。
GATES_DOC_REL = "docs/science/algorithms/GATES_AND_TOLERANCES.md"
KNOWN_FAILURES_REL = "eng/ci/known_failures.json"
# `ctest:<名>` 记号：`ctest:p1wcs_apbp` 形态（文档里的证据 ID）
_CTEST_TOKEN_RE = re.compile(r"ctest:([A-Za-z0-9_.\-]+)")

REFERENCE_SURFACE_FILES = (
    "eng/ci/checks.json",
    "eng/ci/ctest_baseline.json",
    "eng/ci/ctest_skip_register.json",
    "eng/ci/mutation_gates.json",
    "eng/tools/quality/runtime_oracle.py",
)
GLOB_CHARS = "*?["
_FAKE_RE = re.compile(r"\$\{.{1,80}?\}|\$\(|\$<")
# 按名引用 ctest 目标的命令行形态。取值用"非空白非引号"通吃 —— **必须**连 `*` / `[` / `?`
# 一起取下来，否则 `--target drizzle_pf_sb.*` 会被截成 `drizzle_pf_sb.`（一个不存在的名字），
# 于是"正则引用"被误判成"精确引用一个幽灵目标"，判据自己制造假红。
#
# 口径取自 eng/tools/quality/deep_ci_driver.py 的 ctest-target（不另发明一套）：
#   `--target X` / `ctest -R X` / `--tests-regex X` 里的 X 是**正则**，驱动实际执行
#   `ctest -R ^X$`，且**零命中按 fail-closed 判 FAIL** ⇒ 本判据以 `re.fullmatch` 口径核对；
#   `eng/tools/quality/ctest_driver.py --expect X` 里的 X 是**精确测试名**。
_TOKEN = r'[^\s\'"]{2,}'
_RE_META_RE = re.compile(r"[.\\*+?\[\](){}|^$]")
_TARGET_FLAGS = (
    ("--target", "regex"),
    ("-R", "regex"),
    ("--tests-regex", "regex"),
    ("--expect", "exact"),
)
_TARGET_ARG_RE = re.compile("|".join(
    "(?P<f{0}>{1}[= ]+(?P<v{0}>{2}))".format(i, re.escape(flag), _TOKEN)
    for i, (flag, _kind) in enumerate(_TARGET_FLAGS)))


def _target_tokens(blob):
    """从命令行/脚本行里取出按名引用 ctest 目标的记号：(token, 命中形态, 口径)。

    口径 kind ∈ {"regex", "exact"}：正则面按 `re.fullmatch` 核对（驱动的口径就是 `^X$`），
    精确面按名字相等核对。多分支正则只会命中一个分支 —— 按命名组取，不按组号猜。
    """
    out = []
    for mt in _TARGET_ARG_RE.finditer(blob):
        kind = None
        tok = None
        for i, (_flag, kind_i) in enumerate(_TARGET_FLAGS):
            tok = mt.group("v%d" % i)
            if tok:
                kind = kind_i
                break
        if not tok or kind is None:
            continue
        if tok.startswith("^") and tok.endswith("$") and len(tok) > 1:
            tok = tok[1:-1]          # 驱动的 `^X$` 锚点不是 X 的一部分
        if tok.endswith("."):
            continue                 # 命令里常见的省略写法/句读，不是目标名
        if kind == "regex" and not _RE_META_RE.search(tok):
            kind = "exact"          # 无元字符的正则 ≡ 精确名（`--target foo` 就是测试 foo）
        out.append((tok, mt.group(0).strip()[:60], kind))
    return out


class Unavailable(RuntimeError):
    """依赖（面 A / 面 B 工具或 git 面）不可用 —— fail-closed，不给结论。"""


def _utc_now() -> str:
    return _dt.datetime.now(_dt.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def _load(rel, name, repo=REPO):
    path = repo / rel
    if not path.is_file():
        raise Unavailable("依赖工具不存在：%s" % rel)
    spec = importlib.util.spec_from_file_location(name, path)
    mod = importlib.util.module_from_spec(spec)
    sys.modules[name] = mod
    try:
        spec.loader.exec_module(mod)
    except Exception as exc:  # noqa: BLE001
        raise Unavailable("依赖工具加载失败 %s（%s: %s）"
                          % (rel, exc.__class__.__name__, exc)) from None
    return mod

# --------------------------------------------------------------------------- 三面 ----

def surface_a(repo=REPO):
    """面 A：配置期注册面（add_test(NAME …) 名字 + 条件上下文）。

    返回 {name: {"at": "file:line", "cond": "…"}}。名字不可静态枚举由该工具自身
    fail-closed（其 C6），此处原样带出，不吞。
    """
    cond = _load(COND_TOOL_REL, "cross_cond", repo)
    srcs, untracked = cond.collect_real_sources(repo)
    an = cond.analyze(srcs, root=repo)
    reg, ref, vio, errors, summary, runtime = cond.judge(an, root=repo)
    names = {}
    for name in sorted(reg):
        occ = reg[name][0]
        names[name] = {"at": occ.where(repo), "cond": cond.show(occ.cond),
                       "occurrences": len(reg[name])}
    return {"names": names, "errors": list(errors), "summary": summary,
            "reference_names": sorted(ref), "untracked": list(untracked),
            "_sources": srcs}


def surface_b(repo=REPO, parser="grammar"):
    """面 B：既有门（CI-REG-002）的**枚举面** = 它自己解析出的目标名集合。

    这是判据的核心对象：面 B 是"判定面"本身，不是注册表。
    """
    reg_tool = _load(REG_TOOL_REL, "cross_reg", repo)
    try:
        targets, structural, untracked = reg_tool.collect_real(repo, parser=parser)
    except getattr(reg_tool, "GitUnavailable", Exception) as exc:
        raise Unavailable("面 B 的版本库面不可用：%s" % exc) from None
    registry = json.loads((repo / REGISTRY_REL).read_text(encoding="utf-8"))
    baseline = json.loads((repo / BASELINE_REL).read_text(encoding="utf-8"))
    verdict = reg_tool.evaluate(targets, registry, baseline)
    patterns = []
    for cid, pat, cmd in reg_tool.registry_patterns(registry):
        patterns.append({"check": cid, "pattern": pat, "command": cmd})
    return {"names": dict(targets), "errors": list(structural) + list(verdict["errors"]),
            "patterns": patterns, "verdict": verdict,
            "baseline_targets": sorted(reg_tool.baseline_targets(baseline)),
            "registry_targets": registry, "parser": parser}


def surface_c(repo=REPO, registry=None, baseline=None):
    """面 C：**非 CMake 注册**的按名引用面（运行期使用；同属名字面）。

    覆盖四类来源，全部从文件里解析，不写死名字：
      (1) eng/ci/checks.json：每个检查项/step 的 `ctest_targets`（精确名与 glob 分开归类）
          以及 command 里 `--target` / `ctest -R` / `--expect` / `--tests-regex` 的**精确名**引用；
      (2) eng/ci/ctest_baseline.json 的 targets 列表；
      (3) 其它非构建脚本（ctest_skip_register.json / mutation_gates.json / runtime_oracle.py）
          里按名引用 ctest 目标的位置；
      (4) 机器消费的**声明式核对面**：docs/science/algorithms/GATES_AND_TOLERANCES.md
          的 `ctest:<名>` 记号，与 eng/ci/known_failures.json 的 failures[].unit
          （kind=ctest）。二者都不是构建脚本，却被门当按名引用消费。
    """
    found = {}   # name -> [{at, form, kind}]

    def add(name, at, form, kind):
        if not name or "${" in name or "$(" in name or "$<" in name:
            return
        found.setdefault(name, []).append({"at": at, "form": form, "kind": kind})

    if registry is None:
        registry = json.loads((repo / REGISTRY_REL).read_text(encoding="utf-8"))
    if baseline is None:
        baseline = json.loads((repo / BASELINE_REL).read_text(encoding="utf-8"))

    # 面 C 的正则引用单独归类：正则**不是名字**，不得参与 X4 的逐名判定；
    # 但它自己是一条判据面 —— 正则 0 命中 = 空覆盖（X8），同样不得静默。
    patterns = {}
    for check in [c for c in registry.get("checks", []) if isinstance(c, dict)]:
        cid = check.get("id", "?")
        for pat in check.get("ctest_targets", []) or []:
            if isinstance(pat, str) and pat and not any(ch in pat for ch in GLOB_CHARS):
                add(pat, "%s#ctest_targets" % cid, "ctest_targets:%s" % cid, "exact")
        for unit in [check] + [s for s in (check.get("steps") or []) if isinstance(s, dict)]:
            uid = unit.get("id", cid)
            blob = " ".join(str(c) for c in (unit.get("command") or []))
            for tok, form, kind in _target_tokens(blob):
                if kind == "exact":
                    add(tok, "%s#command" % uid, form, "command")
                else:
                    ent = patterns.setdefault(tok, {"kind": "regex", "refs": []})
                    ent["refs"].append({"at": "%s#command" % uid, "form": form})
    for name in baseline.get("targets", []) or []:
        if isinstance(name, str) and name:
            add(name, "ctest_baseline.json#targets", "baseline", "baseline")
    for rel in REFERENCE_SURFACE_FILES:
        path = repo / rel
        if not path.is_file():
            continue
        if rel.endswith(".json"):
            continue   # checks.json / baseline 已按结构化字段解析，避免重复计数
        text = path.read_text(encoding="utf-8", errors="replace")
        for ln, line in enumerate(text.splitlines(), 1):
            for tok, form, kind in _target_tokens(line):
                if kind == "exact":
                    add(tok, "%s:%d" % (rel, ln), form, "script")
                else:
                    ent = patterns.setdefault(tok, {"kind": "regex", "refs": []})
                    ent["refs"].append({"at": "%s:%d" % (rel, ln), "form": form})
    # (4) 机器消费的**声明式核对面**：这两个面不是构建脚本，却被门当「按名引用」消费；
    #     它们引用的名字若在枚举面上不存在，引用落空而无人判红（与 checks.json 的
    #     ctest_targets 同型盲点，只是没有 glob，直接写死了名字）。
    #     (a) GATES_AND_TOLERANCES.md 的 `ctest:<名>` 记号 —— 消费者
    #         eng/tools/check_gates_and_tolerances.py:collect_ctest_names() 只用正则
    #         `add_test\(\s*NAME\s+([A-Za-z0-9_.-]+)` 从 CMake 文本抽名字；该正则看不见
    #         foreach 变量名与 gtest_discover_tests 的发现期名，所以此面的「存在性核对」
    #         本身就是一个正则盲面。
    #     (b) known_failures.json 的 failures[].unit（kind=ctest）—— 消费者
    #         eng/tools/quality/known_failures_baseline.py V3 只在「冻结基线 ∪（模式在
    #         基线集合上展开）」里核存在性，新增目标不在该集合内。
    gates_md = repo / GATES_DOC_REL
    if gates_md.is_file():
        for ln, line in enumerate(gates_md.read_text(encoding='utf-8', errors='replace').splitlines(), 1):
            for m in _CTEST_TOKEN_RE.finditer(line):
                add(m.group(1), '%s:%d' % (GATES_DOC_REL, ln), 'ctest:token', 'doc')
    kf_path = repo / KNOWN_FAILURES_REL
    if kf_path.is_file():
        try:
            kf = json.loads(kf_path.read_text(encoding='utf-8'))
        except (OSError, ValueError):
            kf = {}
        for i, f in enumerate(kf.get('failures') or []):
            if isinstance(f, dict) and str(f.get('kind', '')) == 'ctest':
                add(str(f.get('unit') or ''),
                    '%s#failures[%d].unit' % (KNOWN_FAILURES_REL, i),
                    'known_failures:unit', 'declared')
    return found, patterns


_DISCOVER_RE = re.compile(r"gtest_discover_tests\s*\(\s*([A-Za-z0-9_.:${}-]+)")
_TARGET_DEF_RE = re.compile(r"(?:add_executable|add_library)\s*\(\s*([A-Za-z0-9_.:${}-]+)")
_TEST_PREFIX_RE = re.compile(r"TEST_PREFIX\s+['\"]([^'\"]*)['\"]")


def surface_d(sources):
    """面 D：**发现期**注册面（gtest_discover_tests 的宿主与名字前缀）。

    这是第**三**种注册形态，静态 CMake 源里**看不见**它的 ctest 测试名：
    `gtest_discover_tests(<host> …)` 在 `ctest -N` 时把宿主二进制的 gtest 用例逐个注册成
    ctest 测试，名字形如 `<host>.<Suite>.<Case>`（受 TEST_PREFIX/TEST_SUFFIX 影响）。
    `add_test(NAME …)` 的静态枚举（面 A/面 B）**结构上不可能**包含它们 —— 所以
    按名引用 `--target phase2_.*`（CTEST-PHASE2-GATES）在静态面上 0 命中并不意味着空覆盖。

    本面**只登记可静态派生的那一半**：宿主目标 + 前缀/后缀 + 宿主定义处（add_executable/
    add_library）。归属必须**派生自真实调用**（X9 校验宿主确实被定义），不是白名单。
    """
    hosts = {}
    defs = {}
    for rel, text in sorted(sources.items()):
        for ln, line in enumerate(text.splitlines(), 1):
            if line.lstrip().startswith("#"):
                continue
            for tok in _TARGET_DEF_RE.findall(line):
                defs.setdefault(tok, "%s:%d" % (rel, ln))
            for tok in _DISCOVER_RE.findall(line):
                pf = _TEST_PREFIX_RE.search(line)
                ent = hosts.setdefault(tok, {"at": "%s:%d" % (rel, ln),
                                            "prefix": pf.group(1) if pf else "",
                                            "suffix": ""})
                if pf and not ent["prefix"]:
                    ent["prefix"] = pf.group(1)
    return {"hosts": hosts, "defs": defs}


def _discover_probes(host, ent):
    """宿主的发现期名字见证：真实名字是 `<host>.<Suite>.<Case>`（外加前缀/后缀）。

    静态面无法知道 Suite/Case，故用两组见证名探测正则是否**可能**命中真实名字：
      ① `<prefix><host>.<Suite>.<Case><suffix>`（gtest_discover_tests 的命名式）；
      ② 只有前缀/后缀时的裸名。任一组命中即算有归属（保守：宁可多要求一次核实，
      也不把"可能真实存在"的名字判成空覆盖）。
    """
    base = "%s%s%s" % (ent.get("prefix", ""), host, ent.get("suffix", ""))
    return [base + ".Suite.Case", base + ".Case", base]


# ------------------------------------------------------------------------- 判定 ----

def judge(a, b, c, disc=None, repo=REPO):
    """三面交叉判定（纯函数，供主流程/自检/注入共用）。"""
    errors = []
    detail = {}

    an = dict(a["names"])
    bn = dict(b["names"])
    c_patterns = c.pop("__patterns__", {}) if isinstance(c, dict) else {}
    disc = disc or {"hosts": {}, "defs": {}}
    c_patterns = {k: (v if isinstance(v, dict) else {"kind": "glob", "refs": v})
                  for k, v in c_patterns.items()}
    cn = dict(c)

    # X6 空面 fail-closed
    for label, names_of_face in (("面 A（配置期注册面）", an), ("面 B（既有门枚举面）", bn),
                                ("面 C（按名引用面）", cn)):
        # 注意：循环变量**不得**叫 d —— d 是面 D 的参数，遮蔽它就等于把发现期面丢掉
        if not names_of_face:
            errors.append("X6 %s 为空（0 个名字）—— 空面不得静默判绿，fail-closed" % label)

    # X1 面 B 不得含假名
    fake = sorted(n for n in bn if _FAKE_RE.search(n))
    for n in fake:
        errors.append("X1 面 B 枚举出假名 %r（源 %s）—— 名字含未求值变量/生成器表达式，"
                      "不是真实 ctest 目标；名字通配会把它当已覆盖（覆盖面虚增）" % (n, bn[n]))

    # X2 面 B ⊆ 面 A
    x2 = sorted(set(bn) - set(an))
    if x2:
        for n in x2:
            errors.append("X2 面 B 有名字 %r，面 A 的注册面上没有（源 %s）——"
                          "不是假名就是实际不存在的目标" % (n, bn.get(n, "?")))
    # X3 面 A ⊆ 面 B
    x3 = sorted(set(an) - set(bn))
    if x3:
        for n in x3:
            errors.append("X3 面 A 注册了 %r（%s），面 B 的枚举面上看不见 ⇒ 删掉/改名它时"
                          "没有任何门会红" % (n, an[n]["at"]))
    # X4 面 C ⊆ 面 A ∩ 面 B
    both = set(an) & set(bn)
    x4 = sorted(set(cn) - both)
    if x4:
        for n in x4:
            where = "; ".join("%s(%s)" % (r["at"], r["kind"]) for r in cn[n][:4])
            missing = []
            if n not in an:
                missing.append("面 A（注册面看不见）")
            if n not in bn:
                missing.append("面 B（枚举面看不见）")
            errors.append("X4 面 C 按名引用了 %r（%s），但 %s —— 引用会落空而枚举面不判红"
                          % (n, where, "、".join(missing)))
    # X5 面 B 的登记闭包：精确名条目必须存在于面 A；glob 条目至少匹配一个真名
    empty_glob = []
    for pat in b["patterns"]:
        p = pat["pattern"]
        hits = sorted(n for n in an if fnmatch.fnmatchcase(n, p))
        if not any(ch in p for ch in GLOB_CHARS):
            if p not in an:
                errors.append("X5 ctest_targets 精确名 %r（%s）不在面 A 注册面上 —— "
                              "登记了一个不存在的目标" % (p, pat["check"]))
        elif not hits:
            empty_glob.append((pat["check"], p))
    for cid, p in empty_glob:
        errors.append("X5 ctest_targets 通配 %r（%s）在面 A 上 0 命中 —— 空覆盖（覆盖面虚增）"
                      % (p, cid))

    # X8 面 C 的通配引用不得是**空覆盖**：`--target p1_*` 这类按名通配若在面 A 上 0
    # 命中，就等于"用通配建立了一个不存在的覆盖面"—— 与假名同一类危害（覆盖面虚增）。
    for pat in sorted(c_patterns):
        ent = c_patterns[pat]
        if not isinstance(ent, dict):
            ent = {"kind": "glob", "refs": ent}
        refs = ent.get("refs", [])
        where = "; ".join(str(r.get("at", r)) for r in refs[:3])
        if ent.get("kind") == "regex":
            try:
                rx = re.compile("^(?:%s)$" % pat)
            except re.error as exc:
                errors.append("X8 面 C 的正则引用 %r（%s）不可解析（%s）—— fail-closed"
                              % (pat, where, exc))
                continue
            hits = [n for n in an if rx.match(n)]
        else:
            hits = [n for n in an if fnmatch.fnmatchcase(n, pat)]
        if not hits and ent.get("kind") == "regex":
            # 静态面 0 命中 ≠ 空覆盖：还有**发现期**注册面（面 D）。
            # `gtest_discover_tests(<host>)` 在 ctest -N 时把 gtest 用例注册成
            # `<host>.<Suite>.<Case>`，这些名字**静态源里根本不存在**，只有运行期才知道。
            # 归属必须**派生**：宿主得真有 gtest_discover_tests 调用（下面 X9 再校验
            # 宿主目标确有定义），不是白名单。
            for host in sorted(disc.get("hosts", {})):
                for probe in _discover_probes(host, disc["hosts"][host]):
                    if rx.match(probe):
                        hits = [probe]
                        break
                if hits:
                    break
        if not hits:
            errors.append("X8 面 C 的引用 %r（%s）在面 A 静态注册面与面 D 发现期注册面上"
                          "均 0 命中 —— 空覆盖：建立了不存在的覆盖面（正则口径与驱动 "
                          "ctest -R ^X$ 一致，零命中在驱动侧按 fail-closed 判 FAIL）"
                          % (pat, where))

    # X9 发现期宿主必须**真实定义**（面 D 的归属不得凭空：宿主不存在 ⇒ 归属无效 ⇒ 判红）
    for host in sorted(disc.get("hosts", {})):
        if host not in disc.get("defs", {}):
            errors.append("X9 发现期宿主 %r（%s）没有对应的 add_executable/add_library "
                          "定义 —— gtest_discover_tests 找不到宿主，其测试名一个都不会注册"
                          % (host, disc["hosts"][host]["at"]))

    # 逐名归属（供报告对账；不作为判红面）
    for n in sorted(set(an) | set(bn) | set(cn)):
        detail[n] = {
            "in_a": n in an, "in_b": n in bn, "in_c": len(cn.get(n, [])),
            "a_at": an.get(n, {}).get("at"), "a_cond": an.get(n, {}).get("cond"),
            "b_at": bn.get(n), "c_refs": cn.get(n, [])[:6],
        }
    for name, info in a.get("errors", []) and [] or []:
        pass
    errors = list(errors) + ["A " + e for e in a.get("errors", [])]
    # 面 B 自身的错误（C1..C7）带出，但去掉重复的聚合行
    for e in b.get("errors", []):
        errors.append("B " + e)
    return {"errors": errors, "detail": detail,
            "counts": {"a": len(an), "b": len(bn), "c": len(cn),
                        "a_minus_b": len(x3), "b_minus_a": len(x2), "c_minus_ab": len(x4),
                        "fake_in_b": len(fake), "empty_glob": len(empty_glob),
                        "c_patterns": len(c_patterns),
                        "d_hosts": len(disc.get("hosts", {}))}}

# --------------------------------------------------------------------- 真实扫描 ----

def collect(repo=REPO, parser="grammar"):
    """三面真实扫描面（面 C 与面 B 共用同一份注册表/基线对象，避免两次读盘漂移）。"""
    registry = json.loads((repo / REGISTRY_REL).read_text(encoding="utf-8"))
    baseline = json.loads((repo / BASELINE_REL).read_text(encoding="utf-8"))
    a = surface_a(repo)
    b = surface_b(repo, parser=parser)
    c, c_patterns = surface_c(repo, registry=registry, baseline=baseline)
    c["__patterns__"] = c_patterns
    return a, b, c, surface_d(a.get("_sources", {}))


# -------------------------------------------------------------------- 自检面 ----

def _mk_a(pairs):
    return {"names": {n: {"at": "CMakeLists.txt:1", "cond": "TRUE", "occurrences": 1}
                      for n in pairs}, "errors": [], "summary": {}, "reference_names": [],
            "untracked": []}


def _mk_b(names, patterns):
    return {"names": {n: "CMakeLists.txt" for n in names}, "errors": [],
            "patterns": [{"check": "CHK", "pattern": p, "command": []} for p in patterns],
            "verdict": {}, "baseline_targets": [], "registry_targets": {}, "parser": "fixture"}


def run_self_test():
    """内存 fixture 正/负例（每例钉住一条 X 规则的判红面与非判红面）。"""
    cases = []

    def case(name, a, b, c, expect_red, expect_rule=None):
        j = judge(a, b, c)
        errs = j["errors"]
        red = bool(errs)
        ok = (red == expect_red) and (expect_rule is None or
                                      any(e.startswith(expect_rule) for e in errs))
        cases.append({"case": name, "expect": "RED" if expect_red else "GREEN",
                      "actual": "RED" if red else "GREEN", "ok": ok,
                      "errors": errs[:4]})

    names = ["t_alpha", "t_beta"]
    case("T1_three_surfaces_consistent", _mk_a(names), _mk_b(names, ["t_*"]),
         {"t_alpha": [{"at": "checks.json#x", "kind": "exact"}]}, False)
    case("T2_fake_name_in_surface_b", _mk_a(names),
         _mk_b(names + ["t_alpha_${LATE}"], ["t_*"]), {}, True, "X1")
    case("T3_b_has_name_absent_from_a", _mk_a(names),
         _mk_b(names + ["t_ghost"], ["t_*"]), {}, True, "X2")
    case("T4_a_has_name_invisible_to_b", _mk_a(names + ["t_gamma"]),
         _mk_b(names, ["t_*"]), {}, True, "X3")
    case("T5_c_references_name_invisible_to_b", _mk_a(names), _mk_b(names, ["t_*"]),
         {"t_ghost": [{"at": "checks.json#c", "kind": "command"}]}, True, "X4")
    case("T6_exact_target_not_registered", _mk_a(names), _mk_b(names, ["t_alpha", "t_ghost"]),
         {}, True, "X5")
    case("T7_empty_surface_fail_closed", _mk_a([]), _mk_b([], []), {}, True, "X6")
    case("T8_all_surfaces_empty", _mk_a([]), _mk_b([], []), {"t": []}, True, "X6")
    case("T9_c_subset_of_both_is_green", _mk_a(names), _mk_b(names, ["t_*"]),
         {"t_beta": [{"at": "checks.json#y", "kind": "exact"}]}, False)
    # 正例面：三面一致 + 面 C 有引用 + 无假名 + 无空覆盖 ⇒ 必须判绿（判据非恒真）
    case("T10_full_green_fixture", _mk_a(names + ["t_gamma"]),
         _mk_b(names + ["t_gamma"], ["t_*"]),
         {"t_alpha": [{"at": "checks.json#x", "kind": "exact"}]}, False)
    # T11/T12 面 C 的**声明式核对面**（本波新增的第 4 类来源）必须有正/负例：新加的解析
    # 路径若无人判，就又是一条「看起来覆盖了」的假通路。夹具落临时目录，不碰仓库。
    import tempfile
    with tempfile.TemporaryDirectory() as td:
        root = pathlib.Path(td)
        (root / 'eng' / 'ci').mkdir(parents=True)
        (root / 'docs' / 'science' / 'algorithms').mkdir(parents=True)
        (root / REGISTRY_REL).write_text(json.dumps({'schema_version': 1, 'checks': []}),
                                         encoding='utf-8')
        (root / BASELINE_REL).write_text(json.dumps({'targets': []}), encoding='utf-8')
        (root / GATES_DOC_REL).write_text('| 门 | ctest:t_gamma |' + chr(10), encoding='utf-8')
        (root / KNOWN_FAILURES_REL).write_text(
            json.dumps({'failures': [{'unit': 't_delta', 'kind': 'ctest'}]}), encoding='utf-8')
        decl, _decl_pats = surface_c(root)
        got = sorted(decl)
        case('T11_declaration_surfaces_enter_face_c', _mk_a(got),
             _mk_b(got, ['t_*']), decl, False)
        case('T12_declaration_ghost_name_reds', _mk_a(['t_alpha']),
             _mk_b(['t_alpha'], ['t_*']), decl, True, 'X4')
    failed = [c for c in cases if not c["ok"]]
    print(json.dumps({"tool": "check_reg_surface_cross.py", "mode": "self-test",
                      "cases": cases, "failed": len(failed),
                      "verdict": "PASS" if not failed else "FAIL"},
                     ensure_ascii=False, indent=2))
    return 0 if not failed else 1


# ------------------------------------------------------------------ 注入面（真仓） ----

def run_fault_inject(which, repo=REPO):
    """真实扫描面上的注入：改动只发生在内存副本（不写仓库）。逐例必须判红。"""
    a, b, c, d = collect(repo)
    cases = []

    def inject_and_run(name, mutate, expect_red, expect_rule):
        a2, b2, c2 = mutate(json.loads(json.dumps(a.get("names", {}))),
                            dict(b["names"]), dict(c))
        j = judge(a2, b2, c2)
        errs = j["errors"]
        red = bool(errs)
        ok = (red == expect_red) and any(e.startswith(expect_rule) for e in errs)
        cases.append({"case": name, "expect_rule": expect_rule,
                      "expect": "RED" if expect_red else "GREEN",
                      "actual": "RED" if red else "GREEN", "ok": ok,
                      "first_errors": [e for e in errs if e.startswith(expect_rule)][:2]})

    def inj_fake(an, bn, cn):
        bn["psfw_fake_${LOOPVAR}"] = "CMakeLists.txt"
        return _mk_a(an), _mk_b(bn, ["p1_psfw_*", "p1_*"]), cn

    def inj_b_missing(an, bn, cn):
        # 面 B 看不见某个真名（枚举面失明）
        victim = sorted(an)[0]
        bn.pop(victim, None)
        return _mk_a(an), _mk_b(bn, ["p1_psfw_*", "p1_*"]), cn

    def inj_c_ghost(an, bn, cn):
        cn["ghost_target_not_registered"] = [{"at": "fixture", "kind": "command"}]
        return _mk_a(an), _mk_b(bn, ["p1_psfw_*", "p1_*"]), cn

    def inj_empty(an, bn, cn):
        return _mk_a({}), _mk_b({}, []), {}

    wanted = {"fake": ("X1", inj_fake), "b_missing": ("X3", inj_b_missing),
              "c_ghost": ("X4", inj_c_ghost), "empty": ("X6", inj_empty)}
    # 基线（无注入）必须判绿
    j0 = judge(_mk_a(a["names"]), _mk_b(b["names"], [p["pattern"] for p in b["patterns"]]), c, d)
    cases.append({"case": "I0_no_injection", "expect": "GREEN",
                  "actual": "RED" if j0["errors"] else "GREEN",
                  "ok": not j0["errors"], "first_errors": j0["errors"][:3]})
    for key, (rule, fn) in wanted.items():
        if which not in ("all", key):
            continue
        inject_and_run("I1_" + key, fn, True, rule)
    failed = [x for x in cases if not x["ok"]]
    print(json.dumps({"tool": "check_reg_surface_cross.py", "mode": "fault-inject",
                      "cases": cases, "failed": len(failed),
                      "verdict": "PASS" if not failed else "FAIL"},
                     ensure_ascii=False, indent=2))
    return 0 if not failed else 1


# --------------------------------------------------------------------------- CLI ----

def main(argv=None):
    ap = argparse.ArgumentParser(description="注册面三面交叉判据（X1..X7）")
    ap.add_argument("--repo", default=str(REPO))
    ap.add_argument("--parser", choices=("grammar", "legacy", "auto"), default="grammar",
                    help="面 B 的名字枚举面（默认 grammar=完备）")
    ap.add_argument("--json-out", default=None)
    ap.add_argument("--self-test", action="store_true")
    ap.add_argument("--fault-inject", default=None,
                    help="真仓注入：all|fake|b_missing|c_ghost|empty（逐例必红）")
    ap.add_argument("--inventory", action="store_true")
    args = ap.parse_args(argv)

    if args.self_test:
        return run_self_test()
    if args.fault_inject:
        return run_fault_inject(args.fault_inject)

    repo = pathlib.Path(args.repo).resolve()
    try:
        a, b, c, d = collect(repo, parser=args.parser)
    except Unavailable as exc:
        print("REG-SURFACE-CROSS-FAIL: UNAVAILABLE %s" % exc)
        return 2
    j = judge(a, b, c, d, repo=repo)
    if args.inventory:
        payload = {"tool": "check_reg_surface_cross.py", "mode": "inventory",
                   "counts": j["counts"],
                   "a_only": sorted(set(a["names"]) - set(b["names"]))[:80],
                   "b_only": sorted(set(b["names"]) - set(a["names"]))[:80],
                   "c_only": sorted(set(c) - (set(a["names"]) & set(b["names"])))[:120],
                   "c_names_total": len(c),
                   "verdict": "RED" if j["errors"] else "GREEN"}
    else:
        payload = {"tool": "check_reg_surface_cross.py", "rule": "X1..X9",
                   "generated_utc": _utc_now(), "counts": j["counts"],
                   "error_count": len(j["errors"]), "errors": j["errors"],
                   "verdict": "RED" if j["errors"] else "GREEN"}
    if args.json_out:
        out = pathlib.Path(args.json_out)
        if not out.is_absolute():
            out = repo / out
        out.parent.mkdir(parents=True, exist_ok=True)
        j2 = dict(payload)
        j2["detail"] = j["detail"]
        out.write_text(json.dumps(j2, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(payload, ensure_ascii=False, indent=2))
    return 1 if j["errors"] else 0


if __name__ == "__main__":
    sys.exit(main())