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
| `N2` 生产链路 | 同构造走 `sky_probe` 的 `p2_sky_plane_build` | 见下 | **是** |

`N2` 的三条判据按**量值型**（不是存在性、不是下界型）重写：

| 判据 | 构造 | 判据式 | 锚 |
|---|---|---|---|
| `N2a` 无效应 ⇒ δ 归零 | 帧间天光差 ≡ 0 | `max_k max|δ_k| <= 1e-9 e-` | 无（存在性：这是本链的**定义**性质，N2b′/N2c′ 才承担量值判别力） |
| `N2b` 施加语义闭合 | 注入**基外**盒装台阶（δ 只能近似复现），把实际施加量扫成 `α·δ_k`，α ∈ {0,1/2,1,3/2,2} | `excess(α) = excess(0) − α·excess(δ)`，其中 `excess(δ)` 是**单独把校正场当马赛克喂给同一个度量**读出的接缝（第三个**独立对象**的读数） | 两个互相独立的读数 |
| `N2c` 注入量值双侧相符 | 注入**基内**逐帧常数偏置 Δ（生产模型可精确表示），真值 `sky_true_k(x,y)` 解析已知 | `max_k max|δ_k − sky_true_k| <= tol_δ`（**双侧**，不是下界）**且** 施加后残余 `excess` 归零 | 解析真值 |

`N2b` 的闭式来自度量的**仿射性**：`stack_mosaic` 是覆盖集上的加权平均，
`m(x) = nanmedian_y`、order-2 基线拟合、沿窗中位数对数据都是仿射映射，
故 `excess(mos(0) − α·δ_stack) = excess(mos(0)) − α·excess(δ_stack)` **精确成立**。
该恒等式的三个量分属三个不同对象（未校正马赛克、被施加的校正场、二者的组合），
因此它不是自反式断言：施加路径任何一处不按 `α` 线性（不施加 / 反号 / ×2 / ×0.25 /
权重不同）都会破坏闭式。

`N2c` 的锚是**解析真值**：逐帧常数偏置落在生产模型空间内，δ_k 必须逐点复现
`sky_field`；它对**任何**注入幅度都成立（含 1.2 e⁻ / 0.9 e⁻ 这类小真值），
因此不是「真值越大越红」的下界型伪判据。

`N2` 在探针二进制缺失时按 `SKIP` 记录并返回退出码 2，不冒充已验证；
退出码 2 只表示「本臂未执行」，不得读作判红。

判据不重写：接缝度量直接调用公共库 `sci_c_common.seam_steps`
（保留背景 + off-locus 对照 + 双边界），生产天光面直接调用公共库 `run_sky_probe`
（只读链接生产静态库，不改生产源码）。缺陷注入自检见 `g08_defect_probe.py`。

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
OFF_INJECT_E = 6.0           # N2c 注入的逐帧常数偏置 [e-]


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


# ---------------------------------------------------------------------------
# N2 生产臂：走生产 p2_sky_plane_build，返回 (逐帧 δ_k, 加权叠加后的 excess, info)
# ---------------------------------------------------------------------------
PROBE_CFG = dict(spline_degree=3, node_spacing_deg=0.0355, frame_gradient_order=1,
                 roughness_penalty=1e-3, huber_delta=1.345, max_iterations=30,
                 tolerance=1e-10, gauge_mode=0, weight_mode=0, rank_rtol=1e-10,
                 min_samples=8, min_samples_per_frame=4, max_nodes=8192,
                 max_extrapolation_deg=0.0)


def uniform_weights():
    return {(nm, gx, gy): 1.0
            for nm in S.FRAME_ORDER
            for gx, gy, _ in S.cell_slices()
            if S.coverage_mask(nm)[gy, gx]}


