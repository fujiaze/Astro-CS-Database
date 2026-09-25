import io, os, sys
sys.stdout.reconfigure(encoding='utf-8')
F = r"独立审计/01_文档\迁移合并清单.md"
lines = io.open(F, encoding='utf-8').read().split('\n')


def new_target(p):
    if p.startswith('docs/plugins/'):
        return '模块工作细节（正本目录）'
    if p == 'docs/modules/MODULE_MAP.yaml':
        return '模块工作细节（机器映射面，保留）'
    if p.startswith('docs/modules/registry/'):
        return '模块工作细节（生成面）'
    if p.startswith('docs/modules/'):
        return '模块工作细节（手写页折入 plugins）'
    if p.startswith('docs/browser/'):
        return '模块工作细节（折入 plugins/infrastructure）'
    return None


n = 0
for i, l in enumerate(lines):
    if not l.startswith('| `'):
        continue
    c = [x.strip() for x in l.strip().strip('|').split('|')]
    if len(c) != 9:
        continue
    v = new_target(c[0].strip('`'))
    if v and c[2] != v:
        c[2] = v
        lines[i] = '| ' + ' | '.join(c) + ' |'
        n += 1
io.open(F, 'w', encoding='utf-8').write('\n'.join(lines))
t = '\n'.join(lines)
print('应角色列改写:', n, ' 残留"并入 modules":', t.count('并入 modules'))
