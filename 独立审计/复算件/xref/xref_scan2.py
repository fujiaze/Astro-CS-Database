"""包内/仓库交叉引用自检：判"无悬空引用"这条验收门本身要能红。

用法：python xref_scan2.py [--self-test]
三类判定：
  包内引用 = 首段是本包 8 个目录名之一，或以 独立审计包/ 开头；
  仓库引用 = 首段在仓库顶层白名单内；
  裸名引用 = 只有文件名没有目录前缀（在仓库跟踪集里唯一 ⇒ 歧义；多命中 ⇒ 引用不可解析）。
负例（必须报红）与正例（必须报绿）见 SELFTEST 表。
"""
import re
import sys
import subprocess
from pathlib import Path

sys.stdout.reconfigure(encoding='utf-8')
BASE = Path(r'产出/')
PKG = BASE / '独立审计包'
REPO = Path(r'F:\Astro dev\Astro CS Normalization Database')

PKG_DIRS = {'01_文档', '02_科学', '03_小论文', '04_代码', '05_门禁', '06_实施', '07_未决'}
PKG_FILES = {'00_总目录.md'}
# 目标态文档：其中的"新增/迁移后路径"按设计就是不存在的，不判悬空（判了就是工具不懂文档角色）
TARGET_DOCS = {'01_文档/目标文档架构.md', '01_文档/迁移合并清单.md', '01_文档/索引重建规格.md'}
REPO_ROOTS = ('lib/', 'eng/', 'docs/', '实验/', 'testdata/', 'artifacts/', '工程控制/', 'run/',
              'gaia/', 'vendor/', 'site/')
ROOT_FILES = ('ASTROCS_DESIGN.md', 'ENGINEERING_SPEC.md', 'ACCEPTANCE_SPEC.md', 'CONTROL_PACK_SPEC.md',
              'README.md', 'VERSION', 'AGENTS.md', 'CMakeLists.txt', '.gitignore')
EXT = ('.md', '.csv', '.yaml', '.yml', '.json', '.py', '.cpp', '.h', '.hpp', '.txt',
       '.cmake', '.sh', '.in', '.bib', '.tex', '.c', '.hh', '.list', '.toml', '.ps1')

MDLINK = re.compile(r'\]\(([^)\s]+)')
TICK = re.compile(r'`([^`\n]{2,220})`')


def is_pkg_ref(s):
    h = s.split('#')[0]
    if h.startswith('独立审计包/'):
        return h[len('独立审计包/') + 1:]
    if h in PKG_FILES:
        return h
    seg = h.split('/')[0]
    if seg in PKG_DIRS and (h == seg or h.startswith(seg + '/')):
        return h
    return None


def is_repo_ref(s):
    h = s.split('#')[0].lstrip('./')
    if h.startswith(REPO_ROOTS) or h in ROOT_FILES:
        return h
    return None


def strip_line_suffix(h, exists):
    m = re.match(r'^(.*?):(\d+(?:-\d+)?)$', h)
    if m and not exists(h) and exists(m.group(1)):
        return m.group(1), True
    return h, False


ANCHOR = re.compile(r'\s*(§[0-9A-Za-z.\-（）()、和与至–—到第0-9]+|第\s*[0-9一二三四五六七八九十]+\s*[条章节款]|表首列.*)$')
NOISE = ('*', '<', '>', '…', '→', '|', ' ', '`', '$', '⟂', '...')


def deanchor(h):
    """剥掉尾部"§x / 第 N 条"类锚注，返回（文件路径, 是否剥过）。"""
    prev = h
    while True:
        m = ANCHOR.search(prev)
        if not m or m.start() == 0:
            return prev, prev != h
        prev = prev[:m.start()].rstrip()


def has_noise(h):
    return any(x in h for x in NOISE)


ANCHOR_TAIL = re.compile(r'^[\d,\-–/，、 :]*\d[\d,\-–/，、 :]*$')


def file_part(h):
    """把 `路径:行,行/行段` 一类多锚形态归一回纯路径。"""
    h = deanchor(h)[0].split('::')[0].strip()
    pre = h[:2] if re.match(r'^[A-Za-z]:[/\\]', h) else ''
    rest = h[len(pre):]
    segs = rest.split(':')
    out = segs[0]
    for s in segs[1:]:
        if ANCHOR_TAIL.match(s.rstrip(',，/ ')) or re.fullmatch(r'\d+(-\d+)?', s.rstrip(',，/ ')):
            continue
        out += ':' + s
        break
    return (pre + out).rstrip(',，/ 、')


