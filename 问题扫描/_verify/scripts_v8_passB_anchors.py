
import re, json, os, subprocess, collections
recs = json.load(open('问题扫描/_verify/_v8_comment_lines.json', encoding='utf-8'))
prose = [r for r in recs if r['kind'] in ('cmt-line','cmt-trailing','doc-line')]

# tracked files set
tracked = set(subprocess.run(['git','--no-optional-locks','ls-files'],capture_output=True,text=True).stdout.split('\n'))
worktree = set()
for dp, dn, fn in os.walk('.'):
    if '/.git' in dp: continue
    for f in fn:
        worktree.add(os.path.relpath(os.path.join(dp,f),'.'))
# line counts cache
lc = {}
def nlines(p):
    if p not in lc:
        try:
            with open(p, 'rb') as fh:
                lc[p] = sum(1 for _ in fh)
        except Exception:
            lc[p] = None
    return lc[p]

PATH_RE = re.compile(r'(?<![\w/.\-])((?:\./)?(?:lib|cli|include|tools|docs|tests|ci|contracts|modules|cmake|packaging|scripts|engineering|evidence|run|reports|testdata|providers|runtime)/[A-Za-z0-9_.\-/]*[A-Za-z0-9_.\-])')
LINE_RE = re.compile(r'^((?:\./)?(?:lib|cli|include|tools|docs|tests|ci|contracts|modules|cmake|packaging|scripts|engineering|evidence|run|reports|testdata|providers|runtime)/[A-Za-z0-9_.\-/]*[A-Za-z0-9_.\-]):(\d+)(?:-(\d+))?\b')

bad_path = []
line_oob = []
runrefs = []
for r in prose:
    t = r['t']
    for m in PATH_RE.finditer(t):
        p = m.group(1)
        if p.startswith('run/'):
            runrefs.append((r['c'], r['f'], r['n'], p))
            continue
        # trailing punctuation cleanup
        pp = p.rstrip('.,;:、。）)')
        exists = pp in tracked or pp in worktree
        # check for :NNN directly after
        rest = t[m.end():]
        ml = re.match(r'^:(\d+)(?:-(\d+))?', rest)
        if not exists:
            bad_path.append((r['c'], r['f'], r['n'], pp, bool(ml)))
        if ml:
            lp = os.path.join('.', pp)
            N = nlines(pp) if os.path.isfile(lp) else None
            if N is None:
                pass  # already in bad_path
            else:
                a = int(ml.group(1)); b = int(ml.group(2)) if ml.group(2) else a
                if a > N or b > N:
                    line_oob.append((r['c'], r['f'], r['n'], pp, a, b, N))

print('== PATH with no host (not tracked, not on disk):', len(bad_path))
seen=set()
for c,f,n,p,hasl in bad_path:
    k=(p,hasl)
    print('  %s %s:%d -> %s%s' % (c,f,n,p, ('  (+:line anchor)' if hasl else '')))
print()
print('== :NNN anchor beyond file end:', len(line_oob))
for c,f,n,p,a,b,N in line_oob:
    print('  %s %s:%d -> %s:%d-%d (file has %d lines)' % (c,f,n,p,a,b,N))
print()
print('== run/ references in added prose:', len(runrefs))
cnt = collections.Counter((c,f) for c,f,n,p in runrefs)
print('distinct run-ref mentions:', len(runrefs), 'files:', len(set(f for _,f,_,_ in runrefs)))
for c,f,n,p in runrefs:
    print('  %s %s:%d -> %s' % (c,f,n,p))
