#include "../include/aio_pipeline.h"
#include "../include/astro_image_io.h"
#include "aio_log.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstddef>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

/* ===========================================================================
 * aio_pipeline.cpp - 命名块容器 PipelineFrame 实现
 *
 * 设计要点:
 *   - PipelineFrame 是纯命名块容器，所有数据按块名索引
 *   - 块数据用 malloc 分配 (适配 add_block_move 接管外部 malloc 指针)
 *   - KV 块用 AioKVEntry 数组存储，支持动态扩容
 *   - 缓存文件 (.aio) 为紧凑二进制格式
 *   - XML 调试导出含 base64 编码数据
 *   - Windows 下路径按 UTF-8 -> wchar 转换后 _wfopen, 支持中文路径
 * =========================================================================== */

/* ============================================================================
 * 内部辅助函数
 * ========================================================================= */

static std::string base64_encode(const unsigned char* data, size_t len) {
    static const char table[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    for (size_t i = 0; i < len; i += 3) {
        unsigned int n = (unsigned int)data[i] << 16;
        if (i + 1 < len) n |= (unsigned int)data[i + 1] << 8;
        if (i + 2 < len) n |= (unsigned int)data[i + 2];
        out.push_back(table[(n >> 18) & 0x3F]);
        out.push_back(table[(n >> 12) & 0x3F]);
        out.push_back(i + 1 < len ? table[(n >> 6) & 0x3F] : '=');
        out.push_back(i + 2 < len ? table[n & 0x3F] : '=');
    }
    return out;
}

static std::string xml_escape(const char* s) {
    std::string out;
    if (!s) return out;
    for (const char* p = s; *p; ++p) {
        switch (*p) {
            case '<':  out += "&lt;";   break;
            case '>':  out += "&gt;";   break;
            case '&':  out += "&amp;";  break;
            case '"':  out += "&quot;"; break;
            default:   out.push_back(*p); break;
        }
    }
    return out;
}

static std::string xml_escape_bounded(const char* s, size_t maxlen) {
    std::string out;
    if (!s) return out;
    for (size_t i = 0; i < maxlen && s[i] != 0; ++i) {
        switch (s[i]) {
            case '<':  out += "&lt;";   break;
            case '>':  out += "&gt;";   break;
            case '&':  out += "&amp;";  break;
            case '"':  out += "&quot;"; break;
            default:   out.push_back(s[i]); break;
        }
    }
    return out;
}

static FILE* open_utf8_file(const char* path, const char* mode) {
    if (!path || !mode) return nullptr;
#ifdef _WIN32
    int wpath_len = MultiByteToWideChar(CP_UTF8, 0, path, -1, nullptr, 0);
    if (wpath_len <= 0) return nullptr;
    std::vector<wchar_t> wpath(wpath_len);
    MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath.data(), wpath_len);

    int wmode_len = MultiByteToWideChar(CP_UTF8, 0, mode, -1, nullptr, 0);
    if (wmode_len <= 0) return nullptr;
    std::vector<wchar_t> wmode(wmode_len);
    MultiByteToWideChar(CP_UTF8, 0, mode, -1, wmode.data(), wmode_len);

    return _wfopen(wpath.data(), wmode.data());
#else
    return std::fopen(path, mode);
#endif
}

/* 拷贝定长字符串到 char[N] (保证终止符) */
static void copy_str_to_fixed(char* dst, size_t dst_size, const char* src) {
    if (!dst || dst_size == 0) return;
    if (!src) { dst[0] = '\0'; return; }
    size_t len = std::strlen(src);
    if (len >= dst_size) len = dst_size - 1;
    std::memcpy(dst, src, len);
    dst[len] = '\0';
}

/* 每元素字节大小 (返回 0 表示未知类型) */
static size_t block_type_elem_size(AioBlockType type) {
    switch (type) {
        case AIO_BLOCK_FLOAT32: return sizeof(float);
        case AIO_BLOCK_FLOAT64: return sizeof(double);
        case AIO_BLOCK_INT32:   return sizeof(int32_t);
        case AIO_BLOCK_INT64:   return sizeof(int64_t);
        case AIO_BLOCK_KV:      return sizeof(AioKVEntry);
        case AIO_BLOCK_STRING:  return 1;  /* 字节数 */
        case AIO_BLOCK_RAW:     return 1;  /* 字节数 */
    }
    return 0;
}

/* 块数据总字节数 */
static size_t block_data_bytes(const AioBlock* blk) {
    if (!blk || !blk->data) return 0;
    size_t elem = block_type_elem_size(blk->type);
    return (size_t)blk->count * elem;
}

/* ============================================================================
 * 冻结校验器 (BLOCKER-DF-001/003/004: add_block 与 add_block_move 共用)
 * ============================================================================ */

/* 校验块参数 (不修改 frame)。返回 0=合法, 非0=失败原因。
 * out_elem_size: 输出每元素字节数 (合法时) */
static int validate_block_params(const char* name, AioBlockType type,
                                 int64_t count, const int* dims, int n_dims,
                                 const char* description, size_t* out_elem_size) {
    if (!name || name[0] == '\0' || std::strlen(name) > AIO_BLOCK_NAME_MAX) return 1;
    size_t elem = block_type_elem_size(type);
    if (elem == 0) return 2;                       // 未知类型
    if (count < 0) return 3;                       // 负 count
    if (n_dims < 0 || n_dims > AIO_CACHE_MAX_DIMS) return 4;
    if (dims) {
        for (int i = 0; i < n_dims; i++) {
            if (dims[i] <= 0) return 5;            // 非法维度
        }
    } else if (n_dims > 0) {
        return 5;
    }
    // 字节数乘法溢出 + 单块上限
    uint64_t bytes = (uint64_t)count * (uint64_t)elem;
    if (count > 0 && bytes / (uint64_t)elem != (uint64_t)count) return 6;  // 溢出
    if (bytes > (uint64_t)AIO_CACHE_MAX_BLOCK_BYTES) return 7;
    if (description && std::strlen(description) > AIO_CACHE_MAX_STR_LEN) return 8;
    if (out_elem_size) *out_elem_size = elem;
    return 0;
}

