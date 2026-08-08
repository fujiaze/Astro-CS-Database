# photometric_calib - 模块开发memory

## 模块职责
鲁棒流量校准，将图像归一化到Gaia DR3/DR3SP星表系统，输出与Gaia参考星系统一致的校正图像。当前版本为C++ DLL简化版（全局scale校正，去掉梯度拟合）。

## 当前版本
- 版本号：v2.0 C++ DLL
- 最新commit：1a6fd32
- 更新时间：2026-07-12

## GitHub仓库
- 仓库地址：https://github.com/fujiaze/Flux-calibration
- 默认分支：master

## 依赖列表
- C++17, OpenMP
- astro_image_io.dll（FITS读写）
- gaia_xpsd_client（Gaia星表查询）
- Python ctypes（PhotometricCalib类封装）

## 关键决策记录
- **C++ DLL重写完成（pc_calibrate_simple）**：从Python版本迁移到C++ DLL，697KB静态链接，OpenMP 16线程，仅依赖KERNEL32/msvcrt系统DLL
- **去掉梯度拟合**：去掉M_map曲面拟合（GradientEstimator/gradient_fitter），简化为全局scale校正，避免星点稀疏采样导致过拟合
- **scale=median(F_syn/F_instr)**：算法流程为WCS投影Gaia星→暴力最近邻匹配（距离<3px）→MAD离群清洗→scale=median(F_syn/F_instr)→I_cal=I*scale
- **4个Python文件归档**：estimator.py（旧版GradientEstimator）、gradient_fitter.py（旧版梯度曲面拟合器）等从flux_calibrator/python/移入archive/
- **天光校正封存**：S_map加性梯度天光校正封存（注释调用，可逆），photometric_calib仅做乘性流量定标

## 进度日志

### 2026-08-08 Phase1 Final Closure V3 — XPSD 官方解码生产链
- `spectrum_integrator`: 新增 `compute_f_syn_cached_xpsd(cache, byte, n, flux_min, flux_mul)`
  = ∫ (byte*fluxMul+fluxMin)·T(λ)·Q(λ)·λ dλ (绝对谱辐照度积分, 不再乘 10^(-0.4G))。
- `pc_api.cpp`: 3 处生产调用 (v2/f64_v2/per-star) 全部切换官方解码;
  旧 `compute_f_syn_cached(uint8, mag_g)` 保留仅作历史/测试兼容。
- 交叉验证: 生产 C++ vs Python 同算法 ratio 1.000000000010 (p95|1-r|=2.8e-10);
  C++ vs GaiaXPy 绝对光谱 |dG| median 0.00009 / p95 0.00063 mag (G 通带)。
- 待办: T1-T4 真实帧 PHOTOMETRIC 端到端复验 (下一阶段)。

### 2026-07-15 sigma_residual 暴露（spec: photometric-sigma-residual）
- star_matcher.h/cpp: cleanOutliers/matchAndClean 新增 `double* out_sigma_residual = nullptr` 出参, 暴露已计算的 MAD/0.6745
- photometric_calib.h: pc_calibrate_simple/pc_calibrate_simple_with_gaia 新增末尾参数 out_sigma_residual
- pc_api.cpp: 透传 sigma_residual + 退化路径(n_gaia<=0/n_psf<=0/无光谱星/滤光片失败)设 0.0 + nullptr 检查
- python/photometric_calib.py: argtypes 追加 POINTER(c_double), calibrate_simple/with_gaia 返回 4 元组(out_pixels, n_matched, scale, sigma_residual)
- orchestrator photometric_adapter.py: photo_stats KV 块新增 SIGMA_RESIDUAL 字段
- 端到端验证: sigma_residual=0.168mag, n_matched=1527, 全 6 节点通过, photometric 0.867s
- 向后兼容: out_sigma_residual 可为 nullptr, 旧调用方不受影响

### 2026-07-13 P0/P1/P2 性能优化（spec: format-unification-browser-perf 阶段5）

**性能问题**: photometric 阶段耗时 354.7s（典型应 < 5s），根因是 C++ DLL 循环内大量 fprintf + 滤光片曲线重复预处理 + 固定 mag_max=16.0 返回数万颗星。

**P0: 循环内 fprintf 清除（Task 10）**:
- 新建 `cpp/include/log_macros.h` - LOG_INFO/LOG_DEBUG/LOG_ERROR 宏定义
  - LOG_DEBUG 默认编译时不启用（展开为 ((void)0)），可通过定义 PC_ENABLE_DEBUG 启用
  - LOG_INFO/LOG_ERROR 输出到 stderr，自动加 [INFO]/[ERROR] 前缀和 \n 后缀
