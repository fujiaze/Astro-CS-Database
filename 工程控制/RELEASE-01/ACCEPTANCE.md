# 工程控制 / RELEASE-01 验收记录（ACCEPTANCE）

> PASS 仅由前台独立验证后写入；证据为前台**独立复跑**结果，不复用 SubAgent 自述。

## 1. 总表

| 任务 | 状态 | 机器门结果 | 证据路径 | 前台结论 |
|---|---|---|---|---|
| DOC-001 | **PASS** | 文档包核验器 PASS；受影响检查 8/10 → 现 8/12（新增 DOC-INDEX/SCI-REF 已修绿，2 红为文档包↔仓库冲突） | `工程控制/RELEASE-01/DOC_PACK_MANIFEST.json`、`run/RELEASE-01/logs/docs/` | 36 篇替换到位、治理痕迹清零、引用可达、一致性基线登记 |
| AUD-001 | **PASS** | 分片复跑命令见各报告 §4 | `工程控制/RELEASE-01/GAP_AUDIT.md` + `reports/RELEASE-01/audit/*.md` | 23 模块 + 横向全覆盖；P0=16、P1=87、P2=35、UNRESOLVED=11 主题 |
| SCI-001 | **PARTIAL** | `tools/science_contract_lint.py` PASS（10 篇 15 节）；CHK-SCI-REF PASS(8/8) | `reports/RELEASE-01/science/SCI-S{1,2}-*.md` | 专项 7/7 + 11 主题完成、参考补齐完成；**2 项 P0 科学订正未落**（属冻结文档，需变更 claim 裁决） |
| BLD-001 | **PARTIAL** | cmake/ninja rc=0（0 警告）；ctest 442 = 430 通过 + 12 跳过 + **0 失败**；CHK-MODULE-MANIFEST 红 | `run/RELEASE-01/logs/BLD-001/build_ctest.log` | 构建与全量单测绿；"机器门绿"未达（P0-01/P0-16） |
| TST-001 | **PARTIAL** | 新增负例测试 10 passed；暴露 fail-open 缺陷（已由前台修复并锁定） | `reports/RELEASE-01/tests/TST-001-report.md` | 缺口矩阵/跳过判定/断言抽检完成；**负例面 46 项仅 11 项自带可执行负例**，未补齐 |
| DOC-002 | **PASS** | 订正后 `verify_doc_pack.py` PASS；Mermaid `mermaid.parse` 30/30 | `reports/RELEASE-01/docs/DOC-002-selfcheck.md` | 38 篇自查；6 处非语义订正（前台逐条复核 + 补 manifest） |
| E2E-001 | **PASS**（R 通道口径） | L3 两组 rc 全 0；L4 R 通道全量 rc 全 0；`weight_mode=2` 负例 rc=2；**1/N worker 一致性 PASS** | `run/RELEASE-01/e2e/{l3,l4}/logs/`、`reports/RELEASE-01/E2E-001-1N-consistency.md` | 全链打通；确定性门已验证；范围限 R 通道（按 DEL-001 指示） |
| VIS-001 | **FAIL** | — | `reports/RELEASE-01/vis/VIS-001-report.md`、`artifacts/RELEASE-01/VIS-001/` | 整幅+分块+裁剪放大完成；**"无接缝/背景均匀"不通过**（归因 P0-09） |
| PERF-001 | **FAIL** | — | `reports/RELEASE-01/perf/PERF-001-timing.md` | 计时/热点/L2 复算完成；**G-RES-01 enforce 非零违约**（③ 4/4 normalize、① 3/4 mosaic） |
| DEL-001 | **DELIVERED（待负责人检查）** | 结构/数值校验通过 | `reports/RELEASE-01/DEL-001-delivery.md`、`run/RELEASE-01/deliverables/DEL-001/` | 两个 R 通道平面 FITS 已交付；含已知限制 4 条 |
| FIN-001 | NOT_STARTED | — | — | 前置未满足（见 §3） |

