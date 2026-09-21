// ============================================================================
// FIX-202 / 变更 AHPX-WEIGHT-RETIRE-20260920 — .ahpx 格式内部权重枚举作废回归锁
//
// 规范依据:
//   ASTROCS_DESIGN.md §2.1 (全程只有 SNR, 不存在「权重模式」; HiPS 里存的是
//     帧级 SNR + 稀疏相对 SNR 比值), §3.4 (稀疏 SNR 层在交换合同里有位置)
//   GAP_AUDIT.md §9.73 裁决 A44; DESIGN-DRAFT §3.1-D
//
// 组:
//   N1 负例: 旧格式文件 (头 JSON 含 "weight":{"mode":2,...} 且带 "weight" 块)
//            ⇒ 读侧显式拒绝 (open=false + 非空原因; C ABI rc≠0)
//   N2 对照: 同一手工构造文件去掉 weight 字段/块 ⇒ 必须被接受
//            (证明拒绝判据是"已作废字段", 不是"手工文件畸形")
//   P1 正例: 新格式读写往返逐位一致 (pixel/snr), 头 JSON 无 "weight" 键,
//            块表只有 pixel/snr (无第三块)
//   N3 写侧 fail-closed: 元数据携带已作废 "weight" 字段 ⇒ write() 拒绝且不落盘
//
// 故障注入 (证明"能红", 先例 P2-002/P3-002/AIO-001 的 ASTROCS_*_FAULT):
//   ASTROCS_AHPX_FAULT=accept_legacy             → N1 红 (读侧守卫被跳过)
//   ASTROCS_AHPX_FAULT=writer_accept_legacy_meta → N3 红 (写侧守卫被跳过)
//   ASTROCS_AHPX_FAULT=writer_drop_snr           → P1 红 (snr 块被跳过)
//
// CLI 探针 (验收门用):
//   --emit-legacy <path>  手工构造旧格式文件 (独立于生产 writer)
//   --probe <path>        读侧探针: 接受 rc=0 / 拒绝 rc=2 (并打印原因)
//   --probe-capi <path>   C ABI 探针: 返回 aio_ahpx_read_header 的 rc
// ============================================================================

#include "../../../lib//infrastructure/aio/include/aio_ahpx_format.h"
#include "../../../lib//infrastructure/aio/src/ahpx/aio_ahpx_reader.h"
#include "../../../lib//infrastructure/aio/src/ahpx/aio_ahpx_writer.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include "../../../lib//infrastructure/aio/include/astro_image_io.h"

namespace {

int g_checks = 0;
int g_failures = 0;

#define CHECK(cond)                                                                  do {                                                                                 ++g_checks;                                                                      if (!(cond)) {                                                                       ++g_failures;                                                                    std::fprintf(stderr, "[FAIL] %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        }                                                                        \
    } while (0)

void putU16(std::vector<uint8_t>& v, uint16_t x) {
    v.push_back((uint8_t)(x & 0xFF));
    v.push_back((uint8_t)((x >> 8) & 0xFF));
}

void putU32(std::vector<uint8_t>& v, uint32_t x) {
    for (int i = 0; i < 4; ++i) v.push_back((uint8_t)((x >> (8 * i)) & 0xFF));
}

bool writeAll(const std::string& path, const std::vector<uint8_t>& bytes) {
    FILE* fp = std::fopen(path.c_str(), "wb");
    if (!fp) return false;
    const bool ok = (std::fwrite(bytes.data(), 1, bytes.size(), fp) == bytes.size());
    std::fclose(fp);
    return ok;
}