- `cpp/src/spectrum_integrator.cpp` - 第241行循环内 fprintf → LOG_DEBUG（被 pc_api.cpp OpenMP 循环高频调用）
- `cpp/src/star_matcher.cpp` - 第86-89行循环内每颗匹配星的 fprintf → LOG_DEBUG
- `cpp/src/pc_api.cpp` - 加 #include "log_macros.h"
- 错误路径 fprintf 保留原样（最小改动）
- 编译结果: photometric_calib.dll 1,031.2 KB，LOG_DEBUG 编译时展开为空，零运行时开销

**P1: 滤光片曲线预处理缓存（Task 11，SpectrumIntegratorCache）**:
- 循环前预处理一次（排序+Akima插值重采样到光谱网格）
- 循环内只算 SED + 星等归一化 + Simpson 积分
- 避免每颗星重复对滤光片曲线排序和插值

**P2: 自适应迭代星等（Task 12）**:
- `cpp/src/pc_api.cpp` - pc_calibrate_simple_with_gaia 函数中锥形搜索改为自适应迭代
- mag_max 从 12.0 开始，若 n_gaia<2000 则增大到 13/14/15/16.0，直到 n_gaia>=2000 或达 16.0 上限
- 不缩小 radius_deg，mag_min 保持外部传入值
- 返回 2000-10000 颗星，避免固定 16.0 返回数万颗星导致后续处理耗时过长
- 函数签名不变（mag_max 参数仍接受但内部用迭代覆盖）
- 用 LOG_INFO 输出每次迭代 mag_max 和 n_gaia

**性能结果**:
| 阶段 | 修复前(s) | 修复后(s) | 改善 |
|------|-----------|-----------|------|
| photometric | 354.7 | 0.881 | **99.75%** |
| 总管线 | 409.0 | 60.615 | 85.18% |

- photometric 354.7s → 0.881s，超额完成 < 5s 目标
- n_matched = 1527，scale_factor = 7.132932e-03
- P0 + P1 + P2 三重优化叠加效果显著
- 总管线未达 < 30s 目标主因是 drizzle 阶段 26.3s（占总管线 43%），非 photometric 优化范围

### 2026-07-13 扩展 pc_calibrate_simple_with_gaia 接口
- 新增 `pc_calibrate_simple_with_gaia` 接口：DLL 内部调用 gaia_client 查询 DR3SP 光谱 + OpenMP 16线程并行积分 F_syn
- 新建 `cpp/src/spectrum_integrator.h/.cpp`：Akima 子样条插值 + Simpson 1/3 复合积分 + compute_f_syn（参考 python/synthetic_photometry.py）
- 修改 `cpp/include/photometric_calib.h` + `cpp/src/pc_api.cpp`：新增接口声明与实现，原 pc_calibrate_simple 完全不变
- 修改 `cpp/Makefile` + `build.ps1`：链接 gaia_client.dll，复制到输出目录，mingw64/bin 加入 PATH
- 接口签名补充 WCS/PSF flux 参数（任务描述签名缺这些，无法复用 StarMatcher）
- 编译成功：photometric_calib.dll 1.03MB，导出 pc_calibrate_simple + pc_calibrate_simple_with_gaia
- 关键修复：g++ 需 mingw64/bin 在 PATH 中（找 cc1plus/ld 子进程）；build.ps1 改用 & 调用替代 Start-Process（处理含空格 -I 路径）

### 2026-07-12 C++ DLL重写完成
- C++ DLL重写完成（pc_calibrate_simple），4/4测试通过
- 去掉梯度拟合，简化为全局scale=median(F_syn/F_instr)校正
- 4个Python文件归档到archive/
- pipeline_adapter.py重写为调用C++ DLL，去掉GradientEstimator依赖
- 推送至GitHub：commit 1a6fd32

### 2026-07-13 仓库结构整理完成
- GitHub仓库分支统一为main
- 文档刷新并重新推送
- 最新commit: a743531

---

## 详细开发记录（历史归档）

