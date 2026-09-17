# 工程控制 / RELEASE-01 差异审计表（GAP_AUDIT）

> 本表由 AUD-001 产出（SubAgent 只找不改），经 SCI-001 科学裁决合并后定稿。差距类型：`缺口` / `违规` / `过时` / `漂移` / `无主` / `UNRESOLVED`。状态：`OPEN → ASSIGNED → CLOSED`（CLOSED 仅由前台验证后写入）。
>
> **分片报告（全量明细，含 P1/P2 逐条证据）**：`reports/RELEASE-01/audit/AUD-A1-normalize.md`（54 条）、`AUD-A2-mosaic.md`（25 条）、`AUD-A3-export.md`（27 条）、`AUD-A4-infrastructure.md`（41 条）。本表只做**去重后的 P0 汇总 + UNRESOLVED 归并**；P1/P2 见分片报告。

## 1. 差距汇总（P0，去重后 16 条）

| # | 模块/条款 | 差距类型 | 级别 | 文档条款引用 | 现状复现路径（文件/行/命令） | 建议归属 | 状态 |
|---|---|---|---|---|---|---|---|
| P0-01 | 横向：模块门 | 违规 | P0 | ENGINEERING_SPEC §4/§8；docs/ci/01_CHECKS.md §2 | `python3 ci/run_checks.py --check CHK-MODULE-MANIFEST --quiet` → FAIL；`run/ci/module-map/module_map.json` 139 findings / 20 模块 NOT_IMPLEMENTED（A1-52 / A3 X-01 / A4-39） | 负责人已裁决临时红；登记遗留 | OPEN（owner-sanctioned） |
| P0-02 | drizzle / Phase1 产品 | 缺口 | P0 | docs/plugins/algorithms_phase1/08_drizzle.md:19；docs/design/PHASE1_DETAILED_DESIGN.md:132；docs/science/DRIZZLE.md:53-58 | p1 节点只注入 data 块（`module_adapters.cpp:3385-3419`）；`p1_final.json` `n_variance_tiles=0, n_ivar_tiles=0, uncertainty_available=false`；mosaic `weight_mode=2` rc=2 实测（`run/RELEASE-01/e2e/l3/logs/p2_m42_wm2.log`）（A1-45 = PRE-F-01） | 修复任务（接线 + Oracle） | OPEN |
| P0-03 | noise_snr / frame_snr | 违规 + UNRESOLVED | P0 | docs/plugins/algorithms_phase1/07_noise_snr.md:36-63；contracts/schemas/unified/frame_snr.schema.json:5/42/224-231 | 实装把 frame_snr 定义为 5σ 深度对象（`module_adapters.cpp:3177-3183`；`p1_snr.json` 的 `frame_snr.definition="5-sigma point-source depth"`）；且帧级 SNR 未写入 HiPS 头（A1-44 / SCI F1） | 负责人裁决（UNRESOLVED-A）+ 修复 | OPEN |
| P0-04 | calibration：方差传播 | 缺口 + UNRESOLVED | P0 | ENGINEERING_SPEC:26；docs/plugins/algorithms_phase1/01_calibration.md:31；docs/design/PHASE1_DETAILED_DESIGN.md:52-58 | 生产无 V(y)/ivar；`v6_calibration_covariance.cpp` 已实现未接线；文档面 `docs/science/CALIBRATION.md:144` 与插件/设计冲突（A1-01） | 负责人裁决（UNRESOLVED-B）+ 修复 | OPEN |
| P0-05 | cosmetic：恒等 pass | 缺口 | P0 | docs/plugins/algorithms_phase1/02_cosmetic.md:5-6；docs/algorithms/COSMETIC_ALGORITHMS.md:150-156 | `lib/phase1_session/p1_session.cpp:443-446` 传 NULL master → `cosmetic_corrector.cpp:243-250` 检测全跳过；坏点/热像素/宇宙线从未修复（A1-07） | 修复任务 | OPEN |
| P0-06 | cosmetic：方差未更新 | 缺口 | P0 | docs/plugins/algorithms_phase1/02_cosmetic.md:25/52 | `cosmetic_corrector.cpp:161-226` 插值只写 out[i]，无 variance 入/出参（A1-08） | 修复任务 | OPEN |
| P0-07 | noise_snr：未入根构建 | 违规 | P0 | ENGINEERING_SPEC:41 | `CMakeLists.txt:236-241` 注释掉 `add_subdirectory(lib/algorithms/noise_snr)`，理由（untracked）与 `git ls-files` 事实不符（A1-37） | 修复任务 | OPEN |
| P0-08 | sampling：star_mask/sky_samples | 缺口 | P0 | ASTROCS_DESIGN.md:246；docs/plugins/algorithms_phase2/10_sampling.md:5,18,20,42-46；UNIFIED_MODEL.md:46-48 | `grep -rn "sky_sample\|sky_plane\|sky_sample_spacing\|B_ref" lib/ contracts/ config/` 零实现命中；`sampler.h` 只输出 P2ControlObservation/P2ControlNode（A2 SMP-01） | 负责人裁决（UNRESOLVED-C 范围）+ 修复 | OPEN |
| P0-09 | upm：b_k(x) 稀疏天光面 | 违规/缺口 | P0 | ASTROCS_DESIGN.md:213/246；docs/plugins/algorithms_phase2/11_upm.md:26-27,43-51 | 生产为纯加性 `upm.cpp:4-27`（无 g_k）；V6 求解器 b_k 为**帧级标量**（`upm.h:202-211`；`upm.cpp:1793/1818` Jacobian=1.0）；无 B_ref(x)+δ_k(x)（A2 UPM-01） | 负责人裁决（UNRESOLVED-C）+ 修复 | OPEN |
| P0-10 | upm：g_k 乘法未接线 | 违规 | P0 | ASTROCS_DESIGN.md:213；docs/plugins/algorithms_phase2/11_upm.md:26,30 | 生产链 `module_adapters.cpp:20-22/4155/4331` 只走 `p2_upm_build_geo`/`p2_upm_calibrate_block`；g_k 乘法仅存在于未接 CLI 的 `lib/algorithms/integration/v6`（A2 UPM-02） | 修复任务 | OPEN |
| P0-11 | integration：仅加权均值 | 违规/缺口 | P0 | ASTROCS_DESIGN.md:205；docs/plugins/algorithms_phase2/13_integration.md:5,18-28,42-59 | `integrate.cpp:19-79` 仅 `signal=Σwv/Σw`；`integrate.h:5-11` 无 variance/covariance 目标；GLS/Q-W/psfsw 仅在未接线 v6（A2 INT-01） | 修复任务 | OPEN |
| P0-12 | integration：sparse_snr_layer 缺 | 缺口 | P0 | ASTROCS_DESIGN.md:225-240；docs/plugins/algorithms_phase2/13_integration.md:18-22,32-38 | `grep -rn "sparse_snr_layer" lib/` → NO_MATCH_IN_LIB（A2 INT-02） | 修复任务 | OPEN |
| P0-13 | integration：weight_mode 词表 | 违规 | P0 | docs/contracts/v6/frozen/02_WEIGHT_MODE_VOCABULARY.md:9-18；docs/plugins/algorithms_phase2/13_integration.md:65 | 生产 CLI/session 用**被取代的 legacy 整数**且默认 2：`session_commands.h:133 {"weight_mode","2"}`；`canonical algorithm_weight_mode` 在 lib/ 零消费（A2 INT-03） | 负责人裁决（UNRESOLVED-D）+ 修复 | OPEN |
| P0-14 | CLI：export 配置格式互斥 | 违规 + UNRESOLVED | P0 | contracts/schemas/phase_config_export.schema.json；config/templates/export.phase_config.json；config_registry「唯一事实源」 | 官方模板被 CLI 拒绝 rc=2；CLI 实际接受 `source.hips_dir/center/scale_deg_per_px`（A3 CLI-04） | 负责人裁决（UNRESOLVED-E）+ 修复 | OPEN |
| P0-15 | 版本纪律 | 违规 + UNRESOLVED | P0 | ASTROCS_DESIGN §12；ACCEPTANCE_SPEC:158 | `./build/astrocs --version` = `0.11.0-alpha.2+g41b41e2d…`；VERSION/module.yaml/CI 同（A3 CLI-03 / DOC-002 版本四口径） | 负责人裁决（UNRESOLVED-F，FIN-001 前置） | OPEN |
| P0-16 | 文档包 ↔ 仓库冲突（DOC-001 发现） | 违规（文档） | P0 | ENGINEERING_SPEC §7；docs/ci/01_CHECKS.md §2 | `ENG-CONSTRAINTS` 红（§7 删 `ACCEPTANCE_SPEC.md`）；`CHK-REGISTRY-DOC-SYNC` 红（§2 删 `CHK-E2E-REPRO`/`CHK-EXIT-CONSISTENCY` 两行；两者由前批 agent 于 `5eb1433f`/`bd300e85` 引入） | 负责人裁决（UNRESOLVED-1/2） | OPEN |

