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
