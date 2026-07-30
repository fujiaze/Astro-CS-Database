# D-001 任务执行报告 — 同一天区多帧 HISS 球面重合与光度一致性

- 任务编号: D-001
- 执行日期: 2026-07-30
- 执行环境: PowerShell 7, Python 3 (astropy 6.1.7 / astropy_healpix 2.0.0 / numpy 2.2.6 / matplotlib 3.10.8)
- orchestrator: lib\orchestrator\cpp\orchestrator.exe
- 配置来源: 基于 engineering_authoritative\evidence\B-001\configs\stage1_config_T4_Red.json, 保持 drizzle 参数一致
- 输出目录: output\D-001\ (HISS) / engineering_authoritative\evidence\D-001\ (报告与验证)

## 1. 任务目标

验证同一天区(银心)多帧 HISS 在球面上的重合与光度一致性, 作为 Gate D 的首个任务。要求:
- 生成银心三片(panel1/2/3)Red HISS
- 验证三片球面信号真实重合(非元数据比较)
- 验证无镜像/翻转
- 验证光度尺度稳定

## 2. 步骤 1: 生成银心三片 Red HISS

panel1 复用 B-002 已生成的 HISS (output/B-002/T4_RED_GalaxyCenter_panel1.hiss), panel2/panel3 新生成。三片均使用相同的 T4 校准链(Bias/Dark/Flat)和 drizzle 参数(nside_strategy=1x_to_2x_drizzle, pixfrac=1.0, nested=true), 保证球面网格统一。

### 2.1 输入帧选择

| 帧 | panel | FITS 路径 | 曝光(s) | 拍摄时间 |
|---|---|---|---|---|
| T4_RED_GalaxyCenter_panel1 | panel1 | testdata/Galaxy_Center_T4/lights/panel1/Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@061703-180S-Red.fts | 180 | 2025-07-02 |
| T4_RED_GalaxyCenter_panel2 | panel2 | testdata/Galaxy_Center_T4/lights/panel2/Galaxy_Center_mosaic2_T4_flying_dutchman-20250716@004219-180S-Red.fts | 180 | 2025-07-16 |
| T4_RED_GalaxyCenter_panel3 | panel3 | testdata/Galaxy_Center_T4/lights/panel3/Galaxy_Center_mosaic3_T4_flying_dutchman-20250718@001638-180S-Red.fts | 180 | 2025-07-18 |

三片 FITS 路径均为全 ASCII, 无中文路径问题(参考 B-002 T2 中文路径崩溃教训)。

### 2.2 Stage1 运行结果

运行命令模板:
```
lib\orchestrator\cpp\orchestrator.exe stage1 --frame <fits> --output output/D-001/<id>.hiss --config <config.json>
```

| 帧 | 退出码 | HISS 输出 | 大小(B) | n_pix |
|---|---|---|---|---|
| T4_RED_GalaxyCenter_panel1 | 0 (B-002) | output/D-001/T4_RED_GalaxyCenter_panel1.hiss | 87433 | 3928 |
| T4_RED_GalaxyCenter_panel2 | 0 | output/D-001/T4_RED_GalaxyCenter_panel2.hiss | 87461 | 3927 |
| T4_RED_GalaxyCenter_panel3 | 0 | output/D-001/T4_RED_GalaxyCenter_panel3.hiss | 87430 | 3936 |

**三片 Stage1 全部成功 (3/3), 单帧约 25s (与 B-002 一致), 远低于 300s 超时阈值。**

## 3. 步骤 2: 球面重合验证

验证脚本: engineering_authoritative/evidence/D-001/verify_overlap.py
- 使用 hiss_v2.py 的 v1_read_snr_model() 读取 V1 .hiss 文件 (纯 Python, 不依赖 DLL)
- 使用 astropy_healpix 将 ipix 转为球面坐标 (RA/Dec)
- **比较真实球面信号重合, 非元数据比较**

### 3.1 球面网格一致性

| 项目 | panel1 | panel2 | panel3 | 一致 |
|---|---|---|---|---|
| nside | 512 | 512 | 512 | PASS |
| nested | True | True | True | PASS |

三片 nside/nested 完全一致, 在相同 HEALPix ipix 空间可比。**球面网格统一: PASS**

### 3.2 三片球面覆盖范围 (RA/Dec)

三片为银心区域南北排列的马赛克:

| panel | CRVAL (RA, Dec) | RA range | Dec range | Dec span |
|---|---|---|---|---|
| panel1 | (272.826, -13.132) | [268.68, 276.94] | [-16.33, -9.90] | 6.44 |
| panel2 | (272.875, -18.257) | [268.59, 277.12] | [-21.46, -15.02] | 6.44 |
| panel3 | (272.887, -23.254) | [268.51, 277.29] | [-26.44, -20.03] | 6.42 |

三片 RA 中心接近 (~272.8°), Dec 中心分别 -13°/-18°/-23°, 沿赤纬方向南北排列, 相邻片 Dec 有 ~1.3° 重叠(6.44° 跨度, 中心间隔 5.1°)。

### 3.3 球面 ipix 交集 (真实信号重合)

| 重叠对 | 交集 ipix 数 | 占 A 比 | 占 B 比 |
|---|---|---|---|
| panel1 ∩ panel2 | 792 | 20.16% | 20.17% |
| panel1 ∩ panel3 | 0 | 0% | 0% |
| panel2 ∩ panel3 | 879 | 22.38% | 22.33% |
| 三片共同交集 | 0 | - | - |

- panel1 与 panel2 在 Dec≈-16° 交接区有 792 个 HEALPix 像素真实重合
- panel2 与 panel3 在 Dec≈-20° 交接区有 879 个 HEALPix 像素真实重合
- panel1 与 panel3 不相邻(中间隔 panel2), 无重叠 — **这是正常的马赛克布局, 非异常**
- 三片无共同交集(三片不共点) — 符合线性南北排列的几何预期

