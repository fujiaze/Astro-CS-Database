
import re, os
FILES = ['lib/calibration/src/module_entry.cpp','lib/cosmetic/src/module_entry.cpp','lib/drizzle/src/module_entry.cpp','lib/hips/src/module_entry.cpp','lib/gaia_xpsd_client/src/module_entry.c','lib/snr_estimator/src/module_entry.cpp']
NAMES = {'self','err','what','host','c','out_json','input_manifest_json','node_id','manifest'}
for fp in FILES:
    lines = open(fp, encoding='utf-8', errors='ignore').read().split('\n')
    # find function definitions: line starting at col0 with name( and containing acs_status/void/static
    for i, ln in enumerate(lines):
        m = re.match(r'^(?:static\s+)?(?:acs_status|void|int|uint64_t|acs_str_v1|size_t)\s+(\w+)\s*\(', ln)
        if not m: continue
        fn = m.group(1)
        # collect signature until ')' at end of line
        sig = ln
        j = i
        while sig.count('(') > sig.count(')') and j + 1 < len(lines):
            j += 1
            sig += ' ' + lines[j].strip()
        params = re.findall(r'(\w+)\s*(?:,|$)', sig[sig.find('(')+1:sig.rfind(')')])
        params = [p for p in params if p and not p.isdigit()]
        # body window: until next col0 definition
        k = j + 1
        while k < len(lines) and not re.match(r'^(?:static\s+)?(?:acs_status|void|int|uint64_t|acs_str_v1|size_t)\s+\w+\s*\(', lines[k]):
            k += 1
        body = '\n'.join(lines[j+1:k])
        bad = []
        for p in params:
            if p in ('void',): continue
            used = re.search(r'(?<![A-Za-z0-9_.])' + re.escape(p) + r'(?![A-Za-z0-9_])', body)
            if used: continue
            vc = re.search(r'\(void\)\s*' + re.escape(p) + r'\s*;', body)
            bad.append(p + ('/void-cast' if vc else '/UNREF'))
        # also flag params used ONLY by void-cast
        onlyvoid = []
        for p in params:
            if p == 'void': continue
            body_no_v = re.sub(r'\(void\)\s*' + re.escape(p) + r'\s*;', '', body)
            if not re.search(r'\(void\)\s*' + re.escape(p) + r'\s*;', body): continue
            if re.search(r'(?<![A-Za-z0-9_.])' + re.escape(p) + r'(?![A-Za-z0-9_])', body_no_v): continue
            onlyvoid.append(p)
        if bad:
            print('%s:%d %s(%s)' % (fp, i + 1, fn, sig[sig.find('(')+1:sig.rfind(')')][:120]))
            print('    ->', ', '.join(bad))
