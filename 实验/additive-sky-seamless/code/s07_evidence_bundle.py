#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""S07 证据聚合器：把 run/ACSDSDOC-02/S07/results/ 的机器读数纯转写为
`实验/additive-sky-seamless/docs/S07_P5_GAIN_evidence.json`（机器可读，含
git_write_performed:false 与 base/source SHA）。不重算任何数字。

用法：python3 s07_evidence_bundle.py [--results <dir>] [--out <json>]
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
DEFAULT_RESULTS = os.path.join(REPO, "run", "ACSDSDOC-02", "S07", "results")
DEFAULT_OUT = os.path.join(REPO, "实验", "additive-sky-seamless", "docs",
                           "S07_P5_GAIN_evidence.json")


def sha256_file(p):
    try:
        return hashlib.sha256(open(p, "rb").read()).hexdigest()
    except OSError:
        return None


def load(p):
    try:
        with open(p) as f:
            return json.load(f)
    except OSError:
        return None


def git(*args):
    try:
        r = subprocess.run(["git", *args], cwd=REPO, capture_output=True, text=True)
        return r.stdout.strip() if r.returncode == 0 else None
    except Exception:
        return None


def cond_summary(d, key):
    if not d:
        return None
    s = d.get("summary", {}).get(key, {})
    v = d.get("verdict", {})
    return dict(tag=d.get("tag"), color_condition=d.get("color_condition"),
                psf_condition=d.get("psf_condition"), r_ap_px=d.get("r_ap_px"),
                n=s.get("n"),
                rms_A=s.get("rms_A", {}).get("median"),
                rms_AG_JOINT=s.get("rms_AG_JOINT", {}).get("median"),
                rms_AG_ORACLE_JOINT=s.get("rms_AG_ORACLE_JOINT", {}).get("median"),
                rms_AG_SHUFFLE_JOINT=s.get("rms_AG_SHUFFLE_JOINT", {}).get("median"),
                prod_rms_A=s.get("prod_rms_A", {}).get("median"),
                prod_rms_AG_JOINT=s.get("prod_rms_AG_JOINT", {}).get("median"),
                diff_A_minus_AG_JOINT=s.get("diff_A_minus_AG_JOINT"),
                diff_A_minus_AG_ORACLE_JOINT=s.get("diff_A_minus_AG_ORACLE_JOINT"),
                diff_A_minus_AG_SHUFFLE_JOINT=s.get("diff_A_minus_AG_SHUFFLE_JOINT"),
                gain_chi2_red=s.get("gain_chi2_red"),
                gain_p_eta0=s.get("gain_p_eta0"),
                gain_max_deta_over_sigma=s.get("gain_max_deta_over_sigma"),
                gain_eta=s.get("gain_eta"),
                seam_A=s.get("seam_A"), seam_AG_JOINT=s.get("seam_AG_JOINT"),
                seam_AG=s.get("seam_AG"),
                lowfreq_A=s.get("lowfreq_A"), lowfreq_AG_JOINT=s.get("lowfreq_AG_JOINT"),
                verdict=v)


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--results", default=DEFAULT_RESULTS)
    ap.add_argument("--out", default=DEFAULT_OUT)
    a = ap.parse_args(argv)
    res = {}
    for tag, sub in (("main", ""), ("psf_uniform", "psf_uniform"), ("color", "color")):
        res[tag] = load(os.path.join(a.results, sub, f"S07_gain_{tag}.json"))
    boundary = load(os.path.join(a.results, "S07_boundary_coverage.json"))
    wire = load(os.path.join(a.results, "S07_wire_audit.json"))

    out = dict(
        schema="ACSD-P5-S07-EVIDENCE/1",
        paper_id="P5",
        experiment_id="P5-B/D/F-S07",
        git_write_performed=False,
        branch=git("rev-parse", "--abbrev-ref", "HEAD"),
        base_sha=(res["main"] or {}).get("base_sha"),
        source_sha=(res["main"] or {}).get("source_sha"),
        head_at_bundle_time=git("rev-parse", "HEAD"),
        base_sha_note="本单元未做任何 git 写操作；base == source == 测量时刻 HEAD。"
                      "本单元开工时 HEAD=a73f577b0，实验运行时刻 HEAD=c93b4a6f9（期间其它轮次并发提交），"
                      "聚合时刻 HEAD 见 head_at_bundle_time；a73f577b0..head_at_bundle_time 对 lib/** 零改动"
                      "（`git diff --stat a73f577b0..HEAD -- lib/` 为空）⇒ 接线审计与探针构建对当前 HEAD 同样有效。",
        build_dir="build/linux-sci-s07",
        probe_sha256=(res["main"] or {}).get("probe_sha256"),
        script_sha256=(res["main"] or {}).get("script_sha256"),
        lib_sha256=(res["main"] or {}).get("lib_sha256"),
        predeclared=(res["main"] or {}).get("predeclared"),
        metrics_def=(res["main"] or {}).get("metrics_def"),
        conditions=dict(
            main=cond_summary(res["main"], "pos"),
            psf_uniform=cond_summary(res["psf_uniform"], "pos"),
            color=cond_summary(res["color"], "pos"),
        ),
        negative_world=dict(
            main=cond_summary(res["main"], "neg"),
            psf_uniform=cond_summary(res["psf_uniform"], "neg"),
            color=cond_summary(res["color"], "neg"),
        ),
        wire_audit=dict(verdict=(wire or {}).get("verdict"),
                        n_hits=(wire or {}).get("n_hits")),
        boundary_coverage=boundary,
        evidence_level="PHYSICAL_FORWARD(synthetic analytic worlds) + PRODUCTION(read-only code audit)",
        status=("FAIL" if (res["main"] or {}).get("verdict", {}).get("PASS") is False else
                ("PASS" if (res["main"] or {}).get("verdict", {}).get("PASS") else "NOT_RUN")),
        limitations=[
            "世界为解析合成（逐像素独立噪声）；真实 drz 噪声已相关化 ⇒ 本结论不得迁移为真实数据可辨识性结论",
            "色差条件用注入的帧间色斜率 b_k 表示，不是真实滤光片/光谱色差",
            "PSF 条件用逐帧高斯 sigma 表示；真实 PSF 各向异性与重采样核未建模",
            "二维 g_i(x) 不在本轮；只做逐帧标量臂",
            "权重链 g^-2 缩放与增益估计不确定度传播属 P2 单元，未在本轮闭合",
            "真实帧乘性增益腿 NOT_RUN：无该运行的真实帧集与独立星表；本单元只做合成世界 + 生产只读接线审计",
        ],
        followups=[
            "用真实帧 + 独立星表（Gaia 匹配）重跑同一四臂协议",
            "在 N=32/16 px 对照偏移上重跑生产接缝门以评估被排除的 82 条边界（需保留成品 FITS）",
            "二维 g_i(x) 需独立空间增益锚与明确模型自由度后另立任务",
        ],
    )
    os.makedirs(os.path.dirname(a.out), exist_ok=True)
    with open(a.out, "w") as f:
        json.dump(out, f, indent=1, ensure_ascii=False)
    print(json.dumps({k: out[k] for k in ("schema", "git_write_performed", "base_sha",
                                          "status", "conditions")},
                     indent=1, ensure_ascii=False)[:2000])
    print("wrote", a.out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
