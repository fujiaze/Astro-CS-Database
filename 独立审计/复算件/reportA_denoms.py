"""报告 A 的分母：D4 常数台账里"有支撑 / 无支撑 / 未判"的确定计数。
   只读 独立审计/02_科学/常数公式算法总台账.csv 与 raw/AUD-402-常数台账.csv。"""
import io
import csv
import sys
from collections import Counter, defaultdict
from pathlib import Path

sys.stdout.reconfigure(encoding='utf-8')
BASE = Path(r'产出/')
MASTER = BASE / '独立审计包' / '02_科学' / '常数公式算法总台账.csv'

rows = list(csv.DictReader(io.open(MASTER, encoding='utf-8-sig', newline='')))
cols = list(rows[0].keys())
print('主表数据行 %d，列 %d' % (len(rows), len(cols)))
print('列名：', cols)

PROV = [c for c in cols if ('来源' in c or '处置' in c or '出处' in c or '支撑' in c or '证据' in c or '适用域' in c)]
print('疑似判据列：', PROV)
for c in PROV:
    cnt = Counter((r.get(c) or '(空)').strip()[:26] for r in rows)
    print('--- %s 分布（前 12）---' % c)
    for k, v in cnt.most_common(12):
        print('     %-28s %d' % (k, v))

def has_source(r):
    for c in cols:
        if '出处' in c or '来源' in c:
            v = (r.get(c) or '').strip()
            if v and v not in ('无', '不适用（结构性常数）', '-', '—'):
                return True
    return False

face = defaultdict(Counter)
for r in rows:
    loc = (r.get('位置') or r.get('位点') or r.get('位置(路径:行)') or '')
    where = '测试/实验面' if ('eng/tests' in loc or '实验/' in loc or '/tests/' in loc) else '生产面'
    face[where]['总数'] += 1
    face[where]['有出处' if has_source(r) else '无出处'] += 1
print('=== 分面 × 有无出处 ===')
for k, v in face.items():
    print('   %-12s %s' % (k, dict(v)))

sev = Counter((r.get('严重度') or r.get('定级') or '(空)').strip() for r in rows)
print('=== 严重度分布 ===', dict(sev))
dis = Counter()
for r in rows:
    for c in cols:
        if '处置' in c:
            dis[(r.get(c) or '(空)').strip()[:24]] += 1
            break
print('=== 处置分布 ===')
for k, v in dis.most_common(14):
    print('   %-26s %d' % (k, v))
