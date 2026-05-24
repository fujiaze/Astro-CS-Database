# Dynamic PSF - Moffat4 PSF 拟合引擎

天文图像中星点的 PSF（Point Spread Function，点扩散函数）拟合引擎，采用 β=4 固定 Moffat 模型与 7 参数 Levenberg-Marquardt 求解器，原生 uint16 输入，OpenMP 16 线程并行批量拟合。

**性能摘要**：C++17 · OpenMP 16 线程 · uint16 原生输入 · 7 参数 LM 求解 · β=4 固定 Moffat · `-O2 -march=native`

GitHub：https://github.com/fujiaze/Dynamic-PSF-Cpp

## 概述

### 功能列表

- **Moffat4 模型**：β=4 固定，7 参数拟合（背景 B、振幅 A、中心 cx/cy、sigma sx/sy、旋转角 θ）
- **Levenberg-Marquardt 求解器**：数值雅可比 + 高斯消元，自适应阻尼因子迭代收敛
- **16bit 原生输入**：直接接收 uint16 图像数据，适配业余天文相机 ADC，避免精度损失
- **批量拟合**：图像数据只传入一次，OpenMP 16 线程并行拟合所有坐标点
- **参数约束**：sx/sy 下限 0.3px 防止发散，背景约束 50%，θ 四方向消歧
- **鲁棒背景估计**：下半中位数 + med+2σ clipping，剔除星点信号残余

### 性能指标

| 指标 | 规格 |
|------|------|
| 并行线程数 | 16（OpenMP，可配置） |
| 输入位深 | 16bit uint16 原生（float32 自动转换） |
| 拟合参数数 | 7（B, A, cx, cy, sx, sy, θ） |
| 拟合半径 | 默认 8px，可配置 |
| 最大迭代次数 | 200 |
| 收敛容差 | 1e-8 |
| 编译优化 | `-O2 -march=native` |
| 链接方式 | 静态链接 libgcc/libstdc++，无运行时依赖 |

## 使用方法

### 编译

```bash
g++ -O2 -march=native -Wall -std=c++17 -fopenmp -shared -o dynamic_psf.dll src/*.cpp -Iinclude -static-libgcc -static-libstdc++ -lm
```

或使用 Makefile：

```bash
make all
```

输出 `dynamic_psf.dll`。

**环境变量**：
- `DYNAMIC_PSF_LOG_LEVEL`：日志级别（0=INFO, 1=DEBUG, 2=WARN, 3=ERROR），默认 INFO

### Python 调用示例

```python
from dynamic_psf import DynamicPSF, DPSFFitParamsPy
import numpy as np

image = ...  # np.ndarray, uint16 或 float32（自动转换）

# 单点拟合
result = DynamicPSF.fit(image, cx=1000.0, cy=800.0)
print(f"FWHM: {result.fwhm_x:.2f} x {result.fwhm_y:.2f} px")

# 批量拟合（OpenMP 16 线程并行）
cx_list = [100.0, 200.0, 300.0]
cy_list = [400.0, 500.0, 600.0]
params = DPSFFitParamsPy(fitRadius=10)
results = DynamicPSF.fit_batch(image, cx_list, cy_list, params=params)
for r in results:
    if r.status == 0:
        print(f"({r.cx:.1f}, {r.cy:.1f}) FWHM={r.fwhm_x:.2f}x{r.fwhm_y:.2f}")
```

### C API

```c
#include "dynamic_psf.h"

// 单点拟合
int dpsf_fit(const uint16_t *image, int width, int height,
             double cx, double cy,
             const DPSFFitParams *params,
             DPSFFitResult *result);

// 批量拟合（OpenMP 并行）
int dpsf_fit_batch(const uint16_t *image, int width, int height,
                   const double *cx_array, const double *cy_array, int count,
                   const DPSFFitParams *params,
                   DPSFFitResult **out_results);

void dpsf_free_results(DPSFFitResult *results);
```

### 数据结构

**DPSFFitParams - 拟合参数**

