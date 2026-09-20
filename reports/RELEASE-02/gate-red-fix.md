# RELEASE-02 门禁回归修复分片回执 — GATE-RED-FIX（5 项红全绿）

- 分片：**GATE-RED-FIX**（执行面 = 全量门禁 5 项红的真修）
- 日期：2026-09-19；工作目录 `/workspace/Astro CS Database`
- 硬约束遵守：**零 git 写权限**（未 commit/add/checkout）；**未主动跑 ninja/cmake/ctest**
  （唯一构建动作来自门禁自身 step `CHK-WARN:WARNING-SUPPRESSION` → `cmake --build`，
  该 step 是 `--all` 的登记判据本身，见 §5）；未 waiver / 未删检查 / 未放宽判据 /
  未标 SKIP / 未改 `ci/checks.json` 去掉红项。
- 产物：`run/RELEASE-02/gate-red-fix/`（逐项 before/after 日志 + JSON + 复跑脚本）
- 依据：`docs/ci/01_CHECKS.md` §1/§4、`docs/ci/CI_SPEC.md`、`ENGINEERING_SPEC.md` §8、
  `docs/algorithms/anchors/ANCHOR_CONTRACT.md` §2/§4/§5、`docs/standards/CONCURRENCY_STANDARD.md`、
  `docs/architecture/THREAD_BUDGET_ARCH.md` §1、CI-REG-002（`tools/quality/check_ctest_registration.py` C1–C6）

---

## 0 结论速览

| # | 检查项（聚合 / step） | 修复前 | 修复后 | 根因类型 |
|---|---|---|---|---|
| ① | CHK-RESOURCE / SERIAL-HARDCODE | FAIL rc=1（`sky_plane.cpp:415 workers=1 硬编码`） | **PASS rc=0** | 死字段的字面量默认值（QA-002/P2-002） |
| ② | CHK-ORACLE / V6-RUNTIME-CLOSURE ＋ CHK-CONTRACT-TEST / V6-NEGATIVE-MUTATION | FAIL rc=1（`[MISSED] phase2-mode-flag-dropped anchor matched 0 times`） | **PASS rc=0**（mutations 12/12 detected） | 注入锚点代码形态漂移（语义仍在） |
| ③ | CHK-MODULE-MANIFEST / CTEST-REGISTRATION | FAIL rc=1（C3 未注册 5 个 add_test 目标） | **PASS rc=0**（unregistered=[]） | 新测试目标未同提交登记 CI 检查项 |
| ④ | CHK-SCI-REF / DOC-LINE-ANCHORS | FAIL rc=1（5 条 C4 BINDING_VIOLATION） | **PASS rc=0**（876 锚 OK:867 EXEMPT:9） | 行号锚漂移（符号移出锚范围） |
| ⑤ | CHK-WARN / WARNING-SUPPRESSION | 首次复跑 TIMEOUT（>60s 被杀） | **PASS rc=0（4.21s）** | 冷构建缓存（非判据问题），见 §5 |

**自证（全量门禁）**：

```text
修复前（前台基线）：verdict=FAIL entries=39 steps=84 pass=79 fail=5
本分片最终复跑　　：verdict=PASS entries=39 steps=84 pass=84 fail=0 timeout=0
                    prereq=0 skip_platform=0 skip_waivable=0
                    registry_sha256=125aa96284f5359e57b61911c332a224479ce7f308409f8a9936f6ba945a3cc5
```

复跑命令与证据：`run/RELEASE-02/gate-red-fix/ci-run.final.log`、`ci_result.final.json`。

**改动面（9 文件，科学行为变更 0 条）**：

