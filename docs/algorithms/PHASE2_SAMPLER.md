# Phase2 Sampler Algorithms（P2-SAMP / astrocs.p2.sampling）

> ID: ALG-P2-SMP-001  状态: CONTRACT_READY（P2-SAMP-DOC 冻结，2026-09-09，
> owner SA-P2-S20）。本文件是 Phase2 控制点采样模块（control sampler）
> 算法层**唯一权威**：逐公式源码行号锚定 + 冻结容差 + 实现偏差登记。
> 上游 SCI: SCI-UPM-001（docs/science/PHASE2_UPM.md，FROZEN T106
> 2026-08-23，共享引用不改动；页头明示"模块: phase2 (upm/sampler)"；
> descriptor 占位 SCI-P2-SMP-001⇒SCI-UPM-001 映射声明见 §11.4）。
> 辅助 SCI: SCI-UPM-WEIGHT-001（control_variance 公式权威）、
> SCI-NOISE-001（robust 统计量的科学定义上游）、SCI-SCOPE-001
> §处理链第 5 步"控制采样"（链位置）。
> 关联 ALG: ALG-UPM-CONTROL-IVAR-001（本文件 §5.4 冻结承接，见 §12）；
> ALG-UPM-001（UPM 拟合，下游消费方）。
> 模块: lib/phase2/src/sampler.cpp（1156 行）+ 唯一权威签名头
> lib/phase2/include/astro/phase2/sampler.h（136 行，实测 2026-09-09）；
> DATA: DATA-P2-SMP（DATA_SEMANTICS §23）；API: API-P2-SMP-001
> （PUBLIC_API.md）；MOD: astrocs.p2.sampling（合同三件套
> lib/phase2_samp/，迁移目标 astrocs_p2_sampling.dll 为矩阵合同值
> 尚未存在，由 P2-SAMP-IMPL 建立，禁止声明 IMPLEMENTED）。

## 1 目的与非目标

**目的**（SCI-UPM-001 §1/§3 承接）：在 coverage union 天区 Ω 内按
几何规则布置稀疏球面光度控制点（与 SNR 几何解耦），并从实际 Phase1
HiPS 数据读取每个控制点的 background-clean patch 观测
y_ik/σ_ik/snr_ik/support_ik/quality_ik，产出 UPM 联合加性校准的
唯一输入（P2ControlObservation 流 + 全几何节点 + 统计账）。

**非目标（本模块不做）**：

- 不做 UPM 拟合/权重归一/表面求解（ALG-UPM-001，P2-UPM 域）；
- 不做星点检测（SNR 来自 Phase1 SNR Catalogue，禁止重新检测，
  sampler.h:11-12 语义冻结）；
- 不做 coverage union 计算（上游 DATA-P2-COV，P2-COV 域）；
- 不产出 per-pixel/per-leaf 科学场产品（稀疏控制点观测集才是输出）；
- 不解释 ivar 产品的科学权重语义（obs.ivar 仅诊断，弃用不进科学
  权重，upm.h:38-41 冻结）。

## 2 符号与单位（权威=本表 + DATA_SEMANTICS §23）

| 符号 | 含义 | 单位/域 | 实现锚 |
|---|---|---|---|
| Ω | coverage union（上游 MOC） | — | coverage.h（P2-COV 域） |
| tile_ipix | union tile 的 NESTED 像素号（order=target_order） | 无量纲 | coverage.h P2CoverageCell.ipix |
| grid=G | 每 tile 的 cell 网格边长（默认 8） | 无量纲 | sampler.cpp:296/:499 |
| cell_side | tile 边长/G=64（kTileWidth=512） | leaf 像素 | sampler.cpp:75/:595 |
| cell (t,gx,gy) | 控制点拓扑 = tile × 网格坐标 | — | sampler.cpp:737-746 |
| leaf_ipix | cell 中心 leaf 像素（order+9） | 无量纲 | sampler.cpp:739-740 |
| y_ik | 控制观测（patch 位置估计） | ADU（UPM-calibrated） | sampler.cpp:828 |
| σ_bg | robust scale（MAD×1.4826） | ADU | sampler.cpp:806-829 |
| k_corr | Drizzle 协方差方差放大因子 | 无量纲 | sampler.cpp:82/:547-555 |
| N_retained | clipping 后保留样本数 | 无量纲 | sampler.cpp:833 |
| control_variance | 控制点估计统计方差 | ADU² | sampler.cpp:840-842 |
| control_ivar | 1/control_variance | 1/ADU² | sampler.cpp:842 |
| snr_available | 局部星点存在性标志 | 0/1 | sampler.cpp:859/:1038 |
| reason | 内部拒绝原因 0..5 | 无量纲 | sampler.cpp:608/:759/:995-1005 |

