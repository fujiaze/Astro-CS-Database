# 变更 claim：DOC-202-DETAIL-RED-001 — 详细文档层其余 🔴 订正（R07–R36 共 36 条 + S02/S03/S09/S10/S11/S12）

- 控制包：RELEASE-03 / 任务 **DOC-202**（`工程控制/RELEASE-03/tasks/DOC-202.md`）
- 日期：2026-09-20
- 依据（最高权威）：`ASTROCS_DESIGN.md` §0.1/§0.2、§1.2、§2.1–§2.4、§3.2–§3.6、§4.2/§4.4/§4.5、
  §5.1/§5.3、§6.2/§6.3、§7.1/§7.1a、§8、§9、§10.2、§11.4、§12
- 依据（裁决正本）：`工程控制/RELEASE-03/GAP_AUDIT.md` **§4.2 Q4 / §4.3 Q6 / §4.4 Q8 / §5 实验结论**；
  `run/RELEASE-02/design-merge/DESIGN-DRAFT.md` §3.2（R07–R36）/§3.3（S01–S12）
- 依据（工程流程）：`ENGINEERING_SPEC.md` §3；`CONTROL_PACK_SPEC.md` §5/§6；`AGENTS.md` §1.1/§4/§5/§8
- 状态：**已落地**（文档侧）
- 影响类：**术语/登记/架构描述级**（**不改**公式、常数、默认容差、SCI/ALG 冻结定义、数值结论）
- **零 git 写**：本任务未执行任何 git 写操作（`add/commit/push/checkout/stash/branch/reset` 全未使用）

## 1 逐条处置（R07–R36，全部有归宿，无「两边并存」）

