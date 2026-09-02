# P13-002 栈溢出失败帧分析（待后续任务修复）

## 症状
- exit_code=3221225725 (0xC00000FD = STATUS_STACK_OVERFLOW)
- 崩溃阶段：DRIZZLE 的 hp_drizzle_run 内，snr_model 块加载后
- 崩溃点：`[snr_evaluator] build` 阶段（IDW 插值），日志在 `snr_model 块加载` 行后截断，无 `build 成功` 行
- 崩溃耗时：23-26s（正常帧 DRIZZLE 约 15-24s）

## 失败帧清单（运行中持续更新）
| 序号 | frame_id | 滤镜 | 数据集 | 崩溃时间 |
|------|----------|------|--------|----------|
| 48 | T4_Victory_Nebula_RED_..._20250205@045711-180S-Red | RED | Victory_Nebula | 13:24 |
| 84 | T4_Victory_Nebula_BLUE_..._20250206@060603-180S-Blue | BLUE | Victory_Nebula | (待补) |
| 114 | T4_Victory_Nebula_BLUE_..._20250207@071753-180S-Blue | BLUE | Victory_Nebula | (待补) |

## 模式分析
- 全部为 Victory_Nebula T4 数据集
- 全部为 RED/BLUE 滤镜（非 LUM/GREEN）
- n_points=1984（与其他帧一致），snr_phot=3.39（RED 偏低，LUM 约 4.76）
- median_snr=571（RED 偏低，LUM 约 723）
- 可能原因：某些控制点空间分布导致 IDW 递归过深或栈分配过大

## 后续修复方向（非当前任务）
1. 分析 snr_evaluator 的 IDW 实现是否存在递归过深
2. 检查控制点空间聚类是否导致 KD-tree 不平衡
3. 考虑增加栈大小或改用迭代实现
4. 单独建立修复任务（如 P13-003）处理
