"""合成全链层的公共构造器：哈勃仿真成像 + 纯解析代数合成。

对应正本（逐字）：`run/GOVERN-08/工作包-RECTIFY-09原件/standards/04_SCIENCE_EVIDENCE_AND_EXPERIMENT.md`
§2.1「以 HST 真实数据（本仓 M16，带 PHOTFLAM 定标）作为**纯信号模板**，按完整物理前向过程
生成采样帧：源、天光、暗流在**电子域做 Poisson 采样**；读出噪声做 **Gaussian**；电子经
**增益、饱和与量化**转 ADU；加入**平场响应与天空梯度**；生成多帧以覆盖**不同透明度、天光、
视宁度与指向**。」

---

## 纪律一（硬）：天光**只能**作为 Poisson 事件的入射率进入

`04_SCIENCE_EVIDENCE_AND_EXPERIMENT.md` §2.2 逐字：「**天光对信噪比的影响通过散粒噪声体现**。」
上游正本 `docs/science/noise_snr/NOISE_SNR.md` §4 逐字：「**分子只有源**：天光只经散粒噪声
进入分母。把天光电平或观测电平计入分子，就得到一个随天光上升的量，与信噪比的定义相反。」
同节逐字：「在探测器未饱和、噪声估计由天光散粒主导的成立域内，固定源通量下天光越亮信噪比
越低并趋于零：**分母随天光开方增长而分子不变**。」

**落法（本文件里唯一的合法路径）**：

```text
λ(x,y) = transparency · [ S(x,y) · flat(x,y) + sky(x,y) + dark ]   （单位：电子 / 像元 / 帧）
N_e(x,y) ~ Poisson( λ(x,y) )                                       （逐像元独立）
Var[ N_e | λ ] = λ                                                 （泊松的解析方差）
```

天光 `sky` 提高 λ ⇒ 泊松方差 `λ` 提高 ⇒ 噪声提高。**除此之外天光不得以任何形式进入信噪比
公式**：不许出现 `SNR = (S + sky)/√(...)` 这类写法，不许给天光一个独立的方差项，
不许把「天光水平」当成分子。理由不是风格问题——把天光计入分子会得到一个**随天光上升**的量，
与信噪比的定义相反；P2 创新点的整条链都建立在这条口径上。

**可注入面（负例用）**：`sample_electrons` 的 λ 组成与 `analytic_variance_e2` 的解析方差。
用例把 `sky_e` 换成「天光 + 一个**与天光无关的确定性天光项**」后，泊松方差不再等于 `λ`，
`var ∝ λ` 这条独立判据必红——**判据给超界读数，不是用例失败**。

## 纪律二（硬）：本模块是**数据构造器**，不是 Oracle

`04_SCIENCE_EVIDENCE_AND_EXPERIMENT.md` §1 逐字：「**不以当前程序输出生成唯一 expected**。」
`docs/engineering/testing/TEST.md` §2 逐字：「恒真的比较没有证据资格。」

⇒ **绝对禁止**把 `_kit.py` 自己的输出（`forward_model` 的 ADU、`analytic_starfield`
的阵列等）当成别处的期望值。调用方的期望值只能是四类来源之一：

1. **闭式/解析真值**：本模块刻意把「解析真值」与「抽样实现」**分成两个对象**——
   `AnalyticStarfield.total_flux_e` 是闭式常量，`AnalyticStarfield.signal_e` 是把同一批
   星点铺到像素上的实现。二者对拍才有意义；拿 `signal_e.sum()` 去期望
   `signal_e.sum()` 是空转；
2. **第三方独立实现**：`astropy`（FITS / 卷积 / WCS），白名单见 `TEST.md` §13；
3. **一手文献**：见本文件常量区与 `tolerances.py` §4；
4. **定种子蒙特卡洛**：`_kit` 只保证「同 seed 逐字节可复跑」这条**确定性**，
   统计结论的散布门限在 `tolerances.py` 的 `synth.mc.*` 里已冻结。

## 纪律三（硬）：解析真值只能来自 `docs/science/*` 正本公式当场推导

仓内另有一个前向模型 `实验/shared/synthetic/noise_model.py`，它实现了本节 8/8 环节，
并被 33 个同级文件 import。本层**保持自实现**，理由两条：

1. `05_INDEPENDENT_TEST_SUITE.md` §1「测试代码是**独立的一套代码集**」——测试集与产品代码
   不共用的东西越多，两边一起错的机会越大；
2. 该模块自带的 `noise_selftest()` 自身带一批恒真门与 fail-open 路径
   （`实验/TAUTOLOGY_REGISTER.md` 的登记），它**不是金标准**。

⇒ 因此本模块**禁止 import 它**，且**禁止**转调
`noise_model.predicted_variance_adu2()` / `sky_sigma_adu()` / `canon_snr()` /
`clipped_std_adu()` 这些函数：它们与生成器**同文件**，拿它们当期望值就是**往返自证**。

本文件里每一个解析真值的来源，逐条写在对应函数或常量的 docstring 里，分类只有三种：

- **正本公式当场推导**：`docs/science/noise_snr/NOISE_SNR.md` §2.2 的方差项表、
  `docs/science/drizzle/DRIZZLE.md` 的守恒门、`docs/science/PHOTOMETRY.md` 的定标口径；
- **第三方独立实现**：`astropy`（`TEST.md` §13 白名单）；
- **一手文献**：见本文件常量区与 `tolerances.py` §4。

## 数据只在本盘、不在仓内（实测，影响 fail-closed 口径）

`testdata/HST_M16/` 被 `.gitignore:167` 整目录忽略，`git ls-files testdata/HST_M16` 实测
**0 条**——三帧 FITS 只存在于工作盘，**不在版本库里**。因此：

- 本层的任何判据在**没有这些文件的工作盘上跑不起来**，`load_m16_template` 抛
  `TemplateUnavailable`（显式失败面），**不得**因此把该判据记为通过
  （`TEST.md` §13「外部求解器不可用时，相关判据记为未执行并说明缺什么，
  不以跳过冒充通过」）；
- 纯解析代数合成臂（`analytic_starfield` 等）**不依赖**这些文件，在任何盘上都能跑。

## 数据依赖

只 `import numpy`（顶层）与 `astropy.io.fits`（**惰性** import，只在读 FITS 时）。
**不 import 任何产品 C++ 模块**（`libacsd` 及其模块）——本层是测试侧构造器，
不进产品依赖面（`TEST.md` §13）。也**不用 `scipy`**：`scipy` 不在 `TEST.md` §13 的
白名单里，卷积与几何重采样一律走 numpy 自己的实现（`astropy.convolution` /
`astropy.wcs` 是用例侧可用的独立对照）。

## 确定性

所有随机性走**显式传入的 `numpy.random.Generator`**；本文件**没有模块级隐式随机状态**、
没有 `np.random.seed` 调用、没有 `default_rng()` 的无参调用。固定 seed ⇒ 逐字节可复跑
（`TEST.md` §3）。

## fail-closed

读不到模板、头里没有 `PHOTFLAM`、`BUNIT` 不是电子率、窗口越界、窗口含非有限值、
参数缺面 —— 一律**抛显式异常**（`TemplateUnavailable` / `ValueError`），
**不静默返回零数组**。理由：`VALIDATION_EVIDENCE.md` 的 fail-closed 纪律；
一个静默的全零模板会让每一条以模板为输入的判据「恒绿且恒无意义」。
"""

from __future__ import annotations

import math
import os
from dataclasses import dataclass, field
from typing import Any, Dict, List, Optional, Sequence, Tuple

import numpy as np

# ---------------------------------------------------------------------------
# 字面常量区：每一个常量都带出处。分三类，不混用。
# ---------------------------------------------------------------------------

#: 仓库根（本文件在 `eng/tests/synthetic/`，上溯三级：synthetic → tests → eng → 根）。
#: ⚠ 这条路径算错过一次：`load_m16_template` 会因此去找 `<上一级>/testdata/...`，
#: 抛 `TemplateUnavailable`（fail-closed 起作用了，不是静默零数组）。改这一行时照此复核。
REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))

#: 本仓 HST M16 DRZ 模板目录。来源：04_SCIENCE_EVIDENCE_AND_EXPERIMENT.md §2.1
#: 「本仓 M16」；实际文件 `testdata/HST_M16/hlsp_heritage_hst_wfc3-uvis_m16_{band}_v1_drz.fits`。
M16_DIRNAME = os.path.join("testdata", "HST_M16")

#: 三个窄带的文件名片段。来源：仓内文件名（实测存在三个，各 268917120 字节）。
M16_BANDS = ("f502n", "f657n", "f673n")

#: 头里需要的键名。`PHOTFLAM` / `PHOTZPT` / `EXPTIME` / `BUNIT` 的存在性已实测：
#: 三个窄带都有 `PHOTFLAM`（f502n 5.2676009e-18 / f657n 2.2290223e-18 /
#: f673n 2.23971949999999e-18），同有 `PHOTZPT = -21.1`、`EXPTIME`（16000 / 9600 / 14400）、
#: `BUNIT = 'ELECTRONS/S'`、`PHOTPLAM`、`PHOTFNU`、`PHOTBW`、`PHOTCORR`、`PHOTMODE`。
HDR_PHOTFLAM = "PHOTFLAM"
HDR_PHOTZPT = "PHOTZPT"
HDR_EXPTIME = "EXPTIME"
HDR_BUNIT = "BUNIT"
HDR_CCDGAIN = "CCDGAIN"
HDR_FILTER = "FILTER"
HDR_PHOTMODE = "PHOTMODE"
HDR_INSTRUME = "INSTRUME"
HDR_DETECTOR = "DETECTOR"

#: ⚠⚠ **实测**：本仓三帧的增益与读出噪声**不在**通用名 `GAIN` / `READNOISE` 下。
#: 这两行留在这里只为记录「它们**不存在**」这一事实，防止有人再去找一次。
HDR_GAIN_GENERIC_ABSENT = "GAIN"
HDR_READNOISE_GENERIC_ABSENT = "READNOISE"

#: 真实承载增益的键族（实测四通道全同值 `1.5599999` 电子/ADU）。
#: WFC3/UVIS 的 UVIS2 CCD 有四个放大器象限，每个一个 `ATODGN?` 增益。
HDR_GAIN_CHANNELS = ("ATODGNA", "ATODGNB", "ATODGNC", "ATODGND")

#: 真实承载读出噪声的键族（实测 `READNSEA/B/C/D = 3.03 / 3.13 / 3.08 / 3.18` 电子 rms，
#: **单通道值**）。WFC3/UVIS 的四个象限分别读出。
HDR_READNOISE_CHANNELS = ("READNSEA", "READNSEB", "READNSEC", "READNSED")

#: `BUNIT` 的可接受字面量。DRZ 是 drizzle 叠加图，像元值是**电子率**；
#: `_kit` 据此乘 `EXPTIME` 还原成「每像元电子数」。
BUNIT_ELECTRONS_PER_SECOND = "ELECTRONS/S"

#: WFC3/UVIS 的放大器象限数。四通道各自读出后合成 ⇒ 合成读噪是单通道的 `√4` 倍。
#: 来源分类：**一手文献 + 仓内实测**（`ATODGN?` / `READNSE?` 各四个键，正好四个象限）。
UVIS_AMPLIFIER_CHANNELS = 4

