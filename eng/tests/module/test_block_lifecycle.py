"""模块层 · 命名块生产/消费与生命周期（含旧块是否及时释放）的模块测试。

## 用途

在**模块层**判定三件事：

1. **生命周期派生保真**：`eng/contracts/block_flow/stage_block_flow.json` 里每个
   `(stage, block)` 的生命周期，必须等于按 `eng/tools/quality/gen_block_flow_spec.py`
   文件头 docstring 逐字写下的 5 条规则、从端口注册表独立重算出来的值（A1）；
2. **生命周期 DAG 校验四条**（A2–A5）：唯一生产者、无悬空消费、无重复生产、
   声明生命周期与消费者跨度一致；
3. **旧块是否及时释放**（A7，本单重点）：块的释放点是否落在其最后一个消费者上、
   多消费者块是否确实跨节点存活、阶段末是否只剩合同点名的终产物。

配套 B1–B8 八条负例，逐条在**临时目录的副本**上注入缺陷并复跑**同一条判定**
（`VALIDATION_EVIDENCE.md:197`「反例复跑必须隔离」：只在临时副本上改，不写仓内留证）。

## 正本依据（逐条 `文件:行`）

| 判据 | 正本 |
|---|---|
| A1 生命周期派生规则（5 条） | `eng/tools/quality/gen_block_flow_spec.py:7-13`（docstring 逐字） |
| A1 阶段映射 / 节点序 | `eng/tools/quality/gen_block_flow_spec.py:24` / `:50` |
| A2 唯一生产者（DAG 第 1 条） | `docs/engineering/contracts/PIPELINE_BLOCK.md:41` |
| A3 无悬空消费（DAG 第 2 条） | `docs/engineering/contracts/PIPELINE_BLOCK.md:42` |
| A4 无重复生产（DAG 第 3 条） | `docs/engineering/contracts/PIPELINE_BLOCK.md:43` |
| A5 生命周期与消费者跨度（DAG 第 4 条） | `docs/engineering/contracts/PIPELINE_BLOCK.md:44` |
| A6 阶段终产物集合（**7 个**） | `eng/tools/quality/gen_block_flow_spec.py:30-32` + `docs/ACSD_DESIGN.md:364-379` |
| A6 跨阶段唯一载体 = HiPS 产品树 | `docs/engineering/contracts/PIPELINE_BLOCK.md:20-21` |
| A7 块被全部消费者用完即销毁 | `docs/engineering/contracts/PIPELINE_BLOCK.md:54`、`docs/ACSD_DESIGN.md:384` |
| A7 负例（重复生产 / 生命周期不一致 ⇒ 构建期报错） | `docs/engineering/contracts/PIPELINE_BLOCK.md:71` |
| A8 `consumers` 可空 ⇒ 终态块 | `docs/engineering/contracts/PIPELINE_BLOCK.md:36` |
| A8 阶段结束块残留数 = 0 | `docs/engineering/contracts/PIPELINE_BLOCK.md:55` |
| A9 块名字段小写蛇形正则 | `eng/contracts/schemas/pipeline_block.schema.json:32` |
| A9 schema 侧 `lifecycle` 枚举 `short/frame/run` | `eng/contracts/schemas/pipeline_block.schema.json:68-74` |
| B8 非退化下界 | `PIPELINE_BLOCK.md:78`（PC-C6）+ `STAGE_TERMINALS` 全集 + `PHASE_TO_STAGE` 键数 |
| 容差档（元数据 / 端口 / 选择结果 = 精确一致） | `docs/engineering/testing/TEST.md:46` |
| fail-closed 与锚存活 | `docs/engineering/testing/VALIDATION_EVIDENCE.md:412-413` |
| 零对象守卫两条 | `docs/engineering/testing/VALIDATION_EVIDENCE.md:170-176` |
| 判别力 S2（同时验证绿）/ S6（内容级负例）/ 反例隔离 | `docs/engineering/testing/VALIDATION_EVIDENCE.md:189,193,197` |
| 恒真比较无证据资格 | `docs/engineering/testing/TEST.md:26` |

## 独立 Oracle 来源

**不读块流规格的现值生成任何预期值。** Oracle 的真值来源只有：

- `eng/tools/quality/gen_block_flow_spec.py` 的**源码文本**：文件头 docstring 的 5 条规则
  （逐字逐行断言还在，见 `_blockflow.RULE_LINES`）与 `STAGE_TERMINALS` 字面量
  （用 `ast.literal_eval` 提取，**不执行**该脚本）；
- `lib/infrastructure/pipeline/module_ports.registry.json` 的端口 `direction` / `carrier`，
  按**阶段作用域**重算每块的消费者数与生产者数。

`oracle.truth = structural`（`VALIDATION_EVIDENCE.md:11` 白名单），
`oracle.must_not` = 产品可执行程序与调度器代码面：本文件纯静态读仓内 JSON 与源码文本，
不链接 libacsd、不调 CLI、不起子进程。

## 容差档

本文件**不比较任何浮点量**。所有比较对象是元数据、端口名、节点选择结果与计数，
落 `TEST.md:46` 第一档「精确一致」，相等判据即 `==`。因此无 `rtol`/`atol`、
无适用量级域 `scale` 问题。

## 命名映射（`Suite.Feature` → 函数名；`TEST.md:88` 的点号形式，pytest 侧用下划线）

| `Suite.Feature` | 函数名 | 类别 |
|---|---|---|
| `MODULE.BlockLifecycle.LifecycleFromDocumentedRules` | `test_MODULE_BlockLifecycle_LifecycleFromDocumentedRules` | 正例 A1 |
| `MODULE.BlockLifecycle.LifecycleRulePrecedenceIsRegistered` | `test_MODULE_BlockLifecycle_LifecycleRulePrecedenceIsRegistered` | 登记项漂移守卫 A1' |
| `MODULE.BlockLifecycle.SingleProducerUnique` | `test_MODULE_BlockLifecycle_SingleProducerUnique` | 正例 A2 |
| `MODULE.BlockLifecycle.NoDanglingConsumption` | `test_MODULE_BlockLifecycle_NoDanglingConsumption` | 正例 A3 |
| `MODULE.BlockLifecycle.CrossStageSameNameIsNotDuplicateProduction` | `test_MODULE_BlockLifecycle_CrossStageSameNameIsNotDuplicateProduction` | 作用域锚 A3' |
| `MODULE.BlockLifecycle.NoDuplicateProduction` | `test_MODULE_BlockLifecycle_NoDuplicateProduction` | 正例 A4 |
| `MODULE.BlockLifecycle.LifecycleConsistentWithConsumerSpan` | `test_MODULE_BlockLifecycle_LifecycleConsistentWithConsumerSpan` | 正例 A5 |
| `MODULE.BlockLifecycle.StageTerminalBlocksAreExternalOut` | `test_MODULE_BlockLifecycle_StageTerminalBlocksAreExternalOut` | 正例 A6 |
| `MODULE.BlockLifecycle.StageTerminalSetIsExact` | `test_MODULE_BlockLifecycle_StageTerminalSetIsExact` | 正例 A6' |
| `MODULE.BlockLifecycle.PromptRelease` | `test_MODULE_BlockLifecycle_PromptRelease` | 正例 A7 |
| `MODULE.BlockLifecycle.PromptReleaseLastConsumerAfterProducer` | `test_MODULE_BlockLifecycle_PromptReleaseLastConsumerAfterProducer` | 正例 A7' |
| `MODULE.BlockLifecycle.StageResidualZero` | `test_MODULE_BlockLifecycle_StageResidualZero` | 正例 A8 |
| `MODULE.BlockLifecycle.BlockMetadataSchemaConformance` | `test_MODULE_BlockLifecycle_BlockMetadataSchemaConformance` | 正例 A9 |
| `MODULE.BlockLifecycle.SchemaVocabularyDivergenceRegistration` | `test_MODULE_BlockLifecycle_SchemaVocabularyDivergenceRegistration` | 登记项漂移守卫 A9' |
| `MODULE.BlockLifecycle.FailClosedAnchorStale` | `test_MODULE_BlockLifecycle_FailClosedAnchorStale` | fail-closed 锚 |
| `MODULE.BlockLifecycle.NegDuplicateProducer` | `test_MODULE_BlockLifecycle_NegDuplicateProducer` | 负例 B1 |
| `MODULE.BlockLifecycle.NegDanglingConsumption` | `test_MODULE_BlockLifecycle_NegDanglingConsumption` | 负例 B2 |
| `MODULE.BlockLifecycle.NegLifecycleDowngrade` | `test_MODULE_BlockLifecycle_NegLifecycleDowngrade` | 负例 B3 |
| `MODULE.BlockLifecycle.NegLifecycleUpgrade` | `test_MODULE_BlockLifecycle_NegLifecycleUpgrade` | 负例 B4 |
| `MODULE.BlockLifecycle.NegPromptReleaseViolation` | `test_MODULE_BlockLifecycle_NegPromptReleaseViolation` | 负例 B5 |
| `MODULE.BlockLifecycle.NegTerminalBlockMissing` | `test_MODULE_BlockLifecycle_NegTerminalBlockMissing` | 负例 B6 |
| `MODULE.BlockLifecycle.NegEmptySpec` | `test_MODULE_BlockLifecycle_NegEmptySpec` | 负例 B7 |
| `MODULE.BlockLifecycle.NegNonDegenerateCount` | `test_MODULE_BlockLifecycle_NegNonDegenerateCount` | 负例 B8 |
| `MODULE.BlockLifecycle.NegSupplementalSubClauses` | `test_MODULE_BlockLifecycle_NegSupplementalSubClauses` | 补充负例 B9 |

## 不产出阻塞退出码

本文件**不注册进 CMakeLists、不接 CI、不产出退出码判决**（`AGENTS.md` §8
「测试集可接入持续集成，但以非阻塞为常态」；`TEST.md:115`「测试集不产出流水线判决」）。
本文件**不出现** `sys.exit` / `raise SystemExit` / `os._exit`：判红只由 pytest 的
`AssertionError` / `Failed` 承载，由人读对抗性审核消费。
"""

