
import csv, re
rows=list(csv.DictReader(open('问题扫描/账本/FIX_LEDGER.csv',encoding='utf-8-sig')))
keys=['error_code_registry','acs_status_name','status_codes.h','publish.h','aio_abi_v1','secure_loader','module_registry','BSCALE','executor.cpp','worker_advisor','cpu_routing','ipv_entry','drizzle_engine','aio_fits','bare except','except Exception','value_or','create_directories','filesystem::remove','fsync','set_precision_mode','orchestrator.cpp:3094','photometry_report']
for k in keys:
    ids=[]
    for r in rows:
        blob=' '.join(str(r.get(x,'')) for x in ('title','position','evidence','impact','clause','related'))
        if k.lower() in blob.lower(): ids.append(r['id']+':'+r['priority'])
    print(k, '=>', len(ids), ids[:12])

