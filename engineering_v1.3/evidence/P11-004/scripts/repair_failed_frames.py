#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""P11-004 v3.5 最小修复脚本: 重新求解 6 个失败帧并写入新 FITS header

根因 (AUTONOMOUS_ENTRY.md §2 触发条件):
  6/16 代表帧 FITS header 为旧版本写入:
    - CRPIX=(2048.0, 2048.0) (0-based, 不符合 FITS 1-based 约定)
    - 缺失 SIP A/B/AP/BP 关键字
  当前代码 (ipv_wcs.cpp:287-288,348) 已正确实现 CRPIX+0.5 和 SIP 输出,
  属于"权威星对闭环失败且一致的尺度/位置误差"分支,
 修复方式 = 用当前代码重新生成 FITS header (不修改 CD/SIP/CRPIX 计算逻辑).

流程:
  1. 备份目录已就位 (engineering_v1.3/evidence/P11-004/backups/)
  2. 对 6 帧调用 solve_and_write_wcs(overwrite=True)
  3. 写入新 FITS header (含 SIP, CRPIX 1-based)
  4. 输出汇总 JSON 到 reports/gate_v2_final/repair_summary.json

不修改:
  - ipv_wcs.cpp / ipv_sip.cpp / ipv_solver.cpp 等任何 C++ 代码
  - solve_and_write_wcs.py 的写入逻辑