> 根目录索引: [memory.md](file:///F:/Astro%20dev/Astro%20CS%20Normalization%20Database/memory.md)

## 模块概览

- **模块名称**: photometric_calib
- **功能**: 鲁棒流量校准，消除空间缓变梯度（残留渐晕、月光、光害、大气消光、大气辉光），输出与 Gaia DR3/SP 星表系统一致的校正图像
- **算法依据**: [docs/algorithm.md](file:///F:/Astro%20dev/Astro%20CS%20Normalization%20Database/lib/photometric_calib/docs/algorithm.md)
- **架构依据**: [docs/architecture.md](file:///F:/Astro%20dev/Astro%20CS%20Normalization%20Database/lib/photometric_calib/docs/architecture.md)
- **历史仓库**: https://github.com/fujiaze/Robust-Flux-Calibration (v1.0 Python版, 2026-07-12, 已被Flux-calibration取代)

## 目录结构

```
lib/photometric_calib/
├── cpp/                # C++ DLL（简化版全局scale校正，2026-07-12新建）
│   ├── include/photometric_calib.h   # C API 声明
│   ├── src/
│   │   ├── pc_api.cpp              # C API 包装
│   │   ├── star_matcher.cpp/.h     # 暴力最近邻匹配 + MAD清洗
│   │   ├── image_corrector.cpp/.h  # I_cal=I*scale 校正
│   │   └── wcs_transform.cpp/.h    # TAN+SIP投影
│   ├── test/test_photometric_calib.py  # 测试 (4/4 通过)
│   ├── Makefile
│   └── build.ps1
├── python/             # Python 接口
│   ├── __init__.py
│   ├── photometric_calib.py   # C++ DLL ctypes 封装 (PhotometricCalib类)
│   ├── pc_logger.py          # 日志系统
│   ├── sed_builder.py        # SED构造器 (保留：spectrum_integrator 自测惰性引用)
│   └── synthetic_photometry.py  # 合成测光 (保留：spectrum_integrator 自测惰性引用)
├── flux_calibrator/    # 流量校准器（pipeline_adapter 已切换到 C++ DLL）
│   └── python/
│       ├── pipeline_adapter.py  # 管线适配器（调用C++ DLL, 去掉GradientEstimator）
│       ├── fsyn_loader.py       # F_syn JSON 加载器
│       ├── star_matcher.py      # Python版星匹配（保留供测试）
│       ├── wcs_transform.py     # Python版WCS（保留供测试）
│       └── image_corrector.py   # Python版图像校正（保留供测试）
├── archive/            # 归档（2026-07-12 从 flux_calibrator/python/ 移入）
│   ├── estimator.py           # 旧版 GradientEstimator（含梯度拟合）
│   └── gradient_fitter.py     # 旧版梯度曲面拟合器
├── spectrum_integrator/ # 光谱积分器（活跃版本）
└── logs/               # 日志输出目录
```

## 开发记录

### [简化版 2026-07-12] C++ DLL 全局 scale 校准

**决策**：去掉梯度拟合（M_map曲面拟合），简化为全局scale校正。

**算法**：
1. WCS投影Gaia星到像素坐标（TAN+SIP投影）
2. 暴力最近邻匹配PSF星和Gaia星（距离<3px）
3. MAD离群清洗（r=log10(F_instr/F_syn), sigma=3.0）
4. scale=median(F_syn/F_instr)
5. I_cal=I*scale

**新建文件**：
- `cpp/include/photometric_calib.h` - C API 声明 (pc_calibrate_simple)
- `cpp/src/wcs_transform.cpp/.h` - TAN+SIP投影 (参考 healpix_drizzle/wcs_sip.cpp)
- `cpp/src/star_matcher.cpp/.h` - 暴力最近邻 + MAD清洗 (无nanoflann依赖)
- `cpp/src/image_corrector.cpp/.h` - scale=median(F_syn/F_instr), I_cal=I*scale
- `cpp/src/pc_api.cpp` - C API 包装层
- `cpp/Makefile` / `cpp/build.ps1` - 构建脚本
- `python/photometric_calib.py` - ctypes 封装 (PhotometricCalib类)
- `cpp/test/test_photometric_calib.py` - 4项测试

**修改文件**：
- `flux_calibrator/python/pipeline_adapter.py` - 重写为调用C++ DLL，去掉GradientEstimator/gradient_fitter/image_corrector依赖，去掉grad_map块，保留photo_stats KV块(N_MATCHED, SCALE_FACTOR)

**归档文件** (flux_calibrator/python/ -> archive/)：
- `estimator.py` - 旧版GradientEstimator
- `gradient_fitter.py` - 旧版梯度曲面拟合器

**编译**：`make` 成功，photometric_calib.dll 697KB，`-static` 全静态链接（仅依赖KERNEL32/msvcrt系统DLL），OpenMP 16线程

**测试结果**：4/4 通过
1. 基本测光校准 (10星TAN投影, scale=10.0)
2. MAD离群清洗 (20星注入1离群, 保留19)
3. 无Gaia星退化 (scale=1.0)
4. SIP WCS投影 (二阶SIP, 10星匹配)

**关键设计**：
- WCS: CRPIX 1-based, 像素0-based, dx=x-(CRPIX-1)
- SIP系数按i*6+j索引（长度36扁平数组）
- 无AP/BP时用3次牛顿迭代反解前向SIP
- 暴力最近邻（Gaia星通常<10000, 无需nanoflann）
- MAD: sigma=MAD/0.6745, sigma=0时跳过清洗（与Python版一致）



> **清理记录（2026-07-12）**: 顶层 python/ 下的 star_matcher.py、image_corrector.py 含已确认梯度方向 bug（r=log10(F_syn/F_instr) 方向反转），gradient_fitter.py 未调参（MAX_ORDER=5），wcs_transform.py/curve_loader.py 为冗余副本。上述 5 个文件已删除，新版位于 gradient_estimator/python/ 和 spectrum_integrator/python/。sed_builder.py 和 synthetic_photometry.py 保留（spectrum_integrator/python/synthetic_photometry.py 自测块惰性引用 sed_builder，跨目录依赖未解耦）。

## 开发记录

### [封存 2026-07-12] 天光校正（S_map 加性梯度）

**决策**：封存 S_map 加性梯度天光校正，photometric_calib 仅做乘性流量定标（M_map）。

**原因**：
1. 信号污染：PSF 拟合的局部背景 B 值包含 ISL+DGL+气辉+黄道光+星云目标信号。在银河等区域，缓变星云信号会被当作天光误减，破坏真实目标。
2. 采样稀疏：B 值仅在星点位置有（全图几十到几百个点），多项式在星点之间无物理约束，易过拟合。
3. 无物理先验：未建模 ISL/DGL/气辉的空间结构，多项式无法区分"缓变天光"与"缓变星云"。

**封存方式**：注释调用（可逆）
- estimator.py：注释 `fit_additive` 调用、加性 R² 信号检测、加性残差 CSV 输出；`add_surface = _identity_surface()`
- image_corrector.py：注释 S_map 评估（返回零矩阵）；校正公式 `I_cal = I_float / max(M_map, _MIN_M)`
- 质量报告新增 `sky_calibration_frozen=True` 字段

**后续方案**：天光一致性由后续马赛克背景匹配模块处理（借鉴光度马赛克方法，多帧共享最小残差低频背景）。

**恢复方法**：取消 estimator.py 和 image_corrector.py 中的注释即可恢复天光校正。

**GAMBONS 文献参考**（未引入实现，仅备查）：
- Paper I (2021 MNRAS): A multiband map of the natural night sky brightness including Gaia and Hipparcos integrated starlight — Masana et al., DOI:10.1093/mnras/staa4005, arXiv:2101.01500
- Paper II (2024 arXiv): An enhanced version of the Gaia map of the brightness of the natural sky — arXiv:2408.17371
- 核心方法论：5 分量分解（ISL+DGL+zodiacal+airglow+extinction），先剥离 Gaia 恒星通量再拟合大气气辉
- 用户决策：借鉴方法论但不引入复杂物理建模，黄道光等精细建模过于繁琐

### 图像校正器 ImageCorrector (image_corrector.py)
- **功能**: 利用乘性/加性梯度曲面对图像逐像素校正, 并归一化到 Gaia 参考星系统
- **核心公式**:
  - M(x,y)=10^r(x,y), S(x,y)=s(x,y) (r 为 log10(F_syn/F_instr), s 为 PSF 背景值 B)
  - I_cal=(I-S)/max(M,0.01), M 下限钳位 0.01
  - F_cal_i=F_instr_i/M(x_i,y_i), scale=median(F_syn_i/F_cal_i), I_final=I_cal*scale
- **API**:
  - `evaluate_gradient_maps(mult_surface, add_surface, width, height, fitter=None) -> (M_map, S_map)` float32
  - `correct(image, mult_surface, add_surface, fitter=None) -> I_cal` float32
  - `normalize(image_cal, matches_x, matches_y, f_syn_arr, f_instr_arr, mult_surface, fitter=None) -> (I_final, scale)`
  - `correct_and_normalize(...) -> (I_final, scale, M_map, S_map)` 一站式
- **归一化精度**: normalize 在匹配星位置重新评估曲面 (evaluate_surface) 得到精确 M(x_i,y_i), 而非从 M_map 插值
- **验证**: 100x100 uint16=1000, r=0.1(M=1.2589), S=100 -> I_cal=714.8954, scale=1.0, 钳位测试通过 (5/5)

### SED 构造器 SEDBuilder (sed_builder.py)
- **功能**: 基于 Gaia BP/RP 颜色推断 Teff, 生成 Planck 黑体光谱作为合成测光 SED 输入
- **背景**: gaia_xpsd_client C API 仅暴露 magBP/magRP 积分星等, MVP 阶段用黑体近似
- **BP-RP -> Teff**: 多项式 Teff=5040/(a+b*x+c*x²+d*x³), 系数 a=0.30/b=0.65/c=-0.10/d=0.02
  - 基于 Gaia DR3 颜色-温度关系标定 (Casagrande et al. 2021), 覆盖 A-F-G-K-M 型星
  - 标定点: BP-RP=0.5->8365K(A7V), 1.0->5793K(G2V), 2.0->3706K(M0V)
  - 钳位范围 [3000, 50000]K
- **Planck 黑体**: B(λ,T)=(2hc²/λ⁵)/(exp(hc/λkT)-1), 波长 336-1020nm 步长 0.1nm (6841点), max归一化到[0,1]
- **API**: `from_bp_rp(bp_mag, rp_mag)`, `from_teff(teff)`, `bp_rp_to_teff(bp_rp)`, `planck_spectrum(wl, teff)`
- **验证**: 10/10 通过 (波长数组6841点, BP-RP=0.5->8365K峰值346nm, BP-RP=2.0->3706K峰值782nm, BP-RP=1.0->5793K峰值500nm, 归一化, 一致性, 钳位)

### 星-图匹配器 StarMatcher (star_matcher.py)
- **功能**: 将 Gaia 参考星与图像 PSF 拟合星空间匹配, 为每颗匹配星计算合成流量 F_syn, 生成 StarMatch 列表供梯度拟合器使用
- **数据结构**: `GaiaStarPy`(ra/dec/mag_g/mag_bp/mag_rp/source_id), `StarMatch`(x/y/f_instr/b_local/f_syn/gaia_g_mag/gaia_id/bp_rp)
- **API**:
  - `match(wcs, gaia_stars, psf_results, match_radius_px=3.0) -> list[StarMatch]` - 匹配+合成流量
  - `clean_outliers(matches, outlier_sigma=3.0) -> (cleaned, n_excluded)` - MAD稳健裁剪
  - `match_and_clean(...) -> (cleaned, n_excluded)` 一站式; `to_arrays(matches) -> dict` 转numpy数组
- **算法**:
  1. 过滤 PSF 失败星 (status != 0, DPSF_FIT_OK=0)
  2. scipy.spatial.KDTree 对 PSF 有效星 (cx,cy) 建索引
  3. 批量 WCS 投影 Gaia 星到像素 (sky_to_pixel_batch)
  4. 每颗 Gaia 星 KDTree 最近邻搜索 (距离 < match_radius_px)
  5. SEDBuilder.from_bp_rp(mag_bp,mag_rp) 生成 SED -> SyntheticPhotometry.compute 算 F_syn
  6. clean_outliers: r=log10(F_syn/F_instr), 排除 F<=0, sigma=MAD/0.6745, 剔除 |r-median|>outlier_sigma*sigma
- **设计要点**:
  - StarMatch.x/y 取 PSF 测量质心 cx/cy (仪器实测位置, 而非 Gaia 投影位置), 适合空间梯度建模
  - Gaia 星支持 GaiaStarPy dataclass 或 dict 输入; PSF 结果鸭子类型访问字段 (不硬依赖 dynamic_psf)
  - 日志: 实例 logger 'star_matcher', log_dir 时输出 star_matcher.log (参照 gradient_fitter 模式)
- **验证**: 8/8 通过 (10 Gaia+11 PSF含1失败 -> match 10对过滤失败星; 坐标==PSF质心; 字段填充; clean 0排除; to_arrays 形状; 注入离群点剔除1; match_and_clean一致; dict输入兼容)
- **测试数据注意**: r=log10(F_syn/F_instr) 在颜色上非单调 (F_syn 在 SED 峰值对齐滤光片时最大, 600nm 滤光片对应 BP-RP≈1.35), 测试颜色范围须取 r 单调区间(0.7~1.2)避免天然离群点

### 11.2 photometric 性能优化（2026-07-13，P0+P1+P2）+ 11.3 性能结果（2026-07-15，从 PROJECT_ARCHITECTURE.md 迁入）

**问题**：photometric 阶段在 Galaxy_Center 测试帧上耗时 354.7 s（典型应 < 5 s），根因是循环内 fprintf 日志 + 滤光片曲线重复预处理 + 锥形搜索返回星数过多。总管线 409 s vs 典型 20.9 s，性能回归 18 倍。

#### P0: 循环内 fprintf 清除

**新建** `lib/photometric_calib/cpp/include/log_macros.h`：
- 定义 `LOG_INFO` / `LOG_DEBUG` / `LOG_ERROR` 宏
- **`LOG_DEBUG` 默认编译时不启用**（宏展开为 `((void)0)`），可通过定义 `PC_ENABLE_DEBUG` 启用
- `LOG_INFO` / `LOG_ERROR` 输出到 stderr，自动加 `[INFO]` / `[ERROR]` 前缀和 `\n` 后缀

**修改** 3 个 C++ 文件：
- `spectrum_integrator.cpp`：`compute_f_syn` 末尾的循环内 fprintf 改为 LOG_DEBUG（错误路径上 6 个 fprintf 保留原样）
- `star_matcher.cpp`：`matchBruteForce` 循环内每颗匹配星的 fprintf（第 86-89 行）改为 LOG_DEBUG
- `pc_api.cpp`：循环内 fprintf 改为 LOG_DEBUG

#### P1: 滤光片曲线预处理缓存

**问题**：`pc_calibrate_simple_with_gaia` 循环内每颗星都重新预处理滤光片曲线（排序 + akima 插值到光谱网格），重复计算浪费。

**修复**：引入 `SpectrumIntegratorCache` 结构，循环前预处理一次滤光片曲线，循环内只算：SED（uint8→float64）+ 星等归一化 + 积分

#### P2: 自适应迭代星等

**问题**：固定 `mag_max=16.0` 查询 Gaia，返回星数可能数万颗，后续 OpenMP 光谱积分 + 星匹配耗时过长。

**修复**：`pc_calibrate_simple_with_gaia` 改为自适应迭代星等查询：
- 从 `mag_max=12.0` 开始查询 Gaia
- 去除饱和星（`mag_min` 设为避免饱和的下限）
- 如果返回星数 < 2000，增加 `mag_max` 到 13.0 / 14.0 / 15.0 / 16.0
- 直到返回星数在 **2000-10000** 范围
- 不缩小锥形搜索半径（保持 0.5×FOV 对角线）

#### 11.3 性能结果（Galaxy_Center 测试帧，2026-07-13）

**测试帧**：`Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@061703-180S-Red.fts`（4500×3600，BITPIX=16，Red 滤光片，180 s）

| 阶段 | 修复前 (s) | 修复后 (s) | 改善 |
|------|-----------|-----------|------|
| 0_read_fits | — | 0.061 | — |
| 1_calibrate | — | 1.231 | — |
| 2_platesolve_total | — | 4.434 | — |
| 3_psf_fit | — | 0.436 | — |
| **4_photometric** | **354.7** | **0.881** | **-99.75%** |
| 5_drizzle | — | 26.316 | — |
| **total** | **409.0** | **60.615** | **-85.18%** |

**photometric 关键指标**：
- `n_matched = 1527`（Gaia BP/RP 光谱积分后星匹配数）
- `scale_factor = 7.132932e-03`（全局 scale 校正因子）
- `before_mean=1563.07 → after_mean=11.15`，比值=0.00713 ≈ scale_factor ✓
- 自适应迭代星等查询生效，`pc_calibrate_simple_with_gaia` 内部 `mag_max` 迭代控制星数

**结论**：
- photometric 阶段 354.7 s → 0.881 s（**99.75% 改善**），超额完成 < 5 s 目标
- 总管线 409 s → 60.6 s（**85.18% 改善**），未达 < 30 s（drizzle 26.3 s 占主导，非 photometric 优化范围）
- P0（循环内 fprintf 清除）+ P1（滤光片曲线预处理缓存）+ P2（自适应迭代星等）三重优化叠加效果显著

### 2026-07-17 GAP-012 + GAP-013 修复（CCD QE 曲线 + 亮度比例稳健回归）

**触发**: `docs/DESIGN_IMPL_GAP.md` 审计发现两处高优先级算法-实现不一致：
- GAP-012（高）：算法文档 `F_syn = ∫ S(λ)·T(λ)·Q(λ) dλ`，但 C API 参数列表无 `qe_curve`，DLL 内部积分实际只有 `S(λ)·T(λ)`，CCD QE 量子效率曲线未参与合成流量计算
- GAP-013（高）：photometric_calib C API 简化版仅做 `median(F_syn/F_instr)` 全局 scale，缺乏稳健回归 + 星等一致性预过滤，离群/误匹配会污染 scale

**GAP-012 修复（CCD QE 曲线接入）**:

修改文件:
- `cpp/src/spectrum_integrator.h` - `SpectrumIntegratorCache` 结构新增 `qe_trans` 字段（重采样到光谱网格的 QE），`weighted_wl` 注释改为 `λ × T(λ) × Q(λ)`；`prepare_filter_cache` / `compute_f_syn` 签名新增 `const double* qe_wl, const double* qe_trans, int qe_count`
- `cpp/src/spectrum_integrator.cpp` - `compute_f_syn` 中: QE 排序去重 + QE 范围参与积分范围计算 + 被积函数改为 `S(λ)·T(λ)·Q(λ)·λ`；`prepare_filter_cache` 中: QE Akima 插值重采样到光谱网格 + `weighted_wl[i] = λ_i × T(λ_i) × Q(λ_i)`；QE 为 nullptr 时 Q=1.0 并 LOG_WARNING `"QE curve not provided, F_syn without Q(λ)"`
- `cpp/include/photometric_calib.h` - `pc_calibrate_simple` / `pc_calibrate_simple_with_gaia` 签名均扩展: 在 filter 参数后新增 `const double* qe_wl, const double* qe_trans, int qe_count`（向后兼容: 传 nullptr 即可）
- `cpp/src/pc_api.cpp` - `pc_calibrate_simple` 签名同步扩展（标记 `(void)qe_wl; (void)qe_trans; (void)qe_count;` 因为此接口不在 DLL 内部计算 F_syn）；`pc_calibrate_simple_with_gaia` 签名同步扩展，`prepare_filter_cache` 调用传入 QE
- `lib/orchestrator/cpp/src/orchestrator.cpp` - 新增 `load_qe_curve`（委托给 `load_filter_curve`，JSON 格式完全一致）+ `extract_qe_curve_name`（从 stage1_config.json 文本解析 "qe_curve" 字段值，简单字符串扫描无 nlohmann::json 依赖）；`run_stage_photometric` 中: 加载滤光片曲线后加载 QE 曲线（路径 `lib/photometric_calib/data/response_curves/qe_curves.json`）+ 函数指针类型签名新增三个 QE 参数 + 函数调用传入 `qe_wl.empty() ? nullptr : qe_wl.data()` 等

配置支持:
- `lib/orchestrator/configs/stage1_config.json` 已包含 `"frame": { "qe_curve": "GSENSE2020BSI" }`
- `lib/photometric_calib/data/response_curves/qe_curves.json` 格式: `{"<name>": {"name":"...", "channel":"Q", "wavelength_nm":[...], "value":[...]}}`（**数组键名是 "value" 不是 "qe"**，与 filters.json 一致，故 load_qe_curve 可直接复用 load_filter_curve）

**GAP-013 修复（亮度比例 IRLS + Tukey）**:

修改文件:
- `cpp/src/star_matcher.h` - `StarMatch` 结构新增 `gaia_mag` 字段；`matchAndClean` 签名变更: `outlier_sigma` → `mag_tolerance`，新增 `out_scale_factor` 出参；私有方法重命名 `matchBruteForce` → `matchWithKdTree`，`cleanOutliers` → `cleanAndScale`
- `cpp/src/star_matcher.cpp` - **完全重写**:
  - 自实现 `KdTree2D` 内部类（替代 nanoflann，因 `lib/healpix_db/healpix_stack/gradient/nanoflann.hpp` 实际不存在）: 递归分裂构建 + 最近邻查询
  - `matchWithKdTree`: WCS 投影 Gaia 星到像素坐标 → 对 Gaia 星建 KD-tree（避免每颗 PSF 星扫描全部 Gaia 星）→ 对每颗 PSF 有效星查询最近邻（距离 < match_radius_px，默认 2.0px 收紧）
  - `cleanAndScale`:
    1. 星等一致性预过滤: `delta_i = -2.5*log10(F_instr_i) - gaia_mag_i`（粗略零点差），`median_delta` 作为粗略零点，拒绝 `|delta - median_delta| > mag_tolerance`（默认 3.0 mag）
    2. IRLS + Tukey biweight 迭代: `r = log10(F_instr/F_syn)`，`S = MAD(r_consistent)/0.6745`，`c = _TUKEY_C * S`（`_TUKEY_C = 4.685` 标准稳健统计常数），权重 `w = (1-u²)²`（u=r/c，|u|>=1 时 w=0），迭代最多 50 次（`_IRLS_MAX_ITER = 50`），收敛阈值 `|new_location - prev_location| < 1e-6`（`_IRLS_CONVERGE`）
    3. `scale = 10^(-location)`，`sigma_residual = MAD(r_inliers)/0.6745`，输出亮度比例一致性日志
- `cpp/src/pc_api.cpp` - `matchAndClean` 调用更新: `3.0/3.0` → `2.0/3.0`（match_radius/mag_tolerance），新增 `&scale` 参数；移除 `ImageCorrector::computeScale` 调用（scale 现由 IRLS 直接输出，不再走 median 回退路径）

**关键常量**（star_matcher.cpp 文件顶部）:
```cpp
static constexpr double _TUKEY_C = 4.685;        // Tukey biweight 标准常数
static constexpr int    _IRLS_MAX_ITER = 50;     // IRLS 最大迭代次数
static constexpr double _IRLS_CONVERGE = 1e-6;   // IRLS 收敛阈值
```

**编译验证**:
- `lib/photometric_calib/cpp/build.ps1`: photometric_calib.dll **1056.5 KB**（较 v2.0 的 1031.2 KB 增加 25.3 KB，主要来自 KD-tree + IRLS 代码），编译零警告零错误
- `lib/orchestrator/cpp/Makefile`: orchestrator.exe **3878.7 KB**，编译零警告零错误

**向后兼容性**:
- QE 参数可为 nullptr（`pc_calibrate_simple` 不在 DLL 内计算 F_syn；`pc_calibrate_simple_with_gaia` 收到 nullptr 时 Q=1.0 等价于无 QE）
- `out_scale_factor` 可为 nullptr（旧调用方不受影响）
- `mag_tolerance` 默认 3.0 mag，`match_radius_px` 默认 2.0px（替代原 `outlier_sigma=3.0` 暴力匹配阈值 3.0px）

**待验证**（未在本任务范围内执行）:
- 端到端测试（待用户运行 stage1 验证）: 期望 n_matched 与 sigma_residual 在 Galaxy_Center 测试帧上保持合理（参考 v2.0 基线: n_matched=1527, sigma_residual=0.168mag, scale=7.13e-03）
- Python ctypes 包装层 `python/photometric_calib.py` 的 argtypes 需要同步扩展三个 QE 参数（若 Python 调用方需要使用新接口，否则可继续传 nullptr）
- `pc_calibrate_simple` 旧接口的 Python 调用方需同步更新签名（追加三个 nullptr 占位参数）

## 2026-07-18 GRADIENT_2D 模块归档
- **决策**: 用户审阅 PROJECT_OVERVIEW.md 后纠正——stage1 不做曲面拟合和图像亮度修正（那是 stage2 马赛克阶段的事），PSF 后只做测光坐标系校准（PHOTOMETRIC 已完成）。
- **操作**: 
  - lib/photometric_calib/cpp/gradient_2d/ 整目录归档到 lib/photometric_calib/archive/gradient_2d/
  - 保留全部代码（include/ + src/ + build.ps1），不删改文件内容
  - orchestrator 中删除 PipelineStageV2::GRADIENT_2D 枚举 + run_stage_gradient_2d 函数
  - stage1 重排为 7 节点：READ_FITS/CALIBRATE/PLATESOLVE/PSF/PHOTOMETRIC/SNR/DRIZZLE
- **保留原因**: stage2 马赛克阶段若需要曲面拟合可参考此实现（IRLS+Tukey+Ridge+LOOCV 算法本身正确，只是不应在 stage1 单帧预处理中做）
- **影响**: photometric_calib C API（pc_calibrate_simple_with_gaia）不受影响，仍正常提供 PHOTOMETRIC 阶段的测光坐标系校准功能
