
import csv, re, collections
p='问题扫描/账本/FIX_LEDGER.csv'
rows=list(csv.DictReader(open(p,encoding='utf-8-sig')))
terms = {'吞错':['吞错','吞掉'],'兜底':['兜底'],'fail-fast':['fail-fast'],'降级':['降级'],'退出码':['退出码'],'错误码':['错误码'],'重试幂等':['重试','幂等'],'acs_status':['acs_status'],'诊断':['诊断']}
for name,ts in terms.items():
    print('===== '+name+' =====')
    seen=set()
    for r in rows:
        blob = ' '.join(str(r.get(k,'')) for k in ('title','position','evidence','impact','clause','related'))
        if any(t.lower() in blob.lower() for t in ts) and r['id'] not in seen:
            seen.add(r['id'])
            print(' ', r['id'], '|', r['priority'], '|', (r.get('title') or '')[:110].replace('\n',' '))

