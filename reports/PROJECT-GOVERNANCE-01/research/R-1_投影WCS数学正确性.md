# R-1 投影/WCS 数学正确性 —— 决策建议书

> **任务**：R-1（研究线，不是整改线）　**覆盖**：D-1 / D-2 / D-3 / D-4 / F-5 + P3-002 的 8 条（M1a-A-007/008/009、M7-A-114/117/129/209、M7-F-201）
> **BASE**：`e9b31285af6dd6c9bb2636f9e4d707a433f8abd7`（main）
> **纪律**：零 git 写；仓库文件零改动（逐文件核对见 §6.6）；不采信 `问题扫描` 的 `fix_state/verified_state`，一切结论以本报告内的复跑输出为准。
> **外部标准**：astropy 7.0.1（numpy 2.2.4，Python 3.13.5，g++ 14.2.0）作为 **WCSLIB 可执行标准**；Calabretta & Greisen 2002（Paper II）/ Greisen & Calabretta 2002（Paper I）全文在线获取被站点反爬拦截（HTTP 403，见 §2.0），故按任务授权改以 astropy 实现为可执行标准，并逐条注明所依据的 Paper I/II 语义。

## 结论速览（唯一推荐结论）

| 议题 | 唯一推荐结论 | 置信度 | 影响面 |
|---|---|---|---|
| **D-1** | **成立，且是 P0 缺陷**：v1 与 v2 两套 registry 的 CAR/AIT 让 CRVAL2 不进映射，CRPIX 处 world ≠ CRVAL（偏差恰 = \|CRVAL2\|，实测 30.000000°）。**推荐按 Paper II §2.2 三 Euler 角把 CRVAL2 纳入映射**（已给出并实测验证的最小补丁：+45 行 / 改 5 处，与 astropy 全量对拍 ≤7e-13°）；**不推荐**「把 CAR/AIT 收窄为 CRVAL2=0」 | 高 | ALG §15.2/§15.3/§15.5、v2 registry（版本 2→3）、v1 退场、`v6_p3_proj_wcs_oracle` 用例集、GAP-011 |
| **D-2** | **成立**：v1/v2 的 AIT 域判据 `A<2` 应为 `A≤1`（`A = X²/4+Y²`（v1 单位）= `xp²/4+yp²`（v2 归一））；实测 v2 接受到 X=229.125°（标准 162.0569°），多接受 67.1° 的**折叠环带**（v1 多接受 47.5°）。ALG:424 的「椭圆域边界 X²/4+Y²=2」是错值 | 高 | ALG §15.3/§15.4/§15.5、registry 两处（`ait_pix2world` / `projection_margin`）、AIT FOV claim |
| **D-3** | **成立**：CAR 的「无投影奇点」为假——native 极 θ=±90° 是一条**整行塌缩**（Ω=0、RA 无定义），而实现守卫写 `\|θ\|>90 → 拒`，恰好放行 θ=±90。**推荐 fail-closed 收紧为 `\|θ\|≥90 → 拒`**，并把 ALG 奇点声明改写为「native 极行」；θ₀ 记法按 Paper II 修正（CAR/AIT 参考点 θ₀=0，非常数 90） | 高 | ALG §15.1/§15.3/§15.5、`car_pix2world/car_world2pix`、CAR FOV claim（≤180°→<180°） |
| **D-4** | **成立，且是「以高权威为准」而非「两权威打架」**：DESIGN §5.3（①）与 `docs/plugins/algorithms_phase3/14_projection.md`（⑥）**都写八投影**；只有 ALG §15（推导面）写四投影、且其唯一依据「宪章 ASTROCS-CONSTITUTION-001 §18.1」**在现行权威链中不存在**（§1.4 证据）。**推荐以八投影为准**，registry 升 v3；SCI §9a-3 的「alpha 仅 TAN」是**会话层 alpha 收窄**，不与 registry 八投影冲突，只需显式加一句「registry 面已注册、alpha 会话未接线」 | 高 | ALG §15 全节、registry v3、`docs/plugins/.../14_projection.md`（悬空 schema 引用）、SCI §9a-3 补注、`p3_wcs_validate_request` 不变 |
| **F-5** | **成立**：树内 astropy Oracle（ctest #386）**实现独立但用例集盲**——CAR/AIT 全部用例 dec0=0（`p3_proj_wcs_oracle.py:37-40`），对 D-1/D-2 零区分力；实测把 4 条 dec0=±30 用例加进去后**现状必红（30.000°）**。另有 `p3_proj_legacy_deviation.py` 把 v1 的 CAR 反号 / AIT 缺 √2 **冻结成「预期偏差」并 rc=0 通过**；ALG §15.6 的「独立往返 Oracle」只验自洽（现状 30° 错映射下 roundtrip 仍 4.5e-15 px 全过）。**推荐 Oracle 升级为「绝对标准对拍 + CRPIX↔CRVAL 不变量 + dec0≠0 用例」** | 高 | `tests/unit/v6_p3_proj/p3_proj_wcs_oracle.py`、`p3_proj_legacy_deviation.py`、ALG §15.6 T2/T6、P3-PROJ-TEST |
| **M1a-A-007** | **成立（判据正当，纯文档错）**：`PHASE3_RESAMPLE.md:22` 的 `iwc = CD⁻¹·(…)` 方向反（Paper I §2.1.1：中间坐标 = CD·(p−CRPIX)），`:54` 写 `\|center.Dec\|≥5°` 与 SCI:48 的 `\|dec\|≤85°` 语义相反；**实现两处都正确** ⇒ 只改 ALG | 高 | ALG-P3-002 施工规格两行 |
| **M1a-A-008** | **成立**：SCI:66「存在**有限**像素 ⇒ C=1」与 SCI:86「tile 内 NaN ⇒ C=1，值 NaN」互斥。**推荐取「足迹内存在 tile 像素（无论 NaN）⇒ C=1」**（= SCI:86 + §9a-9 的 missing↔无覆盖语义 + 实现事实），删掉 :66 的「有限」并补真值表 | 高 | SCI §5/§8/§9a-9、RSMP §6.6、DATA §30.4 |
| **M1a-A-009** | **成立**：`DATA_SEMANTICS:2435`（ivar==0 ⇒ u 无效）与 `:2441`（**含 ivar==0 导出** = 产品损坏 ⇒ 显式错误）互斥，`:2341` 又写「ivar==0 合法零权重」。**推荐：ivar==0 是像素级零权重 ⇒ u=NaN 传播态（非硬错误）**，从 :2441 删除该括注，并显式禁止 `1/0→Inf` 导出 | 高 | DATA §30.4-1/-3、`p3_resample.cpp:477-478` 注释（与推荐一致） |
| **M7-A-114** | **成立**：`UNCERTAINTY_AND_COVARIANCE.md:79` 的 `var_out=Σc_k²u_k` 丢同文 :18 的 `Cov(S_p,S_q)=Σ_j c_jp c_jq v_j` 项，而 :22 自报 mean\|ρ\|≈0.19 / max\|ρ\|≈0.57。**推荐把主式改为 `var_out=[R C_in Rᵀ]_ii`**（与 ALG-P3-001 §3 同式），标量式降为「C_in 对角」特例并附偏差界 0.75ρ：实测 ρ=0.19 ⇒ 方差低估 36.3%、σ 低估 20.2%（MC 复算 0.39415 vs 理论 0.39250） | 高 | SCI-UNC Phase3 节、DATA §30.4 传播公式、`p3_rsmp_covariance.cpp` / `p3_rsmp_propagation.cpp` 口径 |
| **M7-A-117** | **前提成立、订正建议错**：①NESTED 跨面邻接在 SCI §5/:63 未定义（实现走 3×3 `neighbors` 切平面四象限，见 §3.6）；②`leaf_order=tile_order+9` 只对 W=512 成立——但一般式是 **`leaf_order = order_sel + log2(W)`**，账本建议的 `+2·log2(W)` **多一倍**（W=512 会变成 +18 ⇒ nside 偏大 512 倍）。**推荐：SCI §4 收窄为 W=512（与 `p3_resample.cpp:43 kTileWidth=512` 一致），一般式写成 `+log2(W)`，并把 `>>2·log2(W)` 标明是**索引位移**而非 order 偏移** | 高 | SCI §2/:28/:57/:61/:62、ALG:32、`p3_resample.cpp:184` |
| **M7-A-129** | **①不成立（对实现是误报）**、②成立：实现 `p3_resample.cpp:369-371` 在象限缺角时**用最近点填充**（不是「只累加保留象限」），且四角任一 tile 缺失即 `coverage=0` fail-closed（:378），故不存在 Σ(kept w)<1 分支；②「Σw=1 精确成立」在 FP64 下非逐位命题——实测 2e5 组随机 (u,v)：\|Σw−1\|max = 2.22e-16 = **1 ULP**，逐位为 1 的比例 96.1% ⇒ 应写「=1 ± k·ULP」 | 高 | SCI:77、RSMP:86/:100/:105、PHASE3_FITS_IMPL:325 |
| **M7-A-209** | **成立**：SCI §2:29 定义 `s_out(deg/px)`，§5:55 的 `order_needed` 却用未定义符号 `s_out_rad`（全文零次换算），实现者按 deg 直代会把 order 算偏 log2(180/π)≈5.84 级。**推荐在 :55 公式内显式 `s_out_rad = s_out·π/180`** | 高 | SCI §2/§5、ALG:28、`docs/contracts/v6/data/08_phase3.md` |
| **M7-F-201** | **成立**：`PHASE3_RESAMPLE.md:86/:100/:105` 用「由构造保证」「max_abs=0」充当容差，而该断言在 FP64 下**不是逐位命题**（同上 1 ULP / 96.1%）。**推荐逐处改为数值容差 + dtype 参照**（bilinear 常数场：`\|Δ\| ≤ 4·ULP(1)·\|C\|`（FP64 权重 / FP32 输出另计）），并补一条能红的 CTest | 高 | RSMP §5c/§8/§9、PHASE3_FITS_IMPL:325、PHASE2_REJECTION §11.4 |

**需负责人确认的那一句话**（供 `DECISION_BRIEF.md` 引用）：
> 「CAR/AIT 一律**按 FITS WCS Paper II 把 CRVAL2（含 LONPOLE 默认规则）纳入映射**、AIT 域界收紧为 A≤1、CAR native 极行 fail-closed 拒绝；投影 registry 以 DESIGN §5.3 的**八投影**为准并升 v3，v1 registry 退场——请确认这三项（D-1/D-2/D-3/D-4）作为文档集变更与后续整改任务的统一口径。」

---

## 1 权威原文（逐条 `文件:行` + 逐字原文 + 权威链位置）

### 1.0 权威链与冲突规则（本报告所有「链位」的判定依据）

`ASTROCS_DESIGN.md:7-32`（§0 权威声明）逐字：

