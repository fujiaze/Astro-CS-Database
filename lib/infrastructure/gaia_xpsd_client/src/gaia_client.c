#include "gaia_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>    /* P19-gaia: strtod 溢出判定 (ERANGE) */
#include <time.h>

#ifdef _OPENMP
#include <omp.h>
#endif

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/resource.h>   /* FAILCLOSED-01: RLIMIT_AS 诊断（装载失败可诊断） */
#include <pthread.h>
#endif

#include <zlib.h>

#define XPSD_MAGIC "XPSD0100"
#define WL_COUNT 343
#define STAR_STRIDE_SP (40 + (WL_COUNT + (WL_COUNT & 1)))
#define STAR_STRIDE_NOSP 32
#define MAX_FILES 32
#define MAX_STARS_RESULT 200000

/* P19-gaia (RQS 行动单 B2 / V5-N-03): magnitudeRange 声明的值域窗。
 * 出处:
 *   ① 问题扫描/findings/H_NUMERIC/p1/V5.md V5-N-03 建议值域窗 [-10, 40];
 *   ② 仓库内 36 个真实 shard (GaiaDR3 16 + GaiaDR3SP 20) 头内 magnitudeRange
 *      实测包络: low ∈ [-2.00, 21.05], high ∈ [16.59, 25.59] (DR3 生产目录
 *      16 片 + DR3SP 20 片, 登记于 run/perf-fix/P1-gaia/evidence/
 *      shard_magranges.tsv); [-10, 40] 完全包含该包络, low 侧余量 >= 8 mag、
 *      high 侧余量 >= 14 mag —— 仅用于拒绝"荒唐声明", 不改变任何合法生产
 *      shard 的解析值 (逐位不变见 eng/tests/unit/gaia_magnitude_range_bounds_test.c);
 *   ③ Gaia DR3 G 星等为物理带通星等, 负值/极大值声明均无产品语义。 */
#define GAIA_MAG_RANGE_MIN (-10.0)
#define GAIA_MAG_RANGE_MAX ( 40.0)

/* ═══ FAILCLOSED-01 (GAIA-FAILCLOSED-01): 静默部分装载 → fail-closed ══════════
 * 根因（run/WCS-DETERMINISM-01/REPORT.md §1.2）: gaia_client_create_ex 对目录里
 * 每个 *.xpsd 调 load_xpsd_file, 返回 -1 时**没有 else 分支** ⇒ 装载失败的
 * shard 被静默丢弃（无日志/无计数/create 仍返回非 NULL）。在地址空间受限
 * （RLIMIT_AS）时丢掉的恰是唯一含亮星的 shard, 参考星表随之变成"暗 shard 里
 * 最亮的 60 颗" ⇒ tri_B/max_vote 崩塌 ⇒ iter_trans_solve 全败。属"静默降级"
 * 缺陷类（AGENTS §6"不以环境问题掩盖失败"）。
 * 本块三条不变量（**不改任何科学公式/容差/归约顺序/输出星序**）:
 *   ① 目录内任一 *.xpsd 装载失败 ⇒ 记原因 + 计数 + create destroy 后返回 NULL
 *      （宁可失败, 不可用残缺星表解算）;
 *   ② 条目数 > MAX_FILES ⇒ 同上拒绝（截断装载与失败装载同属"不完整星表"）;
 *   ③ 查询期单星/单叶丢弃（collector 扩容失败 / 叶块解压失败 / scratch 分配
 *      失败）⇒ 记账 + 告警 + 查询返回 -1 且输出置空（不返回不完整星表）。
 * 暴露面（见 gaia_client.h）: gaia_client_get_file_count / _get_file_entry_count /
 *   _get_file_load_fail_count / _get_last_create_diagnostics / _get_last_create_error。
 * 文档化截断上限 MAX_STARS_RESULT（200000/文件）不属丢弃, 行为不变。 */

/* 最近一次 create 诊断槽：thread-local（同线程创建+读取, 无共享写; 与仓库既有
 * aio_*_last_error / drizzle 错误槽同款约定）。仅用于失败路径的可诊断消息与
 * G-1 shard 覆盖门, 不参与任何科学计算。 */
#if defined(_MSC_VER)
#define GAIA_TLS __declspec(thread)
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define GAIA_TLS _Thread_local
#else
#define GAIA_TLS __thread
#endif

static GAIA_TLS int g_create_diag_valid = 0;
static GAIA_TLS GaiaCreateDiagnostics g_create_diag;
static GAIA_TLS char g_create_error[1536];

static void create_diag_reset(void) {
    memset(&g_create_diag, 0, sizeof(g_create_diag));
    g_create_diag_valid = 1;
    g_create_error[0] = '\0';
}

/* 组装人可读失败原因（与 stderr 的 FATAL 行同源同字段）。 */
static void create_diag_fail(const char *data_dir, const GaiaCreateDiagnostics *d,
                             const char *what) {
    snprintf(g_create_error, sizeof(g_create_error),
             "gaia catalog %s: dir=%s entries=%d loaded=%d failed=%d "
             "db_type_skipped=%d first_failure=%s (%s)",
             what, data_dir ? data_dir : "(null)",
             d->entry_count, d->file_count, d->fail_count, d->db_type_skipped,
             d->first_failed_path[0] ? d->first_failed_path : "(none)",
             d->first_failed_reason[0] ? d->first_failed_reason : "(unknown)");
    /* FAILCLOSED-01 诊断（B3）: 装载失败的可诊断性靠"触发条件 + 需求规模"两条
     * 数字落地 —— 已装载 shard 的 mmap 字节合计（地址空间需求）与进程当前
     * RLIMIT_AS 上限。修复前这类失败只在下游表现为 "iter_trans_solve 全败"。 */
    {
        size_t used = strlen(g_create_error);
        if (used < sizeof(g_create_error)) {
            snprintf(g_create_error + used, sizeof(g_create_error) - used,
                     " loaded_mmap_bytes=%lld", d->mmap_bytes);
        }
    }
#ifndef _WIN32
    {
        struct rlimit rl;
        size_t used = strlen(g_create_error);
        if (used < sizeof(g_create_error) && getrlimit(RLIMIT_AS, &rl) == 0) {
            if (rl.rlim_cur == RLIM_INFINITY)
                snprintf(g_create_error + used, sizeof(g_create_error) - used,
                         " RLIMIT_AS=unlimited");
            else
                snprintf(g_create_error + used, sizeof(g_create_error) - used,
                         " RLIMIT_AS=%llu", (unsigned long long)rl.rlim_cur);
        }
    }
#endif
}

/* V18R3 测试钩子（仅测试程序定义 GAIA_ALLOC_TEST 时生效）：
 * 将本文件内所有堆分配重定向到测试包装，支持分配故障注入。 */
#ifdef GAIA_ALLOC_TEST
#define malloc gaia_test_malloc
#define calloc gaia_test_calloc
#define realloc gaia_test_realloc
#define free gaia_test_free
#endif

/* V18R3 诊断（ASTROCS_GAIA_TRACE=1 启用，默认零共享状态热写）：
 * 统计上下文 per-query 持有，仅查询入口调用一次 getenv 判断开关；
 * 并行线程只原子更新本查询上下文，不再触碰 process-global 计数器，
 * 因此并发查询之间不混合、无 data race，trace-off 时零共享写。 */
typedef struct {
    int enabled;
    long long nodes;
    long long blocks;
    long long bytes;
    long long decomp;
    long long polar_nodes;
    long long eq_nodes;
} GaiaTraceCtx;

static int gaia_trace_enabled(void) {
    const char *v = getenv("ASTROCS_GAIA_TRACE");
    return (v && v[0] == '1') ? 1 : 0;
}

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ═════ CAT-GAIA-IMPL 迁移桥段（纯技术接线，不改科学公式/输出位形） ═════
 * GAIA_QUERY.md §3.1 迁移合同：host executor 租借（并行轴=文件，1..file_count
 * workers）、cancel 检查点=文件循环边界、plan() 由数据集元数据推导 work_units。
 *
 * - worker 租借：adapter 在 execute 期注入租借线程数（0=未注入→行为与历史
 *   完全一致：OpenMP 默认 team=omp_get_max_threads）。direct 路径（共址测试
 *   #include 本源）从不注入 → direct-vs-plugin 对比不受租借影响（输出按
 *   文件序串接，与线程数无关，README §6）。
 * - cancel 检查点：adapter 注入轮询回调；4 个文件级并行循环在每次迭代
 *   （=文件循环边界）开头轮询，命中则跳过该文件工作；execute 出口据此返回
 *   CANCELLED。未注入时零开销零行为变化。
 * - plan 统计：gaia_client_collect_plan_stats 只读遍历树节点（数据集元数据，
 *   不解压数据块、不执行查询），供 module_entry.c plan() 真实推导
 *   work_units/IO/memory/parallel axis/min-max workers（禁空转假 plan）。 */
static int gaia_leased_workers = 0;   /* adapter execute 期注入; 0=默认 team */
static int (*gaia_cancel_poll_fn)(void *user_data) = NULL;
static void *gaia_cancel_poll_ud = NULL;

void gaia_set_worker_lease(int threads) { gaia_leased_workers = threads; }

void gaia_set_cancel_checkpoint(int (*poll_fn)(void *user_data), void *user_data) {
    gaia_cancel_poll_fn = poll_fn;
    gaia_cancel_poll_ud = user_data;
}

/* OpenMP team 大小：租借注入优先；否则与历史行为逐位一致（默认 team）。
 * #pragma omp num_threads(表达式) 运行期求值；无 _OPENMP 时恒 1。 */
static int gaia_omp_team_size(void) {
#ifdef _OPENMP
    if (gaia_leased_workers >= 1) return gaia_leased_workers;
    return omp_get_max_threads();
#else
    return 1;
#endif
}

/* 文件循环边界取消检查点（并行线程内调用；fn 原子读，无共享写） */
static int gaia_cancel_hit(void) {
    return gaia_cancel_poll_fn ? gaia_cancel_poll_fn(gaia_cancel_poll_ud) : 0;
}
/* ═════ 迁移桥段结束（gaia_client_collect_plan_stats 见 struct GaiaClient 后） ═════ */

#define DEG2RAD (M_PI / 180.0)
#define RAD2DEG (180.0 / M_PI)

/* ===== 缓存配置 ===== */
#define BLOCK_CACHE_CAPACITY  8192   /* 解压块缓存哈希表大小 (2的幂) */
#define BLOCK_CACHE_MASK       (BLOCK_CACHE_CAPACITY - 1)
#define QUERY_CACHE_CAPACITY   64     /* 查询结果缓存最大条目数 */
#define QUERY_CACHE_TTL_SEC    60     /* 查询结果缓存TTL (秒) */
/* G3b: 解压块缓存总预算。历史语义为"每个 XPSD 文件各 4GB"(BlockCache 嵌在
 * XPSDFileInternal 内), MAX_FILES=32 时同一客户端理论上可累积 32×4GB; 现改为
 * **客户端级总预算**——常量值不变(保留原常量作为默认总预算值), 但由
 * GaiaClient.block_budget 统一记账, 客户端内所有文件共享此上限(淘汰仍按
 * 各文件 LRU)。 */
#define BLOCK_CACHE_MAX_MEMORY (4ULL * 1024 * 1024 * 1024) /* 解压块缓存客户端总预算 4GB */
/* G3a: 查询结果缓存总字节上限 = 模块 plan 合同值 kQueryCacheCap
 * (来源: lib/infrastructure/gaia_xpsd_client/src/module_entry.c plan() 的
 *  const long long kQueryCacheCap = 64LL * 200000LL * sizeof(double)*3;)
 *   = QUERY_CACHE_CAPACITY(64) × MAX_STARS_RESULT(200000) × 3×sizeof(double)(24B)
 *   = 307,200,000 B ≈ 293 MiB (合同记作 "307 MB")。
 * 条目数上限由 QUERY_CACHE_CAPACITY(64) 槽位给出; 两者共同约束上限, 超限按
 * LRU 淘汰 (lookup 命中刷新 last_access)。 */
#define QUERY_CACHE_MAX_BYTES \
    ((size_t)QUERY_CACHE_CAPACITY * (size_t)MAX_STARS_RESULT * (sizeof(double) * 3))
#define MEMORY_PRESSURE_THRESHOLD (4ULL * 1024 * 1024 * 1024) /* 可用内存<4GB时触发释放 */
/* V18R3: 缓存键版本号。当 dataset / 字段集合 / 参数语义发生破坏性变化时,
 * 递增此版本号即可让旧缓存条目自然失效 (lookup 时 version 不匹配则跳过)。
 * 当前版本: 2 (精确查询语义键: ra/dec/radius/mag_low/mag_high/dataset identity,
 * 不做量化舍入; 命中即同一查询的精确重复) */
#define GAIA_CACHE_VERSION     2
#ifndef TIME_MAX
#define TIME_MAX ((time_t)-1 < 0 ? (time_t)((1ULL << (8 * sizeof(time_t) - 1)) - 1) : (time_t)(-1))
#endif

typedef struct {
    double x0, y0, x1, y1;
    int is_leaf;
    uint64_t block_offset;
    uint32_t block_size;
    uint32_t compressed_size;
    uint32_t child_nw, child_ne, child_sw, child_se;
} QTNode;

typedef struct {
    char projection[64];
    double center_ra, center_dec;
    QTNode *nodes;
    int node_count;
    uint32_t max_block_size;
} TreeInfo;

/* ===== 解压块缓存 ===== */
typedef struct {
    uint64_t block_offset;  /* 键: 0=空槽 */
    uint8_t *data;          /* 解压后的数据 */
    uint32_t data_size;     /* 数据大小 */
    time_t last_access;     /* 最后访问时间 (LRU) */
} BlockCacheEntry;

typedef struct {
    BlockCacheEntry *entries;
    int capacity;
    int count;
    size_t total_memory;
} BlockCache;

/* G3b: 客户端级解压块缓存总预算 (GaiaClient 持有, 所有 XPSD 文件共享)。
 * 锁语义: 插入/淘汰在持 per-file bc_lock 的前提下调用 budget 记账,
 * 加锁顺序恒为 file-lock → budget-lock, 不存在反向路径 (无死锁)。 */
typedef struct {
#ifdef _WIN32
    CRITICAL_SECTION lock;
#else
    pthread_mutex_t lock;
#endif
    int lock_ok;            /* 锁初始化成功标志 */
    size_t total_memory;    /* 当前所有文件解压块缓存字节合计 */
    size_t max_memory;      /* 客户端总预算 (= BLOCK_CACHE_MAX_MEMORY) */
} BlockCacheBudget;

/* ===== 查询结果缓存 ===== */
typedef struct {
    /* V18R3: 精确查询语义键（不做量化舍入）。命中条件=与本次查询的
     * ra/dec/radius/mag_low/mag_high 逐位一致（double 精确比较，仅同参
     * 数重复调用命中）+ dataset identity（db_type/file_count）+ 版本一致。
     * correctness 优先于缓存命中率；量化 superset 方案未采用。 */
    double ra, dec, radius, mag_low, mag_high;
    int db_type;                        /* dataset identity: GAIA_DB_DR3/DR3SP */
    int file_count;                     /* dataset identity: 文件数 */
    double *out_ra;                    /* 缓存的RA数组 */
    double *out_dec;                   /* 缓存的Dec数组 */
    /* KI-1 修复 (CAT-GAIA-IMPL, 纯技术: 不改算法/公式/键语义, scope
     * scientific_change=false): 缓存内 mag 由 float 提升为 double, 与
     * GAIA_QUERY.md §5 I3「缓存等价: 冷路径与缓存路径输出 bitwise 一致」
     * 对齐——缓存命中路径不再引入 float 往返量化误差。内存代价
     * +4B/星/条目 (64 条上限, 可忽略)。 */
    double *out_mag;                   /* 缓存的Mag数组 (double, bitwise 往返) */
    int out_count;                     /* 星数 */
    time_t timestamp;                  /* 缓存创建时间 (TTL 过期判定) */
    time_t last_access;                /* G3a: 最近命中时间 (LRU 淘汰判定) */
    int valid;                         /* 是否有效 */
    int version;                       /* P02-006: 缓存键版本号, 用于在 schema 变更时让旧条目失效 */
} QueryCacheEntry;

typedef struct {
    QueryCacheEntry entries[QUERY_CACHE_CAPACITY];
    int count;
    size_t total_memory;
} QueryCache;

typedef struct {
    char filepath[1024];
    /* FAILCLOSED-01: load_xpsd_file 失败原因（失败路径必写；成功时为空串）。
     * 供 create 循环聚合成可见错误与 GaiaCreateDiagnostics 快照。 */
    char load_error[192];
    char db_identifier[256];
    double magnitude_low, magnitude_high;
    int has_magnitude_range;  /* G1: XML magnitudeRange 是否成功解析 (0=不参与 shard 剪枝, 保守) */
    int magnitude_range_invalid; /* P19-gaia: 声明存在但非法 (已告警, 放弃该文件剪枝) */
    int total_sources;
    int has_spectrum;
    int star_stride;
    int spectrum_start;
    int spectrum_step;
    int spectrum_count;
    int spectrum_bits;
    int use_byte_shuffle;
    int item_size;
    char compression[64];
    uint64_t data_position;
    TreeInfo trees[16];
    int tree_count;
    uint32_t global_max_block_size;
#ifdef _WIN32
    HANDLE hFile, hMap;
#else
    int fd;
#endif
    uint8_t *mmap_data;
    size_t mmap_size;
    BlockCache block_cache;  /* 解压块缓存 (保留到关闭); G3b: 受 client->block_budget 约束 */
    BlockCacheBudget *budget; /* G3b: 客户端级总预算 (NULL=独立文件, 退回 per-file 上限语义) */
#ifdef _WIN32
    CRITICAL_SECTION bc_lock;   /* B4-P1-2: block_cache 互斥锁 */
#else
    pthread_mutex_t bc_lock;    /* B4-P1-2: block_cache 互斥锁 */
#endif
    int bc_lock_ok;
} XPSDFileInternal;

