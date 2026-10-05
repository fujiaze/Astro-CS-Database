r"""WCS 绝对合理性闸门的**逐行转写被测口径**（S5 + S5b）。

## 它是什么

`lib/algorithms/platesolve/cpp/ipv/src/ipv_wcs.cpp:713-770`（`extract_wcs_sip`
末段，标题「8.5 绝对合理性闸门 (RESCUE F-9)」）的 Python 逐行转写。审核包-R2
`T02-门禁退役与判据清单.md` §2.1 把这一段登记为**代码即正本**：

> S5/S5b WCS 三判据 | DISP-WCS-001 | `ipv_wcs.cpp:737-738,753,761`（**代码即正本**）

## 它不是什么

- **不是期望值的来源**。本文件是被测对象；`test_wcs_plausibility.py` 里的
  `expected` 全部取自 `ipv_wcs.cpp` 正本条款的字面常量与闭式推导，或取自独立
  科学 Oracle（`astropy`），**绝不**由本文件的输出反推
  （`docs/engineering/testing/TEST.md` §2「期望值必须由独立来源给出」）。
- **不是产品代码的替身**。本文件不 `import`、不链接、不 `ctypes` 加载任何产品
  产物；它是「把正本条款写成可执行形式」的那一份。

## 转写对照表（逐行，正本在上、本文件在下）

| 正本 `ipv_wcs.cpp` | 本文件 | 逐字要点 |
|---|---|---|
| `:732-733` `det = cd11*cd22 − cd12*cd21` | `cd_det()` | 2×2 行列式，非 `\|cd11·cd22\|` |
| `:734` `if (std::isfinite(s0) && s0 > 0.0)` | `_scale_gate_applies()` | **适用域守卫**；不满足时判据 (a) 整体不适用 |
| `:735` `scale_arcsec = std::sqrt(std::fabs(det)) * 3600.0` | `solve_scale_ratio()` | **取 fabs**：手性无关 |
| `:736` `ratio = scale_arcsec / s0` | `solve_scale_ratio()` | |
| `:737-738` `!isfinite(det) \|\| !isfinite(ratio) \|\| ratio < 0.8 \|\| ratio > 1.25` | `apply_plausibility_gate()` | **严格不等号** ⇒ 端点 `0.8` / `1.25` 本身**接受** |
| `:739-745` | 拒绝分支 | `success=false` + `error` 非空 + 确定性文本 + 立即 return |
| `:753` `!isfinite(rms_px) \|\| rms_px > 0.5` | 同上 | `>` 严格 ⇒ `rms_px == 0.5` **接受** |
| `:761` `n_pairs < 12` | 同上 | `<` 严格 ⇒ `n_pairs == 12` **接受** |

## 阈值为什么是「精确档」而不是浮点容差

`0.8 / 1.25 / 0.5 / 12` 是**判据阈值**（区间端点与门槛），正本以 `double` 字面量与
`int` 字面量写死，是判定「接受/拒绝」的**布尔分支位置**，不是浮点比较容差。
因此本文件的阈值是模块级字面常量，`test_wcs_plausibility.py` 对它们用
`harness.exact()` 判，不套 `TEST.md` §4 的 rtol/atol 档
（套 rtol 会把 `ratio = 0.8` 判成 `0.8 ± 8e-13` 从而放过 `0.8 − 1e-13`，那是把门放宽）。

## 缺陷注入的形态

缺陷**不是**改本文件，而是调用时以关键字参数替换**一处**分支行为
（`use_abs_det=False`、`enable_*_gate=False` 等）。每个开关与正本的一行一一对应，
`test_wcs_plausibility.py` 的每条负例只打开一个开关，并把实测违例量记进 `evidence`。
这样「注入后仍通过」只能意味判据没有牙齿，不可能是注入没生效或文件被改坏。
"""

from __future__ import annotations

import math
from dataclasses import dataclass

# ---------------------------------------------------------------------------
# 判据阈值（正本 ipv_wcs.cpp:738 / :753 / :761 的字面常量 —— 精确档，非浮点容差）
# ---------------------------------------------------------------------------

