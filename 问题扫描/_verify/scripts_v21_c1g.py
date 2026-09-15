
import re, collections
files=[l.strip() for l in open('问题扫描/_cache/_v21_src.txt',encoding='utf-8') if l.strip()]
prod=[f for f in files if not f.startswith('tests/') and not f.startswith('tools/') and 'acr/' not in f]
# pattern:  x = fn(...); ... continue;  where x unused; or goto fail with rc not set
pat_cont = re.compile(r'^\s*(?:[a-zA-Z_][\w:<>,\s*&]*\s+)?(\w+)\s*=\s*[^;]+;\s*$')
rows=[]
for f in prod:
    try: txt=open(f,encoding='utf-8',errors='replace').read()
    except Exception: continue
    lines=txt.split('\n')
    for i,L in enumerate(lines):
        m=pat_cont.match(L)
        if not m: continue
        v=m.group(1)
        if v in ('i','j','k','n','rc','st','ret','err','size','len','off','pos','idx'): 
            pass
        # look at next 8 lines: if the first use of v is inside a 'continue'/'goto' guard, flag
        nxt=lines[i+1:i+9]
        joined='\n'.join(nxt)
        if re.search(r'\b'+v+r'\b', joined):
            first=[j for j,s in enumerate(nxt) if re.search(r'\b'+v+r'\b', s)]
            if first:
                line=nxt[first[0]]
                if re.search(r'continue|goto', line) and not re.search(r'if\s*\(', line):
                    rows.append((f,i+1,L.strip()[:110],line.strip()[:90]))
print('assign-then-unconditional continue/goto (candidates):',len(rows))
for a,b,c,d in rows[:25]: print(a+':'+str(b), '|', c, '||', d)
print()
print('=== pattern: goto fail without setting st/rc (rc reuse) ===')
rows2=[]
for f in prod:
    try: lines=open(f,encoding='utf-8',errors='replace').read().split('\n')
    except Exception: continue
    for i,L in enumerate(lines,1):
        if re.search(r'\bgoto\s+\w*(fail|err|error|bad|out)\w*\s*;', L):
            ctx='\n'.join(lines[max(0,i-7):i-1])
            if not re.search(r'\b(rc|st|status|ret|rv|err_code)\s*=', ctx) and '!=' not in L and '==' not in L:
                rows2.append((f,i,L.strip()[:90]))
print('goto-fail sites with no rc assignment in prior 6 lines:',len(rows2))
for a,b,c in rows2[:20]: print(a+':'+str(b), c)
