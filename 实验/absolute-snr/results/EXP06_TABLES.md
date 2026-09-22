# EXP-06 表格（由 results/exp06_*.json 生成）

## A 纯解析代数合成（臂 A）

规模：48 帧（8 场景 x 6 seed），[1024, 1024]，Delta=64 px。

### A-T1 权重效率损失 E（中位；E=0 最优，尺度不变）

| 场景 | frame_scalar（帧级标量） | dense_patch（块常数） | interp_bilinear（现行生产算子） | interp_spline（EXP-04 推荐算子） | **phys（物理建模，推荐）** | phys_resid（物理+残差插值） | phys_free（自由斜率） | **phys_auto（推荐默认：斜率可辨识则用数据，否则常数）** | phys_auto_resid（推荐默认 + 残差插值） | phys_med3drv（mesh 中值滤波驱动） | phys_unsep（不做结构分离，错误臂） | phys_r0ctrl（R0 控制值，错误臂） | naive_pixel（逐像素代入亮度，错误臂） |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| A0_flat | 6.11e-16 | 0.00059 | 0.00027 | 0.00044 | 1.03e-06 | 0.00044 | 1.70e-06 | 5.00e-15 | 0.00044 | -2.00e-15 | 1.19e-06 | 1.70e-06 | 0.00427 |
| A1_flat_bright_pixels | 4.22e-15 | 0.00077 | 0.00037 | 0.00060 | 1.67e-06 | 0.00060 | 3.40e-06 | 9.09e-07 | 0.00060 | 3.22e-15 | 0.03155 | 3.40e-06 | 0.00523 |
| A2_gradient | 0.03066 | 0.00073 | 0.00029 | 0.00047 | 3.62e-05 | 0.00047 | 8.24e-06 | 8.24e-06 | 0.00047 | 3.07e-05 | 2.62e-05 | 2.53e-05 | 0.00457 |
| A3_smooth_nebula | 0.07761 | 0.00134 | 0.00044 | 0.00060 | 0.00011 | 0.00059 | 5.42e-05 | 5.42e-05 | 0.00059 | 7.55e-05 | 4.09e-05 | 0.00029 | 0.00298 |
| A4_realistic | 0.05817 | 0.00319 | 0.00135 | 0.00201 | 8.61e-05 | 0.00204 | 7.34e-05 | 7.34e-05 | 0.00204 | 5.76e-05 | 0.04223 | 0.01512 | 0.01610 |
| A5_unresolved | 0.02301 | 0.01355 | 0.01719 | 0.01780 | 0.00196 | 0.02010 | 0.00015 | 0.00015 | 0.02006 | 0.00406 | 0.00454 | 0.00016 | 0.00567 |
| A6_strong_gradient | 0.49237 | 0.00352 | 0.00156 | 0.00164 | 0.00029 | 0.00164 | 0.00017 | 0.00017 | 0.00164 | 0.00030 | 0.01240 | 0.01119 | 0.01089 |
| A7_read_noise_dominated | -8.27e-15 | 0.00065 | 0.00031 | 0.00049 | 3.97e-06 | 0.00049 | 3.56e-05 | 3.56e-05 | 0.00049 | 1.37e-06 | 0.00527 | 3.39e-05 | 0.00682 |

### A-T2 权重效率损失 E（中位；E=0 最优，尺度不变）

| 场景 | frame_scalar（帧级标量） | dense_patch（块常数） | interp_bilinear（现行生产算子） | interp_spline（EXP-04 推荐算子） | **phys（物理建模，推荐）** | phys_resid（物理+残差插值） | phys_free（自由斜率） | **phys_auto（推荐默认：斜率可辨识则用数据，否则常数）** | phys_auto_resid（推荐默认 + 残差插值） | phys_med3drv（mesh 中值滤波驱动） | phys_unsep（不做结构分离，错误臂） | phys_r0ctrl（R0 控制值，错误臂） | naive_pixel（逐像素代入亮度，错误臂） |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| A0_flat | 6.11e-16 | 0.00059 | 0.00027 | 0.00044 | 1.03e-06 | 0.00044 | 1.70e-06 | 5.00e-15 | 0.00044 | -2.00e-15 | 1.19e-06 | 1.70e-06 | 0.00427 |
| A1_flat_bright_pixels | 0.14222 | 0.12358 | 0.12657 | 0.12303 | 0.14099 | 0.12302 | 0.14197 | 0.14130 | 0.12302 | 0.14222 | 0.03421 | 0.14197 | 0.00428 |
| A2_gradient | 0.03066 | 0.00073 | 0.00029 | 0.00047 | 3.62e-05 | 0.00047 | 8.24e-06 | 8.24e-06 | 0.00047 | 3.07e-05 | 2.62e-05 | 2.53e-05 | 0.00457 |
| A3_smooth_nebula | 0.07761 | 0.00134 | 0.00044 | 0.00060 | 0.00011 | 0.00059 | 5.42e-05 | 5.42e-05 | 0.00059 | 7.55e-05 | 4.09e-05 | 0.00029 | 0.00298 |
| A4_realistic | 0.21557 | 0.12116 | 0.12588 | 0.11897 | 0.15193 | 0.11884 | 0.15184 | 0.15184 | 0.11884 | 0.15322 | 0.06378 | 0.16600 | 0.00278 |
| A5_unresolved | 0.06382 | 0.05203 | 0.05724 | 0.05863 | 0.04397 | 0.06100 | 0.04274 | 0.04274 | 0.06086 | 0.04565 | 0.03190 | 0.04273 | 0.00384 |
| A6_strong_gradient | 0.54094 | 0.06408 | 0.06486 | 0.06343 | 0.06907 | 0.06336 | 0.06774 | 0.06774 | 0.06336 | 0.06932 | 0.04052 | 0.07549 | 0.00819 |
| A7_read_noise_dominated | 0.04384 | 0.04147 | 0.04203 | 0.04178 | 0.04358 | 0.04179 | 0.04292 | 0.04292 | 0.04184 | 0.04375 | 0.02991 | 0.04294 | 0.00450 |