#: 判据 (a) 下端（`ipv_wcs.cpp:738` `ratio < 0.8`）。区间**闭**，端点本身接受。
RATIO_LO = 0.8
#: 判据 (a) 上端（`ipv_wcs.cpp:738` `ratio > 1.25`）。区间**闭**，端点本身接受。
RATIO_HI = 1.25
#: 判据 (c) 残差门（`ipv_wcs.cpp:753` `rms_px > 0.5`）。`>` 严格 ⇒ 等于接受。
RMS_MAX_PX = 0.5
#: 判据 (d) 内点门（`ipv_wcs.cpp:761` `n_pairs < 12`）。`<` 严格 ⇒ 等于接受。
N_PAIRS_MIN = 12

#: 拒绝文本的共同前缀（`ipv_wcs.cpp:741 / :756 / :764` 三个 `snprintf` 逐字共有）。
ERROR_PREFIX = "WCS 拒绝"
#: 判据 (a) 的溯源标记（`ipv_wcs.cpp:742`）。
ERROR_TAG_RATIO = "RESCUE F-9"
#: 判据 (c)(d) 的溯源标记（`ipv_wcs.cpp:756 / :764`）。
ERROR_TAG_DISP = "DISP-WCS-001 冻结语义, RESCUE F-9"

#: 三个拒绝分支的标识，用于「成对判据不可拆」守卫定位被摘掉的是哪一道门。
GATE_RATIO = "ratio"
GATE_RMS = "rms_px"
GATE_PAIRS = "n_pairs"


@dataclass(frozen=True)
class WcsGateResult:
    """`extract_wcs_sip` 末段三个拒绝分支的可观察后果。

    正本只写 `result->success = false` + `result->error`，本文件额外暴露
    `rejected_by`，仅作测试诊断用（不参与接受/拒绝判定）。
    """

    success: bool
    error: str = ""
    rejected_by: str = ""


def cd_det(cd11: float, cd12: float, cd21: float, cd22: float) -> float:
    """正本 `ipv_wcs.cpp:732-733`。

    ```cpp
    const double det = result->cd.cd11 * result->cd.cd22 -
                       result->cd.cd12 * result->cd.cd21;
    ```
    """
    return cd11 * cd22 - cd12 * cd21


def _scale_gate_applies(s0: float) -> bool:
    """正本 `ipv_wcs.cpp:734` 的适用域守卫 `std::isfinite(s0) && s0 > 0.0`。"""
    return math.isfinite(s0) and s0 > 0.0


def solve_scale_ratio(cd11: float, cd12: float, cd21: float, cd22: float,
                      s0: float, *, use_abs_det: bool = True
                      ) -> tuple[float, float, float]:
    """正本 `ipv_wcs.cpp:732-736`，返回 `(det, scale_arcsec, ratio)`。

    `use_abs_det=False` 模拟把 `std::fabs(det)` 误写成 `det` 的缺陷
    （负例 `WCS-N-DETNOSIGN`）。默认 `True` 即正本写法。
    """
    det = cd_det(cd11, cd12, cd21, cd22)
    arg = abs(det) if use_abs_det else det
    # C++ 的 std::sqrt 对负实数给 NaN；Python 的 math.sqrt 会抛 ValueError。
    scale_arcsec = math.sqrt(arg) * 3600.0 if arg >= 0.0 else math.nan
    ratio = scale_arcsec / s0
    return det, scale_arcsec, ratio


