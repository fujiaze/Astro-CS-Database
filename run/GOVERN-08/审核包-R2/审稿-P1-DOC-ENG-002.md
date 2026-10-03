# 审稿-P1 · DOC-ENG-002（G08-05 对抗审稿 第 1 遍）

> 基线：仓库 `/workspace/Astro CS Database`，HEAD=`f9650dd0`
> 审稿员：P1 车道 · DOC-ENG-002
> 纪律：全程只读。零 git 写（未 add/commit/checkout/reset/stash/git rm --cached）；未编译、未跑 ctest/pytest/构建/实验脚本；未修改任何仓内文件（交付件除外）；未读 `/tmp/acsd_g08/`。

---

## 1. 读完了吗

| 项 | 值 |
|---|---|
| 成员份数（权威片清单） | **43** |
| 实际读完份数 | **43** |
| 成员总行数（权威片清单「实际行数」） | **8211** |
| 实测总行数（`wc -l` 逐件复核） | **8211**（与权威清单零偏差） |
| 实际读到的行数 | **8211** |
| **覆盖率** | **43/43 份 = 100.0%；8211/8211 行 = 100.0%** |
| 未读完的部分 | **无。** 逐件读完，无抽样、无跳读、无只读开头。 |

**目标行数对照**：片清单 `目标行数 11000 / 实际行数 8211 / 超容量 false / 划片依据「SRS-1 层内 LPT 均衡装箱」`。本片实测与权威清单完全一致。

**读取方式说明（诚实登记）**：15 份较长文件用 `read` 工具分段读完（`PUBLIC_API.md` 2064 行走 7 段；`COMPRESSION_CODEC_RESEARCH_PACK.md` 681 行走 2 段）。其余 28 份较短文件用 `cat -n` 全文一次性呈现后逐行读毕——**是逐行读原文，不是脚本扫描替代阅读**（规范 04 §1 禁令针对的是「以脚本扫描替代阅读」，全文呈现后由我逐行判读不属该形态）。机器词表（grep/glob）**只用于产生候选与复核行锚**，所有结论均由我读到的正文推导。

**基线复核**：`git -c core.quotepath=false log --oneline -1` = `f9650dd0 清除运行期文案里从未存在于被引条款的伪引`，与任务书一致。工作树全程未改。

---

## 2. 本片判定

# **阻断（BLOCKING）**

**口径声明（按任务书第 9 条）**：本片问题以「**文件·条款级**」计数。下文凡出现「N 处」「M 条」均给出**计数层级**（同文件条款 / 跨文件同类 / 全片）。涉及门实例数量时，口径为「**条款实例**」（即文档中声明某门/某判据存在的条款条数），不是「去重门实例」，也不是「整改分母」——因为本片**没有任何一条被判定为真的门实例的整改分母**（详见 §7）。

### 最重的 3 条

**① PUBLIC_API.md 的「API 机器单源清单」其一致性判据已物理删除，且该引用是自指**（阻断）
- 位置：`docs/engineering/PUBLIC_API.md:219-220`
- 原文：`详见 \`docs/engineering/PUBLIC_API.md\`（API 机器单源清单，与 \`check_api_contracts\` 的 \`docs/engineering/PUBLIC_API.md\` 一致；完整分类清单）。`
- 两个独立缺陷：
  - **自指**：在本文件内部说「详见 docs/engineering/PUBLIC_API.md」——读者被指向他正在读的同一份文件，零信息增益。
  - **判据已死**：`check_api_contracts` 的源码 `eng/tools/quality/contracts/check_api_contracts.py` **未被 git 跟踪**（`git ls-files --error-unmatch` → 「未匹配任何 git 已知文件」）；它比对的目标 `docs/contracts/API_CONTRACTS.csv` 所在目录 `docs/contracts/` **不存在**；磁盘上只剩一个未跟踪的 `__pycache__/check_api_contracts.cpython-313.pyc`。
- 为何阻断：这份文件自称是 API 的**机器单源清单**，其一致性论据是一个源码已删、比对目标已删、只剩 .pyc 残骸的检查器。这正是本项目固化检查项里的「**退役治理：已退役对象是否真无活调用者**」与「**判据读不到真实对象**」的组合失效——文档把一个不可运行的判据当成现行权威引用。
- 证据：`git ls-files --error-unmatch eng/tools/quality/contracts/check_api_contracts.py`（失败）；`ls docs/contracts/`（No such file）；`ls eng/tools/quality/contracts/__pycache__/`（仅 .pyc）。
- 附带：**`eng/ci/` 整个目录不存在**，而 `eng/ci/workflow_binding.json` 在历史 CI 产物 `artifacts/ci/cbd1fb6da711/.../WORKFLOW-REGISTRY-BINDING.json` 里仍登记着该检查器 ⇒ 已退役门在 CI 绑定面残留登记。

**② PERF_GATE_CONTRACT.md 把 3 条「不改退出码」的判据写成「真判红」，并把 `record_and_justify` 的定义说反了——在正确实现下必然红**（阻断）
- 位置：`docs/engineering/PERF_GATE_CONTRACT.md:7`、`:16`、`:14`
- 原文：`:7`「L2 冻结判据（四条，全部为**真判红**）」；`:16`「**enforcement = fail-closed**：任一判据违规 ⇒ `verdict=red`；`record_and_justify` 只是无违规样本的**记录语义**，不参与裁决」。
- 唯一数值源（文档 `:17` 自认）`eng/contracts/resource_gate_v1.json` 实测：
  - `:44` `mean_utilization_enforcement: "record_and_justify"`
  - `:46` `p50_utilization_enforcement: "record_and_justify"`
  - `:49` `per_sample_enforcement: "record_and_justify"`
  - `:53` `queue_low_window_enforcement: "hard_fail"` ← **四条里只有这一条是硬失败**
  - `:58` 逐字：`record_and_justify = 必须记录 + 超标须登记，不改变退出码（85% 均值门在 16-worker 真负载上实测仅 65.09%，未标定前不得硬失败）。`
- **定义被说反**：合同说 `record_and_justify` 是「超标须登记」的**违规处理**语义；文档说它是「**无违规样本**的记录语义」。方向完全相反。
- **恒红门已成形**：把合同自记的实测值 65.09% 代入文档 `:11` 的判据「平均 CPU 利用率 ≥ 0.85 ⇒ red」，得 red。即**实现完全正确、跑满 16 worker 真负载时该门必红**。这正是任务书点名的恒红门形态（判据与被测量不同源，但阈值按合同自己的实测被证明不可达）。
- `:14` 判据 4 写「无就绪积压同样计违规」，与合同 `:54` `queue_low_window_requires_queued_work: true`、`:59` `queued_work_predicate` **方向相反**。
- **同源错误已扩散**：同一错误定义出现在本片另一份文档 `docs/engineering/VALIDATION_EVIDENCE_STANDARD.md:443`「『记录并给出理由』是无违规样本时的记录语义，不参与裁决」——错误不是孤点。
- 交叉印证（同片）：`VALIDATION_EVIDENCE_STANDARD.md:303` 写「资源门只管磁盘，内存、CPU 与线程不设阻断门」，`:242` 写阈值唯一数值源 = resource_gate_v1.json；`RESOURCE=10` 在 `lib/infrastructure/cli/exit_codes.h:15` 的注释亦写「一般性资源超限门已取消：内存/CPU/线程不设门」。**三份文档 + 一份合同 + 一份代码注释互相矛盾，PERF_GATE_CONTRACT 是唯一的异类。**

**③ FROZEN_GATE_INVENTORY.md 的全部机器证据面已随两次门禁删除而消失，盘点结论目前不可复核**（阻断）
- 位置：`docs/engineering/FROZEN_GATE_INVENTORY.md:6`、`:13`、`:24`、`:26`、`:61`、`:65`、`:67`
- 逐条实测（我亲自执行）：
  - `:6`「盘点表（机器可读）：`eng/tools/acceptance/frozen_gate_inventory.json`」→ **不存在**（`eng/tools/acceptance/` 只有 `fixtures/` 与 `rel790_checklist.json`）。
  - `:13`/`:24`/`:26` 以 `checks.json`（`eng/ci/checks.json`）为唯一证据面，「161 条」「`waivable=true`、前置缺失 rc=77」→ **`eng/ci/` 整个目录不存在**，不可复核。
  - `:61`「机器侧已由 `rel790_pack_check.py` RP-08 覆盖」→ **不存在**。
  - `:67`「实测：`FROZEN_GATE_EXIT_PASS: gates=7 无出口=2 …`」→ 该串**全仓只出现在这份文档自己**，无任何脚本产出它。
  - `:5`「机器判据：FG-01..FG-06」→ 除本行外全仓零命中（其余命中是不相关的 `CFG-011`/`CFG-012`）。
  - `:22`/`:37`/`:50`/`:65`/`:73` 的「门禁注册面（G08-10 重建）」→ **仓内无此文档**（这是一句任务代号被当文档名用）。
- 附带违反自家写法规范：`:7`「盘点时点 commit：`e9fa20d9`」——`DOCUMENT_GOVERNANCE.md:125` 明写「**基线 SHA 不出现在跟踪文档**」，`:152` 把「commit 与散列｜40 位十六进制独立 token」列为写法缺陷形态。**FROZEN_GATE_INVENTORY 被本仓自己的治理规范判红。**