### A-电平比 median(sigma_hat/sigma_true)（T1）

| 场景 | frame_scalar（帧级标量） | dense_patch（块常数） | interp_bilinear（现行生产算子） | interp_spline（EXP-04 推荐算子） | **phys（物理建模，推荐）** | phys_resid（物理+残差插值） | phys_free（自由斜率） | **phys_auto（推荐默认：斜率可辨识则用数据，否则常数）** | phys_auto_resid（推荐默认 + 残差插值） | phys_med3drv（mesh 中值滤波驱动） | phys_unsep（不做结构分离，错误臂） | phys_r0ctrl（R0 控制值，错误臂） | naive_pixel（逐像素代入亮度，错误臂） |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| A0_flat | 0.9825 | 0.9825 | 0.9827 | 0.9826 | 0.9829 | 0.9826 | 0.9830 | 0.9830 | 0.9826 | 0.9829 | 0.9829 | 0.9830 | 0.9829 |
| A1_flat_bright_pixels | 0.9824 | 0.9824 | 0.9830 | 0.9827 | 0.9825 | 0.9827 | 0.9823 | 0.9823 | 0.9827 | 0.9825 | 0.9086 | 0.9823 | 0.9825 |
| A2_gradient | 0.9869 | 0.9865 | 0.9865 | 0.9866 | 0.9861 | 0.9864 | 0.9861 | 0.9861 | 0.9865 | 0.9861 | 0.9859 | 1.0008 | 0.9860 |
| A3_smooth_nebula | 0.9896 | 0.9876 | 0.9871 | 0.9879 | 0.9858 | 0.9881 | 0.9869 | 0.9869 | 0.9881 | 0.9861 | 0.9876 | 1.1383 | 0.9864 |
| A4_realistic | 0.9954 | 0.9954 | 0.9948 | 0.9952 | 0.9929 | 0.9951 | 0.9927 | 0.9927 | 0.9951 | 0.9929 | 0.9490 | 1.1788 | 0.9935 |
| A5_unresolved | 0.9935 | 0.9912 | 0.9911 | 0.9903 | 1.1754 | 0.9901 | 0.9904 | 0.9904 | 0.9901 | 1.1769 | 1.1717 | 1.0000 | 1.1771 |
| A6_strong_gradient | 0.9862 | 0.9910 | 0.9904 | 0.9905 | 0.9913 | 0.9900 | 0.9903 | 0.9903 | 0.9900 | 0.9919 | 0.9878 | 1.1980 | 0.9910 |
| A7_read_noise_dominated | 0.9896 | 0.9896 | 0.9900 | 0.9898 | 0.9896 | 0.9899 | 0.9888 | 0.9888 | 0.9900 | 0.9895 | 0.9830 | 0.9888 | 0.9895 |

### A-源区失效模式（T1 口径；src_level_ratio 越接近 1 越好，src_snr_p05 越小越差）

