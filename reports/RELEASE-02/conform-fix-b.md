# CONFORM-FIX-B — 符合性修复分片 B（代码侧）报告

> 分片：**CONFORM-FIX-B**（AstroCS RELEASE-02 规范↔实现符合性修复，代码侧）
> 输入：`reports/RELEASE-02/conformance/CONFORM-SWEEP-3.md` / `.json`（74 条陈述，27 条需行动）
> 基准快照（审计口径）：`module_adapters.cpp f6eba4bc…` / `upm.cpp 5d7ba55f…` / `sampler.cpp c1dab5f3…` /
> `stage2_common.cpp 832c684e…` / `p2_session.cpp e642eb3f…`（开工时逐文件 md5 复核 = 审计快照，行号可直接对锚）
> 硬约束遵守：**零 git 写**；**未跑 ninja/cmake/ctest**（只用 `g++ -fsyntax-only` + **独立重链**）；
> **未改 `docs/**`**（001 只落变更 claim 草案到 `工程控制/`）；**未改 `ci/checks.json` / `config/**` / `contracts/**`**；
> `TMPDIR=/dev/shm/astrocs_cfb`（收尾清理）；未跑 e2e（前台统一跑）。

---

## 0 交付物与状态

| 项 | 路径 | 状态 |
|---|---|---|
| 本报告 | `reports/RELEASE-02/conform-fix-b.md` | 完成 |
| 001 变更 claim **草案** | `工程控制/RELEASE-02/change-claims/CONFORM-FIX-B-001-tolerance-relative.md` | **已批准**（§9.53:1737-1777；订正 2026-09-20，V5 分片 3：原「草案，待负责人裁决」作废） |
| 判别力测试（红/绿） | `run/RELEASE-02/conform-fix-b/tests/cfb_tests.cpp` | **新实现 49 PASS / 0 FAIL；审计基线实现 34 PASS / 15 FAIL**（独立重链实测，日志 `logs/{new,old}.log`） |
| 旧基线还原脚本 | `run/RELEASE-02/conform-fix-b/tests/revert_to_baseline.py` | 完成 |
| 独立重链脚本 | `run/RELEASE-02/conform-fix-b/tests/build_and_run.sh` | 完成 |
| 构建/运行日志 | `run/RELEASE-02/conform-fix-b/logs/{new,old}.log` | 完成 |

**改动清单（file:line，均为最小改动面）**

| 条目 | 文件 | 行（修复后） | 改动 |
|---|---|---|---|
| 001 | `lib/infrastructure/scheduler/src/module_adapters.cpp` | 5221-5239 | `uc.tolerance=1e-3/relative=1` → **`1e-6/0`**（回退冻结值） |
| 003 | `lib/algorithms/coverage/src/upm.cpp` | 1646-1658 | `s > 1e-12` → **`s > 0.0 && std::isfinite(s)`** |
| 004 | `lib/infrastructure/scheduler/src/module_adapters.cpp` | 6796-6799, 6912-6917, 6931, 7382-7383, 7414, 7448-7450, 7585-7589 | ivar 缺失 ⇒ `uncertainty_available=false` + 原因键贯通 `p2_integrated.json`/`p2_final.json`/节点 manifest |
| 005 | `lib/algorithms/coverage/src/sampler.cpp` | 882-895 | veto `thr/rad` 改消费 `cfg.star_mask_snr_factor`/`cfg.star_mask_radius_deg` |
| 007 | `lib/algorithms/coverage/src/stage2_common.cpp` + `include/astro/phase2/stage2_common.h` | cpp:255-266 / h:66-74 | 默认 profile → `P2_PROFILE_ASTROCS_ADAPTIVE_PIXEL`（自研档） |
| 008 | 同上 | cpp:268-297 / h:75-80 | 默认 `underdetermined_n` → **0（= 按 profile/request 默认）**，缺键时向 `p2_reject_plan_resolve` 查询（单一来源） |
| 009 | 同上 + `module_adapters.cpp` | h:9-21, h:40-42 / cpp:140 / MA:5290-5310 | 新增 `P2_SMOOTHING_LAMBDA_AUTO=0.1` 单一来源；struct 默认 == parser 默认；node chain 消费同语义 `model.smoothing` 键 |
| 011 | `module_adapters.cpp` | 5450-5463 | `sky_plane.enabled` 默认由 `true` → **`delta_wanted`（= additive_mode ∈ {delta,both}）** |
| 012 | `module_adapters.cpp` | 6085-6088, 6104-6106, 6114-6115, 6127-6128, 6156 | 拆 `sky_plane_loaded` / `sky_plane_applied`；`applied == δ 实扣` |
| 014 | `module_adapters.cpp` | 4909-5045, 5046-5060, 5119-5140 | 新增 `p2_sample_cfg_from_doc`（`model:` 段 17 键逐字段透传 + 域校验 fail-closed）；生效配置落 `p2_samples.json.sampler_config` |

**编译状态：自洽可编译**（`g++ -fsyntax-only` 4 个改动源文件 + 测试驱动全部通过，见 §6 复现命令）。

---

## 001 [C2] UPM 收敛容差：回退到冻结值 + 变更 claim 草案

### 001-1 根因

