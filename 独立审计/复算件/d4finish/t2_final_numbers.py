# -*- coding: utf-8 -*-
"""D4 finish —— final number lock for the .md (one deterministic command)."""
import io, re, sys, csv, collections

sys.stdout.reconfigure(encoding='utf-8')
ROOT = r'产出/'
RAW = ROOT + r'\raw'
M = ROOT + r'\独立审计/02_科学\常数公式算法总台账.csv'
J = ['AUD-402-判读-A1.csv', 'AUD-402-判读-A2.csv', 'AUD-402-判读-A3.csv', 'AUD-402-判读-BD1.csv']


def rd(p, skip=1):
    return [r for r in list(csv.reader(io.open(p, encoding='utf-8-sig', newline='')))[skip:] if len(r) > 1]


jr = []
for f in J:
    jr += rd(RAW + '\\' + f)

print('== judgement layer ==')
print('rows', len(jr))
print('distinct symbols', len({r[0].strip() for r in jr}),
      '| named only', len({r[0].strip() for r in jr if not r[0].strip().startswith('(无名)')}))
disp = collections.Counter(r[9].split('（')[0].strip() for r in jr)
print('处置 首词:', dict(disp.most_common()))
src = collections.Counter(r[7].split('（')[0].strip() for r in jr)
print('来源现状 首词:', dict(src.most_common()))
na_disp = sum(1 for r in jr if r[9].strip().startswith('不适用'))
na_src = sum(1 for r in jr if r[7].strip().startswith('不适用'))
print('不适用 by 处置 = %d/%d = %.1f%% | by 来源现状 = %d/%d = %.1f%%'
      % (na_disp, len(jr), 100 * na_disp / len(jr), na_src, len(jr), 100 * na_src / len(jr)))
grp = collections.defaultdict(set)
for r in jr:
    grp[r[8].strip()[:60]].add(r[0].strip())
big = {k: v for k, v in grp.items() if len(v) > 1}
print('出处锚分组: 组数 %d（其中多符号组 %d）；多符号组平均覆盖符号数 %.2f，最大 %d'
      % (len(grp), len(big), sum(len(v) for v in big.values()) / max(1, len(big)),
         max((len(v) for v in big.values()), default=0)))
print('每符号位点数: 行/符号 = %.2f' % (len(jr) / len({r[0].strip() for r in jr})))

print('\n== mechanical four-bucket partition (universe = 20,595 distinct (symbol,site)) ==')
main = rd(RAW + r'\AUD-402-常数台账.csv')
judged_sites = {(r[0].strip(), r[1].strip()) for r in jr}
TEST = re.compile(r'^eng/tests/|^实验/|/tests?/|_test\.|test_|/oracle/')
b = collections.Counter()
for r in main:
    s, loc = r[0].strip(), r[1] or ''
    if TEST.search(loc):
        b['测试钉值'] += 1
    elif s.startswith('(无名)') or s == '':
        b['无名行内（生产）'] += 1
    elif (s, loc) in judged_sites or s in {x[0] for x in judged_sites}:
        b['具名已判'] += 1
    else:
        b['具名未判（生产）'] += 1
print(dict(b), 'sum', sum(b.values()))
# alternative cut: by symbol shape only
alt = collections.Counter()
for r in main:
    s = r[0].strip()
    alt['无名行内' if (s.startswith('(无名)') or s == '') else '具名'] += 1
print('按符号形态二切:', dict(alt))
prod = [r for r in main if not TEST.search(r[1] or '')]
print('生产面行（非测试/实验路径）=', len(prod), '其中具名=',
      sum(1 for r in prod if not (r[0].strip().startswith('(无名)') or r[0].strip() == '')))

print('\n== master post-patch ==')
m = [r for r in list(csv.reader(io.open(M, encoding='utf-8-sig', newline='')))[1:] if len(r) > 1]
print('rows', len(m), '| 处置', dict(collections.Counter(r[10] for r in m)))
print('冲突标记非空', sum(1 for r in m if r[13].strip()))
print('待确认', sum(1 for r in m if r[10] == '待确认'),
      '| 其中标【缺输入】', sum(1 for r in m if r[10] == '待确认' and '【缺输入' in r[9]))
print('第②层改判', sum(1 for r in m if '改判' in r[12]),
      '| 第②层独立复核', sum(1 for r in m if '独立复核' in r[12]))
print('预筛列非空', sum(1 for r in m if r[15].strip()))
print('补侧标注行', sum(1 for r in m if '（补侧' in r[2]))

print('\n== three high-value lists (master-side counts) ==')
MULTI = re.compile(r'两侧|三侧|四侧|五侧|六侧|多侧|两制|相反|不一致|对面|同值.*写|重复')
rows_multi = [r for r in m if r[13].strip() and MULTI.search(r[13])]
print('多侧取值不一致(冲突标记含多侧词) =', len(rows_multi))
opp = [r for r in m if r[13].strip() and re.search(r'对面|兜底.*相反|生产.*取.{0,6}1\.0|缺键兜底', r[13])]
print('  其中"生产兜底站在裁决对面" =', len(opp), [r[0][:26] for r in opp[:8]])
FORMULA = re.compile(r'不应写死|应按式|按公式|可导出|导出式|按.*导出|合并为单键')
rows_f = [r for r in m if FORMULA.search(' '.join([r[9], r[13], r[10]])) or
          (r[10] == '公式导出' and re.search(r'硬编码|写死', r[13] + r[9]))]
print('该按公式却硬编码(仓内已有导出式) =', len(rows_f), [r[0][:26] for r in rows_f[:8]])
NOSRC = re.compile(r'^无出处|^无来源|无（结构性）|冒充|默认值.*测|字面量.*冒充')
rows_n = [r for r in m if NOSRC.search(r[8]) or NOSRC.search(r[9])]
print('阈值/默认值缺来源 =', len(rows_n), '其中"默认字面量冒充测量值"族 =',
      sum(1 for r in rows_n if re.search(r'冒充|默认值.*当.*测|哨兵.*现行值|现行值.*哨兵', r[9] + r[13])))