| 场景 | 方法 | 源区 sigma 比 | 源区 SNR p05 | 全帧 SNR 最大比 |
|---|---|---|---|---|
| A0_flat | phys | n/a | n/a | 1.020 |
| A0_flat | interp_spline | n/a | n/a | 1.049 |
| A0_flat | naive_pixel | n/a | n/a | 1.223 |
| A0_flat | phys_unsep | n/a | n/a | 1.025 |
| A1_flat_bright_pixels | phys | 0.985 | 1.014 | 1.021 |
| A1_flat_bright_pixels | interp_spline | 1.018 | 0.951 | 1.050 |
| A1_flat_bright_pixels | naive_pixel | 1.163 | 0.067 | 1.220 |
| A1_flat_bright_pixels | phys_unsep | 2.178 | 0.380 | 1.106 |
| A2_gradient | phys | n/a | n/a | 1.030 |
| A2_gradient | interp_spline | n/a | n/a | 1.054 |
| A2_gradient | naive_pixel | n/a | n/a | 1.258 |
| A2_gradient | phys_unsep | n/a | n/a | 1.025 |
| A3_smooth_nebula | phys | n/a | n/a | 1.037 |
| A3_smooth_nebula | interp_spline | n/a | n/a | 1.051 |
| A3_smooth_nebula | naive_pixel | n/a | n/a | 1.200 |
| A3_smooth_nebula | phys_unsep | n/a | n/a | 1.024 |
| A4_realistic | phys | 0.995 | 1.001 | 1.028 |
| A4_realistic | interp_spline | 1.065 | 0.901 | 1.058 |
| A4_realistic | naive_pixel | 1.665 | 0.224 | 1.182 |
| A4_realistic | phys_unsep | 1.486 | 0.563 | 1.177 |
| A5_unresolved | phys | 1.176 | 0.813 | 1.038 |
| A5_unresolved | interp_spline | 1.002 | 0.885 | 1.223 |
| A5_unresolved | naive_pixel | 1.283 | 0.223 | 0.979 |
| A5_unresolved | phys_unsep | 1.226 | 0.711 | 1.023 |
| A6_strong_gradient | phys | 0.988 | 1.003 | 1.037 |
| A6_strong_gradient | interp_spline | 1.005 | 0.956 | 1.051 |
| A6_strong_gradient | naive_pixel | 1.081 | 0.230 | 1.402 |
| A6_strong_gradient | phys_unsep | 1.065 | 0.729 | 1.068 |
| A7_read_noise_dominated | phys | 0.991 | 1.007 | 1.012 |
| A7_read_noise_dominated | interp_spline | 1.000 | 0.981 | 1.046 |
| A7_read_noise_dominated | naive_pixel | 1.028 | 0.324 | 1.231 |
| A7_read_noise_dominated | phys_unsep | 1.048 | 0.791 | 1.029 |

### A-拟合诊断（中位）

| 场景 | 自由斜率 gain_hat | 偏差 vs 真值 1.3 | lever_var | c_se/c | 固定斜率 a | 固定斜率 R2 | 幂律 p |
|---|---|---|---|---|---|---|---|
| A0_flat | 1.170 | -10.0%% | 0.002 | 2.254 | 146.4 | 0.001 | 1.042 +- 2.404 |
| A1_flat_bright_pixels | 0.756 | -41.8%% | 0.003 | 4.00e+08 | 146.3 | -0.004 | 3.000 +- 2.222 |
| A2_gradient | 1.343 | 3.3%% | 0.493 | 0.012 | 147.3 | 0.974 | 1.017 +- 0.030 |
| A3_smooth_nebula | 1.335 | 2.7%% | 0.816 | 0.006 | 260.1 | 0.991 | 0.999 +- 0.021 |
| A4_realistic | 1.308 | 0.6%% | 0.618 | 0.011 | 283.1 | 0.979 | 0.995 +- 0.053 |
| A5_unresolved | 1.320 | 1.6%% | 0.285 | 0.016 | 211.6 | 0.063 | 3.000 +- 0.073 |
| A6_strong_gradient | 1.338 | 2.9%% | 1.643 | 0.004 | 166.7 | 0.997 | 1.004 +- 0.012 |
| A7_read_noise_dominated | 0.430 | -66.9%% | 0.007 | 0.233 | 137.4 | 0.046 | 2.894 +- 0.835 |

### A-负例（真值无效应）

| 负例 | seed | 结果 |
|---|---|---|
| N1 平坦真值场 | 20261927 | phys 离散度 0.0031；interp 离散度 0.0451；phys E=1.70e-06 |
| N1 平坦真值场 | 20262927 | phys 离散度 0.0028；interp 离散度 0.0499；phys E=6.26e-07 |
| N1 平坦真值场 | 20263927 | phys 离散度 0.0031；interp 离散度 0.0419；phys E=5.02e-07 |
| N1 平坦真值场 | 20264927 | phys 离散度 0.0029；interp 离散度 0.0355；phys E=1.08e-06 |
| N2 零噪声真值 | 20261927 | truth_is_zero=True；度量判退化=True |
| N2 零噪声真值 | 20262927 | truth_is_zero=True；度量判退化=True |
| N2 零噪声真值 | 20263927 | truth_is_zero=True；度量判退化=True |

## B HST 真实模板 + 物理前向仿真（臂 B）

### B-HST：权重效率损失 E（T1 / T2）与电平比

