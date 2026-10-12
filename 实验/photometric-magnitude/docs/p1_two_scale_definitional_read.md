# 两套帧级标度实现的定义性通读（`k_photo` vs `zero_point_mag`）

> 对象：`lib/algorithms/photometry/cpp/src/star_matcher.cpp` 的 `k_photo`（路线一）与
> `lib/algorithms/photometry/cpp/src/frame_photometry_fit.cpp` 的 `zero_point_mag`（路线二）。
> 性质：**只读**。本文只做逐行通读、代数推导与判别式设计；不改代码、不改其它文档、不做 git 操作。
> 依据：`docs/science/photometry/PHOTOMETRY.md`（SCI-PHOT-001）、`docs/science/algorithms/normalize/PHOTOMETRIC_FIT.md`（ALG-PHOT-001..002）、
> 生产源码与 `实验/photometric-magnitude/docs/fsyn_convention.md`。
>
> **工作树状态（本读取时刻，只读观察，非本任务改动）**：`frame_photometry_fit.cpp` 正被
> `PER-STAR-EXPORT-01`（逐星导出 `f_instr/gaia_mag/residual`）与**零点改名**
> （`zero_point_* → synthetic_colour_*`）改动中，行号较产物生成时已下移；本文行号取**当前**
> 工作树，若漂移以符号名为准。改名后的头注释（`frame_photometry_fit.h:130-145`）已明确：
> 该量"本质是**该天区样本的中位颜色项**（通带/SED 一致性诊断），与帧无关"，
> "**帧的测光标度是 `k_photo`**；帧的星等零点由 k_photo 换算即得"，并点名
> "把它与 `k_photo` 当成'同一件事的两条独立实现'去比对不一致"是**误诊**——
> 这与本文 §2/§6 的推导**独立一致**。

**结论先行（供回报用）**

1. 两路估计的是**不同的代数量**：路线一估计**帧透射率的倒数**（把 ADU 搬到合成通量刻度的乘性标度），
   路线二估计**通带绝对零点**（把目标通带合成通量搬到 Gaia G 星等刻度的加性常数）。
2. 由各自定义推出的**精确关系**必须含"帧仪器零点" `W`：
   `ZMAG = W + 2.5·log10 k_photo`，其中 `W = median_match(G + 2.5·log10 F_instr)`。
   这条关系里的 `W` 是**帧相关量**，产物里没有它。
3. 因此 **`log10 k_photo` 与 `ZMAG` 之间不存在跨帧恒定的组合**（`k_photo` 是帧量，`ZMAG` 是帧无关量）。
   派单引用的 `log10 C' = log10 k_photo + 0.4·ZMAG`（`p1fix_process.md` 附录 N）**不是不变量**，
   它等于 `log10(τ⁻¹·C'_P)`（`τ` 为帧透射率、`C'_P` 为通带归一常数），逐帧随 `τ` 变。
4. 16 帧实测的 11.87% 离散，其量级与"帧透射率跨帧变化"一致（同帧滤光片/曝光全为 `300S-Red`，
   差异只能来自大气/水汽/薄云与读出条件）；且**约一半方差来自单帧** `M4_T2-20251228_044805`
   （剔除后 `SD(log10 C')` 0.04716 → 0.02781 dex）。这**不构成**"两路实现不一致"的证据。

---

## 1 R1.1 两路估计量对照

