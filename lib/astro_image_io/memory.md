# astro_image_io - 模块开发memory

## 模块职责
统一天文图像I/O层 + Pipeline管线引擎，提供FITS/XISF图像读写、命名块容器模型与PipelineFrame+PipelineEngine管线框架，作为所有C++模块的底层依赖。

## 当前版本
- 版本号：v1.0 C++原生实现
- 最新commit：a33d167
- 更新时间：2026-07-12

## GitHub仓库
- 仓库地址：https://github.com/fujiaze/Astro-Image-IO-C
- 默认分支：master

## 依赖列表
- C++17
- OpenMP（多线程并行）
- 无外部库（零依赖，纯C++原生实现）

## 关键决策记录
- **C++原生FITS/XISF解析**：零外部依赖，避免cfitsio等库引入，自行实现FITS关键字解析、BZERO/BSCALE处理与XISF格式解析
- **命名块容器模型**：PipelineFrame采用"命名块"数据结构（add_block/get_block_data/kv_set/kv_get），支持像素块、星表块、KV元数据块统一管理，避免跨模块数据格式耦合
- **PipelineFrame+PipelineEngine架构**：管线引擎以PipelineStageHandler回调为执行单元，通过frame指针在阶段间传递，Python层仅做编排
- **BZERO/BSCALE根因修复**：fits_write_file关键字过滤列表增加BZERO/BSCALE，从源头避免float32数据携带无符号16位关键字导致的二次偏移

## 进度日志
### 2026-07-12 C++迁移完成与性能修复
- 完成C++迁移，15/15测试通过
- 修复动态缓冲区问题（图像读写越界）
- 修复PSF性能问题（详见dynamic_psf模块记录）
- 关键API：ImageReader/FITSWriter读写、PipelineFrame命名块容器、PipelineEngine管线调度
- 推送至GitHub：commit a33d167

### 2026-07-13 仓库结构整理完成
- GitHub仓库分支统一为main
- 文档刷新并重新推送
- 最新commit: c7ae15f

### 2026-07-16 healpix_io 合并入 aio (architecture-refactor spec G1+G2, Phase 1 完成)

**变更背景**: 按 architecture-refactor spec，healpix_io 源码合并入 aio，aio.dll 统一提供 FITS+XISF+HEALPix I/O。

**Phase 1 完成项**:
1. **源码迁移** (1.1-1.2): healpix_io.cpp/h → aio_healpix_io.cpp/h，API 前缀 hiss_/hcsd_/hio_ → aio_hiss_/aio_hcsd_/aio_hio_，添加向后兼容宏（`#define hiss_write aio_hiss_write` 等）
2. **选择编译机制** (1.3-1.4): aio_build_config.json + 条件编译宏（AIO_ENABLE_FITS/XISF/HEALPIX/COMPRESSOR/PIPELINE），build.ps1 动态构建源文件列表
   - **重大 bug 修复**: PowerShell 7 解析无 BOM UTF-8 文件时中文注释导致行解析失败（aio_log.cpp 被丢弃）。根因：中文注释 `# 核心必需源文件` 后的 `$srcFiles += "src/aio_log.cpp"` 被吞。修复：所有中文注释改为英文
3. **healpix_io 归档** (1.5): healpix_io/ 整目录移入 archive/，创建 ARCHIVED.md，FORMAT_SPEC.md/test_*/healpix_io.py 复制到 aio 对应位置
4. **依赖调整** (1.6): healpix_drizzle (6 源文件 + Makefile) + healpix_stack (build.ps1) 改为链接 aio.dll，移除 healpix_io.dll 依赖，添加 -DAIO_ENABLE_HEALPIX 宏
5. **编译验证** (1.7): V1-V4+V10 全部通过

