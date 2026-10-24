# M0–M4 runtime 独立验收（validation/runtime）

本目录是 `docs/engineering/rebuild/VERIFICATION.md` 与
`docs/engineering/rebuild/standards/SCHEDULER.md` 固定构型的**可独立执行的验收包**：
每个 case 一个独立可执行程序、冻结输入、逐项断言和 JSON 证据。它只 include 公开头
`include/acsd/**`，不 include `src/`、不读 `src/runtime` 内部实现、不复制实现常量。

## 独立性约定

* 被测入口只有公开头声明的 `acsd::rt::*`（`make_services` / `open_command` /
  `RegistryBuilder` / `ProcessHost` / `reserve` / `allocate` / `make_pipe` /
  `broadcast` / `begin_async` …）。测试自己的 test process、Storage/Resources 夹具
  是**夹具**，不是生产实现。
* 判据（期望值、容差、必须出现的错误 Code）先写进各 `case.json`；测试程序只输出
  observed 原始证据，`run_cases.py` 按 case.json 判 PASS/FAIL。**比较器本身可证伪**：
  每个 case 声明 `required_checks` 与 `expected_success_count`；`successes==0`、
  `attempts < min_attempts`、必需 check 缺失或任一 FAIL 都判 `fail`，不因“无样本”
  静默通过。
* 实现尚未链接（`libruntime` 不存在或 `undefined reference to acsd::rt::*`）时，
  case 判 `pending` 并写明构建日志，**不写 observed.json，不伪造任何数值**。
* 每个 case 的 `observed.json` 是程序原始 stdout（真实运行）+ 运行命令/退出码；
  `comparison.json` 是逐项判定。失败 attempt 不覆盖旧证据（`observed/attempts/`）。

## 最终状态（M0–M4 快照）

**15 pass / 1 fail（multi01）**（16 个 case）。这是 M0–M4 的快照：M0 公共合同/注册、M1 账本与所有权、
M2 Task/pool/pipe、M3 Storage/诊断、M4 `observation.calibrate` 的 Host 全链。
M5 的其余模块接入不在本 manifest 内；后续 M5 实现变化必须重新跑本目录，
**不得修改判据去迁就实现**（发现缺陷照旧 fail）。

构建分两个目录（`build.sh` 按 `case.json` 的 `link` 字段自动选择，无需手工切换）：

```bash
# 14 个 case 链接 OFF runtime（默认 /tmp/acsd-build）
# d01_on 链接 DEV runtime（默认 /tmp/acsd-build-dev，ACSD_DEV_MODE=1）
ACSD_BUILD=/tmp/acsd-build ACSD_DEV_BUILD=/tmp/acsd-build-dev \
    bash validation/runtime/build.sh
python3 validation/runtime/run_cases.py --no-build
```

若 DEV 目录尚未构建：`cmake --build /tmp/acsd-build-dev`（或 `ninja -C /tmp/acsd-build-dev`）。
`d01_on/case.json` 的 `link` 为 `dev`；其真实 `-L` 目录记录在
`d01_on/comparison.json` 的 `build.link_command` 中。只跑 OFF 构建时 `d01_on`
会正确地报 `pending`（ON 语义不能由 OFF runtime 伪造）。

## 构建与运行

```bash
ACSD_BUILD=/tmp/acsd-build ACSD_DEV_BUILD=/tmp/acsd-build-dev \
    bash validation/runtime/build.sh             # 15 个 case，写 build/build_status.json
python3 validation/runtime/run_cases.py --no-build   # 运行 + 判定 + 汇总 manifest.json
python3 validation/runtime/run_cases.py --no-build reg01   # 指定 case
```

链接行（与任务书一致，`cal01` 额外链接 observation/operators/uncertainty）：

```text
g++ -std=c++20 -O2 -fno-fast-math -ffp-contract=off -I include -I validation/runtime/lib \
    <case>.cpp -L$ACSD_BUILD -lacsd_runtime -lacsd_model -lacsd_numerics -lacsd_storage \
    -lpthread -lzstd -o build/<case>
```

`d01_off` 是编译期 case（`-DACSD_DEV_MODE=0`，宏参数使用本构建不存在的标识符），
只要求编译成功；`d01_on` 是 `-DACSD_DEV_MODE=1` 的运行时 case。

