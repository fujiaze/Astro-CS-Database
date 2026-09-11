# -*- coding: utf-8 -*-
"""REAL-000 index.json v1.2 校验：schema 扩展、M42 实测对账、向后兼容。

测试读 testdata/index.json（唯一数据面写入）并与磁盘只读对账；
不写任何 testdata 文件。
"""
import json
import os
from collections import defaultdict

import pytest

_HERE = os.path.dirname(os.path.abspath(__file__))
_REPO = os.path.normpath(os.path.join(_HERE, "..", ".."))
INDEX_PATH = os.path.join(_REPO, "testdata", "index.json")

import importlib.util
_TOOL = os.path.join(_REPO, "tools", "realdata", "match_plan.py")
_spec = importlib.util.spec_from_file_location("match_plan", _TOOL)
mp = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(mp)


@pytest.fixture(scope="module")
def index():
    with open(INDEX_PATH, encoding="utf-8") as fh:
        return json.load(fh)


@pytest.fixture(scope="module")
def disk_counts(index):
    """磁盘实测：数据集 → (帧数, {telescope: n}, {exposure,filter: n})。

    望远镜归属：multi-telescope 数据集按磁盘目录（M42: T2/..、T3/..），
    单望远镜数据集按 index 声明（与 v1.1 统计口径一致）。
    """
    tel_decl = {}
    for d in index["datasets"]:
        for t in d.get("telescopes", [d.get("telescope")]):
            tel_decl[d["id"]] = (d.get("multi_telescope", False), t)
    out = {}
    for dirpath, _dn, filenames in os.walk(os.path.join(_REPO, "testdata")):
        for fn in sorted(filenames):
            if not fn.lower().endswith((".fts", ".fits", ".fit")):
                continue
            parsed = mp.parse_light_filename(fn)
            if parsed is None:
                continue
            rel = os.path.relpath(os.path.join(dirpath, fn), _REPO)
            ds, _td, _pd, _kind = mp.classify_entry(rel)
            multi, declared = tel_decl[ds]
            tel = mp._infer_telescope_panel(rel.split(os.sep)[2:-1],
                                            parsed["prefix"])[0] or declared
            rec = out.setdefault(ds, {"n": 0, "by_tel": defaultdict(int),
                                      "by_exp_filter": defaultdict(int)})
            rec["n"] += 1
            rec["by_tel"][tel] += 1
            rec["by_exp_filter"][(parsed["exposure_s"], parsed["filter_raw"])] += 1
    return out


# ---------------------------------------------------------------------------
# schema v1.2
# ---------------------------------------------------------------------------
class TestSchemaV12:
    def test_version_and_schema_id(self, index):
        assert index["version"] == "1.2"
        assert index["$schema"] == "AstroCS testdata inventory v1.2"

    def test_schema_changes_documented(self, index):
        ch = index["schema_changes_v1_2"]
        assert "datasets[].telescopes" in ch["added_fields"]
        assert "pixel_size_um" in json.dumps(ch["added_fields"])
        assert ch["compat"]  # 向后兼容声明存在

    def test_pixel_size_um_null_owner_confirm(self, index):
        """T2/T3/T4 像元尺寸：素材信息未记载 → null（OWNER_CONFIRM），
        禁止凭相机型号臆造。"""
        for tel in ("T2", "T3", "T4"):
            assert "pixel_size_um" in index["telescopes"][tel]
            assert index["telescopes"][tel]["pixel_size_um"] is None


# ---------------------------------------------------------------------------
# M42 数据集条目（磁盘实测对账）
# ---------------------------------------------------------------------------
M42 = "M42_T2T3_mosaic_Flying_dutchman"


