# DRIZZLE 核心算法推导与优化推导（审核包 · drizzle 章节）

> 文件：`run/review-package/derive-drizzle/DRIZZLE_DERIVATION.md`
> 作者：审核包 · drizzle 优化推导 章节作者（子代理）
> 基线工作树：`/workspace/Astro CS Database`，main
> 依据来源：P34（`run/perf-fix/P34-algo-research/`）、P35（`run/perf-fix/P35-algo-land/`）、
> P22（`run/perf-fix/P22-drizzle-par/`）、P15a（`run/perf-fix/P15a-drizzle-det/`）、P17（`run/perf-fix/P17-nside/`）
> 纪律：本文只读；仅写本节所在的 `run/review-package/derive-drizzle/`；未改主工作树、未做 git 操作、未编译。
> **代码结论全部由本包亲自 grep/read 复核**（见 §8 与附录 A）。

**标注约定**

- 锚 = `文件:行`（代码，行号取当前 main 工作树）或 `报告 §节` 或 `证据文件`；
- `【实测·原报告】` = 原报告实测值，本包未复算；`【实测·本包独立复核】` = 本包亲自复算/复读产物；
- `【推断】` = 由实测数据推导；`【外推】` = 跨规模/跨报告外推，含不确定度声明；
- **每个论断都必须有锚**；无锚的内容一律不出现在结论里。

---

## 0. 结论摘要（给负责人）

### 0.1 drizzle 是什么、贵在哪（一句话）

drizzle 把每帧抖动观测的**每个源像素**（球面四边形 drop）按**通量守恒权重** `w=a_jp/A_drop`
散播（scatter）到公共 HEALPix 网格的目标叶，再按**固定归约序**合并成 signal/support/variance/ivar 产品。
单核全帧（T4，16.2M 源像素，nside=65536）耗时 **740.06 s**，其中：

| 段 | CPU-s | 占 engine | 锚 |
|---|---:|---:|---|
| 行顶点 WCS | 7.099 | 0.96% | `run/perf-fix/P34-algo-research/logs/A2_fullframe.txt`; `drizzle_engine.cpp:1873-1886` |
| **geom（每源像素包裹）** | **730.227** | **98.67%** | `drizzle_engine.cpp:1921-1930` |
| ├ **候选枚举 cand** | **355.382** | **48.02%** | `drizzle_engine.cpp:1472-1480` |
| ├ **重叠求值 overlap** | **363.526** | **49.12%** | `drizzle_engine.cpp:1489-1570` |
| └ wrapper（包裹自身） | 11.319 | 1.53% | geom − cand − overlap（本包算） |
| post-stripe（归约/写出摊销） | 2.730 | 0.37% | engine − geom − wcs（本包算） |

⇒ **真正不相交的两大段各占约一半**：候选枚举 48.0% + 重叠求值 49.1% + 其余 2.9%。
**没有第三个 67% 的大段**（见 §2，这是本章的重点纠正项）。

### 0.2 三项优化的最终边界

| 项 | 段内加速 | 节点（21.62 µs/src 折算） | 产物影响 | 裁决 |
|---|---:|---:|---|---|
| **K2 增量候选枚举** | candp **2.92×**（P35 落地实测）/ 3.13–3.14×（P34 原型） | 省 9.382 µs/src（43.4%） | **BITWISE_IDENTICAL** | **冻结边界** |
| **TargetGeomCache O(1)** | 几何子段 1.017× | 省 0.033 µs/src（0.15%） | **BITWISE_IDENTICAL** | **冻结边界** |
| K1 切平面 2D 裁剪 | 纯 clip 1.162× / 纯 overlap 1.059× / 超段 1.034× | 省 0.370–0.713 µs/src（**1.7%–3.4%**） | **183 tile 不同**（p99 2.4e-7，max 3.1e-5） | **不做**（负责人裁决） |

- **冻结边界 = K2 + 缓存 O(1)**：逐位不变（本包独立复算三态消融，§3.5），节点折算 1.77×。
- 端到端单核 **1.524×**（M42 T2 M1 Red，`-c 8`）**属于“全补丁 ④”（含 K1）**，不是③的单测；
  ③ 的端到端干净窗口目前是**证据缺口**（§8 发现 D-2）。

### 0.3 仍未闭合（完整列表见 §7.3）

1. **K2 + 缓存 O(1) 目前只在 P35 shadow 树（补丁）里，main 工作树未落地**（本包亲验，§8 发现 D-1）；
2. **并行效率 / 内存带宽**：4 核只有 **1.944×**、逐像素工作占 4 核墙钟 **95.4%**、CPU 通胀 **1.99×** ⇒ 内存带宽/延迟受限（§5）；
3. **K1 的信息损失已定量但不被接受**（31/31.9M 像素 >1e-5）；
4. **phase3 bilinear 核等价改写**仍是 P34 认定的 B 线 open 项（P35 只落了记忆化，非改核）；
5. **分块遍历**（改归约序，可回收 5–8 µs/src）需负责人裁决；
6. **`sh_frac` 打印口径错误**（本包代码级定位，§8 发现 D-3），指标需订正后才能作为门禁量。

---

## 1. drizzle 在做什么（算法本体）

### 1.1 输入 / 输出与科学语义

- **输入**：一帧源图像 `pixels[y][x]`（FP32/FP64）、SNR/weight/variance 可选面、WCS+SIP、`pixfrac`、`nside`。
  入口 `hp_drizzle_run_phase1_hips`（`lib/healpix_db/healpix_drizzle/hp_drizzle_api.h:84`），最终调
  `DrizzleEngine::drizzleTiledImpl`。
- **drop（源像素 footprint）**：源像素四角经 `pixelToSky` 映射为球面四边形，再按 `half=0.5·pixfrac`
  收缩；`A_drop` = drop 内总面积（`docs/science/DRIZZLE.md:142`）。
- **权重与公式**（`docs/science/DRIZZLE.md:40-53`，SCI-DRZ-001）：

  ```
  w_jp = a_jp / A_drop,j
  F_p  = Σ_j x_j · w_jp          # 累计通量 → signal
  D_p  = Σ_j a_jp                # 覆盖面积 → support 底数
  S_p  = F_p / D_p               # 面亮度
  sumVarNum_p = Σ_j v_j · w_jp²  # 方差分子（SCI-DRZ-014）
  variance_p  = sumVarNum_p / D_p² ;  ivar_p = 1/variance_p
  ```

  代码锚：`drizzle_engine.cpp:1531`（`weight = overlap_area/drop_area`）、`:1553`（`sumFlux += pixelValue*weight`）、
  `:1554`（`sumArea += overlap_area`）、`:1557-1560`（**varianceValue 累加点**：`sumVarNum += (double)varianceValue*(double)w2`，
  `w2=weight*weight`；仅当 `varianceValue>0`，`:1912` 的 `<=0` 是合法数据边界而非掩膜）。
- **产品归一在 sink/writer，不在累加器**：
  `astro_sphere_sink.cpp:111-113` 把 `sumFlux/sumArea/sumVarNum` 装入 dense tile →
  `:128` 写 signal/support、`:137` 写 variance（全无效 tile 返回 rc=−5 跳过）；
  `write_hips_phase1` 路径 `astro_sphere_sink.cpp:285-296`：support=u8 量化的 `Σarea/A_cell`、signal=累计通量；
  归一公式与 invalid 语义锚 `docs/contracts/DATA_SEMANTICS.md:262-263,278-280,324-327,346-358`
  （`variance=var_num_sum/covered_area²`，`covered_area≤0/非有限→NaN`，ivar=1/variance）。

> **审核注**：`docs/contracts/DATA_SEMANTICS.md:263` 引用 `drizzle_engine.cpp:1531-1534` 作方差累加锚，
> 行号已漂移到当前 `:1557-1560`（该表自身在 `DRIZZLE_GEOMETRY.md:233 DISP-DRZ-007` 已登记过同类行号漂移）。
> 属文档锚点维护项，不影响语义。

