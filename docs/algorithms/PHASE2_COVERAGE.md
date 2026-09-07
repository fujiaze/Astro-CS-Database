# Phase2 Coverage Union Algorithms (ALG-COV-001)

> 状态: ACTIVE（P2-COV-DOC 冻结，2026-09-07，SA-P2-S20）
> 上游 SCI: SCI-UPM-001（docs/science/PHASE2_UPM.md，FROZEN T106 2026-08-23，
> 共享引用不改动）；三概念分离权威=SCI-INT-001（docs/science/INTEGRATION.md，
> FROZEN T108 2026-08-23，共享引用不改动）；处理链位置=SCI-SCOPE-001 §处理链
> 第 5 步（coverage union 为 Phase2 首节点）。本任务零 SCI 层改动（§11.5）。
> 下游: DATA-COV-001（DATA_SEMANTICS §19）、API-COV-001（PUBLIC_API）、
> MOD-astrocs-phase2-coverage（registry）
> 唯一权威生产源: lib/phase2/src/coverage.cpp（239 行实测）+ 唯一权威签名头
> lib/phase2/include/astro/phase2/coverage.h（59 行）；禁止手抄他版。
> 矩阵行: docs/traceability/TRACEABILITY_MATRIX.json
> MOD-astrocs-phase2-coverage（matrix P2-COV，legacy_paths=lib/phase2 coverage
> sources，迁移目标 astrocs_p2_coverage.dll，module_id=astrocs.p2.coverage）。

## 1 上游 SCI 与输入输出

- SCI-UPM-001（UPM 共享 SCI）：coverage union 天区 Ω 是 UPM 控制采样
  （"控制点布置于整个 coverage union"，lib/phase2/src/sampler.cpp:4）与联合
  加性模型的范围前提；Ω 与逐帧 tile 覆盖数为几何量，不进入任何科学权重。
- 三概念分离（本模块合同红线，登记为负向条款，见 §7）：`coverage` = 几何
  球面集合量（本模块唯一产物）；`support` = 逐像素覆盖支撑 [0,1]（样本级，
  SCI-INT-001 §2/§5，P2PixelStack.support，仅作 eligibility 与 canonical
  reducer `max`，integrator 语义）；`validity` = 数据有效性标志（finite/
  accepted/排异接受掩码语义，SCI-INT-001 §5 valid(i)）。三者禁止混用，
  本模块不生产 support/validity，也不消费之（coverage.cpp 无任何 support/
  accepted 输入，实测 :59-140 只读 properties 与 Moc.fits tile 列表）。
- 输入: N 个 Phase1 单帧 HiPS 目录路径 `const char* const*`（signal 子目录
  语义，`aio_hips_open(path, AIO_HIPS_RD_SIGNAL)`，:61；signal 子目录下
  properties + Moc.fits，AIO 侧 aio_hips_reader.cpp:205/:141）。
- 输出: `P2CoverageResult` POD（coverage.h:40-48）= 逐帧元信息
  P2HipsInputInfo[N]（coverage.h:31-38）+ union MOC 叶级 cell 数组
  P2MocCell[K]（coverage.h:26-29）+ target_order + status/error。
- 下游消费: sampler（p2_sample_controls 系，sampler.cpp:464-1150，消费
  n_inputs/union_cells/target_order）、UPM（哈希输入种子，upm.cpp:4070/
  :4073-4075）、编排 session（lib/phase2_session/p2_session.cpp:119-148
  coverage 阶段，两次调用协议 :125/:138，manifest 登记n_union_cells/
  target_order :146-147）、registry descriptor（module_adapters.cpp:561-576）。
- descriptor astrocs.phase2.coverage（module_adapters.cpp:561-576）为编排层
  词汇，端口 calibrated→coverage 坐标登记 PIXEL 与球面 MOC 实际语义不符，
  以本合同为准修订，P2-COV-INT 对齐，不得反向作为冻结依据。

## 2 离散公式

（锚=coverage.cpp 实测行号；公式与源码一一对应，禁止改写）

- properties 解析（parse_props :20-45）: 逐行 `key=value`（首个 `=` 分割
  :29-31），`#` 开头行跳过 :27，两端空白 trim（lambda :32-37）；无容错
  重复键（后值覆盖前值，`kv[k]=v` :39）。
