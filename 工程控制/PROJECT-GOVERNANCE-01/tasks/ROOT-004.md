# 任务：ROOT-004 旧 bug 清单按最新权威订正 + 根目录存疑文档处置

状态：`NOT_STARTED`
层：L0　依赖：ROOT-001（问题扫描 的文件域互斥；ROOT-001 不处置该目录）　文件域互斥组：S4-R

## 负责人裁决（2026-09-16）

- `问题扫描/` 是**上一轮（旧基线）的 bug 清单**，仍是待处理资产：**保留目录，但必须按最新权威设计重新定版**，确认哪些问题仍需整改。
- `设计大纲/` 已确认无用 → 由 ROOT-001 删除（本任务不删）。
- `VISUAL_CHECK_README.md`、`FATDUCK_ACCESS.md`、`CHANGELOG.md`：先核对内容，有用的保留并登记，无用的归档或列出删除建议。
- `VERSION` 与「Alpha 前移除版本信息」不属本控制包，登记进下一轮工程包。

## 目标

1. 把 `问题扫描/` 的 findings（现状：224 份定稿件 / 520 条标题，P0 81 / P1 306 / P2 133，另有 E 层与 FD 自证条目）**逐条对照最新权威设计**（ASTROCS_DESIGN、ENGINEERING_SPEC、docs/plugins/23 篇、docs/ci、docs/design/UNIFIED_MODEL、docs/science、docs/algorithms）与**当前仓库实际状态**，给出订正后的处置结论；
2. 产出「按最新权威订正后的待治理问题清单」，作为下一轮工程包的输入；
3. 核对三个根目录存疑文档的内容与作用，给出保留 / 归档 / 删除建议。

## 基线状态（编制时实测）

- `问题扫描/` 结构：`00_README.md`（只读工作区说明，代号 RQS-2026-01）、`10_PROTOCOL.md`（作业规程）、`20_AGENT_PLAN.md`、`40_OWNER_DECISIONS.md`（48KB 待裁决项）、`findings/`（A_SCI_DEF / B_STD_MISMATCH / C_DOC_CODE_GAP / D_COMMENT / E_TRACE_BREAK / F_TEST_GAP / G_GOV_GATE / H_NUMERIC / I_DOC_HYGIENE 九类 × p0/p1/p2）、`_merge/`、账本/、_recheck/、_verify/、_cache/、_tools/。
- 文件类型统计：md 501、py 501、json 53、txt 18、sh 12。
- `INDEX.md` 自述规模与口径；`SUMMARY.md` 给出跨域根因簇（例如「机器门看着在跑、实际不会红」为结构性形态）。
- 这份清单的证据锚是基于**旧基线**（旧宪章、旧控制包路径、旧文档体系），其中部分问题已被 a861d8f6 之后的文档替换与本控制包任务覆盖或改写。

## 权威依据

- ASTROCS_DESIGN.md §0（权威链）、§1（定位与非目标）、§11.3（状态阶梯）、§12（发布权）
- ENGINEERING_SPEC.md §3（科学红线）、§4（模块规范）、§5（测试规范）、§6（Git）、§7（目录）、§8（机器检查）
- docs/ci/01_CHECKS.md §1-§5（CHK-* 注册语义、门禁分级、新增流程）
- docs/plugins/00_INDEX.md（23 模块责任边界）与 23 篇插件规范
- docs/design/UNIFIED_MODEL.md（数据对象与三类配置分离）
- CONTROL_PACK_SPEC.md §3（差异审计的判定口径：缺口/违规/过时/漂移/无主）
- 本控制包 `GAP_AUDIT.md`（GAP-001..022 与 U-01..U-07）

## 改动范围（文件域）

### 允许改
- `问题扫描/**`（新增订正产物；原始 findings 只追加抬头与结论字段，**不得删除任何原始 finding 文件**）
- 新建 `reports/PROJECT-GOVERNANCE-01/root-scan/**`（订正后的汇总与统计）
- `CHANGELOG.md`、`VISUAL_CHECK_README.md`、`FATDUCK_ACCESS.md` 的处置（保留登记 / 归档 / 列出删除建议）

### 禁止改
- `lib/**`、`cli/**`、`tests/**`、`contracts/**`、`ci/**`、`tools/**`（订正只做判定，不做修复）
- `docs/science/**`、`docs/algorithms/**` 的公式与推导
- `设计大纲/**`（ROOT-001 负责删除）、`VERSION`（下一轮工程包）
- 不得把「问题仍在」判成 RESOLVED，也不得把「设计已变、原判据失效」判成 OPEN

## 步骤

