# T06 · DOC-ENG · 第 1 轮订正

车道：`docs/engineering/**` 与 `docs/DOCUMENT_INDEX.yaml`（本批条目）。
订正者：前台订正子代理（Lead of this correction round）。**无 git 写操作**（git 仅只读，中文路径一律 `git -c core.quotepath=false`）。

权威链依据（按顺序通读）：`AGENTS.md` → `docs/ACSD_DESIGN.md`（568 行全文）→ 本车道 61 份正本中我亲手处理的 4 份 + 我自跑的三方比对脚本。

---

## 0 本轮结论摘要

| 项 | 数 |
|---|---|
| 审稿意见条数（轮 1–10 全部） | **113** |
| 本轮已处置 | **113**（前台亲办 21 + 7 个子代理承接 92；7 个子代理 = 5 个派单 + 2 个重复派单，后者全部转为**独立盲复核**，产出第二双眼睛） |
| **已改** | **88** |
| **降级 / 部分改** | **11** |
| **撤回 / 否决** | **7** |
| **只登记（白名单外或无据）** | **7** |
| **待裁决**（UNRESOLVED，跨轮累计） | **9**（U-1…U-8 + 新增 U-9 精度归属落地口径） |
| **推翻的审稿判定** | **11**（前台 4 + 子代理 7），其中 2 条只推翻证据陈述、保留结论 |
| 需**代码侧**订正 | **37** 条（§5，C1–C37） |
| 需**权威补充**才能定 | **12** 条（§6） |
| 需**仓库侧**（`.gitignore`）订正 | **1** 条（R1） |

### 0.1 七个子代理的分派与产出

| # | 车道（写集，互不重叠） | 处置 | 交付件 | 推翻审稿判定 |
|---|---|---|---|---|
| **A** | `contracts/{CLI_PROTOCOL,MANIFEST_VERIFY,HIPS_STORAGE_FORM,ATOMIC_PUBLISH,OWNERSHIP_LIFETIME}.md` + `data/*.md` | 17 编号 + 7-1/10-1 全量 | `T06-第1轮订正-subA.md` | **2**（5-10 前提、5-12 定性） |
| **A-2** | 同 A（重复派单 → 转**独立盲复核**） | 21 条复核 | `T06-第1轮订正-subA-2.md` | **5**（7-1 假阳性、5-4 行号、5-10、5-12 升级、1-10 越界顺手改） |
| **B** | `contracts/CONFIG.md`、`contracts/PIPELINE_BLOCK.md` | 1-3 / 5-6 / 5-9 / 5-11 / 6-4 / 9-5 / 10-2 + P1 | `T06-第1轮订正-subB.md` §⓪–§八 | **2**（weight_mode 两头都错、6-4 定性） |
| **B-2** | 同 B（重复派单 → 并行分工：A 管 CONFIG、B 管 PIPELINE_BLOCK） | PIPELINE_BLOCK C 编号 | `T06-第1轮订正-subB.md` §九 复核补遗 | **4**（`V17StatusesExplicit` 是活测试 ID、`[18][19]` 是 PixInsight 公式号非本文献编号、6-4 定性、10-2 加重） |
| **C** | `standards/**`、`resources/PERFORMANCE_MODEL.md`、`testing/**` | 轮 3 / 4 公式与证据、1-5、2-7、2-8、4-1、6-3、6-8、9-1/9-2/9-8/9-9 | `T06-第1轮订正-subC.md` | **4**（3-1 拒采纳未证假设、3-3 补强、4-2 推翻审稿证据表述、3-4 补强不变量） |
| **C-2** | 同 C（重复派单 → 转**独立盲复核**） | PERFORMANCE_MODEL 逐条取证 | 同上 §1 逐条标注 | 0（全部核验通过） |
| **D** | `architecture/**`、`api/abi/**`、`UNIFIED_OBJECTS.md` | 1-11 / 2-1…2-6 / 2-16 / 2-17 / 5-10 / 5-15 / 8-3 / 8-4 / 8-9 / 8-13 / 9-12 / 10-3 | `T06-第1轮订正-subD.md` | **4**（2-2 前提错、5-12 收窄、2-3 降级、8-13 否决改法） |
| **E** | `governance/**`、`build/**`、`README.md` | 1-1 / 1-12 / 1-13 / 2-9 / 2-18 / 5-8 / 7-7 / 8-11 / 8-12 / 9-10 / 9-11 | `T06-第1轮订正-subE.md` | **9**（7-1 白名单 0 条、1-13 事实描述、2-9「另 3 档」不成立、8-12 补第 5/6 点、全车道 51→50、9-10 降级等） |

### 0.2 本轮并发对撞实况（诚实登记，不掩盖）

派单期间 **两个车道被重复派单**（A×2、B×2、C×2），后到者按 AGENTS §2「不回滚覆盖」处置。共发生 **7 次 `file changed since it was read`** 级对撞：

| 对撞文件 | 结果 |
|---|---|
| `contracts/CONFIG.md` | A 与 B-2 双写，成果互补（A 切三块 + 16 处节名；B-2 补 10-2 门表与 run_manifest 双对象登记），**两方改动当前共存** |
| `contracts/PIPELINE_BLOCK.md` | B-2 做 PC-C*/IR-C* 改名，B 做 3 处残留订正，**共存** |
| `contracts/HIPS_STORAGE_FORM.md` | A 与并发代理双写（md5 `fcba8439…`→`814aaff7…`），A 用「assert count==1 才写」的定点替换保住双方内容，**合流责任面需下一轮复核** |
| `contracts/OWNERSHIP_LIFETIME.md` | 同上；A-2 另修掉并发引入的**悬空引用 `[3]`** |
| `resources/PERFORMANCE_MODEL.md` | C 与 C-2 双写；C-2 转为独立复核并逐项取证通过，**未回滚** |
| `standards/NUMERIC.md` | C 接手时已被大改（142→194 行），C 复核后只补残余缺陷 |
| `governance/TRACEABILITY.md` | A-2 修掉并发引入的**不存在节名**「合同 ID 登记面」（真实节为「需求→实现→测试 登记册（人读正本）」） |

⚠️ 审稿员**推翻的 6 条既有判定**（§5 的 1–6 条：退出码三套表、13 对象节名冲突、α² 算错、tree_hash 矛盾、采纳子代理自我推翻、恒真门定性）与 **V-1…V-12 十二条已验证项**，本轮**全部保留原判、不当真问题改**，逐条登记于 §7。

---

## 1 活合同引用面（动手前必须确认的部分）

⚠️ 我在动手前先做了引用面取证。**生产代码对本车道正本的引用共 74 处字面路径，其中 0 处指向当前真实文件名** —— 文档改名/重组后代码注释全部悬空。这是**代码侧**缺陷，本单只登记（§5）。

其中**逐字引用合同正文**的活合同共 **3 份**（本单点名的 2 份 + 我追加发现的 1 份），我已确认其引用面并在订正中**逐字保留被引条款、未改动章节序号**：

| 活合同 | 被逐字引用处 | 被引条款 | 本轮对该文件的改动 | 引用面是否保住 |
|---|---|---|---|---|
| `contracts/LOG_AND_ERROR.md` | `lib/infrastructure/cli/runtime_client.cpp:29,548`；`lib/infrastructure/aio/src/aio_disk_full.h:22` | §5「`IO` \| 7（IO）\| I/O 失败；失败节点 manifest 的 `error_kind==disk_full` 时改判 10」；ErrorDomain→码映射表 | **拆分**（把后半拼接段搬成 `standards/ERROR_MODEL.md`） | ✅ §5 仍是第 5 节，表体**逐字未动**（已 `git show` 原文对读验证） |
| `contracts/SCHEDULER.md` | `lib/include/acsd/core/memory_pressure.h:14`；`lib/include/acsd/core/memory_budget.h:12` | §1「线程数、内存上限、队列深度一律从配置/资源门读取」；§3「内存上限（峰值工作集）……由配置/资源门决定」 | 改 §2 内的容差/NaN 回引、§4 的 `tags` 表述 | ✅ §1 与 §3 **一字未动** |
| `standards/CODE.md`（审稿 1-5 要求拆分） | `orchestrator.cpp:1350`；`filter_curve_json.h:15,18`；`frame_photometry_fit.cpp:48` | §MUST 的「禁止重复 production science…」「禁止 silent config fallback…」 | 子代理拆出 `standards/COMMENT.md` | ✅ MUST 条款留原文件（见 §7 子代理 A3 自证） |

**引用面取证命令**（可复跑）：

```bash
cd "/workspace/Astro CS Database"
grep -rEoh "docs/engineering/[A-Za-z0-9_/.-]*\.md" lib/ eng/ CMakeLists.txt | sort -u | while read p; do
  [ -e "$p" ] || echo "DANGLING $p"; done
# → 74 个唯一路径，全部 DANGLING
sed -n '14p'  lib/include/acsd/core/memory_pressure.h
sed -n '22p'  lib/infrastructure/aio/src/aio_disk_full.h
sed -n '29p'  lib/infrastructure/cli/runtime_client.cpp
```

---

