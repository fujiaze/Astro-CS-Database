# RELEASE-02 UNC-PROP 审计：归一化不确定度传播

> 任务：审计 Phase2 逐帧归一化是否**正确传播方差**，判据为「高 SNR 帧归一化后等效权重
> **不得**被降低」；给出正确公式、最小改动面与权重链接口。
> 性质：**只审计 + 给方案，不改任何生产代码/文档**；零 git 写；未跑 `ninja`/`cmake`/`ctest`。
> 证据目录：`run/RELEASE-02/unc-prop/`（`audit_evidence.py` + `code_facts_and_realdata.log` +
> `unc_prop_oracle.cpp` + `oracle_output.txt`）。
> 执行者：UNC-PROP（SubAgent）。

---

## 0. 结论摘要（先给答案）

| 问题 | 结论 | 证据 |
|---|---|---|
| 生产归一化是否传播方差？ | **否**。`p2_op_upm_apply` 只写 `corrected` 值，无任何 variance/ivar/sigma 输出 | `p2_corrected.json` 键集（§1.1） |
| 参考路径（`/g_k`）是否传播方差？ | **否**。`out_v[i] /= gain` 只作用于值，权重取自与 gain 无关的 `local_ivar_map`/`frame_snr` | `stage2.cpp:1089/1460` + `:1017/1025/1300` |
| 加性 `C_k`/`δ_k` 是否加参数不确定度 `J_out C_theta J_out^T`？ | **否**。生产建的是 W2 模型（无协方差 API）；全仓 `C_theta` 只在 v6 诊断路径被调用，且作用于**模型预测**而非归一化像素 | `module_adapters.cpp:4543`；`phase2_integrate.cpp:1649/1674-1687` |
| 归一化后方差流向何处？ | **从未计算**；权重面直接沿用**归一化前**的帧（Phase1 ivar / 帧级 SNR）。输出产品只有 signal+support，`uncertainty_available=false` | `module_adapters.cpp:5651/6023/6040`；`p2_final.json` |
| 高 SNR 帧权重是否被压低？ | **参考路径：是**（`g_k>1` 的高响应/高归一化 SNR 帧被压低至 `1/g_k²`；well-conditioned 下 `g=2` 时压到 **0.331×**，权重排序**翻转**）。**生产路径：当前无权重可比**（`weight_mode=1` 等权；`weight_mode=2` fail-closed），但缺项方向是**高估**帧权 | §2 oracle + §3 生产实测 |

**一句话**：归一化**完全没有传播方差**——既没有 `/g_k²`，也没有 `J_out C_theta J_out^T`；
下游权重面是归一化前的旧面。参考路径上这直接把 `g_k>1` 的高信噪比帧无声降权并可翻转权重排序。
生产 L4 目前用等权（`weight_mode=1`）且无方差产品，`weight_mode=2` 因数据面缺 ivar / F_ref 非公共而 fail-closed，
所以「高 SNR 帧被降权」在生产**暂时**被 fail-closed 挡住了，但一旦接通 ivar 或放宽 F_ref 门就会暴露。

---

## 1. 现状审计

### 1.1 生产归一化路径：`p2_op_upm_apply`（`lib/infrastructure/scheduler/src/module_adapters.cpp:4731-5037`）

- 逐 tile 逐 valid 像素：`p2_upm_calibrate_block(model, fid, leaves, in_v, out_v, n_valid)`
  （`:4936-4938`）→ 只得到 `out_v`（值）。
- 再扣 `δ_k`：`p2_sky_plane_eval_delta_block`（`:4947`）→
  `out_v[k] -= dvals[k]`（`:4954`）。**只改值。**
- 写出 `p2_corrected_f<fid>.bin`（fp64 每 leaf 一个 double，NaN=无效，`:4975-4976`）。
- `p2_corrected.json` 键集实测：
  `['entry','frames','model_hash','n_pixels_total','schema','sky_plane_applied','sky_plane_artifact','sky_plane_mode','tile_leaf_span']`
  —— **没有 variance / ivar / sigma / covariance 任何字段**。
