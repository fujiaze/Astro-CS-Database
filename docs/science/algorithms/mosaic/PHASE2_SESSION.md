# Phase2 Session Assembly（P2-SESSION / acsd.p2.session）

> 本文件是 Phase2 进程内装配会话
> （lib/phase2_session/）的**装配合同唯一权威**：DAG 拓扑 + 端口 +
> 生命周期 + 错误/并发/取消语义逐锚冻结。定位=assembly 编排层——
> 不含任何科学公式，科学实现全部委托既有冻结 C API：coverage=
> ALG-COV-001 域（PHASE2_COVERAGE.md）、sample=ALG-P2-SMP-001 域
> （PHASE2_SAMPLER.md）、upm=ALG-UPM-001 域（UPM_SOLVER.md）、
> persist=upm 持久化（SCI-UPM-PERSIST-001 面）；HiPS 马赛克写不进
> p2 会话（`PUBLIC_API.md`「Phase2 会话装配」节冻结表述）。
> 上游：ACSD_DESIGN.md [D-1]「固定科学流程」一节与「数据流形态」一节

> 上游 SCI（共享引用零改动）: SCI-UPM-001 / SCI-INT-001 / SCI-REJ-001
> （docs/science/，映射声明见本节）。
> 下游合同: DATA-P2-SESSION（`docs/detail/registry/acsd.phase2.session.md`，并行任务生成）/ 
> API-P2-SESSION-001（PUBLIC_API.md，并行任务生成）。
> 模块: acsd.p2.session（本任务冻结的合同模块词汇）——迁移目标
> acsd_p2_session.dll 为 MODULE_MIGRATION_MATRIX 矩阵合同值，
> **尚未建立**，MISSING 如实登记；现状构建=静态库
> acsd_phase2_session（根 `CMakeLists.txt`），编入 acsd
> 可执行。权威源: `lib/phase2_session/p2_session.h`（39 行）
> + `lib/phase2_session/p2_session.cpp`（318 行，实测；本文件的锚一律给到文件/符号级）。

## 1 目的与非目标

**目的**（API-P2-001 phase session 编排承接）：把 Phase2 四段
canonical 编排（coverage→sample→upm_build→persist）收敛为单一进程内
opaque handle 会话（create→validate→run→inspect→destroy 五函数），
统一 config JSON 键集校验、host services 注入（logger/cancel/
budget/allocator）、manifest 状态机与错误映射，供 CLI 直调（CLI-005）
与 RT-005 SessionModule 工厂委托（`lib/infrastructure/scheduler/src/module_adapters.cpp` 的 P2Api）
两条消费面共用，不产生第二调度顺序。

**非目标（本模块不做）**：

- 不做任何科学计算：无 coverage union/采样/UPM 求解公式（p2_session
  全文零科学实现；facade 委托断言冻结"不复制算法"）；
- 不做 HiPS 马赛克写（upm_apply/reject/integrate/write 四域不在现状
  4 段内；`PUBLIC_API.md`「Phase2 会话装配」节；补齐归P2-SESSION-IMPL；
- 不做线程创建/并行调度决策：worker 数一律经 host->budget 注入域内
  C API（`lib/phase2_session/p2_session.cpp`，禁硬编码）；
- 不重复科学数值容差：装配层 bitwise 透传，容差归各域TEST。

## 2 层级定位与构建面

- **层级**：assembly（编排），**无独立 DLL**（现状）；MODULE_MIGRATION
  _MATRIX 的 P2-SESSION 行 dll_target=acsd_p2_session.dll 为迁移
  合同值（MISSING，§11.4 差距表）。
- **构建**（根 `CMakeLists.txt` 实测）：静态库 `acsd_phase2_session`
  =add_library(STATIC lib/phase2_session/p2_session.cpp) +
  target_include_directories（lib/phase2_session 与 lib/algorithms/coverage/include）
  + target_link_libraries PUBLIC acsd_contracts acsd_phase2；
  编入 acsd 可执行 target_link_libraries（acsd_phase2_session）；
  acsd_module_adapters 亦链接之。
- **QA-001**：acsd_phase2_session 列入自有生产 targets
  严格警告层 -Wall -Wextra -Wpedantic -Wconversion（MSVC /W4）。
- **先例同构**：lib/phase1_session/（registry 页 acsd.phase1.session.md）
  同址三件套布局；`p2_session.h` 五函数与
  `lib/phase1_session/p1_session.h` 逐一同型（run 无 async_io_depth 参数差异）。

## 3 DAG 拓扑：canonical 四段

`lib/phase2_session/p2_session.h` 冻结注释"coverage → sampler → UPM build → persist
(可选); 全部直调 lib/algorithms/coverage 生产函数"；段名与运行 trace 的冻结断言为
{"coverage","sample","upm_build","persist"} 四节点。**段序不可重排**。

