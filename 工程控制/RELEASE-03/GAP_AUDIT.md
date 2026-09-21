# 工程控制 / RELEASE-03 差异审计表（GAP_AUDIT）

> **规范依据**：`CONTROL_PACK_SPEC.md` §3.2（制作流程：派大量 SubAgent 并行扫描 → 前台汇总去重 → gap 清单）、
> §3.3（制作纪律：**逐条给出文档条款引用，不得凭印象**；现状描述可复核；**制作过程不改代码**）。
>
> **差距类型枚举**（§3.2）：`缺口（未实现）` / `违规（与文档冲突）` / `过时（文档已撤销仍存在）` /
> `漂移（实现偏离文档细节）` / `无主（有代码无合同）` / `UNRESOLVED`（无法判定，不强行归类）。

## 1. 扫描留痕（§3.2 步骤 ②）

| 分片 | 范围 | 规模 | 要点 | 🔴 | 🟠 | 🟡 | 报告 |
|---|---|---|---|---|---|---|---|
| **ARCH-01** | `docs/architecture/**` | 33 文件 / 2,693 行 | 47 | 17 | 22 | 3 | `run/RELEASE-02/design-merge/ARCH-01.md` |
| **MOD-01** | `docs/modules/**` | 51 文件 / 5,565 行 | 25 | 6 | 14 | 4 | `run/RELEASE-02/design-merge/MOD-01.md` |
| **IFC-01** | `docs/interfaces/**` + `docs/standards/**` | 20 文件 / 1,789 行 | 57 | 7 | 32 | 4 | `run/RELEASE-02/design-merge/IFC-01.md` |
| **CTR-01** | `docs/contracts/**` | 29 文件 / 11,811 行 | 55 | 7 | 30 | 3 | `run/RELEASE-02/design-merge/CTR-01.md` |
| **合计** | | **133 文件 / 21,858 行** | **184** | **37** | **98** | **14** | + 综合草案 `DESIGN-DRAFT.md`（516 行） |

**最高设计侧已闭合**：`ASTROCS_DESIGN.md` 995 → 1,159 行（提交 `26cb79d9`，26 条条文订正 + 18 组新要点条款）。
**本表登记的是详细层与代码侧尚未闭合的差距。**

## 2. 差距清单（按类型）

### 2.1 违规（与文档冲突）—— 与最高设计相反，**必须**改

| # | 条款引用 | 现状（可复核） | 处置任务 |
|---|---|---|---|
| V01 | 最高设计 §2.1（不存在「权重模式」）+ §9.73 A44 | 详细层约 **22 处** `weight_mode` 家族：`docs/contracts/DATA_SEMANTICS.md:1067,1068,1080,1130-1146,1315,1780,1890,1987,2408,2424,2522,2699-2767`、`PUBLIC_API.md:1177,1303,1400,1432,2124-2161`、`CONFIG_CONTRACT.md:34,48,65,80,164`、`UNIFIED_OBJECTS.md:123,124`、`config_separation_anchors.json:31,125,179,200` 等 | **DOC-201** |
| V02 | 同上 | **HiPS 格式内部**也有权重枚举：`lib/infrastructure/aio/include/aio_ahpx_format.h:18,42`、`src/ahpx/aio_ahpx_writer.h:28,70`、`reader.h:55`、`reader.cpp:587` | **FIX-202** |
| V03 | 最高设计 §9（aio 文件级唯一 I/O 边界）+ §9.73 U5 | 穷举 I/O 点之外的读写 API：`aio_frame_export_block_fits` / `_block_xml` / `_all_xml` / `aio_frame_save_cache` / `load_cache` / `aio_pipeline_export_xml`（`API_CONTRACTS.csv:32,33,34,45,47,49`；符号实测存在 `aio_pipeline.h:200,204,213,218,223,227`）；`lib/algorithms/drizzle/healpix_drizzle/src/aio_publish.cpp`、`fits_output` 的 `fopen` | **FIX-201** |
| V04 | 最高设计 §5.3（投影未实现必须显式报不支持） | `docs/algorithms/phase3_proj.md:40-42,71` + `docs/modules/registry/astrocs.phase3.wcs.md:135-136` 说只有 TAN；代码 `p3_projection.cpp:1` 实现 4 种 | **FIX-205** |
| V05 | 最高设计 §9（所有产品含 HiPS tile 原子发布） | HiPS tile 非原子（`registry/astrocs.phase1.hips-writer.md:80-81`）；阶段二直写无暂存区（`phase2.write.md:51,132`） | **FIX-206** |
| V06 | 最高设计 §10.2（官方 Windows 工具链 = MSVC） | `docs/standards/CODE_STANDARD.md:7`「正式 toolchain = MSYS2 MinGW64 g++ 16.1.0」 | **DOC-202** |
| V07 | 最高设计 §7.1（模块名只有一份） | `MODULE_MAP.yaml` / `docs/plugins/**` 用 `runtime`；`scheduler`/`pipeline` 在 `MODULE_MAP` **零条目** | **DOC-202** |
| V08 | 最高设计 §2.4（星表查询零网络） | `gaia_xpsd_client.md:9,60` 说离线零网络 vs 最高设计旧文（已订正）；详细层两侧不一致 | **DOC-202** |
| V09 | 最高设计 §4.4（UPM apply 只扣 δ_k） | `phase2_upm.md:65-66,109-110` 写全量 `raw − C_f(p)` | **DOC-202** |
| V10 | 最高设计 §2.1 + 4.4（天光采样点权重口径） | `10_sampling.md:38,45` `w∝SNR²` vs `phase2_samp.md:56-59`「科学权重一律 control_ivar」 | **EXP-201** → 后续订正 |
| V11 | 最高设计 §9（NaN 处置规则必须唯一） | `NUMERIC_STANDARD.md:13`「禁止 NaN 传播为合法产品」vs `STANDARDS_REGISTRY.md:173,271` DISP-DRZ-004「NaN 经 F_p 传播、不掩膜」 | **EXP-202** → 后续订正 |
| V12 | 最高设计 §6.2（唯一命令树） | `docs/api/API-001.md:11` 登记**第二套命令树**，含被禁的 `run`（三阶段串接入口） | **DOC-202** |
| V13 | 最高设计 §0.1（只有一份权威链） | `docs/architecture/ARCH-001.md:5` 自称「V6 架构的**唯一权威**」（与自身 `:31`/`:675` 冲突） | **DOC-204** |
| V14 | 最高设计 §12（禁止用版本号作为生效或退役条件） | `docs/contracts/INDEX.yaml:4` 全表 `version 1.0.0` + 退役窗口 `0.11.0-alpha.2→0.12.0` | **DOC-203** |
| V15 | 最高设计 §6.3（退出码唯一源） | `ERROR_MODEL.md:24-30` 与最高设计旧文均指向**不存在**的 `include/astrocs/exit_codes.h`；实际源 `lib/infrastructure/cli/exit_codes.h:6-18` | **DOC-202**（最高设计侧已订正） |
| V16 | 最高设计 §6.3（事件流唯一 schema） | `STRUCTURED_LOGGING_CONTRACT.md:38-59` 与最高设计 `:621` 字段名/枚举不兼容 | **DOC-203**（Q6 前置） |
| V17 | 最高设计 §9（磁盘固定产物逐个登记） | `resource_timeseries.csv` **两套列合同**：`LOG-002:44-48` vs `lib/infrastructure/cli/resource_recorder.h:260-266`（20 列） | **DOC-203**（Q4 前置） |
| V18 | 最高设计 §9（产品只落块级 `output_dir`） | `ARCHITECTURE.md:33,39,55` 把产物位置写成 `run/` | **DOC-202** |
| V19 | 最高设计 §8（生产构建不得链入 ACR/CUDA） | `DEPENDENCY_RULES.md:8` + `BUILD_GRAPH.md:9` 称 phase2 生产链 ACR；代码 `lib/algorithms/coverage/CMakeLists.txt:51-57,62-65` | **DOC-202** + 代码侧 |
| V20 | 最高设计 §8（worker 数不得由硬件并发默认值决定） | 详细层登记 worker 默认 `hardware_concurrency` | **DOC-202** + 代码侧 |
| V21 | 最高设计 §3.6（单一生产实现 + 一个 Oracle） | 噪声模型两套实现并存（`SCI-NOISE-001` §5/§5a） | **EXP-206** → 后续收敛 |
| V22 | 最高设计 §11（登记表/映射表禁止写状态字段） | `MODULE_MAP.yaml`、`DOCUMENT_INDEX.yaml`、`STANDARDS_REGISTRY.md` 等含状态字段 | **DOC-204** |

