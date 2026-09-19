#!/usr/bin/env python3
"""Q2 stage H: build realdata_seam.log (header + summary tables + full stage logs)."""
import json, numpy as np, datetime, os
OUT='/workspace/Astro CS Database/run/RELEASE-02/q2-snr-smooth/realdata'
J=json.load(open(OUT+'/realdata_seam.json'))
L=[]
def w(s=''): L.append(s)
w('='*118)
w('AstroCS RELEASE-02  Q2 real-data measurement log')
w('generated (UTC): '+J['generated_utc'])
w('products: OLD=p3_r_vis (mosaic_out_w1, SWVER 98e529ec) | NEW=bitref_16w -> p3_r_vis_fixed | upmfix -> p3_upmfix (both SWVER 9405f1b)')
w('='*118)
w()
w('--- 1. nmap rebuilt from 49 frames (stage10 method) ---')
w('  used_frames=%d skipped=%d  shape=%s dtype=%s  n>0=%d  n==0=%d'%(J['nmap']['used_frames'],J['nmap']['skipped'],
   J['nmap']['shape'],J['nmap']['dtype'],J['nmap']['n_gt0'],J['nmap']['n_eq0']))
w('  hist (n:count): '+json.dumps(J['nmap']['hist']))
w()
w('--- 2. tilted boundary loci (robust linear fit of the n-map transition) ---')
for k,v in J['loci'].items():
    w('  %-8s slope=%+.6f intercept=%.3f rms=%.3fpx maxres=%.2fpx n=%d  range x/y=[%.0f,%.0f]'%(
      k,v['slope'],v['intercept'],v['rms'],v['resid_max'],v['n_used'],v['x_min'],v['x_max']))
    if v['axis']=='y': w('           y_edge(x)=%.3f %+.6f x  -> y(700)=%.2f  y(2000)=%.2f'%(v['intercept'],v['slope'],v['intercept']+v['slope']*700,v['intercept']+v['slope']*2000))
    else:              w('           x_edge(y)=%.3f %+.6f y  -> x(300)=%.2f  x(3600)=%.2f'%(v['intercept'],v['slope'],v['intercept']+v['slope']*300,v['intercept']+v['slope']*3600))
w()
w('--- 3. GLOBAL SCALE CHECK (important caveat) ---')
w('  product global covered median (ADU): '+json.dumps(J['global_offset_vs_old']['product_global_covered_median']))
w('  local background in the measurement window at each locus (ADU):')
for k,v in J['global_offset_vs_old']['per_product_local_bg_at_locus'].items(): w('    %-24s %.6g'%(k,v))
w('  -> p3_r_vis_fixed (bitref_16w) is ~9.4e12 ADU BELOW p3_r_vis over the WHOLE field (block-median offset spread')
w('     [-1.384e13,-7.017e12], i.e. a smooth sky-pedestal subtraction, not a seam. p3_upmfix is within ~6e11 of p3_r_vis.')
w('     J (the seam step) is offset-invariant; all percentages below use the OLD (p3_r_vis) background as denominator.')
w()
w('--- 4. SEAM STEPS ---')
w('  estimator: stage16 Jy/Jx, per-row/col linear extrapolation K=25px each side (gap=1), J = below/right - above/left, median over samples')
w('  ref OLD background (out-of-band regional median): H1top=1.03542e13  H1bot=1.07818e13  V-right=1.01664e13')
w()
w('  (a) PRIMARY as specified: max |J| searched over +/-8px around the fitted locus')
w('  %-9s %-15s %14s %14s %10s %9s'%('boundary','product','J_median','J_p16','J_p84','%refOLD'))
for bn in ('H1top','H1bot','V-right'):
    for prod in ('p3_r_vis_fixed','p3_upmfix','p3_r_vis'):
        r=J['step_search_pm8']['%s|%s|shift+0'%(bn,prod)]
        w('  %-9s %-15s %+14.5g %+14.4g %+14.4g %+9.3f'%(bn,prod,r['J']['median'],r['J']['p16'],r['J']['p84'],r['pct_of_ref_old_bg']))
    w()
w('  (b) UNBIASED: J exactly at the fitted locus (no search)')
w('  %-9s %-15s %14s %14s %10s %9s'%('boundary','product','J_median','J_p16','J_p84','%refOLD'))
for bn in ('H1top','H1bot','V-right'):
    for prod in ('p3_r_vis_fixed','p3_upmfix','p3_r_vis'):
        r=J['step_no_search']['%s|%s|shift+0'%(bn,prod)]
        w('  %-9s %-15s %+14.5g %+14.4g %+14.4g %+9.3f'%(bn,prod,r['median'],r['p16'],r['p84'],r['pct_of_ref_old_bg']))
    w()
