import csv,json
E='/home/lighthouse/astrocs_audit_v2/v3_cp0'
def rows(p):
    with open(E+'/'+p,encoding='utf-8-sig') as f: return list(csv.DictReader(f))
def cnt(p,col):
    d={}
    for r in rows(p):
        k=r.get(col,''); d[k]=d.get(k,0)+1
    return dict(sorted(d.items()))
summary={
 'control_version':'V3',
 'start_sha':'535e73879662346ee1f599d7a9cae96c6c23680d',
 'candidate_sha':'535e73879662346ee1f599d7a9cae96c6c23680d',
 'checkpoint':'CP0',
 'verdict':'AWAITING_EXTERNAL_REVIEW',
 'task_counts':cnt('TASK_LEDGER.csv','status'),
 'finding_counts':cnt('FINDINGS.csv','severity'),
 'test_counts':cnt('TEST_RESULTS.csv','status'),
 'build_counts':cnt('BUILD_RESULTS.csv','status'),
}
json.dump(summary,open(E+'/SUMMARY.json','w'),indent=2)
print('SUMMARY=',json.dumps(summary))
