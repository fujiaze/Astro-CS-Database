# Phase2 Integration Algorithms（P2-INT / astrocs.p2.integration）

> 上游：ASTROCS_DESIGN.md §5.3（SNR 重建与逆方差叠加）

> ID: ALG-P2-INT-001  状态: CONTRACT_READY
> 模块: lib/algorithms/coverage/src/integrate.cpp（81 行，astrocs_phase2 静态库成员，
> 根 CMakeLists.txt:336-346/:344）+ 唯一权威签名头
> lib/algorithms/coverage/include/astro/phase2/integrate.h（74 行）
> 权威: 本文档（算法级逐符号锚）。SCI 上游: SCI-INT-001
> （docs/science/INTEGRATION.md，FROZEN，集合
> SCI-INT-001/002/004/008，零改动）。DATA: DATA-P2-INT（DATA_SEMANTICS
> §21）。API: API-P2-INT-001（PUBLIC_API.md）。TEST: TEST-P2-INT-001
> （MISSING，P2-INT-TEST 落地；设计冻结面=本文档 §11.4）。
> 本文档为 Phase2 逐像素加权积分 reducer 的算法级权威（逐符号锚 +
> 冻结公式 + 状态语义 + 并行归约容差）；ALG-INT-001/002 ID 语义由
> 本文件 §12 映射承接。

## 1 目的与非目标

- **目的**：逐像素候选栈加权积分 reducer——`signal = Σ wᵢxᵢ / Σ wᵢ`
  （仅 eligible ∧ 正权重样本）+ support canonical reducer + 显式五态
  status。实现=reducer 无知策略（policy/reducer 分离冻结）。
- **非目标**：不决定排异/eligibility gather（SCI-REJ/P2-REJ）；不做
  权重策略（权重=外部 numeric weights，构造在 Stage2）；不输出
  variance/ivar 产品（ivar 为输入侧权重语义，DATA_SEMANTICS §4a/§21）；
  不做马赛克编排/逆归一化（Stage2 域）；无内部并行（像素级纯函数）。

## 2 符号与单位（权威=本表 + DATA_SEMANTICS §21）

| 符号 | 含义 | 单位/dtype | 锚 |
|---|---|---|---|
| `values[i]` | 候选样本科学值 | **面亮度 ADU·sr⁻¹**（同 UPM 校准后标度），f64 | integrate.h:37 |
| `weights[i]` | 数值权重（可空=等权 1.0） | **(ADU·sr⁻¹)⁻²**（仅数值域，语义策略在调用方），f64 | integrate.h:38 |
| `support[i]` | 覆盖支撑（可空=1.0） | 无量纲 [0,1]，f64 | integrate.h:39 |
| `accepted[i]` | 排异接受掩码（可空=全接受） | u8 | integrate.h:40 |
| `count` | 候选数 | 无量纲 u32 | integrate.h:41 |
| `signal` | 加权积分输出 | 面亮度 ADU·sr⁻¹，f64 | integrate.h:55 |
| `support`（输出） | canonical reducer 输出 | 无量纲 [0,1]，f64 | integrate.h:56 |
| `n_used` | 实际参与积分样本数 | 无量纲 u32 | integrate.h:57 |
| `wsum` | Σ wᵢ（eligible ∧ w>0） | (ADU·sr⁻¹)⁻² | integrate.cpp:59-60 |
| `vs` | Σ wᵢxᵢ | (ADU·sr⁻¹)⁻¹ | integrate.cpp:59-60 |
| `sup_max` | max(accepted support) —— **canonical 语义，已实现**（`integrate.cpp:49-50`，位于权重分支之前；§7/§11.3 闭环登记） | 无量纲 | integrate.cpp:49-50 |

权重语义（integrate.h:8-11 注释冻结）: 权重是 Phase2 按该天球像素
对应帧集合现场算出的派生量——逐样本 `ivar`（`1/ADU²`），由调用方构造
并先经 `p2_validate_candidate_weights`（stage2.cpp:1106-1140）；
`weights=null` → 等权 1.0。reducer 只消费权重数组本身，本层不编码
ivar/SNR 策略（SNR 只作 veto/质量门，不直接加权）。