禁止单位漂移：value=ADU、uncertainty=ADU、control_variance=ADU²、
control_ivar=1/ADU²、ra_deg/dec_deg=度（J2000）、snr=无量纲；
连续数学定义见 §5，dtype/shape 唯一权威=DATA_SEMANTICS §23。

## 3 逐符号锚（sampler.cpp 1156 行 / sampler.h 136 行，2026-09-09 实测）

**导出符号（sampler.h 声明 / sampler.cpp 实现）**：

| 符号 | 声明（sampler.h） | 实现（sampler.cpp） | 语义 |
|---|---|---|---|
| p2_sampler_default_config | :60 | :294-312 | 默认配置单一来源（15 字段） |
| p2_frame_id | :93 | :314-438 | truncated-64 canonical SHA-256 帧身份 |
| p2_stats_median | :97 | :440-447 | 共享 median（NaN 过滤） |
| p2_stats_mad | :98-99 | :449-461 | 共享 MAD×1.4826（out_median 回传） |
| p2_sample_controls | :103-114 | :1121-1136 | 采样入口（frame_id 内部计算） |
| p2_sample_controls_cached | :120-132 | :1138-1154 | 采样入口（外部透传 frame_id 缓存） |

**内部符号（匿名/静态）**：

| 符号 | 锚（sampler.cpp） | 语义 |
|---|---|---|
| kTileWidth/kTileShift | :75-76 | 512×512 tile，shift=log2(512)=9 |
| kSnrCatalogMax | :77 | SNR catalogue 上限 65536 |
| kControlCorrDefault | :82 | k_corr 冻结保守默认 1.4 |
| kPiHalf | :83 | π/2 常数（UPMW-004 中位数方差） |
| kcorr_lookup | :88-109 | pixfrac×scale 双线性标定表 |
| frame_drizzle_provenance | :112-137 | 帧 properties 解析 pixfrac/scale |
| read_tile_pair | :163-180 | signal+support tile 成对读（g_aio_mu :161 串行化） |
| median_of | :191-204 | nth_element median（偶数取 [begin,mid) 最大值均值；P0-01 修复） |
| SnrIndex::build/query/any_above | :206-288 | dec 排序索引 + RA 保守窗口 + 精确角距 |
| p2_sample_controls_impl | :463-1119 | 三阶段采样管线本体 |
| CellStat | :599-612 | 每 (cell,frame) 候选统计载体 |

**配置面（P2SamplerConfig，sampler.h:32-57，声明注释 ：31）**：15 字段默认值见
§3 表 p2_sampler_default_config 实现行；`control_grid_per_tile=8`/
`patch_radius_leaf=2`/`min_samples=5`/`snr_search_radius_deg=0.05`/
`background_patch_radius=8`/`background_clip_sigma=3.0`/
`background_clip_iters=3`/`background_max_contamination=0.20`/
`background_contamination_sigma=3.0`/
`background_min_retained_fraction=0.60`/`background_tolerance=3.0`/
`background_neighbor_radius=2`/`background_catalog_veto=1`/
`control_k_corr=1.4`/`cpu_workers=1`（sampler.cpp:296-310）。
显式 cfg 覆盖路径：sampler.cpp:484（`if (cfg_in) cfg = *cfg_in;`）。

## 4 算法结构：三阶段 background-clean 采样管线

sampler.cpp:584-591 冻结注释将管线映射为
BACKGROUND_SAMPLER_SPEC.md Stage A-E；实现按三遍组织：

| 阶段 | 遍 | 锚（sampler.cpp） | 语义 |
|---|---|---|---|
| 预备 | 0 | :485-502 配置修补；:512-523 frame_id 0 拒绝；:529-546 帧打开；:547-555 per-frame k_corr；:562-578 SNR catalogue 读入；:579-581 ivar 产品（可缺） | 输入装配 |
| Stage A+B（第一遍） | 1 | :699-934 pass1_cell（lambda :699-878）+ worker 池/串行（:886-934）：:702-714 越界 tile 占位；:715-717 覆盖帧收集；:719-733 tile pair 读；:735-791 每 cell patch 收集/过滤；:793-800 min_samples 拒绝；:802-833 亮端迭代 clipping（Stage B）；:838-844 cvar 组装；:847-867 SNR 邻域+catalog veto（Stage E） | 候选统计 |
| Stage C+D（第二遍） | 2 | :952-1009 同 tile 邻域 tolerance gate（C）+contamination/retained gate（D） | 局部门控 |
| 输出（第三遍） | 3 | :1011-1063 ≥2 clean 帧 control 输出 + obs 组装 | 观测流 |
| 收尾 | — | :1071-1088 容量上限+stats 补偿；:1089-1097 异常兜底；:1098-1119 输出拷贝 | 账目 |

