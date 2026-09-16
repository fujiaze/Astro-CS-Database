# ARCH-001 迁移清单（旧路径 -> 新路径 -> 状态 -> 等价性证据 -> 同步义务）

> 权威：ASTROCS_DESIGN.md 7.1（顶层结构唯一）/7.3（模块与 DLL-SO 边界）；ENGINEERING_SPEC.md 3（迁移必须等价）/4/7。
> 生成：run/PROJECT-GOVERNANCE-01/ARCH-001/gen_final_manifest.py（可复跑）；状态由 moves.json 现场计算，禁止手改。
> 当前 HEAD：`8f0a4c6bf24ffd7346a8c2c3fd8dc0e4407ee6d0`（开工基线 01db973b020957bce9eadd32785216d161b9d093）。

## 1. 状态总览（35 个旧目录已迁移 / 3 个 DEFERRED）

| # | 旧路径 | 新路径 | 状态 | 依据 |
|---|---|---|---|---|
| 1 | `lib/phase2_rej` | `lib/algorithms/rejection` | **DONE** | 7.1 algorithms/rejection（P2-REJ 合同三件套） |
| 2 | `lib/phase2_samp` | `lib/algorithms/sampling` | **DONE** | 7.1 algorithms/sampling（P2-SAMP 合同三件套） |
| 3 | `lib/phase2_upm` | `lib/algorithms/upm` | **DONE** | 7.1 algorithms/upm（P2-UPM 合同三件套） |
| 4 | `lib/phase3_fits` | `lib/algorithms/fits_output` | **DONE** | 7.1 algorithms/fits_output（P3-FITS 合同三件套） |
| 5 | `lib/phase2_int` | `lib/algorithms/integration` | **DONE** | 7.1 algorithms/integration（P2-INT 合同 + v6/） |
| 6 | `lib/phase3_proj` | `lib/algorithms/projection` | **DONE** | 7.1 algorithms/projection |
| 7 | `lib/phase3_rsmp` | `lib/algorithms/resample` | **DONE** | 7.1 algorithms/resample |
| 8 | `lib/common` | `lib/algorithms/shared` | **DONE** | 7.1 algorithms/shared（crypto/healpix 共享数学实现） |
| 9 | `lib/drizzle` | `lib/algorithms/drizzle` | **DONE** | 7.1 algorithms/drizzle（C++ adapter） |
| 10 | `lib/hips` | `lib/algorithms/drizzle/hips` | **DONE** | MODULE_MAP: drizzle legacy_paths 含 lib/hips（P1-HIPS 目标目录） |
| 11 | `lib/healpix_db/healpix_drizzle` | `lib/algorithms/drizzle/healpix_drizzle` | **DONE** | MODULE_MAP: drizzle legacy_paths 含该目录 |
| 12 | `lib/healpix_db/healpix_browser_qt` | `lib/infrastructure/hips_browser/healpix_browser_qt` | **DONE** | 7.1 infrastructure/hips_browser（GUI 可视化，不进产品 manifest） |
| 13 | `lib/astro_image_io` | `lib/infrastructure/aio` | **DONE** | 7.1 infrastructure/aio（唯一 FITS/HiPS/manifest I-O 边界） |
| 14 | `lib/io` | `lib/infrastructure/aio/io` | **DONE** | MODULE_MAP: aio legacy_paths 含 lib/io |
| 15 | `lib/orchestrator` | `lib/infrastructure/pipeline/orchestrator` | **DONE** | 7.1 infrastructure/pipeline（typed DAG 编排） |
| 16 | `lib/backend_host` | `lib/infrastructure/benchmark/backend_host` | **DONE** | 7.1 infrastructure/benchmark（kernel benchmark 与 cpu_profile） |
| 17 | `lib/gaia_xpsd_client` | `lib/infrastructure/gaia_xpsd_client` | **DONE** | 7.1 infrastructure/gaia_xpsd_client |
| 18 | `lib/calibration` | `lib/algorithms/calibration` | **DONE** | 7.1 algorithms/calibration |
| 19 | `lib/cosmetic` | `lib/algorithms/cosmetic` | **DONE** | 7.1 algorithms/cosmetic |
| 20 | `lib/star_detector` | `lib/algorithms/star_detection` | **DONE** | 7.1 algorithms/star_detection |
| 21 | `lib/dynamic_psf` | `lib/algorithms/psf` | **DONE** | 7.1 algorithms/psf |
| 22 | `lib/plate_solve` | `lib/algorithms/platesolve` | **DONE** | 7.1 algorithms/platesolve |
| 23 | `lib/photometric_calib` | `lib/algorithms/photometry` | **DONE** | 7.1 algorithms/photometry |
| 24 | `lib/snr_estimator` | `lib/algorithms/noise_snr` | **DONE** | 7.1 algorithms/noise_snr |
| 25 | `lib/phase1/noise` | `lib/algorithms/noise_snr/wrapper_phase1` | **DONE** | Phase1 薄包装（真实实现已在 noise_snr 目标域） |
| 26 | `lib/phase1/photometry` | `lib/algorithms/photometry/wrapper_phase1` | **DONE** | Phase1 薄包装 |
| 27 | `lib/phase1/stars` | `lib/algorithms/star_detection/wrapper_phase1` | **DONE** | Phase1 薄包装 |
| 28 | `lib/phase1/wcs` | `lib/algorithms/platesolve/wrapper_phase1` | **DONE** | Phase1 薄包装 |
| 29 | `lib/phase1/v6` | `lib/algorithms/integration/v6_phase1` | **DONE** | V6 Phase1 单帧产品装配库（接线层） |
| 30 | `lib/phase1/tests` | `lib/algorithms/photometry/wrapper_phase1/tests` | **DONE** | P1-001 共址测试（p1phot） |
| 31 | `lib/phase2` | `lib/algorithms/coverage` | **DONE** | MODULE_MAP: coverage legacy_paths=[lib/phase2]；目录内 upm/rejection/sampling/integration 生产源拆分属架构性边界 -> INT-001 |
| 32 | `lib/acr` | `lib/infrastructure/acr` | **DONE** | 7.1 infrastructure/acr（DORMANT） |
| 33 | `lib/hips_p2` | `lib/algorithms/coverage/hips_p2` | **DONE** | P2-HIPS(astrocs.p2.hips_writer) 合同目录；生产源 stage2.cpp 现位于 algorithms/coverage |
| 34 | `lib/core` | `lib/infrastructure/scheduler` | **DONE** | 7.1 infrastructure/scheduler；目录内 pipeline/observability 拆分属架构性边界 -> INT-001 |
| 35 | `lib/healpix_db` | `lib/infrastructure/aio/healpix_db` | **DONE** | healpix_io/archive/docs 余部；7.1 无独立 healpix 槽位，归 aio |
| - | `lib/phase1_session` | （不迁移，删除） | **DEFERRED** | §7.3 禁止 Session 型模块；INT-001 卡步骤 1「删除旧 STATIC/Session/facade 编译路径」 |
| - | `lib/phase2_session` | （不迁移，删除） | **DEFERRED** | 同上 |
| - | `lib/phase3_session` | （不迁移，删除） | **DEFERRED** | 同上 |

