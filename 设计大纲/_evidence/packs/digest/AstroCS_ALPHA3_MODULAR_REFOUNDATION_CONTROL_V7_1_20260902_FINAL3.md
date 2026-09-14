# 包取证摘要：AstroCS_ALPHA3_MODULAR_REFOUNDATION_CONTROL_V7_1_20260902_FINAL3

身份由脚本归一（去复原前缀与 .zip 后缀）。共 4 个实例。

## 【zip】engineering/control/archive/2026-09-09_superseded_V8.1_CI_CONTROL_20260905/baseline/AstroCS_ALPHA3_MODULAR_REFOUNDATION_CONTROL_V7_1_20260902_FINAL3.zip
- 文件 99 | 388.7 KB
- sha256: 005448fb3b6892d3063d23149692fbd9226a892a2c1891e4cb8d14a80e8491c3
- git 事件: {'first': None, 'last': None, 'dels': None, 'adds_n': 0, 'mod_n': None, 'touch_last': '8e03d7da9feacd9a85f6ecd02aab51d4a61b5a0e 2026-09-10T02:22:41+08:00'}
- zip 顶层: 00_READ_FIRST.md, 01_OWNER_FROZEN_CONSTRAINTS.md, 02_CURRENT_BASELINE_AUDIT.md, 03_TARGET_PRODUCT_AND_ARCHITECTURE.md, 04_FOREGROUND_AGENT_RUNBOOK.md, 05_FIXED_SUBAGENT_BINDINGS.yaml
- zip 内 marker: 00_READ_FIRST.md, START_PROMPT.txt
- zip 内规格文件: 00_READ_FIRST.md, 01_OWNER_FROZEN_CONSTRAINTS.md, 02_CURRENT_BASELINE_AUDIT.md, 03_TARGET_PRODUCT_AND_ARCHITECTURE.md, 04_FOREGROUND_AGENT_RUNBOOK.md, 05_FIXED_SUBAGENT_BINDINGS.yaml, 06_TASK_GRAPH_AND_WAVES.md, 07_CHECKPOINTS_AND_GATES.md, 08_GIT_MAIN_ONLY_INTEGRATION.md, 09_WINDOWS_TOOLCHAIN_LOCK.md, 10_LINUX_CONTROL_NODE.md, 11_MODULE_SOURCE_TEST_STANDARD.md, 12_DLL_ABI_AND_LOADER_STANDARD.md, 13_DATA_PIPELINE_AND_ARTIFACT_STANDARD.md, 14_RUNTIME_SCHEDULER_AND_TRACE_STANDARD.md, 15_CPU_PROVIDER_AND_RESOURCE_STANDARD.md, 16_SCIENCE_DOCUMENT_AND_TRACEABILITY_STANDARD.md, 17_INDEPENDENT_REVIEWER.md, 18_AUDIT_PACKAGE_SPEC.md, 19_MACHINE_CHECKS.md, 20_RELEASE_GATES.md, 21_PRIMARY_REFERENCES.md, 22_FAILURE_ESCALATION.md, 23_GRAPH_AND_DOC_TOOL_POLICY.md, 00_FOREGROUND_AGENT_CHECKLIST.md, 01_SUBAGENT_RETURN_CHECKLIST.md, 02_MODULE_ACCEPTANCE_CHECKLIST.md, 03_WINDOWS_RELEASE_CHECKLIST.md, 04_AUDIT_PACKAGE_CHECKLIST.md, 05_INDEPENDENT_REVIEW_CHECKLIST.md, 00_TASK_EXECUTION_CONTRACT.md, 01_W0_GOVERNANCE_TASKS.md, 02_ABI_BUILD_CLI_TASKS.md, 03_RUNTIME_DATA_IO_TASKS.md, 04_CPU_RESOURCE_TASKS.md, 05_PHASE1_TASKS.md, 06_PHASE2_TASKS.md, 07_PHASE3_TASKS.md, 08_QA_LINUX_TASKS.md, 09_WINDOWS_RELEASE_TASKS.md
- zip 内台账: TASK_LEDGER.csv 表头 ['task_id', 'wave', 'owner_id', 'kind', 'depends_on', 'mutex_lock', 'resource_class', 'commit_subject', 'spec_ref', 'acceptance', 'alias_of'] 行 191
- 00_READ_FIRST 开头:

