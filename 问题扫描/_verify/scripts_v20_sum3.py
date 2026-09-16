
import json, collections
rows = json.load(open('问题扫描/_cache/v20_params3.json'))

def prod(f):
    return not any(x in f for x in ('third_party', '/tests/', 'tests/', '_test'))

agg = collections.Counter(); per = collections.Counter()
for r in rows:
    if not prod(r['path']): continue
    for nm, kind in r['unused']:
        agg[kind] += 1
        per[r['path']] += 1
print('PROD defs listed:', sum(1 for r in rows if prod(r['path'])))
print('PROD param totals:', dict(agg))
print('by file:')
for f, c in per.most_common(16):
    print('  %4d %s' % (c, f))
print()
print('=== unref (NOT void-cast) in production, C-ABI-ish names ===')
PREF = ('astrocs_', 'ac_', 'hp_', 'aio_', 'snr_', 'ipv_', 'p1_', 'p2_', 'p3_', 'gaia_', 'drz_', 'cos_', 'cal_', 'hips_', 'noise_', 'acs_')
n = 0
for r in rows:
    if not prod(r['path']): continue
    un = [nm for nm, k in r['unused'] if k == 'unref']
    if not un: continue
    n += 1
    print('  %s:%d %s(%s) -> UNREF: %s' % (r['path'], r['line'], r['fn'], r['params'][:70], ','.join(un)))
print('defs with真 unref (no void-cast):', n)
