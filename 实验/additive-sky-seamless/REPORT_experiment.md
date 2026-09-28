# P5 加性天光去除与无接缝叠加 · 实验报告（审计重做收口版）

**单元**：`实验/additive-sky-seamless/`（SCI-C，科学链第 5 点）
**性质**：整合历史正本实验（C1–C7）＋独立审计三路重做（路线1/2/3）＋两个补实验的定稿实验报告。**订正轮（SCI-705 审查后）已在 HEAD 上重跑**：纯解析腿（route1/c3、route2/e5…）**逐位复现**固化读数；探针腿 C1/C3/C7 与固化读数**不一致**且 C1 自身门转红——逐条见 §6 末三条。
**事实源**：`独立审计/实验重做/总编对账/分歧台账.md`、`五单元成稿简报.md`；争议一律以台账裁决为准。

---

## 1. 假说

- **H1（主假说）**：纯加性天光世界中，UPM 四要素（排除自身的参考面、阻尼 α≈0.5、拟合/堆叠权重同源、末端残差场扣除）使任意覆盖子集上的加权阶跃恒为零，产品保留公共背景 B_ref；帧间乘性差须先由 Phase1 吸收。
- **H2（张力假说）**：猜想失效边界 = 帧间天光差含「参考面不可表示且相干」的分量；残余接缝与该分量 RMS 线性相关，显著尺度由节点间距控制。
- **H3（定权假说）**：control_variance = k_corr·(π/2)·σ_bg²/N_retained 在 N≥65 渐近域内成立；有限 N 处的偏差方向、幅度与 k_corr 的几何依赖可标定。
- **H4（判据假说）**：max|rel_step| ≤ 1e-2 构成「确定性地板＋统计检出」的复合判据，其确定性行为、统计行为与漏检面可被独立复现。

每条假说配「真值无效应⇒度量归零」负例，判据非退化。

## 2. 方法

- **历史正本**（`code/c1–c7*.py`，固定 seed，读数 `results/c1–c7*.json`）：HST 模板＋物理前向仿真世界，生产代码只读探针（`upm_probe.cpp`/`sky_probe.cpp`），证据分级 data/meta。
- **审计重做三路**（归档 `code/audit_rework/{route1,route2,route3}/`，读数 `results/audit_rework/`）：纯 numpy 解析合成，每路独立实现、独立 fixture，负例齐备。
  - 路线1：C1 中位数方差精确积分、C2 k_corr drizzle 前向 MC、C3 接缝门、C4 方差比、C5 Huber 效率、C6 可辨识性/dof、C7 权重两级口径、C8 尺度容差、C9 表示边界、C10 锚取证。
  - 路线2：e1–e10 与路线1 对应面＋smoothing λ 偏差–方差、σ_floor 跨尺度。
  - 路线3：exp01–exp12 十二项独立重做（含零锚权重、权重臂、尺度不变性、质量因子）。
- **补实验**：`supp_507_relstep/`（ea：×5.07 尺度扫描复算；eb：rel_step_max=0.1 标定性）；`supp_control_variance/`（P2 单元，D-07 定稿口径三脚本）。
- **seed**：路线2/路线3/补实验 = 20250926（写死于脚本头/`p5c_common.SEED_BASE`）；路线1 按主题分段（接缝门 20260319、方差比 20260320、表示边界 20260325、其余 20260926 系）。seed 不可经命令行覆盖。

## 3. 数据

1. HST 真实信号模板＋完整物理前向（历史正本）；2. 纯解析合成＋负例（三路＋补实验）；3. testdata 真实数据 M42，**两条腿必须分开读**：(3a) 单元内 C7 = M1/T3 **同一指向的 4 帧** 512² crop（**不是 49 帧**），其 6 条「边界」是 x = 128/192/…/448 的**合成 cell 边界**（`code/c7_realdata.py` 的 BOUND），**不是真实帧足迹边界**；(3b) **真实帧足迹边界**的读数来自**生产 49 帧端到端产品**（逐字冻结记录 `results/production_e2e_seam_record.json`）。详见 REPORT_paper.md §3(3a)/(3b)。

## 4. 结果

（完整数字与来源路线见 `results/audit_rework_summary.json`；此处按假说列主数。）

