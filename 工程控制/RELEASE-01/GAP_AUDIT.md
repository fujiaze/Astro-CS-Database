# 工程控制 / RELEASE-01 差异审计表（GAP_AUDIT）

> 本表由 AUD-001 产出（SubAgent 只找不改），经 SCI-001 科学裁决合并后定稿。差距类型：`缺口` / `违规` / `过时` / `漂移` / `无主` / `UNRESOLVED`。状态：`OPEN → ASSIGNED → CLOSED`（CLOSED 仅由前台验证后写入）。

## 1. 差距汇总

| # | 模块/条款 | 差距类型 | 级别 | 文档条款引用 | 现状复现路径（文件/行/命令） | 建议归属 | 状态 |
|---|---|---|---|---|---|---|---|
| G-01 | ENGINEERING_SPEC §7 根条目 | 违规（文档） | P0 | `ENGINEERING_SPEC.md` §7 | `python3 tools/doccheck/check_engineering_constraints.py` → `manifest_not_wider_than_spec7: ACCEPTANCE_SPEC.md` | 负责人裁决（ACCEPTANCE UNRESOLVED-1） | OPEN |
| G-02 | docs/ci/01_CHECKS.md §2 ↔ ci/checks.json | 违规（文档） | P0 | `docs/ci/01_CHECKS.md` §2 | `python3 ci/check_registry_doc_sync.py` → `registered_not_documented: CHK-E2E-REPRO, CHK-EXIT-CONSISTENCY` | 负责人裁决（UNRESOLVED-2） | OPEN |
| G-03 | 版本纪律 | UNRESOLVED | P1 | `ASTROCS_DESIGN.md` §12；`ACCEPTANCE_SPEC.md` §6；`FIN-001.md` | `./build/astrocs --version` = `0.11.0-alpha.2+ge88a0d89…` vs 文档"Alpha 前无版本信息"；`docs/ci/01_CHECKS.md` 又要求 `--expected 0.11.0-alpha.2` | 负责人裁决（UNRESOLVED-3） | OPEN |
| G-04 | mosaic/sampling+upm：稀疏天光面 | 缺口 | P0 | `ASTROCS_DESIGN.md` §4.4；`docs/plugins/algorithms_phase2/10_sampling.md` §4.2；`11_upm.md` §4.2–4.4 | `grep -rIl sky_plane lib/ contracts/ config/` → 0；`sky_sample_spacing` → 0 | AUD-001 确认 → 修复任务 | OPEN |
| G-05 | runtime：locality-aware 编排与流式内存 | 缺口 | P0 | `ASTROCS_DESIGN.md` §8；`docs/plugins/infrastructure/19_runtime.md` §4.2–4.3 | `schedule_policy`/`locality_first`/`cache_budget_mb` → 0 命中 | AUD-001 确认 → PERF-001 | OPEN |
| G-06 | gaia_xpsd_client：查询合并 + 两级缓存 | 缺口 | P0 | `docs/plugins/infrastructure/22_gaia_xpsd_client.md` §4.1–4.2；`ACCEPTANCE_SPEC.md` §L2"缓存复用" | `prefetch_neighbors`/`tile_scheme`/`mem_cache_budget_mb` → 0 命中 | AUD-001 确认 → PERF-001 | OPEN |
| G-07 | cli/export 文案 | 漂移 | P1 | `docs/plugins/algorithms_phase3/14_projection.md` §4 | `./build/astrocs export --help`："当前唯一实现 TAN" vs `lib/algorithms/projection/p3_proj_v6.cpp:270,349`（冻结 8、实现 4） | AUD-001/API-DOCS | OPEN |
| G-08 | 文档树级残留（非权威文档） | 过时 | P1 | 文档风格红线（DOC-001 步骤 3；DOC-002 验收门） | 见 `DOC_RESIDUE_INVENTORY.md`（活动文档：129 文件含日期、31 含 V19、16 含 W#-A#、8 含头部元信息块） | DOC-002 | OPEN |
| G-09 | `export --help` 投影实现状态 | 漂移 | P2 | 同上 G-07 | 同上 | AUD-001 | OPEN |

> 说明：G-01/G-02/G-03 为**文档包 ↔ 仓库**冲突（DOC-001 阶段发现，属"发现文档包内部矛盾登记 UNRESOLVED 上呈负责人"）；G-04–G-06 为**新文档包新增语义尚未实现**（DOC-001 一致性基线抽查发现，需 AUD-001 全量确认后定级）。

## 2. 按模块覆盖审计

### normalize（algorithms_phase1，8 模块）

