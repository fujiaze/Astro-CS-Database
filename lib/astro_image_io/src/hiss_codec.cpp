// ============================================================================
// hiss_codec.cpp - AstroCS HISS Codec/Checksum 注册表与内置实现
//
// 内容:
// 1. RAW codec (无压缩, 必须): compress/decompress 直接 memcpy, bound = input_size
// 2. LZ4 / Zstd codec (可选): 编译时通过 -DHAS_LZ4 / -DHAS_ZSTD 启用
// 3. CodecRegistry 单例: 注册/查找/列出 codec, RAW 内置不可覆盖
// 4. CRC32-C (Castagnoli) 校验实现 (共享, 供 Reader/Writer 复用)
// 5. ChecksumRegistry 单例: 注册/查找/列出 checksum, CRC32C 内置
//
// 设计说明:
// - hiss_format.h 中 CodecRegistry/ChecksumRegistry 类未声明私有数据成员与构造函数
// (规范已冻结), 故注册表状态以文件作用域静态容器承载, 由单例方法访问。
// - 单例采用 Meyers singleton (C++11 起局部静态初始化线程安全)。
// - 内置 codec/checksum 注册在 RegistryState 构造函数中完成, 由 magic static 保证
// 首次访问时线程安全地一次性初始化。
// - 线程安全: register/find/list 经 std::mutex 保护。
// - 不依赖外部库: LZ4/Zstd 为可选, RAW/CRC32C 为必需。
// - CRC32C 实现从 hiss_reader.cpp 移入此处共享, 避免 Reader/Writer 重复定义。
// ============================================================================
#include "hiss_format.h"

#include <cstring>    // std::memcpy
#include <cstdio>     // fprintf
#include <map>
#include <mutex>
#include <vector>

// 可选压缩库: 编译时通过 -DHAS_ZSTD / -DHAS_LZ4 启用 (与 aio_compressor.cpp 一致)
#ifdef HAS_ZSTD
#include <zstd.h>
#endif
#ifdef HAS_LZ4
#include <lz4.h>
#endif

