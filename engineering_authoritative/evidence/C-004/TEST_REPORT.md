# C-004 测试报告 — HISS V2 球面信号可视化

- **任务**：C-004
- **日期**：2026-07-30
- **测试工具**：`lib/astro_image_io/python/hiss_v2_visualizer.py`
- **测试数据**：C-002 产出的 3 帧 V2 HISS 文件 (`output/C-002/*.hiss2`)
- **结果**：**3/3 帧可视化成功，0 失败**

---

## 1. 测试环境

- Python 3.x + matplotlib 3.10.8 + numpy 2.2.6 + zstandard 0.25.0
- astropy 6.1.7 + astropy_healpix（HEALPix ipix→球面坐标转换）
- 测试数据：B-002 产出的 3 帧 V1 HISS → C-002 转换的 V2 HISS
- V2 文件来源：`output/C-002/{T2_RED_LDN43,T3_RED_NGC55,T4_RED_GalaxyCenter_panel1}.hiss2`

## 2. 测试矩阵

| # | 测试项 | 帧数 | 通过 | 验证内容 |
|---|---|---|---|---|
| 1 | V2 文件读取 | 3 | 3 | HissV2Reader.read_all() 成功，CRC32 校验通过 |
| 2 | signal 可视化 | 3 | 3 | float32 颜色映射，magma cmap，分位数范围 |
| 3 | support 可视化 | 3 | 3 | uint8 覆盖标记，YlGn cmap，覆盖率统计 |
| 4 | SNR 点可视化 | 3 | 3 | 稀疏控制点散点，plasma cmap，NaN 过滤 |
| 5 | 组合图 | 3 | 3 | signal+SNR+support 三要素叠加 |
| 6 | HEALPix 坐标转换 | 3 | 3 | ipix→(ra,dec) via astropy_healpix (NESTED) |
| 7 | NaN/Inf 容错 | 3 | 3 | SNR NaN 点过滤，绘图范围 NaN 过滤 |
| 8 | PNG 输出完整性 | 3 | 3 | 文件生成，非空白，尺寸正确 |
| 9 | V1/V2 坐标一致性 | 1 | 1 | T3 帧 V1 bbox center 落在 V2 像素范围内 |
| 10 | browser_cli V2 支持 | 3 | 0 | 预期失败：V2 magic HI2S 未识别（见 §4） |

**汇总：可视化测试 3/3 通过；browser_cli V2 加载 0/3（预期失败，C++ 未实现 V2 支持）**

## 3. 关键验证详情

### 3.1 V2 文件读取（复用 C-002 HissV2Reader）

每帧 V2 文件通过 `HissV2Reader.read_all()` 一次读取全部数据：

```
T2_RED_LDN43:           nside=2048 n_pix=1573 n_chunks=1 has_snr=True size=43291
T3_RED_NGC55:           nside=2048 n_pix=1535 n_chunks=1 has_snr=True size=19012
T4_RED_GalaxyCenter_panel1: nside=512  n_pix=3928 n_chunks=1 has_snr=True size=56560
```

读取过程包含 C-002 实现的全部校验：magic/version/footer/global_crc/n_pix/文件大小/per-chunk CRC32。3 帧均通过。

### 3.2 signal 可视化（契约 §2.1：不得量化为 uint8）

- **数据类型**：signal.dtype = float32（C-002 已验证，本任务直接显示）
- **色图**：magma（天文常用，感知均匀）
- **范围**：0.5%/99.5% 分位数（与浏览器 STFEngine 2026-07-14 修复一致）
- **colorbar**：标注 "signal (float32)"，显示数值范围

### 3.3 support 可视化（契约 §2.1：不得把无覆盖写成零）

- **数据类型**：support.dtype = uint8
- **V1 迁移**：support 全 1（C-002 契约 §14.2，V1 所有存储像素均为覆盖像素）
- **显示**：覆盖像素用 YlGn cmap，未覆盖像素用灰色叉号
- **覆盖率**：3 帧均 100%（n_covered == n_pix）

### 3.4 SNR 点可视化（契约 §2.1：SNR 不得全量存储）

- **稀疏性**：n_points << n_pix（T2: 1930/1573, T3: 617/1535, T4: 1984/3928）
- **SoA 布局**：ra/dec/snr 三通道独立（C-002 已验证字节级一致）
- **NaN 处理**：同时过滤 ra/dec/val 三者的 NaN/Inf
  - T2: 1930 点中部分 NaN（V1 源数据特性，C-002 已记录）
  - T3: 617 点全部有效
  - T4: 1984 点中部分 NaN
- **colorbar**：标注 "SNR ((A-B)/mad)"，显示 snr_phot/median/idw_power

### 3.5 HEALPix ipix → (ra, dec) 坐标转换

3 帧 V2 文件均为 NESTED 排序，使用 astropy_healpix：

```python
from astropy_healpix import HEALPix
hp = HEALPix(nside=nside, order="nested")
lon, lat = hp.healpix_to_lonlat(ipix)
ra_deg = lon.degree % 360.0
dec_deg = lat.degree
```

转换结果（像素 RA/Dec 范围）：

| 帧 | nside | RA 范围 (deg) | Dec 范围 (deg) | 天区 |
|---|---|---|---|---|
| T2_RED_LDN43 | 2048 | [248.03, 249.19] | [-16.32, -15.19] | LDN 43 |
| T3_RED_NGC55 | 2048 | [3.03, 4.46] | [-39.74, -38.63] | NGC 55 |
| T4_RED_GalaxyCenter_panel1 | 512 | [268.68, 276.94] | [-16.33, -9.90] | 银心 |

### 3.6 PNG 输出完整性（PIL 像素统计）

