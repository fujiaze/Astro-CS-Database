"""端到端层 · 视觉验收**辅助**面（拉伸 PNG 生成器 + 机器可读诊断量 + fail-closed 面）。

## ⚠⚠ 本文件**不产出任何「合格 / 不合格」判决**

规范要求 e2e 含「视觉验收」（`standards/04_SCIENCE_EVIDENCE_AND_EXPERIMENT.md:32-34`；
`docs/ACSD_DESIGN.md:497`），L4 判据写的是「**由负责人目检判定**」
（`docs/ACSD_DESIGN.md:533`、`:536`；`AGENTS.md` §11「发布决定只属项目负责人」）。

⚠ 实测**仓内零参考图**：`testdata/` 全树 PNG/JPG/JPEG/PDF/TIF/TIFF 计数 = **0**
（`rd.count_reference_images()`）。没有基准可比 ⇒ 本层**不可能**给出比图判据。

因此本文件的定位严格限定为**辅助**：

1. **拉伸 PNG 生成器**（纯 numpy + matplotlib，`matplotlib 3.11.2` 实测可用）：
   输入 FITS 阵列 → 线性 / 对数 / asinh 拉伸 PNG，供**人目检**。
   非有限像元**不折叠成 0、不填哨兵**（`TEST.md:79`），在图上渲成洋红 ⇒ 黑洞看得见。
2. **机器可读的诊断量**（NaN/Inf 计数、饱和像元计数、全零块、动态范围、稳健背景中位数/MAD）：
   这些是**诊断读数，不是门**。本层**不**给它们设「合格线」。
3. **禁止布尔门**：判据 **V7** 断言诊断字典的键集等于冻结词表，且**字典里没有任何
   `bool` 值** ⇒ 谁往里塞一个 `passed: True` 就判红。

**真实的 L4 视觉验收链路**（马赛克 → 平面 FITS → 拉伸 PNG → 切块目检）需要产品产物，
登记为 B 类 `E2E.VisualAcceptanceChain`（`test_pipeline_e2e.py`），**需构建**。

## 正本依据（逐条 `file:line`）

| 判据 | 正本 |
|---|---|
| L4 = 马赛克→平面 FITS→拉伸 PNG→切块目检，**由负责人目检判定** | `docs/ACSD_DESIGN.md:533` |
| L4 通过后由负责人决定发布预览版 | `docs/ACSD_DESIGN.md:536`、`:558` |
| 发布决定只属项目负责人 | `AGENTS.md` §11 |
| 比较器不得把非有限值折叠成 `0` 或哨兵值后再参与有限比较 | `docs/engineering/testing/TEST.md:79` |
| NaN / Inf / 「缺失」是并列三态，位置与语义须逐项精确一致 | `docs/engineering/testing/TEST.md:75-82` |
| 硬件能力不可用时显式 SKIP，不以伪通过掩盖 | `docs/engineering/testing/TEST.md:60` |
| 每个度量须有非退化判据 | `docs/engineering/testing/TEST.md:26` |
| 判据必须能红 | `docs/engineering/testing/TEST.md:62` |
| 判别力 S2 / S6 / 反例隔离 | `docs/engineering/testing/VALIDATION_EVIDENCE.md:189,193,197` |

## 临时产物纪律

⚠ 拉伸 PNG **绝不写进 `testdata/`**（那是数据面，不是本层写域，且会污染冻结指纹）。
全部产物落在 pytest 的 `tmp_path`，测试结束即随临时目录销毁。

## 命名映射（`Suite.Feature` → 函数名；`TEST.md:88`）

| `Suite.Feature` | 函数名 | 类别 |
|---|---|---|
| `VISUAL.Aux.StretchedPngWrittenForRealFrame` | `test_VISUAL_Aux_StretchedPngWrittenForRealFrame` | 正例 V1 |
| `VISUAL.Aux.StretchRoundTripsThroughPng` | `test_VISUAL_Aux_StretchRoundTripsThroughPng` | 正例 V2 |
| `VISUAL.Aux.DiagnosticsReadWholeArray` | `test_VISUAL_Aux_DiagnosticsReadWholeArray` | 正例 V4 |
| `VISUAL.Aux.ReferenceImagesAbsentRegistration` | `test_VISUAL_Aux_ReferenceImagesAbsentRegistration` | 漂移守卫 V3 |
| `VISUAL.Aux.NoBooleanVerdictIsProduced` | `test_VISUAL_Aux_NoBooleanVerdictIsProduced` | 正例 V7 |
| `VISUAL.Aux.NegInjectedDefectsAreDiagnosed` | `test_VISUAL_Aux_NegInjectedDefectsAreDiagnosed` | 负例 N-V1（S6） |
| `VISUAL.Aux.NegSaturationLevelExternalIsNoneNotZero` | `test_VISUAL_Aux_NegSaturationLevelExternalIsNoneNotZero` | 负例 N-V2（反 fail-open） |
| `VISUAL.Aux.NegStretchRejectsDegenerateInput` | `test_VISUAL_Aux_NegStretchRejectsDegenerateInput` | 负例 N-V3（fail-closed） |

## 不产出阻塞退出码

不注册进 CMakeLists、不接 CI；不出现 `sys.exit` / `raise SystemExit` / `os._exit`。
判红是**缺陷信号**，**本层不裁决代码**。
"""

