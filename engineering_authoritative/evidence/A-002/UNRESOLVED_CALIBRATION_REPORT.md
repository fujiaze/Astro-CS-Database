# A-002 未解决校准报告

**生成时间**: 2026-07-30
**任务**: A-002 整理 T1-T4 设备与说明文档目录

## 1. 总览

| 指标 | 值 |
|------|-----|
| 数据集总数 | 7 |
| Light 滤镜组合总数 | 35 |
| 已解析 (RESOLVED) | 32 |
| 未解析 (UNRESOLVED) | 3 |
| 覆盖率 | 91.4% |

## 2. 未解决问题清单

### 2.1 缺失 Master Flat

以下数据集的 Light 帧缺少对应滤镜的 Master Flat:

| 数据集 | 设备 | 滤镜 | 曝光(s) | 缺失项 | Bias | Dark | 建议方案 |
|--------|------|------|---------|--------|------|------|----------|
| LDN43_T2 | T2 | Lum | 600 | Flat | T2/masterBias_BIN-1_4096x4096.xisf | T2/masterDark_BIN-1_4096x4096_EXPOSURE-600.00s.xisf | T2无Lum Flat. T3有Lum Flat(相同设备ASA 500N+FLI 16803, 4096x4096, 9um). 可考虑跨设备使用T3 Lum Flat, 但Flat包含光学illumination pattern, 需用户确认是否接受. 或补充拍摄T2 Lum Flat. |
| NGC247_T2 | T2 | Lum | 600 | Flat | T2/masterBias_BIN-1_4096x4096.xisf | T2/masterDark_BIN-1_4096x4096_EXPOSURE-600.00s.xisf | T2无Lum Flat. T3有Lum Flat(相同设备ASA 500N+FLI 16803, 4096x4096, 9um). 可考虑跨设备使用T3 Lum Flat, 但Flat包含光学illumination pattern, 需用户确认是否接受. 或补充拍摄T2 Lum Flat. |
| Victory_T4 | T4 | Lum | 180 | Flat | T4/masterBias_BIN-1_4500x3600.xisf | T4/masterDark_BIN-1_4500x3600_EXPOSURE-180.00s.xisf | T4无Lum Flat. T4是唯一设备(Nikkor 200F2+FLI 16200, 4500x3600), 无法跨设备替代. 可考虑用Red Flat近似(不推荐, 光谱响应不同), 或补充拍摄T4 Lum Flat. 需用户确认. |

### 2.2 文档与 Header 冲突

| 项目 | 说明 | 严重程度 |
|------|------|----------|
| T2 焦距不一致 | 文档1900mm, Light Header 1917.3-1917.8mm, Master Header 1877mm | 低 (对焦微调正常, 不影响校准) |
| T3 焦距不一致 | 文档1900mm, Light Header 1877.0-1934.7mm (NGC55_T3 Lum帧1934.7mm异常), Master Header 1877mm | 中 (NGC55_T3 Lum帧焦距异常, 需确认) |
| T4 相机型号不一致 | Galaxy_Center文档写FLI Microline 16200, Victory_Nebula文档写FLI Proline 16200 | 中 (需确认是否同一相机或文档笔误, Header INSTRUME均为FLI无法区分) |
| T3 无1800s Dark | T3 Master Dark仅有600s/1200s, 无1800s | 无 (T3数据集无1800s曝光Light帧, 不影响) |
| T4 无1200s/1800s Dark | T4 Master Dark仅有180s/300s/600s | 无 (T4数据集无1200s/1800s曝光Light帧, 不影响) |
| 滤镜名变体 | OIII(T2) vs Oiii(T3/T4); H-alpha vs Ha vs Halpha | 低 (已通过FILTER_ALIAS_MAP规范化) |
| Master XISF无温度字段 | T2/T3/T4 Master XISF Header中无CCD-TEMP/SET-TEMP | 低 (根据Light帧惯例推断-20°C, 所有Light帧SET-TEMP=-20) |
| Master XISF无Gain/Offset | T2/T3/T4 Master XISF Header中无GAIN/OFFSET | 低 (Light帧Header也无此字段, FLI相机可能在固件层固定) |

## 3. 建议处理优先级

1. **高优先级**: T2 Lum Flat 缺失 — 影响 LDN43_T2 和 NGC247_T2 共 2 个数据集的 Lum 通道校准
2. **高优先级**: T4 Lum Flat 缺失 — 影响 Victory_T4 (228帧, 最大数据集) 的 Lum 通道校准
3. **中优先级**: NGC55_T3 Lum 帧焦距异常 (1934.7mm vs 其他滤镜 1877mm) — 需确认是否影响板解算
4. **中优先级**: Galaxy_Center_T4 相机型号文档不一致 — 需确认 Microline vs Proline
5. **低优先级**: 滤镜名变体 — 已通过 FILTER_ALIAS_MAP.json 规范化, 无实际影响

## 4. 待用户确认事项

1. T2 Lum Flat: 是否接受使用 T3 Lum Flat (相同设备型号, 但不同望远镜实例) 作为替代?
2. T4 Lum Flat: 是否补充拍摄? 或接受使用 Red Flat 近似 (不推荐)?
3. Galaxy_Center_T4 相机: FLI Microline 16200 还是 FLI Proline 16200? (影响设备档案准确性)
4. NGC55_T3 Lum 帧焦距 1934.7mm: 是否为对焦调整? 需要在板解算中特殊处理吗?

## 5. 结论

根据 README 5.3 节, 用户已确认 TestData 配齐主校准帧。当前发现的 T2/T4 Lum Flat 缺失问题, 
可能是:(a) 别名问题 (Lum 在文件名中用了其他名称); (b) 文件扫描问题 (位于其他目录); 
(c) 需要跨设备匹配 (T2->T3); (d) 真正需要补充拍摄。

建议用户提供以下信息以完成最终解析:
- T2 是否有 Lum Flat 文件 (可能在其他目录或使用不同命名)?
- T4 是否有 Lum Flat 文件?
- 若无, 是否授权跨设备使用 (T2->T3) 或近似使用 (T4 Lum->Red)?