# T06 第1轮订正 · subA（docs/engineering/contracts 白名单 + data 白名单）

本件是**订正记录**，不是复审结论。范围限定为派单白名单的 10 个文件；白名单外文件一律未改。

- 白名单：`contracts/{CLI_PROTOCOL,MANIFEST_VERIFY,HIPS_STORAGE_FORM,ATOMIC_PUBLISH,OWNERSHIP_LIFETIME}.md`、
  `data/{ARTIFACTS,ARTIFACT_STORE,PHASE_PRODUCT_EXCHANGE,PROVENANCE,README}.md`
- git 全程只读，所有 git 调用带 `-c core.quotepath=false`；无 add/commit/checkout/reset/stash/tag/push。
- **并行冲突登记**：`HIPS_STORAGE_FORM.md` 与 `OWNERSHIP_LIFETIME.md` 在本代理工作期间被另一代理并发改写
  （1-10「四件事→六件事」、头部元信息块、P-180 删除、参考文献 [1][2][3]）。本代理只做定点 `edit`/带断言的
  定点替换，未整文件覆写，两方改动在当前工作树中共存且互不覆盖（见 §6 自证段 A-3）。

---

## 1. 逐条处置表

处置取值：**已改** / **降级** / **撤回** / **待裁决**。

### A 类 · 引用失真（机械）

| 编号 | 处置 | 改前逐字 | 改后逐字 | 复核命令与输出 |
|---|---|---|---|---|
| 5-1 | 已改（7 处） | `> 上游：ACSD_DESIGN.md 「配置与 output_dir」一节.1（命令树）、「配置与 output_dir」一节.2（配置、事件与退出码）`<br>`命令树 = …「配置与 output_dir」一节.1 的**唯一命令树**`<br>`命令语义（「配置与 output_dir」一节.1 薄入口）`<br>`串接一律显式**（`../../ACSD_DESIGN.md` 「命令树」一节.2）`<br>`**事件流 = 默认输出**（`../../ACSD_DESIGN.md` 「配置与 output_dir」一节.2）`<br>`运行前预检由 ../../ACSD_DESIGN.md 「JSONL 运行事件流」一节.5 三档页面` | 全部改指真实节名并去掉 `.1/.2/.5`：命令行合同一章[1]；「命令树」→ 最高设计[1] 命令树；「事件流=默认输出」→ 机器输出与退出码[1]；预检三档页面 → 最高设计的运行前预检一章[1] | `grep -n "^#" docs/ACSD_DESIGN.md` → `324:## 7. 命令行合同 / 326:### 7.1 命令树 / 346:### 7.2 机器输出与退出码 / 352:### 7.3 错误传播与日志 / 232:### 4.5 运行前预检（三个命令通用）`；`grep -n "配置与 output_dir" docs/ACSD_DESIGN.md` → 0 命中。**注**：JSONL 事件流不是独立节，只是 7.2 内一句正文（「运行事件默认走 JSONL 事件流」），故指向 7.2 而非造节名。 |
| 5-2 | 已改（3 处，全部在我白名单内） | `> ID: API-CLI-001 状态: FROZEN 上游: API-001/ARCH-002 …`<br>`…incomplete 形态**(与 ARCH-002 「取消与崩溃」一节/ARCH-005 「标准输出与标准错误纪律」一节 原子单元一致)`<br>`## run_manifest.json v1(run 结束原子写, ARCH-002 「落点映射与测试」一节)` | 元信息块整块删除（同时消解 9-7）；取消语义 → 「与本文件「取消与崩溃」一节的原子单元一致；上位取消语义见最高设计的机器输出与退出码[1]与错误传播与日志[1]」；标题 → `## run_manifest.json v1(run 结束原子写，落点映射见「落点映射与测试」一节)` | `grep -rn "ARCH-00[0-9]" docs/engineering/` → 命中全为引用本身与 `governance/TRACEABILITY.md` 的 `ARCH-001` ID 列（该列是台账数据，非文档 ID 体系）；`ls docs/engineering/architecture/ARCHITECTURE.md` → 无 ID 体系 |
| 5-3 | 登记（不在我白名单） | `LOG_AND_ERROR.md:257` 引「机器判据」 | 未改 | `grep -rn "机器判据" docs/ACSD_DESIGN.md docs/science/` → 0 命中，审稿员成立；该行位于 LOG_AND_ERROR 后半拼接段，由拆分该文件的人处理。**交接给 LOG_AND_ERROR 处理者**：改指最高设计的「逐像素排异」或 `testing/TEST.md` 的判据面 |
| 5-4 | 已改（12 处，逐处列出） | `「」` ×12 | 见 §1.1 逐处对照表 | `grep -c "「」" docs/engineering/contracts/HIPS_STORAGE_FORM.md` → 12 → 改后 **0** |
| 5-5 | 已改（10 处 + 附带 12 处同类误挂） | 见 §1.2 逐处对照表 | 见 §1.2 | 见 §1.2 末命令输出 |
| 5-7 | 部分已改 | — | 参考文献节全部改为论文格式编号引用并与正文逐条对应（孤儿 0）。**未做全车道 521 处「X」一节 的批量降级**（审稿员自己定为「分批」，我只在改动所及处改） | 见 §5 自证段命令 4 |
| 5-8 | 已改（9 个有文献表的白名单文件全部） | 全部正文零 `[n]`、文献表全孤儿 | 逐文件在正文真实断言处补 `[n]`，参考文献条目同时改为可解析路径 | 见 §5 命令 4 → 我白名单 9 份全部 `孤儿条目 []` |
| 5-10 | **推翻审稿前提（部分）** | 审稿称 `ARTIFACTS.md:41-60`、`:30` 与 `PHASE_PRODUCT_EXCHANGE.md` 含 `docs/detail/common/UNIFIED_MODEL` | 我白名单内**零处**该串；只把裸写的 `UNIFIED_MODEL` 补成真实路径 `docs/detail/UNIFIED_MODEL.md` | `grep -rn "docs/detail/common" docs/engineering/` → **仅** `UNIFIED_OBJECTS.md:4` 与 `:46` 命中，`ARTIFACTS.md`/`PHASE_PRODUCT_EXCHANGE.md` 均无 |
| 5-12 | 已改（统一到真实节名） | 两篇各用一个**都不存在**的节名：`weight/value/scale/sigma/snr 歧义映射` 与 `13 个对象 → canonical schema → schema ID` | 统一到 `docs/detail/UNIFIED_MODEL.md`「数据对象（各自具名）」一节（15 处） | `grep -n "^#" docs/detail/UNIFIED_MODEL.md` → `15:## 2. 数据对象（各自具名）`；两个争议名在该文件均 0 命中 |
| 5-13 | 已改（2 处） | `登记面 = \`governance/TRACEABILITY.md 「合同 ID 登记面」一节\``（ATOMIC_PUBLISH:63、OWNERSHIP_LIFETIME:26） | `登记面 = \`../governance/TRACEABILITY.md\`[4]/[3] 的合同 ID 登记面` | `ls docs/engineering/contracts/governance` → 无；`ls docs/engineering/governance/TRACEABILITY.md` → EXISTS |

