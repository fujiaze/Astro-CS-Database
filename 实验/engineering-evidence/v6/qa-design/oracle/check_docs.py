# -*- coding: utf-8 -*-
"""QA-MATRIX-001 人读/机读一致性检查（render consistency + 禁止项扫描）。

用法：
  python3 check_docs.py [--docs-dir DIR]                 # 检查 QA_MATRIX.md + BASELINE_COMPARISON_MATRIX.md
  python3 check_docs.py --doc PATH --kind qa_matrix|baseline
退出码：0 一致；1 不一致/违规；2 用法。
"""
from __future__ import annotations
import json, os, re, sys
import render_docs as R

NEG = ("不得", "禁止", "DEFERRED", "deferred", "延迟", "不进", "非生产", "forbidden", "撤销", "REJECT")
BT = chr(96)


def read(p):
    with open(p, encoding="utf-8") as f:
        return f.read()


def block(text, marker):
    b, e = marker
    if b not in text or e not in text:
        return None
    i = text.index(b) + len(b); j = text.index(e)
    return text[i:j].strip("\n")


def check_file(path, kind):
    qm = R.jload(os.path.join(R.ROOT, "qa_matrix.json"))
    bm = R.jload(os.path.join(R.DATA, "baseline_matrix.json"))
    text = read(path)
    viol = []
    if kind == "qa_matrix":
        t = block(text, R.M_QA_TABLE); d = block(text, R.M_QA_DETAIL)
        exp_t = R.render_gate_table(qm); exp_d = R.render_gate_details(qm)
        if t != exp_t.strip("\n"):
            viol.append("QA-MATRIX-TABLE 与 qa_matrix.json 渲染不一致")
        if d != exp_d.strip("\n"):
            viol.append("QA-MATRIX-DETAILS 与 qa_matrix.json 渲染不一致")
        got = set(re.findall(r"^\| " + BT + r"(G-[A-Z0-9]+-\d+|P0-\d+)" + BT + r" \|", text, re.M))
        want = {g["gate_id"] for g in qm["gates"]}
        if got != want:
            viol.append("门行集合不一致：缺少 %s；多余 %s" % (sorted(want-got), sorted(got-want)))
    else:
        for marker, exp in ((R.M_BM_MODES, R.render_bm_modes(bm)),
                            (R.M_BM_CMP, R.render_bm_cmp(bm)),
                            (R.M_BM_RULES, R.render_bm_rules(bm))):
            if block(text, marker) != exp.strip("\n"):
                viol.append("%s 与 baseline_matrix.json 渲染不一致" % marker[0])
    # global forbidden scan: psf_snr_power 被当作生产模式（表格行形状，避免叙事误报）
    for ln, line in enumerate(text.splitlines(), 1):
        if re.match(r"^\|\s*" + BT + r"?psf_snr_power" + BT + r"?\s*\|\s*production\b", line):
            viol.append("第 %d 行把 psf_snr_power 列为生产模式（C-004.1）: %s" % (ln, line.strip()[:80]))
    return viol


def main(argv):
    docs = R.DEFAULT_DOCS; one = None; kind = None
    i = 1
    while i < len(argv):
        if argv[i] == "--docs-dir": docs = argv[i+1]; i += 2
        elif argv[i] == "--doc": one = argv[i+1]; i += 2
        elif argv[i] == "--kind": kind = argv[i+1]; i += 2
        else: print("unknown", argv[i]); return 2
    allv = {}
    if one:
        allv[one] = check_file(one, kind or "qa_matrix")
    else:
        allv[os.path.join(docs, "QA_MATRIX.md")] = check_file(os.path.join(docs, "QA_MATRIX.md"), "qa_matrix")
        allv[os.path.join(docs, "BASELINE_COMPARISON_MATRIX.md")] = check_file(
            os.path.join(docs, "BASELINE_COMPARISON_MATRIX.md"), "baseline")
    n = sum(len(v) for v in allv.values())
    print(json.dumps({"violations": allv, "n": n}, ensure_ascii=False)[:2000])
    return 1 if n else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
