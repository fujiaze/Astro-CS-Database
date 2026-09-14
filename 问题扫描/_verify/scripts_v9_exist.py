import json, os, glob, collections
d = json.load(open('ci/checks.json', encoding='utf-8'))
ch = d['checks']
# A. script existence
missing = []
for c in ch:
    cmd = list(map(str, c['command']))
    # find first non-interpreter arg that looks like a path
    if cmd[0] in ('python3','python'):
        idx = 1
        while idx < len(cmd) and cmd[idx].startswith('-'): idx += 1
        target = cmd[idx] if idx < len(cmd) else None
    elif cmd[0] == 'bash':
        target = cmd[1]
    else:
        target = cmd[0]
    ok = target and os.path.exists(target)
    if not ok: missing.append((c['id'], target, ' '.join(cmd)[:120]))
print('MISSING TARGETS:', len(missing))
for m in missing: print('  ', m)
# B. test discovery dirs
print()
for c in ch:
    cmd = list(map(str,c['command']))
    if 'discover' in cmd:
        s = cmd[cmd.index('-s')+1]
        pat = (cmd[cmd.index('-p')+1] if '-p' in cmd else 'test*.py')
        files = glob.glob(os.path.join(s,'**',pat), recursive=True)
        n = len(files)
        print('DISCOVER %-18s dir=%-40s exists=%s glob=%s matched=%d' % (c['id'], s, os.path.isdir(s), pat, n))
