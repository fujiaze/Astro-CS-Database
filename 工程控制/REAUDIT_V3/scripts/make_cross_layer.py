#!/usr/bin/env python3
"""Cross-layer matrix v1: derived from TRACEABILITY.csv + verified facts; honest PARTIAL where not fully audited."""
import csv, os
ROOT = open("/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt").read().strip()
REPO = "/home/lighthouse/Astro CS Database"
OUT = os.path.join(ROOT, "package", "07_cross_layer")
os.makedirs(OUT, exist_ok=True)

fields = ["module","sci_id","science_doc","formula_or_invariant","units","alg_id","algorithm_doc","algorithm_steps","complexity","api_id","header","symbol","exact_signature","ownership","lifetime","error_model","threading","async_model","backend_route","config_keys","defaults","source_files","test_ids","test_files","current_test_result","consistency_status","contradiction_id","evidence"]
rows = []

trace = list(csv.DictReader(open(os.path.join(REPO, "docs/TRACEABILITY.csv"), encoding="utf-8")))

# verified contradiction / fact annotations keyed by sci_id
annotations = {
  "SCI-UPM-WEIGHT-001": ("VERIFIED_AT_SOURCE", "", "raw_w = quality x control_ivar (upm.cpp L493-524); not yet numerically oracle-verified this round"),
  "ALG-UPM-CONTROL-IVAR-001": ("PARTIAL", "", "k_corr=1.4 frozen per sampler.cpp comment; MC calibration evidence not rerun this round"),
}

module_doc = {
  "phase2": "docs/science/PHASE2_UPM.md",
  "calibration": "docs/science/CALIBRATION.md",
  "dynamic_psf": "docs/science/PSF.md",
  "plate_solve": "docs/science/ASTROMETRY.md",
  "photometric_calib": "docs/science/PHOTOMETRY.md",
  "snr_estimator": "docs/science/NOISE_MODEL.md",
  "healpix_drizzle": "docs/science/DRIZZLE.md",
  "astro_image_io": "docs/science/DRIZZLE.md",
  "acr": "docs/science/ACR_EQUIVALENCE.md",
  "rejection": "docs/science/REJECTION.md",
  "integration": "docs/science/INTEGRATION.md",
}

for r in trace:
    sci = r.get("requirement_id", "").strip()
    mod = r.get("module", "").strip()
    alg = r.get("algorithm_id", "").strip()
    api = r.get("public_api", "").strip()
    impl = r.get("implementation_files", "").strip()
    tests = r.get("test_ids", "").strip()
    test_files = r.get("test_files", "").strip()
    title = r.get("title", "").strip()
    # contradiction mapping for the four verified science contradictions
    status, cid, ev = "PARTIAL", "", "auto-derived from TRACEABILITY row; per-field semantic audit pending"
    if "常量场不变量" in title or "常量场" in title and "UPM" in sci:
        status, cid = "CONTRADICTION", "SCI-UPM-CONSTANT-FIELD-001"
        ev = "science doc PHASE2_UPM.md L75 states raw=C => C_f=C; source model y=M+C_f with reference-frame gauge C_f=0 (upm.cpp L20,592-593,646-648) gives M=C, C_f=0"
    elif "Huber" in title or "huber" in title:
        status, cid = "CONTRADICTION", "ALG-UPM-HUBER-001"
        ev = "algorithm doc UPM_SOLVER.md L17 states delta=1.345*median_abs_r; source uses z=r/sigma_eff with dimensionless delta=1.345 (upm.cpp L580-590,204-215)"
    elif "复杂度" in title or "complexity" in title:
        status, cid = "CONTRADICTION", "ALG-UPM-COMPLEXITY-001"
        ev = "doc claims O(iter*(obs+K log K)); source runs per-frame full-K CG (max_cg=200) for (F-1) frames, each O(K+E) per CG iteration (upm.cpp L528-564,640-648)"
    elif "Drizzle" in sci or "DRZ" in sci or "drizzle" in title:
        status, cid = "CONTRADICTION", "SCI-DRZ-DIMENSION-001"
        ev = "doc DRIZZLE.md L27 says S,F,x are ADU; writer computes sig=flux/area (aio_hips_writer.cpp L477,816) and labels product surface brightness (L1035); S=F/D is ADU/area, constant field S=C only holds for normalized equal-area grid"
    elif sci in annotations:
        status, cid, ev = annotations[sci]
    rows.append({"module": mod or "unknown", "sci_id": sci, "science_doc": r.get("authority_doc",""),
                 "formula_or_invariant": title, "units": "N/A(per-SCI; see oracle notes)",
                 "alg_id": alg, "algorithm_doc": "docs/algorithms/UPM_SOLVER.md" if "UPM" in sci else "",
                 "algorithm_steps": "N/A(per-ALG)", "complexity": "N/A(per-ALG)",
                 "api_id": api, "header": "N/A(per-API)", "symbol": api.split(";")[0].strip() if api else "",
                 "exact_signature": "N/A(per-API; see API_CONTRACTS.csv)",
                 "ownership": "N/A(per-API)", "lifetime": "N/A(per-API)", "error_model": "N/A(per-API)",
                 "threading": "N/A(per-module audit)", "async_model": "N/A(per-module audit)",
                 "backend_route": "N/A(per-module audit)",
                 "config_keys": "N/A(per-config audit)", "defaults": "N/A(per-config audit)",
                 "source_files": impl, "test_ids": tests, "test_files": test_files,
                 "current_test_result": "phase2 module: 79 PASS/1 FAIL/10 SKIP in ctest (this round)" if mod == "phase2" else "NOT_RUN this round",
                 "consistency_status": status, "contradiction_id": cid, "evidence": ev})

with open(os.path.join(OUT, "cross_layer_matrix.csv"), "w", newline="", encoding="utf-8") as f:
    w = csv.DictWriter(f, fieldnames=fields); w.writeheader(); w.writerows(rows)
from collections import Counter
c = Counter(r["consistency_status"] for r in rows)
print("cross_layer_matrix.csv rows=" + str(len(rows)) + " statuses=" + str(dict(c)))
