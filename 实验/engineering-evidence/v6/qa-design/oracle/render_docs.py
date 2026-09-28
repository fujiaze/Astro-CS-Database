# -*- coding: utf-8 -*-
"""QA-MATRIX-001 机械渲染器：qa_matrix.json / baseline_matrix.json -> docs/validation/v6/*.md 的渲染块。

人读正文由任务手写；两处渲染块由本脚本写入，保证人读表与机器规格逐字一致（render consistency）。
用法：python3 render_docs.py [--docs-dir DIR] [--write|--stdout]
"""
from __future__ import annotations
import json, os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
DATA = os.path.join(ROOT, "data")
DEFAULT_DOCS = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(HERE)))),
                            "docs", "validation", "v6")

M_QA_TABLE = ("<!-- QA-MATRIX-TABLE-BEGIN -->", "<!-- QA-MATRIX-TABLE-END -->")
M_QA_DETAIL = ("<!-- QA-MATRIX-DETAILS-BEGIN -->", "<!-- QA-MATRIX-DETAILS-END -->")
M_BM_MODES = ("<!-- BASELINE-MODES-BEGIN -->", "<!-- BASELINE-MODES-END -->")
M_BM_CMP = ("<!-- BASELINE-CMP-BEGIN -->", "<!-- BASELINE-CMP-END -->")
M_BM_RULES = ("<!-- BASELINE-RULES-BEGIN -->", "<!-- BASELINE-RULES-END -->")
BT = chr(96)


def jload(p):
    with open(p, encoding="utf-8") as f:
        return json.load(f)


def crit_str(c):
    if c.get("kind") == "numeric":
        unit = (" " + c["threshold_unit"]) if c.get("threshold_unit") else ""
        return BT + "%s %s %s%s" % (c["metric"], c["op"], c["thresh"], unit) + BT
    return BT + str(c.get("rule", "")) + BT


def render_gate_table(qm):
    L = ["| Gate | Family | Phase | 判据（criterion） | 容差来源 / 状态 | 零用例即红 | 门能红 mutation | Owner |",
         "|---|---|---|---|---|---|---|---|"]
    for g in qm["gates"]:
        c = g["criterion"]
        tol = "%s / %s" % (g["tol_source"], c["threshold_status"])
        tol = tol.replace("|", "/").replace("\n", " ")
        zc = "min=%d；executed=0 或 skip-only -> rc=2" % g["zero_case_red"]["min_cases"]
        L.append("| " + BT + "%s" % g["gate_id"] + BT + " | %s | %s | %s | %s | %s | %s | %s |" % (
            g["family"], g["phase"], crit_str(c), tol, zc,
            ", ".join(g["mutations"]), g["owner"]))
    return "\n".join(L)


def render_gate_details(qm):
    L = []
    for g in qm["gates"]:
        c = g["criterion"]
        L.append("### " + BT + "%s" % g["gate_id"] + BT + " — %s" % g["title"])
        L.append("")
        L.append("- **claim**：%s" % g["claim"])
        L.append("- **条款锚**：%s" % "；".join(g["anchors"]))
        if g.get("literature"):
            L.append("- **文献锚**：%s" % "；".join(g["literature"]))
        L.append("- **输入 -> 输出**：%s -> %s" % (", ".join(g["inputs"]), ", ".join(g["outputs"])))
        L.append("- **单位**：%s" % ", ".join("%s=%s" % (k, v) for k, v in g["units"].items()))
        L.append("- **适用域**：%s" % g["domain"])
        L.append("- **判据**：%s" % crit_str(c))
        if c.get("reference_value"):
            L.append("- **参考值/依据**：%s" % c["reference_value"])
        L.append("- **容差来源**：%s（status=%s%s）" % (
            g["tol_source"], c["threshold_status"],
            ("，owner=%s" % c["threshold_owner"]) if c.get("threshold_owner") else ""))
        L.append("- **零用例即红**：min_cases=%d；%s" % (g["zero_case_red"]["min_cases"],
                                                          g["zero_case_red"]["runner_rule"]))
        L.append("- **fail-closed**：%s" % g["fail_closed"])
        L.append("- **独立 Oracle**：kind=%s；truth=%s；must_not=%s" % (
            g["oracle"]["kind"], g["oracle"]["truth"], ", ".join(g["oracle"]["must_not"])))
        L.append("- **门能红 mutation**：%s" % ", ".join(g["mutations"]))
        if g.get("ledger_layer"):
            L.append("- **账本层**：%s；refs=%s" % (g["ledger_layer"], ", ".join(g.get("ledger_refs", []))))
        if g.get("signoff"):
            L.append("- **负责人签字（只登记）**：%s" % g["signoff"])
        L.append("- **owner / wave / status**：%s / W%s / %s" % (g["owner"], g["wave"], g["status"]))
        L.append("")
    return "\n".join(L)


