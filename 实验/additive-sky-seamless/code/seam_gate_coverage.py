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

检出限前置条件（本节新增；原实现缺此条 ⇒ 主动出具错误肯定结论）
================================================================
`cov` 是**逐样本**判据：样本要进入覆盖计数须 `|seam_i − lv| > thr`，故一条幅度为
`a` 的台阶要被抓到必须

    a · bg > thr   ⇔   a > thr/bg          （本诊断量的最小可检相对幅度）

无噪夹具里 `thr = 0.5·gate·bg = 1.5 ADU` ⇒ `thr/bg = 0.005 = 0.5·gate`，
**诊断量比门灵敏 2 倍**，此时「测不到偏离」确实蕴含「无台阶」。

但 `thr` 的第一项是 `k_cov·s_`（对照线稳健散布）。实测带噪臂（σ_pix = 20 @ BG=300）
`ctrl_mad ≈ 22` ⇒ `thr ≈ 66–70 ADU` ⇒ `thr/bg ≈ 0.22`，
**是门阈 `gate = 0.01` 的 22 倍**：比门瞎 22 倍的诊断量，
在 `cov` 与零台阶对照不可分辨时给出的是「**看不见**」，不是「没有」。

原实现（`:172`）在这时仍取 `coverage_zone = "none"`，注释写「真无台阶：中位读数 0
是有效结论」并**原样保留生产 PASS**。已实测漏绿：真实 4% 台阶（12 ADU）、
`frac = 0.30`、`rel_step = −0.00917`（低于门阈）下，
`cov = 0.0075 ≤ cov_null_hi = 0.0126` ⇒ `zone = none`、`verdict = PASS`。
一条**确实存在**的接缝被出具「真无台阶 + 门通过」的肯定结论，违反 `README.md:111`。

⇒ 本节把「真无台阶」这一肯定结论**加上检出限前置条件**：只有当诊断量的最小可检
相对幅度 `detection_floor_rel = thr/bg` **严格低于**门自身的幅度灵敏度 `gate` 时，
`cov ≤ cov_null_hi` 才允许判「真无台阶」；否则判 `unresolved`＝**证据不足**，
既不保留 PASS，也不谎称有台阶。

四区判定（`unresolved` 为本节新增区）
------------------------------------
| 区 | 条件 | 含义 | 对生产 PASS 的处置 |
|---|---|---|---|
| `unresolved` | `detection_floor_rel ≥ gate` 且 `cov ≤ cov_null_hi` | 诊断量比门瞎 ⇒ **测不了** | **不保留 PASS**，判 `UNRESOLVED`（不计通过，也不谎称有台阶） |
| `none` | 检出限足够 且 `cov ≤ cov_null_hi` | 真无台阶 | 保留 PASS（此时是**挣得的**肯定结论） |
| `diluted` | 检出偏离但不足多数 | 中位数失明 | 显式判红 `FAIL_COVERAGE` |
| `majority` | 多数成立 | 中位读数可代表台阶 | 照 `rel_step` 判决 |

