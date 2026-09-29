#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""tier_verdict_gate.py —— 档位判词裁决器（可执行判据，能红能绿）。

配套正本 = docs/engineering/TIER_VERDICT_CRITERIA.md（判据书 §3 的机器实现）。
本文件不含任何科学公式、阈值或冻结定义：它只裁决"判词记录本身是否合规"，
不裁决"帧是否过闸"（过闸由冻结门自己判）。

用法
----
  python3 eng/tools/acceptance/tier_verdict_gate.py --record <record.json>
      [--repo-root .] [--json-out <out.json>]
  python3 eng/tools/acceptance/tier_verdict_gate.py --self-test

退出码
------
  0 = 判绿（记录合规；不代表档位已通过，只代表"这句话说得合规且有据"）
  1 = 判红（至少一条规则不满足）
  2 = fail-closed（记录不可读/不可解析/顶层字段缺失 => 门不可判）

设计纪律
--------
* 判据只加严不下放：所有新增约束都是"缺证据 => 判红"，不存在"缺证据 => 默认通过"。
* 每条规则都有正例与注入负例（--self-test 内含 12 条注入，逐条必须判红）。
* 判据能断言的范围必须与实际能保证范围一致：claim_scope ⊆ guaranteed_scope
  是硬规则（TV-11）；子集证据不得重判档位是硬规则（TV-04）。
