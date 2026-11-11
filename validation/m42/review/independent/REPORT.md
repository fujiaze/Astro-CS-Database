# Issue #123 / M42 阶段一 —— 独立审核报告

审核者：`m42-reviewer`（未参与本轮设计/实现；未改生产源码/原始 FITS）
审核时间：2026-11-12；分支 `rebuild/acsd-v2`，HEAD `e7ad50b73`（含 `c58fad677`）
范围：`docs/audit/m42-phase1-2026-11-12.md`、`validation/m42/**`、`run/m42-s1/**`、`git show c58fad677`

## 0. 裁决（给 Lead）

**阶段一裁决：FAIL（未完成）。不允许提出/推进阶段二新 Issue。**

理由：正式 `Normalize→Mosaic→Export` 产品不存在，Normalize 产品内容不自洽且 published/exit 0；
Mosaic/Export 未产出；本轮 PSF/测光在真实帧上只用 **2 颗星、其中一颗通量为负**，科学估计量未成立。
本轮**真实取得**且经我独立复算的成果是：诊断 FITS（定标/NaN/WCS/天测）与天测解、酉良的低背景测光噪声；
这些是 PASS 级证据，但不足以让阶段一整体通过。执行者自己的记录也写“阶段一未完成”，我确认该自评**没有夸大成功**，
但**遗漏**了本报告 §3.3 的两条产品级缺陷与 §4.3 的资源上界失真。

| 阶段一完成定义 | 我的裁决 | 依据 |
|---|---|---|
| 真实 M42 文件身份冻结、只读 | **PASS** | 196 FITS、SHA256、头一致、crop 数据可由我在原始文件上复现（§2.1） |
| 0/2309 天测错误解决/排除、星位与 Gaia 留出一致 | **PASS** | 独立复算留出 RMS 0.3176 px=0.307″；wcs JSON 解码对拍 6.4e-12″；星位落在饱和核峰上（§2.2） |
| 每帧 PSF/测光/校准/响应/DQ/球面 R 科学身份 | **FAIL** | 真实 normalize 记录只有 2 颗测量星、一颗 flux=-2.99e6；joint photometry nonconverged；R 的叶支持不成立（§3.1、§3.2） |
| ≥2–3 帧真实三命令链 + 新进程重开 + 导出 2D WCS FITS | **FAIL** | 无正式产品；Normalize 产品 signal[122]/tiles 空；Mosaic/Export 失败（§3.1、§3.3） |
| Sky/接缝有向差模、低频、噪声相容 | **FAIL** | 未产出正式 mosaic；28% 零点差只证明相对标度差，成因未分离（§3.4） |
| SNR 与估计量一致，含 shared/conditional/unknown | **PARTIAL** | `noise=unknown`、covariance 明确 omitted 且写 issue；独立孔径 SNR 只在局部窗口成立；但 `frame_science.json` 写 `science:"bounded", complete:true`，与 manifest 的 `conditional/complete:false` 矛盾（§3.3） |
| 排异：DQ + 小 N 防误删 + 注入 | **PARTIAL→FAIL** | 饱和 DQ 生效；N=2 无统计排异；无注入；隔离规则在“无孤立星”时**静默回退**到重叠窗口（§4.2） |
| Mosaic/Export 本体外部 FITS/WCS/支持/单位核对 | **PARTIAL** | 诊断 FITS 可读且数据可复现；正式 Export 未产出；tile FITS 头非法（§3.3） |
| 无重复原始副本、空间可解释 | **PASS** | 原始只读；run/ 大文件未入 git；本轮未复制原始帧 |
| 独立 Agent 阶段一裁决 | **本页** | FAIL，不允许阶段二 |

## 1. 复现命令

```bash
cd "/workspace/Astro CS Database"
git rev-parse HEAD                       # e7ad50b73…
sha256sum validation/m42/deliverable/M42-S1_Halpha_256_calibrated.fits
# 期望 321378161ae1395a0f34f1fa662e80c149dba75099ab4a00efa1ab3e2bd749f0
python3 run/m42-s1/review/independent/verify_fits.py      # 本报告 §2 全部数字
python3 run/m42-s1/review/independent/verify_product.py   # 本报告 §3 产品检查
```

