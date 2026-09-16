# astrocs.p2.integration — Phase2 逐像素加权积分模块（P2-INT）

> P2-INT-DOC（2026-09-09，SA-P2-I23）新建模块页。合同三件套落位
> `lib/phase2_int/`（README r1 + module.yaml + memory.md，CONTRACT_READY，
> entrypoint=MISSING）——迁移目标目录按 `lib/hips_p2/`（P2-HIPS-DOC）
> 先例新建；`lib/phase2/` 三件套已被 P2-COV（astrocs.p2.coverage）占用，
> 不可覆盖。生产源 `lib/phase2/src/integrate.cpp`（76 行，根 CMakeLists
> astrocs_phase2 静态库成员 :336-346/:344）+ 唯一权威签名头
> `lib/phase2/include/astro/phase2/integrate.h`（74 行）；消费链
> `lib/phase2/tools/stage2.cpp`（马赛克编排）与
> `lib/phase2/src/acr_kernels.cpp`（ACR 加速），均为本模块合同消费者。

## 身份与合同

- MOD ID：`MOD-astrocs-phase2-integrate`（registry 行 ID 沿用
  `MOD-astrocs-phase2-integrate`）；module_id：`astrocs.p2.integration`
  （MODULE_MIGRATION_MATRIX P2-INT 行）；dll_target：
  `astrocs_p2_integration.dll`（合同值，尚未存在，迁移归 P2-INT-IMPL）。
- owner SA-P2-I23；depends_on_int=P2-REJ-INT;P1-NOISE-INT;CPU-005；
  legacy_paths="lib/phase2 integration sources"。
- 合同链：SCI-INT-001（docs/science/INTEGRATION.md，FROZEN T108
  2026-08-23，集合 SCI-INT-001/002/004/008）→ ALG-P2-INT-001
  （docs/algorithms/PHASE2_INTEGRATION.md）→ DATA-P2-INT
  （DATA_SEMANTICS §21）/ API-P2-INT-001（PUBLIC_API）→
  TEST-P2-INT-001（登记面=设计冻结 VERIFIED，ALG 文档 §11.4，COV/HIPS
  先例；可执行测试 MISSING 归 P2-INT-TEST）。

## 职责（摘要，权威=本 README §3/§4）

- 逐像素加权积分 reducer：单像素候选栈 → `signal = Σ wᵢxᵢ / Σ wᵢ`
  （仅 eligible ∧ finite ∧ 正权重样本；候选索引固定序单栈归约）。
- 五态显式 status 状态机：OK / NO_CANDIDATES / ALL_REJECTED /
  ZERO_VALID_WEIGHT / INVALID_INPUT（integrate.h:45-51；零权重图
  不做除法——wsum==0 时 :65-69 分支先行，无除零路径）。
- support 唯一 canonical reducer = max(accepted support)（integrate.h:17
  冻结语义）；sup_max 实现现状含 DISP-P2INT-001 缺陷（见 §6）。
- 输入防御面：`p2_validate_candidate_weights`（integrate.cpp:10-17，
  null→0；NaN/Inf/负→1）供 Stage2 权重构造后预检。
- 非职责：权重策略（上游 Stage2 构造 numeric weights，policy/reducer
  分离已冻结）、排异判据（P2-REJ）、 eligibility gather、马赛克编排、
  逆归一化（area/flux，Stage2 域）、P3。

## 关键合同事实

- 四概念分离（matrix 专项）：signal（加权积分输出）、variance/ivar
  （输入侧逐帧产品权重语义，仅作权重消费）、support（几何覆盖
  [0,1]，仅 eligibility/canonical reducer 消费，禁作科学权重）、
  mask（accepted 标志，不入权重式）。
- 权重 = 外部 numeric weights（`weights` 可空 → 等权 fill 1.0）；
  零权重合同：`w==0` 合法但零贡献（:49 continue，计入
  n_accepted/n_finite，不计 n_used/n_positive_weight）；权重负/
  非有限 → INVALID_INPUT；`p2_validate_candidate_weights` 返回
  0/1（合规/违规）预检。
- all rejected / zero weight 显式区分：`n_accepted==0` →
  ALL_REJECTED；`n_accepted>0 ∧ n_positive_weight==0` →
  ZERO_VALID_WEIGHT（:65-69，synthetic_gate V17StatusesExplicit
  :4746-4760 冻结）。
- 并发：像素级纯函数，无内部并行；像素间并行在调用方（Stage2
  :1288 / ACR :218 OMP）；像素内候选索引固定序归约 + Stage2
  per-thread 统计按 thread id 定序归并（:1305-1313）→ 输出与
  worker 数无关（CON-006/parallel reduction 合同，无浮点重结合）。
- known_defects：DISP-P2INT-001（sup_max 漏计零权重 accepted 样本）、
  DISP-P2INT-002（INTEGRATION.md:58 表述面）——登记不改码，整改归
  P2-INT-IMPL/TEST。

## 验证

可执行 `TEST-P2-INT-001` MISSING（P2-INT-TEST 建立）；登记面=设计冻结
VERIFIED；设计内容与容差来源=ALG-P2-INT-001（PHASE2_INTEGRATION.md
§11.4，五项冻结容差）。现状相邻证据（引用不冒认）：
lib/phase2/tests/synthetic_gate.cpp Phase2Integrate 组（零权重合同
:4702-4715 / support reducer :4737-4738 / 五态 :4741-4760）、
tests/backend/test_p2004_reject_integrate.py（P2-004 生产 Oracle
DRIVER_SRC 积分段 :66-126，含 max(accepted support) 注释 :124）。

## 链接

- README/module.yaml/memory.md：`lib/phase2_int/`（本目录）
- SCI：docs/science/INTEGRATION.md（FROZEN，零改动）
- ALG：docs/algorithms/PHASE2_INTEGRATION.md（ALG-P2-INT-001）
- DATA：docs/contracts/DATA_SEMANTICS.md §21（DATA-P2-INT）
- API：docs/contracts/PUBLIC_API.md API-P2-INT-001
- 模块页：docs/modules/phase2_int.md；
  docs/modules/registry/astrocs.phase2.integrate.md
