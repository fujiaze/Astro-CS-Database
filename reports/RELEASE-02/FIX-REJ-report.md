# FIX-REJ 报告 — Phase2 逐像素排异：卫星线残留根因 + WBPP/Siril 自动选择逻辑 + 按 n 映射表

- 控制包：RELEASE-02 / 任务 FIX-REJ
- 分片：排异专项（SubAgent；**零 git 写权限、未改代码、未构建**）
- 日期：2026-09-18
- 结论状态：**已收敛**（根因有 file:line + 实测数据；实验可复现）
- 变更 claim：`工程控制/RELEASE-02/change-claims/FIX-REJ-001.md`
- 证据/脚本：`run/RELEASE-02/FIX-REJ/`

---

## 0 一句话根因

**排异子系统完整接线且逐像素生效，但 AUTO 只在「整组总帧数」上解析一次（L4 m42 12 帧 → winsorized），而该马赛克 97.71% 的输出像素实际只有 n=2/3/4 个候选：n=2 被 `underdetermined_n=2` 白名单全接受、n=3/4 被 Siril 语义的 `N-r<=4` 最小保留闸判为零拒绝 ⇒ 全图仅 2.29% 的像素具备任何排异能力，卫星线原样进入叠加。**

---

## 1 根因诊断（证据链 + 数据）

### 1.1 全链是通的（先排除"没接线"）

| 环节 | 证据 |
|---|---|
| planning 解析 | `lib/infrastructure/scheduler/src/module_adapters.cpp:4413-4423` `p2_reject_plan_resolve` |
| 逐像素候选收集 | `module_adapters.cpp:4496-4518` 每像素 `p2_collect_candidate_stack`（frame-major，`value_stride=tile_span`） |
| 逐像素 kernel | `module_adapters.cpp:4525-4537` `p2_reject_stack_ex`；kernel 见 `lib/algorithms/coverage/src/rejection.cpp:1698-1876` |
| 逐样本 mask 落盘 | `module_adapters.cpp:4544-4552,4574,4593-4596` `p2_rejection_sample_mask.bin`（索引 `[slot*tile_span+p]`） |
| integrate 逐样本剔除 | `module_adapters.cpp:4899-4914` `sm==0 → continue`；`:4988` 尺寸/空洞 fail-closed；`p2_integrated.json` 记 `sample_mask_consumed=true` |
| 阈值继承守卫 | `module_adapters.cpp:1703-1706` / `rejection.cpp:2118` `p2_reject_plan_thresholds_inherited` |

⇒ 不是"没调用 kernel"，也不是"帧级排异"，更不是"等权/legacy 路径绕开了排异"。

### 1.2 四层能力塌缩

**R1 解析粒度（决定性）**：`module_adapters.cpp:4414-4418`

```cpp
req.request = P2_REJECT_AUTO;
req.nominal_contributors = static_cast<std::uint32_t>(frames.size());   // ← 全组帧数
const std::string profile = doc.value("reject_profile", std::string("wbpp_current"));
```

`rejection.h:183-189` 明示 `wbpp_current` = "group-level 一次解析；tile/pixel 不重选"。
L4 m42 组帧数 12 ⇒ `rejection.cpp:1073-1080` 路由 `6≤n≤15 → P2_REJECT_WINSORIZED_SIGMA`，与实测 `plan.method=2` 一致。

**R2 n=2 白名单**：`module_adapters.cpp:4522-4524` + `rejection.cpp:1759-1765`

```cpp
if (eligible_count > 0 && eligible_count > plan.underdetermined_n && eligible_count >= plan.minimum_n) { ...kernel... }
else { /* UNDERDETERMINED：全接受并记录 */ }
```

`underdetermined_n=2`（`rejection.h:155`）⇒ **n=2 永不进入 kernel**。

**R3 winsorized 的 `N-r<=4` 最小保留闸**：`rejection.cpp:1346`

```cpp
for (std::uint32_t i = 0; i < n; ++i) {
    if (!accept[i]) continue;
    if ((int)nc - r <= 4) break;      // ← n≤4 时立即 break，零拒绝
    ...
}
```

忠实 Siril 1.4.3：`src/stacking/rejection_float.c:188,239,273`（SIGMA/MAD、WINSORIZED、LINEARFIT 同闸），ushort 路径 `median_and_mean.c:791,845,880`。⇒ **n=3/4 即便进入 kernel 也零拒绝**。

