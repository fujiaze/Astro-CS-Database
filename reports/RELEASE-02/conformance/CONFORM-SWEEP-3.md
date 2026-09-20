# CONFORM-SWEEP-3 — 规范↔实现符合性审计（Phase2 coverage/sampling/upm/rejection/integration + p2_op_* 生产节点）

> 分片: **CONFORM-SWEEP-3**（AstroCS RELEASE-02 规范↔实现符合性整体排查）
> 方法: 枚举规范侧规定性陈述（公式/常数/单位/定义域/步骤顺序/默认值/schema 字段语义）→ 定位实现 → 判定 → 取证
> 工作目录: `/workspace/Astro CS Database`（HEAD `cbda64a1424030f977fbaa777fb3923bdcfc959f`，2026-09-19 19:13）
> **实现侧快照（审计基准）**: 审计时冻结于 `/dev/shm/astrocs_conf3/snap/`（审计期间工作树被并行分片持续修改，行号一律以该快照为准；收尾已按硬约束清理，见 §6-5b）
> 快照 md5:
> `module_adapters.cpp f6eba4bcc27ec2f5a316e3db2e7c70b5`（9042 行，mtime 2026-09-19 19:28:37 +0800）/
> `upm.cpp 5d7ba55fd8c30004faf5b9f40c720be7`（2732）/
> `sampler.cpp c1dab5f3e80f4f56d91b8756ba6d796d`（1520）/
> `rejection.cpp 3ec04739653cee1dd0e3c63a25952969`（2857）/
> `coverage.cpp 973affcc64ec377988a2537d26cdeda5`（454）/
> `integrate.cpp 9c6b5338c688ec454a5d298547b55d1c`（81）/
> `stage2_common.cpp 832c684ee4d878fda715516ace95a47f` /
> `p2_session.cpp e642eb3fb1bd2404cf486fdbf2c886f8`
> 硬约束遵守: 未改任何生产代码/文档；零 git 写；未跑 ninja/cmake/ctest；TMPDIR=`/dev/shm/astrocs_conf3`（收尾清理）。
> 只读运行的校验器: `tools/config_consistency_check.py`、`tools/check_module_readmes.py`、`tools/check_p2_symbol_map.py`（结果见 §2-017）。

---

## 1 汇总统计

### 1.1 陈述清点

| 项 | 数 |
|---|---|
| 枚举到的规定性陈述（公式/常数/单位/定义域/顺序/默认值/schema 语义） | **74** |
| 判定「符合」 | **47** |
| 判定「不符」 | **11** |
| 判定「未实现」 | **4** |
| 判定「规范歧义 / 规范内部打架」 | **8** |
| 判定「待定」（证据不足，如实登记） | **4** |
| 其中属「规范本身错/陈旧/自相矛盾」（另一类，非实现缺陷） | **9** |
| 合计需行动条目（§2 逐条展开） | **27** |

> 计数口径: 逐条编号见 §4 全量清点表（`S-xxx`）；§2 的 27 条为需要行动的非符合条目（`CONFORM-SWEEP-3-NNN`）。
> 同一陈述可同时命中两栏时只计一次，取更严重者。「规范本身错/陈旧」条目同时在 §3 展开。

### 1.2 按缺陷类别分组（C1–C7）

| 类 | 定义 | 本分片命中 | 条目 |
|---|---|---|---|
| **C1 口径不符** | 规范规定了公式/域/单位，实现用了别的 | **5** | 003 004 013 020 022 |
| **C2 常数不符** | 规范给了常数，实现用了别的 | **1** | 001 |
| **C3 未接线** | 规范要求的步骤在生产链路不存在 | **4** | 011 014 016 021 |
| **C4 默认值不符** | 规范/工具默认与生产默认不一致 | **3** | 007 008 009 |
| **C5 硬编码绕过** | 用硬编码替代本应计算/配置的量 | **2** | 005 006 |
| **C6 有实现无调用** | 算法实现了但零调用者 | **3** | 010 015 011 |
| **C7 合同不符** | 实现与 `contracts/**`/ALG 冻结表 schema 不一致 | **3** | 002 012 017 |
| **规范侧问题（另一类）** | 规范本身错/陈旧/自相矛盾 | **9** | 018 019 020 022 023 024 025 026 027 |

> 说明: 004 同时命中 C1 与 C3；011 同时命中 C3 与 C6；020 同时命中 C1 与「规范自相矛盾」；022 同时命中 C1 与「规范陈旧」。分组表按主类计一次，条目内注明次类。
> **诚实声明**：本分片**未**把任何「符合」写成「不符」。`F_instr`（PSF 域通量 vs 5×5 盒和 m00）与 `fwhm_px=2.3548σ → Moffat4 FWHM` 两个已知实例**不在本分片范围**（Phase1 测光/星检），已由 CONFORM-SWEEP-1-001/004 承载；本分片在其**下游**找到了同源的传播面（见 004 影响节）。

---

## 2 逐条明细（需行动条目）

### CONFORM-SWEEP-3-001 [C2] UPM 收敛容差：规范冻结 `1e-6`，生产节点链改 `1e-3` + 新增相对判据，未走 SCI/合同变更

- 规范：`docs/algorithms/v6/frozen/01_NUMERIC_THRESHOLD_FREEZE.md:45`「| FZ-UPM-CONVERGENCE | UPM 迭代/收敛参数（继承 UPM_SOLVER.md，不改） | Huber δ=1.345; max_iter=100; **tol=1e-6**; sigma_floor=1e-3; zero_anchor=1e-3 | 1 | **FROZEN** | … | 改动收敛参数或 sigma_floor → 与既有合同不一致 | 改动 tol/σ_floor → rc!=0 |」
- 规范：`docs/algorithms/PHASE2_UPM_IMPL.md:379`「| tolerance | **1e-6**（收敛门 max_dM/max_dC） | :228、收敛判据 ：872 |」；`:400-401`「**本表数值与公式为冻结面：任何修改必须走 SCI/合同变更，禁止在实现或测试内就地放宽。**」
- 规范：`docs/contracts/DATA_SEMANTICS.md:1874`「| tolerance | double | **1e-6** | 收敛容差（h:78） |」；`:1762`「| tolerance | 1e-6（无 config 覆盖键） | :188 |」
- 实现：`lib/infrastructure/scheduler/src/module_adapters.cpp:5083-5084`「`uc.tolerance = 1e-3;` / `uc.tolerance_relative = 1;`」（紧邻注释 `:5078-5082`「RELEASE-02 P2a-3（科学行为变更）：绝对 1e-6 在 max|M|~3e15 时低于 ULP(0.5) 5-8 个数量级，原理上不可达…生产改为相对判据 tolerance_relative=1 + 1e-3（论证见 reports/RELEASE-02/fix-p2a-seam.md §4）」）
- 对照实现（另一条生产路径）：`lib/phase2_session/p2_session.cpp:204`「`uc.tolerance = 1e-6;`」（无 `tolerance_relative`，取 `upm.h:113` 默认 0 = 绝对判据）
- 判定：**不符**
- 证据：`upm.cpp:991-1001`「`double tol_M = cfg.tolerance; … if (cfg.tolerance_relative) { … tol_M = cfg.tolerance * std::max(scale_M, 1.0); }`」——`tolerance_relative=1` 时判据变为 `1e-3·max(scale,1)`；生产 `max|M|~3e15` ⇒ 实际收敛门 ≈ **3e12 ADU**，比冻结绝对门 1e-6 宽 **18 个数量级**。`grep -rn "tolerance_relative" docs/ contracts/` = **0 命中** ⇒ 该字段与其生产取值在任何 SCI/ALG/合同文档中都不存在，`FZ-UPM-CONVERGENCE` 的「任何修改必须走 SCI/合同变更」条件未满足。
- 影响：UPM IRLS 提前判「已收敛」，`p2_upm_convergence.converged=1` 在残差仍达 ~1e12 ADU 时即置位（`upm.cpp:1000`）；`p2_upm_model.json` 的 `converged`/`iterations` 是下游唯一的收敛可见面（`module_adapters.cpp:5192-5194`），因此「不收敛」在数据面上**不可见**。同时两条生产路径（session 与 node chain）对同一冻结常数给出不同解 —— 同一输入经 `p2_session` 与经 node chain 会得到不同 `C` 场与不同 `model_hash`。
- 修复面：`module_adapters.cpp:5083-5084` 恢复 `uc.tolerance = 1e-6; uc.tolerance_relative = 0;`（或按 ENGINEERING_SPEC §3 走变更 claim 把 `tolerance_relative` 与 1e-3 写入 `PHASE2_UPM_IMPL.md §13` + `DATA_SEMANTICS §25` + `FZ-UPM-CONVERGENCE` 并递增版本）。**只登记，不改。**

### CONFORM-SWEEP-3-002 [C7] `P2UpmBuildConfig.tolerance_relative` 字段未入任何合同表（schema 面漏登记）

- 规范：`docs/contracts/DATA_SEMANTICS.md:1860-1880`（P2UpmBuildConfig 字段表，逐字段 dtype/默认/锚）与 `:1966-1967`（字段清单）——**无 `tolerance_relative` 行**。
- 规范：`docs/contracts/PUBLIC_API.md:1827`「覆盖键仅 max_iterations/huber_delta/smoothing_lambda 三个」
- 实现：`lib/algorithms/coverage/include/astro/phase2/upm.h:113`「`int tolerance_relative = 0;`」；序列化 `upm.cpp:1197`「`j["tolerance_relative"] = m->cfg.tolerance_relative;`」；反序列化 `upm.cpp:1303`；生产注入 `module_adapters.cpp:5084/:5118`
- 判定：**不符**（C7；与 001 同源不同面）
- 证据：`grep -rn "tolerance_relative" docs/ contracts/` = 0 命中；而该字段进入 `model_hash` payload 之外的结构体并改变求解判据（`upm.cpp:993-1001`）与模型 JSON（`upm.cpp:1197`）。合同「字段清单 + 默认值表」与实现不同步。
- 影响：合同消费者（schema 校验、Python Oracle、跨版本 open 兼容）无法知道该字段存在；`p2_upm_open` 对旧文件用 `j.value("tolerance_relative", 0)` 静默取 0，使「同一 `.bin` 在不同版本重开得到不同收敛语义」这一风险不可见。
- 修复面：`docs/contracts/DATA_SEMANTICS.md:1860-1880`（增行）与 `docs/contracts/PUBLIC_API.md:1827`（覆盖键集）——或删除该字段。**只登记，不改。**

### CONFORM-SWEEP-3-003 [C1] `p2_upm_normalized_weights` 用绝对门 `s>1e-12` 判「是否归一化」⇒ 生产 `control_ivar` 尺度下全部权重归 0

- 规范：`docs/science/PHASE2_UPM.md:56`「`w_cell = w_UPM / Σ_cell w_UPM × control_reliability,  Σ_cell w_cell = control_reliability`」——**归一化定义域是「Σ 有限且 >0」，规范未给任何绝对阈值**。
- 规范：`docs/algorithms/PHASE2_UPM_IMPL.md:389`「归一化门 **s>1e-12**（:555/:1341）」——ALG 文档把绝对门写成了规范（此为本条的第二面，见 §3-019）。
- 实现（公共 API）：`lib/algorithms/coverage/src/upm.cpp:1646`「`out_norm[i] = (s > 1e-12) ? raw[i] / s * rel : 0.0;`」
- 实现（内部 build 路径，已修）：`upm.cpp:646`「`if (s > 0.0 && std::isfinite(s))`」+ 注释 `:629-643`「FIX-UPMSCALE（RELEASE-02）：尺度无关判据…旧门 sums[ck] > 1e-12 是绝对阈值：生产 control_ivar 中位 ≈5.6e-22（Σ 中位 ≈5.1e-21）使全部 control 判假 ⇒ 100% 权重清零 ⇒ C 场恒 0」
- 判定：**不符**
- 证据：生产 `control_ivar` 中位 ≈ 5.6e-22（`upm.cpp:633-637` 注释实测值），单 control 的 `Σ_cell w_UPM ≈ 5.1e-21 ≪ 1e-12` ⇒ `p2_upm_normalized_weights` 对**每一个**样本返回 0.0，而正确值应为 `≈ rel/n`。同一公式在 build 内部已按尺度无关判据修好（`upm.cpp:646`），公共 API 未同步 ⇒ 同一规范公式在生产尺度上给出两套互斥结果（0 vs rel/n）。
- 影响：`p2_upm_normalized_weights` 是 `API-P2-UPM-001` 公共消费面（`upm.h:176-177`、`upm README §唯一生产源符号清单`）。任何按合同调用它取 `w_cell` 的消费者（报告/门/Oracle/未来的绝对权换算）会拿到全 0 权重，等价于「该 control 无观测」。FIX-UPMSCALE 报告的结论「归一化表达式本身不变，只把判据换成尺度无关形式」**只在 build 内部成立，公共 API 仍是旧行为**。
- 修复面：`lib/algorithms/coverage/src/upm.cpp:1646` 改为 `(s > 0.0 && std::isfinite(s))`（与 :646 同判据）；同步订正 `docs/algorithms/PHASE2_UPM_IMPL.md:389`。**只登记，不改。**

