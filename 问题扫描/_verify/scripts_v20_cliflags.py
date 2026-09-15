
import re
src=open('cli/commands.cpp',encoding='utf-8',errors='replace').read().split('\n')
fn=None
reads={}
cur_args={}
for i,l in enumerate(src,1):
    m=re.match(r'^(?:static\s+)?int\s+(cmd_\w+|run_with_resource_gate|dispatch)\s*\(', l)
    if m: fn=m.group(1); cur_args.setdefault(fn,[])
    if fn:
        for mm in re.finditer(r'(values|flags)\.(?:count|at|find|insert|erase)\(\s*"([^"]+)"', l):
            cur_args.setdefault(fn,set()).add(mm.group(2))
        mm2=re.match(r'^\s*\(void\)(p|args)\s*;', l)
        if mm2: cur_args.setdefault(fn,set()).add('<<VOID_'+mm2.group(1)+'>>')
for k,v in sorted(cur_args.items()):
    print(k, sorted(v))