| 字段 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| fitRadius | int | 8 | 采样区半径，自动构建 [cx-r, cx+r+1] × [cy-r, cy+r+1] 采样区 |
| maxIter | int | 200 | LM 最大迭代次数 |
| tolerance | double | 1e-8 | 收敛容差 |

**DPSFFitResult - 拟合结果**

| 字段 | 类型 | 说明 |
|------|------|------|
| status | int | 0=OK, 1=NoConvergence, 2=InvalidParams, 3=IterLimit |
| B | double | 背景值 |
| A | double | 振幅 |
| cx | double | PSF 中心 x（图像坐标） |
| cy | double | PSF 中心 y（图像坐标） |
| sx | double | sigma_x |
| sy | double | sigma_y |
| theta | double | 旋转角（弧度） |
| fwhm_x | double | FWHM_x = 0.8700 × sx |
| fwhm_y | double | FWHM_y = 0.8700 × sy |
| mad | double | 拟合残差（trimmed MAD） |
| flux | double | 通量估计 |
| eccentricity | double | 偏心率 |

## 架构

### 核心算法

**Moffat4 模型**（β=4 固定）

```
I(x,y) = B + A / (1 + Q)^4
Q = p1*(x-x0)² + 2*p2*(x-x0)(y-y0) + p3*(y-y0)²
p1 = cos²θ/(2sx²) + sin²θ/(2sy²)
p2 = sin2θ/(4sx²) - sin2θ/(4sy²)
p3 = sin²θ/(2sx²) + cos²θ/(2sy²)
```

β=4 固定减少了参数空间维度，同时保持对天文 PSF 的良好近似。

**Levenberg-Marquardt 求解器**

- 数值雅可比（前向差分，h = max(|x|×1e-6, 1e-8)）
- 高斯消元法求解正规方程
- 自适应阻尼因子 λ（成功 ×0.1，失败 ×10）
- 每次迭代后强制约束：sx≥0.3, sy≥0.3, A≥0

**背景估计**

1. 采样区像素排序，取下半部分（低于中位数的像素）
2. 对下半部分做 med+2σ clipping，剔除星点信号残余
3. clipping 后的中位数作为初始背景 bkg0
4. 拟合后约束 |B-bkg0|/max(bkg0,0.01) ≤ 0.5

**θ 四方向消歧**

LM 求解器可能收敛到 θ 的等效方向。拟合完成后，测试 θ、π/2-θ、π/2+θ、π-θ 四个方向，选择 trimmed MAD 最小的作为最终 θ。

### 目录结构

```
dynamic_psf/
├── include/
│   └── dynamic_psf.h        # 公共 C API 头文件（导出函数与数据结构）
├── src/
│   ├── dpsf_psf.cpp/.h      # PSF 模型与 LM 求解器核心
│   ├── dpsf_image.cpp/.h    # 图像采样区构建与背景估计
│   └── dpsf_log.cpp/.h      # 日志输出
├── python/
│   └── dynamic_psf.py       # Python ctypes 封装
├── Makefile                 # 编译脚本
└── README.md
```

### 依赖

- **编译依赖**：MinGW-w64 g++ (C++17)、OpenMP
- **可选依赖**：[Astro-Image-Io](https://github.com/fujiaze/Astro-Image-Io) - FITS/XISF 图像读取（Python 端可选）

## 详细文档链接

- **源码仓库**：https://github.com/fujiaze/Dynamic-PSF-Cpp
- **图像读取依赖**：[Astro-Image-Io](https://github.com/fujiaze/Astro-Image-Io)

**参考文献**（算法参考以下开源项目，核心算法已按 MIT 许可重新实现，代码完全独立）：

- [SExtractor](https://github.com/astromatic/sextractor)（Emmanuel Bertin, CEA/AIM/UParisSaclay）— 背景估计算法，GPL v3
- [PSFEx](https://github.com/astromatic/psfex)（Emmanuel Bertin, IAP/CNRS/UPMC）— PSF 建模方法、chi² 残差计算，GPL v3

**许可**：MIT License