- 每帧叶级 tile 收集（inspect_frame :59-140）: `aio_hips_open`（:61，失败
  → "aio_hips_open failed" + AIO last_error :63-65）；`aio_hips_get_properties`
  8192 B 缓冲（:67-72）；`hips_order=geti("hips_order",-1)`（:83，缺失→
  "missing hips_order" :88-92）；tile_width 必须为 512（:84, :93-97，
  "unsupported tile_width=%d"）；`hips_version` 缺失拒绝（:87, :98-102）；
  `hips_frame` ∈ {equatorial, icrs}（:85, :103-108，"unsupported
  hips_frame=%s"）；`obs_filter` 一致性校验在 build 层（§2 filter 公式）。
- 每帧最高叶 order: `max_leaf_order = hips_order`（P2HipsInputInfo 回填
  :141-143，语义=该 HiPS signal 子目录 properties 声明的叶级 order）。
- target_order（:194-196）:
  `target_order = min(f.max_leaf_order, f∈[0,N))`
  （冻结语义：禁止低 order 插值伪装分辨率，coverage.h:8/:51 注释；全部
  输入同 order 时即该 order）。
- filter/passband 一致性（:181-193）: 基准 = 第一个成功帧的
  `filter_passband`（:173-176 赋值）；后续帧
  `filter ≠ base_filter` → "filter mismatch: %s vs %s" rc=1（:181-185）；
  **空 filter 跳过一致性检查**（`if (!frame.filter_passband.empty())` :181
  与 `if (!base_filter.empty())` :190，空串静默放行，DISP-COV-003）。
- union MOC 父单元聚合（:204-214）:
  对每帧叶级 tile t（hips_order = f.max_leaf_order，:209-210）与目标
  order o=target_order，令 `s = f.max_leaf_order − o`（:207），
  `cell(t) = t >> (2·s)`（:209，NESTED 父索引；2·s 为 z-order 二维象限
  移位粒度）。
  `Ω_cellset = ⋃_{f∈[0,N)} { t>>(2·s_f) : t ∈ MOC_f(hips_order_f) }`
  （:204-211 逐帧 append）→ `std::sort`（:212-213）→ `std::unique`
  原地去重（:213-214）→ K=|Ω_cellset|（:218）。
  输出 cell 的 order 字段统一=target_order（:221），ipix=去重后父索引
  （:222）。
  性质: Ω 允许不连通分量（coverage.h:9）；任意两输入重叠区当且仅当
  父单元索引重合时合并——`constant overlap`（逐 cell 覆盖帧数恒定域）
  由去重后的并集 + 逐帧 tile 集合在下游（sampler/UPM）按需计算，本模块
  不输出 depth/重叠计数（§10 负向条款）。
- 越界守卫: 首个 union cell ipix 必须 < `12·4^order`（NESTED 象限总数；
  sampler 侧断言 sampler.cpp:659-662，本模块自身不重复校验）。

## 3 伪代码

（coverage.cpp p2_coverage_build :144-231 结构直译；两次调用协议，见 §4）

```text
p2_coverage_build(hips_paths, n_inputs, out):
  if out == NULL: return 1                                    # :147
  保存调用方 union_cells/inputs 指针（容量查询两阶段）           # :149-153
  if hips_paths == NULL or n_inputs == 0:
      error="no inputs"; return 1                             # :154-157（status 未置位，DISP-COV-001）
  memset out；恢复指针                                          # :158-163
  for f in 0..n_inputs-1:                                      # :164
      if path NULL/空: error, rc=1                             # :165-170
      rc=inspect_frame(path, &info_f, &tiles_f)                # :171
      if rc: error 组帧序号, rc=1                              # :172-179
      if f==0: base_filter=info_f.filter                       # :173-176
      elif filter 非空且 != base_filter: error, rc=1           # :181-193
      target_order = min(target_order, info_f.max_leaf_order)  # :194-196
  if 无有效帧: error="no valid inputs", rc=1                   # :198-202
  for f: for t in tiles_f: append t>>(2·(order_f−target_order))# :204-211
  sort + unique（保序去重，升序）                                # :212-214
  n_union_cells=K; union_cells 非空时逐 cell 回填(order,ipix)   # :218-224
  inputs 非空时回填 infos；status=0; return 0                  # :225-230
```

## 4 两次调用协议（容量查询）与内存所有权

- 第一次调用: `out->union_cells=NULL`（capacity query）→ 返回
  `n_union_cells=K` 且不写 cell 数组（:219-224 条件回填）；调用方分配
  `K` 个 P2MocCell 后第二次调用获得数据（头注释 coverage.h:50-52；实测
  每次调用完整重新扫描全部输入，无缓存，inputs 指针同理两阶段回填
  :225-228）。