脚本随本报告存放，只读原始数据/产品，不写生产目录。

## 2. 独立实际验证（不是复述执行者数字）

### 2.1 诊断 FITS：身份与数据内容

- `astropy` 打开：1 HDU，`NAXIS=2`，shape `(256,256)`，dtype `>f4`，`BUNIT='ADU'`，`BITPIX=-32`。
- NaN `21494/65536 = 32.7972%`，与报告一致；NaN 位置**恰好等于** raw `==65535` 的像素集合（21494 个，逐像素相等）。
- **数据不是“看起来合理”，而是可逐像素复现**：我用原始 light + 三个 master 独立按
  `x=(raw-b-β(D-b))/(flat/flat_norm)`（β=1）、`flat_norm=median(flat)=0.20615842938423157` 重算 256² crop，
  与交付 FITS 的 max|diff| = **1.95e-3**（值域 5.85e3–6.54e4，相对 3e-8，float32 舍入级），未掩膜像素全等于原始饱和判据。
  raw crop 中位 43714，定标后（未掩膜）中位 42694.2 —— `metrics.json` 的 `calibrated_median_adu=42694.2` 是**未掩膜**中位，
  而 `final_fits.median=28429.75` 是有效像素中位；两数不同不是错误，但报告没有说明口径（小瑕疵）。
- WCS：`CRPIX=(255.651,1663.164)` 与全帧解 `(2047.651,2047.164)` 的差**精确等于** crop 偏移 `(1792,384)`；
  `CDELT=1`、`PC=CD`（deg/px）；`A/B` 阶 3、`AP/BP=0`；`CRVAL=(84.2332072218,-5.43081255368)`。
- **一个真实的小自洽缺陷**：FITS 写了正向 SIP，但 `AP_ORDER=BP_ORDER=0`，即无逆 SIP。
  astropy `all_world2pix(CRVAL)` 返回 0-based `(254.604,1662.138)`，而正确 CRPIX-1 是 `(254.651,1662.164)`，
  往返误差 **≈0.054 px（0.052″）**。这比天测 0.31″ RMS 小，不影响天测结论，但“正向/逆向 WCS 一致”
  只对 C++ cleanroom 解成立（verify JSON 的 3.9e-7 px），**不适用于交付 FITS**；报告未披露。

### 2.2 天测：独立解码 + 独立复算

- 我用自己实现的 `wcs_*.json` 解码（`c/e/north` 正交基、`cd_rad_per_pixel`、SIP u=x−CRPIX(1-based)、TAN 投影）
  在 verify JSON 的 10 个网格点上与 `verify_*.json` 对拍：最大偏差 **6.4e-12″**（不是引用，是我算的）。
- 用 `stars.csv` 的 Gaia 位置 + 自行 PM 传播到 `obs_jyear=2025.94573544883`，重算 `x_pred/y_pred`，
  与 `residuals_20251212.csv` 的差 max **5.0e-4 px**（CSV 只保留 4 位小数）。
- 直接从 CSV 重算：train n=449 RMS **0.31831 px**、holdout n=313 RMS **0.31757 px**，
  与 `fit_20251212.json` 的 0.318311/0.317577 **一致**；holdout 中位 0.0814 px、>0.5 px 占 7.99%，
  说明报告“RMS 被 8% 尾部主导、稳健 RMS 0.062″”是**诚实的**。
- 星位落点独立检查（交付 crop 内 111 颗 Gaia 星）：**64/111** 的 WCS 预测位置落在原始 `65535` 饱和像素上
  （M42 核心亮星，与“投到 65535 峰”相符）；**82/111** 的预测像素是 5×5 局部最大值；其余是 G>14 的暗星/星云梯度区，
  不能要求可见峰。至少 5 颗最亮星（G=4.96/5.00/6.58/6.63/7.83）全部落在 65535 平台上。
- 结论：天测链路（Gaia → TAN+SIP → 星位）**没有发现夸大**；0.307″ 是真实留出值，头部 PinPoint 5.2–5.7″ 的证伪可信。

## 3. 产品与工作流缺陷（独立检查）