### CONFORM-SWEEP-3-004 [C1/C3] `ivar` 产品缺失时用帧级 SNR 链**合成 variance/ivar 产品**并置 `uncertainty_available=true`，违反 §30.1 unavailable 规则 2

- 规范：`docs/contracts/DATA_SEMANTICS.md:2400-2407`「**unavailable 规则（fail-closed，唯一出口）**: 下列任一 → **不写 variance/ivar 子产品** + manifest `uncertainty_available=false` + diagnostics 标红计数，禁止用 support/snr²/常量 0 伪 variance：… 2. **fallback 发生**（`legacy_allow_weight_fallback=true` 且 `ivar_product_missing>0`，§20.4 rc=7 门的显式降级路径）——**混合帧集（部分帧 support 降级）同样整体 unavailable**」
- 规范：`docs/science/INTEGRATION.md:126`「`weights[i]`=逐候选科学权重（可空=等权 1.0），来源为 SCI-NOISE ivar/SCI-UPM 权重链」
- 实现：`module_adapters.cpp:6612-6675`——`ivar` 产品缺失时构造 `FrameWeightInput`（`in.sparse = nullptr`，`:6620`），调 `compute_inverse_variance_weights`（`:6649-6651`），成功后 `:6669`「`uncertainty_available = true;   // SNR 权重 = 合法逆方差面`」；`p2_op_write` `:7239`「`if (uncertainty_available) flags |= AIO_HIPS_PRODUCT_VARIANCE | AIO_HIPS_PRODUCT_IVAR;`」→ `:7315-7324` 写 variance tile
- 判定：**不符**（C1 主类；同时构成 C3——§30.1 的 fail-closed 出口未接线）
- 证据（代码路径推导 + 产品面事实）：生产 L4 归一化帧只有 `signal/` 与 `support/` 两个子目录（`ls run/RELEASE-02/L4-rebuild/norm/t2_m1_red/M42_M1_T2_flying_dutchman-20251212_012404-300S-Red/` → `manifest.json p1_final.json p1_stack.json p1_wcs.json signal support`），**无 `ivar/`、无 `variance/`** ⇒ `aio_hips_open(..., AIO_HIPS_RD_IVAR)` 必失败 ⇒ `ivar_missing = N` ⇒ 走 SNR 链 ⇒ `uncertainty_available=true` ⇒ variance/ivar 子产品落盘。此时 `variance = 1/W = F_ref²/Σ SNR_k²`（`module_adapters.cpp:7290-7294` 写 `varnum = cov²/W`）是**逐帧常数权的合成**，完全不含逐像素噪声；而 §30.1 明文把「ivar 缺失」列为必须整体 unavailable 的情形。实现用「SNR 权重 = 合法逆方差面」的自我声明绕过了该规则（`fallback` 恒 false，`:6560`「legacy 等权降级成功路径已删除（恒 false）」⇒ 规则 2 的字面前提 `legacy_allow_weight_fallback=true` 永不成立）。
- 影响：交付马赛克的 `variance`/`ivar` 层与 `ASTROCS_UNCERTAINTY_AVAILABLE=true` 声明了一个**不含逐像素噪声项**的方差面；下游任何「逆方差加权/SNR 显著性/深度」声明都建立在帧级常数上。且绝对尺度含 `F_ref²`，而 `F_ref` 正是本轮的 `F_instr` 缺陷量（`signal/properties: ASTROCS_REFERENCE_FLUX=3492.3739624023438`，由 Phase1 5×5 盒和口径给出，见 CONFORM-SWEEP-1-004）⇒ **F_instr 的口径错通过 `w=SNR²/F_ref²` 直接进入 variance 产品的绝对标度**（相对权重因组内公共 `F_ref` 相消而幸免）。
- 修复面：`module_adapters.cpp:6669`（SNR 链不得置 `uncertainty_available=true`）或 `:7239/:7317`（variance/ivar 落盘门加 `ivar_product_missing==0` 条件）；如需保留 SNR 链为科学方差面，须按 ENGINEERING_SPEC §3 走变更 claim 修订 `DATA_SEMANTICS §30.1` 规则 2。**只登记，不改。**

### CONFORM-SWEEP-3-005 [C5] sampler catalog veto 阈值/半径硬编码 `10.0`/`0.012`，不消费已声明且已默认的 `cfg.star_mask_*`

- 规范：`lib/algorithms/coverage/include/astro/phase2/sampler.h:58-59`「`double star_mask_snr_factor;  // 默认 10.0` / `double star_mask_radius_deg;  // 默认 0.012`」+ `:56-57` 注释「星点掩膜（10_sampling.md §4.1）：星表中 snr > factor × 帧级 SNR 中位数的星按 radius 膨胀进 star_mask（P0-08；**原 catalog veto 硬编码整改**）」
- 规范：`lib/algorithms/sampling/README.md:123`「| DISP-P2SMP-004 | catalog veto 阈值 10×frame_snr_med 与半径 0.012° 硬编码，**未入 P2SamplerConfig** 配置面 | sampler.cpp:849-850 |」
- 实现（配置面已存在）：`lib/algorithms/coverage/src/sampler.cpp:318-319`「`c.star_mask_snr_factor = 10.0;      // 与 catalog veto 同口径` / `c.star_mask_radius_deg = 0.012;     // 与 veto 半径同口径`」；修补 `:513-514`
- 实现（veto 现场仍硬编码）：`sampler.cpp:884-885`「`const double thr = 10.0 * frame_snr_med[frame_id];` / `const double rad = 0.012;`」
- 实现（另一处正确消费）：`sampler.cpp:1133`「`const double thr = cfg.star_mask_snr_factor * frame_snr_med[i];`」、`:1148`「`cap.radius_deg = cfg.star_mask_radius_deg;`」
- 判定：**不符**（C5）
- 证据：同文件内同一物理量有两个消费者，一个走配置（:1133/:1148），一个走字面量（:884-885）。`sampler.h:56-57` 自称「原 catalog veto 硬编码整改」已完成，但整改只落到 star_mask 路径；catalog veto 本身仍被字面量钉死 ⇒ 调用方设 `star_mask_snr_factor=5` 只改变 star_mask，不改变 veto，两个「同口径」注释（:318-319）与实际行为矛盾。
- 影响：DISP-P2SMP-004 登记的 schema 缺口**未闭合**（且规范描述已过期——字段其实已存在，缺的是消费）。生产调参（收紧/放宽 veto）在 node chain 上完全无效（`p2_op_sample` 本就不透传，见 014），在 stage2 工具上也只能改 star_mask。
- 修复面：`sampler.cpp:884-885` 改用 `cfg.star_mask_snr_factor`/`cfg.star_mask_radius_deg`；同步订正 `lib/algorithms/sampling/README.md:123` 与 `docs/algorithms/PHASE2_SAMPLER.md:398` 的 DISP 文本。**只登记，不改。**

### CONFORM-SWEEP-3-006 [C5] `param_covariance_included` 硬编码 `false` ⇒ 逐像素方差面（P2b-1）永不被积分消费

- 规范：`docs/contracts/DATA_SEMANTICS.md:2387-2392`「**合成公式**（weight_mode=2 科学默认…）: `ivar_mosaic(p) = W(p) = Σ_i ivar_i(p)` / `variance_mosaic(p) = 1 / W(p)`」+ 一致性锚 `:2395-2396`「`variance = Σ_i w_i²·v_i / W²`」
- 规范：`docs/algorithms/v6/frozen/01_NUMERIC_THRESHOLD_FREEZE.md:62`「FZ-FORMULA-COV-PROP | covariance 传播 | `C_out = R C_in R^T`; 标量 `c^T C_in c` | … | **禁止从权重标量反推 variance**」
- 实现：`module_adapters.cpp:5866`「`const bool param_cov_included = false;`」；`:5867-5868`「`const bool uncertainty_available = any_var_ok && all_pixel_noise && param_cov_included;`」；`:5923`「`(*man)["param_covariance_included"] = param_cov_included;`」
- 判定：**不符**（C5；已在本轮登记为已知实例，本分片复核并补出**下游后果**）
- 证据：`p2_op_integrate` 的 priority-1 权重面 `:6561-6566`「`if (corr_var_ready) { weight_basis = "per_pixel_corrected_variance"; … }`」，而 `corr_var_ready` 的前置 `:6528-6537` 要求 `cor_doc["uncertainty_available"]==true`，后者因 `:5866` 恒 false ⇒ **P2b-1 花费的逐像素 Var(corrected) 计算与落盘（`p2_corrected_var_f<fid>.bin`）在生产上永不被积分节点消费**，权重面永远退回 ivar/帧级 SNR 链。这是一条「有实现无调用」的**数据面** C6，与 004 叠加。
- 影响：整个 P2b-1/P2b-2 逐像素方差链在生产是死代码路径；积分权重退化为帧级常数（见 004），逐像素噪声差异（星点/背景/坏像素）不进入权重。
- 修复面：`module_adapters.cpp:5866` 接入 W2 加性模型的参数协方差（`p2_upm_ma_param_cov` 只服务 V6 乘加面，W2 面无 C_θ API）——或显式登记该链为 NOT_WIRED 并从 `p2_corrected.json` schema 中降级。**只登记，不改。**

### CONFORM-SWEEP-3-007 [C4] `reject_profile` 生产默认：node chain `astrocs_adaptive_pixel` vs stage2 工具 `wbpp_2_9_1`

- 规范：`docs/development/CONFIG_SCHEMA.md:25`「`profile(astrocs_adaptive_pixel(生产默认,自研)|wbpp_2_9_1(对照档)|wbpp_current alias|astrocs_adaptive)`」（该文件自述「规则：C++ struct 默认值、parser 默认值、JSON schema、template config、docs、tests **必须一致**」）
- 规范：`docs/science/REJECTION.md:21`「| `profile` | `astrocs_adaptive_pixel`（**生产默认，AstroCS 自研**）/ `wbpp_2_9_1`（对照档）/ … | `plan` |」；`:47`「生产默认 auto + profile = astrocs_adaptive_pixel」；`:38`「`wbpp_2_9_1`（**对照档**…）」
- 规范：`contracts/data/phase2_uncertainty_rejection_provenance_v1.json:67`「`cfg.reject_profile（canonical=astrocs_adaptive_pixel 自研档；wbpp_current / wbpp_2_9_1 为对照档…）`」
- 实现（node chain）：`module_adapters.cpp:6034`「`doc.value("reject_profile", std::string(P2_PROFILE_ASTROCS_ADAPTIVE_PIXEL));`」⇒ 符合
- 实现（stage2 工具）：`lib/algorithms/coverage/src/stage2_common.cpp:257-258`「`cfg->reject_profile = rj.value("profile", std::string("wbpp_2_9_1"));`」，注释 `:255-256`「版本化 profile（**wbpp_2_9_1 冻结**；wbpp_current 仅 migration alias…）」⇒ **不符**
- 判定：**不符**（C4）
- 证据：同一生产语义的默认值在两条链路相反。工具侧默认取「对照档」，与 SCI §4/§5/§7 三处「生产默认 = astrocs_adaptive_pixel」直接冲突；且 SCI §4 的 AUTO 路由阈值（n≤3→none）只在 pixel 档成立，工具默认档在 n=3 时会走 percentile（WBPP 路由 n<6），与 SCI §5 的生产语义不同。附带：`lib/algorithms/rejection/README.md:43` 只描述 `p2_reject_plan_resolve` 的 WBPP 路由，未说明生产默认档，加剧歧义。
- 影响：用 `astrocs-stage2` 工具复现/回归会得到与交付 node chain **不同的排异方法选择**（n=3、n=6..7、n≥16 边界全部分叉），使工具链证据不能作为生产链的符合性证据。
- 修复面：`stage2_common.cpp:258` 默认改 `P2_PROFILE_ASTROCS_ADAPTIVE_PIXEL`（与 node chain/SCI/CONFIG_SCHEMA 一致）。**只登记，不改。**

