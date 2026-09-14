
import json, os, glob, math, struct

def read_header(path, max_blocks=200):
    kv={}
    try:
        with open(path,'rb') as f:
            for _ in range(max_blocks):
                blk=f.read(2880)
                if len(blk)<2880: break
                for i in range(0,2880,80):
                    c=blk[i:i+80].decode('latin-1')
                    key=c[:8].strip()
                    if key=='END': return kv, True
                    if not key or key==' ' or c[8:10]!='= ': continue
                    val=c[10:80]
                    if val[:1].isdigit() or val[:1] in '-+.T F':
                        num=val.split('/')[0].strip()
                        try: kv[key]=float(num)
                        except Exception: kv[key]=num
                    else:
                        kv[key]=val.strip("' ").strip()
        return kv, False
    except Exception as e:
        return {'_err':str(e)}, False

def angsep(r1,d1,r2,d2):
    r1,d1,r2,d2=[math.radians(x) for x in (r1,d1,r2,d2)]
    cosd=math.sin(d1)*math.sin(d2)+math.cos(d1)*math.cos(d2)*math.cos(r1-r2)
    return math.degrees(math.acos(max(-1.0,min(1.0,cosd))))

def ra_to_deg(v):
    return v*15.0

d=json.load(open('testdata/index.json', encoding='utf-8'))
missing_crval=[]; missing_keys=[]; sep_max=(0,None); n=0; bad=0
xpixsz_range=[1e9,-1e9]; focallen_range=[1e9,-1e9]
per_ds_missing={}
for ds in d['datasets']:
    ld=ds.get('lights_dir') or ''
    fs=sorted(glob.glob(os.path.join(ld,'**','*.fts'), recursive=True))
    for i,p in enumerate(fs):
        kv,ok=read_header(p)
        if not ok or '_err' in kv: bad+=1; continue
        n+=1
        need=['FOCALLEN','XPIXSZ','OBJCTRA','OBJCTDEC']
        miss=[k for k in need if k not in kv]
        if miss:
            missing_keys.append((ds['id'],i,miss,os.path.basename(p)))
        if 'CRVAL1' not in kv or 'CRVAL2' not in kv:
            missing_crval.append((ds['id'],i,os.path.basename(p)))
            per_ds_missing[ds['id']]=per_ds_missing.get(ds['id'],0)+1
        if 'XPIXSZ' in kv:
            xpixsz_range[0]=min(xpixsz_range[0],kv['XPIXSZ']); xpixsz_range[1]=max(xpixsz_range[1],kv['XPIXSZ'])
        if 'FOCALLEN' in kv:
            focallen_range[0]=min(focallen_range[0],kv['FOCALLEN']); focallen_range[1]=max(focallen_range[1],kv['FOCALLEN'])
        if all(k in kv for k in ['CRVAL1','CRVAL2','OBJCTRA','OBJCTDEC']):
            try:
                s=angsep(ra_to_deg(kv['OBJCTRA']),kv['OBJCTDEC'],kv['CRVAL1'],kv['CRVAL2'])
                if s>sep_max[0]: sep_max=(s,(ds['id'],i,os.path.basename(p),kv['CRVAL1'],kv['CRVAL2'],kv['OBJCTRA'],kv['OBJCTDEC']))
            except Exception: pass
print('scanned', n, 'err', bad)
print('missing header-WCS keys (FOCALLEN/XPIXSZ/OBJCTRA/OBJCTDEC):', len(missing_keys))
for m in missing_keys[:10]: print('  ', m)
print('frames WITHOUT CRVAL1/2:', len(missing_crval), 'per dataset:', per_ds_missing)
print('max |CRVAL - OBJCT| sep deg:', round(sep_max[0],5), sep_max[1])
print('XPIXSZ range', xpixsz_range, 'FOCALLEN range', focallen_range)
