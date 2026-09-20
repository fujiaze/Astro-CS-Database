# FIX-P1 报告：Phase1 测光归一化真正接到像素（RELEASE-02）

- 分片：**代码修复分片 / 执行面 = Phase1（测光归一化链）**
- 任务：P1-1（最高优先）把测光归一化接到像素；P1-2 修 `pc_api.cpp` 的
  `quality_flags=nullptr`（饱和过滤失效）；P1-3 登记 `F_instr` 估计量缺陷。
- 日期：2026-09-19。环境：`TMPDIR=/dev/shm/fix-p1`（用完已清）。
- **未跑 `ninja`/`cmake`/`ctest`**（按要求由前台统一构建）；仅 `g++ -fsyntax-only`
  + 两个独立 Oracle（`g++` 编译生产源并运行）。**零 git 写**；未改 `docs/**`。
- 证据目录：`run/RELEASE-02/fix-p1/`。

---

## 0 TL;DR

| 问题 | 结论 |
|---|---|
| 测光是否真施加？ | **是（机制已接线并自证）**：`p1_op_photometry` 在 Drizzle 前对**每一帧**施加 `I_photo = k_photo·I_cal`，写独立产物 `photoapplied_<base>`；`apply_photometry` 首次有生产调用者。 |
| k_photo 从哪来？ | 两个显式来源：① `photometry.fit.enabled=true` → 节点直调**生产 star_matcher 链**（`frame_photometry_fit` → `pc_calibrate_simple_with_gaia_f64_v2_qf`，Tukey-IRLS）；② `p1_photscale.json`（DATA-P1-PHOTSCALE-001，外部/离线标定通道）。两者皆无 → 如实中性，不伪造 1.0。 |
| 元数据是否如实？ | `p1_phot.json` 落 `photometry_applied`/`photscal`/`photscales`（逐帧）/`pixel_scaling`/`photscale_source`/`apply_entry`；`p1_stack.json` 写**逐帧** `photscal` 与 `photappl`；drizzle 消费 `photoapplied_<base>`。 |
| P1-2 饱和过滤 | 新增**非 C ABI** 的质量位感知入口 `pc_calibrate_simple_with_gaia_f64_v2_qf`，生产拟合路径把 `p1_sources[].quality&1`（sdet 饱和位）映射为 `PC_QF_SATURATED` 送达 `cleanAndScale`；6 个冻结导出签名/行为逐行不变。 |
| P1-3 `F_instr` | **仅登记**（涉及冻结科学定义，未擅自改）：现用检测等照度 flux，跨望远镜可达 ~0.5 mag 偏差；建议改固定孔径/PSF 总通量，待前台裁决。 |
| 判别力测试 | Oracle A（施加 vs 不施加，生产尺度 4096²）**PASS**；Oracle B（质量位有/无）**PASS**；p1001 新增节点级红/绿用例（前台 ctest 跑）。 |

---

## 1 改动清单（file:line + 理由）

### 1.1 P1-1 核心：施加机制

