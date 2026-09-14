import json,os,re
reg=json.load(open('ci/checks.json',encoding='utf-8'))['checks']
for s in ['check_isa_leak.py','check_prod_reachability.py','check_serial_heavy.py','check_pipeline_graph.py','check_log_contract.py','check_complexity.py','check_testkit.py']:
    users=[(c['id'], ' '.join(map(str,c['command']))) for c in reg if s in ' '.join(map(str,c['command']))]
    print(s, '->', users)