- `p2_upm_calibrate_block` 签名（`upm.h:129-135`）只有 `output_signal`，**没有方差出参**；
  W2 模型也没有 `p2_upm_*_param_cov` 类 API（`p2_upm_ma_param_cov` 只属于 MA 模型）。
- 生产 `p2_op_upm_fit`（`:4466-4614`）只调 `p2_upm_build_geo`（`:4543`）建 **W2 模型**，
  从不建 MA 模型 ⇒ 生产链上**没有 `g_k`、也没有 `C_theta` 数据面**（与 FIX-GK 报告 §6 一致）。

**结论**：生产归一化路径 `corrected = (raw − C_k) − δ_k` **没有计算任何方差**。

### 1.2 参考路径：`lib/algorithms/coverage/tools/stage2.cpp`（FIX-GK 口径）

两处逐像素归一化：

| 位置 | 代码 | 值 | 方差 |
|---|---|---|---|
| ACR 路径 `:1066-1091` | `p2_upm_calibrate_block` → 扣 `δ_k`(`:1087`) → `out_v[i] /= gain`(`:1089`) | `(raw − C_k − δ_k)/g_k` | **无** |
| CPU 路径 `:1439-1462` | 同上（`:1458`/`:1460`） | `(raw − C_k − δ_k)/g_k` | **无** |

权重面（与 `gain` **无关**，即归一化前的帧）：

- ACR：`local_ivar_map`(`:1017`)、`local_snr_map`(`:1025`)、帧级 `frame_snr`(`:1010`)。
- CPU：`local_ivar_map`/`local_snr_map`(`:1294`)、`frame_snr`(`:1300`)；并行体同构（`:1551/1587/1593`）。

`local_ivar_map` 来自 `obs[].ivar`（`:411-419`）即 Phase1 控制估计器 ivar；
`frame_snr` 来自 Phase1 SNR catalogue 中位数（`:74-95`）。二者都是**归一化前**量。

`frame_gain` 默认开启（`stage2_common.h:64 frame_gain_enabled = true`），由 v6 MA 求解器估计
（`:518-535`，DC 比交叉校验 `:536-566`），在 `:567` `p2_upm_ma_close` 前**没有调用
`p2_upm_ma_param_cov`**（全文件 0 处 `param_cov`/`c_out`）。

**结论**：参考路径对 `/g_k` 只做**值缩放**，方差既没除 `g_k²`，也没加参数协方差。

### 1.3 方差/权重的流向（生产节点链）

1. `upm-apply` → `p2_corrected*.bin`：**无方差**。
2. `reject`（`p2_op_reject`, `:5113-5590`）：入口
   `p2_reject_plan_resolve_n / p2_collect_candidate_stack / p2_reject_stack_ex`（`:5394`），
   阈值用**样本栈自估的 median+MAD**（`rejection.cpp:1396 reject_robust_mad_impl`、
   `scratch_mad` `1.4826×MAD`）。**不使用**预测残差方差
   `sigma_eff² = sigma_phase1² + J C_theta J^T`（该入口 `p2_reject_classify` 在生产
   `module_adapters.cpp` 中出现 **0 次**）。
   ⇒ 与 `docs/plugins/algorithms_phase2/12_rejection.md:15/22` 的
   「预测残差方差（含 Phase1 噪声 + UPM 参数不确定度）」要求**不符**。
3. `integrate`（`p2_op_integrate`, `:5591-6199`）：
   - `weight_mode=2`：`w = Phase1 ivar`（`aio_hips_open(..., AIO_HIPS_RD_IVAR)` `:5651`；
     取用 `:6040`），ivar 缺失时 `w = 帧级 SNR²/F_ref²`（`:6023`）。
   - 两条来源都与 `corrected` **无关**，没有 `/g²`、没有参数项。
   - 输出 `ivar_mosaic = Σ w`（`:6079-6098`），`variance = 1/W`（writer 归约，
     `:6197-6199`）⇒ 输出方差也带着同一个错误权重。
4. 输出产品 `p2_final.json`：`products=['signal','support']`、
   `uncertainty_available=False`。
