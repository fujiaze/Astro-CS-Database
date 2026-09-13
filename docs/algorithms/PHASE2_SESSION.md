# Phase2 Session Assembly（P2-SESSION / astrocs.p2.session）

> ID: ALG-P2-SESSION-001  状态: CONTRACT_READY（P2-SESSION-DOC 冻结，
> 2026-09-10，owner SA-P2-X24）。本文件是 Phase2 进程内装配会话
> （lib/phase2_session/）的**装配合同唯一权威**：DAG 拓扑 + 端口 +
> 生命周期 + 错误/并发/取消语义逐锚冻结。定位=assembly 编排层——
> 不含任何科学公式，科学实现全部委托既有冻结 C API：coverage=
> ALG-COV-001 域（PHASE2_COVERAGE.md）、sample=ALG-P2-SMP-001 域
> （PHASE2_SAMPLER.md）、upm=ALG-UPM-001 域（UPM_SOLVER.md）、
> persist=upm 持久化（SCI-UPM-PERSIST-001 面）；HiPS 马赛克写不进
> 本会话（PUBLIC_API.md:1088-1090 冻结表述）。
> 上游 SCI（共享引用零改动）: SCI-UPM-001 / SCI-INT-001 / SCI-REJ-001
> （docs/science/，映射声明见 §11.6）。
> 下游合同: DATA-P2-SESSION（DATA_SEMANTICS §24，并行任务生成）/ 
> API-P2-SESSION-001（PUBLIC_API.md，并行任务生成）。
> 模块: astrocs.p2.session（本任务冻结的合同模块词汇）——迁移目标
> astrocs_p2_session.dll 为 MODULE_MIGRATION_MATRIX 矩阵合同值，
> **尚未建立**，MISSING 如实登记；现状构建=静态库
> astrocs_phase2_session（根 CMakeLists.txt:454-458），编入 astrocs
> 可执行（:501-506）。权威源: lib/phase2_session/p2_session.h（39 行）
> + p2_session.cpp（282 行，2026-09-10 grep/read 实测；本文件行号
> 禁止手抄他版）。

## 1 目的与非目标

**目的**（API-P2-001 phase session 编排承接）：把 Phase2 四段
canonical 编排（coverage→sample→upm_build→persist）收敛为单一进程内
opaque handle 会话（create→validate→run→inspect→destroy 五函数），
统一 config JSON 键集校验、host services 注入（logger/cancel/
budget/allocator）、manifest 状态机与错误映射，供 CLI 直调（CLI-005）
与 RT-005 SessionModule 工厂委托（module_adapters.cpp:777-784 P2Api）
两条消费面共用，不产生第二调度顺序。

**非目标（本模块不做）**：

- 不做任何科学计算：无 coverage union/采样/UPM 求解公式（p2_session
  全文零科学实现；tests/unit/p2_ir_facade_test.cpp:41-52 facade 委托
  断言冻结"不复制算法"）；
- 不做 HiPS 马赛克写（upm_apply/reject/integrate/write 四域不在现状
  4 段内；PUBLIC_API.md:1088-1090；补齐归 P2-SESSION-IMPL，§11.4）；
- 不做线程创建/并行调度决策：worker 数一律经 host->budget 注入域内
  C API（:155/:195，禁硬编码）；
- 不重复科学数值容差：装配层 bitwise 透传，容差归各域 TEST（§11.5）。

## 2 层级定位与构建面

- **层级**：assembly（编排），**无独立 DLL**（现状）；MODULE_MIGRATION
  _MATRIX 的 P2-SESSION 行 dll_target=astrocs_p2_session.dll 为迁移
  合同值（MISSING，§11.4 差距表）。
- **构建**（根 CMakeLists.txt 实测）：静态库 `astrocs_phase2_session`
  =add_library(STATIC lib/phase2_session/p2_session.cpp) :454 +
  target_include_directories（lib/phase2_session 与 lib/phase2/include）
  :455-457 + target_link_libraries PUBLIC astrocs_contracts astrocs_phase2
  :458；编入 astrocs 可执行 target_link_libraries（astrocs_phase2_session
  :504，块 :501-506）；astrocs_module_adapters 亦链接之（:538）。
