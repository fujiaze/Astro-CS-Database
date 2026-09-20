# RELEASE-02 FIX-P2b 报告：Phase2b 方差传播 + 权重链

> 执行面：**Phase2b（方差传播 + 权重链）**。零 git 写；未跑 ninja/cmake/ctest；
> 只用 g++ -fsyntax-only + 自建 Oracle（生产尺度）。证据目录 run/RELEASE-02/fix-p2b/。
> 执行者：FIX-P2b（SubAgent）。日期：2026-09-19。

---

## 0 结论摘要（先给答案）

| 子项 | 结论 | 证据 |
|---|---|---|
| P2b-1 归一化产出逐像素方差 | **已实现**（新模块 variance_propagation + p2_op_upm_apply 落盘 p2_corrected_var_f<fid>.bin）。形式 = **残差制造者 PΣPᵀ**（P=I−H），不是 σ²+Var(ĝ)；含可选参数项 J_out C_θ J_outᵀ 与乘性 ÷g²。 | Oracle T1/T2/T3；realdata_evidence.txt |
| P2b-2 权重链消费归一化方差 | **已接线**：p2_op_integrate priority 1 = 逐像素 w=1/Var(corrected)；priority 2 = 帧级 w=SNR²/F_ref²·g_k²（FrameWeightInput 增可空 gain + require_frame_gain fail-closed）；priority 3 = ivar。 | Oracle T4/T5 |
| P2b-3 修 ivar/snr=0 stub | **已修**（sampler.cpp）：帧 ivar 缺失时写 1/uncertainty²（=control_ivar），snr 写点 SNR |value|/uncertainty。真实数据实测原为 0/277234。 | realdata_evidence.txt §P2b-3 |
| P2b-4 F_ref 组内公共 | 写侧核心**已由既有提交 4f341b15 修复**（module_adapters.cpp:3180-3282 两遍法/显式配置单一公共 F0）；本轮**新增** drizzle sink 写侧闸门（astro_sphere_sink.cpp:365-400）。L4 数据是**旧产物**（早于该提交），故仍逐帧不一致。 | realdata_evidence.txt §P2b-4 |
| P2b-5 过渡期不撒谎 | **保持诚实**：p2_corrected.json 的 uncertainty_available = variance_available ∧ pixel_noise_included ∧ param_covariance_included；当前生产 **false**（L4 输入帧无 variance 产品、W2 无 C_θ API），integrate 不据此声称逐像素逆方差。 | module_adapters.cpp:5603-5610, 5637-5652 |
| 权重序是否修好 | **修好**：现行 w=1/σ_raw² 序 **A>C>B>D**；正确 w=1/Var(corrected)（含 ÷g²）序 **A>B>C>D**，Kendall τ=0.667，A/B 比 6.76→1.69。 | Oracle T3 |

---

## 1 改动 file:line

### 1.1 新增模块（P2b-1 科学核心）

| 文件 | 行 | 内容 |
|---|---|---|
| lib/algorithms/integration/v6/include/astrocs/v6/variance_propagation.h | 1-88 | 残差制造者方差接口（HatRow / residual_maker_variance / naive_variance / normalized_weight_hat_row / corrected_pixel_variance / weight_from_variance） |
| lib/algorithms/integration/v6/src/variance_propagation.cpp | 1-222 | 实现；全部退化路径 fail-closed |
| lib/algorithms/integration/v6/CMakeLists.txt | 29 | 注册到 astrocs_v6_phase2_integrate |
| CMakeLists.txt | 835-836 | 注册到 astrocs_module_adapters（生产节点 target） |

### 1.2 权重链（P2b-2 接口）