### CONFORM-SWEEP-3-008 [C4] `underdetermined_n` 默认：CONFIG_SCHEMA 说 `2`，stage2 工具对 pixel 档默认 `3`

- 规范：`docs/development/CONFIG_SCHEMA.md:26`「`underdetermined_n(2)`」
- 规范：`docs/science/REJECTION.md:48`「n ≤ 3 → none（保守：不排异 + 直接加权积分；provenance 记 underdetermined_no_rejection）」；`docs/algorithms/PHASE2_REJECTION.md`（`p2_reject_plan_resolve` 默认 2，pixel 档 3）
- 实现：`stage2_common.cpp:272-277`「`const std::uint32_t undet_default = (cfg->reject_profile == "astrocs_adaptive_pixel") ? (std::uint32_t)3 : (std::uint32_t)2; cfg->reject_underdetermined_n = rj.value("underdetermined_n", undet_default);`」；`rejection.cpp:1155-1162`「`std::uint32_t undet_default = 2u; if (pixel_profile) { undet_default = (req->request == P2_REJECT_EXTREME_VALUE_PRIOR_SIGMA) ? 1u : 3u; }`」
- 判定：**不符**（C4；规范侧 CONFIG_SCHEMA 表未随 pixel 档更新）
- 证据：实现内部自洽（pixel 档 3），但「单一事实来源」的 CONFIG_SCHEMA 仍写 `underdetermined_n(2)`。按 CONFIG_SCHEMA 自述的一致性规则，三处默认值必须一致，实测不一致。
- 影响：配置模板/文档消费者按 2 写配置时，pixel 档实际语义为 3 ⇒ n=3 像素的「是否排异」在配置面上不可预期。
- 修复面：`docs/development/CONFIG_SCHEMA.md:26` 改为 `underdetermined_n(2；astrocs_adaptive_pixel 档 3)`。**只登记，不改。**

### CONFORM-SWEEP-3-009 [C4] `smoothing_lambda`：CONFIG_SCHEMA/工具 `auto→0.1` vs 模块/ALG 规范 `0.0（默认关闭）`

- 规范（A）：`docs/development/CONFIG_SCHEMA.md:19`「`huber_delta(1.345) smoothing(auto→0.1) zero_anchor_weight(1e-3)`」
- 规范（B）：`docs/algorithms/PHASE2_UPM_IMPL.md:382`「| smoothing_lambda（λs） | **0.0（默认关闭平滑）** | :225/:245 |」；`lib/algorithms/upm/README.md:138`「`zero_anchor_weight=1e-3 / smoothing_lambda=0.0 默认关`」；`upm.h:75`「`double smoothing_lambda;      // 图平滑权重（默认 0=关闭）`」
- 实现（A'）：`stage2_common.cpp:140`「`cfg->smoothing_lambda = 0.1;`」（`smoothing:"auto"` 时）；`stage2_common.h:40`「`double smoothing_lambda = 0.0;`」
- 实现（B'，生产 node chain）：`module_adapters.cpp:5094`（只设 `sigma_floor/zero_anchor_weight/grid`，**不设** `smoothing_lambda`）⇒ 取 `upm.cpp:246` 默认 **0.0**；`p2_session.cpp:207` 同 ⇒ 0.0
- 判定：**不符**（C4；同时是「规范打架」，见 §3-023）
- 证据：生产 node chain 与 session 都是 0.0（符合 ALG/模块规范），只有 stage2 工具在显式写 `smoothing:"auto"` 时取 0.1。两条规范来源给出互斥默认值，且 `smoothing` 键在 node chain 上**完全不可达**（`module_adapters.cpp:5109` 只认 `upm.smoothing_lambda`，不认 `smoothing:"auto"`）。
- 影响：同一 stage2 配置在工具与 node chain 上得到不同 UPM 估计量（λs>0 时按 SCI §7(iii) 会改变估计量）；反向（λs 未同步换算 λ0）正是 SCI §10 禁止的变化。
- 修复面：`docs/development/CONFIG_SCHEMA.md:19` 与 `stage2_common.cpp:140` 二者取一（建议统一为 0.0 并删除 auto→0.1，或把 auto→0.1 写入 ALG §13 冻结表）。**只登记，不改。**

### CONFORM-SWEEP-3-010 [C6] `p2_upm_ma_c_out` 零调用者（V6 乘加面），且其数学方向已被 2026-09-19 裁决取代

- 规范：`lib/algorithms/upm/README.md:208-210`「参数协方差 `C_theta=(JᵀWJ)^-1` 与 `C_out=C_stat+J_out C_theta J_outᵀ`（`FZ-FORMULA-COV-PROP`）、欠定/秩亏/κ 超限 fail-closed」；`lib/algorithms/upm/memory.md:148-150` 同
- 规范（新裁决）：`docs/science/PHASE2_UPM.md:182`「**本期决议：纯加性**（负责人 2026-09-19 裁决，claim `FIX-SCI-SNR-CANON-001`）：… **取代** `FIX-A-UPM-001` 提议的 `y_k(x)=g_k·s(x)+b_k(x)` 乘性方向。`g_k ≡ 1` **本期不启用**」
- 实现：`lib/algorithms/coverage/src/upm.cpp:2315`「`int p2_upm_ma_c_out(const void* model, const double* J_out, …)`」（V6 段，`extern "C"`）
- 判定：**未实现**（C6；按现行裁决应显式退役）
- 证据：`grep -rn "p2_upm_ma_c_out" lib/ tests/ tools/ --include=*.cpp --include=*.h` 在**生产/测试面 0 命中**（仅 `run/v6/IMPL-P2-UPM-001/mutations/*.cpp` 的副本各 1 处）。整组 `p2_upm_ma_*` 公共面（`upm.h` V6 段）同样在生产零调用：生产 `p2_op_upm_fit/apply` 只用 W2 加性面（`p2_upm_build_geo/p2_upm_calibrate_block`）。
- 影响：合同面（`ALG-P2S-UPM.1..8`、`FZ-FORMULA-COV-PROP`）声称的乘加分离求解与 `C_out` 传播在生产链上不存在；同时 `C_out` 正是 006 缺失的「参数协方差项」的候选实现——但它服务的是已被取代的乘加模型，**不能**直接补进 W2 加性链（`g≡1` 时 `J_out` 结构不同）。这是一个需要裁决的三角：006（缺 C_θ）↔ 010（有 C_θ 但模型被取代）↔ 004（无 C_θ 时 variance 产品仍被写出）。
- 修复面：`lib/algorithms/upm/README.md:198-219`（V6 追加节）与 `docs/algorithms/v6/phase2-surface/ALG-P2-SURF-UPM.md`——按 `FIX-SCI-SNR-CANON-001` 登记退役/休眠；或在 W2 加性面补 `C_theta`。**只登记，不改。**

### CONFORM-SWEEP-3-011 [C3/C6] `sky_plane` 默认 `enabled=true` 构建并落盘，但默认 `additive_mode="c"` 从不施加 δ ⇒ 产物无消费方

- 规范：`docs/science/UNIFIED_SCIENCE_MODEL.md:122`「**UNRESOLVED（跨文档口径冲突）**：… 本文件 §2 的 sky_plane（稀疏样条天光面）与 docs/science/PHASE2_UPM.md §1/§5（纯加性 8×8 control cell…）模型不同…**登记 UNRESOLVED，上呈负责人裁决**」
- 规范：`docs/science/PHASE2_UPM.md:182`「…原「设计-实现模型冲突」UNRESOLVED **关闭**：`10_sampling.md`/`11_upm.md`/`UNIFIED_MODEL.md` 的目标态表述同步订正为纯加性；`OPEN-P2S-02` 仅剩数据面/schema 表示待合同流程」
- 实现：`module_adapters.cpp:5241`「`const bool sky_enabled = sp_cfg.value("enabled", true);`」→ `:5282-5305` `p2_sky_plane_build` + `p2_sky_plane_save`（落 `p2_sky_plane.bin`）；施加面 `:5503`「`std::string additive_mode = seam_cfg.value("additive_mode", std::string("c"));`」+ `:5509-5517`（`sub_delta` 仅 `delta`/`both` 为真）
- 判定：**未实现**（C3 主类；同时 C6——`p2_sky_plane.bin` 在默认路径无消费方）
- 证据：默认路径 `sub_c=true, sub_delta=false` ⇒ `p2_sky_plane_eval_delta_block` 从不调用（`:5736` 只在 `sub_delta && sky_guard.m` 时进入）。而 `p2_op_upm_fit` 默认仍付出全量稀疏样条拟合 + `aio_upm` 落盘的代价。UNIFIED_SCIENCE_MODEL 的 UNRESOLVED 与 PHASE2_UPM 的「已关闭」互相矛盾（§3-024），使「sky_plane 是否属生产科学链」无单一权威答案。
- 影响：交付包内含一个**无 SCI 权威、未被施加**的科学模型产物；性能面（`fix-sky-report` 域）付出成本而无科学收益；若消费者误读 `p2_corrected.json` 的 `sky_plane_applied=true`（见 012），会把「已加载」读成「已扣除」。
- 修复面：`module_adapters.cpp:5241` 默认改 `false`（并把「是否施加」与 `additive_mode` 绑定），或把 `sky_plane` 提升为有 SCI 权威的独立模块（补 `docs/science/` 条目 + `ALG`）。**只登记，不改。**

### CONFORM-SWEEP-3-012 [C7] `sky_plane_applied` 在只加载不施加时置 `true`，与 `delta_subtracted=false`/`sky_plane_mode="none"` 同文档矛盾

- 规范：`contracts/data/phase2_uncertainty_rejection_provenance_v1.json`（provenance 键语义）与 `docs/contracts/DATA_SEMANTICS.md:2400-2402`「…manifest `uncertainty_available=false` … 禁止用 support/snr²/常量 0 伪 variance」——provenance 键必须如实表达**实际施加**的量。
- 实现：`module_adapters.cpp:5856`「`const bool sky_applied = (sky_guard.m != nullptr);`」；`:5880`「`{"sky_plane_applied", sky_applied}`」；`:5890`「`{"sky_plane_mode", delta_applied ? "delta_to_B_ref" : "none"}`」；`:5887`「`{"delta_subtracted", delta_applied}`」；`:5920`「`(*man)["sky_plane_applied"] = sky_applied;`」
- 判定：**不符**（C7）
- 证据：默认路径下（`additive_mode="c"`）三个键同文档并列为 `sky_plane_applied=true`、`delta_subtracted=false`、`sky_plane_mode="none"`、`additive_combination="raw_minus_C"`。同一 JSON 内「applied=true」与「subtracted=false / mode=none」互斥，机器消费者按键名取值必被误导。
- 影响：任何按 `sky_plane_applied` 判断「本次 mosaic 是否做了天光面扣除」的消费者/报告/门会得出相反结论；与 `fix-sky-report` 讨论的「degraded 显式登记」纪律相悖（`sky_plane_degraded` 仅在失败时置位，成功但未施加时不置位）。
- 修复面：`module_adapters.cpp:5856` 改为 `sky_applied && delta_applied`（或改名 `sky_plane_loaded` + 新增 `sky_plane_delta_applied`）。**只登记，不改。**

### CONFORM-SWEEP-3-013 [C1] 全拒容错域：规范「域写死 n=4」，实现 `n <= 4`