#: 单通道增益的**外部文献**兜底值（电子/ADU）。
#: ⚠ 头里 `ATODGNA..D = 1.5599999`，`CCDGAIN = 1.5`（标定文件的标称值）。
#: 本层优先用头里的 `ATODGN*`；`1.5` 只在头里完全没有该键族时兜底。
HST_GAIN_E_PER_ADU_FALLBACK = 1.5

#: 单通道读噪的**外部文献**兜底值（电子 rms）。
#: ⚠⚠ 来源分类：**一手文献常量，不是从 FITS 头读的**；头里承载读噪的是
#: `READNSEA..D`，**不是** `READNOISE`（后者实测不存在）。
#: 取 3.1 e⁻ rms 是 WFC3/UVIS 单象限的标称读噪量级
#: （Biretta et al., *WFC3 Instrument Handbook*, STScI；CCD 噪声模型见
#: Newberry 1991, PASP 103, 122），且恰为头里四个单通道值的算术平均
#: （(3.03+3.13+3.08+3.18)/4 = 3.105）——两条来源互证，但**它们是不同的量**：
#: 本条是**单通道**值，四通道合成值是它的 `√4` 倍（见 `HST_READ_NOISE_E_COMBINED`）。
HST_READ_NOISE_E_PER_CHANNEL_FALLBACK = 3.1

#: 四通道**合成**读噪（电子 rms）= 单通道 × `√4` = `3.1 × 2 = 6.2`。
#: 公式：四个**独立**读出通道的噪声平方可加 ⇒ `RN_combined = √(Σ RN_i²)`，
#: 四通道同值时 `= RN_single · √4`。前向误差模型同 `TEST.md` §4 的归约档。
HST_READ_NOISE_E_COMBINED = HST_READ_NOISE_E_PER_CHANNEL_FALLBACK * math.sqrt(UVIS_AMPLIFIER_CHANNELS)

#: ADC 满量程（ADU）。WFC3/UVIS 的 16 位 ADC。
HST_FULL_SCALE_ADU = 65535.0

#: 饱和电子数 = 满量程 ADU × 增益。
HST_SATURATION_E = HST_FULL_SCALE_ADU * HST_GAIN_E_PER_ADU_FALLBACK

#: 均匀量化器的级数 `2**n_bits`。来源：ADC 均匀量化定义
#: （Newberry 1991, PASP 103, 122；Janesick 2001, SPIE PM83, Ch.2）。
QUANT_LEVELS: Dict[int, int] = {8: 1 << 8, 16: 1 << 16}

#: 本层默认量化位数。合成全链跑 16 位（与 HST 实际 ADC 一致）。
#: 8-bit 档在 `QUANT_LEVELS` 里同表给出，供负例把量化噪声放大用。
DEFAULT_N_BITS = 16

#: 默认 seed。**项目冻结的设计参数**，不是测量值。
#: 来源：`TEST.md` §3「固定种子；同一输入重复运行逐字节同结果」要求固定 seed，
#: 数值本身由本层冻结（写用例时照抄，不许每条用例各写一个）。
DEFAULT_SEED = 424242

#: 模板默认子窗口（`(y0, y1, x0, x1)`，左闭右开）。同 `tolerances.py` 的
#: `synth.fixture.m16_window`。位置是**夹具**，不是容差。
DEFAULT_WINDOW = (4000, 4064, 4000, 4064)
#: 默认窗口边长（像元）。
DEFAULT_WINDOW_PX = 64

#: 暗电流默认电平（电子/像元/帧）。**场景描述参数**，不是默认真相——
#: `render_sequence` 从 `Scene` 读它，函数体里没有硬编码。
#: ⚠ 来源标注：**场景构造参数，仓内无独立标定出处**。取值按 WFC3/UVIS 低温暗流量级
#: （低十位 e⁻/s × 万秒曝光 ⇒ 10² e⁻ 量级）选定，**它是可覆盖 kwarg**
#: （`m16_scene(dark_electrons=...)`），任何判据用到它都必须显式写出取值与量级依据。
DEFAULT_DARK_E = 30.0

#: 天空梯度与平场的基底函数约定（**构造口径**，不是物理断言）：
#: 归一化坐标 `x̂ = (x + 0.5)/W − 0.5`、`ŷ = (y + 0.5)/H − 0.5`，二者都落在 `[−0.5, 0.5)`。
#: 梯度系数按 `c0`（常数相对偏置）、`c1`（x 方向斜率）、`c2`（y 方向斜率）排列。
GRADIENT_COEFF_ORDER = ("c0", "c1_x", "c2_y")

#: 平场系数的排列：`a0`（常数）、`a1`（x）、`a2`（y）、`a3`（二次径向）。
FLAT_COEFF_ORDER = ("a0", "a1_x", "a2_y", "a3_r2")


class TemplateUnavailable(RuntimeError):
    """模板不可用。**fail-closed**：读不到就抛，绝不静默返回零数组。

    理由：`VALIDATION_EVIDENCE.md` 的 fail-closed 纪律。零模板会让以模板为输入的每一条
    判据「恒绿且恒无意义」——那是最坏的失效形态（恒真门没有证据资格，`TEST.md` §2）。
    """


# ---------------------------------------------------------------------------
# §1 模板装载（按窗口读，绝不整读 269 MB）
# ---------------------------------------------------------------------------


@dataclass(frozen=True)
class TemplateProvenance:
    """模板来源记录。**每个数值都有出处**，供用例与实验报告逐条引用。"""

    band: str
    path: str
    window: Tuple[int, int, int, int]      # (y0, y1, x0, x1)，左闭右开
    drz_shape: Tuple[int, int]             # (NAXIS2, NAXIS1)，实测 (8400, 8000)
    drz_bitpix: int
    bunit: str
    exptime_s: float
    photflam: float                        # Jy，零点通量密度
    photzpt: float                         # AB 零点星等
    gain_e_per_adu: Optional[float]        # 头里 ATODGN* 有则用之，否则 CCDGAIN，否则文献兜底
    gain_source: str                       # 'atodgn_channels' | 'ccdgain' | 'external_literature'
    read_noise_e: float                    # **四通道合成**读噪（电子 rms）
    read_noise_e_per_channel: Tuple[float, ...]   # **单通道**读噪（电子 rms），头里读到什么就记什么
    read_noise_source: str                 # 'readnse_channels' | 'external_literature'
    filter_name: str
    photmode: str
    instrument: str
    detector: str
    n_pixels: int
    n_nonfinite: int
    n_negative: int                        # DRZ 核振铃造成的负值（本仓该窗口实测 0）
    signal_median_e: float                 # 乘 EXPTIME 之后的每像元电子数中位数
    signal_max_e: float
    units_note: str


def load_m16_template(band: str = "f502n",
                      window: Sequence[int] = DEFAULT_WINDOW,
                      data_root: Optional[str] = None
                      ) -> Tuple[np.ndarray, float, TemplateProvenance]:
    """读 M16 DRZ 的**一个子窗口**，返回 `(signal_e_per_pixel, photflam_per_band, provenance)`。

    **单位（本函数返回值）**：`signal_e_per_pixel` 是**每像元、每帧的电子数**
    （`float64`），由 DRZ 像元值乘 `EXPTIME` 得到 —— 因为本仓 DRZ 的 `BUNIT` 实测是
    `ELECTRONS/S`（电子率），不是 ADU、也不是电子数。

    **只读窗口**：DRZ 全幅是 `8000 × 8400 × 4 B = 268917120 B ≈ 269 MB`。本函数用
    `memmap=True` 打开并只取 `hdul[0].data[y0:y1, x0:x1]`，物理读入量 = 窗口大小。
    **禁止**整读——那会让单条判据吃掉 269 MB 工作集。

    ## 参数

    - `band`：`f502n` / `f657n` / `f673n`，对应仓内三个 DRZ 文件。
    - `window`：`(y0, y1, x0, x1)`，左闭右开，像元索引，缺省取
      `tolerances.SYNTH_FIXTURE_M16_WINDOW.value`。
    - `data_root`：仓库根，缺省用本文件推出的 `REPO_ROOT`。

    ## 返回

    `(signal_e_per_pixel, photflam_per_band, provenance)` 三元组。`photflam_per_band`
    直接来自 FITS 头的 `PHOTFLAM`（Jy），**不是**本模块算的。

    ## fail-closed（全部 `TemplateUnavailable`）

    文件不存在 / `astropy` 不可用 / 头里没有 `PHOTFLAM` / `EXPTIME ≤ 0` /
    `BUNIT` 不是电子率 / 窗口含非有限值 —— 一律抛显式异常。
    **不返回零数组、不做静默降级。**

    ## 已知口径偏差（如实登记，不得在实验报告里抹掉）

    DRZ 是**多次曝光的 drizzle 叠加图**，不是单次曝光。把像元值乘 `EXPTIME` 得到的是
    「按单次曝光时长归一化后的每像元电子数」，**不是**任何一次真实曝光的电子数 ——
    叠加图里混入了 dither 覆盖、多帧平均与核权重。`04` §2.1 只要求它当**纯信号模板**，
    本层就是这么用的；但**不得**据此声称模板通量就是单帧注入通量：
    真值侧必须另取（见 `analytic_starfield`）。
    """
    root = data_root or REPO_ROOT
    if band not in M16_BANDS:
        raise TemplateUnavailable(
            f"band={band!r} 不在仓内三个窄带 {M16_BANDS} 里；"
            f"不得用一个不存在的带名静默回退")
    if len(window) != 4:
        raise ValueError(f"window 必须是 (y0, y1, x0, x1) 四元组，实得 {window!r}")
    y0, y1, x0, x1 = (int(v) for v in window)
    if not (y0 < y1 and x0 < x1):
        raise ValueError(f"window 必须是正面积：{window!r}")

    path = os.path.join(root, M16_DIRNAME,
                        f"hlsp_heritage_hst_wfc3-uvis_m16_{band}_v1_drz.fits")
    if not os.path.isfile(path):
        raise TemplateUnavailable(
            f"模板文件不存在：{path}。"
            f"⚠ 不静默返回零数组——零模板会让每条以模板为输入的判据恒绿且无意义"
            f"（TEST.md §2「恒真的比较没有证据资格」）")

    try:
        from astropy.io import fits  # 惰性：纯解析代数合成臂不需要 astropy
    except ImportError as e:  # pragma: no cover - 环境缺依赖时的显式失败面
        raise TemplateUnavailable(
            f"读 FITS 需要 astropy（TEST.md §13 白名单内的测试侧依赖）：{e}") from e

    try:
        with fits.open(path, memmap=True) as hdul:
            if len(hdul) < 1 or hdul[0].data is None:
                raise TemplateUnavailable(f"{path}: 找不到含像元数据的 HDU")
            hdu = hdul[0]
            hdr = hdu.header

            photflam = hdr.get(HDR_PHOTFLAM)
            if photflam is None:
                raise TemplateUnavailable(
                    f"{path}: 头里没有 {HDR_PHOTFLAM}。"
                    f"04 §2.1 要求模板「带 PHOTFLAM 定标」，缺它就不能做绝对刻度")
            photflam = float(photflam)

            photzpt = hdr.get(HDR_PHOTZPT)
            photzpt = float(photzpt) if photzpt is not None else float("nan")

            exptime = float(hdr.get(HDR_EXPTIME, 0.0))
            if not (exptime > 0.0):
                raise TemplateUnavailable(
                    f"{path}: {HDR_EXPTIME}={exptime!r}，无法把电子率还原成每帧电子数")

            bunit = str(hdr.get(HDR_BUNIT, "")).strip().upper()
            if bunit != BUNIT_ELECTRONS_PER_SECOND:
                raise TemplateUnavailable(
                    f"{path}: {HDR_BUNIT}={bunit!r}，本构造器只认 "
                    f"{BUNIT_ELECTRONS_PER_SECOND!r}（电子率）。"
                    f"换单位需要另写一条换算并留下出处，不得默认按电子率读")

            shape = (int(hdr["NAXIS2"]), int(hdr["NAXIS1"]))
            bitpix = int(hdr["BITPIX"])
            if not (0 <= y0 < y1 <= shape[0] and 0 <= x0 < x1 <= shape[1]):
                raise ValueError(
                    f"window {window!r} 越界：DRZ 全幅 (NAXIS2, NAXIS1)={shape}")

            ccdgain = hdr.get(HDR_CCDGAIN)
            ccdgain = float(ccdgain) if ccdgain is not None else None

            # ⚠ 增益与读噪都不在 GAIN / READNOISE 通用名下（实测这两个键**不存在**）。
            # 真实键族是 ATODGNA..D 与 READNSEA..D。
            gains = [float(hdr[k]) for k in HDR_GAIN_CHANNELS if hdr.get(k) is not None]
            if gains:
                gain = sum(gains) / len(gains)
                gain_source = "atodgn_channels"
            elif ccdgain is not None:
                gain, gain_source = ccdgain, "ccdgain"
            else:
                gain, gain_source = HST_GAIN_E_PER_ADU_FALLBACK, "external_literature"

            rn_channels = [float(hdr[k]) for k in HDR_READNOISE_CHANNELS
                           if hdr.get(k) is not None]
            if rn_channels:
                # 四通道独立读出 ⇒ 噪声平方可加（TEST.md §4 归约档的前向误差模型）
                read_noise = math.sqrt(sum(r * r for r in rn_channels))
                read_noise_source = "readnse_channels"
            else:
                rn_channels = [HST_READ_NOISE_E_PER_CHANNEL_FALLBACK] * UVIS_AMPLIFIER_CHANNELS
                read_noise = math.sqrt(sum(r * r for r in rn_channels))
                read_noise_source = "external_literature"

            meta = {k: str(hdr.get(k, "")) for k in
                    (HDR_FILTER, HDR_PHOTMODE, HDR_INSTRUME, HDR_DETECTOR)}

            # ⚠ 唯一允许的读法：切一个子窗口（不是整块）。
            block = np.asarray(hdu.data[y0:y1, x0:x1], dtype=np.float64)
    except TemplateUnavailable:
        raise
    except OSError as e:
        raise TemplateUnavailable(f"{path}: 读取失败（{type(e).__name__}: {e}）") from e

    n_nonfinite = int((~np.isfinite(block)).sum())
    if n_nonfinite:
        raise TemplateUnavailable(
            f"{path} 窗口 {tuple(window)!r} 含 {n_nonfinite} 个非有限值。"
            f"TEST.md §4.4：非有限值的位置集合必须精确可查，不能用一个带 NaN 的模板"
            f"把所有下游判据污染成非有限")

    signal_e = block * exptime          # 电子/秒 → 电子/像元/帧
    n_negative = int((signal_e < 0.0).sum())

    provenance = TemplateProvenance(
        band=band,
        path=path,
        window=(y0, y1, x0, x1),
        drz_shape=shape,
        drz_bitpix=bitpix,
        bunit=bunit,
        exptime_s=exptime,
        photflam=photflam,
        photzpt=photzpt,
        gain_e_per_adu=gain,
        gain_source=gain_source,
        read_noise_e=read_noise,
        read_noise_e_per_channel=tuple(rn_channels),
        read_noise_source=read_noise_source,
        filter_name=meta[HDR_FILTER],
        photmode=meta[HDR_PHOTMODE],
        instrument=meta[HDR_INSTRUME],
        detector=meta[HDR_DETECTOR],
        n_pixels=int(signal_e.size),
        n_nonfinite=n_nonfinite,
        n_negative=n_negative,
        signal_median_e=float(np.median(signal_e)),
        signal_max_e=float(np.max(signal_e)),
        units_note=(f"DRZ 像元单位 {bunit}（电子率）× {HDR_EXPTIME}={exptime:g} s "
                    f"⇒ 每像元电子数；DRZ 是多次曝光叠加图，不是单次曝光。"
                    f"增益来源 {gain_source}={gain!r} e⁻/ADU；"
                    f"读噪来源 {read_noise_source}：单通道 "
                    f"{[round(r, 4) for r in rn_channels]} e⁻ ⇒ 四通道合成 "
                    f"{read_noise:.4f} e⁻"),
    )
    return signal_e, photflam, provenance


