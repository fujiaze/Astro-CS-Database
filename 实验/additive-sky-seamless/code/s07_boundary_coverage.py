#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""S07 边界覆盖审计：把生产 49 帧接缝门的 196 条边界按**原因**分类，
并量化「82 条未进判据」到底是不是「无观测」。

被测对象（只读）：
  run/M42-E2E-02/results/seam_e2e02_v3.json
  —— 冻结记录 `实验/additive-sky-seamless/results/production_e2e_seam_record.json`
     （已随 1fa477a7a 从工作树移除，可用 `git show 1fa477a7a^:<path>` 取回）
     所钉住的源文件：sha256 = 61bc51bd39688c5f785b12fba3795e16925286972095e470c41c49019686693c。

口径（来自生产门 eng/tools/e2e/seam_footprint.py 的 v3 版本，见
`git show 0c353f351:eng/tools/e2e/seam_footprint.py`）：
  * 判据量 rel_step = median(seam)/bg，seam = 边界法向 ±d（d=2 px）差分。
  * 适用域 interior ⇔ 法向 **±ctrl_shift（默认 200 px）** 两侧各 >= min_samples(20) 个有效样本。
  * margin_px = 两侧「对照线可放置域」的**较小者**（阶梯 0/2/4/8/16/32/64/128/200 px）。
    margin_px >= 200 ⟺ interior；margin_px 为数值 ⇔ **两侧在 margin_px 处都有 >=20 个有效样本**；
    margin_px 为 None ⇔ 至少一侧在任何阶梯偏移上都不足 20 个样本（真·无观测）。
  * exclude = few_samples（该边自身测不了）| not_interior（能测，但一侧在 200 px 对照线外）。

