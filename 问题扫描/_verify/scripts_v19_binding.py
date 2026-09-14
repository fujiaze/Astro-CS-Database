
import json, subprocess
d=json.load(open('ci/checks.json',encoding='utf-8')); cs=d['checks']
by={c['id']:c for c in cs}
wb=json.load(open('ci/workflow_binding.json',encoding='utf-8'))
print('=== workflow_binding check-body steps full ===')
for s in wb['steps']:
    if s.get('role')=='check-body':
        print(json.dumps(s, ensure_ascii=False, indent=1))
print('=== manifest top keys ===', list(wb.keys()))
print()
print('=== TRACEABILITY record ===')
print(json.dumps(by['TRACEABILITY'], ensure_ascii=False, indent=1))
print()
print('=== gitignore status of dirty_ignore / outputs targets ===')
for p in ['artifacts/prerelease_v5/ISA-001/MEASUREMENTS.csv','reports/v19r2/evidence/quality/traceability_check.json','artifacts/KNOWN_FAILURES_BASELINE.json']:
    tr = subprocess.run(['git','--no-optional-locks','ls-files','--error-unmatch',p],capture_output=True).returncode==0
    ig = subprocess.run(['git','--no-optional-locks','check-ignore','-q','--stdin'],input=p.encode(),capture_output=True).returncode==0
    print('  %-62s tracked=%-5s gitignored=%s' % (p,tr,ig))
