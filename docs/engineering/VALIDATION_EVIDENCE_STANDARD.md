# 验证证据标准

> 上游：`docs/ASTROCS_DESIGN.md` §12（验证体系）
> 下级索引：`docs/detail/` 各模块分册的「独立 synthetic 验证命令与容差」一节

本标准规定每条科学判据与每道工程门禁的证据形态：独立 Oracle、零用例即红、负向 mutation 目录、基线比较矩阵与声明边界。

## 1. Oracle 独立性

1. 独立真值来源限于白名单：`independent_numpy`、`independent_numpy_mc`、`independent_stdlib`、`structural`、`real_data_checklist`、`preregistered_comparison`、`independent_driver`。
2. 每条判据必须声明 `oracle.truth`（真值来源）与 `oracle.must_not`（独立于被测对象的对象集，至少含产品可执行程序本身）。
3. Oracle 参考实现只用 NumPy 与标准库，参考实现为显式矩阵、解析恒等式与定种子 Monte Carlo；被测公式以独立转写的形式作为 subject 接受检验。
4. 断言面是值断言；子串存在性只作辅助。期望值由独立参考实现生成，与被测实现分离。
5. 真实数据判据的期望值取自独立真值，生产输出只作辅助证据。
6. 任何「Oracle 全过」的结论一律逐条判据重新实测，不接受历史结论沿用。

## 2. 零用例即红

判据台账逐条登记 `required_cases`、`executed_cases`、`skipped_cases`、`pends`、`counts_as_pass`，执行器按下列规则判退出码：

```text
executed_cases == 0 且非 pending             -> rc=2（不计通过）
skipped_cases >= executed_cases（skip-only） -> rc=2
pending 判据未声明 counts_as_pass=false 或无 owner -> rc!=0
executed_cases < required_cases              -> rc!=0
```

零用例与 skip-only 两类退化由判据校验器检出（rc=1），二者各配一条负例 mutation 证明该规则能红。

## 3. 负向 mutation 目录

每道判据必须配至少一条能红的 mutation，注入缺陷后目标判据至少一个 check 变红。机器目录是唯一事实源，位于实验单元的 qa-design 数据面；本节给出类别与摘要。

| 类别 | 注入点 | 断言 |
|---|---|---|
| `science` | Oracle 的被测 subject（解析、Monte Carlo、注入、基线、结构记录） | 目标判据至少一个 check 变红（rc=1），且每个目标判据都被覆盖 |
| `spec` | 判据规格的结构与合同字段 | 规格校验器报违规（rc=1） |
| `doc` | 本标准与各正本的人读渲染块 | 人读/机读一致性校验器报不一致（rc=1） |

### 3.1 关键 science mutation

| mutation | 注入 | 目标判据 |
|---|---|---|
| MUT-A01 | 广义最小二乘退化为普通最小二乘 | G-ANA-01 |
| MUT-A02 | 组合系数整体乘 1.10 | G-ANA-01/02 |
| MUT-A04 | 广义最小二乘的协方差忽略天光项 | G-ANA-04 |
| MUT-A05 | 白噪式取倒数 | G-ANA-05 |
| MUT-A06 | 单位表把信息权重记为 ADU^2 | G-ANA-06 |
| MUT-A07 | 点源信号权重跳过组内归一 | G-ANA-07 |
| MUT-A08 | Phase3 重采样输入取方差/逆方差而非信号 | G-ANA-08/INJ-07 |
| MUT-A09 | Phase3 用信号和替代加权组合 | G-ANA-09 |
| MUT-A10/A14 | Drizzle 方差漏掉尺度平方 / 漏平方 | G-ANA-10/INJ-08 |
| MUT-A11/A15/A16 | 常量 ADU 构造 / 无条件像素占比 / 点源通量当面亮度 | G-ANA-11/INJ-02 |
| MUT-A13 | 点源信号权重当逆方差 | G-ANA-07/MC-04 |
| MUT-M01 | 跨帧噪声相关性取 0 | G-ANA-03/MC-03 |
| MUT-M02 | 报告方差用错权重 | G-MC-01 |
| MUT-M03 | 像素逆方差近似忽略天光项且宣称最优 | G-MC-02 |
| MUT-M04 | 点源信号权重方差取 1/W | G-MC-04 |
| MUT-M05 | 有效 PSF 用输入中位 FWHM | G-MC-05/INJ-05 |
| MUT-M06 | 宣称像素逆方差等价于点源信息量 | G-MC-06/BASE-02 |
| MUT-M07 | 丢弃天光平面参数项 | G-MC-07 |
| MUT-M08 | 相关核取 1 | G-MC-08 |
| MUT-I01 | 共享系统项当独立项 | G-MC-03/ANA-03 |
| MUT-I02 | 注入恢复用普通最小二乘 | G-MC-01/INJ-01 |
| MUT-I03 | 方差从权重标量反推 | G-INJ-01/02 |
| MUT-I04/I05 | 逐帧阈值交集 / 样本派生信息权重 | G-INJ-03 |
| MUT-I06 | 扫描方向反转 | G-INJ-04 |
| MUT-I07/I08 | 背景非正返回权重 / 回退中位信噪比 | G-INJ-06 |
| MUT-I09 | Phase3 用 delta-PSF 近似 | G-INJ-07 |
| MUT-B01..B04 | 基线最优性冒充 / 点源信号权重用 Fisher / 方差从权重反推 / 等权宣称最优 | G-BASE-01..04 |
| MUT-R01..R03 | 生产输出作唯一期望 / 缺来源链 / 未复验标 VERIFIED | G-RD-01..06 |