/* ============================================================================
 * ABI 握手与规范化分配器 (Dataflow_ABI_Contract)
 * ============================================================================ */

static const AstroAbiInfo g_abi_info = {
    1,                       /* abi_version */
    (uint32_t)sizeof(AstroAbiInfo),
    (uint32_t)sizeof(void*),
    0x5A1C0001u,             /* enum_fingerprint (冻结) */
    (uint32_t)sizeof(PipelineFrame),
    (uint32_t)sizeof(AioBlock),
    (uint32_t)sizeof(AioKVEntry),
    AIO_CAP_BASIC | AIO_CAP_CACHE_V1 | AIO_CAP_ALLOCATOR | AIO_CAP_FP64,
    "astro_image_io-1.0"
};

AIO_EXPORT const AstroAbiInfo* aio_abi_info(void) {
    return &g_abi_info;
}

AIO_EXPORT void* aio_alloc(size_t size) {
    return std::malloc(size ? size : 1);
}

AIO_EXPORT void* aio_realloc(void* ptr, size_t size) {
    return std::realloc(ptr, size ? size : 1);
}

AIO_EXPORT void aio_free(void* ptr) {
    std::free(ptr);
}

/* ============================================================================
 * 帧生命周期
 * ========================================================================= */

AIO_EXPORT PipelineFrame* aio_pipeline_frame_create(void) {
    PipelineFrame* frame = new (std::nothrow) PipelineFrame;
    if (!frame) {
        aio_log(AIO_LOG_ERROR, "PIPELINE", "frame_create: alloc failed");
        return nullptr;
    }
    frame->blocks = nullptr;
    frame->n_blocks = 0;
    frame->blocks_capacity = 0;
    frame->stages_completed = 0;
    aio_log(AIO_LOG_DEBUG, "PIPELINE", "frame_create: ok");
    return frame;
}

AIO_EXPORT void aio_pipeline_frame_destroy(PipelineFrame* frame) {
    if (!frame) return;
    /* 释放所有块数据 */
    for (int i = 0; i < frame->n_blocks; ++i) {
        if (frame->blocks[i].data) {
            std::free(frame->blocks[i].data);
            frame->blocks[i].data = nullptr;
        }
    }
    /* 释放块数组 */
    if (frame->blocks) {
        std::free(frame->blocks);
        frame->blocks = nullptr;
    }
    frame->n_blocks = 0;
    frame->blocks_capacity = 0;
    delete frame;
    aio_log(AIO_LOG_DEBUG, "PIPELINE", "frame_destroy: ok");
}

AIO_EXPORT size_t aio_pipeline_frame_memory_usage(const PipelineFrame* frame) {
    if (!frame) return 0;
    size_t total = 0;
    for (int i = 0; i < frame->n_blocks; ++i) {
        total += block_data_bytes(&frame->blocks[i]);
    }
    /* 块数组本身的内存 */
    total += (size_t)frame->blocks_capacity * sizeof(AioBlock);
    return total;
}

/* ============================================================================
 * 块管理 API
 * ========================================================================= */

/* 内部: 按名查找块索引 (返回 -1 表示未找到) */
static int find_block_index(const PipelineFrame* frame, const char* name) {
    if (!frame || !name) return -1;
    for (int i = 0; i < frame->n_blocks; ++i) {
        if (std::strncmp(frame->blocks[i].name, name, 64) == 0) {
            return i;
        }
    }
    return -1;
}

/* 内部: 扩容块数组 (容量翻倍) */
static int ensure_capacity(PipelineFrame* frame) {
    if (frame->n_blocks < frame->blocks_capacity) return 0;
    int new_cap = frame->blocks_capacity > 0 ? frame->blocks_capacity * 2 : 8;
    AioBlock* new_blocks = (AioBlock*)std::realloc(frame->blocks, (size_t)new_cap * sizeof(AioBlock));
    if (!new_blocks) {
        aio_log(AIO_LOG_ERROR, "PIPELINE", "ensure_capacity: realloc failed (new_cap=%d)", new_cap);
        return -1;
    }
    frame->blocks = new_blocks;
    frame->blocks_capacity = new_cap;
    return 0;
}

/* 内部: 初始化一个块的字段 (不含 data 拷贝) */
static void init_block_fields(AioBlock* blk, const char* name, AioBlockType type,
                               int64_t count, const int* dims, int n_dims,
                               const char* description) {
    std::memset(blk, 0, sizeof(AioBlock));
    copy_str_to_fixed(blk->name, sizeof(blk->name), name ? name : "");
    blk->type = type;
    blk->data = nullptr;
    blk->count = count;
    if (dims && n_dims > 0) {
        if (n_dims > 4) n_dims = 4;
        for (int i = 0; i < n_dims; ++i) blk->dims[i] = dims[i];
        blk->n_dims = n_dims;
    } else {
        blk->n_dims = 0;
    }
    copy_str_to_fixed(blk->description, sizeof(blk->description), description ? description : "");
}

