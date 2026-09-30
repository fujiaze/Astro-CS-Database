# 实验报告 · P1 通量积分拟合（测光星等坐标系）

**单元**：`实验/photometric-magnitude`（SCI-A，SCI-401）。本报告是实验重做轮的成稿实验报告：整合历史正本轮实验（step0–step9，seed=20260921）与三路独立重做（路线1/2/3，seed=20260926）；既有历史文档（`README.md`、`docs/fsyn_convention.md`、`RESOLUTION_m42_curve_resolve.md`、`results/REVIEW.md`、`results/DOC_CORRECTIONS.md`、`results/GATES.md`）保留，失效处按台账订正并在文内注明。
**裁决依据**：`实验/裁决台账.md`（D-xx）、`docs/DISPUTES.md`（A-P1-xx）、`docs/science/DISPUTE_RESOLUTION.md`（科学正本同步件）。本单元无 P1 专属补实验目录（`补实验-control_variance / -k_corr / -5.07` 分别由 P2/P3/P5 承载）。
**论文版**：`REPORT_paper.md`；文献台账：`refs.md`；推导：`docs/derivation_robust_weights.md`。

---

## 1 假说

- **H1（主假说）**：存在逐帧标定系数 `k_photo` 与低阶空间增益 `m(x,y)`，使 `I_photo = k_photo·m(x,y)·I_cal` 后同一物理通量的星在不同帧/位置/天光水平下星等一致；一致性可由本帧自算误差预算的双边界判据判定。
- **可证伪点**：①真值无效应 ⇒ σ_obs = 0 且判红；②乘性空间残差注入 ⇒ σ_obs 单调上升并判红；③σ_obs 落入 `[σ_floor, σ_ceiling]`；④`k_photo` 不含绝对数值窗口（尺度无关）；⑤`k_photo` 不可反解仪器参数。
- **H2（工程假说）**：Gaia 位置引导检测优于盲检测，优势依赖二轮 WCS 平移精化 ≲2 px。
- **重做轮新增假说**：H1′（常数体系）——c = 4.685 是"正态 95% 渐近效率"标准调参值、0.6744897501960817 是解析恒等式 Φ⁻¹(3/4)、平移不变量逐位成立；H1″（系统差源）——星等窗/FOV/预过滤等"约定常数"的实际危害可量化且方向可判。

## 2 方法

- **参考通量**：`F_syn = ∫F_λ·T·Q·λ dλ`（W·m⁻²·nm，官方 343 点 @2 nm，336–1020 nm；G<15 域）；Akima 子样条 + 复合 Simpson 1/3（奇区间 3/8、n==1 退梯形）；**不含 `10^(−0.4·G)`**（`docs/fsyn_convention.md` 判定，生产实现逐位一致）。插值/求积设置配置化 + 运行日志不落盘（负责人已批）。
- **稳健零点**：`r_i = log10(F_instr/F_syn)` [dex]；固定尺度 Tukey biweight IRLS（c=4.685，tol=1e-6，max_iter=50）；`k_photo = 10^(−location)`；`sigma_residual = MAD(r_inliers)/0.6744897501960817`。
- **预筛窗（订正 P1-M03）**：`|delta_i − median(delta)| ≤ 3.0 mag`，`delta_i := −2.5·log10 F_instr,i − G_i`（量纲 mag）；严格等价于 `|r_i − median(r)| ≤ 1.2 dex`（3.0/2.5）。两域容差不得混用。
- **求积退化分支（订正 P1-m02）**：`n_int == 3` 时 1/3 前段区间数 `n_13 = 0` ⇒ 该段必须为 0；历史多计 `2·y[0]·h/3`，常数被积函数得 3.6667 vs 真值 3.0（+22.2%）。生产端与实验参考实现已按 claim `PHOT-SIMPSON-N3-001` 同步订正并补闭式期望 + 故障注入。
- **双边界判据**：`σ_floor/σ_ceiling = (1 ∓ 3·1.166/√n)·(下/上界)`；1.166 = √1.361（MAD 标准化方差，**按台账 A-P1-01 订正标签**）；预算项各计一次。
- **判据作用域（订正 P1-M07）**：`rho_lo = 1 − 3·1.166/√n ≤ 0` ⟺ `n ≤ 12.236` ⇒ 作用域降级 `upper_only`、状态词 `LOWER_BOUND_UNDEFINED`（**不记 PASS**）；**撤回** `max(rho_lo,0)` 夹逼（恒真门）。
- **σ_flat 独立性（订正 P1-B01）**：仿真帧取**真值**逐像素平场散度经 `N_eff` 折算（`scia_common.sigma_flat_independent`，变更 claim `PHOT-SIGMAFLAT-INDEP-001`）；真实帧取 `06_photometry.md` §4.1 的 `σ_flat,hf = 0.0007 mag`；`calibrate()['delta_after_m']`（与被测统计量同源）**降级为诊断字段**，不进预算。反例化见 `results/step7_negatives.json → N6`。
- **重做轮方法学**：三路互不通信独立取证；每项"文献腿（一手 DOI/官方文档/开源逐字）＋实验腿（固定 seed 合成实验，含"真值无效应⇒归零"负例）＋推导腿"三腿补齐；纯 numpy，不 import 仓库任何 Python。