---

## 3. 逐文件清单（43 件，每件给「读了什么 → 看到什么 → 判定」）

| # | 成员文件 | 读了什么 | 看到什么（`文件:行`） | 判定 |
|---|---|---|---|---|
| 1 | `PUBLIC_API.md` (2064) | 全文 7 段 | ① `:219-220` 自指 + `check_api_contracts` 已删；② `:291-311` API-HIPS-001 声明「九导出/9 个全部当前真实存在」+ 9 个行锚 `:104,121,…,177`，实测 `aio_hips.h` 有 **13** 个 `AIO_HIPS_EXPORT`（:157,174,183,211,240,287,291,300,316,321,324,336,378），4 个未登记且 9 个锚全指无关内容；③ `:224` 「六导出」vs `:232`「7 个」vs 实测 **8**（漏 `hp_drizzle_compute_auto_nside`:155）；④ 26 个口径条款块引用中 **13 个括号不平衡**（`:167/:224/:291/:363/:434/:533/:649/:751/:838/:956/:1203/:1272/:1394/:1617`），`:291`/`:838` 开头是半截词「他版；」，`:224`/`:956`「该头」悬空；⑤ 内部行数自相矛盾：`upm.h`「184 行」vs「453 行」、`p3_output.h`「64 行」vs「176 行」、`p3_resample.h`「58 行」vs「202 行」；⑥ `:84` toolchain.ps1 子命令 `run`/`review` 不存在；⑦ `:142-144`「`ac_set_num_threads` 调用面=空」与 3 个生产调用点矛盾 | **阻断** |
| 2 | `CRITERION_QUALITY.md` (457) | 全文 | §1 五条 + §2 八类 + §3 T1–T28 + §4 M1–M22 + §5 E1–E12 逐条读。T1–T28 路径全量核存在性：**27 命中、1 缺失**——`:282` T21 `eng/tests/quality/test_doc_machine_check.py` 仓内无（`.pyc` 残留）。规范自身质量高、双向/恒红/非退化覆盖完整 | **须修**（T21 悬空） |
| 3 | `VALIDATION_EVIDENCE_STANDARD.md` (561) | 全文 | §6 Q1–Q3、§8 S1–S7、§12 门禁分级覆盖本项目固化检查项。缺陷：`:443` 复述 PERF_GATE_CONTRACT 的**反向** `record_and_justify` 定义；`:290` 笔误「冻结结阈值」；`:370` 「可执行面重建后登记、可执行面重建后登记」重复 | **阻断**（因 :443 与 ② 同源） |
| 4 | `STANDARDS_REGISTRY.md` (400) | 全文 | `:3` 含历史叙事「本节号为本轮订正，原写 §5.3」→ 违反 `DOCUMENT_GOVERNANCE.md:121-126`；`:394` 引 `ACSD_DESIGN.md` §0.2（**§0.2 不存在**，只有 §0/§0.1）；C3/C4 自设「EVIDENCE 路径在仓库中实际存在」，但 `:198/:205/:207/:214` 四处引 `docs/detail/gaia_xpsd_client.md` **不存在**；`:354`「前 8 场景**恒退出 0**」= 退出码恒真（`CRITERION_QUALITY.md:334` M7「退出码与判据脱钩」）；`:5/:332/:346/:368/:371/:373/:387/:396/:399/:400` 门禁注册面 | **阻断**（:198 等）+ 须修（其余） |
| 5 | `TRACEABILITY_SPEC.md` (338) | 全文 | `:5` `base_main_sha=0d32c07d…`（40 hex）违反 `DOCUMENT_GOVERNANCE.md:125/:152`；`:272` 行内藏 HTML 注释 `<!-- 订正: 检查-跨文档冲突 绿12——authority 正本 PHASE2_UPM.md:23 … -->`（裁决注记 + 行锚 + 流水号，三重违反 `:60/:121-126/:154`）；`:14`/`:23`「八层/8 个必填层」vs §2 表实列 **9** 行 vs `:185`「九层」；`:125`「22 个 registry 生产模块」vs 实测 **25** vs `:140` `index_module_count=23` vs §9 表 **30** 行（四个数字互不相同）；`:282/:283` 标题仍带已退役名 `THREADING_MODEL`/`ERROR_MODEL`；`:336` SCI-REJ-001「7 种排异」vs 最高设计 `:353`「4 种」 | **须修** |
| 6 | `execution_options_contract.md` (325) | 全文 | `:25`/`:245` 引「设计 §1.4」（**§1.4 不存在**）；`:293` 引 `docs/KNOWN_LIMITATIONS.md`（不存在）；`:3` 上游 §8.1/§8.3/§9 全部核实存在 | 须修 |
| 7 | `DOCUMENT_GOVERNANCE.md` (237) | 全文 | 本片**权威治理正本**，质量最高。`:152` 的写法形态表、`:60` 锚纪律、`:121-126` 历史痕迹清零 = 本片多条缺陷的判据来源。自身缺陷：`:140` 豁免面列 `docs/research/**`、`docs/references/**` 两目录**不存在**；`:56` 引 `AGENTS.md` §8「查证流程」，而 §8 实为「提交纪律」 | 基准件 / 须修（:140/:56） |
| 8 | `EXECUTION_MODEL.md` (216) | 全文 | `:5`（两处）、`:34` 引「最高设计 §1.4」×3；`:140` 引 `eng/tools/quality/runtime_oracle.py`（不存在） | 须修 |
| 9 | `ARCH-001.md` (198) | 全文 | `:95`「档界与算法名的唯一正本 = `docs/detail/algorithms_phase2/12_rejection.md` §9」→ **文件不存在**，mosaic 排异档位表在仓内**无正本**；`:95` 末句「生产排异算法集为 none / percentile / winsorized / linear fit」与最高设计 `:353` 一致（此条正确）；`:60-64` 三阶段节点序与最高设计 §4.2/§5.2/§6.2 核实一致 | **须修**（:95 无正本） |
| 10 | `ISA_VARIANTS.md` (181) | 全文 | 位算术 24/928/992 与 `cpu_features.h` 逐位核对**正确**；`:62`「位定义唯一源 = cpu_features.h」是无条件表述，但 `cpu/CPU_001_CAPABILITY_PROBE.md:47-49` 冻结另一套 `ACS_CAP_FEAT_*`（AVX512F=1<<8 vs 32），跨族比较会静默出错；`:85-86` 声称 provider 族「本轮补入构建目标」，但 `BUILD_GRAPH.md` §2 零登记 | 建议 |
| 11 | `data/DATA-004_PRODUCT_PROVENANCE.md` (171) | 全文 | §1 六类 revision + §2 digest 公式 + §4 五门 + §6 验收映射，逐条与实现面/测试类名对得上；`:71` 写「40 hex 源码 commit」是**字段格式声明**而非文档内嵌 SHA，不违反 §7.2 | **通过** |
| 12 | `CLI_PROTOCOL_V1.md` (168) | 全文 | **本片质量最高的文件**。退出码 11 条 ↔ `exit_codes.h:8-18` 逐条对齐；命令树 ↔ `command_tree.h:89-113` 逐行；`:110` 的 20 列 ↔ `resource_recorder.h:289-293` 逐字；crop 三层齐备。`:8-9` 冻结「`benchmark cpu` 返回 rc=2」但代码 3 处（`parser.cpp:700`、`profile_store.cpp:300`、`cpu_routing.cpp:138`）仍把该命令当恢复手段推荐给用户 | 须修（唯一一条） |
| 13 | `MODULE_MAP.md` (162) | 全文（输出被截断于中段，已从 spill 文件补读尾部 137-162） | `:6-7` 自相矛盾：状态「现场计算…并落在 `docs/engineering/MODULE_MAP.md`」，但 `:5`「本文件的表格**不写状态字段**」且全文无状态列；`:53` 引 `docs/KNOWN_LIMITATIONS.md`（不存在）；`:93` 引 §1.4（不存在）；`:142`/`:149`/`:156` 引 3 个已删文档；§6 符号登记表的 headline 主锚对、二次行锚系统性失准 | 须修 |
| 14 | `RELEASE_STATUS.md` (162) | 全文 | `:158-162` 元信息块（`authoring_owner` / `base_main_sha` / `convergence_task` / `convergence_run` / `convergence_base_sha`）——`DOCUMENT_GOVERNANCE.md:153` 把「元信息块」列为写法形态；`:131` 引控制包 `02_GATES_AND_EXECUTION.md` / `03_AUDIT_PACKAGE_SPEC.md`（控制包已退场）；`:5`/`:95`/`:96`/`:140` 多处「原引「宪章 §X」已废止」= 历史叙事。**优点**：`:40-41`/`:112-114` 明确标注「本节不主张当前提交复跑」「未在本提交复跑的面一律不写 IMPLEMENTED/INSTALLED」——**这是本片最诚实的一段** | 须修 |
| 15 | `ERROR_HANDLING_STANDARD.md` (145) | 全文 | §7 的 11 码 ↔ `exit_codes.h:8-18` **逐名逐值一致**（我实测）；§4 ErrorDomain 八值 ↔ `contracts.h` 一致；`:101-107` 关于裸整数工具二进制不得反查本表的边界写得很清楚。缺陷：`:124`「由 `eng/tools/docs_machine_consistency.py`（`error_taxonomy_exit_codes`）执行校验」→ **该脚本仓内不存在且未被 git 跟踪** | 须修（:124 悬空判据） |
| 16 | `UNIFIED_OBJECTS.md` (137) | 全文 | §2 的 13 个对象表逐条核 schema 路径，**全部存在**；`:46` 声明「可否作权重列逐字照抄 UNIFIED_MODEL §2」；`:37`/`:105` 引 `docs/detail/algorithms_phase1/07_noise_snr.md`（不存在）；`:113-119` 五处引 `docs/detail/calibration.md`/`healpix_drizzle.md`/`phase2_samp.md`/`phase2_upm.md`/`gaia_xpsd_client.md`（**全部不存在**）；`:109`「MODULE_MAP 引用 22 个 DATA ID」、`:84`「49 条 PENDING_OWNER_SIGNOFF」为无出处数字 | 须修 |
| 17 | `PERFORMANCE_MODEL.md` (128) | 全文 | `:14` 引 `docs/detail/algorithms_phase2/11_upm.md`（不存在）；`:20`/`:127` 引 `docs/engineering/BASELINE.md`（存在）；**`:68` 代码块注释「帧内轴（正本 = THREADING_MODEL.md）」**——`THREADING_MODEL.md` 已删，且与**同文件 `:28`**「并行轴语义唯一正本 = `execution_options_contract.md`」**直接冲突** | 须修 |
| 18 | `BUILD_GRAPH.md` (127) | 全文 | `:11-16` **诚实登记**了生成器残留（「ARCHITECTURE §1 迁移冻结」字样来自 `gen_build_graph_doc.py:48/:49/:60/:62`，须改生成器常量后重跑）——这是本片少见的、主动登记未决缺陷的范例。`:7`/`:121`/`:127` 门禁注册面 | 基准件（诚实登记） |
| 19 | `data/DATA-003_PRODUCTION_ARTIFACT_STORE.md` (118) | 全文 | §2 接线结构、§3 四步写路径、§5 六行失败语义表、§7 manifest hash 可重算——与执行面 `production_store.py` 方法名逐条对得上；`:8` 声明「实现锚 = `ArtifactStore`/`Writer`/`StoreIO` 的公开方法名」，真实存在 | **通过** |
| 20 | `MANIFEST_VERIFY_V1.md` (109) | 全文 | §1/§2 配置与 manifest 形态；`:21` 罕见地诚实登记「CLI 侧 `validate_cpu_profile` 当前**无生产调用方**——消费链接线缺口已在死键台账登记」。缺陷：`:18` 引 `schemas/phase3_request_v1.schema.json` 的同类路径在 §2.2 引 `eng/contracts/schemas/hips_storage_form.schema.json`（存在）；§3 校验序 `:89` 与 `commands.cpp` 真实控制流分叉（子代理取证，待复核） | 须修 |
| 21 | `cpu/CPU_001_CAPABILITY_PROBE.md` (100) | 全文 | `:47-49` 位定义 13 个位值与 `:62` 组掩码逐位自洽；`:22-27` 探测序「CPUID→OSXSAVE→XGETBV→ABI/hash→self-test」逻辑正确；`:76-79` 称注入入口是「公开 API `acsd_cap_detect_v1`」但该函数是真机探测、无注入入口（子代理取证） | 须修 |
| 22 | `WINDOWS_BUILD_NODE.md` (93) | 全文 | Windows 构建节点说明；`:52` 检出路径写 `F:/Astro dev/Astro CS **Normalization** Database`，与本仓名 `Astro CS Database` 不一致（待确认是否改名未同步） | 建议 |
| 23 | `TOOLCHAIN_AGENT_HOST.md` (91) | 全文 | `:43` **诚实登记**「`eng/cmake/toolchain/verify_toolchain.py` 不存在」；但 `:5`/`:28`/`:51`/`:68` 的 `eng/ci/toolchain.lock.json`、`toolchain.policy.json`（甚至给出 SHA256）、`verify_toolchain.py` 全部悬空（`eng/ci/` 目录不存在）⇒ 「版本数据 100% 来自实测输出」不可复核 | 须修 |
| 24 | `TEST_MATRIX.md` (82) | 全文 | §2 容差三档（FP64 rtol=1e-12 / FP32 rtol=5e-6 / 归约 γ_n）写法正确，`:26` 的「绝对容差可满足性下限（≥1 ulp）」是有价值的自检；`:40`「MC 用例与实测读数见 `实验/healpix-polar`」按最高设计 `:606-609` UPM 属 P5=`实验/additive-sky-seamless`，疑错指（待核）；`:23` Higham §3.1 定义 3.1 与式 (3.1) **待联网核验** | 建议 |
| 25 | `DUAL_LINE_CONTRACT.md` (81) | 全文 | `:46-53` **主动登记**了 9 个已删名并指定后继正本——写得对。但 `:55`「`docs/engineering/` 与 `docs/engineering/` 留在表内：两处各有在位机器可读件（4 件 / 6 件）」**同一路径写两次却给两个计数**（疑似批量替换把 `contracts/`、`architecture/` 都改成了 `docs/engineering/`）；`:24`/`:40` 声称的 5 个旧目录与 `algorithms_phase1/2/3` 全部不存在 | 须修 |
| 26 | `FROZEN_GATE_INVENTORY.md` (75) | 全文 | 见 §2-③。**实测 75 行，权威片清单记 74 行**（偏差 1 行） | **阻断** |
| 27 | `PHASE3_API_V1.md` (73) | 全文 | 五函数生命周期与 `p3_session.h:16-28` 逐字一致；`:18` 引 `schemas/phase3_request_v1.schema.json` **仓内不存在**；`:71-73` 称 `eng/tests/api/test_p3_api.py` 是机器门，该测试读的是**不存在的 `docs/api/` 路径**且只做 `assertIn` 字符串包含 | **阻断**（与其门测试） |
| 28 | `VERSIONING.md` (67) | 全文 | §2.1 构建指纹合同设计得好（`build_source_digest` 唯一可判同一二进制、`source_sha` 不蕴含，`:39-40` 消费规则明确）；`:18` 引 `schemas/version.schema.json`（根 `schemas/` 不存在，真实在 `eng/contracts/schemas/`）；`:62` 扫描面含 `launch/`（根目录不存在）；`:4`/`:27`/`:44`/`:54`/`:60` 门禁注册面 | 须修 |
| 29 | `DATA_FLOW.md` (60) | 全文 | `:12` 引 `PIPELINE_BLOCK_CONTRACT.md`（存在）；`:40`/`:42` 引 `docs/detail/algorithms_phase2/{11_upm,12_rejection,13_integration}.md` **三个全不存在**；`:27` 引「最高设计 §4.4」承载「测光归一化在 `measure_flux` 同一步内施加」——实测 §4.4 = 「输出合同」，`grep 归一化` 在 `:270-278` **零命中**，该内容在 **§4.2**（节点流程）；`:60` 引 `TRACEABILITY_SPEC §10` 的「`DATA-*` 行」，而 §10 首列是 `requirement_id` | 须修 |
| 30 | `PHASE2_API_V1.md` (55) | 全文 | §2 逐函数并发五字段表与 `PUBLIC_API.md` 逐符号句在 **7 个符号的 threadsafe** 上互斥、**internal_parallel** 1 处、**取消点** 3 处（子代理逐条取证，量级 11 处，**待我复核**）；`:31`/`:47`「sampler=1（串行 reference）」与 `sampler.cpp` 实现 N-worker 冲突；`:55` 自称机器门但门读不存在的路径 | **阻断**（与其门测试） |
| 31 | `ISA_BIT_MANIP_VARIANTS.md` (51) | 全文 | 12 kernel 逐条审计 BMI2/POPCNT 适用性并给出 `NOT_APPLICABLE` 结论，理由（gather 型不受益、比较计数≠位计数）成立；`:3` 上游 §9 + `ISA_VARIANTS.md` 均存在 | **通过** |
| 32 | `PERF_GATE_CONTRACT.md` (49) | 全文 | 见 §2-② | **阻断** |
| 33 | `DOCUMENTATION_STANDARD.md` (47) | 全文 | `:3` 上游写「`ACSD_DESIGN.md` §8.4（模块与 ABI）」——实测 §8.4 = 「顶层结构」，「模块与 ABI」是 **§8.5**；`:5-6` 权威链转述把 `ACSD_DESIGN.md` 与 `docs/engineering/` 各写两遍、**丢掉 `docs/science/` 一级正本**与「代码」一级（被引 `ACSD_DESIGN.md:24` 逐字是「本文档；一级正本 `docs/science/` 与 `docs/engineering/`；二级细节 `docs/detail/`；代码」）；`:13` 引 `docs/KNOWN_LIMITATIONS.md`（不存在）；`:20` 引 `docs/**/v6/**`（不存在，真实在 `实验/engineering-evidence/v6/`）；`:35` 引 `docs_machine_consistency.py`（不存在） | 须修 |
| 34 | `04_ARTIFACTS.md` (40) | 全文 | §1 产物清单 + §5 发布候选门槛；`:11-12` 正确标注「Alpha 前无版本号，见最高设计 §12」；CHUNK 引用均为概念名未给悬空路径 | **通过** |
| 35 | `DEVELOPER_GUIDE.md` (38) | 全文 | 构建/测试入口准确；`:3` 引 `CODE_STANDARD.md`/`TEST_STANDARD.md`（存在）；`:37` 引 `docs/standards/`（不存在）；`:38` 把 `TRACEABILITY_SPEC.md §9` 称作「**机器真相**」，而该文件 `:16`/`:156-158` 明写「**本节无机器检查器**…追溯面不再有 JSON/CSV 矩阵文件，也不由机器读取」 | 须修 |
| 36 | `DEPENDENCY_RULES.md` (34) | 全文 | `:10`/`:12` 引「最高设计 §1.4 非目标」（**§1.4 不存在**，实测只有 §1.1/§1.2/§1.3；§1.3「非目标」在 `:79`，`:81` 逐字含「GPU 与 CPU/GPU 混合生产路由…生产不可达」——**内容对、条款号错**）；`:14` 引 §8.4（同 33 的错）；`:31`「MSYS2/MinGW 依赖面**禁止**」与 `CODE_STYLE.md:5`「MSYS2 MinGW64 g++ 16.1.0」**现行冲突** | 须修 |
| 37 | `CACHE_POLICY.md` (31) | 全文 | §缓存清单三行的四要素（容量/身份/失效/线程）齐全；`:13`「命中路径 O(1)」与 `spherical_overlap.h:354` 注释「原实现命中时对 deque 线性扫描」需核；`:5` 引 §1.4 + §8.4；`:23` 引 `docs/KNOWN_LIMITATIONS.md`（不存在） | 须修 |
| 38 | `OWNERSHIP_AND_LIFETIME.md` (24) | 全文 | `:9`「`aio_hips_reader` → `aio_hips_reader_close`」——子代理实测这两个符号**全仓只命中文档自身**（待我复核）；另两对 `p2_upm_build/p2_upm_close`、`aio_upm_open/aio_upm_close` 成立 | 须修（待复核） |
| 39 | `OPTIMIZATION.md` (17) | 全文 | 纯候选清单，明写「重写依据 = 实测 profile；速度增益只从实现面获取，精度档位保持不变」；无悬空引用 | **通过** |
| 40 | `CODE_STYLE.md` (13) | 全文 | `:5`「MSYS2 MinGW64 g++ 16.1.0」与 `DEPENDENCY_RULES.md:31`「MSYS2/MinGW 依赖面禁止」、`TOOLCHAIN_AGENT_HOST.md:40`「msys2_mingw=FORBIDDEN」**三处现行冲突** | 须修 |
| 41 | `cpu/README.md` (3) | 全文 | 极简且与目录内容匹配（`cpu/` = CPU_001 + CPU_003 + README）；`:3` 说「ISA 变体决策在 ISA_VARIANTS.md」未标「上级目录」 | 通过 |
| 42 | `io/README.md` (3) | 全文 | 极简且与目录内容匹配；点名的 `docs/science/IO_001`、`IO_002`、`DATA_SEMANTICS` 均存在 | **通过** |
| 43 | `data/DATA-003` 已计入 #19 | — | — | — |

