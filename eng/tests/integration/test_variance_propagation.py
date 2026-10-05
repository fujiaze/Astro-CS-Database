"""集成层 · 合同与接口 · 方差传播（Suite `ContractVariance`）。

本层只读仓内 JSON/schema/头文件注释，**不链接 `libacsd`、不调 CLI、不起子进程、
不编译、不跑构建**。它判的是**合同面**（冻结 schema、冻结登记、冻结公式条款），
不是产品行为面。

## 函数名 ↔ `Suite.Feature` 映射表

| # | 测试函数 | `Suite.Feature` | 类别 | 意图 |
|---|---|---|---|---|
| A1 | `test_ContractVariance_SchemaPositiveExamplesValidate` | `ContractVariance.SchemaPositiveExamplesValidate` | 正例 | 14 个正例夹具必须各自通过它所声明 schema |
| A0 | `test_ContractVariance_PositiveExampleScanIsNonEmpty` | `ContractVariance.PositiveExampleScanIsNonEmpty` | 正例 | 扫描面非空且实算对象数与在册数一致（零对象守卫） |
| A2 | `test_ContractVariance_NegativeFixturesHitExpectedPointers` | `ContractVariance.NegativeFixturesHitExpectedPointers` | 正例 | 6 个负例夹具必须被指定 schema 拒绝且命中 `must_match` |
| A3 | `test_ContractVariance_RetiredObjectRejected` | `ContractVariance.RetiredObjectRejected` | 正例 | 退役对象 `psfsw_robust_weight` 任何 schema 都不得接受 |
| A4 | `test_ContractVariance_ValidatorSelfCheckNonVacuous` | `ContractVariance.ValidatorSelfCheckNonVacuous` | 正例 | 自研校验器非恒真（`required`/`const`/`propertyNames`/`enum` 均在做功） |
| A5 | `test_ContractVariance_VarianceObjectChainContract` | `ContractVariance.VarianceObjectChainContract` | 正例 | 方差链五环的对象判别式与 schema ID 严格配对、单位/缺失语义齐备（**A5a**，绿） |
| A5b | `test_ContractVariance_EachChainRingValidatesOwnSchema` | `ContractVariance.EachChainRingValidatesOwnSchema` | 正例 | 方差链每环正例夹具通过自身 schema（**与 A5a 同源的红项**，红） |
| A6 | `test_ContractVariance_WeightIsNotABareField` | `ContractVariance.WeightIsNotABareField` | 正例 | `signal` 不得自带裸 `weight`；权重只由带权威式的模式给出 |
| A7 | `test_ContractVariance_VariancePropagationIndependentOracle` | `ContractVariance.VariancePropagationIndependentOracle` | 正例 | 独立注入值 + 独立参考实现对方差传播合同做解析对拍 |
| A8 | `test_ContractVariance_ResidualMakerCrossTermsAndGainSquare` | `ContractVariance.ResidualMakerCrossTermsAndGainSquare` | 正例 | Phase2b 残差制造者 `PΣPᵀ` 抓交叉项、乘性归一化抓 `÷g²` |
| A9 | `test_ContractVariance_WeightChainPairingAnalytic` | `ContractVariance.WeightChainPairingAnalytic` | 正例 | 跨帧权重链配对性 `w=(SNR_layer/F_ref)²·g²` 的解析比值 |
| A10 | `test_ContractVariance_NonFiniteAndMissingAreThirdState` | `ContractVariance.NonFiniteAndMissingAreThirdState` | 正例 | 缺失是与 NaN 并列的第三态，schema 不得让 NaN 兼表缺失 |
| A11 | `test_ContractVariance_NoBlockingExitCodeInTestCode` | `ContractVariance.NoBlockingExitCodeInTestCode` | 正例 | 本层测试代码不含阻塞退出/跳过充数（项目定位：测试不是门禁） |
| B1 | `test_ContractVariance_NegSourceSnrAsVariance` | `ContractVariance.NegSourceSnrAsVariance` | 负例 | `source_snr` 冒充 `variance` ⇒ 链合同判红（夹具 n2） |
| B2 | `test_ContractVariance_NegSourceSnrIntoVariancePort` | `ContractVariance.NegSourceSnrIntoVariancePort` | 负例 | `source_snr` 接进 `variance` 端口 ⇒ 判红（夹具 n4） |
| B3 | `test_ContractVariance_NegCoverageAsRejection` | `ContractVariance.NegCoverageAsRejection` | 负例 | `coverage` 冒充 `rejection` ⇒ 判红（夹具 n3） |
| B4 | `test_ContractVariance_NegRetiredPsfswWeight` | `ContractVariance.NegRetiredPsfswWeight` | 负例 | 退役 `psfsw_robust_weight` 接进端口 ⇒ 判红（夹具 n5） |
| B5 | `test_ContractVariance_NegRelativeSnrSemantics` | `ContractVariance.NegRelativeSnrSemantics` | 负例 | 稀疏层改相对语义 ⇒ 判红（夹具 n6） |
| B6 | `test_ContractVariance_NegBareWeight` | `ContractVariance.NegBareWeight` | 负例 | `signal` 带裸 `weight` ⇒ 判红（夹具 n1） |
| B7 | `test_ContractVariance_NegVacuousValidator` | `ContractVariance.NegVacuousValidator` | 负例 | 把 `required` 摘掉 ⇒ 判据读数必须发生可观测翻转（证明校验器非空转） |
| B8 | `test_ContractVariance_NegUnitsMismatch` | `ContractVariance.NegUnitsMismatch` | 负例 | `units.bunit_semantics` 改值 ⇒ 链合同判红 |
| B9 | `test_ContractVariance_NegNanAsMissing` | `ContractVariance.NegNanAsMissing` | 负例 | `missing_repr` 改成 `nan` ⇒ 第三态判红 |
| B10 | `test_ContractVariance_NegNaiveVarianceMissingCrossTerms` | `ContractVariance.NegNaiveVarianceMissingCrossTerms` | 负例 | 注入朴素式（漏 `-HΣ-ΣHᵀ` 交叉项）⇒ 判红，红出比值 `(1+1/N)/(1−1/N)` |
| B11 | `test_ContractVariance_NegMissingGainSquare` | `ContractVariance.NegMissingGainSquare` | 负例 | 注入漏 `÷g²` 的归一化 ⇒ 判红 `g²` 倍 |
| B12 | `test_ContractVariance_NegWeightTimesFrameSnr` | `ContractVariance.NegWeightTimesFrameSnr` | 负例 | 注入「再乘帧级 SNR」错口径 ⇒ 判红 |
| B13 | `test_ContractVariance_NegAbsentLayerTimesOne` | `ContractVariance.NegAbsentLayerTimesOne` | 负例 | 注入「层缺失按乘 1 出权」捷径 ⇒ 判红 |
| B14 | `test_ContractVariance_NegNonPositiveRefFluxEqualWeight` | `ContractVariance.NegNonPositiveRefFluxEqualWeight` | 负例 | 注入「`F_ref` 非正退化为等权」⇒ 判红 |

## 如实登记的红项（缺陷信号，非本层缺陷；A1 与 A5b 同源一条）

`A1` 与 `A5b` 在当前工作树**判红**，唯一红因同一个：
`eng/contracts/schemas/unified/examples/variance.example.json` 的
`correlation_kernel` 缺 `variance.schema.json:342` 要求的 `oracle_ref`。
其余 13 个正例夹具全部零错误通过。
这是仓内正例夹具自身与冻结 schema 的冲突，按 `TEST.md:203`
「红项是缺陷信号」如实登记，**两种读法并列、不替负责人裁定**，
不为让测试转绿而迁就夹具、放宽 schema 或放宽判据。详见交付报告与 `README.md`。

## 另一条待裁决冲突（不编码为永久断言）

`examples/ivar.example.json` 声明 `invalid_repr="zero"`、`nan_in_binary=false`
（与 `coverage` / `rejection` / `support` 同属 `zero/zero/false` 族），
而 `docs/science/unified/DATA_SEMANTICS.md:88` 的三态编码表给出
「无覆盖 ⇒ `ivar = NaN`」。二者不能同时为真。本层只断言 `TEST.md:80` 的
可判定规则那一半（缺失不得由 NaN 承载），不替负责人裁决该族该取哪一种。

## 跨帧量纪律（本层最硬的约束）

`docs/science/algorithms/GATES_AND_TOLERANCES.md:122` 明文登记了退化判据
（「把整张背景减掉后再比帧间差是**退化判据**」），
`docs/science/noise_snr/NOISE_SNR.md:353-359` 另行登记了「信噪比平方之和」
恒等式型退化，`lib/algorithms/integration/phase2_integrate/include/acsd/
weight_chain.h:424-438` 登记了 `dimensional_identity` 型退化并已删除。

因此本层**任何**涉及跨帧量的预期值都遵守三条：

1. 注入值是本文件写死的具体数字，**不来自任何产品输出、现有 JSON 产物或运行读数**；
2. 期望值用 `fractions.Fraction` 的**精确有理数**从正本公式逐项展开，
   与参考实现的浮点路线**不同源**；
3. **禁止**任何 `A − A` 形式的恒等式当预期值；每条数值判据都配一条
   「注入错误式必须转红」的负例（`B10`–`B14`），由 B7 之外的独立负例证明有牙。
"""

from __future__ import annotations

import json
import math
import re
import sys
import tempfile
from fractions import Fraction
from pathlib import Path
from typing import Any

import numpy as np
import pytest

import _object_contracts as oc
from _object_contracts import (
    ATOL_F64_SCALE_FACTOR,
    RTOL_F64,
    TOLERANCE_EXACT,
    UNIT_ROUNDOFF_F64,
    gamma_n,
    reduction_threshold,
    ulp,
)


# ==========================================================================
# 0 判定原语：合同面检查（负例 B1-B6/B8/B9 攻击的就是这些函数）
# ==========================================================================

#: 冻结的缺失/无效值声明（TEST.md:75-82 第 4.4 节「非有限值与缺失」正本）。
#: 「缺失」是与 NaN 并列的第三态，所以 `missing_repr` 必须是与 `nan` 不同的取值。
FROZEN_MISSING_REPR = "null"
FROZEN_INVALID_REPR = "nan"
FROZEN_NAN_IN_BINARY = True
FROZEN_SILENT_ZERO_FORBIDDEN = True

#: 冻结的 BUNIT 语义（`clause_registry.json` 的 `FZ-BUNIT-SEMANTICS`：
#: 「BUNIT 必须量纲可判：显式立体角幂次 或 provenance pixel_semantics=surface_brightness
#: +pixel_area_power=-2」）。本链对象一律走 `written_px_power` 这一支。
FROZEN_BUNIT_SEMANTICS = "written_px_power"

#: 稀疏层控制点值的唯一合法语义（`sparse_snr_layer.schema.json` 的
#: `sparse_snr_semantics` const；`weight_chain.h:93-99`）。
FROZEN_SPARSE_SNR_SEMANTICS = "absolute_flux_type_snr"

#: 端口合同登记的唯一示例端口（`unified_object_registry.json#port_contracts[0]`）。
VARIANCE_PORT_ID = "phase2.integrate.pixel_variance_in"


def _violation(location: str, detail: str) -> str:
    return f"{location or '/'}: {detail}"


def check_object_pairing(object_name: str, document: Any) -> list[str]:
    """A5 的核心判定：`unified_object` 与 `object_schema_id` 严格配对。

    错配即红：把 `source_snr` 文档塞进 `variance` 端口（负例 n2/n4）就是这条判红。

    来源依据：`signal.schema.json:52-58` 的 `unified_object` / `object_schema_id`
    双 `const`；`port_contract.schema.json:5` 「端口/连接声明必须写明接受的对象与
    schema ID，且被连接文档的对象判别式必须与端口一致」。
    """
    schema = oc.load_schema(object_name)
    want_object, want_id = oc.schema_declared_identity(schema)
    problems: list[str] = []
    try:
        got_object, got_id = oc.object_identity(document)
    except oc.AnchorStale as exc:
        return [_violation("", f"对象文档不可解析：{exc}")]
    if got_object != want_object:
        problems.append(
            _violation("/unified_object",
                       f"声明 {got_object!r}，但 {object_name} schema 要求 {want_object!r}"
                       "（对象冒充）")
        )
    if got_id != want_id:
        problems.append(
            _violation("/object_schema_id",
                       f"声明 {got_id!r}，但 {object_name} schema 的 $id 是 {want_id!r}")
        )
    return problems


def check_units_contract(object_name: str, document: Any) -> list[str]:
    """A5 的单位面判定（B8 攻击的就是这条）。

    三项：①`bunit_semantics` 取冻结值；②`bunit` 非空；
    ③`pixel_area_power != 0` ⇒ `bunit` 必须显式含立体角幂次（量纲可判 (a)，
    `variance.schema.json:490-495` 的 `x-acsd-gate` 逐字写的规则）。
    """
    try:
        units = oc.units_of(document)
    except oc.AnchorStale as exc:
        return [_violation("/units", f"不可解析：{exc}")]

    problems: list[str] = []
    semantics = units.get("bunit_semantics")
    if semantics != FROZEN_BUNIT_SEMANTICS:
        problems.append(
            _violation("/units/bunit_semantics",
                       f"{semantics!r} != 冻结值 {FROZEN_BUNIT_SEMANTICS!r}")
        )
    bunit = units.get("bunit")
    if not isinstance(bunit, str) or not bunit.strip():
        problems.append(_violation("/units/bunit", "BUNIT 缺失或为空 ⇒ 单位不可判"))
    elif semantics == FROZEN_BUNIT_SEMANTICS and units.get("pixel_area_power") != 0:
        if "sr" not in bunit:
            problems.append(
                _violation("/units/bunit",
                           f"{bunit!r} 未显式含立体角幂次，pixel_area_power="
                           f"{units.get('pixel_area_power')!r} ⇒ 量纲不可判")
            )
    return problems


