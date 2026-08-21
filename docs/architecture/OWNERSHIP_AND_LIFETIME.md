# Ownership & Lifetime

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

- p2_upm_open 失败路径已统一 delete（V19R2 PR#1 门禁修复）。
- dense cache 句柄 AioUpmDense 单出口释放（unique_ptr guard，见 lib/astro_image_io/src/aio_upm.cpp:~448 unique_ptr guard；实现 lib/astro_image_io/src/aio_upm.cpp:283 std::unique_ptr<AioUpmDense> guard(d) 单出口释放）。
- aio_upm_read_all_dynamic 返回 delete[] 由调用方负责（lib/astro_image_io/include/aio_upm.h:61；实现 lib/astro_image_io/src/aio_upm.cpp:163 new char[]）。

## 契约

ENG-OWN-001..003（S2 注册）。