`docs/algorithms/v6/frozen/01_NUMERIC_THRESHOLD_FREEZE.md:45`（FZ-UPM-CONVERGENCE）冻结
`tol=1e-6` 并明文「改动 tol/σ_floor → rc!=0」；`PHASE2_UPM_IMPL.md:379` / `DATA_SEMANTICS.md:1874`
同值且自述「冻结面，任何修改必须走 SCI/合同变更」。RELEASE-02 P2a-3 在实现内就地改为
`uc.tolerance=1e-3; uc.tolerance_relative=1`（未走变更流程），且 `tolerance_relative` 在
`docs/`+`contracts/` **零出现**；另一条路径 `p2_session.cpp:204` 仍取 `1e-6` ⇒ 两条路径分叉。

### 001-2 修法（合规优先）

`module_adapters.cpp:5239-5240` 回退为 `uc.tolerance = 1e-6; uc.tolerance_relative = 0;`。
相对判据保留为**显式 opt-in** 覆盖键（`upm.tolerance_relative`，默认 0），未经裁决不得启用。 **订正（2026-09-20，V5 分片 3）**：该「裁决」= §9.53 **已批准**（`:1737-1777`）；批准后按 §9.53 落地方向实施（无量纲停止判据 + `converged` 枚举 `0/1/2/3` + 三元组 `rms_z`/`Σw/Σraw_w`/`sigma_residual_dex`），不再是「未经裁决」。
**未改冻结文档**；授权路径落变更 claim 草案（§001-4）。

### 001-3 数值验证（生产尺度合成集：4 帧 × 8×8 control，`control_ivar=5.6e-22`，`M~3e15`）

| 判据 | converged | iterations | objective | max\|C\| | model_hash |
|---|---|---|---|---|---|
| 冻结 `1e-6` 绝对（修复后默认） | **0（如实报未收敛）** | **100（耗尽）** | 6.62038642951427e13 | 575.59307364021538 | `c9cc59d3f1dd8695…` |
| `1e-3` 相对（被回退的 P2a-3） | 1 | 49 | 6.62038995328386e13 | 575.59307066553833 | `96ec51da7e77ab81…` |

- 相对判据有效门 = `1e-3 × max(scale,1)`，`scale≈max|M|≈3e15` ⇒ **3.0e12 ADU**，
  比冻结门 `1e-6 ADU` 宽 **18.477 个数量级**（`001/relative_gate_absurd`）。
- **「不动点相同」的实测口径修正（重要）**：两者 C 场**不是逐位相同**——
  `C_field_bitwise_diff = 190/256`，`max|ΔC| = 0.18985 ADU`（相对 max\|C\| = **3.30e-4**），
  `model_hash` 不同。差异量级与 P2a 报告「每轮仅降 ~0.7%、相对残差稳定 1.15e-4~1.32e-4」一致：
  差异来自**停止轮次不同**（100 vs 49）留下的末轮残差，而非不同不动点。
  ⇒ 任务书中「冻结门下不动点相同」应表述为「**同一不动点邻域内、差异 ~3e-4 相对、迭代更多且如实报未收敛**」。
  这一点已写入变更 claim §2，供负责人裁决时知情。
- 冻结门在此量级下**物理不可达**：`max_dM` 的最小非零值是 3e15 处的 ULP（≈0.5 ADU），
  故 `max_dM ∈ {0} ∪ [0.5,∞)`，永不落入 `(0,1e-6)` ⇒ 判据与量纲不匹配（非「严格」）。

### 001-4 `p2_session.cpp:204` 与节点链一致性（逐位/数值证据）

| 面 | 修复前 | 修复后 |
|---|---|---|
| `tolerance` | node chain `1e-3`+相对 vs session `1e-6` 绝对 ⇒ **语义分叉 18 个数量级** | **两者同为 `1e-6` 绝对**（`p2_session.cpp:204` 未改动，node chain 已回退） |
| 求解入口 | node chain `p2_upm_build_geo` vs session `p2_upm_build` | **仍分叉** |
| 阻尼/全场/gauge | node chain `gs_damping=0.5`、`m_full_frame=1`、`final_gauge=1`（P2a-2/P2a-4）；session 全部取 legacy 默认（1.0/0/0） | **仍分叉** |

**结论（如实）**：001 的**容差分叉已闭合**（同输入同容差同判据）；但「两条路径同输入同结果」
**在当前实现下不成立**，剩余差异由 `gs_damping`/`m_full_frame`/`final_gauge`/求解入口造成。
本分片**不擅自**把这三个参数复制进 session——它们在 `docs/`+`contracts/`+`工程控制/` **零登记**
（`grep -rn 'm_full_frame\|gs_damping\|final_gauge' docs/ contracts/ 工程控制/` = 0 命中），
与 001 属**同一类「就地改科学行为」违规**，须走变更流程定性（补齐 session 或把 session 标为非生产路径）。
已列入变更 claim §4「未决耦合」+ 本报告 §7 上呈清单。

### 001-5 变更 claim 草案要点

`工程控制/RELEASE-02/change-claims/CONFORM-FIX-B-001-tolerance-relative.md`（状态：**已批准**（§9.53:1737-1777）—— **订正（2026-09-20，V5 分片 3）**：原「**草案，待负责人裁决**」作废）：

