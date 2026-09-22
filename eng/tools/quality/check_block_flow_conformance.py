#!/usr/bin/env python3
"""块流一致性机器门（ARCH-505）：登记册 vs 代码事实的双向漂移检测。

它**不**判断「注册表该不该改」（那是负责人裁决，AGENTS §10）；它保证：
  D1  登记册结构合法：每条 deviation 有 id/severity/kind/registry_ref/summary/evidence；
  D2  每条 evidence 的文件存在、行号在范围内、且该行**确实包含**声明的 token
      —— token 消失说明代码改了（修好了或改坏了），登记册必须同步，否则判红；
  D3  evidence 不得指向空行/注释占位（防「用注释行凑证据」）；
  D4  id 唯一、severity 取值合法；
  D5  每条 blocker 必须带 owner_decision_required 标记（结构性变更不得静默）；
  D6  登记册不得为空（空登记册 = 未做一致性核查，判红）；
  D7  被登记为 blocker 的偏差必须出现在 owner 上报文件（工程控制/RELEASE-05/OPEN_QUESTIONS.md）
      的引用里，否则判红 —— 防止「登记了但没上呈」。

用法：
  python3 eng/tools/quality/check_block_flow_conformance.py [--register <json>] [--json-out <json>]
  python3 eng/tools/quality/check_block_flow_conformance.py --self-test
"""
import argparse
import copy
import io
import json
import os
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
REGISTER = os.path.join(REPO, "eng/contracts/block_flow/conformance_deviations.json")
OPEN_QUESTIONS = os.path.join(REPO, "工程控制/RELEASE-05/OPEN_QUESTIONS.md")
SEVERITIES = ("blocker", "major", "minor", "info")
KINDS = ("producer_mismatch", "dead_port", "undeclared_input", "undeclared_output",
         "undeclared_module", "no_named_blocks_in_production", "unit_mismatch",
         "shape_mismatch")


def check_register(doc, repo=REPO):
    errs = []
    devs = doc.get("deviations")
    if not isinstance(devs, list) or not devs:
        return ["D6 register is empty or malformed (未做一致性核查 = 判红)"]
    seen = set()
    oq = ""
    oqp = os.path.join(repo, "工程控制/RELEASE-05/OPEN_QUESTIONS.md")
    if os.path.isfile(oqp):
        oq = io.open(oqp, encoding="utf-8").read()
    for d in devs:
        did = d.get("id", "?")
        if did in seen:
            errs.append("D4 duplicate deviation id %s" % did)
        seen.add(did)
        for k in ("severity", "kind", "registry_ref", "summary", "evidence"):
            if not d.get(k):
                errs.append("D1 %s missing field %s" % (did, k))
        if d.get("severity") not in SEVERITIES:
            errs.append("D4 %s invalid severity %r" % (did, d.get("severity")))
        if d.get("kind") not in KINDS:
            errs.append("D4 %s invalid kind %r" % (did, d.get("kind")))
        ev = d.get("evidence") or []
        if not ev:
            errs.append("D1 %s has no evidence" % did)
        for e in ev:
            f = e.get("file")
            ln = e.get("line")
            tok = e.get("contains")
            if not f or not isinstance(ln, int) or not tok:
                errs.append("D1 %s malformed evidence %r" % (did, e))
                continue
            p = os.path.join(repo, f)
            if not os.path.isfile(p):
                errs.append("D2 %s evidence file missing: %s" % (did, f))
                continue
            lines = io.open(p, encoding="utf-8", errors="replace").read().splitlines()
            if ln < 1 or ln > len(lines):
                errs.append("D2 %s evidence line out of range: %s:%d (file has %d lines)"
                            % (did, f, ln, len(lines)))
                continue
            text = lines[ln - 1]
            if tok not in text:
                errs.append("D2 %s evidence token %r not found at %s:%d -> %r"
                            % (did, tok, f, ln, text.strip()[:120]))
            stripped = text.strip()
            if not stripped or stripped.startswith("//") or stripped.startswith("#"):
                errs.append("D3 %s evidence points at empty/comment line %s:%d" % (did, f, ln))
        if d.get("severity") == "blocker" and not doc.get("owner_decision_required"):
            errs.append("D5 %s is blocker but register lacks owner_decision_required" % did)
        if d.get("severity") == "blocker" and oq and did not in oq:
            errs.append("D7 %s is blocker but not escalated in OPEN_QUESTIONS.md" % did)
    return errs


