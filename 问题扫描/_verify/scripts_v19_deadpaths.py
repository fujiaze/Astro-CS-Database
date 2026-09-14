
import json, os, re, subprocess, glob
d = json.load(open('ci/checks.json', encoding='utf-8')); cs = d['checks']
impact = json.load(open('ci/impact_map.json', encoding='utf-8'))
tracked = {t for t in subprocess.check_output(['git','--no-optional-locks','ls-files','-z']).decode('utf-8','replace').split(chr(0)) if t}

def pat_hits(pat):
    # count tracked files matching a glob-ish pattern (support ** as any suffix)
    if pat.endswith('/**'):
        pre = pat[:-3]
        pre = pre if pre.endswith('/') else pre + '/'
        return sum(1 for t in tracked if t.startswith(pre))
    if '**' in pat:
        rx = re.compile('^' + re.escape(pat).replace(r'\*\*/', '(?:.*/)?').replace(r'\*\*', '.*').replace(r'\*', '[^/]*') + '$')
        return sum(1 for t in tracked if rx.match(t))
    return sum(1 for t in tracked if t == pat or t.startswith(pat + '/'))

print('=== A) checks.changed_paths patterns with ZERO tracked hits ===')
tot_dead = 0
for c in cs:
    dead = [p for p in c['changed_paths'] if pat_hits(p) == 0]
    if dead:
        tot_dead += 1
        print('  %-30s dead=%s' % (c['id'], dead))
print('  checks with >=1 dead pattern:', tot_dead)
print()
print('=== B) impact_map rule paths with ZERO tracked hits ===')
for r in impact['rules']:
    for p in r['paths']:
        h = pat_hits(p)
        if h == 0: print('  DEAD RULE PATH: %-30s (rule checks n=%d)' % (p, len(r['checks'])))
print('  base_checks:', impact.get('base_checks'))
print('  fallback:', impact.get('fallback'))
print()
print('=== C) base_checks reachable? ===')
bcs = impact.get('base_checks') or []
for b in bcs[:60]:
    print('   ', b, 'in checks.json:', b in {c['id'] for c in cs})
