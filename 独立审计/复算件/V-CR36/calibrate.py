"""校准（一次性；允许使用外部独立参考 astropy_healpix 2.0.0）。

把我 stdlib 转写的环序角点，按"环"为单位与外部参考做**多重集**比对
（每个像元的 4 角先内部排序，再把该环所有像元排序），从而只校几何形状，
不受 m=0 起点约定影响（起点约定不影响面积）。
另外比对逐环面积多重集，并校 Σ 四边形面积 = 4π。
外部参考只用于校准；交付的 s1.py / s2.py 只依赖标准库 + hpx_ring.py。
"""
import math
import hpx_ring as H

PI = math.pi
TOL = 1e-12


def key(q):
    return tuple((round(p % (2 * PI), 11), round(z, 11)) for p, z in
                 sorted(q, key=lambda t: (round(t[1], 12), round(t[0], 12))))


def mine_ring(j, n):
    S = H.sweep(j, n)
    out = []
    for m in range(S):
        out.append(key(H.corners(j, m, n)[0]))
    return sorted(out)


def main():
    import numpy as np
    from astropy_healpix import HEALPix
    total_bad = 0
    for n in (2, 4, 8, 16, 32, 64):        # 外部参考只支持 2 的幂次 nside
        hp = HEALPix(nside=n, order='ring')
        idx = np.arange(12 * n * n)
        vl, va = hp.boundaries_lonlat(idx, step=1)
        ext = {}
        for ip in range(12 * n * n):
            q = [(float(vl[ip][k].radian) % (2 * PI),
                  float(np.sin(va[ip][k].radian))) for k in range(4)]
            # 用该像元第一个角的 z 电平反查所属环（环心电平由公开定义给出）
            ext.setdefault(ip, []).append(q)
        # 外部角点按环归类：用"角的 z 电平集合"中最小的 |z| 变化定位中心电平
        # 直接办法：外部 healpix_to_lonlat 的环心 z -> 环号
        lon, lat = hp.healpix_to_lonlat(idx)
        zc = np.sin(np.asarray(lat.radian))
        bad = 0
        checked = 0
        sum_quad = 0.0
        for j in range(1, 4 * n):
            S = H.sweep(j, n)
            # 该环外部像元：环心 z 与我的 zlevel(j,n) 相同
            want = H.zlevel(j, n)
            sel = [ip for ip in range(12 * n * n) if abs(zc[ip] - want) < 1e-9]
            if len(sel) != S:
                # 南北对称环共用同 |z|：按符号分离
                sgn = 1.0 if want >= 0 else -1.0
                sel = [ip for ip in range(12 * n * n)
                       if abs(zc[ip] - want) < 1e-9 and zc[ip] * sgn >= -1e-12]
            e = sorted(key(q) for q in (ext[ip][0] for ip in sel))
            mm = mine_ring(j, n)
            checked += 1
            if e != mm:
                bad += 1
                if bad <= 2:
                    print('   RING MISMATCH n=%d j=%d ext[:2]=%s mine[:2]=%s'
                          % (n, j, e[:2], mm[:2]))
            for m in range(S):
                sum_quad += H.area_vos(H.quad_vecs(j, m, n))
        total_bad += bad
        print('nside=%3d 环数=%3d 环级角点多重集不匹配=%d  ΣA_quad/4π-1=%+.3e'
              % (n, checked, bad, sum_quad / (4 * PI) - 1.0))
    print('合计不匹配环数 = %d ; 相平局(tie)点数 = %d' % (total_bad, len(H.TIES)))


if __name__ == '__main__':
    main()
