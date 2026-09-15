
import re, collections, os, json
files = [l.strip() for l in open('问题扫描/_cache/_v21_src.txt', encoding='utf-8') if l.strip()]
prod = [f for f in files if not f.startswith('tests/') and not f.startswith('tools/')]
# collect JSON key writes:  j["key"] = ... ;  doc["key"];  {{"key", val}}  ;  "key": in fprintf format strings
write_keys = collections.Counter(); read_keys = collections.Counter()
wp = [re.compile(r'\[\s*"([a-z0-9_]{2,40})"\s*\]\s*(?::?=|,|\))'),
      re.compile(r'\{\{\s*"([a-z0-9_]{2,40})"\s*,'),
      re.compile(r'\b(?:emplace|insert|push_back)\s*\(\s*"([a-z0-9_]{2,40})"'),
      re.compile(r'\\?"([a-z0-9_]{2,40})\\?"\s*:\s*[%"a-z0-9]')]
rp = [re.compile(r'\.value\s*\(\s*"([a-z0-9_]{2,40})"'),
      re.compile(r'\.contains\s*\(\s*"([a-z0-9_]{2,40})"'),
      re.compile(r'\[\s*"([a-z0-9_]{2,40})"\s*\]\s*(?!\s*=)'),
      re.compile(r'\.at\s*\(\s*"([a-z0-9_]{2,40})"'),
      re.compile(r'\.find\s*\(\s*"([a-z0-9_]{2,40})"')]
per_file_w = collections.defaultdict(set); per_file_r = collections.defaultdict(set)
for f in files:
    try: txt = open(f, encoding='utf-8', errors='replace').read()
    except Exception: continue
    for p in wp:
        for m in p.finditer(txt): write_keys[m.group(1)]+=1; per_file_w[f].add(m.group(1))
    for p in rp:
        for m in p.finditer(txt): read_keys[m.group(1)]+=1; per_file_r[f].add(m.group(1))
allread = set(read_keys)
dead = [(c,k) for k,c in write_keys.items() if k not in allread]
dead.sort(reverse=True)
print('=== JSON keys WRITTEN anywhere in corpus but NEVER read via value/contains/[]/at/find, top 60 ===')
for c,k in dead[:60]:
    wf = sorted(f for f in per_file_w if k in per_file_w[f])
    print(f'{k}  W={c}  in: '+", ".join(wf[:3]))
