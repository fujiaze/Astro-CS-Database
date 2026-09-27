# 检查-S2-scienceA：docs/science 甲组只读对抗巡检

- **切片**：S2「docs/science 甲组」，责任域 = `ACR_EQUIVALENCE.md` / `ASTROMETRY.md` / `CALIBRATION.md` / `CONTROL_WEIGHT_SNR.md` / `DRIZZLE.md` / `INTEGRATION.md` 六文件，全文逐行精读 + 全部 file:line 锚逐条开验。
- **方法**：只读（零 git 写、零构建、零测试执行）；文献经 web/Crossref/PDF 逐字核验；数值经实验结果 JSON 对数；源码锚逐条 sed/grep 开验。
- **排除（按纪律不重报）**：`检查-修复验证.md` PASS 表已验证项（DRIZZLE.md:108 R-3 订正、D_p² 等价参数化、CONTROL §8a/§8b 订正注记、CALIBRATION:468 UNRESOLVED-as-caveat、CONTROL:204→NOISE_MODEL.md:86 锚等）；D-01…D-11 与 A-* 裁决不重裁；上轮三份报告（科学性/行文逻辑/跨文档冲突）已登记且非本切片责任的项不重报。
- **产出计数**：**红 4 · 黄 3 · 绿 10**。

---

## 一、红级（4）

### 红-1 ｜①科学性｜docs/science/CALIBRATION.md:241（推导式）、:253（Δb 定义）、:277（适用域）、:409-411（§11 曝光容差门）

**问题**：§6a 冻结判据 `|K·Δb| ≤ ε·σ_frame` 的推导式 `cal_pipe − cal_true = (t_light/t_d)·(b_light − b0) = −K·Δb` 与 `Δb ≡ b0 − b_light` 是**分支无关书写**，适用域 :277 却明文覆盖「master_dark 与亮场经 §5 **双分支之一**校准」，而 §5 默认分支是 `dark_opt=0`。代数核验表明该式**只在 dark_opt=1（母版含 bias）下成立**，在默认分支方向反转：

- 分支 1（dark_opt=1，母版含 bias）：`cal = raw − bias − K·(dark_total − bias)`，`dark_total(t_d) = b0 + I_d·t_d` ⇒ 残留 `= −K·(b0 − b_light) = −K·Δb`——与文档一致 ✓。
- 分支 0（dark_opt=0，已减 bias 母版 = 默认合规）：`cal = raw − bias − K·dark`，`dark(t_d) = b0 + I_d·t_d` ⇒ 残留 `= −K·b0`（`K·I_d·t_d = I_d·t_light` 精确相消）。文档式给出 `−K·Δb = −K·b0 + K·b_light`，两者恒差 `K·b_light`：
  - **合规母版**（已减 bias ⇒ `b0≈0`；T2 实测 b_light≈1001.87 ADU）：真实残留 ≈ 0，判据值 `|K·Δb| ≈ K·1001.87 ADU` ≫ `ε·σ_frame`（σ_frame=57.82 ADU，ε 任何合理值）⇒ **假红——连 Δt=0 都判不合格**；
  - **违规母版**（含 bias 却按默认分支校准 =「多减一次 bias」，CALIBRATION_ALGORITHMS.md:549/:570 明示该错配在两套真实母版上真实存在）：真实残留 `≈ −K·1001.87 ADU`（大错），判据值 `Δb = b0 − b_light ≈ 0` ⇒ **恒绿——恰对判据要防的错误漏判**。
- 负例同病：:274「Δb ≡ 0 ⇒ 残留逐位为 0」只在分支 1 成立；分支 0 下 `b0=b_light` 正是双重减本底的最大错，残留不为 0。

