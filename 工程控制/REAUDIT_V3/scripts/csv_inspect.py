import csv
ROOT = "/home/lighthouse/astrocs_audit_v2/AstroCS_MAIN_AUDIT_SUPPLEMENT_20260826T055752Z_535e73879662"
rows = list(csv.DictReader(open(ROOT + "/package/07_cross_layer/cross_layer_matrix.csv")))
print("header:", list(rows[0].keys()))
auto = [r for r in rows if "auto-derived" in r["consistency_status"]]
print("auto-derived rows:", len(auto))
if auto:
    r = auto[0]
    for k, v in r.items():
        print(f"  {k} = {v[:120]}")
