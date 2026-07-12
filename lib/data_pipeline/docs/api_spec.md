# Data Pipeline C API 接口规范

本文档详细描述 PipelineFrame 和 PipelineEngine 的 C API 接口。

## 目录

1. [数据类型定义](#1-数据类型定义)
2. [PipelineFrame 接口](#2-pipelineframe-接口)
3. [PipelineEngine 接口](#3-pipelineengine-接口)
4. [缓存文件格式](#4-缓存文件格式)
5. [标准块定义](#5-标准块定义)
6. [块生命周期管理](#6-块生命周期管理)

---

## 1. 数据类型定义

### PipelineStage

管线阶段枚举，定义5个标准阶段：

```c
typedef enum {
    STAGE_CALIBRATE    = 0,  // 校准（本底/暗流/平场）
    STAGE_PLATESOLVE   = 1,  // 定标解算（WCS）
    STAGE_PHOTOMETRIC  = 2,  // 光度测量（星点检测+测光）
    STAGE_DRIZZLE      = 3,  // Drizzle 投影变换
    STAGE_STACK        = 4,  // 多帧叠加
} PipelineStage;
```

### AioBlockType

块数据类型枚举：

```c
typedef enum {
    AIO_BLOCK_FLOAT32 = 0,  // float 数组
    AIO_BLOCK_FLOAT64 = 1,  // double 数组
    AIO_BLOCK_INT32   = 2,  // int32 数组
    AIO_BLOCK_INT64   = 3,  // int64 数组
    AIO_BLOCK_STRING  = 4,  // UTF-8 字符串 (count=字节数)
    AIO_BLOCK_KV      = 5,  // key-value 对 (AioKVEntry 数组)
    AIO_BLOCK_RAW     = 6,  // 原始字节 (uint8_t 数组)
} AioBlockType;
```

### AioKVEntry

KV 条目结构，用于 header / cal_stats / photo_stats 等块：

```c
typedef struct {
    char key[64];    // 键名
    char value[256]; // 值（字符串形式）
} AioKVEntry;
```

### AioBlock

命名块结构：

```c
typedef struct {
    char          name[64];         // 块名 (如 "header", "data", "psf")
    AioBlockType  type;             // 数据类型
    void*         data;             // 数据指针 (frame 拥有, destroy 时释放)
    int64_t       count;            // 元素个数
    int           dims[4];          // 维度 (如 [H,W] 或 [N,4])
    int           n_dims;           // 维度数 (0 表示标量)
    char          description[128]; // 人类可读描述
} AioBlock;
```

### PipelineFrame

命名块容器结构：

```c
typedef struct {
    AioBlock* blocks;            // 块数组 (动态扩容)
    int       n_blocks;          // 当前块数量
    int       blocks_capacity;   // 块数组容量
    int       stages_completed;  // 阶段完成位掩码
} PipelineFrame;
```

### PipelineStageHandler

阶段处理函数签名（in-place 模式）：

```c
typedef int (*PipelineStageHandler)(PipelineFrame* frame,
                                     const void* params,
                                     char* error_msg, int error_capacity);
```

- `frame`: 输入/输出帧，handler 直接修改
- `params`: 阶段参数（调用方管理生命周期）
- `error_msg`: 错误信息输出缓冲区
- `error_capacity`: 错误缓冲区大小
- 返回: 0=成功, 非0=失败

---

## 2. PipelineFrame 接口

### 2.1 帧生命周期

#### aio_pipeline_frame_create

```c
PipelineFrame* aio_pipeline_frame_create(void);
```

创建新的空帧。

- 参数: 无
- 返回: 新帧指针，失败返回 NULL

#### aio_pipeline_frame_destroy

```c
void aio_pipeline_frame_destroy(PipelineFrame* frame);
```

销毁帧，释放所有块数据及块数组。

- 参数: `frame` - 帧指针
- 返回: 无

#### aio_pipeline_frame_memory_usage

```c
size_t aio_pipeline_frame_memory_usage(const PipelineFrame* frame);
```

计算帧占用的总内存（所有块数据 + 块数组）。

- 参数: `frame` - 帧指针
- 返回: 内存字节数

### 2.2 块管理

#### aio_frame_add_block

```c
int aio_frame_add_block(PipelineFrame* frame,
    const char* name, AioBlockType type,
    const void* data, int64_t count,
    const int* dims, int n_dims,
    const char* description);
```

添加块（拷贝数据到 frame 内部 buffer）。若同名块已存在则覆盖。

- `name`: 块名（最长63字符）
- `type`: 数据类型
- `data`: 源数据指针（NULL 则分配零初始化内存）
- `count`: 元素个数
- `dims`: 维度数组（可为 NULL）
- `n_dims`: 维度数（最多4）
- `description`: 描述（可为 NULL）
- 返回: 0=成功, 1=参数错误, 2=count<0, 3=扩容失败, 4=未知类型, 5=内存分配失败

#### aio_frame_add_block_move

```c
int aio_frame_add_block_move(PipelineFrame* frame,
    const char* name, AioBlockType type,
    void* data, int64_t count,
    const int* dims, int n_dims,
    const char* description);
```

添加块（转移数据所有权，frame 接管 free）。data 必须是 malloc 分配的。

- 返回: 0=成功, 非0=失败（同上）

#### aio_frame_get_block

```c
const AioBlock* aio_frame_get_block(const PipelineFrame* frame, const char* name);
```

获取块（只读指针）。

- 返回: 块指针，不存在返回 NULL

#### aio_frame_get_block_data

```c
void* aio_frame_get_block_data(const PipelineFrame* frame, const char* name);
```

获取块数据指针（便捷函数）。

- 返回: 数据指针，不存在返回 NULL

#### aio_frame_get_block_count

```c
int64_t aio_frame_get_block_count(const PipelineFrame* frame, const char* name);
```

获取块元素个数。

- 返回: 元素个数，不存在返回 -1

#### aio_frame_get_block_type

```c
int aio_frame_get_block_type(const PipelineFrame* frame, const char* name);
```

获取块数据类型。

- 返回: AioBlockType 枚举值，不存在返回 -1

#### aio_frame_remove_block

```c
int aio_frame_remove_block(PipelineFrame* frame, const char* name);
```

移除并释放块数据。

- 返回: 0=成功, 1=不存在

#### aio_frame_has_block

```c
int aio_frame_has_block(const PipelineFrame* frame, const char* name);
```

检查块是否存在。

- 返回: 1=存在, 0=不存在

#### aio_frame_list_blocks

```c
int aio_frame_list_blocks(const PipelineFrame* frame,
    char* out_names, int capacity, int* out_count);
```

列出所有块名。

- `out_names`: 输出缓冲区，每个块名占 64 字节
- `capacity`: 缓冲区可容纳的块名数
- `out_count`: 实际块数量（即使缓冲区不够也输出）
- 返回: 0=成功, 1=参数错误, 2=缓冲区不足

### 2.3 KV 块操作

#### aio_frame_kv_set

```c
int aio_frame_kv_set(PipelineFrame* frame, const char* block_name,
    const char* key, const char* value);
```

设置 KV 块中某 key 的 value（字符串）。若块不存在则自动创建 KV 块。key 已存在则覆盖。

- 返回: 0=成功, 非0=失败

#### aio_frame_kv_get

```c
const char* aio_frame_kv_get(const PipelineFrame* frame, const char* block_name,
    const char* key);
```

获取 KV 块中某 key 的 value。

- 返回: value 字符串指针，不存在返回 NULL

#### aio_frame_kv_set_double

```c
int aio_frame_kv_set_double(PipelineFrame* frame, const char* block_name,
    const char* key, double value);
```

设置 KV 块中某 key 的 value（double 自动转字符串 "%.17g"）。

- 返回: 0=成功, 非0=失败

#### aio_frame_kv_get_double

```c
double aio_frame_kv_get_double(const PipelineFrame* frame, const char* block_name,
    const char* key, double default_value);
```

获取 KV 块中某 key 的 value（字符串转 double）。

- `default_value`: 不存在或转换失败时返回的默认值
- 返回: double 值

### 2.4 缓存文件 (.aio)

#### aio_frame_save_cache

```c
int aio_frame_save_cache(const PipelineFrame* frame, const char* path);
```

保存所有块到 .aio 缓存文件（自定义二进制格式）。

- 返回: 0=成功, 1=参数错误, 2=文件打开失败, 3=写入失败

#### aio_frame_load_cache

```c
int aio_frame_load_cache(PipelineFrame* frame, const char* path);
```

从 .aio 缓存文件加载所有块（清除现有块后加载）。

- 返回: 0=成功, 1=参数错误, 2=文件打开失败, 3=读取/格式错误

### 2.5 调试导出

#### aio_frame_export_block_fits

```c
int aio_frame_export_block_fits(const PipelineFrame* frame,
    const char* block_name, const char* path);
```

导出单个块为 FITS 文件（仅 FLOAT32/FLOAT64/INT32/INT64）。

- dims 解释: 1D=[N], 2D=[H,W], 3D=[H,W,C]
- 返回: 0=成功, 1=参数错误, 2=块不存在, 3=块无数据, 4=类型不支持, 5=文件打开失败

#### aio_frame_export_block_xml

```c
int aio_frame_export_block_xml(const PipelineFrame* frame,
    const char* block_name, const char* path);
```

导出单个块为 XML 文件（任意类型，含元数据 + base64 数据）。

- 返回: 0=成功, 1=参数错误, 2=块不存在, 3=文件打开失败, 4=写入不完整

#### aio_frame_export_all_xml

```c
int aio_frame_export_all_xml(const PipelineFrame* frame, const char* path);
```

导出所有块为 XML 文件。

- 返回: 0=成功, 1=参数错误, 2=文件打开失败, 3=写入不完整

#### aio_pipeline_export_xml

```c
int aio_pipeline_export_xml(const PipelineFrame* frame,
    const char* path, const char* comment);
```

旧版兼容包装，等价于 aio_frame_export_all_xml。comment 参数忽略。

---

## 3. PipelineEngine 接口

### 3.1 引擎生命周期

#### aio_pipeline_engine_create

```c
PipelineEngine* aio_pipeline_engine_create(void);
```

创建新引擎。默认 auto_free=1（阶段后自动丢弃块）。

- 返回: 引擎指针，失败返回 NULL

#### aio_pipeline_engine_destroy

```c
void aio_pipeline_engine_destroy(PipelineEngine* engine);
```

销毁引擎，释放内部资源（含自定义块丢弃策略字符串）。

### 3.2 阶段注册

#### aio_pipeline_engine_register

```c
int aio_pipeline_engine_register(PipelineEngine* engine,
    PipelineStage stage,
    PipelineStageHandler handler,
    const void* params);
```

注册阶段处理函数。

- `stage`: 阶段枚举 (STAGE_CALIBRATE ~ STAGE_STACK)
- `handler`: 处理函数（nullptr 表示跳过该阶段）
- `params`: 阶段参数（引擎不拥有，调用方管理生命周期）
- 返回: 0=成功, -1=引擎为空, -2=阶段无效

### 3.3 配置

#### aio_pipeline_engine_set_debug

```c
int aio_pipeline_engine_set_debug(PipelineEngine* engine,
    const char* dir, int stage_mask, int skip_pixels);
```

设置调试导出。

- `dir`: 导出目录（nullptr 或空串则不导出）
- `stage_mask`: 导出阶段位掩码（bit0=calibrate后, bit1=solve后, ...）, -1=所有阶段
- `skip_pixels`: 1=跳过像素数据, 0=导出全部
- 返回: 0=成功, -1=引擎为空

#### aio_pipeline_engine_set_auto_free

```c
int aio_pipeline_engine_set_auto_free(PipelineEngine* engine, int auto_free);
```

设置自动释放（默认开启）。

- `auto_free`: 1=阶段后自动丢弃块, 0=保留所有块
- 返回: 0=成功, -1=引擎为空

#### aio_pipeline_engine_set_block_drop

```c
int aio_pipeline_engine_set_block_drop(PipelineEngine* engine,
    PipelineStage stage, const char* block_names);
```

自定义某阶段后要丢弃的块（覆盖默认策略）。

- `block_names`: 逗号分隔的块名列表（如 "weight,psf"），nullptr 或空串表示不丢弃
- 注意: 调用后 auto_free 对该阶段不再生效
- 返回: 0=成功, -1=引擎为空, -2=阶段无效, -3=内存分配失败

### 3.4 执行

#### aio_pipeline_engine_run_single

```c
int aio_pipeline_engine_run_single(PipelineEngine* engine,
    PipelineFrame* frame,
    int from_stage, int to_stage,
    char* error_msg, int error_capacity);
```

单帧串行执行。

- `frame`: 输入帧（已填充数据）
- `from_stage`: 起始阶段 (0~4)
- `to_stage`: 结束阶段 (0~4)
- `error_msg`/`error_capacity`: 错误信息输出
- 返回: 0=成功, -1=参数错误, -2=阶段范围无效, 其他=阶段handler返回的错误码

#### aio_pipeline_engine_run_batch

```c
int aio_pipeline_engine_run_batch(PipelineEngine* engine,
    PipelineFrame** frames, int n_frames,
    int n_threads,
    int from_stage, int to_stage,
    char* error_msg, int error_capacity);
```

批量并行执行（OpenMP）。

- `frames`: 帧指针数组
- `n_frames`: 帧数
- `n_threads`: 线程数（默认16, <=0 则用16）
- 执行策略: CALIBRATE→DRIZZLE 并行, STACK 串行
- 返回: 成功帧数（若全部成功则 == n_frames）

#### aio_pipeline_stage_name

```c
const char* aio_pipeline_stage_name(PipelineStage stage);
```

获取阶段名称字符串。

- 返回: "calibrate" / "platesolve" / "photometric" / "drizzle" / "stack" / "unknown"

---

## 4. 缓存文件格式 (.aio)

### 文件头

| 偏移 | 字段 | 类型 | 说明 |
|------|------|------|------|
| 0 | Magic | char[4] | "AIO1" |
| 4 | Version | int32 | 1 |
| 8 | N_Blocks | int32 | 块数量 |
| 12 | Stages_Completed | int32 | 阶段完成位掩码 |

### 逐块数据（重复 N_Blocks 次）

| 字段 | 类型 | 说明 |
|------|------|------|
| Name_Len | int32 | 块名长度 |
| Name | bytes[Name_Len] | 块名 |
| Type | int32 | AioBlockType 枚举 |
| N_Dims | int32 | 维度数 |
| Dims | int32×N_Dims | 各维度大小 |
| Count | int64 | 元素个数 |
| Data | 变长 | 块数据（见下） |
| Desc_Len | int32 | 描述长度 |
| Desc | bytes[Desc_Len] | 描述文本 |

### Data 格式

- **KV 块**: 连续的 `[Key_Len][Key][Val_Len][Val]` 条目（Count 个）
- **其他类型**: 原始字节（Count × 元素大小）

---

## 5. 标准块定义

| 块名 | 类型 | dims | 内容 |
|------|------|------|------|
| header | KV | [N_kv] | FITS头 + WCS + SIP (key-value) |
| data | FLOAT32 | [H,W] | 图像像素数据 |
| weight | FLOAT32 | [H,W] | 权重图 |
| snr | FLOAT32 | [H,W] | 信噪比图 |
| psf | FLOAT64 | [6] | PSF 模型参数 |
| star_det | FLOAT32 | [N,4] | 星点检测 (x,y,flux,mag) |
| gaia_cat | FLOAT64 | [N,3] | Gaia 星表 (ra,dec,mag) |
| grad_map | FLOAT32 | [H,W] | 梯度图 M_map |
| cal_stats | KV | [N_kv] | 校准统计信息 |
| photo_stats | KV | [N_kv] | 光度统计信息 |
| healpix | RAW | [N] | HEALPix 数据包 |

注: 块名大小写敏感。未列出的自定义块名也允许。

---

## 6. 块生命周期管理

引擎按阶段自动丢弃中间块以释放内存（auto_free=1 时）：

| 阶段完成后 | 丢弃的块 |
|------------|----------|
| PLATESOLVE | weight |
| PHOTOMETRIC | star_det, gaia_cat, psf |
| DRIZZLE | data, snr, weight, grad_map, cal_stats, photo_stats |
| STACK | healpix |

- `header` 块在整个管线生命周期内保留（含元数据+WCS+SIP）
- 可通过 `aio_pipeline_engine_set_block_drop` 自定义丢弃策略
- 可通过 `aio_pipeline_engine_set_auto_free(0)` 禁用自动丢弃