| # | 文件:行 | 改动 | 理由 |
|---|---|---|---|
| 1 | `CMakeLists.txt`（`astrocs_calibration` 段） | 把 `lib/algorithms/calibration/src/photometry_apply.cpp` 编入**生产库** `astrocs_calibration`；并把 `lib/algorithms/calibration/src` 加入 PUBLIC include | 修复前该 TU 只在测试 target 编译（`tests/unit/CMakeLists.txt:789-800`），`apply_photometry` 生产**零调用者**（chain-audit §3a）。 |
| 2 | `CMakeLists.txt`（`astrocs_phase1_phot` 之后） | 新增静态库 `astrocs_phase1_photcal`：5 个测光生产 TU（`pc_api/star_matcher/wcs_transform/image_corrector/spectrum_integrator`）+ `frame_photometry_fit.cpp`；链 `astrocs_p1_ipv`（提供 gaia_client 符号） | 修复前 `pc_calibrate_simple*` 依赖**未构建**的 `photometric_calib.dll`，CLI 不可达（chain-audit §3c）。零 diff 编入主图，与 p1phot 测试同一组源。 |
| 3 | `CMakeLists.txt`（`astrocs_module_adapters` 链接） | 链入 `astrocs_phase1_photcal` | 让 Phase1 节点可调用生产星匹配链。 |
| 4 | `lib/infrastructure/scheduler/src/module_adapters.cpp:90-95` | 新增 include：`photometry_apply.h`、`frame_photometry_fit.h` | 生产调用面。 |
| 5 | `module_adapters.cpp:1346-1353` | 新增 `p1_photoapplied_path(doc, light)` = `out_dir/photoapplied_<base>` | 施加产物独立路径，不就地覆写上游 cal 产物（同 CORE-RACE-001 语义）。 |
| 6 | `module_adapters.cpp:3095-3365`（`p1_op_photometry` 尾部） | 新增取 k_photo + 施加逻辑（见 §2）；`p1_phot.json` 如实落元数据 | **本任务核心**：`I_photo = k_photo·I_cal` 在 Drizzle 前施加。 |
| 7 | `module_adapters.cpp:3866-3896`（`p1_op_drizzle` 逐帧循环） | 当 provenance 声明 applied ⇒ 读 `photoapplied_<base>`（缺失即 DATA fail-closed）；查 `photscales` 取逐帧 `frame_photscal` | 让 drizzle 真正消费已归一化像素；声明 applied 而产物缺失不得静默退回未测光 ADU。 |
| 8 | `module_adapters.cpp:3831-3841` | 读 `p1_phot.json.photscales`（逐帧标量，缺省退回标量 `photscal`） | 帧间 k_photo 不同（实测帧间尺度极差 3.53×），单标量不足。 |
| 9 | `module_adapters.cpp:4004,4124,4162` | `fmt(frame_photscal,...)` / `{"photscal", frame_photscal}` / `(*man)["photscal"]=frame_photscal` | `p1_stack.json` 与 HISS 头写**逐帧** PHOTSCAL。 |
| 10 | `lib/algorithms/photometry/cpp/src/frame_photometry_fit.h/.cpp`（新增） | 文件无关的装配入口 `astrocs::photometry::fit_frame_photometry`：滤光片/QE 曲线加载 + gaia_client 创建 + 光谱网格 + FOV 半径 + 调 `_qf` | 把"装配"封在测光模块内，节点只调一个函数；与 `orchestrator::run_stage_photometric` 同源同口径（同 C API/映射/FOV 公式）。 |
| 11 | `lib/algorithms/photometry/cpp/src/pc_api_qf.h`（新增） | 声明**非 C 导出**的 `pc_calibrate_simple_with_gaia_f64_v2_qf` | P1-2；不破坏 ABI 冻结。 |

### 1.2 P1-2 质量位（饱和过滤）

| # | 文件:行 | 改动 | 理由 |
|---|---|---|---|
| 12 | `lib/algorithms/photometry/cpp/src/pc_api.cpp:39-58,190-200` | `pc_calibrate_simple` 拆为 `run_simple_impl(..., const uint32_t* quality_flags)` + 冻结 C 导出包装（传 nullptr） | `:138` 的 `nullptr` 变为 impl 形参；C 导出签名/行为不变。 |
| 13 | `pc_api.cpp:213-232,487-500` | 同款拆 `pc_calibrate_simple_with_gaia`（`:397` 的 `nullptr` 变 impl 形参） | 同上。 |
| 14 | `pc_api.cpp:915,1120` | `run_with_gaia_impl<T>` 增 `quality_flags` 形参并送达 `cleanAndScale` | f64_v2 生产路径。 |
| 15 | `pc_api.cpp:1294-1336` | 新增 `pc_calibrate_simple_with_gaia_f64_v2_qf`（C++ 链接期，非 `PC_API`） | 生产拟合入口；`star_matcher.cpp:428-438` 的 `PC_QF_SATURATED_MASK` 过滤首次在生产生效。 |
| 16 | `module_adapters.cpp:3200-3210`（fit 装配） | 由 `p1_sources[].quality & 1`（sdet 饱和位，`star_detector.cpp:170`）构造 `pqf`，映射 `PC_QF_SATURATED`（`1u<<1`） | 把管线里的饱和信息送到 `cleanAndScale`。 |

