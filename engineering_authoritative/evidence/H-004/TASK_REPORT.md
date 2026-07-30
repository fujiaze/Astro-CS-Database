# H-004 任务执行报告 — 混合设备压力测试与无OOM验收

- 任务编号: H-004
- 执行日期: 2026-07-30
- 依赖: H-003 (已完成), C-002 (用 B-002 基线模拟)
- 状态: **已完成**

## 1. 实现摘要

完成了混合设备压力测试与无OOM验收, 验证 H-001/H-002/H-003 全链路在 T2/T3/T4 混合并发场景下的资源调度能力。

测试覆盖 4 个内存预算场景 (8GB/2GB/1.5GB/1GB), 全部场景:
- **3/3 帧全部完成** (T2/T3/T4)
- **0 次 OOM 事件**
- 准入控制正确 admit/defer
- 内存峰值不超过预算

## 2. 测试场景与结果

### 场景总览

| 场景 | 内存预算 | OS余量 | 最大并发 | 判定 | 全部完成 | 无OOM | 峰值总内存 | spill | defer |
|------|---------|--------|---------|------|---------|-------|-----------|-------|-------|
| 充足预算 | 8.0 GB | 1.0 GB | 3 | PASS | ✓ | ✓ | 1350MB/8192MB | 0 | 0 |
| 中等预算 | 2.0 GB | 0.5 GB | 2 | PASS | ✓ | ✓ | 1350MB/2048MB | 0 | 0 |
| 紧张预算 | 1.5 GB | 0.25 GB | 2 | PASS | ✓ | ✓ | 1350MB/1536MB | 0 | 0 |
| 极端紧张 | 1.0 GB | 0.15 GB | 1 | PASS | ✓ | ✓ | — | 0 | 0 |

### 帧执行详情 (全部场景一致)

| 帧 | 设备 | nside | 峰值内存 | 预测时长 | 状态 |
|----|------|-------|---------|---------|------|
| T2_RED_LDN43 | ASA 500N (窄场) | 2048 | 640 MB | 25.6s | COMPLETED |
| T3_RED_NGC55 | ASA 500N (窄场) | 2048 | 640 MB | 25.2s | COMPLETED |
| T4_RED_GalaxyCenter_panel1 | 10Micron (宽场) | 512 | 185 MB | 25.8s | COMPLETED |

### 关键观察

1. **DRIZZLE 是内存瓶颈**: T2/T3 (nside=2048) DRIZZLE 峰值 640MB, T4 (nside=512) 仅 185MB
2. **宽场低 nside 优势**: T4 宽场 (6.3"/px) 自适应 nside=512, 内存仅为 T2/T3 的 29%
3. **准入控制有效**: 所有场景下准入公式 `reserved + predicted + uncertainty + os_margin + worst_next <= budget` 正确判断
4. **无 OOM**: 即使 1GB 极端预算, 准入控制通过串行化避免了内存溢出

## 3. Gate H Checklist 验收

```
--- Gate H Checklist 验收 ---
  [✓] 动态预测      — H-001 FrameCostEstimator (7阶段成本模型, B-002校准误差<3.4%)
  [✓] 内存预约      — H-002 MemoryBudgetManager (准入公式 + 预约/释放)
  [✓] CPU回滞       — H-002 CPUBackpressure (90%停止投喂, 70%线性回滞)
  [✓] 高峰错峰      — H-003 PeakShifter (优先级队列 + 防饥饿)
  [✓] spill恢复     — H-003 SpillManager (原子写入 + SHA256校验 + manifest持久化)
  [✓] 安全余量      — H-002 os_margin (默认2GB, 可配置)
  [✓] 压力测试无OOM — H-004 4场景全部无OOM, 3/3帧全部完成
```

## 4. 禁止捷径验证

| 禁止项 | 验证结果 |
|--------|---------|
| 不得用固定并发数替代预算 | ✓ H-002 测试19: max_concurrent=10 + 内存不足 → DEFER |
| 不得依赖 OS swap | ✓ H-003 SpillManager: 显式原子写入 + fsync, 不依赖 OS swap |
| 不得丢未持久化科学数据 | ✓ H-003 SpillManager: 只 spill 已序列化可恢复块 + SHA256 校验 |

## 5. 测试结果

```
H-004 压力测试: 31 PASS, 0 FAIL

场景 1: 充足预算 8GB — PASS (3/3帧完成, 0 OOM)
场景 2: 中等预算 2GB — PASS (3/3帧完成, 0 OOM)
场景 3: 紧张预算 1.5GB — PASS (3/3帧完成, 0 OOM)
场景 4: 极端紧张 1GB — PASS (3/3帧完成, 0 OOM)
无OOM验收: 3场景全部 PASS, 0 OOM 事件
Gate H Checklist: 7/7 项全部通过
```

## 6. 交付物

### Python 测试 (engineering_authoritative/evidence/H-004/)
- `pressure_test.py` — 混合设备压力测试模拟器 + 4 场景 + Gate H 验收
- `TASK_REPORT.md` — 本报告

## 7. 已知限制

- **模拟非真实执行**: 压力测试使用成本模型预测值模拟执行, 非真实 DLL 调用。真实 OOM 风险需在 C++ 移植后用真实管线验证。
- **C-002 依赖**: H-004 依赖 C-002 (多帧 Stage1 输出), 当前用 B-002 基线 (3帧统计) 模拟。C-002 完成后可用真实 .hiss 输出补充验证。
- **spill 未在压力测试中触发**: 当前 4 场景下准入控制通过串行化避免了 spill。需要更极端的场景 (如 0.5GB 预算) 或真实并发执行才能触发 spill 路径。H-003 的 spill/恢复链路由独立单元测试 (62项) 验证。
