// lib/phase2/src/coverage.cpp — Phase2 W3 coverage union 实现（真实 AIO 接入）
//
// 语义（控制包 34A532A2...B2EB308 + wiki Phase2_Architecture）：
//   - 输入为多个 Phase1 单帧 HiPS（signal/support/snr）；
//   - 通过唯一 AIO（astro_image_io.dll aio_hips_reader）读取每帧
//     properties 与叶级 tile 列表（Moc.fits）；
//   - 兼容校验：hips_version / hips_frame / tile_width / obs_filter；
//   - target_order = min(所有输入 max leaf order)（禁止低 order 插值伪装分辨率）；
//   - Ω = MOC_1 ∪ ... ∪ MOC_N（NESTED，允许不连通分量）。
#include "astro/phase2/coverage.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

// 唯一 AIO HiPS reader（Phase1 冻结模块，只读使用）
extern "C" {
#include "aio_hips_reader.h"
}

extern "C" {

namespace {

std::map<std::string, std::string> parse_props(const char* text) {
    std::map<std::string, std::string> kv;
    if (!text) return kv;
    const char* p = text;
    while (*p) {
        const char* eol = std::strchr(p, '\n');
        const std::string line(p, eol ? (size_t)(eol - p) : std::strlen(p));
        if (!line.empty() && line[0] != '#') {
            const size_t eq = line.find('=');
            if (eq != std::string::npos) {
                std::string k = line.substr(0, eq);
                std::string v = line.substr(eq + 1);
                auto trim = [](std::string& s) {
                    while (!s.empty() && (s.back() == ' ' || s.back() == '\r'))
                        s.pop_back();
                    while (!s.empty() && s.front() == ' ') s.erase(s.begin());
                };
                trim(k); trim(v);
                kv[k] = v;
            }
        }
        if (!eol) break;
        p = eol + 1;
    }
    return kv;
}

// 读取单帧 HiPS：校验兼容性并收集叶级 tile ipix（NESTED order=hips_order）
int inspect_frame(const char* path, P2HipsInputInfo* info,
                  std::vector<std::uint64_t>* tiles, char* err, size_t err_size) {
    AioHipsDataset* d = aio_hips_open(path, AIO_HIPS_RD_SIGNAL);
    if (!d) {
        std::snprintf(err, err_size, "aio_hips_open failed: %s",
                      aio_hips_reader_last_error());
        return 1;
    }
    char buf[8192];
    if (aio_hips_get_properties(d, buf, (int)sizeof(buf)) != 0) {
        std::snprintf(err, err_size, "aio_hips_get_properties failed");
        aio_hips_close(d);
        return 1;
    }
    const auto kv = parse_props(buf);
    auto geti = [&](const std::string& k, int def) -> int {
        auto it = kv.find(k);
        return it == kv.end() ? def : std::atoi(it->second.c_str());
    };
    auto gets = [&](const std::string& k) -> std::string {
        auto it = kv.find(k);
        return it == kv.end() ? std::string() : it->second;
    };

    const int order = geti("hips_order", -1);
    const int tw = geti("hips_tile_width", 0);
    const std::string frame = gets("hips_frame");
    const std::string filter = gets("obs_filter");
    const std::string version = gets("hips_version");
    if (order < 0) {
        std::snprintf(err, err_size, "missing hips_order: %s", path);
        aio_hips_close(d);
        return 1;
    }
    if (tw != 512) {
        std::snprintf(err, err_size, "unsupported tile_width=%d: %s", tw, path);
        aio_hips_close(d);
        return 1;
    }
    if (version.empty()) {
        std::snprintf(err, err_size, "missing hips_version: %s", path);
        aio_hips_close(d);
        return 1;
    }
    if (frame != "equatorial" && frame != "icrs") {
        std::snprintf(err, err_size, "unsupported hips_frame=%s: %s",
                      frame.c_str(), path);
        aio_hips_close(d);
        return 1;
    }

    if (info) {
        std::strncpy(info->hips_path, path, sizeof(info->hips_path) - 1);
        info->hips_path[sizeof(info->hips_path) - 1] = '\0';
        // frame_id：路径 basename（不含扩展名/分隔符）
        std::string base(path);
        const size_t slash = base.find_last_of("/\\");
        if (slash != std::string::npos) base = base.substr(slash + 1);
        std::strncpy(info->frame_id, base.c_str(), sizeof(info->frame_id) - 1);
        info->frame_id[sizeof(info->frame_id) - 1] = '\0';
        info->max_leaf_order = order;
        info->n_tiles = 0;
        std::strncpy(info->filter_passband, filter.c_str(),
                     sizeof(info->filter_passband) - 1);
        info->filter_passband[sizeof(info->filter_passband) - 1] = '\0';
        std::strncpy(info->frame_type, frame.c_str(),
                     sizeof(info->frame_type) - 1);
        info->frame_type[sizeof(info->frame_type) - 1] = '\0';
    }
    if (tiles) {
        const int n = aio_hips_tile_count(d);
        tiles->clear();
        tiles->reserve(n > 0 ? (size_t)n : 0);
        for (int i = 0; i < n; ++i) {
            std::uint64_t ipix = 0;
            if (aio_hips_tile_ipix(d, i, &ipix) == 0) tiles->push_back(ipix);
        }
        if (info) info->n_tiles = (int)tiles->size();
    }
    aio_hips_close(d);
    return 0;
}

} // namespace

int p2_coverage_build(const char* const* hips_paths,
                      std::uint64_t n_inputs,
                      P2CoverageResult* out) {
    if (out == nullptr) return 1;
    // 保存调用方缓冲区指针（memset 会清掉它们）
    P2HipsInputInfo* saved_inputs = out->inputs;
    P2MocCell* saved_cells = out->union_cells;
    std::memset(out, 0, sizeof(*out));
    out->inputs = saved_inputs;
    out->union_cells = saved_cells;
    if (hips_paths == nullptr || n_inputs == 0) {
        std::strncpy(out->error, "no inputs", sizeof(out->error) - 1);
        return 1;
    }
    // 第一次调用：逐帧检查并收集（为第二次填充做准备）
    std::vector<std::vector<std::uint64_t>> frame_tiles(n_inputs);
    std::vector<P2HipsInputInfo> infos(n_inputs);
    int target_order = -1;
    std::string filter_ref;
    bool filter_set = false;
    for (std::uint64_t i = 0; i < n_inputs; ++i) {
        if (!hips_paths[i] || !*hips_paths[i]) {
            std::snprintf(out->error, sizeof(out->error),
                          "empty path at index %llu", (unsigned long long)i);
            out->status = 1;
            return 1;
        }
        char err[512] = {0};
        if (inspect_frame(hips_paths[i], &infos[i], &frame_tiles[i],
                          err, sizeof(err)) != 0) {
            std::strncpy(out->error, err, sizeof(out->error) - 1);
            out->status = 1;
            return 1;
        }
        // 兼容校验：filter 一致
        const std::string f = infos[i].filter_passband;
        if (!f.empty()) {
            if (!filter_set) {
                filter_ref = f;
                filter_set = true;
            } else if (f != filter_ref) {
                std::snprintf(out->error, sizeof(out->error),
                              "filter mismatch: %s vs %s",
                              filter_ref.c_str(), f.c_str());
                out->status = 1;
                return 1;
            }
        }
        // target_order = min(max leaf order)
        if (target_order < 0 || infos[i].max_leaf_order < target_order)
            target_order = infos[i].max_leaf_order;
    }
    if (target_order < 0) {
        std::strncpy(out->error, "no valid inputs", sizeof(out->error) - 1);
        out->status = 1;
        return 1;
    }

    // MOC union：统一到 target_order（NESTED parent 聚合）
    std::vector<std::uint64_t> union_cells;
    for (std::uint64_t i = 0; i < n_inputs; ++i) {
        const int shift = infos[i].max_leaf_order - target_order;
        for (std::uint64_t ip : frame_tiles[i]) {
            union_cells.push_back(ip >> (2 * shift));
        }
    }
    std::sort(union_cells.begin(), union_cells.end());
    union_cells.erase(std::unique(union_cells.begin(), union_cells.end()),
                      union_cells.end());

    out->n_inputs = n_inputs;
    out->target_order = target_order;
    out->n_union_cells = union_cells.size();
    if (out->union_cells != nullptr) {
        for (std::uint64_t i = 0; i < out->n_union_cells; ++i) {
            out->union_cells[i].order = (std::uint64_t)target_order;
            out->union_cells[i].ipix = union_cells[(size_t)i];
        }
    }
    if (out->inputs != nullptr) {
        for (std::uint64_t i = 0; i < n_inputs; ++i)
            out->inputs[i] = infos[(size_t)i];
    }
    out->status = 0;
    return 0;
}

int p2_coverage_free(P2CoverageResult* out) {
    if (out == nullptr) return 0;
    std::memset(out, 0, sizeof(*out));
    return 0;
}

} // extern "C"
