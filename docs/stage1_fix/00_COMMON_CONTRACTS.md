# Stage1/HISS 修复 - 公共约定（所有子代理必须遵守）

> 本文件定义所有子代理共同依赖的接口契约、数据结构、文件规范和验收标准。
> 任何子代理不得擅自修改本文件的接口定义。如有变更需求，必须回到主代理讨论。

## 1. 模块边界

### 1.1 新增模块（按职责独立文件）

| 模块 | 文件路径 | 职责 |
|------|---------|------|
| Tile几何 | `lib/astro_image_io/src/hiss_tile_model.h/.cpp` | Tile父子模型、叶像素计算、local_ipix映射 |
| 球面重叠 | `lib/healpix_db/healpix_drizzle/spherical_overlap.h/.cpp` | 球面多边形裁剪、球面面积计算、候选像素查询 |
| Transform | `lib/astro_image_io/src/hiss_transform.h/.cpp` | byte-shuffle/delta/varint正式路径 |
| 流式写入器 | `lib/astro_image_io/src/hiss_stream_writer.h/.cpp` | 流式临时子块池、原子提交 |
| 测光应用 | `lib/calibration/src/photometry_apply.h/.cpp` | Gaia测光比例应用 |

### 1.2 增量修复模块（现有文件）

| 模块 | 文件路径 | 修复内容 |
|------|---------|---------|
| Drizzle引擎 | `lib/healpix_db/healpix_drizzle/drizzle_engine.cpp/.h` | 球面重叠替换切平面、候选像素查询、NSIDE上限、pixfrac校验 |
| HISS Writer | `lib/astro_image_io/src/hiss_writer.cpp` | signal/support语义、流式写入、BITMAP/SPARSE有效数据、occupancy自动选择 |
| HISS Reader | `lib/astro_image_io/src/hiss_reader.cpp` | SNR布局、未知必需子块拒绝、checksum完整实现 |
| HISS Common | `lib/astro_image_io/src/hiss_common.cpp` | signal=累计通量、support=面积比 |
| HISS Codec | `lib/astro_image_io/src/hiss_codec.cpp` | checksum完整实现、transform集成 |
| Drizzle引擎WCS | `lib/healpix_db/healpix_drizzle/wcs_sip.cpp` | 移除完整WCS写入HISS元数据 |
| 旧aio_healpix_io | `lib/astro_image_io/src/healpix/aio_healpix_io.cpp` | 改造为新HissWriter/Reader后端 |
| CLI | `lib/orchestrator/cpp/src/cli_command.cpp` | stage1参数、诊断、HissWriter接入 |
| Browser | `lib/healpix_db/healpix_browser_qt/` | 按Tile查看signal/support/SNR |

## 2. 核心数据结构（冻结，子代理不得修改）

### 2.1 Tile几何（依据02_FROZEN §11）

```cpp
struct HissTileGeometry {
    uint32_t nside;            // 全局NSIDE
    uint32_t tile_nside;      // Tile父级NSIDE (>=16)
    int      depth;           // d = min(9, log2(NSIDE/16))
    uint32_t n_leaf_per_tile; // 4^d = (NSIDE/tile_nside)^2，单Tile叶像素数
    uint64_t parent_ipix;    // Tile父像素NESTED ipix

    // 计算local_ipix在全局ipix中的位置
    uint64_t local_to_global(uint32_t local_ipix) const;
    uint32_t global_to_local(uint64_t global_ipix) const;
};

// 构造Tile几何
HissTileGeometry make_tile_geometry(uint32_t nside);
// depth = min(9, log2(nside/16))
// tile_nside = nside / 2^depth
// n_leaf_per_tile = 4^depth
```

**关键公式**：
- `n_leaf_per_tile = (NSIDE / NSIDE_tile)² = (2^d)² = 4^d`
- 不是 `tile_nside² × 12`（那是全天像素数）

### 2.2 Drizzle累加器（依据02_FROZEN §8, §10）

```cpp
struct PixelAccumulator {
    double sumFlux = 0.0;     // Σ L_j * (a_jp / A_j_drop) — 累计通量
    double sumArea = 0.0;     // Σ a_jp — 球面重叠面积（未归一化）
    uint32_t nContrib = 0;    // 贡献源像素数（诊断用）
};

// signal输出（02_FROZEN §8）：
//   signal[p] = float(sumFlux)  — 直接保存累计通量，不除面积

// support输出（02_FROZEN §10）：
//   S_p = sumArea / A_p  — A_p是目标HEALPix像素面积
//   support[p] = uint8(round(255 * clamp(S_p, 0.0, 1.0)))
```

