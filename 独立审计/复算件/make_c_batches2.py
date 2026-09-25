"""生产面具名符号 → 语义筛选 + 风险排序，产出可派工批次。"""
import sys
import io
import csv
import re
from collections import Counter, defaultdict
from pathlib import Path

sys.stdout.reconfigure(encoding='utf-8')
BASE = Path(r'产出/')
INV = BASE / 'inventory'
REPO = Path(r'F:\Astro dev\Astro CS Normalization Database')
rows = list(csv.reader(io.open(BASE / 'raw' / 'AUD-402-常数台账.csv', encoding='utf-8-sig', newline='')))
body = [r for r in rows[1:] if len(r) > 9]
SY, LO, VA, UN, SR, DI = 0, 1, 2, 3, 7, 9

SEM = re.compile(r'(tol|thresh|eps|epsilon|sigma|min|max|scale|power|frac|factor|rate|limit|window|'
                 r'lambda|alpha|beta|gamma|weight|snr|zp|zeropoint|gain|bias|dark|flat|sky|fwhm|'
                 r'radius|area|angle|arcsec|pix|nside|order|seed|count|size|depth|thresh|budget|'
                 r'memory|timeout|retry|chunk|stride|pad|margin|clip|reject|cover|support)', re.I)
REGKEYS = set()
for f in ['eng/packaging/config/defaults.json', 'eng/packaging/config/config_registry.json',
          'docs/contracts/CONFIG_CONTRACT.md', 'eng/contracts/resource_gate_v1.json']:
    p = REPO / f
    if p.exists():
        txt = p.read_text(encoding='utf-8', errors='replace')
        REGKEYS |= set(re.findall(r'"([A-Za-z][A-Za-z0-9_.]{2,40})"\s*:', txt))

sites = defaultdict(list)
for r in body:
    loc = r[LO]
    if any(x in loc for x in ('eng/tests/', '实验/', '/tests/', '/oracle/', '/fixtures/')):
        continue
    if any(x in loc for x in ('docs/', 'artifacts/', '工程控制/', 'run/')):
        continue
    s = (r[SY] or '').strip()
    if s in ('', '(无名)'):
        continue
    sites[s].append(r)

cand = {}
for s, rs in sites.items():
    if (rs[0][DI] or '').strip() in ('文献值', '公式导出', '实验标定'):
        continue
    score = 0
    why = []
    if len(s) < 4 and not SEM.search(s):
        continue                                   # 单字母/双字母局部量，非科学语义
    if SEM.search(s):
        score += 3
        why.append('名含科学/阈值语素')
    if s in REGKEYS or s.split('.')[-1] in REGKEYS:
        score += 5
        why.append('出现在 defaults/登记册/合同/资源契约')
    files = {x[LO].split(':')[0] for x in rs}
    if len(files) >= 3:
        score += 2
        why.append('跨 %d 文件共享' % len(files))
    if any(x for x in rs if re.match(r'^\s*(eng/packaging|eng/contracts|lib/include)', x[LO])):
        score += 2
        why.append('位点在配置或公共头')
    if any(x for x in rs if (x[UN] or '').strip()):
        score += 1
        why.append('登记了单位（即被判为物理量）')
    if score >= 4:
        cand[s] = (score, why, rs)

print('生产面具名待判符号 %d → 语义筛选后 %d 个（阈值 score>=4）' % (len(sites), len(cand)))
rank = sorted(cand, key=lambda s: (-cand[s][0], -len(cand[s][2]), s))
print('其中高分（>=7）%d 个、中（5-6）%d、低（4）%d' %
      (sum(1 for s in rank if cand[s][0] >= 7), sum(1 for s in rank if 5 <= cand[s][0] <= 6),
       sum(1 for s in rank if cand[s][0] == 4)))
K = 8
for i in range(K):
    part = rank[i::K]
    lines = ['# 批次 C-%02d｜生产面·具名·语义筛选后待判符号 %d 个（源自 2,592 个待判具名符号，筛掉局部量噪声）'
             % (i + 1, len(part)),
             '# 每行：符号\t风险分\t入选理由\t位点数\t位点样例(路径:行|现行值|单位|来源现状)',
             '# 判据见 DISPATCH-C-PREAMBLE.md；产出必须逐符号给 §4 四选一定档']
    for s in part:
        sc, why, rs = cand[s]
        ex = ' ;; '.join('%s|%s|%s|%s' % (x[LO], x[VA], x[UN], (x[SR] or '')[:34]) for x in rs[:3])
        lines.append('%s\t%d\t%s\t%d\t%s' % (s, sc, '、'.join(why), len(rs), ex))
    (INV / ('C-%02d.txt' % (i + 1))).write_text('\n'.join(lines) + '\n', encoding='utf-8')
print('已写 inventory/C-01..C-08.txt')
for i in range(K):
    t = (INV / ('C-%02d.txt' % (i + 1))).read_text(encoding='utf-8').splitlines()
    print('  C-%02d 行数 %d，首符号 %s' % (i + 1, len(t) - 3, t[3].split('\t')[0] if len(t) > 3 else '空'))