struct GaiaClient {
    XPSDFileInternal files[MAX_FILES];
    int file_count;
    GaiaDbType db_type;
    int db_type_detected;
    int magnitude_range_reject_count; /* P19-gaia: 声明非法而放弃剪枝的文件数 (可见统计) */
    /* FAILCLOSED-01: 目录枚举/装载计数（create 成功返回的 client 恒满足
     * file_count == file_entry_count 且 file_load_fail_count == 0）。 */
    int file_entry_count;        /* 目录内 *.xpsd 条目数（枚举到） */
    int file_load_fail_count;    /* load_xpsd_file 返回 -1 的 shard 数 */
    int file_db_type_skipped;    /* 装载成功但 db_type 不匹配而关闭的 shard 数 */
    int file_max_files_exceeded; /* 条目数 > MAX_FILES（拒绝截断装载） */
    char first_load_fail_path[1024];
    char first_load_fail_reason[192];
    QueryCache query_cache;  /* 查询结果缓存 (60s TTL) */
    BlockCacheBudget block_budget; /* G3b: 客户端级解压块缓存总预算 (所有文件共享) */
#ifdef _WIN32
    CRITICAL_SECTION cache_lock;
#else
    pthread_mutex_t cache_lock;
#endif
    int cache_lock_initialized;
};

/* CAT-GAIA-IMPL 迁移桥段（续）：plan() 元数据统计——只读遍历树节点，
 * 不解压数据块、不执行查询（GAIA_QUERY.md §3.1: work_units 由叶块推导）。 */
int gaia_client_collect_plan_stats(GaiaClient *client, GaiaPlanStats *out_stats) {
    if (!client || !out_stats) return -1;
    memset(out_stats, 0, sizeof(*out_stats));
    out_stats->file_count = client->file_count;
    out_stats->db_type = client->db_type_detected;
    uint32_t max_block = 0;
    for (int f = 0; f < client->file_count; f++) {
        XPSDFileInternal *xf = &client->files[f];
        if (xf->has_spectrum) out_stats->spec_file_count++;
        out_stats->mmap_bytes += (long long)xf->mmap_size;
        if (xf->global_max_block_size > max_block) max_block = xf->global_max_block_size;
        for (int t = 0; t < xf->tree_count; t++) {
            TreeInfo *ti = &xf->trees[t];
            for (int n = 0; n < ti->node_count; n++) {
                if (ti->nodes[n].is_leaf) {
                    out_stats->leaf_blocks++;
                    out_stats->work_units_bytes += (long long)ti->nodes[n].block_size;
                    out_stats->compressed_bytes += (long long)ti->nodes[n].compressed_size;
                }
            }
        }
    }
    out_stats->max_block_bytes = (long long)max_block;
    return 0;
}

/* ===== 简单星结构 ===== */
typedef struct {
    double ra, dec, magG;
} SimpleStar;

/* ── FAILCLOSED-01: 查询期"静默丢弃"记账 ────────────────────────────────────
 * 单星/单叶丢弃会让参考星表变成"错误但看起来正常"的子集（与 shard 静默丢弃
 * 同源，见 run/WCS-DETERMINISM-01/REPORT.md §1.3），故一律**记账 + 告警**并由
 * 查询入口 fail-closed（返回 -1、out_* 置空）—— 不返回不完整星表。
 * 记账器挂在每个 collector 上（per-file / per-coord，正常情况下无跨线程共享）；
 * 写侧用 named critical 兜底（query_spectrum_by_coords 路径一个记账器可能被多个
 * worker 写）。丢弃属异常路径，critical 的开销无关紧要。 */
typedef struct {
    int failed;             /* 0 = 本次查询完整; 1 = 发生过丢弃 */
    long dropped_stars;     /* collector 扩容失败丢掉的星数 */
    long dropped_leaves;    /* 整叶丢弃数（叶块解压失败 / scratch 分配失败） */
    char reason[192];       /* 首个丢弃原因（人可读） */
} GaiaDropLedger;

static void drop_ledger_note(GaiaDropLedger *dl, long stars, long leaves,
                             const char *reason) {
    if (!dl) return;
    #pragma omp critical(gaia_drop_ledger)
    {
        if (!dl->failed) {
            dl->failed = 1;
            snprintf(dl->reason, sizeof(dl->reason), "%s", reason);
            fprintf(stderr,
                    "gaia_client: ERROR query degraded: %s "
                    "(dropped_stars=%ld dropped_leaves=%ld) — refusing "
                    "incomplete catalog\n",
                    dl->reason, dl->dropped_stars + stars,
                    dl->dropped_leaves + leaves);
        }
        dl->dropped_stars += stars;
        dl->dropped_leaves += leaves;
    }
}

static void drop_ledger_init(GaiaDropLedger *dl) {
    if (dl) memset(dl, 0, sizeof(*dl));
}

typedef struct {
    SimpleStar *stars;
    int count;
    int capacity;
    GaiaDropLedger drop;    /* FAILCLOSED-01 */
} StarCollector;

typedef struct {
    double ra, dec, magG;
    float flux_min;
    float flux_mul;
} SpectrumStar;

typedef struct {
    SpectrumStar *stars;
    uint8_t *spectra;
    int count;
    int capacity;
    int spectrum_count;
    GaiaDropLedger drop;    /* FAILCLOSED-01 */
} SpectrumStarCollector;

typedef struct {
    double ra, dec, magG, magBP, magRP;
} PhotometryStar;

typedef struct {
    PhotometryStar *stars;
    int count;
    int capacity;
    GaiaDropLedger drop;    /* FAILCLOSED-01 */
} PhotometryStarCollector;

/* ===== 缓存辅助函数 ===== */

/* B4-P1-2: block_cache 锁封装。
 * 合同 docs/algorithms/GAIA_QUERY.md §4 单写者约定原本依赖
 * "并行轴=文件, 每文件仅单线程访问 block_cache"; 但 match 类查询
 * (gaia_client_query_spectrum_by_coords 等) 并行轴=坐标, 多线程并发
 * 读写同一文件的 block_cache (lookup 写 last_access / insert 淘汰+free /
 * 哈希表结构写) = 无同步数据竞争 (UAF)。修复: block_cache 读写全部
 * 持 per-file 锁; 解压缓冲使用调用方线程局部 scratch, 不共享。 */
#ifdef _WIN32
typedef CRITICAL_SECTION BcLock;
static int bc_lock_init(BcLock *l)       { InitializeCriticalSection(l); return 0; }
static void bc_lock_destroy(BcLock *l)   { DeleteCriticalSection(l); }
static void bc_lock_acquire(BcLock *l)   { EnterCriticalSection(l); }
static void bc_lock_release(BcLock *l)   { LeaveCriticalSection(l); }
#else
typedef pthread_mutex_t BcLock;
static int bc_lock_init(BcLock *l)       { return pthread_mutex_init(l, NULL); }
static void bc_lock_destroy(BcLock *l)   { pthread_mutex_destroy(l); }
static void bc_lock_acquire(BcLock *l)   { pthread_mutex_lock(l); }
static void bc_lock_release(BcLock *l)   { pthread_mutex_unlock(l); }
#endif

static void cache_lock(GaiaClient *client) {
    if (client->cache_lock_initialized) {
#ifdef _WIN32
        EnterCriticalSection(&client->cache_lock);
#else
        pthread_mutex_lock(&client->cache_lock);
#endif
    }
}

static void cache_unlock(GaiaClient *client) {
    if (client->cache_lock_initialized) {
#ifdef _WIN32
        LeaveCriticalSection(&client->cache_lock);
#else
        pthread_mutex_unlock(&client->cache_lock);
#endif
    }
}

/* 检查内存压力: 可用物理内存 < 阈值时返回1 */
static int check_memory_pressure(void) {
#ifdef _WIN32
    MEMORYSTATUSEX ms;
    ms.dwLength = sizeof(ms);
    if (GlobalMemoryStatusEx(&ms)) {
        return (ms.ullAvailPhys < MEMORY_PRESSURE_THRESHOLD) ? 1 : 0;
    }
#else
    /* Linux: 读取 /proc/meminfo */
    FILE *f = fopen("/proc/meminfo", "r");
    if (f) {
        char line[256];
        unsigned long mem_available = 0;
        while (fgets(line, sizeof(line), f)) {
            if (sscanf(line, "MemAvailable: %lu kB", &mem_available) == 1) {
                break;
            }
        }
        fclose(f);
        if (mem_available > 0 && mem_available * 1024 < MEMORY_PRESSURE_THRESHOLD)
            return 1;
    }
#endif
    return 0;
}

/* ===== G3b: 客户端级解压块缓存总预算 =====
 * 所有记账在持 per-file bc_lock 期间发生, budget 锁只保护 total_memory 的
 * 读-改-写; 加锁顺序 file-lock → budget-lock, 全代码无反向获取路径。 */
static void block_budget_init(BlockCacheBudget *b) {
    b->total_memory = 0;
    b->max_memory = (size_t)BLOCK_CACHE_MAX_MEMORY;
#ifdef _WIN32
    bc_lock_init(&b->lock);
    b->lock_ok = 1;
#else
    b->lock_ok = (bc_lock_init(&b->lock) == 0);
#endif
}

static void block_budget_destroy(BlockCacheBudget *b) {
    if (b->lock_ok) bc_lock_destroy(&b->lock);
    b->lock_ok = 0;
    b->total_memory = 0;
}

/* 快照: 当前客户端所有文件解压块缓存字节合计 */
static size_t block_budget_used(BlockCacheBudget *b) {
    if (!b) return 0;
    if (b->lock_ok) bc_lock_acquire(&b->lock);
    size_t v = b->total_memory;
    if (b->lock_ok) bc_lock_release(&b->lock);
    return v;
}

/* 原子"检查并预留": 仅当 total+delta ≤ 预算时才记账并返回 1 */
static int block_budget_try_reserve(BlockCacheBudget *b, size_t delta) {
    if (!b) return 1;
    int ok = 1;
    if (b->lock_ok) bc_lock_acquire(&b->lock);
    if (b->total_memory + delta > b->max_memory) {
        ok = 0;
    } else {
        b->total_memory += delta;
    }
    if (b->lock_ok) bc_lock_release(&b->lock);
    return ok;
}

static void block_budget_release(BlockCacheBudget *b, size_t delta) {
    if (!b) return;
    if (b->lock_ok) bc_lock_acquire(&b->lock);
    if (b->total_memory >= delta) b->total_memory -= delta;
    else b->total_memory = 0;
    if (b->lock_ok) bc_lock_release(&b->lock);
}

/* ===== 解压块缓存函数 ===== */

static void block_cache_init(BlockCache *bc) {
    bc->entries = (BlockCacheEntry *)calloc(BLOCK_CACHE_CAPACITY, sizeof(BlockCacheEntry));
    bc->capacity = BLOCK_CACHE_CAPACITY;
    bc->count = 0;
    bc->total_memory = 0;
}

/* 释放整表; budget 用于同步扣减客户端级记账 (NULL=独立文件/初始化失败路径) */
static void block_cache_free(BlockCache *bc, BlockCacheBudget *budget) {
    if (!bc->entries) return;
    for (int i = 0; i < bc->capacity; i++) {
        if (bc->entries[i].data) {
            size_t freed = bc->entries[i].data_size;
            free(bc->entries[i].data);
            bc->entries[i].data = NULL;
            bc->entries[i].data_size = 0;
            bc->entries[i].block_offset = 0;
            block_budget_release(budget, freed);
        }
    }
    free(bc->entries);
    bc->entries = NULL;
    bc->count = 0;
    bc->total_memory = 0;
}

/* 哈希函数: murmur3 finalizer */
static uint32_t block_hash(uint64_t key) {
    uint64_t h = key;
    h ^= h >> 33;
    h *= 0xff51afd7ed558ccdULL;
    h ^= h >> 33;
    h *= 0xc4ceb9fe1a85ec53ULL;
    h ^= h >> 33;
    return (uint32_t)(h & BLOCK_CACHE_MASK);
}

/* 淘汰 bc 中最旧 (LRU) 的一个条目, 同步 per-file 与客户端级记账。
 * 前提: 调用方已持 xf->bc_lock (bc 结构被独占)。 */
static void block_cache_evict_oldest_locked(BlockCache *bc, BlockCacheBudget *budget) {
    time_t oldest = TIME_MAX;
    int oldest_idx = -1;
    for (int i = 0; i < bc->capacity; i++) {
        if (bc->entries[i].block_offset != 0 && bc->entries[i].last_access < oldest) {
            oldest = bc->entries[i].last_access;
            oldest_idx = i;
        }
    }
    if (oldest_idx < 0) return;
    size_t freed = bc->entries[oldest_idx].data_size;
    free(bc->entries[oldest_idx].data);
    bc->entries[oldest_idx].data = NULL;
    bc->entries[oldest_idx].data_size = 0;
    bc->entries[oldest_idx].block_offset = 0;
    bc->count--;
    bc->total_memory -= freed;
    block_budget_release(budget, freed);
}

/* 查找解压块缓存 (B4-P1-2: 无锁内核, 前提=调用方已持 xf->bc_lock), 命中返回指针, 未命中返回NULL */
static uint8_t *block_cache_lookup_locked(BlockCache *bc, uint64_t block_offset, uint32_t *data_size) {
    if (!bc->entries) return NULL;
    uint32_t idx = block_hash(block_offset);
    for (int probe = 0; probe < bc->capacity; probe++) {
        uint32_t i = (idx + probe) & BLOCK_CACHE_MASK;
        if (bc->entries[i].block_offset == 0) return NULL;  /* 空槽, 未命中 */
        if (bc->entries[i].block_offset == block_offset) {
            bc->entries[i].last_access = time(NULL);
            *data_size = bc->entries[i].data_size;
            return bc->entries[i].data;  /* 命中 */
        }
    }
    return NULL;
}

/* 查找解压块缓存 (B4-P1-2: 持 xf->bc_lock) */
static uint8_t *block_cache_lookup(XPSDFileInternal *xf, uint64_t block_offset, uint32_t *data_size) {
    uint8_t *result = NULL;
    if (xf->bc_lock_ok) bc_lock_acquire(&xf->bc_lock);
    result = block_cache_lookup_locked(&xf->block_cache, block_offset, data_size);
    if (xf->bc_lock_ok) bc_lock_release(&xf->bc_lock);
    return result;
}

/* 插入解压块缓存 (B4-P1-2: 无锁内核, 前提=调用方已持 xf->bc_lock)。
 * G3b: budget!=NULL 时按**客户端总量**判定——先淘汰本文件 LRU 直到可容纳,
 * 再原子预留; 若预算被其他文件占用/并发占满则拒绝插入 (回退 scratch)。
 * budget==NULL 时保持历史 per-file 上限语义 (独立文件/测试路径)。
 * 返回: 成功=缓存内副本指针 (锁保护下读取安全); 失败=NULL (调用方回退 scratch) */
static uint8_t *block_cache_insert_locked(BlockCache *bc, BlockCacheBudget *budget,
                                           uint64_t block_offset,
                                           const uint8_t *data, uint32_t data_size) {
    if (!bc->entries || data_size == 0) return NULL;

    int pressure = check_memory_pressure();

    if (budget) {
        /* 按客户端总预算淘汰本文件最旧条目 (跨文件不取其他锁, 避免锁序反转) */
        size_t guard = (size_t)bc->capacity + 1;
        while (bc->count > 0 &&
               block_budget_used(budget) + data_size > budget->max_memory &&
               guard-- > 0) {
            block_cache_evict_oldest_locked(bc, budget);
        }
        if (pressure && bc->count > 0) {
            int to_evict = bc->count / 4 + 1;
            for (int e = 0; e < to_evict && bc->count > 0; e++)
                block_cache_evict_oldest_locked(bc, budget);
        }
        /* 原子预留本次插入的字节; 失败=客户端预算已被占满 → 不缓存 */
        if (!block_budget_try_reserve(budget, data_size)) return NULL;
    } else if (bc->total_memory + data_size > BLOCK_CACHE_MAX_MEMORY || pressure) {
        /* 历史 per-file 语义: 淘汰最旧 1/4 */
        int to_evict = bc->count / 4 + 1;
        for (int e = 0; e < to_evict && bc->count > 0; e++)
            block_cache_evict_oldest_locked(bc, NULL);
    }

    /* 开放寻址插入 */
    uint32_t idx = block_hash(block_offset);
    for (int probe = 0; probe < bc->capacity; probe++) {
        uint32_t i = (idx + probe) & BLOCK_CACHE_MASK;
        if (bc->entries[i].block_offset == 0) {
            /* 空槽, 插入 */
            uint8_t *copy = (uint8_t *)malloc(data_size);
            if (!copy) { block_budget_release(budget, data_size); return NULL; }
            memcpy(copy, data, data_size);
            bc->entries[i].block_offset = block_offset;
            bc->entries[i].data = copy;
            bc->entries[i].data_size = data_size;
            bc->entries[i].last_access = time(NULL);
            bc->count++;
            bc->total_memory += data_size;
            return copy;
        }
        if (bc->entries[i].block_offset == block_offset) {
            /* 已存在, 更新 (B4-P1-2: 返回缓存内权威副本, 调用方不得读 scratch 旧数据) */
            if (bc->entries[i].data_size == data_size) {
                memcpy(bc->entries[i].data, data, data_size);
                bc->entries[i].last_access = time(NULL);
                block_budget_release(budget, data_size);  /* 同键同大小: 无净增, 退回预留 */
                return bc->entries[i].data;
            }
            /* 大小不同, 替换 (同一 block_offset 大小通常不变, 此分支为防御性)。
             * 记账按净额 delta = new-old 精确调整: 先退回预留, 若净增则原子预留
             * 净增部分, 预留失败则保持旧条目不动并回退 scratch; 净减直接释放。 */
            size_t old_size = bc->entries[i].data_size;
            block_budget_release(budget, data_size);  /* 退回整额预留 */
            if (budget && data_size > old_size &&
                !block_budget_try_reserve(budget, data_size - old_size)) {
                return NULL;  /* 预算不足, 旧条目原样保留 */
            }
            uint8_t *copy = (uint8_t *)malloc(data_size);
            if (!copy) {
                if (budget && data_size > old_size)
                    block_budget_release(budget, data_size - old_size);
                return NULL;  /* 旧条目原样保留 */
            }
            memcpy(copy, data, data_size);
            free(bc->entries[i].data);
            bc->entries[i].data = copy;
            bc->entries[i].data_size = data_size;
            bc->entries[i].last_access = time(NULL);
            bc->total_memory -= old_size;
            bc->total_memory += data_size;
            if (budget && data_size < old_size)
                block_budget_release(budget, old_size - data_size);
            return copy;
        }
    }
    /* 哈希表满, 不插入 */
    block_budget_release(budget, data_size);
    return NULL;
}