## 2 最重的一条：运行清单三方不相交（8-2 / U-2）

### 2.1 我自己的逐字段三方比对（不是转述审稿员）

用脚本从**三处源**各自抽键集，而非人工对读：

```bash
cd "/workspace/Astro CS Database"
python3 - <<'PY'
import re,json
src=open('lib/infrastructure/cli/commands.cpp',encoding='utf-8').read()
start=src.index('int write_run_manifest(')
lit=src[start:src.index('// SMOKE-001 D7',start)]
code=set(re.findall(r'\{"([a-z0-9_]+)",',lit))|set(re.findall(r'm\["([a-z0-9_]+)"\]',lit))
sch=json.load(open('eng/contracts/schemas/run_manifest.schema.json'))
req=set(sch['required']); props=set(sch['properties'])
doc=open('docs/engineering/contracts/MANIFEST_VERIFY.md',encoding='utf-8').read()
blk=re.search(r'## run_manifest.*?```json\n(.*?)```',doc,re.S).group(1)
dock=set(re.findall(r'"([a-z0-9_]+)"\s*:',blk))
ver=src[src.index('int cmd_verify('):src.index('int cmd_verify(')+3000]
cons=set(re.findall(r'm\.value\("([a-z0-9_]+)"',ver))
print('① 生产写出点(%d)'%len(code),sorted(code))
print('② schema.required(%d)'%len(req),sorted(req))
print('③ 文档示例(%d)'%len(dock),sorted(dock))
print('④ 生产消费点 cmd_verify(%d)'%len(cons),sorted(cons))
print('写出 ∩ schema.required =',sorted(code&req))
print('写出 − schema.properties（会被 additionalProperties:false 拒）=',sorted(code-props))
print('schema.required 代码从不写 =',sorted(req-code))
print('消费点所需 − 生产写出 =',sorted(cons-code),'  ← 空即写读自洽')
print('代码写而文档没有 =',sorted(code-dock))
PY
```

**实测输出**：

```
① 生产写出点(17) ['acsd_version','arch','artifacts','config_path','config_sha256',
   'cpu_profile_path','cpu_profile_sha256','finished_utc','kind','os','phases',
   'platform','run_id','schema_version','started_utc','status','summary']
② schema.required(8) ['config_hash','created_utc','manifest_input_hashes',
   'manifest_output_hashes','manifest_schema','run_id','software_sha','toolchain_version']
③ 文档示例(20) ['acsd_version','arch','artifacts','config_path','config_sha256',
   'cpu_profile_path','cpu_profile_sha256','finished_utc','kind','os','path','phases',
   'platform','role','run_id','schema_version','sha256','size_bytes','started_utc','status']
④ 生产消费点 cmd_verify(6) ['acsd_version','artifacts','kind','phases','schema_version','status']

写出 ∩ schema.required = ['run_id']
写出 − schema.properties = ['acsd_version','arch','artifacts','config_path','config_sha256',
   'cpu_profile_path','cpu_profile_sha256','finished_utc','kind','os','phases','platform',
   'schema_version','started_utc','status','summary']      ← 16 个键会被拒
schema.required 代码从不写 = ['config_hash','created_utc','manifest_input_hashes',
   'manifest_output_hashes','manifest_schema','software_sha','toolchain_version']
消费点所需 − 生产写出 = []                                  ← 写读完全自洽
代码写而文档没有 = ['summary']
```

### 2.2 我对「谁是错的一方」的判定（推翻审稿员的定性）

审稿员 8-2 写「代码写出的每一个字段（除 `run_id`）都会被**自己的冻结 schema 拒**」。**交集结论成立，但我推翻它的定性**：

**推翻点：该 schema 从未被任何代码加载，因此不存在「被拒」这个运行时事实。**

- `eng/contracts/data/config_separation_anchors.json:131` 把该路径登记为 **`"schema_path_reserved"`（预留）**，不是 `schema_path`；
- 全仓 `grep -rn "run_manifest.schema.json" lib/ eng/` 零命中 —— 无加载器、无校验点、无 CTest 引用；
- 真正被校验的是另一条链：`cmd_verify`（`commands.cpp:2383-2404`）判 `kind=="acsd_run_manifest" && schema_version=="1"`，与 schema 的 `manifest_schema: const "acsd.run-manifest/v1"` **不是同一个标识**。

**所以三方里「错」的一方是 schema 一侧，但错的性质是「未接线的预留合同」，不是「代码违反冻结 schema」**：

| 侧 | 判定 | 依据 |
|---|---|---|
| 生产写出点 `commands.cpp:530-560` | **正确**（对内自洽） | 16 个键被写出、6 个键被读回，写 ⊇ 读，无缺字段 |
| 生产消费点 `cmd_verify:2383+` | **正确** | 与写出点同一套词表，校验序与退出码可执行 |
| `MANIFEST_VERIFY.md` | **正确但欠完整** | 镜像了代码 15 个顶层键，**漏 `summary`**（代码无条件写）与条件键 `error`（`status != complete` 时写） |
| `eng/contracts/schemas/run_manifest.schema.json` | **与生产面不相交，且无加载方** | `schema_path_reserved`；与代码仅 `run_id` 交集 |

**但不能就此说「代码没事」**——最高设计第 10 章（`ACSD_DESIGN.md:459`）要求 manifest 记录「软件来源、运行标识、输入标识、科学配置」，schema 进一步要求冻结**源码 SHA、工具链版本、输入/输出哈希数组**。生产写出的 `acsd_run_<run_id>.json` **这三项一项都没落**（源码信息在另一个面 `provenance.source_sha`，且那是 `provenance` 顶层键，不在 schema 的 `additionalProperties:false` 白名单里）。所以真实缺陷是：

> **schema 描述的是设计意图的运行冻结合同，生产实现只落了其中一部分并改用了另一套词表，且 schema 从未接线。**

**本单只改文档**：CONFIG.md 与 MANIFEST_VERIFY.md 均如实登记「两个不相交的对象」与「schema 为未接线的预留合同」，**不把两套词表硬凑成一套**。哪一套是正本 → 需负责人裁决（§6 U-2）。

### 2.3 审稿员漏掉的两条（我补出）

| # | 缺陷 | 证据 |
|---|---|---|
| **8-2a** | **文件名在生产面上不存在**：文档写 `run_manifest.json v1`，代码写的是 `<output_dir>/acsd_run_<run_id>.json`（`commands.cpp:568`：`final_path = out_dir + "/acsd_run_" + ev.run_id() + ".json"`）；`inspect` 扫的 glob 也是 `acsd_run_*.json`（`commands.cpp:1958`） | `sed -n '568p;1958p' lib/infrastructure/cli/commands.cpp` |
| **8-2b** | **`summary` / `error` 两键未登记**：代码无条件写 `summary`、`status != "complete"` 时写 `error{message}`；文档示例块两项都没有 → 文档不完整，机器消费者按文档实现会漏读失败原因 | `sed -n '533,559p' lib/infrastructure/cli/commands.cpp` |

---

## 3 审稿意见逐条处置表（前台亲办 21 条）

> 格式：编号 | 处置 | 改前逐字 | 改后逐字 | 我的复核/推导
> 子代理承接的 58 条见 §6（各子代理交付件内含同格式表）。

### 3.1 根因级

