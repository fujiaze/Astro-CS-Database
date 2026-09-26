# 实验报告 · P1 通量积分拟合（测光星等坐标系）

**单元**：`实验/photometric-magnitude`（SCI-A，SCI-401）。本报告是实验重做轮的成稿实验报告：整合历史正本轮实验（step0–step9，seed=20260921）与三路独立重做（路线1/2/3，seed=20260926）；既有历史文档（`README.md`、`RESOLUTION_fsyn_formula.md`、`RESOLUTION_m42_curve_resolve.md`、`results/REVIEW.md`、`results/DOC_CORRECTIONS.md`、`results/GATES.md`）保留，失效处按台账订正并在文内注明。
**裁决依据**：`独立审计/实验重做/总编对账/分歧台账.md`（D-xx/A-xx）与 `五单元成稿简报.md`。本单元无 P1 专属补实验目录（`补实验-control_variance / -k_corr / -5.07` 分别由 P2/P3/P5 承载）。
**论文版**：`REPORT_paper.md`；文献台账：`refs.md`；推导：`docs/derivation_robust_weights.md`。

---

## 1 假说

- **H1（主假说）**：存在逐帧标定系数 `k_photo` 与低阶空间增益 `m(x,y)`，使 `I_photo = k_photo·m(x,y)·I_cal` 后同一物理通量的星在不同帧/位置/天光水平下星等一致；一致性可由本帧自算误差预算的双边界判据判定。
- **可证伪点**：①真值无效应 ⇒ σ_obs = 0 且判红；②乘性空间残差注入 ⇒ σ_obs 单调上升并判红；③σ_obs 落入 `[σ_floor, σ_ceiling]`；④`k_photo` 不含绝对数值窗口（尺度无关）；⑤`k_photo` 不可反解仪器参数。
- **H2（工程假说）**：Gaia 位置引导检测优于盲检测，优势依赖二轮 WCS 平移精化 ≲2 px。
- **重做轮新增假说**：H1′（常数体系）——c = 4.685 是"正态 95% 渐近效率"标准调参值、0.6744897501960817 是解析恒等式 Φ⁻¹(3/4)、平移不变量逐位成立；H1″（系统差源）——星等窗/FOV/预过滤等"约定常数"的实际危害可量化且方向可判。

## 2 方法

- **参考通量**：`F_syn = ∫F_λ·T·Q·λ dλ`（W·m⁻²·nm，官方 343 点 @2 nm，336–1020 nm；G<15 域）；Akima 子样条 + 复合 Simpson 1/3（奇区间 3/8、n==1 退梯形）；**不含 `10^(−0.4·G)`**（`RESOLUTION_fsyn_formula.md` 判定，生产实现逐位一致）。插值/求积设置配置化 + 运行日志不落盘（负责人已批）。
- **稳健零点**：`r_i = log10(F_instr/F_syn)` [dex]；固定尺度 Tukey biweight IRLS（c=4.685，tol=1e-6，max_iter=50）；`k_photo = 10^(−location)`；`sigma_residual = MAD(r_inliers)/0.6744897501960817`。
- **双边界判据**：`σ_floor/σ_ceiling = (1 ∓ 3·1.166/√n)·(下/上界)`；1.166 = √1.361（MAD 标准化方差，**按台账 A-P1-01 订正标签**）；预算项各计一次。
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
- 判据：3 仿真帧 + 1 真实帧全 PASS（σ_obs = 0.045344/0.057457/0.051718/0.026520 mag；观测/预算比 1.009/0.727/0.390/—）[实验:code/step5_calibration_gate.py]。
- 负例齐备：真值无效应归零、乘性残差单调判红、散粒敏感（2.91×，4× 判红）/加性不敏感（1.08×）[实验:code/step7_negatives.py]。
- 低样本：n≲22 下界失效（n=5 时 σ_floor=−0.004911）⇒ 只有上界是硬约束 [实验:code/step5_calibration_gate.py]。
- 引导检测：匹配率 0.2254/0.1972 → 0.9859；4096² 真实帧盲检 13163 检出仅 1.49% 对应星表星、30.3× 拟合次数；WCS 平移精化 ≲2 px 是前提（HST drz ~1.6″ vs 真实帧 0.0068″）[实验:code/step4_guided_vs_blind.py]。
- 判读：**H1 成立（条件划定）、H2 成立（带 WCS 前提与循环性边界）**。

### 4.2 重做轮三路（results/redo/，seed 20260926）

