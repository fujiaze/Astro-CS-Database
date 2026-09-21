#!/usr/bin/env python3
"""IMPL-P2-UPM-001 独立 Oracle — UPM 乘法/加性分离 (y_k = g_k s_p + b_k).

独立于被测 C++ 实现:
  - 解法 = 块坐标下降 (alternating least squares, ALS) + 每块闭式加权最小二乘,
    并可用 scipy.optimize.least_squares (trf) 交叉复核;
  - Jacobian/正规矩阵/秩/条件数/参数协方差全部用 NumPy 独立重算;
  - 输出冻结锚值 anchors.json, 供 C++ 共址测试以常量比较 (跨环境 Oracle).

部署: python3 run/v6/IMPL-P2-UPM-001/oracle/upm_ma_oracle.py
"""
import json
import math
import os
import sys

import numpy as np

RANK_RTOL = 1e-10
KAPPA_MAX = 1.0e6
MIN_FRAMES = 2


# ----------------------------------------------------------------------------
# 模型/参数布局 (与任务契约一致, 独立实现)
# ----------------------------------------------------------------------------
class Graph:
    def __init__(self):
        self.p = {}

    def find(self, x):
        self.p.setdefault(x, x)
        while self.p[x] != x:
            self.p[x] = self.p[self.p[x]]
            x = self.p[x]
        return x

    def union(self, a, b):
        ra, rb = self.find(a), self.find(b)
        if ra != rb:
            self.p[rb] = ra


def components(frames, controls, obs):
    g = Graph()
    for f in frames:
        g.find(("f", f))
    for c in controls:
        g.find(("c", c))
    for (f, c, _y, _w) in obs:
        g.union(("f", f), ("c", c))
    roots = {}
    for f in frames:
        roots.setdefault(g.find(("f", f)), []).append(("f", f))
    for c in controls:
        roots.setdefault(g.find(("c", c)), []).append(("c", c))
    order = sorted(roots.keys(), key=lambda r: (r[0], r[1]))
    comp = {}
    ref = {}
    for ci, r in enumerate(order):
        fr = [x[1] for x in roots[r] if x[0] == "f"]
        for x in roots[r]:
            comp[x] = ci
        ref[ci] = min(fr)
    return comp, ref, len(order)


def layout(frames, controls, comp, ref, allow_additive_only):
    nf, np_ = len(frames), len(controls)
    n_full = np_ + 2 * nf
    fixed = set()
    for c in range(len(ref)):
        rf = ref[c]
        fixed.add(np_ + frames.index(rf))
        fixed.add(np_ + nf + frames.index(rf))
    full_of_free = [j for j in range(n_full) if j not in fixed]
    free_of_full = {j: i for i, j in enumerate(full_of_free)}
    return n_full, full_of_free, free_of_full


def solve_als(frames, controls, obs, comp, ref, allow_additive_only, iters=4000):
    """块坐标下降: 每块闭式加权 LS (不调用 C++ 实现)."""
    nf, np_ = len(frames), len(controls)
    f_idx = {f: i for i, f in enumerate(frames)}
    c_idx = {c: i for i, c in enumerate(controls)}
    recs = [(f_idx[f], c_idx[c], y, w) for (f, c, y, w) in obs]

    # 单帧分量: 若声明 additive-only, g 固定 1; (gauge 也已固定 ref)
    add_only = [False] * len(ref)
    for c in range(len(ref)):
        nfc = len([1 for (fi, ci, _y, _w) in recs if comp[("f", frames[fi])] == c])
        if nfc < MIN_FRAMES:
            add_only[c] = bool(allow_additive_only)

    g = {f: 1.0 for f in frames}
    b = {f: 0.0 for f in frames}
    s = {}
    for c in controls:
        num = den = 0.0
        for (fi, ci, y, w) in recs:
            if controls[ci] != c:
                continue
            f = frames[fi]
            num += w * g[f] * (y - b[f])
            den += w * g[f] * g[f]
        s[c] = num / den if den > 0 else 0.0

    for _ in range(iters):
        maxstep = 0.0
        # s | g,b
        for c in controls:
            num = den = 0.0
            for (fi, ci, y, w) in recs:
                if controls[ci] != c:
                    continue
                f = frames[fi]
                num += w * g[f] * (y - b[f])
                den += w * g[f] * g[f]
            new = num / den if den > 0 else 0.0
            maxstep = max(maxstep, abs(new - s[c]))
            s[c] = new
        # g,b | s
        for f in frames:
            if add_only[comp[("f", f)]]:
                g[f], b[f] = 1.0, 0.0
                continue
            if f == ref[comp[("f", f)]]:
                g[f], b[f] = 1.0, 0.0
                continue
            ATA = np.zeros((2, 2))
            ATy = np.zeros(2)
            for (fi, ci, y, w) in recs:
                if frames[fi] != f:
                    continue
                p = np.array([s[controls[ci]], 1.0])
                ATA += w * np.outer(p, p)
                ATy += w * p * y
            sol = np.linalg.lstsq(ATA, ATy, rcond=None)[0]
            maxstep = max(maxstep, float(np.max(np.abs(sol - np.array([g[f], b[f]])))))
            g[f], b[f] = float(sol[0]), float(sol[1])
        if maxstep < 1e-14:
            break
    return g, b, s, add_only