### 2.2 缺口（未实现）

| # | 条款引用 | 现状 | 处置任务 |
|---|---|---|---|
| G01 | 最高设计 §7.1（块词表唯一权威） | aio「标准块定义表」12 块（`aio_pipeline.h:266-284`，含「未列出的自定义块名也允许」）vs orchestrator 6 块（`:3212-3213`），**交集 4**；`variance` **不在** 12 块表内 | **DOC-203**（Q8 前置） |
| G02 | 最高设计 §9（aio 边界机器判据） | **无**机器判据脚本（「除 aio 外文件写操作为 0」） | **FIX-201** |
| G03 | 最高设计 §6.3（事件流默认输出） | 事件流需旗标开启；schema 两套 | **FIX-208** |
| G04 | 最高设计 §3.5（资源门只管磁盘） | 存在一般性资源超限门（内存/CPU） | **FIX-208** |
| G05 | 最高设计 §3.3（三命令同构块结构） | 两份 schema/模板仍为旧 `{phase_name, config, inputs[]}`；`snr_path`/`precision` **不在 CLI 白名单**（`parser.cpp` 0 命中） | **FIX-207** + **FIX-203** |
| G06 | 最高设计 §7.1（加载前六查 + 信任边界） | `ABI_003_SECURE_LOADER.md:29-48` 规定但未登记进 `ci/checks.json` | **DOC-204** / **BLD-201** |
| G07 | 最高设计 §8（线程预算单一来源 + 机器可检） | `THREAD_BUDGET_ARCH.md:8-10,22,38-41` 规定 `tools/arch/check_thread_budget.py` + `THREAD_BUDGET_EXEMPT`，**检查器不存在** | **DOC-204** / **BLD-201** |
| G08 | 最高设计 §6.3（计划图 ≠ 运行图） | `RUN_GRAPH_CONTRACT.md:30-40,113-126` 规定但未接线 | **DOC-202** |
| G09 | 最高设计 §3.4（稀疏 SNR 层必须在交换合同里有位置） | 交换合同 `plane_id` 枚举 = `{signal,support,variance,ivar,mask}`（schema:162），**没有稀疏 SNR 层** | **DOC-203** |
| G10 | 最高设计 §3.4（每阶段最小科学平面集） | `DATA-002:39` 要求 phase1 含 `variance` vs `:188-189` Phase3 拒 `variance` 输入——**自相矛盾** | **DOC-202**（Phase3 接受域） |
| G11 | 最高设计 §9（跨阶段交换对象定义） | 缺「自包含说明」的完整定义（角色↔登记类型、来源、完整清单、科学内容证据） | **DOC-203** |
| G12 | 最高设计 §11.1.1（科学模块实验单元要求） | 部分科学模块缺实验单元 | **EXP-201..206** |

### 2.3 过时（文档已撤销仍存在）