- `:11-19`（mermaid 节点）：`① 本文 最高设计 是什么·做到什么·顶层架构·CLI·发行·验收` / `② AGENTS.md 机器干活手册` / `③ 工程规范 ENGINEERING_SPEC` / `④ 控制包规范 CONTROL_PACK_SPEC` / `⑤ CI 规范 docs/ci/` / `⑥ 插件文档 docs/plugins/ 23 篇` / `docs/science 科学公式（权威）` / `docs/algorithms 算法推导（权威）` / `docs/design/UNIFIED_MODEL 数据对象与配置`；
- `:29`：**「本文与其他任何文档冲突时，以本文为准。」**
- `:31`：「Agent 无权放宽、重新解释或"为通过检查而改写"本文。」

`ENGINEERING_SPEC.md:23-28`（§3 科学代码红线）：`科学公式、权重/variance/ivar/SNR 定义、排异规则、归约顺序、精度与默认容差 不可修改`；`迁移必须 bitwise 相等`；`禁止一个字段承载多个含义`。
`ENGINEERING_SPEC.md:117-120`（§8 机器一致性检查）：`ci/checks.json 是唯一检查注册表`；**「每项检查有正例与负例（能红能绿）」**；`修改代码/测试后必须本地复跑对应检查项`。
链位判定：本报告把 `docs/science/**`（SCI）与 `docs/algorithms/**`（ALG）视为**公式/推导权威但受 DESIGN 覆盖**；`docs/plugins/**` 为 ⑥ 级、DESIGN 的下级落位文档，与 DESIGN 冲突时仍以 DESIGN 为准。

### 1.1 D-1（CAR/AIT 的 CRVAL2 不进映射）

| # | 文件:行 | 逐字原文 |
|---|---|---|
| D1-a | `docs/algorithms/PHASE3_PROJ_IMPL.md:398-401` | `- CAR/AIT 天球惯例：θ₀=+90°（native 北极=天球北极，LONPOLE=0 语义）⇒ native (φ,θ)=(α−α₀, δ) 恒等旋转；**CRVAL2 仅记录于 header 不进入映射**（CRVAL1=中央经线 α₀ 参与映射）。此为四投影统一 descriptor 语义下的显式冻结声明（FITS celestial CAR/AIT 实践一致）。` |
| D1-b | `docs/algorithms/PHASE3_PROJ_IMPL.md:451` | `\| CAR \| \\\|CRVAL dec\\\|≤85°（保守收窄）, \\\|δ\\\|≤90° \| 无（δ 线性） \| parity 显式；CRVAL1=中央经线 \| 同 G1；CRVAL2 不进映射 \| ≤180°（claim） \| 恒等旋转独立式 \|` |
| D1-c | `docs/algorithms/PHASE3_PROJ_IMPL.md:452` | `\| AIT \| … \| 同 G1；CRVAL2 不进映射 \| ≤360°（claim，全天空设计域） \| Paper II 反演独立式 + 3D 重建 \|` |
| D1-d | `lib/algorithms/projection/p3_projection.h:65` | `double crval_ra_deg = 0;    // ICRS 中心 RA（CAR/AIT: CRVAL1=中央经线；CRVAL2 记录但不进入映射，θ₀=+90° 天球惯例）` |
| D1-e | `docs/science/PHASE3_HIPS_TO_FITS.md:30` + `:101`（**SCI 侧的相反要求**） | `:30`：`\| center \| 输出中心 (RA,Dec) ICRS deg \| 用户显式 \|`；`:101`：`4. **像素中心/CRPIX/CD/经度方向**：FITS 1-based；CRPIX=((W_out+1)/2,(H_out+1)/2)…`（CRVAL=center 见 `docs/algorithms/PHASE3_RESAMPLE.md:16-18`：`CRPIX1=(W_out+1)/2, CRPIX2=(H_out+1)/2, CRVAL=center`） |
| D1-f | `docs/algorithms/PHASE3_PROJ_IMPL.md:392-397`（zenithal 旋转核，**与 CAR/AIT 共用同一组球面三角式**） | `sinθ = sinδ sinδ₀ + cosδ cosδ₀ cosΔα（=denom，≤0 → HEMISPHERE）` / `φ = atan2(−cosδ sinΔα, sinδ cosδ₀ − cosδ sinδ₀ cosΔα)` / `δ = asin(sinθ sinδ₀ + cosθ cosδ₀ cosφ)` / `Δα = atan2(−cosθ sinφ, cosδ₀ sinθ − sinδ₀ cosθ cosφ)` |

**链位**：D1-a/b/c 属 ALG（推导权威，DESIGN §0 虚线引用）；D1-d 是 registry 签名头注释；D1-e 属 SCI（科学公式权威）。**D1-a 与 D1-e 互斥**：D1-e 要求 `CRVAL=center`（用户给的中心，SCI:48 允许 `\|dec\|≤85°`），而 D1-a 冻结「CRVAL2 不进入映射」——后者等价于强制 `CRVAL2≡0`，两者不可能同真（几何论证见 §2.3）。

### 1.2 D-2（AIT 域界）

| # | 文件:行 | 逐字原文 |
|---|---|---|
| D2-a | `docs/algorithms/PHASE3_PROJ_IMPL.md:418-424` | `- **AIT**（Aitoff，θ₀=+90°）: 正向 D=√(1+cosθ·cos(φ/2))；X=2cosθ·sin(φ/2)/D, Y=sinθ/D。逆: A=X²/4+Y²（rad²），D²=2−A，D²≤0 → HEMISPHERE（椭圆域）；sinθ=Y·D，\|sinθ\|>1 → HEMISPHERE；θ=asin(sinθ)，φ=2·atan2(X·D/2, D²−1)（D²=1 即 φ=±180° 边界合法，atan2 唯一）。反演式由恒等式 A=1−cosθ·cos(φ/2) 封闭推导（X²/4+Y²=1−v, v=cosθ·cos(φ/2)），等价 Paper II 反演。奇点声明：椭圆域边界 X²/4+Y²=2；全天空（360°×180°）为设计目标域。` |
| D2-b | `docs/algorithms/PHASE3_PROJ_IMPL.md:437` | `- 域界语义冻结: TAN r≥π/2 / SIN ρ>1 / AIT D²≤0 → HEMISPHERE；` |
| D2-c | `docs/algorithms/PHASE3_PROJ_IMPL.md:418`（同上首句，**缺 √2 因子**） | `X=2cosθ·sin(φ/2)/D`（Paper II 为 `X=2γcosθ sin(φ/2)`、`γ=√2/D`） |

**链位**：ALG（推导权威）。与 Paper II 冲突（见 §2.4）；且与同文档 :422 的自证恒等式 `X²/4+Y²=1−v` 一致（该恒等式在**非标准缩放**下成立），说明整段是按缺 √2 的缩放自封闭推导出来的——错的是缩放，不是推导。

### 1.3 D-3（CAR 奇点与 θ 语义）

| # | 文件:行 | 逐字原文 |
|---|---|---|
| D3-a | `docs/algorithms/PHASE3_PROJ_IMPL.md:415-417` | `- **CAR**（plate carrée）: X=φ, Y=−θ。逆: φ=X·kRad, θ=−Y·kRad，\|θ\|>90° → PARAM（冻结域 \|δ\|≤90° fail-closed，拒绝柱面延伸域另一支）。无投影奇点（δ 线性）；高纬面积畸变由 FOV 声明承载。` |
| D3-b | `docs/algorithms/PHASE3_PROJ_IMPL.md:451` | `\| … \| 无（δ 线性） \| …`（奇点列显式写「无」） |
| D3-c | `docs/algorithms/PHASE3_PROJ_IMPL.md:369-372`（θ₀ 记法） | `\| 0 \| TAN \| "TAN" \| … \| zenithal, θ₀=CRVAL2 \|` / `\| 2 \| CAR \| … \| cylindrical, θ₀=+90° \|` / `\| 3 \| AIT \| … \| pseudo-cylindrical, θ₀=+90° \|` |
| D3-d | `docs/algorithms/PHASE3_PROJ_IMPL.md:437-439` | `CAR/AIT 天球端 \|δ\|>90°、CAR 平面端 \|θ\|>90° → PARAM` |

**链位**：ALG。与 Paper II 的 θ₀ 定义（投影**参考点**的 native 纬度：zenithal 为 +90°、CAR/AIT 为 0°）不一致 ⇒ 与 D-1 同源（把「native 极纬度恒 90°」当成了「参考点 θ₀=+90°」）。

### 1.4 D-4（八投影 vs 四投影）

| # | 文件:行 | 逐字原文 | 链位 |
|---|---|---|---|
| D4-a | `ASTROCS_DESIGN.md:262-267` | `### 5.3 投影算法（内置多种）` / `- **内置多种投影算法**，首批冻结 **TAN / SIN / CAR / AIT / STG / MOL / CEA / ZEA**（每种声明适用域、奇点、经度 wrap、轴手性、CRPIX/CRVAL/CD/PC/CDELT/CTYPE）；` / `- 新增投影经 projection registry 注册并附独立往返 Oracle；` / `- 用户通过 projection 字段选择，缺省 TAN；` | **① 最高设计** |
| D4-b | `docs/plugins/algorithms_phase3/14_projection.md:5` | `- **职责**：管理 export 支持的 WCS 投影 registry（**内置多种投影算法**：TAN/SIN/CAR/AIT/STG/MOL/CEA/ZEA），定义适用域、奇点、坐标语义与正反变换合同。` | ⑥ 插件文档 |
| D4-c | `docs/plugins/algorithms_phase3/14_projection.md:22` + `:31` + `:51` | `:22`：`- **内置多种投影**，首批冻结 **TAN / SIN / CAR / AIT / STG / MOL / CEA / ZEA**，每种声明：适用域、奇点、经度 wrap、轴手性、CRPIX/CRVAL/CD/PC/CDELT、CTYPE；`；`:31`：`\| projection \| tan \| —— \| tan/sin/car/ait/stg/mol/cea/zea \|`；`:51`：`- **八投影全覆盖测试**；` | ⑥ 插件文档 |
| D4-d | `docs/algorithms/PHASE3_PROJ_IMPL.md:20-21` | `- 非目标: 不实现 SIN/ZEA/CAR/AIT（矩阵 notes 列为显式扩展清单，当前仅 TAN，扩展须独立测试+新 claim，SCI-P3 §9a-3）；` | ALG（且与 D4-a 直接冲突） |
| D4-e | `docs/algorithms/PHASE3_PROJ_IMPL.md:348` + `:351-360` | `## 15 P3-001 增补：版本化 projection registry 与冻结四投影（TAN/SIN/CAR/AIT）`；`:351-360`：`> 增补 2026-09-10，任务 P3-001（ASTROCS-CONSTITUTION-ALIGNMENT-V1，BASE=9e0fa3a8）。宪章 ASTROCS-CONSTITUTION-001 §7.3（"投影由版本化 projection registry 注册…"）与 §18.1 负责人裁决 1（首批投影冻结为 TAN + SIN + CAR + AIT；新增投影必须经 projection registry 注册并附独立往返 Oracle）…` | ALG（**依据宪章**） |
| D4-f | `docs/science/PHASE3_HIPS_TO_FITS.md:100` + `:20` + `:43` | `:100`：`3. **输出投影清单**：alpha 仅 **TAN**；SIN/CAR 等增加须独立测试+新 claim（禁止"支持所有"）。`；`:20`：`- **非目标（alpha 显式拒绝项）**：… SIN/CAR 等非 TAN 投影；极近极点视场；GUI。`；`:43`：`- 输出: FITS-WCS **TAN**（CTYPE1=RA---TAN, CTYPE2=DEC--TAN, CUNIT=deg）` | SCI（科学公式权威，**alpha 会话面**） |