- 规范：`docs/science/REJECTION.md:77`「全拒容错（**域写死 n=4**）→ UNDERDETERMINED 全接受 (rejection.cpp:1860-1874)」；`:95-96`「**全拒容错域（n=4，SC-005）**：`n=4` 且方法核全拒 ⇒ `UNDERDETERMINED` 全接受…；`n ≥5` 全拒 ⇒ `ALL_REJECTED`（无 n 相关例外）」；`:34-36`「该容错的**可达域恰为 n=4**：n≤2 已被白名单截走；奇数 n 的百分位带必含中位样本（不可全拒）；n≥5 全拒仍 `ALL_REJECTED`」
- 实现：`lib/algorithms/coverage/src/rejection.cpp:2111-2121`「`if (accepted_count == 0) { if (n <= 4) { … out->status = P2_STATUS_UNDERDETERMINED; } else { out->status = P2_STATUS_ALL_REJECTED; } }`」
- 判定：**规范歧义**（可达域等价，字面不符）
- 证据：实现条件为 `n <= 4`，规范文本为「域写死 n=4」。按 SCI §4 自身给出的可达性论证，n=3 时 percentile 带必含中位样本、n≤2 走白名单、minmax 的 `min_kept=4` 在 n<4 时走独立分支（`rejection.cpp:1815-1817`），故 `n<=4` 与 `n==4` 在本实现下**可达行为一致**。但代码与规范的**字面**不一致，且未来若新增 n=3 可全拒的方法，`n<=4` 会静默把 ALL_REJECTED 变成 UNDERDETERMINED（规范未授权）。
- 影响：当前无科学影响（可达域相同）；作为「阈值不变量」门的锚点，字面漂移会让门无法机械核对。
- 修复面：`rejection.cpp:2112` 改 `n == 4`（并在 SCI §4 保留可达性论证）。**只登记，不改。**

### CONFORM-SWEEP-3-014 [C3] `p2_op_sample` 不透传任何 sampler 配置键（17 字段仅 `cpu_workers` 可设）

- 规范：`docs/algorithms/PHASE2_SAMPLER.md:282`「**生产消费（唯一消费方 stage2.cpp）**：… sccfg 组装 :256-274（**14 字段显式透传**；control_k_corr 未透传…）」；`docs/contracts/DATA_SEMANTICS.md:1516-1537`（`P2SamplerConfig` 逐字段默认表，含「可空语义/约束」栏）
- 规范：`docs/development/CONFIG_SCHEMA.md:11-18`（`model:` 段列出 13 个 sampler 键，规则「C++ struct 默认值、parser 默认值、JSON schema、template config、docs、tests 必须一致」）
- 实现：`module_adapters.cpp:4928`「`P2SamplerConfig sc = p2_sampler_default_config();`」+ `:4931`「`sc.cpu_workers = std::max(1, doc.value("__workers", 1));`」——**仅此一处赋值**，此后直接调用 `p2_sample_controls_cached`（`:4935`）
- 判定：**未实现**（C3）
- 证据：`grep -n "sc\\." module_adapters.cpp`（`p2_op_sample` 段）只命中 `sc.cpu_workers` 与 `sc.control_grid_per_tile`（后者仅用于写 artifact，`:4988`）。stage2.json 中的 `model.background_*`/`min_samples`/`snr_search_radius_deg` 等键在 node chain 上**完全不可达**（无 parser、无 doc 读取）。CONFIG_SCHEMA 声明的「单一事实来源」在 node chain 上不成立。
- 影响：生产 node chain 的采样参数被钉死在编译期默认值；`min_samples=5`/`background_max_contamination=0.20`/`background_min_retained_fraction=0.60` 等直接影响控制点存活率的科学参数无法按数据集调整，且任何「调参后重跑」证据都不适用于交付链。
- 修复面：`module_adapters.cpp:4928-4931` 增 `model` 段解析（键集与 `CONFIG_SCHEMA.md:11-18` 对齐，逐字段显式赋值 + 域校验）。**只登记，不改。**

### CONFORM-SWEEP-3-015 [C6] `uc.snr_weight_mode` 是死配置字段

- 规范：`docs/algorithms/PHASE2_UPM_IMPL.md:386`「| support_power | 1.0（**仅 ablation 路径消费**） | :231/:240 |」；`docs/science/PHASE2_UPM.md:65`「legacy `snr²/(1+snr²)/unc²` 仅 `use_ivar_weight=0` ablation/诊断 (SNR-015)」
- 实现：`module_adapters.cpp:5075`「`uc.snr_weight_mode = 0;          // snr2_normalized`」；`p2_session.cpp:203` 同；`upm.cpp:244`（默认）/ `:1184`（序列化）/ `:1290`（反序列化）
- 判定：**符合**（规范已声明该面为 ablation；字段在生产 `use_ivar_weight=1` 路径上不被消费是**规范允许**的）——但**登记为观察项**：`grep -n "snr_weight_mode"` 在 `upm.cpp` 的求解段（:500-1100）**0 命中**，即该字段既不选权重也不选损失，纯 provenance。
- 证据：`upm.cpp:1612-1618` 的 production 分支只读 `cfg.use_ivar_weight`，不看 `snr_weight_mode`。
- 影响：无（字段名 `snr2_normalized` 会让读者以为权重式含 SNR²；实际 production 分支不含）。属命名/provenance 债。
- 修复面：`upm.h:71-92` 注释标注「production 路径不消费」，或从 `P2UpmBuildConfig` 移除。**只登记，不改。**

### CONFORM-SWEEP-3-016 [C3] 稀疏帧内 SNR 层未接线：规范要求「自动检测 + 默认稀疏路径 + 显式记录实际路径」，实现恒 `in.sparse=nullptr` 且无 `snr_path_effective`

- 规范：`ASTROCS_DESIGN.md:122-124`「`"sparse_snr_layer": true,  // 默认稀疏 SNR 层（负责人裁决 2026-09-19，见 change-claim FIX-SCI-SNR-CANON-001）；` / `// 三条路径 dense / sparse_reconstruct / frame_reconstruct，见 §3.4` / `"snr_path": "sparse_reconstruct"    // Phase2 面 SNR 路径键；默认稀疏重建为稠密`」
- 规范：`ASTROCS_DESIGN.md:256`「Phase2 **自动检测** 输入 HiPS 是否有稀疏 SNR 层：有 → 帧级×帧内；无 → 帧级。」
- 规范：`docs/plugins/algorithms_phase2/13_integration.md:18-22`「**SNR 路径（三条，配置文件 JSON 显式指定；默认稀疏）**：… 输入**无**稀疏层而路径为默认/`sparse_reconstruct` → 按帧级执行并**显式记录实际路径**（`snr_path_effective=frame_reconstruct` + 计数），**不得静默**；稀疏层存在但损坏/不可重建 → 明确失败」
- 规范：`docs/design/UNIFIED_MODEL.md:45`「| sparse_snr_layer | 帧内稀疏控制点 SNR 参考（Phase1 标准层；**默认稀疏路径要求默认产出**） | 帧内精细参考；Phase2 由它重建稠密 SNR |」；`docs/plugins/algorithms_phase1/07_noise_snr.md:112`「| `sparse_snr_layer` | **true** | —— | 是否产出稀疏帧内 SNR 层。**本期决议默认产出**（默认稀疏路径；负责人 2026-09-19 裁决）」
- 实现：`module_adapters.cpp:6620`「`in.sparse = nullptr;   // 稀疏 SNR 层尚未接入生产数据面`」
- 判定：**未实现**（C3）
- 证据：`grep -n "sparse\|snr_path" lib/infrastructure/scheduler/src/module_adapters.cpp` = **1 命中**（:6620）；`grep -rn "snr_path" lib/ --include=*.cpp --include=*.h` 只命中 Phase3 drizzle 的 FITS 入参（`hp_drizzle_api.cpp:191/247`），**Phase2 面无 `snr_path`/`snr_path_effective`**。`weight_chain.h:115-116` 已备好 `compose_actual_snr(frame_snr, intra_snr, …)` 与 `reconstruct_sparse_snr`（`:101-103`），但生产零调用。
- 影响：**实际 SNR 退化为纯帧级**（与「默认稀疏」裁决相反），且降级**静默**——违反 13_integration 明文「不得静默」；无 `snr_path_effective` 计数，事后无法从产物判定走的是哪条路径。由于帧级 SNR 参与 `w=SNR²/F_ref²`（见 004），整条权重链缺少帧内空间结构（星点/背景噪声差异）。
- 修复面：`module_adapters.cpp:6612-6630` 增稀疏层检测（AIO 稀疏层读取）+ `in.sparse` 填充 + `snr_path_effective` 与计数写入 `p2_integrated.json`/`p2_final.json`；或按 ENGINEERING_SPEC §3 走 claim 把默认路径改回 `frame_reconstruct`。**只登记，不改。**

### CONFORM-SWEEP-3-017 [C7] 规范点名的默认值一致性守护工具**已失效**（引用不存在路径），C4 类缺陷无机器门

- 规范：`docs/development/CONFIG_SCHEMA.md:3-4`「规则：C++ struct 默认值、parser 默认值、JSON schema、template config、docs、tests 必须一致；**一致性由 `tools/config_consistency_check.py` 校验（V14 交付）**」
- 实现：`tools/config_consistency_check.py`（只读运行）输出 `{"pass": false, "mismatches":[{"key":"env_missing","issue":"schema 文件不存在: …/工程控制/schemas/stage2.schema.json"},{"…":"template 文件不存在: …/工程控制/configs/stage2.template.json"},{"…":"header 文件不存在: …/lib/phase2/include/astro/phase2/stage2_common.h"},{"…":"source 文件不存在: …/lib/phase2/src/stage2_common.cpp"}]}`
- 实测：`ls lib/phase2` → 「没有那个文件或目录」；`ls 工程控制/schemas` → 同。真实路径为 `lib/algorithms/coverage/{include/astro/phase2,src}/stage2_common.{h,cpp}`。
- 判定：**不符**（C7；规范声明的机器门不存在）
- 证据：另两个被规范点名的检查器同样失效：`tools/check_p2_symbol_map.py` → `FileNotFoundError: docs/refactor/P2_SYMBOL_MAP.md`（文件不存在）；`tools/check_module_readmes.py` → 仅 `DOC-003_PASS`（5 模块 README 链接检查），**不校验行号/常数/默认值**。
- 影响：**C4 类（默认值不符）与「规范行号锚漂移」类缺陷在全仓没有任何机器门**——这正是 001/007/008/009/018 能长期存活的结构性原因；`CONFIG_SCHEMA.md` 自述的「单一事实来源」在工具层落空。
- 修复面：`tools/config_consistency_check.py`（更新 4 条路径）与 `tools/check_p2_symbol_map.py`（更新 MAP 路径或退役）；建议增「ALG 文档行数锚 vs 源文件实测」检查项。**只登记，不改。**

### CONFORM-SWEEP-3-018 [规范陈旧] 三个 ALG 文档的行数/行号锚全部过期（sampler/upm/rejection）

- 规范：`docs/algorithms/PHASE2_SAMPLER.md:14`「模块: `lib/algorithms/coverage/src/sampler.cpp`（**1156 行**）+ 唯一权威签名头 …（136 行）」；`:62`「## 3 逐符号锚（sampler.cpp **1156 行** / sampler.h **136 行**，2026-09-09 实测）」
- 规范：`docs/algorithms/PHASE2_UPM_IMPL.md:68`「## 3 逐符号锚（upm.cpp **1565 行** / upm.h **184 行**，2026-09-10 实测）」
- 规范：`docs/algorithms/PHASE2_REJECTION.md:58`「## 3 逐符号锚（rejection.cpp **2076 行** / rejection.h **329 行**，2026-09-09 实测）」
- 实现（快照实测）：`sampler.cpp` **1520 行**、`sampler.h` **286 行**；`upm.cpp` **2732 行**、`upm.h` **380 行**；`rejection.cpp` **2857 行**、`rejection.h` **579 行**
- 对照（已重锚的先例）：`docs/algorithms/PHASE2_COVERAGE.md:10-12`「coverage.cpp（**454 行，2026-09-17 LEDGER-DOC 复测**；旧记 239 行为迁移前行数）+ … coverage.h（**168 行**，同上；旧记 60 行）」；`docs/algorithms/PHASE2_INTEGRATION.md:49`「（integrate.cpp **81 行** / integrate.h **74 行**，2026-09-17 LEDGER-DOC 复测；旧记 76 行）」
- 判定：**规范歧义**（规范陈旧；属「规范本身错」类）
- 证据：三份文档的**全部**逐符号行号锚（例：PHASE2_SAMPLER §5.4「cvar :840-842」实测在 `sampler.cpp:875`；§5.5「veto :849-850」实测在 `:884-885`；PHASE2_UPM_IMPL §13「归一化门 :555/:1341」实测在 `:646/:1646`；§13「两处生产组装 module_adapters.cpp:3152-3176」实测在 `:5076-5097`）系统性偏移，且 sampler.h 字段数从 15 增到 17（见 025）。`PHASE2_SAMPLER.md:190-200` §5.4 关于 k_corr 的整段说明（「域外…不得 clamp…（:561-572）」「lookup 表 :94-97」）行号同样漂移。
- 影响：以 ALG 文档行号为锚的审计/复核/Oracle 会指向错误代码行；`docs/algorithms/PHASE2_*.md` 是「唯一权威」级文档，漂移直接削弱其权威性。本分片所有实现行号均以快照为准，未沿用文档锚。
- 修复面：`docs/algorithms/PHASE2_SAMPLER.md` / `PHASE2_UPM_IMPL.md` / `PHASE2_REJECTION.md` 按 `LEDGER-DOC` 先例机械重锚（§3 逐符号表 + §5/§13 行号）。**只登记，不改。**

