# RELEASE-02 HUB-C 报告 — SD-18 低 n 保守路径（不排异 + 加权积分）

> 执行者：低 n 保守路径实现者（HUB-C）。工作目录：/workspace/Astro CS Database。
> 时点：2026-09-18。零 git 写；**未跑 ninja/cmake/ctest**（硬要求），验证 = `g++ -fsyntax-only`。
> 主文件面：`lib/algorithms/coverage/{src/rejection.cpp,include/astro/phase2/rejection.h}`、
> `lib/infrastructure/scheduler/src/module_adapters.cpp`；测试面（前台转交后归本分片）：
> `tests/unit/p2002_unc_rej_prov_test.cpp`、`tests/unit/p2001_real_nodes_test.cpp`（仅注释）。
> 证据：`run/RELEASE-02/hub-c/logs/syntax_gate.log`（4 TU rc=0，仅 1 条既有 warning :3819）。

---

## 0 结论摘要

| 项 | 内容 | 状态 |
|---|---|---|
| ① kernel 低 n 档 | `astrocs_adaptive_pixel` 映射改为 **n<=3 none**；4..7 percentile；8..15 winsorized；>=16 linear_fit | **已闭合** |
| ① opt-in 保留 | `P2_REJECT_EXTREME_VALUE_PRIOR_SIGMA` 方法/API/内核全保留，仅移出 AUTO 路由；显式 request 仍解析（`underdetermined_n` 默认 1，n=2 可进 kernel） | **已闭合** |
| ① 冻结路由不变 | `wbpp_2_9_1`/`wbpp_current`/`astrocs_adaptive` 的 AUTO 分支逐字未动；仅新 profile 的 `astrocs_n_map_method` 改动 | **已闭合（逐字比对）** |
| ② 先验移出生产 | `p2_op_reject` 删除对 `p2_reject_local_prior` 的调用与逐样本 `prior_sigma/prior_sky` 填充；函数保留为 `[[maybe_unused]]` opt-in（无死代码警告） | **已闭合** |
| ② 保守路径 | n<=3 像素走 gate else 分支（全接受、逐样本掩码=1）→ 下游正常逆方差加权积分 | **已闭合** |
| ③ provenance | 新增 `low_n_policy="underdetermined_no_rejection"`/`low_n_max_n=3`；`stats.underdetermined_pixels` = 因 n<=3 未排异像素数；manifest 增 `reject_low_n_policy`/`reject_underdetermined_pixels` | **已闭合** |
| 测试面订正 | HUB-B 按旧口径写的 n=2 先验 kernel 重放已改为 SD-18 保守期望；部分拒绝面（2c/parity）显式选冻结 `wbpp_current` 以保留 §30.2 判别力 | **已闭合** |

**科学语义零放宽**：未改任何阈值/判据/冻结枚举；未删检查；低 n 未排异如实计入 provenance；部分拒绝的 §30.2 恒等式仍由显式冻结 profile 锚定。

---

## 1 ① kernel：`astrocs_adaptive_pixel` 低 n 档改保守

### 1.1 新映射（`rejection.cpp:1101-1119`，`astrocs_n_map_method`）

| 几何 n | 旧（HUB-A） | **新（SD-18）** |
|---|---|---|
| 0..1 | none | **none（保守）** |
| 2 | extreme_value_clip_prior_sigma | **none（保守）** |
| 3 | percentile | **none（保守）** |
| 4..7 | percentile | percentile |
| 8..15 | winsorized_sigma | winsorized_sigma（卫星线去除主力档） |
| >=16 | linear_fit | linear_fit |

### 1.2 `underdetermined_n` 默认（`rejection.cpp:1149-1162`）

- `astrocs_adaptive_pixel` AUTO → **3**（与 n<=3 none 一致：n<=3 记 UNDERDETERMINED，不冒充排异成功）；
- `astrocs_adaptive_pixel` + 显式 `request=EXTREME_VALUE_PRIOR_SIGMA`（opt-in）→ **1**（使 n=2 进 kernel；n=1 由 `minimum_n=2` 拦下）；
- 其余冻结 profile → **2（逐字不变）**。

### 1.3 opt-in 保留（未删 API）

- 枚举/语义 id/`method_minimum_n`/`method_is_explicit`/kernel 分派/`extreme_prior_valid`/`reject_extreme_prior_impl`/`P2ExtremeValuePriorSigmaParams`/`P2CandidateStack.prior_*` 全部保留；
- 仅从 `astrocs_n_map_method` 移除（`rejection.cpp:1114-1119`）；显式 request 仍走原路径；
- 头文件注释（`rejection.h:57-61`）明确标注为 **explicit opt-in，永不参与 AUTO 路由**。

### 1.4 冻结 profile 逐位不变

`p2_reject_plan_resolve` 中 `pixel_profile` 分支只作用于 `profile==astrocs_adaptive_pixel`；`else` 分支（wbpp 冻结路由：n<6 percentile / 6..15 winsorized / >15 linear_fit）与阈值/迭代/归一化默认值逐字未动。`astrocs_adaptive` 仍走同一冻结 `else` 分支。

