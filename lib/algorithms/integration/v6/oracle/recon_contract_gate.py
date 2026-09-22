#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""recon_contract_gate.py — sparse_snr_layer 重建声明的合同门（能红能绿）。

校验 eng/contracts/schemas/unified/sparse_snr_layer.schema.json 对**本次新增的两个
声明面**真实生效（用仓库自带的零依赖校验器 eng/tests/common/jsonschema_min.py，
不引第三方）：

  C1 正例 eng/contracts/schemas/unified/examples/sparse_snr_layer.example.json 通过
  C2 reconstruction_operator 取值不在冻结词表内 ⇒ 判红（enum）
  C3 control_point_geometry.node_placement 声明为角点锚定 ⇒ 判红（const）
  C4 缺 sparse_snr_semantics ⇒ 判红（既有门未放松）
  C5 既有负例 n6（相对语义）仍按原门判红（既有门未放松）

用法: python3 recon_contract_gate.py [--out <json>]
"""
from __future__ import annotations

import argparse
import importlib.util
import json
import os
import sys

_HERE = os.path.dirname(os.path.abspath(__file__))
_REPO = os.path.abspath(os.path.join(_HERE, "..", "..", "..", "..", ".."))
_UNIFIED = os.path.join(_REPO, "eng", "contracts", "schemas", "unified")
_SCHEMA = os.path.join(_UNIFIED, "sparse_snr_layer.schema.json")
_EXAMPLE = os.path.join(_UNIFIED, "examples", "sparse_snr_layer.example.json")
_NEG6 = os.path.join(_UNIFIED, "negative", "n6_sparse_snr_relative_semantics.schema-violation.json")


def load(p):
    with open(p, encoding="utf-8") as fh:
        return json.load(fh)


def merged_errors(doc, schema, jm):
    return " | ".join("%s:%s" % ("/".join(str(x) for x in p) or "<root>", m)
                      for p, m in jm.validate(doc, schema))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(_HERE, "recon_contract_gate.json"))
    args = ap.parse_args()

    spec = importlib.util.spec_from_file_location(
        "jsonschema_min", os.path.join(_REPO, "eng", "tests", "common", "jsonschema_min.py"))
    jm = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(jm)

    schema = load(_SCHEMA)
    example = load(_EXAMPLE)
    gates = []

    def gate(name, ok, detail):
        gates.append({"gate": name, "verdict": "PASS" if ok else "FAIL", "detail": detail})
        print("  [%s] %s — %s" % ("PASS" if ok else "FAIL", name, detail), flush=True)

    errs = merged_errors(example, schema, jm)
    gate("C1_positive_example_valid", errs == "", errs or "no error")

    for tok in ("spline_natural_v1", "natural_bicubic_spline_v1", "", "nn"):
        bad = json.loads(json.dumps(example))
        bad["reconstruction_operator"] = tok
        e = merged_errors(bad, schema, jm)
        gate("C2_operator_%s_rejected" % (tok or "<empty>"),
             "reconstruction_operator:enum" in e, e or "unexpectedly accepted")

    bad = json.loads(json.dumps(example))
    bad["control_point_geometry"]["node_placement"] = "cell_corner_v1"
    e = merged_errors(bad, schema, jm)
    gate("C3_corner_anchored_node_placement_rejected",
         "control_point_geometry/node_placement:const" in e, e or "unexpectedly accepted")

    ok = json.loads(json.dumps(example))
    ok["control_point_geometry"]["node_placement"] = "cell_center_v1"
    e = merged_errors(ok, schema, jm)
    gate("C3b_cell_center_node_placement_accepted", e == "", e or "no error")

    bad = json.loads(json.dumps(example))
    del bad["sparse_snr_semantics"]
    e = merged_errors(bad, schema, jm)
    gate("C4_missing_semantics_still_rejected", "sparse_snr_semantics" in e, e or "unexpectedly accepted")

    neg6 = load(_NEG6)
    e = merged_errors(neg6, schema, jm)
    gate("C5_negative_n6_still_rejected_by_semantics_gate",
         "sparse_snr_semantics:const" in e, e or "unexpectedly accepted")

    result = {"oracle": "recon_contract_gate.py",
              "schema": os.path.relpath(_SCHEMA, _REPO),
              "example": os.path.relpath(_EXAMPLE, _REPO),
              "gates": gates,
              "n_pass": sum(1 for g in gates if g["verdict"] == "PASS"),
              "n_fail": sum(1 for g in gates if g["verdict"] == "FAIL")}
    with open(args.out, "w", encoding="utf-8") as fh:
        json.dump(result, fh, indent=2, ensure_ascii=False)
    print("\n== recon contract gate: %d passed, %d failed ==" % (result["n_pass"], result["n_fail"]))
    print("evidence -> " + args.out)
    return 0 if result["n_fail"] == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
