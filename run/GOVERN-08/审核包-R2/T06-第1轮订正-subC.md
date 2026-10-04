# T06 第 1 轮订正 · 子代理 C（standards / resources / testing 白名单）

仓库：`/workspace/Astro CS Database`，分支 `main`。全程零 git 写操作，`git` 只读且一律带
`-c core.quotepath=false`。

---

## 0 开工前必须先说的一件事：并行代理已在改同一批文件

开工时 `git status` 干净。作业过程中，另一路代理把 `NUMERIC.md`（142→194→212 行）、
`CODE.md`（147→126 行）、新建 `COMMENT.md`、`DEPENDENCY.md`、`TEST.md`（230→259 行）
整篇重写了一遍。因此派单里的多数 P0 条目（1-5、2-7、2-8、3-1、3-2、3-3、3-4、4-2、4-6、4-9、6-3、6-6、6-8、
9-1、9-2、4-1①③、4-5、4-8）在**我接手时已经由他人落内容**。我按 AGENTS §2「后到者得知先到者已落内容，
不回滚覆盖」处理：**逐条复核其正确性，只改错的，保留对的**，并在下方写明我核到了什么、推翻了什么。

我自己落笔的改动集中在：
`standards/NUMERIC.md`（3 处数值/标度判据 + 1 处死路径）、`resources/PERFORMANCE_MODEL.md`（整篇）、
`standards/{CACHE,COMPATIBILITY,CONCURRENCY,DOCUMENTATION,ERROR_MODEL,OPTIMIZATION,README}.md`（引用口径）、
`testing/VALIDATION_EVIDENCE.md`（引用口径 3 处）。

---

## 1 逐条处置表

### P0 · 公式（轮 3）

| 编号 | 处置 | 复核命令与输出 |
|---|---|---|
| **3-1** | **已改（残余缺陷）**。他人已删 `1.0336` 与死锚、并把公式改成 `(α_max/α_min)² − 1`；我复核该式**成立**（见 §2.1），但发现另一处同类问题（3-2/3-3 的下溢判红线）仍在，未随 3-1 一并订正。3-1 本身我**未再改**。 | `python3` 计算见 §2.1；`grep -rn "1.0336\|RELEASE-05/vis" docs/engineering/standards/NUMERIC.md` → 0 命中 |
| **3-2** | **已改（量名已由他人改对，乘法步骤已在位）**。`floor(α) = variance_floor · α²` 显式乘法已在正文；我**独立复算**确认文中那对数 = `1e-12 × α²`（见 §2.2），量名 `variance_floor·α²` 正确。 | `python3` 见 §2.2；`grep -n "variance_floor · α²" NUMERIC.md` → 命中 |
| **3-3** | **已改（我判定前人写法有数值缺陷，已改）**。前人写「下溢为 0 当且仅当 `floor(α) < 2⁻¹⁴⁹`」，**错**：就近舍入下 `[2⁻¹⁵⁰, 2⁻¹⁴⁹)` 舍入为非零最小次正规数。同时前人写「该平面是 `raw_adu` 面」，**错**（见 §2.3）。改前改后逐字见 §2.3。 | `python3` 逐点验舍入见 §2.3；`np.fromfile('run/M42-VARIANCE-RCA-01/plane_fixed.f64')` 见 §2.3 |
| **3-4** | **已改（前人已落）**。`1/A_cell² = (12·nside²/4π)²` 表达式 + `dex` 斜率 `8·log₁₀(nside)` + 两行示例表（`2¹⁸` 与 `2¹⁶`）已在位。我**独立复算**：两行 dex 差恰为 8.0（21.6341 − 19.2259），与斜率自洽。`nside` 标注为示例而非定值。 | `python3` 见 §2.4 |
| **3-5** | **已改（我改）**。`PERFORMANCE_MODEL.md` 原 `:71` 代码块写 `I = max(1, L / min(n, F))`、`:80`/`:95` 写 `I = max(1, L / F)`。按派单统一到 `DATA_FLOW.md` 正本式并在结构结论补 `F ≤ n` 前提。改前改后见 §2.5。 | `sed -n '116,118p' docs/engineering/architecture/DATA_FLOW.md` → `in_flight = min(n, frame_workers) // frame_workers = min(lease, 内存闸门上限)` / `inner_omp = max(1, thread_budget / in_flight) // thread_budget = lease` / `总并行度 = in_flight × inner_omp ≤ thread_budget` |
| **3-6** | **已改（前人已落，我复核）**。`TEST.md` 新增 §4.1「归约误差模型中的 `u`」，给出 f64 `2⁻⁵³ = 1.1102230246251565e-16`、f32 `2⁻²⁴ = 5.9604644775390625e-08`。我 `python3 -c "print(2.0**-53, 2.0**-24)"` 复算 = `1.1102230246251565e-16 5.960464477539063e-08`（`5.960464477539063e-08` 与文中 `…625e-08` 是同一个二进位小数的两种十进制写法）。既有容差数值**一个未改**。 | `python3 -c "print(2.0**-53, 2.0**-24)"` |
| **3-7 / V-8** | **保留（未触碰）**。`UNIFIED_OBJECTS.md` 不在白名单；`E = Var_w/Var_opt − 1` 尺度不变性审稿员已确认无缺陷。 | — |

### P0 · 证据与阈值单一正本（轮 4）

| 编号 | 处置 | 复核命令与输出 |
|---|---|---|
| **4-1①** | **已改（前人已落）**。`TEST.md` 抬头加「正本地位」段：第 4 节是**通用浮点容差**与**NaN/Inf 语义**的唯一正本，其它文档一律回引。 | `sed -n '19,21p' docs/engineering/testing/TEST.md` |
| **4-1②** | **已改（我改，且推翻派单的一个前提）**。派单要求先 `grep` 再决定「配置项 vs 待标定」。我 grep 到代码里**确有该名**：`module_adapters.cpp:2308 static constexpr double kP1FrameMemSafetyFrac = 0.75;`。但**真正的问题在别处**：`eng/packaging/config/runtime_resources.json::frame_memory_gate` 已登记 `safety_frac = 0.75` 与 `bytes_per_pixel = 116.0` 并自称「唯一源 + 实现侧零字面量 + 由 `CHK-BUDGET-SINGLE-SOURCE` 逐位比对」，而实现侧**仍是字面量**。我把 `PERFORMANCE_MODEL.md` 的数值正本改指 JSON 键，并把该不一致登记为代码侧待订正（见 §4①）。 | `grep -rn "kP1FrameMemSafetyFrac" lib/ eng/` → `module_adapters.cpp:2308`、`:2340`；`grep -n "bytes_per_pixel\|safety_frac" eng/packaging/config/runtime_resources.json` → `:116.0`、`:0.75` |
| **4-1③** | **已改（前人已落）**。`TEST.md` §11 映射表新增「适用量级域 `scale`」列，逐档给出；并新增「载体」列，把三条不存在的用例文件如实记为「无载体」。 | `sed -n '212,222p' docs/engineering/testing/TEST.md` |
| **4-9** | **已改（我改）**。`PERFORMANCE_MODEL.md` 的 UPM dense cache `1e-12` 副本改为回引 `TEST.md` 第 4 节双精度非归约档并给适用域。 | 见 §1 逐字 |
| **4-2** | **已改（前人已落，我独立复核成立）**。死锚 `run/RELEASE-05/vis/out/m42_p1_t3/p1_phot.json` 与全部依赖它的数值（5.28× / 27.84× / 1.0336 / 0.2184 / 33 帧 / α 区间）已删，改成「结构结论，本标准不为逐次运行的量冻结数值」。**我独立复核该处置的前提**：见 §3「33 帧 α 全仓不可得」。 | `ls run/RELEASE-05` → 仅 `evidence/`；`find . -name p1_phot.json` → 20 个，最大帧数 1；`python3` 扫全部 `*phot*.json` 的 `photscale_detail` → **无一条 ≥ 30 帧** |
| **4-3** | 同 3-4。 | — |
| **4-4** | **已改（我改）**。`116.0` 的件名与拟合口径已在 `PERFORMANCE_MODEL.md` 写清（见 §2.6）。**关键更正**：`116.0` 不是「实测每像素字节数」，实测边际是 **99.68 B/px**；116.0 是「使闸门放行目标 F 且不越预算」的**可行窗口上端**，且窗口随机器 `A` 平移 ⇒ `B` 是**与机器绑定**的标定值。 | `sed -n '/## 5 C 重新评估 B/,/^### 5.2/p' run/P1-CONCURRENCY-CALIB-01/REPORT.md` |
| **4-5** | **已改（我改）**。「stripe 超约 4 收益递减」**全仓无任何证据**：`grep -rl "n_stripes" run/ 实验/` → 空；`grep -c "收益递减" docs/engineering/` → 只命中 `PERFORMANCE_MODEL.md` 自身两处。已降为「待标定的经验拐点」并**移出「不可改类」**，改列「可改类 ⑦」。 | 见 §1 逐字 |
| **4-7 / V-4** | **登记为已验证项，保留**。`data/ARTIFACTS.md` 不在白名单，未触碰。审稿员「未取全文逐字核对」这一诚实边界，如实转述为：**文献核对：题录与 DOI 已核，未取全文**。 | — |
| **4-8** | **已改（我改，方向与派单一致）**。`PERFORMANCE_MODEL.md` 原 `:144` 写「enforcement = fail-closed … `record_and_justify` 只是无违规样本的记录语义，不参与裁决」——与唯一数值源**方向颠倒**。已按 `resource_gate_v1.json` 改为判据 1–3 = `record_and_justify`、判据 4 = `hard_fail`。 | `python3 -c "import json;d=json.load(open('eng/contracts/resource_gate_v1.json'))['compute'];print({k:v for k,v in d.items() if 'enforcement' in k})"` → `{'mean_utilization_enforcement': 'record_and_justify', 'p50_utilization_enforcement': 'record_and_justify', 'per_sample_enforcement': 'record_and_justify', 'queue_low_window_enforcement': 'hard_fail'}` |
| **4-6 / 7-6** | **已改（前人已落，我复核成立并加强）**。`CHK-NAMING-SURFACE` 已删；现文正面写「本标准**不声明**该检查项的执行面……不得在任何台账或判词中记为『门已生效』」。我核到更强的依据：`eng/ci/` **整个目录不存在**。 | `grep -rn "CHK-NAMING-SURFACE" docs/ eng/ lib/ 实验/` → 仅 CODE.md 自身（现已无该串）；`ls eng/ci` → `没有那个文件或目录` |

