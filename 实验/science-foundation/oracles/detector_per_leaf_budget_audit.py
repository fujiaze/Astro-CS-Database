"""Audit individual absolute overlap intervals; do not alter solver statuses."""
import hashlib
import json
from pathlib import Path


if __name__ == "__main__":
    root = Path(__file__).resolve().parent.parent
    path = root/"evidence"/"detector_independent.json"
    evidence = json.loads(path.read_text(encoding="utf-8"))
    rows = []
    for case in evidence["cases"]:
        result = case["enclosure"]
        widths = {key:value["upper_sr"]-value["lower_sr"] for key,value in result["areas"].items()}
        maximum = max(widths.values(),default=0.)
        rows.append({"name":case["name"],"solver_status":result["status"],
                     "maximum_leaf_interval_width_sr":maximum,
                     "requested_absolute_width_sr":result["per_leaf_width_budget_sr"],
                     "each_individual_leaf_meets_budget":maximum<=result["per_leaf_width_budget_sr"],
                     "all_iid_intervals_intersect":case["all_iid_confidence_intervals_intersect"],
                     "tan_analytic_total_inside_enclosure":case.get("analytic_total_inside_enclosure")})
    current = hashlib.sha256((root/"oracles"/"detector_independent.py").read_bytes()).hexdigest()
    out = {"input_sha256":hashlib.sha256(path.read_bytes()).hexdigest(),
           "solver_source_matches_evidence":current==evidence["source_sha256"],
           "cases":rows,"individual_leaf_budget_passes":sum(row["each_individual_leaf_meets_budget"] for row in rows),
           "global_budget_statuses_met":sum(row["solver_status"]=="tolerance_met" for row in rows),
           "iid_confidence_intersection_passes":sum(row["all_iid_intervals_intersect"] for row in rows),
           "tan_analytic_checks":sum(row["tan_analytic_total_inside_enclosure"] is not None for row in rows),
           "tan_analytic_passes":sum(row["tan_analytic_total_inside_enclosure"] is True for row in rows),
           "maximum_forward_density_finite_difference_relative_error":max(abs(c["forward_density_check"]["relative_difference"]) for c in evidence["cases"]),
           "all_negative_cases_rejected":all(c["rejected"] for c in evidence["negative_cases"]),
           "qualification":"Per-leaf widths may meet tolerance even when conservative global uncertainty sum does not. No original status is changed. Interval qualification remains conditional on external WCSLIB/HEALPix."}
    (root/"evidence"/"detector_per_leaf_budget_audit.json").write_text(json.dumps(out,indent=2),encoding="utf-8")
    print(json.dumps({key:value for key,value in out.items() if key!="cases"},indent=2))
