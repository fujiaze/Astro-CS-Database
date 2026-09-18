# RELEASE-02 CLI 面修复报告（CLI-fix）

> 任务：修复 DESIGN-CONFORMANCE 审计确认的 4 条 CLI 面缺陷（预检 fail-open、`-force` 语义、合法键判 unknown、资源门旗标不可达）。
> 文件面：`lib/infrastructure/cli/**`（本报告除外）。
> 依据：`ASTROCS_DESIGN.md` §3.5 / §4.5.5 / §5.3 / §8、`docs/plugins/infrastructure/21_observability.md` §8.4、`ENGINEERING_SPEC.md` §8 fail-closed（:122）、审计 DC-310/DC-313/DC-408/DC-411/DC-401/DC-418/DC-419/DC-503/DC-801。
> 零 git 写：改动全部留在工作树，由前台统一构建/提交。
> 构建约束：未跑 `ninja`/`cmake`/`ctest`；仅 `g++ -fsyntax-only` 单文件 + `/dev/shm` 行为单测（`TMPDIR=/dev/shm/astrocs_cli`）。

## 0 结论

4 条缺陷全部在 CLI 面闭合；新增键已能识别并透传到 `pdoc`，资源门旗标已可达。**未闭合项**见 §5：官方 `config/templates/*.phase_config.json` 的嵌套格式（DC-514）仍需合同/CLI 形态裁决；`-y` 不显示预检页（§3.5）等 2 条既有偏差不在本次 4 条范围内；2 个既有测试断言的是修复前的 fail-open 行为，需测试分片同步（§6）。

## 1 逐缺陷处置

### 缺陷 1 — 预检 fail-open（DC-310 / DC-408，违反 §3.5:204）

**改动**：`lib/infrastructure/cli/subcommand.h`

| file:line | 内容 |
|---|---|
| `subcommand.h:110-122` | 新增 `path_readable_file` / `path_readable_dir`：用 `std::filesystem`（error_code 重载，不抛）+ 打开/枚举做**存在性 + 可读性 + 类型**判定 |
| `subcommand.h:124-139` | `check_input_path`：不存在 → `… 指向的路径不存在：<path>（§3.5 error：文件找不到，-y 不可越）`；存在但不可读/类型不符 → `… 路径不可读或类型不符（应为文件/目录）：<path>` |
| `subcommand.h:141-182` | `input_path_errors(SessionId, doc)`：覆盖 normalize 的 `input_lights[]` + `master_bias/dark/flat`（已给定时）、mosaic 的 `hips_paths[]`（目录）、export 的 `source.hips_dir`（目录）；数组元素非字符串或空串也判 error |
| `subcommand.h:188-202` | `calibration_checks`：已给定但磁盘不可读的 master 不再报 `[correct]`（消除假绿），由 `input_path_errors` 报具体原因 |
| `subcommand.h:209-228` | `precheck_config` 在结构可达后追加 `input_path_errors` 的 `[error]` 行（页面必含具体路径与原因） |
| `subcommand.h:304-309` | `run()`：结构/合同校验后，`input_path_errors` 非空 → 打印页面并 `return astrocs::INPUT`（**3 = 输入缺失**），`-y` 不可越 |

**传递/阻断路径**：`precheck_config`（页面）→ `run()` 判据 → rc=3；`-force` 时整段预检被越过（见缺陷 2），由运行期节点报 `error_kind=input` → rc=3。

**退出码决策**：`exit_codes.h` `INPUT=3 // 输入缺失`，§6.3 表同义；「文件找不到」归 3。结构错仍 2、缺校准帧（未提供）仍 2，三者分离（§2）。

### 缺陷 2 — `-force` 语义（DC-313 / DC-411，§3.5:212-213）

**改动**：`lib/infrastructure/cli/subcommand.h:273-282`

- `const bool forced = p.flags.count("-force") > 0;`（**单横线**，`--force` 仍判 unknown flag）；
- `forced` 为真时**直接** `session_dispatch(SessionOp::Run)`：不渲染/不打印预检页、不请求确认、不阻断结构错；
- 结构非法/路径缺失由运行期自然报错：`cmd_sessionN_run` → `validate_config_full`（`output_dir` 非串/缺 → 2、unknown key → 3）+ 节点 `validate_config`（如 `input_lights` 非数组 → DATA → 2），给明确理由；
- 删除原「`-force` 越过可强制项但结构错仍阻断」的旧分支与 `forced && has_error` 提示。

### 缺陷 3 — 合法配置键被判 unknown（DC-401 / DC-418 / DC-419 / DC-503）

**改动**：`lib/infrastructure/cli/parser.cpp`、`lib/infrastructure/cli/session_commands.h`

