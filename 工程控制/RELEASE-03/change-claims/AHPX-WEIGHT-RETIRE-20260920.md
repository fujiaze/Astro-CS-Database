# 变更 claim：AHPX-WEIGHT-RETIRE-20260920 — .ahpx（HiPS 格式内部）权重枚举作废

- 控制包：RELEASE-03 / 任务 **FIX-202**
- 日期：2026-09-20
- 依据（最高权威）：`ASTROCS_DESIGN.md` §2.1（全程只有 SNR，不存在「权重模式」）、
  §3.4（HiPS 只承载帧级 SNR + 稀疏相对 SNR 比值；稀疏层必须在交换合同里有位置）、§12（版本号不作为生效条件）
- 依据（裁决正本）：`工程控制/RELEASE-02/GAP_AUDIT.md` §9.73【裁决 A44】（逐字）；
  `工程控制/RELEASE-03/GAP_AUDIT.md` V02；`run/RELEASE-02/design-merge/DESIGN-DRAFT.md` §3.1-D
- 依据（工程流程）：`ENGINEERING_SPEC.md` §3（变更 claim + 一致性回归）、§6/§7/§9；`AGENTS.md` §1/§8
- 状态：**已落地**（格式头 / writer / reader / C ABI 声明与定义 / 回归测试 同步完成）
- 影响类：**破坏性格式变更**（仅限**已废弃**的 `.ahpx` 容器；处置对象是 A44 已裁决项，非新决定）

## 1 裁决原文（负责人逐字，不得改写）

> 「2.**这是一个概念吗？而且哪里有权重**。在HiPS里面存的是**帧级SNR和稀疏的相对SNR比值**。
> **全程都是SNR才对**。**只有阶段二消费SNR的时候，根据这个位置上像素对应的集合计算权重**，
> 其他时候哪里有权重？至于叫什么那个是变量名，**直接叫weight不行吗**」

（来源：`工程控制/RELEASE-02/GAP_AUDIT.md` §9.73【裁决 A44】。）

定案（同节）：① HiPS 内**存**帧级 SNR + 稀疏的相对 SNR 比值；② 权重是**阶段二消费 SNR 时
按该像素对应帧集合现场算出的派生量**；③ 变量名直接叫 `weight`；④ 一切「权重模式/权重档位/
mode0·mode1·mode2」表述须删改；⑤ 阶段一/阶段三**不产生也不消费权重**。

## 2 变更内容

### 2.1 格式数据面（.ahpx）

- 只承载 **`pixel` 块**（图像）与 **`snr` 块**（帧级 SNR）；**不承载任何权重**；
- 删除 `WeightMode` 枚举（SCALAR/GRID/PIXEL）、`WeightInfo` 结构、`AhpxWriteConfig` 的
  `weightMode/gridW/gridH`、`AhpxWriter::setWeightScalar/Grid/Pixel`、`AhpxReader::readWeight`；
- 头 JSON 不再写 `"weight":{"mode","grid_w","grid_h"}`，数据块表不再有 `weight` 块；
- 稀疏相对 SNR 比值（`rho_c = SNR_c / SNR_frame`）由 **HiPS 产品层**承载（最高设计 §3.4），
  不在本容器内；本容器只保证"帧级 SNR 有位置"。

### 2.2 变更对象（文件 × 变更性质）

