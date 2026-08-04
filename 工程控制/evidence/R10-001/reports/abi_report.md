# R10 ABI 报告

日期: 2026-08-04 | 提交范围: `85fa651..8cb22a9` | 当前 HEAD: `8cb22a9`

## 1. 公共类型与精度 ABI

| 文件 | 内容 |
| --- | --- |
| `lib/common/include/astro_scalar.h` | `AstroScalarType` 枚举（FP32/FP64）+ 精度 traits |
| `lib/common/include/precision_context.h` | `PrecisionContext` 单例（EXE/DLL 各有副本，跨 DLL 用显式 API 传播） |
| `lib/astro_image_io/include/astro_image_io.h` | `aio_set_precision_mode(int is_fp64)` 显式跨 DLL 精度设置 |
| `lib/astro_image_io/include/aio_healpix_io.h` | HISS FP64 读写：`aio_hiss_read_tile_signal_f64`、`aio_hiss_query_pixel_f64` |

## 2. 模块级双精度 API（R10 新增）

| 模块 | FP64 API | 证据 |
| --- | --- | --- |
| astro_image_io | `aio_read_fits` 按 PrecisionContext 读入 `data_f64`（不降级） | fp64 日志: `FP64 mode: pixels stored in data_f64 (no float32 downgrade)` |
| calibration | Master/图像均按精度读入 FP32/FP64 | 日志: `[CALIBRATE] 图像 ... FP32/FP64` |
| dynamic_psf | `dpsf_fit_batch_d` / `dpsf_fit_batch_f64` | fp64 日志: `[PSF] 调用 dpsf_fit_batch_d (FP64, ...)` |
| photometric_calib | `pc_calibrate_simple_with_gaia_f64` | fp64 日志: `[PHOTOMETRIC] 调用 pc_calibrate_simple_with_gaia_f64` |
| healpix_drizzle | `hp_drizzle_run` 精度参数 + FP32/FP64 累计器 | 日志: `precision_mode=0/1, signal_dtype=0/1` |
| snr_estimator | `SnrControlPoint` `#pragma pack(1)` sizeof=20 | commit `8cb22a9` |

## 3. 结构体打包修复（关键）

`SnrControlPoint` 增加 `#pragma pack(push,1)` 与 `static_assert(sizeof==20)`：

- 修复前: `sizeof=24`（4 字节尾部填充），orchestrator 按 20 字节 `memcpy` 连续序列化 → 从第 2 个点起 ra/dec 错位，1609 个点被判"越界"
- 修复后: `sizeof=20`，FP32 1979 点全部有效，FP64 1947 点有效（其余全部归因 INVALID_PSF）

## 4. 跨 DLL 精度上下文

`PrecisionContext` 单例在 EXE 与各 DLL 中各有一份副本（Windows DLL 静态链接语义），因此 R10 增加显式
`aio_set_precision_mode(int is_fp64)`，由 orchestrator 在流水线启动时设置，DLL 内部用模块级精度变量读取。
日志证据: `[API] Precision mode set to FP32/FP64`。

## 5. 已知的精度例外（如实记录）

1. **星点检测固定 FLOAT32**（`lib/plate_solve/cpp/ipv`）: FP64 模式下 orchestrator 显式转换 FLOAT64→FLOAT32 供检测器使用（性能约束"星检测串行"）。日志: `[PLATESOLVE] data 块为 FLOAT64, 已转换为 FLOAT32 供星点检测使用`。WCS 拟合本身为 double。
2. **WCS/SIP、PSF 拟合、测光最小二乘内部使用 double**（非线性拟合标准做法），FP32 模式下输入为 float32，FP64 模式下输入为 float64 专用 API（不降级）。
3. **FP32 Drizzle 累计器为 FP32**（PREC-009 已修复，不再 double 累计后截断）。

## 6. 结论

R10 实现"真正全链路 FP32/FP64"：每个阶段的输入/输出 dtype 有运行时日志证据（见 `precision_trace.jsonl`），
FP64 模式下除星点检测器（float32 例外，已注释说明）外无 float32 降级。
