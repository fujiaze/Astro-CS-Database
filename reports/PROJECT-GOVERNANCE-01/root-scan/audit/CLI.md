# ROOT-004 扩展轴审计 — CLI 命令树与命令合同（轴代号 CLI）

- **开工基线**：`git rev-parse HEAD main origin/main` = `900916fb0dfe93e21bd36c09908a6cc679de5256`（三 ref 相等，按前台改判规则开工；原固定门 `2c328348304d033aecfa81faf79d1c6cd802b30a` 由前台宣告作废）。
- **收工基线**：`2fc19b2188ef1c084625bc7f8c7ee47b93687d11`（HEAD=main=origin/main 三 ref 相等）。收工过程中 `32a5f5f3` 之后又推进 3 条提交（`181b4496` 旧工具退役、`b19ff57e`/`2fc19b21` 治理文档），均未触碰 `cli/**` 与 `lib/infrastructure/cli/**`（实测 mtime 停在 16:10–16:45），故本报告的定位行号与运行时证据在收工 SHA 上仍然有效。审计期间 HEAD 连续推进（`23f42ffa` CLI-001 命令树切换、`01754fab` INT-001 台账、`35804431`、`32a5f5f3`）；`d414c3e0` 时点 origin/main 曾为 `b4afc135`（三 ref 不等，按规则只登记不中止）。受影响条目已在各条内标注状态。
- **方法**：只读。先读权威（ASTROCS_DESIGN §6.1–§6.3、§3.5、§7.1、§9、§10.1、§12；ENGINEERING_SPEC §1/§7/§9；docs/plugins/infrastructure/18_cli.md；docs/ci/01·03），再核 `cli/parser.cpp`、`cli/commands.cpp`、`lib/infrastructure/cli/**`、`cli/CMakeLists.txt`、根 `CMakeLists.txt`、`ci/checks.json`、`tests/cli/**`；命令面以产品二进制 `build/astrocs` 做运行时探针（模板/预检/确认/-force/JSONL 纯净性/取消/退出码/死代码）。全程未跑 cmake/ninja/ctest/pytest 等构建命令，构建相关结论一律标【需构建复验】。零修复、零 git 写。
- **摘要**（≤10 行）
  1. 开工时点 GAP-005 主体成立（`git show HEAD:cli/parser.cpp | grep -c normalize` = 0，kRules 27 条旧命令）；审计期间并行线以 `23f42ffa` 完成 CLI-001 切换，收工时点 `astrocs help` 已是 §6.2 的 7 行、旧命令 rc=2 收口 → 该主体已闭合，本轴不再立条，只登记残留。
  2. 最重残留：**资源门禁退出码 10 在唯一用户命令面不可达**（strict 旗标未进新树 allowed），而 CHK-RESOURCE 是 P0 无豁免门 → 发布门失效（CLI-1，P0）。
  3. 退出码链第二处缺陷：runtime 错误域 RESOURCE 被硬映射成 5（BACKEND 语义）（CLI-2）。
  4. §6.3「`--json` 时 stdout 恰一个 JSON 文档」在运行路径无实现（`--json` 被占用为配置挂载，机器输出只有 JSONL），`--json` 同名双义靠命令名特判（CLI-3）。
  5. §3.5 预检三级只落两级（橘色无生产者），不存在的输入实测判 `[correct]`，`-force` 可越过全部 error（CLI-4）。
  6. 模板与运行路径以 `"."`（进程 CWD）作 output_dir 隐式缺省（CLI-5）。
  7. 取消实测 exit 9 但不出 incomplete manifest；证据取自产品入口内嵌的 `ASTROCS_TEST_*` 钩子路径（CLI-6）。
  8. 命令树模块未接入根构建图（靠 `#include "../lib/..."` 绕过），叠加 cli/CMakeLists.txt 的第二 project()/同名 astrocs 产物与两个不同版本串（CLI-7/CLI-8）。
  9. tests/cli 内两套互斥命令树 golden 并存（旧 26 行仍在跟踪文件），UT-CLI 且验兼容工程二进制（CLI-9）。
  10. 退出码「唯一源」路径不存在 + 第二处数值表 + 插件文档 schema 指向悬空（CLI-10）；另 5 条 P2（帮助字段、doctor、死代码、入口名、版本信息）。
- **发现计数**：**P0 = 1，P1 = 9，P2 = 5（共 15 条）**。

## 发现表

