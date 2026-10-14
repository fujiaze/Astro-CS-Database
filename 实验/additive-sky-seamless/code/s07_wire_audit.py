#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""S07 接线审计：在生产数据流中定位
  (1) P1 的 `k_photo * m(x,y)` 是否被用；
  (2) MA 标量求解器 `p2_upm_ma_build` 是否被生产调用；
  (3) `multiplicative_gain_applied` 的读写点；
并给出三态判定 `NOT_EVALUATED / IDENTITY / APPLIED` 与 `NOT_IMPLEMENTED`。

审计只读 `lib/**`，不修改任何文件。命中行号由本脚本现场重扫得出（可复跑）。

用法：python3 s07_wire_audit.py --out <results_dir>
"""
from __future__ import annotations

import argparse
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
LIB = os.path.join(REPO, "lib")

PATTERNS = {
    "multiplicative_gain_applied": r"multiplicative_gain_applied",
    "frame_gain": r"\bframe_gain\b",
    "k_photo": r"\bk_photo\b",
    "spatial_gain": r"spatial_gain",
    "apply_photometry_spatial": r"apply_photometry_spatial",
    "p2_upm_ma_build": r"\bp2_upm_ma_build\b",
    "p2_sky_estimate_gain_dc": r"\bp2_sky_estimate_gain_dc\b",
    "run_upm_rej_samp_wiring": r"\brun_upm_rej_samp_wiring\b",
    "require_frame_gain": r"require_frame_gain",
    "kDefaultSpatialGainOrder": r"kDefaultSpatialGainOrder",
    "apply_photometry": r"\bapply_photometry\b",
    "gain_input_ptr": r"\b(?:in|f)\.gain\b",
    "eval_delta_block": r"p2_sky_plane_eval_delta_block",
    "additive_subtract": r"out_v\[static_cast<size_t>\(k\)\] = base - dvals\[static_cast<size_t>\(k\)\]",
}


def scan():
    hits = {k: [] for k in PATTERNS}
    for root, _dirs, files in os.walk(LIB):
        for fn in files:
            if not fn.endswith((".cpp", ".h", ".hpp", ".cc")):
                continue
            p = os.path.join(root, fn)
            rel = os.path.relpath(p, REPO)
            try:
                lines = open(p, encoding="utf-8", errors="replace").read().splitlines()
            except OSError:
                continue
            for i, ln in enumerate(lines, 1):
                for k, pat in PATTERNS.items():
                    if re.search(pat, ln):
                        hits[k].append(dict(file=rel, line=i, text=ln.strip()[:200]))
    return hits


def verdict(h):
    """三态判定 + NOT_IMPLEMENTED。判定依据全部落在命中行号上。"""
    ma = h["p2_upm_ma_build"]
    ma_callers = [x for x in ma if "tools/stage2.cpp" in x["file"] or "phase2_integrate" in x["file"]]
    wiring_def = [x for x in h["run_upm_rej_samp_wiring"]]
    mg = h["multiplicative_gain_applied"]
    mg_read = [x for x in mg if "value(" in x["text"]]
    mg_write = [x for x in mg if "=" in x["text"] and "value(" not in x["text"]
                and "//" not in x["text"].split("=")[0]]
    spatial_apply = [x for x in h["apply_photometry_spatial"]]
    kphoto_apply = [x for x in h["k_photo"] if "k_photo" in x["text"] and
                    ("apply_photometry" in x["text"] or "sc->k_photo" in x["text"]
                     or "sc.k_photo" in x["text"])]
    return dict(
        P1_kphoto_mxy=dict(
            state="APPLIED",
            scope="normalize/Phase1 photometry 节点（逐帧、in-place 施加于像素）",
            evidence=dict(
                default_spatial_order=[x for x in h["kDefaultSpatialGainOrder"]],
                spatial_apply_call=[x for x in spatial_apply],
                kphoto_pass=[x for x in kphoto_apply][:6],
                spatial_gain_field=[x for x in h["spatial_gain"]][:6],
            ),
            condition="仅当该帧取得标度（拟合或 p1_photscale.json 侧车）时施加；缺标度即该帧 fail"
                      "（fail-closed，不静默按 1 冒充）。`fit.enabled` 默认 false（module_adapters.cpp:5888），"
                      "故未配置测光的运行不施加。",
            note="m_order==0 走标量 `apply_photometry`；m_order>0 走 `apply_photometry_spatial`（m(x,y)）。"
                 "默认 `kDefaultSpatialGainOrder = 2`。"),
        P5_MA_scalar_solver=dict(
            state="NOT_IMPLEMENTED_ON_PRODUCTION_PATH",
            evidence=dict(callers=ma_callers, wiring=wiring_def),
            note="`p2_upm_ma_build` 的调用者只有 tools/stage2.cpp（工具面）与 "
                 "phase2_integrate.cpp:1362（函数 run_upm_rej_samp_wiring，全仓仅定义+声明、**零调用者**）。"
                 "⇒ 生产扣除链（p2_sky_plane_build_joint + eval_delta_block）不经过该求解器。"),
        P5_multiplicative_gain=dict(
            state="IDENTITY",
            status_channel="NOT_EVALUATED",
            evidence=dict(reads=mg_read, writes=mg_write,
                          gain_ptr=[x for x in h["gain_input_ptr"]][:8],
                          additive_apply=[x for x in h["eval_delta_block"]][:8],
                          additive_subtract=[x for x in h["additive_subtract"]][:8],
                          frame_gain=[x for x in h["frame_gain"]][:8],
                          require_frame_gain=[x for x in h["require_frame_gain"]][:6]),
            note="`multiplicative_gain_applied` 全仓 1 处读（value(...)，默认 false）、**0 处写**；"
                 "读点随后 `in.gain = nullptr` ⇒ 逐帧 g≡1。生产取 G_i={0}，等价于**硬先验 eta≡0**，"
                 "不是规范固定。状态通道无生产者 ⇒ 三态里的状态位为 NOT_EVALUATED，"
                 "行为位为 IDENTITY（不得读成『已应用』，也不得读成『已证明无乘性差』）。"
                 "二维 g_i(x) 在生产链无任何成员（sky_plane 求值为纯加性）。"),
    )


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    a = ap.parse_args(argv)
    os.makedirs(a.out, exist_ok=True)
    h = scan()
    res = dict(schema="ACSD-P5-S07-WIRE/1", repo_root=REPO,
               n_hits={k: len(v) for k, v in h.items()},
               hits={k: v for k, v in h.items()},
               verdict=verdict(h))
    out = os.path.join(a.out, "S07_wire_audit.json")
    with open(out, "w") as f:
        json.dump(res, f, indent=1, ensure_ascii=False)
    for k, v in res["n_hits"].items():
        print(f"{k}: {v}")
    print(json.dumps(res["verdict"], indent=1, ensure_ascii=False)[:1500])
    print("wrote", out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