- P2CoverageResult/P2MocCell/P2HipsInputInfo 全部为调用方分配（coverage.h
  :42/:44 注释）；`p2_coverage_free`（:233-237）仅 `memset(out,0)`
  清零 POD——不释放任何堆内存，无所有权转移（与 PHASE2_API_V1 §1 所有权
  图行 `Coverage: build/调用方持有/p2_coverage_free/只读借用` 一致，
  docs/api/PHASE2_API_V1.md:15）。
- 重复调用幂等: 同输入两次 build 结果 bitwise 一致（纯函数式扫描，无
  全局状态；错误路径通过 `aio_hips_reader_last_error()` 转述 AIO 层原因
  :63-65）。

## 5 边界/NaN/Inf/invalid

- `hips_paths=NULL ∨ n_inputs=0` → rc=1 "no inputs"（:154-157；
  status 字段未置 1，DISP-COV-001）。
- 路径 NULL/空 → rc=1 "input %llu path NULL or empty"（:165-170）。
- AIO 打开/properties 失败 → rc=1，error 含 AIO 层 last_error（:62-72）。
- `hips_order` 缺失/负 → rc=1（:88-92）；`hips_tile_width≠512` → rc=1
  （:93-97）；`hips_version` 缺失 → rc=1（:98-102）；`hips_frame∉
  {equatorial,icrs}` → rc=1（:103-108）。
- filter mismatch（双方非空时）→ rc=1（:181-193）；单边空 filter 静默
  放行（DISP-COV-003）。
- 输出 MOC cell 升序且唯一（sort+unique :212-214）；K=0 仅当全部输入
  MOC 为空（AIO 层 tiles 空数组），无显式错误（如实登记）。
- 单位/坐标: path 字符串；order/ipix 无量纲整数；NESTED equatorial/ICRS
  球面索引（frame 校验 :103-108 保证），无角度量。

## 6 复杂度与误差来源

- 时间: O(Σ|MOC_f|) 每帧读盘 + O(K log K) 排序去重（:212-213）；两次
  调用协议下整链 ×2（实测语义，无缓存）；内存 O(max|MOC_f|+K)
  uint64。磁盘 I/O 为唯一外部依赖（properties + Moc.fits，8192 B
  properties 缓冲 :67）。
- 误差来源: 无浮点运算——全链路整数集合运算，bitwise 确定；
  target_order 截断（混合 order 输入时低 order 决定全局分辨率）为设计
  保守选择（禁伪装分辨率，coverage.h:8），不构成数值误差。
- 确定性: 固定归约序（输入数组序→逐帧 append→sort），输出与线程数无关
  （单线程实现，无 OpenMP/OpenMP pragma，coverage.cpp 全文实测 0 处）；
  determinism=fixed_reduction_order（module.yaml 合同值）。

## 7 合同负向条款（科学红线，P2-COV 专项）

- **no use as implicit scientific weight**（matrix P2-COV 专项）: 本模块
  输出（union MOC/target_order/逐帧 tile 计数）是几何登记量，**禁止**
  被任何下游作为科学权重、统计权重或置信度使用；科学权重唯一冻结公式
  `w_UPM = quality_factor × geometric_reliability × control_ivar`
  （docs/science/PHASE2_UPM.md §5 F2），其中 support 仅
  eligibility/coverage 语义（PHASE2_UPM.md §5 注释行
  "禁 production 乘 star SNR / snr²/(1+snr²) / support^p；support 仅
  eligibility/coverage"）、canonical reducer=max（SCI-INT-001 §5，
  "覆盖并集保守下界"）——coverage 帧数/N_cover 不得进入该式或替代
  control_ivar。
- **coverage/support/validity 三概念分离**（matrix P2-COV 专项）: 本模块
  只生产 coverage（几何集合）；support 样本级 [0,1]（SCI-INT-001 §2）
  与 validity（有效/接受标志，SCI-INT-001 §5）不在本模块合同域；任何
  把 union cell 数、n_tiles、覆盖帧数当作 support 数值或 validity
  判定的消费均违反本合同。
- **不做**: 不重新校准/PlateSolve/PSF/DR3SP/Drizzle（coverage.h:6）；
  不读 signal/support/snr 像素数据（只读 properties/Moc.fits）；不做
  帧间交集/差集运算（只 union，§2）；不输出 depth/overlap 计数产品
  （§10）；不输出 HiPS（写盘归 P2-HIPS）；不跨滤镜统一（filter mismatch
  显式拒绝，:181-193；UPM 侧"不跨滤镜统一（filter 分组由调用方保证）"
  SCI-UPM-001 §1 同构）。
