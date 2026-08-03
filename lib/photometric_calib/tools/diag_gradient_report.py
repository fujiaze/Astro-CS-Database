# NON_PRODUCTION_TOOL_ONLY
# This file is NOT part of the production pipeline.
# It is a development/testing/research tool only.
# The production pipeline uses orchestrator.exe <stage1.json> exclusively.

# -*- coding: utf-8 -*-
"""
光度拟合梯度诊断报告生成器
功能: 读取 step4 输出的残差 CSV，生成可视化诊断图
用途: 检查乘性/加性梯度拟合效果，发现空间模式或离群点
输出: diag_gradient_report.png (6合1 诊断图)
"""

import os
import sys
import csv
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.gridspec import GridSpec

# 中文字体
plt.rcParams['font.sans-serif'] = ['Microsoft YaHei', 'SimHei', 'Arial Unicode MS', 'DejaVu Sans']
plt.rcParams['axes.unicode_minus'] = False


def load_residuals(csv_path):
    """读取残差 CSV"""
    x, y, obs, fit, w = [], [], [], [], []
    with open(csv_path, 'r', encoding='utf-8') as f:
        rd = csv.reader(f)
        next(rd)
        for row in rd:
            x.append(float(row[0]))
            y.append(float(row[1]))
            obs.append(float(row[2]))
            fit.append(float(row[3]))
            w.append(float(row[4]))
    return (np.array(x), np.array(y), np.array(obs),
            np.array(fit), np.array(w))


