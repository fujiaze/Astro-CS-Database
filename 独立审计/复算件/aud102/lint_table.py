import io, sys, collections
sys.stdout.reconfigure(encoding='utf-8')
F = r"独立审计/01_文档\迁移合并清单.md"
bad = []
n = 0
for i, l in enumerate(io.open(F, encoding='utf-8'), 1):
    if not l.startswith('| `'):
        continue
    n += 1
    c = [x.strip() for x in l.strip().strip('|').split('|')]
    if len(c) != 9:
        bad.append((i, len(c), l[:120]))
print('表格行:', n, '列数异常:', len(bad))
for b in bad[:10]:
    print(b)