/* 插入解压块缓存 (B4-P1-2: 持 xf->bc_lock); G3b: xf->budget 为客户端级预算 */
static uint8_t *block_cache_insert(XPSDFileInternal *xf, uint64_t block_offset,
                                    const uint8_t *data, uint32_t data_size) {
    uint8_t *result = NULL;
    if (xf->bc_lock_ok) bc_lock_acquire(&xf->bc_lock);
    result = block_cache_insert_locked(&xf->block_cache, xf->budget, block_offset, data, data_size);
    if (xf->bc_lock_ok) bc_lock_release(&xf->bc_lock);
    return result;
}

/* ===== 查询结果缓存函数 ===== */

static void query_cache_init(QueryCache *qc) {
    memset(qc->entries, 0, sizeof(qc->entries));
    qc->count = 0;
    qc->total_memory = 0;
}

static void query_cache_free(QueryCache *qc) {
    for (int i = 0; i < QUERY_CACHE_CAPACITY; i++) {
        if (qc->entries[i].valid) {
            free(qc->entries[i].out_ra);
            free(qc->entries[i].out_dec);
            free(qc->entries[i].out_mag);
            qc->total_memory -= ((size_t)qc->entries[i].out_count * (sizeof(double) * 3));
            qc->entries[i].valid = 0;
        }
    }
    qc->count = 0;
    qc->total_memory = 0;
}

/* 清理过期条目 */
static void query_cache_evict_expired(QueryCache *qc) {
    time_t now = time(NULL);
    for (int i = 0; i < QUERY_CACHE_CAPACITY; i++) {
        if (qc->entries[i].valid && (now - qc->entries[i].timestamp) > QUERY_CACHE_TTL_SEC) {
            free(qc->entries[i].out_ra);
            free(qc->entries[i].out_dec);
            free(qc->entries[i].out_mag);
            qc->total_memory -= ((size_t)qc->entries[i].out_count * (sizeof(double) * 3));
            qc->entries[i].valid = 0;
            qc->count--;
        }
    }
}

/* 查找查询结果缓存 */
static int query_cache_lookup(GaiaClient *client, double ra, double dec,
                               double radius, double mag_low, double mag_high,
                               double **out_ra, double **out_dec,
                               double **out_mag, int *out_count) {
    QueryCache *qc = &client->query_cache;
    time_t now = time(NULL);

    for (int i = 0; i < QUERY_CACHE_CAPACITY; i++) {
        if (!qc->entries[i].valid) continue;
        if ((now - qc->entries[i].timestamp) > QUERY_CACHE_TTL_SEC) {
            /* 过期, 清理 */
            free(qc->entries[i].out_ra);
            free(qc->entries[i].out_dec);
            free(qc->entries[i].out_mag);
            qc->total_memory -= ((size_t)qc->entries[i].out_count * (sizeof(double) * 3));
            qc->entries[i].valid = 0;
            qc->count--;
            continue;
        }
        /* P02-006: 版本不匹配视为失效, 清理后跳过 (旧 schema 条目不可复用) */
        if (qc->entries[i].version != GAIA_CACHE_VERSION) {
            free(qc->entries[i].out_ra);
            free(qc->entries[i].out_dec);
            free(qc->entries[i].out_mag);
            qc->total_memory -= ((size_t)qc->entries[i].out_count * (sizeof(double) * 3));
            qc->entries[i].valid = 0;
            qc->count--;
            continue;
        }
        if (qc->entries[i].ra == ra &&
            qc->entries[i].dec == dec &&
            qc->entries[i].radius == radius &&
            qc->entries[i].mag_low == mag_low &&
            qc->entries[i].mag_high == mag_high &&
            qc->entries[i].db_type == client->db_type_detected &&
            qc->entries[i].file_count == client->file_count) {
            /* 命中 (G3a: 刷新 LRU 最近访问时间, 不改命中判定) */
            qc->entries[i].last_access = now;
            *out_ra = qc->entries[i].out_ra;
            *out_dec = qc->entries[i].out_dec;
            *out_mag = qc->entries[i].out_mag;
            *out_count = qc->entries[i].out_count;
            return 1;  /* 缓存命中 */
        }
    }
    return 0;  /* 未命中 */
}

/* 插入查询结果缓存
 * KI-1 修复: mag 缓存通道 float→double（bitwise 往返，I3 对齐）。 */
static void query_cache_insert(GaiaClient *client, double ra, double dec,
                                double radius, double mag_low, double mag_high,
                                double *out_ra, double *out_dec,
                                double *out_mag, int out_count) {
    QueryCache *qc = &client->query_cache;

    /* V18R3: 事务式替换——先全部分配成功再释放旧条目，分配失败绝不留
     * 半状态（valid=1 但指针 dangling 的条目）。 */
    size_t ra_size = (size_t)out_count * sizeof(double);
    size_t dec_size = (size_t)out_count * sizeof(double);
    size_t mag_size = (size_t)out_count * sizeof(double);
    double *new_ra = (double *)malloc(ra_size);
    double *new_dec = (double *)malloc(dec_size);
    double *new_mag = (double *)malloc(mag_size);
    if (!new_ra || !new_dec || !new_mag) {
        free(new_ra);
        free(new_dec);
        free(new_mag);
        return;
    }
    memcpy(new_ra, out_ra, ra_size);
    memcpy(new_dec, out_dec, dec_size);
    memcpy(new_mag, out_mag, mag_size);

    size_t entry_bytes = (size_t)out_count * (sizeof(double) * 3);

    /* G3a: 单条超总字节上限 (合同 kQueryCacheCap) → 整条不缓存。 */
    if (entry_bytes > (size_t)QUERY_CACHE_MAX_BYTES) {
        free(new_ra);
        free(new_dec);
        free(new_mag);
        return;
    }

    /* 内存压力检查 */
    if (check_memory_pressure()) {
        query_cache_evict_expired(qc);
        if (check_memory_pressure()) {
            free(new_ra);
            free(new_dec);
            free(new_mag);
            return;  /* 仍然压力大, 不缓存 */
        }
    }

    /* G3a: 条目数上限 QUERY_CACHE_CAPACITY(64) 或总字节上限 QUERY_CACHE_MAX_BYTES
     * 超限时按 LRU (last_access 最旧) 淘汰, 直到可容纳本条。淘汰只减少存量,
     * 不改变任意未淘汰条目的精确键匹配/命中结果。 */
    while (qc->count >= QUERY_CACHE_CAPACITY ||
           qc->total_memory + entry_bytes > (size_t)QUERY_CACHE_MAX_BYTES) {
        int lru = -1;
        time_t oldest = TIME_MAX;
        for (int i = 0; i < QUERY_CACHE_CAPACITY; i++) {
            if (qc->entries[i].valid && qc->entries[i].last_access < oldest) {
                oldest = qc->entries[i].last_access;
                lru = i;
            }
        }
        if (lru < 0) break;  /* 不变式: count>0 时必有 valid 条目 */
        free(qc->entries[lru].out_ra);
        free(qc->entries[lru].out_dec);
        free(qc->entries[lru].out_mag);
        qc->total_memory -= ((size_t)qc->entries[lru].out_count * (sizeof(double) * 3));
        qc->entries[lru].valid = 0;
        qc->count--;
    }

    /* 找空槽 (淘汰后必存在: count < QUERY_CACHE_CAPACITY) */
    int slot = -1;
    for (int i = 0; i < QUERY_CACHE_CAPACITY; i++) {
        if (!qc->entries[i].valid) { slot = i; break; }
    }
    if (slot < 0) {
        free(new_ra);
        free(new_dec);
        free(new_mag);
        return;
    }

    /* commit 新条目 */
    qc->entries[slot].out_ra = new_ra;
    qc->entries[slot].out_dec = new_dec;
    qc->entries[slot].out_mag = new_mag;
    qc->entries[slot].ra = ra;
    qc->entries[slot].dec = dec;
    qc->entries[slot].radius = radius;
    qc->entries[slot].mag_low = mag_low;
    qc->entries[slot].mag_high = mag_high;
    qc->entries[slot].db_type = client->db_type_detected;
    qc->entries[slot].file_count = client->file_count;
    qc->entries[slot].out_count = out_count;
    qc->entries[slot].timestamp = time(NULL);
    qc->entries[slot].last_access = qc->entries[slot].timestamp;  /* G3a: LRU 起点 */
    qc->entries[slot].valid = 1;
    qc->entries[slot].version = GAIA_CACHE_VERSION;
    qc->total_memory += entry_bytes;
    qc->count++;
}

/* ===== 原有辅助函数 ===== */

static const char *find_tag(const char *xml, const char *tag) {
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "<%s ", tag);
    const char *p = strstr(xml, pattern);
    if (p) return p;
    snprintf(pattern, sizeof(pattern), "<%s>", tag);
    p = strstr(xml, pattern);
    return p;
}

/* P19-gaia: 返回值 = 是否取到带引号的属性值 (1=属性存在且闭合引号; 0=缺失/
 * 未加引号/无闭合)。magnitudeRange 需区分"属性缺失"(正常, 不告警) 与
 * "属性存在但值为空/畸形"(必须告警并放弃剪枝); 其余调用点忽略返回值, 行为不变。 */
static int parse_attr_after(const char *start, const char *attr, char *out, int out_size) {
    out[0] = '\0';
    char pattern[256];
    snprintf(pattern, sizeof(pattern), "%s=", attr);
    const char *a = strstr(start, pattern);
    if (!a) return 0;
    a += strlen(pattern);
    if (*a == '"' || *a == '\'') {
        char quote = *a;
        a++;
        const char *end = strchr(a, quote);
        if (end) {
            int len = (int)(end - a);
            if (len >= out_size) len = out_size - 1;
            memcpy(out, a, len);
            out[len] = '\0';
            return 1;
        }
    }
    return 0;
}

static int parse_int_after(const char *start, const char *attr, int def) {
    char buf[128];
    parse_attr_after(start, attr, buf, sizeof(buf));
    return buf[0] ? atoi(buf) : def;
}

static void extract_tag_text(const char *xml, const char *tag, char *out, int out_size) {
    out[0] = '\0';
    char open_tag[128], close_tag[128];
    snprintf(open_tag, sizeof(open_tag), "<%s>", tag);
    snprintf(close_tag, sizeof(close_tag), "</%s>", tag);
    const char *start = strstr(xml, open_tag);
    if (!start) return;
    start += strlen(open_tag);
    const char *end = strstr(start, close_tag);
    if (!end) return;
    int len = (int)(end - start);
    if (len >= out_size) len = out_size - 1;
    while (len > 0 && (start[len-1] == ' ' || start[len-1] == '\n' || start[len-1] == '\r')) len--;
    memcpy(out, start, len);
    out[len] = '\0';
}

static void unproject(double x, double y, const char *proj, double cra, double cdec,
                       double *ra, double *dec) {
    if (strcmp(proj, "Equirectangular") == 0) {
        *ra = x + cra;
        *dec = y;
    } else if (strcmp(proj, "AzimuthalEquidistant") == 0) {
        double xr = x * DEG2RAD;
        double yr = y * DEG2RAD;
        double r = sqrt(xr * xr + yr * yr);
        if (r < 1e-15) {
            *ra = cra;
            *dec = cdec;
        } else {
            double c = asin(cos(r));
            if (cdec < 0) c = -c;
            *ra = cra + atan2(xr * sin(r) / r, yr * sin(r) / r) * RAD2DEG;
            *dec = c * RAD2DEG;
        }
        if (*ra < 0) *ra += 360.0;
        if (*ra >= 360.0) *ra -= 360.0;
    } else {
        *ra = x;
        *dec = y;
    }
}

static int bbox_intersects(double ra, double dec, double radius_deg,
                            double ra_min, double ra_max, double dec_min, double dec_max) {
    if (dec + radius_deg < dec_min || dec - radius_deg > dec_max) return 0;
    double d_ra = 0;
    if (ra < ra_min) d_ra = ra_min - ra;
    else if (ra > ra_max) d_ra = ra - ra_max;
    if (d_ra > 180) d_ra = 360 - d_ra;
    double cos_dec = cos(dec * DEG2RAD);
    if (cos_dec < 0.01) return 1;
    if (d_ra * cos_dec > radius_deg * 1.2) return 0;
    return 1;
}

/* V18R3：极区（AzimuthalEquidistant）投影平面剪枝——可证明保守 predicate。
 *
 * 投影约定（与树构建一致）：投影中心=极点，平面坐标
 *   x = theta*sin(phi), y = theta*cos(phi)   （单位：度）
 * 其中 theta = 到极点的余纬（度），phi = (ra - tree_center_ra)。
 * 节点 bbox 为投影平面轴对齐矩形 [x0,x1]×[y0,y1]（度）。
 *
 * 数学性质（V18R3 证明，取代 V18R2 的经验裕量 1.2）：
 *   AE 投影以极点为投影中心时，局部缩放 径向=1、切向=theta/sin(theta)，
 *   因此对任意两点 p,q（theta_p, theta_q ∈ [0, pi/2]）：
 *     d_plane(p,q) <= max_{theta∈[0,pi/2]} (theta/sin theta) * delta(p,q)
 *                  <= (pi/2) * delta(p,q)
 *   当两点都位于 theta <= pi/4 子冠内时可用更紧常数
 *     C45 = (pi/4)/sin(pi/4) = pi/(2*sqrt(2)) ~= 1.11072。
 *   即：投影平面距离 ≤ C * 天球角距。
 *
 * 剪枝判据（无假阴性）：
 *   查询 cone（中心 q、半径 radius）内任意点的投影落在平面圆盘
 *   B(q, C*radius) 内；节点矩形与 B 不相交 ⟹ 节点内所有点都在 cone 外
 *   ⟹ 可安全拒绝（拒绝只发生在数学必然相离时，允许误报）。
 *
 * 查询中心在极冠外：
 *   - theta_q - radius > 45°：cone 完全在极冠外，整棵极冠树必不相交 → 拒绝；
 *   - 否则（cone 可能跨过 ±45° 边界）：返回相交（不剪枝，遍历极冠树）。
 *     该分支只影响查询中心距极冠边界 < radius 的少量查询，优先保证无假阴性。
 *   V18R2 的“center outside cap → 必不相交”判断是错误的：
 *   cone 有半径，中心在 44.8°、半径 1° 时必然与极冠相交。
 */
#define AE_CAP_BOUNDARY_DEG 45.0
#define AE_C45_FACTOR       1.1107207345395915   /* pi/(2*sqrt(2)) */
#define AE_GLOBAL_FACTOR    1.5707963267948966   /* pi/2 */

/* V18R3 测试钩子：定义 GAIA_POLAR_PRUNE_DISABLED 时极区剪枝恒为
 * "相交"（不剪枝），作为 differential 测试的 reference mode。
 * 生产编译（未定义）行为与直接调用 predicate 完全一致。 */
#ifdef GAIA_POLAR_PRUNE_DISABLED
#define POLAR_PRUNE_INTERSECTS(...) 1
#else
#define POLAR_PRUNE_INTERSECTS(...) polar_plane_intersects(__VA_ARGS__)
#endif

static int polar_plane_intersects(double ra, double dec, double radius_deg,
                                   double tree_center_ra, double tree_center_dec,
                                   double x0, double y0, double x1, double y1) {
    if (radius_deg < 0.0) return 1;  /* 非法半径防御：不剪枝 */
    /* 查询中心到本树极点的余纬（度）；dec 越界做防御性归一化 */
    double theta_q = (tree_center_dec < 0.0) ? (90.0 + dec) : (90.0 - dec);
    if (theta_q < 0.0) theta_q = -theta_q;
    if (theta_q > 180.0) theta_q = 360.0 - theta_q;
    if (theta_q > 90.0) theta_q = 180.0 - theta_q;

    /* 查询中心在极冠外：cone 完全在冠外 → 整树必不相交（安全拒绝） */
    if (theta_q > AE_CAP_BOUNDARY_DEG + radius_deg) return 0;
    /* 查询中心在极冠外但 cone 可能跨过 ±45° 边界 → 保守不剪枝 */
    if (theta_q > AE_CAP_BOUNDARY_DEG) return 1;

    /* cone 可能跨过 90° 余纬（另一半球）时 AE 双-Lipschitz 常数失效 → 不剪枝 */
    if (theta_q + radius_deg > 90.0) return 1;

    double phi_q = (ra - tree_center_ra) * DEG2RAD;
    /* 归一化 phi 到 [-pi, pi]，避免大角度 sin/cos 精度损失 */
    while (phi_q > M_PI) phi_q -= 2.0 * M_PI;
    while (phi_q < -M_PI) phi_q += 2.0 * M_PI;
    double xq = theta_q * sin(phi_q);
    double yq = theta_q * cos(phi_q);
    /* cone 完全位于 theta<=45° 子冠内用更紧常数 C45，否则用全局 pi/2 */
    double factor = (theta_q + radius_deg <= AE_CAP_BOUNDARY_DEG)
                        ? AE_C45_FACTOR : AE_GLOBAL_FACTOR;
    double rq = radius_deg * factor;
    double dx = 0.0, dy = 0.0;
    if (xq < x0) dx = x0 - xq; else if (xq > x1) dx = xq - x1;
    if (yq < y0) dy = y0 - yq; else if (yq > y1) dy = yq - y1;
    return (dx * dx + dy * dy <= rq * rq) ? 1 : 0;
}

