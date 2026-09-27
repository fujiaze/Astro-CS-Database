# Ownership & Lifetime

> 上游：ASTROCS_DESIGN.md §8（软件架构）

## 规则

- C API 返回的 handle（model/cache/reader）由调用方负责 close/free：
  p2_upm_build → p2_upm_close；aio_upm_open → aio_upm_close；
  aio_hips_reader → aio_hips_reader_close。
- 输出 buffer 语义：调用方分配并传容量；函数不接管所有权。
- 内部 RAII：Model/ControlNode 等均 RAII 管理；失败路径单出口释放。
- 公共指针注释约定：borrowed（不持有）、owned（调用方释放）、
  optional（可空）。
- thread-local：g_upm_error（aio_upm）为 thread_local，避免跨线程污染。

## 已知审计点

- p2_upm_open 失败路径统一 delete。
- dense cache 句柄 AioUpmDense 单出口释放（`lib/infrastructure/aio/src/aio_upm.cpp:298` `std::unique_ptr<AioUpmDense> guard(d)`，所有路径释放）。
- aio_upm_read_all_dynamic 返回 delete[] 由调用方负责（声明 `lib/infrastructure/aio/include/aio_upm.h:75`；实现 `lib/infrastructure/aio/src/aio_upm.cpp:176` `new char[]`）。

## 契约

ENG-OWN-001..003（S2 注册）。
