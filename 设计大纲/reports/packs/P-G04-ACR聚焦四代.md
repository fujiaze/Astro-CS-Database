# P-G04：ACR 聚焦控制包 V1–V4（含底层支线 acr）取证报告

分析组：G04（ACR 聚焦控制包 V1–V4，四代）。身份：'acr'、'ACR_FOCUSED_CONTROL_PACKAGE'（下称 V1）、'_V2'、'_V3'、'_V4'。
方法基线：digest 五份（'设计大纲/_evidence/packs/digest/acr.md' 等）、'pack_lineage_table.md'、'pack_commit_link.csv'、'pack_inventory.csv'，包内本体按现存解包件（'engineering/control/archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1/'，下称【容器】）与历史复原件（'设计大纲/_evidence/packs/history/<rev>/…'）实读核对；git 断言均以 'git -c core.quotepath=false' 逐提交验证。全程只读，未解压任何 zip（本组五身份均无 zip 实例）。

---

## 一、组内包清单与身份归并

| 身份 | 存形态实例数 | 形态 | 文件数 | zip | zip sha256 | 首次出现 | 最后出现 | 是否被删除 / 删除提交 |
|---|---|---|---|---|---|---|---|---|
| acr | 2 | 仅历史复原 + 解包件（容器归档） | 复原 4 / 归档 50 | 无 | 无 | f8d749e4 2026-08-02（main 上最早添加三件套；台账 'pack_inventory.csv' acr 行记 first=12fb99f3 2026-08-04，见第八节 3） | b7b2dea7 2026-09-02（整体归档） | 部分删除：'工程控制/tasks/acr/spec.md'、'tasks.md' 在 49ea5c1e（2026-08-02）删除（'git show --name-status 49ea5c1e'）；目录整体随 b7b2dea7 以 R100 移入容器，原中文路径不再存在于 HEAD |
| ACR_FOCUSED_CONTROL_PACKAGE | 3 | 原始复原件 + 归档复原件 + 容器解包件 | 21 / 21 / 20 | 无 | 无 | 0030c3a5 2026-08-05（'_PROVENANCE.json' first=last=0030c3a5） | b7b2dea7 2026-09-02 | docs 原件在 e1604f14（2026-08-27）删除并移至 '工程控制/archive/'；archive 副本在 b7b2dea7 以 R100 移入容器（'git show --name-status b7b2dea7' 中 ACR_FOCUSED+tasks/acr 共 134 条 R100） |
| …_V2 | 3 | 同上 | 22 / 22 / 21 | 无 | 无 | 40c9d216 2026-08-06 | b7b2dea7 2026-09-02 | 同 V1 轨迹（e1604f14 删 docs，b7b2dea7 R100 归档） |
| …_V3 | 3 | 同上 | 22 / 22 / 21 | 无 | 无 | a36c4825 2026-08-06 | b7b2dea7 2026-09-02 | 同上 |
| …_V4 | 3 | 同上 | 23 / 23 / 22 | 无 | 无 | 38b85702 2026-08-06 | b7b2dea7 2026-09-02 | 同上 |

同名多形态的文件集合一致性：
- V1–V4：三种形态的**载荷文件逐字节一致**（对 0030c3a5 vs e1604f14 等成对 'diff -r' 仅 '_PROVENANCE.json' 的 rev/路径字段不同；容器 worktree 相对复原件仅缺少 '_PROVENANCE.json' 一个文件，故计数 21→20 等差 1）。证据：本节 diff 实测；'pack_inventory.csv' 行（V1 45175/45178/44827 字节差恰为 _PROVENANCE.json 体积差）。
- acr：两形态文件集合**严重不一致**——复原件仅 3 个载荷文件（'spec.md'/'tasks.md'/'checklist.md'，从 49ea5c1e^ 即 f8d749e4 状态取回，见 'history/49ea5c1e/工程控制/tasks/acr/_PROVENANCE.json' recovered_files=3），容器件 50 个载荷文件（23 份编号规格+audits+examples+include+schemas+templates+SHA256SUMS 等）。差异内容 = 49ea5c1e 至 12fb99f3 期间 acr 支线包的演进态（新增 00—22 编号规格、删除 spec.md/tasks.md）。可核对：'git show --name-status 49ea5c1e / 5be31ca8 / 024c87c9 / 12fb99f3'。
- 五身份均无 zip 原件、无影子树：'pack_lineage_table.md' 形态列均为 'reco,work'；'工程控制/_control_packs/' 实测无任何 ACR zip；'git log --all --name-only' 全史未出现 ACR zip 路径。V1 '00_READ_FIRST.md' §首段自述"包名固定：AstroCS_ACR_Control_Package.zip"（V2/V3/V4 同句沿袭），但仓库内从未见该 zip——入库形态一直是目录。

