# Astro Image IO

FITS / XISF 天文图像统一 IO 库。参考 PixInsight Class Library (PCL) 数据模型，提供格式无关的图像数据容器和读写适配器，上层代码无需关心底层文件格式差异。

## 特性

- **统一数据模型**：`ImageData` 容器屏蔽 FITS/XISF 格式差异，一套代码处理两种格式
- **FITS 读写**：基于 astropy.io.fits，完整支持 FITS 关键字、WCS 坐标、多通道数据
- **XISF 读取**：纯 Python 实现 XISF 1.0 规范解析，支持 Float32 / UInt16 / UInt8 等像素格式；可选 PCL C++ 绑定加速
- **自动格式检测**：`ImageReaderFactory` 根据文件扩展名自动选择读取器
- **WCS 坐标支持**：`WCSKeywords` 从 FITS/XISF 头提取 WCS 坐标信息，支持 CD 矩阵、CDELT、像素比例尺和旋转角计算
- **元数据完整提取**：观测元数据 (DATE-OBS, FOCALLEN, OBJECT...) + 校准元数据 (EXPTIME, FILTER, GAIN, CCDBIN...) + WCS
- **零配置安装**：`pip install -e .`，仅依赖 numpy + astropy

## 安装

```bash
# 开发模式安装
pip install -e .

# 或直接将 astro_image_io/ 目录加入 sys.path
```

### 依赖

| 包 | 版本 | 用途 |
|---|---|---|
| numpy | >= 1.24 | 数组操作 |
| astropy | >= 5.3 | FITS 读写 |

## 快速开始

### 读取图像（自动检测格式）

```python
from astro_image_io import ImageReaderFactory

factory = ImageReaderFactory()

# 自动检测 FITS 或 XISF
img = factory.read("M31_300s_L.fits")

print(f"尺寸: {img.width}x{img.height}, 通道: {img.channels}")
print(f"像素比例尺: {img.pixel_scale_arcsec:.2f} arcsec/px")

# 获取 float32 numpy 数组
data = img.to_numpy()
```

### 仅读取元数据（不加载像素数据）

```python
# 基本信息快照（轻量，不读像素）
info = factory.read_info("light_001.fits")
print(f"{info.width}x{info.height}, {info.channels}ch")

# 完整元数据
meta = factory.read_metadata("light_001.fits")
if meta.wcs:
    print(f"RA: {meta.wcs.crval1:.6f}, Dec: {meta.wcs.crval2:.6f}")
    print(f"像素比例尺: {meta.wcs.pixel_scale_arcsec:.2f} arcsec/px")
if meta.observation:
    print(f"目标: {meta.observation.object_name}")
    print(f"焦距: {meta.observation.focallen:.1f} mm")
if meta.calibration:
    print(f"曝光: {meta.calibration.exptime:.1f}s, 滤镜: {meta.calibration.filter_name}")
```

### FITS 写入

```python
from astro_image_io import FITSWriter, ImageData, ImageGeometry

img = ImageData(
    data=calibrated_array,
    geometry=ImageGeometry(width=4500, height=3600),
    keywords=original_keywords,
    metadata=original_metadata,
)

writer = FITSWriter()
writer.write(img, "output/calibrated.fits")
```

### XISF 读取

```python
from astro_image_io import XISFReader

reader = XISFReader()
img = reader.read("M31_300s_L.xisf")
print(f"格式: {img.source_format}, 尺寸: {img.width}x{img.height}")
```

## 核心类参考

### 数据容器

| 类 | 说明 |
|---|---|
| `ImageData` | 统一图像数据容器：像素数组 + 几何信息 + 元数据 + FITS 关键字 |
| `ImageGeometry` | 图像几何信息：宽度、高度、通道数 |
| `ImageOptions` | 样本属性：位深度、浮点/整数、有符号/无符号 |
| `ImageInfo` | 只读元数据快照（不包含像素数据） |
| `ImageMetadata` | 完整元数据：WCS + 观测 + 校准 |

### 元数据

| 类 | 说明 |
|---|---|
| `FITSKeyword` | FITS 头关键字：name / value / comment |
| `WCSKeywords` | WCS 坐标关键字集合：CD 矩阵 / CRVAL / CDELT / 像素比例尺 / 旋转角 |
| `ObservationMetadata` | 观测元数据：时间 (DATE-OBS) / 地点 (SITELAT,SITELONG) / 焦距 (FOCALLEN) / 口径 (APTDIA) |
| `CalibrationMetadata` | 校准元数据：曝光 (EXPTIME) / 滤镜 (FILTER) / 增益 (GAIN) / CCD温度 (CCD-TEMP) / 二值化 (CCDBIN) |

### 读写适配器

| 类 | 说明 |
|---|---|
| `ImageReaderFactory` | 格式自动检测 + 适配器分发，根据扩展名选择 FITS/XISF 读取器 |
| `FITSReader` | FITS 读取适配器，基于 astropy.io.fits |
| `FITSWriter` | FITS 写入适配器 |
| `XISFReader` | XISF 读取适配器：优先使用 PCL C++ 绑定，回退到纯 Python 解析 |

## 架构

```
FITS / XISF 文件
    │
    ├── FITSReader ──┐
    │                ├──→ ImageData (统一容器)
    └── XISFReader ──┘
         │
         ├── _xisf_pcl.py  (PCL C++ 绑定, 可选加速)
         └── _xisf_py.py   (纯 Python XISF 1.0 解析器, 默认)
```

### XISF 解析双引擎

- **纯 Python** (`_xisf_py.py`)：零外部依赖，解析 XISF 1.0 规范的 XML 头 + 二进制数据块，支持 Float32 / UInt16 / UInt8
- **PCL 绑定** (`_xisf_pcl.py`)：通过 ctypes 调用 PixInsight PCL 的 XISF 读写模块，速度更快，需要 PCL 动态库

## 目录结构

```
astro_image_io/
├── astro_image_io/
│   ├── __init__.py              # 包入口，导出所有公共类
│   ├── reader.py                # ImageReader / ImageReaderFactory
│   ├── writer.py                # FITSWriter / XISFWriter
│   ├── fits_reader.py           # FITS 读取适配器
│   ├── xisf_reader.py           # XISF 读取适配器 (双引擎调度)
│   ├── _xisf_py.py              # 纯 Python XISF 1.0 解析器
│   ├── _xisf_pcl.py             # PCL C++ 绑定 (可选)
│   ├── image_data.py            # ImageData 统一容器
│   ├── image_metadata.py        # ImageMetadata 完整元数据
│   ├── wcs_keywords.py          # WCSKeywords 坐标关键字
│   ├── keywords.py              # FITSKeyword
│   ├── observation_metadata.py  # 观测元数据
│   ├── _types.py                # 类型别名
│   └── errors.py                # 异常定义
├── example/
│   └── demo.py                  # 使用示例
├── pyproject.toml               # 包配置
└── README.md                    # 本文件
```

## 许可

MIT License
