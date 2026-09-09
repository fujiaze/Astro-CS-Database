// ============================================================================
// aio_abi.cpp — AstroCS 唯一 AIO C ABI v1 实现 (AIO-001)
//
// 合同: include/astrocs/io/aio_abi_v1.h (v1 冻结) +
//       contracts/data/aio_abi_contract_v1.json (唯一事实源)。
// 归属: astrocs_aio 域 (宪章 §8.5 astrocs_aio 交付单元; AIO 正统归属)。
//
// 关键设计:
//  - SHA-256 原语复用 lib/common/crypto 单一实现 (astrocs::crypto::Sha256;
//    与 AIO UPM 容器/Phase2 模型哈希同源, 禁第二实现)。正确性由 FIPS 180-4
//    标准向量锚定 (tests/unit/aio_abi_tests.cpp U2, oracle=hashlib 预生成)。
//  - 内容哈希复核语义: 先验声明格式 (64hex 小写, 否则 AIO_ERR_DECL_INVALID,
//    不给"格式错"混淆"篡改"), 再重算比对 (不一致 → AIO_ERR_HASH_MISMATCH)。
//    文件复核分块流式 (AIO_HASH_FILE_CHUNK 栈缓冲, 不整体载入)。
//  - 异常屏障 (家族口径 f1cb487c / bughunt_p1_batchI R9-A): 全部公共入口
//    try/catch, C++ 异常不跨 C ABI 边界 (bad_alloc → AIO_ERR_NOMEM,
//    其余 std::exception/未知 → AIO_ERR_INTERNAL 域稳定码; 本 ABI 用
//    AIO_ERR_IO/内部分类, 失败绝不外抛)。
//  - 并发: 全入口 reentrant (无跨调用共享可变状态; FaultRegistry 为测试注入
//    只读面, 生产进程 env 为空)。内部无并行 (internal_parallel=none, 确定性
//    单流; 1/N worker 并发调用位级一致由 U5 验证)。
//  - 无第三方类型泄露; 无 STL 跨边界 (签名全 POD/指针); 禁异常跨边界。
//
// 故障注入 (测试专用; 验收 "注入必败"):
//  ASTROCS_AIO_FAULT=n1_hash_value_flip            → sha256 输出首字符翻转
//             n2_verify_mismatch_shortcut          → verify_buffer 篡改短路恒 OK
//             n3_file_size_skip                    → verify_file 跳过 size 核对
//  每个注册名在首个触发 CHECK 处输出 FAULT-INJECT 行并确定性翻转断言
//  (tests/unit/aio_abi_test_main.hpp FaultRegistry 报告); 生产 env 为空零开销。
// ============================================================================

#include "astrocs/io/aio_abi_v1.h"

#include "crypto/sha256.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