- **QA-001**（:517-529）：astrocs_phase2_session 列入自有生产 targets
  严格警告层 -Wall -Wextra -Wpedantic -Wconversion（:522，MSVC /W4）。
- **先例同构**：lib/phase1_session/（P1-SESSION-DOC，registry 页
  astrocs.phase1.session.md）同址三件套布局；p2_session.h 五函数与
  p1_session.h:16-28 逐一同型（run 无 async_io_depth 参数差异）。

## 3 DAG 拓扑：canonical 四段

p2_session.h:2 冻结注释"coverage → sampler → UPM build → persist
(可选); 全部直调 lib/phase2 生产函数"；段名与运行 trace 由
p2_ir_facade_test.cpp:31-38 断言（{"coverage","sample","upm_build",
"persist"} 四节点）。**段序不可重排**（§10.1）。

| # | 段 | 输入端口（DATA-P2-SESSION §24） | 输出端口 | 被调用符号（path::symbol 实测锚） | 取消点 | 段内并行 |
|---|---|---|---|---|---|---|
| 1 | coverage | `hips_paths`: 非空 string[]（N 个 Phase1 单帧 HiPS 根） | P2CoverageResult（n_union_cells/union_cells/target_order，coverage.h:45-53） | p2_session.cpp:125/:138 p2_coverage_build（两遍 probe/fill，首查 :124 inputs=nullptr 查 union 容量）；:143-144 RAII guard p2_coverage_free | :120 段边界 | 无（串行 properties/MOC 读取） |
| 2 | sample | 段 1 cov + `hips_paths` | P2ControlObservation[n_obs]（upm.h:31-57）+ P2ControlNode[n_controls] + P2SampleStats | :154 p2_sampler_default_config；:158/:167 p2_sample_controls（两遍 probe/fill，err 512B :166） | :152 段边界 | 域内 worker 池（sc.cpu_workers=host->budget.max_workers :155；池在 sampler.cpp 域内，本层不开线程） |
| 3 | upm_build | 段 2 obs + uc 配置（§5 常量面） | opaque model（void*）+ P2ModelInfo（upm.h:60-68） | :204 p2_upm_build；:210 p2_upm_info | :181 段边界（段内无检查点——**整模型不写半成品**，p2_session.h:22） | 域内 blocks 并行（uc.cpu_workers=budget :195） |
| 4 | persist（可选） | model + `upm_save_path` + `persist_upm` | .upm 文件 + manifest artifacts[] | :230 p2_upm_save；:224/:231/:241 p2_upm_close（所有权合同 p2_session.h:3——session 持有，恰一次释放） | :223-226（取消先 close model :224） | 无（串行 IO） |

数据所有权（p2_session.h:3 冻结）：session 持有 Coverage/
Observations/Model，统一经 p2_coverage_free（:144 RAII）/p2_upm_close
（:224/:231/:241）释放；manifest/last_error 由 SessionState 持有
（:22-41），destroy 唯一释放（:268-273）。

## 4 端口连接与 descriptor 占位对照

registry 现状**无 astrocs.p2.session module_id 的 descriptor**；五
函数经 P2Api（module_adapters.cpp:777-784 五静态委托）被占位
descriptor 工厂委托：phase2_descriptor()（:283-300，module_id=
astrocs.phase2.resample）注册段 :746-751；P2-006 canonical 7 节点链
descriptors（p2_coverage/sample/upm_fit/upm_apply/reject/integrate/
write，:561-694）注册段 :782-794——**无第二调度顺序**（注释
:779-781 冻结）。

