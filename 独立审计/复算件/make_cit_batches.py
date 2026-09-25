"""从共享文献台账切出未核批次（不写回台账）。"""
import sys
import io
import csv
from pathlib import Path

sys.stdout.reconfigure(encoding='utf-8')
LED = Path(r'F:\Astro dev\独立审查\整改\out\文献台账.csv')
INV = Path(r'独立审计/批次清单')

rows = list(csv.DictReader(io.open(LED, encoding='utf-8-sig', newline='')))
todo = [r for r in rows if not (r.get('存在性依据') or '').strip()]
p1 = [r for r in todo if (r.get('优先级') or '').strip().upper().startswith('P-1')]
rest = [r for r in todo if r not in p1]


def key(r):
    try:
        return -int(r.get('命中次数') or 0)
    except Exception:
        return 0


p1.sort(key=key)
rest.sort(key=key)
sel = (p1 + rest)[:108]
print('台账 %d 行｜未核 %d｜其中 P-1 未核 %d｜本轮派 %d 条（P-1 优先，按命中次数降序）'
      % (len(rows), len(todo), len(p1), len(sel)))
K = 6
for i in range(K):
    part = sel[i::K]
    lines = ['# 批次 CIT-%02d｜%d 条未核引用（P-1 优先）｜判据见 独立审计/派单规程/DISPATCH-CIT-PREAMBLE.md' % (i + 1, len(part)),
             '# 列：序号\t优先级\t命中次数\t引用件\t类型\t代表位点(断言所在处)']
    for r in part:
        lines.append('\t'.join([r['序号'], r.get('优先级', ''), r.get('命中次数', ''),
                                (r.get('引用件') or '').replace('\n', ' ')[:150],
                                (r.get('类型') or '')[:20], (r.get('代表位点') or '')[:150]]))
    (INV / ('CIT-%02d.txt' % (i + 1))).write_text('\n'.join(lines) + '\n', encoding='utf-8')
    print('  CIT-%02d %d 条，首条序号 %s' % (i + 1, len(part), part[0]['序号'] if part else '—'))