| file:line | 内容 |
|---|---|
| `parser.cpp:309-315` | `kSessionKeys` 新增 `algorithm_rejection_method`、`output_mode` → 不再 rc=3 unknown key |
| `session_commands.h:142-145` | `mosaic --help` 字段说明新增 `algorithm_rejection_method`（留空/0/auto=按 n 自动；显式算法名=强制；分段/表达式=用户映射覆盖内置；方法名不存在/表达式语法错 → error） |
| `session_commands.h:161-164` | `export --help` 字段说明新增 `output_mode`（surface_brightness / point_source_flux / visualization） |

**传递路径（已核实）**：`parse_args` 接受键 → `validate_config_full(session_mode=true)` 放行 → `runtime_client.cpp phase_config()` 直通分支 `pdoc = doc`（phase2 `hips_paths` 分支 :78-79；phase3 `source` 分支 :42-45）→ 节点 `config`。**消费实现不在 CLI 面**，见 §4。

> 未把这两键写进 `--template`（json=nullptr，仅 `--help`），避免在消费者未落地前替用户固化默认值；模板输出结构不变。

### 缺陷 4 — 资源门旗标 unknown flag（DC-801，§8 + 21_observability §8.4）

**改动**：`lib/infrastructure/cli/command_tree.h`、`lib/infrastructure/cli/parser.cpp`

| file:line | 内容 |
|---|---|
| `command_tree.h:45` | `boolean_flags()` 新增 `--strict-resource-gate` |
| `command_tree.h:64` | `value_flags()` 新增 `--on-resource-gate`（取值 accept\|record\|record-only\|strict\|enforce） |
| `command_tree.h:83-90` | normalize/mosaic/export 的 `allowed` 三个命令均登记这两个 token |
| `parser.cpp:37-38` | `cmd_boolean_tokens()` 同步 `--strict-resource-gate` |
| `parser.cpp:44-45` | `cmd_value_tokens()` 同步 `--on-resource-gate`（取值旗标，缺值 → 2） |

**传递路径**：`parse_args`（白名单命中）→ `p.flags["--strict-resource-gate"]` / `p.values["--on-resource-gate"]` → `commands.cpp:564-571 strict_resource_gate_arg` → `run_with_resource_gate(..., strict_gate)`（三命令 `cmd_sessionN_run` 均已接线）→ record_only ↔ enforce（exit 10）可达。非法取值仍 `ParseError` → 2，不静默降级。不写进 help（与既有内部/机器旗标一致）。

## 2 判据/退出码分层（`run()` 非 -force 路径）

1. 配置读不到 / JSON 坏 / 非对象 → 3（§3.5 末条：预检本身失败按 error）；
2. 结构错（键缺失/类型错，`config_structure_errors`）→ 2；
3. 配置合同（白名单键/值域，`validate_config_full`）→ 自身码（unknown key 3、schema_version 2）；
4. **输入路径不存在/不可读（`input_path_errors`）→ 3**，`-y` 不可越；
5. 可强制项（缺校准帧，未提供）→ 2，`-y` 不可越（仅 `-force` 越过整个预检）；
6. 确认（无 `-y` 且非交互 EOF → 2，fail-closed）。

优先级 3 先于 4/5：保持「更具体的诊断不被校准帧 finding 掩盖」（GAP-034 同族）。

## 3 验证证据（未跑构建）

1. `g++ -std=c++17 -fsyntax-only`：`subcommand.h`（经 TU，含 `cancel_token.h`）、`parser.cpp` 均 rc=0（`-Wall -Wextra` 无告警）。
2. 预检行为单测（`/dev/shm/astrocs_cli/precheck_test.cpp` + `empty_elem_test.cpp`，共 12 断言全 PASS）：
   - normalize 4 条不存在路径 → 4 个 error 且文案含具体路径；页面含 `[error]`；
   - 存在文件 + 已给 masters → 0 error；空串 masters → 0 path error、3 个 calibration error；
   - mosaic 不存在目录 → 1 error；`hips_paths` 指向文件（类型错）→ error；`input_lights` 含空串元素 → error；
   - export 不存在 `source.hips_dir` → error；存在目录 → 0 error。
3. 解析器行为单测（`/dev/shm/astrocs_cli/parser_test.cpp`，9 断言全 PASS）：
   - 三命令 `--strict-resource-gate`（布尔）、`--on-resource-gate strict|record`（取值）可解析；
   - `--on-resource-gate` 缺值 → 拒绝；`--definitely-unknown` → 拒绝；`doctor --strict-resource-gate` → 拒绝；`-force` 单横线可解析。
4. 配置键单测（`/dev/shm/astrocs_cli/cfg_test.cpp`，3 断言全 PASS）：
   - `algorithm_rejection_method` → `validate_config_full(session_mode=true)` rc=0；
   - `output_mode` → rc=0；`definitely_not_a_key` → rc=3（判别力保留）。