from __future__ import annotations

import functools
import os
from typing import Dict, List

import numpy as np
import pytest

import _realdata as rd

#: 目检用的**固定**取帧面（写死，避免测试跑着跑着换了一帧就不可复现）。
#: `m42` 的第一帧带 WCS；取中心 `1024×1024` 切块（L4 判据本身就是「切块目检」）。
SAMPLE_FRAME_REL = (
    "testdata/M42_T2T3_mosaic_Flying_dutchman/T2/M1/"
    "M42_M1_T2_flying_dutchman-20251212@012404-300S-Red.fts"
)
SAMPLE_CROP_PX = 1024


@functools.lru_cache(maxsize=1)
def _sample_crop() -> np.ndarray:
    """取固定帧的中心 `1024×1024` 切块。

    读法说明（实测踩过的坑，写在这里免得下次再踩）：
    `fits.open(..., section=...)` 只在 memmap 下生效，而 memmap 遇到
    `BZERO/BSCALE`（本组帧有 `BZERO=32768 BSCALE=1.0`）会被 astropy **拒绝**
    （`Cannot load a memory-mapped image: BZERO/BSCALE/BLANK ... Set memmap=False.`），
    `mode="deny"` 也不是合法取值。⇒ 唯一可用读法是 `memmap=False`（由 astropy 应用
    BZERO/BSCALE 标度）后自行切块。

    `lru_cache(maxsize=1)`：同一进程内复用这次读取（读 4096² 帧一次约 1 s，5 条用例各读
    一次没有必要）。缓存的是**只读**数据，源文件在一次 pytest 运行内不变；
    `make testdata-mutate` 之类的场景下需 `.cache_clear()`（本层无此类用例）。
    """
    from astropy.io import fits

    header = rd.read_frame_header(SAMPLE_FRAME_REL)
    nx, ny = int(header["NAXIS1"]), int(header["NAXIS2"])
    size = SAMPLE_CROP_PX
    x0 = (nx - size) // 2
    y0 = (ny - size) // 2
    with fits.open(rd.anchor_file(SAMPLE_FRAME_REL, "SAMPLE_FRAME"), memmap=False) as hdul:
        data = hdul[0].data
    if data is None:
        raise rd.FrameUnparsable(f"HEADER_MISSING_FIELD: {SAMPLE_FRAME_REL} 无像元数据")
    return np.asarray(data[y0:y0 + size, x0:x0 + size], dtype=np.float64)


