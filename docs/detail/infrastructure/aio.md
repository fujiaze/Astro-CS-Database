# 统一 AIO

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

## ARS 异步 BufferPool 与多对象产品的接线

Host 目标 `submit_async(request,slot_grant) → ticket`：成功接管 buffer read lease 或独立拷贝的所有权，完成/失败/取消一致归还 credits；queue+inflight+压缩 scratch+staging 严格计费，不能只限制 I/O 线程数。计算 worker 不等待普通磁盘完成，慢盘背压交给 scheduler，减少预取/产出任务并发；任务不存在时允许 CPU idle。Frame 的 signal+frame_science+metadata/model 由 complete revision manifest 作为唯一读者入口，单文件 write_fits_atomic 不等价于多文件事务。详见 [ARS 详细设计 §6](../../engineering/architecture/PROCESS_RUNTIME_DETAILED_DESIGN.md)，现有 `aio_pipeline_engine` 与 writer 的实际接线必须逐个验证。

## 5. 并发、错误与验证

reader/writer句柄默认单调用串行，不同worker各持独立会话。异步复制/借用明确寿命，文件发布和数组回收完成后才关闭所属运行。磁盘、格式、权限、取消、缺块和能力不可用分别返回，错误沿Runtime到CLI；普通日志与开发诊断见[日志系统](log_and_error_system.md)。

验证覆盖真实生产入口、缩放一次、双类型、长路径/大文件、同名并发、缺块、hash/checksum、部分写、取消、写满、覆盖及持久化。独立读取器检查格式，独立几何/科学真值检查数值；本轮仅重写与静态定位，没有执行这些用例。
