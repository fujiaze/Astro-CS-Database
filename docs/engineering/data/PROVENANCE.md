# 产品溯源

上游：最高设计的 I/O 与原子产品一章；运行清单的溯源子对象见 `../contracts/MANIFEST_VERIFY.md`。

产品溯源记录的字段、写入者与不可伪造约束的正本。

## 目的与范围

执行形态 = `lib/infrastructure/aio/runtime/artifact_store/provenance.py`（provenance 层与校验器）+
`lib/infrastructure/aio/runtime/artifact_store/production_store.py`（provenance sidecar / 版本门 / 消费门接线）；
验收测试 = `eng/tests/artifact/test_provenance.py`；typed manifest 机器形态 =
`eng/contracts/data/artifact_manifest.schema.json`。下游接线：`../contracts/ATOMIC_PUBLISH.md`
（原子 HiPS/manifest 输出复用 provenance sidecar 语义）、`../architecture/ARCHITECTURE.md`（phase-isolated runtime
消费门接线）、`../resources/observability/STRUCTURED_LOGGING.md`（脱敏语义对齐）。

`ARTIFACT_STORE.md` 定义生产 ArtifactStore 的原子发布与校验读；本合同在其上
定义**产物溯源（provenance）与版本语义**：

1. 区分 **revision 类别**：product / module / ABI / data schema / doc revision /
 history——每类有独立语义与校验规则，各自具名、各按本类规则校验；
2. provenance 写 **source commit / config / provider / worker / input hashes、
 science IDs**；
3. **确定性 provenance digest**：同输入配置 ⇒ 同 digest（运行时间/目录等运行
 事实不参与 digest）；
4. **被替换的 product 版本不静默接收**：发布门（history 已替换版本拒绝重发）+
 消费门（min_product_version 门槛）+ data_schema 绑定（新数据旧 schema /
 旧数据新 schema 一律拒绝）；
5. **privacy scan**：provenance 相关文本/诊断不泄露绝对用户路径与凭据。

约束来源：`../../ACSD_DESIGN.md` 「命名块内存管线与块生命周期」一节（阶段内命名块内存管线）、（阶段间只通过原子发布、哈希
和 provenance 完整的磁盘产品/manifest 交换）；`PHASE_PRODUCT_EXCHANGE.md`
的 `R-EVIDENCE-REQUIRED`（缺 manifest / 缺 hash / 缺 schema / 缺 units → 拒绝）；
`ARTIFACT_STORE.md` 接线冻结语义（发布物保持严格
`eng/contracts/data/artifact_manifest.schema.json` 形态，不附加字段——provenance 以独立
sidecar 旁路持久化，manifest hash 语义不变）。

## revision 类别区分（冻结语义）

| 类别 | 键 | 语义 | 来源/绑定 | 版本示例 |
|---|---|---|---|---|
| product | `revision.product` | 产物版本（构建产物标识） | manifest `producer.module_build_id` | `0.1.0-alpha.1-linux-amd64-gcc14` |
| module | `revision.module` | 模块标识/版本 | manifest `producer.module_id` | `acsd.phase1.frame_hips` |
| ABI | `revision.abi` | C ABI / 文档形态版本 | `eng/contracts/data/artifact_manifest.schema.json` 形态（v1） | `v1` |
| data schema | `revision.data_schema` | type_id 数据 schema revision | manifest `type_id.schema_version` → `v{sv}` | `v1` |
| doc revision | `doc_revision`（旁路） | manifest 文档形态自身修订 | 当前 `v1`；非当前拒绝 | `v1` |
| history | `history`（旁路） | 被替换的 product 版本链（被替换版本显式记录） | `build_history`（replaced 升序 + 接替者条目） | — |

规则：

- **类别不混用**：provenance digest 按类别分开参与；`revision` 键集合严格 =
 `{product, module, abi, data_schema}`（额外键拒绝）；
- **data schema revision 必须与 manifest 一致**（`assert_revision_is_manifest_data_schema`）：
 `revision.data_schema == "v{schema_version}"`；不一致 = “新数据旧 schema 冒充”
 或“旧数据新 schema 静默接收”，一律拒绝；
- **doc revision 只允许当前值 `v1`**（`assert_doc_revision_is_current`）；无 `doc_revision`
 字段的 manifest（字段集按 `eng/contracts/data/artifact_manifest.schema.json`）放行；