5. 生产 L4 配置 `mosaic_upmfix.json`：`weight_mode=1`（等权）；`p2_integrated.json`：
   `weight_basis='unit_weight_mode1'`、`weight_source='none'`、`uncertainty_available=False`。

**结论**：归一化后的方差**从未被计算**，中间被「归一化前的权重面」整体替换。

### 1.4 唯一存在的 `J C_theta J^T` 实现不在生产面上

`lib/algorithms/integration/v6/src/phase2_integrate.cpp:1645-1688`（v6 诊断/自检路由）：

- `:1649` 调 `p2_upm_ma_param_cov` 取 `C_theta`；
- `:1677-1682` 取 `J = [s_ref (g_k 槽), 1 (b_k 槽)]`，**只有一个代表帧 k=1、一个标量**；
- `:1687` `sigma_eff² = sigma_p1² + J C_theta J^T`。

它作用在**模型预测** `g_k s(p) + b_k`（`:1713`）上，**不是**归一化像素
`(raw − C_k − δ_k)/g_k`，也**没有 `1/g_k²`**，也没有 W2 `C_k` 场的协方差。
该结果只喂给 `p2_reject_classify`（`:1739`），不进入生产节点链。

---

## 2. 高 SNR vs 低 SNR 判据的数值结果

### 2.1 判据定义

对归一化样本，正确的等效权重必须是**归一化方差**的单调递减函数：
`w_k = 1 / Var(corrected_k)`。于是「高 SNR 帧（归一化方差小）的权重 ≥ 低 SNR 帧」是
**恒等式**，前提是权重由归一化方差算出。缺陷即「权重用了别的量」。

### 2.2 参考路径乘性项（`/g_k`）：高响应帧被无声降权，排序可翻转

独立 Oracle：`run/RELEASE-02/unc-prop/unc_prop_oracle.cpp`（链接 `build/libastrocs_phase2.a`，
自包含合成；调 `p2_upm_ma_build` + `p2_upm_ma_param_cov`；未跑 ninja/cmake/ctest）。
2 帧（帧 0 gauge 参考 g=1，帧 1 非参考 g>1），多 control，`sigma_raw` 可配。
`Var_correct = sigma_raw²/g² + J_out C_theta J_out^T`；`Var_used = sigma_raw²`（代码口径）。
输出见 `oracle_output.txt`：

| 场景 | `sigma_raw(f0,f1)` | `g1` | `w0` | `w1_correct` | `w1_used` | `used/correct` | 纯 `1/g²` | 排序翻转 |
|---|---|---|---|---|---|---|---|---|
| A well-cond. P=40 | 2.0 / 2.6 | 2.00 | 0.250 | **0.4468** | **0.1479** | **0.331** | 0.250 | **YES**（correct 1.79>1，used 0.59<1） |
| B medium P=12 | 2.0 / 2.6 | 2.00 | 0.250 | 0.2969 | 0.1479 | 0.498 | 0.250 | **YES** |
| C ill-cond. P=4 | 2.0 / 2.6 | 2.00 | 0.250 | 0.1763 | 0.1479 | 0.839 | 0.250 | no（参数项占主导） |
| D mild g=1.25 P=40 | 2.0 / 2.2 | 1.25 | 0.250 | 0.2645 | 0.2066 | 0.781 | 0.640 | **YES** |

`g_k` 抑制扫描（P=40，`sigma0=2.0, sigma1=2.6`）：

| `g1` | `w1_correct` | `w1_used` | `used/correct` | 纯 `1/g²` | 排序翻转 |
|---|---|---|---|---|---|
| 1.10 | 0.1536 | 0.1479 | 0.963 | 0.826 | no |
| 1.25 | 0.1950 | 0.1479 | 0.759 | 0.640 | no |
| 1.50 | 0.2718 | 0.1479 | 0.544 | 0.444 | **YES** |
| 2.00 | 0.4468 | 0.1479 | 0.331 | 0.250 | **YES** |
| 3.00 | 0.8272 | 0.1479 | 0.179 | 0.111 | **YES** |