| # | 条款引用 | 现状 | 处置任务 |
|---|---|---|---|
| O01 | §9.73 A44 | `docs/contracts/v6/frozen/02_WEIGHT_MODE_VOCABULARY.md` 六模式表；`v6/frozen/01:33`、`00_README.md:22` 的 `FZ-FIELD-WEIGHTMODE` | **DOC-201**（随 v6 去留，Q2） |
| O02 | 最高设计 §7.1（代码 v6 退役） | `contracts/schemas/v6/**` 10 份生产 schema + `docs/contracts/v6/**` 16 份；详细层自相矛盾（`DATA_SEMANTICS:2658-2666` 标 ACTIVE vs `UNIFIED_OBJECTS:68-89` 标退役窗口 0.12.0） | **DOC-203**（Q2 前置） |
| O03 | §9.71 裁决 3 | 原排异四档表（`n≤3` / 4–7 / 8–15 / ≥16）**已作废**但仍存在 | **FIX-204**（最高设计侧已订正） |
| O04 | 最高设计 §6.2 | `API-001.md:11` 第二套命令树含 `run` | **DOC-202**（同 V12） |

### 2.4 漂移（实现偏离文档细节）

| # | 条款引用 | 现状 | 处置任务 |
|---|---|---|---|
| D01 | `config/defaults.json` `source_ref` 锚点 | **已修**（38 处行漂移 + 6 处 defaults 锚点 + 3 未登记键 + totals；CFG002 11/11 PASS、`tests/config` 58 passed） | ✅ 已闭合（`d519a67a`） |
| D02 | 最高设计 §4.5（排异） | 实现为全局单算法 + 四档表，非逐像素按 N | **FIX-204** |
| D03 | 最高设计 §3.4（`variance` 帧内块） | orchestrator 噪声节点只写 `p1_snr.json` 标量，**从不构建帧、从不调 `_fill`**；`PipelineFrame` 仅在 drizzle 节点内使用 | **FIX-201** 关联 |
| D04 | `API-001.md:60` | 称 `API_CONTRACTS.csv`「423 行」，实测 **382 行**（381 数据行） | **DOC-204** |
| D05 | `MODULE_MAP.yaml` | **34** 条声明路径缺失（非 12）；`scheduler`/`pipeline` 无条目 | **DOC-202** |
| D06 | `benchmark/PENDING.md` 等三处 | 称「本目录为空」而实际有 **20 / 64 / 38** 文件 | **DOC-204** |
| D07 | 最高设计 §11（门禁字面量如实） | `reports/v19r2/**` 等处的「设计冻结 VERIFIED」与实际可执行测试状态不符 | **DOC-204** |

### 2.5 无主（有代码无合同）

| # | 条款引用 | 现状 | 处置任务 |
|---|---|---|---|
| N01 | 最高设计 §3.3（配置键须有登记） | `additive_mode` / `sky_plane.enabled` / `smoothing_lambda` 生产代码在读（`module_adapters.cpp`），但不在 `defaults.json` / CLI / 任何 schema —— **已补登记为 `unregistered`** | ✅ 已登记（`d519a67a`） |
| N02 | 最高设计 §3.3 | `algorithm_upm_gauge` 在 schema 声明但生产**零读取**（死键） | **FIX-203**（登记台账） |
| N03 | 最高设计 §3.3 | `wcs.center_deg` / `wcs.s_out_deg` 死键（CLI 实读 `center.ra_deg` / `scale_deg_per_px`） | **FIX-203** |
| N04 | 最高设计 §7.1 | 两套并行原子发布实现：`DATA-003` 的 `ArtifactStore` vs `IO_003` 的 `hips_output_store.py` | **FIX-206** |

### 2.6 UNRESOLVED（无法判定，不强行归类）

| # | 条款引用 | 为什么判不了 | 出路 |
|---|---|---|---|
| U01 | `IO_002:60` `{signal,support,snr,variance,ivar}` vs `DATA-002` `{signal,support,variance,ivar,mask}` | **两处都标 `ACTIVE_NORMATIVE`**，无上位裁决 | **DOC-203**（Q8 块词表唯一登记处） |
| U02 | 6 份「被引为权威但不存在」的文档 | 口径相关：口径① 恰 6 处可复现，但其中 1 处只是**路径写法**问题（文件实际在位）⇒ **不能断言「6 份权威文档不存在」为真** | **DOC-204**（逐处核实并如实登记） |
| U03 | `lib/` 代码引用的 `docs/contracts/tasks` 路径 **40 处**不存在 | 多数为历史项（已注明「已删除/已退役/已刷新」） | **DOC-204**（分类登记） |
| U04 | `minmax` 存废 | §4.5 禁 min/max vs schema 枚举含 minmax vs §9.71 裁决 3.4「以 WBPP 脚本表为准」 | ✅ **已定**：一手实测 `BPP-engine.js:2695-2719` 清单内**无 minmax** 且 `:1239-1240` 明文拒绝 ⇒ **维持禁用**（`FIX-204`） |
| U05 | 预检页形态 | §3.5（交互弹窗）vs §9.71 裁决 11（无交互窗）——**割裂在最高设计内部** | ✅ **已定**（§9.74 裁决 7-a + 最高设计 §3.5 已订正） |
| U06 | `P2Stage2Config` 的键名 `hips`（`:1060`）vs CLI `hips_paths`（`:293`）是否同键 | 无裁决 | **FIX-207**（统一键名时定） |
| U07 | export 的 `source` 是单值还是「一组帧」 | §9.71 裁决 2 说一组；详细层写单值 | **FIX-207**（统一键名时定） |
| U08 | 投影集：设计冻结 8 种 vs 实际实现 4 种 vs 文档说 1 种 | 三者不一致，**以可运行验证为准** | **FIX-205**（先核清实际实现数） |
| U09 | Phase3 输入接受域 | `DATA-002:133`（phase1 frame 可直进）↔ `:39`（phase1 最小平面集含 variance）↔ `:188-189`（Phase3 拒 variance 输入） | **DOC-202**（按最高设计 §1.2 判：接受任一兼容 HiPS） |
| U10 | 4 条前置裁决（Q2 v6 去留 / Q4 资源时序唯一列合同 / Q6 事件流唯一 schema / Q8 块词表唯一登记处） | 需先定，否则连动的多条订正无法落 | **DOC-203**（**先定这 4 条**） |

