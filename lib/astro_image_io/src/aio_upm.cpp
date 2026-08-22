// lib/astro_image_io/src/aio_upm.cpp — UPM 模型文件容器实现
//
// 容器层：稀疏 JSON 原子写/读 + 稠密缓存（固定头部 + 二进制块 + checksum）。
// 科学语义不在此层（phase2 负责）；本层保证"所有科学模型 I/O 走唯一 AIO"。
// 持久化契约锚点 SCI-UPM-PERSIST-001/ALG-UPM-FRAME-BIND-001/DATA-UPM-MODEL-001：
// sparse 模型权威形态经 aio_upm_write_sparse 原子持久化→aio_upm_open 重开，
// frame_id→theta 绑定由 phase2 层 save/open 显式 frames[]保证，容器不重排。
#include "aio_upm.h"

#include "crypto/sha256.h"

#include <nlohmann/json.hpp>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <stdio.h>
// MinGW/MSVC 64-bit seek
#ifndef AIO_FSEEK
#define AIO_FSEEK _fseeki64
#endif
#else
#ifndef AIO_FSEEK
#define AIO_FSEEK fseeko
#endif
#endif

namespace {

thread_local std::string g_upm_error;

void set_err(const std::string& m) { g_upm_error = m; }

constexpr int kDenseHeaderBytes = 512;

} // namespace

struct AioUpmSparse {
    std::string content;
    nlohmann::json json;
};

struct AioUpmDense {
    std::FILE* f = nullptr;
    std::string path;
    std::string source_hash;
    int target_order = 0;
    std::uint32_t precision = 0;
    std::uint64_t frame_count = 0;
    std::uint64_t tile_count = 0;
    std::vector<std::uint64_t> tiles;   // 写入顺序去重收集
    std::uint64_t current_frame = 0;    // 写入推进
    std::uint64_t tile_bytes = 0;       // 单 tile 值字节数
};

constexpr std::uint64_t kLeafPerTile = 512ull * 512ull;

