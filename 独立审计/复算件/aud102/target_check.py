import io, os, re, sys, subprocess
sys.stdout.reconfigure(encoding='utf-8')
R = r"F:\Astro dev\Astro CS Normalization Database"
out = subprocess.run(['git', '-C', R, '-c', 'core.quotePath=false', 'ls-files'],
                     capture_output=True, text=True, encoding='utf-8').stdout
tracked = {t.replace(chr(92), '/') for t in out.split()}
F = r"独立审计/01_文档\迁移合并清单.md"
rows = [l.rstrip('\n') for l in io.open(F, encoding='utf-8') if l.startswith('| `')]
TGT = re.compile(r'→ ([\w\u4e00-\u9fff.\-\*/]+)')
bad, ok, dirs = [], 0, 0
for l in rows:
    m = TGT.search(l)
    if not m:
        continue
    v = m.group(1).rstrip('，。；')
    if v.endswith('/'):
        dirs += 1
        exists = any(t.startswith(v) for t in tracked)
    else:
        exists = v in tracked
    if exists:
        ok += 1
    else:
        bad.append((l.split('|')[1].strip(' `'), v))
print('targets:', ok + len(bad), 'exists:', ok, 'missing:', len(bad), 'dir-like:', dirs)
for a, b in bad:
    print('  MISSING  %-58s -> %s' % (a, b))
