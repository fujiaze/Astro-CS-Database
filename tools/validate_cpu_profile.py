#!/usr/bin/env python3
"""cpu_profile 校验器 (06 §5, BENCH-004) — schema 最小校验+失效(stale)判定。
用法: python3 tools/validate_cpu_profile.py <profile.json> [--hardware <inspect.json>] [--commit <sha>]
输出: VALID | SCHEMA_FAIL <reason> | STALE <reason>
"""
import hashlib, json, sys


def validate_schema(d, schema):
    if not isinstance(d, dict):
        return "not an object"
    for k in schema.get("required", []):
        if k not in d:
            return f"missing required '{k}'"
    if schema.get("additionalProperties") is False:
        for k in d:
            if k not in schema.get("properties", {}):
                return f"unknown key '{k}'"
    for k, rule in schema.get("properties", {}).items():
        if k not in d:
            continue
        v = d[k]
        if "const" in rule and v != rule["const"]:
            return f"{k} != const"
        t = rule.get("type")
        if t == "integer" and not isinstance(v, int):
            return f"{k} not integer"
        if t == "string" and not isinstance(v, str):
            return f"{k} not string"
        if t == "array" and not isinstance(v, list):
            return f"{k} not array"
        if "minimum" in rule and isinstance(v, (int, float)) and v < rule["minimum"]:
            return f"{k} < minimum"
        if "pattern" in rule and isinstance(v, str) and not re.match(rule["pattern"], v):
            return f"{k} pattern mismatch"
        if isinstance(rule, dict) and rule.get("type") == "array" and "items" in rule:
            for item in v:
                if isinstance(item, dict):
                    for ik in rule["items"].get("required", []):
                        if ik not in item:
                            return f"kernels item missing '{ik}'"
    return None


def main():
    import re
    profile_path = sys.argv[1]
    hw_path, commit = None, None
    args = sys.argv[2:]
    for i, a in enumerate(args):
        if a == "--hardware" and i + 1 < len(args):
            hw_path = args[i + 1]
        if a == "--commit" and i + 1 < len(args):
            commit = args[i + 1]

    schema = json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                         "..", "工程控制", "RELEASE_V5",
                                         "AstroCS_MAIN_RELEASE_CONTROL_V5_SINGLE_CLI_AMD64_20260828",
                                         "schemas", "cpu_profile.schema.json"), encoding="utf-8"))
    d = json.load(open(profile_path, encoding="utf-8"))
    err = validate_schema(d, schema)
    if err:
        print(f"SCHEMA_FAIL {err}")
        return 1

    # oracle 失败 → verdict FAIL(正确性筛选优先于任何速度)
    for k in d.get("kernels", []):
        if k.get("oracle_status") != "pass":
            print(f"STALE oracle_status={k.get('oracle_status')} for {k.get('kernel_id')}")
            return 1

    # 失效判定(06 §5): CPU identity/ISA state/affinity/build
    if hw_path:
        hw = json.load(open(hw_path, encoding="utf-8"))
        ph = d["hardware"]
        if hw.get("feature_bits") != ph.get("feature_bits"):
            return _stale("OS ISA state changed")
        if hw.get("affinity") != ph.get("affinity"):
            return _stale(f"affinity changed: {hw.get('affinity')} != {ph.get('affinity')}")
        src = (hw.get("vendor", "") + "|" + str(hw.get("family", 0)) + "|" +
               str(hw.get("model", 0)) + "|" + str(hw.get("stepping", 0)) + "|" +
               str(hw.get("feature_bits", 0)) + "|" + str(hw.get("xcr0", 0)) + "|" +
               str(hw.get("available_logical_cpus", 0)))
        fp = hashlib.sha256(src.encode()).hexdigest()
        if fp != ph["fingerprint"]:
            return _stale(f"hardware fingerprint changed: {fp[:16]} != {ph['fingerprint'][:16]}")
    if commit and d["build"]["commit"] != commit:
        return _stale(f"commit changed: {d['build']['commit'][:12]} != {commit[:12]}")

    print("VALID")
    return 0


def _stale(reason):
    print(f"STALE {reason}")
    return 1


if __name__ == "__main__":
    import os
    sys.exit(main())