1. **论证**：冻结门是**绝对**判据，而生产 `max|M|~3e15` ⇒ 冻结门低于该量级 ULP 5.7 个数量级，
   **物理上不可达**；恒不可达的门使「收敛控制」完全失效（永远跑满 100 轮），与冻结条文意图相反
   ⇒ 属「规范本身陈旧/不适定」，应订正而非继续违反。
2. **建议（方案 A，推荐）**：tol 改为**尺度无关相对判据** `tol_rel × max(max|M|, max|C|, 1)`，
   在 `PHASE2_UPM_IMPL.md §13` + `DATA_SEMANTICS §25` 登记 `tolerance_relative` 字段与默认值，
   `PUBLIC_API.md:1827` 覆盖键集补 `tolerance`/`tolerance_relative`；`FZ-UPM-CONVERGENCE` 版本递增。
   （备选 B：门改为 `k × ULP(max|M|)`；备选 C：维持绝对门并把 `converged=0` 升级为发布门 = 停产，不可行。）
3. **取值不预设**：`tol_rel=1e-3` 需收敛专项实测确认（本草案只给证据，不替负责人定值）。
4. **未决耦合**：§001-4 的三个参数分叉须一并裁决。

---

## 003 [C1] `p2_upm_normalized_weights` 绝对门 ⇒ 生产尺度下全部权重归 0

- **根因**：同一归一化公式（`docs/science/PHASE2_UPM.md:56` 只给份额式定义，无阈值）在
  两处实现口径不一致：build 内部 `upm.cpp:646` 已按 FIX-UPMSCALE 改为尺度无关
  `s > 0.0 && std::isfinite(s)`，公共 API `upm.cpp:1646` 仍是绝对门 `s > 1e-12`。
- **修法**：`upm.cpp:1646` 改为与 `:646` **同判据** `(s > 0.0 && std::isfinite(s))`；
  真零权重（Σ=0）仍得 0，非法/缺失 ivar 仍由 `p2_upm_raw_weight` rc=2 拦下（不放宽任何校验）。
- **红绿例（实测）**：

| 例 | 输入 | 旧（`s>1e-12`） | 新（`s>0 ∧ finite`） | 规范期望 |
|---|---|---|---|---|
| 生产尺度 | 4 帧 × 64 control，`control_ivar=5.6e-22` | **256/256 全 0**（per-control Σ=2.24e-21 ≪ 1e-12） | **256/256 = 0.25** | `rel/n = 1/4 = 0.25` |

  ⇒ 判别力：旧必红（全 0 = 「该 control 无观测」）、新必绿（恰等于份额定义值）。

---

## 004 [C1/C3] `ivar` 缺失时伪造 variance 面（违反 fail-closed）

- **规范**：`docs/contracts/DATA_SEMANTICS.md:2387`（合成公式前提 `ivar_product_missing==0`）+ `:2400-2407`
  （unavailable 规则 fail-closed 唯一出口：fallback 发生 ⇒ **不写 variance/ivar 子产品** +
  `uncertainty_available=false` + diagnostics 标红计数，**禁止用 support/snr²/常量 0 伪 variance**）。
- **根因**：`module_adapters.cpp` 旧 `:6669` 在「ivar 缺失 → 帧级 SNR 链闭合」后置
  `uncertainty_available = true;  // SNR 权重 = 合法逆方差面`，绕过 §30.1；
  帧级 SNR 链的绝对标度含 `F_ref²`，而 `F_ref` 正是本轮 A6 查出的缺陷量（CONFORM-SWEEP-1-004）。
- **修法**：该处置 `uncertainty_available = false;` 并置原因键
  `uncertainty_unavailable_reason="ivar_product_missing_frame_snr_fallback"`；原因贯通
  `p2_integrated.json` / `p2_final.json` / 节点 manifest（mode 1 档另有 `weight_mode_1_equal_non_ivar`）。
  权重链**保留**（积分仍有权重、降级显式可见），只是**不再据此发布方差面**。
- **红绿例（节点链端到端，fixture：3 帧、删 `ivar/` 子产品、保留 `ASTROCS_FRAME_SNR`/`ASTROCS_REFERENCE_FLUX`）**：

| 断言 | 旧（基线） | 新（修复后） |
|---|---|---|
| `p2_integrated.json.uncertainty_available` | **true（必红）** | **false** |
| `variance/properties` + `ivar/properties` | **EXISTS（伪方差面落盘）** | **absent** |
| `p2_final.json.products` | `[signal,support,variance,ivar]` | `[signal,support]` |
| `uncertainty_unavailable_reason` | 键不存在 | `ivar_product_missing_frame_snr_fallback` |
| `snr_chain_used` / `ivar_product_missing_frames` | true / 3 | true / 3（降级仍可见） |

---

## 005 [C5] `sampler.cpp` catalog veto 硬编码 10.0 / 0.012

- **根因**：同文件同一物理量两个消费者——star_mask 路径（`:1133`/`:1148`）走 `cfg`，
  catalog veto 现场（旧 `:884-885`）走字面量 `10.0`/`0.012` ⇒ 调用方设 cfg 只改 star_mask，
  不改 veto（`sampler.h:56-59` 自称「原 catalog veto 硬编码整改」未闭合）。
- **修法**：`sampler.cpp:891-895` 改消费 `cfg.star_mask_snr_factor` / `cfg.star_mask_radius_deg`
  （与 `:1133`/`:1148` 同源，默认与修补仍在 `:318-319`/`:513-514`）。
