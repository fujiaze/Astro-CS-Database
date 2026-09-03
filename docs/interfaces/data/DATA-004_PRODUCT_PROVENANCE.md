# DATA-004 产物溯源与版本语义（Product Provenance）

> 文档 ID：DOC-DATA-PRODUCT-PROVENANCE-001
> 状态：ACTIVE_NORMATIVE（DATA-004 冻结语义，本任务交付）
> Owner：SA-DATA-06 ｜ 前序基线：`0d32c07d65c6d7489fa408cbafaa98ddf9ecf4da`（DATA-004 base）
> 上游权威：AstroCS_ENGINEERING_CONSTRAINTS.md §A.3/A.4/A.6（三阶段仅通过原子发布、
> 哈希与 provenance 完整的磁盘产品/manifest 交换）、DATA-001（typed manifest schema）、
> DATA-002（三阶段产品交换合同，R-DISK-ONLY / R-EVIDENCE-REQUIRED）、
> DATA-003（生产 ArtifactStore：原子发布 + 唯一 producer + manifest hash sidecar）
> 机器形态：`runtime/artifact_store/provenance.py`（provenance 层，执行校验器）、
> `runtime/artifact_store/production_store.py`（DATA-004 接线：provenance sidecar /
> 版本门 / 消费门）、`tests/artifact/test_provenance.py`（验收测试）
> 下游：RT-002（phase-isolated runtime 消费门接线）、RT-006（trace 溯源字段）、
> IO-003（原子 HiPS/manifest 输出复用 provenance sidecar 语义）、LOG-001（脱敏语义对齐）

## 0. 目的与范围

DATA-003 建立了生产 ArtifactStore 的原子发布与校验读；本任务（DATA-004）在其上
补全**产物溯源（provenance）与版本语义**：

1. 区分 **revision 类别**：product / module / ABI / data schema / doc revision /
   history——每类有独立语义与校验规则，不允许混为一谈；
2. provenance 写 **source commit / config / provider / worker / input hashes、
   science IDs**；
3. **确定性 provenance digest**：同输入配置 ⇒ 同 digest（运行时间/目录等运行
   事实不参与 digest）；
4. **旧 product 版本不静默接收**：发布门（history 已替换版本拒绝重发）+
   消费门（min_product_version 门槛）+ data_schema 绑定（新数据旧 schema /
   旧数据新 schema 一律拒绝）；
5. **privacy scan**：provenance 相关文本/诊断不泄露绝对用户路径与凭据。

约束来源：`AstroCS_ENGINEERING_CONSTRAINTS.md` A.6（阶段间只通过原子发布、哈希
和 provenance 完整的磁盘产品/manifest 交换）；DATA-002 `R-EVIDENCE-REQUIRED`
（缺 manifest / 缺 hash / 缺 schema / 缺 units → 拒绝）；DATA-003 接线冻结语义
（发布物保持严格 DATA-001 manifest 形态，不附加字段——provenance 以独立
sidecar 旁路持久化，manifest hash 语义不变）。

## 1. revision 类别区分（DATA-004 冻结语义）

| 类别 | 键 | 语义 | 来源/绑定 | 版本示例 |
|---|---|---|---|---|
| product | `revision.product` | 产物版本（构建产物标识） | manifest `producer.module_build_id` | `0.11.0-alpha.1-linux-amd64-gcc14` |
| module | `revision.module` | 模块标识/版本 | manifest `producer.module_id` | `astrocs.phase1.frame_hips` |
| ABI | `revision.abi` | C ABI / 文档形态版本 | DATA-001 manifest_schema 形态（v1） | `v1` |
| data schema | `revision.data_schema` | type_id 数据 schema revision | manifest `type_id.schema_version` → `v{sv}` | `v1` |
| doc revision | `doc_revision`（旁路） | manifest 文档形态自身修订 | DATA-004 冻结：当前 `v1`；非当前拒绝 | `v1` |
| history | `history`（旁路） | 旧 product 版本链（被替换版本显式记录） | `build_history`（replaced 升序 + superseded_by） | — |

规则：

- **类别不混用**：provenance digest 按类别分开参与；`revision` 键集合严格 =
  `{product, module, abi, data_schema}`（额外键拒绝）；
