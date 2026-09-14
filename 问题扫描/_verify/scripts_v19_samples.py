
import json
d = json.load(open('ci/checks.json', encoding='utf-8'))
cs = d['checks']
# print full record for a sample of each kind
import sys
def show(cid):
    for c in cs:
        if c['id']==cid:
            print(json.dumps(c, ensure_ascii=False, indent=1))
            return
    print('NOT FOUND', cid)
for cid in ['UT-BACKEND','CTEST-LINUX-FULL','LINUX-MAIN-BUILD-TREE','BUILD-GCC-RELEASE','WIN-TEST-UNIT','CON-FULL-INTEGRATION','CTEST-P1WCS-ASTROPY-CROSS','UT-GAIA-ZLIB','DEEP-COV-PY','TOOLCHAIN-VERIFY','WORKFLOW-REGISTRY-BINDING','KNOWN-FAILURES-BASELINE-CHECK']:
    show(cid)