## 3. 闭合追踪

- **已闭合**：D01（`d519a67a`）、N01（`d519a67a`）、U04、U05；
- **本包承担**：V01–V22（22）、G01–G12（12）、O01–O04（4）、D02–D07（6）、N02–N04（3）、U01–U03 + U06–U10（8）；
- **每个条目在 `ACCEPTANCE.md` 中必须有归宿**：`CLOSED（附证据）` 或 `变更 claim 裁决记录` 或 `待定（附实验结论）`。

## 4. 前置裁决（Q2 / Q4 / Q6 / Q8，前台 2026-09-20 定，供 DOC-203 落笔）

> 依据：`00_README.md` §5「自主裁决授权」（门禁/判据问题与工程实现选型可自行裁决并直接落地）、
> `ASTROCS_DESIGN.md` §0.1（只有一份权威链）、§12（**禁止用版本号作为生效或退役条件**）、
> `CONTROL_PACK_SPEC.md` §3.3（逐条给可复核依据）。四项均**不涉及**须上呈负责人的六类事：
> 不动三命令划分、不动 JSON 输入输出合同的结构性形态、不动 HiPS 产品数据模型、不删任何交付物。

### 4.1 Q2 —— V6 合同层的去留

**裁决：在位保留，身份归一为「设计档案 / 产品族专用投影（非生产目标态）」，删除版本号退役窗口，weight_mode 家族按 A44 作废。**

| 面 | 实测 | 处置 |
|---|---|---|
| `contracts/schemas/v6/**`（10 件产品族 schema，含 49 条 `PENDING_OWNER_SIGNOFF`，fail-closed） | 在位、被 `tests/contracts/v6/` 独立 Oracle 覆盖 | **保留在位**；身份 = 「产品族专用投影（非生产目标态）」，`fail-closed` 语义不变 |
| `docs/contracts/v6/**`（16 篇） | 在位 | **保留在位**；身份 = 「人类可读设计档案」 |
| `contracts/proposals/v6/**` | 在位 | **保留在位**；身份 = 「提案归档」 |
| `docs/contracts/DATA_SEMANTICS.md §31` 写「状态：ACTIVE（V6 目标态集成）」 | 与 `UNIFIED_OBJECTS.md §4` 的退役窗口互斥 | **删「ACTIVE / 生产目标态」措辞**，改为「**设计档案 / 产品族专用投影**；生效与退役条件由**变更编号**决定，**不得**用版本号窗口表达」 |
| `UNIFIED_OBJECTS.md:72-84`、`unified_object_registry.json#deprecation`、`INDEX.yaml:4` 的 `0.11.0-alpha.2 → 0.12.0` 版本窗口 | 违反 §12 | **版本窗口 → 日期 / 变更编号**（如 `RELEASE-03 / CHG-2026-09-20-01`） |
| v6 内的 `weight_mode` 家族（`frozen/02_WEIGHT_MODE_VOCABULARY.md`、`01:33`、`00_README.md:22`、`data/05_*`、`data/10_*`、`W6_SCHEMA_INTEGRATION.md:65`、`frozen/astrocs.v6.contract-freeze.v1.json` 等） | A44 | 按 §9.73 A44 **作废键面**（删键 / 改写 / 加作废留痕），**文件本身不删** |

**理由**：不删 = 无破坏性变更（避免触碰「删除交付物」上呈线）；去版本号窗口 = 落实 §12；A44 作废 = 落实负责人裁决。

### 4.2 Q4 —— `resource_timeseries.csv` 的唯一列合同

**裁决：唯一列合同 = 生产实现 `lib/infrastructure/cli/resource_recorder.h:260-266`（20 列，run 收尾一次性落盘，被 manifest / 目录树哈希覆盖）。**

- 生产侧实测：`elapsed_seconds,stage,cpu_pct,system_cpu_pct,active_workers,runnable_workers,rss_bytes,pss_bytes,commit_bytes,page_faults,read_bytes,write_bytes,queue_depth,lock_wait_ns,progress,threads,active_compute_threads,per_thread_cpu_max_pct,per_thread_cpu_sum_pct,io_wait_pct`（20 列）；
- `lib/infrastructure/cli/resource_events.h:6` 明文「资源时序曲线的**唯一载体** = 磁盘工件 `resource_timeseries.csv`」；`commands.cpp:463,481,561,696,947` 均以该工件为准；
- LOG-002（`docs/architecture/observability/RESOURCE_MONITORING_CONTRACT.md §3`）的 21 列「每秒采样 + seed 行 + 指纹链」CSV 是**监控伴随器的原始数据**，与上面**不是同一工件**；
- **处置**：LOG-002 侧工件名固定为 `monitor_timeseries.csv`，文档显式声明「两工件不同名、不互替」；`resource_timeseries.csv` 的列合同**只在生产实现与引用它的合同里出现一处**。

### 4.3 Q6 —— 运行事件流的唯一 schema 身份

**裁决：唯一运行事件流 schema = `lib/infrastructure/cli/protocol.h`（`ValidateEventV1`，发送侧硬闸）+ `lib/infrastructure/cli/jsonl.h`（`JsonlEmitter`）。**

