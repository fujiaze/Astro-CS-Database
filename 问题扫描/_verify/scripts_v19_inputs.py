
import json, os, re, subprocess
d = json.load(open('ci/checks.json', encoding='utf-8'))
cs = d['checks']
tracked = {t for t in subprocess.check_output(['git','--no-optional-locks','ls-files','-z']).decode('utf-8','replace').split(chr(0)) if t}

def entry_script(c):
    for tok in c['command'][1:]:
        if tok.endswith('.py') and '/' in tok:
            return tok
    return None

PAT = re.compile(r'["\x27]((?:build|run|artifacts|evidence|reports|logs|out|GaiaDR3|GaiaDR3SP|BASS DR3|testdata|contracts)/[^"\x27]{2,90})["\x27]')
rows = []
for c in cs:
    es = entry_script(c)
    if not es or es not in tracked: continue
    src = open(es, encoding='utf-8', errors='replace').read()
    hits = set()
    for m in PAT.finditer(src):
        p = m.group(1)
        if '%' in p or '{' in p or '$' in p or p.endswith('/'): continue
        hits.add(p)
    bad = []
    for p in sorted(hits):
        ign = subprocess.run(['git','--no-optional-locks','check-ignore','-q','--stdin'], input=p.encode(), capture_output=True).returncode==0
        tr = p in tracked
        if (not tr) and (ign or p.split('/')[0] in ('build','run','logs','out','artifacts','GaiaDR3','GaiaDR3SP','BASS DR3')):
            bad.append((p,'IGNORED' if ign else 'UNTRACKED', 'present' if os.path.exists(p) else 'absent'))
    if bad:
        rows.append((c['id'], es, ','.join(c['profiles']), bad))
print('=== checks whose checker body references untracked/ignored trees ===')
for rid, es, prof, bad in rows:
    print('  %-32s %-54s [%s]' % (rid, es, prof))
    for p,k,e in bad: print('        %-56s %-10s %s' % (p,k,e))
print('total flagged:', len(rows))