def solve_scipy(frames, controls, obs, comp, ref, allow_additive_only, g0, b0, s0):
    """独立交叉复核: scipy trf on free params."""
    from scipy.optimize import least_squares
    nf, np_ = len(frames), len(controls)
    n_full = np_ + 2 * nf
    fixed = set()
    for c in range(len(ref)):
        rf = ref[c]
        fixed.add(np_ + frames.index(rf))
        fixed.add(np_ + nf + frames.index(rf))
    add_only = [False] * len(ref)
    for c in range(len(ref)):
        nfc = len([1 for (f, _c, _y, _w) in obs if comp[("f", f)] == c])
        if nfc < MIN_FRAMES:
            add_only[c] = bool(allow_additive_only)
    f_idx = {f: i for i, f in enumerate(frames)}
    c_idx = {c: i for i, c in enumerate(controls)}
    for c in range(len(ref)):
        if add_only[c]:
            rf = ref[c]
            fixed.add(np_ + f_idx[rf])

    full_of_free = [j for j in range(n_full) if j not in fixed]

    def unpack(x):
        theta = np.zeros(n_full)
        for j, v in zip(full_of_free, x):
            theta[j] = v
        for c in range(len(ref)):
            rf = ref[c]
            theta[np_ + f_idx[rf]] = 1.0
            theta[np_ + nf + f_idx[rf]] = 0.0
        return theta

    def resid(x):
        theta = unpack(x)
        out = []
        for (f, c, y, w) in obs:
            fi, ci = f_idx[f], c_idx[c]
            m = theta[np_ + fi] * theta[ci] + theta[np_ + nf + fi]
            out.append(math.sqrt(w) * (y - m))
        return np.array(out)

    x0_full = np.zeros(n_full)
    for j, f in enumerate(frames):
        x0_full[np_ + j] = g0[f]
        x0_full[np_ + nf + j] = b0[f]
    for j, c in enumerate(controls):
        x0_full[j] = s0[c]
    x0 = np.array([x0_full[j] for j in full_of_free])
    res = least_squares(resid, x0, method="trf", xtol=1e-15, ftol=1e-15, gtol=1e-15)
    theta = unpack(res.x)
    g = {f: float(theta[np_ + i]) for i, f in enumerate(frames)}
    b = {f: float(theta[np_ + nf + i]) for i, f in enumerate(frames)}
    s = {c: float(theta[i]) for i, c in enumerate(controls)}
    return g, b, s, float(np.sum(res.fun ** 2))


def design_matrix(frames, controls, obs, theta, full_of_free):
    nf, np_ = len(frames), len(controls)
    f_idx = {f: i for i, f in enumerate(frames)}
    c_idx = {c: i for i, c in enumerate(controls)}
    rows = []
    for (f, c, _y, _w) in obs:
        fi, ci = f_idx[f], c_idx[c]
        row = np.zeros(len(full_of_free))
        for k, j in enumerate(full_of_free):
            if j == ci:
                row[k] = theta[np_ + fi]          # d/ds_p = g_k
            elif j == np_ + fi:
                row[k] = theta[ci]                # d/dg_k = s_p
            elif j == np_ + nf + fi:
                row[k] = 1.0                      # d/db_k
        rows.append(row)
    return np.array(rows) if rows else np.zeros((0, len(full_of_free)))


