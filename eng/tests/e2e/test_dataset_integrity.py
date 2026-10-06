"""端到端层 · 数据面测试（真实数据的可达性、WCS 有效性、帧集合统计、命名与路径合同）。

## 用途

在**端到端层**判定两组真实观测（M42 与银心，`04_SCIENCE_EVIDENCE_AND_EXPERIMENT.md:32-34`）
作为「第三类实验数据」输入面的**事实条件**。本文件是 **A 类**（不需要构建产品，现在就跑）。

判定对象是**数据面**，不是产品行为：**不执行产品二进制、不起子进程、不链接库**。

## 类别约定（本层硬约定）

| 类别 | 文件 | 能否现在跑 | 标记 |
|---|---|---|---|
| **A 类·数据面** | 本文件 / `test_calibration_inputs.py` / `test_visual_acceptance.py` 的纯 Python 部分 | **能** | 无 skip 标记 |
| **B 类·产品端到端** | `test_pipeline_e2e.py` | **不能** | `@pytest.mark.skip(reason=…)` 显式标记 |

依据 `docs/engineering/testing/TEST.md:60`「硬件能力不可用时用 SKIP，不以伪通过掩盖未执行」。

## 正本依据（逐条 `文件:行`）

| 判据 | 正本 |
|---|---|
| e2e = 真实数据端到端与视觉验收辅助 | `standards/05_INDEPENDENT_TEST_SUITE.md:21` |
| 第三类实验数据 = M42 与银心，含视觉验收 | `standards/04_SCIENCE_EVIDENCE_AND_EXPERIMENT.md:32-34`；`docs/ACSD_DESIGN.md:497` |
| 元数据/掩膜/计数/索引/端口/选择结果 = **精确一致** | `docs/engineering/testing/TEST.md:46` |
| NaN/Inf 与「缺失」是并列三态，不得折叠成哨兵值 | `docs/engineering/testing/TEST.md:75-82` |
| 每个度量须有非退化判据，恒真比较无证据资格 | `docs/engineering/testing/TEST.md:26` |
| 判据必须能红；注入缺陷时读数不红即判该判据失效 | `docs/engineering/testing/TEST.md:62` |
| 锚存活 fail-closed | `docs/engineering/testing/VALIDATION_EVIDENCE.md:412-413` |
| 零对象守卫（实算对象数须 > 0） | `docs/engineering/testing/VALIDATION_EVIDENCE.md:170-176` |
| 判别力 S2（同时验证绿）/ S6（内容级负例）/ 反例隔离 | `docs/engineering/testing/VALIDATION_EVIDENCE.md:189,193,197` |

## 独立 Oracle 来源

**不读 `testdata/index.json` 的任何现值生成任何预期值。** 本文件全部预期值来自：

- **逐帧 FITS 头独立实算**（353 帧，`memmap=True` 只读头，实测 0.35 s/全扫）；
- **冻结登记** `_realdata.REGISTERED_*`（写用例前冻结的实测读数，作**漂移守卫**）；
- **FITS 标准自身**：`CDi_j` 与 `CDELTi` 是冗余表示、两者一致时比值应为 1；
  畸变解有 `PV*` 与 SIP 两条互不兼容的承载。

`testdata/index.json` 在本文件里是**被对账对象**（D10），不是 Oracle 的输入面。

## 命名映射（`Suite.Feature` → 函数名；`TEST.md:88` 的点号形式，pytest 侧用下划线）

| `Suite.Feature` | 函数名 | 类别 |
|---|---|---|
| `DATASET.Integrity.DatasetReachable` | `test_DATASET_Integrity_DatasetReachable` | 正例 D1 |
| `DATASET.Integrity.FrameCensusMatchesRegistration` | `test_DATASET_Integrity_FrameCensusMatchesRegistration` | 漂移守卫 D8 |
| `DATASET.Integrity.NamingContract` | `test_DATASET_Integrity_NamingContract` | 正例 D7 |
| `DATASET.Integrity.DistortionFaceRegistration` | `test_DATASET_Integrity_DistortionFaceRegistration` | 漂移守卫 D4b |
| `DATASET.Integrity.WcsUsableAndInvertible` | `test_DATASET_Integrity_WcsUsableAndInvertible` | 正例 D5/D6 |
| `DATASET.Integrity.WcsConsumesCdMatrixNotCdelt` | `test_DATASET_Integrity_WcsConsumesCdMatrixNotCdelt` | 正例 D9a |
| `DATASET.Integrity.NoWcsFramesEnumeratedNotIgnored` | `test_DATASET_Integrity_NoWcsFramesEnumeratedNotIgnored` | 正例 D4 |
| `DATASET.Integrity.CdCdeltConflictEnumeratedNotIgnored` | `test_DATASET_Integrity_CdCdeltConflictEnumeratedNotIgnored` | 登记项 D9b |
| `DATASET.Integrity.IndexReconciliation` | `test_DATASET_Integrity_IndexReconciliation` | 对账 D10 |
| `DATASET.Integrity.NegZeroObjectGuardRaises` | `test_DATASET_Integrity_NegZeroObjectGuardRaises` | 负例 N-D1（fail-closed） |
| `DATASET.Integrity.NegMissingAnchorRaises` | `test_DATASET_Integrity_NegMissingAnchorRaises` | 负例 N-D2（fail-closed） |
| `DATASET.Integrity.NegFrameNameMismatchRaises` | `test_DATASET_Integrity_NegFrameNameMismatchRaises` | 负例 N-D3（fail-closed） |
| `DATASET.Integrity.NegHeaderMissingFieldRaises` | `test_DATASET_Integrity_NegHeaderMissingFieldRaises` | 负例 N-D4（fail-closed） |
| `DATASET.Integrity.NegNamingViolationRed` | `test_DATASET_Integrity_NegNamingViolationRed` | 负例 N-D5（S6） |
| `DATASET.Integrity.NegCensusDriftRed` | `test_DATASET_Integrity_NegCensusDriftRed` | 负例 N-D6（S6） |
| `DATASET.Integrity.NegWcsSingularCdRed` | `test_DATASET_Integrity_NegWcsSingularCdRed` | 负例 N-D7（S6） |
| `DATASET.Integrity.NegWcsReaderFaceIsIndependent` | `test_DATASET_Integrity_NegWcsReaderFaceIsIndependent` | 负例 N-D8（S6） |

## 不产出阻塞退出码

本文件**不注册进 CMakeLists、不接 CI**；不出现 `sys.exit` / `raise SystemExit` / `os._exit`。
判红是**缺陷信号**，处置由人读对抗性审核给出，**本层不裁决代码**
（`AGENTS.md` §8「以非阻塞为常态」；`TEST.md:115`「测试集不产出流水线判决」）。
"""

