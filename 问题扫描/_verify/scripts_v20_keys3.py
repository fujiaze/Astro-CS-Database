
import re, os, subprocess, json, collections
FILES=['lib/core/src/module_adapters.cpp','lib/calibration/src/module_entry.cpp','lib/hips/src/module_entry.cpp','lib/drizzle/src/module_entry.cpp','lib/gaia_xpsd_client/src/module_entry.c','lib/snr_estimator/src/module_entry.cpp','lib/cosmetic/src/module_entry.cpp']
pat1 = re.compile(r'\.(?:value|contains|count|find|at)\s*\(\s*"([A-Za-z0-9_]+)"')
pat2 = re.compile(r'\b(?:p1_int|p1_flag|p1_num|p1_has|kv_u64|kv_f64|kv_str|kv_bool|kv_find)\s*\([^,]+,\s*"([A-Za-z0-9_]+)"')
pat3 = re.compile(r'^\s*\{\s*"([A-Za-z0-9_]+)"\s*,', re.M)   # vocab tables
keys=collections.defaultdict(set)
for p in FILES:
    src=open(p,encoding='utf-8',errors='replace').read()
    lines=src.split('\n')
    for i,l in enumerate(lines,1):
        # strip // comments for parse-site detection
        code=l.split('//')[0]
        for pat in (pat1,pat2):
            for m in pat.finditer(code):
                keys[m.group(1)].add(os.path.basename(p)+':'+str(i))
print('total parse-site keys:', len(keys))
json.dump({k:sorted(v) for k,v in keys.items()}, open('问题扫描/_cache/v20_keys.json','w'), indent=0)
for k in sorted(keys): print(k, sorted(keys[k])[:6])