namespace hiss {

// ============================================================================
// 内部: codec 实现函数 (文件作用域)
// ============================================================================

// ---------------------------------------------------------------------------
// RAW codec (无压缩, 必须) - compress/decompress 直接 memcpy, bound = input_size
// ---------------------------------------------------------------------------

// RAW compress: 直接拷贝, 无压缩
// input/input_size - 源数据
// output - 输出缓冲区 (调用方需保证容量 >= input_size)
// output_size - 输入时为 output 缓冲区容量, 输出时为实际写入字节数
// 返回 0=成功, <0=失败
static int raw_compress(const uint8_t* input, size_t input_size,
                        uint8_t* output, size_t* output_size) {
    if (!output || !output_size) {
        fprintf(stderr, "[hiss][codec] raw_compress: 无效参数 (output=%p output_size=%p)\n",
                (const void*)output, (void*)output_size);
        return -1;
    }
    // 空输入: 直接返回 0 字节, 不访问 input 指针
    if (input_size == 0) {
        *output_size = 0;
        return 0;
    }
    if (!input) {
        fprintf(stderr, "[hiss][codec] raw_compress: input 为空但 input_size=%zu\n", input_size);
        return -1;
    }
    if (*output_size < input_size) {
        fprintf(stderr, "[hiss][codec] raw_compress: 输出缓冲区不足 (cap=%zu need=%zu)\n",
                *output_size, input_size);
        return -2;
    }
    std::memcpy(output, input, input_size);
    *output_size = input_size;
    return 0;
}

// RAW decompress: 直接拷贝, 无压缩
// input/input_size - 压缩数据 (对于 RAW 即原始数据)
// output/output_size - 输出缓冲区及其容量 (对于 RAW 容量应 >= input_size)
// 返回 0=成功, <0=失败
static int raw_decompress(const uint8_t* input, size_t input_size,
                          uint8_t* output, size_t output_size) {
    if (!output) {
        fprintf(stderr, "[hiss][codec] raw_decompress: 无效参数 (output 为空)\n");
        return -1;
    }
    if (input_size == 0) {
        return 0;
    }
    if (!input) {
        fprintf(stderr, "[hiss][codec] raw_decompress: input 为空但 input_size=%zu\n", input_size);
        return -1;
    }
    if (output_size < input_size) {
        fprintf(stderr, "[hiss][codec] raw_decompress: 输出缓冲区不足 (cap=%zu need=%zu)\n",
                output_size, input_size);
        return -2;
    }
    std::memcpy(output, input, input_size);
    return 0;
}

// RAW bound: 无压缩, 压缩后上界 = 输入大小
static size_t raw_bound(size_t input_size) {
    return input_size;
}

// ---------------------------------------------------------------------------
// Zstd codec (可选, 需 -DHAS_ZSTD)
// ---------------------------------------------------------------------------
#ifdef HAS_ZSTD

// 默认压缩级别 (Zstd 范围 1-22, 3 为库默认, 平衡速度与压缩率)
static const int kZstdDefaultLevel = 3;

static int zstd_compress(const uint8_t* input, size_t input_size,
                         uint8_t* output, size_t* output_size) {
    if (!output || !output_size) {
        fprintf(stderr, "[hiss][codec] zstd_compress: 无效参数\n");
        return -1;
    }
    if (input_size == 0) {
        *output_size = 0;
        return 0;
    }
    if (!input) {
        fprintf(stderr, "[hiss][codec] zstd_compress: input 为空但 input_size=%zu\n", input_size);
        return -1;
    }
    size_t out = ZSTD_compress(output, *output_size, input, input_size, kZstdDefaultLevel);
    if (ZSTD_isError(out)) {
        fprintf(stderr, "[hiss][codec] zstd_compress 失败: %s\n", ZSTD_getErrorName(out));
        return -3;
    }
    *output_size = out;
    return 0;
}

static int zstd_decompress(const uint8_t* input, size_t input_size,
                           uint8_t* output, size_t output_size) {
    if (!output) {
        fprintf(stderr, "[hiss][codec] zstd_decompress: 无效参数\n");
        return -1;
    }
    if (input_size == 0) {
        return 0;
    }
    if (!input) {
        fprintf(stderr, "[hiss][codec] zstd_decompress: input 为空但 input_size=%zu\n", input_size);
        return -1;
    }
    size_t out = ZSTD_decompress(output, output_size, input, input_size);
    if (ZSTD_isError(out)) {
        fprintf(stderr, "[hiss][codec] zstd_decompress 失败: %s\n", ZSTD_getErrorName(out));
        return -3;
    }
    return 0;
}

static size_t zstd_bound(size_t input_size) {
    return ZSTD_compressBound(input_size);
}
#endif // HAS_ZSTD

// ---------------------------------------------------------------------------
// LZ4 codec (可选, 需 -DHAS_LZ4)
// ---------------------------------------------------------------------------
#ifdef HAS_LZ4

static int lz4_compress(const uint8_t* input, size_t input_size,
                        uint8_t* output, size_t* output_size) {
    if (!output || !output_size) {
        fprintf(stderr, "[hiss][codec] lz4_compress: 无效参数\n");
        return -1;
    }
    if (input_size == 0) {
        *output_size = 0;
        return 0;
    }
    if (!input) {
        fprintf(stderr, "[hiss][codec] lz4_compress: input 为空但 input_size=%zu\n", input_size);
        return -1;
    }
    // LZ4_compress_default 返回压缩后字节数, 失败返回 0
    int out = LZ4_compress_default(
        (const char*)input, (char*)output,
        (int)input_size, (int)*output_size);
    if (out <= 0) {
        fprintf(stderr, "[hiss][codec] lz4_compress 失败 (返回 %d)\n", out);
        return -3;
    }
    *output_size = (size_t)out;
    return 0;
}

static int lz4_decompress(const uint8_t* input, size_t input_size,
                          uint8_t* output, size_t output_size) {
    if (!output) {
        fprintf(stderr, "[hiss][codec] lz4_decompress: 无效参数\n");
        return -1;
    }
    if (input_size == 0) {
        return 0;
    }
    if (!input) {
        fprintf(stderr, "[hiss][codec] lz4_decompress: input 为空但 input_size=%zu\n", input_size);
        return -1;
    }
    // LZ4_decompress_safe 返回解压后字节数, 失败返回负值
    int out = LZ4_decompress_safe(
        (const char*)input, (char*)output,
        (int)input_size, (int)output_size);
    if (out < 0) {
        fprintf(stderr, "[hiss][codec] lz4_decompress 失败 (返回 %d)\n", out);
        return -3;
    }
    return 0;
}

static size_t lz4_bound(size_t input_size) {
    return (size_t)LZ4_compressBound((int)input_size);
}
#endif // HAS_LZ4

// ============================================================================
// 内部: CRC32-C (Castagnoli) 校验实现 (共享, 供 Reader/Writer 复用)
// 多项式: 0x1EDC6F41 (反向: 0x82F63B78)
// 初始值: 0xFFFFFFFF, 最终异或: 0xFFFFFFFF
// 用于 HissSubblockDescriptor.checksum (ChecksumType::CRC32C)
// 原实现位于 hiss_reader.cpp, 已移至此处共享, 避免重复定义
// ============================================================================

// CRC32-C 查表实现 (运行时生成表, 首次调用时初始化)
static uint32_t crc32c_table[256];
static bool     crc32c_table_init = false;

static void init_crc32c_table() {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0x82F63B78u;  // Castagnoli 反向多项式
            else
                crc >>= 1;
        }
        crc32c_table[i] = crc;
    }
    crc32c_table_init = true;
}

