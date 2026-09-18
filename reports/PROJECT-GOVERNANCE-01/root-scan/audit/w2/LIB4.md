# W2-LIB4 文件级审计报告 — lib/core · lib/backend_host · lib/orchestrator · lib/astro_image_io · include/ · cli/ · modules/ · providers/

- **基线**：开工 HEAD=main=`180c8a0ad9755e513670c7a1feb8fa215a87e1d7`；origin/main=`b4afc135848d8b401ee36278effcb06699542fec` 不等（并行线未推送），按指令等待 2 分钟复测仍不等，以 **HEAD=main 相等为准开工**并注明。收工 HEAD=`2fc19b2188ef1c084625bc7f8c7ee47b93687d11`（审计期间 HEAD 随并行线推进 180c8a0→23f42ff→01754fab，全部发现均按最新树重锚并复跑取证）。
- **枚举口径**：开工时点 `git ls-files` 共 **455** 个域内 tracked 文件、域内 untracked **0**（`git status --porcelain -uall` 仅 6 项 M，均已含于枚举）。审计期间并行线在**索引**中 staged 了 `lib/astro_image_io/** → lib/infrastructure/aio/**` 的 R100 整体迁移（全仓 510 renames，含 backend_host→infrastructure/benchmark/backend_host、orchestrator→infrastructure/pipeline/orchestrator 等，未提交）。astro_image_io 的 298 个路径按 **NA:moved-wip / NA:vendored / NA:binary** 判（内容审计归 infrastructure 线；第一波 AIO.md 已覆盖该树内容发现）；其余已 moved 文件按**内容**给判（经 HEAD blob / 新路径读取）。
- **方法**：读 AGENTS.md、ASTROCS_DESIGN §0/§1/§6/§7/§8/§9/§10/§11/§12、ENGINEERING_SPEC §1/§2/§4/§7/§8/§9/§10、SHARD_BRIEF §1/§2；HEAD 导出 /tmp 做 cmake configure 实测；python 解析 registry/module.yaml/适配器 descriptor；module_adapters.cpp(6352 行) 注册节/manifest 节/端口节抽查；include/ 全 23 头逐 struct 核查 acs_head(struct_size/abi_version)。
- **发现计数**：**P0=0 ｜ P1=7 ｜ P2=4**（共 11 条）。
- **跨域观察（不占发现条，移交主控/相应线）**：HEAD@01754fab 的根 CMakeLists.txt 与 HEAD 树不一致——`git archive HEAD | cmake -S . -B b` 报 6 错：add_subdirectory `lib/drizzle`/`lib/hips`/`lib/phase3_rsmp`/`lib/phase2_int/v6` 目录在 HEAD 不存在（CMakeLists.txt:184/208/697/699）+ set_target_properties astrocs_p1_drizzle/astrocs_p1_hips_writer 无目标（:242）。修复只存在于**未提交**的工作树版本（根文件归 root/INT 线）；本域 W2-LIB4-1（cli COMPATIBILITY 断裂）与之同型。

## 发现表


### W2-LIB4-1｜P1｜cli/ COMPATIBILITY 工程同名 fat-exe 且引用已删除目录，HEAD 独立配置必红

- **定位**：cli/CMakeLists.txt:10（project(astrocs_cli) 独立工程）、:216（add_executable(astrocs) 与根目标同名 fat 源集，含 ../lib/common/crypto/sha256.cpp、../lib/common/healpix/healpix_core.cpp）
- **违反条款**：ENGINEERING_SPEC §1（唯一根 CMake，不引入第二套构建入口）；ASTROCS_DESIGN §10.1（唯一可执行入口）；与文件头自述「仅保留给遗留 tests/cli 独立构建」矛盾（已不可能构建）。
- **当前证据**：命令：git archive HEAD 导出 /tmp/w2l_head2 后 cmake -S cli -B bc2 → 输出：CMake Error at CMakeLists.txt:216 (add_executable): Cannot find source file: /tmp/w2l_head2/lib/common/crypto/sha256.cpp / No SOURCES given to target: astrocs（lib/common 已于祖先提交 23acb453 删除；git show HEAD:cli/CMakeLists.txt | grep -c lib/common → 10）
- **严重度**：P1
- **影响**：COMPATIBILITY 工程名存实亡仍声称可用；遗留 tests/cli 独立构建路径全红；同名 astrocs 目标延续双二进制混淆。
- **整改建议**：删本文件或显式标 NOT_IMPLEMENTED 占位并同步清理 tests/cli 依赖；产品唯一入口名按 §10.1 收敛（ACSD Cli.exe/acsd_cli）。
- **建议文件域**：cli/、tests/cli、cmake/install_layout.cmake
- **验收门**：git archive HEAD | tar -x -C /tmp/x && cmake -S /tmp/x/cli -B /tmp/x/b → 无 CMake Error（或文件已删且无引用）。
- **同源标注**：第一波 ARCH-6 同源（同名 fat-exe）；「引用已删 lib/common 致 configure 红」为本轮新增断裂证据；归属 ARCH-001/INT-001。

### W2-LIB4-2｜P1｜退出码「唯一源 include/astrocs/exit_codes.h」不存在；orchestrator 第二套退出词表直接作进程退出码

- **定位**：cli/exit_codes.h:1-8（实际唯一源，11 码值与 §6.3 表一致）；lib/orchestrator/README.md:23（--validate INVALID→退出码 1）；lib/orchestrator/cpp/src/main.cpp:88,140,245（return 1）
- **违反条款**：ASTROCS_DESIGN §6.3（退出码唯一源 include/astrocs/exit_codes.h；码表无 1）；ENGINEERING_SPEC §9（统一状态码，同失败同码）。
- **当前证据**：命令：ls include/astrocs/exit_codes.h → 输出：No such file or directory；grep -n 'return 1;' lib/infrastructure/pipeline/orchestrator/cpp/src/main.cpp → 88/140/245 命中。
- **严重度**：P1
- **影响**：唯一源路径与权威偏差；orchestrator 退出码 0/1 逃逸 §6.3 合同，取消/失败不可机器判别。
- **整改建议**：枚举迁至 include/astrocs/exit_codes.h（cli 头改转发）；orchestrator 收尾内部码映射 11 码表。
- **建议文件域**：include/astrocs/、cli/、lib/orchestrator
- **验收门**：test -f include/astrocs/exit_codes.h && grep -c 'return 1;' lib/infrastructure/pipeline/orchestrator/cpp/src/main.cpp → 前者存在且后者=0。
- **同源标注**：第一波 AIO 轴「唯一源路径不存在」与 AIO:114「orchestrator 第二词表」同源；与 GAP-014 相关；归属 CLI-003。

### W2-LIB4-3｜P1｜Alpha 前版本信息进入程序与产物（--version、manifest 字段、module.yaml 版本串）

