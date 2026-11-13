# 01｜Normalize 的逐模块代码迁移：校准—天测—PSF—光度

本文只规定“旧源码→V2单一正式接口”的施工、性能和最小独立回归；算法数学以各 Process 与五篇科学 Foundation 为唯一正本。统一测试及性能判定分别见 [07](07-VERIFICATION.md)、[06](06-PERFORMANCE.md)。**不可把旧 phase1_session 的 calibration-only 流程冒充 main 正式 Normalize。**

## N-00 正式生产链冻结

main 调用：`lib/infrastructure/cli/commands.cpp::run_phase1_block`（约2243行）→ `lib/infrastructure/scheduler/src/module_adapters.cpp::P1NodeModule`，实际节点为 `p1_op_calibrate` → `p1_op_cosmetic` → `p1_op_wcs` → `p1_op_star_psf_impl` → `p1_op_photometry` → `p1_op_noise` → `p1_op_drizzle` → `p1_op_writer`。生产期的 `p1_wcs.json`、`p1_phot.json`、`p1_sources.json`、`p1_stack.json` 是旧工作文件载体，不是必须复刻的科学接口。V2应使用已有 typed Process 和 Ref/ArrayRef + Storage/Pipe 连结原科学边。

每个模块提交的迁移清单要写：main真实被调用符号及文件→V2唯一公开操作和实际实现文件→保留的数值内核/移除的胶水→不能直接移植的科学原因→最小oracle→真实M42对照→对象RSS与CPU画像→不可误用的产品身份。具体编号如下。

## N-01 observation.calibrate

**main优先候选**：`lib/algorithms/calibration/src/{calibrator,dark_optimizer,master_generator}.cpp`、`lib/algorithms/calibration/cpp/cosmetic_corrector.cpp`。**V2既有接收者**：`src/observation/{calibration_kernel,calibration_plan,calibration_signal,calibrate,calibration_process,stream_map}.cpp`。

1. 对照旧函数的bias/dark是否含bias、曝光/温度比、flat归一、读出BZERO、DQ/坏点/饱和判据；已由V2 kernel正确处理的不再写第二套，缺失时抽移旧C++纯算子。
2. 建立母版输入身份及匹配规则：T2/T3、日期/曝光/滤镜/温度、真正二维 shape 和读出方向；不允许“像素总数一致就匹配”。shared bias/dark/flat作为同一个随机根，不能复制成独立帧噪声。
3. 将校准+cosmetic改为按原始FITS块的单一只读输入和有界输出：master只读共享，非有限/坏flat/饱和状态保留；不常驻raw/calibrated/cleaned/normalized四套全图。
4. 真实校准能力与语义只经V2 Storage公开；旧AIO正确的FITS/XISF解析或tile缓存可移入唯一reader，**禁止两个永续reader分别维护**。
5. 最小反例：恒定flat、含bias dark、非零dark温度/曝光、flat≤0、uint16 BZERO、错轴master、共同master误差、输入中断、坏块、T2实测3 master。性能记录读取字节/解码次数/块峰值/像元每秒/额外整图拷贝数；不因改接口退化为逐像元随机磁盘I/O。

## N-02 observation.regroup

**main参照**：旧校准与Drizzle间真实像元排列/平面view；**V2** `src/observation/{regroup,regroup_process}.cpp`。新 Regroup/G 是V2显式算子，不必强行找到旧“等价类”。

- 区分物理收光bin与逻辑display flip/transpose；真实WCS仍绑原探测器坐标。按科学原式处理G与空间光度Dα的非交换性。
- 用稀疏索引表示sum/mean、反转、翻转、缺孔、覆盖变化；协方差经G C Gᵀ，不能挪用独立噪声假设。
- 性能：零拷贝view优先、稀疏G只存必要映射、无m×n稠密化；验证合成解析脉冲/常值与坏像元、非法shape、取消、按块chunk。

## N-03 coordinates.wcs

**main候选**：`lib/algorithms/drizzle/healpix_drizzle/wcs_sip.cpp`、IPV `ipv_wcs/ipv_sip/ipv_itertrans.cpp`、`lib/algorithms/photometry/cpp/src/wcs_transform.cpp`；**V2** `src/coordinates/{tan_sip,wcs,chart,footprint_provider,wcs_process}.cpp`。

- 三条旧 WCS数学路径只能归并为**一个**V2坐标提供器，统一 PixelCenter0 / PixelEdge0 / FITS 1基CRPIX、CD手性、SIP正逆、局部Jacobian与area、RAwrap/极点。
- main #92真实六帧 “Gaia 0/2309” 根因是AP/BP近似反算结果**未先迭代精化就被精度门拒绝**；旧修复用同一个迭代逆解算子后入帧约1083–1090星。V2必须用独立Astropy/WCSLIB重复这个负例（不是单纯同核roundtrip）。
- 多帧的细尺度/Jacobian服务于 [02](02-GEOMETRY.md) 自动Nside，必须使用**最终Gaia解而非未经证明的PinPoint头**。每帧WCS身份改变只失效geometry及下游，不触发重新校准。
- 性能：batch sky→pixel、重复域缓存和空间分块；测单点/批量吞吐，确认不存在锁串行化/每pixel重复构造SIP多项式表。验证镜像、强畸变、WCS外域、非整数CRPIX与留出星。

## N-04 catalogue.access / sources.astrometry

**main候选**：`lib/algorithms/platesolve/cpp/ipv/src/ipv_{solver,triangle,kvector,ransac,robust_refine,sip,select}.cpp`；**V2** `src/catalogue/{index,provider,columns,sky_geometry}.cpp` + `src/sources/{astrometry_detection,astrometry_match,astrometry_fit,astrometry_wcs}.cpp`。

