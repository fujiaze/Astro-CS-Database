# Gaia XPSD 查询（ALG-GAIA-001）

> ID: ALG-GAIA-001  状态: CONTRACT_READY（CAT-GAIA-DOC 冻结，2026-09-05）
> 上游 SCI: SCI-AST-001（docs/science/ASTROMETRY.md，别名 SCI-WCS-001）
> 模块: lib/gaia_xpsd_client（module_id astrocs.catalog.gaia）
> 权威源码: lib/gaia_xpsd_client/src/gaia_client.c（本文件全部离散公式、常量、
> 行为边界均从该文件逐函数核对；与 lib/gaia_xpsd_client/README.md 旧版性能
> 摘要冲突时以本文与源码为准）。

## 1. 科学定义（连续层面，源自 SCI-AST-001）

给定查询锥（球心 `q=(RA_q, Dec_q)`，角半径 `ρ`，单位度，J2000/ICRS）与星等窗
`[m_lo, m_hi]`，返回 XPSD 本地星表（Gaia DR3 / DR3SP）中满足

```text
d(q, s) = acos( sin Dec_q · sin Dec_s + cos Dec_q · cos Dec_s · cos(RA_s − RA_q) ) ≤ ρ
m_lo ≤ m_G(s) ≤ m_hi        （闭区间，两边界都包含）
```

的全部恒星子集。坐标 frame、`RA∈[0,360)`、`Dec∈[-90,90]` 的契约与
`docs/science/ASTROMETRY.md` §3a 一致；本模块不做自行消化（proper motion）、
不做 SIP/像素投影（plate_solve 侧）、不做匹配求解。

## 2. 离散公式（源码逐条对应）

### 2.1 角距判定（gaia_client.c:1242-1249 / 1371-1377 / 1510-1516 / 2082-2088）

实现为标量三角展开（与 2.1 式恒等，避免大 RA 差的余弦精度损失）：

```text
d_ang = cos(Dec_q)·cos(Dec_s)·( cos(RA_q)·cos(RA_s) + sin(RA_q)·sin(RA_s) )
      + sin(Dec_q)·sin(Dec_s)
d_ang = clamp(d_ang, -1, +1)          # 浮点域保护
accept  ⟺  acos(d_ang) ≤ ρ·(π/180)
```

预计算 `cos/sin(RA_q), cos/sin(Dec_q), cos(ρ)`（入口一次）；每颗候选星计算
`cos/sin(RA_s), cos/sin(Dec_s)`。

### 2.2 星等解码（gaia_client.c:1222）

```text
m_G = raw_u16 × 0.001 − 1.5      # XPSD 记录偏移 +20，uint16
m_BP = raw_u16 × 0.001 − 1.5     # 偏移 +22（仅 DR3SP）
m_RP = raw_u16 × 0.001 − 1.5     # 偏移 +24（仅 DR3SP）
```

量化步长 0.001 mag；星等窗过滤在反投影之前（先过滤后投影，块内跳过）。

### 2.3 位置解码（gaia_client.c:1212-1240）

Equirectangular 树（赤道带 4 棵）：

```text
x = x0 + dx · (1/1.8e9)  deg     # dx uint32，1/1.8e9 deg = 7.2 µas/LSB
y = y0 + dy · (1/1.8e9)  deg
RA_s  = center_ra + x
Dec_s = y
RA_s += dra_raw · (1/3.6e8)      # dra_raw int16，偏移 +26，非 0 才加（10 µas/LSB）
RA_s  归一到 [0,360)
```

AzimuthalEquidistant 树（极冠 2 棵，投影中心=极点，`unproject` gaia_client.c:622-646）：

```text
xr = x·(π/180), yr = y·(π/180), r = hypot(xr, yr)
r < 1e-15  → RA_s = center_ra, Dec_s = center_dec   # 投影中心（极点）
否则:
  c = asin(cos r);  南冠 (center_dec < 0) 时 c = −c
  RA_s  = center_ra + atan2(xr·sin r / r, yr·sin r / r) · (180/π)
  Dec_s = c · (180/π)
  RA_s 归一到 [0,360)
```

