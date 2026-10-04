# T06 第 1 轮订正 · subA-2（contracts 五份 + data 五份 · 验收复核轮）

订正者：独立子代理 subA-2。白名单与 subA 相同，10 份：
`contracts/{CLI_PROTOCOL,MANIFEST_VERIFY,HIPS_STORAGE_FORM,ATOMIC_PUBLISH,OWNERSHIP_LIFETIME}.md`、`data/{ARTIFACTS,ARTIFACT_STORE,PHASE_PRODUCT_EXCHANGE,PROVENANCE,README}.md`。
git 全程只读，一律 `git -c core.quotepath=false`；无 add/commit/checkout/reset/stash/tag/push。
**白名单外文件一次都没编辑过**（`LOG_AND_ERROR.md`/`CONFIG.md`/`PIPELINE_BLOCK.md`/`SCHEDULER.md` 的 diff 全部来自并行子代理）。

> **与 `T06-第1轮订正-subA.md` 的关系**：派单把交付路径给了我，但该路径已被另一位同域 subA 代理先写（34 KB）。按「别覆盖别人的文件」，我**没有覆写它**，本件写在 `subA-2.md`。两件结论大面积重合（同一派单、同一批文件、最终收敛到同一批真实节名）。本件的**增量**是：① 对 B/C 组重条逐条读生产代码的独立复核；② 一轮全量验收扫描（派单要求的四条验证命令输出）；③ 修掉并发编辑引入的一个悬空引用与一处不存在的节名。

---

## 0. 撞车实况（必须让前台知道）

我开工时工作树干净。作业期间**并发子代理进入了我的 10 份白名单中的 9 份**，且改动是逐行落地的（我用 `git diff` 与逐次读到的内容对比确认）。按 AGENTS §2「不回滚覆盖」，我一条都没回退，改为逐条核验 + 只补遗漏。

我亲手落笔的文件与条目：**HIPS_STORAGE_FORM.md**（元信息块 + 四件事→六件事 + 12 处空锚 + LOG_AND_ERROR 假节名 + P-180/「曾」+ 6 条参考文献标注）、**OWNERSHIP_LIFETIME.md**（悬空引用修复）、**ATOMIC_PUBLISH.md**（三处指向 HIPS_STORAGE_FORM 的假节名）、**MANIFEST_VERIFY.md**（一处假节名）、**ARTIFACTS.md**（登记行权威锚点整段重写）、**PROVENANCE.md**（验收映射表头一句）。

---

## 1. 逐条处置表

处置取值：**已改** / **核验通过（并发代理已闭合）** / **部分已改** / **待裁决** / **只登记**。

