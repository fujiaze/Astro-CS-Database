# RELEASE-02 FIX-REGRESS 修复回归分片报告

> 执行面：**FIX-REGRESS（修复 2 个失败用例）**。零 git 写权限；未跑 ninja/cmake/ctest。
> 产物：`run/RELEASE-02/fix-regress/`（probe/matrix/oracle/gate 复现程序与日志）。
> 报告：本文件。环境：`TMPDIR=/dev/shm/astrocs_regress`（结束已清理）。

---

## 0 结论速览

| 项 | 结论 |
|---|---|
| 失败1 `p2001_real_nodes:445` | **根因 = P2a-4 `m_full_frame=1` 的 M 冷启动 + `C_ref≡0` gauge + Huber 自锁**；gauge/tolerance/additive_mode 均非原因 |
| 修复前 | `mean_after=448.018351`，`converged=0`，`iterations=100` |
| 修复后 | `mean_after=2.714521`，`converged=1`，objective 37.44 |
| 失败2 `p1001_real_nodes:1226 (B2-A14b)` | **遗留夹具**：声明 `applied=true` 未写 `photoapplied_light_1.fits`；生产 fail-closed 正确 |
| 两个测试 | 均 **PASS**（真实测试二进制重链后实跑） |
| P2a Oracle | **仍 PASS**（legacy 行逐位不变；P2a-full 比值 0.958→0.945，lam0 conv 25→19） |
| 其他遗留夹具 | 全仓仅 B2-A14b；`ci/fixtures/provenance/applied_example.json` 是 checker 样例非 drizzle 输入 |

---

## 1 失败1 根因与逐项定位

### 1.1 定位方法

`ninja/cmake/ctest` 被禁，故用 g++ 直接重编译 `tests/unit/p2001_real_nodes_test.cpp` 为探针（仅把
`upm-fit` 配置改为读环境变量 `PROBE_UPM` 注入逐项开关，并打印 `mean_after`），链接**真实** `upm.cpp`
与全部生产库；`build_probe.sh`/`build_probe_mod.sh` 记录编译链接命令。

### 1.2 逐项数值表（2 帧、帧2 +1000 ADU（=fixture 的 +10 signal/area）、63 有观测 control + 1 无观测角 cell）

| 配置 | converged | objective | mean_after | min b1 | 判定 |
|---|---|---|---|---|---|
| 生产基线 `damp.5 full1 gauge1 rel1e-3` | 0 | 14548.92 | **448.018351** | 10154.9 | 红 |
| `m_full_frame=0` | 1 | 8.978 | 5.187505 | 10275.8 | 仍红（>5，退化角） |
| `gs_damping=1.0`（ff 仍 1） | 0 | 11509.04 | 354.957 | 10180.3 | 红 |
| `final_gauge=0` | 0 | 14548.92 | 448.018351 | 10155.8 | **无变化** |
| `tolerance_relative=0,tol=1e-6` | 0 | 14548.92 | 448.018351 | 10154.9 | **无变化** |
| `m_full_frame=0 + gs_damping=1.0` | 1 | 0.000 | **4.215059** | 10275.6 | 绿（legacy 口径） |
| `m_full_frame=0 + gs_damping=1.0 + gauge0 + rel0` | 1 | 0.000 | 4.215059 | 10275.6 | 绿 |
| `m_full_frame=1 + gs_damping=1.0` | 0 | 11509.04 | 354.957 | 10180.3 | 红 |
| `m_full_frame=1 + final_gauge=0` | 0 | 14548.92 | 448.018351 | 10155.8 | 红 |

**读表**：
1. **`m_full_frame=1` 是主因**（448→5.19）；`gs_damping` 次之（ff=1 时 448→355，ff=0 时 5.19→4.22）。
2. **`final_gauge` 与 `tolerance_relative` 对帧间残差零影响**（gauge 对全部帧同一扣除，在差值中抵消）。
3. 即使 `m_full_frame=0`，`damp.5` 仍留 5.1875（>5）：角区**无观测 control 的 C≡0 假台阶**（≈4.2 ADU）
   + 阻尼/相对容差早停的体残差（≈1 ADU）。

### 1.3 代数根因（448 的来历）

UPM 模型 `y_{f,k}=M_k+C_{f,k}`，分量 gauge 固定 `C_ref≡0`。joint-LS 不动点：
`M_k=y_ref`、`C_{f,k}=y_{f,k}-y_ref`、全部残差=0。full-frame M 更新
`M_k=Σ_f w_f(y_f-C_f)/Σw_f` 在收敛后与 legacy 同解（覆盖帧的参考帧分支），
**唯一作用是收敛路径**。

