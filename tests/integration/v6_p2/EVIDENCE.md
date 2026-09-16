# EVIDENCE — P2-INTEGRATE-001 (Wave 8, V6 并行包)

- 任务卡：`工程控制/AstroCS_PARALLEL_SCIENCE_IMPLEMENTATION_V6_20260915/tasks/P2-INTEGRATE-001.md`
- 基线 HEAD：`960d6051731e817a1e7b26330b3716378f3e2a48`（`git rev-parse HEAD` 实测）
- 写域：`lib/phase2_int/`、`tests/integration/v6_p2/`（tracked 树零越界修改，见 §5）
- 运行产物/日志：`run/v6/P2-INTEGRATE-001/`（gitignore）

## 1. 构建（真实编译，均带 timeout）

| 步骤 | 命令 | rc | 日志 |
|---|---|---|---|
| 库配置 | `cmake -S lib/phase2_int/v6 -B run/v6/P2-INTEGRATE-001/build_lib -DCMAKE_BUILD_TYPE=Release` | 0 | `logs/01_configure_lib.log` |
| 库构建 | `cmake --build .../build_lib -j4` | 0 | `logs/02_build_lib.log` |
| 集成测试配置 | `cmake -S tests/integration/v6_p2 -B run/v6/P2-INTEGRATE-001/build_it -DCMAKE_BUILD_TYPE=Release` | 0 | `logs/03_configure_it.log` |
| 集成测试构建 | `cmake --build .../build_it -j4` | 0 | `logs/13_build_final.log` |

编译源集 = 本层接线 + Wave 5 生产源（`lib/algorithms/coverage/src/{upm,rejection,sampler,coverage}.cpp`）
+ P1-INTEGRATE-001 消费面 + IMPL-AIO-001 + IMPL-P1-PSFW-001 + IMPL-P1-CAL-001 + IMPL-P1-DRZ-001
+ legacy AIO 后端（`aio_hips_reader.cpp`/`aio_upm.cpp` + vendored cfitsio）。**未修改上述任何被链接源。**

## 2. 测试（真实运行，命令带 timeout，检查退出码）

```bash
ctest --test-dir run/v6/P2-INTEGRATE-001/build_it --output-on-failure -j1   # rc=0
```

| ctest | 结果 | 用例内 checks |
|---|---|---|
| `v6_p2_routing` | PASS | 11 |
| `v6_p2_write` | PASS | 50 |
| `v6_p2_negative` | PASS | 21（负向 18/18 检出） |
| `v6_p2_halfproduct` | PASS | 5 |
| `v6_p2_oracle` | PASS | 15 |
| **合计** | **5/5 PASS, 0 skipped** | **102 checks** |

日志：`logs/14_ctest_final.log`（ctest）、`logs/15_groups.log`（逐组 stdout）。

## 3. 覆盖的冻结节点

| 节点 | 证据 |
|---|---|
| FZ-MODE-PRODUCTION / -DEFERRED / FZ-FIELD-WEIGHTMODE | routing：三模式接受；`psf_snr_power`/`auto`/`support_x_snr2`/`0`/未知 REJECT；`equal`/`pixel_ivar` baseline |
| FZ-FORMULA-Q/WINFO/FHAT | point：`F=Q/W`、`Var·W=1`、独立 `Q=ΣQ_k,W=ΣW_k`；Oracle 独立复算 rel<1e-9 |
| FZ-FORMULA-QW-04 / GATE-CORR-04 / FZ-AP2PT-CORR-RATIO-MIN | point_joint：联合 C 使用；ratio=1.3≥1.05；相关帧朴素 Σ 被 REJECT |
| FZ-FORMULA-GLS / FZ-AP2S-IDENT-RTOL(1e-9) | surface：`x_hat`/`Cov=(AᵀC⁻¹A)⁻¹`；`R C_in Rᵀ` 恒等式内部校验；Oracle 独立复算 |
| FZ-GATE-PIXIVAR-APPROX / FZ-AP2S-EPS-PIXIVAR(0.05) / -SUP(0.20) | surface：p05/p50/p95 报告；对角正例 rho=1 过门；相关噪声按独立 → 门红；a_k 不一致结构性 REJECT |
| FZ-FORMULA-PSFSW-COMPOSITE / FZ-FIELD-PSFSW-UNIT / 4COMP | psfsw：磁盘重开组内 median=1；canonical `weight.{kind,units,group_normalized,normalization.*}`；四分量单位 |
| FZ-FORMULA-COV-PROP / FZ-GATE-PSFSW-COV / RULINGS #5 | psfsw：`C_out=alpha^T C_in alpha`；`variance_from=actual_combination_coefficients`；`1/W_psfsw` 未用；边界 gate |
| FZ-GATE-PSFSW-EPSF / FZ-AP2S-EPSF-RTOL | 三模式 effective PSF 必输；归一约定声明；FWHM 由 P_eff 测量（Oracle 独立复现）；只给 FWHM 标量 → REJECT |
| FZ-GATE-MEDIAN-SNR / FZ-GATE-SUPPORT-COVERAGE | 权重来源门（`p2_weight_source_token_reject`、`p2_rejection_weight_surface_guard`）；记录重开扫禁 token |
| FZ-PROV-MINIMAL-SET / FZ-BUNIT-SEMANTICS / FZ-P3-BUNIT-QUADRATIC | provenance 最小集门；BUNIT 可判 + 二次律；`output_hash`=磁盘实际 sha256 |
| FZ-GATE-PSFSW-FAILCLOSED / PSFSW-T-NMIN | 单帧不足 / n_common<3 → fail-closed（测试注入） |
| FZ-AP2S-RANK-RTOL / KAPPA-MAX / FZ-AP2S-UPM-MINFRAMES | wiring：`p2_upm_ma_build` rank==n_free、kappa≤1e6、min_frames=2 |
| ALG-P2S-REJ.3 σ_eff² | wiring：`σ_eff²=σ_p1²+J C_θ Jᵀ`，`p2_reject_classify` + 继承阈值门 |
| FZ-DEGRADE-SCALAR | wiring：空间摘要 p05/p50/p95 + 双门 `p2_scalar_degrade_gate`=ALLOWED |
| OI-01 group_normalization_deferred_to=phase2 | 重开校验；组内归一在 Phase2 完成 |
| P33 撤销（C-004.2） | 记录重开 P33 键守卫；mutation `snr_frame_coefficient` 必红；源内无 `support_x_snr*` |