def nonnegative_electrons(signal_e: np.ndarray) -> Tuple[np.ndarray, int]:
    """把模板的负值清零，返回 `(clipped, n_clipped)`。

    **为什么需要这一步**：drizzle 的插值核有负旁瓣，叠加图在低电平区会出现负像元；
    负的「电子数」不是物理量，而 `Poisson(λ)` 要求 `λ ≥ 0`。本函数是**显式的一步**，
    不是隐式截断：被清掉的像元数随 `provenance.n_negative` 与返回值一并上报，
    清零这件事对下游是可核对的。

    ⚠ 这是**合成口径**（构造数据的约定），不是对 DRZ 的科学断言。
    """
    arr = np.asarray(signal_e, dtype=np.float64)
    n_neg = int((arr < 0.0).sum())
    return (np.where(arr < 0.0, 0.0, arr), n_neg) if n_neg else (arr.copy(), 0)


# ---------------------------------------------------------------------------
# §2 PSF / 几何（纯 numpy；不用 scipy —— 它不在 TEST.md §13 白名单里）
# ---------------------------------------------------------------------------


def normalized_coords(shape: Tuple[int, int]) -> Tuple[np.ndarray, np.ndarray]:
    """归一化坐标 `x̂, ŷ ∈ [−0.5, 0.5)`，中心在窗心。

    约定写死在这里（`GRADIENT_COEFF_ORDER` / `FLAT_COEFF_ORDER` 都按它解释系数）：
    `x̂ = (x + 0.5)/W − 0.5`、`ŷ = (y + 0.5)/H − 0.5`。
    """
    h, w = shape
    xs = (np.arange(w, dtype=np.float64) + 0.5) / w - 0.5
    ys = (np.arange(h, dtype=np.float64) + 0.5) / h - 0.5
    return np.meshgrid(xs, ys)


def gaussian_psf_kernel(fwhm_pix: float, half_size: Optional[int] = None) -> np.ndarray:
    """归一化二维圆形高斯核（`Σ kernel = 1`，按构造精确归一）。

    `fwhm_pix ≤ 0` 视为**无模糊**（δ 核），返回一个中心 1.0 的单元素核。

    ⚠ 归一化是**构造级恒等**：`kernel /= kernel.sum()` 在 f64 下精确到 `u` 量级，
    对应的门限是通用 f64 档，**不是** `0.0`。本函数不是 Oracle。

    ## 独立对照：astropy 的用法与两个已踩过的坑

    `astropy.convolution.Gaussian2DKernel` 的**第一个位置参数是标准差 `x_stddev`，
    不是 FWHM**。正确对拍写法：

    ```python
    FWHM2SIG = 1.0 / (2 * math.sqrt(2 * math.log(2)))       # 0.424661
    ka = Gaussian2DKernel(fwhm * FWHM2SIG, x_size=2*half_size+1, y_size=2*half_size+1)
    ka.normalize(mode='integral')
    ```

    ⚠ 坑一：传 FWHM 当 σ 会让核宽放大 `2.355` 倍，峰值差一个量级，**且不报错**。
    ⚠ 坑二：`x_size` 不给时 astropy 用自己的默认截断宽度，与本层的 `half_size`
      不同 ⇒ 离散核形状不同，尾部几个像元有差；判内部一致性时两边要显式对齐尺寸。

    实测（随机噪声图 32×32、内部区域对拍）：
    `FWHM=1.0` 峰值 `max|Δ| = 8.9e-17`；`FWHM=2.5` `2.8e-17`；`FWHM=5.0` `5.1e-13`
    （后者随 FFT 尺寸增大而降）。⇒ 本实现的卷积与第三方独立实现一致。

    ## 离散峰 vs 连续峰

    连续高斯的峰值是 `1/(2πσ²)`；本函数返回的是**离散归一化核**的峰值，核越窄、
    取样越少，两者差得越多：实测 `FWHM=2.5` 与 `FWHM=5.0` 与连续式吻合到 9 位数字，
    而 `FWHM=1.0`（只有 9×9 个核元）离散峰值 `0.7901` 比连续式 `0.8825` 低约 10%。
    判「峰值是否为 `flux/(2πσ²)`」时**只在 `FWHM ≥ 2` 的核上判**，否则会误红。
    """
    if fwhm_pix <= 0.0:
        return np.ones((1, 1), dtype=np.float64)
    if half_size is None:
        half_size = int(math.ceil(3.0 * fwhm_pix))
    half_size = max(1, int(half_size))
    r = np.arange(-half_size, half_size + 1, dtype=np.float64)
    gx, gy = np.meshgrid(r, r)
    sigma = fwhm_pix / (2.0 * math.sqrt(2.0 * math.log(2.0)))
    k = np.exp(-(gx * gx + gy * gy) / (2.0 * sigma * sigma))
    return k / k.sum()


def convolve_same(frame: np.ndarray, kernel: np.ndarray) -> np.ndarray:
    """线性卷积，输出与输入同形。**边界口径：反射填充**（`reflect`）。

    反射填充是**合成数据的边缘约定**，不是物理断言：它保证窗边缘不会因零填充
    而产生一条假的人工暗边（那会被信噪比类判据当成真结构）。
    独立对照可用 `astropy.convolution.convolve(..., boundary='extend')` 或 `scipy.ndimage`。
    """
    img = np.asarray(frame, dtype=np.float64)
    k = np.asarray(kernel, dtype=np.float64)
    if k.ndim != 2:
        raise ValueError("kernel 必须是二维")
    kh, kw = k.shape
    ph, pw = kh // 2, kw // 2
    padded = np.pad(img, ((ph, ph), (pw, pw)), mode="reflect")
    h, w = img.shape
    big = (h + kh - 1, w + kw - 1)
    conv = np.fft.irfft2(np.fft.rfft2(padded, big) * np.fft.rfft2(k, big), big)
    # 居中偏置是 **2·ph**，不是 ph。推导：conv[n] = Σ_k k[k]·padded[n−k]，而 padded[j] = img[j−ph]；
    # 要取「核心对齐」的 out[i] = Σ_k k[k]·img[i + (kh−1)/2 − k]，令 n−k−ph = i + ph − k，得 n = i + 2·ph。
    # ⚠⚠ 这个偏置算错过一次（写成 ph）：后果是整幅图像平移 `ph` 像元（17×17 核偏 8 像元），
    #    而所有逐像素数值都"正常"——**纯居中错误不会在数值上露出来**，只会让测光位置整体错位。
    #    自检锚：δ 核过 `convolve_same` 必须**逐位等于**输入（见 `__all__` 里的备注）。
    return conv[2 * ph:2 * ph + h, 2 * pw:2 * pw + w]


