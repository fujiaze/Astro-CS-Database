# -*- coding: utf-8 -*-
"""实验 B：rel_step_max=0.1 的前提取证 + 标定实验（固定 seed=20250926）。

前提（本脚本以实验核验的量）：
- "rel_step_max=0.1" 在仓库唯一出处 = 独立审计/08_修复包/③加性天光无缝/
  05_正向规格.md 判据01：相邻 control 预测背景的相对电平台阶诊断量
  rel_step = |bg_i - bg_j| / max(bg_i, bg_j, eps)，阈值 0.1（自述"经验阈值，
  诊断而非强制门"），配套判决规则：>20% 邻对超 0.1 判红、mean>0.2 强提示。
- 它 **不是** IRLS 步长上限：IRLS 收敛面只有 tolerance / tolerance_relative
  （相对判据 1e-6）/ gs_damping（默认 1.0）/ stall 检测（upm.cpp、
  PHASE2_UPM_IMPL.md F3）；grep 全仓库无任何 0.1 步长上限。

本脚本三部分：
  Part1 健康场景假阳性扫描：天光梯度 G x 背景电平 B0，诊断在合法陡峭场景
        的误报率（阈值 0.1 是否被"健康"模型自然击穿）。
  Part2 缺陷检出：按 05 规格注入配方（单节点系数异常）扫缺陷幅度，
        检出概率、与科学接缝门 1e-2 的灵敏度对比、判决规则结构盲区。
  Part3 IRLS 收敛面：把 0.1 当"步长上限"读法下的反事实扫描——
        步长上限 kappa x 收敛速度 x 最终判据值，验证它是否承载科学语义。
复现：python3 code/eb_relstep_calibration.py
"""
from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
import p5c_common as P  # noqa: E402

GRID = 8
DELTA_HUBER = 1.345
TOL = 1e-6
MAX_ITER = 300
ANCHOR = 1e-3  # 弱零锚（相对典型权重的量级，PHASE2_UPM §5 zero_anchor_weight）
N_SUB = 5      # 每 (帧, cell) 的子区中位数观测数（制造真实 IRLS 多观测语境）


# ---------------------------------------------------------------- 场景
def make_scene(b0, grad, curvature=0.0, defect=None, seed_tag="eb",
               outlier_frac=0.0, outlier_sigma=8.0):
    """2 帧全重叠场景的 control 观测。

    帧 A：B0 + 小梯度 grad/4；帧 B：B0 + off + grad 梯度（跨 tile 幅度 grad）
    + curvature 二次项；defect=(gx,gy,delta_e) 在帧 B 的一个 cell 注入台阶。
    观测 = 每 cell N_SUB 个子区中位数 + 控制噪声（control_variance 冻结式）。
    返回 dict(X, y, sigma, cell, frame, truth_pred)。
    """
    rng = P.derive_rng(seed_tag)
    yy, xx = np.mgrid[0:GRID, 0:GRID]
    cx = (xx + 0.5) / GRID   # cell 中心坐标 ∈ [0,1]
    cy = (yy + 0.5) / GRID
    rows = []
    meta = []
    # 每 cell N_SUB 个子采样位置（固定抖动）
    offs = [(0.25, 0.25), (0.75, 0.25), (0.25, 0.75), (0.75, 0.75), (0.5, 0.5)]
    sigma_ctrl = float(np.sqrt(P.K_CORR * np.pi / 2 * 10.0 ** 2 / 3500.0))
    for k, (nm, off, gmul) in enumerate((("A", 0.0, 0.25), ("B", 3.0, 1.0))):
        for gy in range(GRID):
            for gx in range(GRID):
                level = (b0 + off + grad * gmul * (cx[gy, gx] - 0.5)
                         + curvature * gmul * (cx[gy, gx] - 0.5) ** 2)
                dlv = 0.0
                if defect is not None and nm == "B" and defect[0] == gx \
                        and defect[1] == gy:
                    dlv = defect[2]
                for si, (ox, oy) in enumerate(offs):
                    # 子采样位置处的天光（平面在 cell 内的线性变化）
                    sub = (level + dlv
                           + grad * gmul * (ox - 0.5) / GRID)
                    val = sub + rng.normal(0.0, sigma_ctrl)
                    if outlier_frac > 0 and rng.uniform() < outlier_frac:
                        val += rng.normal(0.0, outlier_sigma * sigma_ctrl)
                    rows.append(val)
                    meta.append((gy, gx, k, si))
    y = np.array(rows)
    n = y.size
    n_par = GRID * GRID * 3  # M(64) + C_A(64) + C_B(64)
    X = np.zeros((n, n_par))
    for i, (gy, gx, k, si) in enumerate(meta):
        c = gy * GRID + gx
        X[i, c] = 1.0                    # M_c
        X[i, GRID * GRID + k * GRID * GRID + c] = 1.0  # C_{k,c}
    sigma = np.full(n, sigma_ctrl)
    # 真值预测面（帧 B 的模型预测背景 = level + dlv，cell 中心）
    truth = (b0 + 3.0 + grad * (cx - 0.5) + curvature * (cx - 0.5) ** 2)
    return dict(X=X, y=y, sigma=sigma, meta=meta, truth_b=truth,
                sigma_ctrl=sigma_ctrl)