## 2. 各任务独立验证记录（本轮新增部分）

### SCI-001（S1 + S2）

- S1：研究包 7/7；8 个开源实现源码级核验；photutils 3.0.0 数值对拍通过；"我方有误"5 项；文档订正 6 处；新增文献 9 + 代码库 8。
- S2：11 主题对照；"我方有误/需订正"34 项（P0 2 / P1 8 / P2 24）；45 处文档节补出处；新增文献 47（编号 21–67）+ 参考代码库 23（含许可证）。
- **前台独立复核**：`git diff` 显示 `docs/science|algorithms` 45 文件 **1027 增 / 2 删**；唯一语义订正 = `PSF_SIGNAL_WEIGHT.md` 的 `psf_snr_power` 行改 DEFERRED（证据 `v6_runtime_contract.h:110-114` 路由 reject）；其余为新增参考节，**未改公式**。
- **未落项（PARTIAL 原因）**：S2 的 2 项 P0（`DRIZZLE.md` §5 归一化与 §7 不变量互斥；`ASTROMETRY.md` §5a 1px 平移口径 vs Paper I/实现）位于**冻结科学文档**，按 ENGINEERING_SPEC §3 需走变更 claim + 一致性回归，本轮未改。
- 前台修复的连带红：SCI-S2 新增引用触发 `DOC-INDEX` 与 `DOC-LINE-ANCHORS` 红 → 前台补登 `docs/DOCUMENT_INDEX.yaml` + 登记 1 条 `EXTERNAL_REFERENCE` 锚豁免（photutils 为外部实现未 vendor），复跑 CHK-DANGLING PASS、CHK-SCI-REF PASS(8/8)。

### BLD-001 逐模块并行构建与冒烟

- **前台独立复跑**：`cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release` rc=0 → `ninja -C build` rc=0（0 warning / 0 error）→ `ctest --test-dir build --output-on-failure` rc=0，**442 用例：430 通过 / 12 跳过 / 0 失败**，404.30 s（`run/RELEASE-01/logs/BLD-001/build_ctest.log`）。
- **12 个跳过用例判定**（TST-001）：6 个属合理平台/硬件（无 AVX512F、CUDA/ACR dormant、有合成替代），6 个为真实 HiPS fixture-gated（原写死 Windows 路径，已由 F-02 改为可配置；fixture 需 P0-03 修复后才能生成）。
- **ctest 复跑（2026-09-18，激活 5 个零注册 aio 测试后）**：`ctest --test-dir build` → **447 用例 100% 通过，0 失败**（`run/RELEASE-01/logs/ctest_after_aio_reg.log`）。新增 5 个均为**真实产品面**验证（链接 `astrocs_hips`）：`aio_checksum`（49/0，含 CRC32C Castagnoli 标准向量）、`aio_tile_model`（67/0）、`aio_transform`（75/0/2 skip）、`aio_query_pixel`、`aio_precision_dual`。发现 `lib/infrastructure/aio/tests/` 顶层 20 个测试源**零 CMake 注册**（TST-001 记「8 枚」为低估）；未注册的 3 个及原因见 GAP_AUDIT §7.9。
- **ctest 复跑（2026-09-18，修 7 处写死 `/tmp` 后）**：`ctest --test-dir build` → **442 用例 100% 通过，0 失败**（`run/RELEASE-01/logs/ctest_final_tmpfix.log`）。复跑须知：`TMPDIR` 必须置于**源码树外**（profile store 有源码树防线，置于树内会使 `cpu007_profile_store` 判红——按设计工作，已双向对照证明与「路径含空格」无关），且本机 `/tmp` 悬空，建议 `TMPDIR=/dev/shm/astrocs_tmp`。详见 GAP_AUDIT §7.8。
- **资源门补跑（2026-09-18）**：`python3 ci/run_checks.py --check CHK-RESOURCE --quiet` → **PASS（11/11 步，0 失败）**（`run/RELEASE-01/logs/CHK-RESOURCE.log`）。其判据面为**合成/oracle**；真实数据面 L2 无注册门在跑（见 GAP_AUDIT §7.6），PERF-001 手工复算仍判违约。
- **未达项**：验收门「机器门绿」不成立 —— `CHK-MODULE-MANIFEST` 红（负责人已裁决的临时红，P0-01）+ `CHK-REGISTRY-DOC-SYNC` 红（P0-16）+ `ENG-CONSTRAINTS` 红（U-1）+ `tests/quality/test_root_cleanliness.py` 1 failed（U-1 派生）。
- **逐模块冒烟证据表（前台独立复跑，2026-09-18）**：用例归属由 `ctest --show-only=json-v1` 的**可执行文件路径**映射（非名字猜测），结果取自 BLD-001 全量 ctest 日志；独立构建 rc 由 `ninja -C build <模块 target>` 逐模块复跑。全量 442 = 已归属 356（345 通过 / 11 跳过 / 0 失败）+ 横向脚本类 29 + 未归属 57（明细 `run/RELEASE-01/logs/BLD-001/unmapped_tests.json`）。

