# RELEASE-02 CI / 测试卫生修复报告

> 任务：门禁与测试卫生修复者（RELEASE-02）。工作目录 `/workspace/Astro CS Database`。
> 文件面：`ci/**` 与 `tests/**`（`tests/unit/p1001_real_nodes_test.cpp` 由 P0-21 分片认领，本包未动）。
> 时点：2026-09-18。零 git 写权限；`TMPDIR=/dev/shm/astrocs_ci`；未跑全量 ctest/ninja。
> 依据：`ASTROCS_DESIGN.md` §11/§12、`ENGINEERING_SPEC.md` §8、`AGENTS.md` §8、
> `reports/RELEASE-02/DESIGN-CONFORMANCE/{SUMMARY,SUB-D-verification}.md`。

---

## 0. 结论速览

| 缺陷 | 处置 | 状态 |
|---|---|---|
| 1. CI 版本门方向错误 | 改 `ci/check_version.py` 判据 + `ci/checks.json` 命令；新增 absence 模式正/负例 | **已闭合**（版本号本身未动） |
| 2a. 12 个 SKIP | 逐项查因；本节点可启用 0 条；登记 `ci/ctest_skip_register.json` + 本报告 | **已登记**；2 条属测试设计缺陷（lib/**，未闭合） |
| 2b. `test_golden_parity` 静默 PASS | 该函数在 `tests/unit/p1001_real_nodes_test.cpp`（P0-21 分片文件面） | **移交**（本包未动，见 §2.2） |
| 2c. Python 测试聚合器 | 新增 `ci/check_test_index.py` + `tests/test_index.csv` 机器 verdict/计数列 + CHK-UNIT 步骤 | **已闭合**（聚合实况=FAIL，见 §2.3） |
| 2d. 1/N 一致性 6 个 0 字节文件 | 查清成因（占位 stub 未被 02:52 运行覆写）；报告显式标记未完成 | **已查清/未闭合**（run/** 非本包文件面） |
| 3. mutation 类 ctest 仅 1 条 | 实测登记 `ci/mutation_gates.json` | **已登记**（§11.1 DIVERGENT 未闭合） |

---

## 1. 缺陷 1：CI 版本门方向错误（最严重）

### 1.1 缺陷与证据

- 设计要求：`ASTROCS_DESIGN.md:616`「**Alpha 之前：程序与代码中不包含任何版本信息**」。
- 旧实现方向相反（CI 绿灯 = 必然违反 §12）：
  - `ci/check_version.py:121-128`（旧）`ANCHORS` 含 `VERSION_REL`；`check_anchors` 对缺失路径判 `ANCHOR_STALE` ⇒ `exit 2`。
  - `ci/checks.json:6470-6480`（旧）`VERSION-CONSISTENCY` 硬编码 `--expected 0.11.0-alpha.2`，**强制版本串存在**。
  - `ci/checks.json:6475` 同族 `VERSION-NAMESPACES`（`tools/doccheck/check_version_namespaces.py:9` 第 1 条「根 VERSION 存在且匹配」）同样强制存在 —— 该文件在 `tools/**`，**不在本包文件面**，见 §6。

### 1.2 改法（只改门方向，不改版本号）

`ci/check_version.py`：

1. 新增版本信息存在性探测 `detect_version_presence()`（`:326`）与 `_alpha_hit()`（`:315`）：
   判据 = 根 `VERSION` 非空 ∪ CLI/根 CMake 出现 alpha 字面量 ∪ `project(... VERSION ...)` ∪
   `file(READ .../VERSION ...)` 生成链 ∪ `@ASTROCS_VERSION_STRING@` 占位 ∪ 活动文档 alpha 字面量（`REV_FIELD` 机器修订字段照旧豁免）。
2. `main()`（`:442-453`）：**不存在** ⇒ 进入 absence 模式，跳过版本专用锚 `VERSION_ANCHOR_NAMES=("VERSION_REL","CLI_TEMPLATE_REL")`（`:158`；`check_anchors(skip=...)` `:225`，显式留痕 `SKIPPED(无版本信息)`），仍校验非版本锚与文档集完整性（防移空），输出
   `version_absence_alpha_pre`（`:447`）PASS 并 **exit 0**。
3. **存在** ⇒ 转入 `[1]~[6]` 一致性校验（与旧行为等价，fail-closed），并输出 `version_presence_detected`（`:469`）留痕。
4. 自测新增 3 例（`:790-807`）：`pos_absence_no_version`、`pos_absence_explicit_expected`（显式 `--expected` 也不得强制存在）、`neg_absence_literal_present`（有字面量必判红）。

`ci/checks.json`：`VERSION-CONSISTENCY` 命令由
`python3 ci/check_version.py --expected 0.11.0-alpha.2` 改为 `python3 ci/check_version.py`（`:6479`），
expected 唯一取自根 `VERSION`；同时消除 CI 配置里手抄的版本字面量。

### 1.3 验证证据

- `python3 ci/check_version.py --self-test` ⇒ **10/10 OK**（含旧 7 例负例全保留）。
- 真仓 `python3 ci/check_version.py` ⇒ `verdict=VERSION_CHECK_PASS, exit 0, pass=27 fail=0, expected=0.11.0-alpha.2 (VERSION --expected 缺省)`。
- `tests/version/test_adopt006_version_gate.py` 新增 `test_12_absence_no_version_info_passes` / `test_13_absence_tree_with_version_literal_fails`；`tests/version` 实测 **31 tests OK**。

### 1.4 口径说明

本修复**不改变仓库当前仍含版本信息的事实**（`VERSION=0.11.0-alpha.2`，负责人指示本包不动版本号）。
故当前 CI 绿灯仍对应「版本信息存在且一致」，而 §12「Alpha 前无版本信息」的去留属负责人裁决
（DC-718 / 控制包 P0-15「全部验收通过后再改」）。本修复消除的是「**门方向**把版本不存在判红」这一反向固化。

---

## 2. 缺陷 2：测试绿灯不可信

### 2.1 12 个 SKIP 逐项处置

基线：`run/RELEASE-02/DESIGN-CONFORMANCE/evidence/LastTestsDisabled.log`（2026-09-18 16:47 轮，12 条）。
机器登记：`ci/ctest_skip_register.json`（类别 / 源 file:line / owner / 解除条件）。

| # | SKIP 用例 | 类别 | 源 file:line | 本节点能否启用 | 处置 |
|---|---|---|---|---|---|
| 1 | `cpu001_selftest_avx512` | HOST_CAPABILITY | `tests/unit/cpu001_provider_selftest.cpp:41-48` | 否 | 本节点 `/proc/cpuinfo` 无 `avx512*`；sanctioned `exit 77`（ctest `SKIP_RETURN_CODE`）。非充数 SKIP |
| 2 | `Phase2Acr.CudaEquivalent` | DORMANT_SUBSYSTEM | `lib/algorithms/coverage/tests/synthetic_gate.cpp:3342` | 否 | CUDA bridge 不可用（ACR dormant，纯 CPU 生产） |
| 3 | `Phase2Acr.CudaWeightedSupportEquivalent` | DORMANT_SUBSYSTEM | `…synthetic_gate.cpp:3387` | 否 | 同上 |
| 4 | `Phase2Acr.G9CompactFrameSubset` | DORMANT_SUBSYSTEM | `…synthetic_gate.cpp:3465` | 否 | 同上 |
| 5 | `Phase2Acr.G9WinsorizedCpuRoute` | DORMANT_SUBSYSTEM | `…synthetic_gate.cpp:3523` | **是（测试设计缺陷）** | 核心断言「winsorized 必须 CPU_ROUTE」**不需要 GPU**；:3521-3523 的 CUDA 前置把整例 gate 掉。归 lib/**，本包不改 |
| 6 | `Phase2Coverage.RealHipsUnion` | FIXTURE_MISSING | `…synthetic_gate.cpp:3601` | 否 | 需 `run/temp/phase1_freeze`（不存在；FIX-B） |
| 7 | **`Phase2Coverage.FilterMismatchRejected`（负例）** | FIXTURE_MISSING | `…synthetic_gate.cpp:3634-3646`（skip 前置 :3638） | **是（测试设计缺陷）** | 用例体只断言 `p2_coverage_build(不存在路径)!=0`，**不需要 fixture**；:3638 前置多余 ⇒ 负例保护当前为零。归 lib/** |
| 8 | `Phase2Sampler.RealHipsControlSampling` | FIXTURE_MISSING | `…synthetic_gate.cpp:3653` | 否 | 需 fixture |
| 9 | `Phase2Sampler.G6LocalSnrAvailabilityThreeZones` | FIXTURE_MISSING | `…synthetic_gate.cpp:3705`（TEST :3697） | 否 | 需 fixture |
| 10 | `Phase2Identity.G3StableFrameIdentity` | FIXTURE_MISSING | `…synthetic_gate.cpp:3852`（TEST :3847） | 否 | 需 fixture |
| 11 | `Phase2Identity.G3ManifestOrderCanonical` | FIXTURE_MISSING | `…synthetic_gate.cpp:3959`（TEST :3954） | 否 | 需 fixture |
| 12 | `Phase2SamplerParallel.OneTvsTwoTDeterminism` | COMPENSATED | `lib/algorithms/coverage/tests/sampler_parallel_consistency_test.cpp:45`（TEST :40） | 否 | 真实 fixture 缺；同文件合成等价门 `OneTvsTwoTDetermine`（:80+）在 Linux 跑位精确断言 |

**汇总**：HOST_CAPABILITY 1 / DORMANT_SUBSYSTEM 4 / FIXTURE_MISSING 6 / COMPENSATED 1；
在本包文件面（不改 `lib/**`）与无真实 fixture 前提下，**可启用 0 条**。
其中 **#5、#7 属测试设计缺陷**：删除多余前置即可在无 GPU/无 fixture 节点执行 —— 已在登记表点名，交 lib/** owner。
**#7 负例被跳过是本次最不可接受的 SKIP**（负例保护为零），建议最高优先级解除。

### 2.2 `test_golden_parity` 环境未设即静默 PASS

- 位置：`tests/unit/p1001_real_nodes_test.cpp:2234-2238`（`if (!dump && !cmp) { printf("skipped"); return; }`）。
- 该文件**已由 P0-21 分片认领**，本包按任务约束**未修改**。
- 处置：**移交**。修复口径（供 P0-21 分片/前台执行）：环境未设时改为
  `GTEST_SKIP() << "P1001_GOLDEN_DIR/P1001_GOLDEN_CMP 未设"`（显式 SKIP 标注）或直接 FAIL，
  二者皆可；**禁止** `printf + return` 静默 PASS。
- 本包已用报告 + 本条显式登记，不让它静默消失。

### 2.3 Python 测试聚合器：`test_index.csv` 判据修正

**缺陷**：`tests/test_index.csv` 旧版只有自由文本 `notes`，无机器 verdict；多套件含
errors/failures/skip（api 2 error、arch 1 fail、config 3 fail、abi 1 fail）仍被叙述为「Python 测试绿灯」
（DESIGN-CONFORMANCE SUB-D-35）。

**修正**（三件套）：

1. `tests/test_index.csv` 新增机器列 `verdict,cases,failed,errored,skipped,measured_utc`，
   23 行逐条按 2026-09-18 实测填写；更正 3 处陈旧 notes（`tests/config` 旧写「40 用例 OK」实为
   50 用例 3 failures；`tests/version` 旧写 failures=2 实为 31 OK；`tests/arch` 旧写 29 用例实为 40）。
2. 新增 `ci/check_test_index.py`（stdlib，`--self-test`）把聚合判据机器化：
   - R1 列齐全；R2 path 唯一且必须登记 `tests/config`（CFG002-07 口径）；
   - R3 verdict ∈ {PASS,FAIL,ENV_FAIL,SKIP_ONLY,HOSTED,HEAVY,SCRIPT}；
   - **R4 `PASS ⇔ failed==0 ∧ errored==0`**；`FAIL/ENV_FAIL ⇒ failed+errored>0`；
   - **R5 全 skip（`cases>0 ∧ skipped==cases`）不得记 PASS**，必须 `SKIP_ONLY`；
   - R6 任何 errors/failures/skip 必须有 notes（不得静默）；R7 存在 `SKIP_ONLY` 判红。
   退出码 0 = 标注自洽；1 = 判据违反 / SKIP_ONLY；2 = 输入不可用。
3. 注册为 `ci/checks.json:2408-2433` CHK-UNIT 的步骤 `PY-TEST-INDEX-VERDICTS`；
   `ci/id_migration_map.json` 同步登记（MERGED-INTO CHK-UNIT，absorbed）。

**验证**：`--self-test` **11/11 OK**（含「PASS 带 failure 判红」「全 skip 冒充 PASS 判红」
「FAIL 无计数判红」「缺列/重复 path/缺 tests/config 判红」）；真仓运行
`verdict=TEST_INDEX_CONSISTENT, exit 0, aggregate_verdict=FAIL, aggregate_is_green=false`。

**诚实口径**：聚合 verdict = **FAIL**（api/arch/config/abi 四套件非绿），本门不把它洗成绿；
套件执行面由 CHK-UNIT 的 UT-* 步骤判红，本门只保证聚合标注不会把红标成绿。

### 2.4 1/N worker 一致性目录的 6 个 0 字节文件

- 位置：`run/RELEASE-01/e2e/evidence/consistency__p1_gc_panel1_red_w1/`
  （`alloc_report.json`、`alloc_samples.csv`、`astrocs_run_d20d79d0ccf1.json`、`resource_summary.json`、
  `resource_timeseries.csv`、`worker_balance.csv`，均 0 B）。
- 同目录 `run_context.json` = 231 B（`run_id=d20d79d0ccf1`，`source_sha=41b41e2d…`）。
- **成因（查清）**：6 个 0 B 文件 mtime = `09-18 00:08`，而该 1W 运行发生在 `02:52:48`（`1N-consistency-batch.log`）——
  即目录在 00:08 被预建为空占位 stub，02:52 的运行**未覆写**这些文件；真实对比证据落在别处：
  `1N-consistency-compare.txt`（非空，`1w artifacts: 17 / Nw artifacts: 17 / byte-identical 15/17`）。
- **结论**：1/N 一致性的**产品哈希对比实际做过**（compare.txt 有效），但该 1W 运行的
  资源/分配/worker 平衡证据**是空壳**。§11 声称的「1/N 一致性」在资源面**未闭合**。
- 处置：`run/**` 非本包文件面（`ci/**`/`tests/**`），**显式标记未完成**；解除条件 = 以资源监控包装重跑该 1W 运行并产出非空 CSV/JSON。
- 附带：全仓 `run/RELEASE-01/e2e/evidence` 共 406 文件，其中仅这 6 个为 0 B。

---

## 3. 缺陷 3：mutation 类 ctest 覆盖面（不虚报）

- 设计声称：`ASTROCS_DESIGN.md:565`「每个近似有 mutation 证明门能红」。
- **实测（2026-09-18）**：
  - 默认 ctest mutation 类用例 **1 条**：`v6_aio_impl_mutations`（`tests/unit/v6_aio/CMakeLists.txt:55-58`；`build/v6_aio/CTestTestfile.cmake:23`）。
  - 默认 **OFF** 的 ctest mutation driver 1 条：`v6_p3_rsmp_mutation_driver`（`tests/unit/v6_p3_rsmp/CMakeLists.txt:43-50`，`option(V6_P3RSMP_ENABLE_MUTATION_DRIVER … OFF)`）。
  - Python CI 门 1 条：`V6-NEGATIVE-MUTATION`（`ci/checks.json`；`run/v6/runtime-closure/mutations_ci.json` = `detected=11/11 clean=PASS`）。
  - Python unittest 门 2 条：`tests/contracts/v6/test_v6_negative_mutations.py`（`tests/contracts/v6/evidence/mutations.json` = `total=34 detected=34`）、`tests/quality/test_docchk002_mutation.py`。
  - `run/**` 一次性 driver 4+：`p2-samp/oracle/run_mutations.py`、`IMPL-P2-REJ-001/run_mutations.py`、`IMPL-P1-DRZ-001/mutation_driver.py`、`IMPL-P2-UPM-001/mutations/`；PSFW 以 `ASTROCS_P1PSFW_FAULT` 故障注入进 `v6_p1_psfw_selfcheck`。
- 处置：新建 `ci/mutation_gates.json` 登记「近似 ↔ mutation 门」实况（含 `in_ctest/default_on/reason/open_items`），
  **不虚报覆盖率**；结论：远小于「每个近似」，属 §11.1 **DIVERGENT（部分）**，未闭合。
- 未闭合项：把关键 driver（p3rsmp/p2-samp/REJ/DRZ/UPM）纳入 ctest/CI；给本登记加机器门（当前为人工登记）。

---

## 4. 前台转交项（本包同步处理）

### 4.1 P0-19：`hips_frame=icrs` 断言反转（来自 P3/aio 分片）

- `tests/unit/p1_hips_writer_test.cpp:139-140`：由「断言 `icrs` 为标准、否定 `equatorial`」反转为
  「断言 `equatorial` 为标准、否定 `icrs`」（IVOA REC-HIPS-1.0 §4.4.1；aio 写侧已改 `equatorial`）。
- 其他固化 `icrs` 的测试已排查并同步：
  - 合成 HiPS fixture 由 `hips_frame = icrs` 改 `equatorial`：`tests/unit/p3002_uncertainty_test.cpp:107`、
    `tests/unit/rt001_unique_executor_test.cpp:142`、`tests/unit/p3002_real_nodes_test.cpp:88`；
  - `tests/unit/v6_aio/v6_aio_test.cpp:178,577`：`hp.frame/m.frame` 改 `equatorial`（v6 头文件默认值已为 equatorial）。
- **不属 P0-19 的保留项**（已复核）：`coordinate_frame="icrs"` / `CoordinateFrame::ICRS` 是产品坐标系枚举（ICRS 合法），
  非 HiPS `hips_frame` 关键字值；`tests/artifact/test_phase_product_exchange.py:258` 的「非 icrs 拒绝」同理。

### 4.2 CLI 预检 fail-closed：两个用例期望同步（来自 CLI 分片）

CLI 预检修复后，路径不存在/不可读在 `precheck_config` 阶段（`lib/infrastructure/cli/subcommand.h:304-309`）
即以 `astrocs::INPUT`(3) 阻断，**在 `session_dispatch`/写 manifest 之前返回**，`-y` 不可越：

- `tests/cli/test_cli_protocol.py`：`TestManifestIncomplete` 改判「rc=3、**0 个 manifest**、stdout 无事件、stderr 点名 `missing/unreadable input path`」；
  用例更名 `test_01_missing_input_blocked_before_manifest`（`:257`），移除不再使用的 `hashlib` import。
- `tests/cli/test_cli004_process_protocol.py:189-203`：1b 段由「rc=3 + 完整事件流 + incomplete manifest」
  改为「rc=3、stdout 空、0 个 manifest、stderr 点名」。
- **为什么新期望才对**：`ASTROCS_DESIGN.md` §3.5 把「输入路径不存在/不可读」列为 error；
  `ENGINEERING_SPEC.md:122` 要求检查器在输入缺失时判红（fail-closed）。旧断言固化的是修复前的 fail-open 行为
  （先写 incomplete manifest 再在运行期报错），把「应阻断」写成了「应落盘」。
- 验证限制：两个 CLI 用例需 `astrocs` 二进制（构建由前台统一做），本包只做 Python 语法编译与逻辑核对。

### 4.3 CLI 分片列出的未闭合项（不修，纳入本报告）

- `config/templates/*.phase_config.json` 仍为嵌套 `phase_name/config/inputs`、顶层无 `output_dir` ⇒ 预检结构错 rc=2（阻断级 DC-514）；模板内容已授权 P3/aio 分片，schema↔CLI 形态裁决归前台。
- `algorithm_weight_mode` / `algorithm_upm_gauge` / `algorithm_psf_model` / `sparse_snr_layer` 仍 unknown key（DC-426/309）——列入 hub 批。
- `-y` 不显示预检页、无 warn/optimize 生产者（DC-311）——后续批。

---

## 5. 验证记录（本包实际执行）

| 命令 | 结果 |
|---|---|
| `python3 ci/check_version.py --self-test` | 10 cases, ALL OK |
| `python3 ci/check_version.py`（真仓） | `VERSION_CHECK_PASS`, exit 0, pass=27 fail=0 |
| `python3 -m unittest discover -s tests/version -t tests/version` | Ran 31 tests, **OK** |
| `python3 ci/check_test_index.py --self-test` | 11 cases, ALL OK |
| `python3 ci/check_test_index.py`（真仓） | exit 0, `aggregate_verdict=FAIL`, 23 行自洽 |
| `python3 ci/validate_registry.py --registry ci/checks.json --strict` | `verdict=PASS`, errors=0 |
| `python3 ci/check_registry_doc_sync.py` | `REGISTRY_DOC_SYNC_PASS`（46==46） |
| `python3 ci/run_checks.py --check VERSION-CONSISTENCY --quiet` | PASS |
| `python3 ci/run_checks.py --check PY-TEST-INDEX-VERDICTS --quiet` | PASS |
| `python3 ci/run_checks.py --check CHK-DANGLING / CHK-STATIC --quiet` | PASS |
| `python3 -B -m unittest discover -s tests/{glossary,contracts,artifact,io,monitoring,pipeline,runtime,sciencelint,traceability}` | OK |
| `python3 -B -m unittest discover -s tests/{api,arch,config,abi}` | 非绿（见 §2.3，如实登记） |
| `python3 -m py_compile` on changed `.py` | OK |

未跑：全量 `ninja`/`ctest`（按任务约束由前台统一构建）；需构建的 `tests/backend`/`tests/cli`/`tests/unit` 未在本节点执行。

---

## 6. 仍不可用 / 未闭合项（显式登记）

| # | 项 | 位置 | 性质 | owner / 解除条件 |
|---|---|---|---|---|
| O1 | §12 版本信息仍在仓库 | `VERSION`、`lib/**` module_version 等 | 负责人裁决项（本包不动版本号） | 负责人（P0-15/DC-718）；全部验收通过后再改 |
| O2 | `VERSION-NAMESPACES` 仍强制 VERSION 存在 | `ci/checks.json:6475` + `tools/doccheck/check_version_namespaces.py:9` | 同 §12 反向固化；**文件在 tools/**，非本包文件面** | GOV-001 / 负责人；需与 §12 生效时点一并裁决 |
| O3 | `docs/ci/01_CHECKS.md:52` 仍写 `--expected 0.11.0-alpha.2` | docs/ | 文档漂移（本包不改 docs） | 文档分片：同步为无 `--expected` 形态 |
| O4 | 负例 `FilterMismatchRejected` 仍被 skip | `lib/algorithms/coverage/tests/synthetic_gate.cpp:3638` | 负例保护为零 | lib/**：删多余 fixture 前置 |
| O5 | `G9WinsorizedCpuRoute` 被 CUDA 前置整例 gate | `…synthetic_gate.cpp:3521-3523` | 无 GPU 也能跑的断言被跳过 | lib/**：拆出 CPU_ROUTE 断言 |
| O6 | 6 条真实 HiPS fixture 用例 skip | `…synthetic_gate.cpp:3601/3653/3705/3852/3959`、`sampler_parallel_consistency_test.cpp:45` | 缺 `run/temp/phase1_freeze` | FIX-B / 真实数据 fixture |
| O7 | `test_golden_parity` 环境未设静默 PASS | `tests/unit/p1001_real_nodes_test.cpp:2234-2238` | 假绿 | **P0-21 分片**（本包文件面外） |
| O8 | 1/N 一致性 6 个 0 字节证据 | `run/RELEASE-01/e2e/evidence/consistency__p1_gc_panel1_red_w1/` | 空证据 | 前台：资源监控包装重跑该 1W 运行 |
| O9 | Python 套件非绿：api(2 err)/arch(1 fail)/config(3 fail)/abi(1 fail) | 见 `tests/test_index.csv` | 真实失败 | api/abi→生产头/构建树；arch→生成器幂等；config→`config_registry.json`/docs（非 tests/** 文件面） |
| O10 | mutation 门多数不进 ctest/CI | `ci/mutation_gates.json` | §11.1 DIVERGENT（部分） | 各域 owner；把关键 driver 纳入 ctest/CI |
| O11 | SKIP 登记尚无机器门 | `ci/ctest_skip_register.json` | 人工登记 | 建议新增 checker（读 ctest JUnit skipped 集，未登记即 FAIL） |
| O12 | `CHK-ROOT-CLEAN` 当前 FAIL | 顶层 `path/to/` 目录（非本包产生） | 其他分片运行产物残留 | 前台清理 |

---

## 7. 本包改动文件清单

**新增**
- `ci/check_test_index.py`（Python 测试聚合判据门，stdlib + `--self-test`）
- `ci/ctest_skip_register.json`（12 条 SKIP 显式登记）
- `ci/mutation_gates.json`（mutation 覆盖面实况登记）
- `reports/RELEASE-02/CI-hygiene-report.md`（本文件）

**修改**
- `ci/check_version.py`（§12 门方向修复 + 3 自测例）
- `ci/checks.json`（VERSION-CONSISTENCY 去 `--expected`；CHK-UNIT 增 `PY-TEST-INDEX-VERDICTS` 步骤）
- `ci/id_migration_map.json`（新步骤 ID 登记，absorbed）
- `tests/test_index.csv`（机器 verdict/计数列 + 陈旧值订正）
- `tests/version/test_adopt006_version_gate.py`（absence 正/负例）
- `tests/unit/p1_hips_writer_test.cpp`（P0-19 断言反转）
- `tests/unit/p3002_uncertainty_test.cpp`、`tests/unit/rt001_unique_executor_test.cpp`、`tests/unit/p3002_real_nodes_test.cpp`、`tests/unit/v6_aio/v6_aio_test.cpp`（合成 fixture icrs→equatorial）
- `tests/cli/test_cli_protocol.py`、`tests/cli/test_cli004_process_protocol.py`（预检 fail-closed 期望同步）

**未动**：`lib/**`、`docs/**`、`contracts/**`、`tests/unit/p1001_real_nodes_test.cpp`。
