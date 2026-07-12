# HEALpix Drizzle 引擎

## 用途
将校准后的 FITS 图像通过球面 Drizzle 投影到 HEALPix 网格上，输出 .ahpx 格式的 HEALPix 单帧文件。这是连接平面 CCD 图像与 HEALPix 球面数据库的关键环节。

## 数据流
```
FITS 图像 (含 WCS+SIP) → Drizzle 引擎 → .ahpx (HEALPix 单帧) → healpix_stack 堆栈数据库
```

## 算法：球面 Drizzle 6 步流水线

### 核心理念
不做平面重采样，所有像素直接球面投影 + 面积加权 Drizzle。严格通量守恒，无插值模糊。

### 逐像素流水线

1. **取像素四角**：`(x±0.5, y±0.5)` 四个平面角点
2. **Pixfrac 收缩**：以像素中心为基准，`corner = center + pixfrac × (corner - center)`，在平面空间完成
3. **SIP+WCS 逐角映射**：对 4 个收缩后角点分别应用 AP/BP 逆变换 + CD + TAN 反投影，得到 4 个 (RA,Dec) 球面坐标
4. **HEALPix 邻域检索**：用像素中心 (RA,Dec) 调用 queryDisc 获取所有可能相交的 HEALPix 像素
5. **局部切平面面积裁剪**：将球面四边形和候选 HEALPix 像素投影到切平面，用 Sutherland-Hodgman 多边形裁剪计算重叠面积
6. **通量守恒分配**：`weight = overlap_area / pixel_quad_area`，将通量×weight、SNR²×weight、weight 累加到对应 HEALPix 像素

### 多帧叠加归一化
- Drizzle 输出原始累积量：sum_flux, sum_weight, sum_snr_sq
- 最终亮度 = sum_flux / sum_weight（由堆栈模块计算）
- SNR = sqrt(sum_snr_sq / sum_weight)（由堆栈模块计算）

## 关键约束
1. Pixfrac 收缩必须在平面空间完成
2. 必须四角映射，禁止仅中心单点投影
3. 全程严格通量守恒，无插值模糊、无高斯核、无 PSF 混入
4. 畸变完全前置解耦（SIP 在 Drizzle 前完成），Drizzle 仅负责面积分配
5. 面积计算用局部切平面近似（10"/px 尺度下误差 <0.01%）

## 模块文件
| 文件 | 功能 |
|------|------|
| `fits_reader.h/.cpp` | FITS 文件读取 (头解析 + 像素数据, 不依赖 cfitsio) |
| `wcs_sip.h/.cpp` | WCS+SIP TAN 投影坐标转换 (C++ 自实现) |
| `poly_clip.h/.cpp` | 局部切平面多边形裁剪 (gnomonic + Sutherland-Hodgman + Shoelace) |
| `drizzle_engine.h/.cpp` | Drizzle 核心引擎 (6 步流水线 + OpenMP 并行) |
| `hp_drizzle_api.h/.cpp` | C API 导出层 |
| `healpix_drizzle.py` | Python ctypes 绑定 |
| `tests/test_drizzle.py` | 单元测试 |

## 依赖
- `ahpx_io/compressor` (压缩, 静态链接)
- `ahpx_io/ahpx_writer` (.ahpx 输出, 静态链接)
- `healpix_stack/healpix_core` (HEALPix 坐标运算, 静态链接)
- zstd, lz4 (压缩库)

## 构建
```bash
cd healpix_drizzle
make            # 带 zstd+lz4 压缩
make no-comp    # 不带压缩
```

## Python 接口
```python
from healpix_drizzle import drizzle_fits_to_ahpx

result = drizzle_fits_to_ahpx(
    fits_path="frame.fits",
    output_ahpx_path="frame.ahpx",
    nside=32768,
    nested=True,
    pixfrac=0.8
)
```
