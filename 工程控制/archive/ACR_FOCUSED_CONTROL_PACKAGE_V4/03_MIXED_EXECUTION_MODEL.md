# CPU+GPU混合执行与数据通道

## 1. 分片单位

对规则逐像素任务，工作域固定为输出像素一维范围`[0, width*height)`。每个WorkToken拥有连续范围：

```text
Token = {begin, end, claimant, attempt, output_owner}
```

CPU和GPU只能写自己拥有的输出范围。

## 2. 动态claim

- CPU使用实测CPU推荐块；
- GPU使用实测GPU推荐块；
- GPU收益阈值只决定GPU是否进入，不覆盖推荐块；
- 没有固定CPU/GPU份额；
- 每次claim比较加入该块前后的预测总完工时间；
- 谁先完成谁继续claim；
- 尾段缩块；预计造成拖尾的设备停止新claim。

运行时实测只影响当前调用，不在线写回OperationProfile。

## 3. 加权积分的输入/输出数据流

输入帧栈采用frame-major连续布局：

```text
frames[f * pixel_count + p]
weights[f]
```

输入是只读共享数据：

- CPU从host读；
- GPU在worker启动前一次上传frames和weights；
- 多个GPU token复用同一device view；
- 权重变化时只上传很小的weights buffer；
- generation变化时重新上传。

输出采用独占范围：

- CPU token直接写host output对应范围；
- GPU token写device output对应范围；
- GPU完成范围可异步D2H；
- 相邻GPU范围应尽量合并物化；
- 禁止全量重复上传输入；
- 禁止CPU/GPU写同一输出像素。

## 4. GPU内部通道

推荐实现为每GPU一个executor，内部1～3个in-flight slot：

```text
stream 0: compute token A / D2H token A
stream 1: compute token B / D2H token B
stream 2: optional
```

因为输入已resident，通道重点是计算与输出物化重叠，而不是重复上传帧栈。

必须测量1、2（full可测3）个stream；没有收益时选择1，不能强制多通道。

## 5. 内存预算

加权积分每case至少估算：

```text
RAM  = frame_stack + weights + openmp_output + acr_output + validation + runtime staging
VRAM = resident_frame_stack + weights + device_output + in_flight workspace + safety reserve
```

- benchmark默认不得使用超过可用RAM/VRAM的70%；
- ACR生产预算仍按配置上限和固定余量执行；
- 超限时缩小GPU块/通道数、等待、释放缓存、回退CPU或安全跳过；
- 不得触发系统swap来制造虚假性能结果。

## 6. 报告

ExecutionReport必须记录：

- CPU/GPU实际items与chunks；
- GPU stream数与最大in-flight数；
- H2D/D2H次数与字节；
- input upload次数；
- resident reuse次数；
- CPU/GPU实际ns/item；
- recommended/actual chunk序列；
- RAM/VRAM峰值估算和动作；
- route outcome及停止某设备继续claim的原因。
