# T07 · 重复断言收敛（车道 `docs/engineering/**`）

收敛者：独立子代理。**先止血**：把重复断言收敛为唯一正本，其余改为引用或删除。
只改 `docs/engineering/**` 与 `docs/DOCUMENT_INDEX.yaml`；**全程无 git 写操作**（中文路径一律 `git -c core.quotepath=false`，git 仅 `status`/`diff`/`ls-files`/`check-ignore`/`log`/`show` 只读）。

权威链依据（按顺序通读）：`AGENTS.md`（127 行全文）→ `docs/ACSD_DESIGN.md`（568 行全文）→ 第 2 轮五份审稿（`T06-审稿-SCI-unified-calibration-detection.md`、`T06-r2-审稿-DOC-DETAIL-GLOSSARY.md`、`T06-r2-审稿-noise_snr.md`、`T06-r2-审稿-psf-drizzle-resample.md`、`T06-r2-审稿-DOC-ENG.md`）。

---

## 0. 收敛总账

处置口径：**唯一正本** = 该主题在本车道内保留并订正的那一份；**删除副本/改指** = 其余出现处；**同名不同义保留** = 说的是不同的事，明确区分、不合并。

| # | 重复断言 | 出现位置（`文件:行`） | 处置 | 确认信息未丢失的依据 |
|---|---|---|---|---|
| **R1** | **平台角色**：哪个平台做开发/构建/合成/真实数据终验，谁复验、谁终审 | `build/BUILD_NODES.md:13`、`:14`、`:16`（旧副本）<br>`architecture/ARCHITECTURE.md:123`–`:126`（正本）<br>`build/RELEASE.md:126`（发布门槛面，同向）<br>`ACSD_DESIGN.md:476`–`:478`（权威顶点） | **正本** = `ARCHITECTURE.md`「平台与交付形态」；`BUILD_NODES.md:16` **删除副本并改指**；`:13/:14` 节点分工表**订正**（同表内「不承担」格）；`RELEASE.md:126` 保留（同向，不同条款面） | 被删句的三块信息全部有落点：① 平台角色分派 → `ARCHITECTURE.md:124` 原文在位；② 节点分工（谁编译/谁复验/谁静态分析）→ `BUILD_NODES.md:13/:14` 表保留并**扩写**（Linux 侧补「开发迭代、合成、真实数据终验」，Windows 侧「不承担」补「真实数据终验」）；③ Linux 读数不作 Windows 发布性能结论 → 正确形态由 `RELEASE.md:126`（Linux 终验 + Windows 复验**同为**发布候选门槛）与 `ARCHITECTURE.md:124`（Linux 终验后交 Windows 复验）承载。复现：`grep -rn "架构塑造方向是 Windows\|不作为 Windows 发布性能结论" docs/` → **0 命中** |
| **R2** | **逐对象应归属精度**：`frame_snr` | `UNIFIED_OBJECTS.md:41`（`float64`）<br>`data/ARTIFACTS.md:55`（`f32\|f64`） | **同名不同义保留 + 明确区分**（见 §2-A） | 两侧信息都留：前者问「应归属哪一档」（保留），后者问「DataArtifact 承载面的标量形态」（保留）。已在两处各写一句「三个问句各答一处，不得跨面比较」，并在 `UNIFIED_OBJECTS.md:50` 新增读法段点名另外两面 |
| **R3** | **逐对象精度**：`provenance` | `UNIFIED_OBJECTS.md:48`（非数值元数据）<br>`data/ARTIFACTS.md:62`（`int`） | `ARTIFACTS.md:62` **删副本**（`int` 是真错） | `provenance.schema.json` 的 `required` 九项逐一 `json.load` 解析：`unified_object`/`object_schema_id`/`schema_version`/`precision`/`object_weight_*`/`provenance`/`input_hashes`/`run_id`/`product_type_id`，**无任何数值属性**（`precision` 自身是 `enum`）。故承载面无标量数值面，写 `int` 无据。改为「非数值（字符串 / 键值对象 / 字符串数组，无标量数值面）」——该表述逐条对应 schema 实际类型，不丢信息 |
| **R4** | **精度归属律本身**（稠密单精度 / 稀疏与元数据双精度 / JSON 覆盖） | 权威顶点 `ACSD_DESIGN.md:162`–`:164`<br>`UNIFIED_OBJECTS.md:50`（越权改写：把顶点 `manifest` 换成 `控制点`）<br>`UNRESOLVED.md`（**原无条目**，指针悬空） | `UNIFIED_OBJECTS.md:50` **改指 + 逐字恢复顶点原文**；悬空指针**补建真条目** `裁-28`、`裁-29` | 顶点三条（稠密清单 / 稀疏与元数据清单 / JSON 覆盖）在本车道逐字复现，**信息量不减反增**（补上了顶点第三款「JSON 显式指定位深时以 JSON 为准」，原 `:50` 漏写）。顶点与 science 分册在第五项上的分歧不是信息而是**冲突**，已登记为 `裁-29` 待裁，未替它选边 |
| **R5** | **实现的全局精度位缺口**（未决项指针） | `UNIFIED_OBJECTS.md:52`（「登记在 `governance/UNRESOLVED.md`」）→ 该文件**零条目** | **补建 `裁-28`**（现指针为真） | `UNRESOLVED.md` §2 计数 22 → 24，与表内行数一致；条目把原「实现缺口」段的两块信息（全局位名 `g_aio_precision_mode_fp64`/`aio_set_precision_mode`/`aio_internal_is_fp64`、两个待裁选项）完整承接下来 |
| **R6** | **`K = num_threads` 的结论强度** | `architecture/DATA_FLOW.md:125`（「**唯一最小取值**」）<br>`resources/PERFORMANCE_MODEL.md:143`–`:168`（「充分条件（非定理陈述）」+ 四条来源链 + 适用域 + 算例） | **正本** = `PERFORMANCE_MODEL.md`；`DATA_FLOW.md:125` **改指** | `DATA_FLOW.md:125` 保留公式 `W_eff = in_flight × min(inner_omp, K)`、`K`/`num_threads`/`W_eff` 三个变量取义、以及「`K ≥ inner_omp`」条件；删掉的只有无来源的「唯一最小取值」与「总份数与轴形态无关」（后者在正本 `:152` 以「充分性来源」条目在位）。`DATA_FLOW.md:186` 的 `[7]` 原本文末列出、正文零引用，现被正文引用 |
| **R7** | **内存闸门安全系数数值** | `resources/PERFORMANCE_MODEL.md:47`–`:48`（声明「唯一数值源…**不声明第二套数值**」）<br>`architecture/DATA_FLOW.md:121`（硬编码 `0.75`） | `DATA_FLOW.md:121` **删副本**（`0.75` → 符号 + 键位语义） | 同句的 `B` 早已按符号写、只给「见 `PERFORMANCE_MODEL.md`」，本轮把 `0.75` 改成同法。系数名、键名、闸门公式、三个变量的取义全部保留，只去掉第二套数值 |
| **R8** | **参考文献表重编号漏两条** | `resources/PERFORMANCE_MODEL.md:275`–`:280`（原只有 `[1][4][5][6]`），正文 `:6`、`:7` 引用 `[2]`、`[3]` | **补 `[2]`、`[3]`** | 两条的原文由正文引用对象逐字复原（`[2]` = `../standards/NUMERIC.md` 数值标准；`[3]` = `../build/BUILD_GRAPH.md` 生产构建图），不新增任何未经核对的文献 |
| **R9** | **参考文献条目自引** | `governance/UNRESOLVED.md:100`（「…总章程与查证流程 `[3]`。」） | 删尾部自引，保留条目编号 | 该文件正文 `:91` 已合法引用 `[3]`；`:100` 的尾部 `[3]` 是第 1 轮批量补行内编号时误加在条目自身上的，与 `[1]`/`[2]` 写法不一致 |
| **R10** | **条款归属节名四写**（M1..M4 的「唯一正本」） | `contracts/HIPS_STORAGE_FORM.md:182`（大节名，正确）<br>`contracts/MANIFEST_VERIFY.md:75`（子节名，正确）<br>`contracts/ATOMIC_PUBLISH.md:198`（子节名，正确）<br>`contracts/CONFIG.md:167`（大节名，正确但粒度粗）<br>`contracts/CONFIG.md:103`（**假节名**「滤镜名匹配语义」，两处） | **正本** = `HIPS_STORAGE_FORM.md`；`CONFIG.md:103` **删假副本**改真实节名 + 论文格式引用；`CONFIG.md:167` **粒度对齐** | 用 `grep -nE '^#{1,3} '` 逐节核实：`HIPS_STORAGE_FORM.md` 无「滤镜名匹配语义」节（真实：大节 `:190` 形态的输入配置与输出清单字段，子节 `:238` 运行完成清单 `manifest.json#storage`（加性），`F0` 在 `:210`、`F1`–`F4` 在 `:231`–`:234`、`M1`–`M4` 在 `:255`–`:258`）；`docs/detail/PRODUCT_STORAGE_FORM.md` 亦无该节（真实 `:223` 10. 形态的输入配置与清单登记）。新写的节名逐个由该 grep 复核存在 |
| **R11** | **通用浮点容差的唯一正本** | `testing/TEST.md:42`（正本）<br>`contracts/SCHEDULER.md:33`（正确回引）<br>`contracts/CONFIG.md:106`（「…浮点容差内（`SCHEDULER.md` ）」→ 两跳链，中间那篇自述「本合同不复述」） | `CONFIG.md:106` **改指**（直达 `../testing/TEST.md`，删中间跳） | 「该面事前冻结的浮点容差内」这一判据语义逐字保留；新增 `[7]` 文献条目指向 `TEST.md`，本文件文末 7 条编号连续无重复 |
| **R12** | **UPM 控制点权重归一化公式** | `data/ARTIFACTS.md:110`（完整两阶段式，含 `control_reliability`）<br>`architecture/DATA_FLOW.md:142`（漏 `control_reliability`） | **正本** = `ARTIFACTS.md`；`DATA_FLOW.md:142` **改指**并补一步 | `DATA_FLOW.md` 的职责是冻结**归约顺序**，不是权重公式；新文保留 `raw_w = quality_factor × control_ivar`、按控制的 `sums[ck]` 归一、观测索引固定顺序三处要素，并点明「该步另乘几何可靠性因子」，公式主体交回 `ARTIFACTS.md` |
| **R13** | **排异自动路由档位表三写** | `contracts/CONFIG.md:305`–`:307`（3 档）<br>`contracts/CONFIG.md:355`（3 档，**权威误挂**最高设计）<br>`contracts/CONFIG.md:393`（「**7 档**自动选择 **SD-18**」，同行括号内只有 3 档） | **正本** = `CONFIG.md:355`；`:305`–`:307` 与 `:393` **改指**；`:393` 删「7 档」与流水编号 `SD-18` | 档位表内容一处不丢，全部移到 `:355`；`:355` 补上代码侧来源锚与「生产算法集 4 种 vs 自动路由 3 档是两个集合不是两套口径」的说明。`SD-18` 是任务流水编号，`AGENTS.md` §5 明禁。复现：`grep -rn "SD-18" docs/` → **0 命中**；`grep -rn "7 档" docs/engineering` → **0 命中** |
| **R14** | **`SECURE_LOADER.md` 的 `[n]` 记号二义** | `api/abi/SECURE_LOADER.md:30`–`:35`（六查**步骤编号** `[1]`–`[6]`）<br>同文件 `:116`–`:118`（文献 `[1]`–`[3]`） | 步骤编号改 `①`–`⑥`（消除与文献编号的同形冲突） | 六步的文本一字未改，只换记号；步骤序与文献序不再可混 |
| **R15** | **`os_abi` 值域的平台面挂错** | `contracts/CONFIG.md:229`（`platform: ../build/BUILD_NODES.md 10+ amd64 / Linux amd64）` —— 悬空、无开括号、指向 gitignore 目录）<br>同文件 `:155`（已正确指向权威顶点） | `CONFIG.md:229` **改指**权威顶点并补括号 | `BUILD_NODES.md` 内实测无「10+ amd64 / Linux amd64」字面；平台清单的原文在 `ACSD_DESIGN.md:466`–`:469`，新文按论文格式引 `[3]`（本文件已列该条） |
| **R16** | **`SNR-PREC-001` 精度锚的落点** | `contracts/HIPS_STORAGE_FORM.md:51`（「science 分册的**数据语义卷**」） | **删副本**（落点指错），改指 science 算法卷 | 全仓 `grep -rn "SNR-PREC"` → 正文与判据三条全在 `docs/science/algorithms/HIPS_WRITER.md:247/310/349`；`docs/science/unified/DATA_SEMANTICS.md` 中 `%.9g`/`%.17g` 计数 = **0** |
| **R17** | **「默认 FP64」的来源指针** | `contracts/CONFIG.md:65`、`:105`（指向 `docs/science/unified/SCIENCE_SCOPE.md`「参数与常数」一节） | 指针**悬空**，改如实标注「**需补充**」 | `grep -cn "精度\|precision" docs/science/unified/SCIENCE_SCOPE.md` → **0**；其 `## 4 参数与常数`（`:54`–`:63`）六行表无精度行。**没有替它编数值**，两处都写明「补源前该默认值不得当作已有权威背书」 |
| **R18** | **裁-27 引用的三个伪节名** | `governance/UNRESOLVED.md:45`（「`本节.4`」「`更新规则一节.3`」「`验证证据标准的纪律一节a/b`」） | 两个可定位的改真实节名；第三个真实落点未定位 → 如实写「**需补充**」 | 逐字核实真实落点：`ACSD_DESIGN.md:124` 是 `### 2.4 P4 重建稠密信噪比`、`:127` 是其「重建以噪声信号模型为物理前提：重建量随源亮度变化」；`ACSD_DESIGN.md:277` 是 `### 5.3 信噪比重建与逆方差叠加`、`:280` 是其「三者都产出同一物理量…」。「验证证据标准的纪律一节a/b」经 `grep -rn` 在 `ACSD_DESIGN.md` 与 `docs/engineering/testing/VALIDATION_EVIDENCE.md` 中**零命中**，且该格原有 A12 不可辨识占位符**原样保留**，未猜字 |

