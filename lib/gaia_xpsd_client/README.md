# Gaia DR3 XPSD Client

轻量级 Gaia DR3 本地星表 C 客户端，直接读取 PixInsight XPSD 格式离线星表文件，支持锥形搜索（cone search）和多数据库选择。

## 特性

- **零网络依赖**：纯本地文件读取，无需在线 API，离线可用
- **XPSD 格式原生解析**：完整解析 PixInsight GaiaDR3/GaiaDR3SP `.xpsd` 文件格式（四叉树空间索引 + LZ4/Zlib 压缩数据块）
- **多数据库支持**：支持 GaiaDR3（完整版，18亿星）和 GaiaDR3SP（光谱版，2亿星）切换
- **多文件并行搜索**：自动加载目录下所有 `.xpsd` 文件，OpenMP 多线程并行搜索
- **mmap 零拷贝读取**：Windows (MapViewOfFile) / Linux (mmap) 内存映射，大文件无需全量读入
- **二级缓存加速**：查询结果缓存（60s TTL）+ 解压块缓存（进程级持久），重复查询近乎零耗时
- **内存压力自适应**：自动检测可用物理内存，不足时按 LRU 策略淘汰缓存
- **投影反变换支持**：内置 Equirectangular / Azimuthal Equidistant 投影反变换
- **线程安全**：缓存读写加锁保护，多线程并发安全
- **跨平台**：Windows (MSVC / MinGW) / Linux (GCC) / macOS (Clang)
- **单头文件 + 单源文件**：`gaia_client.h` + `gaia_client.c`，集成到任何 C 项目只需拷贝两个文件

## 数据源

本客户端需要 PixInsight 的 Gaia DR3 离线星表文件（`.xpsd` 格式）。

**GaiaDR3（完整版）**：约18亿颗星，32字节/星，LZ4压缩
**GaiaDR3SP（光谱版）**：约2亿颗星（有光谱数据），Zlib压缩

**数据下载：**
- 百度网盘：https://pan.baidu.com/s/1u8CCMtecsaiz2nVjLsThRg?pwd=fujz
  提取码：fujz

将所有 `.xpsd` 文件放在同一目录下，客户端会自动扫描并加载。

## 编译

### MinGW (Windows)

```bash
# 动态库
gcc -O2 -march=native -shared -o gaia_client.dll src/gaia_client.c -Isrc -lz -fopenmp

# 静态库
ar rcs libgaia_client.a src/gaia_client.c
```

### MSVC (Windows)

```cmd
cl /O2 /LD src/gaia_client.c /Isrc zlib.lib /openmp
```

### Linux / macOS

```bash
# 动态库
gcc -O2 -march=native -shared -fPIC -o libgaia_client.so src/gaia_client.c -Isrc -lz -fopenmp

# 静态库
ar rcs libgaia_client.a src/gaia_client.o
```

## API 参考

### 数据库类型枚举

```c
typedef enum {
    GAIA_DB_AUTO = 0,   // 自动检测（默认）
    GAIA_DB_DR3 = 1,    // GaiaDR3 完整版
    GAIA_DB_DR3SP = 2   // GaiaDR3SP 光谱版
} GaiaDbType;
```

### 创建 / 销毁客户端

```c
#include "gaia_client.h"

/* 创建客户端（自动检测数据库类型） */
GaiaClient *client = gaia_client_create("/path/to/GaiaDR3SP");

/* 创建客户端（指定数据库类型） */
GaiaClient *client = gaia_client_create_ex("/path/to/GaiaDR3", GAIA_DB_DR3);

if (!client) {
    fprintf(stderr, "Failed to load XPSD files\n");
    return -1;
}

/* 获取客户端信息 */
int db_type = gaia_client_get_db_type(client);      // 返回 GAIA_DB_DR3 或 GAIA_DB_DR3SP
int file_count = gaia_client_get_file_count(client); // 加载的文件数
int total_sources = gaia_client_get_total_sources(client); // 总星数

/* 销毁客户端，释放所有资源（包括 mmap 映射和缓存） */
gaia_client_destroy(client);
```

### 锥形搜索（完整结果）

```c
GaiaStar *stars = NULL;
int count = 0;

/* 参数: client, ra(度), dec(度), radius(度), mag_low, mag_high, &stars, &count */
int ret = gaia_client_cone_search(client, 266.4167, -28.9867, 1.0, -1.5, 14.0, &stars, &count);

if (ret == 0) {
    for (int i = 0; i < count; i++) {
        printf("ra=%.6f dec=%.6f magG=%.3f parallax=%.3f\n",
               stars[i].ra, stars[i].dec, stars[i].magG, stars[i].parallax);
    }
}
free(stars);  /* 调用者负责释放 */
```

### 锥形搜索（精简接口，仅 ra/dec/mag）

```c
double *ra_arr, *dec_arr;
float  *mag_arr;
int count;

/* 适用于天体测量匹配，仅返回三个数组，减少内存分配 */
gaia_client_cone_search_for_solver(client, 266.4, -28.9, 1.0, 14.0,
                                    &ra_arr, &dec_arr, &mag_arr, &count);

free(ra_arr);
free(dec_arr);
free(mag_arr);
```

