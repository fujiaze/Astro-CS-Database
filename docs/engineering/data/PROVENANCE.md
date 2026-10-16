# 产品溯源

溯源解释产品由哪些输入、配置、算法、构建与执行条件生成。它与内容完整性互相支持，哈希本身不认证来源真实性。运行级字段见[清单合同](../contracts/MANIFEST_VERIFY.md)，typed Store 的旁挂记录由本页定义。

## 1. revision 与身份

| 字段 | 当前 Python 绑定 | 解释边界 |
|---|---|---|
| `revision.product` | producer.module_build_id | 构建/产品版本身份 |
| `revision.module` | producer.module_id | 模块身份；字段名不意味着另有模块版本值 |
| `revision.abi` | 当前由 manifest 文档形态 v1 派生 | 文档形态不能自动证明已核验动态 C ABI |
| `revision.data_schema` | type_id.schema_version → vN | 数据结构修订 |
| `doc_revision` | provenance 文档自身修订 | 与数据 schema、产品版本分开 |
| `history` | replaced 与 superseded_by | 明确的替换链及理由 |

算法模型版本、数据 schema、C ABI、文档版本、构建与输入 occurrence 分别记录。当前 Python 绑定不具备全部维度的独立值时，说明缺口，不据字段存在制造完整版本语义。历史替换关系不能由一个版本大小比较推断。

## 2. 确定性摘要

`lib/infrastructure/aio/runtime/artifact_store/provenance.py` 实现 `acsd.provenance/v1`：

```text
sha256(canonical_json({
  provenance_schema, version, artifact_id,
  revision{product,module,abi,data_schema}, source_commit,
  config_digest, provider_digest, worker_digest,
  input_digests, science_ids
}))
```

输入产物引用和 science_ids 稳定排序。run_id、created_utc、phase、strategy、history、doc_revision 是旁挂事实，不参与现行摘要。因此“同输入配置”必须指上述全部字段相同，不能只指原始帧相同。strategy 目前被排除尤其不能用来证明采用了同一算法。

source_commit 由调用方明确提供，Store 不自行猜 Git；构建源摘要与实际二进制身份见[发行合同](../build/RELEASE.md)。provider/worker 摘要需要对应真实观测或明确声明，不能填占位串。重算成功证明所列字段一致，不证明所列事实真实或实际运行使用了它们。

## 3. Store 接线与校验

`ArtifactStore.with_provenance` 配置后，publish 构造 sidecar，先核对 data_schema 对应、历史替换、隐私，再按实际顺序写入。文档保持 artifact manifest 的严格形状，provenance 单独放 `<artifact_id>.provenance.json`。

publish 的多个文件 rename 并非单一事务；恢复 start 可在缺损 provenance 时保留基线索引。需要溯源的产品必须用 `bind_product_input`，重算 provenance_digest 并核对数据 schema 与版本条件。完整性、来源、可消费性分别判断，不能因对象已索引就认为全部通过。

`assert_not_superseded` 检查指定类别是否在 history.replaced；`min_product_version` 比较另一个约束。当前 parse_version 只抽取数值版本、忽略预发布后缀，不能宣称实现完整 SemVer 次序。替换记录的身份、摘要、类别和当前接替者一起核对。

## 4. 隐私与证据

scan_privacy/scan_privacy_doc 检查绝对用户路径、UNC、URL凭据、Bearer及密钥样式。结构化身份使用受限词法；自由文本先脱敏或拒绝输出。规则匹配有其适用域，不能据“扫描通过”保证任意文本绝无敏感信息。必要原始路径可在用户本地运行记录保存受控映射，公开产品引用稳定身份与相对路径。

验证分别覆盖字段篡改、输入重排、同一摘要不同运行时间、缺 sidecar、错误 schema、预发布版本比较、跨对象替换、隐私和实际消费者。旧测试类名是验收意图，未提供执行器时不当作已执行证据。Python 溯源层与三阶段 C++生产接线分别核验，完整原文见[来源档案](../../../实验/documentation-refactor/docs/PRODUCTS_SOURCE.txt)。
