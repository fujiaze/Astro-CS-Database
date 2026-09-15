# ALG-P2-POINT-001 — 门目录与负向 mutation 目录

- 文档 ID：ALG-P2-POINT-001-GATES
- 上位：ALG-P2-POINT-001_SPEC.md；机器源：run/v6/alg-p2-point/spec/phase2_point_spec.json
- 基线：HEAD = main = 125bc0999363be1a42a1f2df3254601e0cc7b8fb
- 说明：本表由机器规格机械渲染，保证人读门目录与机器 gates/mutations 一一对应；每条门带条款锚与不可满足时的 unavailable 原因。

## 1. fail-closed 门

| gate | clause | 判据（不可满足即 REJECT） | unavailable 原因 |
|---|---|---|---|
| GATE-UNIT-01 | ALG-P2PT-GATE-01 | 单位门：由声明量纲指数做代数必须得 Q=ADU^-1、W=ADU^-2、flux=ADU、Var=ADU^2；否则 REJECT。 | REJECT; unavailable 原因 unit_algebra_inconsistent |
| GATE-PSFPROF-02 | ALG-P2PT-GATE-02 | PSF 归一：Σ_p P_k,p = 1；P_k 非归一且未声明重标定即 REJECT。 | REJECT; unavailable 原因 psf_not_normalized |
| GATE-COV-03 | ALG-P2PT-GATE-03 | covariance 门：C_k 必须提供且可表示（含可表示相关项）；只给对角 variance 且未给相关核/算子摘要或未声明适用域即 REJECT。 | REJECT; unavailable 原因 covariance_unrepresentable |
| GATE-CORR-04 | ALG-P2PT-GATE-04 | 独立性门：检出跨帧相关时禁止朴素 Σ；必须用联合 C_in 否则 REJECT。 | REJECT; unavailable 原因 correlated_frames_require_joint_covariance |
| GATE-EPSF-05 | ALG-P2PT-GATE-05 | effective PSF 门：缺 effective PSF 或仅 FWHM 标量即 REJECT。 | REJECT; unavailable 原因 effective_psf_missing |
| GATE-COVPROP-06 | ALG-P2PT-GATE-06 | covariance 来源门：variance/covariance 必须由实际组合系数传播；用权重标量反推 variance 即 REJECT。若实现用近似 C~，必须报告 c~^T C c~ 与真实 C 的偏差比。 | REJECT; unavailable 原因 variance_not_from_combination_coefficients |
| GATE-DIAG-07 | ALG-P2PT-GATE-07 | 诊断别名门：median_source_snr/median_snr/source_snr_median/support/coverage/fwhm/psf_residual 等不得出现在 weight.sources / weight_value / covariance.variance_from。 | REJECT; unavailable 原因 diagnostic_token_as_weight_source |
| GATE-MODE-08 | ALG-P2PT-GATE-08 | 模式门：生产科学模式仅 {point_information,surface_gls,psfsw_robust}；psf_snr_power 保持 DEFERRED；legacy 0=support×snr² 与 auto/support_x_snr2 一律 REJECT。 | REJECT; unavailable 原因 production_mode_not_frozen |
| GATE-SCALAR-09 | ALG-P2PT-GATE-09 | 标量降级门：W_info 默认空间量；压成帧级标量必须同时过空间残差/趋势门与功率损失门，并带 p05/p50/p95+最大系统偏差+采样覆盖+模型误差+适用域，否则存 map/model/control points。 | REJECT; unavailable 原因 scalar_degradation_gate_failed |
| GATE-PROV-10 | ALG-P2PT-GATE-10 | provenance 门：缺 FZ-PROV-MINIMAL-SET 键、单位不可判、unavailable 无原因即 REJECT。 | REJECT; unavailable 原因 provenance_incomplete |
| GATE-OUTFRAME-11 | ALG-P2PT-GATE-11 | 输出帧门：Q/W 必须在输出帧由原始量重算；检出对输入 Q/W map 的重采样/平均求和即 REJECT。 | REJECT; unavailable 原因 output_frame_recompute_required |
| GATE-SNR-12 | ALG-P2PT-GATE-12 | SNR 门：独立帧必须满足 SNR_combined^2=ΣSNR_k^2（容差内）；相关帧朴素求和须被检出为过度乐观并 REJECT。 | REJECT; unavailable 原因 independent_frame_snr_identity_violated |
| GATE-EPSF-11 | ALG-P2PT-GATE-13 | 归一约定门：effective PSF 必须显式声明 peak 或 integral 归一与对应产品族；未声明归一约定即 REJECT。 | REJECT; unavailable 原因 effective_psf_normalization_undeclared |

