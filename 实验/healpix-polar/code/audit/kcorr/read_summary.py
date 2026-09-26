import json, sys
OUT = '/workspace/Astro CS Database/独立审计/实验重做/P3守恒映射算子/补实验-k_corr/results'
def load(n): return json.load(open(OUT + '/' + n, encoding='utf-8'))
g0 = load('g0_sanity.json')
print("G0 sanity: N289 k=%.4f k_shape=%.4f pass=%s | N25 k=%.4f pass=%s" % (
  g0['N289']['k_corr'], g0['N289']['k_shape'], g0['gate']['pass289'],
  g0['N25']['k_corr'], g0['gate']['pass25']))
g1 = load('g1_canonical.json')
print("")
print("G1 canonical repro: target=1.3883; over 16 phases x 8 seeds: mean=%.4f sd=%.4f range=[%.4f, %.4f]" % (
  g1['k_corr_mean_over_phases'], g1['k_corr_sd_over_phases'], g1['k_corr_range'][0], g1['k_corr_range'][1]))
for r in g1['phases']:
    print("  phase=%s N=%d k=%.4f +/- %.4f [%.4f,%.4f]" % (r['phase'], r['n_touched'], r['k_corr_mean'], r['k_corr_sd'], r['k_corr_min'], r['k_corr_max']))
g2 = load('g2_decomposition.json')
print("")
print("G2 decomposition: k_corr=%.4f k_shape=%.4f k_geom=%.4f" % (g2['k_corr'], g2['k_shape'], g2['k_geom']))
print("  sigma_bg=%.4f sigma_bg_iid=%.4f" % (g2['sigma_bg'], g2['sigma_bg_iid']))
print("  exact cov: n_eff_mean=%.2f (N=%d) var_marg_mean=%.5f var_mean=%.5f" % (
  g2['exact']['n_eff_mean_cov'], g2['exact']['N'], g2['exact']['var_marg_mean_exact'], g2['exact']['var_mean_exact']))
print("  nn_corr mean=%.4f pairs=%d" % (g2['nn_corr']['mean_nn_corr'], g2['nn_corr']['n_nn_pairs']))
g3 = load('g3_nscan.json')
print("")
print("G3 N-scan (canonical geometry):")
for r in g3['rows']:
    nnc = r['nn_corr']['mean_nn_corr'] if r.get('nn_corr') else None
    print("  %-16s N=%4d k_corr=%.4f k_shape=%.4f k_geom=%.4f nn=%s n_eff=%.1f" % (
      r['shape'], r['N'], r['k_corr'], r['k_shape'], r['k_geom'],
      ('%.4f' % nnc) if nnc is not None else '-', r['exact']['n_eff_mean_cov'] if r.get('exact') else -1))
g5 = load('g5_frames.json')
print("")
print("G5 frames:")
for r in g5['rows']:
    print("  frames=%d N=%d k_corr=%.4f k_shape=%.4f k_geom=%.4f" % (r['n_frames'], r['N'], r['k_corr'], r['k_shape'], r['k_geom']))
g6 = load('g6_fit.json')
print("")
print("G6 fit model k=k_inf*(1+c/(N-1)): k_inf=%.4f c=%.4f max_rel_resid=%.4f" % (g6['k_inf'], g6['c'], g6['max_rel_resid']))
print("  N:", g6['N'])
print("  k:", [round(v,4) for v in g6['k']])
print("  pred:", [round(v,4) for v in g6['pred']])
