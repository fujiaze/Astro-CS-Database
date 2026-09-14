
import json, os, re, ast, subprocess
d = json.load(open('ci/checks.json', encoding='utf-8')); cs = d['checks']
tracked = {t for t in subprocess.check_output(['git','--no-optional-locks','ls-files','-z']).decode('utf-8','replace').split(chr(0)) if t}
def roots_for(c):
    cmd=c['command']; roots=[]
    if 'discover' in cmd: roots.append(cmd[cmd.index('-s')+1])
    for tok in cmd:
        if tok.endswith('.py'): roots.append(tok)
    return roots
NET = re.compile(r'urllib\.request|urlopen|requests\.(get|post|Session)|socket\.create_connection|http\.client|gea\.esa|gaia\.astra|http://|https://')
REAL = re.compile(r'GaiaDR3SP|GaiaDR3|BASS DR3|/mnt/|fujia@|fatduck|Fatduck|VM-BJ|vm-bj')
print('=== A) network / real-host literals inside CI-loaded python ===')
for c in cs:
    roots=roots_for(c)
    if not roots: continue
    files=[f for f in sorted(tracked) if f.endswith('.py') and any(f==r or f.startswith(r+'/') for r in roots)]
    nh=[]; rh=[]
    for f in files:
        src=open(f,encoding='utf-8',errors='replace').read()
        for i,l in enumerate(src.splitlines(),1):
            if NET.search(l) and not l.strip().startswith('#'): nh.append((f,i,l.strip()[:110]))
            if REAL.search(l) and not l.strip().startswith('#'): rh.append((f,i,l.strip()[:110]))
    if nh or rh:
        print('### %s [%s]' % (c['id'], ','.join(c['profiles'])))
        for x in nh[:6]: print('   NET %s:%d %s' % x)
        for x in rh[:6]: print('   REAL %s:%d %s' % x)
print()
print('=== B) checker top-level path reads of non-existent dirs (silent-empty risk) ===')
import collections
pat = re.compile(r'''(?:REPO|repo)\s*(?:/|,\s*)["']([^"']+)["'](?:\s*,\s*["']([^"']+)["'])?''')
for c in cs:
    roots=roots_for(c)
    for f in roots:
        if not f.endswith('.py') or f not in tracked: continue
        src=open(f,encoding='utf-8',errors='replace').read()
        for m in pat.finditer(src):
            p='/'.join(x for x in (m.group(1), m.group(2)) if x)
            if len(p)<3 or p.startswith('run/') or p.startswith('build/'): continue
            if ('/' in p or p in ('docs','lib','cli','tests','tools','ci','evidence','artifacts','modules','runtime','contracts')) and not os.path.exists(p):
                if p.split('/')[0] not in ('run','build','artifacts','evidence','logs','out'): continue
                print('  %-26s %-52s literal path %r MISSING (exists=%s)' % (c['id'], f, p, os.path.exists(p)))
