
import re, collections
files = [l.strip() for l in open('问题扫描/_cache/_v21_src.txt', encoding='utf-8') if l.strip()]
prod = [f for f in files if not f.startswith('tests/') and not f.startswith('tools/')]
# Result<T> style: find Result< usages and calls assigned but never checked
res_decl = re.compile(r'\b(?:Result|StatusOr|ErrorOr)\s*<[^>]{0,60}>\s+(\w+)\s*=')
check = re.compile(r'\.(ok|is_ok|is_err|has_value|value_or|status|Err|err)\b|\bif\s*\(\s*!?(\1)\b|return\s+\1\b|\b(\1)\s*[?)]|\b(\1)\s*&&|\b(\1)\s*\|\|')
rows=[]
for f in prod:
    try: lines=open(f,encoding='utf-8',errors='replace').read().split('\n')
    except Exception: continue
    for i,L in enumerate(lines,1):
        m=res_decl.search(L)
        if not m: continue
        v=m.group(1)
        used=False
        for j in range(i, min(i+40,len(lines))):
            if re.search(r'\b'+v+r'\b\s*\.', lines[j]) or re.search(r'\b(if|while)\s*\([^)]*\b'+v+r'\b', lines[j]) or re.search(r'\breturn\b[^;]*\b'+v+r'\b', lines[j]) or re.search(r'\b'+v+r'\b\s*[=!]', lines[j]) or re.search(r'\b'+v+r'\b\s*[,)]', lines[j]):
                used=True; break
        if not used: rows.append((f,i,L.strip()[:140]))
print('Result<T> bound but no visible check within 40 lines:', len(rows))
for f,i,s in rows[:40]: print(f+':'+str(i), s)
print()
print('=== discarded-call statements of qualified functions (ns::fn(...);) ===')
qs = re.compile(r'^\s{0,8}([A-Za-z_][\w:]*::[A-Za-z_~][\w:]*(?:<[^>]*>)?)\s*\([^;]*\)\s*;\s*$')
cnt=collections.Counter(); ex=collections.defaultdict(list)
for f in prod:
    try: lines=open(f,encoding='utf-8',errors='replace').read().split('\n')
    except Exception: continue
    for i,L in enumerate(lines,1):
        s=L.strip()
        if s.startswith('//') or s.startswith('*'): continue
        m=qs.match(L)
        if m:
            cnt[m.group(1)]+=1
            if len(ex[m.group(1)])<6: ex[m.group(1)].append((f,i,s[:120]))
for n,c in cnt.most_common(40):
    print(n, c, ex[n][0][0]+':'+str(ex[n][0][1]))