**路线1（S1–S18 + H1–H6 全处理）**：
- S1/S2/S3/S13：c=4.685、MAD 系数、IRLS 容差、1.166 因子三腿复核通过；√1.361 = 1.1666 推导腿闭合 [实验:route1/exp1]。
- S4：mag_tolerance=3.0 边际保护 ≈0（IRLS 就位后），作用面 = 样本族控制 [实验:route1/exp2]。
- S5：FOV 三常数为项目约定；B12 危害路径量化 −0.146 dex；奇异 CD 可通过 |r_consistent|≥3 门 [实验:route1/exp3]。
- S6：阶梯"宁多查一档也不空手"；早停只在 >5× 平均密度场生效；"上界 10000"注释宣称未实现（实锚 pc_api.cpp:290 vs :297-301）[实验:route1/exp3]。
- S7：星等窗 → ZP 平移最高 −0.0318 mag（两消费面口径分裂量化）[实验:route1/exp2]。
- S12：空间增益阶数是科学量；S15：ZP_syn 下限 3 的有限样本偏差量化；S16/S17 豁免论证；H1 锚 STALE（PHOTOMETRY.md:227/:356 → A-P1-04<!-- 订正: 检查-跨文档冲突 黄9——原 :355，⑥ 行实为 :356 -->）[实验:route1/exp4, exp5]。

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
- S11：n=3 MAD 偏差 1.49×、渐近式 n=3 高估 7.4%、ZP 三星下限抽样误差 25.7× [实验:route3/exp_S11]；S12：1.0 dex 界 0.055/1.110 非恒真 [实验:route3/exp_S12]。

**判读**：H1′ 成立（常数体系三腿闭合，锚体系整体真实）；H1″ 成立（约定常数的危害方向与量级确认，豁免判定按 §6 清单）。

## 5 结论

1. 测光星等坐标系的参考通量口径 `F_syn = ∫F_λ·T·Q·λ dλ` 与稳健零点估计（Tukey biweight c=4.685）三腿闭合；常数体系（c、0.6745、1.4826、1.166=√1.361）全部有文献/推导/实验三重支撑。
2. 判据非退化性与适用域确立：真值无效应归零、乘性残差单调判红、只有上界是硬约束、不能认证天光扣除质量。
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
| max_stars=5000 | 工程上限 | σ_ZP(N) 曲线支撑（5000 ⇒ 6.36e-4 mag）[实验:route3/exp_S08] |
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

## 8 诚实边界

见 `REPORT_paper.md` §7（八条全列）。补充实验口径：①历史正本轮全部预算项在仿真帧上由本帧推导（含真值重算的 σ_color/σ_flat），真实帧上 σ_color/σ_gaia 不可自算、标 `null` 且上界不完整；②仿真引导匹配的定位输入即注入真值（结构性循环），非循环证据来自真实帧；③σ_psfsys 孔径口径 r=4 为作者约定（保留三方对照）；④重做轮计数模型依赖天空平均密度假设（820/deg²，G<16 全天平均），非逐场实测；⑤仍开放（不阻成稿）：Lindegren 2021 亮端数字、R&C 原文、Huber & Ronchetti 页码、σ_floor/ceiling 政策值、ZP_syn 下限政策值、ieq 工程修复——**UNRESOLVED 项一律不进论文正文**，此处为唯一登记面。

## 9 复现命令

```bash
cd <repo root>
# 历史正本轮（seed 20260921）
bash 实验/photometric-magnitude/code/run_all.sh            # step0–step9（quick 跳过最慢 step6）
bash 实验/photometric-magnitude/code/step0_fetch_refs.sh   # 唯一需网络（脚本内钉死 SHA256）
# 实验重做三路（seed 20260926）
bash 实验/photometric-magnitude/code/redo/run_all.sh       # route1–route3；quick 跳过 route2 exp3/exp7
# 单脚本示例
python3 实验/photometric-magnitude/code/redo/route2/exp1_robust_constants.py
# 基准读数：results/step1..step8*.json、results/redo/route{1,2,3}/*.json；汇总 results/redo_summary.json
```

固定 seed 说明：历史轮 `20260921`（`scia_common.rng(tag)` SHA256 派生）；重做三路统一 `20260926`（脚本内写死；路线1 部分脚本派生 SEED+1…）。RNG 一律 `numpy.random.default_rng(SEED)`。重跑读数与基准快照逐项一致（固定 seed 保证）。