## 3 数据

| 类别 | 数据 | 角色 | 代码 | seed |
|---|---|---|---|---|
| ① 物理前向仿真 | HST M16 F657N HLSP drz 真实结构 + 真实 XP SED 注入 + 完整噪声物理 | 真实条件下的判据行为 | `code/step2_hst_sim.py` | 20260921 |
| ② 纯解析合成 | 600 星解析 Oracle + 多噪声组 + 负例组 | 实现正确性/约定收敛/负例 | `code/step1_analytic.py` | 20260921 |
| ③ testdata 真实帧 | FLI M42 M1 T2 Red 300 s（4096² uint16+BZERO） | 底参照（无真值） | `code/step8_real_frame.py` | — |
| ④ 重做合成 | 三路独立合成实验（常数/窗口/FOV/锚核验/贯穿用例） | 三腿补齐 | `code/redo/route{1,2,3}/` | 20260926 |

外部交叉核对：SVO 通带曲线 + PHOTFLAM/PHOTPLAM；GaiaXPy 2.1.4 官方实现 [文献:V12]；ESA Gaia DR3 文档 [文献:V9]。

## 4 结果

### 4.1 历史正本轮（results/step1..step8*.json，seed 20260921）

- Oracle：估计器 vs 真值 rtol = 0.0，k 相对误差 3.33e-15 [实验:code/step1_analytic.py]。
- 判据（**订正 P1-B01 后重跑**）：3 仿真帧全 PASS（σ_obs = 0.045344/0.057457/0.051718 mag；obs/pred = 1.154/1.008/0.417）[实验:code/step5_calibration_gate.py]；**真实帧判红**——σ_obs = 0.026520 > σ_ceiling = 0.020561 ⇒ **ABOVE_CEILING**（σ_flat 取仓内约定常数 0.0007 mag；翻转临界 σ_flat = 0.013057 mag = 该约定值 18.7 倍）[实验:code/step8_real_frame.py]。
- **三类数据不一致 ⇒ 本单元成立性判「不成立（待修）」**（审查标准 §6）；真实帧判红可归到「未消系统项超预算」这一层（与 L4「PASS 1/49」同一层），但**具体成因不可归因**——双边界门对成因失明（`REPORT_paper.md` §7 第 18 条）。
- 负例齐备（**7/7 通过**）：真值无效应归零判红、乘性残差单调判红、散粒敏感（2.91×，4× 判红）/加性不敏感（1.08×）；**N6** 为 P1-B01 的反例化（自指口径恒 PASS、独立口径 0.05 mag 判红）；**N5** 改为 `LOWER_BOUND_UNDEFINED` 语义 [实验:code/step7_negatives.py]。
- 低样本：`n ≤ 12.236` 时 `rho_lo ≤ 0` ⇒ 3σ 下包络不存在（n=5 实测 σ_floor = −0.004911）；已按 P1-M07 改为显式降级（`upper_only` + 状态词不记 PASS），低样本域判别力只来自上界与状态词 [实验:code/step5_calibration_gate.py][实验:code/step7_negatives.py → N5]。
- 引导检测：匹配率 0.2254/0.1972 → 0.9859；4096² 真实帧盲检 13163 检出仅 1.49% 对应星表星、30.3× 拟合次数；WCS 平移精化 ≲2 px 是前提（HST drz ~1.6″ vs 真实帧 0.0068″）[实验:code/step4_guided_vs_blind.py]。
- 判读（**订正**）：H1 在**仿真腿与解析腿**成立、在**真实帧腿不成立** ⇒ 按审查标准 §6 记 **不成立（待修）**；H2 成立（带 WCS 前提与循环性边界）。

