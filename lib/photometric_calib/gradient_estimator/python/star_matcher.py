# -*- coding: utf-8 -*-
"""
Star Matcher - 星-图匹配器
功能: 将 Gaia 参考星表与图像 PSF 拟合星点进行空间匹配，建立 Gaia 星(天球坐标)与
      实测 PSF 星(像素坐标)的对应关系，生成 StarMatch 列表供梯度拟合器(GradientFitter)使用
用途: 光度定标流程的匹配阶段；合成流量 F_syn 直接取自光谱积分器输出的 JSON 文件，
      本模块不再计算合成测光，亦不依赖滤光片/QE 曲线
依赖: numpy, scipy
调用: from star_matcher import StarMatcher, StarMatch, GaiaStarPy
      sm = StarMatcher(log_dir=None)
      matches = sm.match(wcs_transform, gaia_stars, psf_results)
      cleaned, n_excl = sm.clean_outliers(matches)
数据流: gaia_stars(含 f_syn, 来自光谱积分器 JSON) + psf_results -> 空间匹配 -> StarMatch 列表

算法流程:
  1. 过滤 PSF 拟合失败的星 (status != 0)
  2. 对 PSF 有效星 (cx, cy) 建 scipy.spatial.KDTree
  3. 批量 WCS 投影 Gaia 星到像素坐标
  4. 对每颗 Gaia 星在 KDTree 中搜索最近邻 (距离 < match_radius_px)
  5. f_syn 直接从 Gaia 星输入项读取 (由光谱积分器 JSON 提供，不再实时计算)
  6. clean_outliers 用 r=log10(F_instr/F_syn) 的 MAD 稳健 sigma 裁剪离群点
"""

from __future__ import annotations

import logging
import os
from dataclasses import dataclass, replace
from typing import Optional

import numpy as np
from scipy.spatial import KDTree

logger = logging.getLogger(__name__)

_MAD_SCALE = 0.6745


@dataclass
class GaiaStarPy:
    """Gaia 星表星点 (Python 端数据结构)

    注: 新架构下 F_syn 由光谱积分器 JSON 提供，故新增 f_syn 字段；
        mag_bp/mag_rp 不再必需，保留以兼容旧代码 (用于计算 bp_rp 颜色)。
    """
    ra: float = 0.0        # 赤经 (度)
    dec: float = 0.0       # 赤纬 (度)
    mag_g: float = 0.0     # G 波段星等
    mag_bp: float = 0.0    # BP 波段星等 (可选, 缺省 0)
    mag_rp: float = 0.0    # RP 波段星等 (可选, 缺省 0)
    source_id: int = 0     # Gaia source_id
    f_syn: float = 0.0     # 合成流量 (来自光谱积分器 JSON)


@dataclass
class StarMatch:
    """单颗星-图匹配结果"""
    x: float = 0.0         # 图像像素坐标 x (0-based, 取 PSF 测量质心 cx)
    y: float = 0.0         # 图像像素坐标 y (0-based, 取 PSF 测量质心 cy)
    f_instr: float = 0.0   # 实测仪器流量 (dynamic_psf flux)
    b_local: float = 0.0   # PSF 局部背景 B
    f_syn: float = 0.0     # 合成流量 (取自光谱积分器 JSON)
    gaia_g_mag: float = 0.0  # Gaia G 星等
    gaia_id: int = 0       # Gaia source_id
    bp_rp: float = 0.0     # BP-RP 颜色 (无 mag_bp/mag_rp 时为 0.0)


