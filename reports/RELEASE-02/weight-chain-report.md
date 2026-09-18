# RELEASE-02 权重链修复报告（Phase2 SNR → 逆方差权重）

> 执行者：Phase2 权重链修复 SubAgent。工作目录：/workspace/Astro CS Database。
> 时点：2026-09-18。零 git 写；未跑 ninja/cmake/ctest（仅 `g++ -fsyntax-only`）。
> 文件面：`lib/algorithms/integration/**`（+ 只读引用 `lib/algorithms/noise_snr/**`）。未改 coverage/infrastructure/docs/tests/ci/contracts。

---

## 0. 结论摘要

| 缺陷 | 处置 | 状态 |
|---|---|---|
| B3/B4：`w = SNR²/F_ref²` 无实现 | 新增科学核心 `weight_chain.{h,cpp}`：由帧级/帧内 SNR 现场换算逆方差权重 | **已实现（代码 + Oracle）** |
| B4：稀疏 SNR 层零实现 | 新增稀疏层重建（规则网格双线性 / 显式半径最近点）+ 帧级×帧内合成 | **已实现** |
| legacy 静默降级（L3 假绿） | 模块内 `legacy_allow_weight_fallback` **恒 fail-closed** 且显式报「权重链未闭合」，绝不返回成功 | **科学侧已闭合**；scheduler 侧接线见 §6（**需前台实施，不在本文件面**） |
| 科学文档口径冲突 | `CONTROL_WEIGHT_SNR.md` §2a/§7 与 `UNIFIED_MODEL.md`/`07_noise_snr.md` §4.1 互斥（文档已登记 UNRESOLVED） | **上呈负责人裁决，未改文档** |

**关键等价**：`w = SNR²/F_ref² = 1/σ_F² = W_info`。即只要 HiPS 头写出通量型帧级 SNR（+ 可选稀疏层），Phase2 就能**自行恢复**逐帧逆方差权重，**不再依赖缺失的 per-frame ivar 产品**——这正是消除 legacy 等权降级的科学出路。

---

## 1. 权威依据（只读，逐条锚定）

| 条款 | 原文要点 | 文件:行 |
|---|---|---|
| DESIGN §4.3 | 逆方差叠加：SNR → 逆方差权重；`w = 1/σ² = SNR²/F_ref² ∝ SNR²`；不是直接用 SNR 加权；按需计算不预计算稠密 | `ASTROCS_DESIGN.md:255-261` |
| DESIGN §4.3 检测图 | 自动检测稀疏层：有 → 帧级×帧内；无 → 帧级 | `ASTROCS_DESIGN.md:244-254` |
| DESIGN §3.4 | 帧级 SNR = 未加权原始通量型 `F_ref/σ_F`，写入 HiPS 头（唯一承载）；稀疏层启用时 实际 SNR = 帧级×帧内 | `ASTROCS_DESIGN.md:163-175` |
| DESIGN §2 | 数据对象禁止互相冒充（frame_snr ≠ depth_m5 ≠ weight） | `ASTROCS_DESIGN.md:84-86` |
| noise_snr §4.1 | 通量型口径 `SNR_k = F_ref·sqrt(W_psf,k) = F_ref/σ_F`；`w_k = SNR_k²/F_ref² = 1/σ_F,k²` | `docs/plugins/algorithms_phase1/07_noise_snr.md:55-70` |
| integration §4.0 | 稀疏层存在 → 帧级×帧内；无 → 帧级；损坏/不可重建 → 明确失败，不得静默回退帧级 | `docs/plugins/algorithms_phase2/13_integration.md:32-38,80` |
| ALG-P2-POINT-001 | `SNR_k = F_ref·sqrt(W_info,k)`；`SNR_combined² = Σ SNR_k²` 仅独立帧成立 | `docs/algorithms/v6/phase2-point/ALG-P2-POINT-001_SPEC.md:116-132` |
| sparse schema | `control_points[{x,y,sparse_snr_value}]`，`role=intra_frame_reference`，`object_weight_capability=false` | `contracts/schemas/unified/sparse_snr_layer.schema.json:279-331` |

---

## 2. 交付物（全部在文件面内）

