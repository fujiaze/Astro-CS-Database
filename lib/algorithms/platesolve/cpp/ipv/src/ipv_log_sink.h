#ifndef IPV_LOG_SINK_H
#define IPV_LOG_SINK_H

// ============================================================================
// ipv_log_sink.h - ipv::Logger 落点接缝的唯一实现（**内部头**，不属公共 include 面）
//
// 依据
//   - ASTROCS_DESIGN.md §10「aio 是文件级唯一 I/O 边界」（CLEAN-403）: 日志落盘
//     经 aio 机制原语（aio_atomic::append_open / append_write / append_flush /
//     append_close），本模块不自持 std::ofstream / FILE* 通道。
//   - ASTROCS_DESIGN.md §8.4/§8.5: 模块 include/ 是公共头面，src/ 是实现面。
//
// 为什么接缝实现在这里而不在 ipv/include/ipv_log.h 的内联体里（DOC-004/AST-API）:
//   公共头必须能在**公共 include 面**（lib/**/include ∪ lib/**/cpp）下被 clang
//   独立解析；aio 的机制原语头 aio_atomic_file.h 落在基建层内部目录
//   lib/infrastructure/aio/src/，不属公共 include 面 ⇒ 公共头不得 include 它。
//   同款先例: lib/algorithms/star_detection/src/sdet_log.cpp 与
//   lib/algorithms/psf/src/dpsf_log.cpp 亦把 aio 机制原语只放在 src 侧 TU。
//
// 用法: 使用 ipv::Logger 的**生产 TU** 包含本头（quoted include 先查本文件所在
// 目录，故无需额外 -I）。四个函数全部 inline ⇒ 不新增链接单元，任何编译子集
// （astrocs_p1_ipv 全量 / eng/tests/unit/p1wcs / ipv/test 部分 TU）都能自足链接。
// ============================================================================

#include "ipv_log.h"          // 接缝声明（公共头）
#include "aio_atomic_file.h"  // 基建层内部机制原语（该 TU 的 include 面含 aio/src）

namespace ipv {

struct LogSink {
    aio_atomic::AppendSink* handle = nullptr;
};

inline LogSink* log_sink_open(const std::string& path) {
    aio_atomic::AppendSink* h = aio_atomic::append_open(path, nullptr);
    if (!h) return nullptr;
    LogSink* s = new (std::nothrow) LogSink();
    if (!s) {
        aio_atomic::append_close(h);
        return nullptr;
    }
    s->handle = h;
    return s;
}

inline void log_sink_write(LogSink* sink, const char* data, std::size_t n) {
    if (sink && sink->handle) (void)aio_atomic::append_write(sink->handle, data, n);
}

inline void log_sink_flush(LogSink* sink) {
    if (sink && sink->handle) (void)aio_atomic::append_flush(sink->handle);
}

inline void log_sink_close(LogSink* sink) {
    if (!sink) return;
    if (sink->handle) aio_atomic::append_close(sink->handle);
    delete sink;
}

} // namespace ipv

#endif // IPV_LOG_SINK_H
