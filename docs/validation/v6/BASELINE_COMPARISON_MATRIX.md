> **DOC-001 溯源注记（2026-09-16）**：本文为 V6 产品族冻结/设计档案（上一轮治理产物），因仍被活动合同引用而保留在活动索引；文中 工程控制/旧 V6 控制包（ROOT-007 已删除）/** 等旧控制包路径为该轮任务溯源，该控制包已由 ROOT-007 删除，不作现状引用。

# 基线比较矩阵（QA-MATRIX-001）

> 上游：ASTROCS_DESIGN.md §12（验证体系）

- 机器规格：`reports/v6/qa-design/data/baseline_matrix.json`；渲染块由 `render_docs.py` 机械写入，`check_docs.py` 校验逐字一致。
- 上位锚：`FZ-MODE-PRODUCTION`/`FZ-MODE-BASELINE`/`FZ-MODE-DEFERRED`、`ADJ-S1`、`ADJ-P2-01/02/03`、
  `PSF_SIGNAL_WEIGHT.md` §4/§7、`DESIGN-P2-001` §6.3、控制器 C-004.1。

## 1. 目的

把"哪个科学目标、用哪个统计量、相对哪个基线、能声明什么"写成**预注册可比较**的矩阵，
并强制声明边界：psfsw_robust 只能声明"在指定验收数据上优于指定基线"，不得冒充 ivar/Fisher；
equal/pixel_ivar 仅为文档基线，不得声明科学最优；psf_snr_power 本包保持延迟。

## 2. 比较度量（冻结集合）

<!-- BASELINE-METRIC-NOTE -->
度量集合与方向由 `baseline_matrix.json.metrics` 冻结：M-DET（点源检测功率）、M-VAR（测光方差）、
M-FBIAS（通量偏差）、M-SBBIAS（面亮度偏差）、M-EPSF（由有效 PSF 脉冲响应测量的 FWHM）、
M-ART（接缝/伪影/黑洞清单）、M-NOISE（预测/实测噪声比）。数值阈值与效应量属预注册（W10），本任务不发明。

## 3. 模式表（机读渲染）

<!-- BASELINE-MODES-BEGIN -->
| mode | class | 权重对象 | 单位 | 权威式 | covariance 来源 | effective PSF | 组内归一 | 可声明 | 禁止声明 |
|---|---|---|---|---|---|---|---|---|---|
| `point_information` | production | W_info | ADU^-2 | `Q_k=a_k P_k^T C_k^-1 d_k; W_info,k=a_k^2 P_k^T C_k^-1 P_k; F_hat=Q/W; Var=1/W` | combination_coefficients | required | False | 模型/C 门通过时可声明 BLUE/最大点源 SNR/最小通量方差 | support/coverage/median_source_snr/fwhm/residual 作权重；对任意 PSF 的像素 ivar 等价 |
| `surface_gls` | production | A^T C^-1 A | 1/(BUNIT^2) | `x_hat=(A^T C^-1 A)^-1 A^T C^-1 d; Cov=(A^T C^-1 A)^-1` | combination_coefficients | required | False | GLS 假设成立时可声明 Gauss-Markov BLUE | 像素 ivar 无条件最优；无 Var_approx/Var_GLS<=1+epsilon 误差门声明 |
| `psfsw_robust` | production | psfsw_robust_weight | 1 | `Wt_k=C_norm*S^alpha*Conc^beta/(N^gamma*B^delta); W_psfsw,k=Wt_k/median_j(Wt_j)` | combination_coefficients | required | True | 只能声明在指定验收数据上优于指定基线 | ivar；fisher_information；variance_from_weight；1/W_psfsw；无共同星集回退 median source SNR |
| `equal` | baseline | unit_weight | 1 | `I_out=mean_k d_k` | combination_coefficients | required | False | 无（仅文档基线/对照） | scientific optimality |
| `pixel_ivar` | baseline | pixel_ivar | 1/BUNIT^2 | `I_out=Sum_k w_k d_k/Sum_k w_k, w_k=1/v_k` | combination_coefficients | required | False | 无（扩展源在同点采样+噪声独立退化为 GLS；点源对任意 PSF 不最优） | point-source optimality for arbitrary PSF |
| `psf_snr_power` | deferred | ratio_of_powers | 1 | `DEFERRED（未冻结）` | combination_coefficients | required | True | 不得进生产；不得声明 Fisher 最优 | production；fisher_optimality |
<!-- BASELINE-MODES-END -->

## 4. 比较单元（机读渲染）

<!-- BASELINE-CMP-BEGIN -->
| cell | candidate | baseline | metrics | declaration_limit | 预注册 | 容差来源 |
|---|---|---|---|---|---|---|
| CMP-01 | `point_information` | `equal` | M-DET, M-VAR, M-FBIAS, M-EPSF | candidate 可声明最优（前提满足）；不得据此声明 equal 非法 | True | 预注册（W10 冻结数值） |
| CMP-02 | `point_information` | `pixel_ivar` | M-DET, M-VAR, M-EPSF | candidate 可声明最小点源方差；不得声明像素 ivar 对任意 PSF 等价 | True | PSFW K4 loss_ratio>1.5（构造性） |
| CMP-03 | `point_information` | `psfsw_robust` | M-DET, M-VAR, M-EPSF, M-ART | 两者目标/单位不同，只报度量与偏差，不互称最优 | True | 预注册 |
| CMP-04 | `surface_gls` | `pixel_ivar` | M-VAR, M-SBBIAS, M-EPSF | 仅在同点采样+噪声独立+a_k 一致的声明域内比较 | True | FZ-GATE-PIXIVAR-APPROX（epsilon 待 ALG-P2-SURF-001 冻结） |
| CMP-05 | `surface_gls` | `equal` | M-VAR, M-SBBIAS | 基线对照，不声明 equal 非法 | True | 预注册 |
| CMP-06 | `psfsw_robust` | `equal` | M-DET, M-VAR, M-FBIAS, M-EPSF, M-ART | 只能声明在指定验收数据上优于 equal | True | 预注册（PSF_SIGNAL_WEIGHT §7.4/7.5） |
| CMP-07 | `psfsw_robust` | `pixel_ivar` | M-DET, M-VAR, M-EPSF, M-ART | 只能声明优于指定基线；不得写成 ivar/1/W_psfsw | True | 预注册 |
| CMP-08 | `psfsw_robust` | `point_information` | M-DET, M-VAR, M-EPSF | 不得声明 Fisher 最优；如含 seeing/concentration penalty 必须报告 P_eff 与相对 loss | True | 预注册 |
| CMP-09 | `equal` | `pixel_ivar` | M-VAR, M-FBIAS | 仅基线互比，二者均不得声明科学最优 | True | 预注册 |
| CMP-DEF | `psf_snr_power` | `none` | — | DEFERRED：本包不进生产，不参与比较结论；仅登记 | False | C-004.1 |
<!-- BASELINE-CMP-END -->

## 5. 声明规则（机读渲染）

<!-- BASELINE-RULES-BEGIN -->
- psfsw_robust 只允许 '在指定验收数据上优于指定基线'；禁止 ivar/Fisher/1/W_psfsw 语义。
- pixel_ivar/equal 为文档基线，禁止科学最优声明。
- psf_snr_power 在本包（V6）保持 DEFERRED，不得进入生产路由（C-004.1）。
- 训练/调参样本不得与最终验收样本相同（PSF_SIGNAL_WEIGHT §3）。
- 真实数据比较必须配解析/MC/外部参考真值，不得以生产输出为唯一 expected（PROJECT_SPEC §8）。
<!-- BASELINE-RULES-END -->

## 6. 判读纪律

1. 每个比较单元必须预注册（数据、基线、度量、效应量与 CI），训练/调参样本不得与验收样本相同。
2. 相关帧必须用联合 C；psfsw 的 covariance 只能从实际组合系数传播（禁 1/W_psfsw）。
3. 报告必须包含 effective PSF 与相对基线的 detection power/photometric variance/FWHM/通量偏差/面亮度偏差。
4. 真实数据比较必须配解析/MC/外部参考真值（G-RD-04；PROJECT_SPEC §8）。
