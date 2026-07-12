# -*- coding: utf-8 -*-
"""
Spectrum Integrator - 光谱积分器主程序
功能: 使用 Gaia DR3SP 真实 BP/RP 光谱计算合成流量 F_syn
用途: 光谱积分器模块主程序，将 Gaia BP/RP 光谱 (uint8[343], 336-1020nm) 作为 S(λ)，
      结合滤光片透过率 T(λ) 与可选 CCD QE Q(λ)，通过 SyntheticPhotometry 引擎积分
      F_syn = ∫ S(λ)·T(λ)·Q(λ)·λ dλ，用于测光定标参考星理论流量计算
依赖: numpy, synthetic_photometry (Akima插值+Simpson积分), logging
调用: from integrator import SpectrumIntegrator
      integrator = SpectrumIntegrator(filter_wl, filter_trans, qe_wl, qe_val, spectrum_wl)
      f_syn = integrator.integrate_star(spectrum_uint8)
      results = integrator.integrate_batch(stars)
      integrator.save_results(results, output_path, "Baader R", "GSENSE2020BSI")
数据流: Gaia uint8光谱 -> float64 S(λ) -> SyntheticPhotometry.compute -> F_syn 标量
"""

from __future__ import annotations

import json
import logging
import os
from typing import List, Optional

import numpy as np

from synthetic_photometry import SyntheticPhotometry

logger = logging.getLogger(__name__)

_WL_STEP = 0.1
_SPECTRUM_SOURCE = "gaia_bp_rp"