### 4.1 H1：纯加性世界（成立，域内）

| 量 | 读数 | 来源 |
|---|---|---|
| 背景接缝中位（未校正→δ_k 臂） | 4.80 → 0.383 e⁻（12.5×；各用例 12.5×–37.5×） | [实验:c1_additive.py] |
| 产品中位 vs B_ref（**δ 臂**，`product.delta_median`） | 299.17 ≈ 297.33 e⁻（保留公共背景；同 fixture **全减臂** `product.full_median` = 0.711 e⁻，对照） | [实验:c3_public_plane.py] |
| 全减背景退化臂 | 0.845 e⁻、**48.4% 负值像素**（`full_neg_frac = 0.4839820861816406`，出自 `code/c3_public_plane.py:131` ⇒ `results/c3_public_plane.json`；**不是** c1 的读数，原表来源列写错，P5-01 订正）、度量上反而更好（0.275 < 0.383）——必须禁止 | [实验:c3_public_plane.py] |
| 乘性前提（Phase1 残差） | 0.560 → 5.89e-4；未做 Phase1 接缝 27.37 → 做后 6.31 e⁻（4.33×） | [实验:c2_multiplicative.py；P1 单元] |
| 非退化判据分离度 | 退化 0.09σ vs 非退化 36.4σ；假阳性 0/60；5σ 检测限 6.98 e⁻ | [实验:c4_seam_criterion.py] |
| 稀疏/稠密等价 | max\|Δ\| = 3.1e-15；14,001 B vs 8,389,129 B（0.167%） | [实验:c6_sparse_dense.py] |
| 真实数据 | 乘性斜率中位 0.995、corr(slope,intercept)=−0.9981、动态范围仅 8.5 ADU ⇒ 截距不可辨识 | [实验:c7_realdata.py] |

### 4.2 H3：定权（D-07/D-08 终裁口径）

| 量 | 读数 | 来源 |
|---|---|---|
| 渐近域 | N≥65 时 MC 偏差 <2% | [实验:route1/c1_median_variance.py] |
| N=5 纯公式口径 | 渐近式**高估 ≈9.5%**（精确积分 0.314159 vs 0.286834，9.53%）——保守方向 | [推导]＋[实验:route1/c1、route2/e1（+9.6%）、route3/q3（0.913×formula）] |
| N=5 端到端口径 | ≤±1.5% | [实验:P2 补实验 supp_control_variance] |
| N=5 生产链亮端裁剪臂 | 低估 1.3–3.2% | [实验:P2 补实验 production_chain_control_variance.py] |
| k_corr 身份 | 标定几何专属实测带 1.27–1.43（受控复现 1.3445±0.0416）；1.3883 为带内一次实现 | [实验:p3_kcorr/g1_canonical] |
| k_geo | 紧凑 1.27±0.03 / 全 touched 1.43–1.45 / 分散 1.00 / 扫描全域 1.00–5.0；多帧 1.424/1.338/1.328 | [实验:p3_kcorr/g2–g4] |
| 冻结 1.4 失保守 | N=5 紧凑端低估 32%（k_corr≈2.05±0.09）；源 583–600″/px 端低估约 2× | [实验:p3_kcorr/g6_fit] |
| 口径差 | share/absolute 14.8%（0.14830）；伪 ivar 1.314e26；公共因子消去 1.5e-16 | [实验:route1/c7、route2/e6、route3/q9/q10] |
| 堆叠方差用例 | 1/Σw = 0.05931 | [实验:route2/e6] |

### 4.3 H4：接缝门（含漏检面）