---

## 1. 同名不同义的保留项（逐条给依据，供复核）

> 这一节是本轮**最容易被下一轮误伤**的地方。每条都读过内容，不靠词面。

| 项 | 两处（或多处）在说什么 | 依据 | 处置 |
|---|---|---|---|
| **A · 「精度」的三个问句** | ① `UNIFIED_OBJECTS.md` 精度列 = **应归属档位**（该对象按顶点归属律取哪一档）<br>② `eng/contracts/schemas/unified/*.schema.json` 的 `precision` 属性 = **值域**（`enum ["float32","float64","integer"]`，是实例字段的合法取值集合，**不逐对象指派档位**）<br>③ `ARTIFACTS.md` 的 `scalar` 列 = **DataArtifact 承载面的标量形态**（线格式类型） | `python3 -c "json.load(...)"` 对 `provenance`/`frame_snr`/`signal` 三个 schema 逐一解析：`precision` 属性只有 `description`+`enum`，**无 `default`、无 `const`** ⇒ 它界的是值域不是取值。`ARTIFACTS.md:45` 自述「本表只登记它们在 DataArtifact 面的 scalar/… 列」 | **三处全保留**。已在 `UNIFIED_OBJECTS.md:50` 新增「精度列与另外两个同名项不是同一断言，不得互相代入」段，在 `ARTIFACTS.md:45` 后新增「本表 `scalar` 列的读法」段 |
| **B · `scalar` 与「精度」** | 见 A③ | 同 A | 保留；表头已加限定 |
| **C · `F0`–`F4` 与 `M1`–`M4`** | 两组都是「形态的输入配置与输出清单字段」这一**大节**下的条款，但落在**两个不同子节**：`F0` 在 `### normalize 输入配置键 storage_form`（`:196`/`:210`）、`F1`–`F4` 在输出清单段（`:231`–`:234`）；`M1`–`M4` 在 `### 运行完成清单 manifest.json#storage（加性）`（`:238`/`:255`–`:258`） | `grep -nE '^#{1,3} '` 给出真实节名与行号；`:182` 是该大节自述「不变式 F0–F4 / M1–M4」 | **保留区分，不合并**。`CONFIG.md:103` 现同时点名大节与 M 组所在子节，避免再被读成一节 |
| **D · 「平台」的五义** | ① 交付平台（Windows/Linux）`ARCHITECTURE.md:123`<br>② **平台角色**（谁开发/谁复验）`ARCHITECTURE.md:124`、`BUILD_NODES.md` §1<br>③ 构建工具链平台（编译器族）`CODE.md:7-8`、`DEPENDENCY.md:32`<br>④ CPU 指令集平台（ISA）`ISA_VARIANTS.md`、`CAPABILITY_PROBE.md`、`ABI.md:97`<br>⑤ OS 相关代码分层单元（`PLATFORM-CLI`/`PLATFORM-RUNTIME`/`PLATFORM-IO`）`MODULE_MAP.md:22-23` | 逐处读原文；`ISA_VARIANTS.md:134` 与 `:153` **同文件内**分别是工具链义与 OS 义（`/arch:` 旗标 vs 两族×两平台）；`MODULE_MAP.md:22` 的「平台单元」unit ID 字面为 `PLATFORM-*`，与 OS 无关 | **五义全保留**。本轮只收敛了②这一义（R1），其余四义一字未动 |
| **E · `SECURE_LOADER.md` 的 `[n]`** | `:30`–`:35` 是**六查加载序步骤号**，`:116`–`:118` 是**文献编号** | 两处都在同一文件、都用 `[n]`，读者与脚本都无法区分。脚本 `re.findall(r'\[(\d+)\]')` 确实把步骤号报成「悬空文献引用」 | **两种含义都保留**，只把步骤号改成 `①`–`⑥`（R14） |
| **F · 时间戳的「秒精度」** | `observability/STRUCTURED_LOGGING.md:64`（「`YYYY-MM-DDTHH:MM:SSZ`（秒精度，固定 Z）」）、`observability/RESOURCE_MONITORING.md:129`（「`t_iso_utc` 单调（秒精度允许相等…）」） | 两处「秒精度」指 **ISO8601 时间戳的分辨率**，与 FP32/FP64 位深、与 `UNIFIED_OBJECTS.md` 的精度列**完全无关** | **保留，不合并，不改**。词面撞车，语义零交集 |
| **G · `UNRESOLVED.md:24` 裁-5 的「精度」** | 「按 8 位格点量化的支撑面积」造成的误差 vs 32 位浮点累加噪声 | 是**量化格式/产品取舍**面，与归属律不同 | **保留，不合并** |
| **H · `SECURE_LOADER.md:104` / `RESOURCE_MONITORING.md:28` 的「发行验证面」** | 「Windows 实机验证属发行验证面」= **证据成熟度**分级 | 与「平台角色」无关 | **保留**（本轮只删了 `SECURE_LOADER.md:104` 的「技术预览」这一旧口径**措辞**，把「Linux 技术预览面」改为「Linux 实现面」；该词是 `git show 6934b1a1` 中被订正掉的旧句残留，与 R1 同源） |
| **I · `HIPS_STORAGE_FORM.md:165`「T1 的收益只在 Linux 面有可核验读数」** | **证据可得性**陈述 | 与 R1 的旧句方向看似相反、实为不同命题（前者说某机制的实测覆盖，后者说平台读数的效力） | **保留，严禁与 R1 合并** |
| **J · `CONFIG.md:65` 的 `precision` 与 `:258` 的 `precision(fp32)`** | 前者是**三命令 CLI 位深键**的默认档登记；后者是 **orchestrator Stage2 配置类**的解析缺省（原文自限作用域「仅用于 doc ↔ parser/struct 一致判据」） | 两处作用域不同且 `:258` 已自限 | **保留，不合并**（本轮只在 `:65` 标注来源需补充） |

