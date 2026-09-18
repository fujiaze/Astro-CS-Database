# RELEASE-02 · Phase2 并行化实现（PERF-P2-IMPL）报告

- 任务：按 `reports/RELEASE-02/perf-p2-analysis.md` 的方案，把 Phase2（mosaic）三个最大串行
  计算阶段并行化（S1），并消除/并行化 439 s 串行 artifact 哈希尾部（S2）。
- 范围：**只改 3 个实现文件**（`module_adapters.cpp` / `canonical_hash.cpp` / `commands.cpp`）+
  证据/测试脚本；未跑 `ninja`/`cmake`/`ctest`（构建由前台统一做），最多 `g++ -fsyntax-only`
  与独立可运行的单测；零 git 写权限。
- 工作目录：`/workspace/Astro CS Database`；证据目录：`run/RELEASE-02/perf-p2-impl/`。
- 硬约束遵守：科学数值逐位不变（§3 论证 + §5 位级测试）；线程数一律来自 Runtime lease
  （`__workers` = `P2NodeModule::execute` 的 `cap`）或 CLI 预算 `budget`，**无任何硬编码
  线程/block/ISA**；未改 `docs/**`、未改 `lib/algorithms/coverage/{include,src}/rejection.*`。

> **并发改动提示**：本会话期间 `lib/algorithms/coverage/src/sky_plane.cpp` 被**另一个并行
> agent** 修改（+123 行，gauge/惩罚零空间锚）。该文件**不是本任务改动**，本报告不涉及。

---

## 0. 结论摘要

| 项 | 状态 | 说明 |
|---|---|---|
| **S1** 三阶段并行化 | **完成** | upm_apply / reject / integrate 全部按 tile/frame 维度并行；固定 offset 写入；逐位可复现 by construction |
| **S2** 哈希尾部去冗余 + 流式 + 并行 | **完成** | 每 artifact 只读一遍；raw/.bin 单遍流式（峰值内存 O(64KiB) 而非 O(文件)）；按 artifact 并行、顺序确定 |
| **S3** sample 去重 / pass2·3 并行 | **未做（转交）** | 见 §7 未闭合项 |
| 编译门（`g++ -fsyntax-only`） | **通过** | 3 个 TU 全部 RC=0（§5.1） |
| 位级等价测试（raw 规范哈希） | **通过** | 30/30 PASS，含 64 KiB 分块边界（§5.2） |
| 端到端逐位回归 | **未跑（前台执行）** | 步骤见 §6；基线 `run/RELEASE-02/L4-rebuild/mosaic_out_w1` 仍在 |

**预期（沿用 PERF-P2 Amdahl，N=16）**：S1 整跑 1327.9 s → 694.2 s（**1.91×**）；
S2 叠加哈希尾部后 → 282.3 s（**4.70×**）。实际以 §6 端到端回归为准；考虑
`cfitsio_io_mutex` 全局锁，建议按 **12–13×/16** 而非 16× 做承诺。

---

## 1. 改动清单（file:line）

| 文件 | 位置 | 改动 |
|---|---|---|
| `lib/infrastructure/scheduler/src/module_adapters.cpp` | :4152 | 新增 `p2_parallel_for`（lease 线程数 + 原子认领 + 跨线程异常回传） |
| 同上 | :4782 | `p2_op_upm_apply` frame 级并行 + 逐 tile 流式写 bin + 上游 frame_id 复用 |
| 同上 | :5180 | `p2_op_reject` union-tile 级并行 + 固定 offset 输出 + per-worker support 句柄 |
| 同上 | :5787 | `p2_op_integrate` tile 级并行 + `sm_cursor` 串行预检 + per-worker sup/ivar 句柄 |
| `lib/infrastructure/scheduler/src/canonical_hash.cpp` | :311 起（新分支 :338） | raw 规范哈希改为**单遍流式**（两个 SHA 状态同读遍推进）；头 8 字节探测判定格式 |
| `lib/infrastructure/cli/commands.cpp` | :124 | 新增 `with_canonical_hash(row, ch)` 重载（复用已算结果）；两参版本去重复读 |
| 同上 | :1028 | `cmd_session2_run` artifact 哈希尾部：**逐 artifact 并行** + 每文件只读一遍 |
| `run/RELEASE-02/perf-p2-impl/syntax_check.py` | — | 从 `build/build.ninja` 提取真实编译参数做 `-fsyntax-only`（证据工具） |
| `run/RELEASE-02/perf-p2-impl/canonical_raw_equiv_test.cpp` | — | raw 规范哈希新旧位级等价单测（可独立编译运行） |