| 文件 | 行 | 内容 |
|---|---|---|
| lib/algorithms/integration/v6/include/astrocs/v6/weight_chain.h | 118-123 | 新增 weight_from_corrected_variance |
| 同上 | 133-141 | FrameWeightInput 增可空 const double* gain |
| 同上 | 157-158 | 新 closure token kUnclosedMissingGain=11 / kUnclosedInvalidGain=12 |
| 同上 | 163-168 | WeightChainPolicy::require_frame_gain（默认 false，向后兼容） |
| 同上 | 181 | WeightChainResult::frame_gain（审计） |
| lib/algorithms/integration/v6/src/weight_chain.cpp | 74-75 | token 映射 |
| 同上 | 100-113 | weight_from_corrected_variance 实现 |
| 同上 | 309, 371-398 | w = SNR²/F_ref²·g_k²；缺 gain 声明要求 → fail-closed |
| 同上 | 423 | 等权基线补 frame_gain |

### 1.3 生产节点（lib/infrastructure/scheduler/src/module_adapters.cpp）

> **行号说明**：P1/P2a 正在同一文件并发编辑，下列行号在本次会话内已漂移 ±2；请以
> grep 锚点为准：`p2b_load_control_var`、`variance_include_self`、`corr_var_ready`、
> `corrected_variance_used`、`p2_corrected_var_f`。

| 行 | 内容 |
|---|---|
| 78-80 | include variance_propagation.h |
| 5089-5196 | P2bControlVar + p2b_load_control_var（控制级残差制造者方差表） |
| 5301-5318 | P2FrameOut 方差字段；读 p2_samples.json 建表；variance_include_self 旋钮 |
| 5375-5395 | 逐帧 p2_corrected_var_f<fid>.bin 打开 + 帧 Phase1 variance 句柄 |
| 5487-5545 | 逐 leaf 最近 control → 残差制造者方差（+ 可选逐像素噪声） |
| 5556-5570 | 写 var tile；置 var_ok / pixel_noise_included |
| 5589-5610 | frames[] 增 var_file / variance_available / pixel_noise_included / n_var_pixels；聚合标记 |
| 5603-5610 | param_cov_included=false；uncertainty_available 诚实计算（P2b-5） |
| 5637-5664 | p2_corrected.json 方差字段 + manifest |
| 6264-6280 | integrate：读 corrected 方差面可用性 → corr_var_ready |
| 6303-6310 | priority 1 生效；ivar/SNR 分支仅在 !corr_var_ready 时跑 |
| 6338-6350 | 帧级链 gain 配对（require_gain 来自 multiplicative_gain_applied） |
| 6561 | need_ivar 排除 corr_var_ready |
| 6627-6644 | 逐 tile 读 corrected 方差 tile |
| 6708-6725 | 权重 priority 1：w=1/Var(corrected)（fail-closed） |
| 6866, 6897 | corrected_variance_used 入 artifact/manifest |

### 1.4 采样写侧（P2b-3）

| 文件 | 行 | 内容 |
|---|---|---|
| lib/algorithms/coverage/src/sampler.cpp | 1087-1097 | snr：无局部 catalogue 时写点 SNR |value|/uncertainty（不再 0） |
| 同上 | 1101-1116 | ivar：帧 ivar 缺失时写 1/uncertainty²（=control_ivar，不再 0） |

### 1.5 F_ref 组内公共写侧闸门（P2b-4）

| 文件 | 行 | 内容 |
|---|---|---|
| lib/algorithms/drizzle/healpix_drizzle/astro_sphere_sink.cpp | 365-400 | 读 p1_snr.json 时校验 snr_reference_scope=="group" 且逐帧 flux_adu 组内公共（rtol 1e-9）；违反 → 不写帧级 SNR 键（fail-closed） |
| 同上 | 428-431 | 写键条件追加 group_fref_ok |

---

## 2 方差形式（残差制造者）与推导

### 2.1 形式

归一化把观测 y 映射到被扣除的校正场 ĝ = H y，输出

```
corrected = y − ĝ = (I − H) y = P y          （P = 残差制造者）
```

线性化下（Σ = Cov(y)）：

```
Var(corrected) = P Σ Pᵀ + J_out C_θ^ind J_outᵀ
对角（Σ=diag(σ²)）:
Var(corrected_i) = Σ_j P_ij² σ_j²  +  param_i
```

