import sys, json, os, hashlib
sys.stdout.reconfigure(encoding='utf-8')
root = r"F:/Astro dev/Astro CS Normalization Database"
d = json.load(open(os.path.join(root,"eng/packaging/config/filters.json"), encoding="utf-8"))
for sec in ("transcription","provenance"):
    sp = d[sec]["source_path"]
    full = os.path.join(root, sp.replace("/", os.sep))
    print(f"--- {sec} ---")
    print("source_path:", sp, "exists:", os.path.exists(full))
    if os.path.exists(full):
        b = open(full,"rb").read()
        print("  actual bytes:", len(b), " declared:", d[sec].get("source_bytes"))
        print("  actual sha256:", hashlib.sha256(b).hexdigest())
        print("  declared sha256:", d[sec]["source_sha256"])
        print("  match:", hashlib.sha256(b).hexdigest()==d[sec]["source_sha256"])
print("--- tracked? ---")
os.system(f'cd "{root}" && git ls-files --error-unmatch "lib/algorithms/photometry/data/response_curves/filters.json" "lib/algorithms/photometry/cpp/test/filter_qe_provenance.json"')
print("counts: transcription.filter_count =", d["transcription"]["filter_count"],
      "| per_filter =", len(d["provenance"]["per_filter"]),
      "| filters =", len(d["filters"]))
print("name sets equal:", set(d["provenance"]["per_filter"])==set(d["filters"]))
print("only in filters:", sorted(set(d["filters"])-set(d["provenance"]["per_filter"])))
print("only in per_filter:", sorted(set(d["provenance"]["per_filter"])-set(d["filters"])))
