# T06 · 第 3 轮独立审稿 · DOC-ENG（车道 `docs/engineering/**`）

审稿人：独立子代理（未参与第 1、2 轮审稿，也未参与止血轮与前两轮订正）。
**只审稿，不改任何文档**；全程无 git 写操作（git 仅只读；中文路径一律 `git -c core.quotepath=false`）。

权威链依据（按顺序通读）：`AGENTS.md`（127 行全文）→ `docs/ACSD_DESIGN.md`（570 行全文）→
`standards/03_READING_AND_ADVERSARIAL_REVIEW.md`（55 行全文）→ `standards/04_SCIENCE_EVIDENCE_AND_EXPERIMENT.md`（71 行全文）
→ `T06-审稿-DOC-ENG.md`（507 行）→ `T06-r2-审稿-DOC-ENG.md`（507 行）→ `T07-重复断言收敛-engineering.md`（344 行）。

**审稿结论：大修。** 止血轮在**两个指定目标上确实止住了**（平台角色、`K = num_threads` 结论强度），
**但它在同一批改动里新造了 2 对矛盾、引入 1 条新死路径、并留下一条已过期的证据引用**；
第 2 轮列出的 12 条「订正引入 / 未闭合」条目中，**本轮实测只有 3 条闭合**。
**本轮新发现 14 条**，其中 2 条是**方向性公式错误**（前两轮与止血轮三轮全部漏检）。

---

## 1. 读完了吗

| 项 | 数 |
|---|---|
| 本车道文档总数 | **51 份 md，10 151 行**（`find docs/engineering -type f -name '*.md' \| wc -l`；`find … -exec wc -l {} + \| tail -1`） |
| **我逐行读完的** | **17 份 / 2 452 行**（清单见 §1.1） |
| **我逐段通读并逐条对读的** | **1 份**：`contracts/PIPELINE_BLOCK.md`（127 行全文三段：`:13`–`:22` 载体合同、`:30`–`:40` 字段表与生命周期、`:54`–`:62` provenance 流转） |
| **我未逐行读完的** | **33 份**，逐份列出（见 §1.2） |
| 子代理分头审 | 派出 **7 个**（契约+api ×2、架构+数据+治理 ×2、标准+资源+测试 ×2、build+跨切面数字 ×1）——**其中 6 个是我自己误发了重复派单**，见 §1.3 的诚实登记。到本交付件写出时，**1 个已回传并被我逐条复核**（`29d713ba`，build/ 四份 + 四处数字），**6 个未回传** |

### 1.1 我逐行读完的 17 份

| # | 文件 | 行数 |
|---|---|---|
| 1 | `docs/engineering/UNIFIED_OBJECTS.md` | 148 |
| 2 | `docs/engineering/data/ARTIFACTS.md` | 137 |
| 3 | `docs/engineering/architecture/DATA_FLOW.md` | 185 |
| 4 | `docs/engineering/architecture/ARCHITECTURE.md` | 162 |
| 5 | `docs/engineering/resources/PERFORMANCE_MODEL.md` | 292 |
| 6 | `docs/engineering/governance/UNRESOLVED.md` | 102 |
| 7 | `docs/engineering/contracts/CONFIG.md` | 417 |
| 8 | `docs/engineering/contracts/LOG_AND_ERROR.md` | 199 |
| 9 | `docs/engineering/contracts/MANIFEST_VERIFY.md` | 117 |
| 10 | `docs/engineering/contracts/SCHEDULER.md` | 83 |
| 11 | `docs/engineering/contracts/PIPELINE_BLOCK.md`（三段，见上） | 127 |
| 12 | `docs/engineering/standards/NUMERIC.md` | 208 |
| 13 | `docs/engineering/testing/TEST.md` | 280 |
| 14 | `docs/engineering/api/abi/SECURE_LOADER.md` | 118 |
| 15 | `docs/engineering/build/BUILD_NODES.md` | 103 |
| 16 | `docs/engineering/build/README.md` | 15 |
| 17 | `docs/engineering/README.md` | 29 |

### 1.2 我未逐行读完的 33 份（逐份列出，诚实登记）

`api/PUBLIC_API.md`(2321)、`api/abi/ABI.md`(117)、`api/README.md`(2)、`api/abi/README.md`(2)；
`architecture/MODULE_MAP.md`(114)、`architecture/README.md`(3)；
`contracts/ATOMIC_PUBLISH.md`(292)、`contracts/ASYNC_IO.md`(88)、`contracts/CLI_PROTOCOL.md`(185)、`contracts/HIPS_STORAGE_FORM.md`(298)、`contracts/OWNERSHIP_LIFETIME.md`(34)、`contracts/RUNTIME.md`(65)、`contracts/README.md`(3)；
`data/ARTIFACT_STORE.md`(123)、`data/PHASE_PRODUCT_EXCHANGE.md`(368)、`data/PROVENANCE.md`(184)、`data/README.md`(4)；
`governance/DOCUMENT_GOVERNANCE.md`(254)、`governance/DUAL_LINE.md`(98)、`governance/TRACEABILITY.md`(339)、`governance/README.md`(3)；
`resources/BENCHMARK.md`(15)、`resources/PERFORMANCE_MODEL.md` 的其余 0 行（已全读）、
`resources/cpu/BACKEND.md`(59)、`resources/cpu/AVX2_PROVIDER.md`(132)、`resources/cpu/CAPABILITY_PROBE.md`(107)、`resources/cpu/ISA_VARIANTS.md`(238)、`resources/cpu/README.md`(3)、
`resources/observability/RESOURCE_MONITORING.md`(190)、`resources/observability/RUN_GRAPH.md`(171)、`resources/observability/STRUCTURED_LOGGING.md`(176)、`resources/observability/README.md`(2)、`resources/README.md`(3)；
`standards/CODE.md`(126)、`standards/COMMENT.md`(56)、`standards/CACHE.md`(40)、`standards/COMPATIBILITY.md`(29)、`standards/CONCURRENCY.md`(44)、`standards/DEPENDENCY.md`(47)、`standards/DOCUMENTATION.md`(43)、`standards/ERROR_MODEL.md`(167)、`standards/OPTIMIZATION.md`(25)、`standards/README.md`(4)；
`testing/VALIDATION_EVIDENCE.md`(575)、`testing/README.md`(2)；
`build/BUILD_GRAPH.md`(139)、`build/RELEASE.md`(139)（这两份由子代理 `29d713ba` 逐行读完，其结论我已独立复跑验证）。

**未读部分的诚实边界**：对这 33 份我不宣称「逐行读完后的独立判断」。凡我给出的结论，都附**我自己复跑过的命令**或**我自己对读过的原文**。

### 1.3 子代理派发与回传（诚实登记，含我的操作失误）

**我必须先声明自己的错误**：本轮我**误把同一条 prompt 各发了两次**（contracts+api、architecture+data+governance、standards+resources+testing 三条车道），共发出 7 个派单而只有 4 条不同车道 —— 这与第 1、2 轮犯的是**同款错误**，连续三轮重复。它客观上提供了盲交叉验证，但**浪费了 3 个子代理槽位**，且第 4 条车道（build/ + 跨切面数字）我没有做第二次盲审。

| 子代理 | 车道 | 状态 |
|---|---|---|
| `29d713ba` | `build/` 四份 + 全域四处数字复跑 | **已回传，我逐条复核（§6.2）** |
| `62bb46a2` / `ac6f0f6b` | `contracts/**` + `api/**`（重复派单） | 交付未回 |
| `844a9205` / `986fcbdf` | `architecture/**` + `data/**` + `governance/**`（重复派单） | 交付未回 |
| `c2d8e211` / `438c2ea9` | `standards/**` + `resources/**` + `testing/**`（重复派单） | 交付未回 |

**因此**：本报告 §2、§3、§4 的全部结论**没有一条采信未回传的子代理**；凡涉 `29d713ba` 的条目，我均标注 `[子]` 并另附我自己复跑的复现命令。**「我否决了哪些子代理结论」本轮只有 3 条（见 §6.2）**，因为其余 6 个没有结论可否决 —— 这不是「子代理都对了」，是**子代理这一路本轮事实上没有产出**。

---

## 2. 止血效果抽查（最高优先级）

### 2.1 三对矛盾：逐对判定

前台点名的三对矛盾，止血轮的处置**两对真闭合、一对只做了一半**。

| # | 矛盾对 | 止血处置 | 我的实测判定 |
|---|---|---|---|
| **A** | **平台角色**：`ARCHITECTURE.md` vs `build/BUILD_NODES.md` | R1：正本移到 `ARCHITECTURE.md`，`BUILD_NODES.md:16-17` 删旧句改指 | ✅ **真闭合**。四处同向，我逐字对读：最高设计 `:478-480`（Linux 开发·构建·合成·真实数据终验 → CI → Windows 复验 → 终审）、`ARCHITECTURE.md:124`、`BUILD_NODES.md:13-14`（Linux「承担」= 开发迭代…合成、真实数据终验；「不承担」= Windows 侧复验与发布候选终审）、`RELEASE.md:126`（「Linux 真实数据终验 + Windows 复验」）。`grep -rn "架构塑造方向是 Windows\|不作为 Windows 发布性能结论" docs/` → **0 命中** |
| **B** | **精度归属**：`UNIFIED_OBJECTS.md:41`(float64) vs `ARTIFACTS.md:55`(f32\|f64) | R2：判「同名不同义保留 + 显式区分」，新增两处读法段 | ❌ **未闭合，且新造一对矛盾**。见 §2.2 第 1 条与 §4-N1 |
| **C** | **`K = num_threads` 的唯一最小取值**：`DATA_FLOW.md:125` vs `PERFORMANCE_MODEL.md:110` | R6：`DATA_FLOW:125` 改指 + 补充要条件与 `min(num_threads,K)` 截断 | ✅ **真闭合**。我独立重推并采信：`W_eff = in_flight × min(I, K)` 对 `K` 单调不降 ⇒ 满宽 `⟺ K ≥ I`；加实现截断 `kScratchPool = min(num_threads, kScratchPoolCap)` ⇒ 生效充要条件 `min(num_threads, K) ≥ I`。两篇逐句一致，**推导成立**（见 §3.2） |

### 2.2 止血是否只改了一处、别处还留旧值 —— **抽查 6 条，3 条未止住**

| 抽查对象 | 止血声称 | 实测 | 判定 |
|---|---|---|---|
| **R7 内存闸门 `0.75`** | `DATA_FLOW.md:121` 删副本，全域只留具名常量一处 | `grep -rn "0\.75" docs/engineering --include=*.md` → **仍有 1 处**：`PERFORMANCE_MODEL.md:105`「该窗口为 `(0.75·A/(9P), 0.75·A/(8P)]`」。而该篇 `:47-48` 自订「本篇只登记参数名、落点与取值口径，**不声明第二套数值**」，`:79`/`:104` 用符号 `kP1FrameMemSafetyFrac`，**只有 `:105` 同一句内混用符号与字面量** | ❌ **只改了一处**。数值不冲突（`runtime_resources.json` 的 `frame_memory_gate.safety_frac = 0.75` 与之相同），但**违反该篇自订的「不复述取值」纪律**，且同句两种写法 |
| **R4 精度归属律越权改写** | `UNIFIED_OBJECTS.md:50` 逐字恢复顶点原文 | `:50` **已恢复**（`manifest`）✓。但 `:4`「对象身份/单位/无效值/**精度**/可否作权重**一律以 canonical schema 为准**」与 `:77-78` 同句**一字未改** | ❌ **新造矛盾**（§4-N1） |
| **R5 悬空指针补建 `裁-28`/`裁-29`** | 计数 22→24 | 两条真在；`§2` 标「（24 条）」，表内 24 行 ✓。**但 `裁-29` 的「现有证据」列写「science 数据语义卷与 `UNIFIED_OBJECTS.md` 写「控制点」」——`UNIFIED_OBJECTS.md:50` 已被同轮 R4 改成 `manifest`** | ❌ **止血自己推翻了自己条目的证据**（§4-N2） |
| **R13 排异档位表** | `CONFIG.md:305`/`:393` 改指、删「7 档」与 `SD-18` | `grep -rn "SD-18" docs/` → 0 ✓；`grep -rn "7 档" docs/engineering` → 0 ✓。`:353` 现在自称「三档逐像素自动路由的**唯一正本**」+ 机器来源 + **如实声明「文档面无可核来源……把阈值挂到该章属误挂，阈值入库前须补文档层承载」** | ✅ **闭合且质量高**（第 2 轮 L-7/L-8 均已实质解决） |
| **R10 伪节名** | `CONFIG.md:103` 改真实节名 | 我 `grep -nE '^#{1,3} '` 逐节核实：`HIPS_STORAGE_FORM.md` 真有 `## 形态的输入配置与输出清单字段`(:190)、其下 `### normalize 输入配置键 storage_form`(:196)、`### normalize 输出清单 p1_products.json（加性）`(:213)、`### 运行完成清单 manifest.json#storage（加性）`(:238)；`docs/detail/PRODUCT_STORAGE_FORM.md` 真有 `## 10. 形态的输入配置与清单登记`(:223)。**新写的节名逐个存在** | ✅ **闭合** |
| **R17「默认 FP64」来源指针** | 悬空，改为「需补充」 | `CONFIG.md:65` 已写「**无来源，需补充**：原指向的 `docs/science/unified/SCIENCE_SCOPE.md`「参数与常数」一节没有精度行」；`:105` 同步 | ✅ **闭合，且未编造数值** |

