# Gaia DR3 XPSD Client

轻量级 Gaia DR3 本地星表 C 客户端，直接读取 PixInsight XPSD 格式离线星表文件，支持锥形搜索（cone search）。

## 特性

- **零网络依赖**：纯本地文件读取，无需在线 API，离线可用
- **XPSD 格式原生解析**：完整解析 PixInsight GaiaDR3SP `.xpsd` 文件格式（四叉树空间索引 + LZ4/Zlib 压缩数据块）
- **多文件并行搜索**：自动加载目录下所有 `.xpsd` 文件，OpenMP 多线程并行搜索
- **mmap 零拷贝读取**：Windows (MapViewOfFile) / Linux (mmap) 内存映射，大文件无需全量读入
- **投影反变换支持**：内置 Equirectangular / Azimuthal Equidistant 投影反变换
- **跨平台**：Windows (MSVC / MinGW) / Linux (GCC) / macOS (Clang)
- **单头文件 + 单源文件**：`gaia_client.h` + `gaia_client.c`，集成到任何 C 项目只需拷贝两个文件

## 数据源

本客户端需要 PixInsight 的 Gaia DR3 离线星表文件（`.xpsd` 格式）。

**数据下载（仅保留百度网盘）：**
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

### 创建 / 销毁客户端

```c
#include "gaia_client.h"

/* 创建客户端，扫描目录下所有 .xpsd 文件并加载索引 */
GaiaClient *client = gaia_client_create("/path/to/GaiaDR3SP");
if (!client) {
    fprintf(stderr, "Failed to load XPSD files\n");
    return -1;
}

/* 销毁客户端，释放所有资源（包括 mmap 映射） */
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
4. 锥形搜索时：遍历四叉树剪枝 → 解压命中数据块 → 投影反变换 → 星等过滤 → 返回结果

## 性能参考

| 操作 | 条件 | 耗时 |
|------|------|------|
| 加载 20 个 XPSD 文件 (~5GB) | SSD, 16线程 | ~3s |
| 锥形搜索 (1° 半径, mag<14.6) | 16线程 | ~0.5s |
| 锥形搜索 (0.5° 半径, mag<13.0) | 16线程 | ~0.2s |
| 内存占用 | 20个文件 | 仅 mmap 映射，无额外缓存 |

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
│   └── gaia_client.c    # 完整实现
├── example/
│   └── demo.c           # 使用示例
├── Makefile             # 编译脚本
└── README.md            # 本文件
```

## 许可

MIT License