| 场景 | 帧 | 方法 | E(T1) | E(T2) | 电平比(T1) | 源区 SNR p05(T1) |
|---|---|---|---|---|---|---|
| B0_hst_base | 0 | phys | 0.00081 | 0.03087 | 1.0378 | 0.956 |
| B0_hst_base | 0 | interp_spline | 0.00804 | 0.02791 | 1.0391 | 0.863 |
| B0_hst_base | 0 | frame_scalar | 2.75e-05 | 0.03081 | 1.0396 | 0.958 |
| B0_hst_base | 0 | naive_pixel | 0.01116 | 0.00382 | 1.0353 | 0.906 |
| B0_hst_base | 0 | phys_unsep | 0.01175 | 0.02245 | 1.0348 | 0.880 |
| B1_hst_stars | 0 | phys | 0.00078 | 0.06445 | 1.0487 | 0.946 |
| B1_hst_stars | 0 | interp_spline | 0.00856 | 0.06236 | 1.0414 | 0.862 |
| B1_hst_stars | 0 | frame_scalar | 2.75e-05 | 0.06480 | 1.0417 | 0.956 |
| B1_hst_stars | 0 | naive_pixel | 0.01169 | 0.00366 | 1.0464 | 0.896 |
| B1_hst_stars | 0 | phys_unsep | 0.01467 | 0.04960 | 1.0381 | 0.861 |
| B1_hst_stars | 1 | phys | 0.00078 | 0.06455 | 1.0473 | 0.947 |
| B1_hst_stars | 1 | interp_spline | 0.00840 | 0.06250 | 1.0422 | 0.857 |
| B1_hst_stars | 1 | frame_scalar | 2.75e-05 | 0.06480 | 1.0428 | 0.955 |
| B1_hst_stars | 1 | naive_pixel | 0.01171 | 0.00368 | 1.0450 | 0.897 |
| B1_hst_stars | 1 | phys_unsep | 0.01476 | 0.04961 | 1.0354 | 0.862 |
| B1_hst_stars | 2 | phys | 0.00081 | 0.06454 | 1.0395 | 0.954 |
| B1_hst_stars | 2 | interp_spline | 0.00853 | 0.06336 | 1.0409 | 0.856 |
| B1_hst_stars | 2 | frame_scalar | 2.75e-05 | 0.06480 | 1.0405 | 0.957 |
| B1_hst_stars | 2 | naive_pixel | 0.01201 | 0.00381 | 1.0371 | 0.903 |
| B1_hst_stars | 2 | phys_unsep | 0.01476 | 0.04961 | 1.0356 | 0.862 |
| B2_hst_varsky | 0 | phys | 0.00115 | 0.07904 | 1.0547 | 0.939 |
| B2_hst_varsky | 0 | interp_spline | 0.00880 | 0.07467 | 1.0510 | 0.854 |
| B2_hst_varsky | 0 | frame_scalar | 2.00e-05 | 0.07927 | 1.0510 | 0.948 |
| B2_hst_varsky | 0 | naive_pixel | 0.01369 | 0.00446 | 1.0519 | 0.885 |
| B2_hst_varsky | 0 | phys_unsep | 0.01787 | 0.05806 | 1.0462 | 0.837 |
| B2_hst_varsky | 1 | phys | 0.00078 | 0.06455 | 1.0473 | 0.947 |
| B2_hst_varsky | 1 | interp_spline | 0.00840 | 0.06250 | 1.0422 | 0.857 |
| B2_hst_varsky | 1 | frame_scalar | 2.75e-05 | 0.06480 | 1.0428 | 0.955 |
| B2_hst_varsky | 1 | naive_pixel | 0.01171 | 0.00368 | 1.0450 | 0.897 |
| B2_hst_varsky | 1 | phys_unsep | 0.01476 | 0.04961 | 1.0354 | 0.862 |
| B2_hst_varsky | 2 | phys | 0.00059 | 0.05444 | 1.0345 | 0.960 |
| B2_hst_varsky | 2 | interp_spline | 0.00754 | 0.05357 | 1.0343 | 0.868 |
| B2_hst_varsky | 2 | frame_scalar | 3.34e-05 | 0.05464 | 1.0349 | 0.961 |
| B2_hst_varsky | 2 | naive_pixel | 0.01072 | 0.00323 | 1.0325 | 0.912 |
| B2_hst_varsky | 2 | phys_unsep | 0.01251 | 0.04331 | 1.0313 | 0.879 |
| B2_hst_varsky | 3 | phys | 0.00045 | 0.04691 | 1.0285 | 0.966 |
| B2_hst_varsky | 3 | interp_spline | 0.00730 | 0.04786 | 1.0308 | 0.872 |
| B2_hst_varsky | 3 | frame_scalar | 3.81e-05 | 0.04716 | 1.0307 | 0.965 |
| B2_hst_varsky | 3 | naive_pixel | 0.00998 | 0.00284 | 1.0267 | 0.921 |
| B2_hst_varsky | 3 | phys_unsep | 0.01089 | 0.03844 | 1.0282 | 0.892 |
| B3_hst_lowsky | 0 | phys | 0.00355 | 0.13971 | 1.0913 | 0.903 |
| B3_hst_lowsky | 0 | interp_spline | 0.01495 | 0.12801 | 1.0887 | 0.808 |
| B3_hst_lowsky | 0 | frame_scalar | 1.30e-06 | 0.13967 | 1.0897 | 0.917 |
| B3_hst_lowsky | 0 | naive_pixel | 0.02249 | 0.00766 | 1.0883 | 0.835 |
| B3_hst_lowsky | 0 | phys_unsep | 0.03284 | 0.08894 | 1.0786 | 0.754 |
| B3_hst_lowsky | 1 | phys | 0.00367 | 0.13965 | 1.0830 | 0.909 |
| B3_hst_lowsky | 1 | interp_spline | 0.01530 | 0.12736 | 1.0885 | 0.803 |
| B3_hst_lowsky | 1 | frame_scalar | 1.30e-06 | 0.13967 | 1.0884 | 0.918 |
| B3_hst_lowsky | 1 | naive_pixel | 0.02300 | 0.00792 | 1.0798 | 0.841 |
| B3_hst_lowsky | 1 | phys_unsep | 0.03294 | 0.08895 | 1.0779 | 0.755 |
| B3_hst_lowsky | 2 | phys | 0.00359 | 0.13971 | 1.0883 | 0.905 |
| B3_hst_lowsky | 2 | interp_spline | 0.01425 | 0.12852 | 1.0882 | 0.806 |
| B3_hst_lowsky | 2 | frame_scalar | 1.30e-06 | 0.13967 | 1.0876 | 0.919 |
| B3_hst_lowsky | 2 | naive_pixel | 0.02270 | 0.00777 | 1.0847 | 0.838 |
| B3_hst_lowsky | 2 | phys_unsep | 0.03271 | 0.08893 | 1.0808 | 0.753 |
| B4_hst_highsky | 0 | phys | 4.12e-05 | 0.00988 | 1.0183 | 0.978 |
| B4_hst_highsky | 0 | interp_spline | 0.00418 | 0.01270 | 1.0197 | 0.878 |
| B4_hst_highsky | 0 | frame_scalar | 6.73e-05 | 0.00999 | 1.0192 | 0.975 |
| B4_hst_highsky | 0 | naive_pixel | 0.01545 | 0.00070 | 1.0182 | 0.779 |
| B4_hst_highsky | 0 | phys_unsep | 0.00214 | 0.00982 | 1.0222 | 0.934 |
| B4_hst_highsky | 1 | phys | 4.02e-05 | 0.00988 | 1.0281 | 0.969 |
| B4_hst_highsky | 1 | interp_spline | 0.00404 | 0.01284 | 1.0194 | 0.881 |
| B4_hst_highsky | 1 | frame_scalar | 6.73e-05 | 0.00999 | 1.0189 | 0.976 |
| B4_hst_highsky | 1 | naive_pixel | 0.01038 | 0.00084 | 1.0281 | 0.775 |
| B4_hst_highsky | 1 | phys_unsep | 0.00214 | 0.00982 | 1.0235 | 0.933 |
| B4_hst_highsky | 2 | phys | 4.17e-05 | 0.00988 | 1.0189 | 0.978 |
| B4_hst_highsky | 2 | interp_spline | 0.00396 | 0.01272 | 1.0187 | 0.878 |
| B4_hst_highsky | 2 | frame_scalar | 6.73e-05 | 0.00999 | 1.0180 | 0.976 |
| B4_hst_highsky | 2 | naive_pixel | 0.01471 | 0.00071 | 1.0189 | 0.779 |
| B4_hst_highsky | 2 | phys_unsep | 0.00216 | 0.00984 | 1.0182 | 0.938 |
| B5_hst_bright | 0 | phys | 0.03145 | 0.40522 | 2.1159 | 0.455 |
| B5_hst_bright | 0 | interp_spline | 0.67867 | 0.41408 | 2.0514 | 0.225 |
| B5_hst_bright | 0 | frame_scalar | 2.75e-05 | 0.39709 | 2.0616 | 0.483 |
| B5_hst_bright | 0 | naive_pixel | 0.05338 | 0.04735 | 2.1206 | 0.427 |
| B5_hst_bright | 0 | phys_unsep | 0.05441 | 0.21222 | 2.1009 | 0.348 |
| B5_hst_bright | 1 | phys | 0.03095 | 0.40500 | 2.1218 | 0.454 |
| B5_hst_bright | 1 | interp_spline | 0.67087 | 0.40911 | 2.0603 | 0.224 |
| B5_hst_bright | 1 | frame_scalar | 2.75e-05 | 0.39709 | 2.0686 | 0.481 |
| B5_hst_bright | 1 | naive_pixel | 0.05276 | 0.04786 | 2.1264 | 0.426 |
| B5_hst_bright | 1 | phys_unsep | 0.05371 | 0.21276 | 2.1074 | 0.348 |
| B5_hst_bright | 2 | phys | 0.03139 | 0.40516 | 2.1173 | 0.455 |
| B5_hst_bright | 2 | interp_spline | 0.66430 | 0.40869 | 2.0549 | 0.224 |
| B5_hst_bright | 2 | frame_scalar | 2.75e-05 | 0.39709 | 2.0648 | 0.482 |
| B5_hst_bright | 2 | naive_pixel | 0.05327 | 0.04744 | 2.1218 | 0.427 |
| B5_hst_bright | 2 | phys_unsep | 0.05457 | 0.21208 | 2.0997 | 0.348 |

