# RELEASE-02 · drizzle 算法复杂度优化实施报告 (PERF-DRZ-IMPL)

- 任务：按 PERF-DRZ 复杂度清单 C1/C2/C3（+C4/C7）落地 drizzle 重采样算法优化。
- 工作目录：`/workspace/Astro CS Database`；`TMPDIR=/dev/shm/astrocs_drzimpl`。
- 纪律：**未跑 `ninja`/`cmake`/`ctest`**；仅 `g++ -fsyntax-only`；零 git 写；未改 `docs/**`。
- 证据目录：`run/RELEASE-02/perf-drz-impl/`。
- 改动文件（3 个，全部在 `lib/algorithms/drizzle/healpix_drizzle/`）：
  `spherical_overlap.cpp`、`spherical_overlap.h`、`drizzle_engine.cpp`。

---

## 0. 结论摘要

| 项 | 状态 | 改动 | 对产物 | 预期收益（单帧重采样 33.77s 口径） |
|---|---|---|---|---|
| **C3 增量候选枚举** | **已落地** | `spherical_overlap.cpp:1719-1774` | **逐位不变** | cand CPU 254.0s → ~87s（P35 实测原语 2.92×），省 ~10.8s 墙钟 |
| **C4 每像素局部 vector 分配** | **已落地（C3 副作用）** | 同上（删除 `filtered`） | 逐位不变 | 消除 1678 万次/帧 malloc/free（约 0.8–1.7s CPU/帧） |
| **C1 LRU 命中 O(1) 触摸** | **已落地** | `.h:321-343`、`.cpp:1391-1457` | **逐位不变** | P35 实测 0.15%（0.033 µs/src）；去最坏情形热点 |
| **C7 profiler 计数按帧重置** | **已落地** | `drizzle_engine.cpp:1808-1813,2091-2098`；`spherical_overlap.cpp:45-77` | 仅观测口径 | 0（修观测 bug） |
| **C2 几何缓存容量调整** | **未落地（有意）** | — | — | 上界 ≤0.65%（P34 §4.2）；见 §2.2 决策 |
| **C5 tileMap 查找 / C6 drop 几何** | 未做 | — | — | 小；见 §6 |

> **重要口径订正**：任务书「消除 C1–C6 单帧省 4.8s（7.8×）」不成立。
> 审核包 P34/P35（`reports/review-package-20260915/08_算法推导_drizzle.md`）已实测：
> 在**只做保序改动**的前提下，单核吞吐上界 ≈ **1.9–2.0×**（§4.7），其中 **K2（增量候选枚举）
> 单独就是 1.5× 级**，几何缓存全部只值引擎 4.6%、完美复用仅再省 0.65%。
> 本轮落地的正是 P35 已裁决的「冻结边界 = K2 + 缓存 O(1)」（§7.1）。

---

## 1. 科学红线遵守

- **未触碰 stripe 划分 / stripe 内遍历序 / 归约序**：
  `drizzle_deterministic_stripe_count`（`drizzle_engine.cpp:1597`）、
  `merge_tile_map_into`（`:1611`）、`merge_cursor` 升序归约（`:1961`）全部原样。
- **候选集合逐元素、逐顺序不变**（C3 的等价性论证见 §2.3）⇒ 下游
  `for (uint64_t ipix : candidates)`（`:1512`）的逐候选累加序列不变 ⇒ 逐 leaf 的
  FP 结合树不变。
- **缓存只是加速结构**（C1/C2 的纯度论证见 §2.1）⇒ 命中/未命中取到的 geometry 逐位相同。
- **未硬编码线程/ISA/block**：未改 `num_threads`、未改 stripe 常数、未加 ISA 分支。

---

## 2. 逐项改动

### 2.1 C1：TargetGeomCache 命中触摸 O(capacity) → O(1)

**改动**
- `spherical_overlap.h:321-343`：`Entry` 增加 `lru_prev/lru_next`；删除
  `std::deque<std::uint64_t> lru_`，改为 `Entry* lru_head_/lru_tail_`；新增
  `lru_unlink/lru_push_front/lru_touch` 三个私有方法声明。
