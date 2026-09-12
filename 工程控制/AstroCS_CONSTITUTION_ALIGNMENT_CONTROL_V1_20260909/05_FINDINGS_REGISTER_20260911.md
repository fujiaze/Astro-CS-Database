# 05｜Findings 登记册（项目经理验收，2026-09-11）

> 来源：负责人指令「按国际标准冻结 + 验收已完成部分，P0/1/2 问题记录在册，下一轮控制包一并解决」。
> 本登记册由项目经理基于 git 提交、源码与冻结文档直接核对产出；下一轮控制包须把下列条目
> 转为原子任务或负责人裁决项，闭环前对应域不得宣称完成。独立 SubAgent 复核在下一轮 AUD 线补强。

## 验收范围（16 项已闭环任务）

GOV-001(d8c821db)、GOV-002(8e03d7da/45695873)、WCS-001(86af39d8)、WCS-002(04556761)、
WCS-003(b8d9a69c)、PSF-001(ef1ccfcc)、DATA-001(99713034)、AIO-001(00b73fc1)、AIO-002(36f2edfa)、
P1-001(9e09941a/8545d70a)、P2-001(439f9f20)、P2-002(9e0fa3a8+991e3e2e)、P3-001(9953f103)、
P3-002(9662afa8/1a56ffb7)、P1-HIPS-DIGEST-001(45f80776)。

## P0（0 项）

无。此前唯一 P0（F-P2-002-01 rejection gather 三重错位，ASan 实证堆越界）已由 991e3e2e 修复，
修复后 mismatch=0、ASan 零报告。

## P1（3 项，须负责人裁决或下一轮修复）

### STD-F1｜CRPIX 原点 1px 口径冲突（原 WCS-003-F1，等待负责人裁决）

- **证据**：`docs/science/ASTROMETRY.md:34,47,73` 冻结 `xp=x+1`、`CRPIX=(w/2+0.5,h/2+0.5)`、
  `(u,v)=CD·(xp−CRPIX)+SIP`（1-based）；`lib/phase3_session/p3_wcs.cpp:97-98` 消费方
  `dx=(x+1)−crpix`（FITS Paper I 标准口径）；而 `lib/plate_solve` 迭代反演输出口径为
  `u=x−crpix`（0-based 语义），两者相差常量 1px。
- **标准判定**：FITS WCS Paper I §2.1.1 CRPIX 为 1-based 参考像素；`p3_wcs.cpp` 的消费方面
  与标准一致。ipv 内部自洽口径对自身 roundtrip 免疫，astropy 交叉在 crpix+1 桥接后达机器精度，
  证明差异为纯原点平移而非数学内容差。
- **风险**：真实数据 solve（P1 wcs 节点）产出的 WCS 与 Phase3 消费链/第三方工具混用时，
  存在 1px 系统错位风险；涉及冻结锚 F3 与全部既有对拍口径。
- **处置**：负责人裁决三选一——(a) 以 FITS 标准为唯一口径修 ipv 内部约定（改冻结锚，走宪章变更）；
  (b) ipv 保持内部口径，在导出边界做显式 +1 桥接并写入合同；(c) 维持现状并在合同标注
  「ipv 内部 0-based、消费方 1-based」。裁决前 REAL-001 的 solve 链须用 (b) 桥接跑通并在
  九宫格视觉核验中显式验证无 1px 偏移。

### STD-F2｜integrate 未剔除 kernel 拒绝样本（原 F-P2-002-02）

- **证据**：`lib/phase2/memory.md:208-210`；§30.2 `n_ineligible` 恒等式在部分拒绝场景不成立。
- **影响**：`nused/nrej` 产品在部分拒绝场景数值不可信——REAL-001 的 Phase2 排异产品验收会踩中。
- **处置**：下一轮 Phase2 域原子任务修复；修复前 REAL-001 的 nused/nrej 断言降级为
  「记录不判定」并显式登记。

### STD-F3｜AIO 通道缺失：NREJ/NUSED int32 + ASTROCS_* provenance 键（原 F-P2-002-03）

- **证据**：`lib/phase2/memory.md:210`；P2-002 提交说明 pending_contracts 登记。
- **影响**：DATA-UNC-001 §30.2/§30.3 冻结的子产品位与五 provenance 键无落盘通道，
  Phase2 产品 manifest/provenance 完整性验收（REAL-001 层B）无法满足。
- **处置**：下一轮 AIO 域原子任务（writer 通道 + 键写入 + verify 双向断言）；
  必须先于 REAL-001 闭环。

## P2（3 项，下一轮顺带解决）

### STD-F4｜IVOA HiPS properties 键集可补强

