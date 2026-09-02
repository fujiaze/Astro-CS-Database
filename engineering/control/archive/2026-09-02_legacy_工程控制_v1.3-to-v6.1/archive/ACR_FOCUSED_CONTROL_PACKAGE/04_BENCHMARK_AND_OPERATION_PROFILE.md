# 精简Benchmark与OperationProfile

## 1. 目的

Benchmark只回答三个问题：

1. 某目标像素内核在CPU和GPU上的块吞吐是多少；
2. GPU启动与数据传输需要多少固定/线性开销；
3. 在什么任务规模、驻留状态和块大小下，CPU、GPU或Mixed最划算。

## 2. 必测项目

### 基础开销

- CUDA launch/event/sync中位耗时；
- pageable与pinned H2D、D2H；
- device-to-device可选；
- 分配开销仅用于诊断，生产应优先复用缓冲区。

### 目标合成内核

- Dense pixel accumulate FP32；
- Dense pixel accumulate，FP32输入+FP64累加；
- Pixel reduction，FP64累加；
- Drizzle-like scatter/accumulate，使用可控覆盖密度；
- 两个连续GPU算子的resident chain。

### 尺寸

使用少量对数尺度，例如：

`256K、1M、4M、16M、64M items`

必要时围绕CPU/GPU收益交叉点补1—2个点，不做参数笛卡尔积。

## 3. CPU测量

- 测生产CPU后端的实际配置，不要求完整线程数曲线；
- 至少保留单线程或Scalar正确性基线；
- 可记录当前ISA和线程数作为Profile指纹；
- 不为SSE/AVX/AVX2/AVX-512分别建立复杂生产资格，除非现有实现已经具备且无需额外开发。

## 4. GPU测量语义

必须区分：

- `resident_compute`：数据已在显存；
- `host_to_device`；
- `device_to_host`；
- `host_roundtrip`：上传+计算+必要下载；
- `resident_chain`：连续算子只上传一次、最后下载一次。

禁止把含传输的数据写成纯显存计算曲线。

## 5. OperationProfile最小字段

每个Operation/precision/variant只需保存：

- CPU固定开销、每item耗时、推荐/最小块；
- GPU launch开销、resident每item耗时、推荐块；
- host与resident两种最小GPU收益规模；
- H2D/D2H带宽和固定延迟；
- host/device每item字节与固定workspace；
- Benchmark尺寸范围、样本数和简单置信状态；
- 硬件、驱动、编译器和kernel指纹。

Schema见`schemas/operation_profile.schema.json`。

## 6. 资格规则

- 每个点至少3次预热、7次有效测量，保存中位数和P95；
- CPU/GPU必须执行完全等价的工作量；
- correctness失败的记录不得生成qualified Profile；
- Profile预测在留出尺寸上的中位误差目标≤30%，P95≤60%；
- 预测不达标时允许使用最近实测点或保守CPU回退，不得无限扩充模型；
- `quick`只诊断，`standard`才可用于生产路由。

## 7. 未来真实算法替换

Phase1接入后，积分/Drizzle必须使用正式算法内核和合成数据重新标定对应OperationId。基础launch、传输和内存数据继续复用；不要求保留合成内核对真实算法的路由权重。
