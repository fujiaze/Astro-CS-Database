# 全仓对抗性静态排查 · 切片 S25「lib/algorithms/drizzle 核心＋hips＋入口」

**检查员**: S25 只读检查员（只查不改；本文为唯一产出）
**先读声明**: 已先读 `独立审计/实验重做/总编对账/检查-修复验证.md` PASS 表（红 7 PASS/1 FAIL、黄 15 PASS/1 FAIL、行漂移 6/6 PASS）与 `独立审计/实验重做/总编对账/分歧台账.md` D-01…D-11、A-* 终裁；上轮已订正并验证通过处本报告一律不重报（DRIZZLE.md:108 的 1.0415 订正、DRIZZLE.md:104→spherical_overlap.cpp:40、spherical_overlap.cpp:857→859、1.532 复算、docs F&H 双锚定、D 系全部终裁）。

**责任域覆盖（逐一开文件）**:
- `lib/algorithms/drizzle/healpix_drizzle/`: astro_sphere_sink.cpp/.h、drizzle_engine.cpp/.h、fits_reader.cpp/.h、healpix_core.cpp/.h、hp_drizzle_api.cpp/.h、hp_drizzle_hips_api.cpp、hp_drizzle_internal.h、poly_clip.cpp/.h、reverse_drizzle.cpp/.h、snr_evaluator.cpp/.h、spherical_overlap.cpp/.h、v6_drizzle_science.cpp/.h、v6_spherical_overlap.cpp/.h、wcs_sip.cpp/.h、nanoflann.hpp（仅对接）
- `lib/algorithms/drizzle/hips/`: include/astrocs/hips/{types.h,publish.h}、src/{module_entry.cpp,aio_publish.cpp}、CMakeLists/module.yaml/README
- `lib/algorithms/drizzle/include/astrocs/drizzle/types.h`、`lib/algorithms/drizzle/src/module_entry.cpp`
- 互查文档: docs/science/algorithms/DRIZZLE_GEOMETRY.md（434 行全读）、HEALPIX_MAPPING.md（55 行全读）、HIPS_WRITER.md（§0–§5 精读＋锚抽验）、docs/science/DRIZZLE.md（250 行全读）；旁证 lib/algorithms/drizzle/README.md（A-P3-09 点名文件）

**方法**: ①常数/公式全域 grep＋逐处开上下文；②文档 file:line 锚用脚本逐条实开原文件比对行内容（DRIZZLE_GEOMETRY 46 条、HIPS_WRITER 18 条、DRIZZLE/README 手工 30+ 条）；③文献题录 scholar_fetch 实开；④「文档说有、代码没接」专项：符号存在性 grep（finalize_tile / threadTiles / pragma omp）＋配置键与默认值三方对账（defaults.json ↔ CONFIG_CONTRACT ↔ 源码）。

---

## 一、红级（必须改）

### R-25-01 ｜ 红 ｜ lib/algorithms/drizzle/healpix_drizzle/hp_drizzle_api.h:114、lib/algorithms/drizzle/README.md:93（域外: orchestrator.cpp:180/:181/:198、module_adapters.cpp:7603）

- **问题**: 【A-P3-09 残留核零】禁抄值 211034.6 在责任域内仍有 **2 处**、全库 **4 处**注释仍把它当作 hp_res 常数使用。
- **证据**:
  - `hp_drizzle_api.h:114`:「2 次幂 NSIDE 使 HEALPix 特征尺度 hp_res = **211034.6**/nside <= finest」——把抄写值写成定义；
  - `lib/algorithms/drizzle/README.md:93`:「auto nside：最小 2 次幂 ≥ **211034.6**/finest_arcsec」；
  - 域外同值: `lib/infrastructure/pipeline/orchestrator/cpp/src/orchestrator.cpp:180/:181/:198`、`lib/infrastructure/scheduler/src/module_adapters.cpp:7603`（均为 `// ≈ 211034.6`）；
  - **反方核验（正本为真值）**: `drizzle_engine.cpp:706-710` 按表达式 `sqrt(π/3)*(180/π)*3600` 求值（=211076.28514206142），`DRIZZLE_GEOMETRY.md:157` 记真值 211076.28514206142″——实现正确、注释错；
  - **与「已清除」主张冲突**: `实验/healpix-polar/REPORT_paper.md:83` 写「禁抄值 5 处残留清单已在审计中登记清除（A-P3-09）」，实况仍在（与 run/全仓-01/ledger-raw.json:172 登记一致）；`artifacts/evidence/audit-2026-01/FIX_LEDGER.csv` M2a-D-1 仍 **OPEN**；
  - `DRIZZLE_GEOMETRY.md:163/:165` 与实验/审计目录内的出现是**以禁抄值身份引用**，合法，不计残留。
- **建议改法**: 域内两处按 FIX_LEDGER M2a-D-1 处置①「注释改 211076.3 或直接引用表达式（删字面量）」；域外两文件同批处理；`REPORT_paper.md:83` 的「已清除」改为如实登记。
- **所属面**: ④

### R-25-02 ｜ 红 ｜ lib/algorithms/drizzle/healpix_drizzle/spherical_overlap.cpp:1001-1002