| ID | 定位 | 严重度 | 一句话 |
|---|---|---|---|
| CLI-1 | `lib/infrastructure/cli/command_tree.h:73-88` + `cli/commands.cpp:627-633,973-998` | **P0** | 资源门禁退出码 10 在唯一用户命令面不可达，P0 门失效 |
| CLI-2 | `cli/runtime_client.cpp:378,398` | P1 | RESOURCE 域被映射为退出码 5（BACKEND 语义） |
| CLI-3 | `cli/parser.cpp:32-56` + `lib/infrastructure/cli/subcommand.h:98-133` | P1 | 运行路径无「`--json` 时 stdout 恰一个 JSON 文档」，且 `--json` 同名双义 |
| CLI-4 | `lib/infrastructure/cli/subcommand.h:51-75,118-133` | P1 | §3.5 三级只落两级、文件不存在不判红、`-force` 越过全部 error |
| CLI-5 | `lib/infrastructure/cli/session_commands.h:65,73,80` + `cli/commands.cpp`（11 处） | P1 | output_dir 以 `"."`（进程 CWD）作隐式缺省 |
| CLI-6 | `lib/infrastructure/cli/subcommand.h:83-97` + `cli/commands.cpp:1046,1256,1650` | P1 | 取消只发 rc=9 不写 incomplete manifest；产品入口内嵌环境变量测试钩子 |
| CLI-7 | 根 `CMakeLists.txt`（零命中）+ `cli/parser.cpp:10`、`cli/commands.cpp:79-84` | P1 | 命令树模块未接入根构建图，靠工作区相对包含绕过 |
| CLI-8 | `cli/CMakeLists.txt:10,216` + `build/version_generated.h` vs `build/cli/version_generated.h` | P1 | 第二套构建入口与同名双二进制并存 |
| CLI-9 | `tests/cli/test_cli_protocol.py:20-46,5-8,50-53` | P1 | UT-CLI 验兼容工程二进制，旧 26 行 golden 与新 7 行实现互斥 |
| CLI-10 | `include/astrocs/exit_codes.h`（不存在）、`include/astrocs/core/contracts.h:43-46`、`18_cli.md:16` | P1 | 退出码唯一源路径不存在 + 第二处数值表 + 插件文档 schema 悬空 |
| CLI-11 | `lib/infrastructure/cli/command_tree.h:112-137` | P2 | `help`/`--help` 只有 usage 行，无 §6.3 要求的字段说明 |
| CLI-12 | `cli/commands.cpp:2328-2330` | P2 | `doctor` 强制 `--json`，与 §6.2 及自身 help 文本矛盾 |
| CLI-13 | `cli/commands.cpp:144,161,1424,1452,1517` | P2 | 6 个退役命令实现零调用死留在产品源内，仍读已废弃的 `--config` |
| CLI-14 | 根 `CMakeLists.txt:612` | P2 | 唯一可执行入口名 astrocs ≠ §10.1 的 ACSD Cli.exe / acsd_cli（与 GAP-018 同源不删条） |
| CLI-15 | `./build/astrocs --version` 行为面 + `cli/version_generated.h.in` | P2 | Alpha 前版本信息进入程序（与 GAP-017 同源不删条） |

---

### CLI-1（P0）资源门禁退出码 10 在唯一用户命令面不可达
- **违反的最新权威条款**：ASTROCS_DESIGN §6.3 退出码表列「10 = 资源利用率或内存增长门禁失败」，且「0 = 成功且门禁全过」；docs/ci/01_CHECKS.md 把 CHK-RESOURCE 定为 P0 门禁（03_GATES.md：P0 红灯无豁免）。
- **当前证据**：命令：`timeout 30 grep -n "strict\|on-resource-gate\|resource-detail" lib/infrastructure/cli/command_tree.h` → 输出：`(无命中，rc=1)`；命令：`timeout 60 ./build/astrocs normalize --json /tmp/cfg_bad.json --strict-resource-gate -y` → 输出：`astrocs: unknown flag '--strict-resource-gate'` / `exit=2`；命令：`grep -n "strict_resource_gate_arg\|return astrocs::RESOURCE" cli/commands.cpp` → 输出：`627:static bool strict_resource_gate_arg(const Parsed& p)` / `789: return astrocs::RESOURCE;` / `998: return astrocs::RESOURCE;`（均在 strict→enforced 分支内）。
- **影响**：CLI 面无法启用强制资源/内存增长判决，超阈值运行仍以 0 收工，P0 发布门形同虚设。
- **整改建议**：把 `--strict-resource-gate`、`--on-resource-gate`、`--resource-detail` 登记回 `command_tree.h` 三命令的 allowed 集合（默认 record-only 不动 SO-05 判决权）；若裁决「10 只属 CI 内部码」，须由负责人同时改 §6.3 与 CHK-RESOURCE 语义，不能沉默并存。
- **建议文件域**：`lib/infrastructure/cli/command_tree.h`、`cli/commands.cpp`、`tests/cli/**`、`docs/ci/01_CHECKS.md`（仅在改语义时）。
- **验收门**：`timeout 60 ./build/astrocs normalize --json /tmp/cfg_bad.json --strict-resource-gate -y; echo rc=$?` 的 rc 不等于 2。【需构建复验：本轮未执行构建，判决链以静态 + 当前二进制探针为证】
- **GAP/任务关系**：归属任务 **CLI-003**（机器输出/退出码/取消）；与 GAP-015（资源门双实现）相邻不重复。
- **旧清单同源**：`V21-N-15`（§10.5 门禁标注根因）相邻不同因；本条可达性为新增。

