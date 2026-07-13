# Gaia DR3 XPSD Client

版本：v1.0 | 2026-07-12

轻量级 Gaia DR3 本地星表 C 客户端，直接读取 PixInsight XPSD 格式离线星表文件，支持锥形搜索（cone search）与多数据库切换。

**版本 / 性能摘要**：支持 GaiaDR3（18亿星）/ GaiaDR3SP（2.2亿星）双数据库；1° 半径锥形搜索冷启动 ~0.5s、缓存命中 <0.001s；多文件 OpenMP 并行 + mmap 零拷贝 + 二级缓存。

## GitHub仓库
- 仓库地址：https://github.com/fujiaze/Gaia-DR3-DR3SP-Client-C
- 默认分支：master

---

## 概述

### 功能列表

- **零网络依赖**：纯本地文件读取，离线可用，无需在线 API
- **XPSD 格式原生解析**：完整解析 PixInsight `.xpsd` 文件（6 棵投影树空间索引 + LZ4/Zlib 压缩数据块）
- **多数据库支持**：GaiaDR3 完整版与 GaiaDR3SP 光谱版可切换，支持自动检测
- **多文件并行搜索**：自动加载目录下所有 `.xpsd` 文件，OpenMP 多线程并行
- **mmap 零拷贝读取**：Windows (MapViewOfFile) / Linux (mmap) 内存映射，大文件无需全量读入
- **二级缓存加速**：查询结果缓存（60s TTL）+ 解压块缓存（进程级持久），重复查询近乎零耗时
- **内存压力自适应**：自动检测可用物理内存，不足时按 LRU 策略淘汰缓存
- **投影反变换支持**：内置 Equirectangular / Azimuthal Equidistant 投影反变换
- **线程安全**：缓存读写加锁保护，多线程并发安全
- **跨平台**：Windows (MSVC / MinGW) / Linux (GCC) / macOS (Clang)

### 性能指标

| 操作 | 条件 | 耗时 |
|------|------|------|
| 加载 16 个 DR3 XPSD 文件 | SSD, 16线程 | ~3s |
| 加载 20 个 DR3SP XPSD 文件 | SSD, 16线程 | ~2s |
| 锥形搜索 (1° 半径, mag<14.6) | 16线程, 冷启动 | ~0.5s |
| 锥形搜索 (1° 半径, mag<14.6) | 16线程, 缓存命中 | <0.001s |
| 锥形搜索 (6° 半径, mag<8.5) | 16线程, 冷启动 | ~0.8s |
| 同参数重复查询 | 无缓存→有缓存 | 0.82s → <0.001s (>800x) |
| bisection 7次查询 | 无缓存→有缓存 | 5.88s → 0.06s (93x) |

### 数据库对比

| 属性 | GaiaDR3 (GAIA_DB_DR3) | GaiaDR3SP (GAIA_DB_DR3SP) |
|------|----------------------|---------------------------|
| 总星数 | ~18亿 | ~2.2亿 |
| 条目大小 | 32 bytes | 384 bytes (40头部+343光谱+1填充) |
| 压缩方式 | LZ4-HC + shuffle | Zlib + shuffle |
| 光谱数据 | 无 | 有 (336-1020nm, 343采样, uint8) |
| 光谱API | 不适用 | `gaia_client_cone_search_with_spectrum()` / `_with_photometry()` |
| 适用场景 | Plate solving | 光谱分析 |

---

## 使用方法

### 编译

**编译命令（MinGW, 生成 Windows DLL）**：

```bash
gcc -O2 -march=native -shared -o gaia_client.dll src/gaia_client.c -Isrc -lz -fopenmp -static-libgcc
```

**其他平台**：

```bash
# MSVC (Windows)
cl /O2 /LD src/gaia_client.c /Isrc zlib.lib /openmp

# Linux / macOS
gcc -O2 -march=native -shared -fPIC -o libgaia_client.so src/gaia_client.c -Isrc -lz -fopenmp
```

### Python 调用示例