---

## 2. 改动前后逐字（关键处）

### 2.1 平台角色（R1）

`build/BUILD_NODES.md:13-16` —— **改前**：

```
| Windows x64 正式工具链节点 | Windows 侧编译、用例执行、真实数据复验 | 开发迭代与静态分析 |
| Linux amd64 控制节点 | 常在线控制、静态分析、轻量编译、小合成实验 | Windows 发布性能结论 |

- 架构塑造方向是 Windows；Linux 侧的读数不作为 Windows 发布性能结论。
```

**改后**：

```
| Windows x64 正式工具链节点 | Windows 侧编译、用例执行、真实数据复验 | 开发迭代与静态分析、真实数据终验 |
| Linux amd64 控制节点 | 开发迭代、常在线控制、静态分析、轻量编译、小合成实验、合成、真实数据终验 | Windows 侧复验与发布候选终审 |

- 平台角色（哪个平台做开发、构建、合成与真实数据终验，谁复验、谁终审）的唯一正本在工程正本的
  架构面[5]；本表只登记两个具名构建节点各自执行的构建动作，属细目层，不复述也不改写平台角色。
```

文末新增：`[5] 内部文档 \`../architecture/ARCHITECTURE.md\`，平台与交付形态，平台角色（…）的唯一正本。`

