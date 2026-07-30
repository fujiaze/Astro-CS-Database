#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""A-002: 批量读取 Light FITS 与 Master XISF Header，输出 raw_headers.json。
- Light: 每个数据集每种滤镜各取 1 帧代表，提取关键字段
- Master: 读取 T2/T3/T4 calibration files 下全部 .xisf，解析 XML FITSKeyword
XISF 格式: [0-3]='XISF' [4-7]=version '1.0\0' [8-11]=uint32 LE xml_len [12+]=XML(UTF-8)
"""
import os, json, struct, glob, re
import xml.etree.ElementTree as ET
from astropy.io import fits

ROOT = r"f:\Astro dev\Astro CS Normalization Database"
OUT_DIR = os.path.join(ROOT, "engineering_authoritative", "evidence", "A-002")

FITS_KEYS = [
    'IMAGETYP', 'INSTRUME', 'TELESCOP', 'EXPTIME', 'FILTER', 'GAIN', 'OFFSET',
    'SET-TEMP', 'XBINNING', 'YBINNING', 'XPIXSZ', 'YPIXSZ', 'NAXIS1', 'NAXIS2',
    'OBJECT', 'OBJCTRA', 'OBJCTDEC', 'SITELAT', 'SITELONG', 'DATE-OBS', 'CCD-TEMP',
    'BZERO', 'BITPIX', 'FOCALLEN', 'APTDIA', 'APTAREA', 'EGAIN', 'SATURATE',
    'CBLACK', 'XGUSSUM', 'READOUTM', 'SPEED', 'GAINMODE', 'OFFSETMODE',
]


def read_fits_header(path):
    h = fits.getheader(path)
    return {k: str(h[k]) for k in FITS_KEYS if k in h}


def read_xisf(path):
    with open(path, 'rb') as f:
        magic = f.read(4)
        if magic != b'XISF':
            return {"error": f"not XISF magic: {magic!r}"}
        version = f.read(4)
        xml_len = struct.unpack('<I', f.read(4))[0]
        f.read(4)  # reserved (XISF header = 16 bytes: magic+version+xml_len+reserved)
        raw = f.read(xml_len)
    xml_str = raw.decode('utf-8', errors='ignore')
    # 截取完整 XML（xml_len 可能含尾部二进制）
    start = xml_str.find('<?xml')
    end = xml_str.rfind('</xisf>')
    if start >= 0 and end > start:
        xml_str = xml_str[start:end + 7]
    elif start >= 0:
        xml_str = xml_str[start:]
    result = {"xml_len": xml_len, "xml_str_len": len(xml_str), "geometry": {}, "fits_keywords": {}}
    try:
        root = ET.fromstring(xml_str)
        ns = ''
        if root.tag.startswith('{'):
            ns = root.tag[1:root.tag.index('}')]
        nsmap = {'x': ns} if ns else {}
        img = root.find('x:Image', nsmap) if ns else root.find('Image')
        if img is not None:
            result["geometry"] = dict(img.attrib)
            for kw in (img.findall('x:FITSKeyword', nsmap) if ns else img.findall('FITSKeyword')):
                name = kw.get('name')
                value = kw.get('value')
                if name:
                    result["fits_keywords"][name] = value
    except Exception as e:
        result["xml_parse_error"] = str(e)
        result["xml_preview"] = xml_str[:800]
    return result


DATASETS = {
    "Galaxy_Center_T4": r"testdata\Galaxy_Center_T4\lights",
    "LDN43_T2": r"testdata\LDN43_T2素材_flying_dutchman\lights",
    "NGC1727_T2": r"testdata\NGC1727_T2_flying_dutchman\lights",
    "NGC247_T2": r"testdata\NGC247_T2_flying_dutchman\lights",
    "NGC55_T3": r"testdata\NGC55_T3_flying_dutchman\lights",
    "NGC83_T3": r"testdata\NGC83_cluster_T3_Flying_Dutchman\lights",
    "Victory_T4": r"testdata\Victory_Nebula_T4_Flying_Dutchman\lights",
}

MASTER_DIRS = {
    "T2": r"testdata\T2 calibration files",
    "T3": r"testdata\T3 calibration files",
    "T4": r"testdata\T4 calibration files",
}


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    light = {}
    for name, lights_dir in DATASETS.items():
        files = sorted(glob.glob(os.path.join(ROOT, lights_dir, "**", "*.fts"), recursive=True))
        if not files:
            light[name] = {"error": "no fts found", "dir": lights_dir}
            continue
        by_filter = {}
        for fp in files:
            bname = os.path.basename(fp)
            m = re.search(r'-(\d+S)-([A-Za-z\-]+)\.fts$', bname)
            flt = m.group(2) if m else "?"
            if flt not in by_filter:
                by_filter[flt] = fp
        sample = {}
        for flt, fp in sorted(by_filter.items()):
            try:
                sample[flt] = {"file": os.path.relpath(fp, ROOT), "header": read_fits_header(fp)}
            except Exception as e:
                sample[flt] = {"file": os.path.relpath(fp, ROOT), "error": str(e)}
        light[name] = {"n_lights": len(files), "sample_by_filter": sample}

    masters = {}
    for dev, mdir in MASTER_DIRS.items():
        files = sorted(glob.glob(os.path.join(ROOT, mdir, "*.xisf")))
        for fp in files:
            bname = os.path.basename(fp)
            key = f"{dev}/{bname}"
            try:
                masters[key] = read_xisf(fp)
            except Exception as e:
                masters[key] = {"error": str(e)}

    out = {"light_representative_headers": light, "master_xisf_headers": masters}
    out_path = os.path.join(OUT_DIR, "raw_headers.json")
    with open(out_path, 'w', encoding='utf-8') as f:
        json.dump(out, f, indent=2, ensure_ascii=False, default=str)
    print(f"written: {out_path}")
    print(f"light datasets: {len(light)}, master files: {len(masters)}")
    # 摘要
    for name, info in light.items():
        if "n_lights" in info:
            flts = list(info.get("sample_by_filter", {}).keys())
            print(f"  {name}: {info['n_lights']} lights, filters={flts}")
    for key, info in masters.items():
        fk = info.get("fits_keywords", {})
        geo = info.get("geometry", {})
        print(f"  {key}: geo={geo.get('geometry','?')} instr={fk.get('INSTRUME','?')} expt={fk.get('EXPTIME','?')} filt={fk.get('FILTER','?')}")


if __name__ == "__main__":
    main()
