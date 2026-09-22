/* B4-13 缓存键/并发精细化锚点（不改算法/并发，仅文档化）：
 * - Cache key（精确匹配）: ra/dec/radius/mag_low/mag_high (double逐位) + db_type/file_count (dataset identity) + version=GAIA_CACHE_VERSION(2) — 见 lib/infrastructure/gaia_xpsd_client/src/gaia_client.c:112-128 (QueryCacheEntry) / 427-472 (query_cache_lookup) / 74-75 (GAIA_CACHE_VERSION)；不做量化舍入，命中即同一查询精确重复。
 * - 容量/生命周期: QueryCache 64条 (QUERY_CACHE_CAPACITY) + 总字节上限 QUERY_CACHE_MAX_BYTES = 64×200000×3×sizeof(double) = 307,200,000 B（模块 plan 合同值 kQueryCacheCap, 见 module_entry.c）/ TTL 60s (QUERY_CACHE_TTL_SEC)，事务性替换（先全分配成功再释放旧条目）+ 版本/过期校验失效 + 超限按 LRU (last_access) 淘汰；BlockCache 8192槽/文件 (BLOCK_CACHE_CAPACITY, 2^n) / **客户端级总预算** 4GB (BLOCK_CACHE_MAX_MEMORY, 由 GaiaClient.block_budget 在所有 XPSD 文件间共享) + 内存压力淘汰1/4 LRU — 见 lib/infrastructure/gaia_xpsd_client/src/gaia_client.c 缓存配置/解压块缓存函数/查询结果缓存函数；契约见 docs/algorithms/GAIA_QUERY.md Postconditions/Invariants/并行模型 与 docs/architecture/CACHE_POLICY.md Gaia查询缓存行。
 * - 并发/线程安全: GaiaClient.cache_lock 互斥（Win32 CRITICAL_SECTION / POSIX pthread_mutex_t, cache_lock/cache_unlock）包裹 query_cache_lookup/insert — 查询串行+缓存互斥，符合 docs/architecture/THREADING_MODEL.md「cache必须线程安全或单线程互斥访问」、docs/architecture/ERROR_MODEL.md 归类与 docs/architecture/OWNERSHIP_AND_LIFETIME.md 生命周期；极区剪枝见下 — thread-safe。
 */
/* GAIA_QUERY RA 环绕与极区保守剪枝锚点（B4-12，与 B2-06 对齐，不改算法）：
 * - RA 环绕: lib/infrastructure/gaia_xpsd_client/src/gaia_client.c:bbox_intersects 中按
 *   dra>180°→360°-dra 归一并以 cos(dec) 缩放判相交；极区 |cos(dec)|<0.01
 *   保守返回相交（避免经线收敛退化），裕量 1.2 保持无假阴性。
 * - 极区分支: lib/infrastructure/gaia_xpsd_client/src/gaia_client.c:polar_plane_intersects，
 *   |dec|>45° 进入 AE 极冠平面剪枝，|dec|>85° 仍保守（Lipschitz 常数
 *   C=π/2 / C45=π/(2√2)，平面盘 B(q,C·radius) 不相交则拒绝，false_negative=0）；
 *   跨 ±45° 边界或 θ_q+radius>90° 时保守不剪枝。
 * - 坐标契约: J2000，与 lib/algorithms/platesolve 共享 TAN/SIP 坐标约定
 *  （见 docs/algorithms/PLATESOLVE.md 数值风险段 / docs/science/ASTROMETRY.md
 *   失效条件 / docs/algorithms/GAIA_QUERY.md 数值风险段），无分叉；
 *   锥形查询为球面角距判定（Haversine 余弦定理），与 SIP 畸变几何正交、
 *   SIP 仅由 plate_solve 侧 WCS 前向/逆向处理（SCI-AST-001）。
 */

#ifndef GAIA_CLIENT_H
#define GAIA_CLIENT_H

#include <stddef.h>
#include <stdint.h>

/* CAT-GAIA-IMPL: GAIA_EXPORT 允许构建方预定义覆盖——模块 DLL target
 * astrocs_catalog_gaia 编译本生产源时以 -DGAIA_EXPORT= 将 12 个 legacy
 * 符号本地化（导出面仅 astrocs_module_query_v1，12 §1 / ABI-006）；
 * 未定义时保持原语义（legacy 测试/上游 Makefile 不受影响）。 */
#ifndef GAIA_EXPORT
#ifdef _WIN32
#define GAIA_EXPORT __declspec(dllexport)
#else
#define GAIA_EXPORT __attribute__((visibility("default")))
#endif
#endif /* GAIA_EXPORT */

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

/* ═════ CAT-GAIA-IMPL 模块内部接口（非导出面：无 GAIA_EXPORT，DLL 内可见） ═════
 * 仅供同 DLL 的 C ABI adapter（module_entry.c）与共址测试使用；legacy 调用方
 * 不受影响。plan 统计=只读元数据遍历（不解压数据块），cancel/租借=迁移桥段。 */