- `spherical_overlap.cpp:1391-1457`：实现三个链表原语；`get_or_build` 命中路径由
  `for (auto d = lru_.begin(); ...)` 线性扫描（旧 `:1389-1395`）改为
  `lru_touch(&it->second)`；`clear()` 改为重置 head/tail。
- **容量仍为 8192**（构造函数默认值、`run_target_cache` 均未改）。

**为何不改变输出**
1. 链表只决定「哪些 ipix 留在缓存」，任一 ipix 的 geometry 是 `(nside, ipix)` 的确定性函数：
   未命中路径 `pix2radec → radec_to_vec → get_healpix_boundary4`（`.cpp:1432-1436`）无任何
   跨调用状态；命中路径返回同一对象的同一字段。故**同一 ipix 无论命中与否，`center/boundary4` 逐位相同**。
2. 新链表与旧 deque 的 MRU/LRU 语义**逐项一致**（命中移到 front、淘汰 back、容量不变）
   ⇒ 同一输入下**命中/未命中序列完全相同**（不只是结果相同）。见 `sim_lru_equivalence.py`：
   40 万次带局部性访问，旧 deque 线性扫描版与新侵入链表版 hit/miss 序列**逐项相同**。
3. `unordered_map` 元素地址在 rehash 时稳定（标准保证：rehash 只失效迭代器，不失效元素指针/引用），
   故 `Entry*` 链表安全；未改哈希/键/淘汰容量。
4. 操作计数 `geometry_cache_hits/misses`（`drizzle_engine.cpp:1514-1525`）因此也**逐项不变**。

**预期收益**：P35 实测几何子段 1.017×、节点 0.033 µs/src（0.15%）——
容量 8192 的 `deque<uint64_t>` 扫描在 L1/L2 内，命中项因空间局部性通常就在 front 附近，
所以线性扫描实际很短。本项价值是**去掉最坏情形 O(capacity)**（P34 曾高估 20–100×，
P35 §3.2.3 已订正），不是吞吐大头。

### 2.2 C2：几何缓存容量 / 访问序

**结论：本轮不改容量，保留 8192。** 论证与依据：

1. **纯度论证（任务要求）**：几何缓存对数值是**纯加速、无副作用**。
   - 身份 = `target ipix`；值 = `TargetPixelGeometry{center, boundary4}`，由
     `(hp.getNside(), ipix)` 唯一确定（`.cpp:1432-1436` 无状态、无累加）；
   - 缓存命中/未命中两条路径返回**同一对象同一字段**（`&it->second.geom` vs
     `&e.geom`），`compute_overlap_area_g_ctx_cached`（`.cpp:1461`）之后调用的
     `overlap_area_impl` 入参逐位相同 ⇒ 返回值逐位相同；
   - 缓存**不参与**任何 `sumFlux/sumArea/sumVarNum` 累加，也不影响候选集合；
   - 跨 run 由 `run_target_cache` 的 generation 清空（`drizzle_engine.cpp:290-298`），
     nside 不同不会污染。⇒ **容量改变只改变哪些 ipix 被淘汰，不改变任何数值**。
2. **但收益上界极小**：P34 §4.2 实测「整个目标几何缓存只值 3.0 µs/src = 引擎 4.6%，
   **完美复用仅再省 0.65%**」；P34 §4.6 直接判定「单纯扩大几何缓存容量收益有限
   （8192 已打满，跨行复用距离 ~1e5）」。PERF-DRZ 报告的 35.8% miss 是事实，
   但 82.85M 次重建 × 实测构建成本 B≈0.26 µs ≈ 21.5s CPU/帧 ≈ 1.4s 墙钟（~4%），
   且其中大部分是**强制 miss**（工作集 23.9M 叶远超任何有界容量）。
3. **两条硬约束**：
   - 容量 8192 被三处文档显式钉住：`docs/architecture/CACHE_POLICY.md:9`、
     `docs/algorithms/DRIZZLE_GEOMETRY.md:77`、`docs/modules/healpix_drizzle.md:74`；
     本任务**不改 `docs/**`**，改容量会立即产生文档—代码分叉；
   - P21 内存界定：T2 auto 单帧峰值 RSS 实测 5.72–6.35 GB（本机 MemTotal 15.62 GiB），
     8192→131072 每线程 +~21 MB、16 线程 +~330 MB（`reports/review-package-20260915/09_调度问题与推导.md:32`）。