### 3.2 规格与文档 mutation

| mutation | 注入 | 命中规则 |
|---|---|---|
| MUT-SPEC-01 | 清空某判据的 mutation 列表 | V-GATE-MUT |
| MUT-SPEC-04 | 点源信噪比幂进生产模式枚举 | V-MODES-* |
| MUT-SPEC-05 | 诊断量进权重来源集合 | V-TOKEN-WS |
| MUT-SPEC-06 | 宣称 support×snr² 合法 | V-TOKEN-RETIRED |
| MUT-SPEC-07 | 清空 `oracle.must_not` | V-ORACLE-* |
| MUT-SPEC-08 | `oracle.kind` 非白名单 | V-ORACLE-KIND |
| MUT-SPEC-09 | 门账本去掉层号 | V-LEDGER-LAYER |
| MUT-SPEC-10 | pending 容差去掉 owner | V-CRIT-OWNER |
| MUT-SPEC-11 | 单位表把信息权重记为 ADU^2 | V-UNITS |
| MUT-SPEC-12 | 清空 `must_not` | V-ORACLE-MUSTNOT |
| MUT-SPEC-13 | 退役 token 进生产枚举 | V-MODES-FORBIDDEN |
| MUT-SPEC-14 | 冻结容差无锚 | V-CRIT-ANCHOR |
| MUT-SPEC-15 | `executed_cases=0` | V-LEDGER-ZERO |
| MUT-SPEC-16 | skip-only | V-LEDGER-SKIP |
| MUT-DOC-01/02/03 | 删行 / 改容差 / 插入非法生产模式 | 人读机读一致性校验 |

## 4. 基线比较矩阵

把「哪个科学目标、用哪个统计量、相对哪个基线、能声明什么」写成预注册可比较的矩阵，并强制声明边界。机器规格位于实验单元的 qa-design 数据面。

### 4.1 比较度量（冻结集合）

| 度量 | 含义 |
|---|---|
| M-DET | 点源检测功率 |
| M-VAR | 测光方差 |
| M-FBIAS | 通量偏差 |
| M-SBBIAS | 面亮度偏差 |
| M-EPSF | 由有效 PSF 脉冲响应测量的 FWHM |
| M-ART | 接缝/伪影/黑洞清单 |
| M-NOISE | 预测噪声与实测噪声之比 |

数值阈值与效应量由预注册冻结，本标准不发明数值。

### 4.2 模式表

