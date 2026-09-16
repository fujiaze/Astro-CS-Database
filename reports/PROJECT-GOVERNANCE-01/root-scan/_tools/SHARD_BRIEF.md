# ROOT-004 分片作业简报（所有分片共用；字段与口径以本文件为唯一准绳）

- 任务：ROOT-004「旧 bug 清单按最新权威订正 + 根目录存疑文档处置」的**分片执行层**。
- 基线：`HEAD = main = ecf6ad6f`；`origin/main = f96dff61`。**只做判定，不做任何修复**。
- 你的产物是**唯一被机械合并**的东西：字段必须逐字符合 §3；主控用脚本按 §3 直接 parse，任何偏差都会导致该行被丢弃并要求重做。

---

## 1. 四态定义（唯一口径，不得自创第五态）

| 结论 | 判据 |
|---|---|
| `OPEN` | 对照**最新权威条款**，当前仓库状态**仍然违反/仍然缺失**（≠「原 finding 说自己还在」——必须你本轮亲自在树里复现） |
| `RESOLVED` | 有**可复跑证据**证明该问题不再成立：代码/文档已删或已改、门已实现、旧路径整体消失且无等价物、缺陷对象已不存在 |
| `VOID` | 原判据基于**旧设计或旧路径**，在最新权威下**不构成偏差**：旧条款已撤销、被替换为不同/相反要求、或原路径废止且新设计不要求该物 |
| `UNVERIFIABLE` | 证据不足。**必须第 10 列写明缺什么**（缺构建/缺 Windows 节点/缺真实数据/缺外网原文/缺负责人裁决/缺 23 模块映射表…） |

**三条禁令（违反即该行作废）**
1. 禁止把「问题仍在」判成 `RESOLVED`；
2. 禁止把「设计已变、原判据失效」判成 `OPEN`；
3. `VOID` 必须写「原判据依据的**旧文档/旧路径**」与其**最新替代**；只写「过时」二字不合格。

**判定纪律**
- 原 finding 的「仍成立」自述**不是证据**；本轮必须用**当前树**的 `read`/`grep`/`git ls-files`/解释器解析重新取证。
- 行号一律按当前树重定位；finding 里的行号漂移本身可作 `E_TRACE_BREAK` 类的证据，但不等于结论。
- 集合类判据（「全仓零命中」「N 处」）必须用命令重算并给出数字口径；不得直接抄 finding 的数字。
- 无法判定就 `UNVERIFIABLE`，**不得为凑数强判**。

---

## 2. 旧权威 → 最新权威 映射速查（判 VOID/OPEN 的关键工具）

最新权威链（`ASTROCS_DESIGN.md §0`）：**ASTROCS_DESIGN > AGENTS.md > ENGINEERING_SPEC > CONTROL_PACK_SPEC > docs/ci > docs/plugins(23 篇)**；
另有 `docs/science`（公式权威）、`docs/algorithms`（推导权威）、`docs/design/UNIFIED_MODEL.md`（数据对象与配置）。
**不在该链上的**：`ASTROCS_PROJECT_CONSTITUTION.md`（旧宪章，仍 tracked 但已非权威）、`AstroCS_ENGINEERING_CONSTRAINTS.md`（ARCHIVED_NON_NORMATIVE）、`docs/standards/STANDARDS_REGISTRY.md`（旧冻结注册表，非链上权威）、`docs/contracts/*`、`docs/traceability/*`、`docs/quality/*`、`docs/modules/registry/*`。