**R4 n≤4 全拒容错**：`rejection.cpp:1860-1867` 全拒时回退全接受（为让 32 帧马赛克落盘而加）。这一条在 n=2 时会把 percentile 的有效拒绝**反转为全接受**（见 §5.2）。

### 1.3 实测量化（RELEASE-01 L4 m42）

证据：`run/RELEASE-01/e2e/evidence/l4__p2_m42/p2_rejection.json`

- `profile=wbpp_current`，`plan.method=2`（`astrocs.winsorized_sigma_siril_1_4_3.v1`），`minimum_n=3`，`underdetermined_n=2`
- `stats`：`accepted_pixels=137,101,312`、`rejected_high=16,932`、`rejected_low=5,044`、`rejected_samples=21,976`、`underdetermined_pixels=101,649,311`
- depth 分布（脚本 `rejection_n_experiment.py` §E 重算）：n=2 → 98,304,000 px（71.7%）；n=3 → 2,359,296；n=4 → 33,292,288；n=5 → 524,288；n=8 → 2,621,440
- **现行路径有排异能力的像素 = 3,145,728 / 137,101,312 = 2.29%**

对照：L3/L4 gc（2 帧组）AUTO → `percentile`（method=7），depth 全为 1/2 ⇒ `underdetermined_pixels` 几乎等于全部像素、`rejected_*=0`（`run/RELEASE-01/e2e/evidence/l3__p2_gc_wm1/p2_rejection.json` 等）。

### 1.4 逐条排除（负责人列的 5 种可能）

| 假设 | 判定 | 证据 |
|---|---|---|
| AUTO 在 n 小时本就不可判 | **部分否** | n=3/4 按 n 解析会走 percentile（无 `N-r<=4` 闸），实测剔除率 1.000（§5.1）；错配才是主因 |
| 阈值/参数过宽 | **否** | 继承阈值与 WBPP 默认逐项一致：`rejection.cpp:1053-1061` ↔ WBPP `BPP-parameters.js:740-743`（0.2/0.1、4.0/3.0） |
| L4 走 legacy/等权路径未调 kernel | **否** | L4 m42 `weight_mode=1`（等权）但 mask 逐样本被消费（`module_adapters.cpp:4899-4914`）；排异"没生效"= 能力为零 |
| E2E config 把 rejection 关了 | **否（但配置面有独立缺陷）** | L4 config 无 reject 字段；且 `reject` 块只校验不读、方法硬编码 AUTO（`module_adapters.cpp:4414,5556`）；`algorithm_rejection_method` 无 C++ 消费者 |
| 排异只在帧级 | **否** | kernel 输入为逐像素 `P2CandidateStack`（`module_adapters.cpp:4496-4534`），mask 逐样本索引 |

---

## 2 WBPP / Siril 自动选择逻辑对照表

### 2.1 WBPP（PixInsight 官方脚本；本机源码 `run/RELEASE-02/FIX-REJ/wbpp/BatchPreprocessing/`）

| 维度 | 事实 | 行号 |
|---|---|---|
| 自动路由 | `n<6 → PercentileClip`；`6≤n≤15 或 BIAS/DARK → WinsorizedSigmaClip`；`n>15 → LinearFit` | `BPP-FrameGroup.js:1304-1312` |
| n 的口径 | `bestRejectionMethod` 用 `activeFrames().length`；`rejectionIsGood` 用 `fileItems.length`（两者口径不同） | `:1305` / `:1249` |
| auto 常量 | `ImageIntegration.prototype.auto = 999` | `BPP-global.js:180` |
| 显式拒绝 | NoRejection、MinMax、CCDClip | `:1237,1239,1241` |
| 各方法约束 | percentile ≤8；sigma 8–15；winsorized ≥8；averaged 8–10；linear fit ≥8（<20 提示）；ESD ≥8（<20/<25 提示）；RCR ≥15 | `:1252-1287` |
| 默认参数 | percentile 0.2/0.1；sigma 4.0/3.0 | `BPP-parameters.js:740-743` |
| 消费点 | `rejection: integrationGroup.bestRejectionMethod()` | `BPP-processing.js:247,338` |

