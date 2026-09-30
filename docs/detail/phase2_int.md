# astrocs.p2.integration — Phase2 逐像素加权积分模块页

> 上游：docs/ASTROCS_DESIGN.md §8.5（模块与 ABI）

> 合同三件套落位
> `lib/algorithms/integration/`（README + module.yaml + memory.md，CONTRACT_READY，
> entrypoint 未落地）；落位规则见 docs/detail/README.md。生产源 `lib/algorithms/coverage/src/integrate.cpp`（76 行，根 CMakeLists
> astrocs_phase2 静态库成员；lib/algorithms/coverage/CMakeLists.txt
> 同文件的兼容 target）+ 签名头正本
> `lib/algorithms/coverage/include/astro/phase2/integrate.h`（74 行）。

## 身份与合同

- MOD ID：`MOD-astrocs-phase2-integrate`（registry 行 ID 沿用
  `MOD-astrocs-phase2-integrate`，本页=registry
  astrocs.phase2.integrate.md 同步合同页）；module_id：
  `astrocs.p2.integration`（合同值）；dll_target：
  `astrocs_p2_integration.dll`（合同值，尚未存在；迁移目标未落地）。
- owner SA-P2-I23；depends_on_int=P2-REJ;P1-NOISE;CPU-005；
  legacy_paths="lib/algorithms/coverage integration sources"。
- 合同链：SCI-INT-001（docs/science/INTEGRATION.md，FROZEN，零改动；
  SCI-P2-INT-001⇒SCI-INT-001 映射声明见 ALG
  文档 §11.5）→ ALG-P2-INT-001（docs/science/algorithms/PHASE2_INTEGRATION.md）
  → DATA-P2-INT（DATA_SEMANTICS §21）/ API-P2-INT-001（PUBLIC_API）
  → TEST-P2-INT-001（登记面 = ALG §11.4 设计冻结；可执行测试待建）。

## 职责（摘要，权威=lib/algorithms/integration/README.md）

- 逐像素加权积分 reducer：`signal = Σ wᵢxᵢ / Σ wᵢ`（仅 eligible ∧
  正权重样本，候选索引固定序）；support canonical reducer
  （max(accepted support)，integrate.h 冻结语义；零权重 accepted 样本
  计入 max、全零权仍发布，integrate.cpp；约束与回归门 =
  ALG-P2-INT-001 §11.3）。
- 五态显式 status（OK/NO_CANDIDATES/ALL_REJECTED/ZERO_VALID_WEIGHT/
  INVALID_INPUT）；wsum==0 不做除法；零权重=合法零贡献。
- 输入防御面 p2_validate_candidate_weights（integrate.cpp）。
- 非职责：权重策略（Stage2 构造，policy/reducer 分离）、排异
  （P2-REJ）、eligibility gather、马赛克编排、逆归一化（Stage2 域）、
  内部并行（像素级纯函数，像素间并行在调用方）。

## 关键合同事实

- 四概念分离：signal/variance-ivar（输入侧权重语义）/support（几何覆盖，
  禁作科学权重）/mask（不入权重式）。
- 并发/确定性：像素内候选索引固定序归约；像素间并行在调用方
  （stage2.cpp / acr_kernels.cpp OMP）；per-thread 统计
  thread id 定序归并（stage2.cpp）→ 输出与 worker 数无关。
- known_defects：无未决项。原 `DISP-P2INT-001`（sup_max 漏计零权重 accepted
  样本）与 `DISP-P2INT-002`（support 表述矛盾）均已落地：现行实现 = sup_max
  更新位于权重分支之前（`integrate.cpp`），零权重 accepted 样本**进入**
  max，全零权（ZERO_VALID_WEIGHT）仍发布该 max、ALL_REJECTED 保持 0；
  文档面已同步（`docs/science/INTEGRATION.md`、`DATA_SEMANTICS.md` §21.2/§21.5、
  `PUBLIC_API.md` API-P2-INT-001 节）。正本 = ALG-P2-INT-001 §11.3（约束 + 回归门
  `eng/tests/unit/p2_output_semantics_test.cpp` 4b/4c/4d）。

## 验证

可执行 `TEST-P2-INT-001` 待建；登记面 = 设计冻结；设计内容与容差来源 =
ALG-P2-INT-001（PHASE2_INTEGRATION.md §11.4：常量场/零权重/五态/支撑/
NumPy rtol 1e-12/并行 1..N 线程 bitwise）。现状相邻证据（引用不冒认）：
synthetic_gate.cpp Phase2Integrate 组、weight policy 门
、ACR 等价；
eng/tests/backend/test_p2004_reject_integrate.py（DRIVER_SRC 段）。

## 链接

- 合同三件套：`lib/algorithms/integration/`（README/module.yaml/memory.md）
- registry 页：docs/detail/registry/astrocs.phase2.integrate.md
- SCI：docs/science/INTEGRATION.md（FROZEN，零改动）
- ALG：docs/science/algorithms/PHASE2_INTEGRATION.md；DATA：§21；
  API：PUBLIC_API API-P2-INT-001
