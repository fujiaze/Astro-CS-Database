# AstroCS Stage1 HISS 实现总结

> 生成时间: 2026-07-31
> 项目根目录: `f:\Astro dev\Astro CS Normalization Database`

## 1. 交付物清单

### 1.1 测试程序

| 文件 | 路径 | 说明 |
|------|------|------|
| 正确性测试 | `lib/astro_image_io/tests/hiss_correctness_test.cpp` | 21 个测试用例, 覆盖校准/Drizzle/HISS 格式 |
| 测试输出 | `lib/astro_image_io/tests/test_output.txt` | 测试运行日志 (21/21 通过) |

### 1.2 报告文档

| 文件 | 路径 | 说明 |
|------|------|------|
| 性能剖析 | `AstroCS_Stage1_HISS_Delivery/reports/performance_profile.md` | 模块性能/内存/并行化分析 |
| 正确性报告 | `AstroCS_Stage1_HISS_Delivery/reports/correctness_report.md` | 21 个测试用例详细结果 |
| 实现总结 | `AstroCS_Stage1_HISS_Delivery/reports/implementation_summary.md` | 本文档 |

## 2. 测试实现概述

### 2.1 测试框架

- **语言**: C++17
- **依赖**: hiss_format.h, astro_calibration.h
- **断言宏**: `ASSERT_TRUE`, `ASSERT_NEAR`, `SKIP_TEST`
- **数据生成**: 合成数据 (确定性随机数, `std::mt19937`)
- **无外部依赖**: 不依赖 Google Test 等框架, 自包含

### 2.2 测试覆盖

| 模块 | 测试编号 | 数量 | 关键验证点 |
|------|---------|------|-----------|
| 校准 | 01-05 | 5 | (L-D)/F 公式, [L-B-k(D-B)]/F 公式, 最优 Dark 成功/回退/硬失败 |
| Drizzle | 06-11 | 6 | 通量守恒, support 量化, 自动 NSIDE |
| HISS 格式 | 12-21 | 10 | 占用模式往返, 子块校验, 原子提交, ipix 恢复 |

### 2.3 编译与运行

**编译命令** (从 `tests/` 目录):
```bash
g++ -std=c++17 -O2 -fopenmp -DHAS_LZ4 -DHAS_ZSTD \
  -I../include -I../src \
  -I../../calibration/include \
  hiss_correctness_test.cpp \
  ../src/hiss_codec.cpp ../src/hiss_common.cpp \
  ../src/hiss_writer.cpp ../src/hiss_reader.cpp \
  ../../calibration/src/dark_optimizer.cpp ../../calibration/src/calibrator.cpp \
  -llz4 -lzstd -o hiss_correctness_test.exe
```

**运行命令**:
```bash
./hiss_correctness_test.exe
```

**测试结果**: 21/21 通过, 退出码 0

## 3. 模块实现状态

### 3.1 校准模块 (lib/calibration)

| 组件 | 文件 | 状态 | 备注 |
|------|------|------|------|
| 标准校准 | `src/calibrator.cpp` | 已实现 | `(L-D)/F` 模式, OpenMP 16 线程 |
| 曝光比例校准 | `src/calibrator.cpp` | 已实现 | `[L-B-k(D-B)]/F` 模式 |
| 最优 Dark 估计 | `src/dark_optimizer.cpp` | 已实现 | 8×8 分区 + 鲁棒回归 + 5 轮 MAD |
| 失败诊断 | `src/dark_optimizer.cpp` | 已实现 | `Stage1Diagnostics` 结构化诊断 |
| 回退机制 | `src/dark_optimizer.cpp` | 已实现 | OPTIMAL → EXPOSURE_RATIO 回退 |

### 3.2 Drizzle 模块 (lib/healpix_db/healpix_drizzle)