**错误形式** σ_i² + Var(ĝ_i) = σ_i² + Σ_j H_ij²σ_j² 展开为 Σ + HΣHᵀ，漏掉交叉项
−HΣ − ΣHᵀ。最简例 ĝ=ȳ（N 帧等权均值，H_ij=1/N）：

```
正确:  (1−1/N)²σ² + (N−1)/N²·σ² = σ²(1−1/N)
朴素:  σ² + N/N²·σ²             = σ²(1+1/N)
N=8:   朴素/正确 = (1+1/8)/(1−1/8) = 1.2857  （报告口径 1.29×）
```

Oracle T1 在生产尺度（σ=4.22759e10，σ²=1.78725e21，N=8/49）逐位复算该闭式，并断言
朴素值与正确值可判别。真实 L4 控制级数据（42241 个 control-frame）实测 include-self 档
朴素/正确 中位 **1.2591**、最大 **31.99**（realdata_evidence.txt）。

### 2.2 生产落地（p2_op_upm_apply）

- H 由 p2_samples.json 的**控制级加权耦合**构造：ĝ = Σ_j w_j y_j / W，w_j = control_ivar_j。
- 默认 include_self=false（**(c) 排除自身**，与 P2a 重构口径一致，不假设参考帧）：
  H_kk=0 ⇒ Var = σ_k² + Σ_{j≠k}(w_j/W_{-k})²σ_j²，闭式 = 1/w_k + 1/W_{-k}（Oracle T2 验证）。
- variance_include_self=true 可切到 W2 含自身口径（此时 Var = 1/w_k − 1/W，朴素式高估最显著）。
- **逐 leaf** 用 tile 内 8×8 控制格的最近 control（cell_side=64，Voronoi 边界与控制中心一致）
  插值，得到逐像素方差面。
- **参数项**：param_var 作为独立入参（当前 0；W2 无 C_θ API）。当参数协方差由同一数据诱导时，
  它已包含在 PΣPᵀ 内；独立先验/共享系统项才另加，避免重复计数。
- **乘性项**：Var(corrected)=[PΣPᵀ+param]/g²；g 非法即 fail-closed。

### 2.3 诚实边界（P2b-5）

L4 输入帧 products=['signal','support']、n_variance_tiles=0，且 W2 无 C_θ API：
- pixel_noise_included=false、param_covariance_included=false；
- uncertainty_available = variance_available ∧ pixel_noise_included ∧ param_covariance_included = false；
- integrate 的 priority 1 **不激活**，不声称逐像素逆方差加权（退回既有 ivar/SNR 链或等权）。

---

## 3 权重序验证（能红能绿）

Oracle：run/RELEASE-02/fix-p2b/variance_oracle.cpp（链接 variance_propagation.cpp +
weight_chain.cpp，无第三方依赖；g++ -O2）。

```
== FIX-P2b variance Oracle (production scale sigma=4.22759e+10, N=49) ==
  [PASS] T1 N=8 correct == sigma^2(1-1/N) [GREEN]
  [PASS] T1 N=8 naive   == sigma^2(1+1/N) [RED]
  [PASS] T1 N=8 naive overestimates 1.2857x
  [PASS] T1 N=49 correct == sigma^2(1-1/N) [GREEN]
  [PASS] T2 exclude-self Var == 1/w_k + 1/W_-k
  [PASS] T3 现行 w=1/sigma_raw^2 序 = A>C>B>D
  [PASS] T3 正确 w=1/Var(corrected) 序 = A>B>C>D [GREEN]
  [PASS] T3 Kendall tau = 0.667
  [PASS] T3 A/B ratio: raw 6.76 vs correct 1.69
  [PASS] T4 w == SNR^2/F_ref^2 * g_k^2 for all frames
  [PASS] T5 require_gain + missing gain -> kUnclosedMissingGain
  [PASS] T5 invalid gain -> kUnclosedInvalidGain
  ... pass=35 fail=0
```

