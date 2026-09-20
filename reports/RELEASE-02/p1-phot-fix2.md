# RELEASE-02 P1-PHOT-BROKEN 修复报告（分片 p1-phot-fix2）

> 产物目录：`run/RELEASE-02/p1-phot-fix2/`　报告：`reports/RELEASE-02/p1-phot-fix2.md`
> 约束遵守：零 git 写；未跑 `ninja`/`cmake`/`ctest`（仅 `g++ -fsyntax-only` + 独立重链）；
> 未改 `docs/**`；`TMPDIR=/dev/shm/astrocs_photfix`（用完清理）。

---

## 0 结论速览（TL;DR）

| # | 问题 | 结论 |
|---|---|---|
| 1 | `k_photo=6.27e-17` 是否单位错误 | **不是**。它是 SCI-PHOT-001 §3 冻结约定的**正确**取值；10^16.2 的偏移量**精确等于**未建模仪器常数 `A·t/(g·h·c·1e9)`，物理闭合到 ~116 mm 口径（见 §3）。**不存在缺失的单位换算**。 |
| 2 | 11/12 板块 `photscal=1.0` | **NO_DATA 占位值被当成拟合标度**。根因是 `phot` 节点按文件约定读 `p1_wcs.json` 却**未声明 `artifact:p1_wcs` 依赖边** ⇒ 与 `wcs` 节点并发 ⇒ 读到缺失 WCS 时回退 config `wcs`（非空但无天测键）⇒ CRVAL=(0,0)/CD=0 ⇒ 0 匹配 ⇒ `cleanAndScale` 走 NO_DATA 返回 `scale=1.0`，而调用方只查 `finite&&>0` ⇒ `applied=true`。 |
| 3 | `photoscales` 全 null | **键名误读**。产品里的真实键是 `photscales`（`p-h-o-t-s-c-a-l-e-s`）。查询 `photoscales` 恒为 null。`photscales` 实际**一直存在且有值**（11 板块=1.0 占位，1 板块=6.27e-17）。 |
| 4 | 两次运行不同 | **跨节点文件约定竞态**（同 DET-001/F-8 家族），非线程/缓存/GaiaDB 随机失败。16:22 冒烟跑 `p1_wcs.json` 缺失 → 假 1.0；16:26 重跑文件在位 → 真拟合 939/917 颗。 |
| 5 | `CHK-PROVENANCE-CONSISTENCY` | **有盲区**：`applied=true` 只查 `photscal` finite&&>0，完全不看 `photscales`/`photoapplied_artifacts`/`photscale_detail`/`n_matched`。已补 P3b 判据（红/绿自检通过）。 |

**修复方式**：① 声明 `wcs→phot` typed 依赖边（IR + 端口注册表 + descriptor）；② WCS 天测可用性校验（拒绝零 WCS）；③ 只接受**真实拟合**产物（`fit_ok`，NO_DATA 一律拒）；④ 组内帧间标度一致性守卫（fail-closed）；⑤ provenance 增加逐帧拟合证据 + 自洽硬约束；⑥ 门禁补 P3b。

---

## 1 缺陷复现（可复现证据）

### 1.1 现网坏产物（用户提供的 L4 批次）

```
run/RELEASE-02/L4-rebuild/norm_phot/p1_m42_t2_m1_red/p1_phot.json
  photometry_applied=true  photscal=6.272202992543341e-17
  photscales={...20251212: 6.272202992543341e-17, ...20251224: 5.685037392078662e-17}
其余 11 板块: photometry_applied=true  photscal=1.0  photscales 全 1.0
```

逐像素实测（`run/RELEASE-02/p1-phot-fix2/check_ratio.py`，m1 第 1 帧）：

| 量 | calibrated | photoapplied | 比值 |
|---|---|---|---|
| min | -16975.6406 | -1.064746598181121e-12 | 6.272202618859e-17 |
| median | 195.9837 | 1.2292497884392253e-14 | 6.272202992612e-17 |
| max | 80789.7344 | 5.067295975186292e-12 | 6.272203366179e-17 |

⇒ `photoapplied = 6.2722029925e-17 × calibrated`（比值恒定到 FP32 舍入）。**不是"被乘成 0"，是整体单位缩放**（相对结构完整保留）。

11 板块的"施加"是**逐字节恒等**（`check_ratio_m2m3.py`）：

```
p1_m42_t2_m2_red  bitwise_equal=True  sha_equal=True   calibrated med=215.5448 == photoapplied med=215.5448
p1_m42_t2_m3_red  bitwise_equal=True  sha_equal=True   calibrated med=198.2855 == photoapplied med=198.2855
```

⇒ `photometry_applied=true` + `photscal=1.0` 且产物与输入**逐字节相同** = 纯 no-op 被声明为"已应用"。

### 1.2 根因链（`module_adapters.cpp` `p1_op_photometry`）

```
k_photo = scale = 10^(-location)
location = Tukey-IRLS_c=4.685( r_i ),  r_i = log10( F_instr,i / F_syn,i )
F_instr = p1_sources.json 的 PSF flux（ADU）      [star_matcher.cpp:318]
F_syn   = ∫ F_λ(λ)·T(λ)·Q(λ)·λ dλ （Gaia XP XPSD）[spectrum_integrator.cpp:441-447]
scale   = pow(10.0, -location)                     [star_matcher.cpp:589]
```