`git diff --stat`（本任务 3 文件）：`commands.cpp` +66/-…、`canonical_hash.cpp` +63/-…、
`module_adapters.cpp` +720/-…（含注释与证据说明）。

---

## 2. 并行切分与写入 offset 设计

### 2.1 统一并行原语 `p2_parallel_for`（module_adapters.cpp:4152）

```cpp
template <typename Fn>
static void p2_parallel_for(uint32_t workers, uint64_t n, Fn&& body);   // body(i, worker_idx)
```

- **线程数 = `workers`**，由各 op 从 `doc.value("__workers", 1)` 读取；该值由
  `P2NodeModule::execute`（:6471 `cfg2["__workers"] = cap`）写入，`cap` 来自
  `ctx.acquire_lease(host_workers)`（:6416-6417）⇒ **ThreadLease 是唯一权威**，AGENTS §5 合规。
- `workers<=1 || n<=1` 时退化为**纯串行 reference**（w=0），与旧代码逐指令一致。
- 任务用 `std::atomic<uint64_t>::fetch_add(1)` 动态认领（等价 `schedule(dynamic)`，抗负载不均）。
- worker 内异常经 `std::exception_ptr` 回传、join 后在调用线程 `rethrow`，外层
  `execute` 的 `catch(std::bad_alloc)/catch(std::exception)` 语义不变。
- `body` 第二参 = worker 下标，供每 worker 私有 scratch/句柄使用（见 reject/integrate）。

### 2.2 `p2_op_upm_apply`（module_adapters.cpp:4782）

- **并行单元 = 帧**（49 个；tile 数 5609 含于帧内）。
- 每帧私有：`sig/sup` 句柄（各自 `aio_hips_open`）、`tile_ipix`、`tile_out` 缓冲、输出 bin。
- 只读共享：`model`（`p2_upm_calibrate_block` 为 `const void*`、无 static 可变状态，
  `upm.cpp:1274-1303`）、`sky_model`（`p2_sky_plane_eval_block` 同，`sky_plane.cpp:998`）、
  `local_lut`、`nside`。
- **bin 写入 offset 设计**：文件内布局 = tile 升序 × `kP2TileLeafSpan`（2^18 元素）。
  第 t 个 tile 恰好写 `[t*kP2TileLeafSpan, (t+1)*kP2TileLeafSpan)`；实现为**逐 tile 顺序
  流式写**同一 `std::ofstream`（帧内单线程），与串行 `p2_write_bin(ft.data)` 的
  tile 序拼接**逐字节同值**，同时把每帧峰值内存从 ~239 MB 降到 ~2 MB。
- `frames_j` 在 join 后**按 paths 序**组装；`total_pixels` 按帧序整数相加。
- **frame_id 复用（顺带省 ~12 GB 读）**：sample 已在 `p2_samples.json.frame_ids` 里按
  coverage 路径序算过 `p2_frame_id`。upm_apply 仅当
  `p2_coverage.json.hips_paths == 本节点 paths` **逐元素同序相等**且全部非 0 时复用；
  否则**回退**现场 `p2_node_frame_id`。同一函数、同一路径 ⇒ 同一 fid ⇒
  `p2_corrected_f<fid>.bin` 文件名与 `p2_upm_calibrate_block` 的帧身份逐位不变。
  该守卫已用 L4 真实产物验证会命中（§5.3）。
- 失败：每帧存 `{domain, message}`，join 后按帧下标升序取**首个**失败 ⇒ 与串行首个失败一致。

### 2.3 `p2_op_reject`（module_adapters.cpp:5180）

- **并行单元 = union tile**（523 个，`std::map` 升序 = 串行遍历序）。
- **输出 offset 设计**（预分配定长，tile 升序拼接）：
  - `accepted_bin`(u8) / `nrej_bin`(u16) / `cand_u16`(u16)：第 i 个 tile 写
    `[i*tile_span, (i+1)*tile_span)`；
  - `sample_mask`(u8)：第 i 个 tile 写 `[Σ_{j<i} depth_j*tile_span, +depth_i*tile_span)`
    （depth 前缀和，串行预计算）；
  - `tiles_j` 在 join 后按 tile 升序生成（含 `sample_mask_offset`/`frame_slots`）。
