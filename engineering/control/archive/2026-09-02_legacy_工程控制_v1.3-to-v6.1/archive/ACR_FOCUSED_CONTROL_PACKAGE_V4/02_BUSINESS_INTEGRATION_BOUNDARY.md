# 修改业务代码前的接入边界

## 1. 当前阶段不改业务

本轮只在`lib/acr`内新增独立样例、测试、Benchmark和必要的底层修复。不得改动：

- Phase1算法；
- 当前真实积分实现；
- 当前Drizzle实现；
- HISS、Pipeline、编排器、CLI；
- 现有简单任务的OpenMP路径。

## 2. 未来积分最小改动形态

现有代码若类似：

```cpp
#pragma omp parallel for
for (std::size_t p = 0; p < pixel_count; ++p) {
    output[p] = integrate_one_pixel(p, frames, weights);
}
```

接入时应先抽出不含并行策略的核心：

```cpp
void integrate_range(std::size_t begin,
                     std::size_t end,
                     const IntegrationView& input,
                     float* output);
```

然后：

- 原OpenMP路径继续对pixel范围并行调用同一核心；
- ACR CPU launcher直接调用`integrate_range`，不嵌套OpenMP；
- CUDA launcher实现相同逐像素语义；
- 外层业务仅构造一次Invocation并提交。

外围数据准备、错误处理、文件读写和输出接口无需重写。

## 3. 未来Drizzle最小改动形态

第一阶段允许保留“输入样本分片 + 私有partial + FP64 merge”，用于验证正确性和收益；长期优化再迁移到HEALPix Tile所有权。

业务接入前本轮只验证ACR的`PrivatePartialThenMerge`基础，不改真实Drizzle。

## 4. Buffer绑定要求

未来业务适配器必须显式声明：

- input/output/read-write角色；
- host pointer与bytes；
- stable key与generation；
- 是否允许输入/输出跨调用驻留；
- 每item或每tile峰值workspace；
- 最终是否必须物化到host。

不得只在状态表写“resident”，而launcher仍逐token创建host vector并重复H2D/D2H。

## 5. 接入完成度判定

加权积分独立样例通过后，只表示：

> ACR接口、动态混合、驻留和分块模式已适合开始业务改造。

不表示真实积分或Drizzle已经接入，也不将合成样例性能等同于最终业务性能。
