import os
import re
import io
import subprocess
from pathlib import Path

BASE = Path(r'产出/')
PKG = BASE / '独立审计包'
REPO = Path(r'F:\Astro dev\Astro CS Normalization Database')

files = []
for root, dirs, fs in os.walk(PKG):
    dirs[:] = [d for d in dirs if d not in ('raw', '复算')]
    for fn in fs:
        if fn.endswith('.md'):
            files.append(Path(root) / fn)

repo_files = set(
    subprocess.run(['git', '-C', str(REPO), 'ls-files'],
                   capture_output=True, text=True, encoding='utf-8', errors='replace')
    .stdout.splitlines()
)

lines_out = []
pkg_refs = []
repo_refs = []
ids = {}

MDLINK = re.compile(r'\]\(([^)\s]+)')
TICK = re.compile(r'`([^`\n]{2,220})`')
IDPAT = re.compile(r'ACSD-T\d{2}|AUD-\d{3}|CHK-[A-Z0-9-]{3,}|CON-[A-Z0-9-]{3,}')

for f in sorted(files):
    rel = f.relative_to(PKG).as_posix()
    ids[rel] = set()
    for ln, line in enumerate(f.read_text(encoding='utf-8').splitlines(), 1):
        cand = []
        for m in MDLINK.finditer(line):
            cand.append(m.group(1))
        for m in TICK.finditer(line):
            t = m.group(1).strip()
            if ('/' in t or t.endswith('.md') or t.endswith('.csv')) and ' ' not in t:
                cand.append(t)
        for c in cand:
            c = c.strip('，。；：、 ')
            if not c or c.startswith(('http', '#')):
                continue
            head = c.split('#')[0]
            if head.startswith('..'):
                pkg_refs.append((rel, ln, c))
            elif head.startswith('独立审计包'):
                pkg_refs.append((rel, ln, c[len('独立审计包') + 1:]))
            elif head.startswith(('0', '1', '2', '3', '4', '5', '6', '7')) and '/' in head:
                pkg_refs.append((rel, ln, c))
            else:
                repo_refs.append((rel, ln, c))
        for m in IDPAT.finditer(line):
            ids[rel].add(m.group(0))


def norm(p):
    p = p.split('#')[0]
    m = re.search(r'^(.*?):(\d+)(?:-\d+)?$', p)
    if m and not (REPO / p).exists():
        return m.group(1), m.group(2)
    return p, None


def check_pkg(p):
    q = (PKG / p).resolve()
    return q.exists(), q


def check_repo(p):
    if p in repo_files:
        return True
    return (REPO / p).exists()


bad_pkg = []
ok_pkg = 0
for rel, ln, c in pkg_refs:
    base, _ = norm(c)
    ok, _ = check_pkg(base)
    if ok:
        ok_pkg += 1
    else:
        bad_pkg.append((rel, ln, c))

bad_repo = {}
ok_repo = 0
skip = 0
for rel, ln, c in repo_refs:
    base, line_no = norm(c)
    base = base.lstrip('./')
    if not base or len(base) < 3:
        skip += 1
        continue
    if any(x in base for x in ('…', '→', '<', '>', '*')):
        skip += 1
        continue
    if check_repo(base):
        ok_repo += 1
    else:
        bad_repo.setdefault(base, []).append('%s:%d' % (rel, ln))

lines_out.append('包内路径引用：%d 处，命中 %d，未命中 %d' % (len(pkg_refs), ok_pkg, len(bad_pkg)))
lines_out.append('仓库路径引用（反引号候选）：%d 处，命中跟踪集/磁盘 %d，未命中 %d，形态不合判定 %d'
                 % (len(repo_refs), ok_repo, len(bad_repo), skip))
lines_out.append('仓库路径未命中去重后 %d 个（按被引次数排序，前 40）：' % len(bad_repo))
for p, where in sorted(bad_repo.items(), key=lambda kv: -len(kv[1]))[:40]:
    lines_out.append('  %s  <- %s' % (p, ', '.join(where[:4])))

all_ids = set()
for s in ids.values():
    all_ids |= s
taskfiles = set()
tdir = PKG / '06_实施' / 'tasks'
if tdir.is_dir():
    taskfiles = {f.name[:-3] for f in tdir.glob('*.md')}
bad_ids = []
for rel, s in sorted(ids.items()):
    for i in sorted(s):
        if i.startswith('ACSD-T') and i not in taskfiles:
            bad_ids.append((rel, i))

gate_corpus = ''
for g in (REPO / 'eng' / 'ci').glob('*.json') if (REPO / 'eng' / 'ci').is_dir() else []:
    gate_corpus += g.read_text(encoding='utf-8', errors='replace')
bad_gates = []
for rel, s in sorted(ids.items()):
    for i in sorted(s):
        if i.startswith('ACSD-T') or i.startswith('AUD-'):
            continue
        if gate_corpus and i not in gate_corpus:
            bad_gates.append((rel, i))
lines_out.append('ACSD-Txx 编号引用中不存在的目标：%d 个' % len(bad_ids))
for rel, i in bad_ids[:40]:
    lines_out.append('  %s 缺 %s.md' % (rel, i))
lines_out.append('门 ID 引用中不在 eng/ci/*.json 现值的：%d 个（去重前 40）' % len(bad_gates))
seen = set()
for rel, i in bad_gates:
    if i in seen:
        continue
    seen.add(i)
    if len(seen) <= 40:
        lines_out.append('  %s  <- %s' % (i, rel))
lines_out.append('ACSD-Txx 编号引用中不存在的目标：%d 个' % len(bad_ids))
for rel, i in bad_ids[:40]:
    lines_out.append('  %s 缺 %s.md' % (rel, i))

out = BASE / '复算' / 'xref'
out.mkdir(parents=True, exist_ok=True)
(out / 'report.txt').write_text('\n'.join(lines_out) + '\n', encoding='utf-8')
print('refs pkg=%d repo=%d files=%d' % (len(pkg_refs), len(repo_refs), len(files)))