## 3 逐符号锚（integrate.cpp 81 行 / integrate.h 74 行，实测）

| 符号/段 | 锚（integrate.cpp） | 语义 |
|---|---|---|
| p2_validate_candidate_weights | :10-17 | 预检：weights=null→0（合规）；任一 !finite 或 w<0→1（违规）；w==0 合法（不违规）。调用方: stage2.cpp:1141/:1402 |
| p2_integrate_pixel | :19-74 | 唯一生产入口（C ABI；integrate.h:58-65 声明） |
| ├ null 防御 | :20-21 | stack==null 或 result==null → rc=1 |
| ├ 空栈 | :22-26 | count==0 ∨ values==null → NO_CANDIDATES（rc=0） |
| ├ eligibility 循环 | :30-57 | 候选索引固定序 i=0..count-1 |
| │ ├ accepted 计数 | :34-36 | accepted 非空且为 0 → skip（不计 n_accepted） |
| │ ├ values 非 finite | :37 | → invalid_input |
| │ ├ support 门 | :38-42 | support 非空且（!finite 或 ≤0）→ invalid_input |
| │ ├ n_finite 计数 | :43 | ++n_finite |
| │ ├ 权重门 | :44-48 | w=weights?w[i]:1.0；!finite → invalid_input；w<0 → invalid_input |
| │ ├ 零权重 | :49 | w==0 → continue（合法零贡献；n_accepted/n_finite 已计入） |
| │ ├ sup_max 更新 | :49-50 | support 非空时 sup_max=max(sup_max,sup)——**位于权重分支（:51-57，含 :56 `w==0 continue`）之前**，作用域 = 通过资格门的全部 accepted 样本（canonical 语义，DISP-P2INT-001/002 已闭环，§7/§11.3） |
| │ ├ 累加 | :52-53 | vs+=w*x; wsum+=w（double 固定序） |
| │ └ n_used 计数 | :56 | ++n_used |
| ├ rc 同步 | :58-63 | invalid_input → INVALID_INPUT（rc=0，n_used=已计数部分） |
| ├ 无正权重分支 | :65-69 | n_positive_weight==0 → status = (n_accepted==0 ? ALL_REJECTED : ZERO_VALID_WEIGHT)——**wsum==0 不做除法** |
| ├ signal 计算 | :70 | signal = vs/wsum |
| ├ support 输出 | :71 | support = support ? sup_max : 1.0（空支撑守恒） |
| └ OK | :72 | status=OK，rc=0 |

| 头文件段 | 锚（integrate.h） | 语义 |
|---|---|---|
| 权重策略注释 | :8-11 | stack.support_x_snr2.v1 / stack.equal.v1 |
| support canonical 语义 | :17 | "output support 唯一 canonical reducer：max(accepted support)" |
| P2PixelStack | :36-42 | values/weights/support/accepted 可空语义 + count |
| P2IntegrateStatus | :45-51 | OK=0/NO_CANDIDATES=1/ALL_REJECTED=2/ZERO_VALID_WEIGHT=3/INVALID_INPUT=4 |
| P2PixelResult | :53-63 | signal/support/n_used/n_candidates/n_accepted/n_finite/n_positive_weight/status |
| 函数声明 | :58-66 | p2_integrate_pixel(:58-59) / p2_validate_candidate_weights(:62-66) |

## 4 状态机与返回码（五态显式、互斥、可达）