- **问题**: 切平面/gnomonic 偏差系数旧注**未按 A-P3-02/04 终裁撤换**，而文档已宣称撤换——「文档说已改、源码没改」。
- **证据**:
  - 源码现况: :1001「球面面积 = 切平面有向叉积和 × (1 + O(θ²)), θ=max_angle」、:1002「**θ < 1e-3 rad 时偏差 < 4e-8**」——无符号、量级与终裁差 12.5×；
  - 文档现况: `DRIZZLE_GEOMETRY.md:340` DISP-DRZ-005 行「θ=1e-3 时切平面偏差 ≈ **−θ_max²/2 = −5.0e-7**（恒负、单向下偏，θ_max 按 drop 最远顶点角距约定，A-P3-02/04；**旧注 “<4e-8” 缺符号且偏小 12.5 倍，撤换**）」，`:389` 负面用例注记与 `**:433**` 订正记录同词——三处文档锚 `注释 :1001-1007` 指向的正是这行未改注释；
  - 终裁: 分歧台账 A-P3-02/04（切平面正交投影 −0.2512·θ²；生产实现 θ_max 约定 −5.0e-7）。
- **建议改法**: 按已终裁的 A-P3-02/04 与 DRIZZLE_GEOMETRY 订正记录执行（**科学系数注释，本切片只登记上呈，不另提改法**）；改后同步核 :1078-1079 同组注释。
- **所属面**: ①（连带 ②：订正注记新旧两说）

### R-25-03 ｜ 红 ｜ docs/science/algorithms/DRIZZLE_GEOMETRY.md:43、:349-357（旁证 spherical_overlap.h:93）

- **问题**: 核权重分母在同文档内**两说并存**，其中 `w_jp = a_jp/A_pixel,j` 一说与本文件自身条款、FROZEN 正本、源码三者相反。
- **证据**:
  - 说 A: :43「权重: `w_jp = a_jp / A_pixel,j`」；:349-351「processPixelSharedTiled 内**权重恒为** `weight = overlap_area / pixel_area`（pixel_area = 未收缩源像素球面面积）」；
  - 说 B（同文档）: :48「**核按 drop 面积归一**」、:55「Σ_p w_jp = 1 ⇒ Σ_p F_p = Σ_j x_j，与 pixfrac 无关」、:70、:344 DISP-DRZ-009「canonical 核 w_jp=a_jp/A_drop,j，**正向** weight = overlap_area / drop_area」；
  - 纯读即矛盾: 按 :43 定义 Σ_p w_jp = A_drop/A_pixel = pixfrac² ≠ 1（pixfrac<1），与 :55 冲突；且 :43 的 w 与 :59 的 w'_jp = a/A_pixel **同式**，则 :59 的「w'_jp = pixfrac²·w_jp」蕴含 pixfrac²≡1；
  - 正本: `DRIZZLE.md:45` w_jp = a_jp/A_drop,j；`:158` 不可接受变化第 3 条逐字「把核权重写回 `a_jp/A_pixel,j`（则 Σ_p F_p = pixfrac²·Σ_j x_j…FZ-COND-FLUX-CONSERV 判红）」；`:159` 分母禁止取 D_p；
  - 源码: `drizzle_engine.cpp:1617` `Scalar weight = overlap_area / drop_area;`、`:1644` `sumNorm += overlap*(pixel_area/drop_area)`、`:1608-1616` 注释与正本同向；
  - 既有登记（同根未闭合）: `独立审计/08_修复包/④面积交叠与分配/01_缺陷清单.md:116-127` 缺-6（S1，点名 spherical_overlap.h:93 现仍写「A_pixel,j 是 SCI-DRZ-001 §5 的 w_jp 分母」），`04_二次核对 缺-6` 仍 OPEN。
- **建议改法**: **科学公式/冻结定义口径，本切片只登记上呈**（与缺-6 一并裁决：量名钉死后统一 :43 / :349-357 / spherical_overlap.h:93 三处）。
- **所属面**: ①②③

### R-25-04 ｜ 红 ｜ docs/science/DRIZZLE.md:64（同式 DRIZZLE_GEOMETRY.md:59、docs/plugins/algorithms_phase1/08_drizzle.md:41）

- **问题**: 等价参数化的分母恒等式量纲与数值均不成立：`N'_p = Σ_j w'_jp = D_p/pixfrac²`。
- **证据（独立复算，非转写）**:
  - 量纲: 左 Σ_j w'_jp 无量纲（w' = a/A_pixel，sr/sr），右 D_p/pixfrac² 为 sr——不齐；
  - 数值反例: A_pixel=1e-6 sr、pixfrac=0.8、a=3e-7 sr ⇒ 左 = 0.30，右 = 4.6875e-7（比值 6.4e5）；
  - 正确关系（同页自洽推导）: **Σ_j w_jp**（非 w'）= D_p/(pixfrac²·A_pixel)（单位像素约定下 = D_p/pixfrac²），而 Σ_j w'_jp = D_p/A_pixel（单位约定 = D_p）；要与 canonical c_jp = a/(A_pixel·D_p) 相等必须 **N'_p = D_p**；
  - 与本文件自身冲突: `DRIZZLE.md:63` 自述「给出**同一个** c_jp」，按 :64 所给 N' 代入得 c' = pixfrac²·c（pf=0.8 ⇒ S_p 偏 36%）；
  - **反方核验（独立审计同文）**: `独立审计/06_实施/tasks/ACSD-T28`（T21/T36/T08/T33 同文）逐字「w'_jp = a_jp/A_pixel 只作为等价参数化成立，且必须同时换分母（**N'_p = Σ w'·A_pixel = D_p**）」，并称等价性已独立复核；
  - 未在上轮三报告与 PASS 表出现 ⇒ 非重复项。
- **建议改法**: **FROZEN 正本公式，只登记上呈，不给改法**（三处同式同批裁决）。
- **所属面**: ①

### R-25-05 ｜ 红 ｜ lib/algorithms/drizzle/healpix_drizzle/hp_drizzle_api.h:53