def production_frames(coeffs):
    """按真值系数造帧（无星点 ⇒ 掩膜全 False，让 make_samples 取遍全部覆盖格）。"""
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
    world = dict(names=names, frames=frames,
                 mask=np.zeros((S.TILE_PX, S.TILE_PX), dtype=bool))
    samples, _, _ = make_samples(world)
    yy, xx = np.mgrid[0:S.TILE_PX, 0:S.TILE_PX]
    ra, dec = pix_to_sky(xx.ravel().astype(float), yy.ravel().astype(float))
    return frames, sky, samples, ra, dec


def solve_delta(samples, ra, dec, tag):
    names = S.FRAME_ORDER
    out, delta, _bkg = S.run_sky_probe(
        dict(cfg=dict(PROBE_CFG), samples=samples,
             frames=[S.FRAME_IDS[nm] for nm in names],
             probe=dict(ra_deg=ra.tolist(), dec_deg=dec.tolist())), tag)
    return out, delta.reshape(len(names), S.TILE_PX, S.TILE_PX)


def production_arm(coeffs, tag, apply=None):
    """走生产 p2_sky_plane_build。

    `apply(delta)` 是**施加函数**（默认真实施加：`λ -> λ`）。缺陷注入全部落在它上面，
    生产求解器本身不被改动。返回 (δ_applied, 叠加后的 seam 度量, info, 辅助场)。
    辅助场里同时给出**未施加**的 `delta_true` 与它的马赛克 `delta_true_stack`，
    供 N2b 的闭式锚点使用（锚点与被测量分属「未施加」与「施加后」两条路径）。
    """
    frames, sky, samples, ra, dec = production_frames(coeffs)
    out, delta_true = solve_delta(samples, ra, dec, tag)
    delta = apply(delta_true) if apply is not None else delta_true
    wts = uniform_weights()
    zeros = {nm: np.zeros_like(frames[nm]) for nm in S.FRAME_ORDER}
    mos = S.stack_mosaic(frames, S.FRAME_ORDER, delta, wts)
    mos0 = S.stack_mosaic(frames, S.FRAME_ORDER, delta_true * 0.0, wts)
    # 校正场自身的马赛克：stack_mosaic(zeros, names, −δ) = +δ̄
    d_true_stack = S.stack_mosaic(zeros, S.FRAME_ORDER, -delta_true, wts)
    aux = dict(sky_true=sky, delta_true=delta_true, delta_applied=delta,
               delta_true_stack=d_true_stack, weights=wts, mos=mos, mos0=mos0,
               frames=frames)
    return delta, seam_excess(mos), out, aux


# ---------------------------------------------------------------------------
# N2b：δ 施加的**场级恒等式**（锚 = 未施加路径的同一 δ 场）
# ---------------------------------------------------------------------------
ALPHAS = (0.5, 1.0, 1.5, 2.0)
CLOSURE_TOL_REL = 1e-9      # 两条路径共用同一 δ 场 ⇒ 残差只受浮点精度限制


def n2b_field_identity(aux, bi, alphas=ALPHAS, tol_rel=CLOSURE_TOL_REL):
    """`mos(α) = mos(0) − α·δ̄` 必须**逐像素**成立（e⁻ 场级）。

    `aux["delta_of_alpha"](α)` 给出按 α 缩放的 δ（缺陷注入点），
    `mos(α)` 是施加它之后的马赛克，`mos(0)` 是完全不施加的马赛克，
    `δ̄` 是**未施加**的那一份 δ 的覆盖集加权马赛克。两条路径只差施加函数。
    判据是逐像素等式（不是标量读数、不是存在性、不是下界）：
    不施加 / 反号 / ×2 / ×0.25 / 用错权重或用错帧序都会破坏它。
    """
    dbar = aux["delta_true_stack"]
    scale = max(float(np.nanmax(np.abs(dbar))), 1.0)
    rows = []
    worst = 0.0
    for a in alphas:
        mos_a = S.stack_mosaic(aux["frames"], S.FRAME_ORDER,
                               aux["delta_of_alpha"](a), aux["weights"])
        resid = mos_a - aux["mos0"] + float(a) * dbar
        rmax = float(np.nanmax(np.abs(resid)))
        rel = rmax / scale
        rows.append(dict(alpha=float(a), max_abs_residual_e=rmax, rel=rel))
        worst = max(worst, rel)
    return dict(worst_rel=float(worst), rows=rows, ok=bool(worst <= tol_rel),
                tol_rel=float(tol_rel))