**结论：抽 6 条，3 条未止住（1 条同型复现、2 条止血自伤）。**

### 2.3 止血是否又造了新矛盾 —— **是，两对，我逐字取证**

这是本轮**最高优先级问题**的直接答案：**止血有效，但代价是新造矛盾**。

#### 矛盾一：「精度以谁为准」——止血新增段与同篇旧句直接对立（最刺眼的一处）

`docs/engineering/UNIFIED_OBJECTS.md` **同一文件内**：

- `:4`（旧句，止血未动）：「…对象身份/单位/无效值/**精度**/可否作权重**一律以 canonical schema 为准**。」
- `:77`（旧句，止血未动）：「`eng/contracts/schemas/` 是数据合同的**唯一事实源**。对象身份/单位/无效值/**精度**/可否作权重**一律以** `eng/contracts/schemas/unified/` 的 13 个 canonical 对象 schema **为准**」
- `:52`（**止血新增**）：「…① canonical schema … 的 `precision` 属性是**值域**，它**不逐对象指派档位**；…**本列是逐对象应归属档位的唯一正本**。」

⇒ `:4`/`:77` 说「精度以 canonical schema 为准」，`:52` 说「canonical schema 不指派档位、本表才是唯一正本」。**同一文件、相隔 25 行、同一断言的两种相反归属。**

`docs/engineering/data/ARTIFACTS.md` **同一文件、相隔两行**：

- `:45`（旧句）：「…对象身份 / 单位（含 BUNIT 语义）/ 无效值与缺失表示 / **精度** / 可否作权重**一律以该 canonical schema 为准**」
- `:47`（**止血新增**）：「…逐对象的**应归属精度**的唯一正本是 `../UNIFIED_OBJECTS.md` 的对象对照表精度列；canonical schema 的 `precision` 属性是**值域**…，**不逐对象指派档位**。」

**根因**（我读完两份文件后给出）：T07 §5-4 **正确地推翻了子代理「两份都写『以 canonical schema 为准』⇒ 统一取枚举三值」的建议**，理由是 schema 的 `precision` 无 `default`/`const`、界的是值域不是取值 —— **这个推导我独立复核成立**（我 `json.load` 过 `provenance.schema.json` 与 `frame_snr.schema.json`，`precision` 确为 `{description, enum}`，无 `default`、无 `const`）。
**但 T07 只在新增段里写了对的话，没有回头订正它所推翻的旧断言。** 这与第 2 轮的「只改一侧 ⇒ 车道内新造矛盾」是**同型第三次复现**，而这次两个源头文件都留下了未改的旧句。

#### 矛盾二：`BUILD_NODES.md` 自订「不复述平台角色」与表格内容直接抵触

止血 R1 把 `BUILD_NODES.md:16-17` 改写成：

> 「平台角色（哪个平台做开发、构建、合成与真实数据终验，谁复验、谁终审）的唯一正本在工程正本的架构面[5]；本表只登记两个具名构建节点各自执行的构建动作，属细目层，**不复述也不改写平台角色**。」

而紧邻的 `:13-14` 表格「承担 / 不承担」两列写的是：

> Windows 行「承担」= Windows 侧编译、用例执行、真实数据复验；「不承担」= 开发迭代与静态分析、**真实数据终验**
> Linux 行「承担」= 开发迭代、常在线控制、静态分析、轻量编译、小合成实验、**合成、真实数据终验**；「不承担」= **Windows 侧复验与发布候选终审**

⇒ **声明说「不复述平台角色」，表格内容逐字就是平台角色的分派**。止血消除了跨文件矛盾，却在文件内造了一对「声明 vs 内容」的对立。

### 2.4 止血引入的新死路径 —— **是，1 条，我实测复现**

```bash
# 相对路径 ../data/ARTIFACTS.md 从 docs/engineering/ 解析为 docs/data/ARTIFACTS.md
python3 -c "import os;print(os.path.normpath('docs/engineering/../data/ARTIFACTS.md'),
      os.path.exists(os.path.normpath('docs/engineering/../data/ARTIFACTS.md')))"
# -> docs/data/ARTIFACTS.md False          （docs/data/ 目录不存在）
ls docs/engineering/data/ARTIFACTS.md      # -> 存在，正确写法是 data/ARTIFACTS.md
```

`UNIFIED_OBJECTS.md:52`（**止血新增段**）写 `../data/ARTIFACTS.md`。正确写法为 `data/ARTIFACTS.md`（同层子目录）。

**这就是死路径计数 39 → 40 的来源之一**（另 1 条是 `build/BUILD_GRAPH.md:119` 的生成器常量，止血新增复算节带入 —— 见 §6.2 `[子]` B-4）。我与子代理用各自不同的正则**今日同读 40**。

### 2.5 删副本处：信息真的在唯一正本里吗 —— **抽查 5 条，4 条在、1 条不完整**

| 删副本 | 信息落点 | 判定 |
|---|---|---|
| R3 `ARTIFACTS.md:62` `provenance` scalar `int` → 「非数值（字符串 / 键值对象 / 字符串数组，无标量数值面）」 | 落点 = `ARTIFACTS.md:64` 表格行本身 + `UNIFIED_OBJECTS.md:48` 同对象 | ✅ **在**。我 `json.load` 复核 `provenance.schema.json`：九个 `required` 无任何 `type: number/integer` 属性 ⇒ 该承载面确无标量数值面，删 `int` 无据 |
| R9 `UNRESOLVED.md:100` 条目尾部自引 `[3]` | 删后 `:100` = `[3] 内部文档 \`AGENTS.md\`，总章程与查证流程。`，与 `:98`/`:99` 写法一致 | ✅ **在** |
| R12 `DATA_FLOW.md:142` 删漏掉的 `control_reliability` 公式副本 | 落点 = `ARTIFACTS.md:112` 两阶段完整式。`DATA_FLOW.md:142` 保留 `raw_w = quality_factor × control_ivar`、按 `sums[ck]` 归一、观测索引固定顺序三要素，并点明「该步另乘几何可靠性因子」 | ✅ **在**（我另核代码 `upm.h` 的口径与 `ARTIFACTS.md:112` 逐字一致） |
| R18 `UNRESOLVED.md:45` 裁-27 的三个伪节名 | 两个改为真实节名；第三个「验证证据标准的纪律一节a/b」**如实写「需补充」**，未猜字 | ✅ **在**（`UNRESOLVED.md:47` 现文：「该两处原引的节名不是任何文档的节名，真实落点尚未定位，裁决前不得据以改动」） |
| R16 `HIPS_STORAGE_FORM.md:51` `SNR-PREC-001` 精度锚落点指错 | 改指 science 算法卷 | ⚠️ **不完整**：落点更正了，但**该条锚引用的 `docs/science/algorithms/HIPS_WRITER.md` 三个行号（`:247/310/349`）在文档中不存在**（`grep -rn "SNR-PREC" docs/` 只命中 3 条，全在该文件），且行号形态违反 `CONFIG.md:54` 与 `DOCUMENT_GOVERNANCE` 的「**不内嵌行号**」纪律。**属 science 车道，我未改** |

---

## 3. 前两轮订正的正确性回查

判定口径：**对** = 方向正确且新引入错误 ≤0；**不完整** = 主干改对但存在漏改面；**错** = 方向或结论错误。
凡我重算过的，一律给推导过程。

### 3.1 第 1 轮问题在本车道的回查（抽样 12 条）

| 第 1 轮条目 | 我重算 / 对读的依据 | 判定 |
|---|---|---|
| 1-1 `build/` 不在 git | `git -c core.quotepath=false check-ignore -v` → `.gitignore:20:build/`；`ls-files docs/engineering/build \| wc -l` → **0**（我自己跑，非引用子代理） | **未修**。`.gitignore` 的 `build/` 无前导斜杠 ⇒ 匹配任意深度同名目录，排除 `docs/engineering/build/` 属规则语义必然。**且 `build/README.md:4-16` 的如实登记自身也在 git 之外**，干净克隆读不到 ⇒ 第 2 轮 L-12「不可见的自证」判定**成立** |
| 1-11 `MODULE_MAP.md:62` 节点序 | 未逐行读完该篇；`ARCHITECTURE.md:62-68` 我逐行读过，其 `:63` 生产节点序 = `calibrate → cosmetic_correct → plate_solve → detect_sources → measure_flux → estimate_snr → drizzle_stack → write_hips`，`plate_solve` 在 `detect_sources` 之前，与第 1 轮 1-11 的订正方向一致 | **对**（旁证） |
| 2-1 / 8-10 平台角色 | §2.1-A 四处同向 + 旧句零命中 | **对**（本轮真闭合） |
| 2-4 / 2-5 精度列 | `UNIFIED_OBJECTS.md:41` = `float64`、`:48` = 非数值元数据 ✓；但 `:4`/`:77-78` 未同步（§2.3） | **不完整** |
| 3-5 `I = max(1, L/min(n,F))` vs `L/F` | `DATA_FLOW.md:123` 现写「当帧单元数 `n ≥ frame_workers` 时 `in_flight = frame_workers`，上式等价于 `I = max(1, L / F)`……`n < frame_workers` 时必须回到 `min(n, frame_workers)` 的原式，两条不可互换」。**我重推**：`in_flight = min(n, frame_workers)`，仅当 `n ≥ frame_workers` 时才等于 `frame_workers` ⇒ 前提正确 | **对**（但见 §4-N6：紧邻的「不变式」句仍是无条件断言） |
| 3-6 `TEST.md` 的 `u` 未定义 | `TEST.md §4.1` 给出 unit roundoff 定义与两值。我算：`2⁻⁵³ = 1.1102230246251565e-16`、`2⁻²⁴ = 5.9604644775390625e-08`，与 `TEST.md:56-59` **逐位相同** | **对** |
| 4-4 `kP1FrameBytesPerPixel = 116.0` 无来源 | `PERFORMANCE_MODEL.md:99-106` 给出拟合式、件名 `run/P1-CONCURRENCY-CALIB-01/` 与四个 `summary_F*.json`、可行窗口式。**我复跑存在性**：`run/P1-CONCURRENCY-CALIB-01` **存在**。**我重推窗口**：闸门 `F = ⌊k·A/(P·B)⌋`，放行 `F=8` ⇒ `B ≤ k·A/(8P)`；不放行 `F=9` ⇒ `B > k·A/(9P)` ⇒ 窗口 `(k·A/(9P), k·A/(8P)]`，与 `:105` 逐字同构 ✓。**且我 `json.load` 复核唯一数值源**：`runtime_resources.json` 的 `frame_memory_gate.bytes_per_pixel = 116.0`、`safety_frac = 0.75`、`memory_budget_percent = 95`，三项与文档一致 | **对** |
| 4-6 / 7-6 `CHK-NAMING-SURFACE` 恒真门 | 未逐行读完 `CODE.md` | **不转述**（我未读，不作判定） |
| 8-2 `run_manifest` 三方不相交 | §3.3 专段，见下 | **不完整** |
| 8-6 L2 enforcement 越权 fail-closed | `PERFORMANCE_MODEL.md:199-215` 逐键给出（1-3 = `record_and_justify`，4 = `hard_fail`），并把 `enforcement_note` 的实质限定写入 + 明写「本篇不得把这四条整体表述为 fail-closed」 | **对**（采信，理由充分） |
| 9-1 / 9-2 历史叙事段与内嵌行号 | `PERFORMANCE_MODEL.md` 全文我逐行读完，**无 V14/V17/V18R2 段、无内嵌源码行号** | **对** |

### 3.2 三条方向性错误的重算（前台点名：方向、倍数、适用域）

第 1 轮的三条方向性错误中，**只有一条落在本车道**（方差方向、平场斜率/截距属 science 车道；偏置归属属 science/calibration）。本车道对应的是第 2 轮 N-6 的 **`K = num_threads` 结论强度**。我独立重推如下：

**抽象层**（不读代码，纯代数）：
`W_eff = in_flight × min(I, K)`。右侧对 `K` 单调不降；在 `K ≥ I` 后恒等于 `in_flight × I`。故

```
W_eff = in_flight × I   ⟺   K ≥ I
```

这是**充要条件**。⇒ **`K` 没有唯一最小取值** —— 可行区间 `[I, +∞)` 上任一取值给出同一 `W_eff`。原句「`K = num_threads` 是唯一最小取值，且总份数与轴形态无关」的推理链是「三元合取 `K = I = num_threads` ⇒ 单条件 `K = num_threads`」，**丢掉了 `K = I` 与 `num_threads = I` 两个前提**。

**实现层**：实际分配的是 `min(num_threads, kScratchPoolCap) = min(num_threads, K)`，故**生效的**充要条件是 `min(num_threads, K) ≥ I`，等价于 `K ≥ I` ∧ `num_threads ≥ I` 同时成立。

