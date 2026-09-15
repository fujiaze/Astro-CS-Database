
import csv
rows=list(csv.DictReader(open('问题扫描/账本/FIX_LEDGER.csv',encoding='utf-8-sig')))
keys=['TROUBLESHOOTING','category','detail_code','LOGGING_DIAGNOSTICS','troubleshooting','error_domain_name','status_codes','ACS_ERR_BUDGET','DIAGNOSTICS_STANDARD','jsonl','severity']
for k in keys:
    ids=[]
    for r in rows:
        blob=' '.join(str(r.get(x,'')) for x in ('title','position','evidence','impact','clause','related'))
        if k.lower() in blob.lower(): ids.append(r['id']+':'+r['priority'])
    print('%-22s => %d %s' % (k, len(ids), ids[:10]))

