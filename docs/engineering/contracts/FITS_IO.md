# FITS 读写合同

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