| 维度 | 路线一 `k_photo` | 路线二 `zero_point_mag` |
|---|---|---|
| 入口 | `StarMatcher::cleanAndScale`（`star_matcher.cpp:389`） | `fit_frame_photometry` 的零点块（`frame_photometry_fit.cpp:252-332`） |
| 星样本来源 | Gaia 锥形搜索 → `_qf` 内部**自适应星等**查询（`pc_api.cpp:709-739`）→ WCS 投影 → 与 PSF 有效星**双向 KD 最近邻唯一配对** | **另起一次** Gaia 锥形搜索（`frame_photometry_fit.cpp:267-269`） |
| 星样本定义域 | `psf_status==0`（`star_matcher.cpp:233`）；`F_instr>0 ∧ F_syn>0`（`:451`）；匹配半径 2.0 px（`pc_api.cpp:427`）；质量/饱和位剔除（`:419-435`，`PC_QF_SATURATED|PC_QF_HAS_SATURATED`） | `n_zp` 内**全部**锥形星，仅要求 `F_syn>0` 有限（`frame_photometry_fit.cpp:297`）与 `magG` 有限（`:298`）；**无检出、无 PSF、无质量过滤** |
| 星等域 | `mag_min=6.0`；`mag_max` **不消费**（冻结签名 `pc_api.cpp:216/638/900`；自适应 `{12,13,14,15,16}` 直到 `n_gaia≥2000`，`pc_api.cpp:709-739`）。**星等一致性预过滤**：`delta=mag_inst−magG`，`|delta−median(delta)|≤3.0 mag`（`star_matcher.cpp:456/491`；`pc_api.cpp:435`） | `[mag_min, mag_max] = [6.0, 16.0]`（`frame_photometry_fit.h:69-70`；调用方 `module_adapters.cpp:6038-6068` **不覆盖**这两键） |
| FOV 半径 | 与路线二**同一个** `fov_radius_deg`（`frame_photometry_fit.cpp:168-175`，经 `_qf` 入参 `radius_deg` 下去） | 同左（`frame_photometry_fit.cpp:268`） |
| 星表 / 参考 | Gaia DR3 XP（`gaia_client_cone_search_with_spectrum`）；通带曲线 = `filters.json` + `qe_curves.json`（同一次 `load_curve`） | 同左（同 client、同曲线对象、同 `prepare_filter_cache`） |
| 加权 | Tukey biweight IRLS：`c=4.685`、`u=(r−location)/(cS)`、`w=(1−u²)²(|u|<1) else 0`（`star_matcher.cpp:20/555-568`）；初值 `location=median(r)`、`S=MAD(r)/0.6744897501960817`（`:537/:542`）；50 次上限、1e-6 收敛（`:22-24`） | **无加权**：纯算术中位数（`frame_photometry_fit.cpp:306-310`） |
| 内点判据 | IRLS 固定点处 `|u|<1` 的收入 inliers（`star_matcher.cpp:596-607`）；`|r_consistent|≥3` 才迭代（`:515`），`|r_inliers|≥2` 才算 `sigma_residual`（`:613`） | 无（全部有效 `z_i` 入中位数；`n_zp≥3` 才置 `zero_point_valid=true`，`frame_photometry_fit.cpp:305/322`） |
| 逐星代数量 | `r_i = log10(F_instr,i / F_syn,i)`（`star_matcher.cpp:452`，原句） | `z_i = magG,i + 2.5·log10(F_syn,i)`（`frame_photometry_fit.cpp:299`，原句） |
| 帧级估计量 | `location = IRLS-location({r_i})`；`k_photo = 10^(−location)`（`star_matcher.cpp:537/586`）；`I_cal = I·k_photo`（`star_matcher.h:57`） | `zero_point_mag = median({z_i})`（`frame_photometry_fit.cpp:308-310/319`）；散布 `zero_point_scatter_mag = 1.482602218505602·MAD(z)`（`:321`） |
| 附带诊断 | `sigma_residual_dex = MAD(r_inliers)/0.6744897501960817`（`:618`） = 通量比散度 | `zero_point_scatter_mag`（mag） = 星族 SED/颜色散度 |

**波段核查（R1.1 必查项）**

- `magG` **是 Gaia G 星等**：结构体字段 `double magG;`（`gaia_client.h:60`），路线一注释亦写 `gaia_mag; // Gaia G 星等`（`star_matcher.h:31`），填充处 `gaia_mag[i] = spec_stars[i].magG;`（`pc_api.cpp:818`）。
- `F_syn` **是目标通带（本帧 filter × QE）的绝对合成通量，不含 `10^(−0.4G)`**：
  `F_syn = ∫F_λ(λ)·T(λ)·Q(λ)·λ dλ`（`spectrum_integrator.h:13-16`，权威口径块），实现 `compute_f_syn_cached_xpsd`（`:128`），
  逐星量化参数解码 `F_λ = byte·flux_mul + flux_min`（`:120`）；两条路线用的是**同一个** `compute_f_syn_cached_xpsd`（路线一 `pc_api.cpp:797`，路线二 `frame_photometry_fit.cpp:287`）。