"""
from __future__ import annotations

import json
import logging
import os
import sys
import time
from pathlib import Path

# 配置路径
PROJECT_ROOT = Path(r"f:\Astro dev\Astro CS Normalization Database")
LIB_PYTHON_DIR = PROJECT_ROOT / "lib" / "plate_solve" / "python"
SCRIPT_DIR = PROJECT_ROOT / "engineering_v1.3" / "evidence" / "P11-004" / "scripts"
REPORT_DIR = PROJECT_ROOT / "engineering_v1.3" / "evidence" / "P11-004" / "reports" / "gate_v2_final"
LOG_DIR = PROJECT_ROOT / "engineering_v1.3" / "evidence" / "P11-004" / "raw_logs"

# 加入 sys.path (P11-002/scripts 包含 wcs_closure_diagnostic 模块;
#                 photometric_calib/python 提供 gaia_spectrum_client)
_V2_SCRIPTS_DIR = SCRIPT_DIR.parent.parent / "P11-002" / "scripts"
_PHOTOMETRIC_PY_DIR = PROJECT_ROOT / "lib" / "photometric_calib" / "python"
for p in [str(LIB_PYTHON_DIR), str(_V2_SCRIPTS_DIR), str(_PHOTOMETRIC_PY_DIR)]:
    if p not in sys.path:
        sys.path.insert(0, p)

# 配置日志
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] repair: %(message)s",
    handlers=[
        logging.StreamHandler(sys.stdout),
        logging.FileHandler(str(LOG_DIR / "repair_failed_frames.log"), encoding="utf-8"),
    ],
)
logger = logging.getLogger("repair")

# 6 个失败帧参数 (来自 REPRESENTATIVE_FRAMES_ARCHIVE.json)
FAILED_FRAMES = [
    {
        "frame_id": "T2_RED_LDN43",
        "fits_path": str(PROJECT_ROOT / "testdata/LDN43_T2素材_flying_dutchman/lights/LDN43_LRGBH_flying_dutchman-20250503@032713-1200S-Red.fts"),
        "focal_length_mm": 1917.5,
        "pixel_size_um": 9.0,
    },
    {
        "frame_id": "T2_GREEN_LDN43",
        "fits_path": str(PROJECT_ROOT / "testdata/LDN43_T2素材_flying_dutchman/lights/LDN43_LRGBH_flying_dutchman-20250503@034804-1200S-Green.fts"),
        "focal_length_mm": 1917.5,
        "pixel_size_um": 9.0,
    },
    {
        "frame_id": "T2_BLUE_LDN43",
        "fits_path": str(PROJECT_ROOT / "testdata/LDN43_T2素材_flying_dutchman/lights/LDN43_LRGBH_flying_dutchman-20250503@040855-1200S-Blue.fts"),
        "focal_length_mm": 1917.7,
        "pixel_size_um": 9.0,
    },
    {
        "frame_id": "T2_HA_LDN43",
        "fits_path": str(PROJECT_ROOT / "testdata/LDN43_T2素材_flying_dutchman/lights/LDN43_LRGBH_flying_dutchman-20250503@042947-1200S-H-alpha.fts"),
        "focal_length_mm": 1917.7,
        "pixel_size_um": 9.0,
    },
    {
        "frame_id": "T2_OIII_NGC1727",
        "fits_path": str(PROJECT_ROOT / "testdata/NGC1727_T2_flying_dutchman/lights/NGC1727_RGBHO_T2_flying_dutchman-20251031@075259-1800S-OIII.fts"),
        "focal_length_mm": 1917.7,
        "pixel_size_um": 9.0,
    },
    {
        "frame_id": "T3_LUM_NGC55",
        "fits_path": str(PROJECT_ROOT / "testdata/NGC55_T3_flying_dutchman/lights/NGC55_T3_flying_dutchman-20250703@075400-600S-Lum.fts"),
        "focal_length_mm": 1900.0,
        "pixel_size_um": 9.0,
    },
]


def verify_backup(frame_id: str, fits_path: str) -> bool:
    """验证备份存在"""
    backup_path = PROJECT_ROOT / "engineering_v1.3" / "evidence" / "P11-004" / "backups" / f"{frame_id}.fts"
    if not backup_path.exists():
        logger.error("备份缺失: %s", backup_path)
        return False
    src_size = os.path.getsize(fits_path)
    bak_size = os.path.getsize(backup_path)
    if src_size != bak_size:
        logger.error("备份大小不一致: %s (%d vs %d)", frame_id, src_size, bak_size)
        return False
    logger.info("备份验证通过: %s (%d bytes)", frame_id, bak_size)
    return True


def check_fits_header_before(fits_path: str) -> dict:
    """检查修复前 FITS header"""
    from astropy.io import fits
    with fits.open(fits_path, mode="readonly") as hdul:
        h = hdul[0].header
        has_sip = "A_ORDER" in h and "B_ORDER" in h
        sip_order = int(h.get("A_ORDER", 0)) if has_sip else 0
        return {
            "has_sip_before": has_sip,
            "sip_order_before": sip_order,
            "crpix_before": [float(h.get("CRPIX1", 0)), float(h.get("CRPIX2", 0))],
            "ctype_before": [str(h.get("CTYPE1", "")), str(h.get("CTYPE2", ""))],
        }


def check_fits_header_after(fits_path: str) -> dict:
    """检查修复后 FITS header"""
    from astropy.io import fits
    with fits.open(fits_path, mode="readonly") as hdul:
        h = hdul[0].header
        has_sip = "A_ORDER" in h and "B_ORDER" in h
        sip_order = int(h.get("A_ORDER", 0)) if has_sip else 0
        has_ap = "AP_ORDER" in h and "BP_ORDER" in h
        ap_order = int(h.get("AP_ORDER", 0)) if has_ap else 0
        return {
            "has_sip_after": has_sip,
            "sip_order_after": sip_order,
            "has_ap_after": has_ap,
            "ap_order_after": ap_order,
            "crpix_after": [float(h.get("CRPIX1", 0)), float(h.get("CRPIX2", 0))],
            "ctype_after": [str(h.get("CTYPE1", "")), str(h.get("CTYPE2", ""))],
        }


def main():
    from solve_and_write_wcs import solve_and_write_wcs
    from wcs_closure_diagnostic import init_platesolve_env, close_platesolve_env

    logger.info("=" * 70)
    logger.info("P11-004 v3.5 最小修复: 重新求解 6 帧并写入新 FITS header")
    logger.info("不修改任何 C++/Python 代码, 仅重新生成 FITS header")
    logger.info("=" * 70)

    # 验证备份
    for f in FAILED_FRAMES:
        if not verify_backup(f["frame_id"], f["fits_path"]):
            logger.error("备份验证失败, 终止")
            return 1

    # 初始化 PlateSolve 环境 (复用)
    logger.info("初始化 PlateSolve 环境...")
    env = init_platesolve_env(str(PROJECT_ROOT))
    logger.info("PlateSolve 环境就绪")

    results = []
    overall_ok = True

    try:
        for f in FAILED_FRAMES:
            frame_id = f["frame_id"]
            fits_path = f["fits_path"]
            focal = f["focal_length_mm"]
            ps = f["pixel_size_um"]

            logger.info("-" * 60)
            logger.info("处理: %s", frame_id)
            logger.info("  FITS: %s", fits_path)
            logger.info("  focal=%.2f, ps=%.2f", focal, ps)

            # 修复前 header
            try:
                before = check_fits_header_before(fits_path)
                logger.info("  修复前: %s", before)
            except Exception as e:
                logger.error("  修复前 header 检查失败: %s", e)
                before = {"error": str(e)}

            # 重新求解并写入
            t0 = time.time()
            try:
                result = solve_and_write_wcs(
                    fits_path,
                    ra0=0.0, dec0=0.0,
                    focal_length=focal, pixel_size=ps,
                    overwrite=True, env=env,
                )
                elapsed = time.time() - t0
                success = bool(result.get("success", False))
                rms_px = float(result.get("rms_px", 0.0))
                n_pairs = int(result.get("n_pairs", 0))
                logger.info("  求解: success=%s, rms_px=%.4f, n_pairs=%d, 耗时=%.2fs",
                            success, rms_px, n_pairs, elapsed)

                if not success:
                    err = result.get("error", "unknown")
                    logger.error("  求解失败: %s", err)
                    results.append({
                        "frame_id": frame_id,
                        "fits_path": fits_path,
                        "success": False,
                        "error": err,
                        "elapsed_sec": elapsed,
                        "before": before,
                        "after": None,
                    })
                    overall_ok = False
                    continue

            except Exception as e:
                elapsed = time.time() - t0
                logger.error("  solve_and_write_wcs 异常: %s", e, exc_info=True)
                results.append({
                    "frame_id": frame_id,
                    "fits_path": fits_path,
                    "success": False,
                    "error": str(e),
                    "elapsed_sec": elapsed,
                    "before": before,
                    "after": None,
                })
                overall_ok = False
                continue

            # 修复后 header
            try:
                after = check_fits_header_after(fits_path)
                logger.info("  修复后: %s", after)
            except Exception as e:
                logger.error("  修复后 header 检查失败: %s", e)
                after = {"error": str(e)}

            results.append({
                "frame_id": frame_id,
                "fits_path": fits_path,
                "success": True,
                "rms_px": rms_px,
                "rms_arcsec": float(result.get("rms_arcsec", 0.0)),
                "n_pairs": n_pairs,
                "elapsed_sec": elapsed,
                "before": before,
                "after": after,
            })

            # 检查修复是否成功 (has_sip_after=true, sip_order_after>=2)
            if not after.get("has_sip_after"):
                logger.error("  修复后仍无 SIP!")
                overall_ok = False
            elif after.get("sip_order_after", 0) < 2:
                logger.error("  修复后 SIP order<2: %d", after.get("sip_order_after"))
                overall_ok = False
            else:
                logger.info("  修复后 SIP 已写入: order=%d, ap_order=%d",
                            after.get("sip_order_after"), after.get("ap_order_after"))
    finally:
        close_platesolve_env(env)
        logger.info("PlateSolve 环境已关闭")

    # 写入汇总
    summary = {
        "tool_version": "P11-004 v3.5 (repair_failed_frames)",
        "repair_strategy": "regenerate_fits_header_no_code_change",
        "n_frames": len(FAILED_FRAMES),
        "n_success": sum(1 for r in results if r.get("success")),
        "n_failed": sum(1 for r in results if not r.get("success")),
        "overall_ok": overall_ok,
        "frames": results,
    }

    REPORT_DIR.mkdir(parents=True, exist_ok=True)
    summary_path = REPORT_DIR / "repair_summary.json"
    with open(summary_path, "w", encoding="utf-8") as f:
        json.dump(summary, f, indent=2, ensure_ascii=False)
    logger.info("汇总已写入: %s", summary_path)

    if overall_ok:
        logger.info("所有 6 帧修复成功 (SIP 已写入)")
        return 0
    else:
        logger.error("部分帧修复失败, 请检查日志")
        return 2


if __name__ == "__main__":
    sys.exit(main())
