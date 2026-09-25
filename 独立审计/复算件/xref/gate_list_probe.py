import io
import sys
import csv
import re
import os
from pathlib import Path

sys.stdout.reconfigure(encoding='utf-8')
BASE = Path(r'产出/')
GATE = BASE / '独立审计包' / '05_门禁'
SRC = GATE / '门禁清单.csv'

rows = list(csv.DictReader(io.open(SRC, encoding='utf-8-sig', newline='')))
cols = list(rows[0].keys())

print('=== 尾部 3 行的名称列 ===')
for r in rows[-3:]:
    print(repr(r['名称'])[:120])
print('=== PROGRESS 行是否只有名称列非空 ===')
for r in rows:
    if 'PROGRESS' in (r['名称'] or ''):
        filled = {k: v for k, v in r.items() if (v or '').strip()}
        print('   非空列数 %d / %d：' % (len(filled), len(cols)), list(filled))
print('=== 层次与档位 为 — 或空的行 ===')
for r in rows:
    v = (r.get('层次与档位') or '').strip()
    if v in ('', '—', '-'):
        print('   名称=%s | 落地成本档=%r | 判据前 40=%s' % (r['名称'][:60], r.get('落地成本档'), (r.get('判据') or '')[:40]))

DISPO = [('退役', '退役'), ('改造', '改造'), ('替换', '替换'), ('合并', '合并'), ('拆分', '拆分'),
         ('新增', '新增'), ('保留', '保留'), ('批量', '批量族'), ('注册', '纳入注册'), ('改名', '改名'),
         ('定案', '定案'), ('不再当门', '批量族')]


def split_name(n):
    core = n
    note = ''
    m = re.match(r'^(.*?)（(.*?)）\s*$', n)
    if m and not m.group(1).endswith('/'):
        core, note = m.group(1).strip(), m.group(2).strip()
    dispo = ''
    for kw, tag in DISPO:
        if kw in (note or '') or (('（' + kw) in n) or (n.endswith(kw + '）')):
            dispo = tag
            break
    ids = re.findall(r'(?:CHK|CON|GATE|SPEC|L2|DOC|RESOURCE|WORKER|AIO)-[A-Z0-9][A-Z0-9\-]*', core)
    return core, dispo, note, ids


print('=== 规范化预览（前 12 行 + 全部含路径/斜杠的行）===')
odd = []
for i, r in enumerate(rows):
    core, dispo, note, ids = split_name(r['名称'])
    multi = '/' in core or '.py' in core or ' ' in core
    if multi:
        odd.append((i, r['名称'], core, dispo, ids))
    if i < 12:
        print('   %-46s -> core=%-40s dispo=%-6s ids=%d' % (r['名称'][:46], core[:40], dispo, len(ids)))
print('=== 含路径/空格/斜杠的名称行 %d 条 ===' % len(odd))
for i, n, core, dispo, ids in odd:
    print('   行%2d %s' % (i + 2, n[:110]))
    print('        core=%s dispo=%s ids=%s' % (core[:70], dispo, ids))

out = BASE / '复算' / 'xref' / 'gate_list_probe.txt'
out.write_text('probe done\n', encoding='utf-8')

hits = []
for root, dirs, fs in os.walk(BASE / '独立审计包'):
    for fn in fs:
        if fn.endswith(('.md', '.csv')):
            p = Path(root) / fn
            for ln, line in enumerate(p.read_text(encoding='utf-8').splitlines(), 1):
                if re.search(r'\b65\s*[条道个]|65 个门|65 道|64 条|64 道', line):
                    hits.append('%s:%d  %s' % (p.relative_to(BASE).as_posix(), ln, line.strip()[:130]))
print('=== 包内引用"65/64 条门"的位置 %d 处 ===' % len(hits))
for h in hits[:20]:
    print('   ' + h)