| 模块 | 审计结论（含"无差距"） | 差距号 |
|---|---|---|
| calibration | 待 AUD-001 | |
| cosmetic | 待 AUD-001 | |
| star_detection | 待 AUD-001 | |
| psf | 待 AUD-001 | |
| platesolve | 待 AUD-001（DOC-001 已订正其死引用） | |
| photometry | 待 AUD-001 | |
| noise_snr | 待 AUD-001（07_noise_snr §4.1 新增 PSFSNR/PSFSW 对标，需 SCI-001） | |
| drizzle | 待 AUD-001 | |

### mosaic（algorithms_phase2，5 模块）

| 模块 | 审计结论 | 差距号 |
|---|---|---|
| admit/coverage | 待 AUD-001 | |
| sampling | 待 AUD-001（新增 sky_samples/star_mask 语义） | G-04 |
| upm | 待 AUD-001（新增稀疏天光面） | G-04 |
| rejection | 待 AUD-001 | |
| integration | 待 AUD-001 | |

### export（algorithms_phase3，3 模块）

| 模块 | 审计结论 | 差距号 |
|---|---|---|
| projection | 待 AUD-001（CLI 文案与实现不一致） | G-07 |
| resample | 待 AUD-001 | |
| fits_output | 待 AUD-001 | |

### infrastructure（7 模块）

| 模块 | 审计结论 | 差距号 |
|---|---|---|
| aio | 待 AUD-001 | |
| cli | 待 AUD-001（export 文案） | G-07 |
| runtime | 待 AUD-001（编排/流式内存未实现） | G-05 |
| observability | 待 AUD-001 | |
| gaia_xpsd_client | 待 AUD-001（缓存/合并未实现） | G-06 |
| benchmark | 待 AUD-001 | |
| hips_browser | 待 AUD-001 | |

## 3. 横向条款审计

| 条款 | 审计结论 | 差距号 |
|---|---|---|
| CLI 契约（help/--json/-y/-force/预检三级） | 待 AUD-001 | |
| config/ 分离（defaults.json/filters.json） | 待 AUD-001 | |
| 契约 schema（14 canonical） | 待 AUD-001 | |
| CI 检查器（70+） | 待 AUD-001 | |
| 版本纪律（Alpha 前无版本信息） | **不对应**：当前构建输出 `0.11.0-alpha.2+g…` | G-03 |

## 4. 合并裁决（AUD + SCI）

- P0 差距最终清单与修复归属：待 AUD-001 完成后合并裁决（已知 P0：G-01、G-02、G-04、G-05、G-06）。
- P1 差距最终清单与修复归属：待定（已知 P1：G-03、G-07、G-08）。
- P2/UNRESOLVED 登记：G-09；UNRESOLVED-1/2/3（见 `ACCEPTANCE.md` §3）。

## 5. 附录：DOC-001 一致性基线

以新文档包为权威，抽取 6 条最高设计硬约束与代码现状对照（只登记，不改）：

| # | 硬约束（文档条款） | 代码现状（可复现证据） | 对应结论 |
|---|---|---|---|
| B-1 | 三命令独立、阶段间只通过磁盘产品+manifest+哈希交换，禁止串成一次运行（ASTROCS_DESIGN §2/§3） | `lib/infrastructure/cli/command_tree.h:75-82` 只登记 normalize/mosaic/export；`./build/astrocs help` 实测三条目 | **对应** |
| B-2 | ACR dormant、旧路径无生产符号/CMake/文档入口（ASTROCS_DESIGN §8） | `python3 tools/check_legacy_exit.py` → `LEGACY_EXIT_PASS … ACR dormant 隔离 (prod_sources=270)` rc=0 | **对应** |
| B-3 | 投影首批冻结 8 种（ASTROCS_DESIGN §5.3） | `lib/algorithms/projection/p3_proj_v6.cpp:349` 冻结集含 8 名，`:270` 注明已实现 4（TAN/SIN/CAR/AIT） | **部分对应**（CLI 文案漂移 → G-07） |
| B-4 | Alpha 前代码与产物中不含任何版本信息；发布时 `--version` = `0.1alpha`（ASTROCS_DESIGN §12） | `./build/astrocs --version` → `0.11.0-alpha.2+ge88a0d89…` | **不对应** → G-03（版本口径待裁决） |
| B-5 | 一个进程只有一个资源调度器与线程预算源；模块不得硬编码 workers（ASTROCS_DESIGN §8） | `lib/` 内 OpenMP 使用 22 处；`omp_set_num_threads` 由预算注入（`aio_pipeline_engine.cpp:518`），未见写死 worker 数 | **待 AUD-001 逐点核**（需确认无写死） |
| B-6 | 数据对象按 UNIFIED_MODEL 区分（frame_snr / sky_samples / sky_plane / psfsw_robust_weight 等） | `docs/design/UNIFIED_MODEL.md` 新增 sky_samples/sky_plane/star_mask；`grep -rIl sky_plane lib/ contracts/ config/` → 0 | **不对应** → G-04（新增对象未实现） |
