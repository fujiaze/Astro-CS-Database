# Phase2 Integration Algorithms（P2-INT / astrocs.p2.integration）

> ID: ALG-P2-INT-001  状态: CONTRACT_READY（P2-INT-DOC 冻结，2026-09-09）
> 模块: lib/phase2/src/integrate.cpp（76 行，astrocs_phase2 静态库成员，
> 根 CMakeLists.txt:336-346/:344）+ 唯一权威签名头
> lib/phase2/include/astro/phase2/integrate.h（74 行）
> 权威: 本文档（算法级逐符号锚）。SCI 上游: SCI-INT-001
> （docs/science/INTEGRATION.md，FROZEN T108 2026-08-23，集合
> SCI-INT-001/002/004/008，零改动）。DATA: DATA-P2-INT（DATA_SEMANTICS
> §21）。API: API-P2-INT-001（PUBLIC_API.md）。TEST: TEST-P2-INT-001
> （MISSING，P2-INT-TEST 落地；设计冻结面=本文档 §11.4）。
> 本文档承接 audit PH2-05 建议（reports/v19r7_quality/
> audit_findings_phase2.md:29：B2-12 在 INTEGRATION_ALGORITHMS.md 补
> integrate.cpp:48,65 行号）——旧 INTEGRATION_ALGORITHMS.md 行号锚缺失
> 且含 worker pool 旧表述，本文件为算法级权威重建；ALG-INT-001/002
> ID 语义由本文件 §12 映射承接。

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
| `values[i]` | 候选样本科学值 | ADU，f64 | integrate.h:37 |
| `weights[i]` | 数值权重（可空=等权 1.0） | 1/ADU²（仅数值域，语义策略在调用方），f64 | integrate.h:38 |
| `support[i]` | 覆盖支撑（可空=1.0） | 无量纲 [0,1]，f64 | integrate.h:39 |
| `accepted[i]` | 排异接受掩码（可空=全接受） | u8 | integrate.h:40 |
| `count` | 候选数 | 无量纲 u32 | integrate.h:41 |
| `signal` | 加权积分输出 | ADU，f64 | integrate.h:55 |
| `support`（输出） | canonical reducer 输出 | 无量纲 [0,1]，f64 | integrate.h:56 |
| `n_used` | 实际参与积分样本数 | 无量纲 u32 | integrate.h:57 |
| `wsum` | Σ wᵢ（eligible ∧ w>0） | 1/ADU² | integrate.cpp:52-53 |
| `vs` | Σ wᵢxᵢ | ADU/ADU² | integrate.cpp:52-53 |
| `sup_max` | max(accepted support)（现状=R3-A 缺陷语义，§11.3） | 无量纲 | integrate.cpp:54-55 |

权重语义（integrate.h:8-11 注释冻结）: mode=0 →
`stack.support_x_snr2.v1`（support×snr²）；mode=1 或 weights=null →
`stack.equal.v1`（等权）。mode/策略属于调用方（stage2.cpp:1106-1140
构造 numeric weights），reducer 只消费权重数组本身。

## 3 逐符号锚（integrate.cpp 76 行 / integrate.h 74 行，2026-09-09 实测）

| 符号/段 | 锚（integrate.cpp） | 语义 |
|---|---|---|
| p2_validate_candidate_weights | :10-17 | 预检：weights=null→0（合规）；任一 !finite 或 w<0→1（违规）；w==0 合法（不违规）。调用方: stage2.cpp:1141/:1402 |
| p2_integrate_pixel | :19-74 | 唯一生产入口（C ABI；integrate.h:58-66 声明） |
| ├ null 防御 | :20-21 | stack==null 或 result==null → rc=1 |
| ├ 空栈 | :22-26 | count==0 ∨ values==null → NO_CANDIDATES（rc=0） |
| ├ eligibility 循环 | :30-57 | 候选索引固定序 i=0..count-1 |
| │ ├ accepted 计数 | :34-36 | accepted 非空且为 0 → skip（不计 n_accepted） |
| │ ├ values 非 finite | :37 | → invalid_input |
| │ ├ support 门 | :38-42 | support 非空且（!finite 或 ≤0）→ invalid_input |
| │ ├ n_finite 计数 | :43 | ++n_finite |
| │ ├ 权重门 | :44-48 | w=weights?w[i]:1.0；!finite → invalid_input；w<0 → invalid_input |
| │ ├ 零权重 | :49 | w==0 → continue（合法零贡献；n_accepted/n_finite 已计入） |
| │ ├ sup_max 更新 | :54-55 | support 非空时 sup_max=max(sup_max,sup)——**位于 :49 continue 之后**（DISP-P2INT-001，§11.3） |
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
  n_accepted += accepted[i]!=0                                  :34-36
  invalid ⇔ accepted[i]!=0 ∧ (
      !finite(values[i])                                        :37
      ∨ (support 非空 ∧ (!finite(support[i]) ∨ support[i]<=0))  :38-42
      ∨ (weights 非空 ∧ (!finite(weights[i]) ∨ weights[i]<0))   :44-48
    )
