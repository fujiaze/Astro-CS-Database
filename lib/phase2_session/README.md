# astrocs.p2.session — Phase2 进程内装配会话（P2-SESSION）

> P2-SESSION-DOC（2026-09-10，SA-P2-X24）合同冻结新建模块页。合同三件套
> 落位 `lib/phase2_session/`（README r1 + module.yaml + memory.md，
> CONTRACT_READY，entrypoint=MISSING）——本目录即生产源所在目录（源码
> 同目录三件套，与 `lib/phase1_session/` P1-SESSION-DOC 先例同构）。
> 生产源 `lib/phase2_session/p2_session.cpp`（282 行，根 CMakeLists.txt
> astrocs_phase2_session 静态库 :454-458 实测）+ 唯一权威签名头
> `lib/phase2_session/p2_session.h`（39 行）；消费链
> `lib/core/src/module_adapters.cpp`（P2Api :711-720 工厂委托）与
> `cli/runtime_client.cpp`（:28 passthrough 不自动补 output_dir、:51
> phase2 config 直通）、`cli/parser.cpp`（:344 output_dir 必填校验）。
> 装配算法权威=ALG-P2-SESSION-001（docs/algorithms/PHASE2_SESSION.md）。

## 身份与合同

- MOD ID：`MOD-astrocs-phase2-session`；module_id：`astrocs.p2.session`
  （本任务冻结的合同模块词汇）；dll_target：`astrocs_p2_session.dll`
  （合同值，尚未存在，迁移归 P2-SESSION-IMPL）。
- owner SA-P2-X24；任务链 P2-SESSION-DOC→TEST（node call-count
  tests: coverage through hips writer each once）→IMPL（assemble typed
  phase2 DAG, full execution no partial facade）→INT（register phase2
  pipeline, module integration descriptor + typed ports）（均以
  V7_1_STATIC_TASK_LEDGER.csv:185-188 为权威）。
- 合同链：SCI-UPM-001 / SCI-INT-001 / SCI-REJ-001（docs/science/
  PHASE2_UPM.md、INTEGRATION.md、REJECTION.md，均 FROZEN，共享引用
  不改动；本任务零 SCI 改动）→ ALG-P2-SESSION-001
  （docs/algorithms/PHASE2_SESSION.md）→ DATA-P2-SESSION
  （DATA_SEMANTICS §24）/ API-P2-SESSION-001（PUBLIC_API「Phase2
  装配会话 C API」节）→ TEST-P2-SESSION-001（可执行测试 MISSING，
  P2-SESSION-DOC 登记归 P2-SESSION-TEST，不冒认；设计冻结面=
  ALG 文档 TEST-DESIGN 节 + registry 页）；编排上游 API-P2-001
  （docs/api/PHASE2_API_V1.md，FROZEN 引用不改动）。

## 职责（摘要，权威=ALG-P2-SESSION-001）

- Phase2 进程内装配会话：config JSON 校验（`p2_session_validate` 拒
  缺必需键 hips_paths/output_dir）→ `p2_session_run` 四段编排：
  coverage（p2_coverage_build 两遍 probe/fill :125/:138）→ sample
  （p2_sampler_default_config + p2_sample_controls 两遍 :158/:167）→
  upm_build（P2UpmBuildConfig 冻结常量 :184-194 + p2_upm_build :204）→
  persist（可选，p2_upm_save :230，条件 persist_upm && upm_save_path
  :222；组合静默跳过=DISP-P2SES-004）→ `p2_session_inspect` manifest
  JSON（stages/artifacts/status 状态机 :250-266）→ `p2_session_destroy`。
- 本层为装配序/生命周期/错误映射/取消/预算绑定的权威锚定
  （p2_session.cpp:1-2 冻结头注释：错误映射 rc=1→ACS_ERR_PARAM、
  rc=2→ACS_ERR_STATE[合同 §4 build fail]、其余→IO/INTERNAL）。
- 非职责：不写 HiPS 产物（现状四段无 hips writer 段，"coverage
  through hips writer"全链归 P2-SESSION-IMPL 如实差距）、不做马赛克
  编排（stage2.cpp 域）、无第二调度顺序（registry 工厂委托 P2Api 与
  phase2.resample 占位 descriptor 同一调度）、不定义科学公式（委托
  既有冻结 C API）。

## 关键合同事实

- 取消=阶段边界（p2_session.h:22 冻结注释；四段边界
  p2_session.cpp:120/:152/:181/:223-227；persist 段取消须先 p2_upm_close
  :224；upm 整模型不写半成品；取消后顶层 status 歧义=DISP-P2SES-003）。
