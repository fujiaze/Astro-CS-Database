# 阻断项记录：PAR-002 sampler 并行可扩展性与 N-worker 正加速

## 状态：BLOCKED（真实外部/库依赖，N-worker 正加速 PASS 条件不满足）

按 AGENTS.md『BLOCKED 仅限真实外部依赖，必须记录阻塞对象、实测命令与时间，并继续其他不受阻的 Task』与
『复杂且困难，无法解决的问题可以记录为阻断项，在审核包中汇报』记录。

任务（03_TASK_DETAILS.md §E）：
> `修 sampler 生命周期、共享状态、race、异常；任务粒度与 thread budget`；
> PASS 条件：`TSan/压力/取消/重复运行；无 crash/race，N-worker 正加速`。

---

## 核心结论

在 vm-bj Linux（gcc 14，amd64）上，**N-worker 正加速无法达成**，根因是一个**库级并发读崩溃**：

- `lib/astro_image_io` 的 cfitsio 后端 `aio_hips_*` / `_IO_fread` 在多线程并发读（**即便每线程独立句柄**）时
  稳定 **SIGSEGV**。这是库/工具的确定性缺陷，非 sampler 业务逻辑缺陷。
- 因此 sampler 必须用全局串行锁（`sampler.cpp:169 g_aio_mu`）把所有 aio_hips read 串行化，否则崩溃。
- 读被串行化后，I/O 只是小占比（合成 fixture），并行 compute 只带来 ~6% 收益，**远低于"正加速"预期**；
  且该锁必须保留（删除即崩），所以 N-worker 读并行在 Linux 上不可行。

## 实测证据（时间 2026-08-28，命令与结果）

### 1) 库级并发读崩溃（最小复现，独立句柄）
`tests/backend` 外的独立驱动 `crash.cpp`：4 × std::thread，每线程 `aio_hips_open(F1.hips)` 独立句柄，
循环 `aio_hips_read_tile_f32(d, t, buf)` 50 次。结果 **3/3 次 SIGSEGV（exit 139）**：

```
$ ./crash; echo exit=$?      # 3 次
Segmentation fault      exit=139   (连续 3 次)
```
→ 并发读崩溃与"句柄隔离"无关，是 cfitsio `_IO_fread` 层并发缺陷。

### 2) 删除全局读锁 patches 后：2-worker SIGSEGV（不稳）
对 `sampler.cpp` 增加 `read_tile_pair_nolock`（own 句柄读不取 `g_aio_mu`），重编 `libphase2.a`（openmp-on），
测 `cpu_workers=2`（6 次）：

```
$ ./drv2 2; echo exit=$?
Segmentation fault   exit=139  (6 次中 5 次 SIGSEGV; 1 次跑通但结果不变)
```
→ 去掉锁后并行读即崩，验证了锁的必要性（**已还原 `sampler.cpp`，无残留修改**）。

### 3) N-worker 加速测量（原版全局锁，compute 加大）
驱动把 `patch_radius_leaf=6, background_patch_radius=16, control_grid_per_tile=12`（compute 占比加大），
`P2_ENABLE_OPENMP=ON` 的 `build/linux-openmp-on/libphase2.a`，各 3 次取平均：

```
$ ./drv3 1  -> w=1 avg=614.2ms n=3456 c=1728
$ ./drv3 2  -> w=2 avg=577.3ms n=3456 c=1728
$ ./drv3 4  -> w=4 avg=575.3ms n=3456 c=1728
```
→ 2w/4w 仅较 1w 快 ~6%（614→575ms），**远非 N-worker 正加速**；读被全局锁串行化是主因。

### 4) 确定性（1T/2T/3T/4T 结果完全一致，数值身份相同）
```
w=1 n=1536 c=768 h=2907714054744597248
w=2 n=1536 c=768 h=2907714054744597248
w=3 n=1536 c=768 h=2907714054744597248
w=4 n=1536 c=768 h=2907714054744597248
```
→ **并行不改变每 cell 结果**（共享 `cells[idx]` 写是 distinct-index + 预分配，无 reallocation）。

### 5) 串行（cpu_workers=1）TSan 干净；并行的 TSan 报告（安全分析 + 未权威排除）
- serial（`cpu_workers=1`）：TSan 报告数 `0`，`w=1 n=1536 c=768 h=2907714054744597248`。
- parallel（`cpu_workers=2`）：TSan 报告 `sampler.cpp:886-889 _omp_fn.0`（`cells[idx] = std::move(cs)` +
  OpenMP `reduction(+:sum_catalog_veto, sum_insufficient_support)`）。
  安全分析：`#pragma omp for schedule(dynamic)` 分配**互异** `c` → `idx = c*grid*grid + gy*grid+gx` **互异**，
  `cells` 预先 `resize`（无 reallocation），故写是 distinct-element，C++ 内存模型下安全；
  TSan 对 `std::vector` 元素式并行写无法证明 distinct，属**疑似误报**，但**未做 OMPT 权威排除**。

## 为什么 BLOCKED

- PASS 条件是**合取**：`无 crash/race 且 N-worker 正加速`。
- 其中 **"N-worker 正加速"在 Linux 上不可满足**：
  1) 并发 aio_hips/cfitsio 读必崩（库级），必须全局串行读；
  2) 串行读后并行 compute 仅 ~6% 收益。
- 要真正达成 N-worker 正加速，需**逐 worker 预取缓冲 + 串行化 `_IO_fread` 并 overlap compute** 的重构，
  并需在**真实 HiPS 数据**上证明 compute-bound 时并行占比能带来正加速——Linux 无真实 HiPS，
  需 Fatduck/Windows 或后续真实数据节点验证（当前 Fatduck 离线）。

## 不受阻的下一 Task
PAR-003（UPM 稀疏计算竞态与 CPU 扩展性，依赖 MON-003|ALG-005|ARCH-004，均不受本阻断影响）。
依 AGENTS.md 继续执行 PAR-003。

## 处置
本阻断保持 BLOCKED，在审核包中显式列为阻断项。若后续：
- Fatduck 上线 + 真实 HiPS：可实测 N-worker 正加速并补 PASS 证据；
- 完成逐 worker 预取 buffered reader 重构：需重新对 Oracle + determinsim 验证后转 PASS。