聚合（仅通过门的样本，i 固定序）:
  n_finite += 1                                                 :43
  w_i = weights ? weights[i] : 1.0                              :44-45
  if (w_i == 0) continue            # 零权重合法零贡献           :49
  sup_max = max(sup_max, support[i])  # 现状位置→DISP-P2INT-001  :54-55
  vs += w_i * values[i];  wsum += w_i                           :52-53
  n_used += 1                                                   :56
输出:
  invalid_input        → status=INVALID_INPUT（n_used=已计数）  :58-63
  n_positive_weight==0 → status = n_accepted==0
                          ? ALL_REJECTED : ZERO_VALID_WEIGHT    :65-69
  否则                 → signal = vs / wsum                     :70
                         support = support ? sup_max : 1.0      :71
                         status = OK                            :72
```

- 权重语义（调用方构造，本层无知）: weight_mode=2 →
  `ivar_valid?ivar:support`（1/ADU²，ivar 缺失样本 fallback support，
  stage2.cpp:1106-1117/:1113-1114）；weight_mode=0 →
  `support×snr²`（legacy/诊断，stage2.cpp:1124-1136）；
  weight_mode=1/weights=null → 等权 1.0（stage2.cpp:1139）。
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
- **support reducer 表述矛盾（DISP-P2INT-002）**: SCI §5:58 写
  `max_{valid,W>0} support[i]`（其中 valid 含 accepted 判据），
  integrate.h:17 冻结注释写 "max(accepted support)"——accepted 集
  ⊇ {valid ∧ W>0}，零权重 accepted 样本为两者差集，表述冲突。
  实现现状（sup_max 在 w==0 continue 之后，:54-55）= max over
  {valid ∧ W>0}，即 **实现现状=SCI §5:58 表述**；header :17 为
  冻结合同文本。整改归 P2-INT-IMPL（实现对齐 header :17），
  归一后以 header 为唯一口径；本冻结层按实现现状如实登记，
  禁止反向修改 SCI（FROZEN）。
- **sup_max 缺陷（DISP-P2INT-001，bughunt ledger R3-A，
  run/local/bughunt/ledger.md:249）**: 零权重 accepted 样本的
  support 不进 max → 输出 support 偏低（保守方向——覆盖并集
  保守下界语义不被破坏，但偏离 header :17 冻结语义）；Stage2/ACR
  直接消费。现状测试样本 support 全为正/同值，缺陷在既有门下
  不可达（test_p2004:124 max(accepted support) 注释、
  synthetic_gate:4737-4738）。
- SCI §5:63 声称与 "integrate.cpp:10-79" / "integrate.h:1-75"
  一致——实测文件为 76 行/74 行（锚漂移，行号如实以本文档 §3
  为准；语义一致不含该行号范围漂移）。

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
2. support canonical reducer 语义（integrate.h:17 文本口径；
   实现现状偏差仅按 DISP-P2INT-001 整改，禁止改语义解释）。
3. 零权重=合法零贡献（integrate.cpp:49 / SCI §10）。
4. 候选索引固定序归约（确定性合同，§6）。
5. policy/reducer 分离（本层不引入权重策略；weights 数组外置）。
6. 调用方禁止对 pr.support 二次 max/mean（stage2.cpp:1525-1526
   注释冻结；ACR 同型）。

## 11 P2-INT-DOC 冻结附录（2026-09-09，SRC-P2-INT-001 源码实测）

### 11.1 逐符号锚

见 §3 表（锚=2026-09-09 grep/read 实测；禁止手抄他版行号）。

### 11.2 返回码/状态码语义

- rc: 0=语义由 status 承载；1=stack/result null（:20-21）。
- status: §4 表（五态互斥显式；wsum==0 无除法路径）。
- 并发合同: reentrant=yes（无全局/静态可变状态，纯函数）；
  threadsafe=no（无内部锁，并发由调用方像素划分）；internal_parallel
  =none；取消点=无（迁移 ThreadLease 接线归 P2-INT-IMPL，
  与 DISP-COV-005 同构）。

### 11.3 现状缺陷清单（DISP-P2INT-001..002，登记不改码，整改归 P2-INT-IMPL/TEST）

- **DISP-P2INT-001**（bughunt R3-A）: sup_max 在 `if (w==0) continue`
  （:49）之后更新（:54-55）→ 零权重 accepted 样本 support 被排除，
  输出 support 偏低（保守方向），偏离 integrate.h:17
  "max(accepted support)" 冻结语义；Stage2/ACR 直接消费
  （stage2.cpp:1525-1527、acr_kernels.cpp:205-208）。整改:
  sup_max 更新移至 w==0 continue 之前（P2-INT-IMPL）+ 增补
  零权重样本 support 进 max 的门（P2-INT-TEST，§11.4 F5）。
- **DISP-P2INT-002**（文档级）: INTEGRATION.md:58
  `max_{valid,W>0}` vs integrate.h:17 `max(accepted support)` 表述
  矛盾（§7）；SCI FROZEN 禁改，整改=实现归一 header 口径后以
  header 为唯一文本权威；两处差集=零权重 accepted 样本 support。
- 附注（锚漂移，非缺陷）: SCI §5:63 引用 integrate.cpp:10-79/
  integrate.h:1-75 超出实测 76/74 行——SCI FROZEN 不改，行号权威
  以本文档 §3 实测为准。

### 11.4 TEST-P2-INT-DESIGN-001 冻结测试设计（可执行 TEST-P2-INT-001 由 P2-INT-TEST 落地）

- **F1 常量场门**（SCI §11）: `values[i]=C`（多权重组合
  equal/snr²/support_x_snr2/ivar 形状）→ `signal==C`
  （max_abs==0，rtol 0）；与权重分布无关。
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
- 冻结容差汇总: F1/F2/F4/F5/F7 = bitwise/rtol 0；F6 = rtol 1e-12
  （NumPy 参考域）；无其他容差（本层禁引入 epsilon）。

### 11.5 SCI 层状态声明（本任务零 SCI 改动）

- 积分语义权威已有 FROZEN SCI: SCI-INT-001（docs/science/
  INTEGRATION.md，T108 2026-08-23 冻结，集合
  SCI-INT-001/002/004/008）。**不因本任务改动**（共享 SCI 引用
  不改动；P1-WCS-DOC SCI-WCS-001=共享 ASTROMETRY.md、P2-COV-DOC
  SCI-UPM-001/SCI-INT-001、P2-HIPS-DOC SCI-UPM/INT/REJ 先例）。
- matrix P2-INT 行 science_id=SCI-P2-INT-001（descriptor 占位词汇）
  的语义映射由本节声明——**SCI-P2-INT-001 ⇒ SCI-INT-001**
  （docs/science/INTEGRATION.md，矩阵 science_doc=
  docs/science/INTEGRATION.md，MOD-astrocs-phase2-integrate 行，
  2026-09-09 P2-INT-DOC 冻结）。SCI 公式语义不在此重复定义，
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
- `INTEGRATION_ZERO_WEIGHT_CONTRACT`（冻结名，integrate.h:5 注释、
  audit PH2-05）= §5 零权重条款 + §10.3 冻结项。

## 13 追溯

- 实现: lib/phase2/src/integrate.cpp（76 行）+
  lib/phase2/include/astro/phase2/integrate.h（74 行）。
- 合同: DATA-P2-INT（DATA_SEMANTICS §21）/ API-P2-INT-001
  （PUBLIC_API.md）/ TEST-P2-INT-001（MISSING，§11.4 设计冻结）。
- 交叉: docs/modules/phase2_int.md + lib/phase2_int/ 三件套；
  registry astrocs.phase2.integrate.md；
  INTEGRATION_ALGORITHMS.md（旧 L2 文档，ID 让位本文件）。
- 消费者: stage2.cpp（DATA_SEMANTICS §20 域）/ acr_kernels.cpp
  （ACR 域）/ module_adapters.cpp:657-675 descriptor 占位。