### 1.3 测试

| # | 文件:行 | 改动 |
|---|---|---|
| 17 | `tests/unit/p1001_real_nodes_test.cpp:2989-3140`（新增 `test_fixp1_photometry_apply`）+ `main` 注册 | 节点级红/绿：正例（sidecar k=0.5 → applied=true/photoapplied 存在/像素=0.5·orig/元数据如实）；负例1（无来源 → applied=false/不写产物）；负例2（部分覆盖 → 整组不施加）。 |
| 18 | `tests/unit/CMakeLists.txt:781-791` | `photometry_apply.cpp` 已入 `astrocs_calibration`，测试 target 不再重复编译同一 TU（注释同步）。 |

---

## 2 施加机制与判据（为什么这样实现）

### 2.1 实现方式选择：扩展 `p1_op_photometry`（而非新增 apply 节点）

理由：
1. 该节点**已经逐帧读 calibrated 像素**（`p1_calibrated_path`）并已产出 `p1_phot.json`
   （drizzle 的 `photprov` typed 边指向它）。在测完即施加**不需要新增节点/端口/IR 边**，
   改动面最小；新增节点会破坏 8 节点冻结链（`tests/cli/test_phase1_inprocess.py::test_ir_matches_frozen_chain`
   与 `module_ports.registry.json` 奇偶校验）。
2. `p1_sources.json`（PSF 星 + quality）与 `p1_wcs.json`（逐帧 WCS）都在本节点可达，
   拟合所需的输入天然齐备。
3. 与设计一致：`02_FROZEN_STAGE1_HISS_SPEC §7` / `docs/algorithms/CALIBRATION_ALGORITHMS.md §3.6`
   要求"Drizzle 前应用"；本节点是 drizzle 的直接上游。

### 2.2 k_photo 来源与完整性门

- **来源①（生产拟合）**：`photometry.fit.enabled=true` 时逐帧调
  `fit_frame_photometry`，内部：
  - 取最亮 `max_stars`（默认 5000）颗检测源作 PSF 输入；
  - 由 `p1_wcs.json` 取 TAN+SIP；由 `filters.json`/`qe_curves.json` 取响应曲线；
  - `gaia_client_create(gaia_data_dir)` → `gaia_client_get_spectrum_params` 建光谱网格；
  - 调 `pc_calibrate_simple_with_gaia_f64_v2_qf` → `out_scale_factor = k_photo`。
  - **配置缺失（gaia/filter/filters_json）或任一帧拟合失败 ⇒ 不施加**（不造 1.0），
    记 `degraded_reason=photscale_incomplete` + `photscale_error`。
- **来源②（sidecar）**：`out_dir/p1_photscale.json`（`DATA-P1-PHOTSCALE-001`）逐帧
  `{file, k_photo, n_matched, sigma_residual_dex, source}`。
- **完整性门**：只有**全部帧**都解析到合法 `k_photo`（有限且 >0）才施加。部分归一化会把帧
  拉到不同测光坐标系 ⇒ 比不归一化更糟；此时整组不施加且如实记 `photscale_incomplete`。
- **施加**：`calibration::apply_photometry(in, w, h, k_photo, in)`（in-place，double 中间精度，
  NaN/Inf 透传），写 `photoapplied_<base>`（`p1_write_fits_atomic`）。
- **drizzle**：`photometry_applied=true` ⇒ 读 `photoapplied_<base>`；产物缺失 ⇒ DATA fail-closed。

