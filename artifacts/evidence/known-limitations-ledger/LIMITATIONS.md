# 已知限制登记（ACSD Known Limitations）

> 上游：`docs/ACSD_DESIGN.md` §1.3（非目标）、§12.5（状态阶梯）

本文件登记**现行**的限制陈述：现象 · 规范依据 · 归属/去向。只登记仍成立的限制。
条目号为稳定 ID，仓库内按「条目 N」解析引用。

**过程面**（严重度、状态、发现任务、裁决出处、迁移前原文快照）由同目录
`LEDGER.md` 承载，本文件不重复登记。

---

## A. 平台与验证面

1. **真实数据域**：工程/合成验收已完成；BASS + 2×2 + 3×3 与大规模真实数据的终验未达成（`FINAL_REAL_DATA_VALIDATION` 未过）。
2. **Windows x64 面**：`NOT_VERIFIED`——`VERIFIED` 要求正式平台（Windows x64）+ 真实数据验收通过；Linux 腿全绿不等于 Windows 绿，Windows 腿未实测。
3. **T1 数据集**：无真实数据（外部阻塞，Phase1 冻结前即记录）。
4. **ACR（CPU/GPU 异构）**：`DORMANT`——不进生产构建/加载/路由/发布。
5. **Sanitizer**：MSYS2 MinGW 无 ASan 运行库，WSL gcc 15 可跑。ASan/UBSan 运行时矩阵在 Linux 增量验证中为 `SKIP-evidenced`（以机器门 + 直接科学门禁代替），需 Windows/MSYS2 完整 toolchain 复核后方可视为全量关闭。
6. **GUI / Aladin 冒烟**：无 GUI 环境，未执行。
7. **性能快照**：现有性能基线为轻量快照（单机单次 wall 计时），未做多规模 / 火焰图完整 benchmark。

## B. 科学口径与实现限制

8. **Drizzle 相邻协方差**：pixfrac/resampling 引入，已量化（SNR-012）但未建模。
9. **HISS**：不携带 variance 产品。
10. **weight / rejection_count 诊断产品**：JSON 诊断，未输出为 Image HiPS。
11. **UPM ivar 回退**：输入帧无 ivar 产品时积分权重回退 support（`ivar_product_missing` 计数如实记录）。
12. **Phase1 SNR catalogue**：作为诊断保留，不作为科学权重。
13. **Phase1 不确定度子产品缺失（未关）**：CLI `normalize` 产出的 `p1_final.json` 实测 `n_variance_tiles=0` / `n_ivar_tiles=0`；写出器本身具备产出 variance/ivar 子产品的能力，缺口在 p1 节点未向 PipelineFrame 注入 variance 块。`ACSD_DESIGN.md` §3.1 规定不存在「权重模式」概念（全程只有 SNR），故该缺口的影响面 = 绝对 SNR / 不确定度链的可达性。
15. **接缝残余未随天光面修复消失（未关）**：真实帧足迹域上「无接缝」主张**不成立**——门内读数 `max|rel_step| = 7.2561e-03` 未超门，但同一次读数块的**三个独立诊断量全部越门**（`rel_step_net_max = 1.2948e-02`、`rel_step_d4x_max = 2.1915e-02`、`legacy_rel_max = 2.6110e-02`），且门只覆盖 **114/196** 条边界（82 条 `not_interior` 不进判据）。归因为 `node_spacing` 取 1.0°，现按几何导出。**可核对面** = `实验/additive-sky-seamless/results/production_e2e_seam_record.json`（逐字冻结 + 4 个源 sha256 + 17 项自检，正/负例均可跑）。
16a. **天光面生效后的负值像素属读取约定**：加性天光面生效后背景归零 ⇒ 产品出现负值像素，占比 **48.4%**（`full_neg_frac = 0.4839820861816406`；来源 `实验/additive-sky-seamless/code/c3_public_plane.py` → `results/c3_public_plane.json` 的 `/product/full_neg_frac`；HEAD 复跑同口径 0.4851）。下游读取与判据须按「允许负像素」的约定解释。
16b. **契约面缺陷：请求 δ 而无天光面产物时的降级（未关，major）**：`additive_mode=delta` 且无可用天光面时，`additive_mode_effective` 只能降为 `"c"`（全减、背景归零），而 `docs/detail/algorithms_phase2/11_upm.md` §7 明令该情形**判红**；全减正是本单元判定的**退化路径**（背景近 0 ⇒ 判据量分母病态、`rel_step` 无界）⇒ 负值像素是**退化路径被触发的症状**，不只是约定变更。**现行口径**：该降级为**具名判红面**——`lib/infrastructure/scheduler/src/module_adapters.cpp` 的 `degraded_reason=no_sky_plane_artifact` + `warning_codes=["P2-ADDITIVE-MODE-DEGRADED-NO-SKY-PLANE"]`，随 `p2_corrected.json` 与节点 manifest 落盘，产品照出、rc 不变。**仍缺**：① 全仓无任何测试/门禁**读取**该码（`lib/` `eng/` `docs/` `实验/` 只命中生产者）⇒ 判红面**恒绿**，不可判据；② 承载该降级落盘证据的源 run 树已回收 ⇒ 相关数字按「未验证」处理。补可判红负例属 P2 文件域（`module_adapters.cpp` 不在本单元文件域）。
17. **`drizzle_scale_arcsec` 合法域**：NaN / ≤0 / >824.52″ 一律 `rc=2` + `set_error`（契约侧登记；去向 `docs/science/DATA_SEMANTICS.md`）。