| # | 文件:行（订正前） | 原状 | 改为 | 依据 |
|---|---|---|---|---|
| **R01** | `contracts/schemas/phase_config_{mosaic,export}.schema.json` | 三命令配置键名三套并存 | **归 FIX-207**；DOC-202 侧核查：`docs/architecture|interfaces|standards|modules|plugins|owner` 内**零** `phase_name` / `inputs[].product` 文档侧引用 ⇒ **文档侧无待改**（已登记） | 设计 §3.3；GAP_AUDIT U11 |
| **R02** | `docs/architecture/api_inventory.csv:56,57,58,69,71,73` | 6 个块↔文件 API 未标生产性 | `docs/architecture/IO_AND_ATOMICITY.md` 新增「**非生产 / 诊断接口登记**」表（6 符号 + 处置 + 代码侧归属声明锚 `aio_pipeline.h:205-247`）；`api_inventory.csv` **保持只登记函数签名**（其消费方 `check_doc_symbols.py` 只读 symbol 列，加列会破坏格式） | 设计 §9/§1.13；GAP_AUDIT V03 |
| **R03** | `docs/api/API-001.md:11` | 第二套命令树（含被禁的 `run`） | **只登记**：`docs/api/**` 不在 DOC-202 文件域 ⇒ **交 DOC-203**；本 claim + 回执为登记面（作废该命令面表） | 设计 §6.2；GAP_AUDIT V12 |
| **R04** | `docs/contracts/ARCH-001.md:5` | 「本文件是 V6 架构的**唯一权威**」 | **删除该自我授权句**，改为引用式（架构权威 = 设计 §7；不另立权威链） | 设计 §0.1/§0.2；DESIGN-DRAFT §1.1-A |
| **R05** | `docs/contracts/INDEX.yaml` 等 | 版本号 + 带版本号退役窗口 | **归 DOC-203**；DOC-202 侧核查：`docs/architecture|interfaces|standards|modules|plugins|owner` 内**零**「`0.11.0-alpha.2→0.12.0` / 退役窗口」⇒ **文档侧无待改**（已登记） | 设计 §12 |
| **R06** | `docs/contracts/DATA_SEMANTICS.md:296` | drizzle 文件通道有 weight 面 | **交 DOC-203**（只登记） | 设计 §2.1/§9.73 A44 |
| **R07** | `docs/architecture/ERROR_MODEL.md:22-30` | 第二套 `AstroCsExitCode` 表（1/3/4/9/10 含义与设计不同） | **作废该表**（原文留痕 + ⛔ 标注）；改为**唯一源 `lib/infrastructure/cli/exit_codes.h`**（11 码逐名逐值列出）；并作废「与 orchestrator.h 一致」的 V19R3 依据 | 设计 §6.3/§7.1；GAP_AUDIT V15 |
| **R08** | `docs/architecture/observability/STRUCTURED_LOGGING_CONTRACT.md:38-59` | 事件 JSONL 字段名/枚举与设计不同 | **按 Q6 裁决**：新增 §1.1「**身份声明：本合同不是运行事件流**」两流对照表 —— 运行事件流唯一源 = `lib/infrastructure/cli/protocol.h`（`ValidateEventV1`）+ `jsonl.h`（`JsonlEmitter`），键名 `kind`；LOG-001 = 结构化日志，键名 `event`；**两键不得混用、两工件不互替** | GAP_AUDIT §4.3 Q6；GAP_AUDIT V16 |
| **R09** | `docs/architecture/observability/RESOURCE_MONITORING_CONTRACT.md:44-48` | 同一工件名两套列合同 | **按 Q4 裁决**：新增 §3.0「**工件名与不互替声明**」——`resource_timeseries.csv` 的**唯一列合同 = 生产实现 `lib/infrastructure/cli/resource_recorder.h:260-266`（20 列）**，本文件不复写其列名；LOG-002 的 CSV 工件名**固定 `monitor_timeseries.csv`**（21 列 + seed 行 + 指纹链）；**两工件不得互替** | GAP_AUDIT §4.2 Q4；GAP_AUDIT V17 |
| **R10** | `docs/architecture/execution_options_contract.md:13-20,49-52` | worker 默认 = `hardware_concurrency` / `cpu/2` | **删默认值**：`cpu_workers`/`io_workers` 标「**无默认**（由 benchmark profile / 全局预算对象注入）」；测试节删「默认 = hardware_concurrency」断言 | 设计 §8；GAP_AUDIT U14 |
| **R11** | 同文件 `:15,24-45` | `gpu_route` 键 + 第二 CLI 入口 `astrocs-stage2` | **删键删入口**：配置块与 CLI 覆盖删 `gpu_route`/`--gpu-route`；入口改为唯一 CLI `astrocs` 的 `mosaic` 子命令；被删面**不留第二实现**（原文留痕） | 设计 §6.2/§8；GAP_AUDIT U15 |
| **R12** | `EXECUTION_MODEL.md:14,24,46-53,79`、`THREADING_MODEL.md:8-9`、`CACHE_POLICY.md:10` | 休眠 ACR/GPU/浏览器写成生产执行层 | 逐行标 **`DORMANT`**（ACR/CUDA/GPU buffer/H2D-D2H/ARC-EXEC-005）、**「工具分类（非发布）」**（Browser）、**「历史保留 / 非入口」**（orchestrator/stage2）；文件头加订正横幅；生产 fallback 收窄为「无 cpu_profile → baseline + 动态 worker」 | 设计 §1.3/§7.1/§8/§10.1 |
| **R13** | `docs/architecture/IO_AND_ATOMICITY.md:5` | HiPS tile 非原子 | 改为 **「⚠ 待修缺口登记（未闭合）」**：现状 `remove→fits_create→write_chksum→close`（`aio_hips_writer.cpp:330/:399`），**闭合归属 = FIX-206**，闭合判据与「未闭合期间禁止声称已原子发布」写明；另登记「阶段二直写无暂存区」 | 设计 §9；GAP_AUDIT V05 |
| **R14** | `docs/architecture/ARCHITECTURE.md:33,39,55` | 「`run/` 是唯一运行输出目录」 | 三处均改为「**产品只落块级 `output_dir`**；`run/` **只放临时产物与日志**」（含 §7 不变量第 2 条） | 设计 §9；GAP_AUDIT U16 |
| **R15** | `PIPELINE.md:11,22`、`DATA_FLOW.md:16`、`production_call_paths_stage{1,2}.csv:2` | 旧 exe 被写成唯一入口 | 入口全部改为唯一 CLI 子命令（`normalize` / `mosaic`）；两个 CSV 的入口行改写为 `normalize --json <config.json>` / `mosaic --json <config.json>`，旧 exe 降为**历史登记**；`stage2.csv` 的 `acr_routing` 行标 `DORMANT` | 设计 §6.2/§7.1/§11 |
| **R16** | `DATA_FLOW.md:7-14`、`PIPELINE.md:5-9` | 阶段一单轮 WCS、无「测光归一化落到像素」 | **按设计 §3.2 重写节点顺序**：两轮 WCS（盲解粗解 → 星表引导检测 → 精解）+ **`apply photometry`（`I_photo = k_photo·m(x,y)·I_cal` 落到像素）**；写明「两轮 WCS 是强制节点」「apply photometry 未启用须 fail-closed + `degraded_reason`」 | 设计 §3.2/§7.1 |
| **R17** | `DATA_FLOW.md:28`、`PIPELINE.md:18` | 排异「7 种任选」 | 改为引用**冻结映射表**；并在 `docs/plugins/algorithms_phase2/12_rejection.md` **新增 §9 冻结映射表**（见 §2 下方「EXP-204 覆盖」） | 设计 §4.5；前台 EXP-204 定案 |
| **R18** | `DEPENDENCY_RULES.md:8`、`BUILD_GRAPH.md:9` | 生产链 ACR | 改为「**生产构建不得链入 ACR/CUDA**」；BUILD_GRAPH 的 ACR 源集/编译定义/链接行/`astrocs-stage2` 目标逐行标注**非生产**，代码侧整改登记 | 设计 §8；GAP_AUDIT V03-adjacent |
| **R19** | 块词表三源不一致 | `aio_pipeline.h` 12 块 ↔ `orchestrator.cpp` 6 块 ↔ DATA_SEMANTICS §11.1 | **按 Q8 裁决**：`docs/modules/astro_image_io.md` 声明「**块词表的唯一登记处 = `aio_pipeline.h:287-305` 标准块定义表**」，orchestrator 的 6 名 = `stage_trace` **跟踪子集、不是词表**，DATA_SEMANTICS 表**只作引用**；Q8 待落地代码侧三项（补 `variance` 块 / 删「自定义块名允许」/ 补机器判据）登记 | GAP_AUDIT §4.4 Q8 |
| **R20** | `docs/architecture/MODULE_MAP.md:50` | 投影 4 种 | 改为「**设计冻结 8 种**（TAN/SIN/CAR/AIT/STG/MOL/CEA/ZEA）；**已实现以实际注册表为准**；**未实现必须显式报不支持**」 | 设计 §5.3；GAP_AUDIT V04/U08 |
| **R21** | `DATA_FLOW.md` / `PIPELINE.md`（阶段内传文件） | 阶段内节点间用 JSON/bin 文件传递 | 改为「**阶段内内存块管线，不落中间文件**（设计 §7.1a）」；并如实登记「当前生产实现仍用磁盘 JSON/FITS，属**已登记缺口**，迁移归 FIX」 | 设计 §7.1a |
| **R22** | `PIPELINE.md:26` | 指向不存在的 `SCIENCE_FREEZE.md` | 改为 **`docs/science/`（公式）+ `docs/algorithms/`（推导）**，原引用标作废 | 设计 §0.1/§1.1 |
| **R23** | `MODULE_MAP.md:6`、`docs/owner/RELEASE_STATUS.md:15` | 状态阶梯双声明 + 引错节号（§11.3） | 两处均改为**引 `ASTROCS_DESIGN.md` §11.4**；MODULE_MAP **删去状态词清单复述**并声明「本表不写状态字段，状态由 `check_module_map.py` 现场计算」；RELEASE_STATUS §0 加订正说明（§11.3 = 发布前四层验收，状态阶梯 = §11.4） | 设计 §0.2/§11.4；GAP_AUDIT U21 |
| **R24** | `phase2_upm.md:65-66,109-110`、`registry/astrocs.phase2.upm-apply.md:46-47,69-70` | UPM apply 写成全量 `raw − C_f(p)`（无消歧） | 改为「**默认只扣偏差 δ_k、保留公共天光面 `B_ref`**：`calibrated_k(x) = raw_k(x) − δ_k(x)`」；写明 `C_k ≡ B_ref + δ_k` 是表示层全量，**全量扣除不再是默认**，凡写 `raw − C_k` 必须写明「全量/仅偏差」；登记实现面默认仍为 `c`（待改，以设计为准） | 设计 §4.4/§9.67 定案 1；GAP_AUDIT V09 |
| **R26** | `phase3_proj.md:40-42,71`、`registry/astrocs.phase3.wcs.md:52-53,135-136`（+ `docs/owner/SCIENCE_OVERVIEW.md` 状态行） | 「只有 TAN」/「UNSUPPORTED 无产生点」 | 改为「**8 冻结 / 仅 TAN 已实现**；**未实现必须显式报不支持**」；**复核证据**：`p3_projection_registry.h` 声明集 D = 实现集 I = `{TAN}`，`p3_proj_declare` → `P3_WCS_UNSUPPORTED`（含请求码+原因+已支持清单），`p3_wcs.cpp:98-99` 经 `p3_proj_is_implemented` 产生该状态；`p3_projection.cpp` v1 四行 registry 已 `RETIRED` | 设计 §5.3；GAP_AUDIT V04 |
| **R27** | `docs/modules/MODULE_MAP.yaml:578-606`、`docs/plugins/00_INDEX.md` §2 | 模块名 `runtime`（`scheduler`/`pipeline` 零条目） | **改名对齐设计 §7.1**：`id: runtime→scheduler`、`module_id: astrocs.infra.runtime→astrocs.infra.scheduler`、`target_dir: lib/infrastructure/runtime→lib/infrastructure/scheduler`、`target: astrocs_infra_runtime→astrocs_core`（实测 target，根 `CMakeLists.txt:292`）、`target_file→CMakeLists.txt`；`00_INDEX.md` 第 2 列 `runtime→scheduler`（含「模块名 = scheduler + pipeline」说明）；`19_runtime.md` 标题/§1/§7/新增 §9 全部对齐，**保留「已改名」留痕**；**禁第二名字 `runtime`**（仅历史文件名保留） | 设计 §7.1/§7.1a；GAP_AUDIT V07/D05 |
| **R28** | `docs/modules/gaia_xpsd_client.md:9,60` | 「离线、零网络」 | **保留不改**（它是对的，与设计 §2.4 逐条一致）；订正在设计侧（C16）。加「处置留痕」小节（**追加在文件末尾**以保 `unified_object_registry.json` 的 `:23` 锚点） | 设计 §2.4；GAP_AUDIT V08 |
| **R29** | `astro_image_io.md:76`、`registry/astrocs.phase1.hips-writer.md:80-81`、`registry/astrocs.phase2.write.md:51,132` | HiPS tile 非原子 / 阶段二直写 | 三处均登记为**待修缺口（未闭合）**，闭合归属 **FIX-206**，写明闭合判据与「未闭合期间禁止声称已原子发布」 | 设计 §9；GAP_AUDIT V05 |
| **R30** | `docs/interfaces/data/DATA-002_PHASE_PRODUCT_EXCHANGE.md:91-99` | units 载体归属 | **保留不改**（它是对的，与设计 §9「清单 schema 无 units 字段、由产品内容证据块声明」逐条一致）；订正在设计侧（C13）。加处置留痕 | 设计 §9；DESIGN-DRAFT C13 |
| **R31** | `DATA-002:96-97` | 稀疏 SNR 层在 plane_id 枚举里无位置 | plane_id 集合**新增 `sparse_snr`**（6 值）；写明语义 = 无量纲相对场 `rho_c = SNR_c/SNR_frame`（`p50=1`）、`units=dimensionless`、**禁止当权重**、**可选**（不属最小平面集）；schema 枚举同步**交 DOC-203** | 设计 §1.4/§3.4；GAP_AUDIT G09 |
| **R32** | `DATA-002:188-189` ↔ `:39,106-107` ↔ `:133` | 阶段三接受域自相矛盾 | **删「Phase3 不支持 variance/ivar 输入，显式拒绝」**；改为「**接受任一合同兼容 HiPS**（含来自 `normalize` 的单帧产品）；**variance/ivar 显式消费并传播**（输出 `VARIANCE`/`IVAR` 扩展 HDU），皆无时显式 `unavailable`；**禁止**静默丢弃」；保留「weight/support 冒充方差仍拒绝」「flux-per-pixel 仍拒绝」 | 设计 §5.1/§5.3；GAP_AUDIT U09 |
| **R33** | `NUMERIC_STANDARD.md:19-22` | 权重来源「ivar 优先 / 不确定度回退」 | 改为「**权重 = 阶段二现场派生量，由 SNR 计算**（`w = 1/σ² = SNR²/F_ref²`）；阶段一/三不产生也不消费权重」；**删除**「回退」表述（= 权重档位语义，已按 A44 作废）；`ivar`/`uncertainty` 是**数据对象**不是来源档位 | 设计 §2.1/§4.3/§4.4；§9.73 A44 |
| **R34** | `NUMERIC_STANDARD.md:13` ↔ `STANDARDS_REGISTRY.md:173,271` | NaN 契约相反 | **按 EXP-202 定案「掩膜」**：撤销 `DISP-DRZ-004` 的 **CLOSED → TRACKED/OPEN**（4 处：DEVIATION 字段 / 清单行 / 偏差表 / §3 索引），写明**被三面实测推翻**；NaN 口径统一为 rule_id `NAN-SAMPLE-MASK-COVERAGE-NAN`。**独立变更 claim**：`DOC-202-EXP202-NAN-MASK-001.md` | GAP_AUDIT §5.1 EXP-202；GAP_AUDIT V11 |
| **R35** | `CODE_STANDARD.md:7` | 正式工具链 = MSYS2 MinGW64 g++ 16.1.0 | 改为「**Windows = MSVC v143；Linux = GCC 或 Clang**」；MinGW64 降为「本地开发/兼容性验证」工具链，**不得**作发布构建依据 | 设计 §10.2；GAP_AUDIT V06 |
| **R36** | `STANDARDS_REGISTRY.md:48` ↔ `:161,188` | 权重式相反（`a/A_drop` ↔ `a/A_pixel`） | 统一为**面亮度路径 `w_jp = a_jp / A_pixel`**（§2 域表 CLAUSES 与 D.drizzle CLAUSES 同步；`:188` 的 `DISP-DRZ-009` 保持登记 legacy `a/A_drop` 为**偏差**） | 设计 §3.6（`sb_weight = a / A_pixel`）；§9.54 裁决 3 |