/* plan() 输入元数据统计（GAIA_QUERY.md §3.1：work_units 由叶块推导） */
typedef struct {
    int file_count;               /* XPSD 文件数（并行轴=文件, max_workers 上限） */
    int db_type;                  /* 检测出的 GaiaDbType */
    int spec_file_count;          /* 含光谱文件数 */
    long long leaf_blocks;        /* Σ 叶块数（work_units 基本单位） */
    long long work_units_bytes;   /* Σ 叶块未压缩字节（IO/解压工作量估计） */
    long long compressed_bytes;   /* Σ 叶块压缩字节（磁盘读取量估计） */
    long long mmap_bytes;         /* Σ mmap 只读映射字节（常驻内存估计） */
    long long max_block_bytes;    /* max(global_max_block_size)（每 worker scratch） */
} GaiaPlanStats;

int gaia_client_collect_plan_stats(GaiaClient *client, GaiaPlanStats *out_stats);

/* P19-gaia (RQS 行动单 B2 / V5-N-03): 因文件内 magnitudeRange 声明非法 (low>high
 * 颠倒 / nan / inf / 空串 / 超值域 / 尾随垃圾) 而放弃整 shard 星等剪枝的文件数，
 * 供测试与诊断读取的可见统计字段。非导出面 (无 GAIA_EXPORT)，不改变 legacy
 * 调用方；0 表示所有文件声明合法或缺失 (行为与历史一致)。 */
int gaia_client_get_magnitude_range_reject_count(GaiaClient *client);

/* ═══ FAILCLOSED-01 (GAIA-FAILCLOSED-01): 目录装载/查询的 fail-closed 诊断面 ═══
 * 背景（根因）: run/WCS-DETERMINISM-01/REPORT.md §1.2 —— gaia_client_create_ex
 * 对目录里每个 *.xpsd 调 load_xpsd_file, 返回 -1 时**没有 else 分支** ⇒ 装载
 * 失败的 shard 被静默丢弃（无日志/无计数/create 仍返回非 NULL）。地址空间受限
 * 时丢掉的恰是唯一含亮星的 shard ⇒ 参考星表静默变成"暗 shard 子集" ⇒
 * iter_trans_solve 全败（WCS 节点 fail-closed 发生在更下游，原因不可诊断）。
 *
 * 现合同（create 侧，fail-closed）:
 *   ① 目录内任一 *.xpsd 装载失败 ⇒ 逐文件记原因 + 计数 + 枚举结束后
 *      fprintf(stderr) 明确错误（目录/期望条目数/失败数/首个失败原因）
 *      + gaia_client_destroy + 返回 NULL；**不返回残缺 client**；
 *   ② 条目数 > MAX_FILES(32) ⇒ 同样拒绝（截断装载与失败装载同属"不完整星表"）；
 *   ③ 成功返回的 client 恒满足 file_count == file_entry_count 且
 *      file_load_fail_count == 0（db_type 过滤导致的跳过单独计数，不算失败）；
 *   ④ 空目录仍返回非 NULL 的空 client（file_count=0，历史语义不变）；
 *      上层节点须自行断言 file_count > 0（空星表 fail-closed）。
 * 现合同（查询侧，fail-closed）: 单星/单叶丢弃（collector 扩容失败 / 叶块解压
 *   失败 / scratch 分配失败）⇒ 计数 + 告警 + 查询返回 -1 且 out_* 置空；
 *   **不返回不完整星表**（截断上限 MAX_STARS_RESULT 是文档化契约，不属丢弃）。 */
typedef struct {
    int entry_count;        /* 目录内 *.xpsd 条目数（枚举到） */
    int file_count;         /* 成功装载且 db_type 匹配的 shard 数（= file_count） */
    int fail_count;         /* load_xpsd_file 返回 -1 的 shard 数 */
    int db_type_skipped;    /* 装载成功但 db_type 不匹配而关闭的 shard 数 */
    int max_files_exceeded; /* 条目数 > MAX_FILES（拒绝部分装载） */
    int dir_open_failed;    /* 目录不可打开（create 返回 NULL） */
    long long mmap_bytes;   /* 成功装载 shard 的 mmap 字节合计（地址空间需求诊断） */
    char first_failed_path[1024];
    char first_failed_reason[192];
} GaiaCreateDiagnostics;

/* 装载失败的 shard 数。create 成功返回的 client 恒为 0（>0 只出现在拒绝路径的
 * 诊断快照里）；供上层节点前置条件断言与 G-1 shard 覆盖门使用。 */
int gaia_client_get_file_load_fail_count(GaiaClient *client);
/* 目录内 *.xpsd 条目数（枚举到，含装载失败与被 db_type 过滤者）。 */
int gaia_client_get_file_entry_count(GaiaClient *client);
/* 最近一次（本线程）create 的诊断快照；返回 0 = 有快照，-1 = 无（out 不改）。 */
int gaia_client_get_last_create_diagnostics(GaiaCreateDiagnostics *out);
/* 最近一次 create 失败的人可读原因（目录/期望条目数/失败数/首个失败原因）；
 * 成功或尚未调用过 create 时返回 NULL。缓冲 thread-local，下次 create 覆盖。 */
const char *gaia_client_get_last_create_error(void);

/* worker 租借注入：execute 期生效（0=历史默认 OpenMP team，direct 路径不变） */
void gaia_set_worker_lease(int threads);

/* cancel 检查点注入：文件循环边界轮询（NULL=关闭，历史行为） */
void gaia_set_cancel_checkpoint(int (*poll_fn)(void *user_data), void *user_data);

#ifdef __cplusplus
}
#endif

#endif