### 3.1 “exit 0 但产品内容不自洽”成立

- stdout：`frame_finished: published`、`run_finished.exit_code=0`；1 帧 published（`smoke256sig`，`smoke256sig2` 同）。
- `acsd.SignalEstimate` 的 `signal`、`numerical_lower`、`numerical_upper` 三个 Array **shape 均为 `[122]`**；
  `acsd.SupportMap.index.size=122`，`SupportMap.kind="primitives"`，`primitives=[""]`（**空字符串**，不是可解析的叶标识）。
- 同一产品里 `build_drizzle` 的 trace `work=73151`，`LinearAction` 的 CSR `indptr` 长度 123（122 行）、
  `indices/data` 各 **73151** 个非零。**122 行的信号不可能表示 73151 个叶贡献**；执行者“122 与 256²/order-13 视场不符”的判断成立。
  量级独立核对：256²@0.967″ = 0.0688 deg²；order-13 HEALPix 叶 ≈ 4π/(12·4¹³) sr = 5.12e-5 deg² ⇒ ~1.3e3 叶，不是 122。
- **发布 tiles**：20 个 `Dir*`、每 `Npix*.fits` 为 `(10000,64,64)` float64、`BUNIT='ADU/SR'`，总 6.5 GB。
  我抽样了 `Dir0`、`Dir130000`、`Dir20000` 各 5 个平面（共 15 个，不是全部 10,000×20）：
  **finite=0，全部 NaN**，与执行者结论一致（但执行者说“抽查 10,000 个平面”，我未复验 10,000 个，仅 15 个）。
- **执行者遗漏的 P0**：tile FITS 头**不是合法 FITS**。`ACSUNIT = 'acsd.native:M42_M5_T2_flying_dutchman-20251212_041842-600S-H-alpha*1/`
  超长且**没有闭合单引号**，astropy 读 header 直接抛 `VerifyError: Unparsable card (ACSUNIT)`（data 仍可 memmap 读）。
  即这些“产品”连标准 FITS reader 的头解析都过不了，却被 publish 为 exit 0。这是“exit 0 ≠ 可交付”的**第二个独立反例**。

### 3.2 真实帧上的 PSF/测光：N=2，且一颗通量为负

`trace.jsonl` 的 `measured_stars` 实际内容（crop 1）：

```
{"id":3017360936587007616,"x":178.621,"y":220.526,"flux":4227830.0}
{"id":3017363994611283840,"x":95.9243,"y":30.6567,"flux":-2991650.0}
```

- 真实 256² 核心只测到 **2 颗**共同星，**第二颗 flux = −2.99e6**；其 WCS 预测位置是 (94.4,34.9)，测量位置 (95.9,30.7)，
  偏离 ~4.7 px（远大于 0.32 px 天测 RMS），说明该测量本身不可信。
- 因此本轮 `fit_psf`/`measure` 的“科学身份”不是“受限但成立”，而是**估计量未建立**：用一个负通量样本来约束 PSF/背景，
  再据以发布信号。执行者进度表把该行记为 PARTIAL 可以，但不能被读成 PSF/测光已认证。

### 3.3 元数据/完整性自相矛盾

- `manifest.staging.json`：`assessment.complete=false, noise="unknown", science="conditional", validity="certified"`；
  同产品 `frame_science.json`：`assessment.complete=true, science="bounded"`。
- `capabilities` 里明确写了 `omitted:no_noise: the covariance model has no roots` 与
  `omitted:signal_only: ...`；`SignalEstimate.assessment.issues` 也写了 `unsupported ... noise is unknown`。
  **这部分是诚实的**（unknown 未填零），我确认 §3.3“不得声称 SNR 已认证”的表述成立；错在 `frame_science` 的 `complete/bounded`。
- `validation/m42/science/metrics.json` 的 `photometry` 段：`zero_point_median≈2.73e20`、`zero_point_mad_sigma≈4e20`、
  `snr_median=0.75/0.13`、`flux_negative_fraction=19%/27%`。这是**明显的病态数**，但 README/audit **只写
  “joint photometry nonconverged”，没有点名这组 1e20 零点**。证据包不应把这种数静默放着；应显式标注为失败产物。