AIO_EXPORT int aio_frame_add_block(PipelineFrame* frame,
    const char* name, AioBlockType type,
    const void* data, int64_t count,
    const int* dims, int n_dims,
    const char* description) {
    if (!frame || !name) {
        aio_log(AIO_LOG_ERROR, "PIPELINE", "add_block: frame or name is null");
        return 1;
    }
    size_t elem = 0;
    int rc = validate_block_params(name, type, count, dims, n_dims, description, &elem);
    if (rc != 0) {
        aio_log(AIO_LOG_ERROR, "PIPELINE", "add_block: 参数校验失败 rc=%d name='%s' type=%d",
                rc, name ? name : "", (int)type);
        return rc;   /* frame 完全不变 */
    }

    /* 先在临时块中构建并拷贝, 全部成功后再提交 (替换失败时原块保持不变) */
    AioBlock tmp;
    init_block_fields(&tmp, name, type, count, dims, n_dims, description);
    size_t total_bytes = (size_t)count * elem;
    if (total_bytes > 0) {
        tmp.data = std::malloc(total_bytes);
        if (!tmp.data) {
            aio_log(AIO_LOG_ERROR, "PIPELINE", "add_block: malloc failed (%zu bytes)", total_bytes);
            return 5;
        }
        if (data) {
            std::memcpy(tmp.data, data, total_bytes);
        } else {
            std::memset(tmp.data, 0, total_bytes);
        }
    }

    int idx = find_block_index(frame, name);
    if (idx >= 0) {
        if (frame->blocks[idx].data) std::free(frame->blocks[idx].data);
        frame->blocks[idx] = tmp;
    } else {
        if (ensure_capacity(frame) != 0) {
            std::free(tmp.data);
            return 3;
        }
        frame->blocks[frame->n_blocks++] = tmp;
    }
    aio_log(AIO_LOG_DEBUG, "PIPELINE", "add_block: '%s' type=%d count=%lld (%zu bytes)",
            name, (int)type, (long long)count, total_bytes);
    return 0;
}

AIO_EXPORT int aio_frame_add_block_move(PipelineFrame* frame,
    const char* name, AioBlockType type,
    void* data, int64_t count,
    const int* dims, int n_dims,
    const char* description) {
    if (!frame || !name) {
        aio_log(AIO_LOG_ERROR, "PIPELINE", "add_block_move: frame or name is null");
        return 1;
    }
    /* BLOCKER-DF-004: move 与 copy 共享同一 validator (type/count/dims/溢出) */
    size_t elem = 0;
    int rc = validate_block_params(name, type, count, dims, n_dims, description, &elem);
    if (rc != 0) {
        aio_log(AIO_LOG_ERROR, "PIPELINE", "add_block_move: 参数校验失败 rc=%d name='%s' type=%d",
                rc, name ? name : "", (int)type);
        return rc;   /* 不接管 data, frame 不变 */
    }
    size_t total_bytes = (size_t)count * elem;
    if (total_bytes > 0 && data == nullptr) {
        aio_log(AIO_LOG_ERROR, "PIPELINE", "add_block_move: data 为空但 count=%lld", (long long)count);
        return 5;
    }

    AioBlock tmp;
    init_block_fields(&tmp, name, type, count, dims, n_dims, description);
    tmp.data = data;  /* 接管所有权，不拷贝 */

    int idx = find_block_index(frame, name);
    if (idx >= 0) {
        if (frame->blocks[idx].data) std::free(frame->blocks[idx].data);
        frame->blocks[idx] = tmp;
    } else {
        if (ensure_capacity(frame) != 0) {
            /* 校验通过但扩容失败: 不接管 (调用方仍需释放 data) */
            return 3;
        }
        frame->blocks[frame->n_blocks++] = tmp;
    }
    aio_log(AIO_LOG_DEBUG, "PIPELINE", "add_block_move: '%s' type=%d count=%lld (moved ptr=%p)",
            name, (int)type, (long long)count, data);
    return 0;
}

AIO_EXPORT const AioBlock* aio_frame_get_block(const PipelineFrame* frame, const char* name) {
    int idx = find_block_index(frame, name);
    if (idx < 0) return nullptr;
    return &frame->blocks[idx];
}

AIO_EXPORT void* aio_frame_get_block_data(const PipelineFrame* frame, const char* name) {
    int idx = find_block_index(frame, name);
    if (idx < 0) return nullptr;
    return frame->blocks[idx].data;
}

AIO_EXPORT int64_t aio_frame_get_block_count(const PipelineFrame* frame, const char* name) {
    int idx = find_block_index(frame, name);
    if (idx < 0) return -1;
    return frame->blocks[idx].count;
}

AIO_EXPORT int aio_frame_get_block_type(const PipelineFrame* frame, const char* name) {
    int idx = find_block_index(frame, name);
    if (idx < 0) return -1;
    return (int)frame->blocks[idx].type;
}

AIO_EXPORT int aio_frame_remove_block(PipelineFrame* frame, const char* name) {
    if (!frame || !name) return 1;
    int idx = find_block_index(frame, name);
    if (idx < 0) return 1;

    /* 释放块数据 */
    if (frame->blocks[idx].data) {
        std::free(frame->blocks[idx].data);
    }
    /* 将后续块前移 */
    for (int i = idx; i < frame->n_blocks - 1; ++i) {
        frame->blocks[i] = frame->blocks[i + 1];
    }
    frame->n_blocks--;
    aio_log(AIO_LOG_DEBUG, "PIPELINE", "remove_block: '%s' removed (n_blocks=%d)",
            name, frame->n_blocks);
    return 0;
}

AIO_EXPORT int aio_frame_has_block(const PipelineFrame* frame, const char* name) {
    return find_block_index(frame, name) >= 0 ? 1 : 0;
}

AIO_EXPORT int aio_frame_list_blocks(const PipelineFrame* frame,
    char* out_names, int capacity, int* out_count) {
    if (!frame) {
        if (out_count) *out_count = 0;
        return 1;
    }
    int n = frame->n_blocks;
    if (out_count) *out_count = n;
    if (!out_names || capacity <= 0) {
        return n > 0 ? 2 : 0;  /* 缓冲区不足 */
    }
    for (int i = 0; i < n && i < capacity; ++i) {
        /* 每个块名占 64 字节 */
        char* dst = out_names + (size_t)i * 64;
        std::memset(dst, 0, 64);
        std::strncpy(dst, frame->blocks[i].name, 63);
    }
    return (n > capacity) ? 2 : 0;
}

