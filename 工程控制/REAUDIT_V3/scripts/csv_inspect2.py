import csv, collections
ROOT = "/home/lighthouse/astrocs_audit_v2/AstroCS_MAIN_AUDIT_SUPPLEMENT_20260826T055752Z_535e73879662"
rows = list(csv.DictReader(open(ROOT + "/package/07_cross_layer/cross_layer_matrix.csv")))
# distribution of consistency_status
c = collections.Counter(r["consistency_status"] for r in rows)
print("consistency_status:", dict(c))
# rows whose evidence mentions auto-derived
auto = [r for r in rows if "auto-derived" in r["evidence"]]
print("evidence mentions auto-derived:", len(auto))
if auto:
    r = auto[0]
    print("--- sample row ---")
    for k, v in r.items():
        print(f"  {k} = {v[:150]}")
