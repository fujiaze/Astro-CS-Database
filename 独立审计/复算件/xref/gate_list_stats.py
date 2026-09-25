import io
import sys
import csv
import re
from pathlib import Path

sys.stdout.reconfigure(encoding='utf-8')
BASE = Path(r'产出/')
GATE = BASE / '独立审计包' / '05_门禁'

rows = list(csv.DictReader(io.open(GATE / '门禁清单.csv', encoding='utf-8-sig', newline='')))
print('门禁清单.csv 数据行 =', len(rows))
cols = list(rows[0].keys())
print('列 =', cols)


def nonempty(r, k):
    return bool((r.get(k) or '').strip())


for k in ['层次与档位', '落地成本档', '是否替换现有门（被替换 ID）']:
    d = {}
    for r in rows:
        v = (r.get(k) or '').strip() or '(空)'
        d[v] = d.get(v, 0) + 1
    print('--- 分布 %s ---' % k)
    for v, n in sorted(d.items(), key=lambda x: -x[1]):
        print('   %-28s %d' % (v[:28], n))

print('--- 空判据字段计数 ---')
for k in cols:
    miss = [r['名称'] for r in rows if not nonempty(r, k)]
    print('   %-34s 空 %d %s' % (k, len(miss), ('例: ' + ', '.join(miss[:5])) if miss else ''))

ids = [r['名称'] for r in rows]
print('--- 名称唯一性：重复 =', [x for x in set(ids) if ids.count(x) > 1], ' 总数 =', len(ids))
weird = [i for i in ids if not re.fullmatch(r'[A-Za-z0-9_.:\-]+', i or '')]
print('--- 名称形态不合 [A-Za-z0-9_.:-] 的 =', weird)

# 被替换 ID 面
rep = {}
for r in rows:
    v = (r.get('是否替换现有门（被替换 ID）') or '').strip()
    if v and v not in ('否', '无', '(空)'):
        for m in re.findall(r'CHK-[A-Z0-9\-]+|CON-[A-Z0-9\-]+|[A-Z][A-Z0-9\-]{4,}', v):
            rep.setdefault(m, []).append(r['名称'])
print('--- 被替换在册 ID 数（去重）=', len(rep))
print('--- 同一被替换 ID 被多条新门认领（潜在一对多）=')
for k, v in sorted(rep.items()):
    if len(v) > 1:
        print('   %s <- %s' % (k, ', '.join(v)))
