# CONFORM-SWEEP-1 — 规范↔实现符合性审计（Phase1 校准/坏点/星检/PSF/解算 + p1_op_* 生产节点）

> 分片: CONFORM-SWEEP-1（AstroCS RELEASE-02 规范↔实现符合性整体排查）
> 方法: 枚举规范侧规定性陈述（公式/常数/单位/定义域/步骤顺序/默认值/schema 字段语义）→ 定位实现 → 判定 → 取证
> 工作目录: `/workspace/Astro CS Database`（HEAD `cbda64a1424030f977fbaa777fb3923bdcfc959f`）
> **实现侧快照**（审计基准，因工作树正在被并行分片修改）: `/dev/shm/astrocs_conf1/snap/`
> `module_adapters.cpp md5=35288396163311bf12a13d717270cd6e`（mtime 2026-09-19 19:23:35 +0800，审计期间被并行改动过，行号以本快照为准）
> `sdet_api.cpp db23020bf8d709325fecdc8f8bc8624e` / `dpsf_psf.cpp 37c6f22a32cfee60eed81dfcca7b818a` / `star_detector.cpp(wrapper) ffce241f6707aa4b08e454e3271ec4ee` / `snr_science.cpp ca3b134a462aefd7d86d014530980455` / `calibrator.cpp 11d6672790c075107843594011004f44` / `master_generator.cpp b5a82dad11a454cfcfcc0daf5091d779` / `cosmetic_corrector.cpp 2a32847ae9b61e0229b43ee335a41b29`
> 硬约束遵守: 未改任何生产代码/文档；零 git 写；未跑 ninja/cmake/ctest；TMPDIR=`/dev/shm/astrocs_conf1`（收尾清理）。

---

## 1 汇总统计

### 1.1 陈述清点

| 项 | 数 |
|---|---|
| 枚举到的规定性陈述（公式/常数/单位/定义域/顺序/默认值/schema 语义） | **108** |
| 判定「符合」 | **77** |
| 判定「不符」 | **13** |
| 判定「未实现」 | **2** |
| 判定「规范歧义 / 规范内部打架」 | **11** |
| 判定「待定」（证据不足，如实登记） | **5** |
| 其中属「规范本身错/陈旧」（另一类，非实现缺陷） | **7** |
| 合计非符合条目（本节 §2 逐条展开） | **24** |

> 计数口径: 逐条编号见 §4 全量清点表（`S-xxx`）；§2 的 24 条为需要行动的非符合条目（`CONFORM-SWEEP-1-NNN`）。
> 同一规范陈述可同时映射到「不符」与「规范歧义」两栏时只计一次，取更严重者。
> **口径修订记录（诚实性自校）**：S-026 由「符合」改判「待定」、S-029 由「不符」改判「符合」（规范为「允许丢弃」的许可条款）、S-030/S-032 由「不符/符合」改判「规范歧义」（相关要求定义在 DATA-P1-STAR，而交付链产品是 DATA-P1-SOURCES，二者不是同一产品）。修订后合计仍 108 条。

### 1.2 按缺陷类别分组（C1–C7）

| 类 | 定义 | 本分片命中 | 条目 |
|---|---|---|---|
| **C1 口径不符** | 规范规定了公式/域/单位，实现用了别的 | **7** | 001 002 003 004 008 014 016 |
| **C2 常数不符** | 规范给了常数，实现用了别的 | **2** | 005 011 |
| **C3 未接线** | 规范要求的步骤在生产链路不存在 | **3** | 009 010 015 |
| **C4 默认值不符** | 规范/工具默认与生产默认不一致 | **0** | —（本轮未在范围内找到独立实例；见 §6 诚实声明） |
| **C5 硬编码绕过** | 用硬编码替代本应计算/配置的量 | **2** | 006 008 |
| **C6 有实现无调用** | 算法实现了但零调用者 | **2** | 010 017 |
| **C7 合同不符** | 实现与 `contracts/**`/ALG 冻结表 schema 不一致 | **3** | 007 012 013 |
| **规范侧问题（另一类）** | 规范本身错/陈旧/自相矛盾 | **7** | 018 019 020 021 022 023 024 |

> 说明: 008 同时命中 C1 与 C5（清单写入错量），010 同时命中 C3 与 C6；分组表按主类计一次，条目内注明次类。

---

## 2 逐条明细（非符合条目）

### CONFORM-SWEEP-1-001 [C1] 逐源 SNR 轮廓把「检测域高斯 FWHM」当 Moffat4 FWHM 用 ⇒ σ 系统性高估 1.9140×

- 规范：`docs/science/STAR_DETECTION.md:31-34`「**检测侧母函数 = 椭圆高斯**（本页 §1/§3，ALG-STARDET-001 §2），**PSF 侧 = 椭圆 Moffat4**（SCI-PSF-001 §5）；两模型**宽度列不可跨块比较**：同 sx 下 `FWHM_gauss/FWHM_moffat = 2.354820/1.230310 = 1.9140×`（DISP-STAR-007）」
  规范：`docs/algorithms/STAR_DETECTION_ALGORITHMS.md:64`「`fwhm=2.3548·σ`（TWO_SQRT_2_LOG2）… 同 sx 下 FWHM 相差 1.9140×，**禁止跨块比较**（DISP-STAR-007）」
  规范：`docs/algorithms/STAR_DETECTION_ALGORITHMS.md:253-258`「检测/PSF 双母函数（列语义不可互换）… 因此 `star_det` 的 fwhm/flux 列与 PSF 块同名列**禁止跨块比较**」
- 实现：`lib/algorithms/noise_snr/cpp/src/snr_science.cpp:37` `constexpr double kMoffat4FwhmFactor = 1.230310;`；`:41-42` `moffat4SigmaFromFwhm(fwhm_px) { return fwhm_px / kMoffat4FwhmFactor; }`；`:129` `sigma = (p->fwhm_px > 0.0) ? moffat4SigmaFromFwhm(p->fwhm_px) : p->sigma_px;`
  数据来源：`lib/algorithms/star_detection/wrapper_phase1/star_detector.cpp:162` `s.fwhm_px = 2.3548 * 0.5 * (a + b);`（检测域高斯 FWHM）
  生产装配：`lib/infrastructure/scheduler/src/module_adapters.cpp:3746` `row.fwhm_px = s.value("fwhm_px", 0.0);` → `:3786` `compute_snr_frame_science(rows, cfg)`
- 判定：**不符**
- 证据：fwhm_px 由检测侧以高斯母函数给出（`FWHM = 2.3548·σ_true`），却被 `snr_science.cpp:41-42` 按 Moffat4 关系反解 ⇒ `σ_used = fwhm_px/1.230310 = (2.354820/1.230310)·σ_true = 1.914005×σ_true`（独立复算：`python3: 2.3548200450309493/1.230310 = 1.9140054498711294`）。该 σ 进入 `snr_moffat4_profile_f64` 的 `alpha²=2σ²`（`:53`）与 `autoHalf = ceil(12·fwhm_px)`（`:80`），使归一化轮廓 `ΣP_i²` 整体偏小 ⇒ `SNR_F = F/σ_F` 系统性偏低。
- 影响：Phase1 帧级科学 SNR / 5σ 点源深度 `m_5`（`module_adapters.cpp:3807-3813` 写 `snr_phot`/`median_snr`/`frame_depth_m5_mag`）、`local_snr` 质量权重场全部带上 `1.9140×` 的尺度错（权重场内部相对量部分相消，但绝对 SNR 与深度直接错）。本项即本轮暴露的已知实例 #2。
- 修复面：`lib/algorithms/noise_snr/cpp/src/snr_science.cpp:37,41-42,97,129` —— 要么把 `p1_sources.json` 的 `fwhm_px` 显式标注母函数并先做 `σ_gauss = fwhm_px/2.354820` 再按 Moffat4 `FWHM=1.230310·σ` 重建（等价于 `FWHM_moffat = 0.52245·fwhm_gauss`），要么在 `snr_moffat4_profile_f64` 的入参契约里声明 fwhm_px 属哪一侧母函数并 fail-closed 拒绝跨块。**只登记，不改。**

### CONFORM-SWEEP-1-002 [C1] 科学链的生产检测器不是规范指定的「唯一权威生产源」，公式族整体不同

> ⚠ 已取代横幅（2026-09-20 订正）：本条 ★002「**只登记，不改**…二者必须收敛其一」**已由 §9.49 定案 1 覆盖**：检测改**星表引导**（「根据 wcs 将 gaia 星表投影，能拟合的才是星点，拟合失败直接丢弃就好了」，工程控制/RELEASE-02/GAP_AUDIT.md:1464-1470）；「权威源收敛」在星表引导实现后评估（sdet 去留 = 前台按 §9.50 授权实测评估）。**原判定与证据正文保留为历史留痕，不物理删除。**

- 规范：`docs/algorithms/STAR_DETECTION_ALGORITHMS.md:8-9`「**唯一权威生产源: lib/algorithms/star_detection/src/sdet_api.cpp**（2555 行实测，2026-09-17 LEDGER-DOC 按附录 G 机械重锚；源文件唯一在役副本）」
  规范：`docs/science/STAR_DETECTION.md:46-49`「生产路径 `sdet_detect_impl`，`sdet_api.cpp:1599-2353`」
  规范：`docs/algorithms/STAR_DETECTION_ALGORITHMS.md:33-82`（§2 全部 20 条公式：行差分 bgnoise、11×11 peaker 七步、GSL TR-LM 椭圆高斯 7 参、`mag=−2.5·log10(Σ_box)`、dedup 语义…）
- 实现：`lib/infrastructure/scheduler/src/module_adapters.cpp:2050` `const astrocs::phase1::StarDetector det(5.0);` → `lib/algorithms/star_detection/wrapper_phase1/star_detector.cpp`（177 行）
  该实现：`:40-60` 全局 2 轮 `median±3σ` sigma-clip（非行差分 3 轮 5σ）；`:97-108` 3×3 局部极大（非 11×11 peaker 七步）；`:139-152` 5×5 矩质心（非 GSL TR-LM 椭圆高斯 7 参拟合）；`:114-131` 3×3 邻域「留最强」去重（非 `matchradius=max(1,⌊0.2R⌋)` 曼哈顿去重）；无 reject_star / 无对称性门 / 无 FWHM 上限门。
  同文件自陈：`module_adapters.cpp:2025-2030`「本节点检测器是 `lib/algorithms/star_detection/wrapper_phase1` 的 P1-003 桥接类 … **不是** `lib/algorithms/star_detection` 的 sdet」
