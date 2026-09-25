import io
import os
import re
import csv
from pathlib import Path

PKG = Path(r'独立审计/')
GATE = PKG / '05_门禁'
out = []

for f in sorted(GATE.iterdir()):
    out.append('===== %s  (%d B) =====' % (f.name, f.stat().st_size))
    if f.suffix == '.md':
        lines = f.read_text(encoding='utf-8').splitlines()
        out.append('行数 %d，一级/二级标题：' % len(lines))
        for i, l in enumerate(lines, 1):
            if re.match(r'^#{1,3} ', l):
                out.append('  %4d  %s' % (i, l[:100]))
        out.append('  首 6 行原文：')
        out.extend('    ' + x for x in lines[:6])
    else:
        with io.open(f, encoding='utf-8-sig', newline='') as fh:
            rows = list(csv.reader(fh))
        out.append('CSV 数据行 %d，列 %d' % (len(rows) - 1, len(rows[0])))
        out.append('  列名: ' + ' | '.join(rows[0]))
        for r in rows[1:3]:
            out.append('  样例: ' + ' | '.join(x[:40] for x in r))
        counts = {}
        key = None
        for i, h in enumerate(rows[0]):
            if h.strip() in ('档位', 'profile', '族', '类别', '状态', '处置'):
                key = i
        if key is not None:
            for r in rows[1:]:
                if len(r) > key:
                    counts[r[key]] = counts.get(r[key], 0) + 1
            out.append('  分布(%s): %s' % (rows[0][key], counts))

Path(r'独立审计/复算件/xref\gate_inspect.txt').write_text(
    '\n'.join(out) + '\n', encoding='utf-8')
print('written lines=%d' % len(out))
