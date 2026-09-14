import json
d=json.load(open('ci/checks.json',encoding='utf-8'))
reg=d['checks']
print('total checks now:', len(reg))
for c in reg:
    cmd=' '.join(map(str,c['command']))
    for s in ('check_isa_leak.py','check_prod_reachability.py','check_pipeline_graph.py','check_log_contract.py'):
        if s in cmd:
            print('%-28s %-8s prof=%s :: %s' % (c['id'], 'waiv' if c['waivable'] else '-', ','.join(c['profiles']), cmd))
print()
import re
src=open('ci/run.py',encoding='utf-8').read()
m=re.search(r'EMPTY_OUTPUT_SILENCE_EXEMPT\s*=\s*frozenset\(\{(.*?)\}\)', src, re.S)
print('EMPTY_OUTPUT_SILENCE_EXEMPT =', m.group(1).replace('\n',' ').strip() if m else 'NOT FOUND')