### 1.2 管线五步（定义 / 位置 / 复杂度）

```
每源像素：
 Step1 行顶点 WCS      :1873-1886   2×(W+1) pixelToSky / 行     O(HW)
 Step2 drop 角点装配    :1384-1450  4 边点积 + 60″ 自适应阈值   O(1)/像素
 Step3 drop 几何        :1457-1464  build_drop_geometry_into    O(1)/像素
 Step4 候选枚举         :1472-1480  query_candidate_pixels_fast O(k²)+O(C log C)
 Step5 逐候选重叠+累加  :1512-1562  overlap_area_impl + TileAccumulator O(C)
每 stripe：归约           :1863-1980  stripe 升序左折叠             O(Σ touched)
```

1. **行顶点 WCS**（`drizzle_engine.cpp:1873-1886`，`prof_wcs_tl` 计时）：
   每行算上下两排顶点 `bot_vec/top_vec`（每线程私有 `shared_vertices`），
   每像素复用 4 个顶点缓存 ⇒ 每行 2×(W+1) 次 `pixelToSky`，而不是每像素 4 次。
   实测全帧 `wcs=7.099 s`（0.96%）。
2. **drop 角点装配**（`:1384-1450`）：4 条边做**点积**并与 `cos_thresh_60` 比较（常态 0 次 `acos`），
   仅超阈边走自适应细分（`:1396-1401`）；`drop_corners` 为 `thread_local` 复用向量（`heap_alloc=1/线程`，
   `A2_fullframe.txt heap_alloc=1`）。
3. **drop 几何**（`:1457`）：`build_drop_geometry_into` 从 double 角点源计算
   `drop_area`（Girard/切平面真路径，`spherical_overlap.cpp:1001`、`:1085`）、包围圆中心/半径、裁剪法向；
   `:1462` `drop_area<1e-20` 直接 return。
4. **候选枚举**（`:1475`）：`query_candidate_pixels_fast<double>`（`spherical_overlap.cpp:1527`）：
   由 drop 包围圆定 `(face, ix, iy)` 盒（含面内畸变 1.15 安全系数、极冠/跨 face 回退）、
   对盒内每格做 morton 解交织 + `pix2ang` 中心向量 + 与圆心点积过滤，最后 `std::sort` 升序。
   实测全帧 `cand/src=21.743`、盒 ≈81 格/源像素（9×9），**47.5% 的候选是真重叠**（`cand_eff=0.475`）。
5. **逐候选重叠 + 累加**（`:1512-1562`）：`compute_overlap_area_g_ctx_cached`（带线程私有
   `TargetGeomCache`，`spherical_overlap.cpp:1385`）→ `overlap_area_impl`（`:1168`，S-H 裁剪；
   `max_angle<1e-3` 用切平面面积 `:1282/:1325`，即 DISP-DRZ-005 的“切平面真路径”，**禁删**）→
   `weight` → `tileMap[parent].leaf(local)` 的 4 个字段累加（`:1549-1561`）。

### 1.3 固定归约序（P15a / P22）

- **stripe 划分只依赖输入高度**：`drizzle_deterministic_stripe_count(height)`，`kMinRows=16`、`kMaxStripes=256`
  （`drizzle_engine.cpp:1597-1600`）；边界 `y0/y1` 只由 `height` 与 stripe 数决定（`:1868-1869`）。
- **每 stripe 唯一线程按 (y,x) 行主序累加**（`:1871-1943`），无 atomic、无竞争。
- **按 stripe 索引升序左折叠归约**：P22 把“就地合并”改为 scratch map 池 + `pendingStripe` 槽
  （`:1950-1953` 立即 `continue`），任意线程按 `merge_cursor` 升序 merge（`:1960-1978`）。
  归约结合树 = `stripe 0..n-1` 升序左折叠，与线程预算无关（P15a REPORT §2.1，P22 REPORT §2）。
- ⇒ 产物对 `taskset` 1/2/4/8/16 **逐位一致**（P15a REPORT §3.2；P22 REPORT §3.1/§3.2）。

---

## 2. 成本分解的推导（本章重点）

### 2.1 四个“段”不是四个段，而是**两层嵌套括号**（代码级证明）

引擎 fine profile 把 wcs/geom/cand/overlap 四个计数分开打印（`drizzle_engine.cpp:2132-2139`），
但它们在代码里是**嵌套**关系，不是并列关系：

```
profP_accum[tid]        起点 :1865 (t_acc0)  ── 括住 y/x 二重循环 :1871-1943
  └─ prof_geom_tl[tid]    :1921-1930 / :1932-1940  ── 括住 processPixelSharedTiled 整函数
       ├─ g_tl_prof_cand    :1473-1480  ── 只括住 query_candidate_pixels_fast
       └─ g_tl_prof_overlap :1489-1570  ── 只括住逐候选重叠求值循环
```

代码事实（本包逐行复核）：

- `profP_accum` 起点 `t_acc0` 在 `:1865`，**在 `tileMap.clear()` 之后**，终点 `:1944-1946`；
  它包住 `:1871` 的 `for(y)` 与 `:1889` 的 `for(x)`——**包含行顶点 WCS 计时**。
- `prof_geom_tl` 在 `:1928`（shared_vertices 分支）与 `:1938`（`processPixelTiled` 分支）
  累加“`processPixelSharedTiled` 调用前后差”，**在 `profP_accum` 之内**。
- `g_tl_prof_cand`（`:1478`）与 `g_tl_prof_overlap`（`:1568`）都是 `thread_local`（声明 `:317-318`），
  **在 `processPixelSharedTiled` 函数体内**，因此**在 `prof_geom_tl` 之内**。
- 四者最终各自独立求和打印（`:2089-2094`）；打印行不含彼此差值，**不能相加**。

⇒ **`accum ⊇ geom ⊇ (cand + overlap)`**。用负责人给的四个数复核：
`1041.9 ⊇ 1048.9 ⊇ (498.9 + 534.0 = 1032.9)`；
`geom − (cand+overlap) = 16.0 ≈ 1.5%`（即 wrapper），`accum` 与 `geom` 差 −0.7%（负载噪声级）。
**四个数之和 3123.7 ≈ 给出的“总”3124 ⇒ 该“总”是四个嵌套计数之和，不是作业总 CPU。**
（数据锚：P22 REPORT §1.1 的 T4 auto 基线行：cand 498.924 / overlap 534.039 / geom 1048.939 / accum_cpu 1041.891。）

### 2.2 占比在 40 行 / 400 行 / 全帧三处一致

| 规模 | engine_s | geom / engine | cand / engine | overlap / engine | wrapper / engine | 锚 |
|---|---:|---:|---:|---:|---:|---|
| 40 行 | — | — | 48.3% | 48.8% | 1.46% | P34 REPORT2 §1.1（逐行归属表） |
| 400 行 | 117.2799 | 115.629 (98.59%) | 56.680 (48.33%) | 57.239 (48.81%) | 1.710 (1.46%) | `P34 .../logs/A2_profile.txt`（本包复读） |
| **全帧 3600 行** | **740.0556** | **730.227 (98.67%)** | **355.382 (48.02%)** | **363.526 (49.12%)** | **11.319 (1.53%)** | `A2_fullframe.txt`（本包复读） |

且 `cand/src=21.74`、`true_ov/src=10.33`、`tile_lk/src=10.33`、
`gcache 命中率 53.9%`（189,928,351 / (189,928,351+162,307,009)）、`sh_frac=0.656` 在 400 行与全帧**完全相同**
⇒ **分解与规模无关，可外推到 T4 全作业【外推】。**

### 2.3 真实不相交占比

以全帧单核 CPU-s 为分母（`A2_fullframe.txt`）：

