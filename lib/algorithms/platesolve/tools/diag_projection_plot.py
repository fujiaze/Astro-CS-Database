# NON_PRODUCTION_TOOL_ONLY
# This file is NOT part of the production pipeline.
# It is a development/testing/research tool only.
# The production pipeline uses orchestrator.exe <stage1.json> exclusively.

# -*- coding: utf-8 -*-
"""生成 WCS 投影精度可视化图
展示 Gaia 星投影位置 (红十字) 与图像实际星点在不同径向距离下的对齐情况
"""
import os
import sys
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
# 配置中文字体 (修复 CJK 字体缺失警告)
plt.rcParams['font.sans-serif'] = ['Microsoft YaHei', 'SimHei', 'Arial Unicode MS', 'DejaVu Sans']
plt.rcParams['axes.unicode_minus'] = False

_PROJECT_ROOT = os.path.normpath(os.path.dirname(os.path.abspath(__file__)))
_MINGW_BIN = r"C:\msys64\mingw64\bin"
os.environ["PATH"] = _MINGW_BIN + os.pathsep + os.environ.get("PATH", "")
if hasattr(os, "add_dll_directory"):
    try:
        os.add_dll_directory(_MINGW_BIN)
    except OSError:
        pass

_ASTRO_IO_DIR = os.path.join(_PROJECT_ROOT, "lib", "astro_image_io")
os.environ["PATH"] = _ASTRO_IO_DIR + os.pathsep + os.environ.get("PATH", "")
if hasattr(os, "add_dll_directory"):
    try:
        os.add_dll_directory(_ASTRO_IO_DIR)
    except OSError:
        pass

sys.path.insert(0, os.path.join(_PROJECT_ROOT, "lib", "astro_image_io", "python"))
sys.path.insert(0, os.path.join(_PROJECT_ROOT, "lib", "photometric_calib",
                                "gradient_estimator", "python"))
sys.path.insert(0, os.path.join(_PROJECT_ROOT, "lib", "photometric_calib",
                                "spectrum_integrator", "python"))
from astro_image_io import ImageReader
from wcs_transform import WCSTransform
from gaia_spectrum_client import GaiaSpectrumClient
from astropy.io import fits as astropy_fits
from astropy.coordinates import angular_separation


