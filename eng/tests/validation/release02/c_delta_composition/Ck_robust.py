
import json, numpy as np
BASE='run/RELEASE-02/L4-rebuild/upmfix_out'
M=json.load(open(BASE+'/p2_upm_model.bin'))
S=json.load(open(BASE+'/p2_samples.json'))
CJ=json.load(open(BASE+'/p2_corrected.json'))
frames=M['frames']; FI={f:i for i,f in enumerate(frames)}
K=len(M['controls']); nF=len(frames)
Marr=np.array([c[5] for c in M['controls']],dtype=float)
C=np.zeros((nF,K))
for fi,fr in enumerate(M['C']):
    for cid,val in fr: C[fi,cid]=val
fid2name={fr['frame_id']:__import__('os').path.basename(fr['hips_path']) for fr in CJ['frames']}
obs=S['observations']
ref=frames[0]; refi=0
ref_ctrl=set(o['control_id'] for o in obs if o['frame_id']==ref)
print('ref frame f00 =',fid2name.get(ref,'?'))
print('ref controls',len(ref_ctrl))
# frame coverage overlap with ref
rows=[]
for f in range(nF):
    if f==refi: continue
    c=C[f]; sel0=(c!=0)
    if sel0.sum()<500: continue
    sel=sel0 & np.isin(np.arange(K),list(ref_ctrl))
    if sel.sum()<500: continue
    x=Marr[sel]; y=c[sel]
    a,b=np.polyfit(x,y,1); r=np.corrcoef(x,y)[0,1]
    rows.append((f,a,r,int(sel.sum()),np.log10(x.max()/max(x.min(),1e-9)),fid2name.get(frames[f],'?')))
rows.sort(key=lambda z:-abs(z[2]))
print()
print('REGRESS C_k ~ M_c restricted to controls where REFERENCE frame also observed:')
print('frame slope   corr   n     log10(Mrange)  name')
for f,a,r,n,lg,nm in rows[:10]:
    print('f%02d  %+.4f  %+.3f  %5d  %.2f  %s'%(f,a,r,n,lg,nm[:44]))
print('...')
for f,a,r,n,lg,nm in rows[-4:]:
    print('f%02d  %+.4f  %+.3f  %5d  %.2f  %s'%(f,a,r,n,lg,nm[:44]))
rr=np.array([z[2] for z in rows]); aa=np.array([z[1] for z in rows])
print('corr med %.3f  frac|corr|>0.5 %.2f  |slope| med %.4f  frac|slope|>0.05 %.2f'%(np.median(rr),np.mean(np.abs(rr)>0.5),np.median(np.abs(aa)),np.mean(np.abs(aa)>0.05)))
# within-tile check for one high-corr frame: pick tile with most controls
tc={}
for k,c in enumerate(M['controls']): tc.setdefault(c[0],[]).append(k)
tip=max(tc,key=lambda t:len(tc[t])); kids=np.array(tc[tip])
f=rows[0][0]; c=C[f]
sel=kids[(c[kids]!=0)]
if sel.size>20:
    x=Marr[sel]; y=c[sel]; a,b=np.polyfit(x,y,1); r=np.corrcoef(x,y)[0,1]
    print()
    print('within single tile %d (frame f%02d): n=%d slope %+.4f corr %+.3f  M range %.3g..%.3g'%(tip,f,sel.size,a,r,x.min(),x.max()))
