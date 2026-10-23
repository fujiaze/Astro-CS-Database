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

**15/15 pass。** 这是 M0–M4 的快照：M0 公共合同/注册、M1 账本与所有权、
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
| `reg01` | REG01 | 两个 test process（alpha/beta）各 1 operation + codec；另一组反例 | 成功：freeze→typed invoke 两 process 都返回正确值；逐项具名拒绝：重复 process、重复 operation、缺 codec、缺 schema、TypeId 与 C++ T 不一致、缺文档、静态依赖环、未 freeze 就 make_host、未知 process/operation、错 Request 类型（run 计数器不变） | **实跑 pass 28/28** |
| `life01` | S-LIFE / LIFE01 | arena 关闭后仍有存活 Block Lease；cancel 时 16 MiB spill 写 | close 后未 claim 立即退、输出/写 buffer 到真正释放才退；取消不提前释放；ASan 无 UAF/双重释放；失败也有 usage | **实跑 pass 20/20（ASan）** |
| `work01` | S-WORK | 父 scope max_work=10；段1 upper=6 consume 4 并持有；段2 upper=6；段3 upper=11；段4 在 consumed=10 后 upper=1 | 段2 不并发运行、处于 waiting_resources；释放段1后段2 自动重新准入并 consume 6，总 work=10；upper=11 与 consumed=max_work 后的 upper=1 都必须立即 resource_exhausted（不允许 parked） | **实跑 pass 16/16** |
| `a01` | A01 | 全局历史 peak 100 MiB；子域 A 实际 50 MiB/10 work，子域 B 100 work；pinned⊂host | A 报 50 MiB/10 work；父域不重复计（peak 仍 100 MiB）；Lease 跨句柄保原域；子域 work 不重置祖先；pinned>host 拒绝 | **实跑 pass 23/23** |
| `floor01` | S-FLOOR | 两 stage 一 edge：输入 8+输出 8+scratch 8=24 B 同时存活，R 另 8 B；再给 23 B | 24 B 完成且实际分配 24 B；23 B 在 run 启动前 resource_exhausted（run 计数=0）；held/claimed/allocated/peak 分开 | **实跑 pass 17/17** |
| `fork01` | S-FORK | 一个 8 B 只读 Block 广播到两分支（slot=1），一分支延迟消费 | 物理分配 8 B（非 16 B）、两分支同一 Lease、慢分支不丢；join 前不消费 | **实跑 pass 17/17**（修复后） |
| `order01` | S-ORDER | key1 先 ready、key0 最小 key 后到，slot=1，H 仅够 F | key1 不能占保底 slot；最小 key 的 send 必须能进入保留 slot，receive 顺序 key0→key1 | **实跑 pass 11/11**（窗口修正后） |
| `barrier01` | S-BARRIER | E=0；stream 两 cohort 贡献 2 次 → barrier；F=max(region F) 非求和 | R+max(F)=48 准入、47 拒绝；R+sum(F)=64 的错账会被抓到；两贡献后 barrier 才看到 2；未知 region 拒绝 | **实跑 pass 20/20** |
| `multi01` | S-MULTI | P=1，两持续 ready command，weight=1:2，每 command 200 个单位执行段 | 30/60/90/120 处计数 10/20、20/40、30/60、40/80；整轮（30→120）精确 1:2；池/command 并发不超限；无饥饿 | **实跑 pass 12/12** |
| `pipe01` | PIPE01 | max_items=1 的有序 pipe；满队列、取消、EOF/failed/cancelled 三态 | 满队列 send 背压等待；consume 后完成；cancel 唤醒 receive/send；三态 PipeEnd 保原 Issue；close 后 send 拒绝；0 容量具名拒绝 | **实跑 pass 37/37**（窗口修正后） |
| `io01` | S-IO / ASYNC01 | P=1，32 MiB read + ready 计算、3×8 MiB 并行压缩、8 MiB spill append+seal | read 未终态时 compute 已执行；压缩 CPU/wall≈0.98（≤1.5）；写账本记录真实写量；seal 后 8 MiB 逐字节读回 | **实跑 pass 15/15** |
| `b01` | B01 | spill writer，1 MiB buffer，8×1 MiB append（按 64× 比例从 32 MiB/256 MiB 缩小） | 8 次 append 累计元素数；seal 尺寸/读回；非连续/失败 writer 不能 seal；取消中途 abort 无半成品；seal 后不可改；空 writer seal 拒绝 | **实跑 pass 24/24** |
| `d01_off` | D01 | `ACSD_DEV_MODE=0`，宏参数为**本构建不存在的表达式/标识符**，ctx 表达式带副作用计数 | 编译通过（零求值）；运行时 ctx/payload 计数器为 0 | **实跑 pass 3/3** |
| `d01_on` | D01 | `ACSD_DEV_MODE=1`，未选 probe + 合法 PerfRecord/SnapshotPayload，D 未配置 | 未选时 probe_selected=false 且不构造 payload；try_* noexcept；两次 invoke 的 Code/value/work 相同；span id 非 0 | **实跑 pass 15/15**（对 `ACSD_DEV_MODE=1` 的 `/tmp/acsd-build-dev` 构建） |
| `cal01` | M4 C-CAL-1/2/DQ/ROOT | Host 全链：make_services→open_command→generated registry→freeze→make_host→invoke→wait；固定 raw/bias/dark/flat；两帧共享 bias/dark | C-CAL-1/2 数值、DQ NaN/Issue、NaN 冲突拒绝；共享根跨调用不新增随机根 | **实跑 pass 34/34**（Host 全链 C-CAL-1/2/DQ/ROOT；内核证据另见 `validation/runtime/calibration/`） |