| status | 值 | 触发条件（精确） | 锚 | 冻结测试证据 |
|---|---|---|---|---|
| OK | 0 | ≥1 正权重 eligible 样本 | :72 | synthetic_gate:2630-2636（signal=10.75 门）；p2004 :69-126 |
| NO_CANDIDATES | 1 | count==0 ∨ values==null | :23-26 | synthetic_gate:4747-4750 |
| ALL_REJECTED | 2 | count>0 ∧ n_accepted==0 | :65-69 | synthetic_gate:2639-2643（status==2）；:4751-4756 |
| ZERO_VALID_WEIGHT | 3 | n_accepted>0 ∧ n_positive_weight==0（全零权重 accepted） | :65-69 | synthetic_gate:4746-4760（计数器 n_candidates=2/n_accepted=2/n_finite=2/n_positive_weight=0/n_used=0）；p2004 :93-101 |
| INVALID_INPUT | 4 | 任一 accepted 样本：values 非 finite ∨ support 非 finite/≤0 ∨ weights 非 finite/负 | :37/:38-42/:44-48 | synthetic_gate:4718-4738（NaN support）；:4690-4700（负权重）；:4723-4726 |

- rc（函数返回）与 status 正交：rc=1 仅 stack/result null（:20）；
  rc=0 时 status 载全部语义。**wsum==0 不做除法**（:65-69 分支先行），
  无除零/NaN 泄漏路径。
- 状态穷尽/互斥为 SCI §11 状态穷尽门冻结面；显式计数器
  （n_candidates/n_accepted/n_finite/n_positive_weight/n_used）供
  状态判据自证（synthetic_gate:4757-4760 冻结）。

## 5 逐公式定义（算法级，与 SCI §5 同构；单位见 §2）

```text
eligibility（逐候选 i，候选索引固定序）:
  n_accepted += accepted[i]!=0                                  :34-35
  if (!accepted[i]) continue                                    :36
  invalid ⇔ accepted[i]!=0 ∧ (
      !finite(values[i])                                        :37
      ∨ (support 非空 ∧ (!finite(support[i]) ∨ support[i]<=0))  :38-42
      ∨ (weights 非空 ∧ (!finite(weights[i]) ∨ weights[i]<0))   :52-56
    )
聚合（仅通过门的样本，i 固定序）:
  n_finite += 1                                                 :43
  sup_max = max(sup_max, support[i])  # canonical max(accepted) :44-50
  w_i = weights ? weights[i] : 1.0                              :51-53
  if (w_i == 0) continue            # 零权重合法零贡献           :56
  vs += w_i * values[i]; wsum += w_i; n_used += 1               :58-61
输出:
  invalid_input        → status=INVALID_INPUT（n_used=已计数）  :66-69
  n_positive_weight==0 → status = n_accepted==0
                          ? ALL_REJECTED : ZERO_VALID_WEIGHT    :70-74
  否则                 → signal = vs / wsum                     :75
                         support = support ? sup_max : 1.0      :76
                         status = OK                            :77
```

- 权重语义（调用方构造，本层无知）: **逐样本 `ivar`（1/ADU²），无任何
  fallback** —— 缺失 ivar 是显式科学错误
  （产品级 stage2.cpp:565-575 rc=7 / 像素级 stage2.cpp:1106-1122 fail=2），
  与 SCI-UPM §5:54、DATA-UNC-001 §51（「逆方差权重，无 fallback」）、
  DESIGN §3.1/§5.3 一致；权重是 Phase2 按该天球像素对应帧集合现场算出
  的派生量，SNR 只作 veto/质量门、不直接加权；
  等权分支/weights=null → 等权 1.0（stage2.cpp:1139）。
  `ivar_valid?ivar:support` 是降级键 `legacy_allow_weight_fallback=true`
  （默认 false）时的**显式降级路径**（stage2.cpp:1113-1120），
  **不是逐样本 ivar 的定义**；
  该降级发生时 DATA-UNC-001 §67-68 要求不写 variance/ivar 产品。
- 禁止（SCI §10 逐条承接，本层为合同）: support 改 mean/sum 二次
  聚合；w==0 改判 INVALID_INPUT；INVALID_INPUT 并入
  ZERO_VALID_WEIGHT；在本层引入 ivar/SNR 策略；改变求和顺序。

## 6 消费链与并行语义（parallel reduction tolerance 合同）

