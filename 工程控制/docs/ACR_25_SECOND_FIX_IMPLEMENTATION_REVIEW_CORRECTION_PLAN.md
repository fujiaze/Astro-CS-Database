# Second Fix Implementation 评审专项纠正计划

本文件为当前最高优先级执行入口，覆盖 `24_SECOND_FIX_IMPLEMENTATION_CORRECTION_PLAN.md` 中已经完成或被本轮证据推翻的部分。继续在唯一分支 `feature/astrocompute-runtime` 原地修改；不得修改 AstroCS 业务算法，不得创建新仓库、版本分支或第二套 ACR。

## 0. 当前事实基线

审计对象：`AstroCS_Review_SecondFixImplementation_20260805.zip`  
源码 HEAD：`d7601233464095560ea91cbace00cfe4891bc480`

可保留基础：真实 CPU/CUDA executor、KernelRegistry/Invocation、Eligible Device Set、每设备 completion、FP64 reduction accumulator、attempt-safe WorkToken、636 项无失败 CTest、局部 MSVC ASan 和未改业务算法。

但本轮生成的 HardwareProfile **不得作为正式路由资格文件**，因为微基准存在数据竞争、CPU/GPU 工作量不等价、样本数量不足且 holdout 不合格。

## 1. 先修 Benchmark 正确性，再谈性能

### 1.1 Dot / Reduction

禁止并行 worker 直接写共享 `float dot`。必须使用：

- 每 worker 或每块独立 partial；
- FP32 输入、FP64 partial；
- 明确的树形或顺序 merge；
- 结果与高精度参考值比较；
- CPU ISA 和 GPU 必须执行同一元素数量、相同输入与相同累加策略。

### 1.2 Histogram

分成三个独立类别，不得混写：

1. thread-local bins + merge；
2. atomic bins；
3. hotspot atomic bins，明确热点分布。

每种必须有确定性整数参考结果。存在非原子共享 `++bins[bin]` 的实现不得进入 Benchmark。

### 1.3 Scatter

分开测量：

- collision-free permutation scatter；
- atomic scatter；
- controlled-hotspot scatter。

随机重复索引的非原子写属于未定义行为，必须删除。

### 1.4 统一工作量语义

新增 `BenchmarkWorkloadDescriptor`，至少包含：

```text
logical_items
width / height（二维任务）
input_bytes / output_bytes
operation_count
kernel_shape
precision
residency
boundary_mode
```

`problem_size` 不得在一个后端表示边长、另一个后端表示元素总数。CPU/GPU 同一记录必须共享同一 workload descriptor 和输入种子。

建议提交：`fix(acr): make qualification kernels race-free and workload-equivalent`

## 2. 修复 CUDA bridge 容量与分块正确性

### 2.1 独立容量记账

每个设备缓冲区必须拥有独立 capacity：

```text
d_x_capacity
d_y_capacity
d_partials_capacity
d_kernel_capacity
```

禁止 `d_x/d_y` 或 `d_partials/d_kernel` 共享一个计数器。扩容顺序不得使旧的小缓冲区被误判为足够。

必须测试：

- small → large → medium；
- x 先扩容、y 后使用；
- partials 与 kernel 尺寸交替增长；
- 重复运行和错误回收；
- `compute-sanitizer` memcheck 无越界。

### 2.2 分块卷积

当前每块以 `begin=0` 计算会把左上角结果重复写入后续区间。必须使 kernel 看到全局输出索引，或为每块构造正确 halo 和局部坐标。

验收至少覆盖：

- 强制 2、3、17 个 GPU 块；
- chunk 边界位于行中间和行末；
- 非整除 tail；
- 32×32、257×263、2048×2048；
- 随机输入，与 CPU reference 比较全部像素；
- 边界模式一致；
- 多次扩缩容后重复验证。

建议提交：`fix(acr): harden cuda bridge capacities and chunked convolution`

## 3. 重建全量 Qualification

### 3.1 quick 与 full 明确分层

- `quick` 只用于安装后冒烟和诊断，结果必须标记 `diagnostic_only=true`；
- 只有 `full` 可生成 `qualified=true` 的 HardwareProfile；
- quick 结果不得供生产自动路由使用。