## case 一览（状态以 `manifest.json`/`comparison.json` 为准）

| case | 合同 | 冻结构型 | 独立判据（摘要） | 当前状态 |
|---|---|---|---|---|
| `reg01` | REG01 | 两个 test process（alpha/beta）各 1 operation + codec；另一组反例 | 成功：freeze→typed invoke 两 process 都返回正确值；逐项具名拒绝：重复 process、重复 operation、缺 codec、缺 schema、TypeId 与 C++ T 不一致、缺文档、静态依赖环、未 freeze 就 make_host、未知 process/operation、错 Request 类型（run 计数器不变） | **实跑 pass（required 28/28；observed 29/29）** |
| `life01` | S-LIFE / LIFE01 | arena 关闭后仍有存活 Block Lease；cancel 时 16 MiB spill 写 | close 后未 claim 立即退、输出/写 buffer 到真正释放才退；取消不提前释放；ASan 无 UAF/双重释放；失败也有 usage | **实跑 pass 20/20（ASan）** |
| `work01` | S-WORK | 父 scope max_work=10；段1 upper=6 consume 4 并持有；段2 upper=6；段3 upper=11；段4 在 consumed=10 后 upper=1 | 段2 不并发运行、处于 waiting_resources；释放段1后段2 自动重新准入并 consume 6，总 work=10；upper=11 与 consumed=max_work 后的 upper=1 都必须立即 resource_exhausted（不允许 parked） | **实跑 pass 16/16** |
| `a01` | A01 | 全局历史 peak 100 MiB；子域 A 实际 50 MiB/10 work，子域 B 100 work；pinned⊂host | A 报 50 MiB/10 work；父域不重复计（peak 仍 100 MiB）；Lease 跨句柄保原域；子域 work 不重置祖先；pinned>host 拒绝 | **实跑 pass 23/23** |
| `floor01` | S-FLOOR | 两 stage 一 edge：输入 8+输出 8+scratch 8=24 B 同时存活，R 另 8 B；再给 23 B | 24 B 完成且实际分配 24 B；23 B 在 run 启动前 resource_exhausted（run 计数=0）；held/claimed/allocated/peak 分开 | **实跑 pass 17/17** |
| `fork01` | S-FORK | 一个 8 B 只读 Block 广播到两分支（slot=1），一分支延迟消费 | 物理分配 8 B（非 16 B）、两分支同一 Lease、慢分支不丢；join 前不消费 | **实跑 pass 17/17**（修复后） |
| `order01` | S-ORDER | key1 先 ready、key0 最小 key 后到，slot=1，H 仅够 F | key1 不能占保底 slot；最小 key 的 send 必须能进入保留 slot，receive 顺序 key0→key1 | **实跑 pass 11/11**（窗口修正后） |
| `barrier01` | S-BARRIER | E=0；stream 两 cohort 贡献 2 次 → barrier；F=max(region F) 非求和 | R+max(F)=48 准入、47 拒绝；R+sum(F)=64 的错账会被抓到；两贡献后 barrier 才看到 2；未知 region 拒绝 | **实跑 pass 20/20** |
| `multi01` | S-MULTI | P=2；`PoolConfig.dispatch_observer` 记录池派发前真实快照（sequence/command/ready/running/ceiling/control）；每 run 800 次 dispatch 事件 | 事件 i,i+1,i+2 的该 command 快照都满足可行条件时，`commands[i..i+2]` 多重集必须为 `{w1,w2,w2}`；窗口≥100、违规=0、5/5 | **实跑 pass（required 41/41；observed 42/42）**：5 次独立调用全部 5/5；每 run 可行窗口 497–757、违规 0、事件 800/run；hook 已接线（科学/控制 lane 均回调） |
| `pipe01` | PIPE01 | max_items=1 的有序 pipe；满队列、取消、EOF/failed/cancelled 三态 | 满队列 send 背压等待；consume 后完成；cancel 唤醒 receive/send；三态 PipeEnd 保原 Issue；close 后 send 拒绝；0 容量具名拒绝 | **实跑 pass 37/37**（窗口修正后） |
| `io01` | S-IO / ASYNC01 | P=1，32 MiB read + ready 计算、3×8 MiB 并行压缩、8 MiB spill append+seal | read 未终态时 compute 已执行；压缩 CPU/wall≈0.98（≤1.5）；**`written_bytes` 单独 ≥ payload（实测 8,388,872），`spill_written_bytes` 作为同一写的 spill 视图分开记录、不相加**；seal 后 8 MiB 逐字节读回 | **实跑 pass（required 15/15；observed 17/17）**，ASan 构建 |
| `b01` | B01 | spill writer，1 MiB buffer，8×1 MiB append；另含 N-2 的 +0/−0 与 NaN 位模式 seal | 8 次 append 累计元素数；seal 尺寸/读回；失败/取消无半成品；**`seal(+0.0)` 与 `seal(-0.0)` content_hash 必须不同，payload/signalling NaN 必须按 +quiet 规范化** | **实跑 pass（required 28/28；observed 30/30）**，ASan 构建 |
| `storage01` | Storage | `[7,3]` u64/f64 数组；`read(BlockRequest{6,3})`；越界 `{6,20}`/`{22,1}`/`{0,22}`/溢出 | `begin>count` 但 `begin+count<=total` 必须 ok 且字节一致；全量与跨行合法范围可读；越界/溢出必须 `invalid_request`，不得静默截断 | **实跑 pass（required 15/15；observed 15/15）**：`[7,3]` 的 `{6,3}` 在 u64/f64 上均 byte-exact，越界全部具名拒绝（04af17cb7 的 LocalStorage 行读回归） |
| `d01_off` | D01 | `ACSD_DEV_MODE=0`，宏参数为**本构建不存在的表达式/标识符**，ctx 表达式带副作用计数 | 编译通过（零求值）；运行时 ctx/payload 计数器为 0 | **实跑 pass 3/3** |
| `d01_on` | D01 | `ACSD_DEV_MODE=1`，未选 probe + 合法 PerfRecord/SnapshotPayload，D 未配置 | 未选时 probe_selected=false 且不构造 payload；try_* noexcept；两次 invoke 的 Code/value/work 相同；span id 非 0 | **实跑 pass 15/15**（对 `ACSD_DEV_MODE=1` 的 `/tmp/acsd-build-dev` 构建） |
| `cal01` | M4 C-CAL-1/2/DQ/ROOT | Host 全链；固定 raw/bias/dark/flat；两帧共享 bias/dark + 共享 `master_noise` NoiseRoot；`modules::plan_calibration` 的真实 work 上界 | C-CAL-1/2 数值、DQ NaN/Issue、NaN 冲突拒绝；`usage.work ∈ [samples, plan-declared]`；两帧必须给出共享依赖或共享 noise root 证据 | **实跑 pass（required 37/37；observed 41/41）**：`usage.work=140 ≤ plan-declared 524`；ROOT 输出 `noise.shared_factors[0].shared_id=master_flat`、influence 1/1（共享 NoiseModel 根） |

