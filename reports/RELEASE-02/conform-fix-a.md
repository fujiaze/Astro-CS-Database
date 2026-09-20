# CONFORM-FIX-A — 规范↔实现符合性修复（分片 A：① / ④ / ⑤）

> 分片: CONFORM-FIX-A（AstroCS RELEASE-02，符合性修复）
> 依据: `reports/RELEASE-02/conformance/CONFORM-SWEEP-1.md`（+ `.json`）条目
> CONFORM-SWEEP-1-001 / -005 / -008
> 原则: **规范是对的，改实现**（未改任何 `docs/**`；未改三条之外的任何科学公式/容差）
> 硬约束遵守: 零 git 写；未跑 `ninja`/`cmake`/`ctest`（用 `g++ -fsyntax-only` 与**独立重链**）；
> `TMPDIR=/dev/shm/astrocs_cfa`（收尾清理）；产物落 `run/RELEASE-02/conform-fix-a/`
> 工作树基线: HEAD `cbda64a1424030f977fbaa777fb3923bdcfc959f`（工作树含并行分片在制改动，见 §6 风险）

---

## 0 结论摘要

| # | 条目 | 类 | 判定 | 修法 | 红例（旧） | 绿例（新） | 数值证据（修前 → 修后） |
|---|---|---|---|---|---|---|---|
| ① | `fwhm_px` 跨块混用（σ 高估 1.914×） | C1 | 不符（实现错） | 检测块列改用**高斯因子** 2.3548200450309493；PSF 块路径（`snr_estimator.cpp`）显式改用 Moffat4 因子 1.230310（数值零变化） | `p1snr_linux_test` 旧测试 + 新源 **56/114 失败**；`p1snr_science_test` 旧测试 + 新源 **75/129 失败** | 新测试 + 新源 **125/125**、**138/138** 全过（含新增 `mother_function` 组） | 帧级 `median_snr` 8.1442 → 16.0402（×1.9695）；`F_5` 7032.96 → 3563.64 ADU（×0.5067）；`m_5(ZP=20)` 10.3822 → 11.1203（+0.7381 mag） |
| ④ | 饱和阈值硬编码 `peak>50000.0` | C2 | 不符（实现错） | 改为规范双条件：`dynrange=min(max(img),65535)−bg`、`0.7/0.1·dynrange` 的 3×3 邻域双条件（逐帧由数据算） | 6 用例中 **4 例红**（满井 40000/16000 漏标、高本底过标、旧测试"亮高斯=饱和"） | 6/6 绿（`conf4_driver`）；`p1_stars_test` 新测试全过 | 满井 40000 平台星：不饱和(0) → 饱和(1)；bg=30000 高斯峰 60000：饱和(1) → 不饱和(0) |
| ⑤ | calibrate manifest `dark_scale` 撒谎 | C1/C5 | 不符（实现错） | 统一写 `actual_k`（实际施加的 K） | 新测试 + 旧代码：**13e 红**（`observed=1 expected K=0.5`，exit 1） | 新测试 + 新代码：`observed=0.5`，整目标 0 失败（exit 0） | 标准式 K=0.5：manifest `dark_scale` 1.0 → 0.5（算术两版都是 87.5 ADU，像素逐位不变） |

**规范侧问题上呈: 0 条新发现**（三条的规范侧均为正确；有两处**实现映射判断**需负责人确认，见 §4.1/§4.2，均不要求改规范）。

---

## 1 ① [C1] `fwhm_px` 跨块混用 ⇒ σ 高估 1.914005×（CONFORM-SWEEP-1-001）

### 1.1 规范依据（只读权威）

- `docs/science/STAR_DETECTION.md:31-34`：「**检测侧母函数 = 椭圆高斯**…**PSF 侧 = 椭圆 Moffat4**；两模型**宽度列不可跨块比较**：同 sx 下 `FWHM_gauss/FWHM_moffat = 2.354820/1.230310 = 1.9140×`（DISP-STAR-007）」
- `docs/science/STAR_DETECTION.md:68`：`σ = FWHM/(2√(2ln2))`（检测侧，= 2.3548200450309493）
- `docs/algorithms/STAR_DETECTION_ALGORITHMS.md:63-64`：`fwhm=2.3548·σ`（TWO_SQRT_2_LOG2）…**禁止跨块比较**（DISP-STAR-007）
- `docs/algorithms/STAR_DETECTION_ALGORITHMS.md:253-258`：检测/PSF 双母函数（列语义不可互换）…`star_det` 的 fwhm/flux 列与 PSF 块同名列**禁止跨块比较**
- `docs/science/PSF.md:20,51,63`：PSF 块 `fwhm_x/y = 1.230310·s`（Moffat4 β=4）
- `docs/contracts/DATA_SEMANTICS.md:483,487`：`frame_depth_flux5_adu` 的参考轮廓 = **目录中位 FWHM** + 帧 σ_sky；`snr_sample` = **全部测光有效 sources（flux>0, fwhm_px>0）**，**与 psf.max_stars 解耦**