### 2.2 `K = num_threads`（R6）

`architecture/DATA_FLOW.md:125` —— **改前**：

> …要让有效宽度达到帧内轴宽度必须 `K ≥ inner_omp`；而 `K = inner_omp = num_threads` 时同时在飞的暂存份数等于 `in_flight × inner_omp ≤ 租约`，因此 `K = num_threads` 是达成满宽的**唯一最小取值**，且总份数与轴形态无关。

**改后**：

> …由 `min(inner_omp, K)` 对 `K` 单调不降可知，`W_eff` 达到帧内轴宽度 `in_flight × inner_omp` 当且仅当 `K ≥ inner_omp`；`K < inner_omp` 时多余线程在暂存池上空等。因此 `K` **没有唯一最小取值**：可行区间内的任一取值给出同一满宽，总份数在该区间内不变。`K` 的实际生效值还被内核的 `min(num_threads, K)` 截断，故达成满宽的充要条件是 `min(num_threads, K) ≥ inner_omp`；把 `K` 取成 `num_threads` 只是其中一条充分条件，其来源链、适用域与算例由资源正本给出[7]，本节不复述。

`resources/PERFORMANCE_MODEL.md:143` —— **改前**：「…是达成满宽的一个**充分条件**，其成立依赖**三条**可核事实」＋首条标为「**必要条件**」。
**改后**：「…是达成满宽的一个**充分条件**，**且不是唯一取值**；其成立依赖**四条**可核事实」＋首条改为「**满宽的充要条件**」，并写入 `min(num_threads, K)` 截断式与「实现把 `kScratchPoolCap` 的缺省置为 `num_threads`，故不设环境变量本身就取到 `K = num_threads`，它是**缺省策略值而不是被论证出来的唯一最小值**」。

### 2.3 精度归属律越权改写（R4）

`UNIFIED_OBJECTS.md:50` —— **改前**末句：

> 归依据是最高设计的「精度归属」节与 science 分册数据语义卷的「精度」节——稠密大面（图像面、球面累加器、方差与覆盖面、HiPS tile）承载 FP32，稀疏与元数据（帧级信噪比、WCS 解、星表匹配、测光定标、**控制点**）承载 FP64；本列按该归属逐对象标注。

**改后**（节选）：

> …取自权威顶点的精度归属律——稠密大面（图像面、球面累加器、方差/覆盖面、HiPS tile）默认单精度；稀疏与元数据（帧级信噪比、WCS 解、星表匹配、测光定标、**manifest**）全程双精度；JSON 显式指定位深时以 JSON 为准。列中只给一个 token 表示该对象的应归属档；给出多个 token 表示该对象的归属档可由 JSON 显式指定覆盖（第三款）。

并新增一段：「稀疏与元数据清单第五项在权威顶点与其下级 science 正本之间不一致，登记为 `governance/UNRESOLVED.md` 的方向裁决条目，裁决前本列按权威顶点原文取 `manifest`。」