- **红绿例（端到端；fixture 写入 SNR catalogue：每 control 1 颗 `snr=100`，第 1 个 control 额外 1 颗 `snr=1e5`）**：

| 例 | cfg | 旧（硬编码 10.0/0.012） | 新 |
|---|---|---|---|
| 默认（正向对照） | 无 `model` 段 | **veto = 3** | **veto = 3** |
| factor 放宽 | `star_mask_snr_factor=1e6` | **veto = 3（必红：cfg 被忽略）** | **veto = 0**（thr=1e8 > 1e5） |
| radius 收紧 | `star_mask_radius_deg=1e-9` | **veto = 3（必红）** | **veto = 0**（无星在半径内） |
| veto 开关 | `background_catalog_veto=0` | **veto = 3（必红：`model` 段整体不可达）** | **veto = 0** |

- **默认档正向对照（已补齐）**：`005/default_veto_fires` 实测 **veto = 3**（3 帧各 1 次；特亮星 `snr=1e5 > thr=10×100`）。
  *构建期踩坑记录（供复核）*：首版 fixture 漏设 `AIO_HIPS_PRODUCT_SNR=4`（snr/ 子产品的写入门），
  SNR catalogue 被 writer 丢弃 ⇒ veto 恒 0（**假绿**）；补该位后正向对照成立。

---

## 007 / 008 / 009 [C4] 三处默认值不符

### 007 `reject_profile`：stage2 工具默认改自研档

- **依据**：负责人裁决 C2「自研的 ⇒ 改文档对齐代码」（`GAP_AUDIT §9.40`，claim `FIX-SCI-SNR-CANON-001`）；
  `docs/science/REJECTION.md:21,47`、`CONFIG_SCHEMA.md:25`、`contracts/data/phase2_uncertainty_rejection_provenance_v1.json:67` 均为 `astrocs_adaptive_pixel`；
  node chain（`module_adapters.cpp` p2_op_reject 缺省）本就是自研档。
- **修法**：`stage2_common.cpp:262-266` 与 `stage2_common.h:73` 默认 → `P2_PROFILE_ASTROCS_ADAPTIVE_PIXEL`；
  `wbpp_2_9_1` 保留为对照档、`wbpp_current` 仍为 migration alias。
- **红绿例**：`P2Stage2Config{}.reject_profile` 旧 `wbpp_2_9_1`（必红）→ 新 `astrocs_adaptive_pixel`；
  缺 `profile` 键解析：旧 `wbpp_2_9_1` → 新 `astrocs_adaptive_pixel`（实测 PASS）。

### 008 `underdetermined_n`：默认值单一来源化

- **根因**：`CONFIG_SCHEMA.md:26` 写 `2`，pixel 档实为 `3`；代码里同一默认值有**两份拷贝**
  （`stage2_common.cpp:272-277` 与 `rejection.cpp:1155-1162`），且两者在 `extreme_prior` opt-in 档
  **实际分叉**：工具恒给 3，resolver 给 1（`rejection.h:230-234` 冻结：0 = 按 profile/request 默认）。
- **修法**：`stage2_common.h:75-80` 默认 → `0`（= 按 profile/request 默认）；`stage2_common.cpp:268-297`
  缺键时**向权威 resolver 查询** `p2_reject_plan_resolve(...).underdetermined_n`，不再复制字面量。
- **红绿例（实测）**：

| 场景 | 旧 tool | 新 tool | resolver（权威） |
|---|---|---|---|
| `astrocs_adaptive_pixel` + AUTO，缺键 | 3 | **3** | 3 |
| `wbpp_current`（alias→`wbpp_2_9_1`），缺键 | 2 | **2** | 2 |
| `astrocs_adaptive_pixel` + `extreme_value_clip_prior_sigma`，缺键 | **3（与 resolver 分叉，必红）** | **1** | 1 |
| 显式 `underdetermined_n=7` | 7 | 7 | — |

- **规范侧**：`CONFIG_SCHEMA.md:26` 的 `underdetermined_n(2)` 仍陈旧（应为
  `0=profile 默认：wbpp/adaptive=2；astrocs_adaptive_pixel=3；extreme_prior opt-in=1`）——
  属 `docs/**`，**本分片不改**，已列 §7 上呈。

### 009 `smoothing_lambda`：默认值语义对齐（**判不了/默认值未冻结**：§9.55 S5 与 §9.67 定案 5 是否同指未判定；订正 2026-09-20，V5 分片 3）

- **冲突三方**：`CONFIG_SCHEMA.md:19`「`smoothing(auto→0.1)`」 vs `PHASE2_UPM_IMPL.md:382`
  「`0.0（默认关闭平滑）`」（该表自述冻结面） vs 负责人裁决 `GAP_AUDIT §9.39 A5`「**λ 不能为 0**」。
