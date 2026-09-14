import json, os, re, ast, sys
d = json.load(open('ci/checks.json', encoding='utf-8'))
ch = d['checks']
def script_of(c):
    cmd=list(map(str,c['command']))
    if cmd[0]=='python3' and len(cmd)>1 and cmd[1]!='-m':
        i=1
        while i<len(cmd) and cmd[i].startswith('-'): i+=1
        return cmd[i] if i<len(cmd) else None
    return None
out=[]
for c in ch:
    s = script_of(c)
    if not s or not s.endswith('.py'): continue
    if not os.path.exists(s):
        out.append((c['id'], s, 'MISSING-SCRIPT')); continue
    txt = open(s, encoding='utf-8', errors='replace').read()
    nexit = len(re.findall(r'sys\.exit\(', txt))
    nex1  = len(re.findall(r'sys\.exit\(\s*1', txt))
    nret1 = len(re.findall(r'return\s+1\b', txt))
    nret0 = len(re.findall(r'return\s+0\b', txt))
    has_main = 'if __name__' in txt
    # last return in main body
    out.append((c['id'], s, 'exit=%d exit1=%d ret1=%d ret0=%d main=%s lines=%d' % (nexit,nex1,nret1,nret0,has_main, txt.count(chr(10)))))
for o in out:
    print('%-30s %-58s %s' % o)
print('TOTAL py-gates', len(out))