## 2. 按模块覆盖审计（结论索引）

| 面 | 分片 | 模块数 | 差距（P0/P1/P2/无差距） | UNRESOLVED | 报告 |
|---|---|---|---|---|---|
| normalize | A1 | 8 | 7 / 31 / 10 / 6 | 10 | `reports/RELEASE-01/audit/AUD-A1-normalize.md` |
| mosaic | A2 | 5 | 6 / 17 / 1 / 0 | 1 | `reports/RELEASE-01/audit/AUD-A2-mosaic.md` |
| export + CLI | A3 | 3 | 3 / 15 / 9 / 0 | 4 | `reports/RELEASE-01/audit/AUD-A3-export.md` |
| infrastructure | A4 | 7 | 0 / 24 / 15 / 2 | 3 | `reports/RELEASE-01/audit/AUD-A4-infrastructure.md` |

**A4 无差距（可放心）**：config/ 分离成立（无零点键、defaults.json 无硬件旋钮、filters.json sha256 实测一致）；14 个 canonical schema 齐全、tests/contracts 63 测试通过；G-RES-01 判据表与 `contracts/resource_gate_v1.json` 逐条一致，C++ 经 `configure_file` 生成阈值头引契约常量。

**A1 无差距（6 条）**、**A3 重点判定**：投影 registry v3 实有 TAN/SIN/CAR/AIT（4/8），插件文档 `14_projection.md §4` **正确**；`export --help` 文案「当前唯一实现 TAN」**错误**（`session_commands.h:150`；应为「alpha 会话仅接受 TAN」，`p3_session.cpp:96-97`）。