| 原 finding 常引的旧判据 | 最新权威替代（写第 5 列时用它） |
|---|---|
| 旧宪章 §1.1/§1.2（权威链、权威倒置） | `ASTROCS_DESIGN.md §0`（本文与其他文档冲突以本文为准；Agent 无权放宽/重新解释） |
| 旧宪章 §3.2（三 Phase 不得隐式串接） | `ASTROCS_DESIGN.md §1.2`（三个平级独立命令；禁止隐式串接；阶段间只经磁盘产品+manifest+哈希） |
| 旧宪章 §4.1（数据对象不得混同） | `docs/design/UNIFIED_MODEL.md §2`（数据对象表「可否作权重」）+ `ASTROCS_DESIGN.md §2` |
| 旧宪章 §5.3（Drizzle 能量/面亮度语义） | `ASTROCS_DESIGN.md §11.1`（科学不变量验证）+ `docs/plugins/algorithms_phase1/08_drizzle.md` |
| 旧宪章 §6.3（support/coverage 永不作 ivar/SNR 权重） | `docs/design/UNIFIED_MODEL.md §2`（support/coverage「否」）+ `ASTROCS_DESIGN.md §4.3`（逆方差叠加，SNR→权重） |
| 旧宪章 §7.3（单位/面亮度传播须由 SCI/ALG 明确） | `ENGINEERING_SPEC.md §3`（科学代码红线：公式/权重/精度/容差不可改）+ `UNIFIED_MODEL.md §1` |
| 旧宪章 §8.1（单入口） | `ASTROCS_DESIGN.md §6.1/§6.2/§10.1`（唯一可执行入口 + 唯一命令树） |
| 旧宪章 §10.1（ACR 生产不可达） | `ASTROCS_DESIGN.md §1.3`（非目标）+ `§7.1`（acr DORMANT）+ `§8` |
| 旧宪章 §10.4（禁硬编码 workers/私有线程池） | `ASTROCS_DESIGN.md §8` + `ENGINEERING_SPEC.md §10` |
| 旧宪章 §10.5（线程/利用率门） | `ASTROCS_DESIGN.md §8` + `docs/plugins/infrastructure/19_runtime.md`、`21_observability.md` |
| 旧宪章 §12.2（注释只写单位/数学原因/前后置条件…） | `ENGINEERING_SPEC.md §2`（注释禁令：禁止堆积历史版本号/任务编号/审计流水/代码复述） |
| 旧宪章 §12.3-3（公式唯一实现） | `ENGINEERING_SPEC.md §3` + `§4`（单一 entrypoint；禁止越权覆盖 SCI） |
| 旧宪章 §12.3-4（独立 Oracle） | `ASTROCS_DESIGN.md §11.1` + `ENGINEERING_SPEC.md §5.1`（不调用生产实现的独立 Oracle 或解析解） |
| 旧宪章 §12.3-9（引用存在性） | `ENGINEERING_SPEC.md §8`（检查器覆盖：算法引用有效 SCI/ALG、无悬空引用、能红能绿） |
| 旧宪章 §13（性质/不变量测试） | `ASTROCS_DESIGN.md §11.1` + `ENGINEERING_SPEC.md §5.1` |
| 旧宪章 §14（目录与原地演进） | `ENGINEERING_SPEC.md §7`（根固定条目白名单 + `lib/algorithms`/`lib/infrastructure` 二分） |
| 旧宪章 §15.3/§15.4（验证节点/职责矩阵） | `ASTROCS_DESIGN.md §11.2/§11.3`（验证层级与状态阶梯；VERIFIED=Windows x64+真实数据） |
| 旧宪章 §16（版本纪律） | `ASTROCS_DESIGN.md §12` + `ENGINEERING_SPEC.md §7`（Alpha 前程序与产物零版本信息） |
| 旧宪章 §17.1（发布前提：全部偏差已登记） | `ASTROCS_DESIGN.md §12`（发布候选清单：合同冻结无冲突、追踪无断链、双平台 CI、真实数据终验、P0/P1=0…） |
| 旧宪章 §17.10（P0/P1=0） | `ASTROCS_DESIGN.md §12` 同款（P0/P1=0）+ `CONTROL_PACK_SPEC.md §7.3`（不得用 waiver 掩盖红灯） |
| `AstroCS_ENGINEERING_CONSTRAINTS.md`（ARCHIVED_NON_NORMATIVE） | 若其要求在 `ENGINEERING_SPEC` 无对应 → 该判据 `VOID`；有对应则改用对应节号 |
| 旧控制包路径 `工程控制/AstroCS_*`（已删） | `CONTROL_PACK_SPEC.md §4`（控制包目录唯一模板 `工程控制/<包ID>/`）+ `§3.3`（现状须可复核） |
| `ci/checks.json` 旧 145 项 / `CHK-*` | `docs/ci/01_CHECKS.md §2`（27 个 `CHK-*`）+ `§3`（门禁分级 P0/P1/P2）+ `§4`（新增流程：能绿能红）+ `§5`（`python3 ci/run_checks.py`） |
| 命令树 `phase1/2/3 …` | `ASTROCS_DESIGN.md §6.2`（`normalize/mosaic/export` + `help/--version/doctor/benchmark`） |
| `config/` 位置与 defaults/filters | `ASTROCS_DESIGN.md §3.3`（程序根 `config/filters.json`、`config/defaults.json`） |
| 产品名/安装树旧形态 | `ASTROCS_DESIGN.md §10.1`（`ACSD Cli.exe`/`acsd_cli`，唯一 exe+各 dll+schemas+manifest）+ `§10.2` |
| 国际标准域（FITS WCS Paper I/II、HiPS、HEALPix、Drizzle、Gaia） | `ASTROCS_DESIGN.md` 附录 B + `§5.3`（八投影冻结）+ 对应 `docs/plugins/*` + `docs/science`/`docs/algorithms` 冻结文；**标准偏离仍是偏差**，不因注册表换版而 VOID |
| 文件域/根目录散落 | `ENGINEERING_SPEC.md §7` + `ASTROCS_DESIGN.md §6.3`（产物只落 output_dir） |
| 「VERIFIED/COMPLIANT/PASS」无证据 | `ASTROCS_DESIGN.md §11.3`（状态阶梯唯一口径）+ `§12`（未实现/未验收必须明确报告，不得用空输出/文档声明冒充） |