用法：python3 s07_boundary_coverage.py --out <results_dir>
"""
from __future__ import annotations

import argparse
import collections
import hashlib
import json
import os
import subprocess
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
SEAM = os.path.join(REPO, "run", "M42-E2E-02", "results", "seam_e2e02_v3.json")
RECORD_PATH = "实验/additive-sky-seamless/results/production_e2e_seam_record.json"
RECORD_BLOB = "1fa477a7a^"
RECORD_SHA = "61bc51bd39688c5f785b12fba3795e16925286972095e470c41c49019686693c"
GATE = 1e-2


def sha256_file(p):
    return hashlib.sha256(open(p, "rb").read()).hexdigest()


def frozen_record():
    """从 git 历史取回被移除的冻结记录（只读；失败返回 None）。"""
    try:
        out = subprocess.run(["git", "show", f"{RECORD_BLOB}:{RECORD_PATH}"],
                             cwd=REPO, capture_output=True, text=True)
        if out.returncode == 0:
            return json.loads(out.stdout)
    except Exception:
        pass
    return None


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--seam", default=SEAM)
    a = ap.parse_args(argv)
    os.makedirs(a.out, exist_ok=True)

    got_sha = sha256_file(a.seam)
    rec = frozen_record()
    rec_src_sha = None
    if rec:
        for s in rec.get("sources", []):
            if s.get("path", "").endswith("seam_e2e02_v3.json"):
                rec_src_sha = s.get("sha256")
    res = dict(schema="ACSD-P5-S07-BOUNDARY/1", source=a.seam,
               source_sha256=got_sha, frozen_record_source_sha256=rec_src_sha,
               source_matches_record=(got_sha == rec_src_sha) if rec_src_sha else None,
               gate_rel_step=GATE,
               frozen_record_read_from_git=(rec is not None))

    r = json.load(open(a.seam))
    pe = r["baseline"]["per_edge"]
    val = [e for e in pe if e.get("exclude") is None]
    exc = [e for e in pe if e.get("exclude") is not None]
    res["n_edges_total"] = len(pe)
    res["n_edges_valid"] = len(val)
    res["n_edges_excluded"] = len(exc)
    res["exclude_kinds"] = dict(collections.Counter(e.get("exclude") for e in exc))
    res["criterion"] = r.get("criterion")
    res["gate_readings"] = r["baseline"].get("gate")

    # ── 82 条分原因 ──────────────────────────────────────────────────────
    def arr(sel, k):
        return [e.get(k) for e in sel]

    res["excluded_breakdown"] = dict(
        by_missing_control_side=dict(collections.Counter(
            ("plus_side_absent" if e.get("n_ctrl_plus", 0) == 0 else
             "minus_side_absent" if e.get("n_ctrl_minus", 0) == 0 else "other")
            for e in exc)),
        by_ctrl_side_selected=dict(collections.Counter(e.get("ctrl_side") for e in exc)),
        by_margin_px=dict(collections.Counter(str(e.get("margin_px")) for e in exc)),
        by_edge_name=dict(collections.Counter(e.get("edge") for e in exc)),
        by_edge_name_valid=dict(collections.Counter(e.get("edge") for e in val)),
        margin_none_count=int(sum(1 for e in exc if e.get("margin_px") is None)),
        margin_le_32_count=int(sum(1 for e in exc
                                  if e.get("margin_px") is not None and e["margin_px"] <= 32)),
        margin_le_16_count=int(sum(1 for e in exc
                                  if e.get("margin_px") is not None and e["margin_px"] <= 16)),
        seam_samples_min=int(min(e["n"] for e in exc)),
        seam_samples_max=int(max(e["n"] for e in exc)),
        n_distinct_frames=len(set(e.get("frame") for e in exc)),
    )

    # ── 「缺失不是零」：被排除边界的**已记录读数**（一侧对照口径，非判据）──
    er = np.array([abs(e["rel_step"]) for e in exc if e.get("rel_step") is not None], float)
    vr = np.array([abs(e["rel_step"]) for e in val if e.get("rel_step") is not None], float)
    res["excluded_recorded_rel_step"] = dict(
        n=int(er.size), median=float(np.median(er)), p90=float(np.percentile(er, 90)),
        max=float(er.max()),
        n_exceed_gate=int((er > GATE).sum()),
        n_exceed_ingate_max=int((er > float(np.abs(vr).max())).sum()),
        note="这些读数用**单侧可用**的 200 px 对照线口径算出，不是适用域内的判据值；"
             "登记为诊断，不得当作判据通过/不通过")
    res["valid_recorded_rel_step"] = dict(
        n=int(vr.size), max=float(vr.max()),
        n_exceed_gate=int((vr > GATE).sum()))

    # ── 多尺度可评估性：把 ctrl_shift 降到 margin_px 时是否两侧都有 >=20 样本 ──
    ladder = [200.0, 128.0, 64.0, 32.0, 16.0, 8.0, 4.0, 2.0, 0.0]
    evaluable = {}
    for shift in ladder:
        n = 0
        for e in pe:
            m = e.get("margin_px")
            if m is None:
                continue
            if m >= shift:
                n += 1
        evaluable[str(int(shift))] = n
    res["n_evaluable_by_ctrl_shift"] = evaluable
    res["n_evaluable_note"] = (
        "n_evaluable_by_ctrl_shift[N] = 若把对照线偏移从 200 px 降到 N px，"
        "两侧各有 >=20 个有效样本（即 margin_px >= N）的边界条数。"
        "N=200 给出 114（现判据覆盖）；N=32 给出全部 196 ⇒ 82 条被排除的边界"
        "**不是无观测**，而是固定 200 px 对照偏移超出局部足迹余量。"
        "在 N<200 上重算 rel_step 会改变对照基线（更接近边界的梯度估计），"
        "必须重跑生产门才能出判据值 ⇒ 本轮登记 NOT_RUN。")
    res["followup"] = dict(
        experiment_id="P5-D-boundary-coverage", status="NOT_RUN",
        reason="产品 FITS 已被回收（冻结记录 provenance_note 明示），无法在 N=32/16 上重算判据值",
        needed="重新跑 49 帧端到端并保存成品 FITS（或保留 union footprint 掩膜）")

    out = os.path.join(a.out, "S07_boundary_coverage.json")
    with open(out, "w") as f:
        json.dump(res, f, indent=1, ensure_ascii=False)
    print(json.dumps({k: v for k, v in res.items()
                      if k not in ("criterion",)}, indent=1, ensure_ascii=False)[:4000])
    print("wrote", out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
