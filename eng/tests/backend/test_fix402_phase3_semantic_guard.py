#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""test_fix402_phase3_semantic_guard.py — FIX-402 Phase3 输入语义守卫 + 写端口单位。

依据（逐条可追）:
  * ASTROCS_DESIGN §6.3 —— 输入语义守卫（只接受面亮度语义输入; 方差/逆方差显式
    消费并传播; 其余语义显式拒绝）+ 输出模式 surface_brightness / point_source_flux
    / visualization 显式声明, visualization 标注"不可测量";
  * ASTROCS_DESIGN §5.6 —— Phase2 信号为面亮度量纲（写端口 SURFACE_BRIGHTNESS）;
  * ASTROCS_DESIGN §7.2 —— 退出码 3 = 输入缺失/格式错, 4 = 科学验证或不变量失败;
  * FZ-BUNIT-SEMANTICS / FZ-P3-BUNIT-QUADRATIC（docs/contracts/v6/data/
    01_units_and_bunit.md §1/§3/§4）。

判据（非退化: 负例矩阵必须**全红得正确**; 正例必须真产出产品而非"看起来通过"）:
  A) 合法面亮度输入（canonical "ADU/px^2" 或 裸 ADU + 像素语义声明）→ exit 0,
     产物携带 canonical BUNIT + 像素语义 provenance（props/resampled/writer/FITS）;
  B) 缺 BUNIT / 裸 ADU 缺 provenance → exit 3（error_kind=input）;
  C) 已声明非面亮度（积分通量 ADU / 方差面 / ivar 面 / 冻结表外单位）→ exit 4,
     且任一负例都不得留下 output_phase3.fits（禁半成品）;
  D) 输出模式: visualization 必须显式 measurement_capable=false 且产物无
     VARIANCE/IVAR 测量层; 声明可测量 → exit 4; point_source_flux 未实现 → 显式拒;
  E) variance 传播数值对拍: nearest 下 var_out == u_in（逐像素精确, 独立常量真值）;
     bilinear 下 var_out == Σ c_k²·u_k ≤ u_in（Σc²≠1, 排除 Σc 误归一）且 ivar == 1/var。