def analyze(frames, controls, obs, g, b, s, comp, ref, allow_additive_only):
    nf, np_ = len(frames), len(controls)
    n_full = np_ + 2 * nf
    _, full_of_free, _ = layout(frames, controls, comp, ref, allow_additive_only)
    theta = np.zeros(n_full)
    for i, f in enumerate(frames):
        theta[np_ + i] = g[f]
        theta[np_ + nf + i] = b[f]
    for i, c in enumerate(controls):
        theta[i] = s[c]
    J = design_matrix(frames, controls, obs, theta, full_of_free)
    W = np.diag([w for (_f, _c, _y, w) in obs])
    H = J.T @ W @ J
    # rank via SVD of whitened Jacobian (W^{1/2} J)
    Ws = np.sqrt(np.diag(W))
    Jw = Ws[:, None] * J
    sv = np.linalg.svd(Jw, compute_uv=False)
    smax = sv[0] if sv.size else 0.0
    rank = int(np.sum(sv / smax > RANK_RTOL)) if smax > 0 else 0
    # kappa = cond2(D^-1 H D^-1), D = diag(col norms) of W^{1/2}J
    d = np.sqrt(np.diag(H))
    M = H / np.outer(d, d)
    mev = np.linalg.eigvalsh(M)
    kappa = float(mev[-1] / mev[0]) if mev[0] > 0 else float("inf")
    try:
        C_theta = np.linalg.inv(H)
        invertible = True
    except np.linalg.LinAlgError:
        C_theta = np.linalg.pinv(H)
        invertible = False
    return J, H, sv, rank, kappa, C_theta, theta, invertible


# ----------------------------------------------------------------------------
# 数据集 (与 C++ 共址测试镜像)
# ----------------------------------------------------------------------------
def ds_exact():
    frames = [10, 20, 30]
    controls = [100, 200, 300]
    st = {100: 10.0, 200: 20.0, 300: 40.0}
    gt = {10: 1.0, 20: 0.9, 30: 1.1}
    bt = {10: 0.0, 20: 2.5, 30: -1.5}
    obs = []
    for f in frames:
        for c in controls:
            obs.append((f, c, gt[f] * st[c] + bt[f], 1.0))
    return frames, controls, obs


def ds_disconnected():
    frames = [1, 2, 101, 102]
    controls = [11, 12, 111, 112]
    st = {11: 3.0, 12: 5.0, 111: 7.0, 112: 9.0}
    gt = {1: 1.0, 2: 1.5, 101: 1.0, 102: 0.5}
    bt = {1: 0.0, 2: 4.0, 101: 0.0, 102: -2.0}
    obs = []
    for f in [1, 2]:
        for c in [11, 12]:
            obs.append((f, c, gt[f] * st[c] + bt[f], 1.0))
    for f in [101, 102]:
        for c in [111, 112]:
            obs.append((f, c, gt[f] * st[c] + bt[f], 1.0))
    return frames, controls, obs


def ds_constant_s():
    frames = [1, 2]
    controls = [11, 12]
    obs = []
    for c in controls:
        obs.append((1, c, 5.0, 1.0))
        obs.append((2, c, 0.7 * 5.0 + 3.0, 1.0))
    return frames, controls, obs


def ds_minframes():
    frames = [7]
    controls = [71, 72]
    obs = [(7, 71, 4.0, 1.0), (7, 72, 8.0, 1.0)]
    return frames, controls, obs


def ds_illcond(eps):
    frames = [1, 2]
    controls = [11, 12]
    s0, s1 = 1.0, 1.0 + eps
    obs = [(1, 11, 1.0 * s0 + 0.0, 1.0), (1, 12, 1.0 * s1 + 0.0, 1.0),
           (2, 11, 0.7 * s0 + 3.0, 1.0), (2, 12, 0.7 * s1 + 3.0, 1.0)]
    return frames, controls, obs


