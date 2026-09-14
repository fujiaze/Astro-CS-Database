
import json, subprocess
d=json.load(open('ci/checks.json',encoding='utf-8')); cs=d['checks']
tracked={t for t in subprocess.check_output(['git','--no-optional-locks','ls-files','-z']).decode('utf-8','replace').split(chr(0)) if t}
for i,c in enumerate(cs):
    bad=[t for t in c['command'] if (t.endswith('.sh') or t.endswith('.py')) and t not in tracked]
    if bad: print(i, c['id'], 'MISSING TOKENS:', bad, '| cmd:', ' '.join(c['command'])[:150])