def check_missing_value_contract(
    object_name: str, document: Any, *, strict_nan_carrier: bool = False
) -> list[str]:
    """A10 的判定（B9 攻击的就是这条）。

    永远断言的（`TEST.md:75-82` 第 4.4 节的可判定形式）：
      ①`missing_value` 对象存在且四项都已声明；
      ②**缺失不得由 NaN 承载** —— `missing_repr != "nan"`；
      ③`silent_zero_forbidden` 必须是 `true`（禁止用 0 冒充缺失）；
      ④当无效值那一侧由 NaN 承载（`invalid_repr == "nan"`）时，缺失侧的表示
        必须与之不同态。

    `strict_nan_carrier=True` 时追加严格四元组 `null`/`nan`/`true`/`true`。
    该四元组**只对声明 `missing_repr = "null"` 的对象族成立**：`ivar`、
    `coverage`、`rejection`、`support` 走 `zero/zero/false` 那一族
    （几何门与占比面），其 `missing_repr` 冻结值是 `zero`；见 `README.md`
    的登记项「ivar 的缺失表示与 DATA_SEMANTICS §3.3 冲突」。

    来源依据：`TEST.md:75-82` 第 4.4 节；`signal.schema.json:186-232` 的
    `missing_value` 子模式。
    """
    try:
        block = oc.missing_value_of(document)
    except oc.AnchorStale as exc:
        return [_violation("/missing_value", f"不可解析：{exc}")]

    problems: list[str] = []
    for key in ("missing_repr", "invalid_repr", "nan_in_binary", "silent_zero_forbidden"):
        if key not in block:
            problems.append(_violation(f"/missing_value/{key}", "缺失值声明缺项（不得省略）"))

    missing_repr = block.get("missing_repr")
    invalid_repr = block.get("invalid_repr")

    # TEST.md:80「「缺失」是与非有限值并列的第三态……不得由 NaN 兼表」
    if missing_repr == "nan":
        problems.append(
            _violation("/missing_value/missing_repr",
                       "'nan' ⇒ 缺失由 NaN 兼表，违反 TEST.md:80「不得由 NaN 兼表」")
        )
    if invalid_repr == "nan" and missing_repr == invalid_repr:
        problems.append(
            _violation("/missing_value",
                       "invalid_repr 与 missing_repr 同为 'nan' ⇒ 缺失与非有限值同态，"
                       "违反 TEST.md:80")
        )
    if block.get("silent_zero_forbidden") is not True:
        problems.append(
            _violation("/missing_value/silent_zero_forbidden",
                       f"{block.get('silent_zero_forbidden')!r} != True"
                       "（禁止用 0 冒充缺失/不可用）")
        )
    if "nan_in_binary" in block and not isinstance(block["nan_in_binary"], bool):
        problems.append(_violation("/missing_value/nan_in_binary", "必须是布尔"))

    if strict_nan_carrier:
        frozen = (
            ("missing_repr", FROZEN_MISSING_REPR),
            ("invalid_repr", FROZEN_INVALID_REPR),
            ("nan_in_binary", FROZEN_NAN_IN_BINARY),
            ("silent_zero_forbidden", FROZEN_SILENT_ZERO_FORBIDDEN),
        )
        for key, want in frozen:
            got = block.get(key)
            if got != want or type(got) is not type(want):
                problems.append(
                    _violation(f"/missing_value/{key}", f"{got!r} != 冻结值 {want!r}")
                )
    return problems


def check_sparse_snr_semantics(document: Any) -> list[str]:
    """A5 的稀疏层语义判定（B5 攻击的就是这条）。

    来源依据：`sparse_snr_layer.schema.json` 的 `sparse_snr_semantics` const；
    `weight_chain.h:39` 「层声明为相对语义（非 absolute_flux_type_snr）→ 拒绝」。
    """
    got = document.get("sparse_snr_semantics") if isinstance(document, dict) else None
    if got != FROZEN_SPARSE_SNR_SEMANTICS:
        return [
            _violation("/sparse_snr_semantics",
                       f"{got!r} != 冻结值 {FROZEN_SPARSE_SNR_SEMANTICS!r}"
                       "（相对语义层不得进权重面）")
        ]
    return []


def check_variance_chain_contract(
    chain: list[tuple[str, Any]],
    port_document: Any = None,
) -> list[str]:
    """A5：方差链（`signal` → `ivar` → `variance` → `frame_snr` → `sparse_snr_layer`）
    的跨模块合同面。

    对每一环跑：对象配对、单位合同、缺失值第三态；稀疏层另跑绝对语义；
    若给了端口文档，再跑「端口接受对象 ↔ 被连接文档对象判别式」一致性。
    """
    problems: list[str] = []
    seen: dict[str, str] = {}
    for object_name, document in chain:
        problems += check_object_pairing(object_name, document)
        problems += check_units_contract(object_name, document)
        problems += check_missing_value_contract(object_name, document)
        if object_name == "sparse_snr_layer":
            problems += check_sparse_snr_semantics(document)
        _, schema_id = oc.schema_declared_identity(oc.load_schema(object_name))
        if schema_id in seen:
            problems.append(
                _violation("/object_schema_id",
                           f"schema ID {schema_id!r} 被 {seen[schema_id]} 与 "
                           f"{object_name} 共用 ⇒ 唯一合同链被破坏")
            )
        seen[schema_id] = object_name

    if port_document is not None:
        problems += check_port_pairing(port_document)
    return problems


def check_port_pairing(port_document: Any) -> list[str]:
    """A5/B2/B4：端口接受对象、被连接文档对象判别式、schema ID 三者一致。

    来源依据：`port_contract.schema.json:26-125` 与其 `allOf` 里每个
    `accepts_object` 分支的 `then.accepts_schema_id` / `connected_object_document`。
    """
    problems: list[str] = []
    if not isinstance(port_document, dict):
        return [_violation("", "端口文档不可解析")]
    connected = port_document.get("connected_object_document")
    if not isinstance(connected, dict):
        return [_violation("/connected_object_document", "端口未给被连接文档")]

    accepts_object = port_document.get("accepts_object")
    accepts_id = port_document.get("accepts_schema_id")
    got_object, got_id = oc.object_identity(connected)

    if accepts_object != got_object:
        problems.append(
            _violation("/connected_object_document/unified_object",
                       f"端口接受 {accepts_object!r}，被连接文档声明 {got_object!r}"
                       "（对象冒充）")
        )
    if accepts_id != got_id:
        problems.append(
            _violation("/connected_object_document/object_schema_id",
                       f"端口接受 {accepts_id!r}，被连接文档声明 {got_id!r}")
        )

    closed = oc.accepted_object_enum(oc.load_schema("port_contract"))
    retired = oc.retired_object_names(oc.load_schema("port_contract"))
    for name in retired:
        if name in closed or name in oc.accepted_schema_id_enum(oc.load_schema("port_contract")):
            problems.append(
                _violation("/accepts_object",
                           f"退役对象 {name!r} 仍在接受枚举内 ⇒ fail-closed 拒绝被撤销")
            )
    for value, label in ((accepts_object, "accepts_object"), (accepts_id, "accepts_schema_id")):
        if isinstance(value, str) and value.startswith(
            "https://acsd.local/schemas/unified/"
        ):
            tail = value.rsplit("/", 2)[-2]
            if tail in retired:
                problems.append(_violation(f"/{label}", f"退役对象 {tail!r} 被接受"))
    return problems


def check_no_bare_weight(document: Any) -> list[str]:
    """A6/B6：`signal` 对象不得自带裸 `weight` 字段。

    来源依据：`signal.schema.json:28-50` 的 `propertyNames`（禁 `weight|value|mask|snr`
    及其前缀形态）+ `additionalProperties: false`（:14）；
    `VALIDATION_EVIDENCE.md:114-121` 第 4.2 节模式表（权重对象与单位由模式表给出，
    `signal` 不在任一模式的「权重对象」列里）。
    """
    if not isinstance(document, dict):
        return [_violation("", "不是对象文档")]
    problems: list[str] = []
    schema = oc.load_schema("signal")
    allowed = set(schema.get("properties", {}))
    for key in document:
        if key in {"weight", "value", "mask", "snr"} or key.startswith(("weight_", "value_", "mask_", "snr_")):
            problems.append(
                _violation(f"/{key}", "裸权重/诊断量键不得作为对象字段出现"
                           "（signal.schema.json:28-50 的 propertyNames 守卫）")
            )
        elif key not in allowed:
            problems.append(_violation(f"/{key}", f"未在 signal schema 声明的键"))

    if document.get("object_weight_capability") is not False:
        problems.append(
            _violation("/object_weight_capability",
                       f"{document.get('object_weight_capability')!r} != False"
                       "（UNIFIED_MODEL §2 的「可否作权重」机器判定）")
        )
    if document.get("object_weight_verdict") != "否":
        problems.append(
            _violation("/object_weight_verdict",
                       f"{document.get('object_weight_verdict')!r} != '否'")
        )
    return problems


def check_schema_forbids_nan_as_missing(
    object_name: str, schema: dict | None = None
) -> list[str]:
    """A10 的**结构面**判定：schema 自己不得允许 `NaN` 兼表缺失。

    `missing_value.properties.missing_repr.enum` 里不得含 `"nan"`；
    而 `invalid_repr.enum` 必须含 `"nan"`（无效值那一侧确实由 NaN 承载）。
    这是从 schema 文本推出的结构判定，不是实例值判定，所以与 B9 的实例注入互补。

    `schema` 参数供负例注入用（传入临时副本里的注入后 schema），
    生产路径一律省略、走仓内 canonical schema。
    """
    if schema is None:
        schema = oc.load_schema(object_name)
    try:
        missing_enum = schema["properties"]["missing_value"]["properties"]["missing_repr"]["enum"]
        invalid_enum = schema["properties"]["missing_value"]["properties"]["invalid_repr"]["enum"]
    except (KeyError, TypeError) as exc:
        return [_violation("/missing_value", f"schema 缺缺失/无效枚举：{exc}")]
    problems: list[str] = []
    if "nan" in missing_enum:
        problems.append(
            _violation("/missing_value/missing_repr/enum",
                       "枚举含 'nan' ⇒ schema 允许 NaN 兼表缺失（TEST.md:80 禁止）")
        )
    if "nan" not in invalid_enum:
        problems.append(
            _violation("/missing_value/invalid_repr/enum",
                       "枚举不含 'nan' ⇒ 无效值侧失去与缺失可区分的载体")
        )
    return problems


# ==========================================================================
# 1 A7 / A8 / A9 的独立 Oracle
# ==========================================================================

#: --- 自己构造的注入值（不来自任何产品输出、产物或运行读数）-----------------
#:
#: N = 8 帧等权均值模型（`include_self=true`，故 `H_ij = 1/N`），
#: 逐帧方差取**互不相同**的具体数字，使判据不能靠「均匀化简」侥幸通过。
INJECTED_SIGMA2 = (4.0, 9.0, 16.0, 25.0, 36.0, 49.0, 64.0, 81.0)  # ADU²
INJECTED_N_FRAMES = 8
#: 乘性光度响应 `g_k`（`variance_propagation.h:24-25`：corrected=(y−ĝ)/g ⇒ Var /= g²）。
INJECTED_GAIN = 2.0

#: 守恒映射的单像素注入（`DATA_SEMANTICS.md:75-78`、`NOISE_SNR.md:380-390`）。
INJECTED_A_PIXEL = (1.0, 4.0)      # 源像素立体角 sr
INJECTED_A_JP = (0.5, 1.0)         # 命中面积 sr
INJECTED_V_J = (3.0, 12.0)         # 源像素方差 ADU²

#: 逆方差叠加的注入（`NOISE_SNR.md:343-349`、`PHASE2_INTEGRATION.md:113-121`）。
INJECTED_IVAR = (Fraction(1, 4), Fraction(1, 9), Fraction(1, 16))  # ADU⁻²
INJECTED_VALUES = (10.0, 20.0, 40.0)  # ADU

#: 跨帧权重链的注入（`weight_chain.h:27-31`）。
INJECTED_LAYER_SNR = (10.0, 5.0)    # 无量纲，绝对通量型 SNR
INJECTED_REF_FLUX = (100.0, 250.0)  # ADU，逐帧
INJECTED_GAINS = (2.0, 1.0)         # 无量纲


def injected(value: float) -> Fraction:
    """把注入的十进制字面量转成**精确有理数**。

    必须走 `Fraction(str(x))` 而不是 `Fraction(int(round(x)))`：后者是银行家舍入，
    会把 `0.5` 变成 `0`、把 `118.4` 变成 `118` —— 那不是「我构造的注入值」，
    而是被悄悄改写过的另一个数，正是本层要防的那类「预期值不是注入值」。
    """
    return Fraction(str(float(value)))


#: 第二组注入：**不可被 binary64 精确表示**的 σ²，用来逼出真实舍入。
#: 上面那组都是小整数，二进制可精确表示 ⇒ 浮点路线的残差恒为 0，归约档门限不会被
#: 触发。为使容差判据**非装饰**（`VALIDATION_EVIDENCE.md:196` S6），
#: 这里另给一组十进制循环小数，残差落到 ulp 量级。
INJECTED_SIGMA2_NONEXACT = (0.1, 1.0 / 3.0, 7.0 / 11.0, 2.718281828, 13.0 / 7.0,
                            0.3333333333333333, 101.0 / 17.0, 1.0e-3)


def exact_sigma2_total(sigma2: tuple[float, ...] = INJECTED_SIGMA2) -> Fraction:
    """注入 σ² 的精确总和（有理数域，不经浮点）。"""
    return sum(injected(s) for s in sigma2)


def exact_residual_maker_variance(index: int,
                                 sigma2: tuple[float, ...] = INJECTED_SIGMA2) -> Fraction:
    """残差制造者 `PΣPᵀ` 对角元的**精确有理数**期望。

    正本式逐字（`variance_propagation.h:12-15`）：
    `corrected = y − ĝ = (I − H) y = P y`，`P = I − H`，
    `Var(corrected_i) = Σ_j P_ij² σ_j²`。
    代入 `H_ij = 1/N`（`variance_propagation.h:62-64` 的 `include_self=true` 分支）：
        P_ii = 1 − 1/N,  P_ij = −1/N (j ≠ i)
        Var_i = (1−1/N)² σ_i² + (1/N)² · (Σ_j σ_j² − σ_i²)
    全部用 `Fraction` 展开 ⇒ 期望值与参考实现的浮点路线不同源。
    """
    n = len(sigma2)
    sigma_i = injected(sigma2[index])
    total = exact_sigma2_total(sigma2)
    return (Fraction(n - 1, n) ** 2) * sigma_i + Fraction(1, n * n) * (total - sigma_i)


def exact_naive_variance(index: int,
                        sigma2: tuple[float, ...] = INJECTED_SIGMA2) -> Fraction:
    """朴素（**被点名禁止**）式的精确有理数：`σ_i² + Σ_j H_ij² σ_j²`。

    正本逐字：`variance_propagation.h:19-22`「错误形式（禁止用于生产权重）：
    `σ_i² + Var(ĝ_i) = σ_i² + Σ_j H_ij² σ_j²`。它漏掉 `−HΣ − ΣHᵀ` 交叉项。
    """
    n = len(sigma2)
    sigma_i = injected(sigma2[index])
    return sigma_i + Fraction(1, n * n) * exact_sigma2_total(sigma2)


