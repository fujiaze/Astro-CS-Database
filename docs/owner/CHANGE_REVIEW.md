# 变更审查（Change Review）

> 文档 ID：DOC-GOV-OWNER-CHANGE-001
> 状态：ACTIVE_NORMATIVE（GOV-004 建立，SA-GOV-01）
> 建立基线：`caee3e67e5a209a9e47b514f42b2b63f3dc4da4e`（GOV-004 工作树检出的基）
> 收敛基线：DOC-CONV-001，BASE_SHA = `da3c4b4aaf64ef9b61039fabd1100ddd1f9b8540`
> 目标产品：`0.11.0-alpha.2`（根 VERSION）
> 状态词约定同 `docs/owner/RELEASE_STATUS.md` §0（`CONTRACT_READY`/`IMPLEMENTED`/
> `INSTALLED`/`VERIFIED` 阶梯）；本文件汇总"已集成到 main 的变化"、
> GOV-004 文档骨架变化与 DOC-CONV-001 状态收敛，供负责人逐项审查。

## 1. 本轮集成到 main 的变更（基线之前的提交链，依 00_READ_FIRST 与 git 历史）

| commit | 内容 | scientific_change | 域 |
|---|---|---|---|
| `065c81d` | GOV-001 冻结工程约束（根约束文件 + AGENTS.md 精简 + 机器索引） | NO | governance |
| `b7b2dea` | GOV-002 归档非当前工程文档（DOCUMENT_INDEX.yaml；854+27 文件迁 archive） | NO | governance |
| `39e7731` | GOV-003 统一版本事实源（根 VERSION 单源化，当时基线值见 CHANGELOG 历史节；5 版本命名空间；生成链） | NO | build/version |
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
| `docs/owner/SCIENCE_OVERVIEW.md` | 新增 | 科学权威汇总；Phase1/2/3 逐项状态（**历史条目**：当时标 SIN/ZEA/CAR/AIT + interp4 + 流式 FITS 为未实现，其中四投影口径已由 §7 更正为 TAN/SIN/CAR/AIT 且已 IMPLEMENTED） |
| `docs/owner/PIPELINE_OVERVIEW.md` | 新增 | 三 Phase 隔离模型与内部链（**历史条目**：当时 run --phases 仍未删；该入口已由 CLI-002 删除，见 §5-2） |
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
2. ~~`astrocs run --phases 1,2,3` 遗留未删~~ → **已由 CLI-002 删除**（DOC-CONV-001
   复核：kRules 无 `run`/`graph`；实测 `astrocs run --phases 1,2,3` → rc=2
   `unknown command 'run'`），与约束 §A.4 一致。
3. ~~每 DAG 节点唯一真实模块 operation 未达成（约束 §F.1）~~ → **已由 P1-001
   （`9e09941a`）、P2-001（`439f9f20`）、P3-002（`1a56ffb7`）达成**：
   `lib/core/src/module_adapters.cpp`:4257/:4282/:4309 的 P1 八节点 / P2 七节点 /
   P3 五节点各绑唯一真实 operation；DOC-CONV-001 本提交以 ctest
   `p1001_real_nodes`/`p2001_real_nodes`/`p3002_real_nodes`/`p3002_uncertainty`
   4/4 实测复核。
4. **Phase3 投影与扩展**（DOC-CONV-001 更正）：负责人裁决 §18.1 冻结的**四投影为
   TAN+SIN+CAR+AIT**（旧表述含 ZEA 已更正）——registry v1 实现已落位
   （`lib/phase3_proj/p3_projection.cpp`:267-273）并经 ctest
   `p3_projection_units`/`p3_projection_fault` 与独立 numpy Oracle 实测（`IMPLEMENTED`），
   但生产会话/DLL 挂载未切换（`module.yaml` `entrypoint: MISSING` → 非 `INSTALLED`）；
   `healpix_interp4` 与 Phase3 流式 FITS 接入当前 `NOT_IMPLEMENTED`（P3-RSMP/P3-PROJ-INT
   后续任务域）。
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

## 7. DOC-CONV-001：L0 与模块状态文档收敛（本次提交）

BASE_SHA = `da3c4b4aaf64ef9b61039fabd1100ddd1f9b8540`（执行时 HEAD = main =
origin/main 三 SHA 一致）；cprun run `Rmtxvlrtfa66eb7` rev23 / dispatch
`7c0b15abaee56640`。`scientific_change=false`：只改状态表述与证据锚，
不动任何公式、容差、冻结门、负责人裁决、生产源码与测试。