| # | 段 | 输入端口（DATA-P2-SESSION） | 输出端口 | 被调用符号（path::symbol 实测锚） | 取消点 | 段内并行 |
|---|---|---|---|---|---|---|
| 1 | coverage | `hips_paths`: 非空 string[]（N 个 Phase1 单帧 HiPS 根） | P2CoverageResult（n_union_cells/union_cells/target_order，`lib/algorithms/coverage/include/astro/phase2/coverage.h`） | `lib/phase2_session/p2_session.cpp`：两遍 probe/fill p2_coverage_build（首查 inputs=nullptr 查 union 容量），RAII guard p2_coverage_free | 段边界 | 无（串行 properties/MOC 读取） |
| 2 | sample | 段 1 cov + `hips_paths` | P2ControlObservation[n_obs]（`lib/algorithms/coverage/include/astro/phase2/upm.h`）+ P2ControlNode[n_controls] + P2SampleStats | `lib/phase2_session/p2_session.cpp`：p2_sampler_default_config；两处 p2_sample_controls（两遍 probe/fill，err 512B） | 段边界 | 域内 worker 池（sc.cpu_workers=host->budget.max_workers；池在 `lib/algorithms/coverage/src/sampler.cpp` 域内，本层不开线程） |
| 3 | upm_build | 段 2 obs + uc 配置（[S-1]「判据与误差」一节 常量面） | opaque model（void*）+ P2ModelInfo（`lib/algorithms/coverage/include/astro/phase2/upm.h`） | `lib/phase2_session/p2_session.cpp`：p2_upm_build；p2_upm_info | 段边界（段内无检查点——**整模型不写半成品**，`lib/phase2_session/p2_session.h`） | 域内 blocks 并行（uc.cpu_workers=budget） |
| 4 | persist（可选） | model + `upm_save_path` + `persist_upm` | .upm 文件 + manifest artifacts[] | `lib/phase2_session/p2_session.cpp`：p2_upm_save；三处 p2_upm_close（所有权合同 `lib/phase2_session/p2_session.h`——session 持有，恰一次释放） | 取消先 close model | 无（串行 IO） |

数据所有权（`lib/phase2_session/p2_session.h` 冻结）：session 持有 Coverage/
Observations/Model，统一经 p2_coverage_free（RAII）/p2_upm_close
（三处）释放；manifest/last_error 由 SessionState 持有，destroy 唯一释放。

## 4 端口连接与 descriptor 占位对照

registry 现状**无 acsd.p2.session module_id 的 descriptor**；五
函数经 P2Api（`lib/infrastructure/scheduler/src/module_adapters.cpp` 的五静态委托）被占位
descriptor 工厂委托：phase2_descriptor()（module_id=
acsd.phase2.resample）注册段；P2-006 canonical 7 节点链
descriptors（p2_coverage/sample/upm_fit/upm_apply/reject/integrate/
write）注册段——**无第二调度顺序**（同文件注释冻结）。

| 本文件冻结词汇（会话端口） | descriptor 占位词汇（实测） | 对齐归属 |
|---|---|---|
| 输入 hips_paths / config_json | phase2.resample ports calibrated→resampled；7 链各 ports（均在 `lib/infrastructure/scheduler/src/module_adapters.cpp`） | P2-XX-INT |
| 输出 manifest_json / upm_model（可选 persist） | 占位 sci/alg/data/api/test 词汇：SCI-P2-RES-001/ALG-P2-RES-001/DATA-P2-RES/TEST-P2-RES-001；7 链各自占位（同一文件） | P2-XX-INT |
| 合同 ID：SCI-UPM-001+SCI-INT-001+SCI-REJ-001 / ALG-P2-SESSION-001 / DATA-P2-SESSION / API-P2-SESSION-001 / TEST-P2-SESSION-001 | 占位 api_id 一致同为 API-P2-001（同一文件） | P2-XX-INT |

**注记（冻结边界）**：descriptor 占位 ID 为编排层词汇（P1 registry
页先例 acsd.phase1.session.md 同构表述），编排层词汇只作对齐对象，本
文件冻结依据；对齐由 P2-SESSION-INT 执行（台账 V7_1_STATIC_TASK_
LEDGER.csv 关键词 "module integration descriptor + typed ports"）。

## 5 调用序与生命周期

五函数契约（`lib/phase2_session/p2_session.h` 注释冻结）：handle opaque、
owner=创建者、threadsafe:no（handle 级）、reentrant:yes。

1. **p2_session_create**（`lib/phase2_session/p2_session.cpp`）：host null/struct_size 不符/
   abi_version≠ACS_ABI_VERSION_V1 → ACS_ERR_ABI_MISMATCH；
   out null → PARAM；new nothrow 失败 → NOMEM；
   manifest 初始化 kind="acsd_phase2_session" + stages 空数组。