**证据（含反方核验）**：① 公式、Δb 定义、适用域均在 CALIBRATION.md 冻结节原文（:241/:253/:277），:409-411 把它定为「科学面 = 唯一判据」的强制门；② 实测表 :264-270 三例全部基于 T4，而 §6:158-159 已判定 T4 母版「不满足已减 bias 约定 ⇒ 须走 dark_opt=1」⇒ **证据面只覆盖分支 1**；③ 证据正本 `run/SCI-FIX-SEMANTICS-01/evidence/dark_tolerance_criterion.json` 含同式（`"formula": "cal_pipe − cal_true = … = −K·Δb"`）但 **无任何 dark_opt/分支限定串**（grep dark_opt 零命中）；④ 反方核验——`docs/science/algorithms/CALIBRATION_ALGORITHMS.md` 全文 grep `Δb`/`曝光容差` 零命中，ALG 侧无限定条款可救；docs/ASTROCS_DESIGN.md:381 只管预检面 warn，不涉科学面；⑤ 源码 calibrator.cpp:121-140（k_init=k / 1.0 两分支）证实分支确实不同式。
**处置**：按纪律——**冻结科学判据问题仅登记红级与证据，不提修改方案**，上呈负责人按 §8 查证流程裁决适用域与分支定义。
**所属面**：①（③ 对照无第二口径，不重复计面）。

### 红-2 ｜④幻觉与锚 + ③跨文档冲突｜docs/science/DRIZZLE.md:27、:37、:148、:203（另 :147 佐证）

**问题**：DRIZZLE.md 在符号表、输入有效域、极端条件表、术语四处断言 drizzle 生产实现含一套**极区 prune 机制**：「`|dec|>45°` 走保守 prune」「`θ_q+radius>90°` 保守不剪枝遍历极冠树」「Lipschitz `C=π/2, C45=π/(2√2)`」，证据列写「`gaia_client.c` 复用」。**该机制在 drizzle 模块不存在，证据锚指向另一个模块**：

- 反方核验①：`lib/algorithms/drizzle/` 全目录 grep `prune|剪枝|Lipschitz|C45` 及 45/90 阈值——仅 nanoflann 头文件注释与 `dec<-90||dec>90` 合法性检查（spherical_overlap.cpp:1219/:2485/:2508），**无 45° 阈值、无 θ_q 判据、无 C/C45 常数**；
- 反方核验②：`boundary_fallback` 生产标识符仅存于 `lib/algorithms/drizzle/healpix_drizzle/tests/candidate_oracle_test.cpp:121` 字符串标签；drizzle 实际边界处理 = face/极冠盒查询 + `buffer_rad = 3.0*hp_res`（spherical_overlap.cpp:1524-1525）+「边界回退: 保守 inclusive 查询」（:1664-1693），与文档写的不同形；
- 反方核验③：C/C45、`θ_q+radius>90°` 的真实实现在 `lib/infrastructure/gaia_xpsd_client/src/gaia_client.c:1096/:1097/:1124`（`polar_plane_intersects` :1108，`AE_CAP_BOUNDARY_DEG=45`）——**星表 cone-search 域**；`astrocs_p1_drizzle` 链接库只有 Threads/OpenMP/ZLIB/m（`lib/algorithms/drizzle/CMakeLists.txt:118-131`），**不链 gaia_xpsd_client**，drizzle 源码无任何 gaia_client include；
- 反方核验④：③对应算法文档 `docs/science/algorithms/DRIZZLE_GEOMETRY.md` **无任何极区 45°/90° 条款**（grep 零命中）；同一套常数在 `ASTROMETRY.md:28/:64/:150/:251`、`PLATESOLVE.md:23/:51`、`GAIA_QUERY.md:113/:126/:151` 中锚到 gaia/platesolve 实现且逐条相符——即该条款属**移植残留**，只有 DRIZZLE 侧失锚。

**影响**：按 §4/§8 核 drizzle 极区行为将找不到实现（「doc says exists, code not wired」）；证据列「gaia_client.c 复用」是错误指认（跨模块）。
**建议改法**：上呈裁决——该条款系从 ASTROMETRY/GAIA 域误植或确有未接线设计，须由负责人确认后二选一（移除或指认 drizzle 侧实现），不在本巡检内改冻结文本。
**所属面**：④ + ③。

### 红-3 ｜②行文逻辑 + ④幻觉与锚｜docs/science/ASTROMETRY.md:107 ↔ :265

**问题**：同一锚 `ipv_wcs.h:43,57-60,70-71` 在同一冻结文档中得到**互斥裁决**：
- §5a:107：「`ipv_wcs.h:43,57-60,70-71` 把该输出注释为"0-based FITS 像素"，属**标签错误**（**见 §14a**）」；
- §14a:265：「`ipv_wcs.h:43,57-60,70-71` 的注释**与 §5a 一致**」。

