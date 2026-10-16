# Export 平面 FITS 写出

writer 把重采样信号写为普通二维 WCS FITS。当前生产仅 PRIMARY signal，一个 HDU；不要求输入马赛克具有 variance/ivar，不从单信号制造不确定度。产品和单位要求见[HiPS 到 FITS 正本](../../science/projection/HIPS_TO_FITS.md)与 [Export 管线](../export/pipeline.md)。

## 实际入口与发布路径

[module_adapters.cpp](../../../lib/infrastructure/scheduler/src/module_adapters.cpp) 的 `p3_op_writer` 对应 `acsd.phase3.writer`。输入 `p3_resampled.json/.bin`、`p3_wcs.json` 和 run_context，按同一 crop plan 取子窗。生产使用 `P3FitsStream` 和有界子块 scheduler 写 `output_phase3.fits`，并写 `p3_writer.json`；`unc=false`、`nplanes=1` 明确单信号。整幅 API 仍是库/故障对照能力，不能替代生产子块路径描述。

公开头 [p3_output.h](../../../lib/algorithms/fits_output/p3_output.h)，实现 [p3_output.cpp](../../../lib/algorithms/fits_output/p3_output.cpp)。WCS/裁剪复用 [p3_wcs.h](../../../lib/algorithms/projection/p3_wcs.h)。

## API 与文件合同

| 接口/对象 | 语义 |
|---|---|
| `P3FitsStream.open` | output_path、WCS、W/H、BITPIX、BUNIT、provenance → 同目录私有临时文件 |
| `begin_hdu/write_block/end_hdu` | plane=0 的信号子块；x0/y0 零基、w×h 行主序 f32 |
| `publish` / `abort` | flush/close/fsync、校验/hash、原子改名，或清理临时对象 |
| `p3_output_write_atomic` / `_ex` | 整幅缓冲入口；ex 的误差参数存在不表示本命令发布误差扩展 |
| `P3OutputResult` | sha256、reopen_ok、coverage_ok、covered_px、total_px |
| `P3Provenance` | 输入 hash、软件/运行身份、order、sampler 和可用性说明 |

最终图像 BITPIX 为 -32 或 -64，由真实配置传入；WCS 的 CRVAL/CD 为 degree/degree-per-pixel，CRPIX 为 FITS 一基。裁剪用相同源子窗，CRPIX 减原点，不能重新拟合 WCS或额外插值。无有效贡献 NaN，有限零与负信号合法。

科学要求 BUNIT 继承实际光度尺度；当前 adapter 仍检查固定面亮度 canonical 字符串。该检查的存在不证明 response-calibrated/relative 输入单位链已闭合，相邻限制依[差异地图](../../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)。

## 所有权、并行与错误

调用方拥有源子块，写调用期间保持有效；stream 拥有临时文件和内部状态，析构/abort 按实现清理。FITS 写句柄串行，读取/搬运可经 Runtime 预算并行；队列与在途块受配置限制。内部 coverage 可用于计数，最终不建 COVERAGE 扩展。

P3OutputStatus 为 OK、PARAM、IO、CANCELLED，`p3_output_last_error` 仅诊断，不作状态真源。缺 run/hash、工作数组截断、非法几何、I/O/hash/重开失败要上行；单 signal 的 uncertainty unavailable 是能力声明。原子文件发布依[发布合同](../../engineering/contracts/ATOMIC_PUBLISH.md)，不能由最终路径存在推断全部运行成功。

## 验证入口

从 `P3FitsStream`、`P3FitsVerifyStream` 及[生产 verify 卡](acsd.phase3.verify.md)进入；独立 reader 核对单 HDU、尺寸/BITPIX/WCS、真实单位、子窗像素、checksum/hash、取消/故障残留与大图资源。代码含流式路径不等于本页执行了内存/性能验证，也不等于完成科学保真或发布验收。