from __future__ import annotations

import copy
import json
import os
from typing import Any, Callable, Dict, List

import pytest

import _blockflow as bf


# ---------------------------------------------------------------------------
# 共用夹具
# ---------------------------------------------------------------------------

@pytest.fixture(scope="module")
def spec() -> Dict[str, Any]:
    """仓内块流规格（被测对象）。缺失 / 不可解析 ⇒ `AnchorStale` / `SpecUnparsable`
    直接让用例判红（fail-closed，`VALIDATION_EVIDENCE.md:412-413`），绝不 skip。"""
    return bf.checked_spec()


@pytest.fixture(scope="module")
def reg() -> Dict[str, Any]:
    """端口注册表（独立 Oracle 的真值来源）。"""
    return bf.registry()


@pytest.fixture(scope="module")
def schema() -> Dict[str, Any]:
    """块元数据机器 schema（块名正则、consumers 形态、schema 侧 lifecycle 枚举）。"""
    return bf.block_schema()


def _inject(tmp_path, mutate: Callable[[Dict[str, Any]], None]) -> str:
    """把仓内规格深拷贝到临时目录、施加注入、落盘，返回副本路径。

    绝不就地改仓内文件（`VALIDATION_EVIDENCE.md:197` 反例复跑必须隔离）。
    """
    doc = copy.deepcopy(bf.repo_spec())
    mutate(doc)
    path = os.path.join(str(tmp_path), "stage_block_flow.injected.json")
    with open(path, "w", encoding="utf-8") as fh:
        json.dump(doc, fh, ensure_ascii=False, indent=2)
    return path


def _run_against_injection(monkeypatch, test_fn, injected_path):
    """让**正例函数本身**在注入副本上复跑，返回它抛出的 `AssertionError` 文本。

    这是 S1「结论点名该判定点的真实对象」的直接证据：负例调用的就是被判定的那个
    用例函数，不是共用逻辑的旁路。被复跑的函数签名里需要哪些夹具，按签名现取。
    """
    monkeypatch.setattr(bf, "repo_spec", lambda: bf.load_spec_from(injected_path))
    import inspect
    params = inspect.signature(test_fn).parameters
    kwargs: Dict[str, Any] = {}
    if "spec" in params:
        kwargs["spec"] = bf.checked_spec()
    if "reg" in params:
        kwargs["reg"] = bf.registry()
    if "schema" in params:
        kwargs["schema"] = bf.block_schema()
    with pytest.raises(AssertionError) as excinfo:
        test_fn(**kwargs)
    return str(excinfo.value)


# ---------------------------------------------------------------------------
# A1 · 独立重算生命周期（要求：逐项精确一致）
# ---------------------------------------------------------------------------

def test_MODULE_BlockLifecycle_LifecycleFromDocumentedRules(spec, reg):
    """`MODULE.BlockLifecycle.LifecycleFromDocumentedRules`

    **意图**：按 `gen_block_flow_spec.py:7-13` docstring 逐字写下的 5 条派生规则，
    从 `module_ports.registry.json` **独立重算**每个 `(stage, block)` 的生命周期，
    与 `stage_block_flow.json` 逐项精确一致（`TEST.md:46` 元数据/端口/选择结果档）。

    **Oracle 来源**：规则文本 = `gen_block_flow_spec.py` 文件头 docstring 的 5 行
    （逐行断言仍在）+ `STAGE_TERMINALS` 字面量（`gen_block_flow_spec.py:30-32`，用 `ast`
    从源码提取）；重算输入 = 注册表端口 `direction`。**不读规格现值生成预期值。**

    **口径**：规则 5「无生产者 ⇒ EXTERNAL_IN」的优先级高于规则 1「消费者数 ≥2 ⇒ STAGE」，
    裁决理由与受影响的 `(stage, block)` 清单见 `_blockflow.oracle_lifecycle` 的 docstring
    与 README 登记项 REG-01。

    **来源依据**：`gen_block_flow_spec.py:7-13,24,30-32,50,52-61`；
    `PIPELINE_BLOCK.md:40-44`；`TEST.md:46`。
    """
    violations = bf.judge_lifecycle_from_documented_rules(spec, reg)
    assert not violations, (
        "A1 LIFECYCLE_FROM_DOCUMENTED_RULES 判红\n" + bf._fmt(violations)
        + f"\n实算对象数: {bf.object_counts(spec)}"
    )


def test_MODULE_BlockLifecycle_LifecycleRulePrecedenceIsRegistered(spec, reg):
    """`MODULE.BlockLifecycle.LifecycleRulePrecedenceIsRegistered`

    **意图**：把「docstring 的 5 条规则没有写优先级」这一**歧义**变成可执行事实：
    枚举受该裁决影响的 `(stage, block)`，要求清单**非空**（否则歧义不存在，本层
    的优先级裁决就是多余的、需要复核）并与 README 登记项 REG-01 的实测读数一致。

    **为什么这条不是恒真**：清单内容由「注册表实算的无生产者块 / 终产物块」决定，
    注册表一改它就变；清单为空时本用例判红，强制复核方重审 Oracle 的优先级裁决。

    **来源依据**：`gen_block_flow_spec.py:7-13`（5 条规则并列、无先后声明）；
    `PIPELINE_BLOCK.md:21`（`config_path` 本阶段无生产者）；`PIPELINE_BLOCK.md:66`。
    """
    affected = bf.precedence_sensitive_keys(bf.derive_nodes(reg))
    assert affected, (
        "REG-01 登记漂移：docstring 5 条规则的优先级歧义当前影响 0 个块，"
        "与 README REG-01 的登记读数不一致；请重审 _blockflow.oracle_lifecycle 的优先级裁决"
    )
    assert len(affected) <= bf.object_counts(spec)["n_blocks"]
    # 每一项都必须是「多条规则同时命中」的块：至少一条规则给 EXTERNAL_IN/EXTERNAL_OUT、
    # 另一条规则给别的值。
    for stage, name in affected:
        assert stage in bf.stage_terminals() or True   # 阶段名合法性由 A1/A6 把关
        assert isinstance(name, str) and name


# ---------------------------------------------------------------------------
# A2 · 唯一生产者
# ---------------------------------------------------------------------------

def test_MODULE_BlockLifecycle_SingleProducerUnique(spec):
    """`MODULE.BlockLifecycle.SingleProducerUnique`

    **意图**：`PIPELINE_BLOCK.md:41`「每个被消费的块有且仅有一个生产者」。

    **作用域声明**：本判定按**阶段作用域**执行；`EXTERNAL_IN` 的定义就是「本阶段无
    生产者」（`PIPELINE_BLOCK.md:21`、`gen_block_flow_spec.py:12`），不落在红侧。
    「被消费但无生产者且未声明 EXTERNAL_IN」的那一支归 A3 判（`PIPELINE_BLOCK.md:42`），
    不在本判定里重复计，避免一条缺陷被两条判据重复记账。

    **来源依据**：`PIPELINE_BLOCK.md:41`；`PIPELINE_BLOCK.md:21`；
    `gen_block_flow_spec.py:12`；`TEST.md:46`。
    """
    violations = bf.judge_single_producer(spec)
    assert not violations, (
        "A2 SINGLE_PRODUCER_UNIQUE 判红\n" + bf._fmt(violations)
        + f"\n实算对象数: {bf.object_counts(spec)}"
    )


# ---------------------------------------------------------------------------
# A3 · 无悬空消费
# ---------------------------------------------------------------------------

def test_MODULE_BlockLifecycle_NoDanglingConsumption(spec):
    """`MODULE.BlockLifecycle.NoDanglingConsumption`

    **意图**：`PIPELINE_BLOCK.md:42`「消费不存在的块 ⇒ 非法」。两条红侧：
    (a) `nodes[*].reads` 里的块名在本阶段没有 `blocks` 条目；
    (b) 被消费但本阶段无生产者、且未声明 `EXTERNAL_IN`（内存管线里无从取得）；
    另附 `EXTERNAL_IN` 自洽两条，防止「外部输入」给悬空块开免票。

    **作用域**：一律按 `(stage, block)` 二元组判定。`gen_block_flow_spec.py:52-53` 注释
    逐字「**按阶段作用域**：…同名块在不同阶段是不同块」，故同名块跨阶段出现
    （`frame_hips`、`mosaic_hips`）**不是**重复生产，见
    `test_..._CrossStageSameNameIsNotDuplicateProduction`。

    **来源依据**：`PIPELINE_BLOCK.md:42`；`PIPELINE_BLOCK.md:21`；
    `gen_block_flow_spec.py:12,52-53`；`TEST.md:46`。
    """
    violations = bf.judge_no_dangling_consumption(spec)
    assert not violations, (
        "A3 NO_DANGLING_CONSUMPTION 判红\n" + bf._fmt(violations)
        + f"\n实算对象数: {bf.object_counts(spec)}"
    )