- **问题**: 公开 ABI 头对 pixfrac 的**默认值、值域、缝隙方向三处同时相反**（打回报告B §399 ③ 整改项至今未落）。
- **证据**:
  - 头现况: `hp_drizzle_api.h:53`:「pixfrac: 像素收缩因子 (**0.0~1.0**, **默认 1.0 避免源像素固有缝隙**)」；
  - 权威值: `eng/packaging/config/defaults.json:666-677` `drizzle.pixfrac = 0.8`、`authority_status: owner_adjudicated`，note 逐字：「**默认 0.8：drop 相互重叠，输出网格无未覆盖缝隙；取 1.0 时相邻 drop 仅边界相接，源像素落点错位处出现无覆盖缝隙**」——与头注释「1.0 避免缝隙」方向相反；
  - 值域: `defaults.json:669` `0 < pixfrac <= 1`、`DRIZZLE.md:35/:199`、`CONFIG_CONTRACT.md:54` 均为 **(0,1]**，头写 0.0~1.0（0.0 在契约层被拒）；
  - 同域旁证: `drizzle/README.md:123`「pixfrac（**缺省 0.8**，CFG-001）」；
  - 既有登记: FIX_LEDGER M2a-C-6（两条生产入口默认 0.8 vs 1.0，P1 OPEN/STILL）、M2a-B-3（值域 [0,1] vs (0,1]，OPEN）。
- **建议改法**: 与 owner_adjudicated 0.8、(0,1]、缝隙方向正写对齐；**默认值属裁决值，具体改法上呈**。
- **所属面**: ③④

### R-25-06 ｜ 红 ｜ lib/algorithms/drizzle/src/module_entry.cpp:355

- **问题**: 模块入口 drizzle op **缺键兜底 = 1.0**，与登记册/正本 0.8 分裂（静默默认、无日志、无 provenance 标记）。
- **证据**: `module_entry.cpp:354-355` `if (!found) pixfrac = 1.0; /* 缺键默认 1.0 (契约只约束 (0,1]) */`，校验域 [0,1]（:359-361）；对照 `drizzle/README.md:123`「Stage1Config.drizzle：pixfrac 缺省 0.8，CFG-001」、defaults.json 0.8（owner_adjudicated）、`DRIZZLE.md:26`「默认 0.8」；两生产入口分裂 = FIX_LEDGER M2a-C-6（P1 OPEN/STILL）；打回报告B §399 ③ 要求「生产缺键兜底与本头一次对齐」未落。
- **影响**: 配置键缺失的调用静默走 1.0（相邻 drop 仅边界相接、落点错位处出缝隙），与登记语义相反且产物不可区分。
- **建议改法**: 与 R-25-05 同批对齐；**默认值变更上呈，本切片不越权改**。
- **所属面**: ④

### R-25-07 ｜ 红 ｜ docs/science/algorithms/HIPS_WRITER.md:22-26（行号锚声明）＋ §1–§5、§9 全部 aio_hips_writer.cpp 锚

- **问题**: 文档声明「§4 (4b)(4c)、§5 (5a)(5b)、§9 的锚**已按修订后行号重标**，行号整体推移 +57 起、最大约 +147」，实况全部锚偏移 **+710…+1228 行**，声明与实况相反。
- **证据（逐条实开 lib/infrastructure/aio/src/hips/aio_hips_writer.cpp，2874 行）**:

| 文档锚（HIPS_WRITER.md） | 声称内容 | 实开行号 | 偏移 |
|---|---|---|---|
| :40 `aio_hips_product_begin :386-422` | 产品生命周期起点 | 定义在 **:1165** | +779 |
| :67 `aio_hips_write_signal_support_tile :424-562` | 叶级写出 | 定义在 **:1262** | +838 |
| :42 `leaf_order = ilog2(nside)（:411）` | 叶阶 | `ps->leaf_order = ilog2_u64(nside)` 在 **:1210** | +799 |
| :216-:1105-1108 ASTROCS_DRIZZLE_PIXFRAC | provenance 键 | **:1870** | +765 |
| :218 `set_drizzle_provenance :1408-1440` | setter | 定义在 **:2118** | +710 |
| `finalize_image_product :697-789` | 方差/属性 finalize | **:1816** | +1119 |
| `aio_hips_finalize :1018-1131` | 收尾 | **:2246** | +1228 |

  同文档对 `aio_hips.h:34-43`（flags 位域）锚**相符** ⇒ 漂移只在 writer.cpp 一侧，且声明的 +57…+147 校准值本身失效。
- **反方核验**: `独立审计/08_修复包/05_正向规格/04_二次核对.md:109`「行号锚将漂移 10 处（4-6 月新代码未入锚）OPEN」方向一致，但未覆盖本表 7 条，也未触及「已重标」这一声明本身。
- **建议改法**: 按该文档自身 §22-26 规则重标或整体改符号绑定；订正「+57…+147」与「已重标」两句。
- **所属面**: ②（订正注记新旧两说）④（锚逐条实开）

---

## 二、黄级（建议改）

### Y-25-08 ｜ 黄 ｜ astro_sphere_sink.cpp:528、v6_drizzle_science.cpp:128/:393/:419/:454

