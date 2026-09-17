// ============================================================================
// orchestrator_saturation_wiring_gate.cpp
//   ORCH-001 批次 3 —— 「编排层的科学配置解析必须真的赋给运行时」可执行级门
// ----------------------------------------------------------------------------
// 背景：SAT-001（claim SC-008）把饱和电平解析接进了 orchestrator.cpp 的
// run_stage_snr，但当时的 W3 门是**源级正则断言**（在源码里找
// 「ncfg.saturation_level = ...」这一行）。源级断言无法区分「真接线」与
// 「写了不生效」；且编排层当时不在任何构建图（grep orchestrator CMakeLists = 0），
// 该赋值从未被编译过。本门把它升级为**可执行级**：
//
//   跑起来的最小链路（全部真实组件，零替身除下列明者）：
//     header KV (SATURATE/DATAMAX)
//       → Orchestrator::run_stage_snr（真实生产代码）
//       → DllLoader（真实加载器；Linux dlopen 分支）
//       → SnrNoiseModelConfig（真实 C ABI 结构）
//       → snr_noise_model_v1（生产源 noise_model.cpp，零改动）
//       → frame 的 variance/ivar 块 + photo_stats KV
//
//   唯一替身：snr_extract_model_v3 / snr_free_model_v3（稀疏控制点提取，
//   不属于本门判据；见 gate_module_snr_extract_spy.cpp 的文件头说明）。
//
// 判据（任一 FAIL ⇒ 进程退出码非 0）：
//   G1 提供 SATURATE ⇒ NOISE_SATURATION_LEVEL=1000 / FILTER=ENABLED /
//      SOURCE=HEADER_SATURATE（解析结果进入 ncfg）
//   G2 无 SATURATE/DATAMAX ⇒ 0 / DISABLED_NO_METADATA / NONE（显式降级，非静默）
//   G3 仅 DATAMAX ⇒ 电平取自 DATAMAX（优先级回落）
//   G4 非法 SATURATE 串 ⇒ 视为未提供（不编造数值）
//   G5 四臂 NOISE_MODEL_STATUS=OK（噪声模型真的被调用，不是 SKIPPED_*）
//   G6 **运行时可观测**：同一合成饱和帧，G1 臂的平台区逐像素 variance 必须
//      显著低于 G2 臂（饱和像素真的被排除出 blank-sky 统计）——
//      这是「配置真的赋给了运行时」的行为证据，源级断言拿不到。
//
// 负例注入（必红）：删掉 orchestrator.cpp 的 ncfg.saturation_level =
// resolve_effective_saturation(...) 赋值 ⇒ G1/G6 立刻红（level 退化为 0、
// 平台区 variance 与未过滤臂重合）。
// ============================================================================

#include "orchestrator.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// ============================================================================
// 测试友元（orchestrator.h 的 friend struct OrchestratorGatePeer）：
// 不新增任何生产公开 API —— 仅把私有 stage 处理器与最小链路状态暴露给本门，
// 使「真实 run_stage_snr」可被驱动。生产路径（init_dlls/run_stage1）不经此。
// ============================================================================
struct OrchestratorGatePeer {
    static bool load_minimal(Orchestrator& orch, const std::string& base,
                             std::string& err) {
        const bool aio = orch.dll_loader_.load_module(ModuleId::AIO, base);
        const bool snr = orch.dll_loader_.load_module(ModuleId::SNR, base);
        orch.dlls_loaded_ = aio && snr;
        if (!orch.dlls_loaded_) {
            err = "AIO: " + orch.dll_loader_.get_error(ModuleId::AIO) +
                  " | SNR: " + orch.dll_loader_.get_error(ModuleId::SNR);
        }
        return orch.dlls_loaded_;
    }
    static void set_frame(Orchestrator& orch, PipelineFrame* frame) {
        orch.frame_ = frame;
    }
    static bool run_snr(Orchestrator& orch, TaskResult& result) {
        return orch.run_stage_snr(result);
    }
};

