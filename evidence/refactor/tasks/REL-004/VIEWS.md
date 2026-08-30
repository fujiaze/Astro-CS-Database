# REL-004: 负责人视觉验收视图清单

Agent 无权宣布发布; 视图仅供负责人核对, 状态 = **READY_FOR_OWNER_REVIEW**。

| # | 视图 | 生成 | 内容 |
|---|------|------|------|
| 1 | 三块 overlap | tools/gen_visual_views.py | 三块亮度区相互重叠 (Phase2 overlap 语义) |
| 2 | 最弱背景 | 同上 | 低对比均匀背景 + 弱噪声 (SNR 下限) |
| 3 | 亮星区 | 同上 | 亮星 PSF (饱和/弥散核对) |
| 4 | 卫星线区 | 同上 | 卫星轨迹亮线 (排异候选) |
| 5 | support 边缘 | 同上 | 半图覆盖边缘 (coverage 0→1) |
| 6 | Phase3 FITS | 参考 v19r4 合成 HiPS tile | Phase3 输出参考 (dist/visual_views/view6_phase3_fits.fits) |

产物位置: `dist/visual_views/` (不入 VCS, 由 tools/gen_visual_views.py 再生)。
负责人批准后写发布决定; 之前不称完成。