### 3.2 full 最低采样

每个曲线至少：

- 3 个预热轮次；
- 7 个正式样本，建议 10–15；
- 多尺寸覆盖 L1/L2/L3、主存、显存和大任务；
- 保存每次原始耗时，而不是只保存聚合值；
- 输出 median、P95、MAD、样本数和热稳定状态。

CPU 至少测试实际支持的 Scalar、SSE/AVX、AVX2、AVX-512 与代表线程数；精度覆盖 FP32、FP64、FP32 输入/FP64 累加。

GPU 至少覆盖 resident kernel、pageable/pinned H2D/D2H、显存带宽、launch/event/alloc/sync，且分别记录 transfer-inclusive 与 resident-only。

### 3.3 不允许默认常量伪装实测

Profile 中每项必须携带：

```text
source = measured | estimated | unavailable
qualified = true | false
sample_count
confidence / mad / p95
```

固定 launch/event/alloc/merge 常量只能标 `estimated`，不能使设备或任务曲线变成 qualified。

### 3.4 真实硬件元数据

必须记录 CPU/NUMA/RAM，以及每张 GPU 的 UUID、显存、SM/CU、驱动、runtime、PCI位置和可用内存。零值不得通过 schema 资格校验。

建议提交：`feat(acr): generate full measured hardware profiles with raw records`

## 4. 统一 Profile key 和驻留语义

每条曲线 key 必须包含：

```text
operation
variant / kernel_shape
precision
accumulator_precision
backend / ISA / thread_class
residency
access_pattern
problem_shape
```

必须修正：

- Copy 与 Triad 不得混入同一无 operation 区分的 memory curve；
- GPU global-memory 不得标成 host MainMem；
- `direct:3x3:fp32` 与 `direct:default:fp32` 必须统一；
- `sum` 与 `dot` 必须是不同 Reduction 曲线；
- 找不到精确或明确允许的插值曲线时，不得静默使用无关 fallback。

建议提交：`fix(acr): align profile keys residency and operation semantics`

## 5. 修正 CostEstimator

### 5.1 Profile 可用性

`profile_available` 不得因为设备存在任意曲线或默认 submit overhead 就为真。对当前 TaskDescriptor 必须：

- 命中相同 operation/variant/precision/residency 的 qualified measured curve；或
- 明确进入保守 CPU fallback，且报告 `profile_fallback_reason`。

GPU缺少相关曲线时默认不进入 Eligible Set，不能凭理论峰值参与生产路由。

### 5.2 成本项

完整估算：

```text
T = queue_wait + launch + transfer_in + compute + transfer_out + merge + sync
```

- queue wait 必须由真实 executor queue state 提供；
- `bytes_read/bytes_written` 明确为整个任务还是单块，禁止按 chunk 重复乘导致传输成本过计；
- 数据已驻留 GPU 时不得重复计 H2D；
- 同一 `used_curve` 状态不得被后续查询覆盖；每项独立记录曲线来源和置信度。

建议提交：`fix(acr): make cost estimates task-specific and transfer-correct`

## 6. 重做 Holdout 与路由资格

必须使用未参与拟合的尺寸、线程组合和任务类别验证。单个 128K Copy 不足以资格化整个模型。

最低验收建议：

- 每类曲线至少 3 个 holdout 尺寸；
- CPU 和真实 GPU 都有 holdout；
- 中位相对误差默认 ≤ 0.25，P95 ≤ 0.50；若某类天然波动大，必须单独论证，不能全局放宽到 200%；
- CPU/GPU 完成时间排序正确率 ≥ 90%；
- 交叉点附近必须专门验证设备选择；
- 不满足门限的曲线标 `unqualified`，运行时不得用于自动路由。

当前 119.5% 误差不得判定为通过。

建议提交：`test(acr): qualify cost curves with independent holdouts`

## 7. 只保留 RAM/VRAM 容量预算与反压