注：mosaic 实跑（工作树含 drizzle 噪声传播/CLI gain-read 修改）：normalize 先失败
（x01-00/02/03 `the Armijo backtracking did not find a decreasing step`，x01-01
`the design matrix is rank deficient`，x01-04..07 `no quad of the detection matched
the catalogue`），因此 mosaic 停在 `missing_dependency: the product directory is missing`，
尚未到 `sky.fit`/`coadd`；该停点是具名上游失败，不判 pass。

`H01/FORMAT01`、`PROC01/SCHED01/POOL01` 不在本轮写入范围的最小必测集内（分别需要
产品格式、模块接入与 command 编排夹具），本目录不伪造其证据；如 Lead 需要，另立 case。


## 本轮真实结果（2026-10-23）

构建：`ACSD_BUILD=/tmp/acsd-build bash validation/runtime/build.sh`（16/16 built；
`d01_on` 另用 `ACSD_BUILD=/tmp/acsd-build-dev`，该目录以 `-DACSD_DEV_MODE=1` 构建）。
运行：`python3 validation/runtime/run_cases.py --no-build` → **15 pass / 1 fail（multi01）**；`life01/io01/b01` 以 `-fsanitize=address` 构建并运行（seal/abort 路径无 ASan 报告）。manifest 的 `git_commit` 记录本次运行的 HEAD（N-3）。
逐 case 证据：`<case>/observed.json`、`comparison.json`、`observed.stdout.txt`、
`observed.stderr.txt`；汇总 `manifest.json`。

