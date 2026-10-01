# SCI-A 实验单元 · 测光校准到星等坐标系

- **任务**：`SCI-401`（控制包 RELEASE-04，最高优先级科学实验单元 SCI-A）
- **固定种子**：`20260921`（所有 RNG 由 `scia_common.rng(tag)` SHA256 派生）
- **一键复跑**：`bash 实验/photometric-magnitude/code/run_all.sh`（`quick` 跳过最慢的 step6）
- **判据表**：`results/GATES.md`（机读 `results/gates.json`）
- **规范依据**：`docs/ASTROCS_DESIGN.md` §2.1 / §4.2 / §4.4 / §12.1–12.3；
  `ACCEPTANCE_SPEC.md` §2.1 判据表；`docs/detail/registry/astrocs.phase1.photometry.md` **§4.1**
  （"测光一致性判据（从误差预算推导）"，判据形态逐字来源；该文只有 §1–§8，**没有 §2.1/§3.1**）；
  `docs/science/PHOTOMETRY.md`（推导出处）；`run/RELEASE-02/parallel/06.md` §2（逐项预算实测）
  ——注意 `run/` 是 gitignore 目录，追溯性弱于 `docs/`，前台合并时应把引用改锚到 `docs/`

### 成稿交付（实验重做轮，2026-09）

- **论文**：`REPORT_paper.md`（三路重做证据融合重写版）；**实验报告**：`REPORT_experiment.md`
- **文献台账**：`refs.md`（只收一手 VERIFIED 条目）；**支撑推导**：`docs/derivation_robust_weights.md`
- **重做轮代码/结果**：`code/redo/`（路线1/2/3，seed=20260926，`bash code/redo/run_all.sh`）与 `results/redo/`（基准快照）、`results/redo_summary.json`（关键读数汇总，注明来源路线）
- **裁决依据**：`实验/裁决台账.md`（跨单元 D-xx）与 `docs/DISPUTES.md`（本单元 A-P1-xx）；科学正本同步件 `docs/science/DISPUTE_RESOLUTION.md`；订正记录见 `REPORT_experiment.md` §7（A-P1-01/04/06/08/09/12、D-06）
- **无 P1 专属补实验目录**：`补实验-control_variance / -k_corr / -5.07` 分别由 P2/P3/P5 承载

---

## 1. 可证伪的假说

> **H1（主假说）**：存在一个**逐帧标定系数** `k_photo` 与**低阶空间增益** `m(x,y)`，使得
> `I_photo = k_photo · m(x,y) · I_cal` 之后，同一物理通量的星在不同帧、不同位置、
> 不同天光水平下给出的**星等**一致；且该一致性可由**本帧自算**的误差预算双边界判据判定。
>
> **H0（零假设）**：上述一致性不存在——残差散度不随预算项变化、或判据对真值无效应不归零。

**可证伪点**（任一条不成立即 H1 被否证）：

1. 真值无效应（`F_instr = inject_scale·F_syn` 精确、`m≡1`、无噪声）时，`sigma_obs` 必须为 0 且判据判红；
2. 注入**乘性空间残差**（平场残差）时 `sigma_obs` 必须单调上升并在足够大时判红；
3. `sigma_obs` 必须落在由本帧逐项预算推出的 `[sigma_floor, sigma_ceiling]` 内；
4. 标定系数 `k_photo` **不得**含绝对数值窗口（尺度无关）；
5. 由 `k_photo` **不得**反解出增益/口径/曝光（物理闭合不可辨识）。

**附加假说 H2**：Gaia 星表位置引导检测相对全图盲检测，在**匹配率**与**每匹配星算力**上占优，
且该优势依赖二轮 WCS 精化收敛到 ≲2 px。

---

## 2. 方法、公式与判据

### 2.1 合成测光约定（生产口径 `SCI-PHOT-001` §2a / `spectrum_integrator.cpp`）

```
F_syn = ∫ F_λ(λ)·T(λ)·Q(λ)·λ dλ          # 单位 W·m⁻²·nm；不含 10^(-0.4·G)
F_λ   = byte·flux_mul + flux_min         # 绝对谱辐照度 W·m⁻²·nm⁻¹
```

- `F_λ`：天体 SED（Gaia DR3 XP **外部定标**采样谱，343 点 @2 nm，336–1020 nm；单位 W·m⁻²·nm⁻¹）
- `T`：通带透过率；`Q`：探测器 QE（**通带的组成部分**，未配置时 `Q≡1` 为显式未建模项）；
  插值为 **Akima 子样条**（区间外填 0），求积为**复合 Simpson 1/3**（末尾奇数区间用 3/8，`n==1` 退梯形）；
  **退化分支 `n_int == 3`（订正 P1-m02）**：此时前段 1/3 的区间数 `n_13 = 0`，**1/3 部分必须为 0**——历史实现把 `y[0]` 计入两次，常数被积函数得 **3.6667** vs 解析真值 **3.0**（+22.2%）。生产端 `spectrum_integrator.cpp` 与本单元参考实现 `scia_common.py::simpson_integrate` 已按变更 claim `PHOT-SIMPSON-N3-001` 同步订正，`p1phot` 的 O3 组以闭式期望（常数 3.0 / 三次式 6.75 / n_int=5 组合分支 12.5）与故障注入名 `o3_simpson_n3_reference` 锁定；官方 343 点 XPSD 网格 `n_int = 342` 为偶，不触发该分支
- 本实验**未改动**该约定；只对它做独立 Oracle 校验（见 §4.1）

> **关于 `scia_common.f_syn(..., mag_g=…)` 的 `mag_g` 参数**：它是**星等指派重标度**
> （把源谱绝对通量重标到指定星等，`F_λ → F_λ·10^(−0.4·(m−G_source))`），**不是参考通量定义的一部分**。
> 本单元中 `step2_hst_sim.py` 对注入与模型使用同一因子 ⇒ 在 `r_i` 中**精确相消**，结论不受影响；
> `scia_calib.py`/`step7_negatives.py`/`step8_real_frame.py` 不传该参数。
> **不得把它当作通用参考通量公式搬用到别处**（会给逐星 `r_i` 注入 `+0.4·G_i` dex，单标量零点吸收不掉）。
> 完整订正见 `docs/fsyn_convention.md`。

### 2.2 零点估计（IRLS/Tukey）

`r_i = log10(F_instr,i / F_syn,i)`；`S = MAD(r)/0.6744897501960817`；`c = 4.685`；
`tol = 1e-6`；`max_iter = 50`；`location = Σw·r/Σw`；`k_photo = 10^(-location)`；
`sigma_residual = MAD(r_inliers)/0.6744897501960817`；`sigma_obs = 2.5·sigma_residual`。
预筛（**订正 P1-M03**）：`|delta_i − median(delta)| ≤ 3.0 mag`，`delta_i := −2.5·log10 F_instr,i − G_i`（G 已在该定义内减去一次，实现 `.py:123-126`）。历史写法 `|r − median(r)| ≤ 3.0`（r 为 dex）与同句「= 1.2 dex」自相矛盾，已订正。
**该窗与 `|r_i − median(r)| ≤ 1.2 dex` 不等价**（**订正 P1-M03-b**，G08-04 G2 独立复核推翻前一轮的等价判词）：代数上确有 `delta_i = −2.5·r_i − C_i`、`C_i ≡ G_i + 2.5·log10 F_syn,i`，但 `C_i` 是**逐星量、不是常数**——生产 `F_syn,i` 是逐星 SED 积分 `I_i = ∫F_λ,i·T(λ)·Q(λ)·λ dλ`（`code/scia_common.py:205-233`），随每颗星自己的 XP 谱形与色变化（`10^(−0.4·m)` 只是**额外相乘**的星等指派重标度，不属参考通量定义；本单元正本 `docs/fsyn_convention.md` §1 判「**不含 `10^(−0.4·G)`，`G` 既不进入 `F_syn`**」，与本节 §2.1 上方的引文同源）。两窗的中心与展宽因此不同源，**不等价**。`3.0/2.5 = 1.2` 只是**阈值换算**，**不是**两窗的等价性陈述；等价性成立的前提是 `F_syn,i` 只依赖 `G_i`（`C_i` 退化为星无关常数），该前提不成立。

