# CPU+GPU动态混合路由与分块

## 1. 目标

路由目标是最小化总完工时间，不是固定份额，也不是要求CPU/GPU吞吐接近。

CPU和GPU即使速度差很多，只要较慢设备领取一个合适小块能在最快设备清空剩余工作之前完成，它就应参与；反之应停止claim避免拖尾。

## 2. worker创建前设备筛选

- Operation画像不可信：GPU不进入生产worker集合，CPU保守执行并警告；
- Operation画像可信但GPU路径无收益：GPU不进入，CPU使用OperationProfile块大小；
- host路径有收益：允许GPU处理host输入；
- resident路径有收益且已有真实设备buffer：允许resident GPU；
- ForcedMixed仅用于correctness，不可作为生产性能结论。

画像是否可信与GPU是否eligible必须使用两个独立状态，不能用`profile_available=false`混在一起。

## 3. 推荐块、最小块和阈值

- CPU使用CPU候选块实测推荐值；
- GPU使用GPU候选块实测推荐值；
- 收益阈值只决定GPU是否可进入，不改变推荐块；
- remaining较小时再做tail shrink；
- RAM/VRAM预算可进一步缩块。

## 4. claim的完工时间判断

禁止使用“慢设备速率必须在最快设备5%以内”的门。

对设备`d`拟领取块`c_d`，至少比较：

```text
T_without = 其他活跃设备完成全部remaining的预测完工时间
T_device  = queue_d + fixed_d + rate_d × c_d
T_other   = 其他设备完成remaining-c_d的预测完工时间
T_with    = max(T_device, T_other)
```

只有`T_with`比`T_without`有明确收益或不产生超出安全裕量的拖尾时，设备才领取。CPU/GPU可根据预测完工时间选择不同块大小，使两边接近同时结束。

运行时实测速率只用于当前调用的队列与尾部调整，不写回离线Profile。

## 5. 共享工作池

- 单一pending域；
- 每设备按自身块大小领取；
- 无固定CPU/GPU比例；
- 谁先完成谁继续领取；
- retry块可被其他设备领取；
- 尾段逐步缩块；
- 预计形成拖尾的设备停止新claim。

## 6. Auto自然退化

- 小任务、GPU无收益或不可用：CPU-only；
- GPU明显占优且CPU无法在清尾前完成有效块：GPU-only；
- 两边同时参与可缩短总时间：Mixed。

三种结果都属于正确的Auto混合路由。