| 组件 | 文件 | 状态 | 备注 |
|------|------|------|------|
| Drizzle 引擎 | `drizzle_engine.cpp` | 已实现 | OpenMP 16 线程, schedule(guided) |
| 自动 NSIDE | `drizzle_engine.cpp` | 已实现 | 5 采样点有限差分, 钳位 [16, 1048576] |
| 多边形裁剪 | `poly_clip.cpp` | 已实现 | Sutherland-Hodgman + 切平面投影 |
| WCS+SIP | `wcs_sip.cpp` | 已实现 | TAN 投影 + SIP 多项式 |
| 累加器 | `drizzle_engine.h` | 已实现 | float64 内部累加, finalize 输出 float32/uint8 |

### 3.3 HISS I/O 模块 (lib/astro_image_io)

| 组件 | 文件 | 状态 | 备注 |
|------|------|------|------|
| 格式定义 | `include/hiss_format.h` | 已冻结 | HissGridSpec/HissTile/HissWriter/HissReader |
| Writer | `src/hiss_writer.cpp` | 已实现 | Header 前置 + .partial 原子提交 |
| Reader | `src/hiss_reader.cpp` | 已实现 | 按目录读取 + offset/checksum 校验 |
| Codec 注册 | `src/hiss_codec.cpp` | 已实现 | RAW/LZ4/ZSTD 三种 codec |
| 通用工具 | `src/hiss_common.cpp` | 已实现 | validate_support/compute_tile_depth 等 |

## 4. 关键算法验证

### 4.1 校准公式

| 模式 | 公式 | 验证结果 |
|------|------|---------|
| 标准模式 (dark_opt=0) | `out = (light - dark) / flat` | 0/4096 像素不匹配, actual_k=1.0 |
| 曝光比例模式 (dark_opt=1) | `out = (light - bias - k*(dark - bias)) / flat` | 0/4096 像素不匹配, actual_k=k_init |

### 4.2 最优 Dark 系数估计

| 场景 | 输入 | 输出 | 验证结果 |
|------|------|------|---------|
| 成功路径 | k_true=1.5, c_true=5.0 | k_est=1.4998 | 误差 <0.15, success=0, fell_back=0 |
| 回退路径 | D-B 方差近零 | k_ret=k_init=1.2 | success=-1, fell_back=1, code=RESIDUAL_ABNORMAL |
| 硬失败 | k_init=0 (EXPTIME 缺失) | k_ret=0.0 | success=-1, code=BAD_K_INIT, 硬失败 |

### 4.3 Drizzle 通量守恒

| 场景 | 输入 | 输出 | 验证结果 |
|------|------|------|---------|
| 单像素 | L=100, a=1.0 | signal=100.0 | 通量守恒 |
| 多像素 | 源A=200 (面积 0.6+0.4), 源B=300 (面积 0.7+0.3) | signal=[200,200,300,300], 总通量=500 | 通量守恒 |

### 4.4 自动 NSIDE 计算

| 输入像素尺度 | 计算 NSIDE | HEALPix 像素尺度 | 过采样倍数 | 验证结果 |
|------------|-----------|----------------|-----------|---------|
| 1.0"/px | 262144 | 0.8047"/px | 0.805× | OK |
| 0.5"/px | 524288 | 0.4024"/px | 0.805× | OK |
| 2.0"/px (CD 矩阵) | 131072 | 1.6095"/px | 0.805× | OK |
| 60"/px (粗) | 4096 | - | - | 下限 OK |
| 0.1"/px (极细) | 1048576 | 0.2012"/px | - | 上限钳位 OK |

### 4.5 HISS 格式往返

| 占用模式 | 文件大小 | signal 往返 | support 往返 | 验证结果 |
|---------|---------|------------|-------------|---------|
| FULL | 15798 B | 0 错误 | 0 错误 | OK |
| BITMAP | 16222 B | 0 错误 | 0 错误 | OK |
| SPARSE_LIST | 19934 B | 0 错误 | 0 错误 | OK |

### 4.6 HISS 错误处理

| 错误场景 | 错误码 | 验证结果 |
|---------|--------|---------|
| offset 越界 | read_tile 返回 <0 | OK |
| checksum 错误 (CRC32C) | read_tile 返回 -5 | OK |
| .partial 文件 | Reader.open 返回 <0 | OK |
| 未知可选子块 | 跳过, read_tile 返回 0 | OK |