- 因而 `z = magG + 2.5·log10(F_syn)` 里两个量**确属不同波带**（G 波段 vs 目标波段）——这是**有意设计**，它把"目标通带合成通量"标定到"该星的 Gaia G 星等"，逐星的颜色项即 `zero_point_scatter_mag`（不是实现错误，但确实是两路之间的口径差，见 §3）。
- **没有"两路各自不同的参考"**：两路同星表、同曲线、同 `F_syn` 函数；差别在**星样本与估计量**，不在参考。

---

## 2 R1.2 精确关系推导

### 2.1 记号

对同一颗星 `i`：

| 符号 | 含义 | 来源/单位 |
|---|---|---|
| `g_i` | Gaia G 星等 | `magG`，mag |
| `s_i` | 目标通带合成通量 `F_syn,i` | `compute_f_syn_cached_xpsd`，W·m⁻²·nm |
| `f_i` | 仪器 PSF 域解析通量 `F_instr,i` | `2πA·sx·sy/3`，ADU |
| `r_i` | `log10(f_i/s_i)` | 路线一逐星量 |
| `z_i` | `g_i + 2.5·log10(s_i)` | 路线二逐星量 |
| `L` | `r` 的路线一稳健位置（Tukey 加权均值，IRLS 固定点） | dex |
| `k` | `10^(−L)` | 路线一产物 `k_photo` |
| `Z` | `z` 的路线二中位数 | 路线二产物 `zero_point_mag` (mag) |

### 2.2 逐星恒等式（两路共享）

`z_i + 2.5·r_i = g_i + 2.5·log10(s_i) + 2.5·log10(f_i) − 2.5·log10(s_i) = g_i + 2.5·log10(f_i)`。

令 `W_i := g_i + 2.5·log10(f_i)`（**仪器零点**，mag；与路线一的 `delta_i = −W_i` 同源，`star_matcher.cpp:456`），则

```text
z_i + 2.5·r_i = W_i          （逐星精确）
```

### 2.3 帧级关系（需同星样本、同一线性估计量）

若两路用**同一样本 S**且估计量同为（加权）均值，则

```text
Z + 2.5·L = W            ⇒    Z = W + 2.5·log10 k        （k = 10^(−L)）
```

即**精确关系**为

```text
zero_point_mag = median_match( G + 2.5·log10 F_instr ) + 2.5·log10 k_photo
```

其中 `median_match(·)` 定义在**路线一的匹配星样本**上。等价地
`W = zero_point_mag − 2.5·log10 k_photo`。

生产实现里 `Z` 是**另一批星（锥形全样本 S2）的中位数**、`L` 是**匹配样本 S1 的 Tukey 加权均值**，
中位数不与线性估计量可交换，且 `S1≠S2` ⇒ 上式只是**近似**，偏差由样本差与估计量差贡献；
但**帧相关项 `W` 与严格关系这两点不受影响**。

### 2.4 物理模型下的读数

设帧透射率 `τ`（把合成通带通量搬成 ADU 的乘性因子）：`f_i = τ·s_i`（理想线性、大气/光圈/增益都折进 `τ`）⇒

```text
r_i = log10(τ),     L = log10(τ),     k = 1/τ   ⇒   log10 k = −log10 τ
```

设通带归一常数 `C'_P`（把 Gaia G 搬到目标通带合成通量的乘性常数，含星族中位颜色项）：
`s_i = C'_P·10^(−0.4·g_i)·10^(−0.4·c_i)`（`c_i` = 逐星颜色项）⇒