| 本文件冻结词汇（会话端口） | descriptor 占位词汇（实测） | 对齐归属 |
|---|---|---|
| 输入 hips_paths / config_json | phase2.resample ports calibrated→resampled（:291-292）；7 链各 ports（:568-571/:587-590/:606-609/:625-628/:645-648/:664-667/:684-687） | P2-XX-INT |
| 输出 manifest_json / upm_model（可选 persist） | 占位 sci/alg/data/api/test 词汇：SCI-P2-RES-001/ALG-P2-RES-001/DATA-P2-RES/TEST-P2-RES-001（:294-298）；7 链各自占位（:572-576/:591-595/:610-614/:630-634/:649-653/:669-673/:688-692） | P2-XX-INT |
| 合同 ID：SCI-UPM-001+SCI-INT-001+SCI-REJ-001 / ALG-P2-SESSION-001 / DATA-P2-SESSION / API-P2-SESSION-001 / TEST-P2-SESSION-001 | 占位 api_id 一致同为 API-P2-001（:575 等） | P2-XX-INT |

**注记（冻结边界）**：descriptor 占位 ID 为编排层词汇（P1 registry
页先例 astrocs.phase1.session.md:20-23 同构表述），不得反向作为本
文件冻结依据；对齐由 P2-SESSION-INT 执行（台账 V7_1_STATIC_TASK_
LEDGER.csv:188 关键词 "module integration descriptor + typed ports"）。

## 5 调用序与生命周期

五函数契约（p2_session.h:17-27 注释冻结）：handle opaque、
owner=创建者、threadsafe:no（handle 级）、reentrant:yes（:16）。

1. **p2_session_create**（:56-67）：host null/struct_size 不符/
   abi_version≠ACS_ABI_VERSION_V1 → ACS_ERR_ABI_MISMATCH（:57-59）；
   out null → PARAM（:60）；new nothrow 失败 → NOMEM（:61-62）；
   manifest 初始化 kind="astrocs_phase2_session" + stages 空数组
   （:64）。
2. **p2_session_validate**（:69-98）：纯读无 IO；span 空/null → PARAM
   （:71）；坏 JSON（parse_error）→ PARAM（:73-79）；非 object →
   PARAM（:80）；缺必需键 `hips_paths`/`output_dir` → PARAM（:81-85）；
   类型门 hips_paths 非空 string[]、output_dir string（:86-92）、
   upm 为 object（:93-96）。**无 silent default**（p2_session.h:19）。
3. **p2_session_run**（:100-248）：parse（:103-110，失败文案
   "config parse failed (validate first)"）→ hips 装配（:111-114）→
   预算日志（:115-117）→ 四段（§3）。常量面（:184-194 冻结首版值）：
   robust_loss=0（huber）、snr_weight_mode=0（snr2_normalized）、
   huber_delta=1.345、max_iterations=100、tolerance=1e-6、
   sigma_floor=1e-3、support_power=1.0、use_ivar_weight=1、
   control_reliability=1.0、target_order=cov 实测值（:189）；config
   `upm.{max_iterations,huber_delta,smoothing_lambda}` 可覆盖
   （:196-202）；persist 条件 `persist_upm && upm_save_path`（:222）。
4. **p2_session_inspect**（:250-266）：状态补标（未 run 无错→"created"
   :253；未 run 有错→"failed"+error :254-257）；manifest.dump(2)
   （:258）；out 缓冲经 host->allocator.alloc 16 对齐（:260-261），
   调用方经 host free 释放。
5. **p2_session_destroy**（:268-273）：唯一释放对（delete s）。

诊断：`astrocs::phase2::last_error`（:277-282，脱敏摘要，handle 空→
空串；RT-008 CLI 合同经 P2Api::last_error :717 暴露）。

**output_dir 注入面**（config 生产者侧）：cli/parser.cpp:346-348 缺
`output_dir` 即拒（"config missing 'output_dir'"）；cli/runtime_client
.cpp:30-55 phase_config——run 格式自动补 output_dir（:49），phase2
格式直通**不自动补**（:51）；session validate 拒缺失（CLI 2，
runtime_client.cpp:29 注释冻结）。契约面：parse 拒平铺缺键、
passthrough 交会话拒——两道防线，语义一致。

## 6 trace 语义