- **history 结构**（`acsd.provenance-history/v1`）：
 `revision_category`（product/module/abi/data_schema）+ `artifact_id` +
 `replaced[]`（`{version, digest:{algorithm,hex}, reason?}`，按版本升序、非空）+
 `superseded_by`（接替者 = 本次发布的当前版本）+ `replaced_at_utc`。

## provenance digest（确定性溯源摘要）

provenance digest = sha256(规范 JSON)，公式输入**只**为溯源事实：

```text
provenance_digest = sha256(canonical_json({
 provenance_schema: "acsd.provenance/v1", version: 1,
 artifact_id, revision{product,module,abi,data_schema},
 source_commit, # 40 hex 源码 commit（调用方/运行图给出；绝不自行猜 git）
 config_digest{algorithm,hex}, # 模块运行配置摘要（= manifest.config_digest）
 provider_digest{algorithm,hex}, # 计算后端 provider（CPU ISA/OS 能力）摘要
 worker_digest{algorithm,hex}, # worker/threading 拓扑摘要
 input_digests[{artifact_id,digest}],# 输入产物 hashes（稳定排序）
 science_ids[SCI-*], # 本产物依据的科学合同 ID（稳定排序）
}))
```

性质（验收：同输入配置产生相同 provenance digest）：

- **确定性**：同输入配置（artifact_id + revision + source_commit + config +
 provider/worker + input hashes + science_ids）⇒ 同 digest；`input_digests` 与
 `science_ids` 在公式内稳定排序，顺序无关；
- **运行事实不参与**：`created_utc` / `run_id` / `phase` / `strategy` 只旁路写
 入 provenance 文档（溯源展示），不进入 digest——否则同输入因时钟/目录不同会
 产生“同输入不同 digest”的假象；
- **旁路 digest 可复算**：`provenance_digest` 字段随文档持久化；`validate_doc`
 复算核对一致（消费门对篡改 digest 硬拒绝）；
- `history` / `doc_revision` 是文档形态与替换链展示，不参与 digest（拒收语义由
 发布门/消费门强制，不靠混淆 digest 实现）。

## 接线（production_store.py）

磁盘布局新增（每 run 私有）：

```text
{root}/runs/{run_id}/manifests/{artifact_id}.provenance.json # provenance sidecar
```

- `ArtifactStore.with_provenance(source_commit=…, provider_digest=…, worker_digest=…,
 strategy=…, history=…)`：run 启动时注入溯源事实源（链式返回 Store）；source_commit
 必须 40 hex（本 Store 绝不自行调 git/猜 commit）；
- 配置 source_commit 的 Store 每次 `publish` 额外原子发布 provenance sidecar
 （同一原子区：内容 → manifest → hash sidecar → provenance sidecar）；发布前执行
 provenance 语义门（「发布/消费语义门」一节）；
- 未配置溯源事实源的 Store 发布行为完全不变——`eng/tests/artifact/test_production_store.py` 基线不受影响；
- 恢复（`start`）：成功对象基线 = 内容 + COMPLETE manifest + hash sidecar
 （`ARTIFACT_STORE.md` 冻结形态）；provenance sidecar 存在则加载到
 `_provenance`（损坏 → 不加载，对象仍按基线索引）；带 provenance 的产品消费必须走 `bind_product_input`（要求 provenance
 完整 + digest 复算一致 + data_schema 绑定 + 可选 min_product_version）。

## 发布/消费语义门（被替换的 product 版本不静默接收）

| 门 | 位置 | 规则 | 对应验收 |
|---|---|---|---|
| data_schema 绑定 | publish（`_build_publish_provenance`） | `revision.data_schema` 必须 = manifest `v{schema_version}` | 新/旧 schema 冒充拒 |
| 历史拒收 | publish | revision.product ∈ history.replaced → 硬拒（重发只经显式迁移路径） | 被替换版本不静默接收 |
| 隐私门 | publish（`make_provenance_doc`） | 文档任一字符串字段命中敏感模式 → 拒 | privacy scan 不泄露 |
| 消费溯源门 | `bind_product_input` | provenance sidecar 存在 + 校验通过 + digest 复算一致 | 缺 provenance 拒绑定 |
| 版本门槛 | `bind_product_input(min_product_version)` | 输入 product 版本 ≥ 阈值才放行 | 被替换版本不静默接收 |

`assert_not_superseded(revision, history, category)` 语义：