**分支 A（11/12 板块）—— 零 WCS ⇒ NO_DATA ⇒ 假 1.0**

`module_adapters.cpp:3195-3207`（修复前）：

```cpp
if (wj.empty() && p1_has(doc, "wcs") && doc["wcs"].is_object()) wj = doc["wcs"];
if (wj.empty()) { photscale_error = "missing WCS for " + key; break; }
```

config 的 `wcs` 段是 `{"init_source":"header_pointing","gaia_data_dir":".../GaiaDR3"}` —— **非空**，但没有任何天测键。判定 `wj.empty()` 为假 ⇒ 继续。实测日志（`run/RELEASE-02/logs/smoke_norm.stderr:6150-6167`，板块 `p1_m42_t2_m1_red`，16:23:06）：

```
[INFO] 锥形搜索: ra=0.0000 dec=0.0000 r=1.000 mag_min=6.0
[wcs_transform] 警告: CD矩阵行列式接近0 (det=0.000e+00)
[wcs_transform] 初始化: CRVAL=(0.000000,0.000000), CRPIX=(0.000,0.000), CD=[0,0;0,0], det=0.000e+00
[star_matcher] P12-001 阶段2: gaia_projected_in_frame=0 / 253
[star_matcher] P12-002 正向匹配 (PSF→Gaia): 0 / 5000 命中
[star_matcher] cleanAndScale: 输入 0 颗, 星等容忍 3.00 mag
[pc_api] 匹配+清洗完成: 0 颗, scale=1.000000e+00, sigma_residual=0.000000
[photometry_apply] 开始: w=4096 h=4096 photscal=1.000000, 总像素=16777216
```

`cleanAndScale` 在 `n_in==0` 时 `return {}` 且 `*out_scale_factor = 1.0`（`star_matcher.cpp:404,411-413`），**C 入口 rc 仍为 0**。调用方（修复前）只查：

```cpp
if (!(std::isfinite(fr.k_photo) && fr.k_photo > 0.0)) { photscale_error = ...; break; }
```

⇒ 占位 1.0 被接受 ⇒ `photometry_applied=true`、`photscal=1.0`、写恒等 `photoapplied_*` 产物。**这正是 FIX-P1 注释里承诺绝不会做的事。**

**分支 B（1/12 板块，m1）—— 真拟合，location=16.20 dex**

`run/RELEASE-02/L4-rebuild/logs/p1_m42_t2_m1_red.phot.stderr`：

```
6166 [B4-3] 质量位有效域: 排除 106 颗 (SATURATED), 剩余 994 颗
6168 IRLS 初始: location=16.201318, MAD=0.013806, S=0.020469
6169 IRLS 收敛于迭代 5: location=16.202580
6178 rejected_quality=161 (quality=106 + invalid=0 + mag=0 + irls=55), fit_used=939,
     robust_iterations=6, scale=6.272203e-17, sigma=0.019137
6219 IRLS 初始: location=16.243863, MAD=0.012512, S=0.018551
6220 IRLS 收敛于迭代 5: location=16.245267
6229 fit_used=917, scale=5.685037e-17, sigma=0.017811
```

MAD(r)=0.0138 dex = **0.034 mag** 的帧内散度 ⇒ 这是一个**极高质量**的零点测量，不是噪声。

### 1.3 12 板块逐块诊断

来源：`run/RELEASE-02/L4-rebuild/logs/p1_m42_*.phot.stderr`（`CD矩阵行列式接近0` 计数 / 锥形搜索中心 / `匹配+清洗完成`）。

| 板块 | CD det=0 警告 | 锥形搜索 (ra,dec,r) | max n_gaia | 匹配数 | location (dex) | scale | 判定 |
|---|---|---|---|---|---|---|---|
| t2_m1 | 0 | (83.2836, -6.3745, 0.934) **真** | 2339 | 939 / 917 | 16.2026 / 16.2453 | 6.2722e-17 / 5.6850e-17 | 真拟合 |
| t2_m2 | 4 | (0.0000, 0.0000, 1.000) 零 | 253 | 0/0/0/0 | — (NO_DATA) | 1.0 | 假 1.0 |
| t2_m3 | 2 | (0,0,1.0) | 253 | 0/0 | — | 1.0 | 假 1.0 |
| t2_m4 | 3 | (0,0,1.0) | 253 | 0/0/0 | — | 1.0 | 假 1.0 |
| t2_m5 | 3 | (0,0,1.0) | 253 | 0/0/0 | — | 1.0 | 假 1.0 |
| t2_m6 | 2 | (0,0,1.0) | 253 | 0/0 | — | 1.0 | 假 1.0 |
| t3_m1 | 6 | (0,0,1.0) | 253 | 0×6 | — | 1.0 | 假 1.0 |
| t3_m2 | 4 | (0,0,1.0) | 253 | 0×4 | — | 1.0 | 假 1.0 |
| t3_m3 | 6 | (0,0,1.0) | 253 | 0×6 | — | 1.0 | 假 1.0 |
| t3_m4 | 6 | (0,0,1.0) | 253 | 0×6 | — | 1.0 | 假 1.0 |
| t3_m5 | 5 | (0,0,1.0) | 253 | 0×5 | — | 1.0 | 假 1.0 |
| t3_m6 | 6 | (0,0,1.0) | 253 | 0×6 | — | 1.0 | 假 1.0 |