# AstroCS Alpha 模块化重构控制包：先读我

控制包 ID：`ASTROCS-ALPHA3-MODULAR-REFOUNDATION-V7`  
发布日期：`2026-09-02`  
冻结起始提交：`c1696156583c68c9a3a65287639c726937e9f6e7`  
冻结起始版本：`0.10.0-alpha.2`  
目标版本：`0.11.0-alpha.1`  
正式分支：`main`  
正式平台：Windows x64；Linux x64 为控制/轻验证平台  
状态：可执行控制包；任何历史 V4/V5/V6/V18/V19 控制文件均不得覆盖本包。

## 1. 本轮唯一目标

把当前能够工作的但架构、文档和执行真实性仍不可靠的 AstroCS，收敛为可预发布的纯 CPU 模块化产品，同时保持既有科学定义不因架构迁移而漂移。

必须同时实现：

1. Phase1、Phase2、Phase3 分别运行、分别配置、分别验收；不得在同一进程自动串联。
2. Windows 发布物只有一个用户入口 `astrocs.exe`，科学模块、Runtime、I/O、CPU provider 以 DLL 交付。
3. 每个 DAG 节点绑定一个真实模块操作，不得以多个节点重复包装同一个 Session。
4. 一个共享执行器和一个真实 ThreadBudget 管理所有重计算；重计算不得单线程或长期低利用率。
5. ACR 保留但不进入当前生产构建、加载、benchmark 或发布包。
6. baseline/AVX2/FMA/AVX-512 仅在热点 kernel 上提供，按硬件安全检查、正确性自测和逐 kernel benchmark 选择；无 profile 一律 baseline。
7. SCI → ALG → DATA/API/ARCH → 源码符号/module manifest → TEST → 当前提交证据全链闭合。
8. 每个模块有独立源码、README、manifest 和可复用 unit/property/oracle/negative/performance 测试。
9. Phase3 独立完成 TAN、SIN、ZEA、CAR、AIT，以及 `nearest`、`healpix_interp4`，使用流式 FITS 输出。
10. 最终在 Windows 进行一次候选提交的真实数据/32R/接缝/资源验证，不反复跑历史全版本。

## 2. 执行者第一小时必须完成

前台 Agent `FG-000` 依次执行，任何一步失败均不得把控制包称为有效：

