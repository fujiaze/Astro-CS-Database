"""端到端层 · 标定输入面测试（母版可达性、增益/读噪来源显式性、XISF 读取的 fail-closed 面）。

## 用途

在**端到端层**判定真实观测的**标定输入面**是否成立：27 个母版是否在位、是否可解析、
几何是否与同望远镜亮场一致、平场滤镜是否覆盖、暗场曝光是否有缺口、以及
**电子学量（增益 / 读噪 / 饱和）的来源是否被显式登记**。本文件是 **A 类**。

## ⚠ XISF 门槛裁定（本单第一个决策点，见本文件 §XISF 裁定 与 `_realdata` docstring）

派单给出三条路：(a) 用 `build/acsd` 产品 IO 面读母版；(b) 自写 XISF 1.0 最小解析器；
(c) 母版不在本层职责内，增益/读噪一律从配置读取并标注为外部输入。

**本层裁定 = (b) + (c)，(a) 否决**：

- **否决 (a)**：它把**整个标定输入面**变成「需构建」，也就是把这一层的全部证据价值
  归零；且派单本身**禁止本车道构建与运行产品**。更关键的是，(a) 下 Python 侧
  拿不到任何可复核的中间读数，解析器的正确性也无从在纯 Python 面证伪。
- **采纳 (b)，但收窄到「头 + 未压缩附加块」**：实测 27 个母版全部是
  `sampleFormat="Float32"`、`location="attachment:<off>:<size>"`、**未压缩**，
  声明字节数恒等于 `W·H·C·4`（27/27 逐个相符）。字节布局**不发明**，逐条对照仓内
  一手 C++ 读入器 `lib/infrastructure/aio/src/aio_xisf.cpp`（魔数 `:25,447`；
  64 位小端头长 `:452-460`；上限 64 MiB `:29,467`；`<Image>` 属性 `:158-181`；
  `attachment:` `:91-98`；几何上界 `:33`）。判据 **C6** 把本解析器的常量与那份
  `.cpp` **源码文本**逐条对撞，两者是独立面。
- **(c) 不是可选项而是必然**：实测**亮场 353 帧与母版 27 个都不携带任何**
  `GAIN/EGAIN/READNOISE/RDNOISE/SATURATE/DATAMAX`；三份 `phase_config_*.schema.json`
  也**没有 `gain` 键**。⇒ 增益与读噪在仓内**没有任何承载面**，只能作外部输入。
  饱和电平另有合同规定的三级来源优先级，三级皆无 ⇒ 显式 `DISABLED_NO_METADATA`，禁静默
  （`phase_config_normalize.schema.json#/$defs/noise_config/properties/saturation_level`）。
  本层的职责是把这件事**做成可判读的漂移守卫**（C6'），而不是替它编一个数。

## 正本依据（逐条 `文件:行`）

| 判据 | 正本 |
|---|---|
| 饱和电平来源优先级 + 三级皆无 ⇒ `DISABLED_NO_METADATA` | `eng/contracts/schemas/phase_config_normalize.schema.json#/$defs/noise_config/properties/saturation_level` |
| 跨阶段唯一载体 = HiPS 产品树（本层只判输入面，不判产物） | `docs/engineering/contracts/PIPELINE_BLOCK.md:20` |
| 容差档（元数据/计数 = 精确一致） | `docs/engineering/testing/TEST.md:46` |
| 不得把缺失折叠成哨兵值后再参与比较 | `docs/engineering/testing/TEST.md:75-82` |
| 每个度量须有非退化判据 | `docs/engineering/testing/TEST.md:26` |
| 判据必须能红；注入缺陷时读数不红即失效 | `docs/engineering/testing/TEST.md:62` |
| 锚存活 fail-closed / 零对象守卫 | `docs/engineering/testing/VALIDATION_EVIDENCE.md:412-413,170-176` |
| 判别力 S2 / S6 / 反例隔离 | `docs/engineering/testing/VALIDATION_EVIDENCE.md:189,193,197` |

## 独立 Oracle 来源

预期值不取自 `testdata/index.json`；来自 (a) 母版**文件名**独立抽出的类型/曝光/滤镜；
(b) 母版 **XISF 头**内嵌 `<FITSKeyword>`；(c) 亮场 **FITS 头**逐帧实算；
(d) 仓内 C++ 读入器**源码文本**的常量。`index.json` 是被对账对象（D10，在数据面文件）。

## 命名映射（`Suite.Feature` → 函数名；`TEST.md:88`）

| `Suite.Feature` | 函数名 | 类别 |
|---|---|---|
| `CALIB.Inputs.MastersReachableAndParsable` | `test_CALIB_Inputs_MastersReachableAndParsable` | 正例 C1/C2 |
| `CALIB.Inputs.MasterGeometryMatchesLights` | `test_CALIB_Inputs_MasterGeometryMatchesLights` | 正例 C3 |
| `CALIB.Inputs.FlatFilterCoverageComplete` | `test_CALIB_Inputs_FlatFilterCoverageComplete` | 正例 C5 |
| `CALIB.Inputs.DarkExposureGapMatchesRegistration` | `test_CALIB_Inputs_DarkExposureGapMatchesRegistration` | 漂移守卫 C4 |
| `CALIB.Inputs.ElectronicsAreExternalInputs` | `test_CALIB_Inputs_ElectronicsAreExternalInputs` | 漂移守卫 C6' |
| `CALIB.Inputs.SaturationFallsBackToDisabledNoMetadata` | `test_CALIB_Inputs_SaturationFallsBackToDisabledNoMetadata` | 正例 C7 |
| `CALIB.Inputs.XisfByteLayoutMatchesRepoCpp` | `test_CALIB_Inputs_XisfByteLayoutMatchesRepoCpp` | 正例 C8 |
| `CALIB.Inputs.XisfPixelsMatchDeclaredGeometry` | `test_CALIB_Inputs_XisfPixelsMatchDeclaredGeometry` | 正例 C9 |
| `CALIB.Inputs.NegXisfDefectsRaiseNamedErrors` | `test_CALIB_Inputs_NegXisfDefectsRaiseNamedErrors` | 负例 N-C1（fail-closed，9 种注入） |
| `CALIB.Inputs.NegFlatFilterMissingRed` | `test_CALIB_Inputs_NegFlatFilterMissingRed` | 负例 N-C2（S6） |
| `CALIB.Inputs.NegDarkGapDetectorHasTeeth` | `test_CALIB_Inputs_NegDarkGapDetectorHasTeeth` | 负例 N-C3（S6） |
| `CALIB.Inputs.NegElectronicsKeywordAppearsRed` | `test_CALIB_Inputs_NegElectronicsKeywordAppearsRed` | 负例 N-C4（S6） |

## 不产出阻塞退出码

不注册进 CMakeLists、不接 CI；不出现 `sys.exit` / `raise SystemExit` / `os._exit`。
判红是**缺陷信号**，由人读对抗性审核消费，**本层不裁决代码**。
"""