## 3. 横向条款审计

| 条款 | 审计结论 | 差距号 |
|---|---|---|
| CLI 契约（help/--json/-y/-force/预检三级） | `-y`/`-force` 实测有效；**预检只实现 correct/error 两级，橘色 optimize 无产生点**（A4-04） | P1 |
| config/ 分离 | 成立（A4-35 无差距） | — |
| 契约 schema（14 canonical） | 齐全；但插件文档 18 处 `contracts/schemas/<名>.schema.json` 悬空（A1-53 / DOC-002） | P1 |
| CI 检查器 | CHK-MODULE-MANIFEST 红（P0-01）；CHK-REGISTRY-DOC-SYNC 红（P0-16）；其余复跑绿 | P0 |
| 版本纪律 | **不对应**：`--version` 输出 `0.11.0-alpha.2+git` | P0-15 |
| 编排/缓存/流式（新文档包新增） | locality-aware 编排、cache_budget、调度指标、Gaia 瓦片键/两级缓存/查询合并**全缺**（A4-09/10/13/21..24） | P1（若按新文档定级则 P0） |

## 4. 合并裁决（AUD + SCI）

- **P0 最终清单**：P0-01..P0-16（见 §1）。其中 **P0-02/03/05/06/07/11/12/13** 属"文档要求而实现未达"的**代码侧缺口**；**P0-08/09/10** 属新文档包新增语义（稀疏天光面、g_k 乘法）未实现；**P0-14/15/16** 属**文档 ↔ 仓库口径冲突**。
- **P1 最终清单**：A1 31 条 + A2 17 条 + A3 15 条 + A4 24 条 = 87 条（明细见分片报告，逐条含文件:行与复现命令）。
- **P2/UNRESOLVED**：P2 共 35 条（A1 10 / A2 1 / A3 9 / A4 15）；UNRESOLVED 见 §5。
- **关键范围裁决（须负责人裁定）**：新文档包引入了实现中不存在的语义（稀疏天光面 `sky_samples/sky_plane/star_mask`、locality-aware 编排与流式内存、Gaia 两级缓存与查询合并、GLS/Q-W/psfsw 三目标、variance/ivar 链），而 `00_README.md §2` 明确本轮"**不引入新功能**、仅发布准备相关验证与必要修正"。二者互斥：要么这些属发布阻断项（则当前实现不可发布），要么属下一版本路线图（则 L2/L3 中依赖它们的验收条目不能按新文档判绿）。**该裁决决定 RELEASE-01 的结论走向。**

