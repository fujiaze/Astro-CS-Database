# 数据驻留、分区合并与内存预算

## 1. 真实驻留

ResidencyManager必须驱动真实backend buffer缓存，而不是只修改状态枚举。

Buffer身份至少包含：

- 稳定buffer key；
- generation或host修改标记；
- 元素类型、元素数和真实字节数；
- access mode：read、write、read_write；
- device id；
- 对应device allocation/view。

只有实际上传成功后才能`mark_uploaded`；只有实际下载成功后才能`mark_downloaded`。不得在同一次dispatch结束时对所有buffer机械地先uploaded再downloaded。

## 2. 复用

- 共享只读输入可上传一次，由多个GPU tile/view复用；
- 已resident且generation未变时不得重复H2D；
- GPU写出的中间结果保持DeviceDirty，直到CPU/外部确实需要才D2H；
- 连续GPU算子必须复用同一device allocation；
- 释放缓存时同步更新驻留状态和VRAM reservation；
- report中的次数和字节来自真实桥接操作，不来自推断。

当前同步桥可以先实现正确的持久buffer缓存；异步stream和双缓冲可后续按实测收益增加，不作为本轮扩张目标。

## 3. 分区与合并

### Dense独立输出

CPU与GPU负责不重叠输出范围，可直接写各自区域。

### Reduction

每个WorkToken或设备使用私有FP64 partial；完成后由明确merger合并。partial槽位必须按稳定token容量分配，并覆盖retry attempt。

### Drizzle-like scatter

禁止CPU worker和GPU同时写同一bins/累计数组。必须选择：

- 输出tile独占；或
- 每token/每设备私有累计区，最后明确合并。

本轮至少完成私有partial方案，证明CPU-only、GPU-only、ForcedMixed和AutoMixed数值正确。

## 4. 内存预算

分别管理：

- 全局RAM；
- 实际pinned staging reservation；
- 每张GPU VRAM。

上限：

```text
min(total × ratio_limit, total - fixed_reserve)
```

默认ratio 0.95。

pinned不能用系统RAM总用量近似；应由ACR自己的pinned allocator/reservation ledger记录。

## 5. Claim前峰值

包含：

- 输入输出；
- device/host workspace；
- 私有partial与merge；
- pinned staging；
- 双缓冲（若实际启用）；
- 已resident且可复用的数据不得重复计费。

## 6. 超预算

1. 缩小该设备块并重新估算；
2. 复用或释放可重建缓存；
3. 等待已提交工作释放reservation；
4. 未开始块交给其他eligible设备；
5. 无可行路径时明确失败。

不得依赖操作系统OOM。