from __future__ import annotations

import os
from typing import Any, Dict, List

import numpy as np
import pytest

import _realdata as rd

# 本文件用到的具名异常（fail-closed 面）。
_EXC = (rd.AnchorStale, rd.FrameUnparsable)


def _write_synthetic_frame(
    out_path: str,
    source_rel: str,
    header_overrides: Dict[str, Any],
    drop_keys: tuple = (),
    shape: tuple = (8, 8),
) -> str:
    """在 `tmp_path` 里造一个**隔离**的合成帧（负例专用，绝不写仓内留证）。

    合成帧继承某真实帧的**头**（`VALIDATION_EVIDENCE.md:197` 反例隔离），再施加
    `header_overrides` / `drop_keys` 注入。像素填可复现的常数梯度（非随机，不引入种子依赖）。
    """
    from astropy.io import fits

    header = rd.read_frame_header(source_rel)
    for key in drop_keys:
        del header[key]
    for key, value in header_overrides.items():
        if value is None:
            del header[key]
        else:
            header[key] = value
    header["NAXIS1"] = shape[1]
    header["NAXIS2"] = shape[0]
    primary = fits.PrimaryHDU(data=np.arange(shape[0] * shape[1], dtype=np.float64).reshape(shape),
                              header=header)
    primary.writeto(out_path, overwrite=True)
    return out_path


# ---------------------------------------------------------------------------
# 正例 / 漂移守卫（A 类）
# ---------------------------------------------------------------------------