## 4. 负向用例（全部实测变红）

记录 mutation（重开必红）：`weight_mode=psf_snr_power`、`weight.kind=ivar`、
`variance_from=psfsw_robust_weight`、`psfsw_boundary.variance_from_weight`、
`effective_psf` 只剩 FWHM、`provenance` 删键、`snr_frame_coefficient` 重引入、
`support` 进 weight.sources、第三套词表 token、BUNIT 不可判、`output_hash` 改、`Var·W≠1`。

运行/输入 mutation：FITS bit flip、相关帧朴素 Σ、pixel-ivar 相关噪声（epsilon 门）、
pixel-ivar 结构性 a_k 不一致、无误差门声明、单帧不足。**合计 18/18 检出。**

`v6_p2_halfproduct`：发布后重开验证注入失败 / BUNIT 不可判 → 目标不存在、无 `.staging.tmp-` 残留。

## 5. 独立 Oracle（不调用被测实现）

`oracle/v6_p2_oracle.py` 用 NumPy/Astropy 从磁盘 FITS + 输入原值独立重算：
point 独立合并、point 联合 GLS、surface GLS、psfsw conventional coadd 与 `C_out`；
并做结构门（output_hash = 实际 sha256、BUNIT 二次律、provenance 最小集、权重面禁项、P33）、
以及 **non-vacuity**（对副本注入 `variance_from=weight` 与 P33 键 → 检查器自身变红）。15 checks PASS。

## 6. 越界审计

`git status --porcelain -- lib/phase2_int lib/algorithms/coverage tests/integration/v6_p2` 仅显示两个新增目录；
`git diff --stat -- lib/algorithms/coverage lib/phase1 lib/infrastructure/aio contracts docs` 为空（零 tracked 修改）。
未 commit / 未 push / 未建分支或 worktree / 未派生子代理。

## 7. 未决 / 移交

1. **AR-033 构建面**：`astrocs_v6_phase2_integrate` 与 `tests/integration/v6_p2` 未注册进根/公共
   CMakeLists（`cmake -S ... -B ...` 自包含可独立构建）。按 C-004.4 移交 RUNTIME-CI-001/W9。
2. **session 接线**：`lib/phase2_session/` 未改（其根构建 target 不在本任务可安全接线范围），
   V6 三模式入口待 W9 在 session/runtime 层挂载。
3. **相关帧联合 C 的数据面缺口**：Phase1 单帧产品未持久化逐像素 `d_k`/`C_k`，故联合 GLS 的
   `(K*m)` 原始量由调用方提供（本层校验 `A=a_kP_k` 与磁盘 psf_profile 一致后才接受）；
   若要纯磁盘重放联合 GLS，需 P1 产品或会话提供逐像素输入（登记为接口缺口，非本任务改动）。
4. **FWHM 定义**：记录 FWHM 由 p1psfw `fwhm_from_half`（围绕全局极大的半高线性插值）；Oracle
   独立实现同一定义并逐值一致（rel<1e-9）。
