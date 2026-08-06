# 精简Benchmark与OperationProfile

## 1. 目的

Benchmark只回答：

1. 当前目标像素Operation在CPU、GPU resident和GPU host路径上的真实端到端成本；
2. CPU/GPU各自什么块大小能摊薄启动、传输和merge；
3. 当前任务规模与驻留状态下，GPU是否有收益。

不建设通用硬件评分体系。

## 2. 必测Operation

- Dense pixel accumulate FP32；
- Dense pixel accumulate，FP32输入+FP64累加；
- Pixel reduction，FP64累加；
- Drizzle-like scatter/accumulate；
- 连续GPU算子的resident chain。

CPU与GPU必须执行相同工作域、边界和数值语义。

## 3. 三条曲线

```text
T_cpu(n)
T_gpu_resident(n) = launch + resident_compute + merge
T_gpu_host(n)     = H2D + launch + compute + merge + 必要D2H
```

必须保存原始尺寸、每次重复耗时、候选块耗时和正确性结果，Evidence不能只留下最终JSON和一行摘要。

## 4. 单位和交叉点

内部成本计算统一使用纳秒：

```text
cpu_fixed_ns = cpu.fixed_us × 1000
gpu_fixed_ns = (gpu.fixed_us + gpu.launch_us) × 1000
T_cpu(n) = cpu_fixed_ns + cpu_ns_per_item × n
T_gpu_resident(n) = gpu_fixed_ns + gpu_ns_per_item × n
```

禁止将微秒截距直接除以纳秒/项斜率。

若GPU边际斜率更小，理论交叉点为：

```text
n* = (gpu_fixed_ns - cpu_fixed_ns) /
     (cpu_ns_per_item - gpu_ns_per_item)
```

host路径再加入实际H2D/D2H固定项和与真实输入/输出字节对应的斜率。Reduction的D2H不能按每输入item复制，Drizzle输出字节也必须按真实bins/tile公式计算。

收益阈值必须同时满足：

- 数学交叉点正确；
- 不小于该设备最小有效块；
- 在实测尺寸或额外验证点上确实观察到`T_gpu < T_cpu`；
- 不允许仅凭外推得到远小于最小样本的虚假阈值。

## 5. 块大小与收益阈值分离

- `recommended_chunk_items`来自候选块实测；
- `minimum_chunk_items`表示设备最低有效块；
- `min_profitable_items_*`表示整个剩余任务达到GPU收益的门槛；
- 三者不得互相覆盖。

尤其禁止用收益阈值把4M推荐GPU块压成131K。尾段缩块只由remaining和预测完工时间决定。

## 6. 资格语义

`Operation.qualified`表示该Operation的测量、正确性、误差和路由结论可信，不表示GPU一定有收益。

因此允许：

```text
qualified=true
host_path_eligible=false
resident_path_eligible=false
```

此时Auto必须稳定走CPU，但仍使用该Operation的CPU推荐块，不得退回无关通用模型。

顶层状态：

- 所有必测Operation均qualified：`profile_state=qualified`；
- 只有部分可信：`profile_state=partial`；
- quick或缺关键测量：`diagnostic`；
- 指纹变化：`stale`。

每个Operation保存非空`qualification_reason`。Validator必须检查顶层状态与Operation状态一致。

## 7. 误差与正确性

每个Operation需同时满足：

- CPU/GPU必测点有效；
- CPU、GPU和Mixed结果正确；
- host/resident语义真实；
- 留出尺寸中位误差≤30%，P95≤60%；
- 推荐块有实测依据；
- 收益阈值经实测验证；
- Profile完整roundtrip逐字段一致。

GPU路径无收益不是资格失败；错误测量、错误语义或误差超限才是不合格。

## 8. 指纹

指纹来自实际CPU/GPU、驱动、编译器和真实kernel/binary hash。不得硬编码。Phase1接入后只替换真实OperationId和合成输入，不扩张框架。
