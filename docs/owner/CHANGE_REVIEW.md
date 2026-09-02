# 变更审查（Change Review）

> 文档 ID：DOC-GOV-OWNER-CHANGE-001
> 状态：ACTIVE_NORMATIVE（GOV-004 建立，SA-GOV-01）
> 基线提交：`caee3e67e5a209a9e47b514f42b2b63f3dc4da4e`（GOV-004 工作树检出的基）
> 目标产品：`0.11.0-alpha.1`（根 VERSION）
> 状态词约定同 SCIENCE_OVERVIEW；本文件汇总"本轮已集成到 main 的变化"与
> GOV-004 本次文档骨架变化，供负责人逐项审查。

## 1. 本轮集成到 main 的变更（基线之前的提交链，依 00_READ_FIRST 与 git 历史）

| commit | 内容 | scientific_change | 域 |
|---|---|---|---|
| `065c81d` | GOV-001 冻结工程约束（根约束文件 + AGENTS.md 精简 + 机器索引） | NO | governance |
| `b7b2dea` | GOV-002 归档非当前工程文档（DOCUMENT_INDEX.yaml；854+27 文件迁 archive） | NO | governance |
| `39e7731` | GOV-003 统一版本事实源（VERSION=0.11.0-alpha.1；5 版本命名空间；生成链） | NO | build/version |
| `bbe2e59` | LOG-001 统一结构化日志接口（schema/JSONL/脱敏） | NO | logging |
| `1c3d1dc` | ARC-001 冻结 DLL 产品边界（module_dll_contract schema） | NO | architecture |
| `76b85d4` | ABI-001 定义模块 C ABI v1（4 纯 C 头） | NO | abi |
| `2df77af` | DATA-001 冻结类型化产物合同（manifest schema/registry/validator） | NO | data |
| `5d38ebd` | RT-001 实现类型化运行图（typed DAG + registry + 负测） | NO | runtime |
| `c1ee791` | DATA-002 定义三阶段产品交换合同（矩阵/示例/validator） | NO | data |
| `f260b80` | BLD-001 冻结 Windows 发布工具链（preset + verifier） | NO | build |
| `3e7f758` | BLD-002 建立唯一根 CMake 构建图（唯一 project/add_executable，无 GLOB） | NO | build |
| `caee3e6` | IO-001 实现流式 FITS 读写 API 并修复 F32 字节序 | NO | io |

- 全部为架构/合同/文档/构建域，`scientific_change=NO`（各提交 message 标注），
  未触碰科学公式与容差（约束 §E.1）。
- 各提交的验收证据在各自返回包（returns/）与前台集成日志；本文不复制，不冒充当前复跑。

## 2. 本次 GOV-004 文档骨架变化（本 patch）

