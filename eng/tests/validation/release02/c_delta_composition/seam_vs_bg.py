
import numpy as np, json
from astropy.io import fits
ROOT='/workspace/Astro CS Database'
P={'OLD':ROOT+'/run/RELEASE-02/L4-rebuild/p3_r_vis/output_phase3.fits',
   'NEW':ROOT+'/run/RELEASE-02/L4-rebuild/p3_upmfix/output_phase3.fits'}
D={}
for lbl,p in P.items():
    with fits.open(p,memmap=False) as h: D[lbl]=np.asarray(h[0].data,dtype=np.float64)
def Jx(d,x0,ylo,yhi,K=25,gap=1):
    cL=np.arange(x0-K,x0-gap); cR=np.arange(x0+gap+1,x0+K+1)
    L=d[ylo:yhi,cL[0]:cL[-1]+1]; R=d[ylo:yhi,cR[0]:cR[-1]+1]
    ok=np.isfinite(L).all(1)&np.isfinite(R).all(1)
    if ok.sum()<5: return np.nan
    L=L[ok];R=R[ok]
    AL=np.vstack([np.ones_like(cL,float),cL]).T; AR=np.vstack([np.ones_like(cR,float),cR]).T
    eL=L@np.linalg.pinv(AL).T; eR=R@np.linalg.pinv(AR).T
    return float(np.median((eR[:,0]+eR[:,1]*x0)-(eL[:,0]+eL[:,1]*x0)))
def Jy(d,y0,xlo,xhi,K=25,gap=1):
    rL=np.arange(y0-K,y0-gap); rR=np.arange(y0+gap+1,y0+K+1)
    L=d[rL[0]:rL[-1]+1,xlo:xhi]; R=d[rR[0]:rR[-1]+1,xlo:xhi]
    ok=np.isfinite(L).all(0)&np.isfinite(R).all(0)
    if ok.sum()<5: return np.nan
    L=L[:,ok];R=R[:,ok]
    AL=np.vstack([np.ones_like(rL,float),rL]).T; AR=np.vstack([np.ones_like(rR,float),rR]).T
    eL=L.T@np.linalg.pinv(AL).T; eR=R.T@np.linalg.pinv(AR).T
    return float(np.median((eR[:,0]+eR[:,1]*y0)-(eL[:,0]+eL[:,1]*y0)))
def localbg(d,x,y,w=60):
    s=d[max(0,y-w):y+w,max(0,x-w):x+w]; s=s[np.isfinite(s)]
    return float(np.median(s)) if s.size else np.nan
# vertical boundary near x=2145: step and local bg vs y
res={}
for lbl in ('OLD','NEW'):
    d=D[lbl]; rows=[]
    for y in range(500,3401,25):
        best=(0,None)
        for xx in range(2145-20,2145+21):
            j=Jx(d,xx,y-50,y+50)
            if np.isfinite(j) and abs(j)>abs(best[0]): best=(j,xx)
        bg=localbg(D['OLD'],best[1] if best[1] else 2145,y)
        if np.isfinite(bg) and abs(bg)>1e11: rows.append((bg,best[0],y,best[1]))
    rows=np.array(rows)
    if len(rows)<10: print(lbl,'too few'); continue
    a,b=np.polyfit(rows[:,0],rows[:,1],1); r=np.corrcoef(rows[:,0],rows[:,1])[0,1]
    print('%s vertical x~2145: n=%d  step vs bg: slope %.5f  intercept %+.3g  corr %+.3f'%(lbl,len(rows),a,b,r))
    print('   bg range %.3g..%.3g (x%.1f) ; step med %+.3g ; step/bg med %.4f%%'%(rows[:,0].min(),rows[:,0].max(),rows[:,0].max()/rows[:,0].min(),np.median(rows[:,1]),100*np.median(rows[:,1]/rows[:,0])))
    res[lbl+'_v']=rows.tolist()
# horizontal boundary near y=1185 and y=1392
for nom in (1185,1392):
    for lbl in ('OLD','NEW'):
        d=D[lbl]; rows=[]
        for x in range(700,2001,25):
            best=(0,None)
            for yy in range(nom-20,nom+21):
                j=Jy(d,yy,x-50,x+50)
                if np.isfinite(j) and abs(j)>abs(best[0]): best=(j,yy)
            bg=localbg(D['OLD'],x,best[1] if best[1] else nom)
            if np.isfinite(bg) and abs(bg)>1e11: rows.append((bg,best[0],x,best[1]))
        rows=np.array(rows)
        if len(rows)<10: continue
        a,b=np.polyfit(rows[:,0],rows[:,1],1); r=np.corrcoef(rows[:,0],rows[:,1])[0,1]
        print('%s horiz y~%d: n=%d  slope %.5f intercept %+.3g corr %+.3f  step/bg med %.4f%%'%(lbl,nom,len(rows),a,b,r,100*np.median(rows[:,1]/rows[:,0])))
        res[lbl+'_h%d'%nom]=rows.tolist()
json.dump(res,open('run/RELEASE-02/c-delta/seam_vs_bg.json','w'),indent=0)
print('saved seam_vs_bg.json')