// ───────── 故障注册 (测试注入; 与 tests/unit/aio_abi_test_main.hpp 对齐) ─────────
namespace {

struct AioFaultRegistry {
    bool n1_hash_value_flip = false;
    bool n2_verify_mismatch_shortcut = false;
    bool n3_file_size_skip = false;
};

const AioFaultRegistry& aio_fault_registry() {
    static const AioFaultRegistry r = [] {
        AioFaultRegistry reg;
        const char* env = std::getenv("ASTROCS_AIO_FAULT");
        if (!env || !*env) return reg;
        std::string s(env);
        auto has = [&s](const char* name) {
            const std::size_t n = std::strlen(name);
            std::size_t pos = 0;
            while (pos <= s.size()) {
                std::size_t comma = s.find(',', pos);
                if (comma == std::string::npos) comma = s.size();
                if (comma - pos == n && s.compare(pos, n, name) == 0) return true;
                pos = comma + 1;
            }
            return false;
        };
        reg.n1_hash_value_flip = has("n1_hash_value_flip");
        reg.n2_verify_mismatch_shortcut = has("n2_verify_mismatch_shortcut");
        reg.n3_file_size_skip = has("n3_file_size_skip");
        return reg;
    }();
    return r;
}

// 声明格式校验: 恰 64 字符小写 [0-9a-f]
bool decl_is_valid(const char* s) {
    if (!s) return false;
    if (std::strlen(s) != AIO_DIGEST_HEX_LEN) return false;
    for (std::size_t i = 0; i < AIO_DIGEST_HEX_LEN; ++i) {
        const char c = s[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    }
    return true;
}


}  // namespace

// ───────── 唯一握手入口 ─────────
extern "C" int aio_abi_query_v1(aio_abi_info_v1* out) {
    if (!out) return AIO_ERR_PARAM;
    // handshake: 失配即拒, 不猜布局 (宪章 §8.6)
    if (out->struct_size != sizeof(aio_abi_info_v1) ||
        out->abi_version != AIO_ABI_VERSION_V1) {
        return AIO_ERR_ABI_MISMATCH;
    }
    // 只填静态常量; reentrant/threadsafe; internal_parallel=none
    const aio_abi_info_v1 info = AIO_ABI_INFO_V1_INIT;
    *out = info;
    return AIO_OK;
}

// ───────── 内容哈希复核 v1 ─────────
extern "C" int aio_content_hash_buffer_v1(const void* data, uint64_t len, char* out_hex) {
    if (!out_hex) return AIO_ERR_PARAM;
    if (!data && len != 0) return AIO_ERR_PARAM;
    try {
        astrocs::crypto::Sha256 h;
        if (len != 0) {
            // 分块 update (Sha256 内部 64B 块; 大输入按 AIO_HASH_FILE_CHUNK 逻辑块
            // 推进, 语义等价单流)
            const unsigned char* p = static_cast<const unsigned char*>(data);
            uint64_t remaining = len;
            while (remaining > 0) {
                const std::size_t chunk = static_cast<std::size_t>(
                    remaining > AIO_HASH_FILE_CHUNK ? AIO_HASH_FILE_CHUNK : remaining);
                h.update(p, chunk);
                p += chunk;
                remaining -= chunk;
            }
        }
        // lib/common 单一实现直接输出 64 字符小写 hex (不重写第二份 SHA-256)
        const std::string hex = h.final_hex();
        if (hex.size() != AIO_DIGEST_HEX_LEN) return AIO_ERR_INTERNAL;
        std::memcpy(out_hex, hex.c_str(), AIO_DIGEST_HEX_LEN);
        out_hex[AIO_DIGEST_HEX_LEN] = '\0';
        if (aio_fault_registry().n1_hash_value_flip) {
            // 注入: 输出首字符翻转 (确定性; U2/U3 已知向量必败)
            out_hex[0] = (out_hex[0] == '0') ? '1' : '0';
        }
        return AIO_OK;
    } catch (const std::bad_alloc&) {
        return AIO_ERR_NOMEM;
    } catch (...) {
        return AIO_ERR_INTERNAL;
    }
}

extern "C" int aio_content_hash_verify_buffer_v1(const void* data, uint64_t len,
                                                 const char* declared_hex64) {
    // 先验声明 (非法声明独立于内容; 篡改不得伪装成格式错)
    if (!decl_is_valid(declared_hex64)) return AIO_ERR_DECL_INVALID;
    if (aio_fault_registry().n2_verify_mismatch_shortcut) {
        // 注入: 篡改检测短路恒 OK (negative N3 必败)
        return AIO_OK;
    }
    char actual[AIO_DIGEST_HEX_LEN + 1];
    const int st = aio_content_hash_buffer_v1(data, len, actual);
    if (st != AIO_OK) return st;
    if (std::memcmp(actual, declared_hex64, AIO_DIGEST_HEX_LEN) != 0) {
        return AIO_ERR_HASH_MISMATCH;
    }
    return AIO_OK;
}

extern "C" int aio_content_hash_verify_file_v1(const char* path_utf8,
                                               const char* declared_hex64,
                                               uint64_t expected_size,
                                               uint64_t* actual_size_out) {
    if (!path_utf8 || !*path_utf8) return AIO_ERR_PARAM;
    if (actual_size_out) *actual_size_out = 0;
    // 先验声明 (与 buffer 复核同规则)
    if (!decl_is_valid(declared_hex64)) return AIO_ERR_DECL_INVALID;
    if (aio_fault_registry().n2_verify_mismatch_shortcut) {
        // 注入一致性: 同一注入名使文件篡改复核短路 (防御性; negative 注入点在 N3)
        return AIO_OK;
    }
    try {
        std::FILE* f = std::fopen(path_utf8, "rb");
        if (!f) return AIO_ERR_IO;
        astrocs::crypto::Sha256 h;
        unsigned char buf[AIO_HASH_FILE_CHUNK];
        std::size_t nread = 0;
        std::size_t chunk = static_cast<std::size_t>(AIO_HASH_FILE_CHUNK);
        uint64_t total = 0;
        while ((nread = std::fread(buf, 1, chunk, f)) > 0) {
            h.update(buf, nread);
            total += nread;
        }
        const bool io_err = std::ferror(f) != 0;
        std::fclose(f);
        if (io_err) return AIO_ERR_IO;
        if (actual_size_out) *actual_size_out = total;
        // 大小核对 (DATA-001 manifest size 语义; expected_size=0 = 不核对)
        if (expected_size != 0 && !aio_fault_registry().n3_file_size_skip &&
            total != expected_size) {
            return AIO_ERR_TRUNCATED;
        }
        if (actual_size_out) *actual_size_out = total;  // 截断路径同样回填实际字节数
        // 重算摘要与声明比对
        const std::string hex = h.final_hex();
        if (hex.size() != AIO_DIGEST_HEX_LEN) return AIO_ERR_INTERNAL;
        if (std::memcmp(hex.c_str(), declared_hex64, AIO_DIGEST_HEX_LEN) != 0) {
            return AIO_ERR_HASH_MISMATCH;
        }
        return AIO_OK;
    } catch (const std::bad_alloc&) {
        return AIO_ERR_NOMEM;
    } catch (...) {
        return AIO_ERR_INTERNAL;
    }
}