```text
z_i = g_i + 2.5·log10(s_i) = 2.5·log10 C'_P − c_i
Z  = 2.5·log10 C'_P − median(c_i)        （帧无关、天区/星族相关）
```

于是：

| 量 | 精确表达 | 帧依赖 |
|---|---|---|
| `Z = zero_point_mag` | `2.5·log10 C'_P − median(c)` | **无**（只依赖 filter/QE/星族） |
| `log10 k_photo` | `−log10 τ` | **有**（把增益/曝光/大气全吸收，`PHOTOMETRY.md` §1） |
| `W`（仪器零点） | `2.5·log10(C'_P/τ)` | 有（含 `τ`） |
| `log10 k + 0.4·Z`（附录 N 的 `log10 C'`） | `−log10 τ + log10 C'_P` | **有**（含 `−log10 τ`） |

**⇒ 结论：`log10 k_photo` 与 `ZMAG` 之间不存在跨帧恒定组合**。`α·log10 k + β·ZMAG` 只有当
`α = 0`（即 `ZMAG` 本身）才帧无关。`log10 k + 0.4·ZMAG` 之所以被当成常数，是把
"仪器零点 `W`"误当成了"通带常数 `C'_P`"。

**与派单的关系式的分歧（必须报出）**：`p1fix_process.md` §N 写作
`log10 C' = log10 k_photo + 0.4·ZMAG`，其推导 `ZMAG = 2.5·log10 C' + 2.5·median(r)`。
按 §2.2 的**逐星恒等式**，正确的帧级关系是 `ZMAG = W + 2.5·log10 k_photo`（即 `W` 在当前
`r` 上带**正号**）；若把 `C'` 定义为**仪器**零点（`F_instr = C'·10^(−0.4G)`），则
`W = 2.5·log10 C'_I`，于是

```text
log10 C'_I = 0.4·ZMAG − log10 k_photo        （符号与派单相反）
```

若把 `C'` 定义为**通带**常数（`F_syn = C'_P·10^(−0.4G)`），则

```text
log10 C'_P = 0.4·ZMAG                        （与 k 无关）
```

两种自洽读法都不给出 `log10 k + 0.4·ZMAG`。**这不是某一代码路径自相矛盾**（两路各自的实现
与其注释一致），而是**测试关系式**的代数错误。

---

## 3 R1.3 差异清单与先验判断（能否解释 11.87% 的跨帧离散）

先记住：`ZMAG` 在**同一天区组内几乎不变**（实测同组逐帧差 ≤5e-4 mag），跨组差最大 0.059 mag；
而 `log10 C'` 的 11.87% 几乎全部是 `log10 k_photo` 的离散（实测 `SD(log10 k)=0.0478`、
`SD(log10 C')=0.0472`）。故"能否解释"要问的是**能否解释 k 的跨帧离散**。

