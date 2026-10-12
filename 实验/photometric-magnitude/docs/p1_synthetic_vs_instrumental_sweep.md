# "合成/标准侧 ↔ 仪器/实测侧"误用扫查

> 性质：**只读**。本文只登记命中，不改代码、不改其它文档、不做 git 操作。
> 目的：查全仓还有没有别处把**合成/标准侧**量当**仪器/实测侧**量用（或反向）。
> 已知的一处（`p1_phot.json` 原 `zero_point_*` → 现 `synthetic_colour_*`，及其下游 SNR 锚定）
> **不在本清单重复登记**（见 §5「已排除」）。
> 工作树在读时正被并发改动（改名 + `PER-STAR-EXPORT-01` + SNR 绝对零点 `C_abs` 替换），
> 行号取本读取时刻；若漂移以符号名为准。

---

## 0 两侧的定义与判定依据

- **标准/合成侧**：输入只来自星表与滤镜/QE 曲线、**不含该帧任何观测数据**（无像素、无实测流量）的量。
  正本：`docs/science/photometry/PHOTOMETRY.md`「物理模型」（`F_syn = ∫F_λ·T·Q·λ dλ`，谱为 Gaia DR3 XP 外部定标绝对谱）；
  `docs/detail/registry/acsd.phase1.photometry.md`（「帧级天区合成颜色项读数键」一节原句：
  "**不含任何该帧的观测数据**……因此它**不可能测到**帧的增益或透过率，**不得用作帧零点**"）；
  `docs/detail/common/unified_model.md:66-106`（`F_ref` 基准与 `ZP_k = ZP_syn − 2.5·log10(k_photo)`）。
- **仪器/实测侧**：来自该帧的像素/PSF 拟合域通量、检出目录、噪声。量纲 ADU/ADU²，随帧的增益/曝光/大气变。
  正本：`docs/science/photometry/PHOTOMETRY.md`（`scale = 10^(−location)`，`F_instr` 为 PSF 拟合域解析通量）；
  `docs/detail/common/unified_model.md:39-44`（`signal`/`variance`/`depth_m5` 定义）。
- 另可用落盘键的口径表：`docs/science/unified/DATA_SEMANTICS.md:208`（`frame_zero_point_mag = ZP_k`，满足 `mag = ZP_k − 2.5·log10(F_adu)`）。

---

## 1 从合成端出发：`compute_f_syn_cached_xpsd` 的全部调用点

权威口径块：`lib/algorithms/photometry/cpp/src/spectrum_integrator.h:116-131`
（"`F_syn = ∫F_λ·T·Q·λ dλ`（W·m⁻²·nm；**不含** `10^(-0.4·G)`）"）。
调用点（全仓 grep，仅 4 处，全部在 photometry 模块内）：

| # | 位置 | 原句/用途 | 判定 |
|---|---|---|---|
| A1 | `lib/algorithms/photometry/cpp/src/pc_api.cpp:386`（f32 路径） | `f_syn_values[i] = compute_f_syn_cached_xpsd(...)` → 填 `gaia_fsyn[]` 供 `r = log10(F_instr/F_syn)` | **正确**（标准侧作分母） |
| A2 | `pc_api.cpp:797`（f64 主路径） | 同上 | **正确** |
| A3 | `pc_api.cpp:1078`（第二套 run 实现） | 同上 | **正确** |
| A4 | `lib/algorithms/photometry/cpp/src/frame_photometry_fit.cpp:287` | `fsyn` → `z = magG + 2.5·log10(fsyn)` 作天区颜色项 | **正确**（标准侧之间组合；该对象仅作颜色诊断，见 §5） |

**结论**：`compute_f_syn_cached_xpsd` 无一处被当作仪器实测量使用。**未发现其它命中。**

---

## 2 命名出发：扫"仪器/实测名 + 合成值"与"合成名 + 仪器值"的键

