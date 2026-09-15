
import csv, re, sys, collections
p='问题扫描/账本/FIX_LEDGER.csv'
rows=list(csv.DictReader(open(p,encoding='utf-8-sig')))
print('LEDGER_ROWS', len(rows))
terms = ['空catch','empty catch','catch (...)','吞错','吞掉','静默','silently','silent','降级','warn','退出码','错误码','exit code','fail-fast','重试','retry','幂等','idempot','兜底','fallback','默认值','diagnos','诊断','error_domain','acs_status','errno','strerror','value_or','nodiscard']
hits=collections.defaultdict(list)
for r in rows:
    blob = ' '.join(str(r.get(k,'')) for k in ('title','position','evidence','impact','clause','related'))
    for t in terms:
        if t.lower() in blob.lower():
            hits[t].append((r['id'], r['priority'], r.get('category',''), (r.get('title') or '')[:90]))
for t in terms:
    print('### ', t, len(hits[t]))