/* ============================================================================
 * KV 块操作 API
 * ========================================================================= */

/* 内部: 查找或创建 KV 块，返回块指针 (失败返回 nullptr) */
static AioKVEntry* kv_get_or_create_entries(PipelineFrame* frame, const char* block_name,
                                              int64_t* out_count) {
    if (!frame || !block_name) return nullptr;
    int idx = find_block_index(frame, block_name);
    AioBlock* blk = nullptr;

    if (idx < 0) {
        /* 创建新的 KV 块，初始容量 8 条 */
        if (ensure_capacity(frame) != 0) return nullptr;
        idx = frame->n_blocks++;
        blk = &frame->blocks[idx];
        int init_cap = 8;
        int dims[1] = { init_cap };
        init_block_fields(blk, block_name, AIO_BLOCK_KV, init_cap, dims, 1, "KV block");
        blk->data = std::malloc((size_t)init_cap * sizeof(AioKVEntry));
        if (!blk->data) {
            frame->n_blocks--;
            return nullptr;
        }
        std::memset(blk->data, 0, (size_t)init_cap * sizeof(AioKVEntry));
        /* count 表示当前已使用条目数 (初始为 0) */
        blk->count = 0;
    } else {
        blk = &frame->blocks[idx];
        if (blk->type != AIO_BLOCK_KV) {
            aio_log(AIO_LOG_ERROR, "PIPELINE", "kv: block '%s' is not KV type", block_name);
            return nullptr;
        }
        /* 若未初始化 data，则分配 */
        if (!blk->data) {
            int init_cap = 8;
            blk->data = std::malloc((size_t)init_cap * sizeof(AioKVEntry));
            if (!blk->data) return nullptr;
            std::memset(blk->data, 0, (size_t)init_cap * sizeof(AioKVEntry));
            blk->count = 0;
            blk->dims[0] = init_cap;
            blk->n_dims = 1;
        }
    }

    if (out_count) *out_count = blk->count;
    return (AioKVEntry*)blk->data;
}

/* 内部: 扩容 KV 块 (翻倍) */
static int kv_ensure_capacity(PipelineFrame* /*frame*/, AioBlock* blk) {
    int64_t used = blk->count;
    int64_t cap = blk->n_dims > 0 ? blk->dims[0] : 0;
    if (used < cap) return 0;
    int64_t new_cap = cap > 0 ? cap * 2 : 8;
    AioKVEntry* new_data = (AioKVEntry*)std::realloc(blk->data, (size_t)new_cap * sizeof(AioKVEntry));
    if (!new_data) return -1;
    /* 清零新分配的部分 */
    std::memset(new_data + cap, 0, (size_t)(new_cap - cap) * sizeof(AioKVEntry));
    blk->data = new_data;
    blk->dims[0] = (int)new_cap;
    blk->n_dims = 1;
    return 0;
}

AIO_EXPORT int aio_frame_kv_set(PipelineFrame* frame, const char* block_name,
    const char* key, const char* value) {
    if (!key) return 1;
    int64_t used = 0;
    AioKVEntry* entries = kv_get_or_create_entries(frame, block_name, &used);
    if (!entries) return 2;

    AioBlock* blk = const_cast<AioBlock*>(aio_frame_get_block(frame, block_name));
    if (!blk) return 3;

    /* 查找是否已存在该 key */
    for (int64_t i = 0; i < used; ++i) {
        if (std::strncmp(entries[i].key, key, 64) == 0) {
            copy_str_to_fixed(entries[i].value, sizeof(entries[i].value), value ? value : "");
            return 0;
        }
    }
    /* 新增条目 */
    if (kv_ensure_capacity(frame, blk) != 0) return 4;
    entries = (AioKVEntry*)blk->data;  /* 可能 realloc 后指针变化 */
    int64_t idx = blk->count;
    copy_str_to_fixed(entries[idx].key, sizeof(entries[idx].key), key);
    copy_str_to_fixed(entries[idx].value, sizeof(entries[idx].value), value ? value : "");
    blk->count = idx + 1;
    return 0;
}

AIO_EXPORT const char* aio_frame_kv_get(const PipelineFrame* frame, const char* block_name,
    const char* key) {
    if (!frame || !block_name || !key) return nullptr;
    int idx = find_block_index(frame, block_name);
    if (idx < 0) return nullptr;
    const AioBlock* blk = &frame->blocks[idx];
    if (blk->type != AIO_BLOCK_KV || !blk->data) return nullptr;
    const AioKVEntry* entries = (const AioKVEntry*)blk->data;
    for (int64_t i = 0; i < blk->count; ++i) {
        if (std::strncmp(entries[i].key, key, 64) == 0) {
            return entries[i].value;
        }
    }
    return nullptr;
}

AIO_EXPORT int aio_frame_kv_set_double(PipelineFrame* frame, const char* block_name,
    const char* key, double value) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.17g", value);
    return aio_frame_kv_set(frame, block_name, key, buf);
}

AIO_EXPORT double aio_frame_kv_get_double(const PipelineFrame* frame, const char* block_name,
    const char* key, double default_value) {
    const char* val = aio_frame_kv_get(frame, block_name, key);
    if (!val) return default_value;
    char* end = nullptr;
    double d = std::strtod(val, &end);
    if (end == val) return default_value;  /* 转换失败 */
    return d;
}

/* ============================================================================
 * 缓存文件 (.aio) 读写
 * ========================================================================= */

#define AIO_CACHE_MAGIC "AIO1"
#define AIO_CACHE_VERSION 1

/* 内部: 写入二进制块到文件 */
template<typename T>
static int write_bin(FILE* fp, const T& val) {
    return std::fwrite(&val, sizeof(T), 1, fp) == 1 ? 0 : -1;
}

static int write_bytes(FILE* fp, const void* data, size_t len) {
    return std::fwrite(data, 1, len, fp) == len ? 0 : -1;
}

