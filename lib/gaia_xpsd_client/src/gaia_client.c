#include "gaia_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
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

typedef struct {
    char filepath[1024];
    char db_identifier[256];
    double magnitude_low, magnitude_high;
    int total_sources;
    int has_spectrum;
    int star_stride;
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
} XPSDFileInternal;

struct GaiaClient {
    XPSDFileInternal files[MAX_FILES];
    int file_count;
};

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

static uint8_t *read_leaf_block(XPSDFileInternal *xf, uint64_t block_offset,
                                 uint32_t compressed_size, uint32_t block_size,
                                 uint8_t *scratch) {
    if (!xf->mmap_data || block_size == 0) return NULL;
    const uint8_t *comp = xf->mmap_data + xf->data_position + block_offset;

    if (compressed_size == block_size) {
        memcpy(scratch, comp, block_size);
        if (xf->use_byte_shuffle && xf->item_size > 1)
            byte_unshuffle(scratch, block_size, xf->item_size);
        return scratch;
    }

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

    return scratch;
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

    return 0;
}

static void close_xpsd_file(XPSDFileInternal *xf) {
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

typedef struct {
    double ra, dec, magG;
} SimpleStar;

typedef struct {
    SimpleStar *stars;
    int count;
    int capacity;
} StarCollector;

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
        double corners_ra[4], corners_dec[4];
        double xs[4] = {node->x0, node->x1, node->x1, node->x0};
        double ys[4] = {node->y0, node->y0, node->y1, node->y1};
        for (int i = 0; i < 4; i++)
            unproject(xs[i], ys[i], tree->projection, tree->center_ra, tree->center_dec,
                       &corners_ra[i], &corners_dec[i]);
        ra_min = corners_ra[0]; ra_max = corners_ra[0];
        dec_min = corners_dec[0]; dec_max = corners_dec[0];
        for (int i = 1; i < 4; i++) {
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

GaiaClient *gaia_client_create(const char *data_dir) {
    GaiaClient *client = (GaiaClient *)calloc(1, sizeof(GaiaClient));
    if (!client) return NULL;

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
        if (load_xpsd_file(&client->files[client->file_count], fullpath) == 0)
            client->file_count++;
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
            snprintf(fullpath, sizeof(fullpath), "%s/%s", data_dir, ent->d_name);
            if (load_xpsd_file(&client->files[client->file_count], fullpath) == 0)
                client->file_count++;
        }
    }
    closedir(dir);
#endif

    return client;
}

void gaia_client_destroy(GaiaClient *client) {
    if (!client) return;
    for (int i = 0; i < client->file_count; i++)
        close_xpsd_file(&client->files[i]);
    free(client);
}

int gaia_client_cone_search(GaiaClient *client, double ra, double dec, double radius_deg,
                             double mag_low, double mag_high,
                             GaiaStar **out_stars, int *out_count) {
    int nfiles = client->file_count;
    if (nfiles == 0) { *out_stars = NULL; *out_count = 0; return 0; }

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
    for (int f = 0; f < nfiles; f++) {
        for (int i = 0; i < sc_arr[f].count; i++) {
            (*out_stars)[idx].ra = sc_arr[f].stars[i].ra;
            (*out_stars)[idx].dec = sc_arr[f].stars[i].dec;
            (*out_stars)[idx].magG = sc_arr[f].stars[i].magG;
            (*out_stars)[idx].magBP = 0;
            (*out_stars)[idx].magRP = 0;
            (*out_stars)[idx].source_id = 0;
            idx++;
        }
        collector_free(&sc_arr[f]);
    }
    free(sc_arr);
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