### CLI-2（P1）runtime 错误域 RESOURCE 被映射为退出码 5
- **违反的最新权威条款**：ASTROCS_DESIGN §6.3 退出码表把 5 定义为「backend ABI/签名/CPU 特征/加载失败」、10 为「资源利用率或内存增长门禁失败」；ENGINEERING_SPEC §9 要求统一状态码、跨平台同失败同码。
- **当前证据**：命令：`timeout 30 grep -n "RESOURCE" cli/runtime_client.cpp` → 输出：`378:    //   IO → 7；CANCELLED → 9；RESOURCE → 5；其余 → 70`、`398:      case astrocs::core::ErrorDomain::RESOURCE: return 5;`。
- **影响**：资源类失败与 backend 加载失败共用一码，调用方与 CI 无法区分。
- **整改建议**：`runtime_client.cpp` 该 case 改 `return 10;`，并删注释里 RESOURCE→5 的规范化表述。
- **建议文件域**：`cli/runtime_client.cpp`、`tests/cli/**`。
- **验收门**：`timeout 30 grep -n "ErrorDomain::RESOURCE: return" cli/runtime_client.cpp` 输出含 `return 10`。
- **GAP/任务关系**：归属 **CLI-003**。
- **旧清单同源**：与 `M5b-C-05`（退出码在文档/唯一源/Runtime 映射三处互不覆盖，OPEN）**同源不删条**。

### CLI-3（P1）运行路径无「`--json` 时 stdout 恰一个 JSON 文档」，且 `--json` 同名双义
- **违反的最新权威条款**：ASTROCS_DESIGN §6.3 同时规定「配置挂载：`--json <path>`」与「机器输出：`--json` 时 stdout 恰一个 JSON 文档」；18_cli.md §5 把 `json` 列为配置路径、§8 要求「`--json` 恰一个 JSON 文档」。
- **当前证据**：命令：`timeout 60 ./build/astrocs normalize --json /tmp/cfg_bad.json -y` → 输出（stdout）：`/tmp/acs_probe/astrocs_run_6dd11f6e6ba0.json`（一行路径，非 JSON 文档）；命令：`./build/astrocs normalize --json /tmp/cfg_bad.json --events-jsonl -y | python3 统计` → 输出：`lines 6`、`kinds ['artifact','final','progress','stage_end','stage_start']`、10 必含字段齐备；命令：`grep -n "cmd_boolean_tokens\|cmd_value_tokens\|cmd_allows_bare" cli/parser.cpp` → 输出：`32`/`37`/`44`（`--json` 同现于布尔与取值两个集合，靠命令名特判）。
- **影响**：机器消费方没有稳定的单文档结果接口；新增命令时 `--json` 语义易错配。
- **整改建议**：在 `subcommand.h::run` 末尾补单 JSON 文档结果模式（如 `--json` 裸开关与 `--json <path>` 分离，或新增 `--result-json`），并请负责人把 §6.3 两处 `--json` 改名区分。
- **建议文件域**：`cli/parser.cpp`、`lib/infrastructure/cli/{command_tree,subcommand}.h`、`ASTROCS_DESIGN.md` §6.3（裁决后）。
- **验收门**：`timeout 60 ./build/astrocs normalize --json <cfg> <单文档开关> | python3 -c "import json,sys; json.load(sys.stdin)"` 退出码 0。
- **GAP/任务关系**：归属 **CLI-003**；同时构成文档侧 UNRESOLVED 建议（§6.3 内部歧义）。
- **旧清单同源**：`M5b-C-03` 相邻（未接线以 ARGS(2) 表达），非同源。