| 意见 | 处置 | 改前逐字 | 改后逐字 | 我的复核命令与输出 |
|---|---|---|---|---|
| **5-1** CLI 引三个不存在的节名 | 核验通过 | `> 上游：ACSD_DESIGN.md 「配置与 output_dir」一节.1（命令树）、…一节.2（配置、事件与退出码）`；`命令语义（「配置与 output_dir」一节.1 薄入口）：`；`…（\`../../ACSD_DESIGN.md\` 「命令树」一节.2）`；`（\`../../ACSD_DESIGN.md\` 「配置与 output_dir」一节.2）`；`…由 …「JSONL 运行事件流」一节.5 三档页面 + \`-y\`/\`-force\` 承接…` | 「最高设计的命令行合同一章[1]」／「（最高设计的机器输出与退出码[1]）」／「（最高设计的运行前预检一章[1] 三档页面…）」，`.1/.2/.5` 全删 | `grep -n "^#" docs/ACSD_DESIGN.md` → `324:## 7. 命令行合同` / `326:### 7.1 命令树`（无子节）/ `346:### 7.2 机器输出与退出码` / `352:### 7.3 错误传播与日志`；三档预检页在 `232:### 4.5 运行前预检（三个命令通用）`，`:251`–`:253` 逐字含 correct/warn/error、`-y` 不可越过、`-force` 跳检查。改后 `grep -n "配置与 output_dir\|「JSONL 运行事件流」" CLI_PROTOCOL.md` → 无残留 |
| **5-2** `ARCH-002/004/005` 无定义 | 部分已改 | CLI `:92` `…(与 ARCH-002 「取消与崩溃」一节/ARCH-005 「标准输出与标准错误纪律」一节 原子单元一致)`；CLI `:12` 元信息块；MANIFEST `:23` `## run_manifest.json v1(run 结束原子写, ARCH-002 「落点映射与测试」一节)` | 三处均由并发代理闭合为真实引用；我独立核验并确认 `ARCH-002/004/005` 确无定义 | `grep -rn "ARCH-002\|ARCH-004\|ARCH-005" docs/ eng/ lib/` → 仅剩引用本身与 `lib/infrastructure/cli/commands.cpp:529` 注释；`grep -n "ARCH" docs/DOCUMENT_INDEX.yaml` → **只有 `ARCH-001`**（`:91`）；TRACEABILITY `:63` 只定义 `ARCH-[A-Z0-9]+` 正则命名空间。改后 `grep -rn "ARCH-00[0-9]" contracts/ data/` → **0 命中** |
| **5-3** LOG_AND_ERROR 引「机器判据」 | 只登记 + 已失效 | `LOG_AND_ERROR.md:257` | 未改（不在白名单）。**但该行号已不存在** | `wc -l LOG_AND_ERROR.md` → **199**（审稿时 317）；`grep -n "机器判据" LOG_AND_ERROR.md` → **0**；`sed -n '253,260p'` → 空。该文件已被并发代理重写并拆出 `docs/engineering/standards/ERROR_MODEL.md`。**5-3 现已无处落地**，请前台向该代理确认闭环 |
| **5-4** HIPS_STORAGE_FORM 12 处空锚 `「」` | **已改（本人亲手）** | 12 处：`「」一节`×2（`:27`）、`:140`、`:205`、`:206`、`:234`、`:254`、`:259`、`:265`、`:266`、`:268`、`:282` | 逐处补回真实节名，见 §1.1 | `grep -c "「」"` → 12；改后 `grep -rn "「」" HIPS_STORAGE_FORM.md data/` → **0 命中** |
| **5-5** PHASE_PRODUCT_EXCHANGE 10 处空主语/空槽 | 核验通过 | `最高设计 ：`、`（最高设计 ，`、`；：`、`（/ ）`、`该文件 （` | 全部改为可核验的真实章/节名 | `grep -n "最高设计 ：\|（最高设计 ，\|；：\|（/ ）\|该文件 （\|最高设计 ，" PHASE_PRODUCT_EXCHANGE.md` → **0 命中**。我逐个核对了替换目标：`### 8.1 总原则：唯一入口、阶段独立调度器`（`ACSD_DESIGN.md:365`「一次调用只驱动一个阶段」、`:366`「阶段间唯一交换媒介是磁盘 HiPS 与 manifest」）、`### 2.2 P2 跨帧绝对信噪比`（`:108` `SNR=F_ref/σ_F`、`:111` `w=SNR²/F_ref²`）、`### 5.5 逐像素排异`（`:295`「NaN 采用样本级掩膜，覆盖级缺数置 NaN 并计数」）、`## 10. I/O 与原子产品`（`:458`「无覆盖、无数据用 NaN」）、`### 6.3 投影算法`（`:323`「导出只接受面亮度语义输入，方差/逆方差显式消费并传播」）——**无一处是我凭印象填的** |
| **5-7** 机械跳转锚 | 部分已改（降级） | 「「X」一节」遍布 | 只做**跨文档**假节名清理 + 编号引用改造；文件内自指保留 | AGENTS §5 禁的是跨文档跳转锚。全车道 521 处不在我车道内，见 §5 |
| **5-8** 参考文献与正文脱钩 | 部分已改 | 9 份正文 `[n]` = 0，条目全孤儿 | 9 份全部 **孤儿 0 / 悬空 0** | 见 §2 验证第 5 条 |
| **5-10** `docs/detail/common/UNIFIED_MODEL` 不存在 | 已改 | 裸写的 `UNIFIED_MODEL`（ARTIFACTS 17 处） | `\`docs/detail/UNIFIED_MODEL.md\`「数据对象（各自具名）」一节` | `ls -d docs/detail/common` → **不存在**；`find docs -name "UNIFIED_MODEL*"` → 只 `docs/detail/UNIFIED_MODEL.md`。**并纠正审稿一处**：该字符串在我白名单内本就 0 处（审稿的 `ARTIFACTS.md:30` 说的是裸 `UNIFIED_MODEL` 形态）；该串全仓只在 `UNIFIED_OBJECTS.md:4`/`:46` 命中 —— 那份不在我白名单，见 §4-⑥ |
| **5-12** 同一节两个名字 | 已改 + **部分推翻审稿表述** | ARTIFACTS 叫「weight/value/scale/sigma/snr 歧义映射」；UNIFIED_OBJECTS 叫「13 个对象 → canonical schema → schema ID」 | 统一到 `docs/detail/UNIFIED_MODEL.md`「数据对象（各自具名）」一节 | 见 §3-③ |
| **5-13** `governance/TRACEABILITY.md` 解析错 | 部分已改 + **我补一处新错** | ATOMIC `:63`、OWNERSHIP `:26`：`governance/TRACEABILITY.md 「合同 ID 登记面」一节` | 路径由并发代理修为 `../governance/TRACEABILITY.md`。**我补**：该节名本身也不存在 | `ls docs/engineering/contracts/governance` → 无；`grep -n "合同 ID 登记面" TRACEABILITY.md` → **0 命中**；`grep -n "^## " TRACEABILITY.md` → 真实节是 `## ID 格式（机器正则）`(:54)、`## 需求→实现→测试 登记册（人读正本）`(:259)，`ENG-IO-001` 登记在 `:277`（落在后者内）。我已把 OWNERSHIP 的引用改指该真实节名 |
| **9-3** 标题含 R-42/P-181 | 核验通过 | `### 加性顶层键 \`storage\`（运行级形态事实；R-42/P-181）` | `### 加性顶层键 \`storage\`（运行级形态事实）` | 见 §2 验证第 4 条的逐串拆解 |
| **9-4** 同名异型消歧（P-180） | **已改（本人亲手）** | `- **同名异型消歧（P-180）**：本键的 \`$defs\` **曾**与运行级清单引用**同名** \`coverage_index_ref\`——…现两者分名：…与死键台账已同步。` | `- **同名异型消歧**：运行级清单引用取名 \`coverage_index_ref\`、在本合同机器事实源里是**对象**（…见「运行完成清单 \`manifest.json#storage\`（加性）」一节）；mosaic 输入侧的路径键取名 \`coverage_index_path\`、是**字符串**（\`eng/contracts/schemas/phase_config_mosaic.schema.json#/$defs.coverage_index_path\`）。…与死键台账（\`eng/contracts/ledgers/dead_config_keys.json\`）按此名对齐。` | 删流水号 + 删「曾」+ 把两侧真实 schema 路径写全。`grep -rnE "曾\|P-[0-9]+" <我白名单 10 份>` → **0 命中** |
| **9-6** 「verify* 为已删别名」 | 核验通过 | `独立 \`verify\` 命令不在命令面上（CLI-001 唯一命令树；verify* 为已删别名 → rc=2，负例锁定于 \`eng/tests/cli/test_cli_protocol.py\` test_03）。` | `命令面不含 \`verify*\`；调用返回 rc=2。**判据无载体**：锁定该退出码的负例（\`test_03\`）在仓内不存在（\`eng/tests/\` 无 CLI 测试面）。` | 纯现行口径正面表述成立。`ls eng/tests` → 只有 `conformance/`、`validation/` |
| **2-11** UPM 权重三套互斥公式 | 核验通过（**我读实现确认**） | `\| \`upm.robust_control_weight\` \| upm.cpp \| UPM 控制点权重 = quality×geom×control_ivar \| … \| 已消除 \|` | `…UPM 控制点权重**分两个阶段**（正本 = \`p2_upm_raw_weight\` 与其后的 per-control 归一化）：**① 分子** \`raw_w = quality_factor × control_ivar\`（production，\`cfg.use_ivar_weight != 0\`；**几何可靠性不在分子**；\`control_ivar ≤ 0\`/非有限 ⇒ rc=2 显式拒绝）；**② per-control 归一化** \`w_cell = raw_w / Σ_cell(raw_w) × control_reliability\`（**几何可靠性在这一步施加**）…` | **我自己读生产代码**：`sed -n '173,186p' lib/algorithms/coverage/include/astro/phase2/upm.h` → 头注「production（cfg.use_ivar_weight != 0）: raw_w = quality_factor × control_ivar（**几何可靠性在 per-control 归一化中施加**）」；`sed -n '2043,2052p' lib/algorithms/coverage/src/upm.cpp` → `if (cfg.use_ivar_weight != 0) { const double civ = obs->control_ivar; if (!std::isfinite(civ) \|\| civ <= 0.0) return 2; *out_raw = qf * civ; return 0; }`，注释「science 权重只含 quality × control_ivar。无 star-SNR / support^p / 单像素 ivar 因子」；归一化在 `sed -n '703,706p'` → `raw_w[i] = raw_w[i] / s * m->controls[ck].reliability;`（`reliability` 来自 `cfg.control_reliability`，`:392` 缺省 1.0）。**⇒ 审稿员判定成立，原文把「归一化阶段施加」误写成「分子乘入」，与代码相反** |
| **2-12** 已退役权重口径被当现行正本 | 核验通过（**我读实现确认**） | `候选栈数值权重 = support×SNR² 或等权(1.0)` / 歧义状态 `已消除` | `**单一权重口径**：候选栈数值权重 = 调用方构造的逐样本逆方差 \`w = SNR²/F_ref² = 1/σ_F²\`…；\`weights=nullptr\` 是本 C API 的输入合同（无权重数组 ⇒ 等权），**不是可选权重口径**；生产唯一调用方恒传 \`weights\`` / 歧义状态 `明确（无 \`weight_mode\` 选择键、无权重口径枚举）` | `sed -n '1,25p' lib/algorithms/coverage/include/astro/phase2/integrate.h` → 「**单一权重口径** —— 唯一生产策略 = 调用方构造的**逐样本逆方差**权重 w = SNR²/F_ref² = 1/σ_F²…原 \`stack.support_x_snr2.v1\`（weight_mode=0）与 \`stack.equal.v1\`（weight_mode=1 → 等权）两个**可选口径**及其 weight_mode 选择键**已删除**…\`weights=nullptr\` 是**本 C API 的输入合同**（无权重数组 ⇒ 等权），不是可选择口径」。**⇒ 审稿员判定成立**；结论段的 \`stack.*.v1\` 历史命名也已被换成现行两口径表述 |
| **2-14** 最小平面集含无生产者的 `mask` | 核验通过（**我核三处生产者**） | 三产品最小平面集都含 `mask`；`plane_id` 枚举 `{signal,support,variance,ivar,mask}` | 三个角色都**不含 `mask`**；并写明「schema 面应与本节同步删除 \`mask\`；修订落地前交换对象文档不得声明 \`mask\` 面」 | `ls eng/contracts/schemas/unified/` → 13 个对象 schema + `port_contract`，**无 mask**；`sed -n '35,52p' lib/infrastructure/aio/include/aio_hips.h` → `SIGNAL=1/SUPPORT=2/SNR=4/VARIANCE=8/IVAR=16/NREJ=32/NUSED=64`，**无 MASK**；`grep -n "EXTNAME" lib/algorithms/fits_output/p3_output.cpp` → `COVERAGE`/`VARIANCE`/`IVAR` + 主 HDU，**无 MASK**；`grep -n "mask" aio_hips.h` → 只有入参 `valid_mask`（covered_area>0 有效位），不是产品面。**⇒ 审稿员判定成立**。**但 schema/validator 仍带 `mask`，已如实登记为「schema 需修订」，见 §4-①** |
| **2-15** `coordinate.frame` icrs vs 生产者 equatorial | 核验通过 | 只写「frame 必须 \`icrs\`（唯一允许）」 | 新增映射表：`hips_frame = equatorial`（IVOA HiPS 1.0 §4.4.1 标准写法，即 ICRS）⇒ `coordinate.frame = icrs`；`galactic`/`ecliptic` ⇒ **显式拒绝**；并写明校验器只校验交换对象文档、不读磁盘 | `grep -n "hips_frame" lib/infrastructure/aio/src/hips/aio_hips_writer.cpp` → `:1902`/`:2082` 均写 `"equatorial"`，注释引 IVOA HiPS 1.0 §4.4.1「Format: "equatorial" (ICRS)」并注「"icrs" 不是标准取值」；`phase_product_exchange_validator.py:208` → `if coord.get("frame") != "icrs": errors.append(...唯一允许 frame)`；旁证 `docs/science/PHASE3_HIPS_TO_FITS.md:44` 也写 `hips_frame`='equatorial'。**⇒ 审稿员判定成立；我只改文档、没碰代码** |
| **8-7** `tree_hash` 欠定义 | 核验通过 | `\`tree_hash\` = sha256(规范 JSON 序列化的 tree 条目数组) → **可重算**` | `\`tree_hash\` = **归一后**的 sha256`：① 每条目归一为三元组 \`(path,size,sha256)\`；② 按三元组字典序升序；③ 紧凑 JSON（UTF-8 / `ensure_ascii=false` / `(",",":")`）；④ 对该字节串取 sha256 | `sed -n '103,123p' lib/infrastructure/aio/io/hips_output_store.py` → `norm.append((rel,int(size),sha))`；`norm.sort(key=lambda t:(t[0],t[1],t[2]))`；`json.dumps(norm, ensure_ascii=False, separators=(",",":"))`。与 `HIPS_STORAGE_FORM.md`「哈希口径」一节逐项一致。**⇒ 审稿员「ATOMIC 欠定义、非矛盾」的定性成立** |
| **7-1 / 10-1** 死路径判据/用例引用 | 部分已改 + 登记 | 见 §2 验证第 6 条与 §4-⑥ | 全部改为「**判据无载体** / **判据未接线**」 | 见下 |
| **1-10** 「冻结四件事」却列 6 条（**超出派单，顺手修**） | 已改（本人亲手） | `本合同冻结四件事，四者都是**机器可校验**的：` | `本合同冻结六件事，六者都是**机器可校验**的：` | 对读 `:18` 与 `:20`–`:25`；文首第 5 行本就写「本文冻结六件事」 |
| **2-10** LOG_AND_ERROR `INVALID_INPUT` vs `INPUT_CORRUPT` | 只登记 | — | 未改（不在白名单），且已随该文件拆分迁到 `docs/engineering/standards/ERROR_MODEL.md`，见 §4-⑦ |

### 1.1 5-4 逐处补回（12 处，全部取自本文件自己的 `## 标题` 列表，无一臆造）