def warp_bilinear(img: np.ndarray, dx: float, dy: float, rot_deg: float) -> np.ndarray:
    """反向映射 + 双线性插值的刚体变换（先转 `rot_deg`，后移 `(dx, dy)` 像元）。

    正向映射会把像元送到采样点之外并留下空洞；反向映射保证**每个输出像元都有值**，
    这是构造采样帧需要的性质（空洞会让后续信噪比判据读到假的零）。

    边界口径：采样点落到窗外的部分按**最近的窗内值**夹取（`clamp`），
    同样是合成约定，见 `convolve_same` 的同条说明。
    """
    a = np.asarray(img, dtype=np.float64)
    if a.ndim != 2:
        raise ValueError("img 必须是二维")
    if rot_deg == 0.0:
        return shift_bilinear(a, dx, dy)
    h, w = a.shape
    theta = math.radians(rot_deg)
    ct, st = math.cos(theta), math.sin(theta)
    ys, xs = np.mgrid[0:h, 0:w]
    cx, cy = (w - 1) * 0.5, (h - 1) * 0.5
    ox, oy = xs - cx, ys - cy
    # 反向映射：输出 (ox,oy) 取输入 (ct·ox + st·oy, −st·ox + ct·oy)
    sx = ct * ox + st * oy + cx
    sy = -st * ox + ct * oy + cy
    return sample_bilinear(a, sx + dx, sy + dy)


def shift_bilinear(img: np.ndarray, dx: float, dy: float) -> np.ndarray:
    """平移（正值 = 图像内容向 `+x` / `+y` 方向移动），双线性插值，边界夹取。"""
    h, w = img.shape
    xs, ys = np.meshgrid(np.arange(w, dtype=np.float64) - dx,
                         np.arange(h, dtype=np.float64) - dy)
    return sample_bilinear(img, xs, ys)


def sample_bilinear(img: np.ndarray, sx: np.ndarray, sy: np.ndarray) -> np.ndarray:
    """在 `(sx, sy)`（像元坐标，0 基）处双线性采样；越界部分夹取到窗内。"""
    a = np.asarray(img, dtype=np.float64)
    h, w = a.shape
    x = np.clip(sx, 0.0, w - 1.0)
    y = np.clip(sy, 0.0, h - 1.0)
    x0 = np.floor(x).astype(np.int64)
    y0 = np.floor(y).astype(np.int64)
    x1 = np.minimum(x0 + 1, w - 1)
    y1 = np.minimum(y0 + 1, h - 1)
    fx = x - x0
    fy = y - y0
    return (a[y0, x0] * (1.0 - fx) * (1.0 - fy)
            + a[y0, x1] * fx * (1.0 - fy)
            + a[y1, x0] * (1.0 - fx) * fy
            + a[y1, x1] * fx * fy)


def apply_pointing(psf: np.ndarray, dx: float, dy: float, rot: float,
                   seeing_fwhm: float) -> np.ndarray:
    """环节 1：视宁度（卷积）+ 指向偏移（旋转 + 平移）。

    顺序：**先视宁度后指向**。理由：视宁度是入射光在大气湍流里的 PSF 展宽，
    发生在光到达探测器之前；指向是望远镜姿态误差，发生在成像之后。因此
    `apply_pointing(apply_pointing(g, 0,0,0, f0), dx,dy,rot, 0) ≡ apply_pointing(g,dx,dy,rot,f0)`。

    - `psf`：模板图像（电子/像元），一般先经 `nonnegative_electrons`。
    - `dx, dy`：指向偏移，单位**像元**（`dy` 沿像元 `+y` 方向）。
    - `rot`：绕窗心的旋转角，单位**度**，正方向为 `x → y`。
    - `seeing_fwhm`：卷积核的 FWHM，单位**像元**；`≤ 0` 表示不加模糊。

    **不归一化总通量**：卷积核按 `Σ = 1` 归一，所以视宁度**不改变总电子数**
    （只把它摊开）。这是构造选择，写在这里以便用例判「展宽前后总通量不变」。
    """
    img = np.asarray(psf, dtype=np.float64)
    if img.ndim != 2:
        raise ValueError("psf 必须是二维")
    blurred = convolve_same(img, gaussian_psf_kernel(seeing_fwhm))
    return warp_bilinear(blurred, dx, dy, rot)


def apply_flat(frame: np.ndarray, flat_field: np.ndarray) -> np.ndarray:
    """环节 2：平场响应，**乘性**。

    口径（写死，见 `04` §2.1「加入平场响应与天空梯度」）：平场是**逐像元乘性响应**，
    作用于**光**（源 + 天光）而非电子数。物理：尘埃影与照明不均改变的是入射光的
    收集效率，不是探测效率。本层把平场放在 `forward_model` 里 λ 的**信号项**上
    （见 `_kit` 模块 docstring 的 λ 公式），暗流不乘平场 —— 暗流在硅里产生，
    在像元之间不按尘埃影调制。

    **注入面**：把 `flat_field` 换成 `1 + bias`（常数偏置）就是「平场没标定干净」的缺陷面。
    """
    a = np.asarray(frame, dtype=np.float64)
    f = np.asarray(flat_field, dtype=np.float64)
    if a.shape != f.shape:
        raise ValueError(f"frame {a.shape} 与 flat_field {f.shape} 形状不匹配")
    return a * f


def apply_sky_gradient(frame: np.ndarray, sky_electrons: float,
                       grad_coeffs: Sequence[float]) -> np.ndarray:
    """环节 3：天空梯度。**加性，在电子域**，梯度**乘在天光电平**上。

    口径（写死）：天光面是 `sky(x, y) = sky_electrons · (1 + c0 + c1·x̂ + c2·ŷ)`，
    单位**电子/像元/帧**；本函数把 `frame + sky(x, y)` 返回，**加**到源场上。

    为什么是加性而不是乘性：真实 CCD 上天光梯度来自天光照明不均 × 平场，
    它作用在**入射光**上，所以在 λ 合成里应当与「源 × 平场」并列相加；
    本层把它写成一个独立的加性天光面，是把「照明不均」与「尘埃影（平场）」两个
    物理来源拆开命名，两者都可单独注入缺陷。

    ⚠ 天光**只能**这样进入：它是 λ 的一项（见模块 docstring 的纪律一），
    绝不允许在此之后再往信噪比公式里加一次。
    """
    a = np.asarray(frame, dtype=np.float64)
    if a.ndim != 2:
        raise ValueError("frame 必须是二维")
    if len(grad_coeffs) != 3:
        raise ValueError(f"grad_coeffs 必须是 3 元组 {GRADIENT_COEFF_ORDER}，实得 {grad_coeffs!r}")
    xh, yh = normalized_coords(a.shape)
    c0, c1, c2 = (float(c) for c in grad_coeffs)
    sky_map = float(sky_electrons) * (1.0 + c0 + c1 * xh + c2 * yh)
    return a + sky_map


# ---------------------------------------------------------------------------
# §3 电子域：Poisson 采样 / 读出噪声 / 饱和 / 量化
# ---------------------------------------------------------------------------


def sample_electrons(signal_e: np.ndarray, sky_e: np.ndarray, dark_e: float,
                     rng: np.random.Generator) -> np.ndarray:
    """环节 4：**电子域 Poisson 采样**。

    ```text
    λ(x,y) = signal_e(x,y) + sky_e(x,y) + dark_e          单位：电子 / 像元 / 帧
    N_e(x,y) ~ Poisson( λ(x,y) )                          逐像元独立
    E[N_e] = λ,   Var[N_e] = λ
    ```

    - `signal_e` / `sky_e`：**电子/像元/帧**（`sky_e` 允许是标量常数天光，
      `np.asarray` 会广播；给了带梯度的天光面就逐像元用）。
    - `dark_e`：**电子/像元/帧**。
    - `rng`：**显式传入**的 `numpy.random.Generator`。本函数不碰任何全局随机状态。

    ### 「三项和一次抽样」≡「三项各自抽样」（逐字核对时的措辞差异）

    `04` §2.1 逐字写「源、天光、暗流在**电子域做 Poisson 采样**」，字面上像是三项**各自**
    抽一次；本函数实现的是三项**先求和、再抽一次**。二者**分布等价**，理由是泊松可加性：
    独立随机变量 `X_i ~ Poisson(μ_i)` 满足 `Σ X_i ~ Poisson(Σ μ_i)`（逐字见
    Newberry 1991, PASP 103, 122；Janesick 2001, SPIE PM83, Ch.2 的 CCD 噪声模型）。

    ⚠ 等价的是**分布**，不是**实现**：两者的抽样流不同 ⇒ 同一个 seed 下逐像元实现值
    不同。⇒ 用例**不得**把「逐项分别抽样」的结果当本函数输出的期望值；
    期望值只能取分布层面的统计量（均值、方差、散粒噪声斜率），
    并落 `tolerances.synth.mc.*` 的分位数法口径。

    **返回**：`int64` 计数（`TEST.md` §4 第一档：计数 = 精确一致）。

    ## 天光只经此处进入（纪律一）

    天光提高 λ ⇒ 提高泊松方差。**本函数不产生任何「独立于 λ 的天光方差项」**，
    也不提供任何绕过 λ 的天光通道。想注入「天光进分子」的缺陷，正确做法是在**被测侧**
    改公式，而不是在本构造器里加一条特殊路径 —— 本构造器只保证数据面的天光是干净的。

    ## λ 的合法性

    `λ < 0` 显式抛错（`Poisson` 的定义域）。`λ` 非有限值显式抛错
    （`TEST.md` §4.4：不把非有限值折叠成 0 再参与计算）。
    """
    lam = (np.asarray(signal_e, dtype=np.float64)
           + np.asarray(sky_e, dtype=np.float64)
           + float(dark_e))
    if not np.all(np.isfinite(lam)):
        raise ValueError("λ 含非有限值；TEST.md §4.4 不允许把它折叠成 0 或哨兵值")
    if float(np.min(lam)) < 0.0:
        raise ValueError(f"λ 必须 ≥ 0（Poisson 的定义域），实测最小 {float(np.min(lam))!r} e⁻/px")
    if not isinstance(rng, np.random.Generator):
        raise TypeError("rng 必须是显式传入的 numpy.random.Generator；"
                        "本模块没有模块级隐式随机状态（TEST.md §3 确定性）")
    return rng.poisson(lam).astype(np.int64, copy=False)


def analytic_variance_e2(signal_e: np.ndarray, sky_e: np.ndarray, dark_e: float,
                         read_noise_e: float, gain: float) -> np.ndarray:
    """**解析**方差面（电子²/像元），是本层给用例挂上去的真值，不是测量。

    ```text
    Var = λ + RN² = signal_e + sky_e + dark_e + read_noise_e²
    ```

    逐字依据 `docs/science/noise_snr/NOISE_SNR.md` §2.2 的随机方差项表：
    源光子散粒 `F·P/g`、天光光子散粒 `S_sky/g`、暗电流散粒、暗项、读出噪声
    —— 换到**电子域**（增益为 1）后前四项合并成 `λ`，读出噪声是 `RN²`。

    `gain` 只用于**记录口径**：本函数在电子域给出方差，换到 ADU 域要再除以 `g²`
    （`NOISE_SNR.md` 的单位纪律）。传进来是为了让调用点不必自己换算，
    本函数**不做**这次换算（否则「返回什么域」会变成隐式约定）。
    """
    lam = (np.asarray(signal_e, dtype=np.float64)
           + np.asarray(sky_e, dtype=np.float64)
           + float(dark_e))
    var = lam + float(read_noise_e) ** 2
    if not (gain > 0.0):
        raise ValueError("gain 必须 > 0")
    return var


