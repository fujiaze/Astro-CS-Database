#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""从 raw_headers.json 提取设备参数摘要，打印到 stdout 供核对。"""
import json, os

ROOT = r"f:\Astro dev\Astro CS Normalization Database"
p = os.path.join(ROOT, "engineering_authoritative", "evidence", "A-002", "raw_headers.json")
d = json.load(open(p, encoding='utf-8'))

def g(h, k):
    return h.get(k, '-')

print("=== LIGHT 代表帧设备参数 (每数据集第一帧) ===")
for name, info in d['light_representative_headers'].items():
    if 'sample_by_filter' not in info:
        print(name, "ERROR:", info.get('error'))
        continue
    for flt, s in info['sample_by_filter'].items():
        h = s.get('header', {})
        line = "{}/{:8s}: instr={} pix={}um bin={} expt={}s filt={} ccdtemp={} settemp={} focal={} apd={} size={}x{} gain={} off={}".format(
            name, flt, g(h,'INSTRUME'), g(h,'XPIXSZ'), g(h,'XBINNING'), g(h,'EXPTIME'),
            g(h,'FILTER'), g(h,'CCD-TEMP'), g(h,'SET-TEMP'), g(h,'FOCALLEN'), g(h,'APTDIA'),
            g(h,'NAXIS1'), g(h,'NAXIS2'), g(h,'GAIN'), g(h,'OFFSET'))
        print(line)
        break

print()
print("=== 各数据集所有滤镜代表帧 (确认像元/温度/尺寸一致性) ===")
for name, info in d['light_representative_headers'].items():
    if 'sample_by_filter' not in info:
        continue
    for flt, s in info['sample_by_filter'].items():
        h = s.get('header', {})
        print("{}: {} | pix={} size={}x{} expt={} filt={} ccdtemp={}".format(
            name, flt, g(h,'XPIXSZ'), g(h,'NAXIS1'), g(h,'NAXIS2'),
            g(h,'EXPTIME'), g(h,'FILTER'), g(h,'CCD-TEMP')))

print()
print("=== MASTER XISF 详细 FITSKeyword ===")
for key, m in d['master_xisf_headers'].items():
    fk = m.get('fits_keywords', {})
    geo = m.get('geometry', {})
    print("{}: geo={} sampleFmt={}".format(key, geo.get('geometry','-'), geo.get('sampleFormat','-')))
    print("  instr={} pix={} bin={} focal={} apd={} aparea={} settemp={} expt={} filt={} imagetyp={}".format(
        fk.get('INSTRUME','-'), fk.get('XPIXSZ','-'), fk.get('XBINNING','-'),
        fk.get('FOCALLEN','-'), fk.get('APTDIA','-'), fk.get('APTAREA','-'),
        fk.get('SET-TEMP','-'), fk.get('EXPTIME','-'), fk.get('FILTER','-'),
        fk.get('IMAGETYP','-')))