| case | 实跑 | 关键数字 |
|---|---|---|
| reg01 | pass（required 28/28；observed 29/29） | 两 process freeze→invoke 42/700；全部歧义具名拒绝 |
| life01 | pass 20/20（ASan） | close 后 4096 B 仍计费；释放后归零；取消写不提前释放 |
| work01 | pass 16/16 | upper=6 等待→重新准入 consume 6；总 work=10；upper=11 与 consumed=max_work 后 upper=1 都立即 resource_exhausted |
| a01 | pass 23/23 | A peak 50 MiB/work 10；父 peak 100 MiB 不重复；pinned>host 拒绝 |
| floor01 | pass 17/17 | F=24 完成；23 在 run 前 resource_exhausted |
| fork01 | pass 17/17 | 一份 8 B 物理 Lease、两分支同值、慢分支不丢 |
| order01 | pass 11/11 | slot=1 下最小 key 先出、高 key 后出 |
| barrier01 | pass 20/20 | R+max(F)=48 准入、47 拒绝；两贡献后 barrier 见 2 |
| multi01 | pass（required 41/41；observed 42/42） | 5 次调用 5/5；每 run 800 事件、可行窗口 497–757、违规 0；observer 真实快照 |
| pipe01 | pass 37/37 | 满队列背压、三态 PipeEnd、取消唤醒、按 key 归约 |
| io01 | pass（required 15/15；observed 17/17） | read 未终态时 compute 已跑；压缩 CPU/wall≈0.98；written_bytes=8,388,872 单独判定、spill_written 分开记录（不相加）；seal 后逐字节读回 |
| b01 | pass（required 28/28；observed 30/30） | 8×append、seal/读回、失败/取消无半成品；+0/−0 hash 不同、NaN 规范化为 +quiet |
| storage01 | pass（required 15/15；observed 15/15） | `[7,3]` 的 `read{6,3}` 在 u64/f64 上 byte-exact；全量/跨行合法；越界与溢出全部 invalid_request |
| d01_off | pass 3/3 | OFF 宏零求值（未声明标识符可编译、运行时计数 0） |
| d01_on | pass 15/15 | ON 构建下 span id=5/6；探针前后 Code/value/work 相同 |
| cal01 | pass（required 37/37；observed 41/41） | 数值 C-CAL-1/2、DQ 全过；`usage.work=140` 在 plan-declared `524` 内；C-CAL-ROOT 用共享 `master_noise` 证明 `shared_id=master_flat`、influence 1/1 |

### 稳定性复跑（b01/life01/io01，5 次连续）

在 HEAD b592d88e0 的安静环境（load≈5）下各 case 连续跑 5 次：b01 **5/5 pass**、
life01 **5/5 pass**、io01 **5/5 pass**；`life01/io01/b01` 均为 `-fsanitize=address` 构建，seal/abort 路径无
AddressSanitizer 报告。本轮同时覆盖 storage cleanup claim reset、append 按 buffer 分块、
pipe_expect 回退 key 拒绝（HEAD a17b199c2）。

### 本轮修复的问题（已复测）

1. **公开头模板 P0**（`RegistryBuilder::register_type` 的 encode 2/3 参不一致、
   `add<In,Out>` finalize 的 optional 解引用）：由实现者修复，REG01/所有 typed 夹具已可编译。
2. **fixture 字节窗口**：`Item` 为 40 B，而 `order01/pipe01` 的
   `max_queued_bytes` 只有 8/16/32 B；任何符合合同的 pipe 都无法为 40 B payload
   取得 credit。已把相关 edge 提到 64 B（window=2 用 128 B），判据不变；
   实现同时改为 `bytes > max_queued_bytes` 立即 `resource_exhausted` 而不是挂起。
3. **broadcast<Block> 永不投递 P0**：实现修复后 `fork01` 17/17。
4. **取消不唤醒 ready 队列任务 P0**：用 `repros/cancel_drain.cpp` 复测，
   最新 libruntime 下 20 个任务全部以 ok/cancelled 终态返回（之前第 4 个 state=2
   永久阻塞）。
5. **M4 plan/run 落地**：plan 不再返回 `unsupported: not integrated before M4`；
   Host 全链可运行，C-CAL-1/2/DQ 数值与 DQ 规则全过；`plan_declared_work=524`
   覆盖 `usage.work=140`。C-CAL-ROOT 用真实共享 `master_noise` NoiseRoot 证明
   `shared_id=master_flat`、influence 1/1，`cal01` 41/41 pass。
