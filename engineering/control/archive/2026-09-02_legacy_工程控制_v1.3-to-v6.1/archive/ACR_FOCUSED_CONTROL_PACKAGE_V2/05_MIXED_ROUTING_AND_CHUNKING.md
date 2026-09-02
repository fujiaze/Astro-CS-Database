# CPU/GPU动态混合路由与分块

## 1. 目标

对积分、Drizzle类可拆分重负载任务，让CPU与GPU按各自适合的块大小从同一pending域动态领取，最大化总吞吐并避免传输与尾部拖累。

不使用固定份额。

## 2. 设备进入共享池前的资格门

Auto模式必须先依据当前OperationProfile筛选设备：

- CPU实现可用；
- GPU实现和Profile均qualified；
- 当前host/resident路径为eligible；
- 剩余规模达到对应`min_profitable_items`；
- RAM/VRAM/pinned预算允许；
- 预计下一块可缩短总完成时间。

不满足时该设备不启动worker。

“每个设备先领一块”只允许用于`ForcedMixed`正确性测试，禁止用于Auto生产路径。

## 3. 成本

```text
T_finish(d,n) = queue_wait
              + submit_or_launch
              + 必要传输
              + compute
              + merge/sync
```

host与resident使用不同成本。不存在交叉点的GPU路径视为不eligible，而不是给出伪阈值。

## 4. 动态块

- CPU与GPU使用独立推荐/最小块；
- 块大小来自Operation实测；
- 根据remaining、设备队列和内存状态调整；
- GPU块必须摊薄传输；
- CPU块应缓存友好；
- 尾部逐渐缩块；
- 无法领取最小高效块的慢设备停止claim；
- 最快设备清尾。

不得固定最后30%，也不得在慢设备停止后按固定等待轮数强制它重新领取。

## 5. Mixed不是强制模式

Auto允许：

- 小任务CPU-only；
- 数据已resident且GPU占优时GPU-only；
- 两侧都有边际收益时CPU+GPU Mixed。

ForcedMixed只用于证明底层分区和合并正确，不能作为Auto性能依据。

## 6. 本次运行反馈

可使用本次运行的实际items/ns、队列和剩余量修正尾段判断，但：

- 不写回离线Profile；
- 不跨运行学习；
- 统计按设备汇总，不把多个CPU worker误当多个独立CPU设备；
- 报告预测与实际分开。

## 7. 报告

至少记录：

- 每设备items/blocks/有效耗时；
- 真实H2D/D2H次数和字节；
- resident复用；
- 初始/最终块；
- 设备未进入或停止claim的原因；
- merge、coverage、失败与回退。
