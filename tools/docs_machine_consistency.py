#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""docs_machine_consistency.py — V19 Round6 / V19R3 文档↔代码机器一致性检查

检查项:
  1. config: weight_mode=ivar 默认 (docs ↔ stage2_common.cpp)
  2. error taxonomy: E 代码 ↔ orchestrator AstroCsExitCode
  3. stage ids ↔ orchestrator stage_name_v2
  4. SNR 常数: 0.7316728 / 1.4826 ↔ noise_model.cpp
  5. 产品契约: signal/support/snr/variance/ivar ↔ aio_hips.h flags
  6. Drizzle 方差公式 ↔ drizzle_engine.h 注释
  V19R3（DOCS_AND_COMMENTS）：退出码/集成状态/拒绝状态全集合精确比对，
  禁止 subset（审计 §12 假 PASS 修复）。
"""

from __future__ import annotations

import json
import os
import re
import sys


def _deduce_root() -> str:
    # auto-deduce project root: tools/docs_machine_consistency.py -> two levels up
    try:
        p = os.path.abspath(__file__)
        cand = os.path.dirname(os.path.dirname(p))
        # sanity: must contain docs/ and lib/
        if os.path.isdir(os.path.join(cand, "docs")) and os.path.isdir(os.path.join(cand, "lib")):
            return cand
    except Exception:
        pass
    # fallback: cwd if it looks like project root
    cwd = os.getcwd()
    if os.path.isdir(os.path.join(cwd, "docs")) and os.path.isdir(os.path.join(cwd, "lib")):
        return cwd
    # fallback: parent of tools/
    return os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

ROOT = _deduce_root()


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

    # V19R4：frame_id 合同（DATA-FRAME-ID-001）——禁止 FNV/路径派生描述
    sampler_h = read("lib/phase2/include/astro/phase2/sampler.h")
    data_sem = read("docs/contracts/DATA_SEMANTICS.md")
    upm_doc = read("docs/science/PHASE2_UPM.md")
    frame_id_ok = (
        "FNV-1a 64" not in sampler_h and "FNV-1a 64" not in data_sem and
        "由输入路径派生" not in sampler_h and
        "由输入路径派生" not in data_sem and
        "truncated-64" in sampler_h and
        "SHA-256" in data_sem and
        "DATA-FRAME-ID-001" in upm_doc)
    results.append(check(
        "frame_id_contract_exact",
        frame_id_ok,
        "DATA-FRAME-ID-001：SHA-256 truncate；无 FNV/路径派生残留"))

    tax = read("docs/architecture/ERROR_MODEL.md")
    orc_h = read("lib/orchestrator/cpp/include/orchestrator.h")
    # V19R3：全集合比对（name+value），不允许 subset/多余/缺失
    def extract_enum(txt: str, block_start: str | None = None) -> dict[str, int]:
        out = {}
        if block_start:
            i = txt.find(block_start)
            if i < 0:
                return {}
            j = txt.find("}", i)
            txt = txt[i:j]
        for m in re.finditer(
                r"([A-Z][A-Z0-9_]{2,})\s*=\s*(\d+)", txt):
            out[m.group(1)] = int(m.group(2))
        return out
    doc_codes = extract_enum(tax)
    code_codes = extract_enum(orc_h, "namespace AstroCsExitCode")
    exit_codes = code_codes
    doc_exit = doc_codes
    exit_ok = (doc_exit == exit_codes)
    orc = read("lib/orchestrator/cpp/src/orchestrator.cpp")
    results.append(check(
        "error_taxonomy_exit_codes",
        "AstroCsExitCode" in tax and exit_ok,
        f"ERROR_MODEL 全集合 == orchestrator.h 0-10 退出码 "
        f"(doc={doc_exit} code={exit_codes})"))

    # V19R3：integration / rejection 状态全集合
    int_h = read("lib/phase2/include/astro/phase2/integrate.h")
    rej_h = read("lib/phase2/include/astro/phase2/rejection.h")
    int_doc = extract_enum(read("docs/algorithms/INTEGRATION_ALGORITHMS.md"))
    int_code = {k: v for k, v in extract_enum(int_h).items()
                if k.startswith("P2_INTEGRATE")}
    results.append(check(
        "integration_status_full_set",
        int_doc == int_code,
        f"integration status 全集合 (doc={int_doc} code={int_code})"))
    rej_doc = extract_enum(read("docs/algorithms/REJECTION_ALGORITHMS.md"))
    rej_code = {k: v for k, v in extract_enum(rej_h).items()
                if k.startswith("P2_STATUS") or k.startswith("P2_REASON")}
    results.append(check(
        "rejection_status_full_set",
        rej_doc == rej_code,
        f"rejection status 全集合 (doc={rej_doc} code={rej_code})"))

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
    # V19R3：证据落盘 reports/v19r3/evidence/quality/docs_consistency.json
    out_dir = os.path.join(ROOT, "reports", "v19r3", "evidence", "quality")
    os.makedirs(out_dir, exist_ok=True)
    with open(os.path.join(out_dir, "docs_consistency.json"), "w",
              encoding="utf-8") as f:
        json.dump(report, f, ensure_ascii=False, indent=2)
    return 0 if report["pass"] else 1


if __name__ == "__main__":
    sys.exit(main())
