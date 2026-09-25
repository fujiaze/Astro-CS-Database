import io
import sys
import csv
from pathlib import Path

sys.stdout.reconfigure(encoding='utf-8')
SRC = Path(r'独立审计/05_门禁\门禁清单.csv')
rows = list(csv.DictReader(io.open(SRC, encoding='utf-8-sig', newline='')))
print('行 %d' % len(rows))
for r in rows:
    if '/' in r['名称'] or '.py' in r['名称']:
        print('=== %s' % r['原名称（逐字留存）'])
        for k in ['处置', '关联既有 ID', '检验对象（真实对象）', '命令', '判据', '层次与档位', '对应文档条款', '落地成本档']:
            print('    %-14s %s' % (k, (r.get(k) or '')[:150].replace('\n', ' ')))
print()
print('=== BATCH 行 ===')
for r in rows:
    if r['名称'].startswith('BATCH'):
        print('%s | 处置=%s | 对象=%s | 命令=%s' % (r['名称'], r['处置'], (r['检验对象（真实对象）'] or '')[:70], (r['命令'] or '')[:70]))
print()
print('=== 处置 × 落地成本档 交叉 ===')
tab = {}
for r in rows:
    tab.setdefault(r['处置'], {}).setdefault((r['落地成本档'] or '').strip() or '(空)', 0)
    tab[r['处置']][(r['落地成本档'] or '').strip() or '(空)'] += 1
for k, v in sorted(tab.items()):
    print('   %-16s %s' % (k, v))
print()
print('=== 对应文档条款 覆盖率 ===')
filled = sum(1 for r in rows if (r['对应文档条款'] or '').strip())
print('   填 %d / 空 %d（合计 %d）' % (filled, len(rows) - filled, len(rows)))
empty_ids = [r['名称'] for r in rows if not (r['对应文档条款'] or '').strip()]
print('   空者：%s' % ', '.join(empty_ids[:12]))
print()
print('=== 负例列非空但无"注入/必红"字样的行（判据可红性存疑）===')
n = 0
for r in rows:
    neg = (r['负例（注入什么必红）'] or '')
    if neg.strip() and not any(k in neg for k in ('注入', '必红', '改红', '判红', 'rc')):
        n += 1
        print('   %s: %s' % (r['名称'], neg[:90].replace('\n', ' ')))
print('   合计 %d' % n)