## 5 逐公式定义（算法级，与 SCI-UPM-001 §5/§11 同构；单位见 §2）

### 5.1 坐标/tile 映射（matrix 专项 1）

控制点几何**只由 union 几何与目标角间距决定**（sampler.h:6 语义
冻结；实现 :729-736）：

```text
cell_side   = kTileWidth / G                                  # :595, =64
cx          = gx * cell_side + cell_side/2                    # :737
cy          = gy * cell_side + cell_side/2                    # :738
center_local= xy_to_nested_local(cx, cy, kTileShift=9)        # :739
center_leaf = leaf_of_tile(tile_ipix, 9) + center_local       # :740
(ra,dec)    = pix2ang_nest(1<<(target_order+9), center_leaf)  # :742
cell 索引   = c*G² + gy*G + gx                                # :708-709/:870-871
```

- control_id = cells 索引（uint64，稠密 0..n_union×G²-1，含空覆盖
  占位；:947 与 :1031/:1105 一致）；
- out_n_controls = n_union×G²（几何节点总数，与
  stats.accepted_controls/overlap_controls 区分；sampler.h:118-119
  注释冻结）；
- P2ControlNode 填充 :1105-1111（control_id=索引、tile_ipix、gx/gy、
  ra/dec、leaf_ipix）。

### 5.2 patch 收集与 signal/variance/weight 对齐（matrix 专项 2）

每 (cell,frame) 候选 patch = cell 中心 ±`background_patch_radius`
（默认 8 → 17×17；:596 r=cfg.background_patch_radius）：

- 逐 leaf 扫描 :775-790：`fi_idx = nested_local_to_fits_index(z, 9,
  512)`，**signal/support 同一 fi_idx 对齐读取**（:783-784）——
  signal 值与其支持度按同一 leaf 索引成对消费，禁止错位；
- 无效值过滤 :785-786：非 finite signal 剔除；非 finite support 或
  support≤0 剔除（support 域 (0,1] 上游 DATA-COV-001 §19）；
- 累计 ：787-789：vals.push(s)、sup_sum+=sp、n_valid++；
- support 输出 = patch 均值 `sup_sum/n_valid`（:844）；
- 边界处理：patch 越 tile 边界像素丢弃（:779-780 x/y 范围门），
  patch 不跨 tile 读取（每 cell 只消费本 tile pair，§9）。

### 5.3 Stage B：亮端迭代 sigma-clipping（:802-815）

对 patch 值集 vals（保留负值，仅剔非 finite）：

```text
m0 = median_of(vals)                          # :802
s0 = 1.4826 × median(|v−m0|)                  # :806-809
迭代 it = 1..background_clip_iters (默认 3):   # :812
  nr = { v ∈ ret : v ≤ m0 + clip_sigma·s0 }   # :815 单侧亮端
  若 |nr| < min_samples: break                 # :816
  nm = median_of(nr)                           # :817
  若 |nm−m0| < 1e-12 × max(|m0|, 1e-12): ret=nr; break   # :818 相对收敛
  m0 = nm; ret = nr
  s0 = 1.4826 × median(|v−m0|)（ret 上重算）    # :821-826
  若 s1 ≤ 0: break                             # :825
y_ik = m0（收敛集位置估计）                     # :828
σ_bg = s0 > 0 ? s0 : 1e-12                    # :829 零尺度 floor
```

细节锚：迭代序号按实际行为（先收缩后重算尺度）；`n_total` 为
过滤后 patch 原始样本数（:792）；`n_retained = |ret|`（:833）。

### 5.4 control estimator 方差（ALG-UPM-CONTROL-IVAR-001 冻结承接）

```text
control_variance = k_corr × (π/2) × σ_bg² / N_retained    # :840-842
control_ivar     = 1 / control_variance（cvar>0；否则 0）   # :842
uncertainty      = sqrt(control_variance)                  # :843
n_ret            = max(N_retained, 1.0)                    # :838 防零除
```

- 常数权威：kPiHalf=1.57079632679489661923（:83）；k_corr 默认
  1.4（:82，UPMW-005 MC 实证 1.3883 @pixfrac=0.8，保守上取）；