#### 1.1 5-4 逐处补回（HIPS_STORAGE_FORM.md，12 处）

| 行 | 补回为 | 依据 |
|---|---|---|
| 27 | `压缩档位（默认值见「压缩档位」一节…）` | `### 压缩档位`（:64）定义默认 zstd level 3 |
| 27 | `索引载体的未来演进（见「载体演进」一节）` | `### 载体演进`（:125） |
| 138 | `字段与不变式见「运行完成清单 \`manifest.json#storage\`」一节` | `### 运行完成清单 \`manifest.json#storage\`（加性）`（:236），M1–M4 定义处 |
| 203 | `（见「运行完成清单 \`manifest.json#storage\`」一节的 M2）` | M2 定义在 :256 |
| 204 | `出现即 REJECT（见「mosaic / export：形态键必须 REJECT」一节）` | `### mosaic / export：形态键必须 REJECT`（:268） |
| 232 | `索引必须可由产品内容重算（见「索引 schema」一节的 I4）` | `## 索引 schema`（:69），I4 在 :92 |
| 252 | `字段名与「形态的输入配置与输出清单字段」一节 **同词表**` | `## 形态的输入配置与输出清单字段`（:188） |
| 257 | `形态事实的落点 = …「形态的输入配置与输出清单字段」一节` | 同上 |
| 263 | `缺失 ⇒ 规定回退 = …（「数据集级覆盖索引 \`coverage.index.json\`」一节）` | `### 数据集级覆盖索引 \`coverage.index.json\``（:94），回退规则在 :106 |
| 264 | `mosaic 产物固定裸形态（「mosaic / export：形态键必须 REJECT」一节）` | :270 表首行 |
| 266 | `…（见「运行完成清单 \`manifest.json#storage\`」一节）` | `$defs.manifest_storage` 在 :258 |
| 280 | `合并后按「数据集级覆盖索引 \`coverage.index.json\`」一节 由各产品级索引重算` | :106 派生产物条款 |

#### 1.2 5-5 逐处补回（PHASE_PRODUCT_EXCHANGE.md）