static int write_str_with_len(FILE* fp, const char* s) {
    if (!s) {
        int32_t len = 0;
        return write_bin(fp, len);
    }
    int32_t len = (int32_t)std::strlen(s);
    if (write_bin(fp, len) != 0) return -1;
    if (len > 0) return write_bytes(fp, s, (size_t)len);
    return 0;
}

/* 内部: 读取二进制块 */
template<typename T>
static int read_bin(FILE* fp, T& val) {
    return std::fread(&val, sizeof(T), 1, fp) == 1 ? 0 : -1;
}

static int read_bytes(FILE* fp, void* data, size_t len) {
    return std::fread(data, 1, len, fp) == len ? 0 : -1;
}

/* 读取字符串: 返回长度 (>=0), -1=读失败, -2=长度非法/超限 (调用方决定失败,
 * 不静默截断 — BLOCKER-DF-001: 超长字段必须硬失败, 防止流错位) */
static int read_str_with_len_strict(FILE* fp, char* buf, size_t buf_size) {
    int32_t len = 0;
    if (read_bin(fp, len) != 0) return -1;
    if (len < 0 || (size_t)len >= buf_size) return -2;
    if (len > 0 && read_bytes(fp, buf, (size_t)len) != 0) return -1;
    buf[len] = '\0';
    return (int)len;
}

/* 内部: 序列化单个块的数据 (KV 块特殊处理) */
static int serialize_block_data(FILE* fp, const AioBlock* blk) {
    if (blk->type == AIO_BLOCK_KV) {
        /* KV 块: 连续的 [Key_Len][Key][Val_Len][Val] 条目 */
        const AioKVEntry* entries = (const AioKVEntry*)blk->data;
        for (int64_t i = 0; i < blk->count; ++i) {
            if (write_str_with_len(fp, entries[i].key) != 0) return -1;
            if (write_str_with_len(fp, entries[i].value) != 0) return -1;
        }
        return 0;
    } else {
        /* 其他类型: 直接写原始字节 */
        size_t bytes = block_data_bytes(blk);
        if (bytes > 0) {
            return write_bytes(fp, blk->data, bytes);
        }
        return 0;
    }
}

AIO_EXPORT int aio_frame_save_cache(const PipelineFrame* frame, const char* path) {
    if (!frame || !path) {
        aio_log(AIO_LOG_ERROR, "PIPELINE", "save_cache: frame or path is null");
        return 1;
    }
    /* 原子保存: 先写临时文件, flush 后 rename (中断不留下可误认的正式文件) */
    std::string tmp_path = std::string(path) + ".tmp";
    FILE* fp = open_utf8_file(tmp_path.c_str(), "wb");
    if (!fp) {
        aio_log(AIO_LOG_ERROR, "PIPELINE", "save_cache: open file failed: %s", path);
        return 2;
    }

    /* 头部 */
    if (write_bytes(fp, AIO_CACHE_MAGIC, 4) != 0) goto fail;
    if (write_bin(fp, (int32_t)AIO_CACHE_VERSION) != 0) goto fail;
    if (write_bin(fp, (int32_t)frame->n_blocks) != 0) goto fail;
    if (write_bin(fp, (int32_t)frame->stages_completed) != 0) goto fail;

    /* 逐块序列化 */
    for (int i = 0; i < frame->n_blocks; ++i) {
        const AioBlock* blk = &frame->blocks[i];
        if (write_str_with_len(fp, blk->name) != 0) goto fail;
        if (write_bin(fp, (int32_t)blk->type) != 0) goto fail;
        if (write_bin(fp, (int32_t)blk->n_dims) != 0) goto fail;
        for (int d = 0; d < blk->n_dims; ++d) {
            if (write_bin(fp, (int32_t)blk->dims[d]) != 0) goto fail;
        }
        if (write_bin(fp, (int64_t)blk->count) != 0) goto fail;
        if (serialize_block_data(fp, blk) != 0) goto fail;
        if (write_str_with_len(fp, blk->description) != 0) goto fail;
    }

    if (std::fflush(fp) != 0) {
        std::fclose(fp);
        std::remove(tmp_path.c_str());
        return 4;
    }
    std::fclose(fp);
#ifdef _WIN32
    if (!MoveFileExA(tmp_path.c_str(), path, MOVEFILE_REPLACE_EXISTING)) {
        aio_log(AIO_LOG_ERROR, "PIPELINE", "save_cache: rename failed: %s", path);
        std::remove(tmp_path.c_str());
        return 5;
    }
#else
    if (std::rename(tmp_path.c_str(), path) != 0) {
        aio_log(AIO_LOG_ERROR, "PIPELINE", "save_cache: rename failed: %s", path);
        std::remove(tmp_path.c_str());
        return 5;
    }
#endif
    aio_log(AIO_LOG_INFO, "PIPELINE", "save_cache: ok -> %s (%d blocks)",
            path, frame->n_blocks);
    return 0;

fail:
    std::fclose(fp);
    std::remove(tmp_path.c_str());
    aio_log(AIO_LOG_ERROR, "PIPELINE", "save_cache: write failed");
    return 3;
}

/* 内部: 完整解析缓存到临时 frame (两阶段提交第一阶段)。
 * 任何非法输入立即失败; 失败时临时 frame 由调用方销毁, 目标 frame 不变。 */