| 文件 | 改动 |
|---|---|
| `lib/algorithms/coverage/include/astro/phase2/sky_plane.h` | 删死字段 `cpu_workers`（含注释论证） |
| `lib/algorithms/coverage/src/sky_plane.cpp` | 删 `c.cpu_workers = 1;` 字面量 |
| `tools/v6/v6_runtime_mutation_driver.py` | mutation 锚点改绑 + 台账注记（§2） |
| `ci/checks.json` | CHK-UNIT 登记 5 个 aio ctest 目标（ctest_targets + 5 step + outputs） |
| `docs/algorithms/{CALIBRATION_ALGORITHMS,COSMETIC_ALGORITHMS,NOISE_ESTIMATION,PHASE3_RSMP_IMPL,DRIZZLE_GEOMETRY}.md` | 仅行号锚同步（未动科学内容/结论/公式） |

---

## 1 ① CHK-RESOURCE / SERIAL-HARDCODE — `sky_plane.cpp:415 workers=1`

**根因（既有，非本轮 7ae4b449 引入）**：`git log -S'cpu_workers = 1'` 定位到
`324281c2`（RELEASE-02 FIX-A 稀疏天光面链）在 `p2_sky_plane_default_config()` 里写入
`c.cpu_workers = 1;`（旧行 415）。该字段在 `sky_plane.h:160` 自述「**预留**：1=串行 reference」。
取证结论：**该字段零消费者**——

- `grep -n cpu_workers lib/algorithms/coverage/src/sky_plane.cpp` → 只有那一处赋值，**无任何读取**；
- 生产路径 `module_adapters.cpp:5182-5205` 组装 `P2SkyPlaneConfig` 时逐字段拷贝，**不传 lease**
  （对比 `sc.cpu_workers = std::max(1, doc.value("__workers", 1))`（CON-004）/ `uc.cpu_workers`（CON-005）
  才是真 lease 注入）；
- 非配置文件键（`config/` 无 `sky_plane.cpu_workers`；`ci/ledgers/dead_config_keys.json` 亦无），
  非 ABI/契约面（`contracts/`、`docs/contracts/*`、`docs/architecture/api_inventory.csv` 零引用），
  无 ABI layout lock；
- 持久化面不受影响：`p2_sky_plane_save/open` 是**逐字段 JSON**，不序列化 `cfg` 结构体本体，
  且 `cfg` 段从不含 `cpu_workers`。

**为什么不能走 REGISTERED 登记（不放宽判据）**：`tools/check_serial_hardcode.py` 的
`REGISTERED` 表自述前提是「串行 reference 默认值，**生产路径由 ThreadBudget lease 注入覆盖**」
（现登记 3 处：`upm.cpp:256`/`sampler.cpp:320`/`p3_session.cpp:244`）。天光面**没有** lease 覆盖路径，
按该前提登记即为**虚假登记 = 放宽判据**；而 `SerialSection`（`include/astrocs/core/plan_estimator.h:49-61`）
只收 METADATA/IO_READ/IO_WRITE/FINALIZE 四类**短串行段**，明示「heavy kernel 不得入串行」，
天光面整面 Schur+IRLS 属 heavy，套用即错。

**修法（真修）**：删除该死字段与其字面量默认值，并在头文件写明「内在串行 + 将来并行化必须由
Runtime 预算/租约注入（形如 `P2SamplerConfig.cpu_workers`）」的约束（依据 CONCURRENCY_STANDARD
「线程数外部可配置，禁止硬编码」＋ THREAD_BUDGET_ARCH §1「线程预算唯一来源=Runtime lease」）。
判据本身一字未动。

**复跑**：

```bash
python3 tools/check_serial_hardcode.py
# 修复前：QA-002_VIOLATION: .../sky_plane.cpp:415 workers=1 硬编码        (rc=1)
# 修复后：QA-002_PASS: 无 workers=1/nside=2048/OMP 线程数硬编码; 无生产 GLOB; lease 覆盖默认登记 3 处  (rc=0)
```

编译自证（非门禁）：`g++ -fsyntax-only` 通过 `sky_plane.cpp` 与结构体消费方
`tests/unit/v6_p2_sky/v6_p2_sky_test.cpp`；更强证据：门禁 `CHK-WARN` 的
`cmake --build build --target astrocs` 在删除字段后**重编通过且生产警告 = 0**（§5）。