| 行 | 改前 | 改后 | 依据 |
|---|---|---|---|
| 9 | `最高设计 ：一次 CLI 调用只驱动一个阶段、无 \`--phases 1,2,3\`；：跨阶段只交换磁盘产品` | `最高设计「总原则：唯一入口、阶段独立调度器」一节：一次调用只驱动一个阶段、跨阶段只交换磁盘 HiPS 与 manifest` | `ACSD_DESIGN.md:362-367` 逐字含「一次调用只驱动一个阶段」「阶段间唯一交换媒介是磁盘 HiPS 与 manifest」。**`--phases` 全仓 0 命中 → 该 token 无来源，删除而非保留** |
| 101 | `层语义（最高设计 ，强制）` | `层语义（最高设计「P2 跨帧绝对信噪比」一节，强制）` | `ACSD_DESIGN.md:104 ### 2.2 P2 跨帧绝对信噪比` |
| 104 | `（最高设计 ：HiPS 里只**存** …）` | `（最高设计「P2 跨帧绝对信噪比」一节：HiPS 里只**存** …）` | `:111`「normalize 与 export 不产生、不存储权重」 |
| 111 | `规则依据 \`../../ACSD_DESIGN.md\` ，` | `规则依据最高设计的「逐像素排异」一节，` | `ACSD_DESIGN.md:291 ### 5.5 逐像素排异` |
| 130 | `「NaN 采用样本级掩膜」…「每一层只由它的上一层推出」` | `最高设计的「逐像素排异」一节「NaN 采用样本级掩膜，覆盖级缺数置 NaN 并计数」条款…；逐层推出关系见最高设计的「m osaic：相对定标 · 排异 · 集成」一章固定科学流程` | `:295` 逐字；「每一层只由它的上一层推出」在最高设计无对应文本 → 不造节名，改指真实章 |
| 167 | `—— NaN 保留给「无覆盖」（/ ）。` | `—— NaN 保留给「无覆盖」；该三态编码的正本见 \`docs/science/unified/DATA_SEMANTICS.md\`「方差与逆方差的三态编码」一节。` | `DATA_SEMANTICS.md:59 ### 3.3` 逐字含「NaN 只留给"无覆盖"」 |
| 208 | `并与该文件 （\`ivar==0\` = 合法零权重…）一致` | `并与 \`docs/science/unified/DATA_SEMANTICS.md\`「方差与逆方差的三态编码」一节（…）一致` | 同上，`:78` 逐字「读侧把 `ivar = 0` 视为合法零权重」 |
| 217 | `（\`../../ACSD_DESIGN.md\` ）` | `（最高设计的「逐像素排异」一节）` | 同上 |
| 243 | `（最高设计 ：一次 CLI 调用只驱动一个阶段、无 \`--phases 1,2,3\`；` | `（最高设计「总原则：唯一入口、阶段独立调度器」一节：一次调用只驱动一个阶段；` | 同 9 |
| 294 | `## 验收映射（依据：「兼容矩阵」一节 + \`../../ACSD_DESIGN.md\` ）` | `## 验收映射（依据：本文「兼容矩阵」一节 + 最高设计的「I/O 与原子产品」一章）` | `ACSD_DESIGN.md:453 ## 10. I/O 与原子产品` |
| 319 | `**阶段三接受域（最高设计 ）**` | `**阶段三接受域（最高设计的「投影算法」一节）**` | `ACSD_DESIGN.md:316 ### 6.3 投影算法`，`:320` 逐字「导出只接受面亮度语义输入，方差/逆方差显式消费并传播」 |

附带（同一失真类，审稿员未逐条列）：`DATA_SEMANTICS 「兼容矩阵」一节[a]` 6 处、`DATA_SEMANTICS 「交换对象」一节/「跨 Phase 仅磁盘交换」一节`、`DATA_SEMANTICS 「阶段产品角色与 type 绑定」一节`、`SCI-P3 「跨 Phase 仅磁盘交换」一节[a]` 3 处、`「兼容矩阵」一节a` 3 处 —— 全部是把**本文自己的节名挂到别人名下**。逐条对内容复核后重指：
`D_p`/`W_p` 符号唯一性、三态表、`ivar==0` → `DATA_SEMANTICS.md`「方差与逆方差的三态编码」一节（`:59`）；
ICRS/NESTED/leaf 索引 → 同文件「坐标语义」一节（`:28`）；
TAN/CUNIT/CRPIX → `docs/science/PHASE3_HIPS_TO_FITS.md`「坐标 frame」一节（`:32`）。

### B 类 · 口径与代码一致性