- **问题**: 把「核按 drop 面积归一」这一**权重公式**口径单锚 `F&H 2002 §7.2`，与 D-03 双锚定（权重更新式/s² = §2 式(2)–(5)；§7 只作 a_xy 定义句与方差/相关）不齐。
- **证据**: D-03 终裁（分歧台账:240/:432）「路线1 的失误仅在把 w_jp=a/A_drop 的公式归到 §7.2」；**正确样式**同域已存在——`drizzle_engine.cpp:1608-1610`、`drizzle_engine.h:67`、`v6_drizzle_science.h:24` 均写「§7.2 式(7) 下方逐字 a = “the fractional area overlap of **the drop**…”」（定义句锚，合法）；上列 5 处为裸 §7.2 公式锚。
- **建议改法**: 补 §2 式(2)–(5) 分列或改引定义句样式（同 drizzle_engine.cpp:1608）。
- **所属面**: ④

### Y-25-09 ｜ 黄 ｜ lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp:1490

- **问题**: 「---- Step 4: 计算 drop 球面面积 **(Girard 定理)** ----」违反同批文档禁用命名令，且该处**未被 DISP-DRZ-002 登记**（登记面漏项）。
- **证据**: `DRIZZLE_GEOMETRY.md:140-144`「本模块面积算法的唯一命名 = Sutherland–Hodgman 球面逐边裁剪 + Van Oosterom & Strackee 扇形三角剖分；**禁用 “Girard 定理”（内角和式）这一命名——本模块无该实现**」；`:337` DISP-DRZ-002 只登记 spherical_overlap.h:15,77 与 spherical_overlap.cpp:11（三处实开仍在），**未含本处**；`:1490` 实际走 polygon_area_consistent（VOS）分支。
- **建议改法**: 将本处纳入 DISP-DRZ-002 清单，按「从 SCI 到实现」方向处置。
- **所属面**: ②③

### Y-25-10 ｜ 黄 ｜ spherical_overlap.cpp:1581、spherical_overlap.h:397

- **问题**: 候选缓冲注释写「像素外接圆半径上界取 **1.2 × hp_res**（HEALPix 像素最坏情况外接半径 **≈ 1.19×res**）」，与实际常数、与 D-01 终裁三处不一致。
- **证据**: 实现 `spherical_overlap.cpp:1616` `buffer_rad = HP_CIRCUMRADIUS_FACTOR * hp_res_rad`、`:42` 常数 = **1.25**；`.h:397` 同写 1.2；D-01 终裁「全天 sup ≈ **1.0415**·hp_res（N=64 全天穷举），1.25/1.0415 = 1.2002 ≥20%」（DRIZZLE_GEOMETRY:431、DRIZZLE.md:108）。注释值 1.2/1.19 既非实现值也非终裁值（按 1.19 记账只余 5% 裕量，与 ≥20% 结论相反）。
- **建议改法**: 注释与 spherical_overlap.h:397 对齐 1.25 ＋ D-01 口径（1.0415 sup）。
- **所属面**: ①③

### Y-25-11 ｜ 黄 ｜ spherical_overlap.cpp:1657、:1661（对照 :1679-1680）

- **问题**: 同函数内注释两说——:1657/:1661「快速路径…delta 乘 **1.25** 安全系数…取 **1.25** 覆盖解析+浮点」，实现 `delta = ceil(radius_px_d * **1.15**)`（:1680，:1679 注「安全系数 1.15」）。
- **证据**: 实现 1.15；`DRIZZLE.md:101`「赤道 delta×**1.15** 畸变系数」、`DRIZZLE_GEOMETRY.md:138`「赤道 delta×**1.15**」——文档与代码一致，**仅 1650-1663 注释块仍是 1.25**；解析上界 1.127、实测最坏 1.14（:1660-1661 自述）。
- **附注**: FIX_LEDGER M2a-D-1 处置②「delta 系数（1.127/1.14/采用值）三选一冻结并登记」仍 OPEN——本条不裁系数，只登记注释与实现不一致。
- **建议改法**: 统一注释与实现（系数取值属登记册事项，上呈）。
- **所属面**: ②④

### Y-25-12 ｜ 黄 ｜ 死符号 `aio_hips_writer::finalize_tile`（11 处引用）

- **问题**: 文档与注释反复以 finalize_tile 作实现锚，**源码中该符号不存在**（「文档说有、代码没接」死符号）。
- **证据**: `grep -rn 'finalize_tile' lib/` 只命中文档/注释（DRIZZLE.md:187、DRIZZLE_GEOMETRY:28/:101/:342、drizzle_engine.cpp:3、README:44/:89/:180、HIPS_WRITER:7/:117/:280、DATA_SEMANTICS:389、docs/modules/healpix_drizzle.md:17）；`aio_hips_writer.cpp` 实际符号为 `finalize_image_product`(:1816)、`finalize_hierarchy`(:1972)、`finalize_snr_product`(:1985)、`aio_hips_finalize`(:2246)。行为本身存在（signal = flux_sum/covered_area 归一在 aio_hips_write_signal_support_tile 内）⇒ 属符号名失效而非行为缺失。
- **建议改法**: 全部引用改绑真实符号（finalize_image_product / aio_hips_finalize）。
- **所属面**: ④

### Y-25-13 ｜ 黄 ｜ lib/algorithms/drizzle/README.md:131-138（对照 docs/science/algorithms/DRIZZLE_GEOMETRY.md:225-247）

