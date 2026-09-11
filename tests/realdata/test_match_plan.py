# -*- coding: utf-8 -*-
"""REAL-000 匹配器测试：正例 / 策略例 / 四类负例 / T1 空集 / 滤镜归一。

全部用例确定性：构造合成文件名与母版清单（不读真实数据、不写 testdata），
匹配语义冻结于 tools/realdata/match_plan.py 顶部常量。
"""
import importlib.util
import json
import os
import sys

import pytest

_HERE = os.path.dirname(os.path.abspath(__file__))
_TOOL = os.path.normpath(os.path.join(_HERE, "..", "..", "tools", "realdata", "match_plan.py"))
_spec = importlib.util.spec_from_file_location("match_plan", _TOOL)
mp = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(mp)


# ---------------------------------------------------------------------------
# 固定母版集（与 testdata/{T2,T3,T4} calibration files 语义一致）
# ---------------------------------------------------------------------------
def make_master(kind, binning, sensor, exposure=None, filt=None, tel="T2"):
    name = f"master{kind.capitalize()}_BIN-{binning}_{sensor}"
    if kind == "dark":
        name += f"_EXPOSURE-{exposure:.2f}s"
    if kind == "flat":
        name += f"_FILTER-{filt}_mono"
    name += ".xisf"
    return {"file": name, "dir": f"{tel} calibration files", "telescope": tel,
            "path": f"testdata/{tel} calibration files/{name}",
            "kind": kind.lower(), "binning": binning, "sensor": sensor,
            "exposure_s": exposure, "filter_raw": filt, "filter": filt}


T2_MASTERS = {
    "bias": [make_master("bias", 1, "4096x4096")],
    "dark": [make_master("dark", 1, "4096x4096", 600.0),
             make_master("dark", 1, "4096x4096", 1200.0),
             make_master("dark", 1, "4096x4096", 1800.0)],
    "flat": [make_master("flat", 1, "4096x4096", filt=f) for f in
             ("Blue", "Green", "H-alpha", "OIII", "Red")],
}
T3_MASTERS = {
    "bias": [make_master("bias", 1, "4096x4096", tel="T3")],
    "dark": [make_master("dark", 1, "4096x4096", 600.0, tel="T3"),
             make_master("dark", 1, "4096x4096", 1200.0, tel="T3")],
    "flat": [make_master("flat", 1, "4096x4096", filt=f, tel="T3") for f in
             ("Blue", "Green", "H-alpha", "Lum", "Oiii", "Red")],
}
T4_MASTERS = {
    "bias": [make_master("bias", 1, "4500x3600", tel="T4")],
    "dark": [make_master("dark", 1, "4500x3600", 180.0, tel="T4"),
             make_master("dark", 1, "4500x3600", 300.0, tel="T4"),
             make_master("dark", 1, "4500x3600", 600.0, tel="T4")],
    "flat": [make_master("flat", 1, "4500x3600", filt=f, tel="T4") for f in
             ("Blue", "Green", "H-alpha", "Oiii", "Red")],
}
MASTERS = {"T2": T2_MASTERS, "T3": T3_MASTERS, "T4": T4_MASTERS}
SENSORS = {"T2": "4096x4096", "T3": "4096x4096", "T4": "4500x3600"}
MASTERS_DIR = {"T2": "testdata/T2 calibration files",
               "T3": "testdata/T3 calibration files",
               "T4": "testdata/T4 calibration files"}


def light(path, exp, filt, tel, panel=None):
    return {"path": path, "exposure_s": float(exp), "filter_raw": filt,
            "prefix": "", "telescope": tel, "panel": panel}


def match(frame, masters=MASTERS, sensors=SENSORS, dirs=MASTERS_DIR):
    return mp.match_light_frame(frame["path"],
                                {"exposure_s": frame["exposure_s"],
                                 "filter_raw": frame["filter_raw"]},
                                frame["telescope"], frame["panel"],
                                masters, sensors, dirs[frame["telescope"]])