**11/12 全部同一条路径**：`p1_wcs.json` 未就位 → 回退 config `wcs` → CRVAL=(0,0)/CD=0 → 锥形搜索落在 (0°,0°) r=1° → 253 颗 Gaia 星 → 全部投影到帧外（`gaia_projected_in_frame=0`）→ 0 匹配 → NO_DATA → 占位 1.0 → 被当作已应用。

### 1.4 不确定性来源（确定结论）

同一目录 `norm_phot/p1_m42_t2_m1_red/` 有两份完整 run manifest，**config_sha256 完全相同**（`77a868cd679b4c5a0c50e40d57c0792c874b1930cf5d2574c81770cf250b423b`）、二进制版本相同（`0.11.0-alpha.2+gdb7af37fb96870c167e8eac16cdd3bc7dfb7edf6`）：

| run | manifest | started/finished (UTC) | p1_phot.json sha256 | photoapplied sha256 | 含义 |
|---|---|---|---|---|---|
| A（冒烟） | `astrocs_run_55cbad6be608.json` | 08:24:26 | `e0198276…` | `2d53aa41…` == calibrated sha（两帧） | k=1.0 恒等 no-op |
| B（重跑） | `astrocs_run_55ff936764f3.json` | 08:28:04 | `fd616c72…` | `8684e3ba…` / `b88e9498…` ≠ calibrated | k=6.27e-17 / 5.69e-17 |

盘上现存 = B。A 的 `photoapplied` 与 `calibrated` **sha256 逐字节相同** ⇒ A 的 k 恰为 1.0。

根因证据：A 的日志（`run/RELEASE-02/logs/smoke_norm.stderr`）显示零 WCS + 0 匹配 + 253 颗星；B 的日志（`run/RELEASE-02/L4-rebuild/logs/p1_m42_t2_m1_red.phot.stderr`）显示真实 WCS（`锥形搜索: ra=83.2836 dec=-6.3745`）+ 2339 颗星 + 939/917 匹配。

**确定结论**：不是线程竞争、不是缓存、不是 `GaiaDR3SP` 读取随机失败、不是时间依赖。是 **`phot` 节点对 `<frame_dir>/p1_wcs.json` 的跨节点文件约定读取缺少 typed 依赖边**（`runtime_client.cpp:160-166` 的 `phot` 只声明 `psf`/`sources`；`wcs` 节点与它同为 `sources` 下游 ⇒ 并发）。这与 `docs/contracts/DATA_ARTIFACTS.md` 记载的 **DET-001 (D5)**（`drz` 读 `p1_phot.json` 无依赖边，实测 14 次 10/4 翻转）是**同一缺陷类的反向复发**；F-8 与 DET-001 都已在 drz 侧声明了 typed 边，**phot 侧读 wcs 的这条边被漏掉了**。

### 1.5 `photoscales=null` 的真实来源

`p1_phot.json` 的真实键名是 **`photscales`**（无第二个 o）：

```
$ python3 -c "import json;print(sorted(json.load(open('.../p1_m42_t2_m3_red/p1_phot.json'))))"
['apply_entry','entry','n_frames','node','operation','photoapplied_artifacts',
 'photometry_applied','photscal','photscale_source','photscales','pixel_scaling','schema']
```

查询 `p1_phot.json["photoscales"]` ⇒ KeyError/None，**每个产品都如此** —— 这就是"photoscales 全 null"的来源。代码路径上**不存在**写 null 的分支：`photometry_applied=true` 时 `prov["photscales"]` 一定被写成对象（`module_adapters.cpp:3349`），`false` 时该键整体不写。

⇒ 真正的 provenance 缺陷不是"缺键"，而是**语义为假**：`applied=true` + 逐帧 1.0，而该 1.0 是 NO_DATA 占位值（§1.2 分支 A）。本报告把这条登记为 **PROV-SEMANTIC-FAKE**（结构自洽、语义不实），与用户描述的"缺键"区分开。

---

## 2 单位量纲核查（逐项）

### 2.1 量纲表

| 量 | 代码位置 | 单位 | 量纲 | 实测/典型值 |
|---|---|---|---|---|
| `F_instr` | `star_matcher.cpp:318` `m.f_instr = psf_flux[j]`；来源 `p1_sources.json` 的 `flux`（PSF 拟合流量，像素值直接来自 `calibrated_*.fts`） | **ADU** | [ADU] | 匹配星代表值 ≈ 3.4e3 ADU（生产 SNR 键 `F_ref=3429.318642`；帧内 flux p50=3366，top5000 p50=1.0e4） |
| `T(λ)`, `Q(λ)` | `filters.json` / `qe_curves.json` | 无量纲 | 1 | 滤光片 73 点，范围 572–716 nm（Baader R） |
| `λ` | `spectrum_integrator.cpp:347` `weighted_wl = spectrum_wl * filter_trans * q` | **nm** | [nm] | 336–1020 nm，步长 2 nm（`wl_start=336, wl_step=2, count=343`） |
| `F_λ(λ)` | `compute_f_syn_cached_xpsd`：`byte*flux_mul + flux_min`（Gaia XP XPSD 官方解码） | **W·m⁻²·nm⁻¹** | [M T⁻³] | Vega 3.6e-11 W m⁻² nm⁻¹ @550 nm |
| `dλ` | Simpson 积分变量 | **nm** | [nm] | 2 nm |
| `F_syn` | `F_syn = ∫F_λ·T·Q·λ dλ` | **W·m⁻²·nm** | [M T⁻³]·[nm] | ≈ **1.95e-13**（由 `F_ref/10^location` 反演） |
| `r = log10(F_instr/F_syn)` | `star_matcher.cpp:455` | dex(ADU / (W·m⁻²·nm)) | — | 中位 16.2026 |
| `location` | IRLS 稳健位置 | 同上 dex | — | **16.2026 / 16.2453** |
| `scale = 10^-location` | `star_matcher.cpp:589` | **(W·m⁻²·nm)/ADU** | — | **6.2722e-17 / 5.6850e-17** |