- **现状**（`lib/astro_image_io/src/hips/aio_hips_writer.cpp:766-814,980-1005`）：已写
  `hips_order/hips_tile_width=512/hips_tile_format=fits/ORDERING=NESTED/t_min/t_max/obs_title/
  obs_filter/obs_date/prov_pixfrac/prov_scale`；META-002 无真实 passband 不伪造 em_min/em_max（诚实）。
- **缺口**：testdata/index.json 已有滤镜 brand/model/bandwidth_nm（如 H-alpha 7nm），可提供真实
  `em_min/em_max/obs_bandpass`；另 `hips_version/hips_release/dataproduct_type/moc_order/
  s_pixel_scale/hipsgen_date` 等 IVOA 推荐键未全覆盖。
- **处置**：下一轮以 index.json 滤镜元数据接通真实波段键（不得臆造缺失值），
  键集对照 IVOA HiPS 1.0 §2.3.2 做符合性清单测试。

### STD-F5｜监督任务欠账

BASE-UTIL-001（裁决 2 选 A）与 ARCH-AUDIT-P1（P1-001 五点独立抽验）两项已裁决未执行。
pytest p2001/p2002/p2006 的失败根因（CLI integrate ivar fail-closed fixture 缺口 +
resource gate 事件缺失，CLI 域）仍开放。下一轮必须排在 RT-001 之前/并行。

### STD-F6｜国际标准冻结注册表缺失

- **现状**：宪章 §19 与各 ALG 文档已有文献锚（PHASE3_PROJ_IMPL.md §15.8 逐式锚定
  Calabretta & Greisen 2002 Paper II Table 1/Paper I；HIPS_WRITER.md 锚定 IVOA HiPS
  Fernique 2015/Górski 2005；astropy 交叉验证测试面在位），但**没有一份唯一的、版本化的
  标准注册表**把「领域→标准→版本→条款→符合性清单→偏差登记」固定下来。
- **处置**：下一轮治理任务 `STD-REG-001`：创建 `docs/standards/STANDARDS_REGISTRY.md`
  （ACTIVE_NORMATIVE，登记进 DOCUMENT_INDEX），冻结领域映射：球面投影=FITS WCS Paper I/II
  （含 CRPIX/parity 语义）、HiPS=IVOA HiPS 1.0(+1.4 修订)、HEALPix=Górski 2005 NESTED、
  Drizzle=Fruchter & Hook 2002、星表=Gaia DR3 data model、FITS=4.0；每域附符合性清单与
  已知偏差指针（如 STD-F1）。此后任何科学/格式实现与注册表冲突一律按 finding 处理，
  禁止按实现反推标准。

## P1（补，2026-09-11 负责人指令：CI 必须随代码订正）

### STD-F7｜全量 C++ 测试矩阵仅在 linux-deep profile（7 项）且全部 waivable=true

- **证据**（ci/checks.json 实测解析）：profile 覆盖 fast=57/linux-main=71/windows-main=61/
  **linux-deep=7**；`BUILD-GCC-RELEASE`(linux-main+deep)、`DEEP-CLANG-BUILD`、`DEEP-SAN-ASAN`、
  `DEEP-SAN-TSAN`、`DEEP-COV-CPP`（该检查经 ccov 目标实际执行全量 ctest）全部
  `waivable=true`。即**常规 CI（fast/linux-main）不强制跑 C++ 测试矩阵，deep profile 又可整体豁免**。
- **影响**：本轮 16 任务新增的 12+ 个测试目标（p1001/p2001/p2002/p3002_real_nodes、
  p2002_unc_rej_prov、p3002_uncertainty、p3_projection_units/fault、p1wcs_apbp、
  p1wcs_astropy_cross、aio_abi_* 等，add_test 注册在案）虽然随全量 ctest 在本地跑绿，
  但 **CI 从未把"它们被执行且全绿"变成不可豁免的门**——下一任务完全可能在 CI 层静默回归。
- **处置（下一轮 CI-REG-002，先于 CI-001）**：
  1. 把本轮全部新增测试目标注册为 checks.json 显式检查项（per-domain id，绑定 profile）；
  2. `linux-main` 必须包含全量 ctest（或新建 `linux-ctest` profile 且设为 CI-002 前置门）；
  3. `BUILD-GCC-RELEASE`/`DEEP-SAN-ASAN`/`DEEP-COV-CPP` 改 `waivable=false`；
  4. 任务规格同步修订：写代码任务必须"新增/修改测试目标 → 同提交注册 CI 检查项"，
     validators 增加负向检查（发现未注册的新 add_test 目标即 FAIL）。
- **本轮不改**（负责人指令），仅记录。

### STD-F8｜windows-main 三门已不可豁免（澄清+保持）

