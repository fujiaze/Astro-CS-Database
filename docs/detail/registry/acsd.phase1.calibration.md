# Normalize 校准

校准把 lights 和已绑定的 bias/dark/flat 转为同标度的积分线性像元。母版约定、暗流曝光比例、平场归一与误差传播取自[校准科学分册](../../science/calibration/CALIBRATION.md)及 [Normalize 科学正本](../../science/normalize/NORMALIZE_SCIENCE.md)。

## 生产入口与数据交接

CLI 八节点的入口是 [module_adapters.cpp](../../../lib/infrastructure/scheduler/src/module_adapters.cpp) 的 `p1_op_calibrate`，descriptor 为 `acsd.phase1.calibration`。它消费 `frames`，调用校准库，产生 `calibrated` 供 cosmetic 和 IPV；不会由本节点生成 WCS。局部 `p1_session` 也调用同一库，但其会话边界不同。

当前 adapter 读取 lights/masters，检查尺寸与声明，逐帧调用 `ac_calibrate_frame`，经 AIO 写校准工作面及母版来源记录。像素载体仍包含 FITS 工作文件；目标 `CalibratedPlane`、identity 和对象版本见[数据合同](../../engineering/contracts/NORMALIZE_DATA.md)。

| 字段/数组 | 单位、坐标与有效性 |
|---|---|
| light、bias、dark | 同一已解释的积分线性尺度；FITS BSCALE/BZERO 仅应用一次 |
| flat | 归一后的相对响应；下界和受影响范围由科学分册说明 |
| width、height、pixels | 行主序 `y*width+x`；内部零基像元中心 |
| K、actual_k | 无量纲；曝光来源/暗场含偏置约定由 adapter 登记 |
| 输出 | 负值与有限零合法；坏值及母版能力须携带来源，不能静默补零 |

## 公开 API 与所有权

签名头为 [astro_calibration.h](../../../lib/algorithms/calibration/include/astro_calibration.h)，实现位于 [calibrator.cpp](../../../lib/algorithms/calibration/src/calibrator.cpp) 和 [ac_api.cpp](../../../lib/algorithms/calibration/src/ac_api.cpp)。

`ac_calibrate_frame(light,width,height,master_dark,master_flat,master_bias,out,dark_optimization,dark_scale_factor,actual_k)` 读取调用方数组并写调用方 `out`；不分配输出、不读文件。对应 `_f64` 是独立入口，不能只因名称含 f64 就假定所有母版/修复 API 均无内部降精度。

`ac_generate_master_bias`、`ac_generate_master_dark`、`ac_generate_master_flat` 及其 f64 变体为库能力；CLI 本节点消费已提供母版，不能写成每次 normalize 自动生成它们。`ac_set_num_threads` 改 OpenMP 设置，由宿主预算调用；模块不另定资源预算。

## 配置、错误与实现边界

配置来自本次 block 的 `input_lights`、`master_*`、暗场约定/曝光与标度声明。不得从文件后缀猜归一因子；K 应与真实曝光配对。返回码定义于公开头（`AC_OK`、`AC_ERR_*`），adapter 非成功时转 `Result` 错误；不能把声明了错误枚举等同所有异常已转码。

像元算术与母版随机/共同不确定度是不同能力；当前覆盖以[差异地图](../../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)为准，不宣称 master Jacobian 已全传递。大面寿命和 typed 迁移依[接口规格](../normalize/contracts.md)。

## 验证入口

[阶段一管线测试](../../../eng/tests/pipeline/test_stage_normalize.py)检查生产入口；独立校准检验应覆盖两种暗场约定、非单位 K、平场边界、标度声明、负信号及共享母版误差。并发等价需以实际 budget 配置与输出读数核对，本页不提供未经运行的通过结论。
