#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-01 判据自审（AGENTS §5「判据必须非退化」/ §9 自测要求）。

对每个门同时给出：
  * **实测状态**（用 results/ 里的真实数字判）；
  * **故障注入状态**（人为把数据改成"真值有/无效应"的反面，门必须翻转）。
一个门只有在"正常数据绿、注入故障红"时才被认定为**有证据资格**；
任何 all([]) == True 型的恒真空门会被 G0（比较对数 > 0）当场抓住。

用法：python3 code/exp01/negatives_selftest.py [--results DIR]
退出码：0 = 全部门行为符合预期；1 = 存在无证据资格或行为异常的门。
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any, Callable, Dict, List, Tuple

HERE = Path(__file__).resolve().parent
DEFAULT_RESULTS = HERE.parents[1] / "results"


def _load(results: Path, name: str) -> Dict[str, Any]:
    p = results / name
    if not p.exists():
        raise SystemExit("缺少结果文件：%s（先跑 code/exp01/run_all.sh）" % p)
    return json.loads(p.read_text(encoding="utf-8"))


Gate = Dict[str, Any]


def build_gates() -> List[Gate]:
    g: List[Gate] = []

    # G0：非退化前置门 —— 被比较的记录数必须 > 0
    def g0(d, fault=False):
        n = 0 if fault else (len(d["a_gaussian_mc"]) + len(d["a_hst"]) + len(d["a_real"]))
        return n > 0, "被比较的记录数 = %d" % n
    g.append({"id": "G0_非退化_样本数大于0", "src": "q1", "fn": g0})

    # G1：真值无效应 => 无裁剪低偏必须归零
    def g1(d, fault=False):
        row = d["a_gaussian_mc"][-1]
        v, sem = row["bias_noclip_mean"], row["bias_noclip_sem"]
        if fault:
            v = row["bias_mean"]
        return abs(v) <= 3.0 * sem, "无裁剪低偏 = %.6f（3σ = %.6f）" % (v, 3.0 * sem)
    g.append({"id": "G1_归零_无裁剪低偏为0", "src": "q1", "fn": g1})

    # G2：生产 recipe 的裁剪低偏必须被检出（判红类：不能是 0）
    def g2(d, fault=False):
        v = d["a_gaussian_mc"][-1]["bias_mean"]
        if fault:
            v = 0.0
        return abs(v) > 0.005, "生产 recipe 低偏 = %.6f（要求 |.| > 0.5%%）" % v
    g.append({"id": "G2_判红_裁剪低偏非零", "src": "q1", "fn": g2})

    # G3：语义歧义在真值无效应（RN=0）时必须精确归零
    def g3(d, fault=False):
        v = d["c_negative_controls"]["rel_dev_rn_zero"]
        if fault:
            v = d["c_negative_controls"]["rel_dev_with_rn"]
        return abs(v) <= 1e-12, "RN=0 时两支相对偏差 = %.3e" % v
    g.append({"id": "G3_归零_歧义项RN为零", "src": "q1", "fn": g3})

    # G4：阈值口径自测（δ=1% ⇒ B_e/RN² = 49.751，散粒口径）
    def g4(d, fault=False):
        row = [r for r in d["c_threshold_selftest"] if abs(r["delta"] - 0.01) < 1e-12][0]
        want = 1.0 / (1.01 ** 2 - 1.0)
        v = row["Be_over_RN2_totalrms"] if fault else row["Be_over_RN2_shot"]
        return abs(v - want) <= 1e-6, "B_e/RN² = %.6f（正确 %.6f）" % (v, want)
    g.append({"id": "G4_阈值口径_散粒非总rms", "src": "q1", "fn": g4})

    # G5：歧义项 A 随 δ 严格单调增（非恒真：比较对数必须 > 0）
    def g5(d, fault=False):
        ks = sorted(d["c_A_eff_at_F_ref"].keys(), key=float)
        vals = [d["c_A_eff_at_F_ref"][k] for k in ks]
        if fault:
            vals = list(reversed(vals))
        pairs = len(vals) - 1
        bad = sum(1 for i in range(pairs) if not vals[i] < vals[i + 1])
        return (pairs > 0 and bad == 0), "比较对 = %d，违反 = %d" % (pairs, bad)
    g.append({"id": "G5_非退化_歧义项随delta单调", "src": "q1", "fn": g5})

    # G6：决策规则两向可判红
    def g6(d, fault=False):
        st = dict(d["decision_selftest"])
        a = st["S_sys0_Aeff0_delta_star"]          # 无其它系统项 => 必须拒绝出数
        b = st["S_sys1pct_Aeff0_delta_star"]       # 歧义无效应 => 必须 = S_sys
        c = st["S_sys1pct_Aeff2pct_delta_star"]    # 歧义超预算 => 必须拒绝出数
        if fault:                                  # 注入：让规则"恒真"
            a, c = 0.01, 0.01
        ok = (a is None) and (abs(b - 0.01) < 1e-12) and (c is None)
        return ok, "S_sys=0 => %s ; A_eff=0 => %s ; A_eff>S_sys => %s" % (a, b, c)
    g.append({"id": "G6_非退化_决策规则可判红", "src": "q1", "fn": g6})

    # G7：配对性归零负例 —— delta PSF + 1 像素盒必须 == 1
    # **容差 0.05（不是 5e-3）**：该门是 MC 比较器，n=2000 次实现下 std 估计本身的相对
    # 标准误约 1.58%（独立审稿实测 400 次复算的 5%~95% 区间 = [0.9695, 1.0252]），
    # 容差 5e-3 小于该门自身的抽样噪声 ⇒ 绿灯靠运气（复现率仅 ~22%）。
    # 取 0.05 ≈ 3×抽样噪声：保留"真值无效应必须归零"的判别力（故障注入置 2.0 仍判红），
    # 又不在噪声上抖动。
    def g7(d, fault=False):
        v = 2.0 if fault else d["negatives"]["N1_delta_psf_1px"]["ratio"]
        return abs(v - 1.0) <= 0.05, "报告σ/实测std = %.6f（容差 0.05 = 3×MC 抽样噪声）" % v
    g.append({"id": "G7_归零_配对性deltaPSF", "src": "q2a", "fn": g7})

    # G8：正性截断偏置必须被检出（判红类）
    def g8(d, fault=False):
        v = 0.0 if fault else d["negatives"]["N2_no_clip"]["z_trunc"]
        return abs(v) > 3.0, "截断偏置 z = %.2f（要求 |z| > 3）" % v
    g.append({"id": "G8_判红_截断偏置", "src": "q2a", "fn": g8})

    # G9：去截断后必须无偏（归零类）
    def g9(d, fault=False):
        n2 = d["negatives"]["N2_no_clip"]
        v = n2["z_trunc"] if fault else n2["bias_untrunc_z"]
        return abs(v) <= 3.0, "未截断盒和 z = %.2f" % v
    g.append({"id": "G9_归零_去截断无偏", "src": "q2a", "fn": g9})

    # G10：同帧恒等对照必须严格 0
    def g10(d, fault=False):
        d = d["hst_cross_frame"]
        vals = [abs(v) for v in d["controls"]["same_frame_identity_control"].values()]
        if fault:
            vals = [0.1] + vals[1:]
        return max(vals) <= 1e-12, "同帧恒等对照 = %s" % vals
    g.append({"id": "G10_归零_同帧伪差为0", "src": "q2b", "fn": g10})

    # G11：现行 CUR 的配对错配必须被检出（判红类）
    def g11(d, fault=False):
        vals = [v["pairing"]["CUR_reported_over_measured"] for v in d["mc"].values()]
        if fault:
            vals = [1.0 for _ in vals]
        return all(abs(v - 1.0) > 0.02 for v in vals), "CUR 配对比 = %s" % [round(v, 4) for v in vals]
    g.append({"id": "G11_判红_现行错配", "src": "q2a", "fn": g11})

    # G12：盒和的跨帧（seeing）伪差必须显著非零
    def g12(d, fault=False):
        v = 0.0 if fault else d["hst_cross_frame"]["controls"]["box_trunc_p50_snr20_spread_over_seeing"]
        return v > 0.02, "盒和跨 seeing 伪差 = %.4f" % v
    g.append({"id": "G12_判红_盒和跨帧伪差", "src": "q2b", "fn": g12})

    # G13：方案 A 的跨帧伪差必须显著小于盒和
    def g13(d, fault=False):
        c = d["hst_cross_frame"]["controls"]
        a = c["psf_flux_p50_snr20_spread_over_seeing"]
        b = c["box_trunc_p50_snr20_spread_over_seeing"]
        if fault:
            a, b = b, a
        return a < 0.5 * b, "A = %.4f vs 盒和 = %.4f（比 = %.3f）" % (a, b, a / b)
    g.append({"id": "G13_判红_A优于盒和", "src": "q2b", "fn": g13})

    # G14：真实数据上方案 A 的样本代价必须被如实量化（>0 且 <100%）
    def g14(d, fault=False):
        v = d["real_products"]["psf_valid_frac_p50"]
        if fault:
            v = 1.0
        return 0.0 < v < 1.0, "PSF 拟合成功子集占比（中位）= %.5f" % v
    g.append({"id": "G14_非退化_样本代价被量化", "src": "q2b", "fn": g14})

    return g


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--results", default=str(DEFAULT_RESULTS))
    args = ap.parse_args()
    res = Path(args.results)
    src = {"q1": _load(res, "exp01_q1_delta.json"),
           "q2a": _load(res, "exp01_q2a_pairing.json"),
           "q2b": _load(res, "exp01_q2b_data.json")}
    gates = build_gates()
    rows, bad = [], 0
    for gt in gates:
        d = src[gt["src"]]
        ok, msg = gt["fn"](d, False)
        ok_f, msg_f = gt["fn"](d, True)
        qualified = bool(ok and (not ok_f))
        bad += 0 if qualified else 1
        rows.append({"id": gt["id"], "normal_green": bool(ok), "normal_msg": msg,
                     "fault_injected_green": bool(ok_f), "fault_msg": msg_f,
                     "qualified": qualified})
    out = {"n_gates": len(rows), "n_unqualified": bad, "gates": rows}
    print(json.dumps(out, ensure_ascii=False, indent=1))
    (res / "exp01_negatives_selftest.json").write_text(
        json.dumps(out, ensure_ascii=False, indent=1), encoding="utf-8")
    print("\n== 判据自审：%d 门，%d 门无证据资格 ==" % (len(rows), bad))
    return 1 if bad else 0


if __name__ == "__main__":
    raise SystemExit(main())