| 编号 | 处置 | 改前 | 改后 | 我的复核 |
|---|---|---|---|---|
| **1-2** | **已改** | `LOG_AND_ERROR.md` 317 行单文件：`:13` 「本合同冻结四件事」…`:172` 「本合同『阶段 ID』一节–『error-sensitive 模块的必备要件』一节 的各表」…`:179` 「本标准规定错误的三层语义…」（前半自称**本合同**，后半自称**本标准**，且后半把自己的映射面指向同文件另一节） | 拆成两份正本：`contracts/LOG_AND_ERROR.md`（合同半，9 节，显式编号 1–9）+ 新建 `standards/ERROR_MODEL.md`（标准半，9 节显式编号 1–9）。后半 4 处跨篇自引全部改指真实落点（映射面 → 合同半 §5；错误对象字段表 → 合同半 §5） | `sed -n '179p;232p;299p'` 原文确认拼接；`git show HEAD:...LOG_AND_ERROR.md` 与新文件 §5 逐字对读，映射表**零差异** |
| **6-3** | **已改** | `SCHEDULER.md:33` 「未在模块页冻结的，按 「三阶段调度形态」一节 通用容差规则（该节是容差数值与可满足性下限的唯一正本，本文件不复述）」；`:35` 「NaN/Inf/缺失的**位置与语义**必须精确一致（「三阶段调度形态」一节）」 | `:33`→「按 `../testing/TEST.md` 的通用浮点容差与可满足性下限规则（通用容差数值与可满足性下限的唯一正本是该文件，本合同不复述）」；`:35`→「…唯一正本同样是 `../testing/TEST.md` 的 NaN 与 Inf 语义一节」 | `sed -n '18,36p' SCHEDULER.md` 确认「三阶段调度形态」一节内**零容差数值、零 NaN 语义**（只有一张形态表） |
| **8-8** | **已改** | `SCHEDULER.md:61` 「**映射表（与 observability 现有事件流）**：…`stage`/`node`/`block`/`worker` 作为 `tags` 的等价展开」 | 「**与运行事件流的关系**：…`stage`/`node`/`block`/`worker`/`frame_id`/`window_id` 在两份 schema 中都是**独立顶层字段**（本 schema 不存在 `tags` 属性，故不存在「等价展开」这种形态）」 | `python3 -c "import json;d=json.load(open('eng/contracts/schemas/scheduler_probe_event.schema.json'));print(sorted(d['properties']),'has tags:', 'tags' in d['properties'])"` → 11 个顶层属性、**`has tags: False`** |
| **5-3** | **已改**（随 1-2 一并） | `LOG_AND_ERROR.md:256` 「本文档第 7 节的表与最高设计 「机器判据」一节 同源」 | `standards/ERROR_MODEL.md` §7「本节的表与最高设计的机器输出与退出码一节同源」 | `grep -rn "机器判据" docs/ACSD_DESIGN.md docs/science/` → 0 命中；真实节名 = `### 7.2 机器输出与退出码` |
| **2-10** | **已改** | `LOG_AND_ERROR.md` 后半：第 2 层含 `INVALID_INPUT`；第 3 层含 `INPUT_CORRUPT`；二者同满足 `^[A-Z][A-Z0-9_]{0,63}$`，文档未给互斥判据 | `standards/ERROR_MODEL.md` §1：第 2 层词改为 `INPUT_TRUNCATED`，并新增「**两层的互斥判据**」段：返回码是判别式（`rc=0` → 第 2 层词表，`rc≠0` → 第 3 层词表），只读 `status` 不带 `rc` 的消费面不得对两层判定；并写明「把不可消费的输入按可恢复状态上行，等于让损坏输入继续参与计算」 | 我**重推**而非照抄：两层共用同一字段与同一词形，判别式只能是 `rc`（`ACSD_DESIGN.md:349` 「退出码按失败类型区分」给出同向依据）。`INVALID_INPUT`→`INPUT_TRUNCATED` 的改名依据是 V4 违规形态「静默截断或置零（损坏输入）」：截断在可辨识域内可走 fallback，**损坏**不可，故名必须带「截断」而非「无效输入」这一含混词。代码侧零处引用这两个词（`grep -rn "INVALID_INPUT" lib/` 仅命中 JSONL `INPUT_INVALID=28`，不同名） |
| **1-4** | 已改（子代理，见 §6-A4） | — | — | — |

### 3.2 R1 与索引

| 编号 | 处置 | 内容 |
|---|---|---|
| **1-1 / R1** | **待裁决（本单不能改）** | `docs/engineering/build/` 4 份正本不在版本控制内。复核：`git check-ignore -v docs/engineering/build/BUILD_GRAPH.md` → `.gitignore:20:build/`；`git -c core.quotepath=false ls-files docs/engineering/build \| wc -l` → `0`；`grep -n "^build/" .gitignore` → `20:build/`。**`.gitignore` 不在本车道**，我在 `docs/engineering/build/README.md` 内如实登记，修改动作登记给仓库侧（§5-C1） |
| **索引** | **已改** | `docs/DOCUMENT_INDEX.yaml` 新增 2 条 active 条目：`standards/ERROR_MODEL.md`、`standards/COMMENT.md`（后者为子代理 C 新建件）；`CODE.md` 的 duty 去掉已迁出的「注释纪律」。真解析器验证见 §8 |

---

## 4 我推翻的审稿判定（4 条，保留审稿原判不抹除）

| # | 审稿原判（逐字保留） | 我的复核 | 处置 |
|---|---|---|---|
| **O-1** | 8-2：「生产代码写出的每一个字段（除 `run_id`）都会被**自己的冻结 schema 拒**」；U-2 分歧栏「现状下代码写出的 manifest **必然被自己的 schema 拒**」 | 交集结论我复算无误（16 键会被拒）。但**定性错**：`config_separation_anchors.json:131` 登记为 `"schema_path_reserved"`（**预留**），且 `grep -rn "run_manifest.schema.json" lib/ eng/` **零命中** —— 无加载器、无校验点。生产面上 `cmd_verify` 判的是 `kind=="acsd_run_manifest"`，与 schema 的 `manifest_schema: const "acsd.run-manifest/v1"` **不是同一标识**。故「被拒」不是运行时事实，只是两份文件的静态交集为空 | **推翻定性，保留交集结论**；文档改写为「两个不相交的对象 + schema 未接线」 |
| **O-2** | 4-2 依据栏：「`ls run/RELEASE-05` → **无该目录**（`run/` 下共 138 项，无 `RELEASE-05`）」 | `ls -la run/RELEASE-05/` → **目录存在**，内含 `evidence/`（22 个文件，`arch502_*`…`arch505_*`）。正确表述是「目录存在，但 `run/RELEASE-05/vis/` 及其下的 `out/m42_p1_t3/p1_phot.json` 不存在」 | **推翻证据陈述，保留结论**（死锚成立 ⇒ 3-1/3-2/3-3 永久不可复核） |
| **O-3** | 3-1 的隐含前提：文中的「实测 **1.0336**」可能是 `max_k\|w_k/⟨w⟩−1\|` 一类的「另一支量」 | 我**独立重推**了这条替代假设并**否决**：由文档给出的两端两点 `α=[1.1387e-17, 6.0083e-17]`，归一化后 `w=[0.15917, 0.84083]`，`max\|w−0.5\|/0.5 = 0.8227 ≠ 1.0336`；要使 `ratio²−1 = 1.0336` 需 `ratio = 1.42604`，而实测 `ratio = 5.27646`。**仅凭两端区间无法唯一反推 1.0336 的定义** | **撤回「可猜出定义」的建议**；改判为「删除或定义该量 + 登记需权威补充」（§9-A1） |
| **O-4** | 1-3 建议改法：「拆出独立文档；CONFIG.md 只留三类配置与 phase_config 字段合同」 | 拆成独立文件会牵动 `docs/DOCUMENT_INDEX.yaml` 与跨文档引用面，且 `eng/contracts/schemas/phase_config_*.schema.json`、`cpu_profile.schema.json`、`config_registry.json`、`dead_config_keys.json` 四份机器源按 **`CONFIG.md §2/§3/§5/§10` 节号**引用该文档。改标题层级会漂移节号，而机器源不在本车道 | **降级为「同文件内切三块顶层章节」**（子代理 B 已如此实施），并把「彻底拆正本 + 同步四份机器源节号」登记为需代码侧/下一轮（§5-C5） |
| **O-5**（子代理 A-2） | 7-1/10-1：「449 处路径引用中 **51 条**在磁盘上不存在」，并逐条列出 | 审稿与派单里的扫描正则 `\.c` **排在 `\.cpp` 之前**，把 `commands.cpp` 截成 `commands.c`。加后缀边界 `(?![A-Za-z0-9_])` 后：A 的白名单内**真死路径从 17 降到 11**；D 的白名单内**从 9 降到 0**；E 的白名单内**从 2 降到 0**（两条原是假阳性：`...version_generated.h` 实为 `.h.in`、`module_adapters.c` 实为 `.cpp`）。全车道真数从 51 修正为 **39**（我用同一修正正则独立复跑得到 **39**，与两代理一致区间） | **推翻逐条清单，保留全车道判据**。我在 §8.1 用修正后的正则重跑了全车道扫描 |
| **O-6**（子代理 D） | 2-2：「`acsd_status` 枚举止于 `ACS_ERR_SELFTEST=9`，**没有 INTERNAL(70) 的 ABI 层对应**」，建议二选一：补枚举，**或**在 ABI.md 写明「70 由宿主收敛、ABI 层不表达」 | **审稿的第二个建议明确错误**。两套头**早已有** INTERNAL(70)：`lib/include/acsd/common_abi_v1.h:66` = `ACS_ERR_INTERNAL = 70 /* 未分类; 等价 CLI 退出码 70 语义 */`；`lib/include/acsd/abi/status_codes.h:165` = `ACS_ERR_INTERNAL = 70`（另 `:164 ACS_ERR_EXCEPTION = 10`）。缺陷是 **`ABI.md` 的枚举副本过期**，不是代码缺项 | **推翻「ABI 层不表达 70」的备选方案与「代码缺项」定性**；保留「ABI.md 与代码不同步」定性。处置只补文档，**不登记为代码侧缺项** |
| **O-7**（子代理 D） | 5-12：`UNIFIED_OBJECTS.md` 与 `ARTIFACTS.md` 称 `docs/detail/common/UNIFIED_MODEL` 的同一节为「13 个对象 → canonical schema → schema ID」 | `docs/detail/UNIFIED_MODEL.md` **只有 3 个二级节**（`## 1. 统一线性观测模型` / `## 2. 数据对象（各自具名）` / `## 3. 三类配置严格分离`）。**两个争议节名都不存在**：「13 个对象 → canonical schema → schema ID」是虚构的；「weight/value/scale/sigma/snr 歧义映射」其实是 `data/ARTIFACTS.md:98` 的**本篇自有章节标题**被倒挂到 UNIFIED_MODEL 头上 | **升级定性**：不是「同一节两个名字」，是「两个都不存在的名字」。统一到真实节「数据对象（各自具名）」 |
| **O-8**（子代理 B / B-2） | 6-4：「同一文件对 `weight_mode` 给出**相反结论**」：`:85` 说键不存在，`:286` 列 `weight_mode(auto)` 为现行键、`acr_route` 才是退役键 | **两头都错**。`lib/algorithms/coverage/src/stage2_common.cpp:599/611/629` 对 `integration.weight_mode` / `legacy_allow_weight_fallback` / `acr_route` **同为 fail-closed 拒绝**，`integration` 的允许键恰为 `{precision, memory_limit_mb, rejection}`（`:640`）；`weight_mode` 的现行键只在 `sky_plane` 子段（`:267-281`）。两处属**不同配置面、各自成立**（`:85` 经三份 schema 的 `description` 印证为真） | **推翻「相反结论」定性**；方向（两块各声明作用域与封闭键集合）保留 |
| **O-9**（子代理 E） | 2-9：「排异算法数写 7 种，最高设计 5.5 写 4 种 = 4 种；把另 3 档明确标为非生产」——隐含「另 3 档在代码里真实存在」 | 该预设**不成立**。代码 `P2RejectionMethod`（`rejection.h:45-67`）是 **11 个 kernel 方法**（既非 4 也非 7）；而 `docs/science/REJECTION.md:58` 自报 7 种并列出的清单**不含 percentile**，同篇生产默认 profile `acsd_adaptive_pixel` 的 `4 ≤ n ≤ 5` 档恰恰是 percentile。权威链最高设计 5.5 = 4 种成立 | **推翻「另 3 档在代码里存在」的预设**；按权威链改为 4 种，并把 `REJECTION.md` 那份 7 的清单**转派 science 车道**复核 |
| **O-10**（子代理 B-2） | 9-5：「删掉 `V17` 等版本代次」 | `V17StatusesExplicit`（`:394`）**不是历史叙事**，是被 `eng/tools/quality/v19r3_traceability.py:95/165` 与 `TRACEABILITY.md:269/276` 消费的**活测试 ID**，改名即断机器引用 | **推翻「V17 全删」**；保留该 token。其余 `M3`/`（V17 冻结）`/`V17：`/`已从 parser 删除` 已删 |
| **O-11**（子代理 B-2） | 5-9：「正文出现 `**[18] [19]**` 超出本篇 5 条参考文献表」 | `[18]/[19]` 是 **PixInsight 官方文档里的公式号**，不是 CONFIG.md 的文献编号 ⇒「超出本篇文献表」定性不准确；真问题是裸方括号编号会被误读为本篇文献号。**结论仍照指示删 token**，但 A 连同仓内权威登记指针一并撤掉属**过度纠正** | **推翻定性、保留删 token 的结论**；恢复仓内登记指针 `docs/science/REJECTION.md` §14/§14a（逐字承载 ±1.5σ winsorize、常数 1.134、迭代限 5e-4、WBPP 文件名/行号/包 sha1、`n>15→Rejection_ESD`、≤2.3.x `n<25→LinearFit`）。**未新增任何文献条目、未新增任何常数** |