- `Mosaic`：`mosaic1/mosaic2` 目录有失败证据，但我**未重跑** 10+ 分钟 OOM 作业 —— 归因（11.6–12.6 GiB）来自执行者日志，**NOT_VERIFIED**。
- `Export corrupt_product (LinearAction:2e9d3aa4…)`：我**未重跑** Export，**NOT_VERIFIED**；但 §3.1 已给出一个同类的闭包/发布不完整反例，执行者的“输出闭包缺陷”归因方向合理。

### 3.4 28% 两夜零点差：数字成立，但“解释”过度

- 从 `lowbg_photometry.json` 逐星复算 `frame2/frame1`：1.2702 / 1.3124 / 1.2144 / 1.2830，中位 **1.27659**（=报告 1.2766），
  散布 1.214–1.312（约 ±4%）。**数字成立**。
- 但是否“正确解释为两夜系统项”：四颗星在同一个 128² 小窗口内呈一致乘法比例，**支持相对标度差**；
  可执行者把成因列为“大气/主平场/光路差”。**同一套 master flat 用于两夜，flat 的公共误差在比值中相消**，
  所以“主平场”作为 28% 的解释**没有得到支持**（除非两夜光路/焦比不同，而两帧头 FOCALLEN 同为 M5 1917.x）。
  从现有证据无法区分：大气透过率（3 nm 窄带 28% 偏大）、孔径/PSF 差（FWHM 3.35 vs 4.11 px，r=6 px 的孔径修正我估算仅 ~0.3%，
  但真实 wing 未评估）、或每夜 flat 不匹配。**结论应写“未分离的系统项”，不能写成已被解释。**
- 当前产品未施加该标度：属实（定标公式没有夜间零点项）。

## 4. 修复代码审查（`git show c58fad677`，8 个生产文件）

### 4.1 `process_limits` / 命令 scope（process.hpp、pool.cpp、normalize.cpp、normalize_runtime.hpp）

- 实现：`process_limits()` 返回 `services->config().process`；normalize/workflow 把命令 scope 设为该 process 上限。
  这消除了“硬编码 1 GiB / 4e9 work 在第一个 process 前就拒绝真实帧”的问题，**方向正确**，且不是把上限调成无限：
  命令 scope 仍等于 process 账本上限，未超过 process 配置。
- 但两点需指出：(a) 命令 scope 不再比 process 更窄，**单命令可吃满整个 process 预算**，并发命令之间的隔离性依赖
  process 账本；我没有验证 `open_command` 是否新建独立账本（**NOT_VERIFIED**，若新建则存在重复计账风险）。
  (b) 真实拒绝的观测证据是 `host_bytes`（2.25 GB > 1 GiB），而 fix 同时把 work 预算从 4e9 提到配置的 **1e12（250×）**；
  本轮 trace 的总 work 只有 `calibrate 65536 + 4.19e6(65536×64) + 73151 + 4 + …` ≈ 4.3e6，
  **没有任何证据说明 4e9 work 不足**。资源上界被顺带放宽 250× 而没有取证，属于过度修复。

### 4.2 `fit_work` 与 `≥2×window` 邻域排斥（psf_process.cpp、measure.cpp、normalize.cpp）

- `fit_work`：`passes=4*stars+128`，`bounded=template_work*passes*2*(max_iterations+8)`，floor 8 MiB。
  代码注释说“4·S + 64”，实际用 **+128**（注释与实现不一致，小问题）。
  **它是资源估计而非科学容差**：只进 `work_upper`/准入，不改公式或阈值——这一点与执行者表述一致。
  但它**不是可信的“上界”**：真实运行 trace `fit_psf work=4`（2 颗星），而声明上界
  ≈ `(2×window_samples)×136×2×72` ≈ 千万级（window_samples 由 r=10 px 决定，量级 3e2）。
  **声明值与实际计费差 6 个数量级**，说明这个“结构性上界”是拍出来的，不是从实际计费路径推出来的；
  作为资源保护它只会过度拒绝合法帧，不能保证不 OOM。
