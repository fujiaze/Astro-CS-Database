
import re, collections, json
files = [l.strip() for l in open('问题扫描/_cache/_v21_src.txt', encoding='utf-8') if l.strip()]
# collect all identifier names that look like state bits / counters
pat = re.compile(r'\b([a-z][a-z0-9]*_(?:present|mode|status|ok|flag|flags|valid|available|enabled|active|count|n_|num_|rows|bytes|hits|misses|bad_[a-z_]+))\b')
pat2 = re.compile(r'\b((?:n|num|bad|skipped|dropped|rejected|failed|warning|error|total|valid|invalid|hit|miss|used|alloc|read|write)_[a-z0-9_]{2,}|[a-z0-9_]+_(?:count|counter|rows|bytes|frames|tiles|units|items|calls|ops|events|samples|records|entries|leaves|nodes|jobs|tasks|units))\b')
write_cnt = collections.Counter(); read_cnt = collections.Counter(); where = collections.defaultdict(list)
for f in files:
    try: txt = open(f, encoding='utf-8', errors='replace').read()
    except Exception: continue
    for i, L in enumerate(txt.split('\n'), 1):
        s = L.strip()
        comment = s.startswith('//') or s.startswith('*') or s.startswith('/*')
        for m in pat2.finditer(L):
            name = m.group(1)
            if comment: continue
            # detect write: name = ...  (not ==), or name++, name+=, memset(&name
            after = L[m.end():m.end()+3]
            before = L[max(0,m.start()-3):m.start()]
            iswrite = bool(re.match(r'\s*=(?!=)', L[m.end():])) or bool(re.match(r'\s*(\+\+|--|\+=|-=)', L[m.end():]))
            isaddr = bool(re.search(r'[&]\s*'+re.escape(name)+r'\b', L))
            if iswrite: write_cnt[name]+=1
            else: read_cnt[name]+=1
            if len(where[name])<200: where[name].append((f,i,'W' if iswrite else 'R'))
cands = sorted(set(write_cnt) & set(read_cnt))
dead = [(w,r,n) for n,w in write_cnt.items() for r in [read_cnt.get(n,0)] if r==0]
dead.sort(reverse=True)
print('=== WRITE-ONLY identifiers (read count == 0 across ALL corpus incl tests), top 60 ===')
for w,r,n in dead[:60]:
    print(f'{n}  W={w} R={r}')
print('=== WRITE-heavy with tiny reads (R<=2, W>=3) ===')
for n in cands:
    if read_cnt[n]<=2 and write_cnt[n]>=3:
        print(n, 'W=%d R=%d'%(write_cnt[n],read_cnt[n]))