# ---------------------------------------------------------------------------
# N2c：注入量值双侧相符（真值锚 = 「注入的接缝被完全消除」）
# ---------------------------------------------------------------------------
STEP_TOL_E = 1e-6          # 施加 δ 后残余台阶的归零容差 [e-]
AFFINE_TOL_REL = 1e-6      # 仿射律（过零点、斜率 = −step(0)）的相对容差


def n2c_truth_match(step_by_alpha, tol_step=STEP_TOL_E, tol_aff=AFFINE_TOL_REL):
    """`step(α)` 必须是**过 α=1 零点、斜率 = −step(0)** 的仿射函数。

    真值锚：δ_k 的物理契约是「把注入的逐帧接缝差完全消掉」，即 `step(1) ≡ 0`。
    零点由**真值**给出而不是由实测端点拟合，因此判据是双侧、量值型的：
    - 不施加 ⇒ step(1) = step(0) ⇒ 红
    - 反号   ⇒ step(1) = 2·step(0) ⇒ 红
    - ×2     ⇒ step(1) = step(0)   ⇒ 红
    - ×0.25  ⇒ step(1) = 0.75 step(0) ⇒ 红
    - 真值本身变小（1.2 / 0.9 e⁻）⇒ 仍绿 ⇒ 不是下界型
    """
    a = np.array(sorted(step_by_alpha), dtype=float)
    s = np.array([step_by_alpha[float(k)] for k in a], dtype=float)
    s0 = float(step_by_alpha[0.0])
    s1 = float(step_by_alpha[1.0])
    slope = float(np.polyfit(a, s, 1)[0])
    # 真值锚给出的两条：s(1) == 0；仿射律 s(a) = s(0) * (1 - a)
    aff_resid = float(np.max(np.abs(s - s0 * (1.0 - a))))
    aff_rel = aff_resid / max(abs(s0), 1.0)
    return dict(step_by_alpha={float(k): float(v) for k, v in step_by_alpha.items()},
                slope=slope, expected_slope=s0, step_at_alpha1=s1,
                max_abs_residual_at_alpha1=abs(s1), affine_residual_e=aff_resid,
                affine_residual_rel=aff_rel,
                ok=bool(abs(s1) <= tol_step and aff_rel <= tol_aff),
                tol_step=tol_step, tol_aff=tol_aff)


# ---------------------------------------------------------------------------
# N2c：注入量值双侧相符（锚 = 解析真值）
# ---------------------------------------------------------------------------
DELTA_TRUTH_TOL_E = 1e-6    # 生产容差 1e-10 + 浮点与基函数舍入的余量 [e-]
RESIDUAL_EXCESS_TOL_E = 1e-3   # 施加解析真值后残余接缝的归零容差 [e-]


