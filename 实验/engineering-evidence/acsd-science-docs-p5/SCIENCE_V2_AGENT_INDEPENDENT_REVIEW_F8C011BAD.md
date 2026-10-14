# Science V2 第一轮实施：前台独立审查与裁决（2026-10-14）

审查对象：942c978e875bf7673ebc30a89fc0b59738092e26 → f8c011bad3001fd9dd3ac993dc3ae945d3fabb4e（GitHub compare 确认 7 提交）。方法：直接读取远端源码、Python 测试、Agent 报告与已提交 JSON；**本轮没有连接用户计算服务器重新编译或重新读取大型 FITS**。前台科学判定与 Agent 的真实运行报告分开；本文件不设置流程门禁、不建立 PR、不改 main。

## 1. 首批工程改动可保留，但不能宣告全链科学闭合

- 现有 CMake 由单一 ACSD_DEV_MODE 派生诊断编译入口；原有 probe.h/probe.cpp 和 AIO 被复用，OFF 时详细诊断 TU 不参与编译。
- coverage.cpp 的滤镜字符串跨帧拒绝已经删除；阶段二写盘正式 flags 为 SIGNAL，covered_area 作为归一内部分母仍保留。
- Agent 真实报告称 2 帧 Normalize ON/OFF 对照科学 tile 1496 项逐位相同；串行独立输出目录纠正了并发同目录运行导致的假 checksum 失败。未独立复跑；仓内 run/A/onoff_compare.json 实际是 **D02 改前/改后 signal 对比（188 项）**，而非上述 1496 项的机器可读文件差异清单，应分别保存。
- Agent 的真实 E2E 证明选定 H-alpha 输入能依次生成 HiPS 和普通 FITS，量纲级别自洽；不能由此证明绝对光度、完整 Drizzle 曲边、所有设备与 Gaia XP 的物理归一。
- 实跑科学叠加路径是 per_sample_ivar，snr_path_effective=not_used。阶段一有 frame SNR header，但真正稀疏 SNR producer → P2 consumer 尚未闭合。已有 Python R3 测试不直接调用生产 C++，只能证明结构性问题，不等于 R3 已修复。
- Agent 事实报告所标“最终 SHA=36bcaba8d”落后于实际分支 f8c011bad；后续报告须取真实 HEAD。

## 2. 需先纠正的独立发现（均有具体源码锚）

### F01 Phase1 仍有跨通带组级拒绝
module_adapters.cpp 在 PASSBAND-IDENTITY-GATE-01 位置对同组 frame 的 filter_key/曲线不同执行 mark_frame_fail(PHOT_PASSBAND_IDENTITY_INCONSISTENT)。虽然 P2 coverage 拒绝已删除，但用户要求不因设备/滤镜不同而排除正常输入。**裁决**：每帧必须以自己的 filter/QE/photometric response 拟合；允许内部按响应分模型组共享参数，但外部输入组可以混合，不能对合法帧标科学非法。禁止只删检查却套用第一帧的错误响应。

### F02 P2 debug probe 保存 flux_sum 而非 signal
module_adapters.cpp 计算 flux_out[local]=sig_v[i]×cov，cov_out=covered_area；p2_devdiag_signal_tile 却把 write_flux 当成已归一的 surface brightness signal 标为 ADU/sr。AIO 的正式 signal 随后由 flux_sum/covered_area 求得。这是**诊断科学量错误标注**，不能用于真值对拍。**裁决**：分别命名 flux_sum、covered_area、signal；mosaic.integration.signal_tile 必须真的保存面亮度（可直接在数值运算前保存 sig_v 的正确坐标/位序版本，或与最终 AIO signal 独立对拍），保留 NaN/负值/有限零语义。不能用调试快照证明已有物理 signal 正确。

### F03 一个 R3 相关噪声世界不满足协方差正定
eng/tests/synthetic/test_r3_surface_precision.py 的 corr_eye(n,rho) 在 n=4 是主对角 1、相邻 offdiag=rho、其它 offdiag=0 的三对角 Toeplitz 矩阵。其最小特征值 λmin=1−2rho cos(π/5)。在 rho=0.7 时 λmin≈−0.132624，非半正定，不能是合法噪声协方差。Agent 报告 T(rho=0.7)≈0.200087 **不能**作为真实相关噪声模型证据。本审查已直接更新 science/noise_snr/NOISE_SNR.md §4.2。**裁决**：改为正定模型如同族 rho=0.3，或 AR(1) 完整核 Kij=rho^|i−j| 且 |rho|<1；重跑 7 条并保存 λmin、条件数和新读数。原阳性世界所用 n=4 三对角 rho=0.5 的 λmin≈0.190983 为正，不能因此把全部结果判无效。w_B MC 的 +1.88% 是有限样本方差估计偏差，不是解析最优性的固有损失。增加真正调用 C++ 生产权重核的独立负例，避免测试只验证另一个 Python 公式。

### F04 mixed 通带元数据的空首帧缺陷
P2 writer 将空字符串 first.empty() 同时用作“首帧尚未赋值”哨兵。当输入 ["", "R"] 时第二帧重新设 first="R"，same 可能仍为 true，最终错误地声明唯一 R 通带。**裁决**：改用独立 first_set 布尔值，保留 unknown 的真实身份；同时 distinct_passband_count 与 frame_count 分开。当前 mixed:<counts.size()>_passbands 的 counts.size() 指**通带种类数**，注释却误称 n_frames。

