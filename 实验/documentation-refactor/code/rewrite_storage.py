"""Preserve storage sources and keep one contract plus implementation detail."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[3]
DOCUMENTS = {
 'docs/engineering/contracts/HIPS_STORAGE_FORM.md': r'''# HiPS 存储形态合同

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
''',
 'docs/detail/infrastructure/storage_form.md': r'''# 存储形态实现与接线

字段、布局、哈希和兼容规则统一由[存储合同](../../engineering/contracts/HIPS_STORAGE_FORM.md)定义；本页连接实际 writer、parser、存储原语与消费者，避免第二套形态定义。产品发布见[原子合同](../../engineering/contracts/ATOMIC_PUBLISH.md)，读取见[AIO](aio.md)。

## 当前组件

lib/infrastructure/cli/parser.cpp 的 session_keys 识别 normalize 的 storage_form 和 mosaic 的 coverage_index，session_commands.h 提供字段说明；键识别并不证明 writer 或 coverage 节点已消费。eng/contracts/ledgers/dead_config_keys.json 仍记录这两项生产接线缺口，该登记是核对线索，完成状态须沿实际生产调用确认。

现有 lib/infrastructure/aio/src/hips/aio_hips_writer.cpp 写产品集与 FITS 裸瓦片；不能从合同默认 archive 推断它已经逐成员编码、生成产品索引或写归档清单。archive 打包、索引生产和随机 reader 必须分别找到真实入口、调用者和可复跑产物后才报告支持，不使用整包临时展开冒充索引随机访问。

Python lib/infrastructure/aio/io/hips_output_store.py 的 tree_hash 实现条目三元组排序/紧凑 JSON 摘要，和 scheduler 的带域前缀规范文件摘要不是相同协议。Python Store、C++ writer 与生产命令分别核对，不把某族 helper 存在解释成三命令全接线。

## 几何与清单

aio_hips_set_geometry(AioHipsProductSet*, const char*) 保存调用者传入的 JSON 字符串，非空时设置标志；发布组装 manifest 时直接写出。setter 本身不解析并证明 WCS 合法，合法 JSON、同帧身份、原点和系数完整性由调用边界检查并独立读取验证。未设置省略键。

产品 geometry、存储引用、运行清单与科学 sidecar 分别持有各自对象，不用朴素全文键搜索代替嵌套 JSON 解析。原源码仍有 provenance/geometry 文本搜索的兼容风险，按当前 reader 核对；本轮未更换 reader 或清单格式。

## 打洞真实路径

aio_sparse_punch.h 的 punch_all_zero_blocks(path,out,verify) 扫描字面零块、探测卷、调用系统打洞并记录前后摘要、大小、分配量与状态。输入路径和结果由调用方持有，verify=true 启用读回复算；原语不拥有产品级发布决定。Windows/Linux 分支分别实测，存在分支不能代替平台行为证据。

aio_hips_writer.cpp 的 publish_finish 顺序为校验、文件同步、打洞/读回、后续改名与目录同步。PUNCH_VERIFY_MISMATCH 删除暂存并失败；当前 PUNCH_NO_RELEASE 也删除并失败，其他非 OK 记警告继续。原 detail 却称 NO_RELEASE 跳过后照常发布，现以当前实现说明，是否符合优化不阻断目标登记 E47，不改代码。尤其 post_hash_failed 等 IO_ERROR 与普通“不支持优化”不能混称已证字节不变，仍需故障注入。

当前 writer 还可异步提交 publish_finish；调用返回与尾部持久化完成不是一个时刻，最终产品发布须等待任务结果，取消与析构也要保持句柄和路径寿命。线程池与环境配置的工程纪律单独核对，不能从本页机制图推断所有权已经正确。

## 目标归档链与验证

目标为私有 staging 写合法内容，编码完整 tar/zstd并同步，生成/核对外置索引与摘要，再发布整组可识别产品。读者先检查形态/索引与身份，查询块候选，再按压缩范围读取单帧和相应文件。每步错误保留原因，不把不可读产品当空天区。

验证保存真实产品、索引和命令，覆盖短读、错误偏移/长度、重复ipix、错误阶、丢成员/终止块、哈希失配、歧义形态、缺索引、发布中断和字节保持。覆盖量化只供调度参考，保守候选责任见 Q29。旧 compress-01 数字与任务步骤在[来源档案](../../../实验/documentation-refactor/docs/STORAGE_SOURCE.txt)按原身份保留；本轮未压缩产品、执行系统打洞或运行 reader。
''',
}

if __name__ == '__main__':
    archive = ROOT / '实验/documentation-refactor/docs/STORAGE_SOURCE.txt'
    if archive.exists():
        raise SystemExit('Archive already exists')
    rev = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT).decode().strip()
    with archive.open('wb') as out:
        out.write(f'Storage source documents; commit={rev}\n'.encode())
        for name in DOCUMENTS:
            raw = (ROOT / name).read_bytes()
            blob = subprocess.check_output(['git', 'hash-object', '--stdin'], cwd=ROOT, input=raw).decode().strip()
            out.write(f'\n===== {name} | blob={blob} | bytes={len(raw)} =====\n'.encode())
            out.write(raw)
            out.write(b'\n===== END SOURCE =====\n')
    for name, body in DOCUMENTS.items():
        (ROOT / name).write_text(body, encoding='utf-8', newline='\n')
