"""Archive reviewed I/O contracts and explain their separate API layers."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[3]
DOCUMENTS = {
 'docs/engineering/contracts/FITS_IO.md': r'''# FITS 读写合同

本合同规定FITS承载、缩放、句柄、分块和错误。科学单位由[数据语义](../../science/unified/DATA_SEMANTICS.md)定义，发布由[原子发布](ATOMIC_PUBLISH.md)定义，具体入口见[AIO](../../detail/infrastructure/aio.md)。

## 1. 区分三个接口层

| 入口 | 实际对象与责任 |
|---|---|
| `aio_fits.cpp::fits_read_file` | 生产图像读取，native/CFITSIO分支给物理数值；BSCALE/BZERO在读取中施加 |
| `io/include/acsd/io/fits_stream_v1.h` | 独立版本化C流式接口，头/平面/chunk及opaque reader/writer，调用方持输出缓冲 |
| `product_io/include/astro/aio/fits.h` | 内部C++ FitsStreamWriter、FitsHduSpec、ExpectedHdu与独立重开验证 |

三者都属于AIO，但支持域、返回类型和数值转换不同，不能写成一个已统一的纯C接口。第三方类型不跨公开C ABI，内部C++接口可使用标准库。

## 2. 数据与缩放

数组行主序[H,W]，NAXIS1=W为最快轴、NAXIS2=H；多平面接口另声明plane_index。BITPIX与缓冲类型、容量、字节序分别核对。读头保留真实BUNIT和WCS；不能凭默认ADU覆盖未知单位。

生产fits_read_file给出的物理值已按`BSCALE×stored+BZERO`解码，消费方不得重复施加。底层流式合同保留缩放卡并提供其声明的存储值读面，调用者若从该面进入必须明确转换责任；不能把“不自动缩放”的低层约定推广到全部AIO。整数BLANK、NaN和无覆盖的映射按具体读取器支持说明，损坏头不补默认物理值。

v1流式头支持常见整数/浮点BITPIX、最多三维、最多1024卡片；表、随机群和压缩扩展不属于该面完整支持域。其他生产读取分支是否支持压缩由实际后端核对，不能以低层限制否定CFITSIO全部能力。接口上限是项目选择，不是FITS标准普遍上限。

## 3. 读写生命周期

读为open→get_header→read_plane/read_chunk→close；同句柄串行，不同句柄可并行。输出缓冲由调用方分配，容量单位是元素数；失败可能已经部分写入，以out_got和返回码判断，不继续消费未完成尾部。读取自身不证明产品几何科学正确。

写入由调用方声明shape、BITPIX、BUNIT和用户卡；保留结构卡由writer生成，冲突明确拒绝。在同文件系统私有tmp写完内容、补齐FITS块、写实际校验卡，按发布机制持久化/提升/重开验证。不能承诺所有失败都保持原目标，因为底层机制有rename后验证及持久化未确认状态。

内部FitsStreamWriter借用fd，由外层持有文件与发布生命周期；write_data不可超出该HDU数据量，end_hdu完成对齐及DATASUM/CHECKSUM。跨块写入的校验必须按字节流连续累计。Header/DATASUM/CHECKSUM是否存在、是否执行及实际通过范围分别记录。

## 4. 验证、错误和观测

依次检查参数/ABI、头结构、尺寸与长度、期望shape/BUNIT/卡片、校验和及调用策略。科学图像合法NaN与strict_nan策略拒绝分别解释；结构验证不默认全扫科学有效性。ExpectedHdu为空的内部验证只说明结构与校验自洽，不证明WCS/单位与请求一致。

错误区分参数、ABI、内存、I/O、unsupported、取消、句柄状态、截断、坏头、失配、checksum、数值策略和磁盘写满；C状态按实际头解释，CLI退出映射见[错误合同](LOG_AND_ERROR.md)。计数hooks以实际底层read/write字节记账，不能把它直接当物理磁盘读写量或产品最终大小。

验证应覆盖缩放一次、整数/浮点、BLANK/NaN、分块边界、超过2GiB、长度溢出、取消、部分读写、磁盘故障和独立读取器。旧正文“全部负测已有覆盖”没有本轮命令与结果支持，原文保存在[来源档案](../../../实验/documentation-refactor/docs/IO_SOURCE.txt)，不冒认执行。
''',
 'docs/engineering/contracts/HIPS_IO.md': r'''# HiPS 输入读取

本合同说明磁盘HiPS的属性、tile寻址、缺块、类型与读取边界。几何和单位由[HiPS科学分册](../../science/formats/HIPS_TILE_FORMAT.md)定义，目标存储形态见[存储合同](HIPS_STORAGE_FORM.md)，实际调用见[AIO细节](../../detail/infrastructure/aio.md)。

## 1. 实际入口与支持域

`lib/infrastructure/aio/include/aio_hips.h`及其reader是三阶段和浏览器的实际读取面；`io/include/acsd/io/hips_input_v1.h`另提供版本化C接口，复用fits_stream_v1。声明接口和生产调用分别核对，不把所有reader当成一个实现。

项目科学输入使用NESTED、天球参考系、float FITS信号。低层可读取的support/variance/ivar是文件能力，当前Mosaic最终只发布signal；其他reader支持范围和阶段产品集合分别说明。整数/JPEG/PNG或其他类型是否接受按具体科学调用者处理，不由格式词表存在自动授权。

## 2. 属性与地址

读取properties的dataproduct_type、hips_order、hips_tile_width、hips_frame、hips_tile_format及必要单位/来源，核对该reader的域和实际tile。属性别名按显式兼容规则解释，未知值明确返回不支持。

tile地址为`NorderK/DirD/NpixN.fits`，标准块目录按完整ipix的万进制起点；既有商余命名兼容只能在具名读分支使用。检查order、ipix整数容量、tile宽和shape，再读数据；不能仅凭文件名判断天区、叶阶或坐标手性。512宽tile的叶阶与tile阶差9，使用共享NESTED/FITS映射。

## 3. 状态与缺覆盖

tile定位分PRESENT、MISSING、INVALID；MISSING表示该请求层没有块，不能静默返回父层内容冒充。上层如有显式选择其他层级的算法，记录真实层级与算子，和底层缺块错误分开。

局部树允许缺子树；无覆盖signal按NaN/明确状态，不能填0。tile内合法非有限表示、数据损坏和读取错误按接口策略区分。Moc.fits可作可选枚举提示，损坏提示不应制造伪覆盖；查询候选和实际有效像元不同。

## 4. 句柄与缓冲

open解析产品/子产品并校验属性，调用方分配输出缓冲，使用完close。单句柄串行，不同worker各有reader或在读层串行化共享句柄。dtype转换、缓冲容量、部分读和取消采用对应FITS接口合同，不能把旧整数码按CLI退出码直接解释。

读取后保持真实单位、scale_state、运行/frame身份及必要模型引用。兼容外部HiPS可按信号域读取，缺canonical precision明确unavailable，不要求所有输入由ACSD某个Store生成。

## 5. 裸形态与归档目标

设计支持裸`.hips/`与带产品索引的`.hips.zst`统一读语义：归档按成员索引定位独立压缩帧，查询覆盖不解压整包。归档索引缺失或损坏须明确报错，裸目录的枚举回落另记来源。

当前生产归档写/读、产品索引和storage事实接线未由该接口声明证明，实际裸读路径与未实现目标分开，见[实施差异](../architecture/SCIENCE_IMPLEMENTATION_DELTA.md)。不能让未实现默认archive的文档描述使用户以为已可读取。

验证properties非法、层级/宽度、地址越界、缺块、父回退、手性、dtype、取消、归档索引及兼容外部产品。标准符合性、当前代码支持与真实运行分别记录；原文保存于[来源档案](../../../实验/documentation-refactor/docs/IO_SOURCE.txt)。
''',
 'docs/detail/infrastructure/aio.md': r'''# 统一 AIO

AIO提供正式图像/模型/清单的读写边界。科学库消费声明对象，不直接开文件；[FITS](../../engineering/contracts/FITS_IO.md)、[HiPS](../../engineering/contracts/HIPS_IO.md)、[原子发布](../../engineering/contracts/ATOMIC_PUBLISH.md)和[存储形态](../../engineering/contracts/HIPS_STORAGE_FORM.md)分别拥有工程语义。

## 1. 接口层与消费者

| 位置 | 实际责任 |
|---|---|
| `aio/include` 与 `aio/src` | AIOImageData、PipelineFrame、FITS/XISF、HiPS reader/writer、UPM、压缩和系统I/O；由CLI节点及浏览器调用 |
| `lib/include/acsd/io/aio_abi_v1.h` | 版本化C ABI握手、状态域及内容哈希复核；不表示所有读写已经统一到该面 |
| `aio/io/include/acsd/io` | FITS流式与HiPS输入C接口，opaque句柄、POD和调用方缓冲 |
| `aio/product_io/include/astro/aio` | 内部C++发布、FITS、单位、清单和provenance工具 |
| `aio/runtime/artifact_store`、`aio/io/*.py` | Python Store及发布语义层，与C++生产接线分别核对 |

第三方类型封装在其接口内部，外部C ABI不传STL、异常或编译器私有对象。各层状态码、分配器和支持域按真实头，不因都叫AIO就相互代用。

## 2. PipelineFrame与所有权

PipelineFrame是命名块容器，标准块声明在aio_pipeline.h；自定义块仍可存在，运行时typed复合对象按[命名块合同](../../engineering/contracts/PIPELINE_BLOCK.md)。frame内块不等于跨节点Runtime对象仓已完整实现。

`aio_frame_add_block_move`成功后接管约定分配的buffer，调用方不再释放或访问。输入借用、输出接管、视图有效期与destroy成对；销毁frame会释放块和frame本体，后续header/KV访问也不合法。精度staging的疑似释放后使用见[实施差异](../../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)。

## 3. 图像读取与产品元数据

生产fits_read_file的native/CFITSIO读分支施加BSCALE/BZERO，返回实际物理标度；低层流式存储值读面另有缩放责任，不能重复施加。FITS像元、BUNIT、WCS/原点及shape保持配对，未知单位不能补ADU。HiPS读取使用共享NESTED/FITS局部映射，缺tile和真零分别表达。

`aio_hips_set_geometry`接收几何JSON字符串，writer把实际设置的对象写进逐帧清单；没设置就不伪造geometry。字段的像元原点、SIP和叶阶依[Normalize数据](../../engineering/contracts/NORMALIZE_DATA.md)及存储合同解释，不能只看清单键存在认定几何正确。

UPM sparse模型与dense缓存以其API读写，打开核对版本、来源和checksum。缓存合法性、科学有效域与统计解释分开。

## 4. 发布尾部与存储形态

HiPS writer的write_fits_atomic使用私有tmp、验证/持久化、可选字面全零打洞、rename及目录确认。打洞前后字节必须一致，NaN和−0.0不可按浮点等于0判断可打洞；不支持卷记录跳过。它是裸文件体积优化，不改变科学值或启用TRIM。

发布池可后台处理文件尾部，任务持临时/目标路径，调用方仍要在最终清单前等待所有任务完成。`ACSD_IO_PUBLISH_WORKERS`是运行资源绑定，不能推导计算预算已覆盖或整个进程总线程有界；实际I/O线程、排队和驻留另记录。失败和取消保留真实持久化状态，不一律宣称目标不存在。

归档`.hips.zst`、产品级索引和storage清单是明确设计目标；当前生产裸形态与未接线部分见实施差异。原文的archive默认、已有reader、文件枚举和未实现说明曾混在一起，完整保留于[来源档案](../../../实验/documentation-refactor/docs/IO_SOURCE.txt)。

## 5. 并发、错误与验证

reader/writer句柄默认单调用串行，不同worker各持独立会话。异步复制/借用明确寿命，文件发布和数组回收完成后才关闭所属运行。磁盘、格式、权限、取消、缺块和能力不可用分别返回，错误沿Runtime到CLI；普通日志与开发诊断见[日志系统](log_and_error_system.md)。

验证覆盖真实生产入口、缩放一次、双类型、长路径/大文件、同名并发、缺块、hash/checksum、部分写、取消、写满、覆盖及持久化。独立读取器检查格式，独立几何/科学真值检查数值；本轮仅重写与静态定位，没有执行这些用例。
''',
}

if __name__ == '__main__':
    archive = ROOT / '实验/documentation-refactor/docs/IO_SOURCE.txt'
    if archive.exists():
        raise SystemExit('Archive already exists')
    rev = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT).decode().strip()
    with archive.open('wb') as out:
        out.write(f'I/O source documents; commit={rev}\n'.encode())
        for name in DOCUMENTS:
            raw = (ROOT / name).read_bytes()
            blob = subprocess.check_output(['git', 'hash-object', '--stdin'], cwd=ROOT, input=raw).decode().strip()
            out.write(f'\n===== {name} | blob={blob} | bytes={len(raw)} =====\n'.encode())
            out.write(raw)
            out.write(b'\n===== END SOURCE =====\n')
    for name, body in DOCUMENTS.items():
        (ROOT / name).write_text(body, encoding='utf-8', newline='\n')
