import io
import sys
import csv
import re
import shutil
from pathlib import Path

sys.stdout.reconfigure(encoding='utf-8')
BASE = Path(r'产出/')
GATE = BASE / '独立审计包' / '05_门禁'
SRC = GATE / '门禁清单.csv'
BACKUP = BASE / '复算' / 'xref' / '门禁清单.原样备份.csv'

IDPAT = re.compile(r'(?:CHK|CON|GATE|SPEC|L2|DOC|RESOURCE|WORKER|AIO)-[A-Z0-9][A-Z0-9\-]*')
DISPO = [('退役双删', '退役'), ('已退役件族', '批量族（不入门数）'), ('退役', '退役'), ('批量', '批量族'),
         ('改造', '改造'), ('替换', '替换'), ('合并', '合并'), ('拆', '拆分'), ('新增', '新增'),
         ('保留', '保留'), ('注册', '纳入注册'), ('改名与补齐', '改名'), ('定案', '定案')]
NEWFIRST = ('GATE-',)

rows = list(csv.DictReader(io.open(SRC, encoding='utf-8-sig', newline='')))
cols = list(rows[0].keys())
shutil.copyfile(SRC, BACKUP)

dropped = []
kept = []
for r in rows:
    n = (r.get('名称') or '').strip()
    if n.startswith('<!--') or not n:
        dropped.append(n)
        continue
    kept.append(r)

changes = []
out_rows = []
for r in kept:
    n = r['名称'].strip()
    core, note = n, ''
    m = re.match(r'^(.*?)（(.*?)）\s*$', n)
    if m:
        core, note = m.group(1).strip(), m.group(2).strip()
    dispo = ''
    for kw, tag in DISPO:
        if kw in note or kw in n:
            dispo = tag
            break
    ids = []
    for x in IDPAT.findall(core):
        if x not in ids:
            ids.append(x)
    if core.startswith(NEWFIRST):
        primary, related = core, [x for x in ids if x != core]
        dispo = dispo or '新增'
    elif ids:
        primary, related = ids[0], ids[1:]
        dispo = dispo or '改造'
    else:
        primary, related = core, []
        dispo = dispo or ('批量族' if '族' in core else '待定档')
    if primary in ('BATCH-B1', 'BATCH-B2', 'BATCH-B3', 'BATCH-B4', 'BATCH-B5') or '族' in primary:
        primary = re.match(r'^(BATCH-B\d)', n)
        primary = primary.group(1) if primary else '族名（待规范）'
    tier = (r.get('层次与档位') or '').strip()
    if tier in ('—', '-', ''):
        tier = '不适用（%s）' % (dispo or '未归档')
        changes.append('层次与档位 由 %r 归一为 %r（%s）' % (r['层次与档位'] or '', tier, primary))
    nr = dict(r)
    nr['名称'] = primary
    nr['处置'] = dispo
    nr['关联既有 ID'] = '；'.join(related)
    nr['原名称（逐字留存）'] = n
    nr['层次与档位'] = tier
    if (' / ' in n or '.py' in n) and related:
        changes.append('名称列拆出多身份：%s → 主键 %s ＋ 关联 %s' % (n[:64], primary, '；'.join(related)))
    out_rows.append(nr)

NEWCOLS = ['名称', '处置', '关联既有 ID'] + cols[1:] + ['原名称（逐字留存）']
NEWCOLS = [c for c in dict.fromkeys(NEWCOLS)]
with io.open(SRC, 'w', encoding='utf-8-sig', newline='') as f:
    w = csv.DictWriter(f, fieldnames=NEWCOLS, extrasaction='ignore')
    w.writeheader()
    for r in out_rows:
        w.writerow(r)

print('原行数 %d → 现行数 %d（丢弃 %d 行：%s）' % (len(rows), len(out_rows), len(dropped), dropped))
print('列 %d → %d：%s' % (len(cols), len(NEWCOLS), NEWCOLS))
print('变更 %d 条：' % len(changes))
for c in changes:
    print('   ' + c)

d = {}
for r in out_rows:
    d[r['处置']] = d.get(r['处置'], 0) + 1
print('处置分布：', '；'.join('%s=%d' % kv for kv in sorted(d.items(), key=lambda x: -x[1])))
print('门数口径（不含批量族与退役）= %d' % sum(v for k, v in d.items() if k not in ('批量族', '批量族（不入门数）', '退役')))
prim = [r['名称'] for r in out_rows]
print('主键重复：', [x for x in set(prim) if prim.count(x) > 1])
bad = [x for x in prim if not re.fullmatch(r'[A-Za-z0-9_.\-]+', x)]
print('主键形态仍不合判定的：', bad)
cnt = {}
for r in out_rows:
    for x in (r['关联既有 ID'].split('；') if r['关联既有 ID'] else []):
        cnt[x] = cnt.get(x, 0) + 1
multi = {k: v for k, v in cnt.items() if v > 1}
print('一对多（同一既有 ID 被多条认领）%d 个：%s' % (len(multi), multi))
