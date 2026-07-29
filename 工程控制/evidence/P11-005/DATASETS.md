# testdata 数据集结构化清单

> 来源：各数据集目录下 `素材信息与版权约定.txt` / `素材信息.txt` + 实际文件扫描
> 生成时间：2026-07-28
> 用途：P11-005 PlateSolve 全量回归测试的数据基线

## 1. 数据集总览

| # | 数据集目录 | 目标名 | 望远镜 | 焦距 | 相机 | 传感器 | 滤镜组 | 单张曝光 | 帧数 | FOV类别 |
|---|-----------|--------|--------|------|------|--------|--------|----------|------|---------|
| 1 | `Victory_Nebula_T4_Flying_Dutchman` | 胜利星云（蝘蜓座分子云）LRGB | Chilescope T4 (Nikkor 200F2) | 200mm | FLI Proline 16200 | 4500×3600 | Baader LRGB | 180s | 228 | wide |
| 2 | `Galaxy_Center_T4` | 银心3片RGBHO马赛克 | Chilescope T4 (Nikkor 200F2) | 200mm | FLI Microline 16200 | 4500×3600 | Baader RGBHaOIII | RGB 180s / Ha 300s / OIII 600s | 157 | wide |
| 3 | `NGC55_T3_flying_dutchman` | NGC55 南鲸鱼星系 LRGBHO | Chilescope T3 (ASA 500N) | 1900mm | FLI Proline 16803 | 4096×4096 | Astrodon LRGBHO | LRGB 600s / HO 1200s | 79 | medium/narrow |
| 4 | `NGC247_T2_flying_dutchman` | NGC247 与博比奇链 LRGBHO | Chilescope T2 (ASA 500N) | 1900mm | FLI Proline 16803 | 4096×4096 | Astrodon LRGBHO | LRGB 600s / HO 1200s | 68 | medium/narrow |
| 5 | `NGC1727_T2_flying_dutchman` | NGC1727 宇宙烟花秀 RGBHO | Chilescope T2 (ASA 500N) | 1900mm | FLI Proline 16803 | 4096×4096 | Astrodon RGBHO | RGB 600s / Ha 1200s / OIII 1800s | 64 | medium/narrow |
| 6 | `NGC83_cluster_T3_Flying_Dutchman` | NGC83 星系群 LRGB | Chilescope T3 (ASA 500N) | 1900mm | FLI Proline 16803 | 4096×4096 | Astrodon LRGB | 600s | 72 | medium/narrow |
| 7 | `LDN43_T2素材_flying_dutchman` | LDN43 飞天蝙蝠星云 LRGBHa | Chilescope T2 (ASA 500N) | 1900mm | FLI Proline 16803 | 4096×4096 | Astrodon LRGB+3nm Ha | Lum 600s / RGBHa 1200s | 42 | medium |

**testdata 合计：710 帧**（7 个数据集）

## 2. 数据路径结构

### 2.1 帧文件组织方式

| 数据集 | lights 路径模式 | 路径示例 |
|--------|-----------------|---------|
| Victory_Nebula | `testdata/<set>/lights/<frame>.fts` | `testdata/Victory_Nebula_T4_Flying_Dutchman/lights/Victory_Nebula_mosaic1_flying_dutchman-20250207@071753-180S-Blue.fts` |
| Galaxy_Center | `testdata/<set>/lights/panel<N>/<filter>/<frame>.fts` | `testdata/Galaxy_Center_T4/lights/panel1/Red/Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@061703-180S-Red.fts` |
| NGC55 / NGC247 / NGC1727 / NGC83 / LDN43 | `testdata/<set>/lights/<frame>.fts` | `testdata/NGC55_T3_flying_dutchman/lights/NGC55_T3_flying_dutchman-20250703@075400-600S-Lum.fts` |

### 2.2 baseline 790 帧路径映射

baseline `per_frame.json`（`lib/plate_solve/logs/siril_compare/ipv_baseline_790_v430/per_frame.json`）中 `fits_path` 为旧 flat 路径 `testdata/lights/<bn>.fts`，需映射到嵌套路径：