> 未跑 `ninja`/`ctest`（多分片并行，构建由前台统一做），故上述为单元级证据；端到端由前台构建后复跑。

## 4 需 scheduler 面接消费者的清单（给前台转交）

| 键 | CLI 侧现状 | 需要的消费者（scheduler 面） | 审计 |
|---|---|---|---|
| `algorithm_rejection_method`（mosaic） | 已识别、已透传 `pdoc`；`mosaic --help` 已说明 | `module_adapters.cpp` phase2 reject/integrate：按输出像素 `n`（几何覆盖数，非整组帧数、非 `n_eff`）路由；留空/0/auto=内置映射，显式算法名=强制不被覆盖，分段/表达式=用户映射覆盖内置；实际方法与 `n` 写入 `rejection` provenance；方法名不存在/表达式语法错 → error | DC-401/418/419/426 |
| `output_mode`（export） | 已识别、已透传 `pdoc`；`export --help` 已说明 | phase3 生产 resample/writer（或 v6 export）：surface_brightness / point_source_flux / visualization 三模式；缺所选模式所需信息 → 拒绝或明确 unavailable，禁静默按 surface_brightness | DC-503 |

> `--strict-resource-gate` / `--on-resource-gate` 的消费者**已在 CLI 面**（`commands.cpp strict_resource_gate_arg`），无需 scheduler 接线；本修复只解除白名单阻断。

## 5 未闭合项 / 风险

1. **官方模板仍不可直接运行（DC-514，阻断级）**：`config/templates/*.phase_config.json` 用嵌套 `phase_name`/`config`/`inputs` 形态，CLI 是扁平键形态；顶层无 `output_dir` → 预检结构错 rc=2（本次未改变）。使其可运行需合同/CLI 形态裁决（统一 schema↔CLI 或退役模板并订正 §3.3/§4.5/§5.3），超出本 CLI 分片「不再判 unknown + 透传」的授权面，需前台裁决。
2. **其余死键未接线（DC-426/DC-309）**：`algorithm_weight_mode`、`algorithm_upm_gauge`、`algorithm_psf_model`、`sparse_snr_layer` 等仍不在白名单（会被判 unknown）；本次缺陷只点名 `algorithm_rejection_method` 与 `output_mode`，未扩面。
3. **`-y` 不显示预检页（§3.5「无论 correct/warn/error 都必弹页面」）**：现状 `-y` 跳过 `confirm_run`（页面在 `confirm_run` 内打印）→ 页面不显示。不在本次 4 条缺陷内，未改（避免与既有 stdout/stderr 纪律测试冲突）。
4. **无 warn/optimize 生产者（DC-311）**：预检仍只产 correct/error；暗场-亮场曝光容差等告警未实现（依赖 `config/defaults.json` 加载器，DC-303）。
5. **`-force` 仍受运行期合同校验**：`-force` 越过的是 §3.5 预检（页面/确认/结构阻断/路径存在性），session 层的 `validate_config_full`（unknown key → 3）仍执行，属「运行期自然报错」，符合负责人口径。
6. **`--on-resource-gate` 取值校验时机**：非法值在 `cmd_sessionN_run` 内 `strict_resource_gate_arg` 抛 `ParseError`（rc=2），发生在 `write_run_context` 之后；为既有行为，未改动。

## 6 受影响测试（需测试分片同步，本次未改 tests/）

- `tests/cli/test_cli_protocol.py::TestManifestIncomplete::test_01`（:249-278）：断言「缺输入文件 → rc=3 **且** 落 1 个 incomplete manifest + 完整事件流」。修复后预检在写 manifest 前即以 rc=3 阻断 → manifest/事件断言需改为「预检阻断、无 manifest」。
- `tests/cli/test_cli004_process_protocol.py::test_01` 的 1b 段（:189-223）：同上（期望 rc=3 + incomplete manifest + 事件流）。
- 仍绿（已核对判据）：`test_phase1_inprocess.py::test_04`（rc=3）、`test_phase123_pipeline.py::test_04` b/c（rc=3、无 complete）、`test_cli001_vpi.py::test_04`（空输入 rc=2 + `[error]` + 零产物）、`test_cli_build.py::test_03d`（模板键无 unknown、非 70；负例仍报 unknown key）、`test_cli_protocol.py::test_06`、`test_phase3_inprocess.py::test_11`（真实路径存在性通过）。

## 7 改动文件

```
lib/infrastructure/cli/command_tree.h     |  14 +++-
lib/infrastructure/cli/parser.cpp         |  15 +++-
lib/infrastructure/cli/session_commands.h |  10 +++
lib/infrastructure/cli/subcommand.h       | 131 ++++++++++++++++++++++++++----
```
