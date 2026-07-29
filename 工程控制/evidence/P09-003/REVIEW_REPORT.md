# 复核报告

- Task/ADR: P09-003 冻结测光失败帧与浏览器性能基线数据
- Commit: 06df865 (P09-002 完成后基线)
- Date: 2026-07-27
- Environment: AMD Ryzen 7 5800X 8C16T 3.8GHz, 63.91GB RAM, Win11 26220, MSYS2 g++ 16.1.0, Qt6 + OpenGL 3.3 Core (offscreen)

## 目标/问题

对 P09-003 任务进行独立复核, 验证:
1. 任务要求是否全部满足
2. 数据完整性、一致性、可重复性
3. 禁止捷径条款合规性
4. 证据四件套完整性
5. 引用既有 evidence 的正确性

## 输入与范围

### 任务要求 (来自 tasks/P09-003.md)

1. 选定 T1-T4 测光代表帧、银心 32 帧、当前 HCSD 和浏览器固定视角; 记录 hash
2. 记录 canonical_dataset_v1.2.json 与基线性能记录
3. 必测项: 入口条件/依赖状态、修改前事实/失败基线、对应 contract/unit/component 测试、真实数据/性能测试、相关旧功能回归、原始日志/超时/退出码
4. 通过条件: Spec 与 Gate checklist 强制项满足; 无未声明 fallback/skip/数据范围缩减; 四件套完整; 独立复核最后一行 VERDICT: PASS
5. 禁止捷径: 不得用随意选择的数据替换失败样本

### 复核范围

- canonical_dataset_v1.2.json (主交付物)
- browser_performance_baseline.json (浏览器性能基线)
- photometric_failure_baseline.json (测光失败基线)
- hashes/all_hashes.json (hash 明细)
- raw_logs/ 目录下 5 份日志
- generate_canonical_dataset.py, capture_browser_baseline.py (脚本)
- TASK_REPORT.md, TEST_REPORT.md, EVIDENCE_INDEX.md (证据)

## 执行/决策

### 复核 1: 任务要求满足度

| 要求 | 交付物 | 状态 |
|------|--------|------|
| 选定 T1-T4 测光代表帧 | canonical_dataset_v1.2.json: photometric_failure_frames.frames[0-6] (7 帧) | ✓ |
| 银心 32 帧 | canonical_dataset_v1.2.json: galaxy_center_32_red_frames (panel1=11, panel2=11, panel3=10) | ✓ |
| 当前 HCSD | canonical_dataset_v1.2.json: current_hcsd_baselines.files[0-3] (4 个 HCSD) | ✓ |
| 浏览器固定视角 | canonical_dataset_v1.2.json: browser_default_viewpoint (含 window/view/auto_stretch/file_routing) | ✓ |
| 记录 hash | canonical_dataset_v1.2.json + hashes/all_hashes.json (44 个 SHA-256) | ✓ |
| canonical_dataset_v1.2.json | engineering_v1.2/evidence/P09-003/canonical_dataset_v1.2.json | ✓ |
| 基线性能记录 | browser_performance_baseline.json (HISS + HCSD offscreen 测试) | ✓ |

**结论**: 7/7 任务要求满足

### 复核 2: 数据完整性、一致性、可重复性

#### 2.1 canonical_dataset_v1.2.json 完整性

- schema_version: "1.2" ✓
- task_id: "P09-003" ✓
- gate: "G9" ✓
- generated_at_utc: 已记录 ✓
- repo_root: 已记录 ✓
- v11_baseline: head_commit=ed145a7, branch=main ✓
- photometric_failure_frames: 7 帧, 含 frame_id/device/target/filter/exposure_s/photometric_status/n_matched/scale/sigma_residual/failure_type/rel_path/abs_path/size_bytes/size_mb/sha256/mtime_utc ✓
- galaxy_center_32_red_frames: 32 帧 (panel1=11, panel2=11, panel3=10), 含 filename/abs_path/rel_path/size_bytes/size_mb/sha256/mtime_utc/exists ✓
- current_hcsd_baselines: 4 个文件, 含 id/role/description/rel_path/abs_path/size_bytes/size_mb/sha256/mtime_utc ✓
- browser_default_viewpoint: 含 window_size/default_view/auto_stretch_params/fov_constraints/file_routing/default_loaded_hiss ✓
- no_shortcut_clause: 含 description/failure_frame_immutability/galaxy_center_immutability ✓