**宪章存在性核查（D4-e 的唯一依据）**：`find . -maxdepth 3 -iname "*CONSTITUTION*" -not -path "./build/*"` → 仅命中 `./run/gov001_constitution_check.py`（run/ 为临时产物面）；全树活动文档中 `ASTROCS-CONSTITUTION-001` 只出现在 `docs/archive/HANDOVER.md:7/24`（已归档）、`docs/contracts/v6/**.md`（旧世代档案）、`docs/contracts/DATA_SEMANTICS.md:2287/2288/2521`（引注）、`docs/science/v6/phase3/PHASE3_PROPAGATION_REVIEW.md:20/74/81/93`。**⇒ ALG §15 的「宪章 §18.1 首批四投影」在现行权威链里无可核验的落点。**

### 1.5 P3-002 八条的权威原文

| # | 文件:行 | 逐字原文 |
|---|---|---|
| A007-a | `docs/algorithms/PHASE3_RESAMPLE.md:22` | `iwc = CD^{-1} · ((x+1)−CRPIX1, (y+1)−CRPIX2)      # deg 偏移` |
| A007-b | `docs/algorithms/PHASE3_RESAMPLE.md:54` | `validate(params): frame=icrs, W,H∈[1,20000], s_out>0, \|center.Dec\|≥5°, pixfrac N/A`（对照 `docs/science/PHASE3_HIPS_TO_FITS.md:48`：`视场约束: abs(dec)<=85°（距极点 ≥5°；TAN 极点退化显式拒，单一条件，SCI/API/session 三处一致）`） |
| A008-a | `docs/science/PHASE3_HIPS_TO_FITS.md:65-66` | `coverage:` / `  C(x,y)=1 ⇔ 采样足迹内存在有限 tile 像素; 否则 C=0 且 S=NaN` |
| A008-b | `docs/science/PHASE3_HIPS_TO_FITS.md:86` | `\| tile 内 NaN \| 传播为输出 NaN（C=1，值 NaN），mask 语义经 coverage+NaN 判定 \|` |
| A009-a | `docs/contracts/DATA_SEMANTICS.md:2434-2435` | `1. 输入 HiPS 含 variance/ 子产品 → u_in = variance 平面；否则含 ivar/ → u_in = 1/ivar（ivar==0 像素 = u 无效）；两者并存 → variance 优先，` |
| A009-b | `docs/contracts/DATA_SEMANTICS.md:2441-2443` | `3. u 值域: NaN = 传播态（见 invalid 表）；负/Inf（含 ivar==0 导出）= **产品损坏 → 显式错误**（run 拒绝，rc 由实现任务映射到现行 P3_RS_*/ACS_ERR_* 状态域登记，禁 clamp/补 0/静默跳过）；` |
| A009-c | `docs/contracts/DATA_SEMANTICS.md:2341` | `F-UNC-001）；读侧消费按 §20.1（ivar==0 合法零权重、nonfinite 拒），` |
| A114-a | `docs/science/UNCERTAINTY_AND_COVARIANCE.md:17-22` | `Cov(S_p, S_q) = Σ_j c_jp c_jq v_j` / `V19 不保存完整 covariance matrix；Monte Carlo 量化（SNR-012）：nside=512 合成帧相邻像素 mean\|ρ\|≈0.19、max\|ρ\|≈0.57。` |
| A114-b | `docs/science/UNCERTAINTY_AND_COVARIANCE.md:77-85` | `nearest :  var_out = u_in` / `bilinear:  var_out = Σ_k c_k² · u_k     # c_k = ALG-P3-003 G4 冻结权重, Σc_k=1` / `- **Σc_k² ≠ 1 是正确物理**：bilinear 平均降低独立像素方差但引入相邻相关（§上协方差机制），禁止误用 Σc_k=1 归一 variance（常数信号场不变量 SCI-P3 §7 只对 signal 成立，对 variance 不成立）。` |
| A117-a | `docs/science/PHASE3_HIPS_TO_FITS.md:28` | `\| leaf_order \| tile 内像素 order = tile_order+9 \| DATA_SEMANTICS §3 \|` |
| A117-b | `docs/science/PHASE3_HIPS_TO_FITS.md:57` + `:61` + `:62` | `:57`：`tile 像素角尺度 ≈ sqrt(π/3) / (2^(order_sel) · W) rad        # leaf_order=order_sel+9`；`:61`：`ipix  = ang2pix_NESTED(nside=2^(order_sel+9), RA, Dec)        # 像素级 leaf (leaf_order=order_sel+9)`；`:62`：`tile  = ipix >> (2·log2(W))                                   # DATA_SEMANTICS §3 (W=512 → >>18)` |
| A117-c | `docs/science/PHASE3_HIPS_TO_FITS.md:47`（W 值域） | `properties 必需键存在且合法: hips_order(int≥0), hips_tile_width(2 的幂，默认 512)…` |
| A117-d | `docs/algorithms/PHASE3_RESAMPLE.md:32-34` | `leaf_order = order_sel + tile_shift(=9, W=512)` / `ipix = ang2pix_NESTED(nside=2^leaf_order, RA, Dec)` / `tile = ipix >> (2·log2(W));  local = ipix & ((1<<2·log2(W))−1); (lx,ly)=nested_local_to_xy(local)` |
| A129-a | `docs/science/PHASE3_HIPS_TO_FITS.md:77` | `- **bilinear 权重和**：4 邻域权重和恒为 1（无增益/衰减伪影）。` |
| A129-b | `docs/algorithms/PHASE3_RESAMPLE.md:86` | `- G2 内为逐像素标量三角算术(自动向量化安全: 无跨像素依赖)；S/C 写入行连续无别名；bilinear 权重和=1 由构造保证(4 权重显式归一, FP64)。` |
| A209 | `docs/science/PHASE3_HIPS_TO_FITS.md:29` + `:55` | `:29`：`\| s_out \| 输出像元角尺度 deg/px \| 用户显式 \|`；`:55`：`order_needed = ceil( log2( sqrt(π/3) / (W · s_out_rad) ) )   # tile 像素角尺度 ≤ s_out 的最小 order` |
| F201-a | `docs/algorithms/PHASE3_RESAMPLE.md:100` + `:105` | `:100`：`- reference 实现即生产实现(首版)；Oracle=SCI-P3 §11 全集, **Oracle 不调用本模块**…容差: WCS roundtrip ≤1e-6 px, 常数场 max_abs=0(nearest), bilinear 常数场 max_abs=0, 解析场容差由 SYN-007 预冻结。`；`:105`：`- 常数场 0：bilinear 权重和=1 构造保证；` |
| F201-b | `docs/algorithms/PHASE3_FITS_IMPL.md:325`（账本点名面） | 见 §6.5（`逐值精确` 族，本轮未展开为独立实验，按账本口径并入本条的容差族结论） |

### 1.6 F-5（Oracle 条款）

| # | 文件:行 | 逐字原文 |
|---|---|---|
| F5-a | `docs/science/PHASE3_HIPS_TO_FITS.md:116` | `- Oracle 调用生产 lookup/WCS wrapper（13 §5 独立性）。`（§10 不可接受变化清单之一） |
| F5-b | `docs/science/PHASE3_HIPS_TO_FITS.md:120` | `常数球面场恒定；解析球面函数经 WCS 反变换逐像素比对；…WCS round-trip；baseline/ISA、1/N worker、双平台数值合同；FITS header 以**独立** WCS/FITS 读取器验证。Oracle 不调用生产 HiPS lookup/resampler；小规模允许高精度直接球面计算作 reference。` |
| F5-c | `docs/algorithms/PHASE3_PROJ_IMPL.md:322-325`（§12 T6） | `- **T6 oracle**: 现状=独立解析解（test_p1002_gaps.py）；验收级=**WCSLIB 独立实现**（矩阵 notes "WCSLIB test oracle"，由 P3-PROJ-TEST 建立，禁止用生产实现自证）；双平台数值合同按 backend 数值测试族先例。` |
| F5-d | `docs/algorithms/PHASE3_PROJ_IMPL.md:463-467`（§15.6 T2） | `T2 每投影独立往返 Oracle（3D 单位向量第一性原理：切基正交投影+TAN 透视除法+SIN 正交重建+CAR/AIT 恒等旋转独立式+Paper II 反演式；往返 <1e-6 px 冻结）；` |

**链位**：F5-a/b 属 SCI（Oracle 独立性硬要求）；F5-c 属 ALG（明确「禁止用生产实现自证」）；F5-d 是现有「独立往返 Oracle」的实际口径——**它把「恒等旋转」写进了 Oracle 的构造前提**，因此该 Oracle 与生产实现同源（同一套错误假设）。

---

## 2 外部依据

### 2.0 全文获取失败与替代口径

```
web_fetch https://www.aanda.org/articles/aa/full_html/2002/45/aah3860/aah3860.right.html
→ HTTP 403, body: "<html lang=\"en\"><head><title>aanda.org</title>… Please enable JS and disable any ad blocker … captcha-delivery.com …"
```
故按 R-1 任务书授权：「若无法取全文，用 astropy 的实现作可执行标准并注明」。下文凡标「Paper I/II 语义」处，均给出该语义在 **astropy/WCSLIB 上的可执行验证**（命令与输出在 §2.2/§2.4/§3.4），不冒认逐字引文。

### 2.1 用到的三条标准语义（附可执行验证）

1. **CRPIX↔CRVAL 定义性不变量**（Paper I §2.1.1 / Paper II §2.2）：中间世界坐标为 0 的像素（CRPIX）其天球坐标即 CRVAL。
   验证：构造 astropy WCS（`crval=[10,30], crpix=[9,9]`）→ `all_pix2world([[8,8]],0)` = `(10.000000, 30.000000)`（见 §2.4 输出）。**任何 pixel→world(CRPIX) ≠ CRVAL 的实现都不满足 WCS 标准。**
2. **native 层**（Paper II Table 1）：CAR 为 `x=φ, y=θ`；AIT 为 `x=2γcosθ sin(φ/2)`、`y=γ sinθ`、`γ=√2/√(1+cosθ cos(φ/2))`。
   验证：以 `crval=(α₀,0)`、LONPOLE 默认 0 时，astropy 在 plane `(X,Y)=(40°,5°)` 给 `(RA,Dec)=(α₀+40°, 5°)`（§2.2 输出）⇒ 身份旋转 + `X=φ,Y=θ` 成立；AIT 的 `√2` 因子由「标准边界半轴 = 2√2 与 √2 rad」独立印证（§2.4）。