- **问题**: 两份域内文档对**并行实现**与 **1/N 确定性**给出相反两说，且 README 描述的实现在源码中已不存在。
- **证据**:
  - README:131-138:「现状并行 = OpenMP `parallel for schedule(static) num_threads(...)`（drizzle_engine.cpp:1670-1671）+ per-thread tile 累加器（:1645-1646）…合并 = 串行按线程序 t=1..N-1 合入 **threadTiles[0]**…**跨线程数浮点和序不同不保证 bitwise**」；
  - 实开: `grep threadTiles drizzle_engine.cpp` → **0 命中**；`grep 'pragma omp' drizzle_engine.cpp` → 仅 `#pragma omp parallel num_threads(...)`（:1923/:2254/:2279），**无** `parallel for schedule(static)`（该式只在 snr_evaluator.cpp:323）；现行方案 = `drizzle_deterministic_stripe_count`(:1687) ＋ `merge_tile_map_into`(:1702) ＋ n_stripes(:1840)；
  - DRIZZLE_GEOMETRY:240-247:「**1/N 确定性成立（跨线程数 bitwise）**…回归锁 p1drz_taskset_invariance（taskset 1/2/4/8/16）」；`drizzle_engine.cpp:1676`「与 taskset/OMP 实际线程数无关」与之同向 ⇒ README 的确定性陈述为反向旧说，且与其自引的 ALG-DRZ-001 §6 冲突。
- **建议改法**: README §6 按现行 stripe/pool 方案重写，确定性表述与 DRIZZLE_GEOMETRY §6 对齐。
- **所属面**: ③④

### Y-25-14 ｜ 黄 ｜ drizzle_engine.cpp:1530、:1643；docs/science/DRIZZLE.md:152；DRIZZLE_GEOMETRY.md:356-357

- **问题**: 四处把「pixfrac==1 路径」称作**默认路径**，其中一处直接断言配置值 `defaults.json drizzle.pixfrac=1.0`——与 owner_adjudicated 0.8 冲突（配置事实错误）。
- **证据**: `drizzle_engine.cpp:1529-1530`「硬约束: 默认路径零回归, **defaults.json drizzle.pixfrac=1.0**」（实况 0.8，defaults.json:667）；`:1643`「(默认路径零回归的代数保证)」；`DRIZZLE.md:152`「（**默认路径**零回归）」；`DRIZZLE_GEOMETRY.md:356`「**pixfrac==1 默认路径**的 .norm.hiss…」。四者定义的「默认」= 1.0，而 defaults.json / CONFIG_CONTRACT:54 / README:123 / DRIZZLE.md:26 均为 0.8 ⇒ 现配置下根本不走该分支（pixfrac=0.8 时 sumNorm≠sumArea）。pixfrac==1 的逐位不变性本身有 sha256 锁、**不受影响**——错的是「默认」归属。
- **建议改法**: 「默认路径」改为「pixfrac==1 路径」并删除/更正 :1530 的配置值断言。
- **所属面**: ③④

### Y-25-15 ｜ 黄 ｜ 汇总：域内文档 file:line 锚逐条实开不符（DRIZZLE_GEOMETRY.md / drizzle README / DRIZZLE.md）

- **问题**: 关键代码锚系统性漂移，部分锚指到内容完全无关的行。以下 12 条**逐条实开**（脚本＋人工双读）:

| 文档锚 | 声称内容 | 实开行内容 | 判定 |
|---|---|---|---|
| DRIZZLE_GEOMETRY:9 `hp_drizzle_api.h:43,62,70,130,139,140` | C ABI 导出 | :43 计数字段/:62 参数/:70 注释/:130 结构尾/:139-140 结构头；实导出在 **:58,:78,:86,:102,:131,:187,:196,:197** | 六条全错 |
| DRIZZLE_GEOMETRY:27/:100 `astro_sphere_sink.cpp:99-104` / 「:100 传出原始累加量」 | dense 化/传累加量 | :99 `}`、:100 `if (nside < 512)` | 内容错位 |
| DRIZZLE_GEOMETRY:56 「:1629 前后」（sumNorm） | 累加分母 | :1629 是 trace `cb` 赋值；真累加 **:1639/:1640/:1644/:1649** | 漂移 ~15 行 |
| DRIZZLE_GEOMETRY:202/:338 `drizzle_engine.cpp:1570-1577`（pixfrac 校验） | 引擎拒绝 | :1570 是 trace 选点；真校验 **:782-789** | 漂移 ~210 行 |
| DRIZZLE_GEOMETRY:215 `:1439-1441,1509-1512`（A_drop<1e-20、w≤0 拒绝） | 拒绝条件 | :1439 是注释；真拒绝 **:1603/:1618** | 漂移/错位 |
| DRIZZLE_GEOMETRY:111 `drizzle_engine.cpp:1306`（四角构造） | 半 0.5·pixfrac 四角 | :1306 `tret = writer.add_tile(...)` | 错位 |
| DRIZZLE_GEOMETRY:8/:31 `CMakeLists.txt:356-366`、`:379-382` | astrocs_drizzle 静态库/omp 注释 | 现 **根 CMake :669-697**（:694-696 为 omp 注释） | 漂移 ~310 行 |
| DRIZZLE_GEOMETRY:336 DISP-DRZ-001 `hp_drizzle_api.h:93` / `.cpp:98-103` | sip_order「0..4」vs [0,5] | :93 是 Phase1 C ABI 注释；注释在 **.h:150**、校验在 **.cpp:103-105** | 锚漂移（差异本身真实） |
| DRIZZLE_GEOMETRY:341 DISP-DRZ-006 `drizzle_engine.h:62-63` | 3 字段注释 | :62-63 是 FP32/FP64 注释；release 字段注释在 **:64** | 漂移 2 行 |
| README:13/:79 `hp_drizzle_api.h:42,62,70,130,139,140`、`:42-51,62-75,130-140` | 签名源 | 同上 | 同款全错 |
| README:60/:86/:87/:95 `drizzle_engine.cpp:1567-1574 / 1436-1438,1505 / 1527-1534 / 1712` | 校验/面积/累加/NaN 跳过 | 实际 **:782-789 / :1603,:1618 / :1639-1650 / 计数在 :2000-2027** | 漂移 11–210 行 |
| DRIZZLE.md:104/:187 `drizzle_engine.cpp:100,736-762` | 方差/实现锚 | 已由 DISP-DRZ-007 登记行漂移，**但其给出的「现行方差锚」astro_sphere_sink.cpp:100 亦已失效**（真归一在 :527-547） | 登记锚自错 |