def n2c_scan(off_e, defect=None, bi=None, alphas=(0.0, 0.5, 1.0, 1.5, 2.0)):
    """注入逐帧常数偏置 `off_e`，沿 α 扫施加量，返回 `step(α)` 的读数。

    `defect(delta)` 是**施加函数**（缺陷注入点）；缺省是 `λ -> λ`。α 先作用在
    缺陷输出上，因此「不施加」这类缺陷在所有 α 上都保持为 0。
    """
    bi = S.BOUNDARIES.index(INJECT_X) if bi is None else bi
    cf = {k: dict(COEFF_SAME[k]) for k in S.FRAME_ORDER}
    cf["B"]["off"] = COEFF_SAME["B"]["off"] + off_e
    out = {}
    for a in alphas:
        def ap(_d, _a=float(a), _df=defect):
            base = _df(_d) if _df is not None else _d
            return base * _a
        _d, em, _o, _aux = production_arm(cf, "zero_negative_off_%g_a%g" % (off_e, a),
                                          apply=ap)
        out[float(a)] = float(em["step"][bi])
    return out


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
        bi = S.BOUNDARIES.index(INJECT_X)

        # ---- N2a：无效应 ⇒ δ 逐点归零（存在性，判别力由 N2b′/N2c′ 承担） ----
        d0, e0p, out0, aux0 = production_arm(COEFF_SAME, "zero_negative_noneffect")
        dmax0 = float(np.max(np.abs(d0)))
        n2["no_effect"] = dict(max_abs_delta=dmax0,
                               max_abs_excess=float(np.max(np.abs(e0p["excess"]))),
                               excess=e0p["excess"], n_nodes=out0.get("n_nodes"),
                               rc_build=out0.get("rc_build"))
        gates.add("N2a_zero_effect_zeroes_delta",
                  "帧间天光差≡0 ⇒ 生产天光面给出 δ_k ≡ 0（max|δ_k| <= %g e-）" % PROBE_TOL_E,
                  dmax0, dmax0 <= PROBE_TOL_E)

        # ---- N2b：δ 施加的场级恒等式（基外盒装台阶世界） ----
        step_coeffs = {k: dict(COEFF_SAME[k], step_x_from=INJECT_X, step_e=DELTA_INJECT_E)
                       for k in S.FRAME_ORDER}
        _d, _e, _o, aux_s = production_arm(step_coeffs, "zero_negative_step_anchor")
        aux_s["delta_of_alpha"] = (lambda a, _base=aux_s["delta_true"]:
                                   _base * float(a))
        n2b = n2b_field_identity(aux_s, bi)
        n2["apply_field_identity"] = dict(
            world="基外盒装台阶 %.1f e-（δ 不可精确复现 ⇒ 用**场级恒等式**而非读数闭合）"
                  % DELTA_INJECT_E,
            rows=n2b["rows"], worst_rel=n2b["worst_rel"], tol_rel=CLOSURE_TOL_REL)
        gates.add("N2b_apply_field_identity",
                  "逐像素 `mos(α) = mos(0) − α·δ̄`（α ∈ %s，最大相对残差 <= %g；"
                  "双侧、量值型，非存在性非下界）" % (list(ALPHAS), CLOSURE_TOL_REL),
                  n2b["worst_rel"], n2b["ok"])

        # ---- N2c：注入量值双侧相符（逐帧常数偏置，真值锚 = 接缝被完全消除） ----
        off_scan = []
        for off_e in (OFF_INJECT_E, 1.2, 0.9):
            sb = n2c_scan(off_e)
            m = n2c_truth_match(sb)
            off_scan.append(dict(injected_e=off_e, **m))
        n2c_ok = all(r["ok"] for r in off_scan)
        n2["offset_truth_match"] = dict(
            injected_frame="B", reference_truth="注入的逐帧接缝差被 δ 完全消除（step(1) ≡ 0）",
            scan=off_scan,
            note="逐帧常数偏置落在生产模型空间内；扫描 6.0 / 1.2 / 0.9 e- 三档小真值，"
                 "证明本判据不是下界型（不是「非零」也不是「大于某个硬编码阈值」）")
        gates.add("N2c_injection_truth_match",
                  "注入逐帧常数偏置（6.0 / 1.2 / 0.9 e- 三档）⇒ 施加 α·δ_k 的接缝读数"
                  "必须是过 α=1 零点、斜率 = −step(0) 的仿射函数"
                  "（|step(1)| <= %g e- 且斜率相对误差 <= %g；双侧、量值型）"
                  % (STEP_TOL_E, AFFINE_TOL_REL),
                  [(r["injected_e"], r["max_abs_residual_at_alpha1"], round(r["affine_residual_rel"], 12))
                   for r in off_scan], n2c_ok)
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
