# P03-001 证据索引

- 任务编号: P03-001
- 证据目录: `engineering/evidence/P03-001/`
- 生成日期: 2026-07-25

## 1. 报告与契约文件

| 文件 | 说明 |
|---|---|
| `TASK_REPORT.md` | 任务报告 (目标/数据流/变更/验收) |
| `TEST_REPORT.md` | 测试报告 (6 测试矩阵 + 详情) |
| `EVIDENCE_INDEX.md` | 本索引 |
| `REVIEW_REPORT.md` | 独立复核报告 (含 VERDICT) |
| `calibration_wiring.json` | 配置与数据流契约 (config_schema + cal_stats_kv_schema) |

## 2. 测试配置文件

| 文件 | 测试场景 | 类型 |
|---|---|---|
| `config_red_T4.json` | Red 帧完整校准 (Bias+Dark+Flat 全有, T4 4500x3600, 180s) | 正面 |
| `config_lum_T2_noFlat.json` | Lum 帧无 Flat 降级 (T2 4096x4096, 600s) | 正面 |
| `config_negative1_empty_master.json` | 全 Master 缺失 + allow_no_calibration=false | 负面 |
| `config_negative2_size_mismatch.json` | Bias 尺寸 4096x4096 vs Light 4500x3600 | 负面 |
| `config_negative3_exposure_mismatch.json` | Dark 300s vs Light 180s (tol=0.5s) | 负面 |
| `config_negative4_file_not_found.json` | Bias 文件不存在, Dark+Flat 推导成功 | 正面 (降级) |

## 3. 运行日志文件

| 文件 | 对应测试 | 结果 | 关键指标 |
|---|---|---|---|
| `red_T4_run.log` | red_T4 | success=true | CALIBRATE 0.955s, out_mean=3879.63, CALIBRATION_STATUS=APPLIED |
| `lum_T2_noFlat_run.log` | lum_T2_noFlat | success=true | CALIBRATE 0.899s, HAS_FLAT=0, flat_mean=0.0 |
| `neg1_empty_master_run.log` | neg1_empty_master | success=false | 0.0006s 快速失败, "无 Master 文件且未启用 allow_no_calibration" |
| `neg2_size_mismatch_run.log` | neg2_size_mismatch | success=false | 0.175s, "master_bias 尺寸不匹配: 4096x4096 vs 4500x3600" |
| `neg3_exposure_mismatch_run.log` | neg3_exposure_mismatch | success=false | 0.303s, "master_dark 曝光时间不匹配: 300s vs 180s" |
| `neg4_file_not_found_run.log` | neg4_file_not_found | success=true | CALIBRATE 0.434s, HAS_BIAS=0, bias_mean=0.0 |

## 4. 代码变更

| 文件 | 变更类型 | 说明 |
|---|---|---|
| `lib/orchestrator/configs/stage1_config.json` | 修改 | calibration_dir 改为 T4 实际目录; 新增 calibration 段 (11 字段) |
| `lib/orchestrator/cpp/src/orchestrator.cpp` | 修改 | 重写 run_stage_calibrate; 新增辅助函数 (路径推导/验证/统计) |
| `engineering/control/MASTER_TASK_REGISTER.csv` | 修改 | P02-006/P02-007 -> DONE, P03-001 -> IN_PROGRESS |

## 5. 复现命令

所有测试在项目根目录 `f:\Astro dev\Astro CS Normalization Database` 下执行：

```powershell
# 正面测试: Red 帧完整校准
.\lib\orchestrator\cpp\orchestrator.exe stage1 `
  --frame "testdata\Galaxy_Center_T4\lights\panel1\Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@061703-180S-Red.fts" `
  --output "engineering\evidence\P03-001\red_T4_calibrated.hiss" `
  --config "engineering\evidence\P03-001\config_red_T4.json" `
  2>&1 | Tee-Object engineering\evidence\P03-001\red_T4_run.log

# 负面测试: 全 Master 缺失
.\lib\orchestrator\cpp\orchestrator.exe stage1 `
  --frame "testdata\Galaxy_Center_T4\lights\panel1\Galaxy_Center_mosaic1_T4_flying_dutchman-20250702@061703-180S-Red.fts" `
  --output "engineering\evidence\P03-001\neg1_output.hiss" `
  --config "engineering\evidence\P03-001\config_negative1_empty_master.json" `
  2>&1 | Tee-Object engineering\evidence\P03-001\neg1_empty_master_run.log
```

其余 4 个测试同结构, 替换 --frame/--config/--output/--log 即可。

## 6. 构建产物

- `lib/calibration/astro_calibration.dll` (校准算法 DLL, 含 ac_calibrate_frame 入口)
- `lib/orchestrator/cpp/orchestrator.exe` (CLI 编排器, 含 run_stage_calibrate)

构建命令:
```powershell
# 构建 calibration DLL
powershell -ExecutionPolicy Bypass -File lib\calibration\build.ps1
# 构建 orchestrator
cd lib\orchestrator\cpp; make; cd ..\..\..
```

## 7. 临时文件清理

测试过程中生成的临时 .hiss 输出文件 (red_T4_calibrated.hiss / lum_T2_noFlat_calibrated.hiss / neg4_file_not_found_output.hiss) 已清理, 不纳入证据。
临时编译产物 (stderr.txt / stdout.txt / test_simple.cpp / test_simple.exe) 已清理。
testdata/_P03_001_tmp/ 目录 (中文路径规避用) 已清理。
