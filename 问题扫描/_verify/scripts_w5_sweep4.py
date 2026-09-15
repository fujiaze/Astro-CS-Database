"""W5 axis sweep: swallow-point classification census (read-only, static)."""
import subprocess, re, os, collections, json

ROOT = '/workspace/Astro CS Database'
files = subprocess.run(['git','--no-optional-locks','ls-files'], cwd=ROOT,
                       capture_output=True, text=True).stdout.splitlines()
CPP = ('.cpp','.h','.hpp','.c')
PROD_PREFIXES = ('lib/','cli/','include/','runtime/','providers/','modules/')
VENDORED = ('/archive/','/legacy/','nanoflann','/browser_qt/','third_party')

prod = [f for f in files if f.endswith(CPP) and f.startswith(PROD_PREFIXES)]
core = [f for f in prod if not any(v in f for v in VENDORED)]
acr  = [f for f in core if f.startswith('lib/acr/')]
nonacr = [f for f in core if not f.startswith('lib/acr/')]
print('SET prod_all=%d core(no-vendor,no-tests)=%d of_which_acr=%d non_acr=%d' % (len(prod), len(core), len(acr), len(nonacr)))

def load(f):
    p = os.path.join(ROOT, f)
    try:
        return open(p, encoding='utf-8', errors='replace').read()
    except Exception:
        return ''

COMMENT_BLOCK = re.compile(r'/\*.*?\*/', re.S)
COMMENT_LINE = re.compile(r'//[^\n]*')
def strip(text):
    return COMMENT_LINE.sub(' ', COMMENT_BLOCK.sub(' ', text))

CATCH = re.compile(r'\bcatch\s*\(([^)]*)\)')
def catch_sites(t):
    """yield (line, param, body) with brace matching on raw text."""
    for m in CATCH.finditer(t):
        i = m.end()
        while i < len(t) and t[i] in ' \t\r\n':
            i += 1
        if i >= len(t) or t[i] != '{':
            continue
        depth = 0; j = i
        while j < len(t):
            if t[j] == '{': depth += 1
            elif t[j] == '}':
                depth -= 1
                if depth == 0: break
            j += 1
        yield (t[:m.start()].count('\n')+1, m.group(1).strip(), t[i+1:j])

CTL = re.compile(r'\b(return|throw|break|continue|goto)\b')
LOGISH = re.compile(r'(aio_log|dpsf_log|_log\(|log_|LOG_|Log|warn|WARN|fprintf|printf|cerr|clog|spdlog|seterr|error|status|fail|record|note)')

S1, S2, S2b = [], [], []
for f in core:
    t = load(f)
    if 'catch' not in t: continue
    ts = strip(t)
    for (ln, param, body) in catch_sites(t):
        b = strip(body).strip()
        if b == '':
            S1.append((f, ln, param))
            continue
        if CTL.search(b):
            continue           # control flow preserved → not a swallow
        if LOGISH.search(b):
            S2.append((f, ln, param, body.strip().replace('\n',' ')[:160]))
        else:
            S2b.append((f, ln, param, body.strip().replace('\n',' ')[:160]))
print('S1_empty_catch=%d  S2_catch_log_only_no_controlflow=%d  S2b_catch_other=%d' % (len(S1), len(S2), len(S2b)))
for x in S1: print('  S1', x[0]+':'+str(x[1]), '|', x[2][:30])
print('--- S2 (log/record only, no return/throw) ---')
for x in S2: print('  S2', x[0]+':'+str(x[1]), '|', x[2][:24], '|', x[3][:110])
print('--- S2b (other) ---')
for x in S2b: print('  S2b', x[0]+':'+str(x[1]), '|', x[2][:24], '|', x[3][:110])

# ---- S4: JSON "missing key → in-place default" on production read paths
VAL = re.compile(r'\.value\(\s*"([^"]+)"\s*,')
VALUEOR = re.compile(r'\.value_or\(')
JGET = re.compile(r'\bjget(?:_ref|_str)?\s*\([^;]*,')
s4 = []
for f in nonacr:
    t = strip(load(f))
    a = VAL.findall(t); b = len(VALUEOR.findall(t)); c = len(JGET.findall(t))
    if a or b or c:
        s4.append((f, len(a), b, c))
print('S4 json_default: .value(k,dflt)=%d value_or=%d jget-with-dflt=%d files=%d' % (
    sum(x[1] for x in s4), sum(x[2] for x in s4), sum(x[3] for x in s4), len(s4)))
s4.sort(key=lambda x: -(x[1]+x[2]+x[3]))
for x in s4[:20]: print('   ', x)

# ---- S3: bare call statements of status-returning libc/syscalls (return discarded)
IGNORE = re.compile(r'^\s*(remove|rename|std::filesystem::remove|fs::remove|fclose|fflush|flush|'
                    r'std::filesystem::remove_all|std::filesystem::rename|std::filesystem::create_directories|'
                    r'mkdir|_mkdir|rmdir|unlink|WriteFile|CloseHandle|DeleteFileA|DeleteFileW|MoveFileExA|'
                    r'CreateDirectoryA|SetEndOfFile|CopyFileA|pthread_detach|fclose)\s*\(', re.M)
s3 = []
for f in nonacr:
    t = strip(load(f))
    for m in IGNORE.finditer(t):
        s3.append((f, t[:m.start()].count('\n')+1, m.group(1)))
print('S3_ignored_return(libc/fs side-effect calls as bare stmts)=%d' % len(s3))
for x in s3: print('   ', x[0]+':'+str(x[1]), '|', x[2])

# ---- int rc = ...; never read
s3b = []
for f in nonacr:
    t = strip(load(f))
    for m in re.finditer(r'\b(?:int|acs_status|AioStatus|Status|HRESULT|DWORD)\s+(\w*(?:rc|st|status|err|ret)\w*)\s*=\s*[^;]+;', t, re.I):
        name = m.group(1)
        after = t[m.end():m.end()+1200]
        if not re.search(r'\b'+re.escape(name)+r'\b', after):
            s3b.append((f, t[:m.start()].count('\n')+1, name))
print('S3b rc-assigned-but-never-read(window 1200 chars)=%d' % len(s3b))
for x in s3b: print('   ', x[0]+':'+str(x[1]), '|', x[2])

# ---- (void) explicit discards
s5 = []
for f in nonacr:
    t = strip(load(f))
    for m in re.finditer(r'\(\s*void\s*\)\s*\[?\s*([A-Za-z_][\w:]*)\s*\(', t):
        s5.append((f, t[:m.start()].count('\n')+1, m.group(1)))
print('S5_void_cast=%d' % len(s5))
for x in s5: print('   ', x[0]+':'+str(x[1]), '|', x[2])
