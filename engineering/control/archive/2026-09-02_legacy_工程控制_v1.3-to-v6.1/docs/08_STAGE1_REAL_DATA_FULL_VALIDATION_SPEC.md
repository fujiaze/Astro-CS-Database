# Stage1 真实数据全量验证

分三层执行：

1. T1–T4 每套、每类滤镜代表帧；
2. 银心三片 Red 32 帧正式数据；
3. TestData 全部 Light 批处理。

每帧记录 CALIBRATE、PLATESOLVE、PSF、PHOTOMETRIC、SNR、DRIZZLE、HISS 的成功/失败、耗时、峰值内存和关键指标。任何必需阶段 skipped 都是失败。

全量报告必须区分算法失败、输入损坏、校准解析失败和资源超限。不得只统计最终 exit code。