6. **REG01 fixture 对齐新 registry 合同**：每个 process 唯一 document
   （`validation/runtime/reg01/docs/<process_id>.md`）且至少 1 个 operation；
   负例（duplicate/missing schema 等）也补齐 operation，使拒绝原因仍是被测规则本身。

### S-MULTI 的判据与测量（dispatch_observer；pass）

判据（未降低 1:2）：
* `PoolConfig.dispatch_observer` 记录池派发前的真实快照
  `{sequence, command, ready, running, ceiling, control}`；科学/控制 lane 都回调。
* 按 sequence 排序；滑动窗口 i,i+1,i+2 的三次事件快照都满足
  `ready1>=1 && running1<ceiling1 && ready2>=2 && running2<ceiling2` 时，要求
  `commands[i..i+2]` 多重集 == `{w1,w2,w2}`；窗口≥100、违规=0、5/5。
* 不可行窗口（缺 ready 或某 command 达 ceiling）skip 并计数，不判违规；不做固定三元分组。

实跑（HEAD b592d88e0，observer 已接线）：5 次独立调用 ×5 run 全部 5/5；每 run 800 个
dispatch 事件、可行窗口 497–757、违规 0；窗口 0 拒绝、`ratio_info≈2.0`、per-command
并发 ≤ `ceiling.cpu_tokens=2`。此前“hook 未接线/0 事件”的 fail 已由该提交修复。

### 稳定性复跑（b01/life01/io01，5 次连续）

在 HEAD b592d88e0 的安静环境（load≈5）下各 case 连续跑 5 次：b01 **5/5 pass**、
life01 **5/5 pass**、io01 **5/5 pass**；`life01/io01/b01` 均为 `-fsanitize=address` 构建，seal/abort 路径无
AddressSanitizer 报告。本轮同时覆盖 storage cleanup claim reset、append 按 buffer 分块、
pipe_expect 回退 key 拒绝（HEAD a17b199c2）。

### 本轮修复的问题（已复测）

1. **公开头模板 P0**（`RegistryBuilder::register_type` 的 encode 2/3 参不一致、
   `add<In,Out>` finalize 的 optional 解引用）：由实现者修复，REG01/所有 typed 夹具已可编译。
2. **fixture 字节窗口**：`Item` 为 40 B，而 `order01/pipe01` 的
   `max_queued_bytes` 只有 8/16/32 B；任何符合合同的 pipe 都无法为 40 B payload
   取得 credit。已把相关 edge 提到 64 B（window=2 用 128 B），判据不变；
   实现同时改为 `bytes > max_queued_bytes` 立即 `resource_exhausted` 而不是挂起。
3. **broadcast<Block> 永不投递 P0**：实现修复后 `fork01` 17/17。
4. **取消不唤醒 ready 队列任务 P0**：用 `repros/cancel_drain.cpp` 复测，
   最新 libruntime 下 20 个任务全部以 ok/cancelled 终态返回（之前第 4 个 state=2
   永久阻塞）。
5. **M4 plan/run 落地**：plan 不再返回 `unsupported: not integrated before M4`；
   Host 全链可运行，C-CAL-1/2/DQ 数值与 DQ 规则全过；`plan_declared_work=524`
   覆盖 `usage.work=140`。C-CAL-ROOT 用真实共享 `master_noise` NoiseRoot 证明
   `shared_id=master_flat`、influence 1/1，`cal01` 41/41 pass。
6. **REG01 fixture 对齐新 registry 合同**：每个 process 唯一 document
   （`validation/runtime/reg01/docs/<process_id>.md`）且至少 1 个 operation；
   负例（duplicate/missing schema 等）也补齐 operation，使拒绝原因仍是被测规则本身。

### S-MULTI 的判据与测量（dispatch_observer；当前被 hook 缺口阻塞）

按 Lead 的公开 hook 判据实现：
* `PoolConfig.dispatch_observer` 记录池派发前的真实快照 `{sequence, command, ready, running, ceiling}`；
  不推断 callable 入口状态。
* 按 sequence 排序；滑动窗口 i,i+1,i+2 三次事件的该 command 快照都满足
  `ready1>=1 && running1<ceiling1 && ready2>=2 && running2<ceiling2` 时，要求
  `commands[i..i+2]` 多重集 == `{w1,w2,w2}`；窗口≥100、违规=0、5/5。