| 编号 | 处置 | 改前逐字 | 改后逐字 | 复核命令与输出 |
|---|---|---|---|---|
| 2-11 | 已改（`ARTIFACTS.md`） | `\| UPM 控制点权重 = quality×geom×control_ivar \| DATA-UPM-CONTROL-UNC-001 \| 已消除(与 integration weight 各自具名) \|` | `UPM 控制点权重**分两个阶段**…：**① 分子** raw_w = quality_factor × control_ivar（production；**几何可靠性不在分子**；control_ivar ≤ 0/非有限 ⇒ rc=2）…**② per-control 归一化** w_cell = raw_w / Σ_cell(raw_w) × control_reliability（**几何可靠性在这一步施加**）。cfg.use_ivar_weight == 0 的 quality × support^p × snr²/(1+snr²)/max(unc², sigma_floor²) 只作 ablation/诊断` | `sed -n '173,190p' lib/algorithms/coverage/include/astro/phase2/upm.h` → `production（cfg.use_ivar_weight != 0）：raw_w = quality_factor × control_ivar（几何可靠性在 per-control 归一化中施加）`；进一步读**实现** `sed -n '2024,2058p' lib/algorithms/coverage/src/upm.cpp` → `*out_raw = qf * civ;`，注释 `SCI-UPM-WEIGHT-001：science 权重只含 quality × control_ivar。无 star-SNR / support^p / 单像素 ivar 因子。`；`sed -n '691,707p' upm.cpp` → `raw_w[i] = raw_w[i] / s * m->controls[ck].reliability;` 与 `w_cell = w_UPM / (Σ_cell w_UPM) × control_reliability`。**审稿员成立**（`geom` 在分子与代码相反）。 |
| 2-11 后续 | 登记（不在我白名单） | `architecture/DATA_FLOW.md:142` 写 `raw_w = quality_factor × control_ivar` | 未改 | 该处**与代码一致，无需改**；但应补「per-control 归一化再乘 control_reliability」这第二阶段，否则只写一半仍欠定义。**给 DATA_FLOW 处理者** |
| 2-12 | 已改（`ARTIFACTS.md`） | `候选栈数值权重 = support×SNR² 或等权(1.0) … 已消除(与 UPM 权重分离命名)` | `**单一权重口径**：候选栈数值权重 = 调用方构造的逐样本逆方差 w = SNR²/F_ref² = 1/σ_F²（ivar 产品，或 ivar 缺失时的帧级 SNR 逆方差链）…reducer 不编码 ivar/SNR 科学策略；weights=nullptr 是本 C API 的输入合同（无权重数组 ⇒ 等权），**不是可选权重口径**，生产唯一调用方恒传 weights` | `sed -n '1,40p' lib/algorithms/coverage/include/astro/phase2/integrate.h` → `**单一权重口径** —— 唯一生产策略 = 调用方构造的逐样本逆方差权重 w = SNR²/F_ref² = 1/σ_F² … 原 stack.support_x_snr2.v1（weight_mode=0…）与 stack.equal.v1（weight_mode=1…）两个可选口径及其 weight_mode 选择键**已删除**`；`grep -rn "weight_mode" lib/infrastructure/scheduler/src/module_adapters.cpp` → `:11948 if (doc.contains("weight_mode"))` / `:11950 "weight_mode 已删除：不存在「权重模式」"`（生产侧 fail-closed 拒绝）。**审稿员成立** |
| 2-12 结论段 | 已改 | `结论：weight 在 integrate 与 UPM 两处语义已显式分离命名（\`stack.*.v1\` vs \`upm.robust_control_weight.v1\`）` | `结论：weight 只有两个各自具名的物理量，无选择键、无口径枚举 —— 帧间积分的逐样本逆方差 w = SNR²/F_ref² = 1/σ_F²（integrate/rejection 共用），与 UPM 控制点的 raw 分子 + per-control 归一化` | `stack.*.v1` 两个名字按 `integrate.h` 已删除，保留即为历史叙事 |
| 2-12 登记 | 登记（不在我白名单） | `UNIFIED_OBJECTS.md:5`、`:132` 对应文字 | 未改 | **给 UNIFIED_OBJECTS 处理者**：与本文件口径一致即可，无需再改 |
| 2-14 | 已改 + **schema 需修订登记** | 三角色最小平面集 = `signal, support, variance, mask` / `signal, support, mask` ×2；`plane_id` 集合 `{signal, support, variance, ivar, mask}`；JSON 示例含 `mask` 面；内容语义含 `mask=坏点/质量位掩码`；`b. 最小平面集` 三条均含 mask | 三角色最小平面集统一为 **`signal + support + variance`**（按三个生产者的实际面重写）；`plane_id` 集合 = **`{signal, support, variance, ivar}`**；JSON 示例删 `mask` 面；units 值域删 `bitmask`；并写明「本合同不含 mask 面」的三条代码依据 | `ls eng/contracts/schemas/unified/` → 13 个对象 schema + `port_contract.schema.json`，**无 mask**；`sed -n '34,52p' lib/infrastructure/aio/include/aio_hips.h` → `AioHipsProductFlag{SIGNAL=1,SUPPORT=2,SNR=4,VARIANCE=8,IVAR=16,NREJ=32,NUSED=64}`，**无 MASK**；`grep -n "EXTNAME" lib/algorithms/fits_output/p3_output.cpp` → 只 `COVERAGE`/`VARIANCE`/`IVAR`，`p3_output.h:112` → `begin_hdu 逐个 HDU 建（PRIMARY=signal → COVERAGE → VARIANCE → IVAR）`，**无 MASK**。**审稿员成立**。**schema 需修订**：`eng/contracts/data/phase_product_exchange.schema.json` 的 `/$defs/plane/properties/plane_id/enum` 与 `phase_product_exchange_validator.py:57 _PLANE_ID_SET` 仍含 `mask`（见 §3） |
| 2-15 | 已改（只改文档） | `coordinate.frame` 必须 `icrs`（无映射说明） | 新增整节「coordinate 的 frame 取值映射」：`hips_frame=equatorial`（IVOA HiPS 1.0 §4.4.1 标准写法，即 ICRS）⇒ `coordinate.frame=icrs`；`galactic`/`ecliptic` ⇒ 显式拒绝 | `grep -n "hips_frame" lib/infrastructure/aio/src/hips/aio_hips_writer.cpp` → `:1902` 与 `:2082` 均写 `"equatorial"`，注释 `:1894-1895`「ICRS 参考系的标准写法是 "equatorial"… "icrs" 不是标准取值」；`sed -n '204,212p' phase_product_exchange_validator.py` → `if coord.get("frame") != "icrs"`。**订正审稿定性**：两者是**两份工件上的两个不同字段**（HiPS `properties.hips_frame` vs 交换对象 `product_content.coordinate.frame`），validator 只校验交换对象文档、不读磁盘，**不构成 validator 需修订**；矛盾只存在于映射面，已由新增节定为该映射正本 |
| 8-7 | 已改 | `- \`tree_hash\` = sha256(规范 JSON 序列化的 tree 条目数组) → **可重算**：…` | `\`tree_hash\` = **归一后**的 sha256 → **可重算**：归一规则（唯一实现 = \`hips_output_store.py\` 的 \`tree_hash\`，与 \`HIPS_STORAGE_FORM.md\`[2] 的哈希口径同源）：① 每个条目归一为**三元组** \`(path,size,sha256)\`（非对象数组）；② 按 \`(path,size,sha256)\` 字典序升序；③ 紧凑 JSON（UTF-8、\`ensure_ascii=false\`、分隔符 \`(",",":")\` 无空白）；④ 对该字节串取 sha256` | `sed -n '103,123p' lib/infrastructure/aio/io/hips_output_store.py` → `norm.append((rel,int(size),sha))`；`norm.sort(key=lambda t:(t[0],t[1],t[2]))`；`payload=json.dumps(norm,ensure_ascii=False,separators=(",",":"))`；`hashlib.sha256(payload.encode("utf-8")).hexdigest()`。另核三个重算入口存在：`tree_hash`(:103)、`recompute_tree_hash`(:126)、`verify_tree_hash`(:850)。`contracts/HIPS_STORAGE_FORM.md:135` 的口径与实现一致。**审稿员成立（ATOMIC_PUBLISH 欠定义）** |
| 2-10 | 登记（不在我白名单） | `LOG_AND_ERROR.md:185-186` `INVALID_INPUT` vs `:210` `INPUT_CORRUPT` | 未改 | 未复核（白名单外，且审稿已给证据）。**给 LOG_AND_ERROR 处理者** |