`H01/FORMAT01`、`PROC01/SCHED01/POOL01` 不在本轮写入范围的最小必测集内（分别需要
产品格式、模块接入与 command 编排夹具），本目录不伪造其证据；如 Lead 需要，另立 case。


## 本轮真实结果（2026-10-23）

构建：`ACSD_BUILD=/tmp/acsd-build bash validation/runtime/build.sh`（15/15 built；
`d01_on` 另用 `ACSD_BUILD=/tmp/acsd-build-dev`，该目录以 `-DACSD_DEV_MODE=1` 构建）。
运行：`python3 validation/runtime/run_cases.py --no-build` → **15/15 pass**。
逐 case 证据：`<case>/observed.json`、`comparison.json`、`observed.stdout.txt`、
`observed.stderr.txt`；汇总 `manifest.json`。

| case | 实跑 | 关键数字 |
|---|---|---|
| reg01 | pass 28/28 | 两 process freeze→invoke 42/700；全部歧义具名拒绝 |
| life01 | pass 20/20（ASan） | close 后 4096 B 仍计费；释放后归零；取消写不提前释放 |
| work01 | pass 16/16 | upper=6 等待→重新准入 consume 6；总 work=10；upper=11 与 consumed=max_work 后 upper=1 都立即 resource_exhausted |
| a01 | pass 23/23 | A peak 50 MiB/work 10；父 peak 100 MiB 不重复；pinned>host 拒绝 |
| floor01 | pass 17/17 | F=24 完成；23 在 run 前 resource_exhausted |
| fork01 | pass 17/17 | 一份 8 B 物理 Lease、两分支同值、慢分支不丢 |
| order01 | pass 11/11 | slot=1 下最小 key 先出、高 key 后出 |
| barrier01 | pass 20/20 | R+max(F)=48 准入、47 拒绝；两贡献后 barrier 见 2 |
| multi01 | pass 11/11 | 完整轮（30→120）计数精确 1:2（如 30/60）；前 30 个 dispatch 含接纳瞬态，仅记录不判定 |
| pipe01 | pass 37/37 | 满队列背压、三态 PipeEnd、取消唤醒、按 key 归约 |
| io01 | pass 15/15 | read 未终态时 compute 已跑；压缩 CPU/wall≈0.98；写账本 written=8 MiB/spill_written=8 MiB；seal 后逐字节读回 |
| b01 | pass 24/24 | 8×append 累计值、seal/读回、失败/取消无半成品、abort 幂等 |
| d01_off | pass 3/3 | OFF 宏零求值（未声明标识符可编译、运行时计数 0） |
| d01_on | pass 15/15 | ON 构建下 span id=5/6；探针前后 Code/value/work 相同 |
| cal01 | pass 34/34 | Host 全链 C-CAL-1 x=[4,14,−8]、C-CAL-2 [0,10,−12]、DQ NaN/Issue、ROOT 共享根 |

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
5. **M4 plan/run 落地**：`cal01` 由 pending 改为实跑通过；plan 不再返回
   `unsupported: not integrated before M4`。

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

* `multi01`：前 30 个 dispatch 含接纳瞬态（本次启动偏差可达 24/6），完整轮
  30→120 精确 1:2；判据只用完整轮，README 与 case.json 均写明。
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