class StarMatcher:
    """星-图匹配器: Gaia 星 <-> PSF 拟合星

    F_syn 直接取自 Gaia 星输入项 (由光谱积分器 JSON 提供)，本模块不依赖
    滤光片/QE 曲线，亦不进行合成测光计算。
    """

    def __init__(self, log_dir: Optional[str] = None):
        """构造星-图匹配器（无滤光片/QE依赖）

        Args:
            log_dir: 日志目录，None 则仅输出到控制台
        """
        self._logger = self._setup_logger(log_dir)
        self._logger.info("StarMatcher 初始化: 无滤光片/QE依赖, F_syn 由输入提供")

    # ------------------------------------------------------------------
    # 日志
    # ------------------------------------------------------------------

    @staticmethod
    def _setup_logger(log_dir: Optional[str]) -> logging.Logger:
        lg = logging.getLogger("star_matcher")
        lg.setLevel(logging.DEBUG)
        lg.propagate = False
        if lg.handlers:
            return lg
        sh = logging.StreamHandler()
        sh.setLevel(logging.INFO)
        sh.setFormatter(logging.Formatter("[%(levelname)s] %(message)s"))
        lg.addHandler(sh)
        if log_dir:
            os.makedirs(log_dir, exist_ok=True)
            fh = logging.FileHandler(
                os.path.join(log_dir, "star_matcher.log"), encoding="utf-8")
            fh.setLevel(logging.DEBUG)
            fh.setFormatter(
                logging.Formatter("%(asctime)s [%(levelname)s] %(message)s"))
            lg.addHandler(fh)
        return lg

    # ------------------------------------------------------------------
    # 工具: 兼容 dataclass / dict 取字段
    # ------------------------------------------------------------------

    @staticmethod
    def _get(obj, key):
        """从 dataclass 对象或 dict 取字段值"""
        if isinstance(obj, dict):
            return obj[key]
        return getattr(obj, key)

    # ------------------------------------------------------------------
    # 匹配主流程
    # ------------------------------------------------------------------

    def match(self, wcs_transform, gaia_stars, psf_results,
              match_radius_px: float = 3.0,
              outlier_sigma: float = 3.0) -> list[StarMatch]:
        """将 Gaia 星与 PSF 拟合星进行空间匹配

        Args:
            wcs_transform: WCSTransform 对象 (天球->像素)
            gaia_stars: list[dict] 或 list[dataclass]，每项含 ra, dec, mag_g, f_syn, source_id;
                        可选 mag_bp/mag_rp (用于计算 bp_rp)
            psf_results: list[DPSFFitResultPy] 或类似对象，字段 cx/cy/B/flux/status
            match_radius_px: 匹配半径 (像素)，最近邻距离须小于该值
            outlier_sigma: 透传给 match_and_clean，match 内不使用

        Returns:
            list[StarMatch]，每个匹配对的 F_instr 取自 PSF flux，F_syn 取自 Gaia 星输入
        """
        self._logger.info(
            "match 开始: Gaia 星 %d 颗, PSF 星 %d 颗, 匹配半径 %.2f px",
            len(gaia_stars), len(psf_results), match_radius_px)

        # a. 过滤 PSF 拟合失败的星 (status != 0 视为失败)
        valid_psf = [r for r in psf_results if int(self._get(r, "status")) == 0]
        self._logger.info(
            "PSF 有效星: %d / %d (过滤失败 %d)",
            len(valid_psf), len(psf_results), len(psf_results) - len(valid_psf))
        if not valid_psf or len(gaia_stars) == 0:
            self._logger.warning("无有效 PSF 星或无 Gaia 星, 返回空列表")
            return []

        # b. 提取 PSF 星 (cx, cy) 坐标数组
        psf_xy = np.array(
            [[float(self._get(r, "cx")), float(self._get(r, "cy"))] for r in valid_psf],
            dtype=np.float64)

        # c. KDTree 对 PSF 星建索引
        tree = KDTree(psf_xy)

        # d. 批量 WCS 投影 Gaia 星到像素坐标
        ra_arr = np.array([float(self._get(g, "ra")) for g in gaia_stars], dtype=np.float64)
        dec_arr = np.array([float(self._get(g, "dec")) for g in gaia_stars], dtype=np.float64)
        gx, gy = wcs_transform.sky_to_pixel_batch(ra_arr, dec_arr)
        gaia_pts = np.column_stack([gx, gy])

        # e. 对每颗 Gaia 星在 KDTree 中搜索最近邻
        dist, nn_idx = tree.query(gaia_pts, k=1)
        dist = np.atleast_1d(dist).astype(np.float64)
        nn_idx = np.atleast_1d(nn_idx).astype(np.intp)

        # f. 收集匹配对, f_syn 直接取自 Gaia 星输入 (由光谱积分器 JSON 提供)
        matches: list[StarMatch] = []
        for i in range(len(gaia_stars)):
            d = dist[i]
            if not np.isfinite(d) or d >= match_radius_px:
                self._logger.debug("Gaia 星 %d 投影 (%.2f,%.2f) 无匹配 (最近距离 %.3f px)",
                                   i, gx[i], gy[i], d)
                continue
            psf = valid_psf[int(nn_idx[i])]
            mag_g = float(self._get(gaia_stars[i], "mag_g"))
            sid = int(self._get(gaia_stars[i], "source_id"))
            # f_syn 直接从 Gaia 星输入读取 (来自光谱积分器 JSON)
            f_syn = float(self._get(gaia_stars[i], "f_syn"))

            # bp_rp: 若有 mag_bp/mag_rp 则计算，否则 0.0
            try:
                mag_bp = float(self._get(gaia_stars[i], "mag_bp"))
                mag_rp = float(self._get(gaia_stars[i], "mag_rp"))
                bp_rp = mag_bp - mag_rp
            except (KeyError, AttributeError):
                bp_rp = 0.0

            matches.append(StarMatch(
                x=float(self._get(psf, "cx")),
                y=float(self._get(psf, "cy")),
                f_instr=float(self._get(psf, "flux")),
                b_local=float(self._get(psf, "B")),
                f_syn=f_syn,
                gaia_g_mag=mag_g,
                gaia_id=sid,
                bp_rp=bp_rp,
            ))
            self._logger.debug(
                "匹配 #%d: Gaia %d -> PSF (%.2f,%.2f), dist=%.3f px, F_syn=%.4e, F_instr=%.2f",
                len(matches), sid, matches[-1].x, matches[-1].y, d, f_syn, matches[-1].f_instr)

        self._logger.info("match 完成: 匹配 %d 对", len(matches))
        return matches

    # ------------------------------------------------------------------
    # 离群点清洗
    # ------------------------------------------------------------------

    def clean_outliers(self, matches: list[StarMatch],
                       outlier_sigma: float = 3.0) -> tuple[list[StarMatch], int]:
        """基于 r = log10(F_instr / F_syn) 的 MAD 稳健 sigma 裁剪离群点

        Args:
            matches: StarMatch 列表
            outlier_sigma: 离群阈值 (倍 sigma)

        Returns:
            (清洗后的 matches, 排除数量)
        """
        n_in = len(matches)
        self._logger.info("clean_outliers: 输入 %d 颗, sigma 阈值 %.2f", n_in, outlier_sigma)
        if n_in == 0:
            return [], 0

        f_instr = np.array([m.f_instr for m in matches], dtype=np.float64)
        f_syn = np.array([m.f_syn for m in matches], dtype=np.float64)

        # 排除 F_instr <= 0 或 F_syn <= 0 的星
        valid = (f_instr > 0.0) & (f_syn > 0.0)
        n_invalid = int((~valid).sum())
        valid_idx = np.where(valid)[0]
        if valid_idx.size == 0:
            self._logger.warning("无有效匹配星 (F_instr/F_syn <= 0), 全部排除")
            return [], n_in

        r = np.log10(f_instr[valid] / f_syn[valid])
        med = float(np.median(r))
        mad = float(np.median(np.abs(r - med)))
        sigma = mad / _MAD_SCALE if mad > 0.0 else 0.0
        self._logger.info("r 中位数=%.6f, MAD=%.6f, sigma=%.6f", med, mad, sigma)

        keep = np.ones(n_in, dtype=bool)
        n_outlier = 0
        if sigma > 0.0:
            bad = np.abs(r - med) > outlier_sigma * sigma
            keep[valid_idx[bad]] = False
            n_outlier = int(bad.sum())
        # 无效星全部排除
        keep[~valid] = False

        cleaned = [m for m, k in zip(matches, keep) if k]
        n_excluded = n_in - len(cleaned)
        self._logger.info(
            "清洗完成: 保留 %d, 排除 %d (无效 %d, 离群 %d)",
            len(cleaned), n_excluded, n_invalid, n_outlier)
        return cleaned, n_excluded

    # ------------------------------------------------------------------
    # 一站式: 匹配 + 清洗
    # ------------------------------------------------------------------

    def match_and_clean(self, wcs_transform, gaia_stars, psf_results,
                        match_radius_px: float = 3.0,
                        outlier_sigma: float = 3.0) -> tuple[list[StarMatch], int]:
        """一站式: match + clean_outliers

        Returns:
            (清洗后的 matches, 排除数量)
        """
        matches = self.match(wcs_transform, gaia_stars, psf_results,
                             match_radius_px, outlier_sigma)
        cleaned, n_excluded = self.clean_outliers(matches, outlier_sigma)
        return cleaned, n_excluded

    # ------------------------------------------------------------------
    # 转数组字典 (供 GradientFitter 使用)
    # ------------------------------------------------------------------

    def to_arrays(self, matches: list[StarMatch]) -> dict:
        """将 StarMatch 列表转为 numpy 数组字典

        Returns:
            {"x","y","f_instr","b_local","f_syn","gaia_g_mag","gaia_id","bp_rp"}
        """
        return {
            "x": np.array([m.x for m in matches], dtype=np.float64),
            "y": np.array([m.y for m in matches], dtype=np.float64),
            "f_instr": np.array([m.f_instr for m in matches], dtype=np.float64),
            "b_local": np.array([m.b_local for m in matches], dtype=np.float64),
            "f_syn": np.array([m.f_syn for m in matches], dtype=np.float64),
            "gaia_g_mag": np.array([m.gaia_g_mag for m in matches], dtype=np.float64),
            "gaia_id": np.array([m.gaia_id for m in matches], dtype=np.int64),
            "bp_rp": np.array([m.bp_rp for m in matches], dtype=np.float64),
        }