| # | 文件 | 变更性质 |
|---|---|---|
| 1 | `lib/infrastructure/aio/include/aio_ahpx_format.h` | 删 `WeightMode`/`WeightInfo`；新增数据面与兼容策略注释 + `RETIRED_WEIGHT_FIELD` + `hasJsonKey`（键位探测，唯一事实源） |
| 2 | `lib/infrastructure/aio/src/ahpx/aio_ahpx_writer.h/.cpp` | 删权重配置/成员/setter/weight 块写出；新增"元数据含已作废字段即拒绝写出"守卫；blocks 注入的逗号记账改为不依赖 weight 成员 |
| 3 | `lib/infrastructure/aio/src/ahpx/aio_ahpx_reader.h/.cpp` | 删 `readWeight`；新增旧格式显式拒绝（头 JSON `weight` 键 **或** `weight` 数据块）与 `getRejectReason()` |
| 4 | `lib/infrastructure/aio/src/ahpx/aio_ahpx_api.cpp` | `aio_ahpx_write` 去掉 4 个权重参数与模式分派；读侧入口在拒绝时输出原因 |
| 5 | `lib/infrastructure/aio/include/astro_image_io.h` | `aio_ahpx_write` **声明**同步去掉 4 个权重参数（声明与定义同形；前台 2026-09-20 批准的文件域扩展） |
| 6 | `tests/unit/ahpx_hips_format_test.cpp`（新增）+ `tests/unit/CMakeLists.txt` | 回归锁 N1/N2/P1/N3 + 三档故障注入 + CLI 探针（见 §4） |

**未改**（明确边界）：科学公式 / 默认容差 / SCI/ALG 冻结定义；`VERSION` 常量（仍为 1）；
`docs/**`、`contracts/**`、`config/**`、`ci/checks.json`（域外，只登记）。

## 3 兼容策略（破坏性格式变更必须写清）

1. **旧文件不再可读（显式拒绝，fail-closed）**：旧 `.ahpx`（头 JSON 含 `"weight"` 字段，
   或块表存在 `weight` 数据块）在读取侧**显式拒绝** —— `AhpxReader::open()` 返回 `false`，
   `getRejectReason()` 给出原因；C ABI `aio_ahpx_read_header/read_pixels/read_snr` 返回非 0
   （既有错误码表内的 `2`/`3`，**未新造码值**）。
2. **不迁移、不静默丢弃、不静默忽略**：不提供旧文件自动迁移；不跳过旧字段继续读。
   **旧文件必须由 Phase1 重新生成**（normalize 的输入帧 → 新格式 HiPS）。
3. **写侧同样 fail-closed**：调用方元数据携带 `"weight"` 字段 ⇒ `AhpxWriter::write()` 拒绝
   且**不落盘**（既不产出读侧必拒的文件，也不静默丢弃调用方数据）。
4. **C ABI 变更**：`aio_ahpx_write` 去掉 `weight_mode`/`weight_data`/`grid_w`/`grid_h`；
   声明（`astro_image_io.h`）与定义（`aio_ahpx_api.cpp`）同形。**仓内无调用点**
   （`drizzle_engine` 早已改用 `hiss_write`，见 `docs/archive/history/...` 迁移记录）。
5. **版本号不作为生效条件**（最高设计 §12）：本变更以**变更编号 + 日期**
   `AHPX-WEIGHT-RETIRE-20260920` 登记；Alpha 前代码不含版本信息，`VERSION` 保持 1。
6. **数据面语义不变的部分**：像素与帧级 SNR 的读写、压缩编码、固定头布局、块索引语义
   **逐位不变**（回归锁 P1 断言往返逐位一致）。

## 4 验证证据（命令 + rc）

| 门 | 命令 | rc | 结果 |
|---|---|---|---|
| 静态清零 | `grep -rn "weight_mode" lib/infrastructure/aio/include/aio_ahpx_format.h lib/infrastructure/aio/src/ahpx/` | 1 | **输出为空**（无匹配） |
| 负例·读侧 | `./build/tests/unit/ahpx_hips_format_test --emit-legacy /var/tmp/astrocs/legacy_weight.ahpx && ./build/tests/unit/ahpx_hips_format_test --probe /var/tmp/astrocs/legacy_weight.ahpx` | 0 然后 **2** | 显式拒绝 + 原因（"头 JSON 含已作废的 \"weight\" 字段…"） |
| 负例·C ABI | `./build/tests/unit/ahpx_hips_format_test --probe-capi /var/tmp/astrocs/legacy_weight.ahpx` | **2** | `aio_ahpx_read_header` rc=2（非 0） |
| 正例 | `ctest --test-dir build -R ahpx_hips_format --output-on-failure` | 0 | Passed（36 断言） |
| 能红 | `ASTROCS_AHPX_FAULT=accept_legacy` / `=writer_accept_legacy_meta` / `=writer_drop_snr` 各跑一次 | 1 / 1 / 1 | 分别 7 / 2 / 6 条断言判红 |
| 构建 | `flock -w 7200 /var/tmp/astrocs/build.lock timeout 3000 ninja -C build` | 见 §4.1 | — |
| 全量测试 | `flock -w 7200 /var/tmp/astrocs/build.lock timeout 3000 ctest --test-dir build --output-on-failure` | 见 §4.1 | — |
| 机器门 | `timeout 900 python3 ci/run_checks.py --all --profile fast` | 见 §4.1 | — |