1. 在仓库外建立本轮工作根：`ASTROCS_WORK_ROOT/{control,worktrees,returns,logs,reports,buil


## 【zip】工程控制/AstroCS_ALPHA0.11.0_EXISTING_WORKSPACE_CI_CONTROL_V8_1_20260905/baseline/AstroCS_ALPHA3_MODULAR_REFOUNDATION_CONTROL_V7_1_20260902_FINAL3.zip
- 文件 99 | 388.7 KB
- sha256: 005448fb3b6892d3063d23149692fbd9226a892a2c1891e4cb8d14a80e8491c3
- git 事件: {'first': None, 'last': None, 'dels': None, 'adds_n': 0, 'mod_n': None, 'touch_last': None}
- zip 顶层: 00_READ_FIRST.md, 01_OWNER_FROZEN_CONSTRAINTS.md, 02_CURRENT_BASELINE_AUDIT.md, 03_TARGET_PRODUCT_AND_ARCHITECTURE.md, 04_FOREGROUND_AGENT_RUNBOOK.md, 05_FIXED_SUBAGENT_BINDINGS.yaml
- zip 内 marker: 00_READ_FIRST.md, START_PROMPT.txt
- zip 内规格文件: 00_READ_FIRST.md, 01_OWNER_FROZEN_CONSTRAINTS.md, 02_CURRENT_BASELINE_AUDIT.md, 03_TARGET_PRODUCT_AND_ARCHITECTURE.md, 04_FOREGROUND_AGENT_RUNBOOK.md, 05_FIXED_SUBAGENT_BINDINGS.yaml, 06_TASK_GRAPH_AND_WAVES.md, 07_CHECKPOINTS_AND_GATES.md, 08_GIT_MAIN_ONLY_INTEGRATION.md, 09_WINDOWS_TOOLCHAIN_LOCK.md, 10_LINUX_CONTROL_NODE.md, 11_MODULE_SOURCE_TEST_STANDARD.md, 12_DLL_ABI_AND_LOADER_STANDARD.md, 13_DATA_PIPELINE_AND_ARTIFACT_STANDARD.md, 14_RUNTIME_SCHEDULER_AND_TRACE_STANDARD.md, 15_CPU_PROVIDER_AND_RESOURCE_STANDARD.md, 16_SCIENCE_DOCUMENT_AND_TRACEABILITY_STANDARD.md, 17_INDEPENDENT_REVIEWER.md, 18_AUDIT_PACKAGE_SPEC.md, 19_MACHINE_CHECKS.md, 20_RELEASE_GATES.md, 21_PRIMARY_REFERENCES.md, 22_FAILURE_ESCALATION.md, 23_GRAPH_AND_DOC_TOOL_POLICY.md, 00_FOREGROUND_AGENT_CHECKLIST.md, 01_SUBAGENT_RETURN_CHECKLIST.md, 02_MODULE_ACCEPTANCE_CHECKLIST.md, 03_WINDOWS_RELEASE_CHECKLIST.md, 04_AUDIT_PACKAGE_CHECKLIST.md, 05_INDEPENDENT_REVIEW_CHECKLIST.md, 00_TASK_EXECUTION_CONTRACT.md, 01_W0_GOVERNANCE_TASKS.md, 02_ABI_BUILD_CLI_TASKS.md, 03_RUNTIME_DATA_IO_TASKS.md, 04_CPU_RESOURCE_TASKS.md, 05_PHASE1_TASKS.md, 06_PHASE2_TASKS.md, 07_PHASE3_TASKS.md, 08_QA_LINUX_TASKS.md, 09_WINDOWS_RELEASE_TASKS.md
- zip 内台账: TASK_LEDGER.csv 表头 ['task_id', 'wave', 'owner_id', 'kind', 'depends_on', 'mutex_lock', 'resource_class', 'commit_subject', 'spec_ref', 'acceptance', 'alias_of'] 行 191
- 00_READ_FIRST 开头:

# AstroCS Alpha 模块化重构控制包：先读我

控制包 ID：`ASTROCS-ALPHA3-MODULAR-REFOUNDATION-V7`  
发布日期：`2026-09-02`  
冻结起始提交：`c1696156583c68c9a3a65287639c726937e9f6e7`  
冻结起始版本：`0.10.0-alpha.2`  
目标版本：`0.11.0-alpha.1`  
正式分支：`main`  
正式平台：Windows x64；Linux x64 为控制/轻验证平台  
状态：可执行控制包；任何历史 V4/V5/V6/V18/V19 控制文件均不得覆盖本包。

## 1. 本轮唯一目标

把当前能够工作的但架构、文档和执行真实性仍不可靠的 AstroCS，收敛为可预发布的纯 CPU 模块化产品，同时保持既有科学定义不因架构迁移而漂移。

必须同时实现：

1. Phase1、Phase2、Phase3 分别运行、分别配置、分别验收；不得在同一进程自动串联。
2. Windows 发布物只有一个用户入口 `astrocs.exe`，科学模块、Runtime、I/O、CPU provider 以 DLL 交付。
3. 每个 DAG 节点绑定一个真实模块操作，不得以多个节点重复包装同一个 Session。
4. 一个共享执行器和一个真实 ThreadBudget 管理所有重计算；重计算不得单线程或长期低利用率。
5. ACR 保留但不进入当前生产构建、加载、benchmark 或发布包。
6. baseline/AVX2/FMA/AVX-512 仅在热点 kernel 上提供，按硬件安全检查、正确性自测和逐 kernel benchmark 选择；无 profile 一律 baseline。
7. SCI → ALG → DATA/API/ARCH → 源码符号/module manifest → TEST → 当前提交证据全链闭合。
8. 每个模块有独立源码、README、manifest 和可复用 unit/property/oracle/negative/performance 测试。
9. Phase3 独立完成 TAN、SIN、ZEA、CAR、AIT，以及 `nearest`、`healpix_interp4`，使用流式 FITS 输出。
10. 最终在 Windows 进行一次候选提交的真实数据/32R/接缝/资源验证，不反复跑历史全版本。

## 2. 执行者第一小时必须完成

前台 Agent `FG-000` 依次执行，任何一步失败均不得把控制包称为有效：

1. 在仓库外建立本轮工作根：`ASTROCS_WORK_ROOT/{control,worktrees,returns,logs,reports,buil


## 【zip】工程控制/_control_packs/AstroCS_ALPHA3_MODULAR_REFOUNDATION_CONTROL_V7_1_20260902_FINAL3.zip
- 文件 99 | 388.7 KB
- sha256: 005448fb3b6892d3063d23149692fbd9226a892a2c1891e4cb8d14a80e8491c3
- git 事件: {'first': None, 'last': None, 'dels': None, 'adds_n': 0, 'mod_n': None, 'touch_last': None}
- zip 顶层: 00_READ_FIRST.md, 01_OWNER_FROZEN_CONSTRAINTS.md, 02_CURRENT_BASELINE_AUDIT.md, 03_TARGET_PRODUCT_AND_ARCHITECTURE.md, 04_FOREGROUND_AGENT_RUNBOOK.md, 05_FIXED_SUBAGENT_BINDINGS.yaml
- zip 内 marker: 00_READ_FIRST.md, START_PROMPT.txt
- zip 内规格文件: 00_READ_FIRST.md, 01_OWNER_FROZEN_CONSTRAINTS.md, 02_CURRENT_BASELINE_AUDIT.md, 03_TARGET_PRODUCT_AND_ARCHITECTURE.md, 04_FOREGROUND_AGENT_RUNBOOK.md, 05_FIXED_SUBAGENT_BINDINGS.yaml, 06_TASK_GRAPH_AND_WAVES.md, 07_CHECKPOINTS_AND_GATES.md, 08_GIT_MAIN_ONLY_INTEGRATION.md, 09_WINDOWS_TOOLCHAIN_LOCK.md, 10_LINUX_CONTROL_NODE.md, 11_MODULE_SOURCE_TEST_STANDARD.md, 12_DLL_ABI_AND_LOADER_STANDARD.md, 13_DATA_PIPELINE_AND_ARTIFACT_STANDARD.md, 14_RUNTIME_SCHEDULER_AND_TRACE_STANDARD.md, 15_CPU_PROVIDER_AND_RESOURCE_STANDARD.md, 16_SCIENCE_DOCUMENT_AND_TRACEABILITY_STANDARD.md, 17_INDEPENDENT_REVIEWER.md, 18_AUDIT_PACKAGE_SPEC.md, 19_MACHINE_CHECKS.md, 20_RELEASE_GATES.md, 21_PRIMARY_REFERENCES.md, 22_FAILURE_ESCALATION.md, 23_GRAPH_AND_DOC_TOOL_POLICY.md, 00_FOREGROUND_AGENT_CHECKLIST.md, 01_SUBAGENT_RETURN_CHECKLIST.md, 02_MODULE_ACCEPTANCE_CHECKLIST.md, 03_WINDOWS_RELEASE_CHECKLIST.md, 04_AUDIT_PACKAGE_CHECKLIST.md, 05_INDEPENDENT_REVIEW_CHECKLIST.md, 00_TASK_EXECUTION_CONTRACT.md, 01_W0_GOVERNANCE_TASKS.md, 02_ABI_BUILD_CLI_TASKS.md, 03_RUNTIME_DATA_IO_TASKS.md, 04_CPU_RESOURCE_TASKS.md, 05_PHASE1_TASKS.md, 06_PHASE2_TASKS.md, 07_PHASE3_TASKS.md, 08_QA_LINUX_TASKS.md, 09_WINDOWS_RELEASE_TASKS.md
- zip 内台账: TASK_LEDGER.csv 表头 ['task_id', 'wave', 'owner_id', 'kind', 'depends_on', 'mutex_lock', 'resource_class', 'commit_subject', 'spec_ref', 'acceptance', 'alias_of'] 行 191
- 00_READ_FIRST 开头:

# AstroCS Alpha 模块化重构控制包：先读我

控制包 ID：`ASTROCS-ALPHA3-MODULAR-REFOUNDATION-V7`  
发布日期：`2026-09-02`  
冻结起始提交：`c1696156583c68c9a3a65287639c726937e9f6e7`  
冻结起始版本：`0.10.0-alpha.2`  
目标版本：`0.11.0-alpha.1`  
正式分支：`main`  
正式平台：Windows x64；Linux x64 为控制/轻验证平台  
状态：可执行控制包；任何历史 V4/V5/V6/V18/V19 控制文件均不得覆盖本包。

## 1. 本轮唯一目标

把当前能够工作的但架构、文档和执行真实性仍不可靠的 AstroCS，收敛为可预发布的纯 CPU 模块化产品，同时保持既有科学定义不因架构迁移而漂移。

必须同时实现：

1. Phase1、Phase2、Phase3 分别运行、分别配置、分别验收；不得在同一进程自动串联。
2. Windows 发布物只有一个用户入口 `astrocs.exe`，科学模块、Runtime、I/O、CPU provider 以 DLL 交付。
3. 每个 DAG 节点绑定一个真实模块操作，不得以多个节点重复包装同一个 Session。
4. 一个共享执行器和一个真实 ThreadBudget 管理所有重计算；重计算不得单线程或长期低利用率。
5. ACR 保留但不进入当前生产构建、加载、benchmark 或发布包。
6. baseline/AVX2/FMA/AVX-512 仅在热点 kernel 上提供，按硬件安全检查、正确性自测和逐 kernel benchmark 选择；无 profile 一律 baseline。
7. SCI → ALG → DATA/API/ARCH → 源码符号/module manifest → TEST → 当前提交证据全链闭合。
8. 每个模块有独立源码、README、manifest 和可复用 unit/property/oracle/negative/performance 测试。
9. Phase3 独立完成 TAN、SIN、ZEA、CAR、AIT，以及 `nearest`、`healpix_interp4`，使用流式 FITS 输出。
10. 最终在 Windows 进行一次候选提交的真实数据/32R/接缝/资源验证，不反复跑历史全版本。

## 2. 执行者第一小时必须完成

前台 Agent `FG-000` 依次执行，任何一步失败均不得把控制包称为有效：

1. 在仓库外建立本轮工作根：`ASTROCS_WORK_ROOT/{control,worktrees,returns,logs,reports,buil


## 【zip_recovered_from_history】history_zip/AstroCS_ALPHA3_MODULAR_REFOUNDATION_CONTROL_V7_1_20260902_FINAL3.zip
- 文件 99 | 388.7 KB
- sha256: 005448fb3b6892d3063d23149692fbd9226a892a2c1891e4cb8d14a80e8491c3
- git 事件: {'first': None, 'last': None, 'dels': None, 'adds_n': 0, 'mod_n': None, 'touch_last': '8e03d7da9feacd9a85f6ecd02aab51d4a61b5a0e'}
- zip 顶层: 00_READ_FIRST.md, 01_OWNER_FROZEN_CONSTRAINTS.md, 02_CURRENT_BASELINE_AUDIT.md, 03_TARGET_PRODUCT_AND_ARCHITECTURE.md, 04_FOREGROUND_AGENT_RUNBOOK.md, 05_FIXED_SUBAGENT_BINDINGS.yaml
- zip 内 marker: 00_READ_FIRST.md, START_PROMPT.txt
- zip 内规格文件: 00_READ_FIRST.md, 01_OWNER_FROZEN_CONSTRAINTS.md, 02_CURRENT_BASELINE_AUDIT.md, 03_TARGET_PRODUCT_AND_ARCHITECTURE.md, 04_FOREGROUND_AGENT_RUNBOOK.md, 05_FIXED_SUBAGENT_BINDINGS.yaml, 06_TASK_GRAPH_AND_WAVES.md, 07_CHECKPOINTS_AND_GATES.md, 08_GIT_MAIN_ONLY_INTEGRATION.md, 09_WINDOWS_TOOLCHAIN_LOCK.md, 10_LINUX_CONTROL_NODE.md, 11_MODULE_SOURCE_TEST_STANDARD.md, 12_DLL_ABI_AND_LOADER_STANDARD.md, 13_DATA_PIPELINE_AND_ARTIFACT_STANDARD.md, 14_RUNTIME_SCHEDULER_AND_TRACE_STANDARD.md, 15_CPU_PROVIDER_AND_RESOURCE_STANDARD.md, 16_SCIENCE_DOCUMENT_AND_TRACEABILITY_STANDARD.md, 17_INDEPENDENT_REVIEWER.md, 18_AUDIT_PACKAGE_SPEC.md, 19_MACHINE_CHECKS.md, 20_RELEASE_GATES.md, 21_PRIMARY_REFERENCES.md, 22_FAILURE_ESCALATION.md, 23_GRAPH_AND_DOC_TOOL_POLICY.md, 00_FOREGROUND_AGENT_CHECKLIST.md, 01_SUBAGENT_RETURN_CHECKLIST.md, 02_MODULE_ACCEPTANCE_CHECKLIST.md, 03_WINDOWS_RELEASE_CHECKLIST.md, 04_AUDIT_PACKAGE_CHECKLIST.md, 05_INDEPENDENT_REVIEW_CHECKLIST.md, 00_TASK_EXECUTION_CONTRACT.md, 01_W0_GOVERNANCE_TASKS.md, 02_ABI_BUILD_CLI_TASKS.md, 03_RUNTIME_DATA_IO_TASKS.md, 04_CPU_RESOURCE_TASKS.md, 05_PHASE1_TASKS.md, 06_PHASE2_TASKS.md, 07_PHASE3_TASKS.md, 08_QA_LINUX_TASKS.md, 09_WINDOWS_RELEASE_TASKS.md
- zip 内台账: TASK_LEDGER.csv 表头 ['task_id', 'wave', 'owner_id', 'kind', 'depends_on', 'mutex_lock', 'resource_class', 'commit_subject', 'spec_ref', 'acceptance', 'alias_of'] 行 191
- 00_READ_FIRST 开头:

# AstroCS Alpha 模块化重构控制包：先读我

控制包 ID：`ASTROCS-ALPHA3-MODULAR-REFOUNDATION-V7`  
发布日期：`2026-09-02`  
冻结起始提交：`c1696156583c68c9a3a65287639c726937e9f6e7`  
冻结起始版本：`0.10.0-alpha.2`  
目标版本：`0.11.0-alpha.1`  
正式分支：`main`  
正式平台：Windows x64；Linux x64 为控制/轻验证平台  
状态：可执行控制包；任何历史 V4/V5/V6/V18/V19 控制文件均不得覆盖本包。

## 1. 本轮唯一目标

把当前能够工作的但架构、文档和执行真实性仍不可靠的 AstroCS，收敛为可预发布的纯 CPU 模块化产品，同时保持既有科学定义不因架构迁移而漂移。

必须同时实现：

1. Phase1、Phase2、Phase3 分别运行、分别配置、分别验收；不得在同一进程自动串联。
2. Windows 发布物只有一个用户入口 `astrocs.exe`，科学模块、Runtime、I/O、CPU provider 以 DLL 交付。
3. 每个 DAG 节点绑定一个真实模块操作，不得以多个节点重复包装同一个 Session。
4. 一个共享执行器和一个真实 ThreadBudget 管理所有重计算；重计算不得单线程或长期低利用率。
5. ACR 保留但不进入当前生产构建、加载、benchmark 或发布包。
6. baseline/AVX2/FMA/AVX-512 仅在热点 kernel 上提供，按硬件安全检查、正确性自测和逐 kernel benchmark 选择；无 profile 一律 baseline。
7. SCI → ALG → DATA/API/ARCH → 源码符号/module manifest → TEST → 当前提交证据全链闭合。
8. 每个模块有独立源码、README、manifest 和可复用 unit/property/oracle/negative/performance 测试。
9. Phase3 独立完成 TAN、SIN、ZEA、CAR、AIT，以及 `nearest`、`healpix_interp4`，使用流式 FITS 输出。
10. 最终在 Windows 进行一次候选提交的真实数据/32R/接缝/资源验证，不反复跑历史全版本。

## 2. 执行者第一小时必须完成

前台 Agent `FG-000` 依次执行，任何一步失败均不得把控制包称为有效：

1. 在仓库外建立本轮工作根：`ASTROCS_WORK_ROOT/{control,worktrees,returns,logs,reports,buil