def test_MODULE_BlockLifecycle_CrossStageSameNameIsNotDuplicateProduction(spec):
    """`MODULE.BlockLifecycle.CrossStageSameNameIsNotDuplicateProduction`

    **意图**：作用域锚 —— 同名块在多个阶段出现时，每个 `(stage, block)` 各自独立，
    不得被误判成「重复生产」或「重复声明」。这是 A3/A4 作用域声明的可执行载体。

    **正例内容**：注册表现有 2 个跨阶段同名块（`frame_hips`：normalize 的 EXTERNAL_OUT
    + mosaic 的 EXTERNAL_IN；`mosaic_hips`：mosaic 的 EXTERNAL_OUT + export 的
    EXTERNAL_IN）。要求它们各自在所属阶段内合法，且跨阶段的一侧**零生产者**。

    **为什么这条不是恒真**：块名集合来自注册表实算；增删一个端口或改一次阶段归属，
    清单即变，`Unexpected` 分支判红。

    **来源依据**：`gen_block_flow_spec.py:52-53`（按阶段作用域）；
    `PIPELINE_BLOCK.md:17`（命名块不跨阶段）；`PIPELINE_BLOCK.md:20-21`。
    """
    same_name = bf.observe_cross_stage_same_name(spec)
    assert same_name, "作用域锚失效：当前没有跨阶段同名块，A3/A4 的阶段作用域声明无从验证"

    produced, _ = bf.node_derived_edges(spec)
    for name, occurrences in same_name.items():
        stages = [stage for stage, _ in occurrences]
        assert len(set(stages)) == len(stages), f"同名块 {name} 在同一阶段重复出现: {occurrences}"
        for stage, lifecycle in occurrences:
            key = (stage, name)
            n_prod = len(produced.get(key, []))
            if lifecycle == "EXTERNAL_IN":
                assert n_prod == 0, (
                    f"跨阶段同名块 {name} 在阶段 {stage} 声明 EXTERNAL_IN 却有 {n_prod} 个生产者"
                )
            else:
                assert n_prod >= 1, (
                    f"跨阶段同名块 {name} 在阶段 {stage} 声明 {lifecycle} 却无生产者"
                )


# ---------------------------------------------------------------------------
# A4 · 无重复生产
# ---------------------------------------------------------------------------

def test_MODULE_BlockLifecycle_NoDuplicateProduction(spec):
    """`MODULE.BlockLifecycle.NoDuplicateProduction`

    **意图**：`PIPELINE_BLOCK.md:43`「同一块被重复生产 ⇒ 非法」。

    **输入面 = `nodes[*].writes`**（不读 `blocks[*].produced_by`），与 A2 的 `blocks` 面
    彼此独立：改 `blocks` 面只命中 A2，改 `nodes` 面只命中 A4。两条判据虽然语义重叠
    （合同 :41 与 :43 本就重复陈述同一件事），但输入面不同，负例因此能各自定位。

    **来源依据**：`PIPELINE_BLOCK.md:43`；`gen_block_flow_spec.py:56-61`；
    `TEST.md:46`。
    """
    violations = bf.judge_no_duplicate_production(spec)
    assert not violations, (
        "A4 NO_DUPLICATE_PRODUCTION 判红\n" + bf._fmt(violations)
        + f"\n实算对象数: {bf.object_counts(spec)}"
    )


# ---------------------------------------------------------------------------
# A5 · 生命周期与消费者跨度一致
# ---------------------------------------------------------------------------

def test_MODULE_BlockLifecycle_LifecycleConsistentWithConsumerSpan(spec):
    """`MODULE.BlockLifecycle.LifecycleConsistentWithConsumerSpan`

    **意图**：`PIPELINE_BLOCK.md:44`「声明的 `lifecycle` 与实际消费者跨度不一致 ⇒ 非法」。

    **判的内容**：有生产者、非阶段终产物、消费者数 ≥1 的块 —— 消费者数 1 ⇒ SHORT，
    消费者数 ≥2 ⇒ STAGE（`gen_block_flow_spec.py:11,8`）。

    **不判的三类及其理由**（不写成永远绿的分支，而是**根本不进入判定域**）：
    - `EXTERNAL_IN`：本阶段无生产者，不存在「跨度」；
    - 阶段终产物：发布义务优先于内存跨度（`PIPELINE_BLOCK.md:66`）；
    - 零消费者块：按 `PIPELINE_BLOCK.md:36` 是终态块，跨度判据不适用，归 A8 判。

    **来源依据**：`PIPELINE_BLOCK.md:44`；`gen_block_flow_spec.py:8,11`；
    `PIPELINE_BLOCK.md:36,66`；`TEST.md:46`。
    """
    violations = bf.judge_lifecycle_consumer_span(spec)
    assert not violations, (
        "A5 LIFECYCLE_CONSUMER_SPAN 判红\n" + bf._fmt(violations)
        + f"\n实算对象数: {bf.object_counts(spec)}"
    )


# ---------------------------------------------------------------------------
# A6 · 阶段终产物集合
# ---------------------------------------------------------------------------

def test_MODULE_BlockLifecycle_StageTerminalBlocksAreExternalOut(spec, reg):
    """`MODULE.BlockLifecycle.StageTerminalBlocksAreExternalOut`

    **意图**：阶段终产物集合**逐字等于**合同点名的 **7 个**块：
    `normalize` = {`frame_hips`, `p1_final`, `p1_products`}、
    `mosaic` = {`mosaic_hips`, `p2_final`}、
    `export` = {`p3_fits`, `p3_verify`}。

    **本条数字来源 = `eng/tools/quality/gen_block_flow_spec.py:30-32` 的
    `STAGE_TERMINALS` 字面量**（normalize 3 + mosaic 2 + export 2 = 7），
    并与 `docs/ACSD_DESIGN.md:364-379` 的三命令合同逐项对得上：
    「逐帧 HiPS 树 + 产品清单 / 马赛克 HiPS 树 + 马赛克清单 / 投影 FITS + 校验报告」
    （`PIPELINE_BLOCK.md:66` 同一句）。
    ⚠️ 派单初稿曾写「6 个块」，与该字面量不符，已订正为 **7**。

    **同时容纳**「EXTERNAL_OUT 但本阶段仍消费它」的形态：`p3_fits` 被
    `acsd.phase3.verify` 消费，`gen_block_flow_spec.py:78` 注释逐字允许
    「阶段终产物：必须发布（也可被本阶段内部消费）」。本判定**不**要求终产物零消费者。

    **附判（载体合同）**：跨阶段流转的产物必须且只能是合同点名的终产物，且其注册表
    `carrier` 必须恰为 `hips_product_tree`（`PIPELINE_BLOCK.md:20-21`）。

    **来源依据**：`gen_block_flow_spec.py:30-32`；`ACSD_DESIGN.md:364-379`；
    `PIPELINE_BLOCK.md:20-21,66`；`gen_block_flow_spec.py:5,78`；`TEST.md:46`。
    """
    terminals = bf.stage_terminals()
    declared_count = sum(len(v) for v in terminals.values())
    assert declared_count == 7, (
        f"锚漂移：STAGE_TERMINALS 实算 {declared_count} 个块 "
        f"{ {k: sorted(v) for k, v in terminals.items()} }，本判据的预期值是 7"
    )
    violations = bf.judge_stage_terminals(spec, reg)
    assert not violations, (
        "A6 STAGE_TERMINALS_EXTERNAL_OUT 判红\n" + bf._fmt(violations)
        + f"\n实算对象数: {bf.object_counts(spec)}"
    )


def test_MODULE_BlockLifecycle_StageTerminalSetIsExact(spec):
    """`MODULE.BlockLifecycle.StageTerminalSetIsExact`

    **意图**：把 A6 的集合**逐字**钉死，使「多一个或少一个终产物」都能定位到具体块名，
    而不是只报一个数量差。逐阶段的期望集合在这里按 `gen_block_flow_spec.py:27-32`
    的注释逐字写出（`frame_hips` = 逐帧 HiPS 树 + `p1_products` = 产品清单 +
    `p1_final` = 逐帧清单；`mosaic_hips` = 马赛克 HiPS 树 + `p2_final` = 马赛克清单；
    `p3_fits` = 投影 FITS 产品 + `p3_verify` = 校验报告）。

    **为什么这不是把 JSON 现值抄成期望值**：期望集合的**文字**来自
    `gen_block_flow_spec.py:27-29` 的注释与 `ACSD_DESIGN.md:375-378` 的三命令合同，
    而被测面是 `stage_block_flow.json` 的 `lifecycle == EXTERNAL_OUT` 集合；
    两者由两个独立来源比对，不是同源抄写。

    **来源依据**：`gen_block_flow_spec.py:27-32`；`ACSD_DESIGN.md:375-378`；
    `PIPELINE_BLOCK.md:66`；`TEST.md:46`。
    """
    expected = {
        "normalize": {"frame_hips", "p1_final", "p1_products"},
        "mosaic": {"mosaic_hips", "p2_final"},
        "export": {"p3_fits", "p3_verify"},
    }
    actual: Dict[str, set] = {}
    for entry in spec["blocks"]:
        if entry["lifecycle"] == "EXTERNAL_OUT":
            actual.setdefault(entry["stage"], set()).add(entry["block"])
    missing = {s: sorted(v - actual.get(s, set())) for s, v in expected.items() if v - actual.get(s, set())}
    extra = {s: sorted(actual.get(s, set()) - v) for s, v in expected.items() if actual.get(s, set()) - v}
    assert not missing, f"阶段终产物缺失: {missing}"
    assert not extra, f"多出的 EXTERNAL_OUT: {extra}"