> **合计**：通过 7 件（#11 #19 #18 基准 #31 #34 #39 #41 #42 中 DATA-004、DATA-003、ISA_BIT_MANIP、04_ARTIFACTS、OPTIMIZATION、两 README、cpu/README 共 7）、须修 30 件、阻断 6 件。

---

## 4. 发现清单

### 4.1 阻断（7 条）

| # | 位置 | 现状 | 应为 | 证据 |
|---|---|---|---|---|
| B-1 | `PUBLIC_API.md:219-220` | 自指 + 引用已删判据 `check_api_contracts` | 指向真实第二处，或撤下「与 check_api_contracts 一致」 | `git ls-files --error-unmatch eng/tools/quality/contracts/check_api_contracts.py` → 未匹配；`docs/contracts/` 不存在 |
| B-2 | `PERF_GATE_CONTRACT.md:7,14,16` | 声明四条全真判红 + `record_and_justify` 定义反了 | §1 改为「①②③ = record_and_justify（记录面，不改退出码），④ = hard_fail」；`:16` 改为合同原义「超标须登记」；`:14` 删「无就绪积压同样计违规」 | `resource_gate_v1.json:44,46,49,53,58`；同源错误在 `VALIDATION_EVIDENCE_STANDARD.md:443` |
| B-3 | `FROZEN_GATE_INVENTORY.md:5,6,13,24,26,61,65,67` + `:7` | 全部机器证据面已删；盘点结论不可复核；含 commit SHA 违反自家规范 | 撤下该文件或整篇重写为「旧门禁面已退役」并注明删除提交；删 `:7` 的 commit 行 | `ls eng/tools/acceptance/`；`ls eng/ci/` → No such file；`grep -rn FROZEN_GATE_EXIT_PASS` 仅命中本文档；`DOCUMENT_GOVERNANCE.md:125,152` |
| B-4 | `PUBLIC_API.md:224,232` + `:291,301,312` | drizzle「六/7/实际8」三重不一致；HiPS「9/实际13」且 9 个行锚全错、4 个导出未登记（含 `aio_hips_verify_product_set` 验证入口） | 按真实头重写两节的导出面与行锚 | `hp_drizzle_api.h:82,102,110,126,155,211,220,221`（8 个）；`aio_hips.h:157,174,183,211,240,287,291,300,316,321,324,336,378`（13 个）；`sed -n '104p' aio_hips.h` = `// leaf_order: 叶级 Norder L…` |
| B-5 | `PHASE2_API_V1.md:55` / `PHASE3_API_V1.md:73` 所称机器门 = `eng/tests/api/test_p2_api.py:6` / `test_p3_api.py:6` | 门读 `docs/api/PHASE{2,3}_API_V1.md`，该目录不存在 ⇒ setUpClass 抛 FileNotFoundError ⇒ 用例全 ERROR（既非绿也非红，等于无门）；即使路径修对，两个测试只做 `assertIn(字符串, 文档全文)`，从不打开头文件、从不调用代码 | 改路径至 `docs/engineering/`；并把字符串包含断言改为对**行为**的断言（合法/非法输入→错误码） | `ls docs/` 无 `api` 目录（子代理取证，**我未独立复核，见 §6-N2**） |
| B-6 | `PHASE2_API_V1.md:28-43` 与 `PUBLIC_API.md` 逐符号句 | 7 个符号 threadsafe 互斥 + internal_parallel 1 处 + 取消点 3 处（**11 处，跨 2 文件**）；两份都是 FROZEN 正本 | 负责人裁定谁为准（`PHASE2_API_V1.md:6` 的「签名权威=现存头文件」自证条款是决定性的） | 子代理 69312998 逐条取证，**我未独立复核，见 §6-N2** |
| B-7 | `CLI_PROTOCOL_V1.md:40` 与 `:95`（机器判据第 3 条） | 文档断言「码值与含义**只有一份**，以 `exit_codes.h` 为唯一源」并给出机器判据「**grep 无第二处数值表**」——而 `lib/include/acsd/core/contracts.h:43-47` 就是**第二张完整 11 码数值表**（`enum class ExitCode : uint8_t`，11 个字面量与 `exit_codes.h:8-18` 逐个相同），且它**自己的注释**写「唯一源; 此处只做映射表, **不重定义数值**」——注释与自身行为正面矛盾 | 二选一：(a) `contracts.h` 改为 `using`/引用 `exit_codes.h` 不重写字面量；(b) 把 `CLI_PROTOCOL_V1.md:95` 的判据改为「两处必须逐值一致」并把一致性写成可执行断言。**注意：`ERROR_HANDLING_STANDARD.md:4` 把 `contracts.h` 列为「机器事实源」之一，与 `exit_codes.h` 并列——两份工程正本对同一个头给了两个互斥的角色** | 我亲自跑 `grep -rn "enum \(class \)\?ExitCode" lib/` ⇒ 命中 `exit_codes.h:7` 与 `contracts.h:43` 两处；`sed -n '40,48p' contracts.h` 逐字取证；`sed -n '40p;95p' docs/engineering/CLI_PROTOCOL_V1.md` 取文档断言。**该判据一条 grep 即可证伪，这是本片最易复现的阻断项** |