## 2 同批同步订正（S02/S03/S09/S10/S11/S12）

| # | 处置 | 依据 |
|---|---|---|
| **S02** | 两份 schema 的 `x-astrocs-authority` 引交互式预检窗 ⇒ **归 FIX-207**；DOC-202 侧核查：我域内对设计 §3.5 的引用**无**需改处（已登记） | DESIGN-DRAFT §3.3 S02 |
| **S03** | 排异档位描述 ⇒ **随 R17 一并订正**（见 §3「EXP-204 覆盖」） | DESIGN-DRAFT §3.3 S03 |
| **S08** | 三处 `lib/**/PENDING.md`「本目录为空」⇒ **属 DOC-204**，DOC-202 **未动** | DESIGN-DRAFT §3.3 S08 |
| **S09** | `MODULE_MAP.yaml` 声明路径缺失：R27 后由 **35 → 32 条**；在文件**末尾**新增机器可读 `declared_absent_paths:`（`checked_at` / `count` / 逐条 `module+key+path` + `note`）**如实登记为缺口**；R27 另修掉 `runtime` 的 3 条（target_dir/readme/target_file） | DESIGN-DRAFT §3.3 S09；GAP_AUDIT D05 |
| **S10** | ① 模板样板句误植「错误码=ACS_ERR_*(API 合同); 取消=host cancel 回调; 无 checkpoint(Phase3 原子写)。」**9 页 + 1 页变体**统一替换为准确表述（退出码唯一源 `exit_codes.h` + 协作取消契约 + 无 checkpoint），**逐行同长度**（保 `unified_object_registry.json` 的 `registry/*.md:22/:23/:33/:35/:38/:39` 锚点）；② **补规范 `module_id` 6 页**（photometry/coverage/integrate/reject/upm-apply/phase3.wcs）⇒ `PAGE_MISSING_MODULE_ID` 实测 **20→14**、`MODULE_WITHOUT_PAGE` **17→11**（`check_module_id_normalization.py` 仍 PASS，未下调 classes 阈值）；剩余 14 页的原因（5 页被行锚锁定 / 9 页无 MODULE_MAP 条目）写入 `module_id_migration_baseline.json` 的 history | DESIGN-DRAFT §3.3 S10 |
| **S11** | `docs/architecture/ARCHITECTURE.md:50` 称 `PRODUCTION_EXECUTION_INVENTORY.csv`「217 行」⇒ 实测 **338 行**，已订正并标口径 | DESIGN-DRAFT §3.3 S11 |
| **S12-Y1** | `CODE_STANDARD.md:3` 引 `V19R2 MASTER_CONTROL_SPEC`（全仓零命中）⇒ 改为可解析来源（`ENGINEERING_SPEC.md` §1/§4 + 设计 §10.2），并标「原来源已不可考」 | IFC-01 Y1 |
| **S12-Y2** | `RELEASE_STANDARD.md:3` 要求 `CHANGELOG.md`（不存在）⇒ **撤下该要求**（版本唯一源 = 根 `VERSION` + `docs/owner/RELEASE_STATUS.md`）；**未新建根目录条目**（AGENTS §6：新根条目须先登记并经负责人确认） | IFC-01 Y2 |
| **S12-Y3** | `DOCUMENTATION_STANDARD.md:13` 要求 `docs/history/`（不存在）⇒ 订正为 **`docs/archive/history/`** | IFC-01 Y3 |
| **S12-Y4** | `STANDARDS_REGISTRY.md:341-368`「9 场景负向注入 + 空转守卫」⇒ **如实降级为「计划（PLANNED）」**：实测 `ci/checks.json` 只登记 **2/9** 场景（`STD-REG-FI-DANGLING`、`STD-REG-FI-VERSION-DRIFT`）+ `STD-REG` 主判据；其余 7 场景与空转守卫**未登记**，补登记由前台 **BLD-201** 做（`ci/checks.json` 不在 DOC-202 文件域） | IFC-01 Y4 |