两句不能同真：若注释是标签错误，则与 §5a（out.x = 1-based）不一致；若一致，则 :107 的「标签错误」裁决与「见 §14a」指向失效。
**证据（含反方核验）**：① 源码实况 `lib/algorithms/platesolve/cpp/ipv/include/ipv_wcs.h`：:42「迭代式反演: 天球 → 像素 (**0-based FITS**, Y-down)」、:58-59「STD-F1/R-02 方案 b: ipv 内部保持 **0-based 自洽约定**，本函数不做 FITS 1-based 换算」、:62-66「本函数属 ipv 内部 **0-based 口径**，禁止 +1；FITS 1-based 桥接唯一责任方 = Phase3 导出边界（合同见 docs/science/ASTROMETRY.md）」、:69-70 struct 字段「0-based FITS 像素 x/y」——头文件四段一致标注 0-based；② 数学核验 §5a 推导成立：`out.x = u + CRPIX = det_x + 0.5 = i + 1`（0-based 下标 i ⇒ 1-based 像素号），`ipv_wcs.cpp:942-946` `out.x = u + wcs.crpix[0]` 实存 ⇒ 头文件「0-based」标注意确与 1-based 输出相左（§5a 判「标签错误」有据）；③ 反方核验——ASTROMETRY.md 全文 grep `订正|注记` **零命中**，:265 不是已登记的过期残留；`检查-修复验证.md` PASS 表与上轮三报告 grep `ASTROMETRY|ipv_wcs|桥接` 零命中，均未裁过此项；④ 头文件自称「合同见 ASTROMETRY.md」（:62），而文档一侧说其注释错误、另一侧说其一致——源码-文档互指链断裂（④维度）。
**建议改法**：两说必须择一并使 §5a/§14a 同步（涉及 R-02 方案 b 裁决与源码注释面，按 §8 流程查证后统一，不单改一处）。
**所属面**：②（主）+ ④。

### 红-4 ｜①科学性（冻结定义形式化）｜docs/science/INTEGRATION.md:55

**问题**：§5 连续定义（冻结）中 invalid_input 的形式化谓词语法破损：
`invalid_input ⇔ ∃i: accepted(i) ∧ (¬finite(values[i]) ∨ **non-finite support≤0** ∨ non-finite weights ∨ weights<0)`
「`non-finite support≤0`」按字面读是「（非有限的 support）≤ 0」——类型未定义（NaN/Inf 与 0 比较无意义）；按平行结构（同行 `non-finite weights ∨ weights<0`）应为两个独立析取项，但缺失 `∨` 与左操作数。该式是状态门 INVALID_INPUT 的**形式化正本**，不同读法给出不同判定域。
**证据（含反方核验）**：① 源码 `lib/algorithms/coverage/src/integrate.cpp:36-38`：`!std::isfinite(in->support[i]) || in->support[i] <= 0.0` ⇒ 意图 =「非有限 **∨** ≤0」；② 同文档散文 :43「`support` 非空时每个 support_i 须 `finite ∧ >0` 否则该样本 INVALID_INPUT」正确、:45 values 行正确 ⇒ 破损仅在 :55 形式化行；③ 算法层同位伪码 `docs/science/algorithms/INTEGRATION_ALGORITHMS.md:53`「if support non-finite/≤0 → invalid」正确（其 :33 F3 行简写 `support≤0` 同样含糊，非本切片责任文件）；④ 反方核验——无任何读法能使 :55 与 :43/:45/源码三者同时成立。
**处置**：按纪律——**冻结 §5 形式化判据问题仅登记红级与证据，不提修改方案**，上呈按变更流程订正。
**所属面**：①（兼 ②：正文与式子自相不一致，主面记①）。

---

## 二、黄级（3）

### 黄-1 ｜②行文逻辑｜docs/science/ASTROMETRY.md:46-58 ↔ :151