| # | 差异项 | 内容（锚） | 先验判断 |
|---|---|---|---|
| D-1 | **样本域：星等上限** | 路线一自适应 `{12..16}`（`pc_api.cpp:709-739`）；路线二固定 `16.0`（`frame_photometry_fit.h:70`）。同一帧两路的上限可能不同，且路线一的**逐帧上限可能随星数变化** | **可能贡献一部分**：加入更暗星改变 `k` 的 PSF 通量偏差/选择；但 `k` 是逐星比值（理想下与样本无关），且 IRLS 稳健，故不解释主要量 |
| D-2 | **样本域：检出/PSF 要求** | 路线一仅保留 PSF 有效星且双向 2px 唯一配对（`star_matcher.cpp:233/296-330`）；路线二全锥形星（`frame_photometry_fit.cpp:267-303`）。`n_zp−n_matched ≈ 980–1591` | **不解释 k 的离散**：样本大小与 `log10 k` 相关实测 R²≈0.025（附录 N.2）。它会改变 `Z` 的**水平**（星族颜色中位数），是组间偏移而非帧间散度 |
| D-3 | **加权/估计量** | 路线一 Tukey-IRLS 加权均值（`star_matcher.cpp:555-573`）；路线二纯中位数（`frame_photometry_fit.cpp:306-310`） | **不解释**：逐星 `r_i` 在理想下同为 `log10 τ`，两种稳健估计量的差是二阶小量（≪0.05 dex） |
| D-4 | **波带/参考** | `z` 用 Gaia G（`magG`）对目标通带合成通量（`frame_photometry_fit.cpp:299`）；`r` 是**同星同通带**的实测/合成比（`star_matcher.cpp:452`） | **不解释 k 的离散**：路线一逐星不含跨带颜色项；路线二的跨带项进的是 `median_i(G−R)_i + C_R`（`frame_photometry_fit.h:137`），是 `Z` 的**水平**（组间/星族），不是 k 的帧间散度。**这是两路之间的真实口径差，但对本散度只贡献一个近常数偏移** |
| D-5 | **归一化 / `F_syn` 口径** | 两路同一 `compute_f_syn_cached_xpsd`，已确认 `F_syn` **不含** `10^(−0.4G)`（`spectrum_integrator.h:13-16/121`；`fsyn_convention.md` §1） | **不解释**：不存在"一路乘了 `10^(−0.4G)`、另一路没乘"的错配。若真有该错配，`dlog10(ratio)/dG = 0.400`，会表现为 `r` 的**逐星 G 相关散度**（0.459 dex，`spectrum_integrator.h:29-32`），与实测 `sigma_residual_dex≈0.02–0.04 dex` 不符 |
| D-6 | **离群剔除** | 路线一有饱和/质量位剔除 + 3 mag 预过滤 + IRLS（`star_matcher.cpp:419-435/491/596-607`）；路线二无 | **可能贡献异常帧**：被剔除样本随帧不同（如异常帧 `n_saturated=39`、`n_detected=878`，为全组最少）；但正常帧 `sigma_residual_dex` 稳定，说明正常帧剔除面一致 |
| D-7 | **单位** | `F_instr`=ADU；`F_syn`=W·m⁻²·nm；`Z`=mag | **不解释**：两路换算自洽（`r` 无量纲 dex，`z` 为 mag） |
| D-8 | **同一星表** | 都是 Gaia DR3 XP（`gaia_client_cone_search_with_spectrum`） | **不解释** |
| D-9 | **大气/消光/透射率** | 两路都**不含**消光项；`τ` 全被路线一的 `k` 吸收（`PHOTOMETRY.md` §1「标定因子的绝对值无物理意义…吸收增益、口径、曝光」） | **最可能解释 k 的帧间离散**（见 §4：`C'` 的表达式含 `−log10 τ`）。16 帧同为 `300S-Red`、同望远镜 T2 ⇒ 曝光/增益不是散度来源，**大气/水汽/薄云与读出条件**才是 |

**先验排序**：D-9（帧透射率，按定义进入 `k`）> D-6/D-1（异常帧的样本/质量退化）≫ D-2/D-4（只改水平）> D-3/D-5/D-7/D-8（不贡献）。

---

## 4 R1.4 可检验的判别式

对每一项差异，若它是元凶应当观察到：