- **候选枚举 = 48.0%**
- **重叠求值 = 49.1%**
- **其余 = 2.9%**（= wcs 0.96% + wrapper 1.53% + post-stripe 0.37%；本包相加复核 = 2.86%）。

折算 µs/源像素（16.2M 源像素）：cand 21.94、overlap 22.44、geom 45.07、wcs 0.44、wrapper 0.70、
post 0.17，engine 45.68。

### 2.4 被推翻的错误结论：“后两段合计 67%”——来源、纠正过程与留痕

**错误结论（前台两次判断）**

1. 把 `geom`（每像素包裹）与 `accum`（每 stripe 累加）当作两个**独立段**；
2. 用它们的和除以“四个计数之和 3124”：
   `(1048.9 + 1041.9) / 3124 = 67.0%`，据此判定“后两段合计 67%”。

**为什么错（算术可复现）**

- `accum` 与 `geom` 是**同一段工作的外/内层括号**（§2.1），不是两段；
- 分母 3124 是四个**嵌套**计数之和，分子里的 cand/overlap 还各自被 `geom`、又被 `accum` 重复计入
  ⇒ 用它对任意子集算占比都会**把 cand/overlap 重复计数约 2 次**（`(geom+accum)/geom ≈ 1.99`）。

**纠正过程（两次实测推翻，留痕）**

- **第一次推翻（400 行受控实测）**：引擎自带 fine profile，`taskset` 单核，1.8M 源像素：
  `geom=115.629`、`cand=56.680`、`overlap=57.239`、wrapper 仅 `1.710 = 1.46%`
  （`A2_profile.txt`；P34 REPORT2 §0）。⇒ “每像素包裹 geom 作为独立段只有 1.46%”，
  “每 stripe 累加”没有独立成本（= geom + 行顶点 + ≈0.4%）。
- **第二次推翻（全帧确认）**：3600 行 / 16.2M 源像素单核：`geom=730.227`、`cand=355.382`、
  `overlap=363.526`、wrapper `11.319=1.53%`（`A2_fullframe.txt`；P34 REPORT2 §0）。
  占比与 400 行一致 ⇒ 不是小规模假象。
- **结论订正**：负责人描述的目标两段（geom 包裹 / stripe 累加）**合计只有约 2%~4%**，不是 67%；
  **真实不相交占比 = cand 48.0% + overlap 49.1% + 其余 2.9%**。
  “总 CPU = 四个计数之和”这一读法本身是错的（P34 REPORT2 §0）。

> **本章把这条负结果当作正式推导结果保留**：它直接决定了后续优化的排序——
> 优化的正确靶子是 cand 与 overlap，而不是“geom 包裹”或“stripe 累加”。

### 2.5 从 CPU-s 到墙钟：并行不是免费的

单核全帧 engine 740.06 s；4 核（`taskset` 固定 4 核）engine **380.64 s ⇒ 仅 1.944×**，
且 `geom` CPU-s 从 730.227 涨到 1453.025（**通胀 1.99×**，cand 2.02×、overlap 1.96×）。
逐像素工作在 4 核墙钟里占 `(1453.025/4)/380.639 = 95.4%`。
⇒ 段内比值可靠，但绝对墙钟收益会被并行效率侵蚀（详见 §5）。

---

## 3. 三项优化的推导

### 3.1 K2：增量候选枚举

#### 3.1.1 几何论证

- 源像素尺度 6.31″、HP 像素 3.2208″（T4/nside=65536，P17 REPORT §1.2/§3.2）⇒
  相邻源像素的候选盒在 face 内**平移约 2 格**；9×9 盒交集 ≈ 49/81，只有约 **18–32 个新格**
  需要真正 morton + `pix2ang`。
- 实现（P35 补丁 `patches/P35-algo-land.patch:233-418`）：抽出 `p35_compute_fast_box`（与原 `fast`
  盒计算逐行一致），`thread_local CandidateBoxState`（≤81 格 × (ipix, 单位向量, ix, iy) ≈ 2.6 KB/线程）；
  新像素时保留与旧盒的**交集格**（含上一轮被过滤者），只对“新盒 − 旧盒”做 `morton2`+`pix2ang`，
  再用**新圆心对全部盒内格**做点积过滤，最后 `std::sort`。
- **原型首版踩过的坑（必须记住）**：交集里**上一轮被滤掉的格也必须用新圆心重判**；
  只重判“上一轮保留的格”会漏选（P34 REPORT §2.2 注；P35 有 N2 阴性对照，见 §3.1.3）。

#### 3.1.2 等价性论证

| 证据 | 数值 | 锚 |
|---|---|---|
| P34 原型：增量 vs 生产 `query_candidate_pixels_fast` 逐像素全等 | **252,000 源像素 mismatch=0** | P34 REPORT §2.2 |
| P35 落地：同进程 90,000 源像素、1,953,584 候选 | `{"mismatch":0}` | `P35 .../logs/dz_micro_nrows20_v2.txt`（本包复读） |

集合**逐元素全等（含顺序，因最终 `std::sort` 升序）** ⇒ 下游逐候选累加序列不变 ⇒ **产物逐位不变**（§3.5 已证）。

#### 3.1.3 实测加速与风险控制

| 边界 | 旧 | 新 | 比值 | 锚 |
|---|---:|---:|---:|---|
| P35 落地 candp 原语（µs/src） | 14.2652 | 4.8832 | **2.921×** | `dz_micro_nrows20_v2.txt` |
| P34 原型枚举段（µs/src，3 次） | 41.9/41.7/41.4 | 13.4/13.3/13.2 | **3.13–3.14×** | P34 REPORT §2.2 |
| P34 原型另一轮（nrows8） | 17.2698 | 4.9208 | 3.510× | `P35 logs/dz_micro_nrows8.txt` |

- **两个口径都对，边界不同**：P34 的原型段含原型自己的盒/排序开销（41 µs 量级，且是 prototype 边界）；
  P35 是**生产代码路径**的 `candp` 原语边界（14.27→4.88）。落地口径以 **2.92×** 为准。
- **确定性与风险**：候选集合相同 + `std::sort` 升序 ⇒ 与 P15a 固定归约序相容；
  阴性对照 N2 构造“只复用上一轮保留格”的漏选坑，必红（P35 REPORT §3）。
- **内存**：≤81×40 B/线程，无堆增长，与线程数无关（P34 REPORT §2.2）。

### 3.2 缓存 O(1)（TargetGeomCache）

#### 3.2.1 原实现的结构性缺陷

`spherical_overlap.cpp:1385-1419`（`TargetGeomCache::get_or_build`，本包复核）：

- 命中路径**线性扫描** `for(auto d=lru_.begin(); d!=lru_.end(); ++d)` 找 key（`:1391-1396`），
  容量 8192（`spherical_overlap.h:305`）⇒ 最坏 ~8192 次比较；
- `lru_` 是 `std::deque<uint64_t>`（`spherical_overlap.h:328`），`erase(iterator)` 内部还要搬移。

#### 3.2.2 改动与等价性

- P35 改为 `std::list<uint64_t> lru_` + Entry 保存 `lru_it`，命中时 `lru_.splice(lru_.begin(), lru_, it->second.lru_it)`
  （补丁 `:127-167`）。**MRU/LRU 顺序语义与旧的“逐次线性触达”完全相同**。
- 等价性证据：`dz_micro_nrows20_v2.txt` `CACHE {"geom_bitdiff":0, "old_hits":2107628,"new_hits":2107628,
  "old_misses":1799540,"new_misses":1799540}` ⇒ **命中/未命中序列逐项相同、几何逐位全等**；
  `③ vs ②` 产物逐位相同（§3.5，本包独立复算）。

#### 3.2.3 实测收益与**被修正的推断**

