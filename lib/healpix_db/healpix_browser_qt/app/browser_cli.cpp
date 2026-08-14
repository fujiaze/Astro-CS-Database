// browser_cli.cpp - HEALPix 浏览器 CLI 后台调试工具
// 功能: DLL 依赖诊断 + 文件加载测试 + 性能 benchmark + 模拟操作
// 用途: 调试阶段诊断浏览器打不开问题, 测试数据层和渲染层性能
// 不依赖 Qt Widgets, 纯 C++17 + core 库 + astro_image_io
// 编译: 见 CMakeLists.txt 的 browser_cli 目标
// 用法:
//   browser_cli.exe <file.hiss|file.hcsd> [选项]
//   browser_cli.exe --hips <products_root> [--queries N]  # HiPS 产品集数据层验证
//   browser_cli.exe --diag               # 仅诊断 DLL 依赖
//   browser_cli.exe <file> --benchmark   # 性能测试
//   browser_cli.exe <file> --sim zoom    # 模拟缩放操作
//   browser_cli.exe <file> --sim pan     # 模拟平移操作

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <algorithm>
#include <string>
#include <vector>
#include <cmath>
#include <map>
#include <random>
#include <set>
#include <windows.h>
#include <psapi.h>

// 浏览器 core 库
#include "browser_backend.h"
#include "stf_engine.h"
#include "healpix_math.h"
#include "hips_browser_backend.h"
#include "hips_sky_view.h"
#include "logger.h"

// astro_image_io DLL (用于诊断 + HiPS Reader)
#include "aio_healpix_io.h"
#include "aio_hips_reader.h"
#include "healpix/healpix_core.h"

// ============================================================================
// 计时工具
// ============================================================================
using Clock = std::chrono::high_resolution_clock;
using TimePoint = std::chrono::time_point<Clock>;

static double elapsed_ms(TimePoint start, TimePoint end) {
    return std::chrono::duration<double, std::milli>(end - start).count();
}

static double elapsed_s(TimePoint start, TimePoint end) {
    return std::chrono::duration<double>(end - start).count();
}

// ============================================================================
// JSON 输出
// ============================================================================
struct JsonOut {
    std::string buf;
    bool first_key = true;

    void begin() { buf = "{\n"; first_key = true; }
    void end() { buf += "\n}\n"; }

    void key_str(const char* k, const std::string& v) {
        if (!first_key) buf += ",\n";
        buf += "  \""; buf += k; buf += "\": \""; buf += v; buf += "\"";
        first_key = false;
    }
    void key_num(const char* k, double v) {
        if (!first_key) buf += ",\n";
        buf += "  \""; buf += k; buf += "\": "; buf += std::to_string(v);
        first_key = false;
    }
    void key_int(const char* k, long long v) {
        if (!first_key) buf += ",\n";
        buf += "  \""; buf += k; buf += "\": "; buf += std::to_string(v);
        first_key = false;
    }
    void key_bool(const char* k, bool v) {
        if (!first_key) buf += ",\n";
        buf += "  \""; buf += k; buf += "\": "; buf += (v ? "true" : "false");
        first_key = false;
    }
};

// ============================================================================
// DLL 诊断
// ============================================================================
struct DllDiag {
    std::string name;
    std::string found_path;
    bool loaded = false;
    std::string error;
};

static DllDiag diag_dll(const char* dll_name) {
    DllDiag d;
    d.name = dll_name;

    // 方法1: LoadLibrary (模拟程序加载 DLL)
    HMODULE h = LoadLibraryA(dll_name);
    if (h) {
        d.loaded = true;
        // 获取完整路径
        char path[MAX_PATH] = {0};
        if (GetModuleFileNameA(h, path, MAX_PATH) > 0) {
            d.found_path = path;
        }
        FreeLibrary(h);
    } else {
        DWORD err = GetLastError();
        d.error = "LoadLibrary 失败, error=" + std::to_string(err);
        if (err == 126) d.error += " (ERROR_MOD_NOT_FOUND: 依赖的 DLL 缺失)";
        else if (err == 127) d.error += " (ERROR_PROC_NOT_FOUND: 函数未找到)";
        else if (err == 193) d.error += " (ERROR_BAD_EXE_FORMAT: 位数不匹配)";
    }

    // 方法2: SearchPath (检查 PATH 中是否能找到)
    if (!d.loaded) {
        char found[MAX_PATH] = {0};
        DWORD len = SearchPathA(NULL, dll_name, NULL, MAX_PATH, found, NULL);
        if (len > 0 && len < MAX_PATH) {
            d.found_path = std::string("PATH 中找到: ") + found;
        }
    }

    return d;
}

// ============================================================================
// 内存使用
// ============================================================================
static size_t get_working_set_mb() {
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return pmc.WorkingSetSize / (1024 * 1024);
    }
    return 0;
}

// ============================================================================
// 主函数
// ============================================================================