| 量 | 读数 | 来源 |
|---|---|---|
| 确定性下限 | 闭式 `gate/(1−gate/2)` = **0.0100502512**（= 1.0050251%，**不是**对数口径 e^{1e-2}−1 = 0.01005017——两者在 1.0050% 精度内巧合一致，口径归属按 P5-15 订正）；固化档：Δ/L = 1.00503%（键名 `0.010050`）读数 0.01000005 **判红**、Δ/L = 1.0050% 解析 0.00999975 **判绿**、1.0049% 判绿 | [推导]＋[实验:route1/c3_seam_gate.py] |
| H₀ 散布 | 1.151e-3 vs 公式 1.187e-3（0.970）；虚警 0/4000 | [实验:route1/c3] |
| 多重性检出 @1.05% | 解析 0.6462/0.8748/0.9843、MC 0.6518/0.8700/0.9828（n_s = 1/2/4；n_s=1 处两口径差 0.006 = MC 波动） | [实验:route1/c3] |
| 平滑过渡漏检面 | 观察台阶 ×2d/w；2% 名义台阶判绿 | [实验:route1/c3] |
| **光滑斜坡伪阳面（新增登记）** | 判据量含梯度项 `rel_step = 2d·ρ + Δ/bg` ⇒ ρ ≥ gate/(2d) = 0.25%/px **单独**即判红而真值无接缝：ρ=0.26%/px ⇒ 0.0104 判红、ρ=0.10%/px ⇒ 0.0040 但 d 扫描量 0.0160 越门 1.6×；纯斜坡 d 扫描比值恒 4、真台阶恒 1 | [实验:code/seam_gate_gradient_scan.py，seed=20260928] |
| 梯度相消漏检面 | 1.005%＋反号梯度 → 0.06% 判绿（漏检面 ≈1.73%，01 D-57） | [实验:route1/c3] |
| 负例 | 无台阶 median rel_step=0，4000 MC 全绿 | [实验:route1/c3] |
| f<1/2 失明限定 | 仅无噪极限；M42 级噪声 f=0.3、Δ/L=5% 读数 1.03e-2 | [实验:route3/exp01]（A-P5-05） |
| 方差比正交性 | 电平台阶下 VR≈0.977/rel_step 判红；方差伪影下 rel_step 判绿/VR≈3.96；VR_pool=1.25/2.01/5.03 @Δ/σ=1/2/4 | [实验:route1/c4、route2/e4、route3/exp02]（A-P5-04） |

### 4.4 H2：失效边界（A-P5-11 改写口径）

| 量 | 读数 | 来源 |
|---|---|---|
| 峰值/长尺度比 | ≈8（原 8.03 vs 复算 8.02、第三实现 9.21）——跨实现稳健 | [实验:supp_507_relstep/ea_507_scale_scan.py] |
| 端点比（原 ×5.07） | 不复现：2.95（带噪）/2.43（无噪）/1.78（gauge）/1.71（RMS） | [实验:ea_507_scale_scan.py] |
| 形状 | 非单调（峰 100 px、50 px 回落）；肘点 s≲2h≈256 px | [实验:ea_507_scale_scan.py] |
| 线性结构 | 成立；斜率 fixture 专属（0.798/0.8964 vs 0.207/0.8006），不迁移 | [实验:c1_additive.py、route1/c9、route3/exp08] |
| 负例 | λ→∞（可表示）残余接缝精确为 0 | [实验:route1/c9] |

### 4.5 判据面收口

- Huber δ=1.345：δ(0.95)=1.3449975（闭式二分）；MC 效率 0.948/0.9507；σ_floor 主导域退化为 median [实验:route1/c5、route2/e5/e10]。
- rel_step_max=0.1：前提改正（非 IRLS 步长上限）＋不可标定（B0=5 假红 45–60%、B0=1000 永不红）＋结构性失明（≤4/112 邻对）＋松 20× ⇒ **判据面除名**（A-P5-10）[实验:supp_507_relstep/eb_relstep_calibration.py]。
- 幻觉锚：05_正向规格 12 项证伪 vs 3 项证实 ⇒ §9/§10 整段回炉（A-P5-03）[实验:route1/c10_anchor_forensics.py]。
- dof：E[χ²]=n_obs−r_eff（Andrae 式(9) [文献]）；秩亏时 n_params 分母使 χ²_red 高估 1.0714/1.0581（方向词按 A-P5-01 订正）[实验:route1/c6、route3/exp06]。
- H_solve 恒真门：判决面 = `H_red` 条件数、唯一阈值 = `rank_rtol` τ，数值岭只剩派生角色 `λ_eff = τ·mean(diag(H_red))`；`κ(H_solve)` 只作求解稳定性诊断（κ = 23.14 的「生产 0.1·mean(diag)」是**已退休的 fixture 假定**，不得再当生产值引用；同 fixture 取 λ = τ·mean(diag) 时 κ = 2.214e10 而 `r_eff = 23 < n_free = 24` 逐位不变 ⇒ 恒真门）（A-P5-08）[实验:route3/exp07、route2/e8]。
- 尺度容差：scale_obs=5.26e13 处 ULP=0.0078125；「≈1 ulp」为实现依赖声明（A-P5-02）[实验:route1/c8]。
- quality_factor_initial=0.5：基准值精确抵消（max|Δθ|=0.0）；0.1/0.5 豁免为项目约定（A-P5-12）[实验:route2/e6、route3/exp12]。