3. **旋转三 Euler 角**（Paper II §2.2）：投影参考点 `(φ₀,θ₀)`、`CRVAL=(α₀,δ₀)`、`LONPOLE=φ_p`（native 极的**天球**位置 `(α_p,δ_p)` 为解）满足球面三角形余弦定理：
   `sinδ₀ = sinθ₀ sinδ_p + cosθ₀ cosδ_p cos(φ₀ − φ_p)`　（★）
   `α_p = α₀ − atan2( −cosθ₀ sin(φ₀−φ_p), sinθ₀ cosδ_p − cosθ₀ sinδ_p cos(φ₀−φ_p) )`
   native↔celestial 正映射：`δ = asin(sinθ sinδ_p + cosθ cosδ_p cos(φ−φ_p))`、`Δα = atan2(−cosθ sin(φ−φ_p), sinθ cosδ_p − cosθ sinδ_p cos(φ−φ_p))`、`α = α_p + Δα`。
   **LONPOLE 默认规则**：`δ₀ ≥ θ₀` ⇒ 0°，否则 180°（可执行验证：`w.wcs.lonpole` 在 CAR/AIT 的 δ₀=0/+30 时 = 0.0、δ₀=−30 时 = 180.0；TAN/SIN 恒为 180.0）。
   **可解性**：把 LONPOLE 设成不合法值会被 wcslib 直接拒——`CAR, crval=(0,−30), lonpole=0` 抛 `InvalidTransformError: ERROR 4 in celset() … No valid solution for latp for these values of phip, phi0, and theta0` ⇒ LONPOLE 不是自由自旋，而是被 (★) 约束的量。

### 2.2 从 astropy 反解出的等价旋转（实验 exp4 / exp4b）

最小二乘反解（48 个 native 采样点，`orth = ‖MᵀM−I‖∞`）：

```
proj crval1 crval2  | alpha_p   delta_p   phi_p     | 90-dp    th_cp    | orth      det
CAR  0      0       | 90.5587   90.0000   132.9300  | 0.000    90.000   | 3.77e-15 +1.000000000000
CAR  0      30      | 180.0000  60.0000   360.0000  | 30.000   60.000   | 3.82e-15 +1.000000000000
CAR  0      -30     | 0.0000    60.0000   180.0000  | 30.000   60.000   | 3.80e-15 +1.000000000000
CAR  10     30      | 190.0000  60.0000   360.0000  | 30.000   60.000   | 3.78e-15 +1.000000000000
CAR  0      60      | 180.0000  30.0000   360.0000  | 60.000   30.000   | 3.73e-15 +1.000000000000
CAR  0      -60     | 0.0000    30.0000   180.0000  | 60.000   30.000   | 3.84e-15 +1.000000000000
AIT  0      30      | 180.0000  60.0000   0.0000    | 30.000   60.000   | 3.79e-15 +1.000000000000
AIT  0      -30     | 0.0000    60.0000   180.0000  | 30.000   60.000   | 3.73e-15 +1.000000000000
```
（δ₀=0 行的 `alpha_p/phi_p` 是 `δ_p=90°` 的参数化退化，`delta_p=90` 与 `M` 本身仍然精确。）

直接读 astropy 的 native 网格（`crpix=[1,1], cd=diag(1,1)` ⇒ 像素 = 平面 (X,Y) deg）：

```
CAR crval2=  30.0 lonpole=0.0 latpole=60.0
    (X=   0.0,Y=  90.0) -> (ra= 180.00000, dec=  60.00000)
    (X=   0.0,Y=   5.0) -> (ra= 360.00000, dec=  35.00000)     # δ = δ0 + θ（北向上）
    (X=   0.0,Y=  60.0) -> (ra= 360.00000, dec=  90.00000)
CAR crval2= -30.0 lonpole=180.0 latpole=60.000000000000014
    (X=   0.0,Y=   5.0) -> (ra=   0.00000, dec= -25.00000)     # 仍 δ = δ0 + θ（北向上）
    (X=   0.0,Y=  30.0) -> (ra=   0.00000, dec=   0.00000)
CAR crval2=   0.0 lonpole=0.0 latpole=90.0
    (X=  40.0,Y=   5.0) -> (ra=  40.00000, dec=   5.00000)     # 身份旋转
```
⇒ 用 (★)+(α_p 式) 复算：δ₀=+30,φ_p=0 ⇒ `cosδ_p = sinδ₀ = 0.5` ⇒ δ_p=60、`α_p = α₀−atan2(0,−sinδ_p) = α₀+180`；δ₀=−30,φ_p=180 ⇒ `cosδ_p = sinδ₀/cos180 = 0.5` ⇒ δ_p=60、`α_p = α₀` —— **与反解表逐项一致**（含 α₀=10 的 `α_p=190`）。

### 2.3 推导链、被证伪的猜想与最终验证

- 首轮我把 Paper II 的约束猜成 `sinδ_p = sinθ₀sinδ₀ + cosθ₀cosδ₀cos(φ₀−φ_p)`（**把 δ₀ 与 δ_p 写反**）：对 δ₀=+30/φ_p=0 恰好也给出 60（巧合），但对 δ₀=−30/φ_p=180 给 −60，与 astropy 的 `latpole=+60` 矛盾 ⇒ **该猜想被实验证伪**。改用球面三角形余弦定理重推得 (★)（`sinδ₀` 在左、`δ_p` 在右），δ₀=±30/±60 全部复现 astropy 的 `latpole`；且 wcslib 对 `lonpole=0, δ₀=−30` 报 "No valid solution" 正好对应 (★) 在 φ_p=0 时给出 `|δ_p|=120>90` 的无解分支 —— 三方（推导、反解、报错）互相印证。
- **最终验证不在公式层而在可执行层**：把 (★)+α_p+φ_p 落成 C++ 补丁（`patch/p3_proj_v6_fixed.cpp`，仅 run/ 下副本），与 astropy 在 22 组 (proj, α₀, δ₀∈{0,±10,±30,±60,±85}, parity, PA, 尺寸) 的整网格逐点对拍，最大球面偏差 **≤ 7e-13 deg**（见 §3.4）。
- **CAR/AIT 的 (φ₀,θ₀)**：两者参考点均为 `(0,0)`（AIT 在 `(φ,θ)=(0,0)` 处 `X=Y=0`；CAR 同为 `X=φ,Y=θ`），故 (★) 化简为 `cosδ_p = sinδ₀ / cosφ_p`、`α_p = α₀ − atan2(sinφ_p, −sinδ_p cosφ_p)`。**这正是「CRVAL2 进入映射」的唯一正确形式，也是「θ₀=+90° 恒等旋转」在 δ₀≠0 时的失效点。**

### 2.4 D-2/D-4 的外部依据

- **AIT 椭圆域**（可执行标准）：以 `crpix=[1,1], cd=diag(1,1), crval=(0,0)` 扫 astropy：X 轴最后有限点 = **162.0560°**（= 2√2 rad = 162.0569°）、Y 轴 = **81.0280°**（= √2 rad = 81.0285°）；即 AIT 像椭圆在**标准平面坐标**下为 `X²/8 + Y²/2 ≤ 1`（半轴 2√2、√2）。换算到仓库 v1 的缩放（`X_v1=X_std/√2`）即 `X_v1²/4 + Y_v1² ≤ 1`，换算到 v6 的归一量 `xp=X_std/√2, yp=Y_std/√2` 亦为 `xp²/4+yp² ≤ 1` ⇒ **阈值应为 1，仓库为 2（两倍）**。
- **D-4 八投影的标准性**（可执行）：对 `TAN/SIN/CAR/AIT/STG/MOL/CEA/ZEA` 逐一构造 astropy WCS 并求值，**8/8 全部可用**；抽样 `ARC/AZP/SZP/MER/SFL/PAR` 亦可用（`COP/COE` 因我给的中心参数不适配而报错，与本议题无关）⇒ DESIGN §5.3 的八投影全部落在 Paper II 标准集合内，ALG §15 的「四投影」是子集而非替代。

---

## 3 实现事实（命令 + 逐字输出 + `文件:行`，与 §1/§2 逐条对照）

> 复跑入口（可复现，任何人可重跑）：
```bash
cd "run/PROJECT-GOVERNANCE-01/R-1"
g++ -O2 -std=c++17 -I ../../../lib/algorithms/projection probe_p3proj.cpp \
    ../../../lib/algorithms/projection/p3_projection.cpp \
    ../../../lib/algorithms/projection/p3_proj_v6.cpp -o probe_p3proj
python3 mk_patch.py && g++ -O2 -std=c++17 -I ../../../lib/algorithms/projection \
    probe_p3proj_fixedsrc.cpp ../../../lib/algorithms/projection/p3_projection.cpp \
    patch/p3_proj_v6_fixed.cpp -o probe_fixed
python3 exp1_wcs_astropy.py; python3 exp2_domain_pole.py; python3 exp4_rotation.py
python3 exp4b_astropy_probe.py; python3 exp5_fix_verify.py; python3 exp6_gate_audit.py; python3 exp7_numbers.py
```

### 3.1 v1 registry（`lib/algorithms/projection/p3_projection.cpp`）

- `:192-206` CAR 正向：`:197-198` `const double phi = xd * kRad;    // φ = X (rad)` / `const double theta = -yd * kRad; // θ = −Y (rad)`；`:199` `if (std::fabs(theta) > M_PI / 2.0) return P3ProjectionStatus::P3_PROJ_PARAM;`（**放行 |θ|=90**）；`:201-204` `double ra_out = d->crval_ra_deg + phi * kDeg; … *dec_deg = theta * kDeg;`（**dec 取 −yd，CRVAL2 未出现**）。
- `:208-218` CAR 逆向：`:212-214` `dra = fmod(ra−crval_ra, 360) … 最短角差 ∈ (−180,180]`；`:217` `return plane_to_pix(d, phi * kDeg, -theta * kDeg, x, y);`（**CRVAL2 仍未出现**）。
- `:224-245` AIT 逆向：`:231` `const double dsq = 2.0 - (X * X / 4.0 + Y * Y);`、`:232` `if (!(dsq > 0.0)) return …HEMISPHERE;`（**域判据 A<2**）；`:240-243` `double ra_out = d->crval_ra_deg + phi * kDeg; … *dec_deg = theta * kDeg;`（**CRVAL2 未出现**）。
- `:256-260` AIT 正向：`const double dq = std::sqrt(1.0 + std::cos(theta) * std::cos(phi / 2.0));` / `const double X = 2.0 * std::cos(theta) * std::sin(phi / 2.0) / dq;`（**缺 Paper II 的 √2**）。
- `:315-352` `p3_projection_make`：`:328` `if (std::fabs(centre_dec_deg) > kMaxAbsDec) return …PARAM;`（kMaxAbsDec=85，`:38`）；`:343-350` 四角投影域守卫。

### 3.2 v2 registry（`lib/algorithms/projection/p3_proj_v6.cpp`）