// ============================================================================
// HiPS 产品集模式: Browser 正式数据层 = HiPS -> AIO Reader (V4)
// 用法: browser_cli.exe --hips <products_root> [n_queries]
// 输出 JSON: open 信息 / 1024 随机查询与直接 AIO 引用对比 / SNR 目录唯一性
// ============================================================================
static int run_hips_mode(const std::string& dir, JsonOut& json, int n_queries) {
    const int kTileDim = 512;
    const uint64_t kTileMask = (1ULL << 18) - 1;

    HipsBrowserBackend bk;
    TimePoint t0 = Clock::now();
    int rc = bk.open_product(dir);
    TimePoint t1 = Clock::now();
    json.key_str("hips_root", dir);
    json.key_num("hips_open_time_ms", elapsed_ms(t0, t1));
    if (rc != 0) {
        fprintf(stderr, "[ERROR] HiPS open_product 失败, rc=%d\n", rc);
        json.key_bool("hips_open_success", false);
        json.key_int("hips_open_error_code", rc);
        return 1;
    }
    json.key_bool("hips_open_success", true);
    json.key_int("hips_order", bk.get_hips_order());
    json.key_int("hips_leaf_order", bk.get_leaf_order());
    json.key_bool("hips_fp64", bk.is_fp64());
    json.key_int("hips_n_tiles", (long long)bk.get_n_tiles());
    fprintf(stderr, "[OK] HiPS 产品集打开: order=%d leaf_order=%d fp64=%d tiles=%llu\n",
            bk.get_hips_order(), bk.get_leaf_order(), bk.is_fp64() ? 1 : 0,
            (unsigned long long)bk.get_n_tiles());

    // 直接 AIO 引用 (独立句柄 + 逐 tile 缓存)
    AioHipsDataset* dsig = aio_hips_open(dir.c_str(), AIO_HIPS_RD_SIGNAL);
    AioHipsDataset* dsup = aio_hips_open(dir.c_str(), AIO_HIPS_RD_SUPPORT);
    if (!dsig || !dsup) {
        fprintf(stderr, "[ERROR] 直接 AIO 打开失败\n");
        if (dsig) aio_hips_close(dsig);
        if (dsup) aio_hips_close(dsup);
        return 1;
    }
    std::vector<uint64_t> tiles;
    for (int i = 0, n = aio_hips_tile_count(dsig); i < n; ++i) {
        uint64_t ip = 0;
        if (aio_hips_tile_ipix(dsig, i, &ip) == 0) tiles.push_back(ip);
    }
    if (tiles.empty()) {
        fprintf(stderr, "[ERROR] 产品无叶级 tile\n");
        aio_hips_close(dsig); aio_hips_close(dsup);
        return 1;
    }

    std::mt19937_64 rng(20260809ULL);
    const uint32_t nside = uint32_t(1) << (uint32_t)bk.get_leaf_order();
    const bool fp64 = bk.is_fp64();
    std::map<uint64_t, std::vector<double>> ref_sig, ref_sup;
    std::vector<float> tmp((size_t)kTileDim * kTileDim);
    long long mismatch = 0, inside = 0, no_data = 0;
    int outside_ok = 0;

    for (int q = 0; q < n_queries; ++q) {
        const uint64_t tile_ipix = tiles[(size_t)(rng() % tiles.size())];
        const uint64_t z = rng() & kTileMask;
        const uint64_t leaf_ipix = (tile_ipix << 18) | z;
        double ra = 0, dec = 0;
        astrocs::healpix::pix2ang_nest(nside, leaf_ipix, ra, dec);

        double sig_b = 0, sup_b = 0;
        if (bk.query_pixel(ra, dec, sig_b, sup_b) != 0) {
            ++mismatch;
            continue;
        }
        if (ref_sig.count(tile_ipix) == 0) {
            std::vector<double> sig0((size_t)kTileDim * kTileDim);
            std::vector<double> sup0((size_t)kTileDim * kTileDim);
            if (fp64) {
                aio_hips_read_tile_f64(dsig, tile_ipix, sig0.data());
                aio_hips_read_tile_f64(dsup, tile_ipix, sup0.data());
            } else {
                aio_hips_read_tile_f32(dsig, tile_ipix, tmp.data());
                for (size_t i = 0; i < tmp.size(); ++i) sig0[i] = (double)tmp[i];
                aio_hips_read_tile_f32(dsup, tile_ipix, tmp.data());
                for (size_t i = 0; i < tmp.size(); ++i) sup0[i] = (double)tmp[i];
            }
            ref_sig[tile_ipix] = std::move(sig0);
            ref_sup[tile_ipix] = std::move(sup0);
        }
        // V5 (HIPS-IMG-001): 与 Browser 同一共享标准映射 (不再用 z%512/z/512)
        const uint64_t idx = astrocs::healpix::nested_local_to_fits_index(z, 9u, 512u);
        const double sig_d = ref_sig[tile_ipix][idx];
        const double sup_d = ref_sup[tile_ipix][idx];
        ++inside;
        if (!std::isfinite(sig_d)) ++no_data;
        const bool b_ok = std::isfinite(sig_b);
        if (b_ok != std::isfinite(sig_d) ||
            (b_ok && std::fabs(sig_b - sig_d) > 1e-6) ||
            std::fabs(sup_b - sup_d) > 1e-9) {
            ++mismatch;
            if (mismatch <= 5) {
                fprintf(stderr, "MISMATCH q=%d (%.6f,%.6f) sig_b=%g sig_d=%g sup_b=%g sup_d=%g\n",
                        q, ra, dec, sig_b, sig_d, sup_b, sup_d);
            }
        }
    }
    // outside MOC (南天极附近, 测试产品不含该区域时必在 MOC 外)
    for (int i = 0; i < 64; ++i) {
        const double ra = (double)(rng() % 3600) / 10.0;
        const double dec = -89.0 + (double)(rng() % 100) / 100.0;
        double sig = 0, sup = 0;
        const int rcq = bk.query_pixel(ra, dec, sig, sup);
        if (rcq == -2 || (rcq == 0 && !std::isfinite(sig))) ++outside_ok;
    }

    std::vector<double> cra, cdec, csnr;
    std::vector<int64_t> cid;
    std::vector<uint32_t> cqf, cps;
    const int n_snr = bk.read_snr_catalog(cra, cdec, csnr, cid, cqf, cps);
    std::set<int64_t> id_set;
    for (auto id : cid) id_set.insert(id);
    const bool id_unique = ((int)id_set.size() == n_snr);

    json.key_int("hips_queries", n_queries);
    json.key_int("hips_inside", inside);
    json.key_int("hips_no_data", no_data);
    json.key_int("hips_mismatch", mismatch);
    json.key_int("hips_outside_ok", outside_ok);
    json.key_int("hips_snr_rows", n_snr);
    json.key_bool("hips_snr_id_unique", id_unique);
    fprintf(stderr, "RESULT: queries=%d inside=%lld no_data=%lld mismatch=%lld outside_ok=%d/64 snr_rows=%d id_unique=%d\n",
            n_queries, inside, no_data, mismatch, outside_ok, n_snr, id_unique ? 1 : 0);

    aio_hips_close(dsig);
    aio_hips_close(dsup);
    bk.close();

    // SNR catalogue 可选（mosaic/truth 无 snr 产品）：存在时须 id 唯一
    const bool snr_ok = (n_snr <= 0) || (n_snr > 0 && id_unique);
    const bool pass = (mismatch == 0 && outside_ok >= 1 && snr_ok);
    json.key_bool("hips_pass", pass);
    fprintf(stderr, "RESULT: %s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}

// ============================================================================
// V14: STF 延迟 benchmark（G7 Browser profile 的一部分）
// ============================================================================
static double pct(std::vector<double> v, double p);

// ============================================================================
// V14: 全 dataset 像素分位统计（Auto Global 标尺证据 / 诊断）
//  - 遍历全部 leaf tiles，每 tile 均匀 64×64 采样；
//  - 仅统计 finite && support>0 的 signal；
//  - 输出扫描耗时与 p0.5/p1/p50/p95/p99/p99.5/p99.9（判定 Auto Global
//    是否应基于全 dataset 而非首帧视口）。
// 用法: browser_cli --hips <root> --dataset-stats
// ============================================================================
static int run_dataset_stats(const std::string& dir, JsonOut& json,
                             int layer) {
    HipsBrowserBackend bk;
    if (bk.open_product(dir) != 0) {
        fprintf(stderr, "open_product FAIL\n");
        return 3;
    }
    const int leaf = bk.get_hips_order();
    const auto& tiles = bk.tiles_at_order(leaf);
    std::vector<float> samples;
    samples.reserve(tiles.size() * 4096);
    const auto t0 = Clock::now();
    std::size_t read_ok = 0;
    std::vector<float> sig, sup;
    for (std::uint64_t tile : tiles) {
        sig.clear();
        sup.clear();
        if (bk.read_tile_at_order(leaf, tile, sig, sup) != 0) continue;
        ++read_ok;
        for (int y = 0; y < 512; y += 8)
            for (int x = 0; x < 512; x += 8) {
                const std::size_t k = (std::size_t)y * 512u + (std::size_t)x;
                const float s = sig[k];
                if (std::isfinite(s) && sup[k] > 0.0f) samples.push_back(s);
            }
    }
    const double scan_ms = elapsed_ms(t0, Clock::now());
    std::sort(samples.begin(), samples.end());
    const auto pct = [&samples](double q) -> float {
        const double idx = q * (double)(samples.size() - 1);
        const std::size_t lo = (std::size_t)idx;
        const std::size_t hi = std::min(lo + 1, samples.size() - 1);
        return samples[lo] +
               (float)(idx - lo) * (samples[hi] - samples[lo]);
    };
    const float med = samples[samples.size() / 2];
    std::vector<float> dev;
    dev.reserve(samples.size());
    for (float s : samples) dev.push_back(std::fabs(s - med));
    std::sort(dev.begin(), dev.end());
    const float mad = 1.4826f * dev[dev.size() / 2];

    json.key_str("hips_root", dir);
    json.key_int("leaf_order", leaf);
    json.key_int("tiles", (long long)tiles.size());
    json.key_int("tiles_read_ok", (long long)read_ok);
    json.key_int("samples", (long long)samples.size());
    json.key_num("scan_ms", scan_ms);
    json.key_num("p0_5_pct", pct(0.005));
    json.key_num("p1_pct", pct(0.01));
    json.key_num("p50_median", med);
    json.key_num("mad", mad);
    json.key_num("p95_pct", pct(0.95));
    json.key_num("p98_pct", pct(0.98));
    json.key_num("p99_pct", pct(0.99));
    json.key_num("p99_5_pct", pct(0.995));
    json.key_num("p99_9_pct", pct(0.999));
    fprintf(stderr,
            "[dataset-stats] tiles=%llu samples=%llu scan_ms=%.1f "
            "p1=%.6g med=%.6g p99=%.6g\n",
            (unsigned long long)tiles.size(),
            (unsigned long long)samples.size(), scan_ms, pct(0.01), med,
            pct(0.99));
    return 0;
}

// ============================================================================
//  - stretch-only redraw：view 未变时复用已采样 leaves，仅 tone-map（STF 改变
//    不允许重新 sky→HEALPix→FITS decode）。
//  - stf_recompute：Auto STF 重算 robust median/MAD 标尺 + tone-map。
// 用法: browser_cli --hips <root> --stf-bench --view ra,dec,fov [--frames N]
// ============================================================================
static int run_stf_bench(const std::string& dir, JsonOut& json,
                         const std::string& view_str, int frames,
                         int layer) {
    double ra = 0, dec = 0, fov = 8.0;
    if (!view_str.empty()) {
        if (std::sscanf(view_str.c_str(), "%lf,%lf,%lf", &ra, &dec, &fov) != 3)
            return 2;
    }
    HipsBrowserBackend bk;
    if (bk.open_product(dir) != 0) {
        fprintf(stderr, "open_product FAIL\n");
        return 3;
    }
    HipsSkyView sky;
    sky.set_backend(&bk);
    sky.set_size(960, 720);
    sky.set_layer(layer);
    sky.set_stretch("asinh", true);
    sky.set_view(ra, dec, fov, 960.0 / 720.0);

    std::vector<std::uint32_t> rgba;
    sky.rasterize(rgba);  // cold：首次 tile 解码 + 首次 robust STF
    if (frames < 3) frames = 3;

    // stretch-only redraw：view 不变，复用已采样 leaves
    std::vector<double> redraw_ms;
    for (int i = 0; i < frames; ++i) {
        const auto t0 = Clock::now();
        sky.rasterize(rgba);
        redraw_ms.push_back(elapsed_ms(t0, Clock::now()));
    }

    // Auto STF 重算：robust median/MAD + 亮端 clip + tone-map（复用 leaves）
    std::vector<double> stf_ms;
    for (int i = 0; i < frames; ++i) {
        sky.refresh_auto_range();
        const auto t0 = Clock::now();
        sky.rasterize(rgba);
        stf_ms.push_back(elapsed_ms(t0, Clock::now()));
    }

    json.key_str("hips_root", dir);
    json.key_str("view", view_str);
    json.key_int("frames", frames);
    json.key_num("stf_recompute_ms_p50", pct(stf_ms, 0.50));
    json.key_num("stf_recompute_ms_p95", pct(stf_ms, 0.95));
    json.key_num("stf_recompute_ms_max",
                 stf_ms.empty() ? 0.0
                                : *std::max_element(stf_ms.begin(),
                                                    stf_ms.end()));
    json.key_num("stretch_only_ms_p50", pct(redraw_ms, 0.50));
    json.key_num("stretch_only_ms_p95", pct(redraw_ms, 0.95));
    json.key_num("stretch_only_ms_max",
                 redraw_ms.empty() ? 0.0
                                   : *std::max_element(redraw_ms.begin(),
                                                       redraw_ms.end()));
    json.key_num("range_lo", sky.range_lo());
    json.key_num("range_hi", sky.range_hi());
    json.key_int("peak_ram_mb", (long long)get_working_set_mb());
    fprintf(stderr,
            "[stf-bench] recompute_p50=%.2f recompute_p95=%.2f "
            "stretch_only_p50=%.2f stretch_only_p95=%.2f ms\n",
            pct(stf_ms, 0.50), pct(stf_ms, 0.95), pct(redraw_ms, 0.50),
            pct(redraw_ms, 0.95));
    return 0;
}

// ============================================================================
// V14: Lock STF 行为探针（G6 Lock/Reset 验收）
//  - 锁定后 pan/zoom、auto view 模式切换、Reset 均不得改动 lo/hi；
//  - 解锁后 Reset 恢复重算（证明锁定是冻结标尺的原因）。
// 用法: browser_cli --hips <root> --stf-lock-probe --view ra,dec,fov
// ============================================================================
static int run_stf_lock_probe(const std::string& dir, JsonOut& json,
                              const std::string& view_str, int layer) {
    double ra = 0, dec = 0, fov = 8.0;
    if (!view_str.empty()) {
        if (std::sscanf(view_str.c_str(), "%lf,%lf,%lf", &ra, &dec, &fov) != 3)
            return 2;
    }
    HipsBrowserBackend bk;
    if (bk.open_product(dir) != 0) {
        fprintf(stderr, "open_product FAIL\n");
        return 3;
    }
    HipsSkyView sky;
    sky.set_backend(&bk);
    sky.set_size(960, 720);
    sky.set_layer(layer);
    sky.set_stretch("asinh", true);
    sky.set_view(ra, dec, fov, 960.0 / 720.0);

    std::vector<std::uint32_t> rgba;
    sky.rasterize(rgba);  // 首次 robust STF 计算
    const float lo0 = sky.range_lo();
    const float hi0 = sky.range_hi();
    const std::uint64_t c0 = sky.auto_recompute_count();

    sky.set_stf_locked(true);
    sky.set_auto_view(true);  // 锁定后模式切换不得重算
    const double ra2 = std::fmod(ra + 3.0, 360.0);
    const double dec2 = dec + 0.5;
    const double fov2 = fov * 0.8;
    sky.set_view(ra2, dec2, fov2, 960.0 / 720.0);
    sky.rasterize(rgba);
    const float lo_pan = sky.range_lo();
    const float hi_pan = sky.range_hi();
    const std::uint64_t c_locked_pan = sky.auto_recompute_count();

    sky.refresh_auto_range();  // 锁定后 Reset 不得重算
    sky.rasterize(rgba);
    const float lo_reset = sky.range_lo();
    const float hi_reset = sky.range_hi();
    const std::uint64_t c_locked_reset = sky.auto_recompute_count();

    sky.set_stf_locked(false);
    sky.refresh_auto_range();
    sky.rasterize(rgba);
    const float lo_unlock = sky.range_lo();
    const float hi_unlock = sky.range_hi();
    const std::uint64_t c_unlock = sky.auto_recompute_count();

    const bool pan_frozen = (lo_pan == lo0 && hi_pan == hi0);
    const bool reset_frozen = (lo_reset == lo0 && hi_reset == hi0);
    const bool lock_blocks_recompute =
        (c_locked_pan == c0 && c_locked_reset == c0);
    const bool unlock_recomputes = (c_unlock > c_locked_reset);
    const bool pass = pan_frozen && reset_frozen && unlock_recomputes;

    json.key_str("hips_root", dir);
    json.key_int("auto_recompute_count_initial", (long long)c0);
    json.key_int("auto_recompute_count_locked_pan", (long long)c_locked_pan);
    json.key_int("auto_recompute_count_locked_reset", (long long)c_locked_reset);
    json.key_int("auto_recompute_count_unlocked", (long long)c_unlock);
    json.key_num("range_lo_initial", lo0);
    json.key_num("range_hi_initial", hi0);
    json.key_num("range_lo_locked_pan", lo_pan);
    json.key_num("range_hi_locked_pan", hi_pan);
    json.key_num("range_lo_locked_reset", lo_reset);
    json.key_num("range_hi_locked_reset", hi_reset);
    json.key_num("range_lo_after_unlock", lo_unlock);
    json.key_num("range_hi_after_unlock", hi_unlock);
    json.key_bool("lock_pan_frozen", pan_frozen);
    json.key_bool("lock_reset_frozen", reset_frozen);
    json.key_bool("lock_blocks_recompute", lock_blocks_recompute);
    json.key_bool("unlock_recomputes", unlock_recomputes);
    json.key_bool("lock_probe_pass", pass);
    fprintf(stderr,
            "[stf-lock-probe] pass=%d counts=%llu->%llu/%llu->%llu "
            "lo0=%.6g lo_pan=%.6g lo_reset=%.6g lo_unlock=%.6g\n",
            pass ? 1 : 0, (unsigned long long)c0,
            (unsigned long long)c_locked_pan, (unsigned long long)c_locked_reset,
            (unsigned long long)c_unlock, lo0, lo_pan, lo_reset, lo_unlock);
    return pass ? 0 : 4;
}

// ============================================================================
// V14 v3: 手动 STF 渲染语义探针
//  - midtones 拉最左（0.05）应整体提亮（亮像素占比高）；
//  - midtones 拉最右（0.95）应整体变暗；
//  - 验证手动控制点（shadows/highlights/midtones）真实进入渲染路径。
// 用法: browser_cli --hips <root> --stf-manual-probe --view ra,dec,fov
// ============================================================================
static int run_stf_manual_probe(const std::string& dir, JsonOut& json,
                                const std::string& view_str, int layer) {
    double ra = 0, dec = 0, fov = 8.0;
    if (!view_str.empty()) {
        if (std::sscanf(view_str.c_str(), "%lf,%lf,%lf", &ra, &dec, &fov) != 3)
            return 2;
    }
    HipsBrowserBackend bk;
    if (bk.open_product(dir) != 0) return 3;
    HipsSkyView sky;
    sky.set_backend(&bk);
    sky.set_size(960, 720);
    sky.set_layer(layer);
    sky.set_stretch("asinh", true);
    sky.set_view(ra, dec, fov, 960.0 / 720.0);

    std::vector<std::uint32_t> rgba;
    sky.rasterize(rgba);  // auto 标尺（含全 dataset 扫描）

    auto bright_fraction = [&](const std::vector<std::uint32_t>& px) {
        std::size_t bright = 0, total = 0;
        for (std::uint32_t c : px) {
            const std::uint32_t g = (c >> 8) & 0xFF;
            if (c == 0xFF14181F) continue;  // 空区背景不计
            ++total;
            if (g > 128) ++bright;
        }
        return total ? (double)bright / (double)total : 0.0;
    };

    STFParams bright;   // 中间调最左 → 提亮
    bright.shadows = 0.0f;
    bright.highlights = 1.0f;
    bright.midtones = 0.05f;
    bright.compression = 0.5f;  // asinh 预设压缩
    sky.set_manual_stf(bright);
    sky.rasterize(rgba);
    const double bright_frac = bright_fraction(rgba);

    STFParams dark;     // 中间调最右 → 变暗
    dark.shadows = 0.0f;
    dark.highlights = 1.0f;
    dark.midtones = 0.95f;
    dark.compression = 0.5f;
    sky.set_manual_stf(dark);
    sky.rasterize(rgba);
    const double dark_frac = bright_fraction(rgba);

    const bool pass = bright_frac > 0.5 && dark_frac < bright_frac * 0.5;
    json.key_str("hips_root", dir);
    json.key_num("midtones_0_05_bright_frac", bright_frac);
    json.key_num("midtones_0_95_bright_frac", dark_frac);
    json.key_bool("manual_stf_probe_pass", pass);
    fprintf(stderr, "[stf-manual-probe] bright(m=0.05)=%.3f dark(m=0.95)=%.3f pass=%d\n",
            bright_frac, dark_frac, pass ? 1 : 0);
    return pass ? 0 : 6;
}

// ============================================================================
// V14: 10 分钟内存有界 soak（G6 验收）
//  - 连续 pan/zoom 扫描（FOV 0.5°~14.75° 循环，触发跨 order/tile 解码）；
//  - 每 5 秒采样工作集；LRU 有界缓存应使内存保持平坦（无单调增长）。
// 用法: browser_cli --hips <root> --soak <seconds> [--view ra,dec,fov]
// ============================================================================
static int run_mem_soak(const std::string& dir, JsonOut& json,
                        const std::string& view_str, int seconds,
                        int layer) {
    if (seconds < 30) seconds = 30;
    double ra = 0, dec = 0, fov = 8.0;
    if (!view_str.empty()) {
        if (std::sscanf(view_str.c_str(), "%lf,%lf,%lf", &ra, &dec, &fov) != 3)
            return 2;
    }
    HipsBrowserBackend bk;
    if (bk.open_product(dir) != 0) {
        fprintf(stderr, "open_product FAIL\n");
        return 3;
    }
    HipsSkyView sky;
    sky.set_backend(&bk);
    sky.set_size(960, 720);
    sky.set_layer(layer);
    sky.set_stretch("asinh", true);
    sky.set_view(ra, dec, fov, 960.0 / 720.0);

    std::vector<std::uint32_t> rgba;
    sky.rasterize(rgba);  // cold
    const double ram0 = (double)get_working_set_mb();

    const auto t_end = Clock::now() + std::chrono::seconds(seconds);
    std::size_t frames = 0;
    std::vector<double> ram_samples;
    auto t_last = Clock::now();
    double c_ra = ra, c_dec = dec;
    while (Clock::now() < t_end) {
        c_ra = std::fmod(c_ra + 0.7, 360.0);
        const int k = (int)(frames % 9);
        c_dec = dec + (double)(k - 4) * 0.35;
        if (c_dec > 85.0) c_dec = 85.0;
        if (c_dec < -85.0) c_dec = -85.0;
        const double z_fov =
            0.5 + (double)(frames % 20) * 0.75;  // 0.5°~14.75° 扫描
        sky.set_view(c_ra, c_dec, z_fov, 960.0 / 720.0);
        sky.rasterize(rgba);
        ++frames;
        const auto now = Clock::now();
        if (std::chrono::duration<double>(now - t_last).count() >= 5.0) {
            ram_samples.push_back((double)get_working_set_mb());
            t_last = now;
        }
    }
    const double final_ram = (double)get_working_set_mb();
    const double peak_ram = ram_samples.empty()
                                ? final_ram
                                : *std::max_element(ram_samples.begin(),
                                                    ram_samples.end());
    std::vector<double> sorted = ram_samples;
    std::sort(sorted.begin(), sorted.end());
    const double median_ram =
        sorted.empty() ? final_ram : sorted[sorted.size() / 2];
    const bool bounded = peak_ram <= 512.0 &&
                         final_ram <= peak_ram + 8.0 &&
                         median_ram <= 256.0;

    json.key_str("hips_root", dir);
    json.key_int("soak_seconds", seconds);
    json.key_int("frames", (long long)frames);
    json.key_int("ram_samples", (long long)ram_samples.size());
    json.key_num("ram_initial_mb", ram0);
    json.key_num("ram_final_mb", final_ram);
    json.key_num("ram_peak_mb", peak_ram);
    json.key_num("ram_median_mb", median_ram);
    json.key_int("cache_evictions", (long long)sky.metrics().evictions);
    json.key_int("tiles_decoded_total",
                 (long long)sky.metrics().total_tile_reads);
    json.key_bool("mem_bounded_pass", bounded);
    fprintf(stderr,
            "[soak] seconds=%d frames=%llu ram0=%.0f ram_peak=%.0f "
            "ram_median=%.0f ram_final=%.0f evict=%llu bounded=%d\n",
            seconds, (unsigned long long)frames, ram0, peak_ram, median_ram,
            final_ram, (unsigned long long)sky.metrics().evictions,
            bounded ? 1 : 0);
    return bounded ? 0 : 5;
}

// ============================================================================
// HiPS 2D 天空视图渲染 benchmark（V9 P9-5）
// 用法: browser_cli --hips <root> --benchmark --view ra,dec,fov [--frames N]
//       [--layer signal|support]
// ============================================================================
static double pct(std::vector<double> v, double p) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    const std::size_t idx =
        (std::size_t)(p * (double)(v.size() - 1));
    return v[idx];
}

static int run_hips_raster_bench(const std::string& dir, JsonOut& json,
                                 const std::string& view_str, int frames,
                                 int layer) {
    double ra = 0, dec = 0, fov = 8.0;
    if (!view_str.empty()) {
        if (std::sscanf(view_str.c_str(), "%lf,%lf,%lf", &ra, &dec, &fov) != 3)
            return 2;
    }
    HipsBrowserBackend bk;
    if (bk.open_product(dir) != 0) {
        fprintf(stderr, "open_product FAIL\n");
        return 3;
    }
    HipsSkyView sky;
    sky.set_backend(&bk);
    sky.set_size(960, 720);
    sky.set_layer(layer);
    sky.set_stretch("asinh", true);
    sky.set_view(ra, dec, fov, 960.0 / 720.0);

    std::vector<std::uint32_t> rgba;
    HipsSkyView::Stats st;
    sky.rasterize(rgba);  // cold（含首次 tile 解码）
    const double cold_ms = sky.last_stats().frame_ms;
    const double first_visible_ms = cold_ms;
    fprintf(stderr, "[raster] cold_start=%.2f ms order=%d tiles_decoded=%llu\n",
            cold_ms, sky.last_stats().order,
            (unsigned long long)sky.last_stats().tiles_decoded);

    std::vector<double> pan_ms, zoom_ms;
    double c_ra = ra, c_dec = dec;
    for (int f = 0; f < frames; ++f) {
        c_ra = std::fmod(c_ra + 2.0, 360.0);
        sky.set_view(c_ra, c_dec + ((f % 3) - 1), 8.0, 960.0 / 720.0);
        sky.rasterize(rgba);
        pan_ms.push_back(sky.last_stats().frame_ms);
    }
    double z_fov = fov;
    for (int f = 0; f < frames; ++f) {
        z_fov *= 0.92;
        if (z_fov < 0.1) z_fov = 30.0;
        sky.set_view(ra, dec, z_fov, 960.0 / 720.0);
        sky.rasterize(rgba);
        zoom_ms.push_back(sky.last_stats().frame_ms);
    }

    const auto& m = sky.metrics();
    std::vector<double> decode = m.decode_ms_hist;
    const std::size_t total_lookups = m.cache_hits_total + m.cache_misses;
    const double hit_rate =
        total_lookups ? (double)m.cache_hits_total / (double)total_lookups : 0.0;

    json.key_str("hips_root", dir);
    json.key_num("cold_start_ms", cold_ms);
    json.key_num("first_visible_ms", first_visible_ms);
    json.key_num("pan_frame_ms_p50", pct(pan_ms, 0.50));
    json.key_num("pan_frame_ms_p95", pct(pan_ms, 0.95));
    json.key_num("pan_frame_ms_max", pan_ms.empty() ? 0.0
                                                    : *std::max_element(
                                                          pan_ms.begin(),
                                                          pan_ms.end()));
    json.key_num("zoom_frame_ms_p50", pct(zoom_ms, 0.50));
    json.key_num("zoom_frame_ms_p95", pct(zoom_ms, 0.95));
    json.key_num("tile_decode_ms_p50", pct(decode, 0.50));
    json.key_num("tile_decode_ms_p95", pct(decode, 0.95));
    json.key_num("cache_hit_rate", hit_rate);
    json.key_int("tiles_decoded_total", (long long)m.total_tile_reads);
    json.key_int("cache_evictions", (long long)m.evictions);
    json.key_int("raster_frames", frames * 2);
    json.key_int("peak_ram_mb", (long long)get_working_set_mb());
    fprintf(stderr,
            "[raster] pan_p50=%.1f pan_p95=%.1f zoom_p50=%.1f decode_p50=%.1f "
            "decode_p95=%.1f hit=%.3f evict=%llu\n",
            pct(pan_ms, 0.50), pct(pan_ms, 0.95), pct(zoom_ms, 0.50),
            pct(decode, 0.50), pct(decode, 0.95), hit_rate,
            (unsigned long long)m.evictions);
    return 0;
}

// ============================================================================
// V10 P10-2: 独立 headless reference renderer（slow but trusted）
//  - 不调用 widget/HipsSkyView 绘制路径；
//  - screen->sky 独立实现（gnomonic 逆投影）；
//  - 采样走可信 backend 点查询（AIO leaf order，V9 已过 mismatch=0）；
//  - 固定 linear stretch（0.5%/99.5% 分位由本视图子采样计算）。
// 用法: browser_cli --hips <root> --refrender <out.ppm>
//                   --view ra,dec,fov --size WxH [--layer signal|support]
// ============================================================================
static int run_reference_render(const std::string& dir, const std::string& out,
                                const std::string& view_str, int w, int h,
                                int layer) {
    double ra = 0, dec = 0, fov = 8.0;
    if (std::sscanf(view_str.c_str(), "%lf,%lf,%lf", &ra, &dec, &fov) != 3)
        return 2;
    HipsBrowserBackend bk;
    if (bk.open_product(dir) != 0) return 3;

    const double kPi = 3.14159265358979323846;
    const double tan_half = std::tan(fov * kPi / 360.0);
    const double r0 = ra * kPi / 180.0;
    const double d0 = dec * kPi / 180.0;
    const double aspect = (double)w / (double)h;

    // leaf tile 缓存（reference 独立于 widget/HipsSkyView；点查询语义一致）
    const std::uint32_t leaf_nside = 1u << (std::uint32_t)bk.get_leaf_order();
    const std::uint64_t kMask = (1ULL << 18) - 1;
    // 存在性集合：避免对缺失 tile 逐像素做 CFITSIO 失败打开（~0.8ms/次）
    std::set<std::uint64_t> leaf_tiles;
    for (std::uint64_t t : bk.tiles_at_order(bk.get_hips_order()))
        leaf_tiles.insert(t);
    std::map<std::uint64_t, std::pair<std::vector<double>, std::vector<double>>>
        tile_cache;
    auto sample_leaf = [&](double ra_deg, double dec_deg, double* sig,
                           double* sup) -> bool {
        const std::uint64_t leaf =
            astrocs::healpix::ang2pix_nest(leaf_nside, ra_deg, dec_deg);
        const std::uint64_t tile = leaf >> 18;
        const std::uint64_t local = leaf & kMask;
        auto it = tile_cache.find(tile);
        if (it == tile_cache.end()) {
            if (!leaf_tiles.count(tile)) return false;
            std::vector<double> s, u;
            if (bk.read_tile(tile, s) != 0 ||
                bk.read_support_tile(tile, u) != 0)
                return false;
            it = tile_cache.emplace(tile, std::make_pair(std::move(s),
                                                         std::move(u)))
                     .first;
        }
        const std::uint64_t fi =
            astrocs::healpix::nested_local_to_fits_index(local, 9u, 512u);
        *sig = it->second.first[(size_t)fi];
        *sup = it->second.second[(size_t)fi];
        return true;
    };

    // 子采样收集范围（signal 层固定线性拉伸）
    auto tp0 = Clock::now();
    float dmin = 0.0f, dmax = 1.0f;
    if (layer == 0) {
        std::vector<float> vals;
        for (int j = 0; j < h; j += 4) {
            const double v = 1.0 - 2.0 * (j + 0.5) / (double)h;
            for (int i = 0; i < w; i += 4) {
                const double u = 2.0 * (i + 0.5) / (double)w - 1.0;
                const double xi = -u * tan_half * aspect;
                const double eta = v * tan_half;
                const double rr = std::hypot(xi, eta);
                double rar, dr;
                if (rr < 1e-12) {
                    dr = d0;
                    rar = r0;
                } else {
                    const double c = std::atan(rr);
                    const double sc = std::sin(c), cc = std::cos(c);
                    dr = std::asin(cc * std::sin(d0) +
                                   eta * sc * std::cos(d0) / rr);
                    rar = r0 + std::atan2(xi * sc,
                                          rr * std::cos(d0) * cc -
                                              eta * std::sin(d0) * sc);
                }
                rar = std::fmod(rar, 2.0 * kPi);
                if (rar < 0.0) rar += 2.0 * kPi;
                double sig = 0, sup = 0;
                if (sample_leaf(rar * 180.0 / kPi, dr * 180.0 / kPi, &sig,
                                &sup) &&
                    std::isfinite(sig)) {
                    vals.push_back((float)sig);
                }
            }
        }
        if (!vals.empty()) {
            std::sort(vals.begin(), vals.end());
            const std::size_t n = vals.size();
            dmin = vals[(std::size_t)(0.005 * (double)(n - 1))];
            dmax = vals[(std::size_t)(0.995 * (double)(n - 1))];
            if (dmax <= dmin) dmax = dmin + 1.0f;
        }
    }
    auto tp1 = Clock::now();
    fprintf(stderr, "[refrender] range pass %.1f ms\n",
            elapsed_ms(tp0, tp1));

    std::vector<unsigned char> rgb((std::size_t)w * (std::size_t)h * 3);
    auto tp2 = Clock::now();
    auto tp_seg = tp2;
    std::size_t seg_target = 100000;
    for (int j = 0; j < h; ++j) {
        const double v = 1.0 - 2.0 * (j + 0.5) / (double)h;
        for (int i = 0; i < w; ++i) {
            const double u = 2.0 * (i + 0.5) / (double)w - 1.0;
            const double xi = -u * tan_half * aspect;
            const double eta = v * tan_half;
            const double rr = std::hypot(xi, eta);
            double rar, dr;
            if (rr < 1e-12) {
                dr = d0;
                rar = r0;
            } else {
                const double c = std::atan(rr);
                const double sc = std::sin(c), cc = std::cos(c);
                dr = std::asin(cc * std::sin(d0) +
                               eta * sc * std::cos(d0) / rr);
                rar = r0 + std::atan2(xi * sc,
                                      rr * std::cos(d0) * cc -
                                          eta * std::sin(d0) * sc);
            }
            rar = std::fmod(rar, 2.0 * kPi);
            if (rar < 0.0) rar += 2.0 * kPi;
            double sig = 0, sup = 0;
            unsigned char g = 0;
            if (sample_leaf(rar * 180.0 / kPi, dr * 180.0 / kPi, &sig,
                            &sup)) {
                if (layer == 0) {
                    if (std::isfinite(sig)) {
                        const float x =
                            (float)((sig - dmin) / (dmax - dmin));
                        g = (unsigned char)std::max(
                            0.0f, std::min(255.0f, x * 255.0f));
                    }
                } else {
                    const float s = (float)std::max(0.0, std::min(1.0, sup));
                    g = (unsigned char)std::max(
                        0.0f, std::min(255.0f, std::sqrt(s) * 255.0f));
                }
            }
            const std::size_t k =
                ((std::size_t)j * (std::size_t)w + (std::size_t)i) * 3;
            rgb[k] = g;
            rgb[k + 1] = g;
            rgb[k + 2] = g;
            const std::size_t idx =
                (std::size_t)j * (std::size_t)w + (std::size_t)i;
            if (idx == 2000 || idx == seg_target) {
                fprintf(stderr, "[refrender] probe to %zu px = %.1f ms\n", idx,
                        elapsed_ms(tp_seg, Clock::now()));
                tp_seg = Clock::now();
                seg_target = (idx == 2000) ? 100000 : (std::size_t)w * (std::size_t)h;
            }
        }
    }
    auto tp3 = Clock::now();
    fprintf(stderr, "[refrender] raster pass %.1f ms (cache tiles=%llu)\n",
            elapsed_ms(tp2, tp3), (unsigned long long)tile_cache.size());
    std::FILE* f = std::fopen(out.c_str(), "wb");
    if (!f) return 4;
    std::fprintf(f, "P6\n%d %d\n255\n", w, h);
    std::fwrite(rgb.data(), 1, rgb.size(), f);
    std::fclose(f);
    fprintf(stderr, "[refrender] %s %dx%d layer=%d dmin=%.4f dmax=%.4f\n",
            out.c_str(), w, h, layer, dmin, dmax);
    return 0;
}

int main(int argc, char* argv[]) {
    // 解析命令行
    std::string file_path;
    bool diag_only = false;
    bool benchmark = false;
    bool sim_zoom = false;
    bool sim_pan = false;
    bool stf_bench = false;
    bool stf_lock_probe = false;
    bool stf_manual_probe = false;
    bool dataset_stats = false;
    int soak_seconds = 0;
    bool hips_mode = false;
    int hips_queries = 1024;
    int sim_frames = 100;
    std::string view_str;
    std::string layer_str;
    std::string ref_out;
    int ref_w = 960, ref_h = 720;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--diag" || arg == "-d") {
            diag_only = true;
        } else if (arg == "--benchmark" || arg == "-b") {
            benchmark = true;
        } else if (arg == "--stf-bench") {
            stf_bench = true;
        } else if (arg == "--stf-lock-probe") {
            stf_lock_probe = true;
        } else if (arg == "--stf-manual-probe") {
            stf_manual_probe = true;
        } else if (arg == "--dataset-stats") {
            dataset_stats = true;
        } else if (arg == "--soak") {
            if (i + 1 < argc) soak_seconds = atoi(argv[++i]);
        } else if (arg == "--sim") {
            if (i + 1 < argc) {
                std::string sim_type = argv[++i];
                if (sim_type == "zoom") sim_zoom = true;
                else if (sim_type == "pan") sim_pan = true;
            }
        } else if (arg == "--frames") {
            if (i + 1 < argc) sim_frames = atoi(argv[++i]);
        } else if (arg == "--hips") {
            hips_mode = true;
        } else if (arg == "--queries") {
            if (i + 1 < argc) hips_queries = atoi(argv[++i]);
        } else if (arg == "--view") {
            if (i + 1 < argc) view_str = argv[++i];
        } else if (arg == "--layer") {
            if (i + 1 < argc) layer_str = argv[++i];
        } else if (arg == "--refrender") {
            if (i + 1 < argc) ref_out = argv[++i];
        } else if (arg == "--size") {
            if (i + 1 < argc) {
                if (std::sscanf(argv[i + 1], "%dx%d", &ref_w, &ref_h) != 2)
                    return 2;
                ++i;
            }
        } else if (arg == "--help" || arg == "-h") {
            fprintf(stderr, "HEALPix 浏览器 CLI 后台调试工具\n\n");
            fprintf(stderr, "用法:\n");
            fprintf(stderr, "  browser_cli.exe <file> [选项]\n");
            fprintf(stderr, "  browser_cli.exe --diag\n");
            fprintf(stderr, "  browser_cli.exe --hips <products_root> --queries N\n\n");
            fprintf(stderr, "选项:\n");
            fprintf(stderr, "  --diag           仅诊断 DLL 依赖, 不加载文件\n");
            fprintf(stderr, "  --hips           以 HiPS 产品集根目录打开 (signal/support/snr), 验证 Browser 数据层\n");
            fprintf(stderr, "  --queries N      HiPS 模式随机查询数 (默认 1024)\n");
            fprintf(stderr, "  --benchmark      性能测试 (文件打开 + 子叶加载 + 降采样)\n");
            fprintf(stderr, "  --stf-bench      浏览器 STF 延迟测试 (recompute vs stretch-only)\n");
            fprintf(stderr, "  --stf-lock-probe Lock STF 行为验证 (锁定冻结标尺/解锁恢复)\n");
            fprintf(stderr, "  --stf-manual-probe 手动 STF 渲染语义验证 (midtones 两端)\n");
            fprintf(stderr, "  --dataset-stats  全 dataset 分位统计 (Auto Global 标尺证据)\n");
            fprintf(stderr, "  --soak N         内存有界 soak 测试 N 秒 (pan/zoom 扫描 + RAM 采样)\n");
            fprintf(stderr, "  --sim zoom       模拟缩放操作 (测试视角变化性能)\n");
            fprintf(stderr, "  --sim pan        模拟平移操作 (测试视角变化性能)\n");
            fprintf(stderr, "  --frames N       模拟操作帧数 (默认 100)\n");
            fprintf(stderr, "  --help           显示帮助\n\n");
            fprintf(stderr, "输出: JSON 报告到 stdout, 详细日志到 stderr\n");
            return 0;
        } else if (arg[0] != '-') {
            file_path = arg;
        }
    }

    JsonOut json;
    json.begin();
    json.key_str("tool", "browser_cli v1.0");

    // ================================================================
    // 1. DLL 依赖诊断
    // ================================================================
    fprintf(stderr, "\n========== DLL 依赖诊断 ==========\n");

    const char* dlls[] = {
        "astro_image_io.dll",
        "libgcc_s_seh-1.dll",
        "libstdc++-6.dll",
        "libwinpthread-1.dll",
        "Qt6Core.dll",
        "Qt6Gui.dll",
        "Qt6Widgets.dll",
        "Qt6OpenGL.dll",
        "Qt6OpenGLWidgets.dll",
    };

    bool all_dlls_ok = true;
    fprintf(stderr, "%-25s %-8s %s\n", "DLL", "状态", "路径/错误");
    fprintf(stderr, "-----------------------------------------------------------------\n");

    // DLL 诊断结果存入 JSON
    json.key_str("dll_diag", "");
    // 覆盖: 改用更结构化的方式
    for (int i = 0; i < 9; i++) {
        DllDiag d = diag_dll(dlls[i]);
        const char* status = d.loaded ? "OK" : "FAIL";
        fprintf(stderr, "%-25s %-8s %s\n",
                d.name.c_str(), status,
                d.loaded ? d.found_path.c_str() : d.error.c_str());
        if (!d.loaded) all_dlls_ok = false;
    }

    // 关键 DLL: astro_image_io (浏览器核心依赖)
    DllDiag aio = diag_dll("astro_image_io.dll");
    json.key_bool("astro_image_io_loaded", aio.loaded);
    json.key_str("astro_image_io_path", aio.found_path);
    if (!aio.loaded) {
        json.key_str("astro_image_io_error", aio.error);
    }

    if (!all_dlls_ok) {
        fprintf(stderr, "\n[ERROR] 部分 DLL 加载失败! 浏览器可能无法启动.\n");
        fprintf(stderr, "[HINT] 确保以下目录在 PATH 中:\n");
        fprintf(stderr, "  1. C:\\msys64\\mingw64\\bin  (Qt6 + MinGW runtime)\n");
        fprintf(stderr, "  2. lib\\astro_image_io\\    (astro_image_io.dll)\n");
        fprintf(stderr, "[HINT] 或使用 windeployqt 部署 Qt DLL 到 exe 同级目录\n");
    } else {
        fprintf(stderr, "\n[OK] 所有 DLL 依赖正常.\n");
    }

    if (diag_only) {
        json.key_bool("all_dlls_ok", all_dlls_ok);
        json.end();
        printf("%s", json.buf.c_str());
        return all_dlls_ok ? 0 : 1;
    }

    // ================================================================
    // 2. 文件加载测试
    // ================================================================
    if (file_path.empty()) {
        fprintf(stderr, "\n[ERROR] 未指定文件路径\n");
        fprintf(stderr, "用法: browser_cli.exe <file.hiss|file.hcsd> [选项] | --hips <products_root> [n_queries]\n");
        json.key_bool("error_no_file", true);
        json.end();
        printf("%s", json.buf.c_str());
        return 1;
    }

    if (hips_mode) {
        fprintf(stderr, "\n========== HiPS 产品集加载测试 ==========\n");
        fprintf(stderr, "产品: %s\n", file_path.c_str());
        int rc_h = 0;
        if (benchmark) {
            const int layer = (layer_str == "support") ? 1 : 0;
            rc_h = run_hips_raster_bench(file_path, json, view_str,
                                         sim_frames, layer);
        } else if (stf_bench) {
            const int layer = (layer_str == "support") ? 1 : 0;
            rc_h = run_stf_bench(file_path, json, view_str, sim_frames, layer);
        } else if (stf_lock_probe) {
            const int layer = (layer_str == "support") ? 1 : 0;
            rc_h = run_stf_lock_probe(file_path, json, view_str, layer);
        } else if (stf_manual_probe) {
            const int layer = (layer_str == "support") ? 1 : 0;
            rc_h = run_stf_manual_probe(file_path, json, view_str, layer);
        } else if (dataset_stats) {
            const int layer = (layer_str == "support") ? 1 : 0;
            rc_h = run_dataset_stats(file_path, json, layer);
        } else if (soak_seconds > 0) {
            const int layer = (layer_str == "support") ? 1 : 0;
            rc_h = run_mem_soak(file_path, json, view_str, soak_seconds, layer);
        } else if (!ref_out.empty()) {
            const int layer = (layer_str == "support") ? 1 : 0;
            rc_h = run_reference_render(file_path, ref_out, view_str, ref_w,
                                        ref_h, layer);
        } else {
            rc_h = run_hips_mode(file_path, json, hips_queries);
        }
        json.end();
        printf("%s", json.buf.c_str());
        return rc_h;
    }

    fprintf(stderr, "\n========== 文件加载测试 ==========\n");
    fprintf(stderr, "文件: %s\n", file_path.c_str());

    BrowserBackend backend;
    TimePoint t0 = Clock::now();
    int rc = backend.open_file(file_path);
    TimePoint t1 = Clock::now();
    double open_ms = elapsed_ms(t0, t1);

    json.key_str("file_path", file_path);
    json.key_num("open_time_ms", open_ms);

    if (rc != 0) {
        fprintf(stderr, "[ERROR] open_file 失败, rc=%d\n", rc);
        fprintf(stderr, "[HINT] 检查文件路径是否正确, 文件是否完整\n");
        json.key_bool("open_success", false);
        json.key_int("open_error_code", rc);
        json.end();
        printf("%s", json.buf.c_str());
        return 1;
    }

    fprintf(stderr, "[OK] 文件加载成功 (%.1f ms)\n", open_ms);
    fprintf(stderr, "  类型: %s\n", backend.is_hiss() ? ".hiss (单帧)" : ".hcsd (球面)");
    fprintf(stderr, "  nside: %u\n", backend.get_nside());
    fprintf(stderr, "  n_pix: %llu\n", (unsigned long long)backend.get_n_pix());
    fprintf(stderr, "  filter: %s\n", backend.get_filter().c_str());

    json.key_bool("open_success", true);
    json.key_str("file_type", backend.is_hiss() ? "hiss" : "hcsd");
    json.key_int("nside", backend.get_nside());
    json.key_int("n_pix", (long long)backend.get_n_pix());
    json.key_str("filter", backend.get_filter());

    // 获取数据 bbox
    double center_ra, center_dec, width_deg, height_deg;
    if (backend.get_data_bbox(center_ra, center_dec, width_deg, height_deg) == 0) {
        fprintf(stderr, "  bbox: center=(%.4f, %.4f), size=%.4fx%.4f deg\n",
                center_ra, center_dec, width_deg, height_deg);
        json.key_num("center_ra", center_ra);
        json.key_num("center_dec", center_dec);
        json.key_num("width_deg", width_deg);
        json.key_num("height_deg", height_deg);
    }

    // ================================================================
    // 3. 性能 benchmark
    // ================================================================
    if (benchmark || sim_zoom || sim_pan) {
        fprintf(stderr, "\n========== 性能测试 ==========\n");

        size_t mem_before = get_working_set_mb();
        fprintf(stderr, "内存 (加载前): %zu MB\n", mem_before);
        json.key_int("mem_before_mb", (long long)mem_before);

        if (backend.is_hiss()) {
            // .hiss: 测试 get_all_data 性能
            TimePoint t2 = Clock::now();
            LeafData all = backend.get_all_data();
            TimePoint t3 = Clock::now();
            double get_all_ms = elapsed_ms(t2, t3);
            fprintf(stderr, "get_all_data: %.1f ms (n_pix=%llu)\n",
                    get_all_ms, (unsigned long long)all.n_pix);
            json.key_num("get_all_data_ms", get_all_ms);

            // 测试降采样性能
            if (all.n_pix > 0 && all.nside > 64) {
                TimePoint t4 = Clock::now();
                LeafData graded = backend.ud_grade(all, 64, 0.0f, 1.0f);
                TimePoint t5 = Clock::now();
                double udgrade_ms = elapsed_ms(t4, t5);
                fprintf(stderr, "ud_grade(->64): %.1f ms (n_pix=%llu -> %llu)\n",
                        udgrade_ms, (unsigned long long)all.n_pix,
                        (unsigned long long)graded.n_pix);
                json.key_num("ud_grade_ms", udgrade_ms);
                backend.release_leaf(graded);
            }
        } else {
            // .hcsd: 测试子叶加载性能
            ViewParams view;
            view.center_ra = center_ra;
            view.center_dec = center_dec;
            view.zoom = 1.0;
            view.fov_deg = 60.0;

            TimePoint t2 = Clock::now();
            auto leaves = backend.get_required_leaves(view);
            TimePoint t3 = Clock::now();
            double get_leaves_ms = elapsed_ms(t2, t3);
            fprintf(stderr, "get_required_leaves: %.1f ms (找到 %zu 个子叶)\n",
                    get_leaves_ms, leaves.size());
            json.key_num("get_required_leaves_ms", get_leaves_ms);
            json.key_int("n_required_leaves", (long long)leaves.size());

            // 加载前 10 个子叶
            int n_load = std::min((int)leaves.size(), 10);
            double total_load_ms = 0;
            for (int i = 0; i < n_load; i++) {
                TimePoint t4 = Clock::now();
                LeafData leaf = backend.load_leaf(leaves[i], 64);
                TimePoint t5 = Clock::now();
                total_load_ms += elapsed_ms(t4, t5);
                backend.release_leaf(leaf);
            }
            double avg_load_ms = n_load > 0 ? total_load_ms / n_load : 0;
            fprintf(stderr, "load_leaf (前%d个): 平均 %.1f ms/叶\n", n_load, avg_load_ms);
            json.key_num("avg_load_leaf_ms", avg_load_ms);
        }

        size_t mem_after = get_working_set_mb();
        fprintf(stderr, "内存 (加载后): %zu MB\n", mem_after);
        json.key_int("mem_after_mb", (long long)mem_after);
    }

    // ================================================================
    // 4. 模拟操作 (视角变化性能)
    // ================================================================
    if (sim_zoom || sim_pan) {
        fprintf(stderr, "\n========== 模拟操作 ==========\n");
        const char* sim_type = sim_zoom ? "zoom" : "pan";
        fprintf(stderr, "模拟类型: %s, 帧数: %d\n", sim_type, sim_frames);

        ViewParams view;
        view.center_ra = center_ra;
        view.center_dec = center_dec;
        view.zoom = 1.0;
        view.fov_deg = 60.0;

        double total_time_s = 0;
        double min_frame_ms = 1e9;
        double max_frame_ms = 0;
        int frame_count = 0;

        for (int i = 0; i < sim_frames; i++) {
            // 模拟视角变化
            if (sim_zoom) {
                // 缩放: 从 1.0 到 10.0 再回到 1.0
                double phase = (double)i / sim_frames * 2.0 * M_PI;
                view.zoom = 1.0 + 4.5 * (1.0 + sin(phase));
                view.fov_deg = 60.0 / view.zoom;
            } else if (sim_pan) {
                // 平移: 中心赤经/纬偏移
                double phase = (double)i / sim_frames * 2.0 * M_PI;
                view.center_ra = center_ra + 5.0 * sin(phase);
                view.center_dec = center_dec + 3.0 * cos(phase);
            }

            TimePoint t0 = Clock::now();

            // 执行数据层操作 (模拟渲染时的数据加载)
            if (backend.is_hcsd()) {
                auto leaves = backend.get_required_leaves(view);
                for (int j = 0; j < std::min((int)leaves.size(), 5); j++) {
                    LeafData leaf = backend.load_leaf(leaves[j], 64);
                    backend.release_leaf(leaf);
                }
            } else {
                // .hiss: 重新计算 bbox 视角相关
                auto leaves = backend.get_required_leaves(view);
            }

            TimePoint t1 = Clock::now();
            double frame_ms = elapsed_ms(t0, t1);
            total_time_s += frame_ms / 1000.0;
            if (frame_ms < min_frame_ms) min_frame_ms = frame_ms;
            if (frame_ms > max_frame_ms) max_frame_ms = frame_ms;
            frame_count++;

            if (i % 20 == 0 || i == sim_frames - 1) {
                fprintf(stderr, "  帧 %d/%d: %.1f ms (zoom=%.2f)\n",
                        i + 1, sim_frames, frame_ms, view.zoom);
            }
        }

        double avg_frame_ms = total_time_s / frame_count * 1000;
        double fps = frame_count / total_time_s;

        fprintf(stderr, "\n模拟结果:\n");
        fprintf(stderr, "  总时间: %.2f s\n", total_time_s);
        fprintf(stderr, "  平均帧时间: %.1f ms\n", avg_frame_ms);
        fprintf(stderr, "  最小帧时间: %.1f ms\n", min_frame_ms);
        fprintf(stderr, "  最大帧时间: %.1f ms\n", max_frame_ms);
        fprintf(stderr, "  平均 FPS: %.1f\n", fps);

        json.key_str("sim_type", sim_type);
        json.key_int("sim_frames", frame_count);
        json.key_num("sim_total_s", total_time_s);
        json.key_num("sim_avg_frame_ms", avg_frame_ms);
        json.key_num("sim_min_frame_ms", min_frame_ms);
        json.key_num("sim_max_frame_ms", max_frame_ms);
        json.key_num("sim_fps", fps);
    }

    // ================================================================
    // 5. 最终报告
    // ================================================================
    size_t mem_final = get_working_set_mb();
    json.key_int("mem_final_mb", (long long)mem_final);

    json.end();

    fprintf(stderr, "\n========== JSON 报告 ==========\n");
    printf("%s", json.buf.c_str());

    return 0;
}
