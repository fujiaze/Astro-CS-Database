import pathlib, re
BASE = pathlib.Path('/workspace/Astro CS Database/独立审计/运行时')
P = BASE / 'prompts'
# P5 特殊化：③科学性审查不存在，改用 UPM 权威正本＋③缺陷清单为缺口来源
for n in (1, 2, 3):
    f = P / ('research-P5-%d.txt' % n)
    t = f.read_text(encoding='utf-8')
    old = re.search(r'1\. 独立审计/证据/审查-05-③-科学性[^；]{10,}；', t)
    if old:
        t = t.replace(old.group(0),
            '1. UPM 模块暂无独立科学审查成稿——以 独立审计/08_修复包/③加性天光无缝/01_缺陷清单.md 与 04_二次核对.md 中科学量条目、'
            'docs/science/PHASE2_UPM.md 的常数/阈值（如接缝判据门槛 1e-2、方差比诊断量、稀疏采样点噪声逆方差权重）为缺口清单来源，逐个做三腿核查并补缺；', 1)
        f.write_text(t, encoding='utf-8')
        print('P5-%d input line specialized' % n)
# P3 的 ④科学性审查是否已存在
ev = pathlib.Path('/workspace/Astro CS Database/独立审计/证据')
p3rev = sorted(ev.glob('审查*05-④-科学性*.md'))
print('P3 review sources:', [x.name for x in p3rev])
# 组队列：只排缺 文末自报 的路线
MAXR = '--dangerously-skip-permissions --reasoning-effort max --context-window 393216'
mods = {'P1': 'P1通量积分拟合', 'P2': 'P2跨帧绝对SNR', 'P3': 'P3守恒映射算子',
        'P4': 'P4重建稠密SNR', 'P5': 'P5加性天光去除'}
rows, skipped = [], []
for tag, d in mods.items():
    for n in (1, 2, 3):
        outdir = pathlib.Path('/workspace/Astro CS Database/独立审计/实验重做') / d / ('路线%d' % n)
        report = outdir / 'report.md'
        done = False
        if report.exists():
            txt = report.read_text(encoding='utf-8', errors='replace')
            done = '文末自报' in txt[-5000:] and len(txt) > 3000
        if done:
            skipped.append('%s-%d' % (tag, n))
            continue
        rows.append(chr(9).join(['research-%s-%d' % (tag, n),
            str(P / ('research-%s-%d.txt' % (tag, n))), MAXR, str(report), '文末自报']))
(BASE / 'queue.tsv').write_text(chr(10).join(rows) + chr(10), encoding='utf-8')
(BASE / 'state.json').write_text('{"active": {}, "done": {}}', encoding='utf-8')
print('queued:', len(rows), '| already complete:', skipped)