扫描范围：`docs/` 的产品键表 + `lib/infrastructure/scheduler/src/module_adapters.cpp` 的 JSON 写入面 + `eng/contracts/schemas/**`。

| 命中 | 位置 | 原句/键 | 判定 | 若确实错用会影响什么 |
|---|---|---|---|---|
| **H1** | `lib/infrastructure/aio/healpix_db/archive/legacy/healpix_stack/gradient/gradient_sampler.cpp:160-166` | `// R_reject = base + growth × flux (flux 用星等估算: flux = 10^(-0.4×mag))` → `double flux = std::pow(10.0, -0.4 * (double)gaia_mag[idx]);` | **明确错用（已归档、不参与构建）**：把**星表 G 星等**换算成"相对通量"当**该帧实测星亮度**，据此定星拒绝半径 | 若复活：拒绝半径与该帧实际曝光/透明度无关，同一颗星在深/浅帧上帽大小相同 ⇒ 掩膜在浅帧欠掩、深帧过掩 |
| **H2** | `lib/infrastructure/scheduler/src/module_adapters.cpp:9906-9913`（生产者）+ `lib/algorithms/coverage/src/sampler.cpp:365-399` + `lib/algorithms/coverage/src/sky_plane.cpp:1071-1139`（消费者） | 生产者把 `stars[..].magG` 收进 `halo_mag`；消费者 `p2_gaia_halo_radius_px(mag_g, …)` 用**星表 G 星等**定晕帽半径 | **明确错用，但当前停用**：`halo_src = "disabled-halo-falsified"`（`module_adapters.cpp:9856/9929`），`anchor_scale = 0.0`（`:9972/10010`）、`halos.n = 0` ⇒ `sampler.cpp:366` 直接 `return false` | 若重新启用：晕帽半径只随星表星等变、不随该帧实测流量/曝光变，掩膜与帧深不匹配 |
| **H3** | `lib/algorithms/photometry/cpp/src/spectrum_integrator.cpp:208-210`、`:401`（声明 `spectrum_integrator.h:52-72`、`:104`） | `// [非生产通道] uint8 -> float64, 作为相对谱形, 并乘 10^(-0.4*mag_g) 做星等归一化` → `double mag_factor = std::pow(10.0, -0.4 * mag_g);` | **明确错用，但无调用者**：头注释自述「仅用于与历史口径的数值对拍；生产定标必须走 `compute_f_syn_cached_xpsd`」 | 若被误当生产合成用：逐星注入 `+0.4·G_i` (dex) 加性项，单标量零点吸收不掉（头注释 `:29-32` 已给读数 0.459 dex） |
| **H4** | 配置键 `snr.zero_point_mag`：`lib/infrastructure/scheduler/src/module_adapters.cpp:7170`、`lib/algorithms/noise_snr/cpp/include/snr_estimator.h:335`、`eng/contracts/schemas/phase_config_normalize.schema.json:229`、`lib/infrastructure/cli/session_commands.h:194` | `sci_cfg.zero_point_mag = sc.value("zero_point_mag", 0.0);`；schema 描述只写 "可选 SNR 科学块（gain_e_per_adu/read_noise_e/`zero_point_mag`…）" | **命名可疑（配置面，活路径）**：该键要求的是**帧级仪器零点**（用于 `m_5 = ZP − 2.5·log10(F5)`），但名字与旧的**合成**键完全同名，且正本未标注取哪一侧 | 用户若把 `p1_phot.json` 的合成零点（旧键值 ≈ −14.27）填进本键 ⇒ 5σ 深度系统性偏亮/偏暗，且逐天区带色差 |
| **H5** | `docs/science/algorithms/normalize/PHOTOMETRIC_FIT.md:207`（文档面） | "`zero_point_mag = median_i(magG_i + 2.5·log10 F_syn,i)` 必须由上式**逐位复现**" | **文档滞后（与 H1 同日改名的同源对象）**：代码/产品已改名 `synthetic_colour_*`，本页仍用旧名并称为"生产路径的实证判据" | 若不同步：复现脚本继续找 `zero_point_mag` 键 ⇒ 找不到或误取，判据落空 |