// ============================================================================
// 合成帧（确定性；无 RNG、无外部数据）
//   H=W=192；平台区 = 中央 [64,128)×[64,128)，值 20000 ADU + 1% 平场残差
//   （SAT-001 EXP-S1 §1.2 的 A5 组同款：饱和平台被平场残差抹平后仍 ≥ 电平）；
//   其余天空 = 1000 ADU ± 5 ADU。SATURATE=1000 ⇒ 平台区整块落在饱和域。
//   星点掩膜输入留空（psf 行 cx/cy = NaN）⇒ 本门隔离「饱和过滤」单一变量，
//   不与 MASK-002 的逐星半径耦合。
// ============================================================================
static constexpr int kH = 192;
static constexpr int kW = 192;
static constexpr int kPlatLo = 64;
static constexpr int kPlatHi = 128;
static constexpr double kSkyLevel = 1000.0;
static constexpr double kSkySigma = 5.0;
static constexpr double kPlatLevel = 20000.0;
static constexpr double kPlatSigma = 200.0;  // 1% 平场响应残差
static constexpr double kSaturate = 1000.0;

static double det_noise(int x, int y) {
    const double u = std::sin((double)(x * 12.9898 + y * 78.233)) * 43758.5453;
    return 2.0 * (u - std::floor(u)) - 1.0;  // [-1,1)
}

static bool in_plateau(int x, int y) {
    return x >= kPlatLo && x < kPlatHi && y >= kPlatLo && y < kPlatHi;
}

static void fill_synthetic(std::vector<float>& data) {
    data.resize((size_t)kH * kW);
    for (int y = 0; y < kH; ++y) {
        for (int x = 0; x < kW; ++x) {
            const double v = in_plateau(x, y)
                ? kPlatLevel + kPlatSigma * det_noise(x, y)
                : kSkyLevel + kSkySigma * det_noise(x, y);
            data[(size_t)y * kW + x] = (float)v;
        }
    }
}

// ============================================================================
// 门状态
// ============================================================================
static int g_checks = 0;
static int g_failed = 0;
static void check(bool ok, const std::string& what) {
    ++g_checks;
    if (!ok) ++g_failed;
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what.c_str());
}

// AIO 模块入口（经真实 DllLoader 解析，不用静态链接副本）
using FrameCreateFn = PipelineFrame* (*)();
using FrameDestroyFn = void (*)(PipelineFrame*);
using AddBlockFn = int (*)(PipelineFrame*, const char*, AioBlockType,
                           const void*, int64_t, const int*, int, const char*);
using GetBlockFn = const AioBlock* (*)(const PipelineFrame*, const char*);
using KvSetFn = int (*)(PipelineFrame*, const char*, const char*, const char*);
using KvGetFn = const char* (*)(const PipelineFrame*, const char*, const char*);

struct AioApi {
    FrameCreateFn create = nullptr;
    FrameDestroyFn destroy = nullptr;
    AddBlockFn add_block = nullptr;
    GetBlockFn get_block = nullptr;
    KvSetFn kv_set = nullptr;
    KvGetFn kv_get = nullptr;
};

struct ArmResult {
    bool stage_ok = false;
    std::string level, filter, source, model_status;
    double var_plateau_mean = -1.0;  // 平台区逐像素 variance 均值
    double var_frame_mean = -1.0;    // 全帧均值
    double sigma_global = -1.0;      // photo_stats/NOISE_SIGMA_GLOBAL
    bool variance_block_ok = false;
};