- `≥2×window` 排斥：`2.0*window_radius` 恰好是“窗口不重叠”的几何阈值（半径 r、距离 ≥2r），
  因此**作为“不重叠窗口”的判据是对的**，不是错的容差。但它**确实改变估计量**：
  只在孤立星上拟合 PSF/测光 → 在 M42 拥挤核心只剩 2 颗（其中一颗负通量），PSF 样本有选择偏差（外围、非核心星），
  且不能代表核心 PSF。更严重的是 `normalize.cpp` 里的
  `if (selected_stars.empty()) selected_stars = in_frame;` —— 当**没有任何孤立星**时会**静默回退到重叠窗口**，
  且不产生 issue/assumption，产品元数据无法区分“已隔离”与“回退”。这正是一个“静默改变估计量”的反例。
  正确修法应是局部背景模型（执行者 §5.4 也承认），而不是删源。

### 4.3 饱和阈值 65535（commands.cpp）

- `build_raw_observation` 用 `image.values[i] >= 65535.0` 标 `kDqSaturated`，注释明确写“ADC full scale, not a scientific tolerance”。
  **诚实**，但：(a) 这是一个**设备属性**，不是“ADC 满量程”的通用事实；本数据恰好 BZERO=32768/BITPIX=16，
  若换一帧 BZERO 不同（或数据已被缩放），字面 65535 会**静默失效**（不掩膜任何像素），且没有检查 BZERO/BITPIX；
  (b) 真饱和可发生在满量程以下（线性度/满阱），`==65535` 只是饱和像素的**下界**，交付 FITS 的 32.8% 是下界而非全部饱和；
  (c) 该阈值没有作为 **named unknown** 出现在产品 assessment 中（只在代码注释里）。任务要求“是否应具名 unknown”——**应该**。

### 4.4 calibration FITS 平面读取（commands.cpp）

- 新增 `bias_file/dark_file/flat_file`：读 FITS、用 `values.size() != shape.front()` 校验、一律以 `unit_adu("detector")` 复制。
  缺陷：(a) 只校验**总像素数**，不校验 (nx,ny) 与方向；4096² master 与转置帧同 count 时会静默接受；
  (b) **不看 BUNIT**，把 `flat`（`calibration.json` 明写 normalized dimensionless）也标成 ADU —— 单位元数据错误；
  (c) 没有检查平面是否为 2D/单平面。数值上本轮结果正确（我 §2.1 复算逐像素一致），但这是“靠数据凑对”而非校验正确。

### 4.5 mosaic `groups`/`gauge` 解析（commands.cpp）

- 逐项读 `frames/response_proof/spatial_kernel_proof/assumption_kind/fixed_objects`，缺失时 `string_field` 返回空串，
  于是 `response_proof=Id("")`；`frames` 为空也照样 push；`assumption_kind` 任意字符串照收，默认 `same_response_different_kernel_conditional`；
  `gauge.kind` 只要不是 `weighted_sum` 就一律当 `reference_frame`。
- **没有任何**“proof id 必须非空/必须能解析到已存对象/组必须覆盖输入帧/组间不重叠”的校验。
  因此“静默接受不一致 proof”是**可能的**——至少空 proof 会被当作一个合法 `Id` 传下去。
  下游 `Comparability` 是否会再拒绝，我**未追踪**（NOT_VERIFIED）；但 CLI 解析层本身是宽松的，与执行者“显式 proof”表述有差距。

## 5. 执行者报告的问题：证实 / 遗漏 / 夸大

**证实**（我独立复算）：
- 诊断 FITS 的 SHA256、shape、BUNIT、NaN 32.7972%、定标公式结果、WCS 与天测解一致；
- 天测留出 RMS 0.307″、匹配数 762、头部 WCS 5.18″ 证伪；
- Normalize exit 0 / published、signal[122]、support 122、tiles 抽样全 NaN；
- 4 颗共同星比值中位 1.2766；物理噪声/协方差 unknown 的表述；X01 未完成不计回归。