2. **p2_session_validate**（同文件）：纯读无 IO；span 空/null → PARAM；
   坏 JSON（parse_error）→ PARAM；非 object →
   PARAM；缺必需键 `hips_paths`/`output_dir` → PARAM；
   类型门 hips_paths 非空 string[]、output_dir string、
   upm 为 object。
   **无 silent default（正向约束 + 适用域）**：validate 的判据面 = 必需键存在性
   （`hips_paths`/`output_dir`）、类型、`upm` 形状；**键白名单不在其判据面内**——
   未知键（含科学参数拼写错误，如 `sigma_flor`）不报错、静默落到常量面默认值
   （实现事实：`lib/phase2_session/p2_session.cpp` 的 validate 段无未知键遍历）。
   因此「无 silent default」的**适用域**必须写成：*已知键的取值语义无默认*；
   *未知键的拒绝不在本层*。消费方若要求未知键 fail-closed，必须在上层 config
   schema 校验承担。
   **组合约束（正向）**：`persist_upm=true` 必须与 `upm_save_path` 同时出现——
   当前实现 `doc.value("persist_upm",false) && doc.contains("upm_save_path")`
   （`lib/phase2_session/p2_session.cpp`）在缺 `upm_save_path` 时**不落 persist 且不报错**；
   规范要求该组合缺失时显式判 PARAM（登记 DISP-P2SES-004，整改归 P2-SESSION-IMPL）。
3. **p2_session_run**（`lib/phase2_session/p2_session.cpp`）：parse（失败文案
   "config parse failed (validate first)"）→ hips 装配→
   预算日志 → 四段（[S-1]「公式与推导」一节）。常量面（冻结首版值）：
   robust_loss=0（huber）、upm_weight_source=0、weight 由 Phase2 按该天球像素
   对应帧集合现场算出（逐样本 ivar；SNR 只作 veto/质量门）、
   huber_delta=1.345（**无量纲**；高斯参考分布下渐近效率 95% 的 Huber 阈值，
   出处 Huber 1964, Ann. Math. Statist. 35, 73, DOI 10.1214/aoms/1177703732；
   Holland & Welsch 1977, Comm. Statist. A6, 813, DOI 10.1080/03610927708827533；
   适用域=标准化残差 z=r/sigma_eff 且 sigma_eff 由观测标度主导，见
   `docs/science/sky/UPM.md` [S-1]「参考文献与参考代码」一节）、max_iterations=100（无量纲；域 ≥1，
   达上限时 converged 必须记 0=max_iter 而非「已收敛」）、
   tolerance=1e-6（**本入口为绝对阈值**：`lib/phase2_session/p2_session.cpp` 只赋 `uc.tolerance`，
   **未设** `uc.tolerance_relative`（`P2UpmBuildConfig uc{}` 零初始化 ⇒ 0），
   故判据 = `max_dM < tol ∧ max_dC < tol`（`lib/algorithms/coverage/src/upm.cpp`，legacy 绝对口径）。
   相对口径（阈值 = `tolerance × max(scale_obs, 1.0)`，**floor = 1.0 而非 eps**）
   仅在 `tolerance_relative=1` 时生效（`lib/algorithms/coverage/src/upm.cpp`，default 0），
   由编排入口显式 opt-in（`lib/infrastructure/scheduler/src/module_adapters.cpp`）——
   **口径依入口而变，引用 `converged`/`tolerance_relative` 必须先写明入口**
   （`docs/science/sky/UPM.md` [S-1]「公式与推导」一节 正向约束）。在面亮度 ADU·sr⁻¹ 标度上
   绝对 1e-6 低于 ULP 5–8 个数量级、原理上不可达（`lib/algorithms/coverage/src/upm.cpp`；
   实测 iterations=100, converged=0））、
   sigma_floor=1e-3（**量纲 = σ(ADU)，帧面标度**，与所消费的 `uncertainty` 同标度；
   权威 = `docs/detail/registry/acsd.phase2.upm-fit.md` 的单位/dtype 面标度条与「三地板互不代用」条
   （**不是**面亮度 ADU·sr⁻¹；`photo_scaled_adu` 时按 α 换算）；
   当 `|uncertainty| < sigma_floor` 时 Huber 的 z 失去统计尺度意义，
   见 `docs/science/sky/UPM.md` [S-1]「公式与推导」一节）、support_power=1.0、use_ivar_weight=1、
   control_reliability=1.0、target_order=cov 实测值；config
   `upm.{max_iterations,huber_delta,smoothing_lambda}` 可覆盖
   ；persist 条件 `persist_upm && upm_save_path`。