### 2.4 赤道带 bbox 剪枝（bbox_intersects gaia_client.c:648-659）

节点 bbox `[ra_min,ra_max]×[dec_min,dec_max]`（由节点平面坐标 + center_ra 恢复）：

```text
若 Dec_q + ρ < dec_min 或 Dec_q − ρ > dec_max        → 拒绝
d_ra = RA_q 超出 [ra_min,ra_max] 的距离；d_ra > 180° 时取 360° − d_ra
|cos(Dec_q)| < 0.01                                   → 保守返回相交（经线收敛退化）
d_ra · cos(Dec_q) > 1.2 · ρ                           → 拒绝
```

`1.2` 为经验安全裕量（RA 环绕由 `360°−d_ra` 归一处理）；该分支的
false-negative=0 是工程保守性结论（V18R3 差分 18 查询 683,297 星一致），
非数学证明；数学证明仅覆盖极冠分支（2.5）。

### 2.5 极冠平面剪枝（polar_plane_intersects gaia_client.c:661-734）

AE 投影以极点为中心：`x = θ·sin φ, y = θ·cos φ`（θ=余纬，度；φ=RA−center_ra）。
投影平面距离 ≤ `C ×` 天球角距，`C` 双常数：

```text
C45 = (π/4)/sin(π/4) = π/(2√2) ≈ 1.11072   （cone 完全在 θ≤45° 子冠内）
C   = π/2 ≈ 1.5708                          （全局）
```

剪枝判据（拒绝仅发生在数学必然相离时，无假阴性）：

```text
θ_q = 查询中心到本树极点的余纬（dec 越界做防御归一）
radius < 0                    → 不剪枝（防御）
θ_q > 45° + ρ                 → 整树拒绝（cone 完全在极冠外）
45° < θ_q ≤ 45° + ρ           → 不剪枝（可能跨 ±45° 边界，保守）
θ_q + ρ > 90°                 → 不剪枝（双-Lipschitz 常数失效）
否则: 平面盘 B(q', C·ρ) 与节点矩形 [x0,x1]×[y0,y1] 不相交 → 拒绝
      （C 按 θ_q+ρ ≤ 45° 取 C45，否则取 C）
```

编译期定义 `GAIA_POLAR_PRUNE_DISABLED` 时恒为"相交"（differential reference
mode，仅测试用；生产编译不得定义）。三个搜索变体（star/spectrum/photometry）
使用同一 predicate（gaia_client.c:1183-1192 / 1312-1321 / 1452-1460）。

### 2.6 DR3SP 光谱量化解码（gaia_client.c:1378-1387）

PCL `GaiaDatabaseFile::EncodedStarSPData` 记录布局（stride 384 B = 32B 头 +
float32 fluxMin @32 + float32 fluxMul @36 + uint8 flux[343] @40 + 1B 填充）：

```text
F(λ_j) = byte_j × flux_mul + flux_min     单位 W·m⁻²·nm⁻¹
λ_j = spectrum_start + j × spectrum_step   nm,  j = 0..spectrum_count−1
spectrum_start/step/count 取自 XPSD XML <Data parameters="...">（缺省 0，
  gaia_client_get_spectrum_params 原样输出；count=0 时查询路径回退 343）
```

无光谱文件（DR3）：`flux_min = flux_mul = 0`，`out_spectra = NULL`。

### 2.7 近似与边界汇总

