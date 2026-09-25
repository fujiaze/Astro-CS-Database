import sys, os, glob, re
sys.stdout.reconfigure(encoding='utf-8')
os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
per = {}
for f in glob.glob('inventory/CR-*.txt'):
    b = os.path.basename(f)[3:-4]
    per[b] = [l.strip() for l in open(f, encoding='utf-8') if l.strip() and not l.startswith('#')]
done = set()
flight = 0
for b, fs in per.items():
    p = 'raw/通读-CR-%s.md' % b
    ok = False
    if os.path.exists(p) and os.path.getsize(p) > 20000:
        t = open(p, encoding='utf-8', errors='ignore').read()
        m = re.findall(r'PROGRESS:\s*(\d+)/(\d+)', t)
        if m:
            a, c = max((int(x), int(y)) for x, y in m)
            ok = a >= c
    if ok:
        done.update(x.split(':')[0] for x in fs)
    else:
        flight += 1
txt = "".join(open(p, encoding='utf-8', errors='ignore').read() for p in glob.glob('raw/通读-CR-*.md'))
pat = re.compile('定级：\*{0,2}S1')
print("batches complete: %d/%d | distinct files: %d | S1 lines: %d"
      % (len(per) - flight, len(per), len(done), len(pat.findall(txt))))
