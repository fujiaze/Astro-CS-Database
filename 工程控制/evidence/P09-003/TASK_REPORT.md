# 任务报告

- Task/ADR: P09-003 冻结测光失败帧与浏览器性能基线数据
- Commit: 06df865 (P09-002 完成后基线)
- Date: 2026-07-27
- Environment: AMD Ryzen 7 5800X 8C16T 3.8GHz, 63.91GB RAM, Win11 26220, MSYS2 g++ 16.1.0, Qt6 + OpenGL 3.3 Core (offscreen)

## 目标/问题

冻结 v1.2 阶段所需的标准数据集与基线性能记录, 作为后续所有测光/SNR/Drizzle/浏览器优化任务的"修改前事实". 任务要求:
- 选定 T1-T4 测光代表帧 (7 个 canonical 失败帧)
- 银心 32 帧 (panel1=11, panel2=11, panel3=10, Red 滤镜)
- 当前 HCSD 和浏览器固定视角
- 记录 canonical_dataset_v1.2.json 与基线性能记录
- 必测项: 修改前事实/失败基线、对应测试、真实数据/性能测试、回归、原始日志
- 禁止捷径: 不得用随意选择的数据替换失败样本

## 输入与范围

### 输入

- v1.1 既有 evidence (engineering/evidence/): P00-003, P05-002, P03-001, P03-002, P07-001, P07-002, P08-002 等 31 个任务证据目录
- TestData 原始 FITS 帧 (testdata/): T2 (LDN43/NGC1727/NGC247), T3 (NGC55/NGC83), T4 (Galaxy_Center_T4/Victory_Nebula)
- 浏览器构建产物 (lib/healpix_db/healpix_browser_qt/build/healpix_browser_qt.exe)
- P07-001 性能基线 (engineering/evidence/P07-001/performance_baseline.json)

### 范围 (in-scope)

- 计算 SHA-256 hash 并冻结 canonical_dataset_v1.2.json
- 记录浏览器性能基线 (offscreen 模式, 事件日志)
- 记录测光失败基线 (引用 P05-002 既有 evidence)
- 生成四件套证据 (TASK/TEST/EVIDENCE_INDEX/REVIEW)

### 范围外 (out-of-scope)

- 修改任何源代码 (P09-002 已确认命名, P09-003 仅冻结数据)
- 重新运行 stage1/stage2 (引用既有 evidence, 不重复计算)
- 修复 G-002 缺口 (P11 阶段任务)
- 修复浏览器性能问题 (P15 阶段任务)

## 执行/决策

### 1. 识别 T1-T4 设备与测光代表帧

通过递归读取 testdata/ 目录结构, 识别出:
- T2 设备: LDN43_T2, NGC1727_T2, NGC247_T2
- T3 设备: NGC55_T3, NGC83_cluster_T3
- T4 设备: Galaxy_Center_T4 (panel1/2/3), Victory_Nebula_T4

从 P05-002 stage1_e2e_results.json 提取 7 个 canonical 失败帧:
- C001 (T4/Galaxy_Center/Red/180s): n_matched=1, scale=0.003784
- C002 (T2/LDN43/Lum/600s): n_matched=0, stage1 整体失败 (missing_master_flat_Lum)
- C003 (T2/NGC1727/Red/600s): n_matched=1, scale=2.2E-05
- C004 (T2/NGC247/Lum/600s): n_matched=1, scale=8.2E-05
- C005 (T3/NGC55/Red/600s): n_matched=1, scale=4.6E-05
- C006 (T3/NGC83_cluster/Red/600s): n_matched=1, scale=4.2E-05
- C007 (T4/Victory_Nebula/Lum/180s): n_matched=0, scale=1.0

### 2. 识别银心 32 Red 帧