---

## 二、包内文件地图

**acr（容器 tasks/acr，50 文件）**：23 份编号规格（'00_READ_FIRST.md' + '01_FROZEN_REQUIREMENTS' … '22_FIX_REVIEW_CORRECTION_PLAN'），首行标题职责（实测提自各文件第 1 行）：01 冻结需求 / 02 系统架构 / 03 公共接口设计 / 04 Kernel模型、任务特征与限制 / 05 开源项目详细复用方案 / 06 HardwareProfile Qualification Benchmark 规范 / 07 CostEstimator 与动态异构调度 / 08 资源占用与内存反压控制 / 09 未来算法接入指南（本支线禁止执行）/ 10 底层开发阶段、任务与验收 / 11 Git分支与Agent规则 / 12 测试与验证矩阵 / 13 严格交付包规则 / 14 风险登记 / 15 冻结决策 / 16 Agent总执行指令 / 17 ACR经典实验与硬件画像套件 / 18 合并到 main 与备用集成规范 / 19 当前ACR分支纠正任务 / 20 Phase I AuditPack 专项行动计划 / 21 Commit F 专项纠正计划 / 22 Fix Review 专项纠正计划。另有 'checklist.md'（93 行）、'AUDIT_REPORT_2026-08-02.md'、'audits/' 2 件（COMMIT_F_AUDIT.md、FIX_REVIEW_AUDIT.md）、'examples/' 4 件（README.md + 3 个 .cpp）、'include/astro/compute/acr.hpp'、'package_manifest.json'、'SHA256SUMS.txt'、'00_AGENT_START_PROMPT.txt'。schemas/ 8 件（'*.schema.json' ×6 + '*.example.yaml/json' ×2），templates/ 7 件（含 '名称.template.扩展名' 4 件与 'evidence_index.template.md' 等），命名口径为 '.template.' 中缀。
注意：manifest 'generated_at' 2026-08-03、'updated_at' 2026-08-04（'tasks/acr/package_manifest.json'），其 files 数组仅 38 条，不含 22_FIX、AUDIT_REPORT、FIX_REVIEW_AUDIT、'examples/legacy_chunk_adapter.cpp'、'include/…/acr.hpp'、'schemas/device_execution_report.schema.json'、'schemas/hardware_profile.example.json'、'templates/git_report.template.md'——manifest 清单滞后于包内文件集合（可核对：容器目录 vs manifest files[]）。

**V1–V3（20/21/21 载荷文件）**：同一套 10 份编号规格（00_READ_FIRST、01_SCOPE_AND_DECISIONS、02_TARGET_ARCHITECTURE、03_PUBLIC_API_AND_PARTITION_CONTRACT、04_BENCHMARK_AND_OPERATION_PROFILE、05_MIXED_ROUTING_AND_CHUNKING、06_DATA_RESIDENCY_AND_MEMORY、07_TEST_AND_ACCEPTANCE、08_CURRENT_EXECUTION_PLAN、09_GIT_AND_DELIVERY），职责以首行为准（"范围与冻结决策/目标架构/公共API与分块契约/精简Benchmark与OperationProfile/CPU+GPU动态混合路由与分块/数据驻留、传输（V2 作"分区合并"）与内存预算/测试与验收/当前执行计划/Git与交付规则"）；辅助件 'CHECKLIST.md'、'history/README.md'（V1 起，5 行）、'package_manifest.json'、'00_AGENT_START_PROMPT.txt'、'SHA256SUMS.txt'、schemas/ 4 件（compute_config.example.yaml、execution_report、memory_budget_report、operation_profile 三个 .schema.json）、templates/ 仅 1 件 'evidence_checklist.md'（无 .template 中缀，命名口径与 acr 包不同）。V2 新增 'audits/ACR_FOCUSED_REVIEW.md'（130 行），V3 换为 'audits/ACR_FOCUSED_V2_REVIEW.md'（95 行）。

