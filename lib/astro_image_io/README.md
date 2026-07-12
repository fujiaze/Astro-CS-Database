# Astro Image IO

FITS / XISF 天文图像统一 IO 库 + Pipeline 管线引擎，C++ 原生实现 + Python 封装，零外部依赖。

**版本/性能摘要**：FITS 4500×3600 读取 ~0.030s｜XISF 4500×3600 读取 ~0.033s｜零外部依赖（不依赖 cfitsio / XML 库）。

## 概述

### 功能列表

- **统一数据模型**：`ImageData` 容器屏蔽 FITS / XISF 格式差异，上层代码一套逻辑处理两种格式
- **零外部依赖**：FITS 原生二进制解析（不依赖 cfitsio），XISF 1.0 原生解析（不依赖 XML 库）
- **自动格式检测**：根据文件扩展名 / 文件头自动选择 FITS 或 XISF 读取器
- **完整元数据提取**：FITS 关键字 + WCS 坐标 + 观测元数据 + 校准元数据
- **WCS 坐标支持**：CD 矩阵、CDELT、像素比例尺、旋转角计算
- **FITS 写入**：支持 Float32 / UInt16 写出，保留原始关键字，零拷贝写入 + 1MB 缓冲
- **Python 封装**：ctypes 绑定 C++ DLL，提供原生 NumPy 数组访问
- **UTF-8 路径支持**：Windows 下支持中文路径（`aio_fopen_utf8`）
- **.ahpx 单帧格式**：自定义二进制存储（像素+SNR+权重+WCS+元数据），zstd/lz4 分块压缩
- **压缩层**：aio_compress / aio_decompress C API（codec: 0=NONE, 1=ZSTD, 2=LZ4）
- **Pipeline 管线引擎**：PipelineFrame 内存数据结构 + 阶段注册/调度 + 动态内存管理 + XML 调试导出 + 多帧批量并行（OpenMP 16线程）

### 性能指标

| 操作 | 图像尺寸 | 耗时 | 说明 |
|---|---|---|---|
| FITS 读取 | 4500×3600 | ~0.030s | 含头解析 + 像素转 float32 |
| XISF 读取 | 4500×3600 | ~0.033s | 含 XML 头解析 + 像素转 float32 |
| 仅读 Header | 4500×3600 | <0.001s | 不加载像素数据 |
| 仅读元数据 | 4500×3600 | <0.001s | 直接返回 metadata 结构 |

## 使用方法

### 编译

```bash
g++ -O2 -march=native -Wall -std=c++17 -shared -o astro_image_io.dll \
    src/aio_fits.cpp src/aio_xisf.cpp src/aio_api.cpp src/aio_log.cpp \
    -Iinclude -Isrc -static-libgcc -static-libstdc++ -lm
```

也可使用 Makefile：

```bash
make        # 编译 astro_image_io.dll
make clean  # 清理
```

### Python 调用示例

```python
from astro_image_io import ImageReader, FITSWriter, ImageData

reader = ImageReader()

# 自动检测 FITS / XISF
img = reader.read("M31_300s_L.fits")

print(f"尺寸: {img.width}x{img.height}, 通道: {img.channels}")
print(f"像素比例尺: {img.pixel_scale_arcsec:.2f} arcsec/px")

# 获取 float32 numpy 数组（零拷贝）
data = img.to_numpy()
```

仅读取元数据（不加载像素）：

```python
meta = reader.read_metadata("light_001.fits")
if meta.wcs and meta.wcs.has_wcs:
    print(f"RA: {meta.wcs.crval1:.6f}, Dec: {meta.wcs.crval2:.6f}")
if meta.observation:
    print(f"目标: {meta.observation.object_name}, 焦距: {meta.observation.focallen:.1f} mm")
if meta.calibration:
    print(f"曝光: {meta.calibration.exptime:.1f}s, 滤镜: {meta.calibration.filter_name}")
```

FITS 写入：

```python
from astro_image_io import FITSWriter, FITSKeywordPy

writer = FITSWriter()
writer.write(calibrated_array, "output/calibrated.fits",
             keywords=original_keywords, float_sample=True)
```

## 架构

### 格式支持

| 格式 | 扩展名 | 支持的像素类型 |
|---|---|---|
| FITS | .fits / .fit / .fts | BITPIX 8 / 16 / 32 / -32 / -64 |
| XISF | .xisf | Float32 / Float64 / UInt8 / UInt16 / UInt32 |