| 边界 | 旧 | 新 | 比值 | 锚 |
|---|---:|---:|---:|---|
| 几何子段（µs/src） | 2.0075 | 1.9747 | 1.017× | `dz_micro_nrows20_v2.txt` |
| 节点（折算 µs/src） | — | — | 省 0.033（**0.15%**） | P35 REPORT §9 |

> **纠正 P34 的推断**：P34 REPORT2 §3 结论 3 由“查找 L≈0.40 µs/候选”推断 O(cap) 扫描可回收
> **2–3 µs/src（3–5%）**；P35 落地实测只有 **0.033 µs/src（节点 0.15%）**——**该推断高估约 20–100×**。
> 原因【推断】：容量 8192 的 `deque<uint64_t>` 触达扫描在 L1/L2 内，单次仅几 ns；
> 真正的大头是 `unordered_map` 查找与几何构建，不是扫描。
> ⇒ 该项仍保留在冻结边界，但**理由是“零数值影响 + 去掉最坏情形热点”，不是收益**。

### 3.3 K1：切平面 2D 裁剪

#### 3.3.1 几何推导

- drop 边与目标像素边都是**大圆弧**；**大圆弧在 gnomonic 投影下是直线**
  ⇒ 在 drop 中心切平面内做 2D 直线裁剪，与球面 Sutherland–Hodgman 得到**同一交集多边形**
  （差 O(θ²)，θ=max_angle<1e-3 rad ⇒ 相对面积元 ~2.5e-9）。
- 该分支**原本就用切平面面积**：`overlap_area_impl` 在 `max_angle<1e-3` 时用 `planar_polygon_area_n`
  （`spherical_overlap.cpp:1282-1283`、`:1325-1326`；`DRIZZLE_GEOMETRY.md:231 DISP-DRZ-005`，**禁删**）。
  K1 只是把“裁剪”也搬到同一平面。
- 实现：`plane_clip_overlap_area`（补丁 `:21-101`）——每源像素 1 次正交基 + drop 4 边直线系数，
  每候选 4 角 gnomonic 投影 + 4 条直线 S-H + 鞋带面积，**零 sqrt/acos/normalize**；
  返回 <0 时回退原球面路径（背面角点/退化/缓冲溢出），**绝不返回错误的正值**。

#### 3.3.2 覆盖率对账

PATHS 仪器的分类逻辑（本包复核 `P35 .../src/bench_p35_paths.cpp:119-155`）：
对每个 query 候选依次判定 bounding-circle 拒绝（`nq`）、叶全含（`nf`）、drop 全含（`nd`）、
否则进裁剪（`nsh`）；`f_clip = nsh/(nq+nf+nd+nsh)`。

| 出口 | 计数（全帧 4 段 ×450k src） | 占比 | 锚 |
|---|---:|---:|---|
| 进入裁剪 `clip` | 38,631,991 | **98.70%** | `dz_paths_fullT4.txt`（4 行 0.9870–0.9873） |
| — 切平面成功 `plane` | 38,631,991 | **100%** | 同上 |
| — 切平面回退 `plane_fallback` | **0** | **0%** | 同上 |
| leaf 全含 `leaf_fully` | 500,805 | 1.30% | 同上 |
| bounding-circle 快速拒绝 `quick` | ~5 | ~0 | 同上 |
| drop 全含 `drop_inside` | 0 | 0 | 同上 |

⇒ **K1 覆盖 S-H 主路径 100%，无需扩展适用面**（P35 REPORT §4.1）。

#### 3.3.3 `sh_frac=0.656` 的分母口径——本包代码级订正

P35 REPORT §4.1 澄清“`sh_frac=0.656` 是 query 层候选中到达 S-H 的比例（分母含 query 级 quick-reject），
不是 K1 分支覆盖率”。**结论正确，但机制表述不精确**。本包复核代码：

`drizzle_engine.cpp:2143-2145`：
```cpp
const double sh_frac = (totalOps.sh_calls + totalOps.quick_rejects) > 0
    ? (double)totalOps.sh_calls / (double)(totalOps.sh_calls + totalOps.quick_rejects) : 0.0;
```
而 `totalOps.sh_calls` 在 `:1513` 对**每个候选**无条件 `++`，因此
**`sh_calls ≡ candidates`**（全帧 ops 行同时打印 `cand=352235360` 与 `sh=352235360`，本包核对）。

⇒ 打印的 `sh_frac = cand/(cand+quick_rej)`，是“候选数 / (候选数 + overlap 层快速拒绝数)”，
**不是**“到达 S-H 的比例”（后者应由球面 profile 的 `n_sh` 作分子；全帧 `n_sh=347,721,482`）。
真正的 K1 分支覆盖率由 P35 PATHS 仪器的 `f_clip` 给出 = **98.70%**（§3.3.2）。
**这是一处指标口径缺陷（§8 发现 D-3），不影响任何优化结论。**

#### 3.3.4 误差界与守恒

| 指标 | K1 | 旧 S-H 基线 | 冻结容差 | 锚 |
|---|---:|---:|---|---|
| 单候选权重差 max（全帧 1.8M src） | **1.788e-8** | — | FP32/FP64 逐 leaf <1e-5 | `dz_maxdw_fullT4.txt`; `DRIZZLE_GEOMETRY.md:206-216` |
| 切平面 vs 球面面积差 max | 8.5e-18–1.6e-17 | — | — | `dz_paths_fullT4.txt plane_vs_sh_maxabs` |
| 逐源像素通量守恒残差 max | **4.675e-10** | **4.843e-08** | FP64 通量闭合 <1e-6 | `dz_micro_nrows20_v2.txt`; P34 REPORT §2.3 |

⇒ K1 的守恒残差比现状路径**好两个量级**；`Σoverlap` 全样本和一致到 7 位有效数字（P34 REPORT §2.3）。
**但数值更好不等于产物不变**——见下。

#### 3.3.5 三态消融（差异 100% 来自 K1）

**本包独立复算**（用 P35 自带 `scripts/compare_p1.py`，对 4 个真实 phase1 产物目录两两比较，
科学面 = HiPS tile/FITS + p1_*.json（归一化易变字段）+ signal/support properties（去时间戳））：

| 组合 | 科学面逐位 | 差异 tile | 本包结论 |
|---|---|---:|---|
| ① baseline ↔ ② K2ONLY | **BITWISE_IDENTICAL** | 0 | K2 不动产物 |
| ② K2ONLY ↔ ③ K2+CACHE | **BITWISE_IDENTICAL** | 0 | 缓存 O(1) 不动产物 |
| ③ K2+CACHE ↔ ④ ALL(+K1) | **DIFF** | **183** | **差异 100% 来自 K1** |

（原始 JSON：`P35 logs/p1_ab_summary.json` 的 `ab_baseline_vs_patched` = 183；
本包对 K2ONLY/K2CACHE 的两次比较命令与输出见附录 B。②③ 目录当前仍在 `P35-algo-land/work/`。）

⇒ **冻结边界（K2 + 缓存 O(1)）在真实产物层面逐位不变**；K1 是唯一改变产物的一项。

#### 3.3.6 产物容差分布

③ ↔ ④（183 个差异 tile，31,900,699 像素，`P35 logs/k1_tile_diff_dist.json`）：

| 指标 | 值 |
|---|---|
| 差异像素 | 17,726,260（55.6%） |
| **rel p50** | **6.740e-8（≈1 个 float32 ULP）** |
| rel p90 / p99 / p99.9 | 1.332e-7 / 2.415e-7 / 3.151e-7 |
| **rel max** | **3.145e-5** |
| >1e-5 的像素 | **31 / 31.9M（1e-6）**；>1e-4 = **0** |
| max abs | 6.79e9（数据范围 ~1.9e16 ⇒ 3.6e-7 of range） |
| 附带 | `signal/properties` 的 `hips_data_range` 微变；其余 properties 不变 |