## 5. UNRESOLVED（须负责人裁决；去重后按主题归并）

| 主题 | 内容 | 来源 |
|---|---|---|
| U-A `frame_snr` 规范语义 | 文档（07 §4.1 / UNIFIED_MODEL:42 / ASTROCS_DESIGN:235）= 未加权原始 SNR、写 HiPS 头、w=SNR²/F_ref²；`CONTROL_WEIGHT_SNR.md:10-14`= 相对质量权重中位数；实装 = 5σ 深度对象。三方冲突 | A1-44 / SCI-S1 F1 |
| U-B 校准方差传播 | `docs/science/CALIBRATION.md:144`「不传播母版方差」vs 插件 01:31 + PHASE1_DESIGN:52-58「必须物理传播」 | A1-01 |
| U-C 天光面语义 | 新插件/设计要求 g·s+b(x)+sky_samples；`docs/science/PHASE2_UPM.md:7-8/34/48-50/153`（FROZEN）定义纯加性 C_f(p) 且"无 WCS/天球参与" | A2 X-03 |
| U-D weight_mode 词表 | 冻结词表（字符串枚举、legacy 整数被取代、0 必拒）vs 生产 CLI 默认整数 2 | A2 INT-03 |
| U-E export 配置格式 | 正式 schema/模板 vs CLI 实际接受格式互斥（官方模板 rc=2） | A3 CLI-04 |
| U-F 版本口径 | `0.1alpha`（最高设计 §12/ACCEPTANCE/CI 文档）↔ `0.11.0-alpha.2`（VERSION/checks.json/product.json）↔ `0.0.1alpha`（RELEASE-01 00_README/FIN-001）；且 §12 要求"Alpha 前无版本信息" | A3 CLI-03 / DOC-002 U1 / DOC-001 UNRESOLVED-3 |
| U-G 文档包 §7 / §2 | §7 删 `ACCEPTANCE_SPEC.md`；§2 删两个已注册检查行（均为前批 agent 引入） | DOC-001 UNRESOLVED-1/2 |
| U-H 科学表述订正 | PSFSNR 公式（应为 c3·(Σf)²/(c4σ_n²)）、PSFSW 实现与式[16] 指数/N 语义差异、"帧级 SNR 不随天光漂移"为假、`psf_snr_power` DEFERRED、N*_Sn 常数冲突、PI Moffat √2、付印版式号 | SCI-S1 F2/F3/F4/F5/U3/U4/U5 |
| U-I runtime/scheduler 命名 | 插件文档/00_INDEX 写 runtime；代码目录与 §7/AGENTS §6 为 scheduler；最高设计 §7.1/§7.2 自身两口径 | DOC-002 U5 / A4 U-2 |
| U-J 其他登记面 | 退出码唯一源 `include/astrocs/exit_codes.h` 不存在（实际 `lib/infrastructure/cli/exit_codes.h`）；CLI 可执行名 `acsd_cli` vs `astrocs`；18 处 schema 悬空；PHASE1-3 头部元信息块与 `doc_symbol_namespaces.json` 锚耦合；`docs/archive/**` 是否清理；ACR dormant 双检查器冲突；run 产物 NOT_IMPLEMENTED 状态 | DOC-002 U2/U3/U4/U6/U7/U8 / A4 U-1/U-3 |
| U-K 采样核词表 | 插件 `nearest|bilinear` vs V6 kernel registry；生产默认核不一致 | A3 U4 |

