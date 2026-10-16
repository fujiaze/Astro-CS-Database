# HiPS 存储形态合同

本合同说明[最高设计](../../ACSD_DESIGN.md)的裸目录与归档形态、索引、身份和兼容要求；实现入口与现状见[存储细节](../../detail/infrastructure/storage_form.md)。科学内容与有效值由[数据语义](../../science/unified/DATA_SEMANTICS.md)定义，发布由[原子发布合同](ATOMIC_PUBLISH.md)约束。schema 表达载体形状，不能反向拥有科学定义。

## 形态、内容与能力

| 形态 | 落盘名 | 要求 |
|---|---|---|
| bare | name.hips 目录 | 直接呈现产品树 |
| archive | name.hips.zst 常规文件 | 解压得到同一产品树；外置产品级索引支持随机读 |

同名两形态互斥，共存报歧义。normalize 的目标配置 storage_form 为 archive（缺省）或 bare；缺失、null、空串按既有合同取 archive 并点名警告，form_source 记录 default/config。mosaic 输出固定裸 HiPS，export 输出普通二维 FITS，二者配置不接受 storage_form。目标默认不代表当前 writer 已支持归档：CLI 识别、落盘、索引生产与读取分别验证，不允许无效键静默成功。

产品树包含实际科学通道、模型、元数据与产品清单。normalize 的辅助数据不改成 mosaic 的最终额外科学通道。frame_science.json 是唯一正式科学模型载体，Catalogue/兼容视图不能替代它；旧文 snr/ TSV 的硬性主载体措辞不推翻现行合同。HiPS properties 按实际子产品声明 Image fits 或已定义 Catalogue tsv，不能把容器压缩写成 hips_tile_format=zstd/fits.zst。

## 归档与随机访问

归档解压为完整 tar 流，成员路径相对产品根，无前导 ./、绝对路径、逃逸、符号链接或外部依赖。包括产品内全部合法常规文件，外置产品/数据集索引不放入内容树。成员按路径字典序，tar 头的时间、身份与模式采用声明的确定值。

目标编码为独立标准 zstd 帧串接，每个 tar 成员连同其必要头/填充位于可定位帧；还须明确 tar 终止块的归属，不能遗漏。标准工具解压得到与打包前逐字节相同的合法 tar 流，不采用需额外反变换的 byte-shuffle 或混装 skippable 数据。默认压缩档位保留 level 3，读端按约定默认解压能力执行，压缩档位不改变内容身份。

随机读依产品级索引取得压缩范围，读取并解压该帧，再取成员文件字节；不整包展开到磁盘。一次逻辑读取不能承诺底层系统调用恰好一次，短读、重试与校验仍需完整处理。打包前 FITS 合法不证明归档过程无损，压缩后仍须验证还原与随机定位。

## 产品级索引

同父目录 name.hips.index.json 为 UTF-8 JSON，字段形状见 eng/contracts/schemas/hips_storage_form.schema.json。archive 必须有索引；bare 写侧目标也生成索引，兼容读侧可在缺失时枚举目录并记录来源/降级。

| 字段 | 语义与不变式 |
|---|---|
| index_schema、product、storage_form | acsd.hips-index/v1、产品名、实际形态 |
| archive | archive 时含 name/bytes/sha256/frame_unit=tar_member；bare 为 null |
| subproducts | name 唯一，hips_order/tile_width/format 与实际 properties 相同 |
| coverage | encoding=runs；leaf_ipix_runs 为升序、不重叠、不相邻的 start/count 段；frac_quant=255，leaf_frac 对应展开顺序 |
| n_leaf_tiles | 展开叶 tile 数，与实际文件集合一致 |
| tiles | archive 的 ipix/coff/csize/usize/doff/dsize；bare 可省略 |

coff/csize 是容器压缩字节偏移/长度，doff/dsize 是解压帧内文件偏移/长度；范围非负、不溢出且落在对应字节域，doff+dsize≤usize。tiles 的 ipix 集合、coverage 展开集合与实际叶 tile 集合完全一致，拒绝重复与错阶。索引可由内容与帧表重算，摘要一致不替代这些不变式。

## 数据集覆盖索引

coverage.index.json 的 index_schema=acsd.coverage-index/v1；granularity 记录 hips_leaf_tile、tile_width 和 hips_order；frames 是唯一升序帧标识；blocks 按唯一 ipix 排序，其中 frames[] 的 f 引用已登记帧，frac 为0..255。它是产品级索引的派生倒排，用于块级候选和调度，不能代替实际像素有效性。