通过 `ctypes` 加载 DLL，声明 `GaiaStar` 结构与 API 签名后即可调用：

```python
import ctypes, os

class GaiaStar(ctypes.Structure):
    _fields_ = [
        ('ra', ctypes.c_double), ('dec', ctypes.c_double),
        ('magG', ctypes.c_double), ('magBP', ctypes.c_double), ('magRP', ctypes.c_double),
        ('parallax', ctypes.c_float), ('pmra', ctypes.c_float), ('pmdec', ctypes.c_float),
        ('source_id', ctypes.c_int64),
    ]

GAIA_DB_DR3, GAIA_DB_DR3SP = 1, 2

dll = ctypes.CDLL(os.path.abspath('gaia_client.dll'))
dll.gaia_client_create_ex.argtypes = [ctypes.c_char_p, ctypes.c_int]
dll.gaia_client_create_ex.restype = ctypes.c_void_p
dll.gaia_client_cone_search.argtypes = [
    ctypes.c_void_p, ctypes.c_double, ctypes.c_double, ctypes.c_double,
    ctypes.c_double, ctypes.c_double,
    ctypes.POINTER(ctypes.POINTER(GaiaStar)), ctypes.POINTER(ctypes.c_int),
]
dll.gaia_client_cone_search.restype = ctypes.c_int
dll.gaia_client_destroy.argtypes = [ctypes.c_void_p]

client = dll.gaia_client_create_ex(b'/path/to/GaiaDR3', GAIA_DB_DR3)

stars = ctypes.POINTER(GaiaStar)()
count = ctypes.c_int()
dll.gaia_client_cone_search(client, 266.4167, -28.9867, 1.0, -1.5, 14.0,
                            ctypes.byref(stars), ctypes.byref(count))
for i in range(count.value):
    print(stars[i].ra, stars[i].dec, stars[i].magG)

dll.gaia_client_destroy(client)
```

### 核心 API

```c
typedef enum {
    GAIA_DB_AUTO = 0,   /* 自动检测（默认） */
    GAIA_DB_DR3 = 1,    /* GaiaDR3 完整版 */
    GAIA_DB_DR3SP = 2   /* GaiaDR3SP 光谱版 */
} GaiaDbType;

/* 创建客户端（指定数据库类型） */
GaiaClient *client = gaia_client_create_ex("/path/to/GaiaDR3", GAIA_DB_DR3);

/* 锥形搜索: client, ra, dec, radius(度), mag_low, mag_high, &stars, &count */
gaia_client_cone_search(client, 266.4167, -28.9867, 1.0, -1.5, 14.0, &stars, &count);

/* 精简接口（仅 ra/dec/mag，适用于 plate solving） */
gaia_client_cone_search_for_solver(client, 266.4, -28.9, 1.0, 14.0,
                                   &ra_arr, &dec_arr, &mag_arr, &count);
```

```c
typedef struct {
    double ra, dec, magG, magBP, magRP;
    float  parallax, pmra, pmdec;
    int64_t source_id;
} GaiaStar;
```

---

## 光谱与测光接口（DR3SP 专用）

DR3SP 数据库的每颗星记录为 384 字节 = 40 字节头部 + 343 字节光谱（uint8）+ 1 字节填充。
头部含 magG/magBP/magRP 星等，光谱覆盖 336nm-1020nm（步长 2nm，343 采样点）。

**API 按职责拆分**：光谱查询和测光（BP/RP）查询独立，按需调用，避免不必要的内存开销。

### 三级查询 API

| API | 返回内容 | 适用场景 |
|-----|---------|---------|
| `gaia_client_cone_search()` | ra/dec/magG | Plate solving |
| `gaia_client_cone_search_with_photometry()` | ra/dec/magG/**magBP/magRP** | 颜色分析（不读光谱，省内存） |
| `gaia_client_cone_search_with_spectrum()` | ra/dec/magG + **343光谱点** | 光谱分析 |

### 光谱 API

```c
typedef struct {
    double ra, dec, magG;
} GaiaSpectrumStar;

int gaia_client_cone_search_with_spectrum(
    GaiaClient *client,
    double ra, double dec, double radius_deg,
    double mag_low, double mag_high,
    GaiaSpectrumStar **out_stars,
    uint8_t **out_spectra,
    int *out_count);

int gaia_client_get_spectrum_params(
    GaiaClient *client,
    int *out_start_nm, int *out_step_nm, int *out_count);
/* 返回值: 1=有光谱, 0=无光谱 */
```

### 测光 API

```c
typedef struct {
    double ra, dec, magG, magBP, magRP;
} GaiaPhotometryStar;