4. **p2_session_inspect**（`lib/phase2_session/p2_session.cpp`）：状态补标（未 run 无错→"created"；
   未 run 有错→"failed"+error）；manifest.dump(2)；
   out 缓冲经 host->allocator.alloc 16 对齐，
   调用方经 host free 释放。
5. **p2_session_destroy**（`lib/phase2_session/p2_session.cpp`）：唯一释放对（delete s）。

诊断：`acsd::phase2::last_error`（`lib/phase2_session/p2_session.cpp`，脱敏摘要，handle 空→
空串；RT-008 CLI 合同经 P2Api::last_error 暴露，见 `lib/infrastructure/scheduler/src/module_adapters.cpp`）。

**output_dir 注入面**（config 生产者侧）：`lib/infrastructure/cli/parser.cpp` 缺
`output_dir` 即拒（"config missing 'output_dir'"）；`lib/infrastructure/cli/runtime_client.cpp`
的 phase_config 段——run 格式自动补 output_dir，phase2
格式直通**不自动补**；session validate 拒缺失（CLI 2，同文件注释冻结）。契约面：parse 拒平铺缺键、
passthrough 交会话拒——两道防线，语义一致。

## 6 trace 语义

- **manifest**（SessionState.manifest，同文件）：kind="acsd_phase2_
  session"；stages[] 逐段 name/status + fail 时 rc/err、ok 时
  计数（stage()；coverage ok n_inputs/n_union_cells/target_
  order；sample ok n_obs/n_controls/accepted_obs/overlap_
  controls；upm_build ok control_count/observation_count/
  component_count/target_order/model_hash；persist ok path）；artifacts[]（persist 成功 push save_path）；顶层
  n_inputs/n_obs/status；error_kind "input"/"output"；error（inspect）。
- **结构化日志**：host->logger 通道（log()，同文件）——run 起预算行、
  coverage ok cells=、sample ok obs=/overlap_ controls=。
- **域内 provenance 衔接**：逐帧 P2HipsInputInfo（`lib/algorithms/coverage/include/astro/phase2/coverage.h`）
  由 coverage 域填充（session 仅预填 hips_path），frame_id/
  provenance/拒绝统计等域内 trace **不上浮**会话 manifest（manifest
  仅计数汇总）；sampler 域 stderr 诊断（DISP-P2SMP-003）不经本层。
  manifest 为会话唯一外发 trace（P2Api 经 RT-008 SessionModule 捕获
  上报，`lib/infrastructure/scheduler/src/module_adapters.cpp` 注释）。

## 7 NODE-CALL 唯一性（符号×段矩阵）

**断言**：每个科学域 C API 恰被一个段调用；任意执行路径上每个
所有权符号恰调用一次（除两遍 probe/fill 协议与条件 persist）。

| 符号（声明锚） | coverage | sample | upm_build | persist | 合计/执行路径 |
|---|---|---|---|---|---|
| p2_coverage_build（`lib/algorithms/coverage/include/astro/phase2/coverage.h`） | `lib/phase2_session/p2_session.cpp` 两处 | — | — | — | 恰 2（probe/fill） |
| p2_coverage_free（同上头） | `lib/phase2_session/p2_session.cpp` RAII | — | — | — | 恰 1（含失败路径） |
| p2_sampler_default_config（`lib/algorithms/coverage/include/astro/phase2/sampler.h`） | — | `lib/phase2_session/p2_session.cpp` | — | — | 恰 1 |
| p2_sample_controls（同上头） | — | `lib/phase2_session/p2_session.cpp` 两处 | — | — | 恰 2（probe/fill） |
| p2_upm_build（`lib/algorithms/coverage/include/astro/phase2/upm.h`） | — | — | `lib/phase2_session/p2_session.cpp` | — | 恰 1 |
| p2_upm_info（同上头） | — | — | `lib/phase2_session/p2_session.cpp` | — | 恰 1（失败可容忍） |
| p2_upm_save（同上头） | — | — | — | `lib/phase2_session/p2_session.cpp` | 0..1（条件 persist） |
| p2_upm_close（同上头） | — | — | — | `lib/phase2_session/p2_session.cpp` 三处 | 恰 1（正常；persist 取消；save 失败） |

反断言（grep 实测）：`lib/phase2_session/p2_session.cpp` 不含 p2_integrate_
pixel / p2_reject_* / p2_upm_apply 族 / hips writer 任何符号——7 节点
链其余四域不在现状 4 段（[S-1]差距表）。p2_coverage_build / p2_sample_controls /
p2_upm_build 的委托断言与段序断言冻结于 §11.5 T1。

## 8 预算绑定与并发语义

- **并发合同**（`lib/phase2_session/p2_session.h` 冻结）：reentrant:yes；threadsafe:no
  （handle 级——同一 handle 禁并发调用）；内部并行仅经预算注入域内
  C API。