from __future__ import annotations

import json
import re
from typing import Dict, List

import numpy as np
import pytest

import _realdata as rd

#: C8 的独立面：本层解析器常量必须与仓内 C++ 读入器**源码文本**逐条对撞。
#: 键 = 本层常量名，值 = 在 `.cpp` 源��里必须逐字出现的正则。
CPP_ANCHOR = "lib/infrastructure/aio/src/aio_xisf.cpp"


def _schema_declares_key(schema: Dict, pointer: str, key: str) -> bool:
    """解析 JSON Schema 的 `$ref` / `allOf` / `anyOf` 组合，判断 `pointer` 下是否**声明**了 `key`。

    ⚠ **不能**用「schema 文本里出现过该字符串」来判断：契约描述里逐字写着
    `noise.saturation_level > snr.saturation_level > ...`，纯文本搜索会把
    「文档提到」误判成「键存在」。本函数只看**键的面**。
    """
    defs = schema.get("$defs", {})

    def resolve(node) -> bool:
        if not isinstance(node, dict):
            return False
        if key in (node.get("properties") or {}):
            return True
        ref = node.get("$ref")
        if isinstance(ref, str) and ref.startswith("#/$defs/"):
            if resolve(defs.get(ref.split("/")[-1])):
                return True
        for combiner in ("allOf", "anyOf", "oneOf"):
            for sub in node.get(combiner) or []:
                if resolve(sub):
                    return True
        return False

    node: object = schema
    for part in [p for p in pointer.split("/") if p]:
        if not isinstance(node, dict) or part not in node:
            return False
        node = node[part]
        ref = node.get("$ref") if isinstance(node, dict) else None
        if isinstance(ref, str) and ref.startswith("#/$defs/"):
            node = defs.get(ref.split("/")[-1], {})
    return resolve(node)


# ---------------------------------------------------------------------------
# 正例 / 漂移守卫（A 类）
# ---------------------------------------------------------------------------