CPU/GPU 50%、80%、95%、100% 精确利用率闭环已由用户撤销，不再实现、不再测试，也不作为合并门禁。删除或停用正式路径中的：

- `cpu_target_utilization`、`gpu_target_utilization`；
- token bucket、duty cycle、active permits 等仅为追踪占用率而存在的复杂控制；
- CPU/GPU利用率误差断言与持续30秒目标档测试。

必须保留并完成：

```yaml
capacity:
  ram_limit: 0.95
  vram_limit: 0.95
  ram_fixed_reserve_mib: 2048
  vram_fixed_reserve_mib: 512
```

MemoryBudget必须在每次claim前估算输入、输出、临时区、双缓冲、传输staging、partial和merge的峰值，并真实执行 `ShrinkBlock`、`StopNewClaim`、`ReleaseCache`、`UseLowMemoryPath`、`FallbackOtherDevice` 和 `Fail`。

调度器继续work-conserving，但不承诺精确CPU/GPU利用率。

建议提交：`feat(acr): enforce ram vram budgets and recoverable memory backpressure`

## 8. Sanitizer 与正确性强化

- CPU完整 ACR链运行 ASan+UBSan；适用环境运行 TSan，重点覆盖 BenchmarkDriver 的归约、Histogram、Scatter；
- CUDA运行 `compute-sanitizer --tool memcheck`，覆盖缓冲扩缩容和多块卷积；
- 可行时增加 racecheck；
- 所有外部进程设置明确 timeout；
- 未运行项只能 `SKIPPED`，不能计入通过。

建议提交：`test(acr): add race memory and chunk-boundary sanitizer coverage`

## 9. Evidence 和权威文档同步

Evidence必须在单一干净实现HEAD上于仓库外生成，并包含：

- `git rev-parse HEAD`、branch、merge-base；
- `git status --porcelain` 空；
- path guard PASS；
- 控制包 SHA 与 `25_SECOND_FIX_IMPLEMENTATION_REVIEW_CORRECTION_PLAN.md` 已加载证明；
- 全部命令、timeout、exit code；
- RawBenchmarkRecord、HardwareProfile、schema报告、holdout报告；
- RAM/VRAM预算、峰值估算与内存动作原始记录；
- CTest准确统计 636/0/8 或当次真实数字；
- Sanitizer与compute-sanitizer日志；
- UTF-8路径安全 SHA-256 清单。

仓库内工程控制入口必须同步到25号计划，不得继续保留“仓库内22号、外部25号”的双重权威状态。

建议提交：`docs(acr): produce single-head qualification evidence and sync control entry`

## 10. 推荐原子提交顺序

1. `fix(acr): make qualification kernels race-free and workload-equivalent`
2. `fix(acr): harden cuda bridge capacities and chunked convolution`
3. `feat(acr): generate full measured hardware profiles with raw records`
4. `fix(acr): align profile keys and cost-estimator transfer accounting`
5. `test(acr): add cross-device holdout and routing qualification`
6. `feat(acr): enforce ram vram budgets and recoverable memory backpressure`
7. `test(acr): add compute-sanitizer and full cpu sanitizer coverage`
8. `docs(acr): produce single-head qualification evidence`

每个提交必须可独立构建和测试；不得把大量生成物、Evidence或第三方缓存提交进源码分支。

## 11. 合并门禁

以下全部通过前禁止合并 main：

- 所有 Benchmark kernel 无数据竞争；
- CPU/GPU 工作量定义完全等价；
- full Qualification、原始记录和真实设备元数据完整；
- relevant curve key、驻留与精度一致；
- holdout误差和设备排序达到门限；
- CUDA容量管理和多块卷积通过参考结果与compute-sanitizer；
- CostEstimator传输/queue/merge成本正确，Eligible Set只使用合格曲线；
- RAM/VRAM预算、缩块、缓存释放、跨设备回退和OOM安全路径通过；
- 完整Sanitizer覆盖；
- 无失败、无TIMEOUT、SKIPPED准确；
- 单一干净HEAD Evidence、path guard PASS、仓库内外权威控制同步；
- 业务算法零修改；
- 合并后普通AstroCS无ACR副作用。
