# Star Detector - 天文图像星点检测器

从16bit天文图像中检测星点，参考 Siril `PSF.c` / `star_finder.c` 的方法，采用 GSL trust-region LM Gaussian 拟合 + halfA 边界搜索初始化 + 半阈值饱和星检测，输出坐标/flux/饱和标记及可选拟合参数。

**V5.0** | 16线程 4500×3600 银心 ~9s | 前60匹配率中位 98.3% | IPv拟合率 100% | 3帧验证全部达标

## 概述

### 功能列表

- **全C++核心算法**：动态背景分离、连通域分析、GSL LM Gaussian 拟合、饱和星检测、去重、排序全部在 C++ 中实现
- **Python 胶水层**：ctypes 调用 DLL + 结果可视化，不参与核心计算
- **16bit 原生输入**：直接接收 uint16 图像数据，适配天文相机 ADC
- **饱和星处理机制**：GSL trust-region LM、halfA 边界搜索初始化、mag_est 候选排序、reject_star 圆度/FWHM 过滤、PSF_ERR_DIVERGED 丢弃
- **自适应 fitRadius**：基于连通域大小估算 FWHM，fitRadius=0 触发自动模式
- **半阈值饱和星检测**：独立流程检测 PSF 变形的饱和星，圆盘拟合 + 等效半径
- **有序输出**：饱和星在前按 r 降序，正常星按 flux 降序
- **可选参数输出**：`fwhm_x`, `fwhm_y`, `sx`, `sy`, `theta`, `background`, `amplitude`, `r`

### 性能指标

测试环境：16线程CPU + 64GB内存

| 图像 | 分辨率 | 饱和星 | 正常星 | 总计 | 耗时 |
|------|--------|--------|--------|------|------|
| Red帧(银心) | 4500×3600 | 129 | 40912 | 41041 | ~9s |

各阶段耗时分布：

| 阶段 | 耗时 | 占比 |
|------|------|------|
| uint16->float | 17 ms | 0.2% |
| 动态背景分离 | 447 ms | 5.2% |
| 连通域分析+候选提取 | 178 ms | 2.1% |
| Gaussian 拟合 (16线程) | 7875 ms | 91.0% |
| 饱和星检测 | 37 ms | 0.4% |
| 去重+排序 | 19 ms | 0.2% |

V4.66 全帧测试（793帧 IPv vs Siril 对比）：

| 指标 | 数值 |
|------|------|
| 前60匹配率中位 | 98.3% |
| IPv拟合率中位 | 100% |
| 非饱和星达标率（位置+顺序>90%） | 92.4% |
| 3帧抽样全部达标 | 3/3 |

## 效果展示

![检测效果示例](test_output/example.jpg)

*绿色十字 = 正常 PSF 拟合星点，红色圆圈 = 饱和星点（半径=r）*

## 使用方法

### 编译

依赖：MinGW-w64 g++ (C++17)、OpenMP、GSL (libgsl)

```bash
g++ -std=c++17 -O3 -march=native -Wall -fopenmp -funroll-loops -ffp-contract=fast \
    -shared -o star_detector.dll src/*.cpp -Iinclude -Isrc \
    -static-libgcc -static-libstdc++ -lgsl -lgslcblas -lm
```

或使用 Makefile：

```bash
make all
```

环境变量 `STAR_DETECTOR_LOG_LEVEL`：日志级别（0=INFO, 1=DEBUG, 2=WARN, 3=ERROR），默认 INFO

### Python 调用

```python
from star_detector import StarDetector

det = StarDetector()

# 基础检测：返回 [(x, y), ...]
coords = det.detect(image)

# 扩展检测：正常星 + 饱和星 + 可选参数
result = det.detect_ex(image, extra_names=['fwhm_x', 'fwhm_y', 'r'])
# result.x / result.y / result.flux / result.saturated / result.extras
# 饱和星在前(saturated=1, flux=-1, r有效)，正常星在后(saturated=0, flux=振幅A)

# 调试图像输出（绿色十字=正常星，红色圆圈=饱和星）
det.detect_debug_image(image, "debug_output.png", extra_names=['r'])

det.close()
```

### C API

```c
#include "star_detector.h"

StarDetectorHandle sdet_create(const SDetParams *params);
void sdet_destroy(StarDetectorHandle handle);

/* 基础检测：仅正常星坐标 */
int sdet_detect(StarDetectorHandle handle, const uint16_t *image,
                int width, int height,
                double **out_x, double **out_y, int *out_count);

/* 扩展检测：正常星 + 饱和星 + 可选参数 */
int sdet_detect_ex(StarDetectorHandle handle, const uint16_t *image,
                   int width, int height,
                   double **out_x, double **out_y,
                   float **out_flux, int **out_saturated, int *out_count,
                   const char **extra_names, int extra_count, float ***out_extras);

/* 调试检测：含 detail/smap/binary 调试图输出 */
int sdet_detect_debug(StarDetectorHandle handle, const uint16_t *image,
                      int width, int height,
                      double **out_x, double **out_y, int *out_count,
                      float **out_detail, float **out_smap, float **out_binary,
                      const char **extra_names, int extra_count, float ***out_extras);
```