| 帧 | g | σ_raw | w=1/σ_raw²（现行） | w=1/Var(corrected)（正确） |
|---|---|---|---|---|
| A | 1.0 | 1.0 | **1.000** | **1.000** |
| B | 2.0 | 2.6 | 0.1479 | **0.5917** |
| C | 0.8 | 2.0 | 0.2500 | 0.1600 |
| D | 1.0 | 5.0 | 0.0400 | 0.0400 |

- 现行序 **A > C > B > D**；正确序 **A > B > C > D**；Kendall τ = **0.667**（翻转）。
- A/B 比 **6.76（现行） vs 1.69（正确）**——现行把 g=2 高响应帧压低 **≈4×=1/g²**。
- 另跑 lib/algorithms/integration/v6/oracle/weight_chain_selfcheck：**47 passed / 0 failed**
  （证明 gain 接口向后兼容，旧调用点行为不变）。

**能红证明**（故障注入）：P2B_ORACLE_FAULT=naive 用朴素值冒充"被测正确实现"，
正确闭式断言必红、进程 rc=1（oracle_output_red.txt，red_fail_count=2）。

```
--- GREEN run (expect rc=0) ---   green_rc=0
--- RED run P2B_ORACLE_FAULT=naive (expect rc=1) --- red_fail_count=2 red_rc=1
ORACLE_REDGREEN_OK
```

真实 L4 控制级（生产尺度，42241 个 control-frame）：

```
include-self  naive/correct  median=1.2591  min=1.0016 max=31.9870
exclude-self ((c)) Var/σ_k²   median=1.1296  min=1.0008 max=16.4935
```

---

## 4 与权重链接口（lib/algorithms/integration/v6/weight_chain.*）

优先级（p2_op_integrate）：

```
priority 1: 逐像素 w_k(p) = 1/Var(corrected_k(p))        # 有 corrected 方差面且完整传播时
priority 2: 帧级   w_k    = SNR_k²/F_ref² · g_k²          # ivar/var 缺失时的回退
            （SNR_k 与 F_ref 必须同源、F_ref 组内公共；gain 声明要求而缺 → fail-closed）
priority 3: 逐样本 ivar（既有）
mode 1:     等权（uncertainty_available=false，非 ivar 语义面）
```

- weight_from_corrected_variance(var)：1/var，var<=0/非有限 → fail-closed；
  **禁止**由权重标量反推 variance（upm.h:228）。
- FrameWeightInput.gain（可空）：nullptr 按 g=1（仅 require_frame_gain=false 合法）；
  WeightChainPolicy.require_frame_gain=true 时缺 gain → kUnclosedMissingGain。
- integrate 由 p2_corrected.json 的 multiplicative_gain_applied 决定是否 require_frame_gain；
  生产方案 B（加性-only, g≡1）不置该键 ⇒ 行为与旧口径逐位一致。
- 配对性定理：SNR_f=a_f·F_ref/σ_f ⇒ SNR_f²/F_ref²=a_f²/σ_f²=w_f，成立当且仅当分子分母同源、
  F_ref 组内公共（闸门保持 fail-closed，不放宽）。

---

## 5 科学行为变更清单（须前台记入 ACCEPTANCE §3）

1. **P2b-3-a**：p2_samples.json 的 observations[].snr 语义由"仅局部 catalogue SNR（无则 0）"
   改为"局部 catalogue SNR，缺失时为该控制点自身点 SNR |value|/uncertainty"。snr_available
   语义不变（仍如实标记 catalogue 可用性）。该字段不参与 science 权重（use_ivar_weight=1 时
   p2_upm_raw_weight 只用 control_ivar），故不改变 UPM 解。
2. **P2b-3-b**：observations[].ivar 由"恒 0（stub）"改为"帧 ivar 产品的单 leaf 值；缺失时为
   1/uncertainty² = control_ivar（控制估计器方差）"。该字段被 upm.h:40-42 标为弃用/仅诊断、
   不参与 science 权重，故不改变 UPM 解；仅使下游方差面/审计可读。
