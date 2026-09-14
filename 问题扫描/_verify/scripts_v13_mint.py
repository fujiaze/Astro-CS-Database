
import subprocess, re
files = subprocess.run(['git','--no-optional-locks','ls-files'], capture_output=True, text=True).stdout.splitlines()
pref = re.compile(r'(TEST|EVID|SCI|ALG|DATA|DISP)-')
NQ = '\\x27\\x22'  # chars ' and " in regex class
fstr  = re.compile('f[' + NQ + '][^' + NQ + ']*(TEST|EVID|SCI|ALG|DATA|DISP)-[A-Za-z0-9._-]*[^' + NQ + ']*[{]')
fstr2 = re.compile('f[' + NQ + '][^' + NQ + ']*[{][^}]*[}][^' + NQ + ']*(TEST|EVID|SCI|ALG|DATA|DISP)-')
pct   = re.compile('(TEST|EVID|SCI|ALG|DATA|DISP)-[A-Za-z0-9._-]*%[-0-9.diors]+')
cat   = re.compile('[' + NQ + '](TEST|EVID|SCI|ALG|DATA|DISP)-[A-Za-z0-9._-]*[' + NQ + ']\\s*[+]')
bt = chr(96)
js_tpl = re.compile('[' + bt + '][^' + bt + ']*(TEST|EVID|SCI|ALG|DATA|DISP)-[^' + bt + ']*[$][{]')
hits = {'fstr':[], 'fstr2':[], 'pct':[], 'cat':[], 'js_tpl':[]}
bad = 0
for f in files:
    if not re.search(r'[.](py|js|sh|ps1|cmake|txt)$', f) and 'CMakeLists' not in f: continue
    if f.startswith(('run/','build/','worktrees/','GaiaDR3','BASS','AstroCS.wiki','Testing/','out/')): continue
    try:
        fh = open(f, encoding='utf-8', errors='ignore')
    except Exception:
        bad += 1; continue
    for i, line in enumerate(fh, 1):
        if not pref.search(line): continue
        if fstr.search(line): hits['fstr'].append((f,i,line.strip()[:170]))
        elif fstr2.search(line): hits['fstr2'].append((f,i,line.strip()[:170]))
        elif pct.search(line): hits['pct'].append((f,i,line.strip()[:170]))
        elif cat.search(line): hits['cat'].append((f,i,line.strip()[:170]))
        elif js_tpl.search(line): hits['js_tpl'].append((f,i,line.strip()[:170]))
    fh.close()
for k in ['fstr','fstr2','pct','cat','js_tpl']:
    v = hits[k]
    print('=== %s : %d ===' % (k, len(v)))
    for f,i,l in v[:60]: print('%s:%d: %s' % (f,i,l))
print('unreadable:', bad, 'tracked-files:', len(files))
