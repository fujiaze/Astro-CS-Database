import json, os, re, pathlib, csv, glob
repo=pathlib.Path('.')
R={}
# CON-COMMENTS
find=[]
for p in list(repo.glob('lib/**/*.cpp'))+list(repo.glob('lib/**/*.h'))+list(repo.glob('lib/**/*.hpp')):
    s=str(p).replace(os.sep,'/')
    if '/archive/' in s or '/third_party/' in s or '/superseded/' in s or 'history' in p.name: continue
    try: txt=p.read_text(encoding='utf-8',errors='ignore')
    except Exception: continue
    for ln in txt.splitlines():
        t=ln.strip()
        if t.startswith('//') or t.startswith('*') or t.startswith('/*'):
            if ('V19R2' in t or 'V19R3' in t) and '冻结' not in t:
                find.append(('CON-HIST-NOFREEZE', s))
for p in list(repo.glob('lib/**/*.cpp'))+list(repo.glob('lib/**/*.h')):
    s=str(p).replace(os.sep,'/')
    if '/archive/' in s or '/third_party/' in s: continue
    txt=p.read_text(encoding='utf-8',errors='ignore')
    if 'false_negative' in txt or '不变量' in txt:
        if not re.search(r'SCI-|ALG-|TRACEABILITY', txt): find.append(('CON-INVARIANT-NOID', s))
R['CON-COMMENTS']=('FAIL' if find else 'PASS', find[:8], len(find))
# CON-FORBIDDEN-PATTERNS
f2=[]
for pat,i in [(r'num_threads\s*\(\s*16\s*\)','FORBID-HARDCODE-THREADS'),(r'set_num_threads\s*\(\s*16\s*\)','FORBID-HARDCODE-THREADS')]:
    rx=re.compile(pat)
    for p in list(repo.glob('lib/**/*.cpp'))+list(repo.glob('lib/**/*.h')):
        s=str(p).replace(os.sep,'/')
        if '/tests/' in s or '/test/' in s or '/evidence/' in s or 'third_party' in s or 'archive' in s: continue
        if rx.search(p.read_text(encoding='utf-8',errors='ignore')): f2.append((i,s))
det=[]
for p in repo.glob('lib/**/*.cpp'):
    s=str(p).replace(os.sep,'/')
    if 'third_party' in s or '/tests/' in s or '/test/' in s: continue
    if '.detach()' in p.read_text(encoding='utf-8',errors='ignore'): det.append(('FORBID-DETACH',s))
R['CON-FORBIDDEN-PATTERNS']=('FAIL' if f2 or det else 'PASS', (f2+det)[:8], len(f2)+len(det))
# CON-BUILD-GRAPH
cm_re=re.compile(r'add_library\s*\(\s*([A-Za-z0-9_\-]+)')
cmake_targets=set()
for root,dirs,files in os.walk('.'):
    rp=root.replace(os.sep,'/')
    if any(rp.startswith(x) for x in ['./build','./run','./.git','./worktrees','./artifacts','./third_party']): dirs[:]=[]; continue
    for f in files:
        if f=='CMakeLists.txt' or f.endswith('.cmake'):
            for m in cm_re.finditer(open(os.path.join(root,f),encoding='utf-8',errors='ignore').read()):
                cmake_targets.add(m.group(1))
lib_names={p.name for p in repo.glob('lib/*') if p.is_dir()}
lib_names |= {'lib/'+p.name for p in repo.glob('lib/*/*') if p.is_dir() and (p/'CMakeLists.txt').exists()}
rows=list(csv.DictReader(open('docs/architecture/PRODUCTION_EXECUTION_INVENTORY.csv',encoding='utf-8')))
def has_cmake(n): return (repo/'lib'/n/'CMakeLists.txt').exists() or (repo/'lib'/n).exists() or (repo/n).exists()
missing=[(r.get('requirement_id'),r.get('module_id'),t) for r in rows for t in re.split(r'[,;]',r.get('targets','') or '') if t.strip() and not (repo/'lib'/t.strip()/'CMakeLists.txt').exists()]
mod_ids={ (r.get('module_id') or '').strip() for r in rows } - {''}
R['CON-BUILD-GRAPH']=('FAIL' if (missing or len(mod_ids)<8 or len(cmake_targets)<60) else 'PASS', missing[:6], (len(rows),len(mod_ids),len(cmake_targets)))
# CON-CONFIG-CONTRACTS
pt=open('lib/phase2/src/stage2_common.cpp',encoding='utf-8',errors='ignore').read()
dt=open('docs/development/CONFIG_SCHEMA.md',encoding='utf-8',errors='ignore').read() if os.path.exists('docs/development/CONFIG_SCHEMA.md') else ''
f3=[]
if 'weight_mode = 2' not in pt: f3.append('CFG-PARSER-DEFAULT')
if 'acr_route", std::string("auto")' not in pt: f3.append('CFG-PARSER-ACR')
for e in ['auto','ivar','equal','support_x_snr2']:
    if e not in pt: f3.append('CFG-ENUM-MISSING:'+e)
if 'weight_mode' not in dt: f3.append('CFG-DOC-WEIGHT')
for e in ['weight_mode 只支持','acr_route 只支持']:
    if e not in pt: f3.append('CFG-ERROR-MSG:'+e)
for cfg in repo.glob('lib/phase2/configs/*.json'):
    try:
        j=json.loads(cfg.read_text(encoding='utf-8'))
        if 'inputs' not in j or 'integration' not in j: f3.append('CFG-EXAMPLE-KEYS:'+str(cfg))
    except Exception as ex: f3.append('CFG-EXAMPLE-JSON:'+str(cfg))
R['CON-CONFIG-CONTRACTS']=('FAIL' if f3 else 'PASS', f3[:8], len(f3))
# CON-SCIENCE-UNITS
sci=list(repo.glob('docs/science/*.md')); f4=[]
for doc in sci:
    t=doc.read_text(encoding='utf-8',errors='ignore')
    if not any(k in t for k in ['物理量和单位','变量/单位','ADU','variance','方差']): f4.append('SCI-UNITS-MISSING:'+doc.name)
arows=list(csv.DictReader(open('docs/contracts/API_CONTRACTS.csv',encoding='utf-8')))
if any(not (r.get('units') or '').strip() for r in arows): f4.append('API-UNITS-EMPTY')
allt=' '.join(d.read_text(encoding='utf-8',errors='ignore') for d in sci)
uc=sum(allt.count(u) for u in ['ADU','deg','pixel','rad','mag','dex']); pc=sum(allt.count(p) for p in ['FP32','FP64','float','double'])
if uc<20: f4.append('SCI-UNITS-COUNT %d'%uc)
if pc<5: f4.append('SCI-PRECISION %d'%pc)
R['CON-SCIENCE-UNITS']=('FAIL' if f4 else 'PASS', f4[:8], (uc,pc))
for k,v in R.items(): print(k, '=>', v[0], '| detail', v[1], '| stats', v[2])