**问题**：同一冻结文档对 STD-F1 桥接给出两套表述：
- §3a:46-58：列 3 处桥接（closure_metric.py:223 origin=0 / p3_wcs.cpp:519 fits_pixel_1based / p1_wcs.json xp=x+1），并明文「**「唯一桥接点」这一表述不成立**，正确口径是「每量一次」」；
- §7:151（独立不变量）：「`xp = x + 1` 该桥接**只发生一次**——位于 Phase3 导出边界的**单一函数（§5a）**；**ipv 内部 0-based 输出** `u = x − CRPIX` 不变」。

「只发生一次——单一函数」正是 §3a 声明不成立的表述形态；且 §7 称 ipv 内部为「**0-based 输出**」，与 §5a/§3a:49-51「ipv 输出已是 **1-based** FITS p、+1 已含于该输出」相反（同一量两种口径）。§7 自称依据 §5a，实际与 §5a 相左。
**证据（含反方核验，从严记数）**：最宽容读法可把 §7「该桥接」限定为「FITS 导出侧」（p1_sources/p1_wcs.json 非 FITS 导出），使「单一函数」与 §3a 共存——但 (a) 文本无该限定语，(b) 「ipv 内部 0-based 输出」与 §5a 的 1-based 断言即便按 u（偏移量）vs out.x（输出）区分也措辞互串（§7 把偏移 u 称「输出」），(c) §7 是验收不变量面，两说并存会把 §3a 列的 3 处合规单次换算误判成「重复桥接」。全文无订正注记。与红-3 同根（ipv 口径）但冲突对象不同（§3a/§7 桥接计数 vs §5a/§14a 注释裁决），独立登记。
**建议改法**：§7:151 与 §3a/§5a 三处口径择一统一（或补限定语并明确 u/out.x 各自口径）。
**所属面**：②。

### 黄-2 ｜④幻觉与锚（行号漂移汇总，14 组）｜多文件

**问题**：锚的**内容均存在于同文件且语义相符**，但行号系统性漂移（部分严重）。按上轮 Y-3 同标准合并一条：

| # | 文档锚 | 源码实况 | 漂移 |
|---|---|---|---|
| 1 | ASTROMETRY:49/:51/:56/:110/:114-116 `module_adapters.cpp:2487-2493、:2731-2733、:2555-2557、:2811-2813` | +1 桥接 `xp = x + kP1FitsPixelOrigin` 在 :4677-4678/:5002-5003；`pixel_origin` 声明在 :4744-4746/:5085-5087 | ~1700-2300 行 |
| 2 | ASTROMETRY:101 `ipv_select.cpp:943,947`（cx=img_w/2.0） | `cx = img_w / 2.0` 在 :1121（另 :1426/:1707/:2037）；:943-947 为选星失败日志 | ~178 行 |
| 3 | ASTROMETRY:100 `sdet_api.cpp:546-549`（det_x=i+0.5） | `sp.dx = x + 0.5 - cx` 在 :537-538 | ~9 行 |
| 4 | ASTROMETRY:103 `ipv_wcs.cpp:322`（inv_det 奇异拒绝） | `det_lin` 判据 :343-344、`inv_det_lin` :356 | ~21-34 行 |
| 5 | ASTROMETRY §2 表 `ipv_wcs.cpp:328-331`（cd_inv） | `cd_inv_00..11` :356-360 | ~26 行 |
| 6 | ASTROMETRY §2 表 `ipv_wcs.cpp:340-356`（A/B 系数） | `sip.A[...]` 赋值 :371-386 | ~31 行 |
| 7 | ASTROMETRY §2 表 `ipv_wcs.cpp:464`（AP/BP 逆系数） | `sip.AP[idx]` :504-513 | ~40-49 行 |
| 8 | CALIBRATION:129 `module_adapters.cpp:2129-2147,2198-2216`（EXPTIME fail-closed） | dark :2470-2485、light :2543-2555 | ~340-360 行 |
| 9 | CALIBRATION:356/:362 `module_adapters.cpp:2062-2084`（check_master_domain 调用） | `mu::check_master_domain` :2408/:2418、`check_flat_normalized` :2432 | ~344-370 行 |
| 10 | CALIBRATION:377 `module_adapters.cpp:1992-2002`（退化 flat fail-closed） | :2333-2341（「degenerate flat must not be consumed」） | ~340 行 |
| 11 | CALIBRATION:230 上游 `docs/ASTROCS_DESIGN §4.3:254 / §4.5:300` | 母版标度红线句在 :335（§4.3 ✓）、🟠 warn 句在 :381（§4.5 ✓） | 行 ~81（章节号正确） |
| 12 | ACR §5(a) `stage2.cpp:1137-1142`（depth>64 回退） | 实际回退 = try/catch「ACR block failed, fallback CPU」:1236-1246；:1137-1142 是 ACR 块内 tile 读取循环 | ~100 行（内容不同位） |
| 13 | ACR §5(b) `rejection.cpp:2064-2082`（minimum_n/UNDERDETERMINED 双条件） | `UNDERDETERMINED：n <= underdetermined_n ...` :2084-2091 | ~20 行 |
| 14 | DRIZZLE:144 `spherical_overlap.cpp:192`（几何 NaN 显式拒绝） | 开半球 NaN 拒绝注释块 :201-207（:190-191 为 Van Oosterom 参考注释） | ~9 行 |