### 4.2 须修（30 条，按本片成员归集）

| # | 位置 | 现状 → 应为 | 证据 |
|---|---|---|---|
| S-1 | `CRITERION_QUALITY.md:282` | T21 模板指向已删文件 → 改指现存等价模板或撤下该条 | `eng/tests/quality/` 无该 `.py`；`__pycache__/test_doc_machine_check.*.pyc` 残留（删除提交 `57abe9d8`） |
| S-2 | `STANDARDS_REGISTRY.md:198,205,207,214` | 四处 EVIDENCE 引 `docs/detail/gaia_xpsd_client.md`（不存在），违反其**自设** C3/C4 → 改指现存 `docs/detail/infrastructure/22_gaia_xpsd_client.md` | `ls docs/detail/` 无该文件 |
| S-3 | `STANDARDS_REGISTRY.md:3` | 含历史叙事「本节号为本轮订正，原写 §5.3」→ 按 `DOCUMENT_GOVERNANCE.md:123` 整段重写为现行口径 | `DOCUMENT_GOVERNANCE.md:121-126` |
| S-4 | `STANDARDS_REGISTRY.md:354` | 「前 8 场景**恒退出 0**」= 退出码恒真 → 退出码必须接到失败计数 | `CRITERION_QUALITY.md:334` M7；`:158`（PASS 时 exit 0 / FAIL 为 1）与 `:354` 冲突 |
| S-5 | `STANDARDS_REGISTRY.md:394` | 引 `ACSD_DESIGN.md` §0.2（不存在，只有 §0/§0.1）→ 改指 §0 或 §0.1 | `grep -nE '^#{2,3} 0\.' docs/ACSD_DESIGN.md` → 5, 33 |
| S-6 | `TRACEABILITY_SPEC.md:5` | `base_main_sha=` 40 位 hex → 按 `DOCUMENT_GOVERNANCE.md:125` 删除 | `:125`「基线 SHA 不出现在跟踪文档」；`:152` 形态表 |
| S-7 | `TRACEABILITY_SPEC.md:272` | 行内 HTML 裁决注释（含行锚 + 流水号「绿12」）→ 删除，正文只留现行口径 | `DOCUMENT_GOVERNANCE.md:60,121-126,154` |
| S-8 | `TRACEABILITY_SPEC.md:14,23` vs `:26-36` vs `:185` | 「八层/8 个必填层」vs 表实列 9 行 vs「九层」→ 统一为九层 | 我实测 §2 表 9 行 |
| S-9 | `TRACEABILITY_SPEC.md:125` / `:140` / §9 表 | 「22 个」vs 实测 registry `acsd.phase*.md` **25** 个 vs `index_module_count=23` vs 表 30 行 → 四值统一 | `ls docs/detail/registry/acsd.phase*.md \| wc -l` = 25；表行数实测 30 |
| S-10 | `TRACEABILITY_SPEC.md:282,283` | 标题带已退役名 `THREADING_MODEL`/`ERROR_MODEL` → 改用现行件名 | 同文件 `DUAL_LINE_CONTRACT.md:46-48` 已自认这两个名字「在仓内不存在」 |
| S-11 | `DATA_FLOW.md:40,42` / `PERFORMANCE_MODEL.md:14` / `ARCH-001.md:95` / `PUBLIC_API.md:1294` | 引 `docs/detail/algorithms_phase{1,2}/` 5 个文件（**全部不存在**）；`ARCH-001.md:95` 称其为「档界唯一正本」 ⇒ mosaic 排异档位表**仓内无正本** | `ls docs/detail/` 只有 anchors/infrastructure/registry 三目录 |
| S-12 | `DATA_FLOW.md:27` | 引「最高设计 §4.4」承载测光归一化 → §4.4 是「输出合同」，该内容在 **§4.2** | `sed -n '270,278p' docs/ACSD_DESIGN.md \| grep -c 归一化` = 0；`:218` = `### 4.2 节点流程` |
| S-13 | `PERFORMANCE_MODEL.md:68` | 代码块注释引已删 `THREADING_MODEL.md`，且与**同文件 `:28`** 冲突 → 改指 `execution_options_contract.md` | `docs/engineering/THREADING_MODEL.md` 不存在 |
| S-14 | `DEPENDENCY_RULES.md:10,12` / `EXECUTION_MODEL.md:5,34` / `CACHE_POLICY.md:5` / `execution_options_contract.md:25,245` / `MODULE_MAP.md:93`（**8 处，跨 5 文件**） | 引「最高设计 §1.4」，实测只有 §1.1/§1.2/§1.3 → 改指 **§1.3** | `grep -nE '^#{2,3} 1\.' docs/ACSD_DESIGN.md` → 43,47,53,79；`:81` 逐字含被引内容（**内容对、号错**） |
| S-15 | `DOCUMENTATION_STANDARD.md:3` / `PHASE2_API_V1.md:3` / `PHASE3_API_V1.md:3` / `DEPENDENCY_RULES.md:14` / `CACHE_POLICY.md:5`（**5 处，跨 5 文件**） | 引 §8.4 承载「模块与 ABI」→ §8.4 是「顶层结构」，应为 **§8.5** | `sed -n '483p;520p' docs/ACSD_DESIGN.md` = `### 8.4 顶层结构` / `### 8.5 模块与 ABI` |
| S-16 | `DOCUMENTATION_STANDARD.md:5-6` | 权威链转述**丢失 `docs/science/` 一级正本**、把两个路径各写两遍 → 按 `ACSD_DESIGN.md:24` 原句重写 | `docs/ACSD_DESIGN.md:24` 逐字 |
| S-17 | `ERROR_HANDLING_STANDARD.md:124` / `DOCUMENTATION_STANDARD.md:35` | 引 `eng/tools/docs_machine_consistency.py` 执行校验 → **脚本不存在且未被跟踪** | `git ls-files \| grep -c docs_machine_consistency` = 0；`ls` → No such file |
| S-18 | `PHASE3_API_V1.md:18` | 引 `schemas/phase3_request_v1.schema.json` → 仓内无 | `git ls-files 'eng/contracts/schemas/*.json'` 无 phase3/p3 名式 |
| S-19 | `VERSIONING.md:18` / `:62` | 引根 `schemas/version.schema.json`（不存在）与根 `launch/`（不存在）→ 改指 `eng/contracts/schemas/version.schema.json`，扫描面删 `launch/` | `ls schemas/` `ls launch/` → 均不存在 |
| S-20 | `MODULE_MAP.md:53` / `CACHE_POLICY.md:23` / `execution_options_contract.md:293` / `DOCUMENTATION_STANDARD.md:13` / `PERFORMANCE_MODEL.md:20`(BASELINE 存在,此项不适用)（**4 处**） | 引 `docs/KNOWN_LIMITATIONS.md` → 不存在；真实件是 `artifacts/evidence/known-limitations-ledger/LIMITATIONS.md`（由本片 `DOCUMENT_GOVERNANCE.md:29` 自己写明） | `git ls-files \| grep -i known_limit` → 零命中 |
| S-21 | `PUBLIC_API.md` 26 个口径条款块中的 **13 个** | 括号不平衡；`:291`/`:838` 开头为半截词「他版；」，`:224`/`:956`「该头」悬空，`:363`「其余」回指已丢 → 补回句首与配对括号 | 我写的括号配对扫描：26 块中 13 块 op≠cl |
| S-22 | `PUBLIC_API.md:1619` vs `:1622` / `:1766` vs `:1769` / `:1924` vs `:1928` | 同一头文件行数在同一文档内自相矛盾（upm.h 184/453、p3_output.h 64/176、p3_resample.h 58/202）→ 统一并与真实行数核对 | `wc -l` 三个头文件 |
| S-23 | `DEVELOPER_GUIDE.md:38` | 称 `TRACEABILITY_SPEC.md §9` 是「机器真相」→ 该文件 `:16`/`:156-158` 明写「本节无机器检查器…不由机器读取」 | 自相矛盾，同文件两侧 |
| S-24 | `DOCUMENT_GOVERNANCE.md:140` | 豁免面列 `docs/research/**`、`docs/references/**` → 两目录不存在 | `ls docs/` 无此二目录 |
| S-25 | `DOCUMENT_GOVERNANCE.md:56` | 引 `AGENTS.md` §8「查证流程」→ §8 是「提交纪律」，查证流程在 §3/§4 | `AGENTS.md:97` §8 标题 |
| S-26 | `DUAL_LINE_CONTRACT.md:55` | 同一路径写两次却给「4 件 / 6 件」两个计数 → 恢复为 `contracts/`、`architecture/` | `:24` 同一对计数 |
| S-27 | `DUAL_LINE_CONTRACT.md:24,40` | 声称 `ci/`、`owner/`、`plugins/`、`contracts/`、`architecture/` 五目录状态 + `algorithms_phase1/2/3` 三目录 → **全部不存在** | `ls docs/` 实有六项 |
| S-28 | `CODE_STYLE.md:5` vs `DEPENDENCY_RULES.md:31` vs `TOOLCHAIN_AGENT_HOST.md:40` | 三处对 MSYS2/MinGW 的现行口径互相冲突 → 统一到 preset 合同的 `FORBIDDEN` | 三文件逐行 |
| S-29 | `RELEASE_STATUS.md:158-162` / `:131` | 元信息块 5 键 + 控制包文件名 → 按 `DOCUMENT_GOVERNANCE.md:153` 删元信息块；控制包已退场须改指 | `:131`「参考控制包 02_GATES_AND_EXECUTION.md」 |
| S-30 | `OWNERSHIP_AND_LIFETIME.md:9` | `aio_hips_reader`/`aio_hips_reader_close` 两符号疑似均不存在 → 改指真实 close 对（**待复核，§6-N1**） | 子代理 a5f4b700 取证，我未独立复核 |