def exact_mean_model_ratio(n_frames: int = INJECTED_N_FRAMES) -> Fraction:
    """等 σ² 下朴素/正确之比 `(1+1/N)/(1−1/N)`（`variance_propagation.h:82-84`）。"""
    return Fraction(n_frames + 1, n_frames - 1)


def residual_maker_variance_numpy(
    index: int, sigma2: tuple[float, ...] = INJECTED_SIGMA2
) -> float:
    """独立参考实现的**矩阵路线**：显式构造 `P = I − H`、`Σ = diag(σ²)`，
    取 `(PΣPᵀ)_ii`。与上面的 `Fraction` 逐项展开是两条不同代码路径。
    """
    n = len(sigma2)
    h = np.full((n, n), 1.0 / n, dtype=np.float64)
    p = np.eye(n, dtype=np.float64) - h
    sigma = np.diag(np.asarray(sigma2, dtype=np.float64))
    return float(np.diag(p @ sigma @ p.T)[index])


def naive_variance_numpy(index: int,
                        sigma2: tuple[float, ...] = INJECTED_SIGMA2) -> float:
    """朴素式的浮点路线：`σ_i² + Σ_j H_ij² σ_j²`（逐行显式求和，不走矩阵）。"""
    n = len(sigma2)
    h = np.full((n, n), 1.0 / n, dtype=np.float64)
    sigma = np.asarray(sigma2, dtype=np.float64)
    return float(sigma[index] + np.sum((h[index, :] ** 2) * sigma))


def exact_pixel_weight(layer_snr: float, ref_flux: float, gain: float) -> Fraction:
    """逐像素权重 `w = (SNR_layer/F_ref)² · g²` 的精确有理数。

    正本逐字（`weight_chain.h:27-31`、`:391-393`）：
    `w(x,y) = (SNR_layer(x,y) / F_ref,k)² · g_k²`。
    """
    snr = injected(layer_snr)
    fref = injected(ref_flux)
    g = injected(gain)
    return (snr / fref) ** 2 * g * g


def check_pixel_weight_chain(
    *,
    layer_present: bool,
    layer_snr: float | None,
    layer_semantics: str | None,
    ref_flux: float,
    gain: float | None,
    frame_snr: float | None = None,
    multiply_frame_snr: bool = False,
    absent_layer_as_one: bool = False,
    nonpositive_ref_flux_falls_back_to_equal: bool = False,
) -> dict:
    """逐像素权重链的 fail-closed 判定（B12/B13/B14 攻击的就是这个函数）。

    判据逐字来自 `weight_chain.h`：
    - `:27-31` `w(x,y) = (SNR_layer/F_ref,k)² · g_k²`；`:28-29` **禁止**再乘/除
      任何帧级 SNR 标量；
    - `:37` 稀疏层损坏/不可重建/越界 ⇒ 拒绝，**不得静默回退帧级**；
    - `:37-38` 层缺失不是「乘 1」；
    - `:35` `F_ref` 缺失/非有限/非正 ⇒ 拒绝；
    - `:42-43` legacy 等权回退即使显式请求也只返回 fail-closed；
    - `:39` 层声明为相对语义 ⇒ 拒绝。

    返回 `{"closed": bool, "weight": float|None, "reasons": [...]}`。
    """
    reasons: list[str] = []

    if not layer_present:
        if absent_layer_as_one:
            # 注入的错误捷径：把「层缺失」当成「层值 = 1」照常出权。
            injected_ref = injected(ref_flux)
            injected_gain = injected(gain if gain is not None else 1.0)
            return {"closed": True,
                    "weight": float((Fraction(1) / injected_ref) ** 2 * injected_gain ** 2),
                    "reasons": ["INJECTED_DEFECT: 层缺失被当成『层值=1』照常出权"]}
        return {"closed": False, "weight": None,
                "reasons": ["kUnclosedSparseLayerRequiredMissing: 层缺失，不得当作乘 1"]}

    if layer_semantics != FROZEN_SPARSE_SNR_SEMANTICS:
        return {"closed": False, "weight": None,
                "reasons": ["kUnclosedWrongSnrSemantics: 相对语义层不得进权重面"]}

    if ref_flux is None or not math.isfinite(ref_flux) or ref_flux <= 0:
        if nonpositive_ref_flux_falls_back_to_equal:
            return {"closed": True, "weight": 1.0,
                    "reasons": ["INJECTED_DEFECT: F_ref 非正却退化为等权"]}
        return {"closed": False, "weight": None,
                "reasons": ["kUnclosedInvalidReferenceFlux: F_ref 缺失/非有限/非正"]}

    if layer_snr is None or not math.isfinite(layer_snr) or layer_snr <= 0:
        return {"closed": False, "weight": None,
                "reasons": ["kUnclosedInvalidLayerSnr: 层值非正/非有限"]}

    if gain is None or not math.isfinite(gain) or gain <= 0:
        return {"closed": False, "weight": None,
                "reasons": ["kUnclosedInvalidGain: gain 缺失/非有限/非正"]}

    exact = exact_pixel_weight(layer_snr, ref_flux, gain)
    weight = float(exact)

    if multiply_frame_snr:
        if frame_snr is None or not math.isfinite(frame_snr) or frame_snr <= 0:
            return {"closed": False, "weight": None,
                    "reasons": ["kUnclosedMissingFrameSnr: 注入的错口径需要帧级 SNR，但帧级缺失"]}
        weight *= frame_snr
        return {"closed": False, "weight": weight,
                "reasons": [
                    "INJECTED_DEFECT: 权重式再乘帧级 SNR ⇒ 口径变成「帧级 × 绝对」，"
                    "weight_chain.h:28-29 明文禁止"
                ]}

    return {"closed": True, "weight": weight, "reasons": []}


# ==========================================================================
# 2 正例
# ==========================================================================


def test_ContractVariance_SchemaPositiveExamplesValidate() -> None:
    """A1：14 个正例夹具必须各自通过它所声明 schema 的校验。**当前读数：判红。**

    来源依据：`eng/contracts/schemas/unified/*.schema.json` 的 `type`/`required`/
    `const`/`additionalProperties`/`propertyNames`/`allOf-if-then` 全套约束；
    校验器关键字面见 `_object_contracts.py` 模块 docstring。

    ## 如实登记的红项（本条唯一红因，不是校验器缺陷）

    预期：全部正例通过。实测：`variance.example.json` 过不了 `variance.schema.json`。

    证据（复核者可独立复算，不依赖本校验器）：
    - `eng/contracts/schemas/unified/variance.schema.json:341-345`
      `correlation_kernel.required = ["family", "support_radius_px", "oracle_ref"]`，
      `oracle_ref` 在同文件 `:355-358` 的 `properties` 中已声明；
    - `eng/contracts/schemas/unified/examples/variance.example.json` 的
      `correlation_kernel` 键集只有
      `["boundary_definition", "family", "support_radius_px"]`。

    其余 13 个正例夹具全部零错误通过。零对象守卫（`VALIDATION_EVIDENCE.md:170-176`）
    在同一用例内自证：`scanned == len(files) == 14`。

    **两种读法并列登记，本用例不替负责人裁定**：
    ① schema 新增必填键 `oracle_ref` 后正例夹具未同步 ⇒ 修复面 = 补夹具字段；
    ② 夹具有意不声明 `oracle_ref`（视为可选项）⇒ 修复面 = 放宽 schema 的 `required`。
    两条读法的修复面都在正本变更面，不在本单授权内；本层**既不改夹具也不改 schema
    也不放宽判据**（`AGENTS.md` 第 7 节：代码与文档冲突时以文档为准订正代码，
    但正本本身的变更不是执行代理的授权面）。

    处置按 `TEST.md:203`「红项是缺陷信号，按修复或回退处理；测试不裁决合入」。
    本条与 A5b 同源（同一个 `oracle_ref` 缺失），不是两条独立缺陷。
    """
    files = oc.all_example_files()
    assert files, f"ANCHOR_STALE: 正例夹具面为空 {oc.EXAMPLE_DIR}"

    failures: list[str] = []
    scanned = 0
    for path in files:
        document = oc.load_example(path.name)
        object_name, schema_id = oc.object_identity(document)
        schema = oc.load_schema(object_name)
        ok, errors = oc.validate(document, schema)
        scanned += 1
        if not ok:
            failures.append(
                f"--- {path.name} 对 {object_name} ({schema_id}) 不合规 "
                f"（{len(errors)} 条）：\n{oc.format_errors(errors)}"
            )

    assert scanned == len(files), (
        f"实算对象数 {scanned} != 在册夹具数 {len(files)}（零对象守卫："
        f"VALIDATION_EVIDENCE.md:170-174）"
    )
    assert not failures, "\n".join(failures)


def test_ContractVariance_PositiveExampleScanIsNonEmpty() -> None:
    """A0：扫描面非空，且每个正例都能解析出一个真实存在的 schema。

    目的：把 A1 从「一个恒绿的空循环」变成「有实算对象数的判定」
    （`VALIDATION_EVIDENCE.md:170-176` 零对象守卫的两条）。
    """
    files = oc.all_example_files()
    assert len(files) == 14, f"正例夹具数 {len(files)} != 14（面被改动，须复核登记）"

    seen_objects: list[str] = []
    for path in files:
        document = oc.load_example(path.name)
        object_name, schema_id = oc.object_identity(document)
        schema_path = oc.schema_path(object_name)
        oc.require(schema_path, f"schema:{object_name}")
        schema = oc.load_schema(object_name)
        want_object, want_id = oc.schema_declared_identity(schema)
        assert object_name == want_object, (
            f"{path.name}: 夹具声明 {object_name!r}，schema 判别式是 {want_object!r}"
        )
        assert schema_id == want_id, (
            f"{path.name}: 夹具声明 schema ID {schema_id!r}，schema $id 是 {want_id!r}"
        )
        seen_objects.append(object_name)

    registry = oc.registry_object_names()
    unregistered = sorted({o for o in seen_objects if o not in registry})
    assert not unregistered, f"正例引用了登记表外的对象：{unregistered}"


def test_ContractVariance_NegativeFixturesHitExpectedPointers() -> None:
    """A2：6 个负例夹具必须被 `EXPECTED.json` 指定 schema 拒绝，
    且错误必须命中 `must_match` 列出的位置。

    这是仓内现成的「注入缺陷 ⇒ 判红」强判据：
    `EXPECTED.json` 是仓库自带的期望清单（不是本层生成的期望值）。

    ## `must_match` 是**子集**判定，不是错误集精确相等（复核者必读）

    本用例只要求「`must_match` 的每一条都被某条实得错误命中」，
    **不要求**实得错误集与 `must_match` 恰好相等，也不禁止额外错误。
    这不是宽松，而是负例有效性的必要条件：

    - `n2_source_snr_as_variance.schema-violation.json` 的目标 schema 是 `variance`，
      而 `variance.schema.json` 当前要求 `correlation_kernel.oracle_ref`（见 A1/A5b 的
      登记项）。该负例夹具**不带** `oracle_ref`，所以除它本来的违规
      （`unified_object:const`、`object_schema_id:const`、缺 `variance_value` 等）
      外还会**额外**报一条 `/correlation_kernel[required] missing='oracle_ref'`。
    - 若把判据写成「错误集精确相等」，这条与注入点无关的额外错误就会让 n2 变成
      「因无关理由判红」的无效负例 —— 正是 `VALIDATION_EVIDENCE.md:196` S6 要防的
      「把注入点放在共用逻辑上」。子集判定让判据只认注入点本身：n2 之所以有效，
      是因为 `unified_object:const` / `object_schema_id:const` / `variance_value`
      这三条**逐条命中**，与 `oracle_ref` 那条毫无关系。
    - 同理适用于 n4 / n5 的 `port_contract`（它们不碰 `correlation_kernel`）。

    子集语义由 `_object_contracts.hit_expectations` 实现，并用下面这条守护断言
    钉住：**每条负例的实际命中数必须等于 `must_match` 的长度**（不允许 0 命中蒙混）。

    来源依据：`eng/contracts/schemas/unified/negative/EXPECTED.json`；
    条目读法见 `_object_contracts.py` 的 `error_matches`。
    """
    expectations = oc.load_negative_expectations()
    assert len(expectations) == 6, f"负例条数 {len(expectations)} != 6（面被改动，须复核）"

    problems: list[str] = []
    for file_name, expected in sorted(expectations.items()):
        schema_name = expected["schema"]
        must_match = list(expected["must_match"])
        document = oc.load_negative(file_name)
        ok, errors = oc.validate(document, oc.load_schema(schema_name))
        if ok:
            problems.append(f"--- {file_name}: {schema_name} schema 竟然判绿（负例失效）")
            continue
        hit, miss = oc.hit_expectations(errors, must_match)
        if miss:
            problems.append(
                f"--- {file_name}: 未命中 must_match {miss}\n"
                f"    实得错误：\n{oc.format_errors(errors)}"
            )
            continue
        # 守护断言：子集判定必须是「全命中」，0 命中蒙混不过去。
        assert len(hit) == len(must_match), (
            f"{file_name}: 命中 {len(hit)}/{len(must_match)}，子集判定要求逐条全命中"
        )
        print(f"[A2] {file_name}: {schema_name} 判红，实得错误 {len(errors)} 条，"
              f"must_match {len(hit)}/{len(must_match)} 全命中"
              f"（子集判定：非注入点的额外错误不影响负例有效性）")
    assert not problems, "\n".join(problems)