- **manifest**（SessionState.manifest :25）：kind="astrocs_phase2_
  session"（:64）；stages[] 逐段 name/status + fail 时 rc/err、ok 时
  计数（stage() :36-40；coverage ok n_inputs/n_union_cells/target_
  order :145-147；sample ok n_obs/n_controls/accepted_obs/overlap_
  controls :174-176；upm_build ok control_count/observation_count/
  component_count/target_order/model_hash :211-216；persist ok path
  :239）；artifacts[]（persist 成功 push save_path :237-238）；顶层
  n_inputs/n_obs/status（:243-245）；error_kind "input"/"output"
  （:46/:234）；error（inspect :256）。
- **结构化日志**：host->logger 通道（log() :28-31）——run 起预算行
  （:115-117）、coverage ok cells=（:148）、sample ok obs=/overlap_
  controls=（:177-178）。
- **域内 provenance 衔接**：逐帧 P2HipsInputInfo（coverage.h:31-43）
  由 coverage 域填充（session 仅预填 hips_path :130-134），frame_id/
  provenance/拒绝统计等域内 trace **不上浮**会话 manifest（manifest
  仅计数汇总）；sampler 域 stderr 诊断（DISP-P2SMP-003）不经本层。
  manifest 为会话唯一外发 trace（P2Api 经 RT-008 SessionModule 捕获
  上报，module_adapters.cpp:105 注释）。

## 7 NODE-CALL 唯一性（符号×段矩阵）

**断言**：每个科学域 C API 恰被一个段调用；任意执行路径上每个
所有权符号恰调用一次（除两遍 probe/fill 协议与条件 persist）。

| 符号（声明锚） | coverage | sample | upm_build | persist | 合计/执行路径 |
|---|---|---|---|---|---|
| p2_coverage_build（coverage.h:57-59） | :125/:138 | — | — | — | 恰 2（probe/fill） |
| p2_coverage_free（coverage.h:61） | :143-144 RAII | — | — | — | 恰 1（含失败路径） |
| p2_sampler_default_config（sampler.h:60） | — | :154 | — | — | 恰 1 |
| p2_sample_controls（sampler.h:103-114） | — | :158/:167 | — | — | 恰 2（probe/fill） |
| p2_upm_build（upm.h:95-97） | — | — | :204 | — | 恰 1 |
| p2_upm_info（upm.h:110） | — | — | :210 | — | 恰 1（失败可容忍 :217-219） |
| p2_upm_save（upm.h:108） | — | — | — | :230 | 0..1（条件 :222） |
| p2_upm_close（upm.h:180） | — | — | — | :224/:231/:241 | 恰 1（正常 :241；persist 取消 :224；save 失败 :231） |

反断言（grep 实测 2026-09-10）：p2_session.cpp 不含 p2_integrate_
pixel / p2_reject_* / p2_upm_apply 族 / hips writer 任何符号——7 节点
链其余四域不在现状 4 段（§11.4 差距表）。承载测试=p2_ir_facade_test
.cpp:44-52（p2_coverage_build/p2_sample_controls/p2_upm_build 委托
断言）+ :57-62（段序）；call-count 门设计冻结见 §11.5 T1。

## 8 预算绑定与并发语义

- **并发合同**（p2_session.h:16 冻结）：reentrant:yes；threadsafe:no
  （handle 级——同一 handle 禁并发调用）；内部并行仅经预算注入域内
  C API。
- **预算注入**：sample sc.cpu_workers=host->budget.max_workers
  （:155）；upm uc.cpu_workers=host->budget.max_workers（:195）；
  worker 数零硬编码。sampler 域 0→1 归一（sampler.cpp:883）、1=串行
  reference；UPM blocks 并行语义归 ALG-UPM-001 域。
- **线程创建归零**：p2_session.cpp 无 std::thread/omp 原语（grep 实
  测）——并行全部在域内实现（sampler.cpp worker 池 / upm.cpp blocks），
  会话层单线程顺序编排四段。
