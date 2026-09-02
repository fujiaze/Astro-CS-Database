# 测光修正规范

Gaia DR3SP 解析、响应曲线读取和合成积分为独立阶段；`F_syn` 有效只证明参考流量存在。完整测光还需：标准 WCS 投影、图内筛选、唯一空间匹配、质量筛选、稳健比例拟合和残差统计。

CLI/报告必须暴露：

- spectrum_rows_total / valid_fsyn；
- gaia_projected_in_frame；
- psf_valid；
- spatial_candidates；
- unique_matches；
- rejected_ambiguous / rejected_distance / rejected_quality；
- fit_used；
- scale_factor、sigma_residual、robust iterations；
- 匹配距离 median/p90/max。

建议先使用空间 KD-tree/网格索引，双向最近邻与唯一配对，匹配半径由 WCS 闭环误差和 PSF FWHM确定，不得盲目扩大到掩盖坐标错误。

最低 Gate：Broadband/LRGB 每帧 fit_used ≥ 20；窄带每帧 ≥ 8。若某类真实数据科学上无法达到，必须通过分布和 ADR 修改门限，不能单帧特判。`sigma_residual` 必须有限且 >0。