### 2.3 双边界判据（`docs/detail/registry/astrocs.phase1.photometry.md`「测光一致性判据（单帧、尺度无关、双边界）」，逐字）

```
n             = 本帧匹配星数（Gaia 匹配 ∧ 有效域 ∧ IRLS inlier）
sigma_obs     = 2.5·MAD(r_inliers)/0.6744897501960817
sigma_floor   = (1 − 3·1.166/√n)·sigma_fit(白; 本帧匹配星通量分布)
sigma_ceiling = (1 + 3·1.166/√n)·sqrt(Σ 预算项²; 本帧)
PASS ⟺ sigma_floor ≤ sigma_obs ≤ sigma_ceiling
gate_scope   = "two_sided" if rho_lo > 0 else "upper_only"
判定序: sigma_obs > sigma_ceiling → ABOVE_CEILING;  scope==upper_only → LOWER_BOUND_UNDEFINED;
        sigma_obs < sigma_floor → BELOW_FLOOR;  否则 PASS
```

> **判据作用域（订正 P1-M07）**：`rho_lo = 1 − 3·1.166/√n ≤ 0` ⟺ `n ≤ (3·1.166)² = 12.236` 时
> 3σ **下**包络在数学上不存在。历史建议「下界取 `max(rho_lo, 0)·σ_fit`」**已撤回**——
> clamp 后下界恒为 0 而 `σ_obs ≥ 0` 恒真 ⇒ 下界恒不触发，比原缺陷更隐蔽，属**恒真门（无证据资格）**。
> 现语义：作用域降级 `upper_only`、状态词 `LOWER_BOUND_UNDEFINED`（**不记 PASS**），并把 `n`/`gate_scope` 一并报出。

> **因子标签订正（按分歧台账 A-P1-01）**：`1.166 = √1.361`，其中 1.361 是 MAD 尺度估计量在
> 高斯数据下的渐近**标准化方差**（Rousseeuw & Croux 1993 Table 2，refs.md V5）——即 1.166 是
> σ̂ 的相对标准差因子，`3·1.166/√n` 为 σ 估计量自身抽样涨落的 3σ 包络。推导见
> `docs/derivation_robust_weights.md` D5。

预算项（**仿真帧上逐项由本帧推导**；**其中 `σ_gaia=0.002` 在仿真上未被激活**——注入与模型共用同一 `mag_eff`，参考侧扰动在 `r_i` 中精确相消（订正 P1-m04，偏松方向）；真实帧上 σ_color/σ_gaia 不可自算、如实标 `null`，
见 §4.7）：光子噪声、PSF 拟合不确定度（精确 Fisher，含**自由背景**简并项）、
平场残余、天光扣除残余、颜色项（通带失配）、参考侧（XP 合成通量）、量化。
各项在 `sigma_ceiling` 的平方和中**各计一次**（轮次 1 审稿发现早期版本把系统项二次计入，
已修：见 `results/REVIEW.md` R7 与 §9）。

> **σ_flat 必须独立于被测样本（订正 P1-B01）**：σ_flat 是平场/大尺度响应残差的**物理**预算项。
> 历史实现取 `calibrate()['delta_after_m']`——那是**同一批星在多项式拟合之后**的残差散度，即 `σ_obs` 自身的函数
> ⇒ 上界随被测统计量同步膨胀、判据失去判别力（真实帧占上界方差 **47.6%**）。现约定：
> ① 仿真帧取**真值**逐像素平场散度经 `N_eff` 折算（`scia_common.sigma_flat_independent`，推导见该函数 docstring，
>    变更 claim `PHOT-SIGMAFLAT-INDEP-001`）；② 真实帧取**仓内约定常数** `σ_flat,hf = 0.0007 mag`
>    （活出处 `code/scia_calib.py:31`；一手出处未取得，见 §6 第 16 条）；
> ③ `delta_after_m` 只作**诊断字段**输出（`delta_after_m_role = diagnostic_only`），**不得**进入 `sigma_ceiling`。
> **反例化证据**：`results/step7_negatives.json → N6` 用同一批通量并行跑两种口径——自指口径下注入逐星散度使 `σ_ceiling` 同步膨胀，
> 判定**恒 PASS（无判别力）**；独立口径下同一注入在 0.05 mag 处**判红**。

**本实验不使用**：跨帧/组间一致性门（`PHOT-GATE-DROP-001` 明令禁止）、`0.03 mag` 常数阈值、
`k_photo` 绝对窗口、由 `k_photo` 反解仪器参数的物理闭合式。

### 2.4 apply photometry

`I_photo = k_photo · m(x,y) · I_cal`（设计 §4.2）。未启用时写显式 `degraded_reason` 且 **fail-closed**。

---

## 3. 数据来源与生成（三类数据互证）

| 类别 | 数据 | 角色 | 生成/复现 |
|---|---|---|---|
| ① 物理前向仿真 | HST M16 F657N HLSP drz（真实观测信号模板）24×24 分块平均 → ~0.95″/px；注入星位置/SED 取自真实 Gaia DR3 XP；逐像素 Poisson（源+天光+暗流）→ 高斯读出 → 增益 → 饱和 → 量化 | 真实结构 + 已知真值 | `step2_hst_sim.py` |
| ② 纯解析代数合成 | 600 星，SED 取真实 XP；无噪声组 + 多噪声组；`m(x,y)` 解析已知 | Oracle / 收敛性 / 负例 | `step1_analytic.py` |
| ③ testdata 真实帧 | FLI M42 M1 T2 Red 300 s（4096², uint16+BZERO） | 底参照（无真值） | `step8_real_frame.py` |

### 3.1 输入清单（来源 · 性质 · 用途）

本单元**不新增受版本控制的数据文件**：输入全部来自仓库内既有资产或公开服务，中间产物落
`run/SCI-401/`（gitignore，不入库），结果落 `results/`。

**D1 · HST M16 真实信号模板（真实观测）**

- 路径：`testdata/HST_M16/hlsp_heritage_hst_wfc3-uvis_m16_f657n_v1_drz.fits`
- 性质：HST/WFC3-UVIS F657N HLSP drz，单 HDU，8400×8000 float32，`BUNIT=ELECTRONS/S`，
  `EXPTIME=9600`，`PHOTFLAM=2.2290223e-18`，`PHOTPLAM=6566.60545`，`PHOTBW=41.015`
- 用途：提供**真实的大尺度天光/星云结构**作为仿真背景；仅取 24×24 分块平均后的
  ~0.95″/px 模板（与 testdata FLI/KAF-16803 系统同量级采样）。本单元**只读**，不修改

**D2 · Gaia DR3 XP 光谱（真实天体 SED）**

- 来源：仓库内 `lib/infrastructure/gaia_xpsd_client`（本地 DR3SP 库）+ 仓库内 `gaia_client.c`
  （`code/gaia_xp_dump.c` 直接链接它，只读）
- 查询：M16 场 `(274.7216, −13.8415)` r=0.075°，G<21.5 → 208 源；
  真实帧场 `(83.2833557851, −6.37428025059)` r=0.30°，G<18.0
- XP 采样：343 点、336–1020 nm @ 2 nm，uint8 + `flux_min`/`flux_mul`；
  `F(λ) = byte·flux_mul + flux_min` [W m⁻² nm⁻¹]
- 用途：注入星场的**真实 SED 形状**（星等为已知真值，SED 形状取自真实天体）

**D3 · testdata 真实帧（底参照）**

- 路径：`testdata/` 内 FLI 相机帧（M42 M1 T2 Red 300 s 等），uint16 + `BZERO=32768`
- 用途：真实数据底参照——帧内仪器参数、WCS 二轮精化、引导 vs 盲检、单帧可自算预算。
  本单元**只读**，不修改

**D4 · 外部通带曲线（唯一需要网络）**

- 来源：SVO Filter Profile Service，`HST/WFC3_UVIS2.{F657N,F673N,F502N}` 总系统透过率
- 复现：`bash 实验/photometric-magnitude/code/step0_fetch_refs.sh`（脚本内钉死 SHA256，不匹配即失败）

| 曲线 | SHA256 |
|---|---|
| `svo_HST_WFC3_UVIS2_F657N.txt` | `2af43d2dec10904ca2a3207cbc74a9ca1f83fe02d35b7bfd97832d032ad745cf` |
| `svo_HST_WFC3_UVIS2_F673N.txt` | `fdb18eb39936094323b90e20f06cc88c88412ce9a989c43f22e13cf8fdfa598a` |
| `svo_HST_WFC3_UVIS2_F502N.txt` | `587ef650b0660cb060af58b0267768ecb05d06ef06f17f9d7d19aa8912ac5e3c` |

