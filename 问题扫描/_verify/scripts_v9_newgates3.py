import json
reg=json.load(open('ci/checks.json',encoding='utf-8'))['checks']
NEW=['CTEST-P1SNR-SCIENCE-ALL','CTEST-P1SNR-LINUX-ALL','CTEST-P1DRZ-TASKSET-INVARIANCE','CTEST-P1STAR-ANGLE-GUARD','CTEST-IPV-TRIANGLE-BUDGET']
for c in reg:
    if c['id'] in NEW: print(json.dumps(c, ensure_ascii=False, indent=1))
print()
print('== comparison: an existing per-target gate ==')
for c in reg:
    if c['id']=='CTEST-P1STAR-MAD': print(json.dumps(c, ensure_ascii=False, indent=1))