def render_bm_modes(bm):
    L = ["| mode | class | 权重对象 | 单位 | 权威式 | covariance 来源 | effective PSF | 组内归一 | 可声明 | 禁止声明 |",
         "|---|---|---|---|---|---|---|---|---|---|"]
    for m in bm["modes"]:
        L.append("| " + BT + m["mode"] + BT + " | %s | %s | %s | " % (
            m["class"], m["weight_object"], m["units"]) + BT + m["authoritative_formula"] + BT
            + " | %s | %s | %s | %s | %s |" % (m["covariance_source"], m["effective_psf"],
            m["group_normalized"], m["optimality_declaration"], m["forbidden_declaration"]))
    return "\n".join(L)


def render_bm_cmp(bm):
    L = ["| cell | candidate | baseline | metrics | declaration_limit | 预注册 | 容差来源 |",
         "|---|---|---|---|---|---|---|"]
    for c in bm["comparisons"]:
        L.append("| %s | " % c["cell"] + BT + c["candidate"] + BT + " | " + BT + c["baseline"] + BT
                 + " | %s | %s | %s | %s |" % (", ".join(c["metrics"]) or "—",
                 c["declaration_limit"], c["pre_registration_required"], c["tolerance_source"]))
    return "\n".join(L)


def render_bm_rules(bm):
    return "\n".join("- %s" % r for r in bm["declaration_rules"])


def blocks(docs_dir):
    qm = jload(os.path.join(ROOT, "qa_matrix.json"))
    bm = jload(os.path.join(DATA, "baseline_matrix.json"))
    qa = os.path.join(docs_dir, "QA_MATRIX.md")
    bmfile = os.path.join(docs_dir, "BASELINE_COMPARISON_MATRIX.md")
    return {
        qa: [(M_QA_TABLE, render_gate_table(qm)), (M_QA_DETAIL, render_gate_details(qm))],
        bmfile: [(M_BM_MODES, render_bm_modes(bm)), (M_BM_CMP, render_bm_cmp(bm)),
                 (M_BM_RULES, render_bm_rules(bm))],
    }


def skeleton(name):
    if name == "QA_MATRIX.md":
        return ("# QA-MATRIX-001 科学 QA 矩阵（机读渲染块）\n\n"
                + M_QA_TABLE[0] + "\n" + M_QA_TABLE[1] + "\n\n"
                + M_QA_DETAIL[0] + "\n" + M_QA_DETAIL[1] + "\n")
    return ("# 基线比较矩阵（机读渲染块）\n\n"
            + M_BM_MODES[0] + "\n" + M_BM_MODES[1] + "\n\n"
            + M_BM_CMP[0] + "\n" + M_BM_CMP[1] + "\n\n"
            + M_BM_RULES[0] + "\n" + M_BM_RULES[1] + "\n")


def apply(text, marker, body):
    b, e = marker
    if b not in text or e not in text:
        return text.rstrip("\n") + "\n\n" + b + "\n" + body + "\n" + e + "\n"
    i = text.index(b) + len(b)
    j = text.index(e)
    return text[:i] + "\n" + body + "\n" + text[j:]


def main(argv):
    docs = DEFAULT_DOCS; write = False; to_stdout = False
    i = 1
    while i < len(argv):
        if argv[i] == "--docs-dir": docs = argv[i+1]; i += 2
        elif argv[i] == "--write": write = True; i += 1
        elif argv[i] == "--stdout": to_stdout = True; i += 1
        else: print("unknown", argv[i]); return 2
    os.makedirs(docs, exist_ok=True)
    out = []
    for path, blks in blocks(docs).items():
        text = open(path, encoding="utf-8").read() if os.path.exists(path) else skeleton(os.path.basename(path))
        for m, body in blks:
            text = apply(text, m, body)
        if write:
            with open(path, "w", encoding="utf-8") as f: f.write(text)
        out.append("%s: %d blocks" % (path, len(blks)))
        if to_stdout: print(text)
    print("rendered: " + "; ".join(out))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