| object（baseline） | 目标目录 | 帧数 | 映射方式 |
|--------------------|---------|------|---------|
| `Victory_Nebula_mosaic1_flying_dutchman` | `Victory_Nebula_T4_Flying_Dutchman` | 114 | `testdata/Victory_Nebula_T4_Flying_Dutchman/lights/<bn>.fts` |
| `Victory_Nebula_mosaic2_flying_dutchman` | `Victory_Nebula_T4_Flying_Dutchman` | 114 | 同上 |
| `Galaxy_Center_mosaic1_T4_flying_dutchman` | `Galaxy_Center_T4` | 53 | `testdata/Galaxy_Center_T4/lights/panel1/<filter>/<bn>.fts` |
| `Galaxy_Center_mosaic2_T4_flying_dutchman` | `Galaxy_Center_T4` | 55 | `testdata/Galaxy_Center_T4/lights/panel2/<filter>/<bn>.fts` |
| `Galaxy_Center_mosaic3_T4_flying_dutchman` | `Galaxy_Center_T4` | 49 | `testdata/Galaxy_Center_T4/lights/panel3/<filter>/<bn>.fts` |
| `NGC55_T3_flying_dutchman` | `NGC55_T3_flying_dutchman` | 79 | `testdata/NGC55_T3_flying_dutchman/lights/<bn>.fts` |
| `NGC247_T2_flying_dutchman` | `NGC247_T2_flying_dutchman` | 68 | `testdata/NGC247_T2_flying_dutchman/lights/<bn>.fts` |
| `LDN43_LRGBH_flying_dutchman` | `LDN43_T2素材_flying_dutchman` | 42 | `testdata/LDN43_T2素材_flying_dutchman/lights/<bn>.fts` |

**testdata 可覆盖 baseline 帧数：574/790**

### 2.3 baseline 中 testdata 未覆盖的目标

| object（baseline） | 帧数 | testdata 状态 |
|--------------------|------|--------------|
| `NGC4945_FD_T3_flying_dutchman` | 48 | testdata 无此目录 |
| `NGC4945_FD_T2_flying_dutchman` | 47 | testdata 无此目录 |
| `NGC7293_T2_HO_flying_dutchman` | 47 | testdata 无此目录 |
| `M20_T2_flying_dutchman` | 36 | testdata 无此目录 |
| `NGC6302_T1_flying_dutchman` | 30 | testdata 无此目录 |
| `NGC6302_T1` | 8 | testdata 无此目录 |

**未覆盖合计：216 帧**（6 个目标）

### 2.4 testdata 中 baseline 未包含的数据集

| 数据集 | 帧数 | baseline 状态 |
|--------|------|--------------|
| `NGC1727_T2_flying_dutchman` | 64 | baseline 790 帧未包含 |
| `NGC83_cluster_T3_Flying_Dutchman` | 72 | baseline 790 帧未包含 |

**testdata 独有合计：136 帧**

## 3. 校准帧

校准帧位于 `testdata/<T> calibration files/` 目录，命名格式：`master<type>_BIN-<bin>_<dims>[_EXPOSURE-<exp>s|_FILTER-<filter>_mono].xisf`

| 校准帧目录 | 适用数据集 | 传感器 | Bias | Dark | Flat |
|-----------|-----------|--------|------|------|------|
| `T2 calibration files` | LDN43 / NGC1727 / NGC247 | 4096×4096 | 1个 | 600s / 1200s / 1800s | Blue / Green / H-alpha / OIII / Red |
| `T3 calibration files` | NGC55 / NGC83 | 4096×4096 | 1个 | 600s / 1200s | Blue / Green / H-alpha / Lum / Oiii / Red |
| `T4 calibration files` | Victory_Nebula / Galaxy_Center | 4500×3600 | 1个 | 180s / 300s / 600s | Blue / Green / H-alpha / Oiii / Red |

