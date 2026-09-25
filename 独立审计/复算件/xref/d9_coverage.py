"""D9 覆盖自检：所有"待裁/待确认/UNRESOLVED"主张是否都收在 07_未决，其余文档不留悬而未决结论。"""
import re
import sys
from pathlib import Path

sys.stdout.reconfigure(encoding='utf-8')
BASE = Path(r'产出/')
PKG = BASE / '独立审计包'
UN = PKG / '07_未决' / 'UNRESOLVED清单.md'

un_text = UN.read_text(encoding='utf-8')
un_items = re.findall(r'裁-\s*(\d+)', un_text)
print('07_未决 行数 %d；出现的裁号引用 %s' % (len(un_text.splitlines()), sorted(set(un_items))))
pat = re.compile(r'(待你裁|待裁|需负责人裁|需裁|上呈|UNRESOLVED|待确认|待深核|待点头)')
rows = []
for f in sorted(PKG.rglob('*.md')):
    if f == UN or '07_未决' in f.parts:
        continue
    for i, l in enumerate(f.read_text(encoding='utf-8').splitlines(), 1):
        if pat.search(l):
            rows.append((f.relative_to(PKG).as_posix(), i, l.strip()))
print('07 之外含"待裁/待确认/上呈"类字样的行：%d 处' % len(rows))
byfile = {}
for rel, i, l in rows:
    byfile.setdefault(rel, []).append((i, l))
for rel, v in sorted(byfile.items(), key=lambda x: -len(x[1])):
    print('   %-52s %d 处  例 [%d] %s' % (rel[:52], len(v), v[0][0], v[0][1][:100]))
print()
print('=== 07 未决自己的节结构 ===')
for i, l in enumerate(un_text.splitlines(), 1):
    if l.startswith('#'):
        print('   %4d %s' % (i, l[:100]))
print()
m = re.findall(r'(\d+)\s*条方向裁决|(\d+)\s*条来源核实', un_text)
print('总目录 §5 自述"9 条方向裁决 ＋ 6 条来源核实"；07 内同款句式命中：%s' % (m or '无'))
sec1 = re.search(r'##\s*1[^\n]*\n(.*?)(?=\n##\s|\Z)', un_text, re.S)
if sec1:
    n = len(re.findall(r'^\s*(?:[-*]|\|\s*\d|裁-)', sec1.group(1), re.M))
    tbl = len([x for x in sec1.group(1).splitlines() if x.strip().startswith('|')])
    print('07 §1 条目行数 %d（表体行 %d）' % (n, tbl))
