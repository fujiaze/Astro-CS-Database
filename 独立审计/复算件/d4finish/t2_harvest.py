# -*- coding: utf-8 -*-
"""D4 finish —— harvest for the .md: (i) mechanical symbol-shape + overlap,
(ii) the three high-value lists from the judgement CSVs, (iii) post-patch master
distributions.  Everything printed here is quoted by 常数公式算法总台账.md.
"""
import io, re, sys, csv, collections

sys.stdout.reconfigure(encoding='utf-8')
ROOT = r'产出/'
RAW = ROOT + r'\raw'
M = ROOT + r'\独立审计/02_科学\常数公式算法总台账.csv'
J = ['AUD-402-判读-A1.csv', 'AUD-402-判读-A2.csv', 'AUD-402-判读-A3.csv',
     'AUD-402-判读-BD1.csv']


def rd(p, skip=1):
    return [r for r in list(csv.reader(io.open(p, encoding='utf-8-sig', newline='')))[skip:] if len(r) > 1]


print('== (i) mechanical shapes ==')
main, side = rd(RAW + r'\AUD-402-常数台账.csv'), rd(RAW + r'\AUD-402-常数台账.旁表-无名值与测试钉值.csv')
sh = collections.Counter()
for r in main:
    s = r[0].strip()
    sh['(无名)' if s in ('', '(无名)') or s.startswith('(无名)') else '具名'] += 1
print('main symbol shape:', dict(sh), 'total', len(main))
sh2 = collections.Counter()
for r in side:
    s = r[0].strip()
    sh2['(无名)' if s in ('', '(无名)') or s.startswith('(无名)') else '具名'] += 1
print('side symbol shape:', dict(sh2), 'total', len(side))
k1 = {(r[0], r[1]) for r in main}
k2 = {(r[0], r[1]) for r in side}
print('main ∩ side (same symbol+site):', len(k1 & k2), '| side-only:', len(k2 - k1), '| main-only:', len(k1 - k2))
print('main ∪ side distinct sites:', len(k1 | k2))
TEST = re.compile(r'^eng/tests/|^实验/|/tests?/|_test\.|test_')
print('main: 测试/实验路径行 =', sum(1 for r in main if TEST.search(r[1] or '')),
      '| 生产路径行 =', sum(1 for r in main if not TEST.search(r[1] or '')))
disp = collections.Counter(r[9] for r in main)
print('main 处置 top:', disp.most_common(8))
src = collections.Counter((r[7] or '').split('（')[0] for r in main)
print('main 来源现状 top:', src.most_common(8))

print('\n== (ii) high-value lists from judgement CSVs ==')
jr = []
for f in J:
    jr += [(f, r) for r in rd(RAW + '\\' + f)]


def rows_where(pred, label, limit=14):
    hits = [(f, r) for f, r in jr if pred(r)]
    print('--- %s : %d rows' % (label, len(hits)))
    seen = set()
    for f, r in hits[:limit]:
        k = r[0]
        if k in seen:
            continue
        seen.add(k)
        print('    %-34s | %-46s | %s' % (k[:34], r[1][:46], (r[9] or r[7])[:60]))
    return hits


A = rows_where(lambda r: '公式导出' in r[9] and re.search(r'不应写死|应按式|按公式|可导出|应改.*公式|消除分解任意性|合并为单键', r[9] + r[11]),
               'A 该按公式导出却硬编码（仓内已有导出式）')
B = rows_where(lambda r: re.search(r'多侧|两侧|六侧|三侧|对面|相反|不一致|两制|同值.*写|重复', r[11] + r[6]) and
               re.search(r'默认|兜底|骨架|登记|契约|schema|模板', r[11]),
               'B 多侧取值不一致')
C = rows_where(lambda r: re.search(r'^无出处|无来源|默认值冒充|字面量冒充|无（|冒充', r[7]) and
               re.search(r'阈|门|判据|默认|容差|上限|下限|limit|floor|tol|percent', r[0] + r[4] + r[11], re.I),
               'C 阈值/默认值缺来源（含默认字面量冒充测量值）')
print('unique symbols: A=%d B=%d C=%d' % (len({r[0] for _, r in A}), len({r[0] for _, r in B}),
                                          len({r[0] for _, r in C})))

print('\n== (iii) post-patch master distributions ==')
m = [r for r in list(csv.reader(io.open(M, encoding='utf-8-sig', newline='')))[1:] if len(r) > 1]
print('rows', len(m))
print('类别', dict(collections.Counter(r[1] for r in m)))
print('处置', dict(collections.Counter(r[10] for r in m)))
print('冲突标记非空', sum(1 for r in m if r[13].strip()))
print('第②层已复核', sum(1 for r in m if '第②层' in r[12]), '改判', sum(1 for r in m if '改判' in r[12]))


def fam(r):
    first = re.split(r'（补侧', r[2].split('；')[0])[0]
    p = re.match(r'([\w.\-]+/[\w.\-/]+)', first)
    if not p:
        return '(非站点条目)'
    parts = p.group(1).split('/')
    return '/'.join(parts[:2]) if len(parts) > 2 else parts[0]


print('目录族', collections.Counter(fam(r) for r in m).most_common(14))
print('待确认且有保守方向文字', sum(1 for r in m if r[10] == '待确认' and '【缺输入' not in r[9]))