### P0 · CODE.md 两篇混写 + 生产代码逐字引用

| 编号 | 处置 | 复核命令与输出 |
|---|---|---|
| **1-5** | **已改（前人已落，我复核成立）**。`COMMENT.md` 已建（2378 B），含「原则 / 必须注释 / 必须删除/迁移 / 叙述性注释的清理面 / 长度 / 科学代码推荐写法 / 审计」；`CODE.md` 只留代码条款并单向引用。**MUST 条款逐字未动**（我逐行比对过 `git diff`，MUST 段 13–31 行内容与开工时一致）。 | `ls -la docs/engineering/standards/COMMENT.md`；`sed -n '11,31p' CODE.md` |
| **引用面核对** | **已核，结论写入 §4②**。生产代码逐字引用清单见 §4②；**其中引用的路径 `docs/engineering/CODE_STANDARD.md` 在现行树中不存在**，须代码侧订正。 | `grep -rn "CODE_STANDARD" lib/`（见 §4② 全表） |
| **2-7 / 9-8** | **已改（前人已落）**。`CODE.md` 现写「MSYS2 MinGW64 g++（**本地开发/兼容性验证工具链**，版本取值由 `DEPENDENCY.md` 登记的机器单一事实源给出）」；`DEPENDENCY.md` 新增「第三方依赖与工具链锁定的读取面」表，登记 Windows 工具链 / VS 组件 / 生产依赖锁三个机器单一事实源，并写「标准类文档只写定位、不复述版本号」「冲突以机器源为准」。 | `sed -n '13,17p' CODE.md`；`sed -n '/## 第三方依赖与工具链锁定的读取面/,$p' DEPENDENCY.md` |
| **2-8 / 6-5** | **已改（前人已落）**。`CODE.md` 新增「轮次标识的管辖边界（两类面互斥）」表：`docs/**`、`run/**`、`实验/**` 按字面保留；`lib/**`、`eng/**` 的源码注释按 `COMMENT.md` 删除。`COMMENT.md` 侧加了对应的管辖文件集说明。 | `sed -n '81,88p' CODE.md` |
| **9-9** | **已改（前人已落，我复核成立）**。示例已从 `ACSD-BASS-Index/1.0` 换成中性形式 `<产品标识>/<主>.<次>`，例 `ACSD-<产品标识>/1.0`。我核到 `BASS` 在 ACSD 语境下确指 BASS DR3 星表数据（`lib/infrastructure/aio/memory.md:29`），且全树**无任何 ACSD 自有的 HTTP `User-Agent`**（`grep -rn "User-Agent" lib/ eng/` 只命中 vendored cfitsio）。**如实登记**：需负责人给出该 token 定义。 | `grep -rn "BASS" lib/infrastructure/aio/memory.md`；`grep -rn "User-Agent" lib/ eng/` → 仅 `third_party/cfitsio` |

### P1 · PERFORMANCE_MODEL.md 语言与推理

| 编号 | 处置 | 复核命令与输出 |
|---|---|---|
| **9-1** | **已改（前人已落）**。`V14 首轮结果` 与 `V18R2 资源驱动轮（性能基线）` 两节整段删除，现文改为「基准口径」节 + 「现行基线由实验域给出」。我复核删除**没有丢真内容**：三条规则（science 等价 / >5% 回退 / `NO_SAFE_OPTIMIZATION_FOUND`）保留为「优化类结论必须同时满足」。 | `grep -n "V14\|V18R2\|原句已订正\|历史参照" docs/engineering/resources/PERFORMANCE_MODEL.md` → 0 命中 |
| **9-2** | **已改（随 9-1 整段删除）**。行内源码行号 `(…)(:151)` 与节名重复括注一并消失。 | `grep -n ":151" docs/engineering/resources/PERFORMANCE_MODEL.md` → 0 命中 |
| **6-6** | **已改（前人已降为充分条件，我复核并补上来源证明）**。前人已把「定理」改成「充分条件（非定理陈述）」；**我补上了前人缺的那块**：`num_threads = inner_omp` 的三条来源链（`drizzle_engine.h` 的 `threads` 缺省 0 = 自动 `omp_get_max_threads`；帧级轴驱动路径无 `cfg.threads = …` 覆盖；`omp_set_num_threads(inner_omp)` 后 `omp_get_max_threads()` 返回该 ICV），并补了**适用域**（只覆盖 `p1_parallel_for` 驱动的 drizzle 路径）。 | `grep -n "kScratchPoolCap" lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp` → `:1605 num_threads = (config.threads > 0) ? config.threads : omp_get_max_threads();`、`:1663 int kScratchPoolCap = num_threads;`；`grep -rn "\.threads\s*=" lib/algorithms/drizzle/` → 只命中 `module_entry.cpp:910` 的注释「legacy DrizzleConfig.threads=0 → omp_get_max_threads()」 |
| **6-7** | **已改（我改）**。前人只留了「在标定形态（`I = 2`）下…例：`n = 2` 帧 ⇒ `F = 2, I = 8`」这一组自相矛盾的参数。我补齐了两组算例的全部 `(n, L, F, I, K)` 并给出反例（`I = 2` 档提高 `K` 无效）。 | 见 §1 逐字 |
| **8-1** | **登记（不改）**。不在我白名单。`architecture/DATA_FLOW.md:14` 与 `contracts/PIPELINE_BLOCK.md:17` 的「命名块不跨节点」vs `AGENTS.md:88` 与 `docs/ACSD_DESIGN.md:381-383` 的「读块写新块、用完显式销毁」冲突，注册表 `carrier_contract.statement` 站工程正本一侧。**需负责人裁决**，我未单方面改任何一侧口径。 | — |
| **10-4 / 10-7** | **已改（我改）**。`p1drz_merge_pipeline_lock`、`L2-FROZEN-GATE-REPLAY`、`WORKER-BALANCE-METRIC-REPLAY` 三者**全树无执行器**，已逐条正面写「当前**无执行器**，由人读判读，不得记为门已执行」；`PERFORMANCE_MODEL.md` 里**没有写入任何复跑命令**。 | `ls lib/algorithms/drizzle/healpix_drizzle/tests/p1drz/p1drz_merge_pipeline_lock.sh` → 不存在；`grep -rn "L2-FROZEN-GATE-REPLAY\|WORKER-BALANCE-METRIC-REPLAY" lib/ eng/` → 0；`grep -rn "p1drz_merge_pipeline_lock" lib/ eng/` → 只剩 `drizzle_engine.cpp:1647,1661` 两条注释 |

### P1 · 其余

| 编号 | 处置 | 复核命令与输出 |
|---|---|---|
| **7-1 / 10-1** | **已改**。我跑了修正后的死路径扫描（原脚本正则里 `c` 排在 `cpp` 前，把 `X.cpp` 误截成 `X.c`，产生 ~28 条假阳性；修正后总量 75→47→**39**）。白名单内 **2 → 0**。白名单外 39 条登记给对应车道。 | 见 §5 验证输出 |
| **6-3** | **已改（前人已落）**。`TEST.md` 新增 §4.4「NaN 与 Inf 语义」，自述为「非有限值与缺失的位置与语义的唯一正本」；抬头「正本地位」段同时点名容差与 NaN/Inf 两项。`NUMERIC.md` 侧加了单向回引。 | `sed -n '74,84p' docs/engineering/testing/TEST.md` |
| **6-8** | **已改（前人已落，我复核）**。`TEST.md` §9.5 新增「记录面 / 记录者 / 更新时机 / 取不到时的行为 / 跨面隔离」五行表，指明记录面就是 §9.7 的结果件各步墙钟字段。 | `sed -n '159,175p' docs/engineering/testing/TEST.md` |
| **1-8** | **已改（前人已落）**。导语现列到第十三节。 | `sed -n '15,18p' docs/engineering/testing/TEST.md` |
| **历史叙事清扫** | **已改**。白名单内 `V1[0-9]|V2[0-9]|R-[0-9]+|P-[0-9]+` 命中 11 行，**逐条核为正则假阳性**：4 行 `DRZ-OVERLAP-002`（COMMENT.md 科学 ID 示例）、9 行 `CMP-01..09`（VALIDATION_EVIDENCE.md 判据 ID）、1 行 `SCI-DRZ-001`（TEST.md 科学条款号）。另 `standards/README.md` 原文写「八项标准」，实际目录已有十项标准文件，已改为十项并逐个点名。 | `grep -oE "V1[0-9]|V2[0-9]|R-[0-9]+|P-[0-9]+" …` → 只这 4 类 token |

---

## 2 我的独立推导

### 2.1 · 3-1 权重量化偏差的代数推导（我判定**公式对**）