// 跑一臂：saturate_kw / datamax_kw 为 nullptr 表示不写该 header 关键字
static ArmResult run_arm(const std::string& base, const AioApi& aio,
                         const char* saturate_kw, const char* datamax_kw,
                         const char* saturate_override = nullptr) {
    ArmResult r;
    PipelineFrame* frame = aio.create();
    std::vector<float> data;
    fill_synthetic(data);

    // data 块（FLOAT32 [H,W]）
    int dims2[2] = {kH, kW};
    aio.add_block(frame, "data", AIO_BLOCK_FLOAT32, data.data(),
                  (int64_t)data.size(), dims2, 2, "gate synthetic data");

    // psf 块（FLOAT64 [1,9]）：cx/cy=NaN ⇒ 不进星掩膜（隔离饱和单一变量）
    std::vector<double> psf(9, 0.0);
    psf[0] = 0.0;                       // status = DPSF_FIT_OK
    psf[1] = 100.0;                     // B
    psf[2] = 5000.0;                    // flux
    psf[3] = std::nan("");              // cx（NaN ⇒ 本行不进 star 掩膜数组）
    psf[4] = std::nan("");              // cy
    psf[5] = std::nan("");              // fwhm
    psf[6] = 9000.0;                    // A
    psf[7] = 0.5;                       // residual_scale（历史名 mad）
    int dims9[2] = {1, 9};
    aio.add_block(frame, "psf", AIO_BLOCK_FLOAT64, psf.data(), 9, dims9, 2,
                  "gate synthetic psf");

    // header 元数据
    aio.kv_set(frame, "header", "GAIN", "1.0");
    aio.kv_set(frame, "header", "READNOI", "5.0");
    if (saturate_kw && saturate_kw[0]) {
        aio.kv_set(frame, "header", "SATURATE",
                   saturate_override ? saturate_override : saturate_kw);
    }
    if (datamax_kw && datamax_kw[0]) {
        aio.kv_set(frame, "header", "DATAMAX", datamax_kw);
    }

    Orchestrator orch;
    std::string err;
    if (!OrchestratorGatePeer::load_minimal(orch, base, err)) {
        std::printf("  [FATAL] 最小链路模块加载失败: %s\n", err.c_str());
        aio.destroy(frame);
        return r;
    }
    OrchestratorGatePeer::set_frame(orch, frame);

    TaskResult tr;
    r.stage_ok = OrchestratorGatePeer::run_snr(orch, tr);

    auto kv = [&](const char* block, const char* key) -> std::string {
        const char* v = aio.kv_get(frame, block, key);
        return v ? std::string(v) : std::string();
    };
    r.level = kv("photo_stats", "NOISE_SATURATION_LEVEL");
    r.filter = kv("photo_stats", "NOISE_SATURATION_FILTER");
    r.source = kv("photo_stats", "NOISE_SATURATION_SOURCE");
    r.model_status = kv("photo_stats", "NOISE_MODEL_STATUS");
    if (!kv("photo_stats", "NOISE_SIGMA_GLOBAL").empty()) {
        r.sigma_global = std::atof(kv("photo_stats", "NOISE_SIGMA_GLOBAL").c_str());
    }

    const AioBlock* vb = aio.get_block(frame, "variance");
    if (vb != nullptr && vb->type == AIO_BLOCK_FLOAT32 && vb->data != nullptr &&
        vb->dims[0] == kH && vb->dims[1] == kW) {
        r.variance_block_ok = true;
        const float* p = static_cast<const float*>(vb->data);
        double sum_all = 0.0, sum_plat = 0.0;
        long n_plat = 0;
        for (int y = 0; y < kH; ++y) {
            for (int x = 0; x < kW; ++x) {
                const double v = (double)p[(size_t)y * kW + x];
                sum_all += v;
                if (in_plateau(x, y)) { sum_plat += v; ++n_plat; }
            }
        }
        r.var_frame_mean = sum_all / (double)(kH * kW);
        if (n_plat > 0) r.var_plateau_mean = sum_plat / (double)n_plat;
    }
    OrchestratorGatePeer::set_frame(orch, nullptr);
    aio.destroy(frame);
    return r;
}