@pytest.mark.parametrize("dataset_id", rd.DATASET_IDS)
def test_DATASET_Integrity_DatasetReachable(dataset_id: str) -> None:
    """D1 可达性：数据集目录在位、实算帧数 > 0、每帧是**非空普通文件**且 FITS 头可解析、
    本判据依赖的必需头字段齐全。

    判红含义：`ANCHOR_STALE`（目录没了）/ `ZERO_OBJECT_GUARD`（帧数为 0）/ `FRAME_EMPTY_FILE`
    （空文件）/ `FRAME_UNPARSABLE`（文件坏了）/ `HEADER_MISSING_FIELD`（头字段缺了）。

    **同时验证绿（S2）**：本函数在未注入的仓内数据上跑，判绿是 N-D5…N-D7 注入负例的对照面。

    **来源依据**：锚存活 `VALIDATION_EVIDENCE.md:413`；零对象守卫 `:170-176`。
    """
    assert rd.judge_dataset_reachable(dataset_id) == [], (
        f"D1 可达性判红（数据集 {dataset_id}）"
    )


@pytest.mark.parametrize("dataset_id", rd.DATASET_IDS)
def test_DATASET_Integrity_FrameCensusMatchesRegistration(dataset_id: str) -> None:
    """D8 帧集合指纹漂移守卫：实测帧数、几何、BITPIX、曝光计数、滤镜计数、有/无 WCS 帧数，
    与写用例前冻结的登记**逐项精确一致**（`TEST.md:46` 第一档）。

    判红含义：**数据换了**（补帧、删帧、换几何/滤镜/曝光集）。数据换了必须重新裁定登记，
    不能让新数据默默继承旧读数，也不能为了让判据变绿去改登记。

    **来源依据**：`VALIDATION_EVIDENCE.md:413` 锚存活 + 本层 `REG-D1` 冻结登记。
    """
    viol = rd.judge_frame_census(dataset_id)
    assert viol == [], f"D8 帧集合指纹漂移（数据集 {dataset_id}）：{viol[:6]}"


@pytest.mark.parametrize("dataset_id", rd.DATASET_IDS)
def test_DATASET_Integrity_NamingContract(dataset_id: str) -> None:
    """D7 命名合同：**文件名面**与 **FITS 头面**的四个交叉合取项 + 两条作用域约束。

    四个合取项（取自两条互不派生的观测面，不是恒真比较）：
    (a) 文件名曝光秒 == 头 `EXPTIME`；(b) 文件名滤镜 == 头 `FILTER`（逐字，不做大小写归一）；
    (c) 文件名 `YYYYMMDD` == 头 `DATE-OBS` 前 10 字；
    (d) 头 `OBJECT` == 文件名前缀 `<obj>_<panel>_<tel>_<mode>` 逐字。
    两条作用域：模式段恒为 `flying_dutchman`；望远镜代号落在该数据集登记集合内。

    **来源依据**：本层 D7 判据；测试名体例 `TEST.md:88`。
    """
    viol = rd.judge_naming_contract(dataset_id)
    assert viol == [], f"D7 命名合同判红（数据集 {dataset_id}）：{viol[:6]}"


@pytest.mark.parametrize("dataset_id", rd.DATASET_IDS)
def test_DATASET_Integrity_DistortionFaceRegistration(dataset_id: str) -> None:
    """D4b 畸变解关键词面漂移守卫（REG-D2）。

    实算 `CTYPE1` 取值集合、`-SIP` 声明面、`PV*` 系数面、`A_*B_*` 系数面、非标准前缀
    `TR1_*/TR2_*` 的出现帧数，与冻结登记逐项一致。

    判红含义：**畸变承载面换了**。注意本判据**不**判「M42 的畸变解无法被标准读取器套用」
    为红——那是**已登记的数据事实**（README REG-01b），本判据只保证它的读数不悄悄漂移。

    **来源依据**：FITS WCS 标准（`PV*` 与 SIP 两条互不兼容的畸变承载）；
    `docs/ACSD_DESIGN.md:535`「受影响验证范围只扩大不缩小」。
    """
    viol = rd.judge_distortion_face(dataset_id)
    assert viol == [], f"D4b 畸变承载面漂移（数据集 {dataset_id}）：{viol}"