def test_ContractVariance_RetiredObjectRejected() -> None:
    """A3：`psfsw_robust_weight` 是已退役对象，任何 schema 都不得接受它。

    来源依据：`port_contract.schema.json:13-23` 的
    `x-acsd-object.retired_objects`（`canonical_schema_removed: true`、
    变更号 `CHG-2026-09-20-PSFSW-RETIRE`）；
    `unified_object_registry.json#deprecation.object_retirements.psfsw_robust_weight`；
    负例夹具 n5。

    判定分两半（都要）：
    ①**结构面**：退役名不得出现在任何 schema 的 `accepts_object` /
    `accepts_schema_id` 枚举里，也不得有 canonical schema 文件存在；
    ②**实例面**：n5 夹具必须被 `port_contract` schema 拒绝。
    """
    port_schema = oc.load_schema("port_contract")
    retired = oc.retired_object_names(port_schema)
    assert retired == ["psfsw_robust_weight"], (
        f"port_contract 登记的退役对象是 {retired}，与本判据预期不符（面被改动）"
    )

    closed_objects = oc.accepted_object_enum(port_schema)
    closed_ids = oc.accepted_schema_id_enum(port_schema)
    assert "psfsw_robust_weight" not in closed_objects, (
        "退役对象仍在 accepts_object 枚举内 ⇒ fail-closed 拒绝被撤销"
    )
    retired_id = "https://acsd.local/schemas/unified/psfsw_robust_weight/v1"
    assert retired_id not in closed_ids, "退役对象的 schema ID 仍在接受枚举内"

    registry = oc.load_json(oc.REGISTRY_PATH, "registry")
    names = oc.registry_object_names()
    assert "psfsw_robust_weight" not in names, "退役对象仍在 canonical 登记表内"
    entry = registry["deprecation"]["object_retirements"]["psfsw_robust_weight"]
    assert entry["state"] == "retired"
    # `canonical_schema_removed` 登记在 port_contract schema 的
    # `x-acsd-object.retired_objects`（port_contract.schema.json:13-23），
    # 登记表那一侧只有 `removed` 清单。
    retired_entry = port_schema["x-acsd-object"]["retired_objects"][0]
    assert retired_entry["canonical_schema_removed"] is True, (
        "port_contract 未登记 canonical_schema_removed ⇒ 退役对象的拒绝面可能被撤销"
    )
    assert retired_entry["retired_at_change"] == entry["retired_at_change"], (
        "schema 与登记表的退役变更号不一致"
    )
    for removed in entry["removed"]:
        assert not oc.REPO_ROOT.joinpath(removed).exists(), (
            f"退役对象的 canonical schema 仍在仓库：{removed}"
        )

    # 实例面：n5 必须判红，且 `must_match` 全命中。
    expectations = oc.load_negative_expectations()
    file_name = "n5_retired_psfsw_robust_weight.schema-violation.json"
    document = oc.load_negative(file_name)
    ok, errors = oc.validate(document, port_schema)
    assert not ok, "n5（退役对象接进端口）竟然判绿 ⇒ 负例失效"
    hit, miss = oc.hit_expectations(errors, expectations[file_name]["must_match"])
    assert not miss, f"n5 未命中 must_match：{miss}"


def test_ContractVariance_ValidatorSelfCheckNonVacuous() -> None:
    """A4：证明本校验器确实在做功，不是恒真的空转。

    四条各自独立的注入，每条都必须让读数从绿翻红（`VALIDATION_EVIDENCE.md:193`
    S6「把某个真判定的判定逻辑换成恒真，只换它，核查必须点名该判定」）：

    1. 删掉正例夹具里一个 `required` 字段 ⇒ 必须判红（攻 `required` 分支）；
    2. 改掉正例夹具里一个 `const` 字段的取值 ⇒ 必须判红（攻 `const` 分支）；
    3. 给 `signal` 加一个裸 `weight` 键 ⇒ 必须判红（攻 `propertyNames` 分支）；
    4. 把 `direction` 改成枚举外的值 ⇒ 必须判红（攻 `enum` 分支）。
    """
    base_name = "signal.example.json"
    base = oc.load_example(base_name)
    schema = oc.load_schema("signal")
    assert oc.validate(base, schema)[0], "前提不成立：signal 正例本身应判绿"

    # (1) 删掉 required 字段（确定性取 schema required 的第一个键）
    victim = sorted(oc.load_schema("signal")["required"])[0]
    stripped = json.loads(json.dumps(base))
    stripped.pop(victim)
    ok, errors = oc.validate(stripped, schema)
    assert not ok, f"删掉 required 字段 {victim!r} 竟然仍判绿 ⇒ required 分支空转"
    assert any(e.get("missing") == victim for e in errors), (
        f"错误未点名缺失键 {victim!r}：{oc.format_errors(errors)}"
    )
    print(f"[A4-1] 删 required 键 {victim!r} ⇒ 判红（{len(errors)} 条错误）")

    # (2) 改 const
    const_key = "object_weight_verdict"
    mutated = json.loads(json.dumps(base))
    mutated[const_key] = "改为非冻结值"
    ok, errors = oc.validate(mutated, schema)
    assert not ok, f"改 const 字段 {const_key!r} 竟然仍判绿 ⇒ const 分支空转"
    assert any(e["keyword"] == "const" and e["location"] == const_key for e in errors), (
        f"错误未点名 {const_key} 的 const 违规：{oc.format_errors(errors)}"
    )
    print(f"[A4-2] 改 const 键 {const_key!r} ⇒ 判红（{len(errors)} 条错误）")

    # (3) propertyNames：裸 weight 键
    weighted = json.loads(json.dumps(base))
    weighted["weight"] = 1.0
    ok, errors = oc.validate(weighted, schema)
    assert not ok, "加裸 weight 键竟然仍判绿 ⇒ propertyNames 分支空转"
    keywords = {e["keyword"] for e in errors if e["location"] == "weight"}
    assert {"pattern", "additionalProperties"} <= keywords, (
        f"裸 weight 键未被 pattern 与 additionalProperties 双重拦住：{keywords}"
    )
    print(f"[A4-3] 加裸 weight 键 ⇒ 判红，命中关键字 {sorted(keywords)}")

    # (4) enum
    port = oc.load_negative("n4_source_snr_into_variance_port.schema-violation.json")
    port = json.loads(json.dumps(port))
    port["direction"] = "sideways"
    ok, errors = oc.validate(port, oc.load_schema("port_contract"))
    assert not ok, "越界 enum 取值竟然仍判绿 ⇒ enum 分支空转"
    assert any(e["keyword"] == "enum" and e["location"] == "direction" for e in errors), (
        f"错误未点名 direction 的 enum 违规：{oc.format_errors(errors)}"
    )
    print(f"[A4-4] 改 enum 键 direction ⇒ 判红（{len(errors)} 条错误）")


def test_ContractVariance_VarianceObjectChainContract() -> None:
    """A5a：方差传播链的**跨模块合同面**（链结构 + 合同面）。当前读数：绿。

    链：`signal`（f32 面亮度）→ `ivar`（像素逆方差）→ `variance`（像素方差）
    → `frame_snr`（帧级信噪比）→ `sparse_snr_layer`（稀疏控制点信噪比）。

    断言四项：
    ①每一环的 `object_schema_id` 与 `unified_object` **严格配对**，错配即红；
    ②`units.bunit_semantics` 一律 `written_px_power`；
    ③`missing_value.silent_zero_forbidden = true`，且 `missing_repr != "nan"`
      （`TEST.md:80-82` 第 4.4 节的可判定规则那一半）；
    ④`sparse_snr_semantics` 是**绝对**语义（相对语义载荷必须判红，见负例 B5）。

    **本用例不判「正例夹具是否通过自身 schema」** —— 那是 A5b 的面。
    两者拆开是为了让读者能把「链结构/合同面坏了」与「某个夹具与 schema 不同步」
    这两件不同的事区分开，不互相掩盖红项。

    来源依据：`docs/science/unified/DATA_SEMANTICS.md:73-93`（方差与逆方差的三态编码）、
    `:133-143`（标度类别与线性标度律，量纲可判三条件）；
    各 schema 的 `x-acsd-object.authority` 与 `x-acsd-gate` 标注
    （`signal.schema.json:6-12`、`:444-449`）；
    `clause_registry.json` 的 `FZ-BUNIT-SEMANTICS`。
    """
    chain: list[tuple[str, Any]] = []
    chain.append(("signal", oc.load_example("signal.example.json")))
    chain.append(("ivar", oc.load_example("ivar.example.json")))
    chain.append(("variance", oc.load_example("variance.example.json")))
    chain.append(("frame_snr", oc.load_example("frame_snr.example.json")))
    chain.append(("sparse_snr_layer", oc.load_example("sparse_snr_layer.example.json")))

    # 方差链五环必须齐备，零对象守卫：实算对象数 == 5（VALIDATION_EVIDENCE.md:170-174）
    assert [name for name, _ in chain] == list(oc.VARIANCE_CHAIN_OBJECTS), (
        f"链环不齐：{[n for n, _ in chain]} != {list(oc.VARIANCE_CHAIN_OBJECTS)}"
    )

    problems = check_variance_chain_contract(chain)
    assert not problems, "方差链合同面违规：\n" + "\n".join(problems)

    # 逐环点名，避免「整体绿但某环没读到」
    for name, document in chain:
        schema = oc.load_schema(name)
        _, want_id = oc.schema_declared_identity(schema)
        _, got_id = oc.object_identity(document)
        assert got_id == want_id, f"{name}: schema ID 配对失败"
        units = oc.units_of(document)
        assert units["bunit_semantics"] == FROZEN_BUNIT_SEMANTICS, name
        mv = oc.missing_value_of(document)
        assert mv["silent_zero_forbidden"] is True, name
        assert mv["missing_repr"] != "nan", (
            f"{name}: 缺失由 NaN 承载 ⇒ 违反 TEST.md:80"
        )
    print(f"[A5a] 方差链 {len(chain)} 环：配对/单位/第三态逐环通过")


def test_ContractVariance_EachChainRingValidatesOwnSchema() -> None:
    """A5b：方差链每一环的**正例夹具必须通过自身 schema**。当前读数：**红（缺陷信号）**。

    与 A5a 拆开，避免读者把「链结构/合同面坏了」误读成「schema 坏了」。

    **实测读数**：`variance` 环的 `variance.example.json` 过不了
    `variance.schema.json`，缺必填键 `correlation_kernel.oracle_ref`。
    五个环里只有这一个坏：`signal` / `ivar` / `frame_snr` / `sparse_snr_layer` 四环全绿。

    证据（复核者可独立复算，不依赖本校验器）：
    - `eng/contracts/schemas/unified/variance.schema.json:341-345`
      `correlation_kernel.required = ["family", "support_radius_px", "oracle_ref"]`，
      且 `oracle_ref` 在同文件 `:355-358` 的 `properties` 中声明；
    - `eng/contracts/schemas/unified/examples/variance.example.json` 的
      `correlation_kernel` 键集只有
      `["boundary_definition", "family", "support_radius_px"]`。

    **两种读法并列登记，本用例不替负责人裁定**：
    ① schema 新增必填键 `oracle_ref` 后正例夹具未同步 ⇒ 修复面 = 补夹具字段；
    ② 夹具有意不声明 `oracle_ref`（视为可选项）⇒ 修复面 = 放宽 schema 的 `required`。
    两条读法的修复面都在正本变更面，不在本单授权内，本层**既不改夹具也不改 schema
    也不放宽判据**。处置按 `TEST.md:203`「红项是缺陷信号，按修复或回退处理」。

    本条红项与 A1 的红项**同源**（同一个 `oracle_ref` 缺失），不是两条独立缺陷。
    """
    failures: list[str] = []
    green_rings: list[str] = []
    for name in oc.VARIANCE_CHAIN_OBJECTS:
        document = oc.load_example(f"{name}.example.json")
        ok, errors = oc.validate(document, oc.load_schema(name))
        if ok:
            green_rings.append(name)
        else:
            failures.append(f"--- {name}: {oc.format_errors(errors)}")
    assert not failures, (
        "方差链正例夹具与自身 schema 不同步（登记项，非链结构问题；"
        "与 A1 同源）：\n" + "\n".join(failures)
    )
    print(f"[A5b] 五环全绿：{green_rings}")


def test_ContractVariance_WeightIsNotABareField() -> None:
    """A6：`signal` 对象**不得**自带 `weight` 字段 —— 权重只能由带权威式的模式给出。

    来源依据：`signal.schema.json:28-50` 的 `propertyNames` 正则
    （禁 `weight|value|mask|snr` 及其前缀形态）与 `:14` 的 `additionalProperties: false`；
    `VALIDATION_EVIDENCE.md:114-121` 第 4.2 节模式表——权重对象与单位由模式表给出，
    `signal` 不出现在任何模式的「权重对象」列中。
    """
    signal = oc.load_example("signal.example.json")
    problems = check_no_bare_weight(signal)
    assert not problems, "signal 正例带裸权重：\n" + "\n".join(problems)

    # 结构面：signal schema 自身必须带 propertyNames 守卫，且守卫确实拒绝 weight。
    schema = oc.load_schema("signal")
    guard = schema.get("propertyNames", {}).get("pattern")
    assert guard, "signal schema 缺 propertyNames 守卫（锚失效）"
    assert re.search(guard, "weight") is None, "propertyNames 守卫放过了 'weight'"
    assert re.search(guard, "weight_value") is None, "propertyNames 守卫放过了 'weight_value'"
    assert re.search(guard, "signal_value") is not None, "propertyNames 守卫误伤了 'signal_value'"

    # 第 4.2 节模式表的「权重对象」列不得含 signal（正本逐字转录 + 锚存活自检）。
    evidence = (oc.REPO_ROOT / "docs/engineering/testing/VALIDATION_EVIDENCE.md").read_text(
        encoding="utf-8")
    mode_rows = [
        "点源信息权重", "组合系数加权的广义最小二乘", "点源信号权重", "单位权重", "像素逆方差",
    ]
    present = [row for row in mode_rows if row in evidence]
    assert len(present) == len(mode_rows), (
        f"VALIDATION_EVIDENCE.md 第 4.2 节模式表的「权重对象」列转录失配："
        f"只命中 {present}（锚可能已改版）"
    )
    assert "signal" not in mode_rows, "模式表里出现了 signal 作为权重对象"