- 判定：**不符**（C1 主类；同时构成 C7 合同面）
- 证据：`p1_op_star_psf_impl` 是 `p1_sources.json`（DATA-P1-SOURCES）与 `p1_psf.json` 的唯一生产者，也是 `p1_op_photometry`（`module_adapters.cpp:3268-3269` 读 `s.value("x"/"y")`）与 `p1_op_noise`（`:3746`）的唯一上游 ⇒ 科学链上跑的检测算法与 ALG-STARDET-001 逐条描述的算法**不是同一套**。`sdet_api.cpp` 在 RELEASE-02 生产链中只被 `p1_op_wcs`（经 ipv detector handle，`:2831`）使用。
- 影响：ALG-STARDET-001 §2 的全部阈值/门/语义（5σ 全局阈、饱和双条件、对称性门、reject_star 四门、maxStars×2 双闸门、确定性论证）对交付的 `p1_sources.json` **不成立**；以该文档为 Oracle 的测试/复核会全部指向错误的代码路径。检测完备性/虚警/质心容差（TEST-STAR-DESIGN-001 F1–F6）实际从未在交付链上验收。
- 修复面：`docs/algorithms/STAR_DETECTION_ALGORITHMS.md:8`（权威源声明）与 `module_adapters.cpp:2050`（生产调用）二者必须收敛其一：或把 wrapper 提升为登记在册的生产源并为其补逐公式冻结（含 DISP 登记），或把节点切换到 `sdet_detect_ex_f64`。**只登记，不改。**

### CONFORM-SWEEP-1-003 [C1] 生产质心用 index-is-center，规范冻结 index+0.5 ⇒ 恒定 0.5 px 系统偏移

- 规范：`docs/science/STAR_DETECTION.md:38-40`「SCI-AST-001（ASTROMETRY）：**像素中心=索引+0.5 约定**（ALG-STARDET-001 §2 残差坐标），star_det 权威块消费方（plate solve fallback）按此约定解析坐标」
  规范：`docs/algorithms/STAR_DETECTION_ALGORITHMS.md:65-66`「残差坐标 `dx=x+0.5−cx`（**像素中心=索引+0.5**，:510-513）」
- 实现：`lib/algorithms/star_detection/wrapper_phase1/star_detector.cpp:146-152`：`const double px = nx, py = ny;` … `s.x = m10 / m00; s.y = m01 / m00;`（权重用**数组下标**，无 +0.5）
- 判定：**不符**
- 证据：5×5 强度加权一阶矩以 `px=nx` 计算，得「像素中心=下标」坐标；规范契约要求「像素中心=下标+0.5」。二者恒差 0.5 px（x 与 y 同）。下游 `module_adapters.cpp:3268-3269` `c.x = s.value("x", 0.0); // 位置口径不变（检测质心, 非本次改动）` 直接用作孔径测光中心与 SNR 行的位置 ⇒ 0.5 px 偏移进入测光与 WCS 初始指向。
- 影响：孔径测光在 FWHM≈2 px 时 0.5 px 偏心 ⇒ 流量偏差量级 `exp(-0.5²/(2σ²))` 级；跨块坐标契约（PLATESOLVE §11.2「像素中心双契约」）在同一帧内被同时违反。
- 修复面：`lib/algorithms/star_detection/wrapper_phase1/star_detector.cpp:146-152`（质心改 `px = nx + 0.5`，或在下游统一桥接并登记）。**只登记，不改。**

### CONFORM-SWEEP-1-004 [C1] `p1_sources.json` 的 `flux` = 5×5 正性截断盒和 `m00`，任何规范都未定义该量

- 规范：`docs/science/PSF.md:52`「`flux = 2πA·sxsy/3`（整平面延伸假设）」；`:87`「解析通量 `flux=2πA·sxsy/3`，单位 **ADU**」
  规范：`docs/contracts/DATA_SEMANTICS.md:753`「flux | float `[n]` | ADU（正常星=**检测侧椭圆高斯峰值振幅 A**，非解析积分流量…）」
- 实现：`lib/algorithms/star_detection/wrapper_phase1/star_detector.cpp:144-151`：`const double v = image[...] - cat.background; if (v <= 0) continue; m00 += v; ... s.flux = m00;`（5×5 盒、负像素丢弃）
- 判定：**不符**
- 证据：`flux` 列在生产链上有三个互不相等的候选定义——(a) 规范 PSF 域解析通量 `2πA·sxsy/3`；(b) DATA_SEMANTICS 登记为「峰值振幅 A」；(c) 实际写出的「5×5 正性截断盒和 m00」。三者量纲/数值均不同：(c) 随 seeing 与盒半径变化，不是测光量。审计期间并行分片已在 `p1_psf.json` 新增 `{"flux", p1_psf_analytic_flux(prow[1],prow[4],prow[5])}`（`module_adapters.cpp:2036,2167`）承载 (a)，但 `p1_sources.json` 的 (c) 未改、`DATA_SEMANTICS.md:753` 的 (b) 也未改 ⇒ 三义并存未消解。
- 影响：任何以 `p1_sources.json.flux` 为 `F_instr`/参考通量的消费者（含 SNR 的 `F_ref` 组内中位数，`module_adapters.cpp:3728-3738` 由 `rows[].flux_adu` 取中位）拿到的是盒和，帧间 seeing 差直接冒充流量差。本项即本轮暴露的已知实例 #1 的**未闭合面**。
- 修复面：`lib/algorithms/star_detection/wrapper_phase1/star_detector.cpp:151`（改名或改写）、`docs/contracts/DATA_SEMANTICS.md:753`（列语义须与生产源对齐）。**只登记，不改。**

### CONFORM-SWEEP-1-005 [C2] 生产检测器饱和阈值硬编码 `peak > 50000.0`，规范为 `A_fit > dynrange`（dynrange=`min(max,65535)−median`）

- 规范：`docs/algorithms/STAR_DETECTION_ALGORITHMS.md:37-40`「`dynrange=min(maxi,65535)−bg`，`minsatlevel=0.7·dynrange`，`satrange=0.1·dynrange`；norm 硬编码 65535」
  规范：`docs/algorithms/STAR_DETECTION_ALGORITHMS.md:77-78`「饱和标志（:2159）: `is_saturated = (A_fit > dynrange)`」
  规范：`docs/science/STAR_DETECTION.md:20-23`「饱和判定=3×3 邻域双条件（`meanhigh−bg ≥ 0.7·dynrange` 且 `pixel0−minhigh ≤ 0.1·dynrange`）」
- 实现：`lib/algorithms/star_detection/wrapper_phase1/star_detector.cpp:166-167`：`// 饱和: 绝对幅值接近/超过 16bit 满井 (ADU 域; 不因高 SNR 误判)` / `if (peak > 50000.0) s.quality |= 1;`
- 判定：**不符**
- 证据：生产检测器（见 002）用的是**绝对字面量 50000**，与规范的三套机制（0.7·dynrange 双条件、`A_fit>dynrange`、`norm=65535`）都不同；50000 在 `docs/` 全库无出处（`grep -rn "50000" docs/` 无命中该语义）。规范值随帧动态范围变化（`dynrange = min(max,65535) − median`），实现值固定。
- 影响：对满井 <50000 ADU 的相机全部饱和星漏标（`quality&1`→0），对高本底帧（median 高）饱和星过标。饱和位被 `p1_op_photometry` 映射为 `PC_QF_SATURATED`（`module_adapters.cpp:3288-3290`）用于有效域过滤 ⇒ 直接影响测光样本集。
- 修复面：`lib/algorithms/star_detection/wrapper_phase1/star_detector.cpp:167`（改为按 `dynrange` 判据，或把 50000 提为登记在册的配置键 + 规范出处）。**只登记，不改。**

### CONFORM-SWEEP-1-006 [C5] `p1_op_wcs` 的 `SDetParams` 全字面量：无配置键、无规范出处

- 规范：`docs/algorithms/PLATESOLVE.md:98-100`（生产通道 = orchestrator 直调 ipv，消费 `star_measurements`）；`docs/algorithms/STAR_DETECTION_ALGORITHMS.md:215-221`（`SDetParams` 9 字段生产消费面，DISP-STAR-003）
- 实现：`lib/infrastructure/scheduler/src/module_adapters.cpp:2801-2811`：`sp.structureLayers = 5; sp.hotPixelFilterRadius = 2; sp.iterativeClipSigma = 5.0f; sp.iterativeMaxRounds = 3; sp.medianFilterDetail = 2; sp.maxStars = 2000; sp.fitRadius = 0; sp.fwhmClipSigma = 3.0f; sp.maxAxisRatio = 2.0f;`
- 判定：**不符**（C5；旁证 C6）
- 证据：`wcs` 段配置解析（`:2561` `const Json& wc = doc["wcs"]...`）中**没有任何** `maxStars`/`fwhmClipSigma`/`maxAxisRatio` 键；全部为节点内字面量。对比同文件 PSF 节点已把同类上限提为配置键并明文「禁编译期硬编码」（`module_adapters.cpp:2225-2232` `p1_psf_fit_limit`，`psf.max_stars` 默认 5000），口径不一致。另 `sp.fwhmClipSigma` 在 `sdet_detect_impl` 阶段 8 已不消费（`STAR_DETECTION_ALGORITHMS.md:218-219`「impl 阶段8 已移除全局 FWHM clip」）⇒ 设值即死参数。
- 影响：`maxStars=2000` 截断直接决定进入解算的星数上限，却不可配置、不可审计；`fwhmClipSigma` 给出「已做 FWHM clip」的假象。
- 修复面：`module_adapters.cpp:2801-2811`（提为 `wcs.*` 配置键 + 登记规范出处；删除死参数）。**只登记，不改。**

### CONFORM-SWEEP-1-007 [C7] 节点级 `method` 钳位：非法值 → median，合同说「其他值按 IDW 路径」

- 规范：`docs/contracts/DATA_SEMANTICS.md:157`「| method | int 0=median / 1=IDW（名义 bilinear） | — | **其他值按 IDW 路径** |」
  规范：`docs/algorithms/COSMETIC_ALGORITHMS.md:264-267`「method 非法值（2）→ 按 §3 else 分支走 IDW（**现状：非 0 即 IDW**，`cosmetic_corrector.cpp:174 if (method == AC_METHOD_MEDIAN)`，负面断言现状）」