- **修法（只对齐默认值语义，不定生产值）**：
  1. `stage2_common.h:9-21` 新增单一来源常量 `P2_SMOOTHING_LAMBDA_AUTO = 0.1`（注释明示
     「只定义 `auto` 键值解析成什么，不定义生产 λ；生产值归 SMOOTH-LAMBDA 扫描」）；
  2. `stage2_common.h:40-42` struct 默认 → 该常量，`stage2_common.cpp:140` 同源引用
     ⇒ 满足 `CONFIG_SCHEMA.md:3-4`「struct 默认值 == parser 默认值 == docs」；
  3. `module_adapters.cpp:5290-5310` node chain **新增同语义 `model.smoothing` /
     `model.smoothing_lambda` 键消费**（`auto` → 同一常量），并把生效值与来源写入 manifest
     （`upm_smoothing_lambda` / `upm_smoothing_lambda_source`）。
     **缺键仍保持 `upm.h:75` 的编译期默认 `0.0`** —— 该默认属 ALG §13 冻结面，
     **不擅自改**（否则即重犯 001 的「就地改冻结科学量」）。
- **红绿例（实测）**：struct 默认 旧 `0.0` → 新 `0.1`；缺 `smoothing` 键解析 旧 `0.0` → 新 `0.1`；
  `smoothing:0.3` → 0.3（两版一致）；`smoothing:"auto"` → 0.1（两版一致）。
- **⚠ 与裁决的冲突（登记并说明）**：负责人裁决「λ 不能为 0」与 **node chain 生产默认仍为 0.0**
  直接冲突（`module_adapters.cpp` 不设 λ ⇒ 取 `upm.h:75` 的 0.0），而 `PHASE2_UPM_IMPL.md §13`
  把 0.0 写成**冻结面**。⇒ 本分片**不擅自**改冻结默认，只把冲突显式化：
  ① node chain 现在可消费 `model.smoothing`（同语义）；② manifest 增
  `upm_smoothing_lambda_source="compiled_default_alg13_frozen_0.0"` 使该缺口**机器可见**；
  ③ 冲突上呈（§7），生产 λ 取值归 `SMOOTH-LAMBDA` 分片 + 变更流程。
- ⚠ **订正（2026-09-20，V5 分片 3）—— 判不了（默认值未冻结），不写死 `0.1`**：① §9.55 S5（`工程控制/RELEASE-02/GAP_AUDIT.md:1846-1859`）前台裁决「**λ 生产默认取 0.1**（`P2_SMOOTHING_LAMBDA_AUTO=0.1`）…**保留** `λ=0` 作为**显式 opt-in**」；ALG §13 冻结值 `0.0 → 0.1` 走变更 claim 订正。② §9.67 定案 5（`:2584-2588`）「本期**不加**堆叠平滑项」+ 必须做实验验证。⇒ 上文「**未定生产值**」的「未定」性质已变（取值不再属负责人待决项），但**两条裁决是否同指未判定** ⇒ **默认值未冻结**。**缺什么证据**：§9.67 定案 5 原文未出现 `smoothing_lambda`／`P2_SMOOTHING_LAMBDA_AUTO` 字样，两条裁决的适用对象未在裁决文本中对齐（⑦ 批次 `run/RELEASE-02/merge/⑦.md` §4 判不了 1 同结论）⇒ `docs/plugins/algorithms_phase2/11_upm.md §5` 只登记字段、标「未冻结」，本条不擅自选边。

---

## 011 / 012 [C3/C7] `sky_plane` 默认值与 provenance 自洽

### 011 默认值依据（审计建议 `false`，本分片按「是否真的施加」绑定）

- **规范核查结论**：`docs/` 内**无任何**规定 `sky_plane.enabled` 默认值的条款
  （`grep -rn sky_plane docs/` 仅命中 `UNIFIED_MODEL.md:49` 对象描述与
  `UNIFIED_SCIENCE_MODEL.md:122` 的 UNRESOLVED 登记）。
- 唯一「默认开启」记录是 `工程控制/RELEASE-02/ACCEPTANCE.md:11` + `reports/RELEASE-02/FIX-A-report.md:73,140,156`
  的「选项 a」，其**前提**是 FIX-A 目标模型 `(raw−C−b_k)/g_k`；而 `FIX-A-UPM-001` 已被
  `FIX-SCI-SNR-CANON-001`（负责人 2026-09-19，纯加性）**否决**，该 claim §2/§3.1 明文
  「FIX-P2a 默认路径为**保留 C 去 δ**，`raw − C_k`，`g_k ≡ 1` 本期不启用」
  ⇒ ~~默认路径下 δ **从不施加**（`module_adapters.cpp:5503` 起 `additive_mode` 默认 `"c"`，
  且全仓 `config/`+`run/` **无任何**配置写 `additive_mode`）。~~
  **已作废（2026-09-20，V5 分片 3；依据 §9.67 定案 1，`GAP_AUDIT.md:2548-2555`）**：负责人定案 `additive_mode` 默认 = **`delta`** ⇒ `calibrated_k = raw_k − δ_k`、**保留公共天光面 `B_ref`**；`raw − C_k`（全减）**不再是默认**。故「δ 从不施加」不再成立（实现面默认仍为 `"c"` ⇒ 裁决**未落地**，⑦ 批次已登记）。