**反方核验**：逐条开验确认内容存在、语义相符（例：module_adapters.cpp:4677-4678 注释原文「pts 里的 x/y 是 **0-based 数组下标**…施加**恰好一次** xp = x + kP1FitsPixelOrigin (SCI-WCS-001 §3a/§5a)」✓；:2470-2485「K 只要 dark 在位就必须由 FITS EXPTIME 推导…缺失/非正/不匹配 →」✓）；其余主锚（约 60 条）已核**无漂移**（见绿级）。
**建议改法**：按文件批量重锚（module_adapters 两文件共 7 组漂移最大）；不改内容。
**所属面**：④。

### 黄-3 ｜④幻觉与锚（证据指针路由缺口）｜docs/science/CONTROL_WEIGHT_SNR.md:115 ↔ :195/:209

**问题**：§2b 全部实测数值（14.2%、0.376、37.31、1.36802、−0.4985、2.0% 散布）的证据指针是 :115「本节数值与判据的证据锚在 **§8a/§8b**（实验单元与结果文件路径由彼处给出）」，但 §8a 依据清单 = `b1/b2/b4`、§8b 依据清单 = `b3/b6`——**这些文件不含 §2b 的任何数值**；数值正本实际在 `实验/absolute-snr/results/b7_absolute_snr_recon.json`，而 b7 在 §8a/§8b/§2b 中均未被列出。
**证据（含反方核验）**：逐值对数全部命中 b7：`src_term_captured_fraction=0.1424371097824749`（:1692，=14.2%）、`DIAG_sdet_median_abs_rel_D_core=0.3764637450243278`（:580/:1948）、`median_src_over_sigma_D_core=37.30793689757321`（:455/:1937）、`naive_over_full_median=1.368022039473573`（:939/:1933）、`noise_impl_lag1_rho=-0.49849569160800516`（:942）；`B7_ABSOLUTE_SNR_RECON.md:55` 同述 lag-1 −0.4985。§8a 清单三文件 grep 这五个值全部不命中（b1 中 0.4985 出现在无关的 `sigma_f_skyonly_rn_adu` 不确定度位）⇒ **数值本身全部真实、路由指针指错**。
**建议改法**：§2b:115 或 §8a 依据清单补入 b7（及 B7 报告），使证据链可达；数值无需改。
**所属面**：④。

---

## 三、绿级（10，核验通过留档）