依据：`grep -n "^#" docs/engineering/contracts/HIPS_STORAGE_FORM.md` → 22 个真实标题。

| 位置 | 补回为 |
|---|---|
| `:27` 压缩档位 | 「压缩档位」一节 |
| `:27` 索引载体演进 | 「载体演进」一节 |
| `:140` H2 storage 段 | 「运行完成清单 `manifest.json#storage`（加性）」一节 |
| `:205` M2 机器证据 | 「运行完成清单 `manifest.json#storage`（加性）」一节的 M2 |
| `:206` mosaic/export REJECT | 「mosaic / export：形态键必须 REJECT」一节 |
| `:234` I4 可重算 | 「索引 schema」一节的 I4 |
| `:254` 同词表 | 「形态的输入配置与输出清单字段」一节 |
| `:259` 形态事实落点 | 「形态的输入配置与输出清单字段」一节 |
| `:265` 回退倒排 | 「数据集级覆盖索引 `coverage.index.json`」一节 |
| `:266` mosaic 固定裸形态 | 「mosaic / export：形态键必须 REJECT」一节 |
| `:268` 同名异型 | 「运行完成清单 `manifest.json#storage`（加性）」一节 |
| `:282` 合并后重算 | 「数据集级覆盖索引 `coverage.index.json`」一节 |