---

## 2 ② V6-RUNTIME-CLOSURE / V6-NEGATIVE-MUTATION — mutation 锚点改绑（**正式台账变更，非删除**）

### 2.1 台账变更记录（mutation ledger change record）

| 项 | 内容 |
|---|---|
| 台账载体 | `tools/v6/v6_runtime_mutation_driver.py::MUTATIONS`（该列表即注入锚点台账；W4-A3 两处改绑同此惯例） |
| mutation id | `phase2-mode-flag-dropped` |
| 守护语义（**不变**） | 若 phase2（用户命令名 `mosaic`，ASTROCS_DESIGN §2:317）丢失 `--mode` 显式模式旗标，Oracle 规则 `R2-phase2-mode-flag`（`v6_runtime_oracle.py::rule_r2_cli_flags`，判据 `"--mode" in flags`）必须判红 |
| 失效原因 | RELEASE-02 期 `a986691a`（2026-09-18）§8/21_observability §8.4 资源门旗标落地，`command_tree.h:85-87` 的 mosaic 行在 `--mode` 之后**追加** `--strict-resource-gate`/`--on-resource-gate`，锚点尾部 `--mode"}},` 形态消失（anchor matched **0** times，属「锚点漂移」而非「语义消失」） |
| 处置 | **改绑锚点**（语义不变，仅同步代码形态）；**未删除** mutation，未改 Oracle 判据，未改期望值 |
| 锚点 before | `'                             "-y", "--yes", "-force", "--events-jsonl", "--cpu-profile",\n                             "--mode"}},'` |
| 锚点 after | `'                             "-y", "--yes", "-force", "--events-jsonl", "--cpu-profile",\n                             "--mode", "--strict-resource-gate", "--on-resource-gate"}},'` |
| 注入后形态 | 同两行但**删去 `--mode`**（mosaic 的 allowed 集合缺 `--mode`，其余旗标保留） |
| 理由/证据 | `git show 7ae4b449 --stat` 未动 `command_tree.h`；漂移源＝`a986691a`（2026-09-18 20:08「fix(cli): 预检 fail-closed + -force 语义 + 合法键/资源门旗标」，`git log -S'--strict-resource-gate' -- lib/infrastructure/cli/command_tree.h` 唯一命中）；Oracle `R2-phase2-mode-flag` 判据面未变 |
| 台账注记位置 | `tools/v6/v6_runtime_mutation_driver.py:57-72`（RELEASE-02 改绑注记，含上表要点） |

> 说明：本仓 mutation 台账无独立 JSON/MD 文件（全仓 `grep -rn 'aggregate-pipeline-entry|phase2-mode-flag-dropped'`
> 仅命中 driver 自身），故「正式变更记录」落在**台账文件内的注记**＋本报告 §2；
> 若负责人要求另立 `工程控制/RELEASE-02/change-claims/` 条目，请指示（本分片不擅自新增控制包文件）。

### 2.2 能红能绿自证（改绑后仍具检测力）

```bash
python3 tools/v6/v6_runtime_mutation_driver.py --json-out run/RELEASE-02/gate-red-fix/05-mutations.json
# V6_RUNTIME_MUTATION_PASS detected=11/11 clean=PASS           (rc=0)
python3 tools/v6/check_v6_runtime_closure.py closure --json-out ... --oracle-out ... --mutations-out ...
# [PASS] oracle_all_rules rc=0
# [PASS] budget_selftest rc=0
# [PASS] mutation_driver rc=0                                  (rc=0)
```

逐条证据（`05-mutations.json`）：

- 绿面 `CLEAN_TREE`：`rc=0`，`V6_RUNTIME_ORACLE_PASS checks=2226 violations=0`（未注入 ⇒ 不报）；
- 红面 `phase2-mode-flag-dropped`：`detected=true, rc=1`，`V6_RUNTIME_ORACLE_FAIL checks=2226 violations=1`
  （注入 ⇒ 必报，且正是 `R2-phase2-mode-flag`）；