### CONFORM-SWEEP-3-019 [规范陈旧] ALG 文档把「已被实现修掉的缺陷」写成现行规范（`s>1e-12` 归一化门）

- 规范：`docs/algorithms/PHASE2_UPM_IMPL.md:388-393`「**数值常数**：CG max_cg=200（:567）… **归一化门 s>1e-12（:555/:1341）**、per-control sums 门 den>1e-12（:681/:697/:706/:731/:748/:757…）…」；`:400-401`「**本表数值与公式为冻结面**」
- 实现：`upm.cpp:646` 已改尺度无关门（FIX-UPMSCALE，RELEASE-02），`upm.cpp:1646` 仍为 `1e-12`（见 003）
- 判定：**规范歧义**（规范陈旧 + 规范内部冲突：同一条「s>1e-12」对应两处实现，一处已改一处未改）
- 证据：`docs/algorithms/PHASE2_UPM_IMPL.md` 未登记 FIX-UPMSCALE；`reports/RELEASE-02/fix-upmscale-report.md` 是唯一记录。规范表把「绝对门」写成冻结数值常数，与 `docs/science/PHASE2_UPM.md:56` 的份额式定义（无阈值）冲突。
- 影响：读者按规范会把 `upm.cpp:646` 的尺度无关门当成违规；同时 `upm.cpp:1646` 的真违规被规范「合法化」。
- 修复面：`PHASE2_UPM_IMPL.md:389` 拆分为「build 内：尺度无关 `s>0 ∧ finite`（FIX-UPMSCALE）」与「`p2_upm_normalized_weights`：**应为同一判据，现状 1e-12 = 缺陷**」。**只登记，不改。**

### CONFORM-SWEEP-3-020 [C1/规范打架] `docs/science/REJECTION.md` §4「禁止 per-pixel effective 路由」与 §5「生产默认按逐输出像素几何 n」自相矛盾

- 规范：`docs/science/REJECTION.md:32`「`n` 为 planning 层 `nominal contributors`，**一次解析，禁止 per-pixel effective 路由**（`docs/science/REJECTION.md:16`）」
- 规范：`docs/science/REJECTION.md:47`「生产默认 auto + profile = astrocs_adaptive_pixel (AstroCS 自研，**按逐输出像素几何 n**)」；`:61`「像素候选栈域、**逐输出像素几何 n**；n ≥ 4 才声明排异能力」
- 实现：`module_adapters.cpp:6037-6048`（逐输出像素 `geom_n` 由 `support>0` 计数）→ `:6053`「`const P2RejectionPlan& plan = plan_cache.at(geom_n);`」
- 判定：**规范歧义**（同一 FROZEN 文档内两处互斥；实现跟 §5）
- 证据：§4 的禁令字面覆盖「per-pixel … 路由」，而 §5/§9a 把「逐输出像素几何 n」定为生产默认；§7「阈值不变量」又写「`auto` 路由**不依赖 per-pixel `n_eff`**」——三处对「per-pixel」的限定词不同（effective / 几何 / n_eff），读者无法机械判定实现是否合规。实现注释 `module_adapters.cpp:6030-6033` 明确按 §4.5 的「几何 n」路由并声明「**不得**用 frames.size()，**不得**用 eligible_count(n_eff)」。
- 影响：审计判据不可机械执行；若有人按 §4 字面把几何 n 路由判为违规，会把正确实现判红（反之亦然）。
- 修复面：`docs/science/REJECTION.md:32` 改为「禁止 per-pixel **effective（资格后）** 路由；允许并强制 per-pixel **几何** n 路由（§5）」。**只登记，不改。**

### CONFORM-SWEEP-3-021 [C3] `docs/science/REJECTION.md` 的 `NO_CANDIDATES` 退化态在排异域无实现（已登记 DISP-P2REJ-002）

- 规范：`docs/science/REJECTION.md:108`「| 无候选 | `NO_CANDIDATES` | 同上 |」；`:24`「| `P2_STATUS_*` | `OK/MIN_SAMPLES/ALL_REJECTED/INVALID_INPUT/UNDERDETERMINED/...` | `rejection.h:83-86` |」
- 实现：`rejection.cpp:1944`「`if (n == 0) { out->status = P2_STATUS_MIN_SAMPLES; return 0; }`」；`rejection.cpp:2154` 同；`integrate.h:47`「`P2_INTEGRATE_NO_CANDIDATES = 1`」（该状态属**积分域**）
- 判定：**不符**（C3；已在 `lib/algorithms/rejection/README.md:119` 登记为 DISP-P2REJ-002）
- 证据：排异域 `P2_STATUS_*` 枚举无 `NO_CANDIDATES`；空栈在 ex/compat 两路径均返回 `MIN_SAMPLES`（`rejection.cpp:1944/:2154`）。SCI §8 表却把它列为排异退化态。
- 影响：以 SCI §8 表为 Oracle 的消费者会把「无候选」误判为「样本不足」；两者语义不同（前者无样本，后者样本不足）。
- 修复面：`docs/science/REJECTION.md:108` 改指 `P2_INTEGRATE_NO_CANDIDATES`（积分域）或删除该行。**只登记，不改。**

### CONFORM-SWEEP-3-022 [C1/规范陈旧] 模块 README 的三条 DISP 缺陷已被实现修复，规范仍写「现状缺陷」

- 规范：`lib/algorithms/coverage/README.md:69-70`「空 filter 静默放行**现状** = DISP-COV-003；K=0（空 MOC）合法 rc=0」+ `:160-164`（DISP-COV-003 列入「已知限制/未实现」）
- 实现：`coverage.cpp:92-100`「`const bool has_filter = kv.find("obs_filter") != kv.end(); … if (!has_filter) { … "missing obs_filter property (filter/passband identity is required for compatibility)" … return 1; }`」（B2-A8 fail-closed）
- 规范：`lib/algorithms/sampling/README.md:123`「DISP-P2SMP-004 … 硬编码，**未入 P2SamplerConfig** 配置面」
- 实现：`sampler.h:58-59` 字段已存在、`sampler.cpp:318-319/:513-514` 已有默认与修补（但 veto 现场未消费，见 005）
- 规范：`lib/algorithms/integration/README.md:36`「`sup_max` 实现现状含 **DISP-P2INT-001 缺陷**（见 §6）」+ `:62`
- 实现：`integrate.cpp:44-50`「B2-A7: canonical reducer 契约 = max(**accepted** support) … 故 sup_max 必须在权重分支**之前**更新；旧实现置于 `w==0 continue` 之后…」（已修）
- 判定：**规范歧义**（规范陈旧；属「规范本身错」类）
- 证据：三条 DISP 均为「已修复但规范未同步」；同时 `docs/algorithms/PHASE2_COVERAGE.md:60-64` 已把 DISP-COV-003 标注为「**已关闭**」，与同仓 README 的「现状缺陷」直接冲突（**规范与规范打架**）。
- 影响：审计者/维护者按 README 会把已修行为当缺陷，或按 README 的「现状口径」设计测试（如 DISP-P2SMP-002 双计数被 SPEC 要求按现状口径冻结，而 DISP-P2SMP-004 已不成立）。
- 修复面：`lib/algorithms/coverage/README.md:69-70/:160-164`、`lib/algorithms/sampling/README.md:123`、`lib/algorithms/integration/README.md:36/:62` 三处按实际状态改写（保留 DISP ID 并标「已关闭/部分关闭」）。**只登记，不改。**

### CONFORM-SWEEP-3-023 [规范打架] `smoothing_lambda` 默认值：CONFIG_SCHEMA「auto→0.1」 vs ALG/模块「0.0 默认关」

（与 009 同源，此处记规范侧冲突本身）

- 规范（A）：`docs/development/CONFIG_SCHEMA.md:19`「`smoothing(auto→0.1)`」（自述「单一事实来源」）
- 规范（B）：`docs/algorithms/PHASE2_UPM_IMPL.md:382`「smoothing_lambda（λs） | **0.0（默认关闭平滑）**」；`lib/algorithms/upm/README.md:138`「`smoothing_lambda=0.0 默认关`」；`upm.h:75`「默认 0=关闭」
- 判定：**规范歧义**（两处权威互斥，实现两处都有：`stage2_common.cpp:140`=0.1 / `upm.cpp:246`=0.0）
- 影响：见 009。
- 修复面：同 009。**只登记，不改。**

### CONFORM-SWEEP-3-024 [规范打架] sky_plane 模型地位：`UNIFIED_SCIENCE_MODEL.md` 登记 UNRESOLVED，`PHASE2_UPM.md` 宣告已关闭

- 规范（A）：`docs/science/UNIFIED_SCIENCE_MODEL.md:122`「**UNRESOLVED（跨文档口径冲突）**：… 本文件 §2 的 sky_plane（稀疏样条天光面）与 docs/science/PHASE2_UPM.md §1/§5（纯加性 8×8 control cell，乘性尺度已撤销）模型不同。… **登记 UNRESOLVED，上呈负责人裁决**（本任务不改公式）」
- 规范（B）：`docs/science/PHASE2_UPM.md:182`「**本期决议：纯加性**（负责人 2026-09-19 裁决，claim `FIX-SCI-SNR-CANON-001`）… 原「设计-实现模型冲突」UNRESOLVED **关闭**：`10_sampling.md`/`11_upm.md`/`UNIFIED_MODEL.md` 的目标态表述**同步订正为纯加性**」
- 判定：**规范歧义**（两份 FROZEN/权威 science 文档对同一冲突给出相反状态；B 声称已订正 A，但 A 未改）
- 证据：`docs/science/UNIFIED_SCIENCE_MODEL.md` 的 mtime 未随裁决更新（裁决 claim 与 `ASTROCS_DESIGN.md:122` 均为 2026-09-19）；两文档均属「只读权威」（ENGINEERING_SPEC §3）。
- 影响：sky_plane 是否属生产科学链无单一权威答案（与 011 直接相关）；审计/门无法判定 `p2_sky_plane.bin` 的合法性。
- 修复面：`docs/science/UNIFIED_SCIENCE_MODEL.md:122` 按 `FIX-SCI-SNR-CANON-001` 改写为「已关闭 + 纯加性」并登记 `OPEN-P2S-02` 仅剩数据面。**只登记，不改。**

### CONFORM-SWEEP-3-025 [规范陈旧] `P2SamplerConfig` 字段数：规范 `15`，实现 `17`（含新增 `star_mask_*`）

- 规范：`docs/contracts/DATA_SEMANTICS.md:1516`「**(3) cfg**（P2SamplerConfig，sampler.h:32-57，**15 字段**…）」；`docs/algorithms/PHASE2_SAMPLER.md:91`「配置面（P2SamplerConfig，sampler.h:33-62，声明注释 ：31）：**15 字段**默认值见…」；`lib/algorithms/sampling/memory.md:21-22`「配置 15 字段 h:32-57（control_k_corr 默认 1.4 :53…）」
- 实现：`sampler.h:33-62` 实为 **17 字段**（新增 `double star_mask_snr_factor; // 默认 10.0`、`double star_mask_radius_deg; // 默认 0.012`）
- 判定：**规范歧义**（规范陈旧）
- 证据：`docs/contracts/DATA_SEMANTICS.md:1521-1537` 的逐字段表止于 `cpu_workers`，无 `star_mask_*` 行；`lib/algorithms/sampling/README.md:92-99` 同样列 15 字段。
- 影响：合同消费者（schema 生成、Python 绑定、配置模板）漏掉两个影响 veto/掩膜的科学参数（与 005 叠加）。
- 修复面：`docs/contracts/DATA_SEMANTICS.md:1516-1537`、`docs/algorithms/PHASE2_SAMPLER.md:91-99`、`lib/algorithms/sampling/README.md:92-99`、`memory.md:21-22` 增补两行。**只登记，不改。**