**遗漏**（报告未写、我实测发现）：
1. tile FITS 头 `ACSUNIT` 截断/未闭合，astropy 头解析失败（§3.1）——产品级缺陷。
2. 交付 FITS 缺逆 SIP（AP/BP=0），正/逆 WCS 往返差 ~0.05 px（§2.1）——“对拍 ≤1e-9″”只对 cleanroom 解成立。
3. `frame_science.json` 写 `complete:true/science:"bounded"`，与 manifest 的 `conditional/false` 矛盾（§3.3）。
4. `metrics.json` 的 joint photometry 零点 ~2.7e20 未在正文点名（§3.3）。
5. `measured_stars` 只有 2 颗且一颗 flux=-2.99e6（§3.2）。
6. `fit_work` 声明上界与真实计费（work=4）差 ~6 个数量级；work 预算 4e9→1e12 无证据（§4.1、§4.2）。
7. `selected_stars.empty()` 时静默回退重叠窗口（§4.2）。

**夸大/不成立**：
- “两个完整帧真实三命令链”不成立（无正式产品；只有诊断单帧定标 FITS）。执行者自评 FAIL，这点没有夸大。
- “两夜 28% 零点差解释为大气/主平场/光路差”：**比值成立，成因未分离**；同一 flat 的公共误差在比值中相消，
  把“主平场”列为已解释项不成立（§3.4）。
- “PSF/measure 受限但 usable”：真实产品只有 2 星且一负，不能称估计量成立（§3.2）。

## 6. 是否允许阶段二

**不允许。** 依据：(1) 正式 Normalize→Mosaic→Export 链未产出任何一帧正式产品；
(2) Normalize 产品自身 signal/support 长度与 `build_drizzle` 叶数矛盾、tiles 空、tile 头非法；
(3) Mosaic 内存与 Export 闭包两个 P0 未修；(4) 真实帧 PSF/测光 N=2 + 负通量，估计量未建立；
(5) 阶段一完成定义 4 项 FAIL/2 项 PARTIAL，不满足“闭环后重跑再交审”的门槛。
上述 5 条也应是**阶段一内**的修复项，不是阶段二 Issue 的理由。

## 7. 我实际验证 / 未验证

**实际验证**：Branch/HEAD、交付 FITS SHA256；astropy 读回 shape/dtype/BUNIT/NaN 及 NaN=raw==65535 逐像素；
用原始 light+3 master 独立重算 crop 并逐像素比对（1.95e-3）；wcs JSON 独立解码 vs verify 10 点（6.4e-12″）；
PM 传播重算 x_pred（5e-4 px）；从 residuals CSV 重算 train/holdout RMS；111 颗 Gaia 星投影落点统计；
Normalize stdout/trace/对象 JSON/manifest/frame_science；Dir0/Dir130000/Dir20000 各 5 个平面 finite=0；
tile 头原始 80 字节卡片；`measured_stars` 2 颗含负通量；lowbg 4 星比值；代码 diff 的 5 处实现逻辑；
`fit_work` 与 trace 实际 work 的对比。

**NOT_VERIFIED（不冒充已验证）**：20×10,000 tiles 全量（仅 15 平面抽样）；Mosaic OOM 与 Export `corrupt_product` 的重跑；
mosaic proof 下游是否再校验；并发命令 scope 的账本行为；`fit_work` 是否与实际 template 计费路径一一对应
（只对比了声明上界与 trace 计费）；负通量测量的根因；65535 阈值对真实饱和的覆盖率（无 SATURATE 关键字）；
28% 的系统项分离；响应模型的绝对定标。

## 8. 最小放行条件（阶段一内）

1. `SignalEstimate`/`SupportMap`/tile 发布三者长度与内容一致性检查；不一致必须非零退出；tile 头必须合法 FITS。
2. Mosaic 流式/分块 + 在 12 GiB 账本内产出 1 帧正式 mosaic；Export 闭包重开负例。
3. 真实帧 PSF/测光：修局部背景模型；N≥? 的排异要有统计依据；负通量必须作为失败而非发布。
4. 上述代码修复需带**独立负例**（空支持、长度不一致、空 proof、非法 tile 头、转置 calibration 平面）。
5. 28% 零点差写成“未分离系统项”，或给出可分离的天光/孔径证据。

---
附：本报告脚本 `verify_fits.py`、`verify_product.py` 在 `run/m42-s1/review/independent/`，只读运行。