- 实测：`WIN-BUILD-RELEASE`/`WIN-TEST-UNIT`/`WIN-PACKAGE-CANDIDATE` 均 `waivable=false`——
  早前审计报告"Windows 三门可豁免"以当前 ci/checks.json 为准已不成立（06_ 目录旧审计口径作废）。
- 残留：candidate 缺失时 workflow 的 warning 跳过路径（.github/workflows/ci-windows.yml:64-79）
  与 checks.json 的不可豁免如何联动，下一轮 CI-001 一并核死。

### P2（补）

### STD-F9｜known-failures 基线未机器化

- 各任务提交以"唯一 FAIL=p1_noise_adapter=F-AIO-001 预存豁免"人工口径声明；仓库虽有
  `tools/quality/known_failures_baseline.py` 与 `artifacts/KNOWN_FAILURES_BASELINE.json`（未跟踪），
  但 CI 未强制"失败集 ⊆ 版本化基线"。下一轮把基线纳入 CI 检查（新增失败不在基线 = FAIL），
  防止基线漂移吞掉新回归。

### STD-F10｜测试面第三方依赖未注册

- `tests/unit/p1wcs/p1wcs_astropy_cross.py` 依赖 astropy/numpy，未登记进所在检查项的
  `prerequisite_tools`（当前机制：缺依赖=waiver 而非 FAIL）。下一轮随 CI-REG-002 补登记，
  缺依赖必须显式 SKIP 记录而非静默绿。


## 通过项（PM 直接核验，供下一轮独立审计抽验）

- 四投影公式逐式锚定 Paper II 并有 numpy 第一性对拍（解析 ≤5.68e-14 deg、往返 ≤2.1e-10 px）；
- 三 Phase 节点化均满足宪章 §8.2：唯一 operation/entry、typed artifact 链、call_count=1、
  fail-closed、故障注入必败（p1001/p2001/p2002/p3002 四个 real_nodes 测试在案）；
- AIO C ABI 状态码 17 码与 acs_status/acs_fio_status 编译期对齐断言；内容哈希流式重算复核；
- HiPS 原子发布 staging→fsync→promote 全或无，kill/ENOSPC 注入恢复验证；
- PSF 批量 ABI 五入口统一边界拒绝，w/h∈{0,-1,INT_MIN} 30 断言 + 溢出收口；
- 控制包 rev3.1 与台账/控制图一致（CONTROL_PASS tasks=30）；
- windows-main 三门（BUILD/TEST/PACKAGE）已实测 waivable=false（STD-F8 澄清）；
- 预存 dirty 未被收编，工作区仅剩 4 个历史修改文件。

---

# 附录 A｜V2 round1 前台验收并入（2026-09-12）

> 来源：CPRun run `Rmtxvlrtfa66eb7`（rev5，47 任务）round1 三个节点验收 + 前台独立复核。
> 由前台 Agent 在机器验收后逐条并入；各条证据落 `run/` 与
> `/workspace/.dsh/control-pack-runs/Rmtxvlrtfa66eb7/evidence/<node>/1.json`。

## A.1 CI 面（来源：CI-REG-002 / CI-REPAIR-001 通报 + 前台独立复核）

### FD-R1-005（P1）｜linux-deep 四项 configure 硬失败：`tests/unit/p1wcs/CMakeLists.txt:70` `find_package(OpenMP REQUIRED)`

- **证据**：`DEEP-CLANG-BUILD` / `DEEP-SAN-ASAN` / `DEEP-SAN-TSAN` / `DEEP-COV-CPP` 四份日志同一错：
  `CMake Error ... Could NOT find OpenMP_C (missing: OpenMP_C_FLAGS OpenMP_C_LIB_NAMES)` @ `tests/unit/p1wcs/CMakeLists.txt:70`；
  `DEEP-COV-CPP` 的 ccov 因 configure 未产出 Makefile 级联失败。摘录：`run/ci_repair/round0/linux_deep_failure_excerpts.txt`。
- **blame**：`30296a9f0`（2026-09-09）引入 `REQUIRED`。
- **慢性红**：`ci-linux.yml` 的 schedule(linux-deep) 自 `da0cf578`(09-06) 起**连续 6 次 failure**，非本轮新引入。
- **影响**：deep 是唯一跑全量 C++ 矩阵的 profile（STD-F7 原文）⇒ 全量测试矩阵实际无 CI 覆盖。
- **处置**：交 CI-REPAIR 线（域外，需扩写域或新增任务）；**禁止 waiver**。CI-REG-002 已以 CTEST-LINUX-FULL 绑 linux-main 绕过该依赖，但 deep profile 本身仍红。