- **反方核验**: DRIZZLE_GEOMETRY:33-34 自述「符号名优先于行号，行号仅作导航」可降级，但不豁免「六个导出锚全错」「:100 内容错位」这类指向不同对象的错误。
- **建议改法**: 统一重锚（参照 04_二次核对 缺-7 既有登记，扩到本表 12 条）；DISP-DRZ-007 的「现行方差锚」同步改 `astro_sphere_sink.cpp:527-547`。
- **所属面**: ④

### Y-25-16 ｜ 黄 ｜ lib/algorithms/drizzle/README.md:43、:88

- **问题**: 以 `S_p=F_p/D_p` 概括归一式，字面触碰正本 §10 不可接受变化第 4 条。
- **证据**: `DRIZZLE.md:159`「将 S_p 的分母取覆盖面积 D_p=Σ_j a_jp 而非面亮度归一分母 N_p（pixfrac<1 偏 1/pixfrac²…判红）」；README:43/:88 写「不负责：**S_p=F_p/D_p** 面亮度归一…归一在 astro_sphere_sink.cpp:100 + aio_hips_writer finalize_tile」。实际链路为 `astro_sphere_sink.cpp:527-547` 先折算 `k = sb_publish_scale(sumArea,sumNorm)`、writer 再除 covered_area（:532 逐字给出 sig = sumFlux·D_p/(sumNorm·A_cov)）⇒ 结果等价于 F_p/N_p；README 未写 k 折算，读者按字面实现即触发判红。
- **建议改法**: 改写为 `S_p=F_p/N_p`（或注明「writer 前已乘 k=sumArea/sumNorm」）。
- **所属面**: ①③

---

## 三、绿级（可不改）

### G-25-17 ｜ 绿 ｜ lib/algorithms/drizzle/include/astrocs/drizzle/types.h:44-47

- **问题**: 导出计数自相矛盾且与头不符：「**六导出**中仅这两个在 v1 模块面」＋「其余**四导出** (fits_to_ahpx / reverse_capability / reverse_version)」只列 3 个；实开 `hp_drizzle_api.h` 有 **8 个** HP_DRIZZLE_API 导出函数（:58,:78,:86,:102,:131,:187,:196,:197），其中 `hp_drizzle_run_phase1_hips`、`hp_drizzle_compute_auto_nside` 未在清单说明中出现。
- **证据**: 上表实开；`module.yaml:30-64`、`types.h:48-49` op 词表本身无误。
- **建议改法**: 计数与清单补全（不影响 ABI/行为）。
- **所属面**: ②④

### G-25-18 ｜ 绿 ｜ docs/science/DRIZZLE.md:108

- **问题**: 「球面裁剪外接圆半径 `1.25·hp_res` **覆盖**赤道对角线 `1.532·res`」——直接比较时 1.25 < 1.532，字面读不通（res 与 hp_res 是否同尺度未标注）。
- **证据**: 上轮 R-3 已复算 1.532 本身无误（π/(2nside)/hp_res = 1.535，球面实测 1.532，PASS 表红-3）⇒ **本条不翻案**，仅登记「覆盖」关系表述含混（真正被覆盖的半径量是 1.0415）。
- **建议改法**: 改述为「快速拒绝半径 = max_angle + 1.25·hp_res ≥ max_angle + 1.0415·hp_res（D-01 sup）」，赤道对角线另起一句。
- **所属面**: ①

---

## 四、本片专项核对结果（逐条交代）