4. **决策**：保留 8192，把「O(1) 触摸」这唯一结构性缺陷（C1）修掉；容量调整留作**需前台裁决**项（§7）。

### 2.3 C3：候选枚举增量复用（去 pix2ang）

**改动**（`spherical_overlap.cpp`）
- `:1548-1572`：新增线程私有状态 `CandCell{ipix, vx, vy, vz}` /
  `CandBoxState{nside, face, x0,x1,y0,y1, stride, cells}`。
- `:1719-1774`：快速路径枚举重写：
  - 交集格（上一轮同 `nside`/`face` 的盒 ∩ 本轮盒）直接复用缓存的
    `(ipix, 中心单位向量)`；
  - 只有「本轮盒 − 上一轮盒」才做 `morton + hp.pix2ang + sin/cos`；
  - **全部新盒格仍用当前圆心重过一遍点积过滤**（P35 §3.1.1 明确的坑：
    交集里上一轮被过滤掉的格也必须用新圆心重判），再 `std::sort`；
  - `nside`/`face` 变化时自动失效；极冠/跨 face 回退路径不更新状态。

**为何不改变输出**（等价性证明）
设格 `c=(face,ix,iy)` 的 `ipix(c)=face·nside²+morton(ix,iy)` 与
`v(c)=(sin t cos p, sin t sin p, cos t)`（`(t,p)=pix2ang(ipix(c))`）都是 `c` 的确定性函数。
新实现中每个格取值来源只有两种：现场按上式重算，或从状态里复用「同一 `nside/face`、
同一 `(ix,iy)`」上一轮按同式算出的值。二者**逐位相同**。因此
- 点积判据 `v·center ≥ cos_lim`（`center` 为本轮新圆心，`cos_lim` 同式）对每格给出**同一布尔值**；
- survivors 的收集序仍是 `iy` 外层、`ix` 内层（行主序），与旧实现相同；
- 末尾 `std::sort` 升序 ⇒ 输出 `candidates` 与旧实现**逐元素、逐顺序相同**。

候选集合不变 ⇒ `:1512` 的逐候选累加序列不变 ⇒ 产物逐位不变（P15a 归约序不受影响）。
独立佐证：
- P35 落地实测（`reports/review-package-20260915/08_算法推导_drizzle.md:253`）：
  同进程 90,000 源像素 / 1,953,584 候选，`{"mismatch":0}`（集合含顺序全等）；
- 本轮状态机独立复算 `run/RELEASE-02/perf-drz-impl/sim_k2_state_equivalence.py`：
  200 trials × 300 盒 = 60,000 盒，逐格值与行主序序列与全量枚举**全等**，
  复用率 79.6%（含 1% 随机 face/nside 切换；真实 face 边界切换更少，复用率更高）。

**预期收益**：P35 落地实测候选原语 `candp 14.2652 → 4.8832 µs/src = 2.92×`
（`08_算法推导_drizzle.md:261`）。本配置（hp_res≈0.805″、源像素≈0.94″ ⇒ 7×7 盒、
每源像素盒中心平移 ~1.2 格）复用率高于 P35 的 9×9/~2 格场景 ⇒ 应 ≥2.92×。
折算 RELEASE-02 单帧：cand CPU 254.0s → ~87s，省 ~167s CPU ≈ **10.8s 墙钟**
（按实测 15.43× 并行效率），重采样 33.77s → ~23s（**~1.4×**）。

### 2.4 C4：每源像素一次局部 vector 堆分配

**改动**：C3 重写后，旧实现里的
`std::vector<uint64_t> filtered; filtered.reserve(...)`（旧 `:1670-1682`，每源像素
一次堆分配/释放，未计入 `heap_alloc`）被删除；过滤直接写入调用方的
`candidates`，中间格向量 `box_cells` 为线程私有、容量稳定后零分配。
**为何不改变输出**：纯缓冲区管理，元素与顺序同 §2.3。
**预期收益**：消除 1678 万次/帧 malloc/free（约 0.8–1.7s CPU/帧，~0.05–0.1s 墙钟）。