def main():
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', default=os.path.join(_PROJECT_ROOT, "diag_projection.png"),
                        help='输出图路径')
    parser.add_argument('--fits', default=None, help='指定 FITS 路径 (默认用 Galaxy_Center panel1 Red)')
    parser.add_argument('--label', default='', help='图上标注 (如 "SIP off" 或 "SIP on")')
    parser.add_argument('--no-sip', action='store_true',
                        help='强制不使用 SIP (只读 CD), 用于生成 before 图')
    args = parser.parse_args()

    if args.fits:
        fits_path = args.fits
    else:
        fits_path = os.path.join(
            _PROJECT_ROOT, "testdata", "results", "Galaxy_Center_T4", "panel1", "Red",
            "Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@061703-180S-Red",
            "01_calibrated.fits")
    out_path = args.output
    label_tag = args.label

    # 读取 WCS
    with astropy_fits.open(fits_path, mode='readonly') as hdul:
        header = hdul[0].header
    sip_order = int(header.get('A_ORDER', 0))
    sip_a = None
    sip_b = None
    if sip_order > 0:
        sip_a = [0.0] * 36
        sip_b = [0.0] * 36
        for i in range(sip_order + 1):
            for j in range(sip_order + 1 - i):
                key_a = 'A_%d_%d' % (i, j)
                key_b = 'B_%d_%d' % (i, j)
                if key_a in header:
                    sip_a[i * 6 + j] = float(header[key_a])
                if key_b in header:
                    sip_b[i * 6 + j] = float(header[key_b])

    # 读取逆向 SIP (AP/BP) — world2pix 反投影用
    ap_order = int(header.get('AP_ORDER', 0))
    sip_ap = None
    sip_bp = None
    if ap_order > 0:
        sip_ap = [0.0] * 36
        sip_bp = [0.0] * 36
        for i in range(ap_order + 1):
            for j in range(ap_order + 1 - i):
                key_ap = 'AP_%d_%d' % (i, j)
                key_bp = 'BP_%d_%d' % (i, j)
                if key_ap in header:
                    sip_ap[i * 6 + j] = float(header[key_ap])
                if key_bp in header:
                    sip_bp[i * 6 + j] = float(header[key_bp])

    # --no-sip: 强制只读 CD (生成 before 图, 确认 SIP 修正效果)
    if args.no_sip:
        sip_order = 0
        ap_order = 0
        sip_a = None
        sip_b = None
        sip_ap = None
        sip_bp = None
        print("[before 模式] 强制不使用 SIP, 只读 CD 矩阵")

    wcs_transform = WCSTransform(
        crpix1=float(header.get('CRPIX1', 0.0)),
        crpix2=float(header.get('CRPIX2', 0.0)),
        crval1=float(header.get('CRVAL1', 0.0)),
        crval2=float(header.get('CRVAL2', 0.0)),
        cd11=float(header.get('CD1_1', 0.0)),
        cd12=float(header.get('CD1_2', 0.0)),
        cd21=float(header.get('CD2_1', 0.0)),
        cd22=float(header.get('CD2_2', 0.0)),
        sip_order=sip_order,
        sip_a=sip_a if sip_order > 0 else None,
        sip_b=sip_b if sip_order > 0 else None,
        sip_ap_order=ap_order,
        sip_ap=sip_ap if ap_order > 0 else None,
        sip_bp=sip_bp if ap_order > 0 else None,
        ctype1=str(header.get('CTYPE1', 'RA---TAN')),
        ctype2=str(header.get('CTYPE2', 'DEC--TAN')),
    )
    print("WCS: CTYPE=(%s, %s), SIP_ORDER=%d, AP_ORDER=%d" % (
        header.get('CTYPE1', ''), header.get('CTYPE2', ''), sip_order, ap_order))

    reader = ImageReader()
    image_data = reader.read(fits_path)
    image = image_data.data
    img_w, img_h = image_data.width, image_data.height

    # Gaia 查询
    ra_center_arr, dec_center_arr = wcs_transform.pixel_to_sky_batch(
        np.array([img_w / 2.0]), np.array([img_h / 2.0]))
    ra_center = float(ra_center_arr[0])
    dec_center = float(dec_center_arr[0])
    corner_xs = np.array([0.0, float(img_w), 0.0, float(img_w)])
    corner_ys = np.array([0.0, 0.0, float(img_h), float(img_h)])
    corner_ra, corner_dec = wcs_transform.pixel_to_sky_batch(corner_xs, corner_ys)
    max_sep_rad = 0.0
    for i in range(4):
        sep = angular_separation(
            ra_center * np.pi / 180.0, dec_center * np.pi / 180.0,
            float(corner_ra[i]) * np.pi / 180.0, float(corner_dec[i]) * np.pi / 180.0)
        if sep > max_sep_rad:
            max_sep_rad = sep
    fov_radius_deg = float(max_sep_rad * 180.0 / np.pi)
    cone_radius_deg = fov_radius_deg * 1.05

    gaia_data_dir = os.path.join(_PROJECT_ROOT, "GaiaDR3SP")
    with GaiaSpectrumClient(gaia_data_dir, db_type=2) as client:
        gaia_stars = client.cone_search_with_spectrum(
            ra_center, dec_center, cone_radius_deg, 8.0, 16.0)

    gaia_ra = np.array([s.ra for s in gaia_stars], dtype=np.float64)
    gaia_dec = np.array([s.dec for s in gaia_stars], dtype=np.float64)
    gaia_mag = np.array([s.mag_g for s in gaia_stars], dtype=np.float64)
    gaia_px, gaia_py = wcs_transform.sky_to_pixel_batch(gaia_ra, gaia_dec)
    in_img = (gaia_px >= 20) & (gaia_px < img_w - 20) & (gaia_py >= 20) & (gaia_py < img_h - 20)
    gaia_px = gaia_px[in_img]
    gaia_py = gaia_py[in_img]
    gaia_mag = gaia_mag[in_img]
    print("Gaia 星 (图像内): %d" % len(gaia_px))

    cx_c, cy_c = img_w / 2.0, img_h / 2.0
    r_max = float(np.sqrt(img_w**2 + img_h**2)) / 2.0
    r_arr = np.sqrt((gaia_px - cx_c)**2 + (gaia_py - cy_c)**2) / r_max

    # 选 4 个目标区域: 中心, r=0.4, r=0.6, r=0.8
    # 每个区域选 1 颗亮星 (mag 最小), 放大显示 40x40 像素
    targets = []
    for r_lo, r_hi, label in [(0.0, 0.2, "中心 r<0.2"),
                              (0.3, 0.5, "r=[0.3,0.5)"),
                              (0.5, 0.7, "r=[0.5,0.7)"),
                              (0.7, 1.0, "r=[0.7,1.0) 边缘")]:
        mask = (r_arr >= r_lo) & (r_arr < r_hi) & (gaia_mag < 11.0)
        if mask.sum() == 0:
            print("区域 %s 无亮星" % label)
            continue
        # 选该区域最亮的星
        idx = np.where(mask)[0]
        best = idx[np.argmin(gaia_mag[mask])]
        targets.append((best, label))

    print("选定 %d 个目标区域" % len(targets))

    # 生成图: 2x3 布局
    # 第一行: 全图概览 (Gaia 投影位置) + 2 个放大区
    # 第二行: 2 个放大区 + 偏移统计
    fig = plt.figure(figsize=(18, 12))

    # 1. 全图概览
    ax1 = fig.add_subplot(2, 3, 1)
    # 用对数拉伸显示图像
    img_disp = np.log1p(np.clip(image, 0, None))
    # 降采样显示 (每 10 个像素取 1 个)
    ax1.imshow(img_disp[::10, ::10], cmap='gray', origin='lower',
               extent=[0, img_w, 0, img_h], aspect='auto')
    # Gaia 投影位置 (红点)
    ax1.scatter(gaia_px[::20], gaia_py[::20], c='red', s=1, alpha=0.3, label='Gaia 投影')
    # 标记目标区域
    for i, (idx, label) in enumerate(targets):
        ax1.plot(gaia_px[idx], gaia_py[idx], 'c+', markersize=15, markeredgewidth=2)
        ax1.annotate(label, (gaia_px[idx], gaia_py[idx]),
                     textcoords="offset points", xytext=(10, 10),
                     color='cyan', fontsize=8)
    ax1.set_title('全图概览 (降采样10x + Gaia投影) %s' % (
        '[%s]' % label_tag if label_tag else ''), fontsize=10)
    ax1.set_xlabel('X (pixel)')
    ax1.set_ylabel('Y (pixel)')
    ax1.legend(loc='upper right', fontsize=8)

    # 2-5. 四个放大区
    for i, (idx, label) in enumerate(targets):
        ax = fig.add_subplot(2, 3, i + 2)
        cx, cy = gaia_px[idx], gaia_py[idx]
        half = 25  # 50x50 像素窗口
        x0 = int(max(0, cx - half))
        y0 = int(max(0, cy - half))
        x1 = int(min(img_w, cx + half))
        y1 = int(min(img_h, cy + half))
        patch = image[y0:y1, x0:x1]

        # 显示 patch (用对数拉伸)
        ax.imshow(np.log1p(np.clip(patch, 0, None)), cmap='gray', origin='lower',
                  extent=[x0, x1, y0, y1], aspect='equal')

        # Gaia 投影位置 (红十字)
        ax.plot(cx, cy, 'r+', markersize=20, markeredgewidth=2, label='Gaia投影位置')

        # 该窗口内所有 Gaia 星
        local_mask = (np.abs(gaia_px - cx) < half) & (np.abs(gaia_py - cy) < half)
        if local_mask.sum() > 0:
            ax.scatter(gaia_px[local_mask], gaia_py[local_mask],
                       c='red', s=30, marker='+', linewidths=1)

        mag = gaia_mag[idx]
        r = r_arr[idx]
        ax.set_title('%s\n星 mag=%.2f r=%.2f (%.1f,%.1f)' % (
            label, mag, r, cx, cy), fontsize=9)
        ax.set_xlabel('X (pixel)')
        ax.set_ylabel('Y (pixel)')
        ax.legend(loc='upper right', fontsize=8)

        # 打印该星信息
        print("  %s: mag=%.2f r=%.2f pos=(%.1f, %.1f)" % (label, mag, r, cx, cy))

    # 6. 偏移统计图
    ax6 = fig.add_subplot(2, 3, 6)
    # 对所有亮星 (mag<11) 计算窗口内亮峰偏移
    bright_mask = gaia_mag < 11.0
    bright_px = gaia_px[bright_mask]
    bright_py = gaia_py[bright_mask]
    bright_r = r_arr[bright_mask]

    offsets = []
    offset_r = []
    for i in range(len(bright_px)):
        cx, cy = bright_px[i], bright_py[i]
        half = 8
        x0 = int(max(0, cx - half))
        y0 = int(max(0, cy - half))
        x1 = int(min(img_w, cx + half + 1))
        y1 = int(min(img_h, cy + half + 1))
        patch = image[y0:y1, x0:x1].astype(np.float64)
        if patch.size == 0:
            continue
        bkg = np.median(patch)
        peak_val = patch.max()
        if peak_val < bkg + 1000:
            continue
        # 质心
        threshold = bkg + 0.1 * (peak_val - bkg)
        mask = patch > threshold
        if mask.sum() > 0:
            ys, xs = np.where(mask)
            weights = patch[mask] - bkg
            cen_x = x0 + np.sum(xs * weights) / np.sum(weights)
            cen_y = y0 + np.sum(ys * weights) / np.sum(weights)
            off = np.sqrt((cen_x - cx)**2 + (cen_y - cy)**2)
            offsets.append(off)
            offset_r.append(bright_r[i])

    offsets = np.array(offsets)
    offset_r = np.array(offset_r)
    ax6.scatter(offset_r, offsets, c='blue', s=5, alpha=0.3, label='单星偏移')

    # 分箱 median 拟合曲线 (误差条带)
    bin_edges = np.linspace(0, 1.0, 11)  # 10 个箱, 0.0-1.0
    bin_centers = []
    bin_medians = []
    bin_p25 = []
    bin_p75 = []
    for i in range(len(bin_edges) - 1):
        mask = (offset_r >= bin_edges[i]) & (offset_r < bin_edges[i+1])
        n = mask.sum()
        if n < 3:
            continue
        bin_centers.append((bin_edges[i] + bin_edges[i+1]) / 2)
        bin_medians.append(np.median(offsets[mask]))
        bin_p25.append(np.percentile(offsets[mask], 25))
        bin_p75.append(np.percentile(offsets[mask], 75))

    if bin_centers:
        bin_centers = np.array(bin_centers)
        bin_medians = np.array(bin_medians)
        bin_p25 = np.array(bin_p25)
        bin_p75 = np.array(bin_p75)
        # 绘制 median 拟合曲线
        ax6.plot(bin_centers, bin_medians, 'r-', linewidth=2, label='median')
        # 绘制 25-75% 误差条带
        ax6.fill_between(bin_centers, bin_p25, bin_p75, color='red', alpha=0.2, label='25-75%')

    ax6.axhline(y=2.0, color='red', linestyle='--', alpha=0.5, label='2px 阈值')
    ax6.axhline(y=1.0, color='orange', linestyle='--', alpha=0.5, label='1px')
    ax6.set_xlabel('归一化径向距离 r')
    ax6.set_ylabel('质心偏移 (像素)')
    ax6.set_title('WCS 投影残差: Gaia投影 vs 实际亮峰 %s (mag<11, n=%d)' % (
        '[%s]' % label_tag if label_tag else '', len(offsets)), fontsize=9)
    ax6.legend(fontsize=7, loc='upper left')
    ax6.set_ylim(0, min(20, np.percentile(offsets, 99) * 1.5))
    ax6.set_xlim(0, 1.0)

    # 打印统计
    print("\n偏移统计:")
    for r_lo, r_hi in [(0, 0.2), (0.2, 0.4), (0.4, 0.6), (0.6, 0.8), (0.8, 1.0)]:
        mask = (offset_r >= r_lo) & (offset_r < r_hi)
        n = mask.sum()
        if n == 0:
            continue
        off = offsets[mask]
        print("  r=[%.1f,%.1f): n=%d, median=%.2fpx, mean=%.2fpx, >2px=%d (%.1f%%)" % (
            r_lo, r_hi, n, np.median(off), np.mean(off),
            np.sum(off > 2), 100.0 * np.sum(off > 2) / n))

    plt.tight_layout()
    plt.savefig(out_path, dpi=120, bbox_inches='tight')
    print("\n投影图已保存: %s" % out_path)
    plt.close()


if __name__ == "__main__":
    main()
