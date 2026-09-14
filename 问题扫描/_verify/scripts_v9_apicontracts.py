import json, os, re, subprocess, pathlib, csv, fnmatch, collections
repo=pathlib.Path('.')
def extract(path):
    text=path.read_text(encoding='utf-8',errors='ignore')
    text=re.sub(r'/\*.*?\*/','',text,flags=re.S); text=re.sub(r'//.*','',text)
    out=[]
    for m in re.finditer(r'^\s*(?:(P2_API|AC_API|CC_EXPORT|DPSF_EXPORT|SNR_API)\s+)?([A-Za-z_][\w\s\*\:\<\>\,\&]*?)\b([A-Za-z_]\w*)\s*\([^;{]*\)\s*;\s*$', text, re.M):
        prefix=(m.group(1) or '').strip(); ret=m.group(2).strip(); name=m.group(3).strip()
        if name in {'if','for','while','switch','return'}: continue
        if len(name)<4 and not name.startswith(('aio_','ac_','cc_','dpsf_','snr_','p2_','sdet_','pc_','ipv_','gaia_','healpix_','aio','astro')): continue
        sig=re.sub(r'\s+',' ',m.group(0).strip())
        out.append({'symbol':name,'signature':sig,'header':str(path),'export':prefix or None})
    return out
PATTERNS=['lib/*/include/**/*.h','lib/*/include/*.h','lib/*/cpp/include/**/*.h','lib/*/*/include/**/*.h','lib/plate_solve/cpp/ipv/include/*.h']
def collect(files):
    seen=set(); uniq=[]
    for h in files:
        s=str(h)
        if s in seen: continue
        seen.add(s)
        if 'third_party' in s or 'runtime_internal.h' in s: continue
        uniq.append(h)
    rows=[]
    for h in uniq:
        try:
            for r in extract(h):
                r['header']=str(r['header']).replace('./','')
                rows.append(r)
        except Exception: pass
    ded={}
    for r in rows: ded.setdefault((r['symbol'],r['header']),r)
    return uniq, sorted(ded.values(), key=lambda x:(x['header'],x['symbol']))
# A: working tree via rglob (like the script, includes run/ and build/)
filesA=[]
for p in PATTERNS: filesA += sorted(repo.rglob(p))
uniqA, rowsA = collect(filesA)
# B: tracked only (clean checkout)
tracked=subprocess.run(['git','-c','core.quotepath=off','--no-optional-locks','ls-files'],capture_output=True,text=True).stdout.splitlines()
filesB=[]
for t in tracked:
    for p in PATTERNS:
        if fnmatch.fnmatch(t,p): filesB.append(pathlib.Path(t)); break
uniqB, rowsB = collect(filesB)
shadow=[str(h) for h in uniqA if not str(h).startswith(('lib/','include/','cli/','runtime/','modules/','providers/'))]
print('A(working tree) headers=%d symbols=%d' % (len(uniqA), len(rowsA)))
print('B(tracked-only)  headers=%d symbols=%d' % (len(uniqB), len(rowsB)))
print('shadow-tree headers counted in A:', len(shadow))
for s in shadow[:12]: print('    ', s)
onlyA = sorted({r['symbol'] for r in rowsA} - {r['symbol'] for r in rowsB})
print()
print('symbols ONLY in working-tree(A) i.e. shadow-supplied:', len(onlyA))
print(onlyA[:40])
# CON-API-CONTRACTS verdict on both
def norm(s): return re.sub(r'\s+',' ',(s or '').strip())
rows=list(csv.DictReader(open('docs/contracts/API_CONTRACTS.csv',encoding='utf-8')))
def verdict(rowsA):
    syms=collections.defaultdict(list)
    for r in rowsA: syms[r['symbol']].append(r)
    misses=[]
    for i,r in enumerate(rows,2):
        sym=r['symbol'].strip(); sig=r['full_signature'].strip(); hdr=r['header'].strip()
        if sym not in syms:
            base=sym.split('::')[-1]
            if base not in syms and sym not in [k.split('::')[-1] for k in syms]:
                misses.append((i,sym,hdr,'API-MISSING-AST')); continue
            sy_=syms[base][0]['signature'] if base in syms else None
        else:
            same=[x for x in syms[sym] if x['header']==hdr]
            if same: sy_=same[0]['signature']
            elif any(norm(sig)==norm(x['signature']) for x in syms[sym]): sy_=next(x['signature'] for x in syms[sym] if norm(sig)==norm(x['signature']))
            else: sy_=syms[sym][0]['signature']
        if norm(sig)!=norm(sy_): misses.append((i,sym,hdr,'API-SIG-MISMATCH'))
        if hdr and not os.path.exists(hdr): misses.append((i,sym,hdr,'API-BAD-HEADER'))
    if len(rows)<300: misses.append((0,'count','API-COUNT-LOW'))
    return misses
mA=verdict(rowsA); mB=verdict(rowsB)
print()
print('CON-API-CONTRACTS findings on working tree(A):', len(mA))
for x in mA[:10]: print('   A', x)
print('CON-API-CONTRACTS findings on tracked-only(B):', len(mB))
for x in mB[:15]: print('   B', x)