- 只读共享：`acc_all`/`nrej_all`/`sample_mask_all`/`plan_cache`（`std::map` 只读）/
  `union_tiles`；kernel `p2_reject_stack_ex` 是 `(stack,plan,out)` 纯函数。
- **per-worker support 句柄**：`P2SupportReader` 每 worker 懒打开、析构关闭
  （与 `sampler.cpp:717-730` 同款契约）；上游串行 `fsup` 预开循环**保留**，仅承担
  fail-closed 校验（错误串/域与串行完全一致，且在任何 tile 处理前触发）。
- 整数计数（`acc_total/rej_low/rej_high/rej_samples/undet_low/undet_total`）按 tile
  下标存私有槽，join 后**按 tile 序**合并（整数结合律 ⇒ 精确）。
- 失败：每 tile 存 `{domain, message}`，join 后按 tile 升序取首个（tile 内首像素失败即返回）。

### 2.4 `p2_op_integrate`（module_adapters.cpp:5787）

- **并行单元 = tile**（523 个，`rej_doc["tiles"]` 升序 = reject 的 tile 序）。
- **唯一结构改动**：把原循环内的 `slot/depth` 求解与 `sample_mask` 连续性校验
  （`sm_cursor`）抽成**串行预检 pass**（O(523)），逐 tile 校验
  `sm_off == sm_cursor && sm_depth == depth && sm_off+depth*tile_span <= mask_size`
  以及 `frame_slots` 序；失败错误串与串行**逐字一致**，顺序也一致。
- **输出 offset**：`sig_bin/sup_bin/wsum_bin`(f64) 与 `nused_bin/nrej_plane`(i32) 均
  预分配 `n_tiles*tile_span`，第 i 个 tile 写 `[i*tile_span, (i+1)*tile_span)`；
  `tiles_j` 按 tile 升序生成。
- 只读共享：`acc_all`/`nrej_all`/`sample_mask_all`/`cor_index`/`snr_weights`；
  `p2_integrate_pixel` 纯函数。
- **per-worker sup/ivar 句柄**：`P2FrameReader` 每 worker 懒打开、析构关闭。
  ivar 仅在 `weight_mode==2 && !fallback && !use_snr_chain` 时打开（与原 `has_ivar` 语义一致）。
- 每 tile 私有：`tile_bufs`/`tile_v`/`sup_v`/`ivar_v`/`vals/weights/supports/accs`；
  计数器 `zero_weight_pixels/invalid_pixels/nrej_total/sample_rejected_skipped` 按 tile
  存私有槽、join 后按 tile 序合并。
- 失败：每 tile 存 `{domain, message}`，join 后按 tile 升序取首个。

### 2.5 S2 哈希尾部（canonical_hash.cpp:311+ / commands.cpp:1028）

- **去重复读**：原 `cmd_session2_run` 每 artifact 调用 `file_sha256`（pass1）+
  `with_canonical_hash`（内部再 `file_sha256` pass2 + `canonical_product_hash_file`
  整读 pass3）。现改为**每 artifact 只调用一次 `canonical_product_hash_file`**，由它同时
  给出 `integrity_sha256`（整文件 sha256）与 `canonical_sha256`；`sha256` 与
  `integrity_sha256` 字段取同一值（与旧口径同值，见 §3.4）。
- **流式**：`canonical_product_hash_file` 对 raw 分支（`*.bin` 等）改为
  **单遍 64 KiB 流式**：同时推进 `h_int`（整文件）与 `h_can`（预喂
  `kDomain + "RAW\n"`）两个 SHA 状态。判定格式只需头 8 字节，无需整文件入内存。
  FITS/.json/properties 分支**未改**（仍整读，因结构解析需要）。
- **并行**：`apaths` 按 `collect_node_artifact_paths` 的**确定顺序**取出；`N =
  min(budget, #artifacts)`（`budget = cli_affinity_cpu_count()`，非硬编码）；原子认领
  逐 artifact 哈希，结果写 `chs[i]`；组装严格按 `apaths` 序 ⇒ manifest 字段与顺序不变。
- 影响面仅 manifest 的 `sha256/integrity_sha256/canonical_sha256/size_bytes`，不改科学产品。

---

## 3. 逐位一致性论证

### 3.1 三阶段共同的「无跨 tile 浮点归约」不变量

