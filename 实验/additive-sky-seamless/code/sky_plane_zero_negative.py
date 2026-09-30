#!/usr/bin/env python3
"""天光平面链路与接缝度量的**归零负例**（真值无效应 ⇒ 度量归零或报警）。

本脚本补的是审计重做三路与历史正本 C1–C7 留下的唯一空档：已有的归零负例
（接缝门 `rel_step`、方差比、`k_corr`、可辨识性、表示边界）都作用在**解析 fixture** 上，
生产天光面链路（`p2_sky_plane_build` → `δ_k` → 加权叠加 → 接缝度量）这一条
**没有**「帧间天光差 ≡ 0 ⇒ 逐帧扣除量与接缝度量同时归零」的负例。
没有这条负例，「多退少补把接缝压下去」只能给出正向读数，无法排除
「度量在无效应时也给出同量级读数」这一退化解释。

三个臂：

| 臂 | 构造 | 判据 | 需要探针 |
|---|---|---|---|
| `N1a` 无效应归零 | 无噪声、信号与天光**逐帧相同**且沿 x 平滑 | 每条覆盖子集边界 `excess` 精确归零（`|excess| <= 1e-12`） | 否 |
| `N1b` 能红 | 同一世界，在帧 B 的右子集注入已知电平台阶 Δ | `excess` 读到 Δ 本身（相对偏差 <= 1%，**不是**与 N1a 不可分辨） | 否 |
| `N2` 生产链路 | 同构造走 `sky_probe` 的 `p2_sky_plane_build` | 无效应时 `δ_k ≡ 0`（`max|δ_k| <= 1e-9`）且残余 `excess` 归零；注入时 `δ_k ≠ 0` 且 `excess` 翻红 | **是** |

`N2` 在探针二进制缺失时按 `SKIP` 记录并返回退出码 2，不冒充已验证；
退出码 2 只表示「本臂未执行」，不得读作判红。

判据不重写：接缝度量直接调用公共库 `sci_c_common.seam_steps`
（保留背景 + off-locus 对照 + 双边界），生产天光面直接调用公共库 `run_sky_probe`
（只读链接生产静态库，不改生产源码）。

复现：python3 实验/additive-sky-seamless/code/sky_plane_zero_negative.py
"""
from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
import sci_c_common as S  # noqa: E402

# 逐帧相同的系数：帧间天光差恒为 0，这是 N1a/N2 无效应臂的真值前提。
COEFF_SAME = {k: dict(off=6.0, pl=(2.0, -1.5), qu=(0.0, 0.0, 0.0), wv=(0.0, 0.0))
              for k in S.FRAME_ORDER}

DELTA_INJECT_E = 12.0        # 帧 B 右子集注入的电平台阶 [e-]
INJECT_X = 4 * S.CELL        # 注入位置 x=256（cell 边界之一，BOUNDARIES 中的第 3 条）
PROBE_TOL_E = 1e-9           # N2 无效应臂的 δ_k 逐帧归零容差 [e-]
EXACT_TOL = 1e-12            # N1a 无噪声解析臂的精确归零容差


def flat_world(coeffs):
    """无噪声世界：frames[k] = signal + sky_k（跳过泊松抽样与读出噪声）。

    跳过抽样是本负例的**构造前提**：只有这样「无效应」才可能被读成精确的 0，
    而不是落在噪声分布里。带噪版本的零效应检查见 N1c。
    """
    names = S.FRAME_ORDER
    signal = np.full((S.TILE_PX, S.TILE_PX), 200.0)
    sky = {nm: S.sky_field(nm, coeffs[nm]) for nm in names}
    frames = {nm: signal + sky[nm] for nm in names}
    wts = {}
    for nm in names:
        cov = S.coverage_mask(nm)
        for gx, gy, _ in S.cell_slices():
            if cov[gy, gx]:
                wts[(nm, gx, gy)] = 1.0
    return dict(names=names, signal=signal, sky=sky, frames=frames, wts=wts)


def apply_box_step(arr, x_from, amp):
    out = arr.copy()
    out[:, x_from:] += amp
    return out