# ---------------------------------------------------------------------------
# 正例：T4 精确匹配 180/300/600 + 5 滤镜
# ---------------------------------------------------------------------------
class TestPositiveT4:
    @pytest.mark.parametrize("exp,filt", [
        (180, "Red"), (180, "Green"), (180, "Blue"),
        (300, "H-alpha"), (600, "OIII"),
    ])
    def test_exact_match_all_filters(self, exp, filt):
        """Galaxy_Center/Victory 曝光×滤镜全部精确命中 bias/dark/flat
        （T4 五滤镜 RGBHaOIII × 精确曝光 180/300/600）。"""
        rec = match(light(f"testdata/Galaxy_Center_T4/lights/panel1/"
                          f"GC_p1-20250702@000000-{exp}s-{filt}.fts", exp, filt, "T4"))
        assert rec["status"] == "MATCHED"
        assert rec["bias"]["sensor"] == "4500x3600"
        assert rec["dark"]["exposure_s"] == float(exp)
        assert rec["dark"]["strategy"] == "EXACT_MATCH"
        assert rec["dark"]["K"] == 1.0
        assert mp.normalize_filter(rec["flat"]["filter"]) == mp.normalize_filter(filt)

    def test_victory_rgb_180(self):
        rec = match(light("testdata/Victory_Nebula_T4_Flying_Dutchman/lights/"
                          "V_mosaic1-20250204@035646-180S-Red.fts", 180, "Red", "T4"))
        assert rec["status"] == "MATCHED" and rec["dark"]["exposure_s"] == 180.0

    def test_filter_normalization_oiii_oiii(self):
        """OIII/Oiii 大小写不敏感：T4 帧滤镜 Oiii 命中 Oiii flat；T3 OIII 帧
        命中 Oiii flat（等价类）。"""
        rec = match(light("testdata/NGC55_T3_flying_dutchman/lights/"
                          "N-20250701@081412-1200S-Oiii.fts", 1200, "Oiii", "T3"))
        assert rec["status"] == "MATCHED"
        assert rec["dark"]["exposure_s"] == 1200.0
        assert rec["flat"]["filter_norm"] == "oiii"
        rec2 = match(light("x-20250101@000000-600S-OIII.fts", 600, "OIII", "T2"))
        assert rec2["status"] == "MATCHED" and rec2["flat"]["filter_norm"] == "oiii"


# ---------------------------------------------------------------------------
# 策略例：T2 300s → 600s dark，K=0.5，OPTIMAL/EXPOSURE_RATIO
# ---------------------------------------------------------------------------
class TestDarkScalePolicy:
    def test_t2_300s_scales_to_600s_k_half(self):
        """M42 T2 300s 帧无 300s dark → 600s dark 线性缩放 K=0.5。"""
        rec = match(light("testdata/M42_T2T3_mosaic_Flying_dutchman/T2/M1/"
                          "M42_M1_T2_fd-20251126@000000-300S-Blue.fts", 300, "Blue", "T2"))
        assert rec["status"] == "MATCHED"
        d = rec["dark"]
        assert d["exposure_s"] == 600.0
        assert d["K"] == pytest.approx(0.5)
        assert d["strategy"] == "SCALE_OPTIMAL"
        assert d["estimator"] == "OPTIMAL"
        assert d["fallback"] == "EXPOSURE_RATIO"
        assert d["dark_optimization"] is True

    def test_t3_300s_scales_to_600s(self):
        rec = match(light("testdata/M42_T2T3_mosaic_Flying_dutchman/T3/M6/"
                          "M42_M6_T3_fd-20251228@000000-300S-Red.fts", 300, "Red", "T3"))
        assert rec["status"] == "MATCHED" and rec["dark"]["K"] == pytest.approx(0.5)

    def test_exact_preferred_over_scale(self):
        """存在精确曝光时必须精确命中，不得走缩放。"""
        rec = match(light("x-20250101@000000-1200S-Red.fts", 1200, "Red", "T2"))
        assert rec["dark"]["strategy"] == "EXACT_MATCH" and rec["dark"]["K"] == 1.0

    def test_k_domain_documented(self):
        assert mp.K_MAX == 10.0 and mp.EXPOSURE_TOL_S == 0.01