### FD-R1-011（P1）｜`e6254d4d` 的 ZLIB 修复不完整，WIN-BUILD-RELEASE 仍红

- **证据**：`WIN-BUILD-RELEASE` exit 1；`run/ci/win-stage-configure.log`：
  `CMake Error ... Could NOT find ZLIB (missing: ZLIB_LIBRARY) (found version "1.3.2")`，
  栈 `FindZLIB.cmake:202` → `lib/gaia_xpsd_client/CMakeLists.txt:68 (find_package)`。
- **根因**：`e6254d4d` 仅让 gaia 段消费 `ACS_ZLIB_ROOT`（include 命中 → version 1.3.2 找到），
  **未消费 runner 注入的 `ACS_ZLIB_LIB=zs.lib`**；根 `CMakeLists.txt` 的 astrocs_cfitsio 段消费 `ACS_ZLIB_LIB`，两段口径分叉。
- **级联**：`WIN-PACKAGE-CANDIDATE` / `WIN-TEST-UNIT` 因 configure 未产出 `cmake_install.cmake` 全红
  （`win-stage-install.log: Not a file: .../cmake_install.cmake`；`win-stage-test.log: No tests were found`）。
- **影响**：Windows 三门全红；`F-CI-002-05` 的「已修复」判定与实际不符。

### FD-R1-010（P1）｜任务覆盖缺口：15 条红灯中 13 条无 V2 任务承接

- **方法**：控制包任务规格全文 grep。**已覆盖**：仅 `UT-CLI`（UT-CLI-MAINT / CI-BASELINE-001）。
- **未覆盖**：`AGENTS-GOV` / `DATA-ARTIFACTS` / `THREAD-BUDGET` / `CON-COMMENTS` / `UT-BACKEND` /
  `WIN-BUILD-RELEASE`，以及全部 `DEEP-*`（CLANG-BUILD / SAN-ASAN / SAN-TSAN / COV-CPP / COV-PY）。
- **影响**：**G-CI-L1（双虚拟机绿）无任务承接，门禁链在第一道门即不可达**。

### FD-R1-008（P1，机制）｜跨 lane 同文件并发写风险

- **实证**：CI-REPAIR-001 与 CI-REG-002 写域重叠（`ci/`、`tools/quality/`）；前者读数 80 项→数分钟后 97 项。
- **前台复核**：`validate_registry --strict` PASS(97, errors=0)，schema 已同步定义 `ctest_targets` ⇒ **未造成损坏**。
- **根因**：框架 lane 容量互斥按 lane 计，**不按文件写域计**。前台已裁定单写者并串行化。

### FD-R1-009（P1）｜CI-REPAIR-001 写域结构性不可达

- 15 条红灯根因全在其白名单（`run/ci_repair/`、`ci/`、`tools/quality/`）之外 ⇒「本轮修复至双绿」不可达。
- 前台**不扩写域**（write_scope 属控制包规格，仅负责人可改）。

## A.2 科学/架构面（来源：ARCH-AUDIT-P1 五项确认 + BASE-UTIL-001 归因）

### FD-R1-012（P1）｜共享产物路径原地非原子重写 + 并发消费者（生产数据完整性缺陷）

> **三方独立互证**：CI-REG-002 注册检查时发现 → 前台独立复现 → ARCH-AUDIT-P1 独立审计再次命中。

- **前台独立复现**：`run/ci/build-gcc-release/tests/unit/p1001_real_nodes_test` 连跑 **200 次 → pass=176 / fail=24 = 12%**
  （高于 CI-REG-002 上报的 3%~10%，按最坏值定性）。样本 `run/ci/ci-reg-002/p1001_repro_fail_*.log`（24 份）。
- **ARCH-AUDIT-P1 独立复现**：直跑 20/20 通过；`ctest -R ^p1001_real_nodes$` 10 次中 1 次失败、另一轮 5 次中 3 次失败。
- **交错形态**：`Writing calibrated_light_1.fits` → `Reading` 同路径 → `[ERROR][FITS] Not a FITS file` → `Write OK`
  → `CHECK failed tests/unit/p1001_real_nodes_test.cpp:411: rrun.ok() -- node drz failed: cannot read: .../calibrated_light_1.fits`。
- **根因（生产代码，非测试面）**：`lib/core/src/module_adapters.cpp`
  —— `p1_op_calibration` 写 `out_dir + "/calibrated_" + base`（约 1067 行）；
  `p1_op_cosmetic` 经 `p1_calibrated_path` 读**同一路径**后**原地覆写同一路径**（约 1136 行 `aio_write_fits(im.p, path.c_str())`）；
  `p1_op_drizzle` 经 `p1_calibrated_path` 读同一路径（约 1538 行）。
  chain IR 中 `cos` 与 `drz` 同为 `artifact:cal` 消费者且节点声明 `resources.parallel=true`，`create_runtime(2)` 下并发 → 撕裂读。
