# Stage1 与 HISS 实施规范

本规范从根 README 第5、6章派生，不得改变其边界。

## 必须实现

1. 正式模式必须解析并应用 Bias/Dark/Flat；用户已确认主校准帧配齐，解析失败先修映射器。
2. PipelineFrame 内保持 float32 主信号；PSF 原生 float32 接口优先，避免整图 uint16 量化。
3. HISS 核心块：有序 `ipix`、float32 `signal`、逐像素 `support`、稀疏 SNR 控制点、provenance。
4. 无覆盖不写零；无效值不进入正式数据。
5. 采用按 HEALPix Leaf/连续索引范围的独立压缩块和随机读取索引。
6. `ipix` 可差分/变长编码；signal 默认无损 float32；support 低精度存储必须有误差 Gate。
7. HISS reader 必须支持 header-only、index-only、leaf batch read、校验和错误隔离。

## 验收输出

- T1–T4代表帧 HISS；
- HISS inspector JSON；
- 分块随机读取测试；
- 压缩前后字节数、压缩率和读取速度；
- signal/support/SNR 数值统计；
- 浏览器检查截图索引。
