import re, os, collections
ROOT=os.getcwd()
DIRS=['lib','cli','runtime','providers','tools','ci','tests','modules']
exts=('.c','.cpp','.h','.hpp','.py','.sh')
read=collections.Counter(); write=collections.Counter(); files=[]
for d in DIRS:
    for dp,dn,fn in os.walk(os.path.join(ROOT,d)):
        if 'build' in dp or 'third_party' in dp: continue
        for f in fn:
            if f.endswith(exts): files.append(os.path.join(dp,f))
pat_r=re.compile(r'getenv[^A-Za-z0-9_]*([A-Za-z][A-Za-z0-9_]{2,})')
pat_py=re.compile(r'environ[^A-Za-z0-9_]*[A-Za-z0-9_.]*[^A-Za-z0-9_]*([A-Z][A-Z0-9_]{2,})')
pat_w=re.compile(r'(?:setenv|putenv)[^A-Za-z0-9_]*([A-Za-z][A-Za-z0-9_]{2,})')
for p in files:
    t=open(p,encoding='utf-8',errors='ignore').read()
    for m in pat_r.findall(t): read[m]+=1
    for m in pat_py.findall(t): read[m]+=1
    for m in pat_w.findall(t): write[m]+=1
print('=== READ ===')
for k in sorted(read): print('  %4d %s' % (read[k],k))
print('=== WRITE ===')
for k in sorted(write): print('  %4d %s' % (write[k],k))
print('=== WRITE-only (from never read in scanned dirs) ===')
for k in sorted(write):
    if k not in read: print('   W-only', k)