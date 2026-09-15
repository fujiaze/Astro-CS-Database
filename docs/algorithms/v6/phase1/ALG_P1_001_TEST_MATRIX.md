# ALG-P1-001 测试矩阵（Phase1 算法实施规格）

- 文档 ID：`ALG-P1-001-TEST-MATRIX`
- 语义源（唯一权威）：`alg_p1_001_test_matrix.json`（本文件为机械渲染视图，保证人读正文与机器表一致）
- 上位规格：`ALG_P1_001_PHASE1_ALGORITHM_SPEC.md`；验证器：`tools/verify_alg_p1_001.py`
- 基线：`125bc0999363be1a42a1f2df3254601e0cc7b8fb`
- 容差政策：仅沿用已冻结数值容差（常量场 |S_p/B0-1|<1e-3、缩放律精确、GLS rel<3% 类）。本规格提案但未签字的阈值一律标 PENDING_SO07，门在未签字时 fail-closed（不得声明精确/可用）。

## 测试行（34 项）

| ID | 域 | 层级 | 输入 | 期望 | 门 | 条款锚 |
|---|---|---|---|---|---|---|
| `T-CAL-01` | calibration_covariance | oracle | 合成 n_frames x n_pix（解析 J=[1/f,-(1-alpha)/f,-alpha/f,-y/f]；对角独立项） | V(y_p) 与 J C_in J^T 逐像素一致；rel < 1e-12 | `CAL-COV-FORMULA` | `DESIGN-P1 §4.2;  ADJ-OBS-01` |
| `T-CAL-02` | calibration_covariance | oracle | 同一 bias master_id 出现在 light-path 与 dark-path | 系数合并为 (1-alpha)^2，不是 V_b+alpha^2 V_b（后者仅不同 master_id） | `CAL-COV-FORMULA` | `ADJ-OBS-01;  DESIGN-P1 §4.2` |
| `T-CAL-03` | calibration_covariance | oracle | 含共同 master 模式的帧组 + 点源组合系数 c_k | Var_joint / Var_naive_independent > 1（检测量）；朴素求和被拒 | `ADJ-OBS-01-SHARED` | `ADJ-OBS-01;  ADJ-F-OBS-04` |
| `T-CAL-04` | calibration_covariance | unit | variance / signal / ivar / W_info 声明 | variance=signal^2; ivar=1/variance; W_info=signal^-2 | `CAL-UNIT` | `FZ-UNIT-*;  CONSTITUTION §4.1` |
| `T-CAL-05` | calibration_covariance | boundary | gain 或 read_noise 缺失；f_p<=0；master_id 缺失 | fail-closed unavailable/REJECT，不静默造值 | `CAL-COV-FORMULA` | `DESIGN-P1 §2;  ADJ-OBS-01` |
| `T-CAL-06` | calibration_covariance | property | 含负像素与 <floor 平场的合成帧 | 输出保留负值；不加 pedestal；不夹紧（与 SCI-CAL-001 §9a 一致） | `CAL-NO-CLIP` | `CONSTITUTION §5.3;  SCI-CAL-001 §9a` |
| `T-CAL-07` | calibration_covariance | structural | 共享项表示枚举 | ∈ {lowrank, correlation_kernel, common_master_id}；缺失即红 | `CAL-COV-REPRESENTATION` | `FZ-PROV-SHARED-SYSTEMATIC` |
| `T-WINFO-01` | psf_information | oracle | 归一 PSF P + 对角 C=sigma^2 I + a | W = a^2 sum P^2/sigma^2 == a^2/(sigma^2 A_NEA) | `FZ-COND-WHITENOISE` | `SCI-PSFW-001 §2;  ADJ-P2-01` |
| `T-WINFO-02` | psf_information | oracle | 归一 PSF P + 一般 SPD C + 注入 d | F_hat = Q/W == GLS 解；Var(F_hat)=1/W=c^T C c | `FZ-FORMULA-WINFO` | `UNIFIED §4;  ADJ-P2-01` |
| `T-WINFO-03` | psf_information | property | 同一图像改变源亮度分布/检测阈值 | W_info 不变；median(source SNR) 改变 | `FZ-GATE-MEDIAN-SNR` | `DESIGN-P1 §11;  ADJ-C004-02` |
| `T-WINFO-04` | psf_information | oracle | a -> 2a | W(2a)/W(a) = 4（a^2 律） | `FZ-FORMULA-WINFO` | `FZ-FORMULA-WINFO` |
| `T-WINFO-05` | psf_information | boundary | 只给对角 C~ 但真实 C 含相关 | 报告 c~^T C c~ 与相对理想 1/W 的偏差；不得直接报 1/W | `FZ-FORMULA-WINFO` | `FZ-FORMULA-COV-PROP;  PROJECT_SPEC §3` |
| `T-WINFO-06` | psf_information | structural | W_info 来源字段 | 不得来自 median_snr/support/coverage/fwhm/residual/drizzle-ivar-only | `FZ-GATE-MEDIAN-SNR` | `FZ-GATE-MEDIAN-SNR;  ADJ-GEN-02` |
| `T-WINFO-07` | psf_information | boundary | 标量降级（帧级 W） | 双门（空间残差/趋势 + 功率损失）+ p05/p50/p95 + 最大系统偏差 + 采样覆盖 + 模型误差 + 适用域；否则 map/model | `FZ-DEGRADE-SCALAR` | `FZ-DEGRADE-SCALAR;  ADJ-GEN-04` |
| `T-PSFW-01` | psfsw_components | structural | 四分量记录 | signal/concentration/noise/background 齐备；measurement_id 互异 | `FZ-FIELD-PSFSW-4COMP` | `FZ-FIELD-PSFSW-4COMP;  ADJ-P2-03` |
| `T-PSFW-02` | psfsw_components | oracle | 组内 Wt_k | W_psfsw = Wt/median(Wt) 组内 median=1 且全正 | `FZ-FORMULA-PSFSW-COMPOSITE` | `FZ-FORMULA-PSFSW-COMPOSITE` |
| `T-PSFW-03` | psfsw_components | property | 单变量扫描 S/Conc/N/B | Wt 对 S、Conc 单调增，对 N、B 单调减 | `FZ-FORMULA-PSFSW-COMPOSITE` | `PSFW_FREEZE_RESEARCH §4.3` |
| `T-PSFW-04` | psfsw_components | structural | weight_kind/units/scope | relative_dimensionless / 1 / group；units 含 flux^-2/ivar 即红 | `FZ-FIELD-PSFSW-UNIT` | `FZ-FIELD-PSFSW-UNIT` |
| `T-PSFW-05` | psfsw_components | structural | covariance 来源字段 | propagated_from_composite_coefficients; variance_from_weight=false; effective_psf_id 非空 | `FZ-GATE-PSFSW-COV` | `FZ-GATE-PSFSW-COV;  FZ-GATE-PSFSW-EPSF` |
| `T-PSFW-06` | psfsw_components | boundary | 无共同星集/背景非正/星不足/选择偏差失败/非均匀 | valid=false, weight_value=null, reason ∈ 白名单；不回退 median SNR | `FZ-GATE-PSFSW-FAILCLOSED` | `FZ-GATE-PSFSW-FAILCLOSED` |
| `T-PSFW-07` | psfsw_components | structural | psfsw 产物键集合 | 任何层不含 ivar/variance/fisher/w_info 等禁止键 | `FZ-GATE-PSFSW-NOKEYS` | `FZ-GATE-PSFSW-NOKEYS;  ADJ-GEN-02` |
| `T-PSFW-08` | psfsw_components | property | 共同星集构造路径 | 独立于待测帧测量（外部星表/参考叠加单一门限）；逐帧检测交集即红 | `FZ-FIELD-PSFSW-4COMP` | `SCI-PSFW-001 §3;  PSFW_FREEZE_RESEARCH §5.2` |
| `T-PSFW-09` | psfsw_components | interface | Phase1 单帧产品 | 输出四分量 + 未归一 Wt + 归一契约(scope=group, median_target=1) + validity；不输出单帧 median 伪归一权重 | `FZ-FIELD-PSFSW-4COMP` | `DESIGN-P1 §8.2/§10;  OI-01` |
| `T-DRZ-01` | drizzle | oracle | 常量面亮度 x_j=B0*A_pixel_j，pixfrac ∈ {0.25,0.5,0.8,1.0} | |S_p/B0 - 1| < 1e-3（全部 pixfrac；容差沿用，不改数值） | `FZ-GATE-CONST-SB` | `FZ-GATE-CONST-SB;  ADJ-S3` |
| `T-DRZ-02` | drizzle | oracle | 任意场 + 全覆盖几何 | Phi_out = sum_p S_p D_p = pixfrac^2 * sum_j x_j（精确） | `FZ-COND-FLUX-CONSERV` | `FZ-COND-FLUX-CONSERV;  ADJ-F-OBS-02` |
| `T-DRZ-03` | drizzle | oracle | x -> alpha x | variance -> alpha^2 variance；ivar/alpha^2；逐像素精确 | `FZ-FORMULA-DRIZZLE-VAR` | `FZ-FORMULA-DRIZZLE-VAR` |
| `T-DRZ-04` | drizzle | oracle | 稀疏随机 footprint（输入像素独立） | diag(C_out) == sum_j c_jp^2 v_j（rel<1e-11）；Cov(S_p,S_q)=sum_j c_jp c_jq v_j | `FZ-FORMULA-COV-PROP` | `SCI-OBS §5.1;  ADJ-AR-02` |
| `T-DRZ-05` | drizzle | oracle | aperture 权重 a_p（非负） | 精确 aperture 方差 > 对角-only 求和（严格，权重非负）；对角-only 不得声明精确 | `FZ-FORMULA-COV-PROP` | `ADJ-AR-02;  FZ-GATE-PARENT-VAR` |
| `T-DRZ-06` | drizzle | structural | 父级 tile variance 归约 | lower_bound=true + 相关核/算子摘要 + deficit 门；否则 variance 面 unavailable | `FZ-GATE-PARENT-VAR` | `ADJ-F-OBS-03;  FZ-GATE-PARENT-VAR` |
| `T-DRZ-07` | drizzle | boundary | pixfrac 非法 / RING / 多通道 / 缺 WCS | 显式拒绝，不夹逼（ALG-DRZ-001） | `FZ-GATE-CONST-SB` | `SCI-DRZ-001 §4/§8;  ALG-DRZ-001 §5` |
| `T-DRZ-08` | drizzle | unit | 写盘 BUNIT | signal 与 variance 满足二次律；单位不可判 -> unavailable/REJECT | `G-STRUCT-UNIT-LAW` | `FZ-BUNIT-SEMANTICS;  FZ-P3-BUNIT-QUADRATIC` |
| `T-X-01` | cross | structural | 生产权重模式枚举 | {point_information, surface_gls, psfsw_robust}；psf_snr_power/legacy 0/auto/support_x_snr2 出现即红 | `G-MODE-PRODUCTION` | `FZ-MODE-PRODUCTION;  C-004.1;  ADJ-S1` |
| `T-X-02` | cross | structural | 冻结条目继承集 | inherited_frozen_ids 不得缺失任一条 | `G-STRUCT-FREEZE-INHERIT` | `SCI-ADJ-001 FREEZE_LIST §4` |
| `T-X-03` | cross | structural | 禁止权重来源 token | token 集完整；出现在 weight.sources/weight_value/covariance.variance_from 即红 | `G-STRUCT-FORBIDDEN-TOKENS` | `FZ-GATE-MEDIAN-SNR;  FZ-GATE-SUPPORT-COVERAGE` |

## 层级说明

- `unit`：单函数数值/单位；`oracle`：独立解析或高精度重推导；`property`：不变量/单调性；
- `boundary`：退化/非法输入 fail-closed；`structural`：合同字段一致性；`interface`：跨 Phase 接口拆分。

## 负向 mutation

每条门必须有至少一个 mutation 能把门变红；mutation 清单与期望见验证器 `MUTATIONS`（39 项），
运行 `python3 tools/verify_alg_p1_001.py --all-mutations` 要求 39/39 CAUGHT（rc=0）；单条 `--mutation <ID>` 在捕获时 rc=2。

## 声明

本矩阵与规格同属 `ALG-P1-001` 交付，未改任何冻结公式/容差/门；数值阈值中的未签字项标 `PENDING_SO07`。