- **预算注入**：sample sc.cpu_workers=host->budget.max_workers
  （`lib/phase2_session/p2_session.cpp`）；upm uc.cpu_workers=host->budget.max_workers（同文件）；
  worker 数零硬编码。sampler 域 0→1 归一（`lib/algorithms/coverage/src/sampler.cpp`）、1=串行
  reference；UPM blocks 并行语义归 ALG-UPM-001 域。
- **线程创建归零**：`lib/phase2_session/p2_session.cpp` 无 std::thread/omp 原语（grep 实
  测）——并行全部在域内实现（sampler.cpp worker 池 / upm.cpp blocks），
  会话层单线程顺序编排四段。
- **预算注释矛盾**（DISP-P2SES-008）：`lib/phase2_session/p2_session.h`
  "sampler=1(串行 reference)" 与 "内部并行仅 UPM blocks" 为陈旧
  表述，与实现（sample 亦 budget 多 worker）矛盾——以实现为
  现状口径，合同文本归一归 P2-SESSION-IMPL。

## 9 取消语义

- **取消点=四段边界**（`lib/phase2_session/p2_session.h` 冻结；实现在 `lib/phase2_session/p2_session.cpp`）：每段入口检查 host->cancel.is_cancelled →
  stage 标 "cancelled" + 返回 ACS_ERR_CANCELLED。
- **upm 整模型不写半成品**：upm_build 段内无取消检查点（求解原子）；
  persist 取消先 p2_upm_close 释放再返回（同文件，所有权合同优先）。
- **取消后 manifest 歧义**（DISP-P2SES-003）：顶层 status 未
  设 cancelled，inspect 回落 "created"——整改归
  P2-SESSION-IMPL。
- 域内无检查点与 DISP-COV-005/P2-SMP ThreadLease 缺口同构（PHASE2_
  SAMPLER.md [S-1]「冻结附录」一节），段粒度取消为会话层唯一取消面。

## 10 已冻结禁改清单（本层不可接受变化）

1. 段序 coverage→sample→upm_build→persist 不可重排（`lib/phase2_session/p2_session.h`）。
2. canonical 节点集 {coverage,sample,upm_build,persist}（同一测试的静态
   门；typed DAG 扩面归 P2-SESSION-IMPL，不回头改 4 段 trace 词汇）。
3. 五函数签名与 handle 所有权（`lib/phase2_session/p2_session.h`；P2Api 委托面
   `lib/infrastructure/scheduler/src/module_adapters.cpp`）。
4. validate 无 silent default（`lib/phase2_session/p2_session.h`；缺必需键/类型错
   →PARAM）。
5. 错误映射 rc=1→PARAM / rc=2→STATE（合同 [S-1]「参数与常数」一节）/ persist IO→ACS_ERR_
   IO（`lib/phase2_session/p2_session.cpp` 的头部注释与 map_rc 分支）。
6. 每段恰一取消检查点=段边界；upm 段整模型原子（`lib/phase2_session/p2_session.h`）。
7. 预算零硬编码：worker 数恒经 host->budget 注入（`lib/phase2_session/p2_session.cpp`）。
8. NODE-CALL 唯一性（[S-1]「参考文献与参考代码」一节 矩阵）；facade 不内联科学。

## 11 冻结附录（SRC-P2-SESSION-001 源码实测）

### 11.1 返回码/错误映射（ACS_ERR_* 全清单）

| ACS_ERR_* | 触发（精确） | 锚（p2_session.cpp） |
|---|---|---|
| ACS_ERR_ABI_MISMATCH | host null / struct_size≠sizeof(acsd_host_services_v1) / abi_version≠V1 | `lib/phase2_session/p2_session.cpp` |
| ACS_ERR_PARAM | out/hull、span 空、坏 JSON、非 object、缺必需键、类型错（validate 与 run 前置） | 同上 |
| ACS_ERR_NOMEM | SessionState new 失败；inspect host alloc 失败 | 同上 |
| ACS_ERR_CANCELLED | 四段边界取消 | 同上 |
| ACS_ERR_STATE | 域 rc=2（合同 §4 build fail，如 production 显式缺 ivar） | map_rc |
| ACS_ERR_IO | p2_upm_save 失败（error_kind="output"） | 同上 |
| ACS_ERR_INTERNAL | 域 rc 其他（非 0/1/2） | map_rc |

域 rc→ACS_ERR 归并 map_rc（`lib/phase2_session/p2_session.cpp`）：rc=0→OK；last_error 记
"<what> rc=<n>"；error_kind 标 "input"。域内 rc 细分语义见各域文档
（PHASE2_SAMPLER相应章节等）；本层不重解释域返回码。