static uint32_t crc32c(const uint8_t* data, size_t size) {
    if (!crc32c_table_init) init_crc32c_table();
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < size; i++) {
        crc = crc32c_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFu;
}

// ChecksumFunc 签名适配: uint32_t → uint64_t (高位补 0)
static uint64_t crc32c_compute(const uint8_t* data, size_t size) {
    return (uint64_t)crc32c(data, size);
}

// ============================================================================
// 内部: 注册表状态 (文件作用域静态, 由 CodecRegistry/ChecksumRegistry 单例方法访问)
// ============================================================================

struct RegistryState {
    std::map<CodecId, CodecEntry>     codecs;    // 按 CodecId 索引的 codec 表
    std::map<ChecksumType, ChecksumEntry> checksums;  // 按 ChecksumType 索引的 checksum 表
    std::mutex                         mutex;    // 保护并发访问

    // 构造时注册内置 codec (RAW 必须; Zstd/LZ4 可选) 和内置 checksum (CRC32C 必须)
    // 由 magic static 保证首次访问时线程安全地一次性初始化
    RegistryState() {
        // --- RAW (必须, 始终可用) ---
        {
            CodecEntry e;
            e.id         = CodecId::RAW;
            e.name       = "RAW";
            e.compress   = raw_compress;
            e.decompress = raw_decompress;
            e.bound      = raw_bound;
            codecs[e.id] = e;
            fprintf(stderr, "[hiss][codec] 内置注册: RAW (无压缩)\n");
        }

        // --- Zstd (可选, -DHAS_ZSTD 时启用) ---
#ifdef HAS_ZSTD
        {
            CodecEntry e;
            e.id         = CodecId::ZSTD;
            e.name       = "ZSTD";
            e.compress   = zstd_compress;
            e.decompress = zstd_decompress;
            e.bound      = zstd_bound;
            codecs[e.id] = e;
            fprintf(stderr, "[hiss][codec] 内置注册: ZSTD (level=%d)\n", kZstdDefaultLevel);
        }
#else
        fprintf(stderr, "[hiss][codec] ZSTD 未编译 (需 -DHAS_ZSTD)\n");
#endif

        // --- LZ4 (可选, -DHAS_LZ4 时启用) ---
#ifdef HAS_LZ4
        {
            CodecEntry e;
            e.id         = CodecId::LZ4;
            e.name       = "LZ4";
            e.compress   = lz4_compress;
            e.decompress = lz4_decompress;
            e.bound      = lz4_bound;
            codecs[e.id] = e;
            fprintf(stderr, "[hiss][codec] 内置注册: LZ4\n");
        }
#else
        fprintf(stderr, "[hiss][codec] LZ4 未编译 (需 -DHAS_LZ4)\n");
#endif

        // --- CRC32C (必须, INTERIM_BASELINE_NOT_FROZEN 候选) ---
        // 内置注册, Reader/Writer 通过 ChecksumRegistry::find() 共享同一实现
        {
            ChecksumEntry e;
            e.id      = ChecksumType::CRC32C;
            e.name    = "CRC32C";
            e.compute = crc32c_compute;
            checksums[e.id] = e;
            fprintf(stderr, "[hiss][codec] 内置注册: CRC32C (Castagnoli)\n");
        }
    }
};

// Meyers singleton 风格的全局状态访问点
// 首次调用时线程安全地构造 RegistryState (含内置 codec 注册)
static RegistryState& registry_state() {
    static RegistryState s;
    return s;
}

// ============================================================================
// CodecRegistry 实现
// ============================================================================

// 全局单例 (Meyers singleton, C++11 起局部静态初始化线程安全)
// 注: CodecRegistry 类在头文件中未声明私有成员与构造函数, 使用编译器隐式
// 默认构造; 注册表状态由 registry_state() 独立管理。
CodecRegistry& CodecRegistry::instance() {
    static CodecRegistry inst;
    // 触发注册表状态初始化 (含内置 codec 注册), 首次访问时执行
    (void)registry_state();
    return inst;
}