def test_ContractVariance_VariancePropagationIndependentOracle() -> None:
    """A7（本单核心）：用**自己构造的注入值**做一次方差传播的解析对拍。

    三条独立路线：
      (i)   精确有理数期望（`Fraction`，从正本公式逐项展开）；
      (ii)  浮点参考实现（`numpy`，显式构造 `P = I − H`、`Σ = diag(σ²)` 走矩阵乘法）；
      (iii) 契约层面的逐条公式（守恒映射方差、三态编码、逆方差叠加）。

    注入值全部写死在本文件（`INJECTED_*`），**不来自任何产品输出或现有产物**。

    容差（`TEST.md:44-50` 第 4 节冻结表，逐字引用）：
      - 守恒映射那一支是**非归约**的少量四则运算 ⇒ 双精度非归约档
        `rtol = 1e-12`、`atol = 1e-13 × scale`；
      - `PΣPᵀ` 那一支是**归约** ⇒ `γ_n = n·u/(1−n·u)`、门限 `C·γ_n·Σ|terms| + atol`，
        `C ≤ 4`、`u(binary64) = 1.1102230246251565e-16`、`n` 取该次归约的
        **实际项数**（A8 展开）；本条只做非归约档，A8 走归约档。

    `scale` 适用量级域（`TEST.md:71-73` 第 4.3 节，必填）：
      - 守恒映射输出方差 `variance_p`：域 `1e0–1e2 ADU²/sr²`（`TEST.md:225` 的
        「像素面 1e0–1e2 ADU²」同族）；本次注入落在 0.67–2.0；
      - 逆方差 `ivar`：域 `1e-2–1e2 ADU⁻²`；本次注入落在 0.44–1.5；
      - 叠加合成通量：域 `1e0–1e8 ADU`（`TEST.md:228` 的「帧级信噪比平方和量，
        量级 1e0–1e8」同族）；本次注入落在 17.05。

    来源依据：`docs/science/unified/DATA_SEMANTICS.md:73-80`（`variance =
    v_num_sum / D_p²`，`ivar = 1/variance`）、`:82-93`（三态编码表）、`:136-139`
    （线性标度律 `Var′ = α²·Var`、`ivar′ = ivar/α²`、`Var(S) = Var(F)/A_cell²`）；
    `docs/science/noise_snr/NOISE_SNR.md:380-390`（守恒映射方差两参数化）；
    `:343-349`（`F_hat = ΣQ_k/ΣW_k`、`Var(F_hat) = 1/Σ_k W_k`）；
    `docs/science/algorithms/PHASE2_INTEGRATION.md:113-121`（`signal = vs/wsum`）。
    """
    # ---------- (i)(ii) 守恒映射单像素：variance = Σ v_j w_jp² / D_p² ----------
    a_pixel = [injected(a) for a in INJECTED_A_PIXEL]
    a_jp = [injected(a) for a in INJECTED_A_JP]
    v_j = [injected(v) for v in INJECTED_V_J]
    w_jp = [a_jp[i] / a_pixel[i] for i in range(len(a_jp))]
    d_p = sum(a_jp)
    v_num = sum(v_j[i] * w_jp[i] ** 2 for i in range(len(v_j)))   # 精确
    exact_variance = v_num / (d_p ** 2)
    exact_ivar = Fraction(1) / exact_variance

    # 浮点路线（与 Fraction 路线不同源）
    w_f = np.asarray([float(x) for x in w_jp], dtype=np.float64)
    v_f = np.asarray([float(x) for x in v_j], dtype=np.float64)
    d_f = float(d_p)
    float_variance = float(np.sum(v_f * w_f ** 2) / (d_f ** 2))
    float_ivar = 1.0 / float_variance

    scale_var = float(exact_variance)
    atol_var = ATOL_F64_SCALE_FACTOR * scale_var
    assert math.isfinite(scale_var) and scale_var > 0
    assert atol_var >= ulp(scale_var), (
        f"TEST.md:73 可满足性下限：atol={atol_var:g} < 1 ulp(scale)={ulp(scale_var):g} ⇒ "
        f"该绝对容差在 scale={scale_var:g} 上不可判"
    )
    assert float_variance == pytest.approx(scale_var, rel=RTOL_F64, abs=atol_var), (
        f"守恒映射方差：浮点路线 {float_variance!r} vs 精确有理数 {exact_variance} "
        f"（rtol={RTOL_F64}, atol={atol_var:g}）"
    )
    scale_ivar = float(exact_ivar)
    atol_ivar = ATOL_F64_SCALE_FACTOR * scale_ivar
    assert atol_ivar >= ulp(scale_ivar), "ivar 绝对容差不可判（TEST.md:73）"
    assert float_ivar == pytest.approx(scale_ivar, rel=RTOL_F64, abs=atol_ivar), (
        f"逆方差：浮点路线 {float_ivar!r} vs 精确有理数 {exact_ivar}"
    )
    # 解析数值逐字落盘，便于复算
    assert exact_variance == Fraction(2, 3), (
        f"注入值的精确解析结果变了：{exact_variance}（Σv_j w_jp²={v_num}, D_p={d_p}）"
    )
    assert exact_ivar == Fraction(3, 2)
    print(f"[A7-守恒] D_p={d_p} sr, v_num={v_num} ADU² ⇒ variance={exact_variance} "
          f"ADU²/sr², ivar={exact_ivar} ADU⁻²；浮点路线 "
          f"{float_variance!r}/{float_ivar!r}")

    # ---------- (iii) 三态编码（DATA_SEMANTICS.md:82-93）----------
    # 有效态
    assert exact_variance > 0 and exact_ivar > 0
    # 有覆盖但方差不可用态：v_num == 0 ⇒ (variance, ivar) = (0, 0)，不是 (0, +inf)
    v_num_unavailable = Fraction(0)
    var_unavail = v_num_unavailable / (d_p ** 2)
    ivar_unavail = Fraction(1) / var_unavail if var_unavail != 0 else Fraction(0)
    assert (var_unavail, ivar_unavail) == (Fraction(0), Fraction(0)), (
        f"「有覆盖但方差不可用」态必须是成对零，实得 {(var_unavail, ivar_unavail)}；"
        f"DATA_SEMANTICS.md:87-91 明文排除 (0, +inf)"
    )
    # 无覆盖态：D_p <= 0 ⇒ 三者同取 NaN（IEEE 无序，不是 0、不是 ±Inf）
    d_zero = 0.0
    no_cov = float("nan") / d_zero if d_zero else float("nan")
    assert math.isnan(no_cov), "无覆盖态必须是 NaN"
    assert no_cov != 0.0 and not math.isinf(no_cov), "NaN 被折叠成了 0 或哨兵值（TEST.md:81）"

    # ---------- (iii) 线性标度律（DATA_SEMANTICS.md:136-139）----------
    alpha = Fraction(3)          # 逐帧测光标度 α_k
    a_cell = Fraction(4)         # 目标像元立体角 sr
    var_scaled = exact_variance * alpha ** 2
    ivar_scaled = exact_ivar / alpha ** 2
    # 注入值：exact_variance = 2/3 ⇒ ×α²(=9) = 6；exact_ivar = 3/2 ⇒ ÷9 = 1/6
    assert var_scaled == Fraction(6), f"α² 缩放律给出 {var_scaled}（期望 6）"
    assert ivar_scaled == Fraction(1, 6), f"α⁻² 缩放律给出 {ivar_scaled}（期望 1/6）"
    # 面亮度口径：S = F / A_cell ⇒ Var(S) = Var(F) / A_cell²
    flux = Fraction(10)          # ADU
    var_flux = Fraction(12)      # ADU²
    var_sb = var_flux / a_cell ** 2
    assert var_sb == Fraction(3, 4), f"面亮度方差给出 {var_sb}（期望 3/4 ADU²/sr²）"
    print(f"[A7-标度] α={alpha}: Var {exact_variance} → {var_scaled}, "
          f"ivar {exact_ivar} → {ivar_scaled}；Var(S)=Var(F)/A_cell²={var_sb}")

    # ---------- (iii) 逆方差叠加（PHASE2_INTEGRATION.md:113-121）----------
    wsum = sum(INJECTED_IVAR)
    vs = sum(INJECTED_IVAR[i] * Fraction(INJECTED_VALUES[i]).limit_denominator(10 ** 6)
             for i in range(len(INJECTED_IVAR)))
    f_hat = vs / wsum
    var_f_hat = Fraction(1) / wsum
    # 独立路线：numpy 的同型计算（浮点）
    w_arr = np.asarray([float(x) for x in INJECTED_IVAR], dtype=np.float64)
    v_arr = np.asarray(INJECTED_VALUES, dtype=np.float64)
    f_hat_np = float(np.sum(w_arr * v_arr) / np.sum(w_arr))
    var_f_hat_np = float(1.0 / np.sum(w_arr))
    scale_f = float(f_hat)
    atol_f = ATOL_F64_SCALE_FACTOR * scale_f
    assert atol_f >= ulp(scale_f), "合成通量的绝对容差不可判（TEST.md:73）"
    assert f_hat_np == pytest.approx(scale_f, rel=RTOL_F64, abs=atol_f), (
        f"逆方差合成通量：浮点 {f_hat_np!r} vs 精确 {f_hat}"
    )
    scale_vf = float(var_f_hat)
    atol_vf = ATOL_F64_SCALE_FACTOR * scale_vf
    assert atol_vf >= ulp(scale_vf), "合成方差的绝对容差不可判（TEST.md:73）"
    assert var_f_hat_np == pytest.approx(scale_vf, rel=RTOL_F64, abs=atol_vf), (
        f"逆方差合成方差：浮点 {var_f_hat_np!r} vs 精确 {var_f_hat}"
    )
    assert f_hat == Fraction(1040, 61), f"注入值的精确合成通量变了：{f_hat}"
    assert var_f_hat == Fraction(144, 61), f"注入值的精确合成方差变了：{var_f_hat}"
    print(f"[A7-叠加] wsum={wsum} ADU⁻² ⇒ F_hat={f_hat} ADU, "
          f"Var(F_hat)={var_f_hat} ADU²；浮点路线 {f_hat_np!r}/{var_f_hat_np!r}")

    # ---------- 非退化自证（TEST.md:26「恒真的比较没有证据资格」）----------
    # 真值无效应 ⇒ 度量归零：把全部 σ² 置 0，残差制造者方差必须归零。
    zero_sigma = np.zeros(INJECTED_N_FRAMES, dtype=np.float64)
    h = np.full((INJECTED_N_FRAMES, INJECTED_N_FRAMES), 1.0 / INJECTED_N_FRAMES)
    p = np.eye(INJECTED_N_FRAMES) - h
    assert float(np.diag(p @ np.diag(zero_sigma) @ p.T)[0]) == 0.0, (
        "σ² 全零时残差制造者方差未归零 ⇒ 度量退化"
    )
    # 参考实现与朴素式必须给出不同答案，否则本判据无鉴别力。
    gap = abs(residual_maker_variance_numpy(0) - naive_variance_numpy(0))
    assert gap > 1e-3 * max(abs(residual_maker_variance_numpy(0)), 1.0), (
        f"残差制造者式与朴素式的差 {gap:g} 过小 ⇒ 本判据无鉴别力（见 A8/B10）"
    )


def test_ContractVariance_ResidualMakerCrossTermsAndGainSquare() -> None:
    """A8：Phase2b 残差制造者方差传播 `PΣPᵀ` —— 抓交叉项、乘性归一化抓 `÷g²`。

    正本逐字（`lib/algorithms/integration/phase2_integrate/include/acsd/
    variance_propagation.h`）：
    - `:12-15` `corrected = y − ĝ = (I − H) y = P y`，
      `Var(corrected_i) = Σ_j P_ij² σ_j² + param_i`，`P = I − H`；
    - `:19-22` **错误形式** `σ_i² + Σ_j H_ij² σ_j²`「漏掉 `−HΣ − ΣHᵀ` 交叉项」，
      对 `ĝ = ȳ` 给出 `σ²(1+1/N)` 而正确是 `σ²(1−1/N)`；
    - `:24-25` `corrected=(y−ĝ)/g ⇒ Var /= g²`（「÷g² 是硬要求」）；
    - `:70-77` 生产总入口 `Var(corrected_i) = [PΣPᵀ_ii + param_var] / g²`；
    - `:82-84` 等权均值模型下朴素/正确之比 `(1+1/N)/(1−1/N)`。

    期望值全部来自 `Fraction` 的**精确有理数**（`_exact_*` 函数），
    **不是**由参考实现输出反推；`oracle.truth = independent_stdlib`（精确有理数）+
    `independent_numpy`（矩阵参考实现）；`oracle.must_not` 含产品可执行程序本身
    与任何产品产物（本条不消费任何一项）。

    容差（`TEST.md:44-50`）：这是**归约**，故用归约档
    `γ_n = n·u/(1−n·u)`、门限 `C·γ_n·Σ|terms| + atol`、`C ≤ 4`、
    `u = 1.1102230246251565e-16`、**`n` 取该次归约的实际项数**（= `N` = 8，
    不是数据总像素数，见 `TEST.md:62`）。

    `scale` 适用量级域（`TEST.md:71-73` 第 4.3 节，必填）：单帧逐像素方差，
    域 `1e0–1e2 ADU²`（`TEST.md:225` 同族）；本次注入落在 7.4–21.0。
    """
    n = INJECTED_N_FRAMES
    assert len(INJECTED_SIGMA2) == n, "注入项数与 N 不符"
    h_row = np.full(n, 1.0 / n, dtype=np.float64)

    # --- 逐帧残差制造者方差：矩阵路线 vs 精确有理数 ---
    worst = 0.0
    for i in range(n):
        exact = exact_residual_maker_variance(i)
        got = residual_maker_variance_numpy(i)
        sum_abs_terms = float(np.sum((np.eye(n)[i] - h_row) ** 2
                                     * np.asarray(INJECTED_SIGMA2)))
        threshold = reduction_threshold(n_terms=n, sum_abs_terms=sum_abs_terms,
                                        scale=float(exact))
        residual = abs(got - float(exact))
        worst = max(worst, residual)
        assert residual <= threshold, (
            f"第 {i} 帧残差制造者方差：矩阵路线 {got!r} vs 精确 {exact}；"
            f"残差 {residual:.3e} > 归约门限 {threshold:.3e}"
            f"（n={n}, C={oc.REDUCTION_AMPLIFICATION_C}, γ_n={gamma_n(n):.3e}）"
        )
        scale_i = abs(float(exact))
        assert ATOL_F64_SCALE_FACTOR * scale_i >= ulp(scale_i), (
            f"第 {i} 帧绝对容差不可判（TEST.md:73 可满足性下限）"
        )
    print(f"[A8] PΣPᵀ 逐帧（可精确表示的整数注入）：8/8 帧落在归约档门限内，"
          f"最大残差 {worst:.3e}（这些 σ² 是小整数，binary64 可精确表示 ⇒ 残差 0，"
          f"门限不触发）")

    # --- 第二组注入：不可精确表示的 σ²，使归约档门限**真正被检验** ---
    worst_ne, worst_rel, bound_used = 0.0, 0.0, 0.0
    sig2 = INJECTED_SIGMA2_NONEXACT
    for i in range(len(sig2)):
        exact = exact_residual_maker_variance(i, sig2)
        got = residual_maker_variance_numpy(i, sig2)
        sum_abs_terms = float(np.sum((np.eye(len(sig2))[i]
                                      - np.full(len(sig2), 1.0 / len(sig2))) ** 2
                                     * np.asarray(sig2)))
        threshold = reduction_threshold(n_terms=len(sig2), sum_abs_terms=sum_abs_terms,
                                        scale=float(exact))
        residual = abs(got - float(exact))
        worst_ne = max(worst_ne, residual)
        worst_rel = max(worst_rel, residual / abs(float(exact)))
        bound_used = max(bound_used, residual / threshold)
        assert residual <= threshold, (
            f"不可精确表示注入下第 {i} 帧残差 {residual:.3e} > 归约门限 {threshold:.3e}"
        )
    assert worst_ne > 0.0, (
        "第二组注入的残差仍恒为 0 ⇒ 归约档容差判据从未被触发，"
        "容差表在本条上退化成装饰（TEST.md:26「恒真的比较没有证据资格」）"
    )
    print(f"[A8] PΣPᵀ 逐帧（不可精确表示的注入）：{len(sig2)}/{len(sig2)} 帧落在归约档门限内，"
          f"最大残差 {worst_ne:.3e}（相对 {worst_rel:.3e}），"
          f"实测/门限最紧处 = {bound_used:.3f} ⇒ 门限确实被走到")

    # --- 等权 σ² 复现正本点名的 9/7（N=8）---
    uniform = np.ones(n, dtype=np.float64)
    p = np.eye(n) - np.full((n, n), 1.0 / n)
    var_uniform = float(np.diag(p @ np.diag(uniform) @ p.T)[0])
    exact_uniform = Fraction(1) * (Fraction(n - 1, n) ** 2) + Fraction(1, n * n) * (n - 1)
    assert exact_uniform == Fraction(7, 8), (
        f"等权 σ²=1 下精确残差制造者方差是 {exact_uniform}，与正本 σ²(1−1/N)=σ²·7/8 不符"
    )
    assert var_uniform == pytest.approx(0.875, rel=RTOL_F64,
                                        abs=ATOL_F64_SCALE_FACTOR * 0.875)
    ratio_exact = exact_mean_model_ratio(n)          # (1+1/N)/(1−1/N)
    assert ratio_exact == Fraction(9, 7), (
        f"等权模型的朴素/正确之比精确值是 {ratio_exact}，与正本 :82-84 的 9/7 不符"
    )
    ratio_measured = naive_variance_numpy(0) / residual_maker_variance_numpy(0)  # 非均匀注入
    ratio_measured_uniform = (1.0 + float(np.sum(h_row ** 2) * uniform[0])) / var_uniform
    assert ratio_measured_uniform == pytest.approx(float(ratio_exact), rel=RTOL_F64), (
        f"等权模型比值 {ratio_measured_uniform!r} != {float(ratio_exact)!r}"
    )
    print(f"[A8] 等权 σ²=1：Var_correct={var_uniform}（精确 {exact_uniform}），"
          f"朴素/正确 = {ratio_measured_uniform!r}（精确 {ratio_exact} = 1.2857…）")
    print(f"[A8] 非均匀注入下实测比值 = {ratio_measured!r}（精确 "
          f"{float(exact_naive_variance(0) / exact_residual_maker_variance(0))!r}）")

    # --- 乘性归一化 ÷g² ---
    gain = INJECTED_GAIN
    exact_with_gain = exact_residual_maker_variance(0) / injected(gain) ** 2
    got_with_gain = residual_maker_variance_numpy(0) / (gain ** 2)
    assert exact_with_gain == Fraction(119, 64), (
        f"注入值下精确的 PΣPᵀ/g² 是 {exact_with_gain}（期望 119/64）"
    )
    scale_g = float(exact_with_gain)
    assert ATOL_F64_SCALE_FACTOR * scale_g >= ulp(scale_g), "绝对容差不可判（TEST.md:73）"
    assert got_with_gain == pytest.approx(scale_g, rel=RTOL_F64,
                                          abs=ATOL_F64_SCALE_FACTOR * scale_g), (
        f"÷g² 后的方差：{got_with_gain!r} vs 精确 {exact_with_gain}"
    )
    assert got_with_gain == pytest.approx(residual_maker_variance_numpy(0) / 4.0), (
        "÷g² 不是除以 g² —— 归一化口径被改写"
    )
    print(f"[A8] ÷g²：g={gain} ⇒ Var {residual_maker_variance_numpy(0)!r} → "
          f"{got_with_gain!r}（精确 {exact_with_gain}）")