- **预算注释矛盾**（DISP-P2SES-008，§11.3）：p2_session.h:4
  "sampler=1(串行 reference)" 与 :16 "内部并行仅 UPM blocks" 为陈旧
  表述，与实现 :155（sample 亦 budget 多 worker）矛盾——以实现为
  现状口径，合同文本归一归 P2-SESSION-IMPL。

## 9 取消语义

- **取消点=四段边界**（p2_session.h:22 冻结；实现 :120/:152/:181/
  :223-226）：每段入口检查 host->cancel.is_cancelled（:32-35）→
  stage 标 "cancelled" + 返回 ACS_ERR_CANCELLED。
- **upm 整模型不写半成品**：upm_build 段内无取消检查点（求解原子）；
  persist 取消先 p2_upm_close 释放再返回（:224，所有权合同优先）。
- **取消后 manifest 歧义**（DISP-P2SES-003，§11.3）：顶层 status 未
  设 cancelled，inspect 回落 "created"（:253）——整改归
  P2-SESSION-IMPL。
- 域内无检查点与 DISP-COV-005/P2-SMP ThreadLease 缺口同构（PHASE2_
  SAMPLER.md §11.1），段粒度取消为会话层唯一取消面。

## 10 已冻结禁改清单（本层不可接受变化）

1. 段序 coverage→sample→upm_build→persist 不可重排（p2_session.h:2；
   p2_ir_facade_test.cpp:54-63 断言）。
2. canonical 节点集 {coverage,sample,upm_build,persist}（:31-38 静态
   门；typed DAG 扩面归 P2-SESSION-IMPL，不回头改 4 段 trace 词汇）。
3. 五函数签名与 handle 所有权（p2_session.h:17-27；P2Api 委托面
   module_adapters.cpp:777-784）。
4. validate 无 silent default（p2_session.h:19；缺必需键/类型错
   →PARAM）。
5. 错误映射 rc=1→PARAM / rc=2→STATE（合同 §4）/ persist IO→ACS_ERR_
   IO（p2_session.cpp:2 注释、:47-49/:235）。
6. 每段恰一取消检查点=段边界；upm 段整模型原子（p2_session.h:22）。
7. 预算零硬编码：worker 数恒经 host->budget 注入（:155/:195）。
8. NODE-CALL 唯一性（§7 矩阵）；facade 不内联科学（p2_ir_facade_test
   .cpp:41-52）。

## 11 P2-SESSION-DOC 冻结附录（2026-09-10，SRC-P2-SESSION-001 源码实测）

### 11.1 返回码/错误映射（ACS_ERR_* 全清单）

| ACS_ERR_* | 触发（精确） | 锚（p2_session.cpp） |
|---|---|---|
| ACS_ERR_ABI_MISMATCH | host null / struct_size≠sizeof(astrocs_host_services_v1) / abi_version≠V1 | :57-59 |
| ACS_ERR_PARAM | out/hull、span 空、坏 JSON、非 object、缺必需键、类型错（validate 与 run 前置） | :60/:71-96/:102/:109/:252/:270 |
| ACS_ERR_NOMEM | SessionState new 失败；inspect host alloc 失败 | :61-62/:261 |
| ACS_ERR_CANCELLED | 四段边界取消 | :120/:152/:181/:226 |
| ACS_ERR_STATE | 域 rc=2（合同 §4 build fail，如 production 显式缺 ivar） | map_rc :48 |
| ACS_ERR_IO | p2_upm_save 失败（error_kind="output"） | :230-235 |
| ACS_ERR_INTERNAL | 域 rc 其他（非 0/1/2） | map_rc :49 |

域 rc→ACS_ERR 归并 map_rc（:43-50）：rc=0→OK；last_error 记
"<what> rc=<n>"；error_kind 标 "input"。域内 rc 细分语义见各域文档
（PHASE2_SAMPLER §11.1 等）；本层不重解释域返回码。

### 11.2 manifest 状态机与字段

```text
create 成功 → kind/stages[] → [validate 只读不改]
run 成功  → status="complete" + n_inputs/n_obs [+ artifacts]   :243-246
run 失败  → error_kind/last_error → inspect 补标 status="failed"
            + error                                             :253-257
run 取消  → stage="cancelled"；顶层 status 歧义（DISP-P2SES-003） :120/:253
inspect   → created | complete | failed（"cancelled" 未单列）    :253-257
```