**量化**：代码给帧 1 的权重是 `1/sigma_raw²`；正确权重是 `1/(sigma_raw²/g² + param)`。
当参数项次主导时（well-conditioned），抑制比趋于 **`1/g_k²`**（g=2 时 4×；g=3 时 9×），
且排序在 `g≥1.5` 时**翻转**：正确说帧 1（高归一化 SNR）更重要，代码却说帧 0 更重要。
这正是负责人担心的「高 SNR 帧归一化后等效权重被降低」。

### 2.3 生产 scheme B（无 `g`）：缺参数项导致帧权被**高估**（反方向错误）

同一 Oracle 的 production 列（`J_out = [0.., −1 (b_k)]`，无 `/g`）：

| 场景 | `Var_correct = raw + param` | `Var_used = raw` | `used/correct` 权重比 |
|---|---|---|---|
| A well-cond. | 9.755 (raw 6.76 + param 2.995) | 6.76 | **1.443**（代码高估 44%） |
| C ill-cond. | 339.5 (raw 6.76 + param 332.8) | 6.76 | **50.2**（代码高估 50×） |

生产路径没有 `g`，所以不会触发乘性降权；但**参数不确定度缺失**使「归一化修正量本身很不可靠
的帧」被严重高估权重——与「按预测残差方差加权」的要求相反。且生产连 `C_stat`（Phase1 逐像素
方差）都没有（输入帧 `n_ivar_tiles=0`）。

### 2.4 生产 L4 实测：当前**没有权重面**，判据以「空真」成立

- `p2_final.json`：`products=['signal','support']`，`uncertainty_available=false`。
- `mosaic_upmfix.json`/`mosaic_49_w1.json`/`mosaic_bit_*.json`：`weight_mode=1` ⇒ 每样本 `w=1`。
  ⇒ `w_highSNR == w_lowSNR == 1`，「高 ≥ 低」**空真**成立，但只是因为**根本没有方差面**。
- `mosaic_49.json`（`weight_mode=2`）实测 **fail-closed**：
  ```
  phase2 failed: node integrate failed: weight_mode=2 requires per-frame ivar products;
  49/49 frames missing ivar; frame-SNR weight chain NOT closed (unclosed_invalid_reference_flux):
  ASTROCS_REFERENCE_FLUX 逐帧不一致（组内公共通量标度要求）
  ```
  （`run/RELEASE-02/L4-rebuild/mosaic_out/astrocs_run_212e46b74828.json`，status=incomplete）
- Phase1 输入帧实测：`n_ivar_tiles=0, n_variance_tiles=0, products=['signal','support']`
  （`norm/t2_m1_red/.../p1_final.json`）⇒ mode-2 无来源。

### 2.5 真实帧 SNR / F_ref 配对（权重链接口）

对 49 帧 L4 输入（`mosaic_upmfix.json` 的 `hips_paths`）读 HiPS `signal/properties`：

- `ASTROCS_FRAME_SNR`：min 21.364 / max 47.388 / median 27.446（`max/min=2.22`）；
- `ASTROCS_REFERENCE_FLUX`：min 1728.85 / max 11882.99 / median 3459.66（`max/min=6.87`）；
- **3/49 帧完全缺这两个键**（3 个 `M42_M4_T2_...` 帧）⇒ 权重链
  `unclosed_missing_frame_snr`；
- 其余 46 帧的 F_ref **逐帧不同**（非组内公共）⇒ 1e-9 公共性门**永不可满足**。

若采用「逐帧 F_ref 作分母」的错误口径（`weight_chain.h:34-41` 明确点名的 Phase1 写侧缺陷），
公共尺度权重应为 `w_correct = SNR²/F_common²`，则 `w_correct/w_used = (F_ref/F_common)²`：

| 帧 | SNR | F_ref | `w_correct/w_used` |
|---|---|---|---|
| `...20251128_050425...`（最高 SNR） | 47.388 | 11882.99 | **11.797** |
| `...20251128_070149...` | 44.290 | 9946.68 | 8.266 |
| `...20251212_035745...`（最低 SNR） | 21.364 | 1728.85 | 0.250 |

⇒ **最高 SNR 帧会被压低 11.8×**（相对配对链）；最低 SNR 帧反被抬高 4×。
当前生产以 fail-closed 挡住（安全），但这是数据面缺陷而非已修复。