static int load_cache_parse(PipelineFrame* frame, const char* path) {
    FILE* fp = open_utf8_file(path, "rb");
    if (!fp) {
        aio_log(AIO_LOG_ERROR, "PIPELINE", "load_cache: open file failed: %s", path);
        return 2;
    }

    char magic[4];
    int32_t version = 0;
    int32_t n_blocks = 0;
    int32_t stages = 0;
    bool failed = false;

    if (read_bytes(fp, magic, 4) != 0 || std::memcmp(magic, AIO_CACHE_MAGIC, 4) != 0) {
        aio_log(AIO_LOG_ERROR, "PIPELINE", "load_cache: bad magic");
        failed = true;
    }
    if (!failed && read_bin(fp, version) != 0) {
        failed = true;
    }
    if (!failed && version != AIO_CACHE_VERSION) {
        failed = true;
    }
    if (!failed && read_bin(fp, n_blocks) != 0) {
        failed = true;
    }
    /* BLOCKER-DF-003: 负/超大 block 数量必须拒绝 */
    if (!failed && (n_blocks < 0 || n_blocks > AIO_CACHE_MAX_BLOCKS)) {
        failed = true;
    }
    if (!failed && read_bin(fp, stages) != 0) {
        failed = true;
    }
    if (failed) {
        std::fclose(fp);
        return 3;
    }

    frame->stages_completed = stages;
    if (n_blocks > 0) {
        frame->blocks = (AioBlock*)std::malloc((size_t)n_blocks * sizeof(AioBlock));
        if (!frame->blocks) {
            std::fclose(fp);
            return 3;
        }
        frame->blocks_capacity = n_blocks;
        std::memset(frame->blocks, 0, (size_t)n_blocks * sizeof(AioBlock));
    }

    uint64_t total_frame_bytes = 0;
    for (int i = 0; i < n_blocks && !failed; ++i) {
        AioBlock* blk = &frame->blocks[i];
        char name[256];
        int name_len = read_str_with_len_strict(fp, name, sizeof(name));
        if (name_len < 0 || name_len > AIO_BLOCK_NAME_MAX) { failed = true; break; }
        int32_t type_val = 0;
        if (read_bin(fp, type_val) != 0) { failed = true; break; }
        size_t elem = block_type_elem_size((AioBlockType)type_val);
        if (elem == 0) { failed = true; break; }             // 未知类型
        int32_t n_dims = 0;
        if (read_bin(fp, n_dims) != 0) { failed = true; break; }
        if (n_dims < 0 || n_dims > AIO_CACHE_MAX_DIMS) { failed = true; break; }  // 不截断
        int dims[4] = {0, 0, 0, 0};
        for (int d = 0; d < n_dims; ++d) {
            int32_t dim_val = 0;
            if (read_bin(fp, dim_val) != 0) { failed = true; break; }
            if (dim_val <= 0) { failed = true; break; }
            dims[d] = dim_val;
        }
        if (failed) break;
        int64_t count = 0;
        if (read_bin(fp, count) != 0) { failed = true; break; }
        if (count < 0) { failed = true; break; }
        uint64_t bytes = (uint64_t)count * (uint64_t)elem;
        if (count > 0 && bytes / (uint64_t)elem != (uint64_t)count) { failed = true; break; }
        if (bytes > (uint64_t)AIO_CACHE_MAX_BLOCK_BYTES ||
            total_frame_bytes + bytes > (uint64_t)AIO_CACHE_MAX_FRAME_BYTES) {
            failed = true; break;
        }
        for (int j = 0; j < i; ++j) {                        // 重复 name
            if (std::strncmp(frame->blocks[j].name, name, 64) == 0) { failed = true; break; }
        }
        if (failed) break;

        init_block_fields(blk, name, (AioBlockType)type_val, count, dims, n_dims, "");
        if (blk->type == AIO_BLOCK_KV) {
            int64_t data_size = count;
            int64_t cap = data_size > 0 ? data_size : 8;
            if ((uint64_t)cap * sizeof(AioKVEntry) > (uint64_t)AIO_CACHE_MAX_BLOCK_BYTES) {
                failed = true; break;
            }
            AioKVEntry* entries = (AioKVEntry*)std::malloc((size_t)cap * sizeof(AioKVEntry));
            if (!entries) { failed = true; break; }
            std::memset(entries, 0, (size_t)cap * sizeof(AioKVEntry));
            bool kv_ok = true;
            for (int64_t k = 0; k < data_size && kv_ok; ++k) {
                char key[256], value[512];
                int key_len = read_str_with_len_strict(fp, key, sizeof(key));
                int val_len = read_str_with_len_strict(fp, value, sizeof(value));
                if (key_len < 0 || val_len < 0 || key_len > 63 || val_len > 255) {
                    kv_ok = false;
                } else {
                    copy_str_to_fixed(entries[k].key, sizeof(entries[k].key), key);
                    copy_str_to_fixed(entries[k].value, sizeof(entries[k].value), value);
                }
            }
            if (!kv_ok) { std::free(entries); failed = true; break; }
            blk->data = entries;
            blk->dims[0] = (int)cap;
            blk->n_dims = 1;
        } else if (bytes > 0) {
            void* buf = std::malloc((size_t)bytes);
            if (!buf) { failed = true; break; }
            if (read_bytes(fp, buf, (size_t)bytes) != 0) {
                std::free(buf);
                failed = true;
                break;
            }
            blk->data = buf;
        }
        total_frame_bytes += bytes;

        char desc[256];
        int desc_len = read_str_with_len_strict(fp, desc, sizeof(desc));
        if (desc_len < 0 || desc_len > 127) { failed = true; break; }
        copy_str_to_fixed(blk->description, sizeof(blk->description), desc);
        frame->n_blocks++;
    }

    std::fclose(fp);
    if (failed) {
        for (int i = 0; i < frame->n_blocks; ++i) {
            if (frame->blocks[i].data) std::free(frame->blocks[i].data);
        }
        if (frame->blocks) std::free(frame->blocks);
        frame->blocks = nullptr;
        frame->n_blocks = 0;
        frame->blocks_capacity = 0;
        return 3;
    }
    return 0;
}