## C. 工程与产品面

18. **`p3_output.cpp` 原子提交为自实现**：插件文档要求「复用 infrastructure/aio 的原子提交设施」尚未兑现（该文件自实现 tmp+rename）。
19. **Phase1 产品标记载荷未关**：calibrated 产品未带 `ASTROCS*` 标记 / HISTORY。
20. **资源门回收判据在该负载下不达标**：双线性导出确定性触发 `alloc_reclaim_missing`（`reclaim_frac=0.0` / `unexplained_residual`，peak_rss≈2.6e8），根因未修（属实现/资源面）。
21. **验收证据文件缺位**：`实验/engineering-evidence/prerelease-v5/ISA-005/MEASUREMENTS.csv`、`evidence/v6_1_rework/TASK_LEDGER.csv` 缺位；相应测试显式 SKIP 并标注 NOT_APPLICABLE 依据（不以 skip 充绿）。
22. **三项挂账**：plate_solve `n_inliers` 门限（实测 38 < 40）、dll_loader Win32 工具例外、PSF 无独立单测。
23. **CAT-GAIA 两项**：`query_cache.out_mag` 非 bitwise 复现；混合 DB 光谱未 memcpy。
24. **声明路径与实现的存量缺口**：`docs/modules/MODULE_MAP.yaml` 164 条声明中 53 条路径不存在；`lib/` 代码注释 40 条引用不存在路径（真缺口 28 / 假阳性 12）。缺口清单与归属见差距清单登记。
25. **严格资源门旗标不可达**：`--resource-detail` 未登记进命令树 allowed 列表，CLI 以 `rc=2 unknown flag` 拒绝；`--strict-resource-gate` / `--on-resource-gate` 已登记进命令树，资源门已收窄为磁盘门（record-only），不改变运行判定。

## D. 未覆盖范围（产品面）

26. **多波长 / 多目标产品化**：本版成品仅 R 通道；G/B/H-alpha 与多目标批处理未做。
27. **运动目标 / 光谱 / 时序**：小行星、彗星等运动目标与光谱、时序产品不在本版范围。
28. **异常亮斑与 dense-field**：异常亮斑处理与高密度星场（dense-field）未覆盖。
29. **平台与形态**：Windows 腿、安装器、GUI/HiPS Browser、GPU/ACR 生产化、ARM 均不在本版范围。

## E. 架构与门禁判据面的现行限制

> 每条给：现象（文件:符号，不写死行号）· 规范依据（文档+条款）· 归属/去向。
> 本节只登记，不修任何实现。已由机器台账承载的条目见第 47 条的交叉引用。
> 条目号与发现编号的对照表见 `LEDGER.md` §1。