**验证结果**:
| 验证项 | 结果 |
|--------|------|
| V1 | ✅ aio.dll 默认配置 2923.7 KB，67 aio_* + 9 HEALPix I/O 符号 |
| V2 | ✅ minimal 配置 893.9 KB，25 符号，无 aio_hiss_* |
| V3 | ✅ healpix_drizzle.dll 1243.8 KB，2 hp_drizzle_* 符号，只依赖 astro_image_io.dll |
| V4 | ✅ healpix_stack.dll 1430.8 KB，10 hp_stack_* 符号，只依赖 astro_image_io.dll |
| V10 | ✅ 向后兼容宏工作（旧名 hiss_read/hiss_write 编译通过）|

**新文件**:
- `include/aio_healpix_io.h` - HEALPix I/O API + 向后兼容宏
- `src/healpix/aio_healpix_io.cpp` - HEALPix I/O 实现
- `aio_build_config.json` (默认/full/minimal/healpix 4 套配置)
- `build.ps1` - 选择编译脚本
- `docs/HEALPIX_FORMAT_SPEC.md` - 格式规范（从 healpix_io 复制）
- `tests/test_healpix_io*.py` - 测试（从 healpix_io 复制）
- `python/aio_healpix_io.py` - Python 绑定（从 healpix_io 复制）

**修改文件**:
- `src/aio_api.cpp` - 添加 AIO_ENABLE_FITS/XISF 条件编译宏
- `lib/healpix_db/healpix_drizzle/` - drizzle_engine.cpp/h, hp_drizzle_api.cpp, Makefile
- `lib/healpix_db/healpix_stack/` - hp_stack_api.cpp, hp_stack_hiss.cpp, gradient/gradient_sampler.cpp, build.ps1

### 2026-07-31 HISS Reader 实现 (XISF 式 Header + attachments 格式)

**变更背景**: 按 hiss_format.h 接口规范实现 HissReader 类，支持 XISF 式 Header + attachments 格式的 HISS 文件读取。与 HISS_FORMAT_V2.md (HI2S magic) 是不同的格式体系。

**实现内容**:
1. **新文件 `src/hiss_reader.cpp`** - HissReader 类完整实现 (~750 行):
   - 小端序二进制读写工具 (read_u16/32/64_le, read_i32/f32/f64_le)
   - CRC32-C (Castagnoli) 校验实现 (多项式 0x82F63B78, 查表法)
   - 简单 JSON 键值解析器 (json_find_value/json_get_string/number/int) — 不依赖外部 JSON 库
   - `HissMetadata::from_json` 实现 (reader 必需) + `to_json` 基线实现 (避免链接错误)
   - `compute_tile_depth` / `compute_tile_nside` 实现 (02_FROZEN §11)
   - HEALPix NESTED 坐标转换 (radec_to_nested_ipix, 内部实现, 不依赖外部 HealpixCore)
   - HissReader 全部 9 个公共方法: open/grid/metadata/tiles/read_tile/read_tile_signal/read_tile_support/read_tile_snr/query_pixel/close

2. **二进制布局** (与 Writer 对应):
   - 签名块 20B: MAGIC "ACSHISS\0"(8) + version u32(4) + header_offset u64(8)
   - Header: GridSpec(24B) + JSON(len u32 + data) + Tile目录(n_tiles u32 + tiles)
   - Tile头 15B: parent_ipix u64 + tile_nside u32 + occ_mode u8 + n_subblocks u16
   - 子块描述符 40B: type u8 + flags u16 + offset u64 + comp_size u64 + uncomp_size u64 + codec u16 + transform u16 + checksum_type u8 + checksum u64
   - SNR子块解压后: n_points u32 + points[n×8B(local_ipix u32+snr f32)] + snr_phot f64 + median_snr f64 + idw_power f64

3. **query_pixel 支持 3 种 occupancy 模式**:
   - FULL: 直接用 local_ipix 索引 signal/support 数组
   - BITMAP: 读取 occupancy bitmap, popcount 计算数组索引
   - SPARSE_LIST: 读取 sparse list, 二分查找 local_ipix

4. **错误码** (任务规范):
   - -1=MAGIC不匹配, -2=version不兼容, -3=越界, -4=解压长度不匹配, -5=checksum错误, -6=未知必需子块