- **离线复现边界（诚实说明）**：无网络时 step0 直接用缓存；缓存缺失且无网络则 step3 无法复跑。
  缓存不入库，因此首次复跑需要一次网络访问

**D5 · 本单元生成的仿真帧（非受控数据）**

| 文件 | 内容 | 生成命令 |
|---|---|---|
| `frame_A.npz` | HST 模板 + Gaia XP 注入，透明度 1.0 | `code/step2_hst_sim.py` |
| `frame_B.npz` | 同上，透明度 0.62 | 同上 |
| `frame_C.npz` | 同模板 + 500 星解析合成位置场 | 同上 |

每个 npz 内含：`img`（ADU 图像）、`m`（真实空间增益图）、`sky_map_adu`、
`mu`（期望电子数图，供精确方差用）、真值 `x/y/ra/dec/mag_gaia/mag_eff`、
模型与注入两侧的合成通量 `f_syn`/`f_syn_inject`、`inject_scale`。

外部绝对刻度交叉核对：SVO HST/WFC3_UVIS2 F657N/F673N/F502N 总系统透过率曲线 + 头部 PHOTFLAM/PHOTPLAM。
不删改 `testdata/` 与 `testdata/HST_M16/` 内任何文件。

**三类数据的分工与互证**：① 提供真实的天光/星云结构与完整噪声物理（检验"真实条件下的判据行为"）；
② 提供精确 Oracle（检验实现正确性、积分约定、IRLS 稳健性）；③ 提供真实探测器数据
（检验帧内参数、WCS 二轮精化、引导检测在真实数据上的可用性）。
**三者的结论并不一致（订正 P1-B01）**：① ② 在预算完整、判据非退化下自洽可复现（仿真帧 A/B/C 全 PASS，解析 Oracle 机器精度一致）；
③ **真实帧腿判红**——σ_flat 换成独立项后 σ_obs = 0.026520 mag > σ_ceiling = 0.020561 mag ⇒ **ABOVE_CEILING**（见 §4.7）。
按审查标准（`run/FINAL-07/pkg/standards/01_科研对抗性审查标准.md` §6）「三类结论一致才判定创新点成立」，本单元因此记 **不成立（待修）**。
共同成立的部分只有「标定系数无绝对窗口、不可反解仪器参数」这一条（退化族 + 平移不变量）。

---

## 4. 结果（真实数字 + 不确定度 + 对照）

### 4.1 纯解析合成：实现正确性与约定收敛（`results/step1_analytic.json`）

| 检验 | 实测 | 结论 |
|---|---|---|
| Oracle 零点（`m≡1`） | `location = 16.336461198922965`，真值同值，`rtol = 0.0`，`k_rel_err = 3.33e-15` | 估计器与真值一致到机器精度 |
| 冻结约定 vs 独立稠密线性+梯形 Oracle | `oversample` 4→256：中位相对差 1.64e-4 → 2.04e-4，最大 5.07e-4（≈2.2e-4 mag） | 积分约定差异 ≪ 0.01 mag 预算 |
| 仅求积差（同一被积函数） | 中位 8.49e-5，最大 2.89e-4 | Simpson vs 梯形一致 |
| 空间增益恢复 | 无 `m` 模型 `sigma_obs = 0.03720` → 拟合 `m`(deg2) 后 **`0.000365`**；`Δlocation = −0.001697` dex | 低阶空间增益可被逐帧拟合吸收 |
| 20% 离群稳健性 | 离群率 13.7%，`|Δlocation| = 0.000924` dex（< 0.1） | Tukey IRLS 稳健 |
| `S=0` 退化 | 取 median 不迭代，`location = 0.6989700043`（期望同值） | 退化分支正确 |
| 噪声标定（天光 ×0.25/1/4/16） | `sigma_obs` = 0.013352 / 0.013603 / 0.019149 / 0.034587，全部 PASS，`ratio_to_floor` 1.38/1.24/1.32/1.50 | 度量随噪声单调、判据不恒真 |

**"自由背景简并"的代价**（`sigma_fit_composition_form`）：

| 天光倍数 | 无自由背景（Horne 解析式）σ [mag] | 含自由常数背景（精确 Fisher）σ [mag] | 比值 |
|---|---|---|---|
| 0.25 | 0.006152 | 0.013218 | **2.149** |
| 1 | 0.008666 | 0.015004 | 1.731 |
| 4 | 0.014970 | 0.019987 | 1.335 |
| 16 | 0.028638 | 0.032416 | 1.132 |

`run/RELEASE-02/parallel/06.md` §2.2（第 100 行）给的是 **Horne 1986 最优提取结构**
`σ_F/F = sqrt(1/(g·F) + N_eff·σ_pix²/F²)`，它假定**背景已知、无自由背景参数**；
本实验的 `psf_fit_variance_exact` 多了一个自由常数背景参数（J=[P,1]），因此多出 H01 交叉项。
**这是口径差异，不是文档错误**——06.md 的 `σ_fit = 0.0140` 是生产拟合器 200 次重复拟合的
**实测值**，不是该解析式的取值。轮次 1 审稿指出作者曾把这一条写成"文档订正 C1"，属**稻草人**，
已从 `DOC_CORRECTIONS.md` **撤回**（见该文"已撤回"节与 `REVIEW.md` R5）。

### 4.2 HST 物理前向仿真：逐帧标定与双边界判据（`results/step5_calibration_gate.json`）

| 帧 | 说明 | n | `sigma_obs` [mag] | `sigma_floor` | `sigma_ceiling` | 判定 | obs/pred | `k` 相对误差（有效真值） |
|---|---|---|---|---|---|---|---|---|
| A | 透明 1.0，Gaia XP 注入 | 48 | **0.04534** | 0.00966 | 0.04847 | **PASS** | **1.154** | **2.09%** |
| B | 透明 0.62 | 54 | **0.05746** | 0.01252 | 0.07010 | **PASS** | 1.008 | **1.85%** |
| C | 扩展星场 500 星 | 96（预算样本；候选 105） | **0.05172** | 0.01457 | 0.14815 | **PASS** | 0.417 | **3.95%** |

> **订正 P1-B01 后的读数**：σ_flat 由「`delta_after_m` 自指值」换成「真值平场散度经 `N_eff` 折算」的独立项后，
> 三帧 σ_flat 均为 **0.000881 mag**（真值逐像素平场散度 0.0032 / `N_eff` 15.566 ⇒ `(2.5/ln10)·0.0032/√15.566`），
> σ_ceiling 由 0.05683 / 0.10301 / 0.15990 收紧为 **0.04847 / 0.07010 / 0.14815**，三帧仍全部 PASS。
> 帧 C 的 n 由 105 变为 96、`k` 相对误差与 obs/pred 随之变化，来自本轮的完整重跑（`run_all.sh`），非统计口径改动。

逐项预算（帧 A 实测，全部由本帧推导）：
`σ_pix = 18.40 e-`（= √(RN² + 天光电子 + 暗流)）、结构因子 `1.000`、
`σ_psfsys`（帧内小孔径 r=4 px）= `0.0250` mag（真值口径 0.0256 mag）、
`σ_color = 0.00574` mag（真值重算）、
`σ_flat = 0.000881` mag（**订正 P1-B01**：由**真值**逐像素平场散度 0.0032 经 `N_eff = 15.566` 折算的独立项；
历史用的 `delta_after_m` 自指值 0.01973 mag **已降级为诊断字段** `delta_after_m_diagnostic`，
与「`m̂/m_true` 之比」口径不同——后者口径错误且曾多乘 2.5，见 §9）、
`σ_gaia = 0.002` mag（注入）。

**帧间独立**：`k_B/k_A = 1.60986`，透明度比倒数 `1/0.62 = 1.61290` ⇒ 比值/期望 = **0.9981**；
`σ_obs` 比 = 1.267；`cross_frame_gate_present = false`（跨帧 k 门禁不存在，也不应存在）。
**残差分布一致性**（轮次 1 审稿指出此处曾是死代码，已补做）：两帧 IRLS inlier 的星等残差
KS 检验 `D = 0.1227`、`p = 0.787`（n_A=48, n_B=54）⇒ 分布一致，**这才是"帧间独立"的正面证据**，
而非只看 σ_obs 比。

