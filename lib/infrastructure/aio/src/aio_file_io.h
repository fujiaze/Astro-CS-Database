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
#include <cstdint>
#include <cstdio>
#include <functional>
#include <mutex>
#include <string>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

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

// 只读文件头至多 max_bytes 字节 (格式探测; CLEAN-403 补能力)。
// 返回 true = 成功打开并读完头部 (out 为实际读到的前缀, 可能短于 max_bytes);
// false = 打开失败或读取出错 (out 已清空)。不把整个文件载入内存。
inline bool read_head(const char* path, std::size_t max_bytes, std::string* out) {
    if (out) out->clear();
    if (!path || !*path || !out) return false;
    if (max_bytes == 0) return true;
    std::FILE* f = aio_fopen_utf8(path, "rb");
    if (!f) return false;
    std::string buf(max_bytes, '\0');
    const std::size_t n = std::fread(&buf[0], 1, max_bytes, f);
    const bool read_ok = (std::ferror(f) == 0);
    const bool close_ok = (std::fclose(f) == 0);
    if (!read_ok || !close_ok) { out->clear(); return false; }
    buf.resize(n);
    *out = buf;
    return true;
}

// 分块流式读取 (内存极简: 峰值 = 单块 64 KiB; CLEAN-403 补能力)。
// 对每块调用 fn(data, n); fn 返回 false ⇒ 立即中止且本函数返回 false。
// 返回 true = 完整读完且关闭成功。err 非空时写失败原因。
inline bool read_stream(const char* path,
                        const std::function<bool(const char*, std::size_t)>& fn,
                        std::string* err) {
    if (err) err->clear();
    if (!path || !*path || !fn) {
        if (err) *err = "invalid args";
        return false;
    }
    std::FILE* f = aio_fopen_utf8(path, "rb");
    if (!f) {
        if (err) *err = std::string("open failed: ") + path;
        return false;
    }
    char buf[64 * 1024];
    bool ok = true;
    std::size_t n = 0;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
        if (!fn(buf, n)) { ok = false; break; }
    }
    if (std::ferror(f) != 0) ok = false;
    if (std::fclose(f) != 0) ok = false;
    if (!ok && err) *err = std::string("read failed: ") + path;
    return ok;
}

// 从 offset 起读取恰好 count 字节 (分块; 峰值内存 = count; CLEAN-403 补能力)。
// 返回 true = out 恰为 count 字节; false = 打开失败/越界/短读 (out 已清空)。
inline bool read_range(const char* path, std::uint64_t offset, std::size_t count,
                       std::string* out) {
    if (out) out->clear();
    if (!path || !*path || !out) return false;
    if (count == 0) return true;
    std::FILE* f = aio_fopen_utf8(path, "rb");
    if (!f) return false;
#ifdef _WIN32
    if (_fseeki64(f, static_cast<long long>(offset), SEEK_SET) != 0) {
#else
    if (fseeko(f, static_cast<off_t>(offset), SEEK_SET) != 0) {
#endif
        std::fclose(f);
        return false;
    }
    out->resize(count);
    const std::size_t n = std::fread(&(*out)[0], 1, count, f);
    const bool read_ok = (std::ferror(f) == 0) && (n == count);
    const bool close_ok = (std::fclose(f) == 0);
    if (!read_ok || !close_ok) { out->clear(); return false; }
    return true;
}

// ── 位置写（随机访问顺序无关写入；P3-STREAM-01）────────────────────────────
// 语义：子块流式的**唯一随机写通道**（ASTROCS_DESIGN §8.3 export「子块流式」）。
// 用途：子块产出顺序与平面文件的字节序无关（平面 = 行主序连续区），故必须能
// 「按偏移写子块」而不是只能顺序追加；调用方（算法/基建）禁止自行 fopen/fseek。
// 并发：同一实例的多线程写由内部互斥串行化（FILE* 游标是共享状态）；
// 区间互不重叠时结果与写入顺序无关（子块 = 互斥区间，故输出逐位确定）。
class RandomWriter {
public:
    ~RandomWriter() { close(); }
    RandomWriter() = default;
    RandomWriter(const RandomWriter&) = delete;
    RandomWriter& operator=(const RandomWriter&) = delete;

    // 创建/截断 path（二进制读写）。失败返回 false（err 非空时写原因）。
    bool open(const char* path, std::string* err) {
        if (err) err->clear();
        close();
        f_ = aio_fopen_utf8(path, "wb+");
        if (!f_) {
            if (err) *err = std::string("open(wb+) failed: ") + (path ? path : "");
            return false;
        }
        path_ = path ? path : "";
        return true;
    }

    bool is_open() const { return f_ != nullptr; }

    // 从 offset 起写 count 字节。返回 true = 全部写入且无错误。
    bool write_at(std::uint64_t offset, const void* data, std::size_t count,
                  std::string* err) {
        if (err) err->clear();
        if (!f_) {
            if (err) *err = "write_at on closed writer";
            return false;
        }
        if (count == 0) return true;
        std::lock_guard<std::mutex> lk(mu_);
#ifdef _WIN32
        if (_fseeki64(f_, static_cast<long long>(offset), SEEK_SET) != 0) {
#else
        if (fseeko(f_, static_cast<off_t>(offset), SEEK_SET) != 0) {
#endif
            if (err) *err = "seek failed";
            return false;
        }
        const std::size_t n = std::fwrite(data, 1, count, f_);
        if (n != count || std::ferror(f_) != 0) {
            if (err) *err = "short write";
            return false;
        }
        return true;
    }

    // 把文件长度预置为 size 字节（末字节写 0）。返回 true = 成功。
    bool reserve(std::uint64_t size, std::string* err) {
        if (size == 0) return true;
        const char z = 0;
        return write_at(size - 1, &z, 1, err);
    }

    // flush（fflush）→ fsync（落盘）→ close。三步任一失败 = false。
    bool flush_and_sync(std::string* err) {
        if (err) err->clear();
        if (!f_) {
            if (err) *err = "flush on closed writer";
            return false;
        }
        bool ok = true;
        {
            std::lock_guard<std::mutex> lk(mu_);
            if (std::fflush(f_) != 0) {
                if (err) *err = "fflush failed";
                ok = false;
            }
        }
        if (ok) {
#ifdef _WIN32
            if (_commit(_fileno(f_)) != 0) {
#else
            if (::fsync(fileno(f_)) != 0) {
#endif
                if (err) *err = "fsync failed";
                ok = false;
            }
        }
        if (std::fclose(f_) != 0 && ok) {
            if (err) *err = "fclose failed";
            ok = false;
        }
        f_ = nullptr;
        return ok;
    }

    // 关闭（不落盘保证）；已关闭时幂等。
    void close() {
        if (!f_) return;
        std::lock_guard<std::mutex> lk(mu_);
        std::fclose(f_);
        f_ = nullptr;
    }

    const std::string& path() const { return path_; }

private:
    std::FILE* f_ = nullptr;
    std::string path_;
    std::mutex mu_;
};

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