- **原子性缺口**：AIO-002 仅使 HiPS publish 原子；`aio_write_fits` 本身**非原子**，消费者可观察到半写文件。
- **影响**：REAL-001 真实 Phase1 链在并行 worker 下同样具备触发条件；且 `CTEST-LINUX-FULL` 绑 linux-main 不可豁免后，
  **每次 push 约 12% 概率假红 → G-CI-L1 非确定性**。
- **处置**：cosmetic 改原子写（临时文件 + rename）或独立产物名 + ≥2 worker 链式回归。**禁止 waiver**（科学数据完整性）。
- **归属**：见 FD-R1-013（无在册任务写域）。

### FD-R1-013（P1）｜覆盖缺口：`lib/core/` 与 `tests/unit/p1001_real_nodes_test.cpp` 无任何在册任务写域

- **方法**：`control-pack.json` 全任务 write_scope 扫描。持 `tests/` 写域者为 CI-BASELINE-001 / UT-CLI-MAINT /
  SCI-ANCHOR-001 / RT-001A / RT-001B / MOD-001A / MOD-001B / CLI-001B / STD-F1-ADJ / SCI-F2-001 / SCI-F3-001，
  **均不覆盖** `lib/core/src/module_adapters.cpp` 与 `tests/unit/p1001_real_nodes_test.cpp`（RT-001 曾持 `lib/core/` 但已 PASSED）。
- **影响**：FD-R1-012 的修复无归属任务，无法派发。

### F-ARCH-AUDIT-P1-01（P2）｜HiPS 覆盖分数属性精度丢失

- **证据**：`lib/astro_image_io/src/hips/aio_hips_writer.cpp:801` 用 `std::to_string(double)`（固定 6 位小数）
  序列化 `astrocs_covered_sky_fraction`，真值 **3.179e-07 被写成 "0.000000"**；`moc_sky_fraction` 同函数同写法。
- **影响**：小视场 HiPS 覆盖分数信息丢失。
- **处置**：改 `%.8e`/`%.17g` + 属性精度回归。归 AIO/HiPS 域任务。

### F-ARCH-AUDIT-P1-02（P1）｜与 FD-R1-012 同一缺陷（独立互证）

- ARCH-AUDIT-P1 在影子树内独立命中：`p1_op_cosmetic` 就地重写 `calibrated_<base>.fits`（:1122 读、:1140 写回同路径），
  `p1_op_drizzle` 读同一路径（:1538），workers≥2 并发 → 间歇 `node drz failed: cannot read`。
  与 FD-R1-012 合并处置，不重复登记。

### BASE-F1（P1）｜`evaluate_mon001` 逐样本利用率判据缺 active window ≥10s 前置（短窗恒假拒绝）

- **证据**：`cli/resource_gate.h:284-293` `evaluate_mon001` 仅以 `util_samples_pass_frac < 0.70` 判定，**无 wall 前置**；
  同源判据 MON-002 在 `cli/commands.cpp:784` 有 `if (act.wall_seconds >= 10.0)` ⇒ **两条同源判据口径不一致**（前台已复核该两处代码属实）。
- **边界样本律**（实测）：0.5s 采样下每 run 必含 2 个边界样本（首样本 CPU=0、末样本收尾降速）
  ⇒ 全并行 workload 的 `pass_frac=(n−2)/n < 0.70 ⇔ n < 6.67` ⇒ **active 窗口 < 3.33s 必然 FAIL**。
  去掉边界样本后同一实测序列全部转 ok。与 CI 日志 `UT-BACKEND.log:415,426`「0.500000 < 0.700000（有效样本 4）」同源同机制。
- **影响**：任何 active 窗口 <3.33s 的 heavy run 被恒假拒绝（exit 10）；快宿主随核数增加**更易**失败。
- **处置**：补 `active_window_seconds >= 10.0` 前置（判据数值 75%/70%/10s 与冻结门**不动**）；须负责人确认属「判定域收口」而非放宽。

### BASE-F2（P1）｜门禁分母以机器核数冒充「已分配容量」

- **证据**：`cli/commands.cpp:770-771` `g.available_cpus = cli_affinity_cpu_count(); g.selected_workers = budget;`
  （budget 同源 = 亲和核数），`cli/resource_gate.h:261-264` 分母 = `min(selected_workers, available_cpus)`；
  与 **ARCH-THREAD-001（FROZEN）§1**「CPU 内核获得 `min(budget, kernel_block_hint)`」不符。