尾部来源【推断】：`δv = Σδw_i(v_i−v)/Σw_i` —— 加权累加对**亮离群源像素**的放大；
权重层仍 ≤1.79e-8、守恒更好（P35 REPORT §4.3）。

#### 3.3.7 为何最终不做 K1

| 边界（同一份 `dz_paths_nrows20.txt`） | 旧 | 新 | 省 | 占节点 21.6163 |
|---|---:|---:|---:|---:|
| 纯 clip 算术（µs/src） | 4.0693 | 3.5033 | 0.566 | 2.6% |
| 纯 overlap 段 | 6.6948 | 6.3245 | 0.370 | **1.7%** |
| 超段（candp+几何+overlap） | 21.6163 | 20.9034 | 0.713 | **3.3%** |

- **裁决依据**：K1 只值**节点 1.7%–3.4%（≈2–4%）**，却是**唯一改变产物**的改动
  （183 tile，尾部 31 像素 >1e-5）；**P34 最初预登记的“应得收益 8.7–14%”没有实现**——
  落地形态的 S-H 是 `nb==4` 单次四边形、复用预计算 clip normals，实测纯 clip 只有 1.162×，
  而非 P34 昂贵 S-H 变形的 1.6×（P35 REPORT §4.2 明确“两个口径都对，边界不同”）。
- ⇒ **不做的理由 = 收益 ~2–4% × 100% 改变产物**，风险收益比不成立；**K2 单独已是 1.5× 级**。
- **【审核注】** P35 内部两个数字（§9 表“1.7%”、§4.2 表“超段 1.034× ⇒ 3.3%”）边界不同但不矛盾；
  负责人看到的“1.7–4%”即此区间。若未来 K1 被重新考虑，须以**超段口径 3.3%** 作为收益上限，
  并重新跑 FP32/FP64、常量场、通量闭合与产物级科学门。

---

## 4. 负结果与上界（完整清单，必须逐条列出）

### 4.1 scatter → gather（按输出 tile 收集）

- **局部性现状**：`tile_lk = 10.3 次/源像素`（哈希 + 叶子数组随机写），实测 ≈50 ns/次 ⇒
  **L1/L2 命中**（DRAM miss 单次 ≥100 ns）；原因：stripe=16 行带只覆盖 ≈1.5 个 tile，
  工作集天然小（P34 REPORT2 §1.2/§2）。
- **收益上限**：整个 wrapper（drop 几何 + 全部 scatter + 计数）= **0.95 µs/src = 1.46%**；
  gather 要新增“按 tile 分桶/收集”中间结构，其自身开销与这 1.46% 同量级 ⇒ **上限 <1.5%**。
- **归约序**：现定义 = stripe 索引升序左折叠、stripe 内 (y,x) 行主序；gather **必然改变 stripe 内顺序**
  ⇒ 浮点结合律变化 ⇒ 产物 bitwise 变。可重定义为“新的确定性顺序”，但会改变全部历史产物与基线。
- **判定【实测·负结果】**：**能改，但不值**（上限 <1.5% + 破坏归约序）。锚：P34 REPORT2 §2。

### 4.2 几何复用 / 增量 / 解析化的绝对上限

- **同进程 A/B（cap=1 vs cap=8192）**：14.478/14.251/25.134 → 11.456/11.205/19.138
  ⇒ 整个目标几何缓存只值 **3.0 µs/src = 引擎 4.6%**（P34 REPORT2 §3，3 次一致）。
- ⇒ **任何“几何复用改进”的绝对上限就是这 4.6%**（含“完美复用”：由引擎计数
  `gcache_miss/src=10.02` 与实测构建成本 B≈0.26 µs 解得，完美复用仅再省 **0.65%**）。
- **解析化/近似**：切平面解析已在用；**查表已证伪**（§4.4）。⇒ 该方向基本挖尽。

### 4.3 融合（已做完）

- `shared_vertices` 行顶点缓存（跨 stripe 复用，每线程私有）、`thread_local` drop 角点/几何复用
  （`heap_alloc=1/线程`）、候选→几何→裁剪→累加全在**同一个 x/y 二重循环**内，**无中间结构落盘/跨趟往返**；
  post-stripe（归约+写出）实测 ≈0.35%。
- ⇒ **没有可融合的往返**；唯一“两趟”是为保序而有意拆开的（P34 REPORT2 §4）。

### 4.4 亚像素相位查表

| 相位 bin K/轴 | 箱内极差 max | LUT 误差 max | LUT 误差中位 |
|---:|---:|---:|---:|
| 8 | 3.65e-2 | 1.98e-2 | 1.37e-3 |
| 64 | 6.25e-3 | **4.12e-3** | 8.93e-4 |

- K 翻倍误差只降到 ~0.5×（若仅量化限制应 ~0.25×）⇒ 残差被**非平移不变性**主导
  （face 内畸变 + drop 形状/朝向随位置缓慢变化）；
- 外推达 1e-5 需 K≈2.5e4/轴（**≈6.3e8 表项**）且下降仍缓慢；
- **用于候选集合同样不可行**：生产已登记 `CANDIDATE_QUERY_REPAIR`（平面圆预过滤会漏选，
  对角线实测通量丢 0.6%），保守集合只能靠盒 + 球面距离判定。
- 锚：P34 REPORT §2.4；`docs/algorithms/DRIZZLE_GEOMETRY.md:206-216`（1e-5 冻结容差）。

### 4.5 分块遍历（提升几何复用）

- 按整行 4500 源像素推进 ⇒ 同一目标像素被 ~4 个源像素访问，其中“下一行”的访问者相隔
  `4500×21.7 ≈ 9.8e4` 次访问 ≫ 8192 ⇒ **垂直复用全丢，只剩行内复用（距离 ~65）**，
  这正是命中率只有 54% 的成因；
- 分块遍历（如 256×256 源像素块）可把工作集压进缓存，预期回收 5–8 µs/src；
  **但会改变叶累加顺序 ⇒ 破坏与现状 bitwise 一致**，属 P15a 归约序范畴，须与负责人协商并重跑科学门。
- ⇒ **不做**（P34 REPORT §2.5、P34 REPORT2 §6、P35 REPORT §10）。锚：P34 REPORT §2.5(b)。

### 4.6 其它负结果 / 方法不可用项

| 方向 | 判定 | 锚 |
|---|---|---|
| 单纯扩大几何缓存容量 | 收益有限（8192 已打满，跨行复用距离 ~1e5） | P34 REPORT §4 |
| 可分离核两遍法 | 无收益且违反 `PHASE3_RSMP_IMPL.md:305-311` §13 | P34 REPORT §4 |
| 导出融合进 phase2 | 治理违规（宪章 §3.2 + DATA-002），且只省 I/O 不省核 | P34 REPORT §4 |
| 按产品需要降分辨率 | 无许可条款（order_sel 上限 = 输入实际 order） | P34 REPORT §4 |
| `CLOCK_THREAD_CPUTIME_ID` 细粒度计时 | 方法不可用（syscall 0.5–1.4 µs，把 ~0.5 µs 测成 4.2 µs） | P34 REPORT §1.2 |
| `nice/taskset` 下跨 run 绝对时间对比 | 方法不可用（同 mode 三次漂移 2.2×） | P34 REPORT §1.1 |
| phase3“直接 2×2”非等价改核 | 已裁决不做（oracle：99.8% 候选集合不同，合成场 max\|Δ\|=0.97） | P35 REPORT §2 |
| P15b“包含优先快路径” | 正确采样下 dropin=0.00%，**不落地** | P17 REPORT §4 |

### 4.7 上界汇总

