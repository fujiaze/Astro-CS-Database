# FIX-GK 报告：Phase2 天光面归一化改为方案 B（`corrected = raw − δ_k`）

- 任务：RELEASE-02 / FIX-GK（原任务书要求接 `/g_k`，**已被负责人裁决否决**）
- 裁决：方案 B —— `corrected_k(x) = raw_k(x) − δ_k(x)`，`δ_k(x) = b_k(x) − B_ref(x)`
  （等价 `raw_k − b_k + B_ref`；把每帧归一化到公共参考面 `B_ref`，多退少补，
  保留 `B_ref` 真实天光亮度，只消除帧间差异；**不扣整个背景面、不除 `g_k`**）
- 改动文件：`lib/algorithms/coverage/include/astro/phase2/sky_plane.h`、
  `lib/algorithms/coverage/src/sky_plane.cpp`、
  `lib/infrastructure/scheduler/src/module_adapters.cpp`、
  `lib/algorithms/coverage/tools/stage2.cpp`、
  `lib/algorithms/coverage/include/astro/phase2/stage2_common.h`（仅注释）
- 证据目录：`run/RELEASE-02/fix-gk/`
- 未跑 `ninja`/`cmake`/`ctest`（按要求由前台构建）；仅 `g++ -fsyntax-only` + 独立 Oracle。
- 未改 `docs/**`；未做任何 git 写。

---

## 1. 改动点（file:line）

| # | 文件:行 | 改动 |
|---|---|---|
| 1 | `lib/algorithms/coverage/include/astro/phase2/sky_plane.h:236-253` | 新增两个只读求值入口：`p2_sky_plane_eval_delta`（:242）与 `p2_sky_plane_eval_delta_block`（:248）。**`p2_sky_plane_eval` / `p2_sky_plane_eval_block` 本体与语义一字未动**（其他调用方仍取 `b_k=B_ref+δ_k`）。 |
| 2 | `lib/algorithms/coverage/src/sky_plane.cpp:1156-1188` | 实现 `p2_sky_plane_eval_delta`：只算 `gauge_shift + δ_k 多项式项`，即 `b_k − B_ref`。与 `p2_sky_plane_eval` 共用同一 gnomonic/越域/basis 约定。 |
| 3 | `lib/algorithms/coverage/src/sky_plane.cpp:1191-1207` | 实现 `p2_sky_plane_eval_delta_block`（逐点状态码，语义同 `p2_sky_plane_eval_block`）。 |
| 4 | `lib/infrastructure/scheduler/src/module_adapters.cpp:4937-4956`（`p2_op_upm_apply` 逐 tile 校正） | 生产路径 `corrected = (raw − C_k(x)) − δ_k(x)`：把 `p2_sky_plane_eval_block` 换成 `p2_sky_plane_eval_delta_block`，扣除量由 `b_k` 改为 `δ_k`。`p2_upm_calibrate_block`（唯一真实校正入口）**语义未动**。 |
| 5 | `lib/infrastructure/scheduler/src/module_adapters.cpp:5017,5022` | `p2_corrected.json` 的 `entry` 更新为 `.../p2_sky_plane_eval_delta_block`；新增 provenance 键 `"sky_plane_mode": "delta_to_B_ref"`（无天光面时 `"none"`）。 |
| 6 | `lib/algorithms/coverage/tools/stage2.cpp:1070-1088` 与 `:1443-1459`（参考路径两处） | 与生产同口径：`corrected = (raw − C_k − δ_k) / g_k`。`g_k` 逻辑**未动**（保留原样）。 |
| 7 | `lib/algorithms/coverage/tools/stage2.cpp:451-453`、`lib/algorithms/coverage/include/astro/phase2/stage2_common.h:49-53` | 仅更新过时注释（原写 `corrected=(raw-C-b_k)`），避免注释与实现矛盾。 |

新增行数：`+118 / −29`（`git diff --stat`，只读）。

---

## 2. `B_ref` 与 `δ_k` 的存储 / 求值（file:line）与 gauge 约定

### 2.1 存储
- `SkyPlaneModel::coeff`（`sky_plane.cpp:231`）：**`B_ref`** 的 `nx*ny` 张量积 B 样条系数（切平面均匀节点，`row-major [iy*nx+ix]`）。
- `SkyPlaneModel::deltas`（`sky_plane.cpp:238`）：**`δ_k`** 每帧 `m=(p+1)(p+2)/2` 个低阶多项式系数（归一化切平面坐标 `(u,v)`）。
- `SkyPlaneModel::gauge_shift`（`sky_plane.cpp:232`）：gauge 常数平移。
- 模型文件为稀疏 JSON（只写系数/网格/帧 δ），见 `sky_plane.cpp:1256+`。