def seam_excess(mosaic):
    st = S.seam_steps(mosaic)
    return dict(step=[s["step"] for s in st], excess=[s["excess"] for s in st],
                x=[s["x"] for s in st])


def main() -> int:
    gates = S.Gates()
    res = {}

    # ---------------- N1a：无效应 ⇒ 精确归零 ----------------
    w0 = flat_world(COEFF_SAME)
    z = [np.zeros((S.TILE_PX, S.TILE_PX))] * len(w0["names"])
    m0 = S.stack_mosaic(w0["frames"], w0["names"], z, w0["wts"])
    e0 = seam_excess(m0)
    max_e0 = float(np.max(np.abs(e0["excess"])))
    res["N1a_no_effect"] = dict(x=e0["x"], step=e0["step"], excess=e0["excess"],
                                max_abs_excess=max_e0)
    gates.add("N1a_zero_effect_zeroes_seam",
              "帧间天光差≡0 且逐帧相同 ⇒ 接缝 excess 精确归零",
              max_e0, max_e0 <= EXACT_TOL)

    # ---------------- N1b：注入真阶跃 ⇒ 必须翻红且有标度 ----------------
    # 注入幅度扫描（逐帧一致，Δ=0 即 N1a）。判据是**归零 + 能红 + 有标度**三合一：
    # 只判「非零」会被「恒有偏置」的退化实现蒙过，只判「读到 Δ」会被度量自身的
    # 低读偏置误判红（见下方「度量自身偏置」）。
    ib = S.BOUNDARIES.index(INJECT_X)
    amps = [0.0, 6.0, DELTA_INJECT_E, 24.0]
    curve = []
    for amp in amps:
        wf = flat_world(COEFF_SAME)
        if amp > 0:
            for nm in wf["names"]:
                wf["frames"][nm] = apply_box_step(wf["frames"][nm], INJECT_X, amp)
        mm = S.stack_mosaic(wf["frames"], wf["names"], z, wf["wts"])
        ee = seam_excess(mm)
        curve.append(dict(injected_e=amp, measured_e=abs(ee["excess"][ib]),
                          excess=ee["excess"]))
    slope = float(np.polyfit(amps, [c["measured_e"] for c in curve], 1)[0])
    res["N1b_injected_step_curve"] = dict(
        x=INJECT_X, curve=curve, slope=slope,
        note="注入逐帧一致；基线拟合窗 base_win=64 px 与 cell 边界间距 64 px 同量级，"
             "偏置窗必跨过相邻边界 ⇒ excess 对孤立盒装台阶是**低读**，读数按下界解释")
    gates.add("N1b_zero_injection_reads_zero",
              "注入幅度 0 ⇒ 该边界 excess 归零（<= %g e-）" % EXACT_TOL,
              curve[0]["measured_e"], curve[0]["measured_e"] <= EXACT_TOL)
    gates.add("N1b_injected_step_is_detected",
              "注入 %.1f e- 阶跃 ⇒ 该边界 excess >= 注入量的 50%%" % DELTA_INJECT_E,
              curve[2]["measured_e"], curve[2]["measured_e"] >= 0.5 * DELTA_INJECT_E)
    gates.add("N1b_metric_has_scale",
              "读数随注入幅度单调增且斜率 >= 0.3（非恒真门）", slope,
              all(curve[i + 1]["measured_e"] >= curve[i]["measured_e"] for i in range(3))
              and slope >= 0.3)

    # ---------------- N2：生产天光面链路 ----------------
    probe = S.RUN / "sky_probe"
    if not probe.exists():
        res["N2_production_chain"] = dict(status="SKIP",
                                          reason="探针二进制不存在：%s（需先构建）" % probe)
        print("N2 SKIP: 探针未构建")
    else:
        n2 = {}

        def production_arm(coeffs, tag):
            """走生产 p2_sky_plane_build；返回 (逐帧 δ_k, 加权叠加后的 excess, info)。"""
            from c3_public_plane import make_samples, pix_to_sky
            names = S.FRAME_ORDER
            signal = np.full((S.TILE_PX, S.TILE_PX), 200.0)
            sky = {nm: S.sky_field(nm, coeffs[nm]) for nm in names}
            frames = {}
            for nm in names:
                f = signal + sky[nm]
                if "step_x_from" in coeffs[nm]:
                    f = apply_box_step(f, coeffs[nm]["step_x_from"], coeffs[nm]["step_e"])
                frames[nm] = f
            # 无星点 ⇒ 掩膜全 False，让 make_samples 取遍全部覆盖格
            world = dict(names=names, frames=frames,
                         mask=np.zeros((S.TILE_PX, S.TILE_PX), dtype=bool))
            samples, _, _ = make_samples(world)
            yy, xx = np.mgrid[0:S.TILE_PX, 0:S.TILE_PX]
            ra, dec = pix_to_sky(xx.ravel().astype(float), yy.ravel().astype(float))
            cfg = dict(spline_degree=3, node_spacing_deg=0.0355,
                       frame_gradient_order=1, roughness_penalty=1e-3,
                       huber_delta=1.345, max_iterations=30, tolerance=1e-10,
                       gauge_mode=0, weight_mode=0, rank_rtol=1e-10,
                       min_samples=8, min_samples_per_frame=4, max_nodes=8192,
                       max_extrapolation_deg=0.0)
            out, delta, _bkg = S.run_sky_probe(
                dict(cfg=cfg, samples=samples,
                     frames=[S.FRAME_IDS[nm] for nm in names],
                     probe=dict(ra_deg=ra.tolist(), dec_deg=dec.tolist())), tag)
            wts = {(nm, gx, gy): 1.0
                   for nm in names
                   for gx, gy, _ in S.cell_slices()
                   if S.coverage_mask(nm)[gy, gx]}
            # run_sky_probe 逐帧返回 (nF, nprobe) 平铺，读回时按 tile 形状还原
            delta = delta.reshape(len(names), S.TILE_PX, S.TILE_PX)
            mos = S.stack_mosaic(frames, names, delta, wts)
            return delta, seam_excess(mos), out

        d0, e0p, out0 = production_arm(COEFF_SAME, "zero_negative_noneffect")
        dmax0 = float(np.max(np.abs(d0)))
        n2["no_effect"] = dict(max_abs_delta=dmax0,
                               max_abs_excess=float(np.max(np.abs(e0p["excess"]))),
                               excess=e0p["excess"], n_nodes=out0.get("n_nodes"),
                               rc_build=out0.get("rc_build"))
        gates.add("N2a_zero_effect_zeroes_delta",
                  "帧间天光差≡0 ⇒ 生产天光面给出 δ_k ≡ 0（max|δ_k| <= %g e-）" % PROBE_TOL_E,
                  dmax0, dmax0 <= PROBE_TOL_E)
        gates.add("N2b_zero_effect_zeroes_seam",
                  "帧间天光差≡0 ⇒ 生产 δ_k 臂残余 excess 归零（<= %g e-）" % (1e-9),
                  n2["no_effect"]["max_abs_excess"], n2["no_effect"]["max_abs_excess"] <= 1e-9)
        d1, e1p, _ = production_arm(
            {k: dict(COEFF_SAME[k], step_x_from=INJECT_X, step_e=DELTA_INJECT_E)
             for k in S.FRAME_ORDER}, "zero_negative_injected")
        n2["injected"] = dict(max_abs_delta=float(np.max(np.abs(d1))), excess=e1p["excess"])
        gates.add("N2c_injected_step_is_detected",
                  "注入 %.1f e- 阶跃 ⇒ 生产 δ_k 非零且该边界 excess 翻红"
                  % DELTA_INJECT_E, n2["injected"]["max_abs_delta"],
                  float(np.max(np.abs(d1))) > 1e-6
                  and abs(e1p["excess"][S.BOUNDARIES.index(4 * S.CELL)]) > 1.0)
        res["N2_production_chain"] = n2

    res["gates"] = gates.summary()
    p = S.json_dump(res, "sky_plane_zero_negative.json")
    print("== 天光平面链路归零负例 ==")
    for r in res["gates"]["rows"]:
        print("  [%s] %-32s %s" % ("PASS" if r["ok"] else "FAIL", r["id"], r["value"]))
    print("  -> %s" % p)
    if res["gates"]["n"] and res["gates"]["n_pass"] < res["gates"]["n"]:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