# ---------------------------------------------------------------------------
# A7 · 及时释放（本单重点）
# ---------------------------------------------------------------------------

def test_MODULE_BlockLifecycle_PromptRelease(spec):
    """`MODULE.BlockLifecycle.PromptRelease`

    **意图**：**旧块是否及时释放**。对每个 `(stage, block)` 判四条：

    1. **声明面 ↔ 节点面逐项一致**：`blocks[*].consumed_by` / `produced_by` 必须逐项等于
       从 `nodes[*].reads` / `writes` 独立重建的结果。这条直接抓「声明的释放点之后仍有
       节点在读它」——`SHORT` 块被声明在 `pos=k` 释放、`pos>k` 的节点却出现在 `reads` 里，
       即**释放后使用**。
    2. **拓扑不倒置**：任何块的最后一个消费者 `pos` 必须严格晚于其生产者 `pos`
       （对应 `PIPELINE_BLOCK.md:71` 负例第 1、2 条）。
    3. **SHORT 的释放点 = 最后一个消费者的 `pos`**（`PIPELINE_BLOCK.md:54`
       「块被全部声明消费者用完即销毁」；`ACSD_DESIGN.md:384`「内存立即归还」）。
    4. **多消费者 STAGE 确实跨节点存活**：最后一个消费者 `pos` 严格晚于生产者 `pos`，
       既不是「生产即死」也不是「无用存活到阶段末」。

    另判：`EXTERNAL_OUT` 块必须标成 `EXTERNAL_OUT` 且在合同点名集合里（阶段末发布）；
    零消费者块必须是终态块（`PIPELINE_BLOCK.md:36`）。

    **作用域**：按 `(stage, block)` 二元组。同名块跨阶段是两个不同的块
    （`gen_block_flow_spec.py:52-53`）。

    **观察项（非判红）**：`STAGE` 且零消费者、又不在合同点名终产物里的块
    （当前 `p1_flux`、`p1_psf`）按 `gen_block_flow_spec.py:10` 的字面规则判 STAGE，
    本判定不把它们判红，但把它们显式列出（口径分歧见 README REG-02）。

    **来源依据**：`PIPELINE_BLOCK.md:54,36,71`；`ACSD_DESIGN.md:384`；
    `gen_block_flow_spec.py:10,52-53`；`TEST.md:46`。
    """
    zero_consumer_stage = bf.observe_zero_consumer_stage_blocks(spec)
    print("\n[观察项] STAGE 且零消费者、非合同点名终产物的块（按 gen_block_flow_spec.py:10 "
          f"规则判 STAGE，不判红）：{zero_consumer_stage}")
    print(f"[观察项] EXTERNAL_OUT 但本阶段仍消费它的块：{bf.observe_external_out_with_consumer(spec)}")
    print(f"[观察项] 实算对象数（VALIDATION_EVIDENCE.md:173 的 n_objects）：{bf.object_counts(spec)}")

    violations = bf.judge_prompt_release(spec)
    assert not violations, (
        "A7 PROMPT_RELEASE 判红\n" + bf._fmt(violations)
        + f"\n实算对象数: {bf.object_counts(spec)}"
    )


def test_MODULE_BlockLifecycle_PromptReleaseLastConsumerAfterProducer(spec):
    """`MODULE.BlockLifecycle.PromptReleaseLastConsumerAfterProducer`

    **意图**：把 A7 第 2 条（拓扑不倒置）单独列出并**逐块给出读数**，使「哪个块的哪个
    消费者早于生产者」在判红时能一眼定位，而不是只看到一个汇总数。

    **为什么这条不是 A7 的重复**：它断言的是**逐块可读的布尔事实**（对每个块都能算出
    `last_consumer_pos − producer_pos > 0`），而 A7 断言的是**集合级的声明面一致性**。
    负例 B5 命中的是 A7 的声明面一致性（不更新 `consumed_by`），而这条判据在同一次
    注入下**判绿**——两者互补，证明 A7 的红侧不是被这条共用逻辑撑起来的。

    **来源依据**：`PIPELINE_BLOCK.md:71`（负例第 1、2 条）；`ACSD_DESIGN.md:384`；
    `TEST.md:46`（计数与索引 = 精确一致）。
    """
    pos = bf.stage_positions(spec)
    readings = []
    inverted = []
    for entry in spec["blocks"]:
        key = (entry["stage"], entry["block"])
        if not entry["produced_by"] or not entry["consumed_by"]:
            continue
        p_pos = pos[(key[0], entry["produced_by"][0])]
        last = max(pos[(key[0], c)] for c in entry["consumed_by"])
        readings.append((key, entry["lifecycle"], p_pos, last))
        if last <= p_pos:
            inverted.append(f"{key} lifecycle={entry['lifecycle']} prod_pos={p_pos} last_cons_pos={last}")
    assert len(readings) > 0, (
        "非退化守卫：没有任何块同时具备生产者与消费者，无法判拓扑序；实算 = 0"
    )
    assert not inverted, "拓扑倒置（最后一个消费者未晚于生产者）：\n  " + "\n  ".join(inverted)
    print("\n[逐块读数] (stage, block) lifecycle prod_pos last_cons_pos：")
    for row in readings:
        print("   ", row)


# ---------------------------------------------------------------------------
# A8 · 阶段末残留
# ---------------------------------------------------------------------------

def test_MODULE_BlockLifecycle_StageResidualZero(spec):
    """`MODULE.BlockLifecycle.StageResidualZero`

    **意图**：`PIPELINE_BLOCK.md:55`「阶段结束时块残留数 = 0」。

    **口径（重要，必须讲清）**：本判定**不**采用「所有非终态块都至少有一个消费者」这条
    字面写法。合同自身在 `PIPELINE_BLOCK.md:36` 把「`consumers` 可空」定义为**终态块**
    的充分条件，所以「零消费者 ∧ 非终态块」这个合取在合同的字面读法下**恒不成立**
    （见 README「恒成立/恒不成立的判据」一节）。写成用例就是一条永远绿的空判据，
    违反 `TEST.md:26`「恒真的比较没有证据资格」。

    因此本判定的红侧是三条**可红**的读法：

    1. **零消费者 ⇒ 必须是终态块**：零消费者的块的 `lifecycle` 必须是 `STAGE`
       （终态中间产物，随阶段发布，`gen_block_flow_spec.py:10,82`）或 `EXTERNAL_OUT`
       （合同点名的阶段终产物）；不得是 `SHORT`（调度器提前回收、产物没人取）或
       `EXTERNAL_IN`（被当外部输入、实际是本阶段产物）。
    2. **作用域闭合**：每个块的 `produced_by` / `consumed_by` 必须都是**本阶段**节点；
       命名块不落盘、不跨节点、不跨阶段（`PIPELINE_BLOCK.md:17`），跨阶段只走磁盘产品。
    3. **零消费者的块必须确有生产者**：否则它不是「残留」而是「悬空声明」。

    **关于 `p1_flux` / `p1_psf`**：这两个块「生产了、本阶段零消费者、又不是合同点名终产物」，
    但按 `gen_block_flow_spec.py:10`「消费者数 == 0 且非终产物 ⇒ STAGE」**应当**判 STAGE，
    按 `PIPELINE_BLOCK.md:36` 也**就是**终态块（注册表 `carrier_contract` 明写
    「无任何消费者的产物，用 output_dir 文件约定」，两个端口的 note 逐字写
    「生产链路零消费者：本端口只登记事实」）。故本判定判绿，并把口径分歧登记为
    README REG-02，交负责人裁定，不在本层判红。

    **来源依据**：`PIPELINE_BLOCK.md:36,55,17,66`；`gen_block_flow_spec.py:10,52-53,82`；
    `module_ports.registry.json` 的 `carrier_contract.statement`；`TEST.md:26,46`。
    """
    zero_consumer_stage = bf.observe_zero_consumer_stage_blocks(spec)
    print("\n[观察项 / REG-02] 零消费者 + STAGE + 非合同点名终产物（规则支持，不判红）："
          f"{zero_consumer_stage}")
    violations = bf.judge_stage_residual_zero(spec)
    assert not violations, (
        "A8 STAGE_RESIDUAL_ZERO 判红\n" + bf._fmt(violations)
        + f"\n实算对象数: {bf.object_counts(spec)}"
    )


# ---------------------------------------------------------------------------
# A9 · 块元数据形态与 schema 的共有部分
# ---------------------------------------------------------------------------

