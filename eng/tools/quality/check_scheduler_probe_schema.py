#!/usr/bin/env python3
"""CONTRACT-501 探针事件 schema 校验器（ARCH-502 探针面机器门）。

判据（fail-closed，逐行）：
  ① 每行必须是合法 JSON 对象；
  ② 必填字段 ts/stage/kind/value/unit 齐备且类型正确（CONTRACT-501 SCHEDULER_CONTRACT §4）；
  ③ stage ∈ {normalize, mosaic, export}；kind ∈ 8 类事件名；unit ∈ {s, B, 1}；
  ④ 事件名与单位的一致性：node_wall/queue_wait ⇒ unit=s；block_birth/block_death ⇒ unit=B；
  ⑤ 空文件 / 文件不存在 / 无输出 ⇒ 判红（不得静默通过）。

用法：
  python3 eng/tools/quality/check_scheduler_probe_schema.py --input <jsonl> [--json-out <json>]
  python3 eng/tools/quality/check_scheduler_probe_schema.py --self-test
"""
import argparse
import io
import json
import os
import sys

STAGES = {"normalize", "mosaic", "export"}
KINDS = {"node_wall", "queue_wait", "block_birth", "block_death", "rss", "io",
         "worker_busy", "cache_hit"}
UNITS = {"s", "B", "1"}
REQUIRED = {"ts": (int, float), "stage": str, "kind": str, "value": (int, float), "unit": str}
UNIT_OF_KIND = {"node_wall": "s", "queue_wait": "s", "block_birth": "B", "block_death": "B"}


def validate_lines(lines):
    """返回 (ok, errors)。"""
    errors = []
    n = 0
    for i, raw in enumerate(lines, 1):
        raw = raw.strip()
        if not raw:
            continue
        n += 1
        try:
            ev = json.loads(raw)
        except Exception as exc:  # noqa: BLE001
            errors.append("line %d: bad JSON: %s" % (i, exc))
            continue
        if not isinstance(ev, dict):
            errors.append("line %d: not a JSON object" % i)
            continue
        for k, ty in REQUIRED.items():
            if k not in ev:
                errors.append("line %d: missing required field %r" % (i, k))
            elif not isinstance(ev[k], ty):
                errors.append("line %d: field %r wrong type %s" % (i, k, type(ev[k]).__name__))
        if ev.get("stage") not in STAGES:
            errors.append("line %d: stage %r not in %s" % (i, ev.get("stage"), sorted(STAGES)))
        if ev.get("kind") not in KINDS:
            errors.append("line %d: kind %r not in %s" % (i, ev.get("kind"), sorted(KINDS)))
        if ev.get("unit") not in UNITS:
            errors.append("line %d: unit %r not in %s" % (i, ev.get("unit"), sorted(UNITS)))
        want = UNIT_OF_KIND.get(ev.get("kind"))
        if want and ev.get("unit") != want:
            errors.append("line %d: kind %r requires unit %r, got %r"
                          % (i, ev.get("kind"), want, ev.get("unit")))
    if n == 0:
        errors.append("no events: 空文件/无输出判红（fail-closed）")
    return (not errors), errors, n


def run(path):
    if not os.path.isfile(path):
        return False, ["input not found: %s" % path], 0
    text = io.open(path, encoding="utf-8").read()
    if not text.strip():
        return False, ["input is empty: %s" % path], 0
    return validate_lines(text.splitlines())


def _self_test():
    cases = []
    good = json.dumps({"ts": 1.0, "stage": "normalize", "kind": "node_wall", "value": 0.5,
                       "unit": "s", "node": "n0"}, ensure_ascii=False)
    ok, _, n = validate_lines([good])
    cases.append(("S1-valid-green", ok is True and n == 1))
    ok, errs, _ = validate_lines([json.dumps({"ts": 1.0, "stage": "normalize",
                                              "kind": "node_wall", "value": 0.5})])
    cases.append(("S2-missing-unit-red", ok is False))
    ok, _, _ = validate_lines([json.dumps({"ts": 1.0, "stage": "nope", "kind": "node_wall",
                                           "value": 0.5, "unit": "s"})])
    cases.append(("S3-bad-stage-red", ok is False))
    ok, _, _ = validate_lines([json.dumps({"ts": 1.0, "stage": "normalize", "kind": "node_wall",
                                           "value": 0.5, "unit": "B"})])
    cases.append(("S4-unit-kind-mismatch-red", ok is False))
    ok, _, _ = validate_lines(["not json"])
    cases.append(("S5-bad-json-red", ok is False))
    ok, _, _ = validate_lines([])
    cases.append(("S6-empty-red", ok is False))
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
    ap.add_argument("--input")
    ap.add_argument("--json-out")
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args()
    if args.self_test:
        return _self_test()
    if not args.input:
        print("--input required (or --self-test)", file=sys.stderr)
        return 1
    ok, errors, n = run(args.input)
    verdict = "PASS" if ok else "FAIL"
    print("SCHEDULER_PROBE_SCHEMA_%s: events=%d errors=%d input=%s"
          % (verdict, n, len(errors), args.input))
    for e in errors[:40]:
        print("  " + e)
    if args.json_out:
        os.makedirs(os.path.dirname(args.json_out), exist_ok=True)
        io.open(args.json_out, "w", encoding="utf-8").write(json.dumps(
            {"tool": "check_scheduler_probe_schema", "input": args.input, "events": n,
             "errors": errors, "verdict": verdict}, ensure_ascii=False, indent=1) + "\n")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
