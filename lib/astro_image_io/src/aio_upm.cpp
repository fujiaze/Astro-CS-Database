// lib/astro_image_io/src/aio_upm.cpp — UPM 模型文件容器实现
//
// 容器层：稀疏 JSON 原子写/读 + 稠密缓存（固定头部 + 二进制块 + checksum）。
// 科学语义不在此层（phase2 负责）；本层保证"所有科学模型 I/O 走唯一 AIO"。
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

namespace {

thread_local std::string g_upm_error;

void set_err(const std::string& m) { g_upm_error = m; }

constexpr int kDenseHeaderBytes = 512;

std::string dense_checksum_of(const std::string& header_with_zeros,
                              const std::vector<std::uint8_t>& payload) {
    std::string all = header_with_zeros;
    all.append(reinterpret_cast<const char*>(payload.data()), payload.size());
    return astrocs::crypto::sha256_hex(all.data(), all.size());
}

} // namespace

struct AioUpmSparse {
    std::string content;
    nlohmann::json json;
};

struct AioUpmDense {
    std::FILE* f = nullptr;
    std::string path;
    std::string source_hash;
    std::vector<std::uint8_t> payload;  // controls + frames 二进制
    std::uint64_t control_count = 0;
    std::uint64_t frame_count = 0;
    int target_order = 0;
    std::uint32_t precision = 0;
};

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
        if (j.value("format", std::string()) != "astrocs-upm-v1") {
            set_err("format != astrocs-upm-v1");
            return 1;
        }
    } catch (const std::exception& e) {
        set_err(std::string("json parse: ") + e.what());
        return 1;
    }
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) {
        set_err("cannot open for write");
        return 1;
    }
    f.write(model_json, std::streamsize(std::strlen(model_json)));
    f.flush();
    if (!f.good()) {
        set_err("write failed");
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
        if (m->json.value("format", std::string()) != "astrocs-upm-v1") {
            set_err("format != astrocs-upm-v1");
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

void aio_upm_close(AioUpmSparse* f) {
    delete f;
}

// ===== 稠密缓存 =====

AioUpmDense* aio_upm_dense_begin(const char* path, const char* source_hash,
                                 int target_order, std::uint32_t precision,
                                 std::uint64_t control_count,
                                 std::uint64_t frame_count) {
    g_upm_error.clear();
    if (!path || !source_hash || std::strlen(source_hash) != 64) {
        set_err("invalid args");
        return nullptr;
    }
    std::unique_ptr<AioUpmDense> d(new AioUpmDense);
    d->path = path;
    d->source_hash = source_hash;
    d->target_order = target_order;
    d->precision = precision;
    d->control_count = control_count;
    d->frame_count = frame_count;
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
                  "{\"format\":\"astrocs-upm-dense-v1\",\"source_hash\":\"%s\","
                  "\"target_order\":%d,\"precision\":%u,"
                  "\"control_count\":%llu,\"frame_count\":%llu,"
                  "\"checksum\":\"%s\"}",
                  source_hash, target_order, precision,
                  (unsigned long long)control_count,
                  (unsigned long long)frame_count, zeros);
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
    return d.release();
}

int aio_upm_dense_write_controls(AioUpmDense* d, const double* z,
                                 std::uint64_t count) {
    if (!d || !z || count == 0) return 1;
    const std::size_t bytes = (std::size_t)count * sizeof(double);
    d->payload.insert(d->payload.end(),
                      reinterpret_cast<const std::uint8_t*>(z),
                      reinterpret_cast<const std::uint8_t*>(z) + bytes);
    return 0;
}

int aio_upm_dense_write_frames(AioUpmDense* d, const std::uint64_t* frame_ids,
                               const double* offsets, std::uint64_t count) {
    if (!d || !frame_ids || !offsets || count == 0) return 1;
    for (std::uint64_t i = 0; i < count; ++i) {
        const std::uint8_t* p = reinterpret_cast<const std::uint8_t*>(&frame_ids[i]);
        d->payload.insert(d->payload.end(), p, p + sizeof(std::uint64_t));
        const std::uint8_t* q = reinterpret_cast<const std::uint8_t*>(&offsets[i]);
        d->payload.insert(d->payload.end(), q, q + sizeof(double));
    }
    return 0;
}

int aio_upm_dense_end(AioUpmDense* d) {
    if (!d || !d->f) return 1;
    // 写 payload 并关闭（避免同一流写后读的 Windows 流状态问题）
    if (!d->payload.empty() &&
        std::fwrite(d->payload.data(), 1, d->payload.size(), d->f) !=
            d->payload.size()) {
        std::fclose(d->f);
        d->f = nullptr;
        set_err("payload write failed");
        return 1;
    }
    std::fflush(d->f);
    std::fclose(d->f);
    d->f = nullptr;

    // 重开读取整个文件（头部 + payload）计算 checksum
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
    // 定位 checksum 槽（JSON 顺序：...checksum":"<64 chars>"}）
    const std::string marker = "\"checksum\":\"";
    const std::size_t pos = header.find(marker);
    if (pos == std::string::npos) {
        std::fclose(rf);
        set_err("checksum slot not found");
        return 1;
    }
    const std::size_t slot = pos + marker.size();
    for (int i = 0; i < 64; ++i) header[slot + (std::size_t)i] = '0';
    // payload = 头部后所有字节
    std::fseek(rf, 0, SEEK_END);
    const long file_size = std::ftell(rf);
    const std::size_t payload_size =
        (std::size_t)file_size - (kDenseHeaderBytes + 1);
    std::vector<std::uint8_t> payload(payload_size);
    std::fseek(rf, kDenseHeaderBytes + 1, SEEK_SET);
    if (payload_size > 0 &&
        std::fread(payload.data(), 1, payload_size, rf) != payload_size) {
        std::fclose(rf);
        set_err("payload re-read failed");
        return 1;
    }
    std::fclose(rf);
    const std::string checksum = dense_checksum_of(header, payload);
    // 回填 checksum（r+b）
    std::FILE* wf = std::fopen(d->path.c_str(), "r+b");
    if (!wf) {
        set_err("reopen for checksum write failed");
        return 1;
    }
    if (std::fseek(wf, (long)slot, SEEK_SET) != 0 ||
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

int aio_upm_dense_info(const char* path, const char* source_hash,
                       int* out_target_order, std::uint64_t* out_pixels,
                       char* out_checksum, int checksum_buf_size) {
    g_upm_error.clear();
    if (!path || !source_hash) return 1;
    std::FILE* f = std::fopen(path, "rb");
    if (!f) {
        set_err("cannot open dense cache");
        return 1;
    }
    std::string header(kDenseHeaderBytes, ' ');
    if (std::fread(&header[0], 1, kDenseHeaderBytes, f) !=
        (size_t)kDenseHeaderBytes) {
        std::fclose(f);
        set_err("header read failed");
        return 1;
    }
    std::fclose(f);
    // JSON 行以 '\n' 结尾；头部后可能含二进制 '\0'，截断到 '\n'
    const std::size_t nl = header.find('\n');
    const std::string line = (nl == std::string::npos)
                                 ? header : header.substr(0, nl);
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(line);
    } catch (...) {
        set_err("header parse failed");
        return 1;
    }
    if (j.value("format", std::string()) != "astrocs-upm-dense-v1")
        return 1;
    const std::string src = j.value("source_hash", std::string());
    if (src != source_hash) return 2;  // stale
    if (out_target_order) *out_target_order = j.value("target_order", 0);
    if (out_pixels) *out_pixels = j.value("control_count", 0ull);
    if (out_checksum && checksum_buf_size > 0) {
        const std::string c = j.value("checksum", std::string());
        std::strncpy(out_checksum, c.c_str(), (size_t)checksum_buf_size - 1);
        out_checksum[checksum_buf_size - 1] = '\0';
    }
    return 0;
}

int aio_upm_read_dense_block(const char* path, const char* source_hash,
                             std::uint64_t frame_id,
                             const double* input_signal, double* output_signal,
                             std::uint64_t count) {
    g_upm_error.clear();
    if (!path || !source_hash || !input_signal || !output_signal) return 1;
    std::FILE* f = std::fopen(path, "rb");
    if (!f) {
        set_err("cannot open dense cache");
        return 1;
    }
    std::string header(kDenseHeaderBytes, ' ');
    if (std::fread(&header[0], 1, kDenseHeaderBytes, f) !=
        (size_t)kDenseHeaderBytes) {
        std::fclose(f);
        return 1;
    }
    const std::size_t nl = header.find('\n');
    const std::string line = (nl == std::string::npos)
                                 ? header : header.substr(0, nl);
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(line);
    } catch (...) {
        std::fclose(f);
        return 1;
    }
    if (j.value("format", std::string()) != "astrocs-upm-dense-v1") {
        std::fclose(f);
        return 1;
    }
    const std::string src = j.value("source_hash", std::string());
    if (src != source_hash) {
        std::fclose(f);
        return 2;  // stale
    }
    const std::uint64_t n_controls = j.value("control_count", 0ull);
    const std::uint64_t n_frames = j.value("frame_count", 0ull);
    // 校验 checksum（头部槽置 '0' + 剩余文件）
    std::string header_zero = header;
    const std::string marker = "\"checksum\":\"";
    const std::size_t pos = header_zero.find(marker);
    if (pos != std::string::npos) {
        const std::size_t slot = pos + marker.size();
        for (int i = 0; i < 64; ++i) header_zero[slot + (std::size_t)i] = '0';
    }
    std::fseek(f, 0, SEEK_END);
    const long file_size = std::ftell(f);
    const std::size_t payload_size =
        (std::size_t)file_size - (kDenseHeaderBytes + 1);
    std::vector<std::uint8_t> payload(payload_size);
    std::fseek(f, kDenseHeaderBytes + 1, SEEK_SET);
    if (payload_size > 0 &&
        std::fread(payload.data(), 1, payload_size, f) != payload_size) {
        std::fclose(f);
        return 1;
    }
    const std::string checksum_actual =
        dense_checksum_of(header_zero.substr(0, kDenseHeaderBytes), payload);
    const std::string checksum_decl = j.value("checksum", std::string());
    if (checksum_actual != checksum_decl) {
        std::fclose(f);
        set_err("dense cache checksum mismatch");
        return 2;
    }
    // 读 frame 偏移
    std::fseek(f, kDenseHeaderBytes + 1 + (long)(n_controls * sizeof(double)),
               SEEK_SET);
    double offset = 0.0;
    bool found = false;
    for (std::uint64_t i = 0; i < n_frames; ++i) {
        std::uint64_t fid = 0;
        double off = 0.0;
        if (std::fread(&fid, sizeof(fid), 1, f) != 1 ||
            std::fread(&off, sizeof(off), 1, f) != 1) {
            std::fclose(f);
            return 1;
        }
        if (fid == frame_id) {
            offset = off;
            found = true;
        }
    }
    std::fclose(f);
    if (!found) offset = 0.0;
    for (std::uint64_t i = 0; i < count; ++i)
        output_signal[i] = input_signal[i] - offset;
    return 0;
}

const char* aio_upm_last_error(void) {
    return g_upm_error.c_str();
}

} // extern "C"
