# 校准模块开发记忆

## 模块概述
CCD/CMOS 标准校准模块，包含主帧生成、图像校准（含暗场优化）、坏点修复三个子模块。

## 开发阶段
- Phase 1: Python 实现（已完成）
- Phase 2: C++ 重构（进行中 - 坏点修复模块已用 C++ OpenMP 重写）

## 关键设计决策
- 暗场优化默认关闭，需显式启用
- 暗场优化使用残差最小化（黄金分割搜索最优K值）
- sigma-clip 与 median/mean 合并是独立组合的两个步骤
- 调试模式分步输出中间FITS，生产模式内存直通
- 统一使用 astro_image_io 接口读写图像
- **校准公式（用户纠正后）**:
  - 无暗场优化: `Calibrated = (Light - Dark) / Flat`（Dark 已含 Bias，直接减）
  - 有暗场优化: `Calibrated = (Light - Bias - K*(Dark - Bias)) / Flat`（需提取纯暗电流）
- **坏点修复策略（v4: 采用AstroStack3方案）**: Dark/Bias 主帧保留坏点（校准扣除），但用 Dark 全局统计检测热像素位置 + Bias 全局统计检测冷像素位置（缺陷图），Light 局部统计检测默认关闭（避免星点核心被误判），`filter_by_structure_size` 连通区域过滤排除星点
- **标准校准流程文档**: [CALIBRATION_PROCESS.md](file:///F:/Astro%20dev/Astro%20CS%20Normalization%20Database/lib/calibration/CALIBRATION_PROCESS.md)

## 数据源
- 主校准帧: testdata/Galaxy_Center_T4 全链路测试数据/calibration files/ (XISF格式)
- Light帧: testdata/Galaxy_Center_T4 全链路测试数据/lights/panel1/ (FTS格式)
- Dark: 180s/300s/600s 三种曝光
- Flat: Red/Green/Blue/H-alpha/Oiii 五种滤镜

## 进度
- [x] 目录结构创建
- [x] master_generator.py - 主帧生成模块（sigma-clip + median/mean 合并）
- [x] calibrator.py - 图像校准模块（校准公式已纠正 + 暗场优化 + 主帧自动匹配）
- [x] cosmetic_corrector.py - 坏点修复模块（v4: AstroStack3方案 Dark/Bias缺陷图 + filter_by_structure_size 连通区域过滤）
- [x] calibration_pipeline.py - 管线入口（debug/production 双模式 + CLI + enable_local_detection 参数）
- [x] run_experiment.py - 实验脚本（6帧覆盖所有滤镜/曝光）
- [x] 标准校准流程文档 CALIBRATION_PROCESS.md
- [x] astro_image_io BZERO/BSCALE 根因修复（C++ + GitHub推送）
- [x] filter_by_structure_size 严重 bug 修复（背景 label 0 泄漏）
- [x] 坏点修复实验验证通过（均值变化<0.04%，星点完好）
- [x] cosmetic_corrector C++ OpenMP 版本（替代Python中值滤波，提升性能）
- [x] cosmetic_corrector.py Python绑定C++ DLL（ctypes，模块级缓存，优先调用C++ fallback到Python）
- [x] calibrate_fits.py 统一封装接口（单帧 calibrate_fits + 批量 calibrate_batch 16线程并行）
- [x] 单帧性能测试通过（C++ DLL加载成功，2.0s/帧，较Python版8.5s提速4x）
- [x] calibrate_data() 从 numpy 路径切换到 C++ DLL（ac_calibrate_frame），6/6 测试通过

## 重大Bug修复记录
### BZERO/BSCALE 关键字泄漏 bug（2026-07-10）
- **现象**: 坏点修复后均值从 ~450 ADU 异常跳升到 ~65981 ADU（接近65535）
- **根因**: 原始16位Light帧有 BZERO=32768（FITS标准无符号16位），calibrator 将原始 keywords 复制到校准后 float32 FITS 中，后续读取时 C++ 重复应用 BZERO 导致数据偏移
- **数据链**: Light(BZERO=32768) → calibrator读取(正确应用BZERO, mean=450) → 写_calibrated.fits(保留BZERO) → correct_frame读取(再次应用BZERO, mean=33218) → 写_final.fits(保留BZERO) → 读取(再次应用, mean=65981)
- **修复**: 
  1. calibrator.py / cosmetic_corrector.py / calibration_pipeline.py 三处写入FITS时过滤 BZERO/BSCALE 关键字（float32数据不应携带）
  2. **根因修复**: astro_image_io C++ 的 `fits_write_file` 关键字过滤列表增加 BZERO/BSCALE，从源头避免泄漏（已推送到 GitHub）
- **验证**: 修复后6/6帧坏点修复后均值稳定（450->445, 100->99, 83->81），不再异常跳升

### filter_by_structure_size 背景 label 0 泄漏 bug（2026-07-10）
- **现象**: Dark 热像素候选 43504 个，`filter_by_structure_size` 过滤后变成 16196387 个（几乎全图），整个图像被中值滤波替换，均值暴跌 75%-95%
- **根因**: `sizes[0] = 0` 把背景（label 0）的 size 设为 0，然后 `0 < max_structure_size(4)` 为 True，导致背景 label 0 被包含在 `small_labels` 中。`np.isin(labeled, small_labels)` 把所有背景像素（非候选区域）都标记为 True
- **来源**: AstroStack3 原始代码也有此 bug（`sizes[0] = 0`），复制时未发现
- **修复**: `sizes[0] = max_structure_size`，确保背景不满足 `< max_structure_size` 条件
- **验证**: 修复后候选 43504 -> 过滤后 39891（正确，排除了 >4 像素的大结构如星点）

## 实验结果
### v6（AstroStack3方案 + filter_by_structure_size bug修复 + Light局部检测关闭）
- 6/6帧成功，总耗时50.95s
- 坏点修复后均值变化仅0.02%-0.04%（450.42->450.28, 83.23->83.19），星点信号完好
- 热像素 34656-39891（随曝光递增：180s=34656, 300s=37103, 600s=39891，符合热噪声物理规律）
- 冷像素固定276（Bias与曝光无关）
- max值不变（80849->80849），星点峰值无损

### v5（校准公式纠正 + 坏点修复策略重写后）
- 6/6帧成功，总耗时79.19s
- 校准后均值稳定：Red 450, Green 391, Blue 210, H-alpha 100, Oiii 82-83
- 坏点修复后均值几乎不变（450->448, 100->99），无信号损失
- **问题**: 热像素检测数过多（31K-99K），5σ+MAD对校准后Light过于敏感
- 诊断结果: median残差/σ=7.0，大量5-10σ的正常噪声被误判为坏点

### v2（坏点检测算法局部统计重写后）
- 6/6帧成功，总耗时117.81s（约19.6s/帧）
- 校准后均值显著下降：Red 1542->450, H-alpha 1249->100, Oiii 1360->83
- 热像素检测数：6292-7265，冷像素检测数：854

## 坏点检测算法重大重写（v2）
### 问题
旧版用全局阈值检测Dark热像素（MAD=0.0001极小，3σ阈值误判49万像素）+ 局部3σ cosmic ray检测（误删星点）
### 修复
1. Dark/Bias检测改用**局部统计**（5×5中值滤波+残差MAD），替代全局阈值
2. 新增**孤立性检查**（filter_isolated_pixels）：检查候选像素8个邻居中有几个也高于局部中值，超过max_neighbors_above(默认2)则视为星点PSF扩展，剔除
3. **移除cosmic ray检测**（detect_bad_pixels_local不再在校准管线中调用），宇宙线留给叠加时3sigma处理
4. 阈值从3σ提高到5σ，减少误检
### 效果
热像素 49万→6292，冷像素 31K→854，坏点修复后均值不再异常跳升

## master_generator.py 详情
- **路径**: `lib/calibration/python/master_generator.py`
- **IO**: 统一使用 astro_image_io（ImageReader 读 FITS/XISF，FITSWriter 写 float32 FITS）
- **核心算法**:
  - `sigma_clip_reject(stack, sigma_low, sigma_high, max_iterations)`: 向量化 sigma-clip，沿 axis=0 迭代，MAD 估计 sigma（1.4826 归一化），NaN 标记剔除值，收敛提前终止，常量区域(sigma=0)不误杀
  - `combine_frames(stack, rejection, combine)`: rejection(none/sigma_clip) 与 combine(median/mean) 独立组合，输出 float32
  - `normalize_flat(flat_data)`: 归一化到 median=1.0，最小值裁剪 0.1
- **MasterGenerator 类**:
  - `generate_master_bias`: 加载→sigma-clip→median→写FITS（IMAGETYP/NCOMBINE/BUNIT）
  - `generate_master_dark`: 可选减Bias→sigma-clip→median→写FITS（+EXPTIME 从首帧读取）
  - `generate_master_flat`: 减Bias→逐帧归一化→sigma-clip→mean→再归一化→写FITS（+FILTER 从首帧读取）
  - `max_workers`: ThreadPoolExecutor 并行加载帧（ctypes 释放 GIL）
- **日志**: `lib/calibration/logs/master_generator_YYYYMMDD_HHMMSS.log`，UTF-8，文件+控制台
- **验证**: 6/6 合成测试通过（离群值剔除/median合并/none+mean/flat归一化/常量区域/参数校验）

## calibrator.py 详情
- **路径**: `lib/calibration/python/calibrator.py`
- **IO**: 统一使用 astro_image_io（ImageReader 读 FITS/XISF，FITSWriter 写 float32 FITS）
- **核心算法**:
  - `find_matching_master_dark(dark_dir, exposure, tolerance=10.0)`: 文件名正则解析 EXPOSURE-XXX.XXs，找最接近 exposure 的 Dark，超容差返回 None
  - `find_matching_master_flat(flat_dir, filter_name)`: 文件名正则解析 FILTER-XXX_mono，精确匹配滤镜名（大小写不敏感）
  - `unify_data_range(light, light_bitpix, master, master_bitpix)`: 若一方归一化(max<=1.5)另一方非归一化，将归一化方乘以 2^非归一化方bitpix-1（负bitpix用16默认）
  - `extract_background_mask(data)`: 全局 median + MAD，保留 |data-median|<=3*1.4826*MAD 的背景像素
  - `optimize_dark_scale(light, bias, dark, flat, k_init)`: 黄金分割搜索(0.618法)最优K，目标为背景区域MAD最小；预计算背景区域值加速；搜索范围[k_init*0.5, k_init*1.5]，精度0.01，最大20次迭代
  - `calibrate(light, bias, dark, flat, dark_scale_factor, dark_optimization, ...)`: 
    - 无暗场优化: `(Light - Dark) / NormalizedFlat`（Dark 已含 Bias，直接减）
    - 有暗场优化: `(Light - Bias - K*(Dark - Bias)) / NormalizedFlat`（提取纯暗电流再缩放）
    - Flat归一化median=1.0最小裁剪0.1；返回(calibrated, actual_k, stats)
- **Calibrator 类**:
  - `calibrate_frame(light_path, output_path, master_bias, master_dark, master_flat, dark_optimization, calibration_dir)`: 文件模式，calibration_dir提供时自动匹配主帧（EXPTIME/FILTER从FITS头读取），写FITS头记录CALIBRAT/DARKSCAL/MASTERBI/MASTERDA/MASTERFL
  - `calibrate_data(light_data, master_bias, master_dark, master_flat, dark_optimization, light_exposure, dark_exposure)`: **生产模式内存直通，调用C++ DLL ac_calibrate_frame**，输入numpy返回(numpy, stats_dict)，不读写文件；K_init=light_exp/dark_exp(若都>0)否则1.0；Flat归一化(median=1.0,clip 0.1)在Python端预处理（C++ DLL内部仅裁剪0.1不做median归一化），其余校准运算(减bias/dark、除flat、暗场优化黄金分割搜索)全部由C++ DLL OpenMP 16线程完成；__init__中调用ac_set_num_threads设置DLL线程数
- **日志**: `lib/calibration/logs/calibrator_YYYYMMDD_HHMMSS.log`，UTF-8，文件+控制台
- **验证**: 合成数据全通过（主帧匹配6/6、数据范围统一3/3、背景提取、暗场优化K误差<0.01、标准校准3/3、内存直通、文件模式FITS头5/5关键字）

## cosmetic_corrector.py 详情（v4: AstroStack3方案 + C++ DLL绑定）
- **路径**: `lib/calibration/python/cosmetic_corrector.py`
- **IO**: 统一使用 astro_image_io（ImageReader 读 FITS/XISF，FITSWriter 写 float32 FITS）
- **检测策略**: Dark全局统计检测热像素位置 + Bias全局统计检测冷像素位置（缺陷图），Light局部统计检测默认关闭（enable_local_detection=False）
- **C++ DLL绑定**: 优先调用C++ DLL（OpenMP并行），失败时fallback到Python scipy
  - `_load_cpp_dll()`: 模块级缓存加载 `lib/calibration/cosmetic_corrector.dll`，设置4个函数签名（cc_correct_median/cc_detect_hot/cc_detect_cold/cc_last_error）
  - `detect_hot_pixels_from_dark`: 优先C++ `cc_detect_hot`（全局统计+BFS连通过滤），fallback到Python（scipy.ndimage.label）
  - `detect_cold_pixels_from_bias`: 优先C++ `cc_detect_cold`，fallback到Python
  - `interpolate_bad_pixels`: median方法优先C++ `cc_correct_median`（5×5中值滤波OpenMP并行），fallback到Python（scipy.ndimage.median_filter）；bilinear方法仅Python
  - 使用 `np.ascontiguousarray` 确保数组内存连续，ctypes指针传递
  - C++返回值<0表示错误，调用 `cc_last_error()` 获取错误信息
- **核心算法**:
  - `filter_by_structure_size(mask, max_structure_size=4)`: scipy.ndimage.label 连通区域标记 + np.bincount 统计大小，只保留 < max_structure_size 的结构（排除星点）。**注意**: `sizes[0]=max_structure_size` 防止背景 label 0 泄漏
  - `detect_hot_pixels_from_dark(dark_data, threshold=5.0, max_structure_size=4)`: 全局统计 median+threshold*1.4826*MAD，Dark主帧不修复仅定位坏点
  - `detect_cold_pixels_from_bias(bias_data, threshold=5.0, max_structure_size=4)`: 全局统计 median-threshold*1.4826*MAD，Bias主帧不修复仅定位坏点
  - `detect_bad_pixels_local(data, hot_sigma, cold_sigma, window_size=5, max_structure_size=4)`: 5×5中值滤波残差法，残差MAD估计噪声，默认关闭
  - `interpolate_bad_pixels(data, bad_mask, method="median")`: median用5×5中值滤波替换；bilinear用scipy.interpolate.griddata双线性插值（NaN边缘用最近邻回填）
- **统一入口**:
  - `correct_frame(input_path, output_path, hot_sigma, cold_sigma, method, max_structure_size, master_dark, master_bias, enable_local_detection, ...)`: 文件模式，读取校准后Light->Dark/Bias缺陷图检测->(可选)局部检测->结构过滤->插值->写FITS
  - `CosmeticCorrector.correct_data(data, hot_sigma, cold_sigma, ..., dark_data, bias_data, enable_local_detection)`: **生产模式内存直通**，输入numpy返回numpy+统计dict
- **日志**: `lib/calibration/logs/cosmetic_corrector_YYYYMMDD_HHMMSS.log`，UTF-8，文件+控制台
- **验证**: v6实验通过，均值变化<0.04%，热像素34656-39891，冷像素276，星点完好
- **C++ DLL验证**: 单帧性能测试通过，C++ DLL加载成功，热像素34652/冷像素276/修复34928，均值450.28（与Python版一致）

## cosmetic_corrector C++ OpenMP 版本详情（Phase 2）
- **路径**: `lib/calibration/cpp/cosmetic_corrector.h` + `cosmetic_corrector.cpp`
- **构建**: `lib/calibration/Makefile`（`make` 编译为 `cosmetic_corrector.dll`）
- **编译环境**: MSYS2 MinGW64 g++ 16.1.0，CXXFLAGS=`-O3 -march=native -ffast-math -funroll-loops -fopenmp -Wall -std=c++17`
- **导出C接口**（4个函数，ctypes 可直接调用）:
  - `cc_correct_median(data, bad_mask, H, W, window)`: 5×5中值滤波修复坏像素，OpenMP按行并行 `schedule(dynamic, 64)`，先复制副本避免串行污染，nth_element 求中值，clamp边缘处理
  - `cc_detect_hot(dark_data, H, W, sigma, max_structure_size, out_mask)`: 全局 median+MAD 阈值检测 + BFS连通区域过滤（8连通，>=max_structure_size 移除）
  - `cc_detect_cold(bias_data, H, W, sigma, max_structure_size, out_mask)`: 同上，检测冷像素（低于阈值）
  - `cc_last_error()`: 获取最后错误信息
- **连通区域过滤算法**: BFS 8连通标记，queue 复用避免重复分配，区域大小 >= max_structure_size 的从掩码移除（与Python `filter_by_structure_size` 逻辑一致）
- **依赖**: 仅 STL + OpenMP，不依赖外部库
- **日志**: stderr 输出（printf），包含 median/MAD/threshold/候选数/过滤后数
- **性能**: 设计目标 3600×4500 图像 40万坏像素修复 < 0.5s（16线程 OpenMP 并行）
- **与Python版本的一致性**: 连通区域过滤使用 `>= max_structure_size` 移除（与Python `sizes < max_structure_size` 保留一致），MAD=0 时回退到标准差

## calibrate_fits.py 详情（统一封装接口）
- **路径**: `lib/calibration/python/calibrate_fits.py`
- **功能**: 单帧/批量校准Light帧的简洁接口，内部调用 CalibrationPipeline
- **接口**:
  - `calibrate_fits(light_path, output_path, calibration_dir, mode, cc_method, ...)`: 单帧校准，管线默认输出({base}_calibrated.fits)重命名为指定output_path
  - `calibrate_batch(light_paths, output_dir, calibration_dir, max_workers=16, ...)`: 批量校准，ThreadPoolExecutor并行，每帧独立子目录，worker内部max_workers=1避免嵌套线程池
  - CLI入口: `--light`(多个启用批量) `--output-dir` `--calibration-dir` `--mode` `--cc-method` 等
- **设计要点**:
  - 输入校验: 检查light_path/calibration_dir存在性
  - 输出重命名: pipeline.run()返回output_path后，用shutil.move重命名为用户指定路径
  - 批量顺序保证: results列表按输入顺序填充，as_completed仅控制完成顺序
  - 日志: `lib/calibration/logs/calibrate_fits_YYYYMMDD_HHMMSS.log`，UTF-8
- **性能测试**: 单帧Red(4500x3600) 2.0s/帧（C++ DLL加速后），较Python版8.5s提速4x
  - 时间分布: I/O读取~0.12s(4文件) + 校准~0.1s + C++ DLL(加载+detect+fix)~0.8s + FITS写入~1.0s
  - 瓶颈: FITS写入(61.8MB float32)约1s，C++ DLL首次加载约0.5s（批量时摊销）