### 4.2 重做轮三路（results/redo/，seed 20260926）

**路线1（S1–S18 + H1–H6 全处理）**：
- S1/S2/S3/S13：c=4.685、MAD 系数、IRLS 容差、1.166 因子三腿复核通过；√1.361 = 1.1666 推导腿闭合 [实验:route1/exp1]。
- S4：mag_tolerance=3.0 边际保护 ≈0（IRLS 就位后），作用面 = 样本族控制 [实验:route1/exp2]。
- S5：FOV 三常数为项目约定；B12 危害路径量化 −0.146 dex；奇异 CD 可通过 |r_consistent|≥3 门 [实验:route1/exp3]。
- S6：阶梯"宁多查一档也不空手"；早停只在 >5× 平均密度场生效；"上界 10000"注释宣称未实现（实锚 pc_api.cpp:290 vs :297-301）[实验:route1/exp3]。
- S7：星等窗 → ZP 平移最高 −0.0318 mag（两消费面口径分裂量化）[实验:route1/exp2]。
- S12：空间增益阶数是科学量；S15：ZP_syn 下限 3 的有限样本偏差量化；S16/S17 豁免论证；H1 锚 STALE（PHOTOMETRY.md:227/:356 → A-P1-04作为判别时准确、作为预算项时含噪声（`sigma_psfsys_noise_expect`）须双读）[实验:route1/exp4, exp5]。

**路线2（S1–S14）**：
- S1：ARE(4.685)=0.9499974；c*=4.6850649 ≡ statsmodels 4.685065；两套求积互证 7 位；负例 ψ=x 偏差 1.4e-12 [实验:route2/exp1]。
- S2：常数二分复算差 1.6e-16；**新发现** 1.4826 四位截断违反 NOISE_ESTIMATION.md:213（A-P1-08）[实验:route2/exp1]。
- S3：tol 1e-2→1e-12 中位 location 跨度 1.93e-4 dex、最多 17 步 ⇒ 求解器设置豁免 [实验:route2/exp1]。
- S4：预过滤平移不变性负例归零（内点集逐元素相同）[实验:route2/exp2]。
- S8（mag_max_arr 耦合）：任何第 6 档改动静默失效的负例定量 [实验:route2/exp3]。
- S14：端到端贯穿用例 ZP_syn=28.9358 mag / k_photo=1.00156 / σ_res=0.0493 dex → 下游 1.253·σ/√N=0.00138 dex [实验:route2/exp8]。
- 锚复核 24 处，唯一"内容不符"判 PMC6768164——台账 D-06 判锚有效，误判撤回。

**路线3（S00–S12，15/15 锚核验）**：
- S00：审查引锚 15/15 真实；PHOTOMETRY.md:126/:400 为行号漂移（正本 :13/:15-17）[实验:route3/exp_S00]。
- S01：Kafadar 1983 一手锚 + ARE 复算 [实验:route3/exp_S01]。
- S02：MAD 常数一致性（1.6e-16 族）[实验:route3/exp_S02]；S03：IRLS 收敛阶 [实验:route3/exp_S03]。
- S04：平移不变量逐位 [实验:route3/exp_S04]；S05：σ_residual=0∧fit_ok=true 不可估计 [实验:route3/exp_S05]。
- S06：F_syn/Simpson 误差分解（网格离散主导，uint8 量化 0.025%）[实验:route3/exp_S06]。
- S07：FOV 缓冲/钳位冲突 [实验:route3/exp_S07]；S09：mag 窗 [实验:route3/exp_S09]；S10：匹配半径（歧义 ≈r²）[实验:route3/exp_S10]。
- S11：n=3 MAD 偏差 1.49×、**渐近式 n=3 高估 SE 8.03%**（订正 P1-M04：精确 SE = 0.6698291607404144σ vs 渐近 0.7235930923753581σ；历史 7.4% 是 `1−0.9256987` 的误读）、ZP 三星下限抽样误差 25.7× [实验:route3/exp_S11]；S12：1.0 dex 界 0.055/1.110 非恒真 [实验:route3/exp_S12]（**注 P1-m09**：该界为**粗筛**，历史正文的 3.0 mag 与实现一致；S12 的通过率对照是本单元少数的**有判别力**负例）。