1. 建立订正口径与状态词表，写进 `问题扫描/REBASE.md`：每条 finding 归一为四种结论——`OPEN`（对照最新权威仍成立）、`RESOLVED`（已修复或有可复跑证据证明不再成立）、`VOID`（原判据基于旧设计/旧路径，最新权威下不构成偏差）、`UNVERIFIABLE`（证据不足，写明缺什么）。
2. 分片：按类别 × 优先级切分（九类 × p0/p1/p2），派叶子 Agent 逐条订正；每条必须给出「最新权威条款 + 当前仓库证据（文件:行 或 命令输出）+ 结论 + 归属任务/新任务建议」。**P0 条目必须逐条亲自复核**，不得只抽样。
3. 合并去重：产出 `问题扫描/REBASE_TABLE.md`（逐条：ID | 原类别 | 原优先级 | 结论 | 最新权威依据 | 当前证据 | 归属任务 | 备注）与 `reports/PROJECT-GOVERNANCE-01/root-scan/SUMMARY.md`（按结论分组的统计：OPEN/RESOLVED/VOID/UNVERIFIABLE 各多少，OPEN 中 P0/P1/P2 分布）。
4. 与现有控制包对账：OPEN 条目逐条映射到 `TASK_LIST.md` 的 29 个任务；映射不上的（现有任务覆盖不到）单独列成「下一轮工程包候选任务」，每条给出文件域与验收门建议。
5. 根目录存疑文档核对：`CHANGELOG.md`（16KB 版本历史）、`VISUAL_CHECK_README.md`（可视化检查说明）、`FATDUCK_ACCESS.md`（**含凭据，只读核对，不得打印内容、不得复制到任何提交物**）；对每份给出：内容摘要（不含敏感值）、是否与最新权威冲突、建议（保留 / 归档到 docs/archive/ / 由下一轮包删除）。
6. 在 `问题扫描/00_README.md` 与 `INDEX.md` 顶部加`REBASE`抬头：说明原清单基于旧基线、现行结论以 `REBASE_TABLE.md` 为准、原始 findings 保留为历史证据。

## 验收门（前台独立复跑）

- [ ] 订正覆盖度：结论条目数 == 原始 finding 条目数（给出计数与差集为空）
- [ ] 所有 P0 条目都有「最新权威条款 + 当前仓库证据 + 结论」三要素，抽查 10 条可复现（给出命令与输出）
- [ ] `REBASE_TABLE.md` 的 OPEN 条目 100% 映射到现有任务或「下一轮候选任务」（给出映射计数）
- [ ] `VOID` 条目必须写明「原判据依据的旧文档/旧路径」与其最新替代（无一条只写「过时」）
- [ ] 原始 finding 文件零删除（`find -type f | wc -l` 前后对比）
- [ ] `问题扫描/` 之外无改动（`git status --porcelain=v1` 核对）

## 证据命令
```bash
mkdir -p run/PROJECT-GOVERNANCE-01/ROOT-004/logs reports/PROJECT-GOVERNANCE-01/root-scan
find 问题扫描 -type f | wc -l | tee run/PROJECT-GOVERNANCE-01/ROOT-004/logs/scan-files-before.txt
grep -rc '^### \|^## ' 问题扫描/findings --include=*.md | awk -F: '{s+=$2} END {print s}' | tee run/PROJECT-GOVERNANCE-01/ROOT-004/logs/finding-count.txt
# 订正后：REBASE_TABLE 行数、结论分组统计、映射计数
```

## 执行规则（每个任务都适用）

- 开工前按 `AGENTS.md §1` 读完本任务「权威依据」列出的全部条款；未读不开工。
- 只改本文件「改动范围」声明的文件域；**不顺手改无关代码**。
- 不改 `docs/science/**` 公式与 `docs/algorithms/**` 推导；科学定义只读。
- 工作区有大量预存改动与未跟踪文件：**不得** reset/stash/clean/checkout，**不得**把它们混进本任务改动。
- SubAgent 零 git 写权限：不 commit、不 push、不建分支；改动留工作区并交自证材料。
- 所有外部命令带 timeout，日志落 `run/PROJECT-GOVERNANCE-01/<任务ID>/logs/`。
- 报告「完成」必须附：命令、退出码、关键输出片段、产物路径。禁止用「环境问题/工具问题」掩盖失败。
- 无法判定的一律登记 `UNVERIFIABLE` 并写明缺什么，不得为凑数强判。

## 交付物

1. 限「改动范围」内的改动；
2. `run/PROJECT-GOVERNANCE-01/ROOT-004/logs/` 下的命令日志；
3. 自证摘要（任务ID / 改动清单 / 逐条验收命令与退出码 / 未决项）。