# ---------------------------------------------------------------------------
# 负例 1：错滤镜 —— Lum 亮场无对应 flat，禁止其他滤镜顶替
# ---------------------------------------------------------------------------
class TestNegativeFilter:
    @pytest.mark.parametrize("ds,exp,tel", [
        ("NGC247_T2_flying_dutchman", 600, "T2"),    # 指令表缺口 ×15
        ("LDN43_T2素材_flying_dutchman", 600, "T2"),  # 盘点新发现 ×10
        ("Victory_Nebula_T4_Flying_Dutchman", 180, "T4"),  # 盘点新发现 ×98
    ])
    def test_no_lum_flat(self, ds, exp, tel):
        rec = match(light(f"testdata/{ds}/lights/L-20250101@000000-{exp}s-Lum.fts",
                          exp, "Lum", tel))
        assert rec["status"] == "UNMATCHED"
        assert rec["reason_code"] == "NO_LUM_FLAT"
        assert rec["flat"] is None and rec["dark"] is not None  # dark 可用，flat 缺

    def test_wrong_filter_flat_not_substituted(self):
        """显式把 Red flat 冒充 Lum flat 的清单也必须拒绝（不顶替）。"""
        masters = {"T2": {"bias": T2_MASTERS["bias"], "dark": T2_MASTERS["dark"],
                          "flat": [make_master("flat", 1, "4096x4096", filt="Red")]}}
        rec = match(light("x-20250101@000000-600S-Lum.fts", 600, "Lum", "T2"),
                    masters=masters)
        assert rec["status"] == "UNMATCHED" and rec["reason_code"] == "NO_LUM_FLAT"


# ---------------------------------------------------------------------------
# 负例 2：错曝光超策略（K 越界）
# ---------------------------------------------------------------------------
class TestNegativeExposure:
    def test_k_above_max_rejected(self):
        """K=t_light/t_dark > 10 → NO_MASTER_DARK_BEYOND_POLICY。"""
        masters = {"T2": {"bias": T2_MASTERS["bias"],
                          "dark": [make_master("dark", 1, "4096x4096", 600.0)],
                          "flat": T2_MASTERS["flat"]}}
        rec = match(light("x-20250101@000000-30000S-Red.fts", 30000, "Red", "T2"),
                    masters=masters)
        assert rec["status"] == "UNMATCHED"
        assert rec["reason_code"] == "NO_MASTER_DARK_BEYOND_POLICY"

    def test_no_dark_at_all(self):
        masters = {"T2": {"bias": T2_MASTERS["bias"], "dark": [],
                          "flat": T2_MASTERS["flat"]}}
        rec = match(light("x-20250101@000000-600S-Red.fts", 600, "Red", "T2"),
                    masters=masters)
        assert rec["status"] == "UNMATCHED" and rec["reason_code"] == "NO_MASTER_DARK"


# ---------------------------------------------------------------------------
# 负例 3：错传感器（4096 ↔ 4500）
# ---------------------------------------------------------------------------
class TestNegativeSensor:
    def test_t4_frame_vs_t2_masters_rejected(self):
        """把 T4 帧（4500x3600）交给 4096x4096 母版清单 → SENSOR_MISMATCH。"""
        rec = mp.match_light_frame("x-20250101@000000-180S-Red.fts",
                                   {"exposure_s": 180.0, "filter_raw": "Red"},
                                   "T4", None, {"T4": T2_MASTERS}, SENSORS,
                                   "testdata/T2 calibration files")
        assert rec["status"] == "UNMATCHED"
        assert rec["reason_code"] == "SENSOR_MISMATCH"
        assert rec["detail"]["master_sensors"] == ["4096x4096"]

    def test_t2_frame_vs_t4_masters_rejected(self):
        rec = mp.match_light_frame("x-20250101@000000-600S-Blue.fts",
                                   {"exposure_s": 600.0, "filter_raw": "Blue"},
                                   "T2", None, {"T2": T4_MASTERS}, SENSORS,
                                   "testdata/T4 calibration files")
        assert rec["status"] == "UNMATCHED" and rec["reason_code"] == "SENSOR_MISMATCH"


# ---------------------------------------------------------------------------
# 负例 4：错望远镜目录
# ---------------------------------------------------------------------------
class TestNegativeTelescope:
    def test_unknown_telescope_rejected(self):
        rec = mp.match_light_frame("x-20250101@000000-600S-Red.fts",
                                   {"exposure_s": 600.0, "filter_raw": "Red"},
                                   "T9", None, MASTERS, SENSORS, None)
        assert rec["status"] == "UNMATCHED"
        assert rec["reason_code"] == "TELESCOPE_MISMATCH"

    def test_missing_masters_dir_rejected(self):
        rec = mp.match_light_frame("x-20250101@000000-600S-Red.fts",
                                   {"exposure_s": 600.0, "filter_raw": "Red"},
                                   "T2", None, {"T2": None}, SENSORS, None)
        assert rec["status"] == "UNMATCHED"
        assert rec["reason_code"] == "TELESCOPE_MISMATCH"


