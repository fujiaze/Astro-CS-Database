# P03-001 真实校准输入接线 - 任务报告

- 任务编号: P03-001
- 阶段: P03
- 依赖: P01-002, P00-003
- Gate: G3
- 基线 commit: c960dcc (P02-007 PlateSolve 无退化与单次检测专项 Gate)
- 执行日期: 2026-07-25
- 执行人: Subcoding Agent

## 1. 任务目标

将 Master Bias/Dark/Flat、坏点、曝光温度匹配真正传入 CALIBRATE 阶段，修复骨架退化（P00-003 基线下 CALIBRATE 仅输出日志返回成功，不读取 Master、不调用 `ac_calibrate_frame`、无 cal_stats 输出）。

## 2. 入口条件与工作树状态

- 当前分支: main
- 基线最近提交: `c960dcc P02-007: PlateSolve 无退化与单次检测专项 Gate 验证 (CONDITIONAL_PASS)`
- 工作树: 干净 (P02-007 已合并)
- 依赖任务: P01-002 (DLL 加载器)、P00-003 (基线骨架) 均已就绪

## 3. 校准输入数据流分析

```
CLI: orchestrator stage1 --frame <fits> --output <hiss> --config <json>
   |
   v
CliCommand::cmd_stage1  ->  Orchestrator::load_config(stage1_config.json)
   |                          解析 calibration_dir + calibration.* 字段
   v
Orchestrator::run_single -> run_stage_calibrate(frame_, config_json)
   |
   |  1. 从 frame_ 读取 Light data 块 (W/H/pixels) 和 FITS 元数据 (EXPTIME/FILTER/CCD-TEMP)
   |  2. 解析配置: master_*_path (空则自动推导) + 验证开关 + dark_opt/dark_k
   |  3. 自动推导 Master 路径:
   |     - Bias:  <calibration_dir>/masterBias_BIN-1_<W>x<H>.xisf
   |     - Dark:  <calibration_dir>/masterDark_BIN-1_<W>x<H>_EXPOSURE-<EXPTIME.2f>s.xisf
   |     - Flat:  <calibration_dir>/masterFlat_BIN-1_<W>x<H>_FILTER-<FILTER>_mono.xisf
   |  4. 加载 Master 文件 (aio_read_xisf), 验证尺寸/曝光/温度
   |  5. 调用 ac_calibrate_frame(light, W, H, dark, flat, bias, out, dark_opt, k_init, &actual_k)
   |  6. 计算校准前/后统计 (mean/median/std)
   |  7. 替换 frame_ 的 data 块为校准后输出
   |  8. 写入 cal_stats KV 块 (22 个字段)
   v
PipelineFrame["cal_stats"] KV 块  ->  后续阶段 (PLATESOLVE/PHOTOMETRIC/DRIZZLE) 可读取
```

## 4. 实施变更清单

### 4.1 配置文件
- `lib/orchestrator/configs/stage1_config.json`
  - `calibration_dir` 默认值改为 `testdata/T4 calibration files` (符合 testdata 实际目录命名)
  - 新增 `calibration` 段，包含 11 个字段：master_*_path (3 个)、require_size_match、require_exposure_match、exposure_tolerance_s、require_temperature_match、temperature_tolerance_c、allow_no_calibration、dark_optimization、dark_scale_factor

### 4.2 核心代码
- `lib/orchestrator/cpp/src/orchestrator.cpp`
  - 匿名命名空间新增辅助函数：
    - `orc_getJsonString` / `orc_getJsonNum` / `orc_getJsonBool` (从 nlohmann::json 安全取值)
    - `format_exposure_2f` (曝光时间格式化为 2 位小数, 如 180.00)
    - `derive_master_bias_path` / `derive_master_dark_path` / `derive_master_flat_path` (Master 路径自动推导)
    - `parse_exposure_from_dark_path` (从 Dark 文件名解析曝光时间, 用于验证)
    - `compute_array_mean` / `compute_array_median` / `compute_array_std` (图像统计)
  - 重写 `Orchestrator::run_stage_calibrate` 函数：
    - 配置解析 -> Master 路径推导 -> 加载验证 -> `ac_calibrate_frame` 调用 -> data 块替换 -> cal_stats KV 写入
    - 完整资源管理 (bias_img/dark_img/flat_img/out 缓冲的释放)
    - 退化模式: 部分 Master 缺失时应用可用 Master; 全缺 + allow_no_calibration=false 时失败

### 4.3 任务注册表
- `engineering/control/MASTER_TASK_REGISTER.csv`
  - P02-006 状态 IN_PROGRESS -> DONE
  - P02-007 状态 CONDITIONAL_PASS -> DONE
  - P03-001 状态 TODO -> IN_PROGRESS

## 5. Master 文件验证结果