### CONFORM-SWEEP-3-026 [规范自误] `lib/algorithms/upm/README.md` 称 `P2ControlObservation` 为「14 字段」，实列 13（`DATA_SEMANTICS` 亦为 13）

- 规范：`lib/algorithms/upm/README.md:52-54`「多帧控制点观测（P2ControlObservation，upm.h:31-57）」；`lib/algorithms/sampling/README.md:318`「**P2ControlObservation 14 字段**（upm.h:31-57）：frame_id/control_id/leaf_ipix u64；ra_deg/dec_deg/value/uncertainty/snr/ivar/control_variance/control_ivar/support f64；snr_available int；quality_flags u32」
- 规范（对照）：`docs/contracts/DATA_SEMANTICS.md:1548`「**(1) P2ControlObservation 13 字段**（upm.h:31-57…）」
- 实现：`upm.h:31-57` 实为 **13 字段**（3×u64 + 8×f64 + int + uint32 = 13）
- 判定：**规范歧义**（规范自误：README 自称 14，同句列举 13 项，与 DATA_SEMANTICS 的 13 冲突）
- 影响：低（纯计数）；但作为「字段数冻结」类锚点会误导 schema 生成。
- 修复面：`lib/algorithms/sampling/README.md:318` 改「13 字段」。**只登记，不改。**

### CONFORM-SWEEP-3-027 [规范打架] `sampler.h:60` 注释「0=auto：`omp_get_max_threads/hardware_concurrency`」与 `PHASE2_SAMPLER.md §6` 冻结「无 hardware_concurrency、模块不得自行开线程」冲突

- 规范（A，头文件注释）：`lib/algorithms/coverage/include/astro/phase2/sampler.h:60`「// CON-004 并行采样 worker 数（0=auto：omp_get_max_threads/hardware_concurrency；1=串行默认）。// 仅 P2_ENABLE_OPENMP 且 >1 时启用并行第一遍；否则恒串行（默认行为不变）」
- 规范（B，ALG 唯一权威）：`docs/algorithms/PHASE2_SAMPLER.md:284`「worker 数=cfg.cpu_workers（0 视为 1，:883；:881 **无 hardware_concurrency**——模块不得自行开线程，注释冻结 ：880-882）」；`:297-301`「**OpenMP 残留澄清**：… 当前实现 OpenMP 已移除、`std::thread` 为唯一并行路径… **本节为并行语义唯一权威**」
- 实现：`sampler.cpp:916`「// 无 hardware_concurrency(模块不得自行开线程); 1 => 串行 reference。」
- 判定：**规范歧义**（规范与规范打架；头文件注释为陈旧面，与 `DISP-P2UPM-002` 同构）
- 证据：`grep -n "hardware_concurrency\|omp_get_max_threads" sampler.cpp sampler.h` 仅命中注释（`sampler.h:60`、`sampler.cpp:916`），无实际调用 ⇒ 实现跟 B，头注释跟 A。
- 影响：低-中（合同消费者按头文件可能假定 0=auto 会自行探测 CPU）；与 PHASE2_SAMPLER §7 的 DISP 登记原则（注释漂移须登记）不一致——该漂移未登记。
- 修复面：`sampler.h:60` 按 `PHASE2_SAMPLER.md:284/:297-301` 改写（0 视为 1；无 OpenMP/hardware_concurrency）。**只登记，不改。**

---

## 3 规范侧问题汇总（另一类：规范本身错 / 陈旧 / 自相矛盾）

| 条目 | 类型 | 位置 | 一句话 |
|---|---|---|---|
| 018 | 陈旧 | `docs/algorithms/PHASE2_{SAMPLER,UPM_IMPL,REJECTION}.md` §3/§5/§13 | 行数/行号锚系统性过期（1156/1565/2076 → 1520/2732/2857），全部逐符号锚失效 |
| 019 | 陈旧 | `docs/algorithms/PHASE2_UPM_IMPL.md:389` | 把已修（`upm.cpp:646`）与未修（`upm.cpp:1646`）的两种归一化门写成同一条冻结常数 |
| 020 | 自相矛盾 | `docs/science/REJECTION.md:32` vs `:47/:61` | §4 禁 per-pixel 路由 vs §5 生产默认 per-pixel 几何 n |
| 021 | 陈旧 | `docs/science/REJECTION.md:108` | `NO_CANDIDATES` 属积分域，排异域无该状态（DISP-P2REJ-002） |
| 022 | 陈旧 | 三份模块 README 的 DISP 清单 | DISP-COV-003 / DISP-P2SMP-004 / DISP-P2INT-001 已修未同步 |
| 023 | 打架 | `docs/development/CONFIG_SCHEMA.md:19` vs `PHASE2_UPM_IMPL.md:382` | `smoothing` auto→0.1 vs 0.0 默认关 |
| 024 | 打架 | `docs/science/UNIFIED_SCIENCE_MODEL.md:122` vs `docs/science/PHASE2_UPM.md:182` | sky_plane 冲突 UNRESOLVED vs 已关闭 |
| 025 | 陈旧 | `DATA_SEMANTICS.md:1516` / `PHASE2_SAMPLER.md:91` / `sampling/README.md:92` | P2SamplerConfig 15 字段 → 实为 17 |
| 026 | 自误 | `lib/algorithms/sampling/README.md:318` | 「14 字段」实列 13（DATA_SEMANTICS 亦 13） |
| 027 | 打架 | `sampler.h:60` vs `PHASE2_SAMPLER.md:284/:297` | 0=auto/hardware_concurrency vs 无自行开线程 |

> 另有 `lib/algorithms/coverage/README.md:10-11`（coverage.cpp 239 行 / coverage.h 59 行）与 `lib/algorithms/upm/README.md:9-11`（upm.cpp 1565 行 / upm.h 184 行）、`lib/algorithms/rejection/README.md:9-11`（rejection.cpp 2076 行）、`lib/algorithms/sampling/README.md:9-11`（sampler.cpp 1156 行 / sampler.h 136 行）同样过期，已并入 018/025 不另计。

---

## 4 全量陈述清点表（逐条编号）

> 判定列：符合 / 不符 / 未实现 / 规范歧义 / 待定。「→ 条目」列指向 §2/§3 的需行动条目。

### 4.1 `docs/algorithms/v6/frozen/01_NUMERIC_THRESHOLD_FREEZE.md`（FZ 冻结数值表）

| # | 规范陈述（file:line） | 实现锚（快照） | 判定 | → 条目 |
|---|---|---|---|---|
| S-001 | FZ-UPM-CONVERGENCE Huber δ=1.345（:45） | `upm.cpp:245`；`module_adapters.cpp:5076`；`p2_session.cpp:202` | 符合 | — |
| S-002 | FZ-UPM-CONVERGENCE max_iter=100（:45） | `upm.cpp:247`；`module_adapters.cpp:5077`；`p2_session.cpp:203` | 符合 | — |
| S-003 | FZ-UPM-CONVERGENCE **tol=1e-6**（:45） | `module_adapters.cpp:5083-5084`（1e-3+relative）；`p2_session.cpp:204`（1e-6） | **不符** | 001/002 |
| S-004 | FZ-UPM-CONVERGENCE sigma_floor=1e-3（:45） | `upm.cpp:251`；`module_adapters.cpp:5094` | 符合 | — |
| S-005 | FZ-UPM-CONVERGENCE zero_anchor=1e-3（:45） | `upm.cpp:247`；`module_adapters.cpp:5094` | 符合 | — |
| S-006 | FZ-REJ-INHERITED-THRESH sigma/winsorized/averaged 4.0/3.0/8（:44） | `rejection.cpp:1166-1170` | 符合 | — |
| S-007 | 同上 linear_fit 5.0/3.5/8（:44） | `rejection.cpp:1171-1172` | 符合 | — |
| S-008 | 同上 percentile 0.2/0.1（:44） | `rejection.cpp:1174` | 符合 | — |
| S-009 | 同上 ESD alpha 0.05 / max 10（:44） | `rejection.cpp:1173` | 符合 | — |
| S-010 | 同上 large_scale min_structure 8 / low 2 / high 2（:44） | `rejection.cpp:1180-1183` | 符合 | — |
| S-011 | FZ-PROV-KCORR-VALUE k_corr=1.4（域内）（:53） | `sampler.cpp:83`；`upm.cpp:1883` | 符合 | — |
| S-012 | FZ-AP2S-RANK-RTOL 1e-10（:35） | `upm.cpp:2181` | 符合 | — |
| S-013 | FZ-AP2S-KAPPA-MAX 1e6（:36） | `upm.cpp:2182` | 符合 | — |
| S-014 | FZ-AP2S-UPM-MINFRAMES 2（:37） | `upm.cpp:2180`；`:2282-2285` | 符合 | — |
| S-015 | FZ-AP2S-REJ-CALIB-BINMIN 50（:41） | `rejection.h:472` | 符合 | — |
| S-016 | FZ-AP2S-REJ-CALIB-ABS 0.10（:42） | `rejection.h:473` | 符合 | — |
| S-017 | FZ-AP2S-REJ-BSS-MIN 0.10（:43） | `rejection.h:474` | 符合 | — |
| S-018 | FZ-FORMULA-COV-PROP `C_out=R C_in R^T`（:62） | `upm.cpp:2315`（`p2_upm_ma_c_out`，零调用） | **未实现** | 010 |

### 4.2 `docs/algorithms/PHASE2_SAMPLER.md`（ALG-P2-SMP-001）

| # | 规范陈述 | 实现锚 | 判定 | → 条目 |
|---|---|---|---|---|
| S-020 | control_variance = k_corr×(π/2)×σ_bg²/N_retained（:180-184） | `sampler.cpp:875`「`kcorr_f * kPiHalf * sigma * sigma / n_ret`」 | 符合 | — |
| S-021 | k_corr 域外**回退 1.4 禁 clamp**（:190-198） | `sampler.cpp:100-102` | 符合 | — |
| S-022 | kcorr 标定表九值 {1.2112,1.3925,1.4980\|2.3958,2.8971,3.2035}（:196-197） | `sampler.cpp:96-99` | 符合 | — |
| S-023 | pixfrac 维 clamp [0.5,1.0]（:198） | `sampler.cpp:103`「`std::clamp(pixfrac, 0.5, 1.0)`」 | 符合 | — |
| S-024 | 两段分段线性插值（非均匀网格，:197） | `sampler.cpp:105-112` | 符合 | — |
| S-025 | patch 保留负值（:157/:365） | `sampler.cpp:840-875`（clipping 后取 `m0`，无正值夹取） | 符合 | — |
| S-026 | snr_available=0 不伪装 1.0（:243-247） | `sampler.cpp:888-899`（`cs.snr_avail.push_back` :898） | 符合 | — |
| S-027 | ≥2 clean 帧才入 UPM（:64-65/:367） | `sampler.cpp:1046-1075`（`nclean>=2` :1055；`nclean<2` :1075） | 符合 | — |
| S-028 | frame_id = SHA-256(9 props + signal tiles + support "S" + SNR catalogue)（:255-267） | `sampler.cpp:314-438` | 符合 | — |
| S-029 | catalog veto thr=10.0×frame_snr_med / rad=0.012°（:234-237） | `sampler.cpp:884-885`（字面量） | **不符**（来源） | 005 |
| S-030 | DISP-P2SMP-004「硬编码未入 P2SamplerConfig」（README:123） | 字段已存在（`sampler.h:58-59`） | **规范歧义** | 005/022 |
| S-031 | P2SamplerConfig **15 字段**（:91-99） | `sampler.h:33-62` = 17 | **规范歧义** | 025 |
| S-032 | worker 数唯一来源=Runtime lease，无 hardware_concurrency（:284） | `sampler.cpp:916-917` | 符合（头注释漂移） | 027 |
| S-033 | OpenMP 已移除、std::thread 唯一并行路径（:297-301） | `sampler.cpp:917-965`（无 `#pragma omp`） | 符合 | — |
| S-034 | 输出 obs 序列 bitwise 与 worker 数无关（:291-295） | 固定槽位写回 `sampler.cpp:902-904`（代码结构） | 待定（未运行验证） | — |
| S-035 | p2_stats_mad = 1.482602218505602×median\|x−med\|（:276-277） | `sampler.cpp:470` | 符合 | — |
| S-036 | 三阶段 Stage A–E 管线（:104-113） | `sampler.cpp:615-1100`（Stage A-E 注 :615；pass1_cell :734；第二遍 :987；第三遍 :1046） | 符合 | — |
| S-037 | 生产消费方 stage2.cpp 显式透传 14 字段（:282） | node chain `module_adapters.cpp:4928-4931` 不透传 | **未实现** | 014 |