class SpectrumIntegrator:
    """光谱积分器，对 Gaia BP/RP 光谱逐星计算合成流量 F_syn

    将 Gaia 光谱 (uint8[343], 336-1020nm) 转为 float64 作为 S(λ)，结合滤光片
    透过率 T(λ) 与可选 CCD QE Q(λ)，调用 SyntheticPhotometry.compute 积分:
        F_syn = ∫ S(λ)·T(λ)·Q(λ)·λ dλ
    """

    def __init__(self, filter_wl, filter_trans, qe_wl=None, qe_val=None, spectrum_wl=None):
        """初始化积分器

        Args:
            filter_wl, filter_trans: 滤光片波长(nm)与透过率 [0,1]
            qe_wl, qe_val: QE 波长(nm)与值 [0,1]，可选，None 则 QE=1
            spectrum_wl: Gaia 光谱波长数组 [336, 338, ..., 1020] nm；
                         None 时在首次积分时自动设置
        """
        self._filter_wl = np.asarray(filter_wl, dtype=np.float64)
        self._filter_trans = np.asarray(filter_trans, dtype=np.float64)
        self._qe_wl = np.asarray(qe_wl, dtype=np.float64) if qe_wl is not None else None
        self._qe_val = np.asarray(qe_val, dtype=np.float64) if qe_val is not None else None
        self._spectrum_wl = np.asarray(spectrum_wl, dtype=np.float64) if spectrum_wl is not None else None
        self._wl_step = _WL_STEP

        has_qe = self._qe_wl is not None and self._qe_val is not None
        logger.info(
            "初始化光谱积分器: 滤光片波长范围 %.1f~%.1f nm (%d 点), QE=%s, 光谱波长=%s",
            float(self._filter_wl[0]), float(self._filter_wl[-1]), len(self._filter_wl),
            "已提供" if has_qe else "无(Q=1)",
            "已设置" if self._spectrum_wl is not None else "待首次积分设置",
        )

    def integrate_star(self, spectrum_uint8, spectrum_wl=None, mag_g=None) -> float:
        """单颗星积分

        Args:
            spectrum_uint8: np.ndarray uint8[343]，Gaia BP/RP 光谱（归一化光谱形状）
            spectrum_wl: 可选波长数组，None 则用 self._spectrum_wl
            mag_g: Gaia G 星等，用于绝对通量归一化。
                    Gaia BP/RP uint8 光谱不含绝对通量信息，
                    需乘以 10^(-0.4*mag_g) 恢复星等对应的流量比例。

        Returns:
            F_syn: 合成流量标量
        """
        # uint8 (0-255) 转为 float64，作为归一化光谱形状 S_norm(λ)
        sed_flux = np.asarray(spectrum_uint8, dtype=np.float64)

        # 用 G 星等归一化到绝对通量: S(λ) = S_norm(λ) × 10^(-0.4×mag_g)
        if mag_g is not None and np.isfinite(mag_g):
            sed_flux = sed_flux * (10.0 ** (-0.4 * mag_g))
        else:
            logger.warning("mag_g 缺失或无效，F_syn 未做星等归一化")

        if spectrum_wl is None:
            spectrum_wl = self._spectrum_wl
        if spectrum_wl is None:
            raise ValueError(
                "未设置光谱波长数组，请在 __init__ 或 integrate_star 中传入 spectrum_wl"
            )
        spectrum_wl = np.asarray(spectrum_wl, dtype=np.float64)

        # 首次积分自动缓存波长数组
        if self._spectrum_wl is None:
            self._spectrum_wl = spectrum_wl
            logger.info(
                "自动设置光谱波长数组: %.1f~%.1f nm (%d 点)",
                float(spectrum_wl[0]), float(spectrum_wl[-1]), len(spectrum_wl),
            )

        f_syn = SyntheticPhotometry.compute(
            spectrum_wl, sed_flux,
            self._filter_wl, self._filter_trans,
            self._qe_wl, self._qe_val,
            wl_step=self._wl_step,
        )
        return f_syn

    def integrate_batch(self, stars: list) -> list:
        """批量积分

        Args:
            stars: list[GaiaSpectrumStarPy]，每颗星含 spectrum, mag_g 字段

        Returns:
            list[dict]，每项含 source_id, ra, dec, mag_g, f_syn
        """
        n = len(stars)
        logger.info("开始批量积分, 星数=%d", n)
        results: List[dict] = []
        f_values: List[float] = []

        for i, star in enumerate(stars):
            f_syn = self.integrate_star(star.spectrum, mag_g=star.mag_g)
            results.append({
                "source_id": int(getattr(star, "source_id", 0)),
                "ra": float(star.ra),
                "dec": float(star.dec),
                "mag_g": float(star.mag_g),
                "f_syn": float(f_syn),
            })
            f_values.append(f_syn)
            if (i + 1) % 100 == 0:
                logger.info("积分进度: %d/%d", i + 1, n)

        if f_values:
            f_arr = np.asarray(f_values)
            logger.info(
                "批量积分完成: 星数=%d, F_syn 范围=[%.6e, %.6e]",
                n, float(f_arr.min()), float(f_arr.max()),
            )
        else:
            logger.info("批量积分完成: 星数=0")
        return results

    def save_results(self, results, output_path, filter_name, qe_name=None,
                     ra_center=None, dec_center=None, radius_deg=None):
        """保存结果到 JSON 文件

        Args:
            results: integrate_batch 返回的 list[dict]
            output_path: 输出 JSON 路径
            filter_name: 滤光片名称
            qe_name: QE 名称 (可选)
            ra_center, dec_center, radius_deg: 搜索中心与半径 (可选)
        """
        data = {
            "filter_name": filter_name,
            "qe_name": qe_name,
            "wl_step": self._wl_step,
            "spectrum_source": _SPECTRUM_SOURCE,
            "n_stars": len(results),
            "ra_center": ra_center,
            "dec_center": dec_center,
            "radius_deg": radius_deg,
            "stars": results,
        }
        out_dir = os.path.dirname(os.path.abspath(output_path))
        if out_dir and not os.path.isdir(out_dir):
            os.makedirs(out_dir, exist_ok=True)
        with open(output_path, "w", encoding="utf-8") as f:
            json.dump(data, f, ensure_ascii=False, indent=2)
        logger.info("结果已保存: %s (%d 颗星)", output_path, len(results))

    @staticmethod
    def load_results(json_path) -> dict:
        """从 JSON 文件读取结果"""
        with open(json_path, "r", encoding="utf-8") as f:
            return json.load(f)