| 文件 | 说明 |
|---|---|
| `lib/algorithms/integration/v6/include/astrocs/v6/weight_chain.h` | 公共 API：SNR 语义判别、稀疏层、标量换算、多帧权重链、闭合状态枚举 |
| `lib/algorithms/integration/v6/src/weight_chain.cpp` | 纯 FP64 实现；全退化路径 fail-closed，零静默降级 |
| `lib/algorithms/integration/v6/oracle/weight_chain_selfcheck.cpp` | C++ 独立 Oracle（TU 内独立复算 1/σ_F² + 手写双线性），正/负例 |
| `lib/algorithms/integration/v6/oracle/weight_chain_oracle.py` | Python 独立 FP64 Oracle（不同语言独立复算 + 参考合同负例） |
| `lib/algorithms/integration/v6/oracle/CMakeLists.txt` | Oracle 自包含构建（前台统一构建） |
| `lib/algorithms/integration/v6/CMakeLists.txt` | 增列 `src/weight_chain.cpp` 进 `astrocs_v6_phase2_integrate` |
| `reports/RELEASE-02/weight-chain-report.md` | 本报告 |

---

## 3. 科学计算实现

### 3.1 核心公式

```text
帧级 (HiPS 头):   frame_snr_k = F_ref / σ_F,k          [1]      未加权原始通量型 SNR
稀疏层 (可选):    intra_snr_k(x,y)                      [1]      帧内相对 SNR 因子
实际 SNR:         actual_k = frame_snr_k × intra_k      [1]      DESIGN §3.4:174
逆方差权重:       w_k = actual_k² / F_ref² = 1/σ_F,k²   [ADU^-2] DESIGN §4.3:256
```

`weight_from_snr()` 直接实现 `w = (SNR/F_ref)²`；与 `W_info = 1/Var(F_hat)` 同量纲同值（在模型一致时），可用作独立交叉校验。

**前置条件（DESIGN §4.3:256）**：调用前帧必须已由 UPM 归一到公共通量尺度，`F_ref` 为组内公共常数；未归一化时换算不成立，调用方不得调用本模块（已写入头文件契约）。

### 3.2 稀疏层重建（显式算子，写 manifest）

- `regular_grid=true`：`nx×ny` 控制点（行主序 `j*nx+i`，`x=x0+i·dx, y=y0+j·dy`），**双线性插值**；
  - 几何一致性门：任一控制点位置偏离声明网格 > `grid_tol` → fail-closed；
  - **控制点自身复现自检**：经同一求值路径在节点处复现，残差 > `1e-9` → fail-closed（识别损坏层）；
  - 查询越界 → fail-closed（**不外推**）。
- `regular_grid=false`：最近控制点，**必须显式给 `max_radius_px>0`**；超半径/未声明半径 → fail-closed（**不外推、不回退帧级**）。
- 算子 id（进 manifest）：`bilinear_regular_grid_v1` / `nearest_control_point_v1`。

### 3.3 fail-closed 闭合状态（`WeightClosure`）

| closure token | 触发 |
|---|---|
| `closed` | 全部帧 SNR 有效，权重成立（`ok=true`） |
| `unclosed_missing_frame_snr` | HiPS 头无通量型 frame_snr |
| `unclosed_invalid_frame_snr` | SNR 非有限 / ≤0 |
| `unclosed_invalid_reference_flux` | `F_ref` 非有限 / ≤0 |
| `unclosed_sparse_layer_unreconstructible` | 层存在但空/损坏/越界/未声明半径 |
| `unclosed_invalid_intra_snr` | 重建帧内 SNR 非有限/≤0 |
| `unclosed_non_finite_weight` | 换算权重非有限/≤0 |
| `unclosed_wrong_snr_semantics` | 传入非「通量型未加权原始 SNR」（相对质量权重/中位诊断量冒充） |
| `unclosed_legacy_fallback_rejected` | 显式 `legacy_allow_weight_fallback=true` 请求等权降级 |
| `baseline_equal_weight` | 显式非生产等权基线（**不是闭合**，`ok=false`） |

任何非 `closed` 状态：`ok=false`、`weight_chain_closed=false`、`production_allowed=false`、`weights` 为空（失败时不交出可用权重）。

---

## 4. Oracle 正/负例（已实测）

### 4.1 Python 独立 Oracle — **实测 25 passed / 0 failed**

命令：`python3 lib/algorithms/integration/v6/oracle/weight_chain_oracle.py --out run/RELEASE-02/weight-chain/oracle_result.json`

正例（注入已知 SNR ⇒ 权重可复算，rtol 1e-12）：
- 标量：`F_ref=1000, SNR∈{200,100,50}` ⇒ `w∈{0.04,0.01,0.0025}`，同时满足 `SNR²/F_ref²` 与 `1/σ_F²` 两式；
- 多帧（无稀疏层）：逐帧权重 = 独立复算值；独立帧恒等式 `SNR_combined² = Σ SNR_k²` 成立；
- 稀疏层 2×2：查询 `(0.5,0.5)` ⇒ intra=1.0，查询 `(0.25,0.5)` ⇒ intra=0.95；`actual=frame×intra`，权重按合成 SNR 复算；节点复现残差 ~0。