### 1.2 根因

`p1_sources.json.sources[].fwhm_px` 由**检测块**（`lib/algorithms/star_detection/wrapper_phase1/star_detector.cpp:162` `s.fwhm_px = 2.3548·0.5·(a+b)`，二阶矩高斯等效 FWHM）产出，属**高斯母函数列**；而 `lib/algorithms/noise_snr/cpp/src/snr_science.cpp:37,41-42` 用 **PSF 块**的 Moffat4 因子反解：

```
σ_used = fwhm_px / 1.230310 = (2.3548200450309493/1.230310)·σ_true = 1.9140054498711294·σ_true
```

该 σ 进入 `snr_moffat4_profile_f64` 的 `alpha²=2σ²`、`autoHalf=ceil(12·FWHM)` 与孔径改正 `f_in(r)=1−(1+r²/(2σ²))^{-3}`，使 `ΣP_i²` 整体偏小 ⇒ **σ_F 偏大、SNR_F 偏低、F_5 偏大、m_5 偏亮**。

### 1.3 修法（二选一 → 选 (a)，附 (b) 被排除的依据）

**(a) 已采纳**：`snr_science` 对**检测块列**改用高斯因子 `kGaussFwhmFactor = 2.3548200450309493`；本块自身 Moffat4 模型的 FWHM 一律由 σ **正向**导出（`fwhm_eff = σ·kMoffat4FwhmFactor`，用于网格半边长与孔径半径默认值），**不再与输入列比较**。审计 §001「修复面」给出的第一种方案与 (a) 等价（`σ_gauss = fwhm/2.354820` 后按 Moffat4 `FWHM=1.230310·σ` 重建 ⇒ `FWHM_moffat4 = 0.52245·fwhm_gauss`）。

**(b) 未采纳（改用 PSF 块的 Moffat4 fwhm）**，依据：

1. **合同面禁止**：`DATA_SEMANTICS.md:487` 冻结 `snr_sample` = `sources` 的测光有效源且**与 `psf.max_stars` 解耦**；`:483` 冻结参考轮廓 = **目录中位 FWHM**。PSF 块行受 `psf.max_stars`（默认 5000）截断，改用 PSF 块列会使 SNR 样本塌缩为「最亮子集」，直接违反该条与机器锁 `tests/unit/p1snr/p1snr_frame_parity_test.cpp`（P14-N-08/N-09：`psf.max_stars` 变化时 SNR/深度必须逐位一致）。
2. **同一模块内已有正确的 (b) 实现**：`snr_estimator.cpp`（PSF 块行 → `snr_extract_model_v3` 控制点路径）**本来就**用 1.230310，因为它的输入行 `row[5]=fwhm_x/y` 确属 PSF 块。可见缺陷不是「该不该用 Moffat4」，而是「**同一个因子被用在了错误的块上**」——正确工程是**按数据来源选因子**（两条路径各用各的）。
3. 若负责人认为应改为 (b)，则属**改规范**（`DATA_SEMANTICS §13.4` + P14-N-08/N-09 裁决面），需走变更 claim，不在本分片权限内 ⇒ 见 §4.1。

### 1.4 改动文件（① 独立提交范围）

