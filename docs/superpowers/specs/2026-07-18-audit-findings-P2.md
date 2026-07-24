# 代码审计问题清单 - P2（Medium，建议修复）

> 配套总报告：`2026-07-18-code-audit-report.md`
> 本文档包含 9 模块所有 Medium（54 项）问题
> 每条问题含【问题定位】（AI 看）+ 【问题描述】（用户看）+ 【批复】（用户填写）
> 术语表见 P0+P1 文档顶部

---

## 目录

- [B1 astro_image_io (5 项)](#b1-astro_image_io)
- [B2 calibration (5 项)](#b2-calibration)
- [B3 plate_solve (4 项)](#b3-plate_solve)
- [B4 dynamic_psf (7 项)](#b4-dynamic_psf)
- [B5 photometric_calib (6 项)](#b5-photometric_calib)
- [B6 snr_estimator (4 项)](#b6-snr_estimator)
- [B7 healpix_drizzle (4 项)](#b7-healpix_drizzle)
- [B8 healpix_stack (9 项)](#b8-healpix_stack)
- [B9 orchestrator (10 项)](#b9-orchestrator)

---

## B1 astro_image_io

### B1-M-1: FITS 维度顺序处理可能互换宽高

**问题定位**
- 文件：`lib/astro_image_io/src/aio_fits.cpp`
- 涉及：NAXIS1（宽度）和 NAXIS2（高度）的读取顺序，FITS 标准中 NAXIS1 是行宽（X方向），NAXIS2 是列高（Y方向）

**问题描述**
FITS 文件格式规定 NAXIS1 表示图像宽度（X 方向像素数），NAXIS2 表示高度（Y 方向像素数）。当前代码在读取时可能将两者互换处理，导致图像旋转 90 度。这会影响后续星点检测（检测器假设图像方向）、WCS 求解（CRPIX 中心点位置错位）等所有下游处理。

**批复**
- [ ] 同意修复
- [ ] 刻意为之
- [ ] 暂不修复
- 备注：

---

### B1-M-2: 缓存加载失败时已分配内存泄漏

**问题定位**
- 文件：`lib/astro_image_io/src/aio_compressor.cpp` 或缓存相关代码
- 涉及：load_cache 函数失败时未释放已分配的缓冲区

**问题描述**
图像 I/O 模块有缓存机制（缓存常用数据如压缩表、星表索引等加速重复访问）。当缓存加载失败时（如磁盘错误、文件损坏），已分配的内存缓冲区没有被释放，造成内存泄漏。长时间运行的批量处理任务会逐渐耗尽内存。

**批复**
- [ ] 同意修复
- [ ] 刻意为之
- [ ] 暂不修复
- 备注：

---

### B1-M-3: 编排器未传递观测元数据（DATE-OBS/JD-OBS/BUNIT/LONPOLE/LATPOLE）

**问题定位**
- 文件：`lib/astro_image_io/src/aio_fits.cpp` + 编排器调用
- 涉及：FITS 头中观测时刻、数据单位、黄道极等关键字未提取到管线帧

**问题描述**
读取 FITS 时未提取观测时刻（DATE-OBS）、儒略日（JD-OBS）、数据单位（BUNIT，如 ADU 或 electron）、黄道极（LONPOLE/LATPOLE，用于坐标系转换）。这些字段对测光定标（曝光时刻做大气消光校正）、单位换算、坐标系转换都是必需的。下游模块要么重新读 FITS 头（重复 I/O），要么无法做精确校正。（与 B1-H-2 同源，更细化）

**批复**
- [ ] 同意修复
- [ ] 刻意为之
- [ ] 暂不修复
- 备注：

---

### B1-M-4: 元数据构建未提取 SIP 关键字

**问题定位**
- 文件：`lib/astro_image_io/src/aio_fits.cpp` `build_metadata` 函数
- 涉及：未提取 SIP 多项式关键字（A_ORDER、A_i_j、B_ORDER、B_i_j、AP_ORDER、BP_ORDER 等）

**问题描述**
读取已有 WCS+SIP 信息的 FITS 文件时，build_metadata 函数未提取 SIP 多项式关键字。这意味着如果输入 FITS 已有 WCS（如来自其他解析工具），SIP 畸变信息会丢失，下游重新使用 WCS 时只有 CD 矩阵而无 SIP 修正，畸变大的图像会解析不准。

**批复**
- [ ] 同意修复
- [ ] 刻意为之
- [ ] 暂不修复
- 备注：

---

### B1-M-5: 多资源释放顺序不明确

**问题定位**
- 文件：`lib/astro_image_io/src/aio_pipeline_engine.cpp` 等
- 涉及：文件句柄、内存缓冲区、压缩上下文的释放顺序无规范

**问题描述**
模块在销毁时释放多种资源（文件句柄、内存、压缩上下文等），但释放顺序不明确。某些资源有依赖关系（如压缩上下文依赖文件句柄），乱序释放可能导致访问已释放资源。应明确"后申请先释放"的逆序规则。

**批复**
- [ ] 同意修复
- [ ] 刻意为之
- [ ] 暂不修复
- 备注：

---

## B2 calibration

### B2-M-1: 多处配置参数硬编码

**问题定位**
- 文件：`lib/calibration/src/calibrator.cpp` 等
- 涉及：如 sigma_clip 阈值、坏点识别阈值等硬编码

**问题描述**
校准模块的多个算法参数（如 sigma-clip 阈值、坏点识别阈值）硬编码在代码中，无法通过配置调整。不同相机、不同曝光参数下最优阈值不同，硬编码限制了模块的适用性。

**批复**
- [ ] 同意修复
- [ ] 刻意为之
- [ ] 暂不修复
- 备注：

---

### B2-M-2: 关键步骤日志缺失

**问题定位**
- 文件：`lib/calibration/src/calibrator.cpp` 等
- 涉及：校准各步骤（减暗场、除平场、减偏置）无详细日志

**问题描述**
校准的各个步骤（减暗场、除平场、减偏置、坏点修复）没有详细日志输出。出问题时无法定位是哪一步异常，只能从最终结果推测。应当每步输出"输入均值、输出均值、剔除像素数"等关键统计。

**批复**
- [ ] 同意修复
- [ ] 刻意为之
- [ ] 暂不修复
- 备注：

---

### B2-M-3: 主帧文件路径未持久化

**问题定位**
- 文件：`lib/calibration/src/ac_api.cpp`
- 涉及：使用的主暗场/平场/偏置帧路径未记录到任何持久化输出

**问题描述**
校准时使用了哪些主帧文件（如 master_dark_20250702.fits）没有记录到任何输出。事后追溯"这次校准用了哪天的主帧"只能查日志或代码调用。应当把主帧路径写入校准统计块（cal_stats）持久化。

**批复**
- [ ] 同意修复
- [ ] 刻意为之
- [ ] 暂不修复
- 备注：

---

### B2-M-4: 错误处理不完整

**问题定位**
- 文件：`lib/calibration/src/calibrator.cpp`
- 涉及：主帧尺寸不匹配、数据类型不符等情况未明确报错

**问题描述**
当主帧尺寸与目标图像不匹配（如主帧是 4096×4096 但目标图是 4500×3600），或数据类型不符（主帧是 float 但目标是 uint16），代码未明确报错，可能产生错误结果或崩溃。

**批复**
- [ ] 同意修复
- [ ] 刻意为之
- [ ] 暂不修复
- 备注：

---

### B2-M-5: OpenMP 线程数管理混乱

**问题定位**
- 文件：`lib/calibration/src/calibrator.cpp`
- 涉及：与 B2-H-1/H-2 相关，硬编码 16 + 外部设置无效

**问题描述**
（与 B2-H-1 / B2-H-2 相关）模块的 OpenMP 线程数管理混乱——有外部 API 但不生效，内部硬编码 16 线程。多任务并行时所有任务都强行 16 线程，导致 CPU 争抢。

**批复**
- [ ] 同意修复
- [ ] 刻意为之
- [ ] 暂不修复
- 备注：

---

## B3 plate_solve

### B3-M-1: 主流程参数硬编码

**问题定位**
- 文件：`lib/plate_solve/cpp/ipv/src/ipv_entry.cpp` 等
- 涉及：RANSAC 迭代次数、抽样数、阈值等硬编码

**问题描述**
解析模块的多个算法参数（RANSAC 迭代次数、抽样数、阈值）硬编码，无法通过配置调整。不同图像（宽视场 vs 窄视场、密集星场 vs 稀疏星场）最优参数不同。

**批复**
- [ ] 同意修复
- [ ] 刻意为之
- [ ] 暂不修复
- 备注：

---

### B3-M-2: 错误恢复机制不完善

**问题定位**
- 文件：`lib/plate_solve/cpp/ipv/src/ipv_entry.cpp`
- 涉及：解析失败时无降级路径（如换算法、降参数重试）

**问题描述**
当主算法（三角匹配）失败时，没有自动降级到其他算法或调整参数重试的机制。直接返回失败，用户只能手动改参数重跑。应当实现"主算法失败 → 降级到备用算法 → 调整参数重试"的恢复链。

**批复**
- [ ] 同意修复
- [ ] 刻意为之
- [ ] 暂不修复
- 备注：

---

### B3-M-3: 日志输出不完善

**问题定位**
- 文件：`lib/plate_solve/cpp/ipv/src/ipv_*.cpp` 多处
- 涉及：关键步骤无详细日志（如 RANSAC 每轮迭代、内点数变化）

**问题描述**
解析过程的关键步骤（RANSAC 每轮迭代的内点数、Umeyama 拟合的残差、SIP 阶数选择依据）没有详细日志。解析失败时无法回溯具体在哪一步出问题。

**批复**
- [ ] 同意修复
- [ ] 刻意为之
- [ ] 暂不修复
- 备注：

---

### B3-M-4: 性能瓶颈未优化

**问题定位**
- 文件：`lib/plate_solve/cpp/ipv/src/ipv_kvector.cpp` 等
- 涉及：K-vector 查询、三角匹配等未充分优化

**问题描述**
某些关键路径（如 K-vector 范围查询、三角匹配的候选生成）未充分优化，存在性能瓶颈。当候选星数量大时（如密集星场），解析耗时显著增加。

**批复**
- [ ] 同意修复
- [ ] 刻意为之
- [ ] 暂不修复
- 备注：

---

## B4 dynamic_psf

### B4-M-1: 多处配置参数硬编码

**问题定位**
- 文件：`lib/dynamic_psf/src/dpsf_psf.cpp` 等
- 涉及：拟合参数（如 patch 大小、最大迭代、收敛阈值）硬编码

**问题描述**
PSF 拟合的多个参数（patch 大小、最大迭代次数、收敛阈值、初始 FWHM 估计）硬编码，无法通过配置调整。不同相机的 PSF 形态差异大，硬编码参数不一定最优。

**批复**
- [ ] 同意修复
- [ ] 刻意为之
- [ ] 暂不修复
- 备注：

---

### B4-M-2: 关键步骤日志缺失

**问题定位**
- 文件：`lib/dynamic_psf/src/dpsf_psf.cpp`
- 涉及：每颗星的拟合过程无详细日志（如拟合残差、迭代次数、收敛状态）

**问题描述**
PSF 拟合的每颗星过程（拟合残差、迭代次数、收敛状态、失败原因）没有详细日志。当某颗星拟合失败时，无法知道具体原因（如初始值偏离、迭代不收敛、数据噪声大）。

**批复**
- [ ] 同意修复
- [ ] 刻意为之
- [ ] 暂不修复
- 备注：

---

### B4-M-3: 错误处理不完整

**问题定位**
- 文件：`lib/dynamic_psf/src/dpsf_psf.cpp` / `dpsf_image.cpp`
- 涉及：输入数据异常（如图像全 0、星点数 0）未明确报错

**问题描述**
当输入数据异常（如图像全 0、星点表为空、星点坐标超出图像范围）时，代码未明确报错，可能产生 NaN 或崩溃。

**批复**
- [ ] 同意修复
- [ ] 刻意为之
- [ ] 暂不修复
- 备注：

---

### B4-M-4: 资源管理不严格

**问题定位**
- 文件：`lib/dynamic_psf/src/dpsf_psf.cpp`
- 涉及：OpenMP 线程局部变量、临时缓冲区管理

**问题描述**
PSF 拟合用 OpenMP 并行，但线程局部变量和临时缓冲区的管理不严格。某些边界情况（如线程数动态变化、缓冲区复用）可能产生竞态或内存问题。

**批复**
- [ ] 同意修复
- [ ] 刻意为之
- [ ] 暂不修复
- 备注：

---

### B4-M-5: OpenMP 线程数硬编码

**问题定位**
- 文件：`lib/dynamic_psf/src/dpsf_psf.cpp`
- 涉及：`omp_set_num_threads(16)` 硬编码

**问题描述**
PSF 拟合模块硬编码使用 16 个 OpenMP 线程，无视外部配置。与 B2-H-1 同类问题。

**批复**
- [ ] 同意修复
- [ ] 刻意为之
- [ ] 暂不修复
- 备注：

---

### B4-M-6: 性能优化未到位

**问题定位**
- 文件：`lib/dynamic_psf/src/dpsf_psf.cpp`
- 涉及：Moffat 函数计算用 std::pow 性能差

**问题描述**
Moffat PSF 函数计算中用到 `std::pow`（通用幂运算），但 Moffat4 的 β=4 是整数次幂，可以用连乘替代（x^4 = x*x*x*x），快 5-10 倍。在大批量拟合（2000 颗星 × 100 次迭代）时性能差异显著。

**批复**
- [ ] 同意修复
- [ ] 刻意为之
- [ ] 暂不修复
- 备注：

---

### B4-M-7: 命名不一致

**问题定位**
- 文件：`lib/dynamic_psf/src/*.cpp` 多处
- 涉及：变量命名风格混用（camelCase / snake_case）

**问题描述**
模块内变量命名风格不统一（有的用驼峰命名 imgCx，有的用下划线 img_cx），影响可读性。

**批复**
- [ ] 同意修复
- [ ] 刻意为之
- [ ] 暂不修复
- 备注：

---

## B5 photometric_calib

### B5-M-1: 关键步骤日志缺失

**问题定位**
- 文件：`lib/photometric_calib/cpp/src/pc_api.cpp` 等
- 涉及：F_syn 计算、IRLS 迭代、scale 求解过程无详细日志

**问题描述**
测光定标的关键步骤（F_syn 积分过程、IRLS 每轮迭代的权重变化、scale 求解的残差收敛）没有详细日志。出问题时无法定位是 F_syn 计算错还是 scale 拟合错。

**批复**
- [ ] 同意修复
- [ ] 刻意为之
- [ ] 暂不修复
- 备注：

---

### B5-M-2: 配置参数硬编码

**问题定位**
- 文件：`lib/photometric_calib/cpp/src/pc_api.cpp` 等
- 涉及：IRLS 迭代次数、Tukey 阈值、匹配半径等硬编码

**问题描述**
测光定标的多个参数（IRLS 迭代次数、Tukey 阈值、星匹配半径）硬编码，无法通过配置调整。

**批复**
- [ ] 同意修复
- [ ] 刻意为之
- [ ] 暂不修复
- 备注：

---

### B5-M-3: 错误处理不完整

**问题定位**
- 文件：`lib/photometric_calib/cpp/src/pc_api.cpp`
- 涉及：Gaia 查询返回 0 颗星、PSF 拟合全失败时未明确报错

**问题描述**
当 Gaia 查询返回 0 颗星（视场内无 Gaia 数据）或 PSF 拟合全部失败（图像质量极差）时，代码未明确报错，可能产生 NaN scale 或崩溃。

**批复**
- [ ] 同意修复
- [ ] 刻意为之
- [ ] 暂不修复
- 备注：

---

### B5-M-4: 性能优化未到位

**问题定位**
- 文件：`lib/photometric_calib/cpp/src/spectrum_integrator.cpp`
- 涉及：F_syn 积分未并行化

**问题描述**
F_syn 积分（每颗 Gaia 星都要算一次，可能上千次）未并行化，单线程处理较慢。每颗星的积分相互独立，适合 OpenMP 并行。

**批复**
- [ ] 同意修复
- [ ] 刻意为之
- [ ] 暂不修复
- 备注：

---

### B5-M-5: 命名不一致

**问题定位**
- 文件：`lib/photometric_calib/cpp/src/*.cpp`
- 涉及：变量命名风格混用

**问题描述**
模块内变量命名风格不统一，影响可读性。

**批复**
- [ ] 同意修复
- [ ] 刻意为之
- [ ] 暂不修复
- 备注：

---

### B5-M-6: 注释不全

**问题定位**
- 文件：`lib/photometric_calib/cpp/src/spectrum_integrator.cpp`
- 涉及：F_syn 公式来源、IRLS 数学推导无注释

**问题描述**
F_syn 公式（F_syn = ∫S(λ)·T(λ)·Q(λ)dλ）的物理含义、IRLS+Tukey 的数学推导在代码中无注释，新开发者难以理解算法原理。

**批复**
- [ ] 同意修复
- [ ] 刻意为之
- [ ] 暂不修复
- 备注：

---

## B6 snr_estimator

### B6-M-1: 日志目录空置（仅 .gitkeep）

**问题定位**
- 文件：`lib/snr_estimator/logs/`（仅含 .gitkeep）
- 涉及：模块内 20+ 处 `fprintf(stderr)` 未落盘

**问题描述**
SNR 估算模块的日志目录是空的（只有 .gitkeep 占位文件），但模块内有 20 多处用 fprintf(stderr) 输出日志，没有走模块日志系统。日志无法落盘归档，无法按级别过滤，无法在批量处理时分析。

**批复**
- [ ] 同意修复
- [ ] 刻意为之
- [ ] 暂不修复
- 备注：

---

### B6-M-2: SNR 模型提取缺少 WCS 有效性校验

**问题定位**
- 文件：`lib/snr_estimator/cpp/src/snr_estimator.cpp`
- 涉及：`snr_extract_model` 函数未校验 WCS 是否有效（CD 矩阵是否非零、CRVAL 是否合理）

**问题描述**
SNR 模型提取函数从图像中提取稀疏控制点，需要用 WCS 计算控制点的天球坐标。但函数未校验 WCS 是否有效（如 CD 矩阵是否非零、CRVAL 是否在合理范围）。如果 WCS 无效（如未解析的图像），会产生 NaN 坐标污染后续 IDW 插值。

**批复**
- [ ] 同意修复
- [ ] 刻意为之
- [ ] 暂不修复
- 备注：

---

### B6-M-3: 配置参数硬编码

**问题定位**
- 文件：`lib/snr_estimator/cpp/src/snr_estimator.cpp`
- 涉及：控制点采样密度、IDW 幂次等硬编码

**问题描述**
SNR 估算的参数（控制点采样密度、IDW 插值幂次、稀疏化阈值）硬编码，无法通过配置调整。

**批复**
- [ ] 同意修复
- [ ] 刻意为之
- [ ] 暂不修复
- 备注：

---

### B6-M-4: 性能优化未到位

**问题定位**
- 文件：`lib/snr_estimator/cpp/src/snr_estimator.cpp`
- 涉及：IDW 插值未用 KD-tree 加速

**问题描述**
SNR 估算的 IDW 插值（每个查询点都要遍历所有控制点）未用 KD-tree 加速。当控制点数量大（如 1000 个）或查询点多（如全图像素）时，O(N×M) 复杂度较慢。

**批复**
- [ ] 同意修复
- [ ] 刻意为之
- [ ] 暂不修复
- 备注：

---

## B7 healpix_drizzle

### B7-M-1: OpenMP 线程数硬编码 16

**问题定位**
- 文件：`lib/healpix_db/healpix_drizzle/drizzle_engine.cpp`
- 涉及：`omp_set_num_threads(16)` 硬编码

**问题描述**
Drizzle 引擎硬编码使用 16 个 OpenMP 线程，无视外部配置。与 B2-H-1 / B4-M-5 同类问题。

**批复**
- [ ] 同意修复
- [ ] 刻意为之
- [ ] 暂不修复
- 备注：

---

### B7-M-2: SIP 多项式计算用 std::pow 性能差

**问题定位**
- 文件：`lib/healpix_db/healpix_drizzle/wcs_sip.cpp`
- 涉及：SIP 多项式 `x' = Σ A_ij * x^i * y^j` 用 std::pow 计算

**问题描述**
SIP 多项式计算用 `std::pow` 计算幂次（如 x^2、y^3），但 SIP 多项式的幂次都是小整数（≤4），可以用连乘替代。每像素都要算一次 SIP，图像 4096×4096 有 1600 万像素，性能差异显著。

**批复**
- [ ] 同意修复
- [ ] 刻意为之
- [ ] 暂不修复
- 备注：

---

### B7-M-3: FITS 数据读取不完整时只警告不报错

**问题定位**
- 文件：`lib/healpix_db/healpix_drizzle/fits_reader.cpp`
- 涉及：FITS 数据读取中途截断时只 LOG_WARN 继续处理

**问题描述**
读取 FITS 数据时如果文件被截断（如磁盘错误导致数据不完整），代码只输出警告但继续用不完整数据处理。这会导致后续 Drizzle 产生错误结果但无报错。应当报错并终止处理。

**批复**
- [ ] 同意修复
- [ ] 刻意为之
- [ ] 暂不修复
- 备注：

---

### B7-M-4: 哈希表 reserve 硬编码 4M 桶

**问题定位**
- 文件：`lib/healpix_db/healpix_drizzle/drizzle_engine.cpp`
- 涉及：HEALPix 像素哈希表 `reserve(4*1024*1024)` 硬编码

**问题描述**
Drizzle 引擎用哈希表存储 HEALPix 像素索引到数据的映射，预分配 4M 桶（约 400 万）。这个数字是硬编码的，对于小图像（如 1024×1024）浪费内存（4M 桶 × 16 字节 = 64MB），对于大图像（如 8192×8192）可能不够导致 rehash。应当根据图像尺寸动态计算。

**批复**
- [ ] 同意修复
- [ ] 刻意为之
- [ ] 暂不修复
- 备注：

---

## B8 healpix_stack

### B8-M-1: IDW 权重中距离用度而非弧度，与算法文档不一致

**问题定位**
- 文件：`lib/healpix_db/healpix_stack/gradient/snr_evaluator.cpp:222-232, 283-293`
- 涉及：`greatCircleDistanceDeg` 返回度，idw_power=2 时权重数值与弧度差 3283 倍

**问题描述**
SNR 评估器用 IDW（反距离加权）插值，权重为 1/距离的幂次。距离用"度"为单位（典型 0.01~1.0 度），但算法文档说用"弧度"（度÷57.3）。度与弧度数值差 57 倍，幂次 2 次时差 3283 倍。数值结果不影响（分子分母同时缩放），但与文档描述不一致。

**批复**
- [ ] 同意修复
- [ ] 刻意为之
- [ ] 暂不修复
- 备注：

---

### B8-M-2: 子叶块索引计算存在 bug（nside<64 时不正确）

**问题定位**
- 文件：`lib/healpix_db/healpix_stack/gradient/gradient_sampler.cpp:550-556`
- 涉及：`if (nside_i < 64) leaf_ipix_64 = cp_ipix << shift_64;` 当 shift_64=0 时无效

**问题描述**
计算 HEALPix 子叶块索引（nside=64 的父块）时，当原始 nside 小于 64（如 nside=32），代码用左移运算扩展到 nside=64 的索引，但左移位数 shift_64 此时为 0（循环未执行），导致索引未正确扩展。当前 nside 最小值 64 保证不触发此 bug，但代码隐患存在。

**批复**
- [ ] 同意修复
- [ ] 刻意为之
- [ ] 暂不修复
- 备注：

---

### B8-M-3: 降采样函数名暗示 Morton 位运算，实际用空间网格分组

**问题定位**
- 文件：`lib/healpix_db/healpix_stack/gradient/gradient_sampler.cpp:184-292`
- 涉及：`mortonDownsample` 函数名暗示 NESTED Morton 位运算，实际用 ra/dec 量化分组

**问题描述**
函数名 `mortonDownsample` 暗示用 Morton 编码（HEALPix NESTED 排序的位运算）做降采样，但实际实现是空间网格分组（把 ra/dec 量化到固定网格）。注释承认"由于 SampleRow 只有 ra/dec（非 ipix），改用空间网格分组"。这导致跨帧降采样时控制点分组可能不一致。

**批复**
- [ ] 同意修复
- [ ] 刻意为之
- [ ] 暂不修复
- 备注：

---

### B8-M-4: OpenMP 并行未显式控制线程数

**问题定位**
- 文件：
  - `lib/healpix_db/healpix_stack/gradient/spherical_spline.cpp:265`（`#pragma omp parallel for` 无 schedule）
  - `lib/healpix_db/healpix_stack/gradient/snr_evaluator.cpp:259`（无 num_threads）

**问题描述**
球面样条和 SNR 评估器的 OpenMP 并行循环未显式指定线程数，依赖环境变量 OMP_NUM_THREADS。生产环境与开发环境行为可能不一致。项目规范要求 16 线程。

**批复**
- [ ] 同意修复
- [ ] 刻意为之
- [ ] 暂不修复
- 备注：

---

### B8-M-5: HEALPix 像素数计算未强制 int64 防 npface 溢出

**问题定位**
- 文件：`lib/healpix_db/healpix_stack/healpix_core.cpp`
- 涉及：`12*nside*nside` 计算时若 nside 为 int32，nside=16384 时溢出

**问题描述**
HEALPix 总像素数 = 12×nside²。当 nside=16384 时，结果是 32 亿，超出 32 位有符号整数范围（21 亿）。代码未强制用 64 位整数计算，nside ≥ 16384 时会溢出产生错误像素数。项目硬约束要求"nside=8192 需 uint64_t"。

**批复**
- [ ] 同意修复
- [ ] 刻意为之
- [ ] 暂不修复
- 备注：

---

### B8-M-6: fact2 系数未在 healpix_stack 中实现

**问题定位**
- 文件：`lib/healpix_db/healpix_stack/healpix_core.cpp`
- 涉及：Grep `fact2|npface` 在 healpix_stack 模块无匹配

**问题描述**
项目硬约束要求"fact2 系数必须为 1.0/(3*npface)，非 4.0/(3*npface)"，用于 pix2ang_nest 往返一致性。但 healpix_stack 模块未发现 fact2 的使用。需确认此模块是否需要 fact2，若需要应正确实现。

**批复**
- [ ] 同意修复
- [ ] 刻意为之（此模块不需要 fact2）
- [ ] 暂不修复
- 备注：

---

### B8-M-7: Gaia 客户端创建失败即返回，不支持跳过星拒绝退化路径

**问题定位**
- 文件：`lib/healpix_db/healpix_stack/gradient/gradient_sampler.cpp:322-327`
- 涉及：`gaia_client_create_ex` 失败直接返回错误码 2

**问题描述**
梯度采样器在创建 Gaia 客户端失败时（如 Gaia 数据库不可用）直接返回错误，没有"跳过星拒绝步骤，仅用像素值采样"的退化路径。当用户无 Gaia 数据库时，stage2 直接失败。

**批复**
- [ ] 同意修复
- [ ] 刻意为之
- [ ] 暂不修复
- 备注：

---

### B8-M-8: sigma_clip_method 配置项未做白名单校验

**问题定位**
- 文件：`lib/orchestrator/cpp/src/orchestrator.cpp:2410-2434`
- 涉及：从 config_json 解析 `sigma_clip_method` 时未校验取值

**问题描述**
编排器从配置读取 sigma_clip_method 时，未校验取值是否为 {"standard", "winsorized"} 之一。若用户写"Winsorized"（大写）或拼错，会静默降级为普通 sigma-clip，无任何告警。

**批复**
- [ ] 同意修复
- [ ] 刻意为之
- [ ] 暂不修复
- 备注：

---

### B8-M-9: JSON 头不压缩，与读取端解压逻辑不对称

**问题定位**
- 文件：
  - `lib/healpix_db/healpix_stack/ahps_writer.cpp:332-333`（`headerCompSize = 0`）
  - `lib/healpix_db/healpix_stack/ahps_reader.cpp:215-237`（保留 ZSTD 解压分支）

**问题描述**
.hiss 文件写入时 JSON 头不压缩（headerCompSize=0），但读取端保留了解压分支（死代码）。无功能 bug，但存在死代码和大元数据时文件体积优化空间。

**批复**
- [ ] 同意修复
- [ ] 刻意为之
- [ ] 暂不修复
- 备注：

---

## B9 orchestrator

### B9-M-1: NSIDE 自适应公式与 HEALPix 标准不符

**问题定位**
- 文件：`lib/orchestrator/cpp/src/orchestrator.cpp:153-174`
- 涉及：注释 "HEALPix 像素分辨率 (arcsec) ≈ 3600*60*sqrt(3) / (3*nside) = 1186.18 / nside"

**问题描述**
NSIDE 自适应计算的公式注释与 HEALPix 标准不符。注释说 "1186.18/nside" 是像素分辨率，但 HEALPix 标准的像素边长是 58.6/nside 度 ≈ 210960/nside 角秒，1186.18 实际是面积等效边长（差约 56 倍）。当前公式计算的 NSIDE 偏小，影响 .hiss 文件分辨率。

**批复**
- [ ] 同意修复
- [ ] 刻意为之
- [ ] 暂不修复
- 备注：

---

### B9-M-2: 日志目录硬编码，各 stage 无独立日志

**问题定位**
- 文件：
  - `lib/orchestrator/cpp/src/orchestrator.cpp:247`（默认 "lib/orchestrator/logs"）
  - `lib/orchestrator/cpp/src/orchestrator.cpp:1322`（PLATESOLVE 写到 lib/plate_solve/logs）

**问题描述**
项目规范要求"每个模块建立日志目录"，但编排器把所有 stage 的日志写到单一文件 `lib/orchestrator/logs/orchestrator_YYYY-MM-DD.log`，混杂 7+2 个 stage 信息，难以单独分析某个 stage。仅 PLATESOLVE 内部日志单独写到 lib/plate_solve/logs。

**批复**
- [ ] 同意修复
- [ ] 刻意为之
- [ ] 暂不修复
- 备注：

---

### B9-M-3: 星点检测器默认参数硬编码

**问题定位**
- 文件：`lib/orchestrator/cpp/src/orchestrator.cpp:1108-1119`
- 涉及：SDetParams 各字段硬编码（structureLayers=5, maxStars=2000, fitRadius=0 等）

**问题描述**
星点检测器的所有参数（结构层数、最大星数、拟合半径、FWHM 裁剪 sigma 等）硬编码，无法通过 stage1_config.json 调整。不同相机、不同曝光参数下最优值不同。

**批复**
- [ ] 同意修复
- [ ] 刻意为之
- [ ] 暂不修复
- 备注：

---

### B9-M-4: 检查点"已完成"判定阈值硬编码为 4（旧版 5 阶段）

**问题定位**
- 文件：`lib/orchestrator/cpp/src/checkpoint.cpp:730`
- 涉及：`if (data.current_stage_id >= 4) { data.fully_completed = true; }`

**问题描述**
检查点模块判定"管线是否全部完成"的阈值硬编码为 4，基于旧版 5 阶段（0-4）。但新版是 9 节点（stage1 0-6，stage2 7-8）。阈值 4 对 stage1 来说会过早判定完成（实际需到 6），对 stage2 来说永远无法达到。

**批复**
- [ ] 同意修复
- [ ] 刻意为之
- [ ] 暂不修复
- 备注：

---

### B9-M-5: run_single 旧版路径已废弃但仍保留

**问题定位**
- 文件：`lib/orchestrator/cpp/src/orchestrator.cpp:333-462`
- 涉及：`run_single` 调用 5 阶段（CALIBRATE/PLATESOLVE/PSF/PHOTOMETRIC/DRIZZLE）

**问题描述**
旧版 run_single 路径基于 5 阶段枚举，与新版 9 节点两段流水线架构不符。CLI 仍提供 `orchestrator run` 命令调用它。维护两套路径成本高，新开发者易混淆。

**批复**
- [ ] 同意修复（删除 run_single 和 `run` 子命令）
- [ ] 同意修复（保留但内部转发到 run_stage1）
- [ ] 刻意为之
- [ ] 暂不修复
- 备注：

---

### B9-M-6: 新旧管线阶段枚举并存，StageTiming 用旧枚举

**问题定位**
- 文件：
  - `lib/orchestrator/cpp/include/orchestrator.h:37-43`（旧版 5 阶段）
  - `lib/orchestrator/cpp/include/orchestrator.h:53-65`（新版 9 节点）
  - `lib/orchestrator/cpp/include/orchestrator.h:79`（StageTiming 用旧枚举）
  - `lib/orchestrator/cpp/src/orchestrator.cpp:2566`（`st.stage = PipelineStage::CALIBRATE;`）

**问题描述**
新旧两套阶段枚举并存，StageTiming 结构体用旧枚举但 stage1/stage2 用新枚举调度。导致 timings 数组中 stage 字段全为 CALIBRATE/STACK（旧枚举的硬编码值），无法区分 READ_FITS/PSF/SNR 等。

**批复**
- [ ] 同意修复（StageTiming 改用 PipelineStageV2）
- [ ] 刻意为之
- [ ] 暂不修复
- 备注：

---

### B9-M-7: stage1/stage2 未集成检查点（无断点续传）

**问题定位**
- 文件：
  - `lib/orchestrator/cpp/src/orchestrator.cpp:2482-2605`（run_stage1 无 checkpoint_mgr_ 调用）
  - `lib/orchestrator/cpp/src/orchestrator.cpp:2611-2718`（run_stage2 无 checkpoint_mgr_ 调用）

**问题描述**
只有旧版 run_single 集成了检查点。新版 stage1/stage2 串行执行 7+2 个 stage，中途失败无法从断点恢复，必须重新从头执行（重读 FITS、重解析 WCS，耗时）。

**批复**
- [ ] 同意修复
- [ ] 刻意为之
- [ ] 暂不修复
- 备注：

---

### B9-M-8: Gaia 锥形查询半径 1.2 倍余量可能不足

**问题定位**
- 文件：`lib/orchestrator/cpp/src/orchestrator.cpp:1502-1506`
- 涉及：`fov_radius_deg = pixel_scale_deg * sqrt(w^2 + h^2) / 2.0 * 1.2;`

**问题描述**
Gaia 查询半径用图像视场对角线的 0.5 倍再乘 1.2（20% 余量）。对于有 SIP 畸变的宽视场图像，边缘星可能因 SIP 多项式畸变落在 1.2 倍半径之外。硬约束要求 0.5 倍视场对角线（无余量），当前是 0.6 倍，比硬约束大 20%。

**批复**
- [ ] 同意修复
- [ ] 刻意为之
- [ ] 暂不修复
- 备注：

---

### B9-M-9: PHOTOMETRIC FOV 半径钳位 [1.0, 10.0] 度，与 PLATESOLVE 上限 30 度不一致

**问题定位**
- 文件：
  - `lib/orchestrator/cpp/src/orchestrator.cpp:1872-1875`（PHOTOMETRIC: [1.0, 10.0]）
  - `lib/orchestrator/cpp/src/orchestrator.cpp:1508`（PLATESOLVE: 上限 30 度）

**问题描述**
PHOTOMETRIC 阶段 FOV 半径钳位到 [1.0, 10.0] 度，PLATESOLVE 阶段判定 (0, 30) 度才查询。两个 stage 上限不一致（30 vs 10），宽视场图像（如 20 度）在 PHOTOMETRIC 阶段查询范围被强制限制到 10 度，漏掉部分 Gaia 星。

**批复**
- [ ] 同意修复
- [ ] 刻意为之
- [ ] 暂不修复
- 备注：

---

### B9-M-10: 文件头注释过时（5 阶段 vs 9 节点）

**问题定位**
- 文件：
  - `lib/orchestrator/cpp/src/orchestrator.cpp:1-11`（"串联 5 个阶段"）
  - `lib/orchestrator/cpp/include/orchestrator.h:2`（"管理管线阶段 (CALIBRATE -> PLATESOLVE -> PSF -> PHOTOMETRIC -> DRIZZLE)"）
  - `lib/orchestrator/cpp/include/dll_loader.h:7`（"5 个模块"）

**问题描述**
多处文件头注释描述旧版 5 阶段，但实际已实现 9 节点两段流水线。新开发者阅读注释会误解架构。

**批复**
- [ ] 同意修复
- [ ] 刻意为之
- [ ] 暂不修复
- 备注：

---

## 批复汇总表

> 用户审阅完上述 54 项 Medium 问题后，可在下表汇总批复决定。

| 问题 ID | 模块 | 批复 | 备注 |
|---------|------|------|------|
| B1-M-1 | astro_image_io | | |
| B1-M-2 | astro_image_io | | |
| B1-M-3 | astro_image_io | | |
| B1-M-4 | astro_image_io | | |
| B1-M-5 | astro_image_io | | |
| B2-M-1 | calibration | | |
| B2-M-2 | calibration | | |
| B2-M-3 | calibration | | |
| B2-M-4 | calibration | | |
| B2-M-5 | calibration | | |
| B3-M-1 | plate_solve | | |
| B3-M-2 | plate_solve | | |
| B3-M-3 | plate_solve | | |
| B3-M-4 | plate_solve | | |
| B4-M-1 | dynamic_psf | | |
| B4-M-2 | dynamic_psf | | |
| B4-M-3 | dynamic_psf | | |
| B4-M-4 | dynamic_psf | | |
| B4-M-5 | dynamic_psf | | |
| B4-M-6 | dynamic_psf | | |
| B4-M-7 | dynamic_psf | | |
| B5-M-1 | photometric_calib | | |
| B5-M-2 | photometric_calib | | |
| B5-M-3 | photometric_calib | | |
| B5-M-4 | photometric_calib | | |
| B5-M-5 | photometric_calib | | |
| B5-M-6 | photometric_calib | | |
| B6-M-1 | snr_estimator | | |
| B6-M-2 | snr_estimator | | |
| B6-M-3 | snr_estimator | | |
| B6-M-4 | snr_estimator | | |
| B7-M-1 | healpix_drizzle | | |
| B7-M-2 | healpix_drizzle | | |
| B7-M-3 | healpix_drizzle | | |
| B7-M-4 | healpix_drizzle | | |
| B8-M-1 | healpix_stack | | |
| B8-M-2 | healpix_stack | | |
| B8-M-3 | healpix_stack | | |
| B8-M-4 | healpix_stack | | |
| B8-M-5 | healpix_stack | | |
| B8-M-6 | healpix_stack | | |
| B8-M-7 | healpix_stack | | |
| B8-M-8 | healpix_stack | | |
| B8-M-9 | healpix_stack | | |
| B9-M-1 | orchestrator | | |
| B9-M-2 | orchestrator | | |
| B9-M-3 | orchestrator | | |
| B9-M-4 | orchestrator | | |
| B9-M-5 | orchestrator | | |
| B9-M-6 | orchestrator | | |
| B9-M-7 | orchestrator | | |
| B9-M-8 | orchestrator | | |
| B9-M-9 | orchestrator | | |
| B9-M-10 | orchestrator | | |

---

**统计**：Medium 54 项