@pytest.mark.parametrize("master_dir_name", rd.MASTER_DIR_NAMES)
def test_CALIB_Inputs_MastersReachableAndParsable(master_dir_name: str) -> None:
    """C1/C2 母版可达与可解析：目录在位、实算母版数与冻结登记一致、
    每个母版的 XISF 头可解析、`geometry` 与登记一致、`<FITSKeyword>` 非空。

    判红含义：`ANCHOR_STALE`（目录没了）/ `ZERO_OBJECT_GUARD`（目录空）/
    `XISF_*`（文件坏了）/ 几何漂移（母版换了画幅）。

    **来源依据**：`VALIDATION_EVIDENCE.md:413`、`:170-176`；XISF 1.0 规范。
    """
    viol = rd.judge_master_reachable_and_parsable(master_dir_name)
    assert viol == [], f"C1/C2 母版判红（{master_dir_name}）：{viol}"


@pytest.mark.parametrize("dataset_id", rd.DATASET_IDS)
def test_CALIB_Inputs_MasterGeometryMatchesLights(dataset_id: str) -> None:
    """C3 母版几何 ↔ 同望远镜亮场几何：两条**独立读面**（母版 XISF 头 vs 亮场 FITS 头）
    逐母版对照。银心 ⇒ T4 母版 4500×3600；M42 ⇒ T2/T3 母版 4096×4096。

    判红含义：母版画幅与亮场不配（母版配错望远镜），或某望远镜没有任何亮场。

    **来源依据**：FITS `NAXIS1/NAXIS2` 与 XISF `geometry`；XISF 1.0 规范。
    """
    viol = rd.judge_master_geometry_matches_lights(dataset_id)
    assert viol == [], f"C3 母版/亮场几何判红（{dataset_id}）：{viol}"


@pytest.mark.parametrize("dataset_id", rd.DATASET_IDS)
def test_CALIB_Inputs_FlatFilterCoverageComplete(dataset_id: str) -> None:
    """C5 平场滤镜覆盖（**当前无缺口**，故写成绿的正例判据）：
    对每个 `(数据集, 望远镜, 亮场 FILTER)`，必须存在**同望远镜**且 FILTER **精确匹配**的
    masterFlat。

    ⚠ 匹配按精确串，**不做大小写归一**：T2 的 OIII 母版写 `'OIII'`、T3/T4 写 `'Oiii'`，
    两套写法本身是一处已登记的口径差异（README REG-03）。
    M42 实际用到 H-alpha/Blue/Green/Red、银心用到 H-alpha/Blue/Green/Oiii/Red，两侧均已覆盖。

    **来源依据**：母版**文件名**的 `FILTER-` 段（独立面）vs 亮场**头**的 `FILTER`。
    """
    viol = rd.judge_flat_filter_coverage(dataset_id)
    assert viol == [], f"C5 平场滤镜覆盖判红（{dataset_id}）：{viol}"


@pytest.mark.parametrize("dataset_id", rd.DATASET_IDS)
def test_CALIB_Inputs_DarkExposureGapMatchesRegistration(dataset_id: str) -> None:
    """C4 暗场曝光缺口的**漂移守卫**（REG-02）。

    实测缺口：`m42` 的 `T2` 缺 300 s（**53 帧**）、`T3` 缺 300 s（**94 帧**），
    合计 **147 帧 / 353 帧**（41.6%）没有同曝光的 masterDark 可用；`galaxy_center` 无缺口。
    本判据断言缺口**签名**与冻结登记精确一致。

    ⚠ 缺口**非零**是数据缺陷的真实读数（`TEST.md:62`「判据必须能红」）。
    处置见 README REG-02：登记、不改数据、不降级阈值。
    本层**不**把它写成「期望缺口为空」的绿用例——那等于要求数据没这个缺陷，等于降级判据；
    也不写成一条永远红的用例——那会让本目录长期带红且无裁决主体。
    与模块层 REG-02/REG-03 同一处置体例：**登记 + 漂移守卫**。

    **来源依据**：母版**文件名**的 `EXPOSURE-` 段（独立面）vs 亮场**头**的 `EXPTIME`。
    """
    gaps = rd.judge_dark_exposure_coverage(dataset_id)
    registered = rd.REGISTERED_DARK_EXPOSURE_GAPS[dataset_id]
    assert gaps == registered, (
        f"C4 暗场曝光缺口签名漂移（{dataset_id}）：实测 {gaps}，登记 {registered}；"
        "处置见 eng/tests/e2e/README.md REG-02"
    )
    # 缺口必须被**点名**（含具体帧数），而不是被跳过。
    total = sum(n for per_tel in gaps.values() for n in per_tel.values())
    expected_total = sum(
        n for per_tel in registered.values() for n in per_tel.values()
    )
    assert total == expected_total, (
        f"C4 缺口帧数未点名：实测合计 {total}，登记合计 {expected_total}"
    )


