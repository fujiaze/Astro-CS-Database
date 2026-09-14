import json, collections, subprocess, os, re
d=json.load(open('ci/checks.json',encoding='utf-8'))['checks']
sha=subprocess.run(['git','--no-optional-locks','rev-parse','--short','HEAD'],capture_output=True,text=True).stdout.strip()
print('HEAD', sha)
print('total', len(d), 'unique', len({c['id'] for c in d}))
print('waivable', sum(1 for c in d if c['waivable']))
print('profiles', dict(collections.Counter(p for c in d for p in c['profiles'])))
print('platform', dict(collections.Counter(c['platform'] for c in d)))
print('ctest_targets gates', sum(1 for c in d if c.get('ctest_targets')))
print()
for s in ('validate_registry.py','check_registration_timeouts.py','check_serial_heavy.py','gen_traceability_csv.py','check_comment_hygiene.py','check_prod_reachability.py','check_isa_leak.py','check_pipeline_graph.py','known_failures_baseline.py'):
    hits=[c['id'] for c in d if any(s in str(a) for a in c['command'])]
    carriers=0
    for root,dirs,files in os.walk('.github'):
        for f in files:
            try: carriers += open(os.path.join(root,f),encoding='utf-8',errors='ignore').read().count(s)
            except Exception: pass
    t=0
    for root,dirs,files in os.walk('ci/tests'):
        for f in files: t+=open(os.path.join(root,f),encoding='utf-8',errors='ignore').read().count(s)
    print('%-30s gates=%-3d wf-hits=%-3d ci/tests-hits=%d  %s' % (s, len(hits), carriers, t, hits[:6]))
