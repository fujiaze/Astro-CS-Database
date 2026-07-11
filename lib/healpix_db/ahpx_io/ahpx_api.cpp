#include "ahpx_api.h"
#include "ahpx_reader.h"
#include "ahpx_writer.h"
#include "ahpx_format.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

// ============================================================================
// UTF-8 路径文件打开辅助 (Windows 下支持中文路径)
// ============================================================================
static FILE* ahpx_api_fopen_utf8(const char* path, const char* mode) {
#ifdef _WIN32
    int wpath_len = MultiByteToWideChar(CP_UTF8, 0, path, -1, nullptr, 0);
    if (wpath_len <= 0) return std::fopen(path, mode);
    std::wstring wpath(wpath_len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, path, -1, &wpath[0], wpath_len);

    int wmode_len = MultiByteToWideChar(CP_UTF8, 0, mode, -1, nullptr, 0);
    std::wstring wmode(wmode_len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, mode, -1, &wmode[0], wmode_len);

    return _wfopen(wpath.c_str(), wmode.c_str());
#else
    return std::fopen(path, mode);
#endif
}

// ============================================================================
// 内部辅助: 将 std::vector<float> 转为 malloc 分配的 float 数组
// 返回: malloc 分配的数组 (调用方 free), 失败返回 NULL
// ============================================================================
static float* vectorToMalloc(const std::vector<float>& vec) {
    if (vec.empty()) return nullptr;
    size_t bytes = vec.size() * sizeof(float);
    float* result = (float*)std::malloc(bytes);
    if (!result) {
        fprintf(stderr, "[ahpx][api] malloc 失败 (%zu bytes)\n", bytes);
        return nullptr;
    }
    std::memcpy(result, vec.data(), bytes);
    return result;
}

// ============================================================================
// 读取 API 实现
// ============================================================================

// 不透明类型: C++ 类直接作为 C 结构使用
// 由于 C 不支持类, 我们用 typedef struct AhpxReader AhpxReader
// 在 C++ 中, struct 和 class 等价 (仅默认访问权限不同)
// 所以直接将 ahpx::AhpxReader 类型别名化为 AhpxReader

AHPX_EXPORT AhpxReader* ahpx_open(const char* path) {
    if (!path) return nullptr;

    ahpx::AhpxReader* reader = new ahpx::AhpxReader();
    if (!reader->open(path)) {
        delete reader;
        return nullptr;
    }
    return reinterpret_cast<AhpxReader*>(reader);
}

AHPX_EXPORT const char* ahpx_get_header_json(AhpxReader* reader) {
    if (!reader) return nullptr;
    ahpx::AhpxReader* r = reinterpret_cast<ahpx::AhpxReader*>(reader);
    const std::string& json = r->getHeaderJson();
    // 返回内部字符串指针 (reader 关闭后失效)
    return json.c_str();
}

AHPX_EXPORT int ahpx_get_image_info(AhpxReader* reader, int* w, int* h, int* c) {
    if (!reader) return 0;
    ahpx::AhpxReader* r = reinterpret_cast<ahpx::AhpxReader*>(reader);
    int width = 0, height = 0, channels = 0;
    if (!r->getImageInfo(&width, &height, &channels)) {
        return 0;
    }
    if (w) *w = width;
    if (h) *h = height;
    if (c) *c = channels;
    return 1;
}

AHPX_EXPORT float* ahpx_read_pixels(AhpxReader* reader) {
    if (!reader) return nullptr;
    ahpx::AhpxReader* r = reinterpret_cast<ahpx::AhpxReader*>(reader);
    std::vector<float> pixels = r->readPixels();
    return vectorToMalloc(pixels);
}

AHPX_EXPORT float* ahpx_read_snr(AhpxReader* reader) {
    if (!reader) return nullptr;
    ahpx::AhpxReader* r = reinterpret_cast<ahpx::AhpxReader*>(reader);
    std::vector<float> snr = r->readSnr();
    return vectorToMalloc(snr);
}

AHPX_EXPORT float* ahpx_read_weight(AhpxReader* reader, int* mode, int* gw, int* gh, int* count) {
    if (!reader) return nullptr;
    ahpx::AhpxReader* r = reinterpret_cast<ahpx::AhpxReader*>(reader);

    ahpx::WeightMode wMode = ahpx::WeightMode::SCALAR;
    uint16_t wGw = 0, wGh = 0;
    std::vector<float> weight = r->readWeight(&wMode, &wGw, &wGh);

    if (mode) *mode = (int)wMode;
    if (gw) *gw = (int)wGw;
    if (gh) *gh = (int)wGh;
    if (count) *count = (int)weight.size();

    return vectorToMalloc(weight);
}