- **data schema revision 必须与 manifest 一致**（`assert_revision_is_manifest_data_schema`）：
  `revision.data_schema == "v{schema_version}"`；不一致 = “新数据旧 schema 冒充”
  或“旧数据新 schema 静默接收”，一律拒绝；
- **doc revision 只允许当前值 `v1`**（`assert_doc_revision_is_current`）；DATA-001
  冻结期文档（无 doc_revision）放行——兼容既有 manifest；
- **history 结构**（`astrocs.provenance-history/v1`）：
  `revision_category`（product/module/abi/data_schema）+ `artifact_id` +
  `replaced[]`（`{version, digest:{algorithm,hex}, reason?}`，按版本升序、非空）+
  `superseded_by`（接替者 = 本次发布的当前版本）+ `replaced_at_utc`。

## 2. provenance digest（确定性溯源摘要）

provenance digest = sha256(规范 JSON)，公式输入**只**为溯源事实：

```text
provenance_digest = sha256(canonical_json({
  provenance_schema: "astrocs.provenance/v1", version: 1,
  artifact_id, revision{product,module,abi,data_schema},
  source_commit,                      # 40 hex 源码 commit（调用方/运行图给出；绝不自行猜 git）
  config_digest{algorithm,hex},       # 模块运行配置摘要（= manifest.config_digest）
  provider_digest{algorithm,hex},     # 计算后端 provider（CPU ISA/OS 能力）摘要
  worker_digest{algorithm,hex},       # worker/threading 拓扑摘要
  input_digests[{artifact_id,digest}],# 输入产物 hashes（稳定排序）
  science_ids[SCI-*],                 # 本产物依据的科学合同 ID（稳定排序）
}))
```

性质（DATA-004 验收“同输入配置产生相同 provenance digest”）：

- **确定性**：同输入配置（artifact_id + revision + source_commit + config +
  provider/worker + input hashes + science_ids）⇒ 同 digest；`input_digests` 与
  `science_ids` 在公式内稳定排序，顺序无关；
- **运行事实不参与**：`created_utc` / `run_id` / `phase` / `strategy` 只旁路写
  入 provenance 文档（溯源展示），不进入 digest——否则同输入因时钟/目录不同会
  产生“同输入不同 digest”的假象；
- **旁路 digest 可复算**：`provenance_digest` 字段随文档持久化；`validate_doc`
  复算核对一致（消费门对篡改 digest 硬拒绝）；
- `history` / `doc_revision` 是文档形态与旧链展示，不参与 digest（拒收语义由
  发布门/消费门强制，不靠混淆 digest 实现）。

## 3. 接线（production_store.py DATA-004）

磁盘布局新增（每 run 私有）：

```text
{root}/runs/{run_id}/manifests/{artifact_id}.provenance.json   # provenance sidecar（DATA-004）
```

- `ArtifactStore.with_provenance(source_commit=…, provider_digest=…, worker_digest=…,
  strategy=…, history=…)`：run 启动时注入溯源事实源（链式返回 Store）；source_commit
  必须 40 hex（本 Store 绝不自行调 git/猜 commit）；
- 配置 source_commit 的 Store 每次 `publish` 额外原子发布 provenance sidecar
  （同一原子区：内容 → manifest → hash sidecar → provenance sidecar）；发布前执行
  provenance 语义门（§4）；
- 未配置的 Store（DATA-003 冻结形态）发布行为完全不变——DATA-003 基线测试不受影响；
- 恢复（`start()`）：成功对象基线 = 内容 + COMPLETE manifest + hash sidecar
  （DATA-003 冻结）；provenance sidecar 存在则加载到 `_provenance`（损坏 → 不加载，
  对象仍按基线索引）；DATA-004 产品消费必须走 `bind_product_input`（要求 provenance
  完整 + digest 复算一致 + data_schema 绑定 + 可选 min_product_version）。

## 4. 发布/消费语义门（旧 product 版本不静默接收）

