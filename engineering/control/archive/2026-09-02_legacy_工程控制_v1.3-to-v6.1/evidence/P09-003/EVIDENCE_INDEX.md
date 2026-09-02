# 证据索引

- Task/ADR: P09-003 冻结测光失败帧与浏览器性能基线数据
- Commit: 06df865 (P09-002 完成后基线)
- Date: 2026-07-27
- Environment: AMD Ryzen 7 5800X 8C16T 3.8GHz, 63.91GB RAM, Win11 26220, MSYS2 g++ 16.1.0, Qt6 + OpenGL 3.3 Core (offscreen)

## 目标/问题

索引 P09-003 任务产生的全部证据文件, 建立可追溯的交付物清单.

## 输入与范围

### 任务定义

- 任务文件: `engineering_v1.2/tasks/P09-003.md`
- 阶段: P09
- Gate: G9
- 依赖: P09-001
- 参考: `docs/12_BROWSER_PERFORMANCE_BASELINE_SPEC.md`

### 预期交付

- canonical_dataset_v1.2.json (主交付物)
- 基线性能记录
- 四件套证据 (TASK/TEST/EVIDENCE_INDEX/REVIEW)

## 执行/决策

### 证据目录结构

```
engineering_v1.2/evidence/P09-003/
├── TASK_REPORT.md                          # 任务报告
├── TEST_REPORT.md                          # 测试报告 (12/12 PASS)
├── EVIDENCE_INDEX.md                       # 本文件
├── REVIEW_REPORT.md                        # 独立复核报告
├── canonical_dataset_v1.2.json             # 主交付物: 44 文件 SHA-256 + 浏览器配置
├── browser_performance_baseline.json       # 浏览器性能基线
├── photometric_failure_baseline.json       # 测光失败基线
├── generate_canonical_dataset.py          # hash 计算脚本
├── capture_browser_baseline.py            # 浏览器性能采集脚本
├── hashes/
│   └── all_hashes.json                     # hash 明细
└── raw_logs/
    ├── generate_canonical_dataset.log      # hash 计算日志
    ├── browser_hiss_stderr.log             # HISS 测试事件日志
    ├── browser_hiss_stdout.log             # HISS 测试 stdout (空)
    ├── browser_hcsd_stderr.log             # HCSD 测试事件日志
    └── browser_hcsd_stdout.log             # HCSD 测试 stdout (空)
```

## 原始命令、超时和退出码

| 命令 | 超时 | 退出码 | 日志文件 |
|------|------|--------|----------|
| `python generate_canonical_dataset.py` | N/A | 0 | raw_logs/generate_canonical_dataset.log |
| `python capture_browser_baseline.py` (HISS 30s + HCSD 20s) | 50s 总 | 0 | raw_logs/browser_hiss_*.log, browser_hcsd_*.log |
| `python -c "import json; ..."` (3 份 JSON 校验) | N/A | 0 | stdout |

## 结果与证据

### 主交付物: canonical_dataset_v1.2.json

| 字段 | 内容 | 文件数 |
|------|------|--------|
| photometric_failure_frames | 7 个 canonical 测光失败帧 (T1-T4) | 7 |
| galaxy_center_32_red_frames | 银心三片 32 Red 帧 (panel1=11, panel2=11, panel3=10) | 32 |
| current_hcsd_baselines | 4 个 HCSD 基线文件 (P07-001/P07-002/P08-002) | 4 |
| browser_default_viewpoint | 浏览器默认 HISS + 固定视角配置 | 1 |
| **总计** | | **44** |

### 浏览器性能基线: browser_performance_baseline.json

| 测试 | 文件 | wall_clock | leaf_index_duration | 事件数 | 关键发现 |
|------|------|-----------|---------------------|--------|----------|
| HISS offscreen | T4_2x_nside65536.hiss (705MB) | 30.066s | 15.0s | 9 + 2 hio | 15s leaf_index 瓶颈 |
| HCSD offscreen | stage2_run1.hcsd (178MB) | 20.022s | N/A | 3 + 18 hio | 默认视角不覆盖数据 |

