# FIX-B 方差 / 逆方差链

## 目标

闭合 P0-02（Phase1 不产 variance/ivar）、P0-03（frame_snr 被实现成 5σ 深度对象且未写 HiPS 头）、P0-04（校准方差传播未接线）、P0-05（cosmetic 恒等 pass）、P0-06（cosmetic 修复不更新方差）、P0-07（noise_snr 未入根构建）。这是 mosaic 能做科学加权的前置。

## 修复内容

1. **P0-04 校准方差传播**：接线已实现的 `v6_calibration_covariance.cpp`，bias/dark/flat 校准链输出 V(y)/ivar；解决 `docs/science/CALIBRATION.md:144` 与插件/设计文档的冲突（研究后按变更 claim 订正文档）；暗场 K 缩放两分支都施加 K（M42 用 600s 暗场 K=0.5 的方差缩放必须正确）；
2. **P0-05/06 cosmetic 真实生效**：`p1_session.cpp:443-446` 传真实 master（不再传 NULL），坏点/热像素/宇宙线检测与插值实际执行；插值修复同步更新该像素方差（插值后方差增大，按插值系数传播，不能保持原方差假装无损）；
3. **P0-07 noise_snr 入构建**：根 `CMakeLists.txt:236-241` 取消注释并补全 target 注册，给出当初被排除的真实原因与现在的闭合证据；
4. **P0-03 frame_snr 归位**：
   - frame_snr 实现为**通量型未加权原始信噪比** `F_ref/σ_F`（标量，口径见订正后的 `07_noise_snr.md` §4.1 与 `UNIFIED_MODEL.md`），经 drizzle **写入 HiPS 文件头**，符合 `contracts/schemas/unified/frame_snr.schema.json`（`frame_snr_value`）；
   - 现有 5σ 极限深度对象（flux5_adu/m5_mag/zero_point_mag）保留为**独立量 depth_m5**，不得占用 frame_snr 键；
   - `stage2.cpp` 里把 frame_snr 当"帧 catalogue 相对质量权重中位数"的用法一并归位；
5. **P0-02 drizzle 产品带方差**：p1 节点注入 data 块同时注入方差块；HiPS 产品含 variance/ivar tiles（`p1_final.json` 的 `n_variance_tiles/n_ivar_tiles/uncertainty_available` 如实为正/true）；drizzle 的 IIDR 线性重建按 `C_out=R C_in Rᵀ` 传播相关噪声（Fruchter & Hook 2002），对角近似需注明条件；
6. 单位口径：BUNIT 与数值量级一致（HiPS 面元归一级别 ~1e11–1e13 不得标 ADU；研究后选择"如实标注面元归一口径"或"导出平面时转回物理通量"，自决并在文档冻结）。

## 研究前置

- 对照 SExtractor `Var(F)=Σ(σ_bkg²+F_pix/gain)`、SEP `σ²_sum=Σvar_pix·w²+Σ/gain`、SWarp `var_out=1/Σ(1/var_k)`、photutils CCD 方程（已对拍 <1%）核验方差链每一步；
- 相关噪声（drizzle 后像素相关）在叠加中的处理对照 Paper II / properimage，确认协方差是否可在本版用对角近似 + 文档声明，还是必须带协方差（此项若影响产品模型则上呈）。

## 验收门

- 合成数据：已知增益/读出噪声/天光下，输出 ivar 与理论方差一致（容差由 L1 Oracle 标定）；校准链各步方差可解析复算；
- cosmetic：注入坏点/热像素/宇宙线，被修复且修复点方差正确增大；传 NULL master 的路径删除或显式 fail-closed；
- HiPS 头含 frame_snr 标量且数值为 F_ref/σ_F（不是 5σ 深度）；depth_m5 独立存在；
- `weight_mode=2`（逆方差）不再 rc=2：有 ivar 时真实走逆方差路径；
- 新增正例 + 负例（缺增益、缺方差输入、NaN 污染），fail-closed；
- ctest 全绿，6 个 fixture-gated 用例随本任务解锁转执行。