---

## 2 ② scheduler：逐像素 31×31 先验彻底移出生产路径

文件：`lib/infrastructure/scheduler/src/module_adapters.cpp`（`p2_op_reject`）。

| 环节 | 位置 | 处置 |
|---|---|---|
| 先验调用 | 原 `:5065-5082` | **删除**：不再 `assign`/`fill` `prior_sigma/prior_sky`，`P2CandidateStack.prior_*` 保持 `nullptr` |
| 先验函数 | `:4819-4865` | **保留为 opt-in**：`[[maybe_unused]] static bool p2_reject_local_prior(...)`（无引用、无死代码警告）；注释明确"生产不再调用、供显式先验档复用" |
| 生产 request | `:4906-4907` | `req.underdetermined_n = 0`（用 profile 默认：pixel=3 / wbpp=2），不再硬编 1 |
| 保守路径 | gate `:5068-5115` | n<=3 → `plan.method=NONE`，`eligible_count<=underdetermined_n` → else 分支全接受、逐样本掩码置 1 → 下游 `p2_op_integrate` 正常逆方差加权叠加 |
| 逐像素几何 n 路由 | `:4895-4926`（保留） | support>0 计数 → `p2_reject_plan_resolve_n`，按 n 缓存 plan；未动 |
| gate / plans[] | `:5068-5070` / `:5155-5164` | 未动（gate 同式；`plans[]` 逐 n method/semantic_id/minimum_n/underdetermined_n/normalization） |
| 显式 opt-in 保护 | `:4918-4924` | 保留（`extreme_prior.center_mode=0`）；注释说明 AUTO 默认路由永不含该档 |

**先验计算是否彻底移出生产路径：是。** `lib/` 内对 `prior_sigma/prior_sky` 的赋值仅剩 kernel 内部消费与 plan 默认值；生产 `p2_op_reject` 无任何先验计算/填充/引用（除注释与 opt-in 函数定义）。逐像素几何 n 路由、gate、provenance `plans[]` 全部保留。

---

## 3 ③ provenance 与可观测性（如实报出，不伪装排异成功）

`p2_rejection.json`：

- 顶层新增 `"low_n_policy":"underdetermined_no_rejection"`、`"low_n_max_n":3`（`:5173-5174`）；
- `stats.underdetermined_pixels` = **几何 n<=3 且确有候选（eligible>0）的像素数**（`undet_low_n_pixels`，`:4969/5116-5119/5195`）；void 无候选像素不计（由 `candidates=0` 表达）；
- 原资格门欠定计数移到 `stats.underdetermined_gate_pixels`（语义不丢失，`:5198`）；
- `plans[]` 中 n<=3 档如实为 `method=0`/`semantic_id="astrocs.none.v1"`/`minimum_n=0`/`underdetermined_n=3`；`plan.fallback` 恒 `"none"`（无先验 fallback）；
- 保留 `prior_unavailable_pixels=0`（artifact schema 稳定；生产已无先验路径）。

manifest 新增：`reject_low_n_policy`、`reject_underdetermined_pixels`（`:5211-5212`）；保留 `reject_prior_unavailable_pixels=0`。

---

## 4 预期省下的时间

- HUB-A 实测口径（`hub-a-report.md` §2.2）：L4 m42 中 n=2 像素 ≈ 98M，逐像素 31×31 两次 `std::nth_element` ≈ 5-6 µs ⇒ **~1000-1200 s 单线程**。该计算整体从生产路径删除。
- 额外收益：不再逐像素分配 `prior_sigma/prior_sky` 两个 `std::vector<double>`（原每 n=2 像素一次分配）、不再读 31×31 邻域（缓存/带宽），并跳过 n<=3 像素的 kernel 调用。
- **净预期：≈1000-1200 s 单线程（L4 m42 量级）全部省下**；其余 n 档行为不变。真实数字待前台 E2E benchmark 定档。

---

## 5 测试面订正（HUB-B 旧口径 → SD-18）

> HUB-B 按 HUB-A 的「n=2 先验 σ 档」写期望；SD-18 后这些期望过时。前台明确 tests/ 转本分片，且**不得放宽判据**。