**判读**：H1′ 成立（常数体系三腿闭合，锚体系整体真实，**二手归属两条已标注**：V3/V11）；H1″ 成立（约定常数的危害方向与量级确认，豁免判定按 §6 清单）。
**m09 自洽性检查登记（无判别力，不得当证据）**：`route3/exp_S06` 的 **H6a/H6b** 与 `exp_S02`/`exp_S11` 的 `negative_zero_check` 均为**自洽/实现守卫**——常数序列下度量恒 0，属恒真结构；现有 JSON 字段保留作回归守卫，正文引用已标注其**无判别力**。

## 5 结论

1. 测光星等坐标系的参考通量口径 `F_syn = ∫F_λ·T·Q·λ dλ` 与稳健零点估计（Tukey biweight c=4.685）三腿闭合；常数体系（c、0.6745、1.4826、1.166=√1.361）全部有文献/推导/实验三重支撑。
2. 判据非退化性与适用域确立：真值无效应归零、乘性残差单调判红、`n ≤ 12.236` 时作用域降级 `upper_only`（**不记 PASS**）、不能认证天光扣除质量。**订正后真实帧腿判红 ⇒ 三类数据不一致 ⇒ 本单元成立性按审查标准 §6 判「不成立（待修）」**（见 §4）。
3. 标定系数无绝对窗口由平移不变量逐位支撑；绝对值不可反解仪器参数。
4. 主系统差源量化完成：星等窗 ≤0.032 mag（>统计误差 30 倍）、FOV 错配 −0.146 dex、低样本偏差 1.49×——下游精度约定（链条位置）据此写定。
5. P1 输出的每 dex 精度是 P2–P5 全链误差底座；接口数值见 `REPORT_paper.md` §3。

## 6 豁免清单（写明理由）

| 项 | 性质 | 理由 |
|---|---|---|
| 门①/门② 逻辑守卫 | 结构守恒 | 非科学量（台账豁免判定） |
| irls_tolerance=1e-6 / max_iter=50 | 数值求解器设置 | 敏度实验 1e-2→1e-12 不敏感、50 步不可达 [实验:route2/exp1] |
| 0.6745 / 1.4826 | 解析恒等式 | 推导腿自足（docs/derivation_robust_weights.md D3） |
| FOV 三常数 / 阶梯档距 / mag_min 亮端 / 1.0 dex 界 | 项目约定（不注文献出处） | 敏感性实验与保守方向已给；文献腿按台账 U6 登记为约定 |
| 匹配半径 2.0 px | 实现选择 | 全档敏感性表（0.5–4.0 px）[实验:route1/exp2] |
| max_stars=5000 | 工程上限 | σ_ZP(N) 曲线支撑（**订正 P1-M05**：统一式 `σ_ZP(N) = 1.2533·σ_star/√N`，`σ_star = 0.05 mag ≡ 0.02 dex` ⇒ 5000 ⇒ **8.86e-4 mag**；历史 6.36e-4 mag 来自 route1 的 `0.045/√N` 变体，缺 1.2533 中位数因子、σ_star 取值也不同，相对低估 1.39×）[实验:route3/exp_S08] |
| m_cut 初值 6.0/1.5/2.0、身份指纹容差 1e-9 | 初值/浮点回环余量 | 不进科学值（S16/S17 豁免论证） |
| quality_factor 0.1/0.5 | 项目约定 | 负责人已批豁免；承载单元 P5（本单元不涉） |

## 7 订正记录（按分歧台账，注明条目）