缺数据集索引时目标回退是从产品级索引重建，产品级索引也缺失的 bare 则按其兼容规则枚举。索引存在不等于瓦片有效；缺 tile、损坏和错误单位分别报告。低阶层级不冒充叶级覆盖。覆盖比例与量化剪枝的科学域见[Q29](../../../实验/documentation-refactor/docs/SCIENTIFIC_QUESTIONS.md)，不由调度索引发明像素权重或裁掉合法边界。

当前来源文件对 coverage.index.json 的生产者存在冲突：normalize 输出示例要求逐批索引，另一段却指定 mosaic 发布才生产。生产者必须与输入数据集身份和消费时序一致；本轮登记 E46，不凭正文宣布已接线。跨批合并保留逐帧产品/索引身份，数据集索引重建，不直接拼接不同运行的偏移。

## 哈希与清单

tree_hash 对解压内容的条目三元组 [path,size,sha256] 排序后计算紧凑 UTF-8 JSON 的 SHA-256；排序键为三元组字典序，ensure_ascii=false，分隔符为逗号和冒号。条目成员集合须明确排除自引用摘要，归档参数不参与。相同内容两形态具有相同 tree_hash，archive_sha256 只认证容器字节，index_sha256 只认证索引字节。内容身份与来源真实性是两件事。

逐帧产品清单目标登记 storage_form、index_path、index_sha256、archive_sha256；bare 的 archive_sha256=null。外部运行清单 storage 段保存 form_source、products[] 的 product/形态/索引/容器字节数与哈希/tree_hash，以及 coverage_index 引用；bare 的 archive_bytes 也为 null。coverage_index 在运行清单是 path/sha256/n_frames/n_blocks 对象，在 mosaic 输入是可选路径字符串，不能混读定义。完整字段见 schema 的 frame_storage、manifest_storage、coverage_index_ref 与阶段配置 schema。

容器摘要必须放在容器之外，不能写入被它自己覆盖的内容。索引和内容多文件发布不因连续 rename 就成为事务，恢复/验证应按整组完整性判断。已可见但持久化未确认的状态不能伪装为从未发布。实现状态和发布顺序见原子发布合同与 detail。

## 产品几何

逐帧产品 manifest 的可选 geometry 保存同帧 WCS 来源的 ctype1/2、crval1/2（deg）、crpix1/2（内部零基中心）、CD（deg/pixel）、pixel_origin=0 和 SIP 系数/阶；无 SIP 按合同 null。FITS 一基参考像元只在转换边界加一。系数布局以实际结构为准，不把某轮数组长度36定为所有阶的通则。

未设置 geometry 时省略，不制造几何；在场时应与相同帧的实际解一致。加性键的兼容性需验证真实 reader，不能由“可选”推导所有旧解析器都会接受。geometry 与 frame_science、产品和运行清单作用域不同，不能凭同名 manifest.json 或路径前缀互相冒充。

## 裸形态体积优化

T1 文件系统打洞仅作用字面全零的4 KiB对齐块，保持逻辑长度与全部字节；EOF末块的文件内字节也必须全零。负零和 NaN 位型不可按浮点等于零判定。写满/同步后、摘要与发布前执行，比较前后哈希、逻辑长度、跨洞读取、FITS校验与实际分配字节。卷不支持时保留完整文件并记录；读回不一致是数据错误。未释放空间不能报告优化成功，当前 writer 对此直接失败的行为另见 detail，不据本轮文档私改代码。

T2 包围盒 TRIM 改变 FITS 结构和内容身份，默认关闭，只能在显式声明且下游 TRIM-aware 时使用；记录 TRIM1/TRIM2/ONAXIS1/ONAXIS2，补边位型与位置须对应原对象。读端不支持则拒绝，不能把缩小 NAXIS 静默当原瓦片。它属于待核标准/生态的可选扩展，不把草案当稳定标准。归档不采用上述两种额外优化；两机制收益分开，不能由 TRIM 压缩比证明打洞释放量。

## 验证与来源

核对两形态互斥、容器/索引/内容身份、随机读取、重算覆盖、损坏/截断/缺索引、跨批引用、写盘/取消/恢复、负零/NaN 与不支持卷。能力存在、生产可达、真实跨平台执行和独立读器验证分别报告，schema 通过不等于已完成生产支持。

完整旧字段表、历史裁决、实测引用和参考资料保存于[存储来源](../../../实验/documentation-refactor/docs/STORAGE_SOURCE.txt)。原引用包括 IVOA HiPS 1.0、FITS 4.0、RFC8878、HEALPix 论文与 HiPS 工作草案，本轮未重新外部核验标准条款。来源中的强断言与未来实现不能据归档提升为已验证事实。
