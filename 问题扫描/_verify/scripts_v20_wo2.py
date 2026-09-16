
import re
def scan(fp, struct_name):
    src = open(fp, encoding='utf-8', errors='ignore').read()
    lines = src.split('\n')
    i = next(k for k, l in enumerate(lines) if re.search(r'(?:typedef\s+)?struct', l) ) if False else None
    # find struct block
    m = re.search(r'struct\s+' + struct_name + r'\s*\{([^}]*)\}', src, re.S)
    if not m:
        m = re.search(r'typedef struct \{([^}]*)\}\s*' + struct_name, src, re.S)
    body = m.group(1)
    fields = re.findall(r'([A-Za-z_]\w*)\s*(?:\[[^\]]*\])?\s*;', body)
    fields = [f for f in fields if f not in ('const',)]
    print('== %s :: %s (%d fields)' % (fp, struct_name, len(fields)))
    for f in fields:
        pats = re.compile(r'(?:->|\.)\s*' + f + r'\b')
        sites = [k + 1 for k, l in enumerate(lines) if pats.search(l)]
        wr = [k + 1 for k, l in enumerate(lines) if re.search(r'(?:->|\.)\s*' + f + r'\b\s*=', l) and '==' not in l]
        rd = [s for s in sites if s not in wr]
        if not rd:
            print('   WRITE-ONLY %-22s writes=%s reads=%s' % (f, wr, rd))
    print()

scan('lib/hips/src/module_entry.cpp', 'hips_cfg')
scan('lib/drizzle/src/module_entry.cpp', 'drz_cfg')
scan('lib/gaia_xpsd_client/src/module_entry.c', 'gaia_cfg')
scan('lib/snr_estimator/src/module_entry.cpp', 'noise_cfg')