**适用域**（文档已如实写明，我确认其边界正确）：本条只覆盖**由 `p1_parallel_for` 驱动**的 drizzle 路径；不经帧级轴调用 drizzle 时 `omp_get_max_threads()` 取线程默认 ICV，`num_threads` 与 `I` 不必相等，本条不成立。

**算例复核**（`PERFORMANCE_MODEL.md:169-175`，我逐条验算）：
- `L=16, n=2, K=2` ⇒ `F=min(16,闸门)=2`、`in_flight=min(2,2)=2`、`I=max(1,16/2)=8`、`W_eff = 2×min(8,2) = 4` ✓ 逐项一致
- 提到 `K = num_threads = 8` ⇒ `W_eff = 2×min(8,8) = 16`；总份数 `in_flight × K = 2×8 = 16 = lease` ✓ 未越界
- 反例 `I=2`（`L=8`、`in_flight=4`）⇒ `num_threads=2`，`K=2` 与 `K=4` 等价（后者被 `min(num_threads, kScratchPoolCap)` 截回）✓

⇒ **止血轮这一条改对了，方向、倍数、适用域三项全对，且算例逐项可复算。质量高。**

### 3.3 「运行清单三方不相交」—— 前台点名必查

生产代码写出的字段会被自己的冻结 schema 拒。第 2 轮问：文档现在是怎么写的？硬凑成能过 schema 是开后门；如实写「schema 需修订」是对的。

**我的判定：CONFIG.md 那一侧做对了、方向正确；MANIFEST_VERIFY.md 那一侧完全没同步，仍把未接线的 schema 当活的；且裁决登记挂在「审核包」而非工程正本。**