## 5. 结论

1. H1 成立（可表示域内），背景保留与接缝压缩均有生产代码实测；退化做法被非退化判据锁定。
2. H3 定稿：公式本体成立；N=5 三口径（+9.5%/±1.5%/−1.3~−3.2%，D-07）；k_corr 两因子查表＋引用义务（D-08）。
3. H4 成立且漏检面齐备；判据面仅存接缝门 1e-2 与 Huber δ=1.345；rel_step_max=0.1 除名。
4. H2 结构性主张成立、标度统计量改写为峰值比 ≈8＋非单调＋肘点 2h；斜率为 fixture 专属。

## 6. 诚实边界

- 补实验实验 A 为独立 Python 求解器（非生产二进制）：50 px 端点 2.60 vs 已发布 4.52、800 px 中段 1.03 vs 1.84；1600 px 端点与峰值点吻合；已发布序列本身非单调——任何端点比都不稳健，这正是 A-P5-11 结论的一部分。
- 信号模板为解析弱场替代 HST 真实模板（cell 中位数泄漏已压至 ~0.9 e⁻ 并登记）；RMS 伪影口径带噪臂被两次独立 Poisson 实现之差主导。
- 实验 B 为 2 帧玩具几何；步长上限是反事实注入参数，生产无对应物；κ 扫描单一场景单一污染率。
- 接缝门 fixture 未含双线性亚像素方差因子与足迹多边形几何（二阶效应 4.5e-3）。
- **三类数据的分歧（P5-05）**：②类（解析合成）绿、③a（4 帧单指向）绿，而 **③b（生产 49 帧全帧）就「无接缝」主张红**——门只覆盖 114/196 条边界（82 条 `not_interior` 不进判据），且同一读数块内三个独立诊断量全部越门（`rel_step_net_max = 1.2948e-02`、`rel_step_d4x_max = 2.1915e-02`、`legacy_rel_max = 2.6110e-02`）。本报告不主张「三类一致」。
- **生产读数取证性质**：③b 是**冻结记录**（源 run 树为过程产物、产品 FITS 已被回收）⇒ 可核对面 = 记录内 sha256（4/4 逐字一致）＋自检器 `code/production_e2e_record_check.py`（17 项检查，能红能绿），**不构成可现场重跑的端到端证据**。
- **像素尺度订正（P5-04）**：全稿撤下无来源的 `0.989″/px`；实验网格 = 1.0″/px（`code/c3_public_plane.py:27`），生产 = 0.9404″/px（记录 `pixel_scale_arcsec_derived`）⇒ h = 0.0355° = 127.8 px、肘点 2h = 255.6 px。
- **HEAD 复跑结论（订正轮实测，详证见 REPORT_paper §6 与 `run/FINAL-07/logs/`）**：①**纯解析腿逐位复现**——`route1/c3_seam_gate.json`、`route2/e5_huber_delta_1345.json`、`route2/e3_seam_gate_1e-2.json` 与固化拷贝 **byte-identical**；②**探针腿不能复现**——在 HEAD 上重建探针（需同步两处已退休接口键与新增静态库）后重跑 C3/C7/C1，同一 fixture 下求解器行为已变（`n_params` 58→49、`kappa` 3.0e6→3.5e3、`iterations` 20→28、`model_hash` 变），读数差 1e-3–1e-1（`full_neg_frac` 0.48398→0.48507、`seam.delta_med` 0.386→0.439）；③因此 `code/run_all.sh` 在 HEAD 上 **rc=1**：全部 7 条腿逐条状态 = C1 **FAILED**（`A6_subset_invariance` 0.0236→0.4078、`A9_final_gauge_near_noop` 0.0037→0.1327，`n_fail` 0→2）、C2 OK（150/383 叶子差，多为 1e-13）、C3 OK、C4 OK（100/170 差在 1e-14 级）、**C5 FAILED**（`W1_control_ivar_best`：`margin_snr2` 0.2592→0.1753 < 门槛 0.20；漏入 0.0245/0.0651/0.3203→0.0761/0.1216/0.3590，**13× 优势降为 4.7×**）、C6 OK（稀疏系数落盘 14001→14849 B、`model_hash` 变）、C7 OK ⇒ **results/c1–c7*.json 只具历史效力**，不得当作 HEAD 行为证据；C3/C7 的定性结论（背景保留、δ 臂压缩接缝、全减臂 48.4% 负值）在新读数下方向不变，**C5 的定量优势不可复现**。
- 生产链被估量 y 的合同语义（裁剪后中位数 vs 全样本中位数）待负责人裁决（P2 链，不属本单元）。
- support_min=0.2 出处未定（降级为实现默认）；min_cluster_size=3 幻觉锚随 05 回炉；Tikhonov 1963 题录未核不作依据；Serfling/Kendall pinpoint 书目级标注。
- 11_upm §4.4 w∝SNR² 与正本 §5/§10 互斥，禁令维持；本几何泄漏 18% 不得迁移（A-P5-06）。
- 纯解析腿的读数经 HEAD 复跑逐位复现；探针腿 C1–C7 的固化读数**不能**在 HEAD 上重放（见上条）——引用时一律挂 [实验:c*.py] 并视为历史读数。**commit 口径（P5-18）**：C1–C7 原始运行的 commit 在仓内**不可判定**（`run/SCI-403/` 无 ROUND.md、源目录只有 logs 与探针输出）；订正轮的全部重跑基线 commit = **6808a7e4**（报告撰写时 HEAD 已前移至 58e9dd42，该提交为工程线文档清理，未触及本单元任何文件、`docs/science/PHASE2_UPM.md`、`docs/KNOWN_LIMITATIONS.md` 或 `11_upm.md`），重跑日志逐条落 `run/FINAL-07/logs/`。

