# -*- coding: utf-8 -*-
"""QA-MATRIX-001 负向 mutation 驱动器（science / spec / doc 三类）。

对每条 mutation：注入错误，断言门/校验器变红（rc!=0），并逐条记录实测 rc 与命中的门/规则。
science：注入 Oracle 的 subject 错误，断言目标门至少一个 check 变红；
spec   ：注入 qa_matrix.json 结构错误，断言 validate_spec 报违规；
doc    ：注入人读渲染块错误，断言 check_docs 报不一致。
用法：python3 run_mutations.py [--json out.json]
退出码：0 全部检出；1 存在未检出 mutation。
"""
from __future__ import annotations
import copy, json, os, re, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import qa_oracle, validate_spec, check_docs, render_docs  # noqa: E402

DOCS = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(ROOT))), "docs", "validation", "v6")


def jload(p):
    with open(p, encoding="utf-8") as f:
        return json.load(f)


def run_science(mid, muts):
    bugs = qa_oracle.MUT_BUGS.get(mid)
    if bugs is None:
        return dict(mut=mid, kind="science", detected=False, rc=None, note="no bug mapping")
    res = qa_oracle.run(bugs)
    red = [r for r in res if not r["ok"]]
    targets = [m for m in muts if m["id"] == mid][0]["targets"]
    covered = {t: any(t in r["gates"] for r in red) for t in targets}
    ok = bool(red) and all(covered.values())
    return dict(mut=mid, kind="science", detected=ok, rc=1 if red else 0,
                failed_checks=[r["id"] for r in red], targets_covered=covered)


def run_spec(mid, muts, qm, led, bm, meta):
    m = [x for x in muts if x["id"] == mid][0]
    try:
        qm2, led2 = validate_spec.apply_spec_mutation(qm, led, m["op"])
    except Exception as e:
        return dict(mut=mid, kind="spec", detected=False, rc=None, note="apply failed: %s" % e)
    viol = validate_spec.validate(qm2, muts, bm, meta, led2)
    return dict(mut=mid, kind="spec", detected=bool(viol), rc=1 if viol else 0,
                rules=sorted({v["rule"] for v in viol})[:8])


def run_doc(mid, muts):
    m = [x for x in muts if x["id"] == mid][0]
    src = os.path.join(DOCS, "QA_MATRIX.md")
    text = open(src, encoding="utf-8").read()
    op = m["op"]; gid = op.get("gate_id")
    if op["kind"] == "doc_delete_row":
        text = re.sub(r"^\| " + chr(96) + gid + chr(96) + r" \|.*\n", "", text, flags=re.M)
    elif op["kind"] == "doc_edit_tolerance":
        pat = re.compile(r"(^\| " + chr(96) + gid + chr(96) + r" \|.*?)(1e-3)", re.M)
        text2 = pat.sub(lambda mm: mm.group(1) + "9e-3", text, count=1)
        if text2 == text:
            text2 = re.sub(r"1e-3", "9e-3", text, count=1)
        text = text2
    elif op["kind"] == "doc_append_line":
        text = text.rstrip("\n") + "\n" + op["text"] + "\n"
    else:
        return dict(mut=mid, kind="doc", detected=False, rc=None, note="unknown doc op")
    fd, tmp = tempfile.mkstemp(suffix=".md"); os.close(fd)
    with open(tmp, "w", encoding="utf-8") as f: f.write(text)
    try:
        viol = check_docs.check_file(tmp, "qa_matrix")
    finally:
        os.unlink(tmp)
    return dict(mut=mid, kind="doc", detected=bool(viol), rc=1 if viol else 0, rules=viol[:4])


def main(argv):
    out = None
    if "--json" in argv: out = argv[argv.index("--json")+1]
    muts = jload(os.path.join(ROOT, "data", "mutations.json"))["mutations"]
    qm, muts2, bm, meta, led = validate_spec.load_all()
    results = []
    for m in muts:
        if m["mech"] == "science":
            results.append(run_science(m["id"], muts))
        elif m["mech"] == "spec":
            results.append(run_spec(m["id"], muts, qm, led, bm, meta))
        else:
            results.append(run_doc(m["id"], muts))
    nd = [r for r in results if not r["detected"]]
    doc = dict(driver="run_mutations", n=len(results), n_detected=len(results)-len(nd),
               rc=(1 if nd else 0), undetected=nd, results=results)
    if out:
        with open(out, "w", encoding="utf-8") as f: json.dump(doc, f, ensure_ascii=False, indent=1)
    print(json.dumps(dict(n=doc["n"], n_detected=doc["n_detected"], rc=doc["rc"],
                          undetected=[r["mut"] for r in nd]), ensure_ascii=False))
    return doc["rc"]


if __name__ == "__main__":
    sys.exit(main(sys.argv))