static int lz4_decompress(const uint8_t *src, uint32_t src_size, uint8_t *dst, uint32_t dst_capacity) {
    const uint8_t *ip = src;
    const uint8_t *ip_end = src + src_size;
    uint8_t *op = dst;
    uint8_t *op_end = dst + dst_capacity;

    while (ip < ip_end && op < op_end) {
        uint8_t token = *ip++;
        uint32_t lit_len = (token >> 4) & 0x0F;
        uint32_t match_len = (token & 0x0F) + 4;

        if (lit_len == 15) {
            while (ip < ip_end) {
                uint8_t b = *ip++;
                lit_len += b;
                if (b != 255) break;
            }
        }
        if (op + lit_len > op_end || ip + lit_len > ip_end) return -1;
        memcpy(op, ip, lit_len);
        ip += lit_len;
        op += lit_len;

        if (ip >= ip_end) break;

        if (ip + 2 > ip_end) return -1;
        uint16_t offset = ip[0] | ((uint16_t)ip[1] << 8);
        ip += 2;
        if (offset == 0) return -1;

        if (match_len == 15 + 4) {
            while (ip < ip_end) {
                uint8_t b = *ip++;
                match_len += b;
                if (b != 255) break;
            }
        }
        const uint8_t *match = op - offset;
        if (match < dst) return -1;
        if (op + match_len > op_end) return -1;
        if (offset >= match_len) {
            memcpy(op, match, match_len);
            op += match_len;
        } else {
            for (uint32_t i = 0; i < match_len; i++)
                op[i] = match[i % offset];
            op += match_len;
        }
    }
    return (int)(op - dst);
}

/* B13-R13-5: 返回码化 — 0 成功; -1 分配失败。修复前 malloc 失败静默 return,
 * 调用方把未逆置换的数据当合法科学数据继续解析 (静默数据损坏)。 */
static int byte_unshuffle(uint8_t *data, size_t data_len, int item_size) {
    if (item_size <= 1 || data_len == 0) return 0;
    size_t n = data_len / item_size;
    if (n == 0) return 0;
    uint8_t *tmp = (uint8_t *)malloc(data_len);
    if (!tmp) return -1;
    for (int i = 0; i < item_size; i++) {
        size_t src_start = (size_t)i * n;
        for (size_t j = 0; j < n; j++) {
            tmp[j * item_size + i] = data[src_start + j];
        }
    }
    memcpy(data, tmp, data_len);
    free(tmp);
    return 0;
}

static uint32_t find_max_block_size(QTNode *nodes, int node_count) {
    uint32_t max_bs = 0;
    for (int i = 0; i < node_count; i++) {
        if (nodes[i].is_leaf && nodes[i].block_size > max_bs)
            max_bs = nodes[i].block_size;
    }
    return max_bs;
}

/* ===== 修改后的read_leaf_block: 优先查缓存 =====
 * B4-P1-2: 所有 block_cache 访问持 xf->bc_lock (见文件头注释)。
 * 插入成功返回缓存内权威副本; 插入失败返回线程私有 scratch。
 * 注: 持锁只消除缓存结构性竞争 (free/memcpy/哈希表写); 调用方拿到
 * 返回指针后的读取窗口内, 并发淘汰仍可能释放该块 —— 这是缓存所有权
 * 设计的固有语义, 与单写者时代一致 (命中返回的指针同样可被淘汰),
 * 不在本修复范围内放大或缩小。 */
static uint8_t *read_leaf_block(XPSDFileInternal *xf, uint64_t block_offset,
                                 uint32_t compressed_size, uint32_t block_size,
                                 uint8_t *scratch) {
    if (!xf->mmap_data || block_size == 0) return NULL;

    /* 1. 查解压块缓存 */
    uint32_t cached_size = 0;
    uint8_t *cached = block_cache_lookup(xf, block_offset, &cached_size);
    if (cached && cached_size == block_size) {
        return cached;  /* 缓存命中, 直接返回 */
    }

    /* 2. 缓存未命中, 执行解压 */
    const uint8_t *comp = xf->mmap_data + xf->data_position + block_offset;

    if (compressed_size == block_size) {
        memcpy(scratch, comp, block_size);
        if (xf->use_byte_shuffle && xf->item_size > 1) {
            if (byte_unshuffle(scratch, block_size, xf->item_size) != 0) {
                fprintf(stderr, "gaia_client: byte_unshuffle OOM (block_size=%u)\n",
                        block_size);
                return NULL;  /* B13-R13-5: 不把未逆置换数据当科学结果 */
            }
        }
    } else {
        int decompressed_size = -1;
        if (strstr(xf->compression, "lz4") != NULL) {
            decompressed_size = lz4_decompress(comp, compressed_size, scratch, block_size);
        } else if (strstr(xf->compression, "zlib") != NULL) {
            uLongf dest_len = block_size;
            int zret = uncompress(scratch, &dest_len, comp, compressed_size);
            if (zret == Z_OK && dest_len == block_size)
                decompressed_size = (int)dest_len;
        }

        if (decompressed_size < 0) return NULL;

        if (xf->use_byte_shuffle && xf->item_size > 1) {
            if (byte_unshuffle(scratch, block_size, xf->item_size) != 0) {
                fprintf(stderr, "gaia_client: byte_unshuffle OOM (block_size=%u)\n",
                        block_size);
                return NULL;  /* B13-R13-5: 不把未逆置换数据当科学结果 */
            }
        }
    }

    /* 3. 存入解压块缓存, 返回缓存内权威副本 (B4-P1-2: 锁内完成插入+取指针,
     *    消除"insert后并发重查"窗口; 修复前 re-lookup 可能拿到其他线程
     *    淘汰中的条目) */
    uint8_t *authoritative = block_cache_insert(xf, block_offset, scratch, block_size);
    if (authoritative) {
        return authoritative;
    }

    return scratch;  /* 回退: 返回scratch (缓存插入失败时) */
}

static void close_xpsd_file(XPSDFileInternal *xf);

/* M9-H-2: XPSD 文件自报字段严格解析——只接受十进制无符号整数且 ≤ limit;
 * 语法非法 (负号/非数字/尾随垃圾) 或越界一律返回 0。 */
static int parse_bounded_int(const char* s, int limit, int* out) {
    if (!s || *s < '0' || *s > '9') return 0;
    char* end = NULL;
    long v = strtol(s, &end, 10);
    if (end == s || (*end != '\0' && *end != ',')) return 0;
    if (v < 0 || v > (long)limit) return 0;
    *out = (int)v;
    return 1;
}

/* P19-gaia (RQS B2 / V5-N-03): XPSD 头 magnitudeRange 分量的有界解析。
 * 与 parse_bounded_int (M9-H-2 同批先例) 同款严格纪律, 作用于 double:
 *   ① 整 token 必须被完整消费 (strtod 后必须到 '\0', 拒 "12.5abc" 尾随垃圾);
 *   ② strtod 不得溢出/下溢 (errno==ERANGE, 拒 "1e999" -> ±inf);
 *   ③ isfinite (拒 nan / inf);
 *   ④ 落在 [lo, hi] 值域窗内。
 * 任一失败返回 0; 调用方据此放弃该 shard 的星等剪枝 (宁可不剪不可漏星)。 */
static int parse_bounded_double(const char *s, double lo, double hi, double *out) {
    if (!s || !*s) return 0;
    char *end = NULL;
    errno = 0;
    double v = strtod(s, &end);
    if (end == s || *end != '\0') return 0;   /* 非数字 / 尾随垃圾 */
    if (errno == ERANGE) return 0;             /* 上溢/下溢 (含 ±inf) */
    if (!isfinite(v)) return 0;                /* nan / inf */
    if (v < lo || v > hi) return 0;            /* 值域窗 */
    *out = v;
    return 1;
}

static int load_xpsd_file(XPSDFileInternal *xf, const char *path) {
    memset(xf, 0, sizeof(*xf));
    /* V18R3: 显式截断拷贝，避免 strncpy 截断告警且保证 null 终止 */
    snprintf(xf->filepath, sizeof(xf->filepath), "%s", path);

#ifdef _WIN32
    xf->hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (xf->hFile == INVALID_HANDLE_VALUE) {
        snprintf(xf->load_error, sizeof(xf->load_error),
                 "CreateFileA failed (GetLastError=%lu)", (unsigned long)GetLastError());
        return -1;
    }
    LARGE_INTEGER fsize;
    GetFileSizeEx(xf->hFile, &fsize);
    xf->mmap_size = (size_t)fsize.QuadPart;
    xf->hMap = CreateFileMappingA(xf->hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!xf->hMap) {
        snprintf(xf->load_error, sizeof(xf->load_error),
                 "CreateFileMappingA of %llu bytes failed (GetLastError=%lu)",
                 (unsigned long long)xf->mmap_size, (unsigned long)GetLastError());
        CloseHandle(xf->hFile);
        xf->hFile = INVALID_HANDLE_VALUE;   /* FAILCLOSED-01: 不留悬垂句柄 */
        return -1;
    }
    xf->mmap_data = (uint8_t *)MapViewOfFile(xf->hMap, FILE_MAP_READ, 0, 0, 0);
    if (!xf->mmap_data) {
        snprintf(xf->load_error, sizeof(xf->load_error),
                 "MapViewOfFile of %llu bytes failed (GetLastError=%lu; "
                 "address-space pressure?)",
                 (unsigned long long)xf->mmap_size, (unsigned long)GetLastError());
        CloseHandle(xf->hMap);
        CloseHandle(xf->hFile);
        xf->hMap = NULL;
        xf->hFile = INVALID_HANDLE_VALUE;   /* FAILCLOSED-01: 不留悬垂句柄 */
        return -1;
    }
#else
    xf->fd = open(path, O_RDONLY);
    if (xf->fd < 0) {
        snprintf(xf->load_error, sizeof(xf->load_error), "open failed: %s", strerror(errno));
        return -1;
    }
    struct stat st;
    if (fstat(xf->fd, &st) != 0) {
        snprintf(xf->load_error, sizeof(xf->load_error), "fstat failed: %s", strerror(errno));
        close(xf->fd);
        xf->fd = -1;
        return -1;
    }
    xf->mmap_size = st.st_size;
    xf->mmap_data = mmap(NULL, xf->mmap_size, PROT_READ, MAP_PRIVATE, xf->fd, 0);
    if (xf->mmap_data == MAP_FAILED) {
        xf->mmap_data = NULL;
        snprintf(xf->load_error, sizeof(xf->load_error),
                 "mmap of %llu bytes failed: %s (address-space / RLIMIT_AS pressure?)",
                 (unsigned long long)xf->mmap_size, strerror(errno));
        close(xf->fd);
        xf->fd = -1;
        return -1;
    }
#endif

    /* FAILCLOSED-01: 此前的失败路径既未 munmap 也未 close(fd) ⇒ 目录里一个坏
     * 文件就泄漏 1 个 fd + 最多数 GB 地址空间（加剧本缺陷）。改为统一走
     * close_xpsd_file（此处 bc_lock 尚未初始化, bc_lock_ok=0, 释放是安全的）。 */
    if (xf->mmap_size < 16 || memcmp(xf->mmap_data, XPSD_MAGIC, 8) != 0) {
        snprintf(xf->load_error, sizeof(xf->load_error),
                 "not an XPSD file: size=%llu bytes, magic mismatch (expected \"" XPSD_MAGIC "\")",
                 (unsigned long long)xf->mmap_size);
        close_xpsd_file(xf);
        return -1;
    }

    uint32_t header_len;
    memcpy(&header_len, xf->mmap_data + 8, 4);

    const char *xml = (const char *)(xf->mmap_data + 16);

    const char *data_tag = find_tag(xml, "Data");
    if (data_tag) {
        /* P19-gaia (RQS 行动单 B2 / V5-N-03): 修复前以裸 atof 解析
         * magnitudeRange 并仅校验"XML 逗号存在", 畸形声明 (low>high 颠倒 /
         * nan / 1e999->inf / 空串 / 超值域 / 尾随垃圾) 会经 :2054 成为**整
         * 文件 (shard) 剪枝谓词**⇒ 整 shard 静默漏星。现改为 parse_bounded_
         * double 有界解析; **任一校验失败不置 has_magnitude_range** (放弃该
         * shard 剪枝, 宁可不剪不可漏星) + 可见告警 + 计数。 */
        char mags[64];
        int mag_present = parse_attr_after(data_tag, "magnitudeRange", mags, sizeof(mags));
        if (mag_present) {
            char raw[64];
            snprintf(raw, sizeof(raw), "%s", mags);
            char *comma = strchr(mags, ',');
            double lo = 0.0, hi = 0.0;
            int ok = 0;
            if (comma) {
                *comma = '\0';
                ok = parse_bounded_double(mags, GAIA_MAG_RANGE_MIN, GAIA_MAG_RANGE_MAX, &lo) &&
                     parse_bounded_double(comma + 1, GAIA_MAG_RANGE_MIN, GAIA_MAG_RANGE_MAX, &hi) &&
                     lo <= hi;
            }
            if (ok) {
                xf->magnitude_low = lo;
                xf->magnitude_high = hi;
                xf->has_magnitude_range = 1;  /* G1: 声明有效才允许按星等剪枝 */
            } else {
                xf->magnitude_range_invalid = 1;  /* 计数在 client 侧聚合 */
                fprintf(stderr,
                        "gaia_client: WARNING magnitudeRange declaration rejected "
                        "(file=%s decl=\"%s\"): shard-level magnitude pruning "
                        "disabled for this file (conservative: never silently drop "
                        "stars)\n",
                        xf->filepath, raw);
            }
        }
        char pos_str[64];
        parse_attr_after(data_tag, "position", pos_str, sizeof(pos_str));
        xf->data_position = pos_str[0] ? strtoull(pos_str, NULL, 10) : 0;
        parse_attr_after(data_tag, "compression", xf->compression, sizeof(xf->compression));
        xf->use_byte_shuffle = (strstr(xf->compression, "+sh") != NULL);
        char item_sz[32];
        parse_attr_after(data_tag, "itemSize", item_sz, sizeof(item_sz));
        xf->item_size = item_sz[0] ? atoi(item_sz) : 0;
        char params_str[256];
        parse_attr_after(data_tag, "parameters", params_str, sizeof(params_str));
        xf->spectrum_start = 0;
        xf->spectrum_step = 0;
        xf->spectrum_count = 0;
        xf->spectrum_bits = 0;
        if (params_str[0]) {
            const char *p = params_str;
            while (*p) {
                if (strncmp(p, "spectrumStart=", 14) == 0) { xf->spectrum_start = atoi(p + 14); }
                else if (strncmp(p, "spectrumStep=", 13) == 0) { xf->spectrum_step = atoi(p + 13); }
                else if (strncmp(p, "spectrumCount=", 14) == 0) {
                    /* M9-H-2: 自报计数不得用作无界分配步长/memcpy 长度。
                     * 非数字/负数/超 WL_COUNT → 整文件拒绝 (不放大分配)。 */
                    if (!parse_bounded_int(p + 14, WL_COUNT, &xf->spectrum_count)) {
                        snprintf(xf->load_error, sizeof(xf->load_error),
                                 "invalid spectrumCount declaration (\"%.32s\")",
                                 p + 14);
                        close_xpsd_file(xf);
                        return -1;
                    }
                }
                else if (strncmp(p, "spectrumBits=", 13) == 0) { xf->spectrum_bits = atoi(p + 13); }
                p = strchr(p, ',');
                if (!p) break;
                p++;
            }
        }
    }

    const char *stats_tag = find_tag(xml, "Statistics");
    if (stats_tag)
        xf->total_sources = parse_int_after(stats_tag, "totalSources", 0);

    extract_tag_text(xml, "DatabaseIdentifier", xf->db_identifier, sizeof(xf->db_identifier));
    xf->has_spectrum = (strstr(xf->db_identifier, "GaiaDR3SP") != NULL);
    /* M9-H-2: 光谱文件自报计数必须落在 [1, WL_COUNT]。上限=Gaia DR3 固定光谱
     * 网格 343 (DATA_SEMANTICS spectrum_wl), 同时保证 spec_collector_push 每星
     * memcpy 不超过记录光谱起点 (p+40) 之后 344 字节可读区 (STAR_STRIDE_SP=384);
     * 0/缺失同样视为损坏并整文件拒绝 (修复前四种"==0 才兜底"会放行畸形文件)。 */
    if (xf->has_spectrum &&
        (xf->spectrum_count < 1 || xf->spectrum_count > WL_COUNT)) {
        snprintf(xf->load_error, sizeof(xf->load_error),
                 "spectrumCount=%d out of range [1,%d] for spectrum file",
                 xf->spectrum_count, WL_COUNT);
        close_xpsd_file(xf);
        return -1;
    }
    xf->star_stride = xf->has_spectrum ? STAR_STRIDE_SP : STAR_STRIDE_NOSP;

    const char *tree_search = xml;
    xf->tree_count = 0;
    xf->global_max_block_size = 0;
    while (xf->tree_count < 16) {
        const char *tree_tag = find_tag(tree_search, "Tree");
        if (!tree_tag) break;

        TreeInfo *ti = &xf->trees[xf->tree_count];
        parse_attr_after(tree_tag, "projection", ti->projection, sizeof(ti->projection));
        char center[64];
        parse_attr_after(tree_tag, "center", center, sizeof(center));
        if (center[0]) {
            char *comma = strchr(center, ',');
            if (comma) { *comma = '\0'; ti->center_ra = atof(center); ti->center_dec = atof(comma + 1); }
        }
        int root_pos = parse_int_after(tree_tag, "rootPosition", 0);
        int node_count = parse_int_after(tree_tag, "nodeCount", 0);
        ti->node_count = node_count;

        if (node_count > 0 && root_pos > 0) {
            ti->nodes = (QTNode *)malloc(node_count * sizeof(QTNode));
            if (!ti->nodes) {
                /* FAILCLOSED-01: 修复前为 break —— 该树被静默丢弃而 load 仍返回 0
                 * （"装载成功"却少了整棵树 = 静默部分星表）。改为显式失败。 */
                snprintf(xf->load_error, sizeof(xf->load_error),
                         "QTNode array alloc failed (tree=%d node_count=%d)",
                         xf->tree_count, node_count);
                close_xpsd_file(xf);
                return -1;
            }
            const uint8_t *node_data = xf->mmap_data + root_pos;
            for (int i = 0; i < node_count; i++) {
                QTNode *n = &ti->nodes[i];
                const uint8_t *p = node_data + i * 48;
                memcpy(&n->x0, p, 8);
                memcpy(&n->y0, p + 8, 8);
                memcpy(&n->x1, p + 16, 8);
                memcpy(&n->y1, p + 24, 8);
                uint64_t bo_raw;
                memcpy(&bo_raw, p + 32, 8);
                n->is_leaf = (bo_raw & 0x8000000000000000ULL) != 0;
                if (n->is_leaf) {
                    n->block_offset = bo_raw & 0x7FFFFFFFFFFFFFFFULL;
                    memcpy(&n->block_size, p + 40, 4);
                    memcpy(&n->compressed_size, p + 44, 4);
                    n->child_nw = n->child_ne = n->child_sw = n->child_se = 0;
                } else {
                    n->block_offset = 0;
                    n->block_size = 0;
                    n->compressed_size = 0;
                    memcpy(&n->child_nw, p + 32, 4);
                    memcpy(&n->child_ne, p + 36, 4);
                    memcpy(&n->child_sw, p + 40, 4);
                    memcpy(&n->child_se, p + 44, 4);
                }
            }
            ti->max_block_size = find_max_block_size(ti->nodes, node_count);
            if (ti->max_block_size > xf->global_max_block_size)
                xf->global_max_block_size = ti->max_block_size;
        }
        xf->tree_count++;
        tree_search = tree_tag + 4;
    }

    /* 初始化解压块缓存 (B4-P1-2: Win 侧锁初始化无失败语义) */
    block_cache_init(&xf->block_cache);
#ifdef _WIN32
    bc_lock_init(&xf->bc_lock);
    xf->bc_lock_ok = 1;
#else
    xf->bc_lock_ok = (bc_lock_init(&xf->bc_lock) == 0);
#endif

    return 0;
}