### CLI-4（P1）§3.5 预检三级只落两级，「文件找不到」不判红，`-force` 越过全部 error
- **违反的最新权威条款**：ASTROCS_DESIGN §3.5 规定预检三级（绿 correct / 橘 optimize / 红 error），红色判定含「滤镜不匹配、文件找不到、未知滤镜」且 error 必须阻断，「只有 `-force` 才能在缺少校准帧等情况下无阻塞运行（其余 error 仍阻断）」；18_cli.md §8 要求预检三态测试。
- **当前证据**：命令：`timeout 30 ./build/astrocs normalize --json /tmp/cfg_bad.json </dev/null`（`input_lights=["/nonexistent/x.fits"]`）→ 输出：`  [correct] input_lights 条目数 = 1`；命令：`timeout 60 ./build/astrocs normalize --json /tmp/cfg_empty.json -force -y` → 输出：`astrocs: normalize running with -force over precheck error(s)`；命令：`grep -n '"optimize"' lib/infrastructure/cli/session_commands.h` → 输出：仅 `105` 渲染分支（无任何 `level="optimize"` 生产者）。
- **影响**：输入不可用的配置能进入执行；`-force` 成为越过一切预检的万能开关，与 §3.5 分级阻断相反。
- **整改建议**：`precheck_config` 增加文件存在性与滤镜/容差判定（容差基准取 `config/defaults.json`，据此产出橘色级），并给 `CheckLine` 加 `forceable` 位，只允许「缺少校准帧」类被 `-force` 越过。
- **建议文件域**：`lib/infrastructure/cli/{subcommand,session_commands}.h`、`tests/cli/**`。
- **验收门**：`timeout 60 ./build/astrocs normalize --json /tmp/cfg_bad.json </dev/null 2>&1 | grep -c error` ≥ 1。
- **GAP/任务关系**：归属 **CLI-002**（模板/预检/确认/force）。
- **旧清单同源**：GAP-005 的「预检未实现」附带缺口已部分闭合，本条为剩余偏差（新增）。

### CLI-5（P1）output_dir 以 `"."`（进程 CWD）作隐式缺省
- **违反的最新权威条款**：ASTROCS_DESIGN §6.3「运行产物只落配置 `output_dir`，不得以进程 CWD 作隐式缺省写出」；§6.1 要求模板输出可直接改的 JSON。
- **当前证据**：命令：`timeout 30 ./build/astrocs mosaic --template` → 输出含 `  "output_dir": ".\",`；命令：`grep -c 'value("output_dir", std::string(".")' cli/commands.cpp` → 输出：`11`；命令：`grep -n 'value("output_dir"' cli/runtime_client.cpp` → 输出：`111:  const std::string out_dir = doc.value("output_dir", std::string("."));`；文件 `lib/infrastructure/cli/session_commands.h:65,73,80` 三份模板同写 `"."`。
- **影响**：按模板直接运行即把产品写进进程当前目录（含仓库根），违反产物落位合同并放大根目录污染风险。
- **整改建议**：模板把 `output_dir` 置为显式待填标记（如 `<SET_ME>`），`validate_config_full` 拒绝 `"."` 与空值；运行路径 11 处 `"."` 兜底改为判参数/输入错误。
- **建议文件域**：`lib/infrastructure/cli/session_commands.h`、`cli/commands.cpp`、`cli/runtime_client.cpp`。
- **验收门**：`timeout 30 ./build/astrocs normalize --template | grep -c '"output_dir": \".\"'` 输出 0（模板不再以进程 CWD 作 output_dir 缺省）。
- **GAP/任务关系**：归属 **CLI-002**，并与 **CFG-001**（默认值单源）相关。
- **旧清单同源**：GAP-006（无 config/ 等价物）本轮已被 `config/{defaults.json,filters.json,templates}` 落地部分闭合；本条为新增偏差。