| mode | class | 权重对象 | 单位 | 权威式 | 有效 PSF | 组内归一 | 可声明 | 禁止声明 |
|---|---|---|---|---|---|---|---|---|
| `point_information` | production | 点源信息权重 | ADU^-2 | `Q_k=a_k P_k^T C_k^-1 d_k; W_info,k=a_k^2 P_k^T C_k^-1 P_k; F_hat=Q/W; Var=1/W` | 必需 | 否 | 模型与协方差门通过时可声明最佳线性无偏估计、最大点源信噪比、最小通量方差 | support/coverage/median_source_snr/fwhm/residual 作权重；对任意 PSF 的像素逆方差等价 |
| `surface_gls` | production | 组合系数加权的广义最小二乘 | 1/(BUNIT^2) | `x_hat=(A^T C^-1 A)^-1 A^T C^-1 d; Cov=(A^T C^-1 A)^-1` | 必需 | 否 | 广义最小二乘假设成立时可声明 Gauss-Markov 最佳线性无偏估计 | 像素逆方差无条件最优；无方差近似误差门即宣称最优 |
| `psfsw_robust` | production | 点源信号权重 | 1 | `Wt_k=C_norm*S^alpha*Conc^beta/(N^gamma*B^delta); W_psfsw,k=Wt_k/median_j(Wt_j)` | 必需 | 是 | 只能声明在指定验收数据上优于指定基线 | 点源逆方差；Fisher 信息量；方差从权重反推；1/W_psfsw；无共同星集时回退中位信噪比 |
| `equal` | baseline | 单位权重 | 1 | `I_out=mean_k d_k` | 必需 | 否 | 无（仅文档基线与对照） | 科学最优性 |
| `pixel_ivar` | baseline | 像素逆方差 | 1/BUNIT^2 | `I_out=Sum_k w_k d_k/Sum_k w_k, w_k=1/v_k` | 必需 | 否 | 无（扩展源在同点采样加噪声独立时退化为广义最小二乘；点源对任意 PSF 不最优） | 任意 PSF 下的点源最优性 |
| `psf_snr_power` | deferred | 功率之比 | 1 | 未冻结 | 必需 | 是 | 不得进生产；不得声明 Fisher 最优 | production；fisher_optimality |

### 4.3 比较单元

| cell | candidate | baseline | metrics | declaration_limit | 预注册 | 容差来源 |
|---|---|---|---|---|---|---|
| CMP-01 | `point_information` | `equal` | M-DET, M-VAR, M-FBIAS, M-EPSF | candidate 可声明最优（前提满足）；不得据此声明等权非法 | 是 | 预注册 |
| CMP-02 | `point_information` | `pixel_ivar` | M-DET, M-VAR, M-EPSF | candidate 可声明最小点源方差；不得声明像素逆方差对任意 PSF 等价 | 是 | 构造性判据（比值大于 1.5） |
| CMP-03 | `point_information` | `psfsw_robust` | M-DET, M-VAR, M-EPSF, M-ART | 两者目标与单位不同，只报度量与偏差，不互称最优 | 是 | 预注册 |
| CMP-04 | `surface_gls` | `pixel_ivar` | M-VAR, M-SBBIAS, M-EPSF | 仅在同点采样、噪声独立、天光项一致的声明域内比较 | 是 | 像素逆方差近似门 |
| CMP-05 | `surface_gls` | `equal` | M-VAR, M-SBBIAS | 基线对照，不声明等权非法 | 是 | 预注册 |
| CMP-06 | `psfsw_robust` | `equal` | M-DET, M-VAR, M-FBIAS, M-EPSF, M-ART | 只能声明在指定验收数据上优于等权 | 是 | 预注册 |
| CMP-07 | `psfsw_robust` | `pixel_ivar` | M-DET, M-VAR, M-EPSF, M-ART | 只能声明优于指定基线；不得写成点源逆方差或 1/W_psfsw | 是 | 预注册 |
| CMP-08 | `psfsw_robust` | `point_information` | M-DET, M-VAR, M-EPSF | 不得声明 Fisher 最优；含 seeing/concentration 惩罚时必须报告有效指数与相对损失 | 是 | 预注册 |
| CMP-09 | `equal` | `pixel_ivar` | M-VAR, M-FBIAS | 仅基线互比，二者均不得声明科学最优 | 是 | 预注册 |
| CMP-DEF | `psf_snr_power` | `none` | — | 保持延迟：不进生产、不参与比较结论、仅登记 | 否 | 权威设计 §2 |

### 4.4 声明规则

- `psfsw_robust` 只允许「在指定验收数据上优于指定基线」；禁止点源逆方差、Fisher、`1/W_psfsw` 语义。
- `pixel_ivar` 与 `equal` 是文档基线，禁止科学最优声明。
- `psf_snr_power` 保持延迟，不得进入生产路由。
- 训练与调参样本不得与最终验收样本相同。
- 真实数据比较必须配解析、Monte Carlo 或外部参考真值，不得以生产输出为唯一期望值。

## 5. 判读纪律

1. 每个比较单元必须预注册数据、基线、度量、效应量与置信区间；训练调参样本与验收样本各自独立。
2. 相关帧必须用联合协方差；点源信号权重的协方差只能从实际组合系数传播。
3. 报告必须包含有效 PSF，以及相对基线的检测功率、测光方差、FWHM、通量偏差与面亮度偏差。
4. 真实数据比较必须配解析、Monte Carlo 或外部参考真值。
5. 本标准给出的是证据形态与声明边界；数值阈值由实验单元的预注册冻结，正文以冻结值为准。