static void close_xpsd_file(XPSDFileInternal *xf) {
    /* 释放解压块缓存 (B4-P1-2: 先锁再释放, 期间无其他线程可访问;
     * OpenMP 区结束即隐式 barrier, 所有工作线程已汇合) */
    if (xf->bc_lock_ok) bc_lock_acquire(&xf->bc_lock);
    block_cache_free(&xf->block_cache, xf->budget);
    if (xf->bc_lock_ok) bc_lock_release(&xf->bc_lock);
    if (xf->bc_lock_ok) bc_lock_destroy(&xf->bc_lock);
    xf->bc_lock_ok = 0;

    for (int t = 0; t < xf->tree_count; t++) {
        if (xf->trees[t].nodes) free(xf->trees[t].nodes);
    }
    if (xf->mmap_data) {
#ifdef _WIN32
        UnmapViewOfFile(xf->mmap_data);
        if (xf->hMap) CloseHandle(xf->hMap);
        if (xf->hFile != INVALID_HANDLE_VALUE) CloseHandle(xf->hFile);
#else
        munmap(xf->mmap_data, xf->mmap_size);
        close(xf->fd);
#endif
        xf->mmap_data = NULL;
    }
}

static void collector_init(StarCollector *sc, int initial_cap) {
    sc->stars = (SimpleStar *)malloc(initial_cap * sizeof(SimpleStar));
    sc->count = 0;
    sc->capacity = sc->stars ? initial_cap : 0;
    drop_ledger_init(&sc->drop);   /* FAILCLOSED-01 */
}

static void collector_push(StarCollector *sc, double ra, double dec, double magG) {
    if (sc->count >= sc->capacity) {
        int new_cap = sc->capacity * 2;
        if (new_cap == 0) new_cap = 16;  /* V18R3: init 分配失败后自愈 */
        SimpleStar *new_stars = (SimpleStar *)realloc(sc->stars, new_cap * sizeof(SimpleStar));
        if (!new_stars) {
            /* FAILCLOSED-01: 修复前为静默 return —— 单星被丢弃、查询照常返回
             * 0 与"看起来正常"的更短星表。现记账 + 告警 + 查询 fail-closed。 */
            drop_ledger_note(&sc->drop, 1, 0,
                             "star collector realloc failed (OOM): 1 star dropped");
            return;
        }
        sc->stars = new_stars;
        sc->capacity = new_cap;
    }
    sc->stars[sc->count].ra = ra;
    sc->stars[sc->count].dec = dec;
    sc->stars[sc->count].magG = magG;
    sc->count++;
}

static void collector_free(StarCollector *sc) {
    if (sc->stars) free(sc->stars);
    sc->stars = NULL;
    sc->count = sc->capacity = 0;
}

static void spec_collector_init(SpectrumStarCollector *sc, int capacity, int spectrum_count) {
    drop_ledger_init(&sc->drop);   /* FAILCLOSED-01 */
    sc->stars = (SpectrumStar *)malloc(capacity * sizeof(SpectrumStar));
    if (spectrum_count > 0) {
        sc->spectra = (uint8_t *)malloc((size_t)capacity * spectrum_count);
    } else {
        sc->spectra = NULL;
    }
    sc->count = 0;
    sc->spectrum_count = spectrum_count;
    /* W1-GAIA-001: count==0 means empty ownership -> spectra=NULL, no malloc(0) dependency */
    if (spectrum_count == 0) {
        sc->capacity = sc->stars ? capacity : 0;
    } else {
        sc->capacity = (sc->stars && sc->spectra) ? capacity : 0;
        if (sc->capacity == 0) {
            free(sc->stars); sc->stars = NULL;
            free(sc->spectra); sc->spectra = NULL;
        }
    }
}

static void spec_collector_free(SpectrumStarCollector *sc) {
    free(sc->stars);
    free(sc->spectra);
    sc->stars = NULL;
    sc->spectra = NULL;
    sc->count = 0;
}

static void spec_collector_push(SpectrumStarCollector *sc, double ra, double dec,
                                 double magG, float flux_min, float flux_mul,
                                 const uint8_t *spectrum) {
    if (sc->count >= sc->capacity) {
        int new_cap = sc->capacity * 2;
        if (new_cap == 0) new_cap = 16;
        /* B4-P1-3: realloc 返回值是旧 stars 缓冲的唯一引用。旧指针在 realloc
         * 调用后立即失效, 必须马上提交到 sc->stars (单一所有权)。修复前:
         * spectra realloc 失败路径 free(new_stars) —— 此时 new_stars 是唯一
         * 存活引用, 而 sc->stars 已悬垂, 后续 spec_collector_free(sc->stars)
         * = double free。修复后: 失败路径不 free 任何块, 仅暂不提升 capacity
         * (stars 多占一倍内存, 由 spec_collector_free 统一释放, 无泄漏无重放) */
        SpectrumStar *new_stars = (SpectrumStar *)realloc(sc->stars, (size_t)new_cap * sizeof(SpectrumStar));
        if (!new_stars) {  /* realloc 失败: 旧块未动, sc->stars 仍有效 */
            drop_ledger_note(&sc->drop, 1, 0,
                             "spectrum star collector realloc failed (OOM): 1 star dropped");
            return;
        }
        sc->stars = new_stars;   /* 立即转移所有权 */
        if (sc->spectrum_count > 0) {
            uint8_t *new_spectra = (uint8_t *)realloc(sc->spectra, (size_t)new_cap * sc->spectrum_count);
            if (!new_spectra) {
                /* spectra 扩容失败: sc->spectra 仍指旧有效块, capacity 不提升,
                 * 本条不入队; 下次 push 自动重试扩容。
                 * FAILCLOSED-01: 该条星（含光谱）不得静默丢弃 ⇒ 记账 + 告警。 */
                drop_ledger_note(&sc->drop, 1, 0,
                                 "spectrum buffer realloc failed (OOM): 1 star dropped");
                return;
            }
            sc->spectra = new_spectra;
        }
        sc->capacity = new_cap;
    }
    sc->stars[sc->count].ra = ra;
    sc->stars[sc->count].dec = dec;
    sc->stars[sc->count].magG = magG;
    sc->stars[sc->count].flux_min = flux_min;
    sc->stars[sc->count].flux_mul = flux_mul;
    if (spectrum && sc->spectrum_count > 0)
        memcpy(sc->spectra + (size_t)sc->count * sc->spectrum_count, spectrum, sc->spectrum_count);
    sc->count++;
}

static void phot_collector_init(PhotometryStarCollector *pc, int capacity) {
    pc->stars = (PhotometryStar *)malloc(capacity * sizeof(PhotometryStar));
    pc->count = 0;
    pc->capacity = pc->stars ? capacity : 0;  /* V18R3: 分配失败即 0 容量 */
    drop_ledger_init(&pc->drop);   /* FAILCLOSED-01 */
}

static void phot_collector_free(PhotometryStarCollector *pc) {
    free(pc->stars);
    pc->stars = NULL;
    pc->count = 0;
    pc->capacity = 0;
}

static void phot_collector_push(PhotometryStarCollector *pc, double ra, double dec,
                                 double magG, double magBP, double magRP) {
    if (pc->count >= pc->capacity) {
        int new_cap = pc->capacity * 2;
        if (new_cap == 0) new_cap = 16;  /* V18R3: init 分配失败后自愈 */
        PhotometryStar *new_stars = (PhotometryStar *)realloc(pc->stars, new_cap * sizeof(PhotometryStar));
        if (!new_stars) {
            drop_ledger_note(&pc->drop, 1, 0,
                             "photometry collector realloc failed (OOM): 1 star dropped");
            return;
        }
        pc->stars = new_stars;
        pc->capacity = new_cap;
    }
    pc->stars[pc->count].ra = ra;
    pc->stars[pc->count].dec = dec;
    pc->stars[pc->count].magG = magG;
    pc->stars[pc->count].magBP = magBP;
    pc->stars[pc->count].magRP = magRP;
    pc->count++;
}

static void search_recursive(XPSDFileInternal *xf, TreeInfo *tree, int node_idx,
                              double ra, double dec, double radius_deg,
                              double mag_low, double mag_high,
                              double cos_ra_q, double sin_ra_q,
                              double cos_dec_q, double sin_dec_q,
                              double cos_radius,
                              StarCollector *sc, uint8_t *scratch,
                              GaiaTraceCtx *trace) {
    if (sc->count >= MAX_STARS_RESULT) return;
    QTNode *node = &tree->nodes[node_idx];
    if (trace && trace->enabled) {
        #pragma omp atomic
        trace->nodes++;
        if (strcmp(tree->projection, "AzimuthalEquidistant") == 0) {
            #pragma omp atomic
            trace->polar_nodes++;
        } else {
            #pragma omp atomic
            trace->eq_nodes++;
        }
    }

    double ra_min, ra_max, dec_min, dec_max;
    if (strcmp(tree->projection, "Equirectangular") == 0) {
        ra_min = node->x0 + tree->center_ra;
        ra_max = node->x1 + tree->center_ra;
        dec_min = node->y0;
        dec_max = node->y1;
    } else {
        double corners_ra[5], corners_dec[5];
        double xs[5] = {node->x0, node->x1, node->x1, node->x0, 0.0};
        double ys[5] = {node->y0, node->y0, node->y1, node->y1, 0.0};
        for (int i = 0; i < 5; i++)
            unproject(xs[i], ys[i], tree->projection, tree->center_ra, tree->center_dec,
                       &corners_ra[i], &corners_dec[i]);
        ra_min = corners_ra[0]; ra_max = corners_ra[0];
        dec_min = corners_dec[0]; dec_max = corners_dec[0];
        for (int i = 1; i < 5; i++) {
            if (corners_ra[i] < ra_min) ra_min = corners_ra[i];
            if (corners_ra[i] > ra_max) ra_max = corners_ra[i];
            if (corners_dec[i] < dec_min) dec_min = corners_dec[i];
            if (corners_dec[i] > dec_max) dec_max = corners_dec[i];
        }
    }

    if (strcmp(tree->projection, "AzimuthalEquidistant") == 0) {
        /* V18R2: 极投影平面剪枝（替代天球 bbox——极区 RA 环绕使
         * min/max 退化导致全树遍历，单次查询误读 16GB mmap 数据） */
        if (!POLAR_PRUNE_INTERSECTS(ra, dec, radius_deg,
                                    tree->center_ra, tree->center_dec,
                                    node->x0, node->y0, node->x1, node->y1))
            return;
    } else if (!bbox_intersects(ra, dec, radius_deg, ra_min, ra_max, dec_min, dec_max)) {
        return;
    }

    if (node->is_leaf) {
        if (trace && trace->enabled) {
            #pragma omp atomic
            trace->blocks++;
            #pragma omp atomic
            trace->bytes += (long long)node->compressed_size;
            if (node->compressed_size != node->block_size) {
                #pragma omp atomic
                trace->decomp++;
            }
        }
        uint8_t *data = read_leaf_block(xf, node->block_offset, node->compressed_size,
                                         node->block_size, scratch);
        if (!data) {
            /* FAILCLOSED-01: 修复前为静默 return —— 整叶星被丢弃而查询照常返回
             * 0 与"看起来正常"的更短星表。现记账 + 告警 + 查询 fail-closed。 */
            drop_ledger_note(&sc->drop, 0, 1,
                             "leaf block decode failed (corrupt block / OOM): 1 leaf dropped");
            return;
        }

        int n = node->block_size / xf->star_stride;
        int stride = xf->star_stride;
        int is_eq = (strcmp(tree->projection, "Equirectangular") == 0);
        double inv_scale = 1.0 / (3600.0 * 1000.0 * 500.0);
        double inv_dra = 1.0 / (3600.0 * 1000.0 * 100.0);

        for (int i = 0; i < n && sc->count < MAX_STARS_RESULT; i++) {
            const uint8_t *p = data + i * stride;
            uint32_t dx, dy;
            memcpy(&dx, p, 4);
            memcpy(&dy, p + 4, 4);
            uint16_t mag_raw;
            memcpy(&mag_raw, p + 20, 2);
            double magG = mag_raw * 0.001 - 1.5;
            if (magG < mag_low || magG > mag_high) continue;

            double x = node->x0 + dx * inv_scale;
            double y = node->y0 + dy * inv_scale;
            double s_ra, s_dec;
            if (is_eq) {
                s_ra = x + tree->center_ra;
                s_dec = y;
            } else {
                unproject(x, y, tree->projection, tree->center_ra, tree->center_dec, &s_ra, &s_dec);
            }

            int16_t dra_raw;
            memcpy(&dra_raw, p + 26, 2);
            if (dra_raw != 0)
                s_ra += dra_raw * inv_dra;
            if (s_ra < 0) s_ra += 360.0;
            if (s_ra >= 360.0) s_ra -= 360.0;

            double cos_dec_s = cos(s_dec * DEG2RAD);
            double d_ang = cos_dec_q * cos_dec_s *
                          (cos_ra_q * cos(s_ra * DEG2RAD) + sin_ra_q * sin(s_ra * DEG2RAD))
                         + sin_dec_q * sin(s_dec * DEG2RAD);
            if (d_ang > 1.0) d_ang = 1.0;
            if (d_ang < -1.0) d_ang = -1.0;
            if (acos(d_ang) <= radius_deg * DEG2RAD)
                collector_push(sc, s_ra, s_dec, magG);
        }
    } else {
        if (node->child_nw) search_recursive(xf, tree, node->child_nw, ra, dec, radius_deg,
                                               mag_low, mag_high, cos_ra_q, sin_ra_q,
                                               cos_dec_q, sin_dec_q, cos_radius, sc, scratch, trace);
        if (node->child_ne) search_recursive(xf, tree, node->child_ne, ra, dec, radius_deg,
                                               mag_low, mag_high, cos_ra_q, sin_ra_q,
                                               cos_dec_q, sin_dec_q, cos_radius, sc, scratch, trace);
        if (node->child_sw) search_recursive(xf, tree, node->child_sw, ra, dec, radius_deg,
                                               mag_low, mag_high, cos_ra_q, sin_ra_q,
                                               cos_dec_q, sin_dec_q, cos_radius, sc, scratch, trace);
        if (node->child_se) search_recursive(xf, tree, node->child_se, ra, dec, radius_deg,
                                               mag_low, mag_high, cos_ra_q, sin_ra_q,
                                               cos_dec_q, sin_dec_q, cos_radius, sc, scratch, trace);
    }
}