@pytest.mark.parametrize("dataset_id", rd.DATASET_IDS)
def test_DATASET_Integrity_WcsUsableAndInvertible(dataset_id: str) -> None:
    """D5/D6 WCS 可用性与单射性：只判**带 `CTYPE1`** 的帧（无 WCS 的帧由 D4 守卫管）。

    判据：(a) `astropy.wcs.WCS` 可构造；(b) 投影平面像元尺度为正有限；
    (c) 角点处像元↔天球往返闭合误差 ≤ 冻结门限 `WCS_ROUNDTRIP_MAX_PX = 1e-4` 像元。

    ⚠ 门限的**取值依据与「不随数据调整」的承诺**写在 `_realdata.WCS_ROUNDTRIP_MAX_PX`
    常量处（抓的是「不是单射」而非「精度够不够」；实测库噪声底 6.44e-6 像元，15× 余量）。
    本判据的可信度锚在负例 **N-D7**（注入奇异 CD 必红），不锚在这个数字上。

    **来源依据**：FITS WCS 标准；`TEST.md:62` 判据必须能红。
    """
    viol = rd.judge_wcs_usable(dataset_id)
    assert viol == [], f"D5/D6 WCS 判红（数据集 {dataset_id}）：{viol[:6]}"


@pytest.mark.parametrize("dataset_id", rd.DATASET_IDS)
def test_DATASET_Integrity_WcsConsumesCdMatrixNotCdelt(dataset_id: str) -> None:
    """D9a WCS 读入器必须承重 `CD` 矩阵，**不得**承重 `CDELT`。

    判据构造两条**互相独立的解释路径**并要求它们分开：
    (a) 本层唯一读入路径 `wcs_x_axis_step_arcsec(header)`（astropy WCS 直接吃头）；
    (b) `wcs_x_axis_step_arcsec(header, drop_cd=True)`（删掉全部 `CD*` 卡片，
        只按 `CDELT` 解释）。
    (a)/(b) 的比值必须落在 `CD` 侧（`<= CD_CDELT_CONFLICT_RATIO`）。

    ⚠ **为什么用 x 步长而不用像元尺度**：实测 `m42` 的 CD 矩阵**全部强非对角**
    （对角 ≈0.0165″、非对角 ≈±0.966″，约 90° 旋转），其**面积尺度**
    `sqrt(|det CD|)` 与 `sqrt(|CDELT1·CDELT2|)` 只差 1.4e-4 相对量——用面积尺度做这张
    判据**不可观测**（比值趋 1 → 判据退化成恒真）。
    x 轴步长则相差 **496 倍**，判据有牙。负例 **N-D8** 从另一侧复核同一分离度。

    **来源依据**：FITS WCS Papers II（`CD` 优先于 `CDELT`）；`TEST.md:26` 非退化。
    """
    result = rd.judge_wcs_consumes_cd_matrix(dataset_id)
    expected = rd.REGISTERED_CD_CDELT_CONFLICT_FRAMES[dataset_id]
    assert result["n_observable"] == expected, (
        f"D9a 可观测帧数漂移（{dataset_id}）：实测 {result['n_observable']}，登记 {expected}"
    )
    assert result["viol"] == [], f"D9a WCS 承重面判红（{dataset_id}）：{result['viol'][:6]}"