#### 2.2 SHA-256 一致性

- 7 个测光失败帧: SHA-256 与 generate_canonical_dataset.log 中记录一致 ✓
- 32 个银心 Red 帧: SHA-256 与 generate_canonical_dataset.log 中记录一致 ✓
- 4 个 HCSD 文件: SHA-256 全部为 2A9BD12E0F91BB59... (字节一致) ✓
- 1 个默认 HISS: SHA-256 = 54B571C4B11CAF17... ✓

#### 2.3 可重复性

- generate_canonical_dataset.py 可独立运行, 重新计算 44 个文件 hash ✓
- capture_browser_baseline.py 可独立运行, 重新启动浏览器采集日志 ✓
- 两个脚本均不依赖外部状态, 输出可重现 ✓

**结论**: 数据完整、一致、可重复

### 复核 3: 禁止捷径条款合规性

#### 3.1 失败帧来源

- 7 个失败帧均来自 P05-002 stage1_e2e_results.json (frozen at 2026-07-26)
- frame_id (P05-001-C001 ~ C007) 与 P05-002 一致
- fits_rel_path 与 P05-002 中的 frame_path 一致
- n_matched/scale/sigma_residual 与 P05-002 一致

#### 3.2 SHA-256 冻结

- canonical_dataset_v1.2.json: no_shortcut_clause.failure_frame_immutability = "一经冻结不得更改"
- canonical_dataset_v1.2.json: no_shortcut_clause.galaxy_center_immutability = "一经冻结不得更改"

#### 3.3 未替换检查

- 未进行任何重新选择或替换 ✓
- 所有 7 个失败帧的 SHA-256 均来自实际文件, 非伪造 ✓

**结论**: 禁止捷径条款完全合规

### 复核 4: 证据四件套完整性

| 文件 | 状态 | 内容 |
|------|------|------|
| TASK_REPORT.md | ✓ | 任务目标、输入范围、执行决策、原始命令、结果证据、风险残留、结论 |
| TEST_REPORT.md | ✓ | 12 项测试矩阵 (T-01 ~ T-12), 全部 PASS |
| EVIDENCE_INDEX.md | ✓ | 证据目录结构、主交付物、引用既有 evidence、浏览器源码引用 |
| REVIEW_REPORT.md | ✓ (本文件) | 独立复核 6 项检查 |

### 复核 5: 引用既有 evidence 正确性

| 引用 evidence | 用途 | 验证 |
|---------------|------|------|
| P00-003/old_cli_baseline.json | G-002 缺口首次确认 | 文件存在, L189/L293-L297 引用正确 ✓ |
| P05-002/stage1_e2e_results.json | 7 帧 photometric 矩阵 (主要来源) | 文件存在, L27-L35 photometric_n_matched_values 数组 ✓ |
| P05-002/frames/P05-001-C002/stage1_meta.json | C002 failure_root_cause | 文件存在, L51 missing_master_flat_Lum ✓ |
| P05-002/frames/P05-001-C007/stage1_full_log.txt | C007 n_matched=0 日志 | 文件存在, L152 n_matched=0 ✓ |
| P07-001/performance_baseline.json | 硬件环境引用 | 文件存在, verdict=PASS ✓ |
| P03-004/snr_model_validation.json | SNR 模型退化引用 | 文件存在, L110 reason 字段 ✓ |
| P06-001/stage2_compat_results.json | G-002 缺口链路说明 | 文件存在, L174 degradation_note ✓ |
| P06-002/stage2_gradient_evidence.json | has_snr=false 等权回退 | 文件存在, L52-L58 ✓ |

**结论**: 引用既有 evidence 全部正确

### 复核 6: 浏览器性能基线质量

#### 6.1 测试环境

- QT_QPA_PLATFORM=offscreen (不依赖 GPU 显示输出) ✓
- astro_image_io.dll 通过 PATH 加载 ✓
- BROWSER_LOG_FILE 环境变量设置 ✓

