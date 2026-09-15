
import json, collections
d = json.load(open('ci/checks.json', encoding='utf-8'))
checks = d['checks'] if isinstance(d, dict) and 'checks' in d else d
print('type', type(d).__name__, 'n', len(checks))
c = collections.Counter()
for ch in checks:
    for k,v in ch.items():
        if k in ('heavy','requires_monitor','mutates_workspace','waivable'):
            if v: c[k+'_true']+=1
            else: c[k+'_false']+=1
    if ch.get('outputs'): c['outputs_nonempty']+=1
    if ch.get('changed_paths'): c['changed_paths_nonempty']+=1
    if ch.get('prerequisite_tools'): c['prereq_nonempty']+=1
    if ch.get('ctest_targets'): c['ctest_targets_nonempty']+=1
    if ch.get('dirty_ignore_prefixes'): c['dip']+=1
    if ch.get('dirty_ignore_exact'): c['die']+=1
print(dict(c))
ids=[ch['id'] for ch in checks]
print('n checks', len(ids), 'dup', [k for k,v in collections.Counter(ids).items() if v>1])
rm=[ch['id'] for ch in checks if ch.get('requires_monitor')]
print('requires_monitor=true:', rm)
hv=[ch['id'] for ch in checks if ch.get('heavy')]
print('heavy=true count:', len(hv))
import re
gr=sum(1 for ch in checks if '--gate-required' in ' '.join(ch.get('command',[]) if isinstance(ch.get('command'),list) else [str(ch.get('command'))]))
print('commands containing --gate-required:', gr)