### 测光失败基线: photometric_failure_baseline.json

| 失败类型 | 帧数 | 帧列表 |
|----------|------|--------|
| zero_match (n_matched=0) | 2 | C002, C007 |
| single_point_match (n_matched=1) | 5 | C001, C003, C004, C005, C006 |
| sigma_residual=0.0 | 7 | 全部 |
| has_snr=false | 7 | 全部 |

### 引用既有 evidence

| evidence 路径 | 用途 |
|---------------|------|
| engineering/evidence/P00-003/old_cli_baseline.json | G-002 缺口首次确认 |
| engineering/evidence/P05-002/stage1_e2e_results.json | 7 帧 photometric 完整矩阵 (主要来源) |
| engineering/evidence/P05-002/frames/P05-001-C002/stage1_meta.json | C002 failure_root_cause |
| engineering/evidence/P05-002/frames/P05-001-C007/stage1_full_log.txt | C007 n_matched=0 日志 |
| engineering/evidence/P03-001/lum_T2_noFlat_run.log | C002 副本日志 |
| engineering/evidence/P03-002/orchestrator_test_normal.log | 多次运行一致性 |
| engineering/evidence/P07-001/performance_baseline.json | 硬件环境引用 |
| engineering/evidence/P07-001/TEST_REPORT.md | C003 n_matched=0 标注 |
| engineering/evidence/P03-004/snr_model_validation.json | SNR 模型退化引用 photometric |
| engineering/evidence/P06-001/stage2_compat_results.json | G-002 缺口链路说明 |
| engineering/evidence/P06-002/stage2_gradient_evidence.json | has_snr=false 等权回退 |

### 浏览器源码引用

| 源文件 | 引用内容 |
|--------|----------|
| lib/healpix_db/healpix_browser_qt/widgets/sphere_view.h L102-112 | FOV 约束常量 |
| lib/healpix_db/healpix_browser_qt/widgets/sphere_view.cpp L54-63 | reset_view() 默认值 |
| lib/healpix_db/healpix_browser_qt/widgets/sphere_view.cpp L93-111 | set_initial_view_from_data() |
| lib/healpix_db/healpix_browser_qt/core/stf_engine.cpp L71-125 | auto_stretch() 参数 |
| lib/healpix_db/healpix_browser_qt/app/main_window.cpp L41 | resize(1280, 800) |
| lib/healpix_db/healpix_browser_qt/app/main_window.cpp L200-226 | 文件路由 (.hiss/.hcsd) |
| lib/healpix_db/healpix_browser_qt/app/main.cpp L49-53 | BROWSER_LOG_FILE 实现 |
| lib/healpix_db/healpix_browser_qt/core/logger.h | 日志接口 (内存缓冲) |
| lib/healpix_db/healpix_browser_qt/core/browser_backend.cpp | 18 个 trace 点 |
| lib/healpix_db/healpix_browser_qt/core/gl_renderer.cpp | 渲染器 trace 点 |

## 风险/回滚/残留

### 残留

1. 真实 GPU 性能基线 (frame time p50/p95/p99) 待 P15-001 在真实显示器环境补齐
2. 32 帧银心 HCSD 尚未生成 (P13 阶段任务)
3. HCSD 路由设计缺陷 (reset_view vs set_initial_view_from_data) 待 P15 修复
4. 浏览器 deploy.ps1 未运行, astro_image_io.dll 不在 build/ 目录

## 结论

P09-003 证据索引完整. 主交付物 canonical_dataset_v1.2.json 包含 44 个文件 SHA-256, 浏览器性能基线记录了 15s leaf_index 瓶颈与 15 个 trace 缺口, 测光失败基线记录了 7 帧 G-002 缺口状态. 所有原始日志已归档. 引用既有 evidence 11 项, 浏览器源码引用 10 项.