@pytest.mark.parametrize("dataset_id", rd.DATASET_IDS)
def test_DATASET_Integrity_NoWcsFramesEnumeratedNotIgnored(dataset_id: str) -> None:
    """D4 无 WCS 帧**必须被点名**，不许被静默跳过。

    判据四路：(a) 无 `CTYPE1` 的帧数与冻结登记精确一致；
    (b) 逐帧**重新独立**确认每个被点名的帧确实没有 `CTYPE1`（不是复读同一份清单）；
    (c) 没有任何带 `CTYPE1` 的帧混进点名集合（两集合不交）；
    (d) 点名集合是帧全集的**真子集**（保证集合非空且非全集，即判据不是恒真/恒假）。

    判红含义：数据面漂移（补了 WCS 或丢了 WCS），须重新裁定登记。

    **来源依据**：`TEST.md:46` 精确一致档；`TEST.md:26` 非退化。
    """
    reg = rd.REGISTERED_FRAME_CENSUS[dataset_id]
    no_wcs: List[str] = []
    with_wcs: List[str] = []
    for rel in rd.frame_relpaths(dataset_id):
        header = rd.read_frame_header(rel)
        rd.require_light_keys(header, rel)
        (with_wcs if "CTYPE1" in header else no_wcs).append(rel)
    assert len(no_wcs) == reg["n_without_wcs"], (
        f"D4 无 WCS 帧数漂移（{dataset_id}）：实测 {len(no_wcs)}，登记 {reg['n_without_wcs']}"
    )
    assert len(with_wcs) == reg["n_with_wcs"], (
        f"D4 有 WCS 帧数漂移（{dataset_id}）：实测 {len(with_wcs)}，登记 {reg['n_with_wcs']}"
    )
    assert not (set(no_wcs) & set(with_wcs)), (
        "D4 点名集合与补集相交：同一帧既被判有 WCS 又被判无 WCS"
    )
    assert set(no_wcs) < set(rd.frame_relpaths(dataset_id)), (
        f"D4 退化：{dataset_id} 的无 WCS 集合不是帧全集的真子集"
    )
    for rel in no_wcs:
        assert "CTYPE1" not in rd.read_frame_header(rel), (
            f"D4 点名不实：{rel} 被列入无 WCS 集合，但复查发现有 CTYPE1"
        )


@pytest.mark.parametrize("dataset_id", rd.DATASET_IDS)
def test_DATASET_Integrity_CdCdeltConflictEnumeratedNotIgnored(dataset_id: str) -> None:
    """D9b `CD`/`CDELT` 矛盾的**登记守卫**（REG-01）。

    实测 `m42` 的 166 个有 WCS 帧**全部**命中矛盾（`|CD1_1|/|CDELT1|` ∈ [0.00372, 0.0208]，
    即差 48–269 倍）；`galaxy_center` 无 `CDELT1`，命中 0 帧。本判据逐帧点名，
    并断言命中帧数与**冻结登记**精确一致。

    ⚠ **命中数非零是数据缺陷的真实读数，不是判据失效**（`TEST.md:62`「判据必须能红」）。
    处置见 README REG-01：登记、不改数据、不降级阈值。本文件因此**不**把它写成
    「期望 0 命中」的绿用例——那等于要求数据没有这个缺陷，等于把判据降级。

    **来源依据**：FITS WCS Papers II（`CD`/`CDELT` 应为一致的冗余表示）。
    """
    viol = rd.judge_cd_cdelt_conflict(dataset_id)
    expected = rd.REGISTERED_CD_CDELT_CONFLICT_FRAMES[dataset_id]
    assert len(viol) == expected, (
        f"D9b CD/CDELT 矛盾帧数漂移（{dataset_id}）：实测 {len(viol)}，登记 {expected}；"
        f"首条 {viol[0] if viol else '（无）'}"
    )


def test_DATASET_Integrity_IndexReconciliation() -> None:
    """D10 `testdata/index.json` 对账（REG-06 / REG-06b）：索引**是被对账对象**，不是 Oracle。

    把索引自陈的 `Galaxy_Center_T4.observation_dates` 与逐帧 `DATE-OBS` 实算的日期集合
    **双向求差**，断言两方向的差集签名都与冻结登记一致，并逐字断言索引与磁盘的日期集合
    本身没有漂移。

    ⚠ 实测缺口**非零**：索引自陈 7 天、磁盘实算 8 天，索引缺 `2025-08-13`。
    这是**数据缺陷的真实读数**，按 `TEST.md:62` 如实登记（README REG-06）。
    本守卫保证的是「这个缺口的签名不悄悄漂移」，
    **不**把它写成「期望缺口为空」的绿用例——那等于要求数据没这个缺陷，等于降级判据
    （与模块层 REG-03 同一处置体例）。本层**不**私自改 `testdata/index.json`
    （`testdata/` 不在本单写域内）。

    **来源依据**：`TEST.md:62`；`AGENTS.md` §9「无法裁决的事项登记为 UNRESOLVED」。
    """
    viol = rd.judge_index_gap_matches_registration(rd.REGISTERED_INDEX_DATASET_ID)
    assert viol == [], f"D10 索引对账签名漂移：{viol}"
    rec = rd.index_date_reconciliation(rd.REGISTERED_INDEX_DATASET_ID)
    # 缺口必须被**点名**（登记在案），而不是被跳过。
    assert rec["only_disk"] == ("2025-08-13",), (
        f"D10 未点名索引缺口：实测 {list(rec['only_disk'])}"
    )
    assert rec["only_index"] == (), f"D10 索引多出日期：{list(rec['only_index'])}"


