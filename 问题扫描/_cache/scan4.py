
import ast, re
files=[l.rstrip('\n') for l in open('问题扫描/_cache/scope_py.txt',encoding='utf-8') if l.strip()]
# find comparisons of a variable that looks like rc/ret/exit/returncode against 0
pat = re.compile(r'\b(rc|ret|returncode|exit_code|ec|code|status|res|result|r)\b\s*(==|!=|>|<)\s*\b0\b|\b0\b\s*(==|!=)\s*\b(rc|ret|returncode|exit_code|ec)\b')
hits=0
for f in files:
    try: lines=open(f,encoding='utf-8',errors='replace').read().splitlines()
    except Exception: continue
    for i,L in enumerate(lines,1):
        if pat.search(L):
            hits+=1
print('rc-vs-0 comparisons lines:', hits)
