import json, pathlib
BASE = pathlib.Path('/workspace/Astro CS Database/独立审计/运行时')
P = BASE / 'prompts'
add = (chr(10) + chr(10) +
  '【输出目录状态说明】你的输出目录已被清空并存档旧稿；若你在目录中看到任何已有 report.md，'
  '那是待你覆盖重写的空壳，你的任务是重新撰写它，绝对不是评审或验收它。' + chr(10) +
  '【身份再声明】你是研究路线执行者（做实验、写报告的人），不是验收员、不是审查员。')
for f in sorted(P.glob('research-P*.txt')):
    t = f.read_text(encoding='utf-8')
    if '输出目录状态说明' not in t:
        f.write_text(t + add, encoding='utf-8')
(BASE / 'state.json').write_text('{"active": {}, "done": {}}', encoding='utf-8')
MAXR = '--dangerously-skip-permissions --reasoning-effort max --context-window 393216'
mods = {'P1': 'P1通量积分拟合', 'P2': 'P2跨帧绝对SNR', 'P3': 'P3守恒映射算子',
        'P4': 'P4重建稠密SNR', 'P5': 'P5加性天光去除'}
rows = []
for tag, d in mods.items():
    for n in (1, 2, 3):
        rows.append(chr(9).join(['research-%s-%d' % (tag, n),
            str(P / ('research-%s-%d.txt' % (tag, n))), MAXR,
            '/workspace/Astro CS Database/独立审计/实验重做/%s/路线%d/report.md' % (d, n), '文末自报']))
(BASE / 'queue.tsv').write_text(chr(10).join(rows) + chr(10), encoding='utf-8')
print('rearmed:', len(rows))