### 数据结构

```c
typedef struct {
    double ra;          /* 赤经 (度, ICRS J2016) */
    double dec;         /* 赤纬 (度, ICRS J2016) */
    double magG;        /* G 波段星等 */
    double magBP;       /* BP 波段星等 */
    double magRP;       /* RP 波段星等 */
    float  parallax;    /* 视差 (mas) */
    float  pmra;        /* 赤经自行 (mas/yr) */
    float  pmdec;       /* 赤纬自行 (mas/yr) */
    int64_t source_id;  /* Gaia DR3 source_id */
} GaiaStar;
```

## 缓存机制

客户端内置二级缓存，大幅提升重复查询性能：

### 查询结果缓存（L1）

- **TTL**: 60秒，过期自动清理
- **容量**: 最多64个查询结果
- **键**: 舍入后的 (RA, Dec, radius, mag_high)
- **效果**: 相同参数的重复查询近乎零耗时
- **适用场景**: 同一天区多帧 plate solving（如同一 panel 的 Red/Green/Blue/H-alpha/Oiii 帧）

### 解压块缓存（L2）

- **生命周期**: 进程级持久，直到客户端销毁
- **容量**: 8192个哈希槽，最大4GB
- **键**: 数据块在文件中的偏移量
- **效果**: 不同 mag_limit 查询同天区时，已解压的数据块直接复用，跳过 LZ4/Zlib 解压
- **适用场景**: `bisection_mag_limit` 多次查询同一区域时，后续查询显著加速

### 内存压力自适应

- 自动检测系统可用物理内存（Windows: `GlobalMemoryStatusEx`, Linux: `/proc/meminfo`）
- 可用内存 < 4GB 时触发 LRU 淘汰，释放最旧的缓存条目
- 解压块缓存超过 4GB 时自动淘汰最旧 1/4 条目

### 缓存性能实测

| 场景 | 无缓存 | 有缓存 | 加速 |
|------|--------|--------|------|
| 同参数重复查询 | 0.82s | <0.001s | >800x |
| 同天区不同 mag_limit | 0.82s | 0.01-0.05s | 16-82x |
| bisection 7次查询 | 5.88s | 0.06s | 93x |
| Panel1 53帧批量 (同天区) | 496s | 200s | 2.5x |

## XPSD 文件格式

XPSD 是 PixInsight 专用的天文星表格式，内部结构：

```
XPSD File
├── 文件头 (魔数 + 版本 + 元数据)
├── 四叉树索引 (递归空间划分，每个节点记录 RA/Dec 范围 + 数据偏移)
├── 投影参数 (Equirectangular 或 Azimuthal Equidistant)
└── 数据块 (LZ4 或 Zlib 压缩，每块包含若干 GaiaStar 记录)
```

客户端工作流程：
1. 扫描目录，打开所有 `.xpsd` 文件
2. mmap 映射每个文件到内存
3. 解析文件头和四叉树索引
4. 锥形搜索时：查L1缓存 → 查L2解压块缓存 → 遍历四叉树剪枝 → 解压命中数据块 → 投影反变换 → 星等过滤 → 存入缓存 → 返回结果

## 性能参考

| 操作 | 条件 | 耗时 |
|------|------|------|
| 加载 16 个 DR3 XPSD 文件 | SSD, 16线程 | ~3s |
| 加载 20 个 DR3SP XPSD 文件 | SSD, 16线程 | ~2s |
| 锥形搜索 (1° 半径, mag<14.6) | 16线程, 冷启动 | ~0.5s |
| 锥形搜索 (1° 半径, mag<14.6) | 16线程, 缓存命中 | <0.001s |
| 锥形搜索 (6° 半径, mag<8.5) | 16线程, 冷启动 | ~0.8s |
| 锥形搜索 (6° 半径, mag<8.5) | 16线程, 缓存命中 | <0.001s |
| 内存占用 (缓存空) | 16个DR3文件 | 仅 mmap 映射 |
| 内存占用 (缓存满) | 16个DR3文件 | mmap + 最大4GB解压块缓存 |

## 数据库对比

| 属性 | GaiaDR3 | GaiaDR3SP |
|------|---------|-----------|
| 总星数 | ~18亿 | ~2亿 |
| 条目大小 | 32 bytes | 可变 |
| 压缩方式 | LZ4-HC + shuffle | Zlib + shuffle |
| 光谱数据 | 无 | 有 |
| 适用场景 | Plate solving | 光谱分析 |

## 依赖

| 依赖 | 用途 | 链接选项 |
|------|------|----------|
| zlib | XPSD 数据块解压 | `-lz` |
| OpenMP | 多线程并行搜索 (可选) | `-fopenmp` |

## 目录结构

```
gaia_xpsd_client/
├── src/
│   ├── gaia_client.h    # 公共 API 头文件
│   └── gaia_client.c    # 完整实现（含缓存）
├── python/
│   ├── verify_dr3.py    # DR3格式验证脚本
│   └── test_multi_db.py # 多数据库测试脚本
├── example/
│   └── demo.c           # 使用示例
├── Makefile             # 编译脚本
└── README.md            # 本文件
```

## 许可

MIT License
