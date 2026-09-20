#ifndef AIO_FILE_IO_H
#define AIO_FILE_IO_H

// ============================================================================
// aio_file_io.h - AIO 文件级读/摘要机制原语 (header-only; aio 唯一实现)
//
// 依据:
// - ASTROCS_DESIGN.md §9「aio 是文件级唯一 I/O 边界：任何文件读写必须经 aio；
//   不得有第二处 I/O 实现」+ §9.73 裁决 U5（负责人逐字：「全部走 aio……
//   没有其他需要读写的地方了」）。
// - 机器判据：全仓文件打开 / 流式读写 / 文件系统写操作，除 aio 内部外应为 0。
//   ⇒ 「整文件读入内存」与「算出已落盘文件的 sha256」都属文件读取，
//   **必须**在本头内实现；调用方（算法/基建）禁止自行 fopen/ifstream/fread。
// - 语义承接 R10-C（bughunt p2）：只有**完整读取成功**才产出结果；
//   fopen / ferror / fclose 任一失败 ⇒ 返回 false 且清空输出。**禁止**把空串
//   或"前缀（部分数据）哈希"当作完整性锚写进 provenance/结果结构。
// - SHA-256 单一实现 = lib/algorithms/shared/crypto（astrocs::crypto::Sha256），
//   本头不复制第二份算法。
// ============================================================================

#include "aio_util.h"

#include <cstddef>
#include <cstdio>
#include <string>

#include "crypto/sha256.h"

namespace aio_file {

// 完整读取 path（UTF-8, aio_fopen_utf8）到 out（二进制安全）。
// 返回 true = out 为完整文件内容; false = 打开/读取/关闭失败（out 已清空）。
inline bool read_all(const char* path, std::string* out) {
    if (out) out->clear();
    if (!path || !*path || !out) return false;
    std::FILE* f = aio_fopen_utf8(path, "rb");
    if (!f) return false;
    char buf[64 * 1024];
    std::size_t n = 0;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) out->append(buf, n);
    const bool read_ok = (std::ferror(f) == 0);
    const bool close_ok = (std::fclose(f) == 0);
    if (!read_ok || !close_ok) { out->clear(); return false; }
    return true;
}

// 完整读取 path 并输出 64 字符小写 hex sha256。
// 返回 true = out_hex 为完整文件摘要; false = 读取/关闭失败（out_hex 已清空）。
inline bool sha256_hex(const char* path, std::string* out_hex) {
    if (out_hex) out_hex->clear();
    if (!path || !*path || !out_hex) return false;
    std::FILE* f = aio_fopen_utf8(path, "rb");
    if (!f) return false;
    astrocs::crypto::Sha256 h;
    unsigned char buf[64 * 1024];
    std::size_t n = 0;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) h.update(buf, n);
    const bool read_ok = (std::ferror(f) == 0);
    const bool close_ok = (std::fclose(f) == 0);
    if (!read_ok || !close_ok) return false;
    *out_hex = h.final_hex();
    return true;
}

}  // namespace aio_file

#endif  // AIO_FILE_IO_H
