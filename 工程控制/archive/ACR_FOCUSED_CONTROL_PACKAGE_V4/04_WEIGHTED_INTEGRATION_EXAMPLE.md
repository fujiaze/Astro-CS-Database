# 加权积分最小接入样例

## 1. 目的

新增完全独立于Phase1的合成Operation：

```text
synthetic.weighted_integration.fp64acc
```

它同时承担：

- `IndependentOutputTiles`接入示例；
- OpenMP基线；
- CPU/GPU/Mixed正确性验证；
- 动态块与显存驻留验证；
- 不同图像大小的实际性能比较；
- 未来真实积分接入模板。

## 2. 数学语义

对每个输出像素`p`：

```text
numerator   = Σ_f weight[f] × frame[f,p]
denominator = Σ_f weight[f]
output[p]   = float(numerator / denominator)
```

要求：

- 输入和权重为FP32；
- numerator和denominator使用FP64累加；
- 输出为FP32；
- 所有权重为有限正数；
- 帧顺序固定；
- 不使用破坏语义的fast-math。

## 3. 合成数据

数据生成不计入计时，使用固定seed和可重复公式。建议：

```cpp
frame[f,p] = 0.25f
           + 0.5f * deterministic_hash01(seed, f, p)
           + 0.01f * float(f % 17);
weight[f]  = 0.5f + 0.01f * float((f * 37) % 101);
```

值域保持有限、无NaN/Inf。报告seed、尺寸、帧数和输入校验摘要。

## 4. 必须实现的路径

### SerialReference

仅用于quick小case，FP64累加，作为最终数值参考。

### OpenMPBaseline

- 输出像素并行；
- 与ACR CPU共享同一个`integrate_one_pixel`或`integrate_range`核心；
- 线程数固定并记录；
- `omp_set_dynamic(0)`；
- 不计数据生成和首次线程池初始化。

### AcrCpuOnly

- 通过KernelRegistry CPU launcher执行；
- launcher内部无OpenMP；
- 验证ACR CPU封装开销和结果。

### AcrGpuOnly

分开报告：

- `host_roundtrip`：包含一次输入H2D、kernel和必要D2H；
- `resident`：输入预取在计时外，计时kernel和必要输出物化。

### AcrAutoMixed

- 使用qualified OperationProfile；
- CPU和GPU动态领取输出像素块；
- 不指定份额；
- 输入frames一次resident；
- CPU写host范围，GPU写device范围并物化其拥有范围；
- 报告实际设备分工和传输次数。

### ForcedMixedCorrectness

只用于证明双方非零工作且无重叠/漏算，不作为生产性能结论。

## 5. 参考文件

控制包`examples/weighted_integration/`提供：

- 数据与逐像素核心契约；
- CPU/OpenMP参考；
- CUDA kernel参考；
- Benchmark主程序结构；
- CMake接入示例。

Agent应按当前ACR真实API落地，不得为了让参考代码编译而绕过Dispatcher或伪造设备统计。

## 6. 测试输出

每个case必须生成JSON记录，符合：

`schemas/weighted_integration_report.schema.json`

还应输出简洁CSV或终端表格：

```text
width height frames mode median_ms speedup_vs_omp cpu_items gpu_items h2d_count d2h_count max_abs rel_l2
```