- 头注 `:1`：`// lib/algorithms/projection/p3_proj_v6.cpp — V6 Phase3 投影实现层（标准 FITS WCS Paper II）`；`p3_proj_v6.h:45-46`：`// 版本化 registry 版本（宪章 §7.3）。v2 = 标准 Paper II CAR/AIT 修正。` / `constexpr int kProjectionRegistryVersion = 2;`
- `:163-176` CAR 正向：`:168-169` `const double phi_deg = xd;  // φ = X（deg）` / `const double theta_deg = yd;  // θ = Y（deg）；CAR: θ=δ`；`:170` `if (std::fabs(theta_deg) > 90.0) return ProjStatus::kParam;`；`:171-174` `double ra_out = d->crval_ra_deg + phi_deg; normalize_ra(&ra_out); *ra_deg = ra_out; *dec_deg = theta_deg;`（**CRVAL2 未进入映射**，与 §1.1 D1-a 逐字一致；同时**修正了 v1 的 dec 反号**，`:162` 注释：`标准 Paper II: X=φ, Y=θ`）。
- `:178-184` CAR 逆向：`const double dra = shortest_dra_deg(ra_deg - d->crval_ra_deg); return plane_to_pix(d, dra, dec_deg, x, y);`（仅 CRVAL1 参与）。
- `:187-208` AIT 逆向：`:194` `const double xp = xrad / kSqrt2, yp = yrad / kSqrt2;`；`:195-196` `const double dsq = 2.0 - (xp * xp / 4.0 + yp * yp); if (!(dsq > 0.0)) return …kHemisphere;`（**即 A<2；标准为 A≤1**）。
- `:210-223` AIT 正向：`:219-221` `const double gamma = kSqrt2 / dq; const double X = 2.0 * gamma * std::cos(theta) * std::sin(phi / 2.0); const double Y = gamma * std::sin(theta);`（**标准 √2 在位**）——但 `:203-206` 的 ra/dec 仍是 `crval_ra + phi`、`theta`（CRVAL2 未进入映射）。
- `:267-295` `projection_margin`：CAR 用 `kHalfPi − |th|`、AIT 用 `dsq`（**同一 A<2 口径**）。
- `:225-234` registry 表 4 行（TAN/SIN/CAR/AIT，与 ALG §15.1 一致）。

### 3.3 接线事实（决定影响面）

- `lib/algorithms/projection/module.yaml:38/55`：`dll_target=astrocs_p3_projection.dll 为迁移目标合同值（尚未存在）`、`entrypoint=MISSING`；`tests/unit/CMakeLists.txt:1256-1265` 只把 `p3_projection.cpp` 编进测试目标；`tests/unit/v6_p3_proj/CMakeLists.txt:14-25` 同理。**⇒ 两套 registry 目前都只在测试面编译，生产 alpha 路径（`lib/phase3_session/p3_wcs.cpp`）只认 TAN**（ALG §4：`对 projection 仅接受 "TAN"`）。这决定了 D-1/D-2/D-3 的**当前**生产爆炸半径为零，但 D-4 一旦开工即成 P0。

### 3.4 astropy 逐点对拍（exp1 / exp5，关键输出）

exp1（现状）：

```
v1  CAR crval=(10.0, 0.0)    scale=0.2  max_sep= 3.200e+00 deg   worst at pixel (7.0,0.0): prod=(10.200000,1.600000) astropy=(10.200000,-1.600000)
v6  CAR crval=(10.0, 0.0)    scale=0.2  max_sep= 1.479e-06 deg   ← 等于本测度 acos 地板(≈1.2e-6)，即完全一致
v1  CAR crval=(10.0, 30.0)   scale=0.2  max_sep= 3.320e+01 deg
v6  CAR crval=(10.0, 30.0)   scale=0.2  max_sep= 3.000e+01 deg   worst at (8.0,1.0): prod dec=-1.400000 astropy dec=28.600000
v1  AIT crval=(10.0, 0.0)    scale=0.2  max_sep= 9.376e-01 deg   ← 缺 √2（2.263/1.600 = 1.4142）
v6  AIT crval=(10.0, 0.0)    scale=0.2  max_sep= 1.207e-06 deg   ← 完全一致
v6  AIT crval=(10.0, 30.0)   scale=0.2  max_sep= 3.000e+01 deg
v1/v6 TAN,SIN crval=(10.0,30.0)         max_sep= 1.207e-06 deg   ← 完全一致（zenithal 核本就正确）
```

exp5（现状 vs 候选修复 vs astropy；`max 球面偏差 deg` / CRPIX 处 world）：

```
proj crval1  crval2  | cur_maxsep  fix_maxsep  | 现状 CRPIX->world           | 修复 CRPIX->world
CAR  10      0       | 1.025e-16   1.025e-16   | ( 10.00000,  0.00000)       | ( 10.00000,  0.00000)
CAR  10      10      | 1.000e+01   6.468e-13   | ( 10.00000,  0.00000)       | ( 10.00000, 10.00000)
CAR  10      30      | 3.000e+01   6.262e-13   | ( 10.00000,  0.00000)       | ( 10.00000, 30.00000)
CAR  10      85      | 8.500e+01   4.848e-13   | ( 10.00000,  0.00000)       | ( 10.00000, 85.00000)
CAR  10     -30      | 3.000e+01   6.008e-13   | ( 10.00000,  0.00000)       | ( 10.00000,-30.00000)
CAR  350     30      | 3.000e+01   4.870e-13   | (350.00000,  0.00000)       | (350.00000, 30.00000)
AIT  10      30      | 3.000e+01   6.565e-13   | ( 10.00000,  0.00000)       | ( 10.00000, 30.00000)
AIT  200    -45      | 4.500e+01   5.913e-13   | (200.00000,  0.00000)       | (200.00000,-45.00000)
CAR  10     -85      | 8.500e+01   4.839e-13   | ( 10.00000,  0.00000)       | ( 10.00000,-85.00000)
   （22 组全覆盖，cur_maxsep 恰 = |CRVAL2|；fix_maxsep 最大 6.854e-13 deg = 2.5e-9 arcsec）
AIT 域界: v6 last_ok_X=229.1250 / v6f last_ok_X=162.0000（标准 162.0569）; v6 last_ok_Y=114.5700 / v6f=81.0000（标准 81.0285）
CAR 极点行: (X=0,Y=90) astropy=(190.000000,60.000000) 现状=(OK,90.000000) 修复=(OK,60.000000)
```

### 3.5 现有门禁实测（ctest 子集，全绿）

```
74 p3_wcs Passed / 106 p3_projection_units Passed / 107 p3_projection_fault Passed
380 v6_p3_proj_units Passed / 381-385 v6_p3_proj_fault_* Passed
386 v6_p3_proj_wcs_oracle Passed / 387 v6_p3_proj_legacy_deviation Passed
100% tests passed, 0 tests failed out of 11
```
**⇒ D-1/D-2 的缺陷在「astropy 独立 Oracle 全绿」的当下依然存在**（Oracle 用例集盲区，见 §4-A）。

扩展 Oracle（在**现状生产 probe** 上跑 `f5/p3_proj_wcs_oracle_ext.py`，仅在原 CASES 后追加 4 条 dec0=±30 用例）：

```
CASE CAR_0_0_0.2_16x601 PASS … CASE TAN_0_0_0.2_11x11 PASS        ← 原有 8 条全过（对 D-1 零区分力）
CASE CAR_10_30_0.2_17x17  FAIL   pix2world_vs_astropy=FAIL(30.0)  car_analytic_latitude_band=FAIL(0.17362864503653766)
CASE CAR_10_-30_0.2_17x17 FAIL   pix2world_vs_astropy=FAIL(29.999999999999986)  car_analytic_latitude_band=FAIL(0.17362864503618572)
CASE AIT_10_30_0.2_17x17  FAIL   pix2world_vs_astropy=FAIL(30.0)
CASE AIT_10_-30_0.2_17x17 FAIL   pix2world_vs_astropy=FAIL(29.999999999999986)
ORACLE FAIL   rc=1
```
同一扩展 Oracle 对**候选修复** probe：AIT 两条转 PASS；CAR 两条仅剩 `car_analytic_latitude_band` 一条红（该判据本身不合法，见 §4-B）。

### 3.6 P3-002 八条的实现事实

| 条 | 命令 | 逐字输出 | 对照结论 |
|---|---|---|---|
| M1a-A-007 | 读 `docs/algorithms/PHASE3_RESAMPLE.md` | `22:  iwc = CD^{-1} · ((x+1)−CRPIX1, (y+1)−CRPIX2)      # deg 偏移` | 与 `p3_projection.cpp:73-76` `*xd = d->cd[0][0] * dx + …`（= CD·dx）**互斥** ⇒ 文档错 |
| M1a-A-007 | 同上 `:54` | `54:  validate(params): frame=icrs, W,H∈[1,20000], s_out>0, \|center.Dec\|≥5°, pixfrac N/A` | 与 `p3_projection.cpp:328` `\|centre_dec\| > 85.0 → PARAM` **相反** ⇒ 文档错 |
| M1a-A-008 | `docs/science/PHASE3_HIPS_TO_FITS.md:66/86` | `C(x,y)=1 ⇔ 采样足迹内存在有限 tile 像素; 否则 C=0 且 S=NaN` / `\| tile 内 NaN \| 传播为输出 NaN（C=1，值 NaN）…` | 实现取后者：`p3_resample.cpp:393-396` `any_nan = isnan(v00)…; *value = any_nan ? nanf : val; *coverage = 1;   // §4: NaN 参与仍 C=1(S=NaN)`；`:180` 注释 `tile 缺失=false(coverage=0); NaN 像素=命中(值 NaN)` ⇒ SCI:66「有限」是缺陷 |
| M1a-A-009 | `grep -n 'ivar' lib/phase3_session/p3_resample.cpp` | `477:    // ivar==0 → u 无效 (§30.4-1: 像素级规则, NaN 传播态, 非产品损坏);` / `478:    // 其余负/Inf (负/Inf variance, 负 ivar) → 产品损坏 (§30.4-3)。` | 实现**自行裁决取 §30.4-1**，并在注释里显式抵触 §30.4-3 的括注 ⇒ 合同歧义（实现侧与 §5.5 推荐一致） |
| M7-A-114 | `docs/science/UNCERTAINTY_AND_COVARIANCE.md:17-22/77-85` | 见 §1.5 A114-a/b | 同文 :18 有完整协方差式、:22 自报 ρ，:79 的传播式却只留对角项 ⇒ 内部不一致；数值偏差见 §5.5 |
| M7-A-117 | `grep -n 'leaf_order' docs/science/PHASE3_HIPS_TO_FITS.md lib/phase3_session/p3_resample.cpp` | SCI `:28/:57/:61` 硬写 `+9`；`p3_resample.cpp:43 constexpr uint32_t kTileWidth = 512;`、`:184 const uint32_t leaf_order = static_cast<uint32_t>(tile_order) + 9;` | 实现与「W=512」自洽；SCI §4 却允许任意 2 的幂 ⇒ 文档域声明与公式/实现冲突（订正建议见 §4-F） |
| M7-A-129 | `sed -n '360,400p' lib/phase3_session/p3_resample.cpp` | `:369-371 if (!q[i][j]) q[i][j] = nearest_pt;`（`:358` 注释：`退化防护: 角点位置某些象限可能无邻域点 → 用最近邻点填充(确定性双线性退化)`）；`:378 if (!g00 || !g10 || !g01 || !g11) { *coverage = 0; return …; }` | **不存在「只累加保留象限」分支** ⇒ 账本 ① 对实现是误报；②（Σw 非逐位）成立，实测 1 ULP |
| M7-A-209 | `grep -rn 's_out_rad' docs/ lib/` | `docs/science/PHASE3_HIPS_TO_FITS.md:55`（唯一命中，无定义）、`lib/**` 零命中 | 符号未定义、实现未落地 ⇒ 冻结公式的量纲链断点 |
| M7-F-201 | `grep -n '构造保证\|max_abs=0' docs/algorithms/PHASE3_RESAMPLE.md` | `86:`…`bilinear 权重和=1 由构造保证(4 权重显式归一, FP64)`；`100:`…`bilinear 常数场 max_abs=0`；`105: - 常数场 0：bilinear 权重和=1 构造保证；` | 三处无数值容差、无 dtype 参照；exp7 实测该断言非逐位命题 |

