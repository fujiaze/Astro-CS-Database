
import os, sys, csv
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hpx_np

def fits_f32(path):
    with open(path,'rb') as f:
        cur=0
        while True:
            f.seek(cur); blk=f.read(2880)
            if len(blk)<2880: break
            n1=None; n2=None; done=False
            for i in range(0,2880,80):
                card=blk[i:i+80].decode('ascii','replace')
                if card.startswith('NAXIS1'): n1=int(card.split('=')[1].split('/')[0].strip())
                elif card.startswith('NAXIS2'): n2=int(card.split('=')[1].split('/')[0].strip())
                elif card.startswith('END'): done=True
            if done:
                f.seek(cur+2880)
                data=f.read(n1*n2*4)
                return np.frombuffer(data, dtype='>f4')
            cur+=2880
    return None

def tile_list(root, order):
    sigbase=os.path.join(root,'signal'); ndir=os.path.join(sigbase,'Norder%d'%order)
    out=[]
    if not os.path.isdir(ndir): return out
    for d in os.listdir(ndir):
        dd=os.path.join(ndir,d)
        if not os.path.isdir(dd): continue
        dirv=int(d[3:])
        for fn in os.listdir(dd):
            if not fn.startswith('Npix') or not fn.endswith('.fits'): continue
            npv=int(fn[4:-5]); parent=dirv*10000+npv
            out.append((order,parent,os.path.join(root,'signal','Norder%d'%order,d,fn),
                        os.path.join(root,'support','Norder%d'%order,d,fn)))
    return out

def main():
    root=sys.argv[1]; B=float(sys.argv[2]); out=sys.argv[3]
    maxw=float(sys.argv[4]) if len(sys.argv)>4 else 60.0
    stride=int(sys.argv[5]) if len(sys.argv)>5 else 2
    orders=sys.argv[6].split(',') if len(sys.argv)>6 else ['7']
    degperpx=3.220769/3600.0
    rowsA=[]; rowsB=[]
    for order in orders:
        K=int(order); tiles=tile_list(root,K)
        for (K2,parent,sigp,supp) in tiles:
            if not os.path.exists(sigp): continue
            sig=fits_f32(sigp)
            if sig is None: continue
            sup=fits_f32(supp) if os.path.exists(supp) else None
            rc=np.arange(0,512,stride)
            row,col=np.meshgrid(rc,rc,indexing='ij')
            row=row.ravel(); col=col.ravel()
            x=(511-row).astype(np.int64); y=col.astype(np.int64)
            ra,dec=hpx_np.tile_pix2ang(K2,parent,x,y)
            pos=(dec-B)/degperpx
            m=np.abs(pos)<=maxw
            if not m.any(): continue
            fi=(row*512+col)[m]
            v=sig[fi].astype(np.float64)
            su=(sup[fi].astype(np.float64) if sup is not None else np.zeros(int(m.sum())))
            side=np.where(pos[m]>=0,'A','B')
            posm=pos[m]
            for k in range(int(m.sum())):
                val='' if (not np.isfinite(v[k])) else '%.7f'%v[k]
                rec=(side[k], round(float(posm[k]),3), val, round(float(su[k]),5))
                if side[k]=='A': rowsA.append(rec)
                else: rowsB.append(rec)
            print('  tile K%d p=%d done (cum A=%d B=%d)' % (K2,parent,len(rowsA),len(rowsB)))
    rows=rowsA+rowsB
    with open(out,'w',newline='',encoding='utf-8') as f:
        w=csv.writer(f); w.writerow(['side','pos','value','support'])
        for row in rows: w.writerow(row)
    print('RESULT orders=%s rows=%d (A=%d B=%d) -> %s' % (orders,len(rows),len(rowsA),len(rowsB),out))
if __name__=='__main__':
    main()
