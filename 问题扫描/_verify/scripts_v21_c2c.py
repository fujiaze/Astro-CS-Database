
import re, collections
files=[l.strip() for l in open('问题扫描/_cache/_v21_src.txt',encoding='utf-8') if l.strip()]
# ProcSample field-level W/R census
fields=['cpu_seconds','user_seconds','sys_seconds','rss_bytes','pss_bytes','vms_bytes','threads','ctx_switches','read_bytes','write_bytes','read_ops','write_ops','sys_mem_avail','sys_swap_free','page_faults','d_cpu_seconds','d_rss_bytes','d_read_bytes','d_write_bytes','d_ctx_switches']
for fld in fields:
    w=[];rd=[]
    for f in files:
        try: lines=open(f,encoding='utf-8',errors='replace').read().split('\n')
        except Exception: continue
        for i,L in enumerate(lines,1):
            if re.search(r'\b'+fld+r'\b', L):
                # ignore comment lines
                st=L.strip()
                if st.startswith('//') or st.startswith('*') or st.startswith('/*'): continue
                if re.search(r'(\.|->)\s*'+fld+r'\s*(=|\+=|\+\+)', L) or re.search(r'^\s*\w[\w ]*\b'+fld+r'\s*=\s*[^=]', L):
                    w.append((f,i))
                else:
                    rd.append((f,i))
    prod_w=[x for x in w if not x[0].startswith('tests/')]
    prod_r=[x for x in rd if not x[0].startswith('tests/')]
    print(f'{fld:16s} W={len(w):3d} R={len(rd):3d} | prodW={len(prod_w)} prodR={len(prod_r)}  rdfiles={sorted(set(x[0] for x in rd))[:4]}')