static void search_recursive_spectrum(XPSDFileInternal *xf, TreeInfo *tree, uint32_t node_idx,
                              double ra, double dec, double radius_deg,
                              double mag_low, double mag_high,
                              double cos_ra_q, double sin_ra_q,
                              double cos_dec_q, double sin_dec_q,
                              double cos_radius,
                              SpectrumStarCollector *sc, uint8_t *scratch,
                              GaiaTraceCtx *trace) {
    if (sc->count >= MAX_STARS_RESULT) return;
    QTNode *node = &tree->nodes[node_idx];
    if (trace && trace->enabled) {
        #pragma omp atomic
        trace->nodes++;
        if (strcmp(tree->projection, "AzimuthalEquidistant") == 0) {
            #pragma omp atomic
            trace->polar_nodes++;
        } else {
            #pragma omp atomic
            trace->eq_nodes++;
        }
    }

    double ra_min, ra_max, dec_min, dec_max;
    if (strcmp(tree->projection, "Equirectangular") == 0) {
        ra_min = node->x0 + tree->center_ra;
        ra_max = node->x1 + tree->center_ra;
        dec_min = node->y0;
        dec_max = node->y1;
    } else {
        double corners_ra[5], corners_dec[5];
        double xs[5] = {node->x0, node->x1, node->x1, node->x0, 0.0};
        double ys[5] = {node->y0, node->y0, node->y1, node->y1, 0.0};
        for (int i = 0; i < 5; i++)
            unproject(xs[i], ys[i], tree->projection, tree->center_ra, tree->center_dec,
                       &corners_ra[i], &corners_dec[i]);
        ra_min = corners_ra[0]; ra_max = corners_ra[0];
        dec_min = corners_dec[0]; dec_max = corners_dec[0];
        for (int i = 1; i < 5; i++) {
            if (corners_ra[i] < ra_min) ra_min = corners_ra[i];
            if (corners_ra[i] > ra_max) ra_max = corners_ra[i];
            if (corners_dec[i] < dec_min) dec_min = corners_dec[i];
            if (corners_dec[i] > dec_max) dec_max = corners_dec[i];
        }
    }

    if (strcmp(tree->projection, "AzimuthalEquidistant") == 0) {
        /* V18R2: 极投影平面剪枝（与 star 版一致；spectrum 查询同样因
         * 极区 RA 环绕退化而全树遍历——PHOTOMETRIC 17.8s/36GB 元凶） */
        if (!POLAR_PRUNE_INTERSECTS(ra, dec, radius_deg,
                                    tree->center_ra, tree->center_dec,
                                    node->x0, node->y0, node->x1, node->y1))
            return;
    } else if (!bbox_intersects(ra, dec, radius_deg, ra_min, ra_max, dec_min, dec_max)) {
        return;
    }

    if (node->is_leaf) {
        if (trace && trace->enabled) {
            #pragma omp atomic
            trace->blocks++;
            #pragma omp atomic
            trace->bytes += (long long)node->compressed_size;
            if (node->compressed_size != node->block_size) {
                #pragma omp atomic
                trace->decomp++;
            }
        }
        uint8_t *data = read_leaf_block(xf, node->block_offset, node->compressed_size,
                                         node->block_size, scratch);
        if (!data) {
            /* FAILCLOSED-01: 修复前为静默 return —— 整叶星被丢弃而查询照常返回
             * 0 与"看起来正常"的更短星表。现记账 + 告警 + 查询 fail-closed。 */
            drop_ledger_note(&sc->drop, 0, 1,
                             "leaf block decode failed (corrupt block / OOM): 1 leaf dropped");
            return;
        }

        int n = node->block_size / xf->star_stride;
        int stride = xf->star_stride;
        int is_eq = (strcmp(tree->projection, "Equirectangular") == 0);
        double inv_scale = 1.0 / (3600.0 * 1000.0 * 500.0);
        double inv_dra = 1.0 / (3600.0 * 1000.0 * 100.0);

        for (int i = 0; i < n && sc->count < MAX_STARS_RESULT; i++) {
            const uint8_t *p = data + i * stride;
            uint32_t dx, dy;
            memcpy(&dx, p, 4);
            memcpy(&dy, p + 4, 4);
            uint16_t mag_raw;
            memcpy(&mag_raw, p + 20, 2);
            double magG = mag_raw * 0.001 - 1.5;
            if (magG < mag_low || magG > mag_high) continue;

            double x = node->x0 + dx * inv_scale;
            double y = node->y0 + dy * inv_scale;
            double s_ra, s_dec;
            if (is_eq) {
                s_ra = x + tree->center_ra;
                s_dec = y;
            } else {
                unproject(x, y, tree->projection, tree->center_ra, tree->center_dec, &s_ra, &s_dec);
            }

            int16_t dra_raw;
            memcpy(&dra_raw, p + 26, 2);
            if (dra_raw != 0)
                s_ra += dra_raw * inv_dra;
            if (s_ra < 0) s_ra += 360.0;
            if (s_ra >= 360.0) s_ra -= 360.0;

            double cos_dec_s = cos(s_dec * DEG2RAD);
            double d_ang = cos_dec_q * cos_dec_s *
                          (cos_ra_q * cos(s_ra * DEG2RAD) + sin_ra_q * sin(s_ra * DEG2RAD))
                         + sin_dec_q * sin(s_dec * DEG2RAD);
            if (d_ang > 1.0) d_ang = 1.0;
            if (d_ang < -1.0) d_ang = -1.0;
            if (acos(d_ang) <= radius_deg * DEG2RAD) {
                const uint8_t *spectrum = NULL;
                float flux_min = 0.0f, flux_mul = 0.0f;
                if (xf->has_spectrum) {
                    /* PCL GaiaDatabaseFile::EncodedStarSPData:
                     *   32B EncodedStarData | float fluxMin | float fluxMul | uint8 flux[...]
                     * flux[j] = byte*fluxMul + fluxMin (W*m^-2*nm^-1) */
                    memcpy(&flux_min, p + 32, 4);
                    memcpy(&flux_mul, p + 36, 4);
                    spectrum = p + 40;
                }
                spec_collector_push(sc, s_ra, s_dec, magG, flux_min, flux_mul, spectrum);
            }
        }
    } else {
        if (node->child_nw) search_recursive_spectrum(xf, tree, node->child_nw, ra, dec, radius_deg,
                                               mag_low, mag_high, cos_ra_q, sin_ra_q,
                                               cos_dec_q, sin_dec_q, cos_radius, sc, scratch, trace);
        if (node->child_ne) search_recursive_spectrum(xf, tree, node->child_ne, ra, dec, radius_deg,
                                               mag_low, mag_high, cos_ra_q, sin_ra_q,
                                               cos_dec_q, sin_dec_q, cos_radius, sc, scratch, trace);
        if (node->child_sw) search_recursive_spectrum(xf, tree, node->child_sw, ra, dec, radius_deg,
                                               mag_low, mag_high, cos_ra_q, sin_ra_q,
                                               cos_dec_q, sin_dec_q, cos_radius, sc, scratch, trace);
        if (node->child_se) search_recursive_spectrum(xf, tree, node->child_se, ra, dec, radius_deg,
                                               mag_low, mag_high, cos_ra_q, sin_ra_q,
                                               cos_dec_q, sin_dec_q, cos_radius, sc, scratch, trace);
    }
}

static void search_recursive_photometry(XPSDFileInternal *xf, TreeInfo *tree, uint32_t node_idx,
                              double ra, double dec, double radius_deg,
                              double mag_low, double mag_high,
                              double cos_ra_q, double sin_ra_q,
                              double cos_dec_q, double sin_dec_q,
                              double cos_radius,
                              PhotometryStarCollector *pc, uint8_t *scratch,
                              GaiaTraceCtx *trace) {
    if (pc->count >= MAX_STARS_RESULT) return;
    QTNode *node = &tree->nodes[node_idx];
    if (trace && trace->enabled) {
        #pragma omp atomic
        trace->nodes++;
        if (strcmp(tree->projection, "AzimuthalEquidistant") == 0) {
            #pragma omp atomic
            trace->polar_nodes++;
        } else {
            #pragma omp atomic
            trace->eq_nodes++;
        }
    }

    double ra_min, ra_max, dec_min, dec_max;
    if (strcmp(tree->projection, "Equirectangular") == 0) {
        ra_min = node->x0 + tree->center_ra;
        ra_max = node->x1 + tree->center_ra;
        dec_min = node->y0;
        dec_max = node->y1;
    } else {
        double corners_ra[5], corners_dec[5];
        double xs[5] = {node->x0, node->x1, node->x1, node->x0, 0.0};
        double ys[5] = {node->y0, node->y0, node->y1, node->y1, 0.0};
        for (int i = 0; i < 5; i++)
            unproject(xs[i], ys[i], tree->projection, tree->center_ra, tree->center_dec,
                       &corners_ra[i], &corners_dec[i]);
        ra_min = corners_ra[0]; ra_max = corners_ra[0];
        dec_min = corners_dec[0]; dec_max = corners_dec[0];
        for (int i = 1; i < 5; i++) {
            if (corners_ra[i] < ra_min) ra_min = corners_ra[i];
            if (corners_ra[i] > ra_max) ra_max = corners_ra[i];
            if (corners_dec[i] < dec_min) dec_min = corners_dec[i];
            if (corners_dec[i] > dec_max) dec_max = corners_dec[i];
        }
    }

    if (strcmp(tree->projection, "AzimuthalEquidistant") == 0) {
        /* V18R3: 与 star/spectrum 版一致使用可证明保守的极投影平面剪枝 */
        if (!POLAR_PRUNE_INTERSECTS(ra, dec, radius_deg,
                                    tree->center_ra, tree->center_dec,
                                    node->x0, node->y0, node->x1, node->y1))
            return;
    } else if (!bbox_intersects(ra, dec, radius_deg, ra_min, ra_max, dec_min, dec_max)) {
        return;
    }

    if (node->is_leaf) {
        if (trace && trace->enabled) {
            #pragma omp atomic
            trace->blocks++;
            #pragma omp atomic
            trace->bytes += (long long)node->compressed_size;
            if (node->compressed_size != node->block_size) {
                #pragma omp atomic
                trace->decomp++;
            }
        }
        uint8_t *data = read_leaf_block(xf, node->block_offset, node->compressed_size,
                                         node->block_size, scratch);
        if (!data) {
            /* FAILCLOSED-01: 修复前为静默 return —— 整叶星被丢弃而查询照常返回。 */
            drop_ledger_note(&pc->drop, 0, 1,
                             "leaf block decode failed (corrupt block / OOM): 1 leaf dropped");
            return;
        }

        int n = node->block_size / xf->star_stride;
        int stride = xf->star_stride;
        int is_eq = (strcmp(tree->projection, "Equirectangular") == 0);
        double inv_scale = 1.0 / (3600.0 * 1000.0 * 500.0);
        double inv_dra = 1.0 / (3600.0 * 1000.0 * 100.0);

        for (int i = 0; i < n && pc->count < MAX_STARS_RESULT; i++) {
            const uint8_t *p = data + i * stride;
            uint32_t dx, dy;
            memcpy(&dx, p, 4);
            memcpy(&dy, p + 4, 4);
            uint16_t mag_raw;
            memcpy(&mag_raw, p + 20, 2);
            double magG = mag_raw * 0.001 - 1.5;
            if (magG < mag_low || magG > mag_high) continue;

            double x = node->x0 + dx * inv_scale;
            double y = node->y0 + dy * inv_scale;
            double s_ra, s_dec;
            if (is_eq) {
                s_ra = x + tree->center_ra;
                s_dec = y;
            } else {
                unproject(x, y, tree->projection, tree->center_ra, tree->center_dec, &s_ra, &s_dec);
            }

            int16_t dra_raw;
            memcpy(&dra_raw, p + 26, 2);
            if (dra_raw != 0)
                s_ra += dra_raw * inv_dra;
            if (s_ra < 0) s_ra += 360.0;
            if (s_ra >= 360.0) s_ra -= 360.0;

            double cos_dec_s = cos(s_dec * DEG2RAD);
            double d_ang = cos_dec_q * cos_dec_s *
                          (cos_ra_q * cos(s_ra * DEG2RAD) + sin_ra_q * sin(s_ra * DEG2RAD))
                         + sin_dec_q * sin(s_dec * DEG2RAD);
            if (d_ang > 1.0) d_ang = 1.0;
            if (d_ang < -1.0) d_ang = -1.0;
            if (acos(d_ang) <= radius_deg * DEG2RAD) {
                double magBP = 0.0, magRP = 0.0;
                if (xf->has_spectrum) {
                    uint16_t magBP_raw, magRP_raw;
                    memcpy(&magBP_raw, p + 22, 2);
                    memcpy(&magRP_raw, p + 24, 2);
                    magBP = magBP_raw * 0.001 - 1.5;
                    magRP = magRP_raw * 0.001 - 1.5;
                }
                phot_collector_push(pc, s_ra, s_dec, magG, magBP, magRP);
            }
        }
    } else {
        if (node->child_nw) search_recursive_photometry(xf, tree, node->child_nw, ra, dec, radius_deg,
                                               mag_low, mag_high, cos_ra_q, sin_ra_q,
                                               cos_dec_q, sin_dec_q, cos_radius, pc, scratch, trace);
        if (node->child_ne) search_recursive_photometry(xf, tree, node->child_ne, ra, dec, radius_deg,
                                               mag_low, mag_high, cos_ra_q, sin_ra_q,
                                               cos_dec_q, sin_dec_q, cos_radius, pc, scratch, trace);
        if (node->child_sw) search_recursive_photometry(xf, tree, node->child_sw, ra, dec, radius_deg,
                                               mag_low, mag_high, cos_ra_q, sin_ra_q,
                                               cos_dec_q, sin_dec_q, cos_radius, pc, scratch, trace);
        if (node->child_se) search_recursive_photometry(xf, tree, node->child_se, ra, dec, radius_deg,
                                               mag_low, mag_high, cos_ra_q, sin_ra_q,
                                               cos_dec_q, sin_dec_q, cos_radius, pc, scratch, trace);
    }
}

static int file_matches_db_type(XPSDFileInternal *xf, GaiaDbType db_type) {
    if (db_type == GAIA_DB_AUTO) return 1;
    int is_dr3sp = (strstr(xf->db_identifier, "GaiaDR3SP") != NULL);
    int is_dr3 = (strstr(xf->db_identifier, "GaiaDR3") != NULL) && !is_dr3sp;
    if (db_type == GAIA_DB_DR3SP) return is_dr3sp;
    if (db_type == GAIA_DB_DR3) return is_dr3;
    return 0;
}

GaiaClient *gaia_client_create(const char *data_dir) {
    return gaia_client_create_ex(data_dir, GAIA_DB_AUTO);
}

GaiaClient *gaia_client_create_ex(const char *data_dir, GaiaDbType db_type) {
    GaiaClient *client = (GaiaClient *)calloc(1, sizeof(GaiaClient));
    if (!client) return NULL;
    client->db_type = db_type;
    client->db_type_detected = 0;

#ifdef _WIN32
    InitializeCriticalSection(&client->cache_lock);
#else
    pthread_mutex_init(&client->cache_lock, NULL);
#endif
    client->cache_lock_initialized = 1;

    /* 初始化查询结果缓存 */
    query_cache_init(&client->query_cache);

    /* G3b: 初始化客户端级解压块缓存总预算 */
    block_budget_init(&client->block_budget);

#ifdef _WIN32
    char pattern[1024];
    snprintf(pattern, sizeof(pattern), "%s\\*.xpsd", data_dir);
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(pattern, &fd);
    if (hFind == INVALID_HANDLE_VALUE) {
        create_diag_reset();
        g_create_diag.dir_open_failed = 1;
        create_diag_fail(data_dir, &g_create_diag, "directory not openable");
        fprintf(stderr, "gaia_client: FATAL %s\n", g_create_error);
        gaia_client_destroy(client);
        return NULL;
    }
    do {
        size_t len = strlen(fd.cFileName);
        if (len <= 5 || strcmp(fd.cFileName + len - 5, ".xpsd") != 0) continue;
        /* FAILCLOSED-01: 条目数与装载失败数**分开计数**（修复前只数成功装载的
         * 文件, 失败者无痕迹）。条目数含被 db_type 过滤者, 故恒有
         * file_count + db_type_skipped + fail_count <= file_entry_count。 */
        client->file_entry_count++;
        if (client->file_count >= MAX_FILES) {
            client->file_max_files_exceeded = 1;  /* 继续枚举以数全条目 */
            continue;
        }
        char fullpath[1024];
        snprintf(fullpath, sizeof(fullpath), "%s\\%s", data_dir, fd.cFileName);
        XPSDFileInternal *slot = &client->files[client->file_count];
        if (load_xpsd_file(slot, fullpath) == 0) {
            if (slot->magnitude_range_invalid)
                client->magnitude_range_reject_count++;  /* P19-gaia 可见计数 */
            if (file_matches_db_type(slot, db_type)) {
                client->file_count++;
            } else {
                close_xpsd_file(slot);
                client->file_db_type_skipped++;
            }
        } else {
            /* FAILCLOSED-01: 修复前**没有 else 分支** —— 装载失败的 shard 被
             * 静默丢弃（无日志/无计数/create 仍返回非 NULL）。 */
            client->file_load_fail_count++;
            if (client->file_load_fail_count == 1) {
                snprintf(client->first_load_fail_path,
                         sizeof(client->first_load_fail_path), "%s", fullpath);
                snprintf(client->first_load_fail_reason,
                         sizeof(client->first_load_fail_reason), "%s",
                         slot->load_error[0] ? slot->load_error : "(unknown)");
            }
            fprintf(stderr, "gaia_client: ERROR failed to load shard %s: %s\n",
                    fullpath, slot->load_error[0] ? slot->load_error : "(unknown)");
        }
    } while (FindNextFileA(hFind, &fd));
    FindClose(hFind);
#else
    DIR *dir = opendir(data_dir);
    if (!dir) {
        create_diag_reset();
        g_create_diag.dir_open_failed = 1;
        create_diag_fail(data_dir, &g_create_diag, "directory not openable");
        fprintf(stderr, "gaia_client: FATAL %s\n", g_create_error);
        gaia_client_destroy(client);
        return NULL;
    }
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        size_t len = strlen(ent->d_name);
        if (len <= 5 || strcmp(ent->d_name + len - 5, ".xpsd") != 0) continue;
        /* FAILCLOSED-01: 见 Windows 分支同款注释（条目数/失败数分开计数）。 */
        client->file_entry_count++;
        if (client->file_count >= MAX_FILES) {
            client->file_max_files_exceeded = 1;  /* 继续枚举以数全条目 */
            continue;
        }
        char fullpath[1024];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", data_dir, ent->d_name);
        XPSDFileInternal *slot = &client->files[client->file_count];
        if (load_xpsd_file(slot, fullpath) == 0) {
            if (slot->magnitude_range_invalid)
                client->magnitude_range_reject_count++;  /* P19-gaia 可见计数 */
            if (file_matches_db_type(slot, db_type)) {
                client->file_count++;
            } else {
                close_xpsd_file(slot);
                client->file_db_type_skipped++;
            }
        } else {
            /* FAILCLOSED-01: 修复前**没有 else 分支** —— 装载失败的 shard 被
             * 静默丢弃（无日志/无计数/create 仍返回非 NULL）；在 RLIMIT_AS 受限
             * 时丢掉的恰是唯一含亮星的 shard ⇒ 参考星表静默变成暗 shard 子集。 */
            client->file_load_fail_count++;
            if (client->file_load_fail_count == 1) {
                snprintf(client->first_load_fail_path,
                         sizeof(client->first_load_fail_path), "%s", fullpath);
                snprintf(client->first_load_fail_reason,
                         sizeof(client->first_load_fail_reason), "%s",
                         slot->load_error[0] ? slot->load_error : "(unknown)");
            }
            fprintf(stderr, "gaia_client: ERROR failed to load shard %s: %s\n",
                    fullpath, slot->load_error[0] ? slot->load_error : "(unknown)");
        }
    }
    closedir(dir);