**关键语义**：
- `signal` = 累计通量（不是平均面亮度）
- `support` = 覆盖面积比（0~1，除以A_p）
- `sumArea` = 球面重叠面积（Drizzle和HISS统一语义）

### 2.3 球面重叠接口

```cpp
namespace spherical {

// 球面多边形顶点
struct Vec3 { double x, y, z; };

// 计算球面多边形面积（球面excess公式）
double spherical_polygon_area(const std::vector<Vec3>& vertices);

// 计算源像素drop与目标HEALPix像素的球面重叠面积
// drop_corners: drop球面多边形顶点（已通过WCS/SIP映射到球面）
// hp: HEALPix核心
// target_ipix: 目标像素NESTED ipix
// 返回: 球面重叠面积（球面度）
double compute_overlap_area(
    const std::vector<Vec3>& drop_corners,
    const healpix::HealpixCore& hp,
    uint64_t target_ipix
);

// 查询与drop多边形可能相交的所有HEALPix像素（不限于1-ring）
// drop_corners: drop球面多边形顶点
// hp: HEALPix核心
// candidates: 输出候选像素列表
void query_candidate_pixels(
    const std::vector<Vec3>& drop_corners,
    const healpix::HealpixCore& hp,
    std::vector<uint64_t>& candidates
);

} // namespace spherical
```

### 2.4 HISS Writer接口（改造后）

```cpp
class HissWriter {
public:
    // 打开输出文件
    int open(const std::string& output_path, const HissGridSpec& grid,
             const HissMetadata& metadata);

    // 流式添加Tile（压缩后立即写入临时池，不保留在内存）
    // signal: n_leaf_per_tile个float值（累计通量）
    // support: n_leaf_per_tile个uint8值（覆盖率）
    // occupancy: 可选，nullptr则Writer自动判断
    // sparse_indices: SPARSE_LIST模式下的有效像素局部索引
    // n_valid: 有效像素数（BITMAP/SPARSE模式）
    int add_tile(uint64_t parent_ipix,
                 const float* signal,        // FULL: n_leaf_per_tile; BITMAP/SPARSE: n_valid
                 const uint8_t* support,     // FULL: n_leaf_per_tile; BITMAP/SPARSE: n_valid
                 const uint32_t* sparse_indices, // SPARSE: n_valid; 否则nullptr
                 uint32_t n_valid,            // FULL: n_leaf_per_tile; BITMAP/SPARSE: 有效数
                 const HissSnrBlock* snr);

    // 设置实验codec（正式API不暴露，仅实验用）
    void set_experiment_codec(SubblockType type, CodecId codec, TransformId transform);

    // finalize：生成Header + 原子重命名
    int finalize();

    // 取消
    void cancel();
};
```

**关键变更**：
- `add_tile`不再由调用方传入`OccupancyMode`，Writer根据数据自动选择
- signal/support在BITMAP/SPARSE模式下只保存有效像素，不是全长度数组
- 压缩后立即写入临时池，不保留全部Tile在内存

### 2.5 SNR控制点（依据02_FROZEN §17）

```cpp
// SNR控制点（精简，每点仅2字段）
struct HissSnrControlPoint {
    uint32_t local_ipix;  // Tile内局部索引
    float    snr;         // SNR值
};

// SNR子块布局（冻结）：
// [n_points: uint32]
// [points: n_points * 8B]  — 每点 local_ipix(uint32) + snr(float32)
// 不包含snr_phot/median_snr/idw_power（这些是估计器状态，不写入HISS）
```

### 2.6 子块目录（依据02_FROZEN §15）

```cpp
struct SubblockDescriptor {
    uint32_t block_type;       // signal/support/occupancy/snr
    uint32_t flags;            // required/optional
    uint64_t offset;           // 文件内偏移
    uint64_t compressed_size;  // 压缩后大小
    uint64_t uncompressed_size;// 压缩前大小
    uint16_t codec_id;         // RAW/LZ4/ZSTD
    uint16_t transform_id;     // NONE/SHUFFLE/DELTA/DELTA_VARINT
    uint8_t  checksum_type;    // NONE/CRC32C/XXHASH32
    uint8_t  checksum[32];     // 校验值（最多256bit，当前用4B）
};
```

## 3. 文件与目录规范