# ---------------------------------------------------------------------------
# 正例 / 漂移守卫（A 类）
# ---------------------------------------------------------------------------

def test_VISUAL_Aux_StretchedPngWrittenForRealFrame(tmp_path) -> None:
    """V1 对**真实帧切块**三种拉伸各出一张 PNG，供人目检。

    判据（可红）：PNG 魔数逐字为 `\\x89PNG`；文件非空；写回读回的像素网格尺寸 == 切块尺寸；
    像素值全部落在 `[0, 1]`；三种拉伸**互不相同**（若三者相同则拉伸没起作用）。

    ⚠ 临时产物在 `tmp_path`，**不写进 `testdata/`**。

    **来源依据**：`docs/ACSD_DESIGN.md:533`（L4 的「拉伸 PNG → 切块目检」环节）。
    """
    arr = _sample_crop()
    assert arr.shape == (SAMPLE_CROP_PX, SAMPLE_CROP_PX), (
        f"V1 取帧切块尺寸漂移：{arr.shape}"
    )
    rendered: Dict[str, Dict] = {}
    for mode in ("linear", "log", "asinh"):
        out = str(tmp_path / f"sample_{mode}.png")
        info = rd.render_stretch_png(arr, out, mode=mode, title=f"M42 sample crop ({mode})")
        assert os.path.isfile(out), f"V1 {mode} 没产出 PNG"
        with open(out, "rb") as fh:
            assert fh.read(4) == b"\x89PNG", f"V1 {mode} 不是合法 PNG（魔数不符）"
        assert info["png_bytes"] > 0, f"V1 {mode} 产出了空 PNG"
        assert tuple(info["shape"]) == arr.shape, (
            f"V1 {mode} 映射面尺寸 {info['shape']} ≠ 切块尺寸 {arr.shape}"
        )
        rendered[mode] = info

    back = rd.read_png_back(rendered["asinh"]["path"])
    assert back.shape[:2] == arr.shape, (
        f"V1 PNG 读回网格尺寸 {back.shape[:2]} ≠ 切块尺寸 {arr.shape}"
    )
    assert float(back.min()) >= 0.0 and float(back.max()) <= 1.0 + 1e-6, (
        f"V1 PNG 读回值域越界：[{back.min()}, {back.max()}]"
    )
    # 三种拉伸必须真的不同（否则「拉伸」是空操作）。
    mean_by_mode = {
        mode: float(np.nanmean(rd.read_png_back(info["path"])[:, :, 0]))
        for mode, info in rendered.items()
    }
    assert len({round(v, 6) for v in mean_by_mode.values()}) == 3, (
        f"V1 三种拉伸的像元均值不可区分：{mean_by_mode}"
    )


