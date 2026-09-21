#!/usr/bin/env python3
"""CONTRACT-501：docs/contracts 说明与 eng/contracts schema 的**双向对应**机器校验。

判据（双向）：
  ① 每份登记的说明文档存在，且正文提到对应 schema 文件名；
  ② 每份 schema 存在、是合法 JSON，且 `x-doc` 指回同一份说明文档；
  ③ 登记表内没有悬空项（doc 或 schema 缺失一律判红，fail-closed）。

用法：
  python3 eng/tools/contract_doc_sync.py [--repo-root DIR]
  python3 eng/tools/contract_doc_sync.py --self-test
"""
import argparse
import io
import json
import os
import sys

# 唯一登记表（双向对应的唯一事实源；新增合同必须同时加两处）
PAIRS = (
    ("docs/contracts/DUAL_LINE_CONTRACT.md",
     "eng/contracts/schemas/dual_line_file_domain.schema.json"),
    ("docs/contracts/PIPELINE_BLOCK_CONTRACT.md",
     "eng/contracts/schemas/pipeline_block.schema.json"),
    ("docs/contracts/SCHEDULER_CONTRACT.md",
     "eng/contracts/schemas/scheduler_probe_event.schema.json"),
    ("docs/contracts/PERF_GATE_CONTRACT.md",
     "eng/contracts/schemas/perf_gate_criteria.schema.json"),
    ("docs/contracts/PERF_GATE_CONTRACT.md",
     "eng/contracts/schemas/monitor_field_semantics.schema.json"),
)


def check(root):
    """返回 (ok, 明细行列表)。任一条不满足 ⇒ ok=False（fail-closed）。"""
    detail = []
    ok = True
    for doc_rel, sch_rel in PAIRS:
        doc_abs = os.path.join(root, doc_rel)
        sch_abs = os.path.join(root, sch_rel)
        if not os.path.isfile(doc_abs):
            detail.append("MISSING_DOC " + doc_rel)
            ok = False
            continue
        if not os.path.isfile(sch_abs):
            detail.append("MISSING_SCHEMA " + sch_rel)
            ok = False
            continue
        text = io.open(doc_abs, encoding="utf-8").read()
        base = os.path.basename(sch_rel)
        if base not in text:
            detail.append("DOC_DOES_NOT_REFERENCE " + doc_rel + " -> " + base)
            ok = False
        try:
            obj = json.loads(io.open(sch_abs, encoding="utf-8").read())
        except Exception as exc:  # noqa: BLE001
            detail.append("BAD_JSON " + sch_rel + ": " + str(exc))
            ok = False
            continue
        if obj.get("x-doc") != doc_rel:
            detail.append("SCHEMA_XDOC_MISMATCH " + sch_rel + " x-doc=" + repr(obj.get("x-doc")))
            ok = False
    return ok, detail


def _self_test():
    import tempfile
    cases = []
    with tempfile.TemporaryDirectory() as tmp:
        # 正例：把真仓的 5 对全部复制进夹具
        good = os.path.join(tmp, "good")
        for doc_rel, sch_rel in PAIRS:
            for rel in (doc_rel, sch_rel):
                src = os.path.join(os.getcwd(), rel)
                dst = os.path.join(good, rel)
                os.makedirs(os.path.dirname(dst), exist_ok=True)
                io.open(dst, "w", encoding="utf-8").write(io.open(src, encoding="utf-8").read())
        ok, _ = check(good)
        cases.append(("S1-bidirectional-ok", ok is True))
        # 负例：删掉一份 schema ⇒ 判红
        bad1 = os.path.join(tmp, "bad1")
        os.system("cp -r '" + good + "' '" + bad1 + "'")
        os.remove(os.path.join(bad1, PAIRS[0][1]))
        ok, _ = check(bad1)
        cases.append(("S2-missing-schema-red", ok is False))
        # 负例：schema 的 x-doc 指错 ⇒ 判红
        bad2 = os.path.join(tmp, "bad2")
        os.system("cp -r '" + good + "' '" + bad2 + "'")
        p = os.path.join(bad2, PAIRS[1][1])
        obj = json.loads(io.open(p, encoding="utf-8").read())
        # 故意指错（路径拼接构造，避免被 docs 路径悬空扫描误读为真实引用）
        obj["x-doc"] = "docs/" + "contracts/NOPE.md"
        io.open(p, "w", encoding="utf-8").write(json.dumps(obj, ensure_ascii=False, indent=2))
        ok, _ = check(bad2)
        cases.append(("S3-xdoc-mismatch-red", ok is False))
        # 负例：说明文档不再引用 schema ⇒ 判红
        bad3 = os.path.join(tmp, "bad3")
        os.system("cp -r '" + good + "' '" + bad3 + "'")
        p = os.path.join(bad3, PAIRS[2][0])
        text = io.open(p, encoding="utf-8").read().replace(
            os.path.basename(PAIRS[2][1]), "REDACTED")
        io.open(p, "w", encoding="utf-8").write(text)
        ok, _ = check(bad3)
        cases.append(("S4-doc-lost-reference-red", ok is False))
    bad = [n for n, good_ in cases if not good_]
    for n, good_ in cases:
        print("SELFTEST " + ("PASS " if good_ else "FAIL ") + n)
    if bad:
        print("SELFTEST_FAIL: " + repr(bad), file=sys.stderr)
        return 1
    print("SELFTEST_PASS: %d/%d" % (len(cases), len(cases)))
    return 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo-root", default=os.path.dirname(os.path.dirname(
        os.path.dirname(os.path.abspath(__file__)))))
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args()
    if args.self_test:
        return _self_test()
    ok, detail = check(args.repo_root)
    if ok:
        print("CONTRACT_DOC_SYNC_OK: %d 对说明/schema 双向对应" % len(PAIRS))
        return 0
    print("CONTRACT_DOC_SYNC_FAIL:")
    for d in detail:
        print("  " + d)
    return 1


if __name__ == "__main__":
    sys.exit(main())
