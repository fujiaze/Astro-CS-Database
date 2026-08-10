# AstroCS ACR 工程控制包

唯一分支：`feature/astrocompute-runtime`  
稳定包名：`AstroCS_ACR_Control_Package.zip`  
当前阶段：**业务算法改造前的架构冻结与最小接入验证**。

## 1. 本轮唯一目标

在不修改Phase1、真实积分、Drizzle、HISS、Pipeline和CLI的前提下：

1. 冻结ACR面向重负载规则像素算法的最终架构；
2. 关闭V3剩余的成本、驻留、分块和Evidence门禁；
3. 新增一个独立的“加权积分”合成样例；
4. 用不同图像尺寸和帧数比较串行、OpenMP、ACR CpuOnly、GpuOnly和AutoMixed；
5. 用该样例证明最小接入方式、CPU+GPU动态分片、显存驻留、传输统计和数值正确性；
6. 形成“允许开始修改业务代码”的最终Evidence。

## 2. 冻结定位

ACR不是通用异构平台，只服务积分、Drizzle及后续少数经剖析证明值得改造的重负载、规则并行、逐像素/逐样本算法。

解析、元数据、低负载稀疏查询和简单控制逻辑继续使用现有OpenMP/CPU代码，不进入ACR。

## 3. 冻结执行模型

- 默认`AutoMixed`；不使用固定CPU/GPU比例；
- CPU和GPU从同一pending工作域动态领取不同大小的连续输出块；
- CPU侧为多worker执行器；GPU侧每张卡只有一个逻辑执行器；
- GPU执行器内部可有1～3个CUDA stream/通道，不能把stream伪装成多张GPU；
- GPU输入优先一次上传并驻留；每个token不得重复上传整帧栈；
- GPU输出按其拥有的连续范围异步物化到host；CPU直接写host对应范围；
- 尾段按预测总完工时间缩块或停止慢设备继续claim；
- RAM、staging/pinned和每张GPU VRAM是唯一强制资源约束；
- CPU/GPU精确利用率控制永久撤销。

## 4. 本轮禁止事项

- 禁止修改任何真实业务算法；
- 禁止提前接入真实积分或Drizzle；
- 禁止扩展通用gather、branch、FFT、BLAS或全部ISA画像；
- 禁止在线学习和固定份额；
- 禁止为了展示Mixed而在生产Auto中强制慢设备参与；
- 禁止在ACR CPU worker内部嵌套完整OpenMP并行区。

## 5. 当前基线

评审基线：`AstroCS_Review_ACRFocusedV3_20260806`  
分支：`feature/astrocompute-runtime`  
HEAD：`610d7b6a0b5737db220021948dd969b4c8f9a272`  
评审包SHA-256：`047357818cd8e9d9c2ee28b7e96f87ec892778ec0b0b60708e2d0ee386712327`

V3已具备真实CPU/CUDA执行、共享工作池、makespan路由、OperationProfile、resident桥接和partial基础；已知桥接仍以同步语义为主。本轮只完成冻结和加权积分独立样例，不改变产品范围。

## 6. 阅读顺序

1. `01_ARCHITECTURE_FREEZE.md`
2. `02_BUSINESS_INTEGRATION_BOUNDARY.md`
3. `03_MIXED_EXECUTION_MODEL.md`
4. `04_WEIGHTED_INTEGRATION_EXAMPLE.md`
5. `05_BENCHMARK_MATRIX_AND_FAIRNESS.md`
6. `06_TEST_AND_ACCEPTANCE.md`
7. `07_CURRENT_EXECUTION_PLAN.md`
8. `08_GIT_AND_DELIVERY.md`
9. `CHECKLIST.md`

## 7. 工程启动词

```text
继续feature/astrocompute-runtime分支，读00_READ_FIRST和07号计划；冻结ACR架构并完成加权积分合成Mixed样例，不改Phase1业务代码。
```
