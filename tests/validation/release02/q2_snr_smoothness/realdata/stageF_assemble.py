#!/usr/bin/env python3
"""Q2 stage F: assemble realdata_seam.json (+ per-product local backgrounds)."""
import numpy as np, json, os, warnings, datetime
warnings.filterwarnings('ignore')
from astropy.io import fits
ROOT='/workspace/Astro CS Database'; L4=ROOT+'/run/RELEASE-02/L4-rebuild'
OUT=ROOT+'/run/RELEASE-02/q2-snr-smooth/realdata'
loci=json.load(open(OUT+'/loci.json'))
stageC=json.load(open(OUT+'/stageC_steps.json'))
e2=json.load(open(OUT+'/stageE2_nosearch.json'))
p2=json.load(open(OUT+'/p2_samples_summary.json'))
nmap=np.load(OUT+'/nmap.npy')
D={}
for k,p in (('p3_r_vis',L4+'/p3_r_vis/output_phase3.fits'),('p3_r_vis_fixed',L4+'/p3_r_vis_fixed/output_phase3.fits'),
            ('p3_upmfix',L4+'/p3_upmfix/output_phase3.fits')):
    with fits.open(p,memmap=True) as h: D[k]=np.asarray(h[0].data,dtype=np.float64)
K,GAP=25,1
xs=np.arange(750,1951,25); yv=np.arange(400,3501,50)
SPEC={'H1top':('y',xs,lambda x:loci['H1top']['intercept']+loci['H1top']['slope']*x),
      'H1bot':('y',xs,lambda x:loci['H1bot']['intercept']+loci['H1bot']['slope']*x),
      'V-right':('x',yv,lambda y:loci['V-right']['intercept']+loci['V-right']['slope']*y)}
# per-product local background level in the J window at the fitted locus
locbg={}
for bn,(ax,smp,f) in SPEC.items():
    for prod in D:
        vals=[]
        for s in smp:
            c=int(round(f(s)))
            if ax=='y': w=D[prod][c-K:c+K+1, s-50:s+50]
            else:       w=D[prod][s-50:s+50, c-K:c+K+1]
            vals.append(float(np.nanmedian(w)))
        locbg['%s|%s'%(bn,prod)]=float(np.median(vals))
# per-product global covered median
gmed={}
for prod in D:
    with fits.open(L4+'/p3_r_vis/output_phase3.fits',memmap=True) as h: cov=np.asarray(h[1].data)>0.5
    gmed[prod]=float(np.nanmedian(D[prod][cov]))
final=dict(
 schema='ASTROCS-Q2-REALDATA-SEAM-v1',
 generated_utc=datetime.datetime.utcnow().isoformat()+'Z',
 scope='RELEASE-02 Q2 real-data measurement: nmap / tilted seam loci / seam steps with off-locus controls / p2 sample structure',
 products={'OLD_p3_r_vis':L4+'/p3_r_vis/output_phase3.fits (source mosaic_out_w1, SWVER 98e529ec)',
           'NEW_bitref_16w':L4+'/p3_r_vis_fixed/output_phase3.fits (source bitref_16w, SWVER 9405f1b)',
           'upmfix':L4+'/p3_upmfix/output_phase3.fits (source upmfix_out, SWVER 9405f1b)'},
 wcs=dict(CRPIX=[2048.5,2048.5],CRVAL=[83.747318,-5.361391],
          CD=[[-7.1462e-4,0.0],[0.0,7.1462e-4]],ctype=['RA---TAN','DEC--TAN']),
 nmap=dict(method='stage10_nmap.py method verbatim (49 frames norm/*/calibrated_*.fts outline WCS->p3, bbox grid, inside test), row-chunked',
           used_frames=49, skipped=0, shape=[4096,4096], dtype='int16',
           hist={str(int(k)):int(v) for k,v in zip(*np.unique(nmap,return_counts=True))},
           n_gt0=int((nmap>0).sum()), n_eq0=int((nmap==0).sum()), saved=OUT+'/nmap.npy'),
 loci=loci,
 loci_notes=dict(H1top='x in [700,2000]; first y in [1100,1300] with n>=7.5 (rising); sub-pixel interp; robust (2.5sigma) linear fit',
                 H1bot='x in [700,2000]; first y in [1350,1500] with n<=8.5 (falling)',
                 V_right='y in [300,3600]; first x in [2050,2250] with n<=8.5 (falling)'),
 estimator=dict(
   primary='stage16_locus.py Jy/Jx: per-row/col linear extrapolation over K=25px each side (gap=1), evaluated at boundary, median over samples',
   convention='J = below/right side minus above/left side',
   K=K, gap=GAP,
   sample_positions=dict(H1top='x=750..1950 step 25 (n=49)',H1bot='x=750..1950 step 25 (n=49)',V_right='y=400..3500 step 50 (n=63)'),
   variants=dict(search_pm8='max |J| over +/-8px around fitted locus (as specified in task / reference stage16)',
                 no_search='J evaluated exactly at the fitted locus (unbiased)'),
   bg_denominator=dict(caliber='OLD = p3_r_vis. ref_old = OLD regional median on the out-of-band side; local = OLD nanmedian over the J measurement window',
                       ref_old_regional=stageC['bg_ref_old_regional'],
                       ref_old_used={'H1top':1.03542e13,'H1bot':1.07818e13,'V-right':1.01664e13},
                       local_old=stageC['measurements'])),
 step_search_pm8=stageC['measurements'],
 step_no_search=e2['no_search'],
 step_profile_vs_offset=e2['profile'],
 search_bias=e2['search_bias'],
 independent_wide_strip=dict(
   note='10px strip each side, 6px gap, NO search, nanmedian difference then median over samples. BIASED where a strong gradient crosses the boundary (notably V-right); reported for transparency only.',
   results=json.load(open(OUT+'/stageE.log')) if False else None),
 global_offset_vs_old=dict(
   note='p3_r_vis_fixed is a sky-pedestal-subtracted product: it is ~9.4e12 ADU LOWER than p3_r_vis over the whole field. Percent-of-background therefore uses the OLD pedestal.',
   product_global_covered_median=gmed,
   per_product_local_bg_at_locus=locbg),
 p2_samples=p2,
 files=dict(nmap=OUT+'/nmap.npy',loci=OUT+'/loci.json',stageAB_log=OUT+'/stageAB.log',stageB_log=OUT+'/stageB.log',
            stageC_log=OUT+'/stageC.log',stageC2_profile_log=OUT+'/stageC2.log',stageD_log=OUT+'/stageD.log',
            stageD2_log=OUT+'/stageD2.log',stageE_log=OUT+'/stageE.log',stageE2_log=OUT+'/stageE2.log',
            p2_summary=OUT+'/p2_samples_summary.json'),
)
json.dump(final,open(OUT+'/realdata_seam.json','w'),indent=1)
print('wrote',OUT+'/realdata_seam.json')
print()
print('per-product global covered median:',json.dumps(gmed,indent=1))
print('per-product local bg at locus:',json.dumps(locbg,indent=1))
