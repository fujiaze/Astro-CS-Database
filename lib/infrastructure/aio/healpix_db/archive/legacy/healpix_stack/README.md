# Healpix-Mosaic-Cpp (C++17 + Python ctypes)

HEALPix 稀疏堆栈存储与叠加模块，用于天文图像的球面投影叠加。

## 模块职责
- 提供 HEALPix 球面上的稀疏像素堆栈存储格式（`.ahps`）
- 支持多帧天文图像在球面上的通量加权叠加（mosaic）
- 通过 PipelineFrame 内存管线接收上游 drizzle 输出，输出叠加后的稀疏堆栈

## 功能列表
- `.ahps` 稀疏堆栈格式读写（ipix + value + weight + count）
- 多帧 HEALPix 数据叠加（sigma-clip + SNR 加权 / 通量加权平均）
- 支持 ZSTD / LZ4 压缩（通过 AIO C API）
- OpenMP 16 线程并行叠加
- 通过 PipelineFrame 内存管线获取数据，不经临时文件

## 目录结构
```
healpix_stack/
├── ahps_format.h         # .ahps 格式常量定义
├── ahps_reader.h/.cpp    # .ahps 读取器
├── ahps_writer.h/.cpp    # .ahps 写入器
├── healpix_core.h/.cpp   # HEALPix 球面运算核心
├── stack_engine.h/.cpp   # 堆栈叠加引擎（sigma-clip + 加权平均）
├── stack_db.h/.cpp       # 堆栈数据库管理
├── hp_stack_api.h/.cpp   # C API 导出层（extern "C"）
├── healpix_stack.py      # Python ctypes 绑定
├── Makefile              # 构建脚本
├── tests/
│   └── test_healpix_stack.py  # 单元测试
├── .gitignore
├── memory.md             # 模块开发记忆
└── README.md
```

## 依赖列表
- C++17（编译器支持 `-std=c++17`）
- OpenMP（16 线程并行）
- `astro_image_io.dll`（AIO C API：压缩/解压 + PipelineFrame 内存管线）
- PipelineFrame 内存管线（数据传入传出）

## 编译说明
```bash
# 带 AIO 压缩库构建（依赖 astro_image_io.dll）
make

# 指定 AIO 目录
make AIO_DIR=../../astro_image_io
```

构建产物：`healpix_stack.dll`

## 使用示例（PipelineFrame 接口）

### C API
```c
#include "hp_stack_api.h"

// 多帧叠加入口：从 PipelineFrame 数组读取数据，输出 .ahps 文件
int hp_stack_run(const PipelineFrame** frames,
                 int n_frames,
                 const char* output_path,
                 HpStackResult* result);
```

### Python（ctypes）
```python
from healpix_stack import HealpixStack

stack = HealpixStack(dll_path="healpix_stack.dll")
# frames: PipelineFramePy 对象列表（每帧含 healpix 命名块）
result = stack.run(frames, output_path="mosaic.ahps")
print(f"叠加完成: {result.n_pixels} 像素, 耗时 {result.elapsed_ms} ms")
```

## GitHub 仓库
- 地址：https://github.com/fujiaze/Healpix-Mosaic-Cpp
- 默认分支：main
- 语言：C++17 + Python (ctypes)