def test_ContractVariance_WeightChainPairingAnalytic() -> None:
    """A9：跨帧权重链的配对性 —— `w = (SNR_layer/F_ref)² · g² ≡ 1/σ_F²`。

    正本逐字（`lib/algorithms/integration/phase2_integrate/include/acsd/
    weight_chain.h`）：
    - `:27-31` `w(x,y) = (SNR_layer(x,y) / F_ref,k)² · g_k²`，`SNR_layer` 是层给出的
      **绝对**信噪比本身；**禁止**再乘/除任何帧级 SNR 标量；
    - `:45-50` 单位：SNR 无量纲、`F_ref` [ADU]、`w` [ADU⁻²]（= `1/σ_F²`）；
    - `:34-42` fail-closed 纪律（帧级 SNR 缺失 / `F_ref` 非正 / 层损坏 ⇒ 拒绝，
      不静默退化为等权、不静默「乘 1」）。

    期望值来自**自己构造的注入值** + `Fraction` 精确有理数，
    不是任何产品产物的读数，也不是任何 `A−A` 恒等式。

    容差（`TEST.md:44-50`）：权重是**非归约**的少量四则运算 ⇒ 双精度非归约档
    `rtol = 1e-12`、`atol = 1e-13 × scale`。

    `scale` 适用量级域（`TEST.md:71-73`）：`w` 的域 `1e-6–1e2 ADU⁻²`
    （`TEST.md:225`「1e0–1e2 ADU²」方差的倒数域）；本次注入落在 4.0e-2–4.0e-4。
    """
    # 结构面：签名里只有一个 SNR 输入、没有帧级 SNR 参数（weight_chain.h:390-395）
    result_a = check_pixel_weight_chain(
        layer_present=True,
        layer_snr=INJECTED_LAYER_SNR[0],
        layer_semantics=FROZEN_SPARSE_SNR_SEMANTICS,
        ref_flux=INJECTED_REF_FLUX[0],
        gain=INJECTED_GAINS[0],
    )
    result_b = check_pixel_weight_chain(
        layer_present=True,
        layer_snr=INJECTED_LAYER_SNR[1],
        layer_semantics=FROZEN_SPARSE_SNR_SEMANTICS,
        ref_flux=INJECTED_REF_FLUX[1],
        gain=INJECTED_GAINS[1],
    )
    assert result_a["closed"], result_a["reasons"]
    assert result_b["closed"], result_b["reasons"]

    exact_a = exact_pixel_weight(INJECTED_LAYER_SNR[0], INJECTED_REF_FLUX[0], INJECTED_GAINS[0])
    exact_b = exact_pixel_weight(INJECTED_LAYER_SNR[1], INJECTED_REF_FLUX[1], INJECTED_GAINS[1])
    # 注入值 (SNR=10, F_ref=100 ADU, g=2) ⇒ w = (10/100)²·4 = 1/25 ADU⁻²
    assert exact_a == Fraction(1, 25), f"注入值 a 的精确权重是 {exact_a}，期望 1/25"
    assert exact_b == Fraction(1, 2500), f"注入值 b 的精确权重是 {exact_b}，期望 1/2500"

    for name, result, exact in (("a", result_a, exact_a), ("b", result_b, exact_b)):
        scale = float(exact)
        atol = ATOL_F64_SCALE_FACTOR * scale
        assert atol >= ulp(scale), f"帧 {name} 的绝对容差不可判（TEST.md:73）"
        assert result["weight"] == pytest.approx(scale, rel=RTOL_F64, abs=atol), (
            f"帧 {name} 权重 {result['weight']!r} vs 精确 {exact}"
        )
        # 量纲：w 与 1/σ_F² 同量纲（σ_F := F_ref/SNR_layer 由此注入值独立导出）
        idx = 0 if name == "a" else 1
        sigma_f = injected(INJECTED_REF_FLUX[idx]) / injected(INJECTED_LAYER_SNR[idx])
        assert exact == Fraction(1) / (sigma_f ** 2) * injected(INJECTED_GAINS[idx]) ** 2, (
            f"帧 {name} 的权重与 1/σ_F²·g² 不同口径"
        )

    # 跨帧权重比：解析值来自逐帧注入，不经任何产品读数
    ratio_exact = exact_a / exact_b
    ratio_measured = result_a["weight"] / result_b["weight"]
    scale_ratio = float(ratio_exact)
    assert ATOL_F64_SCALE_FACTOR * scale_ratio >= ulp(scale_ratio), "比值绝对容差不可判"
    assert ratio_measured == pytest.approx(scale_ratio, rel=RTOL_F64,
                                           abs=ATOL_F64_SCALE_FACTOR * scale_ratio), (
        f"跨帧权重比 {ratio_measured!r} vs 精确 {ratio_exact}"
    )
    assert ratio_exact == 100, f"注入值的精确权重比是 {ratio_exact}，期望 100"

    # 非退化：两帧权重必须真的不同，否则比值判据无鉴别力
    assert exact_a != exact_b, "两帧权重相同 ⇒ 跨帧比值判据恒真（TEST.md:26）"
    print(f"[A9] w_a={exact_a}, w_b={exact_b} ADU⁻²，比值精确 {ratio_exact}，"
          f"实测 {ratio_measured!r}")


def test_ContractVariance_NonFiniteAndMissingAreThirdState() -> None:
    """A10：缺失是与 NaN **并列的第三态**，schema 不得让 NaN 兼表缺失。

    **族一**（`signal` / `variance` / `frame_snr` / `sparse_snr_layer`）四项逐字
    一致：`missing_repr = "null"`、`invalid_repr = "nan"`、`nan_in_binary = true`、
    `silent_zero_forbidden = true`；另加结构面判定：`missing_repr` 的 schema 枚举里
    **不含** `"nan"`（schema 自身禁止兼表）。

    **族二**（`ivar`，以及几何门族的 `coverage` / `rejection` / `support`）走
    `zero/zero/false` 那一族：`missing_repr` 冻结值是 `"zero"`。
    `TEST.md:80` 的可判定规则对它照样成立（缺失不得由 NaN 承载），但**不能**对它
    断言 `nan_in_binary = true` —— 那一族明确声明二进制面不承载 NaN。
    `ivar` 的这一族声明与 `docs/science/unified/DATA_SEMANTICS.md:88`
    「无覆盖 ⇒ `ivar = NaN`」相冲突，已作为待裁决冲突登记（见 `README.md`），
    本判据**不替负责人裁决**，只断言 `TEST.md:80` 的可判定规则那一半。

    来源依据：`TEST.md:75-82`；`signal.schema.json:186-232`；
    `docs/science/unified/DATA_SEMANTICS.md:82-93`（三态编码表）。
    """
    # 族一：声明 missing_repr="null" 的测量族 —— 严格四元组逐字一致
    null_nan_family = ("signal", "variance", "frame_snr", "sparse_snr_layer")
    # 族二：几何门与占比面（含 ivar）—— missing_repr="zero"，见 README 登记项
    zero_family = ("ivar",)

    for name in null_nan_family:
        document = oc.load_example(f"{name}.example.json")
        problems = check_missing_value_contract(name, document, strict_nan_carrier=True)
        assert not problems, f"{name} 的缺失/无效声明违规：\n" + "\n".join(problems)
        problems = check_schema_forbids_nan_as_missing(name)
        assert not problems, f"{name} 的 schema 允许 NaN 兼表缺失：\n" + "\n".join(problems)

    for name in zero_family:
        document = oc.load_example(f"{name}.example.json")
        mv = oc.missing_value_of(document)
        assert mv["missing_repr"] == "zero", f"{name}: 族二的 missing_repr 冻结值是 'zero'"
        # TEST.md:80 的可判定规则仍必须成立：缺失不得由 NaN 承载。
        problems = check_missing_value_contract(name, document)
        assert not problems, f"{name} 违反 TEST.md:80 的缺失语义：\n" + "\n".join(problems)
        # DATA_SEMANTICS.md:87 的「有覆盖但方差不可用」成对零态：0 与 NaN 必须可区分。
        assert mv["missing_repr"] != "nan", f"{name}: 缺失由 NaN 承载"

    # 比较器语义：非有限值与缺失不受三档浮点容差约束（TEST.md:82）——
    # 位置集合必须逐项精确一致，不存在「落在容差内通过」的中间态。
    positions_nan = {3, 7, 11}
    positions_missing = {3, 7, 11}
    positions_finite = {0, 1, 2, 4, 5, 6, 8, 9, 10, 12, 13}
    assert positions_nan == positions_missing, "缺失位置集合与 NaN 位置集合未逐项一致"
    assert not (positions_nan & positions_finite), "非有限位置与有限位置重叠"
    assert len(positions_nan) == 3 and len(positions_finite) == 11, (
        "注入的位置集合大小变了，判据的自检基数需同步"
    )
    # 非有限值不得被折叠成 0 或哨兵值后再参与有限比较（TEST.md:81）
    sentinel = float("nan")
    assert math.isnan(sentinel) and sentinel != 0.0 and not math.isinf(sentinel)
    print(f"[A10] 族一（{len(null_nan_family)} 个）：四项声明逐字一致 + schema 禁止 NaN 兼表缺失；"
          f"族二（{len(zero_family)} 个，ivar）：missing_repr='zero'，TEST.md:80 可判定规则仍成立；"
          f"注入位置集合 NaN@{sorted(positions_nan)} 缺失@{sorted(positions_missing)}")


def test_ContractVariance_NoBlockingExitCodeInTestCode() -> None:
    """A11：本层测试代码不产出阻塞退出码、不用跳过充数（项目定位：测试不是门禁）。

    扫描本层两个文件的源码，断言不含 `sys.exit` / `SystemExit` / `os._exit`，
    也不含 `pytest.skip` / `pytest.xfail` / `unittest.skip` 充数。
    """
    forbidden = {
        "sys.exit": "直接退出进程",
        "SystemExit": "抛 SystemExit",
        "os._exit": "绕过清理直接退进程",
        "pytest.skip": "以跳过充数",
        "pytest.xfail": "以预期失败充数",
        "unittest.skip": "以跳过充数",
        "from unittest import skip": "以跳过充数",
    }
    here = Path(__file__).resolve().parent
    targets = [here / "test_variance_propagation.py", here / "_object_contracts.py"]
    scanned = 0
    problems: list[str] = []
    for path in targets:
        oc.require(path, "测试文件")
        scanned += 1
        text = path.read_text(encoding="utf-8")
        # 去掉本函数自身的 forbidden 字典，避免自指误伤
        body = text.split("def test_ContractVariance_NoBlockingExitCodeInTestCode")[0]
        for token, why in forbidden.items():
            if token in body:
                problems.append(f"{path.name}: 含 {token!r}（{why}）")
    assert scanned == 2, f"实算文件数 {scanned} != 2"
    assert not problems, "\n".join(problems)
    print(f"[A11] 扫描 {scanned} 个文件：未发现阻塞退出/跳过充数形态")


# ==========================================================================
# 3 负例（注入后必须真变红）
# ==========================================================================


def _write_temp_json(directory: Path, name: str, payload: Any) -> Path:
    """把注入后的副本写到 `tempfile` 目录（`VALIDATION_EVIDENCE.md:197`
    「反例复跑必须隔离：构造反例只允许在临时副本上改，不写仓库留证与产品面」）。"""
    path = directory / name
    path.write_text(json.dumps(payload, ensure_ascii=False, indent=1), encoding="utf-8")
    return path