- **实测**：N=16 时创建 31 线程但**瞬时并行宽度仅 ~1.65 核**，wall 与 N=1 无差异；以 16 作分母 ⇒ U≈0.06 ⇒ 恒 FAIL。
- **处置**：`selected_workers` 取 Runtime lease 对该 span 的实际授予值，或 gate 输入面拆「机器容量」/「已分配容量」两字段。

### BASE-F3（P1）｜观测面 workers/queue/progress 为配置注入值而非真实观测

- **证据**：`cli/commands.cpp:713` `recorder.set_workers(budget, budget);` —— 实测 CPU 仅 ~1 核却报 `active_workers=16, runnable_workers=16`；
  `worker_balance.csv` 的 `utilization_pct` 恒 50.00%。`set_progress()`/`set_queue()` 在 `cli/` 全仓**无调用点** ⇒ `progress`/`queue_depth` 列恒 0。
- **违反**：宪章 §10.5「创建多个空闲线程均不算通过」；`RESOURCE_MONITORING_CONTRACT.md` §6「配置值冒充观测即违例」。
- **处置**：接 RT-006 trace 真实观测（合同 §6 已定义 `TraceSnapshotObserver`，CLI 未接线）；不可得时留空/哨兵，**不得填配置值**。

### BASE-F4（P2）｜`docs/architecture/THREADING_MODEL.md` 与 phase2 sampler 实现漂移

- 文档称 Phase2 sampler「串行（无任何 OpenMP parallel region）」「`P2_ENABLE_OPENMP OFF` 硬禁用」，
  而 P2-001（`10d1c318`）已改为 `std::thread` 池 + Runtime lease 多 worker（`sampler.cpp:880-913`）。
  `THREAD_BUDGET_ARCH.md` §4 仍引用旧锚点。⇒ L1 架构文档与代码不一致（宪章 §12.3）。

### BASE-F5（P2）｜phase2 节点链 integrate 对合成 fixture 恒 fail-closed（域外）

- **证据**：BASE 二进制上 `phase2 run` 对 F1/F2 与 seam6 fixture 均以
  `node integrate failed: weight_mode=2 requires per-frame ivar products; N/N frames missing ivar (DATA-UNC-001 §30.1)` exit 2；
  fixture 生成器 `tests/backend/phase2_fixture_main.cpp` 只写 SIGNAL|SUPPORT，不写 IVAR。
- **前台独立复核**：亲自复跑 `tests/backend/test_p2001_parallel_sampler.py` 得同因 rc=2（2 failed, 1 passed）。
- **影响**：p2001/p2002/p2006/p2007 的 phase2 测试全部在**资源门之前**失败 ⇒ 利用率门证据**不可得**；
  「p2001/2/6 utilization_p75_low」为**过期口径**（`HANDOVER.md:110` 需订正）。
- **处置**：由 DATA/AIO 线补 fixture ivar 通道，或在测试 config 显式 `legacy_allow_weight_fallback=true`（二选一须裁决）。

### BASE-F6（P2）｜`MemoryGrowth` 判据无最小窗口前置

- **证据**：phase3 n16 场景（active 窗口 1.5s、3 样本）报
  `memory_growth rss_slope 41.855859 MB/s > 阈值 32.000000 MB/s`，rc=10；
  `cli/resource_gate.h:181-182` 的 `rss_slope_measured && rss_slope_mb_per_s > limit` 判定**无最小窗口前置**。
- **影响**：1.5s 窗口的**启动分配爬坡**（首样本 RSS 6MB → 后续 74–136MB）被算成 41.9 MB/s「持续增长」；
  且会**抢占** `utilization_p75_low`（`evaluate_gate` 先返回即短路）。
- **处置**：加最小窗口/最小样本前置（如 active window ≥10s 或 n_samples ≥20），阈值与判定式不动。

## A.3 过程/机制面

### FD-R1-001（P1）｜CI-002 依赖门与负责人「直接 retry」指令冲突

- `cp dispatch CI-002` → `deferred:not_ready`（force 亦不放行）；依赖 8 项（CI-001B / CI-BASELINE-001 / RT-001A /
  RT-001B / MOD-001B / CLI-001B / SCI-F2-001 / SCI-F3-001）均未 PASSED。前台不越权绕过；retry 计数未消耗。

### FD-R1-002（P2）｜repo-write lane capacity=1 为唯一吞吐瓶颈

- `max_parallel=6`，但 read-only 空槽无对应 READY 节点 ⇒ 实际并行度受限；5 个写任务串行排队。

### FD-R1-003（P2）｜台账 PASSED 21 与 `00_READ_FIRST` 声称「V1 已闭环 22 项」差 1

