import sys
import shutil
from pathlib import Path

sys.stdout.reconfigure(encoding='utf-8')
BASE = Path(r'产出/')
T = BASE / '独立审计包' / '06_实施' / 'tasks'
BK = BASE / '复算' / 'xref' / 'domain_fix_backup'
BK.mkdir(exist_ok=True)

FIX = [
    ('ACSD-T24_X5-不可估计哨兵与退化侧守卫族.md',
     'lib/algorithms/star_detection/cpp/src/star_matcher.cpp',
     'lib/algorithms/photometry/cpp/src/star_matcher.cpp', '唯一同名跟踪件'),
    ('ACSD-T24_X5-不可估计哨兵与退化侧守卫族.md',
     'lib/infrastructure/pipeline/orchestrator/src/orchestrator.cpp',
     'lib/infrastructure/pipeline/orchestrator/cpp/src/orchestrator.cpp', '缺 cpp/ 一段'),
    ('ACSD-T40_结果层数值口径与占位骨架.md', '`docs/smooth-lambda.md`',
     '`实验/additive-sky-seamless/docs/smooth-lambda.md`', '单元相对→仓库相对'),
    ('ACSD-T40_结果层数值口径与占位骨架.md', 'docs/smooth-lambda.md 按 R5 处置',
     '实验/additive-sky-seamless/docs/smooth-lambda.md 按 R5 处置', '同上（正文）'),
]
MARK = [
    ('ACSD-T21_X2-X4-核权重分母与几何预算.md', 'docs/algorithms/DRIZZLE_ALGORITHMS.md', '入库'),
    ('ACSD-T06_锚门与引用可解析面扩面.md', 'reports/v6/contract-review/02_CONFLICT_AND_GAP_AUDIT.md', '入库'),
]

log = []
for fn, old, new, why in FIX:
    p = T / fn
    shutil.copyfile(p, BK / (fn + '.bak')) if not (BK / (fn + '.bak')).exists() else None
    t = p.read_text(encoding='utf-8')
    n = t.count(old)
    t2 = t.replace(old, new)
    p.write_text(t2, encoding='utf-8')
    log.append('改 %s：%r→%r ×%d（%s）' % (fn[:14], old[:44], new[:44], n, why))

for fn, path, tag in MARK:
    p = T / fn
    shutil.copyfile(p, BK / (fn + '.bak')) if not (BK / (fn + '.bak')).exists() else None
    t = p.read_text(encoding='utf-8')
    cnt = 0
    out = []
    for line in t.splitlines():
        if path in line and line.rstrip().endswith('| ' + tag + ' |'):
            line = line.rstrip()[:-len('| ' + tag + ' |')] + '| 待核：该路径不在仓库跟踪集，先定对象再派工 |'
            cnt += 1
        out.append(line)
    p.write_text('\n'.join(out) + '\n', encoding='utf-8')
    log.append('标待核 %s：%s ×%d' % (fn[:14], path[:44], cnt))

print('\n'.join(log))
resid = []
for fn in sorted(x.name for x in T.glob('*.md')):
    t = (T / fn).read_text(encoding='utf-8')
    for bad in ['star_detection/cpp/src/star_matcher', 'orchestrator/src/orchestrator.cpp',
                '`docs/smooth-lambda.md`']:
        if bad in t:
            resid.append((fn[:20], bad))
print('残留检查：', resid or '无')
