
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
                kv[key]=c[10:80].rstrip()
    return kv

def parse_num(v, hours=False):
    if v is None: return None
    s=v.strip().strip("'").strip()
    if not s: return None
    if ':' in s or ' ' in s:
        parts=s.replace(' ',':').split(':')
        try: parts=[float(x) for x in parts if x!='']
        except Exception: return None
        x=parts[0]+ (parts[1]/60 if len(parts)>1 else 0) + (parts[2]/3600 if len(parts)>2 else 0)
        neg = s.startswith('-')
        return -x if neg else x
    try: return float(s)
    except Exception: return None

def angsep(r1,d1,r2,d2):
    r1,d1,r2,d2=[math.radians(x) for x in (r1,d1,r2,d2)]
    c=math.sin(d1)*math.sin(d2)+math.cos(d1)*math.cos(d2)*math.cos(r1-r2)
    return math.degrees(math.acos(max(-1.0,min(1.0,c))))

d=json.load(open('testdata/index.json', encoding='utf-8'))
rows=[]
for ds in d['datasets']:
    ld=ds.get('lights_dir') or ''
    fs=sorted(glob.glob(os.path.join(ld,'**','*.fts'), recursive=True))
    for i,p in enumerate(fs):
        kv=read_header(p)
        ra_s=kv.get('OBJCTRA'); dec_s=kv.get('OBJCTDEC')
        cra=parse_num(ra_s); cdec=parse_num(dec_s)
        v1=parse_num(kv.get('CRVAL1')); v2=parse_num(kv.get('CRVAL2'))
        if cra is None or cdec is None or v1 is None or v2 is None: continue
        # OBJCTRA: hours if raw has ':' or numeric <24 or unit hint; decide by magnitude
        ra_deg = cra*15.0 if (':' in str(ra_s)) or (abs(cra)<=24.0) else cra
        s=angsep(ra_deg,cdeg,v1,v2)
        rows.append((s, ds['id'], i, os.path.basename(p), ra_s, dec_s, kv.get('CRVAL1'), kv.get('CRVAL2')))
rows.sort(reverse=True)
print('comparable frames:', len(rows))
print('TOP 6 |CRVAL-OBJCT| deg:')
for r in rows[:6]:
    print('   %-8.5f  %s idx%d  RAraw=%s DECraw=%s CRVAL=(%s,%s)  %s' % (r[0], r[1], r[2], r[4], r[5], r[6], r[7], r[3][:44]))
n_gt001=sum(1 for r in rows if r[0]>0.01); n_gt01=sum(1 for r in rows if r[0]>0.1)
print('frames >0.01deg:', n_gt001, '  >0.1deg:', n_gt01)
