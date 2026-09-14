import json
reg=json.load(open('ci/checks.json',encoding='utf-8'))['checks']
for cid in ['KNOWN-FAILURES-BASELINE','TRACEABILITY','CON-COMMENTS','CON-FULL-INTEGRATION','WORKFLOW-REGISTRY-BINDING','DOC-LINE-ANCHORS','CON-DOC-SYMBOLS','CON-FORBIDDEN-PATTERNS']:
    c=[x for x in reg if x['id']==cid][0]
    print(cid, '| profiles', c['profiles'], '| waivable', c['waivable'], '| outputs', c['outputs'], '| mutates', c['mutates_workspace'])
print()
cmd=' '.join(' '.join(map(str,c['command'])) for c in reg)
for f in ['validate_registry.py','validate_workflow_binding.py','check_doc_line_anchors.py','gen_traceability_csv.py']:
    print(f, '->', sum(1 for c in reg if f in ' '.join(map(str,c['command']))))