### CLI-6（P1）取消只发 rc=9，不写 incomplete manifest；产品入口内嵌环境变量测试钩子
- **违反的最新权威条款**：ASTROCS_DESIGN §6.3「取消（Ctrl-C）：协作取消 → 关 writer → 写 incomplete manifest → 删/隔离临时产物 → exit 9；不得留下看似完整的产品」。
- **当前证据**：命令：`ASTROCS_TEST_SLEEP_MS=6000 timeout 30 ./build/astrocs normalize --json /tmp/cfg_c.json -y & sleep 2; kill -INT $!; wait; echo exit=$?` → 输出：`astrocs: normalize cancelled` / `exit=9`；命令：`ls -1 /tmp/acs_c` → 输出：`x.fits`（无 `astrocs_run_*.json`）；命令：`grep -rn "ASTROCS_TEST_SLEEP_MS\|ASTROCS_TEST_CRASH" cli lib/infrastructure/cli` → 输出：`lib/infrastructure/cli/subcommand.h:85,97`、`cli/commands.cpp:1046,1256,1650`。真实会话路径（如 `cli/commands.cpp:1119,1352,1710`）确有 `write_run_manifest(..., "incomplete", "cancelled by user", ...)`，但新薄入口在进会话前即返回 9。
- **影响**：取消的运行在薄入口层不可追溯；同一入口还允许用环境变量把产品命令改写成延时/必崩路径，属交付面污染。
- **整改建议**：`subcommand.h` 取消分支返回前调用 `write_run_manifest(..., "incomplete", ...);` 并补 `emit_final`；两个 `ASTROCS_TEST_*` 钩子移入仅测试构建开关（或测试专用 TU）。
- **建议文件域**：`lib/infrastructure/cli/subcommand.h`、`cli/commands.cpp`、`tests/cli/**`。
- **验收门**：`timeout 60 ./build/astrocs normalize --json /tmp/cfg_c.json -y & sleep 1; kill -INT $!; wait; ls /tmp/acs_c | grep -c astrocs_run_` ≥ 1。【真实 SIGINT（非钩子）路径需构建复验】
- **GAP/任务关系**：归属 **CLI-003**。
- **旧清单同源**：无（M5b 系列未覆盖取消链）。

### CLI-7（P1）命令树模块未接入根构建图，靠工作区相对包含绕过
- **违反的最新权威条款**：ASTROCS_DESIGN §7.1 指定子命令实现落 `lib/infrastructure/cli/{normalize,mosaic,export}`；ENGINEERING_SPEC §1 要求唯一根 CMake 构建图（模块须由根图登记）。
- **当前证据**：命令：`timeout 30 grep -n "infrastructure/cli" CMakeLists.txt` → 输出：`(无命中，rc=1)`；命令：`grep -n '#include "../lib/infrastructure' cli/parser.cpp cli/commands.cpp` → 输出：`cli/parser.cpp:10`、`cli/commands.cpp:79-84`；命令：`git show 01754fab | grep -n "infrastructure/cli"` → 输出：`39:+...它改用**工作区相对包含**，**未改根** CMakeLists.txt`、`40:+⇒ 你要做：…登记进根构建图（INT-001）`。
- **影响**：产品入口的编译依赖不在构建图内，`lib/infrastructure/cli/CMakeLists.txt`（自称非产品事实源）与实际构建脱钩，分层可被静默破坏。
- **整改建议**：按 `lib/infrastructure/cli/CMakeLists.txt:3-15` 注释登记 `add_subdirectory(lib/infrastructure/cli)` 与 4 个 include 目录到 `astrocs` target，并把相对包含改回短名包含。
- **建议文件域**：根 `CMakeLists.txt`、`cli/{parser,commands}.cpp`、`lib/infrastructure/cli/CMakeLists.txt`。
- **验收门**：`timeout 30 grep -c "infrastructure/cli" CMakeLists.txt` ≥ 1。【需构建复验】
- **GAP/任务关系**：归属 **INT-001**（台账已自认该待办）。
- **旧清单同源**：与 GAP-003（`lib/infrastructure` 不存在）相邻；该目录现已入库，本条为其残留接线缺口。

### CLI-8（P1）第二套构建入口与同名双二进制并存
- **违反的最新权威条款**：ENGINEERING_SPEC §1「唯一根 CMake + presets；不引入第二套构建入口」；ASTROCS_DESIGN §10.1「唯一可执行入口」。
- **当前证据**：命令：`grep -n "^project(\|^add_executable(astrocs" cli/CMakeLists.txt` → 输出：`10:project(astrocs_cli LANGUAGES CXX C)`、`216:add_executable(astrocs main.cpp parser.cpp commands.cpp process.cpp runtime_client.cpp ...)`；命令：`grep ASTROCS_VERSION_STRING build/version_generated.h build/cli/version_generated.h` → 输出：`build/version_generated.h: "0.11.0-alpha.2+g01754fab8618..."` / `build/cli/version_generated.h: "0.11.0-alpha.2+g01754fab8618.dirty"`；命令：`ls build/cli/astrocs` → 输出：`MISSING`（tests/cli 的 `built()` 会自行 `cmake -S cli -B build/cli` 现造）。
- **影响**：同一提交可产出两个版本串不同的 `astrocs`，验收与测试可能指向非交付二进制。
- **整改建议**：`cli/CMakeLists.txt` 收敛为纯源清单 include（不再 `project()`、不再 `add_executable`），全部 CLI 测试统一用根构建产物 `build/astrocs`。
- **建议文件域**：`cli/CMakeLists.txt`、`tests/cli/**`、`ci/**`。
- **验收门**：`timeout 30 grep -c "^project(astrocs_cli" cli/CMakeLists.txt` 输出 0。
- **GAP/任务关系**：归属 **INT-001 / CI-001**。
- **旧清单同源**：与 `M8-F-003`（安装/构建前置恒 SKIP）同族；BLD-002 注释已知但未闭环。