**编译结果**:
- DLL: astro_image_io.dll, 3033 KB
- HissReader 全部 12 个符号 + from_json/to_json + compute_tile_depth/nside 正确导出
- 无 hiss_reader.cpp 相关 warning/error

**修改文件**:
- `Makefile` - SRCS 添加 `$(SRC_DIR)/hiss_reader.cpp`
- `build.ps1` - $srcFiles 添加 `"src/hiss_reader.cpp"`
- `src/hiss_reader.cpp` - 新建

### 2026-07-31 HISS Writer 实现 (XISF 式 Header + attachments 格式)

**变更背景**: 按 hiss_format.h 接口规范 (02_FROZEN §14/§15) 实现 HissWriter 类, 支持 XISF 式 Header + attachments 单体容器写入。

**实现内容**:
1. **新文件 `src/hiss_writer.cpp`** - HissWriter 类完整实现 (~480 行):
   - `HissWriter::open` - 创建 .partial 临时文件, 写入 20B 签名块占位 (MAGIC + version + header_offset=0)
   - `HissWriter::add_tile` - 生成 occupancy(可选)/signal/support/snr(可选) 子块, 压缩后暂存内存池
   - `HissWriter::finalize` - 计算子块 offset, 重写签名块(header_offset=20) + Header + 所有子块, flush, 原子重命名
   - `HissWriter::cancel` - 关闭并删除 .partial, 清理内存
   - `HissWriter::set_experiment_codec` - 按 SubblockType 设置实验性 codec/transform
   - 内部 `build_pending_subblock` 辅助函数: 调用 CodecRegistry 压缩数据, 填充 HissSubblockDescriptor

2. **同时实现的缺失方法** (hiss_format.h 声明但未定义, 链接必需):
   - `compute_tile_depth` / `compute_tile_nside` (02_FROZEN §11)
   - `DrizzleTileAccumulator::finalize_signal` (float64→float32, sum_area 归一化)
   - `DrizzleTileAccumulator::finalize_support` (sum_area→uint8, round(255*S) 钳制 [0,1])
   - `DrizzleTileAccumulator::validate_support` (检查 sum_area 在 [0,1] 范围, 允许浮点误差)
   - `HissMetadata::to_json` (手写 JSON, 含转义) / `from_json` (简易键值查找解析)

3. **二进制布局** (与 hiss_reader.cpp 对应):
   - 签名块 20B: MAGIC "ACSHISS\0"(8) + version u32(4) + header_offset u64(8)
   - Header (紧跟签名块, header_offset=20): GridSpec(24B) + JSON(len u32 + data) + Tile目录(n_tiles u32 + tiles)
   - Tile目录前缀 15B: parent_ipix u64 + tile_nside u32 + occ_mode u8 + n_subblocks u16
   - 子块描述符 40B: type u8 + flags u16 + offset u64 + comp_size u64 + uncomp_size u64 + codec u16 + transform u16 + checksum_type u8 + checksum u64
   - SNR子块数据: snr_phot f64 + median_snr f64 + idw_power f64 + n_points u32 + points[n×8B(local_ipix u32+snr f32)]

4. **occupancy 编码** (02_FROZEN §12):
   - FULL: 全有效, 省略 occupancy 子块
   - BITMAP: 1 bit/叶像素 (LSB 优先), sum_area>0 标记有效
   - SPARSE_LIST: 有效像素局部索引 (uint32 数组)

5. **冒烟测试验证** (全部通过):
   - compute_tile_depth(64)=2, tile_nside=16
   - add_tile FULL 模式: signal(1024B)+support(256B)+snr(44B), 无 occupancy
   - add_tile BITMAP 模式: occupancy(32B)+signal(1024B)+support(256B)
   - finalize: tiles=2, header_offset=20, header_size=606, total=3262B (20+606+1324+1312=3262 完全吻合)
   - 文件头签名 MAGIC/version/header_offset 全部正确
   - .partial 原子重命名成功
   - to_json/from_json 往返一致 (nside/exptime/object/history)

