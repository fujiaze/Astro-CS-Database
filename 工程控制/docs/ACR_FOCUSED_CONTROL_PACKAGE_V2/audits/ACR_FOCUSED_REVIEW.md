# AstroCS ACR Focused Review 审计报告

评审对象：`AstroCS_Review_ACRFocused_20260805.zip`  
评审分支：`feature/astrocompute-runtime`  
包声明 HEAD：`f8cba99e40b4c035bc86436aa803f9edb556e466`  
Evidence HEAD：`1c2ed0f5ce339c9091f278192d5b38f0b12ed05b`  
结论：**聚焦方向正确，真实CPU/CUDA执行和动态工作池可保留；但OperationProfile、真实驻留和归约/Drizzle混合正确性尚未闭合，当前不得合并main。**

## 1. 已确认有效

- 已加载最新聚焦控制包，控制包SHA与上一包一致。
- AstroCS现有业务算法、Phase1、OpenMP和正常CLI未被改动。
- KernelRegistry、KernelInvocation、CPU/CUDA DeviceExecutor和动态工作池仍可用。
- 真实GPU测试、compute-sanitizer memcheck/racecheck、path guard均有证据。
- CPU/GPU精确利用率控制已退出当前生产目标。
- RAM/VRAM预算、缩块、停止提交、释放缓存、回退和失败路径已有可执行骨架。
- CUDA容量管理和多块卷积修复未回退。

## 2. 合并阻断项

### 2.1 OperationProfile读取器会把CPU字段读入GPU字段

`qualification/focused/operation_profile.cpp`的局部读取器按字段名搜索第一个匹配项。一个operation对象内CPU和GPU都包含：

- `fixed_us`
- `ns_per_item`
- `recommended_chunk_items`
- `minimum_chunk_items`
- 误差字段

读取GPU字段时仍会命中CPU对象中的第一个同名字段。当前往返测试只检查CPU `ns_per_item`，没有检查GPU、transfer、memory和fingerprint。因此落盘Profile重新加载后，路由参数可能被静默破坏。

要求：使用可靠JSON解析器或明确的对象层级解析；完整字段往返必须逐项一致。

### 2.2 Profile被错误标记为qualified

`FocusedBenchmark::qualify()`目前只检查CPU样本数量和`standard`档，未验证：

- GPU是否对该operation真实完成全部测点；
- CPU/GPU正确性；
- 留出尺寸预测误差；
- resident曲线是否真实测量；
- host路径是否存在收益交叉点。

Profile中的`median_error_ratio`和`p95_error_ratio`均为0，但没有对应真实留出计算。不能将“未计算”写成“零误差”。

### 2.3 GPU成本曲线与收益阈值公式不成立

当前实现存在以下问题：

- `gpu.fixed_us`取最小尺寸的总耗时，同时又叠加`ns_per_item × n`，重复计入计算量；
- `min_profitable_items`仅按`launch / gpu_ns_per_item`估算，没有比较CPU耗时，也没有加入H2D、D2H和merge；
- 推荐CPU/GPU块固定为64K和1M，并非由候选块实测选择；
- Profile中五个operation的GPU `ns_per_item`均高于CPU，却仍给出有限GPU收益阈值并标记qualified。

按当前Profile在所声明host阈值处估算，GPU计算与传输总成本远高于CPU预测，说明阈值不能用于生产路由。

正确规则：只有当GPU路径的渐近边际成本低于CPU，且固定开销存在可解交叉点时，才生成有限收益阈值；否则该路径应标记为不适用。

### 2.4 “resident”不是实际驻留曲线

Benchmark将同步H2D→kernel→D2H的同一测量同时写入`gpu_ns`和`gpu_resident_ns`。运行时CUDA launcher又为每个块创建host vector、复制子区、调用同步桥并写回。

`ResidencyManager`目前只做状态记账：

- `dispatch_invocation()`按元素数而不是字节注册buffer；
- GPU参与后对每个buffer依次`mark_uploaded()`和`mark_downloaded()`，最终状态回到HostValid；
- 状态变化没有驱动后端保留真实device allocation；
- 下一次调用不能据此跳过实际上传；
- resident chain没有用底层传输计数证明跨调用复用。

因此“共享输入上传一次”和“连续算子保留显存中间结果”尚未实现。

### 2.5 归约和Drizzle-like混合路径缺少正确的私有partial协议

注册的CPU Drizzle launcher直接让多个CPU worker写同一个bins数组；CPU和GPU同时执行时也可能并发写同一host partials。该路径与控制包要求的“输出tile所有权或设备/块私有partial后明确合并”不符。

Pixel reduction使用`partials[token_id]`，但没有生产级测试证明partial容量与动态token数量、重试attempt和最终merge一致。

当前focused测试只验证：

- standalone CPU辅助函数正确；
- dense accumulate的真实Mixed。

没有对注册后的reduce/drizzle `dispatch_invocation()`做CPU-only、GPU-only、ForcedMixed和AutoMixed全路径正确性测试。

### 2.6 Auto路由没有真正执行Profile收益门

`MixedRoutePlanner::should_claim()`会允许尚未执行过的每个设备先领取一块；GPU host/resident收益阈值没有在executor进入worker集合前强制执行。小任务退化测试又没有加载OperationProfile，因此没有覆盖生产Profile下的小任务行为。

此外，慢设备连续拒绝20轮后会被强制领取清尾块，可能重新引入本应停止的慢设备。Auto模式不得为了证明Mixed而强制设备参与；只有ForcedMixed资格测试可以这样做。

### 2.7 pinned预算仍主要是测试注入

MemoryBudget结构包含pinned字段，但生产dispatcher的claim前决策主要检查RAM或VRAM，没有独立追踪实际pinned staging reservation。`sample()`还用系统RAM用量近似pinned用量，不能证明真实pinned限制已生效。

### 2.8 Evidence不是单一HEAD

包顶层源码和git refs指向`f8cba99`，Evidence git信息指向`1c2ed0f`。最终提交修正了Profile传输单位，但现有测试、sanitizer和Evidence未从最终HEAD完整重跑。

## 3. 当前状态判断

| 子系统 | 状态 |
|---|---|
| 聚焦范围与分支边界 | 通过 |
| CPU/CUDA真实执行 | 通过 |
| 动态工作池基础 | 通过 |
| Dense逐像素Mixed正确性 | 通过 |
| OperationProfile序列化/读取 | 未通过 |
| Profile资格与收益模型 | 未通过 |
| Auto收益门 | 未通过 |
| 真实显存驻留与复用 | 未通过 |
| Reduction/Drizzle Mixed合并 | 未通过 |
| RAM/VRAM预算骨架 | 部分通过 |
| pinned生产预算 | 未通过 |
| 单一HEAD Evidence | 未通过 |
| 合并main | 禁止 |

## 4. 下一轮仅做的事情

1. 修复Profile分层解析、完整roundtrip和资格写回。
2. 用CPU、GPU resident、host roundtrip三条真实曲线计算收益；无交叉点时明确禁用对应GPU路径。
3. 测试候选块大小，替代64K/1M硬编码。
4. 在worker启动前应用host/resident收益门；Auto不得强制首GPU块。
5. 将ResidencyManager接到真实device buffer缓存、generation和传输计数。
6. 为reduce/drizzle实现每token或每设备私有partial及确定性merge。
7. 将pinned staging纳入真实reservation。
8. 从一个最终干净HEAD重新生成Evidence。

不扩展通用Benchmark，不改Phase1，不接入真实积分/Drizzle业务代码。
