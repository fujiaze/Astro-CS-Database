# 数值与性能固定门禁

## 1. 容差管理

每项实际容差必须在首次运行前写入版本控制的 `tolerance_contract.json`，包含数据类型、比较域、absolute/relative/ULP 阈值、NaN/Inf 规则和科学理由。禁止事后放宽。

最低要求：

- 整数计数、tile id、frame id、mask、reason code、配置解析、输入排序：exact。
- FP64 deterministic CPU：优先 exact；非固定归约可使用经 SCI 文档证明的严格 abs/rel 容差。
- FP32/GPU/FMA：允许正常舍入和归约误差，但必须逐层统计 max abs、max rel、RMS、P50/P95/P99、超阈值像素数。
- NaN/Inf 的位置和类别必须 exact；不允许以过滤 NaN 后的相似度掩盖差异。
- 科学不变量（flux、constant field、gauge、support accounting）必须单独判定，不能只看像素误差。

## 2. 并行性能

Linux 2 核：

- 生产路径 `max_threads >= 2`；
- CPU 密集窗口平均 `CPU >= 150%`；
- `wall_1T / wall_2T >= 1.50`；
- 采样周期 <=200 ms；
- 负载 wall 5–60 s；
- 单串行段 <1 s，总串行计算 <1%。

Fatduck CPU 路径：

- worker 数必须等于配置值且不超过逻辑核；
- CPU 密集阶段不得只占一个核心；
- 报告 1T/NT speedup、CPU time/wall、RSS、上下文切换。

GPU/Mixed：

- GPU route 必须记录真实 kernel/backend 激活；
- `fallback_reason` 必须为空；
- Mixed 必须同时观测 CPU worker>0 和 GPU utilization>0；
- 只加载 CUDA DLL、只创建 context 或只分配显存不算 GPU 计算。

## 3. 停止规则

短探针首次发现单线程、死锁、数值差异或 speedup 不达标时立即停止并修复。禁止用更大的数据“再看看”，禁止在失败状态启动 32R。