Huber 权 `w=raw·huber_w(r/σ)`，`σ=1.293`。冷启动 `M=0` 时参考帧残差
`r_ref=y_ref-M=11020`（≈8500σ）被 Huber 判为离群，`w_ref` 塌缩到 `~1.7e-3`，
而非参考帧经 C 吸收后 `r→0`、`w→raw=0.5`。带阻尼 Gauss-Seidel 的收缩因子
`c=0.5(1+w_nonref/W)→0.9983`（w_ref≪w_nonref），M 以 ~1/300 速率漂移：
100 轮后仍 `M≈10575`、`C1≈-554`，帧间残差 `|1000-|C1||=448.018`，且
`max_dC≈0.87 > tol_C=1e-3·max|C|≈0.55` ⇒ `converged=0`。

角区 ~4.2 ADU 是**第二个独立缺陷**：`build_geo` 的全 coverage 含 1 个无观测
（单帧区/无覆盖）角 cell，`obs_idx` 为空 ⇒ `M=C=0`；双线性插值把相邻覆盖区
从 `C≈-1000` 拉向假 0，制造角区假台阶。此缺陷在 legacy 已存在（4.215）。

> 结论：**不是 gauge 算错、不是 calibrate 扣错、也不是 `additive_mode=c` 不成立**
> （本场景 `sky_plane` build 回退 ⇒ 无 δ 可扣，`additive_mode` 对残差零影响；
> `final_gauge` 开关亦对残差零影响）。

---

## 2 修复（`lib/algorithms/coverage/src/upm.cpp`）

### 2.1 P2a-4 收敛修正：gauge 一致的 M 初值（仅 `m_full_frame=1`）

在 IRLS 前用**分量参考帧观测均值**初始化 `M_k`（即 legacy 的 gauge 一致解），
使 `r_ref≈0`、Huber 权重首轮即对称。full-frame 加权不动点不变，只修收敛路径。
`m_full_frame=0`（W2 冻结 legacy）不进入该分支 ⇒ 逐位不变（Oracle legacy 行未动）。

### 2.2 退化场景：无观测几何节点的调和延拓（SCI harmonic continuation）

IRLS 后对无观测节点（`obs_idx` 为空）用**观测邻居**做 Jacobi 调和平均，延拓 `M` 与每帧 `C`。
`λs>0` 时由 CG 平滑隐式完成；`λs=0`（生产缺省）下此路径不可达，故显式补齐。
只改写无观测节点，有观测节点逐位不变；孤立（无观测邻居）节点保持 0（模型域外仍=0，
不改变“域外无校正”契约）。

### 2.3 逐项修复效果（同一探针，链接修改后 `upm.cpp`）

| 配置 | converged | objective | mean_after | 判定 |
|---|---|---|---|---|
| 生产基线（ff=1） | 1 | 37.439 | **2.714521** | 绿 |
| `m_full_frame=0` | 1 | 8.978 | **0.976562** | 绿 |

`min b1` 由 10154.9/10275.8 回到 10996.8/10999.5 ⇒ 角区假 0 已消除。

---

## 3 失败2：p1001 B2-A14b 遗留夹具

`tests/unit/p1001_real_nodes_test.cpp` B2-A14b 只写标量 `photscal=0.5`，未写
`photoapplied_light_1.fits`；生产 `module_adapters.cpp:3881-3888` fail-closed 正确
（`applied=true` 而无逐帧产物 ⇒ DATA 错误，绝不静默退回未测光 ADU）。

**修复**：夹具新增 `ScaledStarField` + `scaled_star_field_pixel`（原星场 × photscal 的副本，
等价 `calibration::apply_photometry` 的逐像素乘性结果），真实写出
`output_dir/photoapplied_light_1.fits`。**未放宽任何生产校验。**

- B2-A14a（无 provenance ⇒ PHOTAPPL=0/BUNIT=ADU）：**仍绿**（修改后全测试 PASS）。
- 全仓搜 `photometry_applied.*true`：仅本夹具（已修）+ `ci/fixtures/provenance/applied_example.json`
  （checker 样例，非 drizzle 输入）；drizzle 单测的 `photometry_applied_upstream=true` 走 C++ 引擎
  配置直连，不经 provenance/photoapplied 路径，无需产物。

---

## 4 判别力（能红能绿）

### 4.1 失败1 测试
- 红：原二进制 `build/tests/unit/p2001_real_nodes_test` → `CHECK failed :445 mean_after=448.018351`（`logs/p2001_before.log`）。
- 绿：修改后 `upm.cpp` 重链真实测试 → `P2-001 REAL NODES PASS`（`logs/p2001_after.log`）。

### 4.2 新增退化场景负例 `Phase2Upm.Release02UnobservedNodeHarmonicContinuation`
`lib/algorithms/coverage/tests/synthetic_gate.cpp` 文件尾：64 个几何 control，角 cell 无观测。
- 红（旧 `upm.cpp`）：`c_unobs=0` vs `c_nb=1000`（差 1000），帧间残差 1000。
- 绿（新 `upm.cpp`）：延拓 `c_unobs==c_nb`，帧间残差 <5。

### 4.3 回归
- `phase2_synthetic_gate --gtest_filter=Phase2Upm.*`：**34/34 PASS**（含新负例；`logs/gate_new_green.log`）。
- 旧 gate 二进制同套 33/33 PASS，确认 legacy 路径未动。