- 并发：session 自身无线程原语；threadsafe:no（handle 级）+
  reentrant:yes（p2_session.h:16）；worker 预算唯一来源 host.budget
  （sc.cpu_workers=max_workers :155、uc.cpu_workers :195，禁硬编码）；
  upm 内部并行=blocks(budget)（:180 注释）。
- upm 装配常量冻结（:184-194）：robust_loss=0(huber)、
  snr_weight_mode=0(snr2_normalized)、huber_delta=1.345、
  max_iterations=100、tolerance=1e-6、sigma_floor=1e-3、
  support_power=1.0、use_ivar_weight=1、control_reliability=1.0；
  config `upm` 对象仅可覆盖 max_iterations/huber_delta/smoothing_lambda
  （:196-202，覆盖键无范围校验含于 DISP-P2SES-005 面内）。科学公式
  语义归 SCI-UPM-001/ALG-UPM-001，本层不改。
- model 所有权合同（:241 注释）：session 持有，p2_upm_close 释放；
  persist 失败亦 close（:231）。
- manifest 状态机：created→complete/failed（inspect :253-256）；
  stages[] 逐段 name/status/rc/err/计数；artifacts[] persist 成功追加
  save_path（:237-238）；error_kind=input|output（:46/:234）。
- config 键集（validate :80-96）：必需 hips_paths（非空 string 数组）/
  output_dir（string）；可选 upm（object）、persist_upm（bool）、
  upm_save_path（string）。已知差距（登记不改码，DISP-P2SES-001..008
  权威清单=ALG-P2-SESSION-001 §11.3）：001 validate 未拒未知键（仅查
  必需键/类型，与 p2_session.h:19 注释有出入）；002 run 未强制
  validate、doc["hips_paths"] 类型错异常可穿 extern "C" ABI（:103-112）；
  003 取消路径未设顶层 status、inspect 回落 "created"（:253）；004
  persist_upm=true 而 upm_save_path 缺失静默跳过（:222）；005 UPM 常量
  硬编码仅 3 键可覆盖；006 error_kind 粗粒度（:46/:234）；007 两遍
  probe/fill 部分失败容错未合同化（:126/:160）；008 预算绑定头注释
  陈旧（h:4/:16 vs :155/:195）。另注（现状差距登记，非 DISP 编号）：
  output_dir validate 必填但 run 不消费（:81-85 校验、run 无引用；
  输出目录语义归 P2-SESSION-IMPL 接 HIPS 写盘段，ALG §5 注入面+
  DATA §24.1 如实登记）。
- 错误面：ACS_ERR_PARAM / ACS_ERR_STATE / ACS_ERR_IO / ACS_ERR_INTERNAL
  / ACS_ERR_ABI_MISMATCH（:57-59）/ ACS_ERR_NOMEM（:61-62/:261）/
  ACS_ERR_CANCELLED（:120/:152/:181/:226）；诊断经 host->logger
  （:28-31），无 stderr 直写；last_error=astrocs::phase2::last_error
  （:277-282）。

## Registry 现状（如实登记）

- astrocs.phase2.session 无独立 descriptor（module_adapters.cpp 全文
  实测）；P2 session 五函数经 `phase2_descriptor()`
  （module_id=astrocs.phase2.resample 占位，:283-300）与 canonical
  Phase2 IR 7 节点链 descriptors（p2_coverage/sample/upm_fit/
  upm_apply/reject/integrate/write，定义 :561-694、注册段 :782-794）
  的工厂 make_session_module<P2Api>（:711-718）委托暴露（与 P1 8
  descriptor 同构，无第二调度顺序）。占位词汇（SCI-P2-RES-001/
  ALG-P2-RES-001/DATA-P2-RES/TEST-P2-RES-001 等）由 P2-XX-INT 对齐
  astrocs.p2.session，不得反向作为冻结依据。

## 测试

- 现状：tests/unit/p2_ir_facade_test.cpp（P2-007，91 行，ctest
  `p2_ir_facade`）= 静态 facade 断言（IR 4 节点全链声明 :33-40、
  facade 委托 p2_* 不内联科学 :44-53、阶段序 coverage→sample→upm
  :56-66、manifest trace stages/kind :69-74）。
- 可执行 TEST-P2-SESSION-001（运行时 node call-count：coverage
  through hips writer each once）MISSING，归 P2-SESSION-TEST 建立
  （登记面=ALG 文档 TEST-DESIGN 节冻结容差，不冒认）。