## 3 EXP-204 覆盖（R17 的最终形态，覆盖 DOC-202 任务书原 R17 指示）

`docs/plugins/algorithms_phase2/12_rejection.md` **新增 §9 冻结映射表**（追加在文件末尾，
保 `config/config_registry.json` 的 `:31`/`:34` 行锚）：

| N（几何可贡献帧数） | 方法 |
|---|---|
| `1 ≤ N ≤ 3` | **none（不排异）** |
| `4 ≤ N ≤ 5` | percentile clipping |
| `6 ≤ N ≤ 15`（或 BIAS/DARK） | winsorized sigma clipping |
| `N ≥ 16` | linear fit clipping |

- 依据：**EXP-204 定案**（三数据面 + 6 轮复核；判据冻结 sha256 `bd8982d6…`）
  + 负责人 2026-09-19 原裁决（`n≤3` 不排异）+ WBPP 一手实测（`BPP-FrameGroup.js:1304-1312`，档界 6/16）；
- **与 WBPP 的偏离写明理由**：低/中电平（≲2700 e⁻/pix）下强制 percentile 有损
  （N=3 `ρ−1`=0.3–27% ≫ `τ_ρ`=0.31%；N=2 有 83.5% 像素无输出）⇒ 小 N 段不排异更接近真值；