AHPX_EXPORT void ahpx_close(AhpxReader* reader) {
    if (!reader) return;
    ahpx::AhpxReader* r = reinterpret_cast<ahpx::AhpxReader*>(reader);
    r->close();
    delete r;
}

// ============================================================================
// 写入 API 实现
// ============================================================================

AHPX_EXPORT AhpxWriter* ahpx_writer_new() {
    ahpx::AhpxWriter* writer = new ahpx::AhpxWriter();
    return reinterpret_cast<AhpxWriter*>(writer);
}

AHPX_EXPORT void ahpx_writer_set_metadata(AhpxWriter* w, const char* json) {
    if (!w || !json) return;
    ahpx::AhpxWriter* writer = reinterpret_cast<ahpx::AhpxWriter*>(w);
    writer->setMetadata(std::string(json));
}

AHPX_EXPORT void ahpx_writer_set_pixels(AhpxWriter* w, const float* data,
                                         int width, int height, int channels) {
    if (!w || !data) return;
    ahpx::AhpxWriter* writer = reinterpret_cast<ahpx::AhpxWriter*>(w);
    writer->setPixels(data, width, height, channels);
}

AHPX_EXPORT void ahpx_writer_set_snr(AhpxWriter* w, const float* data,
                                      int width, int height) {
    if (!w || !data) return;
    ahpx::AhpxWriter* writer = reinterpret_cast<ahpx::AhpxWriter*>(w);
    writer->setSnr(data, width, height);
}

AHPX_EXPORT void ahpx_writer_set_weight_scalar(AhpxWriter* w, float scalar) {
    if (!w) return;
    ahpx::AhpxWriter* writer = reinterpret_cast<ahpx::AhpxWriter*>(w);
    writer->setWeightScalar(scalar);
}

AHPX_EXPORT void ahpx_writer_set_weight_grid(AhpxWriter* w, const float* grid,
                                              int gw, int gh) {
    if (!w || !grid) return;
    ahpx::AhpxWriter* writer = reinterpret_cast<ahpx::AhpxWriter*>(w);
    writer->setWeightGrid(grid, (uint16_t)gw, (uint16_t)gh);
}

AHPX_EXPORT void ahpx_writer_set_weight_pixel(AhpxWriter* w, const float* data,
                                               int width, int height) {
    if (!w || !data) return;
    ahpx::AhpxWriter* writer = reinterpret_cast<ahpx::AhpxWriter*>(w);
    writer->setWeightPixel(data, width, height);
}

AHPX_EXPORT int ahpx_writer_write(AhpxWriter* w, const char* path, int zstd_level) {
    if (!w || !path) return 0;
    ahpx::AhpxWriter* writer = reinterpret_cast<ahpx::AhpxWriter*>(w);

    ahpx::AhpxWriteConfig config;
    config.zstdLevel = zstd_level;
    // weightMode/gridW/gridH 由 writer 内部在 setWeight* 调用时记录
    // write() 方法内部会用内部状态覆盖 config 中的对应字段

    return writer->write(path, config) ? 1 : 0;
}

AHPX_EXPORT void ahpx_writer_free(AhpxWriter* w) {
    if (!w) return;
    ahpx::AhpxWriter* writer = reinterpret_cast<ahpx::AhpxWriter*>(w);
    delete writer;
}

// ============================================================================
// 工具函数实现
// ============================================================================

AHPX_EXPORT int ahpx_is_ahpx(const char* path) {
    if (!path) return 0;

    FILE* fp = ahpx_api_fopen_utf8(path, "rb");
    if (!fp) return 0;

    // 读取前 4 字节, 检查 Magic
    char magic[4];
    size_t bytesRead = std::fread(magic, 1, 4, fp);
    std::fclose(fp);

    if (bytesRead != 4) return 0;

    // 比较 Magic "AHPX"
    if (std::memcmp(magic, ahpx::MAGIC, 4) == 0) {
        return 1;
    }
    return 0;
}

// 释放由 ahpx_read_pixels/snr/weight 返回的 malloc 分配内存
AHPX_EXPORT void ahpx_free(void* ptr) {
    if (ptr) std::free(ptr);
}