// 独立手工构造 .ahpx (不调生产 writer): 固定头 18B + 未压缩 JSON 头 + 未压缩块。
// legacy=true ⇒ 头 JSON 带已作废的 "weight" 字段并附 "weight" 块 (旧格式事实形态)。
std::vector<uint8_t> buildAhpxFile(int w, int h, int c,
                                   const std::vector<float>& pixels, bool legacy) {
    std::vector<std::pair<std::string, std::vector<uint8_t>>> blocks;
    std::vector<uint8_t> pixelBytes(pixels.size() * sizeof(float));
    if (!pixels.empty()) std::memcpy(pixelBytes.data(), pixels.data(), pixelBytes.size());
    blocks.push_back(std::make_pair(std::string("pixel"), pixelBytes));
    if (legacy) {
        std::vector<float> wd(pixels.size(), 1.0f);
        std::vector<uint8_t> wb(wd.size() * sizeof(float));
        if (!wd.empty()) std::memcpy(wb.data(), wd.data(), wb.size());
        blocks.push_back(std::make_pair(std::string("weight"), wb));
    }

    std::vector<uint64_t> offs(blocks.size(), 0);
    std::string header;
    for (int iter = 0; iter < 8; ++iter) {
        header = std::string("{\"image\":{\"width\":") + std::to_string(w) +
                 ",\"height\":" + std::to_string(h) +
                 ",\"channels\":" + std::to_string(c) + "}";
        if (legacy) header += ",\"weight\":{\"mode\":2,\"grid_w\":0,\"grid_h\":0}";
        header += ",\"blocks\":[";
        for (size_t i = 0; i < blocks.size(); ++i) {
            if (i) header += ",";
            header += "{\"id\":\"" + blocks[i].first + "\",\"offset\":" +
                      std::to_string(offs[i]) + ",\"size\":" +
                      std::to_string(blocks[i].second.size()) + ",\"codec\":0,\"level\":0}";
        }
        header += "]}";

        std::vector<uint64_t> next(blocks.size(), 0);
        uint64_t cur = 18 + (uint64_t)header.size();
        for (size_t i = 0; i < blocks.size(); ++i) {
            next[i] = cur;
            cur += blocks[i].second.size();
        }
        if (next == offs) break;
        offs = next;
    }

    std::vector<uint8_t> out;
    out.push_back('A'); out.push_back('H'); out.push_back('P'); out.push_back('X');
    putU16(out, 1);                       // VERSION
    putU32(out, (uint32_t)header.size()); // HeaderSize
    putU32(out, 0);                       // HeaderCompSize = 0 (未压缩)
    putU32(out, (uint32_t)blocks.size());
    out.insert(out.end(), header.begin(), header.end());
    for (size_t i = 0; i < blocks.size(); ++i) {
        out.insert(out.end(), blocks[i].second.begin(), blocks[i].second.end());
    }
    return out;
}

std::vector<float> makePixels(int n) {
    std::vector<float> v((size_t)n);
    for (size_t i = 0; i < v.size(); ++i) v[i] = (float)(i + 1) * 0.5f;
    return v;
}

std::vector<float> makeSnr(int n) {
    std::vector<float> v((size_t)n);
    for (size_t i = 0; i < v.size(); ++i) v[i] = 10.0f + (float)i;
    return v;
}

bool bitEqual(const std::vector<float>& a, const std::vector<float>& b) {
    return a.size() == b.size() &&
           (a.empty() || std::memcmp(a.data(), b.data(), a.size() * sizeof(float)) == 0);
}

} // namespace