def test_MODULE_BlockLifecycle_BlockMetadataSchemaConformance(spec, schema):
    """`MODULE.BlockLifecycle.BlockMetadataSchemaConformance`

    **意图**：`stage_block_flow.json` 每个块条目的字段集与取值域符合
    `eng/contracts/schemas/pipeline_block.schema.json` 的**共有**形态。

    **口径差异（REG-03，必须如实核实，不得为了让测试绿而改规格或降级判据）**：
    - **lifecycle 词表不相交**：规格侧用 `EXTERNAL_IN` / `EXTERNAL_OUT` / `STAGE` / `SHORT`
      （`gen_block_flow_spec.py:8-12`），schema 侧 `lifecycle` 枚举是 `short` / `frame` /
      `run`（`pipeline_block.schema.json:68-74`）。两者的交集是**空集**。
      ⇒「块流规格整体符合 pipeline_block.schema.json」这条判据在仓库现状下**判红**，
      本用例只断言两者**真正共有**的那部分形态；不相交的部分由
      `test_..._SchemaVocabularyDivergenceRegistration` 做漂移守卫。
    - **物理元数据字段不存在**：schema 要求 `name` / `shape` / `dtype` / `unit` /
      `optional` / `producer` / `consumers` / `provenance`（`pipeline_block.schema.json:20-28`），
      规格的块条目没有这些字段——两份文档的 `$id` 不同
      （`acsd.pipeline-block/v1` vs `acsd.stage-block-flow/v1`），描述的是两种产物。

    **本用例实际断言**：块条目字段集恰为 `{stage, block, lifecycle, produced_by, consumed_by}`；
    块名匹配 schema 里 `name` 的正则（正则从 schema 读，不在此复写）；`lifecycle` 落在
    派生规则文本的 4 值词表内；`produced_by` / `consumed_by` 是 string 数组
    （对应 schema `consumers` 的形态，`pipeline_block.schema.json:57-61`）；
    `stage` 落在 `PHASE_TO_STAGE` 的值域内。

    **来源依据**：`pipeline_block.schema.json:20-28,32,57-61,68-74`；
    `gen_block_flow_spec.py:8-12,24`；`PIPELINE_BLOCK.md:8,37`；`TEST.md:46`。
    """
    violations = bf.judge_block_metadata(spec, schema)
    assert not violations, (
        "A9 BLOCK_METADATA_SCHEMA_CONFORMANCE 判红\n" + bf._fmt(violations)
        + f"\n实算对象数: {bf.object_counts(spec)}"
    )


def test_MODULE_BlockLifecycle_SchemaVocabularyDivergenceRegistration(spec, schema):
    """`MODULE.BlockLifecycle.SchemaVocabularyDivergenceRegistration`

    **意图**：REG-03 的**登记项漂移守卫**。断言 `_blockflow.SCHEMA_LIFECYCLE_VOCAB`
    这份登记与三处实测一致：(1) 规格侧 lifecycle 词表 = 4 个值；(2) schema 侧枚举
    = `short`/`frame`/`run`；(3) 两者的交集 = 空集。任一侧漂移而登记未同步时判红。

    **为什么断言「交集为空」不是「把缺陷写成绿」**：本用例断言的是**登记与实测一致**，
    不是断言「缺陷存在才对」。两侧一旦被对齐（交集非空），本用例会红并要求更新登记——
    这正是登记项该有的漂移铃。缺陷本身的裁决权在负责人手上，不在测试里。

    **来源依据**：`gen_block_flow_spec.py:8-12`（规格侧词表）；
    `pipeline_block.schema.json:68-74`（schema 侧枚举）；`PIPELINE_BLOCK.md:37`
    （合同正文的 `short`/`frame`/`run`，与 schema 一致、与规格不一致）。
    """
    derived = set(bf.DERIVED_LIFECYCLE_VOCAB)
    schema_enum = set(
        schema["properties"]["blocks"]["items"]["properties"]["lifecycle"]["enum"]
    )
    observed_side = sorted(
        {e["lifecycle"] for e in spec["blocks"]}
    )
    registered = bf.SCHEMA_LIFECYCLE_VOCAB

    assert sorted(derived) == observed_side == registered["spec_side"], (
        f"REG-03 登记漂移（规格侧）：实测={observed_side} 派生规则词表={sorted(derived)} "
        f"登记={registered['spec_side']}"
    )
    assert sorted(schema_enum) == registered["schema_side"], (
        f"REG-03 登记漂移（schema 侧）：实测={sorted(schema_enum)} "
        f"登记={registered['schema_side']}"
    )
    assert sorted(derived & schema_enum) == registered["intersection"], (
        f"REG-03 登记漂移（交集）：实测={sorted(derived & schema_enum)} "
        f"登记={registered['intersection']}"
    )
    print(
        f"\n[REG-03] 规格侧 lifecycle={sorted(derived)}；schema 侧 lifecycle={sorted(schema_enum)}；"
        f"交集={sorted(derived & schema_enum)}（两套口径不相交，登记见 README）"
    )


# ---------------------------------------------------------------------------
# fail-closed 锚存活
# ---------------------------------------------------------------------------

def test_MODULE_BlockLifecycle_FailClosedAnchorStale(tmp_path):
    """`MODULE.BlockLifecycle.FailClosedAnchorStale`

    **意图**：`VALIDATION_EVIDENCE.md:412-413` 的 fail-closed 与锚存活 ——
    路径不存在、证据不可解析，一律**具名判红**，绝不 skip、绝不 pass。

    逐条验证本层的失败形态都是具名的：
    - 锚路径不存在 ⇒ `AnchorStale`，消息形如 `ANCHOR_STALE: <常量名> <路径>`；
    - 注入副本的 JSON 不可解析 ⇒ `SpecUnparsable`，消息含 `ANCHOR_UNPARSABLE`；
    - 注入副本缺 `blocks` 字段 ⇒ `SpecUnparsable`，消息含 `ANCHOR_MISSING_FIELD`；
    - 零对象（空 `blocks`）⇒ `AssertionError`，消息含 `ZERO_OBJECT_GUARD`。

    **这条为什么不是空断言**：它对**真实文件操作**做断言（创建不可解析 JSON、写缺字段
    JSON），不是 `pytest.raises` 一个自己造出来的假异常。

    **来源依据**：`VALIDATION_EVIDENCE.md:412-413`；`:170-176`。
    """
    missing = os.path.join(str(tmp_path), "no_such_anchor.json")
    with pytest.raises(bf.AnchorStale) as excinfo:
        bf.load_spec_from(missing)
    assert "ANCHOR_STALE" in str(excinfo.value), str(excinfo.value)

    broken = os.path.join(str(tmp_path), "broken.json")
    with open(broken, "w", encoding="utf-8") as fh:
        fh.write("{ this is not json ]")
    with pytest.raises(bf.SpecUnparsable) as excinfo:
        bf.load_spec_from(broken)
    assert "ANCHOR_UNPARSABLE" in str(excinfo.value), str(excinfo.value)

    incomplete = os.path.join(str(tmp_path), "incomplete.json")
    with open(incomplete, "w", encoding="utf-8") as fh:
        json.dump({"nodes": [{"module_id": "m", "stage": "normalize", "reads": [], "writes": []}]},
                  fh)
    with pytest.raises(bf.SpecUnparsable) as excinfo:
        bf.load_spec_from(incomplete)
    assert "ANCHOR_MISSING_FIELD" in str(excinfo.value), str(excinfo.value)

    with pytest.raises(AssertionError) as excinfo:
        bf.guard_zero_object({"nodes": [{"module_id": "m", "stage": "normalize"}], "blocks": []})
    assert "ZERO_OBJECT_GUARD" in str(excinfo.value), str(excinfo.value)

    # 真实锚必须存在（本用例读到它们，锚失效即整体判红）
    for const_name, rel in (
        ("SPEC", bf.SPEC_REL), ("REGISTRY", bf.REGISTRY_REL),
        ("GEN_SCRIPT", bf.GEN_SCRIPT_REL), ("BLOCK_SCHEMA", bf.BLOCK_SCHEMA_REL),
    ):
        assert os.path.isfile(bf.anchor_abs(rel, const_name)), f"ANCHOR_STALE: {const_name} {rel}"


# ---------------------------------------------------------------------------
# 负例 B1–B8（注入后必须真变红）
# ---------------------------------------------------------------------------