字段全集：kind/stages[]（name/status/rc/err/计数，§6）/artifacts[]/
n_inputs/n_obs/status/error_kind/error。dtype/键集唯一权威=
DATA-P2-SESSION（§24，并行任务生成）；本节为实现现状锚定。

### 11.3 现状缺陷清单（DISP-P2SES-001..008，登记不改码，整改归 P2-SESSION-IMPL/TEST）

| ID | 锚（p2_session.cpp 除注明外） | 内容 | 整改归属 |
|---|---|---|---|
| DISP-P2SES-001 | :80-96 vs p2_session.h:19 | validate 实际**未拒未知键**（仅必需键/类型/upm 形状）；头注释"拒未知键/缺必需键"与实现出入——config 键白名单缺失，拼错键静默忽略 | P2-SESSION-IMPL（补键白名单或修正头注释）+ TEST 负面门（§11.5 T3） |
| DISP-P2SES-002 | :103-112 | run 未复用/未强制 validate：parse 后直接 doc["hips_paths"]（缺键或非 object 时 nlohmann type_error 未捕获，**异常可穿越 extern "C" ABI**；try 仅覆盖 parse） | P2-SESSION-IMPL（run 前置 validate 或 try/catch 全包） |
| DISP-P2SES-003 | :120/:152/:181/:223-226/:253 | 取消路径未设顶层 status（"cancelled"），ran=false 且 last_error 空 → inspect 回落 "created"，取消态不可辨识 | P2-SESSION-IMPL（status 状态机补 cancelled）+ TEST |
| DISP-P2SES-004 | :222 | persist_upm=true 而 upm_save_path 缺失 → **静默跳过 persist**（无告警无 stage 记录）；validate 不校验该组合，违背"无 silent default"精神 | P2-SESSION-IMPL（validate 增组合校验或 run 显式报错） |
| DISP-P2SES-005 | :184-194 | UPM 首版常量硬编码且仅 max_iterations/huber_delta/smoothing_lambda 可经 config 覆盖（:197-202）；tolerance/sigma_floor/support_power/use_ivar_weight/control_reliability/zero_anchor_weight 等不可配置（与 DISP-P2SMP-004 schema 缺口同构） | P2-SESSION-IMPL（config 键集扩面须同步 DATA-P2-SESSION §24） |
| DISP-P2SES-006 | :46/:234 | error_kind 分类粗粒度：map_rc 一律标 "input"，仅 persist 显式 "output"——HiPS 打开失败（IO 性质）亦标 input，诊断分流失真 | P2-SESSION-IMPL（观察级） |
| DISP-P2SES-007 | :126/:160 | 两遍 probe/fill 的**部分失败容错**未定义于合同：首查 rc≠0 且已得容量（cov.n_union_cells>0 / n_obs>0）时静默续跑 fill，首查错误被丢弃（coverage :126 `rc!=0 && n_union_cells==0` 才失败；sample :160 同型） | P2-SESSION-IMPL（合同化容错语义或收紧为 rc==0）+ TEST |
| DISP-P2SES-008 | p2_session.h:4/:16 vs :155/:195 | 预算绑定头注释陈旧（"sampler=1 串行 reference"/"内部并行仅 UPM blocks"）与实现（sample 亦 budget 多 worker）矛盾；API-P2-001 合同 §3 表述同步归口 | P2-SESSION-IMPL（合同文本归一） |

### 11.4 IMPL-COMPLETE 全链 artifact 目标合同与现状差距表