### 3.1 新增文件目录结构

```
lib/astro_image_io/
├─ src/
│  ├─ hiss_tile_model.h/.cpp      # 新增：Tile几何
│  ├─ hiss_transform.h/.cpp       # 新增：transform正式路径
│  ├─ hiss_stream_writer.h/.cpp   # 新增：流式写入器
│  ├─ hiss_writer.cpp             # 增量修复
│  ├─ hiss_reader.cpp             # 增量修复
│  ├─ hiss_codec.cpp              # 增量修复
│  └─ hiss_common.cpp             # 增量修复

lib/healpix_db/healpix_drizzle/
├─ spherical_overlap.h/.cpp       # 新增：球面重叠
├─ drizzle_engine.cpp/.h          # 增量修复
└─ wcs_sip.cpp                    # 增量修复

lib/calibration/src/
├─ photometry_apply.h/.cpp        # 新增：测光比例应用
└─ dark_optimizer.cpp             # 保持

lib/astro_image_io/tests/
├─ test_tile_model.cpp            # 新增：Tile模型测试
├─ test_spherical_overlap.cpp     # 新增：球面重叠测试
├─ test_hiss_writer.cpp           # 新增：Writer测试（重写）
├─ test_hiss_reader.cpp           # 新增：Reader测试（重写）
└─ test_drizzle_integration.cpp    # 新增：Drizzle集成测试
```

### 3.2 日志规范

```cpp
// 统一日志宏（已有项目规范）
#define HISS_LOG_ERROR(fmt, ...) fprintf(stderr, "[HISS ERROR] " fmt "\n", ##__VA_ARGS__)
#define HISS_LOG_INFO(fmt, ...)  fprintf(stdout, "[HISS INFO] " fmt "\n", ##__VA_ARGS__)
#define HISS_LOG_DEBUG(fmt, ...) // debug模式下启用

// 每个模块建立日志目录
// lib/astro_image_io/logs/
// lib/healpix_db/healpix_drizzle/logs/
// lib/calibration/logs/
```

### 3.3 错误码

```cpp
// HISS错误码（负值）
#define HISS_OK                 0
#define HISS_ERR_INVALID_ARG   -1   // 非法参数（如pixfrac<=0）
#define HISS_ERR_INVALID_STATE -2   // 非法状态（如RING模式）
#define HISS_ERR_IO            -3   // I/O错误
#define HISS_ERR_CHECKSUM      -4   // 校验失败
#define HISS_ERR_FORMAT       -5   // 格式错误
#define HISS_ERR_UNSUPPORTED  -6   // 不支持的特性
#define HISS_ERR_UNKNOWN_REQUIRED -7 // 未知必需子块
```

## 4. 接口契约（子代理必须严格遵守）

### 4.1 pixfrac校验（步骤6）

```cpp
// drizzle_engine.cpp
bool DrizzleEngine::drizzle(...) {
    if (config.pixfrac <= 0.0 || config.pixfrac > 1.0) {
        error_msg = "pixfrac must be in (0, 1], got " + std::to_string(config.pixfrac);
        return false;  // 拒绝，不进入点采样快速路径
    }
    if (!config.nested) {
        error_msg = "HISS requires NESTED ordering, RING not supported";
        return false;
    }
    // ...
}
```

### 4.2 自动NSIDE（步骤5）

```cpp
// drizzle_engine.cpp
static const int NSIDE_MAX = 4194304;  // 2^22，支持0.1"输入
static const int NSIDE_MIN = 16;

int compute_auto_nside(const WcsParams& wcs, int img_w, int img_h) {
    // 网格采样（不是仅中心+四角5点）
    // 至少9×9网格采样，SIP畸变极值可能在边缘
    // 找到最细局部像素尺度
    // 选择最小2次幂NSIDE，使HEALPix线性像素尺度不粗于该最细尺度
    // 钳位 [NSIDE_MIN, NSIDE_MAX]
}
```

### 4.3 signal/support输出（步骤7）