**其它命名面检查（未命中，列出以便复核）**：
`ACSD_REFERENCE_FLUX` / `snr_reference.flux_common` 的 `flux_common_unit = "F_syn (Gaia XPSD absolute spectral integral)"`
（`module_adapters.cpp:7626-7627`）——按 `unified_model.md:89-95` 的 `F_ref,k = F0/k_photo` 展开，`F0` 确为合成通量单位、
`F_ref,k` 为 ADU，**标注正确**。`reference_mag_system = "gaia_g_via_synthetic_xpsd"`
（`:7630-7631`）已自述 synthetic，**正确**。`frame_zero_point_mag`（`DATA_SEMANTICS.md:208`）在绝对零点替换后为真仪器零点，**正确**。

---

## 3 从仪器端出发：实测通量有没有被当标准侧用

- **B1 `star_matcher.cpp:454-491`**：`double mag_inst = -2.5*std::log10(matches[i].f_instr); double delta = mag_inst - matches[i].gaia_mag;`，随后 `|delta − median(delta)| ≤ 3.0 mag` 作星等预过滤。
  判定：**正确（预筛，非定标）**——它是把**实测仪器星等**与**星表 G 星等**直接相减，不经标定因子，但只用于剔除错配（±3 mag 粗筛），真正定标用 `r = log10(F_instr/F_syn)`。
  注：它隐含"仪器星等与 G 同波段"的假设，仅作粗筛可接受。若把它当定标零点用即错，现无此用法。
- **B2 `pc_api.cpp:797/386/1078`**：`gaia_fsyn[]`（标准）与 `f_instr`（实测）进入比值 `r`，**正确**。
- **B3 检出域盒和 `s.flux = m00`**（`star_detection`）与 **PSF 域解析通量 `2πA·sx·sy/3`**：
  两者都是实测侧，只是估计量不同；生产测光（`photometry`）用 PSF 域，SNR 聚合（`noise_snr`）按 `snr_frame_science.h:38-44` 明确用**检测块** `fwhm_px` 并以**高斯因子**换算 σ。属**同侧双口径**（见 §6 邻近项），非合成/仪器混用。
- **未发现**实测通量被当作合成通量（例如直接写进 `F_syn`/星表星等）的位置。

---

## 4 从 `F_ref`/深度/权重链反查

- `module_adapters.cpp:7294-7352`：`F_ref,k = 10^(−0.4(m_ref − ZP_k))`、`ZP_k = C_abs − 2.5·log10(k_photo)`（`kCAbsMag = -0.003651`，`:7345`）。
  判定：**改后正确**——`C_abs` 是**一次算定的绝对常数**（G∈[6,18] 参考域的星表 G 与合成星等差中位归零），不再取逐帧/逐天区颜色项。
- `module_adapters.cpp:7366-7375` 的公共锚 `F0 = 10^(−0.4(m_ref − zp_syn))`：`zp_syn` 现来自 `kCAbsMag`（`:7352`），**正确**。
- `module_adapters.cpp:7265-7288` 的 `group_median` 回退：用 `r.flux_adu`（实测检出通量）取块中位，**正确**（仪器侧）。
- 未发现其它把合成量塞进逆方差/权重式的位置。**未发现其它命中。**

---

## 5 已排除（不重复登记）

以下属**已知问题**（派单点名），本清单不重复：

1. `p1_phot.json` 的 `zero_point_*` → `synthetic_colour_*` 改名与语义澄清
   （`frame_photometry_fit.h:130-145`、`module_adapters.cpp:5706/6468-6474/6551-6554`）；
