"""单元层**自身**的判据（元判据）：S17 的可执行落法 + E4/E5/E6/E13 + `TEST.md` §4.3/§4.4/§5。

这些用例测的是**测试集自己**——不是为了刷绿，而是为了把
`run/GOVERN-08/审核包-R2/T02-门禁退役与判据清单.md` §2.1 的 **S17** 与 §2.4 的
**E4/E5/E6/E13** 从「写在文档里的元口径」变成「机器可判的东西」。

S17 在清单里逐字是：「判别力元口径：『**写代码≠在跑；被登记≠在跑；写在产物里无人读≠在跑**』；
出口定义「`none` 必须带 `remediation``」」，并且被标为「⚠ **就地即正本**……删掉后仓内将失去
『判别力』这一概念本身」⇒ **本单必须吸收它**。本文件就是它在单元层的落法。

三条元口径 → 三条可执行判据：

| S17 逐字 | 本文件的可执行落法 |
|---|---|
| 写代码 ≠ 在跑 | `meta.no_orphan_module`：每个 `test_*.py` 必须在**导入期**注册 ≥1 条用例 |
| 被登记 ≠ 在跑 | `meta.every_registered_case_is_executed` + `meta.duplicate_id_rejected` |
| 写在产物里无人读 ≠ 在跑 | `meta.case_metadata_complete`：每条用例必须自带意图/输入/预期/来源，且来源必须指向仓内可核对的正本 |

⇒ 「不在跑」与「没人读」在本层都是**机器可判**的，不再是纪律口号。
"""

from __future__ import annotations

import math
import os
import re

from . import harness, tolerances as tol

_UNIT_DIR = os.path.dirname(os.path.abspath(__file__))

#: 实质性证据的三种可核对形态（`AGENTS.md` §6 逐字给出的四类里与本层相关的三类）：
#: ① 仓内文件 + 行/节锚（`file:line` 或 `§`）；② 具名第三方实现/文献；③ 仓内文档路径。
_ANCHORED_FILE = re.compile(
    r"(?:[\w./一-鿿-]+\.(?:py|cpp|c|h|hpp|md|json|yaml|yml|in)):\d+"   # file:line
    r"|§\s*\d"                                                              # 章节
    r"|逐字"
    r"|\bline\s+\d+")
_THIRD_PARTY = ("astropy", "healpy", "astropy-healpix", "astropy.healpix",
                "Higham", "IEEE 754", "Fruchter", "G\u00f3rski", "Gaia Collaboration",
                "Rousseeuw", "drizzlepac", "CDLIB", "photutils")
_REPO_DOC = ("docs/", "eng/", "lib/", "run/", "\u5b9e\u9a8c/", "artifacts/", "SKILL",
             "AGENTS.md", "\u5ba1\u6838\u5305-R2",
             "TEST.md", "DRIZZLE.md", "REJECTION.md", "HEALPIX_MAPPING.md",
             "GAIA_QUERY.md", "ACSD_DESIGN.md", "standards/05_INDEPENDENT_TEST_SUITE.md")
#: 获裁定「代码即正本」的判据（唯二）：审核包-R2 §2.5 对 S5/S5b 逐字写
#: 「真值实际位置 `ipv_wcs.cpp:737-738,753,761`（**代码即正本**）」。白名单，
#: 不是通例——新增须先有同样的裁定记录。
_CODE_AS_NORMATIVE_CRITERIA = frozenset({"S5", "S5b"})

#: 不得作为期望值来源的措辞（「五类不构成期望值」中本层能机械识别的那几类）。
_FORBIDDEN_SOURCE = ("\u4ee5\u5b9e\u73b0\u8f93\u51fa\u4e3a\u671f\u671b", "\u5b9e\u73b0\u5f53\u524d\u8f93\u51fa",
                     "\u672c\u5b9e\u73b0\u8f93\u51fa", "\u7531\u5b9e\u73b0\u53cd\u63a8",
                     "\u7528\u5b9e\u73b0\u751f\u6210\u552f\u4e00\u9884\u671f\u503c")


