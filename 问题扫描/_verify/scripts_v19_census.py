
import json, os, subprocess, shlex, re
d = json.load(open('ci/checks.json', encoding='utf-8'))
cs = d['checks']
print('=== ORDER/DEP MECHANISM FIELDS ===')
mech = ['requires','prerequisite_checks','depends_on','after','needs','steps','setup','build']
for c in cs:
    hits = {k:c[k] for k in c if any(m in k.lower() for m in ['requir','depend','after','prereq'])}
    if hits and hits != {'requires_monitor':hits.get('requires_monitor')}:
        print(' ', c['id'], hits)
print()
print('=== FULL CENSUS (id | profiles | plat | cmd | outputs | prereq_tools | changed_paths_n) ===')
for i,c in enumerate(cs):
    cmd = ' '.join(c['command'])
    print('%3d %-38s %-22s %-8s | %s' % (i, c['id'], ','.join(c['profiles']), c['platform'], cmd))
    if c.get('outputs'): print('      OUT:', ' '.join(c['outputs']))
    if c.get('prerequisite_tools'): print('      PT:', ' '.join(c['prerequisite_tools']))