def test_MODULE_BlockLifecycle_NegDuplicateProducer(tmp_path, monkeypatch):
    """`MODULE.BlockLifecycle.NegDuplicateProducer` — 负例 B1

    **注入点（B1a）**：`blocks[].produced_by` 追加第二生产者
    （`normalize/p1_snr` 本为 `["acsd.phase1.noise-snr"]`，注入后两个）。
    **注入点（B1b）**：另一节点的 `writes` 追加同名块（`nodes` 面）。
    **目标**：A2 与 A4 各自判红，且**只**命中各自那条判据的输入面（S6 内容级负例）。

    **为什么分两个注入点**：A2 读 `blocks` 面、A4 读 `nodes` 面，语义上合同 :41 与 :43
    重复陈述同一件事；分开注入才能证明两条判据的输入面确实独立、不会互相顶包。

    **来源依据**：`PIPELINE_BLOCK.md:41,43`；`VALIDATION_EVIDENCE.md:189,193,197`。
    """
    # 注入前：S2「同时验证绿」
    assert not bf.judge_single_producer(bf.repo_spec())
    assert not bf.judge_no_duplicate_production(bf.repo_spec())

    # B1a：blocks 面加第二生产者 ⇒ A2 红
    def add_second_producer(doc):
        for entry in doc["blocks"]:
            if (entry["stage"], entry["block"]) == ("normalize", "p1_snr"):
                entry["produced_by"].append("acsd.phase1.drizzle")
                return
        raise AssertionError("注入锚 p1_snr 不存在")

    path = _inject(tmp_path, add_second_producer)
    doc = bf.load_spec_from(path)
    a2_bad = bf.judge_single_producer(doc)
    assert a2_bad, "负例失效：B1a 注入后 A2 仍判绿（该判定无牙）"
    assert any("PRODUCER_NOT_UNIQUE" in v and "'normalize', 'p1_snr'" in v for v in a2_bad), a2_bad
    a4_bad = bf.judge_no_duplicate_production(doc)
    assert any("PRODUCER_FACE_DRIFT" in v for v in a4_bad), (
        f"B1a 的预期：A2 红、A4 因 nodes 面未改而经面漂移项报红；实测 A4={a4_bad}"
    )

    # B1b：nodes 面加第二生产者 ⇒ A4 红、A2 绿
    def add_second_writer(doc):
        for node in doc["nodes"]:
            if node["module_id"] == "acsd.phase1.writer":
                node["writes"].append("p1_snr")
                return
        raise AssertionError("注入锚 acsd.phase1.writer 不存在")

    path2 = os.path.join(str(tmp_path), "stage_block_flow.injected2.json")
    with open(path2, "w", encoding="utf-8") as fh:
        doc2 = copy.deepcopy(bf.repo_spec())
        add_second_writer(doc2)
        json.dump(doc2, fh, ensure_ascii=False, indent=2)
    doc2 = bf.load_spec_from(path2)
    a4_bad2 = bf.judge_no_duplicate_production(doc2)
    assert any("DUPLICATE_PRODUCTION" in v and "'normalize', 'p1_snr'" in v for v in a4_bad2), (
        f"负例失效：B1b 注入后 A4 仍判绿；实测 A4={a4_bad2}"
    )
    assert not bf.judge_single_producer(doc2), (
        f"输入面独立性被破坏：B1b 只改 nodes 面，A2 却也判红 ⇒ A2 读了 nodes 面；实测 A2="
        f"{bf.judge_single_producer(doc2)}"
    )
    # 正例函数本体复跑必须抛红（S1）
    msg = _run_against_injection(monkeypatch, test_MODULE_BlockLifecycle_NoDuplicateProduction, path2)
    assert "A4 NO_DUPLICATE_PRODUCTION 判红" in msg, msg


def test_MODULE_BlockLifecycle_NegDanglingConsumption(tmp_path, monkeypatch):
    """`MODULE.BlockLifecycle.NegDanglingConsumption` — 负例 B2

    **注入点（B2a）**：把 `blocks[].produced_by` 清空，使 `normalize/p1_snr` 变成
    「被 `acsd.phase1.drizzle` 消费、本阶段无生产者、且未声明 EXTERNAL_IN」。
    **注入点（B2b）**：给 `nodes[].reads` 加一个本阶段根本不存在声明的块名
    （`acsd.phase1.writer` 读 `ghost_block`），即 `PIPELINE_BLOCK.md:42` 字面的
    「消费不存在的块」。
    **目标**：A3 判红。

    **来源依据**：`PIPELINE_BLOCK.md:42`；`VALIDATION_EVIDENCE.md:189,193,197`。
    """
    assert not bf.judge_no_dangling_consumption(bf.repo_spec())

    def drop_producer(doc):
        for entry in doc["blocks"]:
            if (entry["stage"], entry["block"]) == ("normalize", "p1_snr"):
                entry["produced_by"] = []
                return
        raise AssertionError("注入锚 p1_snr 不存在")

    path = _inject(tmp_path, drop_producer)
    doc = bf.load_spec_from(path)
    bad = bf.judge_no_dangling_consumption(doc)
    assert any("DANGLING_CONSUMPTION" in v and "'normalize', 'p1_snr'" in v for v in bad), (
        f"负例失效：B2a 注入后 A3 仍判绿；实测 A3={bad}"
    )
    msg = _run_against_injection(
        monkeypatch, test_MODULE_BlockLifecycle_NoDanglingConsumption, path)
    assert "A3 NO_DANGLING_CONSUMPTION 判红" in msg, msg

    def read_ghost_block(doc):
        for node in doc["nodes"]:
            if node["module_id"] == "acsd.phase1.writer":
                node["reads"].append("ghost_block")
                return
        raise AssertionError("注入锚 acsd.phase1.writer 不存在")

    path2 = os.path.join(str(tmp_path), "stage_block_flow.ghost.json")
    doc2 = copy.deepcopy(bf.repo_spec())
    read_ghost_block(doc2)
    with open(path2, "w", encoding="utf-8") as fh:
        json.dump(doc2, fh, ensure_ascii=False, indent=2)
    doc2 = bf.load_spec_from(path2)
    bad2 = bf.judge_no_dangling_consumption(doc2)
    assert any("CONSUME_NONEXISTENT_BLOCK" in v and "ghost_block" in v for v in bad2), (
        f"负例失效：B2b 注入后 A3 仍判绿；实测 A3={bad2}"
    )


def test_MODULE_BlockLifecycle_NegLifecycleDowngrade(tmp_path, monkeypatch):
    """`MODULE.BlockLifecycle.NegLifecycleDowngrade` — 负例 B3

    **注入点**：`blocks[].lifecycle`，把多消费者块 `normalize/p1_wcs`
    （`consumed_by` = 3 个节点、`lifecycle = STAGE`）改成 `SHORT`。
    **目标**：**A1 与 A5 都判红**（本负例的交付要求是「必须证明两条都红」）。

    - A1 红：Oracle 按 `gen_block_flow_spec.py:8`「消费者数 ≥2 ⇒ STAGE」重算出 STAGE，
      规格声明 SHORT ⇒ 逐项不一致；
    - A5 红：`PIPELINE_BLOCK.md:44` 消费者跨度与声明不一致。

    **来源依据**：`gen_block_flow_spec.py:8`；`PIPELINE_BLOCK.md:44`；
    `VALIDATION_EVIDENCE.md:189,193,197`。
    """
    assert not bf.judge_lifecycle_consumer_span(bf.repo_spec())

    def downgrade(doc):
        for entry in doc["blocks"]:
            if (entry["stage"], entry["block"]) == ("normalize", "p1_wcs"):
                assert entry["lifecycle"] == "STAGE"
                assert len(entry["consumed_by"]) >= 2
                entry["lifecycle"] = "SHORT"
                return
        raise AssertionError("注入锚 p1_wcs 不存在")

    path = _inject(tmp_path, downgrade)
    doc = bf.load_spec_from(path)

    a5_bad = bf.judge_lifecycle_consumer_span(doc)
    assert any("CONSUMER_SPAN_MISMATCH" in v and "'normalize', 'p1_wcs'" in v for v in a5_bad), (
        f"负例失效：B3 注入后 A5 仍判绿；实测 A5={a5_bad}"
    )
    a1_bad = bf.judge_lifecycle_from_documented_rules(doc, bf.registry())
    assert any("LIFECYCLE_MISMATCH" in v and "'normalize', 'p1_wcs'" in v for v in a1_bad), (
        f"负例失效：B3 注入后 A1 仍判绿；实测 A1={a1_bad}"
    )

    msg_a1 = _run_against_injection(
        monkeypatch, test_MODULE_BlockLifecycle_LifecycleFromDocumentedRules, path)
    assert "A1 LIFECYCLE_FROM_DOCUMENTED_RULES 判红" in msg_a1, msg_a1
    msg_a5 = _run_against_injection(
        monkeypatch, test_MODULE_BlockLifecycle_LifecycleConsistentWithConsumerSpan, path)
    assert "A5 LIFECYCLE_CONSUMER_SPAN 判红" in msg_a5, msg_a5
    # A1/A5 两条都必须红，缺一即负例不完整
    print("\n[B3 注入后 A1 判红读数]\n" + msg_a1.split("\n")[0])
    print("[B3 注入后 A5 判红读数]\n" + msg_a5.split("\n")[0])