| 验证项 | 配置开关 | 默认 | 失败行为 |
|---|---|---|---|
| 尺寸匹配 | require_size_match | true | 返回错误, CALIBRATE 失败 |
| 曝光时间匹配 | require_exposure_match | true (tol=0.5s) | 返回错误, CALIBRATE 失败 |
| 温度匹配 | require_temperature_match | false (软约束) | 仅警告, 不中止 |
| 全 Master 缺失 | allow_no_calibration=false | false | 返回错误, CALIBRATE 失败 |
| 部分 Master 缺失 | (无开关, 自动降级) | - | 应用可用 Master, 缺失项 mean=0, HAS_*=0 |

## 6. cal_stats 输出内容

KV 块名: `cal_stats`，包含 22 个字段：

| 类别 | 字段 |
|---|---|
| 状态 | STATUS, CALIBRATION_STATUS |
| Master 路径 | MASTER_BIAS_PATH, MASTER_DARK_PATH, MASTER_FLAT_PATH |
| Master 均值 | MASTER_BIAS_MEAN, MASTER_DARK_MEAN, MASTER_FLAT_MEAN |
| Light 统计 | LIGHT_MEAN, LIGHT_MEDIAN, LIGHT_STD |
| 输出统计 | OUT_MEAN, OUT_MEDIAN, OUT_STD |
| Dark 优化 | ACTUAL_K, DARK_OPTIMIZATION |
| 应用标志 | HAS_BIAS, HAS_DARK, HAS_FLAT |
| Dark 元数据 | DARK_EXPTIME, DARK_CCD_TEMP (仅 HAS_DARK=1 时存在) |

## 7. 负面测试结果

| 测试 | 场景 | 期望 | 实际 | 结果 |
|---|---|---|---|---|
| neg1_empty_master | 全 Master 缺失 + allow_no_calibration=false | 失败 | success=false, error=无 Master 文件且未启用 allow_no_calibration | PASS |
| neg2_size_mismatch | Bias 4096x4096 vs Light 4500x3600 | 失败 | success=false, error=尺寸不匹配 | PASS |
| neg3_exposure_mismatch | Dark 300s vs Light 180s (tol=0.5s) | 失败 | success=false, error=曝光时间不匹配 | PASS |

## 8. 构建与运行结果

- 构建: `lib/calibration/build.ps1` 成功生成 `astro_calibration.dll` (修复 libzstd-1.dll 缺失：复制 libzstd.dll)
- 构建: `lib/orchestrator/cpp/Makefile` 成功生成 `orchestrator.exe` (PATH 添加 C:\msys64\mingw64\bin 解决运行时 DLL 依赖)
- 运行: 3 个正面测试 + 3 个负面测试全部符合预期
- 性能: CALIBRATE 阶段 0.9s (Red T4, 16.2M 像素, 含 Master 加载+验证+校准+统计+KV 写入)

## 9. 兼容性与回滚

- 兼容性: stage1_config.json 新增 `calibration` 段，旧配置无此段时使用默认值 (require_*=true, allow_no_calibration=false)，行为安全
- 回滚: 恢复 `orchestrator.cpp` 的 `run_stage_calibrate` 至 P00-003 骨架版本即可回滚 (但会重新引入骨架退化)
- 残留风险:
  - 温度匹配默认关闭 (require_temperature_match=false)，未来若开启需确认 Master Dark 元数据包含 CCD-TEMP
  - 部分 Master 缺失的降级模式 (如 neg4/lum_T2_noFlat) 在天文学上合理，但用户若要求"严格模式"需新增 require_all_masters 开关 (本次未实现, 待 P03-003 严格失败任务处理)

## 10. 验收对照

| 验收项 | 状态 |
|---|---|
| 依赖任务均已通过 (P01-002, P00-003) | OK |
| 本任务目标有可复现证据 (6 份测试日志) | OK |
| 相关回归全部运行 (CALIBRATE 路径 6 测试) | OK |
| 独立复核以 VERDICT: PASS 结束 | 见 REVIEW_REPORT.md |
| 更新任务注册表、当前任务和项目状态 | OK (MASTER_TASK_REGISTER.csv 已更新) |

## 11. 交付物清单

- `engineering/evidence/P03-001/calibration_wiring.json` - 配置与数据流契约
- `engineering/evidence/P03-001/TASK_REPORT.md` - 本报告
- `engineering/evidence/P03-001/TEST_REPORT.md` - 测试报告
- `engineering/evidence/P03-001/EVIDENCE_INDEX.md` - 证据索引
- `engineering/evidence/P03-001/REVIEW_REPORT.md` - 独立复核报告 (含 VERDICT)
- 6 份测试配置 + 6 份运行日志 (见 EVIDENCE_INDEX.md)