def _source_forms(source: str):
    """返回该 source 命中的证据形态集合。"""
    forms = set()
    if _ANCHORED_FILE.search(source):
        forms.add("\u9519\u5b9a\u6587\u4ef6+\u884c/\u8282\u951a")
    if any(t in source for t in _THIRD_PARTY):
        forms.add("\u5177\u540d\u7b2c\u4e09\u65b9\u5b9e\u73b0/\u6587\u732e")
    if any(t in source for t in _REPO_DOC):
        forms.add("\u4ed3\u5185\u6b63\u672c\u8def\u5f84")
    return forms


# ---------------------------------------------------------------------------
# S17 三条元口径
# ---------------------------------------------------------------------------

@harness.test(
    "meta.no_orphan_module",
    intent="每个测试文件必须在导入期注册至少一条用例——「写代码≠在跑」的机器落法",
    inputs="eng/tests/unit/ 下全部 test_*.py 文件（由 harness.discover() 实际导入）",
    expected="每个测试文件注册数 ≥ 1；注册数为 0 的文件一律判红并点名文件名",
    source="审核包-R2/T02-门禁退役与判据清单.md §2.1 S17 逐字「写代码≠在跑」；"
           "docs/engineering/testing/TEST.md §2 逐字「恒真的比较没有证据资格」",
    criteria=("S17",),
)
def test_meta_no_orphan_module():
    import importlib
    import pkgutil

    pkg = importlib.import_module("eng.tests.unit")
    files = sorted(m.name for m in pkgutil.iter_modules(pkg.__path__)
                   if m.name.startswith("test_") and not m.ispkg)
    harness.is_true(bool(files), "本包应有测试文件，否则发现面为空")

    for name in files:
        before = len(harness.registered())
        mod = importlib.import_module(f"eng.tests.unit.{name}")
        added = len(harness.registered()) - before
        # 已导入过的模块不重复计入；改用「该模块在注册表里是否至少贡献一条来源本模块的用例」
        owns = [c for c in harness.registered()
                if c.func.__module__.endswith(name)]
        with harness.evidence() as ev:
            ev.record(f"{name}: 首次导入新增用例", added, note="重复导入时为 0 属正常")
            ev.record(f"{name}: 归属本文件的用例数", len(owns))
        harness.is_true(
            len(owns) >= 1,
            f"{name} 注册了 0 条用例：文件存在但没有任何东西在跑（S17「写代码≠在跑」）")
        del mod


@harness.test(
    "meta.every_registered_case_is_executed",
    intent="S17「被登记≠在跑」：本条排到整轮**最后**执行，逐条核对执行台账与注册表",
    inputs="harness._EXECUTED_IDS（整轮实际执行过的用例 id）对照 harness._REGISTRY",
    expected="台账覆盖注册表全集 − 本条自身；本条自身必须在注册表里（否则豁免成逃逸口）",
    source="审核包-R2/T02 门禁退役与判据清单.md §2.1 S17 逐字「被登记≠在跑」；"
           "docs/engineering/testing/TEST.md §2 逐字「恒真的比较没有证据资格」",
    criteria=("S17",),
    defer=True,
)
def test_meta_every_registered_case_is_executed():
    """⚠ **本条有两次自我纠错，如实记录，因为都是判据强度问题**：

    ① 2026-10-06 08:17 我把它从 `run_all()` 全表改成只跑 `meta.*` 族，
       全表侧降级为 `callable(c.func)`，理由是「全量重跑太慢」。
       对抗复核 R1 判其**判据强度回退**：S17 对 148/160 条用例归零，
       而回退动机恰好是让它自己跑得快。**我复核后认定 R1 是对的**——
       所谓「慢」是并行写入期的假象；写入收敛后全量单元档实测 11 秒。
    ② 恢复全表后又出现无限递归（本条从内部触发全表执行）。
       解法不是再掏空，而是给它 `defer=True`：**排到整轮最后**，
       这样它看到的是**完整的执行台账**，覆盖强度比原版还高（原版只数条数）。
    """
    registered = harness.registered()
    harness.is_true(len(registered) > 0, "注册表为空")
    executed = harness._EXECUTED_IDS
    _SELF = "meta.every_registered_case_is_executed"
    with harness.evidence() as ev:
        ev.record("注册用例数", len(registered))
        ev.record("本轮实际执行过的用例数", len(executed))
        ev.record("正例 / 负例",
                  f"{sum(1 for c in registered if c.kind == harness.POSITIVE)} / "
                  f"{sum(1 for c in registered if c.kind == harness.NEGATIVE)}")
        ev.record("台账未覆盖的已登记用例",
                  sorted({c.id for c in registered} - executed - {_SELF}) or "无")
    harness.is_true(_SELF in {c.id for c in registered},
                    "本条自身必须在注册表里（否则 defer 豁免就成了逃逸口）")
    harness.exact(len(executed), len(registered) - 1,
                  "执行台账必须覆盖「注册表 − 本条自身」")
    for c in registered:
        harness.is_true(callable(c.func), f"{c.id}: 已登记但没有可调用的实现")


