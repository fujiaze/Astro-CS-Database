> **DOC-001 溯源注记（2026-09-16）**：本文为 V6 产品族冻结/设计档案（上一轮治理产物），因仍被活动合同引用而保留在活动索引；文中 工程控制/旧 V6 控制包（ROOT-007 已删除）/** 等旧控制包路径为该轮任务溯源，该控制包已由 ROOT-007 删除，不作现状引用。

# 观测模型与 covariance 复核（SCI-OBS-001）

- 任务：工程控制/旧 V6 控制包（ROOT-007 已删除）/tasks/SCI-OBS-001.md
- 文档 ID：OBS-REVIEW-001
- 任务基线：HEAD = main = origin/main = 4b508f28bbcada66c417a8a9324aca7eb92869ff（开工时本机只读复核，见 run/v6/sci-obs/logs/rc_summary.txt）
- 报告时 HEAD：192fab3546e710163e5dc276a9cd76309d650ff2 —— 执行期间控制器集成了并行任务 SCI-P2-001（提交信息 docs(v6): SCI-P2-001 Phase2 三模式集成复核通过）。4b508f28..192fab35 的 diff 仅含 docs/science/v6/phase2/** 与 TASK_LEDGER.csv，**不触及本复核引用的任何 SCI/合同/源码文件**；本任务已在 192fab35 重跑结构锚点（16/16，rc=0）与独立 Oracle（15/15，rc=0），结论不变（见 run/v6/sci-obs/evidence.json）。
- 性质：**只读科学复核 + 独立 Oracle + 负向 mutation**。未改任何上位规范，未改源码，未 commit/push，未建分支/worktree，未派生 subagent。
- 机器证据：run/v6/sci-obs/tools/oracle_observation_model.py、run/v6/sci-obs/tools/structural_evidence.py、run/v6/sci-obs/evidence.json、run/v6/sci-obs/logs/

> **基线分歧未裁决（强制标注）**：BASE-OWN-001（run/v6/base/baseline_freeze.json）机器判定工作树相对 HEAD 有 **16 个 tracked 文件回退到祖先 blob + 10 个 tracked 文件被删除**；控制器 run/v6/controller/state.md 记录该项（F1）已请示负责人但**尚未裁决**。本复核先在 HEAD=4b508f28 实测、并在报告时 HEAD=192fab35 复验：**tracked-M / tracked-D 恒为 27 / 10**，16 个回退文件的 worktree blob 与 baseline_freeze.json 逐条一致（见 §9.3）；未跟踪文件数随并行 Wave-1 任务产出增长（-uall 868→878、-unormal 173→177），与本任务无关——本任务仅新增 docs/science/v6/observation/OBSERVATION_MODEL_REVIEW.md 一个 tracked-scope 文件（run/v6/sci-obs/ 受 run/* gitignore 规则约束）。本文件中所有"现状"结论均同时核对 **worktree** 与 **HEAD** 两个状态；凡涉及回退文件的公式锚点，structural_evidence.py 同时校验两态（16/16 双态命中，见 §9.2），因此下述科学结论不因 F1 而改变，但**不把回退态当作已验证基线**。

---

## 0. 复核范围与方法

任务正文要求复核四件事：统一观测模型 d = A x + n、随机/共享/系统 covariance、Drizzle correlation、以及推导的**单位**与**失效域**。

方法（三条独立证据链，互不自证）：

1. **推导链**：从 docs/science/DRIZZLE.md(SCI-DRZ-001/014)、docs/science/UNCERTAINTY_AND_COVARIANCE.md(SCI-NOISE-011/012, DATA-UNC-001)、docs/science/UNIFIED_SCIENCE_MODEL.md(ASTROCS-SCIENCE-MODEL-001) 逐式重推；
2. **独立 Oracle**：run/v6/sci-obs/tools/oracle_observation_model.py（纯 numpy，显式线性代数 + 闭式 + 定种子 Monte Carlo，**不调用、不链接、不执行任何生产 C/C++ 实现或生产测试二进制**；被测公式以 subj_* 转写，只与独立参考比对）；
3. **结构证据**：run/v6/sci-obs/tools/structural_evidence.py（worktree/HEAD 双态锚点核对），只证明"被引用的公式确实在该位置"，不承担正确性证明。

---

## 1. 结论摘要

| # | 结论 | 判定 |
|---|---|---|
| 1 | d_k=A_k x+n_k, Cov(n_k)=C_k 的线性高斯推导、点源 Q/W、W_info 白噪近似、1/W Fisher 方差在独立 Oracle 下**逐项成立** | 一致 |
| 2 | 扩展源 GLS (AᵀC⁻¹A)⁻¹AᵀC⁻¹d、Cov=(AᵀC⁻¹A)⁻¹ 与逐像素 ivar 退化的**条件**表述正确 | 一致 |
| 3 | Drizzle 逐像素方差 variance_p=Σ_j v_j w_jp²/D_p² 与 Cov(S_p,S_q)=Σ_j c_jp c_jq v_j 的对角元**严格相等**（rtol 1e-11，实测 5.6e-17） | 一致 |
| 4 | Drizzle 输出**非对角协方差**必然非零；aperture 方差按"逐像素方差求和"会**低估**（合成 footprint 实测 2.46×） | 已文档化边界 |
| 5 | HiPS 父级 tile 方差归约只保留对角项，是**系统性低估**（合成 footprint 相对缺口 0.82，且因权重非负必为下界），当前无误差门 | finding F-OBS-03 |
| 6 | DRIZZLE.md §3 符号表把输入 v_j[ADU²] 与输出 variance_p[ADU²/px⁴] 同名写作 "variance"，与 §5/§7 自身推导冲突 | finding F-OBS-01 |
| 7 | 常量面亮度不变量 S_p=B0 **只在 pixfrac=1 成立**；pixfrac=0.8 时独立 Oracle 得 S_p=B0/0.64，生产常量场门固定 pixfrac=1，pixfrac<1 无门 | finding F-OBS-02 |
| 8 | 共享系统项（共同 master / 共同天空）未进入任何 covariance 面；独立帧简单求和在存在共同模式时**过乐观**（合成 3.48×） | finding F-OBS-04 |
| 9 | k_corr/N_eff 机制方向正确（独立 MC 复现 k_corr>1），但冻结值 1.3883/1.4 依赖生产球面几何，本任务**无法独立复现该数值** | finding F-OBS-05 |
| 10 | Phase3 var_out=Σc_k²u_k、Σc_k²≠1 表述正确；psfsw 无量纲不得充当 ivar 的边界在合同与实现一致 | 一致 |

**建议状态：REVIEW_REQUIRED**（见 §10 与 §13：存在 1 项冻结 SCI 文档单位/命名内部不一致 + 1 项 pixfrac 相关归一失效域缺口 + 1 项父级方差误差门缺失 + 1 项共享系统项实现缺口 + 1 项冻结标定值不可独立复现，均需 SCI-ADJ-001 / 负责人裁决；任务本身逐项完成、负向门全红）。

---

## 2. 统一观测模型 d = A x + n

### 2.1 模型与算子分解

上位要求（PROJECT_SPEC §3；UNIFIED §2；PHASE1 设计 §1）：

~~~
d_k = A_k x + n_k ,   Cov(n_k) = C_k
~~~

A_k = 光度响应 a_k ∘ PSF P_k ∘ 像素响应 ∘ WCS ∘ 重采样 R_k（UNIFIED §2；PHASE1 §1）。各 Phase 的具体形态：

- 点源：d_k = a_k F P_k + n_k（UNIFIED §4；PSF_SIGNAL_WEIGHT §2）；
- 扩展源/Phase2：y_k(x) = g_k s(x) + b_k(x) + ε_k(x)（PHASE2 设计 §4，g_k 乘法响应、b_k 加性背景，**不得互相代替**）；
- Phase3：y = R x（PHASE3 设计 §4）。

### 2.2 独立 Oracle 验证（无生产实现参与）

oracle_observation_model.py 门 C1/C2：

- 用显式矩阵构造 A（3 帧 × 6 像素、每帧独立 σ_k、PSF 归一 ΣP=1），分别计算 x̂=(AᵀC⁻¹A)⁻¹AᵀC⁻¹d 与逐帧闭式 F̂=ΣQ_k/ΣW_k：实测 x̂ = 6.971861 = Q/W（<1e-9），Var = 1.329235e+00 = 1/ΣW（<1e-12）；
- Monte Carlo（n=6000，Cholesky 独立生成）实测方差 1.357677，与解析 1/W 相对偏差 0.0214 < 0.08。

结论：Q/W 与 GLS 在**对角 C** 下等价、Var(F̂)=1/W 成立，符合 UNIFIED §4/§4.1、PHASE2 设计 §6.2、PSF_SIGNAL_WEIGHT §2。
文献锚：Horne 1986, PASP 98, 609（DOI 10.1086/131801）最优提取统计结构；Naylor 1998, MNRAS 296, 339 成像最优光度；Zackay & Ofek 2017, ApJ 836, 187（arXiv:1512.06872）每帧按自身 PSF matched filter 后组合。

### 2.3 W_info 白噪声近似

PHASE1 设计 §8.1 / PSF_SIGNAL_WEIGHT §2：

~~~
W_info,k = a_k² P_kᵀ C_k⁻¹ P_k
白噪:      W_info,k = a_k² / (σ_pix,k² A_NEA,k) ,  A_NEA = 1/Σ P_k,p²
~~~

门 C3 实测：a²Σp²/σ² = 4.815353e-02 = a²/(σ²A_NEA)，相对误差 <1e-12。**注意该等价不需要 ΣP=1**（A_NEA 定义已吸收归一），但 ΣP=1 是 PHASE1 §6/§9 与 S_p 单位语义的前提，仍须逐帧成立。

### 2.4 用词纪律

宪章 §4.1 要求 signal/variance/ivar/snr/quality/support/coverage/validity/rejection/provenance 不复用模糊名；UNIFIED §3 禁止无前缀 weight/snr。现状：lib/algorithms/coverage/src/stage2_common.cpp:373-384 仅接受 auto/ivar/equal/support_x_snr2（整数 weight_mode），lib/infrastructure/scheduler/src/module_adapters.cpp:4261-4270 在节点链上只放行 1/2 并显式拒绝 legacy 0——方向上与 DATA_SEMANTICS §30.1 一致，但**目标三模式 point_information/surface_gls/psfsw_robust 在生产面仍 0 命中**（run/v6/base/gap_baseline.md §1；本任务不改实现，仅登记）。

---

## 3. 随机 / 共享 / 系统 covariance 分类与传播

### 3.1 三类量的权威口径

UNIFIED §6、PHASE1 设计 §4.2：

| 类别 | 物理来源 | 合同要求的表示 | 现状实现 | 锚 |
|---|---|---|---|---|
| **独立随机项** | 天空/读出/暗电流/量化逐像素噪声 | 对角 variance / ivar=1/variance | 有：NoiseWeightModelV1（SCI-NOISE-001..015） | NOISE_MODEL §5/§7；constitution §4.1 |
| **共享相关项** | 共同 master（bias/dark/flat）、共同天空/背景、重采样相关 | 低秩 covariance / 相关核 / 共同 master ID + 强度参数（**不存完整巨矩阵**） | **未实现**（仅逐像素对角 + Drizzle 后 k_corr 标量代理） | PHASE1 §4.2；UNIFIED §6/§7 |
| **系统/模型偏差** | 光度零点、PSF 模型误差、WCS、UPM 参数 | 进 validity/quality + 系统误差预算 + 参数 covariance；**禁止伪装随机 ivar** | UPM 参数 covariance 有合同要求（PHASE2 §4），但未逐像素入 variance | UNIFIED §6；PHASE2 §4 |

### 3.2 独立帧求和 vs 联合 C

UNIFIED §4：独立帧 Q=ΣQ_k, W=ΣW_k, Var=1/W；**相关帧必须使用联合 C，不能简单求和**。门 C4 构造 4 帧共享共同模式（幅度 0.6σ）后：

~~~
Var_joint(F) = 3.523773   vs   1/ΣW (naive) = 1.013967   → ratio 3.475
~~~

即：忽略共享系统项会把通量方差低估约 3.5 倍。这与 PROJECT_SPEC §8（"有相关项时验证简单求和被拒"）一致；DATA_SEMANTICS §30.1 的一般式 variance = Σ_i w_i²·v_i / W² 只对**实际使用的权重与仅含随机项的 v_i** 成立，不含共享项。

### 3.3 Phase2 马赛克方差的实现边界

DATA_SEMANTICS §30.1（DATA-P2-VAR-001）冻结：ivar_mosaic(p)=W(p)=Σ_i ivar_i(p)，variance_mosaic=1/W，一般式 Σ_i w_i²v_i/W²。现状：

- lib/infrastructure/scheduler/src/module_adapters.cpp:4704-4711 确实写 var_num_sum = cov²/W（即 variance_mosaic=1/W 经 writer 反归一）；uncertainty_available 仅在 weight_mode==2 且全部帧 ivar 存在时为 true（:4281-4306），等权/降级时置 false（fail-closed，符合 §30.1 unavailable 规则）；
- writer 归约 variance = var_num_sum/covered_area²（aio_hips_writer.cpp:668,721-724）。

失效域：该逐像素方差只在 (a) 权重为纯逆方差、(b) 输入帧 ivar 只含随机项、(c) 相邻像素独立 时是无偏的。Drizzle 后 (c) 不成立（§5），共同 master 使 (b) 不成立（§3.1），weight_mode=1 使 (a) 不成立（但此时产品直接 unavailable）。三者都不进入逐像素 variance 产品，是**结构性缺口**而非公式错误（unavailable 规则部分覆盖了 (a)）。

---

## 4. 点源 Q/W 与最优性

UNIFIED §4、PHASE2 设计 §6.2、PSF_SIGNAL_WEIGHT §2：

~~~
Q_k = a_k P_kᵀ C_k⁻¹ d_k      W_info,k = a_k² P_kᵀ C_k⁻¹ P_k
F̂ = ΣQ_k/ΣW_info,k           Var(F̂) = 1/ΣW_info,k
固定参考通量 F_ref: SNR² = F_ref² W        (独立帧 SNR_comb² = Σ SNR_k²)
~~~

- 门 C1/C2 已验证等价性与 Fisher 方差（§2.2）；
- 白噪近似见 §2.3；
- 适用域（PSF_SIGNAL_WEIGHT §2；PHASE1 §9）：P_k、a_k、C_k 必须已知；**仅有 Drizzle 后逐像素 ivar 无法重建 W_info**（会丢 PSF/协方差）；标量压缩必须过 UNIFIED §8 / PHASE1 §8.3 的均匀性与信息损失门。
- 单位：P_k 无量纲（ΣP=1）、C_k⁻¹ 为 [signal]⁻²、d_k 为 [signal] ⇒ Q_k 为 [signal]⁻¹，W_info 为 [signal]⁻²，F̂ 为 [signal]，Var(F̂) 为 [signal]²（门 C1 全程自洽）。

**禁止**（UNIFIED §11）：把 median(source SNR)、PSF 拟合残差、FWHM、support、coverage、q_psf 或 psfsw 复合权冒充 W_info/ivar。门 I1 用合成场实测这些代理与真实 σ 的最大 |corr| = 0.036 远小于 0.99。

---

## 5. Drizzle 线性算子与 correlation

### 5.1 前向算子与单位

DRIZZLE.md §5（SCI-DRZ-001/014）：

~~~
w_jp = a_jp / A_drop,j ;  F_p = Σ_j x_j w_jp ;  D_p = Σ_j a_jp ;  S_p = F_p / D_p
variance_p = Σ_j v_j w_jp² / D_p² ;  ivar_p = 1/variance_p
缩放律 x→αx ⇒ var→α²var, ivar→ivar/α²
~~~

线性性：S_p = Σ_j c_jp x_j，c_jp = w_jp/D_p（D_p 只依赖几何）。因此

~~~
Cov(S_p,S_q) = Σ_j c_jp c_jq v_j    (UNCERTAINTY_AND_COVARIANCE.md, 协方差节)
~~~

门 D1：随机稀疏 footprint（非生产几何）下 diag(Cov) 与 Σ_j v_j w_jp²/D_p² 最大偏差 **5.551e-17**（rtol 1e-11），非对角非零元 492 项、max|offdiag| = 2.152e-01。**推导与实现一致**（实现：drizzle_engine.cpp:1555-1560 累加 v·w²，aio_hips_writer.cpp:722 除以 area²）。

### 5.2 失效域：aperture 与父级归约

- **aperture 方差未建模**（UNCERTAINTY "对使用的约束"）。门 D2（合成 footprint）：aperture 真实方差 0.983433 vs 逐像素方差之和 0.399956，**低估 2.46×**；这与文档"aperture 误差须显式加入 Cov 项"一致，属**已声明边界**。
- **HiPS 父级 tile 方差**（aio_hips_writer.cpp:668-671, 1110-1126）用 variance_parent = Σ_p var_num_p/(Σ_p D_p)²，即只保留对角项。精确值为 Var(S_parent)=Σ_{p,q} (D_pD_q/(ΣD)²) Cov(S_p,S_q)；门 D3 实测相对缺口 **0.8225**（exact 7.139e-03 vs 对角式 1.267e-03），且因权重与重叠面积非负、共享源像素的 Cov(S_p,S_q)≥0，该式**恒为下界（过乐观）**。
  PROJECT_SPEC §3 要求任何"对角 covariance 假设"必须有适用域与误差门；DRIZZLE.md §9a 只说"不存完整协方差矩阵"，未声明父级低估，也**无误差门** → finding F-OBS-03。

### 5.3 k_corr / N_eff（UPM control estimator）

UNCERTAINTY_AND_COVARIANCE.md §V19R3 control estimator 方差（ALG-UPM-CONTROL-IVAR-001）：

~~~
control_variance = k_corr × (π/2) × sigma_bg² / N_retained ;  control_ivar = 1/control_variance
独立 Gaussian 基线 Var(median) ≈ πσ²/(2N)
~~~

冻结值：k_corr=1.3883（pixfrac=0.8，N_retained≈251，N_eff≈181，UPMW-005 MC），保守冻结 **1.4**（lib/algorithms/coverage/src/sampler.cpp:81-82）。门 D5 用**平面 pixfrac=0.8 独立 MC**（4000 实现、8×8 patch、N=64、固定亚像素 shift=0.37）测得 Var(median)=4.011933e-02、σ_marg²=7.460978e-01 ⇒ **k_corr=2.1909 > 1**。

结论：**机制方向被独立复现**（Drizzle 后 k_corr>1、N_eff<N_retained），但**冻结数值 1.3883 未被独立复现**——它依赖生产球面几何、pixfrac、控制 patch 大小与稳健估计器口径。本任务据此登记残余风险 F-OBS-05，建议 SCI-ADJ-001 要求把 k_corr 的几何/口径/provenance 显式化并附可复跑 MC（现状 sampler.cpp:85-92 已有 K_CORR_DOMAIN 选项 B 的按像素尺度查找表，方向正确）。

### 5.4 常量面亮度不变量与 pixfrac（**F-OBS-02**）

DRIZZLE.md §5/§7 同时声明：

- §5：x_j = ∫_{pixel_j} B dΩ = B0·A_pixel_j（输入=源像素积分通量），S_p = F_p/D_p，§5 明写"输出 S_p=F_p/D_p 把通量按覆盖面积归一为面亮度"；
- §7：常数**面亮度**场 B0 → 输出 S_p = B0（无条件声明）。

但由 w_jp=a_jp/A_drop,j、A_drop,j=pixfrac²·A_pixel,j、D_p=Σ_j a_jp：

~~~
S_p = Σ_j x_j a_jp / (A_drop,j · D_p)
    = Σ_j B0 A_pixel,j a_jp / (pixfrac² A_pixel,j · D_p)   (常数 A_pixel)
    = (B0/pixfrac²) · (Σ_j a_jp)/D_p = B0 / pixfrac²
~~~

独立 Oracle 门 D4（平面、drop 全落内部）实测 S/B0：**pixfrac=1.0 → 1.0000；0.8 → 1.5625（=1/0.64）；0.5 → 4.0000（=1/0.25）**。即 §7 恒等式**只在 pixfrac=1 成立**。

结构证据：生产常量面亮度门本身被固定在 pixfrac=1——lib/algorithms/drizzle/healpix_drizzle/tests/p1drz/p1drz_tests_core.cpp:71 make_cfg(NSIDE, 1.0, 1, true)，p1drz_fixtures.hpp:119 注释"与生产 drop=pixel (pixfrac=1) 口径一致 → S_p=B0 恒等式剩余…"，p1drz_oracle.hpp:124-127 明确写"当 A_pixel_j≈A_drop,j（同尺度像素），S_p→B0"，并给门 |S_p/B0-1|<1e-3。**pixfrac<1 的常量面亮度不变量无任何门覆盖。**

对照：通量守恒 Σ_p F_p = Σ_j x_j 对任意 pixfrac 成立（p1drz_oracle.hpp:190-192），所以偏差只出在**面亮度归一/单位**上。UNIFIED §7 明确要求"Drizzle 的 signal 单位、源/目标像素面积、pixfrac 和归一必须统一；**常量面亮度和总积分通量 Oracle 同时成立**"，宪章 §5.3 要求"Drizzle 的单位及误差传播必须明确"。故 F-OBS-02 是科学口径/失效域问题，非纯实现细节：若该 1/pixfrac² 是有意设计，必须**显式声明并由 a_k/provenance 补偿**，且修正 §5/§7 表述；否则应改为面亮度保持的归一口径（c_jp=a_jp/Σ_j a_jp）。**本任务不改公式、不改容差、不改门。**

---

## 6. Phase3 重采样传播

DATA_SEMANTICS §30.4（DATA-P3-UNC-001）与 PHASE3 设计 §4：

~~~
y = R x      ⇒   C_y = R C_x Rᵀ
nearest : var_out = u_in
bilinear: var_out = Σ_k c_k² · u_k     (c_k = ALG-P3-003 G4 冻结权重, Σc_k = 1)
ivar_out = 1/var_out (有限且 >0)
~~~

门 P1/P2：Σc_k²u_k = diag(R diag(u) Rᵀ)（1e-14），非退化位置 Σc_k²=0.3364 ≠ 1；常数方差场 u=3 得 var_out=1.0092 = u·Σc² ≠ u。结论：**"Σc_k²≠1 是正确物理"** 的表述成立——但仅在输入像素独立时；若输入 u 本身来自 Drizzle（已相关），R C_x Rᵀ 的非对角项同样被丢弃，var_out 仍为下界（与 §5.2 同源失效域）。现状实现 lib/phase3_session/p3_resample.cpp:508 为 acc += w*w*u_k（worktree 与 HEAD 双态均有，见 §9.2）。

---

## 7. 单位表（逐量）

| 量 | 符号 | 单位 | 锚 |
|---|---|---|---|
| 源像素积分通量 | x_j | ADU（或 e⁻，同输入标度） | DRIZZLE §3/§5 |
| 天空面亮度 | B(Ω) | ADU/px² | DRIZZLE §5 |
| drop/目标重叠面积、覆盖面积 | a_jp, A_drop,j, D_p | px²（球面立体角等价） | DRIZZLE §3 |
| Drizzle 归一权重 | w_jp | 无量纲 | DRIZZLE §3 |
| 累积通量 | F_p | ADU | DRIZZLE §5 |
| 输出面亮度 | S_p=F_p/D_p | **ADU/px²** | DRIZZLE §5 |
| Drizzle 方差分子 | sumVarNum | ADU² | DRIZZLE §5 |
| 输入逐像素方差 | v_j | ADU² | NOISE_MODEL §3 |
| **输出逐像素方差** | variance_p=Σv_jw²/D_p² | **ADU²/px⁴** | DRIZZLE §5（**§3 符号表未区分，见 F-OBS-01**） |
| 输出 ivar | ivar_p | px⁴/ADU² | DRIZZLE §5 |
| 灰度/噪声 σ | σ_bg | ADU | NOISE_MODEL §3 |
| UPM control 方差 / ivar | control_variance | ADU² | UNCERTAINTY §V19R3 |
| 点源信息权重 | W_info | signal⁻²（ADU⁻²） | PSF_SIGNAL_WEIGHT §2 |
| 噪声等效面积 | A_NEA=1/ΣP² | px² | PHASE1 §6 |
| PSFSW 复合权重 | psfsw_robust_weight | **无量纲** | PSF_SIGNAL_WEIGHT §3/§5 |
| 帧级科学基准 | m_5 | mag | CONTROL_WEIGHT_SNR §2a |
| Phase3 输出方差 | var_out | (主 HDU BUNIT)² | DATA_SEMANTICS §30.4 |

门 U1 用独立量纲代数（基底 ADU/PX）验证了 x = B·A_pix、S = F/D 与 var_p = v·w²/D² 三条链自洽；门 U2 检出 DRIZZLE.md §3 把输出 S 标为 ADU、把 variance 标为 ADU²，与 §5 推导的 ADU/px²、ADU²/px⁴ 冲突 → **F-OBS-01**。

---

## 8. 失效域清单（逐条）

| ID | 失效条件 | 后果 | 现状是否有门 |
|---|---|---|---|
| FD-1 | 存在共同 master / 共同天空（跨像素/跨帧相关） | 独立 ivar 求和低估方差 | 无（§3.1，F-OBS-04） |
| FD-2 | 跨帧相关（同源）仍用 ΣQ_k/ΣW_k | 点源通量方差过乐观 | 文档声明（UNIFIED §4），无实现门 |
| FD-3 | Drizzle 后相邻像素相关，按对角方差做 aperture | aperture 误差低估（实测 2.46×） | 文档声明，消费侧须显式加 Cov |
| FD-4 | HiPS 父级 tile 方差归约只留对角 | 父级方差系统性下界（实测缺口 0.82） | 无（F-OBS-03） |
| FD-5 | pixfrac<1 且要求常量面亮度不变量 | S_p=B0/pixfrac² 尺度偏差 | 无（F-OBS-02） |
| FD-6 | 缺少 P_k/a_k/C_k 却宣 W_info | 信息权重错误 | 合同（PHASE1 §9；UNIFIED §11） |
| FD-7 | W_psf(x,y) 空间非均匀却压成帧标量 | 信息损失 | 有门要求（UNIFIED §8；PHASE1 §8.3），未验证 |
| FD-8 | Phase3 输入 u 已相关仍用 Σc²u | 输出方差下界 | 同 FD-3 |
| FD-9 | 把 psfsw/support/coverage/median SNR/FWHM/residual 当 ivar | 方差语义错误 | 合同禁止（UNIFIED §3/§11；PSF_SIGNAL_WEIGHT §8；门 W1/I1） |
| FD-10 | weight_mode≠2 或 ivar 帧缺失 | 应 fail-closed 不写 variance | §30.1 unavailable 规则；实现 module_adapters.cpp:4289-4306 |
| FD-11 | k_corr 与生产几何/pixfrac 解耦 | control_ivar 有偏 | 冻结 1.4 + 按尺度查找表，无独立复现（F-OBS-05） |

---

## 9. 独立 Oracle、结构与负向 mutation

### 9.1 独立 Oracle（run/v6/sci-obs/tools/oracle_observation_model.py）

- 纯 numpy，显式矩阵/GLS/MC；**不调用任何生产实现或生产测试二进制**；
- 被测公式以 subj_* 转写（来自 docs/science/*.md 与 DATA_SEMANTICS §30），只与独立参考（显式矩阵、闭式、定种子 MC）比较；
- 基线 15 门全过，rc=0（日志 logs/oracle_baseline.log）。

### 9.2 结构证据（run/v6/sci-obs/tools/structural_evidence.py）

16 条公式锚点**在 worktree 与 HEAD 双态均命中**（16/16，rc=0），其中 7 条位于 F1 回退文件（标记 [F1-DIVERGED]），说明回退未改变本复核涉及的公式文本。日志 logs/structural_evidence.log。

### 9.3 F1 基线分歧实测（只读 git）

| 项 | 实测 | 证据 |
|---|---|---|
| 任务基线 HEAD/main/origin/main | 4b508f28…（三 SHA 一致） | logs/rc_summary.txt |
| 报告时 HEAD | 192fab3546e710163e5dc276a9cd76309d650ff2（控制器集成 SCI-P2-001；diff 仅 docs/science/v6/phase2/** + TASK_LEDGER.csv） | run/v6/sci-obs/evidence.json.head_at_report |
| 本复核锚点在报告时 HEAD 复跑 | 结构 16/16 rc=0；Oracle 15/15 rc=0 | evidence.json |
| dirty（-uall / -unormal）@4b508f28 | 868 / 173 | 同 base_freeze |
| dirty（-uall / -unormal）@192fab35 | 878 / 177（增量=并行 Wave-1 未跟踪产物，非本任务） | logs/scope_check.log；本任务仅 1 个 tracked-scope 新文件 |
| tracked-M / tracked-D | 27 / 10（两态相同） | 同上 |
| 16 个回退 worktree blob | 与 baseline_freeze.json.worktree_vs_head.reverted_tracked_modified **逐条一致**（16/16） | 本任务只读 git hash-object 复核 |
| 10 个删除文件 | 仍在删除态（git status 列表一致） | 同上 |
| DATA_SEMANTICS §30 文本 | HEAD 与 worktree 逐字节相同（diff 空） | 本任务只读 diff 复核 |

### 9.4 负向 mutation（全部 rc≠0）

| mutation | 注入内容 | 目标门 | rc |
|---|---|---|---|
| drizzle_missing_square | var=Σv·w/D²（漏平方） | D1 | 2 |
| drizzle_missing_D2 | var=Σv·w²（漏 /D²） | D1 | 2 |
| p3_sigma_c_equals_1 | Phase3 用 Σc_k·u_k（误用 Σc=1 归一 variance） | P2 | 2 |
| white_noise_reciprocal | W=a²σ²A_NEA（倒数错） | C3 | 2 |
| gls_ordinary_least_squares | GLS 退化为 AᵀA | C1 | 2 |
| shared_systematic_is_independent | 宣称共享系统项可当独立 | C4 | 2 |
| aperture_independent_is_exact | 宣称 aperture 方差=逐像素方差和 | D2 | 2 |
| hierarchy_missing_crossterm_is_exact | 宣称父级对角归约精确 | D3 | 2 |
| pixfrac_invariant_holds_for_all | 宣称常量 SB 不变量对任意 pixfrac 成立 | D4 | 2 |
| kcorr_ignores_correlation | 令 k_corr=1（忽略相关） | D5 | 2 |
| psfsw_is_ivar | 把无量纲 psfsw 当 ivar | W1 | 2 |
| proxy_weight_is_variance | 宣称 support/SNR/residual 可当 variance | I1 | 2 |
| units_symbol_table_is_consistent | 宣称 §3 符号表与 §5 推导一致 | U2 | 2 |
| anchor_mismatch | 把 sumVarNum += v*w² 改成 sumVarNum += v | 结构锚点 | 2 |

逐条实测 rc 见 run/v6/sci-obs/logs/rc_summary.txt；机器字段见 evidence.json。

---

## 10. Findings（需裁决）

### F-OBS-01 冻结文档单位/命名内部不一致（DRIZZLE §3 vs §5/§7）

- 事实：§3 记 S,F,x: ADU/e⁻（记法冻结：斜杠为「主单位 + 等价标注」分隔符，ADU 为规范信号单位，e⁻ 为增益标定后的等价标注 [docs/GLOSSARY.md 第 9 行：electron = DRIZZLE 域允许的信号等价标注]，不得读作二选一；本条 finding 的歧义不在此记法，而在 variance 同名不同量纲）；v,variance: ADU²；ivar: ADU⁻²；§5/§7 推出 S_p=F_p/D_p（ADU/px²）与 variance_p=Σv_jw²/D_p²（ADU²/px⁴）。同名 variance 指两个不同量纲的对象。
- 违反：宪章 §4.1（不得混淆的量）、§5.3（Drizzle 单位与误差传播必须明确）、UNIFIED §7。
- 证据：门 U2；DRIZZLE.md:27,48,55,82。
- 建议：SCI-OBS→SCI-ADJ 裁定术语（例如 pixel_variance_in / sb_variance_out），并在 DATA_SEMANTICS §30.4 的 BUNIT² 表达处显式声明输出面亮度与方差的 px 幂次。

### F-OBS-02 pixfrac 相关面亮度归一（§7 常量面亮度不变量只在 pixfrac=1 成立）

- 事实：独立 Oracle 得 S_p = B0/pixfrac²（pixfrac=0.8 → 1.5625×）。
- 违反：UNIFIED §7（常量面亮度与总积分通量 Oracle 必须同时成立）、宪章 §5.3。
- 证据：DRIZZLE.md §5/§7；生产常量场门固定 pixfrac=1（p1drz_tests_core.cpp:71、p1drz_fixtures.hpp:119、p1drz_oracle.hpp:126）；门 D4。
- 附加结构对照：反向 drizzle（lib/algorithms/drizzle/healpix_drizzle/reverse_drizzle.cpp:213-214）用 signal += sig·w、w=ov/drop_area 且 Σ_p w=1（映射的已是 S_p 面亮度），因此反向算子在 pf<1 时**保持面亮度**；正向算子在 pf<1 时**不保持**。该不对称本身即支持 F-OBS-02 需裁决。
- 范围限定：本 finding 针对 DRIZZLE.md §5/§7 的前向 Drizzle 算子与 drizzle_engine.cpp → aio_hips_writer.cpp 链（Phase1 产品）；反向 drizzle 归 reverse_drizzle.cpp，不在本 finding 范围。
- 候选裁决：**(A)** 认定 §7 缺 pixfrac=1 条件，修正 SCI 表述并把 pixfrac 标定为 a_k/provenance 的一部分（要求跨 pixfrac 合成时显式补偿）；**(B)** 认定归一口径应改为面亮度保持（c_jp=a_jp/Σa_jp），作为 SCI 变更 + 重跑全部 Drizzle 不变量与 variance 门。**本任务不改。**

### F-OBS-03 HiPS 父级方差只保留对角项，无误差门

- 事实：aio_hips_writer.cpp:668-671,1110-1126；门 D3 实测相对缺口 0.82（合成），且必为下界。
- 违反：PROJECT_SPEC §3（对角 covariance 假设须有适用域与误差门）。
- 建议：或在 DRIZZLE.md/DATA_SEMANTICS 明写"父级 variance 是下界，不得用于 aperture/总量误差"，或增加跨子像素协方差项/误差门；归 SCI-ADJ-001 决策。

### F-OBS-04 共享系统项未进入 covariance 面

- 事实：现状只逐像素对角随机方差；module_adapters.cpp 的 ivar 求和即 Σivar_i；无低秩/共同 master ID 项。门 C4 实测 3.48× 低估。
- 违反：UNIFIED §6、PHASE1 §4.2、PROJECT_SPEC §3。
- 建议：SCI-ADJ/DATA-DESIGN/ALG 明确随机项与共享项的数据面（低秩因子/相关核/master ID），并规定 unavailable 或误差门；不得把共享项继续留空。

### F-OBS-05 k_corr 冻结值不可独立复现（残余风险）

- 事实：k_corr=1.3883（冻结 1.4，pixfrac=0.8）依赖生产球面几何与控制 patch 口径；独立 MC 只复现方向（k_corr≈2.19，平面几何）。
- 建议：把 k_corr 标定脚本/几何/provenance 纳入可复跑证据（UPMW-005 留档为线索，非当前可复跑），并在 DATA_SEMANTICS §30 声明其适用域（pixfrac/尺度/估计器）。

---

## 11. 科学/算法/接口/代码/测试一致性判定（本任务范围内）

| 面 | 判定 | 依据 |
|---|---|---|
| 科学定义（SCI） | 与 d=Ax+n、Q/W、GLS、Drizzle variance、Phase3 传播一致（除 F-OBS-01/02/03 的文档口径缺口） | §2–§7；门 U/C/D/P |
| 算法（ALG） | 未新增 ALG；引用锚点齐全 | §12 表 |
| 接口/合同（DATA/API） | DATA_SEMANTICS §30.1/§30.4 与 SCI 权威一致；§30 在 HEAD 与 worktree 逐字节相同（本任务实测 diff 为空，rc=0） | §9.3 |
| 代码 | 本任务**未改代码**；引用实现锚点双态命中 | §9.2 |
| 测试 | 本任务**未新增生产测试**；独立 Oracle 15 门 + 结构 16 锚点 + 14 负向 mutation 全红 | §9 |

---

## 12. 条款与文献锚（逐 claim）

**条款锚**：宪章 ASTROCS-CONSTITUTION-001 §1.1/§4.1/§5.3/§6.3/§7.3/§12.3/§14.4；PROJECT_SPEC §3/§4/§5/§6/§7/§8/§9；PHASE1_DETAILED_DESIGN §1/§4.1/§4.2/§6/§8.1/§8.2/§8.3/§9/§10/§11；PHASE2_DETAILED_DESIGN §4/§5/§6.1/§6.2/§6.3/§7/§9/§10；PHASE3_DETAILED_DESIGN §3/§4/§5；UNIFIED_SCIENCE_MODEL §2/§3/§4/§4.1/§5/§6/§7/§8/§9/§10/§11；PSF_SIGNAL_WEIGHT §1/§2/§3/§4/§5/§6/§7/§8；DRIZZLE.md（SCI-DRZ-001,014,015,016）§2/§3/§5/§6/§7/§8/§9a/§10/§11；NOISE_MODEL.md（SCI-NOISE-001..015）§3/§5/§7/§8/§10/§11；INTEGRATION.md（SCI-INT-001,002,004,008）§5/§7/§9/§10；CONTROL_WEIGHT_SNR.md（SCI-CW-001..008）§2a/§4/§5/§7；UNCERTAINTY_AND_COVARIANCE.md（SCI-NOISE-011/012, ALG-UPM-CONTROL-IVAR-001, DATA-P2-VAR-001/DATA-P3-UNC-001）；DATA_SEMANTICS.md §30.1/§30.3/§30.4/§30.5；RULINGS.md 3/4/5；00_READ_FIRST.md 科学模式段。

**文献锚**：Fruchter & Hook 2002, PASP 114, 144（Drizzle 线性重建 / drop / pixfrac / 相关噪声；bibcode 2002PASP..114..144F）；STScI DrizzlePac Handbook（pixfrac 实践语义）；Horne 1986, PASP 98, 609（DOI 10.1086/131801，最优提取 PᵀC⁻¹P）；Naylor 1998, MNRAS 296, 339（https://academic.oup.com/mnras/article-pdf/296/2/339/2988643/296-2-339.pdf，成像最优光度）；Zackay & Ofek 2017, ApJ 836, 187（https://arxiv.org/abs/1512.06872）与 836, 188（https://arxiv.org/abs/1512.06879）；Górski et al. 2005, ApJ 622, 759（HEALPix）；Padmanabhan et al. 2008, ApJ 674, 1217（重叠相对定标/gauge）；Newberry 1991, PASP 103, 122（DOI 10.1086/132801，CCD SNR 上下文）；PixInsight ImageWeighting（https://pixinsight.com/doc/docs/ImageWeighting/ImageWeighting.html，PSFSW 工程语义，访问核对 2026-09-15）。

---

## 13. 未决风险与需控制器裁决事项

1. **F1 基线分歧未裁决**（16 tracked 回退 + 10 tracked 删除，HEAD=4b508f28 实测仍在）。本复核的公式锚点在两态均存在，故科学结论不随之改变；但**任何以"工作树"为基线的实施任务起点不同**，控制器须先裁决。
2. **F-OBS-01/02/03/04/05** 需 SCI-ADJ-001（Wave 2）汇总裁决；其中 F-OBS-02 影响光度零点，F-OBS-03/04 影响方差可信度声明，F-OBS-05 影响 UPM 权重标定。
3. 本任务**只读**：未改任何 SCI/ALG/DATA/API 公式、容差或冻结门；未改源码与测试；上位规范 docs/science/*.md、docs/owner/**、docs/design/**、docs/references/** 全部未动（见 §14 写域自查）。
4. k_corr 与 DATA_SEMANTICS §30 unavailable 规则的**真实数据**验证不在本任务范围（Wave 10 REAL-SCIENCE-001）。

---

## 14. 写域自查（机器证据）

- 本任务全部写入且仅写入：docs/science/v6/observation/、run/v6/sci-obs/；
- git status --porcelain 中本任务新增路径全部落在这两个前缀内（见 run/v6/sci-obs/logs/scope_check.log，rc=0）；
- **未 commit、未 push、未 git add、未建分支/worktree、未 stash/reset/clean/rebase、未派生 subagent**。