- **registry 端口语义修订**: descriptor 端口 coverage 坐标
  CoordinateFrame::PIXEL（module_adapters.cpp:570）与 NESTED 球面 MOC
  实际不符，以本合同（HEALPix NESTED / equatorial/ICRS）为准，
  P2-COV-INT 对齐修正，不改生产码。

## 8 参考实现/Oracle

- Oracle 设计（独立于被测符号，不复制 §2 公式）:
  - Python/astropy 参考: 对合成 HiPS 树（固定 seed 生成 properties +
    Moc.fits）用 astropy_healpix/astropy.io.fits 独立读 MOC，按
    NESTED 父移位 `t >> 2s` 重算 union 集合与 target_order，与
    p2_coverage_build 输出集合严格相等（整数精确，无浮点容差）。
  - 解析解: 两个同 order 相邻 tile → K=2；同一 tile 双帧 → K=1
    （union 幂等）；混合 order 7/8 单 tile → K=1 且 target_order=7。
  - 负例: 空输入/NULL 路径/filter mismatch/tile_width 512 以外/
    frame 非法 → rc=1 且 error 载因。
- 既有可执行测试（legacy gate，迁移基线）:
  lib/phase2/tests/synthetic_gate.cpp `Phase2Coverage.RealHipsUnion`
  （:3374-3408，真实 HiPS 三帧 union/target_order=7/两阶段协议/
  cells[0].order=7）与 `Phase2Coverage.FilterMismatchRejected`
  （:3410-3419，坏路径 rc≠0）；两测试依赖 Fatduck 本地路径 F:/...
  （:3376/:3413）环境缺失时 GTEST_SKIP（:3378/:3415）——P2-COV-TEST
  必须建立不依赖本机真实数据的合成 fixture（TEST-COV-DESIGN-001 F1）。

## 9 容差来源

- 集合运算整数精确: union/target_order/ipix 无浮点，容差=0（bitwise
  相等），无经验容差；任何"近似 union"实现都违反 §2。
- 数值域容差仅存在于 AIO 读取层（tile 计数完整性由 Moc.fits 决定），
  本模块对 AIO 层的信任边界 = `aio_hips_open` rc 与
  `aio_hips_reader_last_error` 透传（:62-65）。
- 上述容差在 P2-COV-TEST 落地时逐项写死（TEST-COV-DESIGN-001 §11.4），
  不得放宽；fixture 生成器须注记容差来源（本节）。

## 10 关联 ARC/API/TST

- API-COV-001（docs/contracts/PUBLIC_API.md）: p2_coverage_build/free
  2 导出 + 两阶段协议 + P2CoverageResult 所有权。
- DATA-COV-001（docs/contracts/DATA_SEMANTICS.md §19）: 输入 HiPS 树
  与输出 MOC/逐帧元信息的单位/dtype/shape/invalid 唯一权威。
- API-P2-001（docs/api/PHASE2_API_V1.md，FROZEN V5 API-004）: 逐函数
  并发五字段（p2_coverage_build/free: yes/no(独立对象)/none/无/TST-COV-*）
  + 所有权图（docs/api/PHASE2_API_V1.md:28/:15）——编排级合同，与本节
  并行不互斥。
- ARC-001（docs/architecture/CPU_ADAPTIVE_V1.md）: cpu_heavy 资源类、
  单线程（internal_parallel=none）与 host_executor_lease 合同值依据。
- TST: TEST-COV-DESIGN-001（§11.4，P2-COV-TEST 落 TEST-P2-COV-001）。

## 11 P2-COV-DOC 冻结附录（2026-09-07，SRC-COV-001 源码实测）

### 11.1 ALG-COV-001 逐符号锚

| 符号 | 锚（coverage.cpp） | 角色 |
|---|---|---|
| p2_coverage_build | :144-231 | 唯一生产入口（C ABI，coverage.h:52-54 声明） |
| p2_coverage_free | :233-237 | POD 清零释放语义 |
| inspect_frame | :59-140 | 匿名 namespace 内部链接（:55），每帧校验+tile 收集 |
| parse_props | :20-45 | 匿名 namespace 内部链接，properties KV 解析 |

结构事实: `extern "C"` 块内 `#include "aio_hips_reader.h"`（:47-51，
AIO 头含 C 链接声明，功能等价但属维护歧义，并入 DISP-COV-005 整改域）；
头文件 aio_hips_reader.h 落位 lib/astro_image_io/include/（根 CMake
astrocs_phase2 include 目录 CMakeLists.txt:346-352 第 4 项，实测）。

