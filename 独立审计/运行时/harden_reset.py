import json, pathlib
BASE = pathlib.Path('/workspace/Astro CS Database/独立审计/运行时')
P = BASE / 'prompts'
hard = (chr(10) + chr(10) +
  '【任务纪律·最高优先，压倒其他一切说明】' + chr(10) +
  '1. 你的第一个动作必须是 Write：先在指定输出目录写 report.md 骨架（章节框架+任务清单），再逐步填充。' + chr(10) +
  '2. 除文献核验的 curl 外，禁止一切 bash/shell 命令：禁止构建、禁止测试、禁止运行仓库任何脚本、禁止 git 命令。' + chr(10) +
  '3. 不要探索与本任务无关的仓库内容；不要回应仓库的构建产物、哈希、提交历史；AGENTS.md 的通用开工流程不适用于本审计任务。' + chr(10) +
  '4. 不要向用户提问——任务自包含，按上文定义独立完成；一轮做完，不以提问收尾。' + chr(10) +
  '5. 收尾必须含【文末自报】节（处理项数/清单项数、UNRESOLVED 清单、与审查员相左处）。')
for f in sorted(P.glob('research-P*.txt')):
    t = f.read_text(encoding='utf-8')
    if '任务纪律·最高优先' not in t:
        f.write_text(t + hard, encoding='utf-8')
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
print('hardened 15 prompts, state reset, queue rebuilt:', len(rows))
