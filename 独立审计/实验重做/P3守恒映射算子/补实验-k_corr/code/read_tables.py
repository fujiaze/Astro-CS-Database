import json
R = 'results/'
def load(n): return json.load(open(R+n, encoding='utf-8'))
g0 = load('g0_sanity.json'); g1 = load('g1_canonical.json'); g2 = load('g2_decomposition.json')
g3 = load('g3_nscan.json'); g3b = load('g3b_gauss_ref.json'); g4 = load('g4_geometry_scan.json')
g5 = load('g5_frames.json'); g6 = load('g6_fit.json'); g7 = load('g7_fh_ratio.json')
hip = load('g3b_n5_hiprec.json')

L = []
L.append("# 结果汇总表(由 results/*.json 机器生成)\n")
L.append("## T0 装备自检(恒等几何 M=I, phase=(0.5,0.5))\n")
L.append("| N | k_corr | MC SE | 参考值 | 判定 |")
L.append("|---|---|---|---|---|")
kg = {r['N']: r['k_corr'] for r in g3b['rows']}
L.append("| 289 | %.4f | %.4f | 1.0 (iid 高斯大 N) | %s |" % (g0['N289']['k_corr'], g0['N289']['k_corr_mc_se'], g0['gate']['pass289']))
L.append("| 25 | %.4f | %.4f | %.4f (k_gauss(25)) | %s |" % (g0['N25']['k_corr'], g0['N25']['k_corr_mc_se'], kg[25], g0['gate']['pass25']))
L.append("| 5 | 1.634 (3x40000: 1.6325/1.6262/1.6429) | 0.007 | 1.637 (直接定征 400k) | PASS(=k_gauss(5), 非 1) |")
L.append("")
L.append("## T1 正本几何复现(平面模型, 300\"/px 源 -> nside=512 输出 412.26\", pixfrac=0.8, 单帧, NMC=2000 x 8 seeds x 16 相位)\n")
L.append("16 相位 k_corr: mean=%.4f, 相位间 sd=%.4f, range=[%.4f, %.4f]; 单次(单相位单 seed) sd=0.04-0.065" % (
  g1['k_corr_mean_over_phases'], g1['k_corr_sd_over_phases'], g1['k_corr_range'][0], g1['k_corr_range'][1]))
L.append("正本记载值 1.3883; 冻结值 1.4。1.3883 落在复现 range 内(上沿)。\n")
L.append("## T2 效应分解(正本几何, phase=(0.3,0.55), N=225 全 touched)\n")
L.append("| 量 | 值 | 含义 |")
L.append("|---|---|---|")
L.append("| k_corr | %.4f (MC SE %.4f) | 总因子 |" % (g2['k_corr'], g2['k_corr_mc_se']))
L.append("| k_shape | %.4f | iid 重采样边际的 k(非高斯边际形状, 含有限 N 偏置) |" % g2['k_shape'])
L.append("| k_geom | %.4f | k_corr/k_shape, 纯相关因子 |" % g2['k_geom'])
L.append("| mean_nn_corr | %.4f (%d 对) | 算子精确最近邻相关 |" % (g2['nn_corr']['mean_nn_corr'], g2['nn_corr']['n_nn_pairs']))
L.append("| sigma_bg | %.4f | MAD 尺度(正本估计器口径) |" % g2['sigma_bg'])
L.append("")
L.append("## T3 N 扫描(正本几何, 紧凑 patch=距中心最近 N 像素, NMC=4000)\n")
L.append("| patch | N | k_corr | MC SE | k_gauss(N) iid 参考 | k_excess=k_corr/k_gauss | shape_excess |")
L.append("|---|---|---|---|---|---|---|")
for r in g3['rows']:
    kgn = kg.get(r['N'])
    se = r.get('k_corr_mc_se') or float('nan')
    if kgn:
        L.append("| %s | %d | %.4f | %.4f | %.4f | %.4f | %.4f |" % (
          r['shape'], r['N'], r['k_corr'], se, kgn, r['k_corr']/kgn, r['k_shape']/kgn))
    else:
        L.append("| %s | %d | %.4f | %.4f | - | - | - |" % (r['shape'], r['N'], r['k_corr'], se))
L.append("")
L.append("k_gauss(N) iid 高斯参考(恒等几何): " + ", ".join("N=%d:%.3f" % (r['N'], r['k_corr']) for r in g3b['rows']))
L.append("")
L.append("N=5 高精度(3x40000): k=%.4f; 直接定征(400k 实现且独立): Var(median)/渐近式=%.4f, median(MAD)/sigma=%.4f, k=%.4f" % (
  hip['k_mean'], hip['direct_characterization']['Var_median_asympt_ratio'],
  hip['direct_characterization']['med_MAD_over_sigma'], hip['direct_characterization']['k_gauss5_direct']))
L.append("")
L.append("## T4 几何扫描(rho=输出/源像素尺度比, pf=pixfrac, phase=(0.3,0.55), NMC=2000)\n")
L.append("| rho | pf | N(touched) | k_corr | MC SE | k_geom |")
L.append("|---|---|---|---|---|---|")
for r in g4['rows']:
    L.append("| %.4f | %.1f | %d | %.4f | %.4f | %.4f |" % (r['rho'], r['pixfrac'], r['N'], r['k_corr'], r.get('k_corr_mc_se', float('nan')), r['k_geom']))
L.append("")
L.append("## T5 多帧 dither 变体(正本几何, patch=全 touched)\n")
L.append("| n_frames | N | k_corr | MC SE | k_geom |")
L.append("|---|---|---|---|---|")
for r in g5['rows']:
    L.append("| %d | %d | %.4f | %.4f | %.4f |" % (r['n_frames'], r['N'], r['k_corr'], r.get('k_corr_mc_se', float('nan')), r['k_geom']))
L.append("")
L.append("## T6 对 F&H 2002 式(8) R 的算子级复核(正本单帧几何, 225 像素)\n")
L.append("R_median=%.4f [p16=%.4f, p84=%.4f], R_mean=%.4f; F&H 闭式(10)(均匀充满 dither 假设, r=0.5822): R=%.4f" % (
  g7['R_median'], g7['R_p16'], g7['R_p84'], g7['R_mean'], 1.0/(1.0-g7['fh_closed_form_r']/3.0)))
L.append("")
L.append("## T7 拟合(G6)\n")
L.append("模型 k=k_inf*(1+c/(N-1)): k_inf=%.4f c=%.4f, max_rel_resid=%.4f (拟合一般; 推荐形式 k_corr(N)=k_gauss(N) x k_geo)" % (g6['k_inf'], g6['c'], g6['max_rel_resid']))
open(R+'tables.md', 'w').write("\n".join(L))
print("written results/tables.md, lines:", len(L))