---

## 5 P2a Oracle（修复后仍 PASS）

`run/RELEASE-02/fix-regress/oracle_out_after.txt`（链接修改后真实 `upm.cpp`）：

```
config                            iter conv   objective    boundary%    internal%    ratio
legacy refM noG @lam1000            60    0       27.99      0.3386%      0.4091%    0.828   (逐位不变)
full_frame noG @lam1000             60    0       24.02      0.3465%      0.4091%    0.847   (24.01→24.02)
refM final_gauge @lam1000           60    0       27.99      0.4824%      0.4374%    1.103   (逐位不变)
P2a-full rel1e-3 @lam1000           60    0       24.46      0.3867%      0.4091%    0.945   (0.958→0.945)
legacy refM noG @lam0               60    0   0.0008331      1.0698%      0.4521%    2.367   (逐位不变)
P2a-full rel1e-3 @lam0              19    1    0.002239      1.0717%      0.4521%    2.371   (iter 25→19)
  raw=6.4904%  raw-delta=5.0762%  raw-C=0.0130%  raw-C-delta=1.4013%   (逐位不变)
```

P2a-full 边界/内部比 **0.945**（≤ 修复前 0.958），lam0 `converged=1` 更快；
P2a-1 组合与 legacy 行逐位不变 ⇒ **科学目标未回退**。

---

## 6 科学行为变更清单（供 ACCEPTANCE §3）

| # | 变更 | 依据 | 影响面 | 如何验证 |
|---|---|---|---|---|
| SC-FR-1 | `m_full_frame=1` 时 M 用分量参考帧观测均值**热启动**（不动点不变，仅修收敛路径） | 本报告 §1.3；Q2 §4.2 含自身 formulation 的收敛缺陷 | 仅 `m_full_frame=1` 的模型求解轨迹/结果（生产 upm-fit）；`cfg{}` legacy 逐位不变 | Oracle legacy 行不变；P2a-full 比值 0.945；Phase2Upm 34/34 |
| SC-FR-2 | `build_geo` 无观测几何节点（单帧区/无覆盖）的 `M/C` 由观测邻居**调和延拓**（原为 0） | SCI PHASE2_UPM「单帧区 harmonic continuation」；λs=0 时原路径不可达 | 含无观测 cell 的模型 `C/M`、model_hash、该 cell 邻域的校正输出；有观测节点逐位不变 | 新负例红→绿；p2001 角区 mean\|Δ\| 4.2→0；Phase2Upm 34/34 |
| SC-FR-3（测试） | B2-A14b 夹具真实产出 `photoapplied_light_1.fits` | 生产 fail-closed 契约（module_adapters:3881-3888） | 仅测试夹具；**生产 fail-closed 未改** | p1001 全测试 PASS（红→绿） |

**未改**：`module_adapters.cpp` 生产装配（`tolerance=1e-3/relative/gs_damping=0.5/m_full_frame=1/final_gauge=1`）、
`seam.additive_mode` 默认、任何 fail-closed 校验、`docs/**`。

---

## 7 证据文件

| 文件 | 内容 |
|---|---|
| `run/RELEASE-02/fix-regress/p2001_probe.cpp` | 可注入逐项开关的 p2001 探针源码 |
| `run/RELEASE-02/fix-regress/build_probe.sh` / `build_probe_mod.sh` | 链接旧/新 `upm.cpp` 的探针构建 |
| `run/RELEASE-02/fix-regress/matrix.sh` / `logs/probe_matrix.log` | 逐项开关数值表 |
| `run/RELEASE-02/fix-regress/p2001_real_nodes_test_mod` | 修改后 `upm.cpp` 重链的真实 p2001 测试 |
| `run/RELEASE-02/fix-regress/logs/p2001_before.log` / `p2001_after.log` | 修复前后真实测试日志 |
| `run/RELEASE-02/fix-regress/oracle_out_after.txt` | 修复后 P2a Oracle |
| `run/RELEASE-02/fix-regress/gate_new_red` / `gate_new_green` | 新负例红/绿二进制 |
| `run/RELEASE-02/fix-regress/logs/gate_new_green.log` | Phase2Upm 34/34 |
| `run/RELEASE-02/fix-regress/p1001_real_nodes_test_mod` | 修改后夹具重链的 p1001 测试 |
| `run/RELEASE-02/fix-regress/logs/p1001_before.log` / `p1001_after.log` | 失败2 修复前后日志 |

## 8 未做 / 边界

- 未跑 ninja/cmake/ctest（任务硬约束）；用 g++ 直接重编译/重链验证。
- 未跑全量 464 用例；仅重链并实跑受影响的两用例 + Phase2Upm gate 套件 + P2a Oracle。
- `λs=0` 下公共场 M 的覆盖子集非可辨识性（P2a §3.4 开放项）未处理，仍登记。
- 未清理：`/dev/shm/astrocs_regress` 已删除。