**顺带修的一处交叉引用**：`HIPS_STORAGE_FORM.md:201` 原写 `日志合同 LOG_AND_ERROR.md 「命名与扩展名」一节 与 LOG-001 事件模型` —— 「命名与扩展名」是**本文件自己**的节名，被误挂到了 LOG_AND_ERROR 上。改为经 LOG_AND_ERROR「日志行格式：机器校验格式」一节转发到 `../resources/observability/STRUCTURED_LOGGING.md`「事件模型」一节，并列出 `level`/`event` 的真实取值域（`grep -n "^#" STRUCTURED_LOGGING.md` → `42:## 事件模型`；`:72` `level` 取 `debug/info/warn/error`、`:73` `event` 取 `start/progress/end/warn/error/metric/checkpoint/cancel/trace`）。

---

## 2. 验证（派单四条命令，输出原样）

```console
$ cd "/workspace/Astro CS Database"

### 1. 空章节锚（应为 0）
$ grep -rn "「」" docs/engineering/contracts/HIPS_STORAGE_FORM.md docs/engineering/data/
（0 命中，达标）

### 2. ARCH-00x（应为 0）
$ grep -rn "ARCH-00[0-9]" docs/engineering/contracts/ docs/engineering/data/
（0 命中，达标）

### 3. docs/detail/common（应为 0）
$ grep -rn "docs/detail/common" docs/engineering/contracts/ docs/engineering/data/
（0 命中，达标）

### 4. 流水编号 / 历史叙事
$ grep -rnoE "V[0-9]{1,2}[A-Z]?|R-[0-9]+|P-[0-9]+|已删|旧版" <我白名单 10 份> | sort | uniq -c
      6 docs/engineering/contracts/CLI_PROTOCOL.md:53,60,62,122,126,127 : V1
      1 docs/engineering/contracts/MANIFEST_VERIFY.md:105                : V1
      9 docs/engineering/data/ARTIFACTS.md:{15,16,24,25,28,35,52,53,55,57} : R-001 ×9 / P-001 ×1
→ 逐串查证：V1 = 配置形态名「V1 顶层形态」（`parser.cpp::validate_config_full` 的现行三形态之一，非版本号/流水号）；
  R-001 = `DATA-OBJ-VARIANCE-001` / `DATA-IMG-VAR-001` 等 ID 的子串；
  P-001 = `DATA-REJ-MAP-001` 的子串。**无一处真流水编号。**
$ grep -rnoE "曾|原句|作废|已从|已退役|历史上" <我白名单 10 份>
（0 命中，达标）

### 5. 参考文献孤儿 / 悬空（派单脚本）
$ python3 - <<'PY'
contracts/CLI_PROTOCOL.md        孤儿 [] 悬空 []
contracts/MANIFEST_VERIFY.md     孤儿 [] 悬空 []
contracts/HIPS_STORAGE_FORM.md   孤儿 [] 悬空 []
contracts/ATOMIC_PUBLISH.md      孤儿 [] 悬空 []
contracts/OWNERSHIP_LIFETIME.md  孤儿 [] 悬空 []
data/ARTIFACTS.md                孤儿 [] 悬空 []
data/ARTIFACT_STORE.md           孤儿 [] 悬空 []
data/PHASE_PRODUCT_EXCHANGE.md   孤儿 [] 悬空 []
data/PROVENANCE.md               孤儿 [] 悬空 []
data/README.md                   无参考文献节

### 6. 死路径（修正派单脚本的正则缺陷后）
$ python3 - <<'PY'   # EXT 加后缀边界 (?![A-Za-z0-9_])
docs/engineering/contracts/MANIFEST_VERIFY.md:22  eng/tests/backend/test_cpu_profile.py
docs/engineering/data/ARTIFACTS.md:46             eng/tests/contracts/test_unified_object_contract.py
docs/engineering/data/ARTIFACTS.md:72             eng/tests/unit/core_pipeline_test.cpp
docs/engineering/data/PROVENANCE.md:108           eng/tests/artifact/test_production_store.py
死路径残留 = 4
→ 这 4 处全部位于「**判据无载体**」声明句内部（刻意点名不存在的文件以登记缺口），
  我逐句读过上下文，不再是「写成已生效」的引用。
```