| 判别式 | 操作 | 若为元凶的预期 | 现状读数 / 卡点 |
|---|---|---|---|
| **P1 不变性检验**（对 D-9） | 在**同一天区组内**看 `log10 k`：大气/透射率若稳定，组内离散应显著小于组间 | 组内 ≪ 组间 | 实测组内 `SD(log10 k)`：M1 0.0154、M2 0.0064、M3 0.0108、M4 0.0920、M5 0.0190、M6 0.0379（dex）。**组内不小**，尤其 M4/M6；提示组内也有帧级差异（大气/异常帧） |
| **P2 剔除异常帧**（对 D-6） | 去掉 `M4_T2-20251228_044805` 重算 | 若为单帧退化，SD 应大幅塌 | 实测 `SD(log10 C')` 0.04716 → **0.02781 dex**（−41%）；剩余量仍非零 |
| **P3 同星集同加权**（对 D-1/D-2/D-3） | 两路限制到**同一星集**（同一 `mag_max`、同一 PSF 匹配域）并统一估计量，重算 `Z` 与 `k` | 若样本/估计量是元凶，对齐后 `Z_route1 ↔ Z_route2` 的散度应塌到 ≪0.01 dex | **当前不可构造**：现有产物的逐帧 JSON 没有逐星 `G`/`F_instr`/`r_dex`（`p1fix_process.md` §N.4；`p1_flux.json` 无 `f_syn/r_dex`）。**通道正在落地**（`PER-STAR-EXPORT-01`：`photometric_calib.h:55-67` 的 `PcMatchRecord.f_instr/gaia_mag/residual`），接上产物后本条即可执行 |
| **P4 颜色项分解**（对 D-4） | 把 `Z` 与两路"隐含零点"之差（`ZMAG − 2.5·log10 k_photo = W`）按**天区组的星族颜色**（如中位 `BP−RP`）作图 | 若跨带颜色项是元凶，组间偏移应随星族颜色线性变（斜率 ≈ −1），而**组内散度不应随它变** | 实测 `ZMAG` 组间差最大 0.059 mag（M4/M5 最负），与 M42 各指向的星族/消光差异相符；但无中位颜色读数 ⇒ **需补星族颜色统计** |
| **P5 常数性判据**（对 §2 分歧） | 把"应跨帧恒定的常数"改成 `ZMAG`（`0.4·ZMAG`），而不是 `log10 k + 0.4·ZMAG` | 若两路都正确且同参考，`ZMAG` 的**组内**离散应 ≈0（组间因星族可不同） | 实测组内 `ZMAG` 差 ≤5e-4 mag（M1/M2/M3/M4/M5/M6 各自组内几乎逐位相同）⇒ **符合**"`ZMAG` 是帧无关常数"的预期；这反证附录 N 的组合式不是不变量 |
| **P6 仪器零点通道**（对 D-9 的正面证据） | 用路线一内部量 `W = median_match(G+2.5 log10 F_instr) = ZMAG − 2.5 log10 k` 对大气代理（airmass、天光、消光曲线）作图 | 若 `W` 随 airmass 线性变，则 k 的离散就是大气 | **卡点**：`W` 未落盘；`p1_sources.json` 的 `background/noise_sigma` 与 `log10 k` 相关性弱（\|r\|≤0.33，n=16），不足以单独作证；需 airmass（帧头 `AIRMASS`）与 `W` 落盘 |
| **P7 逐星残差散度**（对 D-5） | 看 `sigma_residual_dex` 是否随帧异常 | 若 `F_syn` 口径错配，`sigma_residual_dex` 会很大且与 G 相关 | 实测 0.021–0.042 dex，无异常放大 ⇒ 排除归一化错配 |

---

## 5 与派单关系的分歧（优先报告）

1. **`z = magG + 2.5·log10(F_syn)` 的两量确属不同波带**（G vs 目标通带），但这是**设计**而非错误：
   它定义"合成 G 星等"`m = ZMAG − 2.5·log10(F_T)`，颜色项即 `zero_point_scatter_mag`；
   当前工作树的头注释已把它改名为天区样本的**中位颜色项**（`frame_photometry_fit.h:130-145`）。
   **行号证据**：`frame_photometry_fit.cpp:257`（"取值 = median_i(magG_i + 2.5*log10 F_syn,i)"）、
   `:299`（`z = zp_stars[i].magG + 2.5*log10(fsyn)`）、`gaia_client.h:60`（`magG`）、
   `spectrum_integrator.h:13-16`（`F_syn` 目标通带、不含 G 因子）。
2. **派单的 `log10 C' = log10 k + 0.4·ZMAG` 不是不变量**，正确关系见 §2.3–§2.4；
   这不是"某一路定义自相矛盾"，而是**测试式**的代数错误（符号与所取 `C'` 定义都不对应）。
3. 因此附录 N.3 的判定"两路在同参考下仍不一致"**证据不足**；附录 N.4 的"无法构造同星集对照"
   仍然成立，且**恰恰是**唯一能判定实现一致性的检验。

---