| 项 | 可省（µs/src 单核，P34 口径） | 占引擎 | 保序? |
|---|---:|---:|---|
| K2 增量枚举 | 21.5 | **33.0%** | ✅ |
| K1 切平面 | 5.7–9.1（P34 原型）/ 0.37–0.71（P35 落地） | 8.7–14% / **1.7–3.4%** | ✅ |
| 缓存 O(1) | 2–3（P34 推断）/ **0.033（实测）** | 3–5% / **0.15%** | ✅ |
| 几何复用 | ≤3.4 | ≤4.6%（绝对上限） | ✅ |
| scatter→gather / 融合 | <1.5% | <1.5% | ❌ 改归约序 |
| **K2+K1（P34 理论）** | 27–30 | — | ⇒ 1.72–1.85× |
| **K2+缓存（P35 实测，冻结边界）** | 9.415 | — | ⇒ 节点 **1.77×** |
| **全补丁（P35 实测，含 K1）** | 9.785 | — | ⇒ 节点 **1.82×** |

**理论单核吞吐上界 ≈ 1.9–2.0×**（仅由“两段各占 48%”与已实测段内比值决定，只做保序改动）。

---

## 5. 并行维度

### 5.1 4 核只有 1.944×（三条互相印证的证据）

| 证据 | 400 行（1.8M src） | 全帧（16.2M src） |
|---|---:|---:|
| engine 1 线程 | 117.2799 s | 740.0556 s |
| engine 4 线程 | 61.0876 s | 380.6392 s |
| **加速比** | **1.92×** | **1.944×** |
| geom CPU-s 通胀 | 1.91× | **1.99×** |
| cand / overlap 通胀 | — | 2.02× / 1.96× |
| 逐像素工作占 4 线程墙钟 | 90.6% | **95.4%** |

锚：`P34 .../logs/A2_profile.txt`、`A2_fullframe.txt`（本包复读并复算全部比值）。
**cand 与 overlap 同幅通胀（2.02× / 1.96×）⇒ 不是某一段的并行缺陷，而是整段的内存子系统瓶颈。**

### 5.2 判定：内存带宽 / 延迟受限【实测 + 推断】

- 逐像素工作在 4 线程墙钟里占 **95.4%**，其余（行顶点/调度/失衡/stripe 收尾）只有 4.6%；
- 每加一个线程，**每单位工作的 CPU-s 翻倍**（CPU 通胀 1.99×）——
  【推断】多出的核大部分耗在缓存一致性/访存等待上；
- 与 400 行尺度完全一致（1.92× / 90.6%）⇒ 不是全帧特有的数据结构问题。
- ⇒ **墙钟问题的另一半是并行效率（属 P30/P22 范围），而不是“本文所查的两段存在隐藏工作”。**

### 5.3 加核的收益上限在哪里

- 【实测】1→4 核 1.944×（效率 48.6%）；**继续加核无法突破内存带宽**：
  在逐像素工作占 95.4% 且 CPU 通胀 ~2× 的前提下，墙钟 ≈ 逐像素 CPU-s 总量 / 有效带宽，
  加核只能摊薄那 4.6% 的“其余”，对 95.4% 的部分几乎无增益。
- ⇒ **提高单核产出速度（K2/K1/缓存）比继续加核更有效**；这也解释了为什么“43+ 分钟、176% CPU”
  是“两段各占一半 + 并行效率只有 ~48%（4 核）/更低（共享内存带宽）”的组合，**不存在另一个 67% 的大段**。
- **【外推·带不确定度】** 跨报告粗算：P22 T4 auto（同为 3600 行/nside=65536）16 线程 engine 68.036 s
  vs P34 单核 740.056 s ⇒ 约 10.9×（16 核效率 68%）。该数与 P34 的 4 核 48.6% 不构成单调曲线，
  **因为两次测量在不同报告、不同时间窗、不同负载下进行**（P22 §4.2 自记 load 1.3–19.8，P34 REPORT2 记 load 2–25）。
  ⇒ 只能确认“多核显著低于线性”，**不能据此画 scaling 曲线**；画曲线须同批交替 A/B（P22 §9 明确列为未完成项）。
- **与 P22 的关系**：P22 修的是**归约串行关键路径**——T4 auto 归约串行 `merge_work_cpu=4.382 s = 6.3% 墙钟`
  （完全叠加），修复（scratch 池 + 升序流水线合并）后有用核 14.96/16=93.5% → 15.22/16=95.1%，
  且**产物逐位不变**（P22 REPORT §0/§1.2/§4.1）。加 slack（K=+8）可到 97.7%，但 RSS +3.1 GB，**已拒绝**
  （违反 P21 内存界定）。
  ⇒ **P22 解决的是 6% 的串行尾巴；P34/P35 的 95.4% 逐像素内存受限是另一个维度，二者不重叠、可叠加。**
- **P21 的“T2 drizzle 8.6/16”是负载污染**：低载复测 557.33 CPU-s/41.93 s = 13.3/16（引擎 94.4%）
  （P22 REPORT §5）。

### 5.4 与 P15a 确定性归约序的关系

- P15a 用**固定 stripe 划分 + stripe 升序左折叠**把归约结合树定义为**输入的函数**，
  消除了“按线程号归并”导致的跨预算产物差异（P15a REPORT §2.1/§3）。
- P22 在该序不变的前提下把归约**解耦**（scratch 池 + 任意线程按 cursor 升序合并），
  产物与修复前**逐字节一致**（P22 REPORT §3.2：fp64 `.norm.hiss` sha256 `6b1a5750…` 与 P15a 登记值一致）。
- **K2/K1/缓存三项都不触碰归约序**：K2 改变的是候选集合的**生成方式**（集合本身逐元素相同），
  K1 改变的是“给定候选集合后如何算权重”，缓存改变的是几何取用路径 ⇒ 与 P15a/P22 相容。

---

## 6. 确定性契约

### 6.1 产物与线程预算无关

| 结论 | 证据 | 锚 |
|---|---|---|
| 修复前：5 个预算 5 个不同产物 | `taskset` 1/2/4/8/16，fp32/fp64 皆然 | P15a REPORT §3.1 |
| 修复后：单一 sha256 | fp32 `898a0237…`、fp64 `6b1a5750…`（1/2/4/8/16 × r1/r2 全等） | P15a REPORT §3.2 |
| P22 后仍逐字节一致 | 合成 fixture 1/2/4/8/16 `.norm.hiss`/`.canon` base==fix；新增 `p1drz_merge_pipeline_lock` 47.08 s PASS | P22 REPORT §3.1/§3.2 |
| 补丁端到端跨预算 | 补丁 c8/c8-11/c8-15 两两 **BITWISE_IDENTICAL**（371 科学面文件 0 差异） | `P35 logs/p1_thread_independence.json`（本包复读） |
| 基线对照 | 基线 c8/c8-11/c8-15 同样 BITWISE_IDENTICAL（证明哨兵非恒真） | `p1_ab_summary.json baseline_thread_control` |
| 阴性对照 | 突变 `n_stripes += num_threads%3` ⇒ 两把锁均 FAIL，恢复后 PASS | P22 REPORT §3.4 |

### 6.2 与 P15a 固定归约序的关系

产物对预算无关的**根因**是把归约结合树固定为输入的函数：stripe 数只依赖 height（`drizzle_engine.cpp:1597-1600`），
stripe 边界只依赖 height/stripe 数（`:1868-1869`），归约按 stripe 索引升序左折叠（`:1960-1978`）。
**K2/K1/缓存不改变这个序**，所以补丁跨预算仍逐位一致。

### 6.3 `p1_final.json` 差异只是输出路径（非确定性缺陷）

**本包独立复核**：对补丁 `c8` 与 `c8-11` 的 `p1_final.json` 做原始 JSON diff，只有两行不同：

```
"hips_root": .../M42_T2_M1_Red_P1_PATCHED_c8   vs   ..._c8-11
"properties": .../M42_T2_M1_Red_P1_PATCHED_c8/signal/properties  vs ..._c8-11/...
```

