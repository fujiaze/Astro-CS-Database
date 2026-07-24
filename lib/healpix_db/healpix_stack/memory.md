# Healpix-Mosaic-Cpp 模块记忆

## 模块职责
HEALPix 稀疏堆栈存储与叠加模块，用于天文图像的球面投影叠加。

## 当前版本
v1.0

## GitHub 仓库
- 仓库地址：https://github.com/fujiaze/Healpix-Mosaic-Cpp
- 默认分支：main
- 语言标注：C++17 + Python (ctypes)

## 依赖列表
- C++17
- OpenMP（16线程并行）
- astro_image_io.dll（AIO C API：压缩/解压、PipelineFrame 内存管线）
- PipelineFrame 内存管线（数据传入传出）

## 关键决策记录
- `.ahps` 稀疏堆栈格式读写：自定义二进制格式，存储 HEALPix 稀疏像素（ipix + value + weight + count），支持多波段统计块
- 多帧 HEALPix 数据叠加：通量加权平均（sigma-clip + SNR 加权），用于天文图像球面投影叠加
- 支持 ZSTD/LZ4 压缩：通过 AIO C API（aio_compress/aio_decompress），codec 1=ZSTD / 2=LZ4
- 通过 PipelineFrame 内存管线获取数据：Python 层填充 PipelineFrame → C++ 模块读取命名块 → 输出结果，不经临时文件

## 进度日志
- 2026-07-12：从 healpix_db 拆分为独立仓库（原仓库名 Healpix-Mosaic-C-Python-，commit b8a8814），作为 healpix_lod 编译依赖本地保留
- 2026-07-13：仓库迁移到新地址 Healpix-Mosaic-Cpp，更新 README 为统一格式，新增模块 memory.md
- 2026-07-13：完成 Task 5「内存 sigma-clip 堆叠改造（.hiss → .hcsd）」
  - 新增 hp_stack_hiss.cpp/.h：实现内存 sigma-clip 堆叠，逐帧读 .hiss → 按 ipix 对齐累加 count/sum/sum_sq → 迭代剔除离群值 → 输出 .hcsd
  - 内存占用 = 3 × n_unique_pix × 8B (float64 × 3 数组)，与帧数无关；sigma-clip 阶段临时用 rejected 数组跟踪已剔除像素
  - 调用 healpix_io.dll 的 hiss_read/hcsd_write/hio_free，由 hcsd_write 自动构建子叶块索引
  - Python 绑定（healpix_stack.py）：新增 stack_hiss_files() 函数；_load_dll() 增加 astro_image_io.dll 搜索路径和预加载
  - build.ps1 已链接 healpix_io.dll 导入库；Makefile 配置 HIO_DIR 路径
  - 修复 3 个 bug: (1) 测试脚本 sys.path 路径少一层；(2) healpix_stack.dll 缺少 astro_image_io.dll 依赖路径导致 Python 加载失败；(3) sigma-clip 重复剔除 bug（添加 rejected 跟踪数组）
  - 端到端验证 3/3 测试通过：基本3帧堆叠/sigma-clip 剔除离群值/不同 ipix 集合合并
  - 关键发现：mean+std sigma-clip 需要足够多的正常帧才能有效剔除单个离群值（5 帧 4 正常+1 离群不够，11 帧 10 正常+1 离群可剔除）

## SNR² 加权 sigma-clip (2026-07-15)

### 断层4修复: 等权 → SNR² 加权
- **hp_stack_hiss.cpp**: 累加数组 count_arr/sum_arr/sum_sq_arr → weight_arr/sum_w_arr/sum_wsq_arr/count_arr
- **权重计算**: w = snr² (clamp [0, 1e6]); 无 snr 时 w=1.0 (向后兼容)
- **加权统计**: mean=sum_w/weight, std=sqrt(sum_wsq/weight - mean²); sigma-clip 剔除时 SNR² 加权减去
- **输出**: weighted_mean = sum_w_arr[idx] / weight_arr[idx]
- **测试**: 5/5 通过 (基本堆叠/sigma-clip/合并/SNR²加权基本(v=10/snr=10+v=20/snr=20→18.0)/向后兼容(无snr→15.0))

## GAP-017 Winsorized sigma clip 实现 (2026-07-17)

### 背景
corrected_stacker.h 的 CorrectedStackParams 只有 sigma 和 max_iter，无 winsorized 标志。实际执行普通 sigma-clip，对异常值不够稳健。
用户批复："要求实现"。

### 修改
- **corrected_stacker.h**: CorrectedStackParams 新增 3 字段
  - `use_winsorized` (bool, 默认 false 保持向后兼容)
  - `winsorize_low_pct` (double, 默认 0.05)
  - `winsorize_high_pct` (double, 默认 0.95)
- **corrected_stacker.cpp**: stack() 函数 sigma-clip 循环按 use_winsorized 分支
  - false: 原普通 SNR²加权 sigma-clip (基于 sum_w/sum_wsq/weight 加权统计)
  - true:  Winsorized sigma clip - 每轮收集每个 ipix 未剔除帧的 corrected 值，排序后按分位数 winsorize，用缩尾后样本计算稳健 mean/std，再用原值判断 |x-mean| > sigma*std 剔除
  - Winsorized 分支引入 `masked[f][k]` 数组跟踪每帧每像素是否被剔除（原累加器 weight_arr/count_arr 同步更新）
- **hp_stack_api.h**: hp_stack_gradient_corrected C API 签名扩展 3 参数
  - `const char* sigma_clip_method` (nullptr / "standard" = 普通; "winsorized" = Winsorized)
  - `double winsorize_low_pct` (默认 0.05)
  - `double winsorize_high_pct` (默认 0.95)
- **hp_stack_api.cpp**: 解析 sigma_clip_method → use_winsorized 标志，透传给 CorrectedStackParams；meta_json 输出 sigma_clip.method/winsorize_low/winsorize_high 字段

### 关键设计
- 向后兼容：use_winsorized=false 时走原逻辑，不影响已有调用
- C API 向后不兼容（签名扩展），所有调用方需更新（仅 orchestrator.cpp 一处）
- Winsorized 模式下 SNR²加权累加器仍同步维护（最终 mean = sum_w/weight 不变），只是剔除判据更稳健

### 编译验证
- healpix_stack.dll 编译成功（1437.2 KB），hp_stack_gradient_corrected 已正确导出
- 警告：仅原有 unused variable/function 警告，本次修改未引入新警告

### 调用方更新
- orchestrator.cpp run_stage_gradient_sphere 从 stage2_config.json 读 sigma_clip_method/sigma_clip_sigma/winsorize_low_pct/winsorize_high_pct 字段透传