def add_read_noise_adu(n_e: np.ndarray, gain: float, read_noise_e: float,
                       rng: np.random.Generator) -> np.ndarray:
    """环节 5：读出噪声在**电子域**加 Gaussian，再换算成 ADU。

    ```text
    n_e' = n_e + Normal(0, RN²)          单位：电子
    ADU  = n_e' / g                      g 的单位：电子/ADU
    ```

    - **顺序**：先在电子域加噪声再除增益。理由：`RN` 的单位是**电子**，
      它的方差项是 `(RN/g)²`（`NOISE_SNR.md` §2.2 读出噪声行）。先除增益再在
      ADU 域加 `RN/g` 的高斯在数学上等价，但**数值上不等价**（ADU 域的信噪比更差），
      本构造器选电子域，与正本的项分解同形。
    - **允许 `n_e'` 为负**：读出噪声是零均值高斯，下探到 0 e⁻ 以下是物理上正常的
      （电子计数可为负表示「相对参考电平的偏差」）。负值在下一步由量化与偏置处理，
      **这里不做钳制** —— 钳制会把读噪分布截断成一个有偏的分布。
    - `rng`：显式传入的 `Generator`。

    **返回**：`float64`，单位 ADU，**未饱和、未量化**。
    """
    if not isinstance(rng, np.random.Generator):
        raise TypeError("rng 必须是显式传入的 numpy.random.Generator")
    g = float(gain)
    if not (g > 0.0):
        raise ValueError(f"gain 必须 > 0（单位电子/ADU），实得 {gain!r}")
    rn = float(read_noise_e)
    if rn < 0.0:
        raise ValueError(f"read_noise_e 必须 ≥ 0，实得 {read_noise_e!r}")
    counts = np.asarray(n_e, dtype=np.float64)
    noisy = counts + rng.normal(0.0, rn, size=counts.shape)
    return noisy / g


def apply_saturation(adu: np.ndarray, sat_level: float) -> np.ndarray:
    """环节 6：饱和钳制，**上限截断**。

    口径：WFC3/UVIS 是正面照射探测器，满阱前响应线性、满阱后输出停在饱和电平
    （`04` §2.1「电子经增益、饱和与量化转 ADU」）。本函数是 `min(x, sat_level)`，
    **不**模拟溢出（overflow）通道 —— 正面照射探测器没有溢出通道，钳制即物理。
    下限不钳制（读噪允许把电平拉到 0 以下，见 `add_read_noise_adu`）。
    """
    sat = float(sat_level)
    if not (sat > 0.0):
        raise ValueError(f"sat_level 必须 > 0，实得 {sat_level!r}")
    return np.minimum(np.asarray(adu, dtype=np.float64), sat)


def quantize(adu: np.ndarray, n_bits: Optional[int] = None,
             full_scale_adu: Optional[float] = None) -> np.ndarray:
    """环节 7：量化。

    - `n_bits=None`：直接 round 到**整数 ADU**（`np.rint`，银行家舍入）。
      这是「读出即整数 DN」的探测器的口径。
    - `n_bits=8/16`：把 `[0, full_scale_adu]` 线性映到 `2**n_bits` 个数字量，
      `np.rint` 后夹到 `[0, 2**n_bits − 1]`。步长 `Δ = full_scale_adu / 2**n_bits`，
      往返最坏误差 `Δ/2`（门限见 `tolerances.synth.quant.half_lsb`）。

    ⚠ 量化**在 ADU 域**、**在饱和之后**：顺序与正本逐字一致
    （「电子经增益、饱和与量化转 ADU」）。饱和与量化换个次序会让满阱像元读到
    `2**n_bits − 1` 以外的值，是可注入的缺陷面。

    **返回**：`int64` 数字量（`TEST.md` §4：计数 = 精确一致）。
    """
    a = np.asarray(adu, dtype=np.float64)
    if n_bits is None:
        return np.rint(a).astype(np.int64)
    bits = int(n_bits)
    if bits not in QUANT_LEVELS:
        raise ValueError(f"n_bits 必须在 {tuple(QUANT_LEVELS)}（已冻结的量化档），实得 {n_bits!r}")
    fs = float(full_scale_adu) if full_scale_adu is not None else HST_FULL_SCALE_ADU
    if not (fs > 0.0):
        raise ValueError("full_scale_adu 必须 > 0")
    levels = QUANT_LEVELS[bits]
    dn = np.rint(a * (levels / fs))
    return np.clip(dn, 0, levels - 1).astype(np.int64)


def quant_step(n_bits: int, full_scale_adu: Optional[float] = None) -> float:
    """量化步长 `Δ = full_scale / 2**n_bits`（单位 ADU）。往返界是 `Δ/2`。"""
    bits = int(n_bits)
    if bits not in QUANT_LEVELS:
        raise ValueError(f"n_bits 必须在 {tuple(QUANT_LEVELS)}，实得 {n_bits!r}")
    fs = float(full_scale_adu) if full_scale_adu is not None else HST_FULL_SCALE_ADU
    return fs / QUANT_LEVELS[bits]


# ---------------------------------------------------------------------------
# §4 场景与帧
# ---------------------------------------------------------------------------


@dataclass(frozen=True)
class Pointing:
    """指向偏移。`dx/dy` 单位像元，`rot_deg` 单位度。"""

    dx: float
    dy: float
    rot_deg: float


@dataclass(frozen=True)
class Frame:
    """一帧合成观测。

    必含字段（任务书要求）：`adu` / `sky_electrons` / `gain` / `dark_electrons` /
    `read_noise_e` / `photflam` / `band` / `pointing` / `seeing_fwhm` / `transparency`。

    附带字段是**构造器挂上去的解析真值**与可核对读数（`n_saturated`、
    `electrons_mean` = λ、`electrons_variance` = λ + RN²）。它们让用例不必从
    `adu` 反推任何东西 —— 反推就是纪律二禁止的「以程序输出生成唯一 expected」。
    """

    adu: np.ndarray                          # 量化后的数字量，int64
    sky_electrons: np.ndarray                # 逐像元天光电子数（含梯度），float64
    gain: float                              # 电子/ADU
    dark_electrons: float                    # 电子/像元/帧
    read_noise_e: float                      # 电子 rms
    photflam: float                          # Jy
    band: str
    pointing: Pointing
    seeing_fwhm: float                       # 像元
    transparency: float                      # 无量纲，≤ 1

    electrons_mean: np.ndarray               # λ，电子/像元/帧
    electrons_variance: np.ndarray           # λ + RN²，电子²/像元
    flat_field: np.ndarray                  # 无量纲，逐像元乘性
    saturation_level_e: float                # 电子
    saturation_level_adu: float              # ADU
    n_saturated: int                         # ADU 域达到/超过饱和电平的像元计数
    n_clipped_negative: int                  # 模板负值被清零的像元数
    frame_index: int
    n_bits: int
    full_scale_adu: float

    def summary(self) -> Dict[str, Any]:
        """一行的可核对读数（供 `--verbose` / 实验报告引用）。"""
        adu = self.adu
        return {
            "frame_index": self.frame_index,
            "band": self.band,
            "transparency": self.transparency,
            "seeing_fwhm_px": self.seeing_fwhm,
            "dx_px": self.pointing.dx,
            "dy_px": self.pointing.dy,
            "rot_deg": self.pointing.rot_deg,
            "adu_median": float(np.median(adu)),
            "adu_p01": float(np.percentile(adu, 1.0)),
            "adu_p99": float(np.percentile(adu, 99.0)),
            "adu_max": int(adu.max()),
            "sky_e_median": float(np.median(self.sky_electrons)),
            "sky_e_min": float(np.min(self.sky_electrons)),
            "sky_e_max": float(np.max(self.sky_electrons)),
            "lambda_median_e": float(np.median(self.electrons_mean)),
            "lambda_var_median_e2": float(np.median(self.electrons_variance)),
            "gain_e_per_adu": self.gain,
            "read_noise_e": self.read_noise_e,
            "saturation_level_e": self.saturation_level_e,
            "n_saturated": self.n_saturated,
            "n_bits": self.n_bits,
            "photflam_Jy": self.photflam,
        }


@dataclass(frozen=True)
class Scene:
    """场景描述：**所有**逐帧变化的参数都在这里，`forward_model` / `render_sequence`
    从这里取，**不在函数体里硬编码任何物理参数当默认真相**。

    每个区间字段是 `(low, high)`，由 `render_sequence` 在其中均匀取样
    （`rng.uniform`）。区间必须给全：缺面即 `ValueError`，不静默取默认值。
    """

    template_e: np.ndarray                    # 纯信号模板，电子/像元/帧
    band: str
    photflam: float
    provenance: TemplateProvenance

    exptime_s: float
    gain_e_per_adu: float
    read_noise_e: float
    saturation_e: float
    full_scale_adu: float
    n_bits: int

    dark_electrons: float                     # 电子/像元/帧

    #: **大气透过率**（无量纲，≤ 1）。`render_sequence` 在此区间上**逐帧扫掠**。
    transparency_range: Tuple[float, float]   # 无量纲，≤ 1
    sky_electrons_range: Tuple[float, float]  # 电子/像元/帧
    sky_gradient_coeffs: Tuple[float, float, float]   # 见 GRADIENT_COEFF_ORDER
    flat_coeffs: Tuple[float, float, float, float]    # 见 FLAT_COEFF_ORDER
    seeing_fwhm_range: Tuple[float, float]    # 像元
    pointing_dx_range: Tuple[float, float]    # 像元
    pointing_dy_range: Tuple[float, float]    # 像元
    pointing_rot_range: Tuple[float, float]   # 度

    #: 可选亮星（饱和演示臂）：在模板上加一颗已知位置、已知通量的星，
    #: 使饱和分支真的被走到（否则饱和像元计数恒 0，那条判据恒绿、无证据资格）。
    bright_star: Optional["BrightStar"] = None
    #: 负例注入面（**默认值全部是「不注入」**，见 `forward_model` 的 docstring）。
    defects: Dict[str, Any] = field(default_factory=dict)

    @property
    def shape(self) -> Tuple[int, int]:
        return self.template_e.shape


def _check_range(name: str, rng_pair: Sequence[float], lo: float, hi: float) -> None:
    if len(rng_pair) != 2:
        raise ValueError(f"{name} 必须是 (low, high) 二元组，实得 {rng_pair!r}")
    if not (lo <= hi):
        raise ValueError(f"{name} 必须 low ≤ high，实得 ({lo!r}, {hi!r})")


