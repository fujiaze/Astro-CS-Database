import os, re, fnmatch, subprocess, collections, pathlib, json, csv
PATTERNS=['lib/*/include/**/*.h','lib/*/include/*.h','lib/*/cpp/include/**/*.h','lib/*/*/include/**/*.h','lib/plate_solve/cpp/ipv/include/*.h']
tracked=set(x for x in subprocess.run(['git','-c','core.quotepath=off','--no-optional-locks','ls-files'],capture_output=True,text=True).stdout.splitlines())
# walk working tree, skipping heavy dirs to emulate rglob but record shadow hits
hits=[]
for root,dirs,files in os.walk('.'):
    rel=root[2:] if root.startswith('./') else root
    if rel.count('/')>5: dirs[:]=[]; continue
    for f in files:
        if not f.endswith('.h'): continue
        p=(rel+'/'+f) if rel else f
        for pat in PATTERNS:
            if fnmatch.fnmatch(p,pat):
                hits.append(p); break
shadow=[h for h in hits if h.split('/')[0] in ('run','build','out','worktrees','artifacts')]
print('working-tree matching headers:', len(hits), ' tracked among them:', len([h for h in hits if h in tracked]))
print('shadow-tree matching headers:', len(shadow))
for s in shadow[:15]: print('   ', s)
print()
print('non-shadow but UNTRACKED matching headers:', [h for h in hits if h not in tracked and h not in shadow][:15])
