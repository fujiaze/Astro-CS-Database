"""Cut CIT-07.. batches from the shared ledger: works still lacking an existence basis,
excluding the ones already assigned to CIT-01..06. Read-only on the ledger."""
import sys, csv, io
from pathlib import Path
sys.stdout.reconfigure(encoding='utf-8')

LED = Path(r'F:\Astro dev\独立审查\整改\out\文献台账.csv')
INV = Path(r'独立审计/批次清单')

rows = list(csv.DictReader(io.open(LED, encoding='utf-8-sig', newline='')))
verified = {r['序号'] for r in rows if (r.get('存在性依据') or '').strip()}

already = set()
for p in sorted(INV.glob('CIT-0[1-6].txt')):
    for line in io.open(p, encoding='utf-8'):
        if line.startswith('#'):
            continue
        already.add(line.split('\t')[0].strip())

todo = [r for r in rows if r['序号'] not in verified and r['序号'] not in already]


def hits(r):
    try:
        return -int(r.get('命中次数') or 0)
    except Exception:
        return 0


prio = [r for r in todo if (r.get('优先级') or '').strip().upper().startswith('P-1')]
rest = [r for r in todo if r not in prio]
prio.sort(key=hits)
rest.sort(key=hits)

K = 6
sel = (prio + rest)[:K * 14]
print('台账 %d 行｜已核 %d｜此前已派 %d｜本轮未核池 %d（P-1 %d）｜本轮派 %d 条'
      % (len(rows), len(verified), len(already), len(todo), len(prio), len(sel)))
for i in range(K):
    part = sel[i::K]
    lines = ['# 批次 CIT-%02d｜%d 条未核引用（P-1 优先，按命中次数降序）｜判据见 独立审计/派单规程/DISPATCH-CIT-PREAMBLE.md'
             % (i + 7, len(part)),
             '# 列：序号\t优先级\t命中次数\t引用件\t类型\t代表位点(断言所在处)']
    for r in part:
        lines.append('\t'.join([r['序号'], r.get('优先级', ''), r.get('命中次数', ''),
                                (r.get('引用件') or '').replace('\n', ' ')[:170],
                                (r.get('类型') or '')[:20], (r.get('代表位点') or '')[:170]]))
    p = INV / ('CIT-%02d.txt' % (i + 7))
    p.write_text('\n'.join(lines) + '\n', encoding='utf-8')
    print('  %s %d 条' % (p.name, len(part)))
