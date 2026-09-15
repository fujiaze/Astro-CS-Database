
import re, collections, subprocess, json
# gather manifest keys written
txt = open('lib/core/src/module_adapters.cpp', encoding='utf-8', errors='replace').read()
keys = collections.Counter(re.findall(r'\(\*man\)\["([^"]+)"\]', txt))
# also nested stages keys
print('distinct manifest keys:', len(keys), 'total writes:', sum(keys.values()))
# read side: search whole corpus for each key as a string literal
corpus_files=[l.strip() for l in open('问题扫描/_cache/_v21_corpus.txt', encoding='utf-8') if l.strip()]
corpus={}
for f in corpus_files:
    try: corpus[f]=open(f,encoding='utf-8',errors='replace').read()
    except Exception: pass
dead=[]
for k in sorted(keys):
    pat=re.compile(re.escape(k))
    hits=[]
    for f,t in corpus.items():
        if f.endswith('module_adapters.cpp'): continue
        n=len(pat.findall(t))
        if n: hits.append((f,n))
    if not hits: dead.append(k)
    print(f'{k:38s} W={keys[k]:3d} readhits={sum(n for _,n in hits):4d} files={len(hits)}')
print()
print('=== keys with ZERO occurrence outside module_adapters.cpp ===', dead)
