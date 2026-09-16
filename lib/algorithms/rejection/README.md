# astrocs.p2.rejection — Phase2 候选栈排异模块（P2-REJ）

> P2-REJ-DOC（2026-09-09，SA-P2-R22）新建模块页。合同三件套落位
> `lib/phase2_rej/`（README r1 + module.yaml + memory.md，CONTRACT_READY，
> entrypoint=MISSING）——迁移目标目录按 `lib/phase2_int/`（P2-INT-DOC，
> 其按 `lib/hips_p2/` P2-HIPS-DOC 先例）新建；`lib/phase2/` 三件套已被
> P2-COV（astrocs.p2.coverage）占用（lib/phase2/README.md r1，一目录一套
> README/module.yaml/memory.md，不可覆盖）。生产源
> `lib/phase2/src/rejection.cpp`（2076 行，根 CMakeLists astrocs_phase2
> 静态库成员 :336-346/:340）+ 唯一权威签名头
> `lib/phase2/include/astro/phase2/rejection.h`（329 行）；消费链
> `lib/phase2/tools/stage2.cpp`（1762 行，马赛克编排）与
> `lib/phase2/src/acr_kernels.cpp`（361 行，ACR 加速），均为本模块
> 合同消费者。

## 身份与合同

- MOD ID：`MOD-astrocs-phase2-reject`（registry 行 ID 沿用
  `MOD-astrocs-phase2-reject`）；module_id：`astrocs.p2.rejection`
  （MODULE_MIGRATION_MATRIX P2-REJ 行）；dll_target：
  `astrocs_p2_rejection.dll`（合同值，尚未存在，迁移归 P2-REJ-IMPL）。
- owner SA-P2-R22；depends_on_int=P2-UPM-INT;CPU-005；
  legacy_paths="lib/phase2 rejection sources"（均以
  MODULE_MIGRATION_MATRIX.csv P2-REJ 行为权威）。
- 合同链：SCI-REJ-001（docs/science/REJECTION.md，FROZEN T107
  2026-08-23，集合 SCI-REJ-001..008，legacy RJ-001..008；descriptor
  占位 SCI-P2-REJ-001⇒SCI-REJ-001 映射声明于
  docs/algorithms/PHASE2_REJECTION.md §11.5，占位 ID 不入合同）
  → ALG-P2-REJ-001（docs/algorithms/PHASE2_REJECTION.md）
  → DATA-P2-REJ（DATA_SEMANTICS §22）/ API-P2-REJ-001
  （PUBLIC_API Phase2 rejection 公共消费面节）→
  TEST-P2-REJ-001（登记面=设计冻结 VERIFIED，承载于
  docs/modules/registry/astrocs.phase2.reject.md §独立 synthetic
  验证节 + ALG 文档 §11.4 TEST-P2-REJ-DESIGN-001 F1-F8 容差；
  可执行测试 MISSING 归 P2-REJ-TEST，不冒认）。

## 职责（摘要，权威=本 README §3/§4）

- 每像素候选栈排异决策：eligibility strided gather
  （`p2_eligibility_filter` / `p2_collect_candidate_stack`，
  source_indices 权威稳定映射）→ planning AUTO 一次解析
  （`p2_reject_plan_resolve`：nominal n<6→PERCENTILE、6..15→
  WINSORIZED、>15→LINEAR_FIT，rejection.cpp:1072-1078）→
  10 显式方法核（`p2_reject_stack_ex` 唯一生产 kernel 入口；
  per-sample reason u8 0..3 与 stack status int 0..7 分离）→
  large_scale 结构生长后处理（`p2_large_scale_apply`，trail 扩张
  只增不减）。
- 本层为阈值/迭代权威锚定（rejection.cpp:1-11 冻结头注释：
  排异阈值/迭代冻结锚点 SCI-REJ-*/ALG-REJ-001..008）。
- 非职责：不合并/积分（P2-INT 域 `p2_integrate_pixel`）、权重策略
  （weights 数组外置，Stage2 weight_mode 构造 numeric weights）、
  像素外结构重建、session 依赖、瞬变/卫星语义区分（SCI-REJ-001
  §1 非目标）。

## 关键合同事实

- 判向冻结：rejected_low=低于 lower threshold、rejected_high=高于
  upper threshold（rejection.h:20-21，禁止用原始值正负号判向；
  decision 作用于 working stack，accepted mask 应用于原始值）。
- 工作域归一（P2_NORMALIZE_NONE/MEDIAN_CENTER/MEDIAN_SCALE，
  |median| floor 1e-12）：decision 在 working stack，mask 应用回
  原始 calibrated 科学值（rejection.h:7-11 分层注释）。