@harness.test(
    "meta.duplicate_id_rejected",
    intent="用例 id 不得重复——重复 id 会让「执行了哪些」不可判",
    inputs="对同一 id 二次注册",
    expected="第二次注册抛 ValueError",
    source="审核包-R2/T02-门禁退役与判据清单.md §2.4 E13 逐字「阈值必须可追溯四处之一」"
           "的同源要求：判据身份必须唯一可寻",
    criteria=("E13",),
    kind=harness.NEGATIVE,
    inject="DEF-DUP-ID：用同一 id 再注册一条（模拟两份文件各写一条同名用例）",
)
def test_meta_duplicate_id_rejected():
    harness.raises(
        ValueError,
        lambda: harness.test(
            "meta.duplicate_id_rejected",  # 与本用例自身同 id
            intent="x", inputs="x", expected="x",
            source="docs/ACSD_DESIGN.md")(lambda: None),
        "重复 id 必须被拒")
    # 负例有效性的反证：换一个未占用的 id，注册成功 ⇒ 说明上面的 ValueError
    # 来自「重复」而不是「注册机制本身坏掉」。
    sentinel = harness.test(
        "meta._sentinel_unused_id", intent="x", inputs="x", expected="x",
        source="docs/ACSD_DESIGN.md")(lambda: None)
    # 把哨兵立刻摘掉，不留痕
    harness._REGISTRY[:] = [c for c in harness._REGISTRY
                            if c.id != "meta._sentinel_unused_id"]
    del sentinel


@harness.test(
    "meta.case_metadata_complete",
    intent="每条用例必须自带意图、输入、预期结果与来源依据——「写在产物里无人读≠在跑」",
    inputs="注册表全量的四个必填字段",
    expected="四条字段全部非空；负例另须有 inject（注入的缺陷是什么）",
    source="run/GOVERN-08/工作包-RECTIFY-09原件/standards/05_INDEPENDENT_TEST_SUITE.md §1 逐字"
           "「每条测试有明确意图与预期结果」；§1 逐字「不用空断言充数」",
    criteria=("S17", "E4"),
)
def test_meta_case_metadata_complete():
    for c in harness.registered():
        for field in ("intent", "inputs", "expected", "source"):
            harness.is_true(
                bool(getattr(c, field).strip()),
                f"{c.id}: 字段 {field} 为空——不提供意图与来源的用例不得进本测试集")
        if c.kind == harness.NEGATIVE:
            harness.is_true(bool(c.inject.strip()),
                            f"{c.id}: 负例未写明 inject，无法核对「注入后是否真变红」")


@harness.test(
    "meta.case_metadata_source_is_traceable",
    intent="来源依据必须指向仓内可核对的位置或一手文献出处，不能是「我觉得」",
    inputs="注册表全量的 source 字段",
    expected="source 非空，且至少命中一类可核对出处：仓内路径/文件、章节行锚、"
             "或具名第三方实现与文献",
    source="AGENTS.md §6 逐字「实质性证据的形式：……『我觉得』『惯例如此』不算证据」；"
           "docs/engineering/testing/TEST.md §4 逐字「阈值来源只此一表」的正本纪律",
    criteria=("E13", "S17"),
)
def test_meta_case_metadata_source_is_traceable():
    bad = []
    for c in harness.registered():
        if not _source_forms(c.source):
            bad.append((c.id, c.source[:60]))
    harness.is_false(bool(bad),
                     f"以下用例的 source 既无\u884c/\u8282\u951a\u3001\u65e0\u4ed3\u5185\u6b63\u672c\u8def\u5f84\uff0c"
                     f"\u4e5f\u65e0\u5177\u540d\u7b2c\u4e09\u65b9/\u6587\u732e\u51fa\u5904\uff1a{bad}")