| # | 面 | 核验项与结果 |
|---|---|---|
| 绿-1 | ④ | FITS Standard 4.0 引文（CALIBRATION:31-38）：PDF `fits.gsfc.nasa.gov/standard40/fits_standard40aa-le.pdf` HTTP 200，文本定位 §4.4.2.5「Keywords that describe arrays」，BUNIT 句「...in which the quantities in the array, **after application of BSCALE and BZERO**, are expressed.」**逐字相符**；「§4.3 = Units」与 PDF 内部引用一致。 |
| 绿-2 | ④ | 文献 DOI 五条 Crossref 解析全符：Newberry 1991 `10.1086/132801`（PASP 103, 122，题名 Signal-to-noise considerations for sky-subtracted CCD data ✓）；Van Oosterom & Strackee 1983 `10.1109/TBME.1983.325207`（IEEE TBME 30(2), 125-126 ✓，与 spherical_overlap.cpp:190-191 源码注释 BME-30(2),125-126 同值）；Sutherland & Hodgman 1974 `10.1145/360767.360802`（Comm. ACM 17(1) ✓）；Górski 2005 `10.1086/427976`（ApJ 622(2), 759-771 ✓）；Goldberg 1991 `10.1145/103162.103163`（ACM Comput. Surv. 23(1), 1991-03 ✓）。F&H/Aitken/Rousseeuw 等 14 条上轮已核不重报。 |
| 绿-3 | ④ | ACR_EQUIVALENCE 锚组全符：六用例 `synthetic_gate.cpp:3355/:3395/:3466/:3513/:3591/:3648` + GTEST_SKIP :3476 精确；launcher 三指针 `acr_kernels.cpp:359-361`（cpu/legacy_parallel 同指 mosaic_reject_legacy、cuda 指 mosaic_reject_cuda）✓；`scalar_bytes=60`（2·size_t+2·int+2·double+int+size_t+2·int=60，wmode+workers 10 槽布局 ✓）；`kernel_registry.cpp:21` 精确相等判定、`:26-28` empty-domain 拒绝 ✓；`:81-83` px/depth throw、`:90-91` 默认 −4.0/+3.0、`:96-97` minimum_n=3/underdetermined_n、`:99-100` fabs、`:141-143` wmode=2 throw、`:212-245` OpenMP 分支、`:305/:331` CUDA 调用带 und_n、`stage2_common.cpp:469-471` acr_route 校验 ✓；`coverage/CMakeLists.txt:28` P2_ENABLE_OPENMP 默认 OFF ✓；`acr_cuda_bridge_host.cpp:910` `frame_count > 64` 拒绝 ✓；`stage2_common.cpp:511-530` `p2_acr_block_eligible` **恒 return false**（注释原文「本函数恒 false：ACR 块在生产不可达」）⇒ §4「构造性结论」与源码一致 ✓；`register_phase2_acr_kernels` :343 ✓；`stage2.cpp:67` kAcrWeightModeIvar=2、:972 调用点、:1226-1246 ACR try/catch 回退 ✓；`eng/tests/api/test_reject_integration_oracle.py` 存在 ✓。 |
| 绿-4 | ④ | CONTROL_WEIGHT_SNR 实现锚全符：`stage2.cpp:82` frame_snr_medians、`:258-261` frame_snr_by_id、`:394-420/:396/:401-403` local_snr_unavailable 与回退、`stage2_common.cpp:431-440` A44 权重模式域删除注释、`:469-471` acr_route 校验、`:511-530` p2_acr_block_eligible 恒 false、`sampler.cpp:78` kSnrCatalogMax=1<<16、`:244-265` out_qual OR 累积，逐条相符。 |
| 绿-5 | ④+③ | INTEGRATION 锚与算法层一致：`integrate.cpp:10-79`（:21 memset、:49-50 sup_max 在权重分支**之前**、:60-77 状态机）与 §5 逐行同构；`integrate.h:1-75` 状态枚举 :55-59/canonical reducer :26-27 ✓；`hiss_writer.cpp:335-365` BUNIT 冻结集 {ADU/sr, ADU²/sr², sr²/ADU²} 与 §3 逐字相符 ✓；回归门 `eng/tests/unit/p2_output_semantics_test.cpp:85-107` = B2-A7 零权 accepted 计入 support ✓（CMakeLists:815-819 已注册）；③ `INTEGRATION_ALGORITHMS` F2/F3/F5/F6/F6a/状态表与 §5 同口径（除红-4 所指简写外无冲突）。 |
| 绿-6 | ④ | CALIBRATION 主锚组：`calibrator.cpp:91` `if(!(med>0.0f)) return`、`:85-100` normalize_flat floor、`:122` `float k = k_init`、`:129/:139` 两分支 `max(flat,0.1f)`、`:144` `*actual_k = k`、`:157-189` calibrate_d ✓；`master_generator.cpp:243-255` 归一/floor/合并、`:86` single-frame copy、`:120` sigma<=0 break ✓；`master_unit_guard.h:38-39/:146-147`（kNormalizedDomainCeiling=1.0 ⇒ 「median>1.0 通过」）✓；`check_master_unit_guard.py:285-299/:351/:391/:405` accept/自测用例 ✓；`cosmetic_corrector.cpp:158-167` interpolate_pixels 与 bad_mask 1=bad ✓；`defaults.json:19-24` `calibration.dark_light_exposure_tolerance = 5 s` ✓；`ac_api.cpp:101-138` ac_generate_master_bias/AC_ERR_PARAM ✓；证据 JSON `run/SCI-FIX-SEMANTICS-01/evidence/dark_tolerance_criterion.json` 与 T4 实测文件 `testdata/Victory_Nebula_T4_Flying_Dutchman/lights/*20250204@035646-180S-Lum.fts` 均存在 ✓。 |
| 绿-7 | ④ | ASTROMETRY 关键推导锚：`ipv_wcs.cpp:159-162` CRPIX=cx+0.5、:276-280 CD=trans/3600、:343-344/:356-360 奇异拒绝与逆、:504-513 AP/BP、:942-946 `out.x = u + wcs.crpix[0]`（注释 u = x − crpix）✓；`p3_wcs.cpp:49/:52-53/:519-520` kFitsPixelOrigin/fits_pixel_1based ✓；`closure_metric.py:223` = `sky = w.all_pix2world(xy, 0)`（origin=0 参数承载「p=x+1」语义，表述相符）✓；极区 85° 条款（:64/:150）锚 `gaia_client.h:11` 注释 + `gaia_client.c:1061-1063`（cos_dec<0.01 ⇒ 返回 1 不剪枝，对应 |dec|≈85.4°）+ `d_ra*cos_dec` 缩放 ✓ 属地正确。 |
| 绿-8 | ③ | C/C45/θ_q 条款跨文档对照：ASTROMETRY:28/:64/:150/:180/:251 ↔ PLATESOLVE.md:23/:51 ↔ GAIA_QUERY.md:113/:126/:151 ↔ 源码 gaia_client.c:1096/:1097/:1124/:1108——**四处文档 + 源码同口径**（45° 极冠边界、θ_q+ρ>90° 不剪枝、C45=π/(2√2)、|dec|>85° 保守）。（越域的只有 DRIZZLE 侧，见红-2。） |
| 绿-9 | ① | DRIZZLE 科学面按台账已裁口径相符：D-03 双锚（§5 式 w_jp=a_jp/A_drop、§7 方差 sumVarNum/N_p²）与 F&H §2 一致（上轮 PASS 不重裁）；D-01：spherical_overlap.cpp:38 hp_res 注释/:40 因子 1.25/:42 常数、drizzle_engine:1031 EW 1.516、:1487 赤道 1.532/半长 0.766、sup≈1.0415 覆盖关系几何自洽（:108 R-3 已修 PASS）；§9 arc-chord `<1e-6·hp_res ≈3e-6″` 与 :578-579 源码注释逐字相符（NSIDE=65536 复算 3.22e-6″ ✓）；§11 非退化负例（真值无重叠 ⇒ patch=0/n_used=0/双 NaN、d=0 ⇒ σ=∞、QFP 侧 Q=0 ⇒ discard）与 :1535-1550/:654-657 源码一致。 |
| 绿-10 | ①+④ | CONTROL 科学面与数值：γ=2 恒等式与 ulp 表上轮复算 PASS 不重裁；§8a/§8b 数值全部对上正本——斜率 −0.4879=`G2_large_sky_slope_def=-0.4879107345`（b1:546）、×3419=3419.3929、×8040=8040.052、δB*=2.7805（b1）、Var 1275/1260（b4）、+0.0297/0.2626/0.4444/0.0530（b3）、E=7.17 与「对抗打乱场门仍绿」=`DOC_CORRECTIONS.md:65` 逐句相符、`DEGENERATE_flat_field_ranking_never_true` 门名实存（b3/exp04_e1/exp04_e4/exp05_e5）；D1/D2 双计偏差 +12.8%/+34.0% 与 1.44/√N→1.44/ln10/√N 对 `DOC_CORRECTIONS`/`AUDIT_KEY_RESULTS.json` 相符（:203/:204 不冲突，前判确认）。`random.tif` 引用经复核**不存在于本文件**（grep 零命中，前候选撤销）。 |

