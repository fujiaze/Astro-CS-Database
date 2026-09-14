import json, os, re, fnmatch, subprocess, collections
reg=json.load(open('ci/checks.json',encoding='utf-8'))['checks']
# tracked files
tracked = subprocess.run(['git','-c','core.quotepath=off','--no-optional-locks','ls-files'],capture_output=True,text=True).stdout.splitlines()
tset=set(tracked)
print('TRACKED FILES', len(tset))
def glob_match(pat, path):
    # translate ** and * like checks runner: use fnmatch-ish with ** meaning any depth
    if pat.endswith('/**'):
        pre = pat[:-3]
        return path.startswith(pre+'/')
    if '**' in pat:
        rx = ''
        i=0
        while i < len(pat):
            if pat[i:i+3]=='**/':
                rx += '(?:.*/)?'; i+=3
            elif pat[i:i+2]=='**':
                rx += '.*'; i+=2
            elif pat[i]=='*':
                rx += '[^/]*'; i+=1
            elif pat[i]=='?':
                rx += '[^/]'; i+=1
            else:
                rx += re.escape(pat[i]); i+=1
        return re.fullmatch(rx, path) is not None
    # plain: treat as prefix-or-exact
    return path == pat or path.startswith(pat+'/') or fnmatch.fnmatch(path, pat)
never=[]
for c in reg:
    pats=c.get('changed_paths',[])
    hits=set()
    for p in pats:
        for t in tset:
            if glob_match(p,t): hits.add(t)
    if not hits:
        never.append((c['id'], pats))
print()
print('== gates whose changed_paths match ZERO tracked files ==', len(never))
for n in never: print('  ', n[0], '|', n[1])
print()
print('== gates with EMPTY changed_paths ==', [c['id'] for c in reg if not c.get('changed_paths')])
print('== gates with EMPTY profiles ==', [c['id'] for c in reg if not c.get('profiles')])
# distribution of changed_paths counts
cnt = collections.Counter(len(c.get('changed_paths',[])) for c in reg)
print('changed_paths len histogram', dict(sorted(cnt.items())))