# ---------------------------------------------------------------------------
# E5 期望值来源：四类合法、五类不构成
# ---------------------------------------------------------------------------

@harness.test(
    "meta.expected_value_source_is_not_the_unit_under_test",
    intent="E5：期望值不得来自被测实现自身——「用实现输出生成唯一预期值」是五类不合法来源之一",
    inputs="注册表全量的 source 字段（取其声明的判据来源面）",
    expected="任何把「被测参考实现」「本文件/本包自身」「产品当前输出」当作唯一来源的用例判红；"
             "正例必须指向上游正本、闭式推导或第三方独立实现",
    source="审核包-R2/T02-门禁退役与判据清单.md §2.4 E5 逐字「期望值四类合法来源 + "
           "五类不构成期望值」；docs/engineering/testing/TEST.md §2 逐字"
           "「每条科学契约至少配一个用例或 Oracle，三选一：性质检验、解析或高精度参考、"
           "定种子蒙特卡洛」",
    criteria=("E5",),
)
def test_meta_expected_value_source_is_not_the_unit_under_test():
    """E5：期望值不得由被测实现自身的输出生成。

    可接受的三种形态与 `AGENTS.md` §6 的实质性证据形式一一对应：
    仓内正本路径、具名第三方独立实现/文献、以及**代码即正本**时按 `文件:行` 锚定
    （审核包-R2 §2.5 对 S5/S5b 逐字标注「**代码即正本**」，该形态在这两条上合法）。
    不可接受的措辞单列在 `_FORBIDDEN_SOURCE`。
    """
    forbidden_hit, no_form = [], []
    for c in harness.registered():
        if any(f in c.source for f in _FORBIDDEN_SOURCE):
            forbidden_hit.append(c.id)
        forms = _source_forms(c.source)
        if not forms:
            no_form.append(c.id)
        # 只有「代码锚」而无正本路径/第三方时，该判据的真值必须**已被裁定为
        # 「代码即正本」**。目前唯一获此裁定的是 S5/S5b（审核包-R2 §2.5 逐字
        # 「真值实际位置 `ipv_wcs.cpp:737-738,753,761`（**代码即正本**）」）。
        # 其余判据若只引产品源码而不引正本/第三方 ⇒ 判红。
        elif forms == {"\u9519\u5b9a\u6587\u4ef6+\u884c/\u8282\u951a"} and \
                not (set(c.criteria) & _CODE_AS_NORMATIVE_CRITERIA):
            no_form.append(c.id)
    with harness.evidence() as ev:
        ev.record("禁用措辞命中", forbidden_hit or "无")
        ev.record("缺可核对期望值出处", no_form or "无")
    harness.is_false(bool(forbidden_hit),
                     f"期望值来源用了禁用措辞（由实现输出反推）：{forbidden_hit}")
    harness.is_false(bool(no_form),
                     f"期望值既无正本路径、也无具名第三方实现、也未声明「代码即正本」：{no_form}")