## 2. 负向 mutation（注入错误 -> 门必须 rc!=0）

| mutation | 注入 | 命中 | 期望 |
|---|---|---|---|
| M01 | Q 公式去掉 C^-1（改成 a_k P_k^T d_k） | O1/O11 | rc!=0 |
| M02 | W 单位改为 ADU^2 | O11/GATE-UNIT-01 | rc!=0 |
| M03 | Var(F_hat)=W（去掉倒数） | O2 | rc!=0 |
| M04 | P_k 不归一（ΣP≠1） | GATE-PSFPROF-02 | rc!=0 |
| M05 | 相关帧允许朴素 Σ（independence gate 关闭） | GATE-CORR-04/O5 | rc!=0 |
| M06 | effective_psf={"fwhm":4.71} | GATE-EPSF-05 | rc!=0 |
| M07 | variance_from=psfsw_robust_weight | GATE-COVPROP-06 | rc!=0 |
| M08 | weight.sources=[median_source_snr,support,coverage,fwhm,residual] | GATE-DIAG-07 | rc!=0 |
| M09 | 生产模式加入 psf_snr_power | GATE-MODE-08 | rc!=0 |
| M10 | 移除 provenance 必需键（flux_conservation_factor/归一版本/相关核摘要） | GATE-PROV-10 | rc!=0 |
| M11 | 帧级标量 W 且无空间残差/功率损失门与分位数 | GATE-SCALAR-09 | rc!=0 |
| M12 | output_frame.forbid_resample_input_qw=false（允许重采样输入 Q/W） | GATE-OUTFRAME-11 | rc!=0 |
| M13 | SNR_combined=ΣSNR_k（忘平方） | GATE-SNR-12/O10 | rc!=0 |
| M14 | 白噪声近似用于非对角 C 且未声明 sigma_pix | GATE-COV-03 | rc!=0 |
| M15 | effective PSF 未声明归一约定 | GATE-EPSF-11 | rc!=0 |

Oracle 自我 mutation（OM1–OM3，见 oracle/phase2_point_oracle.py）：组合系数 c x1.10 -> O3 红；
相关帧 rho->0 当独立 -> O5 红；单位指数错 -> O11 红。

## 3. 独立 Oracle 正向 check（O1–O12）

- O1 Q/W==GLS
- O2 Var==1/W==(A^TC^-1A)^-1
- O3 c^T C c == 1/W
- O4 independent merge Q=ΣQ_k,W=ΣW_k
- O5 correlated joint>naive (ratio>1)
- O6 white-noise W=a^2/(sigma^2 A_NEA)
- O7 MC injected source sigma_F==1/sqrt(W)
- O8 effective PSF impulse response==analytic combination
- O9 FWHM(P_eff)!=median per-frame FWHM
- O10 SNR^2==ΣSNR_k^2 exact (independent)
- O11 unit dimensional algebra
- O12 detection statistic S=Q/sqrt(W) and 5sigma depth

## 4. 引用冻结条目（required_freeze_ids）

- FZ-UNIT-Q
- FZ-UNIT-WINFO
- FZ-UNIT-FLUX
- FZ-FORMULA-Q
- FZ-FORMULA-WINFO
- FZ-FORMULA-FHAT
- FZ-COND-WHITENOISE
- FZ-FORMULA-COV-PROP
- FZ-GATE-MEDIAN-SNR
- FZ-GATE-SUPPORT-COVERAGE
- FZ-GATE-PSFSW-EPSF
- FZ-MODE-PRODUCTION
- FZ-MODE-DEFERRED
- FZ-DEGRADE-SCALAR
- FZ-PROV-MINIMAL-SET
- FZ-P3-QW-RECOMPUTE
- FZ-BUNIT-SEMANTICS