int gaia_client_cone_search_with_photometry(
    GaiaClient *client,
    double ra, double dec, double radius_deg,
    double mag_low, double mag_high,
    GaiaPhotometryStar **out_stars,
    int *out_count);
```

**内存管理**：`out_stars` 和 `out_spectra` 由 `malloc` 分配，调用方需分别 `free`。

### Python 示例

```python
import ctypes

class GaiaSpectrumStar(ctypes.Structure):
    _fields_ = [('ra', ctypes.c_double), ('dec', ctypes.c_double), ('magG', ctypes.c_double)]

class GaiaPhotometryStar(ctypes.Structure):
    _fields_ = [('ra', ctypes.c_double), ('dec', ctypes.c_double),
                ('magG', ctypes.c_double), ('magBP', ctypes.c_double), ('magRP', ctypes.c_double)]

# --- 测光查询（BP/RP星等，不读光谱） ---
phot_stars = ctypes.POINTER(GaiaPhotometryStar)()
phot_count = ctypes.c_int()
dll.gaia_client_cone_search_with_photometry(
    client, 266.4167, -28.9867, 0.5, -2.0, 15.0,
    ctypes.byref(phot_stars), ctypes.byref(phot_count))
for i in range(phot_count.value):
    s = phot_stars[i]
    print(f"RA={s.ra:.4f}, magG={s.magG:.3f}, magBP={s.magBP:.3f}, magRP={s.magRP:.3f}")
ctypes.cdll.msvcrt.free(phot_stars)

# --- 光谱查询（343采样点，不含BP/RP） ---
spec_stars = ctypes.POINTER(GaiaSpectrumStar)()
spectra = ctypes.POINTER(ctypes.c_uint8)()
spec_count = ctypes.c_int()
dll.gaia_client_cone_search_with_spectrum(
    client, 266.4167, -28.9867, 0.5, -2.0, 15.0,
    ctypes.byref(spec_stars), ctypes.byref(spectra), ctypes.byref(spec_count))
for i in range(spec_count.value):
    s = spec_stars[i]
    offset = i * 343
    spectrum = [spectra[offset + j] for j in range(343)]  # 336nm-1020nm
    print(f"RA={s.ra:.4f}, magG={s.magG:.3f}, spectrum[:5]={spectrum[:5]}")