- 全量 `detected 12/12`（含 CLEAN_TREE 正例），无 `[MISSED]`。

---

## 3 ③ CHK-MODULE-MANIFEST / CTEST-REGISTRATION — 5 个 aio 测试目标未登记

**根因**：`01224779`（2026-09-18「激活 5 个零注册的 aio 顶层测试」）在
`lib/infrastructure/aio/tests/CMakeLists.txt:24-28` 新增 `foreach(_t checksum tile_model transform
query_pixel precision_dual)` → `add_test(NAME aio_${_t})`，并由 `tests/unit/CMakeLists.txt:503+`
`add_subdirectory(../../lib/infrastructure/aio/tests aio_toplevel)` 接入活动 CTest 面，
但**未同提交登记** `ci/checks.json` 的 `ctest_targets`，也不在 `ci/ctest_baseline.json`
（该基线 `base_commit=e6254d4d`，冻结于 CI-REG-002 建立时点 2026-09-12，**早于** 09-18 的这批新目标）。
CI-REG-002 C3 遂判红（`error_count=6`，5 个目标 + 1 条汇总）：

```text
C3   aio_checksum        <- lib/infrastructure/aio/tests/CMakeLists.txt
C3   aio_precision_dual  <- lib/infrastructure/aio/tests/CMakeLists.txt
C3   aio_query_pixel     <- lib/infrastructure/aio/tests/CMakeLists.txt
C3   aio_tile_model      <- lib/infrastructure/aio/tests/CMakeLists.txt
C3   aio_transform       <- lib/infrastructure/aio/tests/CMakeLists.txt
```

**修法（显式登记，非冻结收编）**：`ci/checks.json` 的 `CHK-UNIT`（「每模块单测」；`changed_paths`
已含 `lib/infrastructure/**`）登记 5 个目标，逐目标新增不可豁免的
`deep_ci_driver.py ctest-target` step（与 `CHK-ABI`/`CHK-INVARIANT`/`CHK-SYNTH-*` 既有 46 条
`CTEST-<TARGET>` 门同款），并同步 check 级 `outputs`：

- `ctest_targets` += `aio_checksum, aio_precision_dual, aio_query_pixel, aio_tile_model, aio_transform`；
- 5 个 step `CTEST-AIO-{CHECKSUM,TILE_MODEL,TRANSFORM,QUERY_PIXEL,PRECISION_DUAL}`
  （`profiles=[linux-main]`、`platform=linux`、`timeout_seconds=900`、`waivable=false`），
  **登记在 `CTEST-LINUX-FULL` 之后**（沿用「build dir 由登记顺序保证已构建」约定：
  `CHK-BUILD-LINUX` → `CTEST-LINUX-FULL` 先 configure/build）；
- check 级 `outputs` += `run/ci/ctest/aio_*.json`（与 step 级同平台语义，`ci/run.py`
  的 `_skip_outs`/`_kept_outs` 平台过滤不会误判缺产物）。

目标名真实性核对（只读列举，未执行测试）：主构建树 `ctest -N` 命中
`Test #148 aio_checksum`、`#149 aio_tile_model`、`#150 aio_transform`、`#151 aio_query_pixel`、
`#152 aio_precision_dual` —— 登记的 5 个名字与活动 CTest 面逐字一致。

**为什么不用 `--write-baseline`**：该开关是「CI-REG-002 建立时点**存量**目标的一次性过渡收编」，
把 09-18 新增目标塞进冻结基线会**架空 C3**（登记弱于冻结，冻结只保证不漂移）；且检查器自身注释
（`check_ctest_registration.py:55-59`）明确「显式登记强于冻结」。判据一字未动。

**复跑**：