| # | artifact | 现状（2026-09-10 实测） | 差距归属 |
|---|---|---|---|
| 1 | astrocs_p2_session.dll（独立迁移目标） | 不存在（MISSING）；现状=静态库 astrocs_phase2_session（CMakeLists.txt:454-458）编入 astrocs 可执行（:501-506/:504） | P2-SESSION-IMPL |
| 2 | coverage 域产物（union MOC+target_order，P2CoverageResult 进程内） | 已实现（lib/phase2/src/coverage.cpp，ALG-COV-001 域） | 已存在（P2-COV 域） |
| 3 | sample 域产物（P2ControlObservation/P2ControlNode/P2SampleStats） | 已实现（sampler.cpp，ALG-P2-SMP-001 域） | 已存在（P2-SAMP 域） |
| 4 | upm 域产物（model 构建/持久化 p2_upm_build/save/info/close） | 已实现（lib/phase2/src/upm*.cpp，ALG-UPM-001 域） | 已存在（P2-UPM 域） |
| 5 | persist 段=HiPS writer 域马赛克写产品 | **不在会话**：p2_session persist 段仅 upm_save 单产物；upm_apply/reject/integrate/write 四域未编排（PUBLIC_API.md:1088-1090） | P2-SESSION-IMPL（typed DAG 扩面） |
| 6 | typed phase2 DAG 全链执行（"full execution no partial facade"） | 现状=4 段 facade 直调（p2_ir_facade_test.cpp:41-52 契合现状口径） | P2-SESSION-IMPL（台账 :187） |
| 7 | module integration descriptor + typed ports | registry 无 astrocs.p2.session descriptor；占位 descriptor 工厂委托（§4） | P2-SESSION-INT（台账 :188） |
| 8 | registry 页 astrocs.phase2.session.md / README / memory | 不存在；由并行任务生成（本任务仅本文件+module.yaml） | P2-SESSION-DOC 并行面/主 agent |
| 9 | TEST-P2-SESSION-001 可执行测试 | MISSING（不冒认） | P2-SESSION-TEST（台账 :186） |

### 11.5 TEST-P2-SESSION-DESIGN-001 冻结测试设计（可执行 TEST-P2-SESSION-001 由 P2-SESSION-TEST 落地，MISSING 如实登记）

锚定台账三条任务关键词（V7_1_STATIC_TASK_LEDGER.csv:186-188），
对拍先例 tests/unit/p1_ir_facade_test.cpp / p2_ir_facade_test.cpp
（同址 add_test 注册 tests/unit/CMakeLists.txt:354-358/:407-409）：

- **T1 call-count 唯一性**（:186 "add node call-count tests …
  coverage through hips writer each once"）：§7 矩阵逐符号断言——
  现状 4 段（p2_coverage_build×2/probe-fill、p2_sample_controls×2、
  p2_upm_build×1、p2_upm_close 恰 1、反断言零越段调用）+ typed DAG
  扩面后的 7 节点链全链 call-count（coverage→…→hips writer 每节点
  恰一次，P2-SESSION-IMPL 落地后启用该面）。静态 grep 门
  （p2_ir_facade_test.cpp:44-52 先例）+ 运行期门双层。
- **T2 typed DAG/full execution**（:187）：canonical 节点集断言
  （:31-38 先例）、段序不可重排（:57-62 先例）、facade 零内联科学
  （不含 UPM 求解循环/积分/排异符号）、四段 trace 与静态节点一致
  （:65-70 manifest 门先例）。
- **T3 descriptor 集成/validate 负面矩阵**（:188）：坏 JSON/非
  object/缺 hips_paths/缺 output_dir/hips_paths 空与非 string 项/
  upm 非对象/未知键（DISP-P2SES-001 现状口径：不拒——按 §11.3 登记断
  言，整改后翻转为拒）/output_dir 注入面（parser.cpp:346-348 拒、
  runtime_client.cpp:73 补/:51 直通不补）；取消注入点：四段边界各
  一（mock host cancel → ACS_ERR_CANCELLED + persist 段取消 model
  仍释放）；manifest 状态机：created→complete/failed 全路径 +
  取消歧义现状断言（DISP-P2SES-003）。
- **容差=装配层无科学数值容差**：config 透传/manifest 计数/artifacts
  全部 bitwise（EXPECT_EQ 级）；科学数值容差归各域 TEST（ALG-COV/
  ALG-P2-SMP/ALG-UPM 各自 TEST 面），本层禁止引入 epsilon。
