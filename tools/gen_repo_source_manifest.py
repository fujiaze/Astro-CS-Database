#!/usr/bin/env python3
# V17 G7/G5：first-party repo source manifest 生成器（NON_PRODUCTION_TOOL_ONLY）
#
# 输出 CSV：path,SHA256,semantic classification,production caller
#   - 遍历 first-party 仓库（lib/ docs/ reports/ tools/ 工程控制/ self_review/），
#     排除 .git / run / testdata / 外部数据 / zip / 归档副本；
#   - classification 按模块语义；caller 按已知生产入口映射；
#   - 该清单用于复核"一个科学语义只剩一个 production implementation"。
import csv
import hashlib
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = Path(sys.argv[1]) if len(sys.argv) > 1 else \
    ROOT / "run" / "temp" / "p2_v17_evidence" / "repo_source_manifest.csv"

EXCLUDE_DIRS = {".git", "run", "testdata", "GaiaDR3", "GaiaDR3SP",
                "siril-1.4.3", "archive_deliverables", "artifacts",
                "BASS DR3", "launch", "output", "docs_snapshot", "build",
                "build2"}
EXCLUDE_SUFFIXES = {".zip", ".exe", ".dll", ".a", ".o", ".obj", ".png",
                    ".jpg", ".xisf", ".fits", ".fts", ".tsv", ".cache",
                    ".ninja_deps", ".ninja_log"}
INCLUDE_ROOTS = ["lib", "docs", "reports", "tools", "工程控制", "self_review",
                 "memory.md", "README.md", "AGENTS.md", "HANDOVER.md",
                 "toolchain.ps1"]

# 语义分类（按路径首段）
CLASS_MAP = [
    ("lib/orchestrator", "ORCHESTRATOR"),
    ("lib/astro_image_io", "ASTRO_IMAGE_IO"),
    ("lib/calibration", "CALIBRATION"),
    ("lib/plate_solve", "PLATE_SOLVE"),
    ("lib/dynamic_psf", "PSF"),
    ("lib/photometric_calib", "PHOTOMETRIC"),
    ("lib/snr_estimator", "SNR_ESTIMATOR"),
    ("lib/star_detector", "STAR_DETECTOR"),
    ("lib/gaia_xpsd_client", "GAIA_XPSD"),
    ("lib/healpix_db/healpix_drizzle", "DRIZZLE"),
    ("lib/healpix_db/healpix_browser_qt", "BROWSER"),
    ("lib/healpix_db/archive", "ARCHIVED_LEGACY"),
    ("lib/healpix_db", "HEALPIX_DB"),
    ("lib/phase2", "PHASE2"),
    ("lib/acr", "ACR"),
    ("lib/common", "SHARED_COMMON"),
    ("lib/data_pipeline", "DATA_PIPELINE"),
    ("docs", "DOC"),
    ("reports", "REPORT"),
    ("tools", "TOOL"),
    ("工程控制", "ENGINEERING_CONTROL"),
    ("self_review", "SELF_REVIEW"),
]

# 生产 caller 映射（lib 各模块的调用方）
CALLER_MAP = {
    "ORCHESTRATOR": "production entry: orchestrator.exe",
    "ASTRO_IMAGE_IO": "orchestrator Phase1 + astrocs-stage2 Phase2 + browser",
    "CALIBRATION": "orchestrator Phase1 CALIBRATE",
    "PLATE_SOLVE": "orchestrator Phase1 PLATESOLVE",
    "PSF": "orchestrator Phase1 PSF",
    "PHOTOMETRIC": "orchestrator Phase1 PHOTOMETRIC",
    "SNR_ESTIMATOR": "orchestrator Phase1 SNR",
    "STAR_DETECTOR": "orchestrator Phase1 PLATESOLVE (via ipv_solver)",
    "GAIA_XPSD": "orchestrator Phase1 PLATESOLVE/PHOTOMETRIC",
    "DRIZZLE": "orchestrator Phase1 DRIZZLE",
    "BROWSER": "healpix_browser_qt.exe (consumer only)",
    "ARCHIVED_LEGACY": "NONE (not built/linked/called; archive only)",
    "HEALPIX_DB": "shared / browser / drizzle",
    "PHASE2": "astrocs-stage2.exe (production) + tests",
    "ACR": "phase2 backend (KernelRegistry), same contract",
    "SHARED_COMMON": "shared (all modules)",
    "DATA_PIPELINE": "tooling (non-production)",
    "DOC": "documentation (contract)",
    "REPORT": "reporting (evidence)",
    "TOOL": "tooling (non-production unless marked)",
    "ENGINEERING_CONTROL": "engineering control (authoritative configs)",
    "SELF_REVIEW": "self-review evidence",
}


def sha256(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def classify(rel):
    s = rel.as_posix()
    for prefix, cls in CLASS_MAP:
        if s.startswith(prefix):
            return cls
    return "OTHER"


def main():
    rows = []
    seen = set()
    for root in INCLUDE_ROOTS:
        p = ROOT / root
        if p.is_file():
            items = [p]
        elif p.is_dir():
            items = sorted(p.rglob("*"))
        else:
            continue
        for f in items:
            if not f.is_file():
                continue
            rel = f.relative_to(ROOT)
            if any(part in EXCLUDE_DIRS for part in rel.parts):
                continue
            if f.suffix.lower() in EXCLUDE_SUFFIXES:
                continue
            key = rel.as_posix()
            if key in seen:
                continue
            seen.add(key)
            cls = classify(rel)
            rows.append({
                "path": key,
                "sha256": sha256(f),
                "classification": cls,
                "production_caller": CALLER_MAP.get(cls, "N/A"),
            })
    rows.sort(key=lambda r: r["path"])
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT, "w", newline="", encoding="utf-8") as fp:
        w = csv.DictWriter(fp, fieldnames=["path", "sha256",
                                           "classification",
                                           "production_caller"])
        w.writeheader()
        w.writerows(rows)
    print(f"repo source manifest: {len(rows)} files -> {OUT}")


if __name__ == "__main__":
    main()