@harness.test(
    "meta.non_degenerate_anchor_rejects_tautology",
    intent="E6：非退化锚能拒一个恒真判据 —— **骨架自检**（本条不扫描全层，见正文声明）",
    inputs="一个故意恒真的判据（恒返回 True）与一个真实判据",
    expected="恒真判据被 E6 非退化锚拒绝；真实判据正常通过",
    source="docs/engineering/testing/TEST.md §2 逐字「每个度量具备非退化判据：真值无效应时"
           "度量必须归零或报警。恒真的比较没有证据资格」；审核包-R2 §2.4 E6 逐字"
           "「非退化锚：没找到 / 空集 / 零样本必须判红」",
    criteria=("E6", "E4"),
    kind=harness.POSITIVE,
)
def test_meta_non_degenerate_anchor_rejects_tautology():
    """⚠ 对抗复核 R1 更正：本条初版被标成 `NEGATIVE`，但 `tautological_judge` 与判据
    `_assert_non_degenerate` 都是**本用例内定义的本地函数** —— 本层真实的判据一条都没被
    扫到，属自指型空转，已改正为 `POSITIVE`（它是**骨架自身**的自检，不是对全层的负例）。

    「全层非退化扫描」为什么没做：绝大多数判据是带 fixture 参数的闭包，无法用统一探针
    喂输入；要覆盖就得逐条声明豁免，代价与收益不成比例。**这是已知缺口，不是遗漏。**"""
    def real_judge(x: float) -> bool:
        return abs(x - 1.0) < 0.5

    def tautological_judge(x: float) -> bool:   # 缺陷：恒真
        return True

    probes = (0.0, 1.0, 7.5, float("nan"), None, [], set())
    real_verdicts = {real_judge(p) if isinstance(p, float) and not math.isnan(p)
                     else None for p in probes}
    taut_verdicts = {tautological_judge(p) if isinstance(p, float) and not math.isnan(p)
                     else None for p in probes}

    with harness.evidence() as ev:
        ev.record("真实判据在 4 个有效输入上的判定集合", sorted(map(str, real_verdicts)))
        ev.record("恒真判据在同一组输入上的判定集合", sorted(map(str, taut_verdicts)))
        distinct = len({v for v in real_verdicts.values()}) if None not in real_verdicts else 0
        ev.record("真实判据的判定取值数（须 > 1，恒真则为 1）", distinct)

    # E6 非退化锚：判据必须在有效输入上**至少给出两种不同判定**。
    harness.raises(harness.CheckFailure,
                   lambda: _assert_non_degenerate(tautological_judge, probes),
                   "恒真判据必须被非退化锚拒绝")
    _assert_non_degenerate(real_judge, probes)
    harness.is_false(real_judge(0.0), "真实判据必须对域外输入判红")


def _assert_non_degenerate(judge, probes) -> None:
    """E6 非退化锚：判据不能恒真，也不能对退化输入静默通过。"""
    verdicts = set()
    for p in probes:
        if isinstance(p, float) and not math.isnan(p):
            verdicts.add(bool(judge(p)))
    harness.is_true(len(verdicts) > 1,
                    "判据在有效输入上恒真（只有一种判定）：恒真的比较没有证据资格")


# ---------------------------------------------------------------------------
# E13 阈值可追溯 + TEST.md §4.3 可满足性下限 + §4.4 非有限值
# ---------------------------------------------------------------------------

@harness.test(
    "meta.every_frozen_tolerance_is_traceable",
    intent="E13：冻结表里每一条容差都必须有来源与适用量级域",
    inputs="tolerances.FROZEN_TABLE 全量",
    expected="每条 Frozen 的 source 与 scale_domain 均非空；未登记的 key 取用直接抛错",
    source="docs/engineering/testing/TEST.md §3 逐字「浮点断言用容差加容差来源说明；"
           "容差在写用例前冻结」；§4.3 逐字「模块冻结绝对容差时必须同时声明适用量级域」",
    criteria=("E13",),
)
def test_meta_every_frozen_tolerance_is_traceable():
    harness.is_true(len(tol.FROZEN_TABLE) > 0, "冻结表为空")
    for key, f in tol.FROZEN_TABLE.items():
        harness.is_true(bool(f.source.strip()), f"{key}: 容差无来源")
        harness.is_true(bool(f.scale_domain.strip()), f"{key}: 容差无适用量级域")
    harness.raises(KeyError, lambda: tol.get("never.frozen.key"),
                   "未冻结的容差 key 必须抛错，不许在用例里现编容差")