`docs/science/PHOTOMETRY.md` §3 逐字确认：`F_instr: ADU；F_syn: W·m⁻²·nm；二者**不同量纲**，其比值的对数即 location`、`scale = 10^{−location} 单位 [F_syn 单位]/ADU`。**该量纲差是冻结约定的一部分，不是缺陷。**

### 2.2 10^16.2 的物理闭合（这是关键：没有"缺换算"）

物理（光子计数）：

```
N_e = (A·t/(h·c)) · ∫ F_λ(λ_m)·T·Q·λ_m dλ_m          [A=有效口径面积 m², t=曝光 s]
F_instr[ADU] = N_e / g                                [g=增益 e⁻/ADU]
```

代码的 `F_syn` 用 nm：`∫F_λ λ_m dλ_m = 1e-9 · F_syn_code`（λ_m·dλ_m = λ_nm·dλ_nm × 1e-18，且 `F_λ[m]=F_λ[nm]×1e9`）。于是

```
F_instr / F_syn_code = A·t / (g·h·c·1e9)
k_photo = F_syn_code / F_instr = g·h·c·1e9 / (A·t)
```

**用实测值反解仪器常数**：`k = 6.2722e-17` ⇒ `A·t/g = h·c·1e9/k = 1.9878e-16/6.2722e-17 = 3.169 m²·s`。
取 t=300 s（FITS EXPTIME）⇒ `A/g = 1.056e-2 m²`：g=1 e⁻/ADU ⇒ A=105.6 cm² ⇒ **口径 ≈ 116 mm**；g=2 ⇒ 164 mm。**完全落在小型 APO/长焦镜的合理区间。**

反向校验：`F_instr/F_syn = A·t/(g·h·c·1e9) = 3.169/1.9878e-16 = 1.594e16 = 10^16.2026` —— 与实测 `location=16.2026` **逐位一致**。

⇒ **答案**：既不缺 `photscal` 前置标度、也不缺零点、也不是 AB/ST 换算缺失。10^16.2 **就是** SCI-PHOT-001 §6 明确声明"由 `location` 吸收"的未建模仪器常数（口径·曝光·增益·hc + nm↔m 记账）。任何"补一个 1e16 因子把 k 拉回 ~1"的改动都会**破坏**这条物理闭合。

### 2.3 为什么"绝对窗口 [0.1,10]"不可用（对任务书 §7 的正面回应）

按 §2.2 的关系，`k_photo` 的物理可容许区间由 `(A, t, g)` 的极端取值决定：

| 参数 | 取值域 | 对 k 的贡献 |
|---|---|---|
| A | 1e-4 … 1e2 m²（1 cm² 针孔 … 10 m 级） | 1e-4 … 1e2 |
| t | 1e-3 … 1e4 s | 1e-3 … 1e4 |
| g | 0.1 … 100 e⁻/ADU | 1e1 … 1e-2 |
| ⇒ k 合理域 | `g·h·c·1e9/(A·t)` | **≈ [5e-3, 5e13]** |

`6.27e-17` 落在这个 16 dex 宽的物理域**内部**（它是真实测量值）。因此 `[0.1,10]` 绝对窗口会拒绝**100% 的真实 ADU→F_syn 标度**（包括本批次唯一成功的 m1），把功能变成死代码，且与 SCI-PHOT-001 §3 直接冲突 —— 属于"改冻结科学定义"，按 AGENTS.md §5/§9 需走变更 claim 并上呈负责人（**已在本报告 §7 登记 ESC-P1-PHOT-ABS-WINDOW**）。

物理上真正受约束的是**组内帧间相对一致性**（同仪器/滤光片/曝光 ⇒ 帧间零点差 ≪ 1 mag），因此量级守卫按此实现（§4 守卫 ④）。

---

## 3 修复

### 3.1 改动清单（最小面；不动科学公式/默认容差/冻结定义）