```bash
python3 tools/quality/check_ctest_registration.py --output run/ci/ctest-registration/ctest_registration.json
# 修复前：verdict=FAIL error_count=6  unregistered=[aio_checksum, aio_precision_dual, aio_query_pixel, aio_tile_model, aio_transform]  (rc=1)
# 修复后：verdict=PASS error_count=0  unregistered=[] stale_baseline=[] dangling_patterns=[] pattern_not_in_command=[]
#         targets=329 sources=35 explicit=169 baseline_only=160                                                       (rc=0)
python3 tools/quality/check_ctest_registration.py --selftest
# verdict=PASS cases=8 failed=0（S1 新增未注册目标必红 / S4 陈旧模式必红 / S5 登记未真跑必红 /
#                              S6 基线漂移必红 / S7 真实仓回归必绿 —— 能红能绿自证）      (rc=0)
```

---

## 4 ④ CHK-SCI-REF / DOC-LINE-ANCHORS — 行号锚漂移（5 条 C4 BINDING_VIOLATION）

**根因**：锚定源码行号整体位移，符号仍在文件中但已移出该文档的锚范围（ANCHOR_CONTRACT §4 C4）。
漂移源＝本轮及前序 RELEASE-02 提交（`7ae4b449` 根 CMake +38 行、`98e529ec`/`324281c2` CMake 再增、
`7ae4b449` 的 `module_adapters.cpp` 大改、drizzle `4c599162` 的 +8 行位移）。逐条（旧 → 新，均由
「旧提交行内容 → 现文件唯一匹配行」逐行映射证明，脚本 `run/RELEASE-02/gate-red-fix/map_lines*.py`）：

| binding | 文档:行 | 锚（旧 → 新） | 新行内容（语义锚点不变） |
|---|---|---|---|
| P3RSMP-DESCRIPTOR | `PHASE3_RSMP_IMPL.md:37` | `module_adapters.cpp:678` → `:695` | `ModuleDescriptor p3_resample2_descriptor() {` |
| （同表相邻行，同类漂移，一并同步） | `PHASE3_RSMP_IMPL.md:47` | `module_adapters.cpp:429-443` → `:702-706` | p3_resample2 端口绑定 `DATA-P3-WCS`/`DATA-HIPS-001` 入、`DATA-P3-RES` 出 |
| NOISE-CMAKE | `NOISE_ESTIMATION.md:127,260` | `CMakeLists.txt:559-571` → `:620-632`；`主程序链接 :645` → `:706` | `add_library(astrocs_phase1_noise …` 块 → OpenMP 链接行；主程序链接行 |
| CAL-CMAKE | `CALIBRATION_ALGORITHMS.md:20` | `CMakeLists.txt:420-432` → `:446-465` | `add_library(astrocs_calibration …` 块首 → 块末 |
| CAL-CMAKE | `CALIBRATION_ALGORITHMS.md:242` | `CMakeLists.txt:420-425` → `:446-456` | 源清单段（`dark_optimizer.cpp` 不在清单） |
| CAL-CMAKE | `CALIBRATION_ALGORITHMS.md:285` | `CMakeLists.txt:399-411` → `:446-465` | 同行「构建」行；旧值已漂到 astrocs_aio 块（**指向错误**） |
| COS-CMAKE | `COSMETIC_ALGORITHMS.md:6` | `CMakeLists.txt:420-432` → `:446-465` | `astrocs_calibration` 构建块 |
| DRZ-MAXANGLE | `DRIZZLE_GEOMETRY.md:235` | `spherical_overlap.cpp:1083,1280-1281,1323-1324` → `:1091,1288-1289,1331-1332`；注释锚 `:993-999,:1070-1071` → `:1001-1007,:1078-1079` | `g.drop_area` 微小 drop 切平面分支、nb=4 重叠 `planar_polygon_area_n`、三角扇同策略 |

**纪律**：只改行号数字，**未改**任何公式/结论/容差/SCI 引述；`docs/science/**` 一字未动
（`DRIZZLE.md:98` 等 SCI 侧锚按 ANCHOR_CONTRACT §4.1「禁止反向修改 SCI」保持原样）。

**复跑**：

