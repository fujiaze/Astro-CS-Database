# C++ 未决工程实验计划

## 原则

这些事项尚未由用户冻结。Agent只能使用C++实验并提交数据、分析和推荐，不能替用户做最终决定。

实验必须复用正式HISS内存布局、生产codec调用和Reader/Writer路径。禁止用Python重新实现算法后宣称代表C++性能。

## 1. 待实验事项

### 1.1 各子块 codec / transform

至少比较：

#### signal float32

- RAW；
- LZ4；
- byte-shuffle + LZ4；
- Zstd低级别；
- byte-shuffle + Zstd低级别；
- 仓库已有且维护成本合理的其他候选。

#### support uint8

- RAW；
- LZ4；
- Zstd低级别。

#### BITMAP

- RAW bit-packed；
- LZ4；
- Zstd低级别；
- 如实现简单，可加RLE候选，但不得为了实验引入复杂正式依赖。

#### SPARSE_LIST

- uint32原始列表；
- 升序delta；
- delta + varint；
- 上述形式分别配LZ4/Zstd。

### 1.2 FULL / BITMAP / SPARSE_LIST切换阈值

使用真实Tile占用分布，测量：

- 磁盘体积；
- 编码/解码耗时；
- 单Tile随机读取延迟；
- Reader恢复ipix的CPU成本。

输出建议阈值区间，不写死最终标准。

### 1.3 checksum候选

至少比较当前可用的：

- CRC32C；
- xxHash系列或仓库已有高速hash；
- RAW无校验仅作为基线，不作为推荐结论。

衡量校验吞吐及对总读写的占比。最终算法等待用户确认。

### 1.4 子块对齐

比较例如：

- 8字节；
- 64字节；
- 4 KiB。

测试顺序读、随机Tile读和文件体积浪费。只提交推荐。

## 2. 数据集覆盖

优先使用仓库已有真实Stage1代表数据，不复制到最终ZIP。实验至少覆盖：

- 高分辨率、大像素数、接近或超过100 MP输出；
- 常规天文图像中心密集Tile；
- 视场边缘部分覆盖Tile；
- pixfrac导致内部孔洞的Tile；
- 极稀疏Tile；
- support高连续性与高碎片性样本；
- 不同信号动态范围和噪声水平。

若真实数据无法直接跑通，可从真实处理链导出匿名二进制块作为临时benchmark输入，但不得把大数据放入交付ZIP。

## 3. 测量指标

每个候选至少记录：

- 原始字节数；
- 压缩字节数；
- 压缩比；
- 压缩wall time；
- 解压wall time；
- 压缩/解压吞吐 MB/s；
- CPU time或CPU利用率；
- 峰值RSS；
- 单Tile随机读取 P50/P95/P99；
- 连续读取吞吐；
- 解码后内容校验；
- 编译器、构建类型、CPU、内存和OS。

必须预热并重复多轮，报告中位数及波动，不用单次最快值。

## 4. C++ benchmark输出

必须生成：

- `reports/experiments/raw_results.csv`；
- `reports/experiments/raw_results.json`；
- `reports/experiments/summary.md`；
- `reports/experiments/environment.md`；
- benchmark源码和构建入口；
- 用于复现的命令，但所有外部命令需有超时或明确终止条件。

## 5. 决策边界

报告可写：

- “候选A在当前样本综合最优”；
- “建议signal使用A、support使用B”；
- “建议占用率低于X%时优先SPARSE_LIST”。

但必须同时写：

> 此结论仅为实验建议，未写入冻结规范，也未设为不可更改的正式默认值，等待用户与主审助手确认。
