# 产物库

产物库负责工件寻址、暂存、发布、校验读与恢复。科学对象来自[工件合同](ARTIFACTS.md)，HiPS 产品发布见[原子发布](../contracts/ATOMIC_PUBLISH.md)。本页区分目标 Runtime 接线、Python 语义实现与当前 C++ 实际消费者。

## 1. 对象与接口

`lib/infrastructure/aio/runtime/artifact_store/production_store.py` 提供 `ArtifactStore`、`Writer`、`StoreIO`、`ManifestRead` 和 `Digest`：

```text
start → new_writer → stage_bytes → stage_manifest → publish
                                               ↓
                          bind_as_input → read_verified
```

`with_provenance` 注入构建/后端/输入事实；带溯源产品使用 `bind_product_input` 验证。Python 类和 C++ `acsd::core::ArtifactStore` 是不同实现，不能因为名字相同就声称生产 CLI 每次读写都经该 Python Store。

目标接口向模块提供已校验的 reader/writer/handle，真实路径仅在 I/O 层解析；阶段内数值对象还要遵守[Runtime](../contracts/RUNTIME.md)与[所有权](../contracts/OWNERSHIP_LIFETIME.md)。当前各生产节点仍有工作 JSON/FITS 载体，接线差异见[实施地图](../architecture/SCIENCE_IMPLEMENTATION_DELTA.md)。

## 2. Python 实现布局

```text
<root>/runs/<run_id>/stage/
<root>/runs/<run_id>/objects/<artifact_id>
<root>/runs/<run_id>/manifests/<artifact_id>.manifest.json
<root>/runs/<run_id>/manifests/<artifact_id>.manifest.sha256
<root>/runs/<run_id>/manifests/<artifact_id>.provenance.json
```

最后一项在配置溯源时生成。HiPS 目录 `products/<user_path>/` 属另一个发布器的布局，不能与 typed 对象目录混用。artifact_id/type/revision/schema/producer 保持一致；storage_uri 是寻址身份，不由算法模块解释为任意文件路径。

## 3. 写入与读取

Writer 在私有 stage 写入字节并持久化。stage_manifest 检查登记形状、类型、生产者、摘要和大小；publish 重算内容 SHA-256/大小，与声明核对。manifest JSON 的现行序列化是 `ensure_ascii=False`、紧凑分隔、保留 dict 插入顺序；它不是任意键重排后都保持摘要的通用 canonical JSON。

当前 publish 顺序为内容→COMPLETE manifest→manifest 哈希 sidecar→可选 provenance，每个 rename 原子；这一串不是多文件事务。进程在中间停止时可能已有 COMPLETE manifest 而缺后续文件，不能沿用“任一失败必无 COMPLETE 文件”的旧断言。

start 仅把内容、COMPLETE manifest 和 hash sidecar 三者存在的对象纳入基线索引；损坏 provenance 可被忽略而对象仍按基线索引。带溯源消费必须走 bind_product_input。恢复索引不等于全量内容已重算，最终读取仍需 read_verified 检查 type、内容大小与摘要；实际校验范围需沿调用核对。

## 4. 失败、并发与边界

同 ID 唯一生产者；已发布对象不隐式覆盖。取消/释放终止未完成 writer，并清理私有暂存；中途发布残留按真实完成条件处理，不以单个文件名判断成功。多个 run 隔离，同一对象生产由调用方串行化，异步读者不得超过 Store/对象寿命。

StoreIO 的 spy 只证明被测路径经过它，不能证明整个 CLI 没有直接 I/O。Python 实现中 manifest/provenance 的部分打开还直接使用 open；“全部读写均经 StoreIO”需进一步收敛和验证。磁盘写满、中断在每个发布步骤注入，核对残留与恢复；没有实际执行记录就不声明通过。

Windows DLL 与 Linux 运行时是否具有相同发布状态机，必须以各平台生产调用和实验为据，Python 语义层存在不等于它们已接线。旧全文与验收意图保存于[来源档案](../../../实验/documentation-refactor/docs/PRODUCTS_SOURCE.txt)。