- **禁止 min/max**（WBPP 明文拒绝 `BPP-FrameGroup.js:1239-1240`；算法清单不含 min/max
  `BPP-engine.js:2695-2719`）；合法性窗口**只告警、不硬阻断**；
- `none` 是**显式档位**（须写 provenance），不是静默跳过；
- `docs/architecture/{PIPELINE.md,DATA_FLOW.md}` 已改为**引用该表**（不再是「7 种任选」）；
- `docs/science/REJECTION.md` **不在 DOC-202 文件域** ⇒ 其订正归 **DOC-205**，本文只作引用。

## 4 验收证据（命令 + rc）

见回执 §4（逐条命令与输出已落 `run/RELEASE-03/logs/DOC-202-*.log`）。

## 5 残留与交接

| 项 | 归属 |
|---|---|
| `docs/contracts/**`（R03/R05/R06 + R31/R32 的 schema 同步 + `unified_object_registry.json` 行锚随 `registry/*.md` 加 `module_id` 的 5 页锚点） | **DOC-203** |
| `docs/api/API-001.md:11` 第二套命令树作废 | **DOC-203** |
| `docs/science/**`、`docs/algorithms/**`（`PHASE2_SAMPLER.md:301` 自称「并行语义唯一权威」、`DRIZZLE_GEOMETRY.md` 旧 NaN 行为、`PHASE3_PROJ_IMPL.md`） | **DOC-205** |
| `lib/**` 代码侧（`execution_options.h` 的 `gpu_route`/`hardware_concurrency`；ACR 从生产源集移除；`drizzle_engine.cpp` 掩膜；`p3_v6_export.cpp` 接线；HiPS tile 原子发布） | **FIX-201/206/208 / P1-DRZ-IMPL / Phase3 export 域主** |
| `ci/checks.json` 补登记 7 个注入场景 + 空转守卫 | **BLD-201** |
| `MODULE_MAP.yaml` 32 条声明路径缺失的逐条创建/修正；`pipeline` 单列条目（需改 `index_module_count`） | **MOD-002 / 各模块落地任务** |
| `config/config_registry.json` 的 `runtime` 字面量锚点复核（R27 改名后） | **前台 / BLD-201** |