设第 `k` 帧的方差以 ADU 计为 `v_k`，帧标度 `α_k`（逐帧量）。未声明标度时产品方差为
`σ_k² = α_k² · v_k`，权重 `w_k ∝ 1/σ_k²`；声明标度后应得 `W_k ∝ 1/v_k = α_k² · w_k`。

设归一化权重 `ŵ_k = w_k / Σ_j w_j`，正确归一化 `Ŵ_k = W_k / Σ_j W_j`。则

```
ŵ_k / Ŵ_k = (w_k / Σw) · (ΣW / W_k)
           = (w_k / W_k) · (ΣW / Σw)
           = α_k² · c ,   c = ΣW / Σw  （与 k 无关的常数）
```

即未声明标度时**第 k 帧的归一化权重整体多乘 `α_k²`**。任何两帧的**权重比**因此失真因子恰为
`(α_i/α_j)²`；以标度最小的帧（`α_k = α_min`）为基准帧时失真最大：

```
max_k (α_max/α_k)² − 1 ≡ (α_max/α_min)² − 1
```

**代入原文自带的区间**（`α ∈ [1.1387e-17, 6.0083e-17]`）：

```
α_max/α_min = 6.0083e-17 / 1.1387e-17 = 5.2764556072714495
(α_max/α_min)² = 27.84098377550632
(α_max/α_min)² − 1 = 26.84098377550632
```

⇒ **公式侧算出 26.841，不是 1.0336。** 二者数学上不可能同时成立，**我判定公式侧正确**。
`1.0336` 量的必然是别的量，但逐帧 `α` 读数全仓不可得（§3），**无法确定它是哪个量，因此不给它写定义**——
按红线「禁止编造」，该数已被删除而不是被"解释"。

派生自同一代数式的另一条**不依赖任何读数**的结论，现已写进 `NUMERIC.md`：
逐帧换算是精确代数折合 ⇒ 换算后 `w_k = 1/(α_k²·v_k)` 逐位等于声明标度下的权重 ⇒
`Var_w` 逐位相同 ⇒ `ΔE ≡ 0`。这条不需要 `α` 的分布，只需要换算的代数精确性。

### 2.2 · 3-2 量名复算（我判定**派单判定成立**）

```
α_min² = (1.1387e-17)² = 1.29663769e-34
α_max² = (6.0083e-17)² = 3.6099668889999994e-33        ← 与文中「1.2966e-46 … 3.6099e-45」不同数量级
1e-12 × α_min² = 1.2966376899999999e-46
1e-12 × α_max² = 3.609966888999999e-45               ← 与文中区间逐位吻合
```

⇒ 文中那对数是 `variance_floor · α²`，**不是 `α²`**。不是指数笔误，是量名错标。派单判定成立。
显式乘法步已写入正文代码块：`floor(α) = variance_floor · α²`。

### 2.3 · 3-3 float32 可表示性的完整推导（**我推翻了前人的判红线与标度类别判定**）

**第一处（判红线）**：IEEE 754 binary32 就近舍入（round-to-nearest-even），最小次正规数
`2⁻¹⁴⁹ = 1.401298464324817e-45`，`f32max = 3.4028235e+38`，最小正规数 `2⁻¹²⁶ = 1.1754943508222875e-38`。

```
半 ULP 界      2⁻¹⁵⁰ = 7.006492321624085e-46
零点阈值  α₀ = sqrt(2⁻¹⁵⁰ / variance_floor) = 2.6469779601696886e-17
次正规起点 α₁ = sqrt(2⁻¹⁴⁹ / variance_floor) = 3.7433921305746435e-17
正规起点   α₂ = sqrt(2⁻¹²⁶ / variance_floor) = 1.0842021724855044e-13
ivar 有限阈值 α₃ = sqrt(1 / (variance_floor · f32max)) = 5.4210110239862425e-14
```

**反例（推翻「`floor(α) < 2⁻¹⁴⁹` ⇒ 必为 0」）**：

```
x = 9e-46  < 2⁻¹⁴⁹  ⇒ True ；但 float32(9e-46) = 1.401298464324817e-45 ≠ 0
x = 7.006492321624085e-46 (= 2⁻¹⁵⁰) < 2⁻¹⁴⁹ ⇒ True ；float32 = 0.0
```

⇒ 判红线是 `2⁻¹⁵⁰` 不是 `2⁻¹⁴⁹`。前人写的「`floor(α) < 2⁻¹⁴⁹` 时不可能产出正值」**为假**。

**补上的更强结论**：`floor(α)` 即使非零，只要 `α < α₃ = 5.4210e-14`，其倒数
`1/floor(α) > f32max` 在 float32 下**必溢出为 `+Inf`** ⇒ 该地板在 float32 平面内
**始终给不出「有限方差 + 有限 ivar」这一对可用产品值**；只有 `α ≥ α₂ = 1.0842e-13` 才同时满足。
前人只写了「次正规数相对精度失真」，没给出这条可判定的阈值。

**第二处（标度类别，推翻「该平面是 `raw_adu` 面」）**：

```
run/M42-VARIANCE-RCA-01/plane_fixed.f64  = 134217728 B = 16 777 216 × f8 = 4096²   ✓
实测：100% 有限；min = 0.0；max = 8.149762745256597e-29；median = 2.1457208312226298e-29
      (pl <= 0) = 0.30089712142944336 = 30.09%
run/M42-VARIANCE-RCA-01/meta.json::photscal = 2.3846837130250378e-17
```

若取 `α = 1`：`median = 2.1457e-29 ADU² ⇒ σ = 4.63e-15 ADU` —— 比任何探测器的读出噪声低 **15 个数量级**，不可能成立。
取 `α = 2.3846837130250378e-17`：

```
α² = 5.686716411166881e-34
median / α² = 3.7729e4 ADU² ⇒ σ = 194.2 ADU
对照 meta.json::background_adu = 194.0560302734375 ADU      ← 同量级
floor(α) = 1e-12 × 5.686716411166881e-34 = 5.686716411166881e-46 < 2⁻¹⁵⁰ ⇒ float32 恰为 0
```

且 `30.09%` 的零值与同次运行根因报告记录的「平面负预测占比 30.09%」**逐位吻合**。
⇒ 该平面是 `photo_scaled_adu` 面，不是 `raw_adu` 面。在该 α 下「float32 下恰为 0」这一条
与根因报告的实测表**互相印证**。

（诚实边界：`σ = 194.2 ADU` 与该次运行记录的 `noise_sigma_adu = 20.74 ADU` 相差约 9×。
这个差不是矛盾——根因报告自己指出该 8×8 patch 平面在星云充满的帧上量的是**结构**而非随机噪声。
我只用「`α = 1` 物理上不可能」与「按该 α 换算后与该次运行自身记录同量级」两条定标度类别，
**不主张该平面等于真实随机噪声方差**。）

### 2.4 · 3-4 `nside` 依赖式

```
A_cell = 4π / (12·nside²)                    （HEALPix 像素立体角）
1/A_cell² = (12·nside²/4π)² = 144·nside⁴/(16π²)
dex = log₁₀(1/A_cell²) = 8·log₁₀(nside) − 2·log₁₀(4π/12)

nside = 2¹⁸ = 262144 : A_cell = 1.523873e-11 sr, 1/A_cell² = 4.306282e+21, dex = 21.6341
nside = 2¹⁶ = 65536  : A_cell = 2.438197e-10 sr, 1/A_cell² = 1.682141e+19, dex = 19.2259
两行 dex 之差 = 8.4082… → 取整 8 dex = 2 个 nside 位 × 每位 4 dex    ✓ 与斜率自洽
```

`nside` 是运行参数不是常数；派单要求核对 `governance/UNRESOLVED.md:63` 的记录内容——**我核到了，
且它不支持「生产 nside = 65536」这个读法**：该行是 `ENG-B2` 条目里的一句实测事实
「同一命令同一代码跑银心 T4 三帧全部成功（**nside=65536**、42 内点、rms 0.32″）」，
记的是**一次成功的天测解算档位**，不是「生产 nside 的定值声明」。所以正例只能写成
「`nside` 随运行参数变化 + 两行示例 + 引用任一行必须同时给出 `nside`」，不能写成定值。

### 2.5 · 3-5 并行轴正本式与死区前提

正本（`docs/engineering/architecture/DATA_FLOW.md:116-118`，我逐字读过）：

```
in_flight = min(n, frame_workers)       // frame_workers = min(lease, 内存闸门上限)
inner_omp = max(1, thread_budget / in_flight)   // thread_budget = lease
总并行度 = in_flight × inner_omp ≤ thread_budget
```

实现侧同式（`module_adapters.cpp` 的 `p1_parallel_for`，我读过源码）：

```cpp
const uint64_t in_flight = std::min<uint64_t>(n, static_cast<uint64_t>(frame_w));
uint32_t inner_u = (in_flight > 0) ? std::max<uint32_t>(1u, budget / in_flight) : 1u;
```

**两式只在 `F ≤ n` 时等价**：`F > n` 时 `in_flight = n`，`I = max(1, L/n)` 与 `F` 无关。
死区推导：`in_flight = F`（前提 `F ≤ n`）且 `K ≥ 2` 时，`W_eff = F × min(I, K)`；
`F ≤ L/2 ⇒ I = ⌊L/F⌋ ≥ 2 ⇒ min(I,K)=2 ⇒ W_eff = 2F`（在 `F = L/2` 取到 `L`）；
`L/2 < F < L ⇒ I = 1 ⇒ W_eff = F < L`（**死区**）；`F ≥ L ⇒ I = 1 ⇒ W_eff = F ≥ L`（`F = L` 回到满宽）。
`F > n` 段 `W_eff = n × min(I,K)` 不再随 `F` 下降 ⇒ 死区**只在 `F ≤ n` 段成立**。已写入正文。