def test_CALIB_Inputs_ElectronicsAreExternalInputs() -> None:
    """C6' 电子学量为**外部输入**这一登记的漂移守卫（REG-03）。

    逐帧（353）+ 逐母版（27）实算 `GAIN/EGAIN/READNOISE/RDNOISE/SATURATE/DATAMAX`
    的携带情况；任一关键词出现即判红——那意味着仓内**长出了**承载面，
    「该量为外部输入」的登记必须重做，不能让增益/读噪在两个口径下同时活着。

    同时断言三份 `phase_config_*.schema.json` **没有 `gain` 键**（配置面同样无承载）。

    **来源依据**：`TEST.md:26` 非退化 + 本层 `ELECTRONICS_ABSENT_KEYWORDS` 冻结登记。
    """
    viol = rd.judge_electronics_absent()
    assert viol == [], f"C6' 电子学量承载面漂移：{viol}"
    scan = rd.scan_electronics_keywords()
    assert scan["scanned_lights"] == 353, (
        f"C6' 实扫亮场数漂移：{scan['scanned_lights']}，登记 353"
    )
    assert scan["scanned_masters"] == 27, (
        f"C6' 实扫母版数漂移：{scan['scanned_masters']}，登记 27"
    )
    for phase in ("normalize", "mosaic", "export"):
        rel = f"eng/contracts/schemas/phase_config_{phase}.schema.json"
        text = rd.anchor_file(rel, f"PHASE_CONFIG_SCHEMA[{phase}]")
        with open(text, "r", encoding="utf-8") as fh:
            body = fh.read()
        assert '"gain"' not in body, (
            f"C6' 配置面长出了 gain 键（{rel}）：仓内**已有**电子学增益承载面，"
            "REG-03『外部输入』登记失效，须重做裁定"
        )


def test_CALIB_Inputs_SaturationFallsBackToDisabledNoMetadata() -> None:
    """C7 饱和电平落在合同规定的**末支** `DISABLED_NO_METADATA`（不是 0、不是静默）。

    合同冻结的来源优先级（`phase_config_normalize.schema.json#/$defs/noise_config/
    properties/saturation_level` 逐字）：
    `noise.saturation_level` > `snr.saturation_level` > FITS 头 `SATURATE`/`DATAMAX`；
    **三级皆无 ⇒ 显式 `DISABLED_NO_METADATA`，禁静默**。

    逐级实测三级是否真的都缺：(1) normalize 配置模板里无 `saturation_level`；
    (2) 配置 schema 里无 `snr.saturation_level`；(3) 353 帧无 `SATURATE`/`DATAMAX`。
    三级皆缺 ⇒ 断言本层登记的 `declared_external_inputs()` 把末支**写死**为
    `DISABLED_NO_METADATA`，且把来源优先级**逐条列出**（不是一句「从配置读」）。

    **来源依据**：`phase_config_normalize.schema.json` 的 `saturation_level` 描述逐字；
    `TEST.md:75-82`（缺失不得折叠成哨兵值）。
    """
    template_rel = "eng/packaging/config/templates/normalize.phase_config.json"
    with open(rd.anchor_file(template_rel, "NORMALIZE_TEMPLATE"), "r", encoding="utf-8") as fh:
        template_body = fh.read()
    schema_rel = "eng/contracts/schemas/phase_config_normalize.schema.json"
    with open(rd.anchor_file(schema_rel, "NORMALIZE_SCHEMA"), "r", encoding="utf-8") as fh:
        schema_body = fh.read()

    schema = json.loads(schema_body)
    level1 = "saturation_level" in template_body
    level2 = _schema_declares_key(schema, "/properties/snr", "saturation_level")
    level3 = any(
        kw in rd.read_frame_header(rel)
        for rel in rd.frame_relpaths("m42")
        for kw in ("SATURATE", "DATAMAX")
    )
    # 先证明解析器本身可用：一级来源的**键**在 schema 里确实存在。
    assert _schema_declares_key(schema, "/properties/noise", "saturation_level"), (
        "C7 解析器失效：schema 的 noise 下确实声明了 saturation_level，"
        "说明 `_schema_declares_key` 没在解析 $ref"
    )
    assert not level1, "C7 一级来源（noise.saturation_level）在配置模板里出现了，须重做裁定"
    assert not level2, "C7 二级来源（snr.saturation_level）在 schema 里出现了，须重做裁定"
    assert not level3, "C7 三级来源（SATURATE/DATAMAX）在亮场头里出现了，须重做裁定"

    declared = rd.declared_external_inputs()
    sat = declared["saturation_level"]
    assert sat["all_absent_outcome"] == "DISABLED_NO_METADATA", (
        f"C7 末支未显式登记：{sat['all_absent_outcome']!r}"
    )
    assert sat["frozen_source_priority"] == [
        "noise.saturation_level", "snr.saturation_level", "FITS: SATURATE / DATAMAX",
    ], f"C7 来源优先级登记与合同不一致：{sat['frozen_source_priority']}"
    assert declared["electron_gain"]["in_repo_carrier"] is None, (
        "C7 电子学增益被登记成有仓内承载面，与 REG-03 实测矛盾"
    )
    assert declared["read_noise"]["in_repo_carrier"] is None, (
        "C7 读噪被登记成有仓内承载面，与 REG-03 实测矛盾"
    )


