// ============================================================================
// ipv_select.cpp - IPV StarSelector 模块 (Phase 0) (从 迁移, 复用 算法)
//
// 职责: 图像侧选星 + Gaia 侧不对称密度匹配查询
// 从 vm45_select.cpp 迁移, 仅做 namespace/前缀替换:
// - namespace v45 -> ipv
// - 常量前缀 VM45_ -> IPV_
// - 主入口 vm45_select -> ipv_select
// - 类型 VM45SolveParams -> IPVSolverParams (字段名一致, 已在 ipv_types.h 定义)
// - include vm45_internal.h -> ipv_select.h
// - 保留 output.s0 赋值 (StarSelection.s0 字段已在 ipv_types.h 中定义)
//
// 核心算法 (与 一致, 从 ss_core.cpp 迁移):
// - 图像读取 (astro_image_io.dll 动态加载)
// - 星点检测 (star_detector.dll, 句柄由外部注入)
// - 图像侧选星 (: 统一按 flux(A) 降序取前 img_n_target 颗)
// - FOV/密度计算 + 自适应步长迭代极限星等 ( ss_core.cpp)
// - Gaia 锥形查询 (gaia_client.dll, 句柄由外部注入)
// - Gnomonic 投影 + FOV 内过滤
// -: 保存 Gaia 原始 (ra, dec) 到 selection.gaia_ra/gaia_dec 供迭代重投影
//
// 接口: 内部 C++ 函数, 无 ctypes 边界, 无 JSON 序列化
// 句柄: 通过 get_gaia_client_handle / get_star_detector_handle 获取
// ============================================================================

#include "ipv_select.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <functional>
#include <omp.h>

#ifdef _WIN32
#include <windows.h>
#endif

namespace ipv {

// ============================================================================
// 物理常量
// ============================================================================

static constexpr double IPV_PI = 3.14159265358979323846;
// 206.265 = (180×3600)/π, 把 um/mm 直接转为 角秒/像素
static constexpr double IPV_ARCSEC_PER_UM_PER_MM = 206.265;
static constexpr double IPV_DEGTORAD = IPV_PI / 180.0;
static constexpr double IPV_RADTODEG = 180.0 / IPV_PI;
// 角秒/弧度 = 180×3600/π
static constexpr double IPV_ASEC_PER_RAD = 206264.80624709636;

// ============================================================================
// 内部 DLL 动态加载 (匿名命名空间, 仅本文件可见)
// ============================================================================

#ifdef _WIN32

// --- astro_image_io.dll 函数指针类型 ---
typedef struct AIOImageData AIOImageData;
typedef AIOImageData* (*aio_read_fn)(const char*);
typedef float* (*aio_get_pixel_data_fn)(const AIOImageData*);
typedef int (*aio_get_width_fn)(const AIOImageData*);
typedef int (*aio_get_height_fn)(const AIOImageData*);
typedef void (*aio_free_image_data_fn)(AIOImageData*);

// --- star_detector.dll 函数指针类型 ---
// StarDetectorHandle 是不透明指针
// +: star_detector API 已更新, 新增 mag 和 has_saturated 输出参数
// - mag: 每颗星的星等 (即使饱和星也有效)
// - has_saturated: 每颗星是否属于饱和星区域 (与 saturated 等价, 由 API 内部填充)
typedef int (*sdet_detect_ex_fn)(
    void* handle,
    const uint16_t* image, int width, int height,
    double** out_x, double** out_y, float** out_flux, int** out_saturated,
    float** out_mag, int** out_has_saturated, int* out_count,
    const char** extra_names, int extra_count, float*** out_extras);
// FP64 检测入口 (double 图像)
typedef int (*sdet_detect_ex_f64_fn)(
    void* handle, const double* image, int width, int height,
    double** out_x, double** out_y, float** out_flux, int** out_saturated,
    float** out_mag, int** out_has_saturated, int* out_count,
    const char** extra_names, int extra_count, float*** out_extras);
typedef void (*sdet_free_detect_ex_fn)(
    double* x, double* y, float* flux, int* saturated,
    float* mag, int* has_saturated,
    float** extras, int extra_count);

// --- gaia_client.dll 函数指针类型 ---
typedef int (*gaia_cone_search_for_solver_fn)(
    void* client,
    double ra, double dec, double radius_deg,
    double mag_high,
    double** out_ra, double** out_dec, float** out_mag,
    int* out_count);

// 缓存的 DLL 函数指针
struct DllApi {
    bool loaded = false;
    bool load_failed = false;

    HMODULE aio_dll = nullptr;
    aio_read_fn aio_read = nullptr;
    aio_get_pixel_data_fn aio_get_pixel_data = nullptr;
    aio_get_width_fn aio_get_width = nullptr;
    aio_get_height_fn aio_get_height = nullptr;
    aio_free_image_data_fn aio_free = nullptr;

    HMODULE sdet_dll = nullptr;
    sdet_detect_ex_fn sdet_detect_ex = nullptr;
    sdet_detect_ex_f64_fn sdet_detect_ex_f64 = nullptr;
    sdet_free_detect_ex_fn sdet_free_ex = nullptr;

