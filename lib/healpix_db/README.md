# Healpix Database

天文巡天数据的 HEALpix 球面存储与可视化系统。LOD 金字塔支持多级预览，球面浏览器提供交互式可视化。

本仓库为 HEALpix 数据库核心仓库，仅保留 LOD 金字塔和球面浏览器。单帧读写（.ahpx）、稀疏堆栈（.ahps）、Drizzle 重投影已拆分为独立仓库。

## 模块

| 模块 | 功能 | 技术 |
|------|------|------|
| `healpix_lod/` | LOD 金字塔（多级降采样，增量更新，按需计算） | C++ DLL + Python ctypes |
| `healpix_browser/` | 球面可视化浏览器（STF 拉伸 + 球面渲染 + 投影导出） | Python (PyQt5 + vispy) |
| `docs/` | Drizzle 算法概述文档 | - |
| `tests/` | 端到端集成测试 | Python |

## 关联仓库

| 仓库 | 职责 |
|------|------|
| [Astro-Image-IO-C](https://github.com/fujiaze/Astro-Image-IO-C) | 统一 I/O 层：FITS/XISF 读取、.ahpx 单帧格式、压缩（zstd/lz4）、PipelineFrame 管线骨架 |
| [Healpix-Mosaic](https://github.com/fujiaze/Healpix-Mosaic-C-Python-) | 稀疏 HEALpix 堆栈存储（sigma-clip + SNR 加权合并，.ahps 格式） |
| [Healpix-Drizzle](https://github.com/fujiaze/Healpix-Drizzle-C-Python-) | 球面 Drizzle 重投影（WCS+SIP → HEALPix，通量守恒） |

## 依赖

### C++ 编译
- g++ (C++17)
- OpenMP
- [astro_image_io](https://github.com/fujiaze/Astro-Image-IO-C)（提供 AIO C API：压缩 + .ahpx 读写 + PipelineFrame）
- [healpix_stack](https://github.com/fujiaze/Healpix-Mosaic-C-Python-)（healpix_lod 依赖其 ahps_reader 读取数据层 tile）

### Python
- numpy >= 1.20
- astropy >= 5.0
- PyQt5 >= 5.15 (浏览器)
- vispy >= 0.9 (浏览器)
- healpy >= 1.16 (浏览器)

## 构建

healpix_lod 依赖 astro_image_io.dll 和 healpix_stack 源码（静态编译 ahps_reader），构建前请确保同级行存在 `astro_image_io/` 和 `healpix_stack/` 目录，或通过 Makefile 变量指定路径：

```bash
cd healpix_lod
make            # 带 zstd+lz4 压缩（通过 astro_image_io.dll 提供）
# 或
make no-comp    # 不带压缩 (fallback)
```

编译后生成 `healpix_lod.dll`。

## 使用

### LOD 金字塔

```python
from healpix_lod import LodManager

# 生成完整金字塔
LodManager.generate_full("healpix_db/", band_index=0)

# 增量更新（只重算变化的 tile）
LodManager.update_incremental("healpix_db/", band_index=0, changed_tiles=[100, 101])

# 按需计算单个 tile
tile_data = LodManager.compute_on_demand("healpix_db/", band_index=0, level=2, tile_ipix=100)
```

### 球面浏览器

```bash
cd healpix_browser
python -m healpix_browser
```

支持：
- 单帧浏览（.ahpx 文件，像素/SNR/权重通道切换）
- 球数据库浏览（拖动旋转，滚轮缩放，波段切换，RGB 合成）
- STF 非破坏性拉伸（自动/手动/预设）
- 投影导出（TAN/SIN/ZEA/AIT/CAR，FITS/.ahpx/PNG）

## 文件格式

### .ahpl（LOD 层级）
```
[固定头 34B][压缩数据块]
```
- 数据：ipix(uint64) + value(float) + weight(float) + count(uint16)
- NESTED 位运算父子映射

其他格式（.ahpx 单帧、.ahps 稀疏堆栈）参见对应关联仓库文档。

## 技术特点

- **HEALpix 等面积像素化**：NESTED scheme，nside=32768（~1.7"/px）
- **稀疏存储**：只存有数据的像素，未观测天区零开销
- **全链路压缩**：zstd level 5（像素/统计量）+ lz4（LOD 低层/索引），由 astro_image_io 统一提供
- **分块压缩**：每 4096 像素独立压缩块，支持部分读取
- **OpenMP 16 线程并行**：LOD 生成
- **非破坏性 STF 拉伸**：MTF 公式，MAD 自动计算，不影响原始数据