1. **输出像素与 tile 一一对应**：第 i 个 tile 的 `tile_span` 个输出元素只由该 tile 的帧栈
   决定；跨 tile **不存在任何浮点累加/归约**（reject 的 provenance 计数、integrate 的
   `nrej_total` 等都是**整数**）。
2. **写入位置 = 串行拼接位置**：`plane[i*tile_span + p]` 恰好等于串行第 i 次
   `push_back` 的第 p 个元素 ⇒ 文件字节 = 串行文件字节（tile 划分与 tile 内遍历序不变）。
3. **tile 内遍历序不变**：像素 `p=0..tile_span-1` 逐序；帧 slot 仍为 corrected 帧升序
   （reject 的 `union_tiles[tip]` 按 f=0..N 构造；integrate 的 `it.slot` 按 f 升序）。
   样本栈顺序、权重计算顺序、`p2_integrate_pixel` 的输入序**完全不变**。
4. **整数计数**：结合律成立 ⇒ 合并顺序（tile 序）不影响结果，精确相等。
5. **JSON 组装**：`frames_j`（帧序）/ `tiles_j`（tile 序）在 join 后按原顺序生成；
   `plan_cache` 摘要、`stats` 等字段值同源。
6. **失败语义**：按处理序（帧序 / tile 序）取首个失败，错误串由同一模板 + 同一下标生成
   ⇒ 失败运行的错误信息与串行一致。

⇒ **成功运行的每个产物文件（bin 与 json）与串行逐字节一致，且与线程数/调度顺序无关。**

### 3.2 upm_apply 的额外论证

- 逐 tile 流式写：`df.write(tile_out)` 顺序执行，文件内容 = `tile_out` 按 tile 序拼接，
  与 `p2_write_bin(ft.data)`（`ft.data` 由同一 `tile_out` 逐 tile `push_back` 得到）同值。
- frame_id 复用：`p2_frame_id` 是**路径内容的纯函数**；复用来源是 sample 对**同一
  `p2_coverage.json.hips_paths[i]`** 算出的值，而守卫要求 `paths[i] == cpaths[i]` ⇒
  `up_fids[i] == p2_node_frame_id(paths[i])`。未命中守卫即现场重算，值不变。
- 每帧 `p2_corrected_f<fid>.bin` 只依赖 fid ⇒ 各 worker 写不同文件，无竞争、无顺序约束。

### 3.3 reject / integrate 的额外论证

- kernel 无 static 可变状态：`rejection.cpp` 唯一 `static` 是纯查表函数
  `astrocs_n_map_method`；`p2_integrate_pixel`（integrate.cpp）纯函数；
  `p2_validate_candidate_weights` 纯函数。⇒ 同输入同输出。
- `plan_cache` 在并行区只读（`std::map` 并发只读安全）。
- per-worker 句柄：`AioHipsDataset` 为不可变元数据（`aio_hips_reader.cpp:111-121` 无
  cfitsio 句柄字段），`read_tile_t` 每次调用在 `cfitsio_io_mutex` 内新开 `fitsfile`；
  `aio_hips_open` 的 MOC 读取亦在同一全局锁内（`aio_hips_reader.cpp:216`）。
  ⇒ 并发读/并发 open 均安全，且读数与串行完全一致（同 tile、同函数、同 row-major 布局）。

### 3.4 S2 的逐位论证（含可运行位级测试）

- **流式 raw**：`Sha256::update` 对分块长度不敏感（内部按 64 B 缓冲），
  `sha256(kDomain + "RAW\n" + bytes)` 与整读一遍的旧实现同值；
  `integrity_sha256 = sha256(bytes)` 同理。已由
  `run/RELEASE-02/perf-p2-impl/canonical_raw_equiv_test.cpp` **实测**（§5.2）。
- **去重复读**：`sha256` 字段原 = `file_sha256(path)`；新 = `ch.integrity_sha256`。
  两者都在同一文件字节上做整文件 SHA-256，读失败时同为空串。
- **并行**：结果按 `apaths` 下标存槽、按 `apaths` 序组装，`nlohmann::json` 对象为局部量，
  `canonical_hash.cpp` 无 static 可变状态 ⇒ 与串行 manifest 逐字节同值。

---

## 4. 线程数来源（AGENTS §5 合规）