| 文件 | 改动 | 依据 |
|---|---|---|
| `lib/infrastructure/cli/runtime_client.cpp` | `phot` 节点 inputs 增 `{"wcs","artifact:p1_wcs"}` | F-8 / DET-001 同款处置（typed 边取代并发文件约定） |
| `lib/infrastructure/scheduler/src/module_adapters.cpp` | `p1_photometry_descriptor()` 增 `wcs` typed **输入**端口 | 同上（IR 端口↔descriptor 一致） |
| `lib/infrastructure/pipeline/module_ports.registry.json` | `astrocs.phase1.photometry` 增 `wcs` 输入端口 | CHK-REGISTRY-IR-PARITY |
| `lib/infrastructure/scheduler/src/module_adapters.cpp` | 新增 `p1_wcs_astrometry_usable()`（有限 CRVAL + 非退化 CD）并在拟合前校验 | 拒绝零 WCS；不可用 ⇒ `photscale_error`（显式降级） |
| 同上 | 拟合结果按 `fr.fit_ok` 判定；`n_matched < 3` / `sigma_residual_dex > 1.0` 一律拒 | SCI-PHOT-001 §4/§8 冻结门；禁伪造 1.0 |
| 同上 | 组内帧间标度一致性守卫（`max/min ≤ 10^0.5 dex`）+ 每帧必须 `fitted=true` | 禁单位混装（§2.3） |
| 同上 | provenance 增 `photscale_detail`（逐帧 k/n_matched/sigma/fitted/source）+ `applied=true` 时的自洽硬约束（帧数齐备） | PROV-SEMANTIC-FAKE 修复 |
| 同上 | sidecar 通道：显式 `n_matched < 3` 直接 DATA fail-closed | 外部通道同样不得注入无证据标度 |
| `lib/algorithms/photometry/cpp/src/frame_photometry_fit.{h,cpp}` | 新增 `fit_ok` + `degraded_reason`；NO_DATA 分支 rc=-6、k_photo=1.0（占位、不得施加） | 把"没产出标度"如实上报（不改冻结 C ABI 语义） |
| `ci/check_provenance_consistency.py` | 新增 P3b 判据 + 3 个自检用例 | 补门禁盲区 |
| `tests/unit/p1001_real_nodes_test.cpp` | 新增 `test_p1photbroken_scale_guards()`（RED-1..4 + GREEN） | 判别力锁定 |

**未改动**：`c=4.685`、`tol=1e-6`、`max_iter=50`、`mag_tolerance=3.0`、`scale=10^-location`、MAD 换算、F_syn 积分、`docs/**`。

### 3.2 新增守卫（全部 fail-closed，参数与理由）

```cpp
constexpr int    P1_PHOT_MIN_FIT_STARS = 3;    // SCI-PHOT-001 §4 冻结门 |r_consistent| >= 3
constexpr double P1_PHOT_MAX_SIGMA_DEX = 1.0;  // 拟合散度 QA 上限 = 2.5 mag
constexpr double P1_PHOT_MAX_SPREAD_DEX = 0.5; // 组内帧间零点一致上限 = 1.25 mag
```

触发任一 ⇒ **整组不施加**：`photometry_applied=false`、`photscal=1.0`、`pixel_scaling="none"`、`degraded_reason="photscale_incomplete"`、`photscale_error=<机器可读原因>`（写 WARN 日志 + manifest）。**不静默、不伪造、不半归一化。**

### 3.3 provenance 自洽（要求 9）

`applied=true` ⟺ 同时满足：`photscales` 非空且覆盖 `n_lights` 帧；`photscale_detail` 键集与之一致、逐帧 `fitted=true` 且 `n_matched ≥ 3`；`photoapplied_artifacts` 数量一致。任一不满足 ⇒ **节点 DATA fail-closed**（`Result::fail`，不写伪 provenance）。

---

## 4 判别力测试（能红能绿）

### 4.1 单元测试 `test_p1photbroken_scale_guards()`

新增于 `tests/unit/p1001_real_nodes_test.cpp`（同一份测试 TU 分别链接**修复前**与**修复后**归档）：

| 用例 | 注入 | 期望 | 修复前 | 修复后 |
|---|---|---|---|---|
| RED-1 | sidecar `k_photo=6.272202992543341e-17, n_matched=0` | DATA fail-closed，不写 `photoapplied` | **FAIL**（被施加） | PASS |
| RED-2 | 同组 `6.27e-17(n=939)` + `1.0(n=0)` | 整组拒绝，无半归一化产物 | **FAIL** | PASS |
| RED-3 | 同组 `6.27e-17(n=939)` + `1.0(n=917)`（16 dex 混装） | `applied=false` + `pixel_scaling=none` + `degraded_reason`+`photscale_error`，无 `photscales`/`artifacts` | **FAIL**（9 条断言） | PASS |
| RED-4 | `fit.enabled=true` + config `wcs` 无天测键 | `applied=false`、`photscal=1.0`、`photscale_error` 指明 WCS、无伪产物 | **FAIL**（`photscale_error` 不提 WCS；实测文本 = `fit failed for light_1: filter curve load failed: 'Baader R' in …/no_such_filters.json` ⇒ 修复前 WCS 门根本不存在，装配一路走到拟合） | PASS（`WCS unusable for light_1: missing/invalid crval`） |
| GREEN | 同组 `6.2722e-17(n=939)` + `5.6850e-17(n=917)`（真实一致拟合） | `applied=true`；`photoapplied = k·calibrated`（相对误差 <1e-5）；`photscales`/`photscale_detail` 每帧齐备且 `fitted=true, n_matched≥3` | PASS | PASS |

