import json, os, re, glob, collections
reg=json.load(open('ci/checks.json',encoding='utf-8'))['checks']
print('== VERSION ==', open('VERSION').read().strip())
v=[c for c in reg if c['id']=='VERSION-CONSISTENCY'][0]
print('expected arg:', [a for a in v['command'] if a.startswith('0.')])
print()
print('== UT-* discover: TestCase class counts ==')
for c in reg:
    cmd=list(map(str,c['command']))
    if 'discover' in cmd:
        s=cmd[cmd.index('-s')+1]
        pat=(cmd[cmd.index('-p')+1] if '-p' in cmd else 'test*.py')
        files=glob.glob(os.path.join(s,'**',pat), recursive=True)
        nclass=0; nmethod=0; per={}
        for f in files:
            try: txt=open(f,encoding='utf-8',errors='replace').read()
            except Exception: continue
            a=len(re.findall(r'class\s+\w+\s*\((?:unittest\.)?TestCase\)', txt))
            b=len(re.findall(r'def (test_?\w*)\s*\(', txt))
            per[f]=(a,b); nclass+=a; nmethod+=b
        print('  %-18s files=%-3d TestCase=%-3d test-defs=%-4d %s' % (c['id'], len(files), nclass, nmethod, ('<-- ZERO-TESTCASE' if nclass==0 else '')))
        if nclass==0:
            for f,(a,b) in per.items(): print('        ', f, a, b)
print()
print('== ci/validate_registry.py referenced anywhere? ==')
import subprocess
for f in ['ci/validate_registry.py','ci/bootstrap.py','ci/validate_candidate.py','ci/select_candidate.py','ci/verify_remote_run.py','ci/verify_actions_lock.py','ci/verify_ciqa_report.py','ci/ci_repair_round.py','ci/check_version.py','ci/prepare_linux_fixtures.py']:
    n=0
    for root,dirs,files in os.walk('.'):
        rp=root.replace(os.sep,'/')
        if any(rp.startswith(p) for p in ['./build','./run','./.git','./worktrees','./out','./GaiaDR3','./GaiaDR3SP','./BASS DR3','./AstroCS.wiki']): dirs[:]=[]; continue
        for fn in files:
            if fn.endswith(('.py','.json','.yml','.yaml','.md','.sh','.cmake','.csv')):
                try: t=open(os.path.join(root,fn),encoding='utf-8',errors='ignore').read()
                except Exception: continue
                if os.path.basename(f) in t: n+=1
    print('  %-32s mentions=%d' % (f, n))