- 实现：`lib/infrastructure/scheduler/src/module_adapters.cpp:1956-1957`：`const int method = p1_int(c, "method", AC_METHOD_MEDIAN) == AC_METHOD_BILINEAR ? AC_METHOD_BILINEAR : AC_METHOD_MEDIAN;`
- 判定：**不符**
- 证据：模块层 `ac::interpolate_pixels` 的冻结行为是「非 0 即 IDW」（`cosmetic_corrector.cpp:174`），合同与冻结测试设计都据此断言；但生产节点把任何 `method != 1` 归一为 0 ⇒ `method=2` 在生产链走 **median** 而非 IDW。且该钳位无校验错误、无 manifest 留痕。
- 影响：配置面与合同面行为分叉；以合同/测试设计为 Oracle 的复核在生产链上不成立（正是本分片要抓的「规范对、实现没照做」型）。
- 修复面：`module_adapters.cpp:1956-1957`（删钳位并透传，或在 `config` validate 层显式拒绝非法 method）。**只登记，不改。**

### CONFORM-SWEEP-1-008 [C1/C5] calibrate manifest 的 `dark_scale` 在标准式记录 `k_fixed`（默认 1.0）而非实际施加的 `actual_k`

- 规范：`docs/algorithms/CALIBRATION_ALGORITHMS.md:150-154`「F3.2 否则（标准式, dark_opt==0）… `k = k_init` # K 必须由调用方给出（t_light/t_dark），不再强制 1.0 … `actual_k = k`」
  规范：`docs/algorithms/CALIBRATION_ALGORITHMS.md:167-174`（K/bias 退化对照表：「标准式 K≠1 … K=t_l/t_d → `(raw−bias−K·dark)/flat`」）
  规范：`docs/contracts/DATA_SEMANTICS.md:155`（K 行：标准式与兼容式都施加；B2-A13 消费边界由 EXPTIME 推导 K）
- 实现：`lib/infrastructure/scheduler/src/module_adapters.cpp:1899-1900`：`{"dark_scale", dark_opt ? static_cast<double>(actual_k) : static_cast<double>(k_fixed)}`
  算术侧：`:1862-1874` `ac_calibrate_frame(..., dark_opt ? 1 : 0, k_use, &actual_k)`，其中 `k_use = k_expo = t_light/t_dark`（`:1873`）
- 判定：**不符**
- 证据：`dark_opt=false`（默认分支）且 dark 在位时，算术用 `k_use=k_expo`，`actual_k` 回填为 `k_expo`；但 manifest 的 `per_frame[].dark_scale` 写 `k_fixed`（`doc.value("dark_scale_factor", 1.0f)`，`:1794`）。当 `t_light≠t_dark` 时 manifest 记 1.0 而实际施加 `t_l/t_d` ⇒ **溯源字段与实际算术不符**。
- 影响：manifest 是 RELEASE-02 的审计/溯源面（B2-A13/BIAS-001 都以 manifest 字段为判据）。标准式 K≠1 的每一帧都留下错误的 `dark_scale`，事后复核会认为「K=1，暗场未缩放」。测试 `tests/unit/p1001_real_nodes_test.cpp:1020` 只覆盖 K 分支（`dark_optimization=true`）故未拦住。
- 修复面：`module_adapters.cpp:1899-1900`（统一写 `actual_k`）。**只登记，不改。**

### CONFORM-SWEEP-1-009 [C3] cosmetic 节点恒传 `dark=nullptr, bias=nullptr` ⇒ 检测永久禁用、模块空转，却报 `status=ok`

- 规范：`docs/algorithms/COSMETIC_ALGORITHMS.md:132-136`「热检测: **当且仅当** `dark != NULL && hot_sigma > 0` … 冷检测: 当且仅当 `bias != NULL && cold_sigma > 0`」
  规范：`docs/algorithms/COSMETIC_ALGORITHMS.md:147-156`「两者皆关 → `all_bad` 全 0 … **模块退化为空转 pass** … 即**当前生产 cosmetic 阶段从未执行过真实检测/修复**（no fabrication of valid coverage）」
  规范：`docs/science/CALIBRATION.md:7`「**目的**：去除仪器签名（bias/dark/flat/cosmetic）」
- 实现：`lib/infrastructure/scheduler/src/module_adapters.cpp:1973-1975`：`const int rc = ac_correct_frame(im.px(), im.w(), im.h(), nullptr, nullptr, fixed.data(), hot_sigma, cold_sigma, method, mss, &hot, &cold);`；`:1997-2000` `st["status"]="ok"; st["hot_fixed"]=0; st["cold_fixed"]=0;`
- 判定：**不符**（C3 未接线）
- 证据：节点声明并解析 `hot_sigma/cold_sigma/method/max_structure_size`（`:1954-1958`），却把两个检测源母版恒置 nullptr ⇒ `all_bad≡0`、`interpolate_pixels` 恒等拷贝、`hot_fixed=cold_fixed=0`。节点以 `status=ok` 返回，manifest 无 `mode=disabled` 之类的显式降级标记（对比同函数 `enabled=false` 分支会写 `{"mode","disabled"}`，`:1946-1947`）。
- 影响：交付链上坏点从未被修复（SCI-CAL-001 §1 的 cosmetic 目标未达成），且失败是**静默**的——manifest 显示 cosmetic 阶段成功。星检/测光/SNR 全部建立在未做坏点修复的帧上。
- 修复面：`module_adapters.cpp:1962-1975`（接线 `master_dark/master_bias` 到 `ac_correct_frame`，或在无母版时写显式 `mode=disabled` 降级标记并让门禁可见）。**只登记，不改。**

### CONFORM-SWEEP-1-010 [C3/C6] 迁移模块 `astrocs_p1_cosmetic` 已 IMPLEMENTED 且入包，但生产节点绕过它调用 legacy C ABI

- 规范：`docs/algorithms/COSMETIC_ALGORITHMS.md:226-228`「迁移落点: `lib/algorithms/cosmetic/`（P1-COS-IMPL 建 astrocs_p1_cosmetic.dll + C ABI adapter + plan/execute/cancel/inspect + ThreadLease 接线）」
  规范：`docs/algorithms/CALIBRATION_ALGORITHMS.md:361-376`（P1-CAL-IMPL 迁移合同：plan/execute/cancel/inspect + ThreadLease，废除 `ac_set_num_threads` 全局 ICV）
- 实现：`lib/algorithms/cosmetic/src/module_entry.cpp`（1262 行，C ABI v1 九操作 adapter，`:802-816` 转发 `ac_correct_frame(_f64)`）；`CMakeLists.txt:229` `add_subdirectory(lib/algorithms/cosmetic)`；`packaging/astrocs.product.json:15` `{"unit_id":"MOD-P1-COS", ..., "module_id":"astrocs.p1.cosmetic", "status":"IMPLEMENTED"}`
  生产节点：`module_adapters.cpp:1973` 直接调 `ac_correct_frame`（legacy），全仓无任何 `astrocs_p1_cosmetic` 的加载点（`grep -rn astrocs_p1_cosmetic lib/infrastructure/scheduler/` 仅 descriptor/路径名，无 DLL 加载）
- 判定：**未实现**（就「迁移模块进入生产执行链」这一规范要求而言；算法本身已实现 ⇒ 同时命中 C6 有实现无调用）
- 证据：`module_entry.cpp:32-38` 自陈「manifest 无 inline 平面时按 legacy NULL 语义（检测禁用恒等路径, DISP-COS-009 冻结现状行为保持）」——即迁移模块本身也不做真实检测，而调度器根本不加载它。
- 影响：ThreadLease/取消点/异常屏障（DISP-COS-001/007/008 的整改项）在迁移模块里已落地，却在生产链上不生效；生产链仍走 legacy 全局 OpenMP ICV 路径。
- 修复面：`module_adapters.cpp:748-760`（descriptor）与节点 execute 面（改为经模块 ABI 执行），或登记「迁移模块暂不接线」并撤下 IMPLEMENTED 状态。**只登记，不改。**

### CONFORM-SWEEP-1-011 [C2] `kTrimMeanToSigma = 0.7316727929211932` 与解析值相差 −4.13e-7

- 规范：`docs/science/PSF.md:23`「`robust_residual_sigma` | `residual_scale/0.7316727929211932` | Gaussian 假设」；`:81`「`kTrimMeanToSigma=0.7316727929211932` 解析常数（`noise_model.cpp:35-37`）」；`:119`「数值由高斯分位积分确定」
  规范：`docs/algorithms/STAR_PSF_ALGORITHMS.md:56-57`「`robust_residual_sigma=residual_scale/0.7316727929211932`（`kTrimMeanToSigma=0.7316727929211932`, E[trimmed mean |r|]=0.731673σ, Gaussian）」
- 实现：`lib/algorithms/noise_snr/cpp/src/noise_model.cpp:37` `constexpr double kTrimMeanToSigma = 0.7316727929211932;`（`:322` `r.robust_residual_sigma = r.residual_scale / kTrimMeanToSigma;`）
- 判定：**不符**（C2；**已在册**，见 `问题扫描/_verify/V12.md` V12-N-04 / REBASE_TABLE V12-N-04，本分片独立复算确认）
- 证据：独立复算 `E[10–90% trimmed mean |r|] = (2/0.8)·[φ(Φ⁻¹0.55) − φ(Φ⁻¹0.95)]` ⇒ `python3 -c "from statistics import NormalDist as N; nd=N(); print(2*(nd.pdf(nd.inv_cdf(0.55))-nd.pdf(nd.inv_cdf(0.95)))/0.8)"` = `0.731673095280613`；在册值相对差 `-4.132438676152551e-07`（≈ −0.41 ppm，即第 7 位有效数字起错）。规范把该值同时写成 16 位与 7 位（`0.7316728`/`0.731673`）两种精度，且未声明分母域。
- 影响：`robust_residual_sigma` 与 `sigma_sky_adu`（`snr_estimator` 侧同常数）带 −0.41 ppm 尺度偏差；量级上远小于 001 的 1.914×，属「伪精度」类，但属同一「常数不符」缺陷类，且说明该常数无单一权威。
- 修复面：`lib/algorithms/noise_snr/cpp/src/noise_model.cpp:37`（与 `snr_science.cpp:38` 的第二份副本）、`docs/science/PSF.md:23,81,88,119`、`docs/algorithms/STAR_PSF_ALGORITHMS.md:56-57`（统一到解析值并声明积分域）。**只登记，不改。**

### CONFORM-SWEEP-1-012 [C7] ALG-STARPSF §1.1「布局 A」的 [7]/[8] 语义与批 API 实现不符

