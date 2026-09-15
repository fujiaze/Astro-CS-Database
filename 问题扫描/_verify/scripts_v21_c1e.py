
import re, collections
files = [l.strip() for l in open('问题扫描/_cache/_v21_src.txt', encoding='utf-8') if l.strip()]
prod = [f for f in files if not f.startswith('tests/') and not f.startswith('tools/')]
# stdio / fs calls whose return value matters
names = ['fwrite','fread','fclose','fflush','remove','rename','fputs','fprintf','printf','mkdir','rmdir','unlink','fseek','fseeko','_fseeki64','ftell','fflush','tmpfile','setvbuf','ferror','feof','freopen','chdir','system','qsort','snprintf','fchmod','fsync','close','write','read']
stmt = re.compile(r'^\s*('+ '|'.join(names) + r')\s*\(')
rows=collections.Counter(); ex=collections.defaultdict(list)
for f in prod:
    try: lines=open(f,encoding='utf-8',errors='replace').read().split('\n')
    except Exception: continue
    for i,L in enumerate(lines,1):
        s=L.strip()
        if s.startswith('//') or s.startswith('*'): continue
        m=stmt.match(L)
        if m and s.endswith(';'):
            rows[m.group(1)]+=1
            if len(ex[m.group(1)])<12: ex[m.group(1)].append((f,i,s[:120]))
print('=== discarded-return stdio/fs call statements (prod) ===')
for n,c in rows.most_common():
    print(n, c)
print()
for n in ['remove','rename','fclose','fflush','fseek','fseeko','_fseeki64','qsort','mkdir','rmdir','fsync','system','fchmod','unlink']:
    for f,i,s in ex.get(n,[]):
        print(n, '->', f+':'+str(i), s)