| 位置 | 线程数来源 | 代码 |
|---|---|---|
| upm_apply / reject / integrate | `doc["__workers"]` = `P2NodeModule::execute` 的 lease `cap` | `module_adapters.cpp:6471` → op 内 `std::max(1u, doc.value("__workers", 1u))` |
| S2 artifact 哈希 | `budget = cli_affinity_cpu_count()` | `commands.cpp:1006` → `min(budget, #artifacts)` |
| `workers<=1` | 串行 reference 路径 | `p2_parallel_for` 早退 |

无 `hardware_concurrency`、无固定 16、无 `#pragma omp` 新引入（保留既有 `ScopedOmpWorkerInjection`
不动）、无 ISA 特化。

---

## 5. 验证与证据

### 5.1 编译门（`g++ -fsyntax-only`，用 build.ninja 的真实参数）

```bash
export TMPDIR=/dev/shm/astrocs_p2impl
cd '/workspace/Astro CS Database'
python3 run/RELEASE-02/perf-p2-impl/syntax_check.py 'lib/infrastructure/scheduler/src/module_adapters.cpp'
python3 run/RELEASE-02/perf-p2-impl/syntax_check.py 'lib/infrastructure/cli/commands.cpp'
python3 run/RELEASE-02/perf-p2-impl/syntax_check.py 'lib/infrastructure/scheduler/src/canonical_hash.cpp'
```

结果：**三个 TU 全部 RC=0**（`-DASTROCS_PROBES=1`、`-O3 -DNDEBUG -std=gnu++17 -fopenmp`、
完整 INCLUDES）。日志：`run/RELEASE-02/perf-p2-impl/syntax_module_adapters.log` 等。

### 5.2 raw 规范哈希新旧位级等价（可运行单测）

```bash
export TMPDIR=/dev/shm/astrocs_p2impl
cd '/workspace/Astro CS Database'
g++ -std=gnu++17 -O2 -Wall -Iinclude -Ithird_party \
    -Ilib/algorithms/shared -Ilib/algorithms/shared/crypto \
    -o "$TMPDIR/canon_equiv" run/RELEASE-02/perf-p2-impl/canonical_raw_equiv_test.cpp \
    lib/infrastructure/scheduler/src/canonical_hash.cpp lib/algorithms/shared/crypto/sha256.cpp
"$TMPDIR/canon_equiv" "$TMPDIR/equiv"
```

覆盖：1000003 B / 0 B / 3 B / 65536 B / 65537 B 随机 raw（分块边界）、`.json`、`properties`、
名为 `properties` 但无 `=` 的角例、最小合法 FITS。断言：raw 的
`canonical_sha256 == sha256(kDomain+"RAW\n"+bytes)`（旧整读公式）且
`integrity_sha256 == sha256(bytes)`；非 raw 分支 `format` 未回归。
结果：**ALL PASS: failures=0**（日志 `run/RELEASE-02/perf-p2-impl/canonical_raw_equiv_test.out`）。

### 5.3 frame_id 复用守卫的实产验证

对 L4 真实产物 `run/RELEASE-02/L4-rebuild/mosaic_out_w1`：

- `p2_coverage.json.hips_paths`（49）== 配置 `mosaic_49_w1.json.hips_paths`（49）⇒ 守卫命中；
- `p2_samples.json.frame_ids`（49，全部 > 0，最大值 18227555114356888004 > INT64_MAX，
  以 `number_unsigned` 存储）⇒ `get<uint64_t>()` 合法；
- 49 个 `p2_corrected_f%016x.bin` 文件名与 frame_ids **一一匹配** ⇒ 复用后文件名不变。

### 5.4 未跑项（按指令）

未跑 `ninja`/`cmake`/`ctest`/端到端 mosaic；未跑 1/N worker 一致性回归（需构建）。

---

## 6. 端到端逐位回归步骤（交前台执行）

1. 构建：`cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && ninja -C build`。
2. **1 vs 16 worker 位级回归**（用同一份 L4 输入，输出到两个新目录）：
   ```bash
   export TMPDIR=/dev/shm/astrocs_p2impl
   # 配置：复用 run/RELEASE-02/L4-rebuild/mosaic_49_w1.json，仅改 output_dir
   # A) __workers=1（串行 reference）  B) __workers=16（lease 满额）
   ```
   逐文件比对：`p2_corrected.json`、49×`p2_corrected_f*.bin`、`p2_rejection.json`、
   `p2_rejection_{accepted,nrej,candidates,sample_mask}.bin`、`p2_integrated.json`、
   `p2_integrated_*.bin` 的 `sha256sum` 必须完全相同。
