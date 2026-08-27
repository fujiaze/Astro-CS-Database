# 数据驻留、传输与内存预算

## 1. 真实驻留定义

只有同时满足以下条件才可报告resident：

- 已存在真实device allocation；
- generation与host数据一致；
- CUDA launcher直接使用device view；
- 本次执行未重新逐块H2D同一输入；
- 传输计数来自桥接真实调用。

仅在ResidencyManager中改状态、而launcher仍创建host vector并调用非resident submit，不算驻留。

## 2. 驻留建立顺序

若路由决定使用resident路径，必须在worker执行前完成预取或确认已有device buffer：

```text
register buffer → memory budget reserve → upload/prefetch → mark device valid
→ resident launcher处理多个token → 按策略保留或最终materialize
```

禁止执行结束后才上传输入再声称本次使用了resident路径。

## 3. 启动驻留的最小契约

为避免host路径无收益时形成“GPU不启动→永远无法resident”的死路，至少提供一种明确机制：

- `ResidencyPolicy::KeepDevice/PreferDevice`加预期复用次数；或
- 显式`prefetch(buffer, device)`；或
- 一个很小的连续Operation提交单元。

不建设通用计算图，只需能表达“上传一次，后续多个token/Operation复用，最后一次下载”。

## 4. 输出与累加器驻留

积分、Drizzle的主要收益通常来自累计输出或中间buffer留在显存：

- 输入按批上传；
- GPU accumulator/tile buffer跨块保留；
- CPU路径写自己的partial；
- 最终明确merge；
- 仅在CPU或文件输出真正需要时D2H。

## 5. 真实传输报告

ExecutionReport必须记录：

- H2D/D2H次数和字节；
- resident launch次数；
- 每块是否发生传输；
- device buffer复用次数；
- 最终materialize原因。

测试必须从Dispatcher完整路径核对桥接计数，不能只直接测试resident C接口。

## 6. pinned staging

`PinnedLedger`只表示容量预留，不等于真实pinned memory。最终必须二选一：

- 实现可复用的`cudaHostAlloc/cudaHostRegister` staging pool，并让host路径真实使用；或
- 将其明确命名为普通staging reservation，不宣称pinned传输收益。

为了减少H2D/D2H开销，推荐实现小型可复用pinned pool，不需要复杂通用allocator。

## 7. 内存预算

只保留容量控制，不做CPU/GPU利用率控制：

- 全局RAM；
- staging/pinned；
- 每张GPU独立VRAM；
- 默认容量上限可为95%，仅表示容量限制。

claim前峰值必须区分host路径和resident路径，resident复用不得重复计入整帧分配。超限执行缩块、释放缓存、等待、跨设备回退或安全失败。