---

## 四、已查无问题面（per-face 覆盖说明）

**①科学性（无其他问题；2 项已登记 = 红-1、红-4）**——六文件全部公式/常数/量纲/适用域逐条对照 D-01…D-11 台账与源码：DRIZZLE §5/§7 面亮度与方差式、§9 精度策略、hp_res/pixfrac 常数；CONTROL §2b 分离先验三条、§4 定权式、§8a/§8b 全数值（绿-10）、1.44/√N 单位换算；INTEGRATION §5 归约/状态机（除 :55 形式化行）、§7 不变量；ACR §9 容差表（上轮复算 PASS）与 §5 设备域差异 (a)(b)(c)；CALIBRATION §3 ADU 域、§5 双分支公式、§6a 两面分离。**除红-1、红-4 外未见新的科学性错误**；D 系列裁决、默认容差、冻结定义均未被重开。

**②行文逻辑（无其他问题；3 项已登记 = 红-3、红-4、黄-1）**——六文件全文精读（含被截断段补读）：论证链、单文档自洽、订正注记新旧对照、UNRESOLVED 登记态逐处核查。DRIZZLE R-3 订正注记与正文一致（PASS 表已验）；CONTROL:201/:243 订正注记与 A-P4-06 一致；CALIBRATION:468 与 CONTROL:256-259（ε 未冻结）均为**显式登记的未决状态**、非「以未决充结论」，不计问题；§11/§15 非退化负例（真值无效应⇒归零/判红）在 ACR §11、INTEGRATION §11、CALIBRATION §11、CONTROL §2b 均逐臂/逐对照书写。**除已登记 3 项外无断裂**。

