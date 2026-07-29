# P13-001 — 建立 Stage1 全 TestData 批处理与恢复入口 — TASK_REPORT

| 字段 | 值 |
| --- | --- |
| 任务 ID | P13-001 |
| 阶段 | P13 |
| Gate | G12 (Photometric Diagnostic Gate) |
| 依赖 | P12-006 |
| 参考 Spec | `docs/17_TEST_ARCHITECTURE_AND_FULL_REGRESSION.md` |
| 执行日期 | 2026-07-29 |
| 状态 | DONE |
| Verdict | PASS (runner 建立 + 10/10 测试 PASS + 冒烟端到端 PASS) |

## 1. 目标

建立 Stage1 全 TestData 批处理与恢复入口，实现有超时、hash 缓存、断点和分类报告的批处理框架。

## 2. 交付物

### 2.1 Runner 脚本：`scripts/stage1_batch_runner.py`

**入口子命令**：

| 子命令 | 功能 |
| --- | --- |
| `scan` | 扫描 testdata 得到 710 帧清单 |
| `run` | 运行批处理（默认断点恢复，自动跳过 hash 匹配的 PASS 帧） |
| `status` | 查看批处理进度（从 `batch_state.json`） |
| `report` | 从 `batch_state.json` 重新生成报告 |
| `cache-list` | 列出 cache 目录 |
| `cache-clear` | 清空 cache 目录 |

**CLI 过滤参数**（`run` 子命令）：
- `--device T2 T3 T4` 按设备过滤
- `--target Galaxy_Center NGC55` 按目标过滤
- `--filter LUM RED GREEN BLUE HA OIII` 按滤镜过滤
- `--dataset <dir>` 按数据集目录过滤
- `--limit N` 限制帧数
- `--timeout S` 单帧超时秒数（默认 600s）
- `--fresh` 清空 cache + state 重跑
- `--dry-run` 仅打印 hash_key 不执行

### 2.2 自动测试：`scripts/test_stage1_batch_runner.py`

10 个测试用例覆盖：扫描、文件名解析、hash 稳定性、cache 读写、state 读写、断点恢复、`--fresh` 清空、失败分类、过滤器、端到端冒烟。

### 2.3 Usage 文档

```
python stage1_batch_runner.py scan                       # 扫描 testdata 得到帧清单
python stage1_batch_runner.py run --limit 5              # 小规模冒烟（前 5 帧）
python stage1_batch_runner.py run --device T4            # 仅运行 T4 设备帧
python stage1_batch_runner.py run --target Galaxy_Center # 仅运行 Galaxy_Center 数据集帧
python stage1_batch_runner.py run --filter HA            # 仅运行 HA 滤镜帧
python stage1_batch_runner.py run --fresh                # 清空 cache + state 重跑
python stage1_batch_runner.py run --dry-run              # 仅打印 hash_key 不执行
python stage1_batch_runner.py run --timeout 900          # 单帧超时 900s
python stage1_batch_runner.py status                     # 查看批处理进度
python stage1_batch_runner.py report                     # 从 batch_state.json 重新生成报告
python stage1_batch_runner.py cache-list                  # 列出 cache 目录
python stage1_batch_runner.py cache-clear                 # 清空 cache 目录
```

## 3. 设计实现

### 3.1 Hash 缓存（禁止捷径）

每帧缓存绑定 7 元 hash，任一变更则该帧缓存失效，强制重跑：

```python
hash_key = sha256(concat(
    git_commit_hash,                  # 代码版本
    orchestrator.exe_sha256,          # 二进制版本
    stage1_config_<device>.json_sha256,  # 设备配置
    filters.json_sha256,              # 滤光片曲线
    qe_curves.json_sha256,            # QE 曲线
    input_fits_sha256,                # 输入帧数据
    gaia_data_dir_name,               # Gaia 数据目录
))
```

- 仅 `PASS` 帧会被跳过；`FAIL`/`STAGE1_ERROR`/`TIMEOUT` 仍会重跑
- 缓存写入 `cache/<frame_id>.json`，原子写入（`.tmp` + `os.replace`）

### 3.2 断点恢复

- `batch_state.json` 持久化每帧状态（status/exit_code/elapsed_s/hash_key/fit_used/scale_factor/sigma_residual/has_snr/snr_n_points）
- `run` 子命令默认加载已有 state，跳过已完成帧
- `--fresh` 清空 cache + state 重新开始
- 当 `git_commit` 或 `orchestrator_sha256` 变化时，自动重置 state（保留 cache）

### 3.3 超时保护

- `subprocess.run(timeout=...)` 单帧超时保护（默认 600s，可配置）
- 超时分类为 `TIMEOUT`，记录到 state 和 cache

### 3.4 分类报告

| 报告文件 | 内容 |
| --- | --- |
| `reports/batch_results.csv` | 全量帧结果（含 22 列字段） |
| `reports/batch_summary.json` | 按 status/device/dataset/filter 汇总统计 |
| `reports/failure_classification.json` | 失败帧分类详情 |
| `reports/frame_inventory.json` | `scan` 子命令输出的帧清单 |

### 3.5 HISS inspect（内嵌）

`run_single_frame` 内嵌 HISS 文件 inspect（读取 HISS header 的 `has_snr` 和 `snr_n_points` 字段），无需依赖 `aio_healpix_io` 外部库。

## 4. testdata 扫描规则

按 `evidence/P11-005/DATASETS.md` 规则扫描 7 个数据集：

