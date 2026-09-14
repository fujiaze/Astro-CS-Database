
import json, subprocess
d=json.load(open('ci/checks.json',encoding='utf-8')); cs=d['checks']
by={c['id']:c for c in cs}
print('=== A) ctest_targets declaration vs command --target / --regex ===')
import re
for c in cs:
    cmd=' '.join(c['command'])
    m=re.search(r'--target (\S+)', cmd)
    tg=t=m.group(1) if m else None
    decl=c.get('ctest_targets')
    if tg or decl:
        kind='REGEX-ish' if tg and ('.*' in tg or '*' in tg or '?' in tg) else ''
        same = (decl==[tg]) if decl and tg else (decl is None)
        print('  %-36s cmd_target=%-32s decl=%-34s %s %s' % (c['id'], tg, decl, kind, '' if same else '<-> MISMATCH'))
print()
print('=== B) checks with ctest_targets but no --target in command, or vice versa ===')
a=[c['id'] for c in cs if c.get('ctest_targets') and '--target' not in ' '.join(c['command'])]
b=[c['id'] for c in cs if '--target' in ' '.join(c['command']) and not c.get('ctest_targets')]
print('  decl-without-cmd-target:', a)
print('  cmd-target-without-decl:', b)
print()
print('=== C) workflow_binding.json steps: fail_closed / require_outputs / serves_checks ===')
wb=json.load(open('ci/workflow_binding.json',encoding='utf-8'))
for s in wb.get('steps',[]):
    print('  %-28s role=%-22s binds=%-26s serves=%s' % (s.get('step_id'), s.get('role'), s.get('binds_check'), s.get('serves_checks')))
    if s.get('require_outputs'): print('       require_outputs=%s fail_closed=%s exec=%s' % (s['require_outputs'], s.get('fail_closed'), s.get('exec')))