---

## 3. 输出格式（PSV：`|` 分隔，**10 列**，逐字如此）

文件：`reports/PROJECT-GOVERNANCE-01/root-scan/shards/<分片名>.psv`（UTF-8）
第一行必须是（逐字）：

```
ID|原类别|原优先级|旧判据(文档+节号/路径)|最新权威条款|当前证据(命令+输出)|结论|归属|GAP关系|备注
```

然后每条 finding **恰好一行**，行序 = 分配文件顺序。**列内不得出现 `|` 字符，不得换行**（用 `；` 与空格代替）。

| 列 | 内容要求 |
|---|---|
| 1 ID | 逐字复制分配表 ID（如 `M2a-A-1`、`V11-N-04`） |
| 2 原类别 | 逐字复制分配表 |
| 3 原优先级 | `P0`/`P1`/`P2`（`P?` 照写） |
| 4 旧判据 | 原 finding/账本实际引的**旧文档+节号**或**旧路径**（如 `旧宪章 §17.10`、`AstroCS_ENGINEERING_CONSTRAINTS.md §x`、`工程控制/AstroCS_…_V6_…/`）；若原判据本身就是现行权威，写 `无（原判据即现行权威）` |
| 5 最新权威条款 | `<文档名> §<节号>：<原文要点≤2 句>`。写前用 `read`/`grep` 核对节号与原文确实存在 |
| 6 当前证据 | `命令：<单行命令>；输出：<≤3 行关键输出>` —— 必须你**本轮真跑过**，输出逐字 |
| 7 结论 | `OPEN`/`RESOLVED`/`VOID`/`UNVERIFIABLE` |
| 8 归属 | 30 个任务之一的 ID（`BASE-001`…`FINAL-001`，见 §4）或 `NEXT-PACK:<候选ID>`（自拟如 `NEXT-PACK:NP-07`） |
| 9 GAP关系 | `无` 或 `与 GAP-0xx 重复`（与 `工程控制/PROJECT-GOVERNANCE-01/GAP_AUDIT.md` 的 23 条逐条比对；重复不合并、保留原 ID） |
| 10 备注 | 一句话；`VOID`/`UNVERIFIABLE` 必填理由/缺什么；`RESOLVED` 必填「什么证据使其不再成立」 |

另写一份 `reports/PROJECT-GOVERNANCE-01/root-scan/shards/<分片名>.md`（≤40 行）：
分片名、行数、ID 覆盖自证（命令+输出）、异常（无法定位的 ID/找不到原文的条款）、以及你判 `UNVERIFIABLE` 的条目清单。