---

## 4 ⭐ 门禁自审（这条门/判据本身是否正当）

总判据（`ENGINEERING_SPEC §8`：「每项检查有正例与负例（能红能绿）」）：**能红 ≠ 有效**。以下 7 条门逐项审。

### A. 门：`ctest v6_p3_proj_wcs_oracle`（#386，astropy 独立 Oracle）—— **门正当，但量测域不足（漏报）**

①依据：SCI:120 + ALG §12 T6 明确要求 WCSLIB 独立实现作 Oracle ✓；②可测量可复现 ✓（`python3 p3_proj_wcs_oracle.py run --probe …`，1.09 s，rc=0/1）；③无误判（它确实在做「非生产实现」的绝对对拍）✓；④**量测域错**：`CASES（:34-43）` 里 CAR/AIT 四例的 `dec0` 全是 `0.0`，而 CRVAL2 只在 δ₀≠0 时才进入映射 ⇒ 该门对 D-1 **结构性无区分力**；⑤漏报已实测（§3.5：现状 8/8 PASS，追加 dec0=±30 后 4 条 FAIL）。
**门应为**：`CASES` 至少含 `("CAR", α₀, ±30/±60, …)`、`("AIT", α₀, ±30/±60, …)`，并新增一条独立断言 `pix2world(crpix) == crval`（定义性不变量，比球面距离更硬）。

### B. 门：`car_analytic_latitude_band`（`p3_proj_wcs_oracle.py:168-177`）—— **门本身错（对 CRVAL2≠0 不可满足）**

①依据：它用 CAR 的解析纬度带 `Ω = s_ra(sinδ_hi − sinδ_lo)`，隐含前提「行 = 天球纬度带（δ=δ₀+θ，未倾斜）」；②可测量但**量测域未声明**；③**对任何实现都误判**——实测以 astropy 真值代入该判据：CRVAL2=0 → 相对偏差 1e-6（通过）；CRVAL2=10° → **2.05%**；30° → **17.36%**；60° → **110.2%**（判据阈值 1e-5）⇒ 一旦 D-1 修复（正确倾斜），该门**必红**；现状（未修复）也红（17.36%，见 §3.5）⇒ 该门**两个方向都无法区分**；④⑤误报风险 100%。
**门应为**：把该判据限定在 `|CRVAL2| ≤ 1e-9` 的 CAR 用例（并在注释里写明「tilted CAR 的行不是纬度带」），或直接删除、由 `astropy 逐像素球面盈余（检查 #2）` 承担。

### C. 门：`ctest v6_p3_proj_legacy_deviation`（#387）—— **门被反向使用（把缺陷冻结成期望）**

①依据：它是 finding 的**证据**门而非**修复**门；②可测量 ✓；③**把实现缺陷制度化为「预期偏差」**：脚本头注 `:7-9` 逐字写 `CAR: legacy 用 Y=−θ（declination 反号）-> dec 全反号；AIT: legacy 缺 Paper II γ 的 √2 因子 -> 天区尺度偏差 √2。rc=0 表示偏差如预期复现（证据成立）`，`:74/:77` 用 `worst > 3600.0` / `> 60.0` 做「复现成功」判据 ⇒ **修好之后这个门会变红**，于是它天然反对修复；④阈值明确但方向反了；⑤漏报：v1 的 CRVAL2 缺陷未被该门覆盖（用例 dec0=0）。
**门应为**：`xfail` 语义翻转——登记「v1 已 RETIRED/已修复」后期望该脚本**转红**，或把 v1 的偏差写进一张 `LEGACY_DEVIATION_TABLE`（含 CAR 反号、AIT 缺 √2、AIT 域 2 倍、CRVAL2 四项）并由 CI 断言「偏差集合只减不增」。

### D. 门：ALG §15.6 T2「每投影独立往返 Oracle（3D 向量第一性原理）」—— **同源自证（对 D-1 零区分力）**

①依据：宪章要求的「独立往返 Oracle」（该宪章文本不可核验，见 §1.4）；②可测量 ✓；③**判据把「正反自洽」当成「标准正确」**：现状实现在 30° 错映射下 `rt_err` 仍为 `4.53e-15 px`（§3.4 exp1 输出），门恒绿 ⇒ 与 SCI:116「Oracle 调用生产 WCS wrapper」的禁令精神冲突（用同一套旋转假设构造 Oracle 与实现）；④量测域只覆盖「往返」不覆盖「绝对对拍」；⑤漏报 D-1/D-2/D-3 全部。
**门应为**：T2 拆两条——(i) 往返不变量（现状保留）；(ii) **绝对标准对拍**（astropy/WCSLIB，容差 ≤1e-9 deg，含 CRPIX↔CRVAL 不变量与 dec0≠0 用例），并明写「往返通过不构成标准正确」。

### E. 门：账本 `M7-A-117` 的订正建议「leaf_order 写回 `+2·log2(W)` 一般式」—— **判据前提对、结论错**

算术裁决（exp7）：tile 内含 `W²=2^(2k)` 个 leaf 像素 ⇒ **HEALPix nest 索引位移** = `2k`（SCI:62 的 `>>(2·log2(W))` 是对的），而 **order 偏移** = `k = log2(W)`。W=512 时 `log2(W)=9` ✓、`2·log2(W)=18` ⇒ 按账本建议改会把 nside 抬高 `2^9=512` 倍。
**门应为**：`leaf_order = order_sel + log2(W)`，并把「索引位移」与「order 偏移」写成两个符号（W=256/512/1024 ⇒ 8/9/10 vs 16/18/20）。

### F. 门：账本 `M7-A-129①`（跳象限未重归一 ⇒ Σ(kept w)<1）—— **对实现不成立（误报）**

实现事实（§3.6）：`p3_resample.cpp:358-371` 的退化防护把缺角象限**用最近点填充**（不是丢弃），`:378` 四角任一 tile 缺失即 `coverage=0` fail-closed ⇒ 不存在 `Σ(kept w)<1` 分支，常数场不会被实现成 `(Σkept w)·C`。
**门应为**：把 ① 降级为「文档未定义退化分支的**行为**（填充而非丢弃、且是否允许同一 leaf 重复计入多象限）」，② 保留并改写为「Σw=1 ± k·ULP，禁逐位断言」（实测 `|Σw−1|max = 2.22e-16 = 1 ULP`，逐位为 1 的比例 96.1%）。

### G. 门：`M1a-A-007/008/009、M7-A-114/209、M7-F-201`（文档-实现互斥族）—— **门正当**

①依据充分（Paper I §2.1.1、SCI §4/§5/§8/§9a-9、DATA §30.4、UNC :17-22）；②可测量可复现（§3.6 每条都有命令与逐字输出）；③方向判定正确（A-007/008/009 是**文档错、实现对**；A-114/209/F-201 是**文档自身不一致/不可证伪**）；④阈值：仅 F-201/129 的「精确/构造保证」缺量测域与 dtype；⑤无误报。

### 4.8 门禁自审汇总

| 门 | 正当? | 应为 |
|---|---|---|
| #386 astropy Oracle | 正当但盲 | 补 dec0≠0 用例 + CRPIX↔CRVAL 不变量 |
| `car_analytic_latitude_band` | **错** | 限 `|CRVAL2|≤1e-9` 或删除 |
| #387 legacy deviation | 反向 | xfail + 偏差表只减不增 |
| ALG §15.6 T2 往返 Oracle | 自证 | 拆「往返」与「绝对对拍」 |
| M7-A-117 订正建议 | 结论错 | `+log2(W)` |
| M7-A-129① | 误报 | 降级为「文档未定义退化分支」 |
| A-007/008/009、A-114/209、F-201 | 正当 | 按 §5.5 改文本/容差 |

---

## 5 建议裁决

> 下列每条给出**唯一推荐结论**、置信度、反方论证（最可能被反驳的点）、最小改动路径与影响面。文档改动均落在 `docs/**`（受 `ENGINEERING_SPEC §3` 只读保护）⇒ 执行时须按 `AGENTS.md §8` 走**文档集变更**（负责人批准），但**裁决内容本身已可决策**。

### 5.1 D-1（CAR/AIT 的 CRVAL2）

- **唯一推荐**：按 Paper II §2.2 把 CRVAL2 纳入映射——CAR/AIT 采用**通用球面旋转核**（三 Euler 角 `(α_p, δ_p, φ_p)`，由 (★) 与 α_p 式解出；LONPOLE 取标准默认 0/180 并**显式写入 FITS 头**）。同时**保留** `(φ₀,θ₀)=(0,0)` 的 native 层（CAR `X=φ`；AIT 标准式），即「投影层不变、旋转层修正」。
  **不推荐**「收窄为 CRVAL2≡0」：那会让 `|CRVAL dec|≤85°` 的四道守卫（ALG §15.4、`kMaxAbsDec=85`、SCI §4、registry 表）全部失去意义，且 P3-002 的验收门「独立 WCS 解析器读回关键字正确」会对任何 `δ₀≠0` 的请求失败——**产品语义直接塌回 TAN**。
- **置信度**：高（外部标准可执行验证：22 组配置 ≤6.9e-13°；几何推导、参数反解、wcslib 报错三方互证）。
- **反方论证**（最可能被反驳）：①「ALG §15.2 是**明文冻结**（`CRVAL2 仅记录于 header 不进入映射`），改它等于改冻结科学定义」——反驳：该冻结与 `SCI:30/:101`（CRVAL=center、用户显式中心）**互斥**，冻结条款不能自我否证，且 §0 规定 DESIGN 覆盖任何下级冲突；②「D-1 影响面为零，因为 CAR/AIT 未接线」——反驳：影响面为零是**时机**论证不是**正确性**论证，GAP-011（八投影 registry）一开工即 P0；③「倾斜 CAR 会让 δ₀<0 的图上下翻转（LONPOLE 默认 180）」——这是标准行为，**缓解**：把 LONPOLE 写进头（读方不再依赖默认），并在 ALG 明写该约定；若负责人希望「任何 δ₀ 都北向上」，则须**固定 LONPOLE=0 并写入头**（同样是自洽的标准 WCS，差别只在惯例）——**这一条是本议题唯一需要负责人在两个自洽选项里点一下的**，但两者都要求「CRVAL2 进入映射」，不影响主结论。
- **最小改动路径**：`lib/algorithms/projection/p3_proj_v6.cpp` 增 45 行（`struct Tilt` / `tilt_params` / `native_to_sky_general` / `sky_to_native_general`）并改 CAR/AIT 四个函数体内 5 处（补丁全文见 `logs/patch_p3_proj_v6.diff`，125 行 diff）；`kProjectionRegistryVersion 2 → 3`；ALG §15.2（:398-401）、§15.3（:415-424）、§15.5（:449-457）与 §15.1 表（:365-372）同步；**变更 claim** 记入 ALG §15 变更历史（「v3：CAR/AIT 依 Paper II §2.2 引入 CRVAL2 旋转」）。
- **影响面**：v2 registry（v3）→ `tests/unit/v6_p3_proj/*`（T2/T4/T5 需按新口径重算参考值）、`v6_p3_proj_wcs_oracle.py` CASES、ALG §15.6、GAP-011 八投影实施、`docs/contracts/v6/data/08_phase3.md:21` 的 `CRVAL2=0` 前提注记。