**V4（22 载荷文件）**：编号规格换轨为 9 份（01_ARCHITECTURE_FREEZE 架构冻结 / 02_BUSINESS_INTEGRATION_BOUNDARY 修改业务代码前的接入边界 / 03_MIXED_EXECUTION_MODEL 混合执行与数据通道 / 04_WEIGHTED_INTEGRATION_EXAMPLE 加权积分最小接入样例 / 05_BENCHMARK_MATRIX_AND_FAIRNESS 矩阵与公平性 / 06_TEST_AND_ACCEPTANCE / 07_CURRENT_EXECUTION_PLAN / 08_GIT_AND_DELIVERY，实测第 1 行标题）；schemas/ 缩为 2 件（weighted_integration_benchmark.example.yaml、weighted_integration_report.schema.json）；新增 'examples/weighted_integration/' 6 件（README + contract.hpp + cpu_reference.cpp + cuda_reference.cu + benchmark_reference.cpp + CMakeLists.reference.txt），README 首段自述为"未来 Agent 实现的接口与算法参考"；删除 'history/'、'audits/' 与 V1–V3 的 4 个 schemas（'diff -rq' V3 vs V4，见第六节）。

---

## 三、任务结构

- **五身份均无 TASK_LEDGER.csv、无结构化任务号**：'pack_lineage_table.md' 各行"台账行=0、任务号族=空"；'pack_commit_link.csv'（表头含"台账任务号数/命中率"列，全表仅 17 行）不含本组任何身份。提交消息中的 'ACR-001/002/003' 号族属 REAUDIT_V3、V7、容器整包等其他身份（'pack_commit_link.csv' REAUDIT_V3 三行、archive 行的命中/未命中样例列），与本组四代包无关。
- **acr 三件套（49ea5c1e 复原件）**：'tasks.md'（382 行）按 Phase A–I 分族，任务条目 66 个 '### <字母><序号>' 小节，每条目固定字段口径：**依赖 / 输入 / 输出 / 验收 /（可并行）**；"状态"字面量仅出现 1 处（A1 "大部分已完成（本 spec §0、§2 已记录）"）。顺序/并行声明为包内自述：Phase C"与 D 并行"、E"依赖 B/C/D"、F"依赖 E"、G"依赖 F"、H"依赖 G"、I"本次不做，等用户授权"（tasks.md 各 '## Phase' 标题行）。'checklist.md'（230 行）勾选分布：[x] 5 / [ ] 154（复原件实测）。
- **acr 容器 50 文件态**：'10_PHASES_TASKS_ACCEPTANCE.md' 以 Phase 0–8 定义顺序与逐段验收；任务粒度为 Phase 条目而非编号任务。'checklist.md'（93 行）67 项全部未勾（容器实测 [x]=0/[ ]=67），其中"Fix Review新增硬门禁"一节 15 项为 12fb99f3 追加（'git show 12fb99f3' 对 checklist.md 为 M）。
- **V1–V4**：任务清单载体是 '08/07_CURRENT_EXECUTION_PLAN.md'——V1 §0–§9 十节（"开始前/保留已通过能力/清理错误方向/建立目标Operation合成套件/精简Benchmark/MixedRoutePlanner/驻留与内存/性能与资格/最终Evidence/完成定义"）；V4 改 A–G 七字母节（固化架构/关闭V3剩余门禁/实现加权积分核心/GPU内部通道/Benchmark与报告/测试与Evidence/完成定义，V4/07 实测）。验收状态字面量口径：'CHECKLIST.md' 复选框两值（V1 54 项、V2 57、V3 57、V4 60，全部未勾，容器实测）；运行时块状态机四值 PENDING/CLAIMED/DONE/FAILED（acr/checklist.md "Commit F事实纠正"节；acr/00_READ_FIRST §6 "coverage状态严格为 pending/claimed/done/failed"）；字面结论状态 SKIPPED、PASS、ABORT（acr/00_READ_FIRST §1 第 7 条"path guard 报告为 ABORT"；V1/09 §2 "失败必须终止，不能在报告中把ABORT写成PASS"）、PERFORMANCE_NOT_QUALIFIED、READY_FOR_BUSINESS_ADAPTER（V4/06 §4、§7；V4/CHECKLIST "工程与Evidence"节末项）。

