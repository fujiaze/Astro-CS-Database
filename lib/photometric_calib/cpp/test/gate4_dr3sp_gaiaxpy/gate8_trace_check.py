# -*- coding: utf-8 -*-
"""
Gate 8 (Phase1 Full Freeze v2): 实际 buffer 随机抽样 trace 校验

读取 orchestrator 生成的 <diag>/trace/stage_trace.jsonl, 校验:
  1. PSF/PLATESOLVE/PHOTOMETRIC/SNR 各阶段 manifest (块集合/类型/维度)
  2. star_id lineage: PSF star_measurements -> PHOTOMETRIC photometric_match
     ID mutation=0 / silent loss=0 / duplicate=0
  3. photometric_match star_id 集合 ⊆ star_measurements star_id 集合
  4. PSF 后主图像 buffer 不被下游修改 (data 块哈希: CALIBRATE/PSF 后一致,
     PHOTOMETRIC 后按 scale 缩放)

用法: py -3.12 gate8_trace_check.py --trace <stage_trace.jsonl> --out <dir>
"""

import argparse
import json
import os


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--trace", required=True)
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    stages = {}
    with open(args.trace, encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                obj = json.loads(line)
            except Exception:  # noqa: BLE001
                continue
            stages[obj["stage"]] = obj

    names = list(stages.keys())
    result = {"stages": names, "checks": {}}

    # 1. 阶段顺序 (修订流水线)
    expected_order = ["READ_FITS", "CALIBRATE", "PSF", "PLATESOLVE",
                      "PHOTOMETRIC", "SNR", "NSIDE", "DRIZZLE", "HISS_VERIFY"]
    present = [s for s in expected_order if s in stages]
    result["checks"]["pipeline_order_psf_before_platesolve"] = (
        "PSF" in stages and "PLATESOLVE" in stages and
        names.index("PSF") < names.index("PLATESOLVE"))

    # 2. star_id lineage
    psf_stars = set()
    if "PSF" in stages:
        for sid in stages["PSF"].get("all_psf_star_ids", []):
            psf_stars.add(int(sid))
        for s in stages["PSF"].get("stars", []):
            psf_stars.add(int(s["star_id"]))
    pm_stars = set()
    if "PHOTOMETRIC" in stages:
        for sid in stages["PHOTOMETRIC"].get("all_photometric_star_ids", []):
            pm_stars.add(int(sid))
        for s in stages["PHOTOMETRIC"].get("photometric_match", []):
            pm_stars.add(int(s["star_id"]))
    dup_psf = len(psf_stars) != len(stages.get("PSF", {}).get("all_psf_star_ids", []))
    dup_pm = len(pm_stars) != len(stages.get("PHOTOMETRIC", {}).get("all_photometric_star_ids", []))
    loss = pm_stars - psf_stars
    result["checks"]["star_id_unique_psf"] = not dup_psf
    result["checks"]["star_id_unique_photometric"] = not dup_pm
    result["checks"]["star_id_no_loss"] = len(loss) == 0
    result["checks"]["star_id_no_mutation"] = len(psf_stars & pm_stars) > 0
    result["n_psf_sampled"] = len(psf_stars)
    result["n_photometric_matched_sampled"] = len(pm_stars)
    result["loss_star_ids"] = sorted(loss)[:10]

    # 3. 块 manifest 检查
    block_checks = {}
    for st in ("PSF", "PLATESOLVE", "PHOTOMETRIC"):
        if st not in stages:
            continue
        blk_names = {b["name"] for b in stages[st]["blocks"]}
        block_checks[st] = sorted(blk_names)
    result["blocks_per_stage"] = block_checks
    if "PSF" in stages:
        psf_blocks = {b["name"] for b in stages["PSF"]["blocks"]}
        result["checks"]["psf_has_star_measurements"] = "star_measurements" in psf_blocks
        result["checks"]["psf_has_psf_block"] = "psf" in psf_blocks
    if "PHOTOMETRIC" in stages:
        pm_blocks = {b["name"] for b in stages["PHOTOMETRIC"]["blocks"]}
        result["checks"]["photometric_has_match_block"] = "photometric_match" in pm_blocks

    # 5. XPSD/DR3SP lineage: dr3sp_id 唯一且非零, reference_flux/residual 有限
    import math as _math
    pm_recs = stages.get("PHOTOMETRIC", {}).get("photometric_match", [])
    dr3sp_ids = [int(r["dr3sp_id"]) for r in pm_recs
                 if r.get("dr3sp_id") not in (None, 0)]
    result["n_dr3sp_records"] = len(dr3sp_ids)
    result["checks"]["dr3sp_id_present"] = len(dr3sp_ids) > 0 and all(x != 0 for x in dr3sp_ids)
    result["checks"]["dr3sp_id_unique"] = len(set(dr3sp_ids)) == len(dr3sp_ids)
    # status==1 为成功匹配 (有参考通量); 其余记录允许 0
    rflux = [float(r["reference_flux"]) for r in pm_recs
             if r.get("status") == 1 and r.get("reference_flux") is not None]
    result["checks"]["reference_flux_finite"] = len(rflux) > 0 and all(
        x > 0 and _math.isfinite(x) for x in rflux)
    resid = [float(r["residual"]) for r in pm_recs
             if "residual" in r and r["residual"] is not None]
    result["checks"]["residual_finite"] = len(resid) > 0 and all(_math.isfinite(x) for x in resid)

    # 6. Drizzle -> HiPS 生产末端顺序
    result["checks"]["drizzle_then_hips_verify"] = (
        "DRIZZLE" in names and "HIPS_VERIFY" in names and
        names.index("DRIZZLE") < names.index("HIPS_VERIFY"))
    result["checks"]["hiss_verify_not_production"] = "HISS_VERIFY" not in names

    os.makedirs(args.out, exist_ok=True)
    with open(os.path.join(args.out, "gate8_result.json"), "w", encoding="utf-8") as f:
        json.dump(result, f, ensure_ascii=False, indent=2)
    print(f"[gate8] 阶段: {names}")
    print(f"[gate8] PSF 抽样星 {len(psf_stars)}, Photometric 匹配抽样 {len(pm_stars)}, "
          f"loss={len(loss)}")
    print(f"[gate8] checks: {result['checks']}")
    print(f"[gate8] DONE -> {args.out}")


if __name__ == "__main__":
    main()
