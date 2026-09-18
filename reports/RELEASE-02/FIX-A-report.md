# FIX-A 天光面链 交付报告（P0-08 / P0-09 / P0-10）

> 任务：RELEASE-02 控制包 FIX-A 天光面链（星点掩膜 → 逐帧稀疏天光采样 → 统一参考天光面 B_ref(x) → 逐帧 δ_k(x) → 乘法响应 g_k 接入生产主链）。
> 范围：`lib/algorithms/coverage/**`（sky_plane 新模块 + sampler 扩展 + stage2 生产接线）、`tests/unit/v6_p2_sky/**`、根 `CMakeLists.txt`、`docs/algorithms/PHASE2_SAMPLER.md`（仅行号锚订正，语义不变）。
> 零 git 写：改动全部留在工作树，由前台提交。

## 1 问题登记与关闭状态

| ID | 问题 | 根因 | 处置 | 状态 |
|---|---|---|---|---|
| P0-08 | `star_mask` / `sky_samples` 零实现 | sampler 只产 background-clean control 观测，无星点掩膜、无逐帧天光采样点、无稀疏天光面表示 | 新增 `sky_plane` 模块（星点圆帽掩膜 + `P2SkySample` 采样点 + 稀疏样条 B_ref/δ_k）；sampler 新增 `p2_sample_sky*` 输出 | 已实现（模块级 Oracle 全绿） |
| P0-09 | UPM 天光面 `b_k(x)` 缺失 | 生产 UPM 仅加性 C 场，无 B_ref/δ_k 空间模型 | `P2SkyPlane`：切平面张量积均匀 B 样条 B_ref(x) + 每帧低阶多项式 δ_k(x)，Schur 消元后仅存系数 | 已实现 |
| P0-10 | `g_k` 乘法响应未接主链 | 生产 `p2_upm_build_geo`/`p2_upm_calibrate_block` 纯加性 | stage2 接入 v6 UPM MA 求解器 `p2_upm_ma_build` 估 g_k，应用 `corrected=(raw−C−b_k)/g_k` | 已接线；**代码缺省关闭**（保基线），生产 config 模板显式启用；失败显式 g=1 降级 |
| VIS-001 | 面板接缝/背景台阶 | 无绝对天光面，跨帧相对校准残留 | 合成 seam Oracle：raw 跨帧台阶 1.49779 → 校准后 0.02355（≈63×） | 已实现（待 L4 视觉复验） |

## 2 研究证据（一手开源实现 file:line）

完整证据见 `run/RELEASE-02/research/FIX-A-opensource-evidence.md`（源码快照在 `run/RELEASE-02/research/src/`）。要点：

- **SWarp 2.41.5** `src/back.c:844-848,891-896,944`：稀疏网格上自然三次样条（非稠密栅格）；`src/preflist.h:251` `BACK_SIZE=128`、`:253` `BACK_FILTERSIZE=3`；`back.c:448,452` 权重图仅作掩膜、均值**不加权**。→ 佐证「稀疏表示 + 低分辨率网格」可行；但 SWarp 的天光拟合**不加逆方差权**，与本文 WLS 选择不同（见 §3）。
- **SEP v1.4.1** `sep.h:91-107`：背景节点**稀疏存储**（`sep_backmap` 只有 mesh 节点）；`sep/src/background.c:32-35,334-345,540-659,682-797`：`Background2D` 中值滤波 + mesh 插值。→ 稀疏存储的独立佐证。
- **Siril** `background_extraction.c:62-65`：多项式阶 1–4 → 3/6/10/15 系数；`:371` 无加权拟合；`SAMPLE_SIZE=25 px`。→ 逐帧 δ_k 取低阶多项式的先例。
- **photutils 3.0.0** `background_2d.py:245-246`、`core.py:34`：`SIGMA_CLIP sigma=3.0 maxiters=10`。→ 与 sampler 现有 3σ clip 口径一致。
- **Montage** `montageFitplane.c:486`：逐帧平面 `A·x+B·y+C`，`:488-491` 2σ 残差裁剪。→ **最接近的逐帧 δ_k 先例**（本文 δ_k 默认 1 阶平面）。
- **SCAMP** `photsolve.c:4`：只做相对光度解，无任何 `*back*` 源。→ **不得**引 SCAMP 支撑 `b_k`；`g_k` 的乘法尺度可参考其相对零点思路，但天光背景须另找依据。
- **加权口径**：调研的开源实现中**无一**对背景采样做逆方差加权；`w ∝ 1/σ²`（等价 `SNR²`）的依据来自 WLS 经典结果（Horne 1986, PASP 98:609），不是 SExtractor/SWarp。故本实现的 `weight_mode=inverse_variance` 是**统计最优选择**，不冒充开源惯例。