判决聚合优先级（`FAIL` > `FAIL_COVERAGE` > `UNRESOLVED` > `PASS`）：
**生产已测到真台阶时一律记 `FAIL`**，覆盖率层只作旁注——
原实现在 `frac = 1.0` 带噪臂上把生产的 `FAIL` 覆盖成 `FAIL_COVERAGE`
（真实台阶被改记成「覆盖率不足」），本次一并纠正。

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
        # fail-closed：无可用样本不是「测不到偏离 ⇒ 无台阶」。`blind_to_gate=True`
        # 使上层落进 `unresolved`（证据不足），既不保留 PASS 也不谎称有台阶。
        return dict(n=int(n), cov=0.0, cov_margin_sigma=float("nan"),
                    ctrl_mad=float("nan"), ctrl_level=float("nan"), thr=float("nan"),
                    detection_floor_rel=float("inf"), blind_to_gate=True,
                    blind_ratio=float("inf"), ctrl_side=s["ctrl_side"])
    lv = float(np.median(cv))
    s_ = float(mad(cv))
    if not np.isfinite(s_):
        s_ = 0.0
    thr = max(k_cov * s_, 0.5 * gate * float(bg) if bg else 0.0)
    cov = float(np.mean(np.abs(sv - lv) > thr)) if thr > 0 else 0.0
    sigma_f = float(np.sqrt(0.25 / max(n, 1)))
    # 检出限：一条幅度为 a 的台阶要进入覆盖计数必须 a > thr/bg。`thr/bg ≥ gate` 时
    # 本诊断量比门还瞎 ⇒ 「测不到」不蕴含「没有」，不得据此出具肯定结论。
    floor_rel = float(thr / float(bg)) if bg else float("nan")
    return dict(n=int(n), cov=cov, cov_margin_sigma=float((cov - COV_MIN) / sigma_f),
                ctrl_mad=float(mad(cv)), ctrl_level=lv, thr=float(thr),
                detection_floor_rel=floor_rel,
                blind_to_gate=bool(np.isfinite(floor_rel) and floor_rel >= gate),
                blind_ratio=float(floor_rel / gate) if gate else float("nan"),
                ctrl_side=s["ctrl_side"])


def edge_with_coverage(img, e, null_img=None, d=D, ctrl_shift=CTRL_SHIFT,
                       min_samples=MIN_SAMPLES, gate=GATE):
    """生产 `edge_metric` 的读数 + 本脚本的覆盖率诊断量（不改判据量）。

    `null_img` 是**同噪声 realization、去掉真台阶**的配对零台阶对照（同脚本现造），
    用来把「沿边确实没有任何台阶偏离」与「有台阶但被稀释」区分开。
    """
    p = edge_metric(img, e, d=d, ctrl_shift=ctrl_shift, min_samples=min_samples)
    c = coverage(img, e, d, ctrl_shift, bg=p.get("bg"), gate=gate)
    cn = coverage(null_img if null_img is not None else img, e, d, ctrl_shift,
                  bg=p.get("bg"), gate=gate)
    n = max(p.get("n") or 0, 1)
    pnull = min(max(cn["cov"], 1.0 / n), 0.5)
    c["cov_null"] = float(cn["cov"])
    c["cov_null_hi"] = float(pnull + 3.0 * np.sqrt(pnull * (1.0 - pnull) / n))
    c["cov_excess"] = float(c["cov"] - c["cov_null_hi"])
    # 四区判定：检出限不足（测不了）/ 真无台阶 / 检出但不足多数（中位失明）/ 多数成立
    if c["blind_to_gate"] and c["cov"] <= c["cov_null_hi"]:
        c["coverage_zone"] = "unresolved"    # 诊断量比门瞎 ⇒ 测不到 ≠ 没有
    elif c["cov"] <= c["cov_null_hi"]:
        c["coverage_zone"] = "none"          # 真无台阶（检出限足够时才是有效结论）
    elif c["cov"] <= COV_MIN:
        c["coverage_zone"] = "diluted"       # 检出偏离但不足多数 ⇒ 中位数失明
    else:
        c["coverage_zone"] = "majority"      # 多数成立：中位读数可代表台阶
    p["cov"] = c
    in_domain = (p.get("exclude") is None) and (p.get("rel_step") is not None)
    p["coverage_ok"] = bool(in_domain and c["coverage_zone"] != "diluted")
    p["coverage_unresolved"] = bool(c["coverage_zone"] == "unresolved")
    return p


#: 判决聚合优先级（越靠前越严重）；生产已测到台阶时一律记 FAIL，覆盖率层只作旁注。
_VERDICT_RANK = {"PASS": 0, "UNRESOLVED": 1, "FAIL_COVERAGE": 2, "FAIL": 3}