### 4.3 建议（6 条）

| # | 位置 | 内容 |
|---|---|---|
| A-1 | `TOOLCHAIN_AGENT_HOST.md:5,28,51,68` | 全部证据锚指向不存在的 `eng/ci/`（含一个 SHA256）；「100% 来自实测输出」不可复核 |
| A-2 | `ISA_VARIANTS.md:62` vs `cpu/CPU_001_CAPABILITY_PROBE.md:47-49` | 两张并存的能力位表，同名 `AVX512F` 分别是 32 与 256；「唯一源」是无条件表述，跨族比较会静默出错 |
| A-3 | `UNIFIED_OBJECTS.md:109`（22 个 DATA ID）、`:84`（49 条 PENDING_OWNER_SIGNOFF） | 无出处数字 |
| A-4 | `TEST_MATRIX.md:40` | 「MC 用例与实测读数见 `实验/healpix-polar`」，但 UPM 按最高设计属 P5=`实验/additive-sky-seamless`，疑错指 |
| A-5 | `TEST_MATRIX.md:23` | Higham 2nd ed. §3.1 定义 3.1 与式 (3.1) —— **待联网核验**（γ_n 界在该书的准确编号） |
| A-6 | `COMPRESSION_CODEC_RESEARCH_PACK.md:356` | 称「证据全文见 `evidence/IVOA_HiPS_tile_format_report.md`，600 行」，该路径不存在；真实件是 `docs/science/IVOA_HIPS_TILE_FORMAT_RESEARCH_PACK.md`（610 行）。该文件 §8 的 9 条「诚实边界」写得很好，是本片最好的自查范例 |

