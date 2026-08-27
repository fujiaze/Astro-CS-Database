# CPU/GPU动态混合路由与分块

## 1. 成本估算

对设备`d`和候选块`n`估算：

```text
T_finish(d,n) = queue_wait
              + fixed_submit_or_launch
              + required_transfer
              + measured_compute
              + required_merge_or_sync
```

只使用当前Operation的Profile，不使用无关通用总分。

## 2. 初始块大小

- CPU和GPU独立计算块大小；
- 块大小优先落在Profile稳定区间；
- GPU块必须足够大以摊薄launch和传输；
- CPU块应缓存友好且不会产生过高调度开销；
- 可按目标执行时长生成块，例如让各设备单块预计处于几十毫秒量级，但具体范围由实测决定。

## 3. 动态共享工作池

- 工作池只保存未开始范围；
- CPU与GPU从同一剩余域claim不同大小的块；
- 不预先保存CPU/GPU比例；
- 每次完成后重新读取remaining、队列和内存状态；
- 多GPU不得硬编码`cuda:0`，但当前最低验收只要求一张真实GPU。

## 4. 边际收益门

设备只有在领取下一块预计能缩短总完成时间时才继续claim。

典型规则：

- 小于`gpu_min_profitable_items`时GPU不新领host数据块；
- 数据已resident时使用更低的GPU收益阈值；
- 若CPU新块预计完成时间晚于当前GPU预计清空时间并会形成尾部，则停止CPU新claim；
- 若GPU传输+计算预计晚于CPU完成剩余任务，则停止GPU新claim；
- 设备已经领取的块不迁移。

这不是固定比例，而是防止慢设备在尾部拖累总时间。

## 5. 尾段缩块

当remaining减少时：

1. 按剩余工作和活跃设备数缩小候选块；
2. 保持最小高效块限制；
3. 如果某设备无法领取高效块，退出本轮claim；
4. 最后由预计最快的设备清尾。

禁止固定“最后30%缩块”等伪guided规则。

## 6. 非持久运行时反馈

允许使用本次运行的真实completion时间修正当前队列等待和尾段判断，但：

- 不写回OperationProfile；
- 不跨运行学习；
- 不改变冻结数值语义；
- 报告中区分离线预测与本次实际统计。

## 7. 路由报告

至少记录：

- 各设备真实items/tiles/blocks；
- H2D/D2H字节和次数；
- resident复用次数；
- 队列、传输、计算和合并耗时；
- 初始/最终块范围；
- 停止某设备claim的原因；
- coverage、失败和回退。

Schema见`schemas/execution_report.schema.json`。