---

## 四、门禁与验收要求

（全部为包内条款，出处=文件+节号。）

**机器门清单**
1. path guard：提交前检查算法目录/OpenMP/Pipeline/CLI 零修改，"失败必须终止，不能在报告中把ABORT写成PASS"——acr/00_READ_FIRST §5 与 checklist.md"单一实现与范围"（要求 PASS 且 exit code=0）；V1/09_GIT_AND_DELIVERY §2；V4/08 §1。
2. 构建与测试门：CPU-only 与 CUDA 双构建通过、CTest 0 failed、0 timeout、SKIPPED 原因准确——V1/07 §6；V2/CHECKLIST"测试与交付"；V4/06 §6。
3. Sanitizer 门：ASan/UBSan 真实开启 + compute-sanitizer memcheck/racecheck——acr/checklist.md"Benchmark和可靠性"节；V1/07 §6；V4/06 §6。
4. 超时门：所有外部命令/构建/基准设明确超时并终止进程树——acr/10 共同要求节"所有外部命令设超时"；V4/08 §4 给出建议值（configure/build 600s、quick CTest 600s、standard 900s、full 1800s、sanitizer 600s）；V4/06 §5 规定 quick CTest 单项 ≤180s。
5. 性能门（V1 起四代沿袭）：AutoMixed 中位耗时距实测最佳模式 ≤10%——V1/07 §3；V2/CHECKLIST"混合路由"；V4/06 §4；另 V4/06 §4 增设"至少一个中/大 case Auto 相对 OpenMP 正加速（建议资格线 >=1.05x），否则状态必须为 PERFORMANCE_NOT_QUALIFIED 且不得开始真实业务改造"。
6. 单一 HEAD Evidence 门：证据须出自最终单一干净 HEAD、'git status --porcelain' 为空、在仓库外生成、不混旧 HEAD——V1/09 §3；V4/08 §3（列 14 类证据文件清单）；V2/V3 audit 各以"源码HEAD vs Evidence HEAD 不一致"为阻断项（V2/audits 头部字段；V3/audits 阻断项 10）。
7. 范围零修改门：Phase1/积分/Drizzle/HISS/Pipeline/CLI/OpenMP 零修改——V1/00 §4；V3/00 §5；V4/00 §4。

**完成顺序要求**：acr/10_PHASES 冻结 Phase 0→8 顺序（Phase 5 验收："工具链不可用只能 SKIPPED 且阻断最终合并"）；acr/00_READ_FIRST §6 列 11 项合并前置清单并声明"Commit F 后不得合并 main"、全部通过才允许 --no-ff 合并；V1/08 §9"完成定义"= 满足 07+CHECKLIST 即可 dormant 合并 main；V4/07 G"完成定义"=满足 06+CHECKLIST 后冻结架构、样例成为业务接入模板、"后续真实积分/Drizzle改造另发控制包"。

**状态机口径**：见第三节（coverage 四态 pending/claimed/done/failed，acr/checklist.md 与 00_READ_FIRST §6；每块恰好一次，V1/07 §2）。

