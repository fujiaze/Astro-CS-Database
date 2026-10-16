# Normalize PSF 拟合与模型

PSF 节点把已解 WCS 引导的源测量转为逐星轮廓参数。空间、像元积分归一、有效域与通量估计器的目标由 [Normalize 科学正本](../../science/normalize/NORMALIZE_SCIENCE.md)和[数据合同](../../engineering/contracts/NORMALIZE_DATA.md)定义。

## 生产调用与接口

[adapter](../../../lib/infrastructure/scheduler/src/module_adapters.cpp) 的 `p1_op_star_psf_impl` 对应 `acsd.phase1.star-psf`，消费 `cleaned` 与 `wcs`，输出 `sources`、`psf`。科学源检测后，它将检测行送入 `dpsf_fit_batch_f64`。函数存在且由生产调用，不应写成未接节点。

公开头为 [dynamic_psf.h](../../../lib/algorithms/psf/include/dynamic_psf.h)。uint16 接口 `dpsf_fit`、`dpsf_fit_batch`，float 接口 `dpsf_fit_batch_f`，及 double 的 `dpsf_fit_batch_d` 返回 `DPSFFitResult`；分配型批结果统一用 `dpsf_free_results` 释放。

生产 compact 接口为：
`dpsf_fit_batch_f32/f64(image,width,height,detections,n_detections,params,out_psf_params,out_n_valid,out_status)`。调用方提供输出缓冲，返回 0 表示批调用完成，逐星结果仍需查状态；非法参数返回 -1。

## 字段、坐标与消费者

| 对象 | 字段与语义 |
|---|---|
| detections | double [N,6]，位置、检测 flux/mag、饱和状态；flux 估计器与 PSF flux 分名 |
| compact 参数 | double [n_valid,9]，B、A、cx、cy、sx、sy、theta、fwhm_x、fwhm_y |
| out_status | 长度 N，按输入检测索引；成功参数行按检测下标升序 compact，不能直接将参数第 i 行当输入第 i 星 |
| DPSFFitResult | 另有 status、mad、flux、eccentricity；B/A 为图像尺度，位置/宽度/FWHM 为 px，theta 为 rad |
| 参数配置 | fitRadius 为 px、maxIter 为次数、tolerance 为数值容差；实际消费以实现为准 |

像元中心沿 detector 零基合同，sky 关联沿 ICRS degrees。帧/source ID 与 compact 映射同时保留。photometry 和 noise 当前通过 sources 工作文件中的参数取数；独立 `psf` 端口尚无完整显式消费者，不能因此写成 PSF 数值完全无人消费。

逐星 Gaussian 检测宽度、Moffat4 拟合参数、PSF 残差与背景噪声是不同量。FWHM 或残差可作诊断，不能独自替代后续模板 h 或面亮度统计权重。总通量、窗内通量与截断策略须配对记录。

## 所有权、并行与错误

算法借用图像/检测数组；调用方持有 compact 参数和逐星状态。不同星拟合可并行，节点由 Runtime 注入预算；不建立独立资源来源。批级返回与星级 `DPSF_FIT_*` / `DPSF_PSF_STATUS_*` 分开处理，空窗/未收敛/分配失败不能当有效参数。

当前接口是逐星拟合；空间有效 PSF 求值、像元积分归一与噪声 completion 的真实覆盖由[差异地图](../../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)维护。目标 `EffectivePsfModel` 不能仅靠帧中位宽度或 `psf_spatial_order` 名称宣称完成。

[pixel_integrated_psf.h](../../../lib/algorithms/psf/include/acsd/pixel_integrated_psf.h) 已提供 `pixel_integrated_moffat4_profile(request,out,diagnostics)`：request 指定 sx/sy/theta、零基参考中心、像元中心数组和求积阶数，借用输入数组，写调用方 vector；diagnostics 给实际集合包络及相邻阶自检差。completion 已调用此库，但当前调用传帧级圆化宽度。函数的像元积分能力不能代替空间 PSF 的真实参数与留出验证，相邻阶差也不构成严格真误差界。

## 验证入口

从本头文件批接口和[阶段一管线测试](../../../eng/tests/pipeline/test_stage_normalize.py)进入，检查已知轮廓恢复、非首行失败后的 compact/source ID 映射、边缘截断、饱和、空间变化及按位置积分归一。串并行、跨类型与真实模型误差需实际读数，本页不列未经复核的通过标签。
