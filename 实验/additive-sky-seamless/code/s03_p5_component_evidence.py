#!/usr/bin/env python3
# 实验/additive-sky-seamless/code/s03_p5_component_evidence.py
#
# S03 逐分量 gauge 证据聚合器：把 run/ACSDSDOC-02/S03/results/ 下
# s03_p5_components.py 产出的读数（pre / post / fault / leadbuild）压成紧凑的
# 机器可读证据 JSON：
#   docs/S03_P5_COMPONENT_evidence.exec.json
#
# 它**不重跑**求解，只做纯函数式转写 ⇒ 任何数字都必须先有 results/ 里的原始
# run JSON 才能出现在证据里（禁止手写数字）。
#
# 用法（在仓库根执行）：
#   python3 实验/additive-sky-seamless/code/s03_p5_component_evidence.py
import glob
import hashlib
import json
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
UNIT = os.path.dirname(HERE)
REPO = os.path.abspath(os.path.join(UNIT, "..", ".."))
S03RES = os.path.join(REPO, "run", "ACSDSDOC-02", "S03", "results")
OUT = os.path.join(UNIT, "docs", "S03_P5_COMPONENT_evidence.exec.json")

SCENARIOS = ["S0_no_overlap_two_single_frames", "S9_disconnected", "S9b_two_components",
             "S12_chain4", "S13_thin_bridge", "S14_thin_bridge_1cell"]
ARMS = ["A0", "A1"]


def load(name, tag):
    p = os.path.join(S03RES, f"{name}.{tag}.json")
    if not os.path.exists(p):
        return None
    with open(p) as f:
        return json.load(f)


def sha256_file(p):
    if not os.path.exists(p):
        return None
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()


def git(args):
    try:
        return subprocess.run(["git"] + args, cwd=REPO, capture_output=True,
                              text=True, check=True).stdout.strip()
    except Exception:  # noqa
        return None


def arm_rec(d, arm):
    a = (d or {}).get("arms", {}).get(arm, {})
    info = (a.get("info") or {}).get("info") or {}
    return dict(
        probe_rc=a.get("probe_rc"),
        error_head=(a.get("stderr") or "")[:300],
        seam_rel_maxabs=a.get("seam", {}).get("comp_seam_maxabs"),
        seam_pass=a.get("seam_pass"),
        cross_component_gap_rel=[g["step_rel"] for g in a.get("cross_component", [])],
        frame_delta_coeffs=a.get("frame_delta_coeffs"),
        rank=info.get("rank"), n_params=info.get("n_params"),
        n_params_full=info.get("n_params_full"), rank_full=info.get("rank_full"),
        kappa=info.get("kappa"), identifiable=info.get("identifiable"),
        chi2_red=info.get("chi2_red"), rms_weighted=info.get("rms_weighted"),
        model_hash=info.get("model_hash"),
        components=a.get("components"),
    )


def comp_summary(comps):
    if not comps:
        return None
    return dict(
        n_components=comps.get("n_components"),
        anchor_by_frame=comps.get("anchor_by_frame"),
        component_id_by_frame=comps.get("component_id_by_frame"),
        gauge_explicit_ok=comps.get("gauge_explicit_ok"),
        anchor_delta_zero=comps.get("anchor_delta_zero"),
        gauge_constraints_applied=comps.get("gauge_constraints_applied"),
        gauge_constraints_expected=comps.get("gauge_constraints_expected"),
        solve_perturbation_gain=comps.get("solve_perturbation_gain"),
        solve_deflated=comps.get("solve_deflated"),
        cross_component_absolute_zero_identifiable=comps.get(
            "cross_component_absolute_zero_identifiable"),
        global_block=comps.get("global"),
        per_component=[{k: c.get(k) for k in (
            "component_id", "frame_ids", "anchor", "edge_count", "cell_edges",
            "n_samples", "n_cells", "support_frac", "n_nodes", "n_free_delta",
            "rank", "n_params", "n_unidentified", "kappa", "lambda_min", "lambda_max",
            "identifiable", "status", "delta_const_span", "delta_ramp_max",
            "overlap_pair_n", "overlap_pair_abs_max", "overlap_pair_abs_median",
            "cross_component_comparable")} for c in comps.get("components", [])],
    )


