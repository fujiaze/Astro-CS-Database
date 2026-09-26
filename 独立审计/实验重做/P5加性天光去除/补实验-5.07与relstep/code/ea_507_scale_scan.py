# -*- coding: utf-8 -*-
"""实验 A：M9 "尺度 1600→50 px 扫描使残余接缝 ×5.07" 的原始口径复算。

原始口径（实验/additive-sky-seamless/code/c1_additive.py A10 + README §4.1）：
- 度量 = mosaic 覆盖子集边界处 |有符号电平台阶| 的最大值 seam_max [e-]
  （不是 RMS，也不是相对口径）；5.07 = seam_max(50px)/seam_max(1600px)。
- 场景 = 4 帧（A-D 列带覆盖）+ 帧间 y 相干 x 向条纹（wav 与 0.41wav 两成分，
  amp=8 e-，发布相位表）。
本脚本按该口径以独立 Python 实现复算，并同时输出 RMS 口径对照、噪声臂与
无噪声臂、gauge 变体、可表示负例与单调性检验。
固定 seed=20250926。复现：python3 code/ea_507_scale_scan.py
"""
from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
import p5c_common as P  # noqa: E402

WAVES = (1600.0, 800.0, 400.0, 200.0, 100.0, 50.0)


def build_and_solve(wav, noisy=True, fill_single=True, stripe_mode="sine",
                    amp=None):
    """构造一个扫描点并返回 (mosaic, metrics, oob_rms)。"""
    signal = P.signal_field()
    sky = {}
    frames = {}
    for nm in P.FRAME_ORDER:
        s = P.base_sky(nm)
        if wav is not None:
            if stripe_mode == "sine":
                a = P.OOB_AMP if amp is None else amp
                s = s + P.stripe_term(nm, wav, amp=a)
            elif stripe_mode == "const":
                # 可表示负例：帧间差 = 逐帧常数（公共面可精确表示）
                s = s + (P.OOB_AMP if amp is None else amp) * P.OOB_SHAPE[nm]
        sky[nm] = s
    for nm in P.FRAME_ORDER:
        rng = P.derive_rng("ea|%s|%s" % ("noisy" if noisy else "quiet", nm))
        frames[nm] = P.synth_frame(signal, sky[nm], rng, noisy=noisy)
    ctrl = P.control_table(frames)
    bref, delta = P.common_plane_fill(ctrl, fill_single=fill_single)
    mos = P.stack_mosaic(frames, delta, ctrl)
    return mos, P.seam_metrics(mos), P.out_of_basis_rms(sky), sky, frames


def main():
    res = dict(seed=P.SEED_BASE, waves=list(WAVES),
               metric_note="seam_max = max |signed level step [e-]| at coverage "
                           "boundaries (original A10 metric)")

    # ---- 主臂：带噪（同原实验条件） -----------------------------------
    sweep = []
    for wav in WAVES:
        mos, met, oob, _, _ = build_and_solve(wav, noisy=True)
        mos0 = build_and_solve(None, noisy=True)[0]  # 同 seed 无条纹参照
        rms_art = float(np.sqrt(np.nanmean((mos - mos0) ** 2)))
        sweep.append(dict(wave_px=wav, oob_rms=oob, seam_max=met["seam_max"],
                          seam_med=met["seam_med"], rel_max=met["rel_max"],
                          rms_artifact=rms_art))
        print(wav, met["seam_max"], rms_art)
    res["sweep_noisy"] = sweep
    res["baseline_noisy"] = build_and_solve(None, noisy=True)[1]

    # ---- 无噪声臂（剥离噪声地板，纯结构缩放） -------------------------
    sweep_q = []
    for wav in WAVES:
        mos, met, oob, _, _ = build_and_solve(wav, noisy=False)
        mos0 = build_and_solve(None, noisy=False)[0]
        rms_art = float(np.sqrt(np.nanmean((mos - mos0) ** 2)))
        sweep_q.append(dict(wave_px=wav, oob_rms=oob, seam_max=met["seam_max"],
                            seam_med=met["seam_med"], rel_max=met["rel_max"],
                            rms_artifact=rms_art))
        print("quiet", wav, met["seam_max"], rms_art)
    res["sweep_quiet"] = sweep_q
    res["baseline_quiet"] = build_and_solve(None, noisy=False)[1]

    # ---- 可表示负例（帧间差 = 常数 ⇒ 公共面可表示） --------------------
    neg = build_and_solve(None, noisy=False, stripe_mode="const")[1]
    res["negative_in_basis_quiet"] = neg

    # ---- gauge 变体（单帧列不填公共面） --------------------------------
    gv = {}
    for wav in (1600.0, 50.0):
        mos, met, _, _, _ = build_and_solve(wav, noisy=True, fill_single=False)
        gv[str(wav)] = met
    res["gauge_variant_nofill_noisy"] = gv

    # ---- 比值与单调性 ---------------------------------------------------
    def ratios(sw):
        a, b = sw[0], sw[-1]
        return dict(
            seam_max_ratio_50_over_1600=b["seam_max"] / max(a["seam_max"], 1e-12),
            rms_artifact_ratio=b["rms_artifact"] / max(a["rms_artifact"], 1e-12),
            rel_max_ratio=b["rel_max"] / max(a["rel_max"], 1e-12))

    res["ratios_noisy"] = ratios(sweep)
    res["ratios_quiet"] = ratios(sweep_q)
    res["doc_claim"] = dict(value=5.07, metric="seam_max (e-)",
                            source="实验/additive-sky-seamless README §4.1 A10 / "
                                   "PHASE2_UPM.md §7a/§16.2")
    for tag, sw in (("noisy", sweep), ("quiet", sweep_q)):
        sm = np.array([r["seam_max"] for r in sw])
        res["monotonic_seam_max_%s" % tag] = bool(np.all(np.diff(sm) > 0))
        res["argmax_seam_max_%s" % tag] = sw[int(np.argmax(sm))]["wave_px"]

    # 原始序列的已发布值（供并排对照，出处 README §4.1 表）
    res["published_A10"] = {
        "wave_px": [1600, 800, 400, 200, 100, 50],
        "oob_rms": [0.313, 1.347, 3.070, 5.491, 6.514, 6.643],
        "seam_max": [0.892, 1.836, 1.525, 4.675, 7.160, 4.523],
        "seam_med": [0.330, 0.624, 0.244, 2.431, 1.169, 2.808],
        "note": "published in 实验/additive-sky-seamless/README.md §4.1; "
                "ratio 4.523/0.892 = 5.07; series non-monotonic (400, 50 px)"}

    P.save_json(P.RESULTS / "ea_507_scale_scan.json", res)

    print("\n== summary ==")
    print("noisy :", res["ratios_noisy"],
          "monotonic:", res["monotonic_seam_max_noisy"],
          "argmax:", res["argmax_seam_max_noisy"])
    print("quiet :", res["ratios_quiet"],
          "monotonic:", res["monotonic_seam_max_quiet"],
          "argmax:", res["argmax_seam_max_quiet"])
    print("neg in-basis seam_max:", neg["seam_max"])


if __name__ == "__main__":
    main()