**③跨文档冲突（无其他问题；1 项已登记 = 红-2）**——逐对对照：DRIZZLE ↔ DRIZZLE_GEOMETRY（D-01 sup/1.25、D-03 双锚、边界枚举「RA 跨 0/极区/face 边界」同口径）；CALIBRATION ↔ CALIBRATION_ALGORITHMS（§5 双分支公式 :37-39、F3.1/F3.2 K 直通、EXPTIME fail-closed、dark_optimization 声明）；INTEGRATION ↔ INTEGRATION_ALGORITHMS（状态码表/资格门/reducer）；CONTROL ↔ docs/ASTROCS_DESIGN §3.1（权重两步链、w=SNR²/F_ref²、无权重模式、帧级与稀疏层独立、单向导出约束逐句同构）；ASTROMETRY ↔ GAIA_QUERY/PLATESOLVE/GATES（C/C45、85°、1e-4 px 分层门、G-P1-WCS-BRIDGE-GLOBAL 锚 :188）；ACR ↔ docs/ASTROCS_DESIGN §1.3/§2/§8（ACR 生产不可达、纯 CPU）。**唯一冲突 = 红-2**；CALIBRATION_ALGORITHMS 无 §6a 对应条款属「无对应承接」，已并入红-1 证据。

**④幻觉与锚（无其他问题；3 项已登记 = 红-2、黄-2、黄-3）**——(a) **file:line 锚**：六文件全部锚逐条开验（本切片 ≈130 处，含 CMakeLists/pytest/JSON/头文件锚），相符清单见绿-3…绿-7，漂移 14 组已并入黄-2，指错模块 1 组为红-2，路由缺口 1 组为黄-3；(b) **文献**：11 条 DOI/PDF/节号抽验全过（绿-1/绿-2，其余上轮已核），无幻觉引文；(c) **「doc says exists, code not wired」专项**：drizzle 极区机制（红-2）、`boundary_fallback`（仅测试标签，已并入红-2 佐证）、`ε` 未登记（文档已自 declare 为未决 ✓）、weight_mode token 已删（integrate.h:11-13 与 stage2_common:434-440 双侧同步 ✓）、ACR 恒 false（源码同断言 ✓）、60 字节夹具前置（源码同式 ✓）、`frame_count>64`（源码同判 ✓）——**仅红-2、黄-3 构成缺口**。

> 巡检边界：未执行构建/测试（只读纪律）；`实验/` 与 `run/` 仅做存在性与数值对数，未复算实验；本报告未改动任何被检文件。