| 文件 | 改动 |
|---|---|
| `lib/algorithms/noise_snr/cpp/src/snr_science.cpp` | 新增 `kGaussFwhmFactor`；`detectionSigmaFromFwhm()` 取代 `moffat4SigmaFromFwhm()`；`snr_moffat4_profile_f64`/`snr_source_snr_f64` 的 `fwhm_px` 按高斯因子换算；`fwhm_eff` 统一由 σ 正向导出；契约注释 |
| `lib/algorithms/noise_snr/cpp/src/snr_estimator.cpp` | PSF 块两处调用点改为「先按 1.230310 换算 σ，再经 `sigma_px` 传入」（**数值逐位不变**）；新增 `kMoffat4FwhmFactor` 唯一定义点 |
| `lib/algorithms/noise_snr/cpp/include/snr_estimator.h` | `SnrSourceParams.fwhm_px/sigma_px` 与 `snr_moffat4_profile_f64` 的契约注释（哪一列属哪一块、禁止跨块） |
| `lib/algorithms/noise_snr/wrapper_phase1/snr_frame_science.h` | `SnrSourceRow.fwhm_px` 语义注释（检测块高斯 FWHM，来源列固定为 DATA-P1-SOURCES） |
| `tests/unit/p1snr/p1snr_linux_test.cpp` + `tests/unit/p1snr/CMakeLists.txt` | oracle 因子改为高斯因子；锚值按独立 NumPy oracle 复算；新增判别组 `mother_function`（注册 `p1snr_linux_mother_function`，被 `ci/checks.json` 的 `p1snr_linux_*` 通配覆盖） |
| `lib/algorithms/noise_snr/tests/p1noise/p1snr_science_test.cpp` | `refCompute` 增加「哪一块」显式入参（`from_psf_block`）；锚值复算；新增跨块判别断言；`extract_v3` 期望改走 PSF 块口径（数值不变） |

### 1.5 数值证据（修前 → 修后）

固定输入：σ_true = 1.25 px ⇒ 检测块列 `fwhm_px = 2.3548200450309493·1.25 = 2.9435250562886868`；flux = 1e4 ADU，σ_sky = 20 ADU（天空受限），ZP = 20。
（`run/RELEASE-02/conform-fix-a/harness/conf1_old.out` vs `conf1_new.out`，驱动 `conf1_driver.cpp`）

| 量 | 修前（旧路径） | 修后（新路径） | 比值 |
|---|---|---|---|
| 隐含 σ | 2.3925068123389117 | 1.25 | **1.9140054498711294** |
| `ΣP_i²` | 0.035751441876026788 | 0.14032144048905396 | ×3.9249（连续极限 1.914005² = 3.6634，差见 §1.7） |
| `P_center` | 0.083413198958308288 | 0.3038303209125699 | ×3.6425 |
| `SNR_F` | 94.54025845642002 | 187.29751766177654 | ×1.9811 |
| `σ_F` / `F_5` | 105.77504402116348 / 528.87522010581745 | 53.390990573927873 / 266.95495286963938 | ×0.50476 |
| `m_5`（ZP=20） | 13.191616952292856 | 13.933905042943259 | **+0.74229 mag** |
| `snr_aperture` | 42.889439688334747 | 82.090621305391807 | ×1.9140 |

帧级（wrapper，5 颗真实源 R 波段：flux/fwhm 取自 `p1_sources.json`，σ_sky = 260.8590110604006，组内公共 F0 = 11581.213134765625，ZP = 20）：

| 量 | 修前 | 修后 | 变化 |
|---|---|---|---|
| `snr_f[0..4]` | 4.7863 / 8.5716 / 8.1442 / 19.2810 / 7.9696 | 10.0152 / 16.9596 / 16.0402 / 37.3627 / 15.7282 | ×1.938 – ×2.092 |
| `median_snr`=`snr_phot`=`median_source_snr` | 8.1441962126078504 | 16.040224274269896 | **×1.9695** |
| `frame_depth_flux5_adu` | 7032.9597439466052 | 3563.6411415622206 | **×0.50671** |
| `frame_depth_m5_mag`（ZP=20） | 10.382154671108845 | 11.12026508965055 | **+0.73811 mag**（5σ 深度变深 1.97×） |
| `local_snr`（相对权重场） | 0.5877/1.0525/1/2.3675/0.9786 | 0.6244/1.0573/1/2.3293/0.9805 | 内部相对量部分相消（绝对值仍随之变） |

**方向核对**：σ 由 1.914× 高估修正为真值 ⇒ SNR 上升、F_5 下降、m_5 变深；纯 σ 因子 1.914005，实测 1.94–2.09（差异归因见 §1.7）。**PSF 块路径（`snr_extract_model_v3` → `frameDepthFromPsf`）逐位不变**：`conf1_psf_probe` 在旧/新实现下输出完全一致（`snr_phot=8.1441962126078504`、`frame_depth_flux5_adu=7032.9597439466042`、逐点 `snr_psf` 同值），证明本修复只作用于「检测块列 → SNR 块」这一条通路。

**独立复算**：`harness/conf1_oracle.py`（NumPy，逐像素向量化，不调用被测 C++）与 C++ 新路径一致到 ~1e-14 相对（例：`median_snr` 16.04022427426992 vs 16.040224274269896；`F_5` 3563.641141562232 vs 3563.6411415622206）。该 oracle 同时给出测试锚值（`conf1_oracle.json`）。

### 1.6 判别力测试（能红能绿）