- 生产实测：`jsonl.h` 发 `schema_version / event_id / run_id / ... / kind ∈ {progress,resource,artifact,backend,final}`，`protocol.h:ValidateEventV1` 是 CLI-004 冻结的**进程协议硬闸**；`ASTROCS_DESIGN.md §6.3`（`:750`）同族；
- `docs/architecture/observability/STRUCTURED_LOGGING_CONTRACT.md`（LOG-001，`astrocs.log.event.v1`：`schema/seq/ts/run/task/node/module/phase/commit/host/level/event/units/elapsed/diagnostic`）是**另一份合同**：结构化**日志**（人可读摘要 + 机器 JSONL 双通道同源）；
- **处置**：LOG-001 **保留**为「结构化日志合同」，但**必须显式声明它不是运行事件流**，且其事件键名（`event`）与运行事件流的 `kind` **不得混用**；运行事件流的字段名 / 枚举 / 顺序键**唯一**以 `protocol.h` + `jsonl.h` 为源；两份流各用**不同工件名**，不得互相冒充。

### 4.4 Q8 —— 块词表的唯一登记处

**裁决：唯一登记处 = `lib/infrastructure/aio/include/aio_pipeline.h` 的「标准块定义表」。**

- aio 是**文件级唯一 I/O 边界**（`ASTROCS_DESIGN.md §9`、§9.73 U5），块词表属 aio 的内存块合同 ⇒ 登记处归 aio；
- 实测 `lib/infrastructure/pipeline/orchestrator/cpp/src/orchestrator.cpp:3211-3213` 的 6 个名字**不是块词表**，而是 `stage_trace.jsonl` 的**跟踪子集**（`data/star_det/star_det_psf_compat/psf/star_measurements/photometric_match`）⇒ 只作**引用**，不得自称第二套登记处；且 orchestrator 已在 `§7.1` 退役计划内；
- `docs/contracts/DATA_SEMANTICS.md §11.1` 的「帧内命名块」表**只作引用**；
- **处置**：① 把 `variance` 块补进 aio 标准块定义表；② 删除 `aio_pipeline.h:284`「未列出的自定义块名也允许」；③ 补机器判据：块名 ∉ 标准表 ⇒ 判红（能红能绿）。

> 依据：`TASK_LIST.md` §3 实验纪律；`ASTROCS_DESIGN.md` §11.1.1；`00_README.md` §2.3 B 类清单。
> 每个结论都要求：三种数据面 + 判据事前冻结 + 判据非退化 + ≥5 轮独立复核 + 误差预算。


### 4.5 C01 —— `psfsw_robust_weight` 数据对象的去留（**负责人 2026-09-20 裁决：B = 真删，14→13**）

**上呈依据**：`00_README.md` §2.4「顶层三命令划分与 JSON 合同的**结构性**破坏变更、HiPS 产品数据模型的**破坏性**变更（须先上呈）」——本项属「统一数据对象合同的结构性破坏变更」，故上呈。

**现状（可复核）**：
- `docs/design/UNIFIED_MODEL.md §2` 的「14 个数据对象」含 `psfsw_robust_weight`；
- `contracts/schemas/unified/psfsw_robust_weight.schema.json`（+ `examples/`、`negative/`、`port_contract.schema.json`）；
- `contracts/data/unified_object_compatibility_map_v1.json`、`docs/contracts/unified_object_registry.json`；
- `tests/contracts/test_unified_object_contract.py:33-51` 把「14 对象清单」与「可否作权重」判定**逐字**锁为合同；
- 全仓引用 **124 处**（含 v6 合同层 `contracts/schemas/v6/astrocs.v6.psfsw.v1.schema.json`、`contracts/data/v6_*`、`contracts/proposals/v6/**`）。

**裁决（负责人逐字选择）**：**B —— 按 DESIGN-DRAFT §3.1-C01 彻底删除该对象（14 → 13）**。

**执行要求（交后续任务）**：
1. 删 `contracts/schemas/unified/psfsw_robust_weight.schema.json`、`contracts/schemas/unified/examples/psfsw_robust_weight.example.json` 与 `negative/` 中该对象的反例；
2. `port_contract.schema.json`、`unified_object_compatibility_map_v1.json`、`docs/contracts/unified_object_registry.json` 去引用；
3. `docs/design/UNIFIED_MODEL.md §2` 由 14 改 13（并说明退役依据）；
4. `tests/contracts/test_unified_object_contract.py` 的 `OBJECTS`/`VERDICT` 同步（**收窄断言，不得删测试、不得放宽其它门禁**）；
5. v6 合同层（`contracts/schemas/v6/astrocs.v6.psfsw.v1.schema.json`、`contracts/data/v6_*`、`contracts/proposals/v6/**`）按 §4.1 Q2 裁决 = **在位保留的设计档案**，其 `weight_mode` 家族按 A44 **作废键面**、**文件不删**；但**必须去掉对 `psfsw_robust_weight` 作为 canonical 对象的引用**（改为「v6 提案归档，canonical 已退役」）；
6. 变更 claim `CHG-2026-09-20-PSFSW-RETIRE`：裁决原文 + 影响面 + 兼容策略（旧产品若声明该对象 ⇒ **显式拒绝 + 迁移提示**）；
7. 回归：`CHK-CONTRACT-TEST`、`tests/contracts` 全绿；`python3 ci/run_checks.py --check CHK-CONTRACT-TEST` rc=0。

## 5. 实验结论（C 类，截至 2026-09-20；结论正本在各实验单元 README.md）

### 5.1 EXP-202 —— NaN 处置（单元 `run/RELEASE-02/实验/E08-NaN处置/`）

**定案：掩膜**（样本级掩膜 + 覆盖级 NaN + 强制计数），**不是传播、不是分场景**。