30. **内存静态预算与内存回压（调度准入）**
    - **现行口径**：内存预算是**调度准入输入**（在途节点估算峰值之和 > 预算 ⇒ 就绪节点排队等待，不拒绝、不改退出码），**不是资源门**（`ACSD_DESIGN.md §4.5` 运行前预检的「内存不设门」约束门禁语义，与 §8.3 的调度语义不互斥）；预算 = 实测可用内存（`lib/infrastructure/aio/src/aio_sysinfo.cpp#aio_system_available_memory_bytes`，aio 是文件级唯一 I/O 边界）× 可配置比例（默认 95，唯一数值源 `eng/packaging/config/runtime_resources.json`）；节点 `estimated_memory_bytes` 由 `lib/infrastructure/scheduler/src/plan_estimator.cpp` 真实估算给出，取值 = 实测估算（0 不作占位值）。
    - **规范依据**：`ACSD_DESIGN.md §8.3`「静态预算…内存占用永不越界」、`§9`；`ENGINEERING_SPEC.md §12`「队列有容量/背压」；`docs/engineering/PIPELINE_BLOCK_CONTRACT.md §5`；`docs/engineering/SCHEDULER_CONTRACT.md §3`。
    - **残余**：P1（normalize）节点的帧尺寸只在 FITS 头里，核心层无 IO 面 ⇒ 这些节点的**静态估算仍为 0**（`unestimable_reason` 显式登记），解除路径是 CLI 在 IR 侧带帧形状或模块 `plan()` 自报 `estimated_memory_bytes`。该缺口使**节点级回压在 P1 上恒不生效**（`0 + 0 ≤ limit` 恒真）；该缺口由**实测压力面**兜住：帧级并发闸门按「进程树 RSS / 预算」的运行期实测值决定（`lib/include/acsd/core/memory_pressure.h`，实现 `lib/infrastructure/scheduler/src/memory_pressure.cpp`；帧轴接线点 `lib/infrastructure/scheduler/src/module_adapters.cpp#p1_parallel_for`，压力分子 `lib/infrastructure/aio/src/aio_sysinfo.cpp#aio_process_tree_rss_bytes`）。**节点级静态回压仍按原样保留**（两条面互不替代：静态面管节点准入，实测面管帧级并发）。
    - **残余（丢弃安全点边界）**：§8.3「可丢弃重跑」的丢弃只允许发生在 op 帧体的**安全点**（尚未落盘任何本帧产物、且尚未进入 noise floor 全局注册表的注册→释放区间）。因此：① `wcs`/`writer` 两个 op 走串行帧循环、不经过帧级派发点，其帧体不参与丢弃重跑；② noise/drizzle 帧体在**越过注册表区间之后**不再可丢弃（该注册表以帧体栈地址为键，中途放弃会留下悬挂键并改变下一帧的方差数值 = 科学面污染）；③ 第 0 帧恒不作为牺牲帧（承载帧序基准与 drizzle 首帧 provenance，代价不对称）。上述三个边界是**设计的适用域**，不是待修缺陷；如要把丢弃窗口推到注册表区间之内，须先改注册表键（栈地址 → 显式生命周期 id）。
    - **ACR 同名常数（0.95）的归属**：仓内另有一处内存比例常数 `0.95`，在 `lib/infrastructure/acr/utilization/memory_budget.hpp`（`MemoryBudgetController`）。二者**不是同一口径**：ACR 的是「物理内存总量 × ratio，再减固定预留」的**异构消费模型**（调用点 `lib/infrastructure/acr/scheduler/dispatcher.cpp`），生产口径是「实测**可用**内存 × ratio」（含可回收页缓存 ∩ cgroup 余量）。该常数**有意原样保留、不做合并**：ACR 状态为 `DORMANT`（见条目 4，`ACSD_ENABLE_ACR=OFF` + `lib/infrastructure/acr/CMakeLists.txt` 的 FATAL_ERROR 守卫），且该文件含未加 `#ifdef` 的 `<windows.h>` ⇒ Linux 下无法编译，改动它无法在本机验证。**约束**：生产消费点的唯一预算来源是本口径（`resolve_memory_budget`）；ACR 的 `MemoryBudgetController` 只服务 ACR 自身；生产唯一预算来源 = `eng/packaging/config/runtime_resources.json`（经生成头 `runtime_resources_generated.h` 消费，实现侧零字面量）。
    - **归属/去向**：调度器域；判据面归 `eng/ci`。**不在 `eng/ci/ledgers/dormant_algorithms.json` 登记 `plan_estimator`**：该台账以 `lib/**/module.yaml` 的 `source_symbols` 为唯一键源，`plan_estimator` 未被任何 module.yaml 声明。

31. **生产零探针事件**
    - **现象**：`lib/include/acsd/core/normalize_workflow.h#ProbeEvent` 的**全部发出点**位于四个未接线调度器 TU（`lib/infrastructure/scheduler/src/{normalize_workflow,mosaic_window,export_stream,block_flow}.cpp`），生产路径（`module_adapters.cpp` 的 p1/p2/p3 节点）**零发出点**；探针汇聚点 `#ProbeSink::emit` 因此恒空。
    - **规范依据**：`ENGINEERING_SPEC.md §4.1`「调度器与模块预埋性能探针，编排参数的调优…基于探针数据」；`ACSD_DESIGN.md §9`。
    - **归属/去向**：同族已在控制包差距清单登记（「性能探针不成体系」）；本条补充**符号级事实**（零发出点）与判据缺口——探针 schema 门只做 `--self-test`，**不检查生产是否有发出点**。

32. **块流合同不可机器判定 + 门无完整性判据**
    - **现象**：`eng/contracts/block_flow/stage_block_flow.json` 的声明块名（**逻辑端口名**）与实现面（**产品文件名**）之间无机器可读映射；`eng/tools/quality/gen_block_flow_spec.py` docstring 自认该限制，判据实测 78 token / 75 未匹配。`eng/tools/quality/check_block_flow_conformance.py` 的 D1–D8 **不含**「登记册完整性」判据 ⇒ 漏登记（BFD-A11..A16 即人工补登）不会被门发现。
    - **规范依据**：`docs/engineering/PIPELINE_BLOCK_CONTRACT.md §1/§3`；`AGENTS.md §9`「判据必须非退化…门禁本身不合理时改进门禁本身」。
    - **归属/去向**：`eng/contracts/block_flow/**` + `eng/tools/quality/check_block_flow_conformance.py`（补「登记册完整性」正例/负例）。同族事实（`BFD-A17` 的降级显式性无判据）已写入该登记册的 `note`。

33. **`build_pipeline_ir` 保留多阶段串联面、生产侧该约束缺门**
    - **现象**：`lib/infrastructure/scheduler/src/runtime_client.cpp` 的 `want1/want2/want3` 允许一次调用串多阶段；三阶段组合**仅单测**使用，生产调用点传单 phase，但**无任何门约束生产调用点的 phases 参数基数**。
    - **规范依据**：`ACSD_DESIGN.md §8.1`「一次 CLI 调用只驱动一个阶段」；`AGENTS.md §6`「不串三阶段」。
    - **归属/去向**：调度器域 + `eng/ci`（生产调用点 phases 基数判据）。

