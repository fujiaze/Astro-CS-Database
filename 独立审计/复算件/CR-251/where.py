import sys, json, os, hashlib
sys.stdout.reconfigure(encoding='utf-8')
root = r"F:/Astro dev/Astro CS Normalization Database"
p = os.path.join(root, "eng/packaging/config/filters.json")
raw = open(p, encoding="utf-8").read().splitlines()
print("total lines:", len(raw))
for i, line in enumerate(raw[:2000], 1):
    s = line.strip()
    if i <= 1500 and (s.startswith('"lookup"') or s.startswith('"filters"') or s.startswith('"provenance"') or s.startswith('"transcription"') or s.startswith('"authority"') or s.startswith('"aliases"') or s.startswith('"non_key_examples"') or s.startswith('"resolution_rule"') or s.startswith('"unknown_filter"') or s.startswith('"match"') or s.startswith('"case_sensitive"') or s.startswith('"normalization"')):
        print(i, line[:150])
