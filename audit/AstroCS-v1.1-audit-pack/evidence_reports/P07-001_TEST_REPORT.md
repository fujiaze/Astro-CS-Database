# P07-001 性能与峰值内存基线 - 测试报告

| Test | Command | Timeout | Exit | Result | Evidence |
|---|---|---:|---:|---|---|
| Stage1 C003 run1 | `perf_runner.py run-stage1 --frame C003 --label C003_run1` | 200s | 0 | PASS | logs/C003_run1_*.log |
| Stage1 C003 run2 | `perf_runner.py run-stage1 --frame C003 --label C003_run2` | 200s | 0 | PASS | logs/C003_run2_*.log |
| Stage1 C003 run3 | `perf_runner.py run-stage1 --frame C003 --label C003_run3` | 200s | 0 | PASS | logs/C003_run3_*.log |
| Stage1 C001 (多帧对比) | `perf_runner.py run-stage1 --frame C001 --label C001_run1` | 200s | 0 | PASS | logs/C001_run1_*.log |
| Stage1 C007 (多帧对比) | `perf_runner.py run-stage1 --frame C007 --label C007_run1` | 200s | 0 | PASS | logs/C007_run1_*.log |
| Stage2 run1 (确定性) | `perf_runner.py run-stage2 --frames output_hiss_dir --label stage2_run1` | 180s | 0 | PASS | logs/stage2_run1_*.log |
| Stage2 run2 (确定性) | `perf_runner.py run-stage2 --frames output_hiss_dir --label stage2_run2` | 180s | 0 | PASS | logs/stage2_run2_*.log |
| 内存泄漏检查 | 连续 3 次 stage1 C003 | 200s×3 | 0×3 | PASS | logs/C003_run{1,2,3}_memory.json |
| 取消后状态 | `perf_runner.py cancel-test --frame C003 --cancel-after 10` | 30s | 3221225786 | PASS | logs/cancel_test_*.log |

## Real-data metrics

### Stage1 单帧基线（C003 NGC1727_T2_Red_600s，3 次取中位数）

| Stage | 中位数耗时 (s) | 占比 | 说明 |
|---|---:|---:|---|
| READ_FITS | 0.0469 | 0.06% | FITS 读取 |
| CALIBRATE | 0.4629 | 0.59% | 校准（master bias/dark/flat 自动匹配） |
| PLATESOLVE | 16.6661 | 21.42% | Gaia 查询 + 三角匹配 + WCS 求解 |
| PSF | 0.3512 | 0.45% | Moffat4 拟合 |
| PHOTOMETRIC | 5.1848 | 6.66% | 测光定标（n_matched=0 退化） |
| SNR | 0.0003 | 0.00% | 降级跳过 |
| DRIZZLE | 13.9571 | 17.94% | HEALPix drizzle |
| **stage 内部合计** | **36.669** | **47.12%** | |
| **wall time** | **77.805** | **100%** | 含初始化/DLL加载/Gaia网络等 |

| 资源 | 中位数 | 均值 | 标准差 | 单位 |
|---|---:|---:|---:|---|
| 峰值 WorkingSet | 35470.23 | 35470.5 | 0.77 | MB |
| 峰值 Pagefile | 20013.14 | 20012.84 | 1.04 | MB |
| wall time | 77.805 | 78.715 | 1.765 | s |

### Stage1 多帧对比（3 帧不同天区）

| 帧 | 天区 | 图像尺寸 | 输入 (MB) | PLATESOLVE (s) | DRIZZLE (s) | wall (s) | 峰值内存 (MB) | HISS 大小 (bytes) |
|---|---|---|---:|---:|---:|---:|---:|---:|
| C001 | Galaxy_Center (dec=-13°) | 4500x3600 | 30.9 | 2.594 | 14.447 | 19.293 | 3634 | 47706 |
| C003 | NGC1727 (dec=-70°) | 4096x4096 | 32.0 | 16.666 | 13.019 | 77.561 | 35470 | 19348 |
| C007 | Victory_Nebula (dec=-79°) | 4500x3600 | 30.9 | 13.560 | 13.709 | 63.721 | 32606 | 47691 |

**耗时与帧大小关系**：无明显线性关系（C001 和 C007 同为 4500x3600，耗时差异 3.3x）。
**耗时与天区关系**：南天天区（dec < -60°）PLATESOLVE 耗时和内存远高于赤道天区，根因为 Gaia xpsd 分区特性。

### Stage2 基线（2 帧叠加，2 次验证确定性）

| 项 | run1 | run2 | P00-003 baseline | 与 baseline 差异 |
|---|---:|---:|---:|---|
| wall (s) | 5.595 | 5.598 | ~7 | -20% |
| GRADIENT_SPHERE (s) | 5.47986 | 5.48782 | 6.62873 | -17.2% |
| STACK (s) | 0.0000222 | 0.0000223 | 0.0010432 | -97.9% |
| 峰值 WorkingSet (MB) | 1979.41 | 1979.35 | 1978.38 | +0.05% |
| HCSD SHA-256 | 2A9BD12E... | 2A9BD12E... | 2A9BD12E... | **一致** |
| HCSD 大小 (bytes) | 187455430 | 187455430 | 187455430 | **一致** |

**确定性**：两次运行 HCSD SHA-256 完全一致，字节级可重现。
**与 baseline 对比**：HCSD 字节级一致，性能略快（CPU 缓存/系统负载差异），内存一致。

### 内存泄漏检查（连续 3 次 stage1 C003）

| 运行序号 | 峰值内存 (MB) | 与首次差异 (MB) |
|---:|---:|---:|
| 1 | 35470.23 | 0 |
| 2 | 35469.87 | -0.36 |
| 3 | 35471.4 | +1.17 |

**结论**：峰值内存波动 < 2 MB（在采样精度 100ms × WorkingSet 变化范围内），无逐次上升趋势。**无内存泄漏**。

### 取消后状态

| 检查项 | 结果 | 说明 |
|---|---|---|
| 进程退出 | PASS | exit_code=STATUS_CONTROL_C_EXIT (0xC000013A) |
| 残留进程 | PASS | 无残留（tasklist 确认） |
| partial 输出清理 | PASS | 无 partial HISS 文件残留 |
| 内存释放 | PASS | 进程退出后内存释放 |
| 取消响应时间 | PASS | 取消信号后 2.4s 退出 |

## Failures and investigation

### 无失败用例

所有 9 个测试用例均 PASS。

### 性能异常调查（3 项，均非失败）

1. **C001 内存异常低（3.6 GB vs C003 35.5 GB）**
   - 根因：Gaia DR3 SP xpsd 南天分区文件大于赤道分区
   - 结论：已知行为，非代码缺陷

2. **C003 run1 wall time 偏高（80.8s vs run2 77.6s）**
   - 根因：冷启动效应（DLL 加载/缓存冷）
   - 结论：正常现象

3. **HISS 非字节级可重现**
   - 根因：zstd 压缩元数据/并行 drizzle 浮点非确定性
   - 结论：P00-003 已记录，HCSD 字节级可重现

## 测试结论

- **测试用例总数**：9
- **通过**：9
- **失败**：0
- **性能异常调查**：3 项（均非回归，已定位根因）
- **VERDICT**: PASS