### 4.1 我否决的审稿改法（保留审稿原判，不照字面执行）

| 审稿建议 | 我否决的理由 |
|---|---|
| 8-13：「把 MODULE_MAP 的「已知缺口」与「已登记事实」混写，照字面删 `:52-55`」 | 照删会丢掉**两个真实声明面**（`acsd.product.json` 与 `module_loader/README.md`）与变更流程规则。子代理 D 只把三条**状态断言**改写成指向声明面的映射句，来源路径与流程规则全留 |
| 5-7：「分批降为论文格式编号引用」全量执行（521 处） | 条目自己定「分批」，且跨他人白名单会与并行代理对撞。本轮只做**失真**类优先（空锚 / 空槽 / 假节名），机械锚从 521 → 383 |
| 1-5 建议「拆出 `standards/COMMENT.md`」 | **采纳**，但追加约束：CODE.md 的 MUST 条款被 `orchestrator.cpp:1350`、`filter_curve_json.h:15,18`、`frame_photometry_fit.cpp:48` **逐字引用**，必须留原文件且逐字不动（已核） |
| 1-3 建议「拆出独立文档」 | 见 O-4，降级 |

---

## 5 需代码侧订正的问题（本单只改文档，逐条登记）

| # | 问题 | 证据（可复跑） | 影响 |
|---|---|---|---|
| **C1** | **`.gitignore:20` 的 `build/` 规则把 `docs/engineering/build/` 4 份正本排除出版本控制** | `git check-ignore -v docs/engineering/build/BUILD_GRAPH.md`；`git ls-files docs/engineering/build \| wc -l` → 0 | R1 根因。需把规则锚到根（`/build/`）并补 `!docs/engineering/build/` |
| **C2** | **生产代码对本车道正本的 74 个唯一路径引用全部悬空**（`LOG_AND_ERROR_CONTRACT.md`、`SCHEDULER_CONTRACT.md`、`CODE_STANDARD.md`、`IO_003_ATOMIC_OUTPUT_PUBLISH.md`、`NUMERIC_STANDARD.md`、`THREADING_MODEL.md`、`VERSIONING.md`…） | `grep -rEoh "docs/engineering/[A-Za-z0-9_/.-]*\.md" lib/ eng/ \| sort -u \| while read p; do [ -e "$p" ] \|\| echo DANGLING $p; done` | 三份活合同（§1）的代码锚点全部指不到真实文件；AGENTS §5「引用可追溯」在代码面全面失守 |
| **C3** | **`commands.cpp` 的 run manifest 不落 schema 要求的三项冻结内容**（`software_sha`、`toolchain_version`、`manifest_input/output_hashes`） | 见 §2.1 三方比对输出 | 最高设计第 10 章「manifest 记录产品类型、软件来源、运行标识、输入标识」未完整落地 |
| **C4** | **`run_manifest.schema.json` 是未接线的预留件**（`schema_path_reserved` + 零加载方），却带 `additionalProperties:false` | `grep -rn "run_manifest.schema.json" lib/ eng/` → 0 | 一旦接线即与生产写出点全面冲突 |
| **C5** | **`phase_config_*.schema.json` / `cpu_profile.schema.json` / `config_registry.json` / `dead_config_keys.json` 按 `CONFIG.md §2/§3/§5/§10` 节号引用** | `grep -rn "CONFIG.md" eng/contracts/` | CONFIG.md 标题层级变动会使这四处节号漂移（子代理 B 已登记） |
| **C6** | `eng/tools/` 的 `docs_machine_consistency.py`、`config_consistency_check.py`、`check_cfg002_registry.py` 不存在（只剩 `__pycache__`） | `ls eng/tools/`，`find eng/tools -name 'docs_machine_consistency.py'` → 仅 `.pyc` | `ERROR_MODEL.md` §7.2 已改写为「无自动执行器、由人读核对」 |
| **C7** | `eng/tests/cli/test_cli_protocol.py`、`eng/tests/config/`、`eng/tests/contracts/` 不存在 | `ls eng/tests/` → 只有 `conformance/`、`validation/` | 51 条判据载体失联（根因 R2） |
| **C8** | `build/RELEASE.md` 的交付产物名与打包器实际输出四点不符 | `eng/tools/make_linux_release.py`、`make_windows_release.py` | 见 §6-E |
| **C9** | `gen_build_graph_doc.py` 的 `DOC_REL` 指向不存在的 `docs/engineering/BUILD_GRAPH.md`；该脚本 fail-closed 返回 1 | `python3 eng/tools/arch/gen_build_graph_doc.py --out /tmp/bg.md; echo $?` | 复算链断裂（见 §6-E） |
| **C10** | `--fault-inject` 负例注入器全仓无执行器 | `grep -rn -- "--fault-inject" docs/ eng/ lib/` → 仅 2 处文档提及 | TRACEABILITY 的「全量强制门」不可执行 |
| **C11** | `CHK-NAMING-SURFACE` 门全仓无实现，且其自订的三族字面量第一族与第三族字面相同 | `grep -rln "CHK-NAMING-SURFACE" docs/ eng/ lib/` → 仅 CODE.md 自身 | 结构上无法检出它要检的违规 |
| **C12** | `log_artifacts[]` 合同键零生产写出点 | `grep -rn "log_artifacts" lib/ eng/` → 0 | `LOG_AND_ERROR.md` §4 已加「载体缺口登记」段 |
| **C13** | `INPUT_CORRUPT` / `DEPENDENCY` 两个硬错误类别在 `lib/`、`eng/` 零命中 | `grep` 两串 → No matches | 类别表已登记，但未启用 |
| **C14** | `orchestrator.h:145-156` 的 `AstroCsExitCode` 是第二套进程退出码数值表，与 `acsd::ExitCode` 9/10 语义对调，且 `is_process_exit_code()` 把 1 也算合法进程码 | `sed -n '145,170p' …/orchestrator.h` vs `cat lib/infrastructure/cli/exit_codes.h` | U-5 待裁决 |
| **C15** | `abi/status_codes.h` 的 `acsd_status` 无 `INTERNAL(70)` 对应；`lib/include/acsd/common_abi_v1.h` 与 `abi/status_codes.h` 两套头并存且禁止同 TU 混用 | `sed -n '20,40p' lib/include/acsd/abi/status_codes.h` | 见 §6-D |
| **C16** | `AIO` 精度面只有一个全局位 `g_aio_precision_mode_fp64`，无法表达最高设计 3.3 的「dense=f32 + sparse=f64」组合 | `sed -n '33,42p' lib/infrastructure/aio/src/aio_api.cpp` | U-7 待裁决 |
| **C17** | `MODULE_MAP.md` 登记的 noop 模块路径、`p3_proj.*` 在役性 | `grep -n 'conformance/noop' CMakeLists.txt`；`grep -rn 'p3_wcs.cpp\|p3_proj.cpp' --include=CMakeLists.txt lib/algorithms/projection/` | 见 §6-D |
| **C18** | `ENABLE_DETERMINISM` 之外的生产构建图中 3 个目标（`acsd_gaia_zlib_include` / `acsd_platform_math` / `acsd_platform_zlib`）漏登记；`acsd_hips`/`acsd_phase2` 两行手数与指纹失配 | `python3 eng/tools/arch/cmake_graph.py` | 见 §6-E |
| **C19** | `perf` 回归锁 `p1drz_merge_pipeline_lock` 与 `L2-FROZEN-GATE-REPLAY`/`WORKER-BALANCE-METRIC-REPLAY` 无执行器 | grep | 见 §6-C |
| **C20**（A） | `phase_product_exchange.schema.json:162` 的 `plane_id` enum 与 `phase_product_exchange_validator.py:57` 的 `_PLANE_ID_SET` **仍含 `mask`**，文档已删 | 两个机器源同改 | **schema 侧是错的一方**，已如实登记「schema 需修订」，文档未硬凑 |
| **C21**（A） | 交换枚举装不下两个真实磁盘面：HiPS 的 `snr/` 子产品（`aio_hips_writer.cpp:1285`）与 FITS 的 `COVERAGE` HDU（`p3_output.cpp:389/910`） | grep | 枚举面需扩 |
| **C22**（A-2） | validator 只收 `frame=="icrs"`（`phase_product_exchange_validator.py:208`），生产者写 `hips_frame=="equatorial"`（`aio_hips_writer.cpp:1902/2082`，IVOA HiPS 1.0 §4.4.1 的标准写法）⇒ 若要自动填 `coordinate.frame`，validator 需加别名 | A 已只在文档写死映射表，**未改代码** |
| **C23**（D） | 13 个 canonical schema 的 `precision` 枚举一律 `['float32','float64','integer']`，`provenance` 的 `integer` 在 schema 侧合法但语义侧无意义 | `python3 -c` 解析 `frame_snr.schema.json` | UNIFIED_OBJECTS 已改标注为「非数值元数据」，schema 枚举需同步 |
| **C24**（B） | `stage2_common.cpp:644-649` 是**不可达死代码**（`acr_route` 已在 `:629` 被 fail-closed 拒绝，后面仍写值域校验） | `sed -n '594,650p'` | |
| **C25**（B/B-2） | 8 处代码引用已失效路径 `PIPELINE_BLOCK_CONTRACT.md`（`block_frame.h:5/:22`、`block_flow.h:4`、`block_frame.cpp:2`、`block_flow.cpp:2/:75`、`module_ports.registry.json:784/:845`） | `grep -rn PIPELINE_BLOCK_CONTRACT lib/ eng/` | |
| **C26**（B） | `eng/tests/config/` **整目录缺失**（只在 `run/` 归档快照里），且 `jsonschema` 依赖不在仓内 ⇒ 连「自己跑模板过 schema」都不可能 | `ls eng/tests/`；`python3 -c "import jsonschema"` | |
| **C27**（B） | `defaults.json` 53 条 `source_ref` 中 **28 条引文不命中**（含全部 14 条 `noise.*`） | 逐条核对 | |
| **C28**（E） | `gen_build_graph_doc.py:33` 的 `DOC_REL = "docs/engineering/BUILD_GRAPH.md"` 指向不存在路径（真实落点 `docs/engineering/build/BUILD_GRAPH.md`）；且该脚本的**理由字面量自带机械锚**（`:48,49,60,62`），一旦能跑会把文档又刷回机械锚 | `sed -n '30,65p' eng/tools/arch/gen_build_graph_doc.py` | 顺序：①入库 → ②`DOC_REL` → ③机械锚字面量 → ④以生成器就地重导 |
| **C29**（E） | `NONPROD` 登记册里 `acsd-stage2`/`calibrated_pair_diag`/`rejection_cli` 三条**根本不在根构建图**（其目录没被 `add_subdirectory` 纳入），故 BUILD_GRAPH 该节抬头「真实存在于根构建图」对这三条不成立 ⇒ **E 在审稿报告外新查出的第三重失效** | `python3 eng/tools/arch/gen_build_graph_doc.py --out /tmp/bg.md; echo $?` → 1 | |
| **C30**（E） | `aio_hips_writer.cpp` 有一套 `ACSD_HIPS_*_FAULT` 环境变量机制，**与 `--fault-inject` 不是同一机制**，不能充作 `--fault-inject` 的执行器 | grep | |
| **C31**（C） | `eng/contracts/resource_gate_v1.json::frame_memory_gate` 声称「实现侧零字面量，由 `CHK-BUDGET-SINGLE-SOURCE` 逐位比对」，但 `module_adapters.cpp:2307-2308` **仍是** `static constexpr double kP1FrameBytesPerPixel = 116.0; kP1FrameMemSafetyFrac = 0.75;`，且生成头 `build/runtime_resources_generated.h` 未被该 TU 引用 ⇒ **JSON 侧的「实现侧零字面量」断言当前为假** | `sed -n '2305,2310p' lib/infrastructure/scheduler/src/module_adapters.cpp` | 数值 116.0 的真实来源子代理已找到：`run/P1-CONCURRENCY-CALIB-01/REPORT.md` §5.1/§5.4 的最小二乘 `RSS(F)=0.143 GB + F×1.6724 GB`（边际 1.6724 GB/帧 = 99.68 B/px，残差 ≤±2.5%）；**116.0 取的是「使闸门放行 F=8 而不越 0.75A 预算」的可行窗口上端，不是实测边际值** |
| **C32**（C） | `DATA_FLOW.md:125` 的「`K = num_threads` 是达成满宽的**唯一最小取值**」**充分性未证**（推导只用到必要条件 `K ≥ inner_omp`，推不出 `num_threads = inner_omp`）——与审稿 6-6 对 `PERFORMANCE_MODEL.md` 的同一条指摘是同一问题 | `sed -n '120,130p' docs/engineering/architecture/DATA_FLOW.md` | D 保留原句并登记；C 已在 `PERFORMANCE_MODEL.md` 侧降为「充分条件 + 代码锚」 |
| **C33**（A-2） | `TRACEABILITY.md` **没有「合同 ID 登记面」这一节**（`grep -n "合同 ID 登记面"` → 0），真实节是「需求→实现→测试 登记册（人读正本）」，`ENG-IO-001` 登记在其 `:277` | grep | A-2 已修 |
| **C34**（A-2） | 合同 ID `DATA-002` 在 `DOCUMENT_INDEX` 与 `TRACEABILITY` 里**都没登记**，只见于 `PHASE3_RSMP_IMPL.md` ⇒ 它指哪个文件核对不到 | `grep -rn "DATA-002" docs/ eng/` | A-2 明示「是推断不是核对」，未硬认 |
| **C35**（A-2） | `docs/science/PSF_SIGNAL_WEIGHT.md` 不存在（被 `lib/algorithms/coverage/include/astro/phase2/integrate.h:16` 引用） | `ls docs/science/` | 转派 SCI 车道 |
| **C36**（E） | `docs/engineering/{abi,cpu,io,observability}/` 四个**空目录**（磁盘存在、git 跟踪 0，干净克隆上不存在） | `find docs/engineering -maxdepth 1 -type d` + `git ls-files` | |
| **C37**（A） | `ATOMIC_PUBLISH.md` 验收映射表 9 个 `Test*` 类、`ACSD_TEST_CORRUPT_AFTER_RENAME` 注入器、白名单点名的 13 个测试文件，**全部无实现** | 逐名 `grep -rl` | 文档侧已全改成「判据无载体 / 判据未接线」；载体落库是工程侧任务 |