**WBPP 内部矛盾（如实登记，不照抄文案）**：
1. 文案 vs 代码：LinearFit "at least 15" ↔ 代码 `n<8`；ESD "at least 15" ↔ 代码 `n<8`；
2. `case Rejection_ESD` **缺 break** → 贯穿落入 RCR 分支（`:1278-1285`），ESD 实际仅 n≥25 判可用；
3. auto（n<6→percentile）与 validator（winsorized n≥8）在 **6≤n≤7** 冲突：auto 选出的方法会被自家 validator 判不可用。

### 2.2 Siril 1.4.3（GPL-3.0，只读；仓外取证）

| 维度 | 事实 | 行号 |
|---|---|---|
| 是否有按 n 自动选择 | **没有**。枚举仅 `{NO_REJEC, PERCENTILE, SIGMA, MAD, SIGMEDIAN, WINSORIZED, LINEARFIT, GESDT}`；默认 `NO_REJEC`；GUI 直接取 combobox | `src/core/settings.h:37-46`；`src/stacking/stacking.c:641`；`src/gui/stacking.c:132,248,271` |
| 排异适用范围 | 仅 mean 叠加（median/min/max 无排异） | `src/stacking/stacking.c:131` |
| `N-r<=4` 闸 | SIGMA/MAD、WINSORIZED、LINEARFIT 均"剩 ≤4 不再拒绝" | `src/stacking/rejection_float.c:188,239,273`；`median_and_mean.c:791,845,880` |
| winsorized 语义 | `σ ← 1.134 × sd(winsorized@median±1.5σ)`，`|Δσ|≤5e-4σ` 收敛 | `rejection_float.c:223-259` |
| percentile 语义 | 乘性：`median−v > median·plow` / `v−median > median·phigh`；**无 `N-r<=4` 闸** | `rejection_float.c:31-44` |
| GESDT | `max_outliers = n·sig[0]`，临界值表按 `iter+removed` 索引 | `rejection_float.c:301-348` |

**AstroCS ↔ Siril 对照**：`rejection.cpp:1299-1358`（winsorized）与 `rejection_float.c:223-259` 逐式一致（含 1.134、±1.5σ、5e-4、`N-r<=4`）⇒ 复刻保真，n≤4 零拒绝是 **Siril 的设计语义**，不是 AstroCS 的 bug。

### 2.3 三家选择逻辑对照

| | AstroCS 现状 | WBPP | Siril |
|---|---|---|---|
| 是否按 n 选 | 是，但用**全组帧数**、一次解析 | 是，用 group 帧数、一次解析 | **否**，用户手选 |
| n<6 | winsorized（若组帧数≥6） | percentile | — |
| 6≤n≤15 | winsorized | winsorized | — |
| n>15 | linear_fit | linear_fit | — |
| 小 n 下限 | `underdetermined_n=2` 全接受 + `N-r<=4` 闸 | percentile 无 `N-r` 闸（可拒 n=3..8） | `N-r<=4` 闸（与 AstroCS 同） |
| 不适用组合 | 静默执行 | `rejectionIsGood` 判"不可用" | 无校验 |

---

## 3 算法—论文/权威出处表（已核验）