def test_VISUAL_Aux_StretchRoundTripsThroughPng(tmp_path) -> None:
    """V2 拉伸 ↔ PNG **往返一致**：写出的图与内存里的映射面必须一致（不是只出文件）。

    四条判据，逐条可红：

    (a) **网格尺寸精确一致**：PNG 读回尺寸 == 映射面尺寸（渲染走 `figsize=w/dpi` 的
        逐像元 1:1 路径，标题写进 PNG 元数据而不占像素网格）；
    (b) **主体逐位精确**：`|got − expected|` 的**中位数为 0**
        ⇒ 渲染路径没有多套任何一层变换（gamma、LUT、再次归一化都会立刻破这一条）。
        这是本判据**承重**的那一条；
    (c) **越界像元比例** ≤ `PNG_EDGE_FRACTION_MAX = 1e-3`：1:1 栅格化在整幅图边界上
        存在亚像元效应（实测 46/1 048 576 = 4.4e-5，留 20× 余量）。
        ⚠ 这一条是**余量条**，不是承重条 —— 它之所以不是照着实测卡出来的，
        是因为承重在 (b)：任何全局变换都会先破 (b)；
    (d) 负例自证：把期望面**人为加一层 gamma** 后重算 (b)，中位数不再为 0
        ⇒ 证明 (b) 不是恒真。

    方向约定：`imshow(origin="lower")`（天球惯例北在上）落到 PNG 上是自上而下的行序，
    `read_png_back` 按 PNG 行序返回 ⇒ 期望面须按行翻转。

    **来源依据**：`TEST.md:46` 精确一致档（计数与索引/元数据）。
    """
    arr = _sample_crop()
    for mode in ("linear", "asinh"):
        out = str(tmp_path / f"roundtrip_{mode}.png")
        info = rd.render_stretch_png(arr, out, mode=mode)
        mapped, _readings = rd.stretch_to_unit(arr, mode=mode)
        expected = np.round(mapped * 255.0) / 255.0
        expected = expected[::-1, :]
        got = rd.read_png_back(out)[:, :, 0]

        assert got.shape == arr.shape, (
            f"V2 {mode} PNG 网格尺寸 {got.shape} ≠ 映射面 {arr.shape}（1:1 渲染被破坏）"
        )
        assert float(got.min()) >= 0.0 and float(got.max()) <= 1.0 + 1e-6, (
            f"V2 {mode} PNG 值域越界：[{got.min()}, {got.max()}]"
        )
        dev = np.abs(got - expected)
        median_dev = float(np.median(dev))
        assert median_dev == 0.0, (
            f"V2 {mode} 往返偏差中位数 {median_dev:.6g} ≠ 0：渲染路径多套了一层变换"
        )
        frac_bad = float(np.count_nonzero(dev > 1.0 / 255.0 + 1e-6) / dev.size)
        assert frac_bad <= rd.PNG_EDGE_FRACTION_MAX, (
            f"V2 {mode} 越界像元比例 {frac_bad:.3e} > 门限 {rd.PNG_EDGE_FRACTION_MAX:.3e}"
        )
        assert info["shape"] == list(mapped.shape), (
            f"V2 {mode} 渲染返回的尺寸与映射面不一致"
        )

    # (d) 负例自证：模拟渲染路径里多套了一层**全局**变换 ⇒ (b) 立刻不成立 ⇒ (b) 不是恒真断言。
    # 用「反相」而不是 gamma：M42 这帧线性拉伸后过半像元恰好为 0，0 的任意正幂仍是 0，
    # gamma 面下中位偏差会假性为 0；反相只在一个取值处为零，不会骗过中位数判据。
    mapped, _ = rd.stretch_to_unit(arr, mode="linear")
    expected = (np.round(mapped * 255.0) / 255.0)[::-1, :]
    inverted_dev = float(np.median(np.abs(expected - (1.0 - expected))))
    assert inverted_dev > 0.0, (
        "V2 (d) 自证失败：加一层全局变换都没改变中位偏差，说明 (b) 的判据面取错了"
    )


def test_VISUAL_Aux_DiagnosticsReadWholeArray() -> None:
    """V4 反 fail-open：诊断量必须读**全数组**。

    这是对仓内反面教材 `实验/m42-realdata/code/c3_seam_additive.py:302`
    （`max(..., default=0.0)` 作用在**先筛过的子集**上 ⇒ 真出问题时门自动变绿）
    的直接对照。判据两路闭合：(a) `n_pixels` 恰等于数组元素数；
    (b) `n_nan + n_posinf + n_neginf + finite_pixels` 恰等于 `n_pixels`
    （两个**互不派生**的计数面闭合）。

    **来源依据**：`TEST.md:75-82`；`VALIDATION_EVIDENCE.md:189` S2。
    """
    arr = _sample_crop()
    viol = rd.judge_diagnostics_read_whole_array(arr)
    assert viol == [], f"V4 诊断量未读全数组：{viol}"

    diag = rd.array_diagnostics(arr, saturation_level=None)
    assert set(rd.DIAGNOSTIC_NAMES) <= set(diag), (
        f"V4 诊断词表缺项：缺 {sorted(set(rd.DIAGNOSTIC_NAMES) - set(diag))}"
    )
    assert diag["n_pixels"] == SAMPLE_CROP_PX * SAMPLE_CROP_PX, (
        f"V4 n_pixels={diag['n_pixels']} ≠ {SAMPLE_CROP_PX ** 2}"
    )
    assert diag["finite_pixels"] + diag["n_nan"] + diag["n_posinf"] + diag["n_neginf"] \
        == diag["n_pixels"], "V4 非有限/有限计数不闭合"


