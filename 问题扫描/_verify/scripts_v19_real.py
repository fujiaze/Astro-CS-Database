
import json, os, re, subprocess
d=json.load(open('ci/checks.json',encoding='utf-8')); cs=d['checks']
tracked=[t for t in subprocess.check_output(['git','--no-optional-locks','ls-files','-z']).decode('utf-8','replace').split(chr(0)) if t]
print('=== A) fast label: how many fast-members are also linux-main / windows-main ===')
f=[c for c in cs if 'fast' in c['profiles']]
print('  fast members:', len(f), '| also linux-main:', sum(1 for c in f if 'linux-main' in c['profiles']), '| also windows-main:', sum(1 for c in f if 'windows-main' in c['profiles']), '| fast-only:', sum(1 for c in f if c['profiles']==['fast']))
print()
print('=== B) real-data-area (GaiaDR3|GaiaDR3SP|BASS DR3) references in cmake/test registration surface ===')
hits={}
for t in tracked:
    if not (t.endswith('CMakeLists.txt') or t.endswith('.cmake')): continue
    src=open(t,encoding='utf-8',errors='replace').read()
    for m in re.finditer(r'(GaiaDR3SP|GaiaDR3|BASS DR3|DR3SP|dr3sp)', src):
        hits.setdefault(t,set()).add(m.group(1))
for k,v in sorted(hits.items()): print('   %-60s %s' % (k, sorted(v)))
print()
print('=== C) add_test names in gate4_dr3sp_gaiaxpy + coverage by checks/baseline ===')
base=open('ci/ctest_baseline.json',encoding='utf-8').read()
declared=set()
for c in cs:
    for t in c.get('ctest_targets') or []: declared.add(t)
gate_files=[t for t in tracked if 'gate4_dr3sp_gaiaxpy' in t]
print('  tracked files under gate4_dr3sp_gaiaxpy:', len(gate_files))
for t in gate_files: print('     ', t, '(tracked)' if t in set(tracked) else '(UNTRACKED)')
names=set()
for t in gate_files:
    if t.endswith('CMakeLists.txt'):
        src=open(t,encoding='utf-8',errors='replace').read()
        for m in re.finditer(r'add_test\s*\(\s*(?:NAME\s+)?([A-Za-z0-9_.\-]+)', src): names.add(m.group(1))
        print('  cmake in that dir declares add_test:', sorted(names) or 'NONE', '| ASTROCS/pytest add_test lines:', len(re.findall(r'add_test', src)))
print('  add_test names found:', sorted(names) or 'NONE')
for n in sorted(names):
    print('     %-28s in_checks_ctest_targets=%s in_ctest_baseline=%s' % (n, n in declared, n in base))
print()
print('=== D) the 4 zero-registered oracle scripts: current registration state (L1/L2/L3) ===')
import glob as G
cands=set()
for t in tracked:
    if re.search(r'(gate[0-9]_.*|.*_oracle\.py|check_standards_registry\.py|docs_machine_consistency\.py)$', t) and t.endswith('.py'): cands.add(t)
regcmds=' '.join(' '.join(c['command']) for c in cs)
inbase=base
for t in sorted(cands):
    b=os.path.basename(t)
    direct = t in regcmds
    print('  %-70s direct_in_command=%-6s basename_in_registry=%-6s in_ctest_baseline=%s' % (t, direct, b in regcmds, b in inbase or t in inbase))