---

## 5. 你主动构造的反例

| # | 构造什么 | 期望推翻什么 | 是否推翻 |
|---|---|---|---|
| R-1 | 断言 `PUBLIC_API.md` 引用的「全链没有「权重模式」这一可选概念」是 HEAD 提交（伪引清理）遗留的伪引——去 `docs/ACSD_DESIGN.md` 核该句是否逐字存在、落在哪一节 | 推翻「这是残留伪引」 | **未推翻（我自我否决）**。`docs/ACSD_DESIGN.md:182` 逐字含该句，且 `:177` = `### 3.1 数据对象`、`:187` = `### 3.2`，**该句确在 §3.1 内**，引用成立。HEAD 提交修的是另一批短语（「aio 是文件级唯一 I/O 边界」「权重是纯信号与噪声之比的派生量」）。记此反例为**否决**，避免误报。 |
| R-2 | 取 `resource_gate_v1.json:58` 合同自记的实测值 65.09% 代入 `PERF_GATE_CONTRACT.md:11` 的判据「平均 CPU 利用率 ≥ 0.85 ⇒ red」 | 推翻「四条全部真判红、enforcement=fail-closed」 | **推翻成功**。实现完全正确 + 跑满 16 worker 真负载 ⇒ 该门必红。这是本片唯一的**恒红门**实例。 |
| R-3 | 把 `commands.cpp:941/:960` 的实参代入 `resource_recorder.h:368` 的利用率公式 `100·active/(active+runnable)` | 推翻 `PERF_GATE_CONTRACT.md:31` 的 worker_balance 公式 `/(n_workers)` | **推翻成功**（部分）。实参 `set_workers(x,x)` 使分母 = 2x ⇒ 结果恒 50.00，与负载无关；而文档 `:34` 自己明令「「(min+max)/2」式恒值算法一律判红」。文档公式与实现分母不同 + 生产量本身恒值，双重问题。 |
| R-4 | 逐条核 CRITERION_QUALITY §3 的 28 个模板路径 + STANDARDS_REGISTRY C3/C4 要求的 37 条证据路径 | 推翻「规范自设的路径可达性判据在本片内自洽」 | **推翻成功**。CRITERION_QUALITY 27/28（T21 缺）；STANDARDS_REGISTRY 36/37（`docs/detail/gaia_xpsd_client.md` 缺）。两份文档都**自设**了「路径必须实际存在」的机器判据，而它们自己都踩在这条上。 |
| R-5 | 对 PUBLIC_API.md 全文 26 个「口径条款」块引用做括号配对扫描 | 推翻「条款块是完整可解析的引用锚」 | **推翻成功**。13/26 不平衡；`:291`/`:838` 开头为半截词「他版；」，`:224`/`:956`「该头」悬空。 |
| R-6 | 用 `git ls-files --error-unmatch` + `ls` 逐个验证 FROZEN_GATE_INVENTORY 与 PUBLIC_API.md:219 的判据/清单是否可运行 | 推翻「盘点结论与 API 单源清单可复核」 | **推翻成功**。5 个目标全部 MISSING，`FROZEN_GATE_EXIT_PASS` 全仓零命中。 |
| R-7 | 核「文档写的条款号在目标文档里是否真实存在」——把本片全部 `§x.y` 引用对 `docs/ACSD_DESIGN.md` 的真实节标题表逐条比对 | 推翻「本片条款号引用基本正确」 | **部分推翻**。§1.4（不存在，被引 8 处）、§0.2（不存在，1 处）、§8.4 语义错（5 处）三类被推翻；其余（§12.1/§12.2/§12.4/§12.5/§13/§2.2/§3.1/§4.2/§4.4/§5.2/§5.3/§5.5/§6.2/§6.3/§7.1/§7.2/§8.1/§8.2/§8.3/§8.5/§9/§10/§11/§12/§13/附录 B）**逐条核实存在**。 |
| R-8 | 核「恒红门」在本片是否有实例（含任务书点名的「两量逐位相同时判 ≤1e-4」） | 推翻「本片有恒红门」 | **推翻成功（找到别的恒红）**：R-2 的 65.09% 是真恒红门；R-3 的 50.00 恒值是恒真型；`STANDARDS_REGISTRY.md:354`「前 8 场景恒退出 0」是退出码恒真。**但**：「两量逐位相同却判 ≤1e-4」这一具体形态在本片 43 份文档中**零命中**（本片全是文档不是测试代码），列为未证实。 |

---

## 6. 盲复算

**方法**：对 §4 中**判据级别最高、且前三轮审稿件最可能已触及**的 6 条，我先遮蔽既有判定、只按「位置 + 原文」独立取证，再打开原判定比对。

| 条目 | 盲复算独立取证 | 原判定 | 比对 |
|---|---|---|---|
| B-1（PUBLIC_API:219） | 只拿「PUBLIC_API.md:219 与 check_api_contracts 一致」这句去 `git ls-files` 找该检查器 → 未匹配；再找它要比对的 `API_CONTRACTS.csv` → `docs/contracts/` 不存在；再找 `eng/ci/workflow_binding.json`（注册面）→ 也不存在 | 阻断 | **一致**（自取证据同向） |
| B-2（PERF_GATE enforcement） | 只拿「四条全部真判红」去 `eng/contracts/resource_gate_v1.json` 核 enforcement → 3 条 `record_and_justify`、1 条 `hard_fail`；再核 `record_and_justify` 的定义原文 → :58「超标须登记，不改变退出码」；再把 :58 的实测 65.09% 代入 :11 判据 → 必红 | 阻断 | **一致**（且盲复算独立构造出同一恒红门） |
| B-3（FROZEN_GATE 证据面） | 只拿「盘点表 = frozen_gate_inventory.json」「唯一证据面 = checks.json」「实测 = FROZEN_GATE_EXIT_PASS…」三条去逐个 `ls`/`grep` → 三者全 MISSING / 零命中 | 阻断 | **一致** |
| S-14（§1.4 悬空） | 先列 `ACSD_DESIGN.md` 全部 §0/§1 标题 → 只有 §0、§0.1、§1、§1.1、§1.2、§1.3；再回到 §1.3 正文核被引句 → `:81` 逐字含「生产不可达」 | 须修（号错、内容对） | **一致**，且补正了子代理的定性：这不是伪引，是**条款号错、内容对** |
| S-15（§8.4 语义错） | 核 §8.4 与 §8.5 的标题 → §8.4「顶层结构」、§8.5「模块与 ABI」；再查同片 `PUBLIC_API.md:3` 引的是哪一节 → §8.5（**同一事实在同片两处引得不一样**） | 须修 | **一致**，并补一条新证据：同片内自相矛盾 |
| S-6（40-hex SHA） | 只拿 `TRACEABILITY_SPEC.md:5` 那一行去 `DOCUMENT_GOVERNANCE.md` 的写法形态表比对 → `:152`「commit 与散列｜40 位十六进制独立 token」+ `:125`「基线 SHA 不出现在跟踪文档」 | 须修 | **一致**，并补强：这是**本片用本仓治理正本判本片文档红**，属自证 |

**盲复算结论**：6/6 **一致**，未发现「旧判偏松」或「旧判偏严」的情形。

**但有一条我要显式记录「我可能偏松」**：B-5、B-6、S-30 三条我**没有独立复核**，只读了子代理的报告就采信了信源。理由与补救见下。