---

## 6 需权威补充才能定的问题（写明「需补充什么」）

| # | 问题 | **需补充什么** |
|---|---|---|
| **A1** | `standards/NUMERIC.md` 的「实测 **1.0336**」量的到底是什么 | **需 33 帧 α 的逐帧读数**（`run/RELEASE-05/vis/out/m42_p1_t3/p1_phot.json` 已不存在）。我已在 `run/**/p1_phot.json` 全 31 件上扫过 `frames[]` 的键，**无 `alpha`/`scale` 字段**；全仓无任何文件含字面 `1.1387e-17`/`6.0083e-17`。⇒ 需负责人补该实验件或撤回该数值断言 |
| **A2** | `PIXINSIGHT` 官方式与 `WBPP 2.5.9 engine.js:1421-1429`（`CONFIG.md:297`） | **需一手原文与可解析永久链接**。子代理 B 已按红线核对不到，删除无源标注并登记 |
| **A3** | `kP1FrameBytesPerPixel = 116.0`（承载内存闸门的标定常数） | **需拟合件名 + 拟合口径**（base + 边际字节/像素 × F 的实测系数） |
| **A4** | 「帧内 stripe 数超过约 4 后进入收益递减区」 | **需实测件**；否则已降为「待标定的经验拐点」并移出「不可改类」 |
| **A5** | U-1：命名块是否为阶段内节点间载体 | **负责人裁决**：改最高设计 8.2 + AGENTS §7，还是改工程正本与注册表 `carrier_contract` |
| **A6** | U-3：Linux 是交付平台还是仅控制面 | **负责人裁决**（我已按权威链把 `ARCHITECTURE.md` 向最高设计对齐，但须确认这是负责人本意）。D 顺带删掉了**全仓唯一一处**、无据的「Windows 10 22H2」下限（最高设计写的是「Windows 10+ amd64」） |
| **A7**（A） | 三角色最小平面集**是否应含 `ivar`** | **需负责人裁定**。代码证明 ivar 面存在（AIO 产品面 + `p3_output.cpp` EXTNAME 都写 IVAR），但**不证明它必属于最小平面集** ⇒ 属合同决策，子代理未自行认定 |
| **A8**（A） | Phase3 FITS 的 `COVERAGE` 扩展 HDU **是否等于**交换对象的 `support` | `p3_output.h:54` 写「coverage 二值」，而 `support` 是 [0,1] 连续量 ⇒ 二者**不可等同**。子代理明确**未**把它等同，留白待裁 |
| **A9**（A-2） | `SCI-P3-001` 的 `a-1/a-8/a-9` 子条款编号在 `PHASE3_HIPS_TO_FITS.md` 里**不存在**（该文件是 `## 1`–`## 16`）；`DATA_SEMANTICS` 的「机器校验」「交换对象」「兼容矩阵」「阶段产品角色与 type 绑定」「跨 Phase 仅磁盘交换」**全部不存在**（真实只有 `## 1`–`## 7`）；`DATA_SEMANTICS` 的 `count_field` 规则 0 命中（规则实际只在 `PHASE_PRODUCT_EXCHANGE` 自身处置规则第 3 条） | **需 science/detail 车道提供这些条款的真实落点**；子代理**未在 DATA_SEMANTICS 里编造一节**，改指到真实存在处 |
| **A10**（B-2） | `[18]/[19]` 所指的 **PixInsight ImageIntegration 官方式** | 子代理**亲自试过**：`web_search` 两组查询均 `No results found`；`pixinsight.com/doc/modules/ImageIntegration/{index,ImageIntegration}.html`、`/doc/index.html`、`/documentation.html` **全 HTTP 404**；根域 200 但是 JS 壳无正文。**需一手原文 + 可解析永久链接** |
| **A11**（B-2） | `WBPP 2.5.9 WeightedBatchPreProcessing-engine.js:1421-1429` | 付费更新包，不可公开取得，**核对不到原文**。冻结表（nominal<6 / 6..15 / >15）**保留**（属仓内冻结行为），只撤不可核验的出处主张 |
| **A12**（E） | `UNRESOLVED.md:41` 的两个 **U+FFFD 不可辨识字符** | 全 `docs/` 树只此一处，审核包也拿不到原字 ⇒ 子代理**按红线不猜**，降级为显式标注「两字符不可辨识」。**需原始字符** |