**waiver/豁免机制**：本组五包内**未检索到** waiver/豁免登记条款；最接近的是"准确说明工具链限制"式表述（V2/CHECKLIST、V4/06 §6）与 SKIPPED 语义（acr/10 Phase 5），均为"如实标注"而非放行机制。是否引用仓库级 'ci/checks.json' waiver 体系：包内未证实。

**审核包与提交要求**：V1/09 §1 建议提交序列 5 条（首条 'refactor(acr): focus runtime on target pixel operations'）；V4/08 §2 建议 5 条并规定"可合并提交，但每个提交必须可构建，不允许用Evidence提交制造第二HEAD"；交付物口径 V1/09 §4、V4/08 §5（禁 build 目录、依赖缓存、重复源码树、旧 Evidence）；acr/13_DELIVERY_PACKAGE_RULES.md 与 acr/manifest merge_blockers 6 条（首条"dynamic work pool concurrency safety not proven"，manifest 字段）为同类门。V2/V3/V4 manifest/audit 记录被审评审包名与 SHA：V2/audits 头（AstroCS_Review_ACRFocused_20260805.zip）、V3/audits 头（AstroCS_Review_ACRFocusedV2_20260806.zip）、V4/00 §5（AstroCS_Review_ACRFocusedV3_20260806 + SHA-256 04735781…）；这些 zip 均未入库（第八节 6）。

---

## 五、设计意图（转述）

- **acr 底层支线包**：为 ACR 底层运行时的隔离支线开发提供唯一权威控制包——manifest 字段 versioning: "none; overwrite this single authoritative package"；范围仅 ACR API/backend/调度/资源，"严禁修改或接入 Drizzle、批量积分、HISS、校准…"（00_READ_FIRST §3）；对 Commit F 成果"可保留为中间基础"但九项能力"不得继续宣称完成"（§1）。负责人/用户指令痕迹：复原件 spec.md 头部"控制包版本 AstroCS_ACR_Branch_Control_Package_V1.2_2026-08-02"、§1.2 "Phase I（合并 main）本次不做：全部 Phase 验收通过后，**由用户二次授权**方可执行合并"。废止声明：本组内无（它是被废止方）。
- **V1**：把 ACR 从"通用异构平台"收缩为"只为图像积分/Drizzle 两类热点提供 CPU+GPU 动态混合执行"（00_READ_FIRST §1）；禁止项 6 条（§4，含"不做CPU/GPU精确利用率控制""不做在线学习"）；对前代废止：manifest supersedes "all earlier AstroCS ACR control packages and plans 20-26"，00_READ_FIRST §5 末句"旧20—26号计划及旧通用画像要求已被本包完全取代"，'history/README.md' 全篇宣布旧 20—26 号计划"失去执行权威"、结论压缩进 08 计划——明确废止 acr 支线包 19–22 号计划谱系。
- **V2/V3**：各自针对一轮外部评审的整改包——V2/00 §3 列审计发现的 7 项硬门禁、V3/00 §3 列 10 项，均注明"完整依据见 audits/*"；V2 manifest supersedes "all earlier AstroCS ACR control packages and prior 08 plan"，V3 措辞改 "prior execution plans"。
- **V4**：从"纠错"转为"冻结"——manifest purpose "freeze ACR architecture before business code changes and add weighted integration synthetic mixed example"、business_code_changes_allowed=false；00_READ_FIRST §4 本轮禁止事项 6 条（含"禁止在ACR CPU worker内部嵌套完整OpenMP并行区"）；§5 给出当前基线（评审基线 AstroCS_Review_ACRFocusedV3_20260806、HEAD 610d7b6a、评审包 SHA）。"CPU/GPU精确利用率控制永久撤销"（00_READ_FIRST §3 末条）。
- 负责人裁决痕迹：本组包内无 OWNER_BINDINGS/OWNER_DECISIONS/FRONT_DESK_RULINGS 类文件（属 G13 包）；裁决语义以包内字段与 git 痕迹呈现——如 acr/manifest merge_to_main_allowed: false、V4/manifest 禁改业务代码、支线最终合并发生为 main 提交 198d69e0（2026-08-10，"merge(acr): add astrocompute-runtime as dormant acceleration base"）与 e8624036（同日 docs(memory) 记录）。

