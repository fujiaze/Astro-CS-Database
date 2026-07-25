# 04 PipelineFrame 数据块契约 v1

## 1. 所有权

- `aio_frame_add_block`：复制，调用者释放原始内存，Frame 释放副本。
- `aio_frame_add_block_move`：所有权移交，内存必须与 Frame 的释放器兼容；调用者不得再次访问。
- 替换同名块前必须先验证新块，防止失败后丢失旧数据。
- 各 DLL 不得保留指向 Frame 内存的长期裸指针。

## 2. 必需块

### `data`

- 类型：FLOAT32
- 维度：`[height,width]`
- 布局：row-major
- 生产者：READ_FITS，CALIBRATE/PHOTOMETRIC 可替换
- 约束：尺寸不变；数值允许有限负值直到校准策略明确，禁止 NaN/Inf 静默进入 Drizzle。

### `header`

- 类型：KV
- 生产者：READ_FITS
- 修改者：各 stage 仅写自己的命名空间键；WCS 标准键由 PLATESOLVE 更新。
- 禁止：用同一键表达不同单位。

### `star_det`

- v1 详见 `contracts/star_det_block_v1.md`。
- 单次运行唯一生产者由 PlateSolve 路径 ADR 决定：路径 A 为 STAR_DETECT，路径 B 为 PLATESOLVE_INTERNAL_EXPORT。
- 消费者：路径 A 下为 PLATESOLVE、PSF；路径 B 下为 PSF；诊断工具只读。
- 生产者、路径标识、hash 和 count 必须写入 Header/provenance。

### `psf`

- 类型：FLOAT64 `[N,9]`
- 列：status,B,flux,cx,cy,mean_fwhm,A,mad,eccentricity
- 唯一生产者：PSF
- 消费者：PHOTOMETRIC、SNR
- 行与 `star_det` 必须一一对应；不得排序后不保留索引映射。

### `photo_stats`

- 类型：KV
- 必需键：schema_version,status,n_matched,scale_factor,sigma_residual,input_data_revision,output_data_revision
- 唯一生产者：PHOTOMETRIC

### `snr_model`

- 类型：RAW
- 布局：`uint32 n + n*(double ra,double dec,float snr_psf) + 3*double`
- 必须增加 schema/version 和字节序标识；旧裸布局通过兼容读取器处理。
- 唯一生产者：SNR；消费者：DRIZZLE/HISS writer。

## 3. 推荐诊断块

- `cal_stats`：实际使用的 Master、缩放、坏点、无效值统计。
- `astrometry_stats`：RMS、匹配数、检测数、catalog 数、模型阶数。
- `astrometry_matches`：可选，详见对应契约；用于复核和后续优化，不作为 PHOTOMETRIC 的强依赖。

## 4. Schema 验证

每个 stage 入口必须验证：块存在、类型、维度、count、schema、revision、有限值比例。验证失败返回稳定错误码，不得自动重算上游数据。

## 5. 数据块销毁策略

为降低峰值内存，可在最后消费者完成后删除：

- `star_det`：PSF 完成后若不需调试可删除；但若需生成诊断报告，可保留至报告完成。
- `astrometry_matches`：测光候选优化未启用时可在 PLATESOLVE 报告完成后删除。
- `psf`：SNR 完成后可删除。
- `cal_stats/photo_stats`：体积小，保留至 HISS 元数据组装。

删除策略必须由 Orchestrator 统一决定，算法模块不得擅自删除别人的块。
