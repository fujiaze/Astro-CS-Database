
import re, json, collections
recs = json.load(open('问题扫描/_verify/_v8_added.json', encoding='utf-8')) if False else None
import os
# re-parse full added lines from per-commit diff (all kinds)
lines = open('问题扫描/_verify/_v8_diff_percommit.txt', encoding='utf-8', errors='replace').read().split('\n')
commit=None; cur=None; newln=0; out=[]
for ln in lines:
    if ln.startswith('COMMITSEP '):
        p=ln.split(' ',3); commit=p[1]; cur=None; continue
    if ln.startswith('diff --git'): cur=None; continue
    if ln.startswith('+++ b/'): cur=ln[6:].strip(); continue
    if ln.startswith('--- '): continue
    m=re.match(r'^@@ -\d+(?:,\d+)? \+(\d+)(?:,\d+)? @@', ln)
    if m: newln=int(m.group(1)); continue
    if ln.startswith('+'):
        out.append((commit,cur,newln,ln[1:])); newln+=1
    elif ln.startswith('-') or ln.startswith('\\'): pass
    else: newln+=1
R = re.compile(r'\brun/[A-Za-z0-9_.\-/]+')
hits=[]
for c,f,n,t in out:
    for m in R.finditer(t):
        hits.append((c,f,n,m.group(0)))
print('TOTAL run/ mentions in added lines (scope lib cli include tools docs, HEAD 521095b8):', len(hits))
byfile = collections.Counter((f) for _,f,_,_ in hits)
print('files:', len(byfile))
for f,c in byfile.most_common():
    print('  %3d  %s' % (c,f))
print()
for c,f,n,p in hits:
    print('%s %s:%d -> %s' % (c,f,n,p))