---

## 六、代际差异表（逐项 diff 实测）

**acr → V1**：规格 23 份→10 份全换轨（编号体系从"01_FROZEN_REQUIREMENTS…22_FIX_REVIEW"变为"01_SCOPE_AND_DECISIONS…09_GIT_AND_DELIVERY"）；文件 50→20 载荷；schemas 8→4、templates 7→1；V1 废止 20–26 号计划（见第五节）。

**V1 → V2**（容器 diff -rq）：文件 20→21 载荷（新增 'audits/ACR_FOCUSED_REVIEW.md'）；改动 12 件：00_READ_FIRST、00_AGENT_START_PROMPT、04、05、06、07、08、CHECKLIST、package_manifest、SHA256SUMS、templates/evidence_checklist；**01、02、03、09 四份逐字节不变**。06 标题改"数据驻留、分区合并与内存预算"。门禁语义变化：V2 把 V1 的"本轮必须完成"（00 §3）替换为"当前已保留能力 + 当前尚未完成（7 项硬门禁，引 audit）+ 本轮只做"三节（V2/00 §2–§4）；CHECKLIST 54→57 项。
**V2 → V3**：文件数不变（21）；audit 换文件（删 ACR_FOCUSED_REVIEW.md，增 ACR_FOCUSED_V2_REVIEW.md）；改动 13 件：00、03、04、05、06、07、08、CHECKLIST、manifest、operation_profile.schema.json、SHA256SUMS、templates、start_prompt；**01、02、09 不变**。V3/00 §3 列 10 项硬门禁（含收益阈值单位千倍错误、驻留死路、5% 速率门失效，逐条对应 audit 阻断项 1–10）；CHECKLIST 维持 57 项。
**V3 → V4**：结构性重排——9 份规格全部换名（01–08 无一同名对应，见 diff "Only in" 清单），删除 'audits/'、'history/' 与 V1–V3 全部 4 个 schemas，新增 2 个 weighted schemas 与 'examples/weighted_integration/' 6 件；文件 21→22 载荷；CHECKLIST 57→60 项并新增"架构冻结/加权积分样例"两节与 READY_FOR_BUSINESS_ADAPTER 结论项；执行计划从编号节改 A–G 字母节；manifest 字段换代（去掉 scope/supersedes，新增 purpose/baseline_head/baseline_review_sha256/business_code_changes_allowed/start_prompt/generated_utc）。
**SHA256SUMS 格式变化**：V1 为 19 行文本式（hash␠␠path），V2/V3 改为 JSON 清单（root/total_files/total_bytes/error_count/entries，V2 total_files=20、V3 total_files=20），V4 回到文本式 21 行；acr 为文本式 49 行。
**状态字面量变化**：四代聚焦包未改 coverage 状态机描述；V4 新增两个结论字面量 PERFORMANCE_NOT_QUALIFIED（06 §4）与 READY_FOR_BUSINESS_ADAPTER（06 §7、CHECKLIST 末项），V1–V3 无此二者。
**同名沿袭文件**：09_GIT_AND_DELIVERY 在 V1→V2→V3 逐字节不变（V2 SHA256SUMS 行 hash 6fcfda25… 三代相同），V4 重写为 08_GIT_AND_DELIVERY（新增超时建议值与"不允许用Evidence提交制造第二HEAD"条款）。

---

## 七、执行留痕三方核对

因本组五身份**无台账任务号**（第一节），'pack_commit_link.csv' 不覆盖它们；核对改用"包内计划文本 ↔ 仓库证据留痕 ↔ 提交消息"三方：