def m16_scene(template_e: np.ndarray, photflam: float, provenance: TemplateProvenance,
              transparency_range: Tuple[float, float],
              sky_electrons_range: Tuple[float, float],
              seeing_fwhm_range: Tuple[float, float],
              pointing_dx_range: Tuple[float, float],
              pointing_dy_range: Tuple[float, float],
              pointing_rot_range: Tuple[float, float],
              sky_gradient_coeffs: Tuple[float, float, float] = (0.0, 0.0, 0.0),
              flat_coeffs: Tuple[float, float, float, float] = (0.0, 0.0, 0.0, 0.0),
              dark_electrons: float = DEFAULT_DARK_E,
              gain_e_per_adu: Optional[float] = None,
              read_noise_e: Optional[float] = None,
              saturation_e: Optional[float] = None,
              full_scale_adu: float = HST_FULL_SCALE_ADU,
              n_bits: int = DEFAULT_N_BITS,
              bright_star: Optional["BrightStar"] = None,
              defects: Optional[Dict[str, Any]] = None) -> Scene:
    """由模板 + **显式**的场景参数装配一个 `Scene`。

    ⚠ 只有**探测器常数**（增益 / 读噪 / 满量程 / 位数）在本函数里取缺省值，
    而且缺省值分别来自 FITS 头（`provenance`）或本文件的字面常量区并带出处。
    **所有逐帧变化的物理量**（透明度、天光、视宁度、指向、天光梯度、平场系数）
    **必须显式传入** —— 缺面就 `ValueError`，不静默取默认。

    `bright_star` 用 `BrightStar(x_px, y_px, flux_e)` 构造饱和演示臂：
    加一颗通量足以越过满阱的星，使饱和分支真的被走到（否则饱和像元计数恒 0，
    那条判据恒绿、无证据资格）。通量要多大按 `BrightStar` docstring 的闭式反推。
    """
    for name, pair in (("transparency_range", transparency_range),
                       ("sky_electrons_range", sky_electrons_range),
                       ("seeing_fwhm_range", seeing_fwhm_range),
                       ("pointing_dx_range", pointing_dx_range),
                       ("pointing_dy_range", pointing_dy_range),
                       ("pointing_rot_range", pointing_rot_range)):
        _check_range(name, pair, float(pair[0]), float(pair[1]))
    if len(sky_gradient_coeffs) != 3:
        raise ValueError(f"sky_gradient_coeffs 必须是 {GRADIENT_COEFF_ORDER} 三元组")
    if len(flat_coeffs) != 4:
        raise ValueError(f"flat_coeffs 必须是 {FLAT_COEFF_ORDER} 四元组")
    if float(transparency_range[1]) > 1.0 + 1e-12:
        raise ValueError(f"transparency 上界必须 ≤ 1，实得 {transparency_range[1]!r}")
    if int(n_bits) not in QUANT_LEVELS:
        raise ValueError(f"n_bits 必须在 {tuple(QUANT_LEVELS)}，实得 {n_bits!r}")

    gain = float(gain_e_per_adu) if gain_e_per_adu is not None else (
        float(provenance.gain_e_per_adu) if provenance.gain_e_per_adu is not None
        else HST_GAIN_E_PER_ADU_FALLBACK)
    rn = float(read_noise_e) if read_noise_e is not None else float(provenance.read_noise_e)
    sat = float(saturation_e) if saturation_e is not None else float(full_scale_adu) * gain

    return Scene(
        template_e=np.asarray(template_e, dtype=np.float64),
        band=provenance.band,
        photflam=float(photflam),
        provenance=provenance,
        exptime_s=float(provenance.exptime_s),
        gain_e_per_adu=gain,
        read_noise_e=rn,
        saturation_e=sat,
        full_scale_adu=float(full_scale_adu),
        n_bits=int(n_bits),
        dark_electrons=float(dark_electrons),
        transparency_range=(float(transparency_range[0]), float(transparency_range[1])),
        sky_electrons_range=(float(sky_electrons_range[0]), float(sky_electrons_range[1])),
        sky_gradient_coeffs=tuple(float(c) for c in sky_gradient_coeffs),  # type: ignore[arg-type]
        flat_coeffs=tuple(float(c) for c in flat_coeffs),                  # type: ignore[arg-type]
        seeing_fwhm_range=(float(seeing_fwhm_range[0]), float(seeing_fwhm_range[1])),
        pointing_dx_range=(float(pointing_dx_range[0]), float(pointing_dx_range[1])),
        pointing_dy_range=(float(pointing_dy_range[0]), float(pointing_dy_range[1])),
        pointing_rot_range=(float(pointing_rot_range[0]), float(pointing_rot_range[1])),
        bright_star=bright_star,
        defects=dict(defects or {}),
    )


def analytic_flat_field(shape: Tuple[int, int],
                        coeffs: Sequence[float]) -> np.ndarray:
    """平场的**闭式**真值（构造级恒等，无抽样、无估计量）。

    ```text
    flat(x,y) = 1 + a0 + a1·x̂ + a2·ŷ + a3·(x̂² + ŷ²)
    ```

    系数顺序见 `FLAT_COEFF_ORDER`。全零系数给出恒 1 的平场（纯响应缺陷面）。
    """
    if len(coeffs) != 4:
        raise ValueError(f"flat_coeffs 必须是 {FLAT_COEFF_ORDER} 四元组，实得 {coeffs!r}")
    xh, yh = normalized_coords(shape)
    a0, a1, a2, a3 = (float(c) for c in coeffs)
    return 1.0 + a0 + a1 * xh + a2 * yh + a3 * (xh * xh + yh * yh)


def analytic_sky_gradient(shape: Tuple[int, int], sky_electrons: float,
                         grad_coeffs: Sequence[float]) -> np.ndarray:
    """天光面的**闭式**真值（电子/像元/帧），与 `apply_sky_gradient` 的天光项同式。

    `apply_sky_gradient` 返回 `frame + sky_map`，本函数单独给出 `sky_map`，
    使「梯度面本身」可以与被测侧读到的天光面对拍，而不必先把源减掉。
    """
    if len(grad_coeffs) != 3:
        raise ValueError(f"grad_coeffs 必须是 {GRADIENT_COEFF_ORDER} 三元组，实得 {grad_coeffs!r}")
    xh, yh = normalized_coords(shape)
    c0, c1, c2 = (float(c) for c in grad_coeffs)
    return float(sky_electrons) * (1.0 + c0 + c1 * xh + c2 * yh)


def analytic_flat_generators(shape: Tuple[int, int],
                             flat_coeffs: Sequence[float],
                             sky_grad_coeffs: Sequence[float],
                             sky_electrons: float) -> Dict[str, np.ndarray]:
    """三个**乘性/加性生成元**的闭式解，一次给全，便于逐个对拍与逐个注入缺陷。

    | 生成元 | 口径 | 注入缺陷的方式 |
    |---|---|---|
    | `flat` | 逐像元**乘性**，作用于光（源 + 天光） | 系数置零 ⇒ 平场没标定 |
    | `sky_gradient` | 逐像元**加性**，梯度乘在天光电平上 | 系数置零 ⇒ 天光面被当成常数 |
    | `sky_flat_product` | 二者的**乘积**（照明不均 × 尘埃影） | 与逐项相乘不符 ⇒ 乘性假设错 |

    `sky_flat_product` 是给「天光梯度到底乘在源上还是乘在天光上」这条判据准备的：
    两条独立实现（逐项相加 vs 乘积）必然给出不同的面，判据才有牙齿。
    """
    flat = analytic_flat_field(shape, flat_coeffs)
    sky = analytic_sky_gradient(shape, sky_electrons, sky_grad_coeffs)
    return {"flat": flat, "sky_gradient": sky, "sky_flat_product": flat * sky}


def forward_model(scene: Scene, rng: np.random.Generator, *,
                  frame_index: int = 0,
                  transparency: Optional[float] = None,
                  sky_electrons: Optional[float] = None,
                  seeing_fwhm: Optional[float] = None,
                  pointing: Optional[Pointing] = None,
                  defects: Optional[Dict[str, Any]] = None) -> Frame:
    """把环节 1–7 串成**一帧**（`04` §2.1 的完整物理前向过程）。

    ```text
    S  = nonnegative(template) + bright_star            电子/像元/帧
    S' = apply_pointing(S, dx, dy, rot, seeing)          视宁度 + 指向
    Sf = apply_flat(S', flat)                            平场（乘性）
    G  = apply_sky_gradient(Sf, sky, grad)               天空梯度（加性，电子域）
    λ  = transparency · G + dark                         电子/像元/帧
    N  = sample_electrons(λ, 0, 0, rng)                 电子域 Poisson（天光已在 G 里）
    A  = add_read_noise_adu(N, g, RN, rng)               电子域 Gaussian 读噪 → ADU
    A  = apply_saturation(A, sat_adu)                    饱和钳制
    D  = quantize(A, n_bits, full_scale)                 量化
    ```

    ### 透明度的作用面（本层唯一的取舍，写明以便审查）

    `transparency` 是**显式的标量大气透过率**（`render_sequence` 在
    `Scene.transparency_range` 上扫它，这是 `04` §2.1 四轴里的一轴）。本层让它**乘在入射光
    上**，即同时乘**源与天光**，**不乘暗流**：

    - 源光子与天光光子都要穿过大气 ⇒ 两者一起被 `τ` 衰减；
    - 暗流在硅晶格内产生，不经过大气 ⇒ `dark` 不被 `τ` 衰减。

    若只乘源而让天光随 `τ` 变化，「不同透明度」与「不同天光」两轴就会互相污染，
    跨帧对照失去可解释性——**这是本层对「乘进源率」作的一处刻意扩大**，
    记在这里以便复核者否决。

    ### `λ` 的组装与天光的位置（纪律一）

    `apply_sky_gradient` 在**源场之后**、**电子采样之前**把天光面加进来，于是天光
    完整地落在 `sample_electrons` 的 λ 里。因此 `sample_electrons` 的 `sky_e` 传 `0.0`
    —— **天光已经并入 λ，不会被计两次**。用 `analytic_variance_e2` 复算的解析方差
    同样以 `λ + RN²` 为准。

    ### 负例注入面（`defects`）

    **默认值全部是「不注入」**，`defects` 的每个键都只在显式给出时生效：

    | 键 | 注入的缺陷 | 对应的独立判据应当变红 |
    |---|---|---|
    | `seeing_fwhm` | 视宁度不生效（核退化成 δ） | 视宁度相关的跨帧散布 |
    | `read_noise_e` | 读噪置 0 | `var ∝ λ` 的截距项 |
    | `sky_electrons` | 天光置 0 | 「天光只经散粒噪声进入」的天光档对照臂 |
    | `dark_electrons` | 暗流置 0 | 暗流散粒项 |
    | `quantize` | 量化关闭（`n_bits=None` 的效果） | 量化往返 `Δ/2` |
    | `saturation_e` | 饱和关闭（设成 `+inf`） | 饱和像元计数 > 0 |
    | `pointing_scale` | 指向偏移乘以该系数（0 ⇒ 指向不生效） | 跨帧配准 |
    | `flat_scale` | 平场乘以该系数（0 ⇒ 平场不生效） | 平场响应 |

    ⚠ 注入面只改**数据面**。注入后判据必须给出**超界读数**（而不是用例失败），
    且必须与未注入时的绿读数并列可比 —— 这是 `harness` 的负例语义。
    """
    if not isinstance(rng, np.random.Generator):
        raise TypeError("rng 必须是显式传入的 numpy.random.Generator")
    d = dict(scene.defects)
    d.update(defects or {})

    # --- 逐帧参数：缺省从 Scene 区间的中点取（**显式中点**，不是随机采样；
    #     随机采样是 render_sequence 的活，见下）
    trans = float(transparency) if transparency is not None else 0.5 * sum(scene.transparency_range)
    sky = float(sky_electrons) if sky_electrons is not None else 0.5 * sum(scene.sky_electrons_range)
    seeing = float(seeing_fwhm) if seeing_fwhm is not None else scene.seeing_fwhm_range[0]
    if pointing is None:
        pointing = Pointing(dx=0.0, dy=0.0, rot_deg=0.0)

    # --- 注入面解析
    if "seeing_fwhm" in d:
        seeing = float(d["seeing_fwhm"])
    if "sky_electrons" in d:
        sky = float(d["sky_electrons"])
    dark = float(d.get("dark_electrons", scene.dark_electrons))
    rn = float(d.get("read_noise_e", scene.read_noise_e))
    g = float(scene.gain_e_per_adu)
    ps = float(d.get("pointing_scale", 1.0))
    fs_scale = float(d.get("flat_scale", 1.0))

    # --- 环节 0：模板非负化（drizzle 核振铃；显式一步，见 nonnegative_electrons）
    base, n_neg = nonnegative_electrons(scene.template_e)
    if scene.bright_star is not None:
        # 点源先沉积，随后由 apply_pointing 的视宁度核展宽**一次**（见 BrightStar）。
        base = base + analytic_star_delta(base.shape, scene.bright_star.x_px,
                                          scene.bright_star.y_px,
                                          scene.bright_star.flux_e)

    # --- 环节 1：视宁度 + 指向
    img = apply_pointing(base, ps * pointing.dx, ps * pointing.dy, ps * pointing.rot_deg, seeing)

    # --- 环节 2：平场（乘性）
    img = apply_flat(img, analytic_flat_field(scene.shape, scene.flat_coeffs) * fs_scale)

    # --- 环节 3：天空梯度（加性，电子域）
    with_sky = apply_sky_gradient(img, sky, scene.sky_gradient_coeffs)

    # --- 电子域 λ = transparency · (源×平场 + 天光) + 暗流
    lam = trans * with_sky + dark
    flat_map = analytic_flat_field(scene.shape, scene.flat_coeffs) * fs_scale
    sky_map = analytic_sky_gradient(scene.shape, sky, scene.sky_gradient_coeffs)

    # --- 环节 4：电子域 Poisson（天光已在 λ 里，sky_e 传 0 避免二次计入）
    n_e = sample_electrons(lam, 0.0, 0.0, rng)

    # --- 解析方差（λ + RN²），作为挂给用例的真值
    var_e2 = analytic_variance_e2(lam, 0.0, 0.0, rn, g)

    # --- 环节 5：电子域读噪 → ADU
    adu = add_read_noise_adu(n_e, g, rn, rng)

    # --- 环节 6：饱和
    sat_adu = scene.saturation_e / g
    if "saturation_e" in d:
        sat_adu = float(d["saturation_e"]) / g
    saturated = apply_saturation(adu, sat_adu)
    n_sat = int((saturated >= sat_adu).sum())

    # --- 环节 7：量化
    n_bits = int(d.get("n_bits", scene.n_bits))
    dn = quantize(saturated, None if d.get("quantize", True) is False else n_bits,
                  scene.full_scale_adu)

    return Frame(
        adu=dn,
        sky_electrons=trans * sky_map,
        gain=g,
        dark_electrons=dark,
        read_noise_e=rn,
        photflam=float(scene.photflam),
        band=scene.band,
        pointing=Pointing(ps * pointing.dx, ps * pointing.dy, ps * pointing.rot_deg),
        seeing_fwhm=seeing,
        transparency=trans,
        electrons_mean=lam,
        electrons_variance=var_e2,
        flat_field=flat_map,
        saturation_level_e=scene.saturation_e,
        saturation_level_adu=sat_adu,
        n_saturated=n_sat,
        n_clipped_negative=n_neg,
        frame_index=int(frame_index),
        n_bits=0 if d.get("quantize", True) is False else n_bits,
        full_scale_adu=float(scene.full_scale_adu),
    )