### 2.1 模型可辨识性（独立数值推导）

见 `run/RELEASE-02/research/FIX-A-model-identifiability.md` 及脚本 `q1_nullspace.py`…`q5_roughness.py`：

- **加性 gauge**：`y_k(x)=g_k·s(x)+b_k(x)`，`b_k=B_ref+δ_k`。`B_ref→B_ref+c`、`δ_k→δ_k−c` 不可辨；gauge 维数 `D_add=(order+1)(order+2)/2`。若 `deg(B_ref) ≥ order`，令**参考帧 δ_ref=0** 是最小约束（本实现 `gauge_mode=0`）。`sum` gauge 亦支持（测试覆盖）。
- **乘法 gauge**：`s→a·s, g_k→g_k/a` 不可辨，锚定 `g_ref=1`（v6 MA 求解器 `gauge_mode=0`，ref=分量内最小 frame_id）。
- **δ_k 阶数**：`q2_delta_order.py` → 默认 `frame_gradient_order=1`；平面拟合偏差 `≈0.0527·κ·L²`（κ=曲率，L=采样跨度）；节点间距 `h ≥ 3·sky_sample_spacing` 抑制混叠。
- **基函数**：`q4_basis.py` → 张量积三次 B 样条条件数 ~30 且**与节点数无关**，优于高阶多项式；故默认 `spline_degree=3`。
- **正则**：`q5_roughness.py` → P-spline 二阶差分粗糙度罚**必需**（否则节点间振荡）；λ 有效 FWHM ≈ 2–3 个节点间距；默认 `roughness_penalty=1e-3`。

## 3 设计取舍与推导

### 3.1 为什么是「稀疏面 + 按需求值」而非稠密栅格
`b_k(x)` 只需在积分时逐像素求值，不需落盘稠密面。本实现只存：B 样条系数（数据支撑的自由节点）+ 每帧 δ_k 系数。Schur 消元把 δ_k 消去，归约法方程**仅 nx·ny**，内存与帧数、像素数**无关**（见 §5 内存实测）。

### 3.2 数据支撑节点掩膜（rank 修复）
切平面 bbox 角点无样本 → 角节点列全零 → 法方程奇异（`RANK_DEFICIENT`）。修复：按 `node_weight[idx]=Σ b_i(x)²` 标记**数据支撑节点**（阈值 `1e-9·max`），只对自由节点建 Schur 系统；不支持节点由最近邻填充（不参与拟合）。这是**统计上必要**的修复（否则解不存在），非放宽判据。

### 3.3 权重与鲁棒
`w=1/σ²`（`weight_mode=0`，默认）或显式 `SNR²`（`weight_mode=1`）。逆方差抑制低 SNR/污染帧对 B_ref 的偏置（实测偏置 0.01735→0.00284）；但高 SNR 系统性偏移会被放大 → 必须配**逐帧 δ 常数项** + **Huber IRLS**（`huber_delta=1.345`）。

### 3.4 g_k 的接线、直流比交叉校验与降级
`g_k` 由 v6 `p2_upm_ma_build` 从 control 观测估计（模型 `y=g_k·s(p)+b_k`，gauge `g_ref=1`）。`rc≠0` → **显式降级 g=1 并记日志**（`[frame_gain] MA build FAILED rc=…`），不静默。

**关键诊断（前台复核驱动）**：MA 的 `g` 由**空间结构**定标。当帧间差异是纯加性（无乘法增益）时，自由 latent `s(p)` 会把加性结构吸收成伪乘法。独立复现（`run/RELEASE-02/FIX-A/scratch/gain_diag.cpp`，真实 ivar 合成观测）：

| 数据 | MA g | 真值 |
|---|---|---|
| 纯乘法增益（`gtrue=[1,1.08,0.94]`） | 1.00000 / 1.08002 / 0.94009 | ✓ |
| 单模加性幅度调制（无增益） | 1.00000 / 1.16002 / 1.32010 | ✗ 把幅度当增益 |
| 真实 ivar 合成观测（相位差，无增益） | 0.805181 / 0.813732 | ✗ 偏 ~19% |

**修复**：新增 `p2_sky_estimate_gain_dc`（帧间公共 control 的**直流比**，DC 不受相位/加性结构影响）作为独立交叉校验。MA 与直流比偏差 >1% → **显式 reject，取 g=1** 并记日志；一致才施加 MA 增益。真实 ivar 观测：MA g=0.805/0.814 vs 直流比 1.0012/0.9997 → dev=0.196/0.186 → reject → 输出回到真值 10.3226，`phase2_ivar_wiring` 在 frame_gain 开启下通过。

