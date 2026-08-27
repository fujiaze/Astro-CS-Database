# 精简Benchmark与OperationProfile

## 1. 目的

Benchmark只回答：

1. 当前目标像素Operation在CPU和GPU上的真实端到端成本；
2. 哪些块大小能摊薄启动、传输与merge；
3. host输入、resident输入和Mixed分别在什么规模下有真实收益。

不再建设通用硬件评分体系。

## 2. 必测Operation

- Dense pixel accumulate FP32；
- Dense pixel accumulate，FP32输入+FP64累加；
- Pixel reduction，FP64累加；
- Drizzle-like scatter/accumulate；
- 连续GPU算子的resident chain。

CPU与GPU必须执行相同WorkloadDescriptor、相同边界和相同数值语义。

## 3. 三条必须分开的曲线

每个Operation至少测：

```text
T_cpu(n)
T_gpu_resident(n) = launch + resident_compute + merge
T_gpu_host(n)     = H2D + launch + compute + merge + 必要D2H
```

禁止：

- 将host roundtrip复制成resident曲线；
- 将最小尺寸总耗时写成fixed再叠加每item斜率；
- 将含传输时间写成纯GPU计算；
- 用零值表示“未测得误差”。

## 4. 拟合与交叉点

允许使用简单线性或分段最近点模型，不追求复杂机器学习。

固定开销必须来自拟合截距或独立launch/submit测量；每item成本来自有效斜率。

GPU路径只有在以下条件满足时才可产生有限收益阈值：

```text
GPU渐近边际成本 < CPU渐近边际成本
且存在 n 使 T_gpu(n) < T_cpu(n)
```

host和resident分别计算。不存在交叉点时：

- `host_path_eligible=false`或`resident_path_eligible=false`；
- 对应`min_profitable_items`写`null`；
- Auto路由不得让该路径参与。

## 5. 块大小

CPU和GPU推荐块必须来自候选块实测，至少比较3个候选值。选择目标：

- 吞吐进入稳定区；
- 调度开销可忽略；
- 不造成明显尾部拖延；
- 满足RAM/VRAM预算。

禁止无证据硬编码64K/1M后直接作为qualified结论。

## 6. 资格

`standard`也不能自动qualified。每个Operation需同时满足：

- CPU和GPU全部必测点有效；
- correctness通过；
- host/resident语义真实；
- 留出尺寸中位误差≤30%，P95≤60%；
- 推荐块有实测依据；
- 收益阈值按真实CPU/GPU/transfer模型求得；
- Profile完整往返读取逐字段一致。

某Operation不合格只隔离该Operation，不得让其他Operation的GPU样本替它取得qualified。

## 7. Profile读取和指纹

必须使用可靠JSON对象层级解析，禁止按字符串搜索第一个同名键。

roundtrip至少验证：

- CPU与GPU全部曲线；
- transfer和memory；
- eligibility与nullable阈值；
- GPU数组；
- compiler、driver、runtime kernel hash。

指纹必须来自实际运行环境和真实源码/二进制指纹，不得硬编码编译器字符串或固定日期seed冒充hash。

## 8. 最小字段

每个Operation保存：

- CPU curve；
- GPU resident curve；
- host roundtrip所需transfer；
- CPU/GPU推荐及最小块；
- host/resident路径是否eligible；
- nullable收益阈值；
- 内存公式；
- 样本范围、重复次数、误差与资格原因；
- 硬件、驱动、编译器、kernel指纹。

Phase1接入后仅替换为真实积分/Drizzle OperationId和合成输入，框架不再扩张。
