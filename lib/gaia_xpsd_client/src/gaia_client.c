#include "gaia_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <pthread.h>
#endif

#include <zlib.h>

#define XPSD_MAGIC "XPSD0100"
#define WL_COUNT 343
#define STAR_STRIDE_SP (40 + (WL_COUNT + (WL_COUNT & 1)))
#define STAR_STRIDE_NOSP 32
#define MAX_FILES 32
#define MAX_STARS_RESULT 200000

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define DEG2RAD (M_PI / 180.0)
#define RAD2DEG (180.0 / M_PI)

/* ===== 缓存配置 ===== */
#define BLOCK_CACHE_CAPACITY  8192   /* 解压块缓存哈希表大小 (2的幂) */
#define BLOCK_CACHE_MASK       (BLOCK_CACHE_CAPACITY - 1)
#define QUERY_CACHE_CAPACITY   64     /* 查询结果缓存最大条目数 */
#define QUERY_CACHE_TTL_SEC    60     /* 查询结果缓存TTL (秒) */
#define QUERY_CACHE_RA_ROUND   0.001  /* RA舍入精度 (度) */
#define QUERY_CACHE_DEC_ROUND  0.001  /* Dec舍入精度 (度) */
#define QUERY_CACHE_RAD_ROUND  0.01   /* 半径舍入精度 (度) */
#define QUERY_CACHE_MAG_ROUND  0.01   /* 星等舍入精度 */
#define BLOCK_CACHE_MAX_MEMORY (4ULL * 1024 * 1024 * 1024) /* 解压块缓存最大4GB */
#define MEMORY_PRESSURE_THRESHOLD (4ULL * 1024 * 1024 * 1024) /* 可用内存<4GB时触发释放 */
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

/* ===== 查询结果缓存 ===== */
typedef struct {
    double ra, dec, radius, mag_high;  /* 舍入后的查询参数 */
    double *out_ra;                    /* 缓存的RA数组 */
    double *out_dec;                   /* 缓存的Dec数组 */
    float *out_mag;                    /* 缓存的Mag数组 */
    int out_count;                     /* 星数 */
    time_t timestamp;                  /* 缓存创建时间 */
    int valid;                         /* 是否有效 */
} QueryCacheEntry;

typedef struct {
    QueryCacheEntry entries[QUERY_CACHE_CAPACITY];
    int count;
    size_t total_memory;
} QueryCache;

typedef struct {
    char filepath[1024];
    char db_identifier[256];
    double magnitude_low, magnitude_high;
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
    BlockCache block_cache;  /* 解压块缓存 (保留到关闭) */
} XPSDFileInternal;

struct GaiaClient {
    XPSDFileInternal files[MAX_FILES];
    int file_count;
    GaiaDbType db_type;
    int db_type_detected;
    QueryCache query_cache;  /* 查询结果缓存 (60s TTL) */
#ifdef _WIN32
    CRITICAL_SECTION cache_lock;
#else
    pthread_mutex_t cache_lock;
#endif
    int cache_lock_initialized;
};

/* ===== 简单星结构 ===== */
typedef struct {
    double ra, dec, magG;
} SimpleStar;

typedef struct {
    SimpleStar *stars;
    int count;
    int capacity;
} StarCollector;

typedef struct {
    double ra, dec, magG;
} SpectrumStar;

typedef struct {
    SpectrumStar *stars;
    uint8_t *spectra;
    int count;
    int capacity;
    int spectrum_count;
} SpectrumStarCollector;

typedef struct {
    double ra, dec, magG, magBP, magRP;
} PhotometryStar;

typedef struct {
    PhotometryStar *stars;
    int count;
    int capacity;
} PhotometryStarCollector;

/* ===== 缓存辅助函数 ===== */

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

/* ===== 解压块缓存函数 ===== */

static void block_cache_init(BlockCache *bc) {
    bc->entries = (BlockCacheEntry *)calloc(BLOCK_CACHE_CAPACITY, sizeof(BlockCacheEntry));
    bc->capacity = BLOCK_CACHE_CAPACITY;
    bc->count = 0;
    bc->total_memory = 0;
}

