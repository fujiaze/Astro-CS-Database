# interfaces

> 上游：docs/ASTROCS_DESIGN.md §8.1（唯一 CLI 入口、阶段独立调度器与阶段间磁盘交换）、§8.2（阶段内命名块内存管线）、§10（I/O 与原子产品）。

本目录存放阶段间与文件级的接口合同，按数据交换与 I/O 两个子域分组。

## 职责边界

- 放：阶段产品交换、工件库布局、provenance 链、FITS/HiPS/原子发布的接口合同。
- 不放：数据语义正本（在 docs/contracts/DATA_SEMANTICS.md）；架构层 I/O 与原子性总述（在 docs/architecture/IO_AND_ATOMICITY.md）；aio 模块工作细节（在 docs/plugins/infrastructure/17_aio.md）。

## 内容

- `data/` —— 数据交换子域：阶段间产品交换、生产工件库布局、产品 provenance 链三份合同。
- `io/` —— I/O 子域：FITS 流式读写、HiPS 输入、原子输出发布三份合同。