# ---------------------------------------------------------------- IRLS
def huber_irls(sc, step_cap=None, tol=TOL, max_iter=MAX_ITER,
               anchor=ANCHOR):
    """加权 Huber IRLS + 弱零锚 + 文档口径收敛判据（无量纲、相对化）。

    收敛判据（11_upm §4.6）：max|dM|/max(scale_obs,1) < tol 且
    |dobj|/max(|obj|,eps) < tol；scale_obs = median|y|。
    step_cap（反事实参数）：若 max|dtheta| > cap*scale_obs，按比例收缩该步
    （信任域式阻尼）。返回 dict(theta, iters, converged, objective, ...)。
    """
    X, y, sigma = sc["X"], sc["y"], sc["sigma"]
    scale_obs = float(np.median(np.abs(y)))
    w_iv = 1.0 / sigma ** 2
    lam = anchor * float(np.mean(w_iv))
    n_par = X.shape[1]
    P_reg = np.eye(n_par) * lam
    # 锚只打在 C 上（M 是公共面，不锚零）；M 锚零会把背景拉到 0
    n_m = GRID * GRID
    P_reg[:n_m, :n_m] = 0.0
    theta = np.zeros(n_par)
    theta[n_m:] = 0.0
    obj_prev = np.inf
    iters = 0
    converged = 0  # 0=max_iter / 1=converged / 2=stalled / 3=invalid
    for it in range(max_iter):
        iters = it + 1
        r = y - X @ theta
        z = r / sigma
        u = np.minimum(1.0, DELTA_HUBER / np.maximum(np.abs(z), 1e-300))
        W = w_iv * u
        XW = X * W[:, None]
        H = X.T @ XW + P_reg
        g = X.T @ (W * y)
        try:
            theta_new = np.linalg.solve(H, g)
        except np.linalg.LinAlgError:
            converged = 3
            break
        d = theta_new - theta
        if step_cap is not None:
            cap = step_cap * scale_obs
            md = float(np.max(np.abs(d)))
            if md > cap > 0:
                theta_new = theta + d * (cap / md)
        d = theta_new - theta
        r_new = y - X @ theta_new
        z_new = r_new / sigma
        obj = float(np.sum(w_iv * np.where(
            np.abs(z_new) <= DELTA_HUBER,
            0.5 * z_new ** 2,
            DELTA_HUBER * (np.abs(z_new) - 0.5 * DELTA_HUBER))))
        rel_dtheta = float(np.max(np.abs(d))) / max(scale_obs, 1.0)
        rel_dobj = abs(obj - obj_prev) / max(abs(obj_prev), 1e-300) \
            if np.isfinite(obj_prev) else 1.0
        theta = theta_new
        obj_prev = obj
        if rel_dtheta < tol and rel_dobj < tol:
            converged = 1
            break
    return dict(theta=theta, iters=iters, converged=converged,
                objective=obj_prev, scale_obs=scale_obs)


def predicted_fields(sc, theta):
    """从解参数还原每帧的预测背景面（8x8）。"""
    n_m = GRID * GRID
    M = theta[:n_m].reshape(GRID, GRID)
    C_A = theta[n_m:2 * n_m].reshape(GRID, GRID)
    C_B = theta[2 * n_m:].reshape(GRID, GRID)
    return M + C_A, M + C_B


def relstep_stats(field):
    """05 规格判据01 的诊断量：4-邻接邻对的 rel_step 分布。"""
    a = []
    b = []
    for gy in range(GRID):
        for gx in range(GRID):
            if gx + 1 < GRID:
                a.append(field[gy, gx])
                b.append(field[gy, gx + 1])
            if gy + 1 < GRID:
                a.append(field[gy, gx])
                b.append(field[gy + 1, gx])
    a = np.array(a)
    b = np.array(b)
    rel = np.abs(a - b) / np.maximum(np.maximum(np.abs(a), np.abs(b)), 1e-12)
    return dict(n_pairs=int(rel.size), frac_gt_01=float(np.mean(rel > 0.1)),
                mean_rel=float(np.mean(rel)), max_rel=float(np.max(rel)),
                spec_red=bool(np.mean(rel > 0.1) > 0.2),
                mean_gt_02=bool(np.mean(rel) > 0.2))