| 项 | 性质 | 依据 |
|---|---|---|
| 极冠剪枝 | 数学保守（C45/C 双 Lipschitz），false_negative=0 | 本节 2.5；V18R3 属性测试 80270 例 |
| 赤道带 bbox 1.2 裕量 | 经验保守（差分验证），非数学证明 | 2.4；memory.md 2026-08-15 |
| Equirectangular 反投影 | 线性恒等（XPSD 格式定义，非真球面投影） | 2.3；源码 1228-1230 |
| AE 反投影 r>90° 域 | 反射语义（asin(cos r)），由 2.5 不剪枝分支兜底 | 2.3/2.5 |
| 星等/位置量化 | 0.001 mag / 7.2 µas / 10 µas LSB | 2.2/2.3 |
| 结果上限 | 单文件 collector 200000 星静默截断（MAX_STARS_RESULT） | 源码 27, 1146, 1215 |
| 缓存 | 精确键 double 逐位 + dataset identity + version=2；命中=同查询精确重复 | 源码 112-134, 427-472, 75 |

### 2.8 复杂度

- 单查询：树剪枝后访问叶块 `k` 个，`O(Σ_t 树深 + k·B/stride)`，B=块字节
  （解压 O(B)，缓存命中 O(1)）；输出构造 O(N)。
- 文件级并行：`O((Σ_f 工作量)/W) + O(F)`（W=OpenMP workers，F≤32 文件），
  schedule(dynamic)。
- 缓存命中路径 O(N) 拷贝（持锁内完成，见 §4 并发）。
- 内存：mmap 全部 XPSD（只读）+ 块缓存 ≤4GB + 每线程 scratch
  `max(max_block_size, 65536)` + 查询缓存 ≤64 条。

### 2.9 误差来源（限制结论，不改 SCI 容差）

1. 星等量化 0.001 mag 与 DR3SP 光谱 8-bit 量化（残差 median 0.21%/p95 1.8%，
   memory.md 2026-08-08）；
2. 位置量化 7.2 µas + dra 修正 10 µas LSB；
3. `acos` 在 ρ→0 时相对误差放大（double 域 ~1e-16 rad 绝对）；
4. 赤道带 bbox 1.2 裕量属保守近似（多查不少查，不漏星）；
5. 输出行序依赖目录枚举顺序（跨平台不稳定，见 §3 输出契约）。

## 3. 数据与 API 契约

- DATA 合同：DATA-GAIA-001（docs/contracts/DATA_SEMANTICS.md §6）——输入
  XPSD 目录、输出星表行单位/dtype/shape/invalid。
- API 合同：API-GAIA-001（docs/contracts/PUBLIC_API.md §gaia-client）——
  12 个 GAIA_EXPORT 符号、返回码、所有权、线程安全。
- 唯一生产源符号清单与迁移映射：lib/gaia_xpsd_client/README.md §6、
  module.yaml `source_symbols`。

### 3.1 plan / execute / cancel / inspect（迁移合同，astrocs.catalog.gaia）

- `plan()`：由 file_count、db_type、Σmax_block_size、查询参数推导 work_units
  （≈叶块数×块大小）、memory（mmap 只读 + 块缓存上限 + scratch）、并行轴
  =文件（min 1 / max file_count workers）。当前实现未提供 plan 入口（迁移
  缺口，CAT-GAIA-IMPL 建立 adapter）。
- `execute()`：一次锥形查询（4 个搜索入口之一）；一个 execute 只完成本模块
  查询，不做匹配/积分。
- `cancel()`：当前 C API 无取消检查点（合同缺口）；迁移后取消检查点=文件
  循环边界，由 host ThreadLease/executor 传播。
- `inspect()`：`gaia_client_get_db_type / get_file_count / get_total_sources /
  get_spectrum_params`（真实存在，详见 API-GAIA-001）。

## 4. 并发模型（现状，与源码一致）

- 并行：`#pragma omp parallel for schedule(dynamic)` 按文件分片；每文件独立
  collector 与 scratch，块缓存仅被处理该文件的线程访问（单写者）。
- 互斥：`client->cache_lock`（Win32 CRITICAL_SECTION / POSIX mutex）包裹
  查询缓存 lookup/insert；命中路径在持锁状态下完成 O(N) 输出构造
  （gaia_client.c:1658-1684）——命中查询串行化。
- trace：per-query 上下文 + `#pragma omp atomic`，进程级零共享计数器写
  （ASTROCS_GAIA_TRACE=1 启用，stderr 输出）。