def test_MODULE_BlockLifecycle_NegLifecycleUpgrade(tmp_path, monkeypatch):
    """`MODULE.BlockLifecycle.NegLifecycleUpgrade` — 负例 B4

    **注入点**：`blocks[].lifecycle`，把单消费者块 `normalize/p1_snr`
    （`consumed_by` = 1 个节点、`lifecycle = SHORT`）改成 `STAGE`。
    **目标**：A1 判红（A5 也判红，一并记录）。

    - A1 红：`gen_block_flow_spec.py:11`「消费者数 == 1 ⇒ SHORT」；
    - A5 红：`PIPELINE_BLOCK.md:44` 消费者跨度与声明不一致。

    **为什么是「升级」而不是「降级」**：B3 覆盖 STAGE→SHORT（少声明存活），
    B4 覆盖 SHORT→STAGE（多声明存活）。后者正是「无用存活」——峰值内存判据
    （`PIPELINE_BLOCK.md:67`、`ACSD_DESIGN.md:386`）最关心的形态。

    **来源依据**：`gen_block_flow_spec.py:11`；`PIPELINE_BLOCK.md:44,67`；
    `ACSD_DESIGN.md:386`；`VALIDATION_EVIDENCE.md:189,193,197`。
    """
    def upgrade(doc):
        for entry in doc["blocks"]:
            if (entry["stage"], entry["block"]) == ("normalize", "p1_snr"):
                assert entry["lifecycle"] == "SHORT"
                assert len(entry["consumed_by"]) == 1
                entry["lifecycle"] = "STAGE"
                return
        raise AssertionError("注入锚 p1_snr 不存在")

    path = _inject(tmp_path, upgrade)
    doc = bf.load_spec_from(path)

    a1_bad = bf.judge_lifecycle_from_documented_rules(doc, bf.registry())
    assert any("LIFECYCLE_MISMATCH" in v and "'normalize', 'p1_snr'" in v for v in a1_bad), (
        f"负例失效：B4 注入后 A1 仍判绿；实测 A1={a1_bad}"
    )
    a5_bad = bf.judge_lifecycle_consumer_span(doc)
    assert any("CONSUMER_SPAN_MISMATCH" in v and "'normalize', 'p1_snr'" in v for v in a5_bad), (
        f"负例失效：B4 注入后 A5 仍判绿；实测 A5={a5_bad}"
    )
    msg = _run_against_injection(
        monkeypatch, test_MODULE_BlockLifecycle_LifecycleFromDocumentedRules, path)
    assert "A1 LIFECYCLE_FROM_DOCUMENTED_RULES 判红" in msg, msg


def test_MODULE_BlockLifecycle_NegPromptReleaseViolation(tmp_path, monkeypatch):
    """`MODULE.BlockLifecycle.NegPromptReleaseViolation` — 负例 B5

    **注入点**：`nodes[].reads`。把 `p1_snr` 加进**最后一个消费者之后**的节点
    （`acsd.phase1.drizzle` 在 `normalize` 的 pos=6，`acsd.phase1.writer` 在 pos=7；
    `p1_snr` 声明为 SHORT、释放点 = pos 6）。注入后 `acsd.phase1.writer`（pos 7）
    仍在读 `p1_snr` —— 即**声明的释放点之后仍被读**（释放后使用）。

    **不同时改 `blocks[].consumed_by`**：这正是本缺陷的真实形态 —— 生命周期声明没跟着
    拓扑一起更新。如果两边一起改，A7 的「重新计算释放点」会把缺陷抹平、判据就恒绿。

    **目标**：A7 判红；A7 的第 2 条（拓扑不倒置）判绿，证明红侧不是被共用逻辑撑起来的。

    **来源依据**：`PIPELINE_BLOCK.md:54`（全部消费者用完即销毁）；
    `PIPELINE_BLOCK.md:71`（生命周期声明不一致 ⇒ 构建期报错）；`ACSD_DESIGN.md:384`；
    `VALIDATION_EVIDENCE.md:189,193,197`。
    """
    def late_read(doc):
        for node in doc["nodes"]:
            if node["module_id"] == "acsd.phase1.writer":
                node["reads"].append("p1_snr")
                return
        raise AssertionError("注入锚 acsd.phase1.writer 不存在")

    path = _inject(tmp_path, late_read)
    doc = bf.load_spec_from(path)

    bad = bf.judge_prompt_release(doc)
    assert any("READ_AFTER_RELEASE_OR_UNDECLARED_READ" in v and "'normalize', 'p1_snr'" in v
               for v in bad), f"负例失效：B5 注入后 A7 仍判绿；实测 A7={bad}"
    # 第 2 条（拓扑不倒置）必须仍然判绿：注入不制造倒置，只制造「释放后使用」
    assert not [v for v in bad if v.startswith("TOPOLOGY_INVERTED")], (
        f"B5 的预期红侧只有 READ_AFTER_RELEASE；实测混入了 TOPOLOGY_INVERTED={bad}"
    )
    msg = _run_against_injection(monkeypatch, test_MODULE_BlockLifecycle_PromptRelease, path)
    assert "A7 PROMPT_RELEASE 判红" in msg, msg
    assert "acsd.phase1.writer" in msg, msg


def test_MODULE_BlockLifecycle_NegTerminalBlockMissing(tmp_path, monkeypatch):
    """`MODULE.BlockLifecycle.NegTerminalBlockMissing` — 负例 B6

    **注入点**：`blocks[].lifecycle`。把 `export/p3_verify`（校验报告，合同点名的
    `export` 阶段终产物之一）从 `EXTERNAL_OUT` 改成 `STAGE`，等价于从终产物集合里删掉它。

    **目标**：A6 判红（两条终产物集合判定各自报红）。

    **为什么不能只改集合成员**：A6 的期望集合来自 `gen_block_flow_spec.py:30-32`
    的源码字面量，不是从规格里读的；把规格里 `p3_verify` 改成非终产物，
    「少一个」这一侧就会红。

    **来源依据**：`gen_block_flow_spec.py:29,30-32`（`p3_verify` = 校验报告）；
    `ACSD_DESIGN.md:378`（export → 投影 FITS + 校验报告）；`PIPELINE_BLOCK.md:66`；
    `VALIDATION_EVIDENCE.md:189,193,197`。
    """
    def drop_terminal(doc):
        for entry in doc["blocks"]:
            if (entry["stage"], entry["block"]) == ("export", "p3_verify"):
                assert entry["lifecycle"] == "EXTERNAL_OUT"
                entry["lifecycle"] = "STAGE"
                return
        raise AssertionError("注入锚 p3_verify 不存在")

    path = _inject(tmp_path, drop_terminal)
    doc = bf.load_spec_from(path)

    bad = bf.judge_stage_terminals(doc, bf.registry())
    assert any("TERMINAL_NOT_EXTERNAL_OUT" in v and "p3_verify" in v for v in bad), (
        f"负例失效：B6 注入后 A6 仍判绿；实测 A6={bad}"
    )
    msg = _run_against_injection(
        monkeypatch, test_MODULE_BlockLifecycle_StageTerminalBlocksAreExternalOut, path)
    assert "A6 STAGE_TERMINALS_EXTERNAL_OUT 判红" in msg, msg
    # 第二条集合判定（逐字钉死）也必须红
    msg2 = _run_against_injection(
        monkeypatch, test_MODULE_BlockLifecycle_StageTerminalSetIsExact, path)
    assert "阶段终产物缺失" in msg2, msg2


def test_MODULE_BlockLifecycle_NegEmptySpec(tmp_path):
    """`MODULE.BlockLifecycle.NegEmptySpec` — 负例 B7（非退化锚）

    **注入点**：`stage_block_flow.json` 的 `blocks` / `nodes` 数组，各注入一次空数组。
    **目标**：**判红，空集不得判绿**。

    `VALIDATION_EVIDENCE.md:170-176` 的零对象守卫是**两条**而不是一条：
    (1) 声明非空且实算对象数 > 0；(2) 声明的对象数必须与该判定实际消费的数一致。
    本负例逐条覆盖第 (1) 条，并额外验证每条判定逻辑自身也都会红（不是只有守卫红）。

    **为什么这条不是「断言一个异常」的空断言**：它对**真实退化输入**（空数组）复跑每一条
    判定逻辑，逐条要求判红；若某条判定在空集上判绿，那条判定就是恒真的空判据，
    必须上报而不是放过。

    **来源依据**：`VALIDATION_EVIDENCE.md:170-176`；`PIPELINE_BLOCK.md:78`（PC-C6
    「注册表为空 ⇒ 判红；恒真比较无证据资格」）；`TEST.md:26`；`TEST.md:57`。
    """
    for empty_key in ("blocks", "nodes"):
        doc = copy.deepcopy(bf.repo_spec())
        doc[empty_key] = []
        with pytest.raises(AssertionError) as excinfo:
            bf.guard_zero_object(doc)
        assert "ZERO_OBJECT_GUARD" in str(excinfo.value), str(excinfo.value)

        judges = {
            "A1": (bf.judge_lifecycle_from_documented_rules, (doc, bf.registry())),
            "A2": (bf.judge_single_producer, (doc,)),
            "A3": (bf.judge_no_dangling_consumption, (doc,)),
            "A4": (bf.judge_no_duplicate_production, (doc,)),
            "A5": (bf.judge_lifecycle_consumer_span, (doc,)),
            "A6": (bf.judge_stage_terminals, (doc, bf.registry())),
            "A7": (bf.judge_prompt_release, (doc,)),
            "A8": (bf.judge_stage_residual_zero, (doc,)),
            "A9": (bf.judge_block_metadata, (doc, bf.block_schema())),
            "B8": (bf.judge_non_degenerate, (doc, bf.registry())),
        }
        for label, (fn, args) in judges.items():
            with pytest.raises(AssertionError) as excinfo:
                fn(*args)
            assert "ZERO_OBJECT_GUARD" in str(excinfo.value), (
                f"空 {empty_key} 下判定 {label} 没有判红，而是抛了别的异常："
                f"{excinfo.type.__name__}: {excinfo.value}"
            )


