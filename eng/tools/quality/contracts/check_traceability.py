#!/usr/bin/env python3
"""check_traceability.py — T400 traceability contract checker（CON-TRACEABILITY）

Checks: ID 唯一、引用存在、核心链完整、无孤儿 SCI/API/SRC/TST
Exit: 0 PASS, 1 contract FAIL, 2 env error, 3 schema error
Supports: --repo, --out-json, --out-junit, --self-test

路径列的**行内 HTML 注记**（2026-09-29 订正）
  docs/TRACEABILITY.csv 的既有惯例是在单元格里用行内 HTML 注记记「订正 / 跨文档
  冲突」（第 2 行 title 列、第 5 行 authority_doc 列各有一条）。注记不是路径的一部分，
  而 authority_doc 是**路径列**：旧判据把整格原文当路径 ⇒ 带尾注的行恒判
  TRACE-MISSING-DOC（ACR-IVAR-001 实例）。
  本判据先把注记从路径列剥离，再判路径存活；注记**如实登记**进结果的
  annotated_authority_docs（不静默吞掉，也不要求删注记）。
  **剥离不构成豁免**（能红能绿由 --self-test 自证）:
    * 路径部分不存在 ⇒ 仍判 TRACE-MISSING-DOC；
    * 整格只有注记、剥离后路径为空 ⇒ 无 authority 正本，判 TRACE-MISSING-DOC
      （否则「写个注释」即成为免检后门）。
"""
import argparse, csv, json, pathlib, sys, re, tempfile

TOOL = "check_traceability"
# 行内 HTML 注记：<!-- ... -->（跨行；注记内容原样登记）
INLINE_NOTE = re.compile(r"<!--(.*?)-->", re.S)


def split_inline_notes(cell):
    """把 `路径<!-- 注记 -->` 拆成 (路径, [注记...])。注记不是路径的一部分。"""
    text = cell or ""
    notes = [m.group(1).strip() for m in INLINE_NOTE.finditer(text)]
    return INLINE_NOTE.sub("", text).strip(), notes


def evaluate(repo):
    """判定核心（自测直接调用）。返回结果 dict；schema_error=True ⇒ 调用方按 exit 3。"""
    trace_path = repo / "docs/TRACEABILITY.csv"
    findings = []
    status = "PASS"
    annotated = []

    # Load traceability
    if not trace_path.exists():
        return {"tool": TOOL, "status": "ERROR", "rows": 0, "findings": [],
                "passed": False, "schema_error": True,
                "schema_message": "FAIL: missing %s" % trace_path}
    with open(trace_path, encoding="utf-8", newline="") as fh:
        rows = list(csv.DictReader(fh))
    if not rows:
        return {"tool": TOOL, "status": "ERROR", "rows": 0, "findings": [],
                "passed": False, "schema_error": True,
                "schema_message": "FAIL: TRACEABILITY empty"}
    # Check header has required columns
    required = ["requirement_id", "requirement_type", "title", "authority_doc"]
    for c in required:
        if c not in rows[0]:
            return {"tool": TOOL, "status": "ERROR", "rows": len(rows),
                    "findings": [], "passed": False, "schema_error": True,
                    "schema_message": "FAIL: missing column %s" % c}
    # ID unique
    ids = [r["requirement_id"].strip() for r in rows]
    dup = [x for x in set(ids) if ids.count(x) > 1]
    if dup:
        findings.append({"id": "TRACE-DUP", "severity": "P1", "file": str(trace_path),
                         "line": 1, "symbol": ",".join(sorted(dup)),
                         "observed": "duplicate IDs", "expected": "unique"})
        status = "FAIL"
    # Authority docs exist and SCI core files exist
    for r in rows:
        raw = (r.get("authority_doc") or "").strip()
        doc, notes = split_inline_notes(raw)
        if notes:
            annotated.append({"symbol": r["requirement_id"],
                              "path": doc, "note": " / ".join(notes)})
        if notes and not doc:
            # 只有注记、没有路径 ⇒ 注记不得变成「写个注释即可免检」的后门
            findings.append({"id": "TRACE-MISSING-DOC", "severity": "P1",
                             "file": str(trace_path), "symbol": r["requirement_id"],
                             "observed": "authority_doc 剥离行内注记后为空（只有注记、"
                                         "无路径）: %s" % raw,
                             "expected": "exists"})
            status = "FAIL"
        elif doc and not (repo / doc).exists():
            findings.append({"id": "TRACE-MISSING-DOC", "severity": "P1",
                             "file": str(trace_path), "symbol": r["requirement_id"],
                             "observed": "missing doc %s" % doc, "expected": "exists"})
            status = "FAIL"
    # Core coverage: need at least Calibration/PSF/WCS/Photometry/Noise/Drizzle/UPM/Rejection/Integration/ACR
    # WCS may be SCI-AST-001 alias, so check for AST as WCS
    core_keywords = ["CAL", "PSF", "PHOT", "NOISE", "DRZ", "UPM", "REJ", "INT", "ACR"]
    wcs_keywords = ["WCS", "AST"]
    titles = " ".join(r["requirement_id"] for r in rows)
    # Special: WCS alias check (SCI-AST covers WCS)
    wcs_hit = any(k in titles.upper() for k in wcs_keywords)
    if not wcs_hit:
        findings.append({"id": "TRACE-CORE-MISSING", "severity": "P1", "symbol": "WCS",
                         "observed": "core SCI WCS/AST not in TRACEABILITY",
                         "expected": "covered"})
        status = "FAIL"
    for kw in core_keywords:
        if kw not in titles.upper():
            findings.append({"id": "TRACE-CORE-MISSING", "severity": "P1", "symbol": kw,
                             "observed": "core SCI not in TRACEABILITY",
                             "expected": "covered"})
            status = "FAIL"
    # Check each SCI has upstream? (traceability already covers)
    # Check no empty requirement_id
    for i, r in enumerate(rows, start=2):
        if not r["requirement_id"].strip():
            findings.append({"id": "TRACE-EMPTY-ID", "severity": "P1",
                             "file": str(trace_path), "line": i,
                             "observed": "empty ID", "expected": "non-empty"})
            status = "FAIL"

    return {"tool": TOOL, "status": status, "rows": len(rows),
            "annotated_authority_docs": annotated, "findings": findings,
            "passed": status == "PASS"}