- 线程来源：OpenMP 默认 team（无 host ThreadLease）——迁移缺口，迁移后由
  host executor/lease 授予线程（冻结约束 D.3/D.4）。
- `destroy` 与在途查询并发 = UB（文档契约：调用方保证 join 后再 destroy）。

## 5. fast/reference/oracle 与测试设计（TEST-GAIA-DESIGN-001，冻结）

> 状态: DESIGN_FROZEN（本节为 CAT-GAIA-DOC 冻结的测试设计与容差合同）。
> 可执行测试（tests/{unit,properties,oracle,fixtures,negative,performance}）
> 由 CAT-GAIA-TEST 建立；本文不声明任何测试已 PASS。

- **合成 fixture**：固定 seed 生成微型 XPSD（≥2 文件 × 4 树，含 Equirect 与
  AzimuthalEquidistant、LZ4 与 zlib+shuffle 两种压缩、含/不含光谱记录、极区
  星、RA≈0/360 环绕星、mag 边界星）；生成器入 tests/fixtures（不提交大二进制）。
- **独立 oracle**：Python 参考实现——直接解包同一 fixture 数据集，暴力全枚举
  球面角距 + 独立星等过滤；不调用被测 C 符号、不复制剪枝逻辑；解压用标准
  zlib/lz4 库（隔离参考，非本文件 lz4_decompress/byte_unshuffle）。
- **不变量（properties）**：
  - I1 无假阴性：oracle 全枚举结果 ⊆ 实现结果（极区与赤道带均测；
    GAIA_POLAR_PRUNE_DISABLED differential 模式对照）；
  - I2 无假阳性：实现结果 ⊆ oracle 结果 ∪（量化边界容差内星）；
  - I3 缓存等价：同 client 同参数冷路径与缓存路径输出 bitwise 一致；
  - I4 星等窗闭区间：m=m_lo、m=m_hi 恒被包含；
  - I5 截断上限：单文件 >200000 候选时输出恰为 200000（fixture 缩小参数验证
    逻辑而非规模）。
- **negative**：NULL 句柄/输出指针 → -1；坏魔数/截断文件 → create 失败或 0 结
  果；分配故障注入（GAIA_ALLOC_TEST 包装 malloc/calloc/realloc/free，5 失败点）
  → 无泄漏、无半状态、稳定 -1；空目录语义差异（Win NULL vs POSIX 空 client，
  见 API-GAIA-001）按平台断言。
- **串并行**：1/N worker 加速比（fixture 足量叶块）；64 并发查询 +
  ASTROCS_GAIA_TRACE=1 无混合（V18R3 结论复验）。
- **ISA**：baseline（无 -march）与 -march=native 输出 bitwise 一致（本模块无
  手写 SIMD，默认 bitwise 合同）。
- **资源**：RSS 结束回落有解释高水位；块缓存 total_memory ≤ 4GB；trace 关闭
  时无共享热写。
- **冻结容差（供 CAT-GAIA-TEST 引用，不得事后改）**：
  - 迁移等价（直调 vs DLL）：bitwise（无 SIMD/归约顺序变化）；
  - oracle 数值比较：位置 |Δ| ≤ 5e-10 deg（double acos 往返界）、星等
    |Δ| ≤ 1e-9 mag（同一有理式 raw×0.001−1.5）、光谱字节恒等（整数域）；
  - 性能阈值不在本文冻结（由 CAT-GAIA-TEST 按资源证据设定并引用本节）。
- 每个 threshold 引用本文段落（5/2.5/2.6），测试元数据记录 commit/seed/hash。

## 6. ID 索引

- ALG-GAIA-001（本文件）；SCI 上游 SCI-AST-001；
- DATA-GAIA-001（docs/contracts/DATA_SEMANTICS.md §6）；
- API-GAIA-001（docs/contracts/PUBLIC_API.md）；
- TEST-GAIA-DESIGN-001（本文 §5，设计冻结；可执行 TEST-GAIA-* 待
  CAT-GAIA-TEST）。