### 11.2 manifest 状态机与字段

```text
create 成功 → kind/stages[] → [validate 只读不改]
run 成功  → status="complete" + n_inputs/n_obs [+ artifacts]   # p2_session.cpp
run 失败  → error_kind/last_error → inspect 补标 status="failed"
            + error                                             # p2_session.cpp
run 取消  → stage="cancelled"；顶层 status 歧义（DISP-P2SES-003） # p2_session.cpp
inspect   → created | complete | failed（"cancelled" 未单列）    # p2_session.cpp
```

字段全集：kind/stages[]（name/status/rc/err/计数，§6）/artifacts[]/
n_inputs/n_obs/status/error_kind/error。dtype/键集唯一权威=
DATA-P2-SESSION（并行任务生成）；本节为实现现状锚定。

### 11.3 现状缺陷清单（DISP-P2SES-001..008，登记不改码，整改归 P2-SESSION-IMPL/TEST）

| ID | 锚 | 内容 | 整改归属 |
|---|---|---|---|
| DISP-P2SES-001 | `lib/phase2_session/p2_session.cpp` vs `lib/phase2_session/p2_session.h` | validate 实际**未拒未知键**（仅必需键/类型/upm 形状）；`lib/phase2_session/p2_session.h` 头注释"拒未知键/缺必需键"与实现出入——config 键白名单缺失，拼错键静默忽略 | P2-SESSION-IMPL（补键白名单或修正头注释）+ TEST负面门（[S-1]「判据与误差」一节T3） |
| DISP-P2SES-002 | `lib/phase2_session/p2_session.cpp` | run 未复用/未强制 validate：parse 后直接 doc["hips_paths"]（缺键或非 object 时 nlohmann type_error 未捕获，**异常可穿越 extern "C" ABI**；try 仅覆盖 parse） | P2-SESSION-IMPL（run 前置 validate 或 try/catch 全包） |
| DISP-P2SES-003 | `lib/phase2_session/p2_session.cpp` | 取消路径未设顶层 status（"cancelled"），ran=false 且 last_error 空 → inspect 回落 "created"，取消态不可辨识 | P2-SESSION-IMPL（status 状态机补 cancelled）+ TEST |
| DISP-P2SES-004 | `lib/phase2_session/p2_session.cpp` | persist_upm=true 而 upm_save_path 缺失 → **静默跳过 persist**（无告警无 stage 记录）；validate 不校验该组合，违背"无 silent default"精神 | P2-SESSION-IMPL（validate 增组合校验或 run 显式报错） |
| DISP-P2SES-005 | `lib/phase2_session/p2_session.cpp` | UPM 首版常量硬编码且仅 max_iterations/huber_delta/smoothing_lambda 可经 config 覆盖；tolerance/sigma_floor/support_power/use_ivar_weight/control_reliability/zero_anchor_weight 等不可配置（与 DISP-P2SMP-004 schema 缺口同构） | P2-SESSION-IMPL（config 键集扩面须同步DATA-P2-SESSION [S-1]） |
| DISP-P2SES-006 | `lib/phase2_session/p2_session.cpp` | error_kind 分类粗粒度：map_rc 一律标 "input"，仅 persist 显式 "output"——HiPS 打开失败（IO 性质）亦标 input，诊断分流失真 | P2-SESSION-IMPL（观察级） |
| DISP-P2SES-007 | `lib/phase2_session/p2_session.cpp` | 两遍 probe/fill 的**部分失败容错**未定义于合同：首查 rc≠0 且已得容量（cov.n_union_cells>0 / n_obs>0）时静默续跑 fill，首查错误被丢弃（coverage 的 `rc!=0 && n_union_cells==0` 才失败；sample 同型） | P2-SESSION-IMPL（合同化容错语义或收紧为 rc==0）+ TEST |
| DISP-P2SES-008 | `lib/phase2_session/p2_session.h` vs `lib/phase2_session/p2_session.cpp` | 预算绑定头注释陈旧（"sampler=1 串行 reference"/"内部并行仅 UPM blocks"）与实现（sample 亦 budget 多 worker）矛盾；API-P2-001 合同 [S-1]「公式与推导」一节 表述同步归口 | P2-SESSION-IMPL（合同文本归一） |

### 11.4 IMPL-COMPLETE 全链 artifact 目标合同与现状差距表

