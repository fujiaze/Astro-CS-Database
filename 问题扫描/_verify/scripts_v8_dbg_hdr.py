
import json, os, glob
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
                if not key: continue
                kv[key]=c[8:80].rstrip()
    return kv
d=json.load(open('testdata/index.json', encoding='utf-8'))
for ds in d['datasets'][:2]:
    ld=ds['lights_dir']
    fs=sorted(glob.glob(os.path.join(ld,'**','*.fts'), recursive=True))
    p=fs[0]
    kv=read_header(p)
    print('==', os.path.basename(p))
    for k in ['OBJCTRA','OBJCTDEC','CRVAL1','CRVAL2','FOCALLEN','XPIXSZ','RA','DEC','PLTSOLVD','CTYPE1']:
        print('   ', k, '=', repr(kv.get(k)))
    print('   all keys sample:', sorted(kv)[:25])