#endif

    /* ── FAILCLOSED-01: 装载诊断快照 + 部分装载拒绝 ─────────────────────────── */
    create_diag_reset();
    g_create_diag.entry_count = client->file_entry_count;
    g_create_diag.file_count = client->file_count;
    g_create_diag.fail_count = client->file_load_fail_count;
    g_create_diag.db_type_skipped = client->file_db_type_skipped;
    g_create_diag.max_files_exceeded = client->file_max_files_exceeded;
    for (int f = 0; f < client->file_count; f++)
        g_create_diag.mmap_bytes += (long long)client->files[f].mmap_size;
    snprintf(g_create_diag.first_failed_path, sizeof(g_create_diag.first_failed_path),
             "%s", client->first_load_fail_path);
    snprintf(g_create_diag.first_failed_reason, sizeof(g_create_diag.first_failed_reason),
             "%s", client->first_load_fail_reason);

    if (client->file_load_fail_count > 0) {
        /* 宁可失败, 不可用残缺星表解算（上层 fail-closed）。 */
        create_diag_fail(data_dir, &g_create_diag, "load failed (refusing partial catalog)");
        fprintf(stderr, "gaia_client: FATAL %s\n", g_create_error);
        gaia_client_destroy(client);
        return NULL;
    }
    if (client->file_max_files_exceeded) {
        /* 截断装载与失败装载同属"不完整星表"：条目数超过 MAX_FILES 时拒绝。 */
        create_diag_fail(data_dir, &g_create_diag,
                         "too many shards (refusing truncated catalog)");
        fprintf(stderr, "gaia_client: FATAL %s (MAX_FILES=%d)\n",
                g_create_error, MAX_FILES);
        gaia_client_destroy(client);
        return NULL;
    }

    if (client->file_count > 0) {
        int is_dr3sp = (strstr(client->files[0].db_identifier, "GaiaDR3SP") != NULL);
        client->db_type_detected = is_dr3sp ? GAIA_DB_DR3SP : GAIA_DB_DR3;
    }

    /* G3b: 已接受的文件共享客户端级块缓存总预算 (查询期插入时记账) */
    for (int f = 0; f < client->file_count; f++)
        client->files[f].budget = &client->block_budget;

    return client;
}

void gaia_client_destroy(GaiaClient *client) {
    if (!client) return;

    /* 释放查询结果缓存 */
    query_cache_free(&client->query_cache);

    for (int i = 0; i < client->file_count; i++)
        close_xpsd_file(&client->files[i]);

    /* G3b: 所有文件缓存已释放并回冲记账后销毁总预算锁 */
    block_budget_destroy(&client->block_budget);

    if (client->cache_lock_initialized) {
#ifdef _WIN32
        DeleteCriticalSection(&client->cache_lock);
#else
        pthread_mutex_destroy(&client->cache_lock);
#endif
        client->cache_lock_initialized = 0;
    }

    free(client);
}

int gaia_client_cone_search(GaiaClient *client, double ra, double dec, double radius_deg,
                            double mag_low, double mag_high,
                            GaiaStar **out_stars, int *out_count) {
    if (!client || !out_stars || !out_count) return -1;
    *out_stars = NULL;
    *out_count = 0;
    int nfiles = client->file_count;
    if (nfiles == 0) return 0;

    /* V18R3: per-query trace 上下文（并发查询互不混合，trace-off 零共享写） */
    GaiaTraceCtx trace;
    trace.enabled = gaia_trace_enabled();
    trace.nodes = trace.blocks = trace.bytes = trace.decomp = 0;
    trace.polar_nodes = trace.eq_nodes = 0;

    /* ===== 查询结果缓存检查 ===== */
    cache_lock(client);
    double *cached_ra = NULL, *cached_dec = NULL;
    double *cached_mag = NULL;   /* KI-1 修复: 缓存 mag 通道 double (bitwise) */
    int cached_count = 0;
    if (query_cache_lookup(client, ra, dec, radius_deg, mag_low, mag_high,
                            &cached_ra, &cached_dec, &cached_mag, &cached_count)) {
        /* 缓存命中: 构造GaiaStar数组返回 */
        if (cached_count > 0 && !cached_ra) { cache_unlock(client); return -1; }
        *out_stars = (GaiaStar *)calloc((size_t)cached_count, sizeof(GaiaStar)); /* CAT-GAIA-IMPL: 契约要求 parallax/pmra/pmdec 显式置 0 */
        if (cached_count > 0 && !*out_stars) {
            cache_unlock(client);
            *out_count = 0;
            return -1;
        }
        *out_count = cached_count;
        for (int i = 0; i < cached_count; i++) {
            (*out_stars)[i].ra = cached_ra[i];
            (*out_stars)[i].dec = cached_dec[i];
            (*out_stars)[i].magG = cached_mag[i];
            (*out_stars)[i].magBP = 0;
            (*out_stars)[i].magRP = 0;
            (*out_stars)[i].source_id = 0;
        }
        cache_unlock(client);
        return 0;
    }
    cache_unlock(client);

    /* ===== 正常查询流程 ===== */
    double cos_ra_q = cos(ra * DEG2RAD);
    double sin_ra_q = sin(ra * DEG2RAD);
    double cos_dec_q = cos(dec * DEG2RAD);
    double sin_dec_q = sin(dec * DEG2RAD);
    double cos_radius = cos(radius_deg * DEG2RAD);

    StarCollector *sc_arr = (StarCollector *)calloc(nfiles, sizeof(StarCollector));
    if (!sc_arr) return -1;
    for (int i = 0; i < nfiles; i++) collector_init(&sc_arr[i], 4096);

    #pragma omp parallel for schedule(dynamic) num_threads(gaia_omp_team_size())
    for (int f = 0; f < nfiles; f++) {
        if (gaia_cancel_hit()) continue;   /* 迁移: 文件循环边界取消检查点 */
        XPSDFileInternal *xf = &client->files[f];
        /* G1: 星等 shard 剪枝——每个 XPSD 文件是按星等切片的 shard, 文件声明
         * magnitudeRange=[magnitude_low, magnitude_high]。若该文件星等下界高于
         * 本次查询上限 + 0.25 mag 裕量, 则文件内每条记录的 magG ≥ magnitude_low
         * > mag_high, 在叶子过滤 (search_recursive) 中一律被丢弃, 对结果贡献恒为
         * 0, 故可跳过整文件四叉树遍历。0.25 mag 裕量用于容忍 XML 声明边界不
         * 严格的情形 (必须保守, 绝不可漏掉可能命中的 shard)。逐位等价证据见
         * run/perf-fix/P1-gaia/REPORT.md。 */
        if (xf->has_magnitude_range && xf->magnitude_low > mag_high + 0.25) continue;
        uint32_t scratch_size = xf->global_max_block_size;
        if (scratch_size == 0) scratch_size = 65536;
        uint8_t *scratch = (uint8_t *)malloc(scratch_size);
        if (!scratch) {
            /* FAILCLOSED-01: 修复前为静默 continue —— 整个 shard 的星被丢弃。 */
            drop_ledger_note(&sc_arr[f].drop, 0, 1,
                             "scratch alloc failed (OOM): 1 file skipped");
            continue;
        }

        for (int t = 0; t < xf->tree_count; t++) {
            if (xf->trees[t].node_count > 0 && xf->trees[t].nodes)
                search_recursive(xf, &xf->trees[t], 0, ra, dec, radius_deg,
                                  mag_low, mag_high, cos_ra_q, sin_ra_q,
                                  cos_dec_q, sin_dec_q, cos_radius, &sc_arr[f], scratch, &trace);
        }
        free(scratch);
    }

    /* FAILCLOSED-01: 任一文件发生丢弃（单星扩容失败 / 整叶解压失败 / scratch
     * 分配失败）⇒ **不返回不完整星表**：输出置空并返回 -1。修复前这些丢弃点
     * 静默 return/continue，调用方拿到"看起来正常"的更短星表（与 shard 静默
     * 丢弃同源，见 run/WCS-DETERMINISM-01/REPORT.md §1.3）。 */
    for (int f = 0; f < nfiles; f++) {
        if (sc_arr[f].drop.failed) {
            fprintf(stderr,
                    "gaia_client: FATAL cone_search degraded: %s "
                    "(dropped_stars=%ld dropped_leaves=%ld) — refusing "
                    "incomplete catalog\n",
                    sc_arr[f].drop.reason, sc_arr[f].drop.dropped_stars,
                    sc_arr[f].drop.dropped_leaves);
            for (int g = 0; g < nfiles; g++) collector_free(&sc_arr[g]);
            free(sc_arr);
            *out_stars = NULL;
            *out_count = 0;
            return -1;
        }
    }

    int total = 0;
    for (int f = 0; f < nfiles; f++) total += sc_arr[f].count;

    if (total == 0) {
        for (int f = 0; f < nfiles; f++) collector_free(&sc_arr[f]);
        free(sc_arr);
        *out_stars = NULL; *out_count = 0;
        return 0;
    }

    *out_stars = (GaiaStar *)calloc((size_t)total, sizeof(GaiaStar)); /* CAT-GAIA-IMPL: 未初始化字段显式置 0 */
    if (!*out_stars) {
        for (int f = 0; f < nfiles; f++) collector_free(&sc_arr[f]);
        free(sc_arr);
        *out_count = 0;
        return -1;
    }
    *out_count = total;
    int idx = 0;

    /* 构建缓存数据 (ra/dec/mag数组)。
     * KI-1 修复: 缓存 mag 通道 float→double（bitwise 往返）。 */
    double *cache_ra = (double *)malloc(total * sizeof(double));
    double *cache_dec = (double *)malloc(total * sizeof(double));
    double *cache_mag = (double *)malloc(total * sizeof(double));
    if (total > 0 && (!cache_ra || !cache_dec || !cache_mag)) {
        free(cache_ra);
        free(cache_dec);
        free(cache_mag);
        free(*out_stars);
        *out_stars = NULL;
        *out_count = 0;
        for (int f = 0; f < nfiles; f++) collector_free(&sc_arr[f]);
        free(sc_arr);
        return -1;
    }

    for (int f = 0; f < nfiles; f++) {
        for (int i = 0; i < sc_arr[f].count; i++) {
            (*out_stars)[idx].ra = sc_arr[f].stars[i].ra;
            (*out_stars)[idx].dec = sc_arr[f].stars[i].dec;
            (*out_stars)[idx].magG = sc_arr[f].stars[i].magG;
            (*out_stars)[idx].magBP = 0;
            (*out_stars)[idx].magRP = 0;
            (*out_stars)[idx].source_id = 0;

            if (cache_ra && cache_dec && cache_mag) {
                cache_ra[idx] = sc_arr[f].stars[i].ra;
                cache_dec[idx] = sc_arr[f].stars[i].dec;
                cache_mag[idx] = sc_arr[f].stars[i].magG;
            }
            idx++;
        }
        collector_free(&sc_arr[f]);
    }
    free(sc_arr);

    /* ===== 存入查询结果缓存 ===== */
    if (cache_ra && cache_dec && cache_mag) {
        cache_lock(client);
        query_cache_insert(client, ra, dec, radius_deg, mag_low, mag_high,
                           cache_ra, cache_dec, cache_mag, total);
        cache_unlock(client);
    }
    /* 注意: query_cache_insert会拷贝数据, 这里释放临时数组 */
    free(cache_ra);
    free(cache_dec);
    free(cache_mag);

    /* V18R3 诊断输出（ASTROCS_GAIA_TRACE=1）：per-query 统计 */
    if (trace.enabled) {
        fprintf(stderr,
                "[gaia_trace] query ra=%.4f dec=%.4f r=%.4f -> nodes=%lld "
                "blocks=%lld bytes=%lld decomp=%lld stars=%d "
                "(polar_nodes=%lld eq_nodes=%lld)\n",
                ra, dec, radius_deg,
                trace.nodes, trace.blocks, trace.bytes,
                trace.decomp, *out_count,
                trace.polar_nodes, trace.eq_nodes);
    }

    return 0;
}

int gaia_client_cone_search_for_solver(GaiaClient *client, double ra, double dec, double radius_deg,
                                        double mag_high,
                                        double **out_ra, double **out_dec, float **out_mag,
                                        int *out_count) {
    if (!out_ra || !out_dec || !out_mag || !out_count) return -1;
    *out_ra = NULL; *out_dec = NULL; *out_mag = NULL; *out_count = 0;
    GaiaStar *stars = NULL;
    int count = 0;
    int ret = gaia_client_cone_search(client, ra, dec, radius_deg, -1.5, mag_high, &stars, &count);
    if (ret != 0 || count == 0) {
        *out_ra = NULL; *out_dec = NULL; *out_mag = NULL; *out_count = 0;
        return ret;
    }

    double *ra_arr = (double *)malloc((size_t)count * sizeof(double));
    double *dec_arr = (double *)malloc((size_t)count * sizeof(double));
    float *mag_arr = (float *)malloc((size_t)count * sizeof(float));
    if (!ra_arr || !dec_arr || !mag_arr) {
        free(ra_arr);
        free(dec_arr);
        free(mag_arr);
        free(stars);
        return -1;
    }

    for (int i = 0; i < count; i++) {
        ra_arr[i] = stars[i].ra;
        dec_arr[i] = stars[i].dec;
        mag_arr[i] = (float)stars[i].magG;
    }
    free(stars);
    *out_ra = ra_arr;
    *out_dec = dec_arr;
    *out_mag = mag_arr;
    *out_count = count;
    return 0;
}

int gaia_client_get_db_type(GaiaClient *client) {
    if (!client) return GAIA_DB_AUTO;
    return client->db_type_detected;
}

int gaia_client_get_file_count(GaiaClient *client) {
    if (!client) return 0;
    return client->file_count;
}

int gaia_client_get_total_sources(GaiaClient *client) {
    if (!client) return 0;
    int total = 0;
    for (int i = 0; i < client->file_count; i++) {
        total += client->files[i].total_sources;
    }
    return total;
}

/* P19-gaia (V5-N-03): 因 magnitudeRange 声明非法而放弃整 shard 星等剪枝的
 * 文件数 (可见统计)。0 = 所有文件声明合法或缺失 (与历史行为一致); >0 = 有
 * 畸形声明被拒并已 fprintf(stderr) 告警。 */
int gaia_client_get_magnitude_range_reject_count(GaiaClient *client) {
    if (!client) return 0;
    return client->magnitude_range_reject_count;
}

/* ── FAILCLOSED-01: 装载/枚举可见计数与最近一次 create 诊断 ──────────────────
 * create 成功返回的 client 恒有 file_load_fail_count == 0 且
 * file_count == file_entry_count（db_type 过滤单独计数）；两个计数供上层节点
 * 前置条件断言与 G-1 shard 覆盖门使用（见 gaia_client.h）。 */
int gaia_client_get_file_load_fail_count(GaiaClient *client) {
    if (!client) return 0;
    return client->file_load_fail_count;
}

int gaia_client_get_file_entry_count(GaiaClient *client) {
    if (!client) return 0;
    return client->file_entry_count;
}

/* 最近一次（本线程）create 的诊断快照；thread-local 语义见 g_create_diag 注释。 */
int gaia_client_get_last_create_diagnostics(GaiaCreateDiagnostics *out) {
    if (!out || !g_create_diag_valid) return -1;
    *out = g_create_diag;
    return 0;
}

/* 失败原因串（成功 / 未调用过 create 时为 NULL）。 */
const char *gaia_client_get_last_create_error(void) {
    return g_create_error[0] ? g_create_error : NULL;
}

