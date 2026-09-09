#!/usr/bin/env python3
"""cpu_profile 校验器 (06 §5, BENCH-004) — schema 最小校验+失效(stale)判定。
用法: python3 tools/validate_cpu_profile.py <profile.json> [--hardware <inspect.json>] [--commit <sha>]
输出: VALID | SCHEMA_FAIL <reason> | STALE <reason>
"""
import argparse
import hashlib
import json
import os
import re
import sys


# oracle 口径唯一事实源 = schemas/cpu_profile.schema.json (oracle_status const "PASS")。
# lib/backend_host/profile_gen.cpp 历史写小写 "pass" (该文件禁改)，读取侧统一
# 归一到大写口径后再比较/校验，消除三方分裂。
ORACLE_PASS = "PASS"


def normalize_oracle_status(d):
    """把 d["kernels"] 每项 oracle_status 归一为 schema const 口径 (大写 PASS)。

    返回 {index: 归一前原始值}，供 STALE 报告原样引用。
    """
    raw = {}
    if not isinstance(d, dict):
        return raw
    kernels = d.get("kernels")
    if not isinstance(kernels, list):
        return raw
    for i, k in enumerate(kernels):
        if isinstance(k, dict):
            raw[i] = k.get("oracle_status")
            k["oracle_status"] = str(k.get("oracle_status", "")).strip().upper()
    return raw


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


def _stale(reason):
    print(f"STALE {reason}")
    return 1


def main():
    ap = argparse.ArgumentParser(
        description="cpu_profile 校验器: schema 最小校验 + oracle/硬件/commit 失效(stale)判定")
    ap.add_argument("profile", help="cpu_profile JSON 文件路径")
    ap.add_argument("--hardware", default=None, help="硬件巡检 JSON (可选)")
    ap.add_argument("--commit", default=None, help="期望 commit sha (可选)")
    args = ap.parse_args()

    # SCHEMA 断链修复 (V8-CI-012 R6.5): 产品 schema 唯一事实源 = schemas/
    # (旧 工程控制/RELEASE_V5 路径为 untracked 控制包布局, tracked 工作区不存在)。
    schema = json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                         "..", "contracts", "schemas", "cpu_profile.schema.json"),
                            encoding="utf-8"))
    d = json.load(open(args.profile, encoding="utf-8"))
    raw_oracle = normalize_oracle_status(d)
    err = validate_schema(d, schema)
    if err:
        print(f"SCHEMA_FAIL {err}")
        return 1

    # oracle 失败 → verdict FAIL(正确性筛选优先于任何速度)。
    # 归一后统一比较大写 PASS；小写 "pass" (profile_gen.cpp 历史写出口径) 合法。
    for i, k in enumerate(d.get("kernels", [])):
        if k.get("oracle_status") != ORACLE_PASS:
            print(f"STALE oracle_status={raw_oracle.get(i, k.get('oracle_status'))} "
                  f"for {k.get('kernel_id')}")
            return 1

    # 失效判定(06 §5): CPU identity/ISA state/affinity/build
    if args.hardware:
        hw = json.load(open(args.hardware, encoding="utf-8"))
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
    if args.commit and d["build"]["commit"] != args.commit:
        return _stale(f"commit changed: {d['build']['commit'][:12]} != {args.commit[:12]}")

    print("VALID")
    return 0


if __name__ == "__main__":
    sys.exit(main())