def collect(repo_files):
    def exists_repo(p):
        return p in repo_files or (REPO / p).exists()

    def exists_pkg(p):
        return (PKG / p).resolve().exists()

    by_name = {}
    for p in repo_files:
        by_name.setdefault(p.rsplit('/', 1)[-1], []).append(p)

    pkg_bad, repo_bad, bare, target_only, noise, outside, noanchor = [], [], [], [], [], [], []
    pkg_ok = repo_ok = 0
    files = sorted(PKG.rglob('*.md'))
    for f in files:
        rel = f.relative_to(PKG).as_posix()
        role = '目标态' if rel in TARGET_DOCS else '现状类'
        for ln, line in enumerate(f.read_text(encoding='utf-8').splitlines(), 1):
            cands = [m.group(1) for m in MDLINK.finditer(line)]
            cands += [m.group(1).strip() for m in TICK.finditer(line)]
            for c in cands:
                c = c.strip('，。；：、（）()『』「」"\' ')
                if not c or c.startswith(('http', '#', '$', '\\\\')):
                    continue
                pr = is_pkg_ref(c)
                if pr is not None:
                    h0 = file_part(pr.split('#')[0])
                    if has_noise(h0):
                        noise.append((rel, ln, c))
                        continue
                    h2 = h0 if exists_pkg(h0) else strip_line_suffix(h0, exists_pkg)[0]
                    if exists_pkg(h2):
                        pkg_ok += 1
                    else:
                        pkg_bad.append((rel, ln, c))
                    continue
                rr = is_repo_ref(c)
                if rr is not None:
                    h0 = file_part(rr.split('#')[0])
                    if has_noise(h0):
                        noise.append((rel, ln, c))
                        continue
                    h2 = h0 if exists_repo(h0) else strip_line_suffix(h0, exists_repo)[0]
                    anchored = re.search(r':\d', c) is not None
                    if exists_repo(h2):
                        repo_ok += 1
                    elif role == '目标态':
                        target_only.append((rel, ln, c))
                    elif h0.startswith('run/') or '/run/' in h0:
                        outside.append((rel, ln, c))
                    elif not anchored:
                        noanchor.append((rel, ln, c))
                    else:
                        repo_bad.append((rel, ln, c))
                    continue
                nm = file_part(c.split('#')[0])
                if '/' not in nm and nm.endswith(EXT) and nm in by_name:
                    bare.append((rel, ln, c, len(by_name[nm])))
    return files, pkg_ok, pkg_bad, repo_ok, repo_bad, bare, by_name, target_only, noise, outside, noanchor


SELFTEST = [
    ('01_文档/目标文档架构.md', True), ('04_代码/架构对齐报告.md', True),
    ('独立审计/02_科学/测光链路核验报告.md', True),
    ('lib/algorithms/photometry/cpp/src/image_corrector.h', True),
    ('docs/science/DRIZZLE.md', True),
    ('1/σ_bg²', False), ('4π/(12N²)', False), ('1e-6/1e-5/1e-7', False), ('0.5/q', False),
    ('SNR链路核验报告.md', False), ('2.5·log10(1/pf²) = 0.484550', False),
    ('07_noise_snr.md:121', False), ('03_GATES.md §6', False),
]
DEANCHOR = [('docs/ci/01_CHECKS.md §2.1', 'docs/ci/01_CHECKS.md'),
            ('docs/plugins/00_INDEX.md §2', 'docs/plugins/00_INDEX.md'),
            ('docs/ci/03_GATES.md 第 3 条', 'docs/ci/03_GATES.md'),
            ]
FILEPART = [('docs/science/DRIZZLE.md:120', 'docs/science/DRIZZLE.md'),
            ('lib/algorithms/calibration/src/photometry_apply.cpp:68,:86',
             'lib/algorithms/calibration/src/photometry_apply.cpp'),
            ('eng/ci/checks.json:8982/9005/9029', 'eng/ci/checks.json'),
            ('eng/ci/checks.json:4359-4364,:4547-4625,', 'eng/ci/checks.json'),
            ('docs/science/CALIBRATION.md:420/', 'docs/science/CALIBRATION.md'),
            ('实验/absolute-snr/results/b1_sky_scan.json::gates_bright/gates',
             '实验/absolute-snr/results/b1_sky_scan.json'),
            ('docs/ci/01_CHECKS.md §2.1', 'docs/ci/01_CHECKS.md'),
            ('F:/x/y.py:12', 'F:/x/y.py')]
NOISECASE = [('lib/**', True), ('artifacts/ci/<sha>/', True), ('docs/modules/*.md', True),
             ('docs/science/DRIZZLE.md', False), ('eng/ci/checks.json', False)]