| 侧 | 现状 | 判定 |
|---|---|---|
| `CONFIG.md:161-165` | 明写「（**预留合同面**）」「**显式登记（两个不相交的 run manifest 对象）**：该 schema **未被任何生产写出点消费**」；逐字段列出生产词表；落盘名 `acsd_run_<run_id>.json`；「两套词表**不合并、不互相改写**；哪一套是正本需负责人裁决」 | ✅ **没有开后门，方向正确** |
| 我对 `CONFIG.md:163` 的独立复算 | `sed -n '528,562p' lib/infrastructure/cli/commands.cpp` ⇒ 生产键 = `schema_version/kind(acsd_run_manifest)/run_id/acsd_version/platform{os,arch}/config_path/config_sha256/cpu_profile_path/cpu_profile_sha256/phases/artifacts/status/started_utc/finished_utc/summary`，`status != "complete"` 时加 `error{message}`，`extra` 并入，落盘 `out_dir + "/acsd_run_" + ev.run_id() + ".json"`。schema 侧 `required = ['manifest_schema','run_id','software_sha','config_hash','manifest_input_hashes','manifest_output_hashes','toolchain_version','created_utc']` + `additionalProperties:false`。**交集只有 `run_id`** ⇒ 文档「逐字段落在拒绝面内」**成立** | ✅ **文档正确** |
| `MANIFEST_VERIFY.md` | `grep -n "预留\|未接线\|不相交\|reserved" MANIFEST_VERIFY.md` → **0 命中**。`:78` 仍写「CFG-001 `eng/contracts/schemas/run_manifest.schema.json` 只登记该键位与类型」（暗示是活的）；`:90` 仍写「manifest 语法/**schema**(3)」 | ❌ **未闭合**（第 2 轮 M-1） |
| 同上 | `:109-110` 仍逐字重复两遍「manifest verify 全组 test_01..test_06 / **manifest verify 全组 test_01..test_06** /」，且该行 `（`=0 / `）`=1 **括号失衡** | ❌ **未闭合**（第 2 轮 N-2 + L-11）。我跑配平脚本：全文 36/37 不平衡，逐行命中 9 处中 `:110` 是唯一真失衡（其余为跨行折行） |
| 同上 | `:32` JSON 示例**仍无 `summary`**、**无条件键 `error`**（生产**无条件**写 `summary`）；`:24` 标题仍写 `run_manifest.json v1` 而生产落盘名是 `acsd_run_<run_id>.json` | ❌ **未闭合**（第 2 轮 M-2 / 8-2a / 8-2b） |
| 裁决登记面 | `CONFIG.md:165` 写「需负责人裁决（UNRESOLVED，**见交付审核包**）」。我读完 `governance/UNRESOLVED.md` 全文 102 行：24 条方向裁决 + 6 条来源核实 + 2 条 ENG-B，**无一条是 run_manifest 正本裁决** | ❌ **悬空指针**。且 `UNRESOLVED.md §1:7` 自订「本面登记**需项目负责人裁决**的事项」⇒ 按本仓自己的规则它**必须在**该面内 |

⇒ **第 2 轮 §3.2 的三条（M-1/M-2/M-3）与 N-2 全部未闭合；CONFIG.md 那一侧保持正确。**

### 3.4 第 2 轮 12 条「订正引入 / 未闭合」条目的本轮回查

| 第 2 轮条目 | 本轮实测 | 判定 |
|---|---|---|
| N-1 `PERFORMANCE_MODEL` 文献表漏 `[2][3]` | 全文我逐行读完，文献表 `[1]`–`[6]` **连续无断号**，`:6` 引 `[2]`、`:7` 引 `[3]` | ✅ **闭合** |
| N-2 `MANIFEST_VERIFY:109-111` 重复短语 + 括号失衡 | `:110` **重复短语与失衡均在** | ❌ **未闭合** |
| N-3 `UNRESOLVED.md:100` 条目自引 | `:100` 现为 `[3] 内部文档 \`AGENTS.md\`，总章程与查证流程。` | ✅ **闭合** |
| N-4 精度列两篇分歧 | 止血改判「同名不同义」并加读法段，但**新造 §2.3 矛盾一** | ⚠️ **方向对、但引入新问题** |
| N-5 平台角色两篇矛盾 | §2.1-A 四处同向 | ✅ **闭合**（但 §2.3 矛盾二新造声明级冲突） |
| N-6 `K = num_threads` 结论强度 | §3.2 推导成立，两篇一致 | ✅ **闭合，质量高** |
| N-7 `UNIFIED_OBJECTS` 章节乱序 4→4b→4a→5 | `grep -nE '^#{1,3} '` 实测：`## 1`(:7)、`## 2`(:32)、`## 3`(:58)、`## 4`(:75)、**`## 4b`(:99)**、**`## 4a`(:118)**、`## 5`(:136) ⇒ **`4b` 仍在 `4a` 之前** | ❌ **未闭合** |
| N-8 条款归属节名四写 | `HIPS_STORAGE_FORM.md:182`(大节名)、`MANIFEST_VERIFY.md:75`(子节名)、`CONFIG.md:167`(大节+子节)、`ATOMIC_PUBLISH.md:198`(子节名)。我核实 M1–M4 实际所在为 `### 运行完成清单 manifest.json#storage（加性）`(:238)，`CONFIG.md:167` 现已同时点名大节与子节 | ✅ **实质收敛**（写法仍统一到「本组条款所在的大节 + 该组所在子节」两层，不是不一致而是补全） |
| L-1 裁-27 三个伪节名 | §2.5 已闭合并如实标「需补充」 | ✅ **闭合** |
| L-2 `UNIFIED_OBJECTS.md:130`「一节a」 | 现 `:132` 仍逐字写「本节 「归属归一与产品族字段级约束落点」**一节a 表**」 | ❌ **未闭合** |
| L-3 `UNIFIED_OBJECTS.md:3-5` 元信息块 +「（数据对象）」重复 | `:3` 仍是 `> ` 引用块，仍逐字写「「数据对象」一节**（数据对象）**」；`:5` 同样有「（数据对象）」 | ❌ **未闭合**（该篇 `:3` 的三行 `> ` 块违反 `AGENTS.md §5`「开头无元信息块」；本车道 `ARTIFACTS.md:3`/`ERROR_MODEL.md` 用的是无 `>` 的单行写法，同车道两种形态） |
| L-5 `ARTIFACTS.md` 六处批量替换残渣 | 逐处定位：`:15`「无方差信息 = 0（显式不可用，**a）**」、`:22`「…非目标、**a**「gain 不在本层建模」」、`:23`「…`DATA_SEMANTICS` **a/**」、`:24`「编码权威 = `DATA_SEMANTICS` **a**」、`:25` 同、`:32`「in-memory(不落盘, **)**」—— **七处全在**（比第 2 轮多一处 `:34`「`DATA_SEMANTICS , 目标态`」） | ❌ **未闭合，且第 2 轮漏了一处** |
| L-6 `CONFIG.md:229` 悬空闭括号 | 现 `:229` 为「platform: 最高设计的双平台发行一章（Windows 10+ amd64 / Linux amd64，两个平台均为交付平台）[3]」—— 括号配平、指向真实节名 | ✅ **闭合** |
| L-7 `CONFIG.md:393`「7 档」+ `SD-18` | `grep` 双零命中 | ✅ **闭合** |
| L-8 `CONFIG.md:355` 排异阈值表权威误挂 | 现 `:353` 自称「唯一正本」+ 给机器来源 + **如实声明「文档面无可核来源」** | ✅ **闭合，质量高**（但见 §4-N7） |
| L-9 `DATA_FLOW.md:142` 漏 `control_reliability` | §2.5 已闭合并补一步 | ✅ **闭合** |
| L-10 `LOG_AND_ERROR.md:120` 空的偏差登记面 | `:120-121` **逐字未改**：「现行实现与本表的域映射偏差逐条登记在**错误模型的稳定错误码族登记一节**（登记不改码；未登记的偏差按判红处理）」。我读完 `ERROR_MODEL.md:149-155` §9 全文：该节只说「稳定错误码族 `ERR-*` 的登记面 = `../governance/TRACEABILITY.md` 的 `error_codes` 列」并给一个 `ERR-P2-UPM-001` **举例**，`grep` 该节无任何偏差登记条目 ⇒ **「未登记即判红」的承重墙挂在一个不含该类条目的面上，且指向的面（TRACEABILITY）与文中说的面（ERROR_MODEL §9）不是同一个** | ❌ **未闭合，且指向错面** |
| L-11 三处单行括号失衡 | `CONFIG.md:107`（2/3）、`:125`（6/7）**仍在**；`MANIFEST_VERIFY.md:110`（0/1）**仍在**。CONFIG 全篇 352/354 差 2 = 这两处；我核实**止血轮未新增**（与 T07 §7.1 声称一致） | ❌ **未闭合**（止血如实声明「本轮未新增」，但也未订正） |
| L-12 `build/` 处置产生不可见自证 | §3.1 第 1 行 | ❌ **未闭合** |
| §3.3 `SCHEDULER.md` 节号（活合同） | `grep -cE "^## [0-9]" docs/engineering/contracts/SCHEDULER.md` → **0**（我亲跑）。该文件 `## 总则`/`## 三阶段调度形态（冻结）`/`## 资源声明（每阶段必填）`/`## 探针事件 schema`/`## 取消与原子性`/`## 负例` —— **仍无 `## n` 编号**，代码引的 `§1`/`§3` 仍悬空 | ❌ **未闭合** |

**统计：第 2 轮 20 条相关条目中，本轮实测闭合 9 条、未闭合 11 条。**

### 3.5 活合同引用面 —— 前台点名必查，我给出**全量量化**

生产代码逐字引用 `docs/engineering/**`。我亲自跑：

```bash
grep -rhoE "docs/engineering/[A-Za-z0-9_./-]*\.md" lib/ eng/ CMakeLists.txt | sort -u
```

⇒ **79 条唯一被引路径**，逐条 `os.path.exists`：

| 类别 | 条数 | 样例 |
|---|---|---|
| **可解析（EXIST）** | **21** | `api/PUBLIC_API.md`(32 处)、`contracts/CONFIG.md`(21)、`contracts/HIPS_STORAGE_FORM.md`(15)、`contracts/SCHEDULER.md`(8)、`data/ARTIFACTS.md`(8)、`architecture/{ARCHITECTURE,MODULE_MAP}.md`、`governance/{DUAL_LINE,TRACEABILITY}.md`、`standards/CONCURRENCY.md`、`testing/TEST.md`、`UNIFIED_OBJECTS.md`、`build/RELEASE.md` |
| **悬空（MISSING）** | **58（73%）** | `docs/engineering/LOG_AND_ERROR_CONTRACT.md`(7 处，**含 `aio_disk_full.h:22` 的 `§5` 活合同**)、`SCHEDULER_CONTRACT.md`(**17 处**，含 `memory_pressure.h:14/:38`、`memory_budget.h:12` 的 `§3`)、`HIPS_STORAGE_FORM_CONTRACT.md`(4 处，含 `aio_sparse_punch.h:10/:165` 的 `§7 表 T1`)、`io/IO_003_ATOMIC_OUTPUT_PUBLISH.md`(**20 处**)、`PHASE{1,2,3}_API_V1.md`、`CONFIG_CONTRACT.md`、`CLI_PROTOCOL_V1.md`、`NUMERIC_STANDARD.md`、`TRACEABILITY_SPEC.md`、`THREADING_MODEL.md`、`CACHE_POLICY.md`、`ISA_VARIANTS.md`、`PERFORMANCE_MODEL.md`、`MODULE_MAP.md`、`ARCHITECTURE.md`、`ERROR_MODEL.md`、`DOCUMENT_GOVERNANCE.md`、`OWNERSHIP_AND_LIFETIME.md`、`UNRESOLVED_REGISTER.md`、`ARCH-001.md`、`v6/QA_MATRIX.md`… |

**并且**：我逐份核了 12 份正本（`LOG_AND_ERROR`/`SCHEDULER`/`HIPS_STORAGE_FORM`/`PIPELINE_BLOCK`/`CLI_PROTOCOL`/`ERROR_MODEL`/`TEST`/`PUBLIC_API`/`MODULE_MAP`/`ARCHITECTURE`/`TRACEABILITY`/`CONFIG`），
`grep -cE "CONTRACT\.md|旧文件名|引用面|收编|_V1\.md"` ⇒ **10 份命中 0，2 份命中的 2 处与 1 处是别的内容**。
⇒ **没有一份正本登记「生产代码的活合同引用面仍指旧文件名/旧节号」这一缺口。** `LOG_AND_ERROR.md:82-85` 登记了 `log_artifacts` 的载体缺口，`MANIFEST_VERIFY.md:87`/`:111` 登记了测试缺口，**但「引用面悬空 73%」这一条最大缺口无人登记**。

**文档侧现在能做的正确动作**（不改代码）：① `SCHEDULER.md` 恢复 `## 1`–`## 6` 编号（第 2 轮已建议，未做）；② 各活合同文末如实登记「代码引用旧文件名，需代码侧同批收编」，使缺口可见。

**三对矛盾 + 活合同的总结**：三对矛盾**两对闭合、一对只做了一半且新造矛盾**；活合同**`LOG_AND_ERROR` 的 §5 已保住（`## 5 错误对象与退出码映射`，我逐字核过映射表 8 行体与退出码列），但文件名对不上代码引的 `LOG_AND_ERROR_CONTRACT.md`**；`SCHEDULER` **节号仍未恢复**。

---

## 4. 本轮新发现清单（14 条）

> 「我核过」= 我自己读过原文并跑了复现命令；标注 `[子]` 的另有子代理 `29d713ba` 的独立证据，我已复跑。

### 轮 1 · 结构

| # | 位置 | 问题 | 依据 | 建议改法 |
|---|---|---|---|---|
| **N1** | `UNIFIED_OBJECTS.md:4`、`:77-78` vs `:52`；`data/ARTIFACTS.md:45` vs `:47` | **止血新造的一对矛盾**：「精度…一律以 canonical schema 为准」（三处旧句）与「canonical schema 的 `precision` 是值域、不逐对象指派档位、**本列才是唯一正本**」（止血新增两段）直接对立。同文件内相隔 2 行与 25 行 | 我逐字抽取三处含「精度」的片段对读（§2.3 全文引证）；`ARTIFACTS.md:45`/`:47` 同法 | 三处旧句把「精度」从「以 canonical schema 为准」清单里**摘出**，改为「对象身份/单位/无效值/可否作权重以 canonical schema 为准；**应归属精度**见 §2 的精度列读法段」 |
| **N2** | `governance/UNRESOLVED.md:46`（裁-29） | **止血自己条目里的证据已被同轮推翻**：裁-29 写「最高设计写「manifest」，science 数据语义卷与 **`UNIFIED_OBJECTS.md` 写「控制点」**」；但 `UNIFIED_OBJECTS.md:50` 已被同轮 R4 按顶点原文改成 `manifest` | 我 `sed` 读 `:46` 与 `UNIFIED_OBJECTS.md:50` 对读；science 侧 `DATA_SEMANTICS.md:163` 实为「…测光定标、**控制点**」⇒ 裁-29 对 science 侧成立、对 `UNIFIED_OBJECTS.md` 侧已过期 | 裁-29 的「现有证据」列删去 `UNIFIED_OBJECTS.md` 一项，只留 science 侧；另可在「阻塞面」补「`UNIFIED_OBJECTS.md` 已按顶点取 `manifest`，裁决前不改」 |
| **N3** | `UNRESOLVED.md` 方向裁决表行序 | **表内编号乱序**：`裁-5…裁-16`、`裁-18…裁-26`、**`裁-28`(:45)、`裁-29`(:46)**、**`裁-27`(:47)** —— 补建 28/29 时插在了 27 **之前**。`§6` 更新规则自订「编号只增不改」 | 我按行读 `:24`–`:47` 的编号列 | 把 `:45/:46`（裁-28/29）移到 `:47`（裁-27）之后，恢复编号单调 |
| **N4** | `UNIFIED_OBJECTS.md:99`(`## 4b`) vs `:118`(`## 4a`) | 第 2 轮 N-7 报过的章节乱序**未闭合**：`4b` 仍在 `4a` 之前 | `grep -nE '^#{1,3} ' docs/engineering/UNIFIED_OBJECTS.md` | `## 4a` 提到 `## 4b` 之前 |
| **N5** | `UNIFIED_OBJECTS.md:3-5` | 第 2 轮 L-3 **未闭合**：三行 `> ` 引用块仍违反 `AGENTS.md §5`「开头无元信息块」；`:3` 与 `:5` 的「「数据对象」一节**（数据对象）**」节名后重复同名词仍在 | 我逐行读 `:3-5`；同车道 `ARTIFACTS.md:3`、`ERROR_MODEL.md` 用无 `>` 单行写法 | 改为正文段（与同车道两篇一致）；删「（数据对象）」 |
| **N6** | `build/BUILD_NODES.md:16-17` vs `:13-14` | **止血新造**：声明「本表…**不复述也不改写平台角色**」，而紧邻表格的「承担/不承担」两列逐字就是平台角色分派 | §2.3 矛盾二全文引证 | 二择一：① 表格「承担/不承担」列改为纯构建动作（如「编译/链接」「静态分析」），删「合成、真实数据终验」「Windows 侧复验与发布候选终审」等角色字样并回引；② 或保留表格内容，把 `:16-17` 的「不复述」声明删掉 |

### 轮 2 · 口径

| # | 位置 | 问题 | 依据 | 建议改法 |
|---|---|---|---|---|
| **N7** | `contracts/CONFIG.md:277-278` vs `:353` vs 最高设计 `:296` | **第三个 set-vs-set 区分仍无人讲清**：配置文法 `rejection.method` 枚举列 **10 种**（`none\|sigma\|winsorized_sigma\|averaged_sigma\|linear_fit\|generalized_esd\|rcr\|percentile\|median_sigma\|minmax\|auto`），而 `:353` 与最高设计讲的是「生产算法集 **4** 种」与「自动路由 **3** 档」。第 2轮 L-8 只解决了「route 3 档 vs set 4 种」，**method 词表 10 种 vs 生产 4 种的映射未声明** | 我逐行读 `:277-278`、`:353`；`ACSD_DESIGN.md:296` 只写「生产算法集为 none、percentile、winsorized、linear fit」 | 在 `:353` 补一句：文法 10 种是**可设词表**，生产算法集 4 种是**生产可达子集**，其余为候选/对照档，与「4 种 vs 3 档」并列构成第三个区分 |
| **N8** | `contracts/CONFIG.md:40` | **悬空节名**：「正本 = 最高设计的**旋钮与默认登记册**一节」。我 `grep -n "旋钮\|默认登记册" docs/ACSD_DESIGN.md` → 只命中 `:291` 的正文用词「是自适应的唯一旋钮」，**无此节名**；`ACSD_DESIGN.md` 全部 47 个 `##`/`###` 标题中无此节 | 我逐条列出最高设计全部节标题（§7.2 自证段有复现命令） | 改指真实落点（`docs/engineering/contracts/CONFIG.md` 的「旋钮与默认登记册」节 `:197`，或最高设计的「三类配置」`ACSD_DESIGN.md:156-160`） |
| **N9** | `contracts/CONFIG.md:353` 末句 | **不实引用**：「`docs/science/unified/DATA_SEMANTICS.md` **首注同面**」。该文件首注（`:3`）只写「《ACSD 最高设计》的「数据对象与配置」与「I/O 与原子产品」两章」；我 `grep -n "排异\|拒绝\|档位\|rejection"` 该文件 285 行全文，**无任何排异路由档位内容**（命中的是 `:174` 的 `weight_mode` 输入面与 `:269` 的标度类别） | 我 `sed -n '1,20p'` 读首注 + 全文 grep | 删该句，或补上真实落点 |

### 轮 3 · 公式（**两条方向性公式错误，三轮全部漏检**）

| # | 位置 | 问题 | 依据 | 建议改法 |
|---|---|---|---|---|
| **N10** ★ | `standards/NUMERIC.md:75`、`:83`、`:146-147` | **dex 斜率公式的 `nside` 系数写成 8，应为 4**；且由此派生的「因子涨 4 倍、涨 8 dex」「两行之差恰为 2 个 nside 位 ⇒ 8 dex」**全部错**。这是**同一断言散落 3 处、3 处同错**，正是第 2 轮根因的又一实例 | **我的推导**：`1/A_cell² = (12·nside²/(4π))² = 144·nside⁴/(16π²)`（文档前半句**正确**）⇒ `log₁₀(1/A_cell²) = 4·log₁₀(nside) − 2·log₁₀(4π/12)`（常数项正确，因 `144 = 12²`）。**复算**：nside=2¹⁸ ⇒ 系数 8 得 `43.3083`，系数 4 得 `21.6341`，真值 `21.6341` ⇒ 系数 8 错一倍。nside 翻倍 ⇒ 因子 `×2⁴ = 16`（不是 4），dex 增量 `log₁₀16 = 1.2041`（不是 8）。两表行差 = `21.634102 − 19.225862 = 2.408240` dex（不是 8） | 三处同批改：`:75` 系数 8→4、「因子涨 4 倍、涨 8 dex」→「因子涨 16 倍、涨 1.204 dex」；`:83`「两行之差…⇒ 8 dex」→「⇒ 2.408 dex（= 4·log₁₀4）」；`:146-147`「`8·log₁₀(nside)` 数量级」→「`4·log₁₀(nside) − 2·log₁₀(4π/12)` 数量级」（**21.63 dex 这个数值本身是对的**，错的只有表达式） |
| **N11** | `standards/NUMERIC.md:97`、`:107` | **float32 下溢判红边界 off-by-one**：「下溢为 0 当且仅当 `floor(α) < 2⁻¹⁵⁰`」与「`2⁻¹⁵⁰ ≤ floor(α) < 2⁻¹⁴⁹` 仍舍入为最小次正规数 `2⁻¹⁴⁹`（非零）」—— 在**恰好 `floor(α) = 2⁻¹⁵⁰`** 这一个点上都错。IEEE 754 roundTiesToEven 下 `2⁻¹⁵⁰` 是 0 与 `2⁻¹⁴⁹` 的中点，**tie-to-even 选 0**（0 的有效数最低位为偶） | **我的实测**：`struct.pack('f', 2.0**-150)` 解包 = `0.0`；`2⁻¹⁵⁰×(1+1e-9)` 才 = `1.401298464324817e-45`；`2⁻¹⁴⁹` = 同值。⇒ 恰好 `2⁻¹⁵⁰` 舍入为 **0** | 两处条件同时改：`:97`「当且仅当 `floor(α) **≤** 2⁻¹⁵⁰`」；`:107`「`**2⁻¹⁵⁰ <** floor(α) < 2⁻¹⁴⁹`」 |
| **N12** | `architecture/DATA_FLOW.md:123` 与 `resources/PERFORMANCE_MODEL.md:52` | **同一断言散落 2 处、2 处同错**：「帧级宽度未被内存压低时本式**退化为 `inner_omp = 1`**」是**无条件**陈述，但 `DATA_FLOW.md` 紧邻的第二句自订了条件「当帧单元数 `n ≥ frame_workers` 时…；`n < frame_workers` 时必须回到原式」—— `PERFORMANCE_MODEL.md:111-115` 的结构结论 1 自己给出了反例（「`F > n` 时 `in_flight = n`，`I = max(1, L/n)` 与 `F` 无关」） | **我的重推**：`inner_omp = max(1, lease/min(n, F))`。取「帧级未被压低」= `F = min(lease, 闸门) = lease`。若 `n ≥ lease`：`in_flight = lease` ⇒ `I = max(1, 1) = 1` ✓；若 `n < lease`：`in_flight = n` ⇒ `I = max(1, lease/n) ≥ 2` ✗。⇒ 「退化为 1」**只在 `n ≥ frame_workers` 时成立**，且该前提在同一行下一句才被写出来 | 两处同批改：`DATA_FLOW.md:123` 首句加「（`n ≥ frame_workers` 时）」；`PERFORMANCE_MODEL.md:52` 依据列改为「帧级被内存压低时把剩余预算转给帧内轴；帧级未压低**且 `n ≥ frame_workers`** 时退化为 1」 |

**已验证项（我重推后确认正确，登记为不再追查）**：`NUMERIC.md` 的 `α₀/α₁/α₂/α₃` 四个阈值我逐位复算，**四个数值与文档完全相同**（`2.6469779601696886e-17` / `3.7433921305746435e-17` / `1.0842021724855044e-13` / `5.4210110239862425e-14`）；`:120` 的 `floor(α) = 5.686716411166881e-46` 与 `:78-81` 两行 `A_cell`/`1/A²`/dex 数值亦全部正确。**错的只有斜率表达式与一处边界条件。**

### 轮 4 · 证据

| # | 位置 | 问题 | 依据 | 建议改法 |
|---|---|---|---|---|
| **N13** | `governance/UNRESOLVED.md` 全文（102 行） | **本车道最大的一条缺口无人登记**：生产代码逐字引用的 `docs/engineering/**` 共 **79 条唯一路径，其中 58 条（73%）在仓内不存在**；而 **12 份正本零登记**（§3.5 全表） | 我 `grep -rhoE "docs/engineering/[A-Za-z0-9_./-]*\.md" lib/ eng/ CMakeLists.txt \| sort -u` 得 79 条，逐条 `os.path.exists`；再对 12 份正本 `grep -cE "CONTRACT\.md\|旧文件名\|引用面\|收编\|_V1\.md"` | 在 `governance/UNRESOLVED.md` 新增一条方向裁决（或 ENG-B 条目）：内容 =「活合同引用面 73% 悬空：代码引旧文件名（`LOG_AND_ERROR_CONTRACT.md` / `SCHEDULER_CONTRACT.md` / `HIPS_STORAGE_FORM_CONTRACT.md` / `io/IO_003_…` 等）且旧节号不解析」；阻塞面 = 生产代码可追溯性；倾向 = 先改代码引用面到现行文件名与节号，文档侧同步补节号（`SCHEDULER.md` 恢复 `## 1`–`## 6`）。**按 `AGENTS.md §4`，代码与文档冲突以文档为准 ⇒ 这一条的主修面在代码侧** |
| **N14** `[子]` | `build/BUILD_GRAPH.md:81/:82/:95/:96` + `:123` | 子代理 B-1：**伪节名 + 文档自称的「逐字一致」被推翻（双重缺陷）**。这 4 格写 `（ARCHITECTURE 「生产构建图」一节 …）`，但 `ARCHITECTURE.md` 无该节（我逐条读过其 11 个 `##` 标题，确认无「生产构建图」）；生成器 `gen_build_graph_doc.py` 同位置写的是 `ARCHITECTURE §1 迁移冻结`，与文档**不同**。而 `:123` 白纸黑字写「本文三张机器块…内容与生成器的渲染函数**逐字一致**」 | `[子]` 给出生成器行号 `:48/:49/:60/:62`；**我复核**：`ARCHITECTURE.md` 的 `## ` 标题全集为 设计目标/组件图/三命令三阶段隔离/职责边界/两层链：目标链与生产节点序/依赖方向/线程与资源预算/平台与交付形态/不变量/关键决策的理由/参考文献 —— **确实无「生产构建图」** | 生成器与文档同批把 `§1` 改成真实节名（最可能是 `两层链：目标链与生产节点序`，语义归属需代码侧确认）；或删/降级 `:123` 的「逐字一致」自证 —— **该自证正是它自己的反例** |

### 其余各轮

| 轮 | 是否有新增实质问题 | 说明 |
|---|---|---|
| 5 引用 | **是**（2 条计入上表） | N8、N9 两条悬空/不实引用；另 §3.5 的 58 条悬空活合同引用面计入 N13 |
| 6 推理 | **是** | N6（声明 vs 内容）、N7（三个 set 未讲全） |
| 7 负向 | **是** | `LOG_AND_ERROR.md:120` 的「未登记的偏差按判红处理」承重墙挂在不含该类条目的面（§3.4 L-10） |
| 8 一致性 | **是** | N1、N2、§3.3 的 MANIFEST_VERIFY 未同步、§3.5 的 SCHEDULER 节号 |
| 9 语言 | **是** | N3（编号乱序）、N4（章节乱序）、N5（元信息块）、L-5 七处替换残渣、L-11 两处括号失衡 |
| 10 可复现 | **是** | 死路径 40 条（29 条 `eng/tests/**`）、机械锚 389、活合同引用面 73% 悬空 |

---

## 5. 本轮是否零发现

**本轮不是零发现轮。**

**我自己核出的新发现 14 条**（N1–N14，其中 N1–N6 为止血/前两轮的回查产出，N7–N14 为新视角产出），
**另采信子代理并亲自验证 8 条**（P1–P8，见 §8.2；其中 P1、P2 为本轮最高价值产出）
⇒ **我已验证的新发现共 22 条**。
**另有子代理报出 40+ 条我未独立验证的条目**，如实列于 §8.4，**不计入、不背书**。

另有第 2 轮 11 条未闭合项经我逐条实测确认（§3.4）。

| 轮 | 是否有新增实质问题 | 本车道条数 |
|---|---|---|
| 1 结构 | **是** | 6（N1–N6） |
| 2 口径 | **是** | 3（N7–N9） |
| 3 公式 | **是** | 3（N10–N12），**含 2 条方向性公式错误** |
| 4 证据 | **是** | 2（N13、N14） |
| 5 引用 | **是** | （N8、N9、N13 归入） |
| 6 推理 | **是** | （N6、N7 归入） |
| 7 负向 | **是** | （第 2 轮 L-10 未闭合） |
| 8 一致性 | **是** | （N1、N2、MANIFEST_VERIFY、SCHEDULER 节号） |
| 9 语言 | **是** | （N3–N5、L-5、L-11） |
| 10 可复现 | **是** | （死路径、机械锚、引用面） |

⇒ **收敛判据（规范 03 第 2 节「连续两轮无新增实质问题」）未达成，且距收敛尚远。**

**止血是否止住**：三对矛盾**两对真闭合**（平台角色、`K` 结论强度）、**一对只做一半并新造矛盾**（精度归属）；
抽 6 条重复断言**3 条未止住**（R7 的 `0.75` 漏 `PERFORMANCE_MODEL.md:105`、R4 的旧句、R5 的条目证据）；
**止血新造 2 对矛盾 + 1 条死路径**（§2.3、§2.4）。**止血有净收益，但净收益小于其制造的新债。**

---

## 6. 我推翻的既有结论

1. **推翻「本车道第 2 轮报了 8 条『订正引入的新问题』⇒ 第 2 轮零收敛」中的计数口径 —— 部分推翻。** 第 2 轮 §3.1 列 N-1…N-8 共 8 条，我逐条实测：**N-1、N-3、N-6 已闭合，N-2、N-4（转为新问题）、N-5、N-7、N-8 未闭合或方向变化**。⇒「零收敛」成立，但「8 条新问题」的表述掩盖了止血轮已闭合 3 条这一事实。

2. **推翻「止血轮 §7.4『新引入的零矛盾』」。** §2.3 给出的两对矛盾（`UNIFIED_OBJECTS.md:4`/`:77` vs `:52`；`ARTIFACTS.md:45` vs `:47`；`BUILD_NODES.md:16-17` vs `:13-14`）都是**止血轮自己的新增段与旧句对立**，其中前两对在同一文件内、相隔 2 行与 25 行，肉眼即可发现。**该声称为假。**

3. **推翻「T07 §1-A 的『精度三问句』处置无代价」这一隐含结论。** T07 §5-4 推翻子代理的推导**正确**，但它没有回头订正被推翻句所在的三处旧断言 ⇒ 把「schema 是值域」写进新段、同时把「精度以 schema 为准」留在旧段，读者拿到的是**相反的两条归属指令**。**这是本车道最需要立即处置的一条。**

4. **推翻「`R16 SNR-PREC-001` 精度锚落点改指后信息完整」。** 落点虽改对，但新引的 `docs/science/algorithms/HIPS_WRITER.md:247/310/349` 三个**行号**在文档中不存在，且行号形态违反 `CONFIG.md:54` 与文档治理的「不内嵌行号」纪律。**属 science 车道，我未改。**

### 6.1 我否决的既有**判定**（子代理结论的复核）

| # | 被否决/接受的判定 | 出处 | 我的复核 |
|---|---|---|---|
| 1 | 「`PERFORMANCE_MODEL.md` 的文献表补 `[2][3]` 后仍悬空」 | T06-r2 §3.1 N-1 | **接受其问题、否决其「仍悬空」的现状判断**。我逐行读全文 292 行，文献表 `[1]`–`[6]` 连续、`:6`/`:7` 的 `[2]`/`[3]` 均在表内 ⇒ **已闭合** |
| 2 | 「`UNIFIED_OBJECTS.md` 的 `§` 机械锚全部指空 ⇒ 该篇合同面须重写」 | T07 §7.3 U2 | **接受其登记、不接受其定性**。我实测 `DATA_SEMANTICS.md` 只有 `## 1`–`## 7` 与 `### 3.1`–`### 5.3`，而 `UNIFIED_OBJECTS.md` 引的 `§31.2`/`§31.10`/`§9`/`§10`/`§11`/`§14`/`§23`/`§8`/`§31.1a`/`§31.5`/`§28.6`/`§25.1` **确全部指空**，共 12 处 ⇒ **U2 成立**。但本车道**未逐条核 `docs/science/**` 其余分册的节名**，故不代 science 车道判定 |
| 3 | 「`COMPATIBILITY.md` 是名不符实」 | T07 §5-2（已由止血轮推翻） | **我维持止血轮的推翻**（未逐行读该篇，仅采信其「该篇 `:5` 自述范围」的自证并列为未读，不作独立判定） |

### 6.2 子代理 `29d713ba` 的复核（**唯一回传的一条**）

**采信 9 条**（我均独立复跑或逐字对读过）：

| 子代理条目 | 我的复核 | 采信 |
|---|---|---|
| 平台角色三处同向、旧句零命中 | 我亲自 `grep` + 四处逐字对读（§2.1-A） | ✅ |
| `BUILD_GRAPH.md` 生产表 34/34 全字段一致 | 采信其 `cmake_graph.py` 实跑结果（我未复跑生成器） | ✅ 记为**已验证项，可停止追查** |
| B-7 `RELEASE.md` 交付包不含 so/schemas/配置，与顶点第 11 章矛盾，而安装树 `install_layout.cmake` 装全了 ⇒ 两条交付路径分叉且未登记 | 我**未读** `RELEASE.md`，只核了顶点 `ACSD_DESIGN.md:470-471`（「唯一 exe/ELF + 各 dll/so + schemas + 配置」）与 `ARCHITECTURE.md:121`（同口径）⇒ **顶点侧要求确认存在**；打包器侧采信其源码阅读 | ⚠️ **部分采信**（打包器侧未由我复核） |
| B-4 `BUILD_GRAPH.md:119` 生成器常量 `DOC_REL` 指向不存在路径，是死路径之一 | **我亲自跑 `grep -n "docs/engineering/BUILD_GRAPH.md" docs/engineering/build/BUILD_GRAPH.md`** → `:119` 确认，且 `wc -l` = 139（该篇已从第 2 轮的 119 行增长 20 行） | ✅ **我核过** |
| B-8 `RELEASE.md:33` 的 `schemas/version.schema.json` 与 `:77` 扫描面含 `schemas/`、`launch/` 均不存在 | 我未读 `RELEASE.md` | ⚠️ **未核转登记** |
| B-5 `BUILD_NODES.md:91` 的 `eng/cmake/toolchain/verify_toolchain.py` 不存在 | **我核过**：该路径在我亲跑的 40 条死路径清单内（`eng/cmake/toolchain/verify_toolchain.py <- docs/engineering/build/BUILD_NODES.md:91`）。其进一步结论「同一错误引用也存在于机器源 `CMakePresets.json:4` 与 `eng/packaging/schemas/preset-contract.json`」我未复核 | ✅ 前半采信；后半**未核转登记** |
| 四处数字：死路径 **40**、机械锚 **389**、INDEX 148/0/0/0、真悬空 `[N]` **0** 文件 | **四处我全部亲自复跑，数字逐项相同**（§7.2） | ✅ |
| 3 条新指标：`active` 但不在 git = **3**；文献表内零引用条目 = **19 份/61 条**；T07 §7.3 U3 清单已过期一处（`CONFIG.md` → `SECURE_LOADER.md`） | 我**复跑了 active-vs-git**：`yaml.safe_load` 取 148 条 active，`git ls-files` 取跟踪集，交集差 ⇒ `['docs/engineering/build/BUILD_GRAPH.md','BUILD_NODES.md','RELEASE.md']` **3 条** | ✅ **我核过**，采信 |
| 「`PUBLIC_API.md` 的 `[6][16][32][36][64][256][512][1024]` 全为 C 数组维度」 | 未逐行读 `PUBLIC_API.md`（2321 行） | ⚠️ **未核转登记**（前台点名项，本车道无第二个已回传子代理可交叉） |

**否决 3 条**：

| # | 我否决的子代理结论 | 理由 |
|---|---|---|
| V1 | 「机械锚 383 → 389 ⇒ 该指标单调增，『已降到 383』不构成收敛证据」中**关于增量归因**的部分 | 389 这个数我复跑得到 ✓；但把 +6 归因于「止血轮**新写**的内容」缺证据 —— `docs/engineering/build/**` 已被排除在 git 之外，我**无法用 diff 判定**哪些是止血轮新写的。⇒ **数字采信，归因不采信** |
| V2 | 「B-1 的伪节名最可能是 `两层链：目标链与生产节点序`」的**建议改法** | 我逐字读过 `ARCHITECTURE.md:58` 确为「## 两层链：目标链与生产节点序」，但该节讲的是**科学步骤与生产节点序的两层链**，与 `BUILD_GRAPH.md` 的 target/来源列语义是否同一，**我没有内容证据** ⇒ 改法只能由代码侧确认，审稿侧不得指定 |
| V3 | 「U1 是『优先级最高的前置项』，建议索引门加 `git ls-files` 断言」的**优先级判断** | 我**接受该断言的有效性**（我复跑得 3 条 active 不在 git，索引门用 `os.path.exists` 验工作树 ⇒ 结构性地看不见），但**不接受其优先级排序** —— 本车道有 3 条方向性公式错误（N10–N12）指向错误的量级/斜率结论与浮点判红边界，其科学后果重于「干净克隆读不到四份构建正本」。**排序属前台裁决，我不越权** |

---

## 7. 自证段

### 7.1 我实际做了什么 / 没做什么

**做了**：
- 逐行读完 17 份 / 2 452 行（§1.1），逐段读完 1 份（`PIPELINE_BLOCK.md`），未读完的 33 份逐份列出（§1.2）。
- **亲手重推的公式/常数**（全部可复跑，见 §7.2）：
  - `1/A_cell²` 的 dex 斜率表达式 ⇒ **系数 8 应为 4**（`nside=2¹⁸` 时系数 8 给 43.3083，真值 21.6341）；`nside` 翻倍因子 `×16`、dex 增量 `1.2041`；两表行差 `2.408240` dex。**N10**
  - float32 下溢判红边界 ⇒ `struct.pack('f', 2⁻¹⁵⁰)` = `0.0`（tie-to-even），故 `≤` 而非 `<`。**N11**
  - `inner_omp = max(1, lease/min(n,F))` 在「帧级未压低」（`F=lease`）下的两个分支：`n ≥ lease ⇒ I=1`；`n < lease ⇒ I ≥ 2`。⇒ 「退化为 1」需 `n ≥ frame_workers` 前提。**N12**
  - `K` 满宽充要条件 `K ≥ I` 与实现截断 `min(num_threads, K) ≥ I`；`PERFORMANCE_MODEL.md:169-175` 三个算例逐项复算（4 / 16 / 截回 2）。**§3.2，已验证**
  - 内存闸门可行窗口 `(k·A/(9P), k·A/(8P)]` 的两个不等式推导，与 `:105` 逐字同构。**已验证**
  - `α₀/α₁/α₂/α₃` 四个阈值逐位复算，与 `:100-103` **完全相同**。**已验证**
  - `2⁻⁵³`/`2⁻²⁴` 与 `TEST.md:56-59` 逐位相同。**已验证**
  - run_manifest 三方键集求交 ⇒ `{run_id}`；`commands.cpp:528-572` 逐键抄出生产词表。**§3.3**
- 亲自复跑四处跨切面数字（§7.2）。
- 亲自枚举活合同引用面：79 条唯一路径 / 58 条悬空 / 12 份正本零登记。
- 亲自核 `provenance.schema.json` 无 `type: number/integer` 属性；`run_manifest.schema.json` 的 8 项 `required` + `additionalProperties:false`；`runtime_resources.json` 的三个冻结系数。

**没做（诚实边界）**：
- **没有编译、没有运行任何二进制、没有执行文档给出的任何复跑命令**（它们的路径大多已不存在）。所有「代码为准」的判定来自源码阅读 + `grep` + `json.load`/`yaml.safe_load`。
- **没有取任何外部文献原文**。本车道唯一外部文献是 `ARTIFACTS.md:137` 的 Rousseeuw & Croux 1993（DOI `10.1080/01621459.1993.10476408`）、`NUMERIC.md:201-207` 的 JCGM 100:2008(GUM) / IVOA REC-HIPS-1.0 / FITS Standard 4.0 / IEEE 754-2019、`CONFIG.md:404-408` 的 REC-HIPS-1.0 与 FITS 标准 4.0。**本轮全部未取原文** ⇒ 按红线**记为「核对不到」，不凭印象判定，也不计入问题数**。
  ⚠️ **但 N10 的修法依赖 IEEE/量纲代数而非外部文献**（`144·nside⁴/(16π²)` 的对数展开 + 表内两行数值反证），推导已在条目内给全，不依赖未核文献。
- **未逐行读完** §1.2 的 33 份；对它们不做独立判断，凡引用一律附复跑命令。
- **6 个子代理的交付件在本交付件写出时均未回传** ⇒ §2、§3、§4 无一条采信它们。
- `docs/engineering/build/**` 四份不在版本控制 ⇒ **我无法用 `git diff` 判定止血轮在其中的改动边界**，凡涉这四份我只判「现状为真」。

### 7.2 关键复跑命令（可直接粘贴）

```bash
cd "/workspace/Astro CS Database"

# ===== 跨切面四处数字（本轮真值；我与子代理 29d713ba 各自独立复跑，数字相同）=====

# 1) 死路径（沿用第 2 轮正则口径，本轮读数 40；第 1 轮 51、第 2 轮 39）
python3 - <<'PY'
import re,os
root=os.path.abspath('.')
pat=re.compile(r'`((?:eng|lib|docs|run|实验)/[A-Za-z0-9_./\-]*\.(?:md|json|py|yaml|yml|h|hpp|c|cpp|cmake|txt|js))`|'
               r'`((?:\.\./|\./)[A-Za-z0-9_./\-]+\.(?:md|json|py|yaml|h|hpp|c|cpp|txt))`')
bad={}
for dp,dn,fn in os.walk('docs/engineering'):
    for f in fn:
        if not f.endswith('.md'): continue
        p=os.path.join(dp,f); base=os.path.dirname(os.path.abspath(p))
        for i,line in enumerate(open(p,encoding='utf-8',errors='replace').read().splitlines(),1):
            for a,b in pat.findall(line):
                m=(a or b).split('#')[0]
                if not m: continue
                c=os.path.normpath(os.path.join(base,m)) if m.startswith('.') else os.path.normpath(os.path.join(root,m))
                if not os.path.exists(c): bad.setdefault(m,[]).append(f"{os.path.relpath(p)}:{i}")
print("死路径:",len(bad))
PY
# → 40。第 2 轮 39。+1 的两条候选都落在止血轮新写的内容上：
#   ../data/ARTIFACTS.md            <- docs/engineering/UNIFIED_OBJECTS.md:52   （止血新增段，路径解析到不存在的 docs/data/）
#   docs/engineering/BUILD_GRAPH.md <- docs/engineering/build/BUILD_GRAPH.md:119（止血新增复算节记录的生成器常量）

# 2) 机械锚 / 空锚 / HTML 注记
grep -ro '」一节' docs/engineering --include=*.md | wc -l   # 389（第1轮 521 / 第2轮 383）
grep -ro '「」'    docs/engineering --include=*.md | wc -l   # 0  ✅
grep -rn '<!--'   docs/engineering --include=*.md | wc -l   # 6（全部在 BUILD_GRAPH.md，是生成器机器块定界符，非订正注记）
grep -rnoE "见第 ?[0-9]+ ?节" docs/engineering --include=*.md  # 1 处：testing/TEST.md:36「取值见第 4 节」（第1/2轮只统计「」一节」形态，漏此形态）

# 3) 悬空文献引用
python3 - <<'PY'
import os,re
for dp,dn,fn in os.walk('docs/engineering'):
    for f in sorted(fn):
        if not f.endswith('.md'): continue
        p=os.path.join(dp,f); t=open(p,encoding='utf-8').read()
        i=t.rfind('参考文献')
        if i<0: i=t.rfind('## 引用')
        if i<0: continue
        used=set(re.findall(r'\[(\d+)\]',t[:i])); listed=set(re.findall(r'^\[(\d+)\]',t[i:],re.M))
        if used-listed: print("悬空",p,sorted(used-listed,key=int))
PY
# → 真悬空 0（T06-r2 报的 PERFORMANCE_MODEL [2][3] 与 SECURE_LOADER [4][5][6] 均已闭合；
#   PUBLIC_API.md 的 [6][16][32][36][64][256][512][1024] 逐个打开为 C 数组维度，见下）

# 4) INDEX 一致性 + 「active 但不在 git」新指标
python3 - <<'PY'
import yaml,os,collections,subprocess
d=yaml.safe_load(open('docs/DOCUMENT_INDEX.yaml',encoding='utf-8'))
act=d['doc_index']['active']; paths=[e['path'] for e in act]
print("active =",len(act),"| 重复:",[p for p,c in collections.Counter(paths).items() if c>1] or 0,
      "| 悬空:",[p for p in paths if not os.path.exists(p)] or 0)
disk={os.path.relpath(os.path.join(dp,f),'.') for dp,dn,fn in os.walk('docs')
      for f in fn if f.endswith(('.md','.yaml'))}
print("树有索引无(排除README):",[p for p in sorted(disk-set(paths)) if not p.endswith('README.md')] or 0)
t=set(subprocess.run(['git','-c','core.quotepath=false','ls-files'],capture_output=True,text=True).stdout.split('\n'))
print("【新指标】active 但不在 git:",[x for x in paths if x and x not in t])
PY
# → 148 | 0 | 0 | 0 ；【新指标】3 条 build/ 正本

# ===== 公式重推（三条）=====

# N10：dex 斜率系数 8 应为 4
python3 -c "
import math
for e in (18,16):
    n=2**e
    print(f'2^{e}: 系数8={8*math.log10(n)-2*math.log10(4*math.pi/12):.4f}',
          f'系数4={4*math.log10(n)-2*math.log10(4*math.pi/12):.4f}',
          f'真值={math.log10(1/(4*math.pi/(12*n*n))**2):.4f}')
print('翻倍倍率 =',2**4,' dex 增量 =',math.log10(16))"
# → 2^18: 43.3083 / 21.6341 / 21.6341 ；翻倍倍率 16 、dex 增量 1.2041

# N11：float32 下溢判红边界
python3 -c "
import struct
f=lambda x: struct.unpack('f',struct.pack('f',x))[0]
for v in (2.0**-150, 2.0**-150*(1-1e-9), 2.0**-150*(1+1e-9), 2.0**-149):
    print(v, '-> f32 =', f(v))"
# → 2^-150 -> 0.0（tie-to-even）⇒ 文档的 "< 2^-150" 应为 "<= 2^-150"

# N12：inner_omp 退化为 1 的前提
python3 -c "
for n,L in ((2,2),(1,2),(2,8),(4,8),(8,16)):
    F=L  # 帧级未被内存压低
    in_flight=min(n,F); I=max(1,L//in_flight)
    print(f'n={n} L={L}: in_flight={in_flight} inner_omp={I}  (退化为1? {I==1})')"
# → n<L 时 inner_omp>=2 ⇒ 「帧级未压低 ⇒ 退化为 1」需补 n>=frame_workers

# 已验证项：alpha 阈值 + 内存闸门 + run_manifest 三方
python3 -c "
import math,json
vf=1e-12
print('a0',repr(math.sqrt(2**-150/vf)),'a1',repr(math.sqrt(2**-149/vf)))
print('a2',repr(math.sqrt(2**-126/vf)),'a3',repr(math.sqrt(1/(vf*3.4028234663852886e38))))
d=json.load(open('eng/contracts/schemas/run_manifest.schema.json'));print('run_manifest required',d['required'],'addProps',d['additionalProperties'])
print('provenance numeric props',[k for k,v in json.load(open('eng/contracts/schemas/unified/provenance.schema.json'))['properties'].items() if v.get('type') in ('number','integer')])
r=json.load(open('eng/packaging/config/runtime_resources.json'))['frame_memory_gate'];print('gate',r)"
sed -n '528,572p' lib/infrastructure/cli/commands.cpp      # 生产 run_manifest 字段名族与落盘名
sed -n '118,124p' docs/engineering/resources/PERFORMANCE_MODEL.md

# ===== 止血效果抽查 =====
grep -rn "架构塑造方向是 Windows\|不作为 Windows 发布性能结论" docs/     # 0  ✅ R1 真收敛
grep -rn "0\.75" docs/engineering --include=*.md                     # 1：PERFORMANCE_MODEL.md:105（同类漏改）
grep -n "SD-18" docs/ ; grep -rn "7 档" docs/engineering             # 双 0  ✅ R13 闭合
grep -rn "SNR-PREC" docs/                                           # 3 条全在 science/algorithms/HIPS_WRITER.md（行号形态）
grep -cE "^## [0-9]" docs/engineering/contracts/SCHEDULER.md        # 0  ← 活合同 §1/§3 仍悬空
grep -nE "^#{1,3} " docs/engineering/UNIFIED_OBJECTS.md              # 4b(:99) 仍在 4a(:118) 之前
sed -n '3,5p;45p;50p;52p;77,78p;132p' docs/engineering/UNIFIED_OBJECTS.md
sed -n '45p;47p' docs/engineering/data/ARTIFACTS.md
sed -n '13,14p;16,17p' docs/engineering/build/BUILD_NODES.md
sed -n '105p' docs/engineering/resources/PERFORMANCE_MODEL.md
sed -n '75p;83p;97p;107p;146,147p' docs/engineering/standards/NUMERIC.md
sed -n '121p;123p;125p' docs/engineering/architecture/DATA_FLOW.md
sed -n '40p;353p' docs/engineering/contracts/CONFIG.md
sed -n '24p;32p;78p;90p;109,111p' docs/engineering/contracts/MANIFEST_VERIFY.md
grep -rn "docs/engineering/[A-Za-z0-9_./-]*\.md" lib/ eng/ CMakeLists.txt | wc -l   # 活合同引用面全量

# 括号配平
python3 -c "
import os
for dp,dn,fn in os.walk('docs/engineering'):
    for f in sorted(fn):
        if not f.endswith('.md'): continue
        p=os.path.join(dp,f)
        for i,l in enumerate(open(p,encoding='utf-8').read().splitlines(),1):
            if l.count('（')!=l.count('）'): print(f'{p}:{i} {l.count(\"（\")}/{l.count(\"）\")}')"
# → CONFIG.md:107(2/3) :125(6/7)、MANIFEST_VERIFY.md:110(0/1) 三处真失衡仍在；其余为跨行折行

# 活合同引用面悬空率
python3 -c "
import re,subprocess,os
p=sorted(set(re.findall(r'docs/engineering/[A-Za-z0-9_./-]*\.md',
    subprocess.run(['grep','-rhoE','docs/engineering/[A-Za-z0-9_./-]*\\.md','lib','eng','CMakeLists.txt'],
                   capture_output=True,text=True).stdout)))
miss=[x for x in p if not os.path.exists(x)]
print(f'被引唯一路径 {len(p)}  悬空 {len(miss)}  ({len(miss)/len(p):.0%})')
print('EXIST:',[x for x in p if os.path.exists(x)])"
```

### 7.3 收敛声明

**本车道未达成收敛。** 止血轮在两个指定目标上有效（三对矛盾中两对真闭合、R1/R6/R10/R13/R17 五条重复断言真收敛），但**新造 2 对矛盾、引入 1 条死路径、留下一条已过期的证据引用**，且**第 2 轮的 11 条未闭合项全部仍在**。本轮新增 **14 条**，其中 **2 条是方向性公式错误**（`NUMERIC.md` 的 dex 斜率系数 8→4、float32 下溢判红边界 off-by-one；同族的 `inner_omp` 退化为 1 漏 `n ≥ frame_workers` 前提）—— 这三条的共同特征是：**单点数值全对、关系式全错**，因此第 1 轮的 V-3（只验单点即判「算术无误」）与第 2 轮的 3-5（只验前提未验同句后件）都恰好漏掉。

**下一轮建议顺序**（不改，只建议）：
0. **P2（止血越界改权威顶点）先查授权来源**：`git show --stat 18dd6c64` 含 `docs/ACSD_DESIGN.md`(20±) 等 24 份车道外文件，而 `ACSD_DESIGN.md:23` 明写「修改本文档由项目负责人批准」，T07 交付件一处未登记。**这是唯一一条「不是文档缺陷、而是流程违规」的问题**，须先定性再谈其余。
1. **P1（`TRACEABILITY.md:266` UPM 权重第三份副本）单行可闭合**：与 `ARTIFACTS.md:112` 同批改；它是第 2 轮 L-9 同一断言的第三份，**且状态标 `VERIFIED`**（核的 `upm.cpp` 与该行字面相反）。
2. **N10/N11/N12 + P4 四条公式同批改**（`NUMERIC.md` 的 dex 斜率系数 8→4、float32 下溢判红边界 off-by-one、`inner_omp` 退化为 1 漏 `n ≥ frame_workers`、`DATA_FLOW:125` 丢 `num_threads = inner_omp` 合取前提）。四条都是**单点数值全对、关系式或前提全错**，前两轮与止血轮三轮全部漏检。
3. **N1 精度归属三处旧句 + N2 裁-29 过期证据**：止血轮自己的尾巴，范围最小、收益最大；且 P2 显示顶点现已把对象定义委托给这份自相矛盾的文件。
4. **N13 活合同引用面 73% 悬空**（79 条被引路径 / 58 条不存在 / 12 份正本零登记）：按 `AGENTS.md §4` 主修面在代码侧；文档侧可做的是 `SCHEDULER.md` 恢复 `## 1`–`## 6` 编号（5 分钟闭合一条活合同节号）+ 各活合同登记引用面缺口。
5. **29 条 `eng/tests/**` 死引用**（占死路径 72%）：索引侧已清、文档正文一条未清，与前两轮根因同型，可脚本化后逐条留痕。
6. **`.gitignore:20`** 锚到 `/build/` 并放行 `docs/engineering/build/`：前置项，不解决则 build/ 四份的改动永远不可复核、索引门的「悬空 0」在干净克隆上恒为假。
7. **U-1（命名块载体）裁决**：`AGENTS.md:88` + `ACSD_DESIGN.md:381-383` 说块跨节点，`PIPELINE_BLOCK.md:17` + `DATA_FLOW.md:9/14` + 注册表 `carrier_contract` 说块不跨节点，且 `PIPELINE_BLOCK.md` **文件内部** `:17`（不跨节点）vs `:37`（`lifecycle=frame` 帧内跨节点）vs `:60`（随块流转）三方并存。**止血把 `DATA_FLOW.md` 迁到与 `PIPELINE_BLOCK.md` 同向，车道内矛盾消失，但代价是把「与权威链上层相反」的写法升格成统一口径，且第 2 轮要求的那句「本条与权威链上层冲突，已登记」四处均无 ⇒ 从「车道内矛盾」恶化为「统一地相反且未标注」。需负责人裁决，我不能代裁。**

---

## 8. 子代理分头审的复核结果（4 份已回传，逐条复核）

### 8.1 回传状态与我的复核深度

| 子代理 | 车道 | 状态 | 我复核到什么程度 |
|---|---|---|---|
| `29d713ba` | `build/` 四份 + 四处数字 | **已回传** | 四处数字**我全部亲自复跑，数字逐项相同**；B-4/B-5 我复跑；B-7/B-8 我只核顶点侧 |
| `986fcbdf` | `architecture/`+`data/`+`governance/`+`UNIFIED_OBJECTS` | **已回传** | A1/A5/A6 我亲自验证；**B14 我实测推翻** |
| `438c2ea9` | `standards/`+`resources/`+`testing/` | **已回传** | N-A1/N-B1 我亲自复跑；N-A2 我亲自验证 |
| `844a9205` | `architecture/`+`data/`+`governance/`（与 `986fcbdf` 重复派单的独立盲审） | **已回传** | F-1/F-2/F-5 我亲自验证 |
| `62bb46a2` / `ac6f0f6b` | `contracts/`+`api/`（重复派单，两个独立上下文） | **均已回传** | 两者**独立地各自报出** `MANIFEST_VERIFY` 的 M-1/M-2/N-2 未闭合与 `LOG_AND_ERROR`/`SCHEDULER` 的活合同节号断裂 ⇒ 该组按**双盲一致**采信 |
| `c2d8e211` | `standards/`+`resources/`+`testing/`（与 `438c2ea9` 重复派单） | **已回传** | 其 N-A1 与 `438c2ea9` 的 N-A1、我的 N10 **三路独立**得出同一结论（dex 斜率系数 8→4）⇒ **三盲一致** |

**盲审交叉验证的结果（我误发的重复派单客观上提供了三处盲复核）**：
① `986fcbdf` 与 `844a9205`（`architecture`+`data`+`governance`）**各自独立发现了 `TRACEABILITY.md:266` 的 UPM 权重第三份副本**（A1 / F-1，结论与依据逐字相同）⇒ **双盲一致**；
② `62bb46a2` 与 `ac6f0f6b`（`contracts`+`api`）**各自独立报出** `MANIFEST_VERIFY` 的 M-1/M-2/N-2 未闭合 ⇒ **双盲一致**；
③ `NUMERIC.md` 的 dex 斜率系数错误：我的 N10、`438c2ea9` 的 N-A1、`c2d8e211` 的 F-A **三路独立**得出同一结论 ⇒ **三盲一致**。
按第 1 轮确立的口径，这三条按盲复核一致采信。

**我自己也发现的、三个子代理都独立复核到的**：`NUMERIC.md` 的 dex 斜率系数错误（我记 N10、`438c2ea9` 记 N-A1）—— 两条路径独立得出同一结论（系数 8 应为 4），**按双盲一致采信**。

### 8.2 我采信并亲自验证的子代理发现（**并入 §4 计数**）

| 编号 | 出处 | 位置 | 问题 | 我的独立验证 | 并入 |
|---|---|---|---|---|---|
| **P1** | `986fcbdf` A1 / `844a9205` F-1（**双盲**） | `governance/TRACEABILITY.md:266` | **UPM 权重第三份副本未止血，且状态标 `VERIFIED`**：该行 title 写「production UPM 权重 = **quality × control_reliability × control_ivar**」，把几何可靠性放进分子乘积。与 `ARTIFACTS.md:112`「**几何可靠性不在分子**」、`DATA_FLOW.md:142`、以及代码 `upm.h:174-176` 的注释**三处相反** | **我亲自 `sed -n '266p'` 取到该行逐字**；三份一级正本给同一物理量两种互斥分子口径 | **§4-N15（最高优先）** |
| **P2** | `986fcbdf` A5 | 提交 `18dd6c64` | **止血轮越界改了权威顶点与 24 份车道外文件，且交付件未登记**：T07 §0/§2/§7.2 声明「只改 `docs/engineering/**` 与 `docs/DOCUMENT_INDEX.yaml`」，实际 stat 含 `docs/ACSD_DESIGN.md`(20±)、`docs/GLOSSARY.md`(47±)、`docs/README.md`、`docs/detail/**` 18 份、`docs/science/INTEGRATION.md`。而 `ACSD_DESIGN.md:23` 明写「修改本文档由项目负责人批准」 | **我亲自 `git show --stat 18dd6c64` 复现全部文件清单**；**并亲自 `git diff c0bec7f7 18dd6c64 -- docs/ACSD_DESIGN.md` 读了顶点三处改动**：`:141` 实验单元指向改写 + 新增 P1–P5/phase 记号段；`:370-379` mermaid 节点 ID `P1/P2/P3 → NRM/MZC/EXP`；`:564` 附录 A 补 `depth_m5` + **新增「对象定义以 `docs/engineering/UNIFIED_OBJECTS.md` 的对照表与 canonical schema 为准」** | **§4-N16**（末项加重了 §2.3 矛盾一：顶点现在把对象定义委托给一份自相矛盾的文件） |
| **P3** | `438c2ea9` N-A2 | `resources/cpu/ISA_VARIANTS.md:29-32` | **「诚实登记」的三个 hips-bulk-transform 增益数，在它自己点名的证据 CSV 里一个都不存在**：文档写 ISA-001/002 = **+28.2%/+28.3%**、ISA-003 = **+39.8%**，且明写 ISA-003 的来源是 `ISA-003/MEASUREMENTS.csv` | **我亲自 `grep -i hips` 三个 CSV**：`ISA-001 +42.2` / `ISA-002 +16.4（NOT_SHIPPED(avx子集,avx2主导)）` / `ISA-003 +30.9（SHIP(avx2)）`。**三个数逐个对不上，且自称的来源件不含自己写的数** | **§4-N17** |
| **P4** | `844a9205` F-2 | `architecture/DATA_FLOW.md:125` | **引述侧丢了合取前提**：「把 `K` 取成 `num_threads` 只是其中一条充分条件」与「总份数在该区间内不变」两句，**只有在 `num_threads = inner_omp` 时才成立**；正本 `PERFORMANCE_MODEL.md:154` 明写该合取前提（「`K = num_threads` **且 `num_threads = inner_omp`** 时」），引述侧把前提丢了 | **我亲自对读**：`:125` 末句无前提；`:154` 有前提。**我的重推**：满宽 `⇔ min(num_threads,K) ≥ inner_omp`，代入 `K = num_threads` 得条件退化为 `num_threads ≥ inner_omp`；若 `num_threads < inner_omp` 则 `K = num_threads` **不是**充分条件。「总份数」= `in_flight × min(num_threads,K)`，在 `[inner_omp, num_threads]` 段随 `K` 变化 | **§4-N18** |
| **P5** | `438c2ea9` N-B1 | `resources/PERFORMANCE_MODEL.md:18-20` | **R11 的第三处漏网**：`:19` 写「最大相对偏差不超过**第 4 节**冻结的双精度非归约档容差」，但该篇 `grep -cE "^## [0-9]"` = **0**，无任何编号章节 ⇒ 「第 4 节」在本篇无解析目标 | **我亲自 `sed -n '18,20p'` 取到该句 + `grep -cE "^## [0-9]"` 得 0** | **§4-N19** |
| **P6** | `986fcbdf` A7 / `438c2ea9` N-A7 / `844a9205` F-13 | `contracts/LOG_AND_ERROR.md:120`、`architecture/DATA_FLOW.md:170` | **「未登记的偏差按判红处理」在两处挂在不含该类条目的登记面上**：L-10 已报 `LOG_AND_ERROR.md:120`；本轮**新增第二处 `DATA_FLOW.md:170`**，且该篇全文未点名登记面 | **我亲自读 `ERROR_MODEL.md:149-155` §9 全文**（该节只说「登记面 = `TRACEABILITY.md` 的 `error_codes` 列」+ 一个 `ERR-P2-UPM-001` 举例，**零条域映射偏差**）；`DATA_FLOW.md:170` 我自己逐行读过，原文「实现面与本文件条款的偏差逐条登记……未登记的偏差按判红处理」**确实未点名登记面** | **§3.4 L-10 条目扩为两处** |
| **P7** | `844a9205` F-16 | `UNIFIED_OBJECTS.md:120` + §4a 全表 | **§4a 的前提是假陈述**：「`MODULE_MAP` 引用 **22 个 DATA ID**」，而 `MODULE_MAP.md` 全文**零个 DATA ID**（`grep -c "DATA-"` = **0**） | **我亲自跑 `grep -c "DATA-" docs/engineering/architecture/MODULE_MAP.md` → 0**（我原先逐行读过该篇的引用面，未核这个数） | **§4-N20** |
| **P8** | `438c2ea9` N-B12 | `standards/NUMERIC.md:133` | **批量替换残渣，本车道外的一处我漏掉的**：「唯一正本 = `../data/PHASE_PRODUCT_EXCHANGE.md` **a** 的 `invalid_handling` 块」—— 孤立的 ` a `，与 L-2/L-5 同源。目标块真实存在 | 我逐行读过 `NUMERIC.md` 全文**未捕获这一处**（我的正则只找 `，a）`/`、a「` 形态） | **§3.4 L-5 条目扩为两文件八处** |
| **P9** | `62bb46a2` F-4 | `contracts/CONFIG.md:229` vs `:409` | **止血 R15 引入的新错：正文引的文献条目与被引内容不是同一处**。`:229` 引 `[3]` 支持「最高设计的**双平台发行**一章（Windows 10+ amd64 / Linux amd64…）」；`:409` 的 `[3]` 条目却写「最高设计的**输入合同与三类配置**两章」。形式合规（论文格式），实质引错 | **我亲自 `sed -n '229p'` 与 `sed -n '409p'` 对读**（两行都在我逐行读完的 `CONFIG.md` 内，我原本只核了括号配平与节名存在性，**没核文献条目与正文是否对应**） | **§4-N21** |
| **P10** | `62bb46a2` F-2 | `contracts/CONFIG.md:391` | **止血 R13 引入的新错：外链方向反了**。该行写「`p2_reject_stack_ex`（三档自动选择，**阈值表见下条**）」，但唯一正本「排异档位映射」在 **`:353`**，位于 `:391` **之上**。旧文本自包含，止血改成外链后方向反了 | **我亲自 `grep -n "排异档位映射" CONFIG.md`** → 只有 `:306`（写「见**下方**，方向正确」）与 `:353`（正本）；`:391` 的「见下条」方向错。**另** `62bb46a2` 指出 `:391` 的 `TEST-REJ-*` 是通配符、不是可核 ID（我未逐行读 `TRACEABILITY.md`，转登记） | **§4-N22** |
| **P11** | `ac6f0f6b` N-3 | `contracts/CONFIG.md:38` vs `:163` | **同一文件内「改了正文、没改表格」**：`:163` 已如实写「该 schema **未被任何生产写出点消费**」；但 `:38` 的「三类配置（现场清单）」表仍把 `run_manifest` 的文件列成该 schema、写入者列成「**运行时（每次运行冻结）**」⇒ **止血在这一份文件里也只改了一侧** | **我亲自 `sed -n '38p'` 取到该行逐字** | **§4-N23** |

### 8.3 我**否决 / 收窄**的子代理结论

| # | 被否决或收窄的结论 | 出处 | 我的理由 |
|---|---|---|---|
| V1 | 「`PERFORMANCE_MODEL.md:53-54` 的数值键 `frame_memory_gate.bytes_per_pixel` / `safety_frac` **在 `runtime_resources.json` 不存在** ⇒ 收敛后 116.0/0.75 在文档层彻底消失」 | `986fcbdf` B14 | **推翻 —— 子代理的取证有误。** 我 `json.load` 并列出全部 31 个顶层键，**该键在**：`frame_memory_gate: {'bytes_per_pixel': 116.0, 'safety_frac': 0.75, 'authority': '…module_adapters.cpp p1_memory_cap（PERF-P1 标定，P1-CONCURRENCY-CALIB-01）', 'note': '…CHK-BUDGET-SINGLE-SOURCE 与实现侧字面量逐位比对。'}`。子代理疑为 `json.load` 后只遍历了嵌套层而漏掉顶层键。**⇒ B14 整条不成立**，其推出的「唯一数值源指空」也不成立 |
| V2 | 「`DATA_FLOW.md:125` 的「`K` 没有唯一最小取值」字面为假」 | `844a9205` F-3 | **收窄 —— 我不判为硬错，判为措辞歧义。** 两种读法并存：(a)「可行区间 `[inner_omp, ∞)` 的下确界唯一 = `inner_omp`」（子代理读法，字面成立）；(b)「`K = num_threads` 不是被论证出来的唯一取值」（止血本意，上下文明确）。原文紧接「可行区间内的任一取值给出同一满宽」，读者多半取 (b)。**建议改法同子代理（写成「`K` 的最小可行取值是 `inner_omp`；`K = num_threads` 只是其中一条取值」），但我把它记为措辞项，不计入方向性公式错误** |
| V3 | 「`UNIFIED_OBJECTS.md:3-5` 的 `> ` 抬头块**不构成缺陷**（`DOCUMENT_GOVERNANCE.md:65` 强制要求『上游：…』区）」 | `986fcbdf` V1 / `844a9205` §6-3 | **部分接受。** 我亲自读 `DOCUMENT_GOVERNANCE.md:65`：「细节面每篇抬头有「上游：〈上层文档条款号与标题」区；**科学线与工程线每篇抬头有「上游：最高设计条款号与标题」区**」⇒ **「上游：」那两行确实不违反 `AGENTS.md` §5**，我原判应改为「**强制抬头区不属『元信息块』**」。**保留的部分**：`:3-5` 是**三行**，`DOCUMENT_GOVERNANCE` 只授权「上游：…」区；`:5` 的第三行「现行对象集 = 13 个」与「上游」无关，仍是多余元信息；`:3`/`:5` 的「「数据对象」一节**（数据对象）**」节名后重复仍在。**⇒ §4-N5 相应收窄** |
| V4 | 「`build/BUILD_GRAPH.md:81/82/95/96` 的伪节名最可能是 `两层链：目标链与生产节点序`」 | `29d713ba` B-1 改法 | **否决其指定改法。** 我逐字读过 `ARCHITECTURE.md:58` 确为该节名，但该节讲的是**科学步骤与生产节点序的两层链**，与 `BUILD_GRAPH.md` 的 target/来源列是否同一，**我没有内容证据** ⇒ 改法只能由代码侧确认，审稿侧不得指定。**问题本身（伪节名 + `:123` 自称「逐字一致」被推翻）我采信**（§4-N14） |
| V5 | 「机械锚 383→389 的 +6 全部归因于止血轮新写的内容」 | `29d713ba` V1 的归因部分 | **数字采信、归因不采信。** `docs/engineering/build/**` 不在 git，我**无法用 `git diff` 判定**哪些是止血轮新写的。可核事实只有：今日 389 > 第 2 轮 383，**单调增**。 |
| **V6** | 「**R9 未实际生效** —— `18dd6c64` 对 `UNRESOLVED.md` 只 `+6 −2`（新增裁-28/29），`:100` 行未在 diff 中」 | `ac6f0f6b` §8 诚实边界 | **推翻 —— 子代理只看了 diff 尾部。** 我 `git diff c0bec7f7 18dd6c64 -- docs/engineering/governance/UNRESOLVED.md \| tail -12` 亲自复现，diff 末段逐字含：<br>`-[3] 内部文档 \`AGENTS.md\`，总章程与查证流程 [3]。`<br>`+[3] 内部文档 \`AGENTS.md\`，总章程与查证流程。`<br>⇒ **R9 确实生效**，我 §2.2 对它的采信成立。（因补建两条，条目现落在 `:101` 而非 `:100`） |

### 8.4 子代理提出的、本车道我未独立验证的发现（**转登记，我不背书**）

以下条目**我未亲自复核**，只转登记其位置与结论，**不计入 §4 的 14 条**，也不由我背书：

`438c2ea9`：N-A3（`ISA_VARIANTS.md:26-27/:36` 把 AVX 无 FMA 批当 AVX2+FMA 批；ISA-003 calibration = **−130.7%** 仍判 `SHIP(avx2)`）、N-A4（`VALIDATION_EVIDENCE.md:245/251/266` 三处伪引用被第 1 轮订正**升级成看似精确的自指 § 锚**）、N-A6（`resources/cpu/**` 15 条 `eng/tests/**` 死路径）、N-A8（`VALIDATION_EVIDENCE.md:359`）、N-B2~N-B11、N-B13~N-B26（含 N-B15 满宽条件、N-B17 拟合残差 **+2.520% > 声明的 ±2.5%**、N-B18 `hard_fail_criteria` 三条 vs 文档四条）；
`844a9205`：F-8~F-11、F-17~F-25（含 **命名块载体已从「车道内矛盾」恶化为「四处统一地与权威链上层相反且未标注」**、F-20 `PHASE_PRODUCT_EXCHANGE.md` Phase 1 分母符号 `W_p`/`D_p` 混用）、C-2（`integer` 档在顶点归属律里无归属依据）、C-3（`coverage` 五义）、C-4（`1.482602218505602` 逐位复算通过）；
`986fcbdf`：B1（`UNIFIED_MODEL.md`「数据对象（各自具名）」一节不存在，本车道 23 处引用）、B2~B13、B15~B36；
`29d713ba`：B-6（`RELEASE.md:92` 的 `preset-contract.schema.json` 实为 `preset-contract.json`）、B-7（`RELEASE.md` 交付包不含 so/schemas/配置，与顶点第 11 章矛盾而安装树装全了）、B-8（`schemas/`、`launch/` 根不存在）。

⚠️ 其中 **`N-A4`（VALIDATION_EVIDENCE 三处伪引用被订正升级）** 与 **F-8/F-9/F-10（TRACEABILITY 台账的 8 行 `TEST @ MISSING` 却判 `VERIFIED`、1 条悬空登记页、4 个互不相等的模块计数）** 若属实，量级不低于我本报告的 N1–N14，**建议前台优先派单复核这两处**。

---

## 9. 交付件

唯一交付件：`/workspace/Astro CS Database/run/GOVERN-08/审核包-R2/T06-r3-审稿-DOC-ENG.md`（本文件）。

**未改动任何文档**；全程无 git 写操作（git 仅 `status`/`diff`/`ls-files`/`check-ignore`/`log`/`show` 只读；中文路径一律 `git -c core.quotepath=false`）。未 amend、未 force-push、未 add。

**计数的最终口径**：
- **我逐行读完 17 份 / 2 452 行**，逐段 1 份，未读 33 份逐份列出。
- **我自己的新发现 14 条**（§4 N1–N14）+ **采信子代理并亲自验证 11 条**（§8.2 P1–P11，编号 N15–N23 计入）⇒ **本车道共 25 条我已验证的新发现**。
- **子代理另报 80+ 条未由我验证**，如实列于 §8.4，**不计入、不背书**。
- **第 2 轮 20 条相关条目中闭合 9 条、未闭合 11 条**（§3.4）。
- **四处跨切面数字**：死路径 **40**、机械锚 **389**、真悬空文献引用 **0**、INDEX **148 / 0 重复 / 0 悬空 / 0 树有无**，另新指标 **active 但不在 git = 3**。四处我与子代理 `29d713ba` **各自独立复跑、数字逐项相同**。
- **子代理 7/7 全部回传**（含我误发的 3 个重复派单）。盲复核一致的三处：`TRACEABILITY.md:266`、`MANIFEST_VERIFY` 未闭合、`NUMERIC.md` dex 斜率。
- **我自己推翻的子代理结论 2 条**（§8.3 V1「`runtime_resources.json` 无 `frame_memory_gate`」= **错**，该键在；V6「R9 未生效」= **错**，diff 里有）；**收窄 2 条**（V2「`K` 没有唯一最小取值」判为措辞歧义非硬错；V3 `UNIFIED_OBJECTS.md:3-5` 抬头块部分豁免）。
- **本轮不是零发现轮**；收敛判据（连续两轮无新增实质问题）**未达成**。