int gaia_client_cone_search_with_spectrum(
    GaiaClient *client,
    double ra, double dec, double radius_deg,
    double mag_low, double mag_high,
    GaiaSpectrumStar **out_stars,
    uint8_t **out_spectra,
    int *out_count) {
    if (!client || !out_stars || !out_spectra || !out_count) return -1;
    *out_stars = NULL;
    *out_spectra = NULL;
    *out_count = 0;
    int nfiles = client->file_count;
    if (nfiles == 0) return 0;

    GaiaTraceCtx trace;
    trace.enabled = gaia_trace_enabled();
    trace.nodes = trace.blocks = trace.bytes = trace.decomp = 0;
    trace.polar_nodes = trace.eq_nodes = 0;

    double cos_ra_q = cos(ra * DEG2RAD);
    double sin_ra_q = sin(ra * DEG2RAD);
    double cos_dec_q = cos(dec * DEG2RAD);
    double sin_dec_q = sin(dec * DEG2RAD);
    double cos_radius = cos(radius_deg * DEG2RAD);

    SpectrumStarCollector *sc_arr = (SpectrumStarCollector *)calloc(nfiles, sizeof(SpectrumStarCollector));
    if (!sc_arr) return -1;
    for (int i = 0; i < nfiles; i++) {
        int spec_count = 0;
        if (client->files[i].has_spectrum) {
            spec_count = client->files[i].spectrum_count;
            if (spec_count == 0) spec_count = WL_COUNT;
        }
        spec_collector_init(&sc_arr[i], 4096, spec_count);
    }

    #pragma omp parallel for schedule(dynamic) num_threads(gaia_omp_team_size())
    for (int f = 0; f < nfiles; f++) {
        if (gaia_cancel_hit()) continue;   /* 迁移: 文件循环边界取消检查点 */
        XPSDFileInternal *xf = &client->files[f];
        uint32_t scratch_size = xf->global_max_block_size;
        if (scratch_size == 0) scratch_size = 65536;
        uint8_t *scratch = (uint8_t *)malloc(scratch_size);
        if (!scratch) {
            /* FAILCLOSED-01: 修复前为静默 continue —— 整个 shard 的星被丢弃。 */
            drop_ledger_note(&sc_arr[f].drop, 0, 1,
                             "scratch alloc failed (OOM): 1 file skipped");
            continue;
        }

        for (int t = 0; t < xf->tree_count; t++) {
            if (xf->trees[t].node_count > 0 && xf->trees[t].nodes)
                search_recursive_spectrum(xf, &xf->trees[t], 0, ra, dec, radius_deg,
                                  mag_low, mag_high, cos_ra_q, sin_ra_q,
                                  cos_dec_q, sin_dec_q, cos_radius, &sc_arr[f], scratch, &trace);
        }
        free(scratch);
    }

    /* FAILCLOSED-01: 同 cone_search —— 任一文件丢弃 ⇒ 不返回不完整星表。 */
    for (int f = 0; f < nfiles; f++) {
        if (sc_arr[f].drop.failed) {
            fprintf(stderr,
                    "gaia_client: FATAL cone_search_with_spectrum degraded: %s "
                    "(dropped_stars=%ld dropped_leaves=%ld) — refusing "
                    "incomplete catalog\n",
                    sc_arr[f].drop.reason, sc_arr[f].drop.dropped_stars,
                    sc_arr[f].drop.dropped_leaves);
            for (int g = 0; g < nfiles; g++) spec_collector_free(&sc_arr[g]);
            free(sc_arr);
            *out_stars = NULL;
            *out_spectra = NULL;
            *out_count = 0;
            return -1;
        }
    }

    int total = 0;
    for (int f = 0; f < nfiles; f++) total += sc_arr[f].count;

    if (total == 0) {
        for (int f = 0; f < nfiles; f++) spec_collector_free(&sc_arr[f]);
        free(sc_arr);
        *out_stars = NULL;
        *out_spectra = NULL;
        *out_count = 0;
        return 0;
    }

    int any_spectrum = 0;
    int global_spec_count = 0;
    for (int f = 0; f < nfiles; f++) {
        if (client->files[f].has_spectrum) {
            global_spec_count = client->files[f].spectrum_count;
            if (global_spec_count == 0) global_spec_count = WL_COUNT;
            any_spectrum = 1;
            break;
        }
    }

    *out_stars = (GaiaSpectrumStar *)calloc((size_t)total, sizeof(GaiaSpectrumStar)); /* CAT-GAIA-IMPL: 全字段确定性 */
    if (!*out_stars) {
        for (int f = 0; f < nfiles; f++) spec_collector_free(&sc_arr[f]);
        free(sc_arr);
        *out_count = 0;
        return -1;
    }
    *out_count = total;

    if (any_spectrum) {
        *out_spectra = (uint8_t *)malloc((size_t)total * global_spec_count);
        if (!*out_spectra) {
            free(*out_stars);
            *out_stars = NULL;
            *out_count = 0;
            for (int f = 0; f < nfiles; f++) spec_collector_free(&sc_arr[f]);
            free(sc_arr);
            return -1;
        }
    } else {
        *out_spectra = NULL;
    }

    int idx = 0;
    for (int f = 0; f < nfiles; f++) {
        for (int i = 0; i < sc_arr[f].count; i++) {
            (*out_stars)[idx].ra = sc_arr[f].stars[i].ra;
            (*out_stars)[idx].dec = sc_arr[f].stars[i].dec;
            (*out_stars)[idx].magG = sc_arr[f].stars[i].magG;
            (*out_stars)[idx].flux_min = sc_arr[f].stars[i].flux_min;
            (*out_stars)[idx].flux_mul = sc_arr[f].stars[i].flux_mul;

            if (*out_spectra) {
                if (sc_arr[f].spectrum_count > 0) {
                    int copy_count = sc_arr[f].spectrum_count;
                    if (copy_count > global_spec_count) copy_count = global_spec_count;
                    memcpy(*out_spectra + (size_t)idx * global_spec_count,
                           sc_arr[f].spectra + (size_t)i * sc_arr[f].spectrum_count,
                           copy_count);
                } else {
                    /* KI-2 修复 (CAT-GAIA-IMPL, 纯技术: 漏拷修复, 不改科学
                     * 公式): 混合 DB (SP+DR3) 时 DR3 星的光谱区段原样跳过 →
                     * 返回未初始化内存 (非确定性输出)。按 DATA 合同
                     * "无光谱数据时为 0" (GaiaSpectrumStar.flux_min/flux_mul
                     * 恒 0, F(λ)=byte×mul+min≡0) 零填充该星区段, 输出确定。 */
                    memset(*out_spectra + (size_t)idx * global_spec_count,
                           0, global_spec_count);
                }
            }
            idx++;
        }
        spec_collector_free(&sc_arr[f]);
    }
    free(sc_arr);

    if (trace.enabled) {
        fprintf(stderr,
                "[gaia_trace] spectrum query ra=%.4f dec=%.4f r=%.4f -> nodes=%lld "
                "blocks=%lld bytes=%lld decomp=%lld stars=%d "
                "(polar_nodes=%lld eq_nodes=%lld)\n",
                ra, dec, radius_deg,
                trace.nodes, trace.blocks, trace.bytes,
                trace.decomp, *out_count,
                trace.polar_nodes, trace.eq_nodes);
    }

    return 0;
}

int gaia_client_query_spectrum_by_coords(
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
    int *out_count) {

    if (!client || !ra_list || !dec_list || !out_stars || !out_spectra ||
        !out_match_idx || !out_count) return -1;
    *out_stars = NULL;
    *out_spectra = NULL;
    *out_match_idx = NULL;
    *out_count = 0;
    int nfiles = client->file_count;
    if (nfiles == 0 || n_coords <= 0) {
        return 0;
    }

    double radius_deg = match_radius_arcsec / 3600.0;
    double cos_radius = cos(radius_deg * DEG2RAD);

    /* 获取全局光谱点数 (取第一个有光谱的文件) */
    int global_spec_count = WL_COUNT;
    for (int f = 0; f < nfiles; f++) {
        if (client->files[f].has_spectrum) {
            global_spec_count = client->files[f].spectrum_count;
            if (global_spec_count == 0) global_spec_count = WL_COUNT;
            break;
        }
    }

    /* 临时数组: 每个坐标的最佳匹配结果 (并行写入) */
    GaiaSpectrumStar *temp_stars = (GaiaSpectrumStar *)calloc((size_t)n_coords, sizeof(GaiaSpectrumStar)); /* CAT-GAIA-IMPL */
    uint8_t *temp_spectra = (uint8_t *)malloc((size_t)n_coords * global_spec_count);
    int *found_flags = (int *)calloc(n_coords, sizeof(int));

    if (!temp_stars || !temp_spectra || !found_flags) {
        free(temp_stars);
        free(temp_spectra);
        free(found_flags);
        *out_stars = NULL;
        *out_spectra = NULL;
        *out_match_idx = NULL;
        *out_count = 0;
        return -1;
    }

    /* FAILCLOSED-01: 本入口并行轴=坐标，丢弃记账器由所有 worker 共享
     * （写侧 drop_ledger_note 内 named critical 保护）。 */
    GaiaDropLedger q_drop;
    drop_ledger_init(&q_drop);

    /* 并行搜索: 每个坐标独立搜索所有文件，找角距离最近的星 */
    #pragma omp parallel for schedule(dynamic) num_threads(gaia_omp_team_size())
    for (int i = 0; i < n_coords; i++) {
        if (gaia_cancel_hit()) continue;   /* 迁移: 坐标迭代边界取消检查点 */
        double ra = ra_list[i];
        double dec = dec_list[i];
        double cos_ra_q = cos(ra * DEG2RAD);
        double sin_ra_q = sin(ra * DEG2RAD);
        double cos_dec_q = cos(dec * DEG2RAD);
        double sin_dec_q = sin(dec * DEG2RAD);

        double best_ang_dist = 1e9;

        for (int f = 0; f < nfiles; f++) {
            XPSDFileInternal *xf = &client->files[f];
            uint32_t scratch_size = xf->global_max_block_size;
            if (scratch_size == 0) scratch_size = 65536;
            uint8_t *scratch = (uint8_t *)malloc(scratch_size);
            if (!scratch) {
                /* FAILCLOSED-01: 修复前为静默 continue —— 该文件的星被丢弃。 */
                drop_ledger_note(&q_drop, 0, 1,
                                 "scratch alloc failed (OOM): 1 file skipped");
                continue;
            }

            int spec_count = 0;
            if (xf->has_spectrum) {
                spec_count = xf->spectrum_count;
                if (spec_count == 0) spec_count = WL_COUNT;
            }

            SpectrumStarCollector sc;
            spec_collector_init(&sc, 16, spec_count);

            for (int t = 0; t < xf->tree_count; t++) {
                if (xf->trees[t].node_count > 0 && xf->trees[t].nodes)
                    search_recursive_spectrum(xf, &xf->trees[t], 0, ra, dec, radius_deg,
                                              mag_low, mag_high, cos_ra_q, sin_ra_q,
                                              cos_dec_q, sin_dec_q, cos_radius, &sc, scratch, NULL);
            }
            free(scratch);

            /* 从 collector 中找角距离最小的星 */
            for (int j = 0; j < sc.count; j++) {
                double s_ra = sc.stars[j].ra;
                double s_dec = sc.stars[j].dec;
                /* 球面角距离: cos(d) = sin(dec_q)*sin(dec_s) + cos(dec_q)*cos(dec_s)*cos(ra_q-ra_s) */
                double cos_d = sin_dec_q * sin(s_dec * DEG2RAD) +
                               cos_dec_q * cos(s_dec * DEG2RAD) *
                               (cos_ra_q * cos(s_ra * DEG2RAD) + sin_ra_q * sin(s_ra * DEG2RAD));
                if (cos_d > 1.0) cos_d = 1.0;
                if (cos_d < -1.0) cos_d = -1.0;
                double ang_dist = acos(cos_d);
                if (ang_dist < best_ang_dist) {
                    best_ang_dist = ang_dist;
                    temp_stars[i].ra = s_ra;
                    temp_stars[i].dec = s_dec;
                    temp_stars[i].magG = sc.stars[j].magG;
                    temp_stars[i].flux_min = sc.stars[j].flux_min;
                    temp_stars[i].flux_mul = sc.stars[j].flux_mul;
                    if (sc.spectrum_count > 0) {
                        int copy_cnt = sc.spectrum_count;
                        if (copy_cnt > global_spec_count) copy_cnt = global_spec_count;
                        memcpy(temp_spectra + (size_t)i * global_spec_count,
                               sc.spectra + (size_t)j * sc.spectrum_count, copy_cnt);
                    } else {
                        /* KI-2 修复 (CAT-GAIA-IMPL): by_coords 命中 DR3 星
                         * (无光谱) 时原 copy_cnt=0 → 区段未初始化; 按 DATA
                         * 合同 "无光谱数据时为 0" 零填充。 */
                        memset(temp_spectra + (size_t)i * global_spec_count,
                               0, global_spec_count);
                    }
                    found_flags[i] = 1;
                }
            }
            /* FAILCLOSED-01: 本坐标的丢弃聚合到共享记账器（query 级 fail-closed）。 */
            if (sc.drop.failed)
                drop_ledger_note(&q_drop, sc.drop.dropped_stars,
                                 sc.drop.dropped_leaves, sc.drop.reason);
            spec_collector_free(&sc);
        }
    }

    /* FAILCLOSED-01: 任一坐标发生丢弃 ⇒ 不返回不完整星表（输出全部置空）。 */
    if (q_drop.failed) {
        fprintf(stderr,
                "gaia_client: FATAL query_spectrum_by_coords degraded: %s "
                "(dropped_stars=%ld dropped_leaves=%ld) — refusing incomplete "
                "catalog\n",
                q_drop.reason, q_drop.dropped_stars, q_drop.dropped_leaves);
        free(temp_stars);
        free(temp_spectra);
        free(found_flags);
        *out_stars = NULL;
        *out_spectra = NULL;
        *out_match_idx = NULL;
        *out_count = 0;
        return -1;
    }

    /* 压缩: 将匹配结果紧凑排列到输出数组 */
    GaiaSpectrumStar *result_stars = (GaiaSpectrumStar *)calloc((size_t)n_coords, sizeof(GaiaSpectrumStar)); /* CAT-GAIA-IMPL */
    uint8_t *result_spectra = (uint8_t *)malloc((size_t)n_coords * global_spec_count);
    int *match_idx = (int *)calloc((size_t)n_coords, sizeof(int)); /* CAT-GAIA-IMPL */
    if (!result_stars || !result_spectra || !match_idx) {
        free(result_stars);
        free(result_spectra);
        free(match_idx);
        free(temp_stars);
        free(temp_spectra);
        free(found_flags);
        return -1;
    }
    int matched_count = 0;

    for (int i = 0; i < n_coords; i++) {
        if (found_flags[i]) {
            result_stars[matched_count] = temp_stars[i];
            memcpy(result_spectra + (size_t)matched_count * global_spec_count,
                   temp_spectra + (size_t)i * global_spec_count, global_spec_count);
            match_idx[i] = matched_count;
            matched_count++;
        } else {
            match_idx[i] = -1;
        }
    }

    free(temp_stars);
    free(temp_spectra);
    free(found_flags);

    *out_stars = result_stars;
    *out_spectra = result_spectra;
    *out_match_idx = match_idx;
    *out_count = matched_count;
    return 0;
}

int gaia_client_cone_search_with_photometry(
    GaiaClient *client,
    double ra, double dec, double radius_deg,
    double mag_low, double mag_high,
    GaiaPhotometryStar **out_stars,
    int *out_count) {
    if (!client || !out_stars || !out_count) return -1;
    *out_stars = NULL;
    *out_count = 0;
    int nfiles = client->file_count;
    if (nfiles == 0) return 0;

    GaiaTraceCtx trace;
    trace.enabled = gaia_trace_enabled();
    trace.nodes = trace.blocks = trace.bytes = trace.decomp = 0;
    trace.polar_nodes = trace.eq_nodes = 0;

    double cos_ra_q = cos(ra * DEG2RAD);
    double sin_ra_q = sin(ra * DEG2RAD);
    double cos_dec_q = cos(dec * DEG2RAD);
    double sin_dec_q = sin(dec * DEG2RAD);
    double cos_radius = cos(radius_deg * DEG2RAD);

    PhotometryStarCollector *pc_arr = (PhotometryStarCollector *)calloc(nfiles, sizeof(PhotometryStarCollector));
    if (!pc_arr) return -1;
    for (int i = 0; i < nfiles; i++) {
        phot_collector_init(&pc_arr[i], 4096);
    }

    #pragma omp parallel for schedule(dynamic) num_threads(gaia_omp_team_size())
    for (int f = 0; f < nfiles; f++) {
        if (gaia_cancel_hit()) continue;   /* 迁移: 文件循环边界取消检查点 */
        XPSDFileInternal *xf = &client->files[f];
        uint32_t scratch_size = xf->global_max_block_size;
        if (scratch_size == 0) scratch_size = 65536;
        uint8_t *scratch = (uint8_t *)malloc(scratch_size);
        if (!scratch) {
            /* FAILCLOSED-01: 修复前为静默 continue —— 整个 shard 的星被丢弃。 */
            drop_ledger_note(&pc_arr[f].drop, 0, 1,
                             "scratch alloc failed (OOM): 1 file skipped");
            continue;
        }

        for (int t = 0; t < xf->tree_count; t++) {
            if (xf->trees[t].node_count > 0 && xf->trees[t].nodes)
                search_recursive_photometry(xf, &xf->trees[t], 0, ra, dec, radius_deg,
                                  mag_low, mag_high, cos_ra_q, sin_ra_q,
                                  cos_dec_q, sin_dec_q, cos_radius, &pc_arr[f], scratch, &trace);
        }
        free(scratch);
    }

    /* FAILCLOSED-01: 同 cone_search —— 任一文件丢弃 ⇒ 不返回不完整星表。 */
    for (int f = 0; f < nfiles; f++) {
        if (pc_arr[f].drop.failed) {
            fprintf(stderr,
                    "gaia_client: FATAL cone_search_with_photometry degraded: %s "
                    "(dropped_stars=%ld dropped_leaves=%ld) — refusing "
                    "incomplete catalog\n",
                    pc_arr[f].drop.reason, pc_arr[f].drop.dropped_stars,
                    pc_arr[f].drop.dropped_leaves);
            for (int g = 0; g < nfiles; g++) phot_collector_free(&pc_arr[g]);
            free(pc_arr);
            *out_stars = NULL;
            *out_count = 0;
            return -1;
        }
    }

    int total = 0;
    for (int f = 0; f < nfiles; f++) total += pc_arr[f].count;

    if (total == 0) {
        for (int f = 0; f < nfiles; f++) phot_collector_free(&pc_arr[f]);
        free(pc_arr);
        *out_stars = NULL;
        *out_count = 0;
        return 0;
    }

    *out_stars = (GaiaPhotometryStar *)calloc((size_t)total, sizeof(GaiaPhotometryStar)); /* CAT-GAIA-IMPL: 全字段确定性 */
    if (!*out_stars) {
        for (int f = 0; f < nfiles; f++) phot_collector_free(&pc_arr[f]);
        free(pc_arr);
        *out_count = 0;
        return -1;
    }
    *out_count = total;

    int idx = 0;
    for (int f = 0; f < nfiles; f++) {
        for (int i = 0; i < pc_arr[f].count; i++) {
            (*out_stars)[idx].ra = pc_arr[f].stars[i].ra;
            (*out_stars)[idx].dec = pc_arr[f].stars[i].dec;
            (*out_stars)[idx].magG = pc_arr[f].stars[i].magG;
            (*out_stars)[idx].magBP = pc_arr[f].stars[i].magBP;
            (*out_stars)[idx].magRP = pc_arr[f].stars[i].magRP;
            idx++;
        }
        phot_collector_free(&pc_arr[f]);
    }
    free(pc_arr);

    if (trace.enabled) {
        fprintf(stderr,
                "[gaia_trace] photometry query ra=%.4f dec=%.4f r=%.4f -> nodes=%lld "
                "blocks=%lld bytes=%lld decomp=%lld stars=%d "
                "(polar_nodes=%lld eq_nodes=%lld)\n",
                ra, dec, radius_deg,
                trace.nodes, trace.blocks, trace.bytes,
                trace.decomp, *out_count,
                trace.polar_nodes, trace.eq_nodes);
    }

    return 0;
}

int gaia_client_get_spectrum_params(GaiaClient *client, int *out_start_nm, int *out_step_nm, int *out_count) {
    for (int i = 0; i < client->file_count; i++) {
        if (client->files[i].has_spectrum) {
            if (out_start_nm) *out_start_nm = client->files[i].spectrum_start;
            if (out_step_nm) *out_step_nm = client->files[i].spectrum_step;
            if (out_count) {
                *out_count = client->files[i].spectrum_count;
                if (*out_count == 0) *out_count = WL_COUNT;
            }
            return 1;
        }
    }
    if (out_start_nm) *out_start_nm = 0;
    if (out_step_nm) *out_step_nm = 0;
    if (out_count) *out_count = 0;
    return 0;
}