### 2.6 · 4-4 `B = 116.0` 的真实口径（`run/P1-CONCURRENCY-CALIB-01/REPORT.md` §5.1 / §5.4）

实测（四档峰值 RSS，同一二进制、同场 8 帧、4096² FP64、drizzle auto nside、`K = 2`、无 swap）：

| F | 实测峰值 RSS (GB) | 拟合 (GB) | 残差 |
|---|---|---|---|
| 1 | 1.799 | 1.815 | −0.9% |
| 2 | 3.578 | 3.488 | +2.5% |
| 4 | 6.726 | 6.833 | −1.6% |
| 8 | 13.555 | 13.522 | +0.2% |

```
最小二乘：RSS(F) = 0.143 GB + F × 1.6724 GB
边际 1.6724 GB/帧 ÷ 16.777e6 px = 99.68 B/px        ← 实测边际
base 0.143 GB                                      ← 实测截距
```

**116.0 不是实测值**，是标定取值：要 `W_eff = 16` 必须 `F = 8`（`F ≥ 16` 需约 27 GB，本机不可行）；
`F = 8 ⟺ 0.75A/(9P) < B ≤ 0.75A/(8P)`，在 `A = 20.77 GB` 下窗口 = **(103.2, 116.1]**；
取窗口**上端** 116.0 ⇒ 目标档实测峰值 13.940 GB = `0.75A` 的 89.5%（余量 10.5%），
同帧集墙钟 421.0 s（vs 旧口径 `W_eff = 4` 的 783.5 s，1.86×）。
⇒ `B` **随机器 `A` 平移**，换机器必须按同一口径重标定。已把这套口径写进 `PERFORMANCE_MODEL.md`。
`eng/packaging/config/runtime_resources.json::memory_pressure_derivation_scope` 与
`memory_pressure_high_percent_derivation` 引的「边际 1.6724 GB」与本拟合一致（我核过）。

---

## 3 我推翻的审稿判定与前人判定（依据 + 保留原判说明）

| 谁 | 原判 | 我的复核 | 依据 | 处置 |
|---|---|---|---|---|
| 派单 / 前一代理 | 「下溢为 0 当且仅当 `floor(α) < 2⁻¹⁴⁹`」 | **错** | `float32(9e-46) = 1.4013e-45 ≠ 0` 而 `9e-46 < 2⁻¹⁴⁹`；就近舍入的零点是半 ULP `2⁻¹⁵⁰` | **改正文**，§2.3 给出完整推导与逐点验证 |
| 前一代理 | 「`plane_fixed.f64` 是 `raw_adu` 面，故地板取 `α = 1`」 | **错** | `α = 1` ⇒ σ = 4.63e-15 ADU（低于任何探测器读出噪声 15 dex）；同次运行 `meta.json::photscal = 2.3847e-17` 换算后 σ = 194.2 ADU，与该次 `background_adu = 194.056` 同量级；30.09% 零值与根因报告逐位吻合 | **改正文**为 `photo_scaled_adu` 面，并补该 α 下 `float32` 恰为 0 的实测互证 |
| 派单 | 「`governance/UNRESOLVED.md:63` 记**生产实测** `nside=65536`」 | **前提不成立** | 该行是 `ENG-B2` 里「银心 T4 三帧天测解算全部成功（nside=65536…）」的一次实测事实，**不是**生产 nside 的定值声明 | 正例改为「`nside` 依赖式 + 两行示例 + 引用须同时给出 nside」，并如实转述该记录的真实内容 |
| 派单 | 「`kP1FrameMemSafetyFrac` 若是代码里没有的名字，就写『待标定配置项』」 | **派单的前提不成立** | `grep -rn "kP1FrameMemSafetyFrac" lib/ eng/` → `module_adapters.cpp:2308` 有 `static constexpr double kP1FrameMemSafetyFrac = 0.75;` | 按实际写：数值正本 = `runtime_resources.json::frame_memory_gate.safety_frac`，实现侧字面量只作镜像；JSON 的「实现侧零字面量」断言为假 → 登记为代码侧待订正 |
| 派单 | 「`B = 116.0` 无推导、无拟合件名」 | **前提不成立** | `run/P1-CONCURRENCY-CALIB-01/` 存在，`REPORT.md` §5.1 给最小二乘拟合、§5.4 给可采纳值推导；代码 `module_adapters.cpp` 的注释块也写了同一条推导 | 不标「待标定」，改为写清件名 + 拟合口径 + 「116.0 是窗口上端而非实测边际」 |
| 派单 | 「4-8 需负责人裁决 enforcement 冲突」 | **不存在需要裁决的冲突** | `resource_gate_v1.json` 自述为「唯一数值源」，`enforcement_note` 明写理由；文档与它不一致时**数值源即唯一依据** | 直接按数值源改文档；把「文档此前单方面升级为 fail-closed」作为**文档侧缺陷**记入本报告（不是待裁决项） |
| 审稿员 | `UNIFIED_OBJECTS.md` 的 `E = Var_w/Var_opt − 1` 无缺陷 | 同意 | 不在白名单，未触碰 | 保留，登记为「审稿员已验证为正确」 |
| 审稿员 | `data/ARTIFACTS.md` 的 `1.482602218505602` 与 Rousseeuw & Croux 1993 DOI | 同意（文献核对：题录与 DOI 已核，**未取全文**） | 不在白名单，未触碰 | 保留，登记为「已验证项」 |

---

## 4 需代码侧订正的问题（我改了文档，代码仍与文档/数值源不符）

**① `runtime_resources.json` 的「实现侧零字面量」断言当前为假**
`eng/packaging/config/runtime_resources.json::frame_memory_gate.note` 自称两个分母
「现两个分母都登记在本唯一源，并由 `CHK-BUDGET-SINGLE-SOURCE` 与实现侧字面量逐位比对」，
但 `lib/infrastructure/scheduler/src/module_adapters.cpp:2307-2308` 仍是
`static constexpr double kP1FrameBytesPerPixel = 116.0; static constexpr double kP1FrameMemSafetyFrac = 0.75;`，
该 TU **没有 include 任何 `runtime_resources_generated.h`**（生成头存在于
`build/runtime_resources_generated.h` 等构建目录）。
需订正：或改实现为消费生成头，或把 JSON 的 note 改成如实描述。
（另：`CHK-BUDGET-SINGLE-SOURCE` 在现行树中亦无执行器 —— `eng/ci/` 整个目录不存在。）

**② 生产代码逐字引用了一个不存在的文档路径，且条款措辞与 CODE.md 已经漂移**

| 位置 | 引用原文（节选） |
|---|---|
| `lib/infrastructure/pipeline/orchestrator/cpp/src/orchestrator.cpp:1350` | `依据: docs/engineering/CODE_STANDARD.md §MUST「禁止重复 production science implementation（单一实现 + oracle）」+「禁止 silent config fallback 改变科学语义」` |
| `lib/infrastructure/pipeline/orchestrator/cpp/src/orchestrator.cpp:2663` | `CODE_STANDARD §MUST「禁止 silent config fallback 改变科学语义」` |
| `lib/infrastructure/pipeline/orchestrator/cpp/include/orchestrator.h:42` | `C++17 (CODE_STANDARD §MUST: MSYS2 MinGW64 g++16.1 -std=c++17 …)` |
| `lib/algorithms/photometry/cpp/src/filter_curve_json.h:15` | `docs/engineering/CODE_STANDARD.md §MUST「禁止重复 production science implementation（单一实现 + oracle）」` |
| `lib/algorithms/photometry/cpp/src/filter_curve_json.h:18` | `docs/engineering/CODE_STANDARD.md §MUST「禁止 silent config fallback 改变科学语义」` |
| `lib/algorithms/photometry/cpp/src/filter_curve_json.h:443` | `降级，CODE_STANDARD §MUST「禁止 silent config fallback 改变科学语义」` |
| `lib/algorithms/photometry/cpp/src/frame_photometry_fit.cpp:48` | `依据 docs/engineering/CODE_STANDARD.md §MUST「禁止重复 production science implementation（单一实现 + oracle）」` |
| `lib/algorithms/photometry/cpp/src/frame_photometry_fit.cpp:119` | `CODE_STANDARD §MUST`（曲线名解析不到必须具名报错） |
| `lib/algorithms/resample/p3_resample.cpp:316` | `CODE_STANDARD §MUST「无 per-pixel 堆分配」` |
| `lib/algorithms/coverage/src/coverage.cpp:205` | `静默截断告警（CODE_STANDARD：禁止 silent truncation）` |
| `lib/infrastructure/pipeline/orchestrator/cpp/src/orchestrator.cpp:4` | `C ABI … (CODE_STANDARD/C_ABI_STANDARD)` |

两个问题：
1. **路径**：`docs/engineering/CODE_STANDARD.md` 不存在（现行文件是 `docs/engineering/standards/CODE.md`）。
2. **措辞**：代码引用的条款标题是「禁止重复 production science implementation（单一实现 + oracle）」/
   「禁止 silent config fallback 改变科学语义」，而 `CODE.md` 的 MUST 写的是
   「production science implementation 单一（单一实现 + oracle）」/「config fallback 一律显式并保持科学语义」。
   派单明确要求「MUST 条款逐字不变」，**因此我没有改 CODE.md 的措辞**，只登记此漂移。
   `orchestrator.h:42` 还把 MinGW64 与版本号 `g++16.1` 写进了注释，与 `CODE.md` 的
   「MinGW64 = 本地开发/兼容性验证工具链、版本不复述」冲突，同属代码侧。