| 数据集目录 | 设备 | 扫描模式 | 帧数 |
| --- | --- | --- | --- |
| `Victory_Nebula_T4_Flying_Dutchman` | T4 | 扁平 `lights/<bn>.fts` | 228 |
| `Galaxy_Center_T4` | T4 | 嵌套 `lights/panel<N>/<bn>.fts` | 157 |
| `NGC55_T3_flying_dutchman` | T3 | 扁平 | 79 |
| `NGC247_T2_flying_dutchman` | T2 | 扁平 | 68 |
| `NGC1727_T2_flying_dutchman` | T2 | 扁平 | 64 |
| `NGC83_cluster_T3_Flying_Dutchman` | T3 | 扁平 | 72 |
| `LDN43_T2_flying_dutchman` | T2 | 扁平（ASCII junction） | 42 |
| **合计** | | | **710** |

**关键修复**：Galaxy_Center_T4 的实际目录结构是 `lights/panel<N>/<bn>.fts`（panel 下直接是 .fts 文件，**无** filter 子目录）。初始代码错误假设为 `panel<N>/<filter>/<bn>.fts`，导致扫描结果只有 553 帧（缺失 157 帧 Galaxy_Center）。修复后正确扫描到 710 帧。

## 5. 验证结果

### 5.1 自动测试结果

```
======================================================================
P13-001 stage1_batch_runner.py 自动测试
======================================================================

--- test_scan_testdata ---
  扫描到 710 帧
  按设备: {'T4': 385, 'T3': 151, 'T2': 174}
  按数据集: {'Victory_Nebula_T4_Flying_Dutchman': 228, 'Galaxy_Center_T4': 157,
            'NGC55_T3_flying_dutchman': 79, 'NGC247_T2_flying_dutchman': 68,
            'NGC1727_T2_flying_dutchman': 64, 'NGC83_cluster_T3_Flying_Dutchman': 72,
            'LDN43_T2_flying_dutchman': 42}

--- test_smoke_run_1_frame ---
  测试帧: T4_Galaxy_Center_RED_Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@061703-180S-Red
  [T4_Galaxy_Center_RED_...] exit=0 elapsed=29.2s fit_used=1670 scale=0.002836
                            sigma=0.1816 has_snr=1 → PASS
  PASS: 所有 Gate 通过

======================================================================
测试结果: 795 PASS, 0 FAIL
VERDICT: PASS (all tests passed)
```

| 测试用例 | 结果 |
| --- | --- |
| test_scan_testdata（710 帧） | PASS |
| test_canonical_filter_from_filename（7 case） | PASS |
| test_compute_hash_key_stable | PASS |
| test_cache_save_load | PASS |
| test_state_save_load | PASS |
| test_breakpoint_resume_skips_pass | PASS |
| test_fresh_clears_state_and_cache | PASS |
| test_failure_classification（5 case） | PASS |
| test_filter_frames（5 组合） | PASS |
| test_smoke_run_1_frame（端到端冒烟） | PASS |

### 5.2 冒烟测试关键指标

| 指标 | 值 | 阈值 | 结论 |
| --- | --- | --- | --- |
| exit_code | 0 | 0 | PASS |
| elapsed_s | 29.2 | < 600 | PASS |
| fit_used | 1670 | >= 20 (Broadband) | PASS |
| scale_factor | 0.002836 | > 0 | PASS |
| sigma_residual | 0.1816 | > 0 且有限 | PASS |
| has_snr | 1 | 1 | PASS |
| snr_n_points | 1984 | > 0 | PASS |
| hash_key 长度 | 64 字符 | 64 | PASS |
| cache 写入 | 是 | 是 | PASS |
| batch_state 更新 | 是 | 是 | PASS |

## 6. 修改文件清单

| 文件 | 类型 | 说明 |
| --- | --- | --- |
| `engineering_v1.3/evidence/P13-001/scripts/stage1_batch_runner.py` | 新增 | Stage1 批处理 runner（710 帧，hash 缓存 + 断点 + 超时 + 分类报告） |
| `engineering_v1.3/evidence/P13-001/scripts/test_stage1_batch_runner.py` | 新增 | 自动测试（10 用例，795 断言） |
| `engineering_v1.3/evidence/P13-001/raw_logs/test_run_20260729_120344.log` | 新增 | 测试运行日志 |

## 7. 未声明的 fallback / skip 检查

- **无 fallback**：冒烟测试使用真实 `orchestrator.exe stage1` 流水线，未使用任何降级路径。
- **无 skip**：hash 缓存检查是设计功能（非捷径），仅 `PASS` 帧会被跳过且仅当 7 元 hash 全部匹配时。
- **无数据范围缩减**：扫描覆盖 testdata 全部 7 个数据集 710 帧（与 `DATASETS.md` 一致），无任何数据集被跳过。
- **无 weight=1 默认**：HISS inspect 直接读取 header 的 `has_snr`/`snr_n_points`，未使用默认值。

## 8. 通过条件核对

| 条件 | 状态 |
| --- | --- |
| 1. Spec 和 Gate checklist 强制项全部满足 | ✓（runner 具备超时/hash缓存/断点/分类报告） |
| 2. 无未声明的 fallback/skip/数据范围缩减 | ✓（见第 7 节） |
| 3. TASK/TEST/EVIDENCE/REVIEW 完整 | ✓（本任务生成 4 件套） |
| 4. 独立复核 VERDICT: PASS | ✓（见 REVIEW_REPORT.md） |
| 5. 缓存绑定 commit/config/input hash | ✓（7 元 hash，见第 3.1 节） |

## 9. 后续工作

P13-001 完成后，下一任务由 MASTER_TASK_REGISTER 决定。建议进入 P13-002（全量 710 帧批处理执行），使用本任务建立的 runner 完成全量 stage1 处理。