- **修法**：`sky_plane.enabled` 缺省值由 `true` 改为 **`delta_wanted`（`additive_mode ∈ {delta,both}`）**，
  即**「要施加才构建」**；显式 `sky_plane.enabled` 始终优先；manifest 增
  `sky_plane_enabled` / `sky_plane_enabled_default_source`。
  ⇒ 默认路径不再产出无消费方的 `p2_sky_plane.bin`（稀疏样条拟合 + Schur 解是纯成本），
  且 `additive_mode=delta` 时不会静默退化为 c。
  **登记**：本改动**取代** FIX-A 前提下的「选项 a」；**已确认（§9.55 S8；订正 2026-09-20，V5 分片 3）** —— 原「请负责人在收口验证时确认」作废。⚠ **§9.67 定案 1 追加订正**：`additive_mode` 默认由 `c` 改为 `delta` ⇒ `sky_plane.enabled` 缺省（= `delta_wanted`）实际为 **true**；§9.55 S8 依据行「新默认与默认路径 `raw − C` 一致」**已被 §9.67 定案 1 取代**（冲突已登记）。

### 012 provenance 自洽

- **根因**：旧 `sky_applied = (sky_guard.m != nullptr)`（= 仅**载入**）被写入
  `sky_plane_applied`，与同文档 `delta_subtracted=false` / `sky_plane_mode="none"` /
  `additive_combination="raw_minus_C"` **互斥** ⇒ 按「本次是否做了天光面扣除」取值的消费者必被误导。
- **修法**：拆两键——`sky_plane_loaded`（产物成功载入）/ `sky_plane_applied`（**δ 真的被扣除**），
  `delta_applied = sub_delta && sky_loaded`，`sky_applied = delta_applied`；
  `sky_plane_artifact` 以 `sky_loaded` 为准；manifest 同步增 `sky_plane_loaded`。
- **红绿例（不变式 `applied == (loaded && delta_subtracted)` 且 `mode` 与 `applied` 同真值）**：

| 场景 | 旧 | 新（实测） |
|---|---|---|
| 默认（无 `sky_plane`/`seam` 键） | 默认 enabled=true ⇒ 走构建（白付成本）；若构建失败则 applied=false | `sky_plane_enabled=0`、`status=disabled`、`source=additive_mode_c`；provenance 自洽 |
| `sky_plane.enabled=true` + 默认 `additive_mode=c` | **若载入则 applied=true 与 mode=none 互斥（必红）** | 不变式成立（`loaded=0 applied=0 dsub=0 mode=none`，构建欠定 ⇒ `status=fallback_build_failed` 显式登记） |
| `seam.additive_mode=delta` | 同上 | `sky_plane_enabled=1`（默认随 δ 绑定）、不变式成立 |

- **未闭合项（如实登记）**：`loaded=true && applied=false` 的**真实两态**在本 fixture（3 帧/64 control）
  上不可达——天光面拟合在此规模**欠定 fail-closed**（`status=fallback_build_failed`，与 `GAP_AUDIT §9.13`
  的 rc=6/欠定同源）。故 012 的红绿证据目前是**不变式判别**（旧实现违反不变式即红），
  「载入但未施加」的真两态需更大 fixture（≥6 帧、可辨天光结构）在后续批次补齐（§7 未完成项）。

---

## 014 [C3] `p2_op_sample` 不透传 sampler 配置

- **根因**：`module_adapters.cpp` 旧 `:4928-4931` 只设 `sc.cpu_workers` ⇒ `CONFIG_SCHEMA.md:11-18`
  「`model:` 段」的 13 个 sampler 键（+`sampler.h` 新增 `star_mask_*`/`control_k_corr`）
  在生产 node chain 上**完全不可达**，科学参数钉死编译期默认值，与 stage2 工具分叉。
- **修法**：新增 `p2_sample_cfg_from_doc()`（`module_adapters.cpp:4909-5045`），按 `model:` 段
  **逐字段显式赋值 + 域校验 fail-closed**（键集与 `CONFIG_SCHEMA.md:11-18` 对齐，含
  `star_mask_snr_factor`/`star_mask_radius_deg`/`control_k_corr`；`patch_radius_leaf` 为主名、
  `patch_radius_pixels` 为工具别名）；`cpu_workers` **恒最后赋值**，仍以 Runtime lease 为唯一权威。
  生效配置全量落 `p2_samples.json.sampler_config`，manifest 增 `sampler_config_source`。
- **红绿例（实测）**：

| 例 | 旧 | 新 |
|---|---|---|
| `model.star_mask_snr_factor=1e6` | 忽略（落盘无该键 / veto 不受控） | `sampler_config.star_mask_snr_factor=1000000.0` |
| `model.min_samples=2` | 忽略（恒 5） | `sampler_config.min_samples=2` |
| `model.min_samples=0`（非法） | 静默忽略 | **fail-closed**：`sampler config rejected (fail-closed): model.min_samples 必须 >= 1` |
| 缺 `model` 段 | 默认值 | 默认值 + `sampler_config_source="compiled_defaults"` |

---

## 6 复现命令（不跑 ninja/cmake/ctest）

```bash
export TMPDIR=/dev/shm/astrocs_cfb
cd '/workspace/Astro CS Database'
# 语法检查（4 个改动源文件 + 测试驱动）
bash -c 'source /dev/null; g++ -fsyntax-only … '   # 见 logs/syntax.log（本报告 §0 已给结论）
# 判别力测试：新实现（应全绿）/ 审计基线实现（应多条红）
bash run/RELEASE-02/conform-fix-b/tests/build_and_run.sh new
bash run/RELEASE-02/conform-fix-b/tests/build_and_run.sh old
# 单条
bash run/RELEASE-02/conform-fix-b/tests/build_and_run.sh new 004
```

