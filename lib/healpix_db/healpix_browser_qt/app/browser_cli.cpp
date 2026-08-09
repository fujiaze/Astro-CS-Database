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
        const size_t x = (size_t)(z % kTileDim);
        const size_t y = (size_t)(z / kTileDim);
        const size_t idx = y * kTileDim + x;
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

    const bool pass = (mismatch == 0 && outside_ok >= 1 && n_snr > 0 && id_unique);
    json.key_bool("hips_pass", pass);
    fprintf(stderr, "RESULT: %s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}

int main(int argc, char* argv[]) {
    // 解析命令行
    std::string file_path;
    bool diag_only = false;
    bool benchmark = false;
    bool sim_zoom = false;
    bool sim_pan = false;
    bool hips_mode = false;
    int hips_queries = 1024;
    int sim_frames = 100;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--diag" || arg == "-d") {
            diag_only = true;
        } else if (arg == "--benchmark" || arg == "-b") {
            benchmark = true;
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
        int rc_h = run_hips_mode(file_path, json, hips_queries);
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