- 规范：`docs/algorithms/STAR_PSF_ALGORITHMS.md:14`「`[N,9]` (B, A, x0, y0, sx, sy, θ, **residual_scale, q_psf**)」；`:19-21`「residual_scale=ADU、q_psf=无量纲」
- 实现：`lib/algorithms/psf/src/dpsf_psf.cpp:1116-1117`（f64 批路径）`out_row[7] = result.fwhm_x; out_row[8] = result.fwhm_y;`（f32 批路径同式）
  实际布局与 `docs/contracts/DATA_SEMANTICS.md:609-617`「布局 B：批 API `out_psf_params`」一致：`[0]=B/[1]=A/[2]=cx/[3]=cy/[4]=sx/[5]=sy/[6]=theta/[7]=fwhm_x/[8]=fwhm_y`
- 判定：**不符**
- 证据：按 §1.1 解析会得到 `[7]=residual_scale(ADU)`、`[8]=q_psf(无量纲)`，实际是 `fwhm_x/fwhm_y(px)`；列单位与量纲完全不同（ADU vs px），属 ALG §1.2 自己警告的「全列错位」风险。
- 影响：任何以 ALG-STARPSF §1.1 为契约的消费者/测试 oracle 会取错列（把 FWHM 当残差尺度）。`module_adapters.cpp:2155-2167` 按 DATA_SEMANTICS 布局 B 正确解析，但两文档并存使新消费者无从选择。
- 修复面：`docs/algorithms/STAR_PSF_ALGORITHMS.md:14,19-21`（把 [7]/[8] 改为 fwhm_x/fwhm_y，或明确 §1.1 为历史/非批 API 布局）。**只登记，不改。**

### CONFORM-SWEEP-1-013 [C7] 两套互斥的「生产 PSF 块布局」同时被声明为权威

- 规范：`docs/algorithms/STAR_PSF_ALGORITHMS.md:25-42`「**布局 B：生产 PSF 块布局（编排序列化序，9 列，权威现状）** `[N,9]` (status, B, flux, cx, cy, fwhm, A, mad, eccentricity)」（锚 `orchestrator.cpp:2428-2464`）
  规范：`docs/contracts/DATA_SEMANTICS.md:609-617`「**布局 B：批 API `out_psf_params`** … `[0]=B/[1]=A/[2]=cx/[3]=cy/[4]=sx/[5]=sy/[6]=theta/[7]=fwhm_x/[8]=fwhm_y`」
  规范：`docs/contracts/DATA_SEMANTICS.md:589-605`（布局 A = orchestrator 生产块 `status,B,flux,cx,cy,fwhm,A,mad,eccentricity`）
- 实现：RELEASE-02 生产节点 `module_adapters.cpp:2155-2167` 写出的 `p1_psf.json.psf_params` 行 = `{star_id,B,A,cx,cy,sx,sy,theta,fwhm_x,fwhm_y,flux}` ⇒ 是 DATA_SEMANTICS 的**布局 B**，不是 ALG §1.2 声明的「权威现状」布局
- 判定：**不符**（C7；同时是规范内部打架）
- 证据：ALG §1.2 与 DATA_SEMANTICS §15 把同一个标签「布局 B」赋予**两套不同的列序**（前者 status-序，后者参数序），且各自称权威；RELEASE-02 生产节点与批 API 实现都只实现参数序。ALG §1.2 的锚 `orchestrator.cpp:2428-2464` 属另一条（ORCH-001 才刚进构建图的）通道，不是 RELEASE-02 的 CLI 生产链。
- 影响：PSF 块是 PHOTOMETRIC 的必需块（缺失退出码 3），列序歧义直接导致「status 当 B、fwhm 当 θ」这类科学结果错误（ALG §1.2 自己列的后果）。
- 修复面：`docs/algorithms/STAR_PSF_ALGORITHMS.md:25-47` 与 `docs/contracts/DATA_SEMANTICS.md:589-617`（统一标签与列序；RELEASE-02 生产链以参数序为准）。**只登记，不改。**

### CONFORM-SWEEP-1-014 [C1] 规范称饱和星 `mag = −2.5·log10(A_fit)`，生产 impl 对饱和星也用 box 和

- 规范：`docs/algorithms/STAR_DETECTION_ALGORITHMS.md:73-76`「星等（正常星，:2171-2198）: `mag = −2.5·log10(Σ_box(pixel − B_fit))` … **饱和星（:2175）: `mag = −2.5·log10(A_fit)`**（A>0），失败=NaN（量纲差异登记 DISP-STAR-003）」
  规范：`docs/contracts/DATA_SEMANTICS.md:754`「mag | float `[n]` | mag（正常星=`−2.5·log10(Σ_box(pixel−B_fit))`…；**饱和星=`−2.5·log10(A)`**，量纲差异=DISP-STAR-004）」
- 实现：`lib/algorithms/star_detection/src/sdet_api.cpp:2327-2348`（生产 `sdet_detect_impl` 阶段 8）：对**所有** StarRecord 无条件走 `box_sum += image[...] − local_B; rec.mag = (box_sum > 0.0) ? −2.5f*log10f((float)box_sum) : NAN;`；无饱和分支。`−2.5*log10(A)` 只存在于 `sdet_api.cpp:1548/1608`（legacy `sdet_detect_debug` 路径）
- 判定：**不符**
- 证据：`grep -n "log10" sdet_api.cpp` ⇒ 261(注释)/980(注释)/1548/1608(legacy)/2321(注释)/2348(生产)。生产 impl 内无 `log10(A_fit)`。故 DISP-STAR-003/004 登记的「饱和星 mag 与正常星 mag 量纲不一致（振幅 vs box 流量）」在**当前生产实现上已不存在**，规范/合同仍在断言它存在。
- 影响：以合同为 Oracle 的 mag 断言会失败；若下游按「饱和星 mag 是振幅量纲」解读 `p1_sources.json`，会与实现（box 流量量纲）不符。属「规范写对了、实现没照做」与「规范陈旧」的混合型，本分片按实现侧不符登记（规范同步订正归文档流程）。
- 修复面：`docs/algorithms/STAR_DETECTION_ALGORITHMS.md:75-76`、`docs/contracts/DATA_SEMANTICS.md:754`、`docs/algorithms/STAR_DETECTION_ALGORITHMS.md:247-249`（DISP-STAR-004 描述与现状对齐），或 `sdet_api.cpp:2327-2348` 补饱和分支。**只登记，不改。**

### CONFORM-SWEEP-1-015 [C3] PLATESOLVE §11.1 声明的生产通道与 RELEASE-02 生产节点不一致

- 规范：`docs/algorithms/PLATESOLVE.md:98-100`「**生产通道 = orchestrator PLATESOLVE 阶段直调 `ipv_solve_from_detections_v1`**（DLL=ipv_solver.dll …）；消费 PSF 产出 `star_measurements` 权威块」
  规范：`docs/algorithms/PLATESOLVE.md:131`「orchestrator 过滤+坐标契约 `orchestrator.cpp:1855-1876` … **+0.5 转换 :1867**（统一契约 index-is-center → IPV 接口契约 center=index+0.5）；sdet fallback 坐标已是 +0.5 契约（:1878）」
- 实现：`lib/infrastructure/scheduler/src/module_adapters.cpp:2792`（`StarDetectorHandle sdet`）、`:2812` `sdet_create(&sp)`、`:2831` `ipv_set_detector_handle(ipv, ...)`、`:2886` `ipv_solve_from_memory_with_callback_d(...)` —— 检测在 ipv 内部经 detector handle 触发，**不经** `star_measurements`、**无** adapter 层 +0.5 桥接（`:2900-2901` 的 `+kP1FitsPixelOrigin` 属初始指向采样路径，非检测路径）
- 判定：**不符**（规范陈旧；C3 就「按声明的通道与坐标桥接执行」而言）
- 证据：`module_adapters.cpp:2025-2030` 明文「`sdet` … 只被 wcs-platesolve 节点使用（`module_adapters.cpp:1995 sp.maxStars=2000`）」——与本文件的实际行号（`:2808`）都已漂移。规范描述的执行面（orchestrator + star_measurements + +0.5 桥接）在 RELEASE-02 CLI 生产链上不存在。
- 影响：坐标契约的可追溯性依赖 orchestrator 的显式 +0.5 桥接；改走 ipv 内部 sdet 后，该桥接是否存在只能靠 ipv 内部实现（PLATESOLVE §11.1 的「sdet fallback 坐标已是 +0.5 契约」一句承担），审计面丢失。若 sdet 侧实为 index-is-center（见 003），则解算输入带 0.5 px 系统偏移。
- 修复面：`docs/algorithms/PLATESOLVE.md:98-100,131,148-151`（按 RELEASE-02 生产节点重锚并显式声明坐标桥接位置）。**只登记，不改。**

### CONFORM-SWEEP-1-016 [C3] COSMETIC_ALGORITHMS 登记的「现网生产调用点」anchor 陈旧

- 规范：`docs/algorithms/COSMETIC_ALGORITHMS.md:150-156`「**现网生产调用点** `lib/phase1_session/p1_session.cpp:294-301`（cosmetic stage，2026-09-01 c5629be6 引入）以 `master_dark=nullptr, master_bias=nullptr` 调用 …」
  规范：`docs/algorithms/COSMETIC_ALGORITHMS.md:288`（DISP-COS-009 同锚）
- 实现：RELEASE-02 生产链的 cosmetic 执行面是 `lib/infrastructure/scheduler/src/module_adapters.cpp:1940-2004`（`p1_op_cosmetic`，注册于 `:8971` `{p1_cosmetic_descriptor(), {P1NodeOp::Cosmetic, "cosmetic_correct", "astrocs_phase1_cosmetic_v1"}}`），`nullptr` 传参在 `:1973`
- 判定：**不符**（规范陈旧；DISP-COS-009 的**事实**成立但**锚点**错位）
- 证据：`p1_session.cpp` 是另一条（session）执行面；RELEASE-02 CLI 走 scheduler 节点表。任何按规范锚点去核对的复核者会检查错误的文件。
- 影响：DISP-COS-009「检测从未在生产生效」这一关键结论挂在失效锚上，容易被误判为「已过时/已修复」。
- 修复面：`docs/algorithms/COSMETIC_ALGORITHMS.md:150-156,288`（改锚 `module_adapters.cpp:1973`）。**只登记，不改。**

### CONFORM-SWEEP-1-017 [C6] `sp.fwhmClipSigma` 在 `p1_op_wcs` 设值但零消费