ctypes.cdll.msvcrt.free(spec_stars)
ctypes.cdll.msvcrt.free(spectra)
```

### DR3SP 星记录头部布局（40 字节）

| 偏移 | 大小 | 字段 | 类型 | 说明 |
|------|------|------|------|------|
| 0 | 4 | dx | uint32 | X 位置增量 |
| 4 | 4 | dy | uint32 | Y 位置增量 |
| 8-19 | 12 | (保留) | - | 未使用字段 |
| 20 | 2 | magG_raw | uint16 | magG = raw × 0.001 − 1.5 |
| 22 | 2 | magBP_raw | uint16 | magBP = raw × 0.001 − 1.5 |
| 24 | 2 | magRP_raw | uint16 | magRP = raw × 0.001 − 1.5 |
| 26 | 2 | dra_raw | int16 | RA 修正增量 |
| 28-39 | 12 | (保留) | - | 未使用字段 |
| 40-382 | 343 | spectrum | uint8[343] | BP/RP 光谱流量值 |
| 383 | 1 | (填充) | - | 对齐填充 |

---

## 架构

### XPSD 文件格式

XPSD 是 PixInsight 专用的天文星表格式，每个文件内部包含 **6 棵投影树**：

```
XPSD File
├── 文件头 (魔数 + 版本 + 元数据)
├── 6 棵投影树空间索引
│   ├── 4× Equirectangular 投影树 (覆盖赤道带)
│   └── 2× AzimuthalEquidistant 投影树 (覆盖南北极区)
├── 投影参数 (每棵树对应一种投影)
└── 数据块 (LZ4 或 Zlib 压缩，每块含若干 GaiaStar 记录)
```

**工作流程**：
1. 扫描目录，打开所有 `.xpsd` 文件并 mmap 映射
2. 解析文件头与 6 棵投影树索引
3. 锥形搜索：查 L1 缓存 → 查 L2 解压块缓存 → 遍历投影树剪枝 → 解压命中数据块 → 投影反变换 → 星等过滤 → 存入缓存 → 返回结果

#### 关键修复：bbox 极区漏检（2026-06-07）

**Bug**：AzimuthalEquidistant 投影树的 bbox 仅由 4 个角点确定。极区投影（中心±90°）4 个角点反投影后 Dec≈±26.4°，完全遗漏极点（Dec=±90°），导致 |Dec|>60° 区域所有查询被错误拒绝、返回 0 结果。

**修复**：在 `search_recursive()` 的 bbox 计算中增加第 5 个采样点 `(0.0, 0.0)`（投影中心即极点本身），确保极区覆盖正确。

```c
// 旧代码
double xs[4] = {node->x0, node->x1, node->x1, node->x0};
// 新代码
double xs[5] = {node->x0, node->x1, node->x1, node->x0, 0.0};
```

| 查询位置 | 修复前 | 修复后 |
|---------|--------|--------|
| 北天极 Dec=+90° | 0颗 | 8,927颗 |
| 南天极 Dec=-89° | 0颗 | 13,904颗 |
| 南天 Dec=-75° | 0颗 | 8,880颗 |

#### 二级缓存机制

| 缓存层级 | TTL / 生命周期 | 容量 | 键 | 效果 |
|---------|---------------|------|-----|------|
| L1 查询结果 | 60s | 64 条 | 舍入后 (RA,Dec,radius,mag_high) | 同参数重复查询近乎零耗时 |
| L2 解压块 | 进程级持久 | 8192 槽 / 最大 4GB | 数据块文件偏移 | 不同 mag_limit 同天区跳过解压 |

内存压力自适应：可用内存 <4GB 触发 LRU 淘汰；L2 超 4GB 淘汰最旧 1/4 条目。

### 目录结构

```
gaia_xpsd_client/
├── src/
│   ├── gaia_client.h              # 公共 API 头文件
│   └── gaia_client.c              # 完整实现（含缓存）
├── python/
│   ├── verify_dr3.py              # DR3 格式验证脚本
│   ├── verify_global_coverage.py  # 全天区覆盖验证脚本
│   ├── verify_spectrum.py         # 光谱接口验证测试（基建保留）
│   └── test_multi_db.py           # 多数据库测试脚本
├── example/
│   └── demo.c                     # 使用示例
├── Makefile                       # 编译脚本
└── README.md                      # 本文件
```

### 依赖

| 依赖 | 用途 | 链接选项 |
|------|------|----------|
| zlib | XPSD 数据块解压 (DR3SP) | `-lz` |
| lz4 | XPSD 数据块解压 (DR3) | `-llz4` |
| OpenMP | 多线程并行搜索（可选） | `-fopenmp` |

---

## 详细文档

- **GitHub (C 版)**：https://github.com/fujiaze/Gaia-DR3-DR3SP-Client-C
- **GitHub (Pyd 版)**：https://github.com/fujiaze/Gaia-DR3SP-Client-Pyd
- **数据下载（百度网盘）**：https://pan.baidu.com/s/1u8CCMtecsaiz2nVjLsThRg?pwd=fujz （提取码：fujz）

将所有 `.xpsd` 文件放在同一目录下，客户端会自动扫描并加载。

## 许可

MIT License