def test_VISUAL_Aux_ReferenceImagesAbsentRegistration() -> None:
    """V3「零参考图」这一登记的漂移守卫（REG-05）。

    `testdata/` 树内一旦出现参考图，本层就必须决定是否改为「与参考图比对」的口径；
    在裁定之前**不允许默默开始比对**，也不允许默默把参考图删掉。

    判红含义：`REFERENCE_IMAGE_APPEARED`。处置见 README REG-05（交负责人裁定）。

    **来源依据**：`docs/ACSD_DESIGN.md:533`（L4 是目检判定）；`TEST.md:62`。
    """
    viol = rd.judge_reference_images_absent()
    assert viol == [], f"V3 参考图登记漂移：{viol}"
    counts = rd.count_reference_images()
    assert counts == {}, f"V3 testdata 内出现参考图：{counts}"


def test_VISUAL_Aux_NoBooleanVerdictIsProduced() -> None:
    """V7 **禁止布尔门**：视觉辅助面不得产出任何「合格 / 不合格」判决。

    判据三条，逐条可红：
    (a) 诊断字典的键集恰等于冻结词表 ∪ `{finite_pixels}`（多一个键 ⇒ 有人加了新量，
        须先说明它是不是判决）；
    (b) 诊断字典的**值里没有任何 `bool`**（`bool` 是 `int` 的子类，必须显式排除，
        否则整数计数会被误判）；
    (c) 渲染返回字典的**值里没有任何 `bool`**。

    ⇒ 有人往里塞 `passed: True` / `ok: True` / `valid: True` 立即判红。
    这条判据把「不许发明一个『视觉验收通过』的布尔门并挂 pass」
    从纪律条文变成**可执行**的约束。

    **来源依据**：`docs/ACSD_DESIGN.md:533,558`（目检判定权属负责人）；
    `AGENTS.md` §11；本层 `DIAGNOSTIC_NAMES` 冻结词表。
    """
    arr = _sample_crop()
    diag = rd.array_diagnostics(arr, saturation_level=None)
    expected_keys = set(rd.DIAGNOSTIC_NAMES) | {"finite_pixels"}
    assert set(diag) == expected_keys, (
        f"V7 诊断键集漂移：多出 {sorted(set(diag) - expected_keys)}，"
        f"缺少 {sorted(expected_keys - set(diag))}"
    )
    offenders = [k for k, v in diag.items() if isinstance(v, bool)]
    assert not offenders, f"V7 诊断量里出现布尔判决字段：{offenders}"

    import tempfile

    with tempfile.TemporaryDirectory() as td:
        out = os.path.join(td, "probe.png")
        info = rd.render_stretch_png(arr, out, mode="asinh")
    offenders = [k for k, v in info.items() if isinstance(v, bool)]
    assert not offenders, f"V7 渲染返回里出现布尔判决字段：{offenders}"
    assert not isinstance(info["png_bytes"], bool), "V7 png_bytes 不得是布尔"


# ---------------------------------------------------------------------------
# 负例：内容级注入 + 反 fail-open + fail-closed
# ---------------------------------------------------------------------------

