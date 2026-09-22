#!/usr/bin/env python3
"""E2E-501 配置生成：从 testdata 真实帧构造三命令配置（不猜字段，按 --help 的合同字段）。

产出（落 run/RELEASE-05/e2e/，不入库）：
  configs/p1_m42_t2_red.json    normalize：M42 T2 Red 300s 子集 + T2 母版
  configs/p1_m42_t3_red.json    normalize：M42 T3 Red 300s 子集 + T3 母版
  configs/mosaic_e2e.json       mosaic：hips_paths = 上面两块的产品目录
  configs/export_e2e.json       export：source.hips_dir = mosaic 输出
"""
import glob
import io
import json
import os
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
E2E = os.path.join(REPO, "run/RELEASE-05/e2e")
N_FRAMES = int(os.environ.get("E2E_NFRAMES", "3"))


def pick_lights(pattern, n):
    fs = sorted(glob.glob(pattern))
    return fs[:n]


def main():
    os.makedirs(os.path.join(E2E, "configs"), exist_ok=True)
    out_root = os.path.join(E2E, "out")
    os.makedirs(out_root, exist_ok=True)

    blocks = []
    # 数据集清单：M42（T2/T3，300s Red，600s dark ⇒ K=0.5）与银心（T4，180s Red，180s dark ⇒ K=1）
    datasets = [
        ("T2", "testdata/M42_T2T3_mosaic_Flying_dutchman/T2", "*-300S-Red.fts",
         "masterDark_BIN-1_4096x4096_EXPOSURE-600.00s.xisf",
         "masterFlat_BIN-1_4096x4096_FILTER-Red_mono.xisf",
         "masterBias_BIN-1_4096x4096.xisf", 0.5),
        ("T3", "testdata/M42_T2T3_mosaic_Flying_dutchman/T3", "*-300S-Red.fts",
         "masterDark_BIN-1_4096x4096_EXPOSURE-600.00s.xisf",
         "masterFlat_BIN-1_4096x4096_FILTER-Red_mono.xisf",
         "masterBias_BIN-1_4096x4096.xisf", 0.5),
        ("T4", "testdata/Galaxy_Center_T4/lights/panel1", "*-180S-Red.fts",
         "masterDark_BIN-1_4500x3600_EXPOSURE-180.00s.xisf",
         "masterFlat_BIN-1_4500x3600_FILTER-Red_mono.xisf",
         "masterBias_BIN-1_4500x3600.xisf", 1.0),
    ]
    only = os.environ.get("E2E_ONLY", "").strip()
    for tel, lightdir, globpat, dark, flat, bias, k in datasets:
        if only and tel != only:
            continue
        pat = os.path.join(REPO, lightdir, globpat)
        lights = pick_lights(pat, N_FRAMES)
        if not lights:
            print("MISSING lights for %s: %s" % (tel, pat), file=sys.stderr)
            return 2
        cal = os.path.join(REPO, "testdata/%s calibration files" % tel)
        cfg = {
            "schema_version": "1",
            "blocks": [{
                "name": "m42_%s_red" % tel.lower(),
                "input_lights": lights,
                "master_bias": os.path.join(cal, bias),
                "master_dark": os.path.join(cal, dark),
                "master_flat": os.path.join(cal, flat),
                "output_dir": os.path.join(out_root, "p1_%s" % tel.lower()),
                "drizzle": {"nested": 1, "pixfrac": 1.0, "precision_mode": 1},
                "filter_passband": "Baader R",
                # 真实 IPV 解算链（无静默缺省）：指向仓库只读星表数据集
                "wcs": {"init_source": "header_pointing",
                        "gaia_data_dir": os.path.join(REPO, "gaia/GaiaDR3")},
                # CALIBRATION.md §5 / CALIBRATION_ALGORITHMS.md §378：
                # 母版来自外部流水线（PixInsight 风格）⇒ 显式声明含 bias；
                # 亮场 300s / 暗场 600s ⇒ 显式给 K=0.5（禁 silent default）。
                "dark_optimization": True,
                "dark_scale_factor": k,
                "master_flat_normalize": "median",
                # CALIBRATION.md §5 U1/U4：testdata 母版是 PixInsight 风格归一化域（中位数 ~0.015），
                # 亮场是 16 位 ADU。必须**显式**声明域与到 ADU 的换算因子（禁静默默认）。
                "master_units": {"bias": "normalized", "dark": "normalized", "flat": "normalized"},
                "master_scale": {"bias": 65535, "dark": 65535, "flat": 65535},
            }],
        }
        p = os.path.join(E2E, "configs", "p1_m42_%s_red.json" % tel.lower())
        io.open(p, "w", encoding="utf-8").write(json.dumps(cfg, ensure_ascii=False, indent=2) + "\n")
        blocks.append(cfg["blocks"][0])
        print("wrote %s (%d lights)" % (p, len(lights)))

    # hips_paths 必须是**逐帧 HiPS 产品树**（每个树自带 manifest.json，§10 fail-closed），
    # 权威来源 = normalize 产出的 p1_products.json 的 frames[].hips_path。
    hips = []
    for b in blocks:
        pj = os.path.join(b["output_dir"], "p1_products.json")
        if not os.path.isfile(pj):
            print("NOTE: %s 尚未产出（先跑 normalize 再生成 mosaic 配置）" % pj)
            continue
        doc = json.loads(io.open(pj, encoding="utf-8").read())
        for f in doc.get("frames", []):
            if f.get("hips_path"):
                hips.append(f["hips_path"])
    mosaic_cfg = {
        "schema_version": "1",
        "hips_paths": hips or [b["output_dir"] for b in blocks],
        "output_dir": os.path.join(out_root, "p2_mosaic"),
        "snr_path": "sparse_reconstruct",
    }
    pm = os.path.join(E2E, "configs", "mosaic_e2e.json")
    io.open(pm, "w", encoding="utf-8").write(json.dumps(mosaic_cfg, ensure_ascii=False, indent=2) + "\n")
    print("wrote %s" % pm)

    export_cfg = {
        "schema_version": "1",
        "source": {"hips_dir": mosaic_cfg["output_dir"]},
        "output_dir": os.path.join(out_root, "p3_export"),
        # 导出中心必须落在 mosaic 实际覆盖内（否则 coverage_ok=1 但 covered_px=0，产品全空）。
        # 这里从解算后的逐帧 WCS 采样点算包围盒中心（E2E 实测：银心 panel1 三帧中心）。
        "center": {"ra_deg": 272.7823, "dec_deg": -13.3197},
        "width_px": 512,
        "height_px": 512,
        "scale_deg_per_px": 0.0007,
        "rotation_deg": 0.0,
        "crpix_px": [256.5, 256.5],
        "output_mode": "surface_brightness",
    }
    pe = os.path.join(E2E, "configs", "export_e2e.json")
    io.open(pe, "w", encoding="utf-8").write(json.dumps(export_cfg, ensure_ascii=False, indent=2) + "\n")
    print("wrote %s" % pe)
    return 0


if __name__ == "__main__":
    sys.exit(main())