| # | 历史内容 | 订正 | 依据 |
|---|---|---|---|
| 1 | README §2.3 与旧 REPORT_paper §2.2 "1.166 = SD(MAD)/MAD" | 改为"1.166 = √1.361，MAD 尺度估计量标准化方差的平方根（σ̂ 相对标准差因子）" | **按分歧台账 A-P1-01 订正** |
| 2 | `PHOTOMETRY.md §14` PMC 锚疑云（路线2 判内容不符） | **锚有效，维持不改** | **按分歧台账 D-06** |
| 3 | 审查"幻觉锚"定性（H1 锚 :227/:355、:126/:400 等） | 改记"行号漂移"；正本定位 :13/:15-17 | **按分歧台账 A-P1-04/06/09 订正** |
| 4 | `05 A-4b` FOV"无条件钳位"修法 | 须同时写明缓冲 1.2 与钳位下界 1.0 的优先语义（危害量化 −0.146 dex） | **按分歧台账 A-P1-12 订正** |
| 5 | Gaia DR3 arXiv 号 2205.11321 | 正确号 2208.00211 | 路线1 文献腿自纠 |
| 6 | `frame_photometry_fit.cpp:292` 1.4826 截断、mag_max_arr 耦合、B13 接线 | 登记为条款一致性/接线整改项（不改判据方向） | **按分歧台账 A-P1-08 订正** |
| 7 | 旧 REPORT_paper 精读版（保留其仍成立的三类数据结论） | 本报告与其重写版并存；两轮数字均标注 seed 来源 | 本轮成稿纪律 |
| 8 | **σ_flat 取 `calibrate()['delta_after_m']`**（自指：被测样本拟合后的残差散度充当预算项，真实帧占上界方差 47.6%） | 仿真帧改取**真值**平场散度经 `N_eff` 折算、真实帧取 `06_photometry.md` §4.1 的 `σ_flat,hf = 0.0007 mag`；`delta_after_m` 降级为诊断字段 | **审查 P1-B01（blocker）**；变更 claim `PHOT-SIGMAFLAT-INDEP-001`；反例化 = `step7_negatives.json → N6`；同一订正使真实帧由 PASS 翻为 ABOVE_CEILING（如实改判「不成立（待修）」） |
| 9 | 低样本下界建议 `max(rho_lo, 0)·σ_fit` | **撤回**；改显式最小样本规则：`n ≤ 12.236` ⇒ 作用域 `upper_only` + 状态词 `LOWER_BOUND_UNDEFINED`（不记 PASS），并报出 `n`/`gate_scope` | **审查 P1-M07**（恒真门无证据资格，标准 §7）；`docs/science/PHOTOMETRY.md` §16.5 第 3 条已同步 |
| 10 | 预筛窗写作 `|r − median(r)| ≤ 3.0`（r 为 dex）却注「= 1.2 dex」 | 量纲显式：`|delta − median(delta)| ≤ 3.0 mag` ⟺ `|r − median(r)| ≤ 1.2 dex`，实现 = `delta_i := −2.5·log10 F_instr,i − G_i` | **审查 P1-M03**；`docs/derivation_robust_weights.md` D1 |
| 11 | n=3 渐近式「高估 7.4%」 | **8.03%**（精确 SE 0.6698291607404144σ / 渐近 0.7235930923753581σ − 1） | **审查 P1-M04** |
| 12 | σ_ZP(5000) = 6.36e-4 mag（route1 `0.045/√N`）与 route3 的 `1.2533·σ/√N` 并存 | 统一为 `1.2533·σ_star/√N`（单位随输入域）⇒ **8.86e-4 mag**；route1 变体登记为**被取代** | **审查 P1-M05**；`route3/exp_S08 → H8b/H8d` |
| 13 | Simpson 退化分支 `n_int == 3` 多计 `2·y[0]·h/3`（常数被积函数 3.6667 vs 3.0，+22.2%） | 该段置 0；生产端与实验参考实现同步订正 | **审查 P1-m02**；变更 claim `PHOT-SIMPSON-N3-001`；`p1phot` O3 组闭式期望 + 故障注入 `o3_simpson_n3_reference` |
| 14 | 文献题录：V4 期号、V8 题名/DOI、V10 适用域、V2 路径、Akima 页域 | 6(9) / “…”spectroscopic data”+DOI / **两级域**（采样表示 G<15、连续表示 G<17.65）/ `robust/_tables.py` / **589–602** | **审查 P1-m06 / P1-M02**；`refs.md` + `docs/science/PHOTOMETRY.md` §14a |
| 15 | 一手核验记录指向 `run/SCI-401/lit/verified_refs.md`（`run/` 为 gitignore、已回收 ⇒ 路径不存在） | 证据**随单元入库**：3 条引文订正记录落入 `refs.md`；其余文档改指 `refs.md` | **审查 P1-m10** |
| 16 | 仿真帧 `σ_gaia = 0.002 mag` 计入上界，但注入与模型共用同一 `mag_eff` ⇒ 参考侧扰动在 `r_i` 中精确相消（复算相对差 0） | 标注「**该项在仿真中未被激活（偏松方向）**」，保留数值但降级为未检验项 | **审查 P1-m04**；`code/step5_calibration_gate.py` 注释 + `README.md`/`REPORT_paper.md` §3 + 本节 §4 边界 |
| 17 | 恒真/空断言判据：`exp_S06` H6a（代数恒等）、H6b（积分线性性）、`exp_S02`/`exp_S11` 的 `negative_zero_check`（常数序列度量恒 0） | 三文件加 `discriminating_power` 字段标为「自洽守卫（无判别力）」，**保留字段作回归**但不作证据；有判别力的 H6c/H6d/H12、N0–N6 保留 | **审查 P1-m09**（标准 §7）；重跑落盘 `exp_S02/S06/S11` JSON |
| 18 | `p1phot_performance` 并行比值门在共享过载节点上负载驱动地假红/假绿（同二进制 6 次隔离复跑 2 绿 4 红，parity4 6.0–9.8×） | **不改阈值 4.0**：自适应内循环 K 使单线程计时 ≳0.10 s；`getloadavg()` 过载时降级为 SOFT 登记；新增 `P1PHOT_PERF_FORCE_HARD=1` 负例开关证明该门**能红** | **本轮自查（P1-m13）**；`lib/algorithms/photometry/tests/p1phot/p1phot_tests_perf.cpp`；证据 `run/FINAL-07/logs/p1-perf-gate-red-green.txt` |
| 19 | `results/GATES.md` 的 G1/G7 结论**写死**（G1 恒 PASS、G7 恒 PARTIAL）⇒ 恒真门 | 改为**由实测派生**：G1 三帧全 PASS 才绿；G7 由真实腿派生 ⇒ 本轮为 **RED**（三类数据不一致），合计 8/9 | **本轮自查**；`code/step9_collect.py` |