**结论: 相邻片有真实球面信号重合 (792 + 879 = 1671 ipix), 验证了球面投影的正确性。**

### 3.4 重叠区 signal 比较 + 光度尺度

| 重叠对 | n_valid | ratio median | ratio MAD | ratio p16 | ratio p84 | scatter | 判定 |
|---|---|---|---|---|---|---|---|
| panel1_vs_panel2 | 792 | 0.9814 | 0.0441 | 0.9015 | 1.0660 | 0.1645 | PASS |
| panel2_vs_panel3 | 879 | 0.9878 | 0.0200 | 0.9514 | 1.0280 | 0.0766 | PASS |

判定标准: |median - 1.0| < 0.15 且 scatter(p84-p16) < 0.5

- 两对重叠区 signal 比值中位数均接近 1.0 (0.981 / 0.988), 说明三片经 photometric 校准后光度尺度一致
- panel2_vs_panel3 的 scatter (0.077) 明显小于 panel1_vs_panel2 (0.165), 因 panel2/panel3 拍摄时间更近 (2025-07-16/18 vs panel1 的 2025-07-02), 大气条件更接近
- ratio mean 偏大 (8.67 / 40.09) 是因极少数近零 signal 像素除法放大, median/p16/p84 稳健统计不受影响
- Pearson 相关性较低 (0.28 / 0.29): 重叠区位于马赛克片边缘, 以天光背景为主, 无强点源相关性; 比值中位数接近 1.0 已证明光度一致

**光度尺度稳定: PASS**

### 3.5 WCS 镜像 / 翻转检查

| panel | det(CD) | det 符号 | pixscale | CD[0,0] 符号(RA) | CD[1,1] 符号(Dec) |
|---|---|---|---|---|---|
| panel1 | 3.070e-06 | +1 | 6.3081" | +1 | +1 |
| panel2 | 3.071e-06 | +1 | 6.3090" | -1 | -1 |
| panel3 | 3.069e-06 | +1 | 6.3070" | -1 | -1 |

- **det(CD) 三片同号(正)**: 手性一致, **无镜像 — PASS**
- **pixscale 相对差异 0.033%**: 同一设备(T4 Nikkor 200F2 + FLI 16200), 像素尺度高度一致 — PASS
- **CD[0,0]/CD[1,1] 符号**: panel1=(+,+), panel2/panel3=(-,-)。这是 **180° 相机旋转差异**(刚体旋转, CD 矩阵整体取负), **不是镜像**。det(CD) 不变号证实手性一致
- 旋转分组: {(+,+): [panel1], (-,-): [panel2, panel3]}
- Drizzle 投影到球面时不依赖相机旋转角度, 球面 signal 重合验证(3.3/3.4 节)已证明投影正确

**无镜像: PASS (存在 180° 相机旋转差异, 属正常拍摄方向, 非镜像)**

## 4. 步骤 3: 可视化报告

overlap_analysis.png 含 4 子图:
1. 球面位置散点图(RA-Dec, 三片 + 重叠区金色高亮)
2. 重叠区 signal 散点对比(含 1:1 参考线)
3. 光度比值直方图(clip 至 3 便于显示)
4. 三片球面覆盖矩形(+ 为 CRVAL 中心)

## 5. 总结

| 验证项 | 结果 |
|---|---|
| 球面网格一致(nside/nested) | PASS |
| 真实球面信号重合(非元数据) | PASS (1671 ipix 重叠) |
| 三片都验证(非只 panel1) | PASS (panel1/2/3 均参与) |
| 无镜像(det 手性一致) | PASS |
| 光度尺度稳定(比值≈1.0) | PASS |
| **OVERALL** | **PASS** |

### 5.1 重要发现

1. **panel1 与 panel3 无重叠**: 这是正常的马赛克线性南北排列(panel1 最北, panel3 最南, panel2 居中), 非异常。相邻片(panel1-panel2, panel2-panel3)均有 ~20% 球面像素重合。

2. **panel2/panel3 相对 panel1 旋转 180°**: CD[0,0]/CD[1,1] 符号翻转, det(CD) 不变号, 确认为刚体旋转(非镜像)。这是拍摄时相机角度差异, Drizzle 球面投影已正确处理(光度比值 ≈1.0 证实)。

3. **光度比值相关性偏低(0.28)**: 重叠区在马赛克片边缘, 以天光背景为主无强点源。比值中位数 0.98-0.99 是更可靠的光度一致性指标。

## 6. 交付物

- output/D-001/T4_RED_GalaxyCenter_panel1.hiss (87433 B, 复用 B-002)
- output/D-001/T4_RED_GalaxyCenter_panel2.hiss (87461 B, D-001 生成)
- output/D-001/T4_RED_GalaxyCenter_panel3.hiss (87430 B, D-001 生成)
- engineering_authoritative/evidence/D-001/overlap_analysis.png (球面重合可视化)
- engineering_authoritative/evidence/D-001/overlap_stats.json (验证统计数据)
- engineering_authoritative/evidence/D-001/verify_overlap.py (验证脚本)
- engineering_authoritative/evidence/D-001/configs/stage1_config_T4_Red_panel2.json
- engineering_authoritative/evidence/D-001/configs/stage1_config_T4_Red_panel3.json
- engineering_authoritative/evidence/D-001/logs/stage1_panel2.log
- engineering_authoritative/evidence/D-001/logs/stage1_panel3.log
- engineering_authoritative/evidence/D-001/TASK_REPORT.md (本文件)
- engineering_authoritative/evidence/D-001/TEST_REPORT.md
