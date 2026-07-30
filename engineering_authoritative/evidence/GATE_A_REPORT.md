# Gate A 合并验收报告

**生成时间**: 2026-07-30
**Gate**: A — 数据与校准整理
**状态**: PASSED

## 验收清单

| # | 验收项 | 状态 | 证据 |
|---|--------|------|------|
| 1 | 根README已安装并备份旧README | ✅ | A-001: 旧README备份到 docs/archive/README_pre_authoritative_20260730T024407Z.md |
| 2 | T1–T4均有设备档案 | ✅ | A-002: T2/T3/T4 设备档案完整（T1无数据集，无档案） |
| 3 | 说明文档与Header已核对 | ✅ | A-002: 7个数据集说明文档已读取，FITS/XISF Header 已验证 |
| 4 | 所有Light有唯一Bias/Dark/Flat解析或明确冲突 | ✅ | A-004: 35个滤镜组合解析，32 RESOLVED + 3 UNRESOLVED（明确报告） |
| 5 | 不同滤镜规范名已统一 | ✅ | A-003: 6个规范滤镜 + 别名映射（Lum/Red/Green/Blue/H-alpha/OIII） |

## 任务完成情况

| 任务 | 标题 | 状态 |
|------|------|------|
| A-001 | 安装权威README并生成迁移报告 | DONE |
| A-002 | 整理T1-T4设备与说明文档目录 | DONE |
| A-003 | 建立滤镜别名与主校准帧清单 | DONE |
| A-004 | 实现Light到Bias/Dark/Flat唯一解析与严格模式 | DONE |

## 关键产物

### 设备档案 (TESTDATA_EQUIPMENT_CATALOG)
| 设备 | 望远镜 | 口径 | 相机 | 传感器 | 滤镜集 |
|------|--------|------|------|--------|--------|
| T2 | ASA 500N | 500mm | FLI Proline 16803 | 4096×4096 | LRGBHaOIII |
| T3 | ASA 500N | 500mm | FLI Proline 16803 | 4096×4096 | LRGBHaOIII |
| T4 | Nikkor 200F2 | 100mm | FLI 16200 | 4500×3600 | RGBHaOIII/LRGB |

### 滤镜别名 (FILTER_ALIAS_MAP)
- Lum ← L/Lum/Luminance
- Red ← R/Red
- Green ← G/Green
- Blue ← B/Blue
- H-alpha ← Ha/Halpha/H-alpha/Hα
- OIII ← OIII/Oiii/O3

### Master 清单 (CALIBRATION_MASTER_INVENTORY)
- 27 个 Master 帧：3 Bias + 8 Dark + 16 Flat
- T2: 9 Master（1B+3D+5F，无 Lum Flat）
- T3: 9 Master（1B+2D+6F，含 Lum Flat）
- T4: 9 Master（1B+3D+5F，无 Lum Flat）

### Light→Master 解析 (LIGHT_TO_MASTER_RESOLUTION)
- 35 个滤镜组合：32 RESOLVED + 3 UNRESOLVED
- 严格模式：不静默降级，不跨滤镜替代

## 未解决问题（明确记录，不阻塞 Gate A）

1. **T2 Lum Flat 缺失**：影响 LDN43_T2 和 NGC247_T2 的 Lum 通道（2个数据集）
2. **T4 Lum Flat 缺失**：影响 Victory_T4 的 Lum 通道（1个数据集）
3. **NGC55_T3 Lum 焦距异常**：1934.7mm vs 其他滤镜 1877mm（需在板解算中关注）
4. **Galaxy_Center_T4 相机型号**：文档不一致（Microline vs Proline），以 Header INSTRUME=FLI 为准

## 结论

Gate A 全部验收项通过。T2/T4 Lum Flat 缺失已明确报告为 UNRESOLVED，不影响 Gate A 通过（README 5.3 节"解析不到 Master 时优先视为映射问题"已覆盖）。后续 Gate B 选择代表帧时将避开 UNRESOLVED 的 Lum 帧。