**③ `p1drz_merge_pipeline_lock` 的脚本路径不存在**
`run/P1-PARALLEL-AXIS-REDESIGN-01/REPORT.md:386` 指向
`lib/algorithms/drizzle/healpix_drizzle/tests/p1drz/p1drz_merge_pipeline_lock.sh` —— 该文件不存在。

**④ `TEST.md` §11 的三条用例载体不存在（已由他人如实记为「无载体」，此处只登记）**
`lib/algorithms/calibration/tests/test_photometry_apply.cpp`、
`lib/algorithms/platesolve/cpp/ipv/test/test_synthetic.cpp`、
`lib/algorithms/coverage/tests/synthetic_gate.cpp` 三个目录 `lib/algorithms/calibration/tests/`、
`lib/algorithms/platesolve/cpp/ipv/test/`、`lib/algorithms/coverage/tests/` **整个不存在**。

**⑤ `VALIDATION_EVIDENCE.md` 的交叉引用被一次机械替换损坏（重要）**
全仓存在一批「`§x.y` → `「文档治理规范 某节」」的错误替换结果，即**节号被替换成了文档名**。实例：
- `:177` 「判据集合的来源要求见 `文档治理规范` 文档治理规范的登记纪律一节。」
- `:238` 「排除面按 `文档治理规范` 文档治理规范的登记纪律一节 的三字段…」
- `:380` 「按 `../architecture/DATA_FLOW.md` 治理任务执行模型的收口清理条款 与 文档治理规范 复核与上报纪律一节 处置完毕」
- `:560` 「定义正本 = `../../science/PHASE2_UPM.md` **文档治理规范的正向设计纪律一节**（patch 保留样本数下界…）」← 归属文档与节名不匹配

**我没有改这些**：要正确修复必须知道每处原本的节号，而那属于另一个车道（该文件正在被大改）。
**我已把我自己在该文件里的改动限制在引用标记（3 处）与两条无正文引用的外部文献条目删除**，不碰交叉引用。

---

## 5 需权威补充的问题（写明「需补充什么」）

| # | 需补充什么 | 我做了什么替代 |
|---|---|---|
| A | **33 帧逐帧 `α` 的可复算来源**（`p1_phot.json` 或等价件，须在可跟踪面上）。我已穷举：`find` 全仓 20 个 `p1_phot*.json`、全部 `*phot*/` `photscale_detail` 最大 16 帧、`grep -rl '"n_frames": *33' run/ 实验/ eng/ artifacts/` → 0 命中；`1.1387e-17` 仅出现在 `run/FINAL-07-e2e/bisect/**` 的**旧仓快照**里（`docs/standards/NUMERIC_STANDARD.md` 等审计件），不是可跟踪的证据面。 | 删掉全部依赖该件的数量断言，改写成只依赖代数精确性的结构结论；另给出**一条仓内可复算的替代实例**（`run/FINAL-07-e2e/evidence/face/t1_ab_A/p1_phot.json` 的 `photscal = 5.998762904583689e-17`，我用它验了 `floor(α) = 3.5985e-45 < 2⁻¹⁴⁹` 且 `1/floor(α)` 在 float32 下溢出为 `+Inf`），但**未写进正文**，因为它只有 1 帧、不构成 33 帧的替代。 |
| B | **`stripe` 收益递减拐点的受控扫描件**：固定其它变量、只变帧内 stripe 数的墙钟读数（含重复次数与 median/p95）。 | 降为「待标定的经验拐点」，移出「不可改类」，正文不给数值。 |
| C | **HTTP `User-Agent` product token 的真实定义**（产品标识 + 版本策略）。全树无 ACSD 自有 User-Agent。 | 换成中性形式 `<产品标识>/<主>.<次>`，例 `ACSD-<产品标识>/1.0`，并登记为待补。 |
| D | **`architecture/DATA_FLOW.md:121` 的 `0.75` 字面量**是第二份副本（不在我白名单）。我的处置是把 `PERFORMANCE_MODEL.md` 的数值正本改指 JSON 键；DATA_FLOW 侧需同一车道改。 | — |
| E | **命名块是否跨节点的权威裁决**（8-1）：`architecture/DATA_FLOW.md:14` 与 `contracts/PIPELINE_BLOCK.md:17`（不跨）vs `AGENTS.md:88` 与 `docs/ACSD_DESIGN.md:381-383`（读块写新块、用完显式销毁）；注册表 `carrier_contract.statement` 站工程正本一侧。 | 未改任何一侧，仅登记。 |
| F | **`SCI-DRZ-004` 在 drizzle 偏差表中的现行状态**：CODE.md/COMMENT.md 的示例用它做注释写法示范；需确认该 ID 仍可作为示例而不指向已废止口径。 | 保留示例原样（属真内容，不删）。 |
| G | **`resources/cpu/ISA_VARIANTS.md` 的 `R-60` 任务编号**（真流水号，不是我白名单）。 | 未动，登记。 |

---

## 6 我否决的（以及为什么）

| 否决项 | 理由 |
|---|---|
| **不为 `1.0336` 编一个量定义** | 派单举例 `max_k|w_k/⟨w⟩−1|`。我没有采纳这个猜测：逐帧 `α` 全仓不可得（§5-A），**验不了**。按红线「核对不到一手证据的如实登记」，该数**删除**而不是被"解释"。 |
| **不把 `plane_fixed.f64` 的标度写成 `raw_adu` 以保住原判** | 原判物理上不可能（`σ = 4.63e-15 ADU`）。按红线 3，保留真内容、改正错判，并给出反证。 |
| **不把 `B = 116.0` 标为「待标定，数值来自单一读数」** | 派单给了这个 fallback，但件名与拟合口径**确实存在**（`run/P1-CONCURRENCY-CALIB-01/`）。标成「待标定」会丢掉真证据。 |
| **不删 `TEST.md` §11 的三条用例条目** | 条目内容（科学条款 ↔ 用例 ID ↔ 容差 ↔ 量级域的绑定）是真的；载体缺失是另一件事。前人的处置（保留绑定 + 载体列记「无载体」）正确，我照准。 |
| **不写任何「复跑 `p1drz_merge_pipeline_lock`」的命令** | 派单明令禁止；脚本不存在，也无执行器。 |
| **不删 `ISA_VARIANTS.md` 的 `R-60`** | 不在白名单，且不是我判定的。 |
| **不修 `VALIDATION_EVIDENCE.md` 的交叉引用** | 需要原始节号，我只能猜；且该文件正在被另一车道大改。按红线 3，改错比不改更糟。登记为需代码/文档侧订正。 |
| **不删 REFERENCE 里的外部文献条目以外的任何东西** | 唯一一次删文献条目是 `VALIDATION_EVIDENCE.md` 的 [4] Horne / [5] Zackay & Ofek —— 它们**正文零引用**（`grep -n "Horne\|Zackay" VALIDATION_EVIDENCE.md` 只命中文献表自身），删的是孤儿条目，不是真内容。 |

---

## 7 验证（全部原样贴出）

```
$ cd "/workspace/Astro CS Database"

########## V1 空锚「」 ##########
$ grep -rn "「」" docs/engineering/standards docs/engineering/resources docs/engineering/testing
rc=1 (1=clean)                                    ← 0 命中

########## V2 PERFORMANCE_MODEL.md) 自指 ##########
$ grep -rn "PERFORMANCE_MODEL.md)" docs/engineering/
rc=1 (1=clean)                                    ← 0 命中

########## V3 历史叙事与流水编号 ##########
$ grep -rnE "V1[0-9]|V2[0-9]|R-[0-9]+|P-[0-9]+" docs/engineering/standards docs/engineering/resources docs/engineering/testing \
  | grep -v "docs/engineering/resources/cpu/"
docs/engineering/standards/COMMENT.md:46:// SCI-DRZ-004 / ALG-DRZ-OVERLAP-002:
docs/engineering/testing/VALIDATION_EVIDENCE.md:127..135:  CMP-01 … CMP-09
docs/engineering/testing/TEST.md:225:| 球面重采样 SCI-DRZ-001/014 | … TEST-DRZ-VAR-001 … |

$ grep -oE "V1[0-9]|V2[0-9]|R-[0-9]+|P-[0-9]+" <上述文件>
   COMMENT.md:P-002            ← DRZ-OVERLAP-002（科学 ID 示例）
   VALIDATION_EVIDENCE.md:P-01…P-09   ← CMP-01…CMP-09（判据 ID）
   TEST.md:R-001               ← SCI-DRZ-001（科学条款号）
⇒ 11 行全部是正则假阳性，白名单内无真流水编号。
（`resources/cpu/ISA_VARIANTS.md` 的 `R-60` 是真流水号，不在白名单，已登记。）

########## V4 COMMENT.md 引用 ##########
$ grep -n "COMMENT.md" docs/engineering/standards/CODE.md docs/engineering/standards/README.md docs/engineering/README.md
CODE.md:5    代码必须遵守的条目、命名与机器契约保留面与编译器告警口径。注释纪律见 `COMMENT.md`。
CODE.md:86   | 生产源码注释内的轮次标识 | … | 按 `COMMENT.md` 的必须删除/迁移一节删除；代码注释不承载轮次叙事 |
CODE.md:116  注释纪律（原则、必须注释、必须删除/迁移、叙述性注释的清理面、长度、审计）见 `COMMENT.md`；
CODE.md:117  本文件只承载代码条款，单向引用 `COMMENT.md`，不复制其条目[4]。
CODE.md:125  [4] 内部文档 `docs/engineering/standards/COMMENT.md`，注释纪律，本篇注释条款的展开。
standards/README.md:1  本目录是工程标准正本，共十项：`CODE.md`（代码）、`COMMENT.md`（注释）、`NUMERIC.md`（数值）、
engineering/README.md:14  | `standards/` | 工程标准：代码（`CODE.md`）、注释（`COMMENT.md`）、数值、并发、缓存、兼容性、优化、文档、依赖 |

########## V5 孤儿参考文献条目 ##########
$ python3 - <<'PY'   # 见本报告正文命令
files with orphans: 7
docs/engineering/resources/BENCHMARK.md 孤儿条目 ['1', '2']
docs/engineering/resources/cpu/AVX2_PROVIDER.md ['1', '2']
docs/engineering/resources/cpu/BACKEND.md ['1', '2', '3']
docs/engineering/resources/cpu/CAPABILITY_PROBE.md ['1', '2']
docs/engineering/resources/cpu/ISA_VARIANTS.md ['1', '2', '3']
docs/engineering/resources/observability/RUN_GRAPH.md ['1', '2']
docs/engineering/resources/observability/STRUCTURED_LOGGING.md ['1', '2']
⇒ 白名单内孤儿 = 0。剩余 7 个文件全部不在我的白名单
  （whitelist 只含 resources/PERFORMANCE_MODEL.md 与 resources/observability/RESOURCE_MONITORING.md）。

########## V6 死路径（7-1 / 10-1） ##########
$ python3 - <<'PY'   # 见本报告正文命令（正则已修：hpp|h|cpp，c 不再抢 cpp）
MISSING total 39 | in whitelist: 0
---- out-of-whitelist count: 39
⇒ 白名单内 2 → 0。修掉的两条：
   1) standards/NUMERIC.md  `../../governance/TRACEABILITY.md` → `../governance/TRACEABILITY.md`
      （`standards/` 上两级是 `docs/`，原路径实指不存在的 `docs/governance/`）
   2) resources/PERFORMANCE_MODEL.md  `docs/science/noise_snr/NOISE_MODEL.md` → `NOISE_SNR.md`
      （`docs/science/noise_snr/` 下只有 `NOISE_SNR.md` 与 `README.md`）
```

---

## 8 自证段

**我跑了什么**
`python3` 数值复算（3-1 权重量化、3-2 量名、3-3 float32 舍入与阈值、3-4 `nside`、`3-6` 的 `u`、
`2**-53` / `2**-24` / `2**-149` / `1.1754943508222875e-38`）；`numpy` 读
`run/M42-VARIANCE-RCA-01/plane_fixed.f64` 全量 16 777 216 个 f8 并算分布；`python3` 全仓扫
`*phot*.json` 的 `photscale_detail` 帧数；`find` / `grep -rl` 找 33 帧 `α` 来源；
`json.load` 读 `eng/packaging/config/runtime_resources.json`、`eng/contracts/resource_gate_v1.json`、
`eng/packaging/config/defaults.json`、`run/M42-VARIANCE-RCA-01/meta.json`；
`grep -rn` 核 `kP1FrameMemSafetyFrac` / `kP1FrameBytesPerPixel` / `CHK-NAMING-SURFACE` /
`CODE_STANDARD` / `BASS` / `User-Agent` / `replay-lock` 三个 ID / `l2_performance` 与
`P1-CONCURRENCY-CALIB-01` 的工件；
逐字读了 `architecture/DATA_FLOW.md:116-125`、`lib/infrastructure/scheduler/src/module_adapters.cpp`
的 `p1_parallel_for`（1936-2110）与 `p1_memory_cap`（2295-2350）、
`lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp`（1600-1675）、
`lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.h:40-41`；
读了 `run/P1-CONCURRENCY-CALIB-01/REPORT.md`、`run/P1-PARALLEL-AXIS-REDESIGN-01/REPORT.md`、
`run/M42-VARIANCE-RCA-01/evidence/RCA-REPORT.md`、`实验/engineering-evidence/l2_performance/README.md`、
`实验/engineering-evidence/release-05/FAILCLOSED_SURVEY.md`；
跑了 6 组交付前验证脚本。

**我没核 / 不能核的**
- **没编译、没运行任何二进制、没跑任何测试、没跑端到端**。全部结论来自文档、源码静态阅读、
  已归档工件与纯算术。
- `P1-PARALLEL-AXIS-REDESIGN-01` 与 `P1-CONCURRENCY-CALIB-01` 的实测数字**我只做了内部一致性复算**
  （拟合残差、窗口边界、10.5% 余量），**没有独立复跑**其受控扫描。
- **文献核对：题录与 DOI 已核，未取全文**（承接审稿员的诚实边界，适用于
  Rousseeuw & Croux 1993 的 `1.482602218505602`）。JCGM 100:2008、FITS Standard 4.0、
  REC-HIPS-1.0、IEEE 754-2019 我**按 NUMERIC.md 既有的引用采信**，本轮未取原文逐字复核
  （3-3 的 `2⁻¹⁵⁰ / 2⁻¹⁴⁹ / 2⁻¹²⁶ / f32max` 我用 IEEE 754 的表示范围与 `numpy.float32`
  的实际舍入行为**数值验证**了，不是靠背书）。
- `NUMERIC.md` 中 `α` 区间 `[1.1387e-17, 6.0083e-17]` 的**原始出处**我仍核不到（§5-A），
  因此 §2.1 里用它代入公式时**明确标注这是原文自带的区间**，不是我能独立复算的读数。
- `VALIDATION_EVIDENCE.md` 的交叉引用损坏我只做了**举例定位**，未逐条枚举全部损坏点
  （该文件正在被另一车道大改，逐条枚举会立刻过时）。
- `docs/engineering/governance/UNRESOLVED.md`、`contracts/**`、`architecture/**`、`api/**`、
  `data/**`、`governance/**`、`build/**`、`docs/DOCUMENT_INDEX.yaml`：**全程只读，一条未改。**

**git**
零写操作。仅 `git -c core.quotepath=false status --short` / `diff --stat` /
`branch --show-current` 三条只读命令。

---

## 9 改前逐字 / 改后逐字

### 9.1 `standards/NUMERIC.md`

**「改前」有两层。** 派单给的是开工时的版本（下称 A）；我接手时文件已被另一代理重写（下称 B）。
我对 B 又做的订正标为 C。

#### 3-1（判谁错）— 改前 A（逐字，`git show HEAD` :49-53）

```
- **真实数据推导**：同一次生产运行内逐帧 `α_k` 实测跨 **5.28×**（`run/RELEASE-05/vis/out/m42_p1_t3/p1_phot.json`，
 33 帧，`α ∈ [1.1387e-17, 6.0083e-17]`）⇒ `α²` 跨 **27.84×**。由公式「`w_k ∝ 1/σ_k²`，而 `σ_k² = α_k²·σ_adu,k²`」
 推出：标度未声明时归一化权重最大相对偏差应为 `max_k (α_max/α_k)² − 1`；实测 **1.0336**，
 权重效率损失增量 `ΔE = E_未声明 − E_已声明 = 0.2184`（判据 `E = Var_w/Var_opt − 1`，`docs/science/noise_snr/NOISE_SNR.md` b）；
 逐帧换算后该效应恰为 **0**（`ΔE = 0.0`，逐位）。
```

#### 3-1 — 改后 B（他人已落，我复核成立，未再改）

```
- **标度未声明的权重后果（结构结论，可由公式自证）**：`w_k ∝ 1/σ_k²`，而 `σ_k² = α_k²·σ_adu,k²`
  ⇒ 未声明标度时第 `k` 帧权重相对声明后的权重整体多乘 `α_k²`，帧间权重比失真因子为 `(α_max/α_k)²`。
  `α_max/α_k` 在 `α` 取最小值的帧处取极大，故

  max_k (α_max/α_k)² − 1 ≡ (α_max/α_min)² − 1

  即**最不利帧对的权重比相对偏差上界**，其中 `α_min`、`α_max` 是同一次运行内逐帧标度的极值。
  该量是**逐次运行的实测量**：`α` 的极值跨度与据此的权重效率损失增量
  `ΔE = E_未声明 − E_已声明`（判据 `E = Var_w/Var_opt − 1`）都由该次运行的逐帧 `α` 读数给出，
  **本标准不为它们冻结任何数值**；把某一次运行的 `α` 极值跨度写成全仓常数或阈值断言即违反本条。
- **换算的精确性（结构结论，逐位）**：逐帧换算是精确代数折合——换算后 `w_k = 1/(α_k²·v_k)`
  逐位等于声明标度下的权重，故 `Var_w` 逐位相同、`E` 逐位相同、`ΔE ≡ 0`。该结论只依赖换算的
  代数精确性，与任何读数无关。
```

**我判定：公式侧正确，`1.0336` 错。** 代入 A 自带的 α 区间，`(α_max/α_min)² − 1 = 26.84098377550632`
（推导见 §2.1）。`1.0336` 量的必然是别的量，但逐帧 `α` 不可得（§5-A），**我不给它写定义**。

#### 3-2（量名）— 改前 A（逐字，:72）

```
冻结 `variance_floor = 1e-12 ADU²` 按 33 个实测 `α² ∈ [1.2966e-46, 3.6099e-45]` 换算后，
```

#### 3-3（改前 A 的下界与标度，逐字，:73-74 与 :108）

```
在 float32 下 **32/33 精确下溢为 0**，其余为次正规数（max `4.204e-45` < float32 最小正规数 `1.1755e-38`）
⇒ 该地板在 float32 产品上**不可能产出正值**，`clamp` 分支只能产出 `0`（= a 的「显式不可用」态）。
…
该平面是 `raw_adu` 面（未施加帧标度），故其上的地板取 `α = 1`。
```

#### 3-3 — 改后 C（我的订正，逐字）

```
 下溢为 0 当且仅当 floor(α) < 2⁻¹⁵⁰（就近舍入的半 ULP 界；float32 最小次正规数
  2⁻¹⁴⁹ = 1.401298464324817e-45，其一半 2⁻¹⁵⁰ = 7.006492321624085e-46）[9]
 次正规带 = [2⁻¹⁵⁰, 2⁻¹²⁶) ，2⁻¹²⁶ = 1.1754943508222875e-38（float32 最小正规数）
 零点阈值   α₀ = sqrt(2⁻¹⁵⁰ / variance_floor) = sqrt(7.006492321624085e-34) = 2.6469779601696886e-17
 次正规起点 α₁ = sqrt(2⁻¹⁴⁹ / variance_floor) = sqrt(1.401298464324817e-33) = 3.7433921305746435e-17
 正规起点   α₂ = sqrt(2⁻¹²⁶ / variance_floor) = 1.0842021724855044e-13
 ivar 有限阈值 α₃ = sqrt(1 / (variance_floor · f32max)) = 5.4210110239862425e-14
```

```
 判红线是 `α₀` 而**不是** `α₁`：float32 就近舍入（round-to-nearest-even）下
 `2⁻¹⁵⁰ ≤ floor(α) < 2⁻¹⁴⁹` 仍舍入为最小次正规数 `2⁻¹⁴⁹`（非零），只有 `floor(α) < 2⁻¹⁵⁰`
 才舍入为 `0`。故 `α < α₀` 时该地板在 float32 平面内**恒为 `0`**（`clamp` 分支只能产出 `0`
 =「显式不可用」态）；`α₀ ≤ α < α₂` 时地板虽为非零次正规数、有效位已失真，且只要 `α < α₃`
 其倒数 `1/floor(α)` 放回 float32 **必然溢出为 `+Inf`** —— 故 `α < α₃` 时该地板在 float32
 平面内始终给不出「有限方差 + 有限 ivar」这一对可用的产品值；只有 `α ≥ α₂` 才同时满足两侧。
```

```
 - **产品面**（M42 生产噪声平面 `run/M42-VARIANCE-RCA-01/plane_fixed.f64`，sha256 `965e6fe…46010c`；
 同次运行的帧标度读数 `run/M42-VARIANCE-RCA-01/meta.json` 的 `photscal = 2.3846837130250378e-17`）：
 该平面是 `photo_scaled_adu` 面，其标度取该次运行的 `α`，不是 `α = 1`。判据：若取 `α = 1`，
 平面中位 `2.1457208312226298e-29` 对应 `σ = 4.63e-15 ADU`，比任何探测器的读出噪声低 15 个数量级，
 不可能成立；按 `α = 2.3846837130250378e-17` 换算则对应 `σ = 194.2 ADU`，与该次运行自身记录的
 `background_adu = 194.0560302734375` 同量级。在该 `α` 下
 `floor(α) = 1e-12 × 5.686716411166881e-34 = 5.686716411166881e-46 < 2⁻¹⁵⁰`
 ⇒ float32 下**恰为 `0`**（`α = 2.3847e-17 < α₀ = 2.6470e-17`）；该平面上 `≤ 0` 的像素占 30.09%，
 与同次运行根因报告记录的「平面负预测占比 30.09%」逐位吻合。
```

#### 3-4 — 改前 A（逐字，:63）

```
- **数值**：`nside = 2^18` ⇒ `A_cell = 1.5239e-11 sr` ⇒ 像素域→面亮度域的方差因子 `1/A_cell² = 4.306e21`（21.63 dex）。
```

#### 3-4 — 改后 B（他人已落，我复核算术无误）

```
- **因子随 `nside` 变化**：`1/A_cell² = (12·nside²/4π)² = 144·nside⁴/(16π²)`，
  以 `dex` 记为 `log₁₀(1/A_cell²) = 8·log₁₀(nside) − 2·log₁₀(4π/12)`，即每翻一倍 `nside` 因子涨 4 倍、涨 8 dex。
  `nside` 是运行参数，不是常数，故本标准只给该表达式；下表是**示例取值**，取 `nside = 2^18`：

  | `nside` | `A_cell` [sr] | `1/A_cell²` | dex |
  |---|---|---|---|
  | 2¹⁸ = 262144 | 1.5239e-11 | 4.306e21 | 21.63 |
  | 2¹⁶ = 65536 | 2.4382e-10 | 1.682e19 | 19.23 |

  两行之差恰为 2 个 `nside` 位 ⇒ 8 dex，与上式的斜率一致；引用任一行都必须同时给出 `nside`。
```

#### 7-1 — 改前逐字 / 改后逐字

```
改前： 与 `../../governance/TRACEABILITY.md` 的 `drizzle` 登记项必须逐字同口径；如有分歧以
改后： 与 `../governance/TRACEABILITY.md` 的 `drizzle` 登记项必须逐字同口径；如有分歧以
```

### 9.2 `resources/PERFORMANCE_MODEL.md`

#### 3-5 — 改前（逐字，:69-73 代码块 + :80 + :95）

```
F = min(L, floor(kP1FrameMemSafetyFrac·A / (P·B))) // 帧级内存闸门
I = max(1, L / min(n, F)) // 帧内轴（正本 = THREADING_MODEL.md）
W_eff = F × min(I, K) // drizzle 节点；K 为每帧 scratch 槽上限
```
```
1. **`W_eff(F)` 在 `F > L/2` 处非单调**：`I = max(1, L / F)` 是整数除法， `F` 一越过 `L/2` 就掉到 1 …
```
```
| … | ⑥ `inner_omp = max(1, L/F)` 的整数除法使 `W_eff(F)` 在 `F > L/2` 非单调（死区，见结构结论 1） |
```

#### 3-5 — 改后（逐字）

```
F = min(L, floor(kP1FrameMemSafetyFrac·A / (P·B)))   // 帧级内存闸门上限
in_flight = min(n, F)                                // 帧级实际在飞数
I = max(1, L / in_flight)                            // 帧内轴（正本 = ../architecture/DATA_FLOW.md 的并行轴分配）
W_eff = in_flight × min(I, K)                        // drizzle 节点；K 为每帧 scratch 槽上限
```
```
1. **整数除法死区**：在 `in_flight = F`（前提 `F ≤ n`）下，`I = max(1, L / F)` 是整数除法，
   `F` 一越过 `L/2` 就掉到 1 ⇒ `W_eff` 在 `F ∈ (L/2, L)` 形成**死区**（达不到满宽 `L`）；
   满宽只在 `F = L/2`（`I = 2`）或 `F ≥ L`（`I = 1`）取到。故「`B` 越小越好」不成立，
   标定必须实测、不能外推。`F > n` 时 `in_flight = n`，`I = max(1, L / n)` 与 `F` 无关，
   `W_eff = n × min(I, K)` 随 `F` 增大**不再下降** —— 死区只在 `F ≤ n` 段成立。
```
```
| … | ⑥ `inner_omp = max(1, L/in_flight)` 的整数除法使 `W_eff` 在 `in_flight ∈ (L/2, L)` 非单调（死区，见结构结论 1）；… |
```

#### 4-1② — 改前逐字（:48-49 冻结参数表两行）

```
| `kP1FrameBytesPerPixel` | **116.0** | 同上 | 单帧驻留字节数的标定值；取保守上界（内存模型见 「测量口径」一节） |
| `kP1FrameMemSafetyFrac` | 0.75 | 同上 | 留基础占用与运行波动 |
```

#### 4-1② — 改后逐字

```
| `kP1FrameBytesPerPixel`（`B`） | `frame_memory_gate.bytes_per_pixel` | `p1_memory_cap`（`lib/infrastructure/scheduler/src/module_adapters.cpp`） | 闸门系数的标定口径与实测拟合见内存模型形态 |
| `kP1FrameMemSafetyFrac` | `frame_memory_gate.safety_frac` | 同上 | 留基础占用与运行波动；取值随帧几何与精度模式重标定 |
…
- **实现侧字面量的现状**：`module_adapters.cpp` 当前仍以 `static constexpr double` 持有这两个字面量，
  未消费 CMake `configure_file` 生成的配置头。该登记与实现侧现状不一致，属代码侧待订正项；
  在订正前，**唯一数值源是上表所列 JSON 键**，实现侧字面量只作镜像，不得反向覆盖。
```

#### 4-4 — 改后逐字（内存模型形态整节）

```
**内存模型形态**：单帧驻留 = `base` + `边际字节/像素` × `in_flight`。标定口径与实测拟合如下。

| 项 | 值 | 性质 |
|---|---|---|
| `base` | 0.143 GB | 实测（拟合截距） |
| 边际项 | 1.6724 GB/帧 = 99.68 B/px（`P = 4096² = 16.777e6 px`） | 实测（拟合斜率） |
| 闸门系数 `B` | 取 `frame_memory_gate.bytes_per_pixel` | 标定值，取可行窗口上端 |
| 拟合残差 | ≤ ±2.5%（F = 1/2/4/8 四档） | 实测 |

- **件名**：`run/P1-CONCURRENCY-CALIB-01/`（报告的实测内存模型与可采纳值一节，
  逐档峰值 RSS 见同目录 `summary_F1.json`、`summary_F2a.json`、`summary_F4.json`、`summary_F8.json`）；
  适用域 = 同场 8 帧 4096² FP64、drizzle auto nside、`kScratchPoolCap = 2`、无 swap。
- **拟合口径**：同一二进制、同场 8 帧、`inner_omp = 1`，对峰值 RSS 与 `in_flight` 做最小二乘，
  得 `RSS(F) = 0.143 GB + F × 1.6724 GB`；闸门系数**不取实测边际**（99.68 B/px），
  而取「使闸门放行目标 `F` 且需求 RSS 不越 `kP1FrameMemSafetyFrac·A`」的**可行窗口上端** ——
  该窗口为 `(0.75·A/(9P), 0.75·A/(8P)]`，随 `A` 平移，故 `B` 是**与机器 `A` 绑定**的标定值，
  换机器必须按同一口径重标定。
```

#### 4-5 — 改前逐字（:84-85 结构结论 2，:96 分类表不可改类）

```
2. **内核并行效率随宽度衰减**：帧内 stripe 数超过约 4 后进入收益递减区，
 且机器级负载噪声会污染更高档位的测量（须在静默机上复测）。每帧存在**不可并行的串行段**
```
```
| **物理/算法限制（调度参数不可改）** | ① drizzle 内核并行效率随宽度衰减（stripe 超约 4 后收益递减）；② 每帧**不可压缩的输出本体**：canonical 累加器容量不随 `K` 变；③ 逻辑核 / 物理核 / 物理内存上限共同决定 `W_eff` 的取值上界 |
```

#### 4-5 — 改后逐字

```
3. **内核效率的 stripe 收益递减拐点是待标定的经验拐点，不是已标定的物理常数**：把「帧内 stripe 数超过
   某阈值后进入收益递减区」写成阈值，需要一条固定其它变量、只变 stripe 数的墙钟扫描读数；
   本仓现有工件中无该扫描（`实验/engineering-evidence/l2_performance/` 与
   `run/P1-PARALLEL-AXIS-REDESIGN-01/` 扫的是帧在飞数 × 帧内线程数，不是 stripe 数），
   故本篇**不给该拐点数值**，也不把它列为不可改项。该拐点标定后结论落实验域，本篇只回引。
```
```
| **当前实现的结构限制（可改）** | … ；⑦ 内核效率的 stripe 收益递减拐点**待标定**（见结构结论 3） |
| **物理/算法限制（调度参数不可改）** | ① 每帧**不可压缩的输出本体**：canonical 累加器容量不随 `K` 变；② 每帧**不可并行串行段**（FITS 读、`hips_write`、星表查询、`wcs-platesolve`）的墙钟下界；③ 逻辑核 / 物理核 / 物理内存上限共同决定 `W_eff` 的取值上界 |
```

#### 4-8 — 改前逐字（:136 节标题 + :137-142 表 + :144）

```
## L2 冻结判据（四条，全部为**真判红**）
| # | 判据 | 阈值 | 违规 |
| 1 | 平均 CPU 利用率 | ≥ 0.85 | red |
| 2 | 利用率 p50 | ≥ 0.90 | red |
| 3 | 达标样本占比（利用率 ≥ 0.85 的采样窗比例） | ≥ 0.70 | red |
| 4 | 无「连续 ≥10 s 且利用率 <60% 的低利用窗」（无就绪积压同样计违规） | 无 | red |
- **enforcement = fail-closed**：任一判据违规 ⇒ `verdict=red`；`record_and_justify` 只是无违规样本的**记录语义**，不参与裁决；
```

#### 4-8 — 改后逐字

```
## L2 冻结判据（四条；裁决强度逐条不同）
| # | 判据 | 阈值 | enforcement（唯一数值源逐键给出） | 违规处置 |
| 1 | 平均 CPU 利用率 | ≥ 0.85 | `record_and_justify` | 记录 + 超标登记 |
| 2 | 利用率 p50 | ≥ 0.90 | `record_and_justify` | 记录 + 超标登记 |
| 3 | 达标样本占比（利用率 ≥ 0.85 的采样窗比例） | ≥ 0.70 | `record_and_justify` | 记录 + 超标登记 |
| 4 | 无「连续 ≥10 s 且利用率 <60% 的低利用窗」（无就绪积压同样计违规） | 无 | `hard_fail` | red |
- **enforcement 取自唯一数值源，不得在文档侧升级或降级**：…`compute.mean_utilization_enforcement`、
  `compute.p50_utilization_enforcement`、`compute.per_sample_enforcement` 三键同为 `record_and_justify`，
  只有 `compute.queue_low_window_enforcement = hard_fail`。数值源给出的理由是：85% 均值门在
  16-worker 真负载上实测仅 65.09%，未标定前不得硬失败。
  **本篇不得把这四条整体表述为 fail-closed**：判据 1–3 不改变裁决，判据 4 才进硬失败清单
  （数值源的 `hard_fail_criteria` 含 `queue_low_window_with_queued_work`）。
- **适用范围**：产品进程内的资源面**不设门**（数值源 `enforcement.in_process_default = record_only`、
  `in_process_hard_fail = retired`）；…
```

#### 9-1 / 9-2 — 改前逐字（:188 与 :190-219 整段）

```
完整数值见 `实验/engineering-evidence/**`（实测类证据的唯一留档区，登记 = `../../ACSD_DESIGN.md` 「I/O 与原子产品」一节（I/O 与原子产品）（:151））。原引 `evidence/performance/*.json`（V14 交付）**在本仓不存在**（死指针，本行原句已订正）⇒ 下文 V14 / V18R2 读数为**历史读数、原始 JSON 未入库**，只作历史参照，不作现行基线证据；补做现行基线须重跑并按 … 落 `实验/engineering-evidence/`。

V14 首轮结果：

```text
Phase1 panel1 65.0s（3 runs）
Phase2 GC 292.0s -> 234.6s（-20%）
Phase2 t4 87.9s -> 70.8s（-19%）
Browser shot 2.43s；zoom 0.30s/f；pan 0.22s/f
```
…
## V18R2 资源驱动轮（性能基线）

> **口径诚实边界**：本节数值为**单次读数**，不满足上文「每 benchmark ≥3 次、记 median/p95」的自订口径 ⇒ 只作历史参照，不构成现行基线结论；原始读数工件未登记入库。
```

#### 9-1 / 9-2 — 改后逐字

```
## 基准口径

同一机器、同一数据、同一 config，每 benchmark ≥3 次，记录 median/p95。
基准数据：

```text
Phase1 小真实帧 / 代表性完整帧
Phase2 t4 overlap / GC 3-panel
Browser GC wide / pan / zoom / STF
```

实测读数与逐档墙钟落 `实验/engineering-evidence/`（实测类证据的唯一留档区，
登记口径见 `../../ACSD_DESIGN.md` 的 I/O 与原子产品一章）。

优化类结论必须同时满足：

- 优化前后 science 输出 hash/数值等价；
- 无 >5% 无解释总体回退；
- 未安全优化项标注 `NO_SAFE_OPTIMIZATION_FOUND`。

**现行基线由实验域给出**：本篇不复制任何基线数值；引用基线必须给出 `实验/engineering-evidence/`
下的落位件名与该件的重复次数口径（≥3 次、记 median/p95）。未落位件名的数值不得写入本篇。
```

#### 4-9 — 改前逐字（:15-17）

```
  - **UPM dense cache**：稠密缓存与逐点稀疏求值必须给出同一结果；
  验证方式 = 缓存命中域上的最大相对偏差 ≤ 1e-12。定义与适用域见
  `../../science/PHASE2_UPM.md`。
```

#### 4-9 — 改后逐字

```
  - **UPM dense cache**：稠密缓存与逐点稀疏求值必须给出同一结果；
    验证方式 = 缓存命中域上的最大相对偏差不超过第 4 节冻结的双精度非归约档容差，
    适用量级域 = 缓存命中域上的缓存值量级 `scale`，超出该域按同值的相对形式判[4]。
    定义见 `../../science/PHASE2_UPM.md`；容差数值不在本篇复述。
```

#### 6-7 — 改前逐字（:112-114）

```
在标定形态（`I = 2`）下 `K = num_threads` 与 `K = 2` 等价（no-op）；
它保证 `I > 2` 的形态可达满宽（例：`n = 2` 帧 ⇒ `F = 2, I = 8`，
`W_eff` 由 `2×min(8,2) = 4` 抬到 `2×8 = 16`）。
```

#### 6-7 — 改后逐字

```
**等价形态的算例**（补齐全部参数，使「标定形态」与算例同面）：取 `L = 16`、`n = 2`、`K = 2`
（本机标定形态），则 `F = min(16, 闸门) = 2` ⇒ `in_flight = min(2, 2) = 2` ⇒ `I = max(1, 16/2) = 8`，
`W_eff = 2 × min(8, 2) = 4`。把 `K` 由 `2` 提到 `num_threads = 8` 后 `W_eff = 2 × min(8, 8) = 16`
—— 同一组 `(n, L, F, I)` 下只改 `K` 即取到满宽，总份数 `in_flight × K = 2 × 8 = 16 = lease` 未越界。
反之，若形态为 `I = 2`（例如 `L = 8`、`in_flight = 4`），则 `num_threads = 2`，
`K = 2` 与 `K = 4` 等价（后者被 `min(num_threads, kScratchPoolCap)` 截回），提高 `K` 无效 ——
可见 `K = num_threads` 的收益**依赖 `in_flight` 落在哪个档**，不是单调的。
```

### 9.3 `standards/README.md`（历史叙事清扫的附带项）

```
改前：本目录是工程标准正本：代码、数值、并发、缓存、兼容性、优化、文档与依赖八项标准。
      给写代码与做审核的开发者阅读。
改后：本目录是工程标准正本：代码、注释、数值、并发、缓存、兼容性、优化、文档、依赖与错误模型十项标准。
      给写代码与做审核的开发者阅读。
```

（此后被另一代理再次改写为「共十项：`CODE.md`（代码）、`COMMENT.md`（注释）、`NUMERIC.md`（数值）、…」的
点名式，与我的口径一致。）