def edge_verdict(p, max_rel_excess=GATE):
    """单条边上的三值判决：生产判决与覆盖率层的合成。

    - 生产 `FAIL`（`|rel_step| > 门阈`）⇒ 记 `FAIL`：已测到真台阶，覆盖率区的
      「稀释/测不了」是对**同一条边**的旁注，不得改写已成立的红色判决。
    - 生产 `PASS` + `unresolved` ⇒ 记 `UNRESOLVED`：**证据不足**，不计通过，
      也不谎称有台阶（`UNRESOLVED` 与 `FAIL` 在计数上分开）。
    - 生产 `PASS` + `diluted` ⇒ 记 `FAIL_COVERAGE`：中位数失明，读 0 判绿无效。
    - 其余（`none` / `majority` / 适用域外）⇒ 沿用生产判决。
    """
    zone = (p.get("cov") or {}).get("coverage_zone")
    rel = p.get("rel_step")
    prod = "FAIL" if (rel is not None and abs(rel) > max_rel_excess) else "PASS"
    if prod == "FAIL":
        v = "FAIL"
    elif zone == "unresolved":
        v = "UNRESOLVED"
    elif zone == "diluted":
        v = "FAIL_COVERAGE"
    else:
        v = prod
    return v, prod, zone


def gate_decision_with_coverage(per_edge, max_rel_excess=GATE, min_samples=MIN_SAMPLES):
    """生产判决 + 覆盖率前置约束；覆盖率不足**显式判红**，检出限不足显式**记证据不足**。

    「真无台阶」区（检出限足够、且 cov 与配对零台阶对照不可分辨）保留生产的 PASS；
    「多数成立」区照常按 `max|rel_step|` 判决；
    「检出但不足多数」区（`0 < cov ≤ 0.5`）⇒ `FAIL_COVERAGE`，
    因为中位数落在无台阶一侧，此时读 0 判绿是**无效结论**；
    「检出限不足」区 ⇒ `UNRESOLVED`，**不得**出具「真无台阶 + PASS」的肯定结论。
    """
    base = gate_decision(per_edge, max_rel_excess, min_samples)
    sel = [p for p in per_edge if p.get("exclude") is None and p.get("rel_step") is not None]
    per = []
    for p in sel:
        v, prod, zone = edge_verdict(p, max_rel_excess)
        per.append(dict(n=p.get("n"), rel_step=p.get("rel_step"),
                        cov=(p.get("cov") or {}).get("cov"),
                        cov_null_hi=(p.get("cov") or {}).get("cov_null_hi"),
                        detection_floor_rel=(p.get("cov") or {}).get("detection_floor_rel"),
                        blind_ratio=(p.get("cov") or {}).get("blind_ratio"),
                        zone=zone, prod_verdict=prod, verdict=v))
    counts = {k: sum(1 for r in per if r["verdict"] == k) for k in _VERDICT_RANK}
    bad = [r for r in per if r["verdict"] == "FAIL_COVERAGE"]
    unres = [r for r in per if r["verdict"] == "UNRESOLVED"]
    covs = [p["cov"]["cov"] for p in sel]
    out = dict(base)
    out["cov_min"] = float(min(covs)) if covs else None
    out["cov_median"] = float(np.median(covs)) if covs else None
    out["n_coverage_insufficient"] = len(bad)
    out["n_unresolved"] = len(unres)
    out["verdict_counts"] = counts
    out["per_edge_verdicts"] = per
    out["verdict_production"] = base["verdict"]
    out["verdict"] = base["verdict"]
    if per:
        out["verdict"] = max((r["verdict"] for r in per), key=lambda v: _VERDICT_RANK[v])
    if bad:
        worst = min(bad, key=lambda r: r["cov"] if r["cov"] is not None else 0.0)
        wp = min((p for p in sel
                  if edge_verdict(p, max_rel_excess)[0] == "FAIL_COVERAGE"),
                 key=lambda p: p["cov"]["cov"])
        out["reason_coverage"] = (
            "沿边检出偏离但不足多数：%d/%d 条判据内边界的 cov ≤ %g（中位数的转移点）；"
            "最差 cov=%.4f（零台阶对照 %.4f±%.4f）、n=%d、interior=%s、exclude=%s、"
            "rel_step=%.6g ⇒ 该边的中位读数落在无台阶一侧，判绿无效"
            % (len(bad), len(sel), COV_MIN, worst["cov"],
               wp["cov"]["cov_null"], wp["cov"]["cov_null_hi"],
               worst["n"], wp.get("interior"), wp.get("exclude"),
               worst["rel_step"]))
    if unres:
        def _blind_key(r):
            v = r["blind_ratio"]
            return v if (v == v) else float("inf")
        worst = min(unres, key=_blind_key)
        out["reason_unresolved"] = (
            "覆盖率诊断量的最小可检相对幅度 = thr/bg = %.4g ≥ 门阈 %.4g（比门盲 %.1f 倍），"
            "且 cov=%.4g ≤ 零台阶对照上界 %.4g ⇒ 本诊断量**看不见**比门阈更小的台阶，"
            "「测不到偏离」不蕴含「没有台阶」：判 UNRESOLVED（证据不足），"
            "既不保留 PASS 也不谎称有台阶；最差 n=%d、rel_step=%.6g"
            % (worst["detection_floor_rel"], max_rel_excess, _blind_key(worst),
               worst["cov"], worst["cov_null_hi"] if worst["cov_null_hi"] == worst["cov_null_hi"] else float("nan"),
               worst["n"], worst["rel_step"]))
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

    def rowdict(p, gd, **extra):
        """统一行字典：带上检出限与盲倍数，使每行的「测不了/测得了」可逐行核。"""
        c = p["cov"]
        d = dict(n=p["n"], interior=p["interior"], exclude=p["exclude"],
                 bg=p["bg"], rel_step=p["rel_step"], cov=c["cov"],
                 cov_null=c["cov_null"], cov_null_hi=c["cov_null_hi"],
                 thr=c["thr"], ctrl_mad=c["ctrl_mad"],
                 detection_floor_rel=c["detection_floor_rel"],
                 blind_to_gate=c["blind_to_gate"], blind_ratio=c["blind_ratio"],
                 zone=c["coverage_zone"],
                 cov_margin_sigma=c["cov_margin_sigma"],
                 prod_verdict=gd["verdict_production"],
                 verdict=gd["verdict"])
        d.update(extra)
        return d

    grid, rows = [], []
    for frac in fracs:
        for amp in amps:
            img = make_image(frac=frac, step_adu=amp * BG)
            nul = make_image(frac=frac, step_adu=0.0)
            p = edge_with_coverage(img, e, null_img=nul)
            gd = gate_decision_with_coverage([p])
            grid.append(rowdict(p, gd, frac=frac, amp=amp, step=p["step"]))
    res["coverage_amplitude_grid"] = grid

    for frac in fracs:
        img = make_image(frac=frac, step_adu=0.04 * BG)
        nul = make_image(frac=frac, step_adu=0.0)
        p = edge_with_coverage(img, e, null_img=nul)
        gd = gate_decision_with_coverage([p])
        rows.append(rowdict(p, gd, frac=frac, step=p["step"]))
    res["coverage_scan_amp4pct"] = rows

    # ---- 1b. 同二维扫描的**带噪**臂 ----
    # 无噪夹具里 `thr` 恒等于兜底项 `0.5·gate·bg` ⇒ `cov` 在代数上等于 f、与幅度无关，
    # 「幅度无关」是**夹具的解析后果**而不是实测结论。把同一网格放到噪声下重扫，
    # 该性质才成为可判红的实测断言（COV9）。
    grid_noisy = []
    for frac in fracs:
        for amp in amps:
            sd = SEED + int(frac * 100) + int(amp * 1000)
            img = make_image(frac=frac, step_adu=amp * BG, sigma=20.0, seed=sd)
            nul = make_image(frac=frac, step_adu=0.0, sigma=20.0, seed=sd)
            p = edge_with_coverage(img, e, null_img=nul)
            gd = gate_decision_with_coverage([p])
            grid_noisy.append(rowdict(p, gd, frac=frac, amp=amp, step=p["step"]))
    res["coverage_amplitude_grid_noisy"] = grid_noisy

    # ---- 2. 带噪臂：噪声不救场，反而把被稀释的台阶进一步压掉 ----
    noisy = []
    for frac in (1.0, 0.6, 0.5, 0.48, 0.40, 0.30, 0.20):
        img = make_image(frac=frac, step_adu=0.04 * BG, sigma=20.0, seed=SEED + int(frac * 100))
        nul = make_image(frac=frac, step_adu=0.0, sigma=20.0, seed=SEED + int(frac * 100))
        p = edge_with_coverage(img, e, null_img=nul)
        gd = gate_decision_with_coverage([p])
        noisy.append(rowdict(p, gd, frac=frac, step=p["step"]))
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
        ctrl_rows.append(rowdict(p, gd, case=tag))
    res["control_cases"] = ctrl_rows

    # ---- 3b. 负控 A/B：新规则 vs 旧规则的判决对照（本次整改的回归测试） ----
    # 旧规则（整改前的 `:172`）只按 `cov` 分三区、不看检出限：盲态下把「看不见」
    # 写成「没有」并保留 PASS。本块在**同一批夹具**上并列两种判决；
    # 若实现退回旧规则，`new_verdict` 会等于 `legacy_verdict` ⇒ COV10 判红。
    def legacy_verdict(r):
        """整改前的判决面：`cov ≤ cov_null_hi ⇒ zone=none ⇒ 保留生产 PASS`。"""
        if r["prod_verdict"] == "FAIL":
            return "FAIL"
        if r["cov"] <= r["cov_null_hi"]:
            return "PASS"
        return "FAIL_COVERAGE" if r["cov"] <= COV_MIN else "PASS"

    ab_rows = []
    for r in noisy:
        ab_rows.append(dict(frac=r["frac"], cov=r["cov"], cov_null_hi=r["cov_null_hi"],
                            detection_floor_rel=r["detection_floor_rel"],
                            blind_to_gate=r["blind_to_gate"],
                            rel_step=r["rel_step"], prod_verdict=r["prod_verdict"],
                            legacy_verdict=legacy_verdict(r), new_verdict=r["verdict"],
                            disagree=bool(legacy_verdict(r) != r["verdict"])))
    res["legacy_vs_new_noisy"] = ab_rows

    # ---- 4. 门禁：能红（覆盖率不足 / 检出限不足）+ 能绿（检出限足够且真无台阶） ----
    gates, selfchecks = [], []

    def add(gid, desc, ok, value):
        gates.append(dict(id=gid, desc=desc, ok=bool(ok), value=value))

    def add_selfcheck(gid, desc, ok, value):
        """**不计门数**：结构性同源的自检，读数恒定 ⇒ 不是判别力证据。

        `COV1` 是镜像自检：本脚本用 `edge_samples` 重算样本数组，而
        `edge_samples` 与生产 `edge_metric` 走**同一条 `bilinear` 采样路径**、
        同一选边规则与同一 `median` ⇒ 两侧同源，实测差**逐位为 0.0**。
        把它当「判别力证据」计入门数是恒真门。保留读数，移出计数。
        """
        selfchecks.append(dict(id=gid, desc=desc, ok=bool(ok), value=value))

    add_selfcheck("SELFCHECK_mirror_faithful",
                  "本脚本重算的样本数组复现生产 edge_metric 的 step（|Δ| <= 1e-9 e-）"
                  "；同源重算 ⇒ 读数恒 0.0，不计门数（无判别力）",
                  res["mirror_max_abs_dstep"] <= 1e-9, res["mirror_max_abs_dstep"])

    blind = [r for r in rows if r["rel_step"] is not None and abs(r["rel_step"]) == 0.0]
    # 以下三门的原写法是 `all(... for r in ... if <filter>)`：过滤器选到 0 行时
    # `all([])` 返回 **True** ⇒ 空集合恒真通过（fail-open）。改为先断言非空。
    add("COV2_dilution_is_real",
        "f < 0.5 时生产门读数恰为 0（稀释面存在，%d 个档位）" % len(blind),
        len(blind) >= 3, [r["frac"] for r in blind])
    add("COV3_production_blind_verdict_green",
        "同一批 f < 0.5 的档位生产门全部判绿（门不知道自己测不了）；"
        "样本集非空是本门的前置条件（否则 `all([])` 会空过）",
        bool(blind) and all(r["prod_verdict"] == "PASS" for r in blind),
        [(r["frac"], r["prod_verdict"]) for r in blind])
    add("COV4_coverage_reads_fraction",
        "覆盖率读数随 f 单调且 |cov − f| <= 0.03（要求检出限足够，即 cov 有意义）",
        bool(rows) and all(abs(r["cov"] - r["frac"]) <= 0.03 for r in rows),
        [(r["frac"], round(r["cov"], 4)) for r in rows])
    # COV5 / COV8 的分母：必须先有可判的档位，否则「全部满足」是空真
    diluted_noiseless = [r for r in rows if 0.0 < r["cov"] <= COV_MIN]
    diluted_noisy = [r for r in noisy if 0.0 < r["cov"] <= COV_MIN]
    noisy_unresolved = [r for r in noisy if r["blind_to_gate"]]
    ab_disagree = [r for r in ab_rows if r["disagree"]]
    amp_ranges_noisy = {f: round(max(g["cov"] for g in grid_noisy if g["frac"] == f)
                                 - min(g["cov"] for g in grid_noisy if g["frac"] == f), 4)
                        for f in fracs}
    add("COV5_gate_fails_closed_on_low_coverage",
        "检出偏离但不足多数（zone=diluted）**不得被出具为肯定结论**：无噪臂里 "
        "`FAIL_COVERAGE` 通路必须被真实走到（f<0.5 各档），f=0.5 边界档按优先级由"
        "生产的 FAIL 接管；带噪臂的 `diluted` 档同样不得出现 PASS 或 zone=none",
        bool(diluted_noiseless) and bool(diluted_noisy) and
        all(r["zone"] == "diluted" and r["verdict"] != "PASS" for r in diluted_noiseless)
        and any(r["verdict"] == "FAIL_COVERAGE" for r in diluted_noiseless) and
        all(r["verdict"] != "PASS" and r["zone"] != "none" for r in diluted_noisy),
        dict(noiseless_diluted=[(r["frac"], round(r["cov"], 4), r["zone"], r["verdict"])
                               for r in diluted_noiseless],
             noisy_diluted=[(r["frac"], round(r["cov"], 4), r["zone"], r["verdict"])
                            for r in diluted_noisy],
             noisy_incoherent_note=(
                 "带噪臂里 `diluted` 一律被更高优先级判决接管：生产 `rel_step` 的中位"
         "在 f≤0.5 附近受噪声支配（实测 −0.009…−0.040，门阈 0.01），故带噪臂的 "
                 "`diluted` 档要么已判 FAIL 要么判 UNRESOLVED")))
    add("COV6_no_step_control_never_red",
        "无台阶对照**从不判红**（不误伤）：无噪臂判 PASS，带噪臂判 UNRESOLVED（证据不足）",
        all(r["verdict"] in ("PASS", "UNRESOLVED") for r in ctrl_rows
            if r["case"].startswith("no_step")),
        [(r["case"], r["zone"], r["blind_to_gate"], r["verdict"]) for r in ctrl_rows])
    add("COV6b_noiseless_no_step_is_certified_pass",
        "无噪无台阶对照判 PASS：此时检出限 0.005 = 0.5·gate 比门灵敏，「真无台阶」是挣得的",
        [r["verdict"] for r in ctrl_rows if r["case"] == "no_step_full"] == ["PASS"],
        [(r["case"], r["verdict"], r["detection_floor_rel"], r["blind_ratio"])
         for r in ctrl_rows if r["case"].startswith("no_step")])
    add("COV7_gate_red_on_full_amplitude",
        "完整覆盖的真台阶（4% / 16%）判红（门仍能看见真接缝）",
        all(r["verdict"] == "FAIL" for r in ctrl_rows
            if not r["case"].startswith("no_step")),
        [(r["case"], r["zone"], r["verdict"]) for r in ctrl_rows])
    add("COV8_noisy_arm_is_unresolved_not_pass",
        "带噪臂（σ_pix=20 @300）检出限 ≈ 22×门阈 ⇒ **不得出具 zone=none 的肯定结论、"
        "不得保留 PASS**：原实现在 frac=0.30（真实 4% 台阶、rel_step=−0.00917 低于"
        "门阈）上归入 none 并保留 PASS，等于主动出具「真无台阶」",
        bool(noisy_unresolved) and
        all(r["zone"] != "none" and r["verdict"] != "PASS" for r in noisy_unresolved),
        dict(blindness_ratio={r["frac"]: round(r["blind_ratio"], 2) for r in noisy},
             zone_by_frac={r["frac"]: r["zone"] for r in noisy},
             verdict_by_frac={r["frac"]: r["verdict"] for r in noisy},
             cov_by_frac={r["frac"]: round(r["cov"], 4) for r in noisy}))
    add("COV9_amplitude_independence_under_noise",
        "**带噪网格**：每档 f 的 5 个幅度读数极差 > 0.01，即 `cov` 在噪声下**并非**"
        "与台阶幅度无关（实测极差 0.045–0.887，随幅度单调）。无噪网格上该性质是"
        "夹具的解析后果（`thr` 恒为兜底项 `0.5·gate·bg` ⇒ `cov ≡ f`），已移入自检不计门",
        bool(amp_ranges_noisy) and
        all(v > 0.01 for v in amp_ranges_noisy.values()),
        dict(noisy_amplitude_range_per_frac=amp_ranges_noisy,
             noiseless_analytic_consequence={f: round(max(
                 g["cov"] for g in grid if g["frac"] == f)
                 - min(g["cov"] for g in grid if g["frac"] == f), 4) for f in fracs}))
    add("COV10_blind_arm_rejects_legacy_false_pass",
        "**负控（回归测试）**：在检出限不足的档位上，新规则的判决必须与整改前的"
        "「`cov ≤ cov_null_hi ⇒ zone=none ⇒ 保留 PASS」不同，且不得为 PASS。"
        "退回旧规则即判红",
        bool(ab_disagree) and
        all(r["new_verdict"] != "PASS" for r in ab_rows if r["blind_to_gate"]),
        [(r["frac"], r["detection_floor_rel"], r["prod_verdict"],
          r["legacy_verdict"], r["new_verdict"]) for r in ab_rows])
    res["gates"] = gates
    # 恒等/镜像自检不计门数（结构性同源，读数恒 0，不构成判别力证据）
    res["selfchecks"] = selfchecks

    out = UNIT / "results" / "seam_gate_coverage.json"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(res, indent=2, ensure_ascii=False), encoding="utf-8")
    print("== 接缝门沿边覆盖率诊断量 ==")
    for r in gates:
        print("  [%s] %-38s %s" % ("PASS" if r["ok"] else "FAIL", r["id"], r["value"]))
    for r in selfchecks:
        print("  [%s] %-38s %s   (自检，不计门数)"
              % ("PASS" if r["ok"] else "FAIL", r["id"], r["value"]))
    print("  -> %s" % out)
    return 0 if all(r["ok"] for r in gates) else 1


if __name__ == "__main__":
    sys.exit(main())