负例（全部 fail-closed，**不得静默退化为等权**）：
- SNR 缺失 → `unclosed_missing_frame_snr`；
- SNR ∈ {NaN, +Inf, -3, 0} → `unclosed_invalid_frame_snr`；
- `F_ref ∈ {0, -1, NaN}` → `unclosed_invalid_reference_flux`；
- 稀疏层 present-but-empty / 越界 → `unclosed_sparse_layer_unreconstructible`（**显式禁止回退帧级**）；
- 语义冒充（`relative_quality_weight`）→ `unclosed_wrong_snr_semantics`；
- `legacy_allow_weight_fallback=true` → 仍 `unclosed_missing_frame_snr`（参考合同层面）。
- 非空真：逆方差权重 ≠ 全 1（证明负例/正例非空断言）。

### 4.2 C++ 独立 Oracle — 已交付，**待前台构建执行**

`oracle/weight_chain_selfcheck.cpp` 覆盖同一组正/负例（含 legacy 显式请求 → `unclosed_legacy_fallback_rejected` 且 `weights.empty()`）。本任务受「不跑构建、最多 `g++ -fsyntax-only`」约束，**未执行**；前台按 `oracle/CMakeLists.txt` 构建后应得 `0 failed`。

`g++ -std=c++17 -Wall -Wextra -fsyntax-only`：两个 TU **rc=0，零 warning**（证据 `run/RELEASE-02/weight-chain/syntax_check.log`）。

---

## 5. legacy fallback 的处置

**模块内（本文件面，已闭合）**：`WeightChainPolicy::legacy_allow_weight_fallback` 置真**不会**产生成功结果。实现恒返回：

```text
ok=false, weight_chain_closed=false, production_allowed=false
closure=unclosed_legacy_fallback_rejected
error="weight chain NOT closed (权重链未闭合): <底层原因>;
       legacy equal-weight fallback rejected (...)"
weights=[]            # 不交出可用权重
legacy_equal_weight_used=true, diagnostic_equal_weights=[1,1,...]  # 仅诊断
```

即：任何调用方若继续沿用旧的「置真即成功」语义，会**直接拿到 fail-closed**，不可能再产生 L3 式假绿。

**scheduler 侧（不在本文件面，需前台实施）**：真正的静默降级点在
`lib/infrastructure/scheduler/src/module_adapters.cpp:4938-4961`（`weight_mode=2` 缺 ivar → `legacy_allow_weight_fallback=true` 显式等权降级，`weight_basis=unit_weight_degraded`，仍继续运行）。该文件属基础设施分片，本任务**未改**。详见 §6 的接线约定。

---

## 6. 需要 scheduler 侧怎么接（调用约定）

### 6.1 目标：用「HiPS 头 SNR → 权重」替代「缺 ivar → 等权降级」

`p2_op_integrate`（`module_adapters.cpp`）在 `weight_mode=2` 且 ivar 产品缺失时，当前走等权降级。应改为：

1. **读元数据**（Phase1 侧须先把帧级 SNR 写入 HiPS `properties` + manifest 双写面；键名建议 `ASTROCS_FRAME_SNR`、`ASTROCS_REFERENCE_FLUX`，稀疏层建议 `ASTROCS_SPARSE_SNR_LAYER` 引用层文件/内嵌层。**键名为建议，须与 Phase1 写入端一并冻结**，本任务不擅定合同）。
2. **逐输出像素构造输入**：
```cpp
#include "astrocs/v6/weight_chain.h"
using namespace astrocs::v6::p2weight;

std::vector<FrameWeightInput> inputs;
for (each frame f covering this output pixel) {
  FrameWeightInput in;
  in.frame_id        = f.frame_id;
  in.kind            = FrameSnrKind::kFluxTypeUnweightedSnr;  // 必须；质量权重/中位诊断量会被拒
  in.has_frame_snr   = (header has ASTROCS_FRAME_SNR);
  in.frame_snr       = header.frame_snr;                       // F_ref/σ_F
  in.sparse          = (layer present ? &layer : nullptr);     // 有→帧级×帧内；无→帧级
  in.x = out_x_in_this_frame; in.y = out_y_in_this_frame;      // 该帧像素坐标
  inputs.push_back(in);
}
const WeightChainResult w =
    compute_inverse_variance_weights(inputs, group_reference_flux);
if (!w.ok) {
  // fail-closed：返回 DATA 错误，附 w.closure token 与 w.error（显式「权重链未闭合」）
  return Result<void>::fail(Error(ErrorDomain::DATA,
      std::string("weight chain not closed: ") + weight_closure_token(w.closure) +
      " — " + w.error));
}
// w.weights[k] = 1/σ_F,k² [ADU^-2]，直接作为 p2_integrate_pixel 的逐样本 ivar 权重
```
3. **`legacy_allow_weight_fallback=true` 必须改为 fail-closed**：收到该键时返回 `Error`（或至少不产生 `uncertainty_available=true`、不写 variance/ivar 产品），**不得**再继续等权叠加并报成功。建议保留键名兼容但语义改为「显式拒绝 + 明确报错」，删除 `weight_basis=unit_weight_degraded` 成功路径。
4. **manifest/provenance**：记录 `w.weight_source`（`frame_snr` / `frame_snr_x_sparse_snr`）、逐帧 `sparse_operator_ids` 与 `sparse_node_residual`（重建算子与误差，满足 integration §4.0「重建算子与误差入 manifest」）。
5. **等权仅作显式基线**：若确需等权对照，调用 `make_equal_weight_baseline(n)`，其 `closure=baseline_equal_weight`、`production_allowed=false`，**不得**计入生产权重链成功。
6. **按需计算**：DESIGN §4.3 要求不预计算稠密权重；`compute_inverse_variance_weights` 是逐输出像素的纯函数，天然满足分块/流式。

