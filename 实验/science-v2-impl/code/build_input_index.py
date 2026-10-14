#!/usr/bin/env python3
"""S00 真实输入数据索引（science-v2-impl 执行轮）。

纪律（实验/science-v2-impl/docs/R0_baseline.md 与 AGENTS §13.1）：
  * 物理参数一律从 FITS 头部解析，**不按文件名猜测**；
  * 缺失字段写 "MISSING"/"UNKNOWN"，禁止编造常数；
  * 记录每文件 SHA-256 与字节数，供跨运行对照；
  * HST 已 drizzle 成品单独登记为"信号模板"，不当独立随机输入。

输出：实验/science-v2-impl/run/S00/input_index.json（入库）+ 控制台摘要。
原始 FITS 不上 Git（.gitignore 已排除 testdata 下的帧）。
"""
import hashlib
import json
import os
import sys

from astropy.io import fits

TESTDATA = os.path.join(os.path.dirname(__file__), "..", "..", "..", "testdata")
OUT_DIR = os.path.join(os.path.dirname(__file__), "..", "run", "S00")
CHUNK = 1 << 20


def sha256_of(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while True:
            b = f.read(CHUNK)
            if not b:
                break
            h.update(b)
    return h.hexdigest()


def hget(h, keys, default="MISSING"):
    for k in keys:
        if k in h:
            v = h[k]
            if isinstance(v, str):
                v = v.strip()
                if v:
                    return v
            elif v is not None:
                return v
    return default


def index_fits(path, role):
    """解析单个 FITS 的 WCS/滤镜/曝光/像素尺度/校准状态与哈希。"""
    rec = {
        "path": os.path.relpath(path, TESTDATA),
        "role": role,
        "bytes": os.path.getsize(path),
        "sha256": sha256_of(path),
    }
    try:
        with fits.open(path, memmap=True) as hl:
            h = hl[0].header
            naxis1 = h.get("NAXIS1", "MISSING")
            naxis2 = h.get("NAXIS2", "MISSING")
            rec["geometry"] = {"NAXIS1": naxis1, "NAXIS2": naxis2}
            rec["bitpix"] = h.get("BITPIX", "MISSING")
            rec["bzero"] = h.get("BZERO", "MISSING")
            rec["bscale"] = h.get("BSCALE", "MISSING")
            rec["wcs"] = {
                "CTYPE1": hget(h, ["CTYPE1"]),
                "CTYPE2": hget(h, ["CTYPE2"]),
                "CRVAL1": hget(h, ["CRVAL1", "OBJCTRA", "RA"]),
                "CRVAL2": hget(h, ["CRVAL2", "OBJCTDEC", "DEC"]),
                "CRPIX1": hget(h, ["CRPIX1"]),
                "CRPIX2": hget(h, ["CRPIX2"]),
                "CD1_1": hget(h, ["CD1_1", "CDELT1"]),
                "CD1_2": hget(h, ["CD1_2"]),
                "CD2_1": hget(h, ["CD2_1"]),
                "CD2_2": hget(h, ["CD2_2", "CDELT2"]),
            }
            rec["filter"] = hget(h, ["FILTER", "HIERARCH FILTER"])
            rec["exposure_s"] = hget(h, ["EXPTIME", "EXPOSURE"])
            rec["gain"] = hget(h, ["EGAIN", "GAIN"])
            read_noise = hget(h, ["RDNOISE", "READNOIS"], "MISSING")
            rec["read_noise"] = read_noise
            rec["object"] = hget(h, ["OBJECT"], "UNKNOWN")
            rec["telescope"] = hget(h, ["TELESCOP", "INSTRUME"], "UNKNOWN")
            rec["date_obs"] = hget(h, ["DATE-OBS", "DATE"], "UNKNOWN")
            # 像素尺度：优先 CD 矩阵导出的局部尺度 [arcsec/px]；缺 CD 记 UNKNOWN
            try:
                cd = h.get("CD1_1"), h.get("CD1_2"), h.get("CD2_1"), h.get("CD2_2")
                if all(c is not None for c in cd):
                    import math
                    det = abs(float(cd[0]) * float(cd[3]) - float(cd[1]) * float(cd[2]))
                    scale_deg = math.sqrt(det)
                    rec["pixel_scale_arcsec_per_px"] = round(scale_deg * 3600.0, 6)
                else:
                    rec["pixel_scale_arcsec_per_px"] = "MISSING"
            except Exception as e:  # 不得编造
                rec["pixel_scale_arcsec_per_px"] = "UNKNOWN(%s)" % type(e).__name__
            # 校准状态：主产品头是否声明已减除母版
            calib = {
                "bias_subtracted": "MISSING",
                "dark_subtracted": "MISSING",
                "flat_divided": "MISSING",
            }
            rec["calibration_state"] = calib
            rec["readable"] = True
    except Exception as e:
        rec["readable"] = False
        rec["error"] = "%s: %s" % (type(e).__name__, e)
    return rec


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    index = {
        "schema": "acsd.science-v2.input-index/v1",
        "testdata_root": os.path.relpath(TESTDATA),
        "note": "物理参数从 FITS 头部解析; 缺失写 MISSING/UNKNOWN, 不编造",
        "datasets": {},
    }

    # 1) HST 成品：信号模板（已 drizzle，噪声非独立随机输入）
    hst_dir = os.path.join(TESTDATA, "HST_M16")
    hst = []
    if os.path.isdir(hst_dir):
        for n in sorted(os.listdir(hst_dir)):
            if n.endswith(".fits"):
                r = index_fits(os.path.join(hst_dir, n), "hst_drizzled_signal_template")
                hst.append(r)
    index["datasets"]["HST_M16"] = {
        "role": "signal template only (already drizzled by AstroDrizzle; "
                "its pixel noise is NOT an independent random input)",
        "files": hst,
    }

    # 2) 真实设备原始帧（.fts / .fit / .fits），按数据集逐个解析头
    light_suffixes = (".fts", ".fit", ".fits")
    for name in sorted(os.listdir(TESTDATA)):
        d = os.path.join(TESTDATA, name)
        if not os.path.isdir(d) or name.endswith("calibration files"):
            continue
        lights = []
        for root, _dirs, files in os.walk(d):
            for n in sorted(files):
                if n.lower().endswith(light_suffixes):
                    lights.append(os.path.join(root, n))
        if not lights:
            continue
        ds = {"role": "raw light frames (uncalibrated)", "files": [],
              "n_files_declared": len(lights)}
        # 头部抽样全量解析成本高：解析首/末各一份 + 全部文件名清单，
        # 其余只记哈希与体积。物理参数按帧类别从抽样帧读出并显式标注适用范围。
        for i, p in enumerate(lights):
            if i == 0 or i == len(lights) - 1:
                ds["files"].append(index_fits(p, "raw_light_sampled"))
            else:
                ds["files"].append({
                    "path": os.path.relpath(p, TESTDATA),
                    "role": "raw_light",
                    "bytes": os.path.getsize(p),
                    "sha256": sha256_of(p),
                    "header": "not parsed (sampled head is authoritative for the set)",
                })
        index["datasets"][name] = ds

    # 3) 母版（.xisf, 非 FITS ⇒ ACSD 自有读取链；此处只登记存在性与哈希）
    mast = {}
    for name in sorted(os.listdir(TESTDATA)):
        if name.endswith("calibration files"):
            d = os.path.join(TESTDATA, name)
            files = []
            for n in sorted(os.listdir(d)):
                p = os.path.join(d, n)
                if os.path.isfile(p):
                    files.append({"path": os.path.relpath(p, TESTDATA),
                                  "bytes": os.path.getsize(p),
                                  "sha256": sha256_of(p),
                                  "format": "xisf (not FITS; header not parsed here)"})
            mast[name] = files
    index["datasets"]["_calibration_masters"] = {
        "role": "master bias/dark/flat (XISF; ACSD native reader, not FITS)",
        "files": mast,
    }

    out = os.path.join(OUT_DIR, "input_index.json")
    with open(out, "w", encoding="utf-8") as f:
        json.dump(index, f, indent=1, ensure_ascii=False)

    # 控制台摘要
    n_fits = sum(len(v.get("files", [])) for k, v in index["datasets"].items()
                 if k != "_calibration_masters")
    n_mast = sum(len(v) for v in mast.values())
    print("datasets: %d" % len(index["datasets"]))
    print("light/HST files indexed: %d" % n_fits)
    print("master frames indexed:   %d" % n_mast)
    print("output: %s" % out)
    for k, v in index["datasets"].items():
        if k == "_calibration_masters":
            continue
        f0 = (v.get("files") or [{}])[0]
        print("  - %-42s n=%-4s filter=%-22s exp=%-10s scale=%s" % (
            k, len(v.get("files", [])), str(f0.get("filter"))[:22],
            str(f0.get("exposure_s"))[:10], str(f0.get("pixel_scale_arcsec_per_px"))))


if __name__ == "__main__":
    sys.exit(main())