def apply_plausibility_gate(
    cd11: float, cd12: float, cd21: float, cd22: float,
    s0: float, rms_px: float, n_pairs: int,
    *,
    use_abs_det: bool = True,
    enable_ratio_gate: bool = True,
    enable_rms_gate: bool = True,
    enable_pairs_gate: bool = True,
    success_on_reject: bool = False,
    blank_error: bool = False,
    ratio_lo: float = RATIO_LO,
    ratio_hi: float = RATIO_HI,
    rms_max: float = RMS_MAX_PX,
    n_pairs_min: int = N_PAIRS_MIN,
) -> WcsGateResult:
    """正本 `ipv_wcs.cpp:731-769` 整块的逐行转写。

    默认参数**逐字等于**正本行为；每个非默认参数对应**一处**可被摘掉或改动的判定，
    用于负例。正本 `ipv_wcs.cpp:729` 逐字「不放宽任何既有阈值」，故阈值参数只用来
    模拟「阈值被放宽」的缺陷（负例必须判红）。

    | 参数 | 改动的正本位置 | 负例代号 |
    |---|---|---|
    | `use_abs_det=False` | `:735` `std::fabs(det)` → `det` | `WCS-N-DETNOSIGN` |
    | `enable_ratio_gate=False` | `:737-746` 判据 (a) 整块 | `WCS-N-NORATIO` |
    | `enable_rms_gate=False` | `:753-760` 判据 (c) | `WCS-N-NORMS` |
    | `enable_pairs_gate=False` | `:761-768` 判据 (d) | `WCS-N-NOPAIRS` |
    | `ratio_lo` / `ratio_hi` | `:738` 端点被挪动 | `WCS-N-RATLOWIDEN` |
    | `rms_max` | `:753` `0.5` 被挪动 | `WCS-N-RMSWIDEN` |
    | `n_pairs_min` | `:761` `12` 被挪动 | `WCS-N-NPAIRSWIDEN` |
    | `success_on_reject=True` | `:739/:754/:762` `success = false` | `WCS-N-SUCCOK` |
    | `blank_error=True` | `:740/:755/:763` 写 `error` | `WCS-N-NOERR` |
    """
    # ---- 判据 (a)：尺度可行域（`:731-747`）--------------------------------
    if enable_ratio_gate and _scale_gate_applies(s0):
        det, scale_arcsec, ratio = solve_scale_ratio(
            cd11, cd12, cd21, cd22, s0, use_abs_det=use_abs_det)
        if not math.isfinite(det) or not math.isfinite(ratio) \
                or ratio < ratio_lo or ratio > ratio_hi:
            error = (f'{ERROR_PREFIX}: 解出尺度 {scale_arcsec:.4f}"/px 与初值 '
                     f'{s0:.4f}"/px 之比 {ratio:.3f} 超出可行域 '
                     f'[{ratio_lo:g},{ratio_hi:g}] ({ERROR_TAG_RATIO})')
            return WcsGateResult(success_on_reject, "" if blank_error else error,
                                 GATE_RATIO)

    # ---- 判据 (c)：残差门（`:753-760`）------------------------------------
    if enable_rms_gate and (not math.isfinite(rms_px) or rms_px > rms_max):
        error = (f'{ERROR_PREFIX}: 拟合残差 rms_px={rms_px:.4f} > {rms_max:g} '
                 f'({ERROR_TAG_DISP})')
        return WcsGateResult(success_on_reject, "" if blank_error else error, GATE_RMS)

    # ---- 判据 (d)：内点门（`:761-768`）------------------------------------
    if enable_pairs_gate and n_pairs < n_pairs_min:
        error = (f'{ERROR_PREFIX}: 内点数 n_pairs={n_pairs} < {n_pairs_min} '
                 f'({ERROR_TAG_DISP})')
        return WcsGateResult(success_on_reject, "" if blank_error else error,
                             GATE_PAIRS)

    # ---- 全部通过（`:771-785` 继续走成功路径）-----------------------------
    return WcsGateResult(True, "", "")


# ---------------------------------------------------------------------------
# 夹具构造：按目标尺度比反解 CD，使 `ratio` 被**解析地**钉在指定值上
# ---------------------------------------------------------------------------
#
# 正本 `ipv_wcs.cpp:735-736`：`scale_arcsec = sqrt(|det(CD)|)·3600`，
# `ratio = scale_arcsec / s0`。故给定目标 `ratio = r` 与初值 `s0`，
# 有 `|det(CD)| = (r·s0/3600)²`，取 `k = r·s0/3600` 后
# `CD = diag(k, k)` 给出 `det = +k²`，`CD = diag(k, −k)` 给出 `det = −k²`，
# 两者 `|det|` 逐位相同 ⇒ 可直接对拍「手性无关」这一正本要求
# （`ipv_wcs.cpp:723-726` 逐字「**不以 det 符号做全局判据**」）。


def cd_for_ratio(ratio: float, s0: float, *, det_sign: int = 1,
                 shear: float = 0.0) -> tuple[float, float, float, float]:
    """构造 `CD` 使 `sqrt(|det|)·3600/s0 == ratio`，`det_sign` 给定行列式符号。

    `shear` 非零时取上三角 `[[k, shear·k], [0, k]]`，行列式仍是 `k²`，
    用来证伪「把 det 写成 `|cd11·cd22|`」这种伪实现。
    """
    k = ratio * s0 / 3600.0
    return k, shear * k, 0.0, float(det_sign) * k