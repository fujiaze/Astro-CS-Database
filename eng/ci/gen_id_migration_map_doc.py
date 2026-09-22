#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""从 eng/ci/id_migration_map.json 生成 eng/ci/ID_MIGRATION_MAP.md（人读摘要）。

规范依据：ENGINEERING_SPEC.md §8「注册表双向一致」——eng/ci/checks.json 与
docs/ci/01_CHECKS.md §2 必须双向对齐；eng/ci/ID_MIGRATION_MAP.md 是
eng/ci/id_migration_map.json（CI-001 ID 收敛迁移映射的机器可读事实源）的**人读摘要**，
属同一治理面的第三份文档，此前无任何门禁覆盖，已发生严重漂移（§1 的注册表哈希/条目数、
§2 的覆盖计数、§6 的处置叙述与 JSON 事实相反）。本生成器把它变成「由 JSON 生成」，
并由 CHK-REGISTRY-DOC-SYNC 的一个 step 以 --check 判绿。

判据：
  G1 生成：md 全文 = render(id_migration_map.json[, checks.json])，覆盖 §1–§7 全节；
  G2 一致：--check 逐字节比对磁盘 md 与生成结果，不一致打印差异摘要并 exit 1；
  G3 单一事实源：§1–§6 的映射事实只来自 id_migration_map.json；checks.json 只用于
     §1 的「写后快照 ↔ 当前注册表」交叉核对**谓词**与 §7 的门禁回读，其当前规模/哈希
     **不写入 md**（写入即让本门对注册表每次改动敏感，产生与事实无关的误红）；
  G4 fail-closed：事实源缺失/不可解析/生成结果为空 → exit 2（不得把「读不到」当「一致」）；
  G5 自检：--self-test 用内存小 JSON 验证「生成→check 绿」「手改一行→check 红」
     「缺 md→exit 2」「两次生成字节相同」四个方向，并断言 §6 语义（无 RETIRE-PENDING
     项时必须写「已改判 KEPT」，且不得出现「本波不删不改」这类与 JSON 矛盾的叙述）。

用法:
  python3 eng/ci/gen_id_migration_map_doc.py                    # 生成/覆盖 md
  python3 eng/ci/gen_id_migration_map_doc.py --check            # 只比对，不写（门禁用）
  python3 eng/ci/gen_id_migration_map_doc.py --self-test        # 红/绿双向自检
  python3 eng/ci/gen_id_migration_map_doc.py --json-out <path>  # 机器可读核对报告