**编译结果**:
- DLL: astro_image_io.dll, 3452 KB (含 hiss_writer.cpp)
- HissWriter 全部 7 个方法 + finalize_signal/support + validate_support + to_json/from_json + compute_tile_depth/nside 正确导出
- 无 hiss_writer.cpp 相关 warning/error

**修改文件**:
- `Makefile` - SRCS 添加 `$(SRC_DIR)/hiss_writer.cpp`
- `build.ps1` - $srcFiles 添加 `"src/hiss_writer.cpp"`
- `src/hiss_writer.cpp` - 新建

**重要说明 - 重复定义问题**:
- `hiss_reader.cpp` 和 `hiss_writer.cpp` 都实现了 `to_json`/`from_json`/`compute_tile_depth`/`compute_tile_nside` (两份实现兼容, JSON 格式/字段顺序/算法一致)
- 当前编译列表只含 `hiss_writer.cpp` (不含 `hiss_reader.cpp`), 避免链接重复定义
- hiss_reader.cpp 的 to_json 注释明确写 "Writer 可覆盖此实现", 设计预期由 Writer 提供最终实现
- 若未来需同时编译 Reader+Writer, 应将共享方法 (to_json/from_json/compute_tile_depth/nside/finalize_signal/support/validate_support) 抽出到 `hiss_common.cpp`
- hiss_writer.cpp 独有: finalize_signal/finalize_support/validate_support (hiss_reader.cpp 未实现)
- 用 hiss_writer.cpp 写入的文件, hiss_reader.cpp 的 from_json 能正确解析 (JSON 格式兼容)

## 2026-08-06 R13 HISS_IO_REPAIR 性能闭合 (commit 450e78c/83c4696/6babe22)

### 写入侧 (450e78c)
- 生产 HISS 此前 `experiment_codecs` 为空默认 RAW (316MB 未压缩) → open 默认
  ZSTD (signal=BYTE_SHUFFLE, support/occ/snr=ZSTD), 显式 set_experiment_codec 可覆盖;
- zstd 悬垂指针修复 (compressed_buf 提升到函数作用域);
- Tile 级可复用缓冲 (signal/support/valid/occ) 消除每 Tile 1MB 级分配;
- 逐 Tile/逐子块日志包进 HISS_DLOG (编译期 HISS_VERBOSE)。

### Verify 侧 (83c4696)
- 新增 session API: aio_hiss_open_session / read_tile_{signal,support,snr}[_f64]_session /
  close_session, Reader 打开一次遍历全部 Tile;
- orchestrator HISS_VERIFY 由 130s → 0.85s。

### 日志门控 (6babe22) — 完整帧 129s → 45.43s 的关键一步
- hiss_reader.cpp 成功路径 (inverse_transform/网格/元数据/打开成功/SNR 子块/
  无 SNR) 与 hiss_transform.cpp 全部逐调用日志改为 HISS_DLOG 门控;
- 根因: stderr 重定向文件时每条 fprintf 写盘 + 防病毒扫描, 285 Tile 的
  ~870 行日志实测拖慢 Verify 40s / 写入 ~30s;
- 验证: verify_bench 同文件 0.697s (日志 870→5 行), bench_write 4096²
  write 2.60s (add_tile 6.35ms/tile)。

## 2026-08-06 签字修正: WCS 自适应细分收敛阈值修复 (c7d3b8f)

`build_drop_polygon_adaptive` (hiss 几何共用) 收敛阈值 src_scale×1e-12
(6.3" 像素 ≈3e-17) 低于 pixelToSky 数值噪声 (实测 6e-14), TAN 小像素永不
收敛 → 每边 4096 段 16384 顶点 (反向 Drizzle 卡死)。修复: 阈值下限
1e-11 rad → TAN 立即 4 角收敛, SIP 收敛到 1e-11; 面积误差 ≤1.7e-7 相对。