### F05 快照 I/O 当前同步且在全局锁内
lib/infrastructure/observability/probes/src/dev_diagnostics.cpp 的 snapshot_image_f32/f64/text 取得全局 mutex 后执行 FITS/JSON 写盘与 sha256，mosaic tile 输出循环同步调用探针；因此 ON+snapshot 会阻塞科学线程，并不能仅因 byte budget 存在就声称低干扰。有的 sidecar write 忽略错误，主文件写成仍记 WRITTEN。**裁决**：短锁确定性选择/预算预留；带生命周期/内存预算的 bounded copy 移交独立低优先写入队列；满额丢弃诊断并计数、不阻塞计算；SHA/sidecar 完整后才记 WRITTEN。补线程回收、取消、诊断盘失败、单进程多 run、ON/OFF 正式产物对照。此项不否定无快照 ON/OFF 的逐位测量。

### F06 P1 reference_snr probe 可能夸大统计真实性
p1_devdiag_snr_controls 对 frame_obj 写诊断、静态 meta 声称完整条件参考点源方差。但 p1_noise 实际可给 snr_caliber=upper_bound_no_gain，说明缺源散粒噪声项；快照不能无条件标为绝对 V_F。**裁决**：meta 依据实际 frame 内 gain、caliber、F_ref 和有效 PSF/噪声状态，分别保存 frame SNR 与真正稀疏 SNR catalogue 记录；不能仅因快照体积大就宣称稀疏控制点生产完成。

### F07 阶段二单信号与旧多层 metadata 要对齐
虽然 products=["signal"] 已真实写出，p2_final 中 variance_bunit/ivar_bunit 有时仍按内部 uncertainty_available 非空。应严格区分“内部拥有不确定度估计”与“发布了 variance/ivar 子图”，下游不能仅根据非空单位字段假设额外图像存在。

## 3. 四项前台科学裁决

**C1 R3 生产方案：允许推进，但先建立真实 P1→P2 稀疏数据链、在生产 C++ 实际调用中实现 T 桥梁的可选科学算法臂，并与现有 per_sample_ivar 同天区真值比较。** 必须有同一通量标度的 F_ref,k、SNR_ref、有效 PSF、源/HEALPix 面积、重采样相关核 K 和背景 nuisance 模型。权重 w_B=(SNR_ref/F_ref)^2/T，只能在 T 可辨识或具名近似时使用，禁止默默 T=1。原 ivar 可做独立校准/基线与模型未闭合时的成像 fallback，不因一个 Python 反例而马上强制默认 sparse。用户确立的稀疏重建核心目标必须真实落地，不能永远仅写文档。不同 PSF 或通带下，单 leaf 观测是否对应同一科学被估量，也需要结合有效 PSF/光谱系统差检验。

**C2 gain 缺失：沿用现有配置 snr.gain_e_per_adu（先核验 schema 和真正消费链），支持有来源的头信息/设备参数与独立 PTC/成对 flat 估计；不能由单帧无真值图像虚构电子增益。** 不能给未知 gain 假定 g=1。保留 gain 未知的合法合并，但把源泊松项未纳入的 SNR 明确标成背景限制下的条件上界；有可用真实经验像元噪声可单独定义为经验方差及适用域，不冒充绝对 SNR/严格 5sigma 深度。向最终运行摘要传递条件口径。

**C3 缺 WCS：M42 T2T3 30 帧与 GalaxyCenter T4 1 帧均不应被猜坐标补齐，也不因滤镜型号排除。** 可使用 OBJCTRA/OBJCTDEC 仅作 plate-solve 初始指向，在独立外部星表或重叠已解帧上测量解的残差和方向/尺度，生成独立 sidecar，保留原始 FITS。不通过真实天球配准的帧标为 unsolved，暂不投入需要 WCS 的球面映射；工作集内 31 帧分组汇总成功/失败事实。

**C4 ACSD_*_FAULT：迁入专用测试 harness/build，不要用普通 ACSD_DEV_MODE=ON 环境变量偷偷改变科学数值。** 故障注入用于独立负例，发布二进制与正常开发二进制的被动科学路径均不受外部污染；历史反例代码和证据保留。

## 4. 下一轮建议实施顺序（不是流程门禁）

1. 修正 F02/F03/F04/F06/F07，澄清 F01 P1 组内混合响应的逐帧处理；补 F05 低干扰 I/O，分别复跑设备/通带与真实快照；ON/OFF Normalize 1496 项的机器 diff JSON 另存。
2. 查清 HioSnrControlPoint.snr_psf 与 frame_snr 的真实定义，生成 P1 canonical sparse SNR 控制数据（Catalogue/唯一 sidecar + F_ref、σ_F、PSF、caliber、支持域），实现 P2 **真正读入**和局部精度插值。不可直接将 (A−B)/MAD 改名为 F_ref/sigma_F。
3. 真实 C++ P2 接入 T 精度转换与 w_B，建立 per_sample_ivar 基线/影子读数/可选科学模式的同场独立对照；报告背景支配、Poisson 支配、不同 PSF 和像元面积、已知/未知 K/gain 的偏差与方差。
4. 真实混滤镜、混设备和 mixed-order target-order E2E；P3 曲边 oracle 和 P5 扩展源低频；HST 前向+真实 FITS 图像交给用户评估。该阶段尚未进行，不能称“通过”。

用户不要求人为审批关卡，但模型错误、无效协方差和明显错单位的快照必须实际纠正。主分支不动、不建 PR、不上传 /testdata 大 FITS。下一轮 Agent 任务说明在 SCIENCE_V2_AGENT_TASK_PACKAGE.md 继续更新。