### C 类 · 语言与历史叙事

| 编号 | 处置 | 改前逐字 | 改后逐字 | 复核 |
|---|---|---|---|---|
| 9-3 | 已改 | `### 加性顶层键 \`storage\`（运行级形态事实；R-42/P-181）` | `### 加性顶层键 \`storage\`（运行级形态事实）` | AGENTS §5 |
| 9-6 | 已改 | `独立 \`verify\` 命令不在命令面上（CLI-001 唯一命令树；verify* 为已删别名 → rc=2，负例锁定于 \`eng/tests/cli/test_cli_protocol.py\` test_03）` | `命令面不含 \`verify*\`；调用返回 rc=2。**判据无载体**：锁定该退出码的负例（\`test_03\`）在仓内不存在（\`eng/tests/\` 无 CLI 测试面）` | 纯现行口径正面表述 |
| 9-4 | 已改 | `- **同名异型消歧（P-180）**：本键的 \`$defs\` **曾**与运行级清单引用**同名** \`coverage_index_ref\`…**现**两者分名…与死键台账**已同步**` | `- **同名异型消歧**：两个引用面**分名**，同名异型一律禁止 —— **输入配置路径** = …（字符串）、**输出清单引用** = …（对象）；同名异型属机器可读面的最坏形态。…与死键台账**同步一致**` | 删流水号 + 删「曾/现/已」历史叙事，改为纯现行正面表述。**注**：本行的 `9-4` 修正在本代理工作期间被并发代理先落，本代理核到后未重复改写，保留了对方版本（内容等效） |
| 9-7（额��） | 已改 | `CLI_PROTOCOL.md:10-16` 元信息块 `> ID: API-CLI-001 状态: FROZEN 上游: API-001/ARCH-002 …` | 整块删除；**真实内容未丢**：`phase1\|2\|3` 用户命令与 9 个别名（`config *`/`modules *`/`selftest`/`test synthetic`/`verify*`/`drizzle`/`benchmark cpu`/`verify-profile`/`hardware inspect`）不在命令面上、调用返回 rc=2 这条断言，移入「命令树」一节作正文段落 | 派单未列此条，但它在我白名单文件里且直接违 AGENTS §5「开头无元信息块」，且含 5-2 的 ARCH-002。删除元信息块时**只删元信息、保留断言**，遵硬红线 3 |
| V6 措辞 | 已改 | `- \`v6_mode_route\`：V6 路由登记（…）` | `- \`v6_mode_route\`：模式路由登记（…）` | 标识符 `v6_mode_route` 是冻结机器合同、必须保留（`protocol.h:54`、`:90`、`mode_gate.h:60`、`jsonl_event_v1.schema.json:53`）；只去掉散文里的「V6」版本代次读法 |
| 全量横扫 | 已改 | — | 白名单 10 份内流水编号/版本代次/历史叙事清零（见 §5 命令 3 的词边界复扫） | 4 处残留全为 `V1 顶层形态`（配置形态名，非版本号），保留 |

### 7-1 / 10-1 · 指向不存在路径的判据/用例引用（逐条）

跑派单脚本（修了一处脚本缺陷：扩展名交替顺序 `.c` 先于 `.cpp`，会把 `commands.cpp` 误截成 `commands.c`）。

