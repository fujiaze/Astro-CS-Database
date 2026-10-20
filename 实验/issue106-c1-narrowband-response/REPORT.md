# Issue #106（C1）参数化窄带响应生产实施 — 证据报告

- 任务：[#106 ACSD-CPP/C1] Gaia XP 生产窄带参数化与精确响应积分
- 上级：[#105 总任务板]；科学设计：[#104 已审核]；真实数据节点：[#103]
- 实施基线：`ba81a1631e006ccb4fe2684d2e1356c2e293213a`（= 本轮起点 main）
- 环境：Linux x86_64、g++ 14.2.0、CMake 3.31.6、Makefiles、16 核；NumPy 2.2.4、SciPy 1.15.3
- 数据：仓内 `gaia/GaiaDR3SP`（真实 Gaia DR3 XP 光谱库）、`config/response_curves`（58 条真实曲线，含 `KAF-16803` QE 与 `Baader_7nm_H_alpha`）
- **无用户真实 FITS/HST** ⇒ 本报告只覆盖数学、合成数据与可执行 C++ 验证；真实数据验收属 #103

## 1. 改动范围（生产路径接线）

| 文件 | 改动 |
|---|---|
| `lib/algorithms/photometry/cpp/src/spectrum_integrator.h/.cpp` | 新增参数化响应的**边界精确分段求积**（`make_parametric_band` / `prepare_segmented_response_cache` / `compute_f_syn_segmented_xpsd`）；Akima 重构为 `AkimaSpline`（`akima_interpolate` 行为**逐位不变**，A/B 见 §3） |
| `lib/algorithms/photometry/cpp/src/pc_api_qf.h` / `pc_api.cpp` | C++ 链接期入口追加可选 `ParametricBand*`；6 个冻结 C 导出签名/行为不变（传 nullptr） |
| `lib/algorithms/photometry/cpp/src/frame_photometry_fit.h/.cpp` | `ResponseSpec`/`ResponseIdentity`；按 mode 分发（参数化**不查滤镜库**、QE 仍强制）；响应身份与覆盖状态 |
| `lib/algorithms/photometry/cpp/src/filter_curve_json.h` | 曲线**有序样本内容** SHA-256（`curve_content_sha256`），替代 n/sum/sumsq 作为身份 |
| `lib/infrastructure/scheduler/src/photometry_response_config.h`（新） | 响应来源 one-of 的**站点解析唯一实现**（可单测） |
| `lib/infrastructure/scheduler/src/module_adapters.cpp` | `p1_op_photometry` / `p1_resolve_frame_binding`：配置解析、逐帧绑定、`RESPONSE_SOURCE_CONFLICT`、provenance |
| `lib/infrastructure/scheduler/src/normalize_precision.cpp` + `lib/include/acsd/normalize_precision.h` | 帧侧 `photometry.response_id` / `application_id` 纳入响应 id（空 ⇒ 旧 measured 规范串**逐字不变**） |
| `eng/contracts/schemas/phase_config_normalize.schema.json` | `photometry.fit.response` + `photometry.frame_overrides[].response`（tagged one-of） |
| `CMakeLists.txt` | `acsd_phase1_photcal` 链接 `acsd_common`（SHA-256 单一实现） |
| `docs/`（4 个正本/工程文档） | 只更新**接线状态**与配置口径，不改科学定义 |

生产调用链：
`p1_op_photometry` → `frame_photometry_fit::fit_frame_photometry` → `pc_calibrate_simple_with_gaia_f64_v2_qf` → `run_with_gaia_impl<T>` → `compute_f_syn_segmented_xpsd`（参数化）/ `compute_f_syn_cached_xpsd`（measured）。

## 2. 数学依据

- F_syn 定义不变：`F_syn = ∫ F(λ)·T(λ)·Q(λ)·λ dλ`（XPMODEL 原生积分单位，**不除 hc**）。
- 参数化窄带：`T(λ)=1` 当 `c−Δ/2 ≤ λ ≤ c+Δ/2`，否则 0；峰值归一为 1、FWHM=Δ（PHOTOMETRY.md §2.1）。
- 求积：断点 = 解析端点 ∪ XP 采样点 ∪ QE 插值节点；每段 4 点 Gauss–Legendre。
  **精确性**：Akima 在节点区间内是三次多项式 ⇒ 段内 `F(λ)·Q(λ)·λ` 是 ≤7 次多项式；4 点 GL 对 ≤2n−1=7 次多项式精确 ⇒ **求积误差 = 0**（仅浮点舍入）。
- 误差分账：求积误差（=0）｜XP 重构误差（Akima）｜XP 自身分辨率/外定标｜**响应形状误差**（top-hat 是具名假设，PARAMETRIC_APPROX ≠ MEASURED_CURVE）。

## 3. 独立 Oracle 与数值对拍

| 对拍 | 参照实现 | 结果 |
|---|---|---|
| Akima 重构无回归 | 重构前实现逐字拷贝（测试内） | **200/200 随机 + 边界用例逐位相等** |
| 分段求积（闭式解） | 独立解析积分（二次 F × 线性 Q ⇒ 四次被积函数） | 相对误差 **1.16e-16** |
| 分段求积（SciPy） | `scipy.interpolate.Akima1DInterpolator` + `scipy.integrate.quad` | 3/5/7 nm Hα（真实 KAF-16803 QE）相对误差 **≤ 5.0e-16** |
| 生产路径（真实 XP） | 172 颗**真实 Gaia DR3 XP 光谱星** | 生产 F_syn vs SciPy 复算 **max rel = 5.045e-16**（median 1.19e-16） |
| 生产自洽 | 注入 `F_instr = F_syn·c`（c=1e-3） | 恢复 `k_photo = 1/c`，相对误差 **0.000e+00**，残差散度 **0.000e+00** |

**#43 反例（可复现）**：同一 top-hat 走历史 2 nm 粗格路径 vs 精确值 ⇒ 3 nm **55.6%**、5 nm **33.3%**、7 nm **4.8%**；且 5 nm 与 7 nm 在粗格路径上给出**完全相同**的 F_syn（`3.4986666667e-03`），而精确值相差 40%。真实 `Baader_7nm_H_alpha` 曲线的粗格路径误差 **1.10%**。
生产路径同输入对比：参数化 `k_photo=2.040202e-14` vs measured 等价矩形 `k_photo=9.082279e-15`，比值 2.2464，与逐星 F_syn 比的中位数倒数 2.2468 一致 ⇒ 响应确实进入了生产 F_syn 计算。

## 4. 执行命令与退出码

```
cmake --build build/linux-control --target acsd -j 16            # 0
python3 -m eng.tests.unit.run_unit                                 # 0（160/160）
python3 eng/tests/unit/run_simpson_integrator_domain_test.py       # 0（62/62）
python3 eng/tests/unit/run_photo_identity_test.py --build-dir build/linux-control  # 0
python3 eng/tests/unit/run_narrowband_response_test.py             # 0（252/252）
python3 eng/tests/unit/narrowband_response_oracle.py               # 0
python3 eng/tests/unit/run_narrowband_production_path_test.py      # 0（16/16）
python3 eng/tests/unit/narrowband_production_oracle.py             # 0
python3 eng/tests/unit/narrowband_response_config_test.py          # 0（15/15）
python3 eng/tests/unit/run_photometry_response_config_test.py     # 0（40/40）
```

日志见 `evidence/`。

## 5. 回归与性能

- measured 路径**逐位不变**（Akima A/B 200/200；`simpson_integrator_domain_test` 62/62；既有 160 项单元集全绿）。
- 既有产品身份**逐字不变**：`photoapplied:v2:455519fe8018488ad86ff8df6c47952b`（空 response_id ⇒ 规范串不变）。
- 性能（同一批 343 点 XP 谱，2000 次重复，`--bench` 模式）：参数化分段路径 **4.45–4.53 µs/星**，
  历史 measured 路径 **3.41–3.46 µs/星** ⇒ 开销约 **1.3×**（≈1.1 µs/星，来自逐星重建 f_λ 的
  Akima 样条；3 nm 带只切 2 段）。5000 星/帧 ⇒ 每帧多约 5 ms，对 Normalize 无实际影响。

## 6. 仍未闭合（不宣称已验证）

1. **响应形状误差**：top-hat 是具名工程近似；OD5+ 不证明带内为矩形；快光锥蓝移/温漂不可由 c/Δ 恢复。
2. **实测曲线的 2 nm 粗格求积误差**（#43）**仍 OPEN**：本轮只对参数化模式给精确路径。
3. **XP 覆盖/截断**已发布状态（full/xp_truncated/qe_truncated/no_overlap），但 XP 域外真实通量/外定标误差未建模。
4. **窄线谱（Hα/[N II]）对未知星云 SED 的不可消除偏差**（PHOTOMETRY.md §3.1）不因本轮改变。
5. **真实 FITS/HST/M42 全链**属 #103；本报告**不**代表真实数据科学验收。

## 7. 证据文件

`evidence/` 内为各命令的原始输出（文本，可复算）。逐星生产数据（真实 XP 光谱字节 + 两条 F_syn）不入库，脚本按需重生成（`run_narrowband_production_path_test.py` → `/tmp/narrowband_production_path.json`）。