### 4.3 星表引导检测 vs 盲检测（`results/step4_guided_vs_blind.json`）

| 指标 | 帧 A | 帧 B |
|---|---|---|
| 引导匹配率（真值 1 px 内） | **0.9859**（70/71） | **0.9859** |
| 盲检匹配率（同口径） | 0.2254 | 0.1972 |
| 匹配率提升 | **+0.7606** | **+0.7887** |
| 盲检检出数 / 其中匹配真值 / 虚警 | 19 / 16 / 3 | 19 / 14 / 5 |
| 引导候选数 / 拟合失败 / 虚警 | 71 / 0 / **0** | 71 / 0 / **0** |
| 每匹配星成本 | 引导 `0.00718 s` vs 盲检 `0.00792 s` | 引导 `0.00398 s` vs 盲检 `0.00993 s` |
| 定位输入 | **注入真值本身**（注入与定位共用同一 WCS 正/反变换） | 同 |
| 粗 WCS 残余偏移 0/1/2 px | 匹配率 0.9859（不变） | 同 |
| 粗 WCS 残余偏移 **3 px** | **0.0141**（崩溃） | **0.0141** |

盲检阈值扫描（k=5→1.5）无法把匹配率提到引导水平：帧 A 最高 0.239（k=1.5–4），虚警 3–10；
帧 B 最高 0.197，虚警 2–7。

**真实 M42 帧上的算力对照**（`step8_real_frame.py`，4096²，Gaia 锥 r=0.3°、G<18）：
盲检 **13163** 个检出中仅 **1.49%** 对应星表星；引导只对 **434** 个星表候选做拟合
（其中 **45.2%** 被盲检独立确认）。若对每个盲检源都做 PSF 拟合，需要 **30.3×** 的拟合次数。

**诚实说明**：
① 仿真帧较小且被星云结构主导，盲检的绝对拟合次数（19）反而少于引导（71），
所以"算力节省"只能按**每匹配星**、**同等完备度**或**真实密集帧的拟合次数**口径陈述，
不能按仿真帧的绝对墙钟时间陈述；
② **循环性**（轮次 1 审稿 R16 指出）：仿真帧上引导路径的定位输入就是**注入真值位置**
（两者共用同一 WCS 的正/反变换），所以 0.9859 度量的是**测光流水线的完备度**，
不是独立的定位精度检验。非循环的真实检验来自 §4.7 的真实帧（WCS 与 Gaia 相互独立，
827/827 匹配）。"3 px 崩到 0.0141" 恰好等于 `fit_psf` 的 ±2 px 位置搜索边界 ⇒
"必须 ≲2 px" 是**由实现边界决定**的，不是独立的物理发现，已在 §6 登记。

### 4.4 apply photometry 与物理单位消除（`results/step6_apply_and_units.json`）

- `I_photo = k_photo·m(x,y)·I_cal`：与**独立复算**逐像素最大相对差 = **0.0**；
- 下游消费：drizzle 式重采样尺度比 `2.060596e-17` vs 期望 `2.060581e-17`（相对差 7e-6）；
  星点总通量比 = `2.053358e-17` = `k_photo·m̂`；
- 未启用路径：`degraded_reason = "apply_photometry_disabled"`，且 **fail-closed** 抛错；
- **星等域残差（改正方向自检）**：不改正 `MAD = 0.04534` mag，按交付实现改正后
  `MAD = 0.01973` mag（中位 `0.00511`）⇒ `m_correction_improves = true`。
  轮次 1 审稿（R9）发现早期版本的 `m̂` **方向反了**（把增益本身当修正场，残差反被放大到 0.0738），
  已修：`m(x,y) = 10^{+0.4·poly(x,y)} = 1/m_gain`，并加方向自检字段防回归；
- **反推不可辨识**：`(A,t,g)` 退化族（`A·t/g` 相同）⇒ 中位通量 4831.2 / 4865.1 / 4840.2 ADU、
  `σ_obs` 0.01891 / 0.01812 / 0.01344 mag（前两组统计不可分）；`(A,t)` 为 1 参数退化，
  加 PTC 可得 `g` 但 `(A,t)` 仍不可分 ⇒ **欠定 2 参数族**；
- **零点平移不变量**：`F_instr × 7.3` ⇒ `Δlocation = 0.8633228601`（期望 `0.8633228601`）、
  `k` 比 = `0.1369863014`（期望同值）、`Δσ_residual = 0`。

### 4.5 XP 合成通量 vs HST PHOTFLAM 绝对定标对拍（`results/step3_forward_vs_photflam.json`）

**透过率曲线独立校验**：由 PHOTFLAM 反推有效面积 `A = hc/(PHOTFLAM·∫λT dλ)`：

| 滤镜 | 曲线 SHA256（前 12） | 枢轴波长（曲线 vs 头部） | 相对差 | 反推有效面积 | 几何面积 | 比值 |
|---|---|---|---|---|---|---|
| F657N | `2af43d2dec10` | 6566.581 vs 6566.605 Å | 4e-6 | 44425 cm² | 38453 cm² | 1.155 |
| F673N | `fdb18eb39936` | 6765.966 vs 6765.916 Å | 7e-6 | 45323 cm² | 38453 cm² | 1.179 |
| F502N | `587ef650b066` | — | — | — | — | — |

**WCS 二轮精化**（HLSP drz 头部 WCS 与 Gaia DR3 的系统偏移）：F657N `(23.10, 33.32)` px = **1.622″**，
精化后 83/88 在 4 px 内匹配；F673N `(23.12, 33.52)` px = **1.629″**，80/88 匹配。

**残差与色项**（`Δmag = −2.5·log10(CR_pred/CR_meas)`）：

| 滤镜 | n | 中位 Δmag | bootstrap σ | MAD | 色项斜率 [mag/mag] | 干净子样（G<17 ∧ 低背景） |
|---|---|---|---|---|---|---|
| F657N | 42 | −0.147 | 0.113 | 0.491 | 0.464 | n=12，中位 −0.014，MAD 0.255 |
| F673N | 29 | −0.263 | 0.130 | 0.645 | 0.765 | n=12，中位 −0.224，MAD 0.104 |
| F502N | 75 | −0.080 | 0.032 | 0.213 | 0.565 | n=18，中位 −0.007，MAD 0.049 |

同星跨滤镜（n=19）残差之差：中位 **0.117** mag、MAD 0.326 mag。

### 4.6 非退化负例（`results/step7_negatives.json`，**7/7 通过**；订正 P1-M07 后重跑）

> **轮次 1 审稿（R3/R4/R13）后重做**：原 N2 的通过判据与 `ACCEPTANCE_SPEC` §2.1
> "注入天光梯度时度量如实变大"**方向相反**，且注入形态是"算术相加的确定性图样"（§12.2 禁止）；
> 原 N4 是 `kB := kA/0.62` 的算术恒等式；原 N5 与原 N0 是同一条恒等式。已全部重做：