- **K_CORR_DOMAIN 选项 B（逐帧标定）**：优先帧 Drizzle provenance
  → kcorr_lookup(pixfrac, scale)（:547-555；scale 未知→300" 档
  保守 ：554）；lookup 表 :91-94 冻结数值
  {1.2112,1.3925,1.4980 | 2.3958,2.8971,3.2035}（300"/600" ×
  pixfrac 0.5/0.8/1.0），双线性插值 :97-108，域外 clamp
  [0.5,1.0]×[300,600]（:95-96）；provenance 缺失/无有效 pixfrac →
  cfg.control_k_corr（:839 回退链 frames[i].kcorr>0 ? per-frame
  : cfg.control_k_corr）；per-frame 覆盖关系与 PHASE2_SAMPLER 旧节
  （sampler.cpp:672 旧行号锚）冻结语义一致；
- UPMW-004 独立 MC 基线：Var(median) ≈ πσ²/(2N)
  （synthetic_gate.cpp:4001 先例）；本式为该基线乘 k_corr 的
  Drizzle 相关放大，禁止把 (π/2) 因子解释为其他分布假设。

### 5.5 Stage C/D/E 局部门控（:952-1009 + :847-867）

**Stage C 局部 tolerance gate（DBE-like baseline）**（:959-1007）：
对每个第一遍 accepted（reason=0）候选，收集**同 tile** 内
Chebyshev 距 ≤ background_neighbor_radius（默认 2）邻域 cells 的
该帧 cleaned median `m`：

```text
B = median(neigh)                    # :987-988（<3 → 回退 :977-985）
S = 1.4826 × median(|v−B|)           # :989-992
若 m > B + background_tolerance × S:  # :993-996
  拒绝，reason=3，++rejected_bright_tolerance
邻域 <3（回退后仍 <3）: 不 gate（保守保留）          # :977/:986
```

**Stage D 双门**（:997-1006，else-if 链互斥）：

```text
bfrac = #{v > y + contamination_sigma·σ} / n_total     # :830-832
若 bfrac > background_max_contamination: reason=4，++rejected_high_contamination
若 N_retained < min_retained_fraction × n_total: reason=2，++rejected_insufficient_retained
```

（bfrac 计算在 :830-832 于 Stage B 后立即落账 ：837，gate 于第二遍判定 ：997-1000。）

**Stage E catalogue veto**（:844-849，第一遍内）：仅当
background_catalog_veto 且该帧 SNR catalogue 非空且
frame_snr_med>0：

```text
thr = 10.0 × frame_snr_med[frame]        # :849（帧中位数 10 倍）
rad = 0.012°                             # :850 硬编码（DISP-P2SMP-004）
any_above(thr, ra, dec, rad) → reason=5 拒绝，++rejected_catalog_veto
```

frame_snr_med 为该帧 catalogue SNR 排序中位（:615-627 预计算；
:616 frame_snr_med_exact 声明、:623 精确 median_of 赋值供 fallback）。

**SNR 邻域值与可用性**（:853-867）：query(ra,dec,
snr_search_radius_deg) 收集半径内星 SNR → 中位数为 snr_val 且
snr_available=1；半径内无星 → snr_val=frame_snr_med_exact（整帧
精确中位）且 snr_available=0（**禁止以 1.0 伪装**，upm.h:51-53
冻结）；catalogue 缺失帧 → snr=0.0/available=0（:861/:862-864）。

**SnrIndex 等价性冻结**（:206-288）：dec 升序排序索引 + RA 保守
窗口（ra_win=radius/cos_guard，cos_guard=max(cos(|dec|+r),1e-4)，
:244-248 / :270-274）+ 最终判据恒为
`angular_distance_deg ≤ radius_deg`（:254-255 / :280-283）——
索引路径与全扫描**完全一致**（同一精确判据，无近似替代）。

### 5.6 frame_id 内容稳定标识（:314-438；DATA-FRAME-ID-001）

```text
frame_id = uint64(SHA-256 前缀 16 hex 大端截断)          # :422-430
SHA 流 = 9 关键 properties（creator_did/obs_title/obs_filter/
  obs_exptime/obs_date/hips_order/hips_release_date/
  hips_pixel_scale/moc_sky_fraction，缺 key 记空值；:349-352）
  + 各 tile（升序）"t=" + signal tile 原始 float 字节 + ";"（:360-379）
  + 各 tile "St=" + support tile 字节 + ";"（:381-393）
  + SNR catalogue 逐条 "i:ra,dec,snr,qf;"（max_digits10 格式化；:394-421）
路径/重命名/换根目录不变；任何科学 payload 变化 → 改变；失败/异常
→ 0（调用方 :512-523 拒绝 rc=1；禁止静默继续）。
```

禁止描述为 FNV-1a/路径派生（sampler.h:92 注释冻结；旧文档已修正）。

### 5.7 统计量共享实现（:191-204/:440-459）