| 路径 | 原引用位置 | 处置 |
|---|---|---|
| `eng/tests/cli/test_cli001_vpi.py`、`eng/tests/cli/test_phase123_pipeline.py` | CLI_PROTOCOL「回归锚」 | 改指无载体 + 注明 `eng/tests/` 只有 `conformance/`、`validation/` 两子目录 |
| `eng/tests/cli/test_cli_protocol.py` | MANIFEST_VERIFY ×2 | 同上（负例 test_03 与 golden 组） |
| `eng/tests/backend/test_cpu_profile.py` | MANIFEST_VERIFY cpu profile | 改指现存 oracle `eng/tools/validate_cpu_profile.py`（已核 EXISTS），并标消费侧测试无载体 |
| `eng/tests/io/test_hips_output_contract.py`、`eng/tests/io/hips_output_fixture.py` | ATOMIC_PUBLISH 模块归属表 | 两行改标「判据无载体」；正本保留现存实现 `hips_output_store.py` / `fits_verify.py` |
| `lib/infrastructure/aio/tests/test_hips_atomic_publish.cpp` | ATOMIC_PUBLISH 原子性适用范围 | 改标「判据无载体」（`lib/infrastructure/aio/` 无 `tests/` 目录） |
| `eng/tests/artifact/test_phase_product_exchange.py` | PHASE_PRODUCT_EXCHANGE ×2 | 改指现存校验器 `phase_product_exchange_validator.py` + 标测试面无载体 |
| `eng/tests/artifact/test_provenance.py`、`eng/tests/artifact/test_production_store.py` | PROVENANCE ×5 | 改指现存实现 `provenance.py`/`production_store.py` + 标验收用例与基线均无载体 |
| `eng/tests/contracts/test_unified_object_contract.py` | ARTIFACTS 机器门 | 标「判据无载体」，保留 canonical schema 自身为唯一可校验面 |

**超出审稿条目、但同属「不得写成已生效」的诚实订正**（登记为我的主动处置）：
- ATOMIC_PUBLISH 验收映射表 9 个测试类（`TestUniqueRunDirIsolation` … `TestFitsVerifyCrossOracle`）→ 逐名
  `grep -rl` 于 `lib/`+`eng/` 全 0 命中。表改为「验收要求」并加**状态列**，9 行全部标 `判据无载体`，
  只保留「非生产/诊断接口不被生产路径引用」为 `可人工复跑`。
- 7-3 `ACSD_TEST_CORRUPT_AFTER_RENAME=1` 注入器 → `grep -rn` 全仓仅 ATOMIC_PUBLHD 一处自述，
  无实现 → 改标「**判据未接线**」，删掉「即…可执行证据」这类已生效措辞。

改后白名单内仍指向不存在路径的引用**只剩 4 条，全部在显式「判据无载体」句子里**（点名那个待落库的载体，
以便落库时按名对齐）：`PROVENANCE.md:108`、`MANIFEST_VERIFY.md:22`、`ARTIFACTS.md:46`、`ARTIFACTS.md:72`。

---

## 2. 我推翻的审稿判定（保留审稿原判）

| 编号 | 审稿原判 | 我的复核 | 结论 |
|---|---|---|---|
| **5-10** | `ARTIFACTS.md:41-60`（13 行）、`:30` 与 `PHASE_PRODUCT_EXCHANGE.md` 里出现 `docs/detail/common/UNIFIED_MODEL` | `grep -rn "docs/detail/common" docs/engineering/` → 命中**只有** `UNIFIED_OBJECTS.md:4`、`:46` 两行。我白名单的两个文件里**从未出现过** `docs/detail/common` 这个串（它们写的是裸 `UNIFIED_MODEL`） | **审稿的「出现在哪些文件」清单不成立**；审稿的「`docs/detail/common/` 不存在、实际是 `docs/detail/UNIFIED_MODEL.md`」**成立且保留**。我只在自己白名单内把裸名补成真实路径 |
| **5-12** | 两篇各用一个名字，指向 UNIFIED_MODEL 的同一节 | `grep -n "^#" docs/detail/UNIFIED_MODEL.md` → 全文只有 3 个二级节：`1. 统一线性观测模型`、`2. 数据对象（各自具名）`、`3. 三类配置严格分离`。**两个争议节名都不是 UNIFIED_MODEL 的真实节名** | 审稿的「同一节两个名字」框架**不准确**——是两个都不存在的名字。**但审稿要求的「统一到一处」结论正确且已执行**：统一到真实存在的 `## 2. 数据对象（各自具名）`。选它的理由：它是该文件唯一承载 13 个对象表与「可否作权重」列的节（`:37-56`），UNIFIED_OBJECTS.md 的「可否作权重」列正是照抄该表 |
| **2-15** | 「合同要求 icrs，而 HiPS 生产者写 equatorial」⇒ 要么改 writer，要么 validator 需修订 | 见上，生产者/校验器各自面对**不同工件的不同字段** | **「validator 需修订」不成立**，我按派单要求写成显式映射节。但**审稿发现的不一致是真的**，只是定性为「映射面缺正本」而非「两处取值互斥」 |
| **5-1（部分）** | `「命令树」一节.2` / `「JSONL 运行事件流」一节.5` 都是错节名 | 「命令树」本身**是**真实节名（`### 7.1 命令树`），错的只是 `.2` 后缀；JSONL 事件流**根本不是节**，是 `### 7.2` 内一句正文 | 审稿「三个节名一个都不存在」对「配置与 output_dir」「JSONL 运行事件流」成立，对「命令树」不成立（缺 `.2` 而非缺节名）。已按真实节名改 |

---

## 3. 需代码侧订正的问题