日志：`run/RELEASE-03/logs/FIX-202-*.log`。

### 4.1 收口记录（2026-09-20，FIX-202 执行者）

| 门 | 命令 | rc | 结果 |
|---|---|---|---|
| 全仓构建 | `flock -w 7200 /var/tmp/astrocs/build.lock timeout 3000 ninja -C build` | **0** | 增量构建通过，0 FAILED（`run/RELEASE-03/logs/FIX-202-ninja.log`） |
| 全量测试 | `flock -w 7200 /var/tmp/astrocs/build.lock timeout 3000 ctest --test-dir build --output-on-failure` | 8 | **468/471 通过（99%）**；`ahpx_hips_format`（#115）Passed；3 条红全部属**并发任务在飞改动**：`p3_projection_unsupported_cli`/`p3_projection_units`（FIX-205，工作区 `lib/algorithms/projection/**` 未定稿）、`p2002_unc_rej_prov`（FIX-204，工作区 `lib/algorithms/coverage/**rejection**` + 其测试未定稿）；与本任务文件域**无交集**（`run/RELEASE-03/logs/FIX-202-ctest.log`） |
| 机器门 | `timeout 900 python3 ci/run_checks.py --all --profile fast` | 1 | 40 项 35 PASS；5 项红：① `CHK-MODULE-MANIFEST` → `CTEST-REGISTRATION`：**本任务新增 ctest 目标 `ahpx_hips_format` 未登记 `ci/checks.json`**（含 FIX-205 的 3 个新目标）⇒ 见 §5.1，前台 BLD-201 登记；② `CHK-SCI-REF`(DOC-LINE-ANCHORS)、③ `STD-REG`(DISP-DRZ-004)、④ `CHK-CONFIG-DEFAULTS`(precision_mode)、⑤ `CHK-SPEC-NAMED-IMPL-ON-PROD-PATH`(SNI-S2-NS-01 台账过期) —— ②–⑤ 逐项 grep 对本任务文件 **0 命中**，均为并发任务在飞改动（`run/RELEASE-03/logs/FIX-202-runchecks-fast.log`） |
| 独立复跑（静态门） | `python3 tools/quality/check_ctest_registration.py` | 1 | 唯一与本任务相关的红：C3 `ahpx_hips_format <- tests/unit/CMakeLists.txt` 未注册 |

### 4.2 需前台登记的 CI 检查项（`ci/checks.json`，本任务不写该文件）

| ID | 命令（step） | 期望 rc | 说明 |
|---|---|---|---|
| `AHPX-WEIGHT-RETIRED`（建议；须带 `ctest_targets: ["ahpx_hips_format"]`） | `python3 tools/quality/deep_ci_driver.py ctest-target --build-dir run/ci/build-gcc-release --target ahpx_hips_format --output run/ci/ctest/ahpx_hips_format.json` | 0 | 行为门（N1/N2/P1/N3 共 36 断言）；负例面：`ASTROCS_AHPX_FAULT=accept_legacy` / `=writer_accept_legacy_meta` / `=writer_drop_snr` 各期望 **rc=1** |
| `AHPX-WEIGHT-RETIRED-STATIC`（建议，可并入同项一步） | `bash -lc "! grep -rn 'weight_mode' lib/infrastructure/aio/include/aio_ahpx_format.h lib/infrastructure/aio/src/ahpx/"` | 0 | 静态清零门；能红证据：同一 grep 对 `git show HEAD:<file>` 有命中（见 `run/RELEASE-03/logs/FIX-202-gates.log` G1-red） |