- revision 未声明该类别 → 放行（该类别无版本语义）；
- revision 版本 ∈ history.replaced → 显式拒绝（须显式升版本 supersede）；
- history 的接替者条目是当前发布版本，不构成拒收——本函数只拦
 「被替换版本再次发布」，当前版本发布通过。

## privacy scan（不泄露绝对用户路径/凭据）

`provenance.scan_privacy(text)` 对自由文本/诊断做敏感模式扫描（与
`../resources/observability/STRUCTURED_LOGGING.md` 的 redact 模式对齐）：绝对类 Unix 路径（`/home/…`、`/Users/…`、`/tmp/…`）、Windows 盘符
绝对路径、UNC 路径、URL 用户信息、Bearer、形似凭据键值（password/token/secret/
api_key/credential/private_key 等）→ 命中即报告泄露（不静默改写）。`make_provenance_doc`
在生成时对文档全部字符串字段执行结构扫描（`scan_privacy_doc`），命中 → 拒绝发布。
provenance 顶层字段结构上也不携带任何文件系统路径（storage_uri/artifact_id 词法
层按 `eng/contracts/data/artifact_manifest.schema.json` 拒绝裸路径）——绝对用户路径/凭据在溯源通道不出现。

## 验收映射（依据：本文件 + `../../ACSD_DESIGN.md` 「I/O 与原子产品」一节）

| 验收 | 实现 | 测试 |
|---|---|---|
| 区分 product/module/ABI/data schema/doc revision/history | 「revision 类别区分」一节 + `build_revision`/`build_history`/`assert_doc_revision_is_current` | `TestRevisionCategories` / `TestDocRevisionHistory` |
| 写 source commit/config/provider/worker/input hashes、science IDs | 「provenance digest」一节 `provenance_digest_hex` + `make_provenance_doc` + `with_provenance` | `TestProvenanceDigestFields` / `TestMakeProvenanceDoc` |
| 同输入配置产生相同 provenance digest | 「provenance digest」一节 公式（运行事实不参与；排序稳定） | `TestDeterministicDigest` |
| 被替换的 product 版本不静默接收 | 「发布/消费语义门」一节 发布门 + 消费门 + data_schema 绑定 | `TestOldVersionNotSilentlyAccepted` |
| privacy scan 不泄露绝对用户路径/凭据 | 「privacy scan」一节 + `make_provenance_doc` 隐私门 | `TestPrivacyScanNoLeak` |
| 接线：sidecar 原子发布/恢复加载/digest 可复算 | 「接线」一节 production_store 接线 | `TestStoreProvenanceIntegration` |

测试：`eng/tests/artifact/test_provenance.py`（正/负例，无第三方依赖；
`python3 -m pytest eng/tests/artifact/test_provenance.py`）；`eng/tests/artifact/test_production_store.py`
基线保持通过（未配置溯源事实源的 Store 行为不变）。

## 边界（非目标）

- 本合同**不改科学公式/常数**；不改 `eng/contracts/data/` 与三阶段产品交换合同已冻结的
 schema/registry/validator；不改生产 ArtifactStore 的 manifest hash 语义（provenance 以独立
 sidecar 旁路，不附加 manifest 字段）；
- source_commit 由调用方/运行图显式给出；本模块绝不自行执行 git 或猜测 commit；
- provenance digest 公式按 「provenance digest」一节 冻结；未来扩展（新类别/新字段）必须升 provenance
 `version`（当前 1）并同步本文档与执行形态；
- trace 溯源字段与脱敏接线属 `../resources/observability/STRUCTURED_LOGGING.md`
 与 `../../detail/LOG_AND_ERROR_SYSTEM.md` 的范围（本层为纯 Python 执行语义，Linux 控制/
 轻合成节点可完整验证）。

## 文档追溯

`eng/contracts/data/artifact_manifest.schema.json` / `PHASE_PRODUCT_EXCHANGE.md` /
`ARTIFACT_STORE.md` → 本溯源合同（本文档 + provenance.py +
production_store 接线） → `eng/tests/artifact/test_provenance.py`。

## 参考文献

[1] 内部文档 `docs/ACSD_DESIGN.md，最高设计`，上位来源。
[2] 内部文档 `docs/engineering/contracts/MANIFEST_VERIFY.md`，同层相关正本。
[3] 内部文档 `docs/engineering/data/ARTIFACT_STORE.md`，同层相关正本。
