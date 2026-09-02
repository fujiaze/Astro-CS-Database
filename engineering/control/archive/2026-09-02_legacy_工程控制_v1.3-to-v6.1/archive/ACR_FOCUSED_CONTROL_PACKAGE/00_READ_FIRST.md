# AstroCS ACR 工程控制包

唯一开发分支：`feature/astrocompute-runtime`  
包名固定：`AstroCS_ACR_Control_Package.zip`  
状态：底层支线开发；Phase1业务算法接入不在本轮范围。

## 1. 唯一目标

ACR不是通用异构计算平台。它只为少数已经证明高耗时、规则并行、逐像素或逐样本的核心算法提供CPU+GPU动态混合执行能力，当前目标算法族只有：

- 图像积分/叠加；
- Drizzle映射与累计；
- 后续经真实性能剖析证明值得接入的同类热点。

解析、元数据、星表小查询、稀疏低负载任务及其他简单模块不接入ACR，继续使用现有OpenMP/CPU实现。

## 2. 路由目标

目标不是整批选择CPU或GPU，也不是固定比例切分，而是：

1. 将可拆分的像素/样本域切成动态工作块；
2. CPU与GPU使用不同的合适块大小；
3. 两类设备从共享未开始工作池持续领取；
4. 计入kernel启动、H2D/D2H、合并和队列开销；
5. 优先复用显存驻留数据，避免重复搬运；
6. 尾部缩块，避免慢设备持有最后一个大块；
7. 小任务、GPU不可用或传输不划算时允许CPU-only；GPU明显占优且CPU参与会拖尾时允许GPU-only；大任务默认争取真实Mixed。

禁止`cpu_share`、`gpu_share`、固定70/30及任何持久化设备份额。

## 3. 本轮必须完成

- 保留并稳定现有CPU/CUDA真实执行链、KernelRegistry、KernelInvocation、DeviceExecutor和共享工作池；
- 将运行时与Benchmark收缩到目标像素算法所需的最小信息；
- 建立面向`dense_pixel_accumulate`、`pixel_reduce`、`drizzle_like_scatter_accumulate`的合成基准；
- 形成简单、可解释的OperationProfile；
- 用OperationProfile决定CPU/GPU块大小、最小GPU收益规模和数据驻留成本；
- 完成动态Mixed、尾部停止慢设备claim、驻留复用和RAM/VRAM预算；
- 生成单一干净HEAD的Evidence。

## 4. 明确不做

- 不修改Drizzle、积分、HISS、Pipeline、Stage1/2、正常CLI和现有OpenMP；
- 不继续扩展通用gather/scatter/branch/atomic/FFT/BLAS全家桶；
- 不做所有ISA×线程×精度×尺寸的笛卡尔积画像；
- 不做CPU/GPU精确利用率控制；
- 不做在线学习，不把运行时样本写回离线Profile；
- 不为了删除旧结构而大规模重写已稳定代码；未进入目标生产路径的通用字段可保留但不得成为合并门禁。

## 5. 权威阅读顺序

1. `01_SCOPE_AND_DECISIONS.md`
2. `02_TARGET_ARCHITECTURE.md`
3. `03_PUBLIC_API_AND_PARTITION_CONTRACT.md`
4. `04_BENCHMARK_AND_OPERATION_PROFILE.md`
5. `05_MIXED_ROUTING_AND_CHUNKING.md`
6. `06_DATA_RESIDENCY_AND_MEMORY.md`
7. `07_TEST_AND_ACCEPTANCE.md`
8. `08_CURRENT_EXECUTION_PLAN.md`
9. `09_GIT_AND_DELIVERY.md`
10. `CHECKLIST.md`

旧20—26号计划及旧通用画像要求已被本包完全取代，不得继续作为当前任务依据。

## 6. 工程启动词

```text
继续feature/astrocompute-runtime分支，读00_READ_FIRST和08号计划；聚焦积分/Drizzle混合分块、传输复用与内存预算，不改算法。
```
