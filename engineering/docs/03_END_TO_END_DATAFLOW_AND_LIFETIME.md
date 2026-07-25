# 03 端到端数据流与生命周期

## 1. 三类数据

- 控制面：命令、路径、配置、进度、错误、结果；可使用 JSON/JSONL。
- 运行时数据面：像素、星点、WCS、PSF、SNR 模型；仅存在原生内存。
- 持久化面：FITS/XISF、Master 帧、Gaia 数据、HISS、HCSD、日志、配置快照和报告。

## 2. JobContext 生命周期

CLI 接收命令后创建 JobContext，包含 job_id、有效配置、输入/输出清单、模块能力、取消 token、临时目录和证据目录。任务结束后释放；必要摘要写入结果 JSON 与日志。

## 3. Stage 1 数据块流

| 节点 | 消费 | 产生/修改 | 何时销毁 |
|---|---|---|---|
| READ_FITS | 输入文件 | `data`, `header` | FITS 解码临时对象在复制/移交后销毁 |
| CALIBRATE | data/header + Master | 替换 `data`, `cal_stats` | Master 可在批次上下文缓存；单帧临时输出移交后销毁 |
| STAR_DETECT | 校准后 data | `star_det` | uint16 兼容缓冲与 detector 原始数组在块写入后销毁 |
| PLATESOLVE | star_det/header + Gaia astrometry | 更新 header，产生 `astrometry_stats`、可选 `astrometry_matches` | 求解临时图、三角、RANSAC、catalog 数组在结束后销毁 |
| PSF | data + star_det | `psf` | fit 原始结果写入块后销毁；新接口直接消费 float32 |
| PHOTOMETRIC | data/header/psf + DR3SP/曲线 | 替换 `data`, `photo_stats` | 光谱、匹配、回归临时数组结束后销毁 |
| SNR | psf/photo_stats/header | `snr_model` RAW | SnrModel 序列化后原模型销毁 |
| DRIZZLE | data/header/snr_model | HISS 临时文件→正式文件 | HEALPix 累积与稀疏数组写完后销毁 |
| END | 全 Frame | 无 | `PipelineFrame` 整体销毁 |

## 4. 数据修订号

每次 `data` 块被替换，`header` 中 `ASTROCS.DATA_REVISION` 加 1。派生块必须记录输入 revision：

- `ASTROCS.STARDET.INPUT_REVISION`
- `ASTROCS.PSF.INPUT_REVISION`
- `ASTROCS.PHOTO.INPUT_REVISION`

STAR_DETECT 与 PSF 都应消费 CALIBRATE 后的同一 revision。PHOTOMETRIC 替换 data 后，不需要重做 PSF；全局乘性 scale 对 `(A-B)/MAD` 理论上不改变，但必须在契约中写明该语义。

## 5. HISS 边界

HISS 至少包含：

- 格式版本、nside、ordering、n_pix；
- `ipix:uint64[]`, `pixel:float32[]`；
- 稀疏球面 SNR 模型；
- 原始输入标识与哈希；
- 生效配置哈希；
- 校准、WCS、PSF、测光和 Drizzle 摘要；
- 模块版本与构建 ID；
- filter、时间、曝光、设备信息。

写出成功后重新读取并检查索引递增、有限值、长度、metadata schema 和 SNR 控制点。

## 6. Stage 2 数据流

每个 HISS 由 Stack 模块读取，检查兼容性后建立重叠图。球面梯度参数、重叠采样、局部样本、权重与拒绝标志均为临时数据，完成分块后释放。HCSD 写入采用子叶/索引分块，避免全天球全部常驻内存。

## 7. HCSD 边界

HCSD 至少包含最终 `ipix/pixel`、子叶索引、格式版本、输入 HISS 清单与哈希、配置、模块版本、覆盖统计与叠加摘要。每像素覆盖数/方差/权重/拒绝数是否作为正式通道，由 ADR 决定；未决前不得悄悄改变格式。

## 8. 恢复边界

- 未保存 `.aio` 时：Stage 1 中断后从原始输入重跑。
- `.aio` 正式启用后：快照必须包含 schema、配置哈希、输入哈希与所有所需块；不匹配则拒绝恢复。
- Stage 1 完成后：HISS 是进入 Stage 2 的稳定恢复点。
- 临时 `.partial` 文件不得被扫描为有效输入。