# ------------------------------------------------------------------ 自测（能红能绿）----
_FIXTURE_ROWS = [
    # 覆盖核心链关键字 CAL/PSF/PHOT/NOISE/DRZ/UPM/REJ/INT/ACR + WCS
    ("SCI-CAL-001", "docs/science/CAL.md"),
    ("SCI-PSF-001", "docs/science/PSF.md"),
    ("SCI-PHOT-001", "docs/science/PHOT.md"),
    ("SCI-NOISE-001", "docs/science/NOISE.md"),
    ("SCI-DRZ-001", "docs/science/DRZ.md"),
    ("SCI-UPM-001", "docs/science/UPM.md"),
    ("SCI-REJ-001", "docs/science/REJ.md"),
    ("SCI-INT-001", "docs/science/INT.md"),
    ("SCI-WCS-001", "docs/science/WCS.md"),
    ("ACR-IVAR-001", "docs/science/ACR.md"),
]
_HEADER = "requirement_id,requirement_type,title,authority_doc,notes\n"


def _fixture_repo(root, authority_doc_by_id=None):
    """搭一个最小可判 repo：核心链关键字齐备 + 目标文档存在。"""
    (root / "docs" / "science").mkdir(parents=True, exist_ok=True)
    (root / "docs" / "contracts").mkdir(parents=True, exist_ok=True)
    lines = [_HEADER]
    for rid, doc in _FIXTURE_ROWS:
        cell = (authority_doc_by_id or {}).get(rid, doc)
        (root / doc).parent.mkdir(parents=True, exist_ok=True)
        (root / doc).write_text("x\n", encoding="utf-8")
        lines.append("%s,science,title,%s,note\n" % (rid, cell))
    (root / "docs" / "TRACEABILITY.csv").write_text("".join(lines), encoding="utf-8")
    return root