- 规范：`docs/algorithms/STAR_DETECTION_ALGORITHMS.md:215-221`「SDetParams 9 字段（`star_detector.h:13-24`）生产消费面（DISP-STAR-003）: … `fwhmClipSigma` 仅 debug 入口消费（:1450-1452，**impl 阶段8 已移除全局 FWHM clip** :2140）」
- 实现：`lib/infrastructure/scheduler/src/module_adapters.cpp:2810` `sp.fwhmClipSigma = 3.0f;`；`sdet_api.cpp` 生产 `sdet_detect_impl` 无任何 `fwhmClipSigma` 读取（`grep -n fwhmClipSigma sdet_api.cpp` 仅 legacy `sdet_detect_debug` 路径）
- 判定：**不符**（C6 有实现无调用/死参数；DISP-STAR-003 已登记该缺口，本项补 RELEASE-02 生产锚）
- 证据：节点显式设置该字段 ⇒ 读码者会以为 FWHM clip 生效；实际生产 impl 阶段 8 只靠 `reject_star` 的自适应上限（`sdet_api.cpp:242-245`）。
- 影响：诊断/复核面误导；无科学数值影响（该参数确实不生效）。
- 修复面：`module_adapters.cpp:2810`（删或接线）。**只登记，不改。**

---

## 3 规范侧问题（另一类：规范本身错 / 陈旧 / 自相矛盾）

### CONFORM-SWEEP-1-018 [规范错·内部打架] ALG-STARPSF §3 伪代码 Moffat4 初值 `sx=sy=1.2` 与实现 `sx0=0.15·rw` 冲突，且 §11.1 未登记该出入

- 规范：`docs/algorithms/STAR_PSF_ALGORITHMS.md:72`「init B=median(patch), A=max−B, x0=y0=0, **sx=sy=1.2**, θ=0」
  规范：`docs/algorithms/STAR_PSF_ALGORITHMS.md:144`「初值链 bkg0 中位/A0=max−B/params={`bkg0,A0,0,0,sx0,sx0,0`}」（只给符号 `sx0`，**不给数值**）
- 实现：`lib/algorithms/psf/src/dpsf_psf.cpp:368` `double sx0 = 0.15 * rw;`（`rw`=rect 宽；fitRadius=8 ⇒ rw≈17 ⇒ sx0≈2.55）
- 判定：**规范歧义 / 规范错**
- 证据：§11.1 只自陈了 LM 参数（tol/iter）与 §3 的出入（`STAR_PSF_ALGORITHMS.md:158-163`），**未登记** sx0；`0.15·rw` 在 `docs/` 全库无出处（`grep -rn "0.15 \* rw\|sx0" docs/` 仅命中 §11.1 的符号行）。初值决定 LM 落入哪个局部极小，属科学相关量。
- 影响：以 §3 伪代码为 Oracle 复算 LM 收敛路径会与生产不符；初值口径无权威来源。
- 修复面：`docs/algorithms/STAR_PSF_ALGORITHMS.md:72,144`（订正 §3 并登记 `sx0=0.15·rw` 或改为规范值）。**只登记，不改。**

### CONFORM-SWEEP-1-019 [规范错] ALG-STARPSF §4「NaN/Inf patch 跳过拟合 status=BAD」无对应实现

- 规范：`docs/algorithms/STAR_PSF_ALGORITHMS.md:91`「| 图像含 NaN/Inf | 该 patch 跳过拟合，status=BAD |」
- 实现：`lib/algorithms/psf/src/dpsf_psf.cpp:281-304` 采样阶段**跳过**非有限像素（`n_nonfinite` 计数 + LOG_WARN），`m==0` 才返回 `DPSF_FIT_INVALID_PARAMS`；状态码只有 `OK=0/NO_CONVERGENCE=1/INVALID_PARAMS=2/ITERATION_LIMIT=3`（`dynamic_psf.h:33-36`），**无 BAD**
- 判定：**规范错**（§11.1 已自陈「BAD 码不存在」，但 §4 正文未订正）
- 证据：`STAR_PSF_ALGORITHMS.md:160-163`「§4 "NaN/Inf patch 跳过 status=BAD" 无对应实现：BAD 码不存在 …」——规范自陈矛盾仍留在正文表中。
- 影响：测试设计若按 §4 断言 BAD 码将无法实现；NaN 帧的实际语义（逐像素跳过、非整 patch 跳过）未在 §4 反映。
- 修复面：`docs/algorithms/STAR_PSF_ALGORITHMS.md:91`。**只登记，不改。**

### CONFORM-SWEEP-1-020 [规范歧义] reject_star 的 FWHM 上限公式在规范中无出处

- 规范：`docs/algorithms/STAR_DETECTION_ALGORITHMS.md:112-114`「if reject_star(fit) ≠ OK: drop（FWHM>0.5、圆度≥0.5、RMSE=mad·1.482602218505602/A≤0.2、**FWHM 上限**; 饱和豁免 RMSE）」（未给上限公式）
- 实现：`lib/algorithms/star_detection/src/sdet_api.cpp:238-245`：`fwhm_limit = se_smax * 2.3548200450309493 * (1.0 + 0.5*std::log(se_smax/2.0)); if (fit.fwhm_x > fwhm_limit || fit.fwhm_y > fwhm_limit) return SF_FWHM_TOO_LARGE;`
- 判定：**规范歧义**
- 证据：`grep -rn "fwhm_limit\|log(se_smax" docs/` ⇒ 0 命中。三个常数（`2.3548200450309493`、`0.5`、`KERNEL_SIZE=2.0`）均无规范出处；公式形态（对数放宽）也无推导记录。
- 影响：该门直接决定星是否被丢弃（`sdet_api.cpp:2300` `f_fwhm`），是无规范依据的科学判据；宽星（如 NGC6302 FWHM 7–33）是否被误杀无法用规范复核。
- 修复面：`docs/algorithms/STAR_DETECTION_ALGORITHMS.md:112-114` 或 `docs/algorithms/GATES_AND_TOLERANCES.md`（补公式与出处），或 `sdet_api.cpp:243`（改为规范值）。**只登记，不改。**

### CONFORM-SWEEP-1-021 [规范错·陈旧] COSMETIC_ALGORITHMS §0 称 `lib/algorithms/cosmetic/`「尚未存在生产符号」，实际已落码并入包

- 规范：`docs/algorithms/COSMETIC_ALGORITHMS.md:8-9`「迁移目标目录 `lib/algorithms/cosmetic/`（落码由 P1-COS-IMPL 执行，**尚未存在生产符号**）」；`:13`「禁止声明 IMPLEMENTED」
- 实现：`lib/algorithms/cosmetic/src/module_entry.cpp`（1262 行，C ABI v1 adapter）；`lib/algorithms/cosmetic/src/astrocs_p1_cosmetic.def`；`CMakeLists.txt:223-229`；`packaging/astrocs.product.json:15` `"status": "IMPLEMENTED"`
- 判定：**规范错**（陈旧）
- 证据：模块已编译（`build/CMakeFiles/TargetDirectories.txt:73`）、已入安装清单（`build/install_manifest.txt:9` `modules/astrocs_p1_cosmetic.so`）、已被打包清单标 IMPLEMENTED。
- 影响：文档与打包状态冲突；且 §10 DISP-COS 清单仍以 legacy 路径为「现状唯一生产实现」，掩盖了「双实现并存 + 迁移模块未接线」（见 010）。
- 修复面：`docs/algorithms/COSMETIC_ALGORITHMS.md:4-13,20-31,226-228`。**只登记，不改。**

### CONFORM-SWEEP-1-022 [规范歧义] ALG-STARDET-001 行号锚漂移

- 规范：`docs/algorithms/STAR_DETECTION_ALGORITHMS.md:201`「`reject_star` | **:189-239**」；`:40`「norm 硬编码 65535（**:1689**，DISP-STAR-006）」
- 实现（实测）：`lib/algorithms/star_detection/src/sdet_api.cpp:209-247`（`SfError reject_star(...)` 起于 209，止于 247）；`65535` 出现在 `:1834`（`const float norm = 65535.0f;`）与 `:2203`（注释），**:1689 无 65535**
- 判定：**规范歧义**（锚漂移，非语义错）
- 证据：`grep -n "65535" sdet_api.cpp` ⇒ 1834/2203；`grep -n "SfError reject_star" sdet_api.cpp` ⇒ 209。文档头自陈「2555 行实测，2026-09-17 LEDGER-DOC 按附录 G 机械重锚」，但 §2 的若干行内锚未随重锚刷新。
- 影响：锚点失效削弱「规范可机械复核」的可信度；不影响科学语义。
- 修复面：`docs/algorithms/STAR_DETECTION_ALGORITHMS.md:40,201` 等行内锚。**只登记，不改。**

### CONFORM-SWEEP-1-023 [规范歧义] ALG-STARPSF §3 的 LM 参数（iter≤50 / tol=1e-6）与 §11.1/实现（200 / 1e-8）冲突

- 规范：`docs/algorithms/STAR_PSF_ALGORITHMS.md:73`「LM 7参 Levenberg-Marquardt (`dpsf_psf.cpp:lm_solve`) **iter≤50 tol=1e-6**」
  规范：`docs/algorithms/STAR_PSF_ALGORITHMS.md:145`「LM 调用 **tol=1e-8 / max_iter=200**（硬编码）」
- 实现：`lib/algorithms/psf/src/dpsf_psf.cpp:374-375` `lm_solve(m, NPARAMS, params, ..., 1e-8, 200);`
- 判定：**规范歧义**（§11.1 已自陈「§3 为旧稿」，但 §3 正文未订正 ⇒ 同一文档两套冻结值）
- 证据：`STAR_PSF_ALGORITHMS.md:158-160`「与 §3 伪代码的出入（如实登记，不改 §3）：实测 LM 参数为 tol=1e-8、max_iter=200（:320-321），§3 "iter≤50 tol=1e-6" 为旧稿」
- 影响：收敛判据差 2 个数量级，直接影响拟合质量与 ITERATION_LIMIT 触发率；以 §3 复算的 Oracle 结论不可比。
- 修复面：`docs/algorithms/STAR_PSF_ALGORITHMS.md:73`。**只登记，不改。**

### CONFORM-SWEEP-1-024 [规范歧义] `flux` 列名承载「振幅」语义，全仓三套 flux 定义并存

- 规范：`docs/contracts/DATA_SEMANTICS.md:753`「flux | float `[n]` | ADU（正常星=**检测侧椭圆高斯峰值振幅 A**，非解析积分流量…）」
  规范：`docs/science/PSF.md:52,87`「`flux = 2πA·sxsy/3`，单位 ADU」
  规范：`docs/algorithms/STAR_DETECTION_ALGORITHMS.md:20`「输出: 逐星 `(cx,cy,**flux**,mag,saturated,has_saturated)` 十数组」（未定义 flux）