**对派单 7-1 脚本本身的一处订正（保留审稿原判）**：派单脚本的 `EXT` 里 `c|cpp` 顺序导致 `\.c` 先匹配，把
`lib/infrastructure/cli/commands.cpp` 截成 `commands.c`，在 CLI_PROTOCOL / MANIFEST_VERIFY / OWNERSHIP 上
**报出 12 条假阳性**。加 `(?![A-Za-z0-9_])` 后缀边界后假阳性全消，真死路径从「16 条」收敛到「4 条（且都是刻意的缺口登记）」。
**审稿 7-1 的定性（判据载体失联）不变，但其逐条清单不可直接照抄。**

---

## 3. 我推翻 / 修正的审稿判定（审稿原判保留在案）

**① 5-4 的计数与行号**：审稿写「12 处」并列了 13 个行号（含 `:182`）。我复核：空锚是 **11 行 / 12 处出现**（`grep -c` 按行得 11，`grep -o | wc -l` 按出现得 12）；`:182` 行**没有**空锚（那行写的是「形态的输入配置与输出清单字段」，名字本来就对）。**「12 处」总数成立**；逐行清单有一处偏移、`:182` 属误列。

**② 7-1 的逐条清单**：12/16 条是正则截断假阳性（见 §2 末）。定性保留，清单修正。