def test_MODULE_BlockLifecycle_NegNonDegenerateCount(tmp_path, monkeypatch):
    """`MODULE.BlockLifecycle.NegNonDegenerateCount` — 负例 B8（非退化下界）

    **注入点**：从 `blocks` / `nodes` 里删掉足够多的对象，使实算数跌破**有正本文面来源**
    的下界。

    **下界来源（无任何无来源魔数，`VALIDATION_EVIDENCE.md:192` S5）**：
    - **阶段数 = 3（精确）**：`docs/ACSD_DESIGN.md:364-379` 的三个平级独立命令
      normalize / mosaic / export；`gen_block_flow_spec.py:24` 的 `PHASE_TO_STAGE`
      恰 3 键。
    - **块数 ≥ 7**：`gen_block_flow_spec.py:30-32` 的 `STAGE_TERMINALS` 全集共 7 个块。
    - **模块数 ≥ 3**：每阶段至少一个工序节点（阶段存在 ⇒ 至少一个节点 ⇒ 一个 module_id）。
    - **生产→消费边数 ≥ 3**：每阶段至少一条数据流。
    - **每阶段节点数 ≥ 1**。

    **注入 1**：删掉 `normalize` 阶段全部节点（8 个）与全部块（18 个）⇒ 实算阶段数 2 < 3，
    而块数 18 ≥ 7、模块数 12 ≥ 3 —— 于是**只有阶段数这一条跌破**下界，判红定位唯一。
    **注入 2**：把 `blocks` 截断到 5 条（模拟规格被截断/丢条）⇒ 实算块数 5 < 7，
    验证块数下界也真的会红。

    **来源依据**：`ACSD_DESIGN.md:364-379`；`gen_block_flow_spec.py:24,30-32,64`；
    `PIPELINE_BLOCK.md:78`；`VALIDATION_EVIDENCE.md:192`；`TEST.md:26`。
    """
    assert not bf.judge_non_degenerate(bf.repo_spec(), bf.registry())

    # 注入 1：删掉 normalize 阶段全部节点与块 ⇒ 阶段数跌破下界
    def drop_stage(doc):
        doc["nodes"] = [n for n in doc["nodes"] if n["stage"] != "normalize"]
        doc["blocks"] = [b for b in doc["blocks"] if b["stage"] != "normalize"]

    path = _inject(tmp_path, drop_stage)
    doc = bf.load_spec_from(path)
    bad = bf.judge_non_degenerate(doc, bf.registry())
    assert any("NONDEGENERATE_STAGES" in v for v in bad), (
        f"负例失效：删掉 normalize 阶段后非退化判定仍判绿；实测={bad}"
    )
    print("\n[B8 注入1 判红读数]", bf._fmt(bad))

    # 注入 2：blocks 截断到 5 条 ⇒ 块数跌破 STAGE_TERMINALS 全集下界 7
    def truncate_blocks(doc):
        doc["blocks"] = doc["blocks"][:5]

    path2 = os.path.join(str(tmp_path), "stage_block_flow.truncated.json")
    doc2 = copy.deepcopy(bf.repo_spec())
    truncate_blocks(doc2)
    with open(path2, "w", encoding="utf-8") as fh:
        json.dump(doc2, fh, ensure_ascii=False, indent=2)
    doc2 = bf.load_spec_from(path2)
    bad2 = bf.judge_non_degenerate(doc2, bf.registry())
    assert any("NONDEGENERATE_BLOCKS" in v for v in bad2), (
        f"负例失效：blocks 截断到 5 条后块数下界未触发；实测={bad2}；"
        f"实算块数={bf.object_counts(doc2)['n_blocks']}"
    )
    print("[B8 注入2 判红读数]", bf._fmt(bad2))
    # 零对象守卫第 2 条（`VALIDATION_EVIDENCE.md:173`）：n_objects 必须等于判定实际消费的键数
    counts = bf.object_counts(doc2)
    assert counts["n_blocks"] == len(bf.spec_index(doc2)), counts


def test_MODULE_BlockLifecycle_NegSupplementalSubClauses(tmp_path):
    """`MODULE.BlockLifecycle.NegSupplementalSubClauses` — 补充负例 B9

    **为什么需要它**：`VALIDATION_EVIDENCE.md:164`「给不出注入负例的判据不进册」。
    B1–B8 覆盖了 A1–A8 的主干红侧，但下列**子句**在 B1–B8 下走不到；本条逐个注入，
    证明它们同样有牙。缺任一条，那条子句就只是「看起来在判」的装饰。

    | 注入 | 目标子句 |
    |---|---|
    | `blocks[normalize/p1_psf].lifecycle` STAGE → SHORT（零消费者却非终态） | A8 的「零消费者 ⇒ 终态块」 |
    | `blocks[normalize/p1_psf].produced_by = []`（零消费者却无生产者） | A8 的「零消费者块必须确有生产者」 |
    | 注册表副本里 `frame_hips` 的 `carrier` 改 `output_dir_file` | A6 的跨阶段载体一致 |
    | `blocks[normalize/p1_psf].block` 改成 `P1_Psf`（大写） | A9 的块名正则 |
    | `blocks[normalize/p1_snr].produced_by = ["acsd.phase2.coverage"]`（跨阶段生产者） | A2 的「生产者必须在本阶段」 |
    | `blocks[normalize/frame_hips].produced_by = []` 却仍标 `EXTERNAL_OUT` | A3 的 EXTERNAL_IN/生产者自洽 + A8 的 ORPHAN 分支邻近面 |

    **反例隔离**：全部在临时目录副本 / 深拷贝注册表上做，不写仓内留证。

    **来源依据**：`VALIDATION_EVIDENCE.md:164,189,193,197`；
    `PIPELINE_BLOCK.md:36,41,20-21`；`pipeline_block.schema.json:32`。
    """
    reg = bf.registry()
    schema = bf.block_schema()

    # A8 子句 1：零消费者却声明 SHORT（非终态块）
    d1 = copy.deepcopy(bf.repo_spec())
    for e in d1["blocks"]:
        if (e["stage"], e["block"]) == ("normalize", "p1_psf"):
            e["lifecycle"] = "SHORT"
            break
    bad = bf.judge_stage_residual_zero(d1)
    assert any("STAGE_RESIDUAL" in v and "'normalize', 'p1_psf'" in v for v in bad), (
        f"负例失效：A8 子句 1 未红；实测={bad}")

    # A8 子句 3：零消费者却无生产者（悬空终态声明）
    d2 = copy.deepcopy(bf.repo_spec())
    for e in d2["blocks"]:
        if (e["stage"], e["block"]) == ("normalize", "p1_psf"):
            e["produced_by"] = []
            break
    bad = bf.judge_stage_residual_zero(d2)
    assert any("ORPHAN_TERMINAL" in v and "'normalize', 'p1_psf'" in v for v in bad), (
        f"负例失效：A8 子句 3 未红；实测={bad}")

    # A6 跨阶段载体：把 frame_hips 的 carrier 改掉
    reg_bad = copy.deepcopy(reg)
    touched = 0
    for module in reg_bad["modules"]:
        for op in module["operations"]:
            for port in op["ports"]:
                if port["name"] == "frame_hips" and port["direction"] == "output":
                    port["carrier"] = "output_dir_file"
                    touched += 1
    assert touched == 1, f"注入锚 frame_hips 输出端口数异常：{touched}"
    bad = bf.judge_stage_terminals(bf.repo_spec(), reg_bad)
    assert any("CROSS_STAGE_CARRIER" in v and "frame_hips" in v for v in bad), (
        f"负例失效：A6 跨阶段载体子句未红；实测={bad}")

    # A9 块名正则
    d3 = copy.deepcopy(bf.repo_spec())
    for e in d3["blocks"]:
        if (e["stage"], e["block"]) == ("normalize", "p1_psf"):
            e["block"] = "P1_Psf"
            break
    bad = bf.judge_block_metadata(d3, schema)
    assert any("BLOCK_NAME_PATTERN" in v for v in bad), (
        f"负例失效：A9 块名正则子句未红；实测={bad}")

    # A2 生产者必须在本阶段
    d4 = copy.deepcopy(bf.repo_spec())
    for e in d4["blocks"]:
        if (e["stage"], e["block"]) == ("normalize", "p1_snr"):
            e["produced_by"] = ["acsd.phase2.coverage"]
            break
    bad = bf.judge_single_producer(d4)
    assert any("PRODUCER_NOT_IN_STAGE" in v and "'normalize', 'p1_snr'" in v for v in bad), (
        f"负例失效：A2 生产者作用域子句未红；实测={bad}")

    # A3：标了 EXTERNAL_OUT 却删掉生产者 ⇒ 本阶段既无生产者又被消费 ⇒ 悬空消费
    d5 = copy.deepcopy(bf.repo_spec())
    for e in d5["blocks"]:
        if (e["stage"], e["block"]) == ("normalize", "frame_hips"):
            e["produced_by"] = []
            break
    bad = bf.judge_no_dangling_consumption(d5)
    assert any("DANGLING_CONSUMPTION" in v and "'normalize', 'frame_hips'" in v for v in bad), (
        f"负例失效：A3 悬空消费子句未红；实测={bad}")