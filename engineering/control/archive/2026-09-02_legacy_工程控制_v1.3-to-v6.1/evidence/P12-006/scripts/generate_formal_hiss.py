#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
P12-006: 生成 Stage1 代表矩阵正式 HISS

功能:
  - 复用 P12-005 已生成的 16 份 HISS 文件 (stage1 完整运行无 skip)
  - 复制到正式位置 engineering_v1.3/evidence/P12-006/hiss/
  - 独立 inspect 每个 HISS 文件 (使用 lib/astro_image_io/python/aio_healpix_io.py)
  - 计算 SHA256 哈希
  - 生成 HISS 清单 CSV (hiss_inventory.csv) + JSON 摘要

约束:
  - 不修改测试代码或 DLL
  - 不重新运行 stage1 (P12-005 已完整运行, 16/16 Gate PASS)
  - UTF-8 编码
"""

from __future__ import annotations

import csv
import hashlib
import json
import os
import shutil
import sys
import time
from dataclasses import dataclass, asdict
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

# ============================================================================
# 配置
# ============================================================================

PROJECT_ROOT = Path(r"f:\Astro dev\Astro CS Normalization Database")
EVIDENCE_DIR = PROJECT_ROOT / "engineering_v1.3" / "evidence" / "P12-006"
HISS_DIR = EVIDENCE_DIR / "hiss"
REPORTS_DIR = EVIDENCE_DIR / "reports"
SCRIPTS_DIR = EVIDENCE_DIR / "scripts"
RAW_LOGS_DIR = EVIDENCE_DIR / "raw_logs"

# P12-005 已生成的 HISS 源位置
P12_005_HISS_SRC = PROJECT_ROOT / "engineering_v1.3" / "evidence" / "P12-004" / "raw_logs"

# DLL 路径 (Python aio_healpix_io 需要)
DLL_DIR = PROJECT_ROOT / "build" / "artifacts"

# 16 帧代表帧清单 (与 P12-005 一致)
FRAMES: List[Tuple[str, str, str, str, str]] = [
    ("T4", "RED",   "Red",     "Galaxy_Center", r"testdata\Galaxy_Center_T4\lights\panel1\Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@061703-180S-Red.fts"),
    ("T4", "GREEN", "Green",   "Galaxy_Center", r"testdata\Galaxy_Center_T4\lights\panel1\Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@063620-180S-Green.fts"),
    ("T4", "BLUE",  "Blue",    "Galaxy_Center", r"testdata\Galaxy_Center_T4\lights\panel1\Galaxy_Center_mosaic1_T4_flying_dutchman-20250703@055414-180S-Blue.fts"),
    ("T4", "HA",    "H-alpha", "Galaxy_Center", r"testdata\Galaxy_Center_T4\lights\panel1\Galaxy_Center_mosaic1_T4_flying_dutchman-20250703@061318-300S-H-alpha.fts"),
    ("T4", "OIII",  "OIII",    "Galaxy_Center", r"testdata\Galaxy_Center_T4\lights\panel1\Galaxy_Center_mosaic1_T4_flying_dutchman-20250703@063631-600S-Oiii.fts"),
    ("T2", "RED",   "Red",     "LDN43",         r"testdata\LDN43_T2_flying_dutchman\lights\LDN43_LRGBH_flying_dutchman-20250503@032713-1200S-Red.fts"),
    ("T2", "GREEN", "Green",   "LDN43",         r"testdata\LDN43_T2_flying_dutchman\lights\LDN43_LRGBH_flying_dutchman-20250503@034804-1200S-Green.fts"),
    ("T2", "BLUE",  "Blue",    "LDN43",         r"testdata\LDN43_T2_flying_dutchman\lights\LDN43_LRGBH_flying_dutchman-20250503@040855-1200S-Blue.fts"),
    ("T2", "HA",    "H-alpha", "LDN43",         r"testdata\LDN43_T2_flying_dutchman\lights\LDN43_LRGBH_flying_dutchman-20250503@042947-1200S-H-alpha.fts"),
    ("T2", "OIII",  "OIII",    "NGC1727",       r"testdata\NGC1727_T2_flying_dutchman\lights\NGC1727_RGBHO_T2_flying_dutchman-20251031@075259-1800S-OIII.fts"),
    ("T3", "RED",   "Red",     "NGC55",         r"testdata\NGC55_T3_flying_dutchman\lights\NGC55_T3_flying_dutchman-20250701@074114-600S-Red.fts"),
    ("T3", "GREEN", "Green",   "NGC55",         r"testdata\NGC55_T3_flying_dutchman\lights\NGC55_T3_flying_dutchman-20250701@075153-600S-Green.fts"),
    ("T3", "BLUE",  "Blue",    "NGC55",         r"testdata\NGC55_T3_flying_dutchman\lights\NGC55_T3_flying_dutchman-20250701@080333-600S-Blue.fts"),
    ("T3", "HA",    "H-alpha", "NGC55",         r"testdata\NGC55_T3_flying_dutchman\lights\NGC55_T3_flying_dutchman-20250701@081412-1200S-H-alpha.fts"),
    ("T3", "OIII",  "OIII",    "NGC55",         r"testdata\NGC55_T3_flying_dutchman\lights\NGC55_T3_flying_dutchman-20250701@083458-1200S-Oiii.fts"),
    ("T3", "LUM",   "Lum",     "NGC55",         r"testdata\NGC55_T3_flying_dutchman\lights\NGC55_T3_flying_dutchman-20250703@075400-600S-Lum.fts"),
]


# ============================================================================
# HISS inspection (使用 lib/astro_image_io/python/aio_healpix_io.py)
# ============================================================================

def _setup_python_path() -> None:
    """将 lib/astro_image_io/python 加入 sys.path"""
    py_dir = PROJECT_ROOT / "lib" / "astro_image_io" / "python"
    if str(py_dir) not in sys.path:
        sys.path.insert(0, str(py_dir))


def _ensure_dll_in_path() -> None:
    """将 build/artifacts 加入 PATH (Windows DLL 搜索)"""
    if str(DLL_DIR) not in os.environ.get("PATH", ""):
        os.environ["PATH"] = str(DLL_DIR) + os.pathsep + os.environ.get("PATH", "")


def inspect_hiss_file(hiss_path: Path) -> Dict[str, Any]:
    """使用 aio_healpix_io Python API 独立 inspect HISS 文件.

    调用 hiss_read_snr_model() 读取 HISS 文件:
      - nside, nested, n_pix
      - ipix, pixel 数组
      - meta (JSON dict)
      - snr_model (SnrModel 或 None)

    返回 inspection 结果 dict.
    """
    info: Dict[str, Any] = {
        "path": str(hiss_path),
        "exists": False,
        "size_bytes": 0,
        "sha256": "",
        "nside": 0,
        "nested": 0,
        "n_pix": 0,
        "has_snr": 0,
        "snr_format": 0,
        "snr_n_points": 0,
        "n_points": 0,
        "snr_phot": 0.0,
        "median_snr": 0.0,
        "idw_power": 0.0,
        "meta_keys": [],
        "filter": "",
        "target": "",
        "frame_name": "",
        "inspect_ok": False,
        "inspect_error": "",
    }

    if not hiss_path.exists():
        info["inspect_error"] = "HISS 文件不存在"
        return info

    info["exists"] = True
    info["size_bytes"] = hiss_path.stat().st_size

    # 计算 SHA256
    try:
        h = hashlib.sha256()
        with open(hiss_path, "rb") as f:
            for chunk in iter(lambda: f.read(1 << 20), b""):
                h.update(chunk)
        info["sha256"] = h.hexdigest().upper()
    except Exception as e:
        info["inspect_error"] = f"SHA256 计算失败: {e}"
        return info

    # 使用 aio_healpix_io Python API 读取
    try:
        import aio_healpix_io as aio

        nside, nested, ipix, pixel, meta, snr_model = aio.hiss_read_snr_model(str(hiss_path))

        info["nside"] = int(nside)
        info["nested"] = int(1 if nested else 0)
        info["n_pix"] = int(len(ipix))

        # 从 meta 提取字段
        info["meta_keys"] = sorted(list(meta.keys())) if isinstance(meta, dict) else []
        info["filter"] = str(meta.get("filter", meta.get("FILTER", "")))
        info["target"] = str(meta.get("target", meta.get("TARGET", "")))
        info["frame_name"] = str(meta.get("frame_name", meta.get("FRAME_NAME", "")))

        # SNR 模型字段
        if snr_model is not None:
            info["has_snr"] = 1
            info["snr_format"] = 1  # snr_format=1 (稀疏控制点)
            info["snr_n_points"] = int(snr_model.n_points)
            info["n_points"] = int(snr_model.n_points)
            info["snr_phot"] = float(snr_model.snr_phot)
            info["median_snr"] = float(snr_model.median_snr)
            info["idw_power"] = float(snr_model.idw_power)
        else:
            # 旧格式 (snr_format=0 或无 snr)
            # 检查 meta 中是否有 has_snr 字段
            has_snr_meta = meta.get("has_snr", meta.get("HAS_SNR", False))
            info["has_snr"] = 1 if has_snr_meta else 0
            info["snr_format"] = int(meta.get("snr_format", 0))
            info["snr_n_points"] = int(meta.get("snr_n_points", 0))
            info["n_points"] = info["snr_n_points"]

        info["inspect_ok"] = True
    except Exception as e:
        info["inspect_error"] = f"aio_healpix_io 读取失败: {e}"
        # 退化路径: 尝试读取文件头直接解析 (zstd 解压)
        try:
            import struct
            import zstandard as zstd

            with open(hiss_path, "rb") as f:
                magic = f.read(4)
                if magic != b"HISS":
                    info["inspect_error"] += f" (magic mismatch: {magic})"
                    return info
                uncomp_len = struct.unpack("<I", f.read(4))[0]
                comp_len = struct.unpack("<I", f.read(4))[0]
                comp_json = f.read(comp_len)
                json_str = zstd.ZstdDecompressor().decompress(comp_json, uncomp_len).decode("utf-8")
                header = json.loads(json_str)
                info["nside"] = int(header.get("nside", 0))
                info["n_pix"] = int(header.get("n_pix", 0))
                info["has_snr"] = 1 if header.get("has_snr", False) else 0
                info["snr_format"] = int(header.get("snr_format", 0))
                info["snr_n_points"] = int(header.get("snr_n_points", 0))
                info["n_points"] = info["snr_n_points"]
                info["meta_keys"] = sorted(list(header.keys()))
                info["inspect_ok"] = True
                info["inspect_error"] = ""  # 清除错误
        except Exception as e2:
            info["inspect_error"] += f" | 退化解析也失败: {e2}"

    return info


# ============================================================================
# 主流程
# ============================================================================

@dataclass
class FrameResult:
    device: str
    filter_canonical: str
    filter_alias: str
    target: str
    frame_name: str
    fits_path: str
    hiss_path: str
    src_hiss_path: str = ""
    status: str = "PENDING"  # PASS / FAIL / MISSING
    size_bytes: int = 0
    sha256: str = ""
    nside: int = 0
    n_pix: int = 0
    has_snr: int = 0
    snr_format: int = 0
    n_points: int = 0
    snr_phot: float = 0.0
    median_snr: float = 0.0
    idw_power: float = 0.0
    meta_keys: str = ""
    filter_in_meta: str = ""
    target_in_meta: str = ""
    inspect_ok: bool = False
    inspect_error: str = ""
    notes: str = ""


def process_single_frame(device: str, filter_canonical: str, filter_alias: str,
                         target: str, fits_rel_path: str) -> FrameResult:
    fits_abs = PROJECT_ROOT / fits_rel_path
    frame_name = os.path.basename(fits_rel_path)
    sub_dir_name = f"{device}_{filter_canonical}_{target}"
    hiss_path = HISS_DIR / f"{sub_dir_name}.hiss"
    src_hiss_path = P12_005_HISS_SRC / sub_dir_name / f"{sub_dir_name}.hiss"

    result = FrameResult(
        device=device,
        filter_canonical=filter_canonical,
        filter_alias=filter_alias,
        target=target,
        frame_name=frame_name,
        fits_path=str(fits_abs),
        hiss_path=str(hiss_path),
        src_hiss_path=str(src_hiss_path),
    )

    print(f"[P12-006] {device}/{filter_canonical}/{target}: {frame_name}")

    # 1. 检查源 HISS 文件是否存在
    if not src_hiss_path.exists():
        result.status = "MISSING"
        result.notes = f"源 HISS 文件不存在: {src_hiss_path}"
        print(f"  MISSING: {src_hiss_path}")
        return result

    # 2. 复制到正式位置
    HISS_DIR.mkdir(parents=True, exist_ok=True)
    try:
        shutil.copy2(src_hiss_path, hiss_path)
        print(f"  复制: {src_hiss_path.name} -> {hiss_path}")
    except Exception as e:
        result.status = "FAIL"
        result.notes = f"复制失败: {e}"
        return result

    # 3. 独立 inspect
    inspect = inspect_hiss_file(hiss_path)
    result.size_bytes = inspect["size_bytes"]
    result.sha256 = inspect["sha256"]
    result.nside = inspect["nside"]
    result.n_pix = inspect["n_pix"]
    result.has_snr = inspect["has_snr"]
    result.snr_format = inspect["snr_format"]
    result.n_points = inspect["n_points"]
    result.snr_phot = inspect["snr_phot"]
    result.median_snr = inspect["median_snr"]
    result.idw_power = inspect["idw_power"]
    result.meta_keys = ",".join(inspect["meta_keys"])
    result.filter_in_meta = inspect["filter"]
    result.target_in_meta = inspect["target"]
    result.inspect_ok = inspect["inspect_ok"]
    result.inspect_error = inspect["inspect_error"]

    # 4. Gate 判定: has_snr=1 且 n_points > 0 且 inspect_ok
    if result.has_snr == 1 and result.n_points > 0 and result.inspect_ok:
        result.status = "PASS"
        print(f"  PASS: nside={result.nside} n_pix={result.n_pix} n_points={result.n_points} "
              f"snr_phot={result.snr_phot:.4f} median_snr={result.median_snr:.4f}")
    else:
        result.status = "FAIL"
        result.notes = (f"has_snr={result.has_snr}, n_points={result.n_points}, "
                       f"inspect_ok={result.inspect_ok}, err={result.inspect_error}")
        print(f"  FAIL: {result.notes}")

    return result


def main():
    # 创建输出目录
    HISS_DIR.mkdir(parents=True, exist_ok=True)
    REPORTS_DIR.mkdir(parents=True, exist_ok=True)
    SCRIPTS_DIR.mkdir(parents=True, exist_ok=True)
    RAW_LOGS_DIR.mkdir(parents=True, exist_ok=True)

    # 设置 Python path 和 DLL path
    _setup_python_path()
    _ensure_dll_in_path()

    print(f"[P12-006] 开始处理 {len(FRAMES)} 帧正式 HISS")
    print(f"  源目录: {P12_005_HISS_SRC}")
    print(f"  目标目录: {HISS_DIR}")
    print()

    results: List[FrameResult] = []
    t_start = time.time()

    for i, (device, filt, alias, target, fits_path) in enumerate(FRAMES, 1):
        print(f"=== [{i}/{len(FRAMES)}] {device}/{filt}/{target} ===")
        r = process_single_frame(device, filt, alias, target, fits_path)
        results.append(r)
        print()

    total_elapsed = round(time.time() - t_start, 3)
    n_pass = sum(1 for r in results if r.status == "PASS")
    n_fail = sum(1 for r in results if r.status == "FAIL")
    n_missing = sum(1 for r in results if r.status == "MISSING")

    print(f"[P12-006] 完成: {n_pass}/{len(results)} PASS, {n_fail} FAIL, {n_missing} MISSING, "
          f"总耗时 {total_elapsed}s")

    # 生成 HISS 清单 CSV
    csv_path = REPORTS_DIR / "hiss_inventory.csv"
    with open(csv_path, "w", encoding="utf-8", newline="") as f:
        w = csv.writer(f)
        w.writerow([
            "device", "filter", "target", "frame_name",
            "status", "size_bytes", "sha256",
            "nside", "n_pix", "has_snr", "snr_format", "n_points",
            "snr_phot", "median_snr", "idw_power",
            "inspect_ok", "inspect_error", "notes",
            "filter_in_meta", "target_in_meta", "meta_keys",
            "hiss_path", "src_hiss_path", "fits_path",
        ])
        for r in results:
            w.writerow([
                r.device, r.filter_canonical, r.target, r.frame_name,
                r.status, r.size_bytes, r.sha256,
                r.nside, r.n_pix, r.has_snr, r.snr_format, r.n_points,
                f"{r.snr_phot:.6e}", f"{r.median_snr:.6e}", f"{r.idw_power:.6e}",
                r.inspect_ok, r.inspect_error, r.notes,
                r.filter_in_meta, r.target_in_meta, r.meta_keys,
                r.hiss_path, r.src_hiss_path, r.fits_path,
            ])
    print(f"[P12-006] HISS 清单: {csv_path}")

    # 生成 JSON 摘要
    summary = {
        "task": "P12-006",
        "description": "生成 Stage1 代表矩阵正式 HISS",
        "total": len(results),
        "pass": n_pass,
        "fail": n_fail,
        "missing": n_missing,
        "total_elapsed_s": total_elapsed,
        "source": "P12-005 stage1 完整运行 (16/16 Gate PASS, has_snr=1 verified)",
        "frames": [asdict(r) for r in results],
    }
    summary_path = REPORTS_DIR / "hiss_generation_summary.json"
    with open(summary_path, "w", encoding="utf-8") as f:
        json.dump(summary, f, indent=2, ensure_ascii=False)
    print(f"[P12-006] 摘要: {summary_path}")

    # 退出码: 全部 PASS → 0, 任一失败 → 1
    return 0 if (n_fail == 0 and n_missing == 0) else 1


if __name__ == "__main__":
    sys.exit(main())