| # | 类型 | 负例 | 实测 | 判定 |
|---|---|---|---|---|
| N0 | 退化输入检查 | 真值无效应（`F_instr = inject_scale·F_syn` 精确） | `σ_obs = 0.0` | **BELOW_FLOOR（判红）** ✔ |
| N1 | **注入-响应** | 乘性平场残差，幅度 0/0.01/0.02/0.04/0.08/0.16 | `σ_obs` = 0.04534 / 0.04719 / 0.04798 / **0.07618** / **0.09879** / **0.15959**（单调，**3.52×**）；**≥0.04 判红** | ✔ |
| N2 | **注入-响应** | **Poisson 物理口径**天光抬升 ×1/2/4 | `σ_obs` = 0.0252 / 0.0481 / **0.0734**（单调，**2.91×**）；**4× 判红** | ✔ |
| N3 | **注入-响应** | 预算漏掉颜色项（真实 XP 谱 × 两条 QE 曲线） | 真实颜色项仅 0.0057 mag ⇒ 不改变判定；**阈值 0.04 mag**（真实值的 **7.0×**）时漏项判 **ABOVE_CEILING** | ✔（机制成立，真实量级不成立） |
| N4 | 错误门禁反例 | 用 step5 **实测**的 `k_A`/`k_B` 检验"跨帧 k 一致性门" | `k_B/k_A = 1.60986` vs 期望 `1.61290`（偏差 **0.19%**）⇒ 该门会**稳定误杀正确帧** | ✔ |
| N5 | 退化输入检查 | 对帧 A **实测**通量按 `abs(r−median(r))<tol` 过裁剪 | `σ_obs` 0.0453→0.0380→0.0198→0.0036，`σ_floor` 下降更快、n=5 时**变负**；订正后 n ≤ 12 的行状态词 = **`LOWER_BOUND_UNDEFINED`（不记 PASS）** | **✔**（语义订正 P1-M07：不再有恒真下界） |
| N6 | **注入-响应（口径对照）** | 对帧 A 实测通量乘 `10^(0.4·amp·N(0,1))`（逐星独立、与空间平场无关），**同一批通量并行跑两种 σ_flat 口径** | 自指口径下 `σ_ceiling` 随观测同步膨胀（0.0562→0.1605）⇒ **恒 PASS、无判别力**；独立口径下同一注入在 **amp = 0.05 mag 判红** | ✔（P1-B01 的反例化） |

**汇总**：`7/7` 条通过；其中**注入-响应型 4/4 全部通过**（N1/N2/N3/N6）。
N0/N4/N5 是退化输入与错误门禁反例。
**N5 的历史结论已订正（P1-M07）**：下界 `1−3·1.166/√n` 在 `n ≤ 12.236` 时非正 ⇒ 3σ 下包络**不存在**；
历史按 `max(rho_lo, 0)` 夹逼使其恒不触发（**恒真门**），现已撤回，改为「作用域 `upper_only` + 状态词 `LOWER_BOUND_UNDEFINED`、不记 PASS」。

**另记（能力边界，不计入负例）**：对帧**算术相加**确定性天光梯度（无散粒噪声，§12.2 禁止的形态），
`gx = 0 → 0.4` ADU/px 时 `σ_obs` 只从 0.04534 变到 0.04897（**1.08×**）⇒ 判据对确定性加性图样
不敏感（局部背景吸收），对散粒噪声敏感。见 `DOC_CORRECTIONS.md` C3。

### 4.7 testdata 真实帧（底参照）（`results/step8_real_frame.json`）

主帧：`testdata/M42_T2T3_mosaic_Flying_dutchman/T2/M1/M42_M1_T2_flying_dutchman-20251212@012404-300S-Red.fts`
（FLI，4096²，`EXPTIME=300 s`，`BZERO=32768`，`FILTER=Red`，`CCD-TEMP=−20 °C`）。

| 项 | 实测 |
|---|---|
| 天光中位 / 逐像素背景 RMS | **1202 ADU** / **13.63 ADU**（相邻差稳健尺度） |
| 饱和像元数（≥65535 ADU） | 1248 |
| Gaia DR3 XP 锥（r=0.3°, G<18） | 827 源 |
| WCS 二轮精化 | shift `(−0.263, −0.319)` px = **0.0068″**，**827/827** 在 4 px 内匹配 |
| 引导测光 | 434 候选，全部拟合成功，`2.093 s`（`0.004823 s/星`） |
| 盲检测 | 13163 检出，`1.751 s`；其中 196 个与引导路径吻合（**1.49%** 精度） |
| 单帧可自算预算判据 | n=157、inlier 151，`σ_obs = **0.026520** > σ_ceiling = **0.020561** ⇒ **ABOVE_CEILING（判红）**；`σ_floor = 0.006008` |
| 可自算预算项 | `σ_pix = 40.94 e-`、结构因子 1.000、`σ_psfsys`（帧内小孔径）`0.01367` mag、`σ_flat = **0.0007** mag（仓内约定常数，活出处 `code/scia_calib.py:31`，一手出处未取得，见 §6 第 16 条；**独立于被测样本**） |
| **自指项已降级** | `delta_after_m = 0.019561` mag（低阶 `m(x,y)` 拟合后的残余散度，**σ_obs 自身的函数**）只作诊断字段 `delta_after_m_diagnostic`，**不进预算**；历史把它当 σ_flat 使上界方差 47.6% 来自被测统计量 |
| **翻转临界** | 使该帧由红翻绿的 `σ_flat = 0.013057` mag = 该约定值的 **18.7 倍**（即只有把平场项放宽近 19 倍才能声称「落在预算内」） |
| **不可自算项（如实标 `null`）** | `σ_color`、`σ_gaia`（真实帧无独立通带/参考侧真值，不用 0 冒充）⇒ **上界不完整** |

**与 `docs/detail/registry/astrocs.phase1.photometry.md`「测光一致性判据（单帧、尺度无关、双边界）」 实测对照的关系（订正 P1-B01 后改写）**：该文记录 L4 49 帧真实数据 **PASS 1/49**，
未消系统项 0.0442 mag。本实验在**预算完整**的仿真帧上 3/3 PASS（obs/pred 1.15/1.01/0.42）；
真实 M42 帧在 σ_flat 换成独立项后同样**判红**。
⇒ 本单元**不再声称**「真实帧残差落在本帧预算内」；真实帧判红与 48/49 判红**同归因**：
未建模系统项（拥挤场 σ_psfsys、真实通带 σ_color、差分消光）确实超预算，
**不是判据本身不可用**——本实验的 C2/C3/C5 正好量化了这三项的量级。

**对照结论**：真实帧的 WCS 本身已准确（残余 0.007″，与 HLSP drz 的 1.6″ 形成对照），
说明"二轮精化"的必要性取决于上游 WCS 质量，而非流程固定开销。

---

## 5. 可判定的结论

1. **H1 在仿真腿与解析腿成立，在真实帧腿不成立（订正 P1-B01）**：单帧自算误差预算的双边界判据可以对标定结果给出 PASS/判红，
   三个独立仿真帧（含真实 HST 结构）全 PASS，`obs/predicted` = **1.154 / 1.008 / 0.417**（帧 A/B/C）。
   **真实 testdata 帧判红**：σ_obs = 0.026520 > σ_ceiling = 0.020561 ⇒ **ABOVE_CEILING**。
   **诚实说明 ①**：帧 C 的 0.417 偏离 1 达 2.4×——原因是拥挤场里 `σ_psfsys` 帧内估计偏高（0.107 vs 真值
   0.039 mag，2.7×），使 `σ_ceiling` 偏松。这是**保守方向**（偏松不偏紧），
   但说明拥挤场的上界判别力下降。
   **诚实说明 ②**：真实帧的判红**推翻了本单元原先的结论**——原结论建立在 σ_flat 自指之上（见 §4.7）。
   按审查标准 §6「三类一致才判定成立」，H1 的**创新点成立性判定为不成立（待修）**。
   **判据的已知弱点（轮次 1 审稿 N5 发现，已按 P1-M07 订正）**：下界 `1−3·1.166/√n` 在 `n ≤ 12.236` 时非正
   ⇒ 3σ 下包络不存在；历史用 `max(rho_lo,0)` 夹逼使它恒不触发（恒真门），现改为 `upper_only` +
   `LOWER_BOUND_UNDEFINED`（不记 PASS）⇒ 低样本域的判别力只来自**上界与状态词**。
2. **标定系数无绝对窗口、不可反解仪器参数**：退化族与零点平移不变量给出机器精度级的证据。
3. **H2 成立**：引导检测匹配率 0.9859 vs 盲检 0.2254/0.1972（提升 +0.76/+0.79）；
   真实 4096² 帧上盲检 13163 个检出只有 1.49% 对应星表星，逐源拟合需 30.3× 的拟合次数；
   且**必须**先做二轮 WCS 平移精化（残余 ≤2 px），否则匹配率从 0.9859 崩到 0.0141。
4. **XP 绝对刻度**：窄带上与 HST PHOTFLAM 的中位差 −0.08 ~ −0.26 mag、逐星 MAD 0.21–0.65 mag，
   色项斜率 0.46–0.77 mag/mag。**逐星散度**（全部样本 MAD）0.21–0.65 mag；**干净子样**
   （G<17 ∧ 低背景）MAD 0.049–0.255 mag ⇒ 窄带 XP 合成通量的绝对刻度不确定度是
   **0.05–0.65 mag 量级，取决于样本的星云污染与星等范围**。