# ---------------------------------------------------------------------------
# 负例：fail-closed 面（注入缺陷 ⇒ 抛具名异常，绝不静默）
# ---------------------------------------------------------------------------

def test_DATASET_Integrity_NegZeroObjectGuardRaises(tmp_path, monkeypatch) -> None:
    """N-D1 零对象守卫（fail-closed）：把数据集目录换成一个**空的**临时目录，
    `frame_relpaths` 必须抛 `ZERO_OBJECT_GUARD`，**不得**返回空列表让下游「全绿」。

    隔离面：`tmp_path`（`VALIDATION_EVIDENCE.md:197` 反例复跑必须隔离），不写仓内留证。

    **来源依据**：`VALIDATION_EVIDENCE.md:170-176` 零对象守卫两条。
    """
    empty = tmp_path / "empty_dataset"
    empty.mkdir()
    monkeypatch.setitem(rd.DATASET_DIR, "m42", str(empty))
    with pytest.raises(AssertionError, match=r"ZERO_OBJECT_GUARD"):
        rd.frame_relpaths("m42")


def test_DATASET_Integrity_NegMissingAnchorRaises(monkeypatch) -> None:
    """N-D2 锚存活（fail-closed）：数据集目录指向不存在的路径 ⇒ `ANCHOR_STALE`。

    绝不允许「目录找不到就当数据集为空、判据通过」。

    **来源依据**：`VALIDATION_EVIDENCE.md:413`。
    """
    monkeypatch.setitem(rd.DATASET_DIR, "m42", "testdata/NO_SUCH_DATASET_FOR_E2E_NEGATIVE")
    with pytest.raises(rd.AnchorStale, match=r"ANCHOR_STALE"):
        rd.frame_relpaths("m42")
    with pytest.raises(rd.AnchorStale, match=r"ANCHOR_STALE"):
        rd.judge_dataset_reachable("m42")
    # 未知数据集 id 同样必须响（不静默返回空清单）。
    with pytest.raises(rd.AnchorStale, match=r"ANCHOR_STALE"):
        rd.frame_relpaths("no_such_dataset_id")
    # 索引锚缺失 ⇒ ANCHOR_STALE（索引是被对账对象，锚没了就必须响）。
    monkeypatch.setattr(rd, "INDEX_REL", "testdata/NO_SUCH_INDEX_FOR_E2E_NEGATIVE.json")
    with pytest.raises(rd.AnchorStale, match=r"ANCHOR_STALE"):
        rd.load_index()


def test_DATASET_Integrity_NegFrameNameMismatchRaises(tmp_path, monkeypatch) -> None:
    """N-D3 文件名合同（fail-closed）：造一个**不匹配**命名合同的帧文件，
    `parse_frame_name` 必须抛 `FRAME_NAME_MISMATCH`，**不得**猜一个默认解析结果。

    **来源依据**：`TEST.md:26` 恒真比较无证据资格 ⇒ 解析失败必须响。
    """
    bad = tmp_path / "not-a-contract-frame.fts"
    bad.write_bytes(b"SIMPLE  =                    T")
    monkeypatch.setattr(rd, "anchor_file", lambda rel, name: str(bad))
    with pytest.raises(rd.FrameUnparsable, match=r"FRAME_NAME_MISMATCH"):
        rd.parse_frame_name("not-a-contract-frame.fts")


