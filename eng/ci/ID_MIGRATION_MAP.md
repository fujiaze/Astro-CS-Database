# eng/ci/checks.json ID 收敛迁移映射（人读摘要）

> 本文件由 `eng/ci/gen_id_migration_map_doc.py` 从 `eng/ci/id_migration_map.json` 生成，请勿手改；改 JSON 后重跑生成器：`python3 eng/ci/gen_id_migration_map_doc.py`。
>
> 判绿：`python3 eng/ci/gen_id_migration_map_doc.py --check`（逐字节比对磁盘 md 与生成结果，不一致 exit 1）；自检：`python3 eng/ci/gen_id_migration_map_doc.py --self-test`。

任务：CI-001（控制包 PROJECT-GOVERNANCE-01）。机器可读事实源：`eng/ci/id_migration_map.json`（`schema_version` = 1）。

## 1. 口径

- 机器可读事实源：`eng/ci/id_migration_map.json`；本文件是它的人读摘要，**不含**任何未登记在该 JSON 里的映射事实。
- 写前基线（`source_registry`）：`eng/ci/checks.json` sha256 `62d051cc69c3993f33883ee11fe4f914588496f71d23a6f29f98038a2c04a3e4`（146 项；写前基线（TEST-GREEN-001 交棒，负责人 GO 确认））。
- 写后快照（`result_registry`，CI-001 收敛完成**当时**的状态）：sha256 `f2535f473dbc4c969f71ab970333a843506be862cc68a230fe91ee587edf5850`（38 项 / 136 steps）。
- 写后快照 ↔ 当前注册表交叉核对（生成器实测 `eng/ci/checks.json`）：**已不同步** —— 注册表在本波之后继续演进（各任务持续登记新的检查项与 step），写后快照只是收敛完成当时的状态，**不是**注册表的当前值。注册表当前规模与哈希以 `docs/ci/01_CHECKS.md §2`（由 `CHK-REGISTRY-DOC-SYNC` 双向一致门维护）与注册表自身为准；本文件不重复声明其当前值（否则本门会对注册表每次改动敏感，产生与事实无关的红灯）。核对明细由生成器 stdout / `--json-out` 打印。
- 目标语义 = `docs/ci/01_CHECKS.md §2` 表：`targets` 合计 112 个目标，其中 93 个的 `doc` 字段指向该表（含带登记批注的变体），19 个指向其它权威面（`docs/contracts/**`、`docs/science/**`、`docs/api/**`、`ASTROCS_DESIGN.md` 或扩展登记说明）；按 `kind` 计：doc 44 / extension 44 / ci 24。
- 归并形态：目标项 `steps[]` 聚合旧注册项，旧 ID 原样保留为 `step.id`；目标项 `command` = `python3 eng/ci/run_checks.py --check <目标ID> --quiet`，因此仍被工作流调用的 `eng/ci/run.py` 会逐条派发同一执行序列（不静默丢覆盖）。

## 2. 覆盖计数（`coverage` 块逐字引用，本文件不重算该块）

| 项 | 数（JSON 声明） |
|---|---|
| source_entry_count | 158 |
| mapped_entry_count | 147 |
| merged_into | 149 |
| kept | 146 |
| kept_pending_merge | 3 |
| retire_pending_gov_001 | 0 |
| retired | 0 |
| silent_drops | 0 |
| result_entry_count | 112 |

- `mappings` 逐条计数（生成器实测，**与上表口径不同**）：合计 303 条 —— KEPT 150 / KEPT-PENDING-MERGE 3 / MERGED-INTO 150；其中在 `coverage.absorbed_entries` 内登记为 absorbed 的 145 条。
- 上表由各登记任务按 +N 维护，R13 只机器校验等式 `source_entry_count` == `len(mappings) − len(set(absorbed_entries))`（现为 158 == 303 − 145）；本文件**逐字引用**该块，不用逐条计数覆盖它。
- `absorbed_entries`：145 条登记（唯一 145 条），均为本波基线之外、由后续任务新增的执行单元。 注：`absorbed_note` = CHK-SECRET-HYGIENE 不属本波基线 146 项，来自 ROOT-006 注册文件（吸收并入）
- `retired` = 0：本波无退役项；基线前退役 1 项单列于 §2.1，**不得**计为本波丢失的注册项。
- 覆盖口径注（`coverage.note` 逐字）：覆盖 = 本波基线 146 项逐条登记；TRACEABILITY-CODE 属基线前退役（147→146），单列不得计为丢失 CI-003（2026-09-16）：6 个 GOV-001 治理门由 RETIRE-PENDING 改判 KEPT（保留并修判据）；新增 7 个执行单元登记为 absorbed。 W4-A3（2026-09-16）：新增 2 个执行单元（IMPACT-MAP / IMPACT-MAP-SELFTEST，CHK-IMPACT-MAP）登记为 absorbed。 W4-A3（2026-09-17）：新增 11 个执行单元（CTEST-*，登记 CTEST-REGISTRATION 的C3 未注册目标）为 absorbed。 W4-A3（2026-09-17）：新增 4 个执行单元（PKG-*，CHK-PKG-CONSISTENCY）为 absorbed。 RELEASE-02 fix-gates（2026-09-19）：新增 7 个执行单元（CHK-ALGO-WIRING / CHK-REGISTRY-IR-PARITY / CHK-CONFIG-CONSUMED / CHK-CONFIG-DEFAULTS / CHK-PROD-SCALE / CHK-PROVENANCE-CONSISTENCY / CHK-REALDATA-E2E）登记为 absorbed。 DOC-403（2026-09-21）：DOC-INDEX 由 CHK-DANGLING step 提升为顶层；新增 4 个执行单元（DOC-INDEX-SELFTEST / CHK-RETIRED-CODE / CHK-RETIRED-CODE-SELFTEST / CHK-FIX406-SIGTERM）登记为 absorbed。 GATE-501（2026-09-21，RELEASE-05）：新增 6 个执行单元（CHK-REGISTRY-VALIDATE / L2-FROZEN-GATE-SELFTEST / L2-FROZEN-GATE-REPLAY / WORKER-BALANCE-METRIC-SELFTEST / WORKER-BALANCE-METRIC-REPLAY / PSFSW-RETIRED-STATIC-SELFTEST）登记为 absorbed。 GATE-501（2026-09-21）：补登记 8 个既有孤儿 unit 的迁移映射（R13 缺口，仅登记不改判据）。 RELEASE-05 CONTRACT-501/ARCH-501/SCI-502：新增 2 个执行单元（CTEST-CORE-BLOCK-FRAME、CTEST-V6-P2-SKY-KAPPA）登记为 absorbed。 GATE-502：新增 1 个执行单元（CHK-TEST-DISCRIMINATIVE-STEP，空断言静态门）登记为 absorbed。 ARCH-502：新增 2 个执行单元（CTEST-NORMALIZE-WORKFLOW、CHK-SCHED-PROBE-SCHEMA-STEP）登记为 absorbed。 ARCH-503：新增 1 个执行单元（CTEST-MOSAIC-WINDOW）登记为 absorbed。 ARCH-504：新增 1 个执行单元（CTEST-EXPORT-STREAM）登记为 absorbed。 ARCH-505：新增 3 个执行单元（块流规格/一致性/执行器）登记为 absorbed。 E2E-501：新增 3 个执行单元（预检矩阵/E2E 链/链自测）登记为 absorbed。 GAIA-FAILCLOSED-01（2026-09-22）：新增 2 个执行单元（CTEST-GAIA-SHARD-COVERAGE / CTEST-GAIA-SHARD-COVERAGE-SELF-TEST，CHK-INVARIANT）登记为 absorbed。 DRIZZLE-FIX-01（2026-09-22）：新增 1 个执行单元（CHK-DRZ-DISP009）登记为 absorbed。 LOG-SYS-01（2026-09-22）：新增 2 个执行单元（LOG-SYS-SCAN / LOG-SYS-SELFTEST，CHK-LOG-SYS 日志与错误系统判据）登记为 absorbed。 RULING-DOC-01（2026-09-22）：新增 1 个执行单元（CHK-NWORKER-TOLERANCE，1/N worker 等价判据的容差档非退化自检）登记为 absorbed。 TRIM-LAND-01：新增 2 个执行单元（CHK-SPARSE-PUNCH 静态不变量+谓词判据判别力；CHK-SPARSE-PUNCH-PROBE 由生产头编译探针跑真实系统调用）登记为 absorbed。 REGMAP-FIX-01（2026-09-22）：补登记 4 个执行单元（CHK-HIPS-STORAGE-FORM / STATIC-P3-EXPORT-STREAM-PROD / SELFTEST-P3-EXPORT-STREAM-PROD / CTEST-P3-EXPORT-STREAM-RSS，分属 CHK-HIPS-STORAGE-FORM / CHK-P3-EXPORT-STREAM-PROD / CHK-P3-EXPORT-STREAM-RSS 三个目标）登记为 absorbed —— 三者由 HIPS-PACK-01 与 P3-STREAM-01 登记进 checks.json 时漏登记本映射（R13 孤儿 unit 缺口）；同时把 CHK-HIPS-STORAGE-FORM 移到 CHK-KNOWN-FAILURES-BASELINE 之前，使 linux-main 选中序末位仍是 KNOWN-FAILURES-BASELINE-CHECK（R10）。 RECONCILE-01（并发写回重建）：一次并发写回把本文件与 checks.json 覆写成过期快照，丢失了共享对象符号闭包门、deep profile 注册单测门、迁移映射文档同步两 step 等登记；本次以现存最全快照为底重建，并补回当前注册表独有的新增项。

### 2.1 基线前退役（`pre_baseline_retirements`）

| 旧 ID | 决策 | 依据 | 原因 | 备注 |
|---|---|---|---|---|
| `TRACEABILITY-CODE` | RETIRED | GAP-032 + 前台裁决 2026-09-16（docs/ci/01_CHECKS.md §2.1 退役记录） | 唯一默认输入 artifacts/evidence/prerelease-v5/tables/TRACEABILITY.csv 随 artifacts/ 按负责人裁决删除而断供；本 CI 规范/最高设计/工程规范对「追溯」零命中；内容未丢（git show b1290525^:…） | 不得记为本波「丢失的注册项」；本波 146 项输入 = 147 - TRACEABILITY-CODE |

## 3. 目标项 → 归并的旧 ID

### 3.1 `targets` 声明了 `steps` 的 59 个目标（JSON 逐字）