---

## 3. 正确传播公式

记号：帧 `k`、像素 `p`；`y`=归一化前信号；`C_k`=UPM 加性校正场；`δ_k = b_k − B_ref`；
`g_k`=乘性光度响应；`θ`=UPM 参数向量（含 `s`、`g`、`b`）。

### 3.1 归一化定义

```
corrected_k(p) = ( y_k(p) − C_k(p) − δ_k(p) ) / g_k
```

生产 SD-22 方案 B 是 `g_k ≡ 1` 的特例（`corrected = y − C_k − δ_k`）。

### 3.2 方差传播（三项齐全）

```
Var(corrected_k(p)) = [ σ²_{y,k}(p) + J_out,k(p) · C_theta · J_out,k(p)^T ] / g_k²
```

其中：

1. **Phase1 噪声项 `σ²_{y,k}(p)`**（= `C_stat`）：输入帧逐像素方差（Phase1 variance/ivar 产品）。
2. **乘性项**：整体除 `g_k²`。若写成
   `Var = σ²_{y,k}/g_k² + J'_k C_theta J'_k^T`，则
   `J'_k = g_k·J_out,k = [ … , −corrected_k , −1 , … ]`（对应 `g_k`、`b_k` 槽）。
3. **加性参数协方差项**（`FZ-FORMULA-COV-PROP`，`upm.h:225-228`）：
   ```
   C_theta = (J^T W J)^{-1}     # gauge 消除后的可辨识子空间
   C_out   = C_stat + J_out C_theta J_out^T
   J_out,k = ∂corrected_k/∂θ
           = [ −(1/g_k)·∂C_k(p)/∂s_j ,  −corrected_k(p)/g_k (g_k) ,  −1/g_k (b_k) , 0 (其他帧) ]
   ```
   **禁止**由权重标量/诊断量反推 variance；**禁止** `variance = 1/W_psfsw`（`upm.h:228`）。

### 3.3 权重

```
w_k(p) = 1 / Var(corrected_k(p))            # [ADU^-2]，与 Phase1 W_info 同量纲
```

**关键性质**：`w` 是 `Var(corrected)` 的严格单调递减函数。只要按上式算 `w`，
「高归一化 SNR 帧权重 ≥ 低归一化 SNR 帧」**由构造保证**；任何用
`Var(y)` 或 `SNR²/F_ref²`（非配对）替代 `Var(corrected)` 的做法都可能破坏该序
（§2.2/§2.5 已量化）。

### 3.4 与设计权威的对应

- `lib/algorithms/coverage/include/astro/phase2/upm.h:225-228,326-335`：
  `C_theta=(J^T W J)^-1`、`C_out=C_stat+J_out C_theta J_out^T`、禁权重反推。
- `docs/plugins/algorithms_phase2/11_upm.md:6`：「`g_k`（乘法）与 `b_k(x)`（加性）不得互相代替；
  **校准参数不确定度必须传播**」。
- `docs/plugins/algorithms_phase2/12_rejection.md:15/22`：输入含
  「预测残差方差（含 Phase1 噪声 + UPM 参数不确定度）」；阈值**不得**用固定全局阈值。
- `docs/science/PHASE2_UPM.md`（现行冻结模型）为**纯加性**（`calibrated = raw − C_f(p)`），
  与 `11_upm.md §4.1` 的乘加模型冲突已在 `PHASE2_UPM.md:182` 登记 UNRESOLVED；
  本公式对两种口径都成立（`g_k≡1` 时退化）。

---

## 4. 最小改动面（**本轮不实施**，供前台裁决）

### M1（直接命中负责人要求）——乘性项必须进方差/权重

**文件:行**：

- `lib/algorithms/coverage/tools/stage2.cpp:1089` 与 `:1460`：`out_v[i] /= gain;` 之后，
  该样本的**方差**必须除以 `gain²`（等价：权重乘 `gain²`）。