## 8 诚实边界

见 `REPORT_paper.md` §7（十八条全列）。补充实验口径：①历史正本轮全部预算项在仿真帧上由本帧推导（σ_color 为真值重算、**σ_flat 为真值平场散度经 `N_eff` 折算的独立项**，两者都**不含**被测残差；**`σ_gaia=0.002 mag` 在仿真上未被激活**——注入与模型共用同一 `mag_eff` ⇒ 参考侧扰动在 `r_i` 中精确相消，方向偏松），真实帧上 σ_color/σ_gaia 不可自算、标 `null` 且上界不完整，**σ_flat 取仓内约定常数 0.0007 mag 而非自算**；②仿真引导匹配的定位输入即注入真值（结构性循环），非循环证据来自真实帧；③σ_psfsys 孔径口径 r=4 为作者约定（保留三方对照），生产口径为 r=10，同帧实测 10.3 倍差；④重做轮计数模型依赖天空平均密度假设（820/deg²，G<16 全天平均），非逐场实测；⑤**探测器线性适用域未测**：代码只有饱和硬门（`saturation_adu = 65535`），不拦未饱和的非线性区；「线性化残差」整项不在 `σ_ceiling` 里，进入非线性区的电子数阈值未测（复现：`code/redteam/rt_gate_attribution.py`）；⑥**σ_gaia 的仿真取值自参照**：0.002 mag = 仿真注入的 `ref_err_mag`，比 XP 绝对刻度上限 1%（0.0109 mag）乐观 **5.4 倍**；⑦**σ_flat 的一手出处未取得**：仓内唯一活着处是 `scia_calib.py:31` 的字面量，被多份文档称为「权威常数」，而它单独决定真实帧判红方向；⑧**双边界判据双重偏松**：`n ≤ 12.236` 时只剩上界，且 `sigma_obs` 的 MAD 尺度估计在 n=3 处期望仅 0.67σ；⑨**双边界门成因失明**：`sigma_obs_mag` 在 `m(x,y)` 之前算，`m_degree = 0` 与 `m_degree = 2` 的读数逐位相同；n=48 时仅 2% 的位置依赖乘性残差即把 `BELOW_FLOOR` 翻成 `PASS`、3% 翻成 `ABOVE_CEILING`；⑩仍开放（不阻成稿）：Lindegren 2021 亮端数字、R&C 原文、Huber & Ronchetti 页码、σ_floor/ceiling 政策值、ZP_syn 下限政策值、ieq 工程修复——**UNRESOLVED 项一律不进论文正文**，此处为唯一登记面。

