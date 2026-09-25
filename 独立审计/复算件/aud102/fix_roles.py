import io, os, re, sys
sys.stdout.reconfigure(encoding='utf-8')
F = r"独立审计/01_文档\迁移合并清单.md"
OUT = r"独立审计/复算件/aud102"
FIELDS = ['batch', 'path', 'lines', 'title', 'role', 'topic', 'up', 'down', 'auth', 'dup',
          'meta', 'viol', 'dangle', 'conflict', 'disp']
rec = {}
for ln in io.open(os.path.join(OUT, 'records3.tsv'), encoding='utf-8'):
    p = ln.rstrip('\n').split('\t')
    p += [''] * (len(FIELDS) - len(p))
    r = dict(zip(FIELDS, p))
    path = r['path']
    if path.startswith('tasks/'):
        path = '工程控制/RELEASE-05/' + path
    sc = lambda x: len(x.get('role', '')) + len(x.get('disp', ''))
    if path not in rec or sc(r) > sc(rec[path]):
        rec[path] = r


def clean_role(p):
    r = rec.get(p)
    s = re.sub(r'\s+', ' ', (r or {}).get('role', '') or '')
    s = s.replace('**', '').replace('★', '').replace('⧉', '').replace('`', '')
    s = re.split(r'[（(·⇒|｜]|实际|应属|相符', s)[0].strip(' ，,、。;；:：-')
    s = re.sub(r'\s+', ' ', s)
    if len(s) < 3 or s in ('+', '/', '-'):
        d = os.path.dirname(p)
        s = {'docs/science': '科学定义', 'docs/algorithms': '算法推导', 'docs/architecture': '架构设计',
             'docs/design': '架构设计', 'docs/interfaces': '架构设计（接口）', 'docs/api': '架构设计（接口）',
             'docs/contracts': '合同说明', 'docs/standards': '标准与检查', 'docs/ci': '标准与检查（门）',
             'docs/modules': '模块工作细节', 'docs/plugins': '模块工作细节', 'docs/research': '研究与佐证',
             'docs/references': '研究与佐证', 'docs/owner': '负责人面', 'docs/traceability': '追溯登记',
             'docs/quality': '标准与检查（度量）', 'docs/audit': '过程台账', 'docs/validation': '验证档案',
             'docs/browser': '模块工作细节', 'docs/development': '操作与运营', 'docs/operations': '操作与运营',
             'docs/performance': '操作与运营（性能）', 'docs/diagnostics': '操作与运营（诊断）',
             '实验': '实验单元', '工程控制': '任务包与过程记录'}.get(
            d if d in ('docs',) else next((k for k in ('docs/science', 'docs/algorithms', 'docs/architecture',
                                                       'docs/design', 'docs/interfaces', 'docs/api', 'docs/contracts',
                                                       'docs/standards', 'docs/ci', 'docs/modules', 'docs/plugins',
                                                       'docs/research', 'docs/references', 'docs/owner',
                                                       'docs/traceability', 'docs/quality', 'docs/audit',
                                                       'docs/validation', 'docs/browser', 'docs/development',
                                                       'docs/operations', 'docs/performance', 'docs/diagnostics')
                                           if d.startswith(k)), ''), '')
        s = s or '台账未标注角色'
    return (s[:26] or '台账未标注角色')


lines = io.open(F, encoding='utf-8').read().split('\n')
fixed = 0
for i, l in enumerate(lines):
    if not l.startswith('| `'):
        continue
    c = [x.strip() for x in l.strip().strip('|').split('|')]
    if len(c) != 9:
        continue
    path = c[0].strip('`')
    new = clean_role(path)
    if c[1] != new:
        c[1] = new
        lines[i] = '| ' + ' | '.join(c) + ' |'
        fixed += 1
io.open(F, 'w', encoding='utf-8').write('\n'.join(lines))
print('现角色列改写行数:', fixed)