### 2.3 P1-2 为什么不改 ABI

`p1phot_fixgates.cpp:13-18` 记录负责人 2026-09-14 裁决：**不得新增/修改任何 C 导出 ABI**
（orchestrator 用 raw 函数指针按位置调用）。故：
- 6 个 `PC_API` 导出签名/行为**逐行不变**（`run_simple_impl`/`run_with_gaia_f32_impl`/
  `run_with_gaia_impl` 加形参，导出包装传 `nullptr`）；
- 新增一个 **C++ 链接期**（非 `extern "C"`、无 `PC_API`）入口 `..._qf` 供生产节点使用。
  这既让饱和过滤在生产生效，又不触碰 ABI。

---

## 3 科学行为变更清单（须记入 ACCEPTANCE §3）

| # | 变更 | 依据 | 影响面 | 如何验证 |
|---|---|---|---|---|
| S1 | Phase1 在 Drizzle 前对像素施加 `I_photo = k_photo·I_cal`（此前从未执行） | `docs/science/PHOTOMETRY.md`（SCI-PHOT-001 FROZEN）/ `docs/algorithms/CALIBRATION_ALGORITHMS.md §3.6` / 设计 §7 | 启用后帧间乘性尺度差被消除，产物单位由 ADU → `ASTROCS_RELATIVE_FLUX`（`BUNIT`/`PHOTAPPL=1`）。**默认（无配置/无 sidecar）行为不变**（如实中性）。 | Oracle A（施加 vs 不施加）；p1001 新用例；L4 重跑帧间尺度极差。 |
| S2 | 生产拟合路径把 sdet 饱和位送达 `cleanAndScale`（此前恒 nullptr） | SCI-PHOT-001 §4/§10；`star_matcher.cpp:415-443` | 启用拟合时，饱和星**不进入**零点拟合（此前会进入）。 | Oracle B（rejected_quality 0→1，location 偏移）。 |
| S3 | `p1_phot.json` 新增键 `photscales`/`photoapplied_artifacts`/`photscale_source`/`apply_entry`/`degraded_reason`/`photscale_error`；`operation` 仍为 `measure_flux` | 如实 provenance（DATA-P1-PHOTPROV-001 增量） | 纯增量；既有消费者（B2-A14）不受影响。 | p1001 用例断言键存在；B2-A14 仍绿。 |
| S4 | `p1_stack.json`/HISS 头的 `PHOTSCAL` 改为**逐帧**值（此前为单一标量） | 帧间 k_photo 不同（实测极差 3.53×） | 仅 applied 时；drizzle 读 `photscales[frame_key]`，缺省退回标量（向后兼容 B2-A14 夹具）。 | p1001 用例；B2-A14 14b（无 photscales）仍绿。 |
| S5 | `apply_photometry.cpp` 编入生产库 `astrocs_calibration`；新增静态库 `astrocs_phase1_photcal` | 修复"零生产调用者/未构建 DLL" | 构建图变化；`astrocs_module_adapters` 链接闭包增大。 | `g++ -fsyntax-only`；前台 `ninja`。 |
| S6 | `pc_api.cpp` 内部重构（3 个实现函数加形参 + 新增 `_qf`）；6 导出行为不变 | ABI 冻结裁决 | 无行为变化（导出仍传 nullptr）。 | `p1phot_*` 既有测试应全绿；Oracle B。 |

**未改任何冻结科学公式/默认容差**：`k_photo = 10^(-location)`、Tukey c=4.685、mag_tolerance=3.0、
match_radius=2px、`|r_consistent|>=3` 等一字未动。

---

## 4 判别力测试（能红能绿）

### 4.1 Oracle A：施加语义（生产尺度 4096×4096）

- 源码：`run/RELEASE-02/fix-p1/p1_apply_oracle.cpp`（编译生产
  `lib/algorithms/calibration/src/photometry_apply.cpp`）。