- 实现：三处三义——`sdet_api.cpp:2304` `rec.flux = (float)fit_results[i].A;`（振幅）；`wrapper_phase1/star_detector.cpp:151` `s.flux = m00;`（5×5 盒和）；`dpsf_psf.cpp:429` `flux = 2πA·sxsy/3`（解析通量）
- 判定：**规范歧义**（同名不同物；DATA_SEMANTICS 只覆盖 sdet 一路，未覆盖生产 wrapper 与 PSF 解析通量）
- 证据：三个生产者写出的 `flux` 分别进入 `p1_sources.json`（DATA-P1-SOURCES，无列语义合同）、`p1_psf.json`、`p1_flux.json`（DATA-P1-FLUX）；`docs/contracts/DATA_SEMANTICS.md` 无 DATA-P1-SOURCES 的列语义表（`grep -n "DATA-P1-SOURCES" DATA_SEMANTICS.md` 仅 3 处引用、无表）。
- 影响：跨产品连接 `flux` 时无权威口径；SNR 的 `F_ref`、测光的 `F_instr` 都可能取到非流量量（见 004）。
- 修复面：`docs/contracts/DATA_SEMANTICS.md`（新增 DATA-P1-SOURCES 列语义表）、`docs/algorithms/STAR_DETECTION_ALGORITHMS.md:20`。**只登记，不改。**

---

## 4 全量陈述清点表（逐条编号）

> 判定列：`符合` / `不符` / `未实现` / `歧义`（规范歧义或规范内部打架） / `待定`。
> 标 ★ 者为 §2/§3 展开条目。

### 4.1 `docs/science/CALIBRATION.md`（SCI-CAL-001）

| 编号 | 规范锚 | 陈述摘要 | 实现锚 | 判定 |
|---|---|---|---|---|
| S-001 | CALIBRATION.md:66 | `cal=(raw−bias−K·dark)/max(flat,0.1)`（标准式） | calibrator.cpp:132-141 | 符合 |
| S-002 | CALIBRATION.md:69 | `cal=(raw−bias−K·(dark−bias))/max(flat,0.1)`（兼容式） | calibrator.cpp:124-131 | 符合 |
| S-003 | CALIBRATION.md:71 | `K=t_light/t_dark`，两分支同一 K | calibrator.cpp:122,144；module_adapters.cpp:1873,1867 | 符合 |
| S-004 | CALIBRATION.md:73 | `flat_norm=max(flat/median(flat),0.1)`；median≤0 不归一 | calibrator.cpp:85-100；master_generator.cpp:255-288 | 符合 |
| S-005 | CALIBRATION.md:78-79 | 两分支都必须施加 K（旧实现只在 dark_opt=1 施加属缺陷） | calibrator.cpp:128,138 | 符合 |
| S-006 | CALIBRATION.md:117 | 缺 EXPTIME 时不得静默取 K=1 | module_adapters.cpp:1807-1811,1838-1848 | 符合 |
| S-007 | CALIBRATION.md:127 | `median(flat)≤0` → 不归一保持原样 | calibrator.cpp:91 | 符合 |
| S-008 | CALIBRATION.md:143 | MAD→σ 常数 `1.482602218505602`（float 域舍入 1.4826022f） | master_generator.cpp:76；cosmetic_corrector.cpp:43 | 符合 |
| S-009 | CALIBRATION.md:133-136 | 标度/归一化 fail-closed（U1/U2/U3） | module_adapters.cpp:1669-1771 | 符合 |
| S-010 | CALIBRATION.md:154 | 负值保留不 clamp | calibrator.cpp:136-141（无 clamp） | 符合 |
| S-011 | CALIBRATION.md:156 | `bad_mask` 极性 1=坏点 | cosmetic_corrector.cpp:126-128,147-149 | 符合 |
| S-012 | CALIBRATION.md:120 | 单帧 master 直接拷贝不 sigma-clip | master_generator.cpp:84-88 | 符合 |

### 4.2 `docs/algorithms/CALIBRATION_ALGORITHMS.md`（ALG-CAL-001..006）

| 编号 | 规范锚 | 陈述摘要 | 实现锚 | 判定 |
|---|---|---|---|---|
| S-013 | CALIBRATION_ALGORITHMS.md:82-85 | σ=1.4826·mad；非对称 clip；σ≤0 break | master_generator.cpp:119-132 | 符合 |
| S-014 | CALIBRATION_ALGORITHMS.md:90 | mean 合并按帧下标升序 FP32 累加 | master_generator.cpp:149-153 | 符合 |
| S-015 | CALIBRATION_ALGORITHMS.md:104-112 | 逐帧 `dst=max(dst/frame_med,0.1)`；负/全 NaN fail-closed | master_generator.cpp:196-246 | 符合 |
| S-016 | CALIBRATION_ALGORITHMS.md:114-121 | 最终归一 + 负 median 拒绝 | master_generator.cpp:255-288 | 符合 |
| S-017 | CALIBRATION_ALGORITHMS.md:147-154 | F3.1/F3.2 双分支与 `actual_k=k` | calibrator.cpp:124-144 | 符合（算术）/ **不符**（manifest 侧 ★008） |
| S-018 | CALIBRATION_ALGORITHMS.md:157-162 | bias 两分支都出现；K 两分支都施加 | calibrator.cpp:128,137-138 | 符合 |
| S-019 | CALIBRATION_ALGORITHMS.md:167-177 | K/bias 退化对照表 9 行 | module_adapters.cpp:1793-1920 | 符合（8/9 行）；★008 涉 manifest |
| S-020 | CALIBRATION_ALGORITHMS.md:185-188 | `normalize_flat` 无生产调用方（DISP-CAL-007） | calibrator.cpp:85-100（无调用者） | 符合 |
| S-021 | CALIBRATION_ALGORITHMS.md:197-199 | 热/冷像素判据 `med±sigma·σ` | cosmetic_corrector.cpp:118-155 | 符合 |
| S-022 | CALIBRATION_ALGORITHMS.md:202-203 | 连通域 `size>=max_size` 清零 | cosmetic_corrector.cpp:95-101 | 符合 |
| S-023 | CALIBRATION_ALGORITHMS.md:205-211 | median 5×5 镜像；method=1 实为 4 方向 IDW | cosmetic_corrector.cpp:174-224 | 符合（模块层）；节点层 ★007 |
| S-024 | CALIBRATION_ALGORITHMS.md:227-232 | dark_optimizer 背景/分区/回归公式 | dark_optimizer.cpp（未编译） | 符合（未接线已登记） |
| S-025 | CALIBRATION_ALGORITHMS.md:252-260 | `apply_photometry` 公式与错误码 | photometry_apply.cpp:29-65 | 符合（未接线已登记） |

### 4.3 `docs/science/STAR_DETECTION.md`（SCI-P1-STAR-001）

| 编号 | 规范锚 | 陈述摘要 | 实现锚 | 判定 |
|---|---|---|---|---|
| S-026 | STAR_DETECTION.md:12-15 | 亚像素质心为连续估计；|Δc|≤0.3px | wrapper:139-152（矩质心，连续） | 待定（连续估计成立，但 0.3px 容差在交付链上无机器门、从未验收，见 ★002） |
| S-027 | STAR_DETECTION.md:19 | `threshold=median(img)+5.0·bgnoise` | wrapper:82（bg 非 median(img)，bgnoise 非行差分） | 待定（★002 下不可比） |
| S-028 | STAR_DETECTION.md:20-23 | 饱和双条件 `0.7/0.1·dynrange` | wrapper:167（50000 字面量） | **不符** ★005 |
| S-029 | STAR_DETECTION.md:22-23 | 距边界 <2px **允许**丢弃 | wrapper:143（5×5 出界置 quality\|=2 边缘位，保留该星） | 符合（规范为「允许」非「必须」；实现选择保留并置位，未违反许可条款） |
| S-030 | STAR_DETECTION.md:24-26 | 输出按 mag 升序全序确定，NaN 末尾 | wrapper:115-118（按 val 降序排候选，输出序=候选序） | 规范歧义（该排序要求定义在 DATA-P1-STAR/sdet 块；交付链产品 DATA-P1-SOURCES 无排序合同，实际为亮度降序，见 ★024） |
| S-031 | STAR_DETECTION.md:31-34 | 检测侧高斯 / PSF 侧 Moffat4，宽度列不可跨块比较 | snr_science.cpp:37-42 跨块混用 | **不符** ★001 |
| S-032 | STAR_DETECTION.md:35-37 | 正常星 `mag=−2.5log10(Σ_box(pixel−B_fit))` | sdet_api.cpp:2342-2348；wrapper 无 mag 列 | 规范歧义（sdet 侧符合；交付链产品 DATA-P1-SOURCES 不产出 mag，规范未规定该产品的 mag 语义，见 ★002/★024） |
| S-033 | STAR_DETECTION.md:38-40 | 像素中心=索引+0.5 | wrapper:146-152（index-is-center） | **不符** ★003 |
| S-034 | STAR_DETECTION.md:46-49 | 生产路径 `sdet_detect_impl`（sdet_api.cpp） | module_adapters.cpp:2050（wrapper） | **不符** ★002 |
| S-035 | STAR_DETECTION.md:68 | `σ=FWHM/(2√(2ln2))` | wrapper:162；snr_science.cpp:41（Moffat 反解） | 符合（检测侧）/ ★001 |
| S-036 | STAR_DETECTION.md:16-18 | 召回≥99%@SNR≥10；虚警≤0.1/千像素 | 无机器门锚 | 待定 |
| S-037 | STAR_DETECTION.md:66-67 | 质心/容差单位 px（0-based 索引+0.5 中心） | 同 S-033 | **不符** ★003 |

### 4.4 `docs/algorithms/STAR_DETECTION_ALGORITHMS.md`（ALG-STARDET-001）