### B-HST：跨帧一致性（控制点 sigma 比 / 真值比，中位）

| 场景 | 比较 | median ratio | p95 绝对偏差 | n |
|---|---|---|---|---|
| B1_hst_stars | R1_ctrl_frame1_over_0 | 1.0003 | 0.0384 | 256 |
| B1_hst_stars | truth_ctrl_frame1_over_0 | 1.0000 | 0.0000 | 256 |
| B1_hst_stars | R1_ctrl_frame2_over_0 | 1.0007 | 0.0360 | 256 |
| B1_hst_stars | truth_ctrl_frame2_over_0 | 1.0000 | 0.0000 | 256 |
| B2_hst_varsky | R1_ctrl_frame1_over_0 | 1.0041 | 0.0412 | 256 |
| B2_hst_varsky | truth_ctrl_frame1_over_0 | 1.0000 | 0.0000 | 256 |
| B2_hst_varsky | R1_ctrl_frame2_over_0 | 1.0051 | 0.0630 | 256 |
| B2_hst_varsky | truth_ctrl_frame2_over_0 | 1.0000 | 0.0000 | 256 |
| B2_hst_varsky | R1_ctrl_frame3_over_0 | 1.0070 | 0.0672 | 256 |
| B2_hst_varsky | truth_ctrl_frame3_over_0 | 1.0000 | 0.0000 | 256 |
| B3_hst_lowsky | R1_ctrl_frame1_over_0 | 0.9984 | 0.0415 | 256 |
| B3_hst_lowsky | truth_ctrl_frame1_over_0 | 1.0000 | 0.0000 | 256 |
| B3_hst_lowsky | R1_ctrl_frame2_over_0 | 0.9990 | 0.0346 | 256 |
| B3_hst_lowsky | truth_ctrl_frame2_over_0 | 1.0000 | 0.0000 | 256 |
| B4_hst_highsky | R1_ctrl_frame1_over_0 | 1.0002 | 0.0360 | 256 |
| B4_hst_highsky | truth_ctrl_frame1_over_0 | 1.0000 | 0.0000 | 256 |
| B4_hst_highsky | R1_ctrl_frame2_over_0 | 0.9996 | 0.0327 | 256 |
| B4_hst_highsky | truth_ctrl_frame2_over_0 | 1.0000 | 0.0000 | 256 |
| B5_hst_bright | R1_ctrl_frame1_over_0 | 1.0045 | 0.0394 | 256 |
| B5_hst_bright | truth_ctrl_frame1_over_0 | 1.0000 | 0.0000 | 256 |
| B5_hst_bright | R1_ctrl_frame2_over_0 | 1.0027 | 0.0367 | 256 |
| B5_hst_bright | truth_ctrl_frame2_over_0 | 1.0000 | 0.0000 | 256 |

