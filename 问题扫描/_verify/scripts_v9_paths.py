import json, os, re, ast, collections
d = json.load(open('ci/checks.json', encoding='utf-8'))
ch = d['checks']
def script_of(c):
    cmd=list(map(str,c['command']))
    if cmd[0]=='python3' and len(cmd)>1 and cmd[1]!='-m':
        i=1
        while i<len(cmd) and cmd[i].startswith('-'): i+=1
        return cmd[i] if i<len(cmd) else None
    return None
# find string literals in each checker that look like repo-relative paths
PATHRE = re.compile(r'^[A-Za-z0-9_][A-Za-z0-9_.\-/]*$')
def looks_path(s):
    if not s or ' ' in s: return False
    if not PATHRE.match(s): return False
    if '/' not in s: return False
    if s.startswith('http'): return False
    top = s.split('/')[0]
    return top in ('lib','include','cli','ci','tools','docs','tests','contracts','modules','runtime','providers','scripts','cmake','packaging','artifacts','evidence','testdata','reports','run','工程控制','问题扫描','design') or s in ('VERSION','CMakeLists.txt','AGENTS.md','README.md','memory.md','CHANGELOG.md','REVIEW.md','HANDOVER.md')
res = collections.defaultdict(list)
for c in ch:
    s = script_of(c)
    if not s or not os.path.exists(s): continue
    txt = open(s, encoding='utf-8', errors='replace').read()
    try: tree = ast.parse(txt)
    except SyntaxError as e:
        res[c['id']].append('<SYNTAX ERROR %s>'%e); continue
    for node in ast.walk(tree):
        if isinstance(node, ast.Constant) and isinstance(node.value, str):
            v = node.value.strip()
            if looks_path(v) and not os.path.exists(v):
                res[c['id']].append(v)
for k,v in res.items():
    print('%-30s %s' % (k, sorted(set(v))))
print('GATES WITH DANGLING LITERAL PATHS:', len(res))
