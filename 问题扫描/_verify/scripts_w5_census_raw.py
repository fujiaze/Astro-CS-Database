"""W5 轴权威吞错普查（纯静态、只读）。

与 scripts_w5_sweep4.py 的区别：本脚本在**原始文本**上定位 catch 站点并报**原始行号**
（sweep4 先剥注释再数行，含块注释的文件行号整体前移，不可作锚点）；
分类判据只看"注释与字符串已掩码"的等长掩码串，故不会把注释里的 return/throw 误判为控制流。

用法: python3 -B 问题扫描/_verify/scripts_w5_census_raw.py
口径: W5-A/B（git ls-files；prod C/C++ 前缀 lib/ cli/ include/ runtime/ providers/ modules/；
      排除 vendored 子串 /archive/ /legacy/ nanoflann /browser_qt/ third_party）
"""
import subprocess, re, os, collections

ROOT = '/workspace/Astro CS Database'
files = subprocess.run(['git', '--no-optional-locks', 'ls-files'], cwd=ROOT,
                       capture_output=True, text=True).stdout.splitlines()
CPP = ('.cpp', '.h', '.hpp', '.c')
PROD = ('lib/', 'cli/', 'include/', 'runtime/', 'providers/', 'modules/')
VEND = ('/archive/', '/legacy/', 'nanoflann', '/browser_qt/', 'third_party')

prod = [f for f in files if f.endswith(CPP) and f.startswith(PROD)]
core = [f for f in prod if not any(v in f for v in VEND)]
acr = [f for f in core if f.startswith('lib/acr/')]
nonacr = [f for f in core if not f.startswith('lib/acr/')]


def read(f):
    try:
        return open(os.path.join(ROOT, f), encoding='utf-8', errors='replace').read()
    except OSError:
        return ''


def mask(src):
    """等长掩码：注释内容->空格，字符串字面量内容->点；保留换行与全部位置。"""
    out = list(src)
    i, n = 0, len(src)
    while i < n:
        c = src[i]
        if c == '/' and i + 1 < n and src[i + 1] == '/':
            j = src.find('\n', i)
            j = n if j < 0 else j
            for k in range(i, j):
                out[k] = ' '
            i = j
        elif c == '/' and i + 1 < n and src[i + 1] == '*':
            j = src.find('*/', i + 2)
            j = n if j < 0 else j + 2
            for k in range(i, j):
                if out[k] != '\n':
                    out[k] = ' '
            i = j
        elif c in '"\'':
            q, j = c, i + 1
            while j < n:
                if src[j] == '\\':
                    j += 2
                    continue
                if src[j] == q:
                    j += 1
                    break
                if src[j] == '\n' and q == "'":
                    break
                j += 1
            for k in range(i + 1, min(j - 1, n)):
                if out[k] != '\n':
                    out[k] = '.'
            i = j
        else:
            i += 1
    return ''.join(out)


CATCH = re.compile(r'\bcatch\s*\(([^)]*)\)')
FLOW = re.compile(r'\b(return|throw|break|continue|goto)\b|=\s*(false|true|nullptr|-1)\b'
                  r'|\bfail\(|\bseterr|\bstore\(|\bfetch_add\(|\+=|-=|error\s*=|err\s*=|'
                  r'status\s*=|ok\s*=|mismatch|verdict|last_status\s*=')

buckets = collections.defaultdict(list)
total = 0
for f in core:
    src = read(f)
    m = mask(src)
    for mo in CATCH.finditer(m):
        total += 1
        ln = src[:mo.start()].count('\n') + 1
        i = mo.end()
        while i < len(m) and m[i] in ' \t\r\n':
            i += 1
        if i >= len(m) or m[i] != '{':
            buckets['X_NO_BRACE'].append((f, ln, ''))
            continue
        depth, j = 0, i
        while j < len(m):
            if m[j] == '{':
                depth += 1
            elif m[j] == '}':
                depth -= 1
                if depth == 0:
                    break
            j += 1
        body_code = re.sub(r'\s+', ' ', m[i + 1:j]).strip()
        if body_code == '':
            tag = 'E0_EMPTY_OR_COMMENT_ONLY'
        elif not FLOW.search(body_code):
            tag = 'E2_NO_CONTROLFLOW'
        else:
            tag = 'P_PROPAGATES'
        buckets[tag].append((f, ln, body_code[:80]))

print('CORPUS core=%d acr=%d nonacr=%d  CATCH_TOTAL=%d' % (len(core), len(acr), len(nonacr), total))
for k in ('E0_EMPTY_OR_COMMENT_ONLY', 'E2_NO_CONTROLFLOW', 'P_PROPAGATES', 'X_NO_BRACE'):
    v = buckets.get(k, [])
    na = [x for x in v if not x[0].startswith('lib/acr/')]
    print('%-24s total=%-4d non_acr=%-4d' % (k, len(v), len(na)))
print()
for k in ('E0_EMPTY_OR_COMMENT_ONLY', 'E2_NO_CONTROLFLOW'):
    print('==== %s ====' % k)
    for f, ln, frag in sorted(buckets.get(k, []), key=lambda x: (x[0], x[1])):
        print('  %s:%d | %s' % (f, ln, frag))
print()
print('==== 非 ACR E0 锚点清单 ====')
for f, ln, frag in sorted(buckets.get('E0_EMPTY_OR_COMMENT_ONLY', []), key=lambda x: (x[0], x[1])):
    if not f.startswith('lib/acr/'):
        print('  %s:%d' % (f, ln))
