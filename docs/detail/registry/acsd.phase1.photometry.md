# Normalize 测光与标度应用

测光消费同帧 cleaned 像素、已解 WCS/SIP、科学源及 PSF，使用 Gaia XP 与响应拟合线性标度，并恰应用一次。物理响应、参考约定和跨通带限制由[测光分册](../../science/photometry/PHOTOMETRY.md)及 [Normalize 科学正本](../../science/normalize/NORMALIZE_SCIENCE.md)拥有。

## 实际入口与交接

[module_adapters.cpp](../../../lib/infrastructure/scheduler/src/module_adapters.cpp) 的 `p1_op_photometry` 对应 `acsd.phase1.photometry`，调用 `acsd::photometry::fit_frame_photometry`。其 [FramePhotFitRequest/Result](../../../lib/algorithms/photometry/cpp/src/frame_photometry_fit.h)装配本地 Gaia、滤镜/QE、PSF 通量和完整 WCS，内部进入 `pc_calibrate_simple_with_gaia_f64_v2_qf`。

descriptor 声明 calibrated、sources、wcs 输入和 fluxes、photprov 输出；真实像元取 sources 指向的 cleaned 面，PSF 参数也随 sources 工作文件到达。当前载体含 `p1_phot.json` 和 photoapplied FITS。目标 `PhotometricSolution`、`PhotoAppliedPlane` 与独立 PSF 输入依[数据合同](../../engineering/contracts/NORMALIZE_DATA.md)，端口排序不能冒称完整 typed 取数。

## API、字段与单位

[photometric_calib.h](../../../lib/algorithms/photometry/cpp/include/photometric_calib.h)提供标量/双精度 Gaia 测光及 `pc_project_sky_to_pixel`；[frame_photometry_fit.h](../../../lib/algorithms/photometry/cpp/src/frame_photometry_fit.h)提供文件无关的单帧装配接口。

| 字段 | 消费语义 |
|---|---|
| psf_cx/cy、psf_flux/status/quality | 同一 source 行绑定；像元为 detector 零基，flux 带具体 PSF 估计器与积分单位 |
| crval/crpix/cd/sip | ICRS degree、FITS 一基参考像元、degree/pixel 和完整 SIP |
| filter_name、qe_name、曲线身份 | 实际响应积分依据；滤镜头标签不能代替曲线/QE 内容 |
| k_photo、fit_ok、failure_scope | 标度、是否真实拟合得到、失败属于帧或环境；占位 scale=1 不表示已施加 |
| sigma_residual_dex | 匹配对数残差散度；不是像元噪声或参考 SNR |
| spatial | 空间乘性场与支持；与全局标度共同决定实际像元变换 |
| synthetic_colour_* | 星表与响应的天区合成颜色摘要，mag/星数/布尔；不是独立帧零点或探测器增益测量 |

`photscale_fit` 记录拟合结果，`photscale_detail` 与实际已施加帧集合配对；不能仅凭拟合成功判断某个下游面已缩放。线性像元、参考通量及协方差须按同一次应用配对；BUNIT 随真实数值标度表达。Gaia 恒星拟合不能自动校正未知星云 SED。

## 所有权、配置与错误

request 借用像素、PSF 与 SIP 数组，同步调用期保持有效；result 由调用方持有。adapter 负责 AIO、逐帧应用与 provenance，算法不以 output_dir 猜上游。拟合按帧组织，线程预算来自 Runtime；返回码、`fit_ok` 和具名 reason 分开处理。

配置主要为 `photometry.fit`、block `filter_passband`、Gaia 目录、响应/QE 和空间拟合参数；`photometry.frame_overrides` 已有逐输入路径/basename/key的首项命中绑定，仍不等于严格输入 occurrence 序号。当前响应、参考 SED、标度 application ID 与单位的具体覆盖见[实施差异地图](../../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)。本页不另立参考通带、公共零点表或数据组合禁令。

## 验证入口

[光度合成测试入口](../../../eng/tests/synthetic/test_p1_photometry.py)与[生产阶段一入口](../../../eng/tests/pipeline/test_stage_normalize.py)用于不同层次核对。验证应检查 PSF/source 行绑定、非零 SIP、已知 SED/响应、scale 一次与协方差二次幂、缺响应/拟合不足状态和颜色诊断独立性。源码接线不等于已完成上述实测。