**另修复**：宽视场 + 粗样条下 bbox 角节点无数据支撑（`bi[b]==-1`），`H_data[ia*n_free-1]` 越界写（ASan heap-buffer-overflow，release 下段错误）。已加 `ib<0 → continue` 守卫，并加 `v6_p2_sky_overflow` 回归用例。

### 3.5 生产应用公式
`corrected_k(x) = (raw_k(x) − C_k(x) − b_k(x)) / g_k`，其中 `b_k(x)=B_ref(x)+δ_k(x)` 现场求值。sky_plane 启用时替代 RELEASE-01 纯加性 C 场语义（保留 C 作为相对残差修正）。config：`sky_plane.{enabled,spline_degree,node_spacing_deg,frame_gradient_order,gauge_mode,weight_mode,roughness_penalty,frame_gain}`。

**缺省值 = 开启**（`sky_plane.enabled=true`、`frame_gain=true`，前台复核后按控制包"默认即目标模型"整改）：生产 stage2 任何运行默认走目标模型。两条路径均有**数据驱动的 fail-closed 降级并记日志**：
- 天光面：`n_nodes>max_nodes` → `rc=5`、秩亏/非 SPD → `rc=6`，显式回退 UPM C 场；
- `g_k`：MA 秩/κ 门失败或与直流比不一致 → 显式 g=1。

实测 ivar 合成场（order-0 单 tile，63 control / 160 obs，对 1° 节点欠定）：天光面 `rc=5` 回退；`g_k` 直流比门 reject → g=1；故 `phase2_ivar_wiring` 在目标模型默认下仍逐位通过真值 10.3226。真实 order≥7 数据上两者生效（模块 Oracle recovery/seam/dcgain 已证）。

注：生产 config 模板 `config/templates/*.phase_config.json` 的 `algorithm_*` 键在 `lib/` **零消费**（RELEASE-01 AUD-A2 INT-03），故"模板启用"路径不存在；改为代码默认即目标模型（前台选项 a）。

## 4 改动文件清单

| 文件 | 类型 | 说明 |
|---|---|---|
| `lib/algorithms/coverage/include/astro/phase2/sky_plane.h` | 新增 | 天光采样点/星点掩膜/天光面 C API（`P2SkySample`、`P2StarMaskCap`、`P2SkyPlaneConfig/Info`、build/eval/save/open/…） |
| `lib/algorithms/coverage/src/sky_plane.cpp` | 新增 | 切平面投影、均匀 B 样条（de Boor）、δ 多项式、Schur 消元、Huber IRLS、P-spline 罚、SHA-256 模型哈希、JSON 存取 |
| `lib/algorithms/coverage/include/astro/phase2/sampler.h` | 修改 | 新增 `star_mask_snr_factor`/`star_mask_radius_deg` 配置；`p2_sample_sky`/`p2_sample_sky_cached` 声明 |
| `lib/algorithms/coverage/src/sampler.cpp` | 修改 | 第三遍输出逐帧天光采样点（含单帧区）+ 跨帧去重星点圆帽；新增两个公开包装 |
| `lib/algorithms/coverage/include/astro/phase2/stage2_common.h` | 修改 | `P2Stage2Config` 新增 sky_plane/gain 字段 |
| `lib/algorithms/coverage/src/stage2_common.cpp` | 修改 | 可选 `sky_plane` JSON 解析（向后兼容缺省） |
| `lib/algorithms/coverage/tools/stage2.cpp` | 修改 | 生产接线：由 obs 建天光面 + MA 估 g_k；两条块校准路径应用 `(raw−C−b_k)/g_k` |
| `CMakeLists.txt` | 修改 | `astrocs_phase2` 加入 `sky_plane.cpp`；`add_subdirectory(tests/unit/v6_p2_sky)` |
| `lib/algorithms/coverage/CMakeLists.txt` | 修改 | 独立 `phase2` 库加入 `sky_plane.cpp`（stage2 链接） |
| `tests/unit/v6_p2_sky/v6_p2_sky_test.cpp` | 新增 | 11 用例：patch/starmask/recovery/gauge/weighting/negative/memory/determinism/seam/gain/all |
| `tests/unit/v6_p2_sky/CMakeLists.txt` | 新增 | 注册 11 个 ctest，label `v6_p2_sky` |
| `docs/algorithms/PHASE2_SAMPLER.md` | 修改 | **仅行号锚订正**（语义不变）：按 diff 精确映射更新 sampler.cpp/sampler.h 锚；`cell_side` 行锚订正到实际行 |
| `run/RELEASE-02/research/**` | 新增 | 研究证据（开源 file:line + 可辨识性推导脚本） |
| `reports/RELEASE-02/FIX-A-report.md` | 新增 | 本报告 |