- **Stage2（马赛克编排，astrocs.p2.hips_writer 消费者）**:
  权重构造 :1106-1140 → p2_validate_candidate_weights 预检
  :1141/:1402 → p2_integrate_pixel 三调用点：chunk 并行路径
  :1213-1223、CPU 串行路径 :1515-1527（注释 :1525-1526 冻结
  "support 唯一 canonical reducer（max accepted support）由
  p2_integrate_pixel 计算，Stage2 只消费"——**调用方禁止二次
  max/mean**）、large_scale 二次积分 :1579-1585。逆归一化
  `area=support_out×A_cell`、`flux=signal×area`（:1227-1228/
  :1588-1589）在 Stage2 域（DATA_SEMANTICS §20.3）。
- **ACR 加速（acr_kernels.cpp）**: process_pixel :189-208
  （P2PixelStack :189-195、p2_integrate_pixel :196、失败清零
  :199-208 与 Stage2 `(status==0)?signal:0` 同型）；缓冲布局
  frame-major（:112/:158）。
- **并行边界（像素间并行、像素内串行）**: integrate.cpp 无任何
  线程原语——单像素栈归约按候选索引固定序，无跨 worker 浮点
  重结合。像素间并行发生在调用方: Stage2 `#pragma omp parallel
  num_threads(workers)` :1288（条件 `!large_scale_active &&
  effective_cpu_workers>1` :1280）+ `omp for schedule(static)`
  :1298；ACR :218 + :228 schedule(static)。per-thread 统计按
  thread id 固定顺序定序归并（stage2.cpp:1305-1313 注释冻结）。
- **合同表述（matrix 专项 parallel reduction tolerance）**:
  同输入同配置下，结果与 worker 数无关（1..N 线程 bitwise 一致）——
  像素内无并行归约、像素间无共享累加器、统计归并定序；large_scale
  激活强制串行。验证锚: synthetic_gate W9 LegacyLauncherEquivalent
  :3021-3160（ACR launcher ↔ CPU reference 等价）；冻结容差见 §11.4。

## 7 与 SCI 的对应与偏差（如实登记）

- SCI §5:55-60 伪码与本文档 §5 逐行同构（eligibility/wsum/vs/
  signal/support reducer/状态分支）。
- **support reducer 口径已统一（B2-A7 闭环，DISP-P2INT-001/002 关闭）**:
  SCI §5:58 已订正为 `max_{accepted} support[i]`（SCI-FIX-WEIGHT / SC-005），
  与 integrate.h:17-19、SCI §2:21/§5:63/§7:75 一致。实现把 `sup_max` 更新
  置于权重分支**之前**（integrate.cpp:44-50），零权重 accepted 样本的
  support 进入 max；回归门 eng/tests/unit/p2_output_semantics_test.cpp:85-107
  （4b/4c，B2-A7；`ctest -R p2_output_semantics` Passed）。
  **旧登记「实现现状 = max over {valid ∧ W>0}」已过期**（实现位置为
  `:49-50`，不在 `w==0 continue` 之后），不得据此整改实现。
- SCI §5:63 声称与 "integrate.cpp:10-79" / "integrate.h:1-75"
  一致——实测文件为 81 行/74 行（行号如实以本文档 §3 为准；
  语义一致不含该行号范围漂移）。

## 8 单位与 dtype 登记（唯一权威=DATA_SEMANTICS §21）

- signal: ADU（f64 输出；写盘 f32/f64 由 Stage2 precision 决定）；
  weights: 1/ADU²（本层仅数值域，无量纲混用禁止）；support: 无量纲
  [0,1]；计数: 无量纲 u32/u64。全浮点 IEEE double 域（核内），
  无 long double/复数。
- 整数登记量（status/counters）bitwise 确定；浮点仅 vs/wsum 累加
  与 signal 除法三步。

## 9 边界与退化（SCI §8 逐条实现现状）

| 条件 | 行为 | 实现锚 |
|---|---|---|
| count==0 / values==null | NO_CANDIDATES | :23-26 |
| stack/result null | rc=1（status 不变） | :20-21 |
| 非 finite values/support/weights、w<0 | INVALID_INPUT | :37/:38-42/:44-48 |
| w==0（部分） | 合法零贡献（signal 与移除该样本等价） | :49 |
| w==0（全部 accepted） | ZERO_VALID_WEIGHT（不做除法） | :49/:65-69 |
| n_accepted==0 | ALL_REJECTED | :65-69 |
| support 全空（null） | support=1.0（空支撑守恒） | :71 |
| n_used | = n_positive_weight（=通过门正权样本数） | :56 |