int main(int argc, char** argv) {
    if (argc < 4) {
        std::fprintf(stderr,
                     "用法: %s <aio_module.so> <snr_module.so> <work_base_dir>\n",
                     argv[0]);
        return 2;
    }
    const fs::path aio_src = argv[1];
    const fs::path snr_src = argv[2];
    const fs::path base = fs::absolute(argv[3]);

    std::printf("=== orchestrator_saturation_wiring_gate (ORCH-001 批次 3) ===\n");
    std::printf("AIO 模块夹具: %s\nSNR 模块夹具: %s\n工作根: %s\n",
                aio_src.string().c_str(), snr_src.string().c_str(),
                base.string().c_str());

    // DllLoader 的默认路径（lib/infrastructure/aio/、lib/algorithms/noise_snr/cpp/）
    // 在最小链路工作根下复现，模块文件名与 Windows 侧同名（POSIX 为 .so）。
    std::error_code ec;
    fs::create_directories(base / "lib/infrastructure/aio", ec);
    fs::create_directories(base / "lib/algorithms/noise_snr/cpp", ec);
    fs::copy_file(aio_src, base / "lib/infrastructure/aio/astro_image_io.so",
                  fs::copy_options::overwrite_existing, ec);
    if (ec) { std::fprintf(stderr, "拷贝 AIO 模块失败: %s\n", ec.message().c_str()); return 2; }
    fs::copy_file(snr_src, base / "lib/algorithms/noise_snr/cpp/snr_estimator.so",
                  fs::copy_options::overwrite_existing, ec);
    if (ec) { std::fprintf(stderr, "拷贝 SNR 模块失败: %s\n", ec.message().c_str()); return 2; }

    // AIO 入口解析（真实加载器）
    DllLoader probe;
    AioApi aio;
    if (!probe.load_module(ModuleId::AIO, base.string())) {
        std::fprintf(stderr, "AIO 模块加载失败: %s\n",
                     probe.get_error(ModuleId::AIO).c_str());
        return 2;
    }
    aio.create = probe.get_function<FrameCreateFn>(ModuleId::AIO, "aio_pipeline_frame_create");
    aio.destroy = probe.get_function<FrameDestroyFn>(ModuleId::AIO, "aio_pipeline_frame_destroy");
    aio.add_block = probe.get_function<AddBlockFn>(ModuleId::AIO, "aio_frame_add_block");
    aio.get_block = probe.get_function<GetBlockFn>(ModuleId::AIO, "aio_frame_get_block");
    aio.kv_set = probe.get_function<KvSetFn>(ModuleId::AIO, "aio_frame_kv_set");
    aio.kv_get = probe.get_function<KvGetFn>(ModuleId::AIO, "aio_frame_kv_get");
    if (!aio.create || !aio.destroy || !aio.add_block || !aio.get_block ||
        !aio.kv_set || !aio.kv_get) {
        std::fprintf(stderr, "AIO 模块缺少帧容器入口 (create/add_block/get_block/kv_*)\n");
        return 2;
    }

    // ── 四臂 ────────────────────────────────────────────────────────────────
    std::printf("\n-- 臂 A: header SATURATE=1000（应生效）\n");
    const ArmResult a = run_arm(base.string(), aio, "1000", nullptr);
    std::printf("   stage_ok=%d level=%s filter=%s source=%s status=%s "
                "var_plateau=%.6g var_frame=%.6g sigma=%.6g\n",
                (int)a.stage_ok, a.level.c_str(), a.filter.c_str(), a.source.c_str(),
                a.model_status.c_str(), a.var_plateau_mean, a.var_frame_mean,
                a.sigma_global);

    std::printf("\n-- 臂 B: 无 SATURATE / DATAMAX（应显式降级）\n");
    const ArmResult b = run_arm(base.string(), aio, nullptr, nullptr);
    std::printf("   stage_ok=%d level=%s filter=%s source=%s status=%s "
                "var_plateau=%.6g var_frame=%.6g sigma=%.6g\n",
                (int)b.stage_ok, b.level.c_str(), b.filter.c_str(), b.source.c_str(),
                b.model_status.c_str(), b.var_plateau_mean, b.var_frame_mean,
                b.sigma_global);

    std::printf("\n-- 臂 C: 仅 header DATAMAX=2000（应回落取 DATAMAX）\n");
    const ArmResult c = run_arm(base.string(), aio, nullptr, "2000");
    std::printf("   stage_ok=%d level=%s filter=%s source=%s status=%s\n",
                (int)c.stage_ok, c.level.c_str(), c.filter.c_str(), c.source.c_str(),
                c.model_status.c_str());

    std::printf("\n-- 臂 D: header SATURATE=abc（非法 ⇒ 未提供，不编造）\n");
    const ArmResult d = run_arm(base.string(), aio, "abc", nullptr);
    std::printf("   stage_ok=%d level=%s filter=%s source=%s status=%s\n",
                (int)d.stage_ok, d.level.c_str(), d.filter.c_str(), d.source.c_str(),
                d.model_status.c_str());

    // ── 判据 ────────────────────────────────────────────────────────────────
    std::printf("\n-- 判据 --\n");
    check(a.stage_ok && b.stage_ok && c.stage_ok && d.stage_ok,
          "G0 四臂 run_stage_snr 均返回成功（真实 stage 处理器跑通）");
    check(a.level == "1000.000000",
          "G1a SATURATE=1000 ⇒ NOISE_SATURATION_LEVEL=1000.000000 (实测 " + a.level + ")");
    check(a.filter == "ENABLED", "G1b 过滤状态 ENABLED (实测 " + a.filter + ")");
    check(a.source == "HEADER_SATURATE", "G1c 电平来源 HEADER_SATURATE (实测 " + a.source + ")");
    check(b.level == "0.000000" && b.filter == "DISABLED_NO_METADATA" && b.source == "NONE",
          "G2 无元数据 ⇒ 0 / DISABLED_NO_METADATA / NONE (实测 " + b.level + " / " +
          b.filter + " / " + b.source + ")");
    check(c.level == "2000.000000" && c.source == "HEADER_DATAMAX",
          "G3 仅 DATAMAX ⇒ 2000.000000 / HEADER_DATAMAX (实测 " + c.level + " / " +
          c.source + ")");
    check(d.level == "0.000000" && d.filter == "DISABLED_NO_METADATA",
          "G4 非法 SATURATE 串 ⇒ 0 / DISABLED_NO_METADATA (实测 " + d.level +
          " / " + d.filter + ")");
    check(a.model_status == "OK" && b.model_status == "OK",
          "G5 噪声模型真的被调用 NOISE_MODEL_STATUS=OK (实测 A=" + a.model_status +
          " B=" + b.model_status + ")");
    check(a.variance_block_ok && b.variance_block_ok,
          "G6a variance 块已按 [H,W] 写出（逐像素权重场的唯一来源）");
    // G6b：同一饱和帧，过滤臂的平台区方差必须比未过滤臂低 1 个数量级以上。
    // 未过滤臂把 1% 平场残差（σ≈200 ADU）当天空方差 ⇒ σ²≈4e4；
    // 过滤臂把整块平台排除出 blank-sky 统计 ⇒ σ² 回到天空量级 (~25)。
    const bool g6b = a.var_plateau_mean > 0.0 && b.var_plateau_mean > 0.0 &&
                     a.var_plateau_mean * 50.0 < b.var_plateau_mean &&
                     a.var_plateau_mean < 1000.0;
    char buf[256];
    std::snprintf(buf, sizeof(buf),
                  "G6b 运行时可观测：平台区 variance 过滤臂 %.6g 远小于未过滤臂 %.6g "
                  "(<1/50 且过滤臂 <1000 ADU^2)", a.var_plateau_mean, b.var_plateau_mean);
    check(g6b, buf);
    const bool g6c = a.var_frame_mean > 0.0 && b.var_frame_mean > 0.0 &&
                     a.var_frame_mean * 10.0 < b.var_frame_mean;
    std::snprintf(buf, sizeof(buf),
                  "G6c 全帧均值 variance 过滤臂 %.6g 远小于未过滤臂 %.6g (<1/10)",
                  a.var_frame_mean, b.var_frame_mean);
    check(g6c, buf);

    std::printf("\n=== 门汇总: %d 项, 失败 %d 项 -> %s ===\n", g_checks, g_failed,
                g_failed == 0 ? "PASS" : "FAIL");
    return g_failed == 0 ? 0 : 1;
}
