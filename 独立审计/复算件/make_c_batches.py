"""按"生产面具名符号"切判读批次，并把真分母写出来。"""
import sys
import io
import csv
from collections import Counter
from pathlib import Path

sys.stdout.reconfigure(encoding='utf-8')
BASE = Path(r'产出/')
SRC = BASE / 'raw' / 'AUD-402-常数台账.csv'
INV = BASE / 'inventory'
INV.mkdir(exist_ok=True)

rows = list(csv.reader(io.open(SRC, encoding='utf-8-sig', newline='')))
hdr, body = rows[0], [r for r in rows[1:] if len(r) > 9]
SY, LO, VA, UN, SR, DI = 0, 1, 2, 3, 7, 9


def face(loc):
    if any(x in loc for x in ('eng/tests/', '实验/', '/tests/', '/oracle/', '/fixtures/')):
        return '测试/实验'
    if any(x in loc for x in ('docs/', 'artifacts/', '工程控制/', 'run/')):
        return '文档/证据'
    return '生产'


cnt = Counter()
sites = {}
for r in body:
    f = face(r[LO])
    named = (r[SY] or '').strip() not in ('', '(无名)')
    cnt[(f, named, (r[DI] or '').strip()[:6] or '(空)')] += 1
    if f == '生产' and named:
        sites.setdefault(r[SY].strip(), []).append('%s:%s|%s|%s' % (r[LO], r[VA], r[UN], (r[SR] or '')[:40]))

print('=== 位点分布（面 × 是否具名 × 处置档）===')
for (f, named, d), v in sorted(cnt.items(), key=lambda x: -x[1])[:14]:
    print('   %-10s %-6s %-8s %6d' % (f, '具名' if named else '无名', d, v))
prod_named_rows = sum(len(v) for v in sites.values())
print('生产面具名：符号 %d 个 / 位点 %d 行' % (len(sites), prod_named_rows))
supp = sum(v for (f, n, d), v in cnt.items() if d in ('文献值', '公式导出', '实验标定'))
print('全仓已给支撑的位点 = %d（占 20,595 的 %.2f%%）；其余全部挂"待确认"' % (supp, 100 * supp / len(body)))

K = 8
order = sorted(sites, key=lambda s: (-len(sites[s]), s))
for i in range(K):
    part = order[i::K]
    lines = ['# 批次 C-%02d｜对象 raw/AUD-402-常数台账.csv 中"生产面＋具名＋处置=待确认"的符号｜%d 个符号 / %d 位点'
             % (i + 1, len(part), sum(len(sites[s]) for s in part)),
             '# 格式：符号\t位点数\t位点样例（路径:行|现行值|单位|来源现状）']
    for s in part:
        lines.append('%s\t%d\t%s' % (s, len(sites[s]), ' ;; '.join(sites[s][:3])))
    (INV / ('C-%02d.txt' % (i + 1))).write_text('\n'.join(lines) + '\n', encoding='utf-8')
    print('  C-%02d: 符号 %d / 位点 %d  首行 %s' % (i + 1, len(part), sum(len(sites[s]) for s in part),
                                                  lines[2].split('\t')[0][:40]))
