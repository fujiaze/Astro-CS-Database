# 梯度估算器模块开发记忆
模块名称: gradient_estimator
功能: 基于匹配星的 F_instr/F_syn 比值拟合空间缓变梯度曲面（乘性/加性）
算法依据: spec/photometric_calib_algorithm.md 第4节
架构依据: .trae/specs/restructure-photometric-calib/spec.md
开发记录:
- 2026-07-10: Task 5 合成数据端到端验证完成 (test_synthetic.py)
  - 3个测试: 梯度恢复+图像校正物理正确性 / 质量报告格式 / 残差CSV
  - 合成数据: 1024x1024 uint16, 30颗星(6x5网格), 已知梯度 M_true=10^(0.1+0.15x'+0.10y'), S_true=50+30y'
  - 验证结果: 3/3通过, 30颗星校正误差<0.3%, scale=1.000000, 乘性曲面精确恢复r_true, 加性曲面恢复S_true
  - 测试同时作为回归测试: 若 r 定义回退为 log10(F_syn/F_instr), 测试1会因校正误差>10%而失败
- 2026-07-10: 【重大bug修复】合成数据端到端验证发现 r 定义与校正公式符号不一致
  - 问题: 原设计 r=log10(F_syn/F_instr)=-log10(M_true), M_map=10^r=1/M_true,
    但校正公式 I_cal=(I-S)/M_map 期望 M_map=M_true(渐晕因子), 导致 I_cal=I_star*M_true^2(错误)
  - 根因: 图像模型 I=I_star*M+S 中 M=渐晕因子, F_instr=I_star*M, F_syn=I_star,
    故 log10(M)=log10(F_instr/F_syn), 而非 log10(F_syn/F_instr)
  - 修复: 将 r 定义改为 log10(F_instr/F_syn), 使 M_map=10^r=M_true, 校正 I_cal=(I-S)/M=I_star(正确)
  - 修改文件: estimator.py(r_arr/r_obs定义+自测f_instr计算+注释), star_matcher.py(clean_outliers的r定义+注释),
    image_corrector.py(注释)
  - 验证: test_synthetic.py 合成数据1024x1024+30星, 修复前校正误差0.1%~152.6%/scale=0.63,
    修复后校正误差<0.3%/scale=1.0, 乘性曲面精确恢复r_true=0.1+0.15x'+0.10y'
  - 现有自测全部通过: estimator 8/8, star_matcher 10/10, image_corrector 5/5
  - 注意: 旧版 lib/photometric_calib/python/star_matcher.py 仍有 r=log10(F_syn/F_instr), 如使用需同步修复
- 2026-07-10: 创建模块目录结构，从原单体架构拆分
- 2026-07-10: 重构 star_matcher.py 适配双程序架构。移除 sed_builder/synthetic_photometry 依赖，
  构造函数精简为 StarMatcher(log_dir=None)（无滤光片/QE参数）；match() 中 F_syn 直接取自
  gaia_stars 输入项的 f_syn 字段（由光谱积分器 JSON 提供），不再实时计算合成测光；
  bp_rp: 有 mag_bp/mag_rp 则计算，否则 0.0；GaiaStarPy 新增 f_syn 字段，保留 mag_bp/mag_rp 兼容。
  保留: StarMatch/KDTree匹配/饱和星过滤(status!=0)/MAD离群清洗(clean_outliers,match_and_clean)/to_arrays/_get/_setup_logger。
  验证 10/10 通过（含 dict+dataclass 输入、f_syn 来源、无依赖检查、bp_rp 两条路径）。
- 2026-07-10: 实现 FSynLoader (fsyn_loader.py)，加载光谱积分器 JSON 结果
- 2026-07-10: 实现 GradientEstimator 主程序 (estimator.py)，串联定标全流程
  - calibrate(): PSF(可选内置)->StarMatcher.match_and_clean->退化判定(<6星)->to_arrays->
    r=log10(F_instr/F_syn)->GradientFitter.fit_multiplicative/fit_additive->ImageCorrector.correct_and_normalize->质量报告+残差CSV
  - 退化路径: n_matched<6 返回恒等曲面(order=1,coeffs=0)+scale=1.0+原图float32
  - _detect_and_fit_psf(): 惰性导入 star_detector/dynamic_psf (sys.path 回溯3级), 不影响模块 import
  - _build_quality_report(): n_matched/n_excluded/n_used/mult_*/add_*/scale_factor (n_excluded 经 self._n_excluded 传递)
  - _save_residuals_csv(): mult/add_residuals.csv, 复用 fitter._build_design_matrix+_irls_fit 保证权重一致
  - 设计: r=log10(F_instr/F_syn)=log10(M_true), M=10^r=M_true(渐晕因子), I_cal=(I-S)/M=I_star, F_cal=F_instr/M=I_star, scale=median(F_syn/F_cal)=1.0
  - 验证 8/8 通过 (import干净/30星端到端/字段类型/乘性系数恢复0.10+0.05x/加性恢复100+30y/质量报告/残差CSV/退化路径)
