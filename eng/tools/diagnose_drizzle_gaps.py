#!/usr/bin/env python3
"""
drizzle 缝隙诊断脚本
功能: 读取 .hiss 文件, 生成 gnomonic 投影诊断图, 检测缝隙
用途: 验证 drizzle 缝隙修复效果
"""
import sys
import os
import io
import numpy as np

if sys.platform == 'win32':
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')

sys.path.insert(0, r'f:\Astro dev\Astro CS Normalization Database\lib\healpix_db\healpix_io')

from healpix_io import HissReader
from astropy_healpix import HEALPix
from astropy.coordinates import spherical_to_cartesian
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

def main():
    hiss_path = r'f:\Astro dev\Astro CS Normalization Database\output\pipeline_debug\Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@061703-180S-Red\drizzle\T4_2x_nside65536.hiss'

    print(f"读取: {hiss_path}")
    with HissReader(hiss_path) as reader:
        nside = reader.nside
        nested = reader.nested
        n_pix = reader.n_pix
        ipix = reader.ipix.copy()
        pixel = reader.pixel.copy()
        meta = reader.meta

    print(f"nside={nside} nested={nested} n_pix={n_pix}")
    print(f"meta filter={meta.get('filter','?')} exptime={meta.get('exposure_s','?')}")

    # 像素值统计
    print(f"\n=== 像素值统计 ===")
    print(f"min={pixel.min():.6f} max={pixel.max():.6f} mean={pixel.mean():.6f}")
    print(f"零值像素: {(pixel == 0).sum()} / {n_pix} ({100*(pixel==0).sum()/n_pix:.3f}%)")
    print(f"负值像素: {(pixel < 0).sum()} / {n_pix}")
    print(f"<1e-10像素: {(pixel < 1e-10).sum()} / {n_pix}")

    # 用 astropy_healpix 转换 ipix → ra/dec
    hp = HEALPix(nside=nside, order='nested' if nested else 'ring')
    # astropy_healpix 需要 int64 数组
    ipix_i64 = ipix.astype(np.int64)
    ra_arr, dec_arr = hp.healpix_to_lonlat(ipix_i64)
    ra_arr = np.array(ra_arr.deg)
    dec_arr = np.array(dec_arr.deg)

    # 邻居完整性检查 (采样前 5000 像素)
    print(f"\n=== 邻居完整性检查 (采样前 5000 像素) ===")
    ipix_set = set(int(x) for x in ipix)
    missing_count = 0
    total_nbrs = 0
    sample_size = min(5000, n_pix)
    for i in range(sample_size):
        nbrs = hp.neighbours(int(ipix[i]))
        for nb in nbrs:
            if nb >= 0:
                total_nbrs += 1
                if int(nb) not in ipix_set:
                    missing_count += 1
    print(f"采样 {sample_size} 像素, 总邻居数: {total_nbrs}, 缺失邻居: {missing_count}")
    if total_nbrs > 0:
        print(f"缺失率: {100*missing_count/total_nbrs:.2f}% (0%=无缝隙, 高%=有缝隙)")

    # 分析缺失邻居的位置分布 (判断边界效应 vs 内部缝隙)
    print(f"\n=== 缺失邻居位置分析 (采样前 5000 像素) ===")
    missing_ra = []
    missing_dec = []
    missing_per_pixel = []
    for i in range(sample_size):
        nbrs = hp.neighbours(int(ipix[i]))
        miss_count = 0
        for nb in nbrs:
            if nb >= 0 and int(nb) not in ipix_set:
                miss_count += 1
                # 获取缺失邻居的 ra/dec
                try:
                    nb_ra, nb_dec = hp.healpix_to_lonlat(np.array([int(nb)], dtype=np.int64))
                    missing_ra.append(float(nb_ra[0].deg))
                    missing_dec.append(float(nb_dec[0].deg))
                except Exception:
                    pass
        missing_per_pixel.append(miss_count)

    missing_per_pixel = np.array(missing_per_pixel)
    print(f"每像素缺失邻居数分布:")
    for k in range(9):
        cnt = (missing_per_pixel == k).sum()
        if cnt > 0:
            print(f"  缺失 {k} 个邻居: {cnt} 像素 ({100*cnt/sample_size:.1f}%)")

    # 缺失邻居的 ra/dec 范围
    if missing_ra:
        missing_ra = np.array(missing_ra)
        missing_dec = np.array(missing_dec)
        print(f"\n缺失邻居 ra 范围: {missing_ra.min():.2f} ~ {missing_ra.max():.2f}")
        print(f"缺失邻居 dec 范围: {missing_dec.min():.2f} ~ {missing_dec.max():.2f}")
        # 源图像 FOV 范围 (从 meta 推断或用所有像素范围)
        all_ra_min, all_ra_max = ra_arr.min(), ra_arr.max()
        all_dec_min, all_dec_max = dec_arr.min(), dec_arr.max()
        print(f"全图 ra 范围: {all_ra_min:.2f} ~ {all_ra_max:.2f}")
        print(f"全图 dec 范围: {all_dec_min:.2f} ~ {all_dec_max:.2f}")
        # 判断缺失邻居是否集中在边缘
        edge_ra = ((missing_ra < all_ra_min + 0.01) | (missing_ra > all_ra_max - 0.01)).sum()
        edge_dec = ((missing_dec < all_dec_min + 0.01) | (missing_dec > all_dec_max - 0.01)).sum()
        print(f"缺失邻居在 ra 边缘: {edge_ra} / {len(missing_ra)} ({100*edge_ra/len(missing_ra):.1f}%)")
        print(f"缺失邻居在 dec 边缘: {edge_dec} / {len(missing_ra)} ({100*edge_dec/len(missing_ra):.1f}%)")
        print(f"缺失邻居在边缘(ra或dec): {((edge_ra>0)|(edge_dec>0))} / {len(missing_ra)}")

    # 生成 gnomonic 投影诊断图
    print(f"\n=== 生成诊断图 ===")
    # 取中心像素
    center_idx = n_pix // 2
    ra0 = ra_arr[center_idx]
    dec0 = dec_arr[center_idx]
    print(f"中心: ra={ra0:.4f} dec={dec0:.4f}")

    # gnomonic 投影
    ra_rad = np.radians(ra_arr)
    dec_rad = np.radians(dec_arr)
    ra0_rad = np.radians(ra0)
    dec0_rad = np.radians(dec0)
    cosc = np.sin(dec0_rad)*np.sin(dec_rad) + np.cos(dec0_rad)*np.cos(dec_rad)*np.cos(ra_rad - ra0_rad)
    # 避免除零
    cosc = np.clip(cosc, 1e-12, None)
    x = np.cos(dec_rad)*np.sin(ra_rad - ra0_rad) / cosc
    y = (np.cos(dec0_rad)*np.sin(dec_rad) - np.sin(dec0_rad)*np.cos(dec_rad)*np.cos(ra_rad - ra0_rad)) / cosc
    x_arcsec = x * 180/np.pi * 3600
    y_arcsec = y * 180/np.pi * 3600

    # 中心 400"x400" 区域
    mask = (np.abs(x_arcsec) < 200) & (np.abs(y_arcsec) < 200)
    print(f"中心 400x400 区域像素数: {mask.sum()}")

    if mask.sum() > 0:
        fig, axes = plt.subplots(1, 2, figsize=(20, 9))

        # 图1: 像素值分布
        ax = axes[0]
        sc = ax.scatter(x_arcsec[mask], y_arcsec[mask], c=pixel[mask], s=0.3,
                        cmap='gray', vmin=np.percentile(pixel[mask], 1), vmax=np.percentile(pixel[mask], 99))
        ax.set_xlabel("x (arcsec)")
        ax.set_ylabel("y (arcsec)")
        ax.set_title(f"Drizzle 通量图 ({mask.sum()} px, nside={nside}, 400x400 arcsec)")
        ax.set_aspect('equal')
        plt.colorbar(sc, ax=ax, label='flux')

        # 图2: 像素密度图 (检测缝隙 - 用 2D 直方图)
        ax = axes[1]
        # 用细网格统计像素密度, 缝隙区域密度=0
        grid_size = 200  # 200x200 网格
        x_range = (-200, 200)
        y_range = (-200, 200)
        H, xedges, yedges = np.histogram2d(x_arcsec[mask], y_arcsec[mask],
                                            bins=grid_size, range=[x_range, y_range])
        im = ax.imshow(H.T, origin='lower', extent=[x_range[0], x_range[1], y_range[0], y_range[1]],
                       cmap='hot', aspect='equal')
        ax.set_xlabel("x (arcsec)")
        ax.set_ylabel("y (arcsec)")
        zero_cells = (H == 0).sum()
        total_cells = H.size
        ax.set_title(f"像素密度图 (黑色=缝隙, 零密度格={zero_cells}/{total_cells}={100*zero_cells/total_cells:.1f}%)")
        plt.colorbar(im, ax=ax, label='pixel count')

        out_path = r'f:\Astro dev\Astro CS Normalization Database\output\pipeline_debug\Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@061703-180S-Red\drizzle\diagnose_gaps.png'
        plt.tight_layout()
        plt.savefig(out_path, dpi=150, bbox_inches='tight')
        print(f"诊断图已保存: {out_path}")
        print(f"\n=== 缝隙结论 ===")
        print(f"零密度网格占比: {100*zero_cells/total_cells:.1f}%")
        print(f"邻居缺失率: {100*missing_count/total_nbrs:.2f}%")
    else:
        print("中心区域无像素, 无法生成诊断图")

if __name__ == '__main__':
    main()