## 10 已冻结禁改清单（本层不可接受变化）

1. 五态枚举 name/value/顺序（integrate.h:45-51）。
2. support canonical reducer 语义 = max(accepted support)（integrate.h:17-19
   文本口径；实现已在 integrate.cpp:44-50 对齐，DISP-P2INT-001/002 关闭，
   禁止改语义解释）。
3. 零权重=合法零贡献（integrate.cpp:56 / SCI §10）。
4. 候选索引固定序归约（确定性合同，§6）。
5. policy/reducer 分离（本层不引入权重策略；weights 数组外置）。
6. 调用方禁止对 pr.support 二次 max/mean（stage2.cpp:1525-1526
   注释冻结；ACR 同型）。

## 11 冻结附录（SRC-P2-INT-001 源码实测）

### 11.1 逐符号锚

见 §3 表（锚=`grep -n`/read 实测；禁止手抄他版行号）。

### 11.2 返回码/状态码语义

- rc: 0=语义由 status 承载；1=stack/result null（:20-21）。
- status: §4 表（五态互斥显式；wsum==0 无除法路径）。
- 并发合同: reentrant=yes（无全局/静态可变状态，纯函数）；
  threadsafe=no（无内部锁，并发由调用方像素划分）；internal_parallel
  =none；取消点=无（迁移 ThreadLease 接线归 P2-INT-IMPL，
  与 DISP-COV-005 同构）。

### 11.3 缺陷清单（DISP-P2INT-001..002，**均已闭环**）

- **DISP-P2INT-001**（bughunt R3-A）: **已闭环**。现行实现把 `sup_max` 更新置于
  权重分支**之前**（`integrate.cpp:49-50`；权重分支 = `:51-57`，其中 `:56` 为
  `if (w == 0.0) continue;`），零权重 accepted 样本的 support 进入 max，与
  `integrate.h:17` 的 `max(accepted support)` 冻结语义一致。回归门：
  `eng/tests/unit/p2_output_semantics_test.cpp:85-107`（4b/4c，B2-A7）。
  **能红能绿实证（本任务新增）**：`run/SCI-FIX-PHASE2-01/exp/e5_supmax_contract.cpp`
  直接链接生产 `integrate.cpp`，输入 `{support=0.2,0.3,0.9}`、`{w=1,1,0}`、全 accepted：
  生产实现输出 `support=0.9`（判绿）；同文件内的旧排序参考实现输出 `support=0.3`
  （判红，证明该判据有判别力）；两臂 `signal` 逐位相同（=11），即 support 与 signal 正交。
  证据：`run/SCI-FIX-PHASE2-01/logs/e5.log`。
- **DISP-P2INT-002**（文档级）: **已闭环**。`docs/science/INTEGRATION.md` §5 与 §7 的
  文本口径已是 `max_{accepted} support[i]`（与 `integrate.h:17-19` 逐字一致），
  `docs/contracts/DATA_SEMANTICS.md` §21.5 的旧登记同步失效。两处不再有差集。
- 附注（锚漂移，非缺陷）: SCI §5 末尾引用 `integrate.cpp:10-79` / `integrate.h:1-75`
  超出实测行数——行号权威以本文档 §3 实测为准；该引用不承载语义。

### 11.4 TEST-P2-INT-DESIGN-001 冻结测试设计（可执行 TEST-P2-INT-001 由 P2-INT-TEST 落地）

- **F1 常量场门**（SCI §11）: `values[i]=C`（多权重组合
  equal/snr²/support_x_snr2/ivar 形状）→ `signal==C`
  （max_abs==0，rtol 0）；与权重分布无关。
  **判据非退化声明（本任务新增）**：常量场门对任何满足 `ΣwᵢC/Σwᵢ` 的实现恒真，
  **对权重口径错误无判别力**（把 support 当 ivar、把 SNR² 当 ivar 都照样通过）。
  故 F1 必须与下述 **F1b 权重判别门**成对使用，单独用 F1 不构成权重正确性的证据。
