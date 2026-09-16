# Healpix-Drizzle (C++ Python)

## 模块职责
球面 Drizzle 重投影引擎，将 WCS 投影图像重投影到 HEALPix 等积网格上，支持严格通量守恒。

## 功能列表
- 6步 Drizzle 流水线: 取四角 → pixfrac 收缩 → SIP+WCS 映射 → HEALPix 邻域检索 → 切平面面积裁剪 → 通量守恒分配
- SIP 多项式畸变校正（A/B 前向 + AP/BP 逆向，最多 4 阶）
- TAN 投影（gnomonic 投影）
- Sutherland-Hodgman 多边形裁剪
- OpenMP 16 线程并行
- 严格通量守恒
- hp_drizzle_run 命名块直通接口，对接 PipelineFrame 避免临时文件

## 目录结构
```
healpix_drizzle/
├── README.md
├── Makefile
├── drizzle_engine.cpp / .h      # Drizzle 主引擎
├── fits_reader.cpp / .h         # FITS 读取
├── hp_drizzle_api.cpp / .h      # C API 导出层
├── poly_clip.cpp / .h           # Sutherland-Hodgman 多边形裁剪
├── wcs_sip.cpp / .h             # SIP+WCS 投影变换
├── healpix_drizzle.py           # Python ctypes 绑定
├── pipeline_adapter.py          # PipelineFrame 适配器
└── tests/                       # 单元测试
```

## 依赖列表
- C++17
- OpenMP
- astro_image_io.dll (AIO): 提供 I/O 和压缩 API
- healpix_core: HEALPix 球面运算

## 编译说明
```bash
make  # 需要先编译 astro_image_io.dll
```

## 使用示例

### PipelineFrame 接口
```c
int hp_drizzle_run(PipelineFrame* frame,
                   int nside, int nested, double pixfrac,
                   HpDrizzleResult* result);
```

### Python 调用
```python
from healpix_drizzle import HealpixDrizzle

drizzle = HealpixDrizzle()
result = drizzle.run(frame, nside=2048, nested=True, pixfrac=0.5)
```

## GitHub 仓库
- 地址: https://github.com/fujiaze/Healpix-Drizzle-Cpp
- 语言: C++17 + Python (ctypes)