### 2.4 `provenance` 承载面（R3）

`data/ARTIFACTS.md:62` —— **改前**：`| DATA-OBJ-PROVENANCE-001 | provenance（…） | int | …`
**改后**：`| DATA-OBJ-PROVENANCE-001 | provenance（…） | 非数值（字符串 / 键值对象 / 字符串数组，无标量数值面） | …`

### 2.5 伪节名（R10）

`contracts/CONFIG.md:103` —— **改前**：`…合同与不变式 F0/F1..F4/M1..M4 = \`HIPS_STORAGE_FORM.md\` 「滤镜名匹配语义」一节，设计 = \`../../detail/PRODUCT_STORAGE_FORM.md\` 「滤镜名匹配语义」一节。`
**改后**：`…合同与不变式 F0–F4 与 M1–M4 的唯一正本 = \`HIPS_STORAGE_FORM.md\` 的「形态的输入配置与输出清单字段」一节（F0–F4 在该节的 normalize 输入配置键与输出清单两个子节，M1–M4 在其「运行完成清单 \`manifest.json#storage\`（加性）」子节）[6]；设计说明 = \`../../detail/PRODUCT_STORAGE_FORM.md\` 的「形态的输入配置与清单登记」一节。`

### 2.6 内存闸门系数副本（R7）

`architecture/DATA_FLOW.md:121` —— **改前**：`内存闸门上限 \`cap = floor(MemAvailable × 0.75 / (W×H×B/px))\`，…，\`B\` 是每像素字节数的标定值（见 \`../resources/PERFORMANCE_MODEL.md\`）。`
**改后**：`内存闸门上限 \`cap = floor(MemAvailable × kP1FrameMemSafetyFrac / (W×H×B/px))\`，…，\`B\` 是每像素字节数的标定值，\`kP1FrameMemSafetyFrac\` 是帧级内存闸门安全系数；两个系数的唯一数值源、落点与取值口径见 \`../resources/PERFORMANCE_MODEL.md\`，本式不复述其取值。`

---

## 3. 我自己重推过的公式/常数

> 不照抄前两轮任何一方结论。全部给出推导与可复跑依据。

### 3.1 `K` 的满宽充要条件（推翻「唯一最小取值」）

**抽象层推导。** `W_eff = in_flight × min(inner_omp, K)`。右侧对 `K` 单调不降，在 `K ≥ inner_omp` 后恒等于 `in_flight × inner_omp`。故

```
W_eff = in_flight × inner_omp   ⟺   K ≥ inner_omp
```

这是**充要条件**，不是必要条件 ⇒ **`K` 没有唯一最小取值**：区间 `[inner_omp, +∞)` 上任一取值给出同一 `W_eff`。

**实现层截断。** 读 `lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp`：

```
:1663   int kScratchPoolCap = num_threads;
:1664   if (const char* v = std::getenv("ACSD_P1_AXIS_SCRATCH_CAP")) { … kScratchPoolCap = min<long>(x, 4096); }
:1670   const int kScratchPool = std::min(num_threads, kScratchPoolCap);
```

实际分配的池是 `min(num_threads, K)`，故**生效的**充要条件是

```
min(num_threads, K) ≥ inner_omp   ⟺   (K ≥ inner_omp) ∧ (num_threads ≥ inner_omp)
```

代入正本已给的来源链 `num_threads = inner_omp`（帧级轴驱动路径），该条件退化为 `K ≥ inner_omp`；此时 `K = num_threads` 是可行区间**右端的一个取值**，而且因为 `kScratchPoolCap` 的缺省就是 `num_threads`，它其实是**缺省策略值**。

**反例（正本自己的算例即已给出）。** `PERFORMANCE_MODEL.md:166-168`：`I = 2` 形态下 `num_threads = 2`，`K = 2` 与 `K = 4` **等价**（`K = 4` 被 `min(num_threads, kScratchPoolCap)` 截回 2）。⇒ 「提高 `K`」在 `num_threads` 之上无效，`K = num_threads` 既不必要也不唯一。

**结论**：`K = num_threads` 是**充分条件之一**，不是唯一最小取值。原句的推理链是「三元合取 `K = inner_omp = num_threads` ⇒ 单条件 `K = num_threads`」，这一步丢了两个前提。

**代码侧同步登记**：`drizzle_engine.cpp:1654` 的注释仍逐字写「**K = num_threads 是达成满宽的唯一最小取值**」，与订正后的文档相反。该文件在 `docs/engineering/**` 之外（代码车道），本轮**未改**，登记为待代码侧同步（见 §6）。

### 3.2 三处「精度」问句的分离（推翻「以 schema 为准 ⇒ 全部取枚举三值」）

子代理建议把 `frame_snr` 一律改回 `float32|float64`，理由是两份文档都写「以 canonical schema 为准」。**我否决该建议。**

推导：`json.load(frame_snr.schema.json)` → `properties.precision` = `{description, enum}`，**无 `default`、无 `const`**。一个 `enum` 属性约束的是**该字段取值落在哪三个 token 之一**，它不回答「这个对象该取哪一个」——那是文档层的归属判定。把它当成逐对象赋值，等于把「值域」读成「取值」，**会抹掉顶点归属律本身**：`signal`/`variance`/`ivar` 若一律取三值，顶点第 3 款「JSON 显式指定位深时以 JSON 为准」和第 1/2 款的稠密/稀疏之分在工程层就再无落点。

同理，`ARTIFACTS.md` 的 `scalar` 列按其 `:45` 自述只登记 DataArtifact 面的承载形态，把它与应归属精度合并同样是误判。⇒ 走**同名不同义保留 + 显式区分**，而非二选一。

### 3.3 `provenance` 无标量数值面的核验

`json.load(provenance.schema.json)` → `properties` 逐键类型：`units`=object、`missing_value`=object、`precision`=enum、`provenance`=object、`input_hashes`=array(string)、`run_id`=string、`product_type_id`=string，其余为 `const`/枚举。**无任何 `type: number/integer` 属性** ⇒ 承载面没有标量数值面，`int` 无据。`ARTIFACTS.md:62` 的真错不是「取了哪个档」，而是「这张面根本不存在」。

