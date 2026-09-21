# SCI-A 验收判据逐项结果

固定种子 20260921；一键复跑 bash 实验/SCI-A/code/run_all.sh。

| # | 判据 | 结论 | 实测证据 | 复现命令 |
|---|---|---|---|---|
| G1 | 测光残差双边界（sigma_floor <= sigma_obs <= sigma_ceiling） | **PASS** | 帧A σ_obs=0.045344 ∈ [0.0096589, 0.05683] → PASS；帧B σ_obs=0.057457 → PASS；帧C(n=105) σ_obs=0.051718 → PASS | bash 实验/SCI-A/code/run_all.sh（或 python3 实验/SCI-A/code/step5_calibration_gate.py） |
| G1b | 误差预算逐项（光子噪声/PSF 拟合/平场/天光/颜色/参考侧/量化）在**仿真帧上**由本帧推导 | **PASS** | 预算项来自本帧：σ_pix=18.4 e-、结构因子=1、σ_psfsys(帧内小孔径)=0.025 mag、σ_color=0.005736 mag、σ_flat=0.01973 mag；obs/pred=1.0092 | bash 实验/SCI-A/code/run_all.sh |
| G2 | 物理单位消除（产物只以星等表达；标定系数无绝对窗口；不可反解仪器参数） | **PASS** | 退化族 A·t/g 相同 ⇒ 中位通量 4831 / 4865 / 4840 ADU、σ_obs 0.0189 / 0.0181 / 0.0134 mag；零点平移不变量 Δlocation=0.8633228601（期望 0.8633228601）、Δσ_residual=0 | python3 实验/SCI-A/code/step6_apply_and_units.py |
| G3 | 帧间独立（各帧独立标定；无跨帧 k 门禁） | **PASS** | k_B/k_A=1.6099，透明度比倒数=1.6129 ⇒ 比值/期望=0.99812；σ_obs 比=1.2671；cross_frame_gate_present=False | bash 实验/SCI-A/code/run_all.sh |
| G4 | 星表引导检测（匹配率提升 + 算力节省量化 + 粗 WCS 鲁棒性） | **PASS** | 仿真帧 A：引导匹配率 0.9859 vs 盲检 0.2254（提升 0.7606）；盲检虚警 3、引导 0；粗 WCS 残余 3 px 时引导匹配率降至 0.01408。真实 M42 帧（4096²）：盲检 13163 个检出仅 1.49% 对应星表星，引导 434 个候选（45.2% 被盲检独立确认）⇒ 若逐个拟合盲检源需 30.3× 的 PSF 拟合次数 | python3 实验/SCI-A/code/step4_guided_vs_blind.py |
| G5 | apply photometry（I_photo = k_photo·m(x,y)·I_cal 落像素 + 下游消费 + 未启用降级） | **PASS** | 独立复算最大相对差=0；drizzle 尺度比 2.0606e-17（期望 2.06058e-17）；星等域残差中位 0.005106 mag；m 改正后残差 0.01973 vs 不改正 0.04534（改善=True）；未启用路径 degraded_reason=fail-closed: 产品未归一化到测光星等坐标系，禁止继续下游 | python3 实验/SCI-A/code/step6_apply_and_units.py |
| G6 | 非退化负例（真值无效应 ⇒ 归零/判红；有注入 ⇒ 度量变大） | **PARTIAL** | 5/6 通过；**注入-响应型 3/3 全通过**：N1 乘性平场残差 σ_obs 0.0453→0.1596（3.52×，≥0.04 判红）；N2 Poisson 天光抬升 ×1/2/4 σ_obs 0.0252→0.0481→0.0734（2.91×，4× 判红）；N3 漏颜色项阈值 0.04 mag（真实值 0.0057 的 7.0×）判 ABOVE_CEILING。N0 真值无效应→BELOW_FLOOR；N4 实测 k_B/k_A=1.6099 证明跨帧 k 门会误杀正确帧；**N5 未通过**：过裁剪真实样本时 σ_floor 下降更快、n=5 时变负 ⇒ 下界失效（已单列） | python3 实验/SCI-A/code/step7_negatives.py |
| G7 | 三类实验数据互证（HST 物理前向 / 纯解析合成 / testdata 真实帧；真实帧 σ_color/σ_gaia 不可自算 ⇒ 上界不完整） | **PASS** | ① HST M16 F657N 真实信号模板 + 完整物理前向（帧 A/B/C）；② 纯解析代数合成（step1，Oracle 相对误差 3.33e-15）；③ testdata 真实帧 testdata/M42_T2T3_mosaic_Flying_dutchman/T2/M1/M42_M1_T2_flying_dutchman-20251212@012404-300S-Red.fts | bash 实验/SCI-A/code/run_all.sh |
| G8 | XP 合成通量 vs HST PHOTFLAM 绝对定标对拍（跨滤镜/跨星色） | **PASS** | F657N: n=42 中位 Δmag=-0.1472±0.113 MAD=0.491 色项斜率=0.464；F673N: n=29 中位 Δmag=-0.263±0.13 MAD=0.645 色项斜率=0.765；F502N: n=75 中位 Δmag=-0.08011±0.0323 MAD=0.213 色项斜率=0.565；同星跨滤镜 n=19 中位差=0.1171 | bash 实验/SCI-A/code/step0_fetch_refs.sh && python3 实验/SCI-A/code/step3_forward_vs_photflam.py |

合计：8/9 项 PASS。