```bash
python3 docs/algorithms/anchors/check_doc_line_anchors.py --json-out run/ci/doc-anchors/doc_line_anchors.json
# 修复前：DOC_LINE_ANCHORS_FAIL 5 条 C4_symbol_binding BINDING_VIOLATION          (rc=1)
# 修复后：DOC_LINE_ANCHORS_PASS: 39 docs, 876 anchors, status=EXEMPT:9,OK:867      (rc=0)
python3 -m unittest discover -s tests/quality -t tests/quality -p 'test_doc_line_anchors.py'
# Ran 14 tests ... OK（含 f03_symbol_moved_out_of_range 注入必红 + f11 还原必绿）    (rc=0)
```

---

## 5 ⑤ 复跑中出现并已解释的 `CHK-WARN / WARNING-SUPPRESSION` TIMEOUT（非判据红）

首次全量复跑出现 `pass=83 fail=0 timeout=1`，唯一 timeout 为 `CHK-WARN:WARNING-SUPPRESSION`
（登记超时 60s，实测被杀）。**根因是冷构建缓存，不是判据问题**：

- 该 step 的判据包含 `tools/check_warning_suppression.py::measure_build()`：`touch` 一个生产源后
  执行 `cmake --build build --target astrocs` 并统计 `warning:` 数；
- ① 删除了 `sky_plane.h` 的字段 ⇒ 该公共头的时间戳变化使全部包含它的 TU 失效，
  首次复跑承担**冷增量重编**（`module_adapters.cpp` 等）＝64.4s > 60s ⇒ 被 runner 判 TIMEOUT；
- 重编完成后同一命令 **4.22s PASS**（`06-warning-suppression.rerun.txt`）；第二次全量复跑
  `WARNING-SUPPRESSION PASS rc=0 t=4.21s`，全量 `timeout=0`。

**未做的处置**：没有上调该 step 的 `timeout_seconds`（那属于放宽门禁预算，须负责人裁决），
也未 waiver。**上呈事项**：`CHK-WARN` 的 60s 预算对「改了被广泛包含的生产头」的场景偏紧
（冷重编 64s）；是否把该 step 预算调到能覆盖一次冷增量重编（例如 300s），请负责人裁决。

> ✅ **订正（2026-09-20，V5 分片 3）—— 已裁决，上呈事项闭合**：§9.49 定案 6（`工程控制/RELEASE-02/GAP_AUDIT.md:1475` 负责人答「**a**」（=上调）+ 定案 6 `:1497-1498`「`CHK-WARN` 预算上调至 **180 s**（覆盖一次冷增量重编）」）+ §9.50 定案 6（`:1559`）+ §9.64 C（`:2378`「预算 → **180 s**（负责人 §9.50 已批）**PASS**」）。⇒ 上文「须负责人裁决 / 请负责人裁决」**作废**；`ci/checks.json` 的 `CHK-WARN` 预算已是 180 s。注：本条上呈建议的「例如 300s」未采纳，实际取 **180 s**。

---

## 6 自证与证据清单

```text
最终全量门禁：verdict=PASS entries=39 steps=84 pass=84 fail=0 timeout=0 prereq=0
              skip_platform=0 skip_waivable=0   (rc=0)
```

逐 step（本轮 5 项红，取自 `ci_result.final.json`）：

| 聚合 | step | verdict | rc | t |
|---|---|---|---|---|
| CHK-RESOURCE | SERIAL-HARDCODE | PASS | 0 | 0.52s |
| CHK-ORACLE | V6-RUNTIME-CLOSURE | PASS | 0 | 2.44s |
| CHK-CONTRACT-TEST | V6-NEGATIVE-MUTATION | PASS | 0 | 2.45s |
| CHK-MODULE-MANIFEST | CTEST-REGISTRATION | PASS | 0 | 3.35s |
| CHK-SCI-REF | DOC-LINE-ANCHORS | PASS | 0 | 1.24s |
| CHK-WARN | WARNING-SUPPRESSION | PASS | 0 | 4.21s |

证据文件（`run/RELEASE-02/gate-red-fix/`）：