| 门 | 位置 | 规则 | 对应验收 |
|---|---|---|---|
| data_schema 绑定 | publish（`_build_publish_provenance`） | `revision.data_schema` 必须 = manifest `v{schema_version}` | 新/旧 schema 冒充拒 |
| 历史拒收 | publish | revision.product ∈ history.replaced → 硬拒（已替换版本不得静默重发） | 旧 product 版本不静默接收 |
| 隐私门 | publish（`make_provenance_doc`） | 文档任一字符串字段命中敏感模式 → 拒 | privacy scan 不泄露 |
| 消费溯源门 | `bind_product_input` | provenance sidecar 存在 + 校验通过 + digest 复算一致 | 缺 provenance 拒绑定 |
| 版本门槛 | `bind_product_input(min_product_version)` | 输入 product 版本 ≥ 阈值才放行 | 旧 product 版本不静默接收 |

`assert_not_superseded(revision, history, category)` 语义：

- revision 未声明该类别 → 放行（该类别无版本语义）；
- revision 版本 ∈ history.replaced → 显式拒绝（须显式升版本 supersede）；
- history.superseded_by 是接替者（= 当前发布版本），不构成拒收——本函数只拦
  “旧版本回归”，当前版本发布通过。

## 5. privacy scan（不泄露绝对用户路径/凭据）

`provenance.scan_privacy(text)` 对自由文本/诊断做敏感模式扫描（与 LOG-001 redact
模式对齐）：绝对类 Unix 路径（`/home/…`、`/Users/…`、`/tmp/…`）、Windows 盘符
绝对路径、UNC 路径、URL 用户信息、Bearer、形似凭据键值（password/token/secret/
api_key/credential/private_key 等）→ 命中即报告泄露（不静默改写）。`make_provenance_doc`
在生成时对文档全部字符串字段执行结构扫描（`scan_privacy_doc`），命中 → 拒绝发布。
provenance 顶层字段结构上也不携带任何文件系统路径（storage_uri/artifact_id 词法
层由 DATA-001 拒绝裸路径）——绝对用户路径/凭据在溯源通道不出现。

## 6. 验收映射（tasks/03_RUNTIME_DATA_IO_TASKS.md DATA-004）

| 验收 | 实现 | 测试 |
|---|---|---|
| 区分 product/module/ABI/data schema/doc revision/history | §1 + `build_revision`/`build_history`/`assert_doc_revision_is_current` | `TestRevisionCategories` / `TestDocRevisionHistory` |
| 写 source commit/config/provider/worker/input hashes、science IDs | §2 `provenance_digest_hex` + `make_provenance_doc` + `with_provenance` | `TestProvenanceDigestFields` / `TestMakeProvenanceDoc` |
| 同输入配置产生相同 provenance digest | §2 公式（运行事实不参与；排序稳定） | `TestDeterministicDigest` |
| 旧 product 版本不静默接收 | §4 发布门 + 消费门 + data_schema 绑定 | `TestOldVersionNotSilentlyAccepted` |
| privacy scan 不泄露绝对用户路径/凭据 | §5 + `make_provenance_doc` 隐私门 | `TestPrivacyScanNoLeak` |
| 接线：sidecar 原子发布/恢复加载/digest 可复算 | §3 production_store 接线 | `TestStoreProvenanceIntegration` |

测试：`tests/artifact/test_provenance.py`（61 测试，无第三方依赖）；DATA-003 基线
`tests/artifact/test_production_store.py` 30/30 保持通过（未配置 Store 行为不变）。

## 7. 边界（非目标）

- 本任务**不改科学公式/常数**；不改 DATA-001/002 已冻结 schema/registry/
  validator；不改 DATA-003 manifest hash 语义（provenance 以独立 sidecar 旁路，
  不附加 manifest 字段）；
- source_commit 由调用方/运行图显式给出；本模块绝不自行执行 git 或猜测 commit；
- provenance digest 公式按 §2 冻结；未来扩展（新类别/新字段）必须升 provenance
  `version`（当前 1）并同步本文档与执行形态；
- RT-006 trace 溯源字段、LOG-001 脱敏接线、Windows 正式 DLL 同语义 C 复刻属于
  后续任务范围（本层为纯 Python 执行语义，Linux 控制/轻合成节点可完整验证）。

## 8. 文档追溯

`DATA-001/002/003` → `DATA-004 产物溯源（本文档 + provenance.py + production_store 接线）` →
`tests/artifact/test_provenance.py` → `TASK_RESULT.json / TEST_EVIDENCE.json`（DATA-004 证据）。
