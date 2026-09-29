# io

本目录存放文件级 I/O 接口合同：FITS 流式读写、HiPS 输入与原子输出发布。

## 职责边界

- 放：接口签名、流式读写语义、输入解析与原子发布规则的合同正文。
- 不放：数据对象语义（在 docs/contracts/）；aio 模块工作细节（在 docs/detail/infrastructure/17_aio.md）；落盘形态设计（在 docs/detail/PRODUCT_STORAGE_FORM.md）。

## 内容

- `IO_001_FITS_STREAM_INTERFACE.md` —— FITS 流式读写接口合同。
- `IO_002_HIPS_INPUT_INTERFACE.md` —— HiPS 输入接口合同。
- `IO_003_ATOMIC_OUTPUT_PUBLISH.md` —— 原子输出发布合同。

## 上游

上游：docs/ASTROCS_DESIGN.md §10（I/O 与原子产品）。