### 5.2 D-2（AIT 域界）

- **唯一推荐**：域判据改 `A ≤ 1`（`A = X²/4+Y²`（v1 单位）= `xp²/4+yp²`（v6 归一）），v6 侧即 `dsq ≥ 1`；ALG:424「椭圆域边界 `X²/4+Y²=2`」订正为 `=1`；ALG:418 的 AIT 正向式**补回 Paper II 的 `√2`**（v1 侧同步，否则 v1 的平面尺度仍差 √2）。
- **置信度**：高（astropy 边界 162.0560/81.0280 vs 标准 162.0569/81.0285；仓库 v6 实测接受 229.125/114.570）。
- **反方论证**：①「A<2 只是多余接受，超界像素给的是同一片天区，不影响产品」——反驳：多接受的正是**折叠环带**（实测 v6 在 X=162.06° 处 φ=180.0°，X=229° 处 φ=350.8°，即 φ∈(180°,360°) 的重复覆盖），FOV≤360°（claim）下必然出现同一输出像素被两处天区竞争，`point_source_flux` 列归一会重复计入；②「v1 的 X 截止（162.06°）与标准重合，所以 v1 没错」——反驳：那是「域错 2 倍」与「尺度缺 √2」在 X 轴上的**数值巧合**；v1 的真实像边界在 X_v1=2 rad=114.59°（实测 φ=179.997°），它一路接受到 162.06° ⇒ 47.5° 折叠环带（Y 轴同理）。
- **最小改动路径**：`p3_proj_v6.cpp:195-196`（+ `:287-292` `projection_margin`）、`p3_projection.cpp:231-232` + `:256-258`（补 √2）；ALG §15.3/:418-424、§15.4/:437、§15.5/:452。
- **影响面**：AIT 的 `max_fov_deg=360`（claim）需改为「≤ 椭圆域内」或保留但明确「请求超出域即 HEMISPHERE」；`tests/unit/v6_p3_proj` 的 AIT 边界用例；`v6_p3_rsmp` 中依赖 AIT 全天空的 Ω 采样面。

### 5.3 D-3（CAR 奇点与 θ 语义）

- **唯一推荐**：①守卫收紧为 `|θ| ≥ 90° → PARAM`（native 极行 fail-closed；CAR 行 FOV claim 由 `≤180°` 改为 `<180°`）；②ALG §15.3/:417「无投影奇点（δ 线性）」与 §15.5/:451 奇点列改为「**native 极行 θ=±90°（整行塌缩：Ω=0、RA 无定义）**」；③θ 记法按 Paper II 统一：CAR/AIT 的**参考点** `(φ₀,θ₀)=(0,0)`，native 极恒在 θ=+90°（§15.1「域」列不再用 θ₀ 表示投影类别）。
- **置信度**：高。
- **反方论证**：①「astropy 在同一行也返回 dec=90 且 RA 随 X 变，说明标准允许」——反驳：标准确实允许**正映射**在该退化行取值（实测 astropy 亦如此），但该行的 Ω=0 是**产品级**不可用（`R` 行归一除以 ΣΩ=0、`point_source_flux` 列归一权重无定义），且 P3-002 明令「输出缺 WCS 却报成功」类产品不可接受 ⇒ fail-closed 才与项目原则一致；②「δ=±90° 的天球极点在倾斜 CAR 下不再是退化行」——正确，且**正好支持**把退化条件定义在 **native** θ 上（而非天球 δ），这也是推荐 ③ 的理由。
- **最小改动路径**：`p3_proj_v6.cpp:170`、`p3_projection.cpp:199`（> → ≥）；ALG §15.1/:371-372、§15.3/:415-417、§15.4/:438、§15.5/:451；`projection_margin` 的 CAR 分支同改（`:283`）。
- **影响面**：CAR FOV claim、四角守卫语义（`make` 的 corner guard 对 CAR 变为「禁触碰 native 极」）、`plan()` 的 `pole_margin_deg` 语义注记。

### 5.4 D-4（八投影 vs 四投影）

- **唯一推荐**：**以 DESIGN §5.3 / 插件 14_projection.md 的八投影为准**；ALG §15 全节按八投影重写（首批冻结集合、registry 表、六要素表、TEST 设计面），登记变更 claim；`p3_projection.cpp`（v1）与 `p3_proj_v6.cpp`（v2）**收敛为一套**（推荐：保留 v6 线并升 v3；v1 标 RETIRED 后删除或归档，附「v1 偏差表」入账本）。同时**修正 ALG §15 的依赖锚**：删除对不存在文档 `ASTROCS-CONSTITUTION-001 §18.1` 的引用，改引 `ASTROCS_DESIGN.md §5.3` + `docs/plugins/algorithms_phase3/14_projection.md`。SCI §9a-3 的「alpha 仅 TAN」**保留**（alpha 收窄不扩大），但在同句补「registry 面已注册八投影；alpha 会话未接线，`p3_wcs_validate_request` 仍仅接受 TAN」。
- **置信度**：高。
- **反方论证**：①「ALG 的四投影来自负责人裁决（宪章 §18.1），DESIGN §5.3 是愿景」——反驳：宪章在活动树中**不存在**（§1.4 证据），且 `DESIGN §0:29` 明文「本文与其他任何文档冲突时以本文为准」；`docs/plugins/…/14_projection.md`（⑥ 级、任务必读）同样写八投影并附「八投影全覆盖测试」的验收面；②「八投影会扩大 alpha 范围」——反驳：registry 注册 ≠ 会话接线；SCI §9a-3/:100 与 `p3_wcs_validate_request` 不动的方案已给出。
- **最小改动路径**：ALG §15（:348-508）整节 + §1 非目标（:20-21）；registry v3 表（8 行，含 `STG/MOL/CEA/ZEA` 的 native 层与域界）；插件文档 :18 的 `contracts/schemas/projection_registry.schema.json` 是**悬空引用**（`find contracts -iname '*projection*'` 零命中）⇒ 一并登记（建 schema 或改引用）。
- **影响面**：GAP-011 全部实施面、`module.yaml` 的 entrypoint/registry 版本、八投影各自的独立往返 + 绝对对拍 Oracle（每投影一条 dec0≠0 用例）、`STG/MOL/CEA/ZEA` 的六要素声明（含 CEA/MOL 的 θ₀=90° 与 ZEA 的等积域）。

### 5.5 P3-002 八条

| 条 | 唯一推荐 | 置信度 | 最小改动路径 | 影响面 |
|---|---|---|---|---|
| M1a-A-007 | `:22` 改 `iwc = CD · ((x+1)−CRPIX1, (y+1)−CRPIX2)`；`:54` 改 `|center.Dec| ≤ 85°（距极点 ≥5°）` | 高 | ALG-P3-002 两行 + 行内补 `path::symbol` 实测锚 | P3-002 施工规格；`tests/backend/test_p3_wcs.py` 无需改（实现已对） |
| M1a-A-008 | `:66` 删「有限」→「存在 tile 像素（NaN 计入）」，并在 §8 表下补 coverage⇔NaN 真值表（footprint 存在 ∧ 值有限 ⇒ C=1,S 有限；footprint 存在 ∧ 任一 NaN ⇒ C=1,S=NaN；tile 缺失 ⇒ C=0,S=NaN） | 高 | SCI §5/:66、§8/:86、§9a-9/:106；RSMP §6.6；DATA §30.4 invalid 表 | mask 消费方（coverage+NaN 双判据）、`p3_resample.cpp:396` 注释（已一致） |
| M1a-A-009 | 取「ivar==0 ⇒ 像素级零权重 ⇒ u=NaN 传播态」，从 `:2441` 删除「（含 ivar==0 导出）」，并在 `:2435` 显式写「ivar==0 ⇒ u=NaN（禁 1/0→Inf）」 | 高 | DATA §30.4-1/-3 两处 | `p3_resample.cpp:477-478`（注释即推荐口径）、W4 负面用例族（补 W4c：ivar==0 必产 NaN 且 rc=OK） |
| M7-A-114 | 主式改 `var_out = [R C_in Rᵀ]_ii`（与 ALG-P3-001 §3 一致）；标量式标注为「`C_in` 对角」特例，并附界 `var_out ≤ (Σc² + 0.75ρ_max)u` | 高 | `docs/science/UNCERTAINTY_AND_COVARIANCE.md:71-91`、`docs/contracts/DATA_SEMANTICS.md §30.4` 传播式引注 | VARIANCE/IVAR 产品语义、下游 SNR/自相关消费、`p3_rsmp_covariance.cpp` / `p3_rsmp_propagation.cpp`、`V6_PHASE3_RSMP_IMPL.md` |
| M7-A-117 | ①补 NESTED 跨面邻接定义（或明写「bilinear 走 3×3 `neighbors` 切平面四象限 + 缺角最近点填充 + 缺 tile fail-closed」= 实现事实）；②SCI §4/:47 收窄 `hips_tile_width == 512`（与 `kTileWidth=512` 一致），一般式写 `leaf_order = order_sel + log2(W)` 并标明 `>> 2·log2(W)` 是索引位移 | 高（**并否决账本的 `+2·log2(W)`**） | SCI §2/:28、§4/:47、§5/:57/:61/:62；ALG:32-34 | HEALPix 定位 / R-S 归一全链、非 512 tile 的 HiPS 输入（收窄后须显式拒绝） |
| M7-A-129 | ①降级为「退化分支行为未定义」并补文书；②「Σw=1 精确成立」→「`|Σw−1| ≤ 1 ULP(1.0)`（FP64），禁逐位断言」；常数场 `max_abs=0` → 分 dtype 相对容差 | 高 | SCI:77、RSMP:86/:100/:105、PHASE3_FITS_IMPL:325 | 常数场不变量门、SYN-007 容差表、`tests/unit/v6_p3_rsmp` 的常数场用例 |
| M7-A-209 | `:55` 公式内显式 `s_out_rad = s_out·π/180`（并在 §2 表补一行 `s_out_rad`） | 高 | SCI §2/:29、§5/:55；ALG:28 | order 选择链（`order_needed→order_sel→nside`）、`docs/contracts/v6/data/08_phase3.md` |
| M7-F-201 | 逐处给数值容差 + dtype 参照（bilinear 常数场：FP64 权重下 `|Δ| ≤ 4·ULP(1)·|C|`，FP32 输出另加 `2^-24` 量级）；「构造保证」改「按构造等于 1 ± k·ULP，k=1（实测）」；补一条 CTest 使其可红 | 高 | RSMP §5c/:86、§8/:100、§9/:105；PHASE3_FITS_IMPL:325；PHASE2_REJECTION §11.4 | 验收面可证伪性、`ci/checks.json`（若挂机器门） |