- median_of：nth_element 取中，偶数 n 取 [begin,mid) 最大值均值（P0-01 修复，:196-202）；
- p2_stats_median：先过滤非 finite（:444-445），空/全 NaN 经 median_of :192 → 0.0
  （sampler.h:96 冻结）；
- p2_stats_mad：median 后 1.4826×median(|x−med|)（:452-460），
  out_median 回传收敛 median；UPM 侧共享同一实现（sampler.h:97-99 导出声明；upm.h:43-48 冻结注释同源定义）。

## 6 消费链与并行语义

**生产消费（唯一消费方 stage2.cpp）**：frame_id 预计算
stage2.cpp:219-231（p2_frame_id :222）→ sccfg 组装 :256-274（14 字段显式透传；control_k_corr 未透传，零初始化经 impl :497-498 修补回退默认 1.4；cpu_workers=cfg.exec.cpu_workers，CON-004 Runtime lease 唯一来源 ：273-274）→ probe/fill 两遍调用 p2_sample_controls_cached :279-311（probe :279-281 out_obs=nullptr 查容量；计数上限拒绝 ：296-300；分配 ：301；fill :306-311）；cov 来自 coverage union（上游 P2-COV 域）。

**并行模型**（:880-930）：worker 数=cfg.cpu_workers（0 视为 1，:883；:881 无 hardware_concurrency——模块不得自行开线程，注释冻结 ：880-882）；workers>1 时 std::thread 池 + `next_c.fetch_add` 动态
领取 tile（:886-913），**每 worker 独立 AIO 句柄**（SamplerReader
rdr.init_own :894；避免共享句柄竞争），per-cell 结果写回固定槽位
cells[idx]（:870-872，无跨线程数据竞争面），veto/insufficient 经
原子计数器归并（声明 ：887-889；归并 ：901-902/:907-908）；workers=1 串行 reference 共享
主线程句柄（:916 init_shared，循环 ：917-933）。

**确定性冻结**：cell 输出按 cells 索引固定槽位、第三遍单线程顺序
扫描、SNR 中位数经排序 median——**输出 obs 序列 bitwise 与 worker
数无关**（1/N worker 等价，sampler_parallel_consistency_test.cpp:29
TEST(Phase2SamplerParallel, OneTvsTwoTDeterminism) 承载）；全局
g_aio_mu（:161 声明，read_tile_pair :166 加锁；并行路径 per-worker 独立句柄不经此锁）。

**OpenMP 残留澄清**：PHASE2_SAMPLER 旧 §并行模型 的
`P2_ENABLE_OPENMP` 表述为历史状态——当前实现 OpenMP 已移除、
std::thread 为唯一并行路径（:879-880 注释"OpenMP 条件已移除"；
lib/phase2/CMakeLists.txt:28 option 保留仅影响旧 target 编译面）。
本节为并行语义唯一权威。

## 7 与 SCI 的对应与偏差（如实登记）

| SCI-UPM-001 语义 | 实现现状 | 判定 |
|---|---|---|
| 控制点几何由 union 几何+角间距决定，不由 SNR 决定（sampler.h:6） | cell 网格规则布置 ：737-742；SNR 只进 veto/可信度 | 一致 |
| y_ik 从实际 Phase1 HiPS 读取 | AIO 唯一 I/O（:719-733 read_tile_pair） | 一致 |
| patch robust median/MAD 保留负值 | :802-829（无符号过滤） | 一致 |
| SNR 来自 Catalogue 禁止重检测 | :851-867 纯查询 | 一致 |
| control_variance 公式（SCI-UPM-WEIGHT-001） | :840-842 逐项一致 | 一致 |
| k_corr MC 校准非猜测（sampler.h:49-50） | :82/:88-109/:547-555（选项 B 逐帧） | 一致（冻结保守值 1.4 ≥ 实证） |
| per-control geometric_reliability 参与归一化 | 采样器不产出 per-control 可靠度（UPM 侧缺陷，账本 R3-A upm.cpp:556） | 不在本模块域（登记于 P2-UPM 域） |
| wiki 语义版本 34A532A2...B2EB308 | sampler.cpp:3/:85-87 注释锚定 | 一致 |

## 8 单位与 dtype 登记（唯一权威=DATA_SEMANTICS §23）

- P2ControlObservation 14 字段（upm.h:31-57）：frame_id/control_id/
  leaf_ipix u64；ra_deg/dec_deg/value/uncertainty/snr/ivar/
  control_variance/control_ivar/support f64；snr_available int；
  quality_flags u32；
