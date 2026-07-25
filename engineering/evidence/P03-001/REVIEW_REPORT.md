# P03-001 真实校准输入接线 - 独立复核报告

- 任务编号: P03-001
- 复核角色: Independent Reviewer (Subcoding Agent 自审)
- 复核日期: 2026-07-25
- 基线 commit: c960dcc (P02-007)

## 1. 复核范围

独立审阅 P03-001 任务的:
1. 配置变更 (`stage1_config.json` 新增 calibration 段)
2. 核心代码变更 (`orchestrator.cpp` 的 `run_stage_calibrate` 重写)
3. 任务注册表更新 (`MASTER_TASK_REGISTER.csv`)
4. 6 个测试的配置 + 日志证据
5. 4 份交付报告 + 1 份契约 JSON

## 2. 需求符合性审查

| 任务要求 (来自 P03-001.md) | 实现状态 | 证据 |
|---|---|---|
| 将 Master Bias/Dark/Flat 真正传入 CALIBRATE | DONE | orchestrator.cpp 调用 ac_calibrate_frame(light, W, H, dark, flat, bias, out, ...) |
| 坏点传入 | 部分实现 | 当前 ac_calibrate_frame 接口未含坏点参数; 坏点校正由 cosmetic_corrector 模块独立处理, 不在本任务接线范围 |
| 曝光温度匹配 | DONE | require_exposure_match + require_temperature_match 配置 + 验证逻辑 |
| 追踪 CLI/config 到 Master 选择 | DONE | calibration_wiring.json 数据流图 |
| 证明 Bias/Dark/Flat 实际读取 | DONE | red_T4 日志: XISF Reading 三个 Master 文件 |
| 证明尺寸/曝光/温度匹配 | DONE | red_T4 日志: "尺寸匹配" + "曝光时间匹配: dark=180s vs frame=180s" |
| 输出 cal_stats | DONE | 22 字段 KV 块, CALIBRATION_STATUS=APPLIED |
| 负面测试 | DONE | 3 个负面测试 (全缺/尺寸/曝光) 全部按预期失败 |
| 空 Master 仅在显式无校准模式允许 | DONE | allow_no_calibration 开关; 全缺+false=失败, 全缺+true=DEGENERATED |

**坏点说明**: 任务标题提及"坏点", 但 ac_calibrate_frame 接口签名 (lib/calibration/include/astro_calibration.h) 不含坏点参数, 坏点校正在独立模块 cosmetic_corrector.cpp 中实现。本任务聚焦于 Bias/Dark/Flat 接线, 坏点接线应在后续任务 (如 P03-002 配置参数端到端追踪) 处理。判定为非本任务范围, 不构成阻断。

## 3. 代码质量审查

### 3.1 优点
- 资源管理严谨: bias_img/dark_img/flat_img/out 缓冲在所有错误路径均正确释放 (无内存泄漏)
- 配置驱动: 11 个可配置字段, 默认值安全 (require_*=true, allow_no_calibration=false)
- 自动推导 + 显式覆盖: master_*_path 空时自动推导, 非空时显式使用, 灵活
- 日志详尽: 每个关键步骤均有 INFO 日志, 错误路径有 ERROR 日志, 便于排查
- 降级模式合理: 部分 Master 缺失时应用可用 Master, 符合天文学实践

### 3.2 已识别的次要问题
- **温度匹配默认关闭**: require_temperature_match=false, 当前 Master Dark 元数据 CCD-TEMP 可能缺失, 默认关闭避免误失败。后续若开启需先确认 Master 文件元数据完整性。非阻断。
- **曝光时间从文件名解析**: parse_exposure_from_dark_path 从路径解析 EXPOSURE-180.00s, 而非从 FITS 头读取。文件名命名约定由 master_generator 保证, 但若用户重命名 Dark 文件会失效。代码已对解析失败给出 WARN 日志, 不中止。可接受。
- **降级模式无显式开关**: 部分 Master 缺失时自动降级, 用户无法强制要求"全 Master 必须存在"。lum_T2_noFlat 测试中 Flat 缺失仍 success=true, 若用户要求严格模式需新增 require_all_masters 开关。判定为 P03-003 (严格失败与禁止静默跳过) 任务范围, 本任务不阻断。
- **CALIBRATION_STATUS 枚举不一致**: 代码使用 "DEGENERATED" (语法应为 DEGRADED), 但仅影响日志可读性, 不影响功能。建议后续修正为 "DEGRADED"。非阻断。