| 编号 | 规范锚 | 陈述摘要 | 实现锚 | 判定 |
|---|---|---|---|---|
| S-038 | STAR_DETECTION_ALGORITHMS.md:8-9 | 唯一权威生产源 = sdet_api.cpp | module_adapters.cpp:2050 | **不符** ★002 |
| S-039 | :33-35 | bgnoise=行差分+3 轮 5σ clip+×0.7071 | sdet_api.cpp:462-515 | 符合（sdet 侧） |
| S-040 | :36 | `threshold=median(img)+5.0·bgnoise` | sdet_api.cpp:1790 | 符合（sdet 侧） |
| S-041 | :37-40 | dynrange/minsatlevel/satrange/locthreshold/norm=65535 | sdet_api.cpp:1834-1838 | 符合（sdet 侧）；锚漂移 ★022 |
| S-042 | :41-42 | 饱和双条件 | sdet_api.cpp:1918 | 符合（sdet 侧） |
| S-043 | :43-44 | 一阶导质心 `−0.5−d1rl/(d1rr−d1rl)` | sdet_api.cpp:1927-1928 | 符合 |
| S-044 | :45-47 | 饱和 edge-walking + <2px 丢弃 | sdet_api.cpp:1933-1988 | 符合 |
| S-045 | :48-51 | 二阶导零交叉 Sr/Sc/Ar/Ac | sdet_api.cpp:1991-2068 | 符合 |
| S-046 | :52-54 | `s_factor=3.7172`；R 上限 200；边界收缩 | sdet_api.cpp:1823,2071-2081 | 符合 |
| S-047 | :55-57 | 对称性门 `dA>2∨dSr>2∨dSc>2∨max<locthreshold` | sdet_api.cpp:2084-2095 | 符合 |
| S-048 | :58-59 | `matchradius=max(1,⌊0.2R⌋)` 曼哈顿去重 | sdet_api.cpp:2099-2108 | 符合 |
| S-049 | :60-64 | 椭圆高斯 7 参模型；`fwhm=2.3548σ` | sdet_api.cpp:111-147,412-413 | 符合 |
| S-050 | :65-66 | `theta=−alpha` 归一到 (−90°,90°]；`dx=x+0.5−cx` | sdet_api.cpp:421-430,548-549 | 符合 |
| S-051 | :67-70 | 初值链 halfA/SX_init/fr_init/max_iter | sdet_api.cpp:313-368 | 符合 |
| S-052 | :71-72 | 饱和 mask；m≤8 → INVALID_PARAMS | sdet_api.cpp:542,557-561 | 符合 |
| S-053 | :73-76 | 正常星 box mag；饱和星 `−2.5log10(A)` | sdet_api.cpp:2342-2348（无饱和分支） | **不符** ★014 |
| S-054 | :77-78 | `is_saturated=(A>dynrange)`；`has_saturated=is_saturated` | sdet_api.cpp:2309,2353 | 符合 |
| S-055 | :79-82 | mag 升序 stable_sort；dedup 三规则 | sdet_api.cpp:864-995 | 符合 |
| S-056 | :88-92 | 伪代码 smooth σ=2.0 / 7 步 peaker | sdet_api.cpp:1623-1633,1709-1974 | 符合 |
| S-057 | :112-114 | reject_star 四门（FWHM 上限公式未给） | sdet_api.cpp:209-247 | 歧义 ★020 |
| S-058 | :215-221 | SDetParams 9 字段消费面；fwhmClipSigma 半失效 | sdet_api.cpp（impl 不消费） | 符合（sdet 侧）；节点死设 ★017 |
| S-059 | :236-261 | DISP-STAR-001..007 登记 | — | 符合（登记存在）；DISP-003/004 描述已陈旧 ★014 ★017 |
| S-060 | :265-282 | TEST-STAR-DESIGN-001 F1–F6 冻结设计 | 未在交付链验收（★002） | 待定 |

### 4.5 `docs/science/PSF.md`（SCI-PSF-001）

| 编号 | 规范锚 | 陈述摘要 | 实现锚 | 判定 |
|---|---|---|---|---|
| S-061 | PSF.md:43 | `I(r)=B+A/(1+Q)^4` | dpsf_psf.cpp:113-115 | 符合 |
| S-062 | PSF.md:44-47 | Q 与 p1/p2/p3 定义 | dpsf_psf.cpp:96-101 | 符合 |
| S-063 | PSF.md:48 | `dx=x−(cx+x0)` | dpsf_psf.cpp:104-105,295-296,437 | 符合 |
| S-064 | PSF.md:50-51 | 各向同性 `FWHM=1.230310·σ` | dpsf_psf.cpp:25,393-394 | 符合 |
| S-065 | PSF.md:52 | `flux=2πA·sxsy/3` | dpsf_psf.cpp:428-429 | 符合 |
| S-066 | PSF.md:22-23 | `residual_scale`=10–90% 截尾均值；`robust_residual_sigma=/0.7316727929211932` | dpsf_psf.cpp:222-254；noise_model.cpp:37,322 | 符合（结构）/ 常数 ★011 |
| S-067 | PSF.md:24 | `q_psf=A/residual_scale` | dpsf_psf.cpp（**无 q_psf 输出**） | **不符**（q_psf 未产出，DPSFFitResult 无该字段） |
| S-068 | PSF.md:28 | 单位表（A/B=ADU；σ/sx/sy/fwhm=px；θ=rad；flux=ADU） | dynamic_psf.h:16-31；dpsf_psf.cpp:445-457 | 符合 |
| S-069 | PSF.md:36-38 | 输入有效域与拒门 | dpsf_psf.cpp:271-279,363-366,386-391,404 | 符合 |
| S-070 | PSF.md:63 | FWHM/σ 比值恒 1.230310 | dpsf_psf.cpp:25,393 | 符合 |
| S-071 | PSF.md:65 | θ 四候选消歧 | dpsf_psf.cpp:411-423 | 符合 |
| S-072 | PSF.md:81 | `kTrimMeanToSigma` 解析常数 | noise_model.cpp:37 | 歧义/不符 ★011 |
| S-073 | PSF.md:87-88 | flux 单位 ADU；q_psf 非 SNR | dpsf_psf.cpp:429；snr 侧不消费 q_psf | 符合 |
| S-074 | PSF.md:92-94 | 不可接受变化：β≠4、q_psf 当 SNR、本模块高斯主路径 | dpsf_psf.cpp（β=4 唯一路径） | 符合 |

### 4.6 `docs/algorithms/STAR_PSF_ALGORITHMS.md`（ALG-STARPSF-001）

| 编号 | 规范锚 | 陈述摘要 | 实现锚 | 判定 |
|---|---|---|---|---|
| S-075 | STAR_PSF_ALGORITHMS.md:14,19-21 | 布局 A `[7]=residual_scale,[8]=q_psf` | dpsf_psf.cpp:1116-1117（fwhm_x/y） | **不符** ★012 |
| S-076 | :25-42 | 「布局 B=生产块（status 序）权威现状」 | module_adapters.cpp:2155-2167（参数序） | **不符** ★013 |
| S-077 | :52-58 | F1–F5 公式 | dpsf_psf.cpp:96-115,393-394,429,222-254 | 符合 |
| S-078 | :72-74 | init `sx=sy=1.2`；LM iter≤50 tol=1e-6 | dpsf_psf.cpp:368,374-375 | 歧义 ★018 ★023 |
| S-079 | :76-79 | θ 消歧；guards sx/sy>0；MAD==0 不换算 | dpsf_psf.cpp:386-391,411-423 | 符合 |
| S-080 | :89-96 | §4 边界表（NaN→BAD；饱和掩膜） | dpsf_psf.cpp:281-304（无 BAD） | 规范错 ★019 |
| S-081 | :140-156 | §11.1 实现锚表 | dpsf_psf.cpp 逐条实测 | 符合（个别锚漂移） |
| S-082 | :167-172 | §11.2 四状态码语义 | dpsf_psf.cpp:1104-1122；dynamic_psf.h:33-36 | 符合 |
| S-083 | :182-187 | DISP-PSF-001..006 登记 | — | 符合（登记存在） |

### 4.7 `docs/algorithms/COSMETIC_ALGORITHMS.md`（ALG-COS-001..005）

| 编号 | 规范锚 | 陈述摘要 | 实现锚 | 判定 |
|---|---|---|---|---|
| S-084 | COSMETIC_ALGORITHMS.md:43 | `σ=1.482602218505602·mad` | cosmetic_corrector.cpp:43 | 符合 |
| S-085 | :45-48 | 热判据 `dark>med+hot_sigma·σ` | cosmetic_corrector.cpp:126-128 | 符合 |
| S-086 | :47-48 | 冷判据 `bias<med−cold_sigma·σ` | cosmetic_corrector.cpp:147-149 | 符合 |
| S-087 | :49-51 | `dark==NULL`→返回且不清零 mask | cosmetic_corrector.cpp:121-123 | 符合 |
| S-088 | :63-75 | 8 邻接连通域；`size>=max_size` 清零；背景 label 0 保护 | cosmetic_corrector.cpp:64-111 | 符合 |
| S-089 | :95-105 | median 5×5 镜像；空邻域保留原值 | cosmetic_corrector.cpp:174-199 | 符合 |
| S-090 | :106-114 | method=1 = 4 方向 `1/dist` IDW | cosmetic_corrector.cpp:201-224 | 符合 |
| S-091 | :132-136 | 热/冷检测启用条件 | module_adapters.cpp:1973（恒 nullptr） | **不符** ★009 |
| S-092 | :141-145 | `out_hot/out_cold`=过滤后计数 | cosmetic_corrector.cpp:258-264 | 符合 |
| S-093 | :150-156 | 现网生产调用点 = p1_session.cpp:294-301 | module_adapters.cpp:1973 | **不符** ★016 |
| S-094 | :174-179 | 双中位均值定义 | cosmetic_corrector.cpp:34-43 | 符合 |
| S-095 | :226-228 | 迁移落点 lib/algorithms/cosmetic（P1-COS-IMPL 执行） | module_entry.cpp 已落码但未接线 | **未实现** ★010 |
| S-096 | :8-9,13 | 「尚未存在生产符号」「禁止声明 IMPLEMENTED」 | packaging/astrocs.product.json:15 IMPLEMENTED | 规范错 ★021 |
| S-097 | :264-267 | 非法 method → IDW（负面断言现状） | module_adapters.cpp:1956-1957（→median） | **不符** ★007 |

### 4.8 `docs/algorithms/PLATESOLVE.md`（ALG-WCS-001）

| 编号 | 规范锚 | 陈述摘要 | 实现锚 | 判定 |
|---|---|---|---|---|
| S-098 | PLATESOLVE.md:14 | `CRPIX=w/2+0.5,h/2+0.5`（1-based） | ipv_wcs.cpp:264-277 | 符合 |
| S-099 | :15 | `CD=trans.linear/3600`；`cd_inv=inv(trans.linear)` | ipv_wcs.cpp:256-266,322-325 | 符合 |
| S-100 | :16 | `A[i][j]=cd_inv·trans.x_ij` 解析 | ipv_wcs.cpp:322-365 | 符合 |
| S-101 | :17-19 | AP/BP 采样网格 ≥7×7；41×41/81×81（DISP-WCS-008） | ipv_wcs.cpp:417,541 | 符合 |
| S-102 | :20 | Y-down 符号规则 | ipv_wcs.cpp:528-576 | 符合 |
| S-103 | :31-35 | scale_tol=0.002；tol=5.0"；conv 0.01"；Huber 1.345 | ipv_solver.cpp:458,864,1213；ipv_sip.cpp:241 | 符合 |
| S-104 | :49 | 极区保守剪枝 `C=π/2 / C45=π/(2√2)` | ipv_select.cpp（polar prune） | 符合 |
| S-105 | :98-100 | 生产通道 = orchestrator + star_measurements | module_adapters.cpp:2792-2886 | **不符** ★015 |
| S-106 | :131,148-151 | +0.5 像素中心双契约桥接 | module_adapters.cpp（无 adapter 层桥接） | **不符** ★015 |
| S-107 | :86 | 容差来源（0.01"/0.002/1.345 预冻结） | 同 S-103 | 符合 |
| S-108 | :185-192 | DISP-WCS-008 网格口径订正 | ipv_wcs.cpp:417,541 | 符合 |