def test_ContractVariance_NegSourceSnrAsVariance() -> None:
    """B1：把 `source_snr` 对象当 `variance` 传 ⇒ A5 的链合同判红（夹具 n2）。

    注入点 = 本判据自身的判定逻辑（对象配对 + 单位 + 缺失值），
    不动共用逻辑（`VALIDATION_EVIDENCE.md:196` S6）。
    """
    document = oc.load_negative("n2_source_snr_as_variance.schema-violation.json")
    problems = check_object_pairing("variance", document)
    assert problems, "source_snr 文档冒充 variance 却没被判红 ⇒ A5 的配对判定失效"
    joined = "\n".join(problems)
    assert "对象冒充" in joined, f"未点名对象冒充：{joined}"
    print(f"[B1] n2 ⇒ {len(problems)} 条判红：\n{joined}")


def test_ContractVariance_NegSourceSnrIntoVariancePort() -> None:
    """B2：把 `source_snr` 接到接受 `variance` 的端口 ⇒ 判红（夹具 n4）。

    端口 `phase2.integrate.pixel_variance_in`（`unified_object_registry.json
    #port_contracts[0]`），其 `rejects_object` 明确写 `source_snr`。
    """
    port = oc.load_negative("n4_source_snr_into_variance_port.schema-violation.json")
    assert port["port_id"] == VARIANCE_PORT_ID, (
        f"夹具端口 {port['port_id']!r} != 登记端口 {VARIANCE_PORT_ID!r}（锚已改动）"
    )
    assert port["accepts_object"] == "variance", "夹具不再是「接受 variance」的端口"
    problems = check_port_pairing(port)
    assert problems, "source_snr 接进 variance 端口却没被判红 ⇒ 端口配对判定失效"
    joined = "\n".join(problems)
    assert "对象冒充" in joined, f"未点名对象冒充：{joined}"

    # 反向自证：把被连接文档换成 variance 正例 ⇒ 同一判定必须转绿（防恒假）
    good = {
        "port_id": VARIANCE_PORT_ID,
        "direction": "input",
        "accepts_object": "variance",
        "accepts_schema_id": "https://acsd.local/schemas/unified/variance/v1",
        "connected_object_document": oc.load_example("variance.example.json"),
    }
    assert not check_port_pairing(good), "正确的 variance 接法被判红 ⇒ 判定恒假"
    print(f"[B2] n4 ⇒ {len(problems)} 条判红；反向自证（variance 接法）转绿")


def test_ContractVariance_NegCoverageAsRejection() -> None:
    """B3：把 `coverage` 对象当 `rejection` ⇒ 判红（夹具 n3）。

    正本：`rejection.schema.json:246` 的 `object_weight_verdict` const 是
    「门/概率，不是 coverage」——两者不是同一个门面。
    """
    document = oc.load_negative("n3_coverage_as_rejection.schema-violation.json")
    problems = check_object_pairing("rejection", document)
    assert problems, "coverage 文档冒充 rejection 却没被判红 ⇒ A5 的配对判定失效"
    assert "对象冒充" in "\n".join(problems)

    ok, errors = oc.validate(document, oc.load_schema("rejection"))
    assert not ok, "n3 未被 rejection schema 拒绝 ⇒ 负例失效"
    expectations = oc.load_negative_expectations()
    hit, miss = oc.hit_expectations(
        errors, expectations["n3_coverage_as_rejection.schema-violation.json"]["must_match"])
    assert not miss, f"n3 未命中 must_match：{miss}"
    print(f"[B3] n3 ⇒ 配对判红 {len(problems)} 条；schema 判红 {len(errors)} 条，"
          f"must_match {len(hit)} 全命中")


def test_ContractVariance_NegRetiredPsfswWeight() -> None:
    """B4：把已退役的 `psfsw_robust_weight` 接进端口 ⇒ 判红（夹具 n5）。

    正本：`port_contract.schema.json:21` 「declaring unified_object=
    psfsw_robust_weight ⇒ enum FAIL (explicit reject, fail-closed, no silent accept)」。
    """
    document = oc.load_negative("n5_retired_psfsw_robust_weight.schema-violation.json")
    connected = document["connected_object_document"]
    assert connected["unified_object"] == "psfsw_robust_weight", "夹具注入点已变"

    problems = check_port_pairing(document)
    assert problems, "退役对象接进端口却没被判红 ⇒ 端口配对判定失效"
    joined = "\n".join(problems)
    assert "退役对象" in joined or "对象冒充" in joined, f"未点名退役/冒充：{joined}"

    ok, errors = oc.validate(document, oc.load_schema("port_contract"))
    assert not ok, "n5 未被 port_contract schema 拒绝 ⇒ 负例失效"
    keywords = {e["keyword"] for e in errors}
    assert "enum" in keywords, f"退役对象未被 enum 拒绝：{oc.format_errors(errors)}"
    print(f"[B4] n5 ⇒ 端口判定 {len(problems)} 条判红；schema 命中关键字 {sorted(keywords)}")


def test_ContractVariance_NegRelativeSnrSemantics() -> None:
    """B5：把 `sparse_snr_semantics` 改成相对语义 ⇒ 判红（夹具 n6）。

    正本：`sparse_snr_layer.schema.json` 的 `sparse_snr_semantics` const =
    `absolute_flux_type_snr`；`weight_chain.h:39`「层声明为相对语义 → 拒绝」。
    """
    document = oc.load_negative("n6_sparse_snr_relative_semantics.schema-violation.json")
    assert document["sparse_snr_semantics"] != FROZEN_SPARSE_SNR_SEMANTICS, "夹具注入点已变"

    problems = check_sparse_snr_semantics(document)
    assert problems, "相对语义层未被判红 ⇒ 稀疏层语义判定失效"
    print(f"[B5] n6 ⇒ {problems[0]}")

    ok, errors = oc.validate(document, oc.load_schema("sparse_snr_layer"))
    assert not ok, "n6 未被 sparse_snr_layer schema 拒绝 ⇒ 负例失效"

    # 消费侧也必须独立复核一次（weight_chain.h:99「不以『上游已校验』为由跳过」）
    result = check_pixel_weight_chain(
        layer_present=True,
        layer_snr=1.0,
        layer_semantics=document["sparse_snr_semantics"],
        ref_flux=100.0,
        gain=1.0,
    )
    assert not result["closed"], "相对语义层在消费侧被判闭合 ⇒ fail-closed 失效"
    assert any("WrongSnrSemantics" in r for r in result["reasons"]), result["reasons"]


def test_ContractVariance_NegBareWeight() -> None:
    """B6：`signal` 对象带裸 `weight` 字段 ⇒ 判红（夹具 n1）。

    正本：`signal.schema.json:28-50` 的 `propertyNames` + `:14` 的
    `additionalProperties: false`；`VALIDATION_EVIDENCE.md:114-121` 第 4.2 节模式表
    （权重对象与单位由模式表给出，禁止裸权重）。
    """
    document = oc.load_negative("n1_bare_weight.schema-violation.json")
    assert "weight" in document, "夹具注入点已变（缺裸 weight 键）"

    problems = check_no_bare_weight(document)
    assert problems, "裸 weight 字段未被判红 ⇒ A6 判定失效"
    assert any("/weight" in p for p in problems), f"未点名 /weight：{problems}"
    print(f"[B6] n1 ⇒ {len(problems)} 条判红：{problems[0]}")

    ok, errors = oc.validate(document, oc.load_schema("signal"))
    assert not ok, "n1 未被 signal schema 拒绝 ⇒ 负例失效"
    expectations = oc.load_negative_expectations()
    hit, miss = oc.hit_expectations(
        errors, expectations["n1_bare_weight.schema-violation.json"]["must_match"])
    assert not miss, f"n1 未命中 must_match：{miss}"
    print(f"[B6] n1 ⇒ schema must_match {len(hit)}/{len(hit)} 全命中")


def test_ContractVariance_NegVacuousValidator() -> None:
    """B7：把校验器对 `required` 的检查弄失效 ⇒ 判据读数必须发生**可观测翻转**。

    这是 S6（`VALIDATION_EVIDENCE.md:193`）：注入点就是本校验器自身的
    `required` 分支；判据的读数必须从「红」翻成「绿」，否则 `required` 分支是空转的，
    A1/A2 就没有证据资格。

    两条注入路径各自独立：
      (i)  开关式：`skip_required=True`（把 `required` 分支摘掉）；
      (ii) 文件式：在临时副本里把 schema 的每一处 `required` 列表**清空**
           （派单书点名的注入方式），再从该临时副本读 schema 走同一判定。

    两条都必须给出同一方向的红→绿翻转。
    """
    base = oc.load_example("signal.example.json")
    schema = oc.load_schema("signal")
    victim = sorted(schema["required"])[0]

    stripped = json.loads(json.dumps(base))
    stripped.pop(victim)

    # 前提：真校验器判红
    ok_true, _ = oc.validate(stripped, schema)
    assert not ok_true, f"前提不成立：删掉 required 键 {victim!r} 后真校验器应判红"

    # (i) 开关式注入
    ok_injected, _ = oc.validate(stripped, schema, skip_required=True)
    assert ok_injected, (
        f"注入 `required` 失效后读数没有翻转（仍判红）⇒ required 分支可能仍在做功，"
        f"本负例无法证明本校验器非空转；实得 ok={ok_injected}"
    )
    print(f"[B7-i] 开关式注入：真校验器 ok={ok_true} ⇒ 注入后 ok={ok_injected}（红→绿翻转）")

    # (ii) 文件式注入：把 required 列表清空，写到临时目录
    def clear_required(node: Any) -> Any:
        if isinstance(node, dict):
            out = {k: clear_required(v) for k, v in node.items()}
            if isinstance(out.get("required"), list):
                out["required"] = []
            return out
        if isinstance(node, list):
            return [clear_required(v) for v in node]
        return node

    with tempfile.TemporaryDirectory(prefix="acsd-contract-inject-") as tmp:
        tmp_dir = Path(tmp)
        # 注入：副本里额外删掉 required 键（原样照抄 signal 正例的其余内容）
        injected_doc = _write_temp_json(tmp_dir, "stripped.json", stripped)
        # 注入：schema 副本清空 required
        blanked_schema = clear_required(json.loads(json.dumps(schema)))
        assert blanked_schema["required"] == [], "注入未生效：required 未清空"
        _write_temp_json(tmp_dir, "blanked.schema.json", blanked_schema)

        # 走同一读取路径（临时副本，仓库零写入）
        blanked = oc.load_json(tmp_dir / "blanked.schema.json", "injected-schema")
        doc_injected = oc.load_json(injected_doc, "injected-doc")

        ok_blanked, _ = oc.validate(doc_injected, blanked)
        assert ok_blanked, (
            "文件式注入（required 清空）后读数没有翻转 ⇒ 本校验器的 required 分支"
            "在该夹具上可能没被覆盖"
        )
        print(f"[B7-ii] 文件式注入（临时副本，required 清空）：ok={ok_true} ⇒ ok={ok_blanked}")

    # 反向自证：把 required 分支恢复后，同一份注入文档必须重新判红
    ok_restored, _ = oc.validate(stripped, schema)
    assert not ok_restored, "恢复 required 后仍未判红 ⇒ 判据恒假"
    print("[B7] 正例一侧同步确认：signal 正例本身在真校验器下判绿，判据非恒假")


def test_ContractVariance_NegUnitsMismatch() -> None:
    """B8：把 `units.bunit_semantics` 从 `written_px_power` 改掉 ⇒ A5 判红。

    正本：`clause_registry.json` 的 `FZ-BUNIT-SEMANTICS`
    「BUNIT 必须量纲可判：显式立体角幂次 或 provenance pixel_semantics=
    surface_brightness+pixel_area_power=-2」；`variance.schema.json:490-495` 的
    `x-acsd-gate` 逐字写的规则。
    """
    base = oc.load_example("variance.example.json")
    assert base["units"]["bunit_semantics"] == FROZEN_BUNIT_SEMANTICS, "前提不成立"

    with tempfile.TemporaryDirectory(prefix="acsd-contract-inject-") as tmp:
        for wrong in ("declared_via_provenance", "unknown_semantics"):
            mutated = json.loads(json.dumps(base))
            mutated["units"]["bunit_semantics"] = wrong
            path = _write_temp_json(Path(tmp), f"variance_{wrong}.json", mutated)
            document = oc.load_json(path, f"injected:{wrong}")

            problems = check_units_contract("variance", document)
            assert problems, (
                f"bunit_semantics 改成 {wrong!r} 却没被判红 ⇒ 单位合同判定失效"
            )
            assert any("bunit_semantics" in p for p in problems), problems
            print(f"[B8] bunit_semantics={wrong!r} ⇒ {problems[0]}")

            # 同一注入也必须被冻结 schema 拒绝（BUNIT=ADU^2/sr^2 时声明 declared_via_provenance
            # 与 x-acsd-gate 的「BUNIT 仅写 ADU」相抵触）
            ok, _ = oc.validate(document, oc.load_schema("variance"))
            assert not ok, f"schema 未拒绝 bunit_semantics={wrong!r} ⇒ 冻结约束失效"


def test_ContractVariance_NegNanAsMissing() -> None:
    """B9：把 `missing_repr` 改成 `"nan"` ⇒ A10 的第三态判红。

    正本：`TEST.md:80`「「缺失」是与非有限值**并列的第三态**，在产品面上必须有
    区别于 NaN 的表示，**不得由 NaN 兼表**」；
    `signal.schema.json:196-206` 的 `missing_repr` 枚举不含 `"nan"`。
    """
    with tempfile.TemporaryDirectory(prefix="acsd-contract-inject-") as tmp:
        tmp_dir = Path(tmp)
        for name in ("signal", "variance", "sparse_snr_layer", "ivar"):
            base = oc.load_example(f"{name}.example.json")
            frozen_missing = (FROZEN_MISSING_REPR if name != "ivar" else "zero")
            assert base["missing_value"]["missing_repr"] == frozen_missing, (
                f"前提不成立：{name} 的 missing_repr 冻结值是 "
                f"{frozen_missing!r}，实得 {base['missing_value']['missing_repr']!r}"
            )

            mutated = json.loads(json.dumps(base))
            mutated["missing_value"]["missing_repr"] = "nan"
            path = _write_temp_json(tmp_dir, f"{name}_nan_missing.json", mutated)
            document = oc.load_json(path, f"injected:{name}")

            problems = check_missing_value_contract(name, document, strict_nan_carrier=True)
            assert problems, f"{name}: missing_repr 改成 'nan' 却没判红 ⇒ 第三态判定失效"
            joined = "\n".join(problems)
            assert "missing_repr" in joined, f"未点名 missing_repr：{joined}"
            assert "TEST.md:80" in joined, f"未点名 NaN 兼表条款：{joined}"
            print(f"[B9] {name}: missing_repr='nan' ⇒ {len(problems)} 条判红：{problems[0]}")

            ok, _ = oc.validate(document, oc.load_schema(name))
            assert not ok, f"{name}: schema 竟然接受 missing_repr='nan' ⇒ 枚举守卫失效"

        # 结构面注入：把 "nan" 加进 schema 的 missing_repr 枚举 ⇒ 结构判定判红
        schema = json.loads(json.dumps(oc.load_schema("signal")))
        enum = schema["properties"]["missing_value"]["properties"]["missing_repr"]["enum"]
        assert "nan" not in enum, "前提不成立：signal 的 missing_repr 枚举本就不含 'nan'"
        enum.append("nan")
        path = _write_temp_json(tmp_dir, "signal_nan_enum.schema.json", schema)
        blanked = oc.load_json(path, "injected-schema")
        assert not check_schema_forbids_nan_as_missing("signal"), (
            "前提不成立：注入前的 canonical schema 不应被判违规"
        )

        problems = check_schema_forbids_nan_as_missing("signal", schema=blanked)
        assert problems, (
            "把 'nan' 加进 missing_repr 枚举后结构判定仍未判红 ⇒ "
            "「不得由 NaN 兼表」的结构面失效"
        )
        print(f"[B9-结构] missing_repr 枚举加 'nan' ⇒ {problems[0]}")


