# -*- coding: utf-8 -*-
"""QA-MATRIX-001 组装器：data/*.json -> qa_matrix.json（唯一发布规格）。

用法：python3 assemble.py [--out qa_matrix.json]
退出码：0 成功；2 结构错误。
"""
from __future__ import annotations
import json, os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
DATA = os.path.join(ROOT, "data")


def load(name):
    with open(os.path.join(DATA, name), encoding="utf-8") as f:
        return json.load(f)


def emit(out_path=None):
    meta = load("meta.json")
    lit = load("gates_analytic.json")["literature"]
    gates = []
    for fn in ("gates_analytic.json", "gates_mc.json", "gates_inj.json",
               "gates_realdata_baseline_p0.json"):
        gates.extend(load(fn)["gates"])
    for g in gates:
        zc = g.setdefault("zero_case_red", {})
        zc.setdefault("mechanism", meta["zero_case_red"]["mechanism"])
        zc.setdefault("runner_rule", meta["zero_case_red"]["runner_rule"])
        zc["min_cases"] = g.pop("min_cases")
        g["literature"] = [lit[k] for k in g.get("lit", []) if k in lit]
        # normalise criterion（authoring: metric/op/thresh/unit/status/owner/ref）
        c = g["criterion"]
        c["kind"] = c.get("kind", "structural" if "rule" in c else "numeric")
        c["threshold_unit"] = c.pop("unit", c.get("threshold_unit", ""))
        c["threshold_status"] = c.pop("status", c.get("threshold_status"))
        c["threshold_owner"] = c.pop("owner", c.get("threshold_owner", ""))
        c.setdefault("reference_value", "")
        g["zcr_rule"] = meta["zero_case_red"]["runner_rule"]
    qm = {
        "schema": meta["schema"], "task": meta["task"], "wave": meta["wave"],
        "write_scope": meta["write_scope"], "baseline_head": meta["baseline_head"],
        "production_modes": meta["production_modes"],
        "baseline_modes": meta["baseline_modes"],
        "deferred_modes": meta["deferred_modes"],
        "forbidden_weight_source_tokens": meta["forbidden_weight_source_tokens"],
        "forbidden_production_modes": meta["forbidden_production_modes"],
        "forbidden_psfsw_keys": meta["forbidden_psfsw_keys"],
        "required_freeze_ids": meta["required_freeze_ids"],
        "all_freeze_ids": meta["all_freeze_ids"],
        "ledger_layer_note": meta["ledger_layer_note"],
        "zero_case_red": meta["zero_case_red"],
        "literature": lit,
        "gates": gates,
    }
    out_path = out_path or os.path.join(ROOT, "qa_matrix.json")
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(qm, f, ensure_ascii=False, indent=1)
        f.write("\n")
    return out_path, len(gates)


if __name__ == "__main__":
    out = sys.argv[2] if len(sys.argv) > 2 and sys.argv[1] == "--out" else None
    p, n = emit(out)
    print("assembled gates=%d -> %s" % (n, p))
    sys.exit(0)
