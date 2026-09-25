import io, re, sys
sys.stdout.reconfigure(encoding='utf-8')
F = r"独立审计/01_文档\迁移合并清单.md"
lines = io.open(F, encoding='utf-8').read().split('\n')
n = 0
for i, l in enumerate(lines):
    if not l.startswith('| `'):
        continue
    c = [x.strip() for x in l.strip().strip('|').split('|')]
    if len(c) != 9:
        continue
    p = c[0].strip('`')
    if not p.startswith('docs/plugins/'):
        continue
    if '→ docs/modules/' in c[4] and '/registry/' not in c[4]:
        mm = re.search(r'→ (docs/modules/[^\s（]+)', c[4])
        if mm:
            c[4] = ('本份为模块工作细节正本；对方 `%s` 的手写内容折入本份后随 W3 撤销该短页'
                    '（依据《目标文档架构》§3.3：`MODULE_MAP.yaml` 的 24 个 plugin_doc 字段与模块映射门均指向 `docs/plugins/**`）' % mm.group(1))
            c[3] = '保留（接收合并）'
            c[6] = '公共前置：先按《索引重建规格》把对方短页的条目改指本份；本份 role/落位不动'
            lines[i] = '| ' + ' | '.join(c) + ' |'
            n += 1
# browser 行去向改指实存插件页
for i, l in enumerate(lines):
    if l.startswith('| `docs/browser/HIPS_BROWSER.md`'):
        c = [x.strip() for x in l.strip().strip('|').split('|')]
        if len(c) == 9:
            c[4] = ('→ docs/plugins/infrastructure/23_hips_browser.md（去向实存；台账所写 '
                    '`docs/modules/hips_browser.md` 不存在，按 §3.3 正本面改指插件页）')
            lines[i] = '| ' + ' | '.join(c) + ' |'
            n += 1
io.open(F, 'w', encoding='utf-8').write('\n'.join(lines))
print('方向修正行数:', n)