### 6.2 与现有 Phase2 集成层的关系

现有 `run_point_information` 走 Phase1 `view.w_info`（等价量），本模块是其**从 HiPS 头 SNR 恢复权重**的补充路径，不改动现有冻结行为（未改 `phase2_integrate.cpp`）。两者在模型一致时数值一致（`w = SNR²/F_ref² = W_info`），可作为交叉校验。

---

## 7. 上呈负责人裁决（未擅自改文档）

1. **frame_snr 语义互斥（文档已登记 UNRESOLVED）**：
   - `docs/science/CONTROL_WEIGHT_SNR.md:140` 自述：本文件把 `frame_snr` 定义为「相对质量权重场，不是科学信噪比」；
   - `docs/design/UNIFIED_MODEL.md` §2 与 `docs/plugins/algorithms_phase1/07_noise_snr.md:55-70` 定义为「帧级未加权原始通量型 SNR（方法学对标 PixInsight PSFSNR）」；
   - 二者语义互斥。本实现**站在最高设计 §3.4/§4.3 与 07_noise_snr §4.1 一侧**（通量型 `F_ref/σ_F`），并以 `FrameSnrKind` 显式拒绝相对质量权重/中位诊断量冒充，从而对冲突本身 fail-closed。**请负责人裁决是否订正 CONTROL_WEIGHT_SNR §2a/§7 的措辞**（涉及 SCI 冻结文档，须走变更 claim）。
2. **稀疏层数值语义未在合同冻结**：`sparse_snr_layer.schema.json` 只声明 `sparse_snr_value`「控制点 SNR 参考值」，未区分「绝对帧内 SNR」与「相对帧级因子」。本实现按 DESIGN §3.4:174「实际 SNR = 帧级 × 帧内」的**相对因子**解释（`actual = frame × intra`）。**请负责人在合同/科学文档中冻结该口径**，否则不同 Phase1 写入端可能写出不可比的值。
3. **HiPS 头 SNR 键名未冻结**：审计确认 `properties` 当前无任何 SNR 键（DC-307/308/704/705）。本任务不擅自发明合同键名，仅在 §6.1 给建议，**须由 Phase1 写入分片与合同分片统一冻结**。

---

## 8. 边界与未做

- 未改 `lib/algorithms/coverage/**`、`lib/infrastructure/**`、`docs/**`、`tests/**`、`ci/**`、`contracts/**`、根/公共 CMakeLists；
- 未改 `lib/algorithms/integration/v6/src/phase2_integrate.cpp`（现有冻结行为零改动）；
- 未跑构建/ctest（硬要求）；C++ Oracle 待前台构建执行；
- scheduler 接线、Phase1 写头、合同键名冻结均不在本文件面，见 §6/§7。

---

## 9. 证据索引

| 证据 | 路径 |
|---|---|
| Python Oracle 结果（25/25 通过） | `run/RELEASE-02/weight-chain/oracle_result.json` |
| 语法检查日志（2 TU rc=0，零 warning） | `run/RELEASE-02/weight-chain/syntax_check.log` |
| Python Oracle 脚本 | `lib/algorithms/integration/v6/oracle/weight_chain_oracle.py` |
| C++ Oracle 脚本（待构建） | `lib/algorithms/integration/v6/oracle/weight_chain_selfcheck.cpp` |