- fixture：合成最小 HiPS 树（固定 seed，不提交大二进制；P2-SAMP
  fixture 先例）；host services 用假 host（logger/cancel/budget/
  allocator 可编程）。

### 11.6 SCI 层状态声明（本任务零 SCI 改动）

- 本会话共享引用 SCI-UPM-001 / SCI-INT-001 / SCI-REJ-001
  （docs/science/，FROZEN）——**不因本任务改动**（共享 SCI 引用不改
  动；P1-WCS-DOC SCI-WCS-001=共享 ASTROMETRY.md、P2-COV-DOC
  SCI-UPM-001/SCI-INT-001、P2-INT-DOC SCI-INT-001、P2-REJ-DOC
  SCI-REJ-001 先例）。现状 4 段实际消费面=SCI-UPM-001（coverage/
  sampler/upm 域均在其集合 SCI-UPM-001..010 内）；SCI-INT-001/
  SCI-REJ-001 为会话下游 typed DAG 扩面（upm_apply/reject/integrate/
  write 四域）的共享 SCI 引用，现状未由本会话直接调用——如实登记，
  不冒认调用面。
- SCI 公式语义不在此重复定义；两处冲突以 docs/science/ 为准并回改
  本文档（禁止反向）。
- descriptor 占位词汇（SCI-P2-RES-001 等，§4）与本页冲突时以本页为
  准；本节禁止被编排层词汇反向改写（astrocs.p2.session 由
  P2-SESSION-INT 对齐，不作冻结依据）。

## 12 关联 ID 映射（本文件承接）

- `ALG-P2-SESSION-001` = 本文档整体（DAG §3/端口 §4/生命周期 §5/
  唯一性 §7；矩阵 P2-SESSION 行 algorithm_id，INDEX.yaml path 绑定
  本文件——由并行任务登记，此处引用 id 不引节号）。
- `DATA-P2-SESSION`（DATA_SEMANTICS §24，生成中）= config 键集
  （§5）/manifest 字段（§11.2）契约面；冲突以 §24 为准。
- `API-P2-SESSION-001`（PUBLIC_API.md，生成中）= 五函数 C API +
  last_error 诊断面（§5）。
- `TEST-P2-SESSION-001`（MISSING）= §11.5 设计冻结的可执行载体
  （P2-SESSION-TEST 落地）；`TEST-P2-SESSION-DESIGN-001` = 本文件
  §11.5（双面登记不冒认）。
- `SRC-P2-SESSION-001` = lib/phase2_session/ 源码实测面（p2_session.h
  39 行 + p2_session.cpp 282 行 + 根 CMakeLists.txt:454-458/:501-506/
  :517-529），本文件全部行号锚的权威。
- `MOD-astrocs-phase2-session` = lib/phase2_session/module.yaml（本
  任务同批建立）+ registry 页（并行任务生成）。

## 13 追溯

- 实现：lib/phase2_session/p2_session.h（39 行）+ p2_session.cpp
  （282 行）；构建：静态库 astrocs_phase2_session（CMakeLists.txt
  :454-458）→ astrocs 可执行（:501-506）+ QA-001 严格警告层
  （:517-529）。
- 编排消费面：CLI 直调（CLI-005）与 RT-005/RT-008 SessionModule
  （module_adapters.cpp:777-784 P2Api，注册 :746-751/:782-794）。
- 对拍先例：lib/phase1_session/（P1-SESSION-DOC）+ registry 页
  astrocs.phase1.session.md；PHASE2_SAMPLER.md §11/§12 结构。
- 消费域：ALG-COV-001（PHASE2_COVERAGE.md）/ ALG-P2-SMP-001
  （PHASE2_SAMPLER.md）/ ALG-UPM-001（UPM_SOLVER.md）。
- 差距整改：§11.4（IMPL/INT）+ §11.3 DISP-P2SES-001..008；
  测试落地：P2-SESSION-TEST（§11.5）。