w('  (c) OFF-LOCUS CONTROL: locus shifted by +/-150px (max|J| +/-8 search)')
w('  %-9s %-15s %14s %14s %9s | %14s %14s %9s'%('boundary','product','J@-150','p16','%ref','J@+150','p84','%ref'))
for bn in ('H1top','H1bot','V-right'):
    for prod in ('p3_r_vis_fixed','p3_upmfix','p3_r_vis'):
        a=J['step_search_pm8']['%s|%s|shift-150'%(bn,prod)]; b=J['step_search_pm8']['%s|%s|shift+150'%(bn,prod)]
        w('  %-9s %-15s %+14.5g %+14.4g %+9.3f | %+14.5g %+14.4g %+9.3f'%(bn,prod,
          a['J']['median'],a['J']['p16'],a['pct_of_ref_old_bg'],b['J']['median'],b['J']['p84'],b['pct_of_ref_old_bg']))
    w()
w('  (d) search-bias diagnostic: J(off=0) vs max|J| over off in [-8,8] (per-sample median)')
for k,v in J['search_bias'].items():
    w('    %-24s J(off=0)=%+.5g  max|J|=%+.5g at off=%+d'%(k,v['J_off0'],v['J_at_maxabs'],v['best_off']))
w()
w('--- 5. p2_samples.json STRUCTURE ---')
for tag,f in J['p2_samples']['files'].items():
    w('  == %s (%d bytes)'%(tag,f['size_bytes']))
    w('     controls[] entries=%d (unique %d)  observations[]=%d  distinct frame_id=%d'%(f['n_controls_entries'],f['n_unique_controls'],f['n_observations'],f['n_frames']))
    w('     coverage count per control: min=%d max=%d mean=%.3f median=%.1f ; controls with 0 obs=%d, with 1 obs=%d, >=2=%d'%(
      f['coverage_count_stats']['min'],f['coverage_count_stats']['max'],f['coverage_count_stats']['mean'],
      f['coverage_count_stats']['median'],f['controls_with_zero_obs'],f['controls_with_one_obs'],f['controls_with_ge2_obs']))
    w('     coverage histogram: '+json.dumps(f['coverage_count_stats']['histogram']))
    for k in ('ivar','snr','snr_available','uncertainty','value','support','control_ivar','control_variance','quality_flags'):
        s=f['field_stats'][k]
        w('     %-17s n=%d zero=%d nonzero=%d  min=%s median=%s max=%s'%(k,s['n'],s['n_zero'],s['n_nonzero'],
          ('%.5g'%s['min']) if s['min'] is not None else 'None',('%.5g'%s['median']) if s['median'] is not None else 'None',
          ('%.5g'%s['max']) if s['max'] is not None else 'None'))
    w('     -> ALL observations have ivar=0, snr=0, snr_available=0 (stubs). uncertainty IS populated (median %.5g = 1/sqrt(control_ivar)).'%f['field_stats']['uncertainty']['median'])
    w('     stats: '+json.dumps(f['stats'],sort_keys=True))
    w('     frame_id -> n_samples: '+json.dumps({k2:v2['n_samples'] for k2,v2 in sorted(f['frames'].items(),key=lambda kv:int(kv[0]))}))
w('  NOTE: bitref_16w and upmfix_out p2_samples.json have IDENTICAL controls[] and observations[] (order-sensitive compare);')
w('        only stats.rejected_insufficient_support differs (5792 vs 4379). Accepted sample set is unchanged by the upmfix change.')
w()
w('='*118)
w('FULL PER-STAGE LOGS')
w('='*118)
for name in ('stageAB.log','stageB.log','stageC.log','stageC2.log','stageD.log','stageD2.log','stageE.log','stageE2.log'):
    p=os.path.join(OUT,name)
    if os.path.exists(p):
        w(); w('#'*118); w('# '+name); w('#'*118); w()
        L.append(open(p,errors='replace').read())
open(OUT+'/realdata_seam.log','w').write('\n'.join(L))
print('wrote',OUT+'/realdata_seam.log',os.path.getsize(OUT+'/realdata_seam.log'),'bytes')