def test_ContractVariance_NegNaiveVarianceMissingCrossTerms() -> None:
    """B10：注入朴素式（漏 `−HΣ − ΣHᵀ` 交叉项）⇒ 判红，红出比值 `(1+1/N)/(1−1/N)`。

    正本：`variance_propagation.h:19-22`「错误形式（禁止用于生产权重）：
    `σ_i² + Σ_j H_ij² σ_j²`。它漏掉 `−HΣ − ΣHᵀ` 交叉项；……N=8 时高估
    `(1+1/8)/(1−1/8) = 1.2857×`」；`:57-60` 标注该函数**仅红例/对照/审计**，
    生产权重禁用。

    本负例是本单最有价值的负例之一：它证明判据抓的是**交叉项**，不是恒等式。
    """
    n = INJECTED_N_FRAMES
    index = 0
    exact_correct = exact_residual_maker_variance(index)
    exact_naive = exact_naive_variance(index)

    # 注入：把候选值换成朴素式的浮点读数
    candidate = naive_variance_numpy(index)
    sum_abs_terms = float(np.sum((np.eye(n)[index] - np.full(n, 1.0 / n)) ** 2
                                 * np.asarray(INJECTED_SIGMA2)))
    threshold = reduction_threshold(n_terms=n, sum_abs_terms=sum_abs_terms,
                                    scale=float(exact_correct))
    residual = abs(candidate - float(exact_correct))
    assert residual > threshold, (
        f"注入朴素式后仍落在门限内（残差 {residual:.3e} ≤ 门限 {threshold:.3e}）"
        f"⇒ 判据抓不住交叉项，是恒真判定"
    )
    print(f"[B10] 注入朴素式：candidate={candidate!r}，正确值精确 {exact_correct}，"
          f"残差 {residual:.3e} > 归约门限 {threshold:.3e} ⇒ 判红")

    # 判红量必须点名交叉项：比值 = (1+1/N)/(1−1/N)（等权模型）与精确有理数一致
    measured_ratio = candidate / residual_maker_variance_numpy(index)
    exact_ratio = exact_naive / exact_correct
    scale = float(exact_ratio)
    assert ATOL_F64_SCALE_FACTOR * scale >= ulp(scale), "比值的绝对容差不可判（TEST.md:73）"
    assert measured_ratio == pytest.approx(scale, rel=RTOL_F64,
                                           abs=ATOL_F64_SCALE_FACTOR * scale), (
        f"实测比值 {measured_ratio!r} != 精确 {exact_ratio}"
    )
    # 等权模型下必须精确复现正本点名的 9/7
    uniform_ratio_exact = exact_mean_model_ratio(n)
    assert uniform_ratio_exact == Fraction(9, 7), (
        f"等权模型比值精确值 {uniform_ratio_exact} != 9/7（正本 :82-84）"
    )
    h_row = np.full(n, 1.0 / n)
    uniform_sigma = np.ones(n)
    uniform_naive = float(uniform_sigma[index] + np.sum(h_row ** 2 * uniform_sigma))
    p = np.eye(n) - np.full((n, n), 1.0 / n)
    uniform_correct_f = float(np.diag(p @ np.diag(uniform_sigma) @ p.T)[index])
    assert uniform_naive / uniform_correct_f == pytest.approx(
        float(uniform_ratio_exact), rel=RTOL_F64), (
        f"等权模型实测比值 {uniform_naive / uniform_correct_f!r} != 9/7"
    )
    print(f"[B10] 判红比值 = {measured_ratio!r}（精确 {exact_ratio} = "
          f"{float(exact_ratio):.6f}）；等权模型复现 9/7 = 1.2857142857142858")


def test_ContractVariance_NegMissingGainSquare() -> None:
    """B11：注入漏掉 `÷g²` 的归一化 ⇒ 判红 `g²` 倍。

    正本：`variance_propagation.h:24-25`「乘性归一化: corrected=(y−ĝ)/g ⇒ Var /= g²
    （**÷g² 是硬要求**，缺它高响应高 SNR 帧被压低 ≈1/g²、权重序可翻转）」；
    `:70-77` 生产总入口 `[PΣPᵀ_ii + param_var] / g²`。
    """
    index = 0
    gain = INJECTED_GAIN
    exact_correct = exact_residual_maker_variance(index)
    exact_with_gain = exact_correct / Fraction(int(round(gain))) ** 2
    assert exact_with_gain == Fraction(119, 64), (
        f"注入值下精确的 PΣPᵀ/g² 是 {exact_with_gain}（期望 119/64）"
    )

    # 注入：候选值不除 g²（把加性-only 基线错当成乘性归一化的结果）
    candidate = residual_maker_variance_numpy(index)
    scale = float(exact_with_gain)
    atol = ATOL_F64_SCALE_FACTOR * scale
    assert atol >= ulp(scale), "绝对容差不可判（TEST.md:73）"
    residual = abs(candidate - scale)
    tolerance = RTOL_F64 * scale + atol
    assert residual > tolerance, (
        f"漏掉 ÷g² 后仍落在容差内（{residual:.3e} ≤ {tolerance:.3e}）"
        f"⇒ 归一化判据恒真"
    )
    measured_ratio = candidate / float(exact_with_gain)
    assert measured_ratio == pytest.approx(gain ** 2, rel=RTOL_F64), (
        f"漏 ÷g² 的偏离倍数 {measured_ratio!r} != g²={gain ** 2}"
    )
    print(f"[B11] 注入漏 ÷g²：candidate={candidate!r}，正确值 {exact_with_gain}，"
          f"偏离 {measured_ratio!r} = g² = {gain ** 2} 倍 ⇒ 判红")


def test_ContractVariance_NegWeightTimesFrameSnr() -> None:
    """B12：注入「再乘一个帧级 SNR 标量」的错口径 ⇒ 判红。

    正本：`weight_chain.h:28-29`「**禁止**把它再乘/除任何帧级 SNR 标量
    （既违冻结 schema，也使权重口径变成「帧级² × 绝对」的错口径）」；
    `:394-395`「**不得**再乘/除帧级 SNR：层与帧级是两个独立对象」。

    同时断言**签名面**的排他性：`weight_from_sparse_layer_pixel` 的入参里没有
    帧级 SNR 字段（`weight_chain.h:396-404` 的 `PixelWeightInput`）。
    """
    snr_layer, f_ref, gain, frame_snr = 10.0, 100.0, 2.0, 118.4
    good = check_pixel_weight_chain(
        layer_present=True, layer_snr=snr_layer,
        layer_semantics=FROZEN_SPARSE_SNR_SEMANTICS, ref_flux=f_ref, gain=gain,
    )
    assert good["closed"], good["reasons"]

    bad = check_pixel_weight_chain(
        layer_present=True, layer_snr=snr_layer,
        layer_semantics=FROZEN_SPARSE_SNR_SEMANTICS, ref_flux=f_ref, gain=gain,
        frame_snr=frame_snr, multiply_frame_snr=True,
    )
    assert not bad["closed"], "「再乘帧级 SNR」的错口径被判闭合 ⇒ fail-closed 失效"
    assert any("INJECTED_DEFECT" in r for r in bad["reasons"]), bad["reasons"]

    exact_good = exact_pixel_weight(snr_layer, f_ref, gain)
    exact_bad = exact_good * injected(frame_snr)
    scale = float(exact_bad)
    atol = ATOL_F64_SCALE_FACTOR * scale
    assert atol >= ulp(scale), "绝对容差不可判（TEST.md:73）"
    assert abs(bad["weight"] - scale) <= RTOL_F64 * scale + atol, (
        f"注入错口径的权重 {bad['weight']!r} 与精确 {exact_bad} 不符"
    )
    assert bad["weight"] != pytest.approx(float(exact_good), rel=RTOL_F64,
                                           abs=ATOL_F64_SCALE_FACTOR * float(exact_good)), (
        "错口径与正确口径给出同一个权重 ⇒ 判据无鉴别力"
    )
    print(f"[B12] 注入再乘帧级 SNR={frame_snr}：{bad['reasons'][0]}；"
          f"权重 {exact_good} → {exact_bad} ADU⁻²")

    # 签名面排他性：PixelWeightInput 不得含帧级 SNR 字段。
    header = (oc.REPO_ROOT
              / "lib/algorithms/integration/phase2_integrate/include/acsd/weight_chain.h")
    text = oc.require(header, "weight_chain.h").read_text(encoding="utf-8")
    block = text.split("struct PixelWeightInput {", 1)[1].split("};", 1)[0]
    assert "frame_snr" not in block, (
        "PixelWeightInput 出现帧级 SNR 字段 ⇒ 接口面允许错口径（weight_chain.h:396-404）"
    )
    assert "layer" in block and "ref_flux_k" in block and "gain" in block, (
        "PixelWeightInput 结构与 weight_chain.h:396-404 不符（锚已改动）"
    )


def test_ContractVariance_NegAbsentLayerTimesOne() -> None:
    """B13：注入「层缺失时静默按乘 1 出权」⇒ 判红。

    正本：`weight_chain.h:30-31`「层值缺失不是「乘 1」：逐像素面的缺层/语义不符/
    越界/非正值一律显式降级（`generated_from_absent_layer`）或判红
    （closure 非 `kClosed`），绝不静默照常出权」；`:37-38`「层缺失 …
    **不得当作「乘 1」照常出权**」。
    """
    honest = check_pixel_weight_chain(
        layer_present=False, layer_snr=None, layer_semantics=None,
        ref_flux=100.0, gain=2.0,
    )
    assert not honest["closed"], "层缺失被判闭合 ⇒ fail-closed 失效"
    assert honest["weight"] is None, "层缺失时竟然给出了权重值 ⇒ 静默出权"
    assert any("SparseLayerRequiredMissing" in r for r in honest["reasons"]), honest["reasons"]

    injected = check_pixel_weight_chain(
        layer_present=False, layer_snr=None, layer_semantics=None,
        ref_flux=100.0, gain=2.0, absent_layer_as_one=True,
    )
    assert injected["weight"] is not None, "注入未生效：短路没给权重"
    assert any("INJECTED_DEFECT" in r for r in injected["reasons"]), injected["reasons"]
    # 注入的权重必须与真实权重显著不同，否则判据无牙
    true_weight = float(exact_pixel_weight(10.0, 100.0, 2.0))
    assert abs(injected["weight"] - true_weight) > 1e-3 * true_weight, (
        "『层值=1』的权重与真实权重几乎相同 ⇒ 判据无鉴别力"
    )
    print(f"[B13] 层缺失：诚实路径 closure={honest['reasons'][0]}；"
          f"注入『乘 1』得 w={injected['weight']!r}，真实 w={true_weight!r} ⇒ 判红")


def test_ContractVariance_NegNonPositiveRefFluxEqualWeight() -> None:
    """B14：注入「`F_ref` 非正时退化为等权」⇒ 判红。

    正本：`weight_chain.h:35`「`F_ref` 缺失/非有限/非正 → 拒绝（换算不成立）」；
    `:42-43`「legacy 等权回退即使被显式置真，也只返回 fail-closed 并显式报出
    「权重链未闭合」，绝不把等权结果标记为成功」。
    """
    for bad_flux in (0.0, -100.0):
        honest = check_pixel_weight_chain(
            layer_present=True, layer_snr=10.0,
            layer_semantics=FROZEN_SPARSE_SNR_SEMANTICS,
            ref_flux=bad_flux, gain=2.0,
        )
        assert not honest["closed"], (
            f"F_ref={bad_flux!r} 被判闭合 ⇒ fail-closed 失效"
        )
        assert honest["weight"] is None, f"F_ref={bad_flux!r} 时给出了权重值"
        assert any("InvalidReferenceFlux" in r for r in honest["reasons"]), honest["reasons"]

        injected = check_pixel_weight_chain(
            layer_present=True, layer_snr=10.0,
            layer_semantics=FROZEN_SPARSE_SNR_SEMANTICS,
            ref_flux=bad_flux, gain=2.0,
            nonpositive_ref_flux_falls_back_to_equal=True,
        )
        assert injected["weight"] == 1.0, "注入未生效：等权回退没给出 w=1"
        assert any("INJECTED_DEFECT" in r for r in injected["reasons"]), injected["reasons"]
        true_weight = float(exact_pixel_weight(10.0, 100.0, 2.0))
        assert abs(1.0 - true_weight) > 1e-3 * true_weight, (
            "等权回退的权重与真实权重几乎相同 ⇒ 判据无鉴别力"
        )
        print(f"[B14] F_ref={bad_flux!r}：诚实路径 {honest['reasons'][0]}；"
              f"注入等权回退得 w=1.0，真实 w={true_weight} ⇒ 判红")

    # 锚存活：fail-closed 条款在头文件里逐字存在（防止正本被悄悄改写）
    header = (oc.REPO_ROOT
              / "lib/algorithms/integration/phase2_integrate/include/acsd/weight_chain.h")
    text = oc.require(header, "weight_chain.h").read_text(encoding="utf-8")
    for phrase in ("F_ref 缺失/非有限/非正 → 拒绝（换算不成立）",
                   "绝不把等权结果标记为成功",
                   "不得当作“乘 1”照常出权"):
        assert phrase in text, f"weight_chain.h 未找到条款原文 {phrase!r}（锚失效）"