34. **Phase2 分块大小为受控配置键**
    - **现状**：实现侧**零字面量**——`module_adapters.cpp` 的 `kP2TileLeafSpan` / `kP2TileShift` / `kP3DefaultSubBlockPx` / `kP3MinSubBlockPx` / `kP3MaxSubBlockPx` / `kP3DefaultQueueDepth` 取自 `using acsd::runtime_resources::…`，唯一数值源为 `eng/packaging/config/runtime_resources.json#orchestration_params`（CMake `string(JSON …)` → `runtime_resources_generated.h`）；归属登记 = `eng/packaging/config/config_registry.json#orchestration_params`。值域守卫在编译期 fail-closed（生成头 `static_assert`：span == (2^shift)²、min ≤ default ≤ max、qd ≥ 1）。
    - **规范依据**：`docs/engineering/SCHEDULER_CONTRACT.md §3`「线程预算、内存上限…队列深度、分块/窗口/子块大小；上述参数由配置/资源门决定」；`docs/engineering/CONCURRENCY_STANDARD.md`「默认」节（取值来源 = 配置）；`AGENTS.md §6`「不硬编码 block」。
    - **仍未覆盖（诚实边界）**：① 分块**随内存动态重规划**未接线——内存驱动的 `p2_block_plan`（`lib/algorithms/coverage/include/astro/phase2/block.h`）仍在 `eng/ci/ledgers/dormant_algorithms.json` 登记为生产不可达；本层只把**取值**受控化，未引入按内存改块的路径；② 改配置值 → 重配置 → 重编译的整链红例未跑（需独占 configure 窗口）。

35. **`CHK-ARCH503-MOSAIC-WIN` 的峰值驻留判据（收敛为唯一实现）**
    - **现行口径**：`lib/infrastructure/scheduler/src/mosaic_window.cpp` 的峰值驻留取**实测值**（窗口输出像素缓冲 `capacity()` + 路由指针向量 + 标量局部，循环内取最大）；判据收敛为唯一实现 `window_peak_residency_ok`——peak>0 / peak≤解析上界 / 与总图规模解耦 / 装满时 `window_tiles` 更大 ⇒ peak **严格更大**，四条同时成立才绿；两条负例（常量驻留 `sizeof(double)*4`、零驻留注入）必须判红。
    - **规范依据**：`AGENTS.md §5`「判据必须非退化…恒真门没有证据资格」、`§9`；`docs/engineering/SCHEDULER_CONTRACT.md §6`（负例要求）。
    - **残余**：`eng/ci/mutation_gates.json` 的判据已机器化——`eng/ci/check_mutation_gates.py` 断言 design_claim 锚存活（doc/§节号/行号/引文四查）、登记项仍有对象（driver/registration/evidence 的路径必须存在或在 gone_artifacts 显式登记）、gone_artifacts 棘轮（只减不增），带 `--self-test`。该 checker **尚未**在 `eng/ci/checks.json` 注册；真仓库当前判红：design_claim 引用的 `ACSD_DESIGN.md §11.1` 在现行设计中不存在，且 6 条登记产物的路径已消失。
    - **归属/去向**：mosaic_window 判据面 + `eng/ci`；同族已在控制包差距清单登记（「L2 性能门恒真」「空断言/恒真测试普查」）。

36. **`lib/phase{1,2,3}_session` 不在 `ENGINEERING_SPEC §7` 的 `lib/` 子目录清单内**
    - **现象**：`ENGINEERING_SPEC.md §7` 的 `lib/` 代码块只列 `algorithms/ include/ third_party/ infrastructure/`；实际存在 `lib/phase1_session/`、`lib/phase2_session/`、`lib/phase3_session/` 三个整阶段 Session 目录。`docs/modules/MODULE_MAP.yaml` 既未把三者列入 `modules`（23 个模块）、也未列入任何 `legacy_paths`，其 `declared_absent_paths` 亦**不适用**（这些目录真实存在，登进去会被门判 `stale_declared_absent`）⇒ 现状是**无登记面的未声明目录**。
    - **规范依据**：`ENGINEERING_SPEC.md §7`（`lib/` 子目录清单，强制；同节末「确需新增根目录条目，先登记并经负责人确认」同旨）；`ACSD_DESIGN.md §8.4`。
    - **归属/去向**：三选一（删除 / 迁入 `lib/infrastructure/` / 在 `ENGINEERING_SPEC §7` 与 `MODULE_MAP.yaml` 补登记并给去向）——待裁决。p3 已登记为删除对象，p1/p2 未见同等级登记。