- P2SampleStats 10 字段 u64（sampler.h:63-74）；
- tile payload float32（aio read_tile_f32；:359-361/:375-379）；
- 输出 dtype/shape/invalid/可空语义唯一权威=DATA_SEMANTICS §23；
  本节单位表（§2）与之一致，冲突以 §23 为准。

## 9 边界与退化（matrix 专项 4/5）

- **tile 边界/seam**：patch 采样限制在单 tile 512×512 内（:779-780
  出界丢弃），**不跨 tile 读取**——跨 tile 接缝处 patch 有效面积
  收缩，min_samples 门（:793-800）显式拒绝而非补读邻 tile；第二遍
  邻域 gate 同样按 tile 分组限定（:954-957 `tile_cells` 映射，
  邻域只遍历同 tile cells）——tile 间不共享邻域基线（设计边界，
  冻结；跨 tile 平滑语义归 UPM Laplacian，不归采样器）；
- **空覆盖/越界 tile**：union cell 无覆盖帧 → CellStat 无 frames，
  不产 obs 但计入 control_id 占位（:946-947）；tile_ipix ≥
  12·4^order → 整 tile 占位跳过 tile=-1（:702-714）；首 tile 预读
  校验（:657-664）显式 rc=1；
- **异常/极端输入**：n_union>1e6（:634-638）、cells>2×10⁸
  （:644-648/:1071-1077）、resize OOM（:649-654）、单 cell frames
  >10000（:1083-1086）→ rc=1 或跳过；MSVC /EHa 下 SEH 兜底捕获
  （:936-944）；
- **缺失与无效值**（matrix 专项 5）：signal NaN/Inf 剔除
  （:785）；support≤0/非 finite 剔除（:786）；全 NaN 统计输入 →
  median/mad 返回 0.0（:441/:444-445，经 median_of :192）；ivar 产品缺失或 leaf 无值
  → o.ivar=0.0（"不可用"，不伪装；:1039-1056）；SNR catalogue
  缺失 → snr=0/available=0（:861-864）；frame_id=0（哈希失败
  哨兵）→ rc=1（:512-523）；
- **退化数值**：σ_bg=0 → 1e-12 floor（:829）；N_retained=0 →
  n_ret=1.0 防零除（:838）；cvar≤0 → civar=0（:842）；
- **reason 编码**（内部）：0=ok、1=insufficient_support（tile 读
  失败/坐标越界/min_samples）、2=insufficient_retained、
  3=bright_tolerance、4=high_contamination、5=catalog_veto
  （:608 注释/:759-799/:866/:995-1005）；reason 不外发
  （P2ControlObservation 无该字段），下游唯一拒绝观测面=
  P2SampleStats 六类计数（§23 §23.4）。

## 10 已冻结禁改清单（本层不可接受变化）

1. 控制点几何解耦 SNR（sampler.h:6；改动即 SCI 违约）；
2. control_variance 公式与常数（:82-83/:840-842；含 k_corr 冻结
   默认 1.4 与标定表 :91-94 九值）；
3. frame_id canonical SHA-256 输入白名单与序（:349-421；
   DATA-FRAME-ID-001，改任一输入面即破坏持久化绑定）；
4. patch 保留负值（:787-789 无符号过滤）；
5. snr_available=0 不伪装 1.0（:853-861；upm.h:51-55）；
6. ≥2 clean 帧才入 UPM（:1013-1027；相对光度约束）；
7. 输出 obs 序列 bitwise 独立于 worker 数（:886-934 槽位设计）；
8. 禁止重新检测星点（SNR 纯查询 ：851-867）。

## 11 P2-SAMP-DOC 冻结附录（2026-09-09，SRC-P2-SMP-001 源码实测）

### 11.1 返回码/错误语义（p2_sample_controls / p2_sample_controls_cached）

- rc=0：成功（含空 obs 输出——空覆盖 union 合法）；
- rc=1：错误（err 缓冲 8KB 文本，不区分细分码）：bad args（null
  coverage/hips_paths/out_n_obs/out_n_controls；:476-479）、
  frame_id 0 invalid（:512-523）、open frame failed（:536-546）、
  n_union too large（:634-638）、cells too large（:644-648 /
  :1071-1077）、cells resize failed（:649-654）、tile ipix out of
  range（:657-664）、pairs resize failed（lambda :721；并行 err :909-912；串行 err :919-923）、
  exception（:1089-1097）。错误粒度=rc 二值 + err 文本（编排层
  ACS_ERR 映射归 API-P2-001 编排面，不在本模块域）；
- out_obs/out_controls 容量不足**不报错**：按 capacity 截断拷贝、
  out_n_* 返回真实需求量（:1101-1117；probe/fill 协议
  sampler.h:101-102 冻结）；
