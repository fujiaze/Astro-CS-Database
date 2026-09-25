"""回收 核验-CIT-*.md → CSV（只读共享台账，不写回；由前台统一回写）。"""
import re
import sys
import io
import csv
import glob
from pathlib import Path

sys.stdout.reconfigure(encoding='utf-8')
BASE = Path(r'产出/')
OUT = BASE / 'inventory'

HEAD = re.compile(r'^##+\s*#?(\d+)\b(.{0,120})')
FIELDS = [('存在性', r'存在性[^\n]{0,60}?(已核|不存在|UNPROVEN)'),
          ('关联性', r'关联性[^\n]{0,60}?(关联对|关联错|角色错绑|节号错绑|UNPROVEN)'),
          ('版本', r'版本[^\n]{0,40}?(版本对|版本错|未钉版次|不适用)')]
EVID = re.compile(r'(arXiv|DOI|10\.\d{4}|Crossref|aspbooks|API)')

rows = []
for f in sorted(glob.glob(str(BASE / 'raw' / '核验-CIT-*.md'))):
    txt = Path(f).read_text(encoding='utf-8').splitlines()
    cur = None
    for i, l in enumerate(txt):
        m = HEAD.match(l.strip())
        if m:
            if cur:
                rows.append(cur)
            cur = {'批次': Path(f).name, '序号': m.group(1), '标题行': m.group(2).strip()[:90],
                   '存在性': '', '关联性': '', '版本': '', '依据摘录': '', '行号': i + 1}
            continue
        if not cur:
            continue
        for k, pat in FIELDS:
            mm = re.search(pat, l)
            if mm and not cur[k]:
                cur[k] = mm.group(1) if k != '关联性' else ('角色错绑' if '角色错绑' in l else
                                                           ('节号错绑' if '节号错绑' in l else
                                                            ('关联错' if '关联错' in l else '关联对')))
        if EVID.search(l) and len(cur['依据摘录']) < 200 and l.strip().startswith(('-', '|', '1', '2', '3')):
            cur['依据摘录'] += l.strip()[:120] + ' ~ '
    if cur:
        rows.append(cur)

with io.open(OUT / '文献核验回收.csv', 'w', encoding='utf-8-sig', newline='') as f:
    w = csv.DictWriter(f, fieldnames=list(rows[0].keys()) if rows else ['批次'])
    if rows:
        w.writeheader()
        for r in rows:
            w.writerow(r)
print('回收 %d 条（来自 %d 份成稿）' % (len(rows), len({r['批次'] for r in rows})))
for k in ('存在性', '关联性', '版本'):
    d = {}
    for r in rows:
        d[r[k] or '(未识别)'] = d.get(r[k] or '(未识别)', 0) + 1
    print('   %-6s %s' % (k, d))
bad = [r for r in rows if r['关联性'] in ('角色错绑', '节号错绑', '关联错') or r['存在性'] == '不存在'
       or r['版本'] == '版本错']
print('=== 判红条目 %d 条 ===' % len(bad))
for r in bad:
    print('   #%s [%s] %s | 关联性=%s 版本=%s' % (r['序号'], r['批次'][-8:-3], r['标题行'][:56], r['关联性'], r['版本']))
un = [r for r in rows if 'UNPROVEN' in (r['存在性'] + r['关联性']) or r['版本'] == '未钉版次']
print('待补证据（UNPROVEN／未钉版次）%d 条' % len(un))
