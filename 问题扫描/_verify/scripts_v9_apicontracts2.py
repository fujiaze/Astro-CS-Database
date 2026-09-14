import os, re, fnmatch, subprocess, collections, pathlib, csv, json
PATTERNS=['lib/*/include/**/*.h','lib/*/include/*.h','lib/*/cpp/include/**/*.h','lib/*/*/include/**/*.h','lib/plate_solve/cpp/ipv/include/*.h']
tracked=set(x for x in subprocess.run(['git','-c','core.quotepath=off','--no-optional-locks','ls-files'],capture_output=True,text=True).stdout.splitlines())
cands=[]
for t in tracked:
    if t.endswith('.h') and any(fnmatch.fnmatch(t,p) for p in PATTERNS): cands.append(t)
extra=[p for p in ['lib/snr_estimator/include/astrocs/noise/types.h'] if os.path.exists(p) and p not in tracked]
print('tracked matching headers:', len(cands), ' + untracked matching:', extra)
def extract(path):
    text=open(path,encoding='utf-8',errors='ignore').read()
    text=re.sub(r'/\*.*?\*/','',text,flags=re.S); text=re.sub(r'//.*','',text)
    out=[]
    for m in re.finditer(r'^\s*(?:(P2_API|AC_API|CC_EXPORT|DPSF_EXPORT|SNR_API)\s+)?([A-Za-z_][\w\s\*\:\<\>\,\&]*?)\b([A-Za-z_]\w*)\s*\([^;{]*\)\s*;\s*$', text, re.M):
        name=m.group(3).strip()
        if name in {'if','for','while','switch','return'}: continue
        if len(name)<4 and not name.startswith(('aio_','ac_','cc_','dpsf_','snr_','p2_','sdet_','pc_','ipv_','gaia_','healpix_','aio','astro')): continue
        out.append({'symbol':name,'signature':re.sub(r'\s+',' ',m.group(0).strip()),'header':path})
    return out
def build(files):
    syms=collections.defaultdict(list)
    for f in files:
        try:
            for r in extract(f): syms[r['symbol']].append(r)
        except Exception: pass
    return syms
def norm(s): return re.sub(r'\s+',' ',(s or '').strip())
rows=list(csv.DictReader(open('docs/contracts/API_CONTRACTS.csv',encoding='utf-8')))
def verdict(syms):
    bad=[]
    for i,r in enumerate(rows,2):
        sym=r['symbol'].strip(); sig=r['full_signature'].strip(); hdr=r['header'].strip()
        if sym not in syms:
            base=sym.split('::')[-1]
            if base not in syms and sym not in [k.split('::')[-1] for k in syms]:
                bad.append((i,sym,hdr,'API-MISSING-AST')); continue
            ast=syms[base][0]['signature']
        else:
            same=[x for x in syms[sym] if x['header']==hdr]
            if same: ast=same[0]['signature']
            elif any(norm(sig)==norm(x['signature']) for x in syms[sym]): ast=next(x['signature'] for x in syms[sym] if norm(sig)==norm(x['signature']))
            else: ast=syms[sym][0]['signature']
        if norm(sig)!=norm(ast): bad.append((i,sym,hdr,'API-SIG-MISMATCH csv=%s | hdr=%s' % (norm(sig)[:70], norm(ast)[:70])))
        if hdr and hdr not in tracked: bad.append((i,sym,hdr,'API-BAD-HEADER(untracked)'))
    if len(rows)<300: bad.append((0,'count','','API-COUNT-LOW %d<300'%len(rows)))
    return bad
A=build(cands+extra); B=build(cands)
vA=verdict(A); vB=verdict(B)
print()
print('== verdict with untracked headers present (本机工作树): findings=%d ==' % len(vA))
for x in vA[:12]: print('   ', x)
print()
print('== verdict on CLEAN CHECKOUT (tracked only): findings=%d ==' % len(vB))
for x in vB[:12]: print('   ', x)