def selftest(repo_files):
    bad = 0
    for s, want in SELFTEST:
        got = bool(is_pkg_ref(s) or is_repo_ref(s))
        if got != want:
            bad += 1
            print('  SELFTEST FAIL(分类): %r 判为 %s，应为 %s' % (s, got, want))
    for s, want in DEANCHOR:
        got = deanchor(s)[0]
        if got != want:
            bad += 1
            print('  SELFTEST FAIL(剥锚): %r -> %r，应为 %r' % (s, got, want))
    for s, want in FILEPART:
        got = file_part(s)
        if got != want:
            bad += 1
            print('  SELFTEST FAIL(归一): %r -> %r，应为 %r' % (s, got, want))
    for s, want in NOISECASE:
        got = has_noise(s)
        if got != want:
            bad += 1
            print('  SELFTEST FAIL(噪声): %r -> %s，应为 %s' % (s, got, want))
    hits = [('04_代码/尚不存在的一件.md', False), ('05_门禁/门禁清单.csv', True),
            ('docs/science/NOISE_MODEL.md', True), ('docs/science/绝不存在.md', False)]
    for p, want in hits:
        if p.startswith('docs'):
            got = p in repo_files or (REPO / p).exists()
        else:
            got = (PKG / p).exists()
        if got != want:
            bad += 1
            print('  SELFTEST FAIL(存在性两边都要走到): %r got=%s want=%s' % (p, got, want))
    n = len(SELFTEST) + len(DEANCHOR) + len(FILEPART) + len(NOISECASE) + len(hits)
    print('selftest: %d 例，失败 %d' % (n, bad))
    return 1 if bad else 0


repo_files = set(subprocess.run(['git', '-C', str(REPO), 'ls-files'], capture_output=True,
                                text=True, encoding='utf-8', errors='replace').stdout.splitlines())
if '--self-test' in sys.argv:
    sys.exit(selftest(repo_files))

files, pkg_ok, pkg_bad, repo_ok, repo_bad, bare, by_name, target_only, noise, outside, noanchor = collect(repo_files)
print('扫描文件 %d 份（仓库跟踪件 %d）' % (len(files), len(repo_files)))
print('包内路径引用：命中 %d，悬空 %d' % (pkg_ok, len(pkg_bad)))
for rel, ln, c in pkg_bad:
    print('   [包内悬空] %s:%d  %s' % (rel, ln, c))
print('仓库路径引用（现状类文档）：命中 %d，悬空 %d' % (repo_ok, len(repo_bad)))
agg = {}
for rel, ln, c in repo_bad:
    agg.setdefault(file_part(c.split('#')[0]), []).append('%s:%d' % (rel, ln))
for k, v in sorted(agg.items(), key=lambda x: -len(x[1])):
    print('   [仓库悬空] %-58s %d 处 例 %s' % (k[:58], len(v), v[0]))
print('证据面外（run/** 未跟踪）：%d 处，不判红：' % len(outside))
for rel, ln, c in sorted(set((x[2].split('#')[0],) and (x[0], x[1], file_part(x[2].split('#')[0])) for x in outside)):
    print('   [证据面外] %s:%d  %s' % rel_ln if False else ('   [证据面外] %s:%d  %s' % (rel, ln, c)))
print('无行锚的仓库路径引用（现状主张不可核，列为卫生项）：%d 处' % len(noanchor))
for rel, ln, c in noanchor:
    print('   [无锚] %s:%d  %s' % (rel, ln, c))
print('目标态文档里的未存在路径：%d 处（设计意图，不判红）' % len(target_only))
print('glob／占位／含空格形态：%d 处（不作路径判定）' % len(noise))
amb = [b for b in bare if b[3] > 1]
print('裸名引用（无目录前缀）%d 处；其中仓库同名多命中 ⇒ 不可解析 %d 处' % (len(bare), len(amb)))
seen = set()
for rel, ln, c, n in sorted(amb, key=lambda x: -x[3]):
    key = c.split(':')[0]
    if key in seen:
        continue
    seen.add(key)
    if len(seen) <= 15:
        print('   [裸名多命中] %-38s 同名 %d 个  例 %s:%d' % (key, n, rel, ln))
(BASE / '复算' / 'xref' / 'report2.txt').write_text(
    'files=%d tracked=%d pkg_ok=%d pkg_bad=%d repo_ok=%d repo_bad_uniq=%d '
    'target_only=%d noise=%d bare=%d bare_ambiguous=%d\n'
    % (len(files), len(repo_files), pkg_ok, len(pkg_bad), repo_ok, len(agg),
       len(target_only), len(noise), len(bare), len(amb)), encoding='utf-8')
rc = 1 if (pkg_bad or repo_bad or amb) else 0
print('rc=%d（判红面＝包内悬空 %d ＋ 现状类仓库悬空 %d ＋ 裸名多命中 %d；'
      '目标态未存在路径与 glob 不判红）' % (rc, len(pkg_bad), len(agg), len(amb)))