- **定位**：cli/version_generated.h.in:1-3；cli/commands.cpp:2309-2314（--version 打印 ASTROCS_VERSION_STRING）、:238（source_version）、:291/:1768（manifest astrocs_version 写入+verify 强校验）；modules/conformance/noop/module.yaml:9、modules/conformance/echo/module.yaml:8（module_version: 0.11.0-alpha.2）
- **违反条款**：ASTROCS_DESIGN §12（Alpha 之前程序与代码不包含任何版本信息）；ENGINEERING_SPEC §7（VERSION 仅内部助记，不进程序与产物）。
- **当前证据**：命令：grep -n 'ASTROCS_VERSION_STRING' cli/commands.cpp | wc -l → 9（含 :2314 printf）；grep -rn 'module_version' modules/conformance/*/module.yaml → 0.11.0-alpha.2 两处在树。
- **严重度**：P1
- **影响**：版本串进入 manifest/run_context 并成为 verify 强判据（:1768），阶段产物随版本漂移不可互认；§12 零版本条款失效。
- **整改建议**：Alpha 前注入置空常量、manifest 去 astrocs_version（或 record-only）；module.yaml 版本字段留空并登记。
- **建议文件域**：cli/、modules/conformance/、packaging/、cmake/install_layout.cmake
- **验收门**：grep -c 'ASTROCS_VERSION_STRING' cli/commands.cpp → 0（或 §12 变更获负责人批准）。
- **同源标注**：GAP-017、第一波 PKG-5 同源（本条为 cli/modules 文件级复核并新增 module.yaml 命中点）；归属 GOV-01/PKG-001。

### W2-LIB4-4｜P1｜register_phase_modules 注册 2 个冻结绑定表外的整阶段 Session descriptor（resample 名占整链实现）

- **定位**：lib/core/src/module_adapters.cpp:580-597（phase2_descriptor：id=astrocs.phase2.resample，工厂 make_session_module<P2Api> 跑整 P2 会话）、:599-616（phase3_descriptor：id=astrocs.phase3.resample）、:6262-6276（注册点）、:6332-6333（注释自认「占位 descriptor（P2 模板复制残留）」）
- **违反条款**：ASTROCS_DESIGN §7.3（每生产 DAG 节点映射唯一真实 module/导出入口/操作；多节点不得调用同一完整 Session）；AGENTS §5（禁 facade/占位冒充）；§11.3（未实现须如实标注）。
- **当前证据**：命令：python 比对适配器 22 个 module_id 与 runtime/pipeline/module_ports.registry.json 20 模块 → 输出：adapter-only ['astrocs.phase2.resample','astrocs.phase3.resample']（registry-only=0）。
- **严重度**：P1
- **影响**：resample 节点名承载整阶段实现；若被生产 IR 引用即复现「每子节点重复执行全链」缺陷（ARCH-P0-001 整改残留面）；注册表/适配器两套词表。
- **整改建议**：删除两 descriptor 与注册（或改名 astrocs.phase2.session 并标 LEGACY/NOT_IN_DAG）；加「注册集=绑定表集」机器门。
- **建议文件域**：lib/core/src/、runtime/pipeline/、tests/unit
- **验收门**：python 集合差（适配器 ids − registry ids）→ 空集。
- **同源标注**：GAP-004、第一波 ARCH-2 同源（整阶段 Session 在图）；绑定表差集与占位自认新证；归属 RT-001/MOD-001。

### W2-LIB4-5｜P1｜DESIGN §9 run 记录七件套在本域写入面实名零命中

- **定位**：cli/commands.cpp:364,441（static_graph.json/observed_trace.json/graph_sidecar.json）；cli/resource_recorder.h:257,281,316（resource_samples.csv/resource_summary.json/worker_balance.csv）；cli/memory_report.h:236（alloc_report.json）；lib/core/src/module_adapters.cpp:6214（run_context.json）
- **违反条款**：ASTROCS_DESIGN §9（每次运行至少生成 run-plan.json、run-graph.json、run-trace.jsonl、resource-timeseries.csv、resource-summary.json、artifact-manifest.json、run-summary.json）。
- **当前证据**：命令：grep -rn -E 'run-plan\.json|run-trace\.jsonl|resource-timeseries|artifact-manifest|run-summary' lib/core cli → 输出：0 命中（observed_trace 为 .json 非 JSONL 落盘）。
- **严重度**：P1
- **影响**：阶段间「磁盘产品+manifest+哈希」唯一交换按名不可机械核对；审计溯源合同漂移。
- **整改建议**：与 RT-03 一致——DESIGN §9 登记命名映射表（负责人批准）或落盘侧改实名并补 artifact-manifest/run-summary。
- **建议文件域**：cli/、lib/core/、contracts/schemas/
- **验收门**：grep -E 'run-plan|run-trace|resource-timeseries' cli/commands.cpp 有命中，或 §9 含映射表（二选一判绿）。
- **同源标注**：第一波 RT-03/AIO-4 同源（文件域复核）；归属 OBS-001/AIO-001。

### W2-LIB4-6｜P1｜lib/orchestrator 自带 Makefile 第二构建入口且 README 自称「唯一运行方式 orchestrator.exe」

- **定位**：lib/orchestrator/cpp/Makefile:1-12（g++/MSYS2 独立编译 orchestrator.exe）；lib/orchestrator/README.md:3-11（「唯一运行方式…不支持任何子命令」）；目录无 CMakeLists、不在根 add_subdirectory
- **违反条款**：ENGINEERING_SPEC §1（唯一根 CMake，不引入第二套构建入口）；ASTROCS_DESIGN §6.2/§10.1（唯一可执行入口 normalize/mosaic/export/help/benchmark/doctor）。
- **当前证据**：命令：git ls-files lib/orchestrator | grep -c CMakeLists → 0；grep -n 'orchestrator' CMakeLists.txt | grep -c add_subdirectory → 0；README:6「正式科学运行只有一条命令 orchestrator.exe <stage1.json>」。
- **严重度**：P1
- **影响**：与唯一入口并存的第二可执行工程仍 tracked 且文档冒充权威运行面，用户/CI 误接线风险。
- **整改建议**：整目录标 LEGACY 并按 ARCH-001 并入 infrastructure 或删除；删 Makefile（任何构建经根 CMake）。
- **建议文件域**：lib/orchestrator（staged→infrastructure/pipeline/orchestrator）、docs/
- **验收门**：test ! -f lib/orchestrator/cpp/Makefile && ! grep -q '唯一运行方式' lib/orchestrator/README.md → 绿。
- **同源标注**：GAP-014（调度双实现）与第一波 RT/AIO 系同源；Makefile=第二构建入口、README 措辞为定锚新证；归属 RT-001/ARCH-001。

### W2-LIB4-7｜P1｜orchestrator 三个 tracked 运行配置写死单机绝对路径 F:\…

- **定位**：lib/orchestrator/configs/stage1_gc_panel1_Red.json:4-7（stage1_gc_panel2_Red.json、stage1_gc_panel3_Red.json 同型）——gaia_data_dir 与 lights/master_bias 等全部指向 F 盘单机目录
- **违反条款**：AGENTS §3（不得写死服务器绝对路径）；ASTROCS_DESIGN §6.3（配置挂载 --json；产物只落 output_dir；配置须可移植）。
- **当前证据**：命令：git show HEAD:lib/orchestrator/configs/stage1_gc_panel1_Red.json | grep -n 'F:\\\\' → 输出：4/6/7 行命中；同型命中于 panel2/3。
- **严重度**：P1
- **影响**：真实科学运行配置绑定单机盘符，双平台/他人不可复跑；本机目录布局入库。
- **整改建议**：路径改相对数据根/变量，或转非入库示例并登记。
- **建议文件域**：lib/orchestrator/configs/、config/（CFG-001）
- **验收门**：grep -rn 'F:\\\\' lib/orchestrator/configs/*.json → 0 命中。
- **同源标注**：第一波/GAP/旧账本无同源（新立）；GAP-021 仅涉根产物不同型；归属 CFG-001。

### W2-LIB4-8｜P2｜12 个域内文件注释以已废止旧宪章为权威锚

- **定位**：lib/core/src/executor.cpp:6、executor_runtime.h:11、runtime.cpp:217、module_adapters.cpp:228；cli/resource_gate.h:93、commands.cpp:211、monitor.h:270、v6_runtime_contract.h:4、v6_mode_gate.h:3；include/astrocs/io/aio_abi_v1.h:7（「宪章锚 (ASTROCS-CONSTITUTION-001)」）、core/context.h:95、core/module_adapters.h:20
- **违反条款**：ASTROCS_DESIGN §0（权威链；ASTROCS_PROJECT_CONSTITUTION.md 不在链上，SHARD_BRIEF §2 明列）；ENGINEERING_SPEC §2（注释禁历史/审计流水）。
- **当前证据**：命令：grep -rl '宪章' lib/core cli include modules providers lib/orchestrator → 输出：12 文件命中（清单见日志）。
- **严重度**：P2
- **影响**：代码注释继续引用废止权威为判据锚，口径易被带偏（如资源门 85%/60% 自称「宪章 §18.2 冻结」）。
- **整改建议**：批量改指 DESIGN/ENGINEERING_SPEC 对应节号；同批清理任务编号流水。
- **建议文件域**：lib/core/、cli/、include/astrocs/
- **验收门**：grep -rl '宪章' lib/core cli include | wc -l → 0。
- **同源标注**：GAP-002/第一波 GOV-1 同族（文档面），代码注释面为新证；归属 GOV-01/DOC-001。

### W2-LIB4-9｜P2｜域内对已删除控制包标准的悬空引用（无悬空门覆盖本域）

- **定位**：include/astrocs/abi/status_codes.h:9（12_DLL_ABI_AND_LOADER_STANDARD.md）、lifecycle_v1.h:6-7（02_ABI_BUILD_CLI_TASKS.md、11_MODULE_SOURCE_TEST_STANDARD.md）、host_api_v1.h:10 / artifact_api_v1.h:11（03_TARGET §4）、contracts/artifact_abi_v1.h:3（13_DATA_PIPELINE…）；cli/commands.cpp:1889（03_TARGET_PRODUCT_AND_ARCHITECTURE.md）；modules/conformance/{noop,echo}/module.yaml:3 与 README:43/62；providers/cpu/common/README.md:4,36,59、capability_v1.h:21（15_CPU_PROVIDER…、AstroCS_ENGINEERING_CONSTRAINTS.md=ARCHIVED_NON_NORMATIVE）
- **违反条款**：ENGINEERING_SPEC §8（「删除/重命名无悬空引用」检查器覆盖）；ASTROCS_DESIGN §0（非链上文档不作判据）。
- **当前证据**：命令：for n in 12_DLL_ABI… 11_MODULE_SOURCE… 13_DATA_PIPELINE… 15_CPU_PROVIDER… 03_TARGET; git ls-files | grep -c "$n" → 输出：5 个标准名计数全 0（文件不存在）。
- **严重度**：P2
- **影响**：冻结 ABI 头的权威出处不可达；「权威=控制包 xx」措辞复活旧治理入口。
- **整改建议**：批量改指现行 docs/plugins/infrastructure 篇目或删引用；注册悬空引用 CHK。
- **建议文件域**：include/、modules/、providers/、cli/
- **验收门**：grep -rln -E '1[1235]_[A-Z_]+_STANDARD|03_TARGET' include cli modules providers → 0 文件。
- **同源标注**：GAP-019 同源（活动面引用已删控制包路径）；本条为文件域清单化；归属 DOC-001/CI-001。

### W2-LIB4-10｜P2｜echo 模块 target 不入根图/安装白名单，module.yaml 却标 IMPLEMENTED+astrocs_echo.dll

- **定位**：modules/conformance/echo/CMakeLists.txt:10（add_library(astrocs_echo SHARED)，但全仓无 add_subdirectory(modules/conformance/echo)）；modules/conformance/echo/module.yaml:5-6,10-11（module_status: IMPLEMENTED、dll_name: astrocs_echo.dll）
- **违反条款**：ASTROCS_DESIGN §11.3（IMPLEMENTED/INSTALLED 唯一口径）、§12（不得冒充完成）；ENGINEERING_SPEC §4（每模块必备 CMake target）。
- **当前证据**：命令：grep -n 'conformance/echo' CMakeLists.txt cmake/install_layout.cmake → 0 命中；tests/abi/test_abi005_echo.py:51 以 CC=gcc 测试内自编自装（仅测试面）。
- **严重度**：P2（conformance 探针无科学语义，但状态宣称失真）
- **影响**：模块清单/安装树/构建图三口径不一致，MOD-001 映射门红点。
- **整改建议**：入根图+install 白名单，或 module.yaml 降 TEST_ONLY/SKELETON 如实标注。
- **建议文件域**：modules/conformance/echo/、cmake/install_layout.cmake
- **验收门**：grep -c 'conformance/echo' CMakeLists.txt → ≥1，或 module_status 改 TEST_ONLY。
- **同源标注**：第一波 ARCH-4（module.yaml entrypoint MISSING 16/23）同族、GAP-008 相邻；echo 特例新证；归属 MOD-001。

### W2-LIB4-11｜P2｜providers/cpu 全部 10 文件零 CMake 接线；安装的 astrocs_cpu_baseline.so 实际由 lib/backend_host legacy 源构建

- **定位**：providers/cpu/baseline/src/baseline_provider.cpp（619 行 provider ABI 实现，无任何构建 target 引用）；根 CMakeLists.txt:220-224（add_library(astrocs_cpu_baseline SHARED lib/backend_host/baseline_backend.cpp)）+ :232（注释「命名对齐 install-tree contract: providers/astrocs_cpu_baseline.so」）
- **违反条款**：ASTROCS_DESIGN §7.3（模块=独立 DLL/SO 含构建 target）、§8（provider 面）；ENGINEERING_SPEC §4（CMake target 必备）。
- **当前证据**：命令：grep -rn 'providers/cpu' CMakeLists.txt cmake/ → 仅 3 处注释命中、0 源引用；git ls-files providers | wc -l → 10，git ls-files providers | grep -c CMakeLists → 0。
- **严重度**：P2（根 CMake 注释已如实登记「技术预览，正式 provider 由 CPU-002 交付」——非谎报，但同名异源双实现漂移风险实在）
- **影响**：CPU-001 的 provider/benchmark/profile 任务面落地不完整；安装单元 providers/astrocs_cpu_baseline.so 与 providers/ 源树无构建关系。
- **整改建议**：把 astrocs_cpu_baseline target 切至 providers/cpu/baseline 源，或新增根 target 并让 tests/cpu 复用，消除同名异源。
- **建议文件域**：providers/、lib/backend_host（staged→infrastructure/benchmark/backend_host）、cmake/
- **验收门**：grep -rn 'providers/cpu/baseline/src' CMakeLists.txt → ≥1 命中。
- **同源标注**：GAP-015（provider/profile 双实现）同源；归属 CPU-001。

## 覆盖清单

口径：开工枚举 455 tracked（untracked 0）。NA:moved-wip=审计期间索引内 staged R100 迁往 lib/infrastructure/aio 的原域文件（内容审计归 infrastructure 线/第一波 AIO.md）；NA:vendored=lib/astro_image_io/third_party/cfitsio 与 lib/orchestrator/cpp/third_party/json-schema-validator 的第三方 vendored 源；NA:binary=树内二进制（cfitsio docs PDF 等）。一文件涉多条 FINDING 时取主条。



cli/CMakeLists.txt	FINDING:W2-LIB4-1
cli/astrocs_process.h	OK
cli/cancel_token.h	OK
cli/cli_common.h	OK
cli/commands.cpp	FINDING:W2-LIB4-3
cli/exit_codes.h	FINDING:W2-LIB4-2
cli/jsonl.h	OK
cli/main.cpp	OK
cli/memory_growth.h	OK
cli/memory_report.h	FINDING:W2-LIB4-5
cli/monitor.h	FINDING:W2-LIB4-8
cli/parser.cpp	OK
cli/process.cpp	OK
cli/protocol.h	OK
cli/resource_events.h	OK
cli/resource_gate.h	FINDING:W2-LIB4-8
cli/resource_recorder.h	FINDING:W2-LIB4-5
cli/runtime_client.cpp	OK
cli/runtime_client.h	OK
cli/v6_mode_gate.h	FINDING:W2-LIB4-8
cli/v6_runtime_contract.h	FINDING:W2-LIB4-8
cli/version_generated.h.in	FINDING:W2-LIB4-3
include/astrocs/abi/artifact_api_v1.h	FINDING:W2-LIB4-9
include/astrocs/abi/host_api_v1.h	FINDING:W2-LIB4-9
include/astrocs/abi/lifecycle_v1.h	FINDING:W2-LIB4-9
include/astrocs/abi/module_api_v1.h	FINDING:W2-LIB4-9
include/astrocs/abi/status_codes.h	FINDING:W2-LIB4-9
include/astrocs/common_abi_v1.h	OK
include/astrocs/contracts/artifact_abi_v1.h	FINDING:W2-LIB4-9
include/astrocs/core/artifact.h	OK
include/astrocs/core/artifact_store.h	OK
include/astrocs/core/checkpoint.h	OK
include/astrocs/core/context.h	FINDING:W2-LIB4-8
include/astrocs/core/contracts.h	OK
include/astrocs/core/executor.h	OK
include/astrocs/core/logging.h	OK
include/astrocs/core/module.h	OK
include/astrocs/core/module_adapters.h	FINDING:W2-LIB4-8
include/astrocs/core/pipeline.h	OK
include/astrocs/core/plan_estimator.h	OK
include/astrocs/core/runtime.h	OK
include/astrocs/core/scheduler.h	OK
include/astrocs/core/trace.h	OK
include/astrocs/io/aio_abi_v1.h	FINDING:W2-LIB4-8
include/astrocs/io/io_adapter.h	OK
lib/astro_image_io/.gitignore	NA:moved-wip
lib/astro_image_io/Makefile	NA:moved-wip
lib/astro_image_io/README.md	NA:moved-wip
lib/astro_image_io/aio_build_config.full.json	NA:moved-wip
lib/astro_image_io/aio_build_config.healpix.json	NA:moved-wip
lib/astro_image_io/aio_build_config.json	NA:moved-wip
lib/astro_image_io/aio_build_config.minimal.json	NA:moved-wip
lib/astro_image_io/build.ps1	NA:moved-wip
lib/astro_image_io/docs/HEALPIX_FORMAT_SPEC.md	NA:moved-wip
lib/astro_image_io/include/aio_ahpx_format.h	NA:moved-wip
lib/astro_image_io/include/aio_healpix_io.h	NA:moved-wip
lib/astro_image_io/include/aio_hips.h	NA:moved-wip
lib/astro_image_io/include/aio_hips_reader.h	NA:moved-wip
lib/astro_image_io/include/aio_pipeline.h	NA:moved-wip
lib/astro_image_io/include/aio_pipeline_engine.h	NA:moved-wip
lib/astro_image_io/include/aio_upm.h	NA:moved-wip
lib/astro_image_io/include/astro_image_io.h	NA:moved-wip
lib/astro_image_io/include/hiss_format.h	NA:moved-wip
lib/astro_image_io/memory.md	NA:moved-wip
lib/astro_image_io/pyproject.toml	NA:moved-wip
lib/astro_image_io/src/ahpx/DEPRECATED.md	NA:moved-wip
lib/astro_image_io/src/ahpx/aio_ahpx_api.cpp	NA:moved-wip
lib/astro_image_io/src/ahpx/aio_ahpx_reader.cpp	NA:moved-wip
lib/astro_image_io/src/ahpx/aio_ahpx_reader.h	NA:moved-wip
lib/astro_image_io/src/ahpx/aio_ahpx_writer.cpp	NA:moved-wip
lib/astro_image_io/src/ahpx/aio_ahpx_writer.h	NA:moved-wip
lib/astro_image_io/src/aio_abi.cpp	NA:moved-wip
lib/astro_image_io/src/aio_api.cpp	NA:moved-wip
lib/astro_image_io/src/aio_cfitsio_mutex.h	NA:moved-wip
lib/astro_image_io/src/aio_compressor.cpp	NA:moved-wip
lib/astro_image_io/src/aio_compressor.h	NA:moved-wip
lib/astro_image_io/src/aio_fits.cpp	NA:moved-wip
lib/astro_image_io/src/aio_fits.h	NA:moved-wip
lib/astro_image_io/src/aio_log.cpp	NA:moved-wip
lib/astro_image_io/src/aio_log.h	NA:moved-wip
lib/astro_image_io/src/aio_pipeline.cpp	NA:moved-wip
lib/astro_image_io/src/aio_pipeline_engine.cpp	NA:moved-wip
lib/astro_image_io/src/aio_upm.cpp	NA:moved-wip
lib/astro_image_io/src/aio_util.h	NA:moved-wip
lib/astro_image_io/src/aio_xisf.cpp	NA:moved-wip
lib/astro_image_io/src/aio_xisf.h	NA:moved-wip
lib/astro_image_io/src/healpix/aio_healpix_io.cpp	NA:moved-wip
lib/astro_image_io/src/hips/aio_hips_reader.cpp	NA:moved-wip
lib/astro_image_io/src/hips/aio_hips_writer.cpp	NA:moved-wip
lib/astro_image_io/src/hiss_codec.cpp	NA:moved-wip
lib/astro_image_io/src/hiss_common.cpp	NA:moved-wip
lib/astro_image_io/src/hiss_reader.cpp	NA:moved-wip
lib/astro_image_io/src/hiss_stream_writer.cpp	NA:moved-wip
lib/astro_image_io/src/hiss_stream_writer.h	NA:moved-wip
lib/astro_image_io/src/hiss_tile_model.cpp	NA:moved-wip
lib/astro_image_io/src/hiss_tile_model.h	NA:moved-wip
lib/astro_image_io/src/hiss_transform.cpp	NA:moved-wip
lib/astro_image_io/src/hiss_transform.h	NA:moved-wip
lib/astro_image_io/src/hiss_writer.cpp	NA:moved-wip
lib/astro_image_io/tests/dataflow_fuzz.cpp	NA:moved-wip
lib/astro_image_io/tests/gaia_sanitize_driver.c	NA:moved-wip
lib/astro_image_io/tests/hips_direct_smoke.py	NA:moved-wip
lib/astro_image_io/tests/hips_mapping_oracle.py	NA:moved-wip
lib/astro_image_io/tests/hips_robust_sanitize_driver.cpp	NA:moved-wip
lib/astro_image_io/tests/hips_sanitize_driver.cpp	NA:moved-wip
lib/astro_image_io/tests/hiss_benchmark.cpp	NA:moved-wip
lib/astro_image_io/tests/hiss_correctness_test.cpp	NA:moved-wip
lib/astro_image_io/tests/hiss_experiment_suite.cpp	NA:moved-wip
lib/astro_image_io/tests/hiss_experiments.cpp	NA:moved-wip
lib/astro_image_io/tests/hiss_writer_smoke.cpp	NA:moved-wip
lib/astro_image_io/tests/p1hips/CMakeLists.txt	NA:moved-wip
lib/astro_image_io/tests/p1hips/p1hips_digest_verify.cpp	NA:moved-wip
lib/astro_image_io/tests/p1hips/p1hips_fixtures.hpp	NA:moved-wip
lib/astro_image_io/tests/p1hips/p1hips_oracle.hpp	NA:moved-wip
lib/astro_image_io/tests/p1hips/p1hips_test_main.hpp	NA:moved-wip
lib/astro_image_io/tests/p1hips/p1hips_tests_diag_prov.cpp	NA:moved-wip
lib/astro_image_io/tests/p1hips/p1hips_tests_main.cpp	NA:moved-wip
lib/astro_image_io/tests/p1hips/p1hips_tests_negative.cpp	NA:moved-wip
lib/astro_image_io/tests/p1hips/p1hips_tests_oracle.cpp	NA:moved-wip
lib/astro_image_io/tests/p1hips/p1hips_tests_perf.cpp	NA:moved-wip
lib/astro_image_io/tests/p1hips/p1hips_tests_properties.cpp	NA:moved-wip
lib/astro_image_io/tests/p1hips/p1hips_tests_selfcheck.cpp	NA:moved-wip
lib/astro_image_io/tests/p1hips/p1hips_tests_units.cpp	NA:moved-wip
lib/astro_image_io/tests/p2hips/CMakeLists.txt	NA:moved-wip
lib/astro_image_io/tests/p2hips/p2hips_test_main.hpp	NA:moved-wip
lib/astro_image_io/tests/p2hips/p2hips_unc_prov_test.cpp	NA:moved-wip
lib/astro_image_io/tests/pipeline_frame_contract_test.cpp	NA:moved-wip
lib/astro_image_io/tests/results/dq001_codec_comparison.csv	NA:moved-wip
lib/astro_image_io/tests/results/dq002_occupancy_mode.csv	NA:moved-wip
lib/astro_image_io/tests/results/dq003_random_read_latency.csv	NA:moved-wip
lib/astro_image_io/tests/results/dq004_drizzle_profile.csv	NA:moved-wip
lib/astro_image_io/tests/results/dq005_writer_memory.csv	NA:moved-wip
lib/astro_image_io/tests/results/dq006_auto_nside.csv	NA:moved-wip
lib/astro_image_io/tests/results/dq007_signal_support_semantics.csv	NA:moved-wip
lib/astro_image_io/tests/results/performance_report.md	NA:moved-wip
lib/astro_image_io/tests/sanitize_wsl.sh	NA:moved-wip
lib/astro_image_io/tests/sanitize_wsl_v4.sh	NA:moved-wip
lib/astro_image_io/tests/sanitize_wsl_v5.sh	NA:moved-wip
lib/astro_image_io/tests/test_checksum.cpp	NA:moved-wip
lib/astro_image_io/tests/test_drizzle_integration.cpp	NA:moved-wip
lib/astro_image_io/tests/test_export_fits_fix.py	NA:moved-wip
lib/astro_image_io/tests/test_healpix_io.py	NA:moved-wip
lib/astro_image_io/tests/test_healpix_io_py.py	NA:moved-wip
lib/astro_image_io/tests/test_output.txt	NA:moved-wip
lib/astro_image_io/tests/test_p0_io_hardening.cpp	NA:moved-wip
lib/astro_image_io/tests/test_p1_io_hardening.cpp	NA:moved-wip
lib/astro_image_io/tests/test_pipeline_blocks.py	NA:moved-wip
lib/astro_image_io/tests/test_precision_dual.cpp	NA:moved-wip
lib/astro_image_io/tests/test_psf_fit_handler.py	NA:moved-wip
lib/astro_image_io/tests/test_query_pixel.cpp	NA:moved-wip
lib/astro_image_io/tests/test_report.md	NA:moved-wip
lib/astro_image_io/tests/test_snr_unknown_block.cpp	NA:moved-wip
lib/astro_image_io/tests/test_tile_model.cpp	NA:moved-wip
lib/astro_image_io/tests/test_transform.cpp	NA:moved-wip
lib/astro_image_io/tests/test_wph_cli_browser.cpp	NA:moved-wip
lib/astro_image_io/tests/test_writer_integration.cpp	NA:moved-wip
lib/astro_image_io/tests/v5_maptile_oracle.py	NA:moved-wip
lib/astro_image_io/tests/v5_snr_precision_roundtrip.py	NA:moved-wip
lib/astro_image_io/third_party/cfitsio/.gitattributes	NA:vendored
lib/astro_image_io/third_party/cfitsio/.github/workflows/ci.yml	NA:vendored
lib/astro_image_io/third_party/cfitsio/.github/workflows/codeql.yml	NA:vendored
lib/astro_image_io/third_party/cfitsio/.github/workflows/documentation.yml	NA:vendored
lib/astro_image_io/third_party/cfitsio/.gitignore	NA:vendored
lib/astro_image_io/third_party/cfitsio/CMakeLists.txt	NA:vendored
lib/astro_image_io/third_party/cfitsio/ChangeLog	NA:vendored
lib/astro_image_io/third_party/cfitsio/INSTALL	NA:vendored
lib/astro_image_io/third_party/cfitsio/Makefile.am	NA:vendored
lib/astro_image_io/third_party/cfitsio/Makefile.in	NA:vendored
lib/astro_image_io/third_party/cfitsio/README.MacOS	NA:vendored
lib/astro_image_io/third_party/cfitsio/README.md	NA:vendored
lib/astro_image_io/third_party/cfitsio/README.win	NA:vendored
lib/astro_image_io/third_party/cfitsio/README_OLD.win	NA:vendored
lib/astro_image_io/third_party/cfitsio/SECURITY.md	NA:vendored
lib/astro_image_io/third_party/cfitsio/aclocal.m4	NA:vendored
lib/astro_image_io/third_party/cfitsio/buffers.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/cfileio.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/cfitsio.pc.cmake	NA:vendored
lib/astro_image_io/third_party/cfitsio/cfitsio.pc.in	NA:vendored
lib/astro_image_io/third_party/cfitsio/cfitsio.spec	NA:vendored
lib/astro_image_io/third_party/cfitsio/cfitsio.xcodeproj/project.pbxproj	NA:vendored
lib/astro_image_io/third_party/cfitsio/cfortran.h	NA:vendored
lib/astro_image_io/third_party/cfitsio/checksum.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/cmake/cfitsioConfig.cmake	NA:vendored
lib/astro_image_io/third_party/cfitsio/cmake/portfile.cmake	NA:vendored
lib/astro_image_io/third_party/cfitsio/cmake/vcpkg.json	NA:vendored
lib/astro_image_io/third_party/cfitsio/config/compile	NA:vendored
lib/astro_image_io/third_party/cfitsio/config/config.guess	NA:vendored
lib/astro_image_io/third_party/cfitsio/config/config.sub	NA:vendored
lib/astro_image_io/third_party/cfitsio/config/depcomp	NA:vendored
lib/astro_image_io/third_party/cfitsio/config/install-sh	NA:vendored
lib/astro_image_io/third_party/cfitsio/config/ltmain.sh	NA:vendored
lib/astro_image_io/third_party/cfitsio/config/missing	NA:vendored
lib/astro_image_io/third_party/cfitsio/config/test-driver	NA:vendored
lib/astro_image_io/third_party/cfitsio/configure	NA:vendored
lib/astro_image_io/third_party/cfitsio/configure.ac	NA:vendored
lib/astro_image_io/third_party/cfitsio/docs/cfitsio.pdf	NA:binary
lib/astro_image_io/third_party/cfitsio/docs/cfitsio.tex	NA:vendored
lib/astro_image_io/third_party/cfitsio/docs/cfortran.doc	NA:vendored
lib/astro_image_io/third_party/cfitsio/docs/fitsio.pdf	NA:binary
lib/astro_image_io/third_party/cfitsio/docs/fitsio.tex	NA:vendored
lib/astro_image_io/third_party/cfitsio/docs/fpackguide.md	NA:vendored
lib/astro_image_io/third_party/cfitsio/docs/fpackguide.pdf	NA:binary
lib/astro_image_io/third_party/cfitsio/docs/quick.pdf	NA:binary
lib/astro_image_io/third_party/cfitsio/docs/quick.tex	NA:vendored
lib/astro_image_io/third_party/cfitsio/drvrfile.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/drvrgsiftp.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/drvrgsiftp.h	NA:vendored
lib/astro_image_io/third_party/cfitsio/drvrmem.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/drvrnet.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/drvrsmem.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/drvrsmem.h	NA:vendored
lib/astro_image_io/third_party/cfitsio/editcol.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/edithdu.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/eval.l	NA:vendored
lib/astro_image_io/third_party/cfitsio/eval.y	NA:vendored
lib/astro_image_io/third_party/cfitsio/eval_defs.h	NA:vendored
lib/astro_image_io/third_party/cfitsio/eval_f.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/eval_l.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/eval_tab.h	NA:vendored
lib/astro_image_io/third_party/cfitsio/eval_y.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/f77.inc	NA:vendored
lib/astro_image_io/third_party/cfitsio/f77_wrap.h	NA:vendored
lib/astro_image_io/third_party/cfitsio/f77_wrap1.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/f77_wrap2.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/f77_wrap3.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/f77_wrap4.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/fits_hcompress.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/fits_hdecompress.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/fitscore.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/fitsio.h	NA:vendored
lib/astro_image_io/third_party/cfitsio/fitsio2.h	NA:vendored
lib/astro_image_io/third_party/cfitsio/getcol.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/getcolb.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/getcold.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/getcole.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/getcoli.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/getcolj.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/getcolk.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/getcoll.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/getcols.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/getcolsb.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/getcolui.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/getcoluj.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/getcoluk.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/getkey.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/group.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/group.h	NA:vendored
lib/astro_image_io/third_party/cfitsio/grparser.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/grparser.h	NA:vendored
lib/astro_image_io/third_party/cfitsio/histo.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/imcompress.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/iraffits.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/iter_a.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/iter_a.f	NA:vendored
lib/astro_image_io/third_party/cfitsio/iter_b.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/iter_b.f	NA:vendored
lib/astro_image_io/third_party/cfitsio/iter_c.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/iter_c.f	NA:vendored
lib/astro_image_io/third_party/cfitsio/licenses/License.txt	NA:vendored
lib/astro_image_io/third_party/cfitsio/longnam.h	NA:vendored
lib/astro_image_io/third_party/cfitsio/m4/ax_cfitsio.m4	NA:vendored
lib/astro_image_io/third_party/cfitsio/m4/libtool.m4	NA:vendored
lib/astro_image_io/third_party/cfitsio/m4/ltoptions.m4	NA:vendored
lib/astro_image_io/third_party/cfitsio/m4/ltsugar.m4	NA:vendored
lib/astro_image_io/third_party/cfitsio/m4/ltversion.m4	NA:vendored
lib/astro_image_io/third_party/cfitsio/m4/lt~obsolete.m4	NA:vendored
lib/astro_image_io/third_party/cfitsio/modkey.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/pliocomp.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/putcol.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/putcolb.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/putcold.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/putcole.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/putcoli.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/putcolj.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/putcolk.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/putcoll.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/putcols.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/putcolsb.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/putcolu.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/putcolui.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/putcoluj.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/putcoluk.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/putkey.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/quantize.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/region.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/region.h	NA:vendored
lib/astro_image_io/third_party/cfitsio/ricecomp.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/run-testprog	NA:vendored
lib/astro_image_io/third_party/cfitsio/sample.tpl	NA:vendored
lib/astro_image_io/third_party/cfitsio/scalnull.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/simplerng.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/simplerng.h	NA:vendored
lib/astro_image_io/third_party/cfitsio/swapproc.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/testf77.std	NA:vendored
lib/astro_image_io/third_party/cfitsio/testprog.std	NA:vendored
lib/astro_image_io/third_party/cfitsio/testprog.tpt	NA:vendored
lib/astro_image_io/third_party/cfitsio/utilities/cookbook.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/utilities/cookbook.f	NA:vendored
lib/astro_image_io/third_party/cfitsio/utilities/fitscopy.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/utilities/fitsverify.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/utilities/fpack.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/utilities/fpack.h	NA:vendored
lib/astro_image_io/third_party/cfitsio/utilities/fpackutil.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/utilities/ftverify.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/utilities/funpack.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/utilities/fverify.h	NA:vendored
lib/astro_image_io/third_party/cfitsio/utilities/fvrf_data.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/utilities/fvrf_file.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/utilities/fvrf_head.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/utilities/fvrf_key.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/utilities/fvrf_misc.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/utilities/imcopy.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/utilities/iter_image.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/utilities/iter_var.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/utilities/smem.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/utilities/speed.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/utilities/testf77.f	NA:vendored
lib/astro_image_io/third_party/cfitsio/utilities/testprog.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/vmsieee.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/wcssub.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/wcsutil.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/winDumpExts.mak	NA:vendored
lib/astro_image_io/third_party/cfitsio/win_compat/unistd.h	NA:vendored
lib/astro_image_io/third_party/cfitsio/windumpexts.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/zcompress.c	NA:vendored
lib/astro_image_io/third_party/cfitsio/zuncompress.c	NA:vendored
lib/astro_image_io/v6/CMakeLists.txt	NA:moved-wip
lib/astro_image_io/v6/README.md	NA:moved-wip
lib/astro_image_io/v6/include/astro/aio/v6_atomic_publish.h	NA:moved-wip
lib/astro_image_io/v6/include/astro/aio/v6_bunit.h	NA:moved-wip
lib/astro_image_io/v6/include/astro/aio/v6_fits.h	NA:moved-wip
lib/astro_image_io/v6/include/astro/aio/v6_hips_manifest.h	NA:moved-wip
lib/astro_image_io/v6/include/astro/aio/v6_product_io.h	NA:moved-wip
lib/astro_image_io/v6/include/astro/aio/v6_provenance.h	NA:moved-wip
lib/astro_image_io/v6/include/astro/aio/v6_sha256.h	NA:moved-wip
lib/astro_image_io/v6/include/astro/aio/v6_validation.h	NA:moved-wip
lib/astro_image_io/v6/src/v6_atomic_publish.cpp	NA:moved-wip
lib/astro_image_io/v6/src/v6_bunit.cpp	NA:moved-wip
lib/astro_image_io/v6/src/v6_fits.cpp	NA:moved-wip
lib/astro_image_io/v6/src/v6_hips_manifest.cpp	NA:moved-wip
lib/astro_image_io/v6/src/v6_product_io.cpp	NA:moved-wip
lib/astro_image_io/v6/src/v6_provenance.cpp	NA:moved-wip
lib/astro_image_io/v6/src/v6_sha256.cpp	NA:moved-wip
lib/backend_host/avx2_backend.cpp	OK
lib/backend_host/avx512_backend.cpp	OK
lib/backend_host/avx_backend.cpp	OK
lib/backend_host/backend_loader.cpp	OK
lib/backend_host/backend_loader.h	OK
lib/backend_host/backend_table.inc	OK
lib/backend_host/baseline_backend.cpp	OK
lib/backend_host/baseline_kernels.h	OK
lib/backend_host/baseline_kernels_impl.inc	OK
lib/backend_host/bench_harness.cpp	OK
lib/backend_host/bench_harness.h	OK
lib/backend_host/bench_report.cpp	OK
lib/backend_host/bench_report.h	OK
lib/backend_host/cpu_features.cpp	OK
lib/backend_host/cpu_features.h	OK
lib/backend_host/cpu_routing.cpp	OK
lib/backend_host/cpu_routing.h	OK
lib/backend_host/hardware_inspect.cpp	OK
lib/backend_host/hardware_inspect.h	OK
lib/backend_host/host_services.cpp	OK
lib/backend_host/profile_gen.cpp	OK
lib/backend_host/profile_gen.h	OK
lib/backend_host/profile_gen_v2.cpp	OK
lib/backend_host/profile_store.cpp	OK
lib/backend_host/profile_store.h	OK
lib/backend_host/worker_advisor.cpp	OK
lib/backend_host/worker_advisor.h	OK
lib/core/README.md	OK
lib/core/src/artifact.cpp	OK
lib/core/src/artifact_store.cpp	OK
lib/core/src/checkpoint.cpp	OK
lib/core/src/context.cpp	OK
lib/core/src/executor.cpp	FINDING:W2-LIB4-8
lib/core/src/executor_runtime.h	FINDING:W2-LIB4-8
lib/core/src/logging.cpp	OK
lib/core/src/module.cpp	OK
lib/core/src/module_adapters.cpp	FINDING:W2-LIB4-4
lib/core/src/pipeline.cpp	OK
lib/core/src/plan_estimator.cpp	OK
lib/core/src/runtime.cpp	FINDING:W2-LIB4-8
lib/core/src/scheduler.cpp	OK
lib/orchestrator/.gitignore	OK
lib/orchestrator/README.md	FINDING:W2-LIB4-6
lib/orchestrator/configs/stage1.schema.json	OK
lib/orchestrator/configs/stage1.template.json	OK
lib/orchestrator/configs/stage1_gc_panel1_Red.json	FINDING:W2-LIB4-7
lib/orchestrator/configs/stage1_gc_panel2_Red.json	FINDING:W2-LIB4-7
lib/orchestrator/configs/stage1_gc_panel3_Red.json	FINDING:W2-LIB4-7
lib/orchestrator/cpp/.gitignore	OK
lib/orchestrator/cpp/Makefile	FINDING:W2-LIB4-6
lib/orchestrator/cpp/include/admission_controller.h	OK
lib/orchestrator/cpp/include/checkpoint.h	OK
lib/orchestrator/cpp/include/cli_command.h	OK
lib/orchestrator/cpp/include/dll_loader.h	OK
lib/orchestrator/cpp/include/json_config.h	OK
lib/orchestrator/cpp/include/logger.h	OK
lib/orchestrator/cpp/include/orchestrator.h	OK
lib/orchestrator/cpp/include/resource_monitor.h	OK
lib/orchestrator/cpp/include/spill_manager.h	OK
lib/orchestrator/cpp/src/checkpoint.cpp	OK
lib/orchestrator/cpp/src/cli_command.cpp	OK
lib/orchestrator/cpp/src/dll_loader.cpp	OK
lib/orchestrator/cpp/src/json_config.cpp	OK
lib/orchestrator/cpp/src/logger.cpp	OK
lib/orchestrator/cpp/src/main.cpp	FINDING:W2-LIB4-2
lib/orchestrator/cpp/src/orchestrator.cpp	OK
lib/orchestrator/cpp/tests/test_checkpoint.cpp	OK
lib/orchestrator/cpp/tests/test_dll_loader.cpp	OK
lib/orchestrator/cpp/tests/test_logger.cpp	OK
lib/orchestrator/cpp/tests/test_orchestrator_cli.cpp	OK
lib/orchestrator/cpp/tests/test_p1_batchB_fixes.cpp	OK
lib/orchestrator/cpp/tests/test_p1_batchH_star_coord.cpp	OK
lib/orchestrator/cpp/third_party/json-schema-validator/CMakeLists.txt	OK
lib/orchestrator/cpp/third_party/json-schema-validator/json-patch.cpp	OK
lib/orchestrator/cpp/third_party/json-schema-validator/json-patch.hpp	OK
lib/orchestrator/cpp/third_party/json-schema-validator/json-schema-draft7.json.cpp	OK
lib/orchestrator/cpp/third_party/json-schema-validator/json-uri.cpp	OK
lib/orchestrator/cpp/third_party/json-schema-validator/json-validator.cpp	OK
lib/orchestrator/cpp/third_party/json-schema-validator/nlohmann/json-schema.hpp	OK
lib/orchestrator/cpp/third_party/json-schema-validator/smtp-address-validator.cpp	OK
lib/orchestrator/cpp/third_party/json-schema-validator/smtp-address-validator.hpp	OK
lib/orchestrator/cpp/third_party/json-schema-validator/string-format-check.cpp	OK
lib/orchestrator/docs/architecture.md	OK
lib/orchestrator/memory.md	OK
lib/orchestrator/tests/__init__.py	OK
lib/orchestrator/tests/test_orchestrator_e2e.py	OK
modules/conformance/echo/CMakeLists.txt	FINDING:W2-LIB4-10
modules/conformance/echo/README.md	FINDING:W2-LIB4-9
modules/conformance/echo/include/astrocs/echo/types.h	FINDING:W2-LIB4-10
modules/conformance/echo/module.yaml	FINDING:W2-LIB4-3
modules/conformance/echo/src/echo_module.c	FINDING:W2-LIB4-10
modules/conformance/echo/tests/unit/echo_host_callback_test.c	FINDING:W2-LIB4-10
modules/conformance/noop/CMakeLists.txt	OK
modules/conformance/noop/README.md	FINDING:W2-LIB4-9
modules/conformance/noop/include/astrocs/noop/types.h	OK
modules/conformance/noop/module.yaml	FINDING:W2-LIB4-3
modules/conformance/noop/src/noop_module.c	OK
modules/conformance/noop/tests/unit/noop_handshake_test.c	OK
modules/services/io/include/astrocs/io/fits_stream_v1.h	OK
modules/services/io/include/astrocs/io/hips_input_v1.h	OK
modules/services/io/tests/fits_core_selftest.c	OK
modules/services/io/tests/hips_core_selftest.c	OK
providers/cpu/avx2/include/astrocs/cpu/avx2_provider_v1.h	FINDING:W2-LIB4-11
providers/cpu/avx2/src/avx2_provider.cpp	FINDING:W2-LIB4-11
providers/cpu/avx512/include/astrocs/cpu/avx512_provider_v1.h	FINDING:W2-LIB4-11
providers/cpu/avx512/src/avx512_provider.cpp	FINDING:W2-LIB4-11
providers/cpu/baseline/include/astrocs/cpu/baseline_provider_v1.h	FINDING:W2-LIB4-11
providers/cpu/baseline/src/baseline_provider.cpp	FINDING:W2-LIB4-11
providers/cpu/common/README.md	FINDING:W2-LIB4-9
providers/cpu/common/include/astrocs/cpu/capability_v1.h	FINDING:W2-LIB4-9
providers/cpu/common/schemas/cpu_capability.schema.json	FINDING:W2-LIB4-11
providers/cpu/common/src/capability_detect.c	FINDING:W2-LIB4-11

---
**files_total** = 455
**verdict_counts** = {'FINDING:W2-LIB4-1': 1, 'FINDING:W2-LIB4-10': 4, 'FINDING:W2-LIB4-11': 8, 'FINDING:W2-LIB4-2': 2, 'FINDING:W2-LIB4-3': 4, 'FINDING:W2-LIB4-4': 1, 'FINDING:W2-LIB4-5': 2, 'FINDING:W2-LIB4-6': 2, 'FINDING:W2-LIB4-7': 3, 'FINDING:W2-LIB4-8': 10, 'FINDING:W2-LIB4-9': 10, 'NA:binary': 4, 'NA:moved-wip': 130, 'NA:vendored': 164, 'OK': 110}
**枚举命令(逐字)** = cd "/workspace/Astro CS Database" && git ls-files lib/core lib/common lib/backend_host lib/orchestrator lib/astro_image_io lib/astrocs_runtime lib/astrocs_io lib/astrocs_cpu_baseline include cli modules providers > /tmp/w2l_all.txt（开工时点 455；untracked 补充：git status --porcelain -uall -- <同前缀> 仅 6 项 M 无新增）
