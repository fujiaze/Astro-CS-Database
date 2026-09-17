# 工程控制 / RELEASE-01 — DEL-001 成品帧交付说明

> 依据：tasks/DEL-001.md；ACCEPTANCE_SPEC.md L4。**只交付负责人检查，不推送发布**；发布决定只属负责人。

## 1. 交付物（两个 R 通道平面 FITS）

| 交付文件 | 来源 | 尺寸 | 大小 | sha256 |
|---|---|---|---|---|
| `run/RELEASE-01/deliverables/DEL-001/M42_R_plane.fits` | `run/RELEASE-01/e2e/l4/p3_m42_full/output_phase3.fits`（硬链接） | 7821 × 10947，2 HDU | 653 MiB | `42b13dd3cf86f76fa813cb0ffdc477e85d15e126d42869630b7a4fb5de1af2b2` |
| `run/RELEASE-01/deliverables/DEL-001/GalaxyCenter_R_plane.fits` | `run/RELEASE-01/e2e/l4/p3_gc_full/output_phase3.fits`（硬链接） | 4856 × 9160，2 HDU | 339 MiB | `7892b6fff8869499800a486cc49ba736d10001ab55b6fb6047e254fdf957c54b` |

- HDU 1：`signal`（BITPIX=-32，BUNIT=ADU，`RA---TAN`/`DEC--TAN`，含 CHECKSUM/DATASUM）；
- HDU 2：`EXTNAME='COVERAGE'` 覆盖掩膜（同尺寸，BITPIX=-32）；
- 覆盖外像素 = **NaN**（非 0），用 HDU 2 区分。

## 2. 数据来源与处理链

| 组 | 数据（testdata） | 帧数（R） | normalize | mosaic | export |
|---|---|---|---|---|---|
| M42 | `M42_T2T3_mosaic_Flying_dutchman`（T2 M1–M6 + T3 M1–M6，Red 300s） | 49 | 12 作业，rc=0 | rc=0（weight_mode=1 + 显式 legacy 等权降级） | rc=0 |
| Galaxy Center | `Galaxy_Center_T4/lights`（panel1–3，Red 180s） | 32 | 3 作业，rc=0 | rc=0（同上） | rc=0 |

- 校准：各自望远镜 masterBias/masterDark/masterFlat（.xisf，`master_units=normalized`，`master_scale=65535`，`master_flat_normalize=median`，`dark_optimization=true`）；
  M42 Red 为 300s，T2/T3 无 300s 暗场，按 `K=t_light/t_dark` 用 600s 暗场（K=0.5，`docs/science/CALIBRATION.md` §5/§6 明令两分支均须施加 K）；
  Galaxy Center Red 为 180s，T4 有 180s 暗场（K=1）。
- 坐标：`wcs.init_source=header_pointing` + `gaia_data_dir=GaiaDR3`（真实 IPV 解算）；
- 导出：TAN，bilinear，`longitude_parity=east_left`，`coverage_output=mask`；M42 尺度 2.673868e-4 °/px、GC 1.743156e-3 °/px（原生像元尺度，2% 余量）。

## 3. 校验摘要（前台独立复跑）

- 结构：2 HDU（signal + COVERAGE），CHECKSUM/DATASUM 在位；`p3_resampled.json` planes=[signal, coverage]；
- 数值：M42 finite 中位数 9.088e12（NaN 3.53%）；GC finite 中位数 4.999e11（NaN 6.81%）；**零值 0.0000%**；
- `p3_props.json`：`variance_available=false`、`ivar_available=false`、`uncertainty_source=none`（与 P0-02 一致：Phase1 未产 variance/ivar）；
- WCS：CRVAL 83.747318/−5.361391（M42）、−87.099xxx/−18.179xxx（GC）；CD 对角、尺度与输入一致。

## 4. 已知限制（交付时必须一并说明）

1. **接缝**：VIS-001 判定「无接缝/背景均匀」不通过（面板间背景台阶），根因 P0-09（生产 UPM 无稀疏天光面）；
2. **权重口径**：mosaic 使用 `weight_mode=1` + `legacy_allow_weight_fallback=true`（显式等权降级），因 Phase1 不产逐帧 ivar（P0-02）；**不满足**新文档包要求的逆方差叠加；
3. **单位口径**：BUNIT=ADU 而数值为 HiPS 面元归一后的量级（~1e11–1e13），单位与量级口径待裁决（PRE-F-03 同族）；
4. 仅 R 通道（按负责人指示），未做 G/B/H-alpha 叠加。

## 5. 复现命令

> **磁盘前置检查（务必）**：L4 全量 R 通道的中间产物峰值约 **70 GB**（HiPS 瓦片 + mosaic 中间帧），
> 复跑前请确认 `/workspace` 可用空间 **≥ 100 GiB**；脚本已内置 `df` 余量闸（<30 GiB 直接 ABORT）。
> 本控制包首次运行时未做该评估，曾造成磁盘压力，中间产物已于事后清理至 1.6 GB（仅保留交付帧/日志/证据）。

```bash
python3 run/RELEASE-01/e2e/l4/gen_stage23.py          # 生成 p2/p3 配置
./run/RELEASE-01/e2e/l4/run_normals.sh                # 15 个 normalize 作业（81 帧）
./run/RELEASE-01/e2e/l4/run_stage23.sh                # 2 个 mosaic + 4 个 export
python3 run/RELEASE-01/e2e/fits_probe.py <fits>       # 头部/数值抽检
```
