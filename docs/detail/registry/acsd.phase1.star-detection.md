# Normalize 科学源检测

本页登记检测库与八节点 `star-psf` 中的科学源生产。处理顺序与两类星点的区别由 [Normalize 科学正本](../../science/normalize/NORMALIZE_SCIENCE.md)规定；检测本身不是 WCS 已解或条件参考精度正确的证明。

## 入口、调用者与消费端

[公开头](../../../lib/algorithms/star_detection/include/star_detector.h)及 [sdet_api.cpp](../../../lib/algorithms/star_detection/src/sdet_api.cpp)提供两类入口：

| 路径 | 调用及职责 |
|---|---|
| `sdet_detect_ex` / `sdet_detect_ex_f64` | 图像盲检；IPV 用于生成私有 `solver_detections`，无已解 WCS 前提 |
| `sdet_detect_guided_ex_f64` | 接收调用方预测位置，逐位置质心/轮廓拟合；完整 WCS 后生产 `science_sources` |
| `sdet_create` / `sdet_destroy` | 管理检测参数/句柄；同一 handle 不并发使用 |
| `sdet_free_detect_ex` | 成组释放输出位置、通量、标志及 extras |

CLI 调用链为 [module_adapters.cpp](../../../lib/infrastructure/scheduler/src/module_adapters.cpp) 的 `p1_op_star_psf_impl → p1_guided_predict → pc_project_sky_to_pixel → sdet_detect_guided_ex_f64`。投影消费本帧已解 WCS 与 SIP，近似指向仅用于搜索/初值，不能代替最终预测。该步骤属于 `acsd.phase1.star-psf`，没有另一个独立 `star-detection` 生产调度节点。

输出经 source ID 与 PSF 参数关联，供 photometry、noise、Drizzle 使用；解算候选与科学源不互相冒充。实际跨节点载体含 `p1_sources.json`，完整 typed 对象目标见[数据合同](../../engineering/contracts/NORMALIZE_DATA.md)。

## 输入输出合同

输入图像为行主序 uint16 或 double；引导位置是 detector 像元坐标，外部 sky 为 ICRS degrees。内部数组中心遵循零基索引、FITS WCS 桥接加一；不得从头部注释中的半像元写法另造一次偏移。投影/adapter 必须按同一合同核对。

输出包含 x/y、flux、mag、saturated、has_saturated 和 extras；位置为 double，但既有 C API 的 flux/mag 输出数组是 float。检测 flux 的估计器身份与后续 PSF 积分通量分开，不能按相同字段名互换。

`SDetGuidedStats` 分列 predicted、dropped、fit_failed、rejected、fit_ok、output；去重/截断前后数量各有意义。单候选拟合失败可丢弃并计数；底层零预测域返回成功且 count=0，节点层再按所需下游能力判断。

## 参数、所有权与错误

配置 `star_detection` 提供 mode、Gaia 目录、查询/星数范围与诊断阈值；PSF 拟合样本上限和 `snr.max_sources` 交付上限不是同一个数。当前 mode 分支及可用 WCS 检查由 adapter 执行，不在库内查询星表。

库返回 0 表示调用完成，-1 表示参数/分配失败；候选丢弃用统计表达。调用方必须使用成组 free，视图有效期覆盖同步调用。候选拟合可按独立位置并行，宿主提供预算；不能仅按实现循环推断不同 worker 数逐位等价。

## 验证入口与边界

[检测科学分册](../../science/detection/STAR_DETECTION.md)说明召回、质心和选择偏差的定义。验证应以已知位置/亮度的注入场、错位预测、空域、边缘、饱和、拥挤与去重计数覆盖公开入口；[阶段一管线测试](../../../eng/tests/pipeline/test_stage_normalize.py)用于检查实际节点序。独立绝对 WCS 残差和 source SNR/参考精度仍需分别验证；当前差异统一见[实施地图](../../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)。