### 4.3 `docs/algorithms/PHASE2_UPM_IMPL.md`（ALG-P2-UPM-IMPL-001）+ `docs/science/PHASE2_UPM.md`（SCI-UPM-001）

| # | 规范陈述 | 实现锚 | 判定 | → 条目 |
|---|---|---|---|---|
| S-040 | `w_UPM = quality_factor × control_reliability × control_ivar`（SCI :54） | `upm.cpp:1611-1618`「`qf * civ`」 | 符合 | — |
| S-041 | 禁 production 乘 star SNR / support^p / obs.ivar（SCI :64/:122） | `upm.cpp:1612-1618`（production 分支）；legacy 分支 `:1620-1627` 仅在 `use_ivar_weight=0` | 符合 | — |
| S-042 | `control_ivar≤0`/非有限 → rc=2（SCI :35；ALG :307） | `upm.cpp:1616`「`return 2`」 | 符合 | — |
| S-043 | control_reliability 实现为配置常量 1.0（SCI :21，已登记 SC-005） | `upm.cpp:255/:320`；`module_adapters.cpp:5097` | 符合（已登记缺陷） | — |
| S-044 | `w_cell = w_UPM/Σ_cell w_UPM × control_reliability`（SCI :56） | `upm.cpp:640-647`（build，尺度无关门）/ `upm.cpp:1646`（API，1e-12 门） | **不符**（API 面） | 003 |
| S-045 | `k_corr ≥ 1`，`k_corr<1` 显式拒（SCI :37-39） | `upm.cpp:2717`「`if (k_corr < 1.0) return 1;`」 | 符合 | — |
| S-046 | `k_corr==1.0` → rc=2（memory.md:154） | `upm.cpp:2718` | 符合 | — |
| S-047 | 域外 k_corr 无 MC run id → rc=3（memory.md:155） | `upm.cpp:2719-2722` | 符合 | — |
| S-048 | 纯加性模型 `calibrated=raw−C_f`，`g_k≡1`（SCI :182） | `upm.cpp:1597-1628` + `module_adapters.cpp:5503`（`additive_mode="c"`） | 符合 | — |
| S-049 | 8×8 control cell 双线性（SCI :50） | `upm.cpp:276`（`grid!=8 → rc=3`）；`module_adapters.cpp:5094` | 符合 | — |
| S-050 | gauge 每分量 ref=min(frame_id)，C=0（SCI :69） | `upm.cpp:651/:789-829`（`component_ref_frame`） | 符合 | — |
| S-051 | tolerance **1e-6**（ALG :379，冻结表） | `module_adapters.cpp:5083`（1e-3+relative） | **不符** | 001 |
| S-052 | smoothing_lambda 0.0 默认关（ALG :382） | `upm.cpp:246`；`module_adapters.cpp`（不设） | 符合（模块）/ 打架（工具） | 009/023 |
| S-053 | 归一化门 s>1e-12（ALG :389，冻结表） | `upm.cpp:646`（已改）/ `:1646`（未改） | **规范歧义** | 003/019 |
| S-054 | support_power 仅 ablation 消费（ALG :386） | `upm.cpp:1625`（legacy 分支） | 符合 | — |
| S-055 | link_rad=1.6×cell_dist（ALG :392） | `upm.cpp:422`「`cell_dist_rad * 1.6`」 | 符合 | — |
| S-056 | CG max_cg=200 / 早停 1e-30 / 1e-24（ALG :388） | `upm.cpp:658/677/685` | 符合 | — |
| S-057 | kChunk=16（ALG :391） | `upm.cpp:1712` | 符合 | — |
| S-058 | rc 语义表：n_obs=0→rc=1；未知 frame_id→rc=1（ALG :306/:309） | `upm.cpp:236/:2173`（n_obs==0）；`:1548-1560`（未知 frame_id 显式失败） | 符合 | — |
| S-059 | `p2_upm_evaluate_c` 未知帧 NaN，禁用 0.0 哨兵（ALG :310/:324） | `upm.cpp:1576-1585` | 符合（缺失 cell 返回 0.0 = 已登记开放项） | — |
| S-060 | 两处生产组装显式赋 zero_anchor_weight=1e-3（ALG :373） | `p2_session.cpp:207`；`module_adapters.cpp:5094` | 符合（ALG 文档锚 `:3152-3176` 漂移） | 018 |
| S-061 | `upm.cpp` 1565 行 / `upm.h` 184 行（ALG :68） | 2732 / 380 | **规范歧义** | 018 |
| S-062 | 跨 worker 数 1..N = 1e-12 绝对容差（SCI :95-100） | `upm.cpp:587-616`（worker-local tsums 按 tid 合并） | 符合（代码结构；未运行验证） | — |
| S-063 | 单帧区 harmonic continuation（SCI :109；ALG :220） | `upm.cpp:2282-2290`（`min_frames` + `allow_additive_only_single_frame`） | 待定（未运行验证） | — |
| S-064 | `P2UpmBuildConfig` 字段表（DATA_SEMANTICS :1860-1880） | `upm.h:71-118` 多出 `tolerance_relative` | **不符** | 002 |
| S-065 | 合同：`tolerance | 1e-6（无 config 覆盖键）`（DATA_SEMANTICS :1762） | `module_adapters.cpp:5116-5118` 有覆盖键 | **不符** | 002 |

### 4.4 `docs/algorithms/PHASE2_REJECTION.md` + `docs/science/REJECTION.md`（SCI-REJ-001）

| # | 规范陈述 | 实现锚 | 判定 | → 条目 |
|---|---|---|---|---|
| S-070 | 7 方法阈值冻结锚点（SCI :65-71） | `rejection.cpp:1166-1188` | 符合 | — |
| S-071 | WBPP 对照档 AUTO 路由 n<6 / 6..15 / >15（SCI :54-57） | `rejection.cpp:1200-1202` | 符合 | — |
| S-072 | `astrocs_adaptive_pixel` 路由 n≤3 none / 4..7 pct / 8..15 winsorized / ≥16 linear_fit（SCI :47-51） | `rejection.cpp:1114-1119` | 符合 | — |
| S-073 | 生产默认 profile = `astrocs_adaptive_pixel`（SCI :21/:38/:47） | `module_adapters.cpp:6034` 符合 / `stage2_common.cpp:258` 不符 | **不符**（工具面） | 007 |
| S-074 | `underdetermined_n` 默认 2（CONFIG_SCHEMA :26） | `stage2_common.cpp:272-277`（pixel 档 3）；`rejection.cpp:1155-1162` | **不符** | 008 |
| S-075 | 全拒容错域「写死 n=4」（SCI :77/:95） | `rejection.cpp:2112`「`n <= 4`」 | **规范歧义** | 013 |
| S-076 | `n≥5` 全拒 → ALL_REJECTED 不降级（SCI :96） | `rejection.cpp:2119-2121` | 符合 | — |
| S-077 | large_scale 默认关闭 enabled=0（SCI :71） | `rejection.cpp:1180-1183` | 符合 | — |
| S-078 | PERCENTILE×norm≠MEDIAN_CENTER → INVALID_CONFIGURATION（README:72-73） | `rejection.cpp:1962-1986`（方法×归一化门） | 符合 | — |
| S-079 | AUTO 永不进 kernel（README:66-67；SCI :33） | `rejection.cpp:1192-1204`（planning 消解）；`:2085-2086`（default→rc=1） | 符合 | — |
| S-080 | DISP-P2REJ-001 `low_fraction` 注释「默认 0.1」vs 实现/SCI 0.2（README:118） | `rejection.h:135`「默认 0.1 = 10%」 vs `rejection.cpp:1174`「0.2」 | 符合（已登记缺陷，现状未修） | — |
| S-081 | DISP-P2SMP/REJ 类「禁止 per-pixel effective 路由」（SCI :32） | 实现按逐像素**几何** n 路由 | **规范歧义** | 020 |
| S-082 | `NO_CANDIDATES` 退化态（SCI :108） | 排异域无该状态 | **不符**（已登记） | 021 |
| S-083 | reason(per-sample) 与 status(stack) 分离（SCI :93） | `rejection.h:76/:83-86`；`rejection.cpp:2089-2124` | 符合 | — |
| S-084 | 空栈 n==0 → MIN_SAMPLES（README:70-71） | `rejection.cpp:1944/:2154` | 符合 | — |

### 4.5 `docs/science/INTEGRATION.md` + `lib/algorithms/integration/`（SCI-INT-001）

| # | 规范陈述 | 实现锚 | 判定 | → 条目 |
|---|---|---|---|---|
| S-090 | `signal = Σ_{valid,W>0} w_i x_i / wsum`（SCI :55-57） | `integrate.cpp:59-60/:75` | 符合 | — |
| S-091 | `support = max(accepted support)` canonical（SCI :58/:63） | `integrate.cpp:44-50/:76` | 符合 | — |
| S-092 | `w==0` 合法不贡献（SCI :71） | `integrate.cpp:56`「`if (w == 0.0) continue;`」 | 符合 | — |
| S-093 | 五态 status 互斥（SCI :52-60） | `integrate.cpp:23-25/:66-77`；`integrate.h:45-51` | 符合 | — |
| S-094 | 负/非有限权重 → INVALID_INPUT（SCI :32） | `integrate.cpp:54-55`；`p2_validate_candidate_weights :14` | 符合 | — |
| S-095 | `count==0`/values==null → NO_CANDIDATES（SCI :81） | `integrate.cpp:23-26` | 符合 | — |
| S-096 | DISP-P2INT-001 sup_max 漏计零权 accepted（README:36/:62） | `integrate.cpp:44-50`（B2-A7 已修） | **规范歧义** | 022 |
| S-097 | `integrate.cpp` 81 行 / `integrate.h` 74 行（ALG :49，已重锚） | 81 / 74 | 符合 | — |

### 4.6 `docs/contracts/DATA_SEMANTICS.md` §30 / `contracts/data/phase2_*.json`

| # | 规范陈述 | 实现锚 | 判定 | → 条目 |
|---|---|---|---|---|
| S-100 | `ivar_mosaic = W = Σ ivar_i`；`variance = 1/W`（:2391-2392） | `module_adapters.cpp:7290-7294`（`varnum=cov²/W`，`w=wsum_v[i]`） | 符合 | — |
| S-101 | unavailable 规则 2：ivar 缺失 ⇒ 不写 variance/ivar（:2405-2407） | `module_adapters.cpp:6669` + `:7239/:7317` | **不符** | 004 |
| S-102 | invalid policy：无有效样本 → variance/ivar=NaN（:2415） | `module_adapters.cpp:7292-7294`（NaN 同态） | 符合 | — |
| S-103 | `ASTROCS_REJECT_PROFILE` canonical=`astrocs_adaptive_pixel`（:67） | `module_adapters.cpp:7379` 透传 `rej_doc.profile` | 符合 | — |
| S-104 | provenance 键必须如实（:2400-2402 精神） | `sky_plane_applied` 语义失真 | **不符** | 012 |
| S-105 | P2ControlObservation 13 字段（:1548） | `upm.h:31-57` = 13 | 符合（README 说 14） | 026 |
| S-106 | P2SamplerConfig 15 字段（:1516） | `sampler.h:33-62` = 17 | **规范歧义** | 025 |

### 4.7 `ASTROCS_DESIGN.md` / `docs/design/UNIFIED_MODEL.md` / `docs/plugins/**`（权威链）

| # | 规范陈述 | 实现锚 | 判定 | → 条目 |
|---|---|---|---|---|
| S-110 | `sparse_snr_layer: true` 默认稀疏（DESIGN :122；UNIFIED_MODEL :45；07_noise_snr :112） | `module_adapters.cpp:6620`「`in.sparse = nullptr`」 | **未实现** | 016 |
| S-111 | Phase2 自动检测稀疏层：有→帧级×帧内（DESIGN :256） | 无检测代码 | **未实现** | 016 |
| S-112 | 无稀疏层须显式记录 `snr_path_effective` + 计数，不得静默（13_integration :22） | 无 `snr_path`/`snr_path_effective` | **未实现** | 016 |
| S-113 | 实际 SNR = 帧级 × 帧内（DESIGN :174；13_integration :21） | `weight_chain.h:115` `compose_actual_snr` 零调用 | **未实现** | 016 |
| S-114 | `smoothing(auto→0.1)`（CONFIG_SCHEMA :19） | `stage2_common.cpp:140` 符合 / `upm.cpp:246` 不符 | **规范歧义** | 009/023 |
| S-115 | 默认值一致性由 `config_consistency_check.py` 校验（CONFIG_SCHEMA :3-4） | 该工具 4 条路径全不存在，`pass=false` | **不符** | 017 |