def test_VISUAL_Aux_NegInjectedDefectsAreDiagnosed() -> None:
    """N-V1 诊断量对四类注入缺陷**逐条点名**（S6）：全零块、NaN、±Inf、饱和。

    注入面是诊断函数的**输入数组**，不是诊断结果的回读。
    S2 对照：先对未注入的合成数组跑一次，断言四类计数**都为 0**（证明不是无差别报警）。

    | 注入 | 期望诊断量 |
    |---|---|
    | 中心 `64×64` 全零块 | `n_zero_blocks > 0` 且 `n_zero_block_pixels = n_zero_blocks × 4096` |
    | NaN 像素 | `n_nan` 恰等于注入个数 |
    | `+Inf` / `-Inf` | `n_posinf` / `n_neginf` 恰等于注入个数 |
    | 注入 ≥ 阈值的像元 | `n_saturated` 恰等于注入个数（**给了 `saturation_level` 才会数**） |

    **来源依据**：`TEST.md:75-82`（非有限值不得折叠成哨兵）；`VALIDATION_EVIDENCE.md:193` S6。
    """
    rng_free = np.arange(256, dtype=np.float64).reshape(16, 16) + 100.0

    # S2：未注入时四类计数全为 0（否则下面的「>0」没有对照价值）。
    base = rd.array_diagnostics(rng_free, saturation_level=1000.0)
    assert base["n_zero_blocks"] == 0, "N-V1 对照面已有全零块，负例无意义"
    assert base["n_nan"] == 0 and base["n_posinf"] == 0 and base["n_neginf"] == 0, (
        "N-V1 对照面已有非有限像元，负例无意义"
    )
    assert base["n_saturated"] == 0, "N-V1 对照面已有饱和像元，负例无意义"

    # 注入一：全零块（16×16 的数组放不下 64×64，改用 256×256 的底数组）。
    big = np.full((256, 256), 500.0)
    big[64:192, 64:192] = 0.0   # 与 64 px 分块对齐，覆盖 2×2 = 4 个整块
    d = rd.array_diagnostics(big, saturation_level=1000.0)
    assert d["n_zero_blocks"] == 4, f"N-V1 全零块计数错：{d['n_zero_blocks']}（期望 4 个 64×64 块）"
    assert d["n_zero_block_pixels"] == 4 * 64 * 64, (
        f"N-V1 全零块像元数错：{d['n_zero_block_pixels']}"
    )

    # 注入二：NaN。
    with_nan = rng_free.copy()
    with_nan[3, 4] = np.nan
    with_nan[5, 6] = np.nan
    assert rd.array_diagnostics(with_nan)["n_nan"] == 2, "N-V1 NaN 计数错"

    # 注入三：±Inf。
    with_inf = rng_free.copy()
    with_inf[1, 1] = np.inf
    with_inf[2, 2] = -np.inf
    di = rd.array_diagnostics(with_inf)
    assert di["n_posinf"] == 1 and di["n_neginf"] == 1, (
        f"N-V1 Inf 计数错：pos={di['n_posinf']} neg={di['n_neginf']}"
    )
    # 非有限像元**不**被折叠进有限统计：有限像元数必须随之减少。
    assert di["finite_pixels"] == rng_free.size - 2, (
        f"N-V1 有限像元计数错：{di['finite_pixels']}（非有限值被折叠了？）"
    )

    # 注入四：饱和（只有显式给了 saturation_level 才数）。
    with_sat = rng_free.copy()
    with_sat[0, :] = 5000.0
    ds = rd.array_diagnostics(with_sat, saturation_level=1000.0)
    assert ds["n_saturated"] == with_sat.shape[1], f"N-V1 饱和像元计数错：{ds['n_saturated']}"


