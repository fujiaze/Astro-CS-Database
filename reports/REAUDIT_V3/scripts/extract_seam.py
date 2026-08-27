
import os, sys, math, struct, re, csv, json

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hpx

def fits_f32_data(path):
    # parse FITS header to find data offset; return (naxis1,naxis2,data_bytes_as_array)
    with open(path,'rb') as f:
        head = f.read(2880)
        bitpix=None; n1=None; n2=None
        off=2880
        while True:
            blk = head
            # we read headers in 2880 chunks; better read sequentially
            break
        # re-read properly
    with open(path,'rb') as f:
        cur=0
        nbytes=0
        while True:
            f.seek(cur)
            blk=f.read(2880)
            if len(blk)<2880: break
            for i in range(0,2880,80):
                card=blk[i:i+80].decode('ascii','replace')
                if card.startswith('BITPIX'):
                    bitpix=int(card.split('=')[1].split('/')[0].strip())
                elif card.startswith('NAXIS1'):
                    n1=int(card.split('=')[1].split('/')[0].strip())
                elif card.startswith('NAXIS2'):
                    n2=int(card.split('=')[1].split('/')[0].strip())
                elif card.startswith('END'):
                    off = cur+2880
                    f.seek(off)
                    nbytes = n1*n2*abs(bitpix)//8
                    data = f.read(nbytes)
                    return n1,n2,bitpix,data
            cur+=2880
    return n1,n2,bitpix,None

def parse_f32(data):
    # big-endian float32
    n = len(data)//4
    return struct.unpack('>%df' % n, data)

def tile_list(root, order):
    # walks <root>/signal/Norder<order>/... return [(K,parent,sigpath,supportpath)]
    sigbase = os.path.join(root,'signal'); supbase=os.path.join(root,'support')
    out=[]
    ndir=os.path.join(sigbase,'Norder%d'%order)
    if not os.path.isdir(ndir): return out
    for d in os.listdir(ndir):
        dd=os.path.join(ndir,d)
        if not os.path.isdir(dd): continue
        dirv=int(d[3:])
        for fn in os.listdir(dd):
            if not fn.startswith('Npix') or not fn.endswith('.fits'): continue
            npv=int(fn[4:-5])
            parent=dirv*10000+npv
            rels=os.path.join('signal','Norder%d'%order,d,fn)
            relu=os.path.join('support','Norder%d'%order,d,fn)
            out.append((order,parent,os.path.join(root,rels),os.path.join(root,relu)))
    return out

def main():
    root=sys.argv[1]; B=float(sys.argv[2]); out=sys.argv[3]
    maxw=float(sys.argv[4]) if len(sys.argv)>4 else 60.0
    stride=int(sys.argv[5]) if len(sys.argv)>5 else 2
    orders=sys.argv[6].split(',') if len(sys.argv)>6 else ['7']
    pixel_scale=3.220769  # arcsec/px from properties
    degperpx=pixel_scale/3600.0
    rows=[]
    for order in orders:
        K=int(order)
        tiles=tile_list(root,K)
        for (K2,parent,sigp,supp) in tiles:
            if not os.path.exists(sigp): continue
            r=fits_f32_data(sigp)
            if r[0] is None: continue
            n1,n2,bp,sdata=r
            if sdata is None: continue
            sig=parse_f32(sdata)
            sup_data=None
            if os.path.exists(supp):
                rs=fits_f32_data(supp)
                if rs[0] is not None and rs[2] is not None:
                    sup_data=parse_f32(rs[3])
            # decode pixels
            for row in range(0,512,stride):
                for col in range(0,512,stride):
                    fi=row*512+col
                    v=sig[fi]
                    su=(sup_data[fi] if sup_data is not None else 0.0)
                    # decode (x,y)
                    y=col; x=511-row
                    ra,dec=hpx.tile_pix2ang(K2,parent,x,y)
                    pos=(dec-B)/degperpx
                    if abs(pos)>maxw: continue
                    if math.isnan(v) or not math.isfinite(v):
                        value=''
                    else:
                        value=repr(round(v,7))
                    side='A' if pos>=0 else 'B'
                    rows.append((side,round(pos,3),value,round(su,5)))
    with open(out,'w',newline='',encoding='utf-8') as f:
        w=csv.writer(f)
        w.writerow(['side','pos','value','support'])
        for row in rows: w.writerow(row)
    print('K=%s rows=%d (A=%d B=%d) -> %s' % (orders,len(rows),sum(1 for r in rows if r[0]=='A'),sum(1 for r in rows if r[0]=='B'),out))
    # quick metric by tool
if __name__=='__main__':
    main()
