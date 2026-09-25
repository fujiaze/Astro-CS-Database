import glob
import re
import sys
import os

sys.stdout.reconfigure(encoding='utf-8')
files = sorted(glob.glob('通读-CR-*.md'))
tot_f = tot_s1 = 0
for f in files:
    t = open(f, encoding='utf-8').read()
    heads = re.findall(r'^### (CR-\d+-\d+).{0,80}', t, re.M)
    s1 = [h for h, m in zip(heads, re.findall(r'^### CR-\d+-\d+.{0,400}', t, re.M)) if 'S1' in m]
    tot_f += len(heads)
    tot_s1 += len(s1)
    print('%-16s finding %2d  S1 %d  表行 %3d  KB %d' % (
        f, len(heads), len(s1), len([l for l in t.splitlines() if l.startswith('|') and l.count('|') >= 6]),
        os.path.getsize(f) // 1024))
print('合计：成稿 %d 份，finding %d 条，其中定级含 S1 %d 条' % (len(files), tot_f, tot_s1))