1. **一致（包内计划命中提交消息）**：
   - V1/09 §1 建议提交序列 5 条全部精确命中：0030c3a5（2026-08-05，同时入库 V1 包）、3a405960、fe98df3f、380aae32、78101ae0（均 2026-08-05，'git log --grep' 实测）。
   - 三件套 tasks.md A7 建议消息命中 f8d749e4（2026-08-02）。
   - V4/08 §2 五条建议：精确命中 2 条——ef2106cb（"acr: freeze focused mixed execution contracts"）、b78c58d3（"acr: add resident cuda weighted integration launcher"，均 2026-08-06）；近似命中 2 条——771fc80a（"add weighted integration cpu openmp and mixed benchmark"）、fd0e8531（"add weighted integration correctness and residency tests"）；**未命中 1 条**——"acr: finalize pre-business evidence" 在全部 1999 条提交中无匹配（计入"有账无据"）。
   - 审计引用的 HEAD 全部真实存在：V2/audits 包声明 HEAD f8cba99e、Evidence HEAD 1c2ed0f5；V3/audits 源码 HEAD e107061c、Evidence HEAD c82013ee；V4/manifest baseline_head 610d7b6a；三件套 spec base 8f50519（2026-07-31）——逐一 'git log -1' 验证命中。
   - 执行密度：2026-08-02 至 08-10 窗口内 acr scope 提交 128 条（'git log --all' 按 "(acr" scope 计数；全史 acr scope 130 条），日期分布 08-02:12、08-03:12、08-04:22、08-05:25、08-06:18、08-07:8、08-08:11、08-09:19、08-10:1，与四代包发布时点吻合。