| 文件 | 内容 |
|---|---|
| `ci-run.final.log` / `ci_result.final.json` | **最终全量门禁** rc=0 / fail=0 |
| `ci-run.after.log` / `ci_result.after.json` | 首次全量门禁（83 PASS + 1 冷构建 TIMEOUT，见 §5） |
| `01-serial-hardcode.{before,after}.txt` | ① 前后对照 |
| `05-v6-closure.{before,after,final}.txt`、`05-mutations.json`、`05-oracle.json`、`05-closure.json` | ② 前后对照 + 12/12 检出 |
| `02-ctest-registration.{before,after}.{txt,json}`、`02-ctest-registration.selftest.json` | ③ 前后对照 + 负例自检 8/8 |
| `04-doc-anchors.{before,after}.{txt,json}`、`04-anchor-debt-scan.txt` | ④ 前后对照 + 存量锚债扫描（§7） |
| `06-warning-suppression.rerun.txt` | ⑤ 冷/热构建对照 |
| `map_lines*.py`、`inspect_anchors.py`、`scan_drift.py` | 行号映射/漂移扫描复算脚本（可复跑） |

---

## 7 未修 / 上呈负责人事项（如实登记，不掩盖）

1. **存量行号锚债（非本轮 5 红，判据面之外）**：C4 只覆盖 `bindings` 声明的 42 个符号；
   扫描（`04-anchor-debt-scan.txt`）发现同一批文档中**未被 binding 覆盖**的锚也已漂移，例如
   `DRIZZLE_GEOMETRY.md:6`（`CMakeLists.txt:356-366` → 现 429）、`:143`（`379-382` → 现 532）、
   `PHASE3_RSMP_IMPL.md:44`（`CMakeLists.txt:460-465` → 现 718）、
   `CALIBRATION_ALGORITHMS.md:136,345,350`（`module_adapters.cpp` 系 ARCH-001 迁移前锚）、
   `DRIZZLE_GEOMETRY.md:83`（`spherical_overlap.cpp:1639-1653` → 现 1699-1713）、`:170,:184` 等。
   这些锚在门禁上「不可见」（无 binding ⇒ 不判红），修它们等于做一次全量 SCI-ANCHOR 复测，
   超出本分片「5 项红」范围，**未擅自扩大改动面**；建议单开一片（或给这些符号补 binding）。
2. **文档事实性计数过期（未改，属内容而非锚）**：`CALIBRATION_ALGORITHMS.md:285` 的
   「STATIC，**4 个 cpp**」在本轮 `7ae4b449` 把 `photometry_apply.cpp` 编入
   `astrocs_calibration`（现 5 个 cpp）后已不准确。按本分片约束「docs/algorithms 只允许同步锚点/台账」，
   **未改该措辞**，登记为负责人/文档分片事项。
3. **`CHK-WARN` 60s 预算 vs 冷增量重编 64s**：见 §5，~~是否调预算请裁决~~（本分片未动注册表预算）。 ⇒ **已裁决（§9.49 定案 6 / §9.50 定案 6；订正 2026-09-20，V5 分片 3）**：预算 → **180 s**，§9.64 C 实测 **PASS**。
4. **② 台账变更形式**：已落在 `tools/v6/v6_runtime_mutation_driver.py` 内的正式注记（§2.1）；
   若要求另立控制包 change-claim 文件，请指示。
5. **linux-main 面未跑**：本分片未跑 `--profile linux-main`（会触发 `cmake`/`ninja`/`ctest`，
   超出「不跑构建」约束）。新增的 5 个 `CTEST-AIO-*` step 只在 `linux-main` 生效
   （与既有 46 条 `CTEST-<TARGET>` 门同款）；其前置构建由 `CHK-BUILD-LINUX`/`CTEST-LINUX-FULL`
   保证。**请前台在统一构建后跑一次 `--profile linux-main`** 复核这 5 条
   （当前 `run/ci/build-gcc-release` 是 09-18 前的旧构建树，未含 `aio_*` 目标）。
