# Normalize 噪声与参考精度

noise 库拥有平面条件噪声和 Drizzle 后球面参考 GLS 的数学实现。三种源 SNR/帧摘要/参考控制点的区别，以及点源精度到面亮度精度的传递，由[噪声正本](../../science/noise_snr/NOISE_SNR.md)统一定义；接口表达依 [Normalize 数据合同](../../engineering/contracts/NORMALIZE_DATA.md)。

## 生产路径与消费者

[adapter](../../../lib/infrastructure/scheduler/src/module_adapters.cpp) 的 `p1_op_noise` 是 `acsd.phase1.noise-snr` 节点，实际读取 cleaned、sources、photprov，产出平面噪声诊断和兼容帧级 SNR。`fluxes` descriptor 边用于合成 IR，在生产没有对应读取点。

Drizzle 节点获取同次球面几何后，经 [normalize_precision.cpp](../../../lib/infrastructure/scheduler/src/normalize_precision.cpp) 的 `stage_frame_precision` 调用 `complete_spherical_reference`；没有 noise←Drizzle 调度环。球面模型/controls 随 staging 交 writer 发布，不能把 `p1_snr.json` 改名就视作 canonical 精度。

根 CMake 已包含 `acsd_phase1_noise`；[sparse_reference.cpp](../../../lib/algorithms/noise_snr/cpp/src/sparse_reference.cpp)已有球面计算。构建接入、实际调用、模型完整和独立科学支持分别判断。

## 接口与数据

| 入口/类型 | 作用及所有权 |
|---|---|
| [snr_estimator.h](../../../lib/algorithms/noise_snr/cpp/include/snr_estimator.h) 的 `snr_noise_model_v1` / `_f64` / `_fill` / `_free` | patch 背景模型与平面方差求值；model 经配对 free 释放，填充数组由调用方提供 |
| [snr_frame_science.h](../../../lib/algorithms/noise_snr/wrapper_phase1/snr_frame_science.h) 的 `compute_snr_frame_science` | 兼容逐源行到帧级诊断/参考记录，不自动成为球面 controls |
| [sparse_reference.h](../../../lib/algorithms/noise_snr/include/acsd/sparse_reference.h) 的 `complete_spherical_reference(windows,cfg)` | 同步读取局部平面窗口与实际 R，返回 precision、局部模型载荷和逐点原因 |
| `summarize_frame_reference(controls)` | 对可用 controls 生成独立帧摘要；不重缩放 controls |
| [normalize_data.h](../../../lib/include/acsd/normalize_data.h) | reference、模型身份、availability/accuracy、control 和 summary 类型 |

`PlanarPatchWindow` 保存 detector 零基中心、源像元 row-major indices、P、条件方差、源 Ω/drop A、NESTED 叶索引及稀疏 R；几何为 sr、R 为 1/sr。球面 h/C/Z 必须来自同一观测与索引顺序。控制点 sky 是 ICRS degree；F_ref 为声明积分单位，V_F 为其平方，SNR 无量纲，稀疏计算 FP64。leaf 0 合法，不是未知哨兵。

`SphericalCompletionConfig` 分别承载 reference、背景 nuisance、原生 gain、实际 scale、母版状态、秩容差及矩阵预算。gain 单位 e-/ADU；不能将光度缩放后数组直接除原生 gain 而漏标度幂次。源 Poisson、天空/读噪、母版和共同参数须分项，已有经验总 RMS 不再重复加读噪。

## 状态、配置与实现边界

`snr.reference` 的 canonical 参考路径与兼容 `reference_flux_adu`、参考星等诊断分名。现有 Catalogue 的 `snr_psf` proxy、测光残差派生 SNR、规整 `sparse_snr_layer` 不能仅凭数值可读就升级 canonical。正式产品载体为 writer 发布的 `frame_science.json`。

合法信号可以伴随 precision unavailable；损坏、非法 shape、非有限计算和不可辨识分别表达。available 的 accuracy/omitted_terms 如实保留，不能把缺项当零。I_F=SNR²/F_ref² 属点源通量精度，不能直接命名面亮度权重。Runtime、SED、空间 PSF、局部模型及 reader 的具体限制以[差异地图](../../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)为单一登记。

大面由调用方/Runtime 预算管理，局部窗口和模型不长期复制全帧。线程和取消由 adapter/宿主提供；本同步 completion 无独立资源预算或文件发布职责。

当前常规 completion 使用列主元 Householder QR 行空间投影，背景有效秩与通量可辨识性分别求；大工作集仍有 Jacobi 分支。scheduler 已调用 PSF 库的像元积分，factor writer 已使用 v2。这些实现进展不等于空间 PSF、秩阈值、近共线误差与独立 reader 已完整，避免引用迭代之前的实现描述。

## 验证入口

[生产 GLS 单元入口](../../../eng/tests/unit/sparse_reference_gls_test.cpp)、[噪声合成入口](../../../eng/tests/synthetic/test_p2_absolute_snr.py)、[sidecar 合同入口](../../../eng/tests/conformance/normalize_frame_science_schema_test.py)和[已保存局部验证记录](../../../实验/normalize-precision-p5/evidence/ROUND1_VERIFY.log)分别覆盖不同问题。需区分背景已知/共同拟合、异方差、秩亏、非单位 scale、缺 gain、leaf 0、PSF 截断及 Monte Carlo 的实际条件；已有日志不支持全 CLI、真实 SED/空间 PSF或完整误差链已验证。