- V1 归档台账另有 `WCS-002=PASSED_FINDING_OPEN`、`P2-001=IN_FLIGHT`；21 项全部已在 main 历史核对到对应提交，**不重做**判定不受影响。需 owner/PM 澄清计数口径。

### FD-R1-004（P3）｜根目录散落运行产物（预存，未收编）

- `alloc_report.json` / `alloc_samples.csv`（预存 untracked，非本轮产生）。归位目标 `run/resource/`；
  预存 dirty 保护（`00_READ_FIRST` §3）⇒ 登记不处置。

### FD-R1-014（P3）｜cprun 证据封装格式瑕疵

- CI-REG-002 与 ARCH-AUDIT-P1 的 `evidence/<node>/1.json` 缺 `schema` 字段（框架期望 `cprun/evidence-v1`）；
  CI-REG-002 另有 `checks[].status` 大写 `PASS`（期望小写 `pass`）。
- **影响**：框架判 `evidence_valid=false`；前台经**独立机器复跑**确认实质交付合格，故不构成实质不通过，但应在后续节点避免。

## A.4 round1 验收结论

| 节点 | 前台验收 | 依据 |
|---|---|---|
| CI-REG-002（`d3097319`） | **PASS** | validate_registry --strict PASS(97)；check_ctest_registration PASS(175/15/160)；plan-only linux-main=88 含 17 项 CTEST-*；负向注入 rc=1→还原 rc=0 零 diff；waivable 落地核实 |
| ARCH-AUDIT-P1 | **PASS** | C1–C5 全 PASS；git archive 影子树零越界；5/5 故障注入必败 + 恢复后 GREEN |
| BASE-UTIL-001 | **PASS** | 四类归因齐备；前台独立复核三处关键论断（commands.cpp:765 提前 return、commands.cpp:784 vs resource_gate.h:284 口径不一致、p2001 rc=2 复现）属实；写域零越界 |

**需负责人裁决项（累计 7）**：FD-R1-003（计数口径）、FD-R1-008（跨 lane 写域互斥机制）、FD-R1-009（CI-REPAIR 写域）、
FD-R1-010（G-CI-L1 无承接任务）、FD-R1-012/013（p1001 生产并发缺陷修复归属 + 无写域）、
BASE-F1/F2/F3（资源门判定域收口是否属放宽冻结门）、BASE-F5（fixture ivar 二选一）。



---

## 附录 B｜rev6 前台裁决轮 findings 登记（2026-09-12）

> 来源：rev6 前台（cp run `Rmty4r4lx6b578e`）逐任务机器验收。凡依宪章可裁决者一律在
> `07_FRONT_DESK_RULINGS_20260912.md` 留档裁决，只有宪章无法裁决者才登记为待裁项。

### B.1 GOV-AGENTS-001（`b40c8a49`）验收 = **PASS**

- 交付：`AGENTS.md`（+24/-3，仅此一文件，零越界；预存 dirty 零覆盖）。
- 前台独立机器验收（非采信自述）：
  - `python3 tools/check_agents_gov.py` → `GOV_CHECK_PASS 10/10 要素齐备, 无冲突条款` rc=0（改前 10/10 全 MISS）；
  - 子代理报三 profile CI 同构全绿（fast/linux-main/windows-main `verdict=PASS total=2 pass=2 fail=0`）；
  - **前台逐条核对 10 项映射的宪章条款号真实性**：§3.1/§3.2/§3.3、§8.1、§10.1–10.5、§12.3、
    §14.1/§14.4/§14.5、§15.1/§15.3/§15.4、§16.1 全部命中；§17.6/§17.12 系 §17 编号列表第 6/12 项，引用成立，**未臆造**；
  - 引用文件存在性核实通过；`AGENTS.md` sha256=`3cb31789…` 与子代理申报一致；
  - **符合 R-07**：映射式承载，未放宽 `tools/check_agents_gov.py`、未删检查项、未改 `ci/checks.json`。
- 三 SHA：`b40c8a49` → 已 push（前台复核 `git merge-base --is-ancestor` 确认 origin/main 含之）。

### B.2 GOV-AGENTS-001 上报的检查器口径冲突（F1–F6）——前台逐条裁决

