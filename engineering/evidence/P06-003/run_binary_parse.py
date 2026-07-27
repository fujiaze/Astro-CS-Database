import sys, json, io
sys.path.insert(0, "engineering/evidence/P06-003")
from parse_hcsd_binary import parse_hcsd

files = [
    ("engineering/evidence/P06-002/T1_baseline/output/T1_baseline.hcsd", 200000, "engineering/evidence/P06-003/logs/hcsd_binary_parse_T1.json"),
    ("engineering/evidence/P06-002/T6_determinism/output/T6_run1.hcsd", 100000, "engineering/evidence/P06-003/logs/hcsd_binary_parse_T6.json"),
]

results = {}
for path, sample, out_path in files:
    r = parse_hcsd(path, max_ipix_sample=sample)
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(r, f, indent=2, default=str, ensure_ascii=False)
    results[path] = r.get("summary", {})
    print(f"[OK] {path} -> {out_path}")
    s = r.get("summary", {})
    print(f"  magic_ok={s.get('magic_ok')} json_parse_ok={s.get('json_parse_ok')} non_empty={r['leaf_index']['non_empty_leaves']} sum_eq_npix={s.get('sum_data_length_equals_n_pix')} ascending={s.get('sorted_ipix_ascending')} leaf_sorted={s.get('sorted_ipix_leaf_sorted')} file_size_matches={s.get('file_size_matches')}")

with open("engineering/evidence/P06-003/logs/hcsd_binary_parse_summary.json", "w", encoding="utf-8") as f:
    json.dump(results, f, indent=2, default=str, ensure_ascii=False)
print("\n[SUMMARY] written to engineering/evidence/P06-003/logs/hcsd_binary_parse_summary.json")
