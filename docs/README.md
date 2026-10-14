# ACSD 文档总入口

ACSD 目标是三个相互独立的科学命令：**Normalize** 生成逐帧 HiPS 和稀疏参考 SNR 控制数据；**Mosaic** 用稀疏精度重建、跨设备光度、相对背景和球面统计合成，最终只写**一个 signal HiPS 科学图像通道**；**Export** 将该 HiPS 信号按 WCS 导出普通平面 FITS。用户可以自主组合不同相机、望远镜、滤镜和带宽，不以设备一致性设处理限制。

先读 [最高设计](ACSD_DESIGN.md)，再读 [科学阅读入口](science/README.md)。该入口按数学因果链组织，而不是让每个实验/Agent 各自维护一套不一致的权重公式。

[生产实现差异地图](engineering/architecture/SCIENCE_IMPLEMENTATION_DELTA.md) 将目标模型与当前实际 C++ 代码分开，**不能把目标设计文本当成代码已实现的证据**。工程模块 API、I/O、资源、发行等由 engineering/ 和 detail/ 承载。历史已保留的研究实验及失败记录位于 实验/；实际设备大型 FITS 不上传 Git。

[文档机器索引](DOCUMENT_INDEX.yaml) 记录在册文件身份。过时的工程实现分册可以保留为历史/现状说明，不再覆盖现行科学定义。开发不设置流程门禁，质量依靠明确的物理估计量、可独立计算的对照实验和最终真实 FITS 效果。
