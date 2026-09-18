# FIX-C 集成链（逆方差叠加成为默认路径）

## 目标

闭合 P0-11（integration 仅加权均值、无 variance/covariance）、P0-12（sparse_snr_layer 缺失）、P0-13（weight_mode 用 legacy 整数默认 2）。依赖 FIX-A 天光面与 FIX-B 逐帧 ivar。

## 修复内容

1. **P0-11 科学叠加**：生产链从 `signal=Σwv/Σw` 升级为 point_information 逆方差叠加：每帧保留自身 PSF 核（不预先均质化），`Q_k=a_k P_kᵀC_k⁻¹d_k`、`W_k=a_k²P_kᵀC_k⁻¹P_k`、`F_hat=ΣQ_k/ΣW_k`、`Var=1/ΣW_k`（Zackay & Ofek Paper I 结构，v6 已有实现，接入 CLI 主链）；输出产品含 variance（对角）或 covariance（按 FIX-B 研究结论）；
2. **逐像素按需计算，不建稠密权重面**：叠加分块进行，每个天球像素消费多个源像素时现场取该像素 SNR/ivar 算权重（最小单元一个像素）；内存占用不随全图像素数无界增长；
3. **P0-12 稀疏帧内 SNR 层**：Phase1 可选产出 sparse_snr_layer（标准层入 HiPS）；Phase2 自动检测——有则将帧级×帧内权重重建为稠密参与运算（按需求值），无则整帧用帧级权重；启用/不启用两条路径都有测试；
4. **P0-13 weight_mode canonical 词表**：废弃 legacy 整数（现默认 "2"），改用 `docs/contracts/v6/frozen/02_WEIGHT_MODE_VOCABULARY.md` 的规范词表（inverse_variance / point_information / psfsw_robust / equal 等），`session_commands.h:133` 默认改为规范默认值；canonical 字段在 lib/ 真实消费；`legacy_allow_weight_fallback` 等权降级仅在显式指定时可用，缺 ivar 时默认 fail-closed（不再静默等权）；
5. RELEASE-01 交付帧的 `weight_mode=1` 显式等权降级路径在本任务后不再是默认。

## 研究前置

- 用合成数据对比三种叠加的理论方差：等权、逐像素逆方差（SWarp 式）、point_information（Paper I 式），验证 point_information 在不同 PSF/噪声帧混合时方差最小；
- psfsw_robust 复合权重（α=2,β=1,γ=2,δ=1）在 L1 标定前保持显式可选、不默认；标定记录缺失时该模式 fail-closed 或明确标注实验性（自决并留痕）。

## 验收门

- 合成数据：混合不同 PSF/增益/天光/曝光的帧，point_information 结果的实测散度与 `Var=1/ΣW` 一致且小于等权结果；
- 输出 variance_available=true、ivar_available=true（对照 RELEASE-01 的 false）；
- sparse_snr_layer 启用时结果相对帧级-only 有可测的方差改善（合成数据上给出量化）；
- weight_mode 全词表有正例；非法值 rc=2；缺 ivar 默认阻断；
- 1 worker vs N worker 逐字节一致（除路径报告）；
- 新增 Oracle 与负例测试。