@dataclass(frozen=True)
class BrightStar:
    """一颗已知位置、已知总通量的**点源**（饱和演示臂用）。

    - `x_px` / `y_px`：0 基像元坐标。
    - `flux_e`：**总**电子数（电子/帧），已知名量。

    ## 为什么是点源，而不是一颗已经带 PSF 的星

    探测器上的一颗星，其**像**就是该帧的视宁度 PSF。正确顺序是
    「先放点源 → `apply_pointing` 用视宁度**卷积一次**」。若先放一颗已带 PSF 的星、
    再让视宁度卷积一次，视宁度就被**算了两遍**（有效 `σ` 变成 `√2·σ_seeing`），
    峰值通量与饱和像元计数因此系统性偏低——这是**真缺陷**，不是口径差异。

    ## 峰值 ADU 的闭式（用来反推 `flux_e`，不要拍脑袋）

    ```text
    peak_e   ≈ flux_e · τ / (2πσ²)          σ = seeing_fwhm / (2√(2 ln 2))
    peak_adu = peak_e / g
    ```

    要真的走到饱和分支需 `peak_adu ≥ full_scale_adu`。按此反推后再乘一个余量因子；
    否则饱和像元计数恒为 0，那条判据恒绿、无证据资格（`TEST.md` §2）。
    """

    x_px: float
    y_px: float
    flux_e: float


def analytic_star_delta(shape: Tuple[int, int], x_px: float, y_px: float,
                        flux_e: float) -> np.ndarray:
    """在 `(x, y)`（0 基像元）处沉积 `flux_e` 个电子的**点源**，返回 `float64` 阵列。

    这是**闭式**沉积：单像元、无卷积。PSF 由 `apply_pointing` 的视宁度核施加一次
    （见 `BrightStar` 的「为什么是点源」）。

    位置落在窗外的半像元按最近像元归位（`round`）——这是一个**构造口径**，
    写明以便用例判位置时用同一条规则，而不是各写各的。
    """
    if not (flux_e > 0.0):
        raise ValueError(f"flux_e 必须 > 0（电子/帧），实得 {flux_e!r}")
    h, w = shape
    iy, ix = int(round(float(y_px))), int(round(float(x_px)))
    if not (0 <= iy < h and 0 <= ix < w):
        raise ValueError(
            f"亮星 ({x_px}, {y_px}) 归位到像元 ({iy}, {ix}) 后落在窗口 {(h, w)} 之外；"
            f"这不是一个可用的构造")
    d = np.zeros(shape, dtype=np.float64)
    d[iy, ix] = float(flux_e)
    return d


@dataclass(frozen=True)
class Sequence:
    """多帧序列。`frames` 按 `render_sequence` 的取样序排列。"""

    frames: Tuple[Frame, ...]
    scene: Scene
    seed: int
    draws: Tuple[Dict[str, float], ...]

    def __len__(self) -> int:
        return len(self.frames)

    def __iter__(self):
        return iter(self.frames)

    def __getitem__(self, i: int) -> Frame:
        return self.frames[i]

    def summaries(self) -> List[Dict[str, Any]]:
        return [f.summary() for f in self.frames]


def render_sequence(scene: Scene, n_frames: int, rng: np.random.Generator,
                    *, seed_label: Optional[int] = None) -> Sequence:
    """环节 8–9：生成 **n_frames** 帧，覆盖四轴：透明度 / 天光 / 视宁度 / 指向。

    `04` §2.1 逐字：「生成多帧以覆盖**不同透明度、天光、视宁度与指向**，用于检验跨帧
    定标、信噪比与无接缝叠加。」

    ⚠ **四轴在本层是显式、齐全、各自独立的**：

    | 轴 | 来源字段 | 抽样 |
    |---|---|---|
    | 大气**透明度** | `Scene.transparency_range` | `rng.uniform(low, high)`，**每帧独立** |
    | **天光** | `Scene.sky_electrons_range` | `rng.uniform(low, high)` |
    | **视宁度** | `Scene.seeing_fwhm_range` | `rng.uniform(low, high)` |
    | **指向** | `Scene.pointing_dx/dy/rot_range` | 三次 `rng.uniform(low, high)` |

    （登记：仓内既有的场景 JSON 里**没有**大气透过率轴，靠 `exposure_s` 扫掠 +
    `flux_scale` 钩子顶替。本层不沿用那种顶替——四条轴各自独立，`04` 的逐字要求才算落位。）

    ⚠ **参数全部从 `Scene` 的区间取**，函数体里**没有任何硬编码的物理默认真相**：
    `dark_electrons`、`sky_gradient_coeffs`、`flat_coeffs`、增益、读噪、饱和、位数
    是逐帧恒定的场景常量，由 `Scene` 提供，不逐帧抽。

    抽取顺序固定为 `(transparency, sky, seeing, dx, dy, rot)` ⇒ **同 seed 逐字节可复跑**
    （`TEST.md` §3）。`draws` 把每次抽样值记下来，供实验报告引用而不必重跑。

    **覆盖性不做保证、也不假装做**：均匀抽样不保证区间端点被抽到。用例若要判
    「透明度确实有跨帧变化」，判据是 `max/min > 1`（在 R 次重复下几乎必然成立），
    而不是「抽到了端点」。
    """
    if n_frames <= 0:
        raise ValueError(f"n_frames 必须 ≥ 1，实得 {n_frames!r}")
    if not isinstance(rng, np.random.Generator):
        raise TypeError("rng 必须是显式传入的 numpy.random.Generator")

    frames: List[Frame] = []
    draws: List[Dict[str, float]] = []
    for i in range(int(n_frames)):
        trans = float(rng.uniform(*scene.transparency_range))
        sky = float(rng.uniform(*scene.sky_electrons_range))
        seeing = float(rng.uniform(*scene.seeing_fwhm_range))
        dx = float(rng.uniform(*scene.pointing_dx_range))
        dy = float(rng.uniform(*scene.pointing_dy_range))
        rot = float(rng.uniform(*scene.pointing_rot_range))
        draws.append({"transparency": trans, "sky_electrons": sky,
                      "seeing_fwhm": seeing, "dx": dx, "dy": dy, "rot_deg": rot})
        frames.append(forward_model(
            scene, rng, frame_index=i, transparency=trans, sky_electrons=sky,
            seeing_fwhm=seeing, pointing=Pointing(dx, dy, rot)))
    return Sequence(frames=tuple(frames), scene=scene,
                    seed=int(seed_label) if seed_label is not None else -1,
                    draws=tuple(draws))


# ---------------------------------------------------------------------------
# §5 纯解析代数合成（04 §2.2；真值完全已知）
# ---------------------------------------------------------------------------


@dataclass(frozen=True)
class AnalyticStarfield:
    """真值完全已知的星场。

    ⚠ **`analytic_*` 与 `sampled_*` 分成两个对象，这是纪律二的关键**：前者是闭式常量，
    后者是抽样实现。拿后者去期望前者才有意义；拿前者去期望后者则是在自证。

    | 字段 | 类别 | 口径 |
    |---|---|---|
    | `total_flux_e` | **闭式** | `Σ flux_e · kernel_sum` = `Σ flux_e`（核按构造归一） |
    | `total_flux_e_outside_window` | 闭式 | 落在窗外的星点通量（截断记账） |
    | `signal_e` | 实现 | 星场铺到像素上的图像（电子/像元/帧） |
    | `signal_e_sum` | 实现 | `signal_e.sum()`（**不是**真值，是被比较量） |
    | `background_variance_e2` | **闭式** | `sky_e + dark_e + RN²` |
    | `electron_counts` | 实现 | 电子域 Poisson 抽样结果（`rng=None` 时为 `None`） |
    | `adu` | 实现 | 读噪 + 增益后的 ADU（`rng=None` 时为 `None`） |
    """

    signal_e: np.ndarray
    signal_e_sum: float
    total_flux_e: float
    total_flux_e_outside_window: float
    fluxes_e: Tuple[float, ...]
    positions: Tuple[Tuple[float, float], ...]
    sky_e: float
    dark_e: float
    gain: float
    read_noise_e: float
    background_variance_e2: float
    psf_fwhm_pix: float
    psf_kernel_sum: float
    electron_counts: Optional[np.ndarray]
    adu: Optional[np.ndarray]