所有像素类型在读取时统一转换为 float32 存入 `ImageData`，写入 FITS 时可按 float32 或 uint16 输出。

### 目录结构

```
astro_image_io/
├── include/
│   ├── astro_image_io.h        # C API 主头文件（FITS/XISF + 压缩 + .ahpx + Pipeline）
│   ├── aio_pipeline.h          # PipelineFrame 数据结构 + 内存管理 + XML 导出
│   └── aio_pipeline_engine.h   # 管线编排引擎（阶段注册/调度/批量并行）
├── src/
│   ├── aio_fits.cpp/.h         # FITS 原生读写（不依赖 cfitsio）
│   ├── aio_xisf.cpp            # XISF 1.0 原生解析（不依赖 XML 库）
│   ├── aio_api.cpp             # C API 导出层（aio_read / aio_write_fits ...）
│   ├── aio_log.cpp/.h          # 日志模块
│   ├── aio_compressor.cpp/.h   # zstd/lz4 压缩层
│   ├── aio_pipeline.cpp        # PipelineFrame 实现（内存管理 + XML 导出）
│   ├── aio_pipeline_engine.cpp # 引擎实现（OpenMP 并行 + 自动内存释放）
│   └── ahpx/
│       ├── aio_ahpx_writer.cpp # .ahpx 单帧格式写入
│       ├── aio_ahpx_reader.cpp # .ahpx 单帧格式读取
│       └── aio_ahpx_api.cpp    # .ahpx C API 导出
├── python/
│   └── astro_image_io.py       # Python ctypes 封装（ImageReader / FITSWriter / PipelineFrame / PipelineEngine）
├── Makefile                    # 编译规则（-fopenmp 并行）
├── astro_image_io.dll          # 编译产物（C++ DLL）
└── README.md                   # 本文件
```

### Pipeline 管线引擎

管线引擎提供 `PipelineFrame` 内存数据结构和阶段编排能力。各算法模块（calibration / plate_solve / photometric / drizzle / stack）注册为阶段处理器，通过内存管线传递数据，不直接接触文件系统。

**内存生命周期**：引擎按阶段自动释放不再需要的字段（PLATESOLVE 后释放 weight，DRIZZLE 后释放 pixels/snr/weight，STACK 后释放 healpix）。可通过 `set_auto_free(0)` 禁用以保留中间数据。

**XML 调试导出**：开发期可将任意阶段后的 PipelineFrame 导出为 XML 文件（像素数据 base64 编码），用于调试。不用于生产。

```python
from astro_image_io import (
    PipelineFramePy, PipelineEngine,
    STAGE_CALIBRATE, STAGE_PLATESOLVE, STAGE_PHOTOMETRIC, STAGE_DRIZZLE, STAGE_STACK,
    DEBUG_AFTER_CALIBRATE, DEBUG_AFTER_DRIZZLE,
)

# 1. 创建帧并填充数据
frame = PipelineFramePy()
frame.set_pixels(image_array, width, height)
frame.set_wcs(crval1, crval2, crpix1, crpix2, cd_matrix)
frame.set_metadata(exptime, filter_name, object_name)

# 2. 创建引擎，注册阶段处理器
engine = PipelineEngine()
engine.register(STAGE_CALIBRATE, calibrate_handler, calibrate_params)
engine.register(STAGE_PLATESOLVE, platesolve_handler, platesolve_params)
engine.register(STAGE_DRIZZLE, drizzle_handler, drizzle_params)

# 3. 配置调试导出（可选）
engine.set_debug("./debug_output", DEBUG_AFTER_CALIBRATE | DEBUG_AFTER_DRIZZLE, skip_pixels=1)

# 4. 单帧执行
engine.run_single(frame, STAGE_CALIBRATE, STAGE_DRIZZLE)

# 5. 批量并行（OpenMP 16 线程）
engine.run_batch(frames, n_threads=16, from_stage=STAGE_CALIBRATE, to_stage=STAGE_DRIZZLE)
```

**C++ 阶段处理器签名**（in-place 模式）：

```c
typedef int (*PipelineStageHandler)(PipelineFrame* frame,
                                     const void* params,
                                     char* error_msg, int error_capacity);
```

### 依赖

