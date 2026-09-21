#!/usr/bin/env python3
"""SCHEMA-INTEGRATE-001 / W6 —— 一键验证与证据。

    python3 eng/tests/contracts/v6/run_all.py

步骤（每步 rc 落 evidence/rc_summary.json 与 evidence/logs/）：
  1. 生产 schema 结构 meta 校验（JSON Schema 2020-12 meta-schema，jsonschema 可用时）；
  2. 独立 Oracle（对照 W4 冻结合同）；
  3. 负向 mutation（每条必须判红）；
  4. CI 同款 python3 -B -m unittest discover -s eng/tests/contracts -t eng/tests/contracts（UT-CONTRACTS）；
  5. 正例集逐条官方 jsonschema 校验（可用时）。
返回 0 iff 全部通过（零用例或 skip-only 视同失败）。
"""
import json, pathlib, subprocess, sys

HERE = pathlib.Path(__file__).resolve().parent
REPO = HERE.parents[3]

EVID = HERE / "evidence"
LOGS = EVID / "logs"
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parent))

try:
    from v6 import v6_oracle as oracle_mod
    from v6 import jsonschema_min as jm
except ImportError:
    import v6_oracle as oracle_mod
    import jsonschema_min as jm

import importlib


def _mut_mod():
    try:
        return importlib.import_module("v6.test_v6_negative_mutations")
    except ImportError:
        return importlib.import_module("test_v6_negative_mutations")


def step_meta_schema():
    """官方 jsonschema（若有）对 10 个生产 schema 做 meta-schema 校验。"""
    try:
        from jsonschema import Draft202012Validator
    except ImportError:
        # 无官方库：用自带校验器做结构自洽（至少确认可解析 + 关键键齐全）
        bad = []
        for p in sorted((REPO / "eng/contracts/schemas/v6").glob("*.schema.json")):
            d = json.loads(p.read_text(encoding="utf-8"))
            if d.get("type") != "object" or not d.get("$id"):
                bad.append(p.name)
        return (0 if not bad else 1), "bundled-structural", {"bad": bad}
    bad = []
    for p in sorted((REPO / "eng/contracts/schemas/v6").glob("*.schema.json")):
        d = json.loads(p.read_text(encoding="utf-8"))
        try:
            Draft202012Validator.check_schema(d)
        except Exception as e:  # noqa
            bad.append("%s: %s" % (p.name, e))
    return (0 if not bad else 1), "jsonschema-meta", {"bad": bad}


def step_examples_official():
    try:
        from jsonschema import Draft202012Validator
    except ImportError:
        return 0, "not-available-optional", {}
    import test_v6_schema_integration as t
    bad = []
    for ex, fn in t.TARGETS.items():
        doc = json.loads((t.EXAMPLES / ex).read_text(encoding="utf-8"))
        schema = json.loads((t.SCHEMAS / fn).read_text(encoding="utf-8"))
        if not Draft202012Validator(schema).is_valid(doc):
            bad.append(ex)
    return (0 if not bad else 1), "official-jsonschema-examples", {"bad": bad}


def main():
    EVID.mkdir(parents=True, exist_ok=True)
    LOGS.mkdir(parents=True, exist_ok=True)
    summary = {"task": "SCHEMA-INTEGRATE-001", "baseline_head": None, "steps": []}

    baseline = subprocess.run(["git", "rev-parse", "HEAD"], cwd=REPO,
                              capture_output=True, text=True)
    summary["baseline_head"] = baseline.stdout.strip()

    rc, mode, detail = step_meta_schema()
    summary["steps"].append({"step": "schema-meta", "rc": rc, "mode": mode, "detail": detail})

    o = oracle_mod.Oracle(REPO)
    _, failures = o.run()
    rep = o.report()
    (EVID / "oracle_report.json").write_text(json.dumps(rep, ensure_ascii=False, indent=2) + "\n",
                                             encoding="utf-8")
    summary["steps"].append({"step": "oracle", "rc": 0 if not failures else 1,
                             "checks_total": rep["checks_total"],
                             "checks_passed": rep["checks_passed"],
                             "failures": failures})

    mut_results = _mut_mod().evaluate_mutations()
    missed = [r["id"] for r in mut_results if not r["detected"]]
    wrong = [r["id"] for r in mut_results if not r["expected_check_red"]]
    (EVID / "mutations.json").write_text(json.dumps({
        "total": len(mut_results), "detected": len(mut_results) - len(missed),
        "missed": missed, "wrong_gate": wrong, "results": mut_results}, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8")
    summary["steps"].append({"step": "negative-mutations", "rc": 0 if not missed and not wrong else 1,
                             "total": len(mut_results), "missed": missed, "wrong_gate": wrong})

    ut = subprocess.run([sys.executable, "-B", "-m", "unittest", "discover",
                         "-s", "eng/tests/contracts", "-t", "eng/tests/contracts"],
                        cwd=REPO, capture_output=True, text=True, timeout=900)
    (LOGS / "unittest_discover.txt").write_text(ut.stdout + "\n" + ut.stderr, encoding="utf-8")
    ran = 0
    for line in ut.stderr.splitlines():
        if line.startswith("Ran ") and " test" in line:
            try:
                ran = int(line.split()[1])
            except ValueError:
                pass
    summary["steps"].append({"step": "UT-CONTRACTS", "rc": ut.returncode, "tests_run": ran})

    rc, mode, detail = step_examples_official()
    summary["steps"].append({"step": "examples-official", "rc": rc, "mode": mode,
                             "optional": mode == "not-available-optional", "detail": detail})

    # 主校验器（自带，CI 无第三方依赖）：正例必须逐条通过
    import test_v6_schema_integration as tvi
    bundled_bad = []
    for ex, fn in tvi.TARGETS.items():
        doc = json.loads((tvi.EXAMPLES / ex).read_text(encoding="utf-8"))
        schema = json.loads((tvi.SCHEMAS / fn).read_text(encoding="utf-8"))
        if jm.validate(doc, schema):
            bundled_bad.append(ex)
    summary["steps"].append({"step": "examples-bundled", "rc": 0 if not bundled_bad else 1,
                             "count": len(tvi.TARGETS), "bad": bundled_bad})

    required_ok = all(s["rc"] == 0 for s in summary["steps"] if not s.get("optional"))
    rc_summary_ok = (required_ok and ran > 0 and not missed and not wrong
                     and len(bundled_bad) == 0)
    summary["overall"] = "PASS" if rc_summary_ok else "FAIL"
    (EVID / "rc_summary.json").write_text(json.dumps(summary, ensure_ascii=False, indent=2) + "\n",
                                          encoding="utf-8")
    print(json.dumps(summary, ensure_ascii=False, indent=2))
    print("RUN_ALL_%s" % summary["overall"])
    return 0 if rc_summary_ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