## 6. 附录：DOC-001 一致性基线

| # | 硬约束（文档条款） | 代码现状（可复现证据） | 对应结论 |
|---|---|---|---|
| B-1 | 三命令独立、阶段间只通过磁盘产品+manifest+哈希交换（ASTROCS_DESIGN §2/§3） | `lib/infrastructure/cli/command_tree.h:75-82`；`./build/astrocs help` 实测三条目 | **对应** |
| B-2 | ACR dormant、旧路径无生产入口（§8） | `python3 tools/check_legacy_exit.py` → `LEGACY_EXIT_PASS … ACR dormant 隔离 (prod_sources=270)` rc=0 | **对应** |
| B-3 | 投影首批冻结 8 种（§5.3） | `p3_proj_v6.cpp:348-349` 冻结 8 名；`:272-281` 实有 4（TAN/SIN/CAR/AIT） | **部分对应**（CLI 文案漂移 → P1） |
| B-4 | Alpha 前产物不含版本信息；发布时 `--version` = `0.1alpha`（§12） | `./build/astrocs --version` → `0.11.0-alpha.2+g41b41e2d…` | **不对应** → P0-15 |
| B-5 | 单一资源调度器与线程预算源（§8） | OpenMP 22 处；`tools/arch/check_thread_budget.py` 自述 scheduler per-run 池与 executor 共享池并存 → 同 run 上界≈2×budget | **不对应** → P1（A4-12） |
| B-6 | 数据对象按 UNIFIED_MODEL 区分（frame_snr/sky_samples/sky_plane） | `grep -rIl sky_plane lib/ contracts/ config/` → 0 | **不对应** → P0-08/09 |

## 7. 第二轮补充（PERF-001 / TST-001 / SCI-001-S2 增量，前台已复核）

### 7.1 新增 P0（2 条，去重后编号续 §1）

| # | 模块/条款 | 差距类型 | 级别 | 文档条款引用 | 现状复现路径 | 建议归属 | 状态 |
|---|---|---|---|---|---|---|---|
| P0-17 | 性能：G-RES-01 enforce | 违规 | P0 | ACCEPTANCE_SPEC L2（G-RES-01 判据①②③ enforce）；ASTROCS_DESIGN §8 | PERF-001 复算：判据③ **4/4 normalize 违约**、判据① **3/4 mosaic 违约**；mosaic 16 核配额下利用率 **5.6–6.4%**、active_compute_threads p50=1–2、io_wait 峰值 **94.47%**；Phase1 峰值 RSS 4.1–8.6 GB、稳健斜率 **45.1–77.5 MiB/s**、内存回收率 0.000–0.365（<0.5） | 修复任务（编排/并行/流式内存） | OPEN |
| P0-18 | 测试：负例面（polarity） | 缺口 | P0 | ENGINEERING_SPEC §8「负例注入（能红能绿）」；docs/ci/01_CHECKS.md §1 | TST-001：46 个注册检查中**仅 11 项自带可执行负例**；19 项 FACE-DEFINED-NOT-RUN 的负例是人工中文说明（非机器可执行）；1 项（CHK-E2E-REPRO）无 polarity 记录；9 项 PROVEN 借壳 `check_prod_reachability.py --selftest` | 测试面补强 | OPEN |

