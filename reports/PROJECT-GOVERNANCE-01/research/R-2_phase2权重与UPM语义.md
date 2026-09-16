# R-2 phase2 权重与 UPM 语义（研究线交付书）

> 研究线：工程控制/PROJECT-GOVERNANCE-01/RESEARCH_TASKS.md R-2 ｜ 覆盖议题：D-7 / D-8 / D-9 / D-10 / M4-A-01 / M4-A-02 / M4-F-01 / M7-H-101 / M7-H-103 / M8-C-002
> 状态：研究完成（**不是整改**；零 git 写、零仓库文件改动；全部产物落 reports/ 与 run/）
> 纪律：不采信 问题扫描 账本的 fix_state/verified_state；全部结论以本报告复跑的原始输出为准。

---

## 0. 结论速览（唯一推荐 + 置信度）

| 议题 | 唯一推荐结论 | 置信度 |
|---|---|---|
| **D-7** weight_mode=2 语义 | **mode 2 = 逐样本 ivar 逆方差权重，无 fallback**；ivar 缺失 → fail-closed 报错。三套「互斥权威」实为：SCI-UPM §5:54 + INTEGRATION §5 + 冻结 DATA 合同（UNCERTAINTY_AND_COVARIANCE.md:51「weight_mode=2，无 fallback」）一致；CONTROL_WEIGHT_SNR §4 的 support×snr² 是 **mode 0 legacy**（且是 SCI-CW 内部勘误）；ALG PHASE2_INTEGRATION.md:125-127 的 ivar_valid?ivar:support 只是「显式开启 legacy 降级」分支，**不得写成 mode 2 的定义**。 | **高** |
| **D-8** w_UPM 绝对量 vs 份额 | 两者**不是互斥式而是同一条流水线的两级**（ALG F1→F2）：w_UPM（绝对，ADU⁻²，§5:46/§3:29）是科学定义；raw/Σraw·geom（无量纲，§5:47）是求解器内归一。**实现用份额式，且生产求解器对 control_ivar 绝对尺度按位不变**（探针实测 model_hash 相同）。统计上绝对逆方差权才满足 GLS/BLUE；份额式把「跨 control 精度」丢弃（等价于「单元总权恒定」），在冻结 λs=0.1 下使 UPM 零点恢复精度劣化 **2.0x**。推荐：**保留份额式为求解器约定、把两式分别命名并冻结其适用范围**（不改码），并在 SCI §7/§10 登记份额式丢弃跨 cell 精度这一性质。 | **高**（实现事实）/ **中高**（统计结论） |
| **D-9** CONTROL_WEIGHT_SNR §4 分支号 | **纯勘误，确认**：support×snr² 是 weight_mode=0（stage2_common.cpp:381-382 support_x_snr2 → = 0；stage2.cpp:1123 else if (cfg.weight_mode == 0)）。文中 :66「weight_mode=2」应改为 **0**。 | **高** |
| **D-10** k_corr 三问 | ①查表适用域**未声明**（只有 clamp 边界）；②**确认不相交**：标定域 input 300–600″/px vs 生产真帧 0.9586″/px（且 provenance 生产者实写 abs(cd[0])*3600=0.009856″，另有 −98.97% 误差）⇒ clamp 静默恒取 300″ 档；③**必须强制 k≥1**：k_corr 定义即「Drizzle 输出协方差导致 N_eff<N_retained」，独立 MC 实测 k(drop→0)=0.99、k(drop=0.583)=1.30、k(drop=4)=9.60，**k<1 在物理上不可达**；p2_upm_control_variance(0.9,…) 现返回 rc=0（接受）。推荐：**删除 scale 维（或显式域拒绝）→ 生产恒用冻结默认；k_corr 强制 ≥1 并越界显式拒**。 | **高** |
| **M4-A-01** N≤4 全拒→全接受 | 分支**真实存在且可达**，但**前提需修正**：在 wbpp_2_9_1 auto 路由下全拒**只可能发生在 n=4**（n≤2 已被白名单截走；n=3/5/7 奇数的百分位带必含中位样本，60 万次 MC 全拒率 0；n≥6 走 winsorized）。推荐：**把该容错登记进 SCI（域写死 n≤4）而不是删码**，补 n=3/4/5/6 边界测试，并**同时登记根因**（percentile 的 scale=|median| 在近零天光像素上带塌缩：n=6/8 显式 percentile 的随机栈全拒率 70–74%）。 | **中高** |
| **M4-A-02** SCI INTEGRATION §5:58 自相矛盾 | **门的前提需要改判**：integrate.cpp **已按 header:17 修好**（B2-A7，sup_max 在 w==0 continue 之前），实测 support=0.900000=max(accepted)。因此剩余问题**只是 SCI 一处文本**：:58 的 max_{valid,W>0} 与 :21/:63/:75 + integrate.h:17 互斥 ⇒ **订正 :58 为 max_{accepted}**（变更 claim），并清掉 ALG §5/§7/§10 中「实现现状=SCI:58」的**过期登记**。 | **高** |
| **M4-F-01** SCI 点名测试「全树零源文件」 | **前提不成立（直接判定）**：control_median_mc_test.cpp(8594 B) 与 kcorr_matrix_test.cpp(6295 B) **存在于 lib/algorithms/drizzle/healpix_drizzle/tests/ 且 git-tracked**。真实事实是「**tracked-but-unbuilt**」：两名字在全部 CMakeLists/*.cmake（排除 run/build）与 ci/、.github/ 命中 **0**，ctest -N 414 项中亦无。⇒ 门应为「**SCI §11/§15 不得把未注册的 TU 当作可执行证据**」，修法是「注册或降级 SCI 状态」，不是「找文件」。 | **高** |
| **M7-H-103** null model 返回 0.0 | **fail-open 成立**，且 ALG 自身已写死矛盾（PHASE2_UPM_IMPL.md:309：未知 frame→**NaN**，null model→**0.0**）。0.0 同时是 gauge 参考帧的**合法 C 值** ⇒ 调用方不可区分。推荐：**null model 也返回 NaN**（与 DATA invalid=NaN 约定一致），并在 ALG §10 补哨兵条款。 | **高** |
| **M7-H-101** 未收敛口径 | 成立但需收窄：iterations/objective **已持久化**（upm.cpp:965-966），但 P2ModelInfo 不暴露、p2_upm_build 恒返回 0 ⇒ **API 面 fail-open**。推荐：在 ALG §4 冻结「迭代耗尽」记录语义 + 增补只读访问器（不改冻结结构体），并要求 stage2 diagnostics 落该字段。 | **中** |
| **M8-C-002** worker 无关性口径 | **门的判据弱于合同**：合同/注释称「worker 数无关（位精确）」(upm.cpp:516)，实测门只用 1e-6 (synthetic_gate.cpp:303)，而同配置重复门用 1e-12+hash exact (:314-318)。按构造（连续块划分 + 每 k/每帧不相交写 + 无共享累加器）跨 worker 数**应当位精确** ⇒ 门应为 **1e-12/位精确**；另 phase2_sampler_parallel.OneTvsTwoTDeterminism 在 Linux **SKIPPED**（无 HiPS fixture）⇒ 该层实际无门。 | **中高** |

---

## 1. 权威原文（逐条 文件:行 + 逐字原文 + 在权威链中的位置）

权威链（ASTROCS_DESIGN.md:9-32 §0）：① 本文最高设计 → ② AGENTS → ③ ENGINEERING_SPEC → ④ CONTROL_PACK_SPEC → ⑤ docs/ci → ⑥ docs/plugins；**docs/science 为「科学公式（权威）」、docs/algorithms 为「算法推导（权威）」**（:17-18，由 ① 以「公式引用/算法引用」虚线挂接）。

### 1.1 最高设计 ASTROCS_DESIGN.md（链①，冲突时以此为准）

- `:203`「把一组合同兼容的 Phase1 球面产品相对定标、排异并合成为可继续测量的马赛克；对不同科学目标提供**明确的最优统计量**，而不是一个万能 weight。」
- `:211` 流程图节点「upm 联合相对模型 **g·s+b**」（**注意：SCI 现行为纯加性**，见 1.2(A)；乘性 g 属 V6 目标态，docs/algorithms/v6/phase2-surface/ALG-P2-SURF-UPM.md:105 已登记「现行 PHASE2_UPM_IMPL.md 的模型是 M_k + C_i(p)，**仅加性**；本文件规定 V6 目标态新增乘法 g_k…实施责任 IMPL-P2-UPM-001 (W5)」——**非未登记冲突**）。
- `:231-232`「Phase2 **自动检测**输入 HiPS 是否有稀疏 SNR 层：有 → 帧级×帧内；无 → 帧级。」「**逆方差叠加**：每个天球像素会有很多个源像素输入，利用**每个源像素的 SNR 计算对应权重**（SNR → 逆方差权重），得到最优检测/测光功率——**不是直接用 SNR 加权**。」
- `:237`「检测与重建逻辑是 Phase2 的**标准行为**，不是可选开关。」
- `:241` §4.4 硬约束：「mosaic 的详细硬约束（UPM 不可互相代替、**coverage 不作权重**、排异是污染状态估计、**GLS/Q-W/psfsw 权重分离**、分块并行确定性等）见 docs/plugins/algorithms_phase2/ 各插件文档与 docs/science/」。

> **R-2 关键**：链① 同时给出两条硬判据——「**不是直接用 SNR 加权**」与「**coverage 不作权重**」。这两句直接判定 D-7 的两个 legacy 分支（support×snr² 用 SNR 直接加权；ivar→support 回退把 coverage 当权重）。

### 1.2 docs/science/（链：SCI 科学公式权威）

**(A) docs/science/PHASE2_UPM.md（SCI-UPM-001..010 + SCI-UPM-WEIGHT-001 + SCI-UPM-PERSIST-001；FROZEN T106 2026-08-23）**

- `:3`「> ID: SCI-UPM-001  范围: SCI-UPM-001..010 + SCI-UPM-WEIGHT-001 + SCI-UPM-PERSIST-001  状态: FROZEN (T106 冻结, 2026-08-23)  上游: SCI-SCOPE-001  下游 ALG: ALG-UPM-001..  模块: phase2 (upm/sampler)」
- `:8`「**非目标**：不处理乘性尺度差（已撤销）；…」
- `:18-22`「| control_variance | k_corr·(π/2)·σ_bg²/N_retained | ALG-UPM-CONTROL-IVAR-001 |」「| control_ivar | 1/control_variance | p2_upm_raw_weight |」「| quality_factor | 质量因子（cosmic/geom 质量）| SCI-UPM-WEIGHT-001 |」「| geometric_reliability | **几何可靠性（单 control 覆盖度）** | p2_upm_normalized_weights |」「| k_corr | Drizzle 相关校正 1.4 | sampler.cpp:672 |」
- `:29`「- C, raw, calibrated, σ_bg: ADU；control_variance: ADU²；control_ivar: ADU⁻²；quality, geom: 无量纲 [0,1]；**w_UPM: ADU⁻²**；…」
- `:34-35`「- control_ivar 有效要求 use_ivar_weight=1 时 control_ivar>0 且有限，否则 p2_upm_raw_weight rc=2 → build rc=2（DATA-UPM-CONTROL-UNC-001）。」「- k_corr 为 frames[f].kcorr>0 ? per-frame : cfg.control_k_corr，缺省 1.4（sampler.cpp:672）。」
- `:45-55`（§5 连续定义，**逐字**）：

```text
科学权重 (V19R3 冻结 SCI-UPM-WEIGHT-001):
  w_UPM = quality_factor × geometric_reliability × control_ivar
  raw = quality × control_ivar ; normalized = raw / Σraw · geom (per-control)
  control_ivar = 1 / control_variance
  control_variance = k_corr × (π/2) × σ_bg² / N_retained
  # control estimator = patch median (非单 leaf)
  # N_retained = clipping 后保留数 (非 n_total)
  # k_corr = 1.4 保守冻结 (MC 实测 1.3883, control_median_mc_test, pixfrac=0.8, 2000次)
  # N_eff = N_retained / k_corr
  # 禁 production 乘 star SNR / snr²/(1+snr²) / support^p；support 仅 eligibility/coverage
  # legacy snr²/(1+snr²)/unc² 仅 use_ivar_weight=0 ablation/诊断 (SNR-015)
```

  ⇒ **`:46` 是绝对式（§3:29 单位 ADU⁻²），`:47` 是份额式（无量纲）**；`:54` 是禁令；`:55` 把 legacy 限定在 use_ivar_weight=0 ablation。
- `:69-71` §6 假设：「控制采样 patch 足域近似高斯；**Drizzle 相关可用 k_corr≥1 表征**。」
- `:79` §7 不变量：「- **k_corr 缩放**：control_variance 随 k_corr 线性缩放，N_eff 反比缩放。」
- `:86` §8：「| use_ivar_weight=1 且 control_ivar≤0/非有限 | p2_upm_raw_weight rc=2 → build rc=2 | DATA-UPM-CONTROL-UNC-001 |」
- `:94` §9：「control_median_mc_test 的 k_corr 用 MC 校准后保守取整 1.4。」
- `:98` §10 不可接受变化：「- 在生产 w_UPM 中乘 star SNR / support^p / obs.ivar(已弃用)；…- 改变 k_corr 默认 1.4 或 parameter_rows ↔ frame_id 绑定语义；」
- `:105` §11 Oracle：「**UPMW 硬门 7 项**：001 snr 扰动不变、002 ivar 1:4→weight 1:4、003 星群不变、004 Var(median)≈πσ²/2N、005 MC k_corr≈1.3883、006 无 legacy SNR consumer 且缺 ivar rc=2、007 patch 真值恢复（control_median_mc_test, synthetic_gate）。」
- `:147` §15：「- §11 Oracle 全过（含 control_median_mc_test MC 一致性、gauge 唯一性、harmonic continuation 边界）；」
- `:142` §14 文献：「4. k_corr=1.4（MC 实测 1.3883，pixfrac=0.8，2000 次）：**项目自产 MC 证据**（control_median_mc_test），非外部文献。」

**(B) docs/science/CONTROL_WEIGHT_SNR.md（SCI-CW-001..008；FROZEN 2026-08-27 + P5-SNR 2026-09-14 重定义）**

- `:10-14`「> **P5-SNR 重定义注记（2026-09-14，负责人授权…）**：本文件所称 local_snr / frame_snr 实为**相对质量权重场**（quality_weight = frame_quality_scalar × local_quality_proxy/median），**不是科学信噪比**…」
- `:20-21`「作为 support × snr² 控制权重中的质量权重因子；该权重场**不是科学信噪比**…与 SCI-NOISE 的逐像素 variance/ivar（随机噪声倒数权重）**区隔**，二者量纲语义不同、不混用。」
- `:34`「| quality_weight | frame_quality_scalar × local_quality_proxy/median，无量纲相对质量权重（S4 重定义的规范名；snr_v 为其别名）|」；`:40`「| w_snr | 质量权重因子 = snr_v² | 积分/排异权重 |」
- `:50`「**质量权重场定位**：本文件 §4 的 quality_weight（local_snr/frame_snr）为**相对质量权重场**，**无量纲**，仅供采样/加权（support × snr_v²）…」
- `:57-58`「- local_snr、frame_snr：**无量纲**…；snr_v²：无量纲权重因子；」；`:101` §6「- **量纲区隔**：quality_weight/相对质量权重（无量纲）与 variance/ivar（ADU²/ADU⁻²）不混用；」
- `:66-71`（§4 连续定义，**逐字**）：

```text
# 像素级 SNR 权重（stage2 排异/积分，weight_mode=2）
for 每个候选 s:
  snr_v = local_snr_map[key(frame_id,tile,gx,gy)]        # 有局部星点 → 局部相对质量权重
          else frame_snr_by_id[frame_id]                # 缺失 → 整帧质量权重中位数
  quality_weight[s] = snr_v                              # 相对质量权重，非科学 SNR
  weights[s] = support[s] × snr_v²                      # 禁止 snr=1.0 伪装 unknown
```

- `:94-95` §5：「**与 SCI-NOISE 区隔**：variance/ivar 为逐像素随机噪声权重；local_snr/frame_snr 为区域/帧级**相对质量权重倍率**…二者**不混用**。」
- `:107-109` §7 不可接受变化：「将 local_snr 与逐像素 variance/ivar 当作同一权重语义；…把 quality_weight/local_snr/frame_snr 声明或解释为科学信噪比」。

**(C) docs/science/INTEGRATION.md（SCI-INT-001,002,004,008；FROZEN T108 2026-08-23）**

- `:15`「| weights[i] | 数值权重 w_i（可空=等权 1.0）| P2PixelStack.weights |」；`:27`「- …w, ivar: 信号⁻²（但本层不编码 ivar 语义，仅数值权重）；support: 无量纲 [0,1]；…」
- `:50`「  n_positive_weight = |{i | valid(i) ∧ weights[i]>0}|  (weights 空时视 1.0)」
- `:52-58`（§5 聚合伪码，**逐字**）：

```text
  否则若 n_positive_weight==0:
       status = (n_accepted==0) ? ALL_REJECTED : ZERO_VALID_WEIGHT
  否则:
       wsum = Σ_{valid,W>0} w_i
       vs   = Σ_{valid,W>0} w_i·x_i
       signal  = vs / wsum
       support = (support 空) ? 1.0 : max_{valid,W>0} support[i]   # canonical reducer, 覆盖并集保守下界
       n_used  = n_positive_weight
       status  = OK
```

- `:63`「与 lib/algorithms/coverage/src/integrate.cpp:10-79 及 …integrate.h:1-75 一致。**support 为 max(accepted support)** 且仅消费 pr.support，调用方不二次 max/mean。」
  ⇒ **§5:58 与 §5:63 同一节内自相矛盾**（valid∧W>0 vs accepted）；`:21` 符号表亦写「| sup_max | max(accepted support) canonical reducer |」。
- `:71-75` §7：「**零权重惰性**：w_i==0 时 signal 与未提供该样本等价…」「**支撑单调性**：sup_max = max(accepted support)，增样本不减 support。」
- `:88` §8：「| 全拒 n_accepted==0 | ALL_REJECTED | n_accepted==0 ? ALL_REJECTED |」
- `:94` §10：「- 将 support 改为 mean/sum 或二次聚合（canonical 为 max）；」

**(D) docs/science/REJECTION.md（SCI-REJ-001..008；FROZEN T107 2026-08-23）**

- `:33` §4：「方法合法且 method != AUTO 才进 kernel；n <= underdetermined_n(=2) 或 n < minimum_n ⇒ UNDERDETERMINED（rejection.h:153-154）。」
- `:62` §5：「  UNDERDETERMINED (n ≤2) → 不做猜测，全接受 (P2_REASON_UNDERDETERMINED)」
- `:79` §7：「- **UNDERDETERMINED 单调性**：n ≤2 恒 UNDERDETERMINED，不做剔除（recall=0 显式）。」
- `:86,88` §8：「| n ≤2 或 n < minimum_n | UNDERDETERMINED 全接受 | rejection.h:86 |」「| 全拒 | ALL_REJECTED | rejection.h:84 |」
  ⇒ SCI 全文**只给了 n≤2 的小样本白名单**，且「全拒→ALL_REJECTED」**无 n 相关例外**。

**(E) docs/science/UNCERTAINTY_AND_COVARIANCE.md（DATA-UNC-001，2026-09-09 冻结）**

- `:32`（M7-A-115 争议句，**逐字**）：「(本节公式隶属Phase2 UPM/ALG-UPM-CONTROL-IVAR-001，不属于lib/algorithms/noise_snr的NoiseWeightModelV1；后者仅提供σ_bg，**经sampler阶段乘k_corr=N_eff缩放**)」
- `:38-40`「control_variance = k_corr × (π/2) × sigma_bg² / N_retained / control_ivar = 1 / control_variance」
- `:43-47`「- k_corr 表征 Drizzle 输出协方差导致的 N_eff<N_retained：UPMW-005 MC（pixfrac=0.8，2000 实现）k_corr=1.3883，N_eff≈181/251；冻结 1.4；…- 生产 UPM 权重 = quality × geometric_reliability × control_ivar（SCI-UPM-WEIGHT-001），禁止再用单像素 ivar/support/SNR 乘因子。」
- `:51`（**D-7 的决定性条款**）：「马赛克加权积分（SCI-INT §5，**w_i=逐样本 ivar，weight_mode=2，无 fallback**）的方差传播是 SCI-DRZ-014 一般式…」
- `:56`「ivar_mosaic(p) = W(p) = Σ_i ivar_i(p)     # = wsum（SCI-INT §5 冻结量）」
- `:63-68`「- UPM control_variance…只进 w_UPM，与马赛克 variance 产品严格分离。- invalid（输出面）：无有效样本（n_used=0）→ **variance/ivar=NaN**（signal=NaN 同态…）；输入 ivar 非有限 → hard fail。- **unavailable（fail-closed）：weight_mode≠2 或 ivar 帧缺失 fallback 发生 → 不写 variance/ivar 产品** + manifest uncertainty_available=false（禁伪值）。」

### 1.3 docs/algorithms/（链：ALG 算法推导权威）

**(A) docs/algorithms/PHASE2_INTEGRATION.md（ALG-P2-INT-001）**

- `:125-129`（**D-7 的 ALG 侧原文**）：

```text
- 权重语义（调用方构造，本层无知）: weight_mode=2 →
  ivar_valid?ivar:support（1/ADU²，ivar 缺失样本 fallback support，
  stage2.cpp:1106-1117/:1113-1114）；weight_mode=0 →
  support×snr²（legacy/诊断，stage2.cpp:1124-1136）；
  weight_mode=1/weights=null → 等权 1.0（stage2.cpp:1139）。
```

- `:109-123` §5 逐公式（含 `:113`「sup_max = max(sup_max, support[i])  # **现状位置→DISP-P2INT-001**  :54-55」，`:112`「if (w_i == 0) continue # 零权重合法零贡献 :49」）。
- `:156-160` §6：「**合同表述（matrix 专项 parallel reduction tolerance）**: 同输入同配置下，结果与 worker 数无关（**1..N 线程 bitwise 一致**）——像素内无并行归约、像素间无共享累加器、统计归并定序；large_scale 激活强制串行。」
- `:166-174` §7：「**support reducer 表述矛盾（DISP-P2INT-002）**: SCI §5:58 写 max_{valid,W>0} support[i]（其中 valid 含 accepted 判据），integrate.h:17 冻结注释写 'max(accepted support)'——accepted 集 ⊇ {valid ∧ W>0}，零权重 accepted 样本为两者差集，表述冲突。实现现状（sup_max 在 w==0 continue 之后，:54-55）= max over {valid ∧ W>0}，即 **实现现状=SCI §5:58 表述**；header :17 为冻结合同文本。整改归 P2-INT-IMPL（实现对齐 header :17），归一后以 header 为唯一口径；本冻结层按实现现状如实登记，**禁止反向修改 SCI（FROZEN）**。」
- `:175-181` §7：「**sup_max 缺陷（DISP-P2INT-001，bughunt ledger R3-A…）**: 零权重 accepted 样本的 support 不进 max → 输出 support 偏低…**现状测试样本 support 全为正/同值，缺陷在既有门下不可达**」
- `:182-184` §7：「- SCI §5:63 声称与 'integrate.cpp:10-79' / 'integrate.h:1-75' 一致——实测文件为 **76 行/74 行**（锚漂移…）」
- `:211-212` §10 冻结禁改：「2. support canonical reducer 语义（**integrate.h:17 文本口径**；实现现状偏差仅按 DISP-P2INT-001 整改，禁止改语义解释）。」

**(B) docs/algorithms/UPM_SOLVER.md（ALG-UPM-001，DERIVED T206）**

- `:15-17`：「F1: w = quality·control_ivar (production) 或 qf·support^p·snr²/(1+snr²)/unc² (ablation)」「F2: per-control归一化: w_norm = w / Σw · geometric_reliability」
- `:35`：「  w_norm = w/Σw · geom per-control」
- `:99-101` §12：「control_variance=k_corr·(π/2)·σ_bg²/N_retained，control_ivar=1/var；…**误差排序：数值 FP64(≪1e-12) ≪ 科学/统计容差(k_corr 冻结, 控制噪声) ≪ 门禁**。」

**(C) docs/algorithms/PHASE2_UPM_IMPL.md（ALG-P2-UPM-IMPL-001）**

- `:309`（哨兵条款表，**逐字**）：「| p2_upm_evaluate_c | NaN | 未知 frame_id 返回 NaN（显式不可用；**null model 返回 0.0**） | :1273-1279 |」

**(D) docs/algorithms/PHASE2_SAMPLER.md（ALG-P2-SMP-001）**

- `:186-195`：「- 常数权威：kPiHalf=1.57079632679489661923（:83）；k_corr 默认…」「- **K_CORR_DOMAIN 选项 B（逐帧标定）**：优先帧 Drizzle provenance → kcorr_lookup(pixfrac, scale)（:547-555；**scale 未知→300 档**…）」「…cfg.control_k_corr（:839 回退链 frames[i].kcorr>0 ? per-frame : cfg.control_k_corr）」
- `:308`：「| k_corr MC 校准非猜测（sampler.h:49-50） | :82/:88-109/:547-555（选项 B 逐帧） | 一致（冻结保守值 1.4 ≥ 实证） |」
- `:405` §11.3 F2 门：「| F2 | kcorr_lookup 边界与角点：pf∈{0.5,0.8,1.0}×sc∈{300,600} 九值、域外 clamp、provenance 缺失回退 1.4 | 角点值 exact；插值点 rtol 1e-12 |」

### 1.4 ENGINEERING_SPEC.md（链③）

- `:25-28` §3 科学代码红线：「- 科学公式、权重/variance/ivar/SNR 定义、排异规则、归约顺序、精度与默认容差 **不可修改**；- 架构重构**不得**同时改动科学语义；迁移必须 bitwise 相等（顺序变化时先冻结容差并登记）；- 模块不得根据 CPU 型号改变公式；…- 数据对象按 docs/design/UNIFIED_MODEL.md 区分，**禁止一个字段承载多个含义**。」

---

## 2. 外部依据（第一性原理 + 标准/文献 + 自跑实验）

### 2.1 标准结论（教科书/文献级；本项目 SCI §14 亦采「教科书级，无公式引用」体例）

| # | 结论 | 外部锚 | 与 R-2 的关系 |
|---|---|---|---|
| S1 | **GLS/BLUE**：线性模型 y = Xθ + ε，Cov(ε)=Σ 已知时，w ∝ 1/σ² 的加权最小二乘是 θ 的**最小方差线性无偏估计**；任何其它正权给出 Loewner 意义下不更优的协方差。 | Gauss–Markov / Aitken (1935) 加权最小二乘；任意统计教科书「加权最小二乘=逆方差加权」 | D-7（ivar 权正当）、D-8（绝对式 vs 份额式） |
| S2 | **最优提取/叠加用逆方差权**是天文测光/共加的标准做法（optimal extraction）。 | Horne 1986, *An optimal extraction algorithm for imaging photometry*, MNRAS 296, 339（[PDF](https://academic.oup.com/mnras/article-pdf/296/2/339/2988643/296-2-339.pdf)）；本项目 CONTROL_WEIGHT_SNR.md:51 亦引 Horne 1986 | D-7：链①「SNR → 逆方差权重，不是直接用 SNR 加权」与文献一致 |
| S3 | **中位数渐近方差** Var(median) → 1/(4N f(m)²) = (π/2)σ²/N（Gaussian）。 | 标准渐近统计（Cramér）；本项目已冻结：UNCERTAINTY_AND_COVARIANCE.md:42「独立 Gaussian 基线 Var(median) ≈ πσ²/(2N)（UPMW-004 实证 ratio 0.997）」 | D-10 / k_corr 基线 |
| S4 | **相关样本的有效样本量** N_eff = N/(1+2Σρ_k)（平稳非负相关序列的均值）；**正相关 ⇒ Var(均值/中位数) ≥ iid 值 ⇒ N_eff ≤ N**，等号仅当 ρ≡0。 | Bayley–Hammersley (1946) / Anderson (1971) 有效样本量；drizzle 像素相关见 Fruchter & Hook 2002, PASP 114, 144 | D-10：**k_corr ≥ 1 是定义性约束**，k<1 会使 N_eff>N_retained（违反 S4） |
| S5 | NumPy 语义：np.median 对偶数样本取两中值平均（本项目 percentile 方法「偶数 n 的中位数非样本值」这一事实即源于此）；固定权下均值估计的方差为 Σw²σ²/W²。 | NumPy 文档 + 初等统计 | M4-A-01 的「n=4 才可达全拒」、E2 的方差膨胀 |

### 2.2 自跑实验（逐字输出见 §3.1 / §6；脚本 run/PROJECT-GOVERNANCE-01/R-2/exp_r2.py）

**(E1) 忠实 mini-UPM（与 upm.cpp:766-858 同构：参考帧 gauge、M_k 取参考帧观测、(diag(w)+λsL+λ0I)x=w(y−M)），F=5 / K=4×4 / R=3000 / σ 跨 96x / λ0=1e-3 / λs=0.1：**

```text
(A) 绝对逆方差 w=1/σ², λs=0.1（未换算）      : RMSE(C−Δa)=0.62701  帧间残差=3.37011  马赛克 coadd 误差=1.17321 ADU
(B) 绝对逆方差, λs 同比换算（等价有效正则）   : RMSE(C−Δa)=0.58802  帧间残差=3.71642  马赛克 coadd 误差=1.15904 ADU
(C) per-control 份额式, λs=0.1（=生产实现 F2）: RMSE(C−Δa)=1.17953  帧间残差=2.66409  马赛克 coadd 误差=1.30749 ADU
(D) 等权, λs=0.1                              : RMSE(C−Δa)=2.39703  帧间残差=1.30921  马赛克 coadd 误差=1.80659 ADU
份额/绝对(同λ) coadd 误差比 = 1.128x ; 份额/绝对(同λ) C 精度比 = 2.006x (越大越差)
```

**读数**：同等的有效正则强度下，份额式恢复**逐帧零点差**（UPM 存在的理由，SCI §1「消除逐帧背景/零点差」）的误差是绝对逆方差权的 **2.0 倍**，马赛克 coadd 误差 **1.13 倍**；等权再差一倍。⇒ **§5:46 绝对式在统计上正确（S1），§5:47 份额式与实现一致但非最优。**

**(E2) 马赛克 coadd 权重（精确式 Var=Σw²σ²/W² + 4e5 次 MC，σ 跨 10x，N=8）：**

```text
(a) w=1/σ²（ivar, Gauss-Markov BLUE）        : Var=6.467823e-02 ADU²  1.000x
(b) w=support×snr², snr=c/σ（对精度完美代理）: Var=7.734427e-02 ADU²  1.196x
(c) w=support×snr², snr 与 σ 无关            : Var=4.377689e-01 ADU²  6.768x
(d) 1/8 样本缺 ivar → w=support（无量纲）    : Var=7.359070e-02 ADU²  1.138x
(d) 相对权 w_fb·σ² 均值 1.062e+00（最优=1），跨度 9.11e-03..8.97e+00
```

**读数**：即使「最有利于 legacy」的情形（质量代理恰好等于 1/σ），support 因子也带来 1.20x 方差膨胀；代理与真实精度无关时 6.77x；**ivar 缺失回退 support 的相对权重在 3 个数量级间任意漂移**（量纲不齐：support∈[0,1] 无量纲 vs ivar ADU⁻²）。

**(E3) k_corr 起源 MC（独立实现：输出像素 = drop 面积加权均值；patch 8×8；R=2000–40000）：**

```text
drop= 0.050  k_corr=1.0028      drop= 0.099  k_corr=1.0392
drop= 0.200  k_corr=1.0955      drop= 0.583  k_corr=1.2966   ← 表 300"/px 档实测 1.3925（同量级，机制复现）
drop= 1.166  k_corr=2.0173      drop= 2.000  k_corr=2.9558   ← 表 600"/px 档实测 2.8971
drop= 4.000  k_corr=9.6037
生产域 drop=0.00186（input 0.9586"/px → output nside512≈412"，pixfrac 0.8）: k_corr=0.9874
```

**读数**：① k_corr 是 drop 尺寸（= pixfrac×input_scale/output_scale）的强函数，**标定域 300–600″ 与生产 0.9586″ 不可迁移**；② 全部实测 k ≥ 1（生产域 ≈1），**S4 的 k≥1 得到数值确认**；③ 表值（1.3925/2.8971）与独立 MC（1.2966/2.0173）同量级同单调，说明表的机制真实、只是**域错**。

**(E4) 生产库直接驱动**：C++ 探针 probe_r2 链接 build/libastrocs_phase2.a（只读调用，未改任何仓库文件），逐字输出见 §3.1。

---

## 3. 实现事实（命令 + 逐字输出 + 文件:行，与 §1/§2 逐条对照）

### 3.1 生产库探针的原始输出（run/PROJECT-GOVERNANCE-01/R-2/logs/probe_r2.log，逐字，节选完整语义行）

```text
[REJ n=2 same-sign-free    ]   kernel rc=0 status=UNDERDETERMINED accepted=2 rej_low=0 rej_high=0 reasons=[UNDERDETERMINED,UNDERDETERMINED]
[REJ n=4 all-rej probe     ] n=4 resolve rc=0 method=percentile minimum_n=2 underdet_n=2
[REJ n=4 all-rej probe     ]   kernel rc=0 status=UNDERDETERMINED accepted=4 rej_low=0 rej_high=0 reasons=[UNDERDETERMINED,…]
[REJ n=4 normal            ]   kernel rc=0 status=OK accepted=1 rej_low=1 rej_high=2 reasons=[REJECTED_LOW,ACCEPTED,REJECTED_HIGH,REJECTED_HIGH]
[REJ n=5 odd               ]   kernel rc=0 status=OK accepted=1 rej_low=2 rej_high=2 reasons=[REJECTED_LOW,REJECTED_HIGH,REJECTED_LOW,REJECTED_HIGH,ACCEPTED]
[REJX n=5 percentile      ] n=5 method=percentile resolve=0 kernel=0 status=OK accepted=1 rej_low=2 rej_high=2
[REJX n=6 percentile      ] n=6 method=percentile resolve=0 kernel=0 status=ALL_REJECTED accepted=0 rej_low=3 rej_high=3
[REJX n=4 percentile      ] n=4 method=percentile resolve=0 kernel=0 status=UNDERDETERMINED accepted=4 rej_low=0 rej_high=0
[REJX n=8 percentile      ] n=8 method=percentile resolve=0 kernel=0 status=ALL_REJECTED accepted=0 rej_low=4 rej_high=4
[INT] values={10,20} weights={1,0} support={0.25,0.90}
[INT] rc=0 signal=10.000000 support=0.900000 n_used=1 n_accepted=2 n_finite=2 n_positive_weight=1 status=0
[INT] header:17/SCI:63/:75 canonical = max(accepted support) = 0.900000 ; SCI INTEGRATION.md:58 = max over {valid,W>0} = 0.250000
[UPMW] production (use_ivar_weight=1) control_ivar=4.0 snr=200 support=0.9 -> rc=0 raw_w=2.000000
[UPMW] 同一 obs 仅改 snr=1/support=0.01/unc=999 -> rc=0 raw_w=2.000000 (UPMW-001/003 snr/support 不变性)
[UPMW] control_ivar=0 (ivar 缺失) -> rc=2 raw_w=-1.000000 (rc=2 = 显式科学错误, 无 support 回退)
[UPMW] legacy ablation (use_ivar_weight=0) -> rc=0 raw_w=2.505008e-09 (snr^2/(1+snr^2)/unc^2 路径)
[UPMW] normalized_weights rc=0 control_ivar={4,1,0.25,100} -> {0.038005,0.009501,0.002375,0.950119} sum=1.000000
[KCORR] lookup(pixfrac=0.8, scale) 生产真帧尺度 0.9586"/px : 1.392500
[KCORR] lookup(0.8, provenance 实写值 0.009856"/px)        : 1.392500
[KCORR] lookup(0.8, 300" 表下界)                          : 1.392500
[KCORR] lookup(0.8, 450" 表内)                            : 2.144800
[KCORR] lookup(0.8, 600" 表上界)                          : 2.897100
[KCORR] lookup(0.8, 1e6" 越界)                            : 2.897100
[CVAR] k=1.4 sigma=10 N=251 dom+run -> rc=0 var=0.876141 ivar=1.141368 (baseline (pi/2)*100/251=0.625815)
[CVAR] k=0.9 (<1, N_eff>N_retained) dom+run -> rc=0 var=0.563234 ivar=1.775462  [rc=0 => k<1 被接受]
[CVAR] k=0.9 无 calibration_run_id -> rc=3 (3=域外推拒绝)
[CVAR] k=1.0 -> rc=2 (2=k_corr==1.0 拒)
[SENT] p2_upm_evaluate_c(model=nullptr) = 0.000000
[SHARE] rc=0 cell10(ivar=1e-4) sum=1.000000 ; cell20(ivar=1e4) sum=1.000000 => 单元总权相等（=control_reliability），跨 control 精度被丢弃
[SHARE] control_reliability=0.5 -> sum(cell10)=0.500000 sum(cell20)=0.500000 （geom 是配置常数，不是按覆盖度算出的几何量）
[INVAR] civ=1.0      build rc=0 hash=531b9763dad85fc9 calibrate rc=0 out[0..2]=8.007968127 10.000000000 10.000000000
[INVAR] civ=1e6      build rc=0 hash=531b9763dad85fc9 calibrate rc=0 out[0..2]=8.007968127 10.000000000 10.000000000
[INVAR] civ=1e-3     build rc=0 hash=531b9763dad85fc9 calibrate rc=0 out[0..2]=8.007968127 10.000000000 10.000000000
[INVAR] hash(civ=1.0)==hash(civ=1e6) ? YES(bitwise) ; hash(civ=1.0)==hash(civ=1e-3) ? YES(bitwise)
[MC] req=percentile -> method=percentile  n=3/n=4/n=5/n=7 : 0/200000 随机栈全拒 (0.0000%)
[MC] req=percentile -> method=percentile  n=6 : 147827/200000 随机栈全拒 (73.9135%)
[MC] req=percentile -> method=percentile  n=8 : 140289/200000 随机栈全拒 (70.1445%)
（req=auto/sigma/minmax 全组合、n=3..8 亦为 0/200000）
```

### 3.2 D-7：weight_mode 的真实分支号与生产权重

lib/algorithms/coverage/src/stage2_common.cpp:372-390（逐字）：

```cpp
        const std::string wm =
            in.value("weight_mode", std::string("auto"));
        // 默认 ivar (逆方差)
        // w = 1/variance_p (Drizzle 传播); support 只作 validity/coverage。
        // support_x_snr2 保留为 legacy/diagnostic (SNR-008 语义退休)。
        if (wm == "auto" || wm == "ivar") {
            cfg->weight_mode = 2;   // 默认 ivar
        } else if (wm == "equal") {
            cfg->weight_mode = 1;
        } else if (wm == "support_x_snr2") {
            cfg->weight_mode = 0;   // legacy (仅 ablation/诊断)
        } else {
            *err = "weight_mode 只支持 auto/ivar/equal/support_x_snr2";
            return false;
        }
        // ivar 产品整体缺失时，
        // 默认 → 显式 science/degraded 错误；仅显式允许时才降级。
        cfg->legacy_allow_weight_fallback =
            in.value("legacy_allow_weight_fallback", false);
```

lib/algorithms/coverage/tools/stage2.cpp:1106-1145（逐字，mode 2 / 0 / 1 三分支）：

```cpp
                if (cfg.weight_mode == 2) {
                    for (std::uint32_t s = 0; s < n_valid; ++s) {
                        const std::uint32_t orig = src_idx[s];
                        if (orig < depth && ivar_valid[orig]) {
                            const double iv = (double)ivarv[
                                (std::size_t)orig * chunk_pixels + i];
                            weights[s] = iv;
                            ++tl_local_ivar_used;
                        } else if (orig < depth && ivr[frames[orig]] != nullptr) {
                            // M4-C-03: 帧本应有 ivar 产品（ivr 已打开）但本 tile
                            // 读失败——禁止静默用 support（无量纲）冒充 ivar。
                            if (!cfg.legacy_allow_weight_fallback) { fail = 2; return; }
                            ++ivar_tile_fallback_px; weights[s] = support_v[s];
                        } else {
                            weights[s] = support_v[s];
                        }
                    }
                } else if (cfg.weight_mode == 0) {
                    …
                        weights[s] = support_v[s] * snr_v * snr_v;
                    }
                } else {
                    std::fill(weights.begin(), weights.end(), 1.0);
                }
```

lib/algorithms/coverage/tools/stage2.cpp:565-575（产品级 fail-closed，逐字）：

```cpp
    if (ivar_product_missing > 0 && cfg.weight_mode == 2) {
        if (!cfg.legacy_allow_weight_fallback) {
            log("weight_policy=ivar 且 ivar 产品缺失 " +
                std::to_string(ivar_product_missing) + " 帧 → 显式科学错误 "
                "(legacy_allow_weight_fallback=false)；拒绝继续，防止在 "
                "非逆方差语义下冒充 ivar coadd");
            …
            return 7;
        }
```

ACR 边界（stage2_common.cpp:440-444）：「只有 ACR 已注册、显式 sigma、非大尺度、**非逐像素 ivar** 时可进入 ACR 块」`cfg.weight_mode != 2`；acr_kernels.cpp:70「weight_mode scalar (offset 7); 缺省 0=legacy support×snr²」。

**与 §1 对照**：
- mode 2 = 逆方差 ivar（1/ADU²）⇒ 合 DESIGN §4.3（SNR→逆方差，不是直接用 SNR 加权）、SCI-UPM §5:54、UNCERTAINTY §51（「weight_mode=2，无 fallback」）、DATA-UPM-CONTROL-UNC-001（ivar 缺失=显式科学错误）。**实现默认即 SCI 口径。**
- mode 0 = support×snr² ⇒ 正是 DESIGN §4.3「不是直接用 SNR 加权」与 SCI-UPM §5:54 所禁；CONTROL_WEIGHT_SNR:66 把它的分支号写成 2 **是勘误**。
- ALG :125-127 的 ivar_valid?ivar:support 只在 legacy_allow_weight_fallback=true（默认 false）时可达，且被 SCI-UPM §5:54 / DESIGN §4.4「coverage 不作权重」禁止 ⇒ **文档缺陷**（把非生产降级路径写成 mode 2 的定义）。

### 3.3 D-8：两级流水线 + 份额式的真实性质

upm.cpp:1294-1326（F1 绝对式，逐字）：「// production UPM 观测 raw weight（单一实现，build 内部复用）… `const double qf = quality_factor(obs->quality_flags, cfg.quality_mode);` … `*out_raw = qf * civ;`」；quality_factor（upm.cpp:177-184）「if (flags == 0u) return 0.5;  // 未知 → 中性偏低」（故探针 `raw_w=2.0 = 0.5×4.0`，非缺陷）。

upm.cpp:1328-1347（F2 份额式）：

```cpp
        sums[obs[i].control_id] += raw[i];
    …
        out_norm[i] = (s > 1e-12) ? raw[i] / s * rel : 0.0;
```

upm.cpp:555-561（**求解器实际消费的就是份额式**）：

```cpp
        for (std::uint64_t i = 0; i < n_obs; ++i) {
            const std::size_t ck = m->control_by_id[obs[i].control_id];
            if (sums[ck] > 1e-12)
                raw_w[i] = raw_w[i] / sums[ck] * m->controls[ck].reliability;
            else
                raw_w[i] = 0.0;
        }
```

- m->controls[ck].reliability **只来自配置常数**：upm.cpp:282-283「`m->controls.back().reliability = std::max(0.0, cfg.control_reliability);`」，默认 1.0（upm.cpp:234）；生产装配 p2_session.cpp:194 / module_adapters.cpp:3741 / stage2_common.cpp:460 均写 1.0 ⇒ **SCI §2:21 的「几何可靠性（单 control 覆盖度）」在实现中是常数，不是按覆盖度算出的几何量**（新事实，D-8 的伴生缺陷）。
- 探针 [SHARE]/[INVAR] 证明：两个 control_ivar 相差 8 个数量级的 control cell 得到**相同的单元总权 1.0**；control_ivar 绝对尺度（1 / 1e6 / 1e-3）给出**按位相同**的 model_hash 与标定输出 ⇒ **绝对量 w_UPM（ADU⁻²）根本不进入当前生产解**；D-8 的「绝对 vs 份额」在默认配置下是**语义/产品面之争，不是数值之争**（数值差异只在 λs>0 且跨 cell 精度悬殊时显现，见 E1）。

### 3.4 D-9：分支号勘误（实现侧逐字）

- stage2_common.cpp:381-382：`} else if (wm == "support_x_snr2") { cfg->weight_mode = 0;   // legacy (仅 ablation/诊断)`
- stage2.cpp:1123-1141：`} else if (cfg.weight_mode == 0) { … weights[s] = support_v[s] * snr_v * snr_v;`
- acr_kernels.cpp:70：`// weight_mode scalar (offset 7); 缺省 0=legacy support×snr²`
⇒ CONTROL_WEIGHT_SNR.md:66 的 `weight_mode=2` 必须改为 `weight_mode=0`（纯勘误，无需科学判断）。

### 3.5 D-10：k_corr 查表、域、k≥1

- sampler.cpp:86-110：表 `k[2][3]={{1.2112,1.3925,1.4980},{2.3958,2.8971,3.2035}}`，`sc_grid[2]={300.0,600.0}`，`const double sc = std::clamp(scale_arcsec, 300.0, 600.0);`。
- sampler.cpp:548-556：`frames[i].kcorr = (sc > 0.0) ? kcorr_lookup(pf, sc) : kcorr_lookup(pf, 300.0);   // scale 未知：300" 档保守`。
- sampler.cpp:840-842：`const double kcorr_f = (frames[frame_id].kcorr > 0.0) ? frames[frame_id].kcorr : cfg.control_k_corr; const double cvar = kcorr_f * kPiHalf * sigma * sigma / n_ret;`。
- 标定域来源（lib/algorithms/drizzle/healpix_drizzle/tests/kcorr_matrix_test.cpp，**存在的源文件**）：:6-7「input/output sampling ratio 2 档（像素角尺度 300"/px 与 600"/px）」；:75「`const double scales[] = {300.0, 600.0};   // "/px（2 档采样比）`」；:80「`const double deg_per_px = sc / 3600.0;`」⇒ **轴是输入帧像素角尺度**（且用 cd[0] 构造无旋转 WCS）。
- 生产域：真帧 0.9586″/px（P1-001 W2-N-08 复算）；写入端 hp_drizzle_api.cpp:1153-1154「`meta.fits_meta["src_pixel_scale_arcsec"] = std::to_string(std::fabs(img.wcs.cd[0]) * 3600.0);`」在旋转下为 0.009856″，偏差 −98.97% ⇒ 无论修不修生产者，都 <300″ ⇒ clamp。
- 域界**有门**：lib/algorithms/coverage/tests/kcorr_lookup_test.cpp:60-65 `TEST(kcorr, domain_clamp)` 断言 `lookup(0.8,100.0)==lookup(0.8,300.0)`（即**把静默饱和固化为合同**）；`ctest -R phase2_sampler` **8/8 PASS**（含 4 个 kcorr 门）。
- k<1 未被拒：探针 [CVAR] k=0.9 … rc=0（upm.h:335-337 的 fail-closed 只覆盖 k_corr 非有限或 <=0、k_corr==1.0、缺 calibration_run_id）；而 PHASE2_UPM §6:71 要求「k_corr≥1 表征」、MC 源文件 control_median_mc_test.cpp:9 亦写「k_corr >= 1 表征 Drizzle 输出像素协方差导致的 N_eff < N_retained」。
- M7-A-115（UNCERTAINTY §32 把 k_corr 写成 N_eff）：与同文件 :43-44「k_corr=1.3883，N_eff≈181/251」「冻结 1.4」及 PHASE2_UPM §5:53「N_eff = N_retained / k_corr」对照 ⇒ 括注应为 k_corr = N_retained/N_eff，**纯勘误**。

### 3.6 M4-A-01：N≤4 全拒→全接受 分支

rejection.cpp:1853-1875（逐字）：

```cpp
    // 容错：小栈全拒回退为 UNDERDETERMINED（不 hard fail）
    // 根因：wbpp_2_9_1 percentile (low 0.2 / high 0.1, scale=|median|)
    // 在 N=4 时阈值过严可导致全 rejected（tile 116446 N_B=4），而
    // n=2 走 underdetermined_n 白名单绕过；该状态按 SCIENCE_FREEZE
    // 仍 hard fail。仅调容错路径、阈值冻结不变：N<=4 且全拒时降级为
    // 全接受 UNDERDETERMINED（保留中位数/放宽阈值的等价可继续语义），
    // 保证 32 帧 mosaic 可落盘且不破坏 2 帧语义。
    if (accepted_count == 0) {
        if (n <= 4) { … out->status = P2_STATUS_UNDERDETERMINED; }
        else { out->status = P2_STATUS_ALL_REJECTED; }
```

- 与 §1(D) 对照：SCI 只白名单 n≤2（:33/:62/:79/:86），「全拒→ALL_REJECTED」（:88）无 n 例外 ⇒ **代码单方面扩大白名单到 4**（代码注释自认「该状态按 SCIENCE_FREEZE 仍 hard fail」）。
- 可达域的**精化（本报告新结论）**：reject_percentile_impl（rejection.cpp:1599-1611）的接受带 = [median−0.2|median|, median+0.1|median|] 且 scale=|orig_median|；奇数 n 的中位数**必为样本本身** ⇒ 必被接受 ⇒ 全拒**只可能发生在偶数 n**；n≤2 已被白名单截走 ⇒ **生产 auto 路由下该分支的可达域恰为 n=4**（实测：n=3/5/7 共 3×200k 随机栈全拒 0 次；n=6/8 显式 percentile 全拒 73.9%/70.1%）。
- 根因（非「tile 偶发」）：|median| 在近零天光像素上使带宽塌缩 ⇒ **n=6/8 显式 percentile 的随机高斯栈 70–74% 全拒**；auto 路由把 n≥6 交给 winsorized（实测 0/200k 全拒）才掩盖了它。
- **「静默」不成立的部分**：状态码 P2_STATUS_UNDERDETERMINED 与计数 underdetermined_px（stage2.cpp:608/:1014/:1213/:1515/:1528）**已落 diagnostics**（stage2.cpp:1758「`diag["underdetermined_pixels"] = underdetermined_px.load();`」）⇒ 缺陷是「**SCI 未登记 + 无测试**」，不是「无计数」。
- 测试覆盖：tests/unit/v6_p2_rej/p2_rej_v6_test.cpp:648「// N10: 小样本 n<=2 -> UNDERDETERMINED 全接受 recall=0 显式」；全树无 n=3/4/5 的「全拒降级」用例（探针是本次新增的**外部**复现）。

### 3.7 M4-A-02：support reducer

integrate.cpp:44-56（**已修**，逐字）：

```cpp
        // B2-A7: canonical reducer 契约 = max(**accepted** support)，作用域是
        // 通过资格门 (accepted ∧ value/支持 finite) 的全部样本，**不含**权重
        // 正性要求（w==0 合法但不贡献 signal）。故 sup_max 必须在权重分支
        // 之前更新；旧实现置于 w==0 continue 之后会让零权 accepted 样本的
        // support 被静默丢弃，低估 coverage 并集（integrate.h:17-19 冻结语义）。
        if (in->support != nullptr)
            sup_max = std::max(sup_max, in->support[i]);  // canonical reducer
        …
            if (w == 0.0) continue;  // 零权重=合法但不贡献（ZERO_VALID_WEIGHT）
```

- 回归门在册且在跑：tests/unit/p2_output_semantics_test.cpp:85-107（4b/4c，注释「B2-A7 回归门」）＋ `ctest -R p2_output_semantics` **Passed**。
- 探针实测 support=0.900000（= max(accepted)），证明 ALG §5:113/:170-171「实现现状=SCI §5:58 表述」**已过期**；DISP-P2INT-001/002 两条登记同样是过期登记（`git log -S 'B2-A7'` 命中 eaf32aad 目录迁移提交）。
- 余下唯一不一致：SCI INTEGRATION.md:58 的 max_{valid,W>0} vs :21/:63/:75 + integrate.h:17 的 max(accepted)。

### 3.8 M4-F-01：点名测试的真实状态（**前提改判**）

```text
$ ls -l lib/algorithms/drizzle/healpix_drizzle/tests/{control_median_mc_test,kcorr_matrix_test}.cpp
-rw-r--r-- 1 dsh dsh 8594  9月16日 18:00 …/control_median_mc_test.cpp
-rw-r--r-- 1 dsh dsh 6295  8月25日 21:52 …/kcorr_matrix_test.cpp
$ git ls-files lib/algorithms/drizzle/healpix_drizzle/tests/ | wc -l   → 45（两文件均 tracked）
$ grep -rn -e control_median_mc_test -e kcorr_matrix_test --include=CMakeLists.txt --include=*.cmake . | grep -v '^./run/' | grep -v '^./build/'   → 0 命中（EXIT=0）
$ grep -rn -e control_median_mc_test -e kcorr_matrix_test ci/ .github/                                  → 0 命中（EXIT=0）
$ ctest --test-dir build -N | tail -1                                                                   → Total Tests: 414（无二者）
```

⇒ **「全树零源文件」不成立**；成立的是 tracked-but-unbuilt（构建孤儿）。SCI PHASE2_UPM.md:105/:147、UPM_SOLVER.md:98、PHASE2_SAMPLER.md:308 仍以现在时把 control_median_mc_test 当「已过」证据 ⇒ 门应打在**证据链**上而不是文件存在性上。另：kcorr_lookup_test.cpp（在册、`phase2_sampler.kcorr.*` 4 门 PASS）**已覆盖 F2 角点/域 clamp**，即 M9-A-2 建议的「补域界回归测试」**部分已落地**（但落地的是「把 clamp 固化」，见 §4）。

### 3.9 M7-H-101 / M7-H-103 / M8-C-002

- **M7-H-101**：upm.cpp:603 `for (int iter = 0; iter < cfg.max_iterations; ++iter)`，:861-873「`m->iterations = iter + 1; … if (max_dM < cfg.tolerance && max_dC < cfg.tolerance) break;`」；:965-966 把 iterations/objective 写入模型 JSON，但 P2ModelInfo（upm.h:60-68）**只有 control/observation/component/model_hash**，且 p2_upm_build 恒返回 0 ⇒ **API 消费者无法观测「迭代耗尽」**（SCI/ALG 的 rc 表亦无此码）。
- **M7-H-103**：upm.cpp:1274-1292「`if (model == nullptr) return 0.0;` … `if (it == m->frame_index.end()) return std::numeric_limits<double>::quiet_NaN();`」；upm.cpp:163「`if (it == m->cell_index.end()) return 0.0;`」；而 0.0 是 gauge 参考帧的**合法 C 值**（upm.cpp:792/:831「该分量参考帧 gauge：C=0」）⇒ 三处 fail-open 哨兵与合法值不可区分。ALG 自己已写死矛盾（PHASE2_UPM_IMPL.md:309）。
- **M8-C-002**：upm.cpp:516「定义 determinism class D1(**worker 数无关, 同一 worker 数下位精确**)」；synthetic_gate.cpp:277-318：1T/2T 标定 `EXPECT_NEAR(out1[0], out2[0], 1e-6) << "1T/2T calibrate 差"`（:303，其注释「标定输出按容差一致（1T/2T 浮点合流末位漂移允许）」），重复 2T `EXPECT_NEAR(out3[0], out2[0], 1e-12)`（:318）＋ model_hash exact（:314-315）。实现侧归约按**连续块划分 + 每 k/每帧不相交写**（upm.cpp:655-717 M 更新、:771-858 C 更新，全程无共享累加器/无 atomic 浮点）⇒ **跨 worker 数按位相等是构造保证**，1e-6 门弱于合同（ALG §6 :156-160 对 integration 层写的正是「1..N 线程 bitwise 一致」）。另 phase2_sampler_parallel.OneTvsTwoTDeterminism 在 Linux **Skipped**（sampler_parallel_consistency_test.cpp:34「`GTEST_SKIP() << "真实 HiPS 输入不存在"`」），ctest 汇总「The following tests did not run: 315 …(Skipped)」。

---

## 4. ⭐ 门禁自审（这条门/判据本身是否正当）

| 门/判据 | ①权威依据 | ②可测量可复现 | ③是否把实现缺陷误判成文档错误（或反之） | ④阈值/量测域 | ⑤误报/漏报 | **门应为** |
|---|---|---|---|---|---|---|
| **D-7 门（三套互斥权威）** | **正当但归因错**：链① DESIGN §4.3/§4.4 + SCI-UPM §5:54 + UNCERTAINTY §51 三方一致 | 可（探针 [UPMW]/[INT] 逐字） | **误报**：真缺陷在文档（CW:66 分支号 + ALG:125-127 措辞），实现**已正确**（默认 fail-closed） | 已定义：mode∈{0 legacy,1 equal,2 ivar}；legacy_allow_weight_fallback 默认 false | 漏报点：ALG 文本可能被后人照抄成生产语义 | **门应为「生产权重只允许 mode 2（ivar）；support/SNR/coverage 不得作权重；ivar 缺失 fail-closed」**，并把 ALG:125-127 改成「**仅 legacy_allow_weight_fallback=true 时的降级路径**」 |
| **D-8 门（§5:46 vs §5:47 互斥）** | 正当（两式确实不同量） | 可（[SHARE]/[INVAR] + E1） | **半误报**：二者是同流水线两级（ALG F1/F2），不是互斥定义；**但**实现把 geometric_reliability 实现成**配置常数**（upm.cpp:282-283），这是**真实实现缺陷**（SCI §2:21 承诺「单 control 覆盖度」） | 缺：§3:29 只给了一个 w_UPM：ADU⁻²，未给份额式的单位/归一目标 | 漏报点：绝对尺度不进解 ⇒ 只看数值会认为「无差异」 | **门应为「w_UPM(绝对, ADU⁻²) 与 w_cell(份额, Σ=geom) 必须分别命名、分别给单位；geom 必须来自覆盖度而非配置常数」** |
| **D-9 门（分支号）** | 正当（实现侧唯一） | 可（stage2_common.cpp:377-385 逐字） | 判定正确（纯勘误） | 无歧义 | 误报风险 0 | **门应为「文档分支号 = 实现分支号」**（CW:66 2→0） |
| **D-10 门（k_corr 域/k≥1）** | **正当且更严重**：§6:71 要 k≥1、标定 TU 只标定 300–600″ | 可（[KCORR]/[CVAR] + E3） | **漏判**：门只说「不相交」，实际还有 ①生产者 abs(cd[0])*3600 的 −98.97% ②「300 档保守」方向说反（300 档给**最小** k=1.2112–1.4980）③k<1 未被拒 | **域写清楚了但后果没写**；domain_clamp 门把静默饱和**固化成合同** | 漏报：kcorr_lookup_test 全绿会让人以为域已处理 | **门应为「k_corr ≥ 1 强制（越界显式拒）；标定域外的 scale 不得 clamp，必须显式拒/回退默认并在 diagnostics 标注；生产域恒 <300″ 的后果必须写入 ALG」** |
| **M4-A-01 门（N≤4 容错）** | **正当但域写错**：SCI 只白名单 n≤2；代码到 4 | 可（探针 n=2/4/5/6/8 + 60 万次 MC） | 判定正确（实现越权），但「零测试/静默」两处**需修正**（状态码+计数已落 diagnostics；可达域恰为 n=4） | 域未定义清楚（N≤4 中 3/5 不可达） | 漏报：只看 SCI 文本会以为 n=3 也可能全拒 | **门应为「n=4（偶数且 ≤4）全拒 → UNDERDETERMINED 全接受，登记进 SCI §4/§5/§7/§8 + 计数 + n=3/4/5/6 边界测试；n≥5 全拒仍 ALL_REJECTED」** |
| **M4-A-02 门（support reducer）** | 正当（SCI 自相矛盾） | 可（探针 [INT] + 在册回归门） | **误报（已过期）**：实现已按 header 修好，ALG 的「实现现状=§5:58」与 DISP-P2INT-001/002 是**过期登记** | 无阈值问题 | 漏报：照抄 ALG §5:113 会把已修的代码当成待修 | **门应为「canonical = max(accepted support)（integrate.h:17）；SCI §5:58 一处订正；ALG 三个过期登记清零」** |
| **M4-F-01 门（点名测试）** | 正当（SCI §11/§15 承诺可执行证据） | 可（ls/git ls-files/grep/ctest -N 四条） | **前提错**（「零源文件」不成立）⇒ 门应改判为「未注册」 | 无阈值 | 误报：把「未注册」写成「不存在」会误导整改（去建已存在的文件） | **门应为「SCI/追溯行点名的测试必须同时满足：文件存在 ∧ 在某 CMake/ctest 面注册 ∧ 在 CI 采集；否则该行状态必须显式 MISSING」** |
| **M7-H-103 门（哨兵）** | 正当（ALG:309 自相矛盾 + DATA invalid=NaN 约定） | 可（探针 [SENT]） | 判定正确 | 哨兵值未定义清楚 | 漏报：0.0 也可被读成「合法无校正」 | **门应为「不可用哨兵 = NaN（与未知 frame 一致），禁用与合法值冲突的 0.0」** |
| **M7-H-101 门（未收敛）** | 部分（SCI/ALG 无此条款 ⇒ 属**合同缺口**） | 可（读码 + 需新增 API 探针） | 判定需收窄（「三口径」中 UPM 侧是「记录但不暴露」） | 容差 1e-6 的量纲未声明（max_dM/max_dC 是 ADU 绝对量） | 漏报：模型 JSON 里有 iterations，可能被认为「已可观测」 | **门应为「迭代耗尽必须可由公共 API 观测（只读访问器 + diagnostics），并在 ALG §4 声明 rc/字段语义」** |
| **M8-C-002 门（worker 无关性）** | 正当（ALG §6 位精确合同） | 可（`ctest -R Phase2UpmParallel` Passed） | 判定正确（判据弱于合同） | **阈值未定义清楚**：合同说「位精确」、门用 1e-6 | 漏报：1e-6 无法发现会破坏归约顺序的改动 | **门应为「跨 worker 数 = 位精确（或 1e-12）；同配置重复 = bitwise + model_hash exact（已在册）；skipped 的 sampler 并行门须在 CI 显式记录或补 fixture」** |

**门禁自审的三条总纲**：
1. **本线 6 条「科学定义冲突」里，只有 D-10(k≥1) 与 M4-A-01 的实现确实越过了 SCI；D-7/M4-A-02 的实现是对的、错在文档；M4-F-01 的前提（零源文件）不成立；D-8 是「命名/范围」问题 + 一个真实的 geom 常数化缺陷。**
2. **所有门都缺「后果声明」**：域界/适用范围写了，但「生产域落在域外 ⇒ 实际取哪一档 ⇒ 对产品有什么影响」没有写。D-10 是典型（域写了、clamp 也测了，但没人写「生产恒取 300 档、k_corr 因此与帧无关」）。
3. **本线多处「过期登记」**（M4-A-02 的 DISP-P2INT-001/002、ALG §5:113 的行号、sampler.cpp:672 锚漂移、lib/phase2/** 旧路径）说明**门禁缺「登记随实现漂移」的检测**——这正是 R-6 的题域，本报告只给实例。

---

## 5. 建议裁决（唯一推荐 + 置信度 + 反方论证 + 最小改动路径 + 影响面）

### D-7 weight_mode=2 唯一语义 —— **置信度：高**

**唯一推荐**：**weight_mode=2 ≡ 马赛克积分的逐样本 ivar 逆方差权重（w_i = ivar_i，单位 信号⁻²），无任何 fallback；ivar 缺失 = 显式科学错误（产品级 rc=7 / 像素级 fail=2，默认路径已如此）。**
- 权威序：链① DESIGN §4.3（「SNR → 逆方差权重」「不是直接用 SNR 加权」）+ §4.4（「coverage 不作权重」）＞ SCI-UPM §5:54 ＞ 冻结 DATA UNCERTAINTY §51（「weight_mode=2，无 fallback」）＞ SCI-CW §4（其自身已把 local_snr 降级为「相对质量权重场」，且 §7 禁止与 ivar 混用）。
- support×snr² = **mode 0 legacy/ablation**，且被链①与 SCI-UPM §5:54 双重禁止进生产。
- ALG PHASE2_INTEGRATION.md:125-127 的 ivar_valid?ivar:support 仅在 legacy_allow_weight_fallback=true（默认 false，且 UNCERTAINTY §67-68 规定此时「不写 variance/ivar 产品」）时可达。

**反方论证（最可能被反驳的点）**：①「legacy 分支也是 SCI-CW 冻结条款，改它等于改 FROZEN SCI」——回应：本次不删分支，只**改分支号与语义标注**（CW:66 2→0，并注明「非生产」），CW 的 quality_weight 定义与 §7 禁令保持不动；②「ivar 缺失就报错会让真实数据跑不出产品」——回应：这是**有意的 fail-closed 科学选择**，且已有显式开关与 manifest uncertainty_available=false 通道；③「support 回退在 ivar 缺失时至少比等权好」——回应：E2(d) 证明其相对权重可任意漂移 3 个数量级，且违反链①。

**最小改动路径（文档，零代码）**：
1. docs/science/CONTROL_WEIGHT_SNR.md:66：weight_mode=2 → weight_mode=0（legacy/诊断；生产禁用，见 SCI-UPM-WEIGHT-001 §5:54 与 DESIGN §4.3/§4.4）；:71 保留但加「非生产」标注。
2. docs/algorithms/PHASE2_INTEGRATION.md:125-129：改写为「mode 2 = ivar（缺失即失败）；ivar_valid?ivar:support 仅在 legacy_allow_weight_fallback=true 的显式降级路径出现（默认 false）」。
3. registry/claim：docs/TRACEABILITY.csv 中 SCI-UPM-WEIGHT-001 / SCI-CW-001 行的 1 处描述同步；CW 的 P5-SNR 注记已存在，无需新增 claim（未改科学定义，仅纠分支号与措辞）。

**影响面**：stage2.cpp（无改动）、CONTROL_WEIGHT_SNR.md、PHASE2_INTEGRATION.md、docs/GLOSSARY.md:13（frame_quality_weight 条目已写「support×snr_v²(SCI-CW 域专用)」，需加「非生产」）、M7-A-125 可同批销。

### D-8 w_UPM 绝对量 vs 份额 —— **置信度：高（实现事实）/ 中高（统计结论）**

**唯一推荐（两段并存 + 分别命名 + 冻结范围）**：
- **w_UPM（绝对，ADU⁻²）** = quality_factor × geometric_reliability × control_ivar（§5:46/§3:29）——**科学定义**，用于任何需要绝对精度的消费面（API、门、报告）；
- **w_cell（份额，无量纲，Σ_cell = geometric_reliability）** = w_UPM / Σ_cell w_UPM × geom（§5:47）——**求解器内归一约定**，实现消费的就是它（upm.cpp:555-561、:1344）；
- 冻结三条性质（进 §7 不变量）：**(i)** 同一 control 内两观测的权比 = w_UPM 之比（两式一致，实测 {4,1,0.25,100}→{0.038,0.0095,0.0024,0.950}）；**(ii)** 任何**公共**精度因子（含 k_corr）在份额式内严格消去（实测 hash 位相同）；**(iii)** 份额式**丢弃跨 control 精度**（单元总权恒 = geom），在 λs>0 时改变估计量 ⇒ §10 增列「不得在未同步换算 λs/λ0 的前提下改用绝对权」。
- **同时登记真实缺陷**：geometric_reliability 必须是**按 control 覆盖度算出的量**，现在只是配置常数 1.0（upm.cpp:282-283；生产三处装配均写 1.0）。**推荐**把 §2:21/§5:46 的 geometric_reliability 改标为 control_reliability（配置常量，默认 1.0）并登记变更 claim（不改码、不改科学数值）；备选是按 SCI 实现覆盖度型 geom（改码）。

**反方论证**：①「既然绝对尺度不进解，为什么不干脆删掉绝对式？」——回应：API p2_upm_raw_weight、UPMW-001/002/003 门与 p2_upm_control_variance 都需要绝对量，且 UNCERTAINTY §63 明确「control_variance 只进 w_UPM」；②「份额式劣化 2 倍，为什么还推荐保留？」——回应：改成绝对权必须同步重标定 λs/λ0（否则等于**隐式改科学**，违 ENG_SPEC §3）；E1(A) 行显示「只换权不换 λ」会同时改变正则强度（coadd 1.173 vs 1.159），**不是「更正确」而是「另一个模型」**。若负责人愿意承担重标定，正确目标态是「绝对逆方差权 + 精度相对化的 λ」（E1(B) 行：C 精度最优 0.588）。

**最小改动路径**：PHASE2_UPM.md §5:47 改名为 w_cell 并标注「求解器内、无量纲、Σ_cell=geom」；§3:29 增一行单位；§7 增 (i)(ii)(iii) 三条不变量；§10 增「未同步换算 λ 不得改用绝对权」；UPM_SOLVER.md F1/F2 同步；PHASE2_UPM_IMPL.md 增「求解器消费 w_cell（upm.cpp:558）」锚。**是否改 claim**：属「同一式的命名澄清 + 生效范围」，建议登记为 DOC-CLARIFY（非科学变更）；geometric_reliability 改标需 1 条变更 claim。

**影响面**：DATA_SEMANTICS.md:919/:1048（引用了 §5 的 w_UPM 式）、API_REFERENCE.md:120（raw_w=quality*control_ivar 文字）、THREADING_MODEL.md:20（归一化描述）、docs/modules/phase2*.md、DATA_ARTIFACTS.md:116（k_corr 行）、UPMW-001..007 门（语义不变，无需改）。

### D-9 分支号 —— **置信度：高**

**唯一推荐**：CONTROL_WEIGHT_SNR.md:66 的 weight_mode=2 → **weight_mode=0**（纯勘误）。**反方**：无（实现/ALG/DATA 三方一致为 0）。**最小改动**：1 行；同步 stage2_common.cpp:376 注释「support_x_snr2 保留为 legacy/diagnostic」即可（已是 0）。**影响面**：M3-A-002 可销；无代码/测试变更。

### D-10 k_corr —— **置信度：高**

**唯一推荐**：
1. **强制 k_corr ≥ 1**（p2_upm_control_variance 与 sampler.cpp 的 per-frame 链）：k<1 ⇒ 显式错误（新增 rc，或复用 rc=1 语义并写清）；依据 S4 + §6:71 + MC 源文件 :9（实测 k(drop→0)=0.987→1）。
2. **删除/退役 scale 维**：标定域 [300,600]″ 与生产 0.9586″（以及生产者实写的 0.009856″）不相交 ⇒ 该维在生产**无判别力**；保留 clamp 等于把「恒定取 300 档」写成合同。**推荐**：kcorr_lookup 改为**域外显式回退冻结默认 1.4 并在 diagnostics 打标**（或直接删表只用 1.4），并把 `K_CORR_DOMAIN 选项 B（逐帧标定）`标 RETIRED；kcorr_lookup_test.domain_clamp 相应改为「域外 → 默认 + 标记」。
3. **生产者缺陷另案**：abs(cd[0])*3600 → sqrt(|det|)*3600（P1-001 W2-N-08 已在册）；本线只要求「其结果不得再影响权重」（第 2 条落地后即自动满足）。
4. **文档补后果声明**：PHASE2_SAMPLER.md §5.4 增「当前生产帧 src_pixel_scale_arcsec < 300″ ⇒ 表维恒饱和于 300 档；该维在生产无效」；PHASE2_UPM.md:35/:71 的 kcorr>0 改 1 ≤ kcorr ≤ k_max（k_max 由表/文献给，暂取 10）并写「越界拒」。**M7-A-115 括注**：k_corr=N_eff → k_corr=N_retained/N_eff（纯勘误）。

**反方论证**：①「删 scale 维会丢掉『按帧标定』能力」——回应：该能力从未生效（域外恒饱和），保留它只是**把无效当有效**；重标定可在生产域重跑 kcorr_matrix_test（方法学已在，只需换 scales），但**必须先在册**（见 M4-F-01）；②「k≥1 是新增约束，会不会拒掉合法表值」——回应：表值全 ≥1.21（sampler.cpp:92-95），且 k<1 违反 N_eff≤N 的定义。

**影响面**：sampler.cpp:89-110/:548-556、upm.cpp:2408-2415（p2_upm_control_variance 的域检查）、kcorr_lookup_test.cpp、phase2_sampler.* 4 门、DATA_ARTIFACTS.md:116、PHASE2_SAMPLER.md / PHASE2_UPM.md / UNCERTAINTY_AND_COVARIANCE.md:32；M9-A-2 / M7-A-112/113/115/205 可一并销。

### M4-A-01 n≤4 全拒→全接受 —— **置信度：中高**

**唯一推荐**：**保留容错分支，但把它登记进 SCI 并写死域 —— n = 4 ∧ 全拒 ⇒ UNDERDETERMINED 全接受（n≥5 全拒仍 ALL_REJECTED），同时补边界测试与 diagnostics 字段名。** 理由：①SCI 自身的小样本哲学就是「不做猜测、全接受」（§5:62/§7:79），n=4 同样无法稳健估尺度；②删分支会让 n=4 的 32 帧马赛克出现空洞（SCI §8 的 ALL_REJECTED 在 integration 层不产 signal）；③实测可达域恰为 n=4（可证伪、可测）；④不登记就违 ENG_SPEC §3 的「临场启发式静默改方法」。
**同时必须登记根因**（否则容错会掩盖真实缺陷）：percentile 的 scale=|median| 在近零天光像素上导致带宽塌缩（n=6/8 显式 percentile 随机栈全拒率 70–74%）；建议在 SCI §8 增一行退化说明，并**另立缺陷**（是否改用 max(|median|, MAD) 属 WBPP 对齐面，需单独裁决）。

**反方论证**：①「n=4 全接受会把宇宙线放进产品」——回应：n=4 的 percentile 判据本身不可靠（带宽塌缩），此时「全接受」与「全拒」都不是真值；下游还有 UPM 的 Huber 权与 large_scale 生长（默认关）；**且现状已如此**，本次只是把它写进权威而非改行为；②「应删码恢复 ALL_REJECTED」——回应：会引入产品空洞，且需同时改 stage2/integration 的缺像素语义（更大改动面）。
**最小改动**：SCI REJECTION §4/§5/§7/§8 各 1 行 + 1 条 §8 表行；测试 tests/unit/ 增 n=3/4/5/6 四例（n=4 全拒→UNDERDETERMINED+accepted=n+rejected_*=0；n=3/5 → OK；n=6 显式 percentile 全拒→ALL_REJECTED）。**影响面**：rejection.cpp（无改动）、SCI REJECTION、PHASE2_REJECTION.md §5 F14/§10.9 锚表、stage2.cpp:1758 diagnostics（已有）。

### M4-A-02 support reducer —— **置信度：高**

**唯一推荐**：**canonical = max(accepted support)（integrate.h:17 / SCI §2:21/§5:63/§7:75）；订正 docs/science/INTEGRATION.md:58 一处，并把 ALG 的过期登记清零；实现零改动。** 依据：四份在册文本中三份 + 冻结合同头文件都写 accepted，且 PHASE2_INTEGRATION.md:211-212 已把 header 文本冻结为唯一口径；同时「support 与权重解耦」正是 DESIGN §4.4「coverage 不作权重」的同类红线。
**反方论证**：①「§5:58 是 FROZEN SCI，不能改」——回应：本次是「**消除 SCI 内部自相矛盾**」，需 1 条 DOC 变更 claim；且不改会让 flux=signal×support×A_cell 在「零权 accepted 样本」上少算面积；②「现行实现已 accepted，改文档无收益」——回应：ALG §5:113/:170-171/:266-268 与 DISP-P2INT-001/002 的过期文本会误导整改（M4-A-02 的处置建议就是基于过期事实）。
**最小改动**：SCI :58 表达式 + ALG :113（删「现状位置」标注）、:166-184（改为「已按 B2-A7 对齐，DISP 关闭」）、:199-206 表（:202 零权行）、:266-268。**影响面**：p2_output_semantics_test.cpp（已在测 accepted，无需改）、INTEGRATION_ALGORITHMS.md:19-21（若仍写 W>0 需同步）。

### M4-F-01 点名测试 —— **置信度：高**

**唯一推荐（改判门 + 二选一的任务分派）**：①**门改判**为「点名测试必须文件存在 ∧ 构建注册 ∧ CI 采集」三合一；②对 control_median_mc_test / kcorr_matrix_test 二选一：**(a) 收进构建**（lib/algorithms/coverage/tests/ 已有 GTest 族与 kcorr_lookup_test 先例；两 TU 目前是 main() 手编程序，需包一层 GTest 或 add_test 直接跑二进制），或 **(b) 把 PHASE2_UPM.md:105/:147、UPM_SOLVER.md:98、PHASE2_SAMPLER.md:308 与 TRACEABILITY.csv 对应行的状态显式改为 MISSING**。推荐 **(a)+临时(b)**：先按 (b) 把「已过」改成「未注册（MISSING）」，再派一个把 MC 收进 ctest 的任务（MC 的 2000 次实现须限时，可降为 200 次 + 容差放宽并显式登记）。
**反方论证**：「文件存在就够了，门是误报」——回应：SCI §11/§15 的语义是「Oracle **全过**」，这要求可执行证据；ctest -N 414 项里没有它们，故「全过」不可复现。**影响面**：docs/TRACEABILITY.csv:3,4,8,58-64（UPMW 行引用了 control_median_mc_test.cpp 作为证据列）、M4-F-05（MC 判据 [0.98,2.0] 过宽）、M8-F-013、M2a-F-1（同族）。

### M7-H-103 哨兵 —— **置信度：高**

**唯一推荐**：**不可用一律 NaN**：p2_upm_evaluate_c(nullptr,…) 返回 NaN（与未知 frame_id 一致），evaluate_c_field::at() 的缺失 cell 亦返回 NaN；并在 PHASE2_UPM_IMPL.md:309 表把两行统一为 NaN、在 §10 增「哨兵条款」。调用方（p2_upm_calibrate_block）需按 DATA 约定把 NaN 作为「invalid 输出面」处理（UNCERTAINTY §65-66 已有 NaN 同态约定）。
**反方论证**：①「NaN 会污染产品」——回应：DATA 合同本来就是 invalid→NaN，禁用 0.0 才是防污染（0.0=合法校正值）；②「null model 是编程错误，调用方不该传」——回应：公共 C ABI 必须定义，ALG 表已定义（且定义为不一致的值）。**最小改动**：upm.cpp:1276 / :163 两行 + ALG 表 1 行 + 1 个单测（synthetic_gate 内）。**影响面**：PHASE2_UPM_IMPL.md:80/:309、任何 isnan 下游（需 grep 确认无 ==0.0 判等）。

### M7-H-101 未收敛 —— **置信度：中**

**唯一推荐**：**把「迭代耗尽」冻结为可观测状态**：ALG UPM_SOLVER.md §4 增一行 rc/字段语义；新增只读访问器 `int p2_upm_convergence(const void* model, std::uint64_t* out_iterations, double* out_objective, int* out_converged)`（**不改** P2ModelInfo 冻结布局），并要求 stage2 diagnostics 与 p2_session manifest 落该字段；收敛判据改为**相对式**（max_dM/scale < tol，scale 取 max|M| 或中位 sigma_eff）并在 §12 声明比较量单位。
**反方论证**：①「模型 JSON 已有 iterations，够用」——回应：C API 消费者拿不到（P2ModelInfo 无此字段），且 rc 恒 0；②「新增 API 是架构变更」——回应：纯增量导出，不动 C ABI 结构体，符合 ENG_SPEC §4。**最小改动**：upm.cpp 增访问器（~10 行）+ ALG 1 节 + 1 测试（max_iterations=1 → converged=0）。**影响面**：PUBLIC_API.md / API_CONTRACTS.csv 增 1 行、PHASE2_SESSION.md 若引用 rc 表。

### M8-C-002 worker 无关性 —— **置信度：中高**

**唯一推荐**：**门与合同取齐到「位精确」**：跨 worker 数（1..N）用 EXPECT_EQ（或 1e-12 + model_hash exact）；把 upm.cpp:516 的 D1 注释改述为三个可测口径（**同配置重复 = bitwise + hash exact**（已在册 :314-315）；**跨 worker 数 = bitwise**（构造保证：连续块 + 不相交写 + 无共享累加器）；**跨后端 = 不允许**），并把三档写进 docs/science/PHASE2_UPM.md §9/§7 作为冻结容差来源。另：phase2_sampler_parallel.OneTvsTwoTDeterminism 的 SKIP 必须在 CI 记录（显式 SKIP 白名单）或补 fixture，否则 sampler 层「并行=串行」无门。
**反方论证**：①「1e-6 更稳健，避免脆测试」——回应：合同已承诺位精确，且实现按构造保证；用 1e-6 是**门弱于合同**（漏报域：任何引入 1e-9 级重结合顺序变化的回归都会静默通过）；②「跨 worker 位精确会被未来的负载均衡破坏」——回应：那正是需要让门报警的场景（ENG_SPEC §3 要求「顺序变化时先冻结容差并登记」）。**最小改动**：synthetic_gate.cpp:303 容差 1e-6→1e-12（或 EXPECT_EQ）+ upm.cpp:514-517 注释 + PHASE2_UPM §9 一行。**影响面**：phase2_synthetic_gate.Phase2UpmParallel.OneTvsTwoTDetermine、M8-G-001（SKIP 记录）、THREADING_MODEL.md:20。

---

## 6. 证据清单（全部命令、退出码、产物路径）

**产物根**：run/PROJECT-GOVERNANCE-01/R-2/（probe_r2.cpp、probe_r2、exp_r2.py、logs/probe_r2.log、logs/exp_r2.log）。本报告为唯一交付物：reports/PROJECT-GOVERNANCE-01/research/R-2_phase2权重与UPM语义.md。**零 git 写**（全程只读 git ls-files / git log），**零仓库文件改动**（只写 run/ 与 reports/）。

| # | 命令（可复现） | 退出码 | 产物/关键输出 |
|---|---|---|---|
| C1 | `ls -l lib/algorithms/drizzle/healpix_drizzle/tests/{control_median_mc_test,kcorr_matrix_test}.cpp` | 0 | 8594 B / 6295 B **存在** |
| C2 | `grep -rn -e control_median_mc_test -e kcorr_matrix_test --include=CMakeLists.txt --include=*.cmake .`（滤 run/、build/） | 0（0 命中） | 未注册 |
| C3 | `grep -rn -e control_median_mc_test -e kcorr_matrix_test ci/ .github/` | 0（0 命中） | CI 未采集 |
| C4 | `git ls-files lib/algorithms/drizzle/healpix_drizzle/tests/ | wc -l` | 0 | 45（tracked） |
| C5 | `ctest --test-dir build -N` | 0 | Total Tests: 414；无 MC 两目标 |
| C6 | `g++ -O2 -std=c++17 -Ilib/algorithms/coverage/include -Ithird_party -o run/PROJECT-GOVERNANCE-01/R-2/probe_r2 run/PROJECT-GOVERNANCE-01/R-2/probe_r2.cpp build/libastrocs_phase2.a build/libastrocs_hips.a build/libastrocs_aio.a build/libastrocs_common.a build/libastrocs_cpu.a build/libastrocs_cfitsio.a -lpthread -lm -lz` | 0 | probe_r2（2.5 MB） |
| C7 | `cd run/PROJECT-GOVERNANCE-01/R-2 && ./probe_r2 > logs/probe_r2.log` | 0 | §3.1 全部逐字输出（REJ/REJX/INT/UPMW/KCORR/CVAR/SENT/SHARE/INVAR/MC） |
| C8 | `cd run/PROJECT-GOVERNANCE-01/R-2 && python3 exp_r2.py > logs/exp_r2.log` | 0 | E1/E2/E3 全部逐字输出与 JSON（§2.2） |
| C9 | `ctest --test-dir build -R 'p2_output_semantics|phase2_sampler|phase2_ivar_wiring' --output-on-failure` | 0 | 8/8（OneTvsTwoTDeterminism **Skipped**） |
| C10 | `ctest --test-dir build -R 'phase2_synthetic_gate.Phase2UpmParallel.OneTvsTwoTDetermine'` | 0 | 1/1 Passed（0.07 s） |
| C11 | `git --no-optional-locks log --oneline -S 'B2-A7' -- lib/algorithms/coverage/src/integrate.cpp` | 0 | eaf32aad refactor(lib): ARCH-001 目录等价迁移…（B2-A7 随之入树） |
| C12 | python3 抽取 问题扫描/账本/FIX_LEDGER.jsonl 中 M3-A-002/M4-A-01/M4-A-02/M4-F-01/M7-A-112/113/115/125/205/M7-H-101/103/M8-C-002/M9-A-2 的原判据 | 0 | 用于逐条对照（**未采信其 fix_state**） |

**复现预算**：C6 ~8 s；C7 ~12 s（含 6 组 20 万次 MC）；C8 ~15 s（E1 10.4 s / E2 3 s / E3 ~2 s）；C9+C10 ~5 s。全部 ≤120 s/条，无网络依赖。

**已知局限（诚实声明）**：
1. E1 是**同构 mini-UPM**（L2 而非 Huber、4×4 网格、单一公共天光面），不是生产二进制的重跑；它回答「权重方案对估计量的影响」，不替代端到端验收；
2. E3 是**独立简化 drizzle 模型**（top-hat drop、面积加权、8×8 patch），其 300″/600″ 档 k 值（1.2966/2.0173）与产品自带表（1.3925/2.8971）同量级但不相等，故只用于判「域不可迁移」与「k≥1」，不用于替换表值；
3. 生产 UPM 的 λs 默认值取自 stage2_common.cpp:132-141（`smoothing: "auto" → 0.1`），未在真帧端到端运行中复核该键的实际取值；
4. M7-H-101 未做运行期探针（需新增 API），结论基于读码 + 持久化字段实测。

---

**（本报告结束）** 研究线 R-2 · 唯一交付物 · 零 git 写 · 零仓库文件改动 · 全部结论给出唯一推荐而非「需负责人裁决」。