| 算法 | AstroCS 语义 ID | 出处 | 核验 |
|---|---|---|---|
| generalized ESD | `astrocs.generalized_esd_nist.v1` | Rosner, B. 1983, *Technometrics* **25**(2) 165–172, **DOI 10.1080/00401706.1983.10487848**；NIST/SEMATECH e-Handbook **§1.3.5.17.3**（λ_i 公式；"very accurate for n≥25, reasonably accurate for n≥15"） | ✅ 一手（DOI + NIST 全文） |
| Grubbs / 已知 σ 单离群 | 提案 `astrocs.extreme_value_clip_prior_sigma.v1` | NIST/SEMATECH e-Handbook **§1.3.5.17.1**（Grubbs 1969；Stefansky 1972） | ✅ 一手（NIST 全文） |
| sigma clip（median+MAD） | `astrocs.robust_mad_clip.v1` | Beers, Flynn & Gebhardt 1990, *AJ* **100**, 32, **DOI 10.1086/115487**；Astropy `sigma_clip`/`mad_std` | ✅ DOI 解析；✅ Astropy 文档 |
| winsorized sigma | `astrocs.winsorized_sigma_siril_1_4_3.v1` | Siril 1.4.3 `rejection_float.c:223-259`（冻结语义源）；Wilcox 2012 *Introduction to Robust Estimation and Hypothesis Testing* 3rd ed.（ISBN 978-0-12-386983-8） | ✅ 源码；📌 教科书登记 |
| averaged sigma | `astrocs.averaged_sigma.v1` | IRAF `imcombine` `avsigclip`；Tody 1986 *Proc. SPIE* **627**, 733, **DOI 10.1117/12.968154** | 📌 登记（IRAF 帮助页 403/404，未取一手；公式与 `rejection.cpp:1361-1396` 一致） |
| percentile | `astrocs.percentile_siril.v1` | Siril 1.4.3 `rejection_float.c:31-44`；IRAF `imcombine` `pclip` 同源 | ✅ 源码 |
| median/MAD clip | `astrocs.median_std_clip.v1` | Siril 1.4.3 `rejection_float.c:174-222`（MAD/SIGMEDIAN）；Beers+1990 | ✅ 源码 |
| linear fit | `astrocs.linear_fit_siril_1_4_3.v1` | Siril 1.4.3 `rejection_float.c:260-300` | ✅ 源码 |
| min/max | `astrocs.minmax.v1` | WBPP `BPP-FrameGroup.js:1239`（"should not be used for production work"）；IRAF `imcombine` `minmax`；NIST §1.3.5.17.1（极值统计） | ✅ WBPP 源码；📌 IRAF 登记 |
| 大尺度 grow | `astrocs.large_scale_rejection.v1` | WBPP Large-Scale Pixel Rejection（`largeScaleClipLow/High` 默认关闭） | ✅ 源码 |

> 核验级别说明：✅ = 本次取得一手内容（源码行号 / DOI 解析 / NIST 全文）；📌 = 仅登记来源，未取得一手页面（已在 claim §2.3 标注）。

---

## 4 建议的按 n 映射表 + 不适用判定表

### 4.1 内置映射（auto；n = 该输出像素的候选样本数 `eligible_count`）

| n | 方法 | 参数 | 依据 |
|---|---|---|---|
| 0 | —（不输出像素） | — | — |
| 1 | `none` + provenance UNDERDETERMINED | — | 无对照量 |
| **2** | **`extreme_value_clip_prior_sigma`（新增）** | `k=Φ⁻¹(1−α/(2N))`，α=0.05 | 实测 percentile 在 Δ>S 时全拒→容错全接受（§5.2）；NIST §1.3.5.17.1 |
| **3–7** | `percentile` | 0.2 / 0.1 | WBPP auto n<6；validator 要求 winsorized n≥8 → 延到 7 |
| **8–15** | `winsorized_sigma` | 4.0 / 3.0 / 8 iter | WBPP auto ∩ validator |
| **≥16** | `linear_fit` | 5.0 / 3.5 / 8 iter | WBPP auto n>15（16–19 发 WARN） |

仅显式可选：`robust_mad_clip`、`averaged_sigma`(8–10)、`generalized_esd`(≥25)、`median_sigma`、`rcr`(≥15)、`minmax`、`none`。

### 4.2 不适用判定表（WARN 级；error 只留给"方法名不存在/表达式语法错/缺终止段"）

| 方法 | WARN+确认 区间 | 依据 |
|---|---|---|
| `none` | 恒 | WBPP `:1237`；DESIGN §4.5.1 |
| `minmax` | 恒 | WBPP `:1239` |
| `percentile` | n>8 | WBPP `:1252-1254` |
| `percentile` | n≤4 且全拒回退 | `rejection.cpp:1860-1867` |
| `sigma` | n<8 或 n>15 | WBPP `:1256-1260` |
| `winsorized_sigma` | n<8 | WBPP `:1262-1264` |
| `winsorized_sigma`/`median_sigma`/`linear_fit` | n≤4 | `rejection.cpp:1346/1457/1647` |
| `averaged_sigma` | n<8 或 n>10 | WBPP `:1266-1270` |
| `linear_fit` | 8≤n<20（WARN，不确认） | WBPP `:1272-1276` |
| `generalized_esd` | n<25 | WBPP `:1278-1283`；NIST §1.3.5.17.3 |
| `rcr` | n<15 | WBPP `:1285-1287` |
| 自定义分段任一档 | 命中即 WARN（不阻塞） | DESIGN §4.5.5 |