## 9 复现命令

**公共前置步（先跑，再跑本单元）**：

```bash
cd <repo root>
bash 实验/shared/synthetic/run_selftests.sh     # 合成器物理链自检：4 组件，全绿约 2.5 min
```

退出码 0 = 全绿；1 = 有组件判红（本单元读数不得作为证据）；2 = 有组件缺失。
**不得只调 `noise_selftest.py`**：它只验「独立重实现 vs 生产实现 vs 解析预测」三方对照，
验的是方法（分布与口径），对**生产实现本身**零判别力——生产实现坏掉而独立重实现与解析
预测同源一起坏时它照样绿。**生产面的门禁责任在 `m16_scene --selftest` 与
`m16_sampling --selftest`**。四个组件全跑，不要裁剪成单组件。

本单元：

```bash
cd <repo root>
# 主实验轮（seed 20260921）
bash 实验/photometric-magnitude/code/run_all.sh            # step0–step9（quick 跳过最慢 step6）
bash 实验/photometric-magnitude/code/step0_fetch_refs.sh   # 唯一需网络（脚本内钉死 SHA256）
# 重做三路（seed 20260926）
bash 实验/photometric-magnitude/code/redo/run_all.sh       # route1–route3；quick 跳过 route2 exp3/exp7
# 单脚本示例
python3 实验/photometric-magnitude/code/redo/route2/exp1_robust_constants.py
# 基准读数：results/step1..step8*.json、results/redo/route{1,2,3}/*.json；
#           空间增益逆向验收 results/reverse_verify/p1_spatial_gain/data/*.json
# 汇总 results/redo_summary.json
```

固定 seed 说明：主实验轮 `20260921`（`scia_common.rng(tag)` SHA256 派生）；重做三路统一
`20260926`（脚本内写死；路线1 部分脚本派生 SEED+1…）。RNG 一律
`numpy.random.default_rng(SEED)`。重跑读数与基准快照逐项一致（固定 seed 保证）。
**结果数据一律落 `results/`，`code/` 下只放代码**；各脚本的写出路径由脚本自身位置推导，
不依赖调用者 cwd。

---

## 10 佐证来源

本节把三类支撑分列：**一手文献**（书目/DOI 可解析，原句照抄）、**开源实现**
（项目 + 版本 + 文件:行）、**仓内实测**（复现命令 + 产物路径）。完整台账见 `refs.md`。

### 10.1 一手文献（核验途径与关键原句见 `refs.md`）

| 用于支撑的结论 | 文献 | 核验层级 |
|---|---|---|
| `F_syn = ∫F_λ·T·Q·λ dλ` 的物理刻度与通带定义 | Gaia Collaboration & Montegriffo et al. 2023, A&A 674, A33（arXiv:2206.06215） | 摘要/正文原句照抄 |
| `F_syn` **不含** `10^(−0.4·G)`；XP 外定标 ±2% | Montegriffo et al. 2023, A&A **674, A3**（DOI 10.1051/0004-6361/202243880） | Crossref/arXiv 书目 + §8.1 原句 |
| XP 采样谱 343 点 @2 nm、零点定义式 (5.41)、绝对刻度 ≈1% | ESA Gaia DR3 官方文档 §5.4.1 / §20.12.4 | 官方文档原句 |
| XP 可用域（采样表示 G=15；连续表示 G<17.65，**两级域不得合并**） | Montegriffo et al. 2023 附录 B（arXiv:2206.06205） | 一手原句照抄 |
| `c = 4.685` ⇔ 正态 95% 渐近效率 | Kafadar 1983, J. Res. Natl. Bur. Stand. 88(2), 105–116 | PMC 全文原句照抄 |
| MAD 尺度估计量的 37% 效率与标准化方差 **1.361**（⇒ `1.166 = √1.361`） | Rousseeuw & Croux 1993, JASA 88(424), 1273–1283 | 官方镜像 PDF 全文 + Table 2 |
| Tukey biweight 出处 | Beaton & Tukey 1974, Technometrics 16, 147–185 | **仅二手归属**（原文无 OA 全文） |
| IRLS 权重常数表 | Holland & Welsch 1977, Comm. Statist. 6(9), 813–827 | Crossref/OpenAlex 书目级 |
| 反方差加权口径 | Aitken 1935, Proc. R. Soc. Edinb. 55, 42–48 | **仅二手归属**（原文无 OA 全文） |
| 谱插值基元（Akima 子样条） | Akima 1970, J. ACM 17(4), 589–602 | Crossref 书目级 |
| PHOTFLAM/PHOTPLAM/PHOTBW 定义 | STScI WFC3 Data Handbook §9.1；Bohlin, Hubeny & Rauch 2020, AJ **160, 21** | 官方手册 + Crossref |