### 7.2 新增 P1（并入 §4 的 87 条，逐条证据见分片报告）

| 来源 | 项 | 证据 |
|---|---|---|
| TST-001 | 6 个 C++ 测试为空/恒真断言（在 442 内） | `p2_upm_synthetic_test.cpp:125`、`p2_rejection_test.cpp:89`、`p2_output_semantics_test.cpp:136` 等 |
| TST-001 | 3 个测试未链产品库（"假测试"） | `p2_upm_synthetic`/`p3_coverage`/`p2_ir_facade` |
| TST-001 | 12 个跳过用例中 **6 个因硬编码 Windows 绝对路径**在 Linux 零执行 | `synthetic_gate.cpp:3584/3620/3633/3683/3833/3943`、`sampler_parallel_consistency_test.cpp:33`（`F:/Astro dev/...`） |
| TST-001 | Oracle 假绿：缺 numpy 时 exit 0 | `noise_model_numpy_oracle.py` |
| TST-001 | 测试索引陈旧；`aio` 8 枚 / `gaia` 2 枚 / `hips_browser` 全部测试零 CMake 注册 | `tests/test_index.csv` |
| TST-001 | 文档引用的 `contracts/schemas/events.schema.json` 不存在 | `docs/plugins/infrastructure/21_observability.md:17` |
| TST-001 | `fits_output` 的 `band_height`/`tile_cache_mb` 在 lib/include 零消费；`p3_output.cpp:235` long 溢出风险且无 >2GiB 测试 | `p3_output.cpp:235` |
| PERF-001 | 4 项编排/缓存观测**完全无记录点**：worker 空转率（`worker_balance` 恒 50.00）、上下文切换/块重载（ctx_switches 采样后被 CSV 丢弃）、跨 worker 数据搬运量、同组 Gaia 外部请求计数；2 项部分（缓存命中率、峰值 RSS–块大小关系） | `reports/RELEASE-01/perf/PERF-001-timing.md` |
| PERF-001 | normalize 进程墙钟 **22–35%** 未被日志/资源监测覆盖；mosaic 无 DAG trace；export 无阶段日志 | 同上 |
| SCI-001-S2 | `DRIZZLE.md` §5 归一化 `D=Σa_jp` 使常数面亮度 `S_p=B0/pixfrac²`，与 §7 不变量 `S=B0` 互斥（pf=0.8 偏 1.5625×）——**P0 级科学文档冲突** | `drizzle_engine.cpp:1329/1531/1554` + 独立代数；已登记 M2a-A-1 |
| SCI-001-S2 | `ASTROMETRY.md` §5a「0-based/常量 1px 平移」与 Paper I §2.1.1/§2.1.4 及实现矛盾（同节 :80 自相矛盾）——**P0 级** | `ipv_wcs.cpp:944-945`；finding WCS-003-F1 未裁决 |
| SCI-001-S2 | P1 群：插件校准方差重复计同一 bias 母版；PSF trimmed-mean→σ 常数应为 `0.7316730952806139`；孔径 `flux_error` 漏 gain 与背景估计项；`hips_frame` 值域引反；`UNCERTAINTY` §Phase3 口径过时（实为完整 `R C Rᵀ`）且 36.3% 与 `1+0.75ρ` 不自洽；插件 UPM 承诺已撤销的 `g_k·s+b_k` | `reports/RELEASE-01/science/SCI-S2-topics.md` §3 |