- underdetermined_n=2（plan 解析默认 :1048）；n≤2 白名单全接受、
  reason=UNDERDETERMINED、recall=0 显式（:1738-1745，SCIENCE_FREEZE
  注释 :1838）。
- AUTO kernel 永不接收：AUTO 等非法方法进入 kernel →
  INVALID_METHOD（:1689，rejection.h:56 枚举注释）。
- 兼容门 MIN_SAMPLES compat：`p2_reject_stack` 旧接口 min_samples 由
  sigma_low/sigma_high/max_iterations 换算，仅测试/旧调用，
  rejection.h:299 冻结注释"生产 Stage2 不再调用"；空栈（n==0）在
  ex/compat 两路径均返回 MIN_SAMPLES（:1706/:1869）。
- 方法×normalization 合法性门：PERCENTILE×norm≠MEDIAN_CENTER /
  RCR×norm≠NONE → INVALID_CONFIGURATION（:1722-1744）。
- 四概念分离 signal/weights(ivar)/support/mask 中本层职责=mask
  判定：accepted 标志不入权重式；support 仅作资格门禁
  （eligibility 层消费），不作科学权重；ivar weights 由调用方
  Stage2 外置构造（weight_mode=2），kernel 不知 support/quality
  （rejection.h:7-9 分层）。
- large_scale astrocs.large_scale_rejection.v1 默认关闭：
  enabled=0、min_structure_pixels=8、低/高侧半径 2/2
  （rejection.h:142-148）；DFS 连通 Chebyshev 8 邻域、只增不减
  （mask 1→0 禁止）；cosmic 紧凑结构不生长（min_structure 门 +
  V17LargeScaleGrowsTrailNotCosmic :4798 冻结）。
- 并发：像素级纯函数，与 worker 数无关 bitwise（像素内候选索引
  固定序归约；并行轴在调用方 stage2.cpp:1288 / acr_kernels.cpp:218
  OMP；Stage2 per-thread 统计按 thread id 定序归并
  :1305-1313）→ CON-006 deterministic 决策合同。

## 验证

可执行 `TEST-P2-REJ-001` MISSING（P2-REJ-TEST 建立，不冒认）；
登记面=TEST-P2-REJ-DESIGN-001 设计冻结 VERIFIED，承载于
docs/modules/registry/astrocs.phase2.reject.md §独立 synthetic
验证节 + ALG-P2-REJ-001 §11.4 F1-F8 容差（F1-F6/F8 bitwise/枚举/
计数精确、F7 rtol 1e-12、large_scale mask 精确）。现状相邻证据
（引用不冒认）：lib/phase2/tests/synthetic_gate.cpp Phase2Rejection
组 :2639-2725 + G6 组 :2779-2863 + V15-V17 组 :4138-4864
（V16GatherStridedFp32Fp64 :4592、V17InvalidMethodStatus :4763、
V17LargeScale* :4798-4864）；tests/backend/
test_p2004_reject_integrate.py（P2-004 生产 Oracle）与
tests/unit/p2_rejection_test.cpp（P2-005）。

## 链接

- README/module.yaml/memory.md：`lib/phase2_rej/`（本目录）
- SCI：docs/science/REJECTION.md（SCI-REJ-001，FROZEN T107
  2026-08-23，零改动）
- ALG：docs/algorithms/PHASE2_REJECTION.md（ALG-P2-REJ-001）
- DATA：docs/contracts/DATA_SEMANTICS.md §22（DATA-P2-REJ）
- API：docs/contracts/PUBLIC_API.md API-P2-REJ-001；
  API-P2-001（编排层既有）
- 模块页：docs/modules/registry/astrocs.phase2.reject.md

## 已知限制（DISP-P2REJ，登记不改码，整改归 P2-REJ-IMPL/TEST）

| ID | 摘要 | 源锚 |
|---|---|---|
| DISP-P2REJ-001 | h:118 percentile low_fraction 注释"默认 0.1"与实现/SCI 默认 0.2 表述矛盾（以 ALG §5 冻结表 0.2 为准） | rejection.h:118 |
| DISP-P2REJ-002 | SCI §8 退化态 "NO_CANDIDATES" 与实现空栈=MIN_SAMPLES（:1706/:1869）冲突；NO_CANDIDATES 属积分域 P2IntegrateStatus（integrate.h:46），排异域无该状态 | docs/science/REJECTION.md:89 |
| DISP-P2REJ-003 | SCI/REJECTION_ALGORITHMS 行号锚漂移；行号权威=ALG-P2-REJ-001 §3 逐符号实测锚 | docs/science/REJECTION.md |
| DISP-P2REJ-004 | minmax 比较器 value-only（sort :1667-1669，比较器仅 w[a]<w[b]），tie-break 未显式冻结，等值样本 permutation 不变性未承诺 | rejection.cpp:1667-1669 |