- 承载点（把 `frame_gain[f]²` 带进权重）：
  - ACR 路径：`:1017`（`local_ivar_map`）、`:1025`（`local_snr_map`）、`:1010`（`frame_snr` fallback）
    → `weight_compact[...] = v * gain(f)²`。
  - CPU 路径：`:1268/:1294/:1300` 与 `:1551/:1587/:1593` → 取到的 `ivar_v/snr_v` 乘 `gain(f)²`。

**改法**：定义每帧 `gain2(f) = g_f²`（`frame_gain` 已在 `:455` 建立、`:1076/:1447` 在用），
在写权重前统一 `w *= gain2(f)`；或等价地输出 `ivar_norm = ivar_raw·g_f²`。

**为何保证「高 SNR 帧权重不降低」**：`corrected = y/g` ⇒ `Var(corrected) = Var(y)/g²`
⇒ `w = 1/Var(corrected) = g²·(1/Var(y)) = g²·w_raw`。乘 `g²` 后 `w` 严格是归一化方差的
递减函数，权重序 = 归一化 SNR 序（由构造）。不乘则最多压到 `1/g²` 并可翻转序（§2.2）。
生产 scheme B 无 `g`（`g≡1`）⇒ **生产当前不需要 M1**；一旦把 `g_k` 接入生产，
`p2_op_upm_apply` 必须同时产出 `var/g²`，`p2_op_integrate` 必须消费它。

### M2——加性参数不确定度 `J_out C_theta J_out^T`（合同要求，缺一不可）

- **参考路径**：`stage2.cpp:518` 建了 MA 模型、`:567` 关闭；在 `:567` 之前调用
  `p2_upm_ma_param_cov` 取 `C_theta`，对每个 `(frame, control)` 算
  `J_out C_theta J_out^T`（`J_out = [−1/g_k (b_k), −corrected/g_k (g_k)]`），
  加到逐样本方差。
- **生产路径**：`p2_op_upm_fit` 只建 W2 模型，W2 **没有**协方差 API。
  两条可选最小路线：
  1. 为 W2 加一个 `p2_upm_param_cov`（与 `p2_upm_ma_c_out` 同形），
     `p2_op_upm_apply` 逐像素算 `J_out C_theta J_out^T`，写
     `p2_corrected_var_f<fid>.bin`，`p2_op_integrate` 在 mode-2 优先读它；
  2. 或让 `p2_op_upm_fit` 同时建 MA 模型（现成 API），生产改用 MA 的 `C_theta`。
  任一都涉及 API/artifact schema，属**非最小**改动，须负责人裁决。
- **过渡期纪律（已是现状，须保持）**：拿不到 `C_theta` 时**必须**保持
  `uncertainty_available=false` 且**不得**声称逆方差加权；**不得**用
  `1/W_psfsw` 或权重标量反推（`upm.h:228`）。

### M3——权重链配对（数据面，非 Phase2 代码）

- Phase1 写侧必须给**整组**传同一个公共 `F_ref`（`snr_frame_science.cpp:174` 已要求，
  但 L4 数据违反：逐帧 F_ref 变化 6.87×、3 帧缺键）。修好后 mode-2 的 SNR 链才能闭合。
- 若归一化含 `/g_k`，帧级 SNR 链必须与同一 `g_k` 配对：
  `w = (SNR·g_k)²/F_ref² = SNR²/F_ref² · g_k²`。当前 `compute_inverse_variance_weights`
  签名（`weight_chain.h:172-174`）**没有 `g_k` 入参** ⇒ 接口缺口。

---

## 5. 与权重链（`lib/algorithms/integration/v6/weight_chain.*`）的接口

### 5.1 现状

- `compute_inverse_variance_weights(frames, reference_flux)` 产出
  `w = actual_snr²/F_ref²`（`weight_chain.cpp:89`），`actual_snr = 帧级 SNR × 帧内 SNR`。
- 语义前提（`weight_chain.h:34-41` 配对性定理）：`SNR_f = a_f·F_ref/σ_f ⇒ SNR²/F_ref² = a_f²/σ_f²`，
  **要求分子 SNR 与分母 `F_ref` 同源**，且 `F_ref` 是**组内公共常数**；
  逐帧 F_ref 会丢掉 `a_f²`。