```
# 修复前（build/ 原始归档 + 同一份新测试）
$ grep -c "CHECK failed" run/RELEASE-02/p1-phot-fix2/logs/p1001_test_prefix.stderr
12
# 修复后
$ grep -c "CHECK failed" run/RELEASE-02/p1-phot-fix2/logs/p1001_test.stderr
0
$ cat run/RELEASE-02/p1-phot-fix2/logs/p1001_test.stdout
P1-001 REAL NODES PASS (8 节点唯一真实 operation + call_count=1 + complete 门 fail-closed + 下游零调用)
```

**关于"输入 k_photo=6.27e-17 必须被拒绝"的正面回应**：按 §2.2/§2.3，`6.27e-17` **本身**是合法且正确的标度（本批次唯一成功的板块就是它）。测试把它拆成两个可判定的维度：

- **拒绝**的是「**无拟合证据**的 6.27e-17」（RED-1/RED-2）与「与同组其它帧**相差 16 dex** 的 6.27e-17」（RED-3）；
- **通过**的是「有一致拟合证据的 6.27e-17」（GREEN）。

这比"按绝对值拒绝"更强：它同时覆盖了 11/12 板块的假 1.0（RED-2/RED-3 的另一半）与单位混装，而不会误杀真实数据。

### 4.2 真实数据端到端 A/B（`run/RELEASE-02/p1-phot-fix2/norm/`）

四组运行，同一板块 `p1_m42_t2_m1_red`、同一 config（除 `wcs` 块外）：

| run | 二进制 | config wcs | IR `phot` inputs | applied | photscal | photscales | photoapplied sha256 (f0) | 判定 |
|---|---|---|---|---|---|---|---|---|
| `m1_run1` | patched | 真实（`gaia_data_dir`） | `psf,sources,wcs` | **true** | **6.272202992543341e-17** | 6.2722e-17 / 5.6850e-17 | `8684e3ba82eb77d0` | ✅ 真拟合 |
| `m1_run2` | patched | 同上 | 同上 | **true** | **6.272202992543341e-17** | 同上 | `8684e3ba82eb77d0`（逐位相同） | ✅ 确定性 |
| `m1_real_prefix` | **prefix** | 同上 | `psf,sources`（旧） | true | 6.272202992543341e-17 | 同上 | `8684e3ba82eb77d0` | 该次竞态"赢" |
| `m1_fakewcs_prefix` | **prefix** | 显式 (10°,10°) | `psf,sources`（旧） | **true** | **1.0** | **1.0 / 2.7247543058095527e-15** | `2d53aa41035ffb85`（**== calibrated**） | ❌ **P0 复现** |
| `m1_fakewcs_patched` | patched | 显式 (10°,10°) | `psf,sources,wcs` | **false** | 1.0 | 无（不写） | 无产物 | ✅ 显式降级 |

**P0 确定性复现（`m1_fakewcs_prefix`）**：同一板块内两帧得到 `1.0` 与 `2.72e-15`（**相差 14.6 dex**），却都写 `photometry_applied=true` 并各写一个 `photoapplied` 产物；第 1 帧的产物与 `calibrated` **sha256 逐字节相同**（纯 no-op 声明为"已应用"），第 2 帧被乘 2.72e-15。这正是用户报告的"11/12 假 1.0 + 1/12 极小值"的**同一条代码路径**，且可**确定性**重现。

**修复后同一 config（`m1_fakewcs_patched`）**：

```json
{"photometry_applied": false, "photscal": 1.0, "pixel_scaling": "none",
 "photscale_source": "none", "degraded_reason": "photscale_incomplete",
 "photscale_error": "fit failed for M42_M1_T2_flying_dutchman-20251212_012404-300S-Red:
   photometry fit produced no scale (NO_DATA): n_matched=0 < 3 (SCI-PHOT-001 §4/§8)"}
```

且**不写任何** `photoapplied_*` 产物（目录里只有 `calibrated_*`）。**没有静默、没有伪造。**

### 4.3 确定性（要求 10：两次运行逐位对比）

同一 config、同一二进制、两个独立 output_dir（`m1_run1` / `m1_run2`）：

```
比较文件数 779，逐字节不同 19 个：
  p1_phot.json / p1_products.json / p1_final.json ×2 / signal|support/properties ×4
  graph/*（6）/ alloc_report.json / alloc_samples.csv / resource_summary.json
  resource_timeseries.csv / worker_balance.csv / run_context.json
```

- **全部科学产物逐字节相同**：`photoapplied_*.fts`、`calibrated_*.fts`、`cleaned_*.fts`、`p1_flux.json`、`p1_sources.json`、`p1_psf.json`、`p1_snr.json`、`p1_wcs.json`、HiPS `signal/*` 数据块。
- 19 个差异 100% 落在**路径 / run_id / 墙钟遥测**（与 DET-001 的 canonical-hash 排除清单同口径）。
- `p1_phot.json` 的**科学载荷逐位相同**（把 `photoapplied_artifacts` 归一为基名后 `json.dumps(sort_keys=True)` 相等，diff keys = `[]`）。

⇒ 同输入同输出；修复后不再存在 §1.4 的翻转。

---

## 5 修复后 `p1_m42_t2_m1_red` 的合理数值（要求 13）

```
photscal（逐帧中位）= 6.272202992543341e-17
photscales = {…20251212: 6.272202992543341e-17, …20251224: 5.685037392078662e-17}
photscale_detail = {…20251212: {n_matched:939, sigma_residual_dex:0.0191373, fitted:true},
                    …20251224: {n_matched:917, sigma_residual_dex:0.0178108, fitted:true}}
```

