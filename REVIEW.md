# REVIEW.md — AstroCS 项目负责人审查入口（L0）

> 目标产品：**0.11.0-alpha.1**（根 VERSION，GOV-003 唯一源；
> 生成串 `0.11.0-alpha.1+g<commit12>`，见 `docs/governance/VERSION_NAMESPACES.md`）
> 基线提交：`caee3e67e5a209a9e47b514f42b2b63f3dc4da4e`
> 建立：GOV-004（SA-GOV-01）。项目负责人只需阅读本文件及 `docs/owner/` 5 份顶层文档，
> 底层文档/源码/日志通过证据 ID 追溯，不要求逐文件阅读（约束 §F.7）。

## 1. 一句话结论

```text
Alpha 架构收敛进行中：合同面（工程约束/文档边界/版本单源/ABI v1/数据产物/
Runtime 图/Windows 工具链 preset/DLL schema/FITS 流接口）已冻结入 main；
Windows 发布执行面、真实数据/32R 验收、Phase3 扩展（SIN/ZEA/CAR/AIT、
healpix_interp4、流式 FITS 接入）未完成。
当前状态：NOT_READY_FOR_RELEASE（未到 READY_FOR_OWNER_REVIEW）。
```

## 2. 顶层文档（点击审阅，负责人 L0 直达全部权威）

| 文档 | 内容 |
|---|---|
| [SCIENCE_OVERVIEW](docs/owner/SCIENCE_OVERVIEW.md) | 科学权威源汇总、Phase1/2/3 逐项 PASS/NOT_VERIFIED、诚实缺口 |
| [PIPELINE_OVERVIEW](docs/owner/PIPELINE_OVERVIEW.md) | 三 Phase 隔离模型、各 Phase 内部链、跨 Phase 磁盘交换 |
| [ARCHITECTURE_OVERVIEW](docs/owner/ARCHITECTURE_OVERVIEW.md) | Windows 优先、ACR dormant、唯一 Runtime、DLL 边界、依赖方向 |
| [RELEASE_STATUS](docs/owner/RELEASE_STATUS.md) | 冻结 PASS 清单、未完成 NOT_VERIFIED 清单、发布 Gate 口径 |
| [CHANGE_REVIEW](docs/owner/CHANGE_REVIEW.md) | 本轮集成变化、GOV-004 文档骨架、验证、已知限制 |