### 2.2 求值
- `p2_sky_plane_eval`（`sky_plane.cpp:1089-1130`）：
  `out = (gauge_shift + B_ref_spline(u,v)) + δ_poly(u,v)`，即 `b_k`（`sky_plane.h:11-15, 221` 定义 `b_k = B_ref + δ_k`）。
- **新增** `p2_sky_plane_eval_delta`（`sky_plane.cpp:1156-1188`）：
  `out = gauge_shift + δ_poly(u,v)`，即 `δ_k = b_k − B_ref`。
- 两者满足恒等式 `p2_sky_plane_eval ≡ p2_sky_plane_eval_delta + B_ref`（Oracle [1] 实测误差 `0.0`）。
- 旧有 `p2_sky_plane_frame_delta`（`sky_plane.cpp:1209+`）只返回**未求值的多项式系数**，不给出 `δ_k(x)` 值，故不能直接用于像素级扣除——这正是需要新增求值入口的原因。

### 2.3 gauge 与参考帧约定
- `gauge_mode=0`（**生产/参考默认**，`sky_plane.cpp:398-407` 默认配置 + `:441` 校验；`stage2_common.h:58`；`module_adapters.cpp:4652`）：
  - 参考帧 = 分量内**最小 `frame_id`**（`sky_plane.cpp:585`）；
  - 参考帧 `δ_ref ≡ 0`（`sky_plane.cpp:920`）；
  - `gauge_shift = 0`（`sky_plane.cpp:1001`）；
  - 故参考帧 `b_ref = B_ref`，`δ_ref = 0`。
- `gauge_mode=1`（sum）：`δ_k` 常数项减均值、`gauge_shift=c`（`sky_plane.cpp:1002-1008`）。此时公共面 `B_ref` 仍等于样条部分，`δ_k = gauge_shift + δ_poly`；`b_k = B_ref + δ_k` 不变。新增入口对两种 gauge 均给出正确的 `b_k − B_ref`。
- `B_ref` 的定义取**样条部分**（不含 `gauge_shift`）：因为 `eval − delta = spline` 恒成立，与 gauge 无关；这保证 `corrected = raw − δ` 在数值上把该帧放回 `B_ref`（Oracle 实测 NEW 均值 = `B_ref` 均值）。

---

## 3. 公式与越域 / 无效处理

- 生产：`corrected_k(x) = (raw_k(x) − C_k(x)) − δ_k(x)`，其中 `C_k` 来自 `p2_upm_calibrate_block`（W2 UPM 空间校正场，**语义未动**），`δ_k = b_k − B_ref`。
- 参考：`corrected_k(x) = (raw_k(x) − C_k(x) − δ_k(x)) / g_k`（`g_k` 逻辑原样保留）。
- **越域 / 未知帧**：`p2_sky_plane_eval_delta` 返回状态 `P2_SKY_EVAL_OUT_OF_DOMAIN` / `UNKNOWN_FRAME`，`out_value` 不写；调用侧（`module_adapters.cpp:4950-4955`、`stage2.cpp:1087/1458`）只在 `st == P2_SKY_EVAL_OK && isfinite` 时扣除，**否则保持 `raw−C`（不扣）**，与改动前完全一致，不做外插。
- 无天光面产物（`p2_sky_plane.bin` 不存在）时行为不变：`corrected = raw − C`，`p2_corrected.json.sky_plane_applied=false, sky_plane_mode="none"`。

---

## 4. 数值验证（独立 Oracle）

`run/RELEASE-02/fix-gk/gk_b_oracle.cpp`（自包含合成：`B_ref` 平滑平面 + 帧 200 = 帧 100 + δ=3 ADU；
gauge=reference_frame；`g++ -O0` 链接本仓静态库；未跑 ninja/cmake/ctest）：

```
n=36
[1] max|eval - (delta+B_ref)|        = 0.000e+00
[2] max|delta - 3.0|                  = 7.239e-14
[3] OLD max|raw - b_k| (背景归零)      = 4.161e-05
[4] NEW max|(raw-delta)-B_ref|        = 4.161e-05
[5] max|帧间台阶(new)-帧间台阶(old)|   = 1.421e-14
mean OLD corrected = -0.0000  (≈0, 背景被剪掉)
mean NEW corrected = 100.0000  (≈B_ref, 公共面保留)
mean B_ref         = 100.0000
ORACLE_PASS
```

要点：
- [1] 恒等式精确成立；[2] `δ_k` 正确恢复；
- [3]/[4] 残差量级相同（都是对拟合面的残差），但 **NEW 落在 `B_ref≈100`，OLD 落在 `0`** —— 方案 B 确实把背景抬回公共面（修「背景归零 / 负像素」）；
- [5] **帧间台阶新旧口径逐位相同（1.4e-14 = 浮点零）**。