| 数据面 | 传播支 L1 污染率 | 传播支 L2 污染率 [95% CI] | 掩膜支 |
|---|---|---|---|
| S1 纯合成 | 1.0000 | 0.06622 [0.06107, 0.07148] | 判据全绿 |
| S2 M16 真实信号 + 科学噪声 | 1.0000 | 0.06622 [0.06107, 0.07148] | 判据全绿 |
| S3 testdata 真实（NGC1727） | 1.0000 | 0.04725 [0.04270, 0.05193] | 判据全绿 |

- 非退化正例：真值无坏像素 ⇒ 两支在三面逐位相等；负例：零填 / 不重归一 / 现行 `integrate.cpp` 合同 ⇒ 判据红。
- 判据冻结：`PREREGISTRATION.md` sha256 `539d83a0…`（2026-09-20T08:30:22Z）+ 修正案 A1/A2（跑后未改）。
- ⇒ **`docs/science/DRIZZLE.md:116` 与 `STANDARDS_REGISTRY.md:173,271`（DISP-DRZ-004「传播、不掩膜」）判错，须撤销闭环**；`NUMERIC_STANDARD.md:13` 按精确口径改写。
- 需先改 Oracle/负例再改实现（现行 `p1drz` oracle/negative 各 4 通过 0 失败，把「传播」锁成了合同）。

### 5.2 EXP-206 —— 噪声模型两套取哪套（单元 `run/RELEASE-02/实验/E12-噪声模型两套/`）

**定案：取 A（`lib/algorithms/noise_snr/cpp/src/noise_model.cpp`）；B（`wrapper_phase1/noise_model.cpp`，HEAD 已退役 f0d4a468）不得保留。**

- A：纯合成 K1 −0.51/−0.11/−0.04%（门 5%）、K2 掩膜无偏 ≤0.10%（门 2%）、K3 系数 ≤7.3%（门 10%）、K4 **fail-closed**；M16 −4.62/−3.48%；testdata 真实 4K 帧 116.36 vs 参考 113.11（+2.87%）。
- B：K2 **+2.74…+2.93%（红）**、K3 常量平面 maxdev **+2251%**、K4 **fail-open**（var=1e-12 ⇒ ivar=1e12）；M16 **+104%/+231%**；testdata **+78.6%**。
- 判据冻结 sha256 `d5119c6f…`（2026-09-20T08:47:38Z），5 条偏离逐条登记 `DEVIATIONS.md`；≥5 轮复核（独立 NumPy Oracle rtol 1e-9、`_f64` vs `_v1` 逐位相同、换切片、换判据顺序、第三方 astropy）。
- 误差预算 RSS 0.578% ⇒ 门余量 ≥8.6× / ≥3.4×。
- 新发现（登记为**遗留项**，不在本包范围）：
  1. **第三处 σ 估计** `lib/algorithms/star_detection/wrapper_phase1/star_detector.cpp:30-70`（整帧 3σ 裁剪后 std）在真实 4K 帧比同几何参考高 **+71.8%** ⇒ 违反 §3.6「单一生产实现」；**登记为后续控制包条目**（不属本包六个实验任一，需新实验单元定案）。
  2. `variance_floor` clamp 可产出「rc=0 + deg=0 + var=1e-12」伪有效模型（仅输入误读场景复现）⇒ 硬化项，补判据与守卫。
  3. `docs/plugins/algorithms_phase1/07_noise_snr.md:110-127` 仍写「两套并存/未闭合/须上呈」；`ci/ledgers/dormant_algorithms.json:310,326,342,358` 仍把 A 的符号记为 dormant ⇒ 须订正。
  4. `docs/science/NOISE_MODEL.md:146+` 的平面场 oracle 真值漏 patch 内梯度项 ⇒ 假红 +49.9%（含项后 +3.6%）⇒ 须订正 oracle 表述。

### 5.7 EXP-205 —— SNR 三口径精度对比（单元 `run/RELEASE-02/实验/E11-SNR三口径精度/`）

**定案：三口径无全局最优，只有适用域**（**未预设稀疏最好**，结果确实两面）。

- **稀疏重建在"地面视宁度受限 + σ 场平滑可分辨"时确实最优**：testdata M42 主切片三帧 sparse 0.093/0.228/0.099 vs 帧级 0.111/0.384/0.115，**全部显著胜出**（V2 切片同向）；
- **但高分辨率高对比结构（HST M16）上帧级标量反而最好**：0.112 vs 稀疏 0.261、稠密 0.198（V5 六个配方同向）；
- **稠密口径在任何面上都不最准**，且 **67 MB/帧超存储门 64×**；
- **稀疏失效边界 Δ*/ℓ ≈ 1**（面 A 0.98 / C 0.50 / B 0.35），**比平稳 SE 场解析预言的 4.38 早 4 倍**；失效机制 = **cell 稳健 MAD 被 cell 内未分辨结构抬偏**（+0.02 dex@Δ=64 → **+0.42 dex@Δ=1024**），**不是插值误差**；
- 判据冻结 sha256 `81244053…`（08:30:41Z）+ 追加 01/02，`sha256sum -c` 全过；≥5 轮复核（V1 换实现 / V2 换切片 / V3 换判据顺序与 ℓ 定义 / V4 换估计量与几何 / V5 换数据面配方 / V6 换自助法种子，+V0 hold-out 诊断）；
- **判据自身缺陷已如实登记**：① 帧级臂 RMSE **在定义上恒等于 s_field** ⇒ 帧级精度门**永远为真**（退化）；② τ_B 是单 patch 噪声却用作 4096-patch 中位量的门 ⇒ 灵敏度差 2 个数量级；③ roll-7 打乱在 s_field 小、cell 偏置大时反而降 RMSE ⇒ 改用全随机置换后非退化性成立；
- 误差预算：解析 oracle 交叉点 Δ/ℓ=4.379、渐近 √(13/9)=1.2019；预算闭合缺口已定位为**未含的 cell 估计量偏置项**。
- 待订正：D1 `docs/plugins/algorithms_phase1/07_noise_snr.md:138`（Δ=64 依据需补记实测 ℓ=50.9–208.3 px 与 Δ*/ℓ=0.50–0.98）；**D2 `ASTROCS_DESIGN.md §4.3`（`:518` SP-0）须记录帧级精度门退化**（变更 claim）；D3 `docs/science/NOISE_MODEL.md:86` **无需订正**（1.44 复算一致）；D4 生产实现**无需改动**；D5 `config/defaults.json:691-699` 保持 spacing_px=64 但建议新增运行期适用性判据（**不得静默降级**）；D6 建议逐 cell 追加未分辨结构诊断。