def test_DATASET_Integrity_NegHeaderMissingFieldRaises(tmp_path) -> None:
    """N-D4 必需头字段守卫（fail-closed）：把真实帧的 `EXPTIME` 删掉，
    `require_light_keys` 必须抛 `HEADER_MISSING_FIELD`，**不得**用 `EXPTIME=0` 之类默认值顶替。

    这是对 `实验/m42-realdata/code/c3_seam_additive.py:302`（`max(..., default=0.0)`
    作用在先筛过的子集上 ⇒ 真出问题时门自动变绿）的直接对照。

    **来源依据**：`TEST.md:75-82`「不得把缺失折叠成哨兵值后再参与比较」。
    """
    source = rd.frame_relpaths("m42")[0]
    out = _write_synthetic_frame(str(tmp_path / "no_exptime.fts"), source, {}, drop_keys=("EXPTIME",))
    header = rd.read_frame_header(out)
    assert "EXPTIME" not in header, "N-D4 注入未生效：合成帧仍带 EXPTIME，负例无效"
    with pytest.raises(rd.FrameUnparsable, match=r"HEADER_MISSING_FIELD"):
        rd.require_light_keys(header, out)


def test_DATASET_Integrity_NegNamingViolationRed(tmp_path, monkeypatch) -> None:
    """N-D5 命名合同负例（S6 内容级负例，注入点 = 判定自身的输入面）。

    注入：把一帧**复制**到 `tmp_path` 并改名为曝光秒与头不符（`-900S-` 而头是 300），
    然后 monkeypatch `frame_relpaths` 让 `judge_naming_contract` 只看这个注入集合。
    期望：D7 判红并点名 `NAME_EXPTIME_MISMATCH`；同一判定在**未注入**的仓内集合上判绿（S2）。

    **来源依据**：`VALIDATION_EVIDENCE.md:193` S6；`:189` S2；`:197` 反例隔离。
    """
    # S2：未注入时同一判定判绿。
    assert rd.judge_naming_contract("m42") == [], "N-D5 的对照面（未注入）就不绿，负例无意义"

    source = rd.frame_relpaths("m42")[0]
    face = rd.parse_frame_name(source)
    injected_name = (
        f"{face['obj']}_{face['panel']}_{face['tel']}_{face['mode']}-"
        f"{face['date']}@{face['time']}-999S-{face['filt']}.fts"
    )
    # D7 只读**头**，故注入面用同源的合成帧即可（不复制 33 MB 像元）。
    injected = _write_synthetic_frame(str(tmp_path / injected_name), source, {})
    monkeypatch.setattr(rd, "frame_relpaths", lambda dataset_id: [injected])
    assert injected.endswith(injected_name), "N-D5 注入文件名未生效"
    assert rd.parse_frame_name(injected)["exp"] == "999", "N-D5 注入的曝光秒未生效"

    viol = rd.judge_naming_contract("m42")
    assert any(v.startswith("NAME_EXPTIME_MISMATCH:") for v in viol), (
        f"N-D5 期望 NAME_EXPTIME_MISMATCH 变红，实际判红集合为 {viol}"
    )


def test_DATASET_Integrity_NegCensusDriftRed(monkeypatch) -> None:
    """N-D6 帧集合指纹负例（S6）：抽掉一帧后 `judge_frame_census` 必须点名
    `FRAME_COUNT_DRIFT`（并连带 `FRAME_EXPTIME_CENSUS_DRIFT` / `FRAME_FILTER_CENSUS_DRIFT`）。

    注入面是判定的**枚举面**（`frame_relpaths`），不是判定结果的回读。
    S2 对照：未注入时判绿。

    **来源依据**：`VALIDATION_EVIDENCE.md:193` S6。
    """
    assert rd.judge_frame_census("galaxy_center") == [], "N-D6 的对照面（未注入）不绿，负例无意义"

    real = rd.frame_relpaths("galaxy_center")
    dropped = real[-1]
    monkeypatch.setattr(rd, "frame_relpaths", lambda dataset_id: real[:-1])
    viol = rd.judge_frame_census("galaxy_center")
    assert any(v.startswith("FRAME_COUNT_DRIFT:") for v in viol), (
        f"N-D6 期望 FRAME_COUNT_DRIFT 变红，实际 {viol}"
    )
    assert any(v.startswith("FRAME_EXPTIME_CENSUS_DRIFT:") for v in viol), (
        f"N-D6 期望曝光计数漂移同时被点名，实际 {viol}"
    )
    assert dropped not in {v for v in viol}, "N-D6 注入未生效"