### 5.6 F-5（Oracle 独立性）

- **唯一推荐**：保留现有 astropy Oracle 并**补齐三件事**——(i) CASES 覆盖 `δ₀ ∈ {0, ±30, ±60, ±85}`（CAR/AIT，含 α₀≠0）；(ii) 新增 `pix2world(crpix)==crval` 不变量断言；(iii) 把「独立往返 Oracle（ALG §15.6 T2）」与「绝对标准对拍（T6）」在文档与测试命名上**分开**，并规定**门红 = 偏差 > 容差**，Oracle 参考值由 astropy/解析式给出（不得由生产实现生成）。`p3_proj_legacy_deviation.py` 改为 xfail 语义 + `LEGACY_DEVIATION_TABLE`（含 v1 的 CAR 反号、AIT 缺 √2、AIT 域 2 倍、CRVAL2 四项），并规定「偏差集合只减不增」。
- **置信度**：高（红/绿双向演示已复跑：现状 4 条新用例 FAIL 30.0°；修复后 AIT 转 PASS）。
- **反方论证**：①「Oracle 与实现同源 ⇒ 无效」这一原判据**部分不成立**：该 Oracle 确实调用 astropy（非同源），真正的问题是**用例集盲区**与**判据量测域**（§4-A/B）；把结论写成「Oracle 无效」会误导成「重写 Oracle」，而实际只需补用例与拆分门。
- **最小改动路径**：`tests/unit/v6_p3_proj/p3_proj_wcs_oracle.py`（CASES + 一条 CRPIX 断言 + 把 §4-B 的 band 判据限定 `|CRVAL2|≤1e-9`）、`tests/unit/v6_p3_proj/p3_proj_legacy_deviation.py`、ALG §15.6/§12 T2/T6。
- **影响面**：P3-PROJ-TEST 验收基线、GAP-011 每投影 Oracle、SYN-007 五件套。

### 5.7 汇总：改动面清单（供 `GAP_AUDIT.md` 转任务）

1. **代码**：`lib/algorithms/projection/p3_proj_v6.cpp`（D-1/D-2/D-3 三处；补丁 `logs/patch_p3_proj_v6.diff`）+ `kProjectionRegistryVersion→3`；`p3_projection.cpp` 退场（或同修）。
2. **文档（须文档集变更流程）**：`docs/algorithms/PHASE3_PROJ_IMPL.md` §1（:20-21）、§15 全节（:348-508）；`docs/algorithms/PHASE3_RESAMPLE.md` :22/:32-34/:54/:86/:100/:105；`docs/science/PHASE3_HIPS_TO_FITS.md` §2(:28-29)、§4(:47-48)、§5(:55-66)、§7(:77)、§8(:86)、§9a-3(:100)、§9a-9(:106)；`docs/science/UNCERTAINTY_AND_COVARIANCE.md` :71-91；`docs/contracts/DATA_SEMANTICS.md` §30.4-1/-3；`docs/plugins/algorithms_phase3/14_projection.md` :18（悬空 schema）/:51。
3. **Change claim**：ALG §15 变更历史登记 v3（CAR/AIT 旋转 + AIT 域 + CAR 奇点 + 八投影）；SCI 侧按 FROZEN 文档变更流程登记（§10「不可接受变化」中的 `order_needed` 公式本次只补单位换算，不改公式形态）。
4. **测试/门**：`tests/unit/v6_p3_proj/p3_proj_wcs_oracle.py`（CASES + 不变量 + band 判据域）、`p3_proj_legacy_deviation.py`（xfail + 偏差表）、`tests/unit/p3_projection_test.cpp`（随 v1 退场）、`ci/checks.json`（若新增容差门）。
5. **合同/schema**：`contracts/schemas/projection_registry.schema.json`（悬空 → 新建或改引用）；`docs/contracts/v6/data/08_phase3.md:21`（`CRVAL2=0` 前提注记）。

---

## 6 证据清单（全部命令 / 退出码 / 产物路径）

### 6.1 探针与补丁（全部在 `run/PROJECT-GOVERNANCE-01/R-1/`）

| 产物 | 说明 |
|---|---|
| `probe_p3proj.cpp` | 双 registry 探针（grid/plane/w2p/scan 四模式），编入 `p3_projection.cpp + p3_proj_v6.cpp` |
| `probe_p3proj` | `g++ -O2 -std=c++17 -I ../../../lib/algorithms/projection probe_p3proj.cpp … -o probe_p3proj`，rc=0 |
| `mk_patch.py` / `patch/p3_proj_v6_fixed.cpp` / `logs/patch_p3_proj_v6.diff` | 只在 run/ 下生成 `p3_proj_v6.cpp` 的修复副本（125 行 diff） |
| `probe_p3proj_fixedsrc.cpp` / `probe_fixed` | 修复副本的探针（同上编译命令，rc=0） |
| `probe_v6_fixed_src` | 用**仓库原 `v6_p3_proj_probe.cpp`** + 修复副本编译，供扩展 Oracle 复用（rc=0） |
| `f5/p3_proj_wcs_oracle_ext.py` | 仓库 Oracle 的副本 + 4 条 dec0=±30 用例（不覆盖原文件） |

### 6.2 实验脚本与日志（`timeout ≤115s`）

| 命令（cwd = `run/PROJECT-GOVERNANCE-01/R-1`） | 退出码 | 日志/JSON |
|---|---|---|
| `python3 exp1_wcs_astropy.py` | 0 | `logs/exp1_wcs_astropy.log` / `.json` |
| `python3 exp2_domain_pole.py` | 0 | `logs/exp2_domain_pole.log` / `.json` |
| `python3 exp4_rotation.py` | 0 | `logs/exp4_rotation.log` / `logs/exp4_rotation_params.json` |
| `python3 exp4b_astropy_probe.py` | 1（**预期**：末尾显式 `lonpole=0, δ₀=−30` 触发 wcslib `No valid solution`，该报错本身是证据） | `logs/exp4b_astropy_probe.log` |
| `python3 exp5_fix_verify.py` | 0 | `logs/exp5_fix_verify.log` / `.json` |
| `python3 exp6_gate_audit.py` | 0 | `logs/exp6_gate_audit.log` / `logs/exp6_projection_support.json` |
| `python3 exp7_numbers.py` | 0 | `logs/exp7_numbers.log` / `.json` |
| `ctest --test-dir build -R "p3_projection\|v6_p3_proj\|p3_wcs\|p3_proj" --output-on-failure`（cwd=仓库根） | 0（11/11 通过） | `logs/ctest_p3_subset.log` |
| `python3 f5/p3_proj_wcs_oracle_ext.py run --probe build/v6_p3_proj/v6_p3_proj_probe` | 1（**现状必红**） | `logs/f5_ext_oracle_current.log` |
| `python3 f5/p3_proj_wcs_oracle_ext.py run --probe ./probe_v6_fixed_src` | 1（AIT 两条转 PASS；CAR 两条仅剩 §4-B 的非法判据红） | `logs/f5_ext_oracle_fixed.log` |
| `python3 -c "import astropy…"` | 0 | astropy 7.0.1 / numpy 2.2.4 / Python 3.13.5 / g++ 14.2.0 |
| `web_fetch https://www.aanda.org/…/aah3860.right.html` | HTTP **403** | §2.0 逐字 body（站点反爬） |

### 6.3 门禁实测读数（逐字）

```
$ ctest --test-dir build -R "p3_projection|v6_p3_proj|p3_wcs|p3_proj" --output-on-failure
 74 p3_wcs Passed 0.13s | 106 p3_projection_units Passed 0.02s | 107 p3_projection_fault Passed 0.02s
380 v6_p3_proj_units Passed | 381-385 v6_p3_proj_fault_* Passed | 386 v6_p3_proj_wcs_oracle Passed 1.09s
387 v6_p3_proj_legacy_deviation Passed 0.53s
100% tests passed, 0 tests failed out of 11 ; rc=0
```

### 6.4 关键单点复现（任人可重跑）

```bash
cd "run/PROJECT-GOVERNANCE-01/R-1"
./probe_p3proj plane v1 CAR 10 30 0 0           # 现状: ra=10 dec=0    （CRPIX 处 world != CRVAL）
./probe_p3proj plane v1 CAR 10 30 0 90          # 极点行: status=OK dec=-90（v1 dec 反号；整行塌缩）
./probe_p3proj plane v6 CAR 10 30 0 90          # 极点行: status=OK dec=90 （v6；整行塌缩）
./probe_p3proj scan  v1 AIT 0 0 x 0 250 4000    # last_ok=162.0000（v1 单位）
./probe_fixed  plane v6f CAR 10 30 0 0          # 修复: ra=10 dec=30（= CRVAL）
./probe_fixed  plane v6f AIT 0 0 200 0          # 修复: HEMISPHERE（域外显式拒）
./probe_p3proj plane v1 AIT 0 0 114.59 0        # v1 真实像边界 φ=180.0（ra=179.9969）
./probe_p3proj plane v6 AIT 0 0 229.0 0         # v6 越界仍 OK（ra=350.8378）= 折叠环带
```

### 6.5 未展开项（如实登记）

- `docs/algorithms/PHASE3_FITS_IMPL.md:325` 的「逐值精确」族（`M7-F-201` 点名面之一）本轮只做文本核对，未构造独立实验；结论按「与 `PHASE3_RESAMPLE.md:86/:100/:105` 同族」处理。
- `docs/plugins/algorithms_phase3/15_resample.md` / `16_fits_output.md` 未逐行取证（本议题的合同面已由 SCI §5/§8/§9a 覆盖）。
- 八投影中 `STG/MOL/CEA/ZEA` 只做「astropy 可用性」核查（§2.4），未做仓库侧实现事实（仓库无实现）。

### 6.6 零改动核对

```
$ git status --porcelain -- lib/algorithms/projection tests/unit/v6_p3_proj \
      docs/algorithms/PHASE3_PROJ_IMPL.md docs/science/PHASE3_HIPS_TO_FITS.md docs/algorithms/PHASE3_RESAMPLE.md | wc -l
0
$ git rev-parse HEAD    → e9b31285af6dd6c9bb2636f9e4d707a433f8abd7   (branch main)
```
全程未执行任何 git 写操作（无 add/commit/push/stash/reset/checkout/分支）；本轮所有产物只落在 `run/PROJECT-GOVERNANCE-01/R-1/**` 与本报告 `reports/PROJECT-GOVERNANCE-01/research/R-1_投影WCS数学正确性.md`。
