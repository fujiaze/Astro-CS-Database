"""四步判据机判：对 7,613 个"必须给支撑"位点逐条跑规则，把需要人读的残余压到最小。
   步骤：① 条文点名该符号且同条含该值 → ② 条文点名该符号（值由条文给）→ ③ 可导出（代码里该值由输入算出）→ ④ 无支撑。
   规则全部写死、可复算；任何一步不确定都不许跳到"有支撑"。"""
import sys
import io
import csv
import re
import subprocess
from collections import Counter, defaultdict
from pathlib import Path

sys.stdout.reconfigure(encoding='utf-8')
BASE = Path(r'产出/')
REPO = Path(r'F:\Astro dev\Astro CS Normalization Database')
ATT = BASE / '独立审计包' / '06_实施' / '附件'

src = list(csv.DictReader(io.open(ATT / 'A6_必须给支撑的科学量清单.csv', encoding='utf-8-sig', newline='')))
print('待判位点 %d' % len(src))

# 只把"科学条文面"当出处来源：docs/science, docs/algorithms, docs/contracts, docs/design, 根规范
DOCF = []
for f in subprocess.run(['git', '-c', 'core.quotePath=false', '-C', str(REPO), 'ls-files', '--',
                         'docs/science', 'docs/algorithms', 'docs/contracts', 'docs/design',
                         'docs/ci', 'ASTROCS_DESIGN.md', 'ACCEPTANCE_SPEC.md', 'ENGINEERING_SPEC.md'],
                        capture_output=True, text=True, encoding='utf-8', errors='replace').stdout.splitlines():
    if f.endswith(('.md', '.csv', '.json', '.yaml')):
        try:
            DOCF.append((f, (REPO / f).read_text(encoding='utf-8', errors='replace').splitlines()))
        except Exception:
            pass
print('科学条文面文本 %d 份' % len(DOCF))

NORM = lambda s: s.lower().replace('-', '_').replace('.', '_')
# 值是否以"可算出"形态出现：其右侧是表达式而非字面量
EXPR = re.compile(r'=\s*[^;]*[\*/+\-]\s*\w|\bdouble\b[^;]*[\*/]\s*\w|\bsizeof\b|\b/\s*\(? *(?:4|8|12|3|2)\)?')

# 倒排索引：词 → [(文件, 行号, 行原文)]，避免 2,592 符号 × 全部行 的正则扫
WORD = re.compile(r'[A-Za-z_][A-Za-z0-9_]{3,}')
IDX = {}
for f, lines in DOCF:
    for i, l in enumerate(lines, 1):
        for w in set(WORD.findall(l)):
            e = IDX.setdefault(w.lower(), [])
            if len(e) < 400:
                e.append((f, i, l.strip()[:120]))
print('倒排索引词数 %d' % len(IDX))


def find_in_docs(sym):
    return IDX.get(sym.lower(), [])[:6]


res = Counter()
rows_out = []
for r in src:
    sym = (r['符号或键'] or '').strip()
    val = (r['现行值'] or '').strip()
    if not sym:
        res['④位点·行内值需人工'] += 1
        rows_out.append([r['类型'], '', r['位点'], val, r['登记单位'], '④-待人工·行内值', ''])
        continue
    hits = find_in_docs(sym)
    if not hits:
        res['③符号未见于科学条文面·待判是否可导出'] += 1
        rows_out.append([r['类型'], sym, r['位点'], val, r['登记单位'], '③未见于条文面', ''])
        continue
    withval = [h for h in hits if val and val.replace('.', r'\.') and re.search(re.escape(val), h[2])]
    if withval:
        res['①条文同条含该值（须验是否真为定义处）'] += 1
        rows_out.append([r['类型'], sym, r['位点'], val, r['登记单位'], '①条文含值',
                         '%s:%d %s' % (withval[0][0], withval[0][1], withval[0][2][:70])])
    else:
        res['②条文点名该符号但该行无该值'] += 1
        rows_out.append([r['类型'], sym, r['位点'], val, r['登记单位'], '②条文有名无值',
                         '%s:%d %s' % (hits[0][0], hits[0][1], hits[0][2][:70])])

with io.open(ATT / 'A7_科学量四步机判结果.csv', 'w', encoding='utf-8-sig', newline='') as f:
    w = csv.writer(f)
    w.writerow(['类型', '符号或键', '位点', '现行值', '登记单位', '机判档', '条文命中'])
    for x in rows_out:
        w.writerow(x)

print('=== 机判档分布 ===')
for k, v in res.most_common():
    print('   %-38s %6d' % (k, v))
n1 = res['①条文同条含该值（须验是否真为定义处）']
n2 = res['②条文点名该符号但该行无该值']
n3 = res['③符号未见于科学条文面·待判是否可导出']
n4 = res['④位点·行内值需人工']
print('—— 机器就能定档（①＋②有条文凭据）%d；条文面完全没提到 %d；行内值待读 %d' % (n1 + n2, n3, n4))
print('附件 A7 已写 %d 行' % len(rows_out))