---

## 4. 索引真解析器输出

`docs/DOCUMENT_INDEX.yaml` 的改动：仅 1 处 —— `docs/engineering/build/BUILD_NODES.md` 的 `duty` 字段。本轮把平台角色从该篇的承载面移出（改指 `ARCHITECTURE.md`），原 `duty`「构建节点职责…」会让索引继续暗示该篇承载平台角色，属**索引侧残留副本**，必须同步。其余 7 份改动文档的 `duty`/`upstream`/`downstream` 描述的是主题而非条目数，本轮未增删/改名任何文档路径，故无需改动。

```text
=== 真解析器 yaml.safe_load（改后复验） ===
  yaml.safe_load 解析: OK | schema_rev = 3 | active = 148
  重复登记: 0
  悬空登记: 0
  BUILD_NODES.duty = 两个具名构建节点各自执行的构建动作（细目层）、冻结工具链取值与处置纪律、构建档位与可复现判据；平台角色不在本篇承载
  树有索引无（排除 README）: 0
```

复现命令：

```bash
cd "/workspace/Astro CS Database"
python3 - <<'PY'
import yaml,os,collections
d=yaml.safe_load(open('docs/DOCUMENT_INDEX.yaml',encoding='utf-8'))
act=d['doc_index']['active']
paths=[e['path'] for e in act]
print("active =",len(act))
print("重复登记:",[p for p,c in collections.Counter(paths).items() if c>1] or 0)
print("悬空登记:",[p for p in paths if not os.path.exists(p)] or 0)
disk={os.path.relpath(os.path.join(dp,f),'.') for dp,dn,fn in os.walk('docs')
      for f in fn if f.endswith(('.md','.yaml'))}
print("树有索引无(排除README):",[p for p in sorted(disk-set(paths)) if not p.endswith('README.md')] or 0)
PY
```

**零悬空、零重复、解析通过。** 树侧反查的 27 条差集全部是每目录 README 招牌件，索引自身 `coverage` 字段已声明其不进规范索引面。

---

## 5. 我推翻的既有判定

| # | 被推翻的判定 | 出处 | 我的复核与理由 |
|---|---|---|---|
| **1** | 「`SECURE_LOADER.md` 的 `[4][5][6]` 是悬空文献引用，文献表漏了三条」 | `T06-r2-审稿-DOC-ENG.md:272`（§3.5） | **推翻 —— 词面误判。** 打开 `SECURE_LOADER.md:28`–`:36` 读内容：那是 ```text 代码块里的**六查加载序步骤编号**（`→ [1] 路径: 绝对 + realpath canonical…`），与文末文献表毫无关系。脚本把同形记号当成引用。**但真缺陷另有其事**：同一文件用 `[n]` 同时表示步骤序与文献序，读者与脚本都无法区分 —— 已按 R14 消除记号冲突 |
| **2** | 「`COMPATIBILITY.md` 是名为『兼容性策略』的正本却不含任何平台内容，属名不符实 / 平台条款无处安放」 | 子代理 `70db7ebe` §6 | **推翻 —— 越权判定。** 打开 `COMPATIBILITY.md` 全文 29 行：`:5` 自述范围是「**持久化模型、配置与接口**的兼容性边界，以及不可透明兼容时的处置」，平台兼容**从未被声明为该篇范围**。标题里的「兼容性」在该仓一贯指持久化/配置/接口兼容（该篇 `:11` 只讲 UPM 持久化）。这不是缺口，是另一个已被上游独占的命题。一级正本无权单方改另一篇的职责范围 |
| **3** | 「`Fatduck` 主机在仓内无定义、需补充」 | 子代理 `b4daa3f0` §5-2、`77d81fff` | **推翻。** `eng/tools/HANDOVER.md:111` 有定义：「访问路径：`agent`（本机）→ `vm-bj`（`100.73.70.16`，root）→ `Fatduck`（`100.104.10.71`，用户 `fujia`，pwsh 7.6.3）」。它有定义，只是定义在 `eng/tools` 而非文档正本，文档侧零转述 —— 记为**可追溯性缺口**，不是「无定义」 |
| **4** | 「两份都写『以 canonical schema 为准』，故 `UNIFIED_OBJECTS.md:41` 的单值 `float64` 是不合规的那一份，应统一取枚举三值」 | 子代理 `e276a36a` §4-1、`37c973da` §4-1 | **推翻 —— 推导错。** 见 §3.2：schema 的 `precision` 是无 `default`/`const` 的 `enum`，界的是值域；把它当逐对象赋值会抹掉顶点归属律。正确处置是同名不同义分离，不是二选一 |
| **5** | 「我自己在 `PERFORMANCE_MODEL.md` 新写的『**充要条件（不是必要条件）**』自相矛盾」 | 子代理 `75f73e5e` §10-11 | **接受并已修正。** 充要条件必是必要条件，该括注错。现文为「**满宽的充要条件**」，推导见 §3.1 |

---

## 6. 子代理派发与逐条复核

派出 **6 个**（其中 2 组各被我误发了重复派单，构成独立盲复核）：

| 子代理 | 车道 | 状态 | 我的处置 |
|---|---|---|---|
| `37c973da` / `e276a36a` | 精度归属 | 均交付 | **采信其 3 处悬空指针定位**（`UNRESOLVED.md` 无条目、`SCIENCE_SCOPE.md` 无精度内容、`NUMERIC.md` 无归属条款）—— 我逐条 `grep -c` 复现后全部采信；**否决其「统一取枚举三值」的建议**（§5-4） |
| `70db7ebe` / `b4daa3f0` | 平台角色 | 均交付 | **采信**：`build/` 被 `.gitignore:20` 完全排除（我 `check-ignore` + `ls-files` 独立复现）、`RELEASE.md:126` 与旧句矛盾、`CODE.md:15`/`DEPENDENCY.md:33` 的 MinGW64「本地开发」定位、`CONFIG.md:229` 损坏引用、`SECURE_LOADER.md:104`「技术预览」旧词残留。**否决其 `COMPATIBILITY.md`「名不符实」判定**（§5-2） |
| `77d81fff` / `75f73e5e` | 参数口径 + 参考文献编号 | 均交付 | **采信**：文献编号全域复检（悬空 0 / 断号 0）、`DATA_FLOW.md:121` 硬编码 `0.75` 副本、`UNIFIED_OBJECTS.md` 的 16 处 `§` 锚指空、`parallel_min_work` 无来源。**否决其「参考文献表补漏项」已完成故无需我再查**的说法 —— 两人都是在我的并发编辑中途观测到的，我独立复跑后确认到位 |

**我自己的独立否决/改写：**

| 我的初判 | 复核后 | 结论 |
|---|---|---|
| `SECURE_LOADER.md` 的 `[4][5][6]` 是漏掉的文献 | 打开 `:28`–`:36` 读内容，是六查步骤编号 | **推翻我的初判** → 转为 R14 的记号冲突缺陷 |
| `frame_snr` 应统一取 `float32\|float64` | schema `precision` 无 `default`/`const` | **推翻我的初判** → 走同名不同义分离（§3.2） |
| `provenance` 应统一取「非数值」 | `ARTIFACTS.md` 的列问的是承载面形态 | **部分推翻**：列的语义保留，`int` 这一取值仍必须改（R3） |
| `ARTIFACTS.md:45` 缺本节小标题（非元信息块） | 该行是表格引导句，不是元信息块 | **不编造问题**，未改 |

**诚实边界**：我逐行读完 `UNIFIED_OBJECTS.md`(146)、`ARTIFACTS.md`(135)、`BUILD_NODES.md`(101)、`COMPATIBILITY.md`(29)、`HIPS_STORAGE_FORM.md` 节标题全表、`MANIFEST_VERIFY.md:70-80`、`ATOMIC_PUBLISH.md:194-202`、`DATA_FLOW.md:100-186`、`PERFORMANCE_MODEL.md:1-175`、`UNRESOLVED.md` 全文、`CONFIG.md` 六个改动行及上下文。其余文档**未逐行读完**的结论一律标注来源命令可复跑。我**没有编译、没有运行任何二进制**；「代码为准」的判定来自源码阅读 + `grep`。我**没有取任何外部文献原文**。

---

## 7. 自证段

### 7.1 关键复跑命令

```bash
cd "/workspace/Astro CS Database"

