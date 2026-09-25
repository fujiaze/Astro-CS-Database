import io, re, sys
sys.stdout.reconfigure(encoding='utf-8')
F = r"独立审计/01_文档\迁移合并清单.md"
rows = [l.rstrip('\n') for l in io.open(F, encoding='utf-8') if l.startswith('| `')]
def cells(l):
    return [c.strip() for c in l.strip().strip('|').split('|')]
groups = {'待定案': [], '缺去向': [], 'E0': [], '去向不存在': [], '去向待立': [], '上呈': [], '拆分': [], '下沉': []}
for l in rows:
    c = cells(l)
    path = c[0].strip('`')
    act, tgt, gr = c[3], c[4], c[8]
    if '待定案' in act:
        groups['待定案'].append((path, gr))
    if '缺：台账未点名去向' in tgt:
        groups['缺去向'].append(path)
    if gr == 'E0':
        groups['E0'].append(path)
    if '去向不存在' in tgt:
        groups['去向不存在'].append(path)
    if '去向待立' in tgt:
        groups['去向待立'].append(path)
    if '上呈' in act:
        groups['上呈'].append(path)
    if '拆分' in act:
        groups['拆分'].append(path)
    if '下沉' in act:
        groups['下沉'].append(path)
out = []
for k, v in groups.items():
    out.append('### %s（%d）' % (k, len(v)))
    for x in v:
        out.append('- ' + (x if isinstance(x, str) else '%s ｜举证 %s' % x))
    out.append('')
io.open(r'独立审计/复算件/aud102\special_lists.md', 'w', encoding='utf-8').write('\n'.join(out))
print('\n'.join('%s: %d' % (k, len(v)) for k, v in groups.items()))
