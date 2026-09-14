
import re, json, os, subprocess
added = json.load(open('问题扫描/_verify/_v8_added.json', encoding='utf-8'))
def has(fp, ln):
    return any(a['f']==fp and a['n']==ln for a in added)
# 1) ipv_select.cpp:56 comment new?
for fp in ['lib/plate_solve/cpp/ipv/src/ipv_select.cpp']:
    for ln in (55,56,57,58):
        print('added?', fp, ln, has(fp,ln))
# 2) find added line contents for that file in 50-60
print([ (a['n'], a['t'][:80]) for a in added if a['f']=='lib/plate_solve/cpp/ipv/src/ipv_select.cpp' and a['n']<70][:10])