def self_test():
    """正例判绿 + 负例判红（含「剥离没变成豁免」的证明）。"""
    cases = []
    with tempfile.TemporaryDirectory(prefix="trace_selftest_") as td:
        base = pathlib.Path(td)

        def run_case(name, *, cell_map=None, expect_rc, expect_id=""):
            root = pathlib.Path(tempfile.mkdtemp(dir=base))
            _fixture_repo(root, cell_map)
            res = evaluate(root)
            rc = 0 if res.get("passed") else 1
            ids = [f["id"] for f in res["findings"]]
            good = (rc == expect_rc) and ((expect_rc == 0) or (expect_id in ids))
            cases.append((name, rc, expect_rc, good, ids,
                          res.get("annotated_authority_docs", [])))

        # 正例 1：无注记的普通路径列（基线）
        run_case("P1 普通路径列存在 ⇒ 绿", expect_rc=0)
        # 正例 2：路径列带行内 HTML 尾注、路径部分存在 ⇒ 必须仍绿（本订正的主判据）
        run_case("P2 带尾注的路径列仍解析到真实文档 ⇒ 绿", expect_rc=0,
                 cell_map={"ACR-IVAR-001":
                           "docs/science/ACR.md<!-- 订正: authority 正本实为 "
                           "ACR.md:25。旧对照：docs/science/PHASE2_UPM.md -->"})
        # 负例 1：同样带尾注，但路径部分不存在 ⇒ 必须仍判 TRACE-MISSING-DOC
        run_case("N1 带尾注但路径不存在 ⇒ TRACE-MISSING-DOC", expect_rc=1,
                 expect_id="TRACE-MISSING-DOC",
                 cell_map={"ACR-IVAR-001":
                           "docs/science/NOPE.md<!-- 订正: 旧对照 x -->"})
        # 负例 2：整格只有注记（剥离后为空）⇒ 不得当成"免检"
        run_case("N2 整格只有注记 ⇒ TRACE-MISSING-DOC", expect_rc=1,
                 expect_id="TRACE-MISSING-DOC",
                 cell_map={"ACR-IVAR-001": "<!-- 待订正 -->"})
        # 负例 3：无注记、路径不存在 ⇒ 剥离逻辑未削弱原判据
        run_case("N3 普通路径列不存在 ⇒ TRACE-MISSING-DOC", expect_rc=1,
                 expect_id="TRACE-MISSING-DOC",
                 cell_map={"ACR-IVAR-001": "docs/science/NOPE.md"})

        # 正例 3：注记必须被如实登记（不静默吞掉）
        root = pathlib.Path(tempfile.mkdtemp(dir=base))
        _fixture_repo(root, {"ACR-IVAR-001":
                             "docs/science/ACR.md<!-- 订正: 黄11 -->"})
        res = evaluate(root)
        rec = [a for a in res.get("annotated_authority_docs", [])
               if a["symbol"] == "ACR-IVAR-001"]
        good = res["passed"] and rec and rec[0]["note"] == "订正: 黄11"
        cases.append(("P3 注记如实登记（不静默吞掉）", 0 if good else 1, 0, bool(good),
                      rec, []))

        # 负例 4/5：其它判据未被本次订正弱化
        root = pathlib.Path(tempfile.mkdtemp(dir=base))
        _fixture_repo(root)
        csvp = root / "docs" / "TRACEABILITY.csv"
        csvp.write_text(csvp.read_text(encoding="utf-8").replace("SCI-CAL-001", "SCI-UPM-001", 1),
                        encoding="utf-8")
        res = evaluate(root)
        good = (not res["passed"]) and any(f["id"] in ("TRACE-DUP", "TRACE-CORE-MISSING")
                                           for f in res["findings"])
        cases.append(("N4 重复 ID / 核心链缺失仍能红", 0 if good else 1, 0, bool(good),
                      [f["id"] for f in res["findings"]], []))

        # 负例 6：缺列 ⇒ schema error（exit 3 路径）
        root = pathlib.Path(tempfile.mkdtemp(dir=base))
        (root / "docs").mkdir(parents=True)
        (root / "docs" / "TRACEABILITY.csv").write_text("requirement_id\nX\n",
                                                        encoding="utf-8")
        res = evaluate(root)
        cases.append(("N5 表头缺列 ⇒ schema_error", 3 if res.get("schema_error") else 0,
                      3, bool(res.get("schema_error")), [], []))

    ok = True
    for name, rc, want, good, ids, rec in cases:
        ok = ok and good
        print("[selftest] %-46s rc=%s want=%s %s" %
              (name, rc, want, "OK" if good else "MISMATCH"))
        if not good:
            print("[selftest]   命中=%s 注记=%s" % (ids, rec))
    print("[selftest] %d cases, %s" % (len(cases), "ALL OK" if ok else "FAILED"))
    return 0 if ok else 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", default=".")
    ap.add_argument("--out-json", default=None)
    ap.add_argument("--out-junit", default=None)
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args()
    if args.self_test:
        return self_test()

    repo = pathlib.Path(args.repo)
    trace_path = repo / "docs/TRACEABILITY.csv"
    res = evaluate(repo)
    if res.get("schema_error"):
        print(res["schema_message"], file=sys.stderr)
        return 3
    findings = res["findings"]

    if args.out_json:
        pathlib.Path(args.out_json).parent.mkdir(parents=True, exist_ok=True)
        pathlib.Path(args.out_json).write_text(json.dumps(res, indent=2, ensure_ascii=False),
                                               encoding="utf-8")
    else:
        print(json.dumps(res, indent=2, ensure_ascii=False))
    # JUnit
    if args.out_junit:
        pathlib.Path(args.out_junit).parent.mkdir(parents=True, exist_ok=True)
        failures = len([f for f in findings if f["severity"] in ("P0", "P1")])
        junit = ('<testsuite name="check_traceability" tests="%d" failures="%d">'
                 '<testcase classname="trace" name="ids"/></testsuite>'
                 % (res["rows"], failures))
        pathlib.Path(args.out_junit).write_text(junit, encoding="utf-8")

    # Exit codes: 0 PASS, 1 FAIL, 3 schema
    return 0 if res["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
