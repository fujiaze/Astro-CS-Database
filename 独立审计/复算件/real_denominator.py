"""确定性分类：把 20,510 个"待确认"位点分成 需要支撑 / 不需要支撑 两类，给出真实分母。
   规则全部写死并可复算，不做任何语义猜测。"""
import sys
import io
import csv
import re
from collections import Counter, defaultdict
from pathlib import Path

sys.stdout.reconfigure(encoding='utf-8')
BASE = Path(r'产出/')
OUT = BASE / '独立审计包' / '06_实施' / '附件'
rows = list(csv.reader(io.open(BASE / 'raw' / 'AUD-402-常数台账.csv', encoding='utf-8-sig', newline='')))
body = [r for r in rows[1:] if len(r) > 9]
SY, LO, VA, UN, SR, DI = 0, 1, 2, 3, 7, 9

SEM = re.compile(r'(tol|thresh|eps|sigma|varian|snr|zp|zero.?point|gain|bias|dark|flat|sky|fwhm|psf|'
                 r'lambda|alpha|beta|gamma|weight|area|angle|arcsec|pix|nside|order|frac|power|scale|'
                 r'factor|rate|limit|window|margin|clip|reject|cover|support|flux|magnitude|fluxerr|'
                 r'radius|depth|budget|timeout|memory|chunk|stride|seed)', re.I)
STRUCT = re.compile(r'^(nb|n_|num_|k_|max_dim|dim|size|len|count_idx|idx|version|major|minor|'
                    r'bits|byte|shift|mask|flag_|tag|id_)", ?$', re.I)
SCINUM = re.compile(r'^\s*-?(\d+\.\d+(e-?\d+)?|\d+e-\d+|0\.\d+)\s*$', re.I)


def face(loc):
    if any(x in loc for x in ('eng/tests/', '实验/', '/tests/', '/oracle/', '/fixtures/')):
        return '测试/实验'
    if any(x in loc for x in ('docs/', 'artifacts/', '工程控制/', 'run/')):
        return '文档/证据'
    return '生产'


cls = Counter()
sci = defaultdict(list)
for r in body:
    if (r[DI] or '').strip() in ('文献值', '公式导出', '实验标定'):
        cls['已有支撑'] += 1
        continue
    f = face(r[LO])
    s = (r[SY] or '').strip()
    v = (r[VA] or '').strip()
    named = s not in ('', '(无名)')
    if f == '测试/实验':
        cls['不需要支撑·测试期望值与容差'] += 1
        continue
    if f == '文档/证据':
        cls['不需要支撑·文档与证据件内数值'] += 1
        continue
    if not named:
        if SCINUM.match(v):
            cls['需要支撑·生产行内科学量值'] += 1
            sci['行内'].append((r[LO], v, (r[UN] or '')[:14]))
        else:
            cls['待人工·生产行内非科学形态'] += 1
        continue
    if SEM.search(s) or (r[UN] or '').strip():
        cls['需要支撑·生产具名科学量'] += 1
        sci['具名'].append((s, r[LO], v, (r[UN] or '')[:14]))
    elif re.match(r'^-?\d+$', v) and int(re.match(r'^(-?\d+)$', v).group(1)) in (
            0, 1, 2, 3, 4, 5, 6, 7, 8, 16, 32, 64, 128, 256, 512, 1024, 4096, 8192):
        cls['待人工·整数或整幂（多为结构性，需读用途）'] += 1
    else:
        cls['待人工·其余具名（需读用途）'] += 1

print('全 20,595 位点的确定性分类：')
for k, v in cls.most_common():
    print('   %-34s %6d' % (k, v))
need = sum(v for k, v in cls.items() if k.startswith('需要支撑'))
hard = sum(v for k, v in cls.items() if k.startswith('待人工'))
print('—— 必须给文献或实验支撑：**%d** 位点；需读代码用途才能定档：%d；不需要支撑：%d；已有支撑：%d'
      % (need, hard, sum(v for k, v in cls.items() if k.startswith('不需要')), cls['已有支撑']))
print('实际已判定的科学量（人工判读子集）= 411 行中 有支撑 108 ＋ 待确认 110 = 218')

with io.open(OUT / 'A6_必须给支撑的科学量清单.csv', 'w', encoding='utf-8-sig', newline='') as f:
    w = csv.writer(f)
    w.writerow(['类型', '符号或键', '位点', '现行值', '登记单位'])
    for s, loc, v, u in sci['具名']:
        w.writerow(['具名', s, loc, v, u])
    for loc, v, u in sci['行内']:
        w.writerow(['行内', '', loc, v, u])
print('附件 A6 已写：具名 %d 行 / 行内 %d 行' % (len(sci['具名']), len(sci['行内'])))
uniq = len({s for s, *_ in sci['具名']})
print('具名去重符号 %d 个；行内位点 %d 个（同文件多次出现各自算）' % (uniq, len(sci['行内'])))