---

## 4. 现有 30 个任务（第 8 列只能填这些或 NEXT-PACK）

`BASE-001` 冻结基线与预存改动边界｜`ROOT-001` 根清洁账本与清运｜`ROOT-002` 根目录长效机器门｜`ROOT-003` run/ 保留策略与清运｜`ROOT-004` 本任务｜`GOV-001` 收敛最高权威/根目录/废止治理入口｜`DOC-001` 收敛活动文档术语/状态/引用｜`DATA-001` 统一数据对象合同链（14 对象）｜`CFG-001` defaults/filters 与三命令配置合同｜`MOD-001` 23 插件→模块/target/清单映射门｜`ARCH-001` 目标源码根（lib/algorithms + lib/infrastructure）与等价迁移骨架｜`AIO-001` 唯一 AIO 与原子产品边界｜`RT-001` 统一调度器/typed DAG/线程预算｜`CLI-001` 唯一用户命令树｜`CLI-002` 模板/预检/确认/force 语义｜`CLI-003` 机器输出/退出码/取消协议｜`P1-001` normalize 模块与固定 DAG｜`P1-002` frame_snr/稀疏层/normalize 输出｜`P2-001` mosaic 固定 DAG 与产品合同｜`P2-002` 按需逆方差与三科学目标权重｜`P3-001` 八投影 registry 与独立 Oracle｜`P3-002` 反向重采样/模式/流式 FITS｜`CPU-001` CPU provider/benchmark/profile｜`OBS-001` 观测事件/资源门/运行图｜`INT-001` 目标构建图/注册表/产品装配｜`CI-001` 新规范机器检查入口与治理门｜`QA-001` 双平台 CI 与完整测试矩阵｜`PKG-001` 双平台安装树/清单/候选包｜`REAL-001` 最终 SHA 真实数据与图像验收｜`FINAL-001` 独立总审计与差距闭合。

映射速查：根目录条目→`ROOT-001/002/003`；旧宪章/旧治理入口/版本纪律→`GOV-001`；活动文档术语/状态/陈旧版本→`DOC-001`；合同/schema/数据对象→`DATA-001`；`config/`→`CFG-001`；模块 id/README/module.yaml/registry→`MOD-001`；源码目录与 DLL/SO 边界→`ARCH-001`+`MOD-001`；I/O/manifest/原子提交→`AIO-001`；调度/线程/资源/监控→`RT-001`+`CPU-001`+`OBS-001`；CLI 面→`CLI-001/002/003`；Phase1/2/3 科学与产品→`P1-*`/`P2-*`/`P3-*`；CI 检查项/门禁→`CI-001`（+测试矩阵 `QA-001`）；打包/安装/版本产物→`PKG-001`；真实数据/Windows 复验→`REAL-001`/`QA-001`；总审计→`FINAL-001`。

---

## 5. 纪律（硬约束，违反即该分片作废）

1. **零修复**：不改任何代码/文档/测试/CI；只在 §3 的两个路径写文件。
2. **零 git 写**：不 commit/push/branch/stash/reset/clean/checkout/add；只读查询（`git rev-parse`、`git ls-files`、`git log -1`）允许且推荐。
3. **不碰**：`lib/** cli/** tests/** contracts/** ci/** tools/** docs/** AGENTS.md ASTROCS_DESIGN.md ENGINEERING_SPEC.md CONTROL_PACK_SPEC.md VERSION 设计大纲/**`；**仓库根条目一个都不动**（另一条线在做根清洁）。
4. **`FATDUCK_ACCESS.md` 含凭据：不得 read/打印/复制其内容**（本任务里你也不需要它）。
5. 所有外部命令带 `timeout`；日志落 `run/PROJECT-GOVERNANCE-01/ROOT-004/logs/shards/<分片名>.log`。
6. 结构化文件（JSON/CSV/YAML）一律用解释器解析，不用 `read` 逐行读大文件。
7. 禁止用「环境问题/工具问题」掩盖失败；跑不动就 `UNVERIFIABLE` + 写明缺什么。
