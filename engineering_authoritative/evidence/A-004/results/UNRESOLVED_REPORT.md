# A-004 Light 帧 -> Master 解析报告
**生成时间**: 2026-07-30T11:19:59.542181
**任务**: A-004 实现 Light 到 Bias/Dark/Flat 唯一解析与严格模式
**严格模式**: 默认开启 (不可静默降级)

## 1. 总览
| 指标 | 值 |
|------|-----|
| 解析帧总数 | 4 |
| 已解析 (RESOLVED) | 3 |
| 未解析 (UNRESOLVED) | 1 |
| 覆盖率 | 75.0% |

## 2. UNRESOLVED 帧清单

| Light 帧 | 设备 | 尺寸 | Bin | 曝光(s) | 滤镜 | 缺失项 | Bias状态 | Dark状态 | Flat状态 | 说明 |
|----------|------|------|-----|---------|------|--------|----------|----------|----------|------|
| LDN43_LRGBH_flying_dutchman-20250503@031525-600S-Lum.fts | T2 | 4096x4096 | 1x1 | 600.0 | Lum->Lum | Flat | exact | exact | not_found | 缺失: Flat |

## 3. UNRESOLVED 原因分析

### 3.1 缺失 Flat (1 帧)

- **LDN43_LRGBH_flying_dutchman-20250503@031525-600S-Lum.fts** (T2, Lum, 600.0s): 无 Flat 匹配 T2|4096x4096|1x1|Lum (同设备有 Flat 滤镜: ['Blue', 'Green', 'H-alpha', 'OIII', 'Red'])

## 4. 已知问题 (不修复, 仅记录)
- T2 Lum Flat 缺失: LDN43_T2 和 NGC247_T2 的 Lum 帧标记为 UNRESOLVED (T2 无 Lum Flat)
- T4 Lum Flat 缺失: Victory_T4 的 Lum 帧标记为 UNRESOLVED (T4 无 Lum Flat)
- 这些是真实缺失, 解析器明确报告, 不可静默降级 (如用 Red Flat 替代 Lum Flat)