| # | 位置 | 问题 | 证据 |
|---|---|---|---|
| C-1 | `eng/contracts/data/phase_product_exchange.schema.json` `/$defs/plane/properties/plane_id/enum` | 枚举含 `mask`，但 `mask` 无生产者、无 canonical 统一对象 schema。**应删除 `mask`** | `python3` 读 schema → `{"type":"string","enum":["signal","support","variance","ivar","mask"]}`；`ls eng/contracts/schemas/unified/` 无 mask |
| C-2 | `lib/infrastructure/aio/runtime/artifact_store/phase_product_exchange_validator.py:57` | `_PLANE_ID_SET = {"signal","support","variance","ivar","mask"}` 同上，**应删除 `mask`** | `sed -n '57p'` 逐字；`:250-251` 据此放行 |
| C-3 | `docs/engineering/architecture/DATA_FLOW.md:142`（非我白名单） | 写 `raw_w = quality_factor × control_ivar` 只写了第一阶段；应补第二阶段 `w_cell = raw_w/Σ_cell(raw_w) × control_reliability`，否则该处**只写一半仍欠定义** | `upm.cpp:705` 与 `upm.h:188` |

---

## 4. 需权威补充的问题

| # | 问题 | 为什么我不能自己定 |
|---|---|---|
| A-1 | `PHASE_PRODUCT_EXCHANGE.md` 三个角色的最小平面集，我按代码写成 `signal + support + variance`，**未含 `ivar`** | `AioHipsProductFlag` 有 `IVAR=16`、`p3_output` 也写 `IVAR` 扩展 HDU，所以 `ivar` **有生产者**；但三角色是否**必须**都有 ivar，属「最小集」的合同决策，不是代码能回答的。代码只证明「ivar 面存在」，不证明「必需」。需负责人裁定最小集是否含 ivar |
| A-2 | Phase3 FITS 的 `COVERAGE` 扩展 HDU 与交换对象 `plane_id` 的 `support` 是否同一物 | `p3_output.h:54` 写「扩展=coverage 二值」，而 `support` 的语义是 [0,1] 连续覆盖度。二者可能不是同一面。我按代码原样写「扩展 HDU = COVERAGE/VARIANCE/IVAR」并**未**把它等同于 `support`，需权威确认映射 |
| A-3 | `ARTIFACTS.md` 大量单元格里 `DATA_SEMANTICS a` / `DATA_SEMANTICS 「机器校验」一节` 等残留指代 | 目标文件存在（`docs/science/unified/DATA_SEMANTICS.md`），但这些节名/条目名在目标文件里查不到对应标题（真实节名为 `3.1 坐标语义`…`3.9 权重词表`）。**我未凭猜测改写**，登记待权威给出正确落点 |
| A-4 | `CLI_PROTOCOL.md` `v6_mode_route` 事件 kind 的散文名 | 机器合同名必须保留；其对应的「V6 模式路由」在文档层是否还有一个更正式的名字，需权威给出。我只去掉了版本代次读法 |
| A-5 | `PHASE_PRODUCT_EXCHANGE.md` 引用 `docs/science/drizzle/DRIZZLE.md` | 该路径存在，但不在我白名单，我未核其节名是否支持当前那句「drizzle 侧正本见」的断言 |

---

## 5. 你否决了哪些

| 编号 | 审稿要求 | 我的处置 | 依据 |
|---|---|---|---|
| 5-7 | 全车道 521 处 `「X」一节` 一次性降为论文格式编号引用 | **本轮不批量做**，只做 5-4/5-5/5-6 三类**失真**修复 + 我白名单参考文献节编号化 | 审稿条目自己写「**分批**降为论文格式编号引用；先修 5-4/5-5/5-6 三类失真」。全车道 521 处跨 10+ 文件、含他人白名单，单代理批量改会与并行代理对撞（本次已实测到 HIPS_STORAGE_FORM / OWNERSHIP_LIFETIME 被并发改写） |
| 5-4 | 「多半是同文件的『索引 schema』『哈希口径』『形态的输入配置与输出清单字段』三节」 | **不接受这句推测**，逐处按内容定真实节名（实际用到 7 个不同节名，不是 3 个） | 例：`:203` 的 M2 只在「运行完成清单 `manifest.json#storage`」节；`:204` 的 REJECT 只在「mosaic / export：形态键必须 REJECT」节。照搬「三节」会造出新的错误指向 |

**未删除任何真实内容**（硬红线 3）：9-7 删元信息块时把其中的命令面断言完整移入正文；2-12 改权重行时把
「support×SNR² / 等权」替换成现行口径并保留「reducer 不编码科学策略」这一约束；2-14 删 `mask` 时保留了
`units` 值域、诊断平面不变量、稀疏 SNR 层不入枚举等全部其余条款。

---

## 6. 自证段

**A. 我实际跑的命令与关键输出**

- A-1 真实节名核对：`grep -n "^#" docs/ACSD_DESIGN.md`（全文节名表）、`grep -n "^#" docs/detail/UNIFIED_MODEL.md`
  → `15:## 2. 数据对象（各自具名）`、`grep -n "^#" docs/engineering/contracts/HIPS_STORAGE_FORM.md`（19 个标题，供 5-4 逐处对名）、
  `grep -n "^#" docs/science/unified/DATA_SEMANTICS.md`、`grep -n "^#" docs/science/PHASE3_HIPS_TO_FITS.md`。