def main():
    res = dict(seed=P.SEED_BASE, tol=TOL, max_iter=MAX_ITER, anchor=ANCHOR,
               huber_delta=DELTA_HUBER, n_sub=N_SUB)

    # ================= Part 1：健康场景假阳性扫描 ========================
    part1 = []
    for b0 in (5.0, 100.0, 1000.0):
        for grad in (0.0, 2.0, 5.0, 10.0, 20.0, 40.0, 80.0):
            sc = make_scene(b0, grad, seed_tag="eb|p1|%g|%g" % (b0, grad))
            sol = huber_irls(sc)
            _, pred_b = predicted_fields(sc, sol["theta"])
            st = relstep_stats(pred_b)
            part1.append(dict(b0=b0, grad=grad, **st,
                              iters=sol["iters"], converged=sol["converged"]))
            print("p1", b0, grad, st["frac_gt_01"], st["spec_red"])
    res["part1_false_positive_scan"] = part1

    # ================= Part 2：缺陷检出 ==================================
    # 2a. 05 规格注入配方：单节点系数 x10（后处理注入到预测面）
    base_sc = make_scene(100.0, 10.0, seed_tag="eb|p2|base")
    base_sol = huber_irls(base_sc)
    _, base_pred = predicted_fields(base_sc, base_sol["theta"])
    inj = base_pred.copy()
    inj[4, 3] *= 10.0
    st_inj10 = relstep_stats(inj)
    res["part2a_spec_recipe_x10"] = st_inj10
    print("p2a x10:", st_inj10)

    # 2b. 加性缺陷幅度扫描（后处理注入）
    part2b = []
    for d in (0.0, 1.0, 2.0, 3.0, 5.0, 10.0, 20.0, 30.0, 50.0, 100.0):
        inj = base_pred.copy()
        inj[4, 3] += d
        st = relstep_stats(inj)
        part2b.append(dict(delta_e=d, **st))
    res["part2b_additive_scan_postfit"] = part2b

    # 2c. 真值注入 + 重新拟合（模型吸收效应）
    part2c = []
    for d in (0.0, 2.0, 3.0, 5.0, 10.0, 20.0, 50.0):
        sc = make_scene(100.0, 10.0, defect=(3, 4, d),
                        seed_tag="eb|p2c|%g" % d)
        sol = huber_irls(sc)
        _, pred_b = predicted_fields(sc, sol["theta"])
        st = relstep_stats(pred_b)
        part2c.append(dict(delta_e=d, **st))
    res["part2c_truth_injection_refit"] = part2c

    # 2d. 与科学接缝门 1e-2 的灵敏度对比：缺陷 e- 阈值
    #     诊断阈 0.1 在 B0=100 处的首对触发幅度：
    d50 = next((r["delta_e"] for r in part2b if r["frac_gt_01"] > 0), None)
    dany = next((r["delta_e"] for r in part2b if r["max_rel"] > 0.1), None)
    res["part2d_detection_threshold_e"] = dict(
        first_pair_gt_01=dany, bg=100.0,
        science_gate_1e2_step_e=0.01 * 100.0,
        note="diagnostic threshold 0.1 corresponds to ~10% adjacent-control "
             "level step; science seam gate 1e-2 on footprint rel_step")
    print("p2d threshold:", dany)

    # ================= Part 3：IRLS 收敛面（步长上限反事实扫描）==========
    sc3 = make_scene(100.0, 20.0, curvature=30.0, seed_tag="eb|p3",
                     outlier_frac=0.03, outlier_sigma=8.0)
    sol_ref = huber_irls(sc3, step_cap=None)
    part3 = []
    for cap in (None, 1.0, 0.3, 0.1, 0.03, 0.01):
        sol = huber_irls(sc3, step_cap=cap)
        _, pred_b = predicted_fields(sc3, sol["theta"])
        st = relstep_stats(pred_b)
        dtheta = float(np.max(np.abs(sol["theta"] - sol_ref["theta"])))
        part3.append(dict(
            step_cap=("none" if cap is None else cap),
            iters=sol["iters"], converged=sol["converged"],
            objective=sol["objective"],
            rel_obj=float(sol["objective"] / sol_ref["objective"] - 1.0),
            max_dtheta_over_scale=dtheta / sol_ref["scale_obs"],
            diag_max_rel=st["max_rel"], diag_frac_gt_01=st["frac_gt_01"]))
        print("p3", cap, sol["iters"], sol["converged"], part3[-1])
    res["part3_irls_step_cap_scan"] = part3
    res["part3_reference"] = dict(iters=sol_ref["iters"],
                                  converged=sol_ref["converged"],
                                  objective=sol_ref["objective"])

    P.save_json(P.RESULTS / "eb_relstep_calibration.json", res)
    print("done")


if __name__ == "__main__":
    main()