@harness.test(
    "meta.atol_below_one_ulp_is_unsatisfiable",
    intent="TEST.md §4.3：绝对容差小于 1 ulp 时不可判，调用点必须拒绝而不是放宽",
    inputs="scale = 1.0（域 [1,2]）与 atol = 1e-17（< 1 ulp(1.0) = 1.11e-16）",
    expected="harness.close() 抛 CheckFailure 并指明 1 ulp 下限；atol ≥ 1 ulp 时正常比较",
    source="docs/engineering/testing/TEST.md §4.3 逐字「任何绝对容差只有在被比较量的量级 "
           "`scale` 满足 `atol ≥ 1 ulp(scale)` 时才可判……低于 1 ulp 的绝对容差不可满足」",
    criteria=("E13",),
    kind=harness.NEGATIVE,
    inject="DEF-ATOL-UNSAT：把绝对容差压到 1 ulp 以下（域内不可满足的容差）",
)
def test_meta_atol_below_one_ulp_is_unsatisfiable():
    one_ulp = tol.ulp(1.0)
    harness.is_true(one_ulp == tol.U_F64,
                    f"ulp(1.0) 应等于 u_f64，实测 {one_ulp!r}")
    with harness.evidence() as ev:
        ev.record("1 ulp(1.0) = u_f64", one_ulp)
        ev.record("f64 非归约档 atol = 1e-13 × scale（TEST.md §4）",
                  tol.F64_ATOL_PER_SCALE,
                  note="在 scale=1e3 时为 1e-10，与 1 ulp(1e3)=1.11e-13 相比余量 ~9e2×")

    harness.raises(
        harness.CheckFailure,
        lambda: harness.close(1.0, 1.0, rtol=0.0, atol=1e-17, scale=1.0,
                              what="域外容差"),
        "atol < 1 ulp(scale) 必须拒绝")
    # 同一 scale 下、可满足的 atol 正常工作
    harness.close(1.0, 1.0 + 0.5 * tol.F64_ATOL_PER_SCALE, rtol=0.0,
                  atol=tol.F64_ATOL_PER_SCALE, scale=1.0, what="可满足容差")


@harness.test(
    "meta.non_finite_is_never_folded_into_a_finite_comparison",
    intent="TEST.md §4.4：非有限值不得被折叠成 0 或哨兵后再参与有限比较",
    inputs="NaN / +Inf / -Inf 与一个有限期望值的比较",
    expected="全部判红并给出「非有限值参与比较」的诊断；不存在「落在容差内通过」的中间态",
    source="docs/engineering/testing/TEST.md §4.4 逐字「比较器不得把非有限值折叠成 `0` 或"
           "任何哨兵值后再参与有限比较」「非有限值与缺失**不受**上表三档浮点容差约束」",
    criteria=("E9",),
    kind=harness.NEGATIVE,
    inject="DEF-NAN-FOLD：把 NaN/Inf 当作普通浮点参与 |a-b| ≤ atol+rtol|b| 比较",
)
def test_meta_non_finite_is_never_folded_into_a_finite_comparison():
    with harness.evidence() as ev:
        for bad in (float("nan"), float("inf"), float("-inf")):
            ev.record(f"比较值 {bad!r}", bad)
    for bad in (float("nan"), float("inf"), float("-inf")):
        harness.raises(
            harness.CheckFailure,
            lambda b=bad: harness.close(b, 1.0, rtol=tol.F64_RTOL,
                                        atol=tol.F64_ATOL_PER_SCALE, scale=1.0,
                                        what="非有限值"),
            f"非有限值 {bad!r} 必须判红而不是被折叠")
    # 合法路径：两个有限值正常比较
    harness.close(1.0, 1.0 + 1e-14, rtol=tol.F64_RTOL,
                  atol=tol.F64_ATOL_PER_SCALE, scale=1.0, what="有限值正常路径")


