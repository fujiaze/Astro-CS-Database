# 光谱积分器模块开发记忆
模块名称: spectrum_integrator
功能: 使用 Gaia DR3SP 真实 BP/RP 光谱计算合成流量 F_syn
算法依据: spec/photometric_calib_algorithm.md 第3节
架构依据: .trae/specs/restructure-photometric-calib/spec.md
开发记录:
- 2026-07-10: 创建模块目录结构，从原单体架构拆分
- 2026-07-10: 实现 integrator.py 主程序 SpectrumIntegrator 类
  - integrate_star: uint8[343]->float64 作 S(λ), 调 SyntheticPhotometry.compute 积分
  - integrate_batch: 批量积分返回 list[dict](source_id/ra/dec/mag_g/f_syn), 每100颗输出进度
  - save_results/load_results: JSON 读写, 含 filter_name/qe_name/wl_step/spectrum_source/中心坐标
  - spectrum_wl 首次积分自动缓存; 无 QE 时 Q=1
  - 验证: 6/6通过 (Gaia银心165星取10颗/Baader R 572-716nm/F_syn全正/批量10项/JSON读写一致)
