# 阶段三：单信号 HiPS → 平面 WCS FITS

本分册依据[最高设计](../../ACSD_DESIGN.md)，定义单信号HiPS的平面WCS导出。输入量见[数据语义](../unified/DATA_SEMANTICS.md)，数值核见[重采样](../resample/RESAMPLE.md)，实现状态见[科学实施差异](../../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md)。

## 1. 固定产品职责

阶段三负责将兼容的**单科学信号HiPS**（典型输入为阶段二马赛克） 按用户指定的目标天区、投影、图像尺寸、像元角尺度和方向输出普通二维浮点 FITS。这是提供给 PixInsight 等后期处理软件的图像，不是新的 CCD 科学标定、背景求解或跨设备叠加阶段。

HiPS 标准的多分辨率 Norder tile 是**同一个信号数据通道**的多尺度表示；不是 variance/ivar 等多个图像层。阶段三不要求也不应假定阶段二输出了 variance/ivar/SNR/support 图像层。内部可诊断重采样对 PSF/噪声结构的影响，但从最终单信号图不能反演真实输出协方差。

## 2. 层级与角尺度

HiPS的层级表示遵循[3]。Tile 宽 W=512、tile order K 对应细层 leaf nside=W·2^K。HEALPix 等立体角给出其平均等面积线尺度：
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

对平面输出索引 (i,j)（内部从 0 起），FITS header 中 CRPIX 按WCS标准[1,2]为从 1 起的连续像素中心坐标。经投影的反向映射求 RA/Dec (ICRS)，再经 HEALPix NESTED 找输入叶/tile/局部 FITS 像素。RA wrap、极区、面缝、投影有效域与像元手性必须在独立标准 WCS/HEALPix 库上对拍。

使用者可选择可实现的 TAN、SIN、CAR、AIT 等投影，是否可用取决于生产已实现的投影注册；未实现应明确告知，而不是假定非 TAN 投影数学上均不成立。任意特定视场/Dec 限制若只是实现能力边界，须报告为实现限制，不能写成天球投影理论本身的奇点。

## 4. 重采样与信号含义

输入 leaf 存的是固定光度标度上的球面面亮度 S(ω)，而非每叶直接可交换的点源通量。

- nearest：一个叶值；结构离散、快速，不引入额外混合核，但可产生阶梯/aliasing。
- local bilinear：多个叶值按局部坐标凸组合，常量场保留；这会进一步混合相邻信号和噪声，输出像素统计不再独立。四象限使用近似局部平面几何时不自动实现精确球面面积积分。
- area-overlap：对输出像元真实球面足迹与 HEALPix leaf 求面积交叠，按**实际有效覆盖面积**归一形成目标像元平均面亮度；如几何求积近似，须明示其误差。用于保留扩展源光度时，应与其他核在一致真值世界对照。

若输入少量邻域采样值 NaN，必须有明确的掩膜/重归一政策；不得把 NaN 混为零，也不得在没有有效贡献时生成有限伪信号。部分覆盖的输出像元可按有效贡献平均，但要明确它不是全部目标面积的精确平均。来自上游 HiPS 多尺度层级的粗化和平面重采样两次作用需分离测量。

## 5. 二次重采样造成的细节和噪声变化

阶段一 Drizzle 已经把原始 CCD 像元映射到 HEALPix；阶段三再重采样，会改变有效重建算子。若输入是像元平均值，输出的线性变换 y=R_export S，不论核如何显示平滑，若有原始协方差则精确公式是 C_y=R_export C_S R_exportᵀ。**最终只有单信号 HiPS，并不代表这些项等于零或可以从画面估计**。

对科学与视觉质量实验应追踪：常值场、解析梯度、星点孔径通量、FWHM/椭圆率、亮星周围环状结构、低空间频率星云、噪声功率谱、RA/Dec 天区边缘。比较直接采样同一已知天空真值到目标 FITS 与完整“CCD→HiPS→FITS”输出，分解误差来自哪一次转换。

对输出 FITS 不生成缺乏来源的 ivar、point_information、variance 图像扩展；这些量只能依据外部已知模型作独立研究评价，不能从单信号图制造。

## 6. 输出文件与元数据

输出为浮点二维 FITS 的 PRIMARY 或受控 IMAGE HDU，带实际合法 WCS、像元尺寸、CRVAL/CRPIX/CD/PC 等具名变换及 BUNIT。BUNIT必须沿用实际输入HiPS信号的真实标度，不能把 Gaia XP 光度积分量无条件写回 ADU。可在 FITS 头部/运行摘要记录输入 HiPS 标识、参考响应或混合通带解释、使用的层序与插值核、输入科学有效域和未测不确定性。