def test_CALIB_Inputs_XisfByteLayoutMatchesRepoCpp() -> None:
    """C8 本层 XISF 解析器的字节布局常量 ↔ 仓内 C++ 读入器**源码文本**逐条对撞。

    这是 (b) 路线的证据基础：布局**不是发明**的，是从一手实现抄下来的。
    两个面互不派生（本层 Python 常量 vs `.cpp` 源文本），因此可证伪：
    `.cpp` 改了魔数 / 头长宽度 / 上限 / 几何上界而本层没跟 ⇒ 判红。

    **来源依据**：`aio_xisf.cpp:25`（魔数）、`:29`（64 MiB 上限）、`:33`（几何上界）、
    `:447`（魔数比对）、`:452-460`（64 位小端头长）、`:91-98`（`attachment:`）。
    """
    with open(rd.anchor_file(CPP_ANCHOR, "AIO_XISF_CPP"), "r", encoding="utf-8") as fh:
        cpp = fh.read()
    checks = {
        "magic": r"XISF_MAGIC\[8\]\s*=\s*\{\s*'X',\s*'I',\s*'S',\s*'F',\s*'0',\s*'1',\s*'0',\s*'0'\s*\}",
        "max_xml_bytes": r"XISF_MAX_XML_HEADER_BYTES\s*=\s*64ull\s*\*\s*1024\s*\*\s*1024",
        "max_dim": r"XISF_MAX_DIM\s*=\s*65535",
        "memcmp_magic": r"memcmp\(magic,\s*XISF_MAGIC,\s*8\)",
        "xml_len_u64_le": r"xml_length\s*\|=\s*\(\(uint64_t\)len_bytes\[i\]\)\s*<<\s*\(8\s*\*\s*i\)",
        "attachment_prefix": r'loc\.find\("attachment:"\)',
    }
    missing = [name for name, pat in checks.items() if re.search(pat, cpp) is None]
    assert not missing, (
        f"C8 仓内 C++ 读入器的布局锚点在本层常量处对不上：{missing}；"
        f"对照文件 {CPP_ANCHOR}（本层解析器的布局来源，字节布局不发明）"
    )
    # 本层常量必须与 .cpp 的字面量**一致**，不只是「.cpp 里有这么一行」。
    assert rd.XISF_MAGIC == b"XISF0100", f"C8 魔数常量漂移：{rd.XISF_MAGIC!r}"
    assert rd.XISF_MAX_XML_HEADER_BYTES == 64 * 1024 * 1024, "C8 头长上限常量漂移"
    assert rd.XISF_MAX_DIM == 65535, "C8 几何上界常量漂移"
    assert "little-endian" in rd.XISF_BYTE_ORDERS, "C8 字节序词表缺 little-endian"