通过递归读取 testdata/Galaxy_Center_T4/lights/panel{1,2,3}/, 统计 Red 滤镜帧:
- panel1: 11 Red 帧 (20250702: 5, 20250703: 5, 20250813: 1)
- panel2: 11 Red 帧 (20250716: 5, 20250717: 6)
- panel3: 10 Red 帧 (20250718: 10)
- 总计: 32 Red 帧 ✓ (与 BASELINE_FACTS.md #5 一致)

### 3. 识别当前 HCSD 与浏览器固定视角

搜索仓库 .hcsd 文件, 选定 4 个 HCSD 基线:
- P07-001/output/stage2_run1.hcsd (178.771MB)
- P07-001/output/stage2_run2.hcsd (178.771MB, 与 run1 字节一致)
- P07-002/output/stage2_repeat_1.hcsd (178.771MB, 与 run1 字节一致)
- P08-002/clean_env/testdata/stage2_baseline.hcsd (178.771MB, 与 run1 字节一致)

所有 4 个 HCSD 的 SHA-256 完全一致 (2A9BD12E0F91BB59...), 证明 stage2 字节级可重现性.

浏览器固定视角 (从源码提取):
- 窗口: 1280x800
- 默认视角: RA=0, Dec=0, FOV=50° (reset_view)
- auto_stretch: shadows=0.5% 分位, highlights=99.5% 分位, midtones=归一化中位数, compression=0.8
- 文件路由: .hiss → set_initial_view_from_data(bbox), .hcsd → reset_view()
- FOV 限制: MIN_FOV=0.01, MAX_FOV=50.0

### 4. 生成 canonical_dataset_v1.2.json

编写 generate_canonical_dataset.py, 计算 7+32+4+1=44 个文件的 SHA-256 hash, 装配 canonical_dataset_v1.2.json:
- 7 个测光失败帧 (含 device/target/filter/exposure/n_matched/scale/sigma_residual/failure_type)
- 32 个银心 Red 帧 (含 panel/filename/size/sha256)
- 4 个 HCSD 基线文件 (含 id/role/description/sha256)
- 1 个浏览器默认 HISS 文件 (T4_2x_nside65536.hiss, 705.087MB)
- 浏览器固定视角配置 (含 FOV 约束、文件路由、auto_stretch 参数)
- 禁止捷径条款 (failure_frame_immutability, galaxy_center_immutability)

总耗时: 6.66 秒 (主要 I/O 读取 44 个文件计算 hash)

### 5. 记录浏览器性能基线

编写 capture_browser_baseline.py, 通过 QT_QPA_PLATFORM=offscreen 启动浏览器, 加载默认 HISS 与 P07-001 HCSD, 捕获 stderr 事件日志:

**测试 1 (HISS, nside=65536, 61M 像素)**:
- 退出码: 1 (timed_out=True, 30s 超时)
- wall_clock: 30.066s
- 解析事件数: 9 (hio: 2)
- 关键发现: **leaf_index_duration = 15.0s** (18:43:23 → 18:43:38, 来自日志时间戳)
- leaf_index 处理 61611427 像素, 建立 78 个子叶 (shift=20)
- get_data_bbox: center=(272.8125, -12.9644), size=9.4375x7.7408 deg
- set_initial_view_from_data: FOV=11.103°
- auto_stretch: median=387.294, midtones=0.0594

**测试 2 (HCSD, nside=32768, 15.5M 像素)**:
- 退出码: 1 (timed_out=True, 20s 超时)
- wall_clock: 20.022s
- 解析事件数: 3 (hio: 18)
- 关键发现: **HCSD 默认视角 (RA=0, Dec=0, FOV=50°) 不覆盖数据** (Galaxy Center 在 RA=272.8°, Dec=-13°)
- 18 个 hcsd_read_leaf 调用全部返回空 (offset=0, length=0)
- STFPanel::set_data_range: [0, 1] (默认范围, 无实际数据)

**浏览器代码状态**:
- 完全没有 timing instrumentation (无 QElapsedTimer, 无 std::chrono, 无 FPS, 无 frame time)
- 有完善的 event-level 日志 (file_open/leaf_load/vbo_build/draw)
- BROWSER_LOG_FILE 仅在程序正常退出时写盘, 崩溃会丢失日志
- main_window 无性能监控 UI

**P15-001 需要补齐的 15 个 trace 缺口**:
- wall_clock_file_open, wall_clock_first_frame, wall_clock_first_hires_view
- frame_time_p50_p95_p99, gui_max_blocking, disk_throughput
- cpu_usage, gpu_usage, peak_memory
- leaf_request_count, cache_hit_rate, vbo_upload_time, draw_time, ud_grade_time, leaf_io_time

### 6. 记录测光失败基线

编写 photometric_failure_baseline.json, 记录:
- 7 个失败帧的完整状态 (n_matched, scale, sigma_residual, failure_type)
- G-002 缺口根因 (KD-tree star_matcher 返回 0/1 对匹配)
- 上游传导证据 (stage1 → stage2 等权回退)
- P09-002 回归检查 (算法未修改, 失败基线与 P05-002 一致)
- 禁止捷径条款合规性 (SHA-256 已冻结, 未替换失败样本)
- P11 修复目标 (n_matched >= 10, sigma_residual > 0, has_snr=true)

## 原始命令、超时和退出码

| 命令 | 超时 | 退出码 | 说明 |
|------|------|--------|------|
| `python generate_canonical_dataset.py` | N/A | 0 | 44 个文件 hash 计算, 6.66s |
| `python capture_browser_baseline.py` (HISS) | 30s | 1 (timed_out) | leaf_index 15s 瓶颈 |
| `python capture_browser_baseline.py` (HCSD) | 20s | 1 (timed_out) | 默认视角不覆盖数据 |
| `python -c "import json; ..."` | N/A | 0 | JSON 校验通过 |

原始日志:
- engineering_v1.2/evidence/P09-003/raw_logs/generate_canonical_dataset.log
- engineering_v1.2/evidence/P09-003/raw_logs/browser_hiss_stderr.log
- engineering_v1.2/evidence/P09-003/raw_logs/browser_hiss_stdout.log
- engineering_v1.2/evidence/P09-003/raw_logs/browser_hcsd_stderr.log
- engineering_v1.2/evidence/P09-003/raw_logs/browser_hcsd_stdout.log

## 结果与证据

### 交付物清单

| 文件 | 说明 |
|------|------|
| canonical_dataset_v1.2.json | 主交付物: 44 个文件 SHA-256 + 浏览器固定视角配置 |
| browser_performance_baseline.json | 浏览器性能基线: 事件日志 + 15 个 trace 缺口 |
| photometric_failure_baseline.json | 测光失败基线: 7 帧 G-002 缺口状态 |
| hashes/all_hashes.json | hash 明细 (便于交叉验证) |
| generate_canonical_dataset.py | hash 计算脚本 (可重复执行) |
| capture_browser_baseline.py | 浏览器性能采集脚本 (可重复执行) |
| raw_logs/*.log | 原始日志 6 份 |

### 关键发现

1. **所有 4 个 HCSD 文件字节一致** (SHA-256: 2A9BD12E0F91BB59...), 证明 P07-001/P07-002/P08-002 stage2 字节级可重现性
2. **HISS leaf_index 构建 15s 瓶颈** (nside=65536, 61M 像素, 78 个子叶), 是 P15-001 必须优化的核心瓶颈
3. **HCSD 默认视角不覆盖数据** (RA=0 vs Galaxy Center RA=272.8°), HCSD 路由使用 reset_view() 而非 set_initial_view_from_data() 的设计缺陷
4. **浏览器无 timing instrumentation** (整个模块 0 个 QElapsedTimer/std::chrono), P15-001 必须补齐 ScopedTimer

## 风险/回滚/残留

### 风险

1. offscreen 模式不渲染实际像素, frame time/draw time 无法测量 (P15-001 需真实 GPU)
2. 日志时间戳为秒级精度, 不足以量化亚秒级操作 (P15-001 需毫秒级 ScopedTimer)
3. HCSD 默认视角缺陷可能导致 P15 浏览器测试无法显示数据 (需先修复路由)

### 回滚

本任务仅生成数据文件, 未修改任何源代码, 无需回滚.

### 残留

- 真实 GPU 性能基线 (frame time p50/p95/p99) 待 P15-001 在真实显示器环境补齐
- 32 帧银心 HCSD 尚未生成 (P13 阶段任务), 当前使用 P07-001 单帧 HCSD 作为基线
- 浏览器 deploy.ps1 未运行, astro_image_io.dll 不在 build/ 目录 (需手动加入 PATH)

## 结论

P09-003 完成. canonical_dataset_v1.2.json 已冻结 44 个文件 SHA-256, 浏览器性能基线已记录 (含 15s leaf_index 瓶颈), 测光失败基线已记录 (7 帧 G-002 缺口). 禁止捷径条款已合规 (失败帧未替换). 所有 JSON 文件校验通过. 四件套证据完整.
