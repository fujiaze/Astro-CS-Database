# Astro Calibration (C++)

版本：v1.0 C++ OpenMP | 2026-07-12

天文 CCD/CMOS 图像标准校准模块，C++ 实现，OpenMP 多线程加速，Python ctypes 调用。

## GitHub仓库
- 仓库地址：https://github.com/fujiaze/Astro-Calibration-Cpp
- 默认分支：master
- 最新commit：ef6ac09

## 功能

- **主帧生成** (Master Frame Generation)：sigma-clip 离群值剔除 + median/mean 合并
  - Master Bias / Master Dark / Master Flat
- **图像校准** (Image Calibration)：标准 CCD 校准公式 + 暗场优化
  - 无暗场优化：`(Light - Dark) / Flat`
  - 有暗场优化：`(Light - Bias - K*(Dark - Bias)) / Flat`
  - 暗场优化使用黄金分割搜索最优 K 值（背景区域 MAD 最小）
- **坏点修复** (Cosmetic Correction)：Dark/Bias 缺陷图检测 + 连通区域过滤 + 插值修复
  - 从 Master Dark 全局统计检测热像素位置
  - 从 Master Bias 全局统计检测冷像素位置
  - 连通区域大小过滤排除星点（保留 < max_structure_size 的结构）
  - median 5x5 或 bilinear 插值修复

## 目录结构

```
astro_calibration/
├── include/
│   └── astro_calibration.h    C API 头文件
├── src/
│   ├── master_generator.cpp   主帧生成（sigma-clip + median/mean + flat归一化）
│   ├── calibrator.cpp          图像校准（标准公式 + 暗场优化黄金分割搜索）
│   ├── cosmetic_corrector.cpp  坏点修复（Dark/Bias缺陷图 + 连通区域过滤 + 插值）
│   └── ac_api.cpp              C API 导出层（extern "C"）
├── python/
│   └── astro_calibration.py    Python ctypes 封装
├── build.ps1                   编译脚本（MinGW64 g++）
├── astro_calibration.dll       编译产物（静态链接，无外部依赖）
└── README.md
```

## 编译

依赖：MinGW64 g++ (C++17, OpenMP)

```powershell
.\build.ps1
```

或手动编译：

```bash
g++ -O2 -march=native -Wall -std=c++17 -shared -fopenmp \
    -o astro_calibration.dll \
    src/master_generator.cpp src/calibrator.cpp \
    src/cosmetic_corrector.cpp src/ac_api.cpp \
    -Iinclude -static -lm
```

`-static` 静态链接所有运行时库（libgcc/libstdc++/libgomp），DLL 无外部依赖。

## Python 调用

```python
from astro_calibration import AstroCalibration, COMBINE_MEDIAN, METHOD_MEDIAN

cal = AstroCalibration(max_workers=16)

# 生成主帧
cal.generate_master_bias(["bias1.fits", "bias2.fits", ...], "master_bias.fits")
cal.generate_master_dark(["dark1.fits", ...], "master_dark.fits")
cal.generate_master_flat(["flat1.fits", ...], "master_flat.fits", master_bias_path="master_bias.fits")

# 校准单帧
cal.calibrate_frame("light.fits", "calibrated.fits",
                    master_dark="master_dark.fits",
                    master_flat="master_flat.fits")

# 坏点修复
cal.correct_frame("calibrated.fits", "final.fits",
                  master_dark="master_dark.fits",
                  master_bias="master_bias.fits")

# 全链路（内存直通，只写一次FITS）
cal.calibrate_and_correct("light.fits", "final.fits",
                          master_dark="master_dark.fits",
                          master_flat="master_flat.fits",
                          master_bias="master_bias.fits")
```

## C API

```c
// 主帧生成
int ac_generate_master_bias(const float* stack, int n, int w, int h, float* out,
                            float sigma_low, float sigma_high, int max_iter, int combine);
int ac_generate_master_dark(...);  // 同上
int ac_generate_master_flat(const float* stack, int n, int w, int h,
                            const float* bias, float* out, ...);

// 图像校准
int ac_calibrate_frame(const float* light, int w, int h,
                       const float* dark, const float* flat, const float* bias,
                       float* out, int dark_opt, float k, float* actual_k);

// 坏点修复
int ac_correct_frame(const float* data, int w, int h,
                     const float* dark, const float* bias, float* out,
                     float hot_sigma, float cold_sigma,
                     int method, int max_size, int* out_hot, int* out_cold);
```

## 依赖

- [astro_image_io](https://github.com/fujiaze/Astro-Image-IO-C) - FITS/XISF 读写（Python 侧使用）
- MinGW64 g++ (C++17 + OpenMP)
- Python 3.8+, numpy, ctypes

## 设计要点

- C++ 核心算法不含文件 IO，图像数据由 Python 通过 astro_image_io 读写后传入
- OpenMP 16 线程并行，sigma-clip / 校准 / 坏点检测 / 插值均为并行
- `-static` 静态链接，DLL 无外部依赖
- 坏点修复不处理宇宙线（留给叠加时 3sigma rejection）
- Dark/Bias 主帧保留坏点（校准时扣除），仅用它们定位坏点位置

## 详细文档

- [标准校准流程文档](../calibration/CALIBRATION_PROCESS.md)