def test_CALIB_Inputs_XisfPixelsMatchDeclaredGeometry() -> None:
    """C9 未压缩附加块的像元读回与**声明面**逐项相符（对 T4 的 masterBias 做全量读回）。

    判据四路：(a) 读回数组的形状 == `geometry` 的 `(H, W[, C])`；
    (b) 读回样点数 == `W·H·C`；(c) 声明字节数 == `W·H·C·itemsize`（在头解析里已判，
        这里复述以免「头解析与像元读取用两套口径」）；
    (d) 数组里 `NaN`/`Inf` 计数为 0（母版是有限实数，母版出非有限值即判红）。

    ⚠ 本用例**不**主张 Python 解析结果与产品 `xisf_read_file()` 一致：那条判据
    需要构建产品运行，已登记为 B 类 `E2E.XisfReaderMatchesProduct`。

    **来源依据**：XISF 1.0 `location="attachment:<offset>:<size>"`；`aio_xisf.cpp:500-520`。
    """
    rel = f"{rd.TESTDATA_REL}/T4 calibration files/masterBias_BIN-1_4500x3600.xisf"
    xh = rd.read_xisf_header(rel)
    assert xh.sample_format == "Float32", f"C9 母版样格式漂移：{xh.sample_format}"
    assert xh.channels == 1, f"C9 母版通道数漂移：{xh.channels}"
    arr = rd.xisf_pixels(xh)
    assert arr.shape == (xh.height, xh.width), (
        f"C9 像元形状与声明不符：读回 {arr.shape}，声明 {(xh.height, xh.width)}"
    )
    assert arr.size == xh.width * xh.height * xh.channels, (
        f"C9 样点数不符：{arr.size} ≠ {xh.width * xh.height * xh.channels}"
    )
    assert xh.data_size == arr.size * xh.itemsize, (
        f"C9 声明字节数与「样点数×样宽」不符：{xh.data_size} ≠ {arr.size * xh.itemsize}"
    )
    assert not np.isnan(arr).any(), "C9 母版像元含 NaN"
    assert not np.isinf(arr).any(), "C9 母版像元含 Inf"


# ---------------------------------------------------------------------------
# 负例：fail-closed 面与内容级注入
# ---------------------------------------------------------------------------