## 4. 各数据集详细说明

### 4.1 Victory_Nebula（胜利星云）T4 LRGB

- **目录**：`testdata/Victory_Nebula_T4_Flying_Dutchman/`
- **目标**：蝘蜓座分子云 LRGB
- **设备**：Chilescope T4, Nikkor 200F2, 焦距200mm, FLI Proline 16200, 10 micron GM1000HPS
- **滤镜**：Baader 50mm LRGB
- **曝光**：Lum 294min, Red 132min, Green 132min, Blue 126min，共684min
- **单张曝光**：LRGB 180s
- **拍摄时间**：2025年03月06日, 2025年02月04-08日
- **帧数**：228（mosaic1 + mosaic2 各 114）
- **FOV 类别**：wide（200mm 短焦）
- **校准帧**：T4 calibration files

### 4.2 Galaxy_Center（银心）T4 RGBHO 三片马赛克

- **目录**：`testdata/Galaxy_Center_T4/`
- **目标**：银心 3 片 RGBHO 马赛克
- **设备**：Chilescope T4 (Nikkor 200F2), 焦距200mm, FLI Microline 16200, 10 Micron GM1000HPs
- **滤镜**：Baader 50mm RGBHaOIII
- **曝光**：Red 96min, Green 96min, Blue 96min, Ha 130min, OIII 330min
- **单张曝光**：RGB 180s, Ha 300s, OIII 600s
- **拍摄时间**：2025年07月02-04日, 2025年07月16日
- **帧数**：157（panel1: 53, panel2: 55, panel3: 49）
- **FOV 类别**：wide（200mm 短焦）
- **校准帧**：T4 calibration files
- **路径结构**：`lights/panel<N>/<filter>/<frame>.fts`

### 4.3 NGC55 T3 LRGBHO

- **目录**：`testdata/NGC55_T3_flying_dutchman/`
- **目标**：NGC55 南鲸鱼星系 LRGBHO
- **设备**：Chilescope T3 (ASA 500N), 焦距1900mm, FLI Proline 16803, DDM85
- **滤镜**：Astrodon LRGBHO 50mm
- **曝光**：Lum 150min, Red 110min, Green 110min, Blue 120min, Ha 320min, OIII 280min
- **单张曝光**：LRGB 600s, HO 1200s
- **拍摄时间**：2025年07月01-06日, 2025年08月29日, 2025年09月15日, 2025年10月13日
- **帧数**：79
- **FOV 类别**：medium/narrow（1900mm 长焦）
- **校准帧**：T3 calibration files

### 4.4 NGC247 T2 LRGBHO

- **目录**：`testdata/NGC247_T2_flying_dutchman/`
- **目标**：NGC247 与博比奇链 LRGBHO
- **设备**：Chilescope T2 (ASA 500N), 焦距1900mm, FLI Proline 16803, DDM85
- **滤镜**：Astrodon LRGBHO 50mm
- **曝光**：Lum 150min, Red 90min, Green 100min, Blue 100min, Ha 220min, OIII 240min
- **单张曝光**：LRGB 600s, HO 1200s
- **拍摄时间**：2025年08月16-17日, 2025年08月29日, 2025年09月01-02日, 2025年09月15日, 2025年09月28-30日, 2025年11月11日
- **帧数**：68
- **FOV 类别**：medium/narrow
- **校准帧**：T2 calibration files

### 4.5 NGC1727 T2 RGBHO

- **目录**：`testdata/NGC1727_T2_flying_dutchman/`
- **目标**：NGC1727 宇宙烟花秀
- **设备**：Chilescope T2 (ASA 500N), 焦距1900mm, FLI Proline 16803, DDM85
- **滤镜**：Astrodon RGBHO 50mm
- **曝光**：Red 120min, Green 110min, Blue 120min, Ha 320min, OIII 390min
- **单张曝光**：RGB 600s, Ha 1200s, OIII 1800s
- **拍摄时间**：2025年10月31日, 2025年11月10-12日, 2025年11月14日, 2025年11月27-30日, 2025年12月01日, 2025年12月08-11日
- **帧数**：64
- **FOV 类别**：medium/narrow
- **校准帧**：T2 calibration files
- **注**：baseline 790 帧未包含此数据集，但 testdata 有