# 本车道改动面（注意 build/ 不在 git 中，diff 看不到）
git -c core.quotepath=false diff --stat -- docs/engineering docs/DOCUMENT_INDEX.yaml
git -c core.quotepath=false ls-files docs/engineering/build | wc -l        # 0
git -c core.quotepath=false check-ignore -v docs/engineering/build/BUILD_NODES.md

# R1 平台角色：旧断言已归零
grep -rn "架构塑造方向是 Windows\|不作为 Windows 发布性能结论\|常在线控制、静态分析、轻量编译、小合成实验 | Windows" docs/   # 0

# R6 K 的口径
grep -rn "唯一最小取值" docs/            # 2 处，均为新写的「没有唯一最小取值」
sed -n '1605p;1663p;1670p' lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp

# R7 闸门系数副本已删
grep -n "MemAvailable" docs/engineering/architecture/DATA_FLOW.md            # 无 0.75

# R3 provenance 承载面
python3 -c "import json;d=json.load(open('eng/contracts/schemas/unified/provenance.schema.json'));print([k for k,v in d['properties'].items() if v.get('type') in ('number','integer')])"   # []

# R4/R17 悬空指针已订正
grep -cn "精度\|precision" docs/science/unified/SCIENCE_SCOPE.md            # 0
grep -rn "验证证据标准的纪律\|更新规则一节" docs/ACSD_DESIGN.md docs/engineering/    # 0
grep -nE '^#{1,3} ' docs/engineering/contracts/HIPS_STORAGE_FORM.md | grep -c "滤镜名匹配语义"   # 0

# R8/R9 参考文献编号
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
        n=[int(x) for x in re.findall(r'^\[(\d+)\]',t[i:],re.M)]
        if n and n!=list(range(1,len(n)+1)): print("断号",p,n)
PY
# → 仅 PUBLIC_API.md 的 [6][16][32][36][64][256][512][1024]，逐处读原文为 C 数组下标/尺寸
#    （error_msg[256]、tile_width[64] 等），非文献引用；断号 0

# R13 排异
grep -rn "SD-18" docs/          # 0
grep -rn "7 档" docs/engineering # 0

# 括号配平（本轮改动的 9 份）
python3 -c "
for p in ['docs/engineering/build/BUILD_NODES.md','docs/engineering/architecture/DATA_FLOW.md','docs/engineering/resources/PERFORMANCE_MODEL.md','docs/engineering/governance/UNRESOLVED.md','docs/engineering/UNIFIED_OBJECTS.md','docs/engineering/data/ARTIFACTS.md','docs/engineering/contracts/CONFIG.md','docs/engineering/contracts/HIPS_STORAGE_FORM.md','docs/engineering/api/abi/SECURE_LOADER.md']:
    t=open(p,encoding='utf-8').read(); print(p, t.count('（'), t.count('）'))"