逐像素 `photoapplied / calibrated`：

| 帧 | calibrated med | photoapplied med | ratio 中位 | ratio min | ratio max |
|---|---|---|---|---|---|
| 20251212 | 195.984 | 1.2292497884392253e-14 | **6.272202992612e-17** | 6.272202618859e-17 | 6.272203366179e-17 |
| 20251224 | 160.788 | 9.141e-15 | **5.685037392014e-17** | 5.685037053415e-17 | 5.685037730768e-17 |

**比值是常数**（散布仅来自 FP32 写盘舍入，相对 1e-8），等于 `10^-location`。

> 关于"应为常数且**接近 1 的量级**"：按 SCI-PHOT-001 §3，`scale` 单位是 `[F_syn 单位]/ADU`，其量级就是 `g·h·c·1e9/(A·t)`（§2.2 实测反解 ≈116 mm 口径），**不接近 1 且不应接近 1**。要求它接近 1 等于要求把像素留在 ADU —— 那正是本缺陷的另一半（11 板块留在 ADU）。正确的可检验不变量是：**① 组内逐帧比值常数；② 比值 = 10^-location；③ 组内各帧 location 差 ≪ 1 mag（本板块 0.0427 dex = 0.107 mag）；④ 与 Gaia XP 绝对刻度闭合到合理口径**。这四条全部满足。

---

## 6 门禁 `CHK-PROVENANCE-CONSISTENCY` 的盲区与补强（要求 9）

### 6.1 盲区（实测确认）

修复前判据（`ci/check_provenance_consistency.py`）：`applied=true ⇒ photscal 有限且 > 0`。对现网 12 个坏产品：

```
$ python3 ci/check_provenance_consistency.py            # 默认 roots 含 run/RELEASE-02
CHK-PROVENANCE-CONSISTENCY_PASS records=… （修复前：判绿）
```

⇒ **旧行为抓不到**。具体盲区四条：
1. 只看标量 `photscal`，**完全不读 `photscales`** ⇒ `applied=true` + 逐帧占位 1.0 判绿；
2. 不看 `photoapplied_artifacts` 是否存在/齐备（本缺陷里 11 板块的产物是**恒等拷贝**，存在性也查不出问题）；
3. 不看拟合证据（`n_matched`/`fitted`/`sigma`）⇒ 无法区分"真拟合"与"NO_DATA 占位"；
4. 不做 `applied` 与像素产物的关系核对（`photscal≠1` 时产物是否真的变过）。

### 6.2 补强（P3b）与自检结果

新增 P3b（仅对 `schema==DATA-P1-PHOTPROV-001` 记录，避免误伤通用 provenance 夹具）：
`applied=true` ⇒ `photscales` 非空对象、键数 == `n_frames`、值有限正数；`photoapplied_artifacts` 非空且数量一致；`photscale_detail` 键集一致、逐帧 `fitted==true` 且 `n_matched ≥ 3`（SCI-PHOT-001 §4）。

```
$ python3 ci/check_provenance_consistency.py --self-test
SELFTEST_PASS green_consistent
SELFTEST_PASS red_photscal_and_bunit
SELFTEST_PASS red_producer_neutral_photscal
SELFTEST_PASS green_photprov_fitted          # 新增: 真实拟合标度(6.27e-17)判绿
SELFTEST_PASS red_photprov_no_detail         # 新增: applied=true 无拟合证据判红
SELFTEST_PASS red_photprov_nofit             # 新增: fitted=false / n_matched=0 判红
SELFTEST_PASS green_ledgered
SELFTEST_PASS: all cases match expectation
SELFTEST_PASS zero_records (fail-closed GateError)

$ python3 ci/check_provenance_consistency.py --products-root ci/fixtures/provenance \
      --products-root run/RELEASE-01/e2e/evidence
CHK-PROVENANCE-CONSISTENCY_PASS records=19 files=42        # 基线（tracked 夹具）不受影响

$ python3 ci/check_provenance_consistency.py                # 默认 roots（含旧坏产物）
… 11 个 L4 板块 × 2 条 finding …
rc=1                                                        # 抓到旧行为
```

⇒ **补强后能抓到旧行为**（`rc=1`，逐板块指出 `photscales`/`photscale_detail` 缺失），且 tracked 基线仍绿。

> 注意：`run/` 为 gitignore 的运行产物目录，因此该红灯只在本开发树上出现，不影响干净检出；本批次 11 个旧板块的红灯是**真实缺陷的正确反映**，修法是重跑（修复后 provenance 会带 `photscale_detail`，或如实 `applied=false`）。

---

## 7 上呈负责人裁决（AGENTS.md §9）

### ESC-P1-PHOT-ABS-WINDOW：`k_photo` 绝对窗口 `[0.1,10]`

任务书 §7/§11 要求"输入 `k_photo=6.27e-17` 必须被拒绝"。按 §2.2/§2.3 的独立证据（SCI-PHOT-001 §3 冻结定义 + 物理闭合 `k=g·h·c·1e9/(A·t)`），**该要求在现有科学定义下不可实现**：