| # | artifact | 现状（实测） | 差距归属 |
|---|---|---|---|
| 1 | acsd_p2_session.dll（独立迁移目标） | 不存在（MISSING）；现状=静态库 acsd_phase2_session（根 `CMakeLists.txt`）编入 acsd 可执行 | P2-SESSION-IMPL |
| 2 | coverage 域产物（union MOC+target_order，P2CoverageResult 进程内） | 已实现（lib/algorithms/coverage/src/coverage.cpp，ALG-COV-001 域） | 已存在（P2-COV 域） |
| 3 | sample 域产物（P2ControlObservation/P2ControlNode/P2SampleStats） | 已实现（sampler.cpp，ALG-P2-SMP-001 域） | 已存在（P2-SAMP 域） |
| 4 | upm 域产物（model 构建/持久化 p2_upm_build/save/info/close） | 已实现（lib/algorithms/coverage/src/upm*.cpp，ALG-UPM-001 域） | 已存在（P2-UPM 域） |
| 5 | persist 段=HiPS writer 域马赛克写产品 | **不在会话**：p2_session persist 段仅 upm_save 单产物；upm_apply/reject/integrate/write 四域未编排（`PUBLIC_API.md`「Phase2 会话装配」节） | P2-SESSION-IMPL（typed DAG 扩面） |
| 6 | typed phase2 DAG 全链执行（"full execution no partial facade"） | 现状=4 段 facade 直调（契合现状口径） | P2-SESSION-IMPL（台账） |
| 7 | module integration descriptor + typed ports | registry 无 acsd.p2.session descriptor；占位 descriptor 工厂委托（§4） | P2-SESSION-INT（台账） |
| 8 | registry 页 acsd.phase2.session.md / README / memory | 不存在；归 registry 面登记 | registry 面 |
| 9 | TEST-P2-SESSION-001 可执行测试 | MISSING（不冒认） | P2-SESSION-TEST（台账） |

### 11.5 TEST-P2-SESSION-DESIGN-001 冻结测试设计（可执行 TEST-P2-SESSION-001 由 P2-SESSION-TEST 落地，MISSING 如实登记）

锚定台账三条任务关键词（`V7_1_STATIC_TASK_LEDGER.csv` 的三条 session 行），
对拍先例 `p1_ir_facade_test.cpp` / `p2_ir_facade_test.cpp`：

- **T1 call-count 唯一性**（台账 "add node call-count tests …
  coverage through hips writer each once"）：§7 矩阵逐符号断言——
  现状 4 段（p2_coverage_build×2/probe-fill、p2_sample_controls×2、
  p2_upm_build×1、p2_upm_close 恰 1、反断言零越段调用）+ typed DAG
  扩面后的 7 节点链全链 call-count（coverage→…→hips writer 每节点
  恰一次，P2-SESSION-IMPL 落地后启用该面）。静态 grep 审核
  （`p2_ir_facade_test.cpp` 先例）+ 运行期核对双层。
- **T2 typed DAG/full execution**（台账）：canonical 节点集断言
  （facade 测试先例）、段序不可重排（同上）、facade 零内联科学
  （不含 UPM 求解循环/积分/排异符号）、四段 trace 与静态节点一致
  （manifest 门先例）。
- **T3 descriptor 集成/validate 负面矩阵**（台账）：坏 JSON/非
  object/缺 hips_paths/缺 output_dir/hips_paths 空与非 string 项/
  upm 非对象/未知键（DISP-P2SES-001 现状口径：不拒——按[S-1]登记断
  言，整改后翻转为拒）/output_dir 注入面（`lib/infrastructure/cli/parser.cpp` 拒、
  `lib/infrastructure/cli/runtime_client.cpp` 补/直通不补）；取消注入点：四段边界各
  一（mock host cancel → ACS_ERR_CANCELLED + persist 段取消 model
  仍释放）；manifest 状态机：created→complete/failed 全路径 +
  取消歧义现状断言（DISP-P2SES-003）。
- **容差=装配层无科学数值容差**：config 透传/manifest 计数/artifacts
  全部 bitwise（EXPECT_EQ 级）；科学数值容差归各域 TEST（ALG-COV/
  ALG-P2-SMP/ALG-UPM 各自 TEST 面），本层取值面 = bitwise（无 epsilon）。
- fixture：合成最小 HiPS 树（固定 seed，不提交大二进制；P2-SAMP
  fixture 先例）；host services 用假 host（logger/cancel/budget/
  allocator 可编程）。

### 11.6 SCI 层状态声明（本域零 SCI 改动）

- p2 会话引用 SCI-UPM-001 / SCI-INT-001 / SCI-REJ-001 的语义
  （docs/science/，FROZEN；**共享 SCI 引用不改动**）
  （P1-WCS SCI-WCS-001=共享 docs/science/detection/ASTROMETRY.md、P2-COV
  SCI-UPM-001/SCI-INT-001、P2-INT SCI-INT-001、P2-REJ
  SCI-REJ-001 同构）。现状 4 段实际消费面=SCI-UPM-001（coverage/
  sampler/upm 域均在其集合 SCI-UPM-001..010 内）；SCI-INT-001/
  SCI-REJ-001 为会话下游 typed DAG 扩面（upm_apply/reject/integrate/
  write 四域）的共享 SCI 引用；本层直接消费面 = SCI-UPM-001（coverage/sampler/upm
  域），调用面随 typed DAG 扩面接入。