def plot_gradient_report(frame_dir, output_path):
    """生成 6合1 梯度诊断报告"""
    mult_csv = os.path.join(frame_dir, "04_residuals_mult.csv")
    add_csv = os.path.join(frame_dir, "04_residuals_add.csv")
    report_json = os.path.join(frame_dir, "04_quality_report.json")

    import json
    with open(report_json, 'r', encoding='utf-8') as f:
        report = json.load(f)

    # 读取残差
    mx, my, m_obs, m_fit, m_w = load_residuals(mult_csv)
    ax_, ay_, a_obs, a_fit, a_w = load_residuals(add_csv)

    m_resid = m_obs - m_fit
    a_resid = a_obs - a_fit

    fig = plt.figure(figsize=(18, 12))
    fig.suptitle(
        f"光度拟合梯度诊断报告\n"
        f"帧: {os.path.basename(frame_dir)[:60]}\n"
        f"n_matched={report['n_matched']}, n_excluded={report['n_excluded']}, "
        f"scale={report['scale_factor']:.4e}",
        fontsize=13, fontweight='bold')

    gs = GridSpec(3, 4, figure=fig, hspace=0.35, wspace=0.3)

    # ========== 乘性梯度 (上排) ==========
    # 1. 乘性: observed_r 空间分布
    ax1 = fig.add_subplot(gs[0, 0])
    sc = ax1.scatter(mx, my, c=m_obs, s=15, cmap='viridis', edgecolors='none')
    ax1.set_title(f"乘性 observed_r\n(log10(F_instr/F_syn))\n"
                  f"median={np.median(m_obs):.3f}, std={np.std(m_obs):.3f}",
                  fontsize=10)
    ax1.set_xlabel("X (px)")
    ax1.set_ylabel("Y (px)")
    plt.colorbar(sc, ax=ax1, label='r', shrink=0.8)

    # 2. 乘性: fitted_r 空间分布
    ax2 = fig.add_subplot(gs[0, 1])
    sc = ax2.scatter(mx, my, c=m_fit, s=15, cmap='viridis', edgecolors='none')
    ax2.set_title(f"乘性 fitted_r\n"
                  f"R²={report['mult_r_squared']:.4f}, "
                  f"skipped={report['mult_skipped']}",
                  fontsize=10)
    ax2.set_xlabel("X (px)")
    ax2.set_ylabel("Y (px)")
    plt.colorbar(sc, ax=ax2, label='r', shrink=0.8)

    # 3. 乘性: observed vs fitted 散点
    ax3 = fig.add_subplot(gs[0, 2])
    ax3.scatter(m_fit, m_obs, s=15, alpha=0.6, c=m_w, cmap='coolwarm',
                edgecolors='none')
    lim = [min(m_obs.min(), m_fit.min()), max(m_obs.max(), m_fit.max())]
    ax3.plot(lim, lim, 'k--', lw=1, label='y=x')
    ax3.set_xlabel("fitted_r")
    ax3.set_ylabel("observed_r")
    ax3.set_title(f"乘性: obs vs fit\n"
                  f"resid std={np.std(m_resid):.4f}",
                  fontsize=10)
    ax3.legend(fontsize=8)

    # 4. 乘性: 残差空间分布
    ax4 = fig.add_subplot(gs[0, 3])
    sc = ax4.scatter(mx, my, c=m_resid, s=15, cmap='RdBu_r',
                     edgecolors='none', vmin=-np.std(m_resid)*3,
                     vmax=np.std(m_resid)*3)
    ax4.set_title(f"乘性残差 (obs-fit)\n"
                  f"median={np.median(m_resid):.4f}",
                  fontsize=10)
    ax4.set_xlabel("X (px)")
    ax4.set_ylabel("Y (px)")
    plt.colorbar(sc, ax=ax4, label='resid', shrink=0.8)

    # ========== 加性梯度 (中排) ==========
    # 5. 加性: observed_b 空间分布
    ax5 = fig.add_subplot(gs[1, 0])
    sc = ax5.scatter(ax_, ay_, c=a_obs, s=15, cmap='plasma', edgecolors='none')
    ax5.set_title(f"加性 observed_b\n(PSF 背景值)\n"
                  f"median={np.median(a_obs):.1f}, std={np.std(a_obs):.1f}",
                  fontsize=10)
    ax5.set_xlabel("X (px)")
    ax5.set_ylabel("Y (px)")
    plt.colorbar(sc, ax=ax5, label='B', shrink=0.8)

    # 6. 加性: fitted_b 空间分布
    ax6 = fig.add_subplot(gs[1, 1])
    sc = ax6.scatter(ax_, ay_, c=a_fit, s=15, cmap='plasma', edgecolors='none')
    ax6.set_title(f"加性 fitted_b (阶数={report['add_order']})\n"
                  f"R²={report['add_r_squared']:.4f}",
                  fontsize=10)
    ax6.set_xlabel("X (px)")
    ax6.set_ylabel("Y (px)")
    plt.colorbar(sc, ax=ax6, label='B', shrink=0.8)

    # 7. 加性: observed vs fitted 散点
    ax7 = fig.add_subplot(gs[1, 2])
    ax7.scatter(a_fit, a_obs, s=15, alpha=0.6, c=a_w, cmap='coolwarm',
                edgecolors='none')
    lim = [min(a_obs.min(), a_fit.min()), max(a_obs.max(), a_fit.max())]
    ax7.plot(lim, lim, 'k--', lw=1, label='y=x')
    ax7.set_xlabel("fitted_b")
    ax7.set_ylabel("observed_b")
    ax7.set_title(f"加性: obs vs fit\n"
                  f"resid std={np.std(a_resid):.1f}",
                  fontsize=10)
    ax7.legend(fontsize=8)

    # 8. 加性: 残差空间分布
    ax8 = fig.add_subplot(gs[1, 3])
    resid_clip = np.clip(a_resid, -np.std(a_resid)*3, np.std(a_resid)*3)
    sc = ax8.scatter(ax_, ay_, c=resid_clip, s=15, cmap='RdBu_r',
                     edgecolors='none')
    ax8.set_title(f"加性残差 (obs-fit)\n"
                  f"median={np.median(a_resid):.1f}, std={np.std(a_resid):.1f}",
                  fontsize=10)
    ax8.set_xlabel("X (px)")
    ax8.set_ylabel("Y (px)")
    plt.colorbar(sc, ax=ax8, label='resid', shrink=0.8)

    # ========== 统计摘要 (下排) ==========
    # 9. 加性残差直方图
    ax9 = fig.add_subplot(gs[2, 0:2])
    inlier = a_w > 0
    outlier = a_w == 0
    ax9.hist(a_resid[inlier], bins=40, alpha=0.7, color='steelblue',
             label=f'IRLS内点 (n={np.sum(inlier)})')
    if np.any(outlier):
        ax9.hist(a_resid[outlier], bins=20, alpha=0.7, color='red',
                 label=f'IRLS离群 (n={np.sum(outlier)})')
    ax9.axvline(0, color='k', linestyle='--', lw=1)
    ax9.axvline(np.median(a_resid), color='orange', linestyle='-', lw=2,
                label=f'中位数={np.median(a_resid):.1f}')
    ax9.set_xlabel("加性残差 (obs - fit)")
    ax9.set_ylabel("频数")
    ax9.set_title("加性残差分布直方图", fontsize=10)
    ax9.legend(fontsize=9)

    # 10. 加性 observed_b 分位数图
    ax10 = fig.add_subplot(gs[2, 2])
    sorted_obs = np.sort(a_obs)
    pct = np.linspace(0, 100, len(sorted_obs))
    ax10.plot(pct, sorted_obs, 'b-', lw=1.5)
    ax10.axhline(np.median(a_obs), color='orange', linestyle='--',
                 label=f'P50={np.median(a_obs):.1f}')
    ax10.axhline(np.percentile(a_obs, 90), color='red', linestyle='--',
                 label=f'P90={np.percentile(a_obs, 90):.1f}')
    ax10.set_xlabel("百分位")
    ax10.set_ylabel("observed_b")
    ax10.set_title("加性 observed_b 分位数", fontsize=10)
    ax10.legend(fontsize=8)

    # 11. 质量报告摘要
    ax11 = fig.add_subplot(gs[2, 3])
    ax11.axis('off')
    summary_text = (
        "质量报告摘要\n"
        f"{'─' * 30}\n"
        f"匹配星数:     {report['n_matched']}\n"
        f"排除离群:     {report['n_excluded']}\n"
        f"{'─' * 30}\n"
        f"乘性梯度:\n"
        f"  R² = {report['mult_r_squared']:.4f}\n"
        f"  阶数 = {report['mult_order']}\n"
        f"  跳过 = {report['mult_skipped']}\n"
        f"  残差std = {report['mult_residual_std']:.4f}\n"
        f"{'─' * 30}\n"
        f"加性梯度:\n"
        f"  R² = {report['add_r_squared']:.4f}\n"
        f"  阶数 = {report['add_order']}\n"
        f"  跳过 = {report['add_skipped']}\n"
        f"  残差std = {report['add_residual_std']:.1f}\n"
        f"{'─' * 30}\n"
        f"scale_factor = {report['scale_factor']:.4e}\n"
    )
    ax11.text(0.05, 0.95, summary_text, transform=ax11.transAxes,
              fontsize=10, verticalalignment='top',
              fontfamily='Microsoft YaHei',
              bbox=dict(boxstyle='round', facecolor='lightyellow', alpha=0.8))

    plt.savefig(output_path, dpi=120, bbox_inches='tight')
    plt.close()
    print(f"诊断报告已保存: {output_path}")


if __name__ == '__main__':
    frame_dir = r"testdata\results\Galaxy_Center_T4\panel1\Red\Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@061703-180S-Red"
    output_path = "diag_gradient_report.png"
    plot_gradient_report(frame_dir, output_path)
