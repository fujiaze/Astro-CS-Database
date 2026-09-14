
import json, os, re, subprocess
d = json.load(open('ci/checks.json', encoding='utf-8')); cs = d['checks']
tracked = {t for t in subprocess.check_output(['git','--no-optional-locks','ls-files','-z']).decode('utf-8','replace').split(chr(0)) if t}
ART = re.compile(r'''(["'])((?:build|run|artifacts|evidence|GaiaDR3SP?|BASS DR3)/[^"']{1,70})\1''')
SKIP = re.compile(r'SkipTest|skipIf|skipUnless|self\.skip\(|\bexit\(0\)|pytest\.skip')
for c in cs:
    cmd = c['command']
    if 'discover' not in cmd: continue
    s = cmd[cmd.index('-s')+1]
    files = sorted(t for t in tracked if t.startswith(s+'/') and t.endswith('.py'))
    hits = []
    for f in files:
        src = open(f, encoding='utf-8', errors='replace').read()
        for i, line in enumerate(src.splitlines(), 1):
            m = ART.search(line)
            if m and 'test' not in m.group(2)[:5]:
                hits.append((f, i, m.group(2), 'SKIPRX' if SKIP.search(line) else ''))
    if hits:
        print('### %s [%s] %d hits' % (c['id'], ','.join(c['profiles']), len(hits)))
        for f,i,p,sk in hits[:14]: print('    %-52s:%-5d %s %s' % (f,i,p,sk))
        if len(hits)>14: print('    ... +%d more' % (len(hits)-14))
