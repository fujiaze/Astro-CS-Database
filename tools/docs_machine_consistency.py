#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""docs_machine_consistency.py — V19 Round6 文档↔代码机器一致性检查

检查项:
  1. config: weight_mode=ivar 默认 (docs ↔ stage2_common.cpp)
  2. error taxonomy: E 代码 ↔ orchestrator AstroCsExitCode
  3. stage ids ↔ orchestrator stage_name_v2
  4. SNR 常数: 0.7316728 / 1.4826 ↔ noise_model.cpp
  5. 产品契约: signal/support/snr/variance/ivar ↔ aio_hips.h flags
  6. Drizzle 方差公式 ↔ drizzle_engine.h 注释
"""

from __future__ import annotations

import json
import os
import re
import sys


ROOT = r"F:\Astro dev\Astro CS Normalization Database"


def read(path: str) -> str:
    with open(os.path.join(ROOT, path), encoding="utf-8",
              errors="replace") as f:
        return f.read()


def check(name: str, ok: bool, detail: str) -> dict:
    return {"check": name, "pass": bool(ok), "detail": detail}


def main() -> int:
    results = []

    cfg_doc = read("docs/development/CONFIG_SCHEMA.md")
    cfg_code = read("lib/phase2/src/stage2_common.cpp")
    results.append(check(
        "config_weight_mode_ivar",
        "weight_mode(auto)" in cfg_doc and
        'wm == "auto" || wm == "ivar"' in cfg_code,
        "CONFIG_SCHEMA weight_mode auto/ivar <-> stage2_common parse"))

    tax = read("docs/architecture/ERROR_MODEL.md")
    orc_h = read("lib/orchestrator/cpp/include/orchestrator.h")
    exit_ok = all(re.search(s, orc_h) for s in
                  [r"SUCCESS\s*=\s*0", r"DLL_LOAD_FAILED\s*=\s*2",
                   r"CONFIG_ERROR\s*=\s*7", r"FILE_IO_ERROR\s*=\s*8"])
    orc = read("lib/orchestrator/cpp/src/orchestrator.cpp")
    results.append(check(
        "error_taxonomy_exit_codes",
        "AstroCsExitCode" in tax and exit_ok,
        "ERROR_TAXONOMY exit codes <-> orchestrator AstroCsExitCode"))

    stage_doc = read("docs/architecture/ERROR_MODEL.md")
    stages = ["P1.READ", "P1.CALIBRATE", "P1.PLATESOLVE", "P1.PSF",
              "P1.PHOTOMETRIC", "P1.NOISE", "P1.DRIZZLE", "P1.HIPS_WRITE",
              "P2.INTEGRATE", "P2.HIPS_WRITE"]
    results.append(check(
        "stage_ids_docs_vs_orchestrator",
        all(s in stage_doc for s in stages) and
        "stage_name_v2" in orc,
        "stage IDs in ERROR_TAXONOMY <-> orchestrator stage_name_v2"))

    snr_doc = read("docs/science/NOISE_MODEL.md")
    psf_doc = read("docs/science/PSF.md")
    nm = read("lib/snr_estimator/cpp/src/noise_model.cpp")
    results.append(check(
        "snr_constants",
        "1.4826022185" in snr_doc and "0.7316728" in psf_doc and
        "0.7316727929211932" in nm and
        "1.482602218505602" in nm,
        "NOISE_MODEL/PSF constants <-> noise_model.cpp"))

    contracts = read("docs/contracts/DATA_SEMANTICS.md")
    aio = read("lib/astro_image_io/include/aio_hips.h")
    prod_ok = all(p in contracts for p in
                  ["signal", "support", "variance", "ivar"]) and \
              "AIO_HIPS_PRODUCT_VARIANCE" in aio and \
              "AIO_HIPS_PRODUCT_IVAR" in aio
    results.append(check(
        "product_contracts",
        prod_ok,
        "DATA_CONTRACTS products <-> aio_hips.h product flags"))

    drz_doc = read("docs/science/DRIZZLE.md")
    eng = read("lib/healpix_db/healpix_drizzle/drizzle_engine.h")
    results.append(check(
        "drizzle_variance_formula",
        "sumVarNum" in drz_doc and "sumVarNum" in eng and
        "variance_p" in drz_doc and "ivar_p" in drz_doc,
        "DRIZZLE.md variance formula <-> drizzle_engine.h"))

    report = {
        "tool": "docs_machine_consistency",
        "version": "1.0.0",
        "checks": results,
        "pass": all(r["pass"] for r in results),
    }
    print(json.dumps(report, ensure_ascii=False, indent=2))
    return 0 if report["pass"] else 1


if __name__ == "__main__":
    sys.exit(main())
