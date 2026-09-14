import subprocess, os, re, pathlib, collections
def git(*a):
    return subprocess.run(['git','-c','core.quotepath=off','--no-optional-locks']+list(a),capture_output=True,text=True,errors='replace').stdout
tracked = set(x for x in git('ls-files').splitlines() if x.strip())
untracked = [x for x in git('ls-files','--others','--exclude-standard').splitlines() if x.strip()]
# only source-ish untracked files
srcs = [u for u in untracked if re.search(r'\.(cpp|c|h|hpp|py|inc|def|map|json|csv)$', u)]
print('UNTRACKED (non-ignored) total', len(untracked), 'source-like', len(srcs))
# gather tracked CMakeLists/cmake content
cml = {}
for t in tracked:
    if t.endswith('CMakeLists.txt') or t.endswith('.cmake'):
        try: cml[t]=pathlib.Path(t).read_text(encoding='utf-8',errors='replace')
        except Exception: pass
print('tracked cmake sources', len(cml))
refs = collections.defaultdict(list)
for u in srcs:
    base = os.path.basename(u)
    for f, txt in cml.items():
        if base in txt:
            # find matching lines
            for i,ln in enumerate(txt.splitlines(),1):
                if base in ln and not ln.strip().startswith('#'):
                    refs[u].append((f,i,ln.strip()[:120]))
                    break
print()
print('== untracked-but-referenced-by-tracked-cmake ==')
for u,v in sorted(refs.items()):
    print(' ', u, '->', v[0])
print('COUNT', len(refs))
print()
print('== all untracked source-like files (sample 60) ==')
for u in srcs[:60]: print('  ', u)