P2HipsInputInfo 7 字段（coverage.h:31-38）生产消费面: hips_path
（:127 回填）、frame_id（:113-118，基名截断，见 DISP-COV-002）、
max_leaf_order（:141-143）、n_tiles（:139）、filter_passband（:110/:174
回填）、frame_type（:119-121 回填 hips_frame）；frame_id 64 B /
filter_passband 64 B / frame_type 32 B 截断上限（coverage.h:33-37 +
snprintf 截断语义）。

P2CoverageResult 7 字段（coverage.h:40-48）: n_inputs/inputs/
n_union_cells/union_cells/target_order/status/error（512 B，:47）。
status 语义: 0=ok（:229）；错误路径部分分支置 1（:172-179/:198-202），
"no inputs" 分支未置（DISP-COV-001）。

### 11.2 状态码/返回码语义

- rc: 0=成功（含 K=0 空 union）；1=失败（error[512] 载因，:47）。
- 失败路径 status: 1（:172-179/:198-202 显式）或 0（"no inputs" 分支
  :154-157 未置，DISP-COV-001；memset :158 在该分支后执行，错误信息
  不被清除——error 字段仍有效，status 与 rc 不一致的仅此分支）。
- 并发合同（API-P2-001 §2 行 1）: reentrant=yes / threadsafe=no
  （独立对象）/ internal_parallel=none / 取消点=无 / TST-COV-*；
  实测支撑: 无全局可变状态、无锁、单线程（§6）。

### 11.3 现状缺陷清单（DISP-COV-001..005，登记不改码，整改归 P2-COV-IMPL/INT）

- DISP-COV-001 `no inputs` 分支 status 不一致: `hips_paths==NULL ∨
  n_inputs==0` 时仅 strncpy error 后 return 1（:154-157），`out->status`
  保持 memset 后的 0（:158 在其后才执行）——rc=1 与 status=0 并存，
  违反 status/return 同步惯例（对照 :172-179/:198-202 均置 1）；
  调用方若只看 status 会误判成功。整改: 统一 status=1（P2-COV-IMPL）。