## C testdata 真实数据（臂 C）

### C-真实数据：一致性检验（无真值）

| 帧 | 自由斜率 c | 固定斜率 a | lever_var | 模型/奇族 hold-out | Pearson(log10) | R1/奇族 hold-out | 模型/二阶差分 |
|---|---|---|---|---|---|---|---|
| M42_M1 | 1.2749 | 191.6 | 0.056 | 1.0023 | 0.486 | 1.0014 | 1.0386 |
| M42_M2 | 1.1273 | 220.4 | 0.403 | 0.9993 | 0.864 | 1.0006 | 1.0508 |
| M42_M4 | 0.7341 | 176.0 | 0.091 | 0.9952 | 0.638 | 0.9988 | 1.0238 |

### C-真实数据：帧级标量 vs 区域 sigma（EXP-03 机理复现）

| 帧 | 帧级标量 | 区域 p05 | 区域 p95 | 区域离散度 | 模型场离散度 | 帧级 vs 区域最大偏差 |
|---|---|---|---|---|---|---|
| M42_M1 | 13.792 | 13.421 | 14.984 | 1.116 | 1.056 | 0.504 |
| M42_M2 | 14.782 | 13.591 | 19.600 | 1.442 | 1.350 | 0.751 |
| M42_M4 | 13.318 | 12.898 | 14.573 | 1.130 | 1.052 | 1.448 |

## 门与故障注入

### 门与故障注入（ALL_PASS = True）