| 文件 | 动作 | 说明 |
|---|---|---|
| `REVIEW.md` | 重写 | L0 负责人入口：一句话结论、5 份 docs/owner 链接、进度、组件状态表、关键结论、发布口径 |
| `docs/owner/SCIENCE_OVERVIEW.md` | 新增 | 科学权威汇总；Phase1/2/3 逐项状态；SIN/ZEA/CAR/AIT、interp4、流式 FITS 接入 NOT_VERIFIED |
| `docs/owner/PIPELINE_OVERVIEW.md` | 新增 | 三 Phase 隔离模型与内部链；run --phases 遗留 FAIL 如实记录 |
| `docs/owner/ARCHITECTURE_OVERVIEW.md` | 新增 | Windows 优先、ACR dormant、唯一 Runtime、依赖方向、契约索引 |
| `docs/owner/RELEASE_STATUS.md` | 新增 | 版本/发布面、冻结 PASS 清单、未完成 NOT_VERIFIED 清单、发布结论 |
| `docs/owner/CHANGE_REVIEW.md` | 新增 | 本轮变化汇总、影响、验证、已知限制（本文） |
| `docs/DOCUMENT_INDEX.yaml` | 更新 | 登记 docs/owner/*.md + 补登 docs/governance/VERSION_NAMESPACES.md、docs/interfaces/data/*、docs/interfaces/io/*（修复 check_doc_index docs_fully_covered FAIL） |

> 旧 REVIEW.md 指向的 `docs/review/*.md`（旧轮次顶层文档）在 GOV-002 归档为
> `docs/archive/review/*`（ARCHIVED_NON_NORMATIVE），故旧链接已失效；GOV-004
> 按任务规格在 `docs/owner/` 重建负责人文档并重写 REVIEW 入口（新命名空间与
> 控制包 03 目录规范 `docs/owner/` 一致）。

## 3. 科学影响

- 无。本 patch 只建立负责人审查入口（REVIEW/docs/owner + 索引登记），
  `scientific_change=false`；不改公式/容差/接口/源码。

## 4. 验证（本 patch 的可执行证据）

| 检查 | 命令 | 预期 | 状态 |
|---|---|---|---|
| 文档索引覆盖与归档边界 | `python3 tools/doccheck/check_doc_index.py --root .` | DOC_INDEX_PASS / exit 0 | 基线时 FAIL（3 项未覆盖）→ 本 patch 后 PASS（将留日志） |
| 工程约束机器修订关系 | `python3 tools/doccheck/check_engineering_constraints.py --root . [--base-sha caee3e6...]` | CONSTRAINTS_PASS / exit 0 | PASS（不修改该文件） |
| 版本命名空间扫描 | `python3 tools/doccheck/check_version_namespaces.py --root .` | VERSION_NAMESPACES_PASS / exit 0 | PASS（docs/owner 纳入扫描；文档内无版本漂移） |
| L0 可达性 | 人工核对：REVIEW.md 5 链接 → docs/owner/* 全部存在 | 可到达 | 本 patch 保证 |

## 5. 已知限制与诚实缺口（验收项之一：无未验证"已实现"）

1. **执行验收未在当前提交复跑**：Phase1/2/3 合成/门禁、资源监控、IO 契约 pytest、
   Windows MSVC —— 均属他人域或需要构建/Windows 资源，本文档任务不执行，
   统一标 NOT_VERIFIED（不冒充）。
2. **`astrocs run --phases 1,2,3` 遗留未删**：与约束 §A.4 冲突，属 W4「删除伪/旧路由」，
   由 SA-CLI-04/前台域处理；本文档如实 FAIL 记录（cli/** 不在 GOV-004 写域）。
3. **每 DAG 节点唯一真实模块 operation 未达成（约束 §F.1）**：IR 子模块
   （Phase2 七节点/Phase3 五节点等）的 factory 在 `lib/core/src/module_adapters.cpp`
   中全部委托同一 phaseN session（P1Api/P2Api/P3Api `*_session_run`），
   属"多节点重复包装同一 Session"中间态；真实模块化 + entry 绑定属 W3/RT-002+，
   cli/、lib/core/ 非 GOV-004 写域，本文只如实记录（REVIEW/PIPELINE 同口径）。
4. **SIN/ZEA/CAR/AIT、healpix_interp4、Phase3 流式 FITS 接入**：当前基线未实现，
   文档 NOT_VERIFIED（SA-P3-* 后续任务域）。
5. **DLL 化发布安装树/Windows 验收**：未完成（W2 后宿主/DLL 迁移 + G6 Windows 域）。
6. **io→core 依赖方向与 ARCH-001 §3 差异**：架构域待审（本文只如实记录）。
7. **module_ports.registry 中 entry 为声明名**：真实 DLL 绑定属 ABI-00x/RT-002+（registry note 原文）。
8. `docs/DOCUMENT_INDEX.yaml` 的 base_product_version 仍记录 GOV-002 集成时
   的基线修订值（属机器修订关系字段，版本检查器豁免），待 GOV-005 收敛。
9. 本 patch 前后检查器输出含 warnings（memory/CHANGELOG 历史轮次、他人路径遗留），
   不判 FAIL，属 GOV-005 收敛对象。

## 6. 结论

负责人从 `REVIEW.md` 可到达全部 L0 权威入口；五份 owner 文档对
三 Phase 隔离 / Windows 优先 / ACR dormant / Phase3 状态采用统一口径，
未出现未验证的"已实现"表述。本 patch 通过三个 doccheck 检查器；
文档级验收在返回包 logs 留档。

---
authoring_task: GOV-004
authoring_owner: SA-GOV-01
base_main_sha: caee3e67e5a209a9e47b514f42b2b63f3dc4da4e