**③ 5-10 的位置描述**：审稿称 `ARTIFACTS.md:41-60`、`:30` 与 `PHASE_PRODUCT_EXCHANGE.md` 引 `docs/detail/common/UNIFIED_MODEL`。我复核：该字符串在我白名单内**本就 0 处**（ARTIFACTS 当时写的是裸 `UNIFIED_MODEL`）；`grep -rn "docs/detail/common" docs/engineering/` 只命中 `UNIFIED_OBJECTS.md:4` 与 `:46`。**该串的修复义务全部落在 UNIFIED_OBJECTS.md（他人车道）**。

**④ 5-12「同一节的两种叫法」——部分推翻，升级为「两个都是误挂」**：
审稿说两篇对**同一节**给了两个名字。复核发现更严重：**两个名字在 `docs/detail/UNIFIED_MODEL.md` 里都不存在**。
- 「13 个对象 → canonical schema → schema ID」是 `UNIFIED_OBJECTS.md` **自己**的 `## 2.` 标题，被它自己倒挂成 UNIFIED_MODEL 的节；
- 「weight/value/scale/sigma/snr 歧义映射」是 `ARTIFACTS.md` **自己**的 `## ` 标题，被它自己倒挂成 UNIFIED_MODEL 的节；
- UNIFIED_MODEL.md 的真实节只有 `## 1. 统一线性观测模型` / `## 2. 数据对象（各自具名）` / `## 3. 三类配置严格分离`，其中 `:56` 逐字写「**canonical 数据对象 = 13 个对象**（signal / variance / ivar / source_snr / depth_m5 / frame_snr / point_information / sparse_snr_layer / support / coverage / validity / rejection / provenance）；表中其余行是配置键或辅助面」——正是两份文档都要引的那一节。