`build_and_run.sh old` 先用 `revert_to_baseline.py` 把 4 处实现**逆向还原**成审计基线行
（任何一处未命中即非零退出，禁静默半还原），再独立重链 ⇒ 同一测试源码在旧实现上必红。

---

## 7 已裁决事项与裁决出处（原「需上呈的规范侧问题（本分片**不改规范**）」；**订正（2026-09-20，V5 分片 3）**）

| # | 问题 | 位置 | 建议 |
|---|---|---|---|
| S1 | `tol=1e-6` 绝对门在生产量级物理不可达（判据与量纲不匹配） | `01_NUMERIC_THRESHOLD_FREEZE.md:45`、`PHASE2_UPM_IMPL.md:379`、`DATA_SEMANTICS.md:1874` | 变更 claim 草案已落（`CONFORM-FIX-B-001`），请裁决 |
| S2 | `tolerance_relative` 字段与覆盖键未入任何合同表 | `DATA_SEMANTICS.md:1860-1880/:1966`、`PUBLIC_API.md:1827` | 随 S1 一并登记或删除该字段 |
| S3 | `gs_damping`/`m_full_frame`/`final_gauge` 三个**已进入生产 node chain** 的科学参数在 `docs/`+`contracts/`+`工程控制/` **零登记** | `module_adapters.cpp:5241-5247`（P2a-2/P2a-4） | 与 001 **同类违规**；须走变更流程，并裁决 `p2_session` 与 node chain 的分叉定性 |
| S4 | `CONFIG_SCHEMA.md:26` `underdetermined_n(2)` 与 pixel 档 3 不符 | `docs/development/CONFIG_SCHEMA.md:26` | 改为 `0=profile 默认（pixel=3 / 其余=2 / extreme_prior opt-in=1）` |
| S5 | `smoothing_lambda` 默认值三方冲突，且负责人裁决「λ 不能为 0」与 ALG §13 冻结 `0.0` 冲突 | `CONFIG_SCHEMA.md:19` vs `PHASE2_UPM_IMPL.md:382` vs `GAP_AUDIT §9.39 A5` | 需变更流程把 `auto→0.1` 或裁决值写入 ALG 冻结表；生产值归 SMOOTH-LAMBDA |
| S6 | `PHASE2_UPM_IMPL.md:389` 把「已被实现修掉的缺陷」（`s>1e-12`）写成现行规范 | `docs/algorithms/PHASE2_UPM_IMPL.md:388-393` | 拆分为「build 内尺度无关门」与「公共 API 应为同判据」（003 已修实现，文档待订正） |
| S7 | sky_plane 模型地位：`UNIFIED_SCIENCE_MODEL.md:122` UNRESOLVED vs `PHASE2_UPM.md:182` 已关闭 | 两份 FROZEN 文档 | 按 `FIX-SCI-SNR-CANON-001` 订正 UNIFIED_SCIENCE_MODEL（与 011 的默认值裁决相关） |
| S8 | 「选项 a（sky_plane 默认开启）」的前提已随 `FIX-A-UPM-001` 被否决而消失 | `ACCEPTANCE.md:11`、`FIX-A-report.md:73,140,156` | 确认 011 的新默认（按 additive_mode 绑定） |

> **逐条裁决（2026-09-20，V5 分片 3 补登；上表「建议」列保留为 2026-09-19 写作时留痕）**：
> S1 ⇒ **§9.53 批准**（`:1737-1777`，`CONFORM-FIX-B-001` 已批准）；S2 ⇒ §9.55「其余裁决」S2（**补登记**，与 §9.53 一并）；S3 ⇒ **§9.55 S3**（按 `ENGINEERING_SPEC §3` **补登记**三参数 + **闭合两路径**）；S4 ⇒ §9.55 S4（订正为 `0=按 profile 解析`；须同步删 `config_consistency_known_divergences.json` 台账条目）；S5 ⇒ **§9.55 S5**（λ 生产默认 `0.1`，`λ=0` 仅显式 opt-in；⚠ 与 §9.67 定案 5 是否同指**未判定**）；S6 ⇒ §9.55 S6（**订正**文档）；S7 ⇒ **§9.55 S7**（以「已关闭」为准，订正另一份）；S8 ⇒ **§9.55 S8**（**确认采纳**；⚠ 依据行「与默认路径 `raw − C` 一致」已被 **§9.67 定案 1** 取代 ⇒ 缺省实际为 true）。
> ⇒ 上文标题原义「**需上呈**」与各行「请裁决 / 须走变更流程 / 确认」的上呈状态**全部作废**。

---

## 8 未完成项 / 风险点（**不做半成品声明**）

1. ~~005 默认档正向对照未复现 veto~~ **已闭合**（fixture 补 `AIO_HIPS_PRODUCT_SNR` 写入门后 `veto=3`；新实现 49 PASS / 0 FAIL，旧基线 15 FAIL 中 005 占 3 条）。
2. **012 的 `loaded=true && applied=false` 真两态未在本 fixture 复现**（天光面拟合欠定 fail-closed）。
   当前以不变式判别承载；真两态需 ≥6 帧 fixture。
3. **`p2_session` 与 node chain 仍不同结果**（S3）：001 只闭合了容差分叉；
   其余三个参数分叉**未动**（不擅自复制未登记的科学参数）。