37. **缺 tile 未进产品 provenance**
    - **现象**：`lib/infrastructure/scheduler/src/module_adapters.cpp#p3_op_writer` 硬编 `prov.missing_tiles = nullptr; prov.missing_count = 0;` ⇒ `P3Provenance` 的 missing 字段恒空，FITS HISTORY/manifest 不携带任何缺失信息。**可判据已在**：`lib/algorithms/resample/p3_resample.h#p3_sampler_cache_stats()` 的 `absent_reads`/`absent_entries` 已被 `#p3_op_resample` 调用并落进**中间节点 manifest** `tile_cache.absent_reads`，只是**没有**沿 `p3_resampled.json → p3_writer.json → FITS provenance` 链传下去。已由 `lib/algorithms/resample/module.yaml` 的 `known_defects: DISP-P3RSMP-004` 在**模块面**登记。
    - **规范依据**：`docs/science/PHASE3_HIPS_TO_FITS.md §8`（SCI-P3-001，FROZEN）「缺 tile → coverage=0, S=NaN，**provenance 记录 missing**，不中断」、`§9a-9`；`ACSD_DESIGN.md §10`「请求的 tile 缺失时如实报缺失，不返回父层内容冒充」；`docs/engineering/PHASE3_API_V1.md §4`。
    - **归属/去向**：Phase3 resample/writer 域（接线 `absent_reads`/`absent_entries` + 补「缺失 tile 时 provenance 必非空」负例）。

38. **`API-P3-001 §4` 的拒绝清单与 `SCI-P3 §9a-10` 正面冲突（API 文档过期）**
    - **现象**：`docs/engineering/PHASE3_API_V1.md §4`（API-P3-001，**FROZEN**）把 variance/weight/ivar/flux-per-pixel 输入模式列为 `ACS_ERR_UNSUPPORTED`；`docs/science/PHASE3_HIPS_TO_FITS.md §1`（SCI-P3-001，**FROZEN**）括注「variance/ivar 子产品输入为例外：按 §9a-10 必须显式消费传播，**不属拒绝项**」，`§9a-10` 明文「含 variance/ivar 子产品时必须显式消费传播（输出 VARIANCE/IVAR 扩展 HDU）」；`docs/science/DATA_SEMANTICS.md §27.2` 同旨（禁静默丢弃）。**代码事实按 SCI §9a-10 实现**：`p3_uncertainty_open`/`p3_uncertainty_propagate` 被 `#p3_op_resample` 调用、`#p3_op_writer` 写 VARIANCE/IVAR HDU。
    - **规范依据**：`ACSD_DESIGN.md §0.2`「同一主题只有一份正本」「双向对应」；`ENGINEERING_SPEC.md §3`（科学正确性优先；文档与事实不符时订正文档是义务）。
    - **归属/去向**：按 SCI §9a-10 订正 API-P3-001 §4 行（variance/ivar 移出拒绝清单，改为「必须消费/传播」）；不改代码。

39. **Phase3 三处文档引用的机器 schema 不存在（断链）+ 门禁盲区**
    - **现象**：`docs/engineering/PHASE3_API_V1.md §2` 引 `schemas/phase3_request_v1.schema.json`、`docs/detail/algorithms_phase3/15_resample.md §3` 引 `eng/contracts/schemas/export_product.schema.json`、`docs/detail/algorithms_phase3/16_fits_output.md §3` 引 `eng/contracts/schemas/fits_product.schema.json` —— **三者均不存在**（实测；`eng/contracts` 下 export/fits/p3 名式只命中 `data/examples/phase3_planar_fits_v1.example.json` 与 `schemas/phase_config_export.schema.json`）。**判据盲区（已实测）**：`python3 eng/tools/doccheck/check_doc_index.py --strict` **rc=0 / DOC_INDEX_PASS**，输出不含这三个路径；`CHK-CONTRACT-REF` 的实现是合同 **ID** 图（`eng/tools/check_contract_graph.py` + `check_data_artifacts.py`），其 `changed_paths` 不含 `docs/detail/**`、`docs/engineering/**` ⇒ 三处断链落在覆盖面之外。
    - **规范依据**：`ACSD_DESIGN.md §0.2`「docs/contracts/（合同说明，对应 eng/contracts/ 的 schema）…**双向可追溯**」「每份文档、每个机制都能追溯到本设计的一条要点」；`AGENTS.md §9`。
    - **归属/去向**：合同 schema 面 + 门禁判据面（须可红可绿：注入悬空路径必须判红）；补 schema 与改引用二选一，且**补门判据**「文档中形如 `eng/contracts/**.schema.json` 或 `schemas/*.schema.json` 的路径必须存在」属门禁面变更——待裁决。

40. **`PHASE3_HIPS_TO_FITS §16` 与 `PHASE3_PROJ_IMPL §16` 的 C4/C5 登记面已被反证**
    - **现象**：两处 §16 C4 称「§6.3 输入语义守卫在生产 export 路径**未生效（守卫未接线）**…守卫内核 `p3_rsmp_units.cpp` 与会话接线层 `p3_export.cpp` 未进构建」；C5 称「即使接线也会 REJECT（产品 FITS tile 无 BUNIT）」。**现行事实**：`lib/algorithms/resample/CMakeLists.txt` 已把 `p3_rsmp_units.cpp` 编入 `astrocs_p3_rsmp`；`module_adapters.cpp#p3n_guard_input_units` 已被生产 IR 节点链 properties/resample2/writer 调用（`#resolve_bunit` + 冻结串 `kP3BunitSurfaceBrightness`，缺 BUNIT/不可判 → fail-closed）；`#p3_op_writer` 强制 `p3_resampled.json#bunit` 校验。**仍然成立的部分**：`p3_export.cpp` 未进构建（`grep -c p3_export CMakeLists.txt` = 0），该半已由 `eng/ci/ledgers/spec_named_impl_gaps.json#SNI-S4-P3X-06` 登记。
    - **规范依据**：`ACSD_DESIGN.md §0.2`（同一主题只有一份正本）、`§12.5`；`ENGINEERING_SPEC.md §8`「文档集随代码持续维护更新，保持自解释」。
    - **归属/去向**：订正 §16（两处，同一实验单元只有一套文字），必须按「生产链已接线 / shell 仍未接线」**分开陈述**；登记面由 `SNI-S4-P3X-06` 承载。

