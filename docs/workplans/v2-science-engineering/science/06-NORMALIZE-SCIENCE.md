# B2｜Normalize 科学链：校准、天测、PSF、源测量、Gaia联合光度与逐帧模型

## 主线输入/输出

原始 FITS/像元 `d_i`、偏置/dark/flat/mask、实际曝光与时空坐标、响应/T/QE/大气条件、Gaia DR3 历元/XP 和可用 WCS；产出 **单帧已标度面亮度 + 真实支持 + PSF/response/噪声及误差的可恢复模型**。每一步量纲必须由物理观测模型推导，不允许凭相邻模块类型相同便假定估计量相同。

## N0 校准与原生物理量（Process observation.calibrate/regroup）
- 从 `d=bias+(e+r)/gain` 及 `x=(d-b-t dark)/flat` 推出校准、噪声 Jacobian、共享母版跨帧根、坏区/饱和/非线性处理。验证 Bayer photosite、bin/ROI/flip/tranpose 的几何及 `G C Gᵀ`，含孤立坏样本与负校准结果。
- 区分线性重组与非线性修复；不得凭宽松元数据把未知PRF当矩形均匀。原始ADU输入、输出线性标度和dark单位必须显式转换。
- 基准例：两曝光共享一个母版，独立数值手算协方差非对角与校准后区间；再做有噪声重复实验。

## N1 绝对坐标与星表（coordinates.wcs, catalogue.access, sources.astrometry）
- 使用 WCS/FITS/SIP 原论文与 Gaia 星表适用合同，证明 PixelCenter0→FITS1一次 +1、ICRS/TAN变换、CD手性、LONPOLE和正确观测历元；未知DCR/站址/视差不能假定已校正。
- 盲解从真实 detections 开始，必须追踪源提取/亚像素质心误差、quad候选、全源残差、候选/时间预算和错误镜像；若数据不足返回 unsolved，不能拼虚假WCS。已有X01 B组RMS从0.579改善到0.211px，但不能将一次合成测量直接称通用精度。
- **验证分离**：已知精确WCS、注入有误WCS、无WCS盲解；固定星场、不同SNR/天空背景/畸变/光学手性；绝对天球参照而非自身roundtrip。记录估计WCS误差向PSF/测光/Drizzle传播。

## N2 PSF/PRF 与星点测量（sources.psf, sources.measure）
- 从 `P_i=∫pixel PRF×PSF×Jacobian` 与 `d=H F+Zβ+ε` 逐式推导高斯/Moffat/Airy 的像元积分、边界尾损、模板及模型导数。PSF `p(u;θ)` 的积分归一在整体域，不在有限窗口再次归一为总光量。联合混叠组必须同一共享噪声模型。
- 对每个源构造被测的实际窗口/PRF列 `H` 和原生背景密度列 `Z`；求可估通量/背景/位置/PSF形参，检查秩和源相关；error包含模板数值界、参数根盒、随机Cov和未知物理模型。
- 特别定位X01 B组最暗源约10.3%相对误差：分别注入已知位置/WCS、真实PSF、真实背景、无噪声、Poisson/read有噪声、多个噪声seed；比较偏差、方差、95%覆盖与理论低SNR可达极限。**5%单次峰值阈值不能擅自视为所有随机样本必然可达**；如果前提不成立，科学文档证明后修改统计验收命题，保留原失败。

## N3 响应/光度（response.integrate, photometry.fit, photometry.apply）
- 从光子计数积分 `q=∫f_λ T QE E_atm λ/(hc)dλ` 推出 XP basis向量、各滤镜逐帧身份、窄带解析端点分段积分。2nm粗格不能恢复3/5/7nm真实通带/未知细谱线；求积误差、XP分辨率、系统响应形状偏差、Gaia误差分开。
- 允许参数化中心/FWHM的 `top_hat_approx` 但不得冒称实测主通带、OD5、入射角光锥；QE未知须有理想假设或未知标注。
- 建立联合观测 `C_{ks}=a_k exp(b(u_s)^T θ_k) q_{ks} + ε`、共同Gaia潜变量、相同星跨帧参数相关及秩/可辨识；未知口径×gain不可拆。强颜色-位置相关时要返回不可辨识方向，不能人工固定零。
- `α(u)` 在 Native 各实际采样像元及源模板一致应用**一次**；给出明确application身份和可证幂等拒绝。空间标度同卷积/Drizzle一般不可交换；所有后续误差依赖新Native版本。

## N4 与几何/信息的交接
完整WCS/PSF/Native/response/noise→footprint/`Ω,A,a,R`→`V_F/V_B`及几何反馈。只有前置模型完整或具名conditional，才可以发布声称已认证的单帧模型。不能因为signal可以合法发布就把缺失noise自动填0。

## 逐层交付
源论文明细、逐算法伪代码与数学导数、冻结小型原生观测真值、每个故障的实际生产调用路径、独立高精度Oracle、新进程可重开产品和跨界数值误差/模型偏差表。更新旧F01及X01，但不得用旧PASS覆盖本次模型变化。