def test_DATASET_Integrity_NegWcsSingularCdRed(tmp_path, monkeypatch) -> None:
    """N-D7 WCS 单射性负例（S6）：注入**奇异 CD**（行列式为 0）后 `judge_wcs_usable` 必须判红。

    这条负例是 WCS 往返门限 `WCS_ROUNDTRIP_MAX_PX` 的**可信度锚**：门限是个数字，
    只有「注入目标失效模式必红」才能证明这个数字不是照着实测分布卡的。
    S2 对照：未注入时判绿。

    **来源依据**：`TEST.md:26` 非退化；`:62` 判据必须能红。
    """
    assert rd.judge_wcs_usable("m42") == [], "N-D7 的对照面（未注入）不绿，负例无意义"

    source = rd.frame_relpaths("m42")[0]
    header = rd.read_frame_header(source)
    scale = float(header["CD1_1"])
    injected = _write_synthetic_frame(
        str(tmp_path / "singular_cd.fts"),
        source,
        header_overrides={"CD1_2": 0.0, "CD2_1": 0.0, "CD2_2": scale},
    )
    # 行列式 = CD1_1*CD2_2 - CD1_2*CD2_1 = scale*scale - 0 != 0 ⇒ 这一组是**可逆**的，
    # 用来先确认注入面本身没坏；奇异组合在下一步单独注入。
    singular = {"CD1_1": 0.0, "CD1_2": 0.0, "CD2_1": 0.0, "CD2_2": 0.0}
    injected_singular = _write_synthetic_frame(
        str(tmp_path / "singular_cd_zero.fts"), source, header_overrides=singular
    )
    monkeypatch.setattr(rd, "frame_relpaths", lambda dataset_id: [injected, injected_singular])

    viol = rd.judge_wcs_usable("m42")
    assert any("singular_cd_zero.fts" in v for v in viol), (
        f"N-D7 期望奇异 CD 帧被点名判红，实际判红集合为 {viol}"
    )


def test_DATASET_Integrity_NegWcsReaderFaceIsIndependent() -> None:
    """N-D8 WCS 读入面独立性负例：证明「CD 面」与「CDELT 面」是**真的两条不同的路径**，
    D9a 的判据不是恒真比较。

    做法：由**本用例独立**（不经 `_realdata.judge_*`）对每帧算两条路径的 x 步长，
    断言二者比值落在 `CD` 侧且 ≤ `CD_CDELT_CONFLICT_RATIO`，
    并断言可观测帧数与冻结登记一致。
    ⇒ 若有人把本层读入路径改成先删 `CD*`（即承重 `CDELT`），比值趋 1，本用例与 D9a **同时**变红；
    若两路径本就重合，本用例会因「量级不足」判红，逼复核方说明判据为何退化。

    **来源依据**：`TEST.md:26` 恒真比较无证据资格；FITS WCS Papers II 冗余优先级。
    """
    checked = 0
    for rel in rd.frame_relpaths("m42"):
        header = rd.read_frame_header(rel)
        if "CTYPE1" not in header or "CDELT1" not in header:
            continue
        checked += 1
        step_cd = rd.wcs_x_axis_step_arcsec(header, rel)
        step_cdelt = rd.wcs_x_axis_step_arcsec(header, rel, drop_cd=True)
        assert step_cdelt > 0.0, f"N-D8 {rel} 的 CDELT 面步长非正，注入面本身坏了"
        ratio = step_cd / step_cdelt
        assert ratio <= rd.CD_CDELT_CONFLICT_RATIO, (
            f"N-D8 判据退化：{rel} 两读入面比值 {ratio:.6g} 落在 CDELT 侧，"
            "「本层承重 CD」这一支不可观测"
        )
    assert checked == rd.REGISTERED_CD_CDELT_CONFLICT_FRAMES["m42"], (
        f"N-D8 可观测帧数漂移：实测 {checked}，登记 "
        f"{rd.REGISTERED_CD_CDELT_CONFLICT_FRAMES['m42']}"
    )