| 用例 | 命令（独立重链，非 ctest） | 结果 |
|---|---|---|
| (a) 旧测试 + 旧源（基线） | `p1snr_linux_test_old.cpp + snr_science_old.cpp` | **114 checks, 0 failed**（exit 0） |
| (b) 旧测试 + 新源（红） | 同上测试 + 新 `snr_science.cpp` | **56 failed / 114**（exit 1）例：`FAIL O median_source_snr: got=16.040224274269896 exp=8.1441962126078504`、`FAIL O flux5: got=3563.64 exp=7032.96`、`FAIL O m5 with ZP: got=11.1203 exp=10.3822` |
| (c) 新测试 + 新源（绿） | 新 `p1snr_linux_test.cpp` + 新源 | **125 checks, 0 failed**；7 组（units/oracle/contract/negative/determinism/common_ref/**mother_function**）全 PASS |
| (a') 旧测试 + 旧源（基线） | `p1snr_science_test_old.cpp + 旧 snr_science/snr_estimator` | **129/129 passed** |
| (b') 旧测试 + 新源（红） | 同上 + 新源 | **54/129 passed, 75 failed** |
| (c') 新测试 + 新源（绿） | 新测试 + 新源 | **138/138 passed**；5 组全 PASS |

新增判别组 `mother_function`（`tests/unit/p1snr/p1snr_linux_test.cpp`）与 `units` 组内新增断言（`p1snr_science_test.cpp`）核心锁：

1. **位级恒等（主锁）**：`snr_moffat4_profile_f64(2.3548200450309493·σ, 0, …) == snr_moffat4_profile_f64(0, σ, …)`（旧路径必红，新路径逐位相等）；
2. **跨块因子恒等**：`kGaussFwhmFactor/kMoffat4FwhmFactor == 1.9140054498711294`；
3. **非恒真锁**：按 PSF 块因子反解同一列必须给出**不同**结果；
4. **逐源/帧级同锁**：`snr_source_snr_f64`（fwhm 列）与（`sigma_px` 直传）逐位一致；wrapper 帧级 `σ_F`/`SNR_F` 与解析式 `σ_sky/√ΣP²` 一致；
5. **方向锁**：σ ×1.914 的更宽输入必须给出更低 SNR（比值 ∈ [1.85, 2.15]）。

### 1.7 残留观察（不属本修复面，如实登记）

- 纯 σ 因子是 **1.914005**，但实测 SNR 比为 **1.94–2.09**（σ ≈ 0.6–1.45 px）。原因**不是**网格截断（`half` 从 30 加到 2000，比值仍为 1.981141），而是 **Moffat4 轮廓在 1 px 网格上的离散化**：`ΣP_i² ∝ σ^{-2}` 的连续极限在 σ ≳ 数 px 才成立。本修复只改「σ 尺度口径」，不改轮廓模型/网格规则（`autoHalf=ceil(12·FWHM_moffat4)` 与 `half≥30` 冻结规则保持）。若要进一步收紧，属「SNR 块轮廓模型/采样」议题，需另立条目。
- 检测块的 `fwhm_px` 本身是 5×5 窗内二阶矩（`star_detector.cpp:139-162`），对 Moffat4 真星是**有窗截断的**矩宽度；该口径问题属 CONFORM-SWEEP-1-002/004（生产检测器口径），不在本分片。

---

## 2 ④ [C2] 饱和阈值硬编码 `peak>50000.0`（CONFORM-SWEEP-1-005）

### 2.1 规范依据

- `docs/science/STAR_DETECTION.md:20-23`：饱和判定 = 3×3 邻域**双条件**（`meanhigh−bg ≥ 0.7·dynrange` 且 `pixel0−minhigh ≤ 0.1·dynrange`）
- `docs/algorithms/STAR_DETECTION_ALGORITHMS.md:37-42`：`bg=median(img)`、`maxi=max(img)`、`dynrange=min(maxi,65535)−bg`、`minsatlevel=0.7·dynrange`、`satrange=0.1·dynrange`；`norm` 硬编码 65535；候选饱和判定取 3×3 超阈值像素的 `meanhigh/minhigh`
- 对照实现（规范锚）`lib/algorithms/star_detection/src/sdet_api.cpp:1829-1838,1881-1918`：逐字复刻上述双条件

### 2.2 根因

`lib/algorithms/star_detection/wrapper_phase1/star_detector.cpp:167` 用绝对字面量 `peak > 50000.0`（`docs/` 全库无出处）。后果：满井 <50000 ADU 的相机**漏标**、高本底帧**过标**；饱和位经 `quality&1` → `module_adapters.cpp:3295-3298` 的 `PC_QF_SATURATED` **直接改变测光有效域**。

### 2.3 修法

`detect()` 内在候选循环前**逐帧由数据算**：`maxi = max(img)`、`norm = 65535`（规范常数）、`dynrange = min(maxi, norm) − bg`、`minsatlevel = 0.7·dynrange`、`satrange = 0.1·dynrange`；逐候选取峰值 3×3 邻域中 `≥ thr` 的像素算 `meanhigh/minhigh`，满足双条件才置 `quality |= 1`。**不再有任何绝对幅值常数**。

实现映射（两点如实说明，见 §4.2）：
1. wrapper 无高斯拟合 ⇒ 用规范中**属于检测候选**的 3×3 双条件（`STAR_DETECTION.md:20-23`），而非 `A_fit>dynrange`（`ALG:77-78`，属 sdet 的拟合路径）。
2. `bg` 取 wrapper 自身的帧背景（`cat.background`，2 轮 median±3σ clip 的中位），与同函数内检测阈 `thr = bg + 5σ` 同源；`maxi` 取原始图像最大值（与 sdet 同款）。

### 2.4 改动文件（④ 独立提交范围）

| 文件 | 改动 |
|---|---|
| `lib/algorithms/star_detection/wrapper_phase1/star_detector.cpp` | 新增逐帧 `maxi/dynrange/minsatlevel/satrange`；删除 `peak>50000.0`；3×3 双条件置饱和位 |
| `tests/unit/p1_stars_test.cpp` | `test_saturated_star` 重写：真饱和星（核心被满井截平的 1px 平台）在**满井 40000 / 16000** 两档必须标出；高本底未饱和高斯峰（peak 60000）**不得**标；满井 65535 对照 |

### 2.5 数值证据（修前 → 修后）

驱动 `harness/conf4_driver.cpp`（合成帧 96×96，`StarDetector det(5.0)`）：

| 用例 | 构造 | 规范判定 | 旧实现 | 新实现 |
|---|---|---|---|---|
| S1 | bg=200，核心截平 @**满井 40000**，σ=4 | 饱和（dynrange=39800，minsat=27860，satrange=3980） | **0（漏标，红）** | **1（绿）** |
| S2 | bg=30000，纯高斯峰 60000（无平台） | 不饱和（meanhigh−bg=24919 ≥ 21000 但 pixel0−minhigh=6636 > 3000） | **1（过标，红）** | **0（绿）** |
| S3 | bg=100，核心截平 @65535，σ=4 | 饱和 | 1 | 1 |
| S4 | bg=100，弱高斯峰 900 | 不饱和 | 0 | 0 |
| S5 | bg=500，核心截平 @**满井 16000**，σ=4 | 饱和（dynrange=15500） | **0（漏标，红）** | **1（绿）** |
| S6 | bg=100，amp=60000，σ=3 纯高斯（=旧测试用例） | 不饱和（flatness 6310 > satrange 6000） | **1（过标，红）** | **0（绿）** |

红/绿：旧实现 **4/6 检查失败**（exit 1），新实现 **6/6 通过**（exit 0）。

仓库测试矩阵（`tests/unit/p1_stars_test.cpp`，独立重链）：(a) 旧测试+旧源 = PASS；(b) 旧测试+新源 = **FAIL（2 checks：旧用例的"纯高斯=饱和"断言）**；(c) 新测试+新源 = PASS。

### 2.6 已知边界（如实登记，非本修复面）

wrapper 候选门是 3×3 **严格**局部极大（`star_detector.cpp:101-107`，邻域 `≥ v` 即淘汰）⇒ **多像素平台星（真实满井平台）根本不产生候选**，因此本修复只能修正「已检出峰」的饱和分类；「平台星检不出」属 CONFORM-SWEEP-1-002（生产检测器口径）范围，未在本分片处理。本修复的实际收益：满井 <50000 相机的**近满井平顶星**（欠采样 PSF 下常见：核心 1px 触顶、邻域仍递减）不再漏标；高本底帧的亮非饱和星不再被误标为饱和（测光有效域不再被无依据地切除）。

---

## 3 ⑤ [C1/C5] calibrate manifest 的 `dark_scale` 撒谎（CONFORM-SWEEP-1-008）

### 3.1 规范依据

- `docs/algorithms/CALIBRATION_ALGORITHMS.md:150-154`（F3.2 标准式）：`k = k_init`（K 必须由调用方给出 t_light/t_dark，**不再强制 1.0**）；`actual_k = k`
- `docs/algorithms/CALIBRATION_ALGORITHMS.md:147-149`（F3.1 兼容式）：`actual_k = k_init`
- `docs/algorithms/CALIBRATION_ALGORITHMS.md:167-174`：退化对照表「标准式 K≠1 … K=t_l/t_d → `(raw−bias−K·dark)/flat`」
- `docs/contracts/DATA_SEMANTICS.md:155`：K 行「**标准式与兼容式都施加**；B2-A13 消费边界由 EXPTIME 推导 K」

### 3.2 根因

`module_adapters.cpp:1899-1900`（旧）：

```cpp
{"dark_scale", dark_opt ? static_cast<double>(actual_k)
                        : static_cast<double>(k_fixed)}   // k_fixed = doc["dark_scale_factor"] 默认 1.0
```

而算术侧 `k_use = k_expo = t_light/t_dark`（`:1873`，`k_branch` 时），`ac_calibrate_frame` 回填 `actual_k = k_use`。⇒ `dark_opt=false` 且 **未显式给 `dark_scale_factor`** 时，manifest 写 1.0 而实际施加 `t_l/t_d`。

### 3.3 修法

统一写 `actual_k`（两分支都由 `ac_calibrate_frame` 按 F3.1/F3.2 回填为实际施加的 K；dark 缺失时 `actual_k = k_fixed`，与旧行为一致）。**只改 manifest 字段来源，不改任何算术**。

### 3.4 改动文件（⑤ 独立提交范围）

| 文件 | 改动 |
|---|---|
| `lib/infrastructure/scheduler/src/module_adapters.cpp` | `p1_op_calibrate` 的 `per_frame[].dark_scale` 由 `dark_opt ? actual_k : k_fixed` 改为 `actual_k`（唯一 hunk；该文件同时含并行分片在制改动，提交时须只取本 hunk） |
| `tests/unit/p1001_real_nodes_test.cpp` | 新增 13e：**标准式**（`dark_optimization:false` 单键）+ `t_light≠t_dark` ⇒ 断言 manifest `dark_scale == K` 且像素 == 标准式 Oracle `(L−B−K·D)/F`；并打印证据行 `[B2-A13 13e] standard-form dark_scale observed=… expected K=…` |

### 3.5 判别力测试（红/绿，独立重链）

矩阵 = **同一测试目标**（含 13e）对**旧/新** `module_adapters.o`（`harness/conf5_matrix.sh`，链接面逐项复刻 `build.ninja` 的 `LINK_LIBRARIES`）：

| 目标 | 代码 | 结果 |
|---|---|---|
| `b2a13_new_old` | 新测试 + **旧** `module_adapters.cpp` | **13e 红**：`[B2-A13 13e] standard-form dark_scale observed=1 expected K=t_l/t_d=0.5` + `CHECK failed …:1137 … B2-A13: standard-form dark_scale must equal actual K=t_light/t_dark=0.500000 got=1.000000`（exit 1） |
| `b2a13_new_new` | 新测试 + **新** `module_adapters.cpp` | **13e 绿**：`observed=0.5`；整个测试目标 `CHECK failed` = **0**，exit 0 |
| `b2a13_old_old` | 旧测试 + 旧代码（基线） | exit 0（旧测试**未覆盖**标准式 K≠1，故不拦——正是审计指出的覆盖缺口） |

（`b2a13_new_old` 的 4 条 `CHECK failed` 中仅 1 条属 ⑤（13e）；另 3 条是并行分片在制的 `F-INSTR RED-5`（`p1photbroken` 帧间 k 散度门），旧 `module_adapters.cpp` 尚无该特性，与本分片无关，不计入 ⑤ 判据。）

### 3.6 数值证据

| 量 | 旧 | 新 |
|---|---|---|
| manifest `per_frame[0].dark_scale`（标准式，t_l=300s，t_d=600s，K=0.5，未配 `dark_scale_factor`） | **1.0（错）** | **0.5（= 实际施加 K）** |
| calibrated 像素（L=100,B=10,D=5,F=1） | 87.5 ADU（`(L−B−K·D)/F`） | 87.5 ADU（**不变**） |

即：修前**溯源字段与算术不符**（K≠1 时每帧系统性错误），修后字段=实际值，**像素逐位不变**。

---

## 4 需上呈/确认的规范侧事项

**结论：三条的规范本身均正确，无需改规范（0 条规范错）。** 以下两项是**实现映射判断**，请负责人确认（不确认也不阻塞交付，已按最符合规范意图的方式实现）：

### 4.1 ① 的 (a)/(b) 抉择（规范意图判断）

- 已采纳 (a)「检测块列用高斯因子」，依据见 §1.3；这与审计 §001 修复面第一种方案一致。
- (b)「改用 PSF 块 Moffat4 fwhm」**被合同面排除**：`DATA_SEMANTICS.md:483,487` + P14-N-08/N-09 机器锁要求 SNR 样本与参考轮廓取自 `sources`（检测块列）且与 `psf.max_stars` 解耦。
- 若负责人意图是 (b)，那属于**改合同/裁决**（`DATA_SEMANTICS §13.4`、P14-N-08/N-09、`p1snr_frame_parity_test`），须走变更 claim 后另立任务；本分片不擅自改规范。

### 4.2 ④ 的实现映射（规范条款选择）

- wrapper 无高斯拟合（`A_fit`）⇒ 采用**候选级**3×3 双条件（`STAR_DETECTION.md:20-23`、`ALG:41-42`），未采用 `is_saturated=(A_fit>dynrange)`（`ALG:77-78`，属 sdet 拟合路径）。二者是规范内**并列的两套机制**，选择依据是「本检测器有无拟合振幅」。
- `bg` 取 wrapper 自身帧背景中位数（与同函数检测阈同源）；`maxi` 取原始图像最大值。规范未规定 wrapper 桥接层的背景估计器（该口径问题属 CONFORM-SWEEP-1-002）。
- 若负责人要求 wrapper 也产出 `A_fit` 并走 `A_fit>dynrange`，属检测器口径整改（002），不在本分片。

### 4.3 顺带登记（非规范问题，供前台参考）

- `tests/unit/p1001_real_nodes_test.cpp` 的 13a/13b/13c/13d 配置串含**重复键** `"dark_optimization": false, … "dark_optimization": true`（nlohmann 解析取后者）⇒ 这四条实际都走**兼容式 K 分支**，这正是审计「测试只覆盖 K 分支故未拦住 ⑤」的机制性原因。本分片未改这四条（不在改动面），新增 13e 用**单键 false** 覆盖标准式。
- `lib/algorithms/noise_snr/cpp/src/snr_estimator.cpp` 与 `snr_science.cpp` 现在各自持有**不同块**的因子（1.230310 / 2.3548200450309493），两处都有「禁止跨块」注释；若后续再新增消费点，必须显式声明数据来自哪一块。

---

## 5 未决 / 风险

1. **未跑全量构建与 ctest**（硬约束）：本分片的验证手段是 `g++ -fsyntax-only` + **独立重链**（按 `build.ninja` 的 flags/includes/LINK_LIBRARIES 逐项复刻）+ 直接运行仓库测试目标。**前台仍需跑一次 `ninja` + `ctest -R "p1snr|p1_stars|p1001_real_nodes"` 做最终确认**（预期：`p1snr_linux_*`（含新 `p1snr_linux_mother_function`）、`p1snr_science_*`、`p1_stars`、`p1001_real_nodes` 全绿）。
2. **工作树含并行分片在制改动**：`lib/infrastructure/scheduler/src/module_adapters.cpp`（本次仅 §3 的 1 个 hunk 属本分片，其余 ~200 行为他片在制）与 `tests/unit/p1001_real_nodes_test.cpp`（本次仅 13e 的 hunk）需**按 hunk 拆分提交**；`docs/**` 的改动全部来自他片（本分片未触碰 docs）。
3. **① 的交付数值会发生显著变化**（帧级 SNR ×1.97、m_5 深 0.74 mag）：这是修复的**预期后果**，但会使既有以旧 SNR 为锚的验证/基线数据（如 `tests/validation/release02/*`、`run/**` 基线）失效，需前台评估是否要刷新基线（本分片未改任何基线文件）。
4. ④ 会使 `p1_sources.json` 的 `quality` 位与 `n_saturated` 发生变化 ⇒ 下游 `PC_QF_SATURATED` 有效域随之变化（预期行为）。

---

## 6 复现命令与产物清单

产物根：`run/RELEASE-02/conform-fix-a/`

```bash
export TMPDIR=/dev/shm/astrocs_cfa && mkdir -p /dev/shm/astrocs_cfa
cd "/workspace/Astro CS Database/run/RELEASE-02/conform-fix-a/harness"

# ① 红/绿 + 数值
g++ -O2 -std=gnu++17 -fopenmp -I../../../lib/algorithms/noise_snr/cpp/include \
    -I../../../lib/algorithms/noise_snr/wrapper_phase1 conf1_driver.cpp snr_science_old.cpp \
    ../../../lib/algorithms/noise_snr/wrapper_phase1/snr_frame_science.cpp -o conf1_old -lm
g++ -O2 -std=gnu++17 -fopenmp -I../../../lib/algorithms/noise_snr/cpp/include \
    -I../../../lib/algorithms/noise_snr/wrapper_phase1 conf1_driver.cpp \
    ../../../lib/algorithms/noise_snr/cpp/src/snr_science.cpp \
    ../../../lib/algorithms/noise_snr/wrapper_phase1/snr_frame_science.cpp -o conf1_new -lm
./conf1_old old   # exit 1 (红: 恒等锁失败)
./conf1_new new   # exit 0 (绿)
python3 conf1_oracle.py            # 独立 NumPy oracle (含测试锚值)

# ① 仓库测试矩阵 (旧测试×旧源 / 旧测试×新源 / 新测试×新源)
# 见 logs/ 与 §1.6; 目标 p1snr_linux_test 与 p1snr_science_test

# ④ 红/绿
g++ -O2 -std=gnu++17 -fopenmp -I../../../lib/algorithms/star_detection/wrapper_phase1 -I../../../include \
    conf4_driver.cpp star_detector_old.cpp -o c4_old
g++ -O2 -std=gnu++17 -fopenmp -I../../../lib/algorithms/star_detection/wrapper_phase1 -I../../../include \
    conf4_driver.cpp ../../../lib/algorithms/star_detection/wrapper_phase1/star_detector.cpp -o c4_new
./c4_old old   # exit 1 (4/6 红)
./c4_new new   # exit 0 (6/6 绿)

# ⑤ 独立重链矩阵
bash conf5_relink_v5.sh    # 编译+链接 (旧/新 module_adapters.o × 旧/新测试)
bash conf5_matrix.sh       # 运行 b2a13_new_old (红) / b2a13_new_new (绿), grep B2-A13
```

| 产物 | 内容 |
|---|---|
| `harness/conf1_driver.cpp` / `conf1_old.out` / `conf1_new.out` | ① 修前/修后逐源+帧级数值与恒等锁 |
| `harness/conf1_oracle.py` / `conf1_oracle.json` | ① 独立 NumPy oracle（测试锚值来源） |
| `harness/conf1_psf_probe.cpp` / `psf_old.out` / `psf_new.out` | ① PSF 块路径逐位不变证据 |
| `harness/conf4_driver.cpp` + `c4_old`/`c4_new` | ④ 6 用例红/绿 |
| `harness/conf5_relink_v5.sh` / `conf5_matrix.sh` + `logs/b2a13_*.out` | ⑤ 独立重链矩阵与 `[B2-A13 13e]` 证据行 |
| `harness/*_old.cpp` | `git show HEAD:` 抽取的旧实现快照（红例基准） |
| `evidence/pre-edit-md5.txt` / `post-edit-md5.txt` | 改动前后 md5（含审计基准核对） |
| `logs/*.log` / `logs/*.out` | 全部编译/链接/运行日志 |

**审计基准核对**：`snr_science.cpp` 改前 md5 = `ca3b134a462aefd7d86d014530980455`（= CONFORM-SWEEP-1 快照 md5，一致）；
`star_detector.cpp` 改前 md5 = `ffce241f6707aa4b08e454e3271ec4ee`（= 快照 md5，一致）；
`module_adapters.cpp` 改前 md5 = `f6eba4bcc27ec2f5a316e3db2e7c70b5`（审计快照为 `35288396163311bf12a13d717270cd6e` ⇒ 期间被他片改动，见 §5.2）。

---

## 7 硬约束遵守声明

- **零 git 写**：全程只用 `git status/show/diff/rev-parse`（只读）；未 add/commit/push/分支。
- **未跑 `ninja`/`cmake`/`ctest`**：全部验证用 `g++`（`-fsyntax-only`、独立编译、按 `build.ninja` 复刻 flags/includes/LINK_LIBRARIES 的**独立重链**）并直接运行产物。
- **未改 `docs/**`**（`git diff --name-only -- docs` 的命中项全部来自并行分片，本分片未编辑任何 docs 文件）。
- **未改三条之外的公式/容差**：`snr_science` 的轮廓式/网格规则/孔径式、`star_detector` 的检测阈/质心/矩式、calibrate 的算术式均逐字未动；① 中 `fwhm_eff = σ·1.230310` 是同一 σ 口径下的**导出量**（网格半边长与孔径默认半径随 σ 一致变化）。
- `TMPDIR=/dev/shm/astrocs_cfa`，收尾清理（见交付回执）。