exit 0 = 生成成功/一致；exit 1 = 不一致；exit 2 = 输入不可用（fail-closed）。
"""
from __future__ import annotations

import argparse
import difflib
import hashlib
import json
import pathlib
import sys

REPO = pathlib.Path(__file__).resolve().parent.parent.parent
MAP_REL = "eng/ci/id_migration_map.json"
REGISTRY_REL = "eng/ci/checks.json"
OUT_REL = "eng/ci/ID_MIGRATION_MAP.md"
GENERATOR_REL = "eng/ci/gen_id_migration_map_doc.py"

# 反引号：源码中不出现字面反引号（便于本生成器自身被工具链/夹具安全嵌入）。
Q = "\x60"


def q(text) -> str:
    """markdown 行内代码（反引号包裹）。"""
    return Q + str(text) + Q


def _display(path: pathlib.Path) -> str:
    """仓内路径显示为仓库相对路径（避免绝对路径进入产物，保证跨机确定性）。"""
    try:
        return path.resolve().relative_to(REPO).as_posix()
    except ValueError:
        return str(path)


def _cell(text) -> str:
    """表格单元格：转义竖线与换行。"""
    return str("" if text is None else text).replace("|", "\\|").replace("\n", " ").strip()


def _table(headers, rows) -> list:
    out = ["| " + " | ".join(headers) + " |",
           "|" + "|".join(["---"] * len(headers)) + "|"]
    for row in rows:
        out.append("| " + " | ".join(_cell(c) for c in row) + " |")
    return out


def load_json(path: pathlib.Path):
    """读 JSON；不可用则抛异常（由调用方转 fail-closed）。"""
    return json.loads(path.read_text(encoding="utf-8"))


def registry_facts(path: pathlib.Path):
    """当前注册表事实（只用于交叉核对谓词与 §7 门禁回读，不写入 md 数值）。"""
    facts = {"available": False, "display": _display(path), "sha256": None,
             "entry_count": None, "step_count": None, "unit_count": None, "gates": []}
    if not path.is_file():
        return facts
    try:
        raw = path.read_bytes()
        data = json.loads(raw.decode("utf-8"))
    except Exception:  # noqa: BLE001
        return facts
    checks = data.get("checks") if isinstance(data, dict) else None
    if not isinstance(checks, list):
        return facts
    facts["available"] = True
    facts["sha256"] = hashlib.sha256(raw).hexdigest()
    facts["entry_count"] = len(checks)
    facts["step_count"] = sum(len(c.get("steps") or []) for c in checks if isinstance(c, dict))
    facts["unit_count"] = sum((len(c.get("steps") or []) if (c.get("steps") or [])
                              else 1) for c in checks if isinstance(c, dict))
    for c in checks:
        if not isinstance(c, dict):
            continue
        for s in (c.get("steps") or []):
            if not isinstance(s, dict):
                continue
            cmd = s.get("command") or []
            if any(GENERATOR_REL in str(tok) for tok in cmd):
                facts["gates"].append({"parent": c.get("id"), "step": s.get("id"),
                                       "profiles": list(s.get("profiles") or [])})
    return facts


def _snapshot_matches(facts, snap) -> bool:
    """写后快照是否仍等于当前注册表（三要素全等）。"""
    if not facts.get("available") or not isinstance(snap, dict):
        return False
    return (snap.get("sha256") == facts.get("sha256")
            and snap.get("entry_count") == facts.get("entry_count")
            and snap.get("step_count") == facts.get("step_count"))


def render(doc: dict, facts: dict, map_display: str, out_display: str) -> str:
    """md 全文 = f(id_migration_map.json, checks.json 谓词/门禁回读)。"""
    src = doc.get("source_registry") or {}
    res = doc.get("result_registry") or {}
    cov = doc.get("coverage") or {}
    mappings = [m for m in (doc.get("mappings") or []) if isinstance(m, dict)]
    targets = doc.get("targets") or {}
    reserved = doc.get("reserved_targets") or {}
    pre = [p for p in (doc.get("pre_baseline_retirements") or []) if isinstance(p, dict)]
    absorbed = list(cov.get("absorbed_entries") or [])

    dec_counts = {}
    for m in mappings:
        dec_counts[m.get("decision")] = dec_counts.get(m.get("decision"), 0) + 1
    absorbed_set = set(absorbed)
    kind_counts = {}
    for t in targets.values():
        kind_counts[t.get("kind")] = kind_counts.get(t.get("kind"), 0) + 1
    steps_targets = [t for t in targets.values() if t.get("steps")]
    sec2_targets = sum(1 for t in targets.values()
                       if str(t.get("doc") or "").startswith("docs/ci/01_CHECKS.md"))
    pending = [m for m in mappings if str(m.get("decision", "")).startswith("RETIRE-PENDING")]
    rejudged = [m for m in mappings if m.get("decision") == "KEPT"
                and "RETIRE-PENDING-GOV-001" in str(m.get("note") or "")]

    # §3.2/§3.3：由 mappings 反推 target → 旧 ID（两份登记字段的交叉核对，不引入 JSON 之外的事实）
    derived = {}
    for m in mappings:
        tid = m.get("target")
        if not tid or m.get("old_id") == tid:
            continue
        if str(m.get("decision", "")).startswith("KEPT-PENDING"):
            continue
        derived.setdefault(tid, []).append(m)

    def _order(ms):
        return sorted(ms, key=lambda m: (m.get("order_in_target")
                                         if isinstance(m.get("order_in_target"), int)
                                         else 10 ** 6, str(m.get("old_id"))))

    derived_ids = {tid: [m.get("old_id") for m in _order(v)] for tid, v in derived.items()}
    derived_only = [(tid, ids) for tid, ids in derived_ids.items()
                    if tid in targets and not (targets[tid].get("steps") or [])]
    diff_rows = []
    for tid, t in targets.items():
        dec = list(t.get("steps") or [])
        der = derived_ids.get(tid, [])
        dec_only = [s for s in dec if s not in der]
        der_only = [s for s in der if s not in dec]
        if dec_only or der_only:
            diff_rows.append((tid, dec_only, der_only))

    L = []
    L.append("# eng/ci/checks.json ID 收敛迁移映射（人读摘要）")
    L.append("")
    L.append("> 本文件由 " + q(GENERATOR_REL) + " 从 " + q(map_display) + " 生成，请勿手改；"
             "改 JSON 后重跑生成器：" + q("python3 " + GENERATOR_REL) + "。")
    L.append(">")
    L.append("> 判绿：" + q("python3 " + GENERATOR_REL + " --check")
             + "（逐字节比对磁盘 md 与生成结果，不一致 exit 1）；"
             "自检：" + q("python3 " + GENERATOR_REL + " --self-test") + "。")
    L.append("")
    L.append("任务：" + str(doc.get("generated_by") or "(JSON 未声明 generated_by)")
             + "。机器可读事实源：" + q(map_display) + "（" + q("schema_version")
             + " = " + str(doc.get("schema_version")) + "）。")
    L.append("")

    # ── §1 口径 ────────────────────────────────────────────────────────────
    L.append("## 1. 口径")
    L.append("")
    L.append("- 机器可读事实源：" + q(map_display) + "；本文件是它的人读摘要，"
             "**不含**任何未登记在该 JSON 里的映射事实。")
    L.append("- 写前基线（" + q("source_registry") + "）：" + q(src.get("path"))
             + " sha256 " + q(src.get("sha256")) + "（" + str(src.get("entry_count"))
             + " 项；" + _cell(src.get("captured_note")) + "）。")
    L.append("- 写后快照（" + q("result_registry") + "，CI-001 收敛完成**当时**的状态）："
             "sha256 " + q(res.get("sha256")) + "（" + str(res.get("entry_count"))
             + " 项 / " + str(res.get("step_count")) + " steps）。")
    if not facts.get("available"):
        L.append("- 写后快照 ↔ 当前注册表交叉核对：**无法核对** —— " + q(facts.get("display"))
                 + " 不可用或不可解析（本行不构成判据，注册表事实源不可用时由门禁 fail-closed）。")
    elif _snapshot_matches(facts, res):
        L.append("- 写后快照 ↔ 当前注册表交叉核对（生成器实测 " + q(facts.get("display"))
                 + "）：**一致**。")
    else:
        L.append("- 写后快照 ↔ 当前注册表交叉核对（生成器实测 " + q(facts.get("display"))
                 + "）：**已不同步** —— 注册表在本波之后继续演进（各任务持续登记新的检查项与 "
                 "step），写后快照只是收敛完成当时的状态，**不是**注册表的当前值。"
                 "注册表当前规模与哈希以 " + q("docs/ci/01_CHECKS.md §2")
                 + "（由 " + q("CHK-REGISTRY-DOC-SYNC") + " 双向一致门维护）与注册表自身为准；"
                 "本文件不重复声明其当前值（否则本门会对注册表每次改动敏感，产生与事实无关的红灯）。"
                 "核对明细由生成器 stdout / " + q("--json-out") + " 打印。")
    L.append("- 目标语义 = " + q("docs/ci/01_CHECKS.md §2") + " 表：" + q("targets")
             + " 合计 " + str(len(targets)) + " 个目标，其中 " + str(sec2_targets)
             + " 个的 " + q("doc") + " 字段指向该表（含带登记批注的变体），"
             + str(len(targets) - sec2_targets) + " 个指向其它权威面（" + q("docs/contracts/**")
             + "、" + q("docs/science/**") + "、" + q("docs/api/**") + "、" + q("ASTROCS_DESIGN.md")
             + " 或扩展登记说明）；按 " + q("kind") + " 计：doc " + str(kind_counts.get("doc", 0))
             + " / extension " + str(kind_counts.get("extension", 0)) + " / ci "
             + str(kind_counts.get("ci", 0)) + "。")
    L.append("- 归并形态：目标项 " + q("steps[]") + " 聚合旧注册项，旧 ID 原样保留为 "
             + q("step.id") + "；目标项 " + q("command") + " = "
             + q("python3 eng/ci/run_checks.py --check <目标ID> --quiet")
             + "，因此仍被工作流调用的 " + q("eng/ci/run.py")
             + " 会逐条派发同一执行序列（不静默丢覆盖）。")
    L.append("")

    # ── §2 覆盖计数 ────────────────────────────────────────────────────────
    L.append("## 2. 覆盖计数（" + q("coverage") + " 块逐字引用，本文件不重算该块）")
    L.append("")
    cov_rows = [
        ("source_entry_count", cov.get("source_entry_count")),
        ("mapped_entry_count", cov.get("mapped_entry_count")),
        ("merged_into", cov.get("merged_into")),
        ("kept", cov.get("kept")),
        ("kept_pending_merge", cov.get("kept_pending_merge")),
        ("retire_pending_gov_001", cov.get("retire_pending_gov_001")),
        ("retired", cov.get("retired")),
        ("silent_drops", cov.get("silent_drops")),
        ("result_entry_count", cov.get("result_entry_count")),
    ]
    L.extend(_table(["项", "数（JSON 声明）"], cov_rows))
    L.append("")
    L.append("- " + q("mappings") + " 逐条计数（生成器实测，**与上表口径不同**）：合计 "
             + str(len(mappings)) + " 条 —— " + " / ".join(
                 str(k) + " " + str(v) for k, v in sorted(dec_counts.items()))
             + "；其中在 " + q("coverage.absorbed_entries") + " 内登记为 absorbed 的 "
             + str(sum(1 for m in mappings if m.get("old_id") in absorbed_set)) + " 条。")
    L.append("- 上表由各登记任务按 +N 维护，R13 只机器校验等式 " + q("source_entry_count")
             + " == " + q("len(mappings) − len(set(absorbed_entries))") + "（现为 "
             + str(cov.get("source_entry_count")) + " == " + str(len(mappings)) + " − "
             + str(len(absorbed_set)) + "）；本文件**逐字引用**该块，不用逐条计数覆盖它。")
    L.append("- " + q("absorbed_entries") + "：" + str(len(absorbed)) + " 条登记（唯一 "
             + str(len(absorbed_set)) + " 条），均为本波基线之外、由后续任务新增的执行单元。"
             + (" 注：" + q("absorbed_note") + " = " + _cell(cov.get("absorbed_note")))
             if cov.get("absorbed_note") else "")
    L.append("- " + q("retired") + " = " + str(cov.get("retired"))
             + "：本波无退役项；基线前退役 " + str(len(pre)) + " 项单列于 §2.1，"
             "**不得**计为本波丢失的注册项。")
    if cov.get("note"):
        L.append("- 覆盖口径注（" + q("coverage.note") + " 逐字）：" + _cell(cov.get("note")))
    L.append("")
    L.append("### 2.1 基线前退役（" + q("pre_baseline_retirements") + "）")
    L.append("")
    if pre:
        L.extend(_table(["旧 ID", "决策", "依据", "原因", "备注"],
                        [(q(p.get("old_id")), p.get("decision"), p.get("authority"),
                          p.get("reason"), p.get("note")) for p in pre]))
    else:
        L.append("（JSON 未登记基线前退役项。）")
    L.append("")

    # ── §3 目标项 → 归并的旧 ID ────────────────────────────────────────────
    L.append("## 3. 目标项 → 归并的旧 ID")
    L.append("")
    L.append("### 3.1 " + q("targets") + " 声明了 " + q("steps") + " 的 "
             + str(len(steps_targets)) + " 个目标（JSON 逐字）")
    L.append("")
    L.extend(_table(["目标 ID", "类型", "级", "名称", "steps（旧 ID，按执行序）"],
                    [(q(tid), t.get("kind"), t.get("grade"), t.get("name"),
                      "、".join(q(s) for s in (t.get("steps") or [])))
                     for tid, t in targets.items() if t.get("steps")]))
    L.append("")
    L.append("### 3.2 " + q("mappings") + " 反推：聚合项 " + q("targets.steps")
             + " 未登记 " + str(len(derived_only)) + " 个")
    L.append("")
    if derived_only:
        L.append("- 反推规则：" + q("mappings") + " 中 " + q("target") + " == T 且 "
                 + q("old_id") + " != T 且 decision 非 " + q("KEPT-PENDING-MERGE")
                 + "（后者仍是顶层项，见 §5），按 " + q("order_in_target") + " 排序。")
        L.append("")
        L.extend(_table(["目标 ID", "名称", "反推出的旧 ID（按 order_in_target）"],
                        [(q(tid), (targets.get(tid) or {}).get("name"),
                          "、".join(q(s) for s in ids)) for tid, ids in derived_only]))
    else:
        L.append("（" + q("targets.steps") + " 与 " + q("mappings") + " 反推集合一致。）")
    L.append("")
    L.append("### 3.3 两份登记的差异（" + str(len(diff_rows)) + " 个目标；注册表实际 "
             + q("steps") + " 以 " + q("eng/ci/checks.json") + " 为准，本文件不读它做内容，"
             "故此处只并列两侧登记、不断言谁对）")
    L.append("")
    if diff_rows:
        L.extend(_table(["目标 ID", "仅 " + q("targets.steps") + " 列（mappings 未指向该 target）",
                         "仅 " + q("mappings") + " 指向（" + q("targets.steps") + " 未列）"],
                        [(q(tid), "、".join(q(s) for s in dec_only), "、".join(q(s) for s in der_only))
                         for tid, dec_only, der_only in diff_rows]))
    else:
        L.append("（两侧登记一致。）")
    L.append("")

    # ── §4 RESERVED ────────────────────────────────────────────────────────
    L.append("## 4. §2 有行但本波无实现者（RESERVED，不注册假绿门）")
    L.append("")
    if reserved:
        L.extend(_table(["目标 ID", "级", "名称", "原因", "归属"],
                        [(q(rid), r.get("grade"), r.get("name"), r.get("reason"),
                          r.get("owner")) for rid, r in reserved.items()]))
    else:
        L.append("（JSON 未登记 RESERVED 目标。）")
    L.append("")

    # ── §5 逐条映射明细 ────────────────────────────────────────────────────
    L.append("## 5. 逐条映射明细（" + q("mappings") + "，" + str(len(mappings)) + " 条）")
    L.append("")
    rows = []
    for m in sorted(mappings, key=lambda x: str(x.get("old_id"))):
        tgt = m.get("target")
        if m.get("reserved_target") and m.get("reserved_target") != tgt:
            tgt = str(tgt) + "（预留位 " + str(m.get("reserved_target")) + "）"
        rows.append((q(m.get("old_id")), m.get("decision"), q(tgt), m.get("kind"),
                     m.get("grade"), m.get("note")))
    L.extend(_table(["旧 ID", "决策", "目标/预留位", "类型", "级", "备注（JSON note 逐字）"], rows))
    L.append("")
    with_meta = [m for m in mappings
                 if any(k in m for k in ("checker", "owner", "reason"))]
    L.append("### 5.1 判据实现者 / 归属 / 原始 reason（" + str(len(with_meta)) + " 条）")
    L.append("")
    if with_meta:
        L.extend(_table(["旧 ID", "判据实现者（checker）", "归属（owner）", "reason（JSON 逐字）"],
                        [(q(m.get("old_id")), q(m.get("checker") or "（JSON 未登记）"),
                          m.get("owner"), m.get("reason")) for m in with_meta]))
    else:
        L.append("（JSON 未登记带 checker/owner/reason 的条目。）")
    L.append("")

    # ── §6 RETIRE-PENDING 处置（按 JSON 事实） ─────────────────────────────
    pend_n = cov.get("retire_pending_gov_001")
    if not pending and pend_n in (0, None):
        L.append("## 6. 本波无 RETIRE-PENDING 项：原 GOV-001 冻结门已改判 KEPT（保留并修判据）")
        L.append("")
        L.append("- " + q("coverage.retire_pending_gov_001") + " = " + str(pend_n)
                 + "；" + q("mappings") + " 中 decision 以 " + q("RETIRE-PENDING")
                 + " 开头的条目 " + str(len(pending)) + " 条。")
        L.append("- 结论：**本波没有「不删不改、待 W2 GOV-001 执行」的项**。"
                 "原先按 " + q("RETIRE-PENDING-GOV-001") + " 冻结的 " + str(len(rejudged))
                 + " 项已改判 " + q("KEPT") + " —— 保留为活动门、判据已迁移/修锚并补可执行负例，"
                 "登记在 " + q("docs/ci/01_CHECKS.md §2") + "。以下「现判据」与「目标位」"
                 "逐字取自 JSON 的 " + q("mappings") + "/" + q("targets") + "，不凭印象写。")
        L.append("")
        L.extend(_table(["旧 ID（= 现顶层 ID）", "级", "现判据实现者（checker）",
                         "目标位（target）", "登记文档（targets.doc）", "改判记录（JSON note 逐字）"],
                        [(q(m.get("old_id")), (targets.get(m.get("target")) or {}).get("grade"),
                          q(m.get("checker") or "（JSON 未登记）"), q(m.get("target")),
                          (targets.get(m.get("target")) or {}).get("doc"), m.get("note"))
                         for m in rejudged]))
        L.append("")
        L.append("- 上述条目的 " + q("reason") + " 字段保留改判**前**的迁移意图"
                 "（逐字列出，仅供追溯，**不代表现行处置**）：")
        for m in rejudged:
            L.append("  - " + q(m.get("old_id")) + "：" + _cell(m.get("reason")))
        L.append("- 这些条目的 " + q("note") + " 即改判记录，正文见 §5 表；"
                 "判据实现者与归属见 §5.1 表。")
    else:
        L.append("## 6. RETIRE-PENDING 项（本波不删不改，由后续治理任务执行）")
        L.append("")
        L.append("- " + q("coverage.retire_pending_gov_001") + " = " + str(pend_n)
                 + "；" + q("mappings") + " 中 decision 以 " + q("RETIRE-PENDING")
                 + " 开头的条目 " + str(len(pending)) + " 条。")
        L.append("")
        L.extend(_table(["旧 ID", "决策", "目标/预留位", "归属（owner）", "原因（reason）"],
                        [(q(m.get("old_id")), m.get("decision"),
                          q(m.get("reserved_target") or m.get("target")), m.get("owner"),
                          m.get("reason")) for m in pending]))
    L.append("")

    # ── §7 再生成与门禁 ────────────────────────────────────────────────────
    L.append("## 7. 再生成与门禁")
    L.append("")
    L.append("- 生成：" + q("python3 " + GENERATOR_REL) + "（读 " + q(map_display) + "，写 "
             + q(out_display) + "）。")
    L.append("- 判绿：" + q("python3 " + GENERATOR_REL + " --check") + " —— 逐字节比对磁盘 md "
             "与生成结果，不一致 exit 1；fail-closed：事实源缺失/不可解析/生成结果为空 exit 2。")
    L.append("- 自检：" + q("python3 " + GENERATOR_REL + " --self-test") + " —— "
             "「生成→check 绿」「手改一行→check 红」「缺 md→exit 2」「两次生成字节相同」四向。")
    if facts.get("gates"):
        for g in facts["gates"]:
            L.append("- 注册表登记：本门由 " + q(g.get("parent")) + " 的 step " + q(g.get("step"))
                     + " 承载（profiles: " + ", ".join(g.get("profiles") or []) + "）。")
    elif facts.get("available"):
        L.append("- 注册表登记：**未在 " + q(facts.get("display")) + " 找到承载本门的 step**"
                 "（本文件当前无机器门，属待补缺口）。")
    else:
        L.append("- 注册表登记：无法核对（" + q(facts.get("display")) + " 不可用）。")
    L.append("- 确定性：本文件是 " + q(map_display) + " 的纯函数（无时间戳、无随机、"
             "无环境相关字段；checks.json 只以「写后快照是否仍一致」的谓词与门禁回读进入），"
             "同一输入两次运行字节相同。")
    L.append("- 文档索引：" + q("docs/DOCUMENT_INDEX.yaml") + " 引用本文件，故本文件**不得删除**，"
             "只能由本生成器改写。")
    L.append("")
    return "\n".join(L)


def generate(map_path: pathlib.Path, registry_path: pathlib.Path, out_path: pathlib.Path):
    doc = load_json(map_path)
    if not isinstance(doc, dict):
        raise ValueError("迁移映射 JSON 顶层必须是对象")
    facts = registry_facts(registry_path)
    text = render(doc, facts, _display(map_path), _display(out_path))
    return text, facts


def _diff_summary(expected: str, actual: str, limit: int = 40) -> str:
    exp, act = expected.splitlines(), actual.splitlines()
    diff = list(difflib.unified_diff(act, exp, fromfile="磁盘 md", tofile="生成结果", n=1,
                                     lineterm=""))
    changed = sum(1 for d in diff if d[:1] in "+-" and d[:3] not in ("+++", "---"))
    out = ["差异摘要（行数：磁盘 " + str(len(act)) + " / 生成 " + str(len(exp))
           + "；变更行 " + str(changed) + "）："]
    out += diff[:limit]
    if len(diff) > limit:
        out.append("... （共 " + str(len(diff)) + " 行 diff，已截断）")
    return "\n".join(out)


def run_check(map_path: pathlib.Path, registry_path: pathlib.Path, out_path: pathlib.Path,
              json_out=None) -> int:
    """--check：不写文件，只比对磁盘 md 与生成结果。"""
    if not map_path.is_file():
        print("ID_MIGRATION_MAP_DOC_FAIL: 事实源不可用 " + str(map_path) + "（fail-closed）",
              file=sys.stderr)
        return 2
    if not out_path.is_file():
        print("ID_MIGRATION_MAP_DOC_FAIL: 磁盘 md 不存在 " + str(out_path) + "（fail-closed）",
              file=sys.stderr)
        return 2
    try:
        expected, facts = generate(map_path, registry_path, out_path)
    except Exception as exc:  # noqa: BLE001
        print("ID_MIGRATION_MAP_DOC_FAIL: 生成失败 " + str(exc) + "（fail-closed）", file=sys.stderr)
        return 2
    if not expected.strip():
        print("ID_MIGRATION_MAP_DOC_FAIL: 生成结果为空（fail-closed）", file=sys.stderr)
        return 2
    actual = out_path.read_text(encoding="utf-8")
    ok = actual == expected
    report = {
        "tool": "gen_id_migration_map_doc",
        "map": _display(map_path),
        "registry": _display(registry_path),
        "out": _display(out_path),
        "md_lines": len(actual.splitlines()),
        "generated_lines": len(expected.splitlines()),
        "snapshot_matches_registry": _snapshot_matches(facts, (load_json(map_path) or {}).get(
            "result_registry") or {}) if facts.get("available") else None,
        "registry_available": bool(facts.get("available")),
        "verdict": "PASS" if ok else "FAIL",
    }
    if json_out:
        p = pathlib.Path(json_out)
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    if not ok:
        print("ID_MIGRATION_MAP_DOC_FAIL: " + _display(out_path) + " 与生成结果不一致"
              "（本文件由 " + GENERATOR_REL + " 生成，请勿手改；"
              "重跑 python3 " + GENERATOR_REL + " 后再提交）")
        print(_diff_summary(expected, actual))
        return 1
    print("ID_MIGRATION_MAP_DOC_PASS: " + _display(out_path) + " == 生成结果（"
          + str(len(expected.splitlines())) + " 行）；写后快照 ↔ 当前注册表："
          + ("一致" if report["snapshot_matches_registry"] else "已不同步")
          + "（注册表可用=" + str(report["registry_available"]) + "）")
    if facts.get("available"):
        print("  交叉核对明细：写后快照 entry_count/step_count/sha256 与当前注册表 "
              + str(facts.get("entry_count")) + " 项 / " + str(facts.get("step_count"))
              + " steps / sha256 " + str(facts.get("sha256"))[:16] + "… 比对结果："
              + ("一致" if report["snapshot_matches_registry"] else "已不同步"))
    return 0


def run_generate(map_path: pathlib.Path, registry_path: pathlib.Path, out_path: pathlib.Path,
                 json_out=None) -> int:
    if not map_path.is_file():
        print("ID_MIGRATION_MAP_DOC_FAIL: 事实源不可用 " + str(map_path) + "（fail-closed）",
              file=sys.stderr)
        return 2
    try:
        text, facts = generate(map_path, registry_path, out_path)
    except Exception as exc:  # noqa: BLE001
        print("ID_MIGRATION_MAP_DOC_FAIL: 生成失败 " + str(exc) + "（fail-closed）", file=sys.stderr)
        return 2
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(text, encoding="utf-8")
    snap = (load_json(map_path) or {}).get("result_registry") or {}
    print("ID_MIGRATION_MAP_DOC_WRITE: 已写 " + _display(out_path) + "（"
          + str(len(text.splitlines())) + " 行）；写后快照 ↔ 当前注册表："
          + ("一致" if _snapshot_matches(facts, snap) else "已不同步"))
    if facts.get("available"):
        print("  交叉核对明细：快照 " + str(snap.get("entry_count")) + " 项 / "
              + str(snap.get("step_count")) + " steps / sha256 "
              + str(snap.get("sha256"))[:16] + "… ≠ 当前 " + str(facts.get("entry_count"))
              + " 项 / " + str(facts.get("step_count")) + " steps / sha256 "
              + str(facts.get("sha256"))[:16] + "…" if not _snapshot_matches(facts, snap)
              else "  交叉核对明细：快照与当前注册表三要素全等")
    if json_out:
        p = pathlib.Path(json_out)
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(json.dumps({"tool": "gen_id_migration_map_doc", "mode": "generate",
                                 "out": _display(out_path), "lines": len(text.splitlines()),
                                 "snapshot_matches_registry": _snapshot_matches(facts, snap)},
                                ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return 0


# ── 自检（红/绿双向，纯 tempfile 夹具，不依赖真仓） ─────────────────────────
FIXTURE_MAP = {
    "schema_version": 1,
    "generated_by": "SELFTEST-FIXTURE",
    "source_registry": {"path": "eng/ci/checks.json", "sha256": "a" * 64,
                        "entry_count": 3, "captured_note": "夹具写前基线"},
    "result_registry": {"path": "eng/ci/checks.json", "sha256": "b" * 64,
                        "entry_count": 2, "step_count": 1},
    "targets": {
        "CHK-A": {"kind": "doc", "grade": "P0", "name": "夹具目标 A",
                  "doc": "docs/ci/01_CHECKS.md §2", "steps": ["OLD-1"]},
        "CHK-B": {"kind": "doc", "grade": "P0", "name": "夹具目标 B（targets 未登记 steps）",
                  "doc": "docs/ci/01_CHECKS.md §2"},
        "KEPT-1": {"kind": "extension", "grade": "P1", "name": "夹具保留项",
                   "doc": "docs/ci/01_CHECKS.md §2（夹具登记）"},
    },
    "reserved_targets": {"CHK-GHOST": {"grade": "P0", "name": "夹具预留位",
                                       "reason": "夹具原因", "owner": "夹具归属"}},
    "pre_baseline_retirements": [{"old_id": "OLD-PRE", "decision": "RETIRED",
                                  "authority": "夹具依据", "reason": "夹具原因",
                                  "note": "夹具备注"}],
    "mappings": [
        {"old_id": "OLD-1", "decision": "MERGED-INTO", "target": "CHK-A", "kind": "doc",
         "grade": "P0", "order_in_target": 0, "note": "夹具归并备注"},
        {"old_id": "OLD-2", "decision": "MERGED-INTO", "target": "CHK-B", "kind": "doc",
         "grade": "P0", "order_in_target": 0, "note": "夹具：targets.steps 未登记该 step"},
        {"old_id": "OLD-3", "decision": "KEPT", "target": "CHK-A", "kind": "doc",
         "grade": "P0", "order_in_target": 1, "note": "夹具：mappings 指向 CHK-A 但 steps 未列"},
        {"old_id": "KEPT-1", "decision": "KEPT", "target": "KEPT-1", "checker": "eng/tools/x.py",
         "owner": "夹具归属", "reason": "夹具原始 reason",
         "note": "CI-003（夹具）：原 RETIRE-PENDING-GOV-001 判定撤销 —— 保留为活动门。"},
    ],
    "coverage": {"source_entry_count": 4, "mapped_entry_count": 4, "merged_into": 2,
                 "kept": 2, "kept_pending_merge": 0, "retire_pending_gov_001": 0,
                 "retired": 0, "silent_drops": 0, "result_entry_count": 4,
                 "note": "夹具覆盖口径注", "absorbed_entries": [],
                 "absorbed_note": "夹具 absorbed 注"},
}
FIXTURE_REGISTRY = {"schema_version": 1, "checks": [
    {"id": "CHK-A", "steps": [{"id": "OLD-1", "command": ["python3", "x.py"]}]},
    {"id": "KEPT-1"},
]}


def _self_test() -> int:
    import tempfile

    failures = []
    with tempfile.TemporaryDirectory(prefix="idmap-doc-selftest-") as td:
        tmp = pathlib.Path(td)
        map_p, reg_p, out_p = tmp / "map.json", tmp / "registry.json", tmp / "doc.md"
        map_p.write_text(json.dumps(FIXTURE_MAP, ensure_ascii=False), encoding="utf-8")
        reg_p.write_text(json.dumps(FIXTURE_REGISTRY, ensure_ascii=False), encoding="utf-8")

        # 正例 1：生成 → --check 必须绿（rc 0）
        if run_generate(map_p, reg_p, out_p) != 0:
            failures.append("正例：生成失败")
        text = out_p.read_text(encoding="utf-8")
        if run_check(map_p, reg_p, out_p) != 0:
            failures.append("正例：生成后 --check 应 rc=0（绿）")

        # 正例 2：确定性（两次生成字节相同；重复 check 仍绿）
        if generate(map_p, reg_p, out_p)[0] != text:
            failures.append("正例：两次生成结果不字节相同（非确定性）")

        # 正例 3：语义断言 —— 顶部免责行 + §6 与 JSON 一致（无 RETIRE-PENDING ⇒ 已改判 KEPT）
        if ("从 " + q(_display(map_p)) + " 生成，请勿手改") not in text:
            failures.append("正例：缺少「本文件由生成器从 JSON 生成，请勿手改」抬头")
        if "本波无 RETIRE-PENDING 项" not in text or "已改判" not in text:
            failures.append("正例：§6 未按 JSON 事实写「无 RETIRE-PENDING 项 / 已改判 KEPT」")
        if "本波不删不改" in text:
            failures.append("正例：§6 出现与 JSON 矛盾的「本波不删不改」叙述")
        if "CHK-GHOST" not in text or "夹具预留位" not in text:
            failures.append("正例：§4 RESERVED 未渲染")
        if "### 3.2" not in text or "夹具目标 B" not in text or "OLD-2" not in text:
            failures.append("正例：§3.2（mappings 反推 targets.steps 未登记的聚合项）未渲染")
        if "### 3.3" not in text or "OLD-3" not in text:
            failures.append("正例：§3.3（两份登记差异）未渲染")
        if "OLD-PRE" not in text:
            failures.append("正例：§2.1 基线前退役未渲染")

        # 负例 1：手改一个数字 → --check 必须红（rc 1）
        mutated = text.replace("| source_entry_count | 4 |", "| source_entry_count | 5 |", 1)
        if mutated == text:
            failures.append("负例 1：夹具中未找到可手改的计数行（自检本身失效）")
        else:
            out_p.write_text(mutated, encoding="utf-8")
            if run_check(map_p, reg_p, out_p) != 1:
                failures.append("负例 1：手改计数行后 --check 应 rc=1（红）")

        # 负例 2：手改 §6 叙述（数字不变）→ --check 仍必须红
        out_p.write_text(text.replace("已改判", "本波不删不改，待 W2 改判", 1), encoding="utf-8")
        if run_check(map_p, reg_p, out_p) != 1:
            failures.append("负例 2：手改 §6 叙述后 --check 应 rc=1（红）")

        # 负例 3：删掉磁盘 md → fail-closed（rc 2），不得把「读不到」当「一致」
        out_p.unlink()
        if run_check(map_p, reg_p, out_p) != 2:
            failures.append("负例 3：缺 md 应 rc=2（fail-closed）")

        # 负例 4：事实源缺失 / 不可解析 → rc 2
        if run_check(tmp / "nope.json", reg_p, out_p) != 2:
            failures.append("负例 4：缺事实源应 rc=2（fail-closed）")
        bad_p = tmp / "bad.json"
        bad_p.write_text("{not json", encoding="utf-8")
        if run_check(bad_p, reg_p, out_p) != 2:
            failures.append("负例 5：事实源不可解析应 rc=2（fail-closed）")

        # 负例 6：注册表不可用 ⇒ §1 必须写「无法核对」且不虚构当前值
        out_p.write_text(generate(map_p, tmp / "noreg.json", out_p)[0], encoding="utf-8")
        if "无法核对" not in out_p.read_text(encoding="utf-8"):
            failures.append("负例 6：注册表不可用时 §1 未写「无法核对」")
        if run_check(map_p, tmp / "noreg.json", out_p) != 0:
            failures.append("负例 6：注册表不可用不应影响 md↔JSON 一致性判定（应 rc=0）")

    if failures:
        print("SELFTEST_FAIL:")
        for f in failures:
            print("  " + f)
        return 1
    print("SELFTEST_PASS: 生成→--check 绿；两次生成字节相同；手改计数行/§6 叙述→--check 红"
          "（rc 1）；缺 md/缺事实源/坏 JSON→rc 2（fail-closed）；§6 语义与 JSON 一致"
          "（无 RETIRE-PENDING 项 ⇒ 已改判 KEPT，无「本波不删不改」）")
    return 0


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="从 id_migration_map.json 生成 ID_MIGRATION_MAP.md")
    ap.add_argument("--map", default=str(REPO / MAP_REL))
    ap.add_argument("--registry", default=str(REPO / REGISTRY_REL))
    ap.add_argument("--out", default=str(REPO / OUT_REL))
    ap.add_argument("--check", action="store_true", help="只比对磁盘 md 与生成结果，不写文件")
    ap.add_argument("--self-test", action="store_true", dest="self_test")
    ap.add_argument("--json-out", default=None, help="核对报告 JSON 输出路径")
    args = ap.parse_args(argv)
    if args.self_test:
        return _self_test()
    map_p, reg_p, out_p = pathlib.Path(args.map), pathlib.Path(args.registry), pathlib.Path(args.out)
    if args.check:
        return run_check(map_p, reg_p, out_p, args.json_out)
    return run_generate(map_p, reg_p, out_p, args.json_out)


if __name__ == "__main__":
    raise SystemExit(main())