- 并发安全：无共享可变全局态（g_aio_mu 锁仅覆盖 read_tile_pair :166；并行路径 per-worker 独立句柄），reentrant
  yes；无取消检查点（ThreadLease 接线归 P2-SAMP-IMPL 整改点，与
  DISP-COV-005 同构）。

### 11.2 现状缺陷清单（DISP-P2SMP-001..005，登记不改码，整改归 P2-SAMP-IMPL/TEST）

| ID | 锚（sampler.cpp） | 内容 | 来源 |
|---|---|---|---|
| DISP-P2SMP-001 | :485-502 | 配置修补 `<=0→默认` 吞显式 0（意图"禁用"的 0 被静默改写为默认值，如 background_clip_iters=0 想关 clipping 反而得 3） | bughunt R3-A P3-③（ledger.md:250），本任务复核锚定 |
| DISP-P2SMP-002 | :1022 | 第三遍 ：1022 对 accepted=false 且 reason==2 的帧再次 `++rejected_insufficient_retained`，与第二遍 ：1006 递增重复——P2SampleStats.rejected_insufficient_retained 对该类拒绝双计数（统计面偏差，obs 输出不受影响；:1078-1079 注释自述曾修 double-count，此残留与其意图矛盾） | 本任务实测 |
| DISP-P2SMP-003 | :641-643/:655/:666-672/:705-713 等 17 处 | 诊断进度日志直写 stderr（fprintf/fflush），未走结构化日志通道，err 缓冲外；生产可观测性债（静默失败排查依赖 stderr 文本） | 本任务实测 |
| DISP-P2SMP-004 | :849-850 | catalog veto 阈值 `10.0×frame_snr_med` 与半径 `0.012°` 硬编码，未入 P2SamplerConfig 配置面（与 snr_search_radius_deg 可配置不对称；schema 冻结缺口） | 本任务实测 |
| DISP-P2SMP-005 | :818 | clipping 相对收敛阈值 `1e-12×max(|m0|,1e-12)`：m0≈0 时阈值≈1e-24 过严，实际退化为固定 background_clip_iters 轮全迭代（结果仍确定、单调收缩、min_samples 兜底；无科学输出影响，性能观察级） | 本任务实测 |

### 11.3 TEST-P2-SMP-DESIGN-001 冻结测试设计（可执行 TEST-P2-SMP-001 由 P2-SAMP-TEST 落地，双面登记不冒认）

覆盖 matrix P2-SAMP 行五项科学专项；每个 threshold 引用本节，
禁止事后改：

| # | 设计面 | 冻结容差 | 现状测试锚 |
|---|---|---|---|
| F1 | 统计量单元：median odd/even/负值/重复/乱序/NaN 过滤；MAD=1.4826×median 偏差 | 逐值 bitwise（EXPECT_DOUBLE_EQ） | synthetic_gate.cpp:3594 G1StatisticsCorrectness（先例在库） |
| F2 | kcorr_lookup 边界与角点：pf∈{0.5,0.8,1.0}×sc∈{300,600} 九值、域外 clamp、provenance 缺失回退 1.4 | 角点值 exact；插值点 rtol 1e-12 | 无（新建；表值 :91-94） |
| F3 | control_variance 解析 oracle（Python 复算 k_corr×(π/2)×σ²/N_ret） | rtol 1e-12；UPMW-004 MC 基线 3σ | synthetic_gate.cpp:4001/:4061/:4089（先例在库） |
| F4 | 坐标/tile 映射：单 tile 合成 → 64 cell (ra,dec,leaf_ipix) 对独立 HEALPix 参考实现 | atol 1e-9 deg；cell 索引单射 exact | 无（新建） |
| F5 | constant/gradient/impulse 验证面：constant patch（σ→1e-12 floor 路径）、线性梯度 patch（亮端 clipping 方向性）、单像素 impulse（bfrac=1/n_total 路径） | cvar rtol 1e-12；接受/拒绝判定 exact | 无（新建；公式 :829-843） |
| F6 | 边界/seam：patch 跨 tile 边界截断、相邻 tile 互不污染、第二遍邻域同 tile 限定、空覆盖 tile 占位 | obs 集合 exact；node 占位数 exact | 无（新建） |
| F7 | missing/invalid：NaN/support≤0 过滤、全 NaN patch reason=1、ivar 产品缺失 o.ivar=0、frame_id=0 rc=1、tile 读失败 rc=1 | 判定 exact；rc exact | ivar_wiring_test.cpp:223（ivar 面先例）；其余新建 |
| F8 | 串行/并行等价：1 worker vs N worker 全输出 bitwise | bitwise | sampler_parallel_consistency_test.cpp:29（先例在库，扩展 grid/worker 矩阵） |
| F9 | 拒绝计数守恒（现状口径）：candidate=Σ|frames|、六类计数与 reason 分布自洽（含 DISP-P2SMP-002 双计数现状） | 计数 exact（按 §11.2 登记现状口径） | 无（新建） |