| 动作 | 内容 |
|---|---|
| 状态词统一 | 全 L0 文档由历史三级口径（合同冻结/源码在位/执行验收）切换为 `CONTRACT_READY`/`IMPLEMENTED`/`INSTALLED`/`VERIFIED` 阶梯，权威定义 = `docs/owner/RELEASE_STATUS.md` §0 |
| 删除已修复项的陈旧陈述 | `run --phases` 遗留（CLI-002 已删）、§F.1 节点化（P1/P2/P3 已完成）、四投影（P3-001 已实现）三处 NOT_VERIFIED 以当前提交证据替换 |
| 更正错误事实 | 冻结四投影由旧表述 `SIN/ZEA/CAR/AIT` 更正为宪章 §18.1 的 `TAN+SIN+CAR+AIT`；`cli/runtime_client.cpp` Phase1 IR 由"单节点"更正为 cal→cosmetic 两节点 |
| 落地状态分层 | MOD 安装面与 CLI 命令面按实测写 `INSTALLED`；四投影/会话路径写 `IMPLEMENTED`（`entrypoint: MISSING` 不冒认）；Windows/真实数据保持 `NOT_VERIFIED` |
| 模块地图 | `docs/architecture/MODULE_MAP.md` 重写为"模块/路径/交付状态/证据锚"列，覆盖 `lib/` 全部模块目录 |

本提交内的执行级证据（日志 `run/docconv001/logs/`）：

| 检查 | 命令 | 结果 |
|---|---|---|
| 全量构建 | `ninja -C build` | rc=0（28/28 步） |
| 节点化（三 Phase） | `ctest -R "p1001_real_nodes\|p2001_real_nodes\|p3002_real_nodes\|p3002_uncertainty"` | 4/4 PASS |
| 四投影 registry | `ctest -R "p3_projection_units\|p3_projection_fault"` | 2/2 PASS |
| RT 唯一 executor | `ctest -R rt001_unique_executor` | PASS |
| MOD 安装面/安全 loader | `python3 tests/abi/mod001_install_load_check.py --build-dir build` | 64/64 PASS（含负向注入必败） |
| CLI 命令面 | `python3 -m pytest tests/cli/test_cli001_vpi.py -q` | 15/15 PASS |
| 遗留入口已删 | `build/cli/astrocs run --phases 1,2,3` | rc=2 `unknown command 'run'` |
| doccheck 全套 | `check_doc_index.py --strict` / `check_engineering_constraints.py` / `check_version_namespaces.py` / `check_l0_docs.py` / `check_standards_registry.py` / `check_api_docs.py` / `check_glossary.py` / `check_doc_symbols.py` | 全部 rc=0 |

域外项（DOC-CONV-001 写白名单 = `REVIEW.md` / `docs/` / `memory.md`，以下均未改动）：

1. `lib/*/README.md`（任务目标提及）不在写白名单内 → 未改动，移交 lib/ 写域任务；
2. 跨 L0 文档状态词一致性目前**无 CI 检查项** → 建议 tools/+ci/ 域任务补 checker
   并在同提交注册 `ci/checks.json` 显式检查项；
3. 05 号 findings 登记册与 `ci/checks.json` 均在写白名单外 → 本文与 `memory.md`
   如实登记，由前台并入登记册；
4. **F-DOC-CONV-001-06（HEAD 预存 CI 红灯，非本任务引入）**：`tests/version`（UT-VERSION）在
   BASE=`da3c4b4a` 即失败 —— `tools/check_version_consistency.py` rc=1，19 条
   findings 全部落在 `docs/standards/STANDARDS_REGISTRY.md`（该文件相对 HEAD 零
   diff），成因为注册表内 FITS WCS Paper I 与 IVOA HiPS 的**条款编号**（形如
   §a.b.c 的节号）被版本一致性检查器误判为"未知版本字面量"，疑为 STD-REG-001
   （`fb7f232a`）检查器口径缺口；本任务写域外，只登记
   不修（建议 CI-REPAIR 常驻线处置：检查器排除 `§` 前缀条款号，或 registry 冻结表
   加机器豁免字段）。

---
authoring_task: GOV-004
authoring_owner: SA-GOV-01
base_main_sha: caee3e67e5a209a9e47b514f42b2b63f3dc4da4e
convergence_task: DOC-CONV-001
convergence_base_sha: da3c4b4aaf64ef9b61039fabd1100ddd1f9b8540