像元可为负；无有效数据写 NaN，不能用零假装黑色背景。输出的普通 FITS 不要求后期软件理解 ACSD 私有二进制数据结构。

## 7. 参考文献

[1] Greisen, E. W., Calabretta, M. R. Representations of world coordinates in FITS. A&A, 395, 1061–1075. [论文](https://arxiv.org/abs/astro-ph/0207407)。

[2] Calabretta, M. R., Greisen, E. W. Representations of celestial coordinates in FITS. A&A, 395, 1077–1122. [论文](https://arxiv.org/abs/astro-ph/0207413)。

[3] IVOA. HiPS: Hierarchical Progressive Surveys. [推荐标准](https://www.ivoa.net/documents/HiPS/20170519/REC-HIPS-1.0-20170519.pdf)。


## D7-A｜已核 P3 CLI 投影、FITS 与重采样能力边界

[投影/WCS 实现级合同](../algorithms/export/PHASE3_PROJ_IMPL.md)保留 63eccbb6 全部 G1/G2、CRPIX/CD/手性/四角/错误和投影注册原文。当前 p3_projection_registry.h 的设计冻结八种投影与生产声明 D={TAN} 不同；SIN/CAR/AIT 内部核尚未进入 CLI，STG/MOL/CEA/ZEA 未实现。p3_op_wcs 实际调用 p3_wcs_make 时 PA 传 0.0；库能构造旋转不是当前 CLI 暴露了 PA。内部像素中心0-based，FITS CRPIX1/2 为1-based，外缘需另测±0.5像素边界。TAN 实现另有保守策略 |Dec|≤85°、FOV/数值门和 pix2world 切平面 r<π/2，必须区别于球面理论半球的几何奇点。天空 crop 在离散边样本求窗口，不是再重采样；CRPIX 只减 integer 裁剪偏移，CD/CRVAL 不变。

CURRENT 单Signal P3 使用 nearest 或局部四象限 bilinear，不是 P1 对真实 HEALPix 曲边的面积求交，也**尚没有 P3 conservative area-overlap**。#89 已将 bilinear 输出权重初始化0，#88 已按完整 leaf ipix 聚合重复系数再算条件对角方差；这仅证明库修复代码已经存在，当前 slim export 仍不消费/发布 uncertainty。需要另外核真实源单位与 canonical BUNIT（#9）、各级面积/PSF真值、空间相关协方差以及 #79 的 mono/unknown/mixed response 从 P2 进入最终 FITS 的原子完整传递。现 p3_props.json 未载 obs_filter/response_distribution，不能把 FILTER 和 ACSDRESP/RSPHASH 的目标合同标为已实现。本批外部 WCSLIB/Astropy、真实 FITS CLI、HST/M42 都 NOT_RUN。


## D7-B｜Resample→FITS Writer 发布和校验的真实能力

[重采样长文](../algorithms/export/PHASE3_RSMP_IMPL.md)与[FITS Writer 长文](../algorithms/export/PHASE3_FITS_IMPL.md)分别还原 Stage3 真生产：`p3_resampled.bin` 为未裁剪 signal/coverage 两个 f32 内部平面；P3FitsStream 与 ExportStreamScheduler 仅将裁剪后 signal 写入一个 PRIMARY HDU，BITPIX -32/-64 不增加输入f32有效精度。最终文件没有 COVERAGE、VARIANCE、IVAR 扩展。原子发布的真实顺序为 tmp、写PRIMARY/DATASUM/CHECKSUM、flush/close/fsync、**发布前**验证checksum和完整SHA256、原子rename、再独立流式reader对照工作数据，此外生成完整字节 `integrity_sha256` 与规范内容 `canonical_sha256`（不同身份与用途）。

**当前实现不等于完整跨层验收**：`P3FitsVerifyStream` 回读 WCS/尺寸/单HDU及像素（NaN==NaN），但不读FITS BUNIT/名义响应、也不在发布后重新校CHECKSUM；`close` 的返回状态OK与 `reopen_ok` 可以不同。生产 `p3_op_verify` 会把 `reopen_ok` 和 `canonical_match` 写入JSON，却未以这两个布尔为最终成功的强制条件；这属于静态证实的 fail-closed 调用契约缺口，待编码/故障注入。 `covered_px` 当前verify按非NaN计，非真实coverage plane，不能互相替代。#79的真实响应来源/FILTER传递尚缺；#9的真实已施加光度单位不因固定ADU/sr头字符串而获得保证。独立 FITS/astropy读侧、外部 WCS、跨阶段响应与真值光度、负例故障注入均未在D7-B运行。
