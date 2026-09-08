# memory.md — astrocs.p2.session（P2-SESSION-DOC 冻结）

- 任务: P2-SESSION-DOC（V7_1_STATIC_TASK_LEDGER.csv:185 行，owner
  SA-P2-X24，wave W1，2026-09-10）——合同冻结层，不改生产源码，不
  commit。本目录 `lib/phase2_session/` 三件套（README r1 + module.yaml
  + memory.md）由 P2-SESSION-DOC 建立（module.yaml 由并行 SubAgent
  落位）。
- 落位: `lib/phase2_session/`（本目录，生产源同目录三件套，与
  lib/phase1_session/ P1-SESSION-DOC 先例同构；无需另建迁移目录，
  因 legacy 目录未被其他域合同占用）。
- 权威任务链（V7_1_STATIC_TASK_LEDGER.csv:185-188 实测）:
  P2-SESSION-DOC "freeze assembly contract; co-located
  README/module.yaml/SCI-ALG-DATA-API links; no root science formula
  change" → P2-SESSION-TEST "add node call-count tests; coverage
  through hips writer each once"（:186）→ P2-SESSION-IMPL "assemble
  typed phase2 DAG; full execution no partial facade"（:187）→
  P2-SESSION-INT "register phase2 pipeline; module integration
  descriptor + typed ports + local integration test"（:188）。
- 生产源锚（grep/read 实测，2026-09-10）: p2_session.cpp 282 行 +
  p2_session.h 39 行。h:4 头注释（"sampler=1" 陈旧表述=DISP-008 面）；
  h:16 reentrant:yes/threadsafe:no(handle 级)/内部并行仅 UPM blocks；
  h:19-20 validate 注释（"拒未知键"与实现出入=DISP-001）；h:22 取消点
  =阶段边界；五函数声明 h:17/:20/:23/:25/:27；last_error h:35（C++
  诊断面非 C ABI）。cpp: 头注释错误映射 :1-2；SessionState :22-41
  （log :28、cancelled :32、stage :36）；map_rc :43-50（rc=1→PARAM/
  rc=2→STATE 合同§4 build fail/其余 INTERNAL；error_kind 粗粒度=
  DISP-006）；create ABI 校验 :57-59；validate :69-98（必需键
  hips_paths/output_dir :81-85、类型 :86-92、upm object :93-96；
  **未拒未知键**=DISP-P2SES-001）；run :100-248（parse :103-112 异常
  面可穿 C ABI=DISP-002；coverage 两遍 probe/fill :125/:138、部分失败
  容错=DISP-007、inputs=nullptr 首查容量 :124；sample 预算绑定 :155、
  两遍 :158/:167、err 512B :166；upm 常量冻结 :184-194、config 覆盖
  三键 :196-202 无范围校验=DISP-005 面、build :204、info :210-219；
  persist 条件 :222 组合静默跳过=DISP-004、save :230、close :231、
  artifacts :237-238；model 所有权 close :241）；inspect :250-266
  （状态机 :253-256 取消态回落 created=DISP-003、host.allocator
  :260）；destroy :268-273；last_error :277-282。取消四段边界
  :120/:152/:181/:223（persist 先 close :224）。
- 构建锚: 根 CMakeLists.txt astrocs_phase2_session STATIC :454
  （:455-457 include lib/phase2/include、:458 link
  astrocs_contracts+astrocs_phase2）；astrocs 可执行链接 :501-506/:504；
  QA-001 严格警告名单 :517-529/:522。
- 消费链: lib/core/src/module_adapters.cpp——P2Api :711-718（五静态
  委托）、phase2_descriptor() :283-300（module_id=astrocs.phase2.
  resample 占位）、占位 descriptor 注册段 :746-751、canonical 7 节点链
  descriptors 定义 :561-694/注册段 :782-794（工厂统一
  make_session_module<P2Api>，无第二调度顺序；astrocs.phase2.session
  无独立 descriptor，占位词汇由 P2-XX-INT 对齐）。CLI: parser.cpp:344
  output_dir 必填；runtime_client.cpp:49 run 格式自动补 output_dir、
  :51 phase2 直通不补。
- known_defects（登记不改码，整改归 P2-SESSION-IMPL，权威清单=
  docs/algorithms/PHASE2_SESSION.md §11.3）:
  - DISP-P2SES-001: validate 未拒未知键（:80-96 仅必需键/类型/upm
    形状）与 h:19 注释出入；键白名单缺失拼错键静默忽略。
  - DISP-P2SES-002: run 未强制/复用 validate，parse 后直接
    doc["hips_paths"] 类型错异常未捕获可穿 extern "C" ABI（:103-112）。
  - DISP-P2SES-003: 取消路径未设顶层 status，inspect 回落 "created"
    取消态不可辨识（:120/:152/:181/:223-226/:253）。
  - DISP-P2SES-004: persist_upm=true 而 upm_save_path 缺失静默跳过
    persist（:222）。
  - DISP-P2SES-005: UPM 常量硬编码仅 max_iterations/huber_delta/
    smoothing_lambda 可覆盖（:184-194/:197-202）。
  - DISP-P2SES-006: error_kind 粗粒度（:46/:234，IO 性质失败标 input）。
  - DISP-P2SES-007: 两遍 probe/fill 部分失败容错未合同化（:126/:160）。
  - DISP-P2SES-008: 预算绑定头注释陈旧（h:4/:16 vs :155/:195）。
  - 另注（现状差距登记，非 DISP 编号）: output_dir validate 必填但
    run 不消费；输出目录语义归 P2-SESSION-IMPL 接 HIPS 写盘段
    （ALG §5 注入面 + DATA §24.1 + registry 页如实登记）。
- 验证锚（相邻证据，引用不冒认）: tests/unit/p2_ir_facade_test.cpp
  （P2-007，91 行，ctest p2_ir_facade 注册 tests/unit/CMakeLists.txt
  :407-409，build/ ctest 实测 Passed 2026-09-10）——IR 4 节点全链
  :33-40 / facade 委托 :44-53 / 阶段序 :56-66 / manifest trace :69-74。
  可执行 TEST-P2-SESSION-001 MISSING（归 P2-SESSION-TEST）。
- 合同 ID 登记（本任务冻结）: ALG-P2-SESSION-001
  （docs/algorithms/PHASE2_SESSION.md）/ DATA-P2-SESSION（§24）/
  API-P2-SESSION-001 / TEST-P2-SESSION-001（DORMANT 登记）/
  SRC-P2-SESSION-001（五导出符号）；SCI 层共享引用
  SCI-UPM-001/SCI-INT-001/SCI-REJ-001（FROZEN 零改动）。
