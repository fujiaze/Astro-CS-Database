#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CONTRACT-FREEZE-001 负向 mutation 驱动器：对机器冻结表的临时副本注入错误，独立 Oracle 必须 rc!=0。
正向控制：未注入副本必须 rc=0（非空门）。逐条落 rc 与命中检查。"""
import copy, json, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from oracle_contract_freeze import run_oracle, DEFAULT_MACHINE, jload

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", ".."))
OUT = os.path.join(ROOT, "reports", "v6", "contract-review", "mutations")
BASE = jload(DEFAULT_MACHINE)

def clause(M, cid):
    for c in M["clauses"]:
        if c["id"] == cid:
            return c
    raise KeyError(cid)

MUT = {}
def reg(fn):
    MUT[fn.__name__] = fn
    return fn

@reg
def M01_delete_required_freeze(M):
    M["clauses"] = [c for c in M["clauses"] if c["id"] != "FZ-UNIT-WINFO"]
@reg
def M02_change_frozen_value(M):
    clause(M, "FZ-UNIT-WINFO")["value"] = "ADU^2"
@reg
def M03_pending_written_as_frozen(M):
    c = clause(M, "FZ-FORMULA-DRIZZLE-SB"); c["status"] = "FROZEN"; c["owner_signoff"] = None; c["signoff_reason"] = None
@reg
def M04_pending_numeric_as_frozen(M):
    clause(M, "PSFSW-T-DEPTH")["status"] = "FROZEN"
@reg
def M05_change_numeric_value(M):
    clause(M, "PSFSW-T-DEPTH")["value"] = 0.50
@reg
def M06_median_snr_as_weight_source(M):
    M["declared_weight_sources"]["point_information"].append("median_source_snr")
@reg
def M07_psfsw_written_as_ivar(M):
    for m in M["weight_modes"]["mode_details"]:
        if m["mode"] == "psfsw_robust":
            m["units"] = "ADU^-2"
    M["declared_weight_sources"]["psfsw_robust"].append("psfsw_robust_weight")
@reg
def M08_psf_snr_power_in_production(M):
    M["weight_modes"]["production"].append("psf_snr_power")
@reg
def M09_remove_forbidden_token(M):
    M["forbidden"]["weight_source_tokens"] = [t for t in M["forbidden"]["weight_source_tokens"] if t != "median_source_snr"]
@reg
def M10_units_table_mismatch(M):
    for u in M["units_table"]:
        if u["symbol"] == "W_info":
            u["unit"] = "ADU^-1"
@reg
def M11_null_value_claimed_frozen(M):
    c = clause(M, "QF-G-INJ-01"); c["value"] = None; c["status"] = "FROZEN"; c["owner_signoff"] = None
@reg
def M12_drop_signoff_item(M):
    M["signoff_items"] = [s for s in M["signoff_items"] if s["id"] != "SO-05"]
@reg
def M13_remove_open_item(M):
    M["open_items"] = [o for o in M["open_items"] if o["id"] != "PF-07"]
@reg
def M14_remove_superseded_drizzle(M):
    M["superseded_sections"] = [s for s in M["superseded_sections"] if not s["file"].endswith("DRIZZLE.md")]
@reg
def M15_empty_gate(M):
    clause(M, "FZ-FORMULA-GLS")["gate"] = ""
@reg
def M16_empty_psfsw_forbidden_keys(M):
    M["forbidden"]["psfsw_forbidden_keys"] = []
@reg
def M17_psfsw_units_mismatch_units_table(M):
    for u in M["units_table"]:
        if u["symbol"] == "psfsw_robust_weight":
            u["unit"] = "ADU^-2"
@reg
def M18_coverage_as_weight_source(M):
    M["declared_weight_sources"]["equal"].append("coverage")
@reg
def M19_fuzzy_numeric_claimed_frozen(M):
    c = clause(M, "FZ-AP2S-KAPPA-MAX"); c["value"] = "TBD"; c["value_display"] = "TBD"; c["status"] = "FROZEN"; c["owner_signoff"] = None
@reg
def M20_psfsw_group_not_normalized(M):
    for m in M["weight_modes"]["mode_details"]:
        if m["mode"] == "psfsw_robust":
            m["group_normalized"] = False
@reg
def M21_source_binding_broken(M):
    c = clause(M, "FZ-AP2S-IDENT-RTOL"); c["value"] = 1e-3; c["value_display"] = "1e-3"
@reg
def M22_declared_baseline_promotes_deferred(M):
    M["weight_modes"]["documented_baseline"].append("psf_snr_power")
@reg
def M23_coverage_as_variance_in_psfsw(M):
    M["forbidden"]["psfsw_forbidden_keys"] = [k for k in M["forbidden"]["psfsw_forbidden_keys"] if k != "variance"]

def main():
    os.makedirs(OUT, exist_ok=True)
    pos_fails, pos_checks = run_oracle(DEFAULT_MACHINE)
    print("POSITIVE-CONTROL rc=%d checks=%d" % (1 if pos_fails else 0, len(pos_checks)))
    results = []
    all_caught = (not pos_fails)
    for name in sorted(MUT):
        M = copy.deepcopy(BASE)
        try:
            MUT[name](M)
        except Exception as e:
            print("MUTATION-ERROR %s: %s" % (name, e)); all_caught = False
            results.append({"id": name, "rc": None, "caught": False, "error": str(e)}); continue
        p = os.path.join(OUT, name + ".json")
        with open(p, "w", encoding="utf-8") as f:
            json.dump(M, f, ensure_ascii=False, indent=1)
        fails, checks = run_oracle(p)
        rc = 1 if fails else 0
        caught = rc != 0
        all_caught = all_caught and caught
        print("%-42s rc=%d caught=%s %s" % (name, rc, caught, ("; ".join(fails[:3])) if fails else "NOT-CAUGHT"))
        results.append({"id": name, "rc": rc, "caught": caught, "n_failures": len(fails),
                        "first_failures": fails[:5], "checks": len(checks)})
    summary = {"positive_control_rc": 1 if pos_fails else 0, "mutations_total": len(MUT),
               "caught": sum(1 for r in results if r["caught"]), "all_caught": all_caught,
               "results": results}
    with open(os.path.join(ROOT, "reports", "v6", "contract-review", "evidence", "mutations.json"), "w", encoding="utf-8") as f:
        json.dump(summary, f, ensure_ascii=False, indent=1)
    print("SUMMARY caught=%d/%d all_caught=%s" % (summary["caught"], len(MUT), all_caught))
    return 0 if all_caught else 1

if __name__ == "__main__":
    sys.exit(main())
