
import re, json, os, subprocess
recs = json.load(open('问题扫描/_verify/_v8_comment_lines.json', encoding='utf-8'))
prose = [r for r in recs if r['kind'] in ('cmt-line','cmt-trailing','doc-line')]
tracked = set(subprocess.run(['git','--no-optional-locks','ls-files'],capture_output=True,text=True).stdout.split('\n'))
dirs = set()
for dp, dn, fn in os.walk('.'):
    if '/.git' in dp or '/.git' == dp: continue
    for d in dn:
        dirs.add(os.path.relpath(os.path.join(dp,d),'.'))
def exists(p):
    if p in tracked or p in dirs: return True
    if os.path.exists(p): return True
    return False
PATH_RE = re.compile(r'(?<![\w/.\-])((?:\./)?(?:lib|cli|include|tools|docs|tests|ci|contracts|modules|cmake|packaging|scripts|engineering|evidence|reports|testdata|providers|runtime)/[A-Za-z0-9_.\-/]*[A-Za-z0-9_.\-])')
miss = []
for r in prose:
    t = r['t']
    for m in PATH_RE.finditer(t):
        p = m.group(1).rstrip('.,;:、。）)')
        if p.startswith('run/'): continue
        if exists(p): continue
        # try relative to the file's own directory (module-local reference)
        base = os.path.dirname(r['f'] or '')
        rel = os.path.normpath(os.path.join(base, p)) if base else p
        alt = exists(rel) or exists(os.path.normpath(os.path.join('.', rel)))
        miss.append((r['c'], r['f'], r['n'], p, alt))
print('missing-path mentions:', len(miss))
for c,f,n,p,alt in miss:
    print('  %-9s %s:%d -> %s   [rel-ok=%s]' % (c,f,n,p,alt))