def test_CALIB_Inputs_NegXisfDefectsRaiseNamedErrors(tmp_path, monkeypatch) -> None:
    """N-C1 XISF 解析器的 fail-closed 面：**9 种注入缺陷逐条点名具名异常**，
    并先对**合法**合成文件断言判绿（S2），证明异常不是无差别抛的。

    注入点 = 解析器自身的字节入口（`read_xisf_header`），不是判定结果的回读
    （`VALIDATION_EVIDENCE.md:193` S6）。全部产物在 `tmp_path`（`:197` 反例隔离）。

    | 注入 | 期望异常前缀 |
    |---|---|
    | 坏魔数 | `XISF_BAD_MAGIC` |
    | 头长为 0 | `XISF_BAD_MAGIC` |
    | 头长超 64 MiB 上限 | `XISF_XML_TOO_LARGE` |
    | 文件短于 16+头长（截断） | `XISF_TRUNCATED` |
    | 缺 `xsi:schemaLocation` | `XISF_MISSING_FIELD` |
    | 无 `<Image>` 元素 | `XISF_NO_IMAGE` |
    | 非法 geometry（越界/非整数） | `XISF_BAD_GEOMETRY` |
    | 未知 `sampleFormat` | `XISF_UNSUPPORTED_SAMPLE_FORMAT` |
    | 非 `attachment:` 承载 | `XISF_UNSUPPORTED_LOCATION` |
    | 声明字节数与几何不符 | `XISF_SIZE_MISMATCH` |

    最后一条另有一条**独立**注入：附加块越界（`offset+size > filesize`）
    ⇒ `XISF_ATTACHMENT_TRUNCATED`。

    **来源依据**：XISF 1.0 规范 + `aio_xisf.cpp:29,33,447,452-468,500-520`；
    `TEST.md:75-82`（缺失不得折叠成默认值）。
    """
    monkeypatch.setattr(rd, "REPO_ROOT", str(tmp_path))
    payload = np.arange(12, dtype=np.float32).tobytes()

    # S2：合法合成文件必须判绿，否则下面 11 条异常就没有对照。
    good = rd.build_synthetic_xisf(rd.synthetic_xisf_xml(), payload)
    (tmp_path / "good.xisf").write_bytes(good)
    xh = rd.read_xisf_header("good.xisf")
    assert xh.geometry == (4, 3, 1), f"N-C1 合法合成文件解析异常：{xh.geometry}"
    assert xh.sample_format == "Float32", "N-C1 合法合成文件样格式异常"
    assert rd.xisf_fits_keyword(xh, "IMAGETYP") == "'Master Bias'", "N-C1 合法合成文件关键字异常"

    cases: List[tuple] = [
        ("bad_magic", rd.build_synthetic_xisf(rd.synthetic_xisf_xml(), payload,
                                               magic=b"XISF9999"), "XISF_BAD_MAGIC"),
        ("zero_len", rd.build_synthetic_xisf(rd.synthetic_xisf_xml(), payload,
                                             declared_xml_length=0), "XISF_BAD_MAGIC"),
        ("too_large", rd.build_synthetic_xisf(rd.synthetic_xisf_xml(), payload,
                                              declared_xml_length=64 * 1024 * 1024 + 1),
         "XISF_XML_TOO_LARGE"),
        ("truncated", good[: len(good) // 2], "XISF_TRUNCATED"),
        ("no_schema_loc",
         rd.build_synthetic_xisf(
             '<?xml version="1.0" encoding="UTF-8"?><xisf><Image geometry="4:3:1">'
             '<FITSKeyword name="A" value="1"/></Image></xisf>', payload),
         "XISF_MISSING_FIELD"),
        ("no_image",
         rd.build_synthetic_xisf(
             '<?xml version="1.0" encoding="UTF-8"?>'
             '<xisf xsi:schemaLocation="a b"></xisf>', payload),
         "XISF_NO_IMAGE"),
        ("bad_geometry",
         rd.build_synthetic_xisf(
             rd.synthetic_xisf_xml(geometry="999999999:1:1"), payload), "XISF_BAD_GEOMETRY"),
        ("bad_geometry_nan",
         rd.build_synthetic_xisf(
             rd.synthetic_xisf_xml(geometry="4:3:1:1:1"), payload), "XISF_BAD_GEOMETRY"),
        ("unknown_format",
         rd.build_synthetic_xisf(
             rd.synthetic_xisf_xml(sample_format="Float16"), payload),
         "XISF_UNSUPPORTED_SAMPLE_FORMAT"),
        ("inline_location",
         rd.build_synthetic_xisf(
             rd.synthetic_xisf_xml(location="inline:0:48"), payload), "XISF_UNSUPPORTED_LOCATION"),
        ("size_mismatch",
         rd.build_synthetic_xisf(
             rd.synthetic_xisf_xml(location="attachment:16:9999"), payload), "XISF_SIZE_MISMATCH"),
        # 声明字节数与几何一致（否则先被 SIZE_MISMATCH 拦下），但偏移量远超文件尾。
        ("attachment_beyond_eof",
         rd.build_synthetic_xisf(
             rd.synthetic_xisf_xml(location="attachment:100000:48"), b""),
         "XISF_ATTACHMENT_TRUNCATED"),
    ]
    not_raised: List[str] = []
    for name, blob, expected in cases:
        path = tmp_path / f"{name}.xisf"
        path.write_bytes(blob)
        try:
            rd.read_xisf_header(f"{name}.xisf")
        except rd.XisfUnparsable as exc:
            assert expected in str(exc), (
                f"N-C1 注入 {name} 抛了 {type(exc).__name__} 但前缀不符：{exc}"
            )
        else:
            not_raised.append(name)
    assert not not_raised, (
        f"N-C1 以下注入**没有**让解析器判红（解析器 fail-open 了）：{not_raised}"
    )


@pytest.mark.parametrize("dataset_id", rd.DATASET_IDS)
def test_CALIB_Inputs_NegFlatFilterMissingRed(tmp_path, monkeypatch, dataset_id: str) -> None:
    """N-C2 平场覆盖判据有牙（S6）：注入一个**仓内不存在的滤镜** ⇒ C5 判红。

    注入面：亮场 FITS 头的 `FILTER`（C5 的输入面之一），隔离在 `tmp_path`。
    S2 对照：未注入时判绿。

    **来源依据**：`VALIDATION_EVIDENCE.md:193` S6、`:189` S2。
    """
    assert rd.judge_flat_filter_coverage(dataset_id) == [], "N-C2 对照面（未注入）不绿，负例无意义"

    from astropy.io import fits

    source = rd.frame_relpaths(dataset_id)[0]
    face = rd.parse_frame_name(source)
    header = rd.read_frame_header(source)
    # 注入帧必须**仍满足文件名合同**：D7/C5 的判定链会走 `parse_frame_name`，
    # 名字不合同会先被判 `FRAME_NAME_MISMATCH`，那测的就不是本判据了。
    injected_path = str(
        tmp_path
        / f"{face['obj']}_{face['panel']}_{face['tel']}_{face['mode']}-"
          f"{face['date']}@{face['time']}-{face['exp']}S-{face['filt']}.fts"
    )
    header["NAXIS1"] = 8
    header["NAXIS2"] = 8
    header["FILTER"] = "NoSuchFilterE2E"
    fits.PrimaryHDU(data=np.zeros((8, 8)), header=header).writeto(injected_path, overwrite=True)

    monkeypatch.setattr(rd, "frame_relpaths", lambda ds: [injected_path])
    viol = rd.judge_flat_filter_coverage(dataset_id)
    assert any("NoSuchFilterE2E" in v for v in viol), (
        f"N-C2 期望新滤镜被点名判红，实际判红集合 {viol}"
    )


def test_CALIB_Inputs_NegDarkGapDetectorHasTeeth(tmp_path, monkeypatch) -> None:
    """N-C3 暗场缺口探测器有牙（S6）：注入一个**仓内不存在**的曝光秒（777 s）
    ⇒ 缺口图必须新增该键，**不得**因为「已经在测缺口」就什么都看不见。

    ⚠ 这条负例同时证明 C4 的漂移守卫**不是恒真**：缺口签名一变，守卫就红。

    **来源依据**：`VALIDATION_EVIDENCE.md:193` S6。
    """
    from astropy.io import fits

    source = rd.frame_relpaths("galaxy_center")[0]
    face = rd.parse_frame_name(source)
    header = rd.read_frame_header(source)
    header["NAXIS1"] = 8
    header["NAXIS2"] = 8
    header["EXPTIME"] = 777.0
    injected = str(
        tmp_path
        / f"{face['obj']}_{face['panel']}_{face['tel']}_{face['mode']}-"
          f"{face['date']}@{face['time']}-777S-{face['filt']}.fts"
    )
    fits.PrimaryHDU(data=np.zeros((8, 8)), header=header).writeto(injected, overwrite=True)

    # S2：未注入时 galaxy_center 无缺口。
    assert rd.judge_dark_exposure_coverage("galaxy_center") == {}, (
        "N-C3 对照面（未注入）已有缺口，负例无意义"
    )
    monkeypatch.setattr(rd, "frame_relpaths", lambda ds: [injected])
    gaps = rd.judge_dark_exposure_coverage("galaxy_center")
    assert gaps == {"T4": {777.0: 1}}, f"N-C3 期望新增缺口键 {{'T4': {{777.0: 1}}}}，实测 {gaps}"
    viol = rd.judge_index_gap_matches_registration  # 触达函数，确认符号可用
    assert callable(viol), "N-C3 注册守卫入口丢失"


def test_CALIB_Inputs_NegElectronicsKeywordAppearsRed(tmp_path, monkeypatch) -> None:
    """N-C4 电子学量承载面漂移有牙（S6）：给一帧注入 `GAIN` ⇒ C6' 必须判红并点名文件。

    S2 对照：未注入的仓内数据判绿。

    **来源依据**：`VALIDATION_EVIDENCE.md:193` S6；本层 `ELECTRONICS_ABSENT_KEYWORDS`。
    """
    from astropy.io import fits

    assert rd.judge_electronics_absent() == [], "N-C4 对照面（未注入）不绿，负例无意义"

    source = rd.frame_relpaths("galaxy_center")[0]
    face = rd.parse_frame_name(source)
    header = rd.read_frame_header(source)
    header["NAXIS1"] = 8
    header["NAXIS2"] = 8
    header["GAIN"] = 1.5
    header["RDNOISE"] = 3.2
    injected = str(
        tmp_path
        / f"{face['obj']}_{face['panel']}_{face['tel']}_{face['mode']}-"
          f"{face['date']}@{face['time']}-{face['exp']}S-{face['filt']}.fts"
    )
    fits.PrimaryHDU(data=np.zeros((8, 8)), header=header).writeto(injected, overwrite=True)

    monkeypatch.setattr(rd, "frame_relpaths", lambda ds: [injected])
    viol = rd.judge_electronics_absent()
    joined = " ".join(viol)
    assert "ELECTRONICS_KEYWORD_APPEARED" in joined, f"N-C4 未点名电子学量出现：{viol}"
    assert "GAIN" in joined and "RDNOISE" in joined, f"N-C4 未逐个点名关键词：{viol}"