### 2.5 C7：profiler 计数按帧重置

**改动**
- `drizzle_engine.cpp:1808-1813`：并行累加区入口把 `g_tl_prof_cand/g_tl_prof_overlap`
  清零；`:2091-2098`：汇总读取后再清零一次（覆盖未进入累加区的线程）。
- `spherical_overlap.cpp:45-77`：`profile_overlap_path_counts` 改为
  **read-and-reset**（读取 `g_tl_n_quick/fully/dropin/sh` 后清零）；
  `spherical_overlap.h:353-357` 同步注释。
**为何不改变输出**：这四个量只用于 `[profile]` 行 stderr 打印
（`drizzle_engine.cpp:2132-2139`），不进入 `stats`、不写任何产品文件、不参与累加。
**证据**：原 bug 实测第 2 帧 `cand=531.259 ≈ 254.004+277.255`（累计），
见 `run/RELEASE-02/perf-drz/evidence_fine_profile.txt:6549-6551,6591-6593`。
**预期收益**：0（观测修复）。

---

## 3. 位级回归（P15a DRIZZLE-DET-001）

命令（未构建，使用现存二进制）：

```
export TMPDIR=/dev/shm/astrocs_drzimpl
bash lib/algorithms/drizzle/healpix_drizzle/tests/p1drz/p1drz_taskset_invariance.sh \
     build/tests/unit/p1drz/p1drz_thread_probe run/RELEASE-02/perf-drz-impl/taskset_postchange
```

结果（`run/RELEASE-02/perf-drz-impl/taskset_regression.log`）：

```
budget=1/2/4/8/16 threads=1/2/4/8/16
norm.hiss=ab85199b0acf6aa3e2a912f442fa5233679ba88ff599b06c1db43c7eda59cb62  (5 个预算全等)
[PASS] taskset 1..16 预算 drizzle 产物科学载荷 sha256 一致
```

**口径声明（重要）**：该脚本直接运行 `build/tests/unit/p1drz/p1drz_thread_probe`，
不触发构建。本轮**源码已改但未构建**（构建由前台统一做），因此上面这次 PASS 证明的是
**基线二进制**的线程预算不变式，**不是**本轮改动的产物级验证。
本轮改动的位级回归**必须由前台 `ninja` 后重跑同一脚本**（脚本本身不需改动；
它已覆盖 `query_candidate_pixels_fast` 与 `TargetGeomCache`，probe 用 nside=256）。

作为替代证据，本轮提供两条**独立于构建**的等价性复算：
- `sim_k2_state_equivalence.py`：C3 状态机逐格值与顺序全等（60,000 盒）；
- `sim_lru_equivalence.py`：C1 侵入链表与旧 deque 的 hit/miss 序列逐项相同（40 万次访问）。

---

## 4. 语法检查（允许范围）

```
g++ -fsyntax-only -O3 -DNDEBUG -std=gnu++17 -fopenmp ... spherical_overlap.cpp   # RC=0
g++ -fsyntax-only ... drizzle_engine.cpp                                          # RC=0
```
对两个消费 target（`astrocs_drizzle` 与 `lib/algorithms/drizzle` 的
`astrocs_p1_drizzle`，含 `-fPIC -fvisibility=hidden` 与不同 include 集）各跑一遍，均 RC=0。
未跑 `ninja`/`cmake`/`ctest`。

---

## 5. 预期收益汇总（单帧 RELEASE-02 · M42 T2 M1 Red）

| 分量 | 现状 | 改动后（估） | 依据 |
|---|---:|---:|---|
| 候选查询 cand CPU | 254.0s | ~87s | P35 candp 2.92× |
| 球面重叠 overlap CPU | 244.1s | ~244s | C1 仅 0.15%；C2 未改 |
| WCS | 8.2s | 8.2s | 未改 |
| **重采样 par_wall** | **33.77s** | **~23s（1.4×）** | 上两行 / 15.43× 效率 |
| HiPS 直写（串行） | 3.80s | 3.80s | 未改 |
| **单帧 drizzle_run** | **37.6s** | **~27s（1.4×）** | |
| **整跑 drizzle 阶段** | **2076.2s** | **~1480s（省 ~600s）** | 线性外推 |
| **整跑 3438s** | **3438s** | **~2840s（~1.21×）** | |