def run(path):
    if not os.path.isfile(path):
        return ["register not found: %s" % path], None
    doc = json.load(io.open(path, encoding="utf-8"))
    return check_register(doc), doc


def _self_test():
    doc = json.load(io.open(REGISTER, encoding="utf-8"))
    cases = []
    cases.append(("S1-valid-green", check_register(doc) == []))

    d = copy.deepcopy(doc); d["deviations"] = []
    cases.append(("S2-empty-register-red", any(x.startswith("D6") for x in check_register(d))))

    d = copy.deepcopy(doc); d["deviations"][0]["evidence"][0]["contains"] = "NO_SUCH_TOKEN_XYZ"
    cases.append(("S3-token-rot-red", any(x.startswith("D2") for x in check_register(d))))

    d = copy.deepcopy(doc); d["deviations"][0]["evidence"][0]["line"] = 10 ** 9
    cases.append(("S4-line-out-of-range-red", any(x.startswith("D2") for x in check_register(d))))

    d = copy.deepcopy(doc); d["deviations"][0]["evidence"][0]["file"] = "no/such/file.cpp"
    cases.append(("S5-missing-file-red", any(x.startswith("D2") for x in check_register(d))))

    d = copy.deepcopy(doc); d["deviations"][1]["id"] = d["deviations"][0]["id"]
    cases.append(("S6-duplicate-id-red", any(x.startswith("D4") for x in check_register(d))))

    d = copy.deepcopy(doc); d["deviations"][0]["severity"] = "catastrophic"
    cases.append(("S7-bad-severity-red", any(x.startswith("D4") for x in check_register(d))))

    d = copy.deepcopy(doc); d["owner_decision_required"] = False
    cases.append(("S8-blocker-without-owner-flag-red",
                  any(x.startswith("D5") for x in check_register(d))))

    # D3：把证据指向注释行
    d = copy.deepcopy(doc)
    p = os.path.join(REPO, d["deviations"][0]["evidence"][0]["file"])
    lines = io.open(p, encoding="utf-8", errors="replace").read().splitlines()
    for i, t in enumerate(lines, 1):
        if t.strip().startswith("//"):
            d["deviations"][0]["evidence"][0]["line"] = i
            d["deviations"][0]["evidence"][0]["contains"] = t.strip()[:12]
            break
    cases.append(("S9-comment-line-evidence-red",
                  any(x.startswith("D3") for x in check_register(d))))

    # D7：删掉 OPEN_QUESTIONS 引用
    d = copy.deepcopy(doc)
    import tempfile, shutil
    tmp = tempfile.mkdtemp()
    os.makedirs(os.path.join(tmp, "工程控制/RELEASE-05"), exist_ok=True)
    io.open(os.path.join(tmp, "工程控制/RELEASE-05/OPEN_QUESTIONS.md"), "w",
            encoding="utf-8").write("no ids here\n")
    errs = check_register(d, repo=tmp)
    shutil.rmtree(tmp, ignore_errors=True)
    cases.append(("S10-blocker-not-escalated-red", any(x.startswith("D7") for x in errs)))

    bad = [n for n, g in cases if not g]
    for n, g in cases:
        print("SELFTEST " + ("PASS " if g else "FAIL ") + n)
    if bad:
        print("SELFTEST_FAIL: " + repr(bad), file=sys.stderr)
        return 1
    print("SELFTEST_PASS: %d/%d" % (len(cases), len(cases)))
    return 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--register", default=REGISTER)
    ap.add_argument("--json-out")
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args()
    if args.self_test:
        return _self_test()
    errs, doc = run(args.register)
    verdict = "PASS" if not errs else "FAIL"
    n = len(doc["deviations"]) if doc else 0
    nb = sum(1 for d in (doc or {}).get("deviations", []) if d.get("severity") == "blocker")
    print("BLOCK_FLOW_CONFORMANCE_%s: deviations=%d blockers=%d errors=%d"
          % (verdict, n, nb, len(errs)))
    for e in errs[:40]:
        print("  " + e)
    if args.json_out:
        os.makedirs(os.path.dirname(args.json_out), exist_ok=True)
        io.open(args.json_out, "w", encoding="utf-8").write(json.dumps(
            {"tool": "check_block_flow_conformance", "register": args.register,
             "deviations": n, "blockers": nb, "errors": errs, "verdict": verdict},
            ensure_ascii=False, indent=1) + "\n")
    return 0 if not errs else 1


if __name__ == "__main__":
    sys.exit(main())
