# astrocs.p2.integration — Phase2 逐像素加权积分模块页（P2-INT）

> P2-INT-DOC（2026-09-09，SA-P2-I23）新建模块页。合同三件套落位
> `lib/phase2_int/`（README r1 + module.yaml + memory.md，CONTRACT_READY，
> entrypoint=MISSING）——迁移目标目录按 `lib/hips_p2/`（P2-HIPS-DOC）
> 先例新建；`lib/phase2/` 三件套已被 P2-COV（astrocs.p2.coverage）占用，
> 不可覆盖。生产源 `lib/phase2/src/integrate.cpp`（76 行，根 CMakeLists
> astrocs_phase2 静态库成员 :336-346/:344；lib/phase2/CMakeLists.txt:42-49
> 兼容 target :49 同文件）+ 唯一权威签名头
> `lib/phase2/include/astro/phase2/integrate.h`（74 行）。

## 身份与合同

- MOD ID：`MOD-astrocs-phase2-integrate`（registry 行 ID 沿用
  `MOD-astrocs-phase2-integrate`，本页=registry
  astrocs.phase2.integrate.md 同步合同页）；module_id：
  `astrocs.p2.integration`（MODULE_MIGRATION_MATRIX P2-INT 行）；
  dll_target：`astrocs_p2_integration.dll`（合同值，尚未存在，迁移归
  P2-INT-IMPL）。
- owner SA-P2-I23；depends_on_int=P2-REJ-INT;P1-NOISE-INT;CPU-005；
  legacy_paths="lib/phase2 integration sources"。
- 合同链：SCI-INT-001（docs/science/INTEGRATION.md，FROZEN T108
  2026-08-23，零改动；SCI-P2-INT-001⇒SCI-INT-001 映射声明见 ALG
  文档 §11.5）→ ALG-P2-INT-001（docs/algorithms/PHASE2_INTEGRATION.md）
  → DATA-P2-INT（DATA_SEMANTICS §21）/ API-P2-INT-001（PUBLIC_API）
  → TEST-P2-INT-001（MISSING；登记面=ALG §11.4 设计冻结 VERIFIED，
  COV/HIPS 先例；可执行测试归 P2-INT-TEST）。

## 职责（摘要，权威=lib/phase2_int/README.md）

- 逐像素加权积分 reducer：`signal = Σ wᵢxᵢ / Σ wᵢ`（仅 eligible ∧
  正权重样本，候选索引固定序）；support canonical reducer
  （max(accepted support)，integrate.h:17 冻结语义；现状实现含
  DISP-P2INT-001 缺陷，ALG §11.3 如实登记）。
- 五态显式 status（OK/NO_CANDIDATES/ALL_REJECTED/ZERO_VALID_WEIGHT/
  INVALID_INPUT）；wsum==0 不做除法；零权重=合法零贡献。
- 输入防御面 p2_validate_candidate_weights（:10-17）。
- 非职责：权重策略（Stage2 构造，policy/reducer 分离）、排异
  （P2-REJ）、eligibility gather、马赛克编排、逆归一化（Stage2 域）、
  内部并行（像素级纯函数，像素间并行在调用方）。

## 关键合同事实

- 四概念分离（matrix 专项）：signal/variance-ivar（输入侧权重语义）/
  support（几何覆盖，禁作科学权重）/mask（不入权重式）。
- 并发/确定性：像素内候选索引固定序归约；像素间并行在调用方
  （stage2.cpp:1288 / acr_kernels.cpp:218 OMP）；per-thread 统计
  thread id 定序归并（stage2.cpp:1305-1313）→ 输出与 worker 数无关。
- known_defects：DISP-P2INT-001（sup_max 漏计零权重 accepted 样本，
  bughunt R3-A，integrate.cpp:54-55 vs integrate.h:17，保守方向）；
  DISP-P2INT-002（INTEGRATION.md:58 vs integrate.h:17 表述矛盾，文档
  级）。登记不改码，整改归 P2-INT-IMPL/TEST。

## 验证

可执行 `TEST-P2-INT-001` MISSING（P2-INT-TEST 建立）；登记面=设计冻结
VERIFIED；设计内容与容差来源=ALG-P2-INT-001（PHASE2_INTEGRATION.md
§11.4：常量场/零权重/五态/支撑/NumPy rtol 1e-12/并行 1..N 线程
bitwise）。现状相邻证据（引用不冒认）：synthetic_gate.cpp
Phase2Integrate 组（:4690-4760）、weight policy 门（:2941-2990）、
W9 ACR 等价（:3021-3160）；tests/backend/test_p2004_reject_integrate.py
（DRIVER_SRC :18-141）。

## 链接

- 合同三件套：`lib/phase2_int/`（README/module.yaml/memory.md）
- registry 页：docs/modules/registry/astrocs.phase2.integrate.md
- SCI：docs/science/INTEGRATION.md（FROZEN，零改动）
- ALG：docs/algorithms/PHASE2_INTEGRATION.md；DATA：§21；
  API：PUBLIC_API API-P2-INT-001