### 检测参数 SDetParams

| 字段 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| structureLayers | int | 5 | 保留兼容 |
| hotPixelFilterRadius | int | 1 | 热像素中值滤波半径 |
| iterativeClipSigma | float | 9.0 | sigma-clip 阈值倍数 |
| iterativeMaxRounds | int | 5 | sigma-clip 最大迭代轮数 |
| medianFilterDetail | int | 1 | 是否对细节层做 3×3 中值滤波 |
| maxStars | int | 2000 | 最大输出星点数，0=不限制 |
| fitRadius | int | 6 | PSF 拟合采样区半径，0=自动模式 |
| fwhmClipSigma | float | 3.0 | FWHM 剪裁 sigma 倍数 |
| maxAxisRatio | float | 2.0 | 最大轴比（长轴/短轴） |

## 架构

### 检测流水线

```
输入: uint16图像
  │
  ├─ 正常星检测 ─────────────────────────────────
  ├─ 1. 动态背景分离 -> 细节层 (100px块+20px精细化+积分图+OpenMP)
  ├─ 2. 细节层 >0 二值化
  ├─ 3. 连通域分析
  ├─ 4. 候选预过滤 (像素数≤4 / 包围盒<2×2 / 长宽比>3 -> 丢弃)
  ├─ 5. GSL LM Gaussian 拟合 (halfA 边界搜索初始化, OpenMP 16线程)
  ├─ 6. FWHM 剪裁 (|fwhm-med| > fwhmClipSigma×MAD -> 剔除)
  ├─ 7. 圆度过滤 (min/max < 0.5 -> 拒绝, 参考 Siril reject_star 方法)
  │
  ├─ 饱和星检测 ─────────────────────────────────
  ├─ 8. 半阈值二值化: threshold = (max+min)/2
  ├─ 9. 连通域分析 + 预过滤
  ├─ 10. 圆盘拟合: 加权重心 + 等效半径 r=sqrt(count/π)
  │
  ├─ 合并输出 ───────────────────────────────────
  ├─ 11. 去重: 饱和星与正常星重叠 <2px -> 丢弃饱和星
  ├─ 12. 排序: 饱和星(按r降序)在前 + 正常星(按flux降序)在后
  │
  └─ 输出: x[], y[], flux[], saturated[], extras{}
```

算法实现细节（动态背景分离、候选预过滤、自适应 fitRadius、GSL LM Gaussian 拟合、halfA 初始化、半阈值饱和星检测、去重排序）见 [memory.md](memory.md)。

### 目录结构

```
lib/star_detector/
├── include/star_detector.h    # 公共 C API 头文件
├── src/
│   ├── sdet_api.cpp           # 检测流水线 + GSL LM Gaussian 拟合
│   ├── sdet_detector.cpp      # 连通域分析、去重算法
│   ├── sdet_image.cpp         # 图像处理（动态背景、中值滤波、积分图）
│   ├── sdet_background.cpp    # SExtractor 风格背景估计
│   ├── sdet_log.cpp           # 日志系统
│   └── sdet_detector.h        # 内部头文件
├── python/star_detector.py    # Python 封装（ctypes + 可视化）
├── test_output/example.jpg    # 示例输出图像
├── Makefile
├── README.md
└── memory.md                  # 详细版本迭代与算法实现
```

### 依赖

- **GSL (libgsl)**：trust-region LM 非线性最小二乘拟合（`gsl_multifit_nlinear`）
- **OpenMP**：多线程并行（16线程）
- **MinGW-w64 g++**：C++17 编译器
- **astro_image_io**（可选）：FITS/XISF 图像读取，Python 端使用

## 详细文档

- **[memory.md](memory.md)**：版本迭代历史（V4.27-V4.66）与算法实现细节
  - V4.66：参考 Siril GSL LM 方法 + halfA 边界搜索初始化（3帧全部达标）
  - V4.63：参考 Siril 方法优化（mag_est 排序、reject_star、去 stall_count）
  - V4.62：拟合全部候选 + 不收敛兜底
  - V4.54-V4.57：Gaussian profile 切换 + 候选 Sr/Sc 初始 σ
  - V4.27：参考 Siril reject_star 方法 + maxStars 默认 2000
  - 算法详解：动态背景分离、候选预过滤、自适应 fitRadius、Gaussian 拟合、半阈值饱和星检测、去重排序
- **GitHub 仓库**：https://github.com/fujiaze/Star-Detector-Cpp

## 参考文献

- **[Siril](https://free-astro.org/)**：`PSF.c`（GSL LM Gaussian 拟合、halfA 初始化）、`star_finder.c`（候选排序、reject_star）
- **[SExtractor](https://github.com/astromatic/sextractor)**：网格化背景估计、sigma-clip、连通域分析
- **[GSL](https://www.gnu.org/software/gsl/)**：`gsl_multifit_nlinear` trust-region LM

## 许可

MIT License