| 模块 | 构建产物（.so） | 独立构建 | 用例 总/通过/跳过/失败 |
|---|---|---|---|
| calibration | `build/lib/algorithms/calibration/astrocs_p1_calibration.so` | rc=0 | 6 / 6 / 0 / 0 |
| cosmetic | `build/lib/algorithms/cosmetic/astrocs_p1_cosmetic.so` | rc=0 | 6 / 6 / 0 / 0 |
| star_detection | —（无独立 .so） | 无独立 target | 7 / 7 / 0 / 0 |
| psf | —（无独立 .so） | 无独立 target | 20 / 20 / 0 / 0 |
| platesolve | —（无独立 .so） | 无独立 target | 12 / 12 / 0 / 0 |
| photometry | —（无独立 .so） | 无独立 target | 7 / 7 / 0 / 0 |
| noise_snr | —（无独立 .so） | 无独立 target | 22 / 22 / 0 / 0 |
| drizzle | `.../drizzle/astrocs_p1_drizzle.so`、`.../drizzle/hips/astrocs_p1_hips_writer.so` | rc=0 | 18 / 18 / 0 / 0 |
| coverage | —（无独立 .so） | rc=0 | 137 / 126 / 11 / 0 |
| sampling | —（无独立 .so） | 无独立 target | 10 / 10 / 0 / 0 |
| upm | —（无独立 .so） | 无独立 target | 5 / 5 / 0 / 0 |
| rejection | —（无独立 .so） | 无独立 target | 6 / 6 / 0 / 0 |
| integration | —（无独立 .so） | rc=0 | 8 / 8 / 0 / 0 |
| projection | —（无独立 .so） | rc=0 | 9 / 9 / 0 / 0 |
| resample | —（无独立 .so） | rc=0 | **0 / 0 / 0 / 0** |
| fits_output | —（无独立 .so） | rc=0 | 2 / 2 / 0 / 0 |
| cli | —（无独立 .so） | 无独立 target | 3 / 3 / 0 / 0 |
| scheduler | —（无独立 .so） | 无独立 target | 43 / 43 / 0 / 0 |
| pipeline | `.../orchestrator/cpp/tests/astro_image_io.so` 等 | rc=0 | 6 / 6 / 0 / 0 |
| aio | —（无独立 .so） | rc=0 | 11 / 11 / 0 / 0 |
| benchmark | —（无独立 .so） | 无独立 target | **0 / 0 / 0 / 0** |
| observability | —（无独立 .so） | 无独立 target | 5 / 5 / 0 / 0 |
| gaia_xpsd_client | `.../gaia_xpsd_client/astrocs_catalog_gaia.so` | rc=0 | 13 / 13 / 0 / 0 |
| acr | —（无独立 .so） | 无独立 target | **0 / 0 / 0 / 0**（dormant） |
| hips_browser | —（无独立 .so） | 无独立 target | **0 / 0 / 0 / 0** |