**当前阻塞（HEAD a17b199c2）**：`DispatchEvent` 在
`src/runtime/pool.cpp:1153` 被构造，但源码中没有任何 `dispatch_observer` 的存储或调用
（`grep -rn dispatch_observer src include` 只有头文件声明），因此 hook 永远不触发。
multi01 每次 run 记录 `multi.runX_observer_delivered=false`、0 个事件，无法判定；保持 fail。
此前基于 callable 入口 + queued 上限的滑动窗口曾 25/25 通过，但那不是 hook 证据，已被本次
observer fixture 取代。需要 pool 在派发前调用 observer（控制 lane 也回调）后复跑。

### 实现侧已定位并修正的观察（复跑确认）

* **work 拒绝路径（原“第 5 个操作挂起”）**：`work` 是祖先聚合的**累计消费**，
  按 SCHEDULER 不因新 invocation 重置；`max_work=1000` 时 5×~200 恰好耗尽，
  后续操作本来就必须被拒。真实缺陷是拒绝路径被当成可重试：`units_can_ever_fit`
  只查 `units > max_work`，没有查“已消费 work + units 超限永不可恢复”，于是任务被
  parked。实现已修为 `work + units > max_work` → 立即 `resource_exhausted`。
  `work01` 新增独立断言 `work.consumed_exhausted_named_rejection`：
  `consumed=max_work` 后 `upper=1` 必须立即 `resource_exhausted`（实跑
  `code=resource_exhausted: TaskSpec.work_upper exceeds the declared max_work budget`，
  无 waiting）。`cal01` 每个场景用 fresh command 只是**场景隔离**（且用较大预算），
  不是绕过该限制；同一 command 的第 5 个操作现在返回具名错误而非挂起。
* **io01 写账本**：storage 的 `finish_append` 已记录真实写量。本次实跑
  `written_bytes=8,388,608`、`spill_written_bytes=8,388,608`（同一次 8 MiB spill 写
  在两个账本字段各记一份，`written+spill=16 MiB`），seal 后读回逐字节一致；
  新增 `io.write_ledger_recorded` 断言。

### 仍记录在案、但不阻塞本批判据的观察

* `cal01` 的 `Result.dependencies` 未携带共享 id；C-CAL-ROOT 的共享根证据用
  输出 `NoiseModel` 根集合 + 未变的 bias/dark ArrayRef 表达（本实现两帧 noise 均为
  null，即未凭空造根）。

### 复现命令

```bash
# build.sh 按 case.json 的 link 字段自动选择：d01_on→DEV 目录，其余→OFF 目录
ACSD_BUILD=/tmp/acsd-build ACSD_DEV_BUILD=/tmp/acsd-build-dev \
    bash validation/runtime/build.sh
python3 validation/runtime/run_cases.py --no-build
python3 validation/runtime/run_cases.py --no-build fork01
```

## 判据的文档来源

* REG01：`VERIFICATION.md` 固定基建表 + `FOUNDATION_API.md`「计划、注册和 Host」
  （未 freeze 不能 invoke、错 Request 类型在 run 前 invalid_request、未注册 key
  为 unsupported、freeze 验证重复 key/type/依赖环/文档/schema codec）。
* S-LIFE/S-WORK/A01：`SCHEDULER.md`「资源账本的原子操作」「必须通过的实现反例」
  + `VERIFICATION.md` A01 行。
* S-FLOOR/FORK/ORDER/MULTI/IO/BARRIER：`SCHEDULER.md` 固定配置、容量算法、
  有序 pipe/广播、多 command 公平性、反例表。
* PIPE01/ASYNC01/B01/D01：`VERIFICATION.md` 两表 + `FOUNDATION_API.md`
  pipe/Storage/diagnostics 精确规则。
* C-CAL：`CALIBRATION_WALKTHROUGH.md`「固定验收输入与故障矩阵」与唯一像元核公式
  `x = (raw − b − β·(D−b 或 D)) / f`。

## 与实现者沟通的边界

本目录只写 `validation/runtime/**`；发现缺陷以最小反例 + 复现命令 + 期望/实际
消息 Lead，不改实现、不改 `CMakeLists.txt`、不 git commit/push。
