# 阶段三：单信号 HiPS → 平面 WCS FITS

> 科学正本；上游 docs/ACSD_DESIGN.md §6；坐标与产品模型在 docs/science/unified/DATA_SEMANTICS.md、球面采样在 docs/science/drizzle/DRIZZLE.md、核算法在 docs/science/resample/RESAMPLE.md。重写日期 2026-10-14。

## 1. 固定产品职责

阶段三只负责将阶段二生成的**单科学信号子产品 HiPS** 按用户指定的目标天区、投影、图像尺寸、像元角尺度和方向输出普通二维浮点 FITS。这是提供给 PixInsight 等后期处理软件的图像，不是新的 CCD 科学标定、背景求解或跨设备叠加阶段。

HiPS 标准的多分辨率 Norder tile 是**同一个信号数据通道**的多尺度表示；不是 variance/ivar 等多个图像层。阶段三不要求也不应假定阶段二输出了 variance/ivar/SNR/support 图像层。内部可诊断重采样对 PSF/噪声结构的影响，但从最终单信号图不能反演真实输出协方差。

## 2. 层级与角尺度

Tile 宽 W=512、tile order K 对应细层 leaf nside=W·2^K。HEALPix 等立体角给出其平均等面积线尺度：
\[
 s_K=\sqrt{\pi/3}/(W\,2^K) \quad [{\rm rad}].
\]
给出请求输出像元尺度 s_out，可根据实际存在层级与目标品质选择
\[
 K_{\rm sel}=\mathrm{clamp}\!\left(
 \left\lceil \log_2\frac{\sqrt{\pi/3}}{W\,s_{\rm out}}\right\rceil,
 0,K_{\max}\right).
\]
这是以等面积**平均尺度**作的层级启发式，不是每个 HEALPix 单元两个方向最大采样间隔的误差上界。接近极区/face 边界、视场畸变显著时，局部采样间距和输出 WCS Jacobian 会改变有效精度。避免从过粗层无意义放大并称为获得更多细节。

## 3. WCS 正向/逆向映射

对平面输出索引 (i,j)（内部从 0 起），FITS header 中 CRPIX 习惯为从 1 起的连续像素中心坐标。经投影的反向映射求 RA/Dec (ICRS)，再经 HEALPix NESTED 找输入叶/tile/局部 FITS 像素。RA wrap、极区、面缝、投影有效域与像元手性必须在独立标准 WCS/HEALPix 库上对拍。

使用者可选择可实现的 TAN、SIN、CAR、AIT 等投影，是否可用取决于生产已实现的投影注册；未实现应明确告知，而不是假定非 TAN 投影数学上均不成立。任意特定视场/Dec 限制若只是旧实现能力边界，须报告为实现限制，不能写成天球投影理论本身的奇点。

## 4. 重采样与信号含义

输入 leaf 存的是固定光度标度上的球面面亮度 S(ω)，而非每叶直接可交换的点源通量。

- nearest：一个叶值；结构离散、快速，不引入额外混合核，但可产生阶梯/aliasing。
- local bilinear：多个叶值按局部坐标凸组合，常量场保留；这会进一步混合相邻信号和噪声，输出像素统计不再独立。四象限使用近似局部平面几何时不自动实现精确球面面积积分。
- area-overlap：对输出像元真实球面足迹与 HEALPix leaf 求面积交叠，按**实际有效覆盖面积**归一形成目标像元平均面亮度；如几何求积近似，须明示其误差。用于保留扩展源光度时，应与其他核在一致真值世界对照。

若输入少量邻域采样值 NaN，必须有明确的掩膜/重归一政策；不得把 NaN 混为零，也不得在没有有效贡献时生成有限伪信号。部分覆盖的输出像元可按有效贡献平均，但要明确它不是全部目标面积的精确平均。来自上游 HiPS 多尺度层级的粗化和平面重采样两次作用需分离测量。

## 5. 二次重采样造成的细节和噪声变化

阶段一 Drizzle 已经把原始 CCD 像元映射到 HEALPix；阶段三再重采样，会改变有效重建算子。若输入是像元平均值，输出的线性变换 y=R_export S，不论核如何显示平滑，若有原始协方差则精确公式是 C_y=R_export C_S R_exportᵀ。**最终只有单信号 HiPS，并不代表这些项等于零或可以从画面估计**。

对科学与视觉质量实验应追踪：常值场、解析梯度、星点孔径通量、FWHM/椭圆率、亮星周围环状结构、低空间频率星云、噪声功率谱、RA/Dec 天区边缘。比较直接采样同一已知天空真值到目标 FITS 与完整“CCD→HiPS→FITS”输出，分解误差来自哪一次转换。

对输出 FITS 不生成缺乏来源的 ivar、point_information、variance 图像扩展；若需要它们，应重新讨论产品范围，而不能偷偷把第三阶段改成多层科学数据库。

## 6. 输出文件与元数据

输出为浮点二维 FITS 的 PRIMARY 或受控 IMAGE HDU，带实际合法 WCS、像元尺寸、CRVAL/CRPIX/CD/PC 等具名变换及 BUNIT。BUNIT 必须沿用阶段二信号的真实标度，不能把 Gaia XP 光度积分量无条件写回 ADU。可在 FITS 头部/运行摘要记录输入 HiPS 标识、参考响应或混合通带解释、使用的层序与插值核、输入科学有效域和未测不确定性。

像元可为负；无有效数据写 NaN，不能用零假装黑色背景。输出的普通 FITS 不要求后期软件理解 ACSD 私有二进制数据结构。

## 7. 实验和代码对应

实际代码 lib/algorithms/resample/p3_resample.cpp、lib/phase3_session/、相关 I/O；阶段二产物证据由生产出图测试验证。独立核对 FITS WCS：Greisen & Calabretta 2002、Calabretta & Greisen 2002；HEALPix：Górski 2005；IVOA HiPS Recommendation；CFITSIO 与独立 astropy.wcs/astropy-healpix 仅作为外部对照。