**C++ 编译/运行**：零外部依赖，仅需 MinGW-w64 g++（支持 C++17）。

**Python 运行**：

| 包 | 版本 | 用途 |
|---|---|---|
| numpy | >= 1.24 | 像素数组访问 |

Windows 下 DLL 运行时依赖 MSYS2 MinGW64 运行库（`C:\msys64\mingw64\bin`），Python 封装会自动加载。

## 详细文档

- **C++ 仓库（当前版本）**：https://github.com/fujiaze/Astro-Image-IO-C
- **Python 旧版仓库**：https://github.com/fujiaze/Astro-Image-IO-Py

## 变更日志

### 2026-07-12: Pipeline 管线引擎 + .ahpx 格式 + 压缩层

**Pipeline 管线引擎**

新增管线编排引擎，提供 `PipelineFrame` 内存数据结构和阶段调度能力：

- `PipelineFrame`：统一内存数据容器，包含图像像素、SNR、权重、WCS、SIP、HEALPix、元数据等全部字段，按需分配/释放
- `PipelineEngine`：阶段注册 + 顺序调度 + OpenMP 16 线程批量并行
- 内存生命周期自动管理：PLATESOLVE 后释放 weight，DRIZZLE 后释放 pixels/snr/weight，STACK 后释放 healpix
- XML 调试导出：任意阶段后可将 PipelineFrame 导出为 XML（像素 base64 编码），仅用于开发调试
- in-place 阶段处理器签名：`int (*handler)(PipelineFrame*, const void* params, char* err, int cap)`

**.ahpx 单帧格式**

自定义二进制存储格式，保存像素 + SNR + 权重 + WCS + 元数据，支持 zstd/lz4 分块压缩。用于管线中间结果持久化。

**压缩层**

`aio_compress` / `aio_decompress` / `aio_compress_bound` C API，支持 codec: 0=NONE, 1=ZSTD, 2=LZ4。

### 2026-07-10: UTF-8 路径支持 + FITS 写入优化

**UTF-8 路径支持**

新增 `aio_util.h`，提供 `aio_fopen_utf8()` 工具函数。Windows 下 `std::fopen` 不支持 UTF-8 编码的中文路径（如"全链路测试数据"），导致文件打开失败。`aio_fopen_utf8` 在 Windows 上使用 `MultiByteToWideChar` + `_wfopen` 打开文件，Linux/macOS 直接调用 `std::fopen`。

`aio_fits.cpp` 和 `aio_xisf.cpp` 中所有 `std::fopen` 调用已替换为 `aio_fopen_utf8`。

**FITS 写入优化**

`fits_write_file` 的 float32 写入路径有两个性能问题：
1. 逐像素循环拷贝 + 字节序交换（1620万次迭代），即使小端系统不需要交换
2. 额外分配 64MB 临时缓冲

修复：
- 小端系统（不需要 swap）：直接 `std::fwrite(image->data, ...)` 零拷贝写入
- 大端系统：`std::memcpy` 批量拷贝后再逐像素交换
- int16 写入：OpenMP 并行化 `#pragma omp parallel for`
- 添加 1MB 文件写入缓冲（`std::setvbuf`），减少磁盘 IO 次数

### 2026-07-10: FITS 关键字写入修复

**问题1: `write_card` 引号判断逻辑错误**

`write_card` 函数中判断 FITS 关键字值是否需要加引号的逻辑有缺陷，使用硬编码的关键字名称列表（CTYPE1/RADESYS/OBJECT 等）来判断是否为字符串，导致数值型关键字被错误加引号、字符串型关键字未加引号，产生"非法的 SIMPLE 关键字值"等读取错误。

修复：改用类型推断策略——已带引号的不再加、布尔值 T/F 不加、`strtod` 可完整解析的数值不加、其余视为字符串加引号。

**问题2: BZERO/BSCALE 关键字泄漏**

`fits_write_file` 写入 FITS 时复制原始关键字的过滤列表未包含 BZERO/BSCALE。当从 uint16 FITS（BZERO=32768）读取数据转为 float32 后再写出，BZERO 关键字被保留，后续读取时 C++ 再次应用 BZERO 导致数据偏移（均值从 ~450 ADU 跳升到 ~65981 ADU）。

修复：在 `fits_write_file` 的关键字过滤列表中增加 BZERO/BSCALE，写出的 FITS 不再携带原始数据的缩放参数。
