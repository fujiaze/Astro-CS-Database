# Export FITS 读回复核

verify 重开已发布 FITS，核对声明、结构、像元与完整性；它不重做采样，也不把内部自洽称为独立天文真值。产品与坐标要求见 [Export 管线](../export/pipeline.md)和[投影正本](../../science/projection/HIPS_TO_FITS.md)。

## 生产入口与交接

[adapter](../../../lib/infrastructure/scheduler/src/module_adapters.cpp) 的 `p3_op_verify` 对应 `acsd.phase3.verify`，读取 `p3_resampled.json/.bin`、`p3_wcs.json`、`p3_writer.json`，从 writer 记录定位实际 FITS。它用同一裁剪 plan 生成预期窗口，使用 `P3FitsVerifyStream` 分块比对，写 `p3_verify.json`。生产 `unc=false` 且只校验 PRIMARY signal，不能要求不存在的附加 HDU。

公开头 [p3_output.h](../../../lib/algorithms/fits_output/p3_output.h)及[实现](../../../lib/algorithms/fits_output/p3_output.cpp)还提供整幅 `p3_output_verify` / `_verify_ex`。各入口的能力与生产调用分开，不将头注释当运行证据。

## 校验对象、坐标与所有权

| 检查/接口 | 合同 |
|---|---|
| `P3FitsVerifyStream.open` | 只读重开，预期尺寸、HDU 面和 WCS，has_uncertainty=false |
| `check_block(plane,x0,y0,w,h,expected)` | plane=0，零基子块，对实际源窗口逐像元比对 |
| `close(result)` | 汇总 reopen/coverage 结果并求实际文件 sha256 |
| 元数据核对 | resampled/writer 的 BUNIT、output_mode、measurement_capable 一致 |
| 产品 hash | verify 独立重算 canonical hash，再与 writer 自述比较，不直接采信写侧值 |

WCS 使用 FITS 一基 CRPIX、ICRS degree 和 degree/pixel CD；crop 后按实际输出尺寸校验，不能强制 CRPIX 仍居中。NaN 按约定同态比对，真实零/负值作为正常信号。缺 tile 或内孔可产生合法 NaN；是否异常丢失必须与输入有效域/核支持比较，不能仅按洞的位置判断。

stream 持有读句柄，调用者持有期望子块并复用有界缓冲；校验串行读，不自建线程预算。输入工作文件须来自同次运行/plan，不能读到陈旧目录后把恰好相同的尺寸当同一产品。

## 错误与验证边界

重开、尺寸、HDU、WCS/单位、像素、checksum/hash 或摘要分叉通过非 OK/Result 错误上行，最终状态不写成 verified。局部状态码依 P3OutputStatus，不直接冒作 CLI 退出码。取消与临时对象清理由 Runtime/统一 I/O 管理，不把读后发现错误等同此前从未发布文件。

本节点证明生产写读相符与文件完整性的实际范围；相同 WCS 公式两端同错仍可能通过。绝对 sky/手性、独立源通量、有效 PSF、低频星云与噪声统计须用外部 WCS/HEALPix reader、已知天空模拟或真实观测另验。

验证入口为本页 stream/整幅接口、[WCS 单元入口](../../../eng/tests/unit/test_wcs_plausibility.py)与生产五节点调用。应注入头/像元/hash 篡改、截断、非对称 crop、工作文件版本分叉及合法空洞。本页未运行验证，不继承无复现载体的通过结论。
