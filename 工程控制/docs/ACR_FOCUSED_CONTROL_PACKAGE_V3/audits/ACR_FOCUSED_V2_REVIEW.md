# AstroCS ACRFocusedV2 审计报告

日期：2026-08-06  
评审包：`AstroCS_Review_ACRFocusedV2_20260806.zip`  
源码HEAD：`e107061c94663d4df81497875622bebb4005b324`  
Evidence HEAD：`c82013ee902b2a2f2fa87201249dafa9e4f51d65`

## 结论

本轮修复了上一版的Profile层级解析、候选块测量、Auto前置设备门、private partial基础和桥接resident接口，方向正确。但**仍不能合并main**。当前最关键的问题已经集中到三点：成本阈值单位错误、生产驻留链没有真正接通、Mixed claim算法不能利用异速CPU/GPU。

## 已确认有效

- `nlohmann/json`层级解析代替同名字段字符串搜索；
- CPU/GPU字段、nullable阈值和指纹可以roundtrip；
- 真实CPU/CUDA executor和动态工作池仍可用；
- ForcedMixed可证明CPU和GPU都实际执行；
- reduction/drizzle合成launcher使用token私有partial；
- bridge提供persistent upload和resident submit接口；
- 603项CTest通过，compute-sanitizer memcheck/racecheck无报告错误；
- path guard为PASS；
- 顶层与Evidence SHA JSON清单可完整复核。

## 阻断项

### 1. 收益阈值仍有约1000倍单位错误

`focused_benchmark.cpp`将`cpu_fixed/res_fixed`保存为微秒，却直接除以`ns_per_item`差值。正确计算必须先将固定项乘1000转换为纳秒。

例如`pixel_reduce` Profile显示CPU斜率约0.148 ns/item、GPU约0.071 ns/item、GPU固定约95 us。真实resident交叉点应约为百万item量级，而输出阈值只有3462。`resident_chain`也存在同类低估。

### 2. Profile状态语义不一致

`build_profile()`只要standard且有GPU样本，就先写顶层`profile_state=qualified`；之后即使五个Operation只有两个qualified，顶层仍不改变。

同时`qualify()`要求至少一条GPU路径有收益，导致“测量可信但GPU永远不划算”的Operation被标记不合格。正确语义应是：Operation画像可信即可qualified；GPU路径收益用独立eligible字段表示。

### 3. 推荐块被收益阈值错误覆盖

`MixedRoutePlanner::plan()`在resident路径中执行：

```text
gpu_chunk = min(recommended_chunk, max(minimum_chunk, profitable_threshold))
```

收益阈值是整个任务是否值得启用GPU的门，不是块大小。现有Profile的4M推荐GPU块会被压到131K，直接破坏候选块实测结果。

### 4. 当前Mixed门只允许速率接近的设备共用

`should_claim()`最终判断等价于：较慢设备的ns/item不得超过最快设备约5%。这会排除最常见的“GPU快很多、CPU仍可同时处理一个较小块并提前完成”的有效Mixed。

路由应比较有/无该设备下一块时的总预测完工时间，而不是比较两设备速率是否接近。

### 5. Dispatcher没有使用resident CUDA launcher

生产`cuda_launcher()`仍为每个token创建host `std::vector`，复制子区间，并调用普通`submit_*`接口。`submit_*_resident`只在Benchmark和直接桥接单测中出现。

Dispatcher在执行完成后才调用`upload_persistent()`并更新ResidencyManager。这不能减少本次执行传输，也不代表下一次launcher会复用device buffer。

### 6. Auto驻留存在启动死路

当前Profile所有host路径均为ineligible。Auto在worker创建前因此排除GPU；GPU未执行时，Dispatcher末尾又不会调用persistent upload。结果是输入永远无法通过正常Auto调用进入resident状态。

即使外部状态被标记resident，生产launcher仍使用普通逐块传输接口。

### 7. 关键测试没有覆盖生产链

- `ResidentReuseUploadsOnce`直接调用桥接API，没有经过Dispatcher、路由器和真实launcher选择；
- `AutoMixedWithinTenPercentOfBest`使用`dense_pixel_accumulate.fp32`，而Evidence Profile中该Operation为`qualified=false`，因此测试没有证明OperationProfile驱动的Auto Mixed；
- 性能测试只检查耗时，没有强制核对实际设备集合和传输次数。

### 8. partial容量仍由调用者猜测

合成测试固定分配4096个token槽位，并依据当前min chunk推算。公共API没有提供最大槽位数或运行时托管scratch。动态缩块、配置变化或未来真实算法接入时可能越界。失败attempt也需要显式清零对应partial。

### 9. pinned目前只有账本

代码没有`cudaHostAlloc/cudaMallocHost/cudaHostRegister`。`PinnedLedger`只记录预计字节，而CUDA launcher仍使用普通`std::vector`。这可以作为容量账本，但不能宣称已经获得pinned传输性能。

### 10. Evidence仍非最终单一HEAD

评审包源码为`e107061`，Evidence为`c82013e`。`e107061`提交包含drizzle buffer role、residency input role和GPU测试串行化修复，属于代码变化而非纯文档，因此现有Evidence不能覆盖最终源码。

## 处理建议

保留当前执行链，不推倒重来。下一轮只处理：

1. 修复成本单位、资格状态和真实阈值；
2. 用makespan判断异速CPU/GPU的动态claim；
3. 将device view和resident submit真正接入Dispatcher；
4. 建立最小prefetch/KeepDevice/MaterializeHost契约；
5. 让ACR托管partial容量与attempt清零；
6. 重写真实Auto Mixed、驻留传输计数和单一HEAD Evidence。

Phase1及真实积分/Drizzle代码仍不在本轮修改范围。
