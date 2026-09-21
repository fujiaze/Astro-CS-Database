
import json, numpy as np
BASE='run/RELEASE-02/L4-rebuild/upmfix_out'
M=json.load(open(BASE+'/p2_upm_model.bin'))
CJ=json.load(open(BASE+'/p2_corrected.json'))
frames=M['frames']; K=len(M['controls']); nF=len(frames)
Marr=np.array([c[5] for c in M['controls']],dtype=float)
C=np.zeros((nF,K))
for fi,fr in enumerate(M['C']):
    for cid,val in fr: C[fi,cid]=val
SP=json.load(open(BASE+'/p2_sky_plane.bin'))
deltas=np.array(SP['deltas'])
ra=np.array([c[3] for c in M['controls']]); dec=np.array([c[4] for c in M['controls']])
d2r=np.pi/180; r0=SP['ra0_deg']*d2r; d0=SP['dec0_deg']*d2r
rr=ra*d2r; dd=dec*d2r
cd=np.cos(dd); sd=np.sin(dd); cd0=np.cos(d0); sd0=np.sin(d0); dl=rr-r0
cosc=sd0*sd+cd0*cd*np.cos(dl)
u=cd*np.sin(dl)/cosc; v=(cd0*sd-sd0*cd*np.cos(dl))/cosc
xi=(u-SP['uc'])/SP['us']; eta=(v-SP['vc'])/SP['vs']
print('per-frame correction magnitudes at controls:')
print('frm  rms(C_k)      rms(delta_k)   mean(C_k)     delta0       mean(C)+delta0')
mc=[];md=[]
for f in range(nF):
    dk=deltas[f,0]+deltas[f,1]*xi+deltas[f,2]*eta
    c=C[f]; sel=(c!=0)
    if sel.sum()<100: 
        mc.append(0.0); md.append(0.0); continue
    mc.append(float(np.mean(c[sel]))); md.append(float(deltas[f,0]))
    if f<6 or abs(deltas[f,0])>1e11:
        print('f%02d  %.4g   %.4g   %+.4g   %+.4g   %+.4g'%(f,np.std(c[sel]),np.std(dk[sel]),np.mean(c[sel]),deltas[f,0],np.mean(c[sel])+deltas[f,0]))
mc=np.array(mc);md=np.array(md)
sel=np.arange(nF)!=0
print()
print('corr(mean C_k, delta0) = %.3f'%np.corrcoef(mc[sel],md[sel])[0,1])
print('mean|C| over frames: %.4g ; mean|delta0|: %.4g ; ratio %.3f'%(np.mean(np.abs(mc[sel])),np.mean(np.abs(md[sel])),np.mean(np.abs(mc[sel]))/max(np.mean(np.abs(md[sel])),1e-9)))
print('rms(C) all: %.4g ; rms(delta) all: %.4g'%(np.sqrt(np.mean(C**2)),np.sqrt(np.mean((deltas[:,0][:,None]+deltas[:,1][:,None]*xi+deltas[:,2][:,None]*eta)**2))))
# model/sky info
print()
print('UPM: converged=%s iterations=%d tolerance=%g objective=%.4f'%(M['converged'],M['iterations'],M['tolerance'],M['objective']))
print('sky: chi2_red=%.1f kappa=%.1f n_nodes=%s roughness=%s'%(SP['info']['chi2_red'],SP['info']['kappa'],SP['info'].get('n_nodes'),SP['cfg'].get('roughness_penalty')))
json.dump({'corr_meanC_delta0':float(np.corrcoef(mc[sel],md[sel])[0,1]),
           'rmsC':float(np.sqrt(np.mean(C**2))),
           'rmsdelta':float(np.sqrt(np.mean((deltas[:,0][:,None]+deltas[:,1][:,None]*xi+deltas[:,2][:,None]*eta)**2)))},
          open('run/RELEASE-02/c-delta/c_delta_comp.json','w'),indent=1)
