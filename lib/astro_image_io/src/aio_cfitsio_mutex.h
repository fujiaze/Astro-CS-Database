// aio_cfitsio_mutex.h — RT-008: cfitsio 进程级串行化互斥量（共享单例）。
// cfitsio 的 FptrTable / handleTable / fits_already_open 在并行访问下非线程安全
//（_REENTRANT 锁未覆盖全部表路径）。所有 cfitsio 打开/关闭临界区必须共用
// 同一把进程级锁，否则跨编译单元的 cfitsio 访问仍会竞争。
#pragma once

#include <mutex>

namespace aio {
// 进程级 cfitsio 串行化锁（单一定义，跨 aio_fits / aio_hips_reader 共用）
inline std::mutex& cfitsio_io_mutex() {
    static std::mutex m;
    return m;
}
}  // namespace aio