5. **判据的能力边界**：`σ_obs` 对**乘性**空间残差（N1，3.52×）与**散粒噪声**（N2，2.91×）敏感、
   对**算术相加的确定性加性图样**不敏感（1.08×）⇒ 它不能认证天光**扣除**质量；
   且 `n ≤ 12.236` 时下包络不存在（作用域降级 `upper_only`）⇒ 低样本域**只有上界与状态词可用**。
6. **与 `docs/detail/registry/astrocs.phase1.photometry.md`「测光一致性判据（单帧、尺度无关、双边界）」 实测对照**：真实 L4 数据 PASS 1/49；本实验在预算完整的仿真帧上 3/3 PASS、
   在真实 M42 帧上**判红** ⇒ **两侧一致**：真实数据的判红归因到**未建模系统项**（拥挤场 σ_psfsys、
   真实通带 σ_color、差分消光），而不是判据不可用；本实验的 C2/C3/C5 量化了这三项的量级。

---

## 6. 诚实边界（证据不足 / 未建立的部分）

1. **"全帧消除物理单位"未建立**：本实验证明的是"**标定系数无绝对窗口且不可反解仪器参数**"，
   即在**星等坐标系内**自洽。它**没有**证明"物理单位在全帧被消除"这一更强命题——
   要证明后者需要 `(A, t, g)` 独立可观测，而本实验恰恰证明它们不可分。**该强命题不予支持。**
2. **适用域**：结论只覆盖 `m(x,y)` 为**低阶（deg≤2）乘性**空间响应、星等窗 13–20、
   `G` 波段参考侧、`FWHM ≈ 2` px 的采样条件。高阶/小尺度平场结构、极窄带未覆盖；
   **饱和与非线性区的缺口见第 16 条**（本单元只拦饱和，不拦非线性）。
3. **加性天光**：判据对其不敏感（C3），本实验未提供天光扣除质量的门禁证据。
4. **宽带 XP 误差未测**：4.5 的结论只对**窄带**（等效宽度 29–39 nm）成立；宽带（Baader R，~144 nm）
   的同类误差未直接测量，不可外推。
5. **真实帧无真值（订正 P1-B01）**：`σ_color`/`σ_gaia` 在真实帧上不可自算，如实标 `null`；
   `σ_flat` **不再**用 `delta_after_m` 自指值，改取**仓内约定常数** `σ_flat,hf = 0.0007 mag`
   （活出处 `code/scia_calib.py:31`；一手出处未取得，见第 16 条），
   该常数只作诊断/定标用途，**不得**再作精度锚。**残余不确定度**：该常数向本单元逐星预算的**折算链未独立核证**；
   但取仿真口径（0.000881 mag）判定同样为红，结论对该折算不敏感。
6. **网络边界**：`step0_fetch_refs.sh` 是唯一需要网络的步骤；缓存不入库，
   首次复跑需要一次网络访问，之后可用 SHA256 固定的缓存离线复跑。
7. **对照实现的独立性**：盲检测为自实现的纯 numpy 对照口径（`scia_common.blind_detect`），
   **不是**生产 `sdet_api`；开源对照（photutils 3.0.0 `DAOStarFinder` 的 `xycoords` 语义）
   只作为"引导可跳过源查找"的一手语义证据，未在本环境安装运行。
8. **A14（零点不可辨识）无一手文献**：文献查证（`refs.md`，**订正 P1-m10**：原指向 `run/SCI-401/lit/verified_refs.md`，该路径在 `run/` 回收后已不存在；核验记录现已随单元落到 `refs.md`）未找到
   第一方文献，本实验**自行推导 + 反例实验**证明，**未引用任何文献**支持该结论。
9. **引导检测匹配率的循环性**（轮次 1 审稿 R16）：仿真帧上引导路径的定位输入就是**注入真值位置**
   （共用同一 WCS 正/反变换），因此 0.9859 只度量**测光流水线完备度**，不是独立定位精度检验；
   非循环检验来自真实帧（827/827）。"3 px 崩到 0.0141" 恰好等于 `fit_psf` 的 ±2 px 位置搜索边界
   ⇒ "必须 ≲2 px" 是**实现边界**决定的，不是独立物理发现。
10. **σ_psfsys 的孔径口径是作者选定的**（轮次 1 审稿 R17）：生产文档用 r=10 px，本实验改用 r=4 px
   并论证其更准（§4.2/C2）。这属于**口径选择**，虽有两口径与真值的三方对照支撑，但仍应视为
   本实验的约定而非生产定义。
11. **有效面积 / 几何面积 = 1.155 / 1.179 的解释**（审稿 R20）：几何面积按 `π·120²·0.85` 估算，
   未含 OTA 遮挡与 UVIS 芯片间缝，因此"有效面积 > 几何估算"属预期；但本实验**没有**独立的
   有效面积标定，该比值只作为"透过率曲线绝对刻度合理"的旁证，不作为定量结论。
12. **N5 暴露的判据弱点已订正**（见 §4.6/§5 与 `DOC_CORRECTIONS.md` C1）：下界在 `n ≤ 12.236` 时不存在，
    现以 `LOWER_BOUND_UNDEFINED` 显式降级（不记 PASS），**撤回**历史建议的 `max(rho_lo,0)` 夹逼。
13. **文献腿的二手归属（P1 审查 §1）**：Beaton & Tukey 1974（V3，无 OA 全文）与 Aitken 1935（V11）
    只能作**二手归属**，不得声称已核原文；V4 期号、V8 题名/DOI、V10 适用域（两级域）、V2 路径、Akima 页域已按 P1-m06 订正。
14. **本单元 σ_flat 口径与上游文档的关系**：判据形态与预算项逐字取自 `docs/detail/registry/astrocs.phase1.photometry.md`「测光一致性判据（单帧、尺度无关、双边界）」；
    本单元只订正了实现侧的自指错误，**未改**上游文档的常数与容差（未越权）。
15. **CI 登记缺口（需负责人处理，本轮不自处理）**：`flock /tmp/astrocs_ci.lock python3 eng/ci/run_checks.py --all --profile fast`
   实测 `pass=87 fail=12`，其中 `ENG-CONSTRAINTS` 报
   `顶层条目 实验 不在 §7 白名单且未登记`。但 `ENGINEERING_SPEC.md` §7 的根条目代码块**确实列有**
   `实验/（科学实验单元：SCI-A/B/C 等，随仓库维护）`（第 113 行，位于 §7 同一个代码块的
   "其他固定目录"段），AGENTS.md §7 同样列出；而检查器 `eng/tools/quality/check_root_cleanliness.py`
   只解析该代码块开头的"仓库根固定条目"清单（第 98–101 行）与 `eng/ci/root_manifest.json` 的
   `allowed_dirs`，两者都不含 `实验` ⇒ 规范已认可、机器白名单未同步。
   本实验单元按任务书与 AGENTS.md §7
   落位 `实验/photometric-magnitude/`，**未越权修改根级登记文件**（`eng/ci/root_manifest.json` / `ENGINEERING_SPEC.md`），
   该缺口登记为需负责人处理项。其余 11 项失败与本实验单元无关（`实验/` 未被任何检查项引用，
   `grep -c 实验 run/SCI-401/logs/ci_fast.log = 0`），来自并发的 DOC/SCI-B 改动与既有仓库状态。
