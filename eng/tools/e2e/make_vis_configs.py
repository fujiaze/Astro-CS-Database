#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""VIS-501 两组成品帧配置生成（只做 R 通道）。

依据：控制包 VIS-501 任务书；ASTROCS_DESIGN §6（三命令）；--help 的配置合同。
数据：testdata/Galaxy_Center_T4/lights/panel{1,2,3} Red 180s（共 32 帧）+ T4 母版（K=1）；
      testdata/M42_T2T3_mosaic_Flying_dutchman/{T2,T3} Red 300s + 对应母版（600s dark ⇒ K=0.5）。

产出：run/RELEASE-05/vis/configs/{p1_gc.json, mosaic_gc.json, export_gc.json}
                                        {p1_m42.json, mosaic_m42.json, export_m42.json}
"""
import argparse
import glob
import io
import json
import os
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
VIS = os.path.join(REPO, "run/RELEASE-05/vis")
# 星表目录：光度阶段需要含光谱的 GaiaDR3SP（DATA_SEMANTICS §8.1/§8.2：DR3 记录无光谱、
# BP/RP 恒 0 sentinel），且 orchestrator 的 PLATESOLVE 默认 db_type 即 DR3SP。
GAIA = os.path.join(REPO, "gaia/GaiaDR3SP")


def block(name, lights, cal, bias, dark, flat, out_dir, k):
    return {
        "name": name,
        "input_lights": lights,
        "master_bias": os.path.join(cal, bias),
        "master_dark": os.path.join(cal, dark),
        "master_flat": os.path.join(cal, flat),
        "output_dir": out_dir,
        "drizzle": {"nested": 1, "pixfrac": 1.0, "precision_mode": 1},
        "filter_passband": "Baader R",
        "wcs": {"init_source": "header_pointing", "gaia_data_dir": GAIA},
        # 测光标定通道：正向合成需要 (星表, 滤镜响应, QE) 三者齐备；缺席即显式降级为
        # photscale_absent（像素不缩放），测光星等坐标系便不参与产品。
        "photometry": {
            "fit": {
                "enabled": True,
                "gaia_data_dir": GAIA,
                "filter": "Baader R",
                "filters_json": os.path.join(REPO, "eng/packaging/config/filters.json"),
            }
        },
        "dark_optimization": True,
        "dark_scale_factor": k,
        "master_flat_normalize": "median",
        "master_units": {"bias": "normalized", "dark": "normalized", "flat": "normalized"},
        "master_scale": {"bias": 65535, "dark": 65535, "flat": 65535},
    }


def write(path, doc):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    io.open(path, "w", encoding="utf-8").write(json.dumps(doc, ensure_ascii=False, indent=2) + "\n")
    print("wrote %s" % path)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--group", choices=("gc", "m42", "both"), default="both")
    ap.add_argument("--limit", type=int, default=0, help="每组每 panel 限制帧数（0 = 全部）")
    args = ap.parse_args()

    if args.group in ("gc", "both"):
        cal = os.path.join(REPO, "testdata/T4 calibration files")
        blocks = []
        for panel in ("panel1", "panel2", "panel3"):
            lights = sorted(glob.glob(os.path.join(
                REPO, "testdata/Galaxy_Center_T4/lights", panel, "*-180S-Red.fts")))
            if args.limit:
                lights = lights[:args.limit]
            blocks.append(block("gc_%s_red" % panel, lights, cal,
                                "masterBias_BIN-1_4500x3600.xisf",
                                "masterDark_BIN-1_4500x3600_EXPOSURE-180.00s.xisf",
                                "masterFlat_BIN-1_4500x3600_FILTER-Red_mono.xisf",
                                os.path.join(VIS, "out/gc_p1", panel), 1.0))
        write(os.path.join(VIS, "configs/p1_gc.json"), {"schema_version": "1", "blocks": blocks})
        write(os.path.join(VIS, "configs/mosaic_gc.json"), {
            "schema_version": "1",
            "hips_paths": [b["output_dir"] for b in blocks],   # 占位；真值由 p1_products.json 回填
            "output_dir": os.path.join(VIS, "out/gc_p2"),
            "snr_path": "sparse_reconstruct"})
        write(os.path.join(VIS, "configs/export_gc.json"), {
            "schema_version": "1",
            "source": {"hips_dir": os.path.join(VIS, "out/gc_p2")},
            "output_dir": os.path.join(VIS, "out/gc_p3"),
            "center": {"ra_deg": 272.7823, "dec_deg": -13.3197},
            "width_px": 4096, "height_px": 3072,
            "scale_deg_per_px": 0.0007, "rotation_deg": 0.0,
            "crpix_px": [2048.5, 1536.5], "output_mode": "surface_brightness"})

    if args.group in ("m42", "both"):
        blocks = []
        for tel in ("T2", "T3"):
            cal = os.path.join(REPO, "testdata/%s calibration files" % tel)
            # 每 panel 分别限量：马赛克由 T{2,3}/M1..M6 六个 panel 拼成，
            # 对整块（跨 panel）截断会只留 M1 一个 panel，拼不出马赛克。
            lights = []
            for panel in sorted(glob.glob(os.path.join(
                    REPO, "testdata/M42_T2T3_mosaic_Flying_dutchman", tel, "*"))):
                got = sorted(glob.glob(os.path.join(panel, "*-300S-Red.fts")))
                lights.extend(got[:args.limit] if args.limit else got)
            blocks.append(block("m42_%s_red" % tel.lower(), lights, cal,
                                "masterBias_BIN-1_4096x4096.xisf",
                                "masterDark_BIN-1_4096x4096_EXPOSURE-600.00s.xisf",
                                "masterFlat_BIN-1_4096x4096_FILTER-Red_mono.xisf",
                                os.path.join(VIS, "out/m42_p1_%s" % tel.lower()), 0.5))
        write(os.path.join(VIS, "configs/p1_m42.json"), {"schema_version": "1", "blocks": blocks})
        write(os.path.join(VIS, "configs/mosaic_m42.json"), {
            "schema_version": "1",
            "hips_paths": [b["output_dir"] for b in blocks],
            "output_dir": os.path.join(VIS, "out/m42_p2"),
            "snr_path": "sparse_reconstruct"})
        write(os.path.join(VIS, "configs/export_m42.json"), {
            "schema_version": "1",
            "source": {"hips_dir": os.path.join(VIS, "out/m42_p2")},
            "output_dir": os.path.join(VIS, "out/m42_p3"),
            "center": {"ra_deg": 83.82, "dec_deg": -5.39},
            "width_px": 4096, "height_px": 4096,
            "scale_deg_per_px": 0.0005, "rotation_deg": 0.0,
            "crpix_px": [2048.5, 2048.5], "output_mode": "surface_brightness"})
    return 0


if __name__ == "__main__":
    sys.exit(main())