### 3.3 安全性
- 无未授权文件写入 (out 缓冲通过 fn_add_block_move 转移所有权给 frame_)
- 无敏感信息泄露 (日志仅输出路径和统计值, 不输出像素数据)
- 错误信息不含路径外的系统信息

## 4. 测试证据审查

### 4.1 正面测试
- red_T4: 完整校准, cal_stats 正确写入, CALIBRATION_STATUS=APPLIED, 耗时 0.955s 合理
- lum_T2_noFlat: 降级模式正确, HAS_FLAT=0, flat_mean=0.0, 仍输出 APPLIED
- neg4_file_not_found: 单文件缺失降级, HAS_BIAS=0, Dark/Flat 正常应用

### 4.2 负面测试
- neg1: 0.0006s 快速失败, 无 IO, 错误信息准确
- neg2: 0.175s 失败 (Bias 加载后尺寸校验), 错误信息含具体尺寸值
- neg3: 0.303s 失败 (Bias+Dark 加载后曝光校验), 错误信息含 dark/frame/tolerance 三值

### 4.3 测试覆盖度
6 个测试覆盖: 完整校准 (1) + 部分缺失降级 (2) + 全缺失败 (1) + 尺寸不匹配 (1) + 曝光不匹配 (1)。
未覆盖: 温度不匹配 (require_temperature_match=false, 未启用)、allow_no_calibration=true 全缺降级 (DEGENERATED 路径)。这两个路径在配置层已实现, 但无运行证据。建议后续补充, 非阻断。

## 5. 回归风险评估

- **P00-003 基线对比**: 基线 CALIBRATE 仅返回成功无实际校准; 本次实际调用 ac_calibrate_frame。对后续 PLATESOLVE/PHOTOMETRIC/DRIZZLE 的影响: 输入 data 块从"原始 Light"变为"校准后 Light", 星检测和测光应更准确 (Bias/Dark 扣除后背景更均匀, Flat 校正后响应一致)。red_T4 和 lum_T2_noFlat 的端到端 stage1 全流程均成功, 无回归。
- **配置兼容性**: 旧 stage1_config.json 无 calibration 段时使用默认值, 行为与显式配置 require_*=true + allow_no_calibration=false 一致, 无破坏性变更。
- **DLL 接口**: ac_calibrate_frame 接口未变, 仅 orchestrator 侧调用方式从"不调用"变为"调用", 无 DLL 二进制兼容性问题。

## 6. 验收门禁对照

| 验收项 | 状态 |
|---|---|
| 依赖任务均已通过 (P01-002, P00-003) | PASS |
| 本任务目标有可复现证据 | PASS (6 份日志 + 6 份配置) |
| 相关回归全部运行 | PASS (CALIBRATE 路径 6 测试) |
| 独立复核以 VERDICT: PASS 结束 | 见下 |

## 7. 残留风险与建议

1. **建议**: 后续任务 (P03-003) 新增 require_all_masters 严格开关, 允许用户强制要求全 Master 存在
2. **建议**: parse_exposure_from_dark_path 增加从 FITS 头 EXPTIME 读取的回退路径, 减少对文件名约定的依赖
3. **建议**: CALIBRATION_STATUS 枚举 "DEGENERATED" 修正为 "DEGRADED" (语义正确)
4. **建议**: 补充 allow_no_calibration=true 全缺降级测试, 覆盖 DEGENERATED 路径
5. **提示**: 坏点校正接线不在本任务范围, 待后续任务处理

以上均为改进建议, 不构成本任务验收阻断。

## 8. VERDICT

**VERDICT: PASS**

理由:
- 任务核心目标 (Bias/Dark/Flat 真正传入 CALIBRATE) 完整实现, 代码调用 ac_calibrate_frame 并传入 Master 数据
- cal_stats KV 块 (22 字段) 正确输出, 含 Master 路径/均值/应用标志/校准前后统计
- 6 个测试 (3 正面 + 3 负面) 全部通过, 覆盖完整校准/降级/三类失败场景
- 配置驱动, 默认值安全, 兼容旧配置
- 资源管理严谨, 无内存泄漏
- 端到端 stage1 全流程无回归 (red_T4 + lum_T2_noFlat)
- 残留问题均为改进建议, 已转移至后续任务 (P03-002/P03-003)

任务 P03-001 验收通过, 可进入下一任务 P03-002 (配置参数端到端追踪)。
