"""W5 轴最终吞错普查（纯静态、只读；口径见 _verify/W5_error_diagnostics.md §〇）。
用法: python3 -B 问题扫描/_verify/scripts_w5_census_final.py
"""
import subprocess, re, os, collections

ROOT = '/workspace/Astro CS Database'
files = subprocess.run(['git','--no-optional-locks','ls-files'], cwd=ROOT,
                       capture_output=True, text=True).stdout.splitlines()
CPP = ('.cpp','.h','.hpp','.c')
PROD = ('lib/','cli/','include/','runtime/','providers/','modules/')
VEND = ('/archive/','/legacy/','nanoflann','/browser_qt/','third_party')

prod = [f for f in files if f.endswith(CPP) and f.startswith(PROD)]
core = [f for f in prod if not any(v in f for v in VEND)]
acr  = [f for f in core if f.startswith('lib/acr/')]
nonacr = [f for f in core if not f.startswith('lib/acr/')]
pypy  = [f for f in files if f.endswith('.py') and f.startswith(('tools/','ci/','scripts/'))]
tests = [f for f in files if f.endswith(CPP) and f.startswith('tests/')]
print('CORPUS prod_cpp=%d core_cpp=%d core_acr=%d core_nonacr=%d facade_py=%d test_cpp=%d'
      % (len(prod), len(core), len(acr), len(nonacr), len(pypy), len(tests)))

BLOCK = re.compile(r'/\*.*?\*/', re.S)
LINE = re.compile(r'//[^\n]*')
def strip(t):
    prev = None
    while prev != t:
        prev = t
        t = LINE.sub(' ', t)
        t = BLOCK.sub(' ', t)
    return t

def read(f):
    try:
        return open(os.path.join(ROOT, f), encoding='utf-8', errors='replace').read()
    except OSError:
        return ''

def lineno(raw, pos):
    return raw.count('\n', 0, pos) + 1

CATCH = re.compile(r'\bcatch\s*\(([^)]*)\)\s*\{')
s1, s2, s2b, total = [], [], [], 0
for f in core:
    raw = read(f); t = strip(raw)
    for m in CATCH.finditer(t):
        total += 1
        i = m.end(); depth = 1; j = i
        while j < len(t) and depth:
            if t[j] == '{': depth += 1
            elif t[j] == '}': depth -= 1
            j += 1
        body = t[i:j-1].strip()
        ln = lineno(t, m.start())
        if body == '':
            s1.append('%s:%d' % (f, ln))
        elif re.search(r'\breturn\b|\bthrow\b|\bbreak\b|\bcontinue\b|=\s*(false|true|-1|0)|fail\(|error\s*=|status\s*=|mismatch|verdict|ok\s*=\s*false', body):
            (s2 if re.search(r'log|LOG|fprintf|cerr|warn|WARN', body) else s2b).append('%s:%d' % (f, ln))
        else:
            s1.append('%s:%d(疑似空)' % (f, ln))
print('CATCH total=%d S1_empty=%d S2_log_only=%d S2b_other=%d' % (total, len(s1), len(s2), len(s2b)))
print('S1_LIST ' + ' '.join(s1))
print('S1_nonacr=' + str(len([x for x in s1 if '/acr/' not in x])))

# Python facade: except blocks whose body is pass/continue only; bare except
exc_total = exc_silent = bare = 0
bare_list = []
silent_list = []
for f in pypy:
    lines = read(f).splitlines()
    for k, ln in enumerate(lines):
        m = re.match(r'^(\s*)except\b(.*)$', ln)
        if not m:
            continue
        exc_total += 1
        if re.match(r'^except\s*:', m.group(2).strip() + ':') or m.group(2).strip().startswith(':'):
            bare += 1; bare_list.append('%s:%d' % (f, k + 1))
        ind = len(m.group(1))
        body = []
        for n in lines[k+1:]:
            if not n.strip():
                continue
            if len(n) - len(n.lstrip()) <= ind:
                break
            body.append(n.strip())
        if body and all(b in ('pass', 'continue') or b.startswith('pass ') for b in body):
            exc_silent += 1; silent_list.append('%s:%d' % (f, k + 1))
print('PY except=%d pass_or_continue_only=%d bare_except=%d' % (exc_total, exc_silent, bare))
print('PY_BARE_LIST ' + ' '.join(bare_list))

# ignored return values: bare calls to known status-returning APIs (statement-only lines)
IGNORE_NAMES = r'(create_directories|create_directory|rename|remove|remove_all|copy_file|fsync|CloseHandle|_mkdir|mkdir|WriteFile|ReadFile|fclose|remove_all|fflush|wait_all|join|detach|push_back)'
ign = 0; ign_list = []
for f in nonacr:
    for k, ln in enumerate(strip(read(f)).splitlines()):
        s = ln.strip()
        if re.fullmatch(r'\w[\w:<>\(\)]*::' + IGNORE_NAMES + r'\([^;]*\);', s) or \
           re.fullmatch(r'(std::)?[\w:]*' + IGNORE_NAMES + r'\([^;]*\);', s):
            if s.startswith(('return ', 'if ', 'while ')):
                continue
            ign += 1; ign_list.append('%s:%d' % (f, k + 1))
print('S3_ignored_return_stmts=%d' % ign)

vc = 0; vclist = []
for f in nonacr:
    for k, ln in enumerate(strip(read(f)).splitlines()):
        if '(void)' in ln and '(' in ln:
            vc += 1; vclist.append('%s:%d|%s' % (f, k + 1, ln.strip()[:60]))
print('S5_void_cast=%d' % vc)
print('S5_LIST ' + ' ; '.join(vclist[:20]))

nod = sum(strip(read(f)).count('[[nodiscard]]') for f in core)
asrt = 0; asrt_list = []
for f in nonacr:
    for k, ln in enumerate(strip(read(f)).splitlines()):
        if re.match(r'^\s*assert\s*\(', ln):
            asrt += 1; asrt_list.append('%s:%d' % (f, k + 1))
print('ASSERT_in_nonacr=%d %s  NODEISCARD_in_core=%d  STD_IGNORE=%d'
      % (asrt, ' '.join(asrt_list), nod, sum(strip(read(f)).count('std::ignore') for f in core)))

ret = 0; ret_by_dir = collections.Counter()
for f in core:
    for ln in strip(read(f)).splitlines():
        if re.search(r'\b(retry|retries|attempt|attempts|backoff)\b', ln, re.I):
            ret += 1; ret_by_dir[f.split('/')[1] if '/' in f else f] += 1
print('RETRY_lines=%d by_dir=%s' % (ret, dict(ret_by_dir)))

# named error-space tables (num literal definitions per prefix)
prefixes = ['ACS_ERR_','AIO_ERR_','AIO_PUBLISH_ERR_','ACS_LOADER_EC_','ACS_REG_EC_',
            'HIO_ERR_','ACS_DIAG_ECODE_','P3_RS_','SDET_ERR_','DPSF_ERR_','ACS_FIO_ERR_']
cnt = collections.Counter()
for f in core + [x for x in files if x.startswith('include/') and x.endswith(CPP)]:
    for ln in read(f).splitlines():
        for p in prefixes:
            for m in re.finditer(p + r'[A-Z0-9_]+\s*=\s*(\d+)', ln):
                cnt[p] += 1
print('CODE_TABLE_LITERAL_DEFS ' + ' '.join('%s=%d' % (k, v) for k, v in sorted(cnt.items())))