即**输出目录路径**（测试装置用了不同 `output_dir`），归一化后为 0；
其余 `resource_*/alloc_*/graph/*.dot` 等为时间/线程元数据（`compare_p1.py` 单列 `resource_trace_diff=11`）。
⇒ **不是科学面非确定性**（P35 REPORT §7 的表述经本包证实）。

### 6.4 四项改动各自的确定性归类

| 改动 | 对归约序 | 对产物 | 归类 |
|---|---|---|---|
| K2 增量枚举 | 不改（候选集合逐元素全等） | BITWISE_IDENTICAL | 保序 |
| 缓存 O(1) | 不改 | BITWISE_IDENTICAL | 保序 |
| K1 切平面 | 不改（同一交集，O(θ²)） | 183 tile（p99 2.4e-7，max 3.1e-5） | **数值近似** |
| phase3 记忆化 | 不改 | 逐位相同（1,048,576 px × 10 趟） | 保序（B 线） |

---

## 7. 结论

### 7.1 冻结边界 = **K2 + 缓存 O(1)**

- **逐位不变**（本包独立复算 ①↔②↔③ 两次比较均 BITWISE_IDENTICAL，§3.5）；
- **节点折算 1.77×**（同进程核内 A/B：节点 21.6163 → 12.201 µs/src）；
- 与 P15a/P22 确定性契约相容（§6.2）；
- 建议落地文件：`lib/healpix_db/healpix_drizzle/spherical_overlap.cpp`（+`.h`）与
  `drizzle_engine.cpp` 行循环状态（P35 补丁 `patches/P35-algo-land.patch`）。

> **口径提醒**：P35 测得的端到端单核 **1.524×**（354.13→232.27 s，`-c 8`）是 **④ 全补丁（含 K1）** 的数字；
> **不是**“K2+缓存”单独的数字。冻结边界 ③ 的端到端干净窗口尚未取得（§8 发现 D-2）。

### 7.2 不做项（含负责人裁决留痕）

| 项 | 裁决 | 理由 |
|---|---|---|
| K1 切平面 | **不做** | 节点只省 1.7–3.4%，却是唯一改变产物（183 tile / 31 px >1e-5） |
| scatter→gather | 不做 | 上限 <1.5% + 破坏归约序 |
| 分块遍历 | 不做 | 改归约序（bitwise 变化），须负责人裁决 |
| 几何复用再优化 | 不做 | 绝对上限 4.6%，已接近 |
| phase3 直接 2×2 改核 | 不做（已裁决） | oracle 证明非等价 |
| phase3 改 nearest | 不做 | 改科学口径；须负责人裁决 |
| 归约池加 slack | 拒绝 | RSS +3.1 GB，违反内存界定 |

### 7.3 仍未闭合的问题

1. **落地缺口**：K2/缓存 O(1) 只在 P35 shadow 补丁，**main 未落地**（本包亲验，§8 D-1）；
2. **并行效率 / 内存带宽**：4 核 1.944×、逐像素 95.4%、CPU 通胀 1.99× ⇒
   加核收益上限受内存子系统限制（属 P30/P22 范围，§5.3）；
3. **phase3**：bilinear 核等价改写仍是 open（P34 REPORT §3.1/§7；P35 只落记忆化 1.562×）；
4. **K1 的产物级尾部**（31/31.9M >1e-5）已定量但不被接受；若未来重开须重跑科学门；
5. **分块遍历**（改归约序，可回收 5–8 µs/src）需负责人裁决；
6. **`sh_frac` 指标口径错误**（本包代码级定位，§8 D-3），订正前不应作为门禁量；
7. **P17 采样率合规暴露的下游门禁问题**：`alloc_growth_unbounded`（工作集 ∝ n_healpix_pixels，
   被整条曲线 Theil–Sen 斜率误判）与 `low_avg_cores`（HISS→HiPS writer 单线程整表扫描 3.6e8 次迭代）
   ——属 phase1 writer/门禁范畴，但会掩盖 drizzle 侧的真实并行度（P17 REPORT §5.1/§5.2）。

### 7.4 落地前必须补的门（证据缺口）

- ③（K2+缓存）的**端到端**同批交替 A/B（干净窗口，避免 D-2 的负载污染）；
- K2 候选集合 oracle 扩到**全帧**（当前 252k + 90k 像素 mismatch=0）；
- 缓存 O(1) 的 `.canon/.norm.hiss` **逐位不变**回归锁（设计上应完全 bitwise 相同）；
- 真实 T4/T2 全作业的引擎内 profile 复核（占比三处一致已证，落地后需再看绝对值是否受内存带宽改善）。

---

## 8. 审核发现（本包新增，与既有报告的口径订正）

| # | 发现 | 证据 | 影响 |
|---|---|---|---|
| **D-1** | **K2/缓存 O(1)/K1 均未落在 main** | 本包 grep `lib/healpix_db/healpix_drizzle/`：`p35_compute_fast_box`/`CandidateBoxState`/`plane_clip`/`prevBox` 命中数 **0**；`TargetGeomCache::get_or_build` 仍是 `spherical_overlap.cpp:1391` 的 O(cap) 扫描 | 冻结边界是**待落地提案**，不是现状 |
| **D-2** | **端到端 1.524× 属 ④ 全补丁，不属 ③** | `P35 logs/p1_timing.json` 的 `PATCHED_c8`（=all 变体）；③ 的 `p1_K2ONLY_c8-11.time` wall=**255.96 s**（user 379 s / wall ⇒ 平均仅 1.48 核）明显被外部负载污染 | ③ 的端到端收益**目前无干净实测**；节点折算 1.77× 是核内 A/B |
| **D-3** | **`sh_frac` 打印口径不一致** | `drizzle_engine.cpp:2143-2145` 分母 = `sh_calls+quick_rejects`，而 `sh_calls ≡ candidates`（`:1513` 每候选 ++，全帧 ops 行 `cand==sh==352235360`）⇒ 打印值 = `cand/(cand+quick_rej)`；K1 真实分支覆盖率由 PATHS `f_clip`=98.70% 给出 | P35 §4.1 的“分母含 query 级 quick-reject”表述不精确；指标应订正 |
| **D-4** | **P34 对缓存 O(1) 的收益推断高估** | P34 REPORT2 §3 推断 3–5%；P35 实测节点 0.15%、几何子段 1.017× | 该改动保留的理由是“零数值影响”，不是收益 |
| **D-5** | **K1 收益随边界在 1.7%–3.4% 间变化** | 同一 `dz_paths_nrows20.txt`：纯 overlap 省 0.370（1.7%）、纯 clip 省 0.566（2.6%）、超段省 0.713（3.3%） | 报告用“1.7%”单值；建议以超段 3.3% 为上限口径 |
| **D-6** | **K2 加速比有两个边界值** | P34 原型 3.13–3.14×（原型段 41.9→13.3 µs/src）；P35 落地 2.92×（生产 candp 14.27→4.88） | 落地口径以 **2.92×** 为准；两者不矛盾 |
| **D-7** | **`DATA_SEMANTICS.md` 方差累加锚行号漂移** | 该文件 `:263` 写 `drizzle_engine.cpp:1531-1534`；现行正确锚为 `:1557-1560` | 文档维护项（与 DISP-DRZ-007 同类） |
| **D-8** | **跨报告 scaling 曲线不可直接拼接** | P22 16 线程 68.04 s vs P34 单核 740.06 s（10.9×）与 P34 4 核 48.6% 效率不自洽；两次测量时间窗/负载不同 | 只能陈述“显著低于线性”；画曲线须同批交替 A/B |
| **D-9** | **三态消融由本包独立复算确认** | K2ONLY/K2CACHE 目录仍在 `P35-algo-land/work/`；本包用 `compare_p1.py` 复算：①↔②=0、②↔③=0、③↔④=183 | 补上了 P35 `p1_ab_summary.json` 未保存 ②③ 比较的证据缺口 |

