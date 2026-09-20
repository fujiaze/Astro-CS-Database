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
