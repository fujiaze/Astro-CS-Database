# HEALpix Drizzle 引擎概述

## 用途
drizzle 引擎负责把标准化 CCD 图像(.ahpx 格式)的像素数据按 WCS 投射到 HEALpix 球面像素上，实现图像配准和重采样。这是连接单帧存储与球面堆栈数据库的关键环节。

## 算法概述

### 输入
- .ahpx 单帧文件(含像素数据 + WCS + SNR + 权重)
- 目标 HEALpix 参数(nside, 嵌套/RING scheme)

### 输出
- 像素映射表: 每个 HEALpix 像素对应的 CCD 像素列表
- 重采样值: 每个 HEALpix 像素的值(加权平均)
- 重采样 SNR: 每个 HEALpix 像素的 SNR(传播计算)
- 重采样权重: 每个 HEALpix 像素的最终权重

### 核心算法
1. WCS 像素坐标 → 天球坐标(RA/Dec)转换
2. 天球坐标 → HEALpix 像素号(ang2pix)
3. drizzle 重采样:
   - 对每个 CCD 像素，计算其在 HEALpix 球面上的覆盖区域
   - 按覆盖面积加权分配到目标 HEALpix 像素
   - 支持 drop点(传统 drizzle)和 企鹅(覆盖面积)两种模式
4. SNR 传播: 按 drizzle 权重传播每像素 SNR

### 重采样模式
- 点采样(drop): 每个 CCD 像素中心投射到 1 个 HEALpix 像素
- 面积加权(企鹅): 每个 CCD 像素按覆盖面积分配到多个 HEALpix 像素
- Lanczos: 高质量重采样(可选，计算量大)

## 输入/输出接口定义

### C API (预定义)
```c
// drizzle 单帧到 HEALpix 像素
int hp_drizzle_frame(
    const char* ahpx_path,        // 输入 .ahpx 文件路径
    int nside,                     // 目标 HEALpix nside
    int nested,                    // 1=NESTED, 0=RING
    uint64_t** out_pix,            // 输出: HEALpix 像素号数组
    float** out_values,            // 输出: 重采样值数组
    float** out_snr,               // 输出: 重采样 SNR 数组
    float** out_weights,           // 输出: 重采样权重数组
    int* out_count                 // 输出: 像素数量
);

// 批量 drizzle 多帧
int hp_drizzle_batch(
    const char** ahpx_paths,       // 输入文件路径数组
    int frame_count,
    int nside,
    int nested,
    const char* output_db_path     // 输出堆栈数据库路径
);
```

### Python 接口 (预定义)
```python
def drizzle_frame(ahpx_path: str, nside: int, nested: bool = True) -> DrizzleResult:
    """drizzle 单帧到 HEALpix 像素
    
    Returns:
        DrizzleResult: 含 pix/values/snr/weights 数组
    """

def drizzle_batch(ahpx_paths: list[str], nside: int, db_path: str, nested: bool = True) -> None:
    """批量 drizzle 多帧并直接更新堆栈数据库"""
```

## 与 healpix_stack 的对接点

1. drizzle 输出 → healpix_stack 的堆栈更新接口
2. drizzle 计算每像素权重 → healpix_stack 的 sigma-clip 加权输入
3. drizzle 支持指定文件范围 → healpix_stack 的局部更新

## 后续开发计划

1. **Phase 1**: 点采样 drizzle(最简单，验证流程)
2. **Phase 2**: 面积加权 drizzle(提高精度)
3. **Phase 3**: Lanczos 重采样(高质量，可选)
4. **Phase 4**: 多线程并行(16 线程优化)

## 技术约束

- C++17，编译 -O3 -ffast-math -funroll-loops -fopenmp
- 依赖 astro_image_io(读取 .ahpx)和 healpix 库(ang2pix)
- 支持 WCS+SIP 畸变校正
- drizzle 不修改原始 .ahpx 文件