语法验证：`sky_plane.cpp`、`stage2.cpp`、`module_adapters.cpp` 三文件
`g++ -fsyntax-only -std=gnu++17 -fopenmp`（用 `build/build.ninja` 中各自真实 INCLUDES/DEFINES）**全部 0 错误**，日志见 `run/RELEASE-02/fix-gk/syntax_*.log`。

---

## 5. 关键结论：方案 B 对「接缝」与「卫星线」的影响

> **方案 B 修背景绝对电平，但不改变帧间差，因此不改变接缝与卫星线行为。**

代数：`b_k = B_ref + δ_k`，故
```
OLD corrected_k = raw_k − b_k
NEW corrected_k = raw_k − δ_k
NEW_k − OLD_k = b_k − δ_k = B_ref(x)   ← 帧无关的公共面
⇒ 任意两帧： (NEW_k − NEW_j) ≡ (OLD_k − OLD_j)
```
帧子集边界处进入/退出的帧，其两两差在新旧口径下相同 ⇒ **等权/加权叠加的台阶不变**。
Oracle [5] 实测该不变量误差 `1.4e-14`。

推论（与既有报告对照）：
- **背景归零 / 负像素**：方案 B **修**（每帧抬回 `B_ref`）。
- **SEAM 接缝**：方案 B **不修**。`seam-attribution.md` 记录的台阶由 `δ_k` 在帧子集边界的子集均值跳变驱动，而 `δ_k` 正是帧间差项——它在 OLD 口径中已被 `b_k` 完整包含。要治接缝须在**帧间差**上另行设计（如对 `δ_k` 施加子集连续性/零点和约束，或减少逐帧自由度），不是 B vs OLD 的选择。
- **TRAIL 卫星线**：`trail-attribution.md` 的「逐帧归一后源拒绝率 0.696→0.985」收益来自**「sky_plane 生效」**（该次运行 `sky_plane_applied=false`，完全没有逐帧归一），而不是 B vs OLD；只要天光面生效，新旧口径的帧间差相同，排异输入等价。方案 B 对卫星线**中性**。

> ⚠️ 这一点与裁决说明中的因果预期（「只消除 δ_k ⇒ 帧间一致 ⇒ 无接缝」）不一致：OLD 的 `raw−b_k` 已经包含并消除了 `δ_k`，两种口径的**帧间项逐位相同**。请负责人据此决定接缝是否需另开任务。

---

## 6. `g_k` 未接线（如实报告）

按裁决「`g_k` 不接、不要动 `g_k` 的任何东西」：
- **生产路径未新增任何 `/g_k`**；`module_adapters.cpp` 内 `g_k`/`gain`（光度响应义）零新增。
- `stage2.cpp` 既有的 `g_k` 逻辑（MA 估计 + DC 比门 + `/gain`）**原样保留、未改**。
- 事实澄清（供后续参考）：`p2_upm_calibrate_block` 使用的 W2 模型（`upm.cpp:67-100` `struct Model`）**不包含** `g_k`；`g_k` 只存在于独立的 v6 UPM MA 求解器（`upm.cpp:1751+ MaModel.theta_full`，访问器 `p2_upm_ma_solution`，`upm.h:313`）。生产 `p2_op_upm_fit` 只建 W2 模型，从不建 MA 模型，故生产链上确实没有可用的 `g_k` 源。本轮未改动此现状。

---

## 7. 其他「漏 `/g_k`」或「漏 δ」的位置检查

- `module_adapters.cpp` 内 `p2_upm_calibrate_block` 只有 1 处调用（:4937），天光面扣除只有 1 处（:4944-4955），**均已改为 δ 口径**。
- `p2_upm_model.json` 在 :6251 只用于取 `model_hash` provenance，不参与校正，无需改。
- `lib/algorithms/coverage/src/sky_plane.cpp:1245` 的 `p2_sky_plane_eval` 调用在 `p2_sky_plane_residuals` 内，残差定义就是 `y − b_k`，**应保持 `b_k`**，不改。
- 全仓 `lib/**` 中 `p2_sky_plane_eval` 的其余调用仅在库内（`eval_block`、`residuals`）与测试；生产/参考两路径已一致。
- `lib/algorithms/integration/v6/src/phase2_integrate.cpp:1627` 另建 MA 模型取 `g_k/b_k/s`（v6 诊断/集成路由），不在本任务文件域，且属另一条线，未改。

---

## 8. 离群样本剔除核查（负责人第 2 问；本轮未实现）