**未复核项（如实登记）**：
- **N1**：`OWNERSHIP_AND_LIFETIME.md:9` 的 `aio_hips_reader_close` 不存在（子代理 a5f4b700 取证）—— 我未独立执行 grep。
- **N2**：`eng/tests/api/test_p2_api.py:6`/`test_p3_api.py:6` 的 `docs/api/` 路径不存在（子代理 69312998 取证）—— 我未独立执行 `ls docs/`。
- **N3**：PHASE2_API_V1 与 PUBLIC_API 的 11 处互斥声明 —— 我未逐条比对，只采信报告。
- 按 `VALIDATION_EVIDENCE_STANDARD.md:215` 的两条准入（「机器复算证据」+「另一路代理的独立复核结论」），**B-5、B-6、S-30 三条目前只满足一条准入**，应标 **待证**，交前台补机器复算后再升 `确认`。

---

## 7. 子代理派发记录

**派发数**：**4 个独立任务**，实际启动 **8 个子代理实例**（调度器对每次派发各起 2 个实例，故为 8 个执行体、4 份独立任务书；任务书互不重叠）。

| # | 任务 | 覆盖范围 | 产出 |
|---|---|---|---|
| A | 核验「门/判据/criterion」声明与真实实现一致 | CRITERION_QUALITY、FROZEN_GATE_INVENTORY、PERF_GATE_CONTRACT + resource_gate/run_monitored/resource_monitor/resource_recorder/commands 侧 | 2 份完整报告（B1–B6 + 2 份补充与自我更正） |
| B | 核验 API/CLI/接口类文档与代码一致 | PUBLIC_API、CLI_PROTOCOL_V1、PHASE2_API_V1、PHASE3_API_V1 | 2 份（中期 + 终版，含 8 阻断 / 13 须修 / 5 建议 / 5 反例 / 4 否决） |
| C | 专查伪引与追溯链断裂（含**无引号裸从句形态**） | 19 份（PUBLIC_API/CRITERION/VALIDATION/STANDARDS/TRACEABILITY/DOCUMENT_GOVERNANCE/RELEASE_STATUS/MODULE_MAP/DEPENDENCY_RULES/DOCUMENTATION_STANDARD/ERROR_HANDLING/EXECUTION_MODEL/ARCH-001/TEST_MATRIX/BUILD_GRAPH/UNIFIED_OBJECTS/PERFORMANCE_MODEL/VERSIONING/DATA_FLOW） | 1 份中期 + 1 份补充 |
| D | 核验架构/数据/构建/产物/缓存/平台类文档 | 22 份（DATA-004/DATA-003/BUILD_GRAPH/CPU_001/cpu·io README/WINDOWS_BUILD_NODE/TOOLCHAIN_AGENT_HOST/MANIFEST_VERIFY/ISA_VARIANTS/ISA_BIT_MANIP/ARCH-001/DUAL_LINE_CONTRACT/CACHE_POLICY/OWNERSHIP/DEPENDENCY_RULES/04_ARTIFACTS/COMPRESSION_PACK/OPTIMIZATION/CODE_STYLE/DEVELOPER_GUIDE/execution_options_contract） | 1 份完整报告 |

### 逐条复核：采信 / 修正 / 否决

**采信并由我独立复核确认（8 条）**：
1. A 的 **T21 指向已删文件** —— 我**先于**子代理独立发现（我先跑的全量路径存在性检查），子代理独立复现。**采信**。
2. A 的 **FROZEN_GATE 证据面全删** —— 我亲自 `ls` 了 `eng/tools/acceptance/`、`eng/ci/`、grep 了 `FROZEN_GATE_EXIT_PASS`。**采信**。
3. A 的 **PERF_GATE enforcement 反了** —— 我亲自读了 `resource_gate_v1.json` 全文 97 行与 `PERF_GATE_CONTRACT.md` 全文 49 行。**采信**（并补一条子代理没提的**扩散证据**：`VALIDATION_EVIDENCE_STANDARD.md:443` 复述了同一个反向定义 —— 这是本片第二条文档的同源错误，子代理只报了 PERF_GATE 一处）。
4. B 的 **HiPS 9 vs 实际 13、drizzle 六/7/8 三重不一致** —— 我亲自 grep 了两个头的导出宏。**采信**（并补正子代理未点明的一点：文档 `:312` 的签名行锚 `:104-118,…,177` 与 `:291` 的九行锚**同源**，即整节锚全废，不只是计数问题）。
5. C 的 **§1.4 不存在** —— 我亲自跑了 `grep -nE '^#{2,3} (0|1)\.'`。**采信并修正定性**：C 说「悬空条款号」，我补上「被引句在 §1.3 `:81` 逐字存在 ⇒ **内容对、号错**」，这个区分对整改成本影响很大。
6. C 的 **§8.4 vs §8.5** —— 我亲自 `sed -n '483p;520p'`。**采信并补充新证据**：同片 `PUBLIC_API.md:3` 引的是 §8.5 ⇒ 同片内自相矛盾。
7. C 的 **`docs_machine_consistency.py` 不存在** —— 我亲自跑了 `git ls-files | grep -c`。**采信**。
8. D 的 **COMPRESSION §9 的 18 个复现脚本逐一相符 + §8 的 9 条诚实边界** —— 我读了该文件全文 681 行并独立确认这两点。**采信为正面结论**（这是本片质量最高的自查段之一）。

**否决 / 修正（5 条）**：

| # | 子代理原判 | 我的复核 | 处置 |
|---|---|---|---|
| V-1 | A：「`全链没有「权重模式」这一可选概念` 是残留伪引」方向的暗示 | 我按 R-1 独立核 `ACSD_DESIGN.md:182` ⇒ 逐字存在且确在 §3.1 内 | **否决**。我把这条从怀疑降级为「引用成立」，并写入 §5-R-1 作为**自我否决的记录** |
| V-2 | C：「DOCUMENTATION_STANDARD.md:3 的 §8.4 是 C3-2『括注标题与节标题语义相符但字面不一致』」 | 复核后确认：**不只是字面不一致，是节号整体错位**（模块与 ABI 在 §8.5，差一级），且**同片另一处引对了** | **修正并加强**：从「措辞瑕疵」升为「条款号错 + 同片自相矛盾」 |
| V-3 | D：「BUILD_GRAPH §1 漏 3 个 target」 | 复核：文档 `:11-16` **已主动登记**「生成器残留（待裁决）」并点名 `gen_build_graph_doc.py:48/:49/:60/:62` | **部分否决**：D 的「漏登记」结论我保留，但**文档自己已如实登记未决状态**，不能按「C3-3 自述与事实相反」定罪。改为「须修：按生成器重跑」，不列阻断 |
| V-4 | B：「PUBLIC_API.md:219 自指 + 机器单源清单无消费者」 | 复核：我确认自指成立；但「无消费者」我未独立验证（只读到 `eng/tools/` 无文件读它） | **采信自指部分**，把「无消费者」降为待证 |
| V-5 | A 自我更正 N5（撤回对 `FAILCLOSED_SURVEY.md` 的「选择性报告」指控） | 该更正**我独立认同**：A/B/C 三列是注入故障探针列，「结论=通过」指探针如期失败，是元测试结果 | **采信该自我更正**，并把它记为本项目「红队自我纠错」的正例 |

**合计**：采信 8 条、修正 2 条、部分否决 1 条、否决 1 条、待证 1 条。

---

## 8. 自证段（可复跑命令）

前置：`cd "/workspace/Astro CS Database"`，全部命令**只读**。中文路径一律 `git -c core.quotepath=false`。