> 状态词约定：PASS=当前提交内可核（合同冻结/源码在位）；FAIL=已执行但不符合；
> NOT_VERIFIED=未在当前提交核实（含未复跑执行验收、不在基线内的功能）。
> 历史旧轮次顶层文档（docs/archive/review/*，GOV-002 归档）为非规范，不作为当前权威。

## 3. 当前进度（Gate 与波次口径）

- 本控制包（ASTROCS-ALPHA3-MODULAR-REFOUNDATION-V7）W0/W1 阶段；
  GOV-001..004 文档/治理任务已集成；合同面任务（ABI/DATA/RT/LOG/ARC）已集成；
  宿主构建/IO（BLD-001/002、IO-001）已集成。
- 尚未：模块 DLL 迁移（W3）、旧路由删除（W4，含 run --phases 遗留）、
  Linux 验证（W5）、Windows 正式验证（W6）、文档收敛（W7）、独立终审（W8）。
- 任务状态唯一源在控制包运行账本（工作根 TASK_LEDGER.csv）；Git 主历史只含
  前台集成提交（`git log --oneline main`）。

## 4. 组件状态（统一口径，逐项 PASS/FAIL/NOT_VERIFIED）

| 组件/面 | 状态 | 说明与证据 |
|---|---|---|
| 工程约束/文档边界/版本单源 | PASS | 根约束 + `docs/DOCUMENT_INDEX.yaml` + 根 `VERSION`（GOV-001/002/003，doccheck 系列可核） |
| C ABI v1 / DLL 边界合同 | PASS | `include/astrocs/abi/*.h`（ABI-001）、`contracts/config/module_dll_contract.schema.json`（ARC-001） |
| 数据产物/三阶段产品交换合同 | PASS | DATA-001/002（schema/registry/validator/示例） |
| Runtime 类型化运行图 | PASS | RT-001（typed_dag/registry/负测） |
| Windows 工具链 preset / 唯一根 CMake | PASS（合同/配置面） | BLD-001/002；完整 MSVC 构建未在当前提交验证 |
| FITS 流式接口 | PASS（冻结） | IO-001；Phase3 writer 接入 NOT_VERIFIED |
| Phase1/2/3 科学实现源码在位 | PASS（静态可核） | lib/phaseN_session + 模块库；合成/门禁执行未在当前提交复跑 → 见下 |
| Phase1/2/3 合成/门禁执行验收（当前提交复跑） | NOT_VERIFIED | 未复跑；历史存档不冒充当前证据 |
| Phase3 TAN/WCS/nearest/bilinear/FITS 原子写 | PASS（源码在位） | lib/phase3_session |
| Phase3 SIN/ZEA/CAR/AIT、`healpix_interp4`、流式 FITS 接入 | NOT_VERIFIED（不在基线） | 当前仅 TAN + nearest/bilinear |
| 真实数据（BASS/32R/接缝）/ Windows 正式验证 | NOT_VERIFIED | FINAL_REAL_DATA_VALIDATION=PENDING；待 Fatduck |
| Windows DLL 化发布安装树（astrocs.exe + DLL） | NOT_VERIFIED | 目标在控制包 03 §4；未交付/验证 |
| ACR | DORMANT | 保留源码与隔离测试；生产构建/加载/路由/benchmark/发布不含 ACR/CUDA |
| 遗留 `astrocs run --phases 1,2,3` 进程内连跑 | FAIL（未删） | 与约束 §A.4 冲突；W4 删除范围，SA-CLI/前台处理 |
| 每 DAG 节点唯一真实模块 operation（约束 §F.1） | 进行中/未达成 | IR 子模块（P2 七节点/P3 五节点等）factory 当前委托同一 phaseN session；W3/W4 迁移 |
| 旧 `aio_pipeline_engine` 越权编排 | 保留中 | ARCH-001.md §7 登记，LEG-003 迁移；不宣称已删除 |
| L0 负责人入口（本文件 + docs/owner/） | PASS | GOV-004；5 链接直达全部 L0 权威 |

## 5. 关键结论（每条可回溯）

- 产品版本单源 `0.11.0-alpha.1`（根 VERSION，GOV-003）；CLI `--version` 由生成链
  输出 `0.11.0-alpha.1+g<commit>`（生成链见 `docs/governance/VERSION_NAMESPACES.md`）。
- 科学定义=算法=接口=代码=测试全链闭合是目标（约束 §F.6）；已集成的各任务
  `scientific_change=NO`，未改公式/容差。
- 三 Phase 隔离是产品模型：独立命令 phase1/2/3 run；跨 Phase 仅磁盘交换（DATA-002）；
  **`run --phases 1,2,3` 遗留未删（FAIL，W4）**；且**每节点唯一真实模块 operation
  尚未达成**（IR 子模块委托同一 phaseN session，§F.1 中间态）——负责人不应视为
  已满足约束 §A.4 / §F.1。
- Windows x64 是正式平台：合同面已冻结；DLL 发布树与 MSVC/32R 验收未完成
  （NOT_VERIFIED），不宣称"Windows 已交付"。
- ACR dormant：生产构建默认排除（CMake preset ACR=OFF、根 CMake 不链 lib/acr），
  不加载不发布。
- Phase3 当前只实现 TAN + nearest/bilinear + CFITSIO 原子写；控制包任务清单中的
  SIN/ZEA/CAR/AIT、`healpix_interp4`、流式 FITS 输出属后续任务，**未实现不冒充**。
- 机器验证入口：`tools/doccheck/check_doc_index.py`（GOV-002）、
  `check_engineering_constraints.py`（GOV-001）、`check_version_namespaces.py`
  （GOV-003）；GOV-004 通过三者（logs 见返回包）。

## 6. 待负责人/前台决策事项（节选）

1. `run --phases 1,2,3` 遗留的删除排期（W4）。
2. Phase3 扩展（SIN/ZEA/CAR/AIT、interp4、流式 FITS）任务排期。
3. Windows DLL 化与 32R/真实数据验收资源（Fatduck）。
4. GOV-005 对 README/HANDOVER/memory/CHANGELOG 陈旧现状的收敛。

## 7. 发布口径

本 Agent 无权宣布发布（约束 §H）；当前不满足 READY_FOR_OWNER_REVIEW 门槛，
如实标注 **NOT_READY_FOR_RELEASE**。

---
authoring_task: GOV-004
authoring_owner: SA-GOV-01
base_main_sha: caee3e67e5a209a9e47b514f42b2b63f3dc4da4e