| 编号 | 级别 | 事由 | 前台裁决 |
|---|---|---|---|
| **F1** | P2 | 检查器「状态机」断言字面量 `REVIEW_PENDING`，但 `tools/quality/validate_task_ledger.py:42` 的 `STATUSES` 不含它、`:208` 自测用例名为 `illegal_state` | **采纳子代理护栏，不裁决改工具链**：`REVIEW_PENDING` 系 V5 遗留，现行台账验证器已判非法；映射行已显式标注「不得据此新增状态」。检查器字面量保留（删则门失效），语义以台账验证器为准。 |
| **F2** | P3 | 检查器「节点」要求字面量 `vm-bj`，而宪章 §15.1 有意按角色表述且禁止沿用历史机器规格/硬编码主机路径 | **裁决**：`vm-bj` 为运行事实（见 `FATDUCK_ACCESS.md`、`docs/architecture/ISA_VARIANTS.md`），无规范效力；映射已标注其性质。不为此改写宪章（§1.2 权限在负责人）。 |
| **F3** | P2 | 检查器「alpha/发布」要求字面量 `AWAITING_EXTERNAL_RELEASE_REVIEW`，但 `assemble_audit.py:181,209` 表明当前候选 `verdict=RELEASE_NOT_READY_BLOCKED`、该字面量**合法不可生成** | **采纳子代理处置**：映射已写明「不等同发布、合法达成由审核校验器判定」，与宪章 §17.12「只有负责人可决定发布」一致，避免被读成宣布发布。 |
| **F4** | P3 | 「不停工」字面量与 §15.3 原文措辞略异 | **裁决**：语义同向，以宪章 §15.3 为准，无需处置。 |
| **F5** | P2 | 检查器硬编码 `PATH="AGENTS.md"` 要求其承载治理要素，而 §1.1 视其为薄入口；`ci/ci_repair_round.py:62` 已记录该脱节 | **裁决（R-07 已覆盖）**：映射式承载使其同向而非互斥；长期是否改检查器口径属工具链演进，**不在本轮**（改检查器易被读成放宽门，须负责人明确同意）。 |
| **F6** | P3 | 根目录未跟踪 `alloc_report.json`/`alloc_samples.csv` | 与既有 **FD-R1-004** 同源，已登记；按裁决 **R-06** 归位 `run/resource/`。 |

### B.3 rev6 前台补登记（本轮实测）

- **FD-R6-001（P1，已由 R-01 裁决闭环）**：`ci-repair` 与 `repo-write` 双写 lane 导致跨 lane
  写域污染已 materialize 到 git 历史（`830b38c6` 吞并 CI-BASELINE-001 在制改动，见 FD-R1-017）。
  裁决：`ci-repair` lane 停止派发新任务，全部 tracked 写归写域分道（R-01）。
- **FD-R6-002（P1，已由 R-03 裁决闭环）**：`lib/core/src/module_adapters.cpp` 承载两个独立 P1
  （§30.2 逐样本排异塌缩、p1001 并发撕裂读）却零写域覆盖（FD-R1-013/FD-R1-025）。
  裁决：补正 SCI-F2-001 写域 + 新增 `CORE-RACE-001`（R-03）。
- **FD-R6-003（P2，已裁决）**：控制包写域过粗导致吞吐串行化——原 lanes 仅单条 `repo-write`(cap 1)，
  13 项待办实测连续 3 次 `cp dispatch` 全部 `deferred:lane_capacity`。
  裁决：按实际写域拆为 5 条互斥道（同域仍串行、跨域并行），`max_parallel` 6→12；
  实测一次派发 7 个并发（`178222a7`）。
- **FD-R6-004（P2，需负责人追认）**：cprun 注册修订包 = 新 run，节点状态从零开始，
  而门禁条件形如 `{task,state:passed}` ⇒ 历史已交付节点若不在新 run 内重走将**永久阻塞门禁**。
  前台已瘦身门禁至真实剩余工作（G-CI-FIX 9 项 / G-SCI 3 项 / CI-002 12 项依赖），
  不重走 21 个纯历史归档节点；状态权威以 `TASK_LEDGER.csv` + `07` 裁决件交叉登记。


### B.4 rev6.1 补登记（并行化改造）

- **FD-R6-005（P1，已由 R-15/R-16 裁决闭环）**：并行化后暴露——多个任务规格均含
  「新测试目标→`ci/checks.json` 显式检查项」，即**多条并行道会同时读-改-写同一热点文件**，
  与 FD-R1-017 同源、属同缺陷形态的**结构性复发**。若未拦截，将再次产生丢失更新与历史归因错乱。
  裁决：`ci/checks.json` 单写者=前台（R-15）；并行度改按真实文件写域判定（R-16）。
- **FD-R6-006（P2，已由 R-17 裁决）**：`cp` 框架的 lane 抽象与「同工作区 tracked 写串行」
  （宪章 §14.5）并不等价——lane 是任务分组，真实约束是**文件写域**。误把 lane 当吞吐上限
  会导致修复工作被无关任务阻塞。裁决：lane 仅作记账，不作为吞吐上限（R-16/R-17）。