负面矩阵：null 参数 rc=1、frame_id=0 rc=1、坏 HiPS 路径 rc=1、
n_union/cells 上限 rc=1、probe 容量协议（out_obs=null 查量→分配→
fill）。fixture 由固定 seed 合成 HiPS 树生成，不提交大二进制。

### 11.4 SCI 层状态声明（本任务零 SCI 改动）

- 采样语义权威已有 FROZEN SCI：SCI-UPM-001（docs/science/
  PHASE2_UPM.md，T106 2026-08-23 冻结，集合 SCI-UPM-001..010 +
  SCI-UPM-WEIGHT-001 + SCI-UPM-PERSIST-001；页头明示模块
  "phase2 (upm/sampler)"）。**不因本任务改动**（共享 SCI 引用不
  改动；P1-WCS-DOC SCI-WCS-001=共享 ASTROMETRY.md、P2-COV-DOC
  SCI-UPM-001/SCI-INT-001、P2-INT-DOC SCI-INT-001、P2-REJ-DOC
  SCI-REJ-001 先例）。
- matrix P2-SAMP 行 science_id=SCI-P2-SMP-001（descriptor 占位
  词汇，module_adapters.cpp:657）的语义映射由本节声明——
  **SCI-P2-SMP-001 ⇒ SCI-UPM-001**（docs/science/PHASE2_UPM.md，
  矩阵 science_doc=docs/science/PHASE2_UPM.md，
  MOD-astrocs-phase2-sample 行，2026-09-09 P2-SAMP-DOC 冻结）。
  SCI 公式语义不在此重复定义，两处冲突时以 docs/science/ 为准并
  回改本文档（禁止反向）。辅助语义锚：control_variance 权威=
  SCI-UPM-WEIGHT-001（§5.4 承接）；robust 统计上游=SCI-NOISE-001；
  链位置=SCI-SCOPE-001 §处理链第 5 步。
- 本节禁止被编排层词汇反向改写（descriptor astrocs.phase2.sample
  由 P2-XX-INT 对齐，不作冻结依据）。

## 12 关联 ID 映射（本文件承接）

- `ALG-P2-SMP-001` = 本文档整体（逐符号锚 §3/§5；矩阵 P2-SAMP 行
  algorithm_id，INDEX.yaml path 绑定本文件）。
- `ALG-UPM-CONTROL-IVAR-001`（既有 INDEX 条目，path 历史上即绑定
  本文件）= 本文档 §5.4 + §2（control_variance/control_ivar 公式
  与 k_corr 域）；两 ID 并存不冲突——ALG-P2-SMP-001 为模块合同
  全集，ALG-UPM-CONTROL-IVAR-001 为其方差子面（UPM 权重消费方
  引用），本文件为两 ID 共同权威页（多 ID 同文档先例：
  NOISE_ESTIMATION.md 承载 ALG-NOISE-001..003）。
- 旧词汇 `ALG-P2SAMPLE-001..N` / `TEST-P2SAMPLE-*`（旧版本文档
  尾节遗留）⇒ 由 ALG-P2-SMP-001 / TEST-P2-SMP-001 替代，旧 ID 不
  注册 INDEX、不入矩阵（本节即为退役声明）。
- `UPMW-004/005/007`（MC 验证项词汇）⇒ 测试锚对应本文件 §11.3
  F3（UPMW-004/007）与 k_corr 校准来源（UPMW-005，§5.4）。

## 13 追溯

- 上游 SCI: SCI-UPM-001（共享 FROZEN 零改动）；
- 本层: ALG-P2-SMP-001（本文件）；ALG-UPM-CONTROL-IVAR-001（子面）；
- 下游 DATA: DATA-P2-SMP（DATA_SEMANTICS §23）；API: API-P2-SMP-001
  （PUBLIC_API.md）；MOD: MOD-astrocs-phase2-sample（registry 行，
  合同三件套 lib/phase2_samp/）；TEST: TEST-P2-SMP-DESIGN-001
  （§11.3 设计冻结，registry 页承载）→ TEST-P2-SMP-001（可执行，
  P2-SAMP-TEST 落地前 MISSING）；
- 矩阵行：MOD-astrocs-phase2-sample（P2-SAMP 行原位融合，
  depends_on_int=P2-COV-INT;CPU-005，MODULE_MIGRATION_MATRIX.csv
  :15 权威）。