def analytic_starfield(n_stars: int, fluxes_e: Sequence[float],
                       positions: Sequence[Sequence[float]], psf: Any,
                       sky_e: float, dark_e: float, gain: float,
                       read_noise_e: float,
                       rng: Optional[np.random.Generator] = None,
                       shape: Optional[Tuple[int, int]] = None) -> AnalyticStarfield:
    """构造真值完全已知的星场（`04` §2.2 逐字：「已知通量星点、已知背景、已知噪声参数，
    检验测光、方差传播、守恒映射」）。

    - `n_stars`：星数，必须与 `len(fluxes_e) == len(positions)` 一致（不一致抛错，
      不静默取短的那个）。
    - `fluxes_e`：每颗星的**总**电子数（电子/帧），已知。
    - `positions`：`(x_px, y_px)`，0 基像元坐标，已知。
    - `psf`：点扩散函数。有两条等价入参写法：
      - `float` → 视作 FWHM（像元），本函数用 `gaussian_psf_kernel` 造核；
      - 2-D 数组 → 直接当核（调用方自备），**要求其 `sum ≈ 1`**；
      - 其它 → `ValueError`（fail-closed）。
    - `sky_e` / `dark_e`：常量背景（电子/像元/帧），已知。
    - `gain`：电子/ADU。
    - `read_noise_e`：电子 rms。
    - `rng`：**`None` ⇒ 完全不抽样**（只给解析真值与期望图像）；给了才抽样。
      这是 `04` §2.2「无随机」构造器的入口。
    - `shape`：窗口形状，缺省由星点位置与 PSF 尺寸推出（有 fail-closed 规则，见下）。

    ### 闭式真值

    - `total_flux_e = Σ flux_e · kernel_sum`。核按构造归一 ⇒ `kernel_sum = 1`
      （f64 误差量级 `u`）。**这是构造级恒等，但门限不是 `0.0`**：它是浮点归一化后的和，
      属 `TEST.md` §4 的 f64 非归约档。
    - `background_variance_e2 = sky_e + dark_e + read_noise_e²`（电子²/像元），
      逐字取自 `NOISE_SNR.md` §2.2 随机方差项表。
    - `total_flux_e_outside_window`：落在窗外的星点通量，按闭式记账，
      **不假装它落在窗内**。

    ### fail-closed

    星数与通量/位置长度不一致、PSF 不是 float 或 2-D 数组、通量非有限、星点全落在窗外、
    给定 `shape` 时位置越界 —— 一律抛错。
    """
    if n_stars <= 0:
        raise ValueError(f"n_stars 必须 ≥ 1，实得 {n_stars!r}")
    fluxes = [float(f) for f in fluxes_e]
    pos = [(float(p[0]), float(p[1])) for p in positions]
    if len(fluxes) != n_stars or len(pos) != n_stars:
        raise ValueError(
            f"星数不一致：n_stars={n_stars} 但 len(fluxes_e)={len(fluxes)} "
            f"len(positions)={len(pos)}；不得静默取短的那个")
    if not all(math.isfinite(f) and f > 0.0 for f in fluxes):
        raise ValueError(f"fluxes_e 必须是正的有限值（电子/帧），实得 {fluxes!r}")

    # --- PSF 核
    if isinstance(psf, (int, float)) and not isinstance(psf, bool):
        fwhm = float(psf)
        if not (fwhm > 0.0):
            raise ValueError(f"psf 作为 float 时必须是正的 FWHM（像元），实得 {psf!r}")
        kernel = gaussian_psf_kernel(fwhm)
    else:
        kernel = np.asarray(psf, dtype=np.float64)
        if kernel.ndim != 2:
            raise ValueError(
                f"psf 必须是 float（FWHM，像元）或二维核数组，实得 "
                f"{type(psf).__name__}/ndim={getattr(kernel, 'ndim', None)}")
        fwhm = float("nan")
    if not (float(gain) > 0.0):
        raise ValueError(f"gain 必须 > 0，实得 {gain!r}")
    if float(read_noise_e) < 0.0 or float(sky_e) < 0.0 or float(dark_e) < 0.0:
        raise ValueError("sky_e / dark_e / read_noise_e 必须 ≥ 0")

    # --- 窗口形状
    if shape is None:
        kh, kw = kernel.shape
        half = max(kh, kw)
        xs = [p[0] for p in pos]
        ys = [p[1] for p in pos]
        h = int(math.ceil(max(ys) + half + 2.0))
        w = int(math.ceil(max(xs) + half + 2.0))
        h, w = max(h, kh), max(w, kw)
    else:
        h, w = int(shape[0]), int(shape[1])
        if h <= 0 or w <= 0:
            raise ValueError(f"shape 必须是正面积，实得 {shape!r}")
        for (x, y) in pos:
            if not (0.0 <= x < w and 0.0 <= y < h):
                raise ValueError(f"星点 ({x}, {y}) 落在窗口 {(h, w)} 之外")

    kernel_sum = float(kernel.sum())
    signal = np.zeros((h, w), dtype=np.float64)
    flux_in_window = 0.0
    for (x, y), f in zip(pos, fluxes):
        kh, kw = kernel.shape
        y0 = int(round(y)) - kh // 2
        x0 = int(round(x)) - kw // 2
        ys, ye = max(0, y0), min(h, y0 + kh)
        xs, xe = max(0, x0), min(w, x0 + kw)
        block = kernel[ys - y0:ye - y0, xs - x0:xe - x0]
        signal[ys:ye, xs:xe] += block * f
        flux_in_window += float(block.sum()) * f
    if flux_in_window <= 0.0:
        raise ValueError("全部星点的通量都落在窗口之外（截断后为 0）；"
                         "这不是一个可用的构造，改窗口或改 PSF 尺寸")
    if not np.all(np.isfinite(signal)):
        raise ValueError("signal_e 含非有限值")

    total_flux = sum(fluxes) * kernel_sum
    background_var = float(sky_e) + float(dark_e) + float(read_noise_e) ** 2

    counts = None
    adu = None
    if rng is not None:
        # 天光与暗流进 λ 的同一路（纪律一）；源散粒也由同一个 Poisson 给出。
        lam = signal + float(sky_e) + float(dark_e)
        counts = sample_electrons(lam, 0.0, 0.0, rng)
        adu = add_read_noise_adu(counts, gain, read_noise_e, rng)

    return AnalyticStarfield(
        signal_e=signal,
        signal_e_sum=float(signal.sum()),
        total_flux_e=float(total_flux),
        total_flux_e_outside_window=float(total_flux) - flux_in_window,
        fluxes_e=tuple(fluxes),
        positions=tuple(pos),
        sky_e=float(sky_e),
        dark_e=float(dark_e),
        gain=float(gain),
        read_noise_e=float(read_noise_e),
        background_variance_e2=background_var,
        psf_fwhm_pix=fwhm,
        psf_kernel_sum=kernel_sum,
        electron_counts=counts,
        adu=adu,
    )


def analytic_negative_arm(shape: Tuple[int, int], signal_e: np.ndarray,
                          sky_e: float, dark_e: float, read_noise_e: float,
                          gain: float,
                          rng: np.random.Generator) -> Dict[str, float]:
    """「真值无效应 ⇒ 归零或报警」负例臂（`04` §2.2 逐字）。

    构造**没有效应**的那一臂，返回可直接对拍的读数：

    | 读数 | 含义 | 归零/报警判据 |
    |---|---|---|
    | `snr_numerator_excess_e` | 把天光**错**计入信噪比分子带来的额外分子 | 真值无效应 ⇒ 该读数必须**报警**（非零且显著） |
    | `variance_identity_residual` | `Var[N_e] − λ` 的实测残差 | 天光只经散粒 ⇒ 残差在统计散布内 |
    | `variance_identity_sigma` | 该散布的解析标准差 `√λ` | 判「残差在散布内」用 |
    | `shot_variance_only_e2` | 只有散粒时的解析方差 `λ` | 与含 `RN²` 的版本对照 |

    `snr_numerator_excess_e` 的算法刻意写成**最朴素的错误式**（把天光也加进分子），
    存在的目的就是给判据一个「注入后必然超界」的读数 —— 它不是被测口径。
    """
    lam = (np.asarray(signal_e, dtype=np.float64)
           + float(sky_e) + float(dark_e))
    lam = np.where(lam > 0.0, lam, 1e-12)
    n = sample_electrons(lam, 0.0, 0.0, rng)
    src = float(np.sum(np.asarray(signal_e, dtype=np.float64)))
    sky_tot = float(sky_e) * lam.size
    var_meas = float(np.var(n.astype(np.float64)))
    var_pred = float(np.mean(lam))
    return {
        "snr_numerator_excess_e": sky_tot,
        "source_electrons_e": src,
        "variance_identity_residual": var_meas - var_pred,
        "variance_identity_sigma": math.sqrt(var_pred),
        "shot_variance_only_e2": var_pred,
        "read_noise_variance_e2": float(read_noise_e) ** 2,
        "gain_e_per_adu": float(gain),
    }


def make_rng(seed: Optional[int] = None) -> np.random.Generator:
    """造一个显式的 `Generator`。`seed=None` 时用本层冻结的 `DEFAULT_SEED`。

    ⚠ 本函数是本层**唯一**允许造随机源的入口；`rng` 必须显式往上传，
    模块里没有其它随机状态。
    """
    return np.random.default_rng(DEFAULT_SEED if seed is None else int(seed))


__all__ = [
    "REPO_ROOT", "M16_DIRNAME", "M16_BANDS", "BUNIT_ELECTRONS_PER_SECOND",
    "HDR_GAIN_CHANNELS", "HDR_READNOISE_CHANNELS", "UVIS_AMPLIFIER_CHANNELS",
    "HST_GAIN_E_PER_ADU_FALLBACK", "HST_READ_NOISE_E_PER_CHANNEL_FALLBACK",
    "HST_READ_NOISE_E_COMBINED", "HST_FULL_SCALE_ADU",
    "HST_SATURATION_E", "QUANT_LEVELS", "DEFAULT_N_BITS", "DEFAULT_SEED",
    "DEFAULT_WINDOW", "DEFAULT_WINDOW_PX", "DEFAULT_DARK_E",
    "GRADIENT_COEFF_ORDER", "FLAT_COEFF_ORDER",
    "TemplateUnavailable", "TemplateProvenance", "load_m16_template",
    "nonnegative_electrons", "normalized_coords", "gaussian_psf_kernel",
    "convolve_same", "warp_bilinear", "shift_bilinear", "sample_bilinear",
    "apply_pointing", "apply_flat", "apply_sky_gradient",
    "sample_electrons", "analytic_variance_e2", "add_read_noise_adu",
    "apply_saturation", "quantize", "quant_step",
    "Pointing", "BrightStar", "Frame", "Scene", "m16_scene",
    "analytic_flat_field", "analytic_sky_gradient", "analytic_flat_generators",
    "analytic_star_delta", "forward_model", "render_sequence", "Sequence",
    "AnalyticStarfield", "analytic_starfield", "analytic_negative_arm", "make_rng",
]