// 注册 codec
// RAW 为内置 codec, 不允许覆盖 (返回 <0)
// 其他 codec: 若已存在则覆盖 (更新实现)
// 返回 0=成功, <0=失败
int CodecRegistry::register_codec(const CodecEntry& entry) {
    if (entry.id == CodecId::RAW) {
        fprintf(stderr, "[hiss][codec] register_codec: RAW 为内置 codec, 不可覆盖\n");
        return -1;
    }
    if (!entry.compress || !entry.decompress || !entry.bound) {
        fprintf(stderr, "[hiss][codec] register_codec: codec 回调不完整 (compress/decompress/bound 不能为空)\n");
        return -2;
    }

    RegistryState& s = registry_state();
    std::lock_guard<std::mutex> lock(s.mutex);
    s.codecs[entry.id] = entry;  // 已存在则覆盖
    fprintf(stderr, "[hiss][codec] register_codec: 已注册 codec id=%u name=%s\n",
            (unsigned)entry.id, entry.name.c_str());
    return 0;
}

// 按 CodecId 查找 codec, 未找到返回 nullptr
const CodecEntry* CodecRegistry::find(CodecId id) const {
    RegistryState& s = registry_state();
    std::lock_guard<std::mutex> lock(s.mutex);
    auto it = s.codecs.find(id);
    if (it == s.codecs.end()) {
        return nullptr;
    }
    return &it->second;
}

// 列出所有已注册 codec 的 CodecId (用于 benchmark 遍历)
std::vector<CodecId> CodecRegistry::list() const {
    RegistryState& s = registry_state();
    std::lock_guard<std::mutex> lock(s.mutex);
    std::vector<CodecId> ids;
    ids.reserve(s.codecs.size());
    for (const auto& kv : s.codecs) {
        ids.push_back(kv.first);
    }
    return ids;
}

// ============================================================================
// ChecksumRegistry 实现 (类比 CodecRegistry, INTERIM_BASELINE_NOT_FROZEN)
// ============================================================================

// 全局单例 (Meyers singleton, C++11 起局部静态初始化线程安全)
// 注: ChecksumRegistry 类在头文件中未声明私有成员与构造函数, 使用编译器隐式
// 默认构造; 注册表状态由 registry_state() 独立管理 (与 CodecRegistry 共享)。
ChecksumRegistry& ChecksumRegistry::instance() {
    static ChecksumRegistry inst;
    // 触发注册表状态初始化 (含内置 CRC32C 注册), 首次访问时执行
    (void)registry_state();
    return inst;
}

// 注册 checksum
// NONE 为基线值, 不允许注册 (返回 <0)
// 其他 checksum: 若已存在则覆盖 (更新实现)
// 返回 0=成功, <0=失败
int ChecksumRegistry::register_checksum(const ChecksumEntry& entry) {
    if (entry.id == ChecksumType::NONE) {
        fprintf(stderr, "[hiss][codec] register_checksum: NONE 为基线值, 不可注册\n");
        return -1;
    }
    if (!entry.compute) {
        fprintf(stderr, "[hiss][codec] register_checksum: compute 回调为空\n");
        return -2;
    }

    RegistryState& s = registry_state();
    std::lock_guard<std::mutex> lock(s.mutex);
    s.checksums[entry.id] = entry;  // 已存在则覆盖
    fprintf(stderr, "[hiss][codec] register_checksum: 已注册 checksum id=%u name=%s\n",
            (unsigned)entry.id, entry.name.c_str());
    return 0;
}

// 按 ChecksumType 查找 checksum, 未找到返回 nullptr
const ChecksumEntry* ChecksumRegistry::find(ChecksumType id) const {
    RegistryState& s = registry_state();
    std::lock_guard<std::mutex> lock(s.mutex);
    auto it = s.checksums.find(id);
    if (it == s.checksums.end()) {
        return nullptr;
    }
    return &it->second;
}

// 列出所有已注册 checksum 的 ChecksumType (NONE 不在列表中)
std::vector<ChecksumType> ChecksumRegistry::list() const {
    RegistryState& s = registry_state();
    std::lock_guard<std::mutex> lock(s.mutex);
    std::vector<ChecksumType> ids;
    ids.reserve(s.checksums.size());
    for (const auto& kv : s.checksums) {
        ids.push_back(kv.first);
    }
    return ids;
}

} // namespace hiss
