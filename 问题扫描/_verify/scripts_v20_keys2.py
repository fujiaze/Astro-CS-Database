
import re, collections
files=['lib/core/src/module_adapters.cpp','lib/calibration/src/module_entry.cpp','lib/hips/src/module_entry.cpp','lib/drizzle/src/module_entry.cpp','lib/gaia_xpsd_client/src/module_entry.c','lib/snr_estimator/src/module_entry.cpp','lib/cosmetic/src/module_entry.cpp']
for p in files:
    src=open(p,encoding='utf-8',errors='replace').read()
    lines=src.split('\n')
    keys=collections.Counter(); sites=collections.defaultdict(list)
    # value("k", default)  /  contains("k") / count("k") / find("k")
    pat1 = re.compile(r'\.(?:value|contains|count|find|at)\s*\(\s*"([A-Za-z0-9_]+)"')
    pat2 = re.compile(r'\b(?:p1_int|p1_flag|p1_num|p1_has)\s*\([^,]+,\s*"([A-Za-z0-9_]+)"')
    for i,l in enumerate(lines,1):
        for pat in (pat1,pat2):
            for m in pat.finditer(l):
                keys[m.group(1)]+=1; sites[m.group(1)].append(i)
    print('=== '+p+'  distinct='+str(len(keys))+' sites='+str(sum(keys.values())))
    for k,c in sorted(keys.items()):
        print(f'  {k}\t{c}\t{sites[k][:8]}')
