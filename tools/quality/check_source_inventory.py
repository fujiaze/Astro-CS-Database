#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""check_source_inventory.py — V19R2 S0 inventory gate.

Generates, from `git ls-files`:
  reports/v19r2/source_manifest.csv        (path,size_bytes,sha256,type,module,shipping)
  reports/v19r2/file_audit_inventory.csv   (MASTER_CONTROL_SPEC S8 field set, seeded)
  reports/v19r2/evidence/quality/shipping_units.csv (production compile units)

Exclusions (control package): build/vendor/data/archive.  Third-party vendored
code (lib/astro_image_io/third_party) is recorded but excluded from first-party.

Timeout guard: this is a local-only deterministic walk; no network.
"""

from __future__ import annotations

import csv
import hashlib
import os
import subprocess
import sys
import time


def _deduce_root() -> str:
    # auto-deduce project root: walk up until docs/ and lib/ found (Linux-portable)
    try:
        p = os.path.abspath(__file__)
        cur = os.path.dirname(p)
        for _ in range(5):
            if os.path.isdir(os.path.join(cur, "docs")) and os.path.isdir(os.path.join(cur, "lib")):
                return cur
            parent = os.path.dirname(cur)
            if parent == cur:
                break
            cur = parent
    except Exception:
        pass
    cwd = os.getcwd()
    if os.path.isdir(os.path.join(cwd, "docs")) and os.path.isdir(os.path.join(cwd, "lib")):
        return cwd
    return os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

ROOT = _deduce_root()
REV = os.path.join(ROOT, "reports", "v19r2")

EXCLUDED_PARTS = {"build", "build2", "_deps", "CMakeFiles", "archive",
                  "__pycache__", ".git", "worktrees", "third_party"}
DATA_PARTS = {"BASS DR3", "testdata", "GaiaDR3", "GaiaDR3SP", "siril-1.4.3"}
CONTROL_PARTS = {"工程控制", "AstroCS.wiki"}
REPORT_PARTS = {"reports", "self_review"}

SHIPPING_EXT = {".c", ".cpp", ".h", ".hpp", ".cu", ".cc", ".cxx", ".hh"}

# V19 baseline shipping list (module-relative), reused and verified on HEAD.
V19_SHIPPING = os.path.join(ROOT, "run", "temp", "V19_review",
                            "evidence", "quality", "shipping_units.csv")

MODULE_MAP = {
    "acr": "lib/acr",
    "astro_image_io": "lib/astro_image_io",
    "plate_solve": "lib/plate_solve",
    "healpix_drizzle": "lib/healpix_db/healpix_drizzle",
    "orchestrator": "lib/orchestrator/cpp",
    "phase2": "lib/phase2",
    "photometric_calib": "lib/photometric_calib",
    "calibration": "lib/calibration",
    "star_detector": "lib/star_detector",
    "dynamic_psf": "lib/dynamic_psf",
    "common": "lib/common",
    "gaia_xpsd_client": "lib/gaia_xpsd_client",
    "snr_estimator": "lib/snr_estimator",
}


def tracked_files() -> list[str]:
    r = subprocess.run(["git", "ls-files"], cwd=ROOT, capture_output=True,
                       text=True, encoding="utf-8", errors="replace",
                       timeout=120)
    if r.returncode != 0:
        sys.exit(f"git ls-files failed: {r.stderr}")
    return [p for p in r.stdout.splitlines() if p]


def sha256_of(path: str) -> str:
    try:
        h = hashlib.sha256()
        with open(path, "rb") as f:
            for chunk in iter(lambda: f.read(1 << 20), b""):
                h.update(chunk)
        return h.hexdigest()
    except OSError:
        return "MISSING"


def classify(path: str) -> tuple[str, str]:
    """Return (category, module)."""
    top = path.split("/", 1)[0]
    if top in DATA_PARTS:
        return "data", top
    if top in CONTROL_PARTS:
        return "project_control", top
    if top in REPORT_PARTS:
        return "report", top
    parts = path.split("/")
    if any(p in EXCLUDED_PARTS for p in parts):
        return "excluded", top
    ext = os.path.splitext(path)[1].lower()
    if path.startswith("lib/"):
        if "/tests/" in "/" + path or path.startswith("lib/") and "/test/" in "/" + path:
            cat = "test"
        elif "/examples/" in "/" + path:
            cat = "example"
        elif "/qualification/" in "/" + path:
            cat = "qualification"
        elif ext in SHIPPING_EXT:
            cat = "shipping_src"
        elif ext in (".py", ".ps1", ".sh", ".f", ".cu"):
            cat = "tool"
        elif ext in (".md", ".txt"):
            cat = "doc"
        else:
            cat = "aux"
        mod = parts[1] if len(parts) > 1 else top
        return cat, mod
    if path.startswith("tools/"):
        return ("tool" if ext in (".py", ".ps1") else "tool_aux"), "tools"
    if path.startswith("docs/"):
        return "doc", "docs"
    if path in ("toolchain.ps1", "AGENTS.md", "README.md", "CHANGELOG.md",
                "HANDOVER.md", "memory.md", ".gitignore", ".gitattributes",
                ".editorconfig", ".clang-format", "VISUAL_CHECK_README.md"):
        return "root", "root"
    return "aux", top


def loc_of(path: str) -> int:
    ext = os.path.splitext(path)[1].lower()
    if ext not in (".c", ".cpp", ".h", ".hpp", ".cc", ".cxx", ".hh", ".cu"):
        return 0
    try:
        with open(path, "rb") as f:
            return sum(1 for _ in f)
    except OSError:
        return 0


def load_v19_shipping() -> set[str]:
    out: set[str] = set()
    if not os.path.exists(V19_SHIPPING):
        return out
    with open(V19_SHIPPING, newline="", encoding="utf-8-sig") as f:
        for row in csv.DictReader(f):
            mod = row.get("module", "")
            p = row.get("path", "")
            base = MODULE_MAP.get(mod)
            if base and not p.startswith("build"):
                out.add(f"{base}/{p}")
    return out


def main() -> int:
    t0 = time.time()
    files = tracked_files()
    v19 = load_v19_shipping()
    os.makedirs(os.path.join(REV, "evidence", "quality"), exist_ok=True)

    manifest_rows = []
    audit_rows = []
    shipping_rows = []
    shipped = set()
    for p in files:
        cat, mod = classify(p)
        size = os.path.getsize(os.path.join(ROOT, p)) if os.path.exists(
            os.path.join(ROOT, p)) else 0
        sha = sha256_of(os.path.join(ROOT, p))
        manifest_rows.append([p, size, sha, cat, mod, "yes" if cat == "shipping_src" else "no"])
        if cat in ("excluded", "data", "project_control", "report"):
            continue
        shipping = "yes" if (cat == "shipping_src" or p in v19) else "no"
        if shipping == "yes":
            shipped.add(p)
            rel = p
            if p.startswith("lib/"):
                rel = "/".join(p.split("/")[2:])
            shipping_rows.append([mod, rel, "C++" if p.endswith((".cpp", ".h", ".hpp", ".cc", ".hh")) else "C",
                                  "no", "yes"])
        audit_rows.append([
            p, cat, shipping, mod,
            "test" if cat in ("test", "qualification") else "production",
            loc_of(p), "", "", "", "", "PENDING",
            0, 0, 0, 0, "", "", "", "", "", "", "", "", "", "", "", "",
        ])

    # Files in V19 shipping list that no longer exist -> record for reconciliation.
    tracked = set(files)
    missing = sorted(v19 - tracked)
    for p in missing:
        audit_rows.append([
            p, "shipping_src", "yes", "unknown", "production", 0,
            "", "", "", "", "DELETED", 0, 0, 0, 0, "", "", "", "",
            "", "", "", "", "", "", "", "S0",
        ])

    with open(os.path.join(REV, "source_manifest.csv"), "w", newline="",
              encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["path", "size_bytes", "sha256", "type", "module", "shipping"])
        w.writerows(manifest_rows)
    with open(os.path.join(REV, "file_audit_inventory.csv"), "w", newline="",
              encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow([
            "path", "type", "shipping_target", "module", "production_or_test",
            "loc", "owner_doc", "science_ids", "algorithm_ids", "api_ids",
            "review_status", "findings_p0", "findings_p1", "findings_p2",
            "findings_p3", "dead", "duplicate", "legacy", "comment_hygiene",
            "ownership_ok", "thread_ok", "error_ok", "numeric_ok",
            "performance_ok", "tests_ok", "docs_ok", "reviewer_round",
        ])
        w.writerows(audit_rows)
    with open(os.path.join(REV, "evidence", "quality", "shipping_units.csv"),
              "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["module", "path", "language", "third_party", "shipping"])
        w.writerows(sorted(shipping_rows))

    n_audit = len(audit_rows) - len(missing)
    print(f"S0 inventory: tracked={len(files)} first_party_audit={n_audit} "
          f"shipping={len(shipped)} deleted={len(missing)} "
          f"elapsed={time.time()-t0:.1f}s")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
