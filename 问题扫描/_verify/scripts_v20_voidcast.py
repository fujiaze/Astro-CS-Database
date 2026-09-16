
import re
FILES = ['lib/calibration/src/module_entry.cpp','lib/cosmetic/src/module_entry.cpp','lib/drizzle/src/module_entry.cpp','lib/hips/src/module_entry.cpp','lib/gaia_xpsd_client/src/module_entry.c','lib/snr_estimator/src/module_entry.cpp','lib/core/src/module_adapters.cpp','cli/commands.cpp','cli/parser.cpp','cli/runtime_client.cpp']
tot = {}
for fp in FILES:
    t = open(fp, encoding='utf-8', errors='ignore').read()
    vs = re.findall(r'\(void\)\s*([A-Za-z_]\w*)\s*;', t)
    from collections import Counter
    c = Counter(vs)
    print('%-46s void-cast total=%d' % (fp, sum(c.values())))
    print('    ', ', '.join('%s:%d' % (k, v) for k, v in c.most_common(10)))
    tot[fp] = sum(c.values())
print('SUM', sum(tot.values()))