4. **测试未注册进 CMake**：`cfb_tests.cpp` 是独立重链驱动（硬约束禁跑 cmake），
   未加入 `tests/unit/CMakeLists.txt`。后续批次应把它登记为正式回归测试（含 005/012 的补齐用例）。
5. **未跑全量回归**（禁 `ctest`）：本次只做了 `-fsyntax-only` + 独立重链的**目标用例**，
   未验证对既有 460 用例的影响。**已知可能受影响面**：
   `stage2_common.h` 的 struct 默认（`reject_profile`/`underdetermined_n`/`smoothing_lambda`）
   会改变未显式指定这些键的 stage2 配置解析结果 ⇒ 建议前台收口验证时重点跑
   `phase2_routing`、`phase2_synthetic_gate`、`phase2_sampler*`、`p2002_unc_rej_prov_test`、
   `p2001_real_nodes_test`、`p2_seam_gate_test`。
6. **011 的默认值改动取代了 FIX-A 前提下的「选项 a」**（`ACCEPTANCE.md:11`）——
   有据可依（`FIX-SCI-SNR-CANON-001` 默认路径 raw−C），但**请负责人在收口验证时确认**。 ⇒ **已确认（§9.55 S8；订正 2026-09-20，V5 分片 3）**：`sky_plane.enabled` 缺省 = `delta_wanted`（`additive_mode ∈ {delta,both}`）；⚠ 依 **§9.67 定案 1**（`:2548-2555`）`additive_mode` 默认改 `delta` ⇒ 缺省实际为 true；原文「默认路径 raw−C」为 §9.54 前台裁决口径，**已被 §9.67 定案 1 取代**（冲突已登记）。

## 9 硬约束自查

| 约束 | 状态 |
|---|---|
| 零 git 写 | ✅ 未执行任何 git 写命令（只读 `git status/diff/show/rev-parse`） |
| 不跑 ninja/cmake/ctest | ✅ 只用 `g++ -fsyntax-only` + 独立 `g++` 重链 |
| 不改 `docs/**` | ✅ 仅新增 `工程控制/RELEASE-02/change-claims/CONFORM-FIX-B-001-tolerance-relative.md`（草案） |
| 不改 `ci/checks.json` / `config/**` / `contracts/**` | ✅ 未触碰（工作树中这些文件的 `M` 状态来自其它分片，早于本分片开工） |
| 报告落 `reports/RELEASE-02/conform-fix-b.md` | ✅ |
| 产物落 `run/RELEASE-02/conform-fix-b/` | ✅ |
| `TMPDIR=/dev/shm/astrocs_cfb` 并清理 | ✅（收尾清理） |
| 不改冻结面（tol/σ_floor、SCI/ALG 冻结定义） | ✅ 001 回退到冻结值；009 未改 ALG 冻结默认 |
---

## 10 红/绿总表（独立重链实测；日志 `run/RELEASE-02/conform-fix-b/logs/{new,old}.log`）

```text
NEW (修复后实现)      : == CFB[new] PASS=49 FAIL=0 ==
OLD (审计基线还原实现): == CFB[old] PASS=34 FAIL=15 ==
```

**旧基线 15 条 FAIL = 各条目的「必红」证据（实测输出）**

| 条目 | 旧基线红证据 |
|---|---|
| 003 | `nonzero=0/256`（全 0 权重）；「新值 == rel/n=0.25」不成立 |
| 004 | `uncertainty_available=true reason=`（原因键不存在）；`variance/properties=EXISTS ivar/properties=EXISTS`；`products=[signal,support,variance,ivar]` |
| 005 | `star_mask_snr_factor=1e6 ⇒ veto=3`；`radius=1e-9 ⇒ veto=3`；`catalog_veto=0 ⇒ veto=3`（cfg 全被忽略） |
| 007 | `profile=wbpp_2_9_1`（对照档，与 SCI/CONFIG_SCHEMA/node chain 冲突） |
| 008 | `underdetermined_n=2`（pixel 档应为 3） |
| 011 | `sky_plane_enabled=1 status=fallback_build_failed source=additive_mode_c`（默认白付拟合成本且失败） |
| 014 | `model.star_mask_snr_factor` 被忽略（落盘仍 10.0）；`min_samples=0` 非法值 **chain unexpectedly ok**；`min_samples=2` 被忽略（落盘 5） |
| 001 | 见下注 |
| 012 | 旧基线在本 fixture 上**未红**：3 帧/64 control 天光面拟合欠定 fail-closed ⇒ `loaded=false`，`applied=true` 与 `mode=none` 的互斥态不可达；012 判别力当前由**不变式**承载（见 §8-2） |

> **001 红证据注**：`revert_to_baseline.py` 已把 `uc.tolerance=1e-3; tolerance_relative=1` 还原进 old 构建
> （日志 `[revert] module_adapters.cpp: reverted 1 site`）；`001/numbers` 用例**显式构造两种判据**做对照
> （不依赖默认值），故 old 上仍 PASS；红证据由 `001/relative_gate_absurd`（3.0e12 vs 1e-6，宽 18.477 数量级）
> 与双判据对照共同承载。

> **old 构建的等价性边界（诚实声明）**：`old` 变体还原了 **10 个行为位点**，
> 但保留了本分片新增的 `#include "astro/phase2/stage2_common.h"` 一行（无行为影响）⇒
> 不是逐字节的审计基线，而是**行为等价的基线**。
