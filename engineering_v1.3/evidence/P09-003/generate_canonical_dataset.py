#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
P09-003 T4: 生成 canonical_dataset_v1.2.json
- 计算 T1-T4 测光代表帧 SHA-256 (7 个 canonical 失败帧)
- 计算 Galaxy Center 32 Red 帧 SHA-256 (panel1=11, panel2=11, panel3=10)
- 计算当前 HCSD 代表文件 SHA-256 (P07-001 基线)
- 计算浏览器默认加载 HISS SHA-256
- 记录浏览器固定视角与 STF 配置
- 输出 canonical_dataset_v1.2.json + hash 明细
"""
from __future__ import annotations

import hashlib
import json
import os
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

REPO_ROOT = Path(r"f:\Astro dev\Astro CS Normalization Database")
EVIDENCE_DIR = REPO_ROOT / "engineering_v1.2" / "evidence" / "P09-003"
HASHES_DIR = EVIDENCE_DIR / "hashes"
RAW_LOGS_DIR = EVIDENCE_DIR / "raw_logs"


def sha256_file(path: Path) -> str:
    """计算文件 SHA-256 (大文件分块读取)。"""
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest().upper()


def file_info(path: Path) -> dict:
    """返回文件 size, sha256, mtime。"""
    if not path.exists():
        return {"path": str(path), "exists": False}
    stat = path.stat()
    return {
        "path": str(path.relative_to(REPO_ROOT)) if path.is_absolute() else str(path),
        "abs_path": str(path),
        "exists": True,
        "size_bytes": stat.st_size,
        "size_mb": round(stat.st_size / (1024 * 1024), 3),
        "sha256": sha256_file(path),
        "mtime_utc": datetime.fromtimestamp(stat.st_mtime, tz=timezone.utc).isoformat(),
    }


# ---------------------------------------------------------------------------
# 1. T1-T4 测光代表帧 (7 个 canonical 失败帧, 来自 P05-002)
# ---------------------------------------------------------------------------
CANONICAL_FRAMES = [
    {
        "frame_id": "P05-001-C001",
        "device": "T4",
        "target": "Galaxy_Center",
        "filter": "Red",
        "exposure_s": 180,
        "photometric_status": "degraded",
        "n_matched": 1,
        "scale": 0.003784,
        "sigma_residual": 0.0,
        "failure_type": "single_point_match",
        "rel_path": "testdata/Galaxy_Center_T4/lights/panel1/Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@061703-180S-Red.fts",
    },
    {
        "frame_id": "P05-001-C002",
        "device": "T2",
        "target": "LDN43",
        "filter": "Lum",
        "exposure_s": 600,
        "photometric_status": "failed",
        "n_matched": 0,
        "scale": None,
        "sigma_residual": None,
        "failure_type": "stage1_overall_fail_missing_master_flat_Lum",
        "failure_root_cause": "missing_master_flat_Lum",
        "rel_path": "testdata/LDN43_T2素材_flying_dutchman/lights/LDN43_LRGBH_flying_dutchman-20250503@031525-600S-Lum.fts",
    },
    {
        "frame_id": "P05-001-C003",
        "device": "T2",
        "target": "NGC1727",
        "filter": "Red",
        "exposure_s": 600,
        "photometric_status": "degraded",
        "n_matched": 1,
        "scale": 2.2e-05,
        "sigma_residual": 0.0,
        "failure_type": "single_point_match",
        "rel_path": "testdata/NGC1727_T2_flying_dutchman/lights/NGC1727_RGBHO_T2_flying_dutchman-20251031@064517-600S-Red.fts",
    },
    {
        "frame_id": "P05-001-C004",
        "device": "T2",
        "target": "NGC247",
        "filter": "Lum",
        "exposure_s": 600,
        "photometric_status": "degraded",
        "n_matched": 1,
        "scale": 8.2e-05,
        "sigma_residual": 0.0,
        "failure_type": "single_point_match",
        "rel_path": "testdata/NGC247_T2_flying_dutchman/lights/NGC247_T2_flying_dutchman-20250816@033428-600S-Lum.fts",
    },
    {
        "frame_id": "P05-001-C005",
        "device": "T3",
        "target": "NGC55",
        "filter": "Red",
        "exposure_s": 600,
        "photometric_status": "degraded",
        "n_matched": 1,
        "scale": 4.6e-05,
        "sigma_residual": 0.0,
        "failure_type": "single_point_match",
        "rel_path": "testdata/NGC55_T3_flying_dutchman/lights/NGC55_T3_flying_dutchman-20250701@074114-600S-Red.fts",
    },
    {
        "frame_id": "P05-001-C006",
        "device": "T3",
        "target": "NGC83_cluster",
        "filter": "Red",
        "exposure_s": 600,
        "photometric_status": "degraded",
        "n_matched": 1,
        "scale": 4.2e-05,
        "sigma_residual": 0.0,
        "failure_type": "single_point_match",
        "rel_path": "testdata/NGC83_cluster_T3_Flying_Dutchman/lights/NGC90_2025wwk_T3_flying_dutchman-20251011@020846-600S-Red.fts",
    },
    {
        "frame_id": "P05-001-C007",
        "device": "T4",
        "target": "Victory_Nebula",
        "filter": "Lum",
        "exposure_s": 180,
        "photometric_status": "failed",
        "n_matched": 0,
        "scale": 1.0,
        "sigma_residual": 0.0,
        "failure_type": "zero_match_g002_gap",
        "rel_path": "testdata/Victory_Nebula_T4_Flying_Dutchman/lights/Victory_Nebula_mosaic1_flying_dutchman-20250204@035646-180S-Lum.fts",
    },
]


# ---------------------------------------------------------------------------
# 2. Galaxy Center 32 Red 帧 (panel1=11, panel2=11, panel3=10)
# ---------------------------------------------------------------------------
GALAXY_CENTER_PANEL1_RED = [
    "Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@061703-180S-Red.fts",
    "Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@062109-180S-Red.fts",
    "Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@062457-180S-Red.fts",
    "Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@062844-180S-Red.fts",
    "Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@063231-180S-Red.fts",
    "Galaxy_Center_mosaic1_T4_flying_dutchman-20250703@051551-180S-Red.fts",
    "Galaxy_Center_mosaic1_T4_flying_dutchman-20250703@051958-180S-Red.fts",
    "Galaxy_Center_mosaic1_T4_flying_dutchman-20250703@052346-180S-Red.fts",
    "Galaxy_Center_mosaic1_T4_flying_dutchman-20250703@052735-180S-Red.fts",
    "Galaxy_Center_mosaic1_T4_flying_dutchman-20250703@053123-180S-Red.fts",
    "Galaxy_Center_mosaic1_T4_flying_dutchman-20250813@011752-180S-Red.fts",
]

GALAXY_CENTER_PANEL2_RED = [
    "Galaxy_Center_mosaic2_T4_flying_dutchman-20250716@002647-180S-Red.fts",
    "Galaxy_Center_mosaic2_T4_flying_dutchman-20250716@003055-180S-Red.fts",
    "Galaxy_Center_mosaic2_T4_flying_dutchman-20250716@003443-180S-Red.fts",
    "Galaxy_Center_mosaic2_T4_flying_dutchman-20250716@003831-180S-Red.fts",
    "Galaxy_Center_mosaic2_T4_flying_dutchman-20250716@004219-180S-Red.fts",
    "Galaxy_Center_mosaic2_T4_flying_dutchman-20250717@031324-180S-Red.fts",
    "Galaxy_Center_mosaic2_T4_flying_dutchman-20250717@031717-180S-Red.fts",
    "Galaxy_Center_mosaic2_T4_flying_dutchman-20250717@032221-180S-Red.fts",
    "Galaxy_Center_mosaic2_T4_flying_dutchman-20250717@032620-180S-Red.fts",
    "Galaxy_Center_mosaic2_T4_flying_dutchman-20250717@033543-180S-Red.fts",
    "Galaxy_Center_mosaic2_T4_flying_dutchman-20250717@033941-180S-Red.fts",
]

GALAXY_CENTER_PANEL3_RED = [
    "Galaxy_Center_mosaic3_T4_flying_dutchman-20250718@001638-180S-Red.fts",
    "Galaxy_Center_mosaic3_T4_flying_dutchman-20250718@002045-180S-Red.fts",
    "Galaxy_Center_mosaic3_T4_flying_dutchman-20250718@002432-180S-Red.fts",
    "Galaxy_Center_mosaic3_T4_flying_dutchman-20250718@002821-180S-Red.fts",
    "Galaxy_Center_mosaic3_T4_flying_dutchman-20250718@003210-180S-Red.fts",
    "Galaxy_Center_mosaic3_T4_flying_dutchman-20250718@003556-180S-Red.fts",
    "Galaxy_Center_mosaic3_T4_flying_dutchman-20250718@003945-180S-Red.fts",
    "Galaxy_Center_mosaic3_T4_flying_dutchman-20250718@004333-180S-Red.fts",
    "Galaxy_Center_mosaic3_T4_flying_dutchman-20250718@004720-180S-Red.fts",
    "Galaxy_Center_mosaic3_T4_flying_dutchman-20250718@005108-180S-Red.fts",
]


# ---------------------------------------------------------------------------
# 3. 当前 HCSD 代表文件 (P07-001 基线 + 早期基线)
# ---------------------------------------------------------------------------
CURRENT_HCSD_FILES = [
    {
        "id": "P07-001-stage2-run1",
        "role": "current_canonical_hcsd_baseline",
        "description": "P07-001 性能基线 stage2_run1, 用作 v1.2 当前 HCSD 基线 (run1/run2 字节级一致)",
        "rel_path": "engineering/evidence/P07-001/output/stage2_run1.hcsd",
    },
    {
        "id": "P07-001-stage2-run2",
        "role": "current_canonical_hcsd_baseline_replica",
        "description": "P07-001 性能基线 stage2_run2 (与 run1 字节一致, 用作可重现性证据)",
        "rel_path": "engineering/evidence/P07-001/output/stage2_run2.hcsd",
    },
    {
        "id": "P07-002-stage2-repeat-1",
        "role": "current_canonical_hcsd_stability",
        "description": "P07-002 稳定性测试 stage2_repeat_1 (3 次重跑之一)",
        "rel_path": "engineering/evidence/P07-002/output/stage2_repeat_1.hcsd",
    },
    {
        "id": "P08-002-clean-env-stage2-baseline",
        "role": "current_canonical_hcsd_clean_env",
        "description": "P08-002 干净环境测试基线",
        "rel_path": "engineering/evidence/P08-002/clean_env/testdata/stage2_baseline.hcsd",
    },
]


# ---------------------------------------------------------------------------
# 4. 浏览器默认加载 HISS + 固定视角
# ---------------------------------------------------------------------------
BROWSER_DEFAULT_HISS = {
    "rel_path": "output/pipeline_debug/Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@061703-180S-Red/drizzle/T4_2x_nside65536.hiss",
    "loaded_by": "lib/healpix_db/healpix_browser_qt/run_healpix.bat",
    "nside": 65536,
}

BROWSER_FIXED_VIEWPOINT = {
    "window_size": {"width": 1280, "height": 800},
    "default_view": {
        "ra_deg": 0.0,
        "dec_deg": 0.0,
        "fov_deg": 50.0,
        "source": "lib/healpix_db/healpix_browser_qt/widgets/sphere_view.cpp reset_view() L54-63",
    },
    "auto_stretch_params": {
        "shadows_percentile": 0.5,
        "highlights_percentile": 99.5,
        "midtones": "normalized_median_clamped_0.01_0.99",
        "compression": 0.8,
        "source": "lib/healpix_db/healpix_browser_qt/core/stf_engine.cpp auto_stretch() L71-125",
    },
    "fov_constraints": {
        "MIN_FOV": 0.01,
        "MAX_FOV": 50.0,
        "FOV_SPEED": 0.0015,
        "DRAG_RATIO": 0.003,
        "ARROW_RATIO": 0.1,
        "FOV_STEP": 1.2,
        "source": "lib/healpix_db/healpix_browser_qt/widgets/sphere_view.h L102-112",
    },
    "file_routing": {
        ".hiss": "SphereView + RenderMode::SPHERE + set_initial_view_from_data(bbox)",
        ".hcsd": "SphereView + RenderMode::SPHERE + reset_view()",
        "source": "lib/healpix_db/healpix_browser_qt/app/main_window.cpp L200-226",
    },
}


def main() -> int:
    HASHES_DIR.mkdir(parents=True, exist_ok=True)
    RAW_LOGS_DIR.mkdir(parents=True, exist_ok=True)

    log_lines = []
    t_start = time.time()
    log_lines.append(f"P09-003 canonical_dataset 生成开始: {datetime.now(timezone.utc).isoformat()}")

    # --- 阶段 1: T1-T4 测光代表帧 (7 个 canonical 失败帧) ---
    log_lines.append("")
    log_lines.append("=== 阶段 1: T1-T4 测光代表帧 (7 个 canonical 失败帧) ===")
    canonical_frames_with_hash = []
    for entry in CANONICAL_FRAMES:
        path = REPO_ROOT / entry["rel_path"]
        info = file_info(path)
        merged = {**entry, **{k: v for k, v in info.items() if k != "path"}}
        merged["abs_path"] = str(path)
        canonical_frames_with_hash.append(merged)
        log_lines.append(
            f"  [{entry['frame_id']}] {entry['rel_path']}"
            f" size={info.get('size_mb', 'N/A')}MB sha256={info.get('sha256', 'N/A')[:16]}..."
        )

    # --- 阶段 2: Galaxy Center 32 Red 帧 ---
    log_lines.append("")
    log_lines.append("=== 阶段 2: Galaxy Center 32 Red 帧 (panel1=11, panel2=11, panel3=10) ===")
    galaxy_center_panels = {
        "panel1": GALAXY_CENTER_PANEL1_RED,
        "panel2": GALAXY_CENTER_PANEL2_RED,
        "panel3": GALAXY_CENTER_PANEL3_RED,
    }
    galaxy_center_frames = {}
    total_galaxy_center = 0
    for panel_name, filenames in galaxy_center_panels.items():
        panel_dir = REPO_ROOT / "testdata" / "Galaxy_Center_T4" / "lights" / panel_name
        panel_entries = []
        for fname in filenames:
            path = panel_dir / fname
            info = file_info(path)
            panel_entries.append({
                "filename": fname,
                "abs_path": str(path),
                "rel_path": str(path.relative_to(REPO_ROOT)),
                "size_bytes": info.get("size_bytes"),
                "size_mb": info.get("size_mb"),
                "sha256": info.get("sha256"),
                "mtime_utc": info.get("mtime_utc"),
                "exists": info.get("exists", False),
            })
            total_galaxy_center += 1
            log_lines.append(
                f"  [{panel_name}] {fname}"
                f" size={info.get('size_mb', 'N/A')}MB sha256={info.get('sha256', 'N/A')[:16]}..."
            )
        galaxy_center_frames[panel_name] = panel_entries

    log_lines.append(f"  总计 Galaxy Center Red 帧: {total_galaxy_center}")

    # --- 阶段 3: 当前 HCSD 代表文件 ---
    log_lines.append("")
    log_lines.append("=== 阶段 3: 当前 HCSD 代表文件 ===")
    current_hcsd_with_hash = []
    for entry in CURRENT_HCSD_FILES:
        path = REPO_ROOT / entry["rel_path"]
        info = file_info(path)
        merged = {**entry, **{k: v for k, v in info.items() if k != "path"}}
        merged["abs_path"] = str(path)
        current_hcsd_with_hash.append(merged)
        log_lines.append(
            f"  [{entry['id']}] {entry['rel_path']}"
            f" size={info.get('size_mb', 'N/A')}MB sha256={info.get('sha256', 'N/A')[:16]}..."
        )

    # --- 阶段 4: 浏览器默认 HISS + 固定视角 ---
    log_lines.append("")
    log_lines.append("=== 阶段 4: 浏览器默认 HISS + 固定视角 ===")
    browser_hiss_path = REPO_ROOT / BROWSER_DEFAULT_HISS["rel_path"]
    browser_hiss_info = file_info(browser_hiss_path)
    browser_default_hiss = {
        **BROWSER_DEFAULT_HISS,
        "abs_path": str(browser_hiss_path),
        "size_bytes": browser_hiss_info.get("size_bytes"),
        "size_mb": browser_hiss_info.get("size_mb"),
        "sha256": browser_hiss_info.get("sha256"),
        "mtime_utc": browser_hiss_info.get("mtime_utc"),
        "exists": browser_hiss_info.get("exists", False),
    }
    log_lines.append(
        f"  [browser_default_hiss] {BROWSER_DEFAULT_HISS['rel_path']}"
        f" size={browser_hiss_info.get('size_mb', 'N/A')}MB sha256={browser_hiss_info.get('sha256', 'N/A')[:16]}..."
    )

    # --- 阶段 5: 装配 canonical_dataset_v1.2.json ---
    log_lines.append("")
    log_lines.append("=== 阶段 5: 装配 canonical_dataset_v1.2.json ===")
    canonical_dataset = {
        "schema_version": "1.2",
        "task_id": "P09-003",
        "gate": "G9",
        "generated_at_utc": datetime.now(timezone.utc).isoformat(),
        "repo_root": str(REPO_ROOT),
        "v11_baseline": {
            "head_commit": "ed145a7",
            "branch": "main",
            "v12_head_commit_at_p09_002": "06df865",
        },
        "photometric_failure_frames": {
            "description": (
                "7 个 canonical 测光失败/退化帧 (来自 P05-002 stage1 e2e 验证). "
                "全部 G-002 缺口退化: n_matched ∈ {0,1}, sigma_residual=0.0. "
                "禁止用其他帧替换这些失败样本 (P09-003 禁止捷径条款)."
            ),
            "g002_gap_root_cause": (
                "KD-tree star_matcher 返回 0 或 1 对匹配, 导致 sigma_residual=0, "
                "SNR 模型未构建, HISS has_snr=0, stage2 SNR² 加权退化为等权. "
                "根因待查 (疑似 PSF 星点坐标与 Gaia 星表 WCS 转换或匹配半径配置问题). "
                "首次记录: engineering/evidence/P00-003/old_cli_baseline.json L189 L293-L297."
            ),
            "frames": canonical_frames_with_hash,
            "evidence_sources": [
                "engineering/evidence/P00-003/old_cli_baseline.json",
                "engineering/evidence/P05-002/stage1_e2e_results.json",
                "engineering/evidence/P05-002/frames/P05-001-C002/stage1_meta.json",
                "engineering/evidence/P05-002/frames/P05-001-C007/stage1_full_log.txt",
                "engineering/evidence/P03-001/lum_T2_noFlat_run.log",
                "engineering/evidence/P03-002/orchestrator_test_normal.log",
                "engineering/evidence/P07-001/TEST_REPORT.md",
                "engineering/evidence/P03-004/snr_model_validation.json",
                "engineering/evidence/P06-001/stage2_compat_results.json",
                "engineering/evidence/P06-002/stage2_gradient_evidence.json",
            ],
        },
        "galaxy_center_32_red_frames": {
            "description": (
                "银心三片 32 Red 帧 (T4 设备, panel1=11, panel2=11, panel3=10). "
                "v1.2 P13 阶段将用于无梯度/有梯度 HCSD 接缝对比."
            ),
            "total_frames": total_galaxy_center,
            "panel_breakdown": {
                "panel1": len(galaxy_center_frames["panel1"]),
                "panel2": len(galaxy_center_frames["panel2"]),
                "panel3": len(galaxy_center_frames["panel3"]),
            },
            "panels": galaxy_center_frames,
        },
        "current_hcsd_baselines": {
            "description": (
                "当前 HCSD 基线代表文件 (P07-001 性能基线 + P08-002 干净环境基线). "
                "v1.2 阶段浏览器性能基线测试将使用这些 HCSD 作为输入."
            ),
            "files": current_hcsd_with_hash,
        },
        "browser_default_viewpoint": {
            "description": (
                "浏览器固定视角与 STF 配置 (所有默认值硬编码在 C++ 源码中, 无 .ini/.json 配置文件). "
                "v1.2 阶段 P15 浏览器性能优化必须以此视角作为基线."
            ),
            **BROWSER_FIXED_VIEWPOINT,
            "default_loaded_hiss": browser_default_hiss,
        },
        "no_shortcut_clause": {
            "description": (
                "P09-003 禁止捷径条款: 不得用随意选择的数据替换失败样本. "
                "本数据集中所有失败帧均来自 P05-002 既有 evidence, 不得替换."
            ),
            "failure_frame_immutability": "photometric_failure_frames.frames[*].sha256 一经冻结不得更改",
            "galaxy_center_immutability": "galaxy_center_32_red_frames.panels[*][*].sha256 一经冻结不得更改",
        },
    }

    # --- 写出 ---
    out_json = EVIDENCE_DIR / "canonical_dataset_v1.2.json"
    out_json.write_text(json.dumps(canonical_dataset, indent=2, ensure_ascii=False), encoding="utf-8")
    log_lines.append(f"  写出: {out_json.relative_to(REPO_ROOT)}")

    hash_detail_path = HASHES_DIR / "all_hashes.json"
    hash_detail = {
        "photometric_failure_frames": [
            {"frame_id": f["frame_id"], "rel_path": f["rel_path"], "sha256": f.get("sha256"),
             "size_bytes": f.get("size_bytes")}
            for f in canonical_frames_with_hash
        ],
        "galaxy_center_32_red_frames": {
            panel: [
                {"filename": f["filename"], "rel_path": f["rel_path"], "sha256": f.get("sha256"),
                 "size_bytes": f.get("size_bytes")}
                for f in frames
            ]
            for panel, frames in galaxy_center_frames.items()
        },
        "current_hcsd_baselines": [
            {"id": f["id"], "rel_path": f["rel_path"], "sha256": f.get("sha256"),
             "size_bytes": f.get("size_bytes")}
            for f in current_hcsd_with_hash
        ],
        "browser_default_hiss": {
            "rel_path": browser_default_hiss["rel_path"],
            "sha256": browser_default_hiss.get("sha256"),
            "size_bytes": browser_default_hiss.get("size_bytes"),
        },
    }
    hash_detail_path.write_text(json.dumps(hash_detail, indent=2, ensure_ascii=False), encoding="utf-8")
    log_lines.append(f"  写出: {hash_detail_path.relative_to(REPO_ROOT)}")

    elapsed = round(time.time() - t_start, 3)
    log_lines.append("")
    log_lines.append(f"P09-003 canonical_dataset 生成完成. 耗时 {elapsed}s")

    raw_log_path = RAW_LOGS_DIR / "generate_canonical_dataset.log"
    raw_log_path.write_text("\n".join(log_lines), encoding="utf-8")
    print("\n".join(log_lines))
    return 0


if __name__ == "__main__":
    sys.exit(main())