- SCI 公式语义不在此重复定义；两处冲突以 docs/science/ 为准并回改
  本文档（方向 = 从 docs/science/ 到本文档）。
- descriptor 占位词汇（SCI-P2-RES-001 等，[S-1]「参数与常数」一节）与本页冲突时以本页为
  准；本节是唯一冻结依据（编排层词汇只作对齐对象；acsd.p2.session 由
  P2-SESSION-INT 对齐，不作冻结依据）。

## 12 关联 ID 映射（本文件承接）

- `ALG-P2-SESSION-001` = 本文档整体（DAG [S-1]「公式与推导」一节/端口 [S-1]「参数与常数」一节/生命周期 [S-1]「判据与误差」一节/
  唯一性 [S-1]「参考文献与参考代码」一节；矩阵 P2-SESSION 行 algorithm_id，INDEX.yaml path 绑定
  本文件——由并行任务登记，此处引用 id 不引节号）。
- `DATA-P2-SESSION`（`docs/detail/registry/acsd.phase2.session.md`，生成中）= config 键集
  （[S-5]「输入输出端口」一节）/manifest 字段（[S-5]「输入输出端口」一节）契约面；冲突以 [S-5]「输入输出端口」一节 为准。
- `API-P2-SESSION-001`（PUBLIC_API.md，生成中）= 五函数 C API +
  last_error 诊断面（[S-1]「判据与误差」一节）。
- `TEST-P2-SESSION-001`（MISSING）= [S-1]设计冻结的可执行载体
  （P2-SESSION-TEST 落地）；`TEST-P2-SESSION-DESIGN-001` = 本文件
  [S-1]双面登记不冒认。
- `SRC-P2-SESSION-001` = lib/phase2_session/ 源码实测面（p2_session.h
  39 行 + `lib/phase2_session/p2_session.cpp` 318 行（复测）+ 根 `CMakeLists.txt` 相应段），本文件全部锚的权威。
- `MOD-acsd-phase2-session` = lib/phase2_session/module.yaml（本
  任务同批建立）+ registry 页（并行任务生成）。

## 13 追溯

- 实现：lib/phase2_session/p2_session.h（39 行）+ p2_session.cpp
  （318 行，复测）；构建：静态库 acsd_phase2_session（CMakeLists.txt
  的相应段）→ acsd 可执行 + QA-001 严格警告层
  （同一 `CMakeLists.txt`）。
- 编排消费面：CLI 直调（CLI-005）与 RT-005/RT-008 SessionModule
  （`lib/infrastructure/scheduler/src/module_adapters.cpp` 的 P2Api 与两处注册段）。
- 对拍先例：lib/phase1_session/ + registry 页
  acsd.phase1.session.md；PHASE2_SAMPLER.md [S-1]「冻结附录」一节结构。
- 消费域：ALG-COV-001（PHASE2_COVERAGE.md）/ ALG-P2-SMP-001
  （PHASE2_SAMPLER.md）/ ALG-UPM-001（UPM_SOLVER.md）。
- 差距整改：[S-1]（IMPL/INT）+ [S-1]「判据与误差」一节DISP-P2SES-001..008；
  测试落地：P2-SESSION-TEST。

> 本文引用上游正本（论文式编号，正文引用处均已改为自然语言节名，不再使用跨文档 §N 跳转）：
> - [D-1] docs/ACSD_DESIGN.md（最高设计）。
> - [S-1] docs/science/sky/UPM.md（天光平面科学正本）。
> - [S-2] docs/science/integration/INTEGRATION.md（集成科学正本）。
> - [S-3] docs/science/integration/REJECTION.md（排异科学正本）。
> - [S-5] docs/detail/registry/acsd.phase2.session.md（相关科学正本）。

## 参考文献与参考代码库（含许可证）

- DAG/拓扑排序：Kahn 1962, Comm. ACM 5, 558（DOI 10.1145/368996.369025）；Cormen et al. 2009, Introduction to Algorithms 3rd ed., MIT Press, §22.4。
- provenance/来源链：W3C PROV-DM（https://www.w3.org/TR/prov-dm/）；manifest 字段语义以 DATA_SEMANTICS 为准。
- 内容寻址/哈希：Merkle 1988, Advances in Cryptology (CRYPTO 87), 369；SHA-256 NIST FIPS 180-4。
- FITS checksum：FITS Standard 3.0 §5.5（DATASUM/CHECKSUM）；CFITSIO（宽松许可）。
- 取消/生命周期语义：Project-defined（本文件 §5/§9）。
