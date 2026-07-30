# H-002 任务执行报告 — 内存预约、CPU回滞和准入控制

- 任务编号: H-002
- 执行日期: 2026-07-30
- 依赖: H-001 (已完成)
- 状态: **已完成**

## 1. 实现摘要

完成了内存预约、CPU回滞和准入控制三大机制:

1. **MemoryBudgetManager**: 内存预约/释放/安全余量, 实现准入公式 `reserved + predicted_peak + uncertainty + OS_margin + worst_next_frame <= budget`
2. **CPUBackpressure**: 系统负载高时自动降低并发度 (90%停止投喂, 70%开始回滞, 线性插值)
3. **AdmissionController**: 新任务准入决策 (ADMIT/DEFER/REJECT), 集成内存预算+CPU回滞+阶段兼容矩阵
4. **PressureHandler**: 8级递进式压力处理状态机 (NORMAL→THROTTLE→STOP_ADMISSION→...→OS_SWAP)

## 2. 关键设计决策

### 2.1 准入公式实现 (规范核心)
```python
can_allocate = (reserved + predicted_peak + uncertainty*factor + os_margin + worst_next_frame) <= budget
```
- **reserved**: 当前已预约内存 (MemoryBudgetManager 追踪)
- **predicted_peak**: FrameCostEstimator 预测的该阶段峰值 (H-001)
- **uncertainty**: 成本模型不确定度 * 放大因子 (默认 1.0, 可调)
- **os_margin**: OS 安全余量 (默认 2GB, 防止 OS swap)
- **worst_next_frame**: 最坏下一帧内存需求 (DRIZZLE 阶段, 保证管线连续性)
- **budget**: 总内存预算 (Commit Limit)

### 2.2 CPU回滞策略
| CPU 负载 | 并发度 | 状态 |
|---------|--------|------|
| < 60% | configured_max (全速) | NORMAL |
| 60-70% | configured_max (全速) | NORMAL |
| 70-90% | 线性插值 (max → 1) | THROTTLE |
| >= 90% | 1 (仅允许完成) | STOP_ADMISSION |

滚动平均 (20 次采样) 避免瞬时抖动。

### 2.3 阶段兼容矩阵
| 阶段 | 可并发 | 互斥 |
|------|--------|------|
| READ_FITS | CALIBRATE, PSF | PLATESOLVE, DRIZZLE |
| CALIBRATE | READ_FITS, SNR | PLATESOLVE, DRIZZLE |
| PLATESOLVE | (无) | 全部 (高内存+高CPU, 独占) |
| PSF | READ_FITS, SNR | PLATESOLVE, DRIZZLE |
| PHOTOMETRIC | SNR | PLATESOLVE, DRIZZLE |
| SNR | READ_FITS, CALIBRATE, PSF, PHOTOMETRIC | PLATESOLVE, DRIZZLE |
| DRIZZLE | (无) | 全部 (高内存+高CPU+高IO, 独占) |

### 2.4 压力处理状态机 (8级递进)
```
NORMAL → THROTTLE → STOP_ADMISSION → WAIT_RELEASE → CLEAR_CACHE → PAUSE → SPILL → OS_SWAP
```
- **SPILL** (H-003 实现): 显式将中间结果写到磁盘, 不依赖 OS swap
- **OS_SWAP**: 最后手段, 应避免 (规范明确禁止依赖 OS swap)

### 2.5 不使用固定并发数替代预算 (规范禁止捷径)
即使 `max_concurrent=10`, 内存不足时仍 DEFER。测试 19 明确验证此约束。

## 3. 测试结果

```
H-002 单元测试: 49 PASS, 0 FAIL

Part 1: MemoryBudgetManager (3 测试) — 预约/释放/预算检查/安全余量
Part 2: CPUBackpressure (4 测试) — 低负载/中负载/高负载/阈值边界
Part 3: AdmissionController (6 测试) — 正常准入/内存不足/CPU过载/释放重入/兼容性/多帧并发
Part 4: PressureHandler (4 测试) — 正常/CPU回滞/停止准入/内存压力
Part 5: 准入公式验证 (2 测试) — 各分量非负/不使用固定并发数替代预算
```

关键验证:
- T2 DRIZZLE (640MB peak) 在 16GB 预算下 ADMIT, 总需求 3.50GB
- T2 DRIZZLE 在 1GB 预算下 DEFER (1.88GB > 1.07GB)
- CPU 95% 时 DEFER (feeding stopped)
- T2+T4 并发准入: 885MB 预约, 两帧均 ADMIT

## 4. 交付物

### Python 原型 (engineering_authoritative/evidence/H-002/)
- `admission_controller.py` — MemoryBudgetManager + CPUBackpressure + AdmissionController + PressureHandler
- `test_h002.py` — 单元测试 (49 项断言)
- `TASK_REPORT.md` — 本报告

### C++ 头文件骨架 (lib/orchestrator/cpp/include/)
- `admission_controller.h` — 准入控制器接口定义 (待实现 .cpp)

## 5. 下一任务前置条件

- H-003 (高峰错峰/显式spill/恢复) 依赖本任务的:
  - `PressureHandler.assess()` — 提供压力等级 (SPILL 级触发显式spill)
  - `AdmissionController.admit()` — 推迟的任务由错峰调度器管理
  - `MemoryBudgetManager.get_reserved()` — spill 后释放预约内存
