"""把 ③（条文面零提及）与 ④（行内值）再机判一刀：读赋值行，分"字面量硬编码 / 随输入算出 / 读不到行"。"""
import sys
import io
import csv
import re
from collections import Counter, defaultdict
from pathlib import Path

sys.stdout.reconfigure(encoding='utf-8')
BASE = Path(r'产出/')
REPO = Path(r'F:\Astro dev\Astro CS Normalization Database')
ATT = BASE / '独立审计包' / '06_实施' / '附件'
rows = list(csv.DictReader(io.open(ATT / 'A7_科学量四步机判结果.csv', encoding='utf-8-sig', newline='')))
todo = [r for r in rows if r['机判档'].startswith(('③', '④'))]
print('待再判 %d 行' % len(todo))

LIT = re.compile(r'^\s*-?\d+(\.\d+)?([eE][-+]?\d+)?\s*(?:[fFuUlL]{0,3}|f)?\s*[;,)]?\s*$')
EXPRR = re.compile(r'(\w+\s*[*/+-]\s*\w|\b(?:sqrt|pow|exp|log|sin|cos|abs|floor|ceil|round|min|max)\s*\(|'
                   r'>>|<<|sizeof|\bwidth\b|\bheight\b|\bnside\b|\bpix\b|\bscale\b|\barea\b|/\s*\()')
DECL = re.compile(r'(?:const|constexpr|static|#define|double|float|int|uint\d+_t|size_t)\b')

cache = {}


def line_of(site):
    path = site.split(':')[0]
    try:
        ln = int(re.search(r':(\d+)', site).group(1))
    except Exception:
        return ''
    if path not in cache:
        p = REPO / path
        if not p.exists():
            cand = list(REPO.rglob(Path(path).name))
            if not cand:
                cache[path] = None
            else:
                cache[path] = cand[0].read_text(encoding='utf-8', errors='replace').splitlines()
        else:
            cache[path] = p.read_text(encoding='utf-8', errors='replace').splitlines()
    src = cache[path]
    if not src:
        return ''
    return src[ln - 1] if 0 < ln <= len(src) else ''


cls = Counter()
out = []
for r in todo:
    txt = line_of(r['位点'])
    m = re.search(r'[:=]\s*([^=]*?)(-?\d+\.?\d*(?:[eE][-+]?\d+)?)\s*[;,)]?\s*(?://.*)?$', txt)
    val = r['现行值'].strip()
    tail = txt.split('=', 1)[1] if '=' in txt else txt
    if not txt.strip():
        k = '读不到行（位点漂移或路径不在跟踪面）'
    elif LIT.match(tail.strip()) or re.search(r'[=:,(\[]\s*%s\s*[);,]' % re.escape(val), txt):
        k = '字面量硬编码·条文面零提及' if r['机判档'].startswith('③') else '字面量硬编码·行内无名'
    elif EXPRR.search(tail):
        k = '随输入算出（可导出，但须登记导出式）'
    elif DECL.search(txt):
        k = '声明行未见值（须读初始化处）'
    else:
        k = '形态不明·需人工读用途'
    cls[k] += 1
    out.append([r['类型'], r['符号或键'], r['位点'], r['现行值'], r['机判档'], k, txt.strip()[:110]])

with io.open(ATT / 'A8_零条文支撑常数的赋值形态.csv', 'w', encoding='utf-8-sig', newline='') as f:
    w = csv.writer(f)
    w.writerow(['类型', '符号或键', '位点', '现行值', '机判档', '赋值形态', '该行原文'])
    for x in out:
        w.writerow(x)

print('=== 赋值形态分布（共 %d）===' % len(out))
for k, v in cls.most_common():
    print('   %-40s %6d' % (k, v))
hard = cls['字面量硬编码·条文面零提及'] + cls['字面量硬编码·行内无名']
print('—— 最硬的一档（字面量硬编码且条文面零提及/无名）= %d 位点：这些是"值凭谁定"完全无凭据的数' % hard)
print('附件 A8 已写 %d 行' % len(out))