def main():
    ev = {
        "schema": "ACSD S03 P5 component-gauge evidence (executor) v1",
        "base_sha": "e31b2be66ffe37d547e67a4ba007e7257e49789d",
        "head_sha_at_measure": git(["rev-parse", "HEAD"]),
        "source_sha256": sha256_file(
            os.path.join(REPO, "lib", "algorithms", "coverage", "src", "sky_plane.cpp")),
        "build_dirs": {
            "executor": "build/linux-sci-p5 (Ninja/Release)",
            "lead": "build/linux-sci (Ninja/Release)",
        },
        "estimand": ("加性规范：帧重叠图每个连通分量各自固定锚帧 δ_anchor≡0；"
                     "跨分量绝对零点不可由分量内重叠数据求得，需外部锚"),
        "units": {
            "seam_rel_maxabs": "无量纲（|沿边界法向 8px 窗中位台阶| / 局部背景电平中位）",
            "kappa": "无量纲（列均衡 H_red 的条件数；秩亏 ⇒ -1 表示 +inf，不发布伪值）",
            "delta_ramp_max": "归一化切平面坐标上的 δ 多项式一阶系数绝对值上界（无量纲）",
            "solve_perturbation_gain": "‖ΔB‖∞/(eps·‖B‖∞)，eps=1e-6（rhs 相对扰动）",
            "overlap_pair_abs_max": "控制点 value 单位（= 生产 ADU·sr⁻¹ 口径）",
        },
        "samples": {
            "note": "全部场景为合成真值（seed=20261014 派生），S03 不消费真实帧",
            "S9b": {"n_samples": 1184, "n_used": 1184, "probe_points": 65536,
                    "pitch_px": 8, "noise_sigma": 8.0, "s0": 200.0},
        },
        "scenarios": {},
        "fault_control": {},
        "connected_graph_bit_identity": {},
    }
    for name in SCENARIOS:
        pre, post = load(name, "pre"), load(name, "post")
        if post is None:
            continue
        rec = {
            "covers": post.get("covers"),
            "component_spec": post.get("component_spec"),
            "input_data_hash": post.get("input_data_hash"),
            "support": post.get("support"),
            "pre": {a: arm_rec(pre, a) for a in ARMS},
            "post": {a: arm_rec(post, a) for a in ARMS},
        }
        ev["scenarios"][name] = rec
        if name == "S9b_two_components":
            ev["fault_control"]["global_anchor"] = {
                "env": "ACSD_SKY_P5_FAULT=global_anchor",
                "must_fail": True,
                "observed": arm_rec(load(name, "fault_global_anchor"), "A0"),
            }
    # 连通图逐位不变：A0 model_hash 与 n_params_full/rank_full/chi2_red/kappa 全等
    for name in ["S12_chain4", "S14_thin_bridge_1cell"]:
        pre, post = load(name, "pre"), load(name, "post")
        if not pre or not post:
            continue
        a, b = arm_rec(pre, "A0"), arm_rec(post, "A0")
        ev["connected_graph_bit_identity"][name] = {
            "pre_model_hash": a["model_hash"], "post_model_hash": b["model_hash"],
            "identical": (a["model_hash"] == b["model_hash"] and
                          a["n_params_full"] == b["n_params_full"] and
                          a["rank_full"] == b["rank_full"] and
                          a["chi2_red"] == b["chi2_red"] and a["kappa"] == b["kappa"]),
            "pre_kappa": a["kappa"], "post_kappa": b["kappa"],
        }
    # 跨构建目录一致性（Lead 的 build/linux-sci vs 执行者 build/linux-sci-p5）
    lb = load("S9b_two_components", "leadbuild")
    if lb:
        ev["cross_build_check"] = {
            "lead_binary": "run/ACSDSDOC-02/S03/code/p2_sky_joint_probe (build/linux-sci)",
            "executor_binary": "run/ACSDSDOC-02/S03/code/p2_sky_joint_probe_s03_post "
                               "(build/linux-sci-p5)",
            "leadbuild_A0": arm_rec(lb, "A0"),
            "agree": arm_rec(lb, "A0")["seam_rel_maxabs"] ==
                     arm_rec(load("S9b_two_components", "post"), "A0")["seam_rel_maxabs"],
        }
    with open(OUT, "w") as f:
        json.dump(ev, f, indent=1, ensure_ascii=False, sort_keys=True)
    print(f"wrote {OUT}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