### CLI-9（P1）UT-CLI 验兼容工程二进制，且旧 26 行命令树 golden 与新实现互斥
- **违反的最新权威条款**：docs/ci/01_CHECKS.md 的 UT-CLI = `python3 -B -m unittest discover -s tests/cli -t tests/cli`（机器门须验实际交付面，ENGINEERING_SPEC §8/§9）；ASTROCS_DESIGN §6.2 规定唯一命令树。
- **当前证据**：命令：python 比对 `HELP_LINES` 与 `./build/astrocs help` → 输出：`golden lines: 26  actual lines: 7`、`MISMATCH`、`GOLDEN-NOT-IN-ACTUAL: astrocs phase1 run --config <path> [--cpu-profile <path>] [--events-jsonl]`；命令：`grep -rl "phase1 run\|config show-effective\|benchmark cpu\|modules list\|test synthetic" tests/cli/*.py | wc -l` → 输出：`7`（tests/cli 共 22 个 .py）；命令：`sed -n '5,8p;50,53p' tests/cli/test_cli_protocol.py` → 输出：`BUILD = os.path.join(REPO, "build", "cli")`、`subprocess.run(["cmake", "-S", CLI, "-B", BUILD]...)`。同目录另存 `tests/cli/test_command_tree.py`（断言新 7 行树）。
- **影响**：`unittest discover` 同时收集新旧两套 golden，UT-CLI 结果不再反映交付命令面；兼容工程二进制可能被误当交付物验收。
- **整改建议**：把 tests/cli 全部 EXE 指向 `build/astrocs`；`test_cli_protocol.py` 的 `HELP_LINES` 换成 §6.2 的 7 行；其余引用被删命令的用例改为断言 rc=2。
- **建议文件域**：`tests/cli/**`。
- **验收门**：`timeout 600 python3 -B -m unittest discover -s tests/cli -t tests/cli` 返回 0。【需构建复验：本轮未跑】
- **GAP/任务关系**：归属 **CLI-001/CLI-003**，并落在新立 **TEST-CLI-SYNC** 任务面。
- **旧清单同源**：与 `M8-F-003`、GAP-027（门恒绿/静默跳过）同源族，不删条。

### CLI-10（P1）退出码唯一源路径不存在 + 第二处数值表 + 插件文档 schema 指向悬空
- **违反的最新权威条款**：ASTROCS_DESIGN §6.3「退出码（唯一源 `include/astrocs/exit_codes.h`）」；18_cli.md §6 同款，§3/`16 行` 引用 `contracts/schemas/cli_output.schema.json`、`events.schema.json`。
- **当前证据**：命令：`find . -name exit_codes.h -not -path "./build/*" -not -path "./run/*"` → 输出：`./cli/exit_codes.h`；命令：`ls include/astrocs/exit_codes.h` → 输出：`无法访问 'include/astrocs/exit_codes.h': 没有那个文件或目录`；命令：`grep -n "enum class ExitCode" -A 3 include/astrocs/core/contracts.h` → 输出：`43:enum class ExitCode : uint8_t {` / `44-45` 完整列出 11 个数值（含 `RESOURCE = 10`）；命令：`ls contracts/schemas/` → 输出含 `jsonl_event_v1.schema.json`，不含 `cli_output`/`events` 两个被引用名。
- **影响**：「唯一源」不可依赖，两份数值可各自漂移；机器输出没有 schema 事实源，检查器只能按文件名兜底。
- **整改建议**：把 `cli/exit_codes.h` 落位为 `include/astrocs/exit_codes.h`，`contracts.h` 的 enum 改为引用同一常量（删重复数值）；或按实况改 §6.3/18_cli.md 路径；并补 `cli_output`/`events` schema 或把文档指向 `jsonl_event_v1`。
- **建议文件域**：`include/astrocs/**`、`cli/exit_codes.h`、`contracts/schemas/**`、`docs/plugins/infrastructure/18_cli.md`、`tools/check_cli_command_layer.py`。
- **验收门**：`timeout 30 bash -c 'test -f include/astrocs/exit_codes.h && ! grep -q "RESOURCE = 10" include/astrocs/core/contracts.h'` 返回 0。
- **GAP/任务关系**：归属 **CLI-003 / DOC-001**。
- **旧清单同源**：与 `M5b-E-03`（声明的唯一源与 JSONL schema 路径均不存在，检查器回退第二路径，OPEN）**同源不删条**。

