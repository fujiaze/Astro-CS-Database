# Photometric Calib 模块记忆

> 本文件记录 photometric_calib 模块的开发进度与关键信息。
> 根目录索引: [memory.md](file:///F:/Astro%20dev/Astro%20CS%20Normalization%20Database/memory.md)

## 模块概览

- **模块名称**: photometric_calib
- **功能**: 鲁棒流量校准，消除空间缓变梯度（残留渐晕、月光、光害、大气消光、大气辉光），输出与 Gaia DR3/SP 星表系统一致的校正图像
- **算法依据**: [spec/photometric_calib_algorithm.md](file:///F:/Astro%20dev/Astro%20CS%20Normalization%20Database/spec/photometric_calib_algorithm.md)
- **架构依据**: [spec/photometric_calib_architecture.md](file:///F:/Astro%20dev/Astro%20CS%20Normalization%20Database/spec/photometric_calib_architecture.md)

## 目录结构

```
lib/photometric_calib/
├── python/             # Python 接口（旧版单体架构，大部分已迁移）
│   ├── __init__.py
│   ├── pc_logger.py          # 日志系统
│   ├── sed_builder.py        # SED构造器 (保留：spectrum_integrator 自测惰性引用)
│   └── synthetic_photometry.py  # 合成测光 (保留：spectrum_integrator 自测惰性引用)
│   # 已删除（2026-07-12）: star_matcher.py/image_corrector.py/gradient_fitter.py/wcs_transform.py/curve_loader.py
│   # 新版位于 gradient_estimator/python/ 和 spectrum_integrator/python/
├── gradient_estimator/  # 梯度估算器（活跃版本，已修复梯度方向 bug）
├── spectrum_integrator/ # 光谱积分器（活跃版本）
└── logs/               # 日志输出目录
    └── calib_YYYYMMDD_HHMMSS.log
```

> **清理记录（2026-07-12）**: 顶层 python/ 下的 star_matcher.py、image_corrector.py 含已确认梯度方向 bug（r=log10(F_syn/F_instr) 方向反转），gradient_fitter.py 未调参（MAX_ORDER=5），wcs_transform.py/curve_loader.py 为冗余副本。上述 5 个文件已删除，新版位于 gradient_estimator/python/ 和 spectrum_integrator/python/。sed_builder.py 和 synthetic_photometry.py 保留（spectrum_integrator/python/synthetic_photometry.py 自测块惰性引用 sed_builder，跨目录依赖未解耦）。

## 开发记录

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