16. **探测器线性适用域未测（乘性假设的前提缺失）**：`I_photo = k_photo·m(x,y)·I_cal` 的乘性假设要求探测器工作在线性区，这是 `r_i` 取对数变成加性零点的**唯一前提**。代码里只有饱和硬门（`saturation_adu = 65535`，`code/scia_sim.py:75`、`code/step5_calibration_gate.py:63`、`code/step7_negatives.py:48`、`code/step8_real_frame.py:112`），**不拦未饱和但已进入非线性区的像元**；全单元源码无「线性化 / 非线性 / 线性区」的任何量。**「线性化残差」整项不在 `sigma_ceiling` 的任何一项里**，进入非线性区的电子数阈值**未测**。实测：亮度依赖乘性残差 `a·(F/F_max)` 在 a = 0.02/0.05 时把 `sigma_obs` 由 0.00220 抬到 0.00874/0.01216（`code/redteam/rt_gate_attribution.py`）⇒ 在该阈值落地前，本单元**不宣称**真实帧判红的成因已被排除。
17. **σ_gaia 的仿真取值自参照且比正本口径乐观 5.4 倍**：仿真帧取 `σ_gaia = 0.002 mag`（`code/step5_calibration_gate.py:111`、`code/step7_negatives.py:69`），恰等于仿真注入的参考侧误差 `ref_err_mag = 0.002`（`code/scia_sim.py:93`）⇒ **预算项 = 被注入真值**。Gaia DR3 XP 绝对刻度上限 1% ⇒ `−2.5·log10(1.01) = 0.0109 mag`，是 0.002 的 **5.4 倍**；换成 0.0109 时帧 A 的 `sigma_ceiling` 由 0.048473 升到 0.049646（+2.4%，判定不翻转）⇒ 三帧 PASS 条件性成立。真实帧 `σ_gaia = null` 不可自算。
18. **σ_flat = 0.0007 mag 的一手出处未取得**：仓内唯一活着处是 `code/scia_calib.py:31` 的字面量 `SIGMA_FLAT_HF_CANONICAL`；被归给的 `docs/detail/registry/astrocs.phase1.photometry.md`「数值落地口径」（内的「测光一致性判据（单帧、尺度无关、双边界）」 与「`docs/science/PHOTOMETRY.md` §16.4 的同一行」在当前树**均不存在**（该节只有 `σ_flat,hf` 字样、无 0.0007）。该值**单独决定真实帧判红方向**（`sigma_ceiling = 0.020561` 由它定，翻转临界 0.013057 = 其 18.7 倍）⇒ 按 AGENTS §4，它在本单元内只能作**仓内约定常数**引用，**不得称权威常数**；其向逐星预算的折算链亦未独立核证（第 5 条）。
19. **双边界判据在低样本域与尺度估计上双重偏松**：`n ≤ 12.236` 时 `rho_lo ≤ 0` ⇒ 作用域降级为 `upper_only`（下界**不存在**，只报 `LOWER_BOUND_UNDEFINED`）；同时 `sigma_obs` 由 MAD 型尺度估计量给出，该估计在 n=3 处的**期望值只有 0.67σ**（缺损因子 1.49）⇒ 被测统计量本身也系统性偏低。降级条件**只按 n 判**。
20. **双边界门对未建模乘性残差「可见但成因失明」，且下包络在自称有效的样本域内可被推翻**：`sigma_obs_mag` 在 `m(x,y)` **之前**算（`code/scia_calib.py` 中 `sigma_obs_mag = 2.5·sigma_residual` 的赋值位于 `if m_degree > 0` 分支之前），`m(x,y)` 只改诊断字段 `delta_after_m`。实测（`code/redteam/rt_gate_attribution.py`，n = 48 ≫ 12.236，`sigma_floor = 0.01731`、`sigma_ceiling = 0.03097`）：**第 B 组全部 16 个算例的 `m_degree = 0` 与 `m_degree = 2` 读数逐位相同**；位置依赖残差下 `m(x,y)` 把同一批星残差散度由 `sigma_obs = 0.06892` 压到 `delta_after_m = 0.00819`（吸收 99%），而 `sigma_obs` 一动不动 ⇒ 门**无法**区分「平场残余 / PSF / 颜色 / 探测器非线性」中的任何一项；且**仅 2% 的位置依赖乘性残差**即把判定由 `BELOW_FLOOR` 翻成 `PASS`（`sigma_obs` 0.00220 → 0.02884），3% 翻成 `ABOVE_CEILING` ⇒ `LOWER_BOUND_UNDEFINED` 的降级条件对「样本量够但残差场被单一时变结构支配」不充分。**判读后果**：真实帧判红只能归到「未消系统项超预算」这一层，具体归因超出该门能给出的证据。

---

## 7. 复现命令

**公共前置步（先跑，再跑本单元）**：

```bash
cd <repo root>
bash 实验/shared/synthetic/run_selftests.sh     # 合成器物理链自检：4 组件，全绿约 2.5 min
```

`run_selftests.sh` 是**所有实验单元**的一键复现公共前置步：先证明合成器链本身能红能绿
（能证伪），再让单元去跑真实验。退出码 0 = 全绿；1 = 有组件判红（本单元读数不得作为证据）；
2 = 有组件缺失（自检面不完整）。

> **不得只调 `noise_selftest.py`**：它只验「独立重实现 vs 生产实现 vs 解析预测」三方对照，
> 验的是**方法**（分布与口径是否正确），对**生产实现本身**零判别力——生产实现坏掉而独立
> 重实现与解析预测同源一起坏时，它照样绿。**生产面的门禁责任在 `m16_scene --selftest` 与
> `m16_sampling --selftest`**（这两个直接打生产渲染面/采样面）。四个组件全跑，不要裁剪成
> 单组件。
>
> 本单元另有一处**本地**物理链自校验入口：`code/scia_sim.py` 的采样段与共享链
> `noise_model.expose()` 的逐条对照写在 `code/scia_sim.py` 的模块 docstring
> 「与共享物理链的关系」一节——那是**说明性对拍**，不是可执行门禁；可执行门禁在上面前置步里。

本单元：

```bash
cd <repo root>
bash 实验/photometric-magnitude/code/run_all.sh          # 全量；日志落 run/SCI-401/logs/
bash 实验/photometric-magnitude/code/run_all.sh quick    # 跳过 step6
bash 实验/photometric-magnitude/code/redo/run_all.sh     # 重做三路 seed 20260926
```

单步：

```bash
bash 实验/photometric-magnitude/code/step0_fetch_refs.sh            # 外部通带曲线（唯一需网络）
python3 实验/photometric-magnitude/code/step1_analytic.py           # 解析合成 / Oracle / 负例
python3 实验/photometric-magnitude/code/step2_hst_sim.py            # HST 模板 → 前向仿真帧 A/B/C
python3 实验/photometric-magnitude/code/step3_forward_vs_photflam.py
python3 实验/photometric-magnitude/code/step4_guided_vs_blind.py
python3 实验/photometric-magnitude/code/step5_calibration_gate.py
python3 实验/photometric-magnitude/code/step6_apply_and_units.py
python3 实验/photometric-magnitude/code/step7_negatives.py
python3 实验/photometric-magnitude/code/step8_real_frame.py
python3 实验/photometric-magnitude/code/step9_collect.py            # → results/GATES.md
```

---

## 8. 佐证来源

### 8.1 一手文献（经 `web_search`/`web_fetch` 核验，见 **`refs.md`**；订正 P1-m10：原指向的 `run/SCI-401/lit/verified_refs.md` 路径已不存在）