AIO_EXPORT int aio_frame_load_cache(PipelineFrame* frame, const char* path) {
    if (!frame || !path) {
        aio_log(AIO_LOG_ERROR, "PIPELINE", "load_cache: frame or path is null");
        return 1;
    }
    /* BLOCKER-DF-002: 两阶段提交 — 解析到临时帧, 成功后再原子交换;
     * 失败时原 frame 字节级不变。 */
    PipelineFrame* tmp = aio_pipeline_frame_create();
    if (!tmp) return 2;
    int rc = load_cache_parse(tmp, path);
    if (rc != 0) {
        aio_pipeline_frame_destroy(tmp);
        return rc;
    }
    for (int i = 0; i < frame->n_blocks; ++i) {
        if (frame->blocks[i].data) std::free(frame->blocks[i].data);
    }
    if (frame->blocks) std::free(frame->blocks);
    *frame = *tmp;
    tmp->blocks = nullptr;
    tmp->n_blocks = 0;
    tmp->blocks_capacity = 0;
    aio_pipeline_frame_destroy(tmp);
    return 0;
}

/* ============================================================================
 * 调试导出: XML / FITS
 * ========================================================================= */

/* 内部: 生成单个块的 XML 描述 (元数据 + base64 数据) */
static std::string block_to_xml(const AioBlock* blk, const char* block_name_override,
                                  bool skip_data) {
    const char* name = block_name_override ? block_name_override : blk->name;
    std::string xml;
    xml += "  <Block name=\"";
    xml += xml_escape(name);
    xml += "\" type=\"";

    const char* type_str = "unknown";
    switch (blk->type) {
        case AIO_BLOCK_FLOAT32: type_str = "FLOAT32"; break;
        case AIO_BLOCK_FLOAT64: type_str = "FLOAT64"; break;
        case AIO_BLOCK_INT32:   type_str = "INT32";   break;
        case AIO_BLOCK_INT64:   type_str = "INT64";   break;
        case AIO_BLOCK_STRING:  type_str = "STRING";  break;
        case AIO_BLOCK_KV:      type_str = "KV";      break;
        case AIO_BLOCK_RAW:     type_str = "RAW";     break;
    }
    xml += type_str;
    xml += "\" count=\"";
    xml += std::to_string((long long)blk->count);
    xml += "\" n_dims=\"";
    xml += std::to_string(blk->n_dims);
    xml += "\"";

    if (blk->n_dims > 0) {
        xml += " dims=\"[";
        for (int i = 0; i < blk->n_dims; ++i) {
            if (i > 0) xml += ",";
            xml += std::to_string(blk->dims[i]);
        }
        xml += "]\"";
    }

    if (blk->description[0]) {
        xml += " description=\"";
        xml += xml_escape_bounded(blk->description, sizeof(blk->description));
        xml += "\"";
    }
    xml += ">\n";

    /* 数据 */
    if (blk->type == AIO_BLOCK_KV && blk->data) {
        const AioKVEntry* entries = (const AioKVEntry*)blk->data;
        for (int64_t i = 0; i < blk->count; ++i) {
            xml += "    <KV key=\"";
            xml += xml_escape_bounded(entries[i].key, sizeof(entries[i].key));
            xml += "\" value=\"";
            xml += xml_escape_bounded(entries[i].value, sizeof(entries[i].value));
            xml += "\"/>\n";
        }
    } else if (!skip_data && blk->data && blk->count > 0) {
        size_t bytes = block_data_bytes(blk);
        std::string b64 = base64_encode(
            reinterpret_cast<const unsigned char*>(blk->data), bytes);
        xml += "    <Data encoding=\"base64\">";
        xml += b64;
        xml += "</Data>\n";
    } else if (skip_data) {
        xml += "    <Data encoding=\"base64\" skipped=\"true\"/>\n";
    }

    xml += "  </Block>\n";
    return xml;
}

AIO_EXPORT int aio_frame_export_block_xml(const PipelineFrame* frame,
    const char* block_name, const char* path) {
    if (!frame || !block_name || !path) return 1;
    const AioBlock* blk = aio_frame_get_block(frame, block_name);
    if (!blk) {
        aio_log(AIO_LOG_ERROR, "PIPELINE", "export_block_xml: block '%s' not found", block_name);
        return 2;
    }

    std::string xml;
    xml += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    xml += "<PipelineFrame>\n";
    xml += block_to_xml(blk, nullptr, false);
    xml += "</PipelineFrame>\n";

    FILE* fp = open_utf8_file(path, "wb");
    if (!fp) {
        aio_log(AIO_LOG_ERROR, "PIPELINE", "export_block_xml: open file failed: %s", path);
        return 3;
    }
    size_t written = std::fwrite(xml.data(), 1, xml.size(), fp);
    std::fclose(fp);
    if (written != xml.size()) return 4;
    aio_log(AIO_LOG_INFO, "PIPELINE", "export_block_xml: '%s' -> %s (%zu bytes)",
            block_name, path, xml.size());
    return 0;
}

AIO_EXPORT int aio_frame_export_all_xml(const PipelineFrame* frame, const char* path) {
    if (!frame || !path) {
        aio_log(AIO_LOG_ERROR, "PIPELINE", "export_all_xml: frame or path is null");
        return 1;
    }

    std::string xml;
    xml += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    xml += "<PipelineFrame stages_completed=\"";
    xml += std::to_string(frame->stages_completed);
    xml += "\" n_blocks=\"";
    xml += std::to_string(frame->n_blocks);
    xml += "\">\n";

    for (int i = 0; i < frame->n_blocks; ++i) {
        xml += block_to_xml(&frame->blocks[i], nullptr, false);
    }

    xml += "</PipelineFrame>\n";

    FILE* fp = open_utf8_file(path, "wb");
    if (!fp) {
        aio_log(AIO_LOG_ERROR, "PIPELINE", "export_all_xml: open file failed: %s", path);
        return 2;
    }
    size_t written = std::fwrite(xml.data(), 1, xml.size(), fp);
    std::fclose(fp);
    if (written != xml.size()) return 3;
    aio_log(AIO_LOG_INFO, "PIPELINE", "export_all_xml: ok -> %s (%zu bytes, %d blocks)",
            path, xml.size(), frame->n_blocks);
    return 0;
}