表达式/分段语法（冻结提案）见 claim §5：形式 A `method_map`（JSON 原生，first-match-wins + 强制终止项）与形式 B `method_expr`（`n<=K->method; ...; else->method`），error 仅三种（缺终止/未知方法/非法区间）。

---

## 5 合成卫星线注入实验（能红能绿）

- 脚本：`run/RELEASE-02/FIX-REJ/rejection_n_experiment.py`
- 输出：`run/RELEASE-02/FIX-REJ/out/rejection_n_experiment.json`
- 方法：**Python 逐行语义复刻** `rejection.cpp` 的 kernel 与 `p2_reject_stack_ex` 驱动（含 gate / normalization / n≤4 全拒容错），并用已冻结的 C++ 断言自证保真。
- 自证：**25/25 PASS**（锚点 `lib/algorithms/coverage/tests/synthetic_gate.cpp:425-486` M4A01 + `tests/unit/p2_rejection_test.cpp:40`）。
- 说明：本分片硬约束"只跑 Python、不跑构建"，故**未执行 C++ kernel**；真实 kernel 的验证命令见 §7。复刻保真由上述 C++ 断言锚点保证。

### 5.1 现行 vs 按 n（卫星线 +50σ）

| n | 现行 method | 现行剔除率 | 按 n method | 按 n 剔除率 |
|---|---|---|---|---|
| 2 | winsorized | **0.000** | percentile | 0.502 |
| 3 | winsorized | **0.000** | percentile | **1.000** |
| 4 | winsorized | **0.000** | percentile | **1.000** |
| 5 | winsorized | 1.000 | percentile | 1.000 |
| 6–12 | winsorized | 1.000 | winsorized | 1.000 |
| 16–30 | winsorized | 1.000 | linear_fit | 1.000 |

### 5.2 n=2 的关键反例（percentile 不可用）

| Δ/S | percentile 剔除污染帧 | 全拒回退→全接受 | 先验 σ 极值 |
|---|---|---|---|
| 0.10 | 0.000 | 0.000 | **1.000** |
| 0.30 | 1.000 | 0.000 | 1.000 |
| 0.50 | 0.493 | 0.507 | 1.000 |
| **0.80 / 1.0 / 2.0 / 5.0** | **0.000** | **1.000** | 1.000 |

⇒ **亮卫星线（Δ>S）在 n=2 下 percentile 必然全拒 → R4 容错 → 全接受**。这是"卫星线没被去除"的最直接机制，也是必须新增先验 σ 方法的理由。

### 5.3 各方法单离群能力矩阵（Δ=50σ）

| method | n=2 | n=3 | n=4 | n=5 | n=6 | n=8 | n=12 | n=20 |
|---|---|---|---|---|---|---|---|---|
| percentile | 0.495 | 1.000 | 1.000 | 1.000 | 1.000 | 1.000 | 1.000 | 1.000 |
| winsorized | 0.000 | 0.000 | 0.000 | 1.000 | 1.000 | 1.000 | 1.000 | 1.000 |
| median_sigma | 0.000 | 0.000 | 0.000 | 0.000 | 0.000 | 0.000 | 1.000 | 1.000 |
| linear_fit | 0.000 | 0.000 | 0.000 | 0.000 | 0.000 | 0.000 | 1.000 | 1.000 |
| generalized_esd | 0.000 | 0.721 | 1.000 | 1.000 | 1.000 | 1.000 | 1.000 | 1.000 |
| minmax(min_kept=4) | 0.000 | 0.000 | 0.000 | 0.000 | 1.000 | 1.000 | 1.000 | 1.000 |

> 注：ESD 在 n=3/4 数值上"能剔"，但 NIST 明示其临界值近似仅对 n≥15 起可靠、n≥25 准确 ⇒ **不得**用于小 n（列入 §4.2 WARN）。minmax 因 `min_kept=4` + 删 1 低 1 高，实际需 n≥6。

### 5.4 n=2 先验 σ 极值（提案方法）

| n | percentile 剔除率 | percentile 全拒回退 | 先验 σ 剔除率 | 先验 σ 假阳率 |
|---|---|---|---|---|
| 2 | 0.511 | 0.488 | **1.000** | 0.0000 |
| 3 | 1.000 | 0.000 | 1.000 | 0.0000 |
| 4 | 1.000 | 0.000 | 1.000 | 0.0000 |

### 5.5 订正前后覆盖率（L4 m42 实测 depth 分布）