- DISP-COV-002 frame_id 取基名截断: `std::string(path).substr(path.find
  _last_of("/\\")+1)`（:113-118）以 `/` 或 `\` 基名为 frame_id，跨平台
  分隔符混用时截断点漂移；64 B 上限截断（snprintf，coverage.h:33）后
  唯一性可能退化（两长同名基名碰撞）——UPM frame 绑定/持久化引用该
  id（lib/phase2/memory.md「W4 UPM 完整化：真实内容哈希」），碰撞风险
  如实登记；整改: 内容哈希派生 id（P2-COV-IMPL，与 UPM SHA-256 设施
  对齐）。
- DISP-COV-003 空 filter 静默放行: filter 一致性检查双方非空才比较
  （:181/:190），`obs_filter` 缺失（空串）的输入绕过 filter 组校验，
  可能混入异 passband 帧（违背「同一 filter/passband」兼容前提
  coverage.h:7）；负例语义缺口（AIO 写侧恒写 obs_filter 时不可达，
  但合同须防外部 HiPS）。整改: 空 filter 显式拒绝或显式通配标记
  （P2-COV-IMPL，归 TEST 负例 F5）。
- DISP-COV-004 overlap/intersection/missing-tiles 产品缺失（合同范围
  缺口，非公式错误）: matrix P2-COV 专项要求 union/intersection/
  missing tiles/constant overlap 四语义；现状仅 union（:204-214）+
  target_order，无逐 cell depth map（每 cell 覆盖帧数）、无交集
  Ω_1∩…∩Ω_N、无缺失 tile 列表输出——下游 sampler/UPM 现以逐帧
  tile 集合自行推导，且 UPM 侧 geometric_reliability 权重因子
  （SCI-UPM-001 §2）与覆盖度关联的乘数实现恒 1.0（乘数未生效，
  R3-A 已登记，PUBLIC_API.md API-UPM-001 DISP-UPM-003）——本模块
  不以 depth 产品补齐该缺口（不改生产码），语义澄清: 覆盖度几何
  （本模块）≠ 权重因子（UPM 层），修正归 P2-UPM-IMPL 域；
  P2-COV-IMPL 是否增补 depth/intersection 只读辅助导出由其 TASK_RESULT
  登记（属合同扩展，非本冻结层）。
- DISP-COV-005 extern "C" 内 include + 两阶段全量重扫: AIO 头包含于
  extern "C" 块（:47-51，AIO 头自带 C 链接声明，双保险属维护歧义）；
  两阶段协议每次调用全量重扫全部输入（§4，实测无缓存），大 N 输入
  ×2 I/O 开销——记录为性能/卫生整改项（P2-COV-IMPL），非科学错误。
- 线程数未接 ThreadBudget: 单线程实现天然满足 determinism，但
  threading_model=host_executor_lease（module.yaml 合同值）的
  ThreadLease/取消检查点无接线（P2-COV-IMPL 整改点，同 DISP-WCS-005
  先例）；阶段级取消点由编排 session 提供（p2_session.cpp:120 阶段
  边界检查），模块内无取消检查点（API-P2-001 §2 行 1 取消点=无，
  一致）。

### 11.4 TEST-COV-DESIGN-001 冻结测试设计（可执行 TEST-P2-COV-001 由 P2-COV-TEST 落地）

- F1 合成 HiPS union oracle: 固定 seed 生成 2-8 帧 properties+Moc.fits
  （order 3-8 混合、含不连通分量/跨象限 tile），astropy 独立重算 union
  集合+target_order 与 p2_coverage_build 输出集合严格相等（bitwise，
  §9）；专项=union/missing tiles（缺 tile 的 cell 在帧 tile 集合
  补集中显式断言）。
- F2 两阶段协议: union_cells=NULL 容量查询 → 分配 → 二次调用数据
  一致；K 与 sort+unique 后集合大小严格相等；重复两次 build 输出
  bitwise 相等（幂等，§4）。
- F3 兼容校验负例: hips_order 缺失/tile_width≠512/hips_version 缺失/
  hips_frame 非法/filter mismatch（双方非空）→ rc=1 且 error 载因
  （§5）；F5 扩展: 空 filter 静默放行现状断言为 DISP-COV-003 行为
  锚（整改后本断言翻转为拒绝——P2-COV-IMPL 同步更新）。
- F4 边界: 单输入（N=1，union=自身父聚合）；K=0 空 MOC（rc=0 如实）；
  越界断言 max(ipix) < 12·4^target_order（NESTED 象限总数）。
- F5 负例/错误通道: NULL out/NULL paths/n_inputs=0 → rc=1；rc 与
  status 一致性断言（DISP-COV-001 整改门：整改后 status 必须同步=1）。
- F6 确定性/资源: 同输入两次调用 bitwise 一致；单线程断言（无
  parallel axis）；I/O 次数 = 2×帧数 ×（properties+MOC）上界登记
  （DISP-COV-005 整改基线）。容差冻结: F1-F6 全部整数/bitwise 断言，
  无数值容差；fixture 生成器注记容差来源（§9）。

### 11.5 SCI 层状态声明（本任务零 SCI 改动）

- 覆盖度几何语义已有 FROZEN 权威：SCI-UPM-001（docs/science/
  PHASE2_UPM.md，T106 2026-08-23 冻结）§1 目的句「在多帧覆盖并集上」、
  SCI-INT-001（docs/science/INTEGRATION.md，T108 2026-08-23 冻结）§5
  support「覆盖并集保守下界」、SCI-SCOPE-001 §处理链第 5 步
  「coverage union → 控制采样 → …」。三者均**不因本任务改动**（共享
  SCI 引用不改动；P1-WCS-DOC SCI-WCS-001=共享 ASTROMETRY.md 先例）。
- 因此本任务不新建 docs/science/ 冻结层文档：matrix P2-COV 行
  science_id=SCI-P2-COV-001 的语义映射由本节声明——
  SCI-P2-COV-001 ⇒ 指向既有 FROZEN 共享 SCI（权威=PHASE2_UPM.md §1
  覆盖并集 + INTEGRATION.md §5 support/validity 分离 + SCIENCE_SCOPE.md
  §处理链），矩阵 science_doc=docs/science/PHASE2_UPM.md
  （MOD-astrocs-phase2-coverage 行，2026-09-07 P2-COV-DOC 冻结）。
  三概念分离/union 离散公式/负向条款的算法定义权威=ALG-COV-001
  （本文档 §2/§7），SCI 公式语义不在此重复定义，两处冲突时以
  docs/science/ 为准并回改本文档（禁止反向）。
- 本节禁止被编排层词汇反向改写（descriptor astrocs.phase2.coverage
  由 P2-COV-INT 对齐，不作冻结依据）。