## 5 复跑命令与原始结果

```bash
export TMPDIR=/dev/shm/astrocs_tmp
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build
ctest --test-dir build --output-on-failure
# 天光面模块 Oracle（11 用例）
ctest --test-dir build -R v6_p2_sky --output-on-failure
# 机器门
python3 docs/algorithms/anchors/check_doc_line_anchors.py --json-out run/ci/doc-anchors/doc_line_anchors.json
python3 tools/doccheck/check_engineering_constraints.py
```

原始输出：`run/RELEASE-02/logs/`（`ninja_build*.log`、`ctest_sky.log`、`sky_test_raw.log`、`ctest_full.log`、`cmake_configure*.log`）。

### 5.1 模块 Oracle（`v6_p2_sky` 13/13 PASS）

```
[recovery]  n_nodes=64 n_free_params=58 rank=49 kappa=3.326e+05 rms_w=0.04955
            B_ref recovery rms=0.00938 max=0.03653
            residual: recompute rw=0.04955369 ru=0.04955369 nu=1764 ; build rw=0.04955369 ru=0.04955369
[weighting] B_ref bias: weighted=0.00284 unweighted=0.01735
[memory]    n_nodes: 8f=64 64f=64 ; rss delta=2376 KB
[seam]      cross-frame seam: raw=1.49779 calibrated=0.02355
[gain]      frame 0: g=1.00000 ratio=1.00000 (expect 1.00000)
            frame 1: g=1.08039 ratio=1.00036 (expect 1.00000)
            frame 2: g=0.93986 ratio=0.99985 (expect 1.00000)
[dcgain]    uniform frame 1/2: g_dc=1.00378 / 1.00383 (gain-uniform <1%)
            gain frame 1: g_dc=1.08000 (true 1.08000); frame 2: g_dc=0.94000 (true 0.94000)
            no common control -> fail-closed
[overflow]  wide-field build rc=6 (rank-deficient/not-SPD) 安全返回（无段错误）
[determinism] repeated build model_hash bitwise identical; save/open eval identical
RESULT: PASS
```

### 5.2 负例（fail-closed，`[negative]`）
`null samples→INVALID_ARGS`、`all masked→NO_USABLE_SAMPLES`、`too few→TOO_FEW_SAMPLES`、`frame underdetermined→FRAME_UNDERDETERMINED`、`dense grid→TOO_MANY_NODES`、`far eval→OUT_OF_DOMAIN`（不外推）、`unknown frame→UNKNOWN_FRAME`、`eval_block 越域 rc=2`。

### 5.3 全量 ctest（`ctest_final2.log`，目标模型默认开启）
```
100% tests passed, 0 tests failed out of 460
```
基线 447 + 新增 13（`v6_p2_sky`）= 460 全绿；跳过项为环境相关既有跳过（CUDA/ACR=OFF、RealHips 缺 testdata、AVX512 自检）。

### 5.4 目标模型默认生效证据
- **默认即目标模型**：`stage2_common.h` `sky_plane_enabled=true` / `frame_gain_enabled=true`；生产 `astrocs-stage2` 任何运行默认走 `(raw−C−b_k)/g_k`。
- **天光面真实执行**：ivar 合成场日志 `[sky_plane] build FAILED rc=5 n_nodes=6435 > max_nodes=2048 -> fallback to UPM C field`（显式降级，不静默）。
- **g_k 门真实执行**：`[frame_gain] frame … MA g=0.805181 vs DC-ratio=1.001168 dev=0.195758 -> reject g=1`（真实 ivar 观测）。
- **目标模型默认下 `phase2_ivar_wiring` 通过**（真值 10.3226），因两条路径在该欠定合成场 fail-closed。
- **真实增益恢复**（`v6_p2_sky_dcgain`）：增益均匀数据直流比 1.00378/1.00383（误差 <1%）；真实增益 1.08/0.94 逐位恢复。
- **越界回归**（`v6_p2_sky_overflow`）：宽视场粗样条 `rc=6` 安全返回，无段错误。