### 7.3 本轮已闭合（1 条）

| # | 缺陷 | 发现 | 处置 | 复验 |
|---|---|---|---|---|
| F-02 | 测试代码**写死机器绝对路径** `F:/Astro dev/Astro CS Normalization Database/run/temp/phase1_freeze`（违反 AGENTS §3「不得写死服务器绝对路径」），导致 6 个真实 HiPS 用例在 Linux 零执行 | TST-001 | 前台修复：新增 `phase1_fixture_root()`/`phase1_tmp_root()`，优先读环境变量 `ASTROCS_PHASE1_FREEZE_DIR`/`ASTROCS_PHASE1_TMP_DIR`，默认仓库相对 `run/temp/phase1_freeze`；skip 消息改为可执行的提供方式；去掉 `F:/definitely/not/a/hips` 这类平台相关坏路径 | ① `grep -rn 'F:/Astro dev' lib/algorithms/coverage/tests/*.cpp` → NONE；② 负例注入：`ASTROCS_PHASE1_FREEZE_DIR=<fake>` 下 `RealHipsUnion` **不再 SKIP 而是执行并 FAIL** ⇒ 环境变量确被读取；③ `ctest -R 'phase2_synthetic_gate|phase2_sampler_parallel'` → **101/101 通过，0 失败**（`phase2_synthetic_gate` 99 用例：89 通过 / 10 跳过 / 0 失败） |
| F-01 | `ci/verify_toolchain.py` **fail-open**：lock/policy 缺件或不可解析时打印 `[FAIL]` 但 `exit 0`（违反 ENGINEERING_SPEC §8） | TST-001 | 前台修复：`if __name__ == "__main__": sys.exit(main())` | 缺件 → `rc=1`（订正前 rc=0）；CHK-ENV-ADOPTION 仍 PASS；负例锁定 `tests/quality/test_env_adoption_negative.py`（10 passed） |

### 7.4 另记（非差距，但影响判读）

- **文档↔仓库口径冲突**：DOC-002 查出 7 项代码不一致 + `config_registry.json` 95 条行锚中 23 条失配（`07_noise_snr` +27、`10_sampling` +33、`11_upm` +38 且 `bkg_model_order` 字段在文档中不存在、`19_runtime` +34、`22_gaia` +22~25），行锚漂移面属 `config/`+`contracts/` 只读域，待分派。
- **运行产物面**：run-trace / artifact-manifest / run-summary / run-graph 四类输出在文档中要求但实现为 NOT_IMPLEMENTED（AUD-A4 U-1）。
- **fixture 面（F-02 后续，前台已查证）**：6 个真实 HiPS 用例现在**可通过环境变量提供 fixture 而无需改代码**，但**当前管线无法自行生成该 fixture**：Phase1 不产出 `snr` 子产品（`lib/phase1_session/p1_session.cpp:493` 将 `noise_snr` 标为 `unavailable`；实测 `p1_final.json` `products=["signal","support"]`），而 `Phase2Sampler.RealHipsControlSampling`/`G6LocalSnrAvailabilityThreeZones` 断言 `snr_used>0`；HiPS writer 支持的产品集为 `{signal,support,snr,variance,ivar}`（`lib/algorithms/drizzle/hips/src/module_entry.cpp:1061`）。⇒ **该 fixture 无法在修复 P0-03（帧级 SNR/不确定度产品链）之前重建**；两处断言与 P0-03 同源，不得以"补 fixture"绕过。
- **环境面（影响复现，非产品缺陷）**：本轮构建/链接期间 `/tmp` 不可写（`Cannot create temporary file in /tmp/`），前台改用工作区内 `TMPDIR=<repo>/run/tmp` 完成构建；复跑者若遇同样报错，请先确认 `/tmp` 可写或显式设置 `TMPDIR`。
