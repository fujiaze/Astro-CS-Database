
import json, numpy as np
BASE='run/RELEASE-02/L4-rebuild/upmfix_out'
M=json.load(open(BASE+'/p2_upm_model.bin'))
CJ=json.load(open(BASE+'/p2_corrected.json'))
ctrl=M['controls']; Marr=np.array([c[5] for c in ctrl])
frames=M['frames']; ref=M['component_ref_frame'][0]
refi=frames.index(ref)
C=np.zeros((len(frames),len(ctrl)))
for fi,fr in enumerate(M['C']):
    for cid,val in fr: C[fi,cid]=val
fid2name={fr['frame_id']:__import__('os').path.basename(fr['hips_path']) for fr in CJ['frames']}
# restrict to controls with substantial M and C
valid=Marr>1e11
print('controls with M>1e11:',valid.sum(),' M median %.4g'%np.median(Marr[valid]))
print()
print('frame  slope(C_k~M)  corr     intercept/Mmed   g_k=1+slope   name')
rows=[]
for fi in range(len(frames)):
    if fi==refi: continue
    c=C[fi]; m=Marr
    sel=valid & (c!=0)
    if sel.sum()<500: continue
    x=m[sel]; y=c[sel]
    a,b=np.polyfit(x,y,1)
    r=np.corrcoef(x,y)[0,1]
    rows.append((fi,a,r,b,1+a,fid2name.get(frames[fi],'?')))
rows.sort(key=lambda z:-abs(z[1]))
for fi,a,r,b,g,nm in rows[:15]:
    print('f%02d  %+.4f  %.3f  %+.4f  %.4f  %s'%(fi,a,r,b/np.median(Marr[valid]),g,nm[:46]))
print('...')
for fi,a,r,b,g,nm in rows[-5:]:
    print('f%02d  %+.4f  %.3f  %+.4f  %.4f  %s'%(fi,a,r,b/np.median(Marr[valid]),g,nm[:46]))
aa=np.array([z[1] for z in rows]); rr=np.array([z[2] for z in rows])
print()
print('slope distribution: min %+.4f med %+.4f max %+.4f'%(aa.min(),np.median(aa),aa.max()))
print('|slope|: med %.4f p90 %.4f max %.4f'%(np.median(np.abs(aa)),np.percentile(np.abs(aa),90),np.abs(aa).max()))
print('corr: min %.3f med %.3f max %.3f'%(rr.min(),np.median(rr),rr.max()))
print('frac frames |slope|>0.05: %.2f'%(np.mean(np.abs(aa)>0.05)))
np.save('run/RELEASE-02/c-delta/Ck_slope.npy', np.array([[z[0],z[1],z[2],z[3]] for z in rows]))