---

## 附录 A：锚点索引

**代码（当前 main 工作树，本包逐行复核）**

- `lib/healpix_db/healpix_drizzle/drizzle_engine.cpp`
  - `:1360` processPixelSharedTiled；`:1384-1401` 边点积/自适应；`:1405-1450` drop 角点；
    `:1457-1464` drop 几何/drop_area；`:1472-1480` cand 计时+`query_candidate_pixels_fast`；
    `:1489-1570` overlap 计时+逐候选循环；`:1512-1518` 重叠调用；`:1531-1536` weight/true_overlaps；
    `:1548-1561` 累加（`:1557-1560` **varianceValue 累加点**）
  - `:1597-1600` stripe 数；`:1865-1946` `t_acc0`/`profP_accum`；`:1868-1869` 固定边界；
    `:1871-1943` y/x 循环；`:1921-1930`/`:1932-1940` `prof_geom_tl`；`:1950-1953` pendingStripe；
    `:1960-1978` 升序 merge
  - `:2071-2095` profile 汇总；`:2143-2145` `sh_frac` 定义；`:2146-2161` ops 打印
  - `:317-318` `g_tl_prof_cand`/`g_tl_prof_overlap` 声明
- `lib/healpix_db/healpix_drizzle/spherical_overlap.cpp`
  - `:1001` planar_polygon_area_n；`:1085/:1282/:1325` 切平面真路径；`:1168` overlap_area_impl；
    `:1385-1419` TargetGeomCache::get_or_build（`:1391` O(cap) 扫描）；`:1527` query_candidate_pixels_fast
- `lib/healpix_db/healpix_drizzle/spherical_overlap.h`:303-330 TargetGeomCache；`:328` `lru_` 为 deque
- `lib/healpix_db/healpix_drizzle/astro_sphere_sink.cpp`:92-160 直写（variance）；`:277-296` write_hips_phase1
- `lib/healpix_db/healpix_drizzle/hp_drizzle_api.h`:84 `hp_drizzle_run_phase1_hips`；`:94-113` auto nside 语义

**文档**

- `docs/science/DRIZZLE.md`:40-53 公式；`:79-83` 不变量；`:104` FP64 累积；`:138-146` 语义
- `docs/algorithms/DRIZZLE_GEOMETRY.md`:206-216 冻结容差；`:231` DISP-DRZ-005（切平面真路径，禁删）
- `docs/contracts/DATA_SEMANTICS.md`:26-31 signal/support/invalid；`:35-50` variance/ivar；
  `:251` nside；`:262-263,278-280` 归一边界；`:324-327` 产品表；`:346-358` 恒等式

**报告 / 证据**

- P34：`REPORT.md` §1.1/§2.2/§2.3/§2.4/§2.5/§4/§7；`REPORT2_drizzle_segments.md` §0/§1/§2/§3/§4/§5/§6；
  `logs/A2_profile.txt`、`logs/A2_fullframe.txt`
- P35：`REPORT.md` §0/§3/§4/§5/§7/§9/§10；
  `logs/dz_micro_nrows20_v2.txt`、`dz_paths_fullT4.txt`、`dz_maxdw_fullT4.txt`、
  `k1_tile_diff_dist.json`、`p1_timing.json`、`p1_ab_summary.json`、`p1_thread_independence.json`；
  `patches/P35-algo-land.patch`；`src/bench_p35_paths.cpp`
- P22：`REPORT.md` §0/§1.1/§1.2/§2/§3/§4.1/§5
- P15a：`REPORT.md` §0/§2.1/§3.1/§3.2/§4/§5/§6
- P17：`REPORT.md` §0/§1.2/§2.1/§3.2/§4/§5

## 附录 B：本包独立复核命令（只读）

```bash
# 1) 代码级嵌套关系与 K1/K2 落地状态（本包亲验）
grep -n "profP_accum\|prof_geom_tl\|g_tl_prof_cand\|g_tl_prof_overlap" \
     lib/healpix_db/healpix_drizzle/drizzle_engine.cpp
grep -rn "p35_compute_fast_box\|CandidateBoxState\|plane_clip" lib/healpix_db/healpix_drizzle/   # 命中 0
grep -n "lru_" lib/healpix_db/healpix_drizzle/spherical_overlap.cpp                              # :1391 仍是 O(cap) 扫描

# 2) 三态消融独立复算（P35 自带比较器；只读产物目录）
cd run/perf-fix/P35-algo-land
W=work
python3 scripts/compare_p1.py $W/M42_T2_M1_Red_P1_BASE_c8-11   $W/M42_T2_M1_Red_P1_K2ONLY_c8-11
python3 scripts/compare_p1.py $W/M42_T2_M1_Red_P1_K2ONLY_c8-11 $W/M42_T2_M1_Red_P1_K2CACHE_c8-11
python3 scripts/compare_p1.py $W/M42_T2_M1_Red_P1_K2CACHE_c8-11 $W/M42_T2_M1_Red_P1_PATCHED_c8-11
# 期望：前两条 scientific_verdict=BITWISE_IDENTICAL；第三条 n_scientific_binary_diff=183, DIFF

# 3) p1_final.json 差异定性
A=$W/M42_T2_M1_Red_P1_PATCHED_c8/p1_final.json; B=$W/M42_T2_M1_Red_P1_PATCHED_c8-11/p1_final.json
diff <(python3 -m json.tool "$A") <(python3 -m json.tool "$B")   # 仅 hips_root/properties 输出路径

# 4) 全帧占比与并行效率复算
grep RESULT run/perf-fix/P34-algo-research/logs/A2_fullframe.txt
```

---

*本章所有数值均可回溯到上述锚点；“实测/推断/外推”标注贯彻全文；被推翻的错误结论（§2.4、§3.2.3、§3.3.7）与其纠正过程按负责任的推导要求完整保留。*

---

## 定稿补记（P35 最终交付，2026-09-15 15:40）

负责人已裁决 **K1 不做**，drizzle 冻结 = **K2 + TargetGeomCache O(1)**。最终交付两份**独立补丁**：

| 补丁 | sha256（前 16） | 文件 | 内容 |
|---|---|---|---|
| `P35-drizzle-K2-cache.patch` | `e6670ea601bfa24a` | 6 | K2 候选增量 + 缓存 O(1)；**K1 引用数 = 0** |
| `P35-phase3-memo.patch` | `a231231af6958077` | 5 | phase3 保核双线性记忆化（不在 phase1 冻结范围） |

**最终等价性与收益（③ = K2+缓存 与 baseline 直跑对照）**：
- **科学产物 BITWISE_IDENTICAL**（单核 c8 与四核 c8-11 各直跑：0 tile / 0 JSON / 0 properties / 0 only-in-one）；
- **绑核 1/4/8 核两两 BITWISE_IDENTICAL**；
- **端到端干净 A/B（同批交替 2 轮，taskset -c 8，load 3.7-6.5）**：wall **333.26 s → 219.94 s = 1.515×**；user CPU **327.14 → 214.81 s = 1.523×**；峰值 RSS 2012.6 → 2105.5 MB（+4.6%）；
- 对照：含 K1 时为 1.524× ⇒ **K1 对总收益贡献 ≈ 0**（其节点级收益仅 1.7-3.4%，且会改变产品）；
- 验收：当前 HEAD 干净树 + 两补丁 ⇒ 全量构建通过、**全量串行 ctest 329/329 通过 0 失败**（含新增 `p35_candidate_incremental` / `p35_bilinear_memo`）。

**本节订正的三条边界**：`sh_frac = cand/(cand+quick_rej)`（query 层口径，**不是**“到达 S-H 比例”，不得作门禁量）；缓存 O(1) 落地实测整节点 **≈0.15%**（P34 的 3-5% 为高估）；K2 **原型 3.14× / 落地 2.92×** 两个边界值。