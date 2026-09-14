import csv, json, re
p="/workspace/Astro CS Database/问题扫描/账本/FIX_LEDGER.csv"
rows=list(csv.DictReader(open(p,newline='',encoding='utf-8')))
KEYS=['内存回收','reclaim','unexplained','残留','分配器','arena','反向钉死','基线','known-failures','0 用例','退出码','资源门','85%','分母','UT-ABI','install','SKIP','OpenMP','ICV','租约','ThreadBudget','单线程']
for k in KEYS:
    hits=[]
    for r in rows:
        blob=json.dumps(r,ensure_ascii=False)
        if k.lower() in blob.lower():
            hits.append((r.get('\ufeffid') or r.get('id'), r.get('fix_state'), r.get('fix_commit')))
    print(f"### {k}: {len(hits)} -> {hits[:12]}")