**现状：天光面拟合只有 Huber 软降权，没有硬剔除离群样本。**
- `sky_plane.cpp:948`：`w[t] = base_w[t] * huber_w(z, cfg.huber_delta)` —— 只降权，永不为 0。
- `sky_plane.cpp:949`：`if (std::fabs(z) > 5.0) ++nrej;` —— `nrej` 是**迭代内局部死变量**，从未使用。
- `sky_plane.cpp:1052-1057`：事后诊断循环统计 `|z|>5` 写入 `info.n_rejected`（:1057）——**只是计数**，样本仍以 Huber 权重留在法方程。
- `used` 集合在迭代前一次性固定（`sky_plane.cpp:446-461`），迭代中不增删；`P2_SKY_FLAG_REJECTED` 只作为**输入**掩膜被读取（:451-453），build 从不写出该 flag。
- `p2_sky_plane_residuals`（`sky_plane.cpp:1225-1253`）用**基础权重**复算 RMS，不含 Huber/剔除 ⇒ 其 `rms_weighted` 不是稳健拟合判据。
- 设计原文 `docs/plugins/algorithms_phase2/11_upm.md:63`：「污染点由稳健迭代（M 估计/σ-clipping）进一步降权，硬污染交 rejection 裁决」——即**软降权是文档口径**；负责人要求的「用 RMS 判据把异常采样点剔除」是更强的硬剔除。

**最小改法（建议，本轮未实现）**：
1. 在 IRLS 残差循环（`sky_plane.cpp:933-950`）内，除 Huber 外增加硬门：
   `if (iter >= 1 && std::fabs(z) > z_hard) w[t] = 0.0;`（`z_hard` 建议复用 5.0，即与 `:949` 诊断门一致；首轮不剔除以让模型先成形）。
2. 因零权等价于剔除，须防「节点/帧支撑塌缩」：`node_weight`/`support_floor` 只在 `:611-638` 计算一次；建议每轮统计每帧零权后剩余点数，若 `< cfg.min_samples_per_frame` 则按 `P2_SKY_PLANE_FRAME_UNDERDETERMINED` fail-closed（不静默）。
3. 收敛后把真正被零权的样本数写入 `info.n_rejected`（替换 :1052-1057 的纯诊断计数），并在 `p2_sky_plane_residuals` 里同样跳过 `w==0` 的样本，使 `rms_weighted` 反映剔除后的拟合。
4. 需要新增/复用配置项（如 `reject_sigma`、`reject_min_iter`），**会改变默认科学行为**，属合同级变更，须负责人批准后再做。

---

## 9. 测试影响

- **未新增测试**（本轮只做接线，构建/测试由前台执行）。
- 现有 `p2_sky_plane_eval` / `eval_block` / save-open roundtrip / `v6_p2_sky` 用例：**不受影响**（eval 本体未改；新入口是新增符号）。
- 全链用例（`tests/unit/p2001_real_nodes_test.cpp`、`p2002_unc_rej_prov_test.cpp` 等）会经过 `upm-apply`：若该 fixture 的 `p2_sky_plane.bin` 存在，`corrected` 由 `raw−C−b` 变为 `raw−C−δ`。由于两者只差公共面 `B_ref`，**帧间残差类断言（如 `mean_after < 5.0`）预期不变**；但涉及绝对电平的断言需前台复跑确认。
- `p2_corrected.json` 新增 `sky_plane_mode` 键（纯增量 provenance）；若存在对 artifact 键集的严格白名单校验，需前台确认（本仓未见 `DATA-P2-COR` 的 JSON-Schema 强校验）。
- **建议前台**：`ninja -C build` 后跑 `ctest`（尤其 `v6_p2_sky`、`p2001`、`p2002`），并在 L4 复跑接缝/负像素统计。

---

## 10. 遗留 / 上呈

1. **接缝与方案 B 无因果**（§5）：若负责人目标是消除接缝，方案 B 不能达成，需另立任务在**帧间差**上设计。
2. **`C_k` 与 `B_ref` 的关系**：生产公式是 `raw − C_k − δ_k`。裁决文字只写了 `raw − δ_k`。本次按「`p2_upm_calibrate_block` 语义不动」保留 `−C_k`。若负责人意图是连 `C_k` 一并去掉，请明确（那会改变 W2 UPM 校正语义，超出本任务）。
3. **硬剔除离群样本**（§8）：最小改法已给出，待负责人确认后实施（涉及默认科学行为/合同）。
4. **`g_k`**：本任务不接（§6）；生产链目前无 `g_k` 数据面。

---

### 附：证据文件
- `run/RELEASE-02/fix-gk/gk_b_oracle.cpp` — Oracle 源码
- `run/RELEASE-02/fix-gk/oracle_output.txt` — Oracle 输出（ORACLE_PASS）
- `run/RELEASE-02/fix-gk/oracle_build.log` — Oracle 构建日志
- `run/RELEASE-02/fix-gk/syntax_sky_plane.log`、`syntax_stage2.log`、`syntax_module_adapters.log` — 三文件 `-fsyntax-only` 日志（均空 = 0 错）