| 专项 | 结果 |
|---|---|
| **A-P3-09 禁抄值 211034.6 残留核零** | **不通过**：域内 2 处（hp_drizzle_api.h:114、drizzle/README.md:93）＋域外 2 文件 4 行仍残留 → R-25-01（红） |
| **spherical_overlap.cpp:857 附近 WCS_ADAPTIVE_MAX_DEPTH=12** | 实开 :859 定义 =12、:857 注释「max_depth = 12 (每边最多 4096 段)」、:892 使用 ✓；**12 ≥ 9，D-02 矢高预算已满足**（:857 vs :859 为注释/定义之分，非错） |
| **矢高预算注释若提 depth=8** | 全域 grep 矢高/sagitta/8.094/depth=8 → 域内**零命中**（仅 tests/reference_overlap.cpp:170 的弧垂 margin `sagitta ≈ α²/8`，不同对象）⇒ 无 D-02 冲突 |
| **切平面/gnomonic 偏差系数注释（A-P3-02/04）** | **未修**：spherical_overlap.cpp:1001-1002 仍为「<4e-8」，而文档三处已称撤换 → R-25-02（红） |
| **F&H 注释锚与 D-03 双锚定** | 部分不齐：定义句锚 3 处合规（drizzle_engine.cpp:1608、drizzle_engine.h:67、v6_drizzle_science.h:24）；裸 §7.2 公式锚 5 处 → Y-25-08（黄）。文献题录经 scholar_fetch 实开 [arXiv:astro-ph/9808087](https://arxiv.org/abs/astro-ph/9808087)：题名 *Drizzle: A Method for the Linear Reconstruction of Undersampled Images*、A. S. Fruchter & R. N. Hook、v2 (19 Oct 2001) ✓ 与 DRIZZLE.md:207/:215、DRIZZLE_GEOMETRY:411 一致 |
| **弦四边形总面积不得作逐叶 ALE 判据（A-P3-10）** | **合规**：grep `ALE/弦四边形/总面积.*判据` 于本片源码与三份算法文档零命中；DRIZZLE.md §11 与 DRIZZLE_GEOMETRY §9 的逐叶判据均为 `|S_p/B0−1|<1e-3`、`false_negative=0`、`max_abs==0` 等非退化负例，未见以全天总面积恒等作逐叶证据 |

---

## 五、已查无问题面

### ① 科学性（常数/公式/单位/量纲/适用域 vs D 系终裁）

- **D-01 外接半径**: `spherical_overlap.cpp:27-42` 注释块只写「1.25×hp_res（覆盖两者+浮点舍入）」，不引旧读数 1.0442/1.1284；`DRIZZLE_GEOMETRY:119-137/:431` 与 `DRIZZLE.md:108` 均为 sup≈1.0415 ＋ 裕量 1.2002，1.1284 记为极冠叶对角常数 ✓（方式：grep `1.044|1.1284` 全域，docs 仅存新旧对照与改记账）。域内另有 1.19/1.2 旧值 → Y-25-10。
- **D-02 深度**: 无 depth=8 表述（见专项表）；生产 12 已满足 ≥9。
- **D-09 亏缺律**: grep `0.104369|0.1043885` → 域内与 docs/algorithms 零命中 ✓（与 PASS 表「docs 无 0.104369 残留」一致）。
- **A-P3-09 真值常数**: `drizzle_engine.cpp:705-714` 按表达式求值、注释写 211076.3，:639-645 钳位 [16,2²²] 的尺度推算（0.05″–3.66″）独立复算相符 ✓。
- **方差链**: `drizzle_engine.cpp:1645-1650` `sumVarNum += v·w²`、`drizzle_engine.h:56` `variance_p = sumVarNum/sumNorm²`、`DRIZZLE.md:83/:120/:202` `/N_p²`、`astro_sphere_sink.cpp:545-547` 同步乘 k²——四层同向 ✓（三处逐行读比对）。
- **单位/量纲**: `DRIZZLE.md:31`（px²/sr/ADU/ADU² 逐项）、`:194`、`DRIZZLE_GEOMETRY:42/:68-74` 与 DATA_SEMANTICS §31.1a 链一致 ✓；A_cell = 4π/(12·nside²) 在 `astro_sphere_sink.cpp:330`、`drizzle_engine.cpp:706`、`spherical_overlap.cpp:1240` 三处同式 ✓。
- **判据非退化（负例资格）**: `DRIZZLE.md:169-173` 常量面亮度门负向注入（每像素常量 ADU、S_p=F_p 漏归一、分母取 D_p 三档）必须判红 ＋ `--inject-legacy-pixfrac2`；`DRIZZLE_GEOMETRY:58` 的 `1/pf²−1` 判据（pf=0.8 → +56.25%）；`:383`「分母取 A_drop 必判红」；9003 例 `false_negative=0` 双侧枚举——均为可执行正/负例，**未见恒真门** ✓。
- **无量纲常数**: 三层缓冲 1.25/3.0/1.15 在 DRIZZLE.md:99-101、DRIZZLE_GEOMETRY:128-138、源码 :42/:1199/:1616/:1680 逐条对数一致（delta 系数注释例外 → Y-25-11）。

### ② 行文逻辑（论证链/单文档矛盾/订正注记/UNRESOLVED）

- `DRIZZLE_GEOMETRY.md:425-433` 订正记录表（D-01、D-03、A-P3-02/04 三行）与正文 :122/:52/:340/:389/:433 逐处对照——**文档侧新旧闭合**，未见「订正后各说各话」（唯一断裂是文档已声明而代码侧未改 → R-25-02）；
- `DRIZZLE.md:108` 内联 `<!-- 订正: ... -->` 新旧对照完整（1.044→1.0415），FROZEN 文件仅此一处改动 ✓；
- 全文无 UNRESOLVED 混入正文当结论（grep UNRESOLVED/TBD/待定 于四份互查文档零命中；「待复核」仅见于 08_修复包，非正文结论）；
- `HEALPIX_MAPPING.md`（55 行全读）: 输入/输出/前后条件/不变量/复杂度/数值风险/ID/文献七节自洽，Postconditions 1e-12° 与文献节 Project-defined 标注一致，**无问题**；其声称的「lib/algorithms/shared/healpix 唯一权威实现」实开存在（lib/algorithms/shared/healpix/{healpix_core.cpp,.h} ✓）。
- **例外**: R-25-03（同文档权重两说）、R-25-07（「已重标」声明与实况反）、G-25-17（计数自相矛盾）。

### ③ 跨文档冲突（五方权威链）

逐主题对数（L1 docs/ASTROCS_DESIGN ↔ docs/ ↔ 实验/ ↔ 独立审计/08_修复包 ↔ 源码）:

- **核权重/分母**: DRIZZLE.md §5/§7/§10 ↔ DRIZZLE_GEOMETRY §1/§10 ↔ drizzle_engine.cpp:1617/:1644 ↔ 08_修复包 缺-6——除 R-25-03 所列三处外其余全部同向 ✓；
- **D-01/D-02/D-03/D-04/D-08**: 域内文档与代码常数逐一对数，无翻案项残留（D-08 的 k_corr 表/1.4 域外回退不在本片）；
- **pixfrac 默认**: defaults.json ↔ CONFIG_CONTRACT ↔ DRIZZLE.md ↔ README ↔ module_entry ↔ hp_drizzle_api.h 三方对账 → R-25-05/06、Y-25-14；
- **并行与 1/N 确定性**: README ↔ DRIZZLE_GEOMETRY ↔ 源码 → Y-25-13；
- **HiPS 产品面**: HIPS_WRITER ↔ DRIZZLE.md:201（support=D_p/A_cell）↔ aio_hips_writer（行为存在）↔ drizzle sink（k 折算）——**语义一致**，仅符号/行号失效（Y-25-12、R-25-07）；
- **DISP-DRZ-001…009 九行**逐行核对: 001/002/003/004/005/006/008/009 所述差异**均与源码实况相符**（sip_order 0..4 vs [0,5]、Girard 命名、pixfrac 双轨、NaN 计数 :2000-2027/:2203-2208、切平面三分支 :1091/:1288/:1331、4 字段、poly_clip 零调用、weight=overlap/drop_area），差异本身不是新问题；仅 007 的「现行方差锚」自错（并入 Y-25-15）。

### ④ 幻觉与锚（文献题录、file:line 实开、文档说有代码没接）

- **文献题录抽验**: [arXiv:astro-ph/9808087](https://arxiv.org/abs/astro-ph/9808087) 实开核对 Fruchter & Hook 题录（题名/作者/v2 日期）✓ 与 DRIZZLE.md:207/:215、DRIZZLE_GEOMETRY:411 一致；Górski 2005 DOI 10.1086/427976（DRIZZLE_GEOMETRY:410、HEALPIX_MAPPING:43）上轮已按「文献（Górski 2005 §4）+ 独立复算」核过（PASS 表 §4.1），本轮不重复翻案；`web_search` 两次返回空源（工具侧无结果），故 DOI 侧以单篇实开＋上轮 PASS 为据，此为本轮局限，如实登记。
- **file:line 锚实开**: 脚本批量核对 DRIZZLE_GEOMETRY 46 条 ＋ HIPS_WRITER 18 条 ＋ 手工 30+ 条；**相符**者包括: DRIZZLE.md:98→spherical_overlap.cpp:40 ✓、DRIZZLE_GEOMETRY:130/:157/:159→drizzle_engine.cpp:706-710 ✓、DISP-DRZ-005 三个分支锚 :1091/:1288/:1331 ✓、DISP-DRZ-004 计数锚 :2000-2027/:2203-2208 ✓、DRIZZLE_GEOMETRY:147→drizzle_engine.cpp:1662-1666 ✓、HIPS_WRITER:54-55→aio_hips.h:34-43 ✓、HIPS_WRITER:73→shared/healpix/healpix_core.cpp:287-296 ✓、HIPS_WRITER:76→healpix_core.h:39-43 ✓、module.yaml 的 module_id/dll_name/threading_model 与 types.h:8-10 一致 ✓、根 CMake 含 astrocs_p1_drizzle RPATH 遍历 ✓；不符者全部入 R-25-07/Y-25-15。
- **文档说有、代码没接 专项**: `finalize_tile`（死符号 → Y-25-12）、`threadTiles`（零命中 → Y-25-13）、`parallel for schedule(static)` 在 drizzle_engine 零命中、`defaults.json drizzle.pixfrac=1.0`（假断言 → Y-25-14）、hp_drizzle_api.h 六个导出锚全错（→ Y-25-15）、types.h 导出计数（G-25-17）。**反向核验（代码有、文档没记）**: hp_drizzle_run_phase1_hips / hp_drizzle_compute_auto_nside 两导出未进 types.h:44-47 清单（G-25-17）；`sb_publish_scale`（astro_sphere_sink.h:50）为两末端归一唯一源，四份文档均未点名（非行为缺失，登记备改）。
- **配置死键 专项**: `DRZ_CFG_KEY_*` 9 键在 module_entry.cpp:341-481 全部被读取 ✓；`HIPS_CFG_KEY_*` 13 键与 `HIPS_M_KEY_*` 19 键在 hips/src/module_entry.cpp:433-567 全部被消费（HIPS_M_KEY_SNR_ST_B64 → types.h:97 定义 ✓），**无死键**。
- **第三方对接（只查接口不查内部）**: `nanoflann.hpp` 仅被 snr_evaluator.cpp:17 include；适配器 `kdtree_get_point_count/kdtree_get_pt/kdtree_get_bbox` 与 `KDTreeSingleIndexAdaptor<L2_Simple_Adaptor,double,PointCloudAdaptor>`(3, …, Params(32)) 构造、PIMPL 隐藏头（snr_evaluator.h:101）符合其公开接口用法，注释「nanoflann Size=size_t」与模板参数一致 ✓；cfitsio/nlohmann 不在本片域内，未触。

---

## 六、统计

| 级别 | 数量 | 编号 |
|---|---|---|
| 红 | **7** | R-25-01…07 |
| 黄 | **9** | Y-25-08…16 |
| 绿 | **2** | G-25-17…18 |
| 合计 | **18** | |

**最重要的三项**: ① A-P3-09 禁抄值残留核零**未通过**（域内 2 处、全库 4 处仍当常数用，论文却已称「已清除」）；② 核权重分母 `a/A_pixel` 与 `a/A_drop` 两说在 DRIZZLE_GEOMETRY 内部并存，且等价参数化分母恒等式量纲/数值不成立（涉及 FROZEN 公式，只上呈）；③ pixfrac 默认 0.8（owner_adjudicated）与 1.0（ABI 头注释、模块缺键兜底、drizzle_engine 断言）三方分裂，缝隙方向注释与登记册相反，打回报告B 整改项未落。
