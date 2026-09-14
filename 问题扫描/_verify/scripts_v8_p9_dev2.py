
import json, os, glob, math

def read_header(path, max_blocks=300):
    kv={}
    with open(path,'rb') as f:
        for _ in range(max_blocks):
            blk=f.read(2880)
            if len(blk)<2880: break
            for i in range(0,2880,80):
                c=blk[i:i+80].decode('latin-1')
                key=c[:8].strip()
                if key=='END': return kv
                if not key or c[8:10]!='= ': continue
                v=c[10:80]
                j=v.find(' /')
                if j>=0 and v[0]=="'" and j> 0:
                    # only cut comment if outside quotes
                    if v[:j].count("'")%2==0: v=v[:j]
                elif j>=0 and v[0]!="'":
                    v=v[:j]
                kv[key]=v.strip()
    return kv

def parse_hms(s):
    if s is None: return None
    t=s.strip().strip("'").strip()
    if not t: return None
    neg=t.startswith('-')
    t=t.lstrip('+-')
    parts=t.replace(' ',':').split(':')
    try: parts=[float(x) for x in parts if x!='']
    except Exception: return None
    x=parts[0]+(parts[1]/60 if len(parts)>1 else 0)+(parts[2]/3600 if len(parts)>2 else 0)
    return -x if neg else x

def angsep(r1,d1,r2,d2):
    r1,d1,r2,d2=[math.radians(x) for x in (r1,d1,r2,d2)]
    c=math.sin(d1)*math.sin(d2)+math.cos(d1)*math.cos(d2)*math.cos(r1-r2)
    return math.degrees(math.acos(max(-1.0,min(1.0,c))))

d=json.load(open('testdata/index.json', encoding='utf-8'))
rows=[]
for ds in d['datasets']:
    fs=sorted(glob.glob(os.path.join(ds['lights_dir'],'**','*.fts'), recursive=True))
    for i,p in enumerate(fs):
        kv=read_header(p)
        ra=parse_hms(kv.get('OBJCTRA')); dec=parse_hms(kv.get('OBJCTDEC'))
        c1=parse_hms(kv.get('CRVAL1')); c2=parse_hms(kv.get('CRVAL2'))
        if None in (ra,dec,c1,c2): continue
        radeg = ra*15.0 if abs(ra)<=24.0 and ' ' in str(kv.get('OBJCTRA')) else ra
        rows.append((angsep(radeg,dec,c1,c2), ds['id'], i, os.path.basename(p)))
rows.sort(reverse=True)
print('comparable frames:', len(rows))
print('TOP 5 |CRVAL-OBJCT| deg:')
for r in rows[:5]: print('   %8.5f  %s idx%d  %s' % (r[0], r[1], r[2], r[3][:52]))
print('count >0.01:', sum(1 for r in rows if r[0]>0.01), ' >0.5:', sum(1 for r in rows if r[0]>0.5))
print('NGC55 idx561 row:', [ (round(x[0],5), x[1], x[2], x[3][:40]) for x in rows if x[1].startswith('NGC55') and x[2]==561 ])
print('NGC55 top3:', [ (round(x[0],5), x[2]) for x in rows if x[1].startswith('NGC55')][:3])