- 生产调用点 `module_adapters.cpp:5675-5745`：读 HiPS 头
  `ASTROCS_FRAME_SNR`/`ASTROCS_REFERENCE_FLUX`，先强制**组内公共 F_ref**
  （相对容差 1e-9，`:5703`），否则 fail-closed。**这是正确且必须保留的。**

### 5.2 一致性判定

| 面 | 量纲/尺度 | 与归一化方差的接口 |
|---|---|---|
| 权重链 | 帧级常数 `[ADU^-2]`，`w=SNR²/F_ref²` | 只在「归一化不改变帧内噪声形状、且 SNR 用公共 F_ref 定义」时与逐像素方差一致 |
| 归一化方差 | 逐像素 `1/Var(corrected_k(p))` | 含 `1/g_k²` 与 `J_out C_theta J_out^T`，随像素变化 |

**两者当前不一致**，原因三条：

1. **没有 `g_k`**：若归一化除 `g_k`，帧级权重必须乘 `g_k²`；权重链接口无此输入。
2. **F_ref 非公共**：L4 数据 46 帧 F_ref 变化 6.87×、3 帧缺键 ⇒ 链 fail-closed（安全，
   但无权重可用）。
3. **参数不确定度**：权重链只有帧级 SNR，没有 `C_theta` 项；生产 reject 用 MAD 自估，
   三者互相不闭合。

### 5.3 建议接口（最小、向后兼容）

```
priority 1: 逐像素 w_k(p) = 1 / Var(corrected_k(p))     # 有 p2_corrected_var 产品时
priority 2: 帧级     w_k    = SNR_k²/F_ref² · g_k²       # ivar/var 缺失时的回退
            （SNR_k 与 F_ref 必须同源、F_ref 组内公共；缺一 → fail-closed，不静默等权）
```

即在 `compute_inverse_variance_weights` 的 `FrameWeightInput` 增一个可空
`gain`（或让调用方在 `frame_snr` 上预先乘 `g_k`）；并在 `p2_op_integrate` 里让
逐像素方差优先于帧级链。这样权重序恒等于归一化 SNR 序，满足负责人判据。

---

## 6. 诚实边界与未做

- **未跑** `ninja`/`cmake`/`ctest`（按任务约束）；Oracle 只做 `g++ -O0` 链接既有
  `build/libastrocs_phase2.a`，未改任何生产代码/文档。
- **未改**任何文件（除本报告与 `run/RELEASE-02/unc-prop/` 证据）；零 git 写。
- `stage2` 参考路径的 `g_k` 是否在生产 L4 中实际非 1，取决于该 run 是否走 stage2；
  本轮按任务给定口径（stage2 为参考路径）审计代码路径本身。
- 生产 scheme B 无 `g_k`，其「缺参数项」方向是**高估**帧权（§2.3），与负责人所忧方向相反，
  但同为「方差未传播」缺陷，且 `12_rejection.md:15` 要求含参数不确定度。
- `C_theta` 的量级依赖拟合条件数（Oracle kappa 43–1917，`J C_theta J^T` 从 0.46 到 333），
  生产实际量级须由真实 UPM 模型复算（本轮未取，因生产 W2 无协方差 API）。

---

## 7. 证据文件

| 文件 | 内容 |
|---|---|
| `run/RELEASE-02/unc-prop/audit_evidence.py` | 代码事实 + 真实帧 SNR/F_ref + 判据的只读脚本 |
| `run/RELEASE-02/unc-prop/code_facts_and_realdata.log` | 上述脚本输出（含行号、artifact 键集、49 帧统计） |
| `run/RELEASE-02/unc-prop/unc_prop_oracle.cpp` | `J_out C_theta J_out^T` / 权重抑制 / g 扫描 Oracle |
| `run/RELEASE-02/unc-prop/unc_prop_oracle` | 已编译 Oracle（g++ -O0，链接 build 静态库） |
| `run/RELEASE-02/unc-prop/oracle_output.txt` | Oracle 输出（4 场景 + 5 点 g 扫描） |
| `run/RELEASE-02/unc-prop/build.log` | Oracle 编译日志（0 错误） |
