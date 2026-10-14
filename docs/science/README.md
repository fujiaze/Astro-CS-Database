# 科学正本阅读入口

本目录只维护 ACSD 的统一科学模型、各物理问题的详细推导，以及算法/实验与生产接口的关系。阶段二最终是**只有 signal 的 HiPS 科学图像产品**；阶段一稀疏 SNR、阶段二内部精度/背景/排异均不是最终必须输出的第二数据层。

**科学阅读顺序：**

1. [最高设计](../ACSD_DESIGN.md)：用户可任意合并不同设备/滤镜；Normalize→Mosaic→Export；不设研发流程门禁。
2. [统一科学模型](unified/UNIFIED_SCIENCE_MODEL.md)：同一被估计量、单位、球面协方差和产品定义。
3. [数据语义](unified/DATA_SEMANTICS.md)：HEALPix NESTED、光度单位、无效域和阶段产品。
4. [Gaia XP 光度](photometry/PHOTOMETRY.md)：原生 R_k 与目标 R_0 及未知 SED 系统误差。
5. [稀疏 SNR → 球面精度](noise_snr/NOISE_SNR.md)：点源 V_F 和面亮度 V_B 不相同，需传递 T。
6. [球面 Drizzle / HiPS](drizzle/DRIZZLE.md)：等面积曲边、重建算子、相关噪声与多层级。
7. [相对背景和接缝](sky/UPM.md)：真实重叠图、通带/PSF/加性分离、星云低频保真。
8. [阶段三导出](projection/HIPS_TO_FITS.md)、[重采样](resample/RESAMPLE.md)：单信号 HiPS→平面 FITS。

[生产源码差异地图](../engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md) 对照哪些已实现、哪些尚未闭合。旧 science/algorithms、detail/registry 中的某些实现级叙述是历史生产路径描述；当它与上面科学正本冲突，以统一科学模型和物理实验为准，并在 Agent 修码后由前台重写对应工程文档。独立实验数据和历史失败记录保存在 仓库实验/ 中，不因重写自动升级为新科学结果。