3. **对 L4 基线比对**：与 `run/RELEASE-02/L4-rebuild/mosaic_out_w1` 同名文件
   `sha256sum` 比对（注意：sky_plane.cpp 被并行 agent 改动，若其 gauge 修复改变了
   `p2_sky_plane.bin`/`p2_corrected_*.bin`，需以「同一构建的 1 vs 16」为准）。
4. **哈希尾部**：比对 run manifest 中每个 artifact 的
   `sha256/integrity_sha256/canonical_sha256/size_bytes` 与旧二进制输出一致；
   记录尾部墙钟（预期 439 s → ~25–30 s @16）。
5. **资源门**：`resource_summary.json` 的 `avg_equivalent_cores` 应从 0.73 显著上升；
   探针 `sky_plane.apply.tile` / `reject.tile` / `integrate.tile` 的 `thread_id`
   应从单一值变为多值。

---

## 7. 未闭合项（hand-off）

1. **S3 未做（转交）**：
   - `sample` query/fill 双扫描去重：`module_adapters.cpp:4339`（query）与 `:4349`（fill）
     各跑一遍 `p2_sample_controls_cached`。真正的 ~50% 节省需要**缓存 pass1 结果**（或让
     函数在 `out_obs==nullptr` 时早退，仅省 pass2/3），涉及 `sampler.cpp` 接口/内部缓存，
     本任务未动以控制风险。
   - pass2（`sampler.cpp:987`）/ pass3（`:1046`）按 cell 并行：必须按 **cell 升序
     （cell 内 frame 升序）**拼接以保发射顺序。
2. **端到端逐位回归未跑**：见 §6；构建与回归由前台统一做。**在回归通过前，不应宣布
   S1/S2 的数值正确性已被端到端证实**——本报告的位级保证是「by construction + 独立单测」。
3. **per-worker 句柄的 open 开销**：reject/integrate 每 worker 懒打开全部帧的 support
   （≈16×49 次 `aio_hips_open`，其 MOC 读取被 `cfitsio_io_mutex` 串行化）。预计 <1–2 s，
   但实测未做；若成为瓶颈，可改用共享只读句柄（`AioHipsDataset` 不可变，读全在全局锁内，
   证据见 §3.3），这是与任务书「per-worker 句柄」建议的**有意偏离点**，需前台确认。
4. **`plan().work_units` 仍为 1**（`module_adapters.cpp:6407`）：本任务未改。调度器仍无
   work unit 可扇出，但三个 op 已按 lease `cap` 自并行，故墙钟收益不依赖它。若后续要让
   调度器感知真实并行度（资源门/度量诚实），需单独任务。
5. **`p2_parallel_for` 每次调用新建线程**（而非复用 Runtime 唯一 executor 池）。对
   Phase2 三次调用无实质影响；若要统一线程治理，可后续接到 `CpuHeavyExecutor`。
6. **并发文件改动**：`lib/algorithms/coverage/src/sky_plane.cpp` 由另一 agent 并行修改，
   与本任务无关；提交时须分开。

---

## 8. 复现命令汇总

```bash
export TMPDIR=/dev/shm/astrocs_p2impl; mkdir -p "$TMPDIR"
cd '/workspace/Astro CS Database'

# 语法门（3 TU）
python3 run/RELEASE-02/perf-p2-impl/syntax_check.py 'lib/infrastructure/scheduler/src/module_adapters.cpp'
python3 run/RELEASE-02/perf-p2-impl/syntax_check.py 'lib/infrastructure/cli/commands.cpp'
python3 run/RELEASE-02/perf-p2-impl/syntax_check.py 'lib/infrastructure/scheduler/src/canonical_hash.cpp'

# raw 规范哈希位级等价单测
g++ -std=gnu++17 -O2 -Wall -Iinclude -Ithird_party -Ilib/algorithms/shared \
    -Ilib/algorithms/shared/crypto -o "$TMPDIR/canon_equiv" \
    run/RELEASE-02/perf-p2-impl/canonical_raw_equiv_test.cpp \
    lib/infrastructure/scheduler/src/canonical_hash.cpp \
    lib/algorithms/shared/crypto/sha256.cpp
"$TMPDIR/canon_equiv" "$TMPDIR/equiv"

# 改动面
git diff --stat lib/infrastructure/scheduler/src/module_adapters.cpp \
                lib/infrastructure/scheduler/src/canonical_hash.cpp \
                lib/infrastructure/cli/commands.cpp
```