---

## 5 常数（魔数）清单与出处回查

> 范围为分片内实现的**全部数值字面量**（按文件），逐条回查 `docs/` 是否有出处、值是否一致。

| 常数 | 值 | 实现锚 | 规范出处 | 判定 |
|---|---|---|---|---|
| MAD→σ | `1.482602218505602` | sdet_api.cpp:226,590；dpsf_psf.cpp:343；master_generator.cpp:76；cosmetic_corrector.cpp:43；wrapper:51 | CALIBRATION.md:143；NOISE_MODEL | 符合（float 域 `1.4826022f` 已声明 +1.36e-08） |
| 高斯 FWHM 因子 | `2.3548200450309493` | sdet_api.cpp:48,57,238,412；wrapper:162(`2.3548`) | ALG-STARDET §2（`2.3548`） | 符合（规范给舍入值） |
| Moffat4 FWHM 因子 | `1.230310` | dpsf_psf.cpp:25；snr_science.cpp:37 | PSF.md:51 | 符合（值对；**用法**错见 ★001） |
| trimmed-mean→σ | `0.7316727929211932` | noise_model.cpp:37；snr_science.cpp:38 | PSF.md:23,81,88,119；STAR_PSF_ALGORITHMS.md:56-57 | **不符** ★011（解析 0.731673095280613） |
| `1/√2`（行差分→σ） | `0.70710678` | sdet_api.cpp:514 | ALG-STARDET §2（`×0.7071`） | 符合（规范为 `1/√2` 舍入） |
| `√e`（振幅估计） | `1.6487212707` | sdet_api.cpp:1819 | ALG-STARDET §2（`SQRT_EXP1=√e`） | 符合 |
| `s_factor=√(−2ln0.001)` | `3.7172`（运行期计算） | sdet_api.cpp:1822-1823 | ALG-STARDET §2 | 符合 |
| uint16 归一 | `65535` | sdet_api.cpp:1834 | ALG-STARDET §2（DISP-STAR-006） | 符合（硬编码已登记） |
| 盒半径上限 | `200` | sdet_api.cpp:1824,2074,2330 | ALG-STARDET §2（MAX_BOX_RADIUS） | 符合 |
| 去重半径比 | `0.2` | sdet_api.cpp:1825,2099 | ALG-STARDET §2 | 符合 |
| 饱和动态范围比 | `0.7` / `0.1` | sdet_api.cpp:1820-1821 | ALG-STARDET §2 | 符合（sdet 侧） |
| 高斯拟合 `INV_4_LOG2` | `0.36067376022224075` | sdet_api.cpp:46 | ALG-STARDET §2（`SX=FWHM²/4ln2`） | 符合（解析 0.36067376022224085，1 ulp） |
| LM 收敛容差（星检） | `1e-3` ×3 | sdet_api.cpp:39-41 | ALG-STARDET §9「XTOL/GTOL/FTOL 编译期常量」（**无值**） | 待定（无规范值可比） |
| LM 最大迭代（星检） | `20` | sdet_api.cpp:42 | ALG-STARDET §2（`LM_MAX_ITER_ANGLE`） | 符合（符号级） |
| 圆度退化替换 | `0.999→0.9` | sdet_api.cpp:362 | 无出处 | 待定（未登记的实现约定） |
| 自动 fitRadius 因子 | `0.87`、`3.0`、钳 `[6,20]` | sdet_api.cpp:2166-2168 | ALG-STARDET §11.1（仅「auto 半径日志推导」） | 待定（公式无出处） |
| `reject_star` RMSE 门 | `0.2`；FWHM 下限 `0.5`；圆度 `0.5` | sdet_api.cpp:214,221,227 | ALG-STARDET §3 | 符合 |
| `reject_star` FWHM 上限 | `2.3548·(1+0.5·ln(se/2))` | sdet_api.cpp:239-244 | **无出处** | 歧义 ★020 |
| 饱和阈值（wrapper） | `50000.0` | wrapper:167 | **无出处** | **不符** ★005 |
| 背景 sigma-clip（wrapper） | 2 轮、`3.0` | wrapper:41,56 | ALG-STARDET §2 为「3 轮、5σ、行差分」 | **不符** ★002 |
| Moffat4 LM | tol `1e-8`、iter `200`、λ₀ `1e-3`、步 `1e-6/1e-8` | dpsf_psf.cpp:133,141,374-375 | STAR_PSF_ALGORITHMS §11.1（§3 冲突 ★023） | 符合（§11.1）/ 歧义（§3） |
| Moffat4 初值 | `sx0=0.15·rw` | dpsf_psf.cpp:368 | **无出处**（§3 说 1.2） | 歧义 ★018 |
| sx/sy 下界、A 下界 | `0.3` / `0.0` | dpsf_psf.cpp:195-197,386 | STAR_PSF_ALGORITHMS §11.1 | 符合 |
| 背景约束比 | `0.5`（分母 `max(bkg0,0.01)`） | dpsf_psf.cpp:403-404 | STAR_PSF_ALGORITHMS §11.1 | 符合 |
| rect 面积下限 | `9` | dpsf_psf.cpp:271 | STAR_PSF_ALGORITHMS §11.2 | 符合 |
| 截尾分位 | `0.1/0.9` | dpsf_psf.cpp:248-249 | PSF.md:22 | 符合 |
| 每弧秒换算 | `206.265` | ipv_select.cpp:56-57 | PLATESOLVE §11.1（`s0=206.265·pixel_um/focal_mm`） | 符合 |
| 三角形尺度容差 | `0.002` | ipv_solver.cpp:458,864,1213 | PLATESOLVE §9 | 符合 |
| Huber δ | `1.345` | ipv_sip.cpp:241；ipv_distortion.cpp:221 | PLATESOLVE §9 | 符合 |
| SIP 网格 | `NB_GRID=41` / `NB_GRID_X=81` | ipv_wcs.cpp:417,541 | PLATESOLVE §2 F4 / DISP-WCS-008 | 符合（`ipv_solver.h:72` 注释仍写 7，**代码注释漂移**） |
| robust_refine `min_stars` | `100` | ipv_robust_refine.h:45 | PLATESOLVE §3（`min_stars`，无值） | 待定 |
| PSF 拟合上限 | `psf.max_stars` 默认 `5000` | module_adapters.cpp:2231 | PSF-FAST-001（负责人裁决，配置键） | 符合 |
| sdet maxStars（wcs 节点） | `2000` | module_adapters.cpp:2808 | **无配置键、无规范出处** | **不符** ★006 |
| sdet fwhmClipSigma | `3.0f` | module_adapters.cpp:2810 | DISP-STAR-003（impl 不消费） | **不符** ★006 ★017 |
| sdet maxAxisRatio | `2.0f` | module_adapters.cpp:2811 | ALG-STARDET §3（`maxAxisRatio`，无默认值） | 待定 |
| 检测 sigma（节点） | `StarDetector det(5.0)` | module_adapters.cpp:2050 | STAR_DETECTION.md:19（5σ 语义） | 符合（语义）/ 算法族 ★002 |
| 检测 sigma（sdet 默认） | `sdet_create` 默认参数 | sdet_api.cpp:963-975 | ALG-STARDET §11.1 | 符合 |
| FITS 1-based 桥接 | `kP1FitsPixelOrigin=1.0` | module_adapters.cpp:374,2619,2900 | SCI-WCS-001 §3a/§5a；PLATESOLVE §11.2 | 符合 |
| cosmetic 默认（节点） | `5.0/5.0/median/4` | module_adapters.cpp:1954-1958 | batch_config.json（实验配置 5.0/5.0/median/4）；CALIBRATION_ALGORITHMS.md:383 | 符合（值一致）/ 配置键未入 schema ★006 邻域 |

---

## 6 诚实声明与未覆盖面

1. **工作树并发修改**：审计期间 `lib/infrastructure/scheduler/src/module_adapters.cpp` 被并行分片修改（mtime 2026-09-19 19:23:35 +0800，审计中途行号整体 +12 后又有局部改动）。本报告全部 `module_adapters.cpp` 行号以 §0 声明快照（md5 `35288396163311bf12a13d717270cd6e`）为准；★004 已注明「并行分片已新增 `p1_psf.json.flux` 列」这一在飞修复。
2. **未判定为「不符」的候选项（不得凑数）**：
   - `sdet_api.cpp:514` 的 `0.70710678` vs 规范 `0.7071`：规范自带 `(1/√2)` 注解，判**符合**（规范给的是舍入值）。
   - `INV_4_LOG2=0.36067376022224075` vs 解析 `...085`：1 ulp，判**符合**。
   - `sqrte=1.6487212707` vs `1.6487212707001282`：规范未给位数，判**符合**。
   - 星检 LM `XTOL=GTOL=FTOL=1e-3`、`max_iter=20`：规范只写「编译期常量」无值，判**待定**（不足以判不符）。
3. **未覆盖面（本分片范围外或证据不足）**：
   - `lib/algorithms/psf/tests/p1psf/**` 与 `lib/algorithms/star_detection/tests/p1star/**` 的测试是否把上述不符固化为「期望」——未逐条核（超出「规范侧」范围）。
   - `ipv` 内部 `iter_trans_solve`/`robust_refine` 的逐式复算未做（只核了 §2/§9 的常数与网格）。
   - `docs/science/ASTROMETRY.md` 未逐条清点（时间预算），只在 PLATESOLVE 交叉处引用了 CRPIX 不变量。
   - 星检完备性/虚警门（S-036）、TEST-STAR-DESIGN-001 可执行面（S-060）标**待定**：交付链上找不到对应机器门锚。
4. **未跑构建/测试**（硬约束），所有结论均为静态证据 + 独立数值复算（`2.3548200450309493/1.230310=1.9140054498711294`；`E[10–90% trimmed |r|]=0.731673095280613`）。
5. **不改任何生产代码/文档**：本报告只登记修复面，未执行。