# 8 份平衡；CONFIG.md 差 2 个「）」，全部来自 :107 与 :125 两处**既有**失衡（第 2 轮 L-11 已列），本轮未新增
```

### 7.2 本轮改动清单

| 文件 | 行数变化 |
|---|---|
| `docs/engineering/build/BUILD_NODES.md` | **不在 git 中**，改动无法用 `git diff` 复核（`.gitignore:20`），已在 §2.1 留逐字前后 |
| `docs/engineering/architecture/DATA_FLOW.md` | +3 −3 |
| `docs/engineering/resources/PERFORMANCE_MODEL.md` | +18 −4 |
| `docs/engineering/governance/UNRESOLVED.md` | +6 −2 |
| `docs/engineering/UNIFIED_OBJECTS.md` | +4 −2 |
| `docs/engineering/data/ARTIFACTS.md` | +3 −1 |
| `docs/engineering/contracts/CONFIG.md` | +14 −10 |
| `docs/engineering/contracts/HIPS_STORAGE_FORM.md` | +1 −1 |
| `docs/engineering/api/abi/SECURE_LOADER.md` | +7 −7 |
| `docs/DOCUMENT_INDEX.yaml` | +1 −1 |

`git status` 中另有 `docs/detail/**`、`docs/science/**` 共 47 份显示为已修改，**均非本轮所为**（其它车道并发工作树），本轮未触碰。

### 7.3 未收敛项（本轮明确不做，交前台派单）

| # | 未收敛项 | 已取证 | 为什么本轮不做 |
|---|---|---|---|
| **U1** | `.gitignore:20` 的 `build/` 无条件排除，使 `docs/engineering/build/` 四份一级正本**整层不入版本控制** | `check-ignore -v` → `.gitignore:20:build/`；`ls-files docs/engineering/build \| wc -l` → **0** | `.gitignore` 与 `git add` 都超出「改文档 + 禁止 git 写操作」的授权。**这是本车道一切「只改一侧」的物理成因**：`BUILD_NODES.md` 的订正进不了 diff，索引门双向比对恒绿（索引侧有、干净克隆上树侧也没有）。**优先级最高的前置项** |
| **U2** | `UNIFIED_OBJECTS.md` 13 行 / 16 处 `§` 机械锚**全部指空**（`DATA_SEMANTICS` 只有 `## 1`–`## 7`，`NOISE_SNR.md` 同）；另有 8 个 `docs/detail/**` 目录在仓内不存在 | `grep -nE '^#{1,3} ' docs/science/unified/DATA_SEMANTICS.md` 逐节复核；`os.path.exists` 逐条校验 | 这是**整张「合同面（条款位置）」列**的重写（约 13 行密集改动），与本轮的重复断言收敛不同族，一次做完风险高于收益。**且它是 R2 前车用批量替换制造出来的** —— 同一批替换还造出了本轮 R10/R18 的伪节名。建议与「§ 锚禁令」同批处理 |
| **U3** | 16–18 份文档**文末有编号文献表、正文零 `[N]` 引用**（`ARCHITECTURE.md`、`MODULE_MAP.md`、`CONFIG.md`、`SCHEDULER.md`、`LOG_AND_ERROR.md`、`PIPELINE_BLOCK.md`、`RUNTIME.md`、`ABI.md`、`ASYNC_IO.md`、`BENCHMARK.md`、`cpu/**` 四份、`RUN_GRAPH.md`、`STRUCTURED_LOGGING.md`、`PUBLIC_API.md` 的 `[3]`） | 本车道脚本全域复跑，逐份列出 | 违反 `AGENTS.md` §5 的论文格式，但属**格式债**不是重复断言；且需先裁决「补正文编号」还是「删无用条目」两种方向，一次裁决后批量做 |
| **U4** | `docs/detail/registry/acsd.phase1.noise-snr.md:508`–`:514` 仍写「`UNIFIED_OBJECTS.md` 的对象登记对全部对象只给『float32 或 float64』**一个全局精度位**」，与 `UNIFIED_OBJECTS.md:50`–`:52` **同一提交内互斥** | 逐处读原文 | **车道外**（`docs/detail/**` 属 detail 车道）。已在本交付件登记，供 detail 车道派单 |
| **U5** | `docs/detail/common.md:34` 把发布面归属挂到 `NUMERIC.md`，而该文件**全篇无任何稠密/稀疏归属赋值** | `grep -n "稠密\|稀疏" docs/engineering/standards/NUMERIC.md` 仅 `:178` 一处（讲 HiPS 存什么对象，与精度无关） | 同 U4，车道外 |
| **U6** | `docs/science/unified/DATA_SEMANTICS.md:156`–`:165` 已把顶点 FP64 清单第五项 `manifest` 改成 `控制点` | 与顶点 `:163` 逐字对读 | **science 车道**，本轮只在自己车道内按顶点原文取 `manifest` 并登记 `裁-29`，未改 science |
| **U7** | `PIPELINE_BLOCK.md:78/:97/:116`（PC-C6/IR-C7 下界）、`ARCHITECTURE.md:118`（`parallel_min_work`）、`TEST.md:162`（`max(60 s, 3×)`）—— **声称有数值/下界，仓内查无来源** | `grep -rn` 全域零命中 | 属「无来源需补充」而非重复断言。本轮**没有替它们编数值**；建议按 `UNRESOLVED.md` §2 补登记 |
| **U8** | `drizzle_engine.cpp:1654` 的代码注释仍逐字写「`K = num_threads` 是达成满宽的**唯一最小取值**」，与本轮订正后的两份文档相反 | 已读原文 | 代码车道。按 `AGENTS.md` §4「代码与文档冲突时以文档为准订正代码」 |
| **U9** | `CODE.md:15` / `DEPENDENCY.md:33`/`:37` 把 Windows 原生的 MSYS2/MinGW64 定为「**本地开发**与兼容性验证工具链」，与「开发在 Linux amd64 节点完成」不相容 | 逐处读原文；`ARCHITECTURE.md:124` 为准 | 属**需负责人裁决**的方向问题（AGENTS §9），不归执行车道 |
| **U10** | `.github/workflows/` 目录不存在，但 `CODE.md:73`–`:74` 声明三个 CI workflow 名与 artifact 前缀 | `ls .github` → 「没有那个文件或目录」 | 机器源缺失，须登记而非编造 |

### 7.4 收敛声明

本车道**未达成收敛**，但本轮的目标是「先止血」：**任务点名的四类已实测重复（精度归属、平台角色、唯一最小取值、参考文献漏两条）在本车道内已各自归一为唯一正本**，并对 10 组同名不同义给出了可复核的保留依据。新引入的零矛盾：每处改动都只把副本改为引用或订正，未新增任何口径；索引经真解析器复验零悬空零重复。

---

## 8. 交付件

唯一交付件：`/workspace/Astro CS Database/run/GOVERN-08/审核包-R2/T07-重复断言收敛-engineering.md`（本文件）。

**未执行任何 git 写操作**；未 amend、未 force-push、未 add。