"""生产具名常数的机械卷宗：每个符号一行，先能机判的都机判，剩下的才是派工残余。"""
import sys
import io
import csv
import re
import subprocess
from collections import defaultdict
from pathlib import Path

sys.stdout.reconfigure(encoding='utf-8')
BASE = Path(r'产出/')
REPO = Path(r'F:\Astro dev\Astro CS Normalization Database')
ATT = BASE / '独立审计包' / '06_实施' / '附件'
ATT.mkdir(exist_ok=True)

rows = list(csv.reader(io.open(BASE / 'raw' / 'AUD-402-常数台账.csv', encoding='utf-8-sig', newline='')))
body = [r for r in rows[1:] if len(r) > 9]
SY, LO, VA, UN, SR, DI = 0, 1, 2, 3, 7, 9

tracked = subprocess.run(['git', '-c', 'core.quotePath=false', '-C', str(REPO), 'ls-files'],
                         capture_output=True, text=True, encoding='utf-8', errors='replace').stdout.splitlines()
DOC_TXT = {}
for f in tracked:
    if f.startswith(('docs/', 'eng/contracts/', 'eng/packaging/', 'ASTROCS_DESIGN', 'ACCEPTANCE_SPEC', 'ENGINEERING_SPEC')) \
            and f.endswith(('.md', '.json', '.yaml', '.csv')):
        try:
            DOC_TXT[f] = (REPO / f).read_text(encoding='utf-8', errors='replace')
        except Exception:
            pass
print('登记/文档面文本 %d 份入内存' % len(DOC_TXT))

sites = defaultdict(list)
for r in body:
    loc = r[LO]
    if any(x in loc for x in ('eng/tests/', '实验/', '/tests/', '/oracle/', '/fixtures/',
                             'docs/', 'artifacts/', '工程控制/', 'run/')):
        continue
    s = (r[SY] or '').strip()
    if s in ('', '(无名)'):
        continue
    sites[s].append(r)

NUMSHAPE = [(re.compile(r'^(0|1|2|4|8|16|32|64|128|256|512|1024|4096|1e[+-]?\d+|\d+e[+-]\d+)$'), '整数/整幂/科学计数'),
            (re.compile(r'^0?\.\d+$'), '小于 1 的小数'),
            (re.compile(r'^-?\d+(\.\d+)?$'), '一般数值'),
            (re.compile(r'"[^"]*"'), '字符串字面量')]


def shape(v):
    v = (v or '').strip()
    for pat, name in NUMSHAPE:
        if pat.match(v):
            return name
    return '其他'


out = []
for s, rs in sorted(sites.items()):
    if (rs[0][DI] or '').strip() in ('文献值', '公式导出', '实验标定'):
        pre = '已有支撑（本轮不派）'
    elif len(s) < 4 and not re.search(r'tol|eps|min|max|zp|snr', s, re.I):
        pre = '局部量噪声（不入科学判读）'
    else:
        pre = ''
    files = sorted({x[LO].split(':')[0] for x in rs})
    named_in = []
    for f, txt in DOC_TXT.items():
        if re.search(r'\b%s\b' % re.escape(s), txt):
            named_in.append(f)
    has_unit = any((x[UN] or '').strip() for x in rs)
    sem = bool(re.search(r'(tol|thresh|eps|sigma|min|max|scale|power|frac|factor|rate|limit|window|lambda|'
                         r'alpha|weight|snr|zp|zeropoint|gain|bias|dark|flat|sky|fwhm|radius|area|angle|'
                         r'nside|order|budget|clip|reject|support)', s, re.I))
    if not pre:
        if named_in:
            pre = '残余A·登记面点名但无支撑'
        elif sem and has_unit:
            pre = '残余B·带单位的科学量'
        elif sem:
            pre = '残余C·名称含科学/阈值语素'
        else:
            pre = '残余D·语义未明（需读用途）'
    out.append({
        '符号': s, '预分档': pre, '位点数': len(rs), '跨文件数': len(files),
        '值样例': ' | '.join(sorted({(x[VA] or '').strip() for x in rs if (x[VA] or '').strip()})[:4])[:60],
        '值形态': shape(rs[0][VA]), '有单位': '是' if has_unit else '否',
        '登记面点名数': len(named_in), '点名位置样例': ' ; '.join(named_in[:3])[:170],
        '来源现状样例': (rs[0][SR] or '')[:60],
        '位点样例': ' ; '.join('%s' % x[LO] for x in rs[:4])[:200],
    })

COLS = list(out[0].keys())
with io.open(ATT / 'A4_生产具名常数机械卷宗.csv', 'w', encoding='utf-8-sig', newline='') as f:
    w = csv.DictWriter(f, fieldnames=COLS)
    w.writeheader()
    for r in out:
        w.writerow(r)
cnt = defaultdict(lambda: defaultdict(int))
for r in out:
    cnt[r['预分档']]['符号'] += 1
    cnt[r['预分档']]['位点'] += int(r['位点数'])
print('生产具名符号 %d 个，机判预分档：' % len(out))
for k, v in sorted(cnt.items(), key=lambda x: -x[1]['符号']):
    print('   %-28s 符号 %4d / 位点 %5d' % (k, v['符号'], v['位点']))
need = [r for r in out if r['预分档'].startswith('残余')]
print('需判读残余 = %d 符号 / %d 位点' % (len(need), sum(int(r['位点数']) for r in need)))
INVDIR = BASE / 'inventory'
K = 6
order = sorted(need, key=lambda r: (-r['登记面点名数'], -int(r['位点数']), r['符号']))
for i in range(K):
    part = order[i::K]
    lines = ['# 批次 C-%02d｜待判残余符号 %d 个（生产面·具名·无支撑）' % (i + 1, len(part)),
             '# 列：符号\t预分档\t位点数\t值样例\t值形态\t有单位\t登记面点名数\t点名位置样例\t位点样例']
    for r in part:
        lines.append('\t'.join([r['符号'], r['预分档'], r['位点数'], r['值样例'], r['值形态'], r['有单位'],
                               str(r['登记面点名数']), r['点名位置样例'][:90], r['位点样例'][:120]]))
    (INVDIR / ('C-%02d.txt' % (i + 1))).write_text('\n'.join(lines) + '\n', encoding='utf-8')
    print('  C-%02d 符号 %d（最大位点数 %s）' % (i + 1, len(part), part[0]['位点数'] if part else 0))