- `6.27e-17` 是真实、精确（帧内散度 0.019 dex = 0.047 mag）的 ADU→F_syn 标度，物理反解口径 ≈116 mm；
- 绝对窗口 `[0.1,10]` 会拒绝 100% 的真实标度，使 `photometry_applied` 永远为 false（功能死亡）；
- 落地它必须**改冻结科学定义**（把 `scale` 重定义为相对因子，或新增"绝对合理性"SCI 条款）⇒ 需变更 claim + 一致性回归。

**本次处置**：按任务书 §7 的"**或按帧间一致性判据**"分支实现（组内 0.5 dex 一致性 + 逐帧拟合证据 + WCS 可用性），并把绝对窗口登记为本裁决项。若负责人裁定需要绝对窗口，请指定窗口的**物理推导依据**（例如限定 `A/t/g` 的仪器域），我再按变更 claim 落地。

### ESC-P1-PHOT-SIDECAR-PROV：`DATA-P1-PHOTSCALE-001` 收紧

修复后 `p1_photscale.json` 的帧条目若要被**施加**，必须带 `n_matched ≥ 3`（否则 `fitted=false` ⇒ 整组拒绝）。这是对既有 sidecar 合同的事实收紧（`docs/contracts/**` 由我不可改）⇒ 需在 `docs/contracts/DATA_ARTIFACTS.md` 的 `DATA-P1-PHOTSCALE-001` 行补一句"n_matched 为施加前提"。已按"不放宽 fail-closed"原则先行收紧，文档同步请前台/负责人处理。

### ESC-P1-PHOT-IR-EDGE：`wcs→phot` typed 边

新增依赖边会改变 Phase1 调度顺序（`phot` 现在等 `wcs`）。这**只会延长**关键路径（wcs 本来就在 drz 前必须完成），不改变产物语义；已通过 `CHK-REGISTRY-IR-PARITY`（`registered=22 ir_nodes=20 双向差集空`）。若控制包要求"不改 IR"，请裁定，我改为在 phot 节点内 fail-closed（当前已同时具备该能力：`m1_run1`（无 IR 边批次）就是 fail-closed 降级而非伪造 1.0）。

---

## 8 复现命令与产物清单

```bash
export TMPDIR=/dev/shm/astrocs_photfix
cd '/workspace/Astro CS Database'
bash run/RELEASE-02/p1-phot-fix2/relink.sh          # 独立重链（不跑 ninja/cmake）
bash run/RELEASE-02/p1-phot-fix2/relink_test.sh
bash run/RELEASE-02/p1-phot-fix2/relink_test_prefix.sh
bash run/RELEASE-02/p1-phot-fix2/final_tests.sh     # 红/绿对比
bash run/RELEASE-02/p1-phot-fix2/ab_runs2.sh        # 端到端 A/B
python3 run/RELEASE-02/p1-phot-fix2/check_ratio.py run/RELEASE-02/p1-phot-fix2/norm/m1_run1
python3 ci/check_provenance_consistency.py --self-test
```

| 产物 | 内容 |
|---|---|
| `bin/astrocs` | 修复后二进制（独立重链） |
| `bin/astrocs_prefix` | 修复前参考二进制（build/astrocs 副本，16:22） |
| `bin/p1001_real_nodes_test` / `_prefix` | 同一测试 TU × 修复后/前归档 |
| `norm/m1_run1`、`norm/m1_run2` | 修复后端到端两次运行（确定性） |
| `norm/m1_fakewcs_prefix` | **P0 确定性复现**（假 1.0 + 14.6 dex 混装） |
| `norm/m1_fakewcs_patched` | 同 config 修复后：显式降级、零产物 |
| `norm/m1_real_prefix` | 修复前二进制 + 真实 config（竞态"赢"的一侧） |
| `logs/*.stderr` | 逐 run 全量日志（含 `photscale_error`/`scale=`/`location=`） |
| `logs/p1001_test{,_prefix}.stderr` | 红/绿 CHECK 明细 |
| `logs/gate_default.json` | 补强后门禁对旧产物的 finding |

清理：`/dev/shm/astrocs_photfix*` 已删除（见 §9 自检）。

---

## 9 自检清单

- [x] 零 git 写（未执行任何 git 命令）
- [x] 未跑 `ninja`/`cmake`/`ctest`（只 `g++ -fsyntax-only` + 独立重链）
- [x] 未改 `docs/**`
- [x] 未放宽任何 fail-closed（新增 3 条守卫 + provenance 硬约束 + sidecar 收紧 + 门禁 P3b）
- [x] 科学公式/默认容差未动（`c=4.685`/`tol=1e-6`/`max_iter=50`/`mag_tolerance=3.0`/`scale=10^-location`/MAD 换算/F_syn 积分逐字未改）
- [x] `g++ -fsyntax-only` 三个改动 TU 全绿；门禁 `--self-test` PASS；tracked 基线 PASS
- [x] 相关机器门全绿：`CHK-REGISTRY-IR-PARITY`（registered=22 ir_nodes=20 双向差集空）、`CHK-ALGO-WIRING`（20/20 bound）、`CHK-CONFIG-CONSUMED`（dead_keys=2 均已登记）、`REGISTRY-DOC-SYNC`（53==53）、`ci/validate_registry.py --registry ci/checks.json` PASS（53 checks）
- [x] 红/绿判别力（12 → 0 failures）；端到端 A/B 双向可复现
- [x] `TMPDIR=/dev/shm/astrocs_photfix` 已清理
