# -*- coding: utf-8 -*-
"""归零负例 N2b′ / N2c′ 的**缺陷注入自检**（能红能绿）。

背景
====
`sky_plane_zero_negative.py` 的 N2b 与 N2c 此前是两条无效判据：N2b 检的是
「真值是否为 0」这一**存在性事实**（缺陷注入改变的是量值不是存在性 ⇒ 6/6 缺陷全绿），
N2c 是**下界型**（只有被测量本身小于硬编码阈值时才红）。重写后的两条：

| 判据 | 型态 | 锚 |
|---|---|---|
| `N2b` `N2b_apply_field_identity` | 逐像素场级等式 | 未施加路径的同一 δ 场 |
| `N2c` `N2c_injection_truth_match` | 过零点的仿射律 | 真值：注入的接缝差被 δ **完全**消除 |

本脚本对每条注入缺陷实跑这两条判据，验证「正确实现绿 / 注入缺陷红」。

纪律
====
- 只读调用生产 `sky_probe` 与公共库 `sci_c_common`；不改任何生产源码。
- 缺陷全部注入在**施加函数**上（`δ_k` 解出来之后、进马赛克之前），
  生产求解器本身不被改动；D1 注入在求解器输出上（模拟求解器返回带偏置的 δ）。
- 固定 seed；无网络；无时间/环境随机源；新增结果文件，不改写既有数字 JSON。

复现：python3 实验/additive-sky-seamless/code/g08_defect_probe.py
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

import numpy as np

CODE = Path(__file__).resolve().parent
sys.path.insert(0, str(CODE))
import sci_c_common as S  # noqa: E402
import sky_plane_zero_negative as Z  # noqa: E402

UNIT = CODE.parent
BI = S.BOUNDARIES.index(Z.INJECT_X)

STEP_COEFFS = {k: dict(Z.COEFF_SAME[k], step_x_from=Z.INJECT_X,
                       step_e=Z.DELTA_INJECT_E) for k in S.FRAME_ORDER}
OFF_E = Z.OFF_INJECT_E


# ---------------------------------------------------------------- 缺陷定义
def d_identity(d):
    return d


def d_bias(d):
    """D1：求解器返回 `δ_k + 0.5 e⁻`（无效应世界也返回非零 δ）。"""
    return d + 0.5


def d_zero(d):
    """D2：δ_k 算出但**不施加**（叠加用零校正）。"""
    return d * 0.0


def d_sign(d):
    """D3：δ_k 符号反转。"""
    return -d


def d_x2(d):
    """D4：δ_k × 2（过校正）。"""
    return d * 2.0


def d_quarter(d):
    """D5：δ_k × 0.25（欠校正）。"""
    return d * 0.25


def d_shift_frame(d):
    """D8：δ_k 施加到错误的帧（帧序错位一格）。"""
    return np.roll(d, 1, axis=0)


def d_weighted(d):
    """D9：施加时用了非均匀权重（覆盖集归一化被改错）。"""
    w = np.linspace(1.0, 2.0, d.shape[1])[None, :, None]
    return d * w


DEFECTS = [
    ("D0_baseline", "正确实现", d_identity, True, None),
    ("D1_solver_bias", "求解器返回 δ + 0.5 e-", d_bias, False, None),
    ("D2_not_applied", "δ 算出但不施加", d_zero, False, None),
    ("D3_sign_flip", "δ 符号反转", d_sign, False, None),
    ("D4_double", "δ × 2（过校正）", d_x2, False, None),
    ("D5_quarter", "δ × 0.25（欠校正）", d_quarter, False, None),
    ("D6_inject_1p2", "注入幅度降到 1.2 e-（真值本身变小）", d_identity, True, 1.2),
    ("D7_inject_0p9", "注入幅度降到 0.9 e-（真值本身变小）", d_identity, True, 0.9),
    ("D8_frame_mismatch", "δ 施加到错误的帧", d_shift_frame, False, None),
    ("D9_wrong_weights", "施加时用非均匀权重", d_weighted, False, None),
]


def eval_n2a(defect):
    """无效应世界：δ 必须逐点归零。"""
    _d, _e, _o, aux = Z.production_arm(Z.COEFF_SAME, "defect_n2a")
    applied = defect(aux["delta_true"])
    return float(np.max(np.abs(applied))), applied


def eval_n2b(defect):
    """基外盒装台阶世界：逐像素场级恒等式。"""
    _d, _e, _o, aux = Z.production_arm(STEP_COEFFS, "defect_n2b_anchor")
    aux = dict(aux)
    aux["delta_of_alpha"] = (lambda a, _df=defect, _b=aux["delta_true"]:
                             _df(_b) * float(a))
    return Z.n2b_field_identity(aux, BI)


def eval_n2c(defect, off_e=OFF_E):
    """逐帧常数偏置世界：过零点的仿射律。"""
    return Z.n2c_truth_match(Z.n2c_scan(off_e, defect=defect))


def main():
    res = dict(
        note="N2b′/N2c′ 的缺陷注入自检；defects 只注入施加函数（δ 解出之后）",
        n2b_tol_rel=Z.CLOSURE_TOL_REL, n2c_tol_step=Z.STEP_TOL_E,
        n2c_tol_aff=Z.AFFINE_TOL_REL, injected_offset_e=OFF_E, rows=[])
    for key, desc, fn, expect_green, off_e in DEFECTS:
        a_max, _ap = eval_n2a(fn)
        b = eval_n2b(fn)
        c = eval_n2c(fn, off_e if off_e is not None else OFF_E)
        row = dict(defect=key, desc=desc, expect_green=bool(expect_green),
                   n2a_max_abs_delta=a_max, n2a_ok=bool(a_max <= Z.PROBE_TOL_E),
                   n2b_worst_rel=b["worst_rel"], n2b_ok=b["ok"],
                   n2c_step_at_alpha1=c["step_at_alpha1"],
                   n2c_affine_rel=c["affine_residual_rel"], n2c_ok=c["ok"])
        row["n2b_red"] = not b["ok"]
        row["n2c_red"] = not c["ok"]
        row["any_red"] = bool((not row["n2a_ok"]) or row["n2b_red"] or row["n2c_red"])
        res["rows"].append(row)
        print("  %-20s expect_green=%-5s N2a=%-4s(%.3e) N2b=%-4s(%.3e) N2c=%-4s(|s(1)|=%.4e aff=%.3e)"
              % (key, expect_green, "PASS" if row["n2a_ok"] else "FAIL", a_max,
                 "PASS" if b["ok"] else "FAIL", b["worst_rel"],
                 "PASS" if c["ok"] else "FAIL",
                 abs(c["step_at_alpha1"]), c["affine_residual_rel"]))

    gates = []

    def add(gid, desc, ok, value):
        gates.append(dict(id=gid, desc=desc, ok=bool(ok), value=value))

    base = res["rows"][0]
    small = [r for r in res["rows"] if r["defect"] in ("D6_inject_1p2", "D7_inject_0p9")]
    bad = [r for r in res["rows"] if not r["expect_green"]]

    add("DEF1_baseline_all_green",
        "D0 正确实现下 N2a/N2b/N2c 全绿",
        base["n2a_ok"] and base["n2b_ok"] and base["n2c_ok"],
        dict(n2a=base["n2a_ok"], n2b=base["n2b_ok"], n2c=base["n2c_ok"]))
    add("DEF2_n2b_reds_on_all_value_defects",
        "N2b 对每个量值型缺陷（不施加/反号/×2/×0.25/错帧/错权重）判红",
        all(r["n2b_red"] for r in bad if r["defect"] != "D1_solver_bias"),
        {r["defect"]: r["n2b_worst_rel"] for r in bad if r["defect"] != "D1_solver_bias"})
    add("DEF3_n2c_reds_on_all_value_defects",
        "N2c 对每个量值型缺陷判红",
        all(r["n2c_red"] for r in bad if r["defect"] != "D1_solver_bias"),
        {r["defect"]: (round(r["n2c_step_at_alpha1"], 6),
                       round(r["n2c_affine_rel"], 6))
         for r in bad if r["defect"] != "D1_solver_bias"})
    add("DEF4_n2a_catches_solver_bias",
        "D1（求解器偏置）由 N2a 判红（N2b′/N2c′ 不覆盖求解器侧）",
        (not [r for r in res["rows"] if r["defect"] == "D1_solver_bias"][0]["n2a_ok"]),
        [r["n2a_max_abs_delta"] for r in res["rows"] if r["defect"] == "D1_solver_bias"])
    add("DEF5_smaller_truth_stays_green",
        "真值本身变小（1.2 / 0.9 e-）时两条新判据**仍绿**（不是下界型）",
        all(r["n2b_ok"] and r["n2c_ok"] for r in small),
        [(r["defect"], r["n2b_ok"], r["n2c_ok"]) for r in small])
    add("DEF6_every_defect_red_somewhere",
        "每个注入缺陷至少被三条判据之一判红",
        all(r["any_red"] for r in res["rows"] if not r["expect_green"]),
        {r["defect"]: r["any_red"] for r in res["rows"] if not r["expect_green"]})
    res["gates"] = gates

    out = UNIT / "results" / "g08_defect_probe.json"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(res, indent=2, ensure_ascii=False), encoding="utf-8")
    print("== 归零负例缺陷注入自检 ==")
    for r in gates:
        print("  [%s] %-42s %s" % ("PASS" if r["ok"] else "FAIL", r["id"], r["value"]))
    print("  -> %s" % out)
    return 0 if all(r["ok"] for r in gates) else 1


if __name__ == "__main__":
    sys.exit(main())