41. **SIN / CAR / AIT 缺「往返误差上界 + 尺度域」声明**
    - **现象**：`lib/algorithms/projection/p3_wcs.cpp#p3_wcs_applicability()` 只对 **TAN** 返回非空，SIN/CAR/AIT 返回 `nullptr`（`p3_proj_probe`/`p3_wcs_check_applicability` 因此 fail-closed，**不静默回落 TAN**）；投影 Spec 结构体字段只有 `max_abs_crval_dec_deg` / `max_fov_deg` / `singularity_kind` 三项——**没有往返容差字段、没有尺度下限字段**。`max_fov_deg` 是**声明字段而非 make 硬门**（文档 §15.1 明说，实测一致）。
    - **规范依据**：`ACSD_DESIGN.md §6.3`「每种投影必须声明适用域（**含往返误差上界**），违反⇒拒绝」；`docs/science/algorithms/PHASE3_PROJ_IMPL.md §15.1`。
    - **风险面**：三者为内核-only（非产品声明）且 make 的四角域守卫 + `|CRVAL2|≤85°` 仍在 ⇒ 风险 = 「**内核被接线时缺门**」，不是「当前产品缺门」。
    - **归属/去向**：接线任务必须同时补 `P3WcsApplicability` 行（往返紧门 + 全局门 + `min_scale`），否则 `p3_proj_probe` 永远 UNSUPPORTED。

42. **T2 往返判据对 SIN 缺陷零区分力 + 文档 §15.6 与实现值不一致**
    - **现象**：`docs/science/algorithms/PHASE3_PROJ_IMPL.md §15.6` 写「每投影 pixel→world→pixel **< 1e-8 px**」，实现是 `eng/tests/unit/p3_proj/p3_proj_test.cpp` 的 `kRoundtripTolPx = 1e-6`（`p3_proj_wcs_oracle.py::roundtrip_tol_px` 对非 TAN 取 `contract["global"]` = 1e-6）⇒ **文档值与实现值不符**。T2 用例表给 SIN 的尺度是 0.02 deg/px = 72″/px，该尺度下 SIN 误差 2.1e-9 px ⇒ **判据恒绿**，对 world→pixel 条件数缺陷零区分力（该缺陷由反向门 `sin_roundtrip_gate.py` 承担）。
    - **规范依据**：`AGENTS.md §5`「判据必须非退化」；`ACSD_DESIGN.md §6.3`；`ENGINEERING_SPEC.md §8`（文档随代码维护）。
    - **归属/去向**：§15.6 改为实测门值并注明「T2 不覆盖 world→pixel 条件数缺陷，后者由反向门承担」；若要 T2 具备区分力，需把 SIN 用例尺度压到 ≤0.9″/px 并配尺度感知门（判据面变更）。

43. **AIT 可构造域（fail-closed 四角守卫不动）**
    - **现行口径**：`docs/science/algorithms/PHASE3_PROJ_IMPL.md §15.3/§15.5` 的 AIT 可构造域 = **内接矩形族**，上限 **229.24°×114.56°**（面积 8.000 sr = 4π 的 **63.7%**，与解析最优 2ab = 8 rad² 吻合）；**全天空（360°×180°）不可构造**——四角守卫下 360×180 帧四角 A = xp²/4 + yp² = **1.994 > 1** ⇒ 必判 HEMISPHERE（720×360 → 1.997）。性质：**数学上不可达**——椭圆内接矩形四角恒在椭圆上（A=1），覆盖整个椭圆的矩形四角恒在椭圆外（A=2>1）。
    - **规范依据**：`ACSD_DESIGN.md §6.3`；`docs/science/algorithms/PHASE3_PROJ_IMPL.md §15.1/§15.3/§15.5`；`ENGINEERING_SPEC.md §3`（文档与事实不符时订正文档是义务）。
    - **归属/去向**：`lib/algorithms/projection/` 的 fail-closed 四角守卫**保持不动**；`p3_proj.h` 头注「全天空展示」属内核注释，随接线任务一并订正。影响面为内核-only，不影响产品声明集。

