import json, pathlib, datetime as dt, fnmatch, subprocess, re
BASE = json.load(open('ci/known_failures.json',encoding='utf-8'))
cb = json.load(open('ci/ctest_baseline.json',encoding='utf-8'))
reg = json.load(open('ci/checks.json',encoding='utf-8'))['checks']
known=set(t for t in cb.get('targets',[]) if isinstance(t,str) and t)
for c in reg:
    for pat in (c.get('ctest_targets') or []):
        if isinstance(pat,str) and pat and not any(ch in pat for ch in '*?['):
            known.add(pat)
known |= {t for t in list(known) for pat in [p for c in reg for p in (c.get('ctest_targets') or []) if isinstance(p,str)] if fnmatch.fnmatchcase(t,pat)}
print('known ctest set size', len(known))
ALLOWED={'WORKSPACE_HYGIENE','LEGACY_V7_GATED','HOST_ENV_HOSTED','DOC_DRIFT','TOOLING_DRIFT'}
NEVER=set()
m = re.search(r'NEVER_WAIVABLE_CATEGORIES\s*=\s*\{(.*?)\}', open('tools/quality/known_failures_baseline.py',encoding='utf-8').read(), re.S)
print('NEVER block raw:', (m.group(1)[:600] if m else 'n/a'))
now = dt.datetime.now(dt.timezone.utc)
errs=[]
for i,e in enumerate(BASE.get('failures',[])):
    where='failures[%d]'%i
    REQ=('unit','kind','category','owner','reason','first_seen_commit','source_sha','reproducer','expiry','expected','removal_condition','registered_by','activation') if e.get('expected')=='conditional' else ('unit','kind','category','owner','reason','first_seen_commit','source_sha','reproducer','expiry','expected','removal_condition','registered_by')
    miss=[f for f in REQ if f not in e or e[f] in (None,'',[])]
    if miss: errs.append('%s 缺字段 %s'%(where,miss))
    if str(e.get('check_id'))!=str(e.get('unit')): errs.append('%s check_id!=unit'%where)
    if e.get('category') not in ALLOWED: errs.append('%s category 越界 %r'%(where,e.get('category')))
    if not re.match(r'^[0-9a-f]{40}$', str(e.get('first_seen_commit',''))): errs.append('%s first_seen 非40hex'%where)
    if not re.match(r'^[0-9a-f]{40}$', str(e.get('source_sha',''))): errs.append('%s source_sha 非40hex'%where)
    try:
        if dt.datetime.fromisoformat(str(e['expiry']).replace('Z','+00:00')) < now: errs.append('%s 过期'%where)
    except Exception as ex: errs.append('%s expiry 非法 %s'%(where,ex))
    if e.get('kind')=='check' and e.get('unit') not in {c['id'] for c in reg}: errs.append('%s 非登记检查 id'%where)
    if e.get('kind')=='ctest' and e.get('unit') not in known: errs.append('%s (%s) 不在已知 CTest 目标集 => V3 FAIL'%(where,e.get('unit')))
    for f in ('first_seen_commit','source_sha'):
        v=str(e.get(f,''))
        if re.match(r'^[0-9a-f]{40}$',v):
            rc=subprocess.run(['git','--no-optional-locks','cat-file','-e',v+'^{commit}'],capture_output=True).returncode
            if rc!=0: errs.append('%s %s 不存在于仓库历史: %s'%(where,f,v)); continue
            rc2=subprocess.run(['git','--no-optional-locks','merge-base','--is-ancestor',v,'HEAD'],capture_output=True).returncode
            if rc2!=0: errs.append('%s %s 不是 HEAD 祖先: %s'%(where,f,v))
print()
print('VERIFY static errors:', len(errs))
for x in errs: print('   ', x)
print('=> KNOWN-FAILURES-BASELINE-VERIFY verdict:', 'FAIL' if errs else 'PASS')
