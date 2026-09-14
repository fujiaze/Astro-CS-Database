
import json, os, re, subprocess, collections
d=json.load(open('ci/checks.json',encoding='utf-8')); cs=d['checks']
tracked={t for t in subprocess.check_output(['git','--no-optional-locks','ls-files','-z']).decode('utf-8','replace').split(chr(0)) if t}

# --- 1) script existence (all tokens) ---
def script_tokens(c):
    out=[]
    for tok in c['command']:
        if tok.endswith('.sh') or tok.endswith('.py'): out.append(tok)
    return out
# --- 2) build-artifact consumption ---
BUILD_TOK=re.compile(r'(^|/)(build|run/ci|run/temp)(/|$)|/astrocs(\b|$)|\.(so|dll|a|junit\.xml)\b')
# --- 3) host tool invocation from AST scans (hard-coded from earlier measured sets) ---
TOOLS_BY_GATE = {
 'UT-BACKEND':['g++','gcc','cmake','objdump','taskset','numpy','astropy'],
 'UT-CLI':['cmake','g++','gcc','nm'],
 'UT-API':['g++','numpy'],
 'UT-RUNTIME':['g++'],
 'UT-IO':['gcc','numpy'],
 'UT-QUALITY':['tar','cmake','g++'],
 'UT-ABI':['cmake','g++','nm'],
 'UT-CPU-BASELINE':['g++','gcc'],'UT-CPU-DISPATCH':['g++','gcc'],
 'UT-CPU-AVX2':['g++','gcc'],'UT-CPU-AVX512':['g++','gcc'],
 'ACR-DORMANT':['nm'],'DUPLICATION':['nm'],'PROD-REACH-SELFTEST':['nm'],
 'ISA-LEAK-SELFTEST':['objdump'],'WARNING-SUPPRESSION':['bash'],'TESTKIT-LIST':['bash'],
 'CI-BINDING-TESTS':['bash','yaml'],'WORKFLOW-REGISTRY-BINDING':['yaml'],
 'UT-MONITORING':['procfs'],'UT-ARCH':['g++'],
 'CTEST-P1DRZ-TASKSET-INVARIANCE':['taskset'],
}
# --- 4) prior-gate artifact consumption ---
PRODUCERS = {'BUILD-GCC-RELEASE':'run/ci/build-gcc-release','CTEST-LINUX-FULL':'run/ci/build-gcc-release/ctest-full.junit.xml',
 'LINUX-MAIN-BUILD-TREE':'build/**','LINUX-MAIN-FIXTURES':'run/temp/**','WIN-BUILD-RELEASE':'build/win-msvc-*/**'}
# --- 5) gitignored reads / outputs
def ign(p):
    return subprocess.run(['git','--no-optional-locks','check-ignore','-q','--stdin'],input=p.encode(),capture_output=True).returncode==0
SKIPQ={'UT-CLI':(71,174),'UT-API':(16,48),'UT-BACKEND':(44,209),'UT-RUNTIME':(9,70),'UT-QUALITY':(6,63),'UT-ARCH':(3,36),'UT-MONITORING':(3,74)}
DEAD={'PRODUCTION-GRAPH':'graph/**','ACR-DORMANT':'legacy/**','GLOSSARY-DOCS':'docs/glossary/**','LOG-CONTRACT-SELFCHECK':'schemas/**','TRACEABILITY-MATRIX':'schemas/traceability_matrix.schema.json','WORKSPACE-ADOPTION':'.git/HEAD','RECONCILE-STATE':'engineering/control/CONTROL_TASK_LEDGER.csv'}
declared={c['id']:c.get('prerequisite_tools') or [] for c in cs}
rows=[]; col=collections.Counter()
for i,c in enumerate(cs):
    cid=c['id']; j=' '.join(c['command'])
    a1 = '缺失' if any(t not in tracked for t in script_tokens(c)) else ('死触发面' if cid in DEAD else 'OK')
    build = bool(re.search(r'build/|run/ci/|\.so\b|\.dll\b|\.a\b|junit\.xml|libastrocs|astro_image_io|libphase2', j))
    a2 = ('消费:'+('run/ci' if 'run/ci' in j else 'build/')) if build else '-'
    need=TOOLS_BY_GATE.get(cid,[])
    miss=[t for t in need if t not in ' '.join(declared[cid])]
    a3 = ('未声明:'+','.join(miss)) if miss else ('已声明' if declared[cid] else '-')
    a4 = '前序产物' if re.search(r'run/ci/build-gcc-release', j) and cid not in PRODUCERS else '-'
    outg=[o for o in c['outputs'] if ign(o)]
    a5 = ('outputs-gitignored:'+','.join(outg[:2])) if outg else '-'
    for k,v in (('a1',a1),('a2',a2),('a3',a3),('a4',a4),('a5',a5)):
        if v!='-': col[k+':'+v.split(':')[0]]+=1
    if a1!='OK' or a2!='-' or a3!='-' or a4!='-' or a5!='-':
        rows.append((i,cid,','.join(c['profiles']),a1,a2,a3,a4,a5,('%d/%d'%(SKIPQ[cid][0],SKIPQ[cid][1]) if cid in SKIPQ else '')))
print('risk rows:', len(rows), 'of', len(cs))
lines=['| # | 检查项 | profiles | ①脚本/触发面 | ②构建产物 | ③宿主工具/模块 | ④前序产物 | ⑤gitignored | 守卫用例 |','|--|--|--|--|--|--|--|--|--|']
for r in rows:
    lines.append('| %d | %s | %s | %s | %s | %s | %s | %s | %s |' % r)
open('问题扫描/_verify/V19_matrix_rows.md','w',encoding='utf-8').write('\n'.join(lines)+'\n')
print('\n'.join(lines[:34]))
print()
print('=== column counters ==='); print(dict(col))
