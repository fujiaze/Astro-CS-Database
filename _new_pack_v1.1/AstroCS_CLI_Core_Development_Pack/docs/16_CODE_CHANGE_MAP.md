# 16 代码修改定位图

以下路径基于当前上下文仓库；Agent 必须先在实际仓库确认函数仍存在。

## 1. Orchestrator

主要文件：

- `lib/orchestrator/cpp/include/orchestrator.h`
- `lib/orchestrator/cpp/src/orchestrator.cpp`
- `lib/orchestrator/cpp/src/cli_command.cpp`
- `lib/orchestrator/cpp/src/main.cpp`

修改点：

1. 增加 PlateSolve detection-path capability 与运行时 provenance；
2. 路径 A 仅在全量 TestData PASS 后加入显式 `STAR_DETECT` 并调用 `ipv_solve_from_detections_v1`；
3. 路径 B 保持原 PLATESOLVE 顺序，通过 detection sink 把内部同次检测结果复制为 `star_det v1`；
4. 两条生产路径都必须保证每帧 detector 一次，且禁止运行时静默切换；
5. PSF 改用 `dpsf_fit_batch_f32` 并记录消费的 `star_det` hash；
6. 删除 PLATESOLVE 末尾无消费者 `gaia_cat` 二次 cone search，或改为显式可选诊断；
7. 所有 DLL/块缺失从 warning+true 改为稳定错误；
8. 增加 stage/event、质量指标和调用计数测试钩子。

## 2. PlateSolve

主要文件：

- `lib/plate_solve/cpp/ipv/include/ipv_api.h`
- `lib/plate_solve/cpp/ipv/include/ipv_select.h`
- `lib/plate_solve/cpp/ipv/src/ipv_select.cpp`
- `lib/plate_solve/cpp/ipv/src/ipv_entry.cpp`
- `lib/plate_solve/cpp/ipv/src/ipv_solver.cpp`

### 候选路径 A

- 将“检测之后”的逻辑抽成 detections 输入函数；
- 保持现有选星、Gaia 密度估算、投影、RANSAC、robust_refine 不变；
- 只用于全量 TestData A/B，PASS 前不得替代生产路径。

### 保守路径 B

- 原 `ipv_solve_from_memory` 图像输入和内部检测路径保持不变；
- 在内部 detector 返回后增加只读 sink/callback；
- callback 导出完整原始检测结果，不改变排序和选择；
- callback 关闭/开启时的 WCS 结果必须在旧路径重复性容差内一致；
- 不返回跨 DLL 所有权不明的堆指针。

## 3. Dynamic PSF

主要文件：

- `lib/dynamic_psf/include/dynamic_psf.h`
- `lib/dynamic_psf/src/dpsf_image.*`
- `lib/dynamic_psf/src/dpsf_psf.*`

修改边界：

- 新增 float32 image view/reader；
- 拟合内部统一转 double 或直接读取 float，不建立全图 uint16；
- 保持坐标、fit radius、模型和结果 ABI 一致；
- 旧 uint16 API 可转调共用模板/实现，避免维护两套算法。

## 4. Star Detector

第一版不修改检测算法。只允许增加：

- API 版本/能力查询；
- 测试构建调用计数；
- 参数序列化/hash；
- 明确输出内存释放器。

禁止为了让候选路径通过 A/B 而改变 detector 参数或算法。

## 5. PipelineFrame

主要文件：

- `lib/astro_image_io/include/aio_pipeline.h`
- `lib/astro_image_io/src/aio_pipeline.cpp`
- `lib/data_pipeline/include/aio_pipeline.h`
- `lib/data_pipeline/src/aio_pipeline.cpp`

先完成 ADR 决定唯一归属，再增加 schema/revision 工具。不要同时修改两份并继续双重导出。

## 6. Tests

至少增加：

- PlateSolve 全 TestData manifest 与哈希；
- 旧路径三次重复性基线；
- detections 输入候选 API 全量 A/B；
- internal detection sink 零影响测试；
- `star_det v1` round-trip；
- PSF float32 high-dynamic-range test；
- Orchestrator single detector call test；
- selected-path capability/provenance test；
- strict missing API/module tests；
- HISS/HCSD inspect and round-trip tests。