### 4.6 NGC83 T3 LRGB

- **目录**：`testdata/NGC83_cluster_T3_Flying_Dutchman/`
- **目标**：NGC83 星系群 LRGB
- **设备**：Chilescope T3 (ASA 500N), 焦距1900mm, FLI Proline 16803, DDM85
- **滤镜**：Astrodon LRGB 50mm
- **曝光**：Lum 210min, Red 160min, Green 170min, Blue 150min
- **单张曝光**：LRGB 600s
- **拍摄时间**：2025年07月01日, 2025年10月11-12日, 2025年10月17日, 2025年10月19日, 2025年10月24-25日, 2025年11月09-11日
- **帧数**：72
- **FOV 类别**：medium/narrow
- **校准帧**：T3 calibration files
- **注**：baseline 790 帧未包含此数据集，但 testdata 有

### 4.7 LDN43 T2 LRGBHa

- **目录**：`testdata/LDN43_T2素材_flying_dutchman/`
- **目标**：LDN43 飞天蝙蝠星云 LRGBHa
- **设备**：Chilescope T2 (ASA 500N), 焦距1900mm, FLI Proline 16803, DDM85
- **滤镜**：Astrodon LRGB 50mm + 3nm Halpha
- **曝光**：Lum 80+送20min, Red 160min, Green 160min, Blue 160min, Ha 160min
- **单张曝光**：Lum 600s, RGBHa 1200s
- **拍摄时间**：2025年05月03-05日, 2025年05月07日, 2025年05月20日, 2025年06月20日, 2025年07月16-17日
- **帧数**：42
- **FOV 类别**：medium
- **校准帧**：T2 calibration files

## 5. P11-005 回归测试范围说明

### 5.1 ipv 求解器状态

- **未修改 ipv 代码**：P11-004 仅修改了诊断工具 `visualize_reproject.py` 和 P11-002 的 `wcs_closure_diagnostic_v3.py`（CRPIX 1-based bug 修复），未触碰 `lib/plate_solve/cpp/ipv/` 任何 C++ 源码
- **790 帧求解能力不变**：baseline `ipv_baseline_790_v430/per_frame.json` 记录 789/790 pass（success_rate=99.87%），该能力保持不变
- **P11-004 修复范围**：仅重新生成 6 个失败帧的 FITS header（SIP 序列化 + CRPIX 修正），未改代码

### 5.2 P11-005 回归范围

P11-005 目标：对当前 testdata 可用帧做 PlateSolve 回归 + 权威星对 WCS Gate 验证

| 范围 | 帧数 | 说明 |
|------|------|------|
| testdata 可用（7 个数据集） | 710 | Victory_Nebula(228) + Galaxy_Center(157) + NGC55(79) + NGC247(68) + NGC1727(64) + NGC83(72) + LDN43(42) |
| baseline 790 帧中 testdata 可覆盖 | 574 | 上述 710 帧中与 baseline 匹配的（排除 NGC1727 和 NGC83） |
| baseline 790 帧中 testdata 未覆盖 | 216 | NGC4945/NGC7293/M20/NGC6302，testdata 无原始帧 |

### 5.3 路径映射要求

回归测试需将 baseline `per_frame.json` 中的 flat 路径 `testdata/lights/<bn>.fts` 映射到实际嵌套路径：

- **Victory_Nebula**：`testdata/Victory_Nebula_T4_Flying_Dutchman/lights/<bn>.fts`
- **Galaxy_Center**：`testdata/Galaxy_Center_T4/lights/panel<N>/<filter>/<bn>.fts`（需从 bn 解析 filter）
- **NGC55/NGC247/NGC1727/NGC83/LDN43**：`testdata/<target_dir>/lights/<bn>.fts`