class TestM42Entry:
    def test_entry_exists_multi_telescope(self, index):
        m42 = next(d for d in index["datasets"] if d["id"] == M42)
        assert m42["telescopes"] == ["T2", "T3"]
        assert m42["multi_telescope"] is True
        assert m42["telescope"] == "T2+T3"      # 兼容字段保留
        assert m42["mosaic_panels"] == [f"M{i}" for i in range(1, 7)]

    def test_registered_in_telescope_datasets(self, index):
        assert M42 in index["telescopes"]["T2"]["datasets"]
        assert M42 in index["telescopes"]["T3"]["datasets"]

    def test_filters_from_info_file(self, index):
        m42 = next(d for d in index["datasets"] if d["id"] == M42)
        by_name = {f["name"]: f for f in m42["filters"]}
        assert by_name["H-alpha"]["brand"] == "Astrodon"
        assert by_name["H-alpha"]["bandwidth_nm"] == 3
        for rgb in ("Red", "Green", "Blue"):
            assert by_name[rgb]["brand"] == "Astrodon"
            assert by_name[rgb]["bandwidth_nm"] is None  # 素材信息未记载

    def test_lights_count_matches_disk(self, index, disk_counts):
        m42 = next(d for d in index["datasets"] if d["id"] == M42)
        assert m42["lights_count"] == disk_counts[M42]["n"] == 196

    def test_per_panel_exposure_filter_matches_disk(self, index, disk_counts):
        """逐面板逐曝光逐滤镜计数 == 磁盘实测（T2 300s 53 帧 / T3 300s 94 帧）。"""
        m42 = next(d for d in index["datasets"] if d["id"] == M42)
        tree = m42["lights_by_telescope_panel_exposure_filter"]
        disk = disk_counts[M42]["by_exp_filter"]
        seen = defaultdict(int)
        for tel, panels in tree.items():
            for panel, cells in panels.items():
                for exp_key, filt_map in cells.items():
                    exp = float(exp_key.rstrip("s"))
                    for filt, n in filt_map.items():
                        seen[(exp, filt)] += n
        assert dict(seen) == {k: v for k, v in disk.items()}
        # 指令表缺口帧数：T2 300s×53、T3 300s×94
        t2_300 = sum(tree["T2"][p].get("300s", {}).get(f, 0)
                     for p in tree["T2"] for f in ("Blue", "Green", "Red", "H-alpha"))
        t3_300 = sum(tree["T3"][p].get("300s", {}).get(f, 0)
                     for p in tree["T3"] for f in ("Blue", "Green", "Red"))
        assert t2_300 == 53 and t3_300 == 94
        assert sum(tree["T2"][p].get("600s", {}).get("H-alpha", 0)
                   for p in tree["T2"]) == 20
        assert sum(tree["T3"][p].get("600s", {}).get("H-alpha", 0)
                   for p in tree["T3"]) == 29


# ---------------------------------------------------------------------------
# 向后兼容（v1.1 消费方字段全保留）
# ---------------------------------------------------------------------------
class TestBackwardCompat:
    def test_all_v11_datasets_present(self, index):
        v11_ids = {"Galaxy_Center_T4", "LDN43_T2素材_flying_dutchman",
                   "NGC1727_T2_flying_dutchman", "NGC247_T2_flying_dutchman",
                   "NGC55_T3_flying_dutchman", "NGC83_cluster_T3_Flying_Dutchman",
                   "Victory_Nebula_T4_Flying_Dutchman", M42}
        assert {d["id"] for d in index["datasets"]} == v11_ids

    def test_v11_dataset_fields_intact(self, index):
        for d in index["datasets"]:
            for key in ("telescope", "photographer", "info_file", "lights_dir",
                        "lights_count", "filters", "single_exposure_s",
                        "observation_dates"):
                assert key in d, (d["id"], key)
        gc = next(d for d in index["datasets"] if d["id"] == "Galaxy_Center_T4")
        assert gc["lights_by_panel_filter"]["panel1"]["Blue"] == 11

    def test_calibration_masters_intact(self, index):
        m = index["calibration_masters"]["by_telescope"]
        for tel, n in (("T2", 9), ("T3", 9), ("T4", 9)):
            got = sum(len(m[tel][k]) for k in ("bias", "dark", "flat"))
            assert got == n

    def test_frame_format_convention_intact(self, index):
        conv = index["frame_format_convention"]
        assert ".fts" in conv["raw_light_frames"]
        assert ".xisf" in conv["raw_master_frames"]


# ---------------------------------------------------------------------------
# statistics 与磁盘对账（8 数据集 / 906 帧 / 27 母版）
# ---------------------------------------------------------------------------
class TestStatistics:
    def test_totals(self, index):
        s = index["statistics"]
        assert s["total_datasets"] == 8
        assert s["total_raw_light_frames"] == 906
        assert s["total_calibration_masters"] == 27

    def test_by_telescope_matches_disk(self, index, disk_counts):
        by_tel = index["statistics"]["by_telescope"]
        disk_tel = defaultdict(int)
        for ds, rec in disk_counts.items():
            for tel, n in rec["by_tel"].items():
                disk_tel[tel] += n
        for tel in ("T2", "T3", "T4"):
            assert by_tel[tel]["raw_frames"] == disk_tel[tel]
        assert by_tel["T1"]["raw_frames"] == 0
        assert sum(v["raw_frames"] for v in by_tel.values()) == 906

    def test_every_dataset_lights_count_matches_disk(self, index, disk_counts):
        for d in index["datasets"]:
            assert d["lights_count"] == disk_counts[d["id"]]["n"], d["id"]


if __name__ == "__main__":
    import sys
    sys.exit(pytest.main([__file__, "-v"]))