| 目标 ID | 类型 | 级 | 名称 | steps（旧 ID，按执行序） |
|---|---|---|---|---|
| `CHK-BUILD-LINUX` | doc | P0 | Linux Release 构建 | `BUILD-GCC-RELEASE`、`DEEP-CLANG-BUILD` |
| `CHK-BUILD-WIN` | doc | P0 | Windows Release 构建 | `WIN-BUILD-RELEASE` |
| `CHK-WARN` | doc | P0 | 编译警告 | `WARNING-SUPPRESSION` |
| `CHK-STATIC` | doc | P1 | 静态分析 | `DUPLICATION`、`CON-FORBIDDEN-PATTERNS`、`PROD-REACH-SELFTEST`、`DEEP-COMPLEXITY` |
| `CHK-MODULE-MANIFEST` | doc | P0 | 模块 manifest/注册表/构建 target/产品清单一致 | `CHK-MODULE-MANIFEST`、`MODULE-READMES`、`CTEST-REGISTRATION`、`CON-BUILD-GRAPH`、`P3-STATUS`、`WORKFLOW-REGISTRY-BINDING`、`CI-BINDING-TESTS`、`CTEST-SCI-EVIDENCE-REGISTRATION` |
| `CHK-CONTRACT-REF` | doc | P0 | 端口引用有效 DATA 合同（canonical=eng/contracts/schemas/unified/**、docs/contracts/unified_object_registry.json） | `CONTRACT-GRAPH`、`DATA-ARTIFACTS` |
| `CHK-SCI-REF` | doc | P0 | 算法引用有效 SCI/ALG | `TRACEABILITY`、`CON-TRACEABILITY`、`PRODUCTION-GRAPH`、`PIPELINE-TRACE`、`CON-SCIENCE-UNITS`、`UNIT-CLOSURE`、`ACR-DORMANT`、`DOC-LINE-ANCHORS` |
| `CHK-CONTRACT-TEST` | doc | P0 | 核心合同有独立测试 | `CON-TEST-CONTRACTS`、`CON-FULL-INTEGRATION`、`TRACEABILITY-MATRIX`、`UT-TRACEABILITY`、`UT-CONTRACTS`、`V6-NEGATIVE-MUTATION`、`V6-CTEST-INTEGRATION`、`CTEST-ORCH-LOGGER-UNITS`、`CTEST-ORCH-CHECKPOINT-UNITS`、`CTEST-ORCH-CLI-INTEGRATION`、`CTEST-ORCH-LEGACY-CLI-SMOKE`、`CTEST-ORCH-LEGACY-CLI-VALIDATE`、`CTEST-ORCH-SATURATION-GATE` |
| `CHK-DANGLING` | doc | P1 | 删除/重命名无悬空引用 | `CON-DOC-SYMBOLS` |
| `CHK-STALE-DOC` | doc | P1 | 活动文档无陈旧版本号/历史状态冒充 | `CON-COMMENTS` |
| `API-DOCS` | doc | P0 | doc↔code 命令树/签名/退出码/schema 一致 | `API-DOCS`、`CLI-COMMAND-LAYER`、`CLI-RUN-PRESET`、`CON-API-CONTRACTS` |
| `CHK-UNIT` | doc | P0 | 每模块单测 | `UT-API`、`UT-ARCH`、`UT-BACKEND`、`UT-CLI`、`UT-CONFIG`、`UT-VERSION`、`UT-GLOSSARY`、`UT-MONITORING`、`UT-PIPELINE`、`UT-RUNTIME`、`UT-RUNTIME-INTEGRATION`、`UT-SCIENCELINT`、`UT-ARTIFACT`、`UT-IO`、`UT-QUALITY`、`UT-GAIA-ZLIB`、`PY-TEST-INDEX-VERDICTS`、`TEST-INDEX-LIVE`、`TESTKIT-LIST`、`CTEST-LINUX-FULL`、`CTEST-AIO-CHECKSUM`、`CTEST-AIO-TILE_MODEL`、`CTEST-AIO-TRANSFORM`、`CTEST-AIO-QUERY_PIXEL`、`CTEST-AIO-PRECISION_DUAL`、`WIN-TEST-UNIT`、`V6-CTEST-UNIT`、`CTEST-AIO-HIPS-ATOMIC-PUBLISH`、`CTEST-FIX402-PHASE3-SEMANTIC-GUARD` |
| `CHK-ORACLE` | doc | P0 | SCI/ALG Oracle 测试 | `UT-CPU-BASELINE`、`V6-RUNTIME-CLOSURE`、`CTEST-P1WCS-APBP`、`CTEST-P1WCS-ASTROPY-CROSS`、`CTEST-P1WCS-STD-F1-BRIDGE`、`CTEST-P1PHOT-FIXGATES`、`CTEST-P1SNR-SCIENCE-ALL`、`CTEST-IPV-MAG-ITER`、`CTEST-IPV-MAG-ITER-DELIVERY`、`CTEST-IPV-SELECT-DOMAIN`、`CTEST-P1NOISE-NUMPY-ORACLE`、`CTEST-P1WCS-CLOSURE-METRIC-GATE` |
| `CHK-INVARIANT` | doc | P0 | 科学不变量/性质测试 | `CTEST-DRIZZLE-PRECISION-DEFAULT`、`CTEST-P1STAR-MAD`、`CTEST-P1PSF-PRODPATH-CENTROID`、`CTEST-P1STAR-ANGLE-GUARD`、`CTEST-P1STAR-GUIDED`、`CTEST-P1STARDET-NODE-GATE`、`CTEST-P1SNR-FRAME-PARITY`、`CTEST-P2HIPS-UNITS`、`CTEST-P2HIPS-DETERMINISM`、`CTEST-P2HIPS-NEGATIVE`、`CTEST-P2HIPS-SELFCHECK`、`CTEST-GAIA-MANIFEST-BOUNDS`、`CTEST-XPSD-SPECTRUM-COUNT-BOUNDS`、`CTEST-GAIA-MAGNITUDE-RANGE-BOUNDS`、`CTEST-GAIA-SHARD-COVERAGE`、`CTEST-GAIA-SHARD-COVERAGE-SELF-TEST`、`CTEST-IPV-EXTRACT-WCS-SIP-FAILCLOSED`、`CTEST-IPV-TRIANGLE-BUDGET`、`CTEST-AIO-HIPS-PUBLISH-ATOMIC-UNITS`、`CTEST-AIO-HIPS-PUBLISH-ATOMIC`、`IPV-PLATFORM-BINDING`、`CTEST-P1PSF-CENTROID-GATE`、`CTEST-P1PSF-CENTROID-GATE-NEG`、`CTEST-P1PSF-CENTER-CONTRACT`、`CTEST-P1PSF-CENTER-CONTRACT-NEG`、`CTEST-P1PSF-CENTER-CONTRACT-ASTROPY`、`CTEST-P1CAL-BIAS-INFLUENCE-GATE`、`CTEST-P1NOISE-MASK`、`CTEST-P1NOISE-SATURATION`、`CTEST-P1NOISE-SATURATION-SELFCHECK`、`CTEST-P1NOISE-SATURATION-WIRING` |
| `CHK-ABI` | doc | P0 | C ABI 兼容性 | `ABI-BOUNDARY`、`AST-API`、`UT-ABI`、`CTEST-AIO-ABI-UNITS`、`CTEST-AIO-ABI-NEGATIVE`、`CTEST-AIO-ABI-SELFCHECK`、`CTEST-IPV-ABI-LAYOUT-LOCK`、`CTEST-IPV-ABI-LAYOUT-LOCK-SELFCHECK`、`CTEST-IPV-PARAMS-ABI-FAILCLOSED`、`CTEST-AIO-ABI-LAYOUT-LOCK`、`CTEST-AIO-ABI-LAYOUT-LOCK-SELFCHECK`、`CTEST-IPV-DEAD-PARAMS-LOCK`、`CTEST-IPV-DEAD-PARAMS-LOCK-SELFCHECK`、`CTEST-IPV-ERROR-UTF8`、`CTEST-P1NOISE-ABI-LAYOUT` |
| `CHK-SCHEMA` | doc | P0 | schema 校验 | `CON-CONFIG-CONTRACTS`、`LOG-CONTRACT-SELFCHECK`、`EVT-FIELD-SETS`、`EVT-FIELD-SETS-SELFTEST` |
| `CHK-SYNTH-P1` | doc | P0 | normalize 合成全链 | `CTEST-P1001-REAL-NODES` |
| `CHK-SYNTH-P2` | doc | P0 | mosaic 合成全链 | `CTEST-P2001-REAL-NODES`、`CTEST-P2002-UNC-REJ-PROV`、`CTEST-P1DRZ-MERGE-PIPELINE-LOCK`、`CTEST-PHASE2-GATES`、`V6-CLI-MODE-ROUTING`、`V6-CLI-MODE-MATRIX` |
| `CHK-SYNTH-P3` | doc | P0 | export 合成全链 | `CTEST-P3002-REAL-NODES`、`CTEST-P3002-UNCERTAINTY`、`CTEST-P3-PROJECTION-UNITS`、`CTEST-P3-PROJECTION-FAULT`、`CTEST-P3-SAMPLER-CACHE` |
| `CHK-ISA-EQ` | doc | P1 | baseline/AVX2/AVX-512 等价 | `UT-CPU-AVX2`、`UT-CPU-AVX512`、`UT-CPU-DISPATCH`、`ISA-LEAK-SELFTEST` |
| `CHK-NWORKER` | doc | P0 | 1 vs N worker 数值一致 | `V6-DETERMINISM`、`CTEST-P1DRZ-TASKSET-INVARIANCE`、`CTEST-P1SNR-LINUX-ALL` |
| `CHK-SANITIZER` | doc | P1 | ASan/UBSan | `DEEP-SAN-ASAN`、`DEEP-SAN-TSAN` |
| `CHK-COVERAGE` | doc | P2 | 覆盖率报告 | `DEEP-COV-CPP`、`DEEP-COV-PY` |
| `CHK-RESOURCE` | doc | P0 | 内存/线程/利用率门禁 | `THREAD-BUDGET`、`NO-SERIAL-HEAVY`、`SERIAL-HARDCODE`、`SERIAL-HEAVY-SELFTEST`、`V6-RESOURCE-GATE-POLICY`、`CTEST-RESOURCE-MONITOR-QUALITY`、`CTEST-MON004-ENFORCEMENT`、`CTEST-RT001-UNIQUE-EXECUTOR`、`CTEST-COMPARE-PRODUCTS-QUALITY`、`AIO-OWNERSHIP`、`CON-EXECUTION-CONTRACTS`、`CON-EXECUTION-CONTRACTS-SELFTEST` |
| `CHK-PACKAGE` | doc | P0 | 发布候选打包/白名单/哈希/版本/provenance | `WIN-PACKAGE-CANDIDATE` |
| `CHK-ENV-ADOPTION` | extension | P1 | CI 环境/接管基线（工具链策略、工作区接管证据、任务-证据对账） | `TOOLCHAIN-VERIFY` |
| `CHK-KNOWN-FAILURES-BASELINE` | extension | P1 | 版本化已知失败基线门（聚合型，linux-main 末位） | `KNOWN-FAILURES-BASELINE`、`KNOWN-FAILURES-BASELINE-VERIFY`、`KNOWN-FAILURES-BASELINE-CHECK` |
| `STD-REG` | extension | P1 | 标准注册表 C1–C8 判据 + 9 场景 fault-inject 负例面 | `STD-REG`、`STD-REG-FI-DANGLING`、`STD-REG-FI-VERSION-DRIFT` |
| `CHK-REGISTRY-DOC-SYNC` | extension | P0 | 注册表 ↔ docs/ci/01_CHECKS.md §2 双向一致（ENGINEERING_SPEC §8） | `REGISTRY-DOC-SYNC`、`REGISTRY-DOC-SYNC-SELFTEST`、`ID-MIGRATION-MAP-DOC-SYNC`、`ID-MIGRATION-MAP-DOC-SYNC-SELFTEST` |
| `CHK-IMPACT-MAP` | extension | P0 | eng/ci/impact_map.json 判据一致性门（changed-path→checks 映射；ENGINEERING_SPEC §8） | `IMPACT-MAP`、`IMPACT-MAP-SELFTEST` |
| `CHK-PKG-CONSISTENCY` | extension | P0 | 产品清单/安装树合同/依赖锁/安装规则/许可登记面一致 + SBOM 实树 hash 自证 | `PKG-CONSISTENCY`、`PKG-CONSISTENCY-NEG`、`PKG-SBOM`、`PKG-SBOM-NEG` |
| `CHK-EXIT-CONSISTENCY` | ci | P1 |  | `EXIT-CONSISTENCY-SCAN`、`EXIT-CONSISTENCY-SELFTEST` |
| `CHK-E2E-REPRO` | ci | P0 |  | `E2E-WCS-CLOSURE-REPRO-SELFTEST` |
| `CHK-SPEC-NAMED-IMPL-ON-PROD-PATH` | extension | P0 | 规范点名的权威实现必须在生产可达路径（CMake 根构建图闭包 + 生产节点调用点；登记表 eng/ci/spec_named_impls.json；缺口台账 eng/ci/ledgers/spec_named_impl_gaps.json） | `SPEC-NAMED-IMPL-ON-PROD-PATH`、`SPEC-NAMED-IMPL-ON-PROD-PATH-SELFTEST` |
| `CHK-P3-PROJ-DECL` | extension | P0 | 投影注册表声明集 == 实际可运行集（FIX-205） | `P3-PROJ-DECL-REGISTRY`、`P3-PROJ-DECL-UNSUPPORTED-CLI` |
| `CHK-P3-PROJ-DECL-SELFTEST` | extension | P0 | 上项的注册表自检面 | `P3-PROJ-DECL-SELFTEST` |
| `CHK-PSFSW-RETIRED-STATIC` | extension | P1 | PSFSW 退役对象 canonical 面静态清零 | `PSFSW-RETIRED-STATIC`、`PSFSW-RETIRED-STATIC-SELFTEST` |
| `CHK-FAILCLOSED-SURVEY` | ci | P0 | 全门禁 fail-closed 普查（缺失证据 / 坏证据 / 无输出三注入；适用面必须判红） | `FAILCLOSED-SURVEY`、`FAILCLOSED-SURVEY-SELFTEST` |
| `CHK-ARCH501-BLOCK-FRAME` | doc | P0 | ARCH-501 命名块与块生命周期 | `CTEST-CORE-BLOCK-FRAME` |
| `CHK-SCI502-SKY-KAPPA` | doc | P0 | SCI-502 天光面 κ 自适应 | `CTEST-V6-P2-SKY-KAPPA` |
| `CHK-TEST-DISCRIMINATIVE` | doc | P0 | 空断言静态门（测试判别力） | `CHK-TEST-DISCRIMINATIVE-STEP` |
| `CHK-ARCH502-NORMALIZE-WF` | doc | P0 | ARCH-502 normalize 异步工作流调度器 | `CTEST-NORMALIZE-WORKFLOW` |
| `CHK-SCHED-PROBE-SCHEMA` | doc | P0 | 探针事件 schema 机器校验 | `CHK-SCHED-PROBE-SCHEMA-STEP` |
| `CHK-ARCH503-MOSAIC-WIN` | doc | P0 | ARCH-503 mosaic 天球窗口并行调度器 | `CTEST-MOSAIC-WINDOW` |
| `CHK-MEM-WIRE-01` | doc | P0 | MEM-WIRE-01 内存静态预算与回压生产路径 | `CTEST-MEM-WIRE` |
| `CHK-ARCH504-EXPORT-STREAM` | doc | P0 | ARCH-504 export 子块流式调度器 | `CTEST-EXPORT-STREAM` |
| `CHK-BLOCKFLOW-SPEC` | doc | P0 | 块流规格机器门 | `CHK-BLOCKFLOW-SPEC-STEP` |
| `CHK-BLOCKFLOW-CONFORMANCE` | doc | P0 | 块流一致性登记册机器门 | `CHK-BLOCKFLOW-CONFORMANCE-STEP` |
| `CHK-ARCH505-BLOCK-FLOW` | doc | P0 | ARCH-505 阶段块流执行器 | `CTEST-BLOCK-FLOW` |
| `CHK-PREFLIGHT-MATRIX` | doc | P0 | 预检语义矩阵 | `PREFLIGHT-MATRIX-STEP` |
| `CHK-E2E-CHAIN` | doc | P0 | 三命令真实数据全链与 manifest 链 | `E2E-CHAIN-VERIFY-STEP` |
| `CHK-E2E-CHAIN-SELFTEST` | doc | P0 | E2E 链判据自测 | `E2E-CHAIN-SELFTEST-STEP` |
| `CHK-LOG-SYS` | doc | P0 | 日志与错误系统判据（最高设计 §7.3：错误上行 + 日志落 output_dir） | `LOG-SYS-SCAN`、`LOG-SYS-SELFTEST` |
| `CHK-BLOCKFLOW-PORTS-VS-CODE` | doc | P0 | 块流端口↔代码双向一致门 | `CHK-BLOCKFLOW-PORTS-VS-CODE-STEP` |
| `CHK-P3-EXPORT-STREAM-PROD` | extension | P0 | Phase3 导出子块流式的生产接线静态锁（生产 writer 节点 TU 必须引用 ExportStreamScheduler；整幅驻留路径不得回流） | `STATIC-P3-EXPORT-STREAM-PROD`、`SELFTEST-P3-EXPORT-STREAM-PROD` |
| `CHK-P3-EXPORT-STREAM-RSS` | extension | P0 | Phase3 导出子块流式的内核记账驻留门（clear_refs 归零后读 VmHWM，峰值与产品面积解耦） | `CTEST-P3-EXPORT-STREAM-RSS` |
| `CHK-PLUGIN-SYMBOL-CLOSURE` | extension | P0 | 交付共享对象（plugin .so）符号闭包：产品清单登记的非 exe unit 逐个断言强未定义符号可在依赖闭包内解析（闭包 = DT_NEEDED 传递闭包 ∪ 宿主基线库）、DT_NEEDED 可解析、dlopen(RTLD_NOW\|RTLD_LOCAL) 成功；含可执行正负例自检 | `STATIC-PLUGIN-SYMBOL-CLOSURE`、`SELFTEST-PLUGIN-SYMBOL-CLOSURE` |
| `CHK-CI-INTEGRATION-SELFTESTS` | extension | P1 | eng/ci/tests 下 3 个真起子进程/真跑 CLI/含真实测量窗的自测，按 CI_SPEC §2.6 归 integration 档 | `CI-INTEGRATION-CI001-FAILCLOSED`、`CI-INTEGRATION-CI-REPAIR-ROUND`、`CI-INTEGRATION-RUN-MONITORED` |
| `CHK-CI-CONTRACT-SELFTESTS` | extension | P1 | eng/ci/tests 下 15 个 CI 契约/工作流/资源门自测，逐文件独立进程执行（整目录 discover 的「绿」是 sys.path 顺序副作用，会掩盖坏模块） | `CI-CONTRACT-BOOTSTRAP-UTF8`、`CI-CONTRACT-RUNNER-CONTRACT`、`CI-CONTRACT-RUNNER-EXECUTION`、`CI-CONTRACT-RUNNER-SELECTION`、`CI-CONTRACT-GAP4-EMPTY-OUTPUTS`、`CI-CONTRACT-IMPACT-MAP`、`CI-CONTRACT-NEGATIVE-GUARDS`、`CI-CONTRACT-RESOURCE-PROBE`、`CI-CONTRACT-RESOURCE-MONITOR-SHIM`、`CI-CONTRACT-RUN-PREFIX-PROBE`、`CI-CONTRACT-FATDUCK-WORKFLOW`、`CI-CONTRACT-VERIFY-REMOTE-RUN`、`CI-CONTRACT-WINDOWS-CI`、`CI-CONTRACT-WORKFLOW-LOCK`、`CI-CONTRACT-CTEST-REGISTRATION` |

### 3.2 `mappings` 反推：聚合项 `targets.steps` 未登记 0 个

（`targets.steps` 与 `mappings` 反推集合一致。）

### 3.3 两份登记的差异（5 个目标；注册表实际 `steps` 以 `eng/ci/checks.json` 为准，本文件不读它做内容，故此处只并列两侧登记、不断言谁对）

| 目标 ID | 仅 `targets.steps` 列（mappings 未指向该 target） | 仅 `mappings` 指向（`targets.steps` 未列） |
|---|---|---|
| `CHK-MODULE-MANIFEST` | `CHK-MODULE-MANIFEST` |  |
| `API-DOCS` | `API-DOCS` |  |
| `CHK-SCHEMA` |  | `TASK-RESULT-SCHEMA` |
| `CHK-ENV-ADOPTION` |  | `WORKSPACE-ADOPTION`、`RECONCILE-STATE` |
| `STD-REG` | `STD-REG` |  |

## 4. §2 有行但本波无实现者（RESERVED，不注册假绿门）

| 目标 ID | 级 | 名称 | 原因 | 归属 |
|---|---|---|---|---|
| `CHK-FMT` | P0 | 格式检查（clang-format --dry-run --Werror） | 宿主无 clang-format（实测 MISSING），且现注册表零 FMT 命令；本波不注册缺工具即恒红的假门，登记 RESERVED 待补工具+正负例再注册 | 后续 CI 治理任务（工具可用性前置） |
| `CHK-DUAL-TOL` | P1 | 双平台允许误差 | 现注册表零双平台数值对比命令；Windows 复验面（REAL-001/负责人触发） | REAL-001 / 负责人触发复验 |
| `CHK-AGENT-HARD-RULES` | P0 | AGENTS.md 硬禁令存在 | 唯一实现者 eng/tools/check_agents_gov.py 本波按裁决冻结（RETIRE-PENDING-GOV-001，W2 GOV-001 执行迁移） | GOV-001（W2） |

## 5. 逐条映射明细（`mappings`，303 条）

| 旧 ID | 决策 | 目标/预留位 | 类型 | 级 | 备注（JSON note 逐字） |
|---|---|---|---|---|---|
| `ABI-BOUNDARY` | MERGED-INTO | `CHK-ABI` | doc | P0 | C ABI 兼容性 |
| `ACR-DORMANT` | MERGED-INTO | `CHK-SCI-REF` | doc | P0 | 算法引用有效 SCI/ALG |
| `AGENTS-GOV` | KEPT | `AGENTS-GOV` |  |  | CI-003（2026-09-16）：原 RETIRE-PENDING-GOV-001 判定撤销 —— 该门经判据迁移/修锚/加可执行负例后保留为活动门，已登记 docs/ci/01_CHECKS.md §2。 |
| `AHPX-WEIGHT-RETIRED` | KEPT | `AHPX-WEIGHT-RETIRED` | extension | P0 | RELEASE-03 BLD-201（2026-09-20）新增执行单元（非本波基线，absorbed）：各判据来源见工程控制/RELEASE-03/change-claims/** 与 run/RELEASE-03/logs/*-receipt.md；可执行负例面见各检查器 --self-test 或登记命令内建的负例用例。 |
| `AIO-IO-BOUNDARY-SELFTEST` | KEPT | `AIO-IO-BOUNDARY-SELFTEST` | extension | P0 | RELEASE-03 BLD-201（2026-09-20）新增执行单元（非本波基线，absorbed）：各判据来源见工程控制/RELEASE-03/change-claims/** 与 run/RELEASE-03/logs/*-receipt.md；可执行负例面见各检查器 --self-test 或登记命令内建的负例用例。 |
| `AIO-OWNERSHIP` | MERGED-INTO | `CHK-RESOURCE` | doc | P0 | 内存/线程/利用率门禁 |
| `ALG-LINE-ANCHORS` | KEPT | `ALG-LINE-ANCHORS` |  |  | 本波新增执行单元，登记为 absorbed（不计入 source_entry_count）。 |
| `API-DOCS` | MERGED-INTO | `API-DOCS` | doc | P0 | doc↔code 命令树/签名/退出码/schema 一致 |
| `AST-API` | MERGED-INTO | `CHK-ABI` | doc | P0 | C ABI 兼容性 |
| `BUILD-GCC-RELEASE` | MERGED-INTO | `CHK-BUILD-LINUX` | doc | P0 | Linux Release 构建 |
| `CHK-AIO-IO-BOUNDARY` | KEPT | `CHK-AIO-IO-BOUNDARY` | extension | P0 | RELEASE-03 BLD-201（2026-09-20）新增执行单元（非本波基线，absorbed）：各判据来源见工程控制/RELEASE-03/change-claims/** 与 run/RELEASE-03/logs/*-receipt.md；可执行负例面见各检查器 --self-test 或登记命令内建的负例用例。 |
| `CHK-ALGO-WIRING` | KEPT | `CHK-ALGO-WIRING` |  |  | 本波新增执行单元，登记为 absorbed（不计入 source_entry_count）。 |
| `CHK-BLOCKFLOW-CONFORMANCE-STEP` | KEPT | `CHK-BLOCKFLOW-CONFORMANCE` | doc | P0 | ARCH-505 新增执行单元 |
| `CHK-BLOCKFLOW-PORTS-VS-CODE-STEP` | KEPT | `CHK-BLOCKFLOW-PORTS-VS-CODE` | doc | P0 | REGISTRY-ALIGN-01 新增执行单元（端口↔代码双向一致） |
| `CHK-BLOCKFLOW-SPEC-STEP` | KEPT | `CHK-BLOCKFLOW-SPEC` | doc | P0 | ARCH-505 新增执行单元 |
| `CHK-CONFIG-CONSUMED` | KEPT | `CHK-CONFIG-CONSUMED` |  |  | 本波新增执行单元，登记为 absorbed（不计入 source_entry_count）。 |
| `CHK-CONFIG-DEFAULTS` | KEPT | `CHK-CONFIG-DEFAULTS` |  |  | 本波新增执行单元，登记为 absorbed（不计入 source_entry_count）。 |
| `CHK-DEEP-PROFILES-TESTS` | KEPT | `CHK-DEEP-PROFILES-TESTS` | extension | P0 | 并发写回后重建：注册表已登记的执行单元，按 KEPT 归入其父项 |
| `CHK-DOC-HYGIENE` | KEPT | `CHK-DOC-HYGIENE` | ci | P0 | DOC-HYGIENE-01 新增执行单元（非本波基线，absorbed）：ENGINEERING_SPEC §8 规则 2（正式文档不堆任务编号与日期）+ 规则 4（悬空即缺陷）；判据含已知限制台账 ID 可解析（D2） |
| `CHK-DOC-HYGIENE-SELFTEST` | KEPT | `CHK-DOC-HYGIENE-SELFTEST` | ci | P0 | DOC-HYGIENE-01 新增执行单元（非本波基线，absorbed）：ENGINEERING_SPEC §10 可执行负例面（1 正例 + 10 负例） |
| `CHK-DRZ-DISP009` | KEPT | `CHK-DRZ-DISP009` | extension | P0 | DRIZZLE-FIX-01（2026-09-22）新增执行单元（非本波基线，absorbed）：DISP-DRZ-009 面亮度保持权重 w_jp=a_jp/A_pixel,j 回归门；判据 = 常量面亮度 \|S_p/B0-1\|<1e-3（pixfrac∈(0,1]）+ "分母取 A_drop 必判红"负例控制；可执行负例面见 run/DRIZZLE-FIX-01/REPORT.md §⑥（真实注入红/绿 + sha256）。 |
| `CHK-E2E-REPRO` | KEPT | `CHK-E2E-REPRO` | ci | P0 | E2E-FIX-001 发布门 G-P1-WCS-CLOSURE-REPRO CI 注册 |
| `CHK-EXIT-CONSISTENCY` | KEPT | `CHK-EXIT-CONSISTENCY` | ci | P1 | C 类收口：结论与退出码一致性（W4-A3 新增门） |
| `CHK-FAILCLOSED-SURVEY` | KEPT | `CHK-FAILCLOSED-SURVEY` | extension | P0 | 新增聚合项（GATE-501）（非本波 146 项基线，absorbed） |
| `CHK-FIX203-PROMOTED-KEYS` | KEPT | `CHK-FIX203-PROMOTED-KEYS` | extension | P1 | RELEASE-03 BLD-201（2026-09-20）新增执行单元（非本波基线，absorbed）：各判据来源见工程控制/RELEASE-03/change-claims/** 与 run/RELEASE-03/logs/*-receipt.md；可执行负例面见各检查器 --self-test 或登记命令内建的负例用例。 |
| `CHK-FIX208-DISK-GATE` | KEPT | `CHK-FIX208-DISK-GATE` | extension | P0 | RELEASE-03 BLD-201（2026-09-20）新增执行单元（非本波基线，absorbed）：各判据来源见工程控制/RELEASE-03/change-claims/** 与 run/RELEASE-03/logs/*-receipt.md；可执行负例面见各检查器 --self-test 或登记命令内建的负例用例。 |
| `CHK-FIX208-EVENT-STREAM-DEFAULT` | KEPT | `CHK-FIX208-EVENT-STREAM-DEFAULT` | extension | P0 | RELEASE-03 BLD-201（2026-09-20）新增执行单元（非本波基线，absorbed）：各判据来源见工程控制/RELEASE-03/change-claims/** 与 run/RELEASE-03/logs/*-receipt.md；可执行负例面见各检查器 --self-test 或登记命令内建的负例用例。 |
| `CHK-FIX406-SIGTERM` | KEPT | `CHK-FIX406-SIGTERM` | ci | P0 | FIX-406 新增测试矩阵；DOC-403 登记（2026-09-21，依据 run/FIX-406/WINDOWS_CANCEL_PLATFORM_LIMITS.md §3 W4） |
| `CHK-GATE-FAILCLOSED-SELFTEST` | KEPT | `CHK-GATE-FAILCLOSED-SELFTEST` | extension | P0 | GATE-501（RELEASE-05）新增执行单元（非本波 146 项基线，absorbed） |
| `CHK-HIPS-STORAGE-FORM` | KEPT | `CHK-HIPS-STORAGE-FORM` | extension | P0 | HIPS-PACK-01（2026-09-22）新增执行单元（非本波基线，absorbed）：落盘形态合同（CONTRACT-STORAGE-001）机器门，判据与 21 例可执行负例见 eng/tools/hipsform/check_hips_storage_form.py --self-test。REGMAP-FIX-01 补登记迁移映射（R13 孤儿 unit 缺口，仅登记不改判据）。 |
| `CHK-MODULE-MANIFEST` | MERGED-INTO | `CHK-MODULE-MANIFEST` | doc | P0 | 模块 manifest/注册表/构建 target/产品清单一致 |
| `CHK-NO-WEIGHT-MODE` | KEPT | `CHK-NO-WEIGHT-MODE` | extension | P0 | RELEASE-03 BLD-201（2026-09-20）新增执行单元（非本波基线，absorbed）：各判据来源见工程控制/RELEASE-03/change-claims/** 与 run/RELEASE-03/logs/*-receipt.md；可执行负例面见各检查器 --self-test 或登记命令内建的负例用例。 |
| `CHK-NO-WEIGHT-MODE-CODE` | KEPT | `CHK-NO-WEIGHT-MODE-CODE` | extension | P0 | 并发写回后重建：注册表已登记的执行单元，按 KEPT 归入其父项 |
| `CHK-NO-WEIGHT-MODE-CODE-SELFTEST` | KEPT | `CHK-NO-WEIGHT-MODE-CODE-SELFTEST` | extension | P0 | 并发写回后重建：注册表已登记的执行单元，按 KEPT 归入其父项 |
| `CHK-NO-WEIGHT-MODE-SELFTEST` | KEPT | `CHK-NO-WEIGHT-MODE-SELFTEST` | extension | P0 | RELEASE-03 BLD-201（2026-09-20）新增执行单元（非本波基线，absorbed）：各判据来源见工程控制/RELEASE-03/change-claims/** 与 run/RELEASE-03/logs/*-receipt.md；可执行负例面见各检查器 --self-test 或登记命令内建的负例用例。 |
| `CHK-NWORKER-TOLERANCE` | KEPT | `CHK-NWORKER-TOLERANCE` | extension | P0 | RULING-DOC-01（2026-09-22）新增执行单元（非本波基线，absorbed）：1/N worker 等价判据由「逐位一致」改为「逐字节优先 + 冻结浮点容差」（负责人裁决 2026-09-22）；本项为容差档的非退化自检 python3 eng/tools/v6/v6_numeric_equiv.py --self-test |
| `CHK-PATH-DOMAIN-ANCHORS` | KEPT | `CHK-PATH-DOMAIN-ANCHORS` |  |  | 本波新增执行单元，登记为 absorbed（不计入 source_entry_count）。 |
| `CHK-PHOTOMETRY-APPLY-SELFTEST` | KEPT | `CHK-PHOTOMETRY-APPLY-SELFTEST` | extension | P0 | RULING-DOC-01（2026-09-22）新增执行单元（非本波基线，absorbed）：负责人裁决 B（合并 photometry 与施加为一步，须保留像素级可核对判据） |
| `CHK-PROD-SCALE` | KEPT | `CHK-PROD-SCALE` |  |  | 本波新增执行单元，登记为 absorbed（不计入 source_entry_count）。 |
| `CHK-PROVENANCE-CONSISTENCY` | KEPT | `CHK-PROVENANCE-CONSISTENCY` |  |  | 本波新增执行单元，登记为 absorbed（不计入 source_entry_count）。 |
| `CHK-PSFSW-RETIRED-NEGATIVE` | KEPT | `CHK-PSFSW-RETIRED-NEGATIVE` | extension | P1 | RELEASE-03 BLD-201（2026-09-20）新增执行单元（非本波基线，absorbed）：各判据来源见工程控制/RELEASE-03/change-claims/** 与 run/RELEASE-03/logs/*-receipt.md；可执行负例面见各检查器 --self-test 或登记命令内建的负例用例。 |
| `CHK-REALDATA-E2E` | KEPT | `CHK-REALDATA-E2E` |  |  | 本波新增执行单元，登记为 absorbed（不计入 source_entry_count）。 |
| `CHK-REGISTRY-IR-PARITY` | KEPT | `CHK-REGISTRY-IR-PARITY` |  |  | 本波新增执行单元，登记为 absorbed（不计入 source_entry_count）。 |
| `CHK-REGISTRY-VALIDATE` | KEPT | `CHK-REGISTRY-VALIDATE` | extension | P0 | GATE-501（RELEASE-05）新增执行单元（非本波 146 项基线，absorbed）：python3 eng/ci/validate_registry.py --registry eng/ci/checks.json --strict |
| `CHK-RETIRED-CODE` | KEPT | `CHK-RETIRED-CODE` | ci | P0 | CLEAN-401 新增检查器；DOC-403 登记（2026-09-21） |
| `CHK-RETIRED-CODE-SELFTEST` | KEPT | `CHK-RETIRED-CODE-SELFTEST` | ci | P0 | CLEAN-401 新增检查器；DOC-403 登记（2026-09-21） |
| `CHK-ROOT-CLEAN` | KEPT | `CHK-ROOT-CLEAN` | doc | P0 | 仓库根目录整洁 |
| `CHK-RUN-MANIFEST-SCHEMA` | KEPT | `CHK-RUN-MANIFEST-SCHEMA` | extension | P0 | RULING-DOC-01（2026-09-22）新增执行单元（非本波基线，absorbed）：负责人裁决 D（run manifest 以冻结 schema 为准 + 补门） |
| `CHK-RUN-MANIFEST-SCHEMA-SELFTEST` | KEPT | `CHK-RUN-MANIFEST-SCHEMA-SELFTEST` | extension | P0 | RULING-DOC-01（2026-09-22）新增执行单元（非本波基线，absorbed）：负责人裁决 D（run manifest 以冻结 schema 为准 + 补门） |
| `CHK-SCHED-PROBE-SCHEMA-STEP` | KEPT | `CHK-SCHED-PROBE-SCHEMA` | doc | P0 | ARCH-502 新增执行单元 |
| `CHK-SECRET-HYGIENE` | KEPT | `CHK-SECRET-HYGIENE` | extension | P0 | 凭据/密钥卫生（ROOT-006 注册，absorbed） |
| `CHK-SPARSE-PUNCH` | KEPT | `CHK-SPARSE-PUNCH` | extension | P0 | TRIM-LAND-01 新增执行单元（非本波基线，absorbed）：裸形态体积削减（文件系统打洞）落地与判据 |
| `CHK-SPARSE-PUNCH-PROBE` | KEPT | `CHK-SPARSE-PUNCH-PROBE` | extension | P0 | TRIM-LAND-01 新增执行单元（非本波基线，absorbed）：裸形态体积削减（文件系统打洞）落地与判据 |
| `CHK-SPEC-NAMED-IMPL-ON-PROD-PATH` | KEPT | `CHK-SPEC-NAMED-IMPL-ON-PROD-PATH` | extension | P0 | DESIGN-GAP-LAND（2026-09-19）新增执行单元（非基线 146 项，absorbed）：GAP_AUDIT §9.44 负责人令新增门禁 CHK-SPEC-NAMED-IMPL-ON-PROD-PATH，覆盖 S1-002/S2-NS-01/S4-P3X-06 三条同类缺陷；判据+可执行负例见 eng/ci/check_spec_named_impl.py --self-test。 |
| `CHK-TEST-DISCRIMINATIVE-STEP` | KEPT | `CHK-TEST-DISCRIMINATIVE` | doc | P0 | GATE-502 新增执行单元（空断言静态门） |
| `CI-BINDING-TESTS` | MERGED-INTO | `CHK-MODULE-MANIFEST` | doc | P0 | 模块 manifest/注册表/构建 target/产品清单一致 |
| `CI-CONTRACT-BOOTSTRAP-UTF8` | KEPT | `CHK-CI-CONTRACT-SELFTESTS` | extension | P1 | 注册表已登记的执行单元，按 KEPT 归入其父项 |
| `CI-CONTRACT-CTEST-REGISTRATION` | KEPT | `CHK-CI-CONTRACT-SELFTESTS` | extension | P1 | 注册表已登记的执行单元，按 KEPT 归入其父项 |
| `CI-CONTRACT-FATDUCK-WORKFLOW` | KEPT | `CHK-CI-CONTRACT-SELFTESTS` | extension | P1 | 注册表已登记的执行单元，按 KEPT 归入其父项 |
| `CI-CONTRACT-GAP4-EMPTY-OUTPUTS` | KEPT | `CHK-CI-CONTRACT-SELFTESTS` | extension | P1 | 注册表已登记的执行单元，按 KEPT 归入其父项 |
| `CI-CONTRACT-IMPACT-MAP` | KEPT | `CHK-CI-CONTRACT-SELFTESTS` | extension | P1 | 注册表已登记的执行单元，按 KEPT 归入其父项 |
| `CI-CONTRACT-NEGATIVE-GUARDS` | KEPT | `CHK-CI-CONTRACT-SELFTESTS` | extension | P1 | 注册表已登记的执行单元，按 KEPT 归入其父项 |
| `CI-CONTRACT-RESOURCE-MONITOR-SHIM` | KEPT | `CHK-CI-CONTRACT-SELFTESTS` | extension | P1 | 注册表已登记的执行单元，按 KEPT 归入其父项 |
| `CI-CONTRACT-RESOURCE-PROBE` | KEPT | `CHK-CI-CONTRACT-SELFTESTS` | extension | P1 | 注册表已登记的执行单元，按 KEPT 归入其父项 |
| `CI-CONTRACT-RUN-PREFIX-PROBE` | KEPT | `CHK-CI-CONTRACT-SELFTESTS` | extension | P1 | 注册表已登记的执行单元，按 KEPT 归入其父项 |
| `CI-CONTRACT-RUNNER-CONTRACT` | KEPT | `CHK-CI-CONTRACT-SELFTESTS` | extension | P1 | 注册表已登记的执行单元，按 KEPT 归入其父项 |
| `CI-CONTRACT-RUNNER-EXECUTION` | KEPT | `CHK-CI-CONTRACT-SELFTESTS` | extension | P1 | 注册表已登记的执行单元，按 KEPT 归入其父项 |
| `CI-CONTRACT-RUNNER-SELECTION` | KEPT | `CHK-CI-CONTRACT-SELFTESTS` | extension | P1 | 注册表已登记的执行单元，按 KEPT 归入其父项 |
| `CI-CONTRACT-VERIFY-REMOTE-RUN` | KEPT | `CHK-CI-CONTRACT-SELFTESTS` | extension | P1 | 注册表已登记的执行单元，按 KEPT 归入其父项 |
| `CI-CONTRACT-WINDOWS-CI` | KEPT | `CHK-CI-CONTRACT-SELFTESTS` | extension | P1 | 注册表已登记的执行单元，按 KEPT 归入其父项 |
| `CI-CONTRACT-WORKFLOW-LOCK` | KEPT | `CHK-CI-CONTRACT-SELFTESTS` | extension | P1 | 注册表已登记的执行单元，按 KEPT 归入其父项 |
| `CI-INTEGRATION-CI-REPAIR-ROUND` | KEPT | `CHK-CI-INTEGRATION-SELFTESTS` | extension | P1 | 注册表已登记的执行单元，按 KEPT 归入其父项 |
| `CI-INTEGRATION-CI001-FAILCLOSED` | KEPT | `CHK-CI-INTEGRATION-SELFTESTS` | extension | P1 | 注册表已登记的执行单元，按 KEPT 归入其父项 |
| `CI-INTEGRATION-RUN-MONITORED` | KEPT | `CHK-CI-INTEGRATION-SELFTESTS` | extension | P1 | 注册表已登记的执行单元，按 KEPT 归入其父项 |
| `CLI-COMMAND-LAYER` | MERGED-INTO | `API-DOCS` | doc | P0 | doc↔code 命令树/签名/退出码/schema 一致 |
| `CLI-RUN-PRESET` | MERGED-INTO | `API-DOCS` | doc | P0 | doc↔code 命令树/签名/退出码/schema 一致 |
| `CON-API-CONTRACTS` | MERGED-INTO | `API-DOCS` | doc | P0 | doc↔code 命令树/签名/退出码/schema 一致 |
| `CON-BUILD-GRAPH` | MERGED-INTO | `CHK-MODULE-MANIFEST` | doc | P0 | 模块 manifest/注册表/构建 target/产品清单一致 |
| `CON-COMMENTS` | MERGED-INTO | `CHK-STALE-DOC` | doc | P1 | 活动文档无陈旧版本号/历史状态冒充 |
| `CON-CONFIG-CONTRACTS` | MERGED-INTO | `CHK-SCHEMA` | doc | P0 | schema 校验 |
| `CON-DOC-SYMBOLS` | MERGED-INTO | `CHK-DANGLING` | doc | P1 | 删除/重命名无悬空引用 |
| `CON-EXECUTION-CONTRACTS` | MERGED-INTO | `CHK-RESOURCE` | doc | P0 | 内存/线程/利用率门禁 |
| `CON-EXECUTION-CONTRACTS-SELFTEST` | MERGED-INTO | `CHK-RESOURCE` | extension | P0 | GATE-501（RELEASE-05）补登记：既有执行单元缺迁移映射登记（注册表结构校验器 R13 孤儿 unit），按注册表父项补登，不改判据 |
| `CON-FORBIDDEN-PATTERNS` | MERGED-INTO | `CHK-STATIC` | doc | P1 | 静态分析 |
| `CON-FULL-INTEGRATION` | MERGED-INTO | `CHK-CONTRACT-TEST` | doc | P0 | 核心合同有独立测试 |
| `CON-SCIENCE-UNITS` | MERGED-INTO | `CHK-SCI-REF` | doc | P0 | 算法引用有效 SCI/ALG |
| `CON-TEST-CONTRACTS` | MERGED-INTO | `CHK-CONTRACT-TEST` | doc | P0 | 核心合同有独立测试 |
| `CON-TRACEABILITY` | MERGED-INTO | `CHK-SCI-REF` | doc | P0 | 算法引用有效 SCI/ALG |
| `CONTRACT-GRAPH` | MERGED-INTO | `CHK-CONTRACT-REF` | doc | P0 | 端口引用有效 DATA 合同（canonical=eng/contracts/schemas/unified/**、docs/contracts/unified_object_registry.json） |
| `CTEST-AIO-ABI-LAYOUT-LOCK` | KEPT | `CHK-ABI` | extension | P1 | W4-A3（2026-09-17）新增执行单元：收口 CTEST-REGISTRATION 的 C3未注册目标 11 项（W4-A1 PSF 中心口径门 / P27 死字段锁 / P1NOISE NumPy oracle / SCI-FIX-WEIGHT 三合一证据门），均属非本波 146 项基线（absorbed） |
| `CTEST-AIO-ABI-LAYOUT-LOCK-SELFCHECK` | KEPT | `CHK-ABI` | extension | P1 | W4-A3（2026-09-17）新增执行单元：收口 CTEST-REGISTRATION 的 C3未注册目标 11 项（W4-A1 PSF 中心口径门 / P27 死字段锁 / P1NOISE NumPy oracle / SCI-FIX-WEIGHT 三合一证据门），均属非本波 146 项基线（absorbed） |
| `CTEST-AIO-ABI-NEGATIVE` | MERGED-INTO | `CHK-ABI` | doc | P0 | C ABI 兼容性 |
| `CTEST-AIO-ABI-SELFCHECK` | MERGED-INTO | `CHK-ABI` | doc | P0 | C ABI 兼容性 |
| `CTEST-AIO-ABI-UNITS` | MERGED-INTO | `CHK-ABI` | doc | P0 | C ABI 兼容性 |
| `CTEST-AIO-CHECKSUM` | MERGED-INTO | `CHK-UNIT` | extension | P0 | GATE-501（RELEASE-05）补登记：既有执行单元缺迁移映射登记（注册表结构校验器 R13 孤儿 unit），按注册表父项补登，不改判据 |
| `CTEST-AIO-HIPS-ATOMIC-PUBLISH` | MERGED-INTO | `CHK-UNIT` | extension | P0 | GATE-501（RELEASE-05）补登记：既有执行单元缺迁移映射登记（注册表结构校验器 R13 孤儿 unit），按注册表父项补登，不改判据 |
| `CTEST-AIO-HIPS-PUBLISH-ATOMIC` | MERGED-INTO | `CHK-INVARIANT` | doc | P0 | 科学不变量/性质测试 |
| `CTEST-AIO-HIPS-PUBLISH-ATOMIC-UNITS` | MERGED-INTO | `CHK-INVARIANT` | doc | P0 | 科学不变量/性质测试 |
| `CTEST-AIO-PRECISION_DUAL` | MERGED-INTO | `CHK-UNIT` | extension | P0 | GATE-501（RELEASE-05）补登记：既有执行单元缺迁移映射登记（注册表结构校验器 R13 孤儿 unit），按注册表父项补登，不改判据 |
| `CTEST-AIO-QUERY_PIXEL` | MERGED-INTO | `CHK-UNIT` | extension | P0 | GATE-501（RELEASE-05）补登记：既有执行单元缺迁移映射登记（注册表结构校验器 R13 孤儿 unit），按注册表父项补登，不改判据 |
| `CTEST-AIO-TILE_MODEL` | MERGED-INTO | `CHK-UNIT` | extension | P0 | GATE-501（RELEASE-05）补登记：既有执行单元缺迁移映射登记（注册表结构校验器 R13 孤儿 unit），按注册表父项补登，不改判据 |
| `CTEST-AIO-TRANSFORM` | MERGED-INTO | `CHK-UNIT` | extension | P0 | GATE-501（RELEASE-05）补登记：既有执行单元缺迁移映射登记（注册表结构校验器 R13 孤儿 unit），按注册表父项补登，不改判据 |
| `CTEST-BLOCK-FLOW` | KEPT | `CHK-ARCH505-BLOCK-FLOW` | doc | P0 | ARCH-505 新增执行单元 |
| `CTEST-COMPARE-PRODUCTS-QUALITY` | MERGED-INTO | `CHK-RESOURCE` | doc | P0 | 内存/线程/利用率门禁 |
| `CTEST-CORE-BLOCK-FRAME` | KEPT | `CHK-ARCH501-BLOCK-FRAME` | doc | P0 | ARCH-501 命名块生命周期 ctest 目标（新增执行单元） |
| `CTEST-DRIZZLE-PRECISION-DEFAULT` | MERGED-INTO | `CHK-INVARIANT` | doc | P0 | 科学不变量/性质测试 |
| `CTEST-EXPORT-STREAM` | KEPT | `CHK-ARCH504-EXPORT-STREAM` | doc | P0 | ARCH-504 新增执行单元 |
| `CTEST-FIX402-PHASE3-SEMANTIC-GUARD` | MERGED-INTO | `CHK-UNIT` | extension | P0 | GATE-501（RELEASE-05）补登记：既有执行单元缺迁移映射登记（注册表结构校验器 R13 孤儿 unit），按注册表父项补登，不改判据 |
| `CTEST-GAIA-MAGNITUDE-RANGE-BOUNDS` | MERGED-INTO | `CHK-INVARIANT` | doc | P0 | 科学不变量/性质测试 |
| `CTEST-GAIA-MANIFEST-BOUNDS` | MERGED-INTO | `CHK-INVARIANT` | doc | P0 | 科学不变量/性质测试 |
| `CTEST-GAIA-SHARD-COVERAGE` | MERGED-INTO | `CHK-INVARIANT` | doc | P0 | GAIA-FAILCLOSED-01 新增执行单元（G-1 shard 覆盖门：file_count==条目数 且 fail_count==0） |
| `CTEST-GAIA-SHARD-COVERAGE-SELF-TEST` | MERGED-INTO | `CHK-INVARIANT` | doc | P0 | GAIA-FAILCLOSED-01 新增执行单元（G-1 判据自检 --self-test，能红能绿） |
| `CTEST-IPV-ABI-LAYOUT-LOCK` | MERGED-INTO | `CHK-ABI` | doc | P0 | C ABI 兼容性 |
| `CTEST-IPV-ABI-LAYOUT-LOCK-SELFCHECK` | MERGED-INTO | `CHK-ABI` | doc | P0 | C ABI 兼容性 |
| `CTEST-IPV-DEAD-PARAMS-LOCK` | KEPT | `CHK-ABI` | extension | P1 | W4-A3（2026-09-17）新增执行单元：收口 CTEST-REGISTRATION 的 C3未注册目标 11 项（W4-A1 PSF 中心口径门 / P27 死字段锁 / P1NOISE NumPy oracle / SCI-FIX-WEIGHT 三合一证据门），均属非本波 146 项基线（absorbed） |
| `CTEST-IPV-DEAD-PARAMS-LOCK-SELFCHECK` | KEPT | `CHK-ABI` | extension | P1 | W4-A3（2026-09-17）新增执行单元：收口 CTEST-REGISTRATION 的 C3未注册目标 11 项（W4-A1 PSF 中心口径门 / P27 死字段锁 / P1NOISE NumPy oracle / SCI-FIX-WEIGHT 三合一证据门），均属非本波 146 项基线（absorbed） |
| `CTEST-IPV-ERROR-UTF8` | KEPT | `CHK-ABI` | extension | P0 | 注册表已登记的执行单元，按 KEPT 归入其父项 |
| `CTEST-IPV-EXTRACT-WCS-SIP-FAILCLOSED` | MERGED-INTO | `CHK-INVARIANT` | doc | P0 | 科学不变量/性质测试 |
| `CTEST-IPV-MAG-ITER` | MERGED-INTO | `CHK-ORACLE` | doc | P0 | SCI/ALG Oracle 测试 |
| `CTEST-IPV-MAG-ITER-DELIVERY` | MERGED-INTO | `CHK-ORACLE` | doc | P0 | SCI/ALG Oracle 测试 |
| `CTEST-IPV-PARAMS-ABI-FAILCLOSED` | MERGED-INTO | `CHK-ABI` | doc | P0 | C ABI 兼容性 |
| `CTEST-IPV-SELECT-DOMAIN` | KEPT | `CHK-ORACLE` | extension | P0 | 注册表已登记的执行单元，按 KEPT 归入其父项 |
| `CTEST-IPV-TRIANGLE-BUDGET` | MERGED-INTO | `CHK-INVARIANT` | doc | P0 | 科学不变量/性质测试 |
| `CTEST-LINUX-FULL` | MERGED-INTO | `CHK-UNIT` | doc | P0 | 每模块单测 |
| `CTEST-MEM-WIRE` | KEPT | `CHK-MEM-WIRE-01` | doc | P0 | MEM-WIRE-01 新增执行单元 |
| `CTEST-MON004-ENFORCEMENT` | MERGED-INTO | `CHK-RESOURCE` | doc | P0 | 内存/线程/利用率门禁 |
| `CTEST-MOSAIC-WINDOW` | KEPT | `CHK-ARCH503-MOSAIC-WIN` | doc | P0 | ARCH-503 新增执行单元 |
| `CTEST-NORMALIZE-WORKFLOW` | KEPT | `CHK-ARCH502-NORMALIZE-WF` | doc | P0 | ARCH-502 新增执行单元 |
| `CTEST-ORCH-CHECKPOINT-UNITS` | KEPT | `CHK-CONTRACT-TEST` | ci | P1 | ORCH-001 编排层共址测试（补登记） |
| `CTEST-ORCH-CLI-INTEGRATION` | KEPT | `CHK-CONTRACT-TEST` | ci | P1 | ORCH-001 编排层共址测试（补登记） |
| `CTEST-ORCH-LEGACY-CLI-SMOKE` | KEPT | `CHK-CONTRACT-TEST` | ci | P1 | ORCH-001 编排层共址测试（补登记） |
| `CTEST-ORCH-LEGACY-CLI-VALIDATE` | KEPT | `CHK-CONTRACT-TEST` | ci | P1 | ORCH-001 编排层共址测试（补登记） |
| `CTEST-ORCH-LOGGER-UNITS` | KEPT | `CHK-CONTRACT-TEST` | ci | P1 | ORCH-001 编排层共址测试（补登记） |
| `CTEST-ORCH-SATURATION-GATE` | KEPT | `CHK-CONTRACT-TEST` | ci | P1 | ORCH-001 编排层共址测试（补登记） |
| `CTEST-P1001-REAL-NODES` | MERGED-INTO | `CHK-SYNTH-P1` | doc | P0 | normalize 合成全链 |
| `CTEST-P1CAL-BIAS-INFLUENCE-GATE` | KEPT | `CHK-INVARIANT` | extension | P1 | W4-A3（2026-09-17）：登记新交付的 ctest 目标 p1cal_bias_influence_gate（CHK-INVARIANT 域门）；CTEST-REGISTRATION C3 要求新增/改名测试同提交登记 |
| `CTEST-P1DRZ-MERGE-PIPELINE-LOCK` | MERGED-INTO | `CHK-SYNTH-P2` | doc | P0 | mosaic 合成全链 |
| `CTEST-P1DRZ-TASKSET-INVARIANCE` | MERGED-INTO | `CHK-NWORKER` | doc | P0 | 1 vs N worker 数值一致 |
| `CTEST-P1NOISE-ABI-LAYOUT` | KEPT | `CHK-ABI` | extension | P1 | W4-A3（2026-09-17）：登记新交付的 ctest 目标 p1noise_abi_layout（CHK-ABI 域门）；CTEST-REGISTRATION C3 要求新增/改名测试同提交登记 |
| `CTEST-P1NOISE-MASK` | KEPT | `CHK-INVARIANT` | extension | P1 | W4-A3（2026-09-17）：登记新交付的 ctest 目标 p1noise_mask（CHK-INVARIANT 域门）；CTEST-REGISTRATION C3 要求新增/改名测试同提交登记 |
| `CTEST-P1NOISE-NUMPY-ORACLE` | KEPT | `CHK-ORACLE` | extension | P1 | W4-A3（2026-09-17）新增执行单元：收口 CTEST-REGISTRATION 的 C3未注册目标 11 项（W4-A1 PSF 中心口径门 / P27 死字段锁 / P1NOISE NumPy oracle / SCI-FIX-WEIGHT 三合一证据门），均属非本波 146 项基线（absorbed） |
| `CTEST-P1NOISE-SATURATION` | KEPT | `CHK-INVARIANT` | extension | P1 | W4-A3（2026-09-17）：登记新交付的 ctest 目标 p1noise_saturation（CHK-INVARIANT 域门）；CTEST-REGISTRATION C3 要求新增/改名测试同提交登记 |
| `CTEST-P1NOISE-SATURATION-SELFCHECK` | KEPT | `CHK-INVARIANT` | extension | P1 | W4-A3（2026-09-17）：登记新交付的 ctest 目标 p1noise_saturation_selfcheck（CHK-INVARIANT 域门）；CTEST-REGISTRATION C3 要求新增/改名测试同提交登记 |
| `CTEST-P1NOISE-SATURATION-WIRING` | KEPT | `CHK-INVARIANT` | extension | P1 | W4-A3（2026-09-17）：登记新交付的 ctest 目标 p1noise_saturation_wiring（CHK-INVARIANT 域门）；CTEST-REGISTRATION C3 要求新增/改名测试同提交登记 |
| `CTEST-P1PHOT-FIXGATES` | MERGED-INTO | `CHK-ORACLE` | doc | P0 | SCI/ALG Oracle 测试 |
| `CTEST-P1PSF-CENTER-CONTRACT` | KEPT | `CHK-INVARIANT` | extension | P1 | W4-A3（2026-09-17）新增执行单元：收口 CTEST-REGISTRATION 的 C3未注册目标 11 项（W4-A1 PSF 中心口径门 / P27 死字段锁 / P1NOISE NumPy oracle / SCI-FIX-WEIGHT 三合一证据门），均属非本波 146 项基线（absorbed） |
| `CTEST-P1PSF-CENTER-CONTRACT-ASTROPY` | KEPT | `CHK-INVARIANT` | extension | P1 | W4-A3（2026-09-17）新增执行单元：收口 CTEST-REGISTRATION 的 C3未注册目标 11 项（W4-A1 PSF 中心口径门 / P27 死字段锁 / P1NOISE NumPy oracle / SCI-FIX-WEIGHT 三合一证据门），均属非本波 146 项基线（absorbed） |
| `CTEST-P1PSF-CENTER-CONTRACT-NEG` | KEPT | `CHK-INVARIANT` | extension | P1 | W4-A3（2026-09-17）新增执行单元：收口 CTEST-REGISTRATION 的 C3未注册目标 11 项（W4-A1 PSF 中心口径门 / P27 死字段锁 / P1NOISE NumPy oracle / SCI-FIX-WEIGHT 三合一证据门），均属非本波 146 项基线（absorbed） |
| `CTEST-P1PSF-CENTROID-GATE` | KEPT | `CHK-INVARIANT` | extension | P1 | W4-A3（2026-09-17）新增执行单元：收口 CTEST-REGISTRATION 的 C3未注册目标 11 项（W4-A1 PSF 中心口径门 / P27 死字段锁 / P1NOISE NumPy oracle / SCI-FIX-WEIGHT 三合一证据门），均属非本波 146 项基线（absorbed） |
| `CTEST-P1PSF-CENTROID-GATE-NEG` | KEPT | `CHK-INVARIANT` | extension | P1 | W4-A3（2026-09-17）新增执行单元：收口 CTEST-REGISTRATION 的 C3未注册目标 11 项（W4-A1 PSF 中心口径门 / P27 死字段锁 / P1NOISE NumPy oracle / SCI-FIX-WEIGHT 三合一证据门），均属非本波 146 项基线（absorbed） |
| `CTEST-P1PSF-PRODPATH-CENTROID` | MERGED-INTO | `CHK-INVARIANT` | doc | P0 | 科学不变量/性质测试 |
| `CTEST-P1SNR-FRAME-PARITY` | MERGED-INTO | `CHK-INVARIANT` | doc | P0 | 科学不变量/性质测试 |
| `CTEST-P1SNR-LINUX-ALL` | MERGED-INTO | `CHK-NWORKER` | doc | P0 | 1 vs N worker 数值一致 |
| `CTEST-P1SNR-SCIENCE-ALL` | MERGED-INTO | `CHK-ORACLE` | doc | P0 | SCI/ALG Oracle 测试 |
| `CTEST-P1STAR-ANGLE-GUARD` | MERGED-INTO | `CHK-INVARIANT` | doc | P0 | 科学不变量/性质测试 |
| `CTEST-P1STAR-GUIDED` | KEPT | `CHK-INVARIANT` | extension | P1 | STARDET-01（星表引导检测按文档订正）：登记新交付的 ctest 目标 p1star_guided（CHK-INVARIANT 域门）；CTEST-REGISTRATION C3 要求新增/改名测试同提交登记 |
| `CTEST-P1STAR-MAD` | MERGED-INTO | `CHK-INVARIANT` | doc | P0 | 科学不变量/性质测试 |
| `CTEST-P1STARDET-NODE-GATE` | KEPT | `CHK-INVARIANT` | extension | P1 | STARDET-01（星表引导检测按文档订正）：登记新交付的 ctest 目标 p1stardet_node_gate（CHK-INVARIANT 域门）；CTEST-REGISTRATION C3 要求新增/改名测试同提交登记 |
| `CTEST-P1WCS-APBP` | MERGED-INTO | `CHK-ORACLE` | doc | P0 | SCI/ALG Oracle 测试 |
| `CTEST-P1WCS-ASTROPY-CROSS` | MERGED-INTO | `CHK-ORACLE` | doc | P0 | SCI/ALG Oracle 测试 |
| `CTEST-P1WCS-CLOSURE-METRIC-GATE` | KEPT | `CHK-ORACLE` | extension | P1 | W4-A3：登记新交付的 ctest 目标 p1wcs_closure_metric_gate（CHK-ORACLE 域门）；CTEST-REGISTRATION C3 要求新增/改名测试同提交登记 |
| `CTEST-P1WCS-STD-F1-BRIDGE` | MERGED-INTO | `CHK-ORACLE` | doc | P0 | SCI/ALG Oracle 测试 |
| `CTEST-P2001-REAL-NODES` | MERGED-INTO | `CHK-SYNTH-P2` | doc | P0 | mosaic 合成全链 |
| `CTEST-P2002-UNC-REJ-PROV` | MERGED-INTO | `CHK-SYNTH-P2` | doc | P0 | mosaic 合成全链 |
| `CTEST-P2HIPS-DETERMINISM` | MERGED-INTO | `CHK-INVARIANT` | doc | P0 | 科学不变量/性质测试 |
| `CTEST-P2HIPS-NEGATIVE` | MERGED-INTO | `CHK-INVARIANT` | doc | P0 | 科学不变量/性质测试 |
| `CTEST-P2HIPS-SELFCHECK` | MERGED-INTO | `CHK-INVARIANT` | doc | P0 | 科学不变量/性质测试 |
| `CTEST-P2HIPS-UNITS` | MERGED-INTO | `CHK-INVARIANT` | doc | P0 | 科学不变量/性质测试 |
| `CTEST-P3-EXPORT-STREAM-RSS` | KEPT | `CHK-P3-EXPORT-STREAM-RSS` | extension | P0 | P3-STREAM-01（2026-09-22）新增执行单元（非本波基线，absorbed）：ctest 目标 p3_export_stream_prod 经资源监视器跑内核记账驻留判据（clear_refs 归零后读 VmHWM）。REGMAP-FIX-01 补登记迁移映射（R13 孤儿 unit 缺口，仅登记不改判据）。 |
| `CTEST-P3-PROJECTION-FAULT` | MERGED-INTO | `CHK-SYNTH-P3` | doc | P0 | export 合成全链 |
| `CTEST-P3-PROJECTION-UNITS` | MERGED-INTO | `CHK-SYNTH-P3` | doc | P0 | export 合成全链 |
| `CTEST-P3-SAMPLER-CACHE` | MERGED-INTO | `CHK-SYNTH-P3` | doc | P0 | export 合成全链 |
| `CTEST-P3002-REAL-NODES` | MERGED-INTO | `CHK-SYNTH-P3` | doc | P0 | export 合成全链 |
| `CTEST-P3002-UNCERTAINTY` | MERGED-INTO | `CHK-SYNTH-P3` | doc | P0 | export 合成全链 |
| `CTEST-PHASE2-GATES` | MERGED-INTO | `CHK-SYNTH-P2` | doc | P0 | mosaic 合成全链 |
| `CTEST-REGISTRATION` | MERGED-INTO | `CHK-MODULE-MANIFEST` | doc | P0 | 模块 manifest/注册表/构建 target/产品清单一致 |
| `CTEST-RESOURCE-MONITOR-QUALITY` | MERGED-INTO | `CHK-RESOURCE` | doc | P0 | 内存/线程/利用率门禁 |
| `CTEST-RT001-UNIQUE-EXECUTOR` | MERGED-INTO | `CHK-RESOURCE` | doc | P0 | 内存/线程/利用率门禁 |
| `CTEST-SCI-EVIDENCE-REGISTRATION` | KEPT | `CHK-MODULE-MANIFEST` | extension | P1 | W4-A3（2026-09-17）新增执行单元：收口 CTEST-REGISTRATION 的 C3未注册目标 11 项（W4-A1 PSF 中心口径门 / P27 死字段锁 / P1NOISE NumPy oracle / SCI-FIX-WEIGHT 三合一证据门），均属非本波 146 项基线（absorbed） |
| `CTEST-V6-P2-SKY-KAPPA` | KEPT | `CHK-SCI502-SKY-KAPPA` | doc | P0 | SCI-502 FIX-3 κ 自适应 ctest 目标（新增执行单元） |
| `CTEST-XPSD-SPECTRUM-COUNT-BOUNDS` | MERGED-INTO | `CHK-INVARIANT` | doc | P0 | 科学不变量/性质测试 |
| `DATA-ARTIFACTS` | MERGED-INTO | `CHK-CONTRACT-REF` | doc | P0 | 端口引用有效 DATA 合同（canonical=eng/contracts/schemas/unified/**、docs/contracts/unified_object_registry.json） |
| `DEEP-CLANG-BUILD` | MERGED-INTO | `CHK-BUILD-LINUX` | doc | P0 | Linux Release 构建 |
| `DEEP-COMPLEXITY` | MERGED-INTO | `CHK-STATIC` | doc | P1 | 静态分析 |
| `DEEP-COV-CPP` | MERGED-INTO | `CHK-COVERAGE` | doc | P2 | 覆盖率报告 |
| `DEEP-COV-PY` | MERGED-INTO | `CHK-COVERAGE` | doc | P2 | 覆盖率报告 |
| `DEEP-SAN-ASAN` | MERGED-INTO | `CHK-SANITIZER` | doc | P1 | ASan/UBSan |
| `DEEP-SAN-TSAN` | MERGED-INTO | `CHK-SANITIZER` | doc | P1 | ASan/UBSan |
| `DOC-INDEX` | KEPT | `DOC-INDEX` | doc | P1 | DOC-403（2026-09-21）：原为 CHK-DANGLING 的 step，提升为顶层注册项（顶层化后 docs/ci/01_CHECKS.md §2 才能有自己的登记行，满足 CHK-REGISTRY-DOC-SYNC 双向一致） |
| `DOC-INDEX-SELFTEST` | KEPT | `DOC-INDEX-SELFTEST` | ci | P0 | DOC-403（2026-09-21）新增执行单元（ENGINEERING_SPEC §10 可执行负例面） |
| `DOC-L0` | KEPT | `DOC-L0` |  |  | CI-003（2026-09-16）：原 RETIRE-PENDING-GOV-001 判定撤销 —— 该门经判据迁移/修锚/加可执行负例后保留为活动门，已登记 docs/ci/01_CHECKS.md §2。 |
| `DOC-LINE-ANCHORS` | MERGED-INTO | `CHK-SCI-REF` | doc | P0 | 算法引用有效 SCI/ALG |
| `DUPLICATION` | MERGED-INTO | `CHK-STATIC` | doc | P1 | 静态分析 |
| `E2E-CHAIN-SELFTEST-STEP` | KEPT | `CHK-E2E-CHAIN-SELFTEST` | doc | P0 | E2E-501 新增执行单元 |
| `E2E-CHAIN-VERIFY-STEP` | KEPT | `CHK-E2E-CHAIN` | doc | P0 | E2E-501 新增执行单元 |
| `E2E-WCS-CLOSURE-REPRO-SELFTEST` | KEPT | `CHK-E2E-REPRO` | ci | P0 | G-P1-WCS-CLOSURE-REPRO 自检：7 类注入必红 + 缺输入 fail-closed 必红 |
| `ENG-CONSTRAINTS` | KEPT | `ENG-CONSTRAINTS` |  |  | CI-003（2026-09-16）：原 RETIRE-PENDING-GOV-001 判定撤销 —— 该门经判据迁移/修锚/加可执行负例后保留为活动门，已登记 docs/ci/01_CHECKS.md §2。 |
| `EVT-FIELD-SETS` | KEPT | `CHK-SCHEMA` | doc | P0 | EVTFIELD-01 新增执行单元（事件协议 per-kind 扩展字段集跨面一致门，正本 protocol.h::missing_required_extension_v1） |
| `EVT-FIELD-SETS-SELFTEST` | KEPT | `CHK-SCHEMA` | doc | P0 | EVTFIELD-01 新增执行单元（15 组逐面负例 + 11 组 fail-closed 负例自检） |
| `EXIT-CONSISTENCY-SCAN` | KEPT | `CHK-EXIT-CONSISTENCY` | ci | P1 | 扫描 eng/tools/**+eng/ci/**：打印 FAIL 必须 rc≠0 |
| `EXIT-CONSISTENCY-SELFTEST` | KEPT | `CHK-EXIT-CONSISTENCY` | ci | P1 | 自测面：3 正例 + 3 负例 |
| `FAILCLOSED-SURVEY` | MERGED-INTO | `CHK-FAILCLOSED-SURVEY` | extension | P0 | 新增执行单元（GATE-501）（非本波 146 项基线，absorbed） |
| `FAILCLOSED-SURVEY-SELFTEST` | MERGED-INTO | `CHK-FAILCLOSED-SURVEY` | extension | P0 | 新增执行单元（GATE-501，普查自身能红能绿）（非本波 146 项基线，absorbed） |
| `GLOSSARY-DOCS` | KEPT | `GLOSSARY-DOCS` |  |  | CI-003（2026-09-16）：原 RETIRE-PENDING-GOV-001 判定撤销 —— 该门经判据迁移/修锚/加可执行负例后保留为活动门，已登记 docs/ci/01_CHECKS.md §2。 |
| `ID-MIGRATION-MAP-DOC-SYNC` | KEPT | `CHK-REGISTRY-DOC-SYNC` | extension | P0 | 并发写回后重建：注册表已登记的执行单元，按 KEPT 归入其父项 |
| `ID-MIGRATION-MAP-DOC-SYNC-SELFTEST` | KEPT | `CHK-REGISTRY-DOC-SYNC` | extension | P0 | 并发写回后重建：注册表已登记的执行单元，按 KEPT 归入其父项 |
| `IMPACT-MAP` | KEPT | `CHK-IMPACT-MAP` | extension | P0 | W4-A3（2026-09-16）新增执行单元（非本波 146 项基线，absorbed）：收口 eng/ci/tests/test_impact_map.py 4 条既有红的根因（step id 与顶层 id 混用） |
| `IMPACT-MAP-SELFTEST` | KEPT | `CHK-IMPACT-MAP` | extension | P0 | W4-A3（2026-09-16）新增执行单元（非本波 146 项基线，absorbed）：收口 eng/ci/tests/test_impact_map.py 4 条既有红的根因（step id 与顶层 id 混用） |
| `IPV-PLATFORM-BINDING` | MERGED-INTO | `CHK-INVARIANT` | doc | P0 | 科学不变量/性质测试 |
| `ISA-LEAK-SELFTEST` | MERGED-INTO | `CHK-ISA-EQ` | doc | P1 | baseline/AVX2/AVX-512 等价 |
| `KNOWN-FAILURES-BASELINE` | MERGED-INTO | `CHK-KNOWN-FAILURES-BASELINE` | extension | P1 | 版本化已知失败基线门（聚合型，linux-main 末位） |
| `KNOWN-FAILURES-BASELINE-CHECK` | MERGED-INTO | `CHK-KNOWN-FAILURES-BASELINE` | extension | P1 | 版本化已知失败基线门（聚合型，linux-main 末位） |
| `KNOWN-FAILURES-BASELINE-VERIFY` | MERGED-INTO | `CHK-KNOWN-FAILURES-BASELINE` | extension | P1 | 版本化已知失败基线门（聚合型，linux-main 末位） |
| `L2-FROZEN-GATE-REPLAY` | KEPT | `L2-FROZEN-GATE-REPLAY` | extension | P0 | GATE-501（RELEASE-05）新增执行单元（非本波 146 项基线，absorbed）：python3 eng/ci/check_frozen_gate.py --replay --json-out run/ci/l2-frozen-gate/replay.json |
| `L2-FROZEN-GATE-SELFTEST` | KEPT | `L2-FROZEN-GATE-SELFTEST` | extension | P0 | GATE-501（RELEASE-05）新增执行单元（非本波 146 项基线，absorbed）：python3 eng/ci/check_frozen_gate.py --self-test |
| `LINUX-MAIN-BUILD-TREE` | KEPT-PENDING-MERGE | `CHK-BUILD-LINUX` | doc | P0 | Linux Release 构建 |
| `LINUX-MAIN-FIXTURES` | KEPT-PENDING-MERGE | `CHK-BUILD-LINUX` | doc | P0 | Linux Release 构建 |
| `LOG-CONTRACT-SELFCHECK` | MERGED-INTO | `CHK-SCHEMA` | doc | P0 | schema 校验 |
| `LOG-SYS-SCAN` | KEPT | `CHK-LOG-SYS` | doc | P0 | LOG-SYS-01 新增执行单元（日志与错误系统判据） |
| `LOG-SYS-SELFTEST` | KEPT | `CHK-LOG-SYS` | doc | P0 | LOG-SYS-01 新增执行单元（6 类故障注入必红） |
| `MODULE-READMES` | MERGED-INTO | `CHK-MODULE-MANIFEST` | doc | P0 | 模块 manifest/注册表/构建 target/产品清单一致 |
| `NO-SERIAL-HEAVY` | MERGED-INTO | `CHK-RESOURCE` | doc | P0 | 内存/线程/利用率门禁 |
| `P3-PROJ-DECL-REGISTRY` | KEPT | `CHK-P3-PROJ-DECL` | extension | P0 | RELEASE-03 BLD-201（2026-09-20）新增执行单元（非本波基线，absorbed）：各判据来源见工程控制/RELEASE-03/change-claims/** 与 run/RELEASE-03/logs/*-receipt.md；可执行负例面见各检查器 --self-test 或登记命令内建的负例用例。 |
| `P3-PROJ-DECL-SELFTEST` | KEPT | `CHK-P3-PROJ-DECL-SELFTEST` | extension | P0 | RELEASE-03 BLD-201（2026-09-20）新增执行单元（非本波基线，absorbed）：各判据来源见工程控制/RELEASE-03/change-claims/** 与 run/RELEASE-03/logs/*-receipt.md；可执行负例面见各检查器 --self-test 或登记命令内建的负例用例。 |
| `P3-PROJ-DECL-UNSUPPORTED-CLI` | KEPT | `CHK-P3-PROJ-DECL` | extension | P0 | RELEASE-03 BLD-201（2026-09-20）新增执行单元（非本波基线，absorbed）：各判据来源见工程控制/RELEASE-03/change-claims/** 与 run/RELEASE-03/logs/*-receipt.md；可执行负例面见各检查器 --self-test 或登记命令内建的负例用例。 |
| `P3-STATUS` | MERGED-INTO | `CHK-MODULE-MANIFEST` | doc | P0 | 模块 manifest/注册表/构建 target/产品清单一致 |
| `PIPELINE-TRACE` | MERGED-INTO | `CHK-SCI-REF` | doc | P0 | 算法引用有效 SCI/ALG |
| `PKG-CONSISTENCY` | KEPT | `CHK-PKG-CONSISTENCY` | extension | P0 | W5-PKG-001（2026-09-17）交付；W4-A3 按 eng/ci/checks.json 注册（非本波 146 项基线，absorbed） |
| `PKG-CONSISTENCY-NEG` | KEPT | `CHK-PKG-CONSISTENCY` | extension | P0 | W5-PKG-001（2026-09-17）交付；W4-A3 按 eng/ci/checks.json 注册（非本波 146 项基线，absorbed） |
| `PKG-SBOM` | KEPT | `CHK-PKG-CONSISTENCY` | extension | P0 | W5-PKG-001（2026-09-17）交付；W4-A3 按 eng/ci/checks.json 注册（非本波 146 项基线，absorbed） |
| `PKG-SBOM-NEG` | KEPT | `CHK-PKG-CONSISTENCY` | extension | P0 | W5-PKG-001（2026-09-17）交付；W4-A3 按 eng/ci/checks.json 注册（非本波 146 项基线，absorbed） |
| `PREFLIGHT-MATRIX-STEP` | KEPT | `CHK-PREFLIGHT-MATRIX` | doc | P0 | E2E-501 新增执行单元 |
| `PROD-REACH-SELFTEST` | MERGED-INTO | `CHK-STATIC` | doc | P1 | 静态分析 |
| `PRODUCTION-GRAPH` | MERGED-INTO | `CHK-SCI-REF` | doc | P0 | 算法引用有效 SCI/ALG |
| `PSFSW-RETIRED-STATIC` | KEPT | `CHK-PSFSW-RETIRED-STATIC` | extension | P1 | RELEASE-03 BLD-201（2026-09-20）新增执行单元（非本波基线，absorbed）：各判据来源见工程控制/RELEASE-03/change-claims/** 与 run/RELEASE-03/logs/*-receipt.md；可执行负例面见各检查器 --self-test 或登记命令内建的负例用例。 |
| `PSFSW-RETIRED-STATIC-SELFTEST` | KEPT | `CHK-PSFSW-RETIRED-STATIC` | extension | P0 | GATE-501（RELEASE-05）新增执行单元（非本波 146 项基线，absorbed）：python3 eng/ci/check_psfsw_retired.py --self-test |
| `PY-TEST-INDEX-VERDICTS` | MERGED-INTO | `CHK-UNIT` |  |  | RELEASE-02（2026-09-18）新增；非本波 146 项基线，absorbed。 |
| `RECONCILE-STATE` | MERGED-INTO | `CHK-ENV-ADOPTION` | extension | P1 | CI 环境/接管基线（工具链策略、工作区接管证据、任务-证据对账） |
| `REGISTRY-DOC-SYNC` | KEPT | `CHK-REGISTRY-DOC-SYNC` | extension | P0 | CI-003（2026-09-16）新增执行单元（非本波 146 项基线，absorbed） |
| `REGISTRY-DOC-SYNC-SELFTEST` | KEPT | `CHK-REGISTRY-DOC-SYNC` | extension | P0 | CI-003（2026-09-16）新增执行单元（非本波 146 项基线，absorbed） |
| `RESOURCE-GATE-REAL` | KEPT | `RESOURCE-GATE-REAL` | extension | P0 | CI-003（2026-09-16）新增执行单元（非本波 146 项基线，absorbed） |
| `RESOURCE-GATE-REAL-NEG` | KEPT | `RESOURCE-GATE-REAL-NEG` | extension | P0 | CI-003（2026-09-16）新增执行单元（非本波 146 项基线，absorbed） |
| `SELFTEST-P3-EXPORT-STREAM-PROD` | KEPT | `CHK-P3-EXPORT-STREAM-PROD` | extension | P0 | P3-STREAM-01（2026-09-22）新增执行单元（非本波基线，absorbed）：上项的可执行负例面（同一检查器 --self-test）。REGMAP-FIX-01 补登记迁移映射（R13 孤儿 unit 缺口，仅登记不改判据）。 |
| `SELFTEST-PLUGIN-SYMBOL-CLOSURE` | KEPT | `CHK-PLUGIN-SYMBOL-CLOSURE` | extension | P0 | 新增执行单元（非本波基线，absorbed）：上项的可执行负例面（同一检查器 --self-test，未纳入闭包的符号引用 / DT_NEEDED 不可解析 / 构建图缺失各自判红）。 |
| `SERIAL-HARDCODE` | MERGED-INTO | `CHK-RESOURCE` | doc | P0 | 内存/线程/利用率门禁 |
| `SERIAL-HEAVY-SELFTEST` | MERGED-INTO | `CHK-RESOURCE` | doc | P0 | 内存/线程/利用率门禁 |
| `SPEC-NAMED-IMPL-ON-PROD-PATH` | KEPT | `CHK-SPEC-NAMED-IMPL-ON-PROD-PATH` | extension | P0 | DESIGN-GAP-LAND（2026-09-19）新增执行单元（非基线 146 项，absorbed）：GAP_AUDIT §9.44 负责人令新增门禁 CHK-SPEC-NAMED-IMPL-ON-PROD-PATH，覆盖 S1-002/S2-NS-01/S4-P3X-06 三条同类缺陷；判据+可执行负例见 eng/ci/check_spec_named_impl.py --self-test。 |
| `SPEC-NAMED-IMPL-ON-PROD-PATH-SELFTEST` | KEPT | `CHK-SPEC-NAMED-IMPL-ON-PROD-PATH` | extension | P0 | DESIGN-GAP-LAND（2026-09-19）新增执行单元（非基线 146 项，absorbed）：GAP_AUDIT §9.44 负责人令新增门禁 CHK-SPEC-NAMED-IMPL-ON-PROD-PATH，覆盖 S1-002/S2-NS-01/S4-P3X-06 三条同类缺陷；判据+可执行负例见 eng/ci/check_spec_named_impl.py --self-test。 |
| `STATIC-P3-EXPORT-STREAM-PROD` | KEPT | `CHK-P3-EXPORT-STREAM-PROD` | extension | P0 | P3-STREAM-01（2026-09-22）新增执行单元（非本波基线，absorbed）：静态锁生产 writer 节点引用 ExportStreamScheduler，3 处变异必红的可执行负例见 eng/tools/arch/check_p3_export_stream_prod.py --self-test。REGMAP-FIX-01 补登记迁移映射（R13 孤儿 unit 缺口，仅登记不改判据）。 |
| `STATIC-PLUGIN-SYMBOL-CLOSURE` | KEPT | `CHK-PLUGIN-SYMBOL-CLOSURE` | extension | P0 | 新增执行单元（非本波基线，absorbed）：plugin .so 符号闭包静态判据 —— 强未定义符号必须能在 DT_NEEDED 传递闭包 ∪ 宿主基线库内解析，缺产物/缺构建图 fail-closed。 |
| `STD-REG` | KEPT | `STD-REG` | extension | P0 | CI-003（2026-09-16）新增执行单元（非本波 146 项基线，absorbed） |
| `STD-REG-FI-DANGLING` | KEPT | `STD-REG` | extension | P0 | CI-003（2026-09-16）新增执行单元（非本波 146 项基线，absorbed） |
| `STD-REG-FI-VERSION-DRIFT` | KEPT | `STD-REG` | extension | P0 | CI-003（2026-09-16）新增执行单元（非本波 146 项基线，absorbed） |
| `TASK-RESULT-SCHEMA` | MERGED-INTO | `CHK-SCHEMA` | doc | P0 | schema 校验 |
| `TEST-INDEX-LIVE` | MERGED-INTO | `CHK-UNIT` |  |  | 本波新增执行单元，登记为 absorbed（不计入 source_entry_count）。 |
| `TESTKIT-LIST` | MERGED-INTO | `CHK-UNIT` | doc | P0 | 每模块单测 |
| `THREAD-BUDGET` | MERGED-INTO | `CHK-RESOURCE` | doc | P0 | 内存/线程/利用率门禁 |
| `TOOLCHAIN-VERIFY` | MERGED-INTO | `CHK-ENV-ADOPTION` | extension | P1 | CI 环境/接管基线（工具链策略、工作区接管证据、任务-证据对账） |
| `TRACEABILITY` | MERGED-INTO | `CHK-SCI-REF` | doc | P0 | 算法引用有效 SCI/ALG |
| `TRACEABILITY-MATRIX` | MERGED-INTO | `CHK-CONTRACT-TEST` | doc | P0 | 核心合同有独立测试 |
| `UNIT-CLOSURE` | MERGED-INTO | `CHK-SCI-REF` | doc | P0 | 算法引用有效 SCI/ALG |
| `UT-ABI` | MERGED-INTO | `CHK-ABI` | doc | P0 | C ABI 兼容性 |
| `UT-API` | MERGED-INTO | `CHK-UNIT` | doc | P0 | 每模块单测 |
| `UT-ARCH` | MERGED-INTO | `CHK-UNIT` | doc | P0 | 每模块单测 |
| `UT-ARTIFACT` | MERGED-INTO | `CHK-UNIT` | doc | P0 | 每模块单测 |
| `UT-BACKEND` | MERGED-INTO | `CHK-UNIT` | doc | P0 | 每模块单测 |
| `UT-CLI` | MERGED-INTO | `CHK-UNIT` | doc | P0 | 每模块单测 |
| `UT-CONFIG` | MERGED-INTO | `CHK-UNIT` | doc | P0 | W5-CFG-002（2026-09-17）交付 eng/tests/config（47 例 rc=0）；W4-A3 注册为 CHK-UNIT 执行单元（原无 CI 步骤） |
| `UT-CONTRACTS` | MERGED-INTO | `CHK-CONTRACT-TEST` | doc | P0 | 核心合同有独立测试 |
| `UT-CPU-AVX2` | MERGED-INTO | `CHK-ISA-EQ` | doc | P1 | baseline/AVX2/AVX-512 等价 |
| `UT-CPU-AVX512` | MERGED-INTO | `CHK-ISA-EQ` | doc | P1 | baseline/AVX2/AVX-512 等价 |
| `UT-CPU-BASELINE` | MERGED-INTO | `CHK-ORACLE` | doc | P0 | SCI/ALG Oracle 测试 |
| `UT-CPU-DISPATCH` | MERGED-INTO | `CHK-ISA-EQ` | doc | P1 | baseline/AVX2/AVX-512 等价 |
| `UT-GAIA-ZLIB` | MERGED-INTO | `CHK-UNIT` | doc | P0 | 每模块单测 |
| `UT-GLOSSARY` | MERGED-INTO | `CHK-UNIT` | doc | P0 | 每模块单测 |
| `UT-IO` | MERGED-INTO | `CHK-UNIT` | doc | P0 | 每模块单测 |
| `UT-MONITORING` | MERGED-INTO | `CHK-UNIT` | doc | P0 | 每模块单测 |
| `UT-PIPELINE` | MERGED-INTO | `CHK-UNIT` | doc | P0 | 每模块单测 |
| `UT-QUALITY` | MERGED-INTO | `CHK-UNIT` | doc | P0 | 每模块单测 |
| `UT-RUNTIME` | MERGED-INTO | `CHK-UNIT` | doc | P0 | 每模块单测 |
| `UT-RUNTIME-INTEGRATION` | KEPT | `CHK-UNIT` | test | P0 | CI-INCREMENTAL：eng/tests/runtime 的集成型用例拆出（真实会话/子进程），只进 integration 档 |
| `UT-SCIENCELINT` | MERGED-INTO | `CHK-UNIT` | doc | P0 | 每模块单测 |
| `UT-TRACEABILITY` | MERGED-INTO | `CHK-CONTRACT-TEST` | doc | P0 | 核心合同有独立测试 |
| `UT-VERSION` | MERGED-INTO | `CHK-UNIT` | doc | P0 | 每模块单测 |
| `V6-CLI-MODE-MATRIX` | MERGED-INTO | `CHK-SYNTH-P2` | doc | P0 | mosaic 合成全链 |
| `V6-CLI-MODE-ROUTING` | MERGED-INTO | `CHK-SYNTH-P2` | doc | P0 | mosaic 合成全链 |
| `V6-CTEST-INTEGRATION` | MERGED-INTO | `CHK-CONTRACT-TEST` | doc | P0 | 核心合同有独立测试 |
| `V6-CTEST-UNIT` | MERGED-INTO | `CHK-UNIT` | doc | P0 | 每模块单测 |
| `V6-DETERMINISM` | MERGED-INTO | `CHK-NWORKER` | doc | P0 | 1 vs N worker 数值一致 |
| `V6-NEGATIVE-MUTATION` | MERGED-INTO | `CHK-CONTRACT-TEST` | doc | P0 | 核心合同有独立测试 |
| `V6-RESOURCE-GATE-POLICY` | MERGED-INTO | `CHK-RESOURCE` | doc | P0 | 内存/线程/利用率门禁 |
| `V6-RUNTIME-CLOSURE` | MERGED-INTO | `CHK-ORACLE` | doc | P0 | SCI/ALG Oracle 测试 |
| `VERSION-CONSISTENCY` | KEPT | `VERSION-CONSISTENCY` |  |  | CI-003（2026-09-16）：原 RETIRE-PENDING-GOV-001 判定撤销 —— 该门经判据迁移/修锚/加可执行负例后保留为活动门，已登记 docs/ci/01_CHECKS.md §2。 |
| `VERSION-NAMESPACES` | KEPT | `VERSION-NAMESPACES` |  |  | CI-003（2026-09-16）：原 RETIRE-PENDING-GOV-001 判定撤销 —— 该门经判据迁移/修锚/加可执行负例后保留为活动门，已登记 docs/ci/01_CHECKS.md §2。 |
| `WARNING-SUPPRESSION` | MERGED-INTO | `CHK-WARN` | doc | P0 | 编译警告 |
| `WIN-BUILD-RELEASE` | MERGED-INTO | `CHK-BUILD-WIN` | doc | P0 | Windows Release 构建 |
| `WIN-CANDIDATE-VALIDATE` | KEPT-PENDING-MERGE | `CHK-PACKAGE` | doc | P0 | 发布候选打包/白名单/哈希/版本/provenance |
| `WIN-PACKAGE-CANDIDATE` | MERGED-INTO | `CHK-PACKAGE` | doc | P0 | 发布候选打包/白名单/哈希/版本/provenance |
| `WIN-TEST-UNIT` | MERGED-INTO | `CHK-UNIT` | doc | P0 | 每模块单测 |
| `WORKER-BALANCE-METRIC-REPLAY` | KEPT | `WORKER-BALANCE-METRIC-REPLAY` | extension | P0 | GATE-501（RELEASE-05）新增执行单元（非本波 146 项基线，absorbed）：python3 eng/ci/check_worker_balance.py --replay-archived --json-out run/ci/worker-balance/replay.json |
| `WORKER-BALANCE-METRIC-SELFTEST` | KEPT | `WORKER-BALANCE-METRIC-SELFTEST` | extension | P0 | GATE-501（RELEASE-05）新增执行单元（非本波 146 项基线，absorbed）：python3 eng/ci/check_worker_balance.py --self-test |
| `WORKFLOW-REGISTRY-BINDING` | MERGED-INTO | `CHK-MODULE-MANIFEST` | doc | P0 | 模块 manifest/注册表/构建 target/产品清单一致 |
| `WORKSPACE-ADOPTION` | MERGED-INTO | `CHK-ENV-ADOPTION` | extension | P1 | CI 环境/接管基线（工具链策略、工作区接管证据、任务-证据对账） |

### 5.1 判据实现者 / 归属 / 原始 reason（20 条）

| 旧 ID | 判据实现者（checker） | 归属（owner） | reason（JSON 逐字） |
|---|---|---|---|
| `AGENTS-GOV` | `eng/tools/check_agents_gov.py` | GOV-001 (W2) | AGENTS.md 硬禁令门实现者；GOV-001 迁移入目标 CHK-AGENT-HARD-RULES |
| `CHK-ALGO-WIRING` | `eng/ci/check_algo_wiring.py` | RELEASE-02 fix-gates | RELEASE-02 代码修复分片（CI 门禁防复发）新增检查项；判据+可执行负例见 eng/ci/check_algo_wiring.py --self-test。 |
| `CHK-CONFIG-CONSUMED` | `eng/ci/check_config_consumed.py` | RELEASE-02 fix-gates | RELEASE-02 代码修复分片（CI 门禁防复发）新增检查项；判据+可执行负例见 eng/ci/check_config_consumed.py --self-test。 |
| `CHK-CONFIG-DEFAULTS` | `eng/ci/check_config_defaults.py` | RELEASE-02 fix-gates | RELEASE-02 代码修复分片（CI 门禁防复发）新增检查项；判据+可执行负例见 eng/ci/check_config_defaults.py --self-test。 |
| `CHK-PROD-SCALE` | `eng/ci/check_prod_scale.py` | RELEASE-02 fix-gates | RELEASE-02 代码修复分片（CI 门禁防复发）新增检查项；判据+可执行负例见 eng/ci/check_prod_scale.py --self-test。 |
| `CHK-PROVENANCE-CONSISTENCY` | `eng/ci/check_provenance_consistency.py` | RELEASE-02 fix-gates | RELEASE-02 代码修复分片（CI 门禁防复发）新增检查项；判据+可执行负例见 eng/ci/check_provenance_consistency.py --self-test。 |
| `CHK-REALDATA-E2E` | `eng/ci/check_realdata_e2e.py` | RELEASE-02 fix-gates | RELEASE-02 代码修复分片（CI 门禁防复发）新增检查项；判据+可执行负例见 eng/ci/check_realdata_e2e.py --self-test。 |
| `CHK-PATH-DOMAIN-ANCHORS` | `eng/tools/quality/check_path_domain_anchors.py` | PATHDOMAIN-01 | changed_paths 是 glob 路径域，决定改了哪些文件要跑哪些门，但从来没有存在性判据——目录退役后该域静默失效。判据+可执行负例见 eng/tools/quality/check_path_domain_anchors.py --self-test。 |
| `CHK-REGISTRY-IR-PARITY` | `eng/ci/check_registry_ir_parity.py` | RELEASE-02 fix-gates | RELEASE-02 代码修复分片（CI 门禁防复发）新增检查项；判据+可执行负例见 eng/ci/check_registry_ir_parity.py --self-test。 |
| `DOC-L0` | `eng/tools/check_l0_docs.py` | GOV-001 (W2) | L0 固定文档/链接完整性；悬空引用门归 CHK-DANGLING（GOV-001 迁移） |
| `ENG-CONSTRAINTS` | `eng/tools/doccheck/check_engineering_constraints.py` | GOV-001 (W2) | 旧权威绑定（AstroCS_ENGINEERING_CONSTRAINTS.md 文本快照）清理归 GOV-001；语义继任者 = AGENTS.md 硬禁令门 |
| `GLOSSARY-DOCS` | `eng/tools/check_glossary.py` | GOV-001 (W2) | 词典锚点/别名唯一性；悬空引用门归 CHK-DANGLING（GOV-001 迁移） |
| `LINUX-MAIN-BUILD-TREE` | `（JSON 未登记）` | QA-001 / 后续 CI 治理任务（改 eng/ci/run.py 绑定同批） | 本波保持顶层：eng/ci/workflow_binding.json 的 wf_step 绑定以该 ID 声明 check 体，并入聚合需同步适配 eng/ci/validate_workflow_binding.py（超出 GO 的注册项-only 边界）；QA-001 改绑定后由后续任务并入目标 |
| `LINUX-MAIN-FIXTURES` | `（JSON 未登记）` | QA-001 / 后续 CI 治理任务（改 eng/ci/run.py 绑定同批） | 本波保持顶层：eng/ci/workflow_binding.json 的 wf_step 绑定以该 ID 声明 check 体，并入聚合需同步适配 eng/ci/validate_workflow_binding.py（超出 GO 的注册项-only 边界）；QA-001 改绑定后由后续任务并入目标 |
| `PY-TEST-INDEX-VERDICTS` | `eng/ci/check_test_index.py` | RELEASE-02 CI-HYGIENE | DESIGN-CONFORMANCE SUB-D-35：test_index.csv 无机器 verdict，含 errors/failures/skip 的套件被当作通过。本门把聚合判据机器化（PASS ⇔ failed==0∧errored==0；全 skip 不得记 PASS），并入 CHK-UNIT 聚合面。 |
| `TEST-INDEX-LIVE` | `eng/tools/doccheck/check_test_index_live.py` | DOC-DRIFT-FIX-01 | test_index.csv 是手工维护、无生成器；既有门只判「标注自洽」不判与实测一致，这正是陈旧登记能长期存活的根因。新判据把 verdict / cases-failed-errored-skipped / 耗时量级与实测对齐。 |
| `ALG-LINE-ANCHORS` | `eng/tools/doccheck/check_alg_line_anchors.py` | DOC-DRIFT-FIX-01 | 它守的两类（文档自述行数锚、逐符号表行号范围）没有其他门——既有行锚门只判「能解析 + 界内」，不判与源文件实测一致。 |
| `VERSION-CONSISTENCY` | `eng/ci/check_version.py` | GOV-001 (W2) | 硬绑定 --expected 0.11.0-alpha.2 旧版本号；陈旧版本号门归 CHK-STALE-DOC（GOV-001 迁移） |
| `VERSION-NAMESPACES` | `eng/tools/doccheck/check_version_namespaces.py` | GOV-001 (W2) | 五版本命名空间与最高设计 §12「版本信息下线」冲突；陈旧版本号门归 CHK-STALE-DOC（GOV-001 迁移） |
| `WIN-CANDIDATE-VALIDATE` | `（JSON 未登记）` | QA-001 / 后续 CI 治理任务（改 eng/ci/run.py 绑定同批） | 本波保持顶层：eng/ci/workflow_binding.json 的 wf_step 绑定以该 ID 声明 check 体，并入聚合需同步适配 eng/ci/validate_workflow_binding.py（超出 GO 的注册项-only 边界）；QA-001 改绑定后由后续任务并入目标 |

## 6. 本波无 RETIRE-PENDING 项：原 GOV-001 冻结门已改判 KEPT（保留并修判据）

- `coverage.retire_pending_gov_001` = 0；`mappings` 中 decision 以 `RETIRE-PENDING` 开头的条目 0 条。
- 结论：**本波没有「不删不改、待 W2 GOV-001 执行」的项**。原先按 `RETIRE-PENDING-GOV-001` 冻结的 6 项已改判 `KEPT` —— 保留为活动门、判据已迁移/修锚并补可执行负例，登记在 `docs/ci/01_CHECKS.md §2`。以下「现判据」与「目标位」逐字取自 JSON 的 `mappings`/`targets`，不凭印象写。

| 旧 ID（= 现顶层 ID） | 级 | 现判据实现者（checker） | 目标位（target） | 登记文档（targets.doc） | 改判记录（JSON note 逐字） |
|---|---|---|---|---|---|
| `AGENTS-GOV` | P0 | `eng/tools/check_agents_gov.py` | `AGENTS-GOV` | docs/ci/01_CHECKS.md §2（CI-003 登记） | CI-003（2026-09-16）：原 RETIRE-PENDING-GOV-001 判定撤销 —— 该门经判据迁移/修锚/加可执行负例后保留为活动门，已登记 docs/ci/01_CHECKS.md §2。 |
| `DOC-L0` | P1 | `eng/tools/check_l0_docs.py` | `DOC-L0` | docs/ci/01_CHECKS.md §2（CI-003 登记） | CI-003（2026-09-16）：原 RETIRE-PENDING-GOV-001 判定撤销 —— 该门经判据迁移/修锚/加可执行负例后保留为活动门，已登记 docs/ci/01_CHECKS.md §2。 |
| `ENG-CONSTRAINTS` | P0 | `eng/tools/doccheck/check_engineering_constraints.py` | `ENG-CONSTRAINTS` | docs/ci/01_CHECKS.md §2（CI-003 登记） | CI-003（2026-09-16）：原 RETIRE-PENDING-GOV-001 判定撤销 —— 该门经判据迁移/修锚/加可执行负例后保留为活动门，已登记 docs/ci/01_CHECKS.md §2。 |
| `GLOSSARY-DOCS` | P2 | `eng/tools/check_glossary.py` | `GLOSSARY-DOCS` | docs/ci/01_CHECKS.md §2（CI-003 登记） | CI-003（2026-09-16）：原 RETIRE-PENDING-GOV-001 判定撤销 —— 该门经判据迁移/修锚/加可执行负例后保留为活动门，已登记 docs/ci/01_CHECKS.md §2。 |
| `VERSION-CONSISTENCY` | P1 | `eng/ci/check_version.py` | `VERSION-CONSISTENCY` | docs/ci/01_CHECKS.md §2（CI-003 登记） | CI-003（2026-09-16）：原 RETIRE-PENDING-GOV-001 判定撤销 —— 该门经判据迁移/修锚/加可执行负例后保留为活动门，已登记 docs/ci/01_CHECKS.md §2。 |
| `VERSION-NAMESPACES` | P1 | `eng/tools/doccheck/check_version_namespaces.py` | `VERSION-NAMESPACES` | docs/ci/01_CHECKS.md §2（CI-003 登记） | CI-003（2026-09-16）：原 RETIRE-PENDING-GOV-001 判定撤销 —— 该门经判据迁移/修锚/加可执行负例后保留为活动门，已登记 docs/ci/01_CHECKS.md §2。 |

- 上述条目的 `reason` 字段保留改判**前**的迁移意图（逐字列出，仅供追溯，**不代表现行处置**）：
  - `AGENTS-GOV`：AGENTS.md 硬禁令门实现者；GOV-001 迁移入目标 CHK-AGENT-HARD-RULES
  - `DOC-L0`：L0 固定文档/链接完整性；悬空引用门归 CHK-DANGLING（GOV-001 迁移）
  - `ENG-CONSTRAINTS`：旧权威绑定（AstroCS_ENGINEERING_CONSTRAINTS.md 文本快照）清理归 GOV-001；语义继任者 = AGENTS.md 硬禁令门
  - `GLOSSARY-DOCS`：词典锚点/别名唯一性；悬空引用门归 CHK-DANGLING（GOV-001 迁移）
  - `VERSION-CONSISTENCY`：硬绑定 --expected 0.11.0-alpha.2 旧版本号；陈旧版本号门归 CHK-STALE-DOC（GOV-001 迁移）
  - `VERSION-NAMESPACES`：五版本命名空间与最高设计 §12「版本信息下线」冲突；陈旧版本号门归 CHK-STALE-DOC（GOV-001 迁移）
- 这些条目的 `note` 即改判记录，正文见 §5 表；判据实现者与归属见 §5.1 表。

## 7. 再生成与门禁

- 生成：`python3 eng/ci/gen_id_migration_map_doc.py`（读 `eng/ci/id_migration_map.json`，写 `eng/ci/ID_MIGRATION_MAP.md`）。
- 判绿：`python3 eng/ci/gen_id_migration_map_doc.py --check` —— 逐字节比对磁盘 md 与生成结果，不一致 exit 1；fail-closed：事实源缺失/不可解析/生成结果为空 exit 2。
- 自检：`python3 eng/ci/gen_id_migration_map_doc.py --self-test` —— 「生成→check 绿」「手改一行→check 红」「缺 md→exit 2」「两次生成字节相同」四向。
- 注册表登记：本门由 `CHK-REGISTRY-DOC-SYNC` 的 step `ID-MIGRATION-MAP-DOC-SYNC` 承载（profiles: fast, linux-main, windows-main）。
- 注册表登记：本门由 `CHK-REGISTRY-DOC-SYNC` 的 step `ID-MIGRATION-MAP-DOC-SYNC-SELFTEST` 承载（profiles: fast）。
- 确定性：本文件是 `eng/ci/id_migration_map.json` 的纯函数（无时间戳、无随机、无环境相关字段；checks.json 只以「写后快照是否仍一致」的谓词与门禁回读进入），同一输入两次运行字节相同。
- 文档索引：`docs/DOCUMENT_INDEX.yaml` 引用本文件，故本文件**不得删除**，只能由本生成器改写。