/* 旧版兼容包装 */
AIO_EXPORT int aio_pipeline_export_xml(const PipelineFrame* frame,
    const char* path, const char* comment) {
    (void)comment;  /* comment 参数忽略，保留是为了向后兼容 */
    return aio_frame_export_all_xml(frame, path);
}

/* FITS 导出: 简化版本，写入裸二进制 + 元数据头 */
/* 注: 不依赖 cfitsio，使用简单的 FITS 2880 字节块格式 */
AIO_EXPORT int aio_frame_export_block_fits(const PipelineFrame* frame,
    const char* block_name, const char* path) {
    if (!frame || !block_name || !path) return 1;
    const AioBlock* blk = aio_frame_get_block(frame, block_name);
    if (!blk) {
        aio_log(AIO_LOG_ERROR, "PIPELINE", "export_block_fits: block '%s' not found", block_name);
        return 2;
    }
    if (!blk->data || blk->count <= 0) {
        aio_log(AIO_LOG_ERROR, "PIPELINE", "export_block_fits: block '%s' has no data", block_name);
        return 3;
    }

    /* 确定 BITPIX 和数据类型 */
    int bitpix = 0;
    size_t elem_size = 0;
    const char* bzero_str = "";
    double bzero = 0.0;
    switch (blk->type) {
        case AIO_BLOCK_FLOAT32:
            bitpix = -32; elem_size = 4; break;
        case AIO_BLOCK_FLOAT64:
            bitpix = -64; elem_size = 8; break;
        case AIO_BLOCK_INT32:
            bitpix = 32; elem_size = 4; break;
        case AIO_BLOCK_INT64:
            bitpix = 64; elem_size = 8; break;
        default:
            aio_log(AIO_LOG_ERROR, "PIPELINE", "export_block_fits: block '%s' type %d not supported",
                    block_name, (int)blk->type);
            return 4;
    }

    /* 确定 NAXIS */
    int naxis = blk->n_dims > 0 ? blk->n_dims : 1;
    if (naxis > 3) naxis = 3;
    int naxis1 = 1, naxis2 = 1, naxis3 = 1;
    if (blk->n_dims >= 1) naxis1 = blk->dims[0];
    if (blk->n_dims >= 2) naxis2 = blk->dims[1];
    if (blk->n_dims >= 3) naxis3 = blk->dims[2];
    /* 若 n_dims==0，按 1D 处理 */
    if (blk->n_dims == 0) {
        naxis = 1;
        naxis1 = (int)blk->count;
    }
    /* FITS 维度顺序: NAXIS1 是最快变化的维度 (通常是 width)
     * 我们的 dims[0] 通常是 H (行数)，需要交换 */
    /* 简化处理: 直接按 dims 顺序写入，NAXIS1=dims[0] */
    int64_t total_elems = blk->count;
    size_t data_bytes = (size_t)total_elems * elem_size;

    FILE* fp = open_utf8_file(path, "wb");
    if (!fp) {
        aio_log(AIO_LOG_ERROR, "PIPELINE", "export_block_fits: open file failed: %s", path);
        return 5;
    }

    /* 写 FITS 头 (2880 字节块) */
    char header[2880];
    std::memset(header, ' ', sizeof(header));
    int pos = 0;
    auto write_card = [&](const char* key, const char* val) {
        int n = std::snprintf(header + pos, 81, "%-8s= %-70s", key, val);
        if (n > 0) pos += 80;
    };
    auto write_card_str = [&](const char* key, const char* val) {
        int n = std::snprintf(header + pos, 81, "%-8s= '%-68s'", key, val);
        if (n > 0) pos += 80;
    };
    /* SIMPLE/EXTEND: 逻辑值 T (无引号), astropy 要求 T 在 column 11 */
    write_card("SIMPLE", "T");
    write_card("BITPIX", std::to_string(bitpix).c_str());
    write_card("NAXIS", std::to_string(naxis).c_str());
    if (naxis >= 1) write_card("NAXIS1", std::to_string(naxis1).c_str());
    if (naxis >= 2) write_card("NAXIS2", std::to_string(naxis2).c_str());
    if (naxis >= 3) write_card("NAXIS3", std::to_string(naxis3).c_str());
    if (bzero != 0.0) write_card("BZERO", std::to_string(bzero).c_str());
    write_card("EXTEND", "T");
    write_card_str("BLOCK_NA", block_name);
    /* END 卡片 */
    int end_pos = pos;
    std::snprintf(header + end_pos, 81, "%-80s", "END");
    /* 写入头 (2880 字节) */
    std::fwrite(header, 1, 2880, fp);

    /* 写数据 (FITS 标准要求大端字节序, x86 主机为小端, 需逐元素反转字节) */
    if (elem_size == 4 || elem_size == 8) {
        std::vector<char> be_buf(data_bytes);
        const char* src = static_cast<const char*>(blk->data);
        char* dst = be_buf.data();
        for (int64_t i = 0; i < total_elems; ++i) {
            for (size_t b = 0; b < elem_size; ++b) {
                dst[i * elem_size + b] = src[i * elem_size + (elem_size - 1 - b)];
            }
        }
        std::fwrite(be_buf.data(), 1, data_bytes, fp);
    } else {
        std::fwrite(blk->data, 1, data_bytes, fp);
    }
    size_t pad = (2880 - (data_bytes % 2880)) % 2880;
    if (pad > 0) {
        std::vector<char> zeros(pad, 0);
        std::fwrite(zeros.data(), 1, pad, fp);
    }

    std::fclose(fp);
    aio_log(AIO_LOG_INFO, "PIPELINE", "export_block_fits: '%s' -> %s (BITPIX=%d, %lld elems, %zu bytes)",
            block_name, path, bitpix, (long long)total_elems, data_bytes);
    (void)bzero_str;
    return 0;
}