```bash
# ── C0 覆盖率自证：成员清单 = 43 份、8211 行，逐件存在性 ────────────────
python3 - <<'PY'
import re,subprocess
p="run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml"
lines=open(p,encoding="utf-8").read().split("\n")
i=lines.index("  - 片号: DOC-ENG-002")
blk=[]
for l in lines[i:i+60]:
    m=re.search(r'"(docs/[^"]+)"',l)
    if m: blk.append(m.group(1))
    elif blk and "片号" in l: break
tot=0
for f in blk:
    import os
    n=sum(1 for _ in open(f,encoding="utf-8")) if os.path.exists(f) else -1
    tot+=max(n,0)
    print(f"{n:6d}  {f}")
print(f"份数={len(blk)}  总行数={tot}")
PY
# 期望：份数=43 总行数=8211

# ── C1 B-1 判据已删 + 自指 ──────────────────────────────────────────
sed -n '219,220p' docs/engineering/PUBLIC_API.md
git -c core.quotepath=false ls-files --error-unmatch eng/tools/quality/contracts/check_api_contracts.py; echo "rc=$?"   # 期望 rc=1（未跟踪）
ls docs/contracts/ ; echo "rc=$?"                                                                          # 期望 rc=2（不存在）
ls eng/ci/ ; echo "rc=$?"                                                                                # 期望 rc=2

# ── C2 B-2 恒红门（enforcement 语义被说反）─────────────────────────
sed -n '7p;11p;14p;16p' docs/engineering/PERF_GATE_CONTRACT.md
grep -n '"_enforcement"' eng/contracts/resource_gate_v1.json      # 期望 44/46/49=record_and_justify, 53=hard_fail
sed -n '58p' eng/contracts/resource_gate_v1.json                    # 期望含「65.09%」「未标定前不得硬失败」
sed -n '443p' docs/engineering/VALIDATION_EVIDENCE_STANDARD.md     # 同源错误扩散面

# ── C3 B-3 FROZEN_GATE 证据面已删 ───────────────────────────────────
for p in eng/tools/acceptance/frozen_gate_inventory.json eng/ci/checks.json eng/tools/acceptance/rel790_pack_check.py; do
  [ -e "$p" ] && echo "OK $p" || echo "MISSING $p"; done
git -c core.quotepath=false grep -n "FROZEN_GATE_EXIT_PASS" -- docs lib eng | cat
grep -n "盘点时点 commit" docs/engineering/FROZEN_GATE_INVENTORY.md
grep -n "基线 SHA 不出现在跟踪文档" docs/engineering/DOCUMENT_GOVERNANCE.md   # 规范自证：§7.2 :152 形态表

# ── C4 B-4 导出面计数（我用过的原始命令）────────────────────────────
grep -n "AIO_HIPS_EXPORT" lib/infrastructure/aio/include/aio_hips.h
sed -n '104p;121p;130p' lib/infrastructure/aio/include/aio_hips.h   # 文档声称的锚 → 全是注释/形参行
grep -n "HP_DRIZZLE_API" lib/algorithms/drizzle/healpix_drizzle/hp_drizzle_api.h | grep -v define
grep -n "六导出\|九导出\|导出符号（7 个\|导出符号（9 个" docs/engineering/PUBLIC_API.md

# ── C5 S-1 T21 模板指向已删文件 ────────────────────────────────────
sed -n '282p' docs/engineering/CRITERION_QUALITY.md
ls eng/tests/quality/test_doc_machine_check.py ; echo "rc=$?"
ls eng/tests/quality/__pycache__/ | grep doc_machine

# ── C6 S-2 STANDARDS_REGISTRY 自设判据被自己违反 ─────────────────────
grep -n "gaia_xpsd_client.md" docs/engineering/STANDARDS_REGISTRY.md
ls docs/detail/gaia_xpsd_client.md ; echo "rc=$?"
sed -n '323p' docs/engineering/STANDARDS_REGISTRY.md   # C3 的「EVIDENCE 路径在仓库中实际存在」
sed -n '354p' docs/engineering/STANDARDS_REGISTRY.md   # 「前 8 场景恒退出 0」= 退出码恒真

# ── C7 S-6/S-8/S-9 TRACEABILITY_SPEC 三项自证 ──────────────────────
sed -n '5p;14p;23p;125p;140p;185p;272p' docs/engineering/TRACEABILITY_SPEC.md
sed -n '26,36p' docs/engineering/TRACEABILITY_SPEC.md | grep -c '^|'          # 期望 9（八层/九层矛盾）
ls docs/detail/registry/acsd.phase*.md | wc -l                                  # 期望 25（文档说 22）
sed -n '192,221p' docs/engineering/TRACEABILITY_SPEC.md | grep -c '^| MOD-'    # 期望 30
sed -n '152p' docs/engineering/DOCUMENT_GOVERNANCE.md                          # 形态表：commit 与散列

# ── C8 S-14/S-15 条款号悬空（一次跑全）─────────────────────────────
grep -nE '^#{2,3} (0|1|8)\.' docs/ACSD_DESIGN.md
sed -n '79,81p;177p;187p;483p;520p' docs/ACSD_DESIGN.md
git -c core.quotepath=false grep -n '§1\.4' -- docs/engineering | head -20
git -c core.quotepath=false grep -n '§0\.2' -- docs/engineering | head

# ── C9 S-11/S-13 目录消失导致的悬空正本 ────────────────────────────
ls docs/detail/
git -c core.quotepath=false grep -n 'docs/detail/algorithms_phase' -- docs/engineering
git -c core.quotepath=false grep -n 'THREADING_MODEL' -- docs/engineering lib | head
sed -n '28p;68p' docs/engineering/PERFORMANCE_MODEL.md   # :68 与 :28 自相矛盾

# ── C10 R-1 自我否决的反例（证明我没误报）──────────────────────────
git -c core.quotepath=false grep -n "全链没有" -- docs/ACSD_DESIGN.md docs/engineering/PUBLIC_API.md
sed -n '177p;182p;187p' docs/ACSD_DESIGN.md    # 期望：:177=§3.1, :182=该句逐字, :187=§3.2 ⇒ 引用成立

# ── C11 S-21 口径条款块括号配对（我写的扫描脚本的最小复现）──────────
python3 - <<'PY'
lines=open("docs/engineering/PUBLIC_API.md",encoding="utf-8").read().split("\n")
i=0;n=0;bad=0
while i<len(lines):
    if lines[i].startswith("> "):
        j=i
        while j<len(lines) and lines[j].startswith("> "): j+=1
        t="".join(x[2:] for x in lines[i:j]); n+=1
        if t.count("（")!=t.count("）"):
            bad+=1; print(f"不平衡 行{i+1}-{j}  ({t.count('（')} vs {t.count('）')})  {lines[i][:44]}")
        i=j
    else: i+=1
print(f"块总数={n} 不平衡={bad}")     # 期望 26 / 13
PY

# ── C12 全片「门禁注册面」占位串规模（69 处，跨 17 文件）─────────────
git -c core.quotepath=false grep -c "门禁注册面" -- docs/engineering | grep -v ':0'
```

---

## 9. 结论与建议

**本片判定 = 阻断。** 理由不是「问题多」，而是三条**已定级、可复算、且都属本项目血买来的失效型**：

1. **判据读不到真实对象**（B-1、B-3、B-5）：三份文档把「机器判据/机器单源清单/机器门」当作现行权威引用，而它们的载体（`check_api_contracts` / `frozen_gate_inventory.json` + `eng/ci/checks.json` / `docs/api/` 路径）**全部已被物理删除**。
2. **恒红门**（B-2）：`PERF_GATE_CONTRACT.md` 写出的四条「真判红」里三条按其自认的数值源根本不会红；**并且它把 `record_and_justify` 的定义说反了**，按文档读会以为 85% 均值门超标能阻断，实际不能。同一个反向定义已扩散到 `VALIDATION_EVIDENCE_STANDARD.md:443`。
3. **C ABI 归档面与真实头脱节**（B-4）：PUBLIC_API.md 的 HiPS 节漏登 4 个导出（含**验证入口** `aio_hips_verify_product_set`），drizzle 节漏 1 个，且两节的全部行锚指向注释与形参行。这是「代码改了、归档没重跑」的标准形态，且归档自称是**机器单源清单**。

**建议处置顺序（不静默覆盖红灯）**：
- **第一步（裁定，非施工）**：`PHASE2_API_V1.md:6` 的「签名权威=现存头文件」自证条款 vs `PUBLIC_API.md` 的逐符号句，11 处互斥谁为准。这是**范围裁定**，不裁定则 7 个 `p2_*` 符号的冻结口径无法判定（B-6）。
- **第二步（最便宜的高价值）**：`eng/tests/api/test_{p2,p3}_api.py:6` 两行路径改到 `docs/engineering/`。但**只改路径不够**——必须同时把 `assertIn(字符串, 文档全文)` 改成对行为的断言，否则只是从「报错」变成「空转」，落进 CRITERION_QUALITY §2.5「判据读不到真实对象」。
- **第三步**：`PERF_GATE_CONTRACT.md §1` 按 `resource_gate_v1.json:44,46,49,53,58` 改写，并同步订正 `VALIDATION_EVIDENCE_STANDARD.md:443`。**只改一处会留下两处矛盾。**
- **第四步**：`FROZEN_GATE_INVENTORY.md` 与 `PUBLIC_API.md:219` 二选一：要么重建载体，要么撤下声明并标注删除提交。**在载体重建前，这两份文档不得以「机器单源」身份被引用。**
- **第五步（批量，低风险）**：悬空条款号三批（§1.4 ×8、§8.4 ×5、§0.2 ×1）与悬空路径四批（`docs/detail/algorithms_phase*` ×5、`docs/KNOWN_LIMITATIONS.md` ×4、`docs_machine_consistency.py` ×2、`eng/ci/**`）。这些是单向文档订正，不触碰代码。
- **第六步**：本片 6 条文档用**本仓自己的治理正本**判红（`DOCUMENT_GOVERNANCE.md:125/:152` 判 TRACEABILITY_SPEC 与 FROZEN_GATE_INVENTORY；`:121-126` 判 STANDARDS_REGISTRY 与 TRACEABILITY_SPEC:272；`:153` 判 RELEASE_STATUS）。建议把「文档域人读核对」优先跑在 `docs/engineering/` 全域，而不是逐篇随机抽样——本片 43 篇里 **30 篇有须修级问题**，命中率 70%，说明这一域是系统性欠收敛而非个别失守。

**移交 UNRESOLVED（三条，均因我未独立复核）**：
- B-5、B-6、S-30 三条按 `VALIDATION_EVIDENCE_STANDARD.md:215` 的两准入只满足一条（缺机器复算），标 **待证**，交前台补跑后升 `确认`。
- `FROZEN_GATE_INVENTORY.md:13`「`ctest -N` 实测 585 个」——我被禁止跑 ctest，**未能独立复核**（旁证：子代理实测 `lib/`+`eng/` 的 `add_test(NAME …)` 为 400 处，差额来源不明）。该数字现已不可复核（`eng/ci/` 已删）。

---

*本交付件为审稿人产出，未修改任何仓内文件（除本文件外），未执行任何 git 写、编译、测试或实验脚本。*