- 构造：真值场景 `S`（天光+梯度+200 星点）；帧级响应 `g=[0.80,1.00,1.25,2.82]`（3.53× 极差，
  与 q1 实测一致）；`F_instr = g_k·F_syn ⇒ k_photo = 1/g_k`（star_matcher 定义）。
- 结果（`run/RELEASE-02/fix-p1/oracle_output.txt`，`ORACLE_PASS`）：
  - **负例**：不施加 → 帧间比值极差 `max|g_k/g_ref−1| = 1.820000`（非零，判别力锚）；
  - **正例**：施加 `k_photo=1/g_k` → 帧间比值中位 `1.000000000000`，`|中位−1| = 0.000e+00`；
    逐像素 `max|out − k·in| = 4.883e-04`（float32 舍入内）；
  - 误差语义：`k=0/−1 → rc=-5`、`k=NaN/Inf → rc=-4`、NaN/Inf 像素透传。
- **红/绿**：若节点不施加（修复前行为），正例 `|中位−1|` 会是 1.82 量级 ⇒ 必红。

### 4.2 Oracle B：质量位（P1-2）

- 源码：`run/RELEASE-02/fix-p1/p1_qf_oracle.cpp`（编译 pc_api + star_matcher + wcs_transform +
  image_corrector + spectrum_integrator + p1phot gaia 桩）。
- 构造：9 颗匹配星，8 颗内点（r 散布 ±0.11 dex ⇒ S>0），第 0 颗**饱和**且 `r=+0.25 dex`
  （在 Tukey `c·S` 有效域内 ⇒ 无质量位时不会被 IRLS 剔除）。
- 结果（`run/RELEASE-02/fix-p1/qf_oracle_output.txt`，`ORACLE_PASS`）：
  - ① `quality_flags=nullptr`：`location=+0.021628 dex`，`rejected_quality=0`；
  - ② `quality_flags[0]=PC_QF_SATURATED`：`location=+0.005000 dex`，`rejected_quality=1`；
  - `|loc_b|<|loc_a|` 且 `|loc_a−loc_b|=0.0166 dex > 1e-3`。
- **红/绿**：若 `_qf` 把质量位丢成 nullptr（修复前语义），①② 完全相同 ⇒ 必红。

### 4.3 节点级用例（前台 ctest）

`tests/unit/p1001_real_nodes_test.cpp::test_fixp1_photometry_apply`（已注册 main）：
- 正例：`p1_photscale.json k=0.5` → `man.photometry_applied=true`、`man.photscal=0.5`、
  `photoapplied_light_1.fits` 存在且像素 `≈0.5·orig`、`p1_phot.json` applied/applied/photscales 齐全；
- 负例1：无来源 → `applied=false`、`photscal=1.0`、无 `photoapplied`、`degraded_reason=photscale_absent`；
- 负例2：sidecar 只覆盖 1/2 帧 → `applied=false`、无任何 `photoapplied`。
- **RED 锚**：修复前节点写死 `applied=false/photscal=1.0` ⇒ 正例三条断言必红。

### 4.4 语法门（本分片自查）

- `syntax_pc_api.log`、`syntax_frame_fit.log`、`syntax_module_adapters.log`、
  `syntax_p1001_test.log`：**全部 0 行输出（0 错）**。

---

## 5 未做项与原因

1. **未在本分片跑构建/全量测试**：按要求由前台统一 `ninja`/`ctest`。新增
   `astrocs_phase1_photcal` 与 `astrocs_calibration` 源变更需前台编译验证；
   若 CMake 有前向引用问题（`astrocs_p1_ipv` 在 photcal 之后定义），CMake 在
   generate 期解析 target 名，正常可解，但请前台确认。
