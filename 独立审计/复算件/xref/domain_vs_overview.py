import io
import re
import sys
import json
import subprocess
from pathlib import Path

sys.stdout.reconfigure(encoding='utf-8')
BASE = Path(r'产出/')
rc = subprocess.run([sys.executable, str(BASE / '复算' / 'xref' / 'domain_audit.py')],
                    capture_output=True, text=True, encoding='utf-8')
mine = json.load((BASE / '复算' / 'xref' / 'dup_objects.json').open(encoding='utf-8'))

txt = (BASE / '独立审计包' / '06_实施' / '实施任务总览与依赖图.md').read_text(encoding='utf-8')
m = re.search(r'^##\s*5[^\n]*\n(.*?)(?=^##\s|\Z)', txt, re.S | re.M)
block = m.group(1)
theirs = {}
for line in block.splitlines():
    if not line.strip().startswith('|'):
        continue
    cells = [c.strip().strip('`*') for c in line.strip().strip('|').split('|')]
    if len(cells) < 2 or not cells[0] or set(cells[0]) <= set('-: '):
        continue
    if cells[0].startswith(('枢纽对象', '相交对象', '路径')):
        continue
    n = None
    for c in cells[1:3]:
        if re.fullmatch(r'\d+', c):
            n = int(c)
            break
    theirs[cells[0]] = (n, len(cells[1:]) - 1)
print('总览 §5 表体行数（=登记对象数）：%d；自述"相交对象 57 / 相交对 155"' % len(theirs))
print('我独立复算：相交对象 %d，相交对 %d' % (len(mine), sum(len(v) * (len(v) - 1) // 2 for v in mine.values())))
only_mine = sorted(set(mine) - set(theirs))
only_theirs = sorted(set(theirs) - set(mine))
print('=== 只有我复算出来有（总览漏登）：%d 个 ===' % len(only_mine))
for p in only_mine:
    print('   %-58s %d 任务：%s' % (p[:58], len(mine[p]), '、'.join(x.split('_')[0] for x in mine[p])))
print('=== 只在总览表里（我复算判为不相交）：%d 个 ===' % len(only_theirs))
for p in only_theirs:
    print('   %-58s 总览记 %s' % (p[:58], theirs[p]))
bad = []
for p in sorted(set(mine) & set(theirs)):
    if theirs[p][0] != len(mine[p]):
        bad.append((p, theirs[p][0], len(mine[p])))
print('=== 两边都有、但任务数不一致：%d 个 ===' % len(bad))
for p, a, b in bad:
    print('   %-58s 总览 %s vs 复算 %d' % (p[:58], a, b))
h = [line for line in block.splitlines() if '两两取' in line or '相交' in line]
print('=== 总览自述原句 ===')
for line in h[:4]:
    print('   ' + line.strip()[:200])