int main(int argc, char** argv) {
    const int W = 4, H = 3, C = 1;
    const std::vector<float> pixels = makePixels(W * H * C);
    const std::vector<float> snr = makeSnr(W * H);

    const char* fault = std::getenv("ASTROCS_AHPX_FAULT");
    if (fault && *fault) {
        std::fprintf(stderr, "[ahpx-format-test] 故障注入激活: ASTROCS_AHPX_FAULT=%s\n", fault);
    }

    // ---- CLI 探针: 手工构造旧格式文件 ----
    if (argc == 3 && std::strcmp(argv[1], "--emit-legacy") == 0) {
        const bool ok = writeAll(argv[2], buildAhpxFile(W, H, C, pixels, true));
        std::fprintf(stderr, "[ahpx-format-test] 旧格式样本写出 %s: %s\n", argv[2],
                     ok ? "ok" : "fail");
        return ok ? 0 : 1;
    }

    // ---- CLI 探针: 读侧接受/拒绝 ----
    if (argc == 3 && std::strcmp(argv[1], "--probe") == 0) {
        aio::ahpx::AhpxReader reader;
        if (!reader.open(argv[2])) {
            std::fprintf(stderr, "[ahpx-format-test] 读侧拒绝 %s: %s\n", argv[2],
                         reader.getRejectReason().empty() ? "(未给出原因)"
                                                          : reader.getRejectReason().c_str());
            return 2;
        }
        std::fprintf(stderr, "[ahpx-format-test] 读侧接受 %s (blocks=%zu)\n",
                     argv[2], reader.getBlocks().size());
        return 0;
    }

    // ---- CLI 探针: C ABI rc ----
    if (argc == 3 && std::strcmp(argv[1], "--probe-capi") == 0) {
        char hdr[4096];
        const int rc = aio_ahpx_read_header(argv[2], hdr, sizeof(hdr));
        std::fprintf(stderr, "[ahpx-format-test] C ABI aio_ahpx_read_header rc=%d\n", rc);
        return rc;
    }

    namespace fs = std::filesystem;
    std::error_code ec;
    const fs::path dir = fs::temp_directory_path() / "astrocs_ahpx_fix202";
    fs::create_directories(dir, ec);
    const std::string legacyPath = (dir / "legacy_weight.ahpx").string();
    const std::string controlPath = (dir / "control_no_weight.ahpx").string();
    const std::string newPath = (dir / "new_roundtrip.ahpx").string();
    const std::string refusedPath = (dir / "must_not_exist.ahpx").string();
    std::remove(refusedPath.c_str());

    CHECK(writeAll(legacyPath, buildAhpxFile(W, H, C, pixels, true)));
    CHECK(writeAll(controlPath, buildAhpxFile(W, H, C, pixels, false)));

    // ---- N1 负例: 旧格式 (含 weight 字段/块) 必须显式拒绝 ----
    {
        aio::ahpx::AhpxReader r;
        const bool opened = r.open(legacyPath);
        CHECK(!opened);
        CHECK(!r.getRejectReason().empty());
        CHECK(r.getRejectReason().find("weight") != std::string::npos);
        CHECK(r.getRejectReason().find("作废") != std::string::npos);
        CHECK(r.getBlocks().empty());      // 拒绝后不留半解析状态
        CHECK(r.getHeaderJson().empty());
    }

    // ---- N2 对照: 同构造但无 weight 字段/块 ⇒ 必须接受并读出像素 ----
    {
        aio::ahpx::AhpxReader r;
        CHECK(r.open(controlPath));
        CHECK(r.getRejectReason().empty());
        CHECK(r.findBlock("weight") == nullptr);
        CHECK(r.findBlock("pixel") != nullptr);
        CHECK(bitEqual(r.readPixels(), pixels));
    }

    // ---- P1 正例: 新格式读写往返逐位一致, 且不写任何权重 ----
    {
        aio::ahpx::AhpxWriter w;
        w.setMetadata("{\"image\":{\"width\":4,\"height\":3,\"channels\":1}}");
        w.setPixels(pixels.data(), W, H, C);
        w.setSnr(snr.data(), W, H);
        CHECK(w.write(newPath));

        aio::ahpx::AhpxReader r;
        CHECK(r.open(newPath));
        CHECK(r.getRejectReason().empty());
        CHECK(r.findBlock("weight") == nullptr);
        CHECK(r.findBlock("pixel") != nullptr);
        CHECK(r.findBlock("snr") != nullptr);
        CHECK(r.getBlocks().size() == 2);   // pixel + snr, 无第三块
        CHECK(!aio::ahpx::hasJsonKey(r.getHeaderJson(), aio::ahpx::RETIRED_WEIGHT_FIELD));
        CHECK(bitEqual(r.readPixels(), pixels));
        CHECK(bitEqual(r.readSnr(), snr));
    }

    // ---- N3 写侧 fail-closed: 元数据带已作废字段 ⇒ 拒绝且不落盘 ----
    {
        aio::ahpx::AhpxWriter w;
        w.setMetadata("{\"image\":{\"width\":4,\"height\":3,\"channels\":1},"
                      "\"weight\":{\"mode\":0,\"grid_w\":0,\"grid_h\":0}}");
        w.setPixels(pixels.data(), W, H, C);
        CHECK(!w.write(refusedPath));
        CHECK(!fs::exists(refusedPath));
    }

    // ---- C ABI 面 (api.cpp): 旧文件 rc≠0; 新入口写出的文件无 weight 且往返一致 ----
    {
        char hdr[4096];
        CHECK(aio_ahpx_read_header(legacyPath.c_str(), hdr, sizeof(hdr)) != 0);
        CHECK(aio_ahpx_read_header(controlPath.c_str(), hdr, sizeof(hdr)) == 0);

        const std::string apiPath = (dir / "api_roundtrip.ahpx").string();
        CHECK(aio_ahpx_write(apiPath.c_str(), pixels.data(), W, H, C, snr.data(), W, H,
                             "{\"image\":{\"width\":4,\"height\":3,\"channels\":1}}", 5) == 0);
        aio::ahpx::AhpxReader r;
        CHECK(r.open(apiPath));
        CHECK(r.findBlock("weight") == nullptr);
        CHECK(!aio::ahpx::hasJsonKey(r.getHeaderJson(), aio::ahpx::RETIRED_WEIGHT_FIELD));

        std::vector<float> px(pixels.size(), 0.0f);
        std::vector<float> sn(snr.size(), 0.0f);
        int gw = 0, gh = 0, gc = 0, sw = 0, sh = 0;
        CHECK(aio_ahpx_read_pixels(apiPath.c_str(), px.data(), (int)px.size(),
                                   &gw, &gh, &gc) == 0);
        CHECK(aio_ahpx_read_snr(apiPath.c_str(), sn.data(), (int)sn.size(), &sw, &sh) == 0);
        CHECK(gw == W && gh == H && gc == C && sw == W && sh == H);
        CHECK(bitEqual(px, pixels));
        CHECK(bitEqual(sn, snr));
        std::remove(apiPath.c_str());
    }

    std::remove(legacyPath.c_str());
    std::remove(controlPath.c_str());
    std::remove(newPath.c_str());
    std::remove(refusedPath.c_str());
    fs::remove_all(dir, ec);

    std::fprintf(stderr, "[ahpx-format-test] checks=%d failures=%d\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
