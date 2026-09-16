
import re
def field_sites(fp, f):
    lines=open(fp,encoding='utf-8',errors='ignore').read().split('\n')
    pat=re.compile(r'(?:->|\.)\s*'+f+r'\b')
    out=[]
    for i,l in enumerate(lines,1):
        if pat.search(l):
            kind='WRITE' if re.search(r'(?:->|\.)\s*'+f+r'\b\s*=[^=]', l) else ('DECL' if re.match(r'^\s*[A-Za-z_].*\b'+f+r'\s*[;\[,]', l) else 'READ')
            out.append((i,kind,l.strip()[:95]))
    return out
for fp,f in [('lib/drizzle/src/module_entry.cpp','n_bytes'),
             ('lib/hips/src/module_entry.cpp','max_workers'),
             ('lib/snr_estimator/src/module_entry.cpp','variance_floor'),
             ('lib/snr_estimator/src/module_entry.cpp','cfg_present'),
             ('lib/snr_estimator/src/module_entry.cpp','max_workers'),
             ('lib/drizzle/src/module_entry.cpp','have_scale')]:
    s=field_sites(fp,f)
    print('== %s :: %s  (%d sites: %s)' % (fp,f,len(s),{k:sum(1 for _,kk,_ in s if kk==k) for k in ('WRITE','READ','DECL')}))
    for i,k,l in s[:6]: print('   %5d %-5s %s' % (i,k,l))
    print()
