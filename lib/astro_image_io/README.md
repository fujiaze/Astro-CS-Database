# Astro Image IO

FITS / XISF 天文图像统一 IO 库，C++ 原生实现 + Python 封装，零外部依赖。

**版本/性能摘要**：FITS 4500×3600 读取 ~0.030s｜XISF 4500×3600 读取 ~0.033s｜零外部依赖（不依赖 cfitsio / XML 库）。

## 概述

### 功能列表

- **统一数据模型**：`ImageData` 容器屏蔽 FITS / XISF 格式差异，上层代码一套逻辑处理两种格式
- **零外部依赖**：FITS 原生二进制解析（不依赖 cfitsio），XISF 1.0 原生解析（不依赖 XML 库）
- **自动格式检测**：根据文件扩展名 / 文件头自动选择 FITS 或 XISF 读取器
- **完整元数据提取**：FITS 关键字 + WCS 坐标 + 观测元数据 + 校准元数据
- **WCS 坐标支持**：CD 矩阵、CDELT、像素比例尺、旋转角计算
- **FITS 写入**：支持 Float32 / UInt16 写出，保留原始关键字
- **Python 封装**：ctypes 绑定 C++ DLL，提供原生 NumPy 数组访问

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
│   └── astro_image_io.h        # C API 头文件（结构体 + 函数声明）
├── src/
│   ├── aio_fits.cpp/.h         # FITS 原生读写（不依赖 cfitsio）
│   ├── aio_xisf.cpp            # XISF 1.0 原生解析（不依赖 XML 库）
│   ├── aio_api.cpp             # C API 导出层（aio_read / aio_write_fits ...）
│   └── aio_log.cpp/.h          # 日志模块
├── python/
│   └── astro_image_io.py       # Python ctypes 封装（ImageReader / FITSWriter / ImageData）
├── Makefile                    # 编译规则
├── astro_image_io.dll          # 编译产物（C++ DLL）
└── README.md                   # 本文件
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