---

## 5 常数（魔数）清单与出处回查（范围内）

> 口径：范围内代码里所有非平凡数值字面量/常量，逐个回查规范出处。仅列**有出处或疑似缺口**者；纯算法内部常数（RCR 临界值表、erfinv 多项式系数、P 值反演表等）标「数学常数表，规范未要求逐值冻结」。

| 常数 | 值 | 位置（快照） | 规范出处 | 判定 |
|---|---|---|---|---|
| kControlCorrDefault | 1.4 | `sampler.cpp:83` | SCI-UPM §5/:22；FZ-PROV-KCORR-VALUE | 符合 |
| kPiHalf | 1.57079632679489661923 | `sampler.cpp:84` | SCI-UPM §5（π/2）；ALG §5.4 | 符合 |
| kcorr 标定表九值 | 1.2112/1.3925/1.4980；2.3958/2.8971/3.2035 | `sampler.cpp:96-99` | ALG-P2-SMP §5.4（冻结九值） | 符合 |
| 标定域 scale | [300,600]″/px | `sampler.cpp:95` | SCI-UPM §4/:40-42 | 符合 |
| pixfrac clamp | [0.5,1.0] | `sampler.cpp:103` | ALG-P2-SMP §5.4 | 符合 |
| kSnrCatalogMax | 1<<16 = 65536 | `sampler.cpp:77` | ALG-P2-SMP §3（:77） | 符合 |
| catalog veto 阈值 | 10.0 | `sampler.cpp:884` | ALG §5.5（值对；来源错） | **不符**（005） |
| catalog veto 半径 | 0.012° | `sampler.cpp:885` | ALG §5.5（值对；来源错） | **不符**（005） |
| star_mask 默认 | 10.0 / 0.012 | `sampler.cpp:318-319`；`sampler.h:58-59` | sampler.h 注释「P0-08 整改」 | 符合（字段）/ 未消费（005） |
| MAD 系数 | 1.482602218505602 | `sampler.cpp:470`；`rejection.cpp:1072` | ALG-P2-SMP §5.7 | 符合 |
| σ_bg floor | 1e-12 | `sampler.cpp:864` | ALG-P2-SMP §5.3 | 符合 |
| clipping 收敛阈值 | 1e-12×max(\|m0\|,1e-12) | `sampler.cpp:853` | ALG §5.3（DISP-P2SMP-005 已登记） | 符合（已登记） |
| n_union 上限 | 1e6 | `sampler.cpp:669` | ALG §9/:339 | 符合 |
| cells 上限 | 2e8 | `sampler.cpp:679/:1161` | ALG §9/:339-340 | 符合 |
| 单 cell 帧数上限 | 10000 | `sampler.cpp:1172` | ALG §9/:340-341 | 符合 |
| UPM huber_delta | 1.345 | `upm.cpp:245/:2183`；`p2_session.cpp:202` | FZ-UPM-CONVERGENCE | 符合 |
| UPM max_iterations | 100 | `upm.cpp:247`；`p2_session.cpp:203` | FZ-UPM-CONVERGENCE | 符合 |
| UPM tolerance | **1e-6（模块默认）/ 1e-3（生产 node chain）** | `upm.cpp:249` / `module_adapters.cpp:5083` | FZ-UPM-CONVERGENCE=1e-6 | **不符**（001） |
| UPM tolerance_relative | 1（生产） | `module_adapters.cpp:5084` | **无任何规范出处** | **不符**（002） |
| UPM sigma_floor | 1e-3 | `upm.cpp:251` | FZ-UPM-CONVERGENCE | 符合 |
| UPM zero_anchor_weight | 1e-3 | `upm.cpp:247`；`p2_session.cpp:207`；`module_adapters.cpp:5094` | SCI §9a:160 | 符合 |
| UPM smoothing_lambda | 0.0（模块）/ 0.1（工具 auto） | `upm.cpp:246` / `stage2_common.cpp:140` | ALG §13=0.0 vs CONFIG_SCHEMA=0.1 | **规范歧义**（009） |
| UPM grid | 8（!=8 → rc=3） | `upm.cpp:276` | SCI §5；M7-C-001 | 符合 |
| UPM tile_shift | 9 | `upm.cpp:299` | ALG §3（leaf order=target+9） | 符合 |
| UPM link_rad | 1.6×cell_dist | `upm.cpp:422` | ALG :392（冻结表） | 符合 |
| CG max_cg | 200 | `upm.cpp:658` | ALG :388 | 符合 |
| CG 早停 | 1e-30 / 1e-24 | `upm.cpp:677/:685` | ALG :388 | 符合 |
| per-control den 门 | 1e-12 | `upm.cpp:776/:792/:801/:828/:845/:854` | ALG :390 | 符合 |
| 归一化门（build） | `s>0 ∧ finite` | `upm.cpp:646` | ALG :389 写 1e-12 ⇒ 规范陈旧 | **规范歧义**（019） |
| 归一化门（API） | 1e-12 | `upm.cpp:1646` | SCI §5 无阈值 | **不符**（003） |
| rank_rtol | 1e-10 | `upm.cpp:2181` | FZ-AP2S-RANK-RTOL | 符合 |
| kappa_max | 1e6 | `upm.cpp:2182` | FZ-AP2S-KAPPA-MAX | 符合 |
| min_frames | 2 | `upm.cpp:2180` | FZ-AP2S-UPM-MINFRAMES | 符合 |
| kMaKCcorrFrozenInDomain | 1.4 | `upm.cpp:1883` | FZ-PROV-KCORR-VALUE | 符合 |
| materialize kChunk | 16 | `upm.cpp:1712` | ALG :391 | 符合 |
| saturated quality_factor | 0.1 | `upm.cpp:203` | SCI §5（quality_factor 语义）；**具体值无规范出处** | 待定 |
| rejection 继承阈值 | 4.0/3.0/8；5.0/3.5/8；0.2/0.1；0.05/10；8/2/2 | `rejection.cpp:1166-1183` | FZ-REJ-INHERITED-THRESH | 符合 |
| rejection normalization_floor | 1e-12 | `rejection.cpp:1165` | ALG-P2-REJ §5（已登记） | 符合 |
| extreme_prior alpha | 0.05 | `rejection.cpp:1184` | FIX-REJ §3 | 符合 |
| minmax min_kept | 4 | `rejection.cpp:1178` | SCI :70 | 符合 |
| RCR 技术码 | 0 = SS_MEDIAN_DL | `rejection.cpp:1179` | SCI :163（RCR 2.4.7 oracle） | 符合 |
| RCR 常数表 | kRcrSSDLUnityCF[101] / kRcrSSUnity[1001] / 拟合系数 | `rejection.cpp:178-320/:596-835` | SCI :163（官方 RCR 2.4.7 对照） | 待定（表值未逐项核） |
| ESD/正态分位常数 | 0.682689 / 0.317311 / 6.36 / 1.134 / 39.2519 / 1.8688 / 3.578 / 0.942 | `rejection.cpp:403-835/:1478` | 无规范出处（方法核内部拟合，Siril 对照） | 待定（数学常数表，规范未要求逐值冻结） |
| V6 排异校准常数 | 50 / 0.10 / 0.10 | `rejection.h:472-474` | FZ-AP2S-REJ-CALIB-BINMIN/ABS/BSS-MIN | 符合（未引用 FZ id，登记建议） |
| integrate 无魔数 | — | `integrate.cpp`（仅 0.0/1.0 语义值） | SCI-INT §5 | 符合 |
| mosaic 单元立体角 | `a_cell = 4π/(12·nside²)` | `module_adapters.cpp:7221-7222` | HEALPix 定义（DATA_SEMANTICS §12.x `support=min(area/A_cell,1)` 逆式） | 符合 |
| SNR 链 `F_ref` 一致性门 | 1e-9 相对 | `module_adapters.cpp:6634` | WEIGHT-SCI-001（fail-closed，不放宽） | 符合 |
| sky_plane 默认参数 | spline_degree 3 / node_spacing 1.0° / roughness 1e-3 | `module_adapters.cpp:5264-5270`；`sky_plane.cpp:400-403` | `docs/plugins/algorithms_phase2/11_upm.md:80`（仅 roughness 一行）；无 SCI 权威 | 待定（011） |
| corrected 方差输出 | `varnum = cov²/W` | `module_adapters.cpp:7290-7294` | DATA_SEMANTICS §30.1（`variance=1/W`，writer 归约） | 符合（但写出条件见 004） |
| coverage properties 缓冲 | 8192 B | `coverage.cpp:68` | ALG-COV §2（8192 B 缓冲） | 符合 |
| coverage leaf 父移位 | `ip >> 2·shift` | `coverage.cpp:248-251` | ALG-COV §2（NESTED 父聚合） | 符合 |
| coverage tile_width 门 | 512 | `coverage.cpp:106` | ALG-COV §2 | 符合 |

---

## 6 诚实声明与未覆盖面

1. **未覆盖面**：
   - `lib/algorithms/coverage/src/{sky_plane,acr_kernels,block,stage2_common}.cpp` 与 `lib/algorithms/integration/v6/**` 只做了「被 p2_op_* 调用到的面」的核对；其**独立算法面**（sky_plane 样条求解细节、ACR kernel 数值、weight_chain 的 `C_in` 联合协方差路径）未逐公式核。
   - `rejection.cpp` 的 10 个方法核（sigma/winsorized/averaged/linear_fit/ESD/RCR/percentile/median_sigma/minmax/extreme_prior）只核了**阈值常数**与**路由**，未逐行核算法语义（属 RELEASE-02 SCI-AUDIT / P2-REJ 既有域）。
   - `sampler.cpp` 的三阶段管线只核了公式与常数，未做数值复算（无运行环境，禁 ninja/ctest）。
   - Phase1 侧的 `F_instr`（PSF 域通量 vs 5×5 盒和 `m00`）与 `fwhm_px=2.3548σ → Moffat4 FWHM` 不在本分片范围（CONFORM-SWEEP-1-001/004 承载）；本分片只登记其**下游传播面**（004 影响节：`F_ref²` 进入 variance 绝对标度）。
2. **待定项（4 条）**：S-034（bitwise 并行确定性，仅代码结构）、S-063（harmonic continuation 运行行为）、`quality_factor` 饱和值 0.1 的规范出处、RCR/ESD 数值表逐值核对。均如实标「待定」，未以「符合」充数。
3. **判定为「符合」的 47 条**均为逐条读过实现代码后给出，未使用「文档说实现了」作为证据。
4. **行号基准**：所有实现行号来自 §头部列出的快照（`/dev/shm/astrocs_conf3/snap`），因工作树在审计期间被并行修改（`module_adapters.cpp` mtime 19:28:37，较 HEAD 19:13 更晚；审计开始时该文件为 8960 行、结束前为 9042 行）。**任何按本报告行号复核者请先核对 md5**。
5. **未做**：未改任何生产代码/文档；未 git add/commit/push；未跑 ninja/cmake/ctest；只读运行了三个 python 校验器（结果见 017）。
5b. **快照收尾**：按硬约束清理 `/dev/shm/astrocs_conf3`；快照内容与审计结束时的**工作树逐位相同**（复核 md5：`module_adapters.cpp f6eba4bcc27ec2f5a316e3db2e7c70b5` / `upm.cpp 5d7ba55fd8c30004faf5b9f40c720be7` / `sampler.cpp c1dab5f3e80f4f56d91b8756ba6d796d`，mtime 2026-09-19 19:28:37），故行号可对工作树直接复核。
6. **口径修订记录（诚实性自校）**：S-015/016/017（V6 排异校准常数 50/0.10/0.10）初判「待定」，核对 `FZ-AP2S-REJ-CALIB-*` 后改判「符合」；S-013（全拒容错域）初判「不符」，复核 SCI §4 的可达性论证后改判「规范歧义」（可达域等价，仅字面不符），未按「不符」计入 C1；S-080（percentile 注释 0.1）判定为「符合（已登记缺陷）」，因其属 `DISP-P2REJ-001` 明文登记的现状，非新发现。
