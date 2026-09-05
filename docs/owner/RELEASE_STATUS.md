# 发布状态（Release Status）

> 文档 ID：DOC-GOV-OWNER-RELEASE-001
> 状态：ACTIVE_NORMATIVE（GOV-004 建立，SA-GOV-01）
> 目标产品：`0.11.0-alpha.2`（根 VERSION，GOV-003 唯一源；生成串
> `0.11.0-alpha.2+g<commit12>`，见 `docs/governance/VERSION_NAMESPACES.md`）
> 基线提交：`caee3e67e5a209a9e47b514f42b2b63f3dc4da4e`
> 状态词约定同 SCIENCE_OVERVIEW；最终发布裁定只属项目负责人（约束 §H），
> 本 Agent 至多声明 READY_FOR_OWNER_REVIEW，不替代批准。

## 1. 一句话结论

```text
Alpha 架构收敛进行中：合同面（工程约束/版本/文档边界/ABI v1/数据产物/Runtime 图/
工具链 preset/DLL schema/FITS 流接口）已冻结入 main；Windows 发布执行面与
真实数据/32R 验收未完成，SIN/ZEA/CAR/AIT、healpix_interp4、Phase3 流式 FITS
接入未实现 → 当前状态 NOT_READY_FOR_RELEASE，而非 READY_FOR_OWNER_REVIEW。
```

## 2. 版本与发布面

- 产品版本唯一源：根 `VERSION` = `0.11.0-alpha.2`（GOV-003）。
  版本命名空间：product / module / ABI(v1) / data-schema(schema_version=1) /
  doc-revision / history（机器检查器 `tools/doccheck/check_version_namespaces.py`）。
- Windows 正式发布候选：**未产生**。DLL 化安装树（astrocs.exe + astrocs_runtime.dll +
  astrocs_io.dll + 模块 DLL + provider DLL）未在当前提交交付/验证（目标定义见控制包 03 §4）。
- Linux `.so` 技术预览：未在当前提交验证。
- 已知他人路径遗留（不属 GOV-004 范围）：`docs/VERSIONING.md`、CMake
  `project(... VERSION)` 字面量、若干 tests/tools 硬编码旧版本号 ——
  由版本检查器 `known_legacy_reported` 输出登记，待 GOV-005/前台协调（详见
  VERSION_NAMESPACES.md known_limits 与检查器 out_of_scope 列表）。

## 3. 冻结面状态（PASS 清单，当前提交静态可核）

| 面 | 状态 | 主要依据 |
|---|---|---|
| 工程约束入 main | PASS | `AstroCS_ENGINEERING_CONSTRAINTS.md`（GOV-001，机器修订关系可核） |
| 文档边界/索引 | PASS | `docs/DOCUMENT_INDEX.yaml`（GOV-002；本任务 GOV-004 补登 docs/owner 与三份 interface/governance 文档后全 PASS） |
| 版本单源 | PASS | `VERSION` + `docs/governance/VERSION_NAMESPACES.md`（GOV-003） |
| 类型化产物合同 | PASS | DATA-001（manifest schema/registry/validator） |
| 三阶段产品交换合同 | PASS | DATA-002（矩阵/schema/validator/4 示例） |
| C ABI v1 | PASS | ABI-001 4 纯 C 头（C11/C++17 编译断言在集成验收） |
| Runtime 类型化运行图 | PASS | RT-001（typed_dag + registry + 负测） |
| 结构化日志合同 | PASS | LOG-001（schema/JSONL 契约） |
| DLL 边界 schema | PASS | ARC-001 |
| Windows 工具链 preset | PASS | BLD-001（合同面） |
| 唯一根 CMake 构建图 | PASS | BLD-002（配置面；完整编译属构建任务） |
| FITS 流式接口冻结 | PASS | IO-001（接口+实现+契约测试；本任务未复跑 pytest） |
| L0 负责人入口 | PASS | `REVIEW.md` + `docs/owner/*`（本任务 GOV-004） |

> 上述"PASS"多数是**合同/源码在位**级（当前提交静态核实）；各集成提交由前台在 main
> 上跑过各自验收（pytest 等），本轮文档任务不重复执行，也不把这些历史执行冒充为本
> 提交上的复跑证据。文档任务的可执行证据 = 三个 doccheck 检查器 + 文档一致性核对。

## 4. 未完成 / 未验证面（NOT_VERIFIED / 如实记录）

| 面 | 状态 | 说明 |
|---|---|---|
| Windows DLL 化发布安装树 | NOT_VERIFIED | 未产出/验证 |
| Windows MSVC 编译 + 测试（Win10 22H2 下限 / Win11 主验证） | NOT_VERIFIED | Fatduck 侧执行，未完成 |
| 真实数据（BASS/32R/接缝）最终验收 | NOT_VERIFIED | `docs/RELEASE_STATUS.md`：FINAL_REAL_DATA_VALIDATION=PENDING；KNOWN_LIMITATIONS 同 |
| 32R 单线程重计算禁令的执行证据 | NOT_VERIFIED | 资源门禁执行面属 Windows/W5 域 |
| Phase3 SIN/ZEA/CAR/AIT 投影 | NOT_VERIFIED（不在基线） | 当前仅 TAN |
| Phase3 `healpix_interp4` | NOT_VERIFIED（不在基线） | 当前 nearest/bilinear |
| Phase3 流式 FITS 输出接入 | NOT_VERIFIED | IO-001 接口在位但未接入 p3 writer |
| 遗留 `astrocs run --phases 1,2,3` 连跑 | FAIL（未删，冲突约束 §A.4） | W4 遗留；REVIEW/PIPELINE 一致记录 |
| 每 DAG 节点唯一真实模块 operation（§F.1） | 进行中/未达成 | module_adapters 委托同一 phaseN session；W3/W4 |
| ACR | DORMANT | 保留源码隔离测试；生产构建/加载/路由/benchmark/发布不含 ACR/CUDA |

## 5. 发布 Gate 口径（参考控制包 20_RELEASE_GATES.md / 07_CHECKPOINTS_AND_GATES.md）

- 当前处于 W1 波次（合同冻结）内 GOV 系列文档收敛；GOV-004 属 L0 骨架任务。
- 达到 `READY_FOR_OWNER_REVIEW` 仍需：Windows 正式验证（G6）、文档质量收敛（G7）、
  独立终审（G8）等 Gate 依序通过；**当前不满足**，如实标注 NOT_READY。

## 6. 状态汇总

```text
冻结/合同面:      PASS（见 §3）
发布执行面:        NOT_VERIFIED / 未完成
科学扩展面:        NOT_VERIFIED（SIN/ZEA/CAR/AIT、interp4、流式 FITS）
架构迁移面:        进行中（§F.1 每节点唯一 operation 未达成；run --phases 遗留 FAIL）
违反项:            FAIL（run --phases 1,2,3 遗留）
发布结论:          NOT_READY_FOR_RELEASE（未到 READY_FOR_OWNER_REVIEW）
```

---
authoring_task: GOV-004
authoring_owner: SA-GOV-01
base_main_sha: caee3e67e5a209a9e47b514f42b2b63f3dc4da4e