3. **P2b-4**：drizzle sink 写侧新增闸门——p1_snr.json 逐帧 flux_adu 非组内公共（或
   snr_reference_scope != "group"）时**不写** ASTROCS_FRAME_SNR/ASTROCS_REFERENCE_FLUX。
   旧数据（逐帧 F_ref）将从"写出不一致键"变为"不写键"（Phase2 权重链 fail-closed，安全方向）。
   **不改变**已一致的合法数据（当前生产写侧输出）行为。
4. **P2b-1/2 新增产物与字段**（不改变现有 corrected 值）：
   - 新文件 p2_corrected_var_f<fid>.bin（fp64，逐 leaf 方差，NaN=无效）；
   - p2_corrected.json 增 variance_available/variance_model/variance_reason/pixel_noise_included/
     param_covariance_included/n_var_pixels_total/uncertainty_available/frames[].var_file 等；
   - p2_integrated.json 增 corrected_variance_used。
   - 新增节点配置旋钮 variance_include_self（默认 false = (c) 排除自身）。
   **过渡期 uncertainty_available 恒 false**（L4 无逐像素噪声、W2 无 C_θ），priority 1 不激活
   ⇒ 生产权重与旧口径一致，无隐性行为变更。
5. **未改**任何冻结科学公式、默认容差、SCI/ALG 定义；未改 docs/**。

---

## 6 与 P1 / P2a 的接口约定

### 6.1 与 P2a（接缝 formulation）——**同一文件并发编辑，必须协调**

- **观测到的事实**：本分片工作期间，lib/algorithms/coverage/include/astro/phase2/upm.h、
  lib/algorithms/coverage/src/upm.cpp、lib/infrastructure/scheduler/src/module_adapters.cpp
  被 P2a **并发修改**（git status 显示 upm.h/upm.cpp 为 M；module_adapters 出现
  sub_c/sub_delta/additive_mode/gauge 等 P2a 新增代码）。本轮所有对 module_adapters.cpp 的
  改动改用**原子 Python 补丁**（读-改-写一次完成）以缩小竞争窗口。**前台提交前请核对本报告
  §1.3 的每一行仍在位**（尤其 apply/integrate 两个函数）。
- **接口约定（H 的通用形式，不假设参考帧）**：
  - P2a 的 (c) 排除自身 ⇔ 本模块 normalized_weight_hat_row(..., include_self=false)；
    W2 含自身 ⇔ include_self=true。二者同一实现，仅 H 行不同。
  - 若 P2a 后续把**公共场 gauge G** 作为"对所有帧相同的扣除"引入，G 是**确定性场**
    （不引入随机性），不进方差；其参数不确定度（若有）应通过 param_var 传入
    corrected_pixel_variance。
  - 若 P2a 让 UPM 输出 C_θ（JᵀWJ 逆），本模块的 param_var 入口即可接 J_out C_θ J_outᵀ；
    届时把 p2_op_upm_apply 的 param_cov_included 置真，uncertainty_available 才会随逐像素
    噪声一并转真。
- **未改动 P2a 的文件**（upm.h/upm.cpp 一行未碰）。

### 6.2 与 P1（Phase1 测光链）——**同一文件并发编辑**

- **观测到的事实**：module_adapters.cpp:90-96 出现 P1 新增的 photometry_apply.h/
  frame_photometry_fit.h include；P1 的 include 目录（lib/algorithms/calibration/src、
  lib/algorithms/photometry/cpp/src）尚未进 build.ninja（本轮只做 -fsyntax-only，额外补了 -I）。
  **前台构建前请确认 P1 已把这两个 include 目录加入 astrocs_module_adapters 的
  target_include_directories**。
- **接口约定（F_ref 组内公共，P2b-4）**：
  - 核心修复已由既有提交 4f341b15 落在 module_adapters.cpp（P1 SNR 节点）**3180-3282 行**：
    (a) 显式 snr.reference_flux_adu；(b) 两遍法块级中位数；所有帧共用同一
    sci_cfg.reference_flux_adu。**本分片未改这些行**。
  - 本分片新增的 sink 闸门（astro_sphere_sink.cpp:365-400）只做**防御性**校验，不改变 P1
    写侧逻辑；若 P1 也改 sink，请保留该闸门。
  - 若 P1 引入 I_photo = k_photo·I_cal 的**乘性**归一化，则 Phase2 帧级权重链必须以
    g_k = k_photo,k（或其倒数，按 corrected 定义）配对：w = SNR²/F_ref²·g_k²；接口已就绪
    （FrameWeightInput.gain + require_frame_gain），并请 P1 在 p2_corrected.json 写
    multiplicative_gain_applied 与逐帧 frame_gain。注意 F_ref 必须与定义 ASTROCS_FRAME_SNR 时
    **同一个**参考通量（配对性定理）。

---

## 7 诚实边界与未做

- **未跑** ninja/cmake/ctest（任务约束）；仅 g++ -fsyntax-only（module_adapters / sampler /
  astro_sphere_sink / weight_chain / phase2_integrate 全 0 错误）与自建 Oracle。
- **未做** P2a 的加性 formulation、P1 的测光施加；未改 docs/**；未碰 upm.h/upm.cpp。
- **生产方差面在 L4 上不激活**：L4 输入帧 n_variance_tiles=0 且 W2 无 C_θ API ⇒
  uncertainty_available=false（P2b-5 诚实标记），priority 1 不触发。要让逐像素逆方差真正生效，
  需要 (i) Phase1 写出 variance/ivar 产品，或 (ii) 让 UPM 暴露 C_θ；二者任一到位后本模块即可
  接通（代码路径已存在并 fail-closed）。
- **F_ref 组内公共**的"修好"目前是**代码面**；L4 数据是 2026-09-18 23:53 的旧产物（早于修复
  提交 01:42），仍逐帧不一致（6.873×、3 帧缺键）。重跑 normalize 后即应组内公共。
- p2_samples.json 的 snr 新语义（点 SNR）未同步更新 upm.h 的结构体注释，以避免与 P2a 并发
  编辑冲突；该字段仍为**弃用/仅诊断**，语义变更已在 §5 登记。
- Oracle 的 T3 用 H=∅（P=I）+ gain 路径验证权重序；生产控制级残差制造者方差由
  realdata_evidence.py 在真实 L4 数据上独立复算（闭式 1/w_k + 1/W_{-k}）。

---

## 8 证据文件

| 文件 | 内容 |
|---|---|
| run/RELEASE-02/fix-p2b/variance_oracle.cpp | 独立 Oracle（生产尺度，闭式对拍 + 故障注入） |
| run/RELEASE-02/fix-p2b/variance_oracle | 已编译 Oracle |
| run/RELEASE-02/fix-p2b/oracle_output.txt | 绿跑输出（35 pass / 0 fail） |
| run/RELEASE-02/fix-p2b/oracle_output_red.txt | 红跑输出（P2B_ORACLE_FAULT=naive，2 fail，rc=1） |
| run/RELEASE-02/fix-p2b/build_oracle.sh | Oracle 构建 + 红/绿双跑脚本 |
| run/RELEASE-02/fix-p2b/realdata_evidence.py / realdata_evidence.txt | 真实 L4 生产尺度证据（stub 统计 / 残差制造者比值 / F_ref 扫描） |
| run/RELEASE-02/fix-p2b/wc_selfcheck.txt | 既有 weight_chain_selfcheck（47 pass / 0 fail，向后兼容） |
| run/RELEASE-02/fix-p2b/syntax_check.sh / syntax_check2.sh | g++ -fsyntax-only 验证脚本 |
| run/RELEASE-02/fix-p2b/patch_integrate.py / patch_integrate2.py / patch_apply2.py | 原子补丁（并发编辑下的可复现改动） |
