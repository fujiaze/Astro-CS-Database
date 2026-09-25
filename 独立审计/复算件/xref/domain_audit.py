"""D8 任务书文件域自检：① 跨任务重复声明（违反"文件域互斥"）② 声明了不存在的路径
   ③ 声明粒度（目录 vs 文件）。判据：§4 文件域 代码块内的每一行。"""
import io
import re
import sys
import json
import subprocess
from pathlib import Path
from collections import defaultdict

sys.stdout.reconfigure(encoding='utf-8')
BASE = Path(r'产出/')
TASKS = BASE / '独立审计包' / '06_实施' / 'tasks'
REPO = Path(r'F:\Astro dev\Astro CS Normalization Database')

repo_files = set(subprocess.run(['git', '-C', str(REPO), '-c', 'core.quotePath=false', 'ls-files'],
                                capture_output=True, text=True, encoding='utf-8').stdout.splitlines())
repo_dirs = set()
for p in repo_files:
    parts = p.split('/')
    for i in range(1, len(parts)):
        repo_dirs.add('/'.join(parts[:i]))
run_dirs = {p for p in (x.split('/') for x in [])}

claims = defaultdict(list)
per_task = {}
for f in sorted(TASKS.glob('*.md')):
    t = f.name[:-3]
    txt = f.read_text(encoding='utf-8')
    m = re.search(r'^##\s*4[^\n]*文件域[^\n]*\n(.*?)(?=^##\s|\Z)', txt, re.S | re.M)
    if not m:
        print('%s: 缺 §4 文件域节' % t)
        per_task[t] = None
        continue
    block = m.group(1)
    paths = []
    for line in block.splitlines():
        raw = line.strip()
        if raw.startswith('```'):
            continue
        s = raw.strip('`')
        if not s or s.startswith(('#', '|', '-')) or ' ' in s:
            continue
        if not re.match(r'^[\w.\-]+([/][\w.\-*]+)*/?$', s):
            continue
        paths.append(s)
    per_task[t] = paths
    for p in paths:
        claims[p].append(t)

print('任务书 %d 份，文件域声明行 %d 条，去重路径 %d 个' %
      (len(per_task), sum(len(v or []) for v in per_task.values()), len(claims)))
dup = {p: ts for p, ts in claims.items() if len(ts) > 1}


def nests(a, b):
    """目录声明与落在其中的任何声明算相交。"""
    if a == b:
        return True
    da, db = a.rstrip('/'), b.rstrip('/')
    return (a.endswith('/') and db.startswith(da + '/')) or (b.endswith('/') and da.startswith(db + '/'))


import itertools
tp_exact = set()
tp_nest = set()
for p, ts in claims.items():
    for a, b in itertools.combinations(sorted(ts), 2):
        tp_exact.add((a, b))
task_claims = {}
for p, ts in claims.items():
    for t in ts:
        task_claims.setdefault(t, set()).add(p)
for ta, tb in itertools.combinations(sorted(task_claims), 2):
    if any(nests(x, y) for x in task_claims[ta] for y in task_claims[tb]):
        tp_nest.add((ta, tb))
print('=== 相交（精确同名对象）：%d 个对象 / %d 对任务 ===' % (len(dup), len(tp_exact)))
print('=== 相交（含目录嵌套）：%d 对任务 ===' % len(tp_nest))
print('=== 按对象求和的相交对（不去重）：%d ===' % sum(len(t) * (len(t) - 1) // 2 for t in claims.values() if len(t) > 1))
print('   目录声明共 %d 条：%s' % (sum(1 for p in claims if p.endswith('/')),
                                  '、'.join(sorted(p for p in claims if p.endswith('/'))[:8])))
(BASE / '复算' / 'xref' / 'domain_counts.txt').write_text(
    'exact_objects=%d pairs_exact=%d pairs_nested=%d object_sum=%d dir_claims=%d claims=%d uniq=%d tasks=%d\n'
    % (len(dup), len(tp_exact), len(tp_nest),
       sum(len(t) * (len(t) - 1) // 2 for t in claims.values() if len(t) > 1),
       sum(1 for p in claims if p.endswith('/')),
       sum(len(v) for v in per_task.values() if v), len(claims), len(per_task)), encoding='utf-8')
print('=== ① 跨任务重复声明（违反互斥）：%d 个路径 ===' % len(dup))
(BASE / '复算' / 'xref' / 'dup_objects.json').write_text(
    json.dumps({p: sorted(ts) for p, ts in dup.items()}, ensure_ascii=False, indent=0), encoding='utf-8')
for p, ts in sorted(dup.items()):
    print('   %s	%d	%s' % (p, len(ts), '、'.join(sorted(ts))))
(BASE / '复算' / 'xref' / 'dup_objects.json').write_text(
    json.dumps({p: sorted(ts) for p, ts in dup.items()}, ensure_ascii=False, indent=0), encoding='utf-8')

print('=== ② 声明了跟踪集里不存在的路径 ===')
miss = []
for p, ts in sorted(claims.items()):
    if p in repo_files or p.rstrip('/') + '/' in repo_dirs or p in repo_dirs:
        continue
    if (REPO / p).exists():
        continue
    miss.append((p, sorted(ts)))
for p, ts in miss:
    print('   %-62s %s' % (p[:62], '、'.join(ts)))
print('   合计 %d 条' % len(miss))

no4 = [t for t, v in per_task.items() if v is None]
empty = [t for t, v in per_task.items() if v == []]
print('=== ③ 结构完整性：缺 §4 节 %d 份 %s；§4 解析到 0 条 %s' % (len(no4), no4, empty))
gran = defaultdict(int)
for p in claims:
    gran['目录' if p.endswith('/') else ('通配' if '*' in p else '文件')] += 1
print('   粒度分布：', dict(gran))
rc = 1 if (dup or miss or no4 or empty) else 0
print('rc=%d' % rc)
(BASE / '复算' / 'xref' / 'domain_audit.txt').write_text(
    'tasks=%d claims=%d uniq=%d dup=%d missing=%d no4=%d empty=%d\n'
    % (len(per_task), sum(len(v or []) for v in per_task.values()), len(claims),
       len(dup), len(miss), len(no4), len(empty)), encoding='utf-8')
sys.exit(rc)
