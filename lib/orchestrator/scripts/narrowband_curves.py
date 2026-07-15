# -*- coding: utf-8 -*-
"""
窄带滤光片曲线生成器 (Narrowband Filter Curve Generator)
功能: 生成方波(top-hat)窄带滤光片透过率曲线，用于光谱积分器运行时注入。
用途: 全链路整合测试中，为 H-alpha / Oiii 等窄带帧提供理论滤光片曲线，
      替代 CurveLoader 从 filters.json 加载的宽带曲线。
依赖: numpy, logging
调用示例:
    from narrowband_curves import make_narrowband_curve, BAADER_HA
    wl, trans = make_narrowband_curve(**BAADER_HA)
    # wl: numpy ndarray 波长(nm), trans: numpy ndarray 透过率[0,1]
"""

from __future__ import annotations

import logging

import numpy as np

logger = logging.getLogger(__name__)

# 波长采样的物理边界 (nm)，覆盖可见光到近红外
_WL_MIN = 300.0
_WL_MAX = 1100.0
# 带外过渡区半宽 (nm)，在通带边缘外侧采样的额外波长范围
_MARGIN_NM = 5.0


def make_narrowband_curve(center_nm, bandwidth_nm, transmittance=1.0, step=0.5):
    """生成方波滤光片曲线 (top-hat)

    在 [center - bw/2, center + bw/2] 范围内透过率 = transmittance，
    范围外透过率 = 0。波长采样从 center-bw/2-5 到 center+bw/2+5，
    并裁剪到 [300, 1100] nm 物理边界，步长 step nm。

    Args:
        center_nm: 中心波长 (nm)
        bandwidth_nm: 带宽 (nm, FWHM)
        transmittance: 带内透过率 [0,1]，默认 1.0
        step: 波长采样步长 (nm)，默认 0.5

    Returns:
        (wl_array, trans_array): 两个 numpy ndarray
            wl_array: 波长 (nm), float64, 升序
            trans_array: 透过率 [0,1], float64, 与 wl_array 等长
    """
    center_nm = float(center_nm)
    bandwidth_nm = float(bandwidth_nm)
    transmittance = float(transmittance)
    step = float(step)

    half_bw = bandwidth_nm / 2.0
    band_low = center_nm - half_bw
    band_high = center_nm + half_bw

    # 波长采样范围: 带外各延伸 _MARGIN_NM，裁剪到物理边界
    wl_start = max(band_low - _MARGIN_NM, _WL_MIN)
    wl_stop = min(band_high + _MARGIN_NM, _WL_MAX)

    if wl_stop <= wl_start:
        raise ValueError(
            f"窄带曲线波长范围无效: wl_start={wl_start}, wl_stop={wl_stop} "
            f"(center={center_nm}, bw={bandwidth_nm})"
        )

    # 生成升序波长数组 (确保包含 wl_stop)
    n_points = int(np.floor((wl_stop - wl_start) / step)) + 1
    wl_array = wl_start + np.arange(n_points, dtype=np.float64) * step
    # 防止浮点累积误差导致末点超出 wl_stop
    if wl_array[-1] > wl_stop:
        wl_array = wl_array[:-1]
    # 确保 wl_stop 被包含 (若步长未整除)
    if not np.isclose(wl_array[-1], wl_stop, atol=step * 0.01):
        wl_array = np.append(wl_array, wl_stop)

    # 方波透过率: 带内 = transmittance, 带外 = 0
    trans_array = np.zeros_like(wl_array)
    in_band = (wl_array >= band_low) & (wl_array <= band_high)
    trans_array[in_band] = transmittance

    logger.info(
        "生成窄带方波曲线: center=%.2f nm, bw=%.2f nm, trans=%.3f, "
        "步长=%.2f, 采样 %d 点, 波长范围 %.1f~%.1f nm, 带内 %d 点",
        center_nm, bandwidth_nm, transmittance, step,
        len(wl_array), float(wl_array[0]), float(wl_array[-1]),
        int(np.sum(in_band)),
    )
    return wl_array, trans_array


# ======================================================================
# Baader 窄带滤光片预设参数
# ======================================================================
BAADER_HA = {"center_nm": 656.3, "bandwidth_nm": 7.0, "transmittance": 1.0}
BAADER_OIII = {"center_nm": 500.7, "bandwidth_nm": 8.5, "transmittance": 1.0}


# ======================================================================
# 模块自验证
# ======================================================================
if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO, format="[%(levelname)s] %(name)s: %(message)s")

    print("=" * 60)
    print("narrowband_curves 模块验证")
    print("=" * 60)

    all_pass = True

    # ---- 测试 1: Baader H-alpha 预设 ----
    print("\n[测试 1] Baader H-alpha 预设")
    wl, trans = make_narrowband_curve(**BAADER_HA)
    print("  波长范围: %.1f~%.1f nm (%d 点)" % (wl[0], wl[-1], len(wl)))
    print("  带内透过率: %.2f, 带外透过率: %.2f" % (trans.max(), trans.min()))
    band_low = BAADER_HA["center_nm"] - BAADER_HA["bandwidth_nm"] / 2
    band_high = BAADER_HA["center_nm"] + BAADER_HA["bandwidth_nm"] / 2
    in_band = (wl >= band_low) & (wl <= band_high)
    ok1 = (trans[in_band] == 1.0).all() and (trans[~in_band] == 0.0).all()
    print("  [%s] 方波形状正确 (带内=1, 带外=0)" % ("PASS" if ok1 else "FAIL"))
    all_pass = all_pass and ok1

    # ---- 测试 2: Baader OIII 预设 ----
    print("\n[测试 2] Baader OIII 预设")
    wl2, trans2 = make_narrowband_curve(**BAADER_OIII)
    print("  波长范围: %.1f~%.1f nm (%d 点)" % (wl2[0], wl2[-1], len(wl2)))
    ok2 = len(wl2) > 0 and trans2.max() == 1.0
    print("  [%s] 曲线生成成功" % ("PASS" if ok2 else "FAIL"))
    all_pass = all_pass and ok2

    # ---- 测试 3: 自定义透过率 ----
    print("\n[测试 3] 自定义透过率 0.85")
    wl3, trans3 = make_narrowband_curve(656.3, 7.0, transmittance=0.85)
    ok3 = np.isclose(trans3.max(), 0.85) and np.isclose(trans3.min(), 0.0)
    print("  带内最大透过率: %.2f" % trans3.max())
    print("  [%s] 透过率参数生效" % ("PASS" if ok3 else "FAIL"))
    all_pass = all_pass and ok3

    # ---- 测试 4: 波长数组升序且步长一致 ----
    print("\n[测试 4] 波长数组属性")
    diffs = np.diff(wl)
    ok4 = (diffs > 0).all() and np.allclose(diffs, diffs[0], atol=1e-9)
    print("  步长: %.2f nm, 升序: %s" % (diffs[0], (diffs > 0).all()))
    print("  [%s] 波长数组升序且步长一致" % ("PASS" if ok4 else "FAIL"))
    all_pass = all_pass and ok4

    print("\n" + "=" * 60)
    print("验证结果: %s" % ("全部通过" if all_pass else "存在失败项"))
    print("=" * 60)
