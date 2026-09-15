
import re, os, json, collections
FILES={
 'lib/calibration/src/module_entry.cpp':'calibration',
 'lib/cosmetic/src/module_entry.cpp':'cosmetic',
 'lib/hips/src/module_entry.cpp':'hips',
 'lib/drizzle/src/module_entry.cpp':'drizzle',
 'lib/gaia_xpsd_client/src/module_entry.c':'gaia',
 'lib/snr_estimator/src/module_entry.cpp':'snr',
}
def strip(t):
    t=re.sub(r'/\*.*?\*/','',t,flags=re.S); t=re.sub(r'//[^\n]*','',t); return t
for p,mod in FILES.items():
    raw=open(p,encoding='utf-8',errors='replace').read()
    t=strip(raw)
    # vocab entries: { "name", ... } inside *_VOCAB tables
    vocab=set()
    for m in re.finditer(r'(CFG_VOCAB|cfg_keys|KEY_VOCAB|kCfgKeys)\s*(?:\[[^\]]*\])?\s*=\s*\{(.*?)\};', t, re.S):
        for mm in re.finditer(r'"([A-Za-z0-9_]+)"', m.group(1) if False else m.group(2)):
            vocab.add(mm.group(1))
    # also collect key-name string literals used with kv_find/kv_u64/... style helpers
    used=collections.Counter()
    for m in re.finditer(r'\b(?:kv_find|kv_u64|kv_f64|kv_str|kv_bool|cfg_get|json_str|json_num|json_bool|get_str|get_num|get_bool|has_key)\s*\([^;]{0,120}?"([A-Za-z0-9_]+)"', t):
        used[m.group(1)]+=1
    for m in re.finditer(r'\.value\(\s*"([A-Za-z0-9_]+)"', t): used[m.group(1)]+=1
    print(f'=== {mod}  ({os.path.basename(os.path.dirname(os.path.dirname(p)))})  vocab={len(vocab)} helper-used={len(used)}')
    print('  VOCAB:', sorted(vocab))
    print('  USED :', sorted(used))
    if vocab:
        print('  IN-VOCAB-BUT-NO-READ-HIT:', sorted(vocab-set(used)))
