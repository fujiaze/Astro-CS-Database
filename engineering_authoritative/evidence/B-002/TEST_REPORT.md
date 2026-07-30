# B-002 科学统计测试报告 — 代表帧 Stage1

- 报告日期: 2026-07-30
- 数据来源: orchestrator stage1 运行日志 + `orchestrator inspect --hiss` 元数据
- 原始日志: engineering_authoritative/evidence/B-002/stage1_{T2,T3,T4}_Red.log
- 汇总表: engineering_authoritative/evidence/B-002/science_stats.csv

## 1. 测试范围

对 B-001 选定的 3 个代表帧执行完整 Stage1 流水线（read_fits → calibrate → platesolve → psf → photometric → snr → drizzle），产出 .hiss 并采集科学统计指标。三帧覆盖三种典型工况：长焦窄场长曝光(T2)、长焦窄场中曝光(T3)、广角宽场短曝光(T4)。

## 2. 科学统计摘要

### 2.1 PlateSolve (天定标)

| 帧 | RMS (arcsec) | RMS (px) | n_pairs | n_detected | trans_order | cd_Δ (%) | CRVAL (ra,dec) |
|---|---|---|---|---|---|---|---|
| T2 | 0.1170 | 0.120 | 34 | 2000 | 3 | 0.289 | 248.610, -15.759 |
| T3 | 0.1387 | 0.142 | 39 | 996 | 3 | 0.489 | 3.750, -39.194 |
| T4 | 0.3460 | 0.056 | 36 | 2000 | 3 | 6.895 | 272.826, -13.132 |

判定：T2/T3 窄场 RMS < 0.15" 优；T4 宽场 RMS 0.346"（像素级 0.056px 远亚像素），cd_Δ 偏高系 9.9° FOV 大畸变所致，robust_refine 在 iter3 触发最小匹配数回退，最终结果可接受。三帧 SIP order=3，WCS 写入成功。

### 2.2 PSF (点扩散函数)

PSF FWHM 中位数取自 StarDetector (SDET) 阶段的 "FWHM stats: med=..." 汇总（单位 px），arcsec = med_px × pixel_scale。

| 帧 | FWHM med (px) | FWHM mad (px) | FWHM med (arcsec) | 拟合成功/总数 | 成功率 | fit_radius(auto) |
|---|---|---|---|---|---|---|
| T2 | 3.1989 | 0.3248 | 3.09 | 1949/2000 | 97.45% | 11 |
| T3 | 2.4964 | 0.3223 | 2.39 | 981/996 | 98.49% | 11 |
| T4 | 1.4508 | 0.2379 | 9.15 | 1984/2000 | 99.20% | 11 |

判定：三帧 PSF 拟合成功率均 ≥97%；T4 单星 FWHM 小（1.45px）但像素尺度大（6.31"/px），对应 9.15" 视宁度/光学综合，与 Nikkor 200mm F2 宽场实际一致。被剔除的单星（FWHM 越界/背景越界）以 WARN 形式记录于日志，不影响统计。

### 2.3 Photometric (测光校准)

| 帧 | fit_used | sigma_residual | scale_factor | gaia_in_frame | match_dist median(px) | p90 | max | robust_iter | status |
|---|---|---|---|---|---|---|---|---|---|
| T2 | 1095 | 0.066314 | 8.442e-06 | 1210 | 0.473 | 0.821 | 1.915 | 8 | OK |
| T3 | 285 | 0.128846 | 1.015e-05 | 311 | 0.539 | 0.948 | 1.642 | 7 | OK |
| T4 | 1670 | 0.182142 | 1.565e-03 | 6021 | 0.367 | 0.650 | 1.942 | 7 | OK |

判定：三帧测光状态 OK。T2 银河面附近星密度高（gaia_in_frame=1210, fit_used=1095），sigma_residual 最低 0.066（最优）；T3 南天星场稀疏（fit_used 仅 285）；T4 宽场匹配量最大（1670）但宽场畸变使 sigma 略高 0.182。所有 match_dist max < 2.0px 阈值，IRLS 清洗正常收敛。

### 2.4 SNR (信噪比模型)

| 帧 | n_points | snr_phot | median_snr | idw_power | snr_format | has_snr |
|---|---|---|---|---|---|---|
| T2 | 1930 | 6.549 | 83.02 | 2.0 | 1 | true |
| T3 | 617 | 3.371 | 86.59 | 2.0 | 1 | true |
| T4 | 1984 | 2.384 | 378.62 | 2.0 | 1 | true |

判定：三帧 SNR 模型均写入 HISS（has_snr=true, format=1 稀疏控制点+IDW）。T4 median_snr=378.6 最高（银河中心 180s 短曝光但恒星极密+亮），T3 fit_used 少导致 n_points 仅 617。snr_phot 越大表示测光归一化后模型残差越大。

### 2.5 Drizzle (HEALPix 投影)

| 帧 | nside | nested | n_healpix (support) | n_source (signal) | pixfrac | drizzle elapsed(s) | HISS size(B) |
|---|---|---|---|---|---|---|---|
| T2 | 2048 | true | 1573 | 16777216 | 1.0 | 14.53 | 58076 |
| T3 | 2048 | true | 1535 | 16777216 | 1.0 | 13.34 | 31352 |
| T4 | 512 | true | 3928 | 16200000 | 1.0 | 12.64 | 87433 |

- signal = n_source_pixels（输入图像参与投影的像素数）；support = n_healpix_pixels（命中输出 HEALPix 像素数）。
- nside 由 `1x_to_2x_drizzle` 自适应：T2/T3 像素尺度 ~0.96"/px → nside=2048；T4 像素尺度 6.31"/px → nside=512。
- T4 support(3928) 最高系宽场覆盖天区大；HISS 文件大小取决于 support + SNR 控制点数。

判定：三帧 Drizzle 全部成功，HISS magic 校验通过（"HISS"），WCS+SIP(order=3) 一致性已由 SNR 阶段 "SIP 前向系数加载 A_ORDER=3 B_ORDER=3" 与 drizzle "SIP A_ORDER=3 B_ORDER=3 AP_ORDER=3 BP_ORDER=3" 交叉确认。

## 3. HISS 完整性校验 (inspect --hiss)

对三个 .hiss 运行 `orchestrator inspect --hiss`，全部返回 status=ok：

| 帧 | magic | nside | nested | n_pix | has_snr | snr_format | snr_n_points | uncomp_json_len | comp_json_len |
|---|---|---|---|---|---|---|---|---|---|
| T2 | HISS | 2048 | true | 1573 | true | 1 | 1930 | 808 | 560 |
| T3 | HISS | 2048 | true | 1535 | true | 1 | 617 | 803 | 552 |
| T4 | HISS | 512 | true | 3928 | true | 1 | 1984 | 830 | 577 |

inspect meta_json 与 stage1 日志中的 nside/n_pix/snr_n_points 完全一致，证明 HISS 序列化-反序列化闭环正确，无数据丢失。

## 4. 结论

- 3/3 代表帧 Stage1 全链路成功，产出 3 个合规 .hiss 文件。
- PlateSolve / PSF / Photometric / SNR / Drizzle 五项科学指标全部由流水线实际计算产出（非文件写出），数值落入各自工况的合理区间。
- 唯一阻塞问题为 T2 中文路径触发的 std::filesystem 编码崩溃，已用 ASCII 路径副本规避；建议后续在 orchestrator 层对路径统一做宽字符处理（根因修复留作后续任务）。
- 全部交付物见 TASK_REPORT.md 第 5 节。