# ---------------------------------------------------------------------------
# T1 显式空集用例：枚举 0 帧 0 母版 → empty(PASS)，不是 skip
# ---------------------------------------------------------------------------
class TestT1EmptySet:
    def test_t1_enumerates_zero_frames_zero_masters(self):
        """T1 目录存在但 0 母版 → 空 bucket（或未登记），均计 0 母版 0 帧。"""
        t1_masters = mp.build_masters_by_telescope(_t1_only_tree(tmp_tree()))
        bucket = t1_masters.get("T1")
        if bucket is not None:
            assert bucket["bias"] == [] and bucket["dark"] == [] \
                and bucket["flat"] == []

    def test_plan_records_t1_empty_pass(self, tmp_path):
        index = {"datasets": [], "telescopes": {
            "T1": {"status": "not_available", "calibration_masters_dir": None}}}
        (tmp_path / "index.json").write_text(json.dumps(index), encoding="utf-8")
        (tmp_path / "T1 calibration files").mkdir()
        # 空目录：T1 枚举 0 帧 0 母版
        masters = mp.build_masters_by_telescope(str(tmp_path))
        bucket = masters.get("T1") or {"bias": [], "dark": [], "flat": []}
        assert bucket["bias"] == [] and bucket["dark"] == [] \
            and bucket["flat"] == []  # 0 母版
        plan = {"datasets": {"T1": {
            "frames": [], "master_records": [],
            "summary": {"lights": 0, "matched": 0, "unmatched": 0,
                        "unmatched_reasons": {}},
            "status": "empty(PASS)"}}}
        assert plan["datasets"]["T1"]["status"] == "empty(PASS)"
        assert plan["datasets"]["T1"]["frames"] == []
        assert plan["datasets"]["T1"]["master_records"] == []


def tmp_tree():
    return os.path.join(os.environ.get("TMPDIR", "/tmp"), "astrocs_real000_t1")


def _t1_only_tree(root):
    os.makedirs(root, exist_ok=True)
    os.makedirs(os.path.join(root, "T1 calibration files"), exist_ok=True)
    return root


# ---------------------------------------------------------------------------
# 文件名解析与盘点分类（回归保护）
# ---------------------------------------------------------------------------
class TestParsing:
    def test_light_filename_case_insensitive_suffix(self):
        p = mp.parse_light_filename(
            "M42_M2_T3_flying_dutchman-20251226@044713-600S-H-alpha.fts")
        assert p["exposure_s"] == 600.0 and p["filter_raw"] == "H-alpha"
        p2 = mp.parse_light_filename(
            "Victory_Nebula_mosaic1_flying_dutchman-20250204@035646-180s-Lum.fts")
        assert p2["exposure_s"] == 180.0

    def test_master_filename(self):
        p = mp.parse_master_filename(
            "masterDark_BIN-1_4500x3600_EXPOSURE-300.00s.xisf")
        assert p == {"kind": "dark", "binning": 1, "sensor": "4500x3600",
                     "exposure_s": 300.0, "filter_raw": None}
        p2 = mp.parse_master_filename("masterFlat_BIN-1_4096x4096_FILTER-Oiii_mono.xisf")
        assert p2["kind"] == "flat" and p2["filter_raw"] == "Oiii"

    def test_normalize_filter_equivalence_classes(self):
        for a, b in [("OIII", "Oiii"), ("H-alpha", "h-alpha"), ("Lum", "LUM "),
                     ("H_alpha", "H-alpha")]:
            assert mp.normalize_filter(a) == mp.normalize_filter(b)

    def test_normalize_filter_rejects_cross_class(self):
        assert mp.normalize_filter("Lum") != mp.normalize_filter("Luminance")
        assert mp.normalize_filter("Red") != mp.normalize_filter("Green")


if __name__ == "__main__":
    sys.exit(pytest.main([__file__, "-v"]))