- A-2 代码正本：`sed -n '173,190p' …/upm.h`、`sed -n '2024,2058p' …/upm.cpp`、`sed -n '691,707p' …/upm.cpp`、
  `sed -n '1,40p' …/integrate.h`、`grep -rn "weight_mode" lib/…/module_adapters.cpp`、
  `sed -n '34,52p' lib/infrastructure/aio/include/aio_hips.h`、`grep -n "EXTNAME" lib/algorithms/fits_output/p3_output.cpp`、
  `sed -n '103,123p' lib/infrastructure/aio/io/hips_output_store.py`、`grep -n "hips_frame" lib/infrastructure/aio/src/hips/aio_hips_writer.cpp`、
  `sed -n '204,212p' …/phase_product_exchange_validator.py`、`ls eng/contracts/schemas/unified/`。
- A-3 并发冲突实测：我的 `edit` 两次报 `file changed since it was read`，`md5sum` 从 `fcba8439…` 变为 `814aaff7…`，
  证明另一代理在我读与写之间改了 HIPS_STORAGE_FORM.md。随后我改用「`assert count==1` 才写」的定点替换，
  并逐次复验 12 个锚点与对方改动共存。
- A-4 我发现并修正了自己的一处脚本缺陷：派单给的破路径扫描脚本扩展名交替顺序使 `.c` 先于 `.cpp` 匹配，
  把 `commands.cpp`/`aio_fits.cpp` 等**存在**的文件误报为断链；修正后断链数从 17 降到 11（真实值）。

**B. 派单要求的四条验证命令与实际输出**

```text
$ grep -rn "「」" docs/engineering/contracts/HIPS_STORAGE_FORM.md docs/engineering/data/
(无输出, exit 1)                                   # 应为 0 ✓

$ grep -rn "ARCH-00[0-9]" docs/engineering/contracts/ docs/engineering/data/
0 命中 ✓

$ grep -rn "docs/detail/common" docs/engineering/contracts/ docs/engineering/data/
0 命中 ✓

$ grep -rnE "V[0-9]{1,2}[A-Z]?|R-[0-9]+|P-[0-9]+|已删|旧版" <我白名单 10 份>
命中仅 4 处，全为 `V1 顶层形态`（配置形态名）+ 9 处 `R-001`（`DATA-IMG-VAR-001` 的子串）+ 1 处 `P-001`
$ grep -oE ... | sort | uniq -c            # 逐 token 证明为标识符误报，非流水编号
  6 CLI_PROTOCOL.md:V1        1 MANIFEST_VERIFY.md:V1
  9 ARTIFACTS.md:R-001        1 ARTIFACTS.md:P-001
$ grep -on "DATA-IMG-VAR-001|ValidateEventV1|kEventFieldsV1|V1 顶层形态" ...
DATA-IMG-VAR-001 / ValidateEventV1 / kEventFieldsV1 / V1 顶层形态   # 标识符，非版本号 ✓

$ python3 孤儿条目扫描（全 contracts/ + data/）
ATOMIC_PUBLISH []   CLI_PROTOCOL []   HIPS_STORAGE_FORM []   MANIFEST_VERIFY []
OWNERSHIP_LIFETIME []   ARTIFACTS []   ARTIFACT_STORE []
PHASE_PRODUCT_EXCHANGE []   PROVENANCE []                      # 我白名单 9 份全部清零 ✓
ASYNC_IO ['1','2','3']  CONFIG ['1'..'5']  LOG_AND_ERROR ['1'..'5']
PIPELINE_BLOCK ['1','2','3']  RUNTIME ['1','2','3']  SCHEDULER ['1','2','3']
                                       # 以上 6 份不在我白名单，未动
```

**C. 我核了但结论「核对不到 / 未核」的**

- `docs/science/PSF_SIGNAL_WEIGHT.md`：**不存在**（`integrate.h:16` 引用了它）。我白名单文件未引用该路径，
  故未处理；登记给 SCI 车道。
- `PHASE_PRODUCT_EXCHANGE.md` 里 `SCI-P3 a.8`、`DATA-002 「…歧义映射」一节a` 指向的原始条款：
  **核对不到** —— `grep -n "count_field\|n_rejected_nonfinite" docs/science/unified/DATA_SEMANTICS.md` → 0 命中，
  该规则实际只存在于 `PHASE_PRODUCT_EXCHANGE.md` 自身的处置规则第 3 条。我把 ARTIFACTS.md 的两处指向
  改到真实存在的那一处，**没有**在 DATA_SEMANTICS 里编造一节（见 §4 A-3）。
- `LOG_AND_ERROR.md` 的 2-10（`INVALID_INPUT` vs `INPUT_CORRUPT`）与 5-3（`机器判据`）：
  白名单外，我只登记、**未复核其内容**。
- Phase2/Phase3 的 `nrej`/`nused`/`n_rejected_nonfinite` 三个诊断平面是否构成「最小平面集」的一部分：
  代码与文档都把它们排除在 exchange science planes 枚举外，我按此写；该判断的合同依据未独立验证。
- 我**没有**编译、**没有**跑测试（本轮纯文档订正，无代码改动可验）。