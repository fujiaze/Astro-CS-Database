# Healpix Database

天文巡天数据的 HEALpix 球面存储与可视化系统。自定义二进制格式存储单帧图像，稀疏堆栈合并多帧数据，LOD 金字塔支持多级预览，球面浏览器提供交互式可视化。

## 模块

| 模块 | 功能 | 技术 |
|------|------|------|
| `ahpx_io/` | .ahpx 单帧格式读写（像素+SNR+权重+WCS+元数据） | C++ DLL + Python ctypes |
| `healpix_stack/` | 稀疏 HEALpix 堆栈存储（sigma-clip + SNR 加权合并） | C++ DLL + Python ctypes |
| `healpix_lod/` | LOD 金字塔（多级降采样，增量更新，按需计算） | C++ DLL + Python ctypes |
| `healpix_browser/` | 球面可视化浏览器（STF 拉伸 + 球面渲染 + 投影导出） | Python (PyQt5 + vispy) |
| `docs/` | Drizzle 算法概述文档 | - |
| `tests/` | 端到端集成测试 | Python |

## 依赖

### C++ 编译
- g++ (C++17)
- zstd (libzstd)
- lz4 (liblz4)
- OpenMP

### Python
- numpy >= 1.20
- astropy >= 5.0
- PyQt5 >= 5.15 (浏览器)
- vispy >= 0.9 (浏览器)
- healpy >= 1.16 (浏览器)

## 构建

各 C++ 模块独立编译为 DLL，需在各自目录下执行：

```bash
# ahpx_io
cd ahpx_io
make            # 带 zstd+lz4 压缩
# 或
make no-comp    # 不带压缩 (fallback)

# healpix_stack (依赖 ahpx_io/compressor)
cd ../healpix_stack
make

# healpix_lod (依赖 ahpx_io + healpix_stack)
cd ../healpix_lod
make
```

编译后生成 `ahpx_io.dll`、`healpix_stack.dll`、`healpix_lod.dll`。

## 使用

### 单帧读写 (.ahpx)

```python
from ahpx_io import AhpxReader, AhpxWriter

# 写入
writer = AhpxWriter()
writer.set_metadata(metadata_json)
writer.set_pixels(pixels, width, height, channels)
writer.set_snr(snr, width, height)
writer.set_weight_scalar(1.0)
writer.write("frame.ahpx", zstd_level=5)

# 读取
reader = AhpxReader("frame.ahpx")
pixels = reader.read_pixels()   # numpy array (H, W, C)
snr = reader.read_snr()         # numpy array (H, W)
header = reader.header_json      # 元数据 JSON
reader.close()
```

### 稀疏堆栈 (.ahps)

```python
from healpix_stack import StackDatabase, StackEngine

config = {
    "nsideData": 32768,
    "nsideLod": [512, 2048, 8192, 32768],
    "bands": ["L", "R", "G", "B"],
    "tileNside": 512,
    "sigmaClipLow": 3.0,
    "sigmaClipHigh": 3.0,
    "nested": True
}

# 创建数据库
db = StackDatabase.create("healpix_db/", config)

# 全局更新（传入所有帧的 HEALpix 像素数据）
db.update_global(frames)

# 局部更新（只重新堆栈指定文件范围包含的 tile）
db.update_range(frames, file_range)

# 读取 tile
result = db.read_tile(0)
```

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

### .ahpx（单帧）
```
[Magic 4B][Version 2B][HeaderSize 4B][HeaderCompSize 4B][BlockCount 4B]
[压缩 JSON 头][数据块...]
```
- 像素块、SNR 块、权重块独立压缩
- 权重模式：scalar（整图统一）/ grid（分块网格）/ pixel（逐像素）
- 压缩：zstd / lz4 / none，分块压缩（每 4096 像素独立块）

### .ahps（稀疏堆栈）
- 只存有数据的 HEALpix 像素的累计统计量
- sigma-clip + SNR 加权合并
- 支持 4 级 LOD（nside 512/2048/8192/32768）

### .ahpl（LOD 层级）
```
[固定头 34B][压缩数据块]
```
- 数据：ipix(uint64) + value(float) + weight(float) + count(uint16)
- NESTED 位运算父子映射

## 技术特点

- **HEALpix 等面积像素化**：NESTED scheme，nside=32768（~1.7"/px）
- **稀疏存储**：只存有数据的像素，未观测天区零开销
- **全链路压缩**：zstd level 5（像素/统计量）+ lz4（LOD 低层/索引）
- **分块压缩**：每 4096 像素独立压缩块，支持部分读取
- **OpenMP 16 线程并行**：堆栈合并、LOD 生成
- **非破坏性 STF 拉伸**：MTF 公式，MAD 自动计算，不影响原始数据