- 2026-07-10: 实现命令行入口 run_estimator.py
  - argparse参数: --image(必填) --fsyn(必填) --output(calibrated.fits) --report(quality_report.json)
    --residual-dir(logs) --match-radius(3.0) --outlier-sigma(3.0) --max-order(5) --wcs-json(可选)
  - build_wcs(image_data, wcs_json_path): 三分支 - has_wcs从FITS头构造 / 无WCS+json从JSON加载 / 无WCS无json抛ValueError
  - 主流程: ImageReader.read -> FSynLoader.load -> build_wcs -> GradientEstimator.calibrate -> FITSWriter.write -> 质量报告JSON -> 残差CSV(estimator内部)
  - 依赖: astro_image_io(sys.path回溯3级), 同目录 fsyn_loader/estimator/wcs_transform
  - 验证: 7/7通过 (缺--image/缺--fsyn报错, 默认值, build_wcs三分支含ValueError, 主流程build_wcs步骤)

子模块:
### FSynLoader - F_syn 结果加载器 (python/fsyn_loader.py)
- **入口**: `from lib.photometric_calib.gradient_estimator.python.fsyn_loader import FSynLoader`
- **API**:
  - `load(json_path) -> list[dict]`: 加载星列表，每项含 ra/dec/mag_g/f_syn/source_id
  - `load_with_metadata(json_path) -> (metadata, stars)`: 返回元数据(filter_name/qe_name/n_stars/ra_center等)+星列表
- **处理规则**: 校验 "stars" 字段缺失抛 ValueError；过滤 f_syn<=0；类型转换 ra/dec/mag_g/f_syn=float, source_id=int
- **验证**: 4/4通过 (f_syn_galactic_center.json 5276星, F_syn范围[5.8e4,1.6e7]全正, filter_name="Baader R")

### GradientEstimator - 梯度估算器主程序 (python/estimator.py)
- **入口**: `from lib.photometric_calib.gradient_estimator.python.estimator import GradientEstimator`
- **构造**: `GradientEstimator(log_dir=None, match_radius_px=3.0, outlier_sigma=3.0, max_order=5)`
- **API**:
  - `calibrate(image, gaia_stars, wcs_transform, psf_results=None) -> dict`: 完整定标流程
    - 返回: image_calibrated(float32)/mult_surface/add_surface/scale_factor/n_matched/n_excluded/quality_report
  - `_detect_and_fit_psf(image) -> list`: 内置 StarDetector.detect + DynamicPSF.fit_batch (惰性导入)
  - `_build_quality_report(matches, mult, add, scale) -> dict`: 质量报告
  - `_save_residuals_csv(matches, mult, add, fitter, output_dir)`: mult/add_residuals.csv
- **依赖**: 同目录 star_matcher/gradient_fitter/image_corrector; 内置PSF路径额外 star_detector/dynamic_psf
- **关键点**: 匹配星<6退化; r=log10(F_instr/F_syn)=log10(M_true); 残差CSV复用fitter私有方法保证IRLS权重一致; n_excluded经self._n_excluded传递
- **验证**: 8/8通过 (合成30星已知梯度, 乘性恢复0.10+0.05x, 加性恢复100+30y, 退化路径scale=1.0)

### Run Estimator CLI - 梯度估算器命令行入口 (python/run_estimator.py)
- **入口**: `python run_estimator.py --image img.fits --fsyn f_syn.json --output calibrated.fits`
- **参数**: --image(必填,FITS/XISF) --fsyn(必填,F_syn JSON) --output(默认calibrated.fits) --report(默认quality_report.json)
  --residual-dir(默认logs/) --match-radius(默认3.0) --outlier-sigma(默认3.0) --max-order(默认5) --wcs-json(可选,plate_solve结果)
- **build_wcs**: 图像有WCS->从FITS头(crpix/crval/cd)构造; 无WCS+--wcs-json->从JSON加载; 无WCS无json->ValueError
- **主流程**: ImageReader.read -> FSynLoader.load -> build_wcs -> GradientEstimator.calibrate -> FITSWriter.write(校正图) -> JSON(质量报告) -> 残差CSV(estimator内部写log_dir)
- **依赖**: astro_image_io(ImageReader/FITSWriter, sys.path回溯3级); 同目录 fsyn_loader/estimator/wcs_transform
- **验证**: 7/7通过 (parse_args必填校验/默认值, build_wcs三分支含ValueError, 主流程build_wcs步骤)