- Montegriffo et al. 2023, A&A **674, A3**, DOI [10.1051/0004-6361/202243880](https://doi.org/10.1051/0004-6361/202243880)（Gaia DR3 XP 连续谱）
- Riello et al. 2021, A&A **649, A3**, DOI [10.1051/0004-6361/202039587](https://doi.org/10.1051/0004-6361/202039587)（Gaia EDR3 测光）
- Gaia Collaboration 2021, A&A **649, A1**, DOI [10.1051/0004-6361/202039657](https://doi.org/10.1051/0004-6361/202039657), arXiv:2012.01533（EDR3 总览）
- Bohlin, Hubeny & Rauch 2020, AJ **160, 21**, DOI [10.3847/1538-3881/ab94b4](https://doi.org/10.3847/1538-3881/ab94b4), arXiv:2005.10945（HST 通量标准）
- Horne 1986, PASP 98, 609（PSF 测光的 Fisher 结构；本实验 §2.3 的精确方差形式来源）
- STScI WFC3 Data Handbook §9.1（PHOTFLAM/PHOTPLAM/PHOTBW 定义与 `ABMAG = STMAG − 5log(PHOTPLAM) + 18.692`）

> 注：`refs.md` 的「引文订正记录」节记录了 3 条必须的引文订正（Montegriffo = 674 A3 而非 A33；
> 10.1051/0004-6361/202039587 = Riello 2021 A&A 649 **A3**；Bohlin/Hubeny/Rauch 2020 = AJ **160, 21**）。
> 本 README 已按订正后的信息书写。该记录原在 `run/SCI-401/lit/verified_refs.md`（`run/` 为 gitignore 目录，已回收）——
> 按审查 P1-m10 要求，**证据已随单元入库**（`refs.md`）。

### 8.2 开源实现（项目 + 版本 + 文件:行）

- astropy `astropy/stats/funcs.py:945`：`mad_std` 返回 `MAD × 1.482602218505602`（注释 `1./scipy.stats.norm.ppf(0.75)`）
  ⇒ 与本实验 `1/0.6744897501960817` 同源。
  **版本说明**（审稿 R21）：文献记录核到的是 **8.0.1**（上游 main 线），而**本机实际运行环境是
  astropy 7.0.1**（`python3 -c "import astropy; print(astropy.__version__)"`），该行号属于 8.0.1；
  7.0.1 的对应实现语义相同但行号可能不同，本实验**未在 7.0.1 上核对该行号**。
- photutils 3.0.0 `detection/daofinder.py:26`（`class DAOStarFinder`）、`:210`（`__init__`）：
  文档逐字 "If `xycoords` are input, the algorithm will skip the source-finding step."
  ⇒ 星表引导可跳过源查找的一手语义证据
- photutils 3.0.0 `psf/photometry.py:217`（`PSFPhotometry`）、`aperture/photometry.py:30`（`aperture_photometry`）
- sep 1.4.1 `sep.pyx:387`（`cdef class Background`）
- GaiaXPy 2.1.4 `src/gaiaxpy/generator/generator.py:17`（`generate()`）、`sampled_spectrum.py:114`（`return coefficients @ design_matrix`）
- SExtractor 2.28.2 `src/analyse.c:310`（`obj->fluxerr = sigtv;`）、`:561`（`sqrt()`）
- SVO Filter Profile Service：`HST/WFC3_UVIS2.{F657N,F673N,F502N}` 总系统透过率（SHA256 见 §3.1 D4 与 §4.5）

### 8.3 独立审稿（非作者，两轮）

`results/REVIEW.md`（428 行）。**结论：不可接受（需修改后重审）**，共 22 条意见。
作者对全部意见逐条核实并修复，其中 **5 条为真实硬伤**（已在 §9 列明）。
审稿确认作者**正确**的部分：八要素齐备；§6 的自我限制到位且未过度保守；
禁用方法①–⑦除 N2 注入形态外均无违规；未发现"把失败说成环境问题"。

**轮次 2 定向复核**（追加于同一文件）：5 条硬伤在**代码与归档 JSON 层面已实质修复**，
未发现新的科学阻断项；但复核指出"README 文本与归档 JSON 数值同步"这一同源失效模式**未修干净**
（4 处过期数字 + 1 处标题矛盾 + 1 处孤儿表头 + step1 的稻草人引文"马甲"仍在）。
**上述 6 项已全部按归档 JSON 回填/删除**，并重新生成 `GATES.md`/`gates.json`。
最终判定：`8/9` 项 PASS（G6 = PARTIAL，因 N5 未通过）。

### 8.4 仓内实测

全部结果 JSON 在 `results/`；判据表 `results/GATES.md`；
文档订正建议 `results/DOC_CORRECTIONS.md`；独立审稿 `results/REVIEW.md`。

合成器物理链的可执行自校验（公共前置步，四个组件全绿）：
`bash 实验/shared/synthetic/run_selftests.sh` →
`m16_mask` / `m16_scene` / `m16_sampling` / `noise_selftest` 全 PASS，退出码 0。
本单元的采样段与共享链的逐条口径对照见 `code/scia_sim.py` 模块 docstring
「与共享物理链的关系」一节。

---

## 9. 独立审稿意见的处置（轮次 1，非作者）

审稿结论为 **不可接受（需修改后重审）**。作者逐条核实并修复，处置如下：

| 审稿号 | 严重度 | 问题 | 处置 | 证据位置 |
|---|---|---|---|---|
| R1 | 高 | 归档 `step8_real_frame.json` 与交付代码不自洽（负值图、n=0、36/827）——一个被 kill 的旧进程在 13:22 覆写了新结果 | **已修**：杀掉僵尸进程、重跑；现为 sky=1202、827/827、n=157 | `results/step8_real_frame.json` |
| R2 | 高 | README 写"盲检 2.00 s"，实为 890.7 s（同 R1 根因） | **已修**：现为 1.751 s（归档 JSON） | 同上 |
| R3 | 高 | N2 的通过判据与 `ACCEPTANCE_SPEC` §2.1 方向相反 | **已修**：N2 改为 **Poisson 物理口径**天光抬升，判据改为"必须响应"，2.91×、4× 判红 | §4.6、`step7_negatives.json` |
| R4 | 高 | N4 是算术恒等式；N0/N5 是同一恒等式 | **已修**：N4 改用 step5 **实测** k；N5 换成真实过裁剪实验（并因此**未通过**） | §4.6 |
| R5 | 高 | `DOC_CORRECTIONS` C1 是稻草人（该公式不存在于任何文档） | **已撤回** C1；作者亲自复核确认，改记为 C8"自由背景简并代价" | `DOC_CORRECTIONS.md` |
| R7 | 中高 | 预算上限把系统项二次计入 | **已修**：`sig_robust_mc` 只合成噪声；ceiling 收紧（帧 A 0.0743→0.0568） | `scia_calib.py` |
| R8 | 中高 | `delta_after_m` 多乘 2.5 | **已修**：0.049331 → **0.019732**（与审稿独立复算一致） | `scia_calib.py` |
| R9 | 高 | step6 的低阶空间增益改正**方向反了**（残差被放大 63%） | **已修**：`m(x,y)=10^{+0.4·poly}`；加方向自检字段；0.07378 → **0.01973**（不改正 0.04534） | §4.4、`step6_apply_and_units.py` |
| R10 | 中 | `run_all.sh quick` 在干净状态崩溃 | **已修**：step9 容忍缺失结果并标 `NOT_RUN` | `step9_collect.py` |
| R11 | 中 | "帧间残差分布一致"从未被检验（死代码） | **已修**：补做 KS 检验 `D=0.1227, p=0.787` | §4.2 |
| R13 | 中 | N2 的注入是"算术相加的确定性图样"（§12.2 禁止） | **已修**：改为 `resimulate()` 走 Poisson 前向；算术版本降为对照记录 | `step7_negatives.py` |
| R14 | 中 | 判据出处写成 `docs/detail/registry/astrocs.phase1.photometry.md`（原 §3.1 → ）`（不存在） | **已修**：改为 **§4.1**，并注明 `run/` 引用追溯性弱 | §2.3、文首 |
| R15 | 中 | step0 只打印 SHA256 不校验 | **已修**：脚本内钉死 SHA256，不匹配直接失败 | `step0_fetch_refs.sh` |
| R16 | 中 | 引导匹配率的定位输入就是注入真值（循环） | **已登记**为诚实边界（§6.9），并指出非循环检验来自真实帧 | §4.3、§6.9 |
| R17 | 中 | σ_psfsys 孔径口径由 r=10 改为 r=4 | **已登记**为作者约定（§6.10），保留三方对照 | §6.10 |
| R18 | 中 | step5 JSON 含裸 NaN（jq 拒绝） | **已修**：`jdump` 统一把非有限值转 `null`，`allow_nan=False` | `scia_common.py` |
| R20 | 中 | 有效面积/几何面积 1.155/1.179 无解释 | **已登记**并给出预期解释（§6.11） | §6.11、`DOC_CORRECTIONS.md` C5 |
| R21 | 中 | 引 astropy 8.0.1 但环境是 7.0.1 | **已修**：明确区分文献版本与本机运行版本 | §8.2 |

**未修复/保留分歧**：
- **R6（审稿判"夸大"）已修**：§2.3 与 G1b 的"逐项由本帧推导"改为**限定表述**——
  仿真帧上全部预算项由本帧推导（含真值重算的 σ_color/σ_flat）；**真实帧上 σ_color/σ_gaia 不可自算**，
  如实标 `null` 且上界不完整（见 §4.7）。
- **R19**（硬编码常量/死代码）中不影响结论的部分保留：实验脚本的默认预算项取值仍为显式常量，
  已在 `gate()` 的签名与注释中标明来源。
- **R12/R22** 为纯风格项或审稿确认作者正确。
**遗留需负责人处理**：`实验/` 根条目在 `eng/ci/root_manifest.json` 未登记（见 §6.13）。