### CLI-11（P2）`help`/`--help` 无字段说明
- **违反的最新权威条款**：ASTROCS_DESIGN §6.3「字段说明在 `help` / `--help`」；§6.2 注「`help` 直接输入即为详细帮助」。
- **当前证据**：命令：`timeout 30 ./build/astrocs normalize --help` → 输出：`astrocs normalize (--json <config.json> | --template [-o <path>] | --help)`（单行）；命令：`timeout 30 ./build/astrocs help | wc -l` → 输出：`7`；文件 `lib/infrastructure/cli/command_tree.h:112-137` 的 `help_usage/help_text` 只拼 usage 行。
- **影响**：用户没有配置字段权威来源，只能读 schema 或猜。
- **整改建议**：为三命令各补字段说明段（字段名/必填/默认/落位），与 `contracts/schemas/phase_config_*.schema.json` 对齐。
- **建议文件域**：`lib/infrastructure/cli/command_tree.h`、`docs/api/CLI_PROTOCOL_V1.md`、`tests/cli/test_command_tree.py`。
- **验收门**：`timeout 30 ./build/astrocs help | grep -c output_dir` ≥ 1。
- **GAP/任务关系**：归属 **CLI-001/CLI-002**。
- **旧清单同源**：GAP-005 附带缺口（help 无字段说明）剩余部分，不删条。

### CLI-12（P2）`doctor` 强制 `--json`，与命令树及自身 help 文本矛盾
- **违反的最新权威条款**：ASTROCS_DESIGN §6.2 命令树列 `doctor`（环境自检，未规定 `--json` 必填），§6.3 把 `--json` 定义为机器输出开关。
- **当前证据**：命令：`timeout 30 ./build/astrocs doctor` → 输出：`astrocs: doctor requires --json` / `exit=2`；命令：`timeout 30 ./build/astrocs help | grep doctor` → 输出：`astrocs doctor [--json]`。
- **影响**：同一份 help 与实际行为相反，人类自检路径不可用。
- **整改建议**：删除 `commands.cpp:2328-2330` 该 `parse_fail`，无 `--json` 时输出人类可读报告；或把 help 文本改成 `doctor --json` 并同步 §6.2。
- **建议文件域**：`cli/commands.cpp`、`lib/infrastructure/cli/command_tree.h`。
- **验收门**：`timeout 30 ./build/astrocs doctor; echo rc=$?` 的 rc 不等于 2。
- **GAP/任务关系**：归属 **CLI-001**。
- **旧清单同源**：未查/无同源。

### CLI-13（P2）退役命令实现零调用死留在产品源内，仍读已废弃的 `--config`
- **违反的最新权威条款**：ASTROCS_DESIGN §6.1（CLI 薄入口）+ §6.2（唯一命令树）；ENGINEERING_SPEC §9（不被任何门验证的死面不得留在交付路径）。
- **当前证据**：命令：`for f in cmd_config_init cmd_config_validate cmd_show_effective cmd_verify cmd_verify_profile cmd_drizzle; do printf "%s def:%s calls:%s\n" ...; done` → 输出：六个 `def:1 calls:0`；命令：`grep -n '"--config"' cli/commands.cpp | head` → 输出：`145`、`164`、`1399`、`1440`、`1482`、`1519`（python 归属判定：`1399 -> cmd_session3_run`、`1440/1482/1519 -> cmd_phase_validate/plan/inspect`，新树 allowed 已无 `--config`）。
- **影响**：约千行不参与交付的死代码留在产品 TU，掩盖真实入口面；`--config` 读点与新树冲突，任何回归接线都会成为必失败路径。
- **整改建议**：删除退役 handler 与 `cmd_phase_*`；把 `cmd_session3_run` 的 `--config` 读点改为 `--json`。
- **建议文件域**：`cli/commands.cpp`。
- **验收门**：`timeout 30 grep -c 'need_value(p, "--config")' cli/commands.cpp` 输出 0。
- **GAP/任务关系**：归属 **CLI-001** 收尾。
- **旧清单同源**：与 `M5b-C-03`（幽灵命令）同源族。