# ======================================================================
# 模块验证 (使用 GaiaDR3SP 真实光谱数据)
# ======================================================================
if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO, format="[%(levelname)s] %(name)s: %(message)s")

    from gaia_spectrum_client import GaiaSpectrumClient
    from curve_loader import CurveLoader

    print("=" * 60)
    print("SpectrumIntegrator 模块验证")
    print("=" * 60)

    project_root = os.path.normpath(
        os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "..")
    )
    data_dir = os.path.join(project_root, "GaiaDR3SP")
    all_pass = True

    # ---- 步骤 1: 连接 GaiaDR3SP 并锥形搜索银心方向 ----
    print("\n[步骤 1] 连接 GaiaDR3SP 并锥形搜索 (ra=266.4168, dec=-28.9833, r=0.2, mag=[10,14])")
    with GaiaSpectrumClient(data_dir=data_dir, db_type=2) as client:
        stars_all = client.cone_search_with_spectrum(266.4168, -28.9833, 0.2, 10.0, 14.0)
        spectrum_wl = client.get_wavelength_array()
    stars = stars_all[:10]
    print("  搜索到 %d 颗星, 取前 %d 颗" % (len(stars_all), len(stars)))
    print("  光谱波长: %.1f~%.1f nm (%d 点)" % (spectrum_wl[0], spectrum_wl[-1], len(spectrum_wl)))
    star_ok = len(stars) == 10
    print("  [%s] 取得 10 颗星" % ("PASS" if star_ok else "FAIL"))
    all_pass = all_pass and star_ok

    # ---- 步骤 2: 加载 Baader R 滤光片 ----
    print("\n[步骤 2] 加载 Baader R 滤光片")
    loader = CurveLoader()
    f_wl, f_trans = loader.load_filter("Baader R")
    print("  滤光片: %d 点, 范围 %.1f~%.1f nm" % (len(f_wl), f_wl[0], f_wl[-1]))

    # ---- 步骤 3: 创建积分器 ----
    print("\n[步骤 3] 创建 SpectrumIntegrator")
    integrator = SpectrumIntegrator(f_wl, f_trans, spectrum_wl=spectrum_wl)

    # ---- 步骤 4: 逐颗积分, 验证 F_syn > 0 ----
    print("\n[步骤 4] 逐颗积分 (验证 F_syn > 0)")
    single_ok = True
    for i, star in enumerate(stars):
        f_syn = integrator.integrate_star(star.spectrum)
        ok = f_syn > 0.0 and np.isfinite(f_syn)
        if not ok:
            single_ok = False
        print("  星 %d: ra=%.4f, dec=%.4f, mag_g=%.2f, F_syn=%.6e [%s]"
              % (i, star.ra, star.dec, star.mag_g, f_syn, "OK" if ok else "FAIL"))
    print("  [%s] 10 颗星 F_syn 均为正有限值" % ("PASS" if single_ok else "FAIL"))
    all_pass = all_pass and single_ok

    # ---- 步骤 5: 批量积分 ----
    print("\n[步骤 5] 批量积分 (验证返回长度=10)")
    results = integrator.integrate_batch(stars)
    batch_ok = len(results) == 10
    print("  返回列表长度=%d" % len(results))
    print("  [%s] 批量积分返回 10 项" % ("PASS" if batch_ok else "FAIL"))
    all_pass = all_pass and batch_ok

    # ---- 步骤 6: 保存 JSON + 读回一致性 ----
    print("\n[步骤 6] 保存 JSON 并读回验证一致性")
    logs_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "logs")
    os.makedirs(logs_dir, exist_ok=True)
    out_path = os.path.join(logs_dir, "integrator_verify.json")
    integrator.save_results(
        results, out_path, "Baader R", qe_name=None,
        ra_center=266.4168, dec_center=-28.9833, radius_deg=0.2,
    )
    loaded = SpectrumIntegrator.load_results(out_path)
    n_match = loaded["n_stars"] == 10
    count_match = len(loaded["stars"]) == 10
    f_match = all(
        abs(loaded["stars"][i]["f_syn"] - results[i]["f_syn"]) < 1e-9 for i in range(10)
    )
    field_match = all(
        loaded["stars"][i]["ra"] == results[i]["ra"]
        and loaded["stars"][i]["dec"] == results[i]["dec"]
        and loaded["stars"][i]["mag_g"] == results[i]["mag_g"]
        for i in range(10)
    )
    print("  n_stars=%d, stars=%d" % (loaded["n_stars"], len(loaded["stars"])))
    print("  filter_name=%s, spectrum_source=%s" % (loaded["filter_name"], loaded["spectrum_source"]))
    print("  [%s] n_stars/stars 数量一致" % ("PASS" if n_match and count_match else "FAIL"))
    print("  [%s] f_syn 数值一致" % ("PASS" if f_match else "FAIL"))
    print("  [%s] ra/dec/mag_g 字段一致" % ("PASS" if field_match else "FAIL"))
    all_pass = all_pass and n_match and count_match and f_match and field_match

    print("\n" + "=" * 60)
    print("验证结果: %s" % ("全部通过" if all_pass else "存在失败项"))
    print("=" * 60)