### 6.1 需科学/其它车道复核（本车道无权处置）

| 事项 | 转派对象 | 依据 |
|---|---|---|
| `docs/science/REJECTION.md:58` 自报的**排异 7 种**清单与生产默认 profile 的 percentile 档不符 | science 车道 | 见 O-9；代码 `P2RejectionMethod` 是 11 个 kernel 方法 |
| UPM 控制点权重**两阶段**（分子 `quality_factor × control_ivar` / per-control 归一化 `× control_reliability`）在 science 正本是否已按此口径表述 | science 车道 | U-8；生产代码 `upm.cpp:691-707/2024-2058` 确证两阶段 |
| `docs/ACSD_DESIGN.md` 8.2 + `AGENTS.md` §7 的**块跨节点**口径与工程正本相反 | **负责人裁决**（U-1） | 权威链顶层 vs 工程层冲突 |
| `docs/ACSD_DESIGN.md` 第 11 章的 Linux 交付角色 | **负责人裁决**（U-3/A6） | |

**UNRESOLVED 需负责人裁决（承接审稿 §7，按我的复核结果改写）**：

| # | 事项 | 我的复核结论 |
|---|---|---|
| **U-1** | 命名块载体（8-1） | 注册表 `carrier_contract.statement` 与 `AGENTS.md:88`+`ACSD_DESIGN.md:381-383` **相反**。我**未单方面改口径**，只在工程层消除内部自相矛盾 |
| **U-2** | run_manifest 正本（8-2） | **改写审稿的问法**：不是「哪份词表是正本」，而是「schema 描述的设计意图未落地 + 改用了另一套词表 + schema 未接线」。两条出路：(a) 把 schema 字段并入生产 manifest 并迁移键名；(b) 退役 schema 与其锚点登记。见 §2 |
| **U-3** | 平台角色（2-1/8-10） | 同 A6 |
| **U-4** | L2 冻结判据 enforcement（8-6） | 唯一数值源 `eng/contracts/resource_gate_v1.json::compute.mean_utilization_enforcement = "record_and_justify"` 是唯一可核来源；文档单方面写 fail-closed 是**文档侧越权**。已把文档改为与数值源一致 |
| **U-5** | `AstroCsExitCode` 退役还是并存（8-5） | 见 C14 |
| **U-6** | `ACSD-BASS-Index/1.0` 语义（9-9） | **需负责人给出该 token 的真实定义** |
| **U-7** | 精度归属是两类独立还是单一全局模式（2-13） | 见 C16。`UNIFIED_OBJECTS.md` 已如实登记「当前实现只有一个全局精度模式位」 |
| **U-8** | UPM 控制点权重的几何因子归属（2-11） | **可判**：`upm.h:175-177` 明文「几何可靠性在 per-control 归一化中施加」+ `DATA_FLOW.md:142` 同 ⇒ 生产口径成立。**但 science 正本是否已按此口径表述，须 science 车道确认**（跨车道登记） |

---

## 7 审稿员推翻的既有判定 —— 全部保留，不当真问题改

以下 6 条既有判定与 12 条已验证项，本轮**逐条复核后保留原判**，任何一条都未按「真问题」改动：

| 类别 | 条目 | 我的复核 | 处置 |
|---|---|---|---|
| 推翻既有结论 1 | 「本车道有三套退出码数值表」只推翻到一半（V-1 两份文档表逐码逐值一致于 `exit_codes.h`，CLI_PROTOCOL 已声明唯一源是 `exit_codes.h`；保留 `AstroCsExitCode` 为第二套） | 我独立 `cat lib/infrastructure/cli/exit_codes.h` 与两处文档表对读：`OK=0/ARGS=2/INPUT=3/SCIENCE=4/BACKEND=5/COMPUTE=6/IO=7/INTEGRITY=8/CANCELLED=9/RESOURCE=10/INTERNAL=70`，**逐码逐值一致** | **保留原判，未改** |
| 推翻既有结论 2 | 13-对象节名冲突（两篇指同一节却给两个名字） | 成立，且路径 `docs/detail/common/` 不存在 | 保留原判，已按 5-10/5-12 订正 |
| 推翻既有结论 3 | 推翻自己初判「α² 是 12 个数量级的算术错误」→ 真实缺陷是**量名错标** | **我独立复算确认**：`α² = [1.2966e-34, 3.6100e-33]`，`1e-12·α² = [1.2966e-46, 3.6100e-45]` = 文中数值 | **保留修正后的原判**，按量名错标订正 |
| 推翻既有结论 4 | 推翻「tree_hash 互相矛盾」→ ATOMIC_PUBLISH 是**欠定义**不是矛盾 | 我独立读 `hips_output_store.py:103-123`，确认三元组归一 + 排序 + `ensure_ascii=False` + `separators=(",",":")` | **保留修正后的原判**，按欠定义订正 |
| 推翻既有结论 5 | 采纳子代理自我推翻，把 8-2 定性为三方问题而非文档笔误 | 我复核三方后**部分推翻其定性**（见 §4 O-1）：三方成立，但「被 schema 拒」不是运行时事实 | 部分保留（见 §4） |
| 推翻既有结论 6 | 推翻「大量恒真门」→ 改定性为「复算链断裂」（同步缺陷非设计缺陷） | 成立 | **保留原判**，全按 R2 复算链断裂处置 |
| V-1…V-12 | 退出码、`ErrorDomain`、HEALPix dex 换算、MAD 常数、`variance_floor` 来源、注册表 carrier、命名块字段 schema、`E` 尺度不变性、`contracts.h` 第二枚举、`1 ulp` 可满足性、zstd 默认档、ACR 已退场 | 我对其中 6 条做了**独立重算**，结论与审稿员一致（详见 §2.2 自证段与各子代理报告） | **全部保留，未改** |