2. **L4 默认未启用拟合**：拟合是**显式 opt-in**（`photometry.fit.enabled=true`），
   避免改动既有测试/既有 L4 行为。前台重跑 L4 时按
   `run/RELEASE-02/fix-p1/l4_photometry_fit_snippet.json` 追加配置。
   **注意 `gaia_data_dir` 必须指向含光谱的 `GaiaDR3SP`**（`GaiaDR3` 无光谱；
   现有 L4 配置的 `wcs.gaia_data_dir=.../GaiaDR3` 仅够 WCS）。
3. **未量化拟合在真实 L4 数据上的质量**（需 GaiaDR3SP + 前台构建）：建议前台启用后
   记录 `n_matched`/`sigma_residual_dex` 与帧间尺度极差（q1 实测 3.53× 应显著下降）。
4. **P1-3 `F_instr` 估计量未改**（涉及冻结科学定义，见 §6）。
5. **未给 `photoapplied_<base>` 增加 typed IR 输出端口**：drizzle 经既有 `photprov`
   typed 边（`artifact:p1_phot`）已保证 phot 先于 drz；产物经 output_dir 文件约定消费
   （与 `p1_wcs.json` 同款）。如需静态图强校验，可后续加端口（会动 registry，超出最小面）。
6. **`p1_photometry_descriptor` 的 `UnitId::ELECTRON` 与实现（ADU/相对流量）不一致**：
   chain-audit §3 已登记；本分片未改 descriptor（避免动注册表/端口冻结面）。
7. **拟合路径每帧 `gaia_client_create`**：与 WCS 节点同款（每帧建/销毁）。L4 49 帧下可能偏慢；
   后续可缓存 client 或按需惰性创建（性能优化，非正确性问题）。

---

## 6 P1-3 登记：`F_instr` 估计量（仅登记，不擅自改）

- **现状**：`F_instr` 取 `sdet` 的**检测等照度 flux**（连通域内 `image−bkg>0` 像素求和；
  `sdet_detector.cpp:281-285` → `module_adapters.cpp:2102-2106`）。
- **缺陷**：该估计量依赖检测阈值与连通域形状，有强视宁度/阈值偏差；
  `phot-verify.md` 实测：固定孔径(r=6px) 跨望远镜帧间差 0.03–0.12 mag，
  而生产口径（检测等照度 flux）可达 **~0.5 mag**。
- **影响面**：`k_photo` 的零点偏差 → 帧间测光一致性（接缝/绝对标度）。
- **建议**（**需前台裁决，因涉及 SCI-PHOT-001 的 `F_instr` 定义**）：
  改用**固定孔径总通量**或 **PSF 总通量**（`F_instr = Σ PSF 模型`）。
  两者都会改变 SCI-PHOT 的输入定义与默认容差，属科学行为变更，本分片不擅自实施。
- **登记锚**：`reports/RELEASE-02/phot-verify.md §2.1`；`工程控制/RELEASE-02/GAP_AUDIT.md §9.31 D5`。

---

## 7 证据文件

| 文件 | 内容 |
|---|---|
| `run/RELEASE-02/fix-p1/p1_apply_oracle.cpp` / `oracle_output.txt` / `oracle_build.log` | Oracle A（施加语义，ORACLE_PASS） |
| `run/RELEASE-02/fix-p1/p1_qf_oracle.cpp` / `qf_oracle_output.txt` / `qf_oracle_build.log` | Oracle B（质量位，ORACLE_PASS） |
| `run/RELEASE-02/fix-p1/syntax_pc_api.log` / `syntax_frame_fit.log` / `syntax_module_adapters.log` / `syntax_p1001_test.log` | `g++ -fsyntax-only`（均 0 错） |
| `run/RELEASE-02/fix-p1/l4_photometry_fit_snippet.json` | L4 启用拟合的配置片段 |
| `run/RELEASE-02/fix-p1/p1_photscale_example.json` | sidecar 契约示例 |
| `run/RELEASE-02/fix-p1/ma_cmd.txt` / `p1001_cmd.txt` | 从 `build.ninja` 取的编译命令（语法门用） |
