import subprocess, sys, collections, re
sys.stdout.reconfigure(encoding='utf-8')
REPO = r"F:/Astro dev/Astro CS Normalization Database"

def bucket(p):
    low = p.replace('\\', '/')
    if '/third_party/' in low: return 'VEND'
    if low.startswith('lib/third_party/'): return 'VEND'
    is_test = ('/tests/' in '/' + low or low.startswith('eng/tests/') or re.search(r'(^|/)(test_|.*_test|.*_probe|.*_oracle|.*_selftest)[^/]*\.(c|cpp|cc|cxx|h|hpp|py)$', low))
    if low.startswith('docs/') and low.endswith('.md'): return 'D' if not is_test else 'T'
    if low.startswith('lib/') and low.endswith('.md'): return 'D'
    if low.startswith('实验/'): return 'D'
    if is_test and (low.startswith('eng/tests/') or '/tests/' in '/' + low): return 'T'
    if low.startswith('eng/tests/') or low.startswith('eng/ci/tests/'): return 'T'
    if low.endswith('.md') or low.endswith('.yaml') and False: return 'D'
    if low.startswith('docs/') and (low.endswith('.csv') or low.endswith('.json') or low.endswith('.yaml')): return 'R'
    if low.startswith('eng/contracts/') or low.startswith('eng/ci/') or low.startswith('eng/packaging/') or low.startswith('artifacts/'): return 'R'
    if low.startswith('eng/tools/') and low.endswith('.md'): return 'D'
    if low.startswith('eng/tools/'): return 'R-TOOLS'
    if low.startswith('lib/') and is_test: return 'T'
    if low.startswith('lib/') or low == 'CMakeLists.txt' or low.startswith('eng/cmake/'): return 'P'
    if low.startswith('reports/') or low.startswith('工程控制/'): return 'R'
    return 'OTHER'

toks = sys.argv[1:]
for t in toks:
    out = subprocess.run(['git', '-C', REPO, '-c', 'core.quotepath=false', 'grep', '-n', '-w', '-e', t],
                         capture_output=True, text=True, encoding='utf-8', errors='replace)'.replace(')',''))
    lines = [l for l in out.stdout.splitlines() if l.strip()]
    c = collections.Counter()
    for l in lines:
        p = l.split(':', 1)[0]
        c[bucket(p)] += 1
    print('TOKEN', t, 'TOTAL', len(lines), dict(c))
    for l in lines:
        p = l.split(':', 1)[0]
        print('   ', bucket(p), l[:190])
    print()
