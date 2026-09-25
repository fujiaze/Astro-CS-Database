"""导出打回报告的附件清单：无支撑常数（待确认）＋ 多侧默认值分叉 ＋ 有支撑但锚不可核。"""
import io
import csv
import sys
import re
from pathlib import Path

sys.stdout.reconfigure(encoding='utf-8')
BASE = Path(r'产出/')
MASTER = BASE / '独立审计包' / '02_科学' / '常数公式算法总台账.csv'
ATT = BASE / '独立审计包' / '06_实施' / '附件'
ATT.mkdir(parents=True, exist_ok=True)

rows = list(csv.DictReader(io.open(MASTER, encoding='utf-8-sig', newline='')))
print('主表行 %d' % len(rows))


def prod(loc):
    return not (('eng/tests' in loc) or ('实验/' in loc) or ('/tests/' in loc) or ('oracle' in loc))


COL = dict(sym='符号或键', cat='类别', loc='位置集合（全部侧，规范化全路径:行）', val='现行值',
           unit='单位', coord='坐标系或归一化', prec='精度要求', dom='有效有限域',
           src='来源现状', prov='出处', disp='处置', scope='适用域', chk='是否已复核',
           conflict='冲突标记', origin='来源成稿与条目')

out1 = []
for r in rows:
    d = (r[COL['disp']] or '').strip()
    if d != '待确认':
        continue
    out1.append({
        '编号': '', '符号或键': r[COL['sym']], '类别': r[COL['cat']],
        '现行值': r[COL['val']], '单位': r[COL['unit']],
        '位点(面)': '生产' if prod(r[COL['loc']]) else '测试/实验',
        '位点': r[COL['loc']][:400], '来源现状': r[COL['src']],
        '它声称的出处': r[COL['prov']], '缺什么支撑': r[COL['scope']],
        '冲突标记': r[COL['conflict']], '第②层已复核': r[COL['chk']],
        '取证分片': r[COL['origin']],
    })
for i, r in enumerate(out1, 1):
    r['编号'] = 'A1-%03d' % i

with io.open(ATT / 'A1_无支撑常数清单.csv', 'w', encoding='utf-8-sig', newline='') as f:
    w = csv.DictWriter(f, fieldnames=list(out1[0].keys()))
    w.writeheader()
    for r in out1:
        w.writerow(r)

prod_n = sum(1 for r in out1 if r['位点(面)'] == '生产')
confl = [r for r in out1 if r['冲突标记'].strip() not in ('', '无', '—', '-')]
print('待确认 %d 条（其中生产面 %d）；带冲突标记 %d 条' % (len(out1), prod_n, len(confl)))

# 有支撑但出处锚不可核：出处指向 docs/ 或 实验/ 却写了行号，抽查行号是否真的含该值
susp = []
for r in rows:
    prov = (r[COL['prov']] or '')
    m = re.findall(r'([A-Za-z0-9_./\-\u4e00-\u9fff]+\.(?:md|json|csv|cpp|h|py)):(\d+)', prov)
    if not m:
        continue
    susp.append((r[COL['sym']], prov[:120], m))
print('出处里带"路径:行"锚的条目 %d 条（需逐条验锚）' % len(susp))

with io.open(ATT / 'A2_出处锚待验清单.csv', 'w', encoding='utf-8-sig', newline='') as f:
    w = csv.writer(f)
    w.writerow(['编号', '符号或键', '出处原文', '解析出的锚数', '锚样例'])
    for i, (s, p, m) in enumerate(susp, 1):
        w.writerow(['A2-%03d' % i, s, p, len(m), '; '.join('%s:%s' % x for x in m[:3])])

# 有支撑（文献/公式/标定）的 108 条，作为"可对外主张"的白名单
ok = []
for r in rows:
    d = (r[COL['disp']] or '').strip()
    if d in ('文献值', '公式导出', '实验标定'):
        ok.append({'编号': '', '符号或键': r[COL['sym']], '类别': r[COL['cat']], '现行值': r[COL['val']],
                   '支撑类型': d, '出处': r[COL['prov']][:200], '适用域': r[COL['scope']][:160],
                   '位点(面)': '生产' if prod(r[COL['loc']]) else '测试/实验',
                   '第②层已复核': r[COL['chk']], '取证分片': r[COL['origin']]})
for i, r in enumerate(ok, 1):
    r['编号'] = 'OK-%03d' % i
with io.open(ATT / 'A3_已有支撑白名单.csv', 'w', encoding='utf-8-sig', newline='') as f:
    w = csv.DictWriter(f, fieldnames=list(ok[0].keys()))
    w.writeheader()
    for r in ok:
        w.writerow(r)
cnt = {}
for r in ok:
    cnt[r['支撑类型']] = cnt.get(r['支撑类型'], 0) + 1
print('有支撑白名单 %d 条：%s' % (len(ok), cnt))
notapp = sum(1 for r in rows if (r[COL['disp']] or '').strip() == '不适用')
print('判为结构性不适用 %d 条；空处置 %d 条' % (notapp, sum(1 for r in rows if not (r[COL['disp']] or '').strip())))