1. 把旧生产IPV实际调用的提点、索引、候选、RANSAC、SIP拟合、留出验证分离；**有可复用且正确的kernel优先抽取**，新算法需要有独立误差/性能证据才能替代，不因旧算法C ABI而重写数学。
2. 本地Gaia索引/扇区缓存做一次只读provider并跨帧复用。列、source_id、天球历元、自行和未知parallax/RV显式。禁止每帧重复装载数十GiB星表。
3. solver_detections与science_sources分别建模；30帧原FITS缺CD时要实际盲解，不是仅凭缺头剔除。记录候选数、匹配数、留出残差、计算work和失败原因。
4. 并行约束：候选批次可使用唯一CPU Task租约，搜索接受/最终拟合顺序可重复；不能各IPV调用自建独立长寿命线程池。
5. 对照旧#92六帧、M42 T2/M5三帧、失配/旋转/强SIP解析造例；出站WCS/参数covariance及身份只能由sources.astrometry生产一次。

## N-05 sources.psf / sources.measure

**main候选**：`lib/algorithms/star_detection/src/{sdet_image,nls_lm}.cpp`、`lib/algorithms/psf/src/{pixel_integrated_psf,spatial_psf,psf_information}.cpp`；**V2** `src/sources/{psf,psf_fit,psf_model,measure,psf_process,measure_process}.cpp`。

- 旧 pixel-integrated Gaussian/Moffat 等有效模板、检测、L-M和空间变化参数场优先移入V2；已有V2连续空间模型比旧核严格时保留改进，不能丢掉旧有效的数值初始化、窗口复用与稳健迭代。
- 先检查 main 真实调用的 `p1_op_star_psf_impl` 对 `n_fit_limit`和已停用 precise path 的判定，不能把“源码有无限星拟合”误报成生产必定使用。
- M42强结构星云需局部背景(常数→倾斜→有证据时更高阶)、邻星混叠、饱和掩膜；保证未参与拟合的Gaia星位置/FWHM/椭圆率/孔径通量及背景残差。高SNR巨大负亮星通量、无收敛的联合测光不能用于标度；合法低SNR负线性测量不可一刀切截零。
- 性能：按组预取窗口、复用PSF求积模板/设计矩阵/背景基工作区，LM line-search/有限差分真实工作计费，局部组间并行，避免全幅搜索对每一星重复。
- 固定反例：旧S1只测2星、其中一颗约−2.99e6和非收敛，解析Gaussian与双星混叠/空间变化、真实低背景与核心强背景分层留出。不能以PSF header可读宣称拟合准确。

## N-06 response.integrate / photometry.fit / photometry.apply

**main候选**：`lib/algorithms/photometry/cpp/src/{spectrum_integrator,frame_photometry_fit,spatial_gain,image_corrector,star_matcher,wcs_transform}.cpp`，真实 `p1_op_photometry`→`p1_op_drizzle`。**V2** `src/response/{spectral,integrate_process}.cpp`、`src/photometry/{joint_fit,spatial_basis,apply_scale,photometry_fit_process,photometry_apply_process}.cpp`。

- 保留旧实际谱积分、Gaia XP匹配、空间增益拟合与低秩运算，但按V2数值/真实响应重验。旧M42曾错误将 Antlia V Pro Series B 配作 Baader R 且 Q≡1，49帧实测残差从历史0.447mag订正到0.045mag量级，属于必须重放的**错误输入配置+模型适用域反例**，不能把旧修后值当本次PASS。
- 区分source fitting与同数据共享Gaia latent、θ covariance；每帧/每空间零点与夜间透明度/局部背景有清晰可辨识域。真实1.2766跨夜通量比是现象，不能预设主flat成因。
- **应用一次**：Dα在原像元上实际施加，signal×α，variance×α²，ivar÷α²，PSF/response/WCS与标度身份绑定，同一次Drizzle消费已施加的观测；重复application_id拒绝。参考旧 `实验/issue9-p0-b1-production-fix/` 真实A/B与严格BUNIT。
- 性能：光谱积分按 response/SED hash 缓存，多个星共享响应基积分；光度空间系数作为块元数据供Drizzle使用，在科学上允许时融合像元循环以减少额外整图及内存带宽，禁止隐式改变几何次序。
- 反例：解析积分/XP窗口、无QE条件、有效响应不一致的T2/T3、常量与梯度scale、同一flat shared root、重复施加、两通带强制标度和非收敛拟合。

## N-07 information.reference / information.scene 的挂接

旧实际 `lib/infrastructure/scheduler/src/normalize_precision.cpp::stage_frame_precision` 调用 `lib/algorithms/noise_snr/cpp/src/sparse_reference.cpp`，与Drizzle同一算子。迁入 `src/information/` 和 `src/uncertainty/` 但详细任务归 [02](02-GEOMETRY.md)。
- V_F(参考点源通量)、V_B(实际天空面亮度)、V_ctrl(天空控制)是不同统计量；旧Fisher-to-scene短路必须替换。
- PSF/information正向观测模型和噪声估计依赖同一实际Geometry R与校准随机根；未知物理gain/read/QE或近似协方差不得写0/1凑成功。
- 复用旧稀疏GLS/参考信息算法与可用高性能数值核，但V2严格区间、秩和共享根若改变数值须由独立oracle裁决。

## N-08 本组完成判据

按7个生产边，先小数值oracle再真实单帧/2帧ROI，最后完整4096²；每个Producer必须有实际V2入口、唯一调用、有效对象/单位与无静默失败，配对性能真实测量。**不能在自动高阶Drizzle尚缺失时用低阶三帧完成验收。** GPU不是本阶段目标，Windows/Linux均验证应用后没有使用不同数学实现。
