# Normalize 球面 Drizzle

Drizzle 将本帧已解 WCS/SIP 下的已施加光度线性面映射到 HEALPix NESTED。面积、归一和协方差定义属于 [Drizzle 正本](../../science/drizzle/DRIZZLE.md)；球面参考精度完成边界属于 [Normalize 正本](../../science/normalize/NORMALIZE_SCIENCE.md)。

## 实际入口与上下游

[module_adapters.cpp](../../../lib/infrastructure/scheduler/src/module_adapters.cpp) 的 `p1_op_drizzle` 对应 `acsd.phase1.drizzle`。descriptor 消费 calibrated、wcs、photprov、sources、snr，输出 stacked。实际逐帧选面须核对 photprov 与 photoapplied；端口名 calibrated 不表示允许重复缩放或忽略 cleaned 来源。

生产调用 `hp_drizzle_run_phase1_hips`，引擎和 sphere sink 通过统一 AIO 写 HiPS。相同几何贡献捕获后，由 `stage_frame_precision → complete_spherical_reference` 完成球面参考模型，局部载荷及 sidecar staging 交 writer。此调用存在，不能继续写成 descriptor 未接或 canonical producer 不存在；具体限制集中见[实施地图](../../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)。

## 公开接口与几何载荷

[hp_drizzle_api.h](../../../lib/algorithms/drizzle/healpix_drizzle/hp_drizzle_api.h)公开帧接口 `hp_drizzle_run_phase1_hips` 及 `_begin` 异步 job 形态，并保留 `hp_drizzle_run`、`hp_drizzle_run_hips`、文件/反向接口。不同入口的能力和返回不能互相推断。

| 输入/输出 | 数值合同 |
|---|---|
| PipelineFrame data/header | 行主序图像及本帧完整 WCS/SIP；FITS CRPIX 一基，内部像元中心零基 |
| nside、ordering | 2 的幂和 NESTED；叶阶 log2(nside)，512 tile 阶比叶阶小 9 |
| pixfrac、Ω、A、a | 无量纲收缩及未收缩/收缩/交叠 sr；A 不能无条件用 pixfrac²Ω 代替 |
| 信号算子 R | 作用于积分像元的系数，1/sr；归一分母须与同次正式叶完整样本一致 |
| signal | 实际积分线性单位/sr；负值和零合法，无数据 NaN |
| variance/ivar 辅助输入输出 | 只在真实模型可用时参与；平方传播与 support 分名 |
| precision staging | 控制点、h/C/Z/R、支持和具名状态，算法归 noise 库而非 scheduler 重写 |

[drizzle_engine.cpp](../../../lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp)承担交叠/累加，[astro_sphere_sink.cpp](../../../lib/algorithms/drizzle/healpix_drizzle/astro_sphere_sink.cpp)适配写出。分配通量守恒、常值保持、支持面积、目标孔径通量和真实曲边误差是不同检验，不能以单个闭合残差替代全部。

## 配置、寿命与错误

nside/目标尺度、pixfrac、ordering、精度和存储请求来自配置与实际解析；不在卡片另立默认值。生产可按帧、源行和 tile 组织并行，预算由 Runtime 提供；捕获窗口、tile 累加器及异步写 job 在所有消费者/wait 完成前保持有效。不同 worker 数归约序可变，不宣称无条件逐位一致。

返回 0 表示调用成功，非零读取 result 的 error_msg 并上行；不能直接把各入口局部整数当 CLI 退出码。无解 WCS 不生成伪球面，非法参数、AIO 失败与 precision unavailable 分别处理。开发快照只观察，不供 completion 或 writer 补正式输入。

## 验证入口

[守恒映射合成入口](../../../eng/tests/synthetic/test_p3_conservative_map.py)与[实际阶段一入口](../../../eng/tests/pipeline/test_stage_normalize.py)分别检查算子和 production。核对常值、drop 分割、真实 HEALPix 曲边、源/叶尺度比、12 面/极区、NaN、孔径量与相邻协方差；precision 另核对完整 R 与模型条件。本页不据接口或已有局部日志宣称全链已完成。