| 判定 | 门 | 预期 | 实测 |
|---|---|---|---|
| PASS | G1a_flat_truth_physics_field_is_constant | dispersion <= 1e-2 | 平坦天光 + 无源（真值无空间效应）⇒ 物理重建场必须退化为常数；实测离散度 0.00322 |
| PASS | G1b_flat_truth_physics_flatter_than_interpolation | disp <= disp_interp/5 | 同一平坦真值场上，物理重建的伪空间结构必须比纯插值至少小 5 倍；实测 phys=0.00322 interp=0.0393 frame=0（比 12.2） |
| PASS | G2_zero_noise_truth_must_flag_degenerate | degenerate=True | 零噪声真值：真值方差恒 0 ⇒ 度量必须显式判退化（不得静默产出数字）；实测 degenerate=True n_eval=0 |
| PASS | G3a_separation_blocks_bright_pixels | dispersion <= 1e-2 | 平坦天光 + 6 颗极亮星（flux 1e6.5~1e7 e-）⇒ 结构分离后物理场仍须近似常数；实测离散度 0.00363 |
| PASS | G3b_naive_pixel_substitution_must_be_red | src_snr_p05 <= 0.5（错误臂必须红） | 逐像素代入亮度：源像素上 sigma 被高估 ⇒ SNR 伪暗洞必须判红；实测源区 SNR 比值 p05 = 0.066 |
| PASS | G4a_naive_pixel_source_snr_dip_must_be_red | p05 <= 0.5（错误臂必须红） | 逐像素代入亮度：源像素 SNR 比值的 p05 必须显著低于 1（伪暗洞）；实测 0.223 |
| PASS | G4b_physics_source_zone_clean | p05 >= 0.9 | 物理重建在源像素上不得产生 SNR 伪结构；实测源区 SNR p05 = 1.002 |
| PASS | G5_no_separation_must_be_red | E_unsep >= 2*E_phys | 不做结构分离（大尺度高斯平滑原始帧作驱动量）⇒ 权重效率损失必须显著变差；实测 E_unsep=0.05169 vs E_phys=0.00006（比 938.78） |
| PASS | G6a_shuffled_controls_destroy_the_brightness_law | E_shuffled >= 10*E_phys | 控制点值随机洗牌（破坏与亮度的配对）⇒ 物理模型的增益信息被破坏，必须显著劣于物理臂；实测 E_shuffled=0.05544 vs E_phys=0.00006（比 1006.9）；洗牌后斜率 -0.0109 vs 正确 0.7454 |
| PASS | G6b_shuffled_model_degenerates_to_frame_scalar | |E_shuffled/E_frame - 1| <= 0.5 | 洗牌后模型退化为帧级标量（亮度项归零）⇒ E 必须与帧级臂同量级；实测 E_shuffled=0.05544 vs E_frame=0.05399 |
| PASS | G7_gain_recovery_on_clean_backgrounds | |gain_hat/g - 1| <= 5% | 弥散分量在 cell 尺度上足够平滑的场景（线性/强梯度）⇒ 自由斜率必须还原 1/g，偏差 <= 5%；实测最差 3.66% |
| PASS | G8_power_law_exponent_consistent_with_1 | p 与 1 在 3 sigma 内一致 | 物理模型预言 Var 对电平的幂律指数 p = 1；自由指数拟合必须与 1 在 3 sigma 内一致 |
| PASS | G9_physics_beats_interpolation_on_smooth_backgrounds | E_phys < E_interp（全部场景） | 平滑背景（弥散分量在 cell 尺度可分辨）上，物理建模的权重效率损失必须优于纯插值 |
| PASS | G10_constant_driver_injection_must_be_red | E_const >= 2*E_phys | 把驱动量置为常数（等价于丢掉亮度信息）⇒ 必须显著劣于物理重建；实测 E_const=0.49237 vs E_phys=0.00028 |
| PASS | G11_wrong_gain_injection_must_be_red | E_wrong >= 2*E_phys | 把帧级增益用错一倍（斜率 1/(2g)）⇒ 必须显著劣于正确增益；实测 E_wrong=0.13190 vs E_phys=0.00028 |
| PASS | G12a_caliberP_weight_efficiency_loss_detected | E_wP >= 10*E_ivar（口径错配必须判红） | 把逐像素显著性平方（w = ((I-D)/sigma)^2）当作权重：必须显著劣于逆方差权重；实测 E_wP=3347303.22081 vs E_ivar=0.05640（比 59348386.3） |
| PASS | G12b_caliberP_field_is_not_smooth | >= 3（该口径下场不平滑） | 逐像素显著性场的空间动态范围 p99.9/p50（星点处爆表 ⇒ 场不平滑）；实测 16360.7 |
| PASS | G13a_flat_background_auto_is_harmless_and_constant | E_auto <= 1e-4 且场近似常数 | 背景恒定时斜率不可辨识，但驱动量恒定 ⇒ 模型场与斜率无关，auto 必须仍退化为常数且 E 触底；实测 E_auto=4.13e-08，场相对标准差 0.000102；CV 误差 free=0.011408 vs zero=0.011409（几乎并列） |
| PASS | G13b_cv_accepts_slope_on_strong_gradient | choose_free = True | 强梯度（lever_var=1.641）时交叉验证必须接受自由斜率 |
| PASS | G13c_auto_never_loses_to_either_candidate | E_auto <= 1.1*min(E_frame, E_free) | phys_auto（CV 选斜率）在任何场景都不得显著劣于 frame 与 phys_free 中的较优者（绝对下限 1e-4：低于此量级的差异无工程意义） |
| PASS | G15a_free_slope_model_is_positive_homogeneous | <= 1e-9 | 自由斜率模型对控制值整体缩放严格正齐次 R[a*v]=a*R[v]（冻结算子要求 ②）；实测最大相对偏差 6.66e-16 |
| PASS | G15b_fixed_gain_model_breaks_homogeneity | >= 0.05（必须识别出该差异） | 用帧级增益固定斜率时模型**不正齐次**（斜率项不随控制值缩放）；实测最大相对偏差 0.213 |

