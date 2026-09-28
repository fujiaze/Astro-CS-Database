# SCI-A 验收判据逐项结果

固定种子 20260921；一键复跑 bash 实验/photometric-magnitude/code/run_all.sh。

| # | 判据 | 结论 | 实测证据 | 复现命令 |
|---|---|---|---|---|
| G1 | 测光残差双边界（sigma_floor <= sigma_obs <= sigma_ceiling） | **PASS** | 帧A σ_obs=0.045344 ∈ [0.0096589, 0.048473] → PASS；帧B σ_obs=0.057457 → PASS；帧C(n=105) σ_obs=0.051718 → PASS；判据作用域 two_sided/two_sided（n≤12 时下界不可检验 ⇒ upper_only + LOWER_BOUND_UNDEFINED，见变更 claim PHOT-GATE-LOWSAMPLE-001） | bash 实验/photometric-magnitude/code/run_all.sh（或 python3 实验/photometric-magnitude/code/step5_calibration_gate.py） |
| G1b | 误差预算逐项（光子噪声/PSF 拟合/平场/天光/颜色/参考侧/量化）在**仿真帧上**由本帧推导 | **PASS** | 预算项来自本帧：σ_pix=18.4 e-、结构因子=1、σ_psfsys(帧内小孔径)=0.025 mag、σ_color=0.005736 mag、σ_flat=0.0008806 mag（真值平场散度 0.0032 经 N_eff 折算的**独立**项，变更 claim PHOT-SIGMAFLAT-INDEP-001；delta_after_m=0.01973 仅作诊断）；obs/pred=1.154 | bash 实验/photometric-magnitude/code/run_all.sh |
| G2 | 物理单位消除（定标坐标系=星等域、像素承载面=线性标度面 photo_scaled_adu；标定系数无绝对窗口；不可反解仪器参数） | **PASS** | 退化族 A·t/g 相同 ⇒ 中位通量 4831 / 4865 / 4840 ADU、σ_obs 0.0189 / 0.0181 / 0.0134 mag；零点平移不变量 Δlocation=0.8633228601（期望 0.8633228601）、Δσ_residual=0 | python3 实验/photometric-magnitude/code/step6_apply_and_units.py |
| G3 | 帧间独立（各帧独立标定；无跨帧 k 门禁） | **PASS** | k_B/k_A=1.6099，透明度比倒数=1.6129 ⇒ 比值/期望=0.99812；σ_obs 比=1.2671；cross_frame_gate_present=False | bash 实验/photometric-magnitude/code/run_all.sh |
| G4 | 星表引导检测（匹配率提升 + 算力节省量化 + 粗 WCS 鲁棒性） | **PASS** | 仿真帧 A：引导匹配率 0.9859 vs 盲检 0.2254（提升 0.7606）；盲检虚警 3、引导 0；粗 WCS 残余 3 px 时引导匹配率降至 0.01408。真实 M42 帧（4096²）：盲检 13163 个检出仅 1.49% 对应星表星，引导 434 个候选（45.2% 被盲检独立确认）⇒ 若逐个拟合盲检源需 30.3× 的 PSF 拟合次数 | python3 实验/photometric-magnitude/code/step4_guided_vs_blind.py |
| G5 | apply photometry（I_photo = k_photo·m(x,y)·I_cal 落像素 + 下游消费 + 未启用降级） | **PASS** | 独立复算最大相对差=0；drizzle 尺度比 2.0606e-17（期望 2.06058e-17）；星等域残差中位 0.005106 mag；m 改正后残差 0.01973 vs 不改正 0.04534（改善=True）；未启用路径 degraded_reason=fail-closed: 产品未归一化到测光星等坐标系，禁止继续下游 | python3 实验/photometric-magnitude/code/step6_apply_and_units.py |
| G6 | 非退化负例（真值无效应 ⇒ 归零/判红；有注入 ⇒ 度量变大） | **PASS** | 7/7 通过；**注入-响应型 4/4 全通过**：N1 乘性平场残差 σ_obs 0.0453→0.1596（3.52×，≥0.04 判红）；N2 Poisson 天光抬升 ×1/2/4 σ_obs 0.0252→0.0481→0.0734（2.91×，4× 判红）；N3 漏颜色项阈值 0.04 mag（真实值 0.0057 的 7.0×）判 ABOVE_CEILING；N6 自指预算项反例：注入 0.10 mag 逐星散度时自指口径恒 PASS、独立口径判 ABOVE_CEILING。N0 真值无效应→BELOW_FLOOR；N4 实测 k_B/k_A=1.6099 证明跨帧 k 门会误杀正确帧；N5 过裁剪（n≤12）不再静默 PASS，降级为 LOWER_BOUND_UNDEFINED（单边界） | python3 实验/photometric-magnitude/code/step7_negatives.py |
| G7 | 三类实验数据互证（HST 物理前向 / 纯解析合成 / testdata 真实帧；真实帧 σ_color/σ_gaia 不可自算 ⇒ 上界不完整） | **RED** | ① HST M16 F657N 真实信号模板 + 完整物理前向（帧 A/B/C）；② 纯解析代数合成（step1，Oracle 相对误差 3.33e-15）；③ testdata 真实帧 testdata/M42_T2T3_mosaic_Flying_dutchman/T2/M1/M42_M1_T2_flying_dutchman-20251212@012404-300S-Red.fts：σ_obs=0.02652 vs σ_ceiling=0.020561 ⇒ **ABOVE_CEILING**。三类**不一致**（仿真/解析绿、真实腿红）⇒按标准 01 §6 本单元的成立性判定为**不成立（待复审）**；真实腿与 06_photometry.md §4.1 的 L4 49 帧 PASS 1/49 同归因（未消系统项超预算） | bash 实验/photometric-magnitude/code/run_all.sh |
| G8 | XP 合成通量 vs HST PHOTFLAM 绝对定标对拍（跨滤镜/跨星色） | **PASS** | F657N: n=42 中位 Δmag=-0.1472±0.113 MAD=0.491 色项斜率=0.464；F673N: n=29 中位 Δmag=-0.263±0.13 MAD=0.645 色项斜率=0.765；F502N: n=74 中位 Δmag=-0.07361±0.0325 MAD=0.205 色项斜率=0.558；同星跨滤镜 n=19 中位差=0.1171 | bash 实验/photometric-magnitude/code/step0_fetch_refs.sh && python3 实验/photometric-magnitude/code/step3_forward_vs_photflam.py |

合计：8/9 项 PASS。