### 5.6 EXP-201 —— 天光采样点权重：SNR² vs control_ivar（单元 `run/RELEASE-02/实验/E07-天光采样点权重/`）

**定案：取 `control_ivar`**；但订正理由只能写「**SNR² 不是有效逆方差代理**（`∝` 要求信号恒定，而天光面的值在变）」，**不得**写「SNR² 已被证明劣化天光面重建」。

- 生产型（柔性稀疏样条 78 系数 + 逐帧平面）配置下两支**不可区分**：Δm_tilt=+3.5e-6（95% CI [−5.9e-5,+6.9e-5]）≪ δ_detect=0.0496（小 1.4e4 倍）；
- **预注册的两条非退化判据在该配置下都不通过**（①无梯度时度量差不归零 Δm_rms=+1.16e-3；②等权/反逆方差/value²/打乱四种明显错误权重 CI 全含 0）⇒ 按预注册自身规则，主配置结果**不得**用作任何一方的证据（已如实登记）；
- 补做**刚性扫描**恢复鉴别力：公共面收紧到单一平面（3 系数）后红例全红，**control_ivar 显著优于 SNR²**（Δm_rms=+3.4e-3，CI 不含 0）；
- 三数据面齐备；判据冻结 sha256 `0d41e29b…`（08:29:34Z，跑后哈希不变；运行前仅新增 METRIC_ADDENDUM）；≥5 轮复核 5/5 一致；
- 误差预算 σ_stat=0.00178 / σ_model=0.02475 / δ_detect=0.0496 / δ_practical=0.10。
- 待订正：`docs/plugins/algorithms_phase2/10_sampling.md:38,45` → control_ivar；`ASTROCS_DESIGN.md §4.4`（`:537`）「采样点带 SNR 权重」→ control_ivar（**变更 claim 落地，见 §6**）；`ASTROCS_DESIGN.md §4.3`（`:493`）**不改**但补边界句；`docs/modules/phase2_samp.md:56-59`、`registry/astrocs.phase2.sample.md:69-72`、`docs/science/CONTROL_WEIGHT_SNR.md`、`docs/science/UNCERTAINTY_AND_COVARIANCE.md` **不改（实验证实其正确）**；DESIGN-DRAFT §4.1-S1 的判据表述需改（模型过柔时红例不红 ⇒ 须给刚性扫描）。

### 5.8 EXP-204 —— 排异 N ≤ 3 是否保留「不排异」（单元 `run/RELEASE-02/实验/E10-小N排异/`）

**定案（前台依实验裁决，保守读法）：保留 `1 ≤ N ≤ 3 → none`。** 最终逐像素映射表：

| N（几何可贡献帧数） | 方法 |
|---|---|
| `1 ≤ N ≤ 3` | **none（不排异）** |
| `4 ≤ N ≤ 5` | percentile clipping |
| `6 ≤ N ≤ 15`（或 BIAS/DARK 类帧） | winsorized sigma clipping |
| `N ≥ 16` | linear fit clipping |

- 三数据面 + 6 轮独立复核（R1 换实现 9.68M 次比对 0 不一致；R2 换 API 路径 0/800k；R3 换切片不翻转但修正 N=2 定性；R4 换顺序/聚合 0/775；**R5 对抗轮找到反例**；R6 构建漂移 0/158 760）；判据冻结 sha256 `bd8982d6…`（08:36:04Z）；
- **低/中电平（≲2700 e⁻/pix，含真实数据 NGC1727 1110 ADU、LDN43 2664 ADU）**：N=3 强制 percentile **有损**（`ρ−1`=0.3–27% ≫ τ_ρ=0.31%）；N=2 **无益/不可用**（A1s 83.5% 像素无输出）；
- **R5 对抗轮反例（如实登记，未改判据）**：高电平 ≳3400 e⁻/pix 时 N=3 判**占优**（`ρ=1.000000`、强抑制、`|b|≤τ_b`，16/16 配置，作者已复现）⇒ **判据由电平决定、不由 N 决定**，翻转边界 ≈3000–3400 e⁻/pix；
- 选保守读法的理由：① 低电平可达且常见；② 反例区（≳3400）**超出实验网格上界 1734 e⁻/pix**，属外延；③ 保留 `N≤3→none` 即**维持负责人 2026-09-19 的原裁决**，非新决定；④ 预注册未规定跨配置聚合口径（该缺口已登记为实验方法缺陷）。
- ⇒ 执行：`lib/algorithms/coverage/src/rejection.cpp` 的 `kPixelSmallNPolicy` 置 `kConservativeNone`（1 行）；`underdetermined_n` 默认保持 3。**若负责人改判对称读法（对齐 WBPP），改这 1 行即可**——已在 `SUMMARY.md` 显著登记供复核。
- 待订正（变更 claim `CHG-2026-09-20-REJ-SMALLN`）：`ASTROCS_DESIGN.md §4.5`（`:597-609` 旧四档表 ⇒ 上表单表 + EXP-204 标注 + 电平依赖注记）；`docs/plugins/algorithms_phase2/12_rejection.md`（冻结该表）；`docs/science/REJECTION.md:47-53,33-36,52,112-124`（补偏离代价量化与 SC-005 可达域条件）；下游断言**不得**用 `plan.method` 断言「N≤3⇒none」（当前解析为 percentile，实际不排异来自内核闸 `undet=3`）。