| 测试 | 位置 | 旧（HUB-B/HUB-A 口径） | 新（SD-18 正确行为） |
|---|---|---|---|
| `resolve_adaptive_pixel_plan` 助手 | `p2002:372-386` | `underdetermined_n=1` + 强制 `center_mode=0` | `underdetermined_n=0`（生产同参 → 默认 3）；删 `center_mode` 强制 |
| `local_prior_31x31` | `p2002:388-433` | 31×31 邻域中位数/MAD 独立复算 | **整段删除**（生产已无 31×31 先验） |
| `run_kernel_plan` | `p2002:435-473` | 按 `extreme_prior` 提供先验数组 | 删先验分支；`prior_*` 恒 `nullptr` |
| `test_s302_kernel_semantics` | `p2002`（新增块） | — | **新增**：锁定 n<=3 none / 4..7 pct / 8..15 winsorized / >=16 linear_fit + `underdetermined_n=3` + 显式 opt-in `extreme_prior` 仍解析（`underdetermined_n=1`） |
| `test_f_p2002_01_rejection_parity` | `p2002:680-806` | `kern_ran`/`any_rej`（depth=3 必产拒绝） | 断言 n=3 档 `method=NONE`/`underdetermined_n=3`/`semantic_id=astrocs.none.v1`；`low_n_policy`；`stats.underdetermined_pixels == 复算低 n 像素数`；`rejected_low+high==0`；删 `kern_ran/any_rej`（depth=3 全部 geom_n<=3，生产不排异） |
| `test_f_p2002_02_n_ineligible_identity` | `p2002:920-926` | 默认 profile（旧映射 n=3→percentile）产部分拒绝 | **显式 `reject_profile=wbpp_current`**：默认 SD-18 路由在 depth=3 不排异，无法产生部分拒绝；显式选冻结 profile（n=3→percentile）保留 §30.2 逐样本掩码在部分拒绝下的恒等式与 FAULT=identity 判别力 |
| `test_determinism_and_parity` depth=3 1v4 块 | `p2002:1843-1854` | 默认 profile（旧映射）产部分拒绝 parity | 同上显式 `reject_profile=wbpp_current`，保留部分拒绝下的 1/4 worker bitwise parity |
| p2001 加权均值 oracle | `p2001:477-482`（仅注释） | 注释称 n=2 进排异 | 注释改为 SD-18：depth=2 的 n=2 像素全接受；oracle 仍按 `p2_rejection_sample_mask.bin` 逐样本剔除 |

**p2001 oracle 自洽性确认（前台问项）**：`tests/unit/p2001_real_nodes_test.cpp:515-564` 的 oracle 以 `sample_mask ∧ corrected finite` 逐样本判定，`expect_w=Σ accepted ivar`、`signal=Σ w·s/Σ w`。先验拆除后 depth=2 的 n=2 像素全部 `plan.method=NONE`（`underdetermined_n=3` → gate 不跑 kernel）⇒ 掩码全 1 ⇒ `W=2.5`、`signal=两帧加权均值`，**自洽**；无覆盖/无候选像素仍走 `expect_w=0 → NaN/NaN` 分支。该 oracle **未放宽**，只是全接受时自然退化为旧 2.5。

---

## 6 转交 / 需前台注意

1. **`stage2_common.cpp` 未改（文件面外）**：`lib/algorithms/coverage/src/stage2_common.cpp:268-274` 的 `astrocs_adaptive_pixel` `underdetermined_n` 默认仍为 **1**。该文件是独立 `stage2` 工具配置解析（**非** scheduler 生产路径），新映射下 n=2/3 会跑 `NONE` 而不记 UNDERDETERMINED。建议前台决定是否同改为 **3**（保持与 kernel 默认一致）；本分片按"最小改动面/文件面"未动。
2. **测试未实跑**：受硬要求限制仅 `g++ -fsyntax-only`（4 TU rc=0）。真实行为须前台构建后跑 `p2002_unc_rej_prov`、`p2001_real_nodes` 与一次 depth≥8 E2E（验证 n≈8 winsorized 卫星线去除与 `underdetermined_pixels` 计数）。
3. **HUB-A 报告 §2 已被 SD-18 取代**：`reports/RELEASE-02/hub-a-report.md` 的 n=2 先验接线描述不再反映生产；其 §5.2 测试影响表亦以本报告 §5 为准。
4. **provenance token 变更**：`plan.fallback` 不再产生 `"prior_sigma_unavailable"`（现恒 `"none"`），低 n 语义改由 `low_n_policy` 表达。仓库内代码/测试无消费者（grep 确认），历史报告中的旧数字仅存档。
5. **`stats.underdetermined_pixels` 语义变更**：现 = 低 n（n<=3）未排异像素数；原资格门口径迁至 `stats.underdetermined_gate_pixels`。仓库内无代码消费者。
6. **未改 `docs/**`、`lib/infrastructure/cli/**`**（按硬要求）。

---

## 7 证据索引

| 证据 | 路径 |
|---|---|
| 语法门（rejection.cpp / module_adapters.cpp / p2002 / p2001，4 TU rc=0） | `run/RELEASE-02/hub-c/logs/syntax_gate.log` |
| include flags | `run/RELEASE-02/hub-c/logs/include_flags.txt` |
| 改动面 | `git status --porcelain`：`rejection.{h,cpp}`、`module_adapters.cpp`、`p2002_unc_rej_prov_test.cpp`、`p2001_real_nodes_test.cpp` |
| 权威锚 | 工程控制/RELEASE-02/ACCEPTANCE.md SD-18（收回 SD-14）；负责人 2026-09-18 原话；`reports/RELEASE-02/hub-a-report.md` §1/§2；`docs/science/REJECTION.md` §7/§8（n<=2 恒 UNDERDETERMINED） |