**我统一到的名字**：`docs/detail/UNIFIED_MODEL.md`「数据对象（各自具名）」一节。
**UNIFIED_OBJECTS.md 该怎么改**（不在我白名单，请改它的人照办）：
- `:1` 标题、`:4`、`:9`、`:19`、`:30` 表头、`:46`、`:50` 中全部 `UNIFIED_MODEL 「13 个对象 → canonical schema → schema ID」一节` → `docs/detail/UNIFIED_MODEL.md`「数据对象（各自具名）」一节`；
- `:4`/`:46` 的 `docs/detail/common/UNIFIED_MODEL` → `docs/detail/UNIFIED_MODEL.md`；
- `:4`/`:46` 的 `「统一对象」一节` 同样不存在 → 一并改为「数据对象（各自具名）」一节。

**⑤ 2-11 / 2-12 / 2-14 / 2-15 / 8-7 五条全部成立，无一推翻**（依据见 §1 复核列，均为我亲自读源码）。

---

## 4. 需代码 / schema 侧订正（我只登记，不改代码）

| # | 事项 | 证据 |
|---|---|---|
| **①** | `phase_product_exchange.schema.json` 的 `plane_id` enum 与 validator `_PLANE_ID_SET` **仍含 `mask`**，文档侧已按 2-14 删除 → **schema 需修订**（两侧同改） | `schema.json:162` `"enum": ["signal","support","variance","ivar","mask"]`；`phase_product_exchange_validator.py:57` `_PLANE_ID_SET = {"signal","support","variance","ivar","mask"}` |
| **②** | 交换 `plane_id` 枚举**装不下**两个真实存在的磁盘面：HiPS 的 `snr/` 子产品、平面 FITS 的 `COVERAGE` 扩展 HDU → 枚举覆盖缺口，是否扩枚举属合同域裁决 | `aio_hips_writer.cpp:1285` `{AIO_HIPS_PRODUCT_SNR,"snr"}`；`p3_output.cpp:389/910` `EXTNAME=COVERAGE` |
| **③** | validator 只接受 `coordinate.frame=="icrs"`，生产者写 `hips_frame=="equatorial"` → 若合同意图是「从 `hips_frame` 逐字自动填 `coordinate.frame`」，validator 必须加 `equatorial` 别名，否则自动填必被拒。**我按契约语义在文档里显式写死映射表，未改 validator** | validator `:208`；writer `:1902/2082` |
| **④** | `TRACEABILITY.md` **没有「合同 ID 登记面」这一节**（ATOMIC/OWNERSHIP 引的就是它） | `grep -n "合同 ID 登记面" TRACEABILITY.md` → 0；真实节为 `## ID 格式（机器正则）`、`## 需求→实现→测试 登记册（人读正本）` |
| **⑤** | `DATA-002` 这个合同 ID 在 `DOCUMENT_INDEX.yaml` 与 TRACEABILITY 里**都没有登记**，只见于 `docs/science/algorithms/PHASE3_RSMP_IMPL.md` → **该 ID 对应哪个文件仍需权威确认**（我按内容相似性推断是 `PHASE_PRODUCT_EXCHANGE.md`，**这是推断不是核对**） | `grep -rn "DATA-002" docs/DOCUMENT_INDEX.yaml TRACEABILITY.md` → 0 |
| **⑥** | `UNIFIED_OBJECTS.md` 的 6 处 `docs/detail/common/UNIFIED_MODEL` + 7 处误挂节名（5-10/5-12），需由改它的人按 §3-④ 处置 | `grep -rn "docs/detail/common" docs/engineering/` → `UNIFIED_OBJECTS.md:4`、`:46` |
| **⑦** | `LOG_AND_ERROR.md` 拆分后，5-3 的落点已消失、2-10 的 `INVALID_INPUT` vs `INPUT_CORRUPT` 迁到新文件 `docs/engineering/standards/ERROR_MODEL.md` → **请前台向该代理确认这两条在其批次内已闭环** | `wc -l LOG_AND_ERROR.md` → 199；`grep -n "机器判据" LOG_AND_ERROR.md` → 0 |
| **⑧** | 白名单内点名的判据载体（`test_cpu_profile.py`、`test_unified_object_contract.py`、`core_pipeline_test.cpp`、`test_provenance.py`、`test_production_store.py`、`test_phase_product_exchange.py`、`test_hips_output_contract.py`、`test_cli_protocol.py`、`test_cli001_vpi.py`、`test_phase123_pipeline.py`、`test_hips_atomic_publish.cpp`、9 个 `Test*` 类、`ACSD_TEST_CORRUPT_AFTER_RENAME` 注入器）**全部无实现** → 门当前不可复跑；文档侧已全部改成「判据无载体 / 判据未接线」，**载体落库是工程侧任务** | `ls eng/tests` → 只有 `conformance/`（内含且仅含 `noop/`）与 `validation/`；`grep -rn ACSD_TEST_CORRUPT_AFTER_RENAME lib/ eng/ docs/` → 只命中 `ATOMIC_PUBLISH.md:167` 自身 |

---

## 5. 需权威补充（我核对不到一手来源，如实留白）

| # | 事项 | 我核到什么程度 |
|---|---|---|
| ① | **SCI-P3-001 的 `a-1`/`a-8`/`a-9` 子条款编号**在 `docs/science/PHASE3_HIPS_TO_FITS.md` 里不存在（该文件是 `## 1`–`## 16` 编号章） | 我已把 ARTIFACTS.md 换成可核验的真实章名「输入有效域」「连续定义」。若别处仍按 `a-1` 引，**需 science 侧确认这一编号体系是否另有所指** |
| ② | `DATA_SEMANTICS` 的「机器校验」「交换对象」「兼容矩阵」「阶段产品角色与 type 绑定」「跨 Phase 仅磁盘交换」**全部不存在** | `grep -n "^#" DATA_SEMANTICS.md` → 只有 `## 1`–`## 7` / `### 3.1`–`### 5.3`。ARTIFACTS/PHASE_PRODUCT_EXCHANGE 已换成 `「坐标语义」一节`（`:42` 逐字含 `FITS index = (511 - x) * 512 + y`）、`「方差与逆方差的三态编码」一节`（`:59`）等真实节名；**其余文档若仍按旧节名引需各自改** |
| ③ | `TRACEABILITY.md` 的「模块与源码追溯矩阵」节名 | 实际为 `## 逐模块追溯台账（人读正本）`（`:178`）；ARTIFACTS 已改 |
| ④ | 「强制剔除计数合同判据（六条 G1–G6）」的逐条原文 | 全仓只找到 `ALG-P3-003 §2 G4` 一处零散引用（`docs/science/algorithms/PHASE3_RSMP_IMPL.md:348`），**G1–G6 完整清单核对不到**；我保留引用但未复述内容 |