| 路径 | 有排异能力的像素 | 占比 |
|---|---|---|
| 现行 | 3,145,728 | **2.29%** |
| 订正（按 n + n=2 先验 σ） | 137,101,312 | **100%** |

---

## 6 需前台运行的构建/测试命令（本分片未执行）

```bash
export TMPDIR=/dev/shm/astrocs_rej

# 1) 构建（含按需工具 rejection_cli）
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build astrocs astrocs-stage2 phase2_synthetic_gate \
      phase2_rej_nonfinite_weights p2_rejection_test rejection_cli

# 2) 排异相关测试（kernel 语义 + 非有限权重负例 + 冻结断言）
ctest --test-dir build --output-on-failure \
      -R 'p2_rejection|phase2_synthetic_gate|phase2_rej_nonfinite_weights'

# 3) 真实 kernel 的 Oracle 对照（需先构建 rejection_cli）
export ASTROCS_REJECTION_CLI="$PWD/build/lib/algorithms/coverage/rejection_cli"
python3 lib/algorithms/coverage/tools/rejection_oracle_compare.py
python3 lib/algorithms/coverage/tools/rejection_matrix.py      # N=2..500 × 污染矩阵

# 4) 本报告的语义复刻实验（纯 Python，可独立复现；应 25/25 PASS）
python3 run/RELEASE-02/FIX-REJ/rejection_n_experiment.py

# 5) 端到端复跑 L4 m42（改实现后；产物与证据目录同名便于对比）
timeout 7200 ./build/astrocs mosaic --json run/RELEASE-01/e2e/l4/configs/p2_m42.json -y
#    核对 run/RELEASE-01/e2e/l4/p2_m42/p2_rejection.json 的
#    stats.underdetermined_pixels 是否显著下降、rejected_high 是否上升；
#    并对 p3_m42_full/vis 逐块视觉验收"无卫星线残留"（DESIGN §4.5 验收要求）。
```

**建议新增的 C++ 测试（实现分片）**：
- `phase2_synthetic_gate`：n=2 先验 σ 正例（+50σ 必剔）/ 负例（无污染零误剔）；n=3/4 percentile 正例；n=6/7 路由断言；`N-r<=4` 边界；
- `p2_rejection`：`method_map`/`method_expr` 解析（缺终止 → error、未知方法 → error、非法区间 → error、first-match-wins）；
- 集成：`p2_op_reject` 逐像素 plan，1 worker vs N worker 一致；provenance 字段完整性。

---

## 7 交付物与证据索引

| 文件 | 内容 |
|---|---|
| `工程控制/RELEASE-02/change-claims/FIX-REJ-001.md` | 变更 claim（根因、证据、映射表、判定表、表达式语法、订正前后、影响面、实现方案、请求裁决） |
| `reports/RELEASE-02/FIX-REJ-report.md` | 本报告 |
| `run/RELEASE-02/FIX-REJ/rejection_n_experiment.py` | 语义复刻 + 自证锚点 + 卫星线注入实验（可复现） |
| `run/RELEASE-02/FIX-REJ/out/rejection_n_experiment.json` | 实验原始输出（自证、注入、n=2 窗口、depth 覆盖） |
| `run/RELEASE-02/FIX-REJ/wbpp/BatchPreprocessing/` | WBPP 一手源码（前台解出，本次复核行号） |
| 仓外（未入仓） | `/dev/shm/astrocs_rej/siril-1.4.3/`（Siril 1.4.3 只读对照；GPL 未复制入仓） |

---

## 8 上呈（仅 1 项，需负责人裁决）

**n=2 档的 prior 来源选择**（互斥的产品/成本取舍，证据已穷尽）：

- **选项 A（推荐）**：用该帧该 tile 内 **31×31 邻域中位数/MAD** 作 `prior_sky/prior_sigma` —— 不需新增产品输入，纯 CPU 可向量化，代价 = 每帧每 tile 一次邻域稳健统计（需 benchmark 定档）；
- **选项 B**：接入 **Phase1 ivar/variance** 作 `prior_sigma`（`σ_i=1/√ivar_i`）—— 更准，但需把 ivar 面接入 `p2_op_reject`（当前只读 corrected 值），属**合同扩面**。

其余事项（映射表、WARN 分级、表达式语法、最小改动面）本分片已按 DESIGN §4.5 订正口径闭环，无需上呈。
