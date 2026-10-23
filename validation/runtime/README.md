# M0–M3 runtime 独立验收（validation/runtime）

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

## 构建与运行

```bash
# 默认 build 目录：$ACSD_BUILD 或 /tmp/acsd-build
bash validation/runtime/build.sh                 # 编译全部 case，写 build/build_status.json
python3 validation/runtime/run_cases.py          # 构建 + 运行 + 判定 + 汇总 manifest.json
python3 validation/runtime/run_cases.py reg01    # 指定 case
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
| `reg01` | REG01 | 两个 test process（alpha/beta）各 1 operation + codec；另一组反例 | 成功：freeze→typed invoke 两 process 都返回正确值；逐项具名拒绝：重复 process、重复 operation、缺 codec、缺 schema、TypeId 与 C++ T 不一致、缺文档、静态依赖环、未 freeze 就 make_host、未知 process/operation、错 Request 类型（run 计数器不变） | **实跑 pass 28/28** |
| `life01` | S-LIFE / LIFE01 | arena 关闭后仍有存活 Block Lease；cancel 时 16 MiB spill 写 | close 后未 claim 立即退、输出/写 buffer 到真正释放才退；取消不提前释放；ASan 无 UAF/双重释放；失败也有 usage | **实跑 pass 20/20（ASan）** |
| `work01` | S-WORK | 父 scope max_work=10；段1 upper=6 consume 4 并持有；段2 upper=6；段3 upper=11；段4 在 consumed=10 后 upper=1 | 段2 不并发运行、处于 waiting_resources；释放段1后段2 自动重新准入并 consume 6，总 work=10；upper=11 具名 resource_exhausted；段4 观察记录为 finding（见下） | **实跑 pass 15/15** |
| `a01` | A01 | 全局历史 peak 100 MiB；子域 A 实际 50 MiB/10 work，子域 B 100 work；pinned⊂host | A 报 50 MiB/10 work；父域不重复计（peak 仍 100 MiB）；Lease 跨句柄保原域；子域 work 不重置祖先；pinned>host 拒绝 | **实跑 pass 23/23** |
| `floor01` | S-FLOOR | 两 stage 一 edge：输入 8+输出 8+scratch 8=24 B 同时存活，R 另 8 B；再给 23 B | 24 B 完成且实际分配 24 B；23 B 在 run 启动前 resource_exhausted（run 计数=0）；held/claimed/allocated/peak 分开 | **实跑 pass 17/17** |
| `fork01` | S-FORK | 一个 8 B 只读 Block 广播到两分支（slot=1），一分支延迟消费 | 物理分配 8 B（非 16 B）、两分支同一 Lease、慢分支不丢；join 前不消费 | **实跑 fail：broadcast<Block> 两个 receive 都不被唤醒（P0）** |
| `order01` | S-ORDER | key1 先 ready、key0 最小 key 后到，slot=1，H 仅够 F | key1 不能占保底 slot；最小 key 的 send 必须能进入保留 slot，receive 顺序 key0→key1 | **实跑 fail：key0 send 被 resource_exhausted（P1）** |
| `barrier01` | S-BARRIER | E=0；stream 两 cohort 贡献 2 次 → barrier；F=max(region F) 非求和 | R+max(F)=48 准入、47 拒绝；R+sum(F)=64 的错账会被抓到；两贡献后 barrier 才看到 2；未知 region 拒绝 | **实跑 pass 20/20** |
| `multi01` | S-MULTI | P=1，两持续 ready command，weight=1:2，每 command 200 个单位执行段 | 30/60/90/120 处计数 10/20、20/40、30/60、40/80；整轮（30→120）精确 1:2；池/command 并发不超限；无饥饿 | **实跑 pass 12/12** |
| `pipe01` | PIPE01 | max_items=1 的有序 pipe；满队列、取消、EOF/failed/cancelled 三态 | 满队列 send 背压等待；consume 后完成；cancel 唤醒 receive/send；三态 PipeEnd 保原 Issue；close 后 send 拒绝；0 容量具名拒绝 | **实跑 fail：第二次 send 直接 failed 而非等待 credit（P1），取消段未到达** |
| `io01` | S-IO / ASYNC01 | P=1，32 MiB read + ready 计算、3×8 MiB 并行压缩、8 MiB spill append+seal | read 未终态时 compute 已执行；压缩 CPU/wall≈0.98（≤1.5）；seal 后 8 MiB 逐字节读回 | **实跑 pass 14/14；写账本 0（P2 finding）** |
| `b01` | B01 | spill writer，1 MiB buffer，8×1 MiB append（按 64× 比例从 32 MiB/256 MiB 缩小） | 8 次 append 累计元素数；seal 尺寸/读回；非连续/失败 writer 不能 seal；取消中途 abort 无半成品；seal 后不可改；空 writer seal 拒绝 | **实跑 pass 24/24** |
| `d01_off` | D01 | `ACSD_DEV_MODE=0`，宏参数为**本构建不存在的表达式/标识符**，ctx 表达式带副作用计数 | 编译通过（零求值）；运行时 ctx/payload 计数器为 0 | **实跑 pass 3/3** |
| `d01_on` | D01 | `ACSD_DEV_MODE=1`，未选 probe + 合法 PerfRecord/SnapshotPayload，D 未配置 | 未选时 probe_selected=false 且不构造 payload；try_* noexcept；两次 invoke 的 Code/value/work 相同；span id 非 0 | pending：libruntime 是 OFF 构建；ON 语义需 ON 运行时 |
| `cal01` | M4 C-CAL-1/2/DQ/ROOT | Host 全链：make_services→open_command→generated registry→freeze→make_host→invoke→wait；固定 raw/bias/dark/flat；两帧共享 bias/dark | C-CAL-1/2 数值、DQ NaN/Issue、NaN 冲突拒绝；共享根跨调用不新增随机根 | pending：链已跑通，但 M4 plan hook 仍是 M0 stub（plan 返回 unsupported）；内核证据见 `validation/runtime/calibration/` |

`H01/FORMAT01`、`PROC01/SCHED01/POOL01` 不在本轮写入范围的最小必测集内（分别需要
产品格式、模块接入与 command 编排夹具），本目录不伪造其证据；如 Lead 需要，另立 case。


## 本轮真实结果与缺陷（2026-10-23）

构建：`bash validation/runtime/build.sh` → 15/15 built（`libruntime.a`/`libacsd_builtin.a` 等按
实际命名探测）。运行：`python3 validation/runtime/run_cases.py --no-build` →
**pass 10 / fail 3 / pending 2**，逐 case 证据在 `<case>/observed.json`、`comparison.json`。

### 已实跑通过

* `reg01` 28/28：两 process 登记→freeze→typed invoke；重复 process/operation、缺 codec、
  空 schema、TypeId↔C++ 不一致、空 document、依赖环、未 freeze、未知 process/operation、
  错 Request 类型全部具名拒绝；错类型在 run 前，run 计数 0。
* `life01` 20/20（ASan）：arena close 后存活 Lease 仍计费、释放后归零、取消写不提前释放；
  无 UAF/重复释放。
* `work01` 15/15：upper=6+6 在 max_work=10 下第二段 waiting_resources 且不并发执行；
  释放后自动重新准入、consume 6，总 10；upper=11 具名 resource_exhausted。
* `a01` 23/23：A 报 50 MiB/10 work、父 peak 不重复计（100 MiB 历史）、Lease 跨句柄保原域、
  pinned>host 拒绝。
* `floor01` 17/17：F=24 完成、23 在 run 前 resource_exhausted；held/claimed/allocated/peak 分离。
* `barrier01` 20/20：R+max(F)=48 准入、47 拒绝；R+sum(F)=64 的错账会被同一 case 抓到；
  两 cohort 贡献后 barrier 才看到 2。
* `multi01` 12/12：P=1 下 30/60/90/120 处 dispatch 计数为 10/20、20/40、30/60、40/80；
  稳定段（30→120）精确 1:2；池/command 并发不超限。
* `io01` 14/14：P=1 时 read 未终态而 compute 已执行；压缩 CPU/wall≈0.98；seal 后 8 MiB
  数据可逐字节读回。
* `b01` 24/24：8 次 append 累计值、seal 形状、逐字节读回、非连续/失败 writer 不能 seal、
  取消中途无半成品、abort 幂等。
* `d01_off` 3/3：`-DACSD_DEV_MODE=0` 下宏参数使用本构建不存在的标识符仍编译通过，
  运行时 ctx/payload 计数器为 0。

### 实跑失败（缺陷，最小反例与复现）

1. **P0 `broadcast<T>` 对 `Block` payload 永不投递**（`fork01`）。两个 branch 的
   receive 先挂起，broadcast 返回 Task(state=0) 后两个 receive 3 s 内都不终结，外部
   `wait_external` 永久阻塞。最小反例 `validation/runtime/repros/pipe_block.cpp`：
   两 edge（10/11）、`Pipe<Block<uint8_t>>`、post 两个 receive、broadcast 一个 8 B Block；
   `timeout 12` 后停在 `broadcast posted state=0`。
   复现：
   `g++ -std=c++20 -O1 -g -I include -c validation/runtime/repros/pipe_block.cpp -o /tmp/pb.o && g++ /tmp/pb.o -L/tmp/acsd-build -Wl,--start-group -lruntime -lmodel -lnumerics -Wl,--end-group -lpthread -lzstd -o /tmp/pblock && timeout 12 /tmp/pblock`
   期望：S-FORK 要求一个只读块广播两分支、一份物理 Lease、两份逻辑 credit 且慢分支不丢；
   实际：分支永远收不到，违反 publish permit 的提交语义。同一最小程序把 T 换成 16 B
   普通结构时正常，故问题在 Block/大 payload 的 pipe 登记（公开头没有 bounded_size 登记入口）。
2. **P1 有序 pipe window=1 时最小 key 的 send 被 `resource_exhausted`**（`order01`）。
   先 send key1（高 key）后 send key0（最小 key），key0 的 send 返回
   `resource_exhausted: the bounded pipe reorder window is full`，随后 receive 无法推进，
   高 key send 永不完成。期望：最低必需 key 有保留 slot（SCHEDULER S-ORDER/PIPE01），
   高 key 不能占保底信用。
3. **P1 队列满时第二次 send 直接 failed 而非背压等待**（`pipe01`）。max_items=1 下第一次
   send 后第二次 send 25 ms 时 state=6(failed)，不是 waiting_resources；期望
   FOUNDATION_API：“未获 credit 时不能预先产出一个未计费结果”，send 应等待 credit。
   `pipe01` 的后续取消测试因此未到达。
4. **P0（独立复现，本批 case 已绕开）取消不唤醒 ready 队列任务**。最小反例
   `validation/runtime/repros/cancel_drain.cpp`：P=1 提交 20 个任务，30 ms 后 `request_cancel`，
   逐个 `wait_external` 在第 4 个任务（state=2 ready）永久阻塞；返回的任务均为
   ok/cancelled，唯独 ready 项未终态。期望：取消唤醒所有等待者并给 queued 项 cancelled
   终态（FOUNDATION_API 取消规则、SCHEDULER 失败路径）。此缺陷会影响任何“取消后等终态”
   的排空路径。
5. **P2 写账本不更新**（`io01`）：spill writer append+seal 成功后
   `written_bytes == spill_written_bytes == 0`（峰值/租约正常、读回一致）。输出数据本身
   正确，但 ScopeSnapshot 的写账本没有记录真实写量，资源报告会低估 I/O。
6. **构建配置**：`d01_on` 需要以 `-DACSD_DEV_MODE=1` 构建的 libruntime 才能验证 ON 语义；
   当前 `/tmp/acsd-build` 是 OFF 构建，`allocate_span_id` 返回 0，故记 pending 而非伪造。
7. **M4**：`cal01` 的 Host/registry 全链已实跑（services/open_command/generated codecs/
   freeze/make_host/invoke/wait/Storage arrays/close 全部 ok，24 attempts 中 18 成功），
   但注册的 plan hook 仍是 M0 stub，返回
   `unsupported: not integrated before M4`；数值 C-CAL 门槛保持 pending，内核证据在
   `validation/runtime/calibration/`（不由本目录重复）。

### 复现命令

```bash
ACSD_BUILD=/tmp/acsd-build bash validation/runtime/build.sh
python3 validation/runtime/run_cases.py --no-build            # 全部
python3 validation/runtime/run_cases.py --no-build fork01     # 单 case
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
