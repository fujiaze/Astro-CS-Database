
import csv
rows=list(csv.DictReader(open('问题扫描/账本/FIX_LEDGER.csv',encoding='utf-8-sig')))
keys=['tasks_executed','executor.cpp:13','worker_task','io_worker_loop','observed_trace','gen_run_graphs','COMPLETED','acs_status_name_v1','status_str','module_adapters','exit_codes','ERROR_MODEL','aio_fits.cpp:16','worker_advisor.cpp:7','cpu_routing.cpp:4','cpu_routing.cpp:5','prepare_linux_fixtures','orchestrator.cpp:3094','photometry_report','drizzle_engine.cpp:114','ipv_entry','stage2.cpp:17','commands.cpp:55','uncertainty_available','build_run_provenance','file_sha256']
for k in keys:
    ids=[]
    for r in rows:
        blob=' '.join(str(r.get(x,'')) for x in ('title','position','evidence','impact','clause','related'))
        if k.lower() in blob.lower(): ids.append(r['id']+':'+r['priority'])
    print('%-26s => %d %s' % (k, len(ids), ids[:10]))