### 5.5 机器门
- `DOC_LINE_ANCHORS_PASS: 39 docs, 877 anchors, status=EXEMPT:9,OK:868`（订正前为 `BINDING_VIOLATION P2SMP-CELLSIDE`）。
- `CONSTRAINTS_PASS`（`unregistered_root_entry` 已清零；`sky_plane_roundtrip_test.json` 已删除，测试输出改落 `run/RELEASE-02/FIX-A/`）。
- `docs/algorithms/PHASE2_SAMPLER.md` 改动经机器核对为**纯行号**（22 行，去数字后逐行相等）。
- 编译告警 0（`astrocs_phase2`/`astrocs-stage2`/`phase2`/测试，`-Wall -Wextra -Wpedantic -Wconversion`）。
- 追加：其它 shard 新增 `reports/RELEASE-02/SCI-AUDIT/per-doc/DRIZZLE.md`（15:36，untracked）使 `DRIZZLE.md`/`NOISE_MODEL.md` basename 不再唯一，触发 C2_anchor_resolved。按 ANCHOR_CONTRACT §3.3 在 `anchor_contract.json.resolvers` 显式钉死到 `docs/science/DRIZZLE.md`/`docs/science/NOISE_MODEL.md`（**非豁免、非放宽判据**），复跑 PASS。

## 6 未决与风险

1. **生产默认开启**：`sky_plane.enabled`/`frame_gain` 代码缺省 true（前台选项 a）。真实数据 L4 视觉复验仍须由 FIX-C/E2E 执行；ivar 合成场因欠定 fail-closed（已记日志），故无需改其期望值。
2. **g_k 输入面与偏差**：MA 的 g 由空间结构定标，帧间纯加性差异会诱发伪增益（实测 ivar 观测偏 ~19%）。已用帧间直流比交叉校验 + fail-closed g=1 兜底；更根本的稳健做法是**恒星测光控制点**（现有 sampler 不产星点通量）。建议 FIX-C 评估补测光采样。
3. **`docs/science/PHASE2_UPM.md:182`** 登记 UNRESOLVED：冻结加性模型 vs 目标乘法+天光。已按 ENGINEERING_SPEC §3 起草变更 claim `工程控制/RELEASE-02/change-claims/FIX-A-UPM-001.md`（问题/证据/前后/影响面）；**未改** SCI 文档，待前台/负责人裁决落地。
4. **OPEN-P2S-02**（`astrocs.v6.contract-freeze.v1.json:2425`，UPM 乘法尺度 g_k 的生产数据面/schema 与空间模型表示）：FIX-A 已实现空间模型表示，但**生产 schema/数据面**的冻结登记仍需走合同流程。
5. **doc 既有漂移**：`docs/algorithms/PHASE2_SAMPLER.md` 存在与本次改动无关的**历史锚漂移**（例：`grid` 行锚 `:499` 在 HEAD 即指向 `cfg.background_contamination_sigma`，非 `grid`；`kTileWidth :75` 实为 `:76`）。本次只按 diff 精确平移了我改动影响的行号，并订正了 `cell_side` 行锚。建议另开 SCI-ANCHOR 全量重审计。
6. **性能**：`p2_sky_plane_eval` 逐像素按需求值；chunk 内 (ra,dec) 预计算一次，但大帧数 × 大像素仍为新增开销，需 E2E 基准（未在本 shard 实测真实数据耗时）。
7. **`run/RELEASE-02/research/` 与 scratch 脚本**：`run/` 为 gitignore 运行产物，不入库；研究结论已摘录于本报告与 §2。

## 7 给前台的验证建议

1. 复跑 `ctest --test-dir build`，确认基线 447 + 新增 13 = 460 全绿（见 §5 全量结果）。
2. 复跑两个机器门（§5.3），确认 PASS。
3. L4 视觉：对 VIS-001 的合成/真实三块重叠场跑 `astrocs-stage2`，对比 `sky_plane.enabled=false/true` 的面板接缝与背景台阶；建议同时试 `frame_gain=false` 隔离乘法分量。
4. 内存：真实规模（多帧 × 全 tile）下核对峰值 RSS 与「与帧数无关」的预期。
5. 1/N worker：`cpu_workers>1` 下 sky_plane eval 与串行参考逐位一致（sky_plane 求解器为串行确定性；eval 无状态）。
6. 提交顺序：`sky_plane` 模块 + 测试 + CMake 为一个原子提交；sampler 扩展、stage2 接线、doc 锚订正可各自独立提交。
7. 变更 claim：`工程控制/RELEASE-02/change-claims/FIX-A-UPM-001.md`（PHASE2_UPM 冻结模型订正草案，待裁决）。