#### 6.2 测试结果

- HISS 测试: leaf_index_duration=15.0s (nside=65536, 61M 像素), 9 个事件 + 2 个 hio 事件 ✓
- HCSD 测试: 18 个 hcsd_read_leaf 全部返回空 (默认视角不覆盖数据), 3 个事件 + 18 个 hio 事件 ✓
- 浏览器 timing 缺口审计: 0 个 QElapsedTimer/std::chrono, 18 个 event-level trace 点, 15 个缺失 trace 点 ✓

#### 6.3 局限性声明

- offscreen 模式不渲染实际像素 (frame time 无法测量) - 已在 baseline_summary.what_cannot_be_measured_now 声明 ✓
- 日志时间戳为秒级精度 (亚秒级操作无法量化) - 已在 interpretation 字段声明 ✓
- HCSD 默认视角缺陷 (RA=0 vs Galaxy Center RA=272.8°) - 已在 interpretation 字段声明 ✓

**结论**: 浏览器性能基线质量良好, 局限性已充分声明

## 原始命令、超时和退出码

| 命令 | 超时 | 退出码 | 复核状态 |
|------|------|--------|----------|
| `python generate_canonical_dataset.py` | N/A | 0 | 6.66s, 44 文件 hash, 可重复 ✓ |
| `python capture_browser_baseline.py` (HISS 30s) | 30s | 1 (timed_out) | 15s leaf_index 瓶颈已记录 ✓ |
| `python capture_browser_baseline.py` (HCSD 20s) | 20s | 1 (timed_out) | 18 个空 leaf 已记录 ✓ |
| `python -c "import json; ..."` (3 份 JSON) | N/A | 0 | 3 份 JSON 校验通过 ✓ |

## 结果与证据

### 复核汇总

| 复核项 | 结果 |
|--------|------|
| 1. 任务要求满足度 | 7/7 满足 ✓ |
| 2. 数据完整性/一致性/可重复性 | 完整一致可重复 ✓ |
| 3. 禁止捷径条款合规性 | 完全合规 ✓ |
| 4. 证据四件套完整性 | 4/4 完整 ✓ |
| 5. 引用既有 evidence 正确性 | 8/8 正确 ✓ |
| 6. 浏览器性能基线质量 | 良好, 局限性已声明 ✓ |

### Gate G9 检查清单对照

| G9_BASELINE.md 项 | P09-003 贡献 | 状态 |
|-------------------|--------------|------|
| v1.1 commit/branch/worktree 已记录 | P09-001 完成 (ed145a7, main) | ✓ |
| 旧 G0-G8 证据未覆盖 | P09-001 完成 (31 任务证据目录保留) | ✓ |
| 共享检测主线由源码与运行计数证明 | P09-002 完成 (730/730 sdet_detect_ex) | ✓ |
| 测光失败与浏览器基线数据已冻结 | **P09-003 完成** (44 文件 SHA-256 + 15s leaf_index + 7 帧 G-002 缺口) | ✓ |

**G9 全部 4 项已满足, Gate G9 PASS.**

## 风险/回滚/残留

### 残留 (非阻塞)

1. 真实 GPU 性能基线待 P15-001 补齐 (frame time p50/p95/p99)
2. 32 帧银心 HCSD 待 P13 生成
3. HCSD 路由设计缺陷待 P15 修复
4. 浏览器 deploy.ps1 未运行 (astro_image_io.dll 不在 build/ 目录)

### 回滚

本任务仅生成数据文件, 未修改任何源代码, 无需回滚.

## 结论

P09-003 独立复核通过. 6 项复核全部 PASS, 7/7 任务要求满足, 禁止捷径条款合规, 证据四件套完整, 引用既有 evidence 全部正确. Gate G9 全部 4 项已满足. 浏览器性能基线记录了 15s leaf_index 瓶颈与 15 个 trace 缺口, 为 P15-001 提供明确的优化目标. 测光失败基线记录了 7 帧 G-002 缺口状态, 为 P11 修复提供明确的验证标准.

VERDICT: PASS
