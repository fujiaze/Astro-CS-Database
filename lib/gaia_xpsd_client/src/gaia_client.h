/* B4-13 缓存键/并发精细化锚点（不改算法/并发，仅文档化）：
 * - Cache key（精确匹配）: ra/dec/radius/mag_low/mag_high (double逐位) + db_type/file_count (dataset identity) + version=GAIA_CACHE_VERSION(2) — 见 lib/gaia_xpsd_client/src/gaia_client.c:112-128 (QueryCacheEntry) / 427-472 (query_cache_lookup) / 74-75 (GAIA_CACHE_VERSION)；不做量化舍入，命中即同一查询精确重复。
 * - 容量/生命周期: QueryCache 64条 (QUERY_CACHE_CAPACITY) / TTL 60s (QUERY_CACHE_TTL_SEC)，事务性替换（先全分配成功再释放旧条目）+ 版本/过期校验失效；BlockCache 8192槽 (BLOCK_CACHE_CAPACITY, 2^n) / 上限4GB + 内存压力淘汰1/4 LRU — 见 lib/gaia_xpsd_client/src/gaia_client.c:64-70 / 391-557；契约见 docs/algorithms/GAIA_QUERY.md Postconditions/Invariants/并行模型 与 docs/architecture/CACHE_POLICY.md Gaia查询缓存行。
 * - 并发/线程安全: GaiaClient.cache_lock 互斥（Win32 CRITICAL_SECTION / POSIX pthread_mutex_t, cache_lock/cache_unlock）包裹 query_cache_lookup/insert — 查询串行+缓存互斥，符合 docs/architecture/THREADING_MODEL.md「cache必须线程安全或单线程互斥访问」、docs/architecture/ERROR_MODEL.md 归类与 docs/architecture/OWNERSHIP_AND_LIFETIME.md 生命周期；极区剪枝见下 — thread-safe。
 */
/* GAIA_QUERY RA 环绕与极区保守剪枝锚点（B4-12，与 B2-06 对齐，不改算法）：
 * - RA 环绕: lib/gaia_xpsd_client/src/gaia_client.c:bbox_intersects 中按
 *   dra>180°→360°-dra 归一并以 cos(dec) 缩放判相交；极区 |cos(dec)|<0.01
 *   保守返回相交（避免经线收敛退化），裕量 1.2 保持无假阴性。
 * - 极区分支: lib/gaia_xpsd_client/src/gaia_client.c:polar_plane_intersects，
 *   |dec|>45° 进入 AE 极冠平面剪枝，|dec|>85° 仍保守（Lipschitz 常数
 *   C=π/2 / C45=π/(2√2)，平面盘 B(q,C·radius) 不相交则拒绝，false_negative=0）；
 *   跨 ±45° 边界或 θ_q+radius>90° 时保守不剪枝。
 * - 坐标契约: J2000，与 lib/plate_solve 共享 TAN/SIP 坐标约定
 *  （见 docs/algorithms/PLATESOLVE.md 数值风险段 / docs/science/ASTROMETRY.md
 *   失效条件 / docs/algorithms/GAIA_QUERY.md 数值风险段），无分叉；
 *   锥形查询为球面角距判定（Haversine 余弦定理），与 SIP 畸变几何正交、
 *   SIP 仅由 plate_solve 侧 WCS 前向/逆向处理（SCI-AST-001）。
 */

#ifndef GAIA_CLIENT_H
#define GAIA_CLIENT_H

#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
#define GAIA_EXPORT __declspec(dllexport)
#else
#define GAIA_EXPORT __attribute__((visibility("default")))
#endif

typedef enum {
    GAIA_DB_AUTO = 0,
    GAIA_DB_DR3 = 1,
    GAIA_DB_DR3SP = 2
} GaiaDbType;

typedef struct {
    double ra;
    double dec;
    double magG;
    double magBP;
    double magRP;
    float parallax;
    float pmra;
    float pmdec;
    int64_t source_id;
} GaiaStar;

typedef struct {
    double ra;
    double dec;
    double magG;
    /* XPSD 记录内光谱量化参数 (PCL GaiaDatabaseFile::EncodedStarSPData):
     *   flux[j] = byte[j]*flux_mul + flux_min   (W*m^-2*nm^-1)
     * 无光谱数据时为 0。 */
    float flux_min;
    float flux_mul;
} GaiaSpectrumStar;

typedef struct {
    double ra;
    double dec;
    double magG;
    double magBP;
    double magRP;
} GaiaPhotometryStar;

typedef struct GaiaClient GaiaClient;

#ifdef __cplusplus
extern "C" {
#endif

GAIA_EXPORT GaiaClient *gaia_client_create(const char *data_dir);
GAIA_EXPORT GaiaClient *gaia_client_create_ex(const char *data_dir, GaiaDbType db_type);
GAIA_EXPORT void gaia_client_destroy(GaiaClient *client);

GAIA_EXPORT int gaia_client_cone_search(
    GaiaClient *client,
    double ra, double dec, double radius_deg,
    double mag_low, double mag_high,
    GaiaStar **out_stars, int *out_count);

GAIA_EXPORT int gaia_client_cone_search_for_solver(
    GaiaClient *client,
    double ra, double dec, double radius_deg,
    double mag_high,
    double **out_ra, double **out_dec, float **out_mag,
    int *out_count);

GAIA_EXPORT int gaia_client_get_db_type(GaiaClient *client);
GAIA_EXPORT int gaia_client_get_file_count(GaiaClient *client);
GAIA_EXPORT int gaia_client_get_total_sources(GaiaClient *client);

GAIA_EXPORT int gaia_client_cone_search_with_spectrum(
    GaiaClient *client,
    double ra, double dec, double radius_deg,
    double mag_low, double mag_high,
    GaiaSpectrumStar **out_stars,
    uint8_t **out_spectra,
    int *out_count);

GAIA_EXPORT int gaia_client_query_spectrum_by_coords(
    GaiaClient *client,
    const double *ra_list,
    const double *dec_list,
    int n_coords,
    double match_radius_arcsec,
    double mag_low,
    double mag_high,
    GaiaSpectrumStar **out_stars,
    uint8_t **out_spectra,
    int **out_match_idx,
    int *out_count);

GAIA_EXPORT int gaia_client_cone_search_with_photometry(
    GaiaClient *client,
    double ra, double dec, double radius_deg,
    double mag_low, double mag_high,
    GaiaPhotometryStar **out_stars,
    int *out_count);

GAIA_EXPORT int gaia_client_get_spectrum_params(
    GaiaClient *client,
    int *out_start_nm,
    int *out_step_nm,
    int *out_count);

#ifdef __cplusplus
}
#endif

#endif