## 6. 实验驱动的最高设计订正（变更 claim，依 `00_README.md` §5 自主裁决授权 + §9.72「科学问题由实验证明」）

| 变更编号 | 对象 | 由 | 改为 | 依据 |
|---|---|---|---|---|
| `CHG-2026-09-20-UPM-CTRLWEIGHT` | `ASTROCS_DESIGN.md §4.4` | 天光采样点「配 SNR 权重」 | 配 `control_ivar` 权重；SNR 仅作 veto/质量门 | EXP-201（三数据面 + 刚性扫描；sha256 `0d41e29b…`） |
| `CHG-2026-09-20-NAN-MASK` | `ASTROCS_DESIGN.md §9` + `docs/science/DRIZZLE.md` + `docs/standards/{NUMERIC_STANDARD,STANDARDS_REGISTRY}.md` | NaN「传播、不掩膜」（DISP-DRZ-004 自称已闭环） | 样本级掩膜 + 重归一 + 强制计数；NaN 仅作零合格样本的唯一无效表示 | EXP-202（三数据面；L1 污染 1.0000、L2 0.0473–0.0662） |
| `CHG-2026-09-20-REJ-SMALLN` | `ASTROCS_DESIGN.md §4.5`（旧四档表） | `n≤3` 不排异 + 4–7 percentile / 8–15 winsorized / ≥16 linear fit（已被 :667 作废但仍在） | `1≤N≤3` none / `4≤N≤5` percentile / `6≤N≤15` winsorized / `N≥16` linear fit（并注明电平依赖 ≈3000–3400 e⁻/pix） | EXP-204（三数据面 + 6 轮复核 + 对抗轮；sha256 `bd8982d6…`） |
| `CHG-2026-09-20-NOISE-A` | `docs/plugins/algorithms_phase1/07_noise_snr.md` + `ci/ledgers/dormant_algorithms.json` + `docs/science/NOISE_MODEL.md` oracle 表述 | 两套并存/未闭合；A 记为 dormant | 生产唯一实现 = A（`cpp/src/noise_model.cpp`）；B 已退役；补 `variance_floor` 退化判据 | EXP-206（三数据面；B 偏差 +104%…+231%） |

> ✅ **负责人已批准（追认）—— 2026-09-20**：上表三处对 `ASTROCS_DESIGN.md` 的订正（§4.3 `:493` 边界句、§4.3 `:519` SP-0 门退化、§4.4 `:538` 天光权重改 `control_ivar`、§4.5 `:597-603` 旧四档表作废横幅）**已获项目负责人明确批准**，满足 §0「修改本文必须由项目负责人明确批准」的形式要求。批准方式：负责人在 RELEASE-03 交付复核中逐项确认（本包先按 §9.72「科学问题一律待定、由实验证明」+ `00_README.md` §5 自主裁决授权执行，随后提交负责人追认）。
>
> 三处订正**均未改动任何科学公式、常数或容差本身**（EXP-201 的 §4.3 公式逐字未动，仅追加边界句），依据为六项实验的冻结判据（sha256 见上表）。


### 5.4 EXP-203 —— Phase2 signal 量纲（单元 `run/RELEASE-02/实验/E09-Phase2信号量纲/`）

**定案：现实现产出的 Phase2 马赛克 signal 平面 = 面亮度（surface brightness）**，不是通量。以**实跑产品**为准（`build/astrocs mosaic`）。

- 量纲链语义保持：sampler 不乘面积 → integrate 加权均值 → 逆归一 `flux=signal×support×A_cell` → writer 落盘 `signal=flux_sum/covered_area`；叶级上逆归一是恒等往返；
- D2 产品内 hierarchy 守恒：R_hier 中位数 = 1.0（20 case，最大 n=16,349,302；p16/p84 偏差 ≤5e-8）；
- D1 跨分辨率：常量场 R=1.0（CI[1,1]）；真实 testdata R=0.9999999727；
- D4 测光可对接：Σsignal·Ω/F_true = 1.000062（阶 9）；
- 非退化（负例红）：把输入平面改成通量语义后跑同一二进制 ⇒ R_cross=4.0、T≈−1；
- 判据冻结 sha256 `562d9f74…`（2026-09-20T08:39:28Z）。
- ⇒ **与最高设计 §5.3「导出只接受面亮度语义输入」一致，§5.3 无需订正**；但**守卫未接线**：`lib/phase3_session/p3_session.cpp:166-172,396` 只透传 BUNIT（缺省 ADU、无语义拒绝），守卫内核 `p3_rsmp_units.cpp:137-171` 与其会话层 `p3_v6_export.cpp` **未进构建**（`CMakeLists.txt:759-760`）；且 Phase1/Phase2 FITS tile **无 BUNIT**、properties 无 pixel_semantics ⇒ 即使接线也会 REJECT 当前产品。**登记为遗留项**（订正清单 C1–C8 见 `ALIGNMENT-5.3.md`）。

### 5.5 FIX-205 报告的科学缺口（登记为遗留项，非本包范围）

`lib/algorithms/projection/p3_proj_v6.cpp` 的 **SIN 内核**在 0.5"/px 尺度往返误差 **2.5e-5 px**，超 SCI §7 冻结容差 1e-6 px 的 **25 倍**（TAN/CAR/AIT ~3e-10 px）；v6 测试 7×7 网格在 0.02 deg/px 取样，漏掉该像素尺度。⇒ 登记为后续控制包条目（证据 `run/RELEASE-03/logs/FIX-205-v6-sin-roundtrip.log`）。