"""
from __future__ import annotations

import argparse
import copy
import hashlib
import json
import pathlib
import sys

SCHEMA = "acsd.tier_verdict.v1"
EXIT_PASS, EXIT_RED, EXIT_FAILCLOSED = 0, 1, 2

# 封闭词表（改词表 = 改判据，须走变更流程；本文件是唯一实现点）
TIER_VERDICTS = {"VERIFIED", "NOT_VERIFIED", "BLOCKED", "NOT_ATTEMPTED"}
SUBSET_VERDICTS = {"SUBSET_VERIFIED", "SUBSET_NOT_VERIFIED", "SUBSET_REJECTED"}
BLOCK_OUTCOMES = {"complete", "partial", "all_failed", "first_frame_abort", "not_attempted"}
REJECTION_CLASSES = {"product_behavior", "defect", "unknown"}
NOT_ATTEMPTED_REASONS = {
    "BLOCKED_UPSTREAM",
    "OWNER_STANDDOWN",
    "ENV_MISSING",
    "RESOURCE",
    "SCOPE_EXCLUDED",
    "OTHER",
}
FRAME_VERDICTS = {"ACCEPT", "REJECT", "NOT_EVALUATED"}
ALLOWED_DENOMINATORS = {
    "evaluated_frames",
    "planned_frames",
    "total_px",
    "covered_px",
    "finite_px",
}
STAGES = ("normalize", "mosaic", "export")
TIER_STAGE_REQUIREMENT = {
    "frame_count": {
        "e2e_tier1_single_frame": ["normalize"],
        "e2e_tier2_8f_mosaic": ["normalize", "mosaic"],
        "e2e_tier3_full": ["normalize", "mosaic", "export"],
    },
    "coverage": {"coverage_tier_product": ["export"]},
}
TIER_AXES = {"frame_count", "coverage"}


def _rule(fid, ok, detail):
    return {"rule": fid, "ok": bool(ok), "detail": detail}


def _entry_weight(f):
    """逐帧记录的权重：单帧=1；显式帧组=len(frame_ids)；混合判词的组=0（非法）。"""
    if not isinstance(f, dict):
        return 0
    v = f.get("verdict")
    if v not in FRAME_VERDICTS:
        return 0
    fc = f.get("frame_count")
    if isinstance(fc, int) and fc >= 1:
        # 显式帧组：权重由 frame_count 声明（组内逐帧同判词），须有组标识
        return fc if (f.get("frame_ids") or f.get("frame_id")) else 0
    if v == "NOT_EVALUATED":
        return 1 if f.get("frame_id") else len(f.get("frame_ids") or [])
    if f.get("frame_id") and not f.get("frame_ids"):
        return 1
    ids = f.get("frame_ids")
    if isinstance(ids, list) and ids:
        return len(ids)
    return 0


def _sha256_file(p: pathlib.Path) -> str:
    h = hashlib.sha256()
    with p.open("rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


# ---------- 逐条规则 ----------
def tv01_declared(rec):
    """TV-01 分母声明：档位、轴、计划帧、计划帧清单指纹、分母词表、阶段面必须齐。"""
    d = rec.get("declared")
    if not isinstance(d, dict):
        return _rule("TV-01", False, "declared 块缺失或非对象（分母未声明 => 不可判）")
    miss = [k for k in ("planned_frames", "planned_frame_list_sha256", "denominators",
                        "stage_coverage", "axis", "tier")
            if d.get(k) in (None, "", [], {})]
    if miss:
        return _rule("TV-01", False, "declared 缺字段: " + ",".join(miss))
    if not isinstance(d["planned_frames"], int) or d["planned_frames"] <= 0:
        return _rule("TV-01", False, "planned_frames 必须是正整数")
    bad = [k for k, v in d["denominators"].items() if v not in ALLOWED_DENOMINATORS]
    if bad:
        return _rule("TV-01", False, "分母词表外取值: " + ",".join(sorted(bad)))
    if d["axis"] not in TIER_AXES:
        return _rule("TV-01", False, "axis 取值域外: " + repr(d["axis"]))
    unknown_stage = [s for s in d["stage_coverage"] if s not in STAGES]
    if unknown_stage:
        return _rule("TV-01", False, "stage_coverage 含未知阶段: " + ",".join(unknown_stage))
    return _rule("TV-01", True, "分母已声明：planned=%d denominators=%s"
                 % (d["planned_frames"], sorted(d["denominators"])))


def tv02_count_conservation(rec):
    """TV-02 计数守恒：accepted + rejected + not_attempted == planned_frames。"""
    c, d = rec.get("counts"), rec.get("declared") or {}
    if not isinstance(c, dict) or not isinstance(d, dict):
        return _rule("TV-02", False, "counts/declared 缺失")
    need = ("accepted", "rejected", "not_attempted")
    miss = [k for k in need if not isinstance(c.get(k), int)]
    if miss:
        return _rule("TV-02", False, "counts 缺整数项: " + ",".join(miss))
    tot = c["accepted"] + c["rejected"] + c["not_attempted"]
    planned = d.get("planned_frames")
    if tot != planned:
        return _rule("TV-02", False, "计数不守恒: %d+%d+%d=%d != planned %s"
                     % (c["accepted"], c["rejected"], c["not_attempted"], tot, planned))
    return _rule("TV-02", True, "计数守恒 %d == planned %s" % (tot, planned))


def tv03_block_outcome(rec):
    """TV-03 形态判定：必须先分清"全部失败"还是"首帧中止"，且与计数自洽。

    本门最关键的一条：all_failed 要求每一帧都被实际求值并拒绝；
    first_frame_abort 要求给出中止位置且未评估帧必须显式计数。
    把"块在首帧中止"写成"5 帧全拒"在本条判红。
    """
    bo = rec.get("block_outcome")
    c, d = rec.get("counts") or {}, rec.get("declared") or {}
    planned = d.get("planned_frames")
    frames = rec.get("frames")
    if bo not in BLOCK_OUTCOMES:
        return _rule("TV-03", False, "block_outcome 缺或取值域外: " + repr(bo))
    if not isinstance(frames, list):
        return _rule("TV-03", False, "frames 必须是逐帧记录数组（逐帧记录缺失 => 形态不可判）")
    n_eval = sum(_entry_weight(f) for f in frames
                 if isinstance(f, dict) and f.get("verdict") in ("ACCEPT", "REJECT"))
    n_not = sum(_entry_weight(f) for f in frames
                if isinstance(f, dict) and f.get("verdict") == "NOT_EVALUATED")
    zero = [i for i, f in enumerate(frames) if _entry_weight(f) == 0]
    if zero:
        return _rule("TV-03", False,
                     "第 %s 条逐帧记录既无 frame_id 也无同判词 frame_ids（形态不可判）"
                     % zero[:3])
    if n_eval != c.get("accepted", 0) + c.get("rejected", 0):
        return _rule("TV-03", False, "逐帧记录数(%d) 与 counts.accepted+rejected 不一致" % n_eval)
    if n_not != c.get("not_attempted", 0):
        return _rule("TV-03", False, "逐帧 NOT_EVALUATED 数(%d) 与 counts.not_attempted 不一致" % n_not)
    ab = rec.get("aborted_at")
    if bo == "first_frame_abort":
        if n_eval >= planned:
            return _rule("TV-03", False, "声明 first_frame_abort 但全部帧都被求值（形态判错）")
        if not isinstance(ab, dict) or ab.get("frame_id") in (None, ""):
            return _rule("TV-03", False, "first_frame_abort 必须给出 aborted_at.frame_id")
        if not isinstance(ab.get("frame_index"), int):
            return _rule("TV-03", False, "first_frame_abort 必须给出 aborted_at.frame_index")
        if c.get("rejected", 0) == planned:
            return _rule("TV-03", False,
                         "首帧中止被写成全部失败：rejected==planned 但存在未评估帧")
        return _rule("TV-03", True, "形态=首帧中止：已求值 %d，未评估 %d，中止于 %s"
                     % (n_eval, n_not, ab.get("frame_id")))
    if bo == "all_failed":
        if n_eval != planned:
            return _rule("TV-03", False, "声明 all_failed 但只有 %d/%s 帧被实际求值"
                         "（未求值的帧不能计为失败）" % (n_eval, planned))
        if c.get("accepted", 0) != 0 or c.get("rejected", 0) != planned:
            return _rule("TV-03", False, "all_failed 要求 accepted=0 且 rejected=planned")
        if ab:
            return _rule("TV-03", False, "all_failed 不应同时给出 aborted_at（形态互斥）")
        return _rule("TV-03", True, "形态=全部失败：%s/%s 帧被求值并拒绝" % (planned, planned))
    if bo == "complete":
        if n_eval != planned:
            return _rule("TV-03", False, "complete 要求 %s 帧全被求值，实得 %d" % (planned, n_eval))
        if c.get("not_attempted", 0) != 0:
            return _rule("TV-03", False, "complete 要求 not_attempted=0")
    if bo == "partial":
        if n_eval >= planned:
            return _rule("TV-03", False, "partial 要求存在未评估帧")
    if bo == "not_attempted":
        if n_eval != 0:
            return _rule("TV-03", False, "not_attempted 要求 0 帧被求值")
        if not rec.get("not_attempted_reasons"):
            return _rule("TV-03", False, "not_attempted 必须给 not_attempted_reasons")
    return _rule("TV-03", True, "形态=%s（已求值 %d / 未评估 %d）" % (bo, n_eval, n_not))


def tv04_tier_eligibility(rec):
    """TV-04 档位重判资格：子集证据不得把档位判成 VERIFIED。

    钉死的现实：5 帧小集 5 收只能支撑 SUBSET_*，不支撑 49 帧档重判。
    """
    v = rec.get("tier_verdict")
    d, c = rec.get("declared") or {}, rec.get("counts") or {}
    if v not in TIER_VERDICTS:
        return _rule("TV-04", False, "tier_verdict 取值域外: " + repr(v))
    planned = d.get("planned_frames") or 0
    n_eval = (c.get("accepted", 0) or 0) + (c.get("rejected", 0) or 0)
    sub = rec.get("subset_evidence")
    if v == "VERIFIED":
        if n_eval < planned:
            return _rule("TV-04", False,
                         "VERIFIED 但只求值 %d/%s 帧（子集证据不得重判档位）" % (n_eval, planned))
        need = TIER_STAGE_REQUIREMENT.get(d.get("axis"), {}).get(d.get("tier"))
        if need and not set(need).issubset(set(d.get("stage_coverage") or [])):
            return _rule("TV-04", False, "VERIFIED 但阶段面不全：本档要求 " + ",".join(need))
        return _rule("TV-04", True, "VERIFIED 的求值面已跑满（%d/%s）" % (n_eval, planned))
    if sub is not None:
        if not isinstance(sub, dict):
            return _rule("TV-04", False, "subset_evidence 必须是对象")
        if sub.get("verdict") not in SUBSET_VERDICTS:
            return _rule("TV-04", False, "subset_evidence.verdict 取值域外: "
                         + repr(sub.get("verdict")))
        for k in ("tier", "n_evaluated", "evidence"):
            if sub.get(k) in (None, ""):
                return _rule("TV-04", False, "subset_evidence 缺 " + k)
    return _rule("TV-04", True, "档位判词=%s（未主张重判）" % v)


def tv05_frame_records(rec):
    """TV-05 逐帧记录完备：每帧必须带 frame_id + verdict + 证据指针。"""
    frames = rec.get("frames")
    if not isinstance(frames, list) or not frames:
        return _rule("TV-05", False, "frames 缺失或为空（逐帧面不可复核）")
    bad = []
    for i, f in enumerate(frames):
        if not isinstance(f, dict):
            bad.append("[%d] 非对象" % i)
            continue
        if f.get("frame_id") in (None, "") and not f.get("frame_ids"):
            bad.append("[%d] 缺 frame_id/frame_ids" % i)
        if f.get("verdict") not in FRAME_VERDICTS:
            bad.append("[%d] verdict 取值域外: %r" % (i, f.get("verdict")))
        if not f.get("evidence"):
            bad.append("[%d] 缺证据指针" % i)
    ids = []
    for f in frames:
        if not isinstance(f, dict):
            continue
        ids.append(f.get("frame_id")) if f.get("frame_id") else ids.extend(f.get("frame_ids") or [])
    if len(ids) != len(set(ids)):
        return _rule("TV-05", False, "frame_id 重复（逐帧记录不可对账）")
    if bad:
        return _rule("TV-05", False, "%d 处缺项，例：%s" % (len(bad), "; ".join(bad[:3])))
    return _rule("TV-05", True, "%d 帧逐帧记录齐备" % len(frames))


def tv06_rejection_records(rec):
    """TV-06 拒绝记录：必带门 ID / 原文原因 / 分类（产品行为 or 缺陷）+ 出口 + 确定性。

    unknown 分类直接判红：拒绝若分不清是产品行为还是缺陷，就没有可执行的下一步。
    """
    out = []
    gate_hits = rec.get("frozen_gate_hits") or []
    for f in rec.get("frames") or []:
        if not isinstance(f, dict) or f.get("verdict") != "REJECT":
            continue
        fid = f.get("frame_id")
        r = f.get("rejection")
        if not isinstance(r, dict):
            out.append("%s: 缺 rejection 块" % fid)
            continue
        if r.get("gate_id") in (None, ""):
            out.append("%s: 缺 rejection.gate_id" % fid)
        if r.get("reason") in (None, ""):
            out.append("%s: 缺 rejection.reason（须逐字引产品日志）" % fid)
        cls = r.get("classification")
        if cls not in REJECTION_CLASSES:
            out.append("%s: classification 取值域外 %r" % (fid, cls))
        elif cls == "unknown":
            out.append("%s: classification=unknown（必须定为产品行为或缺陷）" % fid)
        if r.get("exit") not in ("stop_work", "registered"):
            out.append("%s: 缺 rejection.exit（stop_work / registered）" % fid)
        if r.get("gate_id") and r["gate_id"] not in str(gate_hits):
            out.append("%s: 冻结门 %s 未登记进 frozen_gate_hits" % (fid, r["gate_id"]))
        if r.get("deterministic_replay") is not True:
            out.append("%s: 缺 deterministic_replay=true（拒绝须证明是确定的）" % fid)
    if out:
        return _rule("TV-06", False, "%d 处拒绝记录不合格，例：%s" % (len(out), out[0]))
    n_rej = sum(_entry_weight(f) for f in (rec.get("frames") or [])
                if isinstance(f, dict) and f.get("verdict") == "REJECT")
    return _rule("TV-06", True, "%d 条拒绝记录齐备（含分类与出口）" % n_rej)


def tv07_not_attempted(rec):
    """TV-07 未尝试面怎么报：逐项原因码 + 负责人 + 证据；不得静默省略。"""
    items = rec.get("not_attempted")
    planned_na = ((rec.get("counts") or {}).get("not_attempted")) or 0
    if not items:
        if planned_na:
            return _rule("TV-07", False, "counts.not_attempted=%d 但未尝试面为空" % planned_na)
        return _rule("TV-07", True, "无未尝试项")
    if not isinstance(items, list):
        return _rule("TV-07", False, "not_attempted 必须是数组")
    bad = []
    for it in items:
        if not isinstance(it, dict):
            bad.append("非对象条目")
            continue
        if it.get("reason_code") not in NOT_ATTEMPTED_REASONS:
            bad.append("%r: reason_code 取值域外" % it.get("item"))
        if it.get("reason_code") == "OTHER" and not it.get("note"):
            bad.append("%r: OTHER 必须在 note 写明" % it.get("item"))
        if it.get("owner") in (None, ""):
            bad.append("%r: 缺 owner" % it.get("item"))
        if not it.get("evidence"):
            bad.append("%r: 缺 evidence" % it.get("item"))
    if bad:
        return _rule("TV-07", False, "%d 处未尝试项不合格，例：%s" % (len(bad), bad[0]))
    return _rule("TV-07", True, "%d 项未尝试面已按原因码登记" % len(items))


def tv08_evidence_pointers(rec, repo_root):
    """TV-08 证据面可解析：指针必须存在、非空；run/ 面必须带内容 sha256。"""
    bad, checked, entries = [], 0, []
    for f in rec.get("frames") or []:
        if isinstance(f, dict):
            entries += [(f.get("frame_id", "?"), e) for e in (f.get("evidence") or [])]
    for item in (rec.get("not_attempted") or []):
        if isinstance(item, dict):
            entries.append((item.get("item", "?"), item.get("evidence")))
    sub = rec.get("subset_evidence")
    if isinstance(sub, dict):
        entries.append(("subset_evidence", sub.get("evidence")))
    flat = []
    for who, ev in entries:          # 证据既可写成单对象也可写成数组，先归一
        if isinstance(ev, list):
            flat += [(who, e) for e in ev] or [(who, None)]
        else:
            flat.append((who, ev))
    for who, ev in flat:
        checked += 1
        ev_sha = None
        if isinstance(ev, str):
            ev_path = ev
        elif isinstance(ev, dict):
            ev_path = ev.get("path")
            ev_sha = ev.get("sha256")
            if not ev_path:
                bad.append("%s: 证据对象缺 path" % who)
                continue
            if ev_path.startswith("run/") and not ev_sha:
                bad.append("%s: run/ 下证据必须带 sha256（过程产物可再生 => 须留指纹）" % who)
        else:
            bad.append("%s: 证据类型非法" % who)
            continue
        p = repo_root / ev_path
        if not p.is_file():
            bad.append("%s: 证据不存在 %s" % (who, ev_path))
            continue
        if p.stat().st_size == 0:
            bad.append("%s: 证据为空文件 %s" % (who, ev_path))
            continue
        if ev_sha and _sha256_file(p) != ev_sha:
            bad.append("%s: 证据指纹不符 %s" % (who, ev_path))
    if bad:
        return _rule("TV-08", False, "%d/%d 处证据不合格，例：%s" % (len(bad), checked, bad[0]))
    return _rule("TV-08", True, "%d 个证据指针全部可解析且非空" % checked)


def tv09_coverage_axis(rec):
    """TV-09 覆盖度轴：covered/total/finite 三个分母都要声明且自洽。"""
    d = rec.get("declared") or {}
    cov = rec.get("coverage")
    if d.get("axis") != "coverage":
        return _rule("TV-09", True, "非覆盖度轴（覆盖度由帧数档记录承载）")
    if not isinstance(cov, dict):
        return _rule("TV-09", False, "覆盖度轴缺 coverage 块（分母不可判）")
    for k in ("covered_px", "total_px", "finite_px"):
        if not isinstance(cov.get(k), int) or cov[k] < 0:
            return _rule("TV-09", False, "coverage.%s 缺合法整数" % k)
    if cov["total_px"] <= 0:
        return _rule("TV-09", False, "total_px 必须为正（零分母不得判绿）")
    if not (0 < cov["covered_px"] <= cov["total_px"]):
        return _rule("TV-09", False, "covered_px=%d 不在 (0, total_px=%d]"
                     % (cov["covered_px"], cov["total_px"]))
    if cov["finite_px"] > cov["total_px"]:
        return _rule("TV-09", False, "finite_px > total_px")
    if "total_px" not in (d.get("denominators") or {}):
        return _rule("TV-09", False, "覆盖度轴未声明 total_px 作分母")
    return _rule("TV-09", True, "覆盖 %d/%d = %.4f（分母 total_px 已声明）"
                 % (cov["covered_px"], cov["total_px"], cov["covered_px"] / cov["total_px"]))


def tv10_rates_recomputable(rec):
    """TV-10 报出的每个比率必须能用声明的分母复算出来（防口径漂移/分母敏感）。"""
    d, c = rec.get("declared") or {}, rec.get("counts") or {}
    rates = rec.get("reported_rates") or {}
    bad = []
    for name, val in rates.items():
        den = (d.get("denominators") or {}).get(name)
        if den is None:
            bad.append("%s: 未在 declared.denominators 点名分母" % name)
            continue
        if den == "evaluated_frames":
            base = (c.get("accepted", 0) or 0) + (c.get("rejected", 0) or 0)
        elif den == "planned_frames":
            base = d.get("planned_frames")
        else:
            base = (rec.get("coverage") or {}).get(den)
        if not base:
            bad.append("%s: 分母 %s 在本记录中无取值（零分母）" % (name, den))
            continue
        num = val.get("numerator") if isinstance(val, dict) else None
        if not isinstance(num, (int, float)):
            bad.append("%s: 缺 numerator" % name)
            continue
        want = num / base
        got = val.get("value") if isinstance(val, dict) else None
        if not isinstance(got, (int, float)) or abs(got - want) > 1e-9:
            bad.append("%s: 报出 %s != 复算 %.9f（分母 %s）" % (name, got, want, den))
    if bad:
        return _rule("TV-10", False, "%d 处比率不可复算，例：%s" % (len(bad), bad[0]))
    return _rule("TV-10", True, "%d 个报出比率全部可按声明分母复算" % len(rates))


def tv11_claim_scope(rec):
    """TV-11 判据能断言的范围必须与实际能保证范围一致：claim_scope ⊆ guaranteed_scope。"""
    claim = rec.get("claim_scope") or []
    guar = set(rec.get("guaranteed_scope") or [])
    if not claim:
        return _rule("TV-11", False, "claim_scope 缺失（不得留空当万能通过）")
    over = [c for c in claim if c not in guar]
    if over:
        return _rule("TV-11", False, "越界主张（不在 guaranteed_scope 内）：" + ",".join(over))
    return _rule("TV-11", True, "%d 条主张均在保证范围内" % len(claim))


def tv12_no_free_pass(rec):
    """TV-12 禁放行：存在未消的冻结门命中时，档位判词不得为 VERIFIED。"""
    hits = rec.get("frozen_gate_hits") or []
    open_hits = [h for h in hits if isinstance(h, dict) and h.get("resolved") is not True]
    if open_hits and rec.get("tier_verdict") == "VERIFIED":
        return _rule("TV-12", False, "存在 %d 条未消冻结门命中却判 VERIFIED" % len(open_hits))
    if rec.get("tier_verdict") == "VERIFIED" and (rec.get("counts") or {}).get("rejected", 0):
        return _rule("TV-12", False, "存在被拒帧却判 VERIFIED（须先归零或显式登记豁免面）")
    return _rule("TV-12", True, "无未消冻结门命中导致的放行")


RULES = [tv01_declared, tv02_count_conservation, tv03_block_outcome, tv04_tier_eligibility,
         tv05_frame_records, tv06_rejection_records, tv07_not_attempted, tv09_coverage_axis,
         tv10_rates_recomputable, tv11_claim_scope, tv12_no_free_pass]


def adjudicate(rec, repo_root: pathlib.Path):
    findings = [fn(rec) for fn in RULES]
    findings.insert(7, tv08_evidence_pointers(rec, repo_root))
    red = [f for f in findings if not f["ok"]]
    return {"verdict": "pass" if not red else "red", "findings": findings,
            "n_red": len(red), "tier_verdict": rec.get("tier_verdict"),
            "block_outcome": rec.get("block_outcome")}


# ---------- 自证 + 注入负例 ----------
def _good_record(repo: pathlib.Path):
    ev = repo / "run/_tier_gate_selftest/evidence.txt"
    ev.parent.mkdir(parents=True, exist_ok=True)
    ev.write_text("tier-verdict-gate self-test evidence\n", encoding="utf-8")
    rel = str(ev.relative_to(repo))
    ev_ref = [{"path": rel, "sha256": _sha256_file(ev)}]
    return {
        "schema": SCHEMA,
        "declared": {"axis": "frame_count", "tier": "e2e_tier3_full",
                     "planned_frames": 2,
                     "planned_frame_list_sha256": "0" * 64,
                     "denominators": {"accept_rate": "evaluated_frames"},
                     "stage_coverage": ["normalize"]},
        "block_outcome": "first_frame_abort",
        "counts": {"accepted": 0, "rejected": 1, "not_attempted": 1},
        "frames": [
            {"frame_id": "F1", "verdict": "REJECT",
             "evidence": [dict(ev_ref[0])],
             "rejection": {"gate_id": "F9-WCS-ABS", "reason": "rms_px=0.6554 > 0.5",
                           "classification": "defect", "exit": "registered",
                           "deterministic_replay": True}},
            {"frame_id": "F2", "verdict": "NOT_EVALUATED",
             "evidence": [dict(ev_ref[0])]}],
        "aborted_at": {"frame_index": 0, "frame_id": "F1"},
        "not_attempted": [{"item": "F2 及以后全部帧",
                           "reason_code": "BLOCKED_UPSTREAM",
                           "owner": "front-desk", "evidence": [dict(ev_ref[0])]}],
        "frozen_gate_hits": [{"gate_id": "F9-WCS-ABS", "frame_id": "F1",
                              "exit": "registered", "resolved": True}],
        "tier_verdict": "NOT_VERIFIED",
        "subset_evidence": {"tier": "e2e_subset_5f", "n_evaluated": 5,
                            "verdict": "SUBSET_VERIFIED", "evidence": [dict(ev_ref[0])]},
        "reported_rates": {"accept_rate": {"numerator": 0, "value": 0.0}},
        "claim_scope": ["首帧中止形态已定性"],
        "guaranteed_scope": ["首帧中止形态已定性"],
    }


def _mutate(rec, fn):
    r = copy.deepcopy(rec)
    fn(r)
    return r


def _n12(r):
    """把记录改造成"跑满且全收"的绿档位，但留一条未消的冻结门命中。"""
    r["tier_verdict"] = "VERIFIED"
    r["declared"]["stage_coverage"] = ["normalize", "mosaic", "export"]
    r["counts"] = {"accepted": 2, "rejected": 0, "not_attempted": 0}
    r["frames"][0] = {"frame_id": "F1", "verdict": "ACCEPT", "evidence": r["frames"][0]["evidence"]}
    r["frames"][1] = {"frame_id": "F2", "verdict": "ACCEPT", "evidence": r["frames"][1]["evidence"]}
    r["frozen_gate_hits"][0]["resolved"] = False


NEG_CASES = [
    ("N1 首帧中止被写成全部失败",
     lambda r: r.update(block_outcome="all_failed"), "TV-03"),
    ("N2 子集证据冒充档位 VERIFIED",
     lambda r: r.update(tier_verdict="VERIFIED"), "TV-04"),
    ("N3 分母未声明",
     lambda r: r["declared"].pop("denominators"), "TV-01"),
    ("N4 计数不守恒",
     lambda r: r["counts"].update(rejected=3, not_attempted=0), "TV-02"),
    ("N5 拒绝分不清产品行为还是缺陷",
     lambda r: r["frames"][0]["rejection"].update(classification="unknown"), "TV-06"),
    ("N6 拒绝无出口",
     lambda r: r["frames"][0]["rejection"].pop("exit"), "TV-06"),
    ("N7 拒绝未证明确定性",
     lambda r: r["frames"][0]["rejection"].update(deterministic_replay=False), "TV-06"),
    ("N8 未尝试面无合法原因码",
     lambda r: r["not_attempted"][0].update(reason_code="whatever"), "TV-07"),
    ("N9 证据指针悬空",
     lambda r: r["frames"][0].update(evidence=[{"path": "run/_tier_gate_selftest/nope.txt"}]),
     "TV-08"),
    ("N10 报出比率分母漂移",
     lambda r: r["reported_rates"].update(accept_rate={"numerator": 0, "value": 0.9}), "TV-10"),
    ("N11 主张超出能保证范围",
     lambda r: r["claim_scope"].append("生产测光链已确认无缺陷"), "TV-11"),
    ("N12 未消冻结门命中却判 VERIFIED", _n12, "TV-12"),
]


def self_test(repo_root: pathlib.Path) -> int:
    good = _good_record(repo_root)
    res = adjudicate(good, repo_root)
    lines = []
    ok = res["verdict"] == "pass"
    lines.append("[%s] S0 正例（合规记录）=> %s（red=%d）"
                 % ("PASS" if ok else "FAIL", res["verdict"], res["n_red"]))
    if not ok:
        for f in res["findings"]:
            if not f["ok"]:
                lines.append("        %s: %s" % (f["rule"], f["detail"]))
    n_neg_ok = 0
    for name, mut, expect in NEG_CASES:
        r2 = adjudicate(_mutate(good, mut), repo_root)
        hit = [f["rule"] for f in r2["findings"] if not f["ok"]]
        good_case = r2["verdict"] == "red" and expect in hit
        n_neg_ok += int(good_case)
        lines.append("[%s] %s => %s rules=%s 期望含 %s"
                     % ("PASS" if good_case else "FAIL", name, r2["verdict"], hit, expect))
    passed = ok and n_neg_ok == len(NEG_CASES)
    lines.append("SELFTEST_%s: 正例=%s 注入负例=%d/%d"
                 % ("PASS" if passed else "FAIL", "1/1" if ok else "0/1",
                    n_neg_ok, len(NEG_CASES)))
    print("\n".join(lines))
    return EXIT_PASS if passed else EXIT_RED


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="档位判词裁决器（TV-01..TV-12）")
    ap.add_argument("--record")
    ap.add_argument("--repo-root", default=".")
    ap.add_argument("--json-out")
    ap.add_argument("--self-test", action="store_true")
    a = ap.parse_args(argv)
    repo_root = pathlib.Path(a.repo_root).resolve()
    if a.self_test:
        return self_test(repo_root)
    if not a.record:
        print("用法: --record <record.json> | --self-test", file=sys.stderr)
        return EXIT_FAILCLOSED
    p = pathlib.Path(a.record)
    try:
        raw = p.read_text(encoding="utf-8")
    except OSError as exc:
        print("TIER_VERDICT_FAILCLOSED: 记录不可读 %s: %s" % (p, exc), file=sys.stderr)
        return EXIT_FAILCLOSED
    if not raw.strip():
        print("TIER_VERDICT_FAILCLOSED: 记录为空文件 %s" % p, file=sys.stderr)
        return EXIT_FAILCLOSED
    try:
        rec = json.loads(raw)
    except ValueError as exc:
        print("TIER_VERDICT_FAILCLOSED: 记录不可解析 %s: %s" % (p, exc), file=sys.stderr)
        return EXIT_FAILCLOSED
    if not isinstance(rec, dict) or not rec.get("schema"):
        print("TIER_VERDICT_FAILCLOSED: 缺 schema 字段（门不可判）", file=sys.stderr)
        return EXIT_FAILCLOSED
    res = adjudicate(rec, repo_root)
    for f in res["findings"]:
        print("  [%s] %s: %s" % ("ok " if f["ok"] else "RED", f["rule"], f["detail"]))
    print("TIER_VERDICT_%s: tier_verdict=%s block_outcome=%s red_rules=%d"
          % (res["verdict"].upper(), res["tier_verdict"], res["block_outcome"], res["n_red"]))
    if a.json_out:
        pathlib.Path(a.json_out).parent.mkdir(parents=True, exist_ok=True)
        pathlib.Path(a.json_out).write_text(
            json.dumps(res, ensure_ascii=False, indent=2), encoding="utf-8")
    return EXIT_PASS if res["verdict"] == "pass" else EXIT_RED


if __name__ == "__main__":
    raise SystemExit(main())