## 7. 复现命令

```bash
# 审计重做三路 + 两个补实验（固定 seed，python3+numpy，无网络，零 git 写）
bash 实验/additive-sky-seamless/code/audit_rework/run_all.sh

# 历史正本 C1–C7（需先构建探针）。**在 HEAD 上 rc=1**：C1 自身两项门由 PASS 转 FAIL（见 §6 末条），
# 读数不能逐位复现 ⇒ 该入口只用于"现场复核历史读数为何不可复现"，不作为复现证据。
bash 实验/additive-sky-seamless/code/run_all.sh

# 单项示例（在**干净检出**上可跑：各脚本已自建输出目录，P5-21 订正）
cd 实验/additive-sky-seamless/code/audit_rework/route1 && python3 c3_seam_gate.py
cd ../supp_507_relstep && python3 ea_507_scale_scan.py && python3 eb_relstep_calibration.py
```

产物：三路脚本就地写 `code/audit_rework/results/*.json`（该目录已 `.gitignore` 自我忽略，重跑**不覆盖**已入库的固化拷贝
`results/audit_rework/<route>/`；P5-23 订正）；历史正本写 `results/c1–c7*.json`（**会覆盖**入库证据 ⇒ `run_all.sh` 已加
"重跑前备份到 `run/SCI-403/results_prior/`"）。单脚本 CPU ≤ 90 s（C4 例外：本机负载下可达 20 min），全量 < 30 min。
补实验 `supp_507_relstep` 与 `supp_control_variance` 的输出位置按各脚本头注释。

## 8. 佐证来源

- 文献：`refs.md`（只收一手 VERIFIED 条目；标注级单列）。
- 代码证据：历史正本 `results/evidence_code.json`（35/35，项目＋文件:行）；审计三路 report.md 位置见台账引用。
- 事实源：`独立审计/实验重做/总编对账/{分歧台账,五单元成稿简报}.md`；三路 report.md 与补实验 report.md（规范目录原件，只读）。
- 汇总：`results/audit_rework_summary.json`（每个数字注明来源路线与固化文件）。