    HMODULE gaia_dll = nullptr;
    gaia_cone_search_for_solver_fn gaia_cone_search = nullptr;
};

static DllApi g_dll;

// 加载所有依赖 DLL (首次调用时加载, 后续复用)
// 返回 true 表示全部加载成功
static bool load_dlls(Logger* logger) {
    if (g_dll.loaded) return true;
    if (g_dll.load_failed) return false;

    // astro_image_io.dll
    g_dll.aio_dll = LoadLibraryA("astro_image_io.dll");
    if (!g_dll.aio_dll) {
        if (logger) logger->error("ipv_select: 无法加载 astro_image_io.dll");
        g_dll.load_failed = true;
        return false;
    }
    g_dll.aio_read = reinterpret_cast<aio_read_fn>(reinterpret_cast<void*>(GetProcAddress(g_dll.aio_dll, "aio_read")));
    g_dll.aio_get_pixel_data = reinterpret_cast<aio_get_pixel_data_fn>(reinterpret_cast<void*>(GetProcAddress(g_dll.aio_dll, "aio_get_pixel_data")));
    g_dll.aio_get_width = reinterpret_cast<aio_get_width_fn>(reinterpret_cast<void*>(GetProcAddress(g_dll.aio_dll, "aio_get_width")));
    g_dll.aio_get_height = reinterpret_cast<aio_get_height_fn>(reinterpret_cast<void*>(GetProcAddress(g_dll.aio_dll, "aio_get_height")));
    g_dll.aio_free = reinterpret_cast<aio_free_image_data_fn>(reinterpret_cast<void*>(GetProcAddress(g_dll.aio_dll, "aio_free_image_data")));
    if (!g_dll.aio_read || !g_dll.aio_get_pixel_data ||
        !g_dll.aio_get_width || !g_dll.aio_get_height || !g_dll.aio_free) {
        if (logger) logger->error("ipv_select: astro_image_io 函数符号解析失败");
        g_dll.load_failed = true;
        return false;
    }

    // star_detector.dll
    g_dll.sdet_dll = LoadLibraryA("star_detector.dll");
    if (!g_dll.sdet_dll) {
        if (logger) logger->error("ipv_select: 无法加载 star_detector.dll");
        g_dll.load_failed = true;
        return false;
    }
    g_dll.sdet_detect_ex = reinterpret_cast<sdet_detect_ex_fn>(reinterpret_cast<void*>(GetProcAddress(g_dll.sdet_dll, "sdet_detect_ex")));
    g_dll.sdet_detect_ex_f64 = reinterpret_cast<sdet_detect_ex_f64_fn>(reinterpret_cast<void*>(GetProcAddress(g_dll.sdet_dll, "sdet_detect_ex_f64")));
    g_dll.sdet_free_ex = reinterpret_cast<sdet_free_detect_ex_fn>(reinterpret_cast<void*>(GetProcAddress(g_dll.sdet_dll, "sdet_free_detect_ex")));
    if (!g_dll.sdet_detect_ex || !g_dll.sdet_detect_ex_f64 || !g_dll.sdet_free_ex) {
        if (logger) logger->error("ipv_select: star_detector 函数符号解析失败");
        g_dll.load_failed = true;
        return false;
    }

    // gaia_client.dll
    g_dll.gaia_dll = LoadLibraryA("gaia_client.dll");
    if (!g_dll.gaia_dll) {
        if (logger) logger->error("ipv_select: 无法加载 gaia_client.dll");
        g_dll.load_failed = true;
        return false;
    }
    g_dll.gaia_cone_search = reinterpret_cast<gaia_cone_search_for_solver_fn>(reinterpret_cast<void*>(GetProcAddress(
        g_dll.gaia_dll, "gaia_client_cone_search_for_solver")));
    if (!g_dll.gaia_cone_search) {
        if (logger) logger->error("ipv_select: gaia_client_cone_search_for_solver 符号解析失败");
        g_dll.load_failed = true;
        return false;
    }

    g_dll.loaded = true;
    if (logger) logger->info("ipv_select: 依赖 DLL 全部加载成功");
    return true;
}

// gaia_query_count 已删除 (不再使用密度迭代)
// 保留 gaia_query_stars 用于一次性查询

// 通过 GaiaClient 句柄查询星表 (返回 ra/dec/mag 数组)
// 返回: 0=成功, -1=失败
static int gaia_query_stars(void* gaia_handle, double ra, double dec,
                             double radius_deg, double mag_lim,
                             std::vector<double>& out_ra,
                             std::vector<double>& out_dec,
                             std::vector<float>& out_mag) {
    out_ra.clear(); out_dec.clear(); out_mag.clear();
    if (!g_dll.gaia_cone_search || !gaia_handle) return -1;
    double *ra_ptr = nullptr, *dec_ptr = nullptr;
    float *mag_ptr = nullptr;
    int count = 0;
    int ret = g_dll.gaia_cone_search(gaia_handle, ra, dec, radius_deg, mag_lim,
                                      &ra_ptr, &dec_ptr, &mag_ptr, &count);
    if (ret != 0) return -1;
    if (count > 0 && ra_ptr && dec_ptr && mag_ptr) {
        out_ra.assign(ra_ptr, ra_ptr + count);
        out_dec.assign(dec_ptr, dec_ptr + count);
        out_mag.assign(mag_ptr, mag_ptr + count);
    }
    if (ra_ptr) free(ra_ptr);
    if (dec_ptr) free(dec_ptr);
    if (mag_ptr) free(mag_ptr);
    return 0;
}

#else
// 非 Windows 平台 stub
static bool load_dlls(Logger* logger) {
    if (logger) logger->error("ipv_select: 非 Windows 平台不支持 DLL 动态加载");
    return false;
}
static int gaia_query_count(void*, double, double, double, double) { return 0; }
static int gaia_query_stars(void*, double, double, double, double,
                             std::vector<double>&, std::vector<double>&,
                             std::vector<float>&) { return -1; }
#endif // _WIN32

// ============================================================================
// 内部辅助函数 (外部链接, 供单元测试调用)
// ============================================================================

// ----------------------------------------------------------------------------
// compute_fov_density - 计算 FOV 与密度 (从 ss_core.cpp 迁移)
// ----------------------------------------------------------------------------
void compute_fov_density(
    double focal_length_mm, double pixel_size_um,
    double img_width, double img_height,
    int n_img_bright,
    double gaia_density_ratio, double gaia_query_radius_factor,
    double& s0, double& fov_diag_deg,
    double& query_radius_deg, double& query_area_sqdeg,
    double& img_area_sqdeg, double& rho_img,
    double& rho_target, int& n_target,
    Logger* logger)
{
    // 初始化输出
    s0 = 0.0; fov_diag_deg = 0.0; query_radius_deg = 0.0;
    query_area_sqdeg = 0.0; img_area_sqdeg = 0.0;
    rho_img = 0.0; rho_target = 0.0; n_target = 0;

    if (focal_length_mm <= 0.0) {
        if (logger) logger->error("compute_fov_density: focal_length_mm 非法");
        return;
    }

    // 像素尺度 (角秒/像素)
    s0 = IPV_ARCSEC_PER_UM_PER_MM * pixel_size_um / focal_length_mm;

    // FOV 对角线 (度)
    double diag_pix = std::sqrt(img_width * img_width + img_height * img_height);
    double fov_diag_asec = diag_pix * s0;
    fov_diag_deg = fov_diag_asec / 3600.0;

    // 查询半径 (度) 与查询面积 (平方度)
    query_radius_deg = fov_diag_deg * gaia_query_radius_factor;
    query_area_sqdeg = IPV_PI * query_radius_deg * query_radius_deg;

    // 图像面积 (平方度)
    img_area_sqdeg = (img_width * s0 / 3600.0) * (img_height * s0 / 3600.0);
    if (img_area_sqdeg <= 0.0) img_area_sqdeg = query_area_sqdeg;

    // 图像面密度
    rho_img = (img_area_sqdeg > 0.0)
              ? static_cast<double>(n_img_bright) / img_area_sqdeg
              : 0.0;

    // Gaia 目标密度 = gaia_density_ratio × 图像密度
    rho_target = gaia_density_ratio * rho_img;

    // 目标星数 = gaia_density_ratio × n_img × (查询圆面积/图像面积), 下限 50
    // 密集星场兜底保护: 避免N_W爆炸导致kvector_build内存爆炸(34GB内存元凶)
    // 宽 FOV (>3°) 原上限 150 (星密导致六边形形状碰撞, polygon_match 区分度低)
    // 宽 FOV 上限 150→300 (Galaxy_Center 银心方向 Gaia 截断导致分布不匹配, max_vote=6)
    // 窄/中 FOV 上限 300
    // 回退: 宽 FOV 上限 300→150 (全量测试退化严重: wide FOV 84.7%→79.2%, 多失败 21 帧)
    // 300 在多数宽 FOV 场景下引入过多形状碰撞, 整体识别率下降, 故回退至 150
    // 宽窄 FOV 统一为 60 (与 img_n_target 一致, 减少形状碰撞)
    // 选星改用统一 mag/flux 排序后, 60 颗足够三角形匹配, 不需要更大候选池
    double img_area_safe = std::max(img_area_sqdeg, 1e-10);
    double n_target_dbl = gaia_density_ratio * static_cast<double>(n_img_bright)
                        * (query_area_sqdeg / img_area_safe);
    int n_target_cap = 60;
    n_target = std::min(n_target_cap, std::max(50, static_cast<int>(std::lround(n_target_dbl))));

    if (logger) {
        char buf[512];
        std::snprintf(buf, sizeof(buf),
            "FOV计算: s0=%.4f\"/px, FOV_diag=%.4f°, query_r=%.4f°, "
            "img_area=%.5f°², query_area=%.5f°², rho_img=%.2f, rho_target=%.2f, n_target=%d",
            s0, fov_diag_deg, query_radius_deg,
            img_area_sqdeg, query_area_sqdeg,
            rho_img, rho_target, n_target);
        logger->info(buf);
    }
}

// ----------------------------------------------------------------------------
// compute_initial_mag_cut - 计算初始极限星等 ( 公式)
// m_cut = 6 + 1.5×log10(f_mm) + 2×log10(t_s)
// ----------------------------------------------------------------------------
double compute_initial_mag_cut(
    double focal_length_mm, double exposure_time_s,
    Logger* logger)
{
    double f_safe = std::max(focal_length_mm, 1.0);
    double t_safe = std::max(exposure_time_s, 0.1);
    double m_cut = 6.0
                 + 1.5 * std::log10(f_safe)
                 + 2.0 * std::log10(t_safe);

    if (logger) {
        char buf[256];
        std::snprintf(buf, sizeof(buf),
            "初始星等: m_cut=%.4f (f=%.2fmm, t=%.2fs)",
            m_cut, focal_length_mm, exposure_time_s);
        logger->info(buf);
    }
    return m_cut;
}

// ----------------------------------------------------------------------------
// estimate_mag_lim_by_density - 基于天球平均星点密度直接估算极限星等
// 用户指导: "根据天球的平均星点密度, 搞一个根据视场角自动估算需要的极限星等的公式,
// 保证查出来的星比需要的多就行"
// 模型 (Gaia DR3 G 波段近似, 由公开星表密度数据拟合):
// ρ(G) = 5 × 10^(1.3×(G-10)) 颗/平方度
// (G=10 → 5, G=12 → 30, G=14 → 150, G=16 → 800, G=18 → 4000)
// 反解: G = 10 + log10(ρ/5) / 1.3
// 安全余量: +0.5 mag 保证查出的星数 >= n_required
// clip 到 [6, 18]: 过亮查询不到星, 过暗密度模型偏差大
// ----------------------------------------------------------------------------
double estimate_mag_lim_by_density(
    int n_required,
    double area_sqdeg,
    Logger* logger)
{
    if (n_required <= 0 || area_sqdeg <= 0.0) {
        if (logger) logger->warn("estimate_mag_lim_by_density: 参数非法, 返回默认 m=14");
        return 14.0;
    }

    // 目标密度 (颗/平方度)
    double rho_target = static_cast<double>(n_required) / area_sqdeg;

    // 反解极限星等
    // log10(rho/5) = 1.3 × (G - 10)
    // G = 10 + log10(rho/5) / 1.3
    double G = 10.0 + std::log10(rho_target / 5.0) / 1.3;

    // 安全余量: +0.5 mag (放宽星等, 保证查出的星数 > n_required)
    // 理论上 +0.5 mag 对应约 2× 密度 (10^0.65 ≈ 4.5×), 足以覆盖密度起伏
    G += 0.5;

    // clip 到 [6, 18]
    if (G < 6.0) G = 6.0;
    if (G > 18.0) G = 18.0;

    if (logger) {
        char buf[256];
        std::snprintf(buf, sizeof(buf),
            "密度估算极限星等: G=%.3f (n_required=%d, area=%.4f°², rho_target=%.2f/°²)",
            G, n_required, area_sqdeg, rho_target);
        logger->info(buf);
    }
    return G;
}

// ----------------------------------------------------------------------------
// density_match_iterate - 自适应步长迭代极限星等 (从 ss_core.cpp 迁移)
// 默认不再使用, 保留作为兜底 (estimate_mag_lim_by_density 失败时)
// 前4次 step_init, 后续 step_init/2
// ----------------------------------------------------------------------------
void density_match_iterate(
    std::function<int(double, double, double, double)> query_func,
    double center_ra, double center_dec, double query_radius_deg,
    int n_target, double m_cut_initial,
    double step_init, int max_iter, double tolerance,
    double& final_mag_lim, int& final_n_gaia,
    int& iterations, bool& converged,
    Logger* logger)
{
    final_mag_lim = m_cut_initial;
    final_n_gaia = 0;
    iterations = 0;
    converged = false;

    if (!query_func) {
        if (logger) logger->error("density_match_iterate: query_func 为空");
        return;
    }
    if (n_target <= 0) {
        if (logger) logger->error("density_match_iterate: n_target 非正");
        return;
    }

    // 容差上下界
    double n_lo = n_target * (1.0 - tolerance);
    double n_hi = n_target * (1.0 + tolerance);

    double m = m_cut_initial;
    int n = 0;
    int i = 0;

    if (logger) {
        char buf[512];
        std::snprintf(buf, sizeof(buf),
            "迭代开始: ra=%.4f, dec=%.4f, r=%.4f°, N_target=%d, "
            "tol=%.2f, 范围=[%.1f, %.1f], m0=%.3f, step_init=%.3f, max_iter=%d",
            center_ra, center_dec, query_radius_deg, n_target,
            tolerance, n_lo, n_hi, m_cut_initial, step_init, max_iter);
        logger->info(buf);
    }

    for (i = 0; i < max_iter; ++i) {
        n = query_func(center_ra, center_dec, query_radius_deg, m);

        // 自适应步长: 前4次 step_init, 后续 step_init/2
        double step = (i < 4) ? step_init : step_init * 0.5;

        if (logger) {
            char buf[512];
            std::snprintf(buf, sizeof(buf),
                "iter=%d  m_lim=%.3f  n_gaia=%d  (target=%d, 范围=[%.1f,%.1f], step=%.3f)",
                i, m, n, n_target, n_lo, n_hi, step);
            logger->info(buf);
        }

        if (n < n_lo) {
            // 星数不足 → 放宽星等
            m += step;
        } else if (n > n_hi) {
            // 星数过多 → 收紧星等
            m -= step;
        } else {
            converged = true;
            break;
        }
    }

    final_mag_lim = m;
    final_n_gaia = n;
    iterations = i;
    converged = converged;

    if (logger) {
        char buf[512];
        std::snprintf(buf, sizeof(buf),
            "迭代结束: %s  m_final=%.3f  n_final=%d  iters=%d",
            converged ? "收敛" : "未收敛(达到max_iter)",
            final_mag_lim, final_n_gaia, iterations);
        logger->info(buf);
    }
}

// ----------------------------------------------------------------------------
// select_image_stars - 图像侧选星: 按 mag(box积分) 升序排序
// 旧策略: 饱和星数 > img_n_target → 全选饱和星;
// 否则 → 饱和全选 + 非饱和按 flux 降序补足到 img_n_target
// 策略: 统一按 flux(A) 降序排序 (等价于按 mag 升序), 取前 img_n_target 颗
// 策略: star_detector mag 已改为 box 积分。
// 对饱和星, A(Moffat 振幅) 与 box 积分排序差异巨大, 按 flux(A)
// 排序会不一致。改用 mag(box积分) 升序排序, mag 越小越亮。
// 跳过 mag 为 NaN 的失效星 (NaN 排到最后, 不会被选中)。
// 注: flux 参数保留以备后续使用, 当前未使用 (排序基于 mag)
// ----------------------------------------------------------------------------
std::vector<int> select_image_stars(
    const std::vector<double>& flux,
    const std::vector<double>& mag,
    const std::vector<bool>& saturated,
    int img_n_target,
    Logger* logger)
{
    std::vector<int> sel_idx;
    int n_total = static_cast<int>(flux.size());
    if (n_total == 0 || img_n_target <= 0) return sel_idx;

    // 统一索引 (不分饱和/非饱和)
    std::vector<int> all_idx(n_total);
    for (int i = 0; i < n_total; ++i) all_idx[i] = i;

    // 按 mag(box积分) 升序排序 (mag 越小越亮)
    // 跳过 mag 为 NaN 的失效星 (NaN 排到最后)
    std::sort(all_idx.begin(), all_idx.end(),
              [&](int a, int b) {
                  bool a_nan = std::isnan(mag[a]);
                  bool b_nan = std::isnan(mag[b]);
                  if (a_nan && b_nan) return false;
                  if (a_nan) return false;  // NaN 排到最后
                  if (b_nan) return true;
                  return mag[a] < mag[b];
              });

    // 取前 img_n_target 颗
    int n_sel = std::min(img_n_target, n_total);
    sel_idx.reserve(n_sel);
    for (int i = 0; i < n_sel; ++i) sel_idx.push_back(all_idx[i]);

    // 统计饱和星数 (日志用)
    int n_sat_sel = 0;
    for (int i = 0; i < n_sel; ++i) {
        if (saturated[all_idx[i]]) n_sat_sel++;
    }

    if (logger) {
        char buf[256];
        std::snprintf(buf, sizeof(buf),
            "选星: 按 mag(box积分) 升序取前 %d 颗 (含饱和 %d 颗)",
            n_sel, n_sat_sel);
        logger->info(buf);
    }
    return sel_idx;
}

// ----------------------------------------------------------------------------
// gnomonic_forward_proj - Gnomonic 正向投影 (从 Python 迁移)
// 将天球坐标 (ra, dec) 投影到以 (ra0, dec0) 为中心的切平面
// 输出: xi, eta (角秒), valid (cosc > 1e-10 时有效)
// ----------------------------------------------------------------------------
void gnomonic_forward_proj(
    double ra_deg, double dec_deg,
    double ra0_deg, double dec0_deg,
    double& xi_asec, double& eta_asec, bool& valid)
{
    double ra = ra_deg * IPV_DEGTORAD;
    double dec = dec_deg * IPV_DEGTORAD;
    double ra0 = ra0_deg * IPV_DEGTORAD;
    double dec0 = dec0_deg * IPV_DEGTORAD;

    double sin_dec0 = std::sin(dec0), cos_dec0 = std::cos(dec0);
    double delta_ra = ra - ra0;
    double sin_dec = std::sin(dec), cos_dec = std::cos(dec);
    double cos_delta_ra = std::cos(delta_ra);

    // cosc = sin(dec0)×sin(dec) + cos(dec0)×cos(dec)×cos(delta_ra)
    double cosc = sin_dec0 * sin_dec + cos_dec0 * cos_dec * cos_delta_ra;
    valid = (cosc > 1e-10);
    double cosc_safe = valid ? cosc : 1.0;

    // xi = cos(dec)×sin(delta_ra) / cosc (弧度)
    // eta = (cos(dec0)×sin(dec) - sin(dec0)×cos(dec)×cos(delta_ra)) / cosc (弧度)
    double xi_rad = cos_dec * std::sin(delta_ra) / cosc_safe;
    double eta_rad = (cos_dec0 * sin_dec - sin_dec0 * cos_dec * cos_delta_ra) / cosc_safe;

    // 转换为角秒
    xi_asec = valid ? xi_rad * IPV_ASEC_PER_RAD : 0.0;
    eta_asec = valid ? eta_rad * IPV_ASEC_PER_RAD : 0.0;
}

// ============================================================================
// ipv_select 主实现
// ============================================================================

int ipv_select(
    const std::string& image_path,
    double ra, double dec,
    double focal_length_mm,
    double pixel_size_um,
    const IPVSolverParams& params,
    StarSelection& output,
    Logger* logger)
{
    // 清零输出
    output = StarSelection{};

    // --- 参数校验 ---
    if (focal_length_mm <= 0.0) {
        if (logger) logger->error("ipv_select: focal_length_mm 非法");
        return -1;
    }
    if (pixel_size_um <= 0.0) {
        if (logger) logger->error("ipv_select: pixel_size_um 非法");
        return -1;
    }
    if (image_path.empty()) {
        if (logger) logger->error("ipv_select: image_path 为空");
        return -1;
    }

    if (logger) logger->info("=== ipv_select 启动 ===");

    // --- 获取注入的句柄 ---
    void* gaia_handle = get_gaia_client_handle();
    void* detector_handle = get_star_detector_handle();
    if (!gaia_handle) {
        if (logger) logger->error("ipv_select: GaiaClient 句柄未注入");
        return -1;
    }
    if (!detector_handle) {
        if (logger) logger->error("ipv_select: StarDetector 句柄未注入");
        return -1;
    }

    // --- 加载依赖 DLL ---
    if (!load_dlls(logger)) {
        if (logger) logger->error("ipv_select: 依赖 DLL 加载失败");
        return -1;
    }

#ifdef _WIN32
    // --- Step 1: 读取图像 ---
    if (logger) logger->info("Step 1: 读取图像 " + image_path);
    AIOImageData* img_data = g_dll.aio_read(image_path.c_str());
    if (!img_data) {
        if (logger) logger->error("ipv_select: 图像读取失败");
        return -1;
    }
    int img_w = g_dll.aio_get_width(img_data);
    int img_h = g_dll.aio_get_height(img_data);
    float* pixel_data = g_dll.aio_get_pixel_data(img_data);
    if (img_w <= 0 || img_h <= 0 || !pixel_data) {
        if (logger) logger->error("ipv_select: 图像数据非法");
        g_dll.aio_free(img_data);
        return -1;
    }
    if (logger) {
        char buf[256];
        std::snprintf(buf, sizeof(buf), "  图像尺寸: %d×%d", img_w, img_h);
        logger->info(buf);
    }

    // 转 uint16 (star_detector 需要 uint16_t*)
    std::vector<uint16_t> img_u16(static_cast<size_t>(img_w) * img_h);
    for (size_t i = 0; i < img_u16.size(); ++i) {
        float v = pixel_data[i];
        if (v < 0.0f) v = 0.0f;
        if (v > 65535.0f) v = 65535.0f;
        img_u16[i] = static_cast<uint16_t>(v);
    }
    g_dll.aio_free(img_data);

    // --- Step 2: 星点检测 ---
    if (logger) logger->info("Step 2: 星点检测");
    double *det_x = nullptr, *det_y = nullptr;
    float *det_flux = nullptr;
    int *det_sat = nullptr;
    // +: star_detector API 新增输出参数, 即使不使用也必须传入以匹配签名
    // mag(box积分) 用于选星排序, 替代旧 flux(A) 排序
    float *det_mag = nullptr;
    int *det_has_sat = nullptr;
    int det_count = 0;
    int det_ret = g_dll.sdet_detect_ex(
        detector_handle, img_u16.data(), img_w, img_h,
        &det_x, &det_y, &det_flux, &det_sat,
        &det_mag, &det_has_sat, &det_count,
        nullptr, 0, nullptr);
    if (det_ret != 0 || det_count <= 0) {
        if (logger) logger->error("ipv_select: 星点检测失败或未检测到星");
        if (det_x || det_y || det_flux || det_sat || det_mag || det_has_sat) {
            g_dll.sdet_free_ex(det_x, det_y, det_flux, det_sat,
                               det_mag, det_has_sat, nullptr, 0);
        }
        return -1;
    }
    if (logger) {
        int n_sat = 0;
        for (int i = 0; i < det_count; ++i) if (det_sat[i]) n_sat++;
        char buf[256];
        std::snprintf(buf, sizeof(buf), "  检测到星点: %d 颗 (饱和 %d, 正常 %d)",
                      det_count, n_sat, det_count - n_sat);
        logger->info(buf);
    }

    // --- Step 3: 图像侧选星 ( 按 mag 升序) ---
    if (logger) logger->info("Step 3: 图像侧选星");
    std::vector<double> flux_vec(det_flux, det_flux + det_count);
    std::vector<double> mag_vec(det_mag, det_mag + det_count);  // box 积分 mag
    std::vector<bool> sat_vec(det_count);
    for (int i = 0; i < det_count; ++i) sat_vec[i] = (det_sat[i] != 0);

    std::vector<int> sel_idx = select_image_stars(
        flux_vec, mag_vec, sat_vec, params.img_n_target, logger);
    int N = static_cast<int>(sel_idx.size());
    if (N < 2) {
        if (logger) logger->error("ipv_select: 图像侧选星数过少");
        g_dll.sdet_free_ex(det_x, det_y, det_flux, det_sat,
                           det_mag, det_has_sat, nullptr, 0);
        return -1;
    }

    // --- Step 4: 构建 U 向量组 ( 像素坐标, 原点在图像中心, Y 轴向上) ---
    // 用户指导: "角秒不可靠, 应该用像素"
    // 像素坐标直接用, 不乘 s0, 避免 FOCALLEN 标称误差影响匹配
    double s0 = IPV_ARCSEC_PER_UM_PER_MM * pixel_size_um / focal_length_mm;
    double cx = img_w / 2.0, cy = img_h / 2.0;
    output.U.resize(N);
    for (int i = 0; i < N; ++i) {
        int idx = sel_idx[i];
        output.U[i].x = (det_x[idx] - cx);              // 像素, 不乘 s0
        output.U[i].y = -(det_y[idx] - cy);             // Y 轴向上 (图像 Y 向下)
        output.U[i].flux = static_cast<double>(det_flux[idx]);
        output.U[i].saturated = (det_sat[idx] != 0);
    }
    output.img_width = img_w;
    output.img_height = img_h;

    // 保存全部检测星点供 robust_refine 使用 (在 sdet_free_ex 之前)
    // 坐标约定同 U: 像素坐标, 原点图像中心, Y 轴向上
    output.U_full.resize(det_count);
    output.mag_full.resize(det_count);
    for (int i = 0; i < det_count; ++i) {
        output.U_full[i].x = det_x[i] - cx;
        output.U_full[i].y = -(det_y[i] - cy);
        output.U_full[i].flux = static_cast<double>(det_flux[i]);
        output.U_full[i].saturated = (det_sat[i] != 0);
        output.mag_full[i] = static_cast<double>(det_mag[i]);
    }

    // 释放星点检测内存
    g_dll.sdet_free_ex(det_x, det_y, det_flux, det_sat,
                       det_mag, det_has_sat, nullptr, 0);

    if (logger) {
        char buf[256];
        std::snprintf(buf, sizeof(buf), "  U 向量组: %d×2 (s0=%.4f\"/px)", N, s0);
        logger->info(buf);
    }

    // --- Step 5: 计算 FOV 与密度 ---
    if (logger) logger->info("Step 4: FOV/密度计算");
    double fov_diag_deg, query_radius_deg, query_area_sqdeg, img_area_sqdeg;
    double rho_img, rho_target;
    int n_target;
    compute_fov_density(
        focal_length_mm, pixel_size_um,
        static_cast<double>(img_w), static_cast<double>(img_h),
        N, params.gaia_density_ratio, params.gaia_query_radius_factor,
        s0, fov_diag_deg, query_radius_deg, query_area_sqdeg,
        img_area_sqdeg, rho_img, rho_target, n_target, logger);

    output.fov_diag_deg = fov_diag_deg;
    output.rho_img = rho_img;
    output.rho_target = rho_target;
    output.s0 = s0;  // 供后续 Phase (相对向量法等) 使用

    // --- Step 6: 基于天球平均密度直接估算极限星等 ---
    // 用户指导: "根据天球的平均星点密度, 搞一个根据视场角自动估算需要的极限星等的公式,
    // 保证查出来的星比需要的多就行。初始只需要很小的极限星等"
    // 用 query_area 估算, +0.5 mag 安全余量, 一次查询即可, 不再迭代
    if (logger) logger->info("Step 5: V4.9 密度公式估算极限星等 (替代迭代)");
    double m_lim_final = estimate_mag_lim_by_density(n_target, query_area_sqdeg, logger);
    int m_lim_iters = 0;  // 不再迭代

    // --- Step 7: 用估算的极限星等查询 Gaia 星表 (一次查询) ---
    if (logger) {
        char buf[256];
        std::snprintf(buf, sizeof(buf), "Step 6: 用 m_lim=%.3f 查询 Gaia 星表", m_lim_final);
        logger->info(buf);
    }
    std::vector<double> cat_ra, cat_dec;
    std::vector<float> cat_mag;
    int q_ret = gaia_query_stars(gaia_handle, ra, dec, query_radius_deg, m_lim_final,
                                  cat_ra, cat_dec, cat_mag);

    // 补救: 若一次查询不足, 逐步放宽 (最多 2 次, 每次 +1.0 mag)
    int rescue_count = 0;
    while ((q_ret != 0 || (int)cat_ra.size() < n_target) && rescue_count < 2) {
        double m_rescue = m_lim_final + 1.0 * (rescue_count + 1);
        if (logger) {
            char buf[256];
            std::snprintf(buf, sizeof(buf),
                "V4.9 补救查询 %d: Gaia 返回 %d < n_target=%d, 放宽到 m=%.3f",
                rescue_count + 1, (int)cat_ra.size(), n_target, m_rescue);
            logger->warn(buf);
        }
        cat_ra.clear(); cat_dec.clear(); cat_mag.clear();
        q_ret = gaia_query_stars(gaia_handle, ra, dec, query_radius_deg, m_rescue,
                                  cat_ra, cat_dec, cat_mag);
        if (q_ret == 0 && (int)cat_ra.size() >= n_target) {
            m_lim_final = m_rescue;
            break;
        }
        rescue_count++;
    }

    // 最终兜底: mag=22
    if (q_ret != 0 || cat_ra.size() < 2) {
        if (logger) logger->warn("Gaia 返回过少, 启用 mag=22 兜底查询");
        cat_ra.clear(); cat_dec.clear(); cat_mag.clear();
        q_ret = gaia_query_stars(gaia_handle, ra, dec, query_radius_deg, 22.0,
                                  cat_ra, cat_dec, cat_mag);
    }
    if (q_ret != 0 || cat_ra.size() < 2) {
        if (logger) logger->error("ipv_select: Gaia 星表查询星数过少");
        return -1;
    }

    int n_gaia_final = (int)cat_ra.size();
    output.m_lim_final = m_lim_final;
    output.n_gaia_final = n_gaia_final;
    output.m_lim_iterations = m_lim_iters;

    // --- Step 9: Gnomonic 投影 + FOV 内过滤 ---
    if (logger) logger->info("Step 7: Gnomonic 投影 + FOV 过滤");
    double fov_half_w = img_w / 2.0 * s0;  // 角秒
    double fov_half_h = img_h / 2.0 * s0;

    // 并行预计算所有 Gaia 星的 Gnomonic 投影 (gnomonic_forward_proj 是纯函数,
    // 无副作用, 可安全并行)。结果缓存供后续 FOV 过滤和 W 向量组构建复用,
    // 避免重复调用 gnomonic_forward_proj (原实现调用 3 次: 1×FOV + 1.5×FOV + W 构建)。
    // 使用 char 而非 bool 存储 valid 标志, 避免 std::vector<bool> 位压缩导致的并行写竞争。
    const size_t n_cat = cat_ra.size();
    std::vector<double> proj_xi(n_cat), proj_eta(n_cat);
    std::vector<char> proj_valid(n_cat, 0);
    #pragma omp parallel for num_threads(16) schedule(static)
    for (ptrdiff_t i = 0; i < (ptrdiff_t)n_cat; ++i) {
        double xi = 0.0, eta = 0.0;
        bool valid = false;
        gnomonic_forward_proj(cat_ra[i], cat_dec[i], ra, dec, xi, eta, valid);
        proj_xi[i] = xi;
        proj_eta[i] = eta;
        proj_valid[i] = valid ? 1 : 0;
    }

    std::vector<int> fov_idx;
    for (size_t i = 0; i < n_cat; ++i) {
        if (!proj_valid[i]) continue;
        if (std::abs(proj_xi[i]) < fov_half_w && std::abs(proj_eta[i]) < fov_half_h) {
            fov_idx.push_back(static_cast<int>(i));
        }
    }
    if (fov_idx.size() < 2) {
        // 放宽到 1.5×FOV (复用已缓存的投影结果, 无需重新计算)
        if (logger) logger->warn("FOV 内星数过少, 放宽到 1.5×FOV");
        fov_idx.clear();
        for (size_t i = 0; i < n_cat; ++i) {
            if (!proj_valid[i]) continue;
            if (std::abs(proj_xi[i]) < fov_half_w * 1.5 && std::abs(proj_eta[i]) < fov_half_h * 1.5) {
                fov_idx.push_back(static_cast<int>(i));
            }
        }
    }
    if (fov_idx.size() < 2) {
        if (logger) logger->error("ipv_select: FOV 内 Gaia 星数过少");
        return -1;
    }

    // --- Step 10: 按星等升序 (最亮优先) 取前 n_target 颗 ---
    // 注: fov_idx 通常 <1000, 并行排序收益有限, 保持 std::sort
    std::sort(fov_idx.begin(), fov_idx.end(),
              [&](int a, int b) { return cat_mag[a] < cat_mag[b]; });
    int M = std::min(n_target, static_cast<int>(fov_idx.size()));

    output.W.resize(M);
    output.gaia_ra.resize(M);     // 保存原始 (ra,dec) 用于迭代重投影
    output.gaia_dec.resize(M);
    for (int i = 0; i < M; ++i) {
        int idx = fov_idx[i];
        // 复用 Step 9 缓存的投影结果, 避免重复调用 gnomonic_forward_proj
        // W 直接用角秒坐标 (TRANS: U(像素)->W(角秒))
        // U = 像素坐标, 原点图像中心, Y 轴向上
        // W = 角秒坐标, 原点投影中心 (gnomonic xi/eta)
        output.W[i].x = proj_xi[idx];    // 角秒
        output.W[i].y = proj_eta[idx];   // 角秒
        output.W[i].flux = 0.0;  // Gaia 星表无 flux, 用星等代理 (后续模块不依赖)
        output.W[i].saturated = false;
        // 保存 Gaia 原始 (ra, dec) (切平面投影前), 供 iterative_reproject 使用
        output.gaia_ra[i]  = cat_ra[idx];
        output.gaia_dec[i] = cat_dec[idx];
    }

    if (logger) {
        char buf[256];
        std::snprintf(buf, sizeof(buf),
            "  W 向量组: %d×2 (FOV 内 %d, 取最亮 %d, gaia_ra/dec 已保存)",
            M, static_cast<int>(fov_idx.size()), M);
        logger->info(buf);
    }
#else
    // 非 Windows 平台不支持
    if (logger) logger->error("ipv_select: 非 Windows 平台不支持");
    return -1;
#endif // _WIN32

    output.success = true;
    if (logger) logger->info("=== ipv_select 完成 ===");
    return 0;
}

// ============================================================================
// ipv_select_from_memory - 从内存像素数据选星 (不读文件)
//
// 与 ipv_select 算法完全一致, 区别:
// - 直接接受 float* pixels 参数, 跳过 aio_read 文件读取
// - 不调用 aio_free (像素数据由调用方管理)
// - 复用 uint16 转换 + sdet_detect_ex + 选星 + Gaia 查询 + gnomonic 投影
// ============================================================================

int ipv_select_from_memory(
    const float* pixels,
    int width, int height,
    double ra, double dec,
    double focal_length_mm,
    double pixel_size_um,
    const IPVSolverParams& params,
    StarSelection& output,
    Logger* logger)
{
    // 清零输出
    output = StarSelection{};

    // --- 参数校验 ---
    if (focal_length_mm <= 0.0) {
        if (logger) logger->error("ipv_select_from_memory: focal_length_mm 非法");
        return -1;
    }
    if (pixel_size_um <= 0.0) {
        if (logger) logger->error("ipv_select_from_memory: pixel_size_um 非法");
        return -1;
    }
    if (pixels == nullptr || width <= 0 || height <= 0) {
        if (logger) logger->error("ipv_select_from_memory: pixels/width/height 非法");
        return -1;
    }

    if (logger) logger->info("=== ipv_select_from_memory 启动 ===");

    // --- 获取注入的句柄 ---
    void* gaia_handle = get_gaia_client_handle();
    void* detector_handle = get_star_detector_handle();
    if (!gaia_handle) {
        if (logger) logger->error("ipv_select_from_memory: GaiaClient 句柄未注入");
        return -1;
    }
    if (!detector_handle) {
        if (logger) logger->error("ipv_select_from_memory: StarDetector 句柄未注入");
        return -1;
    }

    // --- 加载依赖 DLL ---
    if (!load_dlls(logger)) {
        if (logger) logger->error("ipv_select_from_memory: 依赖 DLL 加载失败");
        return -1;
    }

#ifdef _WIN32
    // --- Step 1: 使用传入的内存像素数据 (不读文件) ---
    int img_w = width;
    int img_h = height;
    const float* pixel_data = pixels;
    if (logger) {
        char buf[256];
        std::snprintf(buf, sizeof(buf), "Step 1: 使用内存像素数据 %d×%d", img_w, img_h);
        logger->info(buf);
    }

    // 转 uint16 (star_detector 需要 uint16_t*)
    std::vector<uint16_t> img_u16(static_cast<size_t>(img_w) * img_h);
    for (size_t i = 0; i < img_u16.size(); ++i) {
        float v = pixel_data[i];
        if (v < 0.0f) v = 0.0f;
        if (v > 65535.0f) v = 65535.0f;
        img_u16[i] = static_cast<uint16_t>(v);
    }
    // 注: 不调用 aio_free, pixel_data 由调用方管理

    // --- Step 2: 星点检测 ---
    if (logger) logger->info("Step 2: 星点检测");
    double *det_x = nullptr, *det_y = nullptr;
    float *det_flux = nullptr;
    int *det_sat = nullptr;
    float *det_mag = nullptr;
    int *det_has_sat = nullptr;
    int det_count = 0;
    int det_ret = g_dll.sdet_detect_ex(
        detector_handle, img_u16.data(), img_w, img_h,
        &det_x, &det_y, &det_flux, &det_sat,
        &det_mag, &det_has_sat, &det_count,
        nullptr, 0, nullptr);
    if (det_ret != 0 || det_count <= 0) {
        if (logger) logger->error("ipv_select_from_memory: 星点检测失败或未检测到星");
        if (det_x || det_y || det_flux || det_sat || det_mag || det_has_sat) {
            g_dll.sdet_free_ex(det_x, det_y, det_flux, det_sat,
                               det_mag, det_has_sat, nullptr, 0);
        }
        return -1;
    }
    if (logger) {
        int n_sat = 0;
        for (int i = 0; i < det_count; ++i) if (det_sat[i]) n_sat++;
        char buf[256];
        std::snprintf(buf, sizeof(buf), "  检测到星点: %d 颗 (饱和 %d, 正常 %d)",
                      det_count, n_sat, det_count - n_sat);
        logger->info(buf);
    }

    // --- Step 3: 图像侧选星 ( 按 mag 升序) ---
    if (logger) logger->info("Step 3: 图像侧选星");
    std::vector<double> flux_vec(det_flux, det_flux + det_count);
    std::vector<double> mag_vec(det_mag, det_mag + det_count);
    std::vector<bool> sat_vec(det_count);
    for (int i = 0; i < det_count; ++i) sat_vec[i] = (det_sat[i] != 0);

    std::vector<int> sel_idx = select_image_stars(
        flux_vec, mag_vec, sat_vec, params.img_n_target, logger);
    int N = static_cast<int>(sel_idx.size());
    if (N < 2) {
        if (logger) logger->error("ipv_select_from_memory: 图像侧选星数过少");
        g_dll.sdet_free_ex(det_x, det_y, det_flux, det_sat,
                           det_mag, det_has_sat, nullptr, 0);
        return -1;
    }

    // --- Step 4: 构建 U 向量组 ( 像素坐标, 原点在图像中心, Y 轴向上) ---
    double s0 = IPV_ARCSEC_PER_UM_PER_MM * pixel_size_um / focal_length_mm;
    double cx = img_w / 2.0, cy = img_h / 2.0;
    output.U.resize(N);
    for (int i = 0; i < N; ++i) {
        int idx = sel_idx[i];
        output.U[i].x = (det_x[idx] - cx);
        output.U[i].y = -(det_y[idx] - cy);
        output.U[i].flux = static_cast<double>(det_flux[idx]);
        output.U[i].saturated = (det_sat[idx] != 0);
    }
    output.img_width = img_w;
    output.img_height = img_h;

    // 保存全部检测星点供 robust_refine 使用
    output.U_full.resize(det_count);
    output.mag_full.resize(det_count);
    for (int i = 0; i < det_count; ++i) {
        output.U_full[i].x = det_x[i] - cx;
        output.U_full[i].y = -(det_y[i] - cy);
        output.U_full[i].flux = static_cast<double>(det_flux[i]);
        output.U_full[i].saturated = (det_sat[i] != 0);
        output.mag_full[i] = static_cast<double>(det_mag[i]);
    }

    // 释放星点检测内存
    g_dll.sdet_free_ex(det_x, det_y, det_flux, det_sat,
                       det_mag, det_has_sat, nullptr, 0);

    if (logger) {
        char buf[256];
        std::snprintf(buf, sizeof(buf), "  U 向量组: %d×2 (s0=%.4f\"/px)", N, s0);
        logger->info(buf);
    }

    // --- Step 5: 计算 FOV 与密度 ---
    if (logger) logger->info("Step 4: FOV/密度计算");
    double fov_diag_deg, query_radius_deg, query_area_sqdeg, img_area_sqdeg;
    double rho_img, rho_target;
    int n_target;
    compute_fov_density(
        focal_length_mm, pixel_size_um,
        static_cast<double>(img_w), static_cast<double>(img_h),
        N, params.gaia_density_ratio, params.gaia_query_radius_factor,
        s0, fov_diag_deg, query_radius_deg, query_area_sqdeg,
        img_area_sqdeg, rho_img, rho_target, n_target, logger);

    output.fov_diag_deg = fov_diag_deg;
    output.rho_img = rho_img;
    output.rho_target = rho_target;
    output.s0 = s0;

    // --- Step 6: 基于天球平均密度直接估算极限星等 ---
    if (logger) logger->info("Step 5: V4.9 密度公式估算极限星等 (替代迭代)");
    double m_lim_final = estimate_mag_lim_by_density(n_target, query_area_sqdeg, logger);
    int m_lim_iters = 0;

    // --- Step 7: 用估算的极限星等查询 Gaia 星表 (一次查询) ---
    if (logger) {
        char buf[256];
        std::snprintf(buf, sizeof(buf), "Step 6: 用 m_lim=%.3f 查询 Gaia 星表", m_lim_final);
        logger->info(buf);
    }
    std::vector<double> cat_ra, cat_dec;
    std::vector<float> cat_mag;
    int q_ret = gaia_query_stars(gaia_handle, ra, dec, query_radius_deg, m_lim_final,
                                  cat_ra, cat_dec, cat_mag);

    // 补救: 若一次查询不足, 逐步放宽 (最多 2 次, 每次 +1.0 mag)
    int rescue_count = 0;
    while ((q_ret != 0 || (int)cat_ra.size() < n_target) && rescue_count < 2) {
        double m_rescue = m_lim_final + 1.0 * (rescue_count + 1);
        if (logger) {
            char buf[256];
            std::snprintf(buf, sizeof(buf),
                "V4.9 补救查询 %d: Gaia 返回 %d < n_target=%d, 放宽到 m=%.3f",
                rescue_count + 1, (int)cat_ra.size(), n_target, m_rescue);
            logger->warn(buf);
        }
        cat_ra.clear(); cat_dec.clear(); cat_mag.clear();
        q_ret = gaia_query_stars(gaia_handle, ra, dec, query_radius_deg, m_rescue,
                                  cat_ra, cat_dec, cat_mag);
        if (q_ret == 0 && (int)cat_ra.size() >= n_target) {
            m_lim_final = m_rescue;
            break;
        }
        rescue_count++;
    }

    // 最终兜底: mag=22
    if (q_ret != 0 || cat_ra.size() < 2) {
        if (logger) logger->warn("Gaia 返回过少, 启用 mag=22 兜底查询");
        cat_ra.clear(); cat_dec.clear(); cat_mag.clear();
        q_ret = gaia_query_stars(gaia_handle, ra, dec, query_radius_deg, 22.0,
                                  cat_ra, cat_dec, cat_mag);
    }
    if (q_ret != 0 || cat_ra.size() < 2) {
        if (logger) logger->error("ipv_select_from_memory: Gaia 星表查询星数过少");
        return -1;
    }

    int n_gaia_final = (int)cat_ra.size();
    output.m_lim_final = m_lim_final;
    output.n_gaia_final = n_gaia_final;
    output.m_lim_iterations = m_lim_iters;

    // --- Step 9: Gnomonic 投影 + FOV 内过滤 ---
    if (logger) logger->info("Step 7: Gnomonic 投影 + FOV 过滤");
    double fov_half_w = img_w / 2.0 * s0;
    double fov_half_h = img_h / 2.0 * s0;

    const size_t n_cat = cat_ra.size();
    std::vector<double> proj_xi(n_cat), proj_eta(n_cat);
    std::vector<char> proj_valid(n_cat, 0);
    #pragma omp parallel for num_threads(16) schedule(static)
    for (ptrdiff_t i = 0; i < (ptrdiff_t)n_cat; ++i) {
        double xi = 0.0, eta = 0.0;
        bool valid = false;
        gnomonic_forward_proj(cat_ra[i], cat_dec[i], ra, dec, xi, eta, valid);
        proj_xi[i] = xi;
        proj_eta[i] = eta;
        proj_valid[i] = valid ? 1 : 0;
    }

    std::vector<int> fov_idx;
    for (size_t i = 0; i < n_cat; ++i) {
        if (!proj_valid[i]) continue;
        if (std::abs(proj_xi[i]) < fov_half_w && std::abs(proj_eta[i]) < fov_half_h) {
            fov_idx.push_back(static_cast<int>(i));
        }
    }
    if (fov_idx.size() < 2) {
        if (logger) logger->warn("FOV 内星数过少, 放宽到 1.5×FOV");
        fov_idx.clear();
        for (size_t i = 0; i < n_cat; ++i) {
            if (!proj_valid[i]) continue;
            if (std::abs(proj_xi[i]) < fov_half_w * 1.5 && std::abs(proj_eta[i]) < fov_half_h * 1.5) {
                fov_idx.push_back(static_cast<int>(i));
            }
        }
    }
    if (fov_idx.size() < 2) {
        if (logger) logger->error("ipv_select_from_memory: FOV 内 Gaia 星数过少");
        return -1;
    }

    // --- Step 10: 按星等升序 (最亮优先) 取前 n_target 颗 ---
    std::sort(fov_idx.begin(), fov_idx.end(),
              [&](int a, int b) { return cat_mag[a] < cat_mag[b]; });
    int M = std::min(n_target, static_cast<int>(fov_idx.size()));

    output.W.resize(M);
    output.gaia_ra.resize(M);
    output.gaia_dec.resize(M);
    for (int i = 0; i < M; ++i) {
        int idx = fov_idx[i];
        output.W[i].x = proj_xi[idx];
        output.W[i].y = proj_eta[idx];
        output.W[i].flux = 0.0;
        output.W[i].saturated = false;
        output.gaia_ra[i]  = cat_ra[idx];
        output.gaia_dec[i] = cat_dec[idx];
    }

    if (logger) {
        char buf[256];
        std::snprintf(buf, sizeof(buf),
            "  W 向量组: %d×2 (FOV 内 %d, 取最亮 %d, gaia_ra/dec 已保存)",
            M, static_cast<int>(fov_idx.size()), M);
        logger->info(buf);
    }
#else
    // 非 Windows 平台不支持
    if (logger) logger->error("ipv_select_from_memory: 非 Windows 平台不支持");
    return -1;
#endif // _WIN32

    output.success = true;
    if (logger) logger->info("=== ipv_select_from_memory 完成 ===");
    return 0;
}

// ============================================================================
// 路径 A - ipv_select_from_detections
//
// 从外部 detections (FLOAT64 [N,6] star_det v1) 选星, 跳过 sdet_detect_ex。
// 算法与 ipv_select_from_memory 一致, 区别:
// - 跳过 float→uint16 转换
// - 跳过 sdet_detect_ex 调用
// - 直接从 detections 构建 U_full/mag_full 和 U
// ============================================================================

int ipv_select_from_detections(
    const double* detections,
    int n_detections,
    int image_width, int image_height,
    double ra, double dec,
    double focal_length_mm,
    double pixel_size_um,
    const IPVSolverParams& params,
    StarSelection& output,
    Logger* logger)
{
    // 清零输出
    output = StarSelection{};

    // --- 参数校验 ---
    if (focal_length_mm <= 0.0) {
        if (logger) logger->error("ipv_select_from_detections: focal_length_mm 非法");
        return -1;
    }
    if (pixel_size_um <= 0.0) {
        if (logger) logger->error("ipv_select_from_detections: pixel_size_um 非法");
        return -1;
    }
    if (detections == nullptr || n_detections <= 0) {
        if (logger) logger->error("ipv_select_from_detections: detections/n_detections 非法");
        return -1;
    }
    if (image_width <= 0 || image_height <= 0) {
        if (logger) logger->error("ipv_select_from_detections: image_width/height 非法");
        return -1;
    }

    if (logger) logger->info("=== ipv_select_from_detections 启动 (路径 A) ===");

    // --- 获取注入的句柄 ---
    void* gaia_handle = get_gaia_client_handle();
    if (!gaia_handle) {
        if (logger) logger->error("ipv_select_from_detections: GaiaClient 句柄未注入");
        return -1;
    }
    // 路径 A 不需要 detector_handle (跳过 sdet_detect_ex)

    // --- 加载依赖 DLL (路径 A 仍需 gaia_client.dll) ---
    if (!load_dlls(logger)) {
        if (logger) logger->error("ipv_select_from_detections: 依赖 DLL 加载失败");
        return -1;
    }

#ifdef _WIN32
    int img_w = image_width;
    int img_h = image_height;

    if (logger) {
        char buf[256];
        std::snprintf(buf, sizeof(buf),
            "Step 1: 使用外部 detections %d 颗, 图像 %d×%d (路径 A, 跳过 sdet_detect_ex)",
            n_detections, img_w, img_h);
        logger->info(buf);
    }

    // --- Step 2: 从 detections 构建内部检测向量 ---
    // star_det v1: [N,6] = x_px, y_px, flux, mag, saturated, has_saturated
    std::vector<double> det_x(n_detections), det_y(n_detections);
    std::vector<double> det_flux(n_detections);
    std::vector<float>  det_mag(n_detections);
    std::vector<int>    det_sat(n_detections), det_has_sat(n_detections);

    for (int i = 0; i < n_detections; ++i) {
        const double* row = detections + (size_t)i * 6;
        det_x[i]      = row[0];
        det_y[i]      = row[1];
        det_flux[i]   = row[2];
        det_mag[i]    = static_cast<float>(row[3]);
        det_sat[i]    = (row[4] != 0.0) ? 1 : 0;
        det_has_sat[i]= (row[5] != 0.0) ? 1 : 0;
    }

    if (logger) {
        int n_sat = 0;
        for (int i = 0; i < n_detections; ++i) if (det_sat[i]) n_sat++;
        char buf[256];
        std::snprintf(buf, sizeof(buf), "  detections: %d 颗 (饱和 %d, 正常 %d)",
                      n_detections, n_sat, n_detections - n_sat);
        logger->info(buf);
    }

    // --- Step 3: 图像侧选星 ( 按 mag 升序) ---
    if (logger) logger->info("Step 3: 图像侧选星");
    std::vector<double> flux_vec = det_flux;
    std::vector<double> mag_vec(det_mag.begin(), det_mag.end());
    std::vector<bool> sat_vec(n_detections);
    for (int i = 0; i < n_detections; ++i) sat_vec[i] = (det_sat[i] != 0);

    std::vector<int> sel_idx = select_image_stars(
        flux_vec, mag_vec, sat_vec, params.img_n_target, logger);
    int N = static_cast<int>(sel_idx.size());
    if (N < 2) {
        if (logger) logger->error("ipv_select_from_detections: 图像侧选星数过少");
        return -1;
    }

    // --- Step 4: 构建 U 向量组 (像素坐标, 原点图像中心, Y 轴向上) ---
    double s0 = IPV_ARCSEC_PER_UM_PER_MM * pixel_size_um / focal_length_mm;
    double cx = img_w / 2.0, cy = img_h / 2.0;
    output.U.resize(N);
    for (int i = 0; i < N; ++i) {
        int idx = sel_idx[i];
        output.U[i].x = (det_x[idx] - cx);
        output.U[i].y = -(det_y[idx] - cy);
        output.U[i].flux = det_flux[idx];
        output.U[i].saturated = (det_sat[idx] != 0);
    }
    output.img_width = img_w;
    output.img_height = img_h;

    // 保存全部检测星点供 robust_refine 使用 (坐标约定同 U)
    output.U_full.resize(n_detections);
    output.mag_full.resize(n_detections);
    for (int i = 0; i < n_detections; ++i) {
        output.U_full[i].x = det_x[i] - cx;
        output.U_full[i].y = -(det_y[i] - cy);
        output.U_full[i].flux = det_flux[i];
        output.U_full[i].saturated = (det_sat[i] != 0);
        output.mag_full[i] = static_cast<double>(det_mag[i]);
    }

    if (logger) {
        char buf[256];
        std::snprintf(buf, sizeof(buf), "  U 向量组: %d×2 (s0=%.4f\"/px)", N, s0);
        logger->info(buf);
    }

    // --- Step 5: 计算 FOV 与密度 ---
    if (logger) logger->info("Step 4: FOV/密度计算");
    double fov_diag_deg, query_radius_deg, query_area_sqdeg, img_area_sqdeg;
    double rho_img, rho_target;
    int n_target;
    compute_fov_density(
        focal_length_mm, pixel_size_um,
        static_cast<double>(img_w), static_cast<double>(img_h),
        N, params.gaia_density_ratio, params.gaia_query_radius_factor,
        s0, fov_diag_deg, query_radius_deg, query_area_sqdeg,
        img_area_sqdeg, rho_img, rho_target, n_target, logger);

    output.fov_diag_deg = fov_diag_deg;
    output.rho_img = rho_img;
    output.rho_target = rho_target;
    output.s0 = s0;

    // --- Step 6: 基于天球平均密度直接估算极限星等 ---
    if (logger) logger->info("Step 5: V4.9 密度公式估算极限星等 (替代迭代)");
    double m_lim_final = estimate_mag_lim_by_density(n_target, query_area_sqdeg, logger);
    int m_lim_iters = 0;

    // --- Step 7: 用估算的极限星等查询 Gaia 星表 (一次查询) ---
    if (logger) {
        char buf[256];
        std::snprintf(buf, sizeof(buf), "Step 6: 用 m_lim=%.3f 查询 Gaia 星表", m_lim_final);
        logger->info(buf);
    }
    std::vector<double> cat_ra, cat_dec;
    std::vector<float> cat_mag;
    int q_ret = gaia_query_stars(gaia_handle, ra, dec, query_radius_deg, m_lim_final,
                                  cat_ra, cat_dec, cat_mag);

    // 补救: 若一次查询不足, 逐步放宽 (最多 2 次, 每次 +1.0 mag)
    int rescue_count = 0;
    while ((q_ret != 0 || (int)cat_ra.size() < n_target) && rescue_count < 2) {
        double m_rescue = m_lim_final + 1.0 * (rescue_count + 1);
        if (logger) {
            char buf[256];
            std::snprintf(buf, sizeof(buf),
                "V4.9 补救查询 %d: Gaia 返回 %d < n_target=%d, 放宽到 m=%.3f",
                rescue_count + 1, (int)cat_ra.size(), n_target, m_rescue);
            logger->warn(buf);
        }
        cat_ra.clear(); cat_dec.clear(); cat_mag.clear();
        q_ret = gaia_query_stars(gaia_handle, ra, dec, query_radius_deg, m_rescue,
                                  cat_ra, cat_dec, cat_mag);
        if (q_ret == 0 && (int)cat_ra.size() >= n_target) {
            m_lim_final = m_rescue;
            break;
        }
        rescue_count++;
    }

    // 最终兜底: mag=22
    if (q_ret != 0 || cat_ra.size() < 2) {
        if (logger) logger->warn("Gaia 返回过少, 启用 mag=22 兜底查询");
        cat_ra.clear(); cat_dec.clear(); cat_mag.clear();
        q_ret = gaia_query_stars(gaia_handle, ra, dec, query_radius_deg, 22.0,
                                  cat_ra, cat_dec, cat_mag);
    }
    if (q_ret != 0 || cat_ra.size() < 2) {
        if (logger) logger->error("ipv_select_from_detections: Gaia 星表查询星数过少");
        return -1;
    }

    int n_gaia_final = (int)cat_ra.size();
    output.m_lim_final = m_lim_final;
    output.n_gaia_final = n_gaia_final;
    output.m_lim_iterations = m_lim_iters;

    // --- Step 9: Gnomonic 投影 + FOV 内过滤 ---
    if (logger) logger->info("Step 7: Gnomonic 投影 + FOV 过滤");
    double fov_half_w = img_w / 2.0 * s0;
    double fov_half_h = img_h / 2.0 * s0;

    const size_t n_cat = cat_ra.size();
    std::vector<double> proj_xi(n_cat), proj_eta(n_cat);
    std::vector<char> proj_valid(n_cat, 0);
    #pragma omp parallel for num_threads(16) schedule(static)
    for (ptrdiff_t i = 0; i < (ptrdiff_t)n_cat; ++i) {
        double xi = 0.0, eta = 0.0;
        bool valid = false;
        gnomonic_forward_proj(cat_ra[i], cat_dec[i], ra, dec, xi, eta, valid);
        proj_xi[i] = xi;
        proj_eta[i] = eta;
        proj_valid[i] = valid ? 1 : 0;
    }

    std::vector<int> fov_idx;
    for (size_t i = 0; i < n_cat; ++i) {
        if (!proj_valid[i]) continue;
        if (std::abs(proj_xi[i]) < fov_half_w && std::abs(proj_eta[i]) < fov_half_h) {
            fov_idx.push_back(static_cast<int>(i));
        }
    }
    if (fov_idx.size() < 2) {
        if (logger) logger->warn("FOV 内星数过少, 放宽到 1.5×FOV");
        fov_idx.clear();
        for (size_t i = 0; i < n_cat; ++i) {
            if (!proj_valid[i]) continue;
            if (std::abs(proj_xi[i]) < fov_half_w * 1.5 && std::abs(proj_eta[i]) < fov_half_h * 1.5) {
                fov_idx.push_back(static_cast<int>(i));
            }
        }
    }
    if (fov_idx.size() < 2) {
        if (logger) logger->error("ipv_select_from_detections: FOV 内 Gaia 星数过少");
        return -1;
    }

    // --- Step 10: 按星等升序 (最亮优先) 取前 n_target 颗 ---
    std::sort(fov_idx.begin(), fov_idx.end(),
              [&](int a, int b) { return cat_mag[a] < cat_mag[b]; });
    int M = std::min(n_target, static_cast<int>(fov_idx.size()));

    output.W.resize(M);
    output.gaia_ra.resize(M);
    output.gaia_dec.resize(M);
    for (int i = 0; i < M; ++i) {
        int idx = fov_idx[i];
        output.W[i].x = proj_xi[idx];
        output.W[i].y = proj_eta[idx];
        output.W[i].flux = 0.0;
        output.W[i].saturated = false;
        output.gaia_ra[i]  = cat_ra[idx];
        output.gaia_dec[i] = cat_dec[idx];
    }

    if (logger) {
        char buf[256];
        std::snprintf(buf, sizeof(buf),
            "  W 向量组: %d×2 (FOV 内 %d, 取最亮 %d, gaia_ra/dec 已保存)",
            M, static_cast<int>(fov_idx.size()), M);
        logger->info(buf);
    }
#else
    // 非 Windows 平台不支持
    if (logger) logger->error("ipv_select_from_detections: 非 Windows 平台不支持");
    return -1;
#endif // _WIN32

    output.success = true;
    if (logger) logger->info("=== ipv_select_from_detections 完成 (路径 A) ===");
    return 0;
}

// ============================================================================
// INTERNAL_DETECTION_SHARED_EXPORT (历史 路径 B) -
// ipv_select_from_memory_with_callback
//
// 与 ipv_select_from_memory 算法完全一致, 区别:
// - sdet_detect_ex 调用后, 选星前, 调用 callback 导出完整检测结果
// - callback 接收 FLOAT64 [N,6] star_det v1 格式
// - callback 为 NULL 时行为与 ipv_select_from_memory 完全一致
// ============================================================================

// 选星核心模板双实例 (T=float 原行为, T=double FP64 不降级)
template <typename T>
static int ipv_select_from_memory_with_callback_impl(
    const T* pixels,
    int width, int height,
    double ra, double dec,
    double focal_length_mm,
    double pixel_size_um,
    const IPVSolverParams& params,
    DetectionSinkFn callback,
    void* user_data,
    StarSelection& output,
    Logger* logger)
{
    // 清零输出
    output = StarSelection{};

    // --- 参数校验 ---
    if (focal_length_mm <= 0.0) {
        if (logger) logger->error("ipv_select_from_memory_with_callback: focal_length_mm 非法");
        return -1;
    }
    if (pixel_size_um <= 0.0) {
        if (logger) logger->error("ipv_select_from_memory_with_callback: pixel_size_um 非法");
        return -1;
    }
    if (pixels == nullptr || width <= 0 || height <= 0) {
        if (logger) logger->error("ipv_select_from_memory_with_callback: pixels/width/height 非法");
        return -1;
    }

    if (logger) logger->info("=== ipv_select_from_memory_with_callback 启动 (INTERNAL_DETECTION_SHARED_EXPORT) ===");

    // --- 获取注入的句柄 ---
    void* gaia_handle = get_gaia_client_handle();
    void* detector_handle = get_star_detector_handle();
    if (!gaia_handle) {
        if (logger) logger->error("ipv_select_from_memory_with_callback: GaiaClient 句柄未注入");
        return -1;
    }
    if (!detector_handle) {
        if (logger) logger->error("ipv_select_from_memory_with_callback: StarDetector 句柄未注入");
        return -1;
    }

    // --- 加载依赖 DLL ---
    if (!load_dlls(logger)) {
        if (logger) logger->error("ipv_select_from_memory_with_callback: 依赖 DLL 加载失败");
        return -1;
    }

#ifdef _WIN32
    // --- Step 1: 使用传入的内存像素数据 (不读文件) ---
    int img_w = width;
    int img_h = height;
    const T* pixel_data = pixels;
    if (logger) {
        char buf[256];
        std::snprintf(buf, sizeof(buf), "Step 1: 使用内存像素数据 %d×%d", img_w, img_h);
        logger->info(buf);
    }

    // T=float: 转 uint16 (旧行为); T=double: 直接用 double 图像, 不降级
    std::vector<uint16_t> img_u16;
    if constexpr (std::is_same_v<T, float>) {
        img_u16.resize(static_cast<size_t>(img_w) * img_h);
        for (size_t i = 0; i < img_u16.size(); ++i) {
            float v = pixel_data[i];
            if (v < 0.0f) v = 0.0f;
            if (v > 65535.0f) v = 65535.0f;
            img_u16[i] = static_cast<uint16_t>(v);
        }
    }

    // --- Step 2: 星点检测 ---
    if (logger) logger->info(std::is_same_v<T, float>
        ? "Step 2: 星点检测 (sdet_detect_ex)"
        : "Step 2: 星点检测 (sdet_detect_ex_f64, double 不降级)");
    double *det_x = nullptr, *det_y = nullptr;
    float *det_flux = nullptr;
    int *det_sat = nullptr;
    float *det_mag = nullptr;
    int *det_has_sat = nullptr;
    int det_count = 0;
    int det_ret;
    if constexpr (std::is_same_v<T, float>) {
        det_ret = g_dll.sdet_detect_ex(
            detector_handle, img_u16.data(), img_w, img_h,
            &det_x, &det_y, &det_flux, &det_sat,
            &det_mag, &det_has_sat, &det_count,
            nullptr, 0, nullptr);
    } else {
        det_ret = g_dll.sdet_detect_ex_f64(
            detector_handle, pixel_data, img_w, img_h,
            &det_x, &det_y, &det_flux, &det_sat,
            &det_mag, &det_has_sat, &det_count,
            nullptr, 0, nullptr);
    }
    if (det_ret != 0 || det_count <= 0) {
        if (logger) logger->error("ipv_select_from_memory_with_callback: 星点检测失败或未检测到星");
        if (det_x || det_y || det_flux || det_sat || det_mag || det_has_sat) {
            g_dll.sdet_free_ex(det_x, det_y, det_flux, det_sat,
                               det_mag, det_has_sat, nullptr, 0);
        }
        return -1;
    }
    if (logger) {
        int n_sat = 0;
        for (int i = 0; i < det_count; ++i) if (det_sat[i]) n_sat++;
        char buf[256];
        std::snprintf(buf, sizeof(buf), "  检测到星点: %d 颗 (饱和 %d, 正常 %d)",
                      det_count, n_sat, det_count - n_sat);
        logger->info(buf);
    }

    // --- Step 2.5 (INTERNAL_DETECTION_SHARED_EXPORT): 调用 callback 导出检测结果 (FLOAT64 [N,6]) ---
    // callback 在 sdet_free_ex 之前调用, 确保源指针有效
    // callback 返回后源指针仍由本函数管理, 稍后 sdet_free_ex 释放
    if (callback != nullptr) {
        if (logger) logger->info("Step 2.5: 调用 callback 导出 detections (INTERNAL_DETECTION_SHARED_EXPORT)");
        // 构建 FLOAT64 [N,6] 缓冲区 (栈上分配可能过大, 使用堆)
        std::vector<double> det_v1(static_cast<size_t>(det_count) * 6);
        for (int i = 0; i < det_count; ++i) {
            double* row = det_v1.data() + (size_t)i * 6;
            row[0] = det_x[i];
            row[1] = det_y[i];
            row[2] = static_cast<double>(det_flux[i]);
            row[3] = static_cast<double>(det_mag[i]);
            row[4] = static_cast<double>(det_sat[i]);
            row[5] = static_cast<double>(det_has_sat[i]);
        }
        // 调用 callback (同步, 调用期间缓冲区有效)
        callback(det_v1.data(), det_count, user_data);
        if (logger) logger->info("  callback 完成, 源缓冲区已由 callback 复制");
        // det_v1 在此处析构, callback 必须已复制数据
    }

    // --- Step 3: 图像侧选星 ( 按 mag 升序) ---
    if (logger) logger->info("Step 3: 图像侧选星");
    std::vector<double> flux_vec(det_flux, det_flux + det_count);
    std::vector<double> mag_vec(det_mag, det_mag + det_count);
    std::vector<bool> sat_vec(det_count);
    for (int i = 0; i < det_count; ++i) sat_vec[i] = (det_sat[i] != 0);

    std::vector<int> sel_idx = select_image_stars(
        flux_vec, mag_vec, sat_vec, params.img_n_target, logger);
    int N = static_cast<int>(sel_idx.size());
    if (N < 2) {
        if (logger) logger->error("ipv_select_from_memory_with_callback: 图像侧选星数过少");
        g_dll.sdet_free_ex(det_x, det_y, det_flux, det_sat,
                           det_mag, det_has_sat, nullptr, 0);
        return -1;
    }

    // --- Step 4: 构建 U 向量组 (像素坐标, 原点图像中心, Y 轴向上) ---
    double s0 = IPV_ARCSEC_PER_UM_PER_MM * pixel_size_um / focal_length_mm;
    double cx = img_w / 2.0, cy = img_h / 2.0;
    output.U.resize(N);
    for (int i = 0; i < N; ++i) {
        int idx = sel_idx[i];
        output.U[i].x = (det_x[idx] - cx);
        output.U[i].y = -(det_y[idx] - cy);
        output.U[i].flux = static_cast<double>(det_flux[idx]);
        output.U[i].saturated = (det_sat[idx] != 0);
    }
    output.img_width = img_w;
    output.img_height = img_h;

    // 保存全部检测星点供 robust_refine 使用
    output.U_full.resize(det_count);
    output.mag_full.resize(det_count);
    for (int i = 0; i < det_count; ++i) {
        output.U_full[i].x = det_x[i] - cx;
        output.U_full[i].y = -(det_y[i] - cy);
        output.U_full[i].flux = static_cast<double>(det_flux[i]);
        output.U_full[i].saturated = (det_sat[i] != 0);
        output.mag_full[i] = static_cast<double>(det_mag[i]);
    }

    // 释放星点检测内存
    g_dll.sdet_free_ex(det_x, det_y, det_flux, det_sat,
                       det_mag, det_has_sat, nullptr, 0);

    if (logger) {
        char buf[256];
        std::snprintf(buf, sizeof(buf), "  U 向量组: %d×2 (s0=%.4f\"/px)", N, s0);
        logger->info(buf);
    }

    // --- Step 5: 计算 FOV 与密度 ---
    if (logger) logger->info("Step 4: FOV/密度计算");
    double fov_diag_deg, query_radius_deg, query_area_sqdeg, img_area_sqdeg;
    double rho_img, rho_target;
    int n_target;
    compute_fov_density(
        focal_length_mm, pixel_size_um,
        static_cast<double>(img_w), static_cast<double>(img_h),
        N, params.gaia_density_ratio, params.gaia_query_radius_factor,
        s0, fov_diag_deg, query_radius_deg, query_area_sqdeg,
        img_area_sqdeg, rho_img, rho_target, n_target, logger);

    output.fov_diag_deg = fov_diag_deg;
    output.rho_img = rho_img;
    output.rho_target = rho_target;
    output.s0 = s0;

    // --- Step 6: 基于天球平均密度直接估算极限星等 ---
    if (logger) logger->info("Step 5: V4.9 密度公式估算极限星等 (替代迭代)");
    double m_lim_final = estimate_mag_lim_by_density(n_target, query_area_sqdeg, logger);
    int m_lim_iters = 0;

    // --- Step 7: 用估算的极限星等查询 Gaia 星表 (一次查询) ---
    if (logger) {
        char buf[256];
        std::snprintf(buf, sizeof(buf), "Step 6: 用 m_lim=%.3f 查询 Gaia 星表", m_lim_final);
        logger->info(buf);
    }
    std::vector<double> cat_ra, cat_dec;
    std::vector<float> cat_mag;
    int q_ret = gaia_query_stars(gaia_handle, ra, dec, query_radius_deg, m_lim_final,
                                  cat_ra, cat_dec, cat_mag);

    int rescue_count = 0;
    while ((q_ret != 0 || (int)cat_ra.size() < n_target) && rescue_count < 2) {
        double m_rescue = m_lim_final + 1.0 * (rescue_count + 1);
        if (logger) {
            char buf[256];
            std::snprintf(buf, sizeof(buf),
                "V4.9 补救查询 %d: Gaia 返回 %d < n_target=%d, 放宽到 m=%.3f",
                rescue_count + 1, (int)cat_ra.size(), n_target, m_rescue);
            logger->warn(buf);
        }
        cat_ra.clear(); cat_dec.clear(); cat_mag.clear();
        q_ret = gaia_query_stars(gaia_handle, ra, dec, query_radius_deg, m_rescue,
                                  cat_ra, cat_dec, cat_mag);
        if (q_ret == 0 && (int)cat_ra.size() >= n_target) {
            m_lim_final = m_rescue;
            break;
        }
        rescue_count++;
    }

    if (q_ret != 0 || cat_ra.size() < 2) {
        if (logger) logger->warn("Gaia 返回过少, 启用 mag=22 兜底查询");
        cat_ra.clear(); cat_dec.clear(); cat_mag.clear();
        q_ret = gaia_query_stars(gaia_handle, ra, dec, query_radius_deg, 22.0,
                                  cat_ra, cat_dec, cat_mag);
    }
    if (q_ret != 0 || cat_ra.size() < 2) {
        if (logger) logger->error("ipv_select_from_memory_with_callback: Gaia 星表查询星数过少");
        return -1;
    }

    int n_gaia_final = (int)cat_ra.size();
    output.m_lim_final = m_lim_final;
    output.n_gaia_final = n_gaia_final;
    output.m_lim_iterations = m_lim_iters;

    // --- Step 9: Gnomonic 投影 + FOV 内过滤 ---
    if (logger) logger->info("Step 7: Gnomonic 投影 + FOV 过滤");
    double fov_half_w = img_w / 2.0 * s0;
    double fov_half_h = img_h / 2.0 * s0;

    const size_t n_cat = cat_ra.size();
    std::vector<double> proj_xi(n_cat), proj_eta(n_cat);
    std::vector<char> proj_valid(n_cat, 0);
    #pragma omp parallel for num_threads(16) schedule(static)
    for (ptrdiff_t i = 0; i < (ptrdiff_t)n_cat; ++i) {
        double xi = 0.0, eta = 0.0;
        bool valid = false;
        gnomonic_forward_proj(cat_ra[i], cat_dec[i], ra, dec, xi, eta, valid);
        proj_xi[i] = xi;
        proj_eta[i] = eta;
        proj_valid[i] = valid ? 1 : 0;
    }

    std::vector<int> fov_idx;
    for (size_t i = 0; i < n_cat; ++i) {
        if (!proj_valid[i]) continue;
        if (std::abs(proj_xi[i]) < fov_half_w && std::abs(proj_eta[i]) < fov_half_h) {
            fov_idx.push_back(static_cast<int>(i));
        }
    }
    if (fov_idx.size() < 2) {
        if (logger) logger->warn("FOV 内星数过少, 放宽到 1.5×FOV");
        fov_idx.clear();
        for (size_t i = 0; i < n_cat; ++i) {
            if (!proj_valid[i]) continue;
            if (std::abs(proj_xi[i]) < fov_half_w * 1.5 && std::abs(proj_eta[i]) < fov_half_h * 1.5) {
                fov_idx.push_back(static_cast<int>(i));
            }
        }
    }
    if (fov_idx.size() < 2) {
        if (logger) logger->error("ipv_select_from_memory_with_callback: FOV 内 Gaia 星数过少");
        return -1;
    }

    // --- Step 10: 按星等升序 (最亮优先) 取前 n_target 颗 ---
    std::sort(fov_idx.begin(), fov_idx.end(),
              [&](int a, int b) { return cat_mag[a] < cat_mag[b]; });
    int M = std::min(n_target, static_cast<int>(fov_idx.size()));

    output.W.resize(M);
    output.gaia_ra.resize(M);
    output.gaia_dec.resize(M);
    for (int i = 0; i < M; ++i) {
        int idx = fov_idx[i];
        output.W[i].x = proj_xi[idx];
        output.W[i].y = proj_eta[idx];
        output.W[i].flux = 0.0;
        output.W[i].saturated = false;
        output.gaia_ra[i]  = cat_ra[idx];
        output.gaia_dec[i] = cat_dec[idx];
    }

    if (logger) {
        char buf[256];
        std::snprintf(buf, sizeof(buf),
            "  W 向量组: %d×2 (FOV 内 %d, 取最亮 %d, gaia_ra/dec 已保存)",
            M, static_cast<int>(fov_idx.size()), M);
        logger->info(buf);
    }
#else
    // 非 Windows 平台不支持
    if (logger) logger->error("ipv_select_from_memory_with_callback: 非 Windows 平台不支持");
    return -1;
#endif // _WIN32

    output.success = true;
    if (logger) logger->info("=== ipv_select_from_memory_with_callback 完成 (INTERNAL_DETECTION_SHARED_EXPORT) ===");
    return 0;
}

// ============================================================================
// ipv_select_from_memory_with_callback - FP32 入口 (float 图像, 原 ABI 兼容)
// ============================================================================
int ipv_select_from_memory_with_callback(
    const float* pixels,
    int width, int height,
    double ra, double dec,
    double focal_length_mm,
    double pixel_size_um,
    const IPVSolverParams& params,
    DetectionSinkFn callback,
    void* user_data,
    StarSelection& output,
    Logger* logger)
{
    return ipv_select_from_memory_with_callback_impl<float>(
        pixels, width, height, ra, dec, focal_length_mm, pixel_size_um,
        params, callback, user_data, output, logger);
}

// ============================================================================
// ipv_select_from_memory_with_callback_f64 - FP64 入口 (double 图像)
// double 图像直接检测, 不转 uint16/float
// ============================================================================
int ipv_select_from_memory_with_callback_f64(
    const double* pixels,
    int width, int height,
    double ra, double dec,
    double focal_length_mm,
    double pixel_size_um,
    const IPVSolverParams& params,
    DetectionSinkFn callback,
    void* user_data,
    StarSelection& output,
    Logger* logger)
{
    return ipv_select_from_memory_with_callback_impl<double>(
        pixels, width, height, ra, dec, focal_length_mm, pixel_size_um,
        params, callback, user_data, output, logger);
}

} // namespace ipv