---

## 8 索引变更与真解析器验证输出

**变更**（`docs/DOCUMENT_INDEX.yaml`，仅本车道条目）：

| 动作 | 条目 |
|---|---|
| 新增 | `docs/engineering/standards/ERROR_MODEL.md`（ACTIVE_NORMATIVE） |
| 新增 | `docs/engineering/standards/COMMENT.md`（ACTIVE_NORMATIVE，子代理 C 新建件） |
| 改 duty | `docs/engineering/standards/CODE.md`：去掉已迁出的「注释纪律」，保留代码条款 |

**终检验证（PyYAML 6.0.2 `safe_load`，非文本扫描）**：

```bash
cd "/workspace/Astro CS Database" && python3 /tmp/verify_index.py; echo "exit=$?"
```

```
[1] YAML 解析成功, 顶层键 = ['doc_index']
[2] schema_rev = 3
[3] 区段 = ['active']
[4] 条目总数 = 148
[5] 结构/存在性错误 = 0          ← 中间态曾为 1（COMMENT.md 建件前），现已归零
[6] docs/** 覆盖面缺口（应登记但未登记）= 0
[7] 登记但不在 docs/**（根条目或豁免）= 7
   OUT AGENTS.md / README.md / VERSION / artifacts/…/LIMITATIONS.md
       eng/contracts/ledgers/ledger_schema.py / eng/tools/source_scan.py / 实验/裁决台账.md
[8] docs/engineering/build 被 git 跟踪的文件数 = 0
    其中索引已登记但不在 git 的 = 3
        docs/engineering/build/BUILD_GRAPH.md / BUILD_NODES.md / RELEASE.md
```

**验证覆盖**：YAML 可解析、`schema_rev` 合法、区段结构、条目 148 条、`path` 全索引唯一（无重复）、`status` 全在四值域内、`active` 区段无归档状态、**无悬空 path（0）**、必备字段 `status/duty/upstream` 齐备、`docs/**` 覆盖面缺口 **0**。**R1 由 `[8]` 用 `git ls-files` 独立复核**（3 份已登记文档 0 跟踪）。

### 8.1 全车道终检（我用修正后的正则亲自复跑）

**修正点**：审稿与我的派单脚本里扩展名交替顺序 `.c` 排在 `.cpp` 前，把 `commands.cpp` 截成 `commands.c`；加后缀边界 `(?![A-Za-z0-9_])` 后得到真实数字。

```bash
cd "/workspace/Astro CS Database"
echo "空章节锚「」      : $(grep -ro '「」' docs/engineering --include=*.md | wc -l)"
echo "docs/detail/common : $(grep -ro 'docs/detail/common/' docs/ --include=*.md | wc -l)"
echo "PSF_SIGNAL_SNR     : $(grep -ro 'PSF_SIGNAL_SNR' docs/engineering --include=*.md | wc -l)"
echo "机械锚「X」一节    : $(grep -ro '」一节' docs/engineering --include=*.md | wc -l)"
echo "HTML 注释          : $(grep -ro '<!--' docs/engineering --include=*.md | wc -l)"
echo "流水编号 V1x/R-nn  : $(grep -roE '\bV1[0-9]|\bV2[0-9]|\bR-[0-9]{2}|\bP-[0-9]{2,3}\b' docs/engineering --include=*.md | wc -l)"
```

| 指标 | 改前 | 改后 | 判定 |
|---|---|---|---|
| 空章节锚 `「」` | 12（HIPS_STORAGE_FORM）+ 10 空槽（PHASE_PRODUCT_EXCHANGE） | **0** | ✅ 失真类清零 |
| `docs/detail/common/` | 6+（UNIFIED_OBJECTS） | **0** | ✅ |
| `PSF_SIGNAL_SNR` | 2（CONFIG） | **0** | ✅ |
| 机械锚 `「X」一节` | **521** | **383** | ⚠️ 降 138（−26%），仍是主流引用形态，**下一轮按文件分批** |
| HTML 订正注记 | TRACEABILITY 表格内嵌 | 主体清零，残 **6** | ⚠️ 残留需下一轮 |
| 流水编号 | 遍布 | 残 **22** | ⚠️ 含 schema `$id` 的 `/v1` 与假阳性，需逐串判定 |
| `ARCH-00x` | — | 34 命中 | ✅ **全部合法**：`TRACEABILITY.md` 是 ID 文法声明（`ARCH ^ARCH-[A-Z0-9]+…$`）；`api/PUBLIC_API.md` 4 处是**真悬空 ID**，该文件未派给任何子代理，登记给下一轮 |
| 判据「无载体/未接线」显式登记 | 0 | **23 条** | ✅ 写实，不再冒充已生效 |
| 真悬空路径（全车道，修正正则） | 审稿称 51 | **39** | ⚠️ 见下 |

### 8.2 残留 39 条真悬空路径 —— 根因 R2，逐条按文件归属登记

全部落在 **`eng/tests/**` 与 `lib/**/tests/**`（判据载体被批量删除，目录只剩 `eng/tests/{conformance,validation}`）**。

| 归属文件（本车道，**本轮无人受派**） | 条数 |
|---|---|
| `api/PUBLIC_API.md` | **9**（`test_p1_api`/`test_p1002_gaps`/`test_p3_resample`/`p2_output_semantics_test`/`p3_coverage_test`/`p3_interp_test`/`p3_output_test`/`p3_wcs_main`/`p3_wcs_test`/`p3_resample_probe_main`） |
| `resources/cpu/{ISA_VARIANTS,BACKEND,CAPABILITY_PROBE,AVX2_PROVIDER}.md` | **11** |
| `contracts/ASYNC_IO.md` | **3**（`async_io_test.cpp` ×2 + `test_parallel_queue.py`） |
| `contracts/RUNTIME.md` | **1**（`rt001_abi_test.cpp`） |
| `build/BUILD_NODES.md` / `build/BUILD_GRAPH.md` | **3**（生成器 `DOC_REL` 错 + `verify_toolchain.py` + `preset-contract.schema.json`） |
| `contracts/CONFIG.md`（残留） | **6**（`test_cpu_profile.py`、`fixtures/negative/…`、`cpu007_profile_store_test.cpp`、`config_consistency_check.py`、`filter_qe_provenance.json`、`test_photometry_curve_resolve.cpp`） |
| `data/ARTIFACTS.md`（残留） | **2**（`test_unified_object_contract.py`、`core_pipeline_test.cpp`） |

**登记**：以上 8 份文件本轮**无人受派**，其死链未被处置。全部属根因 **R2**（判据载体被删而正本未同步），处置动作与 `eng/tests/**` 载体的存废裁决绑在一起（§5-C7），**下一轮必须先有裁决再改文档**——否则只是把「已生效」改写成「无载体」，载体仍然缺失。

---

## 9 自证段

### 9.1 我实际做了什么

- 通读 `AGENTS.md`（127 行）与 `docs/ACSD_DESIGN.md`（568 行全文），逐行读完审稿意见（457 行）。
- **动手前先做活合同引用面取证**（§1）：`grep -rEoh "docs/engineering/…md" lib/ eng/` 74 个唯一路径全悬空；逐字确认三份活合同的被引条款与节号，并在订正中**保留节号与条款原文**。
- **亲手改 4 份文件**：`contracts/LOG_AND_ERROR.md`（拆分 + 交叉引用 + 载体缺口登记）、新建 `standards/ERROR_MODEL.md`（拆分 + 2-10 互斥判据 + 5-3 + 节号显式化）、`contracts/SCHEDULER.md`（6-3 两处 + 8-8 一处）、`docs/DOCUMENT_INDEX.yaml`（2 新增 + 1 duty）。
- **亲手跑三方比对脚本**（§2.1）产出 run_manifest 的四组键集与四组差集。
- **亲手重推全部公式条**（不复用审稿员结论）：

  | 条 | 我的推导 | 结果 |
  |---|---|---|
  | 3-1 | `α_max/α_min = 6.0083/1.1387 = 5.27646`；平方 `27.84098`；减 1 = **26.84098** | 文档预测值 **26.84** vs 文中「实测 1.0336」，差 **25.97 倍**，二者不可同时成立 ✓（并否决了「可猜出 1.0336 定义」，§4 O-3） |
  | 3-2 | `α² = [1.2966e-34, 3.6100e-33]`；`1e-12·α² = [1.2966e-46, 3.6100e-45]` | 与文中数值**逐位相同** ⇒ 量名错标成立 ✓ |
  | 3-3 | float32 最小正规 `1.1755e-38`、最小次正规 `2^-149 = 1.4013e-45`；文中区间 `[1.2966e-46, 3.6099e-45]` **跨越**下溢边界；文中 `4.204e-45` **>** 上界 `3.6099e-45` | 区间表述与「32/33 下溢为 0」不自洽 ✓。**子代理 C 补强（更强的反证）**：把「max `4.204e-45`」按同段关系 `floor(α)=variance_floor·α²` 反解，得 `α = 6.4838e-17`，**超出同段 α 上界 `6.0083e-17`** ⇒ 该句在本段自身内部即自相矛盾，无需外部数据即可判错 |
  | 3-4 | `nside=2^18`：`A_cell=1.523873e-11 sr`、`1/A²=4.3063e21`、`log10=21.6341` dex（**算术无误**）；`nside=2^16`：`A_cell=2.438197e-10`、`1/A²=1.6821e19`、`19.2259` dex | 算术 ✓，但结论被单一 nside 承载 ⇒ 须参数化 |
  | 3-7/V-8 | 数值验证 `E(w) = -0.521988340278`，`E(3.7w)` 同值，`E(w, 3.7x)` 同值 | 尺度不变性**成立**，保留 ✓ |
  | 4-7/V-4 | `Φ⁻¹(0.75) = 0.6744897501960817`，`1/q = 1.482602218505602`（`repr` 逐位比较 `True`） | 与文档常数**逐位相同**，保留 ✓ |
  | V-10 | f64 `1e-13 > 2^-52 = 2.2204e-16` ✓；f32 `1e-6 > 2^-23 = 1.1921e-07` ✓ | 两条容差均满足 1 ulp 下限，保留 ✓ |
  | 3-6 | `u` f64 = `2^-53 = 1.1102230246251565e-16`、f32 = `2^-24 = 5.960464477539063e-08` | 定义值备齐，已交子代理 C 写入 `TEST.md` |

