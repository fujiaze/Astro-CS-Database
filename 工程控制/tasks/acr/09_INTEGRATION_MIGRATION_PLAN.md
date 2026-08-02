# 未来算法接入指南（当前支线禁止执行）

## 1. 文档性质

本文件仅保留给将来的算法重构阶段。本 `feature/astrocompute-runtime` 分支不得修改或接入任何 AstroCS 现有算法。

底层完成、合并 `main` 并备用后，只有在用户完成其他算法逻辑验证并明确启动新任务时，才允许从最新 `main` 创建独立集成分支，例如：

```text
feature/acr-drizzle-integration
feature/acr-batch-integration
```

## 2. 未来接入门禁

真实算法接入前必须具备：

- 算法逻辑已经由用户确认；
- 真实数据结果已经稳定；
- 有明确的耗时证据；
- 预计收益高于迁移、传输和维护成本；
- 可以清楚描述工作域、读写范围和合并规则；
- ACR 对应经典能力测试已通过。

单次 CPU 一两秒完成且不形成累计瓶颈的模块默认不接入。

## 3. 未来最小改造方式

只允许通过公共 API：

- `parallel_for`；
- `parallel_tiles`；
- `parallel_reduce`；
- `parallel_batch`。

算法不得直接依赖 alpaka、oneTBB、CUDA、HIP、SYCL、StarPU 或设备句柄。

## 4. 旧路径保护

未来每个接入热点必须：

- 保留原 CPU/reference 路径；
- 用 feature flag 独立启用；
- 先做结果对照，再做性能评估；
- GPU、标定或路由不可用时可回退；
- 未完成验收前不得删除旧实现。

## 5. 与本支线的隔离

本支线交付中只验证合成经典 kernel，不应存在任何 `drizzle`、`hiss`、`calibration`、`psf` 等业务源码的修改 diff。若出现，视为越界并回退。

## 6. 未来接入时不得指定设备比例

算法接入只提供TaskClass/TaskTraits和工作域，不填写CPU/GPU share。ACR依据通用硬件画像动态派发。只有任务语义和必要特征属于算法代码，性能参数属于Qualification结果。