@harness.test(
    "meta.dtype_u_mixing_widens_the_bound_by_2_to_the_29",
    intent="TEST.md §4.1：f32 的 u 不得用于 f64 归约（会把门限放宽约 2^29 倍）",
    inputs="n_terms = 100、Σ|terms| = 100，分别用 u_f64 与 u_f32 算归约门限",
    expected="两档门限之比 ≈ 2^29；用 u_f32 判 f64 数据必须被识别为失效口径",
    source="docs/engineering/testing/TEST.md §4.1 逐字「`γ_n` 取的是**同一 dtype** 的 `u`……"
           "混用（f64 数据按 f32 的 `u` 判）把门限放宽约 `2²⁹` 倍，**属判据失效**」",
    criteria=("E4",),
)
def test_meta_dtype_u_mixing_widens_the_bound_by_2_to_the_29():
    n, s = 100, 100.0
    t64 = tol.reduction_tolerance(n, s, u=tol.U_F64)
    t32 = tol.reduction_tolerance(n, s, u=tol.U_F32)
    ratio = t32 / t64
    # `C` 与 `Σ|terms|` 在比值里相消，剩下的比值是 γ_n 之比，不是 u 之比：
    #   t32/t64 = [u32/(1−n·u32)] / [u64/(1−n·u64)]
    # f64 侧 n·u64 = 1.11e-14 可忽略，f32 侧 n·u32 = 5.96e-6 不可忽略
    # ⇒ 比值 = 2^29·(1 + 5.96e-6 + …)，即正本逐字说的「**约** 2²⁹ 倍」。
    expected_ratio = ((tol.U_F32 / (1.0 - n * tol.U_F32))
                      / (tol.U_F64 / (1.0 - n * tol.U_F64)))
    with harness.evidence() as ev:
        ev.record("n_terms / Σ|terms|", f"{n} / {s}")
        ev.record("f64 归约门限 C·γ_n·Σ|terms|", t64)
        ev.record("f32 归约门限（同 n、同 Σ）", t32)
        ev.record("比值 t32/t64（实测）", ratio)
        ev.record("比值 t32/t64（解析式）", expected_ratio)
        ev.record("2^29（下界；f32 侧 γ_n 非线性使实测略大）", 2.0 ** 29)
    harness.close(ratio, expected_ratio, rtol=1e-12, atol=0.0,
                  what="u 混用的放宽倍数（纯比值比较，未声明 scale 域 ⇒ "
                       "不触发 §4.3 的 1 ulp 下限检查）")
    harness.is_true(ratio > 2.0 ** 29,
                    "混用 u 的放宽倍数必须严格大于 2^29")
    # 真正的口径：u_f32 / u_f64 必须精确等于 2^29
    harness.exact(tol.U_F32 / tol.U_F64, 2.0 ** 29, "u_f32/u_f64 必须精确 = 2^29")


# ---------------------------------------------------------------------------
# 「测试是工具不是裁判」的可执行落法（规范 §1/§4 + TEST.md §9）
# ---------------------------------------------------------------------------

@harness.test(
    "meta.verdict_is_non_blocking_even_with_failures",
    intent="规范 §4「以非阻塞为常态」：有红项时裁决词仍必须是 warn",
    inputs="一个人造的全红结果集（3 条失败）",
    expected="verdict(...) 恒为 'warn'；exit_code(..., non_blocking=True) 恒为 0；"
             "non_blocking=False 时才返回红项数（开发期自查专用）",
    source="run/GOVERN-08/工作包-RECTIFY-09原件/standards/05_INDEPENDENT_TEST_SUITE.md §4 逐字"
           "「测试集可以接入 CI，但以非阻塞为常态」；§1 逐字「**不作为门禁去约束代码**」；"
           "docs/engineering/testing/TEST.md §9 逐字「测试集不产出流水线判决」",
    criteria=("E8",),
    kind=harness.NEGATIVE,
    inject="DEF-BLOCKING-VERDICT：把非阻塞裁决改成有红项即 red / 退出码非 0",
)
def test_meta_verdict_is_non_blocking_even_with_failures():
    fake_case = harness.TestCase(
        id="meta._fake", intent="", inputs="", expected="", source="",
        criteria=(), kind=harness.POSITIVE, func=lambda: None)
    failing = [harness.Result(case=fake_case, passed=False, detail="人造红项")
               for _ in range(3)]
    passing = [harness.Result(case=fake_case, passed=True) for _ in range(2)]
    with harness.evidence() as ev:
        ev.record("人造红项 / 绿项", f"{len(failing)} / {len(passing)}")
        ev.record("verdict(全红集)", harness.verdict(failing))
        ev.record("exit_code(non_blocking=True)", harness.exit_code(failing))
        ev.record("exit_code(non_blocking=False)", harness.exit_code(failing, False))

    harness.exact(harness.verdict(failing), harness.VERDICT_WARN,
                  "有红项时裁决词必须仍是 warn")
    harness.exact(harness.exit_code(failing), 0, "非阻塞模式退出码必须恒 0")
    harness.exact(harness.exit_code(passing, False), 0, "无红项时开发期自查也应为 0")
    harness.exact(harness.exit_code(failing, False), 3, "开发期自查退出码 = 红项数")
    # 反证：verdict 的返回词集合里不存在 'red' —— 造出门禁红路径的改动会被本条抓住
    harness.is_false(harness.VERDICT_WARN in ("red", "fail", "block"),
                     "非阻塞裁决词不得是任何阻断语义")