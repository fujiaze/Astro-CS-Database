# G07 报告 — T1 搜索 + T4 post-fix crop + 唯一一次完整 FP32 (Phase1 Final Signoff V4)

- 日期: 2026-08-09
- 分支: main (HEAD 见 git log)

## T1 真实数据搜索

全盘递归搜索 `testdata/`、`run/`、`工程控制/evidence/` 及相关素材说明文档。

| 来源 | 结论 |
| --- | --- |
| testdata/index.json | T1: status=not_available, datasets=[], calibration_masters_dir=null |
| P10-002 T1_DEVICE_PROFILE.json | status=no_data, 设备字段全空, light_dirs=[], master_dir='' |
| P11-003 REPRESENTATIVE_FRAMES_ARCHIVE.json | T1: no_data, "未提供数据" |
| testdata 物理扫描 (710 帧 .fit/.fits/.fts) | T1=0 帧 (T2=174, T3=151, T4=385) |
| run/ 递归 | 无 T1 望远镜数据; 同名文件均为 ACR 构建依赖/调试残留 |

结论: `BLOCKED_EXTERNAL_DATA: T1 representative dataset unavailable` (如实记录, 不伪造、不用 T2 冒充)。
完整清单: `工程控制/evidence/P13-003/t1_data_inventory.json`。

## T4 post-fix 1024² crop (FP32/FP64, 当前 HEAD 构建)

配置: `run/temp/v4_final_crop/t4_crop_fp{32,64}.json`, NSIDE=65536 显式, pixfrac=0.8, stop_after=hips_verify。

| 检查 | FP32 | FP64 |
| --- | --- | --- |
| HIPS_VERIFY | 通过 tiles=24 order=7 snr_points=997 dtype=float32 | 通过 tiles=24 order=7 snr_points=997 dtype=float64 |
| Browser headless 2048 查询 | mismatch=0 outside_ok=64/64 snr_rows=997 id_unique=1 | 同左 |
| SNR 空间 Oracle (逐行 RA/Dec) | 997 行 wrong=0 dup=0 tiles 24/24 | 同左 |
| VOTable metadata.xml | <VOTABLE> 根 + 推荐元数据键齐 | 同左 |

## G4 actual-buffer trace (最终 crop 上重新生成)

- trace_selection: seed=7982490244666596157, 1024 像素 (与 drizzle trace 同源)
- pixel 链 1024/1024 (READ_FITS→CALIBRATE→PHOTOMETRIC)
- drizzle 逐源贡献 1024/1024, sum_contribution 守恒 1024/1024, effective==photometric 1024/1024
- leaf 内部 20000 样本 readback 通量/面积相对误差 ≤1e-5: bad=0
- SNR id 997 ⊆ PSF 1111, 无 i+1 重编号
- Browser 同样本 1024 点 AIO vs BrowserBackend mismatch=0
- 结果: trace_g4_result.json PASS=true

## 最终唯一一次 post-fix T4 完整 FP32

授权: 所有前置 Gate (G1-G6 + crop) 通过后仅 1 次。

- 输入: `testdata/Galaxy_Center_T4/lights/panel3/Galaxy_Center_mosaic3_T4_flying_dutchman-20250718@001638-180S-Red.fts` (4500x3600)
- 管线: JSON→READ→CAL→PSF→PLATE→PHOTO→SNR→NSIDE→DRIZZLE→HiPS→HIPS_VERIFY→Browser core verify
- 结果: exit_code=0; HIPS_VERIFY tiles=285 order=7 snr_points=1947 dtype=float32 PASS
- Browser core verify: 4096 查询 mismatch=0 outside_ok=64/64 snr_rows=1947 id_unique=1 PASS
- SNR 空间 Oracle: 1947 行 wrong=0 dup=0 tiles 253/253 PASS

### 耗时 (单次, 16 线程)

| 阶段 | 秒 |
| --- | --: |
| READ_FITS | 0.082 |
| CALIBRATE | 0.586 |
| PSF | 1.836 |
| PLATESOLVE | 2.558 |
| PHOTOMETRIC | 0.609 |
| SNR | 0.001 |
| NSIDE | 0.0001 |
| DRIZZLE (PRECISE→HiPS 直写) | 49.705 |
| HIPS_VERIFY | 0.481 |
| 总墙钟 | 58.974 |

完整 FP64 post-fix: 0 次 (未运行, 符合限制)。

## 证据文件

- `crop_final/`: 双精度 crop 日志/配置、trace 全链、browser/snr oracle 输出、product_manifest.json
- `full_final/`: 最终完整 FP32 日志/配置/时序/browser/snr oracle、product_manifest.json (含大 FITS path/size/SHA256)
- `t1_data_inventory.json`: T1 搜索证据