> 收益不确定度主要来自 P35 的 2.92× 是在 9×9 盒 / ~2 格平移下测得；
> RELEASE-02 是 7×7 盒 / ~1.2 格平移，复用率更高（`sim_k2` 79.6% vs P35 约 60%），
> 故候选段应 ≥2.92×，但墙钟会被内存带宽/并行效率侵蚀（P34 §5）。

---

## 6. 未做项与理由

| 项 | 理由 |
|---|---|
| **C5** `tileMap[parent]` 97.5M 次 unordered_map 查找 | 可换成开放寻址（不改 FP：不同 parent 的 leaf 互不相交，归约按 stripe 升序，`tiles` 末尾按 parent 排序；`touched` 序由首触源像素序决定）。但改动面大、需自建哈希，收益约 1–2s CPU/帧；本轮不做，留待前台评估。 |
| **C6** drop 几何逐像素重建（1678 万次） | P34 §4.3 实测整个 wrapper（drop 几何 + scatter + 计数）= 0.95 µs/src = 引擎 1.46%，绝对上界 <1.5%；且按行复用需保证与逐像素重建逐位相同，风险/收益比不划算。 |
| **C2 容量调整** | 见 §2.2：纯度已证，但收益 ≤0.65% + 三处文档钉住 8192 + P21 内存边界。 |
| **K1 切平面 2D 裁剪** | 负责人已裁决不做（唯一改变产物的一项：183 tile，尾部 31/31.9M >1e-5），见 P35 §3.3.7。 |

---

## 7. 未闭合项（需前台/负责人动作）

1. **构建 + 位级回归**：前台 `ninja` 后重跑
   `p1drz_taskset_invariance.sh`（期望仍 `ab8519…` 或等价单一 sha256）；
   并跑 drizzle 科学门（candidate oracle / freeze / variance，若已注册）。
2. **文档同步（因 C1 实现结构变化）**：`docs/architecture/CACHE_POLICY.md:9`
   现写「deque lru + unordered_map 实现」——已变为**侵入式双向链表 + unordered_map**；
   容量 8192 / 身份 / 失效 / 线程模型均不变。本轮不改 `docs/**`，请前台另起文档提交。
   （`docs/algorithms/DRIZZLE_GEOMETRY.md:77`、`docs/modules/healpix_drizzle.md:74`
   只说「LRU 8192」，仍准确；可选补一句「O(1) 触摸」。）
3. **C2 裁决**：若负责人仍要调容量，请一并批准文档同步与内存预算；
   改动是 `TargetGeomCache` 构造参数一处，纯度证明已在 §2.2。
4. **追溯登记**：本轮落地了审核包 P35 的「冻结边界（K2 + 缓存 O(1)）」；
   建议在 `docs/TRACEABILITY.csv` / 审核包 05 索引登记「已落地」。
5. **P35 的端到端干净窗口缺口（D-2）**：P35 从未取得「仅 K2+缓存 O(1)」的端到端
   A/B 数字（1.524× 是含 K1 的全补丁）。本轮构建后建议补测该干净窗口。

---

## 8. 复现 / 证据文件

| 文件 | 内容 |
|---|---|
| `run/RELEASE-02/perf-drz-impl/taskset_regression.log` | 位级回归输出（基线二进制 PASS） |
| `run/RELEASE-02/perf-drz-impl/sim_k2_state_equivalence.py` | C3 状态机等价性复算（60,000 盒，逐格全等） |
| `run/RELEASE-02/perf-drz-impl/sim_lru_equivalence.py` | C1 hit/miss 序列等价性复算（40 万次访问） |
| `run/RELEASE-02/perf-drz/evidence_fine_profile.txt` | 基线 fine profile / ops（收益折算口径来源） |
| `reports/review-package-20260915/08_算法推导_drizzle.md` | P34/P35 实测：K2 2.92×、缓存 0.15%、冻结边界 1.77× |