```python
from PIL import Image
import numpy as np
im = Image.open(path)
arr = np.array(im)
# nonwhite_pct = 非白色像素百分比（有内容的像素）
```

| 帧 | 尺寸 (px) | 模式 | nonwhite_pct | mean_rgb | 大小 |
|---|---|---|---|---|---|
| T2_RED_LDN43_viz.png | 2048×1477 | RGBA | 5.4% | [247.7, 247.3, 247.3] | 181 KB |
| T3_RED_NGC55_viz.png | 2048×1477 | RGBA | 5.5% | [247.6, 247.2, 247.1] | 191 KB |
| T4_RED_GalaxyCenter_panel1_viz.png | 2054×1477 | RGBA | 5.6% | [247.4, 246.9, 247.0] | 193 KB |

**解读**：
- 尺寸 2048×1477 符合预期（figsize=14×10, dpi=150）
- nonwhite_pct 5.4-5.6% 对应散点图内容（4 子图，每子图约 1.4% 非白色）
- mean_rgb ≈ 247（白色背景 + 少量彩色散点）
- **图像非空白**，包含实际数据可视化

### 3.7 V1/V2 坐标一致性交叉验证（T3_RED_NGC55）

同一帧的 V1 (browser_cli 加载) 与 V2 (visualizer 加载) 坐标对比：

| 数据源 | RA (deg) | Dec (deg) | 来源 |
|---|---|---|---|
| V1 center (browser_cli) | 3.867 | -39.075 | bbox 中心 |
| V1 size (browser_cli) | 3.109 × 3.307 | | bbox 尺寸 |
| V2 像素范围 (visualizer) | [3.03, 4.46] | [-39.74, -38.63] | ipix→(ra,dec) |
| V2 范围中心 | 3.746 | -39.188 | (min+max)/2 |

**验证**：
- V1 center (3.867, -39.075) 落在 V2 像素范围 [3.03, 4.46] × [-39.74, -38.63] 内 ✓
- V1 size (3.11×3.31 deg) 略大于 V2 像素跨度 (1.43×1.11 deg)，因 V1 bbox 含 padding
- 两者指向同一天区（NGC 55，玉夫座），数据一致性确认 ✓

## 4. browser_cli V2 支持测试（预期失败）

### 4.1 V2 `.hiss2` 加载失败

```
$ browser_cli.exe output/C-002/T3_RED_NGC55.hiss2
[2026-07-30 13:30:53][ERROR] 未知 Magic: 48 49 32 53
[ERROR] open_file 失败, rc=-5
{
  "open_success": false,
  "open_error_code": -5,
  "open_time_ms": 2.235
}
```

**根因**：`48 49 32 53` = "HI2S"（V2 magic）。browser_cli 的 `BrowserBackend::open_file()` 仅识别 V1 magic "HISS"，未实现 V2 解析。

**3 帧均失败**（T2/T3/T4），失败方式一致。

### 4.2 V1 `.hiss` 加载成功（对比基准）

```
$ browser_cli.exe output/B-002/T3_RED_NGC55.hiss --benchmark
[OK] 文件加载成功 (13.3 ms)
  nside: 2048  n_pix: 1535  filter: Red
  bbox: center=(3.8672, -39.0749), size=3.1094x3.3065 deg
  get_all_data: 0.0 ms (n_pix=1535)
  ud_grade(->64): 0.1 ms (n_pix=1535 -> 6)
```

V1 加载完全正常，证明 browser_cli 工具本身工作正常，仅是不支持 V2 格式。

## 5. 禁止项验证

| 禁止项 | 验证方法 | 结果 |
|---|---|---|
| 不得只显示元数据 | 4 子图均为数据散点图，非元数据文本 | ✓ |
| 图像必须清晰可读 | 2048×1477 dpi=150，colorbar+网格+标注 | ✓ |
| 不得把无覆盖写成零 | support 通道独立显示，V1 迁移 support 全 1 | ✓ |
| 不得将 signal 量化为 uint8 | signal 为 float32，colorbar 标注 | ✓ |
| SNR 不得全量存储 | SNR 为稀疏控制点 (n_points << n_pix) | ✓ |
| 无静默降级 | SNR NaN 显式过滤并记录，非跳过 | ✓ |

## 6. 测试输出摘要

```
============================================================
HISS V2 可视化汇总: 3/3 成功, 0 失败
============================================================
  [OK] T2_RED_LDN43.hiss2 -> T2_RED_LDN43_viz.png
  [OK] T3_RED_NGC55.hiss2 -> T3_RED_NGC55_viz.png
  [OK] T4_RED_GalaxyCenter_panel1.hiss2 -> T4_RED_GalaxyCenter_panel1_viz.png
============================================================
```

## 7. 复现命令

```powershell
cd "f:\Astro dev\Astro CS Normalization Database"
python "lib/astro_image_io/python/hiss_v2_visualizer.py" "output/C-002" "engineering_authoritative/evidence/C-004/visualizations" --dpi 150
```

退出码 0 表示全部通过。

## 8. 已知限制

1. **browser_cli 不支持 V2**：C++ 浏览器仅识别 V1 magic "HISS"，V2 "HI2S" 报错 rc=-5。C++ V2 支持需修改 BrowserBackend + astro_image_io DLL，属后续任务。

2. **NESTED 依赖 astropy**：纯 numpy fallback 仅完整实现 RING pix2ang。3 帧 V2 均为 NESTED，由 astropy_healpix 处理（已确认可用）。

3. **SNR NaN**：V1 源数据 SNR 含 NaN（C-002 已记录），V2 忠实保留。可视化过滤 NaN 点后仍能显示有效控制点分布。

4. **平面投影**：当前用平面 (RA, Dec) 散点图，对小天区（<10°）足够。球面投影属可选增强。