### 10.2 开源实现（项目 + 版本 + 文件:行）

| 实现 | 锚 | 支撑的结论 |
|---|---|---|
| astropy 8.0.1 `astropy/stats/funcs.py:945` | `mad_std` 返回 `MAD × 1.482602218505602` | 与本实验 `1/0.6744897501960817` 同源。**本机运行版本是 astropy 7.0.1**，语义相同、行号可能不同，未在 7.0.1 上核对该行号 |
| statsmodels `robust/_tables.py` L16–L27 | L24 逐字 `0.95: (4.685065, 0.119414),` | `c = 4.685` 的第三方数值锚（与本实验解析解 4.6850649 六位一致） |
| photutils 3.0.0 `detection/daofinder.py:26,:210` | 文档逐字 "If `xycoords` are input, the algorithm will skip the source-finding step." | 星表引导可跳过源查找的一手语义证据 |
| photutils 3.0.0 `psf/photometry.py:217`、`aperture/photometry.py:30` | `PSFPhotometry` / `aperture_photometry` | PSF 测光与孔径测光的口径对照 |
| sep 1.4.1 `sep.pyx:387` | `cdef class Background` | 背景估计对照 |
| SExtractor 2.28.2 `src/analyse.c:310,:561` | `obj->fluxerr = sigtv;` / `sqrt()` | 误差传播对照 |
| GaiaXPy 2.1.4 `src/gaiaxpy/spectrum/sampled_spectrum.py:114` | 纯线性组合 `coefficients @ design_matrix` | F_syn 绝对口径的实现侧旁证（不乘任何星等因子） |
| SVO Filter Profile Service | `HST/WFC3_UVIS2.{F657N,F673N,F502N}` | 总系统透过率曲线，SHA256 见 README §3.1 D4 |

> 本单元**未**在本环境安装运行 photutils / sep；上表行号来自文献与上游源码核对，不是本机
> 安装版本（诚实边界，见 §8 与 `README.md` §6.7）。

### 10.3 仓内实测（复现命令 + 产物路径）

| 证据 | 复现命令 | 产物 |
|---|---|---|
| 主实验轮 step1–step9 | `bash code/run_all.sh` | `results/step1..step8*.json`、`results/GATES.md`、`results/gates.json` |
| 重做三路（seed 20260926） | `bash code/redo/run_all.sh` | `results/redo/route{1,2,3}/*.json`、`results/redo_summary.json` |
| 低阶空间增益逆向验收 | `code/reverse_verify/p1_spatial_gain/src/*.py` + `cpp/p1sg_oracle.cpp` | `results/reverse_verify/p1_spatial_gain/data/*.json` |
| **合成器物理链自检（公共前置步）** | `bash 实验/shared/synthetic/run_selftests.sh` | 四组件全 PASS，退出码 0 |
| 判据表汇总 | `python3 code/step9_collect.py` | `results/GATES.md` / `results/gates.json` |
| 独立审稿（非作者，两轮） | — | `results/REVIEW.md`（22 条意见；5 条真实硬伤的处置见 `REPORT_experiment.md` §7） |

### 10.4 合成器物理链与共享链的口径对照

本单元的采样段（`code/scia_sim.py` 的 `forward()` / `stamp_photometry()`）与共享物理链
`实验/shared/synthetic/noise_model.py` 的 `expose()` 执行同一条链
`λ_e = (源+天光+暗流)·m(x,y) → Poisson(λ_e) + N(0,σ_R) → 饱和·取整(n_e/g)`。
两者三处差异（平场是否乘暗流、量化方差 1/12、偏置基座）及其量级上界，
以及本单元**不能**由共享链替代的四项（已知系数空间增益、逐星真值位置渲染、逐星加权 PSF
最小二乘测光、纯解析代数合成臂）逐条写在 `code/scia_sim.py` 的模块 docstring
「与共享物理链的关系」一节。采样段收敛到共享链需以「重跑并重新落盘全部归档 JSON」为前置。