44. **球面 S-H 的逐源像素闭合散布无逐像素门覆盖**
    - **现象**：Σ_p a_jp 应恒等于 A_drop（与裁剪实现无关的解析恒等式）。球面 S-H 路径实测闭合散布：1.00″/px（nside=2^18）max **+1.72e-06** / min −1.54e-06 / **σ=1.04e-06**；0.30″/px max +3.14e-05 / σ=1.22e-05；而独立平面精确算法（顶点枚举，完全不同的算法）闭合误差 **2.7e-16**（机器精度）。归因：球面 S-H 用相邻单位向量叉积重建大圆的条件数限制（近平行平面交点误差 ~1e-11 rad ÷ drop 角尺度 θ），`lib/algorithms/drizzle/` 下 `spherical_overlap.cpp` 既有注释已识别该机制。**性质：散布而非系统偏置**（16 像素 mean≈0）⇒ **帧级**通量闭合仍很好（实测 2.4e-08 @pf=1，远优于冻结 FP64 <1e-6 门），**但逐源像素的乘性随机误差 σ≈1.0e-6 @1″/px 不被任何现有门覆盖**（现有门是帧级/逐叶级）。
    - **规范依据**：`docs/science/algorithms/DRIZZLE_GEOMETRY.md`（ALG-DRZ-001）；`AGENTS.md §5`「判据必须非退化」；docs/science 冻结的 FP64 通量闭合门。
    - **诚实边界**：证据来自**忠实 Python 复刻**（叶边界与 astropy_healpix 逐位一致），**未**在产品二进制上复现。
    - **归属/去向**：drizzle 域 + 判据面；是否新增**逐像素/逐源**闭合门并冻结其预算待裁决（若 SCI-B 要声明 1e-6 级绝对 SNR 精度，该噪声不可忽略）。

45. **`leaf_fully_inside_drop` 解析面积快路径与 S-H 路径的面积不连续**
    - **现象**：`overlap_area_impl`（`lib/algorithms/drizzle/` 下 `spherical_overlap.cpp`）在「叶完全落在 drop 内」时返回**解析**叶面积 π/(3·nside²)，其余情况返回 **4 角大圆弧多边形**面积；二者之差 = 用弦代弧的系统性亏缺，标度 ≈ **0.5/nside²**（nside=512 实测 1.905e-06；1024 → 4.779e-07；262144 → 3.990e-11）。生产 nside（≥2^17）时 ≤1e-9 可忽略；但 W1 配置用 `nside=512`，此时不连续达 **1.9e-6**，与冻结的「FP64 通量闭合 <1e-6」**同阶**。解析快路径返回的是**真值**（HEALPix 像素面积恒为 4π/(12·nside²)），S-H 路径**低估** ⇒ 不连续方向 = 「部分覆盖叶被系统性低估」。
    - **规范依据**：`docs/science/algorithms/DRIZZLE_GEOMETRY.md`（ALG-DRZ-001）；`AGENTS.md §5`（真值无效应⇒归零；判据非退化）。
    - **归属/去向**：drizzle 域 + 性能尺度配置面；是否要求两条路径在同一 nside 下连续，或把「nside 下限」写入适用域——待裁决。

46. **drizzle 两处文档漂移**
    - **现象（两处，均为文档滞后于代码）**：
      1. `docs/science/algorithms/DRIZZLE_GEOMETRY.md §6` 记「跨线程数时 leaf 内浮点和顺序不同，**不保证 bitwise**」，而现行代码跨 worker **逐位一致**（实测 stripe 归约跨 worker digest 集合大小 = 1，且以 legacy 归约 5 个 worker 数 → 5 个不同 digest 为**负例**自证非退化）。
      2. `docs/detail/algorithms_phase1/08_drizzle.md §7` 记「生产调度路径不挂 variance 块 ⇒ has_variance=0 ⇒ uncertainty_available=false」，与工作区现状（`module_adapters.cpp` 的 variance 块接线**存在**，定案 2 / NoiseWeightModelV1 blank-sky variance，且 fail-closed）**不符**。
    - **规范依据**：`ENGINEERING_SPEC.md §8`（文档集随代码持续维护更新，保持自解释）；`ACSD_DESIGN.md §0.2`。
    - **归属/去向**：§6 订正为「跨线程数 bitwise 一致，由 `p1drz_merge_pipeline_lock` 回归锁守护」；`08_drizzle.md §7` 按接线现状订正。