- **该表揭示的两类事实**：① 仅 5 个模块产出独立 `.so`，多数模块**无独立构建 target**（与 `CHK-MODULE-MANIFEST` 红同源，P0-01）；② `resample`/`benchmark`/`acr`/`hips_browser` **零模块级注册用例**（TST-001 亦记 aio/gaia/hips_browser 测试零 CMake 注册）。
- **本轮修复 F-03**：`orchestrator_cli_integration` 一度红（186 通过 / 47 失败），根因是测试把 stderr 分离文件**写死 `/tmp`**（`test_orchestrator_cli.cpp:253`），而本机 `/tmp` 为悬空挂载点不可写 ⇒ 全部 spawn 类断言失败。改为**当前工作目录相对路径**后复跑：**233 通过 / 0 失败**，`ctest -R orchestrator` **6/6 通过**，pipeline 模块 target rc=1 → **rc=0**。

### TST-001

- 审查 442 ctest + Python 域；缺口矩阵按 7 类给出；12 个跳过用例判定：6 个合理（无 AVX512F / CUDA/ACR dormant / 有合成替代）、6 个因**硬编码 Windows 绝对路径**（`F:/Astro dev/...`）在 Linux 零执行；无 SKIP 充数。
- 发现并（前台）修复 1 项 P1 缺陷：`ci/verify_toolchain.py` fail-open（打印 [FAIL] 却 exit 0）→ 加 `sys.exit(main())`；负例锁定 `tests/quality/test_env_adoption_negative.py`（10 passed）；复跑 CHK-ENV-ADOPTION PASS。
- **PARTIAL 原因**：46 个注册检查中仅 11 项自带可执行负例（19 项负例是人工说明、1 项无 polarity 记录、9 项借壳），未补齐；另登记 15 项缺陷/缺口（P0 3）。
- **本轮闭合 1 项（F-02）**：6 个真实 HiPS 用例写死 Windows 绝对路径 `F:/Astro dev/...`（违反 AGENTS §3）→ 前台改为环境变量 `ASTROCS_PHASE1_FREEZE_DIR`/`ASTROCS_PHASE1_TMP_DIR`（默认仓库相对路径）。**复验**：`grep` 零命中；负例注入下 `RealHipsUnion` 由 SKIP 变为执行并 FAIL（证明环境变量生效）；`ctest -R 'phase2_synthetic_gate|phase2_sampler_parallel'` **101/101 通过 0 失败**。
  该 6 用例仍为 fixture-gated：**当前管线无法生成该 fixture**（Phase1 不产 `snr` 子产品，`p1_session.cpp:493`；而 2 个用例断言 `snr_used>0`），与 P0-03 同源，登记于 GAP_AUDIT §7.4。
- **环境注记**：本轮构建时 `/tmp` 不可写，改用工作区 `TMPDIR`；复跑者需注意。

### E2E-001（L4 R 通道全量）

| 组 | normalize | mosaic | export（full / vis） | 交付帧 |
|---|---|---|---|---|
| M42（T2 M1–M6 + T3 M1–M6，Red 300s，49 帧） | 12 作业 rc=0（22:25→23:14，≈49 min） | rc=0（≈6 min，12 HiPS 输入，obs=67363/controls=28574） | rc=0 / rc=0 | 7821×10947 |
| Galaxy Center（T4 panel1–3，Red 180s，32 帧） | 3 作业 rc=0 | rc=0 | rc=0 / rc=0 | 4856×9160 |