## 5. 已知问题与后续工作

### 5.1 已知问题 (2 项)

#### 问题 1: Writer/Reader SNR 二进制布局不一致
- **严重度**: 中
- **影响**: SNR 控制点无法往返读取 (signal/support 不受影响)
- **根因**: Writer 在 n_points+points 之后追加三个全局 double, Reader 只读取 n_points+points
- **建议修复**: 统一 Writer/Reader 的 SNR 子块二进制布局

#### 问题 2: 未知必需子块未主动拒绝
- **严重度**: 低
- **影响**: 文件含未知必需子块时 Reader 不会拒绝 (规范要求拒绝)
- **根因**: Reader 按 type 查找子块, 未全扫描未知必需子块
- **建议修复**: Reader.open 或 read_tile 时增加全扫描逻辑

### 5.2 后续工作建议

1. **修复 SNR 布局不一致**: 统一 Writer/Reader 的 SNR 子块二进制布局, 重新运行 TEST 13 验证
2. **实现未知必需子块拒绝**: 在 Reader 中增加全扫描逻辑, 重新运行 TEST 16 验证
3. **添加 LZ4/ZSTD codec 测试**: 当前测试仅覆盖 RAW codec, 建议增加 LZ4/ZSTD 压缩往返测试
4. **性能基准测试**: 基于 `hiss_benchmark.cpp` 建立性能基线, 加入 CI 性能回归监测
5. **真实数据验证**: 使用真实天文数据 (如 testdata 目录) 验证端到端流程

## 6. 文件清单

### 6.1 新增文件

| 文件 | 行数 | 说明 |
|------|------|------|
| `lib/astro_image_io/tests/hiss_correctness_test.cpp` | ~1340 | 21 个正确性测试用例 |
| `AstroCS_Stage1_HISS_Delivery/reports/performance_profile.md` | ~160 | 性能剖析报告 |
| `AstroCS_Stage1_HISS_Delivery/reports/correctness_report.md` | ~250 | 正确性测试报告 |
| `AstroCS_Stage1_HISS_Delivery/reports/implementation_summary.md` | ~200 | 实现总结 (本文档) |

### 6.2 依赖的已有文件

| 文件 | 说明 |
|------|------|
| `lib/astro_image_io/include/hiss_format.h` | HISS 格式定义 (已冻结) |
| `lib/astro_image_io/src/hiss_writer.cpp` | HISS Writer 实现 |
| `lib/astro_image_io/src/hiss_reader.cpp` | HISS Reader 实现 |
| `lib/astro_image_io/src/hiss_codec.cpp` | Codec 注册 (RAW/LZ4/ZSTD) |
| `lib/astro_image_io/src/hiss_common.cpp` | 通用工具 (validate_support 等) |
| `lib/calibration/include/astro_calibration.h` | 校准 API 定义 |
| `lib/calibration/src/calibrator.cpp` | 校准算法实现 |
| `lib/calibration/src/dark_optimizer.cpp` | 最优 Dark 系数估计 |
| `lib/healpix_db/healpix_drizzle/drizzle_engine.h` | Drizzle 引擎定义 |

## 7. 编译与测试环境

| 项目 | 值 |
|------|-----|
| 操作系统 | Windows 11 |
| 编译器 | g++ 16.1.0 (Rev4, MSYS2 MinGW64) |
| C++ 标准 | C++17 |
| 编译选项 | -O2 -fopenmp -DHAS_LZ4 -DHAS_ZSTD |
| 链接库 | -llz4 -lzstd |
| OpenMP 线程数 | 16 (固定) |
| 测试日期 | 2026-07-31 |

## 8. 声明

- 本实现总结仅记录测试与实现状态, 不代表用户验收完成
- 已知问题需在后续迭代中修复并重新测试
- 测试使用合成数据, 未使用真实天文数据
- 测试不修改任何数学算法或科学语义