48. **P4→P5 稀疏层生产接线（`in.sparse`）与 IDW 参数配置面：两条跨域前置**
    - **M06 现行接线**：稀疏层的唯一消费面 = 集成侧逐像素权重 API `weight_from_sparse_layer_pixel[_prepared]`（`weight_chain.cpp` 两处）：`w(x,y) = (SNR_layer(x,y)/F_ref,k)²·g_k²`——层值即**绝对** SNR，**不乘/除**帧级标量；`module_adapters.cpp` 的逐像素面直接调同一生产实现（单一实现 ⇒ 两入口口径不可能分叉）。帧级标量链（`compute_inverse_variance_weights`）**不消费**稀疏层：层所在帧的权释放到逐像素面（`weight_deferred_to_pixel_path[k]=true`、`weights[k]=0`、`weight_source="sparse_snr_layer_absolute_snr"`）。判据：ctest `p2_pixel_weight`（量纲/口径一致 + 缺层显式降级 + 误乘帧级标量的可判红反例 + 两条逐像素入口逐像素一致）与 `p2_pixel_weight_wiring`（生产侧接线核对，带 `--self-test`）。
    - **M05 现象**：`idw_power` 在 `eng/packaging/config/**` **零命中**（`grep -rn idw_power eng/packaging/config` = 0），无法直接登记为受控键：`eng/tests/config/check_cfg002_registry.py` 要求 `docs/detail/**` 配置项表与 `config_registry.json#plugin_knobs` **一一对应**（文档有、登记册无 ⇒ 红；登记册有、文档无 ⇒ 红），而 `07_noise_snr.md` 只在正文提到 `idw_power`，**无表格行**；且 `defaults.json` 的 `authority.transcription_rule` 要求取值只从 `docs/science/**` 逐字转录 ⇒ 插件文档来源的默认值不能直接进 defaults 数值面。代码侧现状：消费侧已是规范默认 1.0（`lib/algorithms/drizzle/healpix_drizzle/snr_evaluator.h`、`snr_evaluator.cpp` 两处），生产侧仍硬写 2.0（`lib/algorithms/noise_snr/cpp/src/snr_estimator.cpp` 三处）。
    - **规范依据**：`docs/detail/algorithms_phase1/07_noise_snr.md`（插值设置配置化 + 日志输出 `p*`，不随产物落盘）；`eng/contracts/schemas/unified/sparse_snr_layer.schema.json`（层语义冻结）；`AGENTS.md §6`（不动科学默认值）。
    - **归属/去向**：M06 = **已落地**（集成侧逐像素消费面 + 生产接线 + 双向判据）；M05 = 文档单元（`07_noise_snr.md` 补表格行与现状句订正）+ 算法单元（生产默认值与配置读取面、`p*` 日志）；两条落地后，配置键登记面（`eng/packaging/config/**`）才可加键（否则该键无生产读取面 ⇒ `CHK-CONFIG-CONSUMED` 判死键）。

47. **已由机器台账承载（本节只给交叉引用，不重复登记）**
    - **静默回退**（`module_adapters.cpp#p1_calibrated_path` 与 `#p1_cleaned_input_path` 在声明产物缺失时**不报错、不写 degraded_reason**，直接回退到原帧或上游 cal 产物）→ `eng/contracts/block_flow/conformance_deviations.json#BFD-A17`（blocker，`undeclared_input`）。
    - **phase3 hips 输入端口单位仍为 ADU** → `eng/contracts/block_flow/conformance_deviations.json#BFD-U1`（major，`unit_mismatch`）。
    - **死配置键 `algorithm_rejection_method` / `reject`** → `eng/ci/ledgers/dead_config_keys.json#dead_config_key:algorithm_rejection_method` 与 `#dead_config_key:reject`。
    - **整阶段 Session 仍注册在生产注册表** → `eng/ci/ledgers/registry_ir_parity.json#registered_not_in_ir:acsd.phase2.resample`。
    - **占位注册行 `acsd.phase3.resample`** → `eng/ci/ledgers/registry_ir_parity.json#registered_not_in_ir:acsd.phase3.resample` 与 `eng/contracts/block_flow/conformance_deviations.json#BFD-C1`（同一事实的端口面双登记）。
    - **shell `p3_export.cpp` 未进构建（第 40 条的一部分）** → `eng/ci/ledgers/spec_named_impl_gaps.json#SNI-S4-P3X-06`。
    - **同族已在控制包差距清单登记、本节不另立条目**：命名块内存管线未落地、三阶段无独立调度器、块生命周期未实现、性能探针不成体系、L2 性能门恒真、空断言/恒真测试普查、docs/contracts ↔ eng/contracts 双向对应无机器校验；以及控制包未决问题清单的「端口声明 vs 真实数据流」与 OQ-10 两条。

49. **数据集级覆盖索引 `coverage.index.json` 零生产者（发布路径已指定、未实现）**：该索引的登记名与字段模型已冻结（`docs/engineering/HIPS_STORAGE_FORM_CONTRACT.md` §4.2：`index_schema`/`granularity`/`frames[]`/`blocks[].{ipix,frames[].{f,frac}}`），但**全仓无生产者**——写出发布清单的同一条命令（`mosaic`）尚未在其发布步内写它（`lib/` 侧零写入点；产品级 `<name>.hips.index.json` 同样只有门禁夹具会写临时样例，无生产写入点）。发布路径 = `mosaic` 发布步，写出**运行输出根层** `coverage.index.json`，与产品集清单**同一次原子发布**；该裁决**单判据**裁决、**不设豁免名单**，**不得**以放宽 `IO_003` §4（发布清单必含 `properties`）或把产品集判定改成白名单的方式消解张力。消费面 `coverage_index`（加性可选键）因此仍在死键台账：`eng/ci/ledgers/dead_config_keys.json#dead_config_key:coverage_index`（与本条同一事实的两面登记，双面均须显式、不得静默留白）。**下一动作与判据**：实现该发布路径时须同时给出**正例**（索引存在且同批清单含 `properties`、随清单原子落盘）与**负例**（索引缺失或与清单不同批 ⇒ 判红）并进机器门；在此之前不得声称覆盖索引可用，也不得把「CLI 认识该键」当成「已消费」。