2. **有据无账（存在证据留痕但包内无台账号可归）**：'工程控制/evidence/acr/' 曾入库（含 EVIDENCE_INDEX.md、sanitizer/*、path_guard/*、git/*、commit_f_fix/* 等路径实测于 ls-tree），12fb99f3 时 70 个文件，7c67328a（2026-08-04 "chore(acr): remove in-repo evidence (external single-head generation only)"）删净最后 19 个文件，HEAD 与容器均无此目录——即 ACR 的 evidence 留痕按包规则外置，仓库内**只余删除事件**，无法与任何任务号对账。数量：evidence 文件 70→19→0。
3. **有账无据（包内声明在仓库找不到对应实物）**：V1 supersedes 所称 "plans 20-26" 中 23–26 号计划文件全史无踪迹（'git log --all --name-only' 无 'tasks/acr/2[3-9]_' 路径，实测）；V4/08 §2 第 5 条建议提交消息未出现（上）；V2/V3/V4 引用的三个评审 zip 与被引用的 acr/manifest source_review_pack（AstroCS_ACR_CommitF_Review_2026-08-03(1).zip）均未入库（'git log --all --name-only' 无匹配，实测），其 SHA 无从复核。
分类小计：一致 = 12 项计划/HEAD 引用命中 + 128 条窗口内 acr scope 提交；有据无账 = evidence/acr 全部留痕（70 文件峰值）；有账无据 = 4 项（23–26 号计划缺档、3 个评审 zip 未入库、1 条建议提交未命中、manifest source_review_pack 未入库）。

---

## 八、缺口与不确定

1. **五身份均无 zip 原件与 zip sha256**：'pack_lineage_table.md' 形态列 'reco,work'；'工程控制/_control_packs/' 与 git 全史均无 ACR zip（实测）。包自述固定 zip 名与仓库目录形态不符，无法核对发行包字节。
2. **acr 两形态文件集合差异无共同基线可比**：复原件止于 49ea5c1e^（3 载荷文件），容器件为 12fb99f3 后 50 文件态；'history/49ea5c1e/…/_PROVENANCE.json' recovered_files=3 与容器 50 之差全部发生在 49ea5c1e→12fb99f3 之间（git 明细见第一节），复原件不能代表支线包最终态。
3. **acr 身份台账日期倒序**：'pack_inventory.csv' acr 行首次=12fb99f3(2026-08-04)、最后=49ea5c1e(2026-08-02)，而 main 上最早添加三件套的是 f8d749e4(2026-08-02)；first_commit 口径（疑按幸存文件计）未在该 CSV 中说明——脚本口径未证实，本报告以 git 实测（f8d749e4 最早、49ea5c1e 删 spec/tasks）为准。
4. **digest 与本体不一致（V2/V3 的 SHA256SUMS 核对）**：五份 digest 对 V2/V3 各实例报 ok:0, mismatch:0, missing:0，但包内 'SHA256SUMS.txt' 为 JSON 清单且自报 total_files=20、error_count=0（V2 实测）——digest 脚本未解析 JSON 格式，"0"不代表无条目。V4/V1/acr 为文本格式故可解析（ok=21/19/48）。
5. **acr 容器 SHA256SUMS missing:1 实因**：清单列 'CHECKLIST.md'（大写），盘上实为 'checklist.md'（小写）——大小写错位（实测 comm 对比，仅此一进一出），非文件丢失。V1 容器 SHA256SUMS ok=19 与清单 19 行一致。
6. **被审对象不可核**：V2/00、V3/00、V4/00 与 audits 所引评审包（AstroCS_Review_ACRFocused_20260805.zip、…V2_20260806.zip、AstroCS_Review_ACRFocusedV3_20260806）及 acr/manifest source_review_pack 均无 zip 实物（第七节 3），其 SHA-256（04735781…、e2b7125b…）无法复算。
7. **"plans 20-26"中 23–26 号计划**：仓库全史未见文件，V1 声明废止的对象本身不在库——被废止内容仅 20/21/22 三份可实读（容器 tasks/acr）。
8. **恢复实例删除提交字段**：'pack_inventory.csv' "删除提交"列对本组复原件留空、digest 'dels:[]'，但 git 显示 docs 原件被 e1604f14 删除（移档）且 tasks/acr 部分文件被 49ea5c1e 删除——台账未记录这两个删除事件；b7b2dea7 的 R100 归档移动在台账中仅体现为容器实例 first=last=b7b2dea7。

---

## 衔接段一：与上一组（G03）的衔接

G03 末包 '_agent_package'（digest '设计大纲/_evidence/packs/digest/_agent_package.md'：13 文件、仅历史复原件、2026-07-31）。ACR 五身份包内**未见任何对 Stage1 HISS / _agent_package 的引用或废止声明**：grep -ri 容器 tasks/acr 与 archive/ACR_FOCUSED_CONTROL_PACKAGE 对 'HISS_Delivery|agent_package|Stage1_HISS' 零命中（实测）；V1 manifest supersedes 仅覆盖"all earlier AstroCS ACR control packages and plans 20-26"，即本组 acr 支线包谱系。三件套 spec.md §1.3 与 V1 00_READ_FIRST §4 只把 "HISS" 列为禁改对象（算法名），非控制包继承关系。结论：G04 各包与 _agent_package **无直接继承/废止关系**；唯一可核对的时间衔接是 acr 支线 base commit 8f50519（2026-07-31，spec.md §2.2，实测存在）落在 G03 包活跃日期当天。依据文件：'_PROVENANCE.json'×5、V1/package_manifest.json、V1/00_READ_FIRST.md §5、V1/history/README.md、三件套 spec.md §1.3/§2.2、G03/G04 五份 digest。

## 衔接段二：重叠身份核对

'dispatch_groups.json' 全表核对：本组 5 身份（acr、ACR_FOCUSED_CONTROL_PACKAGE{,_V2,_V3,_V4}）**未出现在其他任何组的身份列表**——无重叠身份。相邻组仅共享"ACR"这一任务号前缀与算法名：G05/G06 的 CONTROL_V4 与 G10 的 V7 包、G05 的 REAUDIT_V3 三件套使用 ACR-001/002/003 任务号族（'pack_commit_link.csv' 相应行；REAUDIT_V3 任务号族列含 ACR），这些号在包内语境指 REAUDIT/V7 的 ACR 隔离任务，与本组四代聚焦包（无编号任务）不同谱系；本组包与其 digest（各自组 digest 文件）无同名实例交叉，不存在需要比对的 digest 分歧。