```cpp
// hiss_common.cpp
void finalize_tile_data(
    const std::unordered_map<uint64_t, PixelAccumulator>& accumulators,
    const HissTileGeometry& tile_geom,
    const healpix::HealpixCore& hp,
    std::vector<float>& signal,       // 输出：累计通量
    std::vector<uint8_t>& support,    // 输出：覆盖率
    std::vector<uint32_t>& valid_indices, // 输出：有效像素索引
    uint32_t& n_valid                  // 输出：有效像素数
) {
    double A_p = hp.pixel_area(); // 目标HEALPix像素面积（球面度）
    for (auto& [ipix, acc] : accumulators) {
        // signal = 累计通量（不除面积）
        signal[i] = float(acc.sumFlux);
        // support = 面积比（0~1）
        double S = acc.sumArea / A_p;
        S = std::max(0.0, std::min(1.0, S)); // 仅浮点误差级钳制
        support[i] = uint8_t(std::round(255.0 * S));
    }
}
```

### 4.4 HISS元数据（不保存完整WCS）

```cpp
// hiss_writer.cpp
// 禁止写入的字段：
// - cd矩阵 / crval / crpix / sip_order / sip系数
// 允许保留的摘要字段：
// - 解算质量（如RMS、匹配星数）
// - 仅记录NSIDE/ORDERING/RADESYS/TILENSID/PIXFRAC用于定位
```

### 4.5 流式写入（步骤10）

```cpp
// hiss_stream_writer.cpp
class HissStreamWriter {
    // 临时子块池：每个Tile压缩后立即写入临时文件
    std::ofstream temp_pool_;  // .partial文件的临时池

    int add_tile(...) {
        // 1. 压缩signal/support/occupancy
        // 2. 计算checksum
        // 3. 立即写入temp_pool_
        // 4. 记录SubblockDescriptor到内存目录
        // 5. 释放Tile内存（不保留compressed_data）
    }

    int finalize() {
        // 1. 生成Header（含完整子块目录）
        // 2. 将Header写入最终文件开头
        // 3. 将temp_pool_内容追加到Header后
        // 4. flush + 原子重命名
    }
};
```

### 4.6 原子替换（安全）

```cpp
// Windows安全原子替换
int atomic_replace(const std::string& temp_path, const std::string& final_path) {
    // 使用MoveFileExW + MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH
    // 不能先删除旧文件再rename
#ifdef _WIN32
    return MoveFileExW(
        widen(temp_path).c_str(),
        widen(final_path).c_str(),
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH
    ) ? 0 : -1;
#else
    return rename(temp_path.c_str(), final_path.c_str());
#endif
}
```

## 5. 测试与验收标准

### 5.1 单元测试要求

- 每个模块独立测试，使用合成数据（确定性随机数）
- 契约不满足必须真正失败（禁止`ASSERT_TRUE(true, "已知问题")`）
- 测试框架维护正确的通过/失败计数

### 5.2 关键验收项

| 验收项 | 标准 | 测试方法 |
|--------|------|---------|
| Tile叶像素数 | n_leaf = 4^d | 检查NSIDE=64/tile_nside=16 → 16个叶像素 |
| signal语义 | signal = Σ L_j*(a_jp/A_j_drop) | 合成数据通量守恒（drop未截断时Σsignal=L_j） |
| support语义 | S = Σa_jp / A_p, 范围0~1 | 合成数据面积比验证 |
| 球面重叠 | 球面面积误差 < 1e-6 | 已知球面多边形面积验证 |
| 候选像素 | 不限于1-ring | 高NSIDE+大源像素测试 |
| NSIDE上限 | 支持2^22 | 0.1"输入验证 |
| pixfrac<=0 | 返回错误 | 参数校验 |
| RING | 返回错误 | 参数校验 |
| 通量守恒 | Σsignal ≈ L_j (误差<1e-5) | 合成数据drop未截断 |
| BITMAP/SPARSE | 只保存有效像素 | 稀疏数据体积 < FULL体积 |
| transform | Writer执行变换，Reader逆向 | byte-shuffle/delta往返 |
| SNR布局 | Writer/Reader一致 | 往返测试 |
| 未知必需块 | Reader拒绝 | 构造非法文件测试 |
| checksum | Writer生成，Reader校验 | CRC32C往返 |
| 流式写入 | 内存不保留全部Tile | 1000 Tile内存峰值测试 |
| 元数据 | 无完整WCS | 检查输出文件 |
| DrizzleEngine接入 | writeHis调用新Writer | 集成测试 |
| Gaia测光 | signal×photscal | 校准测试 |
| CLI | stage1生成HISS | 端到端测试 |

### 5.3 禁止事项

- 禁止用`ASSERT_TRUE(true)`软通过
- 禁止用Python代替C++实验
- 禁止自动运行710帧回归
- 禁止修改Stage2代码
- 禁止冻结实验结论为默认值