static void block_cache_free(BlockCache *bc) {
    if (!bc->entries) return;
    for (int i = 0; i < bc->capacity; i++) {
        if (bc->entries[i].data) {
            free(bc->entries[i].data);
            bc->entries[i].data = NULL;
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

/* 查找解压块缓存, 命中返回指针, 未命中返回NULL */
static uint8_t *block_cache_lookup(BlockCache *bc, uint64_t block_offset, uint32_t *data_size) {
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

/* 插入解压块缓存 */
static void block_cache_insert(BlockCache *bc, uint64_t block_offset,
                                const uint8_t *data, uint32_t data_size) {
    if (!bc->entries || data_size == 0) return;

    /* 内存压力检查: 超限则淘汰最旧的1/4 */
    if (bc->total_memory + data_size > BLOCK_CACHE_MAX_MEMORY || check_memory_pressure()) {
        /* 按last_access排序, 淘汰最旧的1/4条目 */
        int to_evict = bc->count / 4 + 1;
        /* 简单策略: 扫描找最旧的条目淘汰 */
        for (int e = 0; e < to_evict && bc->count > 0; e++) {
            time_t oldest = TIME_MAX;
            int oldest_idx = -1;
            for (int i = 0; i < bc->capacity; i++) {
                if (bc->entries[i].block_offset != 0 &&
                    bc->entries[i].last_access < oldest) {
                    oldest = bc->entries[i].last_access;
                    oldest_idx = i;
                }
            }
            if (oldest_idx >= 0) {
                free(bc->entries[oldest_idx].data);
                bc->total_memory -= bc->entries[oldest_idx].data_size;
                bc->entries[oldest_idx].data = NULL;
                bc->entries[oldest_idx].block_offset = 0;
                bc->count--;
            }
        }
    }

    /* 开放寻址插入 */
    uint32_t idx = block_hash(block_offset);
    for (int probe = 0; probe < bc->capacity; probe++) {
        uint32_t i = (idx + probe) & BLOCK_CACHE_MASK;
        if (bc->entries[i].block_offset == 0) {
            /* 空槽, 插入 */
            uint8_t *copy = (uint8_t *)malloc(data_size);
            if (!copy) return;
            memcpy(copy, data, data_size);
            bc->entries[i].block_offset = block_offset;
            bc->entries[i].data = copy;
            bc->entries[i].data_size = data_size;
            bc->entries[i].last_access = time(NULL);
            bc->count++;
            bc->total_memory += data_size;
            return;
        }
        if (bc->entries[i].block_offset == block_offset) {
            /* 已存在, 更新 */
            if (bc->entries[i].data_size == data_size) {
                memcpy(bc->entries[i].data, data, data_size);
                bc->entries[i].last_access = time(NULL);
                return;
            }
            /* 大小不同, 替换 */
            free(bc->entries[i].data);
            uint8_t *copy = (uint8_t *)malloc(data_size);
            if (!copy) {
                bc->entries[i].block_offset = 0;
                bc->count--;
                return;
            }
            memcpy(copy, data, data_size);
            bc->total_memory -= bc->entries[i].data_size;
            bc->entries[i].data = copy;
            bc->entries[i].data_size = data_size;
            bc->entries[i].last_access = time(NULL);
            bc->total_memory += data_size;
            return;
        }
    }
    /* 哈希表满, 不插入 */
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
            qc->entries[i].valid = 0;
            qc->count--;
        }
    }
}

/* 舍入查询参数用于缓存键 */
static double round_to(double val, double precision) {
    return round(val / precision) * precision;
}

/* 查找查询结果缓存 */
static int query_cache_lookup(GaiaClient *client, double ra, double dec,
                               double radius, double mag_high,
                               double **out_ra, double **out_dec,
                               float **out_mag, int *out_count) {
    QueryCache *qc = &client->query_cache;
    time_t now = time(NULL);

    double r_ra = round_to(ra, QUERY_CACHE_RA_ROUND);
    double r_dec = round_to(dec, QUERY_CACHE_DEC_ROUND);
    double r_radius = round_to(radius, QUERY_CACHE_RAD_ROUND);
    double r_mag = round_to(mag_high, QUERY_CACHE_MAG_ROUND);

    for (int i = 0; i < QUERY_CACHE_CAPACITY; i++) {
        if (!qc->entries[i].valid) continue;
        if ((now - qc->entries[i].timestamp) > QUERY_CACHE_TTL_SEC) {
            /* 过期, 清理 */
            free(qc->entries[i].out_ra);
            free(qc->entries[i].out_dec);
            free(qc->entries[i].out_mag);
            qc->entries[i].valid = 0;
            qc->count--;
            continue;
        }
        if (qc->entries[i].ra == r_ra &&
            qc->entries[i].dec == r_dec &&
            qc->entries[i].radius == r_radius &&
            qc->entries[i].mag_high == r_mag) {
            /* 命中 */
            *out_ra = qc->entries[i].out_ra;
            *out_dec = qc->entries[i].out_dec;
            *out_mag = qc->entries[i].out_mag;
            *out_count = qc->entries[i].out_count;
            return 1;  /* 缓存命中 */
        }
    }
    return 0;  /* 未命中 */
}

/* 插入查询结果缓存 */
static void query_cache_insert(GaiaClient *client, double ra, double dec,
                                double radius, double mag_high,
                                double *out_ra, double *out_dec,
                                float *out_mag, int out_count) {
    QueryCache *qc = &client->query_cache;

    /* 内存压力检查 */
    if (check_memory_pressure()) {
        query_cache_evict_expired(qc);
        if (check_memory_pressure()) return;  /* 仍然压力大, 不缓存 */
    }

    /* 找空槽或最旧的条目 */
    int slot = -1;
    time_t oldest = TIME_MAX;
    for (int i = 0; i < QUERY_CACHE_CAPACITY; i++) {
        if (!qc->entries[i].valid) {
            slot = i;
            break;
        }
        if (qc->entries[i].timestamp < oldest) {
            oldest = qc->entries[i].timestamp;
            slot = i;  /* 备选: 替换最旧的 */
        }
    }

    if (slot < 0) return;

    /* 如果覆盖旧条目, 先释放 */
    if (qc->entries[slot].valid) {
        free(qc->entries[slot].out_ra);
        free(qc->entries[slot].out_dec);
        free(qc->entries[slot].out_mag);
        qc->count--;
    }

    /* 分配并拷贝数据 */
    size_t ra_size = (size_t)out_count * sizeof(double);
    size_t dec_size = (size_t)out_count * sizeof(double);
    size_t mag_size = (size_t)out_count * sizeof(float);

    qc->entries[slot].out_ra = (double *)malloc(ra_size);
    qc->entries[slot].out_dec = (double *)malloc(dec_size);
    qc->entries[slot].out_mag = (float *)malloc(mag_size);

    if (!qc->entries[slot].out_ra || !qc->entries[slot].out_dec || !qc->entries[slot].out_mag) {
        free(qc->entries[slot].out_ra);
        free(qc->entries[slot].out_dec);
        free(qc->entries[slot].out_mag);
        return;
    }

    memcpy(qc->entries[slot].out_ra, out_ra, ra_size);
    memcpy(qc->entries[slot].out_dec, out_dec, dec_size);
    memcpy(qc->entries[slot].out_mag, out_mag, mag_size);

    qc->entries[slot].ra = round_to(ra, QUERY_CACHE_RA_ROUND);
    qc->entries[slot].dec = round_to(dec, QUERY_CACHE_DEC_ROUND);
    qc->entries[slot].radius = round_to(radius, QUERY_CACHE_RAD_ROUND);
    qc->entries[slot].mag_high = round_to(mag_high, QUERY_CACHE_MAG_ROUND);
    qc->entries[slot].out_count = out_count;
    qc->entries[slot].timestamp = time(NULL);
    qc->entries[slot].valid = 1;
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

static void parse_attr_after(const char *start, const char *attr, char *out, int out_size) {
    out[0] = '\0';
    char pattern[256];
    snprintf(pattern, sizeof(pattern), "%s=", attr);
    const char *a = strstr(start, pattern);
    if (!a) return;
    a += strlen(pattern);
    if (*a == '"') {
        a++;
        const char *end = strchr(a, '"');
        if (end) {
            int len = (int)(end - a);
            if (len >= out_size) len = out_size - 1;
            memcpy(out, a, len);
            out[len] = '\0';
        }
    } else if (*a == '\'') {
        a++;
        const char *end = strchr(a, '\'');
        if (end) {
            int len = (int)(end - a);
            if (len >= out_size) len = out_size - 1;
            memcpy(out, a, len);
            out[len] = '\0';
        }
    }
}

static int parse_int_after(const char *start, const char *attr, int def) {
    char buf[128];
    parse_attr_after(start, attr, buf, sizeof(buf));
    return buf[0] ? atoi(buf) : def;
}

static uint64_t parse_uint64_after(const char *start, const char *attr, uint64_t def) {
    char buf[128];
    parse_attr_after(start, attr, buf, sizeof(buf));
    return buf[0] ? strtoull(buf, NULL, 10) : def;
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

static void byte_unshuffle(uint8_t *data, size_t data_len, int item_size) {
    if (item_size <= 1 || data_len == 0) return;
    size_t n = data_len / item_size;
    if (n == 0) return;
    uint8_t *tmp = (uint8_t *)malloc(data_len);
    if (!tmp) return;
    for (int i = 0; i < item_size; i++) {
        size_t src_start = (size_t)i * n;
        for (size_t j = 0; j < n; j++) {
            tmp[j * item_size + i] = data[src_start + j];
        }
    }
    memcpy(data, tmp, data_len);
    free(tmp);
}

static uint32_t find_max_block_size(QTNode *nodes, int node_count) {
    uint32_t max_bs = 0;
    for (int i = 0; i < node_count; i++) {
        if (nodes[i].is_leaf && nodes[i].block_size > max_bs)
            max_bs = nodes[i].block_size;
    }
    return max_bs;
}

/* ===== 修改后的read_leaf_block: 优先查缓存 ===== */
static uint8_t *read_leaf_block(XPSDFileInternal *xf, uint64_t block_offset,
                                 uint32_t compressed_size, uint32_t block_size,
                                 uint8_t *scratch) {
    if (!xf->mmap_data || block_size == 0) return NULL;

    /* 1. 查解压块缓存 */
    uint32_t cached_size = 0;
    uint8_t *cached = block_cache_lookup(&xf->block_cache, block_offset, &cached_size);
    if (cached && cached_size == block_size) {
        return cached;  /* 缓存命中, 直接返回 */
    }

    /* 2. 缓存未命中, 执行解压 */
    const uint8_t *comp = xf->mmap_data + xf->data_position + block_offset;

    if (compressed_size == block_size) {
        memcpy(scratch, comp, block_size);
        if (xf->use_byte_shuffle && xf->item_size > 1)
            byte_unshuffle(scratch, block_size, xf->item_size);
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

        if (xf->use_byte_shuffle && xf->item_size > 1)
            byte_unshuffle(scratch, block_size, xf->item_size);
    }

    /* 3. 存入解压块缓存 */
    block_cache_insert(&xf->block_cache, block_offset, scratch, block_size);

    /* 4. 返回缓存中的数据 (确保后续读的是缓存指针) */
    cached = block_cache_lookup(&xf->block_cache, block_offset, &cached_size);
    if (cached && cached_size == block_size) {
        return cached;
    }

    return scratch;  /* 回退: 返回scratch (缓存插入失败时) */
}

static int load_xpsd_file(XPSDFileInternal *xf, const char *path) {
    memset(xf, 0, sizeof(*xf));
    strncpy(xf->filepath, path, sizeof(xf->filepath) - 1);

#ifdef _WIN32
    xf->hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (xf->hFile == INVALID_HANDLE_VALUE) return -1;
    LARGE_INTEGER fsize;
    GetFileSizeEx(xf->hFile, &fsize);
    xf->mmap_size = (size_t)fsize.QuadPart;
    xf->hMap = CreateFileMappingA(xf->hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!xf->hMap) { CloseHandle(xf->hFile); return -1; }
    xf->mmap_data = (uint8_t *)MapViewOfFile(xf->hMap, FILE_MAP_READ, 0, 0, 0);
    if (!xf->mmap_data) { CloseHandle(xf->hMap); CloseHandle(xf->hFile); return -1; }
#else
    xf->fd = open(path, O_RDONLY);
    if (xf->fd < 0) return -1;
    struct stat st;
    fstat(xf->fd, &st);
    xf->mmap_size = st.st_size;
    xf->mmap_data = mmap(NULL, xf->mmap_size, PROT_READ, MAP_PRIVATE, xf->fd, 0);
    if (xf->mmap_data == MAP_FAILED) { close(xf->fd); return -1; }
#endif

    if (xf->mmap_size < 16 || memcmp(xf->mmap_data, XPSD_MAGIC, 8) != 0) return -1;

    uint32_t header_len;
    memcpy(&header_len, xf->mmap_data + 8, 4);

    const char *xml = (const char *)(xf->mmap_data + 16);

    const char *data_tag = find_tag(xml, "Data");
    if (data_tag) {
        char mags[64];
        parse_attr_after(data_tag, "magnitudeRange", mags, sizeof(mags));
        if (mags[0]) {
            char *comma = strchr(mags, ',');
            if (comma) { *comma = '\0'; xf->magnitude_low = atof(mags); xf->magnitude_high = atof(comma + 1); }
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
                else if (strncmp(p, "spectrumCount=", 14) == 0) { xf->spectrum_count = atoi(p + 14); }
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
            if (!ti->nodes) break;
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

    /* 初始化解压块缓存 */
    block_cache_init(&xf->block_cache);

    return 0;
}

static void close_xpsd_file(XPSDFileInternal *xf) {
    /* 释放解压块缓存 */
    block_cache_free(&xf->block_cache);

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
}

static void collector_push(StarCollector *sc, double ra, double dec, double magG) {
    if (sc->count >= sc->capacity) {
        int new_cap = sc->capacity * 2;
        SimpleStar *new_stars = (SimpleStar *)realloc(sc->stars, new_cap * sizeof(SimpleStar));
        if (!new_stars) return;
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
    sc->stars = (SpectrumStar *)malloc(capacity * sizeof(SpectrumStar));
    sc->spectra = (uint8_t *)malloc((size_t)capacity * spectrum_count);
    sc->count = 0;
    sc->capacity = capacity;
    sc->spectrum_count = spectrum_count;
}

static void spec_collector_free(SpectrumStarCollector *sc) {
    free(sc->stars);
    free(sc->spectra);
    sc->stars = NULL;
    sc->spectra = NULL;
    sc->count = 0;
}

static void spec_collector_push(SpectrumStarCollector *sc, double ra, double dec,
                                 double magG,
                                 const uint8_t *spectrum) {
    if (sc->count >= sc->capacity) {
        int new_cap = sc->capacity * 2;
        SpectrumStar *new_stars = (SpectrumStar *)realloc(sc->stars, new_cap * sizeof(SpectrumStar));
        if (!new_stars) return;
        sc->stars = new_stars;
        uint8_t *new_spectra = (uint8_t *)realloc(sc->spectra, (size_t)new_cap * sc->spectrum_count);
        if (!new_spectra) return;
        sc->spectra = new_spectra;
        sc->capacity = new_cap;
    }
    sc->stars[sc->count].ra = ra;
    sc->stars[sc->count].dec = dec;
    sc->stars[sc->count].magG = magG;
    if (spectrum && sc->spectrum_count > 0)
        memcpy(sc->spectra + (size_t)sc->count * sc->spectrum_count, spectrum, sc->spectrum_count);
    sc->count++;
}

static void phot_collector_init(PhotometryStarCollector *pc, int capacity) {
    pc->stars = (PhotometryStar *)malloc(capacity * sizeof(PhotometryStar));
    pc->count = 0;
    pc->capacity = capacity;
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
        PhotometryStar *new_stars = (PhotometryStar *)realloc(pc->stars, new_cap * sizeof(PhotometryStar));
        if (!new_stars) return;
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
                              StarCollector *sc, uint8_t *scratch) {
    if (sc->count >= MAX_STARS_RESULT) return;
    QTNode *node = &tree->nodes[node_idx];

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

    if (!bbox_intersects(ra, dec, radius_deg, ra_min, ra_max, dec_min, dec_max))
        return;

    if (node->is_leaf) {
        uint8_t *data = read_leaf_block(xf, node->block_offset, node->compressed_size,
                                         node->block_size, scratch);
        if (!data) return;

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
                                               cos_dec_q, sin_dec_q, cos_radius, sc, scratch);
        if (node->child_ne) search_recursive(xf, tree, node->child_ne, ra, dec, radius_deg,
                                               mag_low, mag_high, cos_ra_q, sin_ra_q,
                                               cos_dec_q, sin_dec_q, cos_radius, sc, scratch);
        if (node->child_sw) search_recursive(xf, tree, node->child_sw, ra, dec, radius_deg,
                                               mag_low, mag_high, cos_ra_q, sin_ra_q,
                                               cos_dec_q, sin_dec_q, cos_radius, sc, scratch);
        if (node->child_se) search_recursive(xf, tree, node->child_se, ra, dec, radius_deg,
                                               mag_low, mag_high, cos_ra_q, sin_ra_q,
                                               cos_dec_q, sin_dec_q, cos_radius, sc, scratch);
    }
}

static void search_recursive_spectrum(XPSDFileInternal *xf, TreeInfo *tree, uint32_t node_idx,
                              double ra, double dec, double radius_deg,
                              double mag_low, double mag_high,
                              double cos_ra_q, double sin_ra_q,
                              double cos_dec_q, double sin_dec_q,
                              double cos_radius,
                              SpectrumStarCollector *sc, uint8_t *scratch) {
    if (sc->count >= MAX_STARS_RESULT) return;
    QTNode *node = &tree->nodes[node_idx];

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

    if (!bbox_intersects(ra, dec, radius_deg, ra_min, ra_max, dec_min, dec_max))
        return;

    if (node->is_leaf) {
        uint8_t *data = read_leaf_block(xf, node->block_offset, node->compressed_size,
                                         node->block_size, scratch);
        if (!data) return;

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
                if (xf->has_spectrum) {
                    spectrum = p + 40;
                }
                spec_collector_push(sc, s_ra, s_dec, magG, spectrum);
            }
        }
    } else {
        if (node->child_nw) search_recursive_spectrum(xf, tree, node->child_nw, ra, dec, radius_deg,
                                               mag_low, mag_high, cos_ra_q, sin_ra_q,
                                               cos_dec_q, sin_dec_q, cos_radius, sc, scratch);
        if (node->child_ne) search_recursive_spectrum(xf, tree, node->child_ne, ra, dec, radius_deg,
                                               mag_low, mag_high, cos_ra_q, sin_ra_q,
                                               cos_dec_q, sin_dec_q, cos_radius, sc, scratch);
        if (node->child_sw) search_recursive_spectrum(xf, tree, node->child_sw, ra, dec, radius_deg,
                                               mag_low, mag_high, cos_ra_q, sin_ra_q,
                                               cos_dec_q, sin_dec_q, cos_radius, sc, scratch);
        if (node->child_se) search_recursive_spectrum(xf, tree, node->child_se, ra, dec, radius_deg,
                                               mag_low, mag_high, cos_ra_q, sin_ra_q,
                                               cos_dec_q, sin_dec_q, cos_radius, sc, scratch);
    }
}

static void search_recursive_photometry(XPSDFileInternal *xf, TreeInfo *tree, uint32_t node_idx,
                              double ra, double dec, double radius_deg,
                              double mag_low, double mag_high,
                              double cos_ra_q, double sin_ra_q,
                              double cos_dec_q, double sin_dec_q,
                              double cos_radius,
                              PhotometryStarCollector *pc, uint8_t *scratch) {
    if (pc->count >= MAX_STARS_RESULT) return;
    QTNode *node = &tree->nodes[node_idx];

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

    if (!bbox_intersects(ra, dec, radius_deg, ra_min, ra_max, dec_min, dec_max))
        return;

    if (node->is_leaf) {
        uint8_t *data = read_leaf_block(xf, node->block_offset, node->compressed_size,
                                         node->block_size, scratch);
        if (!data) return;

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
                                               cos_dec_q, sin_dec_q, cos_radius, pc, scratch);
        if (node->child_ne) search_recursive_photometry(xf, tree, node->child_ne, ra, dec, radius_deg,
                                               mag_low, mag_high, cos_ra_q, sin_ra_q,
                                               cos_dec_q, sin_dec_q, cos_radius, pc, scratch);
        if (node->child_sw) search_recursive_photometry(xf, tree, node->child_sw, ra, dec, radius_deg,
                                               mag_low, mag_high, cos_ra_q, sin_ra_q,
                                               cos_dec_q, sin_dec_q, cos_radius, pc, scratch);
        if (node->child_se) search_recursive_photometry(xf, tree, node->child_se, ra, dec, radius_deg,
                                               mag_low, mag_high, cos_ra_q, sin_ra_q,
                                               cos_dec_q, sin_dec_q, cos_radius, pc, scratch);
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

#ifdef _WIN32
    char pattern[1024];
    snprintf(pattern, sizeof(pattern), "%s\\*.xpsd", data_dir);
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(pattern, &fd);
    if (hFind == INVALID_HANDLE_VALUE) { free(client); return NULL; }
    do {
        if (client->file_count >= MAX_FILES) break;
        char fullpath[1024];
        snprintf(fullpath, sizeof(fullpath), "%s\\%s", data_dir, fd.cFileName);
        if (load_xpsd_file(&client->files[client->file_count], fullpath) == 0) {
            if (file_matches_db_type(&client->files[client->file_count], db_type)) {
                client->file_count++;
            } else {
                close_xpsd_file(&client->files[client->file_count]);
            }
        }
    } while (FindNextFileA(hFind, &fd));
    FindClose(hFind);
#else
    DIR *dir = opendir(data_dir);
    if (!dir) { free(client); return NULL; }
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL && client->file_count < MAX_FILES) {
        size_t len = strlen(ent->d_name);
        if (len > 5 && strcmp(ent->d_name + len - 5, ".xpsd") == 0) {
            char fullpath[1024];
            snprintf(fullpath, sizeof(fullpath), "%s/%s", ent->d_name);
            if (load_xpsd_file(&client->files[client->file_count], fullpath) == 0) {
                if (file_matches_db_type(&client->files[client->file_count], db_type)) {
                    client->file_count++;
                } else {
                    close_xpsd_file(&client->files[client->file_count]);
                }
            }
        }
    }
    closedir(dir);
#endif

    if (client->file_count > 0) {
        int is_dr3sp = (strstr(client->files[0].db_identifier, "GaiaDR3SP") != NULL);
        client->db_type_detected = is_dr3sp ? GAIA_DB_DR3SP : GAIA_DB_DR3;
    }

    return client;
}

void gaia_client_destroy(GaiaClient *client) {
    if (!client) return;

    /* 释放查询结果缓存 */
    query_cache_free(&client->query_cache);

    for (int i = 0; i < client->file_count; i++)
        close_xpsd_file(&client->files[i]);

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
    int nfiles = client->file_count;
    if (nfiles == 0) { *out_stars = NULL; *out_count = 0; return 0; }

    /* ===== 查询结果缓存检查 ===== */
    cache_lock(client);
    double *cached_ra = NULL, *cached_dec = NULL;
    float *cached_mag = NULL;
    int cached_count = 0;
    if (query_cache_lookup(client, ra, dec, radius_deg, mag_high,
                            &cached_ra, &cached_dec, &cached_mag, &cached_count)) {
        /* 缓存命中: 构造GaiaStar数组返回 */
        *out_stars = (GaiaStar *)malloc(cached_count * sizeof(GaiaStar));
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
    for (int i = 0; i < nfiles; i++) collector_init(&sc_arr[i], 4096);

    #pragma omp parallel for schedule(dynamic) num_threads(16)
    for (int f = 0; f < nfiles; f++) {
        XPSDFileInternal *xf = &client->files[f];
        uint32_t scratch_size = xf->global_max_block_size;
        if (scratch_size == 0) scratch_size = 65536;
        uint8_t *scratch = (uint8_t *)malloc(scratch_size);
        if (!scratch) continue;

        for (int t = 0; t < xf->tree_count; t++) {
            if (xf->trees[t].node_count > 0 && xf->trees[t].nodes)
                search_recursive(xf, &xf->trees[t], 0, ra, dec, radius_deg,
                                  mag_low, mag_high, cos_ra_q, sin_ra_q,
                                  cos_dec_q, sin_dec_q, cos_radius, &sc_arr[f], scratch);
        }
        free(scratch);
    }

    int total = 0;
    for (int f = 0; f < nfiles; f++) total += sc_arr[f].count;

    if (total == 0) {
        for (int f = 0; f < nfiles; f++) collector_free(&sc_arr[f]);
        free(sc_arr);
        *out_stars = NULL; *out_count = 0;
        return 0;
    }

    *out_stars = (GaiaStar *)malloc(total * sizeof(GaiaStar));
    *out_count = total;
    int idx = 0;

    /* 构建缓存数据 (ra/dec/mag数组) */
    double *cache_ra = (double *)malloc(total * sizeof(double));
    double *cache_dec = (double *)malloc(total * sizeof(double));
    float *cache_mag = (float *)malloc(total * sizeof(float));

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
                cache_mag[idx] = (float)sc_arr[f].stars[i].magG;
            }
            idx++;
        }
        collector_free(&sc_arr[f]);
    }
    free(sc_arr);

    /* ===== 存入查询结果缓存 ===== */
    if (cache_ra && cache_dec && cache_mag) {
        cache_lock(client);
        query_cache_insert(client, ra, dec, radius_deg, mag_high,
                           cache_ra, cache_dec, cache_mag, total);
        cache_unlock(client);
    }
    /* 注意: query_cache_insert会拷贝数据, 这里释放临时数组 */
    free(cache_ra);
    free(cache_dec);
    free(cache_mag);

    return 0;
}

int gaia_client_cone_search_for_solver(GaiaClient *client, double ra, double dec, double radius_deg,
                                        double mag_high,
                                        double **out_ra, double **out_dec, float **out_mag,
                                        int *out_count) {
    GaiaStar *stars = NULL;
    int count = 0;
    int ret = gaia_client_cone_search(client, ra, dec, radius_deg, -1.5, mag_high, &stars, &count);
    if (ret != 0 || count == 0) {
        *out_ra = NULL; *out_dec = NULL; *out_mag = NULL; *out_count = 0;
        return ret;
    }

    *out_ra = (double *)malloc(count * sizeof(double));
    *out_dec = (double *)malloc(count * sizeof(double));
    *out_mag = (float *)malloc(count * sizeof(float));
    *out_count = count;

    for (int i = 0; i < count; i++) {
        (*out_ra)[i] = stars[i].ra;
        (*out_dec)[i] = stars[i].dec;
        (*out_mag)[i] = (float)stars[i].magG;
    }
    free(stars);
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

int gaia_client_cone_search_with_spectrum(
    GaiaClient *client,
    double ra, double dec, double radius_deg,
    double mag_low, double mag_high,
    GaiaSpectrumStar **out_stars,
    uint8_t **out_spectra,
    int *out_count) {
    int nfiles = client->file_count;
    if (nfiles == 0) {
        *out_stars = NULL;
        *out_spectra = NULL;
        *out_count = 0;
        return 0;
    }

    double cos_ra_q = cos(ra * DEG2RAD);
    double sin_ra_q = sin(ra * DEG2RAD);
    double cos_dec_q = cos(dec * DEG2RAD);
    double sin_dec_q = sin(dec * DEG2RAD);
    double cos_radius = cos(radius_deg * DEG2RAD);

    SpectrumStarCollector *sc_arr = (SpectrumStarCollector *)calloc(nfiles, sizeof(SpectrumStarCollector));
    for (int i = 0; i < nfiles; i++) {
        int spec_count = 0;
        if (client->files[i].has_spectrum) {
            spec_count = client->files[i].spectrum_count;
            if (spec_count == 0) spec_count = WL_COUNT;
        }
        spec_collector_init(&sc_arr[i], 4096, spec_count);
    }

    #pragma omp parallel for schedule(dynamic) num_threads(16)
    for (int f = 0; f < nfiles; f++) {
        XPSDFileInternal *xf = &client->files[f];
        uint32_t scratch_size = xf->global_max_block_size;
        if (scratch_size == 0) scratch_size = 65536;
        uint8_t *scratch = (uint8_t *)malloc(scratch_size);
        if (!scratch) continue;

        for (int t = 0; t < xf->tree_count; t++) {
            if (xf->trees[t].node_count > 0 && xf->trees[t].nodes)
                search_recursive_spectrum(xf, &xf->trees[t], 0, ra, dec, radius_deg,
                                  mag_low, mag_high, cos_ra_q, sin_ra_q,
                                  cos_dec_q, sin_dec_q, cos_radius, &sc_arr[f], scratch);
        }
        free(scratch);
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

    *out_stars = (GaiaSpectrumStar *)malloc(total * sizeof(GaiaSpectrumStar));
    *out_count = total;

    if (any_spectrum) {
        *out_spectra = (uint8_t *)malloc((size_t)total * global_spec_count);
    } else {
        *out_spectra = NULL;
    }

    int idx = 0;
    for (int f = 0; f < nfiles; f++) {
        for (int i = 0; i < sc_arr[f].count; i++) {
            (*out_stars)[idx].ra = sc_arr[f].stars[i].ra;
            (*out_stars)[idx].dec = sc_arr[f].stars[i].dec;
            (*out_stars)[idx].magG = sc_arr[f].stars[i].magG;

            if (*out_spectra && sc_arr[f].spectrum_count > 0) {
                int copy_count = sc_arr[f].spectrum_count;
                if (copy_count > global_spec_count) copy_count = global_spec_count;
                memcpy(*out_spectra + (size_t)idx * global_spec_count,
                       sc_arr[f].spectra + (size_t)i * sc_arr[f].spectrum_count,
                       copy_count);
            }
            idx++;
        }
        spec_collector_free(&sc_arr[f]);
    }
    free(sc_arr);

    return 0;
}

int gaia_client_cone_search_with_photometry(
    GaiaClient *client,
    double ra, double dec, double radius_deg,
    double mag_low, double mag_high,
    GaiaPhotometryStar **out_stars,
    int *out_count) {
    int nfiles = client->file_count;
    if (nfiles == 0) {
        *out_stars = NULL;
        *out_count = 0;
        return 0;
    }

    double cos_ra_q = cos(ra * DEG2RAD);
    double sin_ra_q = sin(ra * DEG2RAD);
    double cos_dec_q = cos(dec * DEG2RAD);
    double sin_dec_q = sin(dec * DEG2RAD);
    double cos_radius = cos(radius_deg * DEG2RAD);

    PhotometryStarCollector *pc_arr = (PhotometryStarCollector *)calloc(nfiles, sizeof(PhotometryStarCollector));
    for (int i = 0; i < nfiles; i++) {
        phot_collector_init(&pc_arr[i], 4096);
    }

    #pragma omp parallel for schedule(dynamic) num_threads(16)
    for (int f = 0; f < nfiles; f++) {
        XPSDFileInternal *xf = &client->files[f];
        uint32_t scratch_size = xf->global_max_block_size;
        if (scratch_size == 0) scratch_size = 65536;
        uint8_t *scratch = (uint8_t *)malloc(scratch_size);
        if (!scratch) continue;

        for (int t = 0; t < xf->tree_count; t++) {
            if (xf->trees[t].node_count > 0 && xf->trees[t].nodes)
                search_recursive_photometry(xf, &xf->trees[t], 0, ra, dec, radius_deg,
                                  mag_low, mag_high, cos_ra_q, sin_ra_q,
                                  cos_dec_q, sin_dec_q, cos_radius, &pc_arr[f], scratch);
        }
        free(scratch);
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

    *out_stars = (GaiaPhotometryStar *)malloc(total * sizeof(GaiaPhotometryStar));
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