## 6 最可能的元凶

**不是两路实现的定义性不一致，而是"被监测的量不是不变量"。** 逐条理由：

1. `log10 C'` 的解析式 = `log10(τ⁻¹·C'_P)`（§2.4），含帧透射率 `τ` ⇒ 按定义逐帧变。
2. 16 帧同为 `300S-Red`、同望远镜 T2（帧名 `…-300S-Red`），曝光/增益不是散度源；
   差异只能来自大气/水汽/薄云等透射率因素。
3. 实测 `ZMAG` 组内几乎逐位相同（≤5e-4 mag），说明路线二的零点**正确地**帧无关；
   散度全在 `log10 k` 一侧。
4. 单帧 `M4_T2-20251228_044805` 贡献约 41% 的方差，且该帧同时是路线一拟合质量最差、
   源数最少、背景最高的一帧（`sigma_resid=0.0373`、`kappa=2.05`、`n_detected=878`、
   `background=211`）⇒ 即使有实现/标度缺陷，也更可能在**路线一单帧的 k**，而不是"两路不一致"。

**若必须给一个可证伪的实现级嫌疑**：路线一在**低星数/高背景帧**（`M4 20251228`）上的
`k` 偏高（+57%）与 `sigma_residual_dex`、`kappa`、源数、背景同时异常，提示该帧的
PSF 域通量或匹配样本有偏；但这**尚未**与路线二对齐验证（缺逐星量，P3 卡点）。

**明确的不确定项**：`τ` 的物理分解（大气 vs 仪器）无 airmass/`W` 读数；`Z` 的组间差
（0.059 mag）是否纯星族颜色项也未分解。两者都需先落逐星 `f_syn/r_dex` 与帧 `AIRMASS`。

---

## 7 引用锚

- 路线一：`lib/algorithms/photometry/cpp/src/star_matcher.cpp:449-461`（`r`/`delta`）、`:483-507`（星等预过滤）、`:510-533`（星数门）、`:535-583`（IRLS/Tukey）、`:585-624`（`scale`/`sigma_residual`）；`star_matcher.h:31/54-58/96-104`；`pc_api.cpp:216/427/435/709-739/797-819`。
- 路线二：`lib/algorithms/photometry/cpp/src/frame_photometry_fit.cpp:168-175`（FOV 半径）、`:252-332`（零点块）、`:267-269`（锥形搜索）、`:287-303`（`F_syn`/`z`）、`:305-327`（中位/MAD）；`frame_photometry_fit.h:69-70/130-145`；`spectrum_integrator.h:11-32/117-131`；`lib/infrastructure/gaia_xpsd_client/src/gaia_client.h:57-66`。
- 并发改动（只读观察）：`lib/algorithms/photometry/cpp/include/photometric_calib.h:55-67`（`PcMatchRecord` 的逐星 `f_instr/gaia_mag/residual`，`PER-STAR-EXPORT-01`）、`lib/infrastructure/scheduler/src/module_adapters.cpp:5711-5714/6468-6472`（`synthetic_colour_*` 改名面）。
- 调用与落盘：`lib/infrastructure/scheduler/src/module_adapters.cpp:6038-6094`（`FramePhotFitRequest`，不覆盖 `mag_min/mag_max`）、`:6455-6478`/`:6537-6555`（`photscale_detail`/`photscale_fit` 的零点键）。
- 实测数据：`run/PHOT-FIX/p1_rerun/t2/p1_phot.json`（16 帧 `photscale_fit`/`photscale_detail`）、`run/PHOT-FIX/p1_rerun/t2/p1_sources.json`。
- 既有过程文档：`实验/photometric-magnitude/docs/p1fix_process.md` 附录 N、`实验/photometric-magnitude/docs/fsyn_convention.md` §1–§2。
- 科学正本：`docs/science/photometry/PHOTOMETRY.md`（SCI-PHOT-001，`F_syn`/`scale`/判据）、`docs/science/algorithms/normalize/PHOTOMETRIC_FIT.md`（ALG-PHOT-001..002）。
