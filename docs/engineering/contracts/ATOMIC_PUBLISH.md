# 原子发布

本合同定义正式产品的可见性、完整性与持久化；[阶段交换](../data/PHASE_PRODUCT_EXCHANGE.md)定义消费者，[产物库](../data/ARTIFACT_STORE.md)定义 typed 对象。单文件原子 rename、整棵目录发布和多文件完成协议是不同机制，不能统称一次不可分事务。

## 1. 发布协议

1. 在本运行私有 stage 生成内容，确认输入、目标路径、类型与覆盖政策。
2. 完成写入和关闭/flush/fsync；按格式核对结构、校验和、单位、几何及必要引用。
3. 计算实际内容哈希与大小，等待所有 AIO 任务完成。失败不能把未完成对象列入产品。
4. 同文件系统提升文件或整目录；需要多个文件的产品保留明确的完成协议。
5. 最后发布消费所需完整清单，说明真实文件、模型、哈希、能力和来源。

完成表示产品按声明完整，precision 可以显式 unavailable；它不表示模型已经独立科学验证。兼容外部 HiPS 可按输入合同读取信号，不要求它由 ACSD 的某个 Python 发布器生成。

默认不覆盖已完成产品。显式覆盖时先完成新内容暂存，旧产品保持可恢复副本，成功后再回收；失败报告实际目标和恢复状态。路径来自显式输出配置，输入只读，产品不落源码或进程 CWD 的隐式目录。

## 2. C++ 机制接口

`lib/infrastructure/aio/product_io/include/astro/aio/atomic_publish.h` 提供 `atomic_publish_file`、`atomic_write_bytes`、`atomic_publish_directory` 和 `PublishResult`。实际机制先写 tmp、rename，再按 VerifyFn 重开目标验证；可能在验证前短暂可见，验证失败按选项撤销。需要完成清单的消费者不能只凭目标路径出现就读取。

生产者可在 rename 前做内容验证，机制层可在 rename 后重开验证；二者针对不同窗口，并不构成无条件强弱排序。rename 后外部修改、验证后的再次修改、目录持久化失败与覆盖旧目标的风险分别核对。单纯“重开校验通过”不能证明之后永不损坏。

| `PublishDurability` | 可见性与调用责任 |
|---|---|
| `kNotPublished` | 本次目标未发布或已撤销；按状态清理暂存与决定重试 |
| `kDurable` | 已发布且目录项持久化已确认 |
| `kNotDurable` | 目标已可见，持久化尚无证据；明确记录并重试确认，不直接删除回滚或静默当确认成功 |

durability 与 PublishStatus 正交，renamed、target_checked、target_present 说明实际观测。不同平台目录持久化能力不同，status=OK 也不能单独推出 kDurable。调用方不能把“所有错误必无正式产品”作为清理规则。

## 3. Python HiPS 发布器

`lib/infrastructure/aio/io/hips_output_store.py::HipsOutputStore` 使用：

```text
<root>/runs/<run_id>/stage/
<root>/runs/<run_id>/products/<user_path>/manifest.json
```

publish_directory 校验相对路径与允许文件名，在 stage 写完并核对每个图像 tile，再让旧目标移至 `.replaced-*`，逐文件换入新目录并最后写 COMPLETE manifest；失败尝试恢复旧副本，恢复失败保留并报告副本位置。这是完成清单协议，非一次整树 rename。

允许 properties、Moc.fits、规定的 Norder/Dir/Npix FITS 和数据集级 coverage.index.json。metadata、frame_science 与模型文件等正式产品组成由各产品合同和实际发布器说明，不能套用这份局部文件白名单拒绝所有其他科学元数据。

路径词法拒绝绝对路径、父目录、空段、反斜杠与不安全字符，并检查已有符号链接组件。权限模式是具体平台实现；路径检查与实际打开之间的竞态仍需专门验证，不能凭词法检查宣称全部文件系统逃逸已排除。同 run 同目标的发布由调用方串行化。

## 4. 清单、tree hash 与存储形态

Python 产品清单为 `acsd.hips-output-manifest/v1`，包含 manifest_version、COMPLETE、run_id、user_path、product、publisher、tree、tree_hash、fitsverify、created_utc 和可选 producer。tree 每项为 path/size/sha256；tree_hash 对这些项归一为三元组，按(path,size,sha256)排序，以 UTF-8 紧凑 JSON 后取 SHA-256。

fitsverify 只记录实际检查范围：图像 tile 的结构和所用校验卡，Moc.fits 的 BINTABLE 不在该图像验证器支持域内。未检 tile 不计入 tile_count；没有 DATASUM 不能写已通过 DATASUM。结构和校验和不证明天区、科学单位或像元映射正确。

裸 `.hips/` 与 `.hips.zst` 的互斥形态、索引、容器哈希和逻辑产品 tree_hash 见[存储形态](HIPS_STORAGE_FORM.md)。归档容器完整性与解压后内容等价分别验证。运行级 storage 元数据和产品级 tree 清单分别保存，不能互相替代。

## 5. 当前边界与验证

typed Python Store 的内容/manifest/hash/provenance 是另一多文件顺序；C++ CLI、Mosaic writer、Normalize signal+sidecar 是否共享事务，分别按生产调用登记，见[实施差异](../architecture/SCIENCE_IMPLEMENTATION_DELTA.md)。库接口存在不能推断所有命令都符合目标发布协议。

故障实验覆盖写满、部分写、取消、每个 rename 前后、验证失败、目录 fsync失败、覆盖恢复、并发同目标及读者过早消费，保留失败尝试。逐文件原子性、整产品完成性、持久化、读后完整性和科学正确性分别报告。旧原文、历史测试名与未接线注入保存于[来源档案](../../../实验/documentation-refactor/docs/PRODUCTS_SOURCE.txt)，本轮未执行发布实验。
