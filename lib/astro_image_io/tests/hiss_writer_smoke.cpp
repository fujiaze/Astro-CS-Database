// hiss_writer_smoke.cpp - HissWriter 冒烟测试
// 验证: open → add_tile(FULL + BITMAP) → finalize, 并检查文件头签名
#include "hiss_format.h"
#include <cstdio>
#include <cstring>
#include <vector>
#include <filesystem>

int main() {
    using namespace hiss;
    const char* path = "hiss_writer_smoke.hiss";

    // 1. 网格规格
    HissGridSpec grid;
    grid.nside      = 64;
    grid.tile_nside = compute_tile_nside(64);  // 64/2^2=16
    grid.ordering   = 1;  // NESTED
    grid.radesys    = 0;  // ICRS
    grid.pixfrac    = 1.0;
    fprintf(stderr, "[smoke] nside=%u tile_nside=%u depth=%u\n",
            grid.nside, grid.tile_nside, compute_tile_depth(64));

    // 2. 元数据
    HissMetadata meta;
    meta.nside = grid.nside;
    meta.tile_nside = grid.tile_nside;
    std::strncpy(meta.object, "TestObject", sizeof(meta.object) - 1);
    std::strncpy(meta.filter, "R", sizeof(meta.filter) - 1);
    meta.exptime = 60.0;
    meta.history = "smoke test\nline2";
    // BUNIT=ASTROCS_RELATIVE_FLUX (默认) 要求 PHOTAPPL=TRUE (Writer 元数据一致性校验)
    meta.photappl = 1;
    meta.photscal = 1.0;

    // 3. 累加器: tile_nside=16, 16^2=256 叶像素
    DrizzleTileAccumulator acc;
    acc.tile_nside  = 16;
    acc.parent_ipix = 42;
    acc.pixel_area  = 1.0;  // sum_area=0.7 为任意值, 用 1.0 保持测试原有行为
    acc.pixels.resize(16 * 16);
    for (size_t i = 0; i < acc.pixels.size(); i++) {
        acc.pixels[i].sum_flux = (double)i * 1.5;
        acc.pixels[i].sum_area = 0.7;  // 部分有效
        acc.pixels[i].n_contrib = 3;
    }
    // 部分像素无贡献 (测试 BITMAP)
    acc.pixels[0].sum_area = 0.0;
    acc.pixels[10].sum_area = 0.0;
    if (acc.validate_support() != 0) {
        fprintf(stderr, "[smoke] FAIL: validate_support\n");
        return 1;
    }

    // 4. SNR (冻结布局: 仅 n_points + points, 不含估计器状态)
    HissSnrBlock snr;
    snr.points.push_back({1, 8.5f});
    snr.points.push_back({5, 15.2f});

    // 5. 写入
    HissWriter w;
    if (w.open(path, grid, meta) != 0) {
        fprintf(stderr, "[smoke] FAIL: open\n");
        return 1;
    }
    // FULL 模式 + SNR
    if (w.add_tile(42, acc, &snr, OccupancyMode::FULL) != 0) {
        fprintf(stderr, "[smoke] FAIL: add_tile FULL\n");
        return 1;
    }
    // BITMAP 模式无 SNR
    DrizzleTileAccumulator acc2 = acc;
    acc2.parent_ipix = 100;
    if (w.add_tile(100, acc2, nullptr, OccupancyMode::BITMAP) != 0) {
        fprintf(stderr, "[smoke] FAIL: add_tile BITMAP\n");
        return 1;
    }
    if (w.finalize() != 0) {
        fprintf(stderr, "[smoke] FAIL: finalize\n");
        return 1;
    }

    // 6. 检查文件
    if (!std::filesystem::exists(path)) {
        fprintf(stderr, "[smoke] FAIL: 输出文件不存在 %s\n", path);
        return 1;
    }
    auto fsize = std::filesystem::file_size(path);
    fprintf(stderr, "[smoke] 文件大小: %llu 字节\n", (unsigned long long)fsize);

    // 7. 检查文件头签名 (R04-B14: 签名块 16B = magic[8] + header_length(u32) + feature_flags(u32))
    FILE* fp = std::fopen(path, "rb");
    if (!fp) {
        fprintf(stderr, "[smoke] FAIL: 无法打开输出文件\n");
        return 1;
    }
    unsigned char head[16];
    if (std::fread(head, 1, 16, fp) != 16) {
        fprintf(stderr, "[smoke] FAIL: 读取签名块失败\n");
        std::fclose(fp);
        return 1;
    }
    std::fclose(fp);

    if (std::memcmp(head, "HISS0100", 8) != 0) {
        fprintf(stderr, "[smoke] FAIL: MAGIC 不匹配\n");
        return 1;
    }
    uint32_t header_length;
    std::memcpy(&header_length, head + 8, 4);
    if (header_length == 0) {
        fprintf(stderr, "[smoke] FAIL: header_length=0 (期望非零)\n");
        return 1;
    }
    uint32_t feature_flags;
    std::memcpy(&feature_flags, head + 12, 4);
    fprintf(stderr, "[smoke] MAGIC=OK header_length=%u feature_flags=0x%08X\n",
            header_length, feature_flags);
    if ((feature_flags & HISS_FEAT_TLV_HEADER) == 0) {
        fprintf(stderr, "[smoke] FAIL: feature_flags 缺少 TLV_HEADER 位\n");
        return 1;
    }

    // 8. 检查 .partial 已被清理
    if (std::filesystem::exists(std::string(path) + ".partial")) {
        fprintf(stderr, "[smoke] FAIL: .partial 残留\n");
        return 1;
    }

    // 9. 测试 to_json / from_json 往返
    std::string json = meta.to_json();
    fprintf(stderr, "[smoke] JSON: %s\n", json.c_str());
    HissMetadata meta2;
    if (meta2.from_json(json) != 0) {
        fprintf(stderr, "[smoke] FAIL: from_json\n");
        return 1;
    }
    if (meta2.nside != meta.nside ||
        meta2.exptime != meta.exptime ||
        std::string(meta2.object) != std::string(meta.object) ||
        meta2.history != meta.history) {
        fprintf(stderr, "[smoke] FAIL: JSON 往返不一致\n");
        fprintf(stderr, "  nside %u vs %u\n", meta2.nside, meta.nside);
        fprintf(stderr, "  exptime %g vs %g\n", meta2.exptime, meta.exptime);
        fprintf(stderr, "  object '%s' vs '%s'\n", meta2.object, meta.object);
        fprintf(stderr, "  history '%s' vs '%s'\n", meta2.history.c_str(), meta.history.c_str());
        return 1;
    }

    // 10. 清理
    std::filesystem::remove(path);
    fprintf(stderr, "[smoke] ALL PASS\n");
    return 0;
}