extern "C" {

int aio_upm_write_sparse(const char* path, const char* model_json) {
    g_upm_error.clear();
    if (!path || !model_json) {
        set_err("invalid args");
        return 1;
    }
    // 校验 JSON 可解析 + format 字段
    try {
        auto j = nlohmann::json::parse(model_json);
        if (j.value("format", std::string()) != "astrocs-upm-v1" &&
            j.value("format", std::string()) != "astrocs-upm-v2") {
            set_err("format != astrocs-upm-v1/v2");
            return 1;
        }
    } catch (const std::exception& e) {
        set_err(std::string("json parse: ") + e.what());
        return 1;
    }
    // （ENG-IO-001）：science 模型写盘走 temp → validate →
    // atomic promote；失败清理 temp，绝不留下半成品当正式模型。
    const std::string tmp = std::string(path) + ".tmp";
    std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
    if (!f) {
        set_err("cannot open temp file for write");
        return 1;
    }
    f.write(model_json, std::streamsize(std::strlen(model_json)));
    f.flush();
    if (!f.good()) {
        f.close();
        std::remove(tmp.c_str());
        set_err("write failed");
        return 1;
    }
    f.close();
    if (std::rename(tmp.c_str(), path) != 0) {
        // Windows rename 不覆盖已存在目标：删除旧正式模型后重试一次
        // （temp 仍完整，失败可恢复；ENG-IO-001 单文件原子语义保持）
        if (std::remove(path) == 0 &&
            std::rename(tmp.c_str(), path) == 0) {
            return 0;
        }
        std::remove(tmp.c_str());
        set_err("atomic promote failed");
        return 1;
    }
    return 0;
}

AioUpmSparse* aio_upm_open(const char* path) {
    g_upm_error.clear();
    if (!path) {
        set_err("invalid args");
        return nullptr;
    }
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        set_err("cannot open sparse model");
        return nullptr;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    std::unique_ptr<AioUpmSparse> m(new AioUpmSparse);
    m->content = ss.str();
    try {
        m->json = nlohmann::json::parse(m->content);
        if (m->json.value("format", std::string()) != "astrocs-upm-v1" &&
            m->json.value("format", std::string()) != "astrocs-upm-v2") {
            set_err("format != astrocs-upm-v1/v2");
            return nullptr;
        }
    } catch (const std::exception& e) {
        set_err(std::string("json parse: ") + e.what());
        return nullptr;
    }
    return m.release();
}

int aio_upm_read_info(AioUpmSparse* f, std::uint32_t* version,
                      std::uint32_t* precision, std::uint32_t* target_order,
                      std::uint64_t* control_count,
                      std::uint64_t* observation_count, char* model_hash,
                      int hash_buf_size) {
    if (!f) return 1;
    const auto& j = f->json;
    if (version) *version = j.value("version", 0u);
    if (precision) *precision = j.value("precision", 0u);
    if (target_order) *target_order = j.value("target_order", 0u);
    if (control_count) *control_count = j.value("control_count", 0ull);
    if (observation_count)
        *observation_count = j.value("observation_count", 0ull);
    if (model_hash && hash_buf_size > 0) {
        const std::string h = j.value("model_hash", std::string());
        std::strncpy(model_hash, h.c_str(), (size_t)hash_buf_size - 1);
        model_hash[hash_buf_size - 1] = '\0';
    }
    return 0;
}

int aio_upm_read_all(AioUpmSparse* f, char* buf, int buf_size) {
    if (!f || !buf || buf_size <= 0) return 1;
    if ((int)f->content.size() + 1 > buf_size) {
        set_err("buffer too small");
        return -1;
    }
    std::memcpy(buf, f->content.data(), f->content.size());
    buf[f->content.size()] = '\0';
    return 0;
}

int aio_upm_read_all_dynamic(AioUpmSparse* f, char** out, std::size_t* out_len) {
    if (!f || !out || !out_len) return 1;
    char* buf = new char[f->content.size() + 1];
    std::memcpy(buf, f->content.data(), f->content.size());
    buf[f->content.size()] = '\0';
    *out = buf;
    *out_len = f->content.size();
    return 0;
}

void aio_upm_close(AioUpmSparse* f) {
    delete f;
}

// ===== 稠密缓存 =====

AioUpmDense* aio_upm_dense_begin(const char* path, const char* source_hash,
                                 int target_order, std::uint32_t precision,
                                 std::uint64_t frame_count,
                                 std::uint64_t tile_count) {
    g_upm_error.clear();
    if (!path || !source_hash || std::strlen(source_hash) != 64 ||
        frame_count == 0 || tile_count == 0 ||
        (precision != 0 && precision != 1)) {
        set_err("invalid args");
        return nullptr;
    }
    std::unique_ptr<AioUpmDense> d(new AioUpmDense);
    d->path = path;
    d->source_hash = source_hash;
    d->target_order = target_order;
    d->precision = precision;
    d->frame_count = frame_count;
    d->tile_count = tile_count;
    d->tile_bytes = kLeafPerTile * (precision == 1 ? sizeof(double)
                                                   : sizeof(float));
    d->f = std::fopen(path, "wb");
    if (!d->f) {
        set_err("cannot open dense cache for write");
        return nullptr;
    }
    // 固定 512B 头部行：checksum 槽先 64 个 '0'
    char zeros[65];
    std::memset(zeros, '0', 64);
    zeros[64] = '\0';
    char header[kDenseHeaderBytes + 2] = {0};
    std::snprintf(header, sizeof(header),
                  "{\"format\":\"astrocs-upm-dense-v2\",\"source_hash\":\"%s\","
                  "\"target_order\":%d,\"precision\":%u,"
                  "\"frame_count\":%llu,\"tile_count\":%llu,"
                  "\"leaf_order\":%d,\"checksum\":\"%s\"}",
                  source_hash, target_order, precision,
                  (unsigned long long)frame_count,
                  (unsigned long long)tile_count, target_order + 9, zeros);
    const int len = (int)std::strlen(header);
    if (len > kDenseHeaderBytes) {
        std::fclose(d->f);
        set_err("header exceeds 512B");
        return nullptr;
    }
    std::memset(header + len, ' ', (size_t)kDenseHeaderBytes - (size_t)len);
    header[kDenseHeaderBytes] = '\n';
    if (std::fwrite(header, 1, kDenseHeaderBytes + 1, d->f) !=
        (size_t)kDenseHeaderBytes + 1) {
        std::fclose(d->f);
        set_err("header write failed");
        return nullptr;
    }
    // tile 表占位（tile_count × u64，dense_end 回填）
    std::vector<std::uint64_t> zero_tiles(tile_count, 0);
    if (std::fwrite(zero_tiles.data(), sizeof(std::uint64_t), tile_count,
                    d->f) != tile_count) {
        std::fclose(d->f);
        d->f = nullptr;
        set_err("tile table write failed");
        return nullptr;
    }
    return d.release();
}

int aio_upm_dense_write_tile(AioUpmDense* d, std::uint64_t frame_index,
                             std::uint64_t tile_ipix, const double* values,
                             std::uint64_t count) {
    if (!d || !d->f || !values || count != kLeafPerTile) return 1;
    if (frame_index != d->current_frame) {
        if (frame_index != d->current_frame + 1) {
            set_err("dense write tile: frame_index 必须单调推进");
            return 1;
        }
        if (d->tiles.size() != d->tile_count) {
            set_err("dense write tile: 前一帧 tile 数不足");
            return 1;
        }
        ++d->current_frame;
        d->tiles.clear();
    }
    d->tiles.push_back(tile_ipix);
    std::vector<std::uint8_t> bytes;
    bytes.reserve((std::size_t)count * (d->precision == 1 ? 8 : 4));
    if (d->precision == 1) {
        for (std::uint64_t i = 0; i < count; ++i) {
            const std::uint8_t* p =
                reinterpret_cast<const std::uint8_t*>(&values[i]);
            bytes.insert(bytes.end(), p, p + sizeof(double));
        }
    } else {
        for (std::uint64_t i = 0; i < count; ++i) {
            const float v = (float)values[i];
            const std::uint8_t* p =
                reinterpret_cast<const std::uint8_t*>(&v);
            bytes.insert(bytes.end(), p, p + sizeof(float));
        }
    }
    if (std::fwrite(bytes.data(), 1, bytes.size(), d->f) != bytes.size()) {
        set_err("dense write tile: data write failed");
        return 1;
    }
    return 0;
}

int aio_upm_dense_end(AioUpmDense* d) {
    if (!d || !d->f) return 1;
    std::unique_ptr<AioUpmDense> guard(d);   // 所有路径释放
    if (d->tiles.size() != d->tile_count) {
        std::fclose(d->f);
        d->f = nullptr;
        set_err("dense end: tile 数量不匹配");
        return 1;
    }
    std::fclose(d->f);
    d->f = nullptr;

    // 回填 tile 表
    std::FILE* wf = std::fopen(d->path.c_str(), "r+b");
    if (!wf) {
        set_err("reopen for tile table failed");
        return 1;
    }
    if (AIO_FSEEK(wf, (std::int64_t)(kDenseHeaderBytes + 1), SEEK_SET) != 0 ||
        std::fwrite(d->tiles.data(), sizeof(std::uint64_t),
                    d->tile_count, wf) != d->tile_count) {
        std::fclose(wf);
        set_err("tile table writeback failed");
        return 1;
    }
    std::fclose(wf);

    // 计算 checksum：header（checksum 槽置 0）+ 整个文件其余（分块 streaming）
    std::FILE* rf = std::fopen(d->path.c_str(), "rb");
    if (!rf) {
        set_err("reopen for checksum failed");
        return 1;
    }
    std::string header(kDenseHeaderBytes, ' ');
    if (std::fread(&header[0], 1, kDenseHeaderBytes, rf) !=
        (size_t)kDenseHeaderBytes) {
        std::fclose(rf);
        set_err("header re-read failed");
        return 1;
    }
    const std::string marker = "\"checksum\":\"";
    const std::size_t pos = header.find(marker);
    if (pos == std::string::npos) {
        std::fclose(rf);
        set_err("checksum slot not found");
        return 1;
    }
    const std::size_t slot = pos + marker.size();
    for (int i = 0; i < 64; ++i) header[slot + (std::size_t)i] = '0';
    astrocs::crypto::Sha256 sha;
    sha.update(header.data(), header.size());
    std::vector<unsigned char> chunk(1 << 20);
    std::size_t got = 0;
    while ((got = std::fread(chunk.data(), 1, chunk.size(), rf)) > 0)
        sha.update(chunk.data(), got);
    std::fclose(rf);
    const std::string checksum = sha.final_hex();

    // 写回 checksum
    wf = std::fopen(d->path.c_str(), "r+b");
    if (!wf) {
        set_err("reopen for checksum write failed");
        return 1;
    }
    if (AIO_FSEEK(wf, (std::int64_t)slot, SEEK_SET) != 0 ||
        std::fwrite(checksum.data(), 1, 64, wf) != 64) {
        std::fclose(wf);
        set_err("checksum write failed");
        return 1;
    }
    std::fclose(wf);
    return 0;
}

void aio_upm_dense_abort(AioUpmDense* d) {
    if (!d) return;
    if (d->f) {
        std::fclose(d->f);
        d->f = nullptr;
        std::remove(d->path.c_str());
    }
    delete d;
}

namespace {

// 读 dense header 并校验 source_hash/format；失败返回 false 并设置 err。
bool dense_read_header(const char* path, const char* source_hash,
                       nlohmann::json* out_j, std::string* err) {
    std::FILE* f = std::fopen(path, "rb");
    if (!f) {
        *err = "cannot open dense cache";
        return false;
    }
    std::string header(kDenseHeaderBytes, ' ');
    if (std::fread(&header[0], 1, kDenseHeaderBytes, f) !=
        (size_t)kDenseHeaderBytes) {
        std::fclose(f);
        *err = "header read failed";
        return false;
    }
    std::fclose(f);
    const std::size_t nl = header.find('\n');
    const std::string line = (nl == std::string::npos)
                                 ? header : header.substr(0, nl);
    try {
        *out_j = nlohmann::json::parse(line);
    } catch (...) {
        *err = "header parse failed";
        return false;
    }
    if (out_j->value("format", std::string()) != "astrocs-upm-dense-v2") {
        *err = "format != astrocs-upm-dense-v2";
        return false;
    }
    if (out_j->value("source_hash", std::string()) != source_hash) {
        *err = "stale cache (source_hash mismatch)";
        return false;
    }
    return true;
}

// 分块 streaming 校验整个 dense 文件 checksum。
bool dense_verify_checksum(const char* path, const nlohmann::json& j,
                           std::string* err) {
    std::FILE* f = std::fopen(path, "rb");
    if (!f) {
        *err = "cannot open dense cache for checksum";
        return false;
    }
    std::string header(kDenseHeaderBytes, ' ');
    if (std::fread(&header[0], 1, kDenseHeaderBytes, f) !=
        (size_t)kDenseHeaderBytes) {
        std::fclose(f);
        *err = "header read failed";
        return false;
    }
    const std::string marker = "\"checksum\":\"";
    const std::size_t pos = header.find(marker);
    if (pos == std::string::npos) {
        std::fclose(f);
        *err = "checksum slot not found";
        return false;
    }
    const std::size_t slot = pos + marker.size();
    for (int i = 0; i < 64; ++i) header[slot + (std::size_t)i] = '0';
    astrocs::crypto::Sha256 sha;
    sha.update(header.data(), header.size());
    std::vector<unsigned char> chunk(1 << 20);
    std::size_t got = 0;
    while ((got = std::fread(chunk.data(), 1, chunk.size(), f)) > 0)
        sha.update(chunk.data(), got);
    std::fclose(f);
    const std::string actual = sha.final_hex();
    const std::string declared = j.value("checksum", std::string());
    if (actual != declared) {
        *err = "dense cache checksum mismatch";
        return false;
    }
    return true;
}

} // namespace

int aio_upm_dense_info(const char* path, const char* source_hash,
                       int* out_target_order, std::uint64_t* out_tile_count,
                       char* out_checksum, int checksum_buf_size) {
    g_upm_error.clear();
    if (!path || !source_hash) return 1;
    nlohmann::json j;
    std::string err;
    if (!dense_read_header(path, source_hash, &j, &err)) {
        set_err(err);
        return 2;  // stale/格式不匹配
    }
    if (out_target_order) *out_target_order = j.value("target_order", 0);
    if (out_tile_count) *out_tile_count = j.value("tile_count", 0ull);
    if (out_checksum && checksum_buf_size > 0) {
        const std::string c = j.value("checksum", std::string());
        std::strncpy(out_checksum, c.c_str(), (size_t)checksum_buf_size - 1);
        out_checksum[checksum_buf_size - 1] = '\0';
    }
    return 0;
}

int aio_upm_read_dense_block(const char* path, const char* source_hash,
                             std::uint64_t frame_id,
                             const std::uint64_t* leaf_ipix,
                             const double* input_signal, double* output_signal,
                             std::uint64_t count) {
    g_upm_error.clear();
    if (!path || !source_hash || !leaf_ipix || !input_signal ||
        !output_signal || count == 0) {
        return 1;
    }
    nlohmann::json j;
    std::string err;
    if (!dense_read_header(path, source_hash, &j, &err)) {
        set_err(err);
        return 2;
    }
    if (!dense_verify_checksum(path, j, &err)) {
        set_err(err);
        return 2;
    }
    const std::uint64_t n_frames = j.value("frame_count", 0ull);
    const std::uint64_t n_tiles = j.value("tile_count", 0ull);
    const int precision = j.value("precision", 0);
    const std::size_t elem = (precision == 1) ? sizeof(double) : sizeof(float);
    const std::size_t tile_bytes = (std::size_t)kLeafPerTile * elem;
    const std::uint64_t frame_index = frame_id;  // phase2 保证 frame_id=index
    if (frame_index >= n_frames) {
        set_err("frame_id out of range");
        return 1;
    }
    std::FILE* f = std::fopen(path, "rb");
    if (!f) {
        set_err("cannot open dense cache");
        return 1;
    }
    std::vector<std::uint64_t> tiles(n_tiles);
    if (AIO_FSEEK(f, (std::int64_t)(kDenseHeaderBytes + 1), SEEK_SET) != 0 ||
        std::fread(tiles.data(), sizeof(std::uint64_t), n_tiles, f) !=
            n_tiles) {
        std::fclose(f);
        set_err("tile table read failed");
        return 1;
    }
    const int tile_shift = 9;
    const std::uint64_t mask = (1ULL << (2u * (unsigned)tile_shift)) - 1ULL;
    std::vector<double> tile_cache(kLeafPerTile);
    std::uint64_t cur_tile = ~0ULL;
    bool tile_loaded = false;
    for (std::uint64_t i = 0; i < count; ++i) {
        const std::uint64_t tile =
            leaf_ipix[i] >> (2u * (unsigned)tile_shift);
        const std::uint64_t local = leaf_ipix[i] & mask;
        if (!tile_loaded || tile != cur_tile) {
            const auto it = std::find(tiles.begin(), tiles.end(), tile);
            if (it == tiles.end()) {
                std::fclose(f);
                set_err("leaf tile not in dense cache");
                return 1;
            }
            const std::size_t t_idx = (std::size_t)(it - tiles.begin());
            const std::uint64_t off =
                (std::uint64_t)(kDenseHeaderBytes + 1) +
                (std::uint64_t)n_tiles * sizeof(std::uint64_t) +
                (frame_index * n_tiles + t_idx) * (std::uint64_t)tile_bytes;
            if (AIO_FSEEK(f, (std::int64_t)off, SEEK_SET) != 0) {
                std::fclose(f);
                set_err("seek to tile block failed");
                return 1;
            }
            if (precision == 1) {
                if (std::fread(tile_cache.data(), sizeof(double),
                               kLeafPerTile, f) != kLeafPerTile) {
                    std::fclose(f);
                    set_err("tile block read failed");
                    return 1;
                }
            } else {
                std::vector<float> tmp(kLeafPerTile);
                if (std::fread(tmp.data(), sizeof(float), kLeafPerTile, f) !=
                    kLeafPerTile) {
                    std::fclose(f);
                    set_err("tile block read failed");
                    return 1;
                }
                for (std::size_t k = 0; k < kLeafPerTile; ++k)
                    tile_cache[k] = (double)tmp[k];
            }
            cur_tile = tile;
            tile_loaded = true;
        }
        if (local >= kLeafPerTile) {
            std::fclose(f);
            set_err("leaf local out of range");
            return 1;
        }
        output_signal[i] = input_signal[i] - tile_cache[(std::size_t)local];
    }
    std::fclose(f);
    return 0;
}

const char* aio_upm_last_error(void) {
    return g_upm_error.c_str();
}

} // extern "C"
