# -*- coding: utf-8 -*-
"""接缝门沿边**覆盖率诊断量**与覆盖率不足时的显式失败（部分覆盖接缝的稀释面）。

纪律
====
- **只读**调用生产门本体 `eng/tools/e2e/seam_footprint.py` 的 `edge_metric` /
  `gate_decision`（判据不重写、不复制；正负例与生产同一条代码路径）。
- 本脚本**不修改**生产门本体，也不改判据量 `rel_step`；只在其**判决面之外**
  追加一条覆盖率诊断量与一条 fail-closed 前置约束，并把两者落盘。
- 固定 seed；无网络；无时间/环境随机源。

要闭合的失效域
==============
门判据量 `rel_step = median(seam)/bg` 的分子是**沿边中位数**。一个只覆盖边界
长度 f 的真实台阶，读数的中位数只在 `f > 0.5` 时等于台阶本身；`f < 0.5` 时中位数
落在无台阶的一侧 ⇒ `rel_step` 精确读 0（门判绿）。该转移点**与台阶幅度无关**：
台阶是 32%（32× 确定性地板）时 f = 0.40 仍读 0.00000。而此时 `n` 远大于
`min_samples`、`interior = True`、`exclude = None` ⇒ **门不知道自己测不了**。

覆盖率诊断量的定义
==================
沿边每条边的法向差分样本 `seam_i`（i = 1..n）与生产门同口径：
`seam = I(p + d·n̂) − I(p − d·n̂)`，`d = 2 px`；对照线取生产门同一条
（法向 `k·ctrl_shift = ±200 px` 的平行线，`k` 由生产的「有效样本多的一侧」规则定）。

    lv  ≡ median(ctrl)                      # 对照线上的背景结构台阶水平 [ADU]
    s_  ≡ 1.4826 · median|ctrl − lv|        # 对照线的稳健散布 [ADU]（生产 MAD 口径）
    cov ≡ #{ i : |seam_i − lv| > k_c · s_ } / n        # **沿边覆盖率**

`cov` 的含义：沿边有多少比例的样本**处在偏离对照线电平的状态**。完整覆盖的真台阶
`cov → 1`；部分覆盖 `cov → f`；纯噪声世界 `cov → 2·Φ̄(−k_c)`（高斯下 k_c = 3 时
≈ 0.0027）。`k_c = 3` 取「3 倍对照线散布」这一常用稳健判带，与本单元 `patch_estimate`
的亮端裁剪门同量级，不是拟合值。

门限（推导，不是拟合）
======================
判据分子是**中位数**，中位数从「无台阶样本」转移到「台阶样本」的**精确转移点是
f = 0.5**，且与台阶幅度无关 ⇒ 中位数等于台阶本身的充要条件是 `cov > 0.5`。
故本脚本的前置约束取

    cov ≥ 0.5   （且 cov·n ≥ min_samples：承载台阶的样本数本身要够）

`cov = 0.5` 恰是中位数的**退化点**（读数在台阶与无台阶之间任意取值），故用严格
不等号一侧的闭区间 `cov ≥ 0.5` 只在无噪声理想情形取到；实跑里同时落盘
`cov_margin_sigma = (cov − 0.5)/sqrt(0.25/n)`，让读者看到离退化点多远。

`cov < 0.5` ⇒ **显式判红**（`FAIL_COVERAGE`），与「读到 0 判绿」在判决面上分开：
前者说「这条边测不了」，后者说「这条边没有接缝」。

复现：python3 实验/additive-sky-seamless/code/seam_gate_coverage.py
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

import numpy as np

UNIT = Path(__file__).resolve().parents[1]           # 实验/additive-sky-seamless/
ROOT = UNIT.parents[1]                               # 仓库根
sys.path.insert(0, str(ROOT / "eng" / "tools" / "e2e"))
from seam_footprint import (  # noqa: E402  （生产门本体，只读）
    edge_metric, gate_decision, _ctrl_at, bilinear, mad)

SEED = 20260930          # 本探针专属 seed
GATE = 1e-2              # SCI-C R5 相对接缝门（判据面唯一阈值，本脚本不改）
D = 2.0
CTRL_SHIFT = 200.0
MIN_SAMPLES = 20
K_COV = 3.0              # 覆盖率诊断的稳健判带（倍对照线散布）
COV_MIN = 0.5            # 中位数的精确转移点（推导值）

BG = 300.0               # 背景电平 [ADU]
XB = 600                 # 被检竖边（法向 = −x̂）
NY = 1200                # 沿边采样点数
Y_LO, Y_HI = 200.0, 1400.0
IMG_HALF = 800


def boundary(n=NY, y_lo=Y_LO, y_hi=Y_HI):
    y = np.linspace(y_lo, y_hi, n)
    return np.stack([np.full(n, float(XB)), y], axis=1)


def make_image(frac=1.0, step_adu=0.0, sigma=0.0, n=NY, seed=SEED):
    """背景 300 ADU 的平场 + 一条竖直边界；真台阶只覆盖沿边**前 frac 比例**。

    台阶施加在 + 法向一侧（x ≥ XB）的**前 frac 长度**上；frac < 1 时其余段是
    「足迹边缘」（无台阶），这正是抖动镶嵌里第二条帧只覆盖边界一部分的形态。
    """
    rng = np.random.default_rng(seed)
    img = np.full((2 * IMG_HALF, 2 * IMG_HALF), BG, dtype=float)
    if step_adu:
        y0 = int(round(Y_LO))
        y1 = y0 + max(1, int(round(frac * n)))
        img[y0:y1, XB:] += step_adu      # 台阶在 + 法向一侧（x ≥ XB）的沿边前 f 段
    if sigma:
        img = img + rng.normal(0.0, sigma, img.shape)
    return img


def edge_samples(img, e, d=D, ctrl_shift=CTRL_SHIFT):
    """重算生产门的 `seam` / `ctrl` **样本数组**（判据量本身仍由 edge_metric 给）。

    只重算样本数组（判据的分子分母仍取自生产 `edge_metric`），并由 `verify_mirror`
    逐条核对与生产读数一致；对照线直接调用生产的 `_ctrl_at` 与选边规则。
    """
    ex, ey = e[:, 0], e[:, 1]
    tx, ty = np.gradient(ex), np.gradient(ey)
    tn = np.hypot(tx, ty)
    tn[tn == 0] = 1.0
    tx, ty = tx / tn, ty / tn
    nxv, nyv = -ty, tx
    seam = bilinear(img, ex + nxv * d, ey + nyv * d) - bilinear(img, ex - nxv * d, ey - nyv * d)
    seam_ok = np.isfinite(seam)
    ctrl_by_side, n_ok = {}, {}
    for s in (+1.0, -1.0):
        ctrl_by_side[s] = _ctrl_at(img, ex, ey, nxv, nyv, s, ctrl_shift, d)
        n_ok[s] = int(np.isfinite(ctrl_by_side[s]).sum())
    k = +1.0 if n_ok[+1.0] >= n_ok[-1.0] else -1.0
    ctrl = ctrl_by_side[k]
    m = seam_ok & np.isfinite(ctrl)
    return dict(seam=seam[m], ctrl=ctrl[m], n=int(m.sum()), ctrl_side=("+" if k > 0 else "-"))


def coverage(img, e, d=D, ctrl_shift=CTRL_SHIFT, k_cov=K_COV, bg=None,
             gate=GATE):
    """沿边覆盖率诊断量 cov 与其离中位数退化点的 σ 余量。

    判别阈 `thr` 取两者的大者，两项都有依据：
    - `k_cov · s_`：对照线的稳健散布（噪声/结构决定的分辨下限），`k_cov = 3`；
    - `0.5 · gate · bg`：**半个门阈**（`bg` 由生产门的局部电平口径给出）。
      无噪夹具里对照线散布恰为 0，只靠第一项会退化成机器精度地板；用半个门阈兜底
      使「单点偏离就能把这条边推过门」成为计入标准，与门判据量同量纲。
    """
    s = edge_samples(img, e, d, ctrl_shift)
    sv, cv = s["seam"], s["ctrl"]
    n = sv.size
    if n == 0 or cv.size == 0:
        return dict(n=int(n), cov=0.0, cov_margin_sigma=float("nan"),
                    ctrl_mad=float("nan"), ctrl_level=float("nan"), thr=float("nan"))
    lv = float(np.median(cv))
    s_ = float(mad(cv))
    if not np.isfinite(s_):
        s_ = 0.0
    thr = max(k_cov * s_, 0.5 * gate * float(bg) if bg else 0.0)
    cov = float(np.mean(np.abs(sv - lv) > thr)) if thr > 0 else 0.0
    sigma_f = float(np.sqrt(0.25 / max(n, 1)))
    return dict(n=int(n), cov=cov, cov_margin_sigma=float((cov - COV_MIN) / sigma_f),
                ctrl_mad=float(mad(cv)), ctrl_level=lv, thr=float(thr),
                ctrl_side=s["ctrl_side"])


def edge_with_coverage(img, e, null_img=None, d=D, ctrl_shift=CTRL_SHIFT,
                       min_samples=MIN_SAMPLES):
    """生产 `edge_metric` 的读数 + 本脚本的覆盖率诊断量（不改判据量）。

    `null_img` 是**同噪声 realization、去掉真台阶**的配对零台阶对照（同脚本现造），
    用来把「沿边确实没有任何台阶偏离」与「有台阶但被稀释」区分开。
    """
    p = edge_metric(img, e, d=d, ctrl_shift=ctrl_shift, min_samples=min_samples)
    c = coverage(img, e, d, ctrl_shift, bg=p.get("bg"))
    cn = coverage(null_img if null_img is not None else img, e, d, ctrl_shift,
                  bg=p.get("bg"))
    n = max(p.get("n") or 0, 1)
    pnull = min(max(cn["cov"], 1.0 / n), 0.5)
    c["cov_null"] = float(cn["cov"])
    c["cov_null_hi"] = float(pnull + 3.0 * np.sqrt(pnull * (1.0 - pnull) / n))
    c["cov_excess"] = float(c["cov"] - c["cov_null_hi"])
    # 三区判定：与零台阶对照不可分辨（真无台阶）/ 检出但不足多数（门测不了）/ 多数成立
    if c["cov"] <= c["cov_null_hi"]:
        c["coverage_zone"] = "none"          # 真无台阶：中位读数 0 是有效结论
    elif c["cov"] <= COV_MIN:
        c["coverage_zone"] = "diluted"       # 检出偏离但不足多数 ⇒ 中位数失明
    else:
        c["coverage_zone"] = "majority"      # 多数成立：中位读数可代表台阶
    p["cov"] = c
    in_domain = (p.get("exclude") is None) and (p.get("rel_step") is not None)
    p["coverage_ok"] = bool(in_domain and c["coverage_zone"] != "diluted")
    return p


def gate_decision_with_coverage(per_edge, max_rel_excess=GATE, min_samples=MIN_SAMPLES):
    """生产判决 + 覆盖率前置约束；覆盖率不足（检出但不足多数）**显式判红**并单列原因。

    「真无台阶」区（cov 与配对零台阶对照不可分辨）保留生产的 PASS；
    「多数成立」区照常按 `max|rel_step|` 判决；
    「检出但不足多数」区（`0 < cov ≤ 0.5`）⇒ `FAIL_COVERAGE`，
    因为中位数落在无台阶一侧，此时读 0 判绿是**无效结论**。
    """
    base = gate_decision(per_edge, max_rel_excess, min_samples)
    sel = [p for p in per_edge if p.get("exclude") is None and p.get("rel_step") is not None]
    bad = [p for p in sel if not p.get("coverage_ok")]
    covs = [p["cov"]["cov"] for p in sel]
    out = dict(base)
    out["cov_min"] = float(min(covs)) if covs else None
    out["cov_median"] = float(np.median(covs)) if covs else None
    out["n_coverage_insufficient"] = len(bad)
    if bad:
        worst = min(bad, key=lambda p: p["cov"]["cov"])
        out["verdict_production"] = base["verdict"]
        out["verdict"] = "FAIL_COVERAGE"
        out["reason_coverage"] = (
            "沿边检出偏离但不足多数：%d/%d 条判据内边界的 cov ≤ %g（中位数的转移点）；"
            "最差 cov=%.4f（零台阶对照 %.4f±%.4f）、n=%d、interior=%s、exclude=%s、"
            "rel_step=%.6g ⇒ 该边的中位读数落在无台阶一侧，判绿无效"
            % (len(bad), len(sel), COV_MIN, worst["cov"]["cov"],
               worst["cov"]["cov_null"], worst["cov"]["cov_null_hi"],
               worst["n"], worst.get("interior"), worst.get("exclude"),
               worst["rel_step"]))
    return out


def main():
    e = boundary()
    res = dict(seed=SEED, gate=GATE, d=D, ctrl_shift=CTRL_SHIFT,
               min_samples=MIN_SAMPLES, k_cov=K_COV, cov_min=COV_MIN,
               cov_min_derivation="median 沿 f=0.5 转移；与台阶幅度无关")

    # ---- 0. 镜像自检：本脚本重算的样本数组必须复现生产门的判据读数 ----
    mirror = []
    for frac, amp in ((1.0, 0.04), (0.5, 0.04), (1.0, 0.0)):
        img = make_image(frac=frac, step_adu=amp * BG)
        p = edge_metric(img, e)
        s = edge_samples(img, e)
        lv = np.concatenate([np.abs(bilinear(
            img, e[:, 0] + (s["ctrl_side"] == "-") * (-1) * CTRL_SHIFT,
            e[:, 1]))]) if False else None
        # 直接用生产 edge_metric 的 bg/step 与重算样本的中位核对
        step_re = float(np.median(s["seam"]))
        mirror.append(dict(frac=frac, amp=amp,
                           prod_step=p["step"], repro_step=step_re,
                           d_step=abs(p["step"] - step_re),
                           prod_rel=p["rel_step"], prod_bg=p["bg"],
                           n=p["n"], interior=p["interior"], exclude=p["exclude"]))
    res["mirror_selfcheck"] = mirror
    res["mirror_max_abs_dstep"] = float(max(m["d_step"] for m in mirror))

    # ---- 1. 覆盖率 × 台阶幅度 二维扫描（无噪） ----
    amps = [0.02, 0.04, 0.08, 0.16, 0.32]
    fracs = [1.00, 0.80, 0.60, 0.50, 0.48, 0.40, 0.20, 0.05]
    grid, rows = [], []
    for frac in fracs:
        for amp in amps:
            img = make_image(frac=frac, step_adu=amp * BG)
            nul = make_image(frac=frac, step_adu=0.0)
            p = edge_with_coverage(img, e, null_img=nul)
            gd = gate_decision_with_coverage([p])
            grid.append(dict(frac=frac, amp=amp, n=p["n"], interior=p["interior"],
                             exclude=p["exclude"], step=p["step"], bg=p["bg"],
                             rel_step=p["rel_step"], cov=p["cov"]["cov"],
                             cov_null=p["cov"]["cov_null"],
                             zone=p["cov"]["coverage_zone"],
                             cov_margin_sigma=p["cov"]["cov_margin_sigma"],
                             prod_verdict=gd["verdict_production"]
                             if "verdict_production" in gd else gd["verdict"],
                             verdict=gd["verdict"]))
    res["coverage_amplitude_grid"] = grid

    for frac in fracs:
        img = make_image(frac=frac, step_adu=0.04 * BG)
        nul = make_image(frac=frac, step_adu=0.0)
        p = edge_with_coverage(img, e, null_img=nul)
        gd = gate_decision_with_coverage([p])
        rows.append(dict(frac=frac, n=p["n"], interior=p["interior"], exclude=p["exclude"],
                         step=p["step"], bg=p["bg"], rel_step=p["rel_step"],
                         cov=p["cov"]["cov"], cov_null=p["cov"]["cov_null"],
                         zone=p["cov"]["coverage_zone"],
                         cov_margin_sigma=p["cov"]["cov_margin_sigma"],
                         prod_verdict=gd["verdict_production"]
                         if "verdict_production" in gd else gd["verdict"],
                         verdict=gd["verdict"]))
    res["coverage_scan_amp4pct"] = rows

    # ---- 2. 带噪臂：噪声不救场，反而把被稀释的台阶进一步压掉 ----
    noisy = []
    for frac in (1.0, 0.6, 0.5, 0.48, 0.40, 0.30, 0.20):
        img = make_image(frac=frac, step_adu=0.04 * BG, sigma=20.0, seed=SEED + int(frac * 100))
        nul = make_image(frac=frac, step_adu=0.0, sigma=20.0, seed=SEED + int(frac * 100))
        p = edge_with_coverage(img, e, null_img=nul)
        gd = gate_decision_with_coverage([p])
        noisy.append(dict(frac=frac, rel_step=p["rel_step"], cov=p["cov"]["cov"],
                          cov_null=p["cov"]["cov_null"], zone=p["cov"]["coverage_zone"],
                          prod_verdict=gd["verdict_production"]
                          if "verdict_production" in gd else gd["verdict"],
                          verdict=gd["verdict"]))
    res["coverage_scan_noisy"] = dict(sigma_pix=20.0, bg=BG, rows=noisy)

    # ---- 3. 对照：完整覆盖 + 无台阶 ⇒ 生产门判绿、覆盖率也通过 ----
    ctrl_rows = []
    for tag, kw in (("no_step_full", dict(frac=1.0, step_adu=0.0)),
                    ("step_full", dict(frac=1.0, step_adu=0.04 * BG)),
                    ("step_full_16pct", dict(frac=1.0, step_adu=0.16 * BG)),
                    ("no_step_noisy", dict(frac=1.0, step_adu=0.0, sigma=20.0))):
        img = make_image(**kw)
        nul = make_image(**dict(kw, step_adu=0.0))
        p = edge_with_coverage(img, e, null_img=nul)
        gd = gate_decision_with_coverage([p])
        ctrl_rows.append(dict(case=tag, rel_step=p["rel_step"], cov=p["cov"]["cov"],
                              cov_null=p["cov"]["cov_null"],
                              zone=p["cov"]["coverage_zone"],
                              prod_verdict=gd["verdict_production"]
                              if "verdict_production" in gd else gd["verdict"],
                              verdict=gd["verdict"]))
    res["control_cases"] = ctrl_rows

    # ---- 4. 门禁：能红（覆盖率不足）+ 能绿（完整覆盖且判绿） ----
    gates = []

    def add(gid, desc, ok, value):
        gates.append(dict(id=gid, desc=desc, ok=bool(ok), value=value))

    add("COV1_mirror_faithful",
        "本脚本重算的样本数组复现生产 edge_metric 的 step（|Δ| <= 1e-9 e-）",
        res["mirror_max_abs_dstep"] <= 1e-9, res["mirror_max_abs_dstep"])
    blind = [r for r in rows if r["rel_step"] is not None and abs(r["rel_step"]) == 0.0]
    add("COV2_dilution_is_real",
        "f < 0.5 时生产门读数恰为 0（稀释面存在，%d 个档位）" % len(blind),
        len(blind) >= 3, [r["frac"] for r in blind])
    add("COV3_production_blind_verdict_green",
        "同一批 f < 0.5 的档位生产门全部判绿（门不知道自己测不了）",
        all(r["prod_verdict"] == "PASS" for r in blind),
        [r["prod_verdict"] for r in blind])
    add("COV4_coverage_reads_fraction",
        "覆盖率读数随 f 单调且 |cov − f| <= 0.03",
        all(abs(r["cov"] - r["frac"]) <= 0.03 for r in rows),
        [(r["frac"], round(r["cov"], 4)) for r in rows])
    add("COV5_gate_fails_closed_on_low_coverage",
        "0 < cov ≤ 0.5 的档位本脚本的门**显式判红**（FAIL_COVERAGE）",
        all(r["verdict"] == "FAIL_COVERAGE" for r in rows
            if 0.0 < r["cov"] <= COV_MIN),
        [(r["frac"], r["verdict"]) for r in rows])
    add("COV6_gate_green_on_no_step_control",
        "完整覆盖的**无台阶**对照（含带噪臂）本脚本的门判绿（不误伤）",
        all(r["verdict"] == "PASS" for r in ctrl_rows
            if r["case"].startswith("no_step")),
        [(r["case"], r["verdict"]) for r in ctrl_rows])
    add("COV7_gate_red_on_full_amplitude",
        "完整覆盖的真台阶（4% / 16%）判红（门仍能看见真接缝）",
        all(r["verdict"] == "FAIL" for r in ctrl_rows if not r["case"].startswith("no_step")),
        [(r["case"], r["verdict"]) for r in ctrl_rows])
    add("COV8_coverage_diagnostic_noise_limited",
        "带噪臂（σ_pix=20 @300）下 cov 与 f 无关（极差 ≤ 0.01）⇒ 4% 台阶低于逐样本"
        "可分辨限，覆盖率诊断量本身被噪声限制（登记为该诊断量的适用域）",
        max(r["cov"] for r in noisy) - min(r["cov"] for r in noisy) <= 0.01,
        dict(cov_by_frac={r["frac"]: round(r["cov"], 4) for r in noisy},
             cov_null_by_frac={r["frac"]: round(r["cov_null"], 4) for r in noisy},
             verdict_by_frac={r["frac"]: r["verdict"] for r in noisy}))
    add("COV9_amplitude_independence",
        "cov 与台阶幅度无关（每档 f 的 5 个幅度读数极差 <= 0.01）",
        all(max(g["cov"] for g in grid if g["frac"] == f)
            - min(g["cov"] for g in grid if g["frac"] == f) <= 0.01 for f in fracs),
        {f: round(max(g["cov"] for g in grid if g["frac"] == f)
                  - min(g["cov"] for g in grid if g["frac"] == f), 4) for f in fracs})
    res["gates"] = gates

    out = UNIT / "results" / "seam_gate_coverage.json"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(res, indent=2, ensure_ascii=False), encoding="utf-8")
    print("== 接缝门沿边覆盖率诊断量 ==")
    for r in gates:
        print("  [%s] %-34s %s" % ("PASS" if r["ok"] else "FAIL", r["id"], r["value"]))
    print("  -> %s" % out)
    return 0 if all(r["ok"] for r in gates) else 1


if __name__ == "__main__":
    sys.exit(main())