"""
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

from tests.backend.fixture_common import ensure_f1f2_hips, ensure_hips_unit_declaration

import numpy as np
from astropy.io import fits

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
EXE = os.path.join(REPO, "build", "astrocs")

SB_BUNIT = "ADU/px^2"
VAR_BUNIT = "ADU^2/px^4"
IVAR_BUNIT = "px^4/ADU^2"


def _clone_tree(src, dst):
    """克隆目录树（fixture 变体）: 大块 .fits 用硬链接省盘, 文本面
    （properties/manifest.json）**物理复制** —— 变体要改写它们, 硬链接会
    写穿到源 fixture（inode 共享）。"""
    for root, dirs, files in os.walk(src):
        rel = os.path.relpath(root, src)
        target = dst if rel == "." else os.path.join(dst, rel)
        os.makedirs(target, exist_ok=True)
        for name in files:
            s = os.path.join(root, name)
            d = os.path.join(target, name)
            if os.path.exists(d):
                os.remove(d)
            if name.endswith(".fits"):
                try:
                    os.link(s, d)
                    continue
                except OSError:
                    pass
            shutil.copy2(s, d)
    return dst


def _props_path(hips_dir, sub):
    return os.path.join(hips_dir, sub, "properties")


def _set_props(hips_dir, sub, set_keys, drop_keys=()):
    """按 (key, value) 列表改写子产品 properties（幂等: 先摘同键再追加）。"""
    path = _props_path(hips_dir, sub)
    with open(path, encoding="utf-8") as f:
        lines = f.read().splitlines()
    drop = set(drop_keys) | {k for k, _ in set_keys}
    keep = [ln for ln in lines if ln.split("=", 1)[0].strip() not in drop]
    keep += ["%s=%s" % (k, v) for k, v in set_keys]
    # 原子替换（新 inode）: 即便 fixture 面误用硬链接也不会写穿源产品。
    tmp = path + ".fix402.tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        f.write("\n".join(keep) + "\n")
    os.replace(tmp, path)


def _run_export(hips_dir, out_dir, mode="surface_brightness", sampler="nearest",
                measurement_capable=None, size=64, extra=None, timeout=600):
    cfg = {
        "schema_version": "1",
        "source": {"hips_dir": hips_dir},
        "center": {"ra_deg": 0.0, "dec_deg": 30.0},
        "scale_deg_per_px": 0.002,
        "width_px": size,
        "height_px": size,
        "sampler": sampler,
        "projection": "TAN",
        "coverage_output": "mask",
        "output_mode": mode,
        "output_dir": out_dir,
    }
    if measurement_capable is not None:
        cfg["measurement_capable"] = measurement_capable
    if extra:
        cfg.update(extra)
    cfg_path = out_dir + ".cfg.json"
    with open(cfg_path, "w", encoding="utf-8") as f:
        json.dump(cfg, f)
    r = subprocess.run([EXE, "export", "--json", cfg_path, "-y"],
                       capture_output=True, text=True, timeout=timeout)
    return r


def _load_json(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def _hdu(path):
    return fits.open(path, checksum=True)


class Fix402Phase3SemanticGuard(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        fdir = ensure_f1f2_hips()
        cls.src = os.path.join(fdir, "F1.hips")
        assert os.path.isdir(cls.src), cls.src
        ensure_hips_unit_declaration(cls.src)
        cls.tmp = tempfile.mkdtemp(prefix="fix402_guard_")
        cls.canon = os.path.join(cls.tmp, "canonical.hips")
        _clone_tree(cls.src, cls.canon)
        # 信号常量（用于 variance 传播数值对拍的真值来源）
        tile = os.path.join(cls.canon, "signal", "Norder0", "Dir0", "Npix0.fits")
        with _hdu(tile) as h:
            cls.sig_const = float(np.asarray(h[0].data, dtype=np.float64)[0, 0])

    def _variant(self, name):
        dst = os.path.join(self.tmp, name)
        if not os.path.isdir(dst):
            _clone_tree(self.canon, dst)
        return dst

    # ── A) 合法面亮度输入 ──────────────────────────────────────────────────
    def test_01_positive_canonical_surface_brightness(self):
        """canonical ADU/px^2 输入 → exit 0, 产物 BUNIT/像素语义齐全。"""
        out = os.path.join(self.tmp, "out_pos_canon")
        r = _run_export(self.canon, out)
        self.assertEqual(r.returncode, 0, r.stderr[-500:])
        props = _load_json(os.path.join(out, "p3_props.json"))
        self.assertEqual(props["bunit"], SB_BUNIT)
        self.assertEqual(props["bunit_input"], SB_BUNIT)
        self.assertEqual(props["pixel_semantics"], "surface_brightness")
        self.assertEqual(props["pixel_area_power"], -2)
        res = _load_json(os.path.join(out, "p3_resampled.json"))
        self.assertEqual(res["bunit"], SB_BUNIT)
        self.assertEqual(res["output_mode"], "surface_brightness")
        self.assertTrue(res["measurement_capable"])
        self.assertEqual(res["variance_propagation"], "C_out = R C_in R^T")
        wr = _load_json(os.path.join(out, "p3_writer.json"))
        self.assertEqual(wr["bunit"], SB_BUNIT)
        self.assertEqual(wr["pixel_semantics"], "surface_brightness")
        self.assertTrue(wr["measurement_capable"])
        with _hdu(os.path.join(out, "output_phase3.fits")) as h:
            self.assertEqual(h[0].header["BUNIT"], SB_BUNIT)
            self.assertEqual(h["VARIANCE"].header["BUNIT"], VAR_BUNIT)
            self.assertEqual(h["IVAR"].header["BUNIT"], IVAR_BUNIT)

    def test_02_positive_bare_adu_with_pixel_semantics(self):
        """BUNIT=ADU + 像素语义声明（FZ-BUNIT-SEMANTICS (b)）→ 放行并归一为 canonical。"""
        hips = self._variant("bare_adu_ok.hips")
        _set_props(hips, "signal", [("BUNIT", "ADU"),
                                    ("ASTROCS_PIXEL_SEMANTICS", "surface_brightness"),
                                    ("ASTROCS_PIXEL_AREA_POWER", "-2")])
        out = os.path.join(self.tmp, "out_pos_bare")
        r = _run_export(hips, out)
        self.assertEqual(r.returncode, 0, r.stderr[-500:])
        props = _load_json(os.path.join(out, "p3_props.json"))
        self.assertEqual(props["bunit"], SB_BUNIT)
        self.assertEqual(props["bunit_input"], "ADU")
        with _hdu(os.path.join(out, "output_phase3.fits")) as h:
            self.assertEqual(h[0].header["BUNIT"], SB_BUNIT)

    # ── B/C) 负例矩阵: 必须全红得正确 ─────────────────────────────────────
    def test_03_negative_matrix_all_rejected(self):
        """非面亮度/不可判输入 100% 被拒（exit 3/4 + 明确错误码 + 无半成品）。"""
        cases = [
            # name, 属性改写, 期望 rc, 期望错误码
            ("missing_bunit", [("BUNIT", None)], 3, "P3-INPUT-BUNIT-MISSING"),
            # 裸 ADU 且**摘除**像素语义声明 ⇒ 单位不可判（fixture 默认带声明,
            # 故必须显式摘键, 否则退化为 test_02 的合法形态）
            ("bare_adu_no_provenance",
             [("BUNIT", "ADU"), ("ASTROCS_PIXEL_SEMANTICS", None),
              ("ASTROCS_PIXEL_AREA_POWER", None)], 3,
             "P3-INPUT-BUNIT-UNDECIDABLE"),
            ("integrated_flux", [("BUNIT", "ADU"),
                                 ("ASTROCS_PIXEL_SEMANTICS", "integrated_flux"),
                                 ("ASTROCS_PIXEL_AREA_POWER", "0")], 4,
             "P3-INPUT-NOT-SURFACE-BRIGHTNESS"),
            ("variance_face", [("BUNIT", VAR_BUNIT)], 4,
             "P3-INPUT-NOT-SURFACE-BRIGHTNESS"),
            ("ivar_face", [("BUNIT", IVAR_BUNIT)], 4, "P3-INPUT-UNIT-UNSUPPORTED"),
            ("unknown_unit_jy_beam", [("BUNIT", "Jy/beam")], 4,
             "P3-INPUT-UNIT-UNSUPPORTED"),
            # "1" = 无量纲（不在冻结面亮度词汇内）: 已声明但不可用 ⇒ exit 4
            ("dimensionless", [("BUNIT", "1")], 4,
             "P3-INPUT-UNIT-UNSUPPORTED"),
        ]
        for name, edits, want_rc, want_code in cases:
            with self.subTest(case=name):
                hips = self._variant(name + ".hips")
                set_keys = [(k, v) for k, v in edits if v is not None]
                drop_keys = [k for k, v in edits if v is None]
                _set_props(hips, "signal", set_keys, drop_keys)
                out = os.path.join(self.tmp, "out_neg_" + name)
                r = _run_export(hips, out)
                self.assertEqual(r.returncode, want_rc,
                                 "%s: rc=%d stderr=%s" % (name, r.returncode,
                                                          r.stderr[-300:]))
                blob = r.stdout + r.stderr
                self.assertIn(want_code, blob, "%s: 缺错误码 %s" % (name, want_code))
                self.assertFalse(
                    os.path.exists(os.path.join(out, "output_phase3.fits")),
                    "%s: 负例不得留下 output_phase3.fits（半成品）" % name)

    # ── D) 输出模式显式声明 ───────────────────────────────────────────────
    def test_04_visualization_non_measurable(self):
        """visualization → 显示型降级（无测量层, manifest 标不可测量）; 冒充测量 → 拒。"""
        out = os.path.join(self.tmp, "out_vis")
        r = _run_export(self.canon, out, mode="visualization")
        self.assertEqual(r.returncode, 0, r.stderr[-500:])
        res = _load_json(os.path.join(out, "p3_resampled.json"))
        self.assertEqual(res["output_mode"], "visualization")
        self.assertFalse(res["measurement_capable"])
        self.assertFalse(res["uncertainty_available"])
        self.assertTrue(res["input_uncertainty_available"])
        wr = _load_json(os.path.join(out, "p3_writer.json"))
        self.assertFalse(wr["measurement_capable"])
        with _hdu(os.path.join(out, "output_phase3.fits")) as h:
            names = [hd.name for hd in h]
            self.assertNotIn("VARIANCE", names)
            self.assertNotIn("IVAR", names)
            self.assertEqual(h[0].header["BUNIT"], SB_BUNIT)
        # 冒充"可测量"没有配置通道: 配置面 unknown key ⇒ 显式拒, 且不留半成品
        # （可测量性由 output_mode 唯一决定, FZ-P3-MODES; 不新造同义配置键）。
        out2 = os.path.join(self.tmp, "out_vis_claim")
        r2 = _run_export(self.canon, out2, mode="visualization",
                         measurement_capable=True)
        self.assertNotEqual(r2.returncode, 0,
                            "visualization 不得冒充测量产品: " + r2.stderr[-200:])
        self.assertFalse(
            os.path.exists(os.path.join(out2, "output_phase3.fits")),
            "被拒的可测量声明不得留下产品")

    def test_05_point_source_flux_not_silently_downgraded(self):
        """未实现的 point_source_flux → 显式拒（禁静默按 surface_brightness 出产品）。"""
        out = os.path.join(self.tmp, "out_psf")
        r = _run_export(self.canon, out, mode="point_source_flux")
        self.assertNotEqual(r.returncode, 0)
        self.assertIn("point_source_flux", r.stdout + r.stderr)
        self.assertFalse(os.path.exists(os.path.join(out, "output_phase3.fits")))

    # ── E) variance 传播数值对拍 ──────────────────────────────────────────
    def _const_variance_variant(self):
        """variance 子产品 = 信号常量场（真值 C 已知; variance 优先于 ivar）。"""
        hips = self._variant("const_var.hips")
        sig_dir = os.path.join(hips, "signal", "Norder0", "Dir0")
        var_dir = os.path.join(hips, "variance", "Norder0", "Dir0")
        os.makedirs(var_dir, exist_ok=True)
        for name in os.listdir(sig_dir):
            if not name.endswith(".fits"):
                continue
            dst = os.path.join(var_dir, name)
            if os.path.exists(dst):
                os.remove(dst)
            os.link(os.path.join(sig_dir, name), dst)
        _set_props(hips, "variance", [("BUNIT", VAR_BUNIT),
                                      ("ASTROCS_PIXEL_SEMANTICS", "surface_brightness"),
                                      ("ASTROCS_PIXEL_AREA_POWER", "-4")])
        return hips

    def test_06_variance_propagation_nearest_exact(self):
        """nearest: var_out == u_in 逐像素精确（对角 C_out = R C_in R^T, c=1）。"""
        hips = self._const_variance_variant()
        out = os.path.join(self.tmp, "out_var_nearest")
        r = _run_export(hips, out, sampler="nearest")
        self.assertEqual(r.returncode, 0, r.stderr[-500:])
        with _hdu(os.path.join(out, "output_phase3.fits")) as h:
            cov = np.asarray(h["COVERAGE"].data, dtype=np.float64)
            var = np.asarray(h["VARIANCE"].data, dtype=np.float64)
            ivar = np.asarray(h["IVAR"].data, dtype=np.float64)
        cov_px = cov > 0.5
        self.assertGreater(int(cov_px.sum()), 100, "覆盖像素过少, 判据退化")
        diff = np.abs(var[cov_px] - self.sig_const)
        self.assertLess(float(diff.max()), 1e-6 * self.sig_const,
                        "nearest: var_out 必须逐像素等于 u_in (max rel dev=%g)"
                        % (float(diff.max()) / self.sig_const))
        ratio = ivar[cov_px] * var[cov_px]
        self.assertLess(float(np.abs(ratio - 1.0).max()), 1e-5,
                        "ivar 必须 == 1/variance (FZ-P3-BUNIT-QUADRATIC)")

    def test_07_variance_propagation_bilinear_sum_c_squared(self):
        """bilinear: var_out == Σ c_k²·u_k ≤ u_in（Σc²≠1 判别 Σc 误归一）。"""
        hips = self._const_variance_variant()
        out = os.path.join(self.tmp, "out_var_bilinear")
        r = _run_export(hips, out, sampler="bilinear")
        self.assertEqual(r.returncode, 0, r.stderr[-500:])
        with _hdu(os.path.join(out, "output_phase3.fits")) as h:
            cov = np.asarray(h["COVERAGE"].data, dtype=np.float64)
            var = np.asarray(h["VARIANCE"].data, dtype=np.float64)
            ivar = np.asarray(h["IVAR"].data, dtype=np.float64)
        cov_px = cov > 0.5
        v = var[cov_px]
        self.assertGreater(int(cov_px.sum()), 100, "覆盖像素过少, 判据退化")
        self.assertLessEqual(float(v.max()), self.sig_const * (1.0 + 1e-6),
                             "Σc_k² ≤ 1 ⇒ var_out ≤ u_in")
        self.assertGreater(float(v.min()), 0.2 * self.sig_const,
                           "四角双线性 Σc_k² ≥ 1/4 ⇒ var_out ≥ u_in/4")
        frac_below = float((v < 0.999 * self.sig_const).mean())
        self.assertGreater(frac_below, 0.5,
                           "Σc_k²≠1 必须体现在多数像素上（Σc 误归一 ⇒ 恒 =u_in）; "
                           "实测 frac=%.3f" % frac_below)
        ratio = ivar[cov_px] * var[cov_px]
        self.assertLess(float(np.abs(ratio - 1.0).max()), 1e-5,
                        "ivar 必须 == 1/variance (FZ-P3-BUNIT-QUADRATIC)")


if __name__ == "__main__":
    unittest.main(verbosity=2)