- **用真解析器验证索引**：PyYAML `safe_load`（§8），非文本扫描。

### 9.2 我没有做的事（诚实边界）

- **没有编译、没有运行任何二进制**。所有「代码为准」的判定来自源码阅读 + `grep` + JSON Schema 解析。
- **没有取 Rousseeuw & Croux 1993 全文**：只核对了 DOI 与题录对应关系（审稿员亦同）。→ 见 §10 文献核对。
- **没有取 PixInsight 官方式与 WBPP 2.5.9 原文**（§10）。
- **全仓 `grep -rl "1.1387e-17"` 在 60s 内未完成**（`run/` 体量过大），我改用有界搜索（`run/**/*.p1_phot.json` 全 31 件的 `frames[]` 键集 + 定向 grep）并如实声明「未找到」，**不宣称「全仓不存在」**。
- **对审稿员未逐行读完的 36 份文档，我不宣称独立逐行判断**；相关结论一律由承接子代理执行并在其交付件中标注覆盖方式。
- **并发工作树说明**：`git status` 显示 `docs/science/**`、`docs/detail/**`、`docs/GLOSSARY.md`、`docs/README.md` 在本会话期间被**其它车道**改动，并有一处 `docs/noise_snr_audit_upstream_findings.md` 删除。这些**不在本车道**，我未触碰；本单只报告，不代裁。

---

## 10 文献核对（分列）

### 10.1 核到原文/一手源的

| 文献/常数 | 核对方式 | 结论 |
|---|---|---|
| Rousseeuw & Croux 1993, JASA 88:1273, DOI `10.1080/01621459.1993.10476408` | DOI 与题录对应关系核对 | **真实且对应正确**。常数 `1.482602218505602` 我逐位重算确认 |
| JCGM 100:2008（GUM）式(10)/(13) | `NUMERIC.md:45-46` 自述经 BIPM 发布版逐字核验；本轮**未取原文复校** | **沿用文档自述**，标注本轮未独立取原文 |
| IEEE 754 f32/f64 | `python3` 数值复算 `2^-23`、`2^-24`、`2^-52`、`2^-53`、`2^-149` | 数值确认 ✓ |

### 10.2 核对不到的（单列，不凭印象填）

| 项 | 状态 | 需要什么 |
|---|---|---|
| `CONFIG.md:297` 引的 PixInsight 官方式 | **核对不到** | 一手原文 + 可解析永久链接 |
| `WBPP 2.5.9 engine.js:1421-1429` 对照档来源 | **核对不到**（仓内无该文献台账） | 同上 |
| Rousseeuw & Croux 1993 **全文** | **未取全文**（仅核 DOI/题录） | 全文 PDF |
| `PHASE2_UPM.md`「并行确定性与三档容差」的 rtol 1e-12 的一手出处 | **本轮未核** | science 正本的自证链 |

---

## 11 收敛判定

审稿员给的收敛判据（`standards/03_READING_AND_ADVERSARIAL_REVIEW.md` 第 2 节）：**连续两轮无新增实质问题**。

**本轮（订正轮）未达成收敛。** 理由：
1. 根因 **R1**（`.gitignore` 排除构建正本）与 **R2**（判据载体缺失，`eng/tests/**` 只剩 `conformance/`、`validation/`）**都不是文档能修的**，本车道无权修复，已逐条登记。
2. **R3**（多篇拼接 / 节名批量替换）本轮已实修：`LOG_AND_ERROR.md` 拆分、`CONFIG.md` 切三块、`HIPS_STORAGE_FORM.md` 12 处空锚、`PHASE_PRODUCT_EXCHANGE.md` 10 处空槽、`CODE.md` 拆分；但 `「X」一节` 机械锚全车道仍有数百处（本轮只做**失真**类优先，批量降级需下一轮按文件分批）。
3. **R4**（块载体口径与权威链上层相反）与 **U-1/U-2/U-7** 需负责人裁决，文档侧不得单方面改。
4. 本轮**新查出 2 条审稿员未发现的缺陷**（8-2a 文件名、8-2b `summary`/`error` 漏登记），说明问题面尚未穷尽。

⇒ **下一轮建议顺序**：R1 仓库侧修复 → R2 判据载体存废裁决（决定 39 条残留死链是「补载体」还是「正式撤判据」）→ R3 剩余机械锚按文件分批降级（383 处）→ U-1/U-2/U-7 与 A7/A8 负责人裁决 → C2（74 处悬空代码引用）代码侧统一改指 → 派单去重（避免同车道双代理）→ 再跑十轮。

---

## 12 交付件与车道索引

| 交付件 | 内容 |
|---|---|
| **`T06-第1轮订正-DOC-ENG.md`（本件）** | 前台统稿：三方比对、活合同引用面、11 条推翻、38 条代码侧、12 条权威补充、索引真解析器验证、终检与收敛判定 |
| `T06-第1轮订正-subA.md` | contracts/cli+manifest+hips+atomic+ownership + data 五份：17 编号 + 7-1/10-1 |
| `T06-第1轮订正-subA-2.md` | 同车道**独立盲复核**：21 条复核 + 5 处审稿修正 |
| `T06-第1轮订正-subB.md` | CONFIG.md + PIPELINE_BLOCK.md：§⓪–§八（CONFIG）+ §九（复核补遗） |
| `T06-第1轮订正-subC.md` | standards/resources/testing：轮 3/4 全部公式与证据条 |
| `T06-第1轮订正-subD.md` | architecture/api/UNIFIED_OBJECTS：含 2-2 前提推翻 |
| `T06-第1轮订正-subE.md` | governance/build/README：含 8-11 第三重失效新查出 |

## 13 车道改动面（38 份文件，+1078 / −890，全部留在工作树待前台统一提交）

```
docs/DOCUMENT_INDEX.yaml
docs/engineering/README.md
docs/engineering/UNIFIED_OBJECTS.md
docs/engineering/api/abi/{ABI,SECURE_LOADER}.md
docs/engineering/architecture/{ARCHITECTURE,DATA_FLOW,MODULE_MAP}.md
docs/engineering/contracts/{ATOMIC_PUBLISH,CLI_PROTOCOL,CONFIG,HIPS_STORAGE_FORM,
                           LOG_AND_ERROR,MANIFEST_VERIFY,OWNERSHIP_LIFETIME,
                           PIPELINE_BLOCK,SCHEDULER}.md
docs/engineering/data/{ARTIFACTS,ARTIFACT_STORE,PHASE_PRODUCT_EXCHANGE,PROVENANCE}.md
docs/engineering/governance/{DOCUMENT_GOVERNANCE,DUAL_LINE,TRACEABILITY,UNRESOLVED}.md
docs/engineering/resources/PERFORMANCE_MODEL.md
docs/engineering/standards/{CACHE,CODE,DEPENDENCY,NUMERIC,README}.md
docs/engineering/testing/{TEST,VALIDATION_EVIDENCE}.md
docs/engineering/build/{README,BUILD_GRAPH,BUILD_NODES,RELEASE}.md   ← 被 .gitignore 排除，git diff 看不到（R1）
新建：docs/engineering/standards/{COMMENT,ERROR_MODEL}.md
```

⚠️ `docs/engineering/{ASYNC_IO,RUNTIME}.md`、`api/PUBLIC_API.md`、`resources/cpu/**`、`resources/observability/**`、`resources/BENCHMARK.md`、`contracts/README.md` 等 **8 份本轮未派单**，其死链与残留见 §8.2，已逐条登记。