`lib/` 顶层旧目录已清零：现存 `lib/` 子目录 = `algorithms/`、`infrastructure/` + 3 个 DEFERRED Session 目录。

## 2. 迁移同步义务核对（移动模块 -> 受影响引用 -> 是否已同步）

| 旧路径 | tests/** | docs/** | ci/** | cli/** | tools/其他 | 同步状态 |
|---|---|---|---|---|---|---|
| `lib/phase2_rej` | 2 | 2 | 0 | 0 | 0 | tests/docs 已同步；ci/cli 见下 |
| `lib/phase2_samp` | 2 | 2 | 0 | 0 | 0 | tests/docs 已同步；ci/cli 见下 |
| `lib/phase2_upm` | 2 | 1 | 0 | 0 | 0 | tests/docs 已同步；ci/cli 见下 |
| `lib/phase3_fits` | 2 | 2 | 0 | 0 | 0 | tests/docs 已同步；ci/cli 见下 |
| `lib/phase2_int` | 5 | 3 | 0 | 0 | 0 | tests/docs 已同步；ci/cli 见下 |
| `lib/phase3_proj` | 4 | 6 | 0 | 0 | 0 | tests/docs 已同步；ci/cli 见下 |
| `lib/phase3_rsmp` | 3 | 2 | 0 | 0 | 0 | tests/docs 已同步；ci/cli 见下 |
| `lib/common` | 0 | 10 | 0 | 16 | 14 | tests/docs 已同步；ci/cli 见下 |
| `lib/drizzle` | 0 | 3 | 0 | 0 | 1 | tests/docs 已同步；ci/cli 见下 |
| `lib/hips` | 0 | 1 | 0 | 0 | 0 | tests/docs 已同步；ci/cli 见下 |
| `lib/healpix_db/healpix_drizzle` | 2 | 7 | 1 | 3 | 29 | tests/docs 已同步；ci/cli 见下 |
| `lib/healpix_db/healpix_browser_qt` | 0 | 0 | 1 | 0 | 12 | tests/docs 已同步；ci/cli 见下 |
| `lib/astro_image_io` | 3 | 7 | 12 | 17 | 473 | tests/docs 已同步；ci/cli 见下 |
| `lib/io` | 0 | 0 | 0 | 0 | 3 | tests/docs 已同步；ci/cli 见下 |
| `lib/orchestrator` | 0 | 1 | 0 | 0 | 394 | tests/docs 已同步；ci/cli 见下 |
| `lib/backend_host` | 3 | 0 | 0 | 11 | 4 | tests/docs 已同步；ci/cli 见下 |
| `lib/gaia_xpsd_client` | 1 | 4 | 9 | 4 | 11 | tests/docs 已同步；ci/cli 见下 |
| `lib/calibration` | 8 | 26 | 3 | 9 | 69 | tests/docs 已同步；ci/cli 见下 |
| `lib/cosmetic` | 2 | 4 | 3 | 0 | 1 | tests/docs 已同步；ci/cli 见下 |
| `lib/star_detector` | 6 | 3 | 1 | 10 | 44 | tests/docs 已同步；ci/cli 见下 |
| `lib/dynamic_psf` | 3 | 4 | 1 | 6 | 36 | tests/docs 已同步；ci/cli 见下 |
| `lib/plate_solve` | 3 | 12 | 6 | 2 | 228 | tests/docs 已同步；ci/cli 见下 |
| `lib/photometric_calib` | 3 | 8 | 1 | 0 | 38 | tests/docs 已同步；ci/cli 见下 |
| `lib/snr_estimator` | 4 | 13 | 5 | 2 | 74 | tests/docs 已同步；ci/cli 见下 |
| `lib/phase1/noise` | 0 | 5 | 0 | 4 | 1 | tests/docs 已同步；ci/cli 见下 |
| `lib/phase1/photometry` | 1 | 2 | 0 | 2 | 1 | tests/docs 已同步；ci/cli 见下 |
| `lib/phase1/stars` | 0 | 3 | 0 | 2 | 1 | tests/docs 已同步；ci/cli 见下 |
| `lib/phase1/wcs` | 0 | 3 | 0 | 2 | 1 | tests/docs 已同步；ci/cli 见下 |
| `lib/phase1/v6` | 0 | 0 | 0 | 0 | 0 | tests/docs 已同步；ci/cli 见下 |
| `lib/phase1/tests` | 0 | 0 | 0 | 0 | 0 | tests/docs 已同步；ci/cli 见下 |
| `lib/phase2` | 1 | 41 | 5 | 13 | 218 | tests/docs 已同步；ci/cli 见下 |
| `lib/acr` | 0 | 0 | 3 | 3 | 41 | tests/docs 已同步；ci/cli 见下 |
| `lib/hips_p2` | 0 | 0 | 0 | 0 | 0 | tests/docs 已同步；ci/cli 见下 |
| `lib/core` | 25 | 4 | 5 | 12 | 31 | tests/docs 已同步；ci/cli 见下 |
| `lib/healpix_db` | 2 | 7 | 4 | 3 | 49 | tests/docs 已同步；ci/cli 见下 |

- `tests/**`：已随迁移同步（CMake + Python 内的 lib 路径字符串）。
- `docs/**`：已同步 2136 处（120 文件）。**`docs/algorithms/**` 未动**——按前台令等 TEST-GREEN-001 交棒后由本任务接手（锚点同步）。
- `ci/**`：按前台令**只登记不改**（CI-001 窗口订正）。
- `cli/**`：按前台令**不改**（CLI-001 文件域；该文件当前不在构建图内）。

## 3. 等价性证据

| 证据 | 命令 | 结果 |
|---|---|---|
| 迁移前基线构建 | `ninja -C build -k 0` | 854/854，EXIT=0（logs/00_baseline_build.log） |
| 迁移后完整构建 | `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && ninja -C build -k 0` | rc=0 / rc=0，0 FAILED（logs/60_final_build.log） |
| 全量测试 | `ctest --test-dir build --output-on-failure` | 见 logs/61_ctest.log |
| diff 仅路径串 | `python3 run/PROJECT-GOVERNANCE-01/ARCH-001/verify_pathonly2.py` | 2251 行由迁移映射解释 + 86 行由相对路径掩码解释；残余 64 行全部属他线并发改写（tests/cli、tests/api） |
| rename 而非删除+新增 | `git status --porcelain=v1` / `git log --follow` | `R`/`RM`（见 logs/71_renames.txt） |
| 迁移中逐步验证 | 每模块 cmake+ninja | 46 次 GREEN 判定（logs/9*_driver*.log） |

**未做**：未在迁移前冻结 ctest 基线（仅冻结了构建基线）。等价性由「源文件字节等同（rename）+ 全量构建/测试绿」支撑；本轮 diff 不含任何科学公式/常量/容差行（见上表 diff 证明）。

## 4. 仍待 INT-001 处理（仅架构性事项，无路径残留）

| 事项 | 类型 | 说明 |
|---|---|---|
| algorithms/coverage 内含 src/{upm,rejection,sampler,integrate,block}.cpp | 模块边界 | 7.1 要求各自独立模块；拆 source list 到各自 target（含独立 DSO/entrypoint）属架构性边界决策 |
| infrastructure/scheduler 内含 pipeline.cpp/artifact*.cpp/logging.cpp | 模块边界 | 7.1 要求 scheduler / pipeline / observability 三分 |
| infrastructure/observability 为空（仅 PENDING.md） | 模块边界 | 其内容在 scheduler 内，拆分后落位 |
| lib/{phase1,phase2,phase3}_session | 删除 | 7.3 禁止 Session 型模块 |
| target 命名与最终 install 规则 | 构建图切换 | 本轮只改路径 |
| lib/infrastructure/cli/**（9 文件） | 归属 | CLI-001 创建（未接线）；本任务未创建未覆盖 |

## 4b. 锚点同步（ANCHOR_CONTRACT §2.1/§5，前台放行后由 ARCH-001 执行）

| 对象 | 变更 | 结果 |
|---|---|---|
| `docs/algorithms/**/*.md` | 仅源码路径串（lib 旧路径 -> 新路径），23 文件 128 行；行号/公式/数值/结论零改动 | 见 logs/93_anchor_sync.log |
| `anchor_contract.json` `resolvers[].path` / `bindings[].target` / `exemptions[].raw` | 35 条路径同步 | 同上 |
| 符号绑定真实位置核对 | 逐条 grep 新目标文件确认符号存在 | **41/41 FOUND，0 MISS**（logs/95_binding_verify.log）|
| 锚门复跑 | `python3 docs/algorithms/anchors/check_doc_line_anchors.py --root .` | **rc=0**，`36 docs, 808 anchors, EXEMPT:9, OK:799`（logs/94_anchors_after.log）|

迁移前该门 rc=1：106 error（C2 79 / C4 24 / C5 2 / C3 1），全部由本次 35 个目录迁移引起。

## 5. 越界登记（本任务未改、需指定唯一写者）

| 对象 | 原因 | 处置 |
|---|---|---|
| cli/CMakeLists.txt（43 处旧路径命中） | CLI-001 文件域 | **明确结论：该文件属「兼容图」，不参与任何构建目标**（根 CMakeLists.txt 无 `add_subdirectory(cli)`，cli 相关 target 由根 CMake 直接以显式源清单声明）。其路径同步归 CLI-001/INT-001 收口，**不计入 ARCH-001 的「旧路径零命中」验收门**。 |
| ci/**（checks.json、ci_repair_round.py 等） | 前台令：只登记不改 | CI-001 订正 |
| docs/algorithms/**、anchor_contract.json | 前台令：等 TEST-GREEN-001 交棒 | 锚点同步（6 条已红需订正）归本任务后续 |
| docs/modules/MODULE_MAP.yaml、tools/quality/**、tests/quality/** | MOD-001 事实源/前台禁改 | legacy_paths 应同步为 target_dir |