- **F1b 权重判别门（非退化，本任务新增）**: 构造**非均匀权重 + 非常量场**：
  `values = {1, 2, 4}`、`weights = {1, 1, 2}`、全 accepted、support 全 1
  → 断言 `signal == (1·1 + 1·2 + 2·4)/4 = 2.75`（bitwise，rtol 0）；
  再构造 `weights = {1, 1, 0}`（含零权重）→ 断言 `signal == 1.5` 且 `n_used == 2`。
  负例（能红）：把权重整列替换为常数或按 support 替代，`signal` 必须偏离 2.75/1.5。
  该门对「权重被静默替换/降级」有判别力，是 F1 的补集。
- **F2 零权重门**（SCI §11 零权重惰性）: 含 w=0 样本（含
  零权重 accepted 且 support 不同的样本）→ signal 与移除该样本
  bitwise 等价；n_accepted/n_finite 计入、n_used/n_positive_weight
  不计；全零权重 → ZERO_VALID_WEIGHT 且计数器五元组精确断言
  （synthetic_gate:4746-4760 先例）。
- **F3 状态穷尽门**: 五态互斥覆盖 + p2_validate_candidate_weights
  负/NaN/Inf/null 各一例 + w<0/非 finite values/support/weights
  → INVALID_INPUT + rc=1 null 栈。
- **F4 支撑门**: `support=max(accepted)` 与暴力 max 等价
  （rtol 0）；support=null → 1.0；NaN/≤0 support → INVALID_INPUT。
- **F5 DISP-P2INT-001 回归门**: 构造零权重 accepted 样本
  （support 高于正权样本）→ 断言输出 support=全局 max（按 header
  :17 口径；现状实现该门 FAIL，登记为整改验收门）。
- **F6 Python 参考 Oracle**（SCI §11）: NumPy 对同
  values/weights/support/accepted 复算 signal/support/status/
  全计数器，`rtol 1e-12`。
- **F7 并行一致性门**（§6）: 同输入 1..N 线程（Stage2 路径
  workers∈{1,2,4}）signal/support bitwise 一致（无 rtol——像素内
  无并行归约）；ACR launcher ↔ CPU reference 等价（W9 先例
  synthetic_gate:3021-3160）；per-thread 统计 thread id 定序归并
  幂等。
- 冻结容差汇总: F1/F1b/F2/F4/F5/F7 = bitwise/rtol 0；F6 = rtol 1e-12
  （NumPy 参考域）；无其他容差（本层禁引入 epsilon）。
  **适用域**：rtol 0 只在归约顺序完全一致时可达——F1/F1b 的期望值用同一 `i=0..count-1`
  固定序、同一 `vs/wsum` 双累加器复算；跨编译器/跨 FMA 契约的复算必须降到 rtol 1e-12
  并在报告中声明所用域。F1 的「与权重分布无关」指**结果与权重分布无关**（常量场恒真），
  **不**指「任何权重下的舍入都相同」——该表述不得作为容差依据。

### 11.5 SCI 层状态声明（本域零 SCI 改动）

- 积分语义权威已有 FROZEN SCI: SCI-INT-001（docs/science/
  INTEGRATION.md，集合 SCI-INT-001/002/004/008）。**共享 SCI 引用
  不改动**（P1-WCS SCI-WCS-001=共享 ASTROMETRY.md、P2-COV
  SCI-UPM-001/SCI-INT-001、P2-HIPS SCI-UPM/INT/REJ 同构）。
- matrix P2-INT 行 science_id=SCI-P2-INT-001（descriptor 占位词汇）
  的语义映射由本节声明——**SCI-P2-INT-001 ⇒ SCI-INT-001**
  （docs/science/INTEGRATION.md，矩阵 science_doc=
  docs/science/INTEGRATION.md，MOD-astrocs-phase2-integrate 行）。
  SCI 公式语义不在此重复定义，
  两处冲突时以 docs/science/ 为准并回改本文档（禁止反向）。
  ALG-INT-001/002（SCI §12）⇒ 本文档 §3/§5 算法定义承接。
