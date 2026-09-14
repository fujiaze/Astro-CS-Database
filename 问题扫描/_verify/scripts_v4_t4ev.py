import json, os, glob, csv
d="/workspace/Astro CS Database/run/release-rescue/real-chain-fix/cli/F78_t4_green"
for f in sorted(os.listdir(d)):
    if f.endswith('.json') and f!='alloc_report.json':
        p=os.path.join(d,f)
        try: j=json.load(open(p,encoding='utf-8'))
        except Exception as e: print(f,"ERR",e); continue
        print("###",f, "keys:", list(j)[:12])
        s=json.dumps(j,ensure_ascii=False)
        if 'resource' in s or 'gate' in s or 'cpu' in s:
            # print interesting substrings
            for k in ('cpu_pct_mean','cpu_pct_p50','workers_p50','granted_workers','verdict','cpu_mean_percent','cpu_p50_percent','avg_equivalent_cores','selected_workers','available_cpus','normalized_cpu_100pct_all_allocated_cores','n_samples','wall_seconds'):
                if '"'+k+'"' in s:
                    print("   contains key:",k)
rs=os.path.join(d,"resource_summary.json")
print("resource_summary exists:", os.path.isfile(rs))
for p in glob.glob("/workspace/Astro CS Database/run/release-rescue/**/resource_summary.json", recursive=True)[:6]:
    j=json.load(open(p,encoding='utf-8'))
    print("---",p)
    print(json.dumps(j,ensure_ascii=False)[:700])
