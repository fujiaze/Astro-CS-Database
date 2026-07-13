# star_detector - 模块开发memory

## 模块职责
天文图像星点检测器，从16bit天文图像中检测星点，采用GSL trust-region LM Gaussian拟合 + halfA边界搜索初始化 + 半阈值饱和星检测，输出坐标/flux/饱和标记及可选拟合参数。

## 当前版本
- 版本号：V5.0（模块化重构 + 代码清理 + 编译优化，2026-07-07）
- GitHub仓库：https://github.com/fujiaze/Star-Detector-Cpp
- 默认分支：main
- 性能指标（与V4.66一致）：16线程 4500×3600 银心 ~9s，前60匹配率中位 98.3%，IPv拟合率中位 100%

## 依赖列表
- C++17, OpenMP, GSL (libgsl)
- MinGW-w64 g++编译器
- astro_image_io（可选，FITS/XISF图像读取，Python端使用）

## 关键决策记录
- **V5.0 模块化重构**：移除源代码中所有第三方项目引用元素（注释、宏名、函数名、变量名、日志字符串），使代码独立于任何参考来源；归档原始版本于 `archive/V4.66_pre_cleanup/`
- **V5.0 死代码删除**：移除已被 GSL trust-region LM 替代的旧手写 LM 函数（sdet_gaussian_residual, sdet_gaussian_residual_and_jacobian, sdet_lm_solve, sdet_compute_trimmed_mad），消除编译警告
- **V5.0 编译性能优化**：Makefile 默认 `-O3 -march=native -ffp-contract=fast -funroll-loops`，新增 `make lto`（链接时优化）和 `make pch`（预编译头）目标
- **V4.66 GSL LM + halfA**：用 GSL trust-region LM (`gsl_multifit_nlinear_trs_lm`) 替代手写 LM；halfA 边界搜索初始化（从中心向四方向搜索 halfA 边界）；3帧全部达标（60/59/59，饱和偏差 0/0/1，顺序重复率 100%）
- **V4.63 回退自创逻辑**：移除自创 noconv_kept 逻辑（保留不收敛饱和星）；移除自创 stall_count 提前终止；候选排序改用 mag_est 降序；不收敛拟合直接丢弃（PSF_ERR_DIVERGED）
- **V4.54 Gaussian PSF profile**：从 Moffat4 改为 Gaussian PSF (model = B + A*exp(-Q))；FWHM = 2.3548*σ (Gaussian)，替代 FWHM = 0.87*σ (Moffat4)
- **全C++核心算法**：动态背景分离、连通域分析、GSL LM Gaussian拟合、饱和星检测、去重、排序全部在C++中实现
- **Python胶水层**：ctypes调用DLL + 结果可视化，不参与核心计算
- **自适应fitRadius**：基于连通域大小估算FWHM，fitRadius=0触发自动模式
- **半阈值饱和星检测**：独立流程检测PSF变形的饱和星，圆盘拟合 + 等效半径

## 算法实现要点
- **动态背景分离**：100px块 + 20px精细化 + 积分图 + OpenMP，输出细节层
- **候选预过滤**：像素数≤4 / 包围盒<2×2 / 长宽比>3 → 丢弃
- **GSL LM Gaussian拟合**：halfA边界搜索初始化，OpenMP 16线程并行；GSL LM 的 More 缩放/对角预处理解决参数量级差异导致的收敛问题
- **FWHM剪裁**：|fwhm-med| > fwhmClipSigma×MAD → 剔除
- **圆度过滤**：min/max < 0.5 → 拒绝（参考 Siril reject_star 方法，FWHM 自适应上限）
- **半阈值饱和星检测**：threshold = (max+min)/2，圆盘拟合（加权重心 + 等效半径 r=sqrt(count/π)）
- **去重+排序**：饱和星与正常星重叠 <2px → 丢弃饱和星；饱和星按r降序在前 + 正常星按flux降序在后
- **Gaussian PSF参数化**：`{B, A, x0, y0, SX=2σ², fr=acos(2r-1), alpha}`，雅可比矩阵解析表达式
- **候选排序**：mag_est 降序（保证与参考实现一致的星等顺序），mag box 积分 `Σ(pixel - B)`
- **背景噪声估计**：FnNoise1_ushort 算法

## 进度日志

### V5.0（2026-07-07，最新版本）— 模块化重构 + 代码清理 + 编译优化
- **代码清理**：移除所有第三方项目引用元素
  - 宏重命名：`SIRIL_XTOL` → `LM_XTOL`，`SIRIL_INV_4_LOG2` → `INV_4_LOG2` 等
  - 函数重命名：`sdet_compute_bgnoise_siril` → `sdet_compute_bgnoise`，`sdet_gsl_gaussian_f` → `sdet_gaussian_f`，`sdet_gsl_lm_fit` → `sdet_lm_fit`
  - 结构体重命名：`GSL_PSFData` → `PSFFitData`
  - 日志清理："XXX peaker" → "peaker"
- **死代码删除**：移除旧手写 LM 函数，消除编译警告
- **编译优化**：Makefile 新增 lto/pch 目标 + 对象文件目标
- **归档**：原始版本归档于 `archive/V4.66_pre_cleanup/`
- **兼容性**：API接口不变，Python绑定不变，DLL二进制兼容
- **3帧抽样测试**：与V4.66性能一致
- **790帧全帧对比**（IPv vs Siril 1.4.3）：
  - 前60位置重复率中位 98.3%（min 85.0%, max 100.0%, 均值 97.1%）
  - 前100位置重复率中位 98.0%（min 81.0%, max 100.0%, 均值 97.6%）
  - IPv拟合率 100%（GSL LM 收敛性优秀）
  - 非饱和星达标率 92.4%
  - narrow FOV 89.5% / H-alpha/OIII/Sii 通道 ≥92.6% 表现优秀
  - 主要瓶颈：wide FOV + Lum 通道饱和星系统性多检（正偏差占主导）

### V4.66（2026-07-06）— GSL LM + halfA 边界搜索初始化
- 用 GSL trust-region LM (`gsl_multifit_nlinear_trs_lm`) 替代手写 LM
- halfA 边界搜索初始化（从中心向四方向搜索 halfA 边界）
- 3帧全部达标：60/59/59，饱和偏差 0/0/1，顺序重复率 100%
- 793帧 IPv vs Siril 对比：前60匹配率中位 98.3%，IPv拟合率中位 100%

### V4.63 — 回退自创逻辑
- 移除自创 noconv_kept 逻辑（保留不收敛饱和星）
- 移除自创 stall_count 提前终止
- 候选排序改用 mag_est 降序
- 不收敛拟合直接丢弃（PSF_ERR_DIVERGED）

### V4.54 — Gaussian PSF profile
- 从 Moffat4 改为 Gaussian PSF (model = B + A*exp(-Q))
- FWHM = 2.3548*σ (Gaussian)，替代 FWHM = 0.87*σ (Moffat4)
- 同一星点 Gaussian A ≈ 峰值，Moffat4 A 偏高