- 本节禁止被编排层词汇反向改写（descriptor astrocs.phase2.integrate
  由 P2-XX-INT 对齐，不作冻结依据）。

## 12 关联 ID 映射（本文件承接）

- `ALG-P2-INT-001` = 本文档整体（逐符号锚 §3/§5；矩阵 P2-INT 行
  algorithm_id）。
- `ALG-INT-001`（SCI §12, p2_integrate_pixel 加权均值+max reducer）
  ⇒ 本文档 §3/§5；`ALG-INT-002`（p2_validate_candidate_weights）
  ⇒ 本文档 §3/:10-17。两 ID 为共享 SCI 层 ALG 词汇，本文件不
  抢注、不重复登记（INDEX.yaml ALG-INT-001 path 绑定本文件后，
  经 §13 指向语义承接）。
- `INTEGRATION_ZERO_WEIGHT_CONTRACT`（冻结名，integrate.h:5 注释）
  = §5 零权重条款 + §10.3 冻结项。

## 13 追溯

- 实现: lib/algorithms/coverage/src/integrate.cpp（81 行）+
  lib/algorithms/coverage/include/astro/phase2/integrate.h（74 行）。
- 合同: DATA-P2-INT（DATA_SEMANTICS §21）/ API-P2-INT-001
  （PUBLIC_API.md）/ TEST-P2-INT-001（MISSING，§11.4 设计冻结）。
- 交叉: docs/modules/phase2_int.md + lib/algorithms/integration/ 三件套；
  registry astrocs.phase2.integrate.md；
  INTEGRATION_ALGORITHMS.md（L2 文档，ID 语义由本文件承接）。
- 消费者: stage2.cpp（DATA_SEMANTICS §20 域）/ acr_kernels.cpp
  （ACR 域）/ module_adapters.cpp:723-741 descriptor 占位。

## 参考文献与参考代码库（含许可证）— SCI-001-S2 补齐

> 本节只补出处与参考实现，不改动本文件任何公式、锚点、阈值与容差；原有条款全部保留。

- 加权均值/逆方差聚合：教科书级（Bevington & Robinson 2003；Aitken 1935）。**差异**：本层不编码 ivar 语义（§7）。
- 最优叠加/信息保持：Zackay & Ofek 2017, ApJ 836, 187/188；Naylor 1998, MNRAS 296, 339。
- support=max：Project-defined（§5）；与 SCI-INT §5 同构。
- 并行归约容差：IEEE 754-2019；Higham 2002。

参考代码库（含许可证；GPL 代码仅作行为/数值对照，不复制进本仓）：
- Astropy（BSD-3-Clause，https://github.com/astropy/astropy）；photutils（BSD-3-Clause，https://github.com/astropy/photutils）；astropy-healpix（BSD-3-Clause，https://github.com/astropy/astropy-healpix）；ccdproc（BSD-3-Clause，https://github.com/astropy/ccdproc）；reproject（BSD-3-Clause，https://github.com/astropy/reproject）。
- DrizzlePac（BSD-3-Clause，https://github.com/spacetelescope/drizzlepac）。
- SExtractor / PSFEx / SWarp / SCAMP（GPL-3.0，https://github.com/astromatic/）。
- healpy（GPL-2.0，https://github.com/healpy/healpy）；Siril（GPL-3.0，https://gitlab.com/free-astro/siril）；LSST ip_isr（GPL-3.0，https://github.com/lsst/ip_isr）；GSL（GPL-3.0，https://www.gnu.org/software/gsl/）。
- WCSLIB（LGPL-3.0）；CFITSIO（宽松许可，NASA/HEASARC，https://heasarc.gsfc.nasa.gov/fitsio/）。
- NumPy / SciPy（BSD-3-Clause）：独立 FP64 Python Oracle。

