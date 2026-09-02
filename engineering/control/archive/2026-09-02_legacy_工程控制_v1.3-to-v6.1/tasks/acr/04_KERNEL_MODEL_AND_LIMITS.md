# Kernel模型、任务特征与限制

## 1. ACR自动拆的是工作域

理想形式：

```cpp
output[i] = f(input[i], constants);
```

算法作者定义一个工作项，ACR自动切块并让CPU/GPU动态领取。

不能无条件拆分：

```cpp
for (...) state = update(state, current);
```

这种跨项依赖需要scan、递推或专门算法。

## 2. 预定义任务类别

- Elementwise：连续逐元素；
- Reduction：局部结果再合并；
- Stencil/Convolution：输出Tile读取有限邻域；
- Resampling/Gather：坐标驱动读；
- Histogram/Atomic：共享计数或冲突写；
- Sparse Gather/Scatter：稀疏和随机访存；
- Branch Heavy：分支或循环次数不均；
- Batch Independent：对象彼此独立；
- FFT/GEMM Library：专用成熟库。

通用硬件画像只能在算法提供正确类别和关键提示时合理推算，不能只看总元素数。

## 3. TaskTraits不是手工性能调参

TaskTraits只表达算法事实：

- 数据是否连续；
- 是否有halo；
- 是否稀疏；
- 是否有原子冲突；
- 工作量是否均匀；
- 精度；
- 是否允许混合设备。

不得填写“GPU快4倍”或设备百分比。性能全部来自Benchmark画像。

## 4. GPU可移植kernel限制

默认禁止文件I/O、socket、系统调用、GUI、异常、RTTI、虚函数、host指针、复杂STL、隐式全局可变状态和未受控动态分配。

允许POD、BufferView、固定小结构、设备数学函数、显式原子、局部变量、有界循环和分支。

## 5. CPU ISA

同一逻辑可以编译为baseline、SSE、AVX、AVX2、AVX-512等变体。cpu_features只做安全门禁；实际选择依据硬件画像曲线。不得假设最高ISA永远最快。

## 6. 混合安全

CPU/GPU同时执行必须满足至少一项：

- 输出范围不重叠；
- 每设备局部结果最后合并；
- 使用受控原子且语义明确。

Tile任务默认按输出Tile所有权分配；不得让两个设备无保护写同一输出Tile。

## 7. 正确性

- 整数实验要求完全一致；
- FP32/FP64按经典实验容差；
- 默认不要求逐位一致；
- 特殊任务可要求确定性合并；
- 未通过正确性验证的backend实现不得进入画像候选。