**登记前该门必红**（C3 fail-closed）—— 属预期，不是本任务缺陷。

## 5 域外镜像面（只登记，前台统一订正）

`aio_ahpx_write` 的**旧签名**仍被以下**域外**文件镜像（本任务不改）：

| 文件:行 | 内容 |
|---|---|
| `docs/architecture/api_inventory.csv:103` | 旧签名整行（含 `int weight_mode, const void *weight_data, int grid_w, int grid_h`） |
| `docs/contracts/API_CONTRACTS.csv:79` | `API-aio_ahpx_write` 行 |
| `tools/quality/contracts/fixtures/check_api_contracts/valid_min.csv:98` | 同上 |
| `tools/quality/contracts/fixtures/check_api_contracts/invalid_missing_symbol.csv:98` | 同上 |
| `tools/quality/contracts/fixtures/check_api_contracts/invalid_sig_mismatch.csv:98` | 同上 |

`lib/infrastructure/aio/Makefile:65`、`lib/infrastructure/aio/build.ps1:110` 编译 `src/ahpx/*.cpp`
（legacy 独立构建入口，不在根 CMake 图内）；`lib/infrastructure/aio/README.md:123` 为目录说明，
均不含旧签名，无需订正。

## 6 遗留与转派

1. **活 HiPS writer 的 `ASTROCS_WEIGHT_MODE`（已转 FIX-201）**：
   `lib/infrastructure/aio/include/aio_hips.h:222,236`、
   `lib/infrastructure/aio/src/hips/aio_hips_writer.cpp:530,1197,1557,1574,1591-1592,1603,1806,1815,1921,2104`、
   `lib/infrastructure/aio/tests/p1hips/p1hips_tests_diag_prov.cpp:59,67,172,232,327`、
   `lib/infrastructure/aio/tests/p2hips/p2hips_unc_prov_test.cpp:75,470` —— 属 `astrocs_hips`
   **生产目标**的 provenance 五键之一，与 A44 冲突；FIX-202 文件域外，**前台已转 FIX-201**。
2. `lib/infrastructure/scheduler/src/module_adapters.cpp:8321` 的 `ASTROCS_WEIGHT_MODE`：**前台已转 FIX-204**。
3. `lib/infrastructure/aio/memory.md:258,260` 的 `weight_mode` 描述（模块备忘，非合同）：域外，登记。
4. `lib/infrastructure/aio/v6/**`（`v6_provenance.h:124`、`v6_provenance.cpp:56,111` 的
   `weight_mode_version`）：v6 遗留合同面，随 v6 去留（DOC-201/Q2）统一处置，登记。
5. `.ahpx` 容器仍标注**已废弃**（`src/ahpx/DEPRECATED.md`，2026-07-13，由 `.hiss` 替代）；
   本变更不改变其废弃地位，只清除 A44 违规面。

## 7 规范依据清单

`ASTROCS_DESIGN.md` §2.1 / §3.4 / §12；`工程控制/RELEASE-02/GAP_AUDIT.md` §9.73 A44；
`工程控制/RELEASE-03/GAP_AUDIT.md` V02；`run/RELEASE-02/design-merge/DESIGN-DRAFT.md` §3.1-D；
`ENGINEERING_SPEC.md` §3 / §6 / §7 / §9；`AGENTS.md` §1.1 / §4 / §5 / §8；
`CONTROL_PACK_SPEC.md` §5 / §6 / §7；`工程控制/RELEASE-03/tasks/FIX-202.md`；`docs/ci/CI_SPEC.md` §3/§7。