### CLI-14（P2）唯一可执行入口名 astrocs 与 §10.1 不符
- **违反的最新权威条款**：ASTROCS_DESIGN §10.1「唯一可执行 `ACSD Cli.exe` / `acsd_cli`」、§6.2「全部命令由唯一可执行 `ACSD Cli` 提供」。
- **当前证据**：命令：`grep -n "add_executable(astrocs" CMakeLists.txt` → 输出：`612:add_executable(astrocs`；命令：`grep -rn "acsd" --include="*.txt" --include="*.cmake" --include="*.json" CMakeLists.txt cmake packaging` → 输出：`(零命中)`。
- **影响**：交付合同名与实际产物名不一致，打包门与文档口径分裂。
- **整改建议**：由 PKG-001 统一改名（`OUTPUT_NAME acsd_cli` / Windows `ACSD Cli`），或负责人改 §10.1。
- **建议文件域**：根 `CMakeLists.txt`、`cmake/install_layout.cmake`、`packaging/**`。
- **验收门**：`timeout 30 grep -rl acsd cmake packaging CMakeLists.txt` 有命中。
- **GAP/任务关系**：归属 **PKG-001**。
- **旧清单同源**：与 **GAP-018 同源不删条**。

### CLI-15（P2）Alpha 前版本信息进入程序
- **违反的最新权威条款**：ASTROCS_DESIGN §12「Alpha 之前：程序与代码中不包含任何版本信息」、§6.2 注「`--version` 版本（Alpha 前无版本信息）」；ENGINEERING_SPEC §7 同款。
- **当前证据**：命令：`timeout 30 ./build/astrocs --version` → 输出：`astrocs 0.11.0-alpha.2+g32a5f5f300700b817cfa1ceafe047acaeff4ee9b`（`rc=0`）；文件 `cli/version_generated.h.in` 生成 `ASTROCS_VERSION_STRING`/`ASTROCS_COMMIT_SHA` 并被 `cli/commands.cpp` 使用。
- **影响**：交付面提前固化版本号与 commit 串，与 §12 发布纪律冲突，并放大 CLI-8 的双二进制漂移。
- **整改建议**：Alpha 前 `--version` 只输出产品名（或按 §12 撤除该输出），版本信息仅存于内部合同工件。
- **建议文件域**：`cli/{commands,main}.cpp`、`cli/version_generated.h.in`、`cmake/**`。
- **验收门**：`timeout 30 ./build/astrocs --version | grep -c alpha` 输出 0。
- **GAP/任务关系**：归属 **GOV-001 / PKG-001**。
- **旧清单同源**：与 **GAP-017 同源不删条**。

---

## 闭合与状态说明（不计入发现数）
- **GAP-005 主体（27 条旧命令 / 三命令零命中）在审计期间被 `23f42ffa`（CLI-001）闭合**：开工时点 `git show HEAD:cli/parser.cpp | grep -c normalize` = 0；收工时点 `./build/astrocs phase1 run --config …` → `astrocs: unknown command 'phase1 run'`，`help` 已是 §6.2 的 7 行。故不对该主体立条，只保留其残留（CLI-7 / CLI-9 / CLI-11 / CLI-13）。
- **GAP-006（无 config/ 等价物）**：收工时点 `config/{defaults.json,filters.json,templates}` 已存在，但预检与模板尚未引用（见 CLI-4 / CLI-5）。
- `tools/check_cli_command_layer.py` 收工时点输出 `CLI-001_PASS: 7 commands (§6.2 tree), legacy phase1/2/3 absent (rc=2 asserted), exit codes stable [static+runtime(.../build/astrocs)]`（rc=0）；但 `ci/checks.json` 正被并行线重写（HEAD 146 条 → 工作树 38 条 CHK-*，CLI-COMMAND-LAYER 折入 API-DOCS 的 steps），CLI 门覆盖须由 CI 轴复验。
- 实测**通过**、故不登记为发现的合同项：`stdout 无日志污染`（日志全在 stderr）、JSONL 10 必含字段齐备、未确认时 fail-closed 且不落任何产物、产物只落配置 `output_dir`（探针 `/tmp/acs_probe`）、旧命令收口 rc=2、`help` rc=0。