def run_dataset(name, frames, controls, obs, allow_additive_only=False, do_scipy=False):
    comp, ref, ncomp = components(frames, controls, obs)
    g, b, s, add_only = solve_als(frames, controls, obs, comp, ref, allow_additive_only)
    if do_scipy:
        g2, b2, s2, sse2 = solve_scipy(frames, controls, obs, comp, ref,
                                       allow_additive_only, g, b, s)
        maxdiff = max([abs(g2[f] - g[f]) for f in frames] +
                      [abs(b2[f] - b[f]) for f in frames] +
                      [abs(s2[c] - s[c]) for c in controls])
    else:
        maxdiff = None
    J, H, sv, rank, kappa, C_theta, theta, invertible = analyze(
        frames, controls, obs, g, b, s, comp, ref, allow_additive_only)
    out = {
        "name": name,
        "frames": frames, "controls": controls,
        "n_components": ncomp,
        "component_ref_frame": {str(k): v for k, v in ref.items()},
        "n_free": len(av := layout(frames, controls, comp, ref, allow_additive_only)[1]),
        "rank": rank,
        "invertible": invertible,
        "kappa": kappa,
        "sigma": [float(x) for x in sv],
        "g": {str(f): g[f] for f in frames},
        "b": {str(f): b[f] for f in frames},
        "s": {str(c): s[c] for c in controls},
        "C_theta": C_theta.tolist(),
        "als_vs_scipy_maxdiff": maxdiff,
        "full_of_free": av,
    }
    return out


def main():
    out = {}
    out["rank_rtol"] = RANK_RTOL
    out["kappa_max"] = KAPPA_MAX
    out["min_frames"] = MIN_FRAMES

    # A: 精确恢复 (3x3), scipy 交叉复核
    fr, co, ob = ds_exact()
    out["A_exact"] = run_dataset("A_exact", fr, co, ob, do_scipy=True)

    # B: 两个断开分量, 独立 gauge
    fr, co, ob = ds_disconnected()
    out["B_disconnected"] = run_dataset("B_disconnected", fr, co, ob, do_scipy=True)

    # C: 恒常 s -> g/b 退化 -> 秩亏
    fr, co, ob = ds_constant_s()
    out["C_constant_s"] = run_dataset("C_constant_s", fr, co, ob)

    # E: 单帧 additive-only
    fr, co, ob = ds_minframes()
    out["E_additive_only"] = run_dataset("E_additive_only", fr, co, ob,
                                         allow_additive_only=True)

    # F: 近简并 g/b -> kappa 超限; 扫描 eps 找到 >1e6
    found = None
    for eps in [1e-4, 1e-5, 1e-6, 1e-7, 1e-8, 1e-9]:
        fr, co, ob = ds_illcond(eps)
        r = run_dataset("F_illcond_%g" % eps, fr, co, ob)
        if r["kappa"] > KAPPA_MAX:
            found = (eps, r)
            break
    if found is not None:
        out["F_illcond"] = found[1]
        out["F_illcond"]["eps"] = found[0]
    else:
        out["F_illcond"] = None

    # 写 anchors
    here = os.path.dirname(os.path.abspath(__file__))
    with open(os.path.join(here, "anchors.json"), "w") as fh:
        json.dump(out, fh, indent=2, sort_keys=True)

    # 打印 C++ 测试可直接嵌入的锚点
    a = out["A_exact"]
    print("== A_exact ==")
    print("n_components=%d n_free=%d rank=%d" % (a["n_components"], a["n_free"], a["rank"]))
    print("kappa=%.12g" % a["kappa"])
    print("g=", {k: "%.15g" % v for k, v in a["g"].items()})
    print("b=", {k: "%.15g" % v for k, v in a["b"].items()})
    print("s=", {k: "%.15g" % v for k, v in a["s"].items()})
    print("als_vs_scipy_maxdiff=%.3e" % a["als_vs_scipy_maxdiff"])
    print("C_theta[0][0]=%.15g C_theta[6][6]=%.15g" % (a["C_theta"][0][0], a["C_theta"][6][6]))
    b = out["B_disconnected"]
    print("== B_disconnected ==")
    print("n_components=%d n_free=%d rank=%d refs=%s" %
          (b["n_components"], b["n_free"], b["rank"], b["component_ref_frame"]))
    print("g=", {k: "%.15g" % v for k, v in b["g"].items()})
    print("s=", {k: "%.15g" % v for k, v in b["s"].items()})
    c = out["C_constant_s"]
    print("== C_constant_s == n_free=%d rank=%d (expect rank<n_free)" % (c["n_free"], c["rank"]))
    e = out["E_additive_only"]
    print("== E_additive_only == n_free=%d rank=%d g=%s b=%s" %
          (e["n_free"], e["rank"], e["g"], e["b"]))
    if out["F_illcond"]:
        f = out["F_illcond"]
        print("== F_illcond eps=%g == kappa=%.6e (>1e6) rank=%d n_free=%d" %
              (f["eps"], f["kappa"], f["rank"], f["n_free"]))
    print("WROTE", os.path.join(here, "anchors.json"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
