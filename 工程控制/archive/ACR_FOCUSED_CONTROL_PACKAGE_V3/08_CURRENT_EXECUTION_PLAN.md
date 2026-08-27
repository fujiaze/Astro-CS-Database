# 当前执行计划

本文件是当前唯一任务计划。保持聚焦范围，不改Phase1和真实业务算法。

## 0. 基线

- 继续`feature/astrocompute-runtime`；
- 保留已通过的对象JSON读取、真实CUDA、工作池、WorkToken、private partial基础和内存预算；
- 记录本控制包SHA并拒绝旧包；
- 不新建仓库、版本分支或日期控制包。

## 1. 修复成本单位与Profile状态

- 成本计算内部全部使用ns；
- 修复`fixed_us`与`ns_per_item`交叉点1000倍单位错误；
- host路径按真实输入/输出字节和merge计算，不用统一input bytes代替D2H；
- 交叉点必须以额外实测点验证；
- `Operation.qualified`与GPU路径收益分离；
- GPU无收益但测量可信的Operation应qualified并稳定CPU路由；
- 顶层状态按全部Operation结果重算；
- 增加`qualification_reason`并强化validator；
- 保存原始样本、重复值、候选块和拟合结果。

## 2. 修正块大小和Auto claim

- 收益阈值只作GPU进入门，不修改GPU推荐块；
- 删除resident threshold压缩recommended chunk的逻辑；
- 删除“慢设备必须在最快设备5%以内”的判断；
- 对每次claim比较有/无该块的预测总完工时间；
- 依据当前remaining、队列和实测速率选择CPU/GPU块；
- CPU/GPU异速时仍可Mixed，只要能缩短makespan；
- 尾段预计拖延时停止慢设备claim。

## 3. 接通Dispatcher真实驻留路径

- 增加最小ResidencyPolicy或显式prefetch；
- 在worker启动前完成真实upload并建立device view；
- CUDA launcher根据buffer device view调用`submit_*_resident`；
- 禁止resident路径仍创建每token host vector并调用普通submit；
- 输出/accumulator可按策略继续驻留；
- 仅MaterializeHost时下载；
- 删除执行后补传输入再mark resident的逻辑；
- 报告真实bridge传输次数和字节。

## 4. 完成partial scratch契约

- ACR托管partial scratch，或在执行前返回精确required slots/bytes；
- 不允许调用者按4096等常数猜测；
- 每次attempt清零对应partial；
- retry、尾块、Mixed和设备失败均不重复累计；
- merge由注册callback或ACR明确执行并计时。

## 5. 完成staging与容量

- 维持RAM/VRAM容量控制；
- resident路径峰值不重复计算整帧；
- 若保留“pinned”名称，接入真实可复用pinned staging pool；否则改名为普通staging ledger；
- Shrink/Release/Wait/Fallback/Fail继续保持生产闭环。

## 6. 重写关键验收

新增或修正：

- 交叉点单位回归；
- Profile partial/qualified一致性；
- qualified CPU-only Operation路由；
- recommended chunk不被threshold覆盖；
- 异速CPU/GPU makespan Mixed测试；
- Dispatcher一次upload、多token resident、一次materialize测试；
- Auto性能测试必须使用qualified Operation并断言实际设备与传输；
- partial容量、attempt清零和retry测试。

当前`ResidentReuseUploadsOnce`直接调用桥接API，只保留为底层单测；当前Dense性能测试使用未qualified Operation，不能作为最终Auto Mixed证据。

## 7. Evidence

从最终实现HEAD一次性生成：源码、git HEAD、原始Benchmark样本、Profile、CTest、sanitizer、path guard和SHA完全一致。禁止源码`e107061`配Evidence`c82013e`。

## 8. 完成定义

满足`07_TEST_AND_ACCEPTANCE.md`和`CHECKLIST.md`后，ACR底层可dormant合并main。Phase1、真实积分和Drizzle接入后续另行处理。
