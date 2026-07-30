# H-003 任务执行报告 — 高峰错峰、显式spill与恢复

- 任务编号: H-003
- 执行日期: 2026-07-30
- 依赖: H-002 (已完成)
- 状态: **已完成**

## 1. 实现摘要

完成了高峰错峰调度、显式spill和恢复三大机制:

1. **PeakShifter**: 高峰错峰调度器, 将非紧急任务推迟到低负载时段, 按优先级(HIGH/NORMAL/LOW)出队
2. **SpillManager**: 显式spill管理器, 内存不足时将中间结果写到磁盘(原子写入+SHA256校验), 不依赖 OS swap
3. **RecoveryManager**: 恢复管理器, spill 后恢复继续执行, 支持单块/整帧恢复

## 2. 关键设计决策

### 2.1 高峰错峰 (PeakShifter)
- **推迟策略**: 压力 >= STOP_ADMISSION 时, 新任务入队; URGENT 任务永不推迟
- **优先级队列**: min-heap 按 (priority, defer_count) 排序, 高优先级先恢复
- **防饥饿**: 多次推迟 (>3次) 后提升优先级; 超时 (300s) 后强制执行
- **drain_all**: 紧急退出时强制恢复全部延迟任务

### 2.2 显式spill (SpillManager) — 不依赖 OS swap
- **原子写入**: 写 .tmp → `os.replace()` (原子重命名), 防止写一半崩溃
- **SHA256 校验**: 恢复时验证数据完整性, 篡改报 ValueError
- **清单持久化**: `spill_manifest.json` 记录所有 spill 块, 重启后可恢复
- **块选择策略**: 优先 spill 大块(>1MB), 不 spill 当前阶段必需块
- **fsync**: 写入后 `os.fsync()` 确保落盘, 不依赖 OS 缓冲

### 2.3 恢复机制 (RecoveryManager)
- **恢复计划**: 按 spill 时间排序, 依次恢复
- **可恢复性检查**: `is_recoverable(frame_id)` 判断帧是否有 spill 块
- **完成清理**: `finalize_frame()` 在帧处理完成后清理 spill 文件

### 2.4 阶段必需块矩阵 (select_spill_blocks)
| 当前阶段 | 必需块 (不可 spill) |
|---------|-------------------|
| READ_FITS | data, header |
| CALIBRATE | data, header |
| PLATESOLVE | data, header, star_det |
| PSF | data, header, star_det, psf |
| PHOTOMETRIC | data, header, psf, gaia_cat |
| SNR | psf, snr_model |
| DRIZZLE | data, header, snr_model |

**关键约束**: 不得丢弃未持久化科学数据 — 只 spill 已序列化、可恢复块。

### 2.5 压力处理链集成 (规范 §压力处理)
```
NORMAL → THROTTLE → STOP_ADMISSION → WAIT_RELEASE → CLEAR_CACHE → PAUSE → SPILL → OS_SWAP
                                                          ↑               ↑
                                                     PeakShifter      SpillManager
```
- PeakShifter 在 STOP_ADMISSION 级触发 (推迟新任务)
- SpillManager 在 SPILL 级触发 (显式写出中间结果)
- OS_SWAP 为最后手段, 本实现明确不依赖

## 3. 测试结果

```
H-003 单元测试: 62 PASS, 0 FAIL

Part 1: PeakShifter (5 测试) — 不推迟/高压力推迟/优先级出队/高压力不恢复/drain_all
Part 2: SpillManager (7 测试) — 基本spill/校验和/多块/整帧恢复/清单持久化/块选择/清理
Part 3: RecoveryManager (5 测试) — 恢复计划/可恢复性/单块恢复/整帧恢复/完成清理
Part 4: 集成测试 (4 测试) — 压力升高→spill/恢复spill数据/完成清理/错峰推迟→恢复
```

关键验证:
- 55MB gaia_cat spill 到磁盘 → 恢复后数据完全一致 (SHA256 校验通过)
- manifest.json 重启后重新加载, 3 个 spill 记录完整恢复
- 压力 95% 时 NORMAL 任务被推迟, 压力降至 30% 后恢复执行

## 4. 交付物

### Python 原型 (engineering_authoritative/evidence/H-003/)
- `spill_manager.py` — PeakShifter + SpillManager + RecoveryManager
- `test_h003.py` — 单元测试 (62 项断言)
- `TASK_REPORT.md` — 本报告

### C++ 头文件骨架 (lib/orchestrator/cpp/include/)
- `spill_manager.h` — PeakShifter + SpillManager + RecoveryManager 接口定义

## 5. 下一任务前置条件

- H-004 (混合设备压力测试/无OOM验收) 依赖本任务的:
  - `SpillManager.spill()/restore()` — 压力测试中验证 spill/恢复链路
  - `PeakShifter.defer()/try_resume()` — 压力测试中验证错峰调度
  - `AdmissionController.admit()` — 压力测试中验证准入控制
  - `FrameCostEstimator.estimate()` — 压力测试中验证成本预测