- 产品结构：2 HDU（signal + `EXTNAME='COVERAGE'`），CHECKSUM/DATASUM 在位；覆盖外 = NaN（非 0）；`p3_props.json` `variance_available=false`（P0-02）。
- **1/N worker 数值一致性（验收门项）**：**PASS**。派生 workers=1 的 cpu_profile（仅改 kernels.*.workers，host/build 不动）重跑同一科学配置，与 N-worker 参考清单逐产物比对：**17 项中 15 项 sha256 逐字节相同**；差异 2 项为 `p1_final.json`（含绝对路径的运行报告）与 `properties`（其 **canonical_sha256 相同**）→ 并行度未改变科学结果。证据：`reports/RELEASE-01/E2E-001-1N-consistency.md`、`run/RELEASE-01/e2e/evidence/1N-consistency-*`。
- **范围**：仅 R 通道（按 DEL-001 指示）；G/B/H-alpha 未跑，如需按"全滤镜"口径验收需补跑。

### VIS-001 / PERF-001 / DEL-001

见 §1 证据路径；结论分别为 **FAIL（接缝）**、**FAIL（L2 enforce 违约）**、**DELIVERED**。

## 3. 红灯与豁免

- 本控制包**不使用 waiver 掩盖红灯**。

### 未通过项

| 项 | 判据 | 结论 | 归因 |
|---|---|---|---|
| VIS-001 | L4 §5.2「无接缝/背景均匀」 | **不通过**（行中位数跳变 M42 17.2–22.4×、GC 9.8–12.4×；裁剪放大见面板交界台阶） | P0-09（生产 UPM 无 `b_k(x)` 稀疏天光面） |
| PERF-001 | G-RES-01 enforce 零违约 | **违约**：判据③ 4/4 normalize、判据① 3/4 mosaic | 编排/并行/内存（P1 群） |
| BLD-001 | 机器门绿 | 未达：CHK-MODULE-MANIFEST 红（负责人已裁决临时红）+ CHK-REGISTRY-DOC-SYNC 红（U-2） | 文档↔仓库冲突 + 模块门 |
| TST-001 | 每核心检查具备可执行负例 | 未达：46 项中 11 项具备 | 测试面欠账 |
| SCI-001 | "我方有误"全部订正 | 未达：2 项 P0 属冻结文档 | 需变更 claim 裁决 |

### UNRESOLVED（上呈负责人）

- U-1（P0）`ENGINEERING_SPEC.md` §7 删 `ACCEPTANCE_SPEC.md` → ENG-CONSTRAINTS 红 + `tests/quality/test_root_cleanliness.py` 1 failed（前台已复现）。
- U-2（P0）`docs/ci/01_CHECKS.md` §2 删 `CHK-EXIT-CONSISTENCY`/`CHK-E2E-REPRO` → CHK-REGISTRY-DOC-SYNC 红。**关键证据**：两项均为前批 agent 于 `5eb1433f`/`bd300e85` 自行引入，负责人文档包从未登记；建议按 §2.1 退役流程从 `ci/checks.json` 移除注册。
- U-3 / U-F（P1）版本口径四冲突（`0.1alpha` / `0.11.0-alpha.2` / `0.0.1alpha` / "Alpha 前无版本"）。
- U-4（范围裁决）新文档包新增语义（稀疏天光面、locality-aware 编排与流式内存、Gaia 两级缓存与查询合并、GLS/Q-W/psfsw、variance/ivar 链）是否属本轮发布阻断项——**决定 RELEASE-01 结论走向**。
- 其余 UNRESOLVED 主题（U-A..U-K）见 `GAP_AUDIT.md` §5。

## 4. 提交台账（本轮）

| commit | 目的 |
|---|---|
| `41b41e2d` | 负责人授权文档包替换（36 篇）+ DOC-001 核验器/清单/验收记录 |
| `44456cd9` | DOC-002 非语义订正 5 篇 + 文档包哈希清单同步 |
| `891b05a3` | AUD-001 差异审计定稿 + DOC-002 报告 + 验收记录 |
| `2d75e636` | fix(ci): verify_toolchain fail-closed + 负例锁定 |
| `fcf87d8e` | SCI-001 三方自审订正 + 参考文献/代码库补齐 + 索引/锚登记 |