def test_VISUAL_Aux_NegSaturationLevelExternalIsNoneNotZero() -> None:
    """N-V2 反 fail-open：`saturation_level` **外部不可得**时，饱和像元计数必须是
    `None`（未定义），**不得**是 `0`（=「没有饱和像元」）或任何默认数值。

    这是 REG-03 的直接可执行后果：仓内没有任何承载面给出饱和电平
    （`test_calibration_inputs.py` 的 C7 已判），所以「饱和像元 = 0」是一个**未测量**的断言，
    写成 0 就是把「没测」说成「测了是零」。

    ⚠ 同时断言：给了 `saturation_level` 后该字段**必须**变成整数（不是恒 `None`）。

    **来源依据**：`TEST.md:79`「不得把非有限值折叠成 0 或任何哨兵值后再参与比较」；
    `TEST.md:82`「『缺失』是与非有限值并列的第三态」。
    """
    arr = _sample_crop()
    diag = rd.array_diagnostics(arr, saturation_level=None)
    assert "n_saturated" in diag, "N-V2 诊断量缺 n_saturated 键"
    assert diag["n_saturated"] is None, (
        f"N-V2 饱和电平外部不可得时必须记 None（未定义），实测 {diag['n_saturated']!r}；"
        "记 0 等于把「没测」说成「测了是零」"
    )
    with_level = rd.array_diagnostics(arr, saturation_level=0.0)
    assert isinstance(with_level["n_saturated"], int) and not isinstance(
        with_level["n_saturated"], bool
    ), (
        f"N-V2 给了饱和电平后 n_saturated 必须是整数计数，实测 "
        f"{with_level['n_saturated']!r}"
    )
    assert with_level["n_saturated"] == arr.size, (
        f"N-V2 阈值为 0 时全部有限像元都该算饱和，实测 {with_level['n_saturated']}"
    )


def test_VISUAL_Aux_NegStretchRejectsDegenerateInput(tmp_path) -> None:
    """N-V3 拉伸的 fail-closed 面：常量图（动态范围为 0）与 log 拉伸遇非正像元，
    一律抛具名异常，**不得**静默出图。

    静默出图 = 把「没有信息」画成一张看起来正常的图，正是 L4 目检最怕的失败模式。

    **来源依据**：`TEST.md:26` 非退化；`TEST.md:75-82` 缺失语义。
    """
    flat = np.full((16, 16), 42.0)
    with pytest.raises(rd.FrameUnparsable, match=r"STRETCH_DEGENERATE_RANGE"):
        rd.stretch_to_unit(flat, mode="linear")
    with pytest.raises(rd.FrameUnparsable, match=r"STRETCH_DEGENERATE_RANGE"):
        rd.stretch_to_unit(flat, mode="asinh")

    with_nonpositive = np.linspace(-5.0, 5.0, 256).reshape(16, 16)
    with pytest.raises(rd.FrameUnparsable, match=r"STRETCH_LOG_NONPOSITIVE"):
        rd.stretch_to_unit(with_nonpositive, mode="log")

    with pytest.raises(rd.FrameUnparsable, match=r"STRETCH_UNKNOWN_MODE"):
        rd.stretch_to_unit(np.arange(16.0).reshape(4, 4), mode="magic")

    with pytest.raises(rd.FrameUnparsable, match=r"STRETCH_NO_FINITE_PIXELS"):
        rd.stretch_to_unit(np.full((4, 4), np.nan), mode="linear")

    # 对照：正数组在 log 下必须能出图（否则 N-V3 的 log 那条是无差别拒绝）。
    positive = np.linspace(1.0, 100.0, 256).reshape(16, 16)
    mapped, readings = rd.stretch_to_unit(positive, mode="log")
    assert mapped.shape == positive.shape, "N-V3 正数组 log 拉伸形状错"
    assert float(np.nanmin(mapped)) >= 0.0 and float(np.nanmax(mapped)) <= 1.0, (
        f"N-V3 正数组 log 拉伸值域越界：[{np.nanmin(mapped)}, {np.nanmax(mapped)}]"
    )
    assert readings["mode"] == "log", "N-V3 读数未记录模式"