## 作用域图谱（物理建模 vs 插值）

### Q1 真值 sigma^2 场的空间方差被各方法解释的比例（R2_var，T1 = var_bg（空背景口径））

| 场景 | phys | interp_spline | phys_resid | frame_scalar | E_phys | E_interp |
|---|---|---|---|---|---|---|
| gradient_weak | 0.8439 | 0.7436 | 0.7421 | -0.1380 | 6.39e-06 | 0.00045 |
| gradient_strong | 0.9992 | 0.9952 | 0.9951 | -0.0246 | 0.00028 | 0.00176 |
| nebula_smooth | 0.9943 | 0.9820 | 0.9823 | -0.2184 | 5.83e-05 | 0.00101 |
| read_noise_dominated | -4.38e+26 | -8.63e+26 | -8.56e+26 | -4.33e+26 | 4.11e-06 | 0.00049 |

### Q2 弥散分量尺度扫描（T1 = var_bg（空背景口径），E 中位）

| ell_B [px] | ell/Delta | E_phys | E_interp | E_phys_resid | E_frame | E_dense |
|---|---|---|---|---|---|---|
| 20 | 0.31 | 0.00060 | 0.00555 | 0.00585 | 0.02009 | 0.00571 |
| 40 | 0.62 | 0.00193 | 0.01779 | 0.02027 | 0.02301 | 0.01344 |
| 80 | 1.25 | 1.73e-05 | 0.00691 | 0.00705 | 0.03368 | 0.00784 |
| 160 | 2.50 | 2.10e-05 | 0.00060 | 0.00060 | 0.05979 | 0.00144 |
| 320 | 5.00 | 6.49e-05 | 0.00067 | 0.00067 | 0.04268 | 0.00138 |

### Q3 亮度解释不了的噪声结构（T1 = var_bg（空背景口径））

| 场景 | E_phys | E_interp | E_phys_resid | E_frame | R2var phys | R2var interp | R2var phys_resid |
|---|---|---|---|---|---|---|---|
| flat_sky_two_amp | 0.34906 | 0.00613 | 0.00689 | 0.34898 | -0.0030 | 0.9744 | 0.9755 |
| flat_sky_two_amp_stars | 0.34875 | 0.00631 | 0.00690 | 0.34898 | -0.0011 | 0.9740 | 0.9754 |
| prnu_only | 0.00014 | 0.00108 | 0.00109 | 0.00014 | -0.7330 | -8.1395 | -8.1894 |

### Q1 真值 sigma^2 场的空间方差被各方法解释的比例（R2_var，T2 = var_local（逐像素总方差口径））

| 场景 | phys | interp_spline | phys_resid | frame_scalar | E_phys | E_interp |
|---|---|---|---|---|---|---|
| gradient_weak | 0.8439 | 0.7436 | 0.7421 | -0.1380 | 6.39e-06 | 0.00045 |
| gradient_strong | 0.1372 | 0.1370 | 0.1370 | -0.0066 | 0.08100 | 0.07546 |
| nebula_smooth | 0.0569 | 0.0573 | 0.0573 | -0.0192 | 0.02928 | 0.02852 |
| read_noise_dominated | -0.0013 | -0.0009 | -0.0009 | -0.0014 | 0.04319 | 0.04220 |

### Q2 弥散分量尺度扫描（T2 = var_local（逐像素总方差口径），E 中位）

| ell_B [px] | ell/Delta | E_phys | E_interp | E_phys_resid | E_frame | E_dense |
|---|---|---|---|---|---|---|
| 20 | 0.31 | 0.04199 | 0.04557 | 0.04604 | 0.06089 | 0.04450 |
| 40 | 0.62 | 0.04321 | 0.05775 | 0.06110 | 0.06355 | 0.05228 |
| 80 | 1.25 | 0.04058 | 0.04639 | 0.04649 | 0.07326 | 0.04587 |
| 160 | 2.50 | 0.03781 | 0.03681 | 0.03681 | 0.09553 | 0.03685 |
| 320 | 5.00 | 0.02636 | 0.02560 | 0.02559 | 0.06864 | 0.02611 |

### Q3 亮度解释不了的噪声结构（T2 = var_local（逐像素总方差口径））

| 场景 | E_phys | E_interp | E_phys_resid | E_frame | R2var phys | R2var interp | R2var phys_resid |
|---|---|---|---|---|---|---|---|
| flat_sky_two_amp | 0.34906 | 0.00613 | 0.00689 | 0.34898 | -0.0030 | 0.9744 | 0.9755 |
| flat_sky_two_amp_stars | 0.38722 | 0.05672 | 0.05758 | 0.38750 | -0.0015 | 0.1279 | 0.1281 |
| prnu_only | 0.04299 | 0.03977 | 0.03976 | 0.04314 | -0.0003 | 0.0003 | 0.0004 |