2. SNR 节点把该天区颜色项当帧零点（`module_adapters.cpp` 原 `zp_inst = zps − 2.5·log10(kp)`），
   现正被替换为 `ZP_k = C_abs − 2.5·log10(k_photo)`（`:7339-7349`）；
3. 同一对象的文档面 `docs/detail/registry/acsd.phase1.photometry.md:87` 已登记"旧名（仅作追溯）"。

（H5 是同一对象的**文档面滞后**，因为它在 `docs/science/**`，需同步；列此仅供归属，不计为独立缺陷。）

---

## 6 邻近项（同侧口径，不属本次扫查，登记以备）

- **检测块高斯 FWHM vs PSF 块 Moffat4 FWHM**（相差 1.914005×）：
  `snr_frame_science.h:20/43`、`snr_science.cpp:22-25/50-54`、`star_detector.h:21-23`，已带"禁止跨块比较/互换"警告并有 CONFORM-SWEEP-1-001 修复面。
- **检出域盒和 vs PSF 域解析通量**：`frame_photometry_fit.h:36-44` 明确 `psf_flux` 只收 PSF 域解析通量；SNR 聚合按检测域列、不混用。

两者都是**仪器侧内部**的口径差，不是合成/仪器混用。

---

## 7 扫过的范围（便于复核）

- **函数/符号调用图**：`compute_f_syn_cached_xpsd` / `compute_f_syn_cached` / `compute_f_syn` 全仓调用点（含 `run/**/wsrc` 历史快照已排除，仅活树 4 处；非生产通道活树无调用者）。
- **量-通量换算**：全仓 `pow(10, -0.4…)/10^(-0.4…)/0.4*` 字面式（仅命中 §2 H1/H3 与 SNR 的 `F_ref/F0` 公式）。
- **星表量消费**：`magG`/`gaia_mag`/`mag_g` 的全部消费点（排除 `gaia_xpsd_client`/`platesolve`/`star_matcher`/`pc_api`/`frame_photometry_fit` 后逐一判定）。
- **零点/仪器名**：`mag_inst`/`instrumental`/`zp_inst`/`zero_point`/`zeropoint`/`mag_zp` 全仓。
- **参考通量/权重链**：`reference_flux`/`ACSD_REFERENCE_FLUX`/`F_ref`/`frame_snr`/`depth_m5` 的产消点（normalize、drizzle/sink、aio_hips、coverage/sampler、sky_plane、phase2 integrate）。
- **产品键与 schema**：`module_adapters.cpp` 的 `p1_phot.json`/`p1_snr.json`/`p1_flux.json`/`p1_sources.json` 写入面；`eng/contracts/schemas/unified/{frame_snr,depth_m5,point_information,sparse_snr_layer}.schema.json`、`phase_config_normalize.schema.json`。
- **归档/遗留**：`lib/infrastructure/aio/healpix_db/archive/**`（仅 `gradient_sampler.cpp` 命中，未编入 CMake）、`hiss_stream_writer` 等无关项。
- **口径文档**：`docs/science/photometry/PHOTOMETRY.md`、`docs/detail/registry/acsd.phase1.photometry.md`、`docs/detail/common/unified_model.md`、`docs/science/unified/DATA_SEMANTICS.md`、`docs/science/algorithms/normalize/PHOTOMETRIC_FIT.md`。

## 8 总体判定

- **未发现**与已修 `zero_point_*` 同类的、仍在生产生效的"合成量当仪器量"新缺陷。
- 测得的**其它命中 = H1/H2/H3（均已死/停用）、H4（配置键命名歧义，活路径但需人工填错才触发）、H5（文档滞后）**。
- 合成端调用点（§1）与仪器端消费点（§3）逐点判定**正确**。
- **唯一建议**：给配置键 `snr.zero_point_mag` 在正本标注"取仪器零点（`ZP_k`），**不得**填合成颜色项"，并同步 `PHOTOMETRIC_FIT.md:207` 的旧名；两者属"只登记、不改"。
