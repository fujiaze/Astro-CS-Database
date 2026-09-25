"""CR-36-01 独立复算：把 HEALPix 像元表示成"以其 4 个角点为顶点、边为大圆弧的
球面四边形"，其面积相对该像元真实球面面积（等面积构造给 π/(3 n^2)，并用
"极冠内按环求差"的解析式独立复核）的相对误差，随 nside 与像元位置的分布。

只跑标准库 + 我自己写的 hpx_ring.py；不调用被审 C++ 的任何函数。
"""
import json
import sys
from math import pi, fabs
import hpx_ring as H

AT = 4.0 * pi
CLASSES = (('极点/极点邻域', 'pole'), ('冠边缘', 'capedge'), ('赤道带', 'eq'))


def analyse(n):
    At = H.a_true(n)
    res = {
        'n': n, 'npix': 12 * n * n, 'A_true': At,
        'sum_vos': 0.0, 'sum_lh': 0.0,
        'max_abs_rel_vos': 0.0, 'min_rel_vos': 0.0,
        'max_abs_rel_lh': 0.0, 'min_rel_lh': 0.0,
        'n_gt_1pct': 0, 'n_gt_0p1pct': 0, 'n_gt_1pct_lh': 0,
        'worst_loc': None, 'best_eq_loc': None, 'max_eq_abs': 0.0,
        'max_cross_dev': 0.0,
        'band_max_dev': 0.0,
        'cls': {k: [0.0, None] for _, k in CLASSES},
        'ring_tab': {},
    }
    for j in range(1, 4 * n):
        S = H.sweep(j, n)
        rmax = 0.0
        rmin = 0.0
        rm_loc = None
        for m in range(S):
            v = H.quad_vecs(j, m, n)
            av = H.area_vos(v)
            al = H.area_lhuilier(v)
            if fabs(al) > 0:
                res['max_cross_dev'] = max(res['max_cross_dev'], fabs(al - av) / al)
            rv = av / At - 1.0
            rl = al / At - 1.0
            res['sum_vos'] += av
            res['sum_lh'] += al
            if fabs(rv) > fabs(res['max_abs_rel_vos']):
                res['max_abs_rel_vos'] = fabs(rv)
            if rv < res['min_rel_vos']:
                res['min_rel_vos'] = rv
                res['worst_loc'] = (j, m, S)
            if fabs(rl) > fabs(res['max_abs_rel_lh']):
                res['max_abs_rel_lh'] = fabs(rl)
            if fabs(rv) > 0.01:
                res['n_gt_1pct'] += 1
            if fabs(rv) > 0.001:
                res['n_gt_0p1pct'] += 1
            if fabs(rl) > 0.01:
                res['n_gt_1pct_lh'] += 1
            rmax = max(rmax, fabs(rv))
            rmin = min(rmin, rv)
        # 环级汇总
        res['ring_tab'][j] = (S, rmax, rmin)
        # 位置分类
        kinds = []
        if j <= 2 or j >= 4 * n - 1:
            kinds.append('pole')
        if (n - 1) <= j <= (n + 1) or (3 * n - 1) <= j <= (3 * n + 1):
            kinds.append('capedge')
        if (2 * n - 1) <= j <= (2 * n + 1):
            kinds.append('eq')
        if n < j < 3 * n:
            res['max_eq_abs'] = max(res['max_eq_abs'], rmax)
        for k in kinds:
            cur = res['cls'][k][0]
            if rmax > cur:
                res['cls'][k] = [rmax, j]
        bd = H.band_check(n, j)
        res['band_max_dev'] = max(res['band_max_dev'], fabs(bd / At - 1.0))
    res['sum_vos_rel'] = res['sum_vos'] / AT - 1.0
    res['sum_lh_rel'] = res['sum_lh'] / AT - 1.0
    res['mean_rel'] = res['sum_vos'] / (res['npix'] * At) - 1.0
    return res


def main():
    ns = [int(x) for x in (sys.argv[1:] or [2, 4, 8, 16, 32, 64, 128, 256])]
    out = []
    print('nside   npix    max|rel|(VOS)  min rel(VOS)  max|rel|(lH)  '
          '#|rel|>1%  #>1%(lH)  ΣA/4π-1     mean/A-1   crossdev   worst ring')
    for n in ns:
        r = analyse(n)
        out.append(r)
        wl = r['worst_loc']
        print('%5d %8d  %+.6e  %+.6e  %+.6e  %7d %9d  %+.3e  %+.3e  %.2e  '
              'j=%d m=%d S=%d' %
              (n, r['npix'], r['max_abs_rel_vos'], r['min_rel_vos'],
               r['max_abs_rel_lh'], r['n_gt_1pct'], r['n_gt_1pct_lh'],
               r['sum_vos_rel'], r['mean_rel'], r['max_cross_dev'],
               wl[0], wl[1], wl[2]))
        print('        (|rel|>1%% 的像元数: VOS=%d, lHuilier=%d)' %
              (r['n_gt_1pct'], r['n_gt_1pct_lh']))
        print('        按环求差解析面积 vs π/(3n²) 最大偏差 = %.3e ; '
              '赤道带 max|rel| = %.3e' % (r['band_max_dev'], r['max_eq_abs']))
        for name, k in CLASSES:
            print('        %-12s max|rel| = %+.6e (ring %s)' %
                  (name, r['cls'][k][0], r['cls'][k][1]))
        # 极点位与前 3 大误差环
        tab = sorted(r['ring_tab'].items(), key=lambda kv: -kv[1][1])[:4]
        print('        误差最大的 4 个环: ' +
              ' ; '.join('j=%d S=%d max=%.4e' % (j, t[0], t[1]) for j, t in tab))
    with open('s1_result.json', 'w', encoding='utf-8') as f:
        json.dump([{k: (list(v) if isinstance(v, tuple) else v)
                    for k, v in r.items() if k != 'ring_tab'} for r in out],
                  f, ensure_ascii=False, indent=1, default=str)


if __name__ == '__main__':
    main()