# ============================================================================
# 模块自测 / 验证
# ============================================================================

if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO, format="[%(levelname)s] %(message)s")
    import inspect as _inspect
    import sys as _sys

    from wcs_transform import WCSTransform

    print("=" * 60)
    print("StarMatcher 模块自测")
    print("=" * 60)

    all_pass = True

    # ---- 简单 TAN 投影 WCS (crpix=100,100, crval=10,20, cd=0.01) ----
    crpix1, crpix2 = 100.0, 100.0
    crval1, crval2 = 10.0, 20.0
    cd_val = 0.01  # 度/像素
    wcs = WCSTransform(
        crpix1=crpix1, crpix2=crpix2,
        crval1=crval1, crval2=crval2,
        cd11=cd_val, cd12=0.0, cd21=0.0, cd22=cd_val,
    )

    # ---- 10 颗 Gaia 星 (dict 含 ra/dec/mag_g/f_syn/source_id, 小范围分布) ----
    n_stars = 10
    gaia_stars: list[dict] = []
    for i in range(n_stars):
        ra = crval1 + (i % 5) * 0.005
        dec = crval2 + (i // 5) * 0.005
        gaia_stars.append({
            "ra": ra,
            "dec": dec,
            "mag_g": 12.0 + 0.1 * i,
            "f_syn": 10000.0 + i * 500.0,
            "source_id": 1000 + i,
        })

    # ---- PSF 星: 对应 Gaia 像素位置 + 微小偏移, flux=f_syn/10, B=100, status=0 ----
    ra_arr = np.array([g["ra"] for g in gaia_stars])
    dec_arr = np.array([g["dec"] for g in gaia_stars])
    px_x, px_y = wcs.sky_to_pixel_batch(ra_arr, dec_arr)

    # flux=f_syn/10 附加微小线性扰动, 使 r=log10(F_syn/F_instr) 有自然散布 (MAD>0),
    # 便于 clean_outliers 的离群点检测; 扰动幅度 ±0.45%, 远小于离群阈值
    psf_fluxes: list[float] = []
    psf_results: list[dict] = []
    for i in range(n_stars):
        flux_i = gaia_stars[i]["f_syn"] / 10.0 * (1.0 + 0.001 * (i - 4.5))
        psf_fluxes.append(flux_i)
        psf_results.append({
            "status": 0,
            "B": 100.0,
            "flux": flux_i,
            "cx": float(px_x[i]) + 0.1,
            "cy": float(px_y[i]) - 0.1,
        })
    # 1 颗拟合失败的 PSF 星 (应被过滤)
    psf_results.append({"status": 1, "B": 0.0, "flux": 0.0, "cx": 10.0, "cy": 10.0})

    # ---- 验证 1: match() 匹配数量 ----
    print("\n[验证 1] match() 匹配数量")
    sm = StarMatcher()
    matches = sm.match(wcs, gaia_stars, psf_results, match_radius_px=3.0)
    print(f"  匹配到 {len(matches)} 对 (期望 10)")
    m1_ok = len(matches) == 10
    print(f"  [{'PASS' if m1_ok else 'FAIL'}] 匹配 10 对 (失败 PSF 星已过滤)")
    all_pass = all_pass and m1_ok

    # ---- 验证 2: f_syn 来自输入 Gaia 星 ----
    print("\n[验证 2] f_syn 来自输入 Gaia 星")
    fsyn_ok = all(
        abs(matches[i].f_syn - gaia_stars[i]["f_syn"]) < 1e-9 for i in range(n_stars)
    )
    for i in range(n_stars):
        print(f"  星 {i}: 输入 f_syn={gaia_stars[i]['f_syn']:.1f}, "
              f"匹配 f_syn={matches[i].f_syn:.1f}")
    print(f"  [{'PASS' if fsyn_ok else 'FAIL'}] 每对 f_syn 与输入一致")
    all_pass = all_pass and fsyn_ok

    # ---- 验证 3: 匹配坐标正确性 (应接近 PSF cx/cy) ----
    print("\n[验证 3] 匹配坐标正确性")
    coord_ok = True
    for i in range(n_stars):
        if abs(matches[i].x - (px_x[i] + 0.1)) > 1e-6 or \
                abs(matches[i].y - (px_y[i] - 0.1)) > 1e-6:
            coord_ok = False
            break
    print(f"  [{'PASS' if coord_ok else 'FAIL'}] 匹配坐标 == PSF 质心 cx/cy")
    all_pass = all_pass and coord_ok

    # ---- 验证 4: 字段填充正确性 (无 mag_bp/mag_rp, bp_rp=0.0) ----
    print("\n[验证 4] 字段填充正确性")
    field_ok = all(
        abs(matches[i].f_instr - psf_fluxes[i]) < 1e-9
        and matches[i].b_local == 100.0
        and matches[i].gaia_id == 1000 + i
        and matches[i].bp_rp == 0.0
        and abs(matches[i].gaia_g_mag - (12.0 + 0.1 * i)) < 1e-10
        for i in range(n_stars)
    )
    print("  F_instr/B/gaia_id/bp_rp=0.0/gaia_g_mag 全部正确")
    print(f"  [{'PASS' if field_ok else 'FAIL'}] 字段填充")
    all_pass = all_pass and field_ok

    # ---- 验证 5: clean_outliers() 无明显离群点 ----
    print("\n[验证 5] clean_outliers() 无明显离群点")
    cleaned, n_excluded = sm.clean_outliers(matches, outlier_sigma=3.0)
    print(f"  保留 {len(cleaned)}, 排除 {n_excluded} (期望排除 0)")
    m5_ok = len(cleaned) == 10 and n_excluded == 0
    print(f"  [{'PASS' if m5_ok else 'FAIL'}] 正常数据无离群点")
    all_pass = all_pass and m5_ok

    # ---- 验证 6: to_arrays() 数组形状 ----
    print("\n[验证 6] to_arrays() 数组形状")
    arrs = sm.to_arrays(matches)
    shape_ok = (
        arrs["x"].shape == (10,) and arrs["y"].shape == (10,)
        and arrs["f_instr"].shape == (10,) and arrs["b_local"].shape == (10,)
        and arrs["f_syn"].shape == (10,) and arrs["gaia_g_mag"].shape == (10,)
        and arrs["gaia_id"].shape == (10,) and arrs["bp_rp"].shape == (10,)
        and arrs["gaia_id"].dtype == np.int64
    )
    print(f"  x={arrs['x'].shape}, f_instr={arrs['f_instr'].shape}, "
          f"gaia_id dtype={arrs['gaia_id'].dtype}")
    print(f"  [{'PASS' if shape_ok else 'FAIL'}] 数组形状与 dtype")
    all_pass = all_pass and shape_ok

    # ---- 验证 7: clean_outliers() 剔除注入离群点 ----
    print("\n[验证 7] clean_outliers() 剔除注入离群点")
    matches_outlier = list(matches)
    # 将第 0 颗 F_instr 改为 1.0 (原 f_syn/10), r 偏离极大
    matches_outlier[0] = replace(matches[0], f_instr=1.0)
    cleaned2, n_excluded2 = sm.clean_outliers(matches_outlier, outlier_sigma=3.0)
    print(f"  注入 1 离群点后: 保留 {len(cleaned2)}, 排除 {n_excluded2} (期望排除 >=1)")
    m7_ok = n_excluded2 >= 1 and len(cleaned2) == 10 - n_excluded2
    print(f"  [{'PASS' if m7_ok else 'FAIL'}] 离群点被剔除")
    all_pass = all_pass and m7_ok

    # ---- 验证 8: match_and_clean() 一站式 ----
    print("\n[验证 8] match_and_clean() 一站式")
    cleaned3, n_excluded3 = sm.match_and_clean(
        wcs, gaia_stars, psf_results, match_radius_px=3.0, outlier_sigma=3.0)
    print(f"  匹配+清洗: 保留 {len(cleaned3)}, 排除 {n_excluded3}")
    m8_ok = len(cleaned3) == 10 and n_excluded3 == 0
    print(f"  [{'PASS' if m8_ok else 'FAIL'}] 一站式与分步一致")
    all_pass = all_pass and m8_ok

    # ---- 验证 9: 无 sed_builder/synthetic_photometry 依赖 + 构造函数签名 ----
    print("\n[验证 9] 无 sed_builder/synthetic_photometry 依赖")
    _mod = _sys.modules[__name__]
    ns_ok = (not hasattr(_mod, "SEDBuilder")
             and not hasattr(_mod, "SyntheticPhotometry"))
    sig_params = set(_inspect.signature(StarMatcher.__init__).parameters.keys())
    sig_ok = ("filter_wl" not in sig_params and "filter_trans" not in sig_params
              and "qe_wl" not in sig_params and "qe_val" not in sig_params
              and "log_dir" in sig_params)
    print(f"  命名空间无 SEDBuilder/SyntheticPhotometry: {ns_ok}")
    print(f"  构造函数参数: {sorted(sig_params)}")
    print(f"  [{'PASS' if ns_ok and sig_ok else 'FAIL'}] 依赖已移除 + 构造函数无滤光片/QE参数")
    all_pass = all_pass and ns_ok and sig_ok

    # ---- 验证 10: GaiaStarPy dataclass 兼容 (含 f_syn + mag_bp/mag_rp) ----
    print("\n[验证 10] GaiaStarPy dataclass 输入兼容")
    gaia_dc: list[GaiaStarPy] = []
    for i in range(n_stars):
        ra = crval1 + (i % 5) * 0.005 + 0.0001
        dec = crval2 + (i // 5) * 0.005 + 0.0001
        gaia_dc.append(GaiaStarPy(
            ra=ra, dec=dec, mag_g=12.0 + 0.1 * i,
            mag_bp=13.5, mag_rp=12.5,
            source_id=2000 + i,
            f_syn=20000.0 + i * 100.0,
        ))
    ra_arr2 = np.array([g.ra for g in gaia_dc])
    dec_arr2 = np.array([g.dec for g in gaia_dc])
    px_x2, px_y2 = wcs.sky_to_pixel_batch(ra_arr2, dec_arr2)
    psf_dc = [
        {"status": 0, "B": 100.0, "flux": g.f_syn / 10.0,
         "cx": float(px_x2[i]) + 0.1, "cy": float(px_y2[i]) - 0.1}
        for i, g in enumerate(gaia_dc)
    ]
    matches_dc = sm.match(wcs, gaia_dc, psf_dc, match_radius_px=3.0)
    dc_ok = (len(matches_dc) == 10
             and all(abs(m.bp_rp - 1.0) < 1e-10 for m in matches_dc)
             and all(abs(matches_dc[i].f_syn - gaia_dc[i].f_syn) < 1e-9
                     for i in range(10)))
    print(f"  dataclass 输入匹配 {len(matches_dc)} 对, "
          f"bp_rp={matches_dc[0].bp_rp:.1f} (期望 1.0)")
    print(f"  [{'PASS' if dc_ok else 'FAIL'}] dataclass 兼容 + bp_rp 计算 + f_syn 读取")
    all_pass = all_pass and dc_ok

    print("\n" + "=" * 60)
    print(f"测试结果: {'全部通过' if all_pass else '存在失败项'}")
    print("=" * 60)