---

## 6. 我否决 / 未采纳的

1. **未覆写 `T06-第1轮订正-subA.md`**（另一代理的交付件）。本件改名为 `subA-2.md`。
2. **未回滚任何并发代理的内容**（AGENTS §2）。
3. **未改任何非白名单文件**（`LOG_AND_ERROR.md`/`CONFIG.md`/`PIPELINE_BLOCK.md`/`SCHEDULER.md` 我一次都没编辑；它们的 diff 全部来自并发代理）。
4. **未照抄审稿 7-1 的逐条清单**（含 12 条正则假阳性），理由见 §2。
5. **未对文件内自指锚点做全量降级**：AGENTS §5 禁的是跨文档「见第几节」式锚；我车道内 5-7 的覆盖范围因此只到跨文档假节名与编号引用改造。

---

## 7. 自证段

**我实际跑过并据此下结论的命令**（均可复跑）：
```bash
cd "/workspace/Astro CS Database"
grep -n "^#" docs/ACSD_DESIGN.md                     # 真实章名清单
grep -n "^#" docs/detail/UNIFIED_MODEL.md            # 13 对象节的真实节名
grep -n "^#" docs/science/unified/DATA_SEMANTICS.md  # 真实节名
grep -n "^#" docs/engineering/resources/observability/STRUCTURED_LOGGING.md
grep -n "^#" docs/engineering/governance/TRACEABILITY.md
grep -n "^#" docs/engineering/contracts/HIPS_STORAGE_FORM.md
grep -rn "ARCH-002\|ARCH-004\|ARCH-005" docs/ eng/ lib/ ; grep -n "ARCH" docs/DOCUMENT_INDEX.yaml
grep -c "「」" HIPS_STORAGE_FORM.md
grep -n "最高设计 ：\|（最高设计 ，\|；：\|（/ ）\|该文件 （" PHASE_PRODUCT_EXCHANGE.md
sed -n '165,200p' lib/algorithms/coverage/include/astro/phase2/upm.h
sed -n '1,25p;585,600p;685,712p;2024,2060p' lib/algorithms/coverage/src/upm.cpp
sed -n '1,30p' lib/algorithms/coverage/include/astro/phase2/integrate.h
sed -n '95,130p' lib/infrastructure/aio/io/hips_output_store.py
ls eng/contracts/schemas/unified/ ; sed -n '35,52p' lib/infrastructure/aio/include/aio_hips.h
grep -n "EXTNAME" lib/algorithms/fits_output/p3_output.cpp
grep -n "hips_frame" lib/infrastructure/aio/src/hips/aio_hips_writer.cpp
grep -n "AIO_HIPS_PRODUCT_SIGNAL" lib/infrastructure/scheduler/src/module_adapters.cpp
grep -n "plane_id\|_PLANE_ID_SET\|\"frame\"" eng/contracts/data/phase_product_exchange.schema.json \
      lib/infrastructure/aio/runtime/artifact_store/phase_product_exchange_validator.py
ls eng/tests ; find eng/tests -type f | sort ; ls -d lib/infrastructure/aio/tests
grep -rn "docs/detail/common" docs/engineering/ ; grep -rn "DATA-002" docs/
git -c core.quotepath=false status --porcelain ; git -c core.quotepath=false diff --stat …
python3（死路径扫描 + 参考文献孤儿扫描，各写一版并在 §2 贴出）
```

**我没有核、如实留白的结论**：

- 我**没有编译、没有运行任何测试、没有跑任何端到端**。对生产代码的全部结论都是**读源码**（函数体与头注）得出的，不是运行读数；运行行为层面的偏差我无法排除。
- **§4-⑧ 列的 13 个测试文件 / 9 个 `Test*` 类，我只证明了「文件名 grep 不到」**，没有逐个核对它们是否以别的名字存在。
- **§5-① 的 `a-1/a-8/a-9` 编号体系**：我只能证明它在 `PHASE3_HIPS_TO_FITS.md` 里不存在，不能排除它在 science 别的分册里有定义。
- **§5-④ 的 G1–G6**：只找到 `G4` 一处零散引用，其余五条核对不到。
- **§4-⑤ `DATA-002 → 文件`映射是推断不是核对**（依据是 `invalid_handling` 规则块位置与规则 1/2/3 编号与 `PHASE3_RSMP_IMPL.md` 的引用对得上）。
- **`HIPS_STORAGE_FORM.md` 的「实验域的压缩编码评估证据面」「实验/engineering-evidence/」我没有核**（不在派单死路径正则的字符集内），保持原样。
- 我**没有复核**并发代理在 LOG_AND_ERROR / CONFIG / SCHEDULER / PIPELINE_BLOCK 上的改动质量，只确认了那四个文件不是我动的。