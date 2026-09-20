# STARDET-CONFORM-INVEST — 星检链符合性调查（调查项 ② / ③）

> 分片: STARDET-CONFORM-INVEST（AstroCS RELEASE-02 星检链符合性调查）
> 上游: `reports/RELEASE-02/conformance/CONFORM-SWEEP-1.md` 的 CONFORM-SWEEP-1-002 / -003（两条高风险不符）
> 工作目录: `/workspace/Astro CS Database`（HEAD `cbda64a1424030f977fbaa777fb3923bdcfc959f`）
> 性质: **只调查、只定案、只给方案；未改任何生产代码/文档；零 git 写；未跑 ninja/cmake/ctest**
> 独立重链: 调查用探针程序**独立链接** `build/libastrocs_phase1_stars.a` + `build/libastrocs_p1_sdet.a`（`g++ -O2 -fopenmp`），未触发任何构建系统
> 数值底: 真实数据 `run/RELEASE-02/L4-rebuild/norm/t2_m2_red/`（M42，4096x4096，4 帧）+ Gaia DR3（`GaiaDR3/*.xpsd`，经生产 `gaia_client` 锥形搜索）
> 产物: `run/RELEASE-02/stardet-invest/`（脚本 / 星表 / 日志）
> TMPDIR=`/dev/shm/astrocs_sdi`（收尾清理）

---

## 0 结论速览

| 项 | 定案 | 一句话 |
|---|---|---|
| **②** 生产检测器 != 规范唯一权威生产源 | **确认不符（C1）** | `sdet_api.cpp` 可编译/可链接/可运行且**已在生产链上跑**（但只在 `p1_op_wcs` 内）；交付的 `p1_sources.json` 由 177 行 wrapper 产出，与 sdet 同帧差 **235x** 星数、**99.67%** 的交付源无 sdet 对应、Gaia 外部纯度 **0.21% vs 45.55%**。**符合规范意图的是 sdet**；切换可行但属科学影响面变更，须走变更 claim + 重新验收。 |
| **③** 质心 `index-is-center` vs 规范 `index+0.5` | **不是缺陷（约定一致）** | 生产 wrapper 的 `px=nx` **精确等于**全链登记的统一契约「index-is-center」（`star_coord_contract.h`）；`p1_sources.json` 的**全部**消费方（孔径测光/框测光/PSF/SNR）都实现 index-is-center；WCS 路径走 sdet（index+0.5）也自洽。真实星场 + Gaia 实测：`wrapper+0.5` 与独立高斯真值差 **(-0.014, +0.034) px**，**不存在 0.5 px 偏移**。审计 003 把 **两个不同产品**（DATA-P1-SOURCES vs DATA-P1-STAR）的坐标口径当成同一口径比较。 |
| 旁支 | 新发现（非 ②③） | 链内 WCS 在**标准 FITS 口径**下相对 Gaia 有 **约 (+0.26, +0.62) px** 的空间恒定零点偏移（N=203，sd 0.11/0.13），**与质心口径无关**（sdet 与 wrapper 两种读法恒差 0.5 px）。建议另立调查项。 |

---

## 1 调查项 ② — 生产检测器 != 规范指定「唯一权威生产源」

### 1.1 规范侧（只读引用）

| 锚 | 陈述 |
|---|---|
| `docs/algorithms/STAR_DETECTION_ALGORITHMS.md:8-9` | 「**唯一权威生产源: lib/algorithms/star_detection/src/sdet_api.cpp**（2555 行实测）…；合同头 `include/star_detector.h`（73 行）；**禁止手抄他版**」 |
| `docs/algorithms/STAR_DETECTION_ALGORITHMS.md:33-82` | 第 2 节逐条冻结 20 条公式：行差分 bgnoise（3 轮 5-sigma clip、MAD 1.482602218505602、x0.7071）、`threshold=median+5*bgnoise`、`dynrange=min(max,65535)-bg` / `minsatlevel=0.7*dynrange` / `satrange=0.1*dynrange`、饱和双条件、一阶导质心 `-0.5-d1rl/(d1rr-d1rl)`、饱和 edge-walking、二阶导零交叉、`s_factor=3.7172`、对称性门、`matchradius=max(1,floor(0.2R))` 曼哈顿去重、椭圆高斯 GSL TR-LM 7 参、`fwhm=2.3548*sigma`、`mag=-2.5*log10(sum_box)`、`is_saturated=(A>dynrange)`、mag 升序 stable_sort + dedup 三规则 |
| `docs/science/STAR_DETECTION.md:46-49` | 「生产路径 `sdet_detect_impl`，`sdet_api.cpp:1599-2353`」 |
| `docs/science/STAR_DETECTION.md:16-18` | 完备性/虚警由合成星场验收（召回 >=99% @SNR>=10；虚警 <=0.1/千像素，纯噪声场） |
| `docs/science/STAR_DETECTION.md:57-62` | TEST-STAR-DESIGN-001 F1-F6 为冻结测试设计，容差冻结不得放宽 |

### 1.2 实现侧

| 锚 | 事实 |
|---|---|
| `lib/infrastructure/scheduler/src/module_adapters.cpp:2050` | `const astrocs::phase1::StarDetector det(5.0);` -> `det.detect(...)`（`:2062`） |
| `lib/algorithms/star_detection/wrapper_phase1/star_detector.cpp`（**177 行**） | `:40-60` 全局 2 轮 `median±3sigma`；`:97-108` 3x3 局部极大；`:114-131` 3x3 邻域「留最强」去重；`:139-152` 5x5 正性截断盒和 + 矩质心；`:167` `peak>50000` 饱和位 |
| `module_adapters.cpp:2013-2018` | 自陈：「本节点检测器是 `wrapper_phase1` 的 P1-003 桥接类 …**不是** `lib/algorithms/star_detection` 的 sdet —— sdet 只被 wcs-platesolve 节点使用」 |
| `module_adapters.cpp:2093-2099, 2141-2146` | 由 wrapper 目录写 `p1_sources.json`（DATA-P1-SOURCES）与 `p1_psf.json` |
| `module_adapters.cpp:2812, 2831` | sdet 仅由 `p1_op_wcs` 经 `sdet_create` + `ipv_set_detector_handle` 使用 |

### 1.3 问题 1：`sdet_api.cpp` 可编译、可链接、可运行吗？——**三者皆可，且已在生产链上运行**

| 判据 | 证据 |
|---|---|
| **可编译** | `CMakeLists.txt:684-695` `add_library(astrocs_p1_sdet STATIC …sdet_api.cpp sdet_detector.cpp sdet_image.cpp sdet_log.cpp sdet_background.cpp)`；`build/CMakeFiles/astrocs_p1_sdet.dir/…/sdet_api.cpp.o` 在位（203,184 B，mtime 09-17 07:54 晚于源 09-17 02:27，**非陈旧**） |
| **可链接** | `build/libastrocs_p1_sdet.a`（347,236 B）在位；依赖 `gsl/gslcblas/m` 与 `OpenMP::OpenMP_CXX`（`:694-695`），系统 `/usr/lib/x86_64-linux-gnu/libgsl.so` 在位 |
| **可运行（独立重链实验）** | 本调查探针**不经构建系统**直接 `g++` 链接 `libastrocs_p1_sdet.a` 后成功调用 `sdet_create -> sdet_detect_ex_f64 -> sdet_free_detect_ex -> sdet_destroy`，`rc=0`，输出 1,473 星，耗时 3.58 s（见 1.4） |
| **在生产链上跑** | `p1_op_wcs` 每帧都经 IPV detector handle 触发 sdet（`module_adapters.cpp:2812, 2831`）——RELEASE-02 的 4 帧 WCS 都是 sdet 检测出来的 |

**API 面差距（`include/star_detector.h:31-67` vs `p1_sources.json` 需要的列）**

| `p1_sources.json` 列（`module_adapters.cpp:2142-2145`） | sdet 主输出 | 桥接可行性 |
|---|---|---|
| `x`, `y` | `out_x/out_y`（double） | 直接可用（口径见第 2 节） |
| `flux` | `out_flux`（float，= A_fit 振幅，`sdet_api.cpp:2304`） | 语义不同（见 CONFORM-SWEEP-1-004），需登记 |
| `fwhm_px` | 无主列 | **extras 可得**：`extra_names={"fwhm_x","fwhm_y","sx","sy"}`（`sdet_api.cpp:820-861`） |
| `ellipticity` | 无主列 | extras `sx/sy` 可推 `1-min/max` |
| `snr` | 无主列 | extras `amplitude/background` + 帧 sigma 可推 |
| `quality`（1=饱和 2=边缘） | `out_saturated`/`out_has_saturated` | 饱和可直接映射；边缘位需 extras `r/cand_R` 或帧边界判定 |
| `id`（`src-<idx>`） | 无 | 节点自生成 |
| — | `mag`、`has_saturated` | 交付产品当前不写 |

**结论**：API 差距**不需要改 sdet**（extras 机制已具备），但需要一个**显式的列语义桥接层**；差距是「工程接线 + 列语义登记」，不是「算法不可用」。
### 1.4 问题 2：同一真实数据上的差异（数值）

**实验设置**：真实帧 `cleaned_M42_M2_T2_flying_dutchman-20251212@020002-300S-Red.fts`（4096x4096 float32，来自 `run/RELEASE-02/L4-rebuild/norm/t2_m2_red/`）。
- wrapper：`StarDetector(5.0)`（生产 `module_adapters.cpp:2050` 同参）
- sdet：生产 `SDetParams`（`module_adapters.cpp:2801-2811` 同值：structureLayers=5, hotPixelFilterRadius=2, iterativeClipSigma=5, iterativeMaxRounds=3, medianFilterDetail=2, maxStars=2000, fitRadius=0, fwhmClipSigma=3, maxAxisRatio=2）
- 探针：`run/RELEASE-02/stardet-invest/sdi_det_compare.cpp`；星表 `run/RELEASE-02/stardet-invest/det/*.csv`；日志 `logs/det_run.log`

**(a) 检测星数（同一帧、同一输入）**

| 检测器 | N | 耗时 | 备注 |
|---|---|---|---|
| wrapper（生产交付源） | **345,960** | 3.95 s | 与交付 `p1_sources.json` 的 `n_sources=345960` **逐值相同**（探针保真度校验通过）；bg=198.0496、sigma=38.2220 也与交付一致 |
| sdet（规范唯一权威源） | **1,473** | 3.58 s | maxStars=0 与 2000 **同值**（截断未生效）；候选 1,692 -> 拟合 1,557 OK -> 门后 1,473（饱和 65 / 正常 1,408） |

**差异 235x**（345,960 / 1,473）。sdet 内部量：`bgnoise=14.1242`（行差分）、`threshold=median+5*bgnoise=286.17`、`dynrange=65319.5`、`minsatlevel=45723.6`、`satrange=6531.9`、`s_factor=3.7169`、`locthreshold=70.62`；FWHM 中位 3.4788、MAD 0.4253。wrapper 阈值 `bg+5*sigma=198.05+5*38.22=389.1`。

**(b) 互匹配（1.0 px 容差，wrapper 星表为参考）**

| 量 | 值 |
|---|---|
| sdet 在 wrapper 中有对应者 | 1,152 / 1,473 = **78.21%**（wrapper 漏掉 sdet 找到的 21.8% 星） |
| wrapper 被 sdet 命中者 | 1,152 / 345,960 = **0.33%** |
| 即 **交付 `p1_sources.json` 中 99.67% 的源没有 sdet 对应** | 344,808 条 |
| 未匹配 wrapper 源的 flux 分位 | p10=4,733 / p50=9,362 / p90=53,910 / p99=166,900 ADU（约 6 sigma 到 4,400 sigma 以上） |

**(c) 位置差（sdet - wrapper，含口径差）**

全部匹配对：`dX med=+0.580, dY med=+0.381`（sd 0.22，受 wrapper 盒质心噪声污染）。
**孤立亮星（Gaia G<15、独立高斯拟合真值，N=58）**：

| 对照 | dX | dY | sd |
|---|---|---|---|
| `sdet - 高斯真值` | **+0.0016** | **-0.0006** | 0.001 / 0.002 |
| `wrapper - 高斯真值` | **-0.5140** | **-0.4660** | 0.079 / 0.076 |
| `sdet - wrapper` | **+0.5251** | **+0.4601** | 0.084 / 0.086 |

即：**sdet 与 wrapper 恰好差 0.5 px（两个登记在册的不同坐标口径），且 sdet 落在标准 FITS 像素中心系、wrapper 落在 index-is-center 系**（详见第 2 节）。

**(d) 完备性（注入-恢复，真实帧底 + 真实 bg/sigma，高斯 PSF FWHM=3.48 px，每档 400 颗）**

| 注入 SNR | wrapper 召回 | sdet 召回 |
|---|---|---|
| 10-100 | **100.0%** | **100.0%** |
| 5-50（含 5-10 段） | 98.0%（SNR5-10: 93.7%） | 82.5%（SNR5-10: **44.4%**） |
| 3-10 | 65.5%（SNR3-5: 30.5%） | 22.0%（SNR3-5: **0.0%**） |

- 规范要求「召回 >=99% @SNR>=10」：**两者都达标**。
- SNR 5-10 段 sdet 明显更保守（平滑 + 11x11 peaker + 对称性门 + LM/reject 门），wrapper 更宽松。

**(e) 虚警**

| 场景 | wrapper | sdet | 规范门 |
|---|---|---|---|
| **纯噪声场**（bg=198.05、sigma=38.22 由真实帧实测；16.78 Mpx） | 8 颗 = **0.00048 /千像素** | **0** | <=0.1 /千像素，**两者都过** |
| **真实 M42 帧** | 345,960 | 1,473 | 规范未定义真实场虚警门 |

- wrapper 检测密度分区（/千像素）：洁净天区 **0.12-0.24**；M42 星云区 **30.96 / 76.75 / 48.33 / 96.16 / 63.50**。
- 即：wrapper 的 34.5 万条**不是统计噪声**（纯噪声场只有 8 条），而是**星云结构 / PSF 翼 / 平场残差 / 宇宙线等非星结构**。

**(f) 外部纯度（Gaia DR3 为真值，经链内 WCS 投影到天球）**

| 星表 | N | 最近 Gaia(G<=16) <1 px | <2 px | 到最近 Gaia 星的中位距离 |
|---|---|---|---|---|
| wrapper | 345,960 | 732（**0.212%**） | 1,079（0.312%） | **56.1 px** |
| sdet | 1,473 | 671（**45.553%**） | 843（57.230%） | **1.1 px** |

Gaia(G<=16) 1 px 内命中率（受 Gaia G<=16 深度限制，非完备性上限）：wrapper 11.7%、sdet 7.1%。

**(g) 下游已受影响的量（交付产物实测）**

| 量 | 当前值（wrapper 源） | 说明 |
|---|---|---|
| `p1_snr.json` `n_snr_input` | **345,960** | SNR 样本 = `p1_sources.json` 全部源（`module_adapters.cpp:3740-3753`） |
| `snr_reference.flux_adu`（组内公共 F0） | **9,373.90 ADU**（块中位 9,280.79） | 由 345,960 条 5x5 盒和取中位，中位源是星云结构而非星 |
| `snr_phot` / `median_snr` | 41.11 | 依赖 F0 与 sigma_F |
| `local_snr.values` | 345,960 个值 | 权重场逐源 |
| `p1_psf.json` 拟合输入 | `n_fit_input=5000`、`n_psf_valid=169` | 最亮 5000 条中 4,830 条拟合失败/被剔 |

### 1.5 问题 3：哪个符合规范意图？——**sdet**

1. ALG-STARDET-001 `:8` 明文把 `sdet_api.cpp` 定为**唯一权威生产源**且「禁止手抄他版」；
2. `sdet_api.cpp:1599-2353` 的 `sdet_detect_impl` 与第 2 节的 20 条公式**逐条对应**（CONFORM-SWEEP-1 第 4.4 节已逐条判「符合（sdet 侧）」，S-039..S-058）；
3. `docs/science/STAR_DETECTION.md:44-49` 把「peaker 七步 + 椭圆高斯 GSL TR-LM」定为**基线算法**，并显式把 wrapper 所在的旧路径登记为遗留（DISP-STAR-005）；
4. 交付链实际跑的是 wrapper，**以 ALG-STARDET-001 为 Oracle 的全部测试/复核都指向错误代码路径**（审计 002 结论成立）。

**但**：`docs/science/STAR_DETECTION.md:10-15` 的验收容差（|dc|<=0.3 px @SNR>=20）与 TEST-STAR-DESIGN-001 F1-F6 **在交付链上从未验收过**；本调查的注入-恢复（1.4d）与纯噪声虚警（1.4e）是首次对**交付检测器**给出的验收型数据，结论是：**规范的两条合成场门（召回/虚警）wrapper 也过**；wrapper 的真实场问题不是统计虚警而是**非星结构混入**。
### 1.6 问题 4：切换的代价与风险（下游影响）

| 下游 | 现状（消费 wrapper 的 `p1_sources.json`） | 切到 sdet 后 | 风险级别 |
|---|---|---|---|
| **PSF 拟合** `dpsf_fit_batch_f64`（`module_adapters.cpp:2112`） | 输入 = 全量 345,960 检测的最亮 5,000 条；实测 `n_psf_valid=169` | 输入 = 1,473 条（< `psf.max_stars=5000`，**不再截断**，`psf_fit_truncated` 转 false）；拟合失败率可能下降 | **高**（psf_params 行集合全变） |
| **孔径/框测光** `p1_op_flux`（`:3118-3119` -> `photometer.cpp:38-66`）与 `p1_op_photometry`（`:3281-3282, 3363-3365`） | 345,960 个孔径中心 | 1,473 个（**-99.6%**）；`p1_flux.json`（480 MB）、`p1_phot.json` 全变 | **高**（产品规模与样本集全变） |
| **测光定标 `k_photo`**（`:3153+`，按 `star_id` 关联 `p1_psf.json` 取 PSF 域解析通量） | 只有 169 条有效 PSF 行可用 | 有效行集合变（可能变多），星匹配/IRLS 样本集变 | **高**（标度数值变） |
| **SNR / 权重** `p1_op_noise`（`:3740-3753, 3786-3807`） | 样本 = 345,960 条盒和；`F0=9,280.8 ADU`（组内公共参考通量，配对性定理）；`local_snr` 345,960 值 | 样本 = 1,473 条真星通量；**F0 会显著改变**（盒和中位 -> 星通量中位）；`frame_depth_m5`、`median_snr`、权重场全变 | **高**（SCI-CW-001 权重链整体改数） |
| **WCS 初始指向 / 板解** `p1_op_wcs`（`:2812, 2831`） | 已用 sdet（index+0.5 = IPV 接口契约） | **无变化**（已一致）；但若两处都跑 sdet 则每帧多算一次（3.6 s/帧）或需复用句柄 | **低**（可零变化，亦可省一次） |
| **导出 / drizzle**（`:2635-2636, 2916-2917` `+kP1FitsPixelOrigin`；`wcs_sip.cpp:247-248`） | 用 WCS + 像素网格，**不消费检测质心** | **无变化** | **无** |
| **性能** | wrapper 3.95 s/帧 | sdet 3.58 s/帧（同帧实测） | **无**（不构成阻塞；但 sdet 已被 wcs 节点调用一次，需评估是否复用） |
| **确定性/线程无关性** | wrapper 有逐位确定性论证（`star_detector.cpp:87-92`） | sdet 第 5 节有 mag 升序 stable_sort + 串行 dedup 的确定性论证 | **低**（需重跑 F3 线程一致性门） |

**核心风险**：这不是「换个检测器」而是**交付产品的源集合与规模变更（345,960 -> 1,473，-99.6%）**，会改到 PSF 行集、测光样本、标度 `k_photo`、SNR 参考通量 F0、权重场与 5 sigma 深度。**必须走变更 claim + 全链重新验收**，不能当作 bugfix 直接切。

### 1.7 问题 5：切换方案（file:line + 工作量）

**方案 A（推荐）：把 `p1_op_star_psf` 的检测源切到 sdet**

| # | 落点 | 改动 | 工作量 |
|---|---|---|---|
| A1 | `module_adapters.cpp:2050-2062` | 用 `sdet_create` + `sdet_detect_ex_f64` 替换 `StarDetector det(5.0); det.detect(...)`；把 `SDetParams` 提为配置键（`psf.sdet.*`，禁编译期硬编码，对齐 `:2225-2232` 先例） | 0.5-1 人日 |
| A2 | `module_adapters.cpp:2090-2100` | `dets[]` 六列由 sdet 主输出 + `extra_names={"fwhm_x","fwhm_y","sx","sy","amplitude","background"}` 构造；`quality` 位语义重定义（饱和位来自 `out_saturated`，边缘位需新定义或删除） | 0.5-1 人日 |
| A3 | `module_adapters.cpp:2140-2146` | `p1_sources.json` 的 `flux` 列语义改为 sdet 语义（A_fit）或改用 `p1_psf_analytic_flux`；同步改 `docs/contracts/DATA_SEMANTICS.md`（新增 DATA-P1-SOURCES 列语义表） | 0.5 人日 + 文档流程 |
| A4 | `docs/algorithms/STAR_DETECTION_ALGORITHMS.md:8-12` | 把 `wrapper_phase1` 从 `legacy_paths` 移出/标注为「非生产」；登记 sdet 为 `p1_op_star_psf` 的实际生产源 | 0.5 人日 |
| A5 | 测试 | 新增「交付链检测器 = sdet」的接线门（行为级：同帧 sdet 与节点输出一致）；TEST-P1-STAR-001 F1-F6 落到交付链 | 2-4 人日 |
| A6 | 验收 | 全链重跑 + 逐产品差异报告（`p1_sources/p1_psf/p1_flux/p1_phot/p1_snr` 全部改数）+ 门禁重跑 | 3-5 人日 |
| **合计** | | | **约 7-12 人日**（不含负责人裁决与重验收窗口） |

**方案 B（备选，若负责人判定「不切换」）：规范侧订正**

| # | 落点 | 改动 |
|---|---|---|
| B1 | `docs/algorithms/STAR_DETECTION_ALGORITHMS.md:8-12` | 撤销「唯一权威生产源 = sdet_api.cpp」或改写为「sdet 为**算法参考实现**；RELEASE-02 交付链生产源 = `wrapper_phase1/star_detector.cpp`（登记为 DISP）」 |
| B2 | `docs/algorithms/STAR_DETECTION_ALGORITHMS.md` 第 2 节 | 为 wrapper 的 5 条实际公式（2 轮 median±3sigma、3x3 局部极大、3x3 去重、5x5 盒和+矩质心、`peak>50000`）补逐条冻结与 DISP 登记 |
| B3 | `docs/science/STAR_DETECTION.md:44-49,57-62` | 把基线算法与 TEST-STAR-DESIGN-001 的 Oracle 指向 wrapper；撤回对 sdet 生产路径的断言 |
| B4 | `docs/science/STAR_DETECTION.md:16-18` | 保留合成场门，但明确其验收对象是 wrapper，并补真实场「非星结构混入」的登记（DISP） |

> **本分片建议 A**：规范逐条冻结的 20 条公式 + 「唯一权威生产源」措辞 + SCI 基线选择 + 遗留双实现登记（DISP-STAR-005）**四条独立证据同向**指向 sdet；方案 B 需要把「非星结构混入 34.5 万条」也一起登记为可接受现状，代价更大。
> **但这是科学影响面变更，须负责人裁决**（AGENTS.md 第 9 节：涉及最高设计/文档集与现状冲突且需要改文档）。

---

## 2 调查项 ③ — 质心 `index-is-center` vs 规范 `index+0.5`

### 2.1 规范侧与实现侧

| 侧 | 锚 | 陈述 |
|---|---|---|
| 规范 | `docs/science/STAR_DETECTION.md:38-40` | 「SCI-AST-001：**像素中心=索引+0.5 约定**（ALG-STARDET-001 第 2 节 残差坐标），**star_det 权威块**消费方（plate solve fallback）按此约定解析坐标（DATA_SEMANTICS 第 17 节）」 |
| 规范 | `docs/algorithms/STAR_DETECTION_ALGORITHMS.md:65-66` | 「残差坐标 `dx=x+0.5-cx`（像素中心=索引+0.5，`:510-513`）」 |
| 规范 | `docs/contracts/DATA_SEMANTICS.md:752` | `star_det` 块 `x / y` = 「pixel（0-based，**像素中心=索引+0.5**）」 |
| 实现 | `lib/algorithms/star_detection/wrapper_phase1/star_detector.cpp:146-152` | `const double px = nx, py = ny;` … `s.x = m10/m00;`（无 +0.5） |
### 2.2 全链约定审计（问题 7）

**关键事实：链内存在一份登记在册、带机器门、三系并列的坐标契约唯一事实源。**

`lib/infrastructure/pipeline/orchestrator/cpp/include/star_coord_contract.h:4-18, 41-57`：

| 系 | 定义 | 值 = 连续坐标 - 0.5？ |
|---|---|---|
| (1) 连续系 = **IPV 接口契约** | 像素中心 = 索引 + 0.5；sdet 检测输出即此系（`sdet_api.cpp` 残差 `dx=x+0.5-cx`） | 否（= 标准 FITS 像素中心） |
| (2) **统一契约 = index-is-center** | `star_measurements` 权威块；`star_measurement_from_sdet(v)=v-0.5` | 是 |
| (3) dpsf 拟合中心 | **已是统一契约**（`sp.dx = x - cx`，拟合中心回移 `img_cx = cx + x0`）；`star_measurement_from_dpsf(v)=v`（恒等） | 是 |

机器门：`lib/algorithms/psf/tests/p1psf/p1psf_center_contract_gate.cpp`（G-P1-CENTER-CONTRACT-1，C1 契约恒等式 / C2 psf->photometry vs 独立 gnomonic + astropy 第三方 / C3 psf->snr / C4 导出链单次 +1 桥接 / C5 原点鉴别力 >=0.9 px + 负例注入）与 `p1psf_centroid_gate.cpp`（G-P1-CENTROID-1）。

**逐消费点审计（file:line + 口径）**

| # | 消费点 | 输入来源 | 该消费方**要求**的口径 | 实际喂入 | 判定 |
|---|---|---|---|---|---|
| 1 | 孔径/框测光 `p1_op_flux`：`module_adapters.cpp:3118-3119` -> `Photometer::measure`（`lib/algorithms/photometry/wrapper_phase1/photometer.cpp:38-66`，`floor(cx-r)` 直接当数组下标） | `p1_sources.json.x/y` | **index-is-center**（数组下标） | index-is-center | 一致 |
| 2 | 框测光（PC/帧测光）`p1_op_photometry`：`module_adapters.cpp:3281-3282, 3363-3365` -> `pc::WcsTransform::pixelToSky`（`lib/algorithms/photometry/cpp/src/wcs_transform.cpp:189-191`：`dx = x - (crpix-1)`） | 同上 | **index-is-center** | index-is-center | 一致 |
| 3 | PSF 拟合 `dpsf_fit_batch_f64`：`module_adapters.cpp:2093-2094, 2112-2114` -> `lib/algorithms/psf/src/dpsf_psf.cpp:295-296`（`sp.dx = x - cx`）、`:437-449`（`img_cx = cx + x0`） | 同上 | **index-is-center**（恒等，`star_coord_contract.h:46-51`） | index-is-center | 一致 |
| 4 | SNR 控制点 `snr_extract_model_v3` -> `lib/algorithms/noise_snr/cpp/src/snr_estimator.cpp:504-512`（`dx = x - (crpix-1)`） | `p1_psf.json` 的 `cx/cy`（= dpsf 输出，index-is-center） | **index-is-center** | index-is-center | 一致 |
| 5 | 板解/WCS `p1_op_wcs`：`module_adapters.cpp:2812, 2831` -> IPV detector handle -> sdet | sdet 输出 | **index+0.5**（IPV 接口契约，`DATA_SEMANTICS.md:819`） | sdet = index+0.5 | 一致（**无需桥接**，因为 sdet 本身就是该系） |
| 6 | 导出/WCS 自检 `WcsTan`：`module_adapters.cpp:2635-2636, 2916-2917`（`xp = x + kP1FitsPixelOrigin`，`kP1FitsPixelOrigin=1.0` @ `:374`）-> `lib/algorithms/platesolve/wrapper_phase1/wcs_tan.cpp:8-9`（`dx = x - crpix1`） | 0-based 数组下标 | **FITS 1-based**（须恰好一次 +1） | 单次 +1 | 一致（`:379` 明文「漏加 = 恒定 1px，加两次 = 双重桥接」） |
| 7 | drizzle/HIPS `WcsSip::pixelToSky`（`lib/algorithms/drizzle/healpix_drizzle/wcs_sip.cpp:246-248`：`dx = x - (crpix-1)`）+ 网格取样 `drizzle_engine.cpp:1883`（`vx-0.5` / `y±0.5`） | 像素网格 | **index-is-center** | index-is-center | 一致 |
| 8 | `star_measurements` 权威块（orchestrator 写端 `orchestrator.cpp:1817+`，读端 `:2513+`，桥 `orchestrator.h:340-360`） | sdet / dpsf | **index-is-center** | 经 `star_coord_contract.h` 三函数 | 一致（本次 RELEASE-02 CLI 链不经此路，但契约同源） |

**全链结论**：**没有任何一处消费方要求 `p1_sources.json` 是 index+0.5**；要求 index+0.5 的只有 **IPV 接口**，而它拿到的是 **sdet 直出**（不经 `p1_sources.json`）。因此不存在「某处 +0.5、某处不用」的真实偏移。

### 2.3 数值判定（问题 8）——真实星场 + Gaia

**实验 1：质心口径（WCS 无关，最决定性）**
选取 Gaia DR3 G<15、孤立（30 px 内无邻星）、未饱和、峰值落在预测 2 px 内的星（N=58），用**独立 2D 高斯 + 常数背景最小二乘**（11x11 窗口，坐标定义 `X = 下标 + 0.5` = 标准 FITS 像素中心）作真值：

| 对照 | dX med | dY med | sd |
|---|---|---|---|
| `wrap_replica - p1_sources.json`（逐位复算校验） | **+0.0000** | **+0.0000** | **0.0000** |
| `(p1_sources.json.x + 0.5) - 高斯真值` | **-0.0140** | **+0.0340** | 0.079 / 0.076 |
| `p1_sources.json.x - 高斯真值` | -0.5140 | -0.4660 | 0.079 / 0.076 |
| `sdet.x - 高斯真值` | **+0.0016** | **-0.0006** | 0.001 / 0.002 |

**读法**：
- 生产 wrapper 质心**精确等于 index-is-center**（+0.5 后与独立真值一致到 **0.04 px**，远优于 0.5 px 判据）；
- **sdet 精确落在标准 FITS 像素中心系（index+0.5）**，与 `star_coord_contract.h` 的 (1)/(2) 两系定义逐值吻合；
- 因此 **wrapper 与 sdet 之间的 0.5 px 是两个登记在册的不同坐标系的定义差，不是误差**。

**实验 2：Gaia 绝对残差（问题 8 要求的「vs Gaia 真值」判据）**
经链内 WCS 投影到天球，与 Gaia DR3（XPSD，J2016.0 位置；`gaia_client_cone_search` 按契约强制 `parallax/pmra/pmdec=0`，见 `gaia_client.c:2072`，故无法做自行历元改正；但抽样核对显示该场 G<15 星自行 <= 数 mas/yr，10 年 <=0.02 px，可忽略）：

| 读法 | dX med | dY med | N | sd |
|---|---|---|---|---|
| 链内 WCS @ **wrapper 质心**（index-is-center） | **-0.251** | **+0.175** | 56 | 0.14 / 0.14 |
| 链内 WCS @ **标准质心**（= sdet 口径） | **+0.261** | **+0.646** | 55 | 0.10 / 0.09 |
| 两者之差 | **+0.512** | **+0.471** | — | 约 **(+0.5, +0.5)**（校验通过） |

**没有出现「约 0.5 px 系统偏移」**：把 wrapper 质心按**它自己的登记口径**解读时，残差是 0.31 px（且这 0.31 px 完全来自链内 WCS 自身的零点，见第 3 节——因为 sdet 口径下同样有 0.69 px）；把 wrapper 质心**当成 index+0.5** 解读时才会凭空多出 0.5 px，而那正是审计 003 建议的改法（会把误差**从 0.31 加到 0.70**）。
### 2.4 定案（问题 6）——**不是缺陷；是「全链一致约定」**

| 判据 | 结论 |
|---|---|
| 生产质心口径 | = **index-is-center**（实测 0.04 px 内一致） |
| 全链是否统一 | **统一**：`star_coord_contract.h` 登记的统一契约就是 index-is-center；`p1_sources.json` 的 4 条消费路径（孔径测光 / 帧测光 / PSF / SNR）**全部**实现 index-is-center；WCS 路径用 sdet（index+0.5）也自洽；导出链有且只有一次 +1 桥接 |
| 是否存在「某处 +0.5、某处不用」 | **不存在**（2.2 逐点核过） |
| 规范是否被违反 | **没有**。`STAR_DETECTION.md:38-40` / `ALG-STARDET-001:65-66` 的 index+0.5 是对 **sdet 输出 / `star_det` 权威块（DATA-P1-STAR）** 的规定；交付产品是 **DATA-P1-SOURCES**（wrapper 产出），二者**不是同一产品**。审计 003 把两者当成同一口径比较，属**口径混淆**（与审计 001/030/032 同型）。 |
| 真实 0.5 px 偏移 | **不存在**（数值见 2.3） |

**因此登记为「非缺陷」（口径约定一致），撤销 CONFORM-SWEEP-1-003 的「不符」判定。**

**仍然成立的两点（如实登记，非科学缺陷）**：
1. **文档缺口（建议修）**：`docs/contracts/DATA_SEMANTICS.md` **没有 DATA-P1-SOURCES 的列语义表**（全文只有 3 处引用、无表，`:488/:515/:566`），因此 `p1_sources.json.x/y` 的像素原点**在任何合同里都未被登记**。这是「读者无法机械复核」的真实缺口，也是审计 003 误判的土壤。
2. **规范措辞可加固**：`STAR_DETECTION.md:38-40` 的「star_det 权威块消费方」一句可显式补注「DATA-P1-SOURCES 不属本约定，其口径见 `star_coord_contract.h` 统一契约」。

### 2.5 修复方案（问题 9）

**若按审计 003 原建议改代码（`star_detector.cpp:146-152` 加 +0.5）——禁止**：会同时破坏 4 条消费路径（2.2 的 #1-#4）与 `star_coord_contract.h` 的恒等式，把 0.31 px 的残差放大到 0.70 px，并触发 G-P1-CENTER-CONTRACT-1 / G-P1-CENTROID-1 变红。**建议不做任何生产代码改动。**

**建议的（纯文档）动作清单**：

| # | 落点 | 改动 | 类型 |
|---|---|---|---|
| C1 | `docs/contracts/DATA_SEMANTICS.md`（DATA-P1-SOURCES 段） | 新增列语义表：`x/y` = pixel（0-based，**index-is-center**，= 标准 FITS 像素中心 - 0.5；与 `star_coord_contract.h` 统一契约同系）；`flux` = 5x5 正性截断盒和（DISP）；`fwhm_px` = 检测侧高斯 FWHM；`quality` 位语义 | 文档变更 claim |
| C2 | `docs/science/STAR_DETECTION.md:38-40` | 补注：本约定适用于 `star_det` 权威块（DATA-P1-STAR）；DATA-P1-SOURCES 用统一契约 index-is-center | 文档变更 claim |
| C3 | `docs/algorithms/STAR_DETECTION_ALGORITHMS.md:65-66` | 同上补注（与 C2 同批） | 文档变更 claim |
| C4 | `docs/algorithms/GATES_AND_TOLERANCES.md` | 把 G-P1-CENTER-CONTRACT-1 从「待登记」转正式登记（`p1psf_center_contract_gate.cpp:23` 自陈待登记） | 文档变更 claim |
| C5 | CONFORM-SWEEP-1 台账 | 把 003 改判「非缺陷（口径混淆）」，并记入 2.4 证据 | 台账订正 |

**工作量**：C1-C5 合计 **约 0.5-1 人日**，零代码改动、零回归风险。

---

## 3 旁支发现（**不属于 ② / ③**，但由本次数值实验暴露，建议另立调查项）

> 本项**未定案**，仅登记证据与量级，避免被误读为 ② 或 ③ 的一部分。

### 3.1 链内 WCS 的绝对零点相对 Gaia 有约 0.7 px 偏移（标准口径下）

- 方法：sdet 星表（1,473 颗）经**链内 WCS（TAN+SIP，`p1_wcs.json`）**投影到天球，与 Gaia DR3 最近邻匹配（2 px），N=221；残差用链内 CD 逆换算回像素。
- 结果（`logs/sdi9.log`）：`dX med=+0.262, dY med=+0.624`（|d|=0.688 px，sd 0.105/0.128，N=203）；**空间分区恒定**（4x4 分区内 dX +0.18..+0.31、dY +0.53..+0.72），是**零点平移**，不是旋转/尺度。
- 外部交叉：raw FITS 头里的 **PinPoint TAN WCS**（MaxIm DL 外部解，无畸变项）在同样星上给 `dX med=+0.032, dY med=-0.263`（sd 0.76/0.68，|d|=0.71 px，散布大是无 SIP）。
- 两帧一致性（`logs/sdi8.log`，M42 不同指向）：chain 020002 `(+0.249,+0.624)`、033733 `(+0.504,+0.716)`；PinPoint 同期 `(+0.098,-0.275)`、`(+0.175,-0.201)`。
- **与质心口径无关**：sdet 与 wrapper 两种读法恒差 (0.5, 0.5) px（2.3），偏移量不随口径消失。
- 链内 WCS 的 crpix = **2048.5**，PinPoint = **2048.0**。
- 链内自检（`module_adapters.cpp:2940-2955` roundtrip + forward-cross 门）**对此零鉴别力**（自身与自身比，源码注释亦自陈「对成对单位错零鉴别力」）。

### 3.2 `p1_wcs.json` 的 `samples[]` 像素原点声明与实际不符（1 px 记录不一致）

- 声明：`{"pixel_origin": "0-based array index (index-is-center); FITS 1-based xp = x + 1"}`、`{"fits_pixel_origin": 1.0}`（`module_adapters.cpp:3000`）。
- 实测（`logs/sdi2.log`）：把 `samples[]` 的 `(ra,dec)` 用**同一 wcs 对象**（TAN，crpix=2048.5）反解，落点 = `(x, y)`（**偏移 0**），而非声明的 `(x+1, y+1)`（偏移 -1.000）。**在 L4-rebuild / perf-drz / f-instr-fix 三个不同 run 的产物上完全一致复现**。
- 另注：`samples[]` 的 ra/dec 是**纯 TAN**（不含 SIP），而 `p1_op_wcs` 的 IPV 分支代码路径确实未对 samples 施加 SIP（`module_adapters.cpp:2916-2928` 只调 `wcs.pix2sky`）；而 `wcs.sip` 的修正量级在视场角上可达 **±17 px**（`A_3_0=-1.03e-9`、`A_1_2=-1.08e-9`，dx=dy=2000 时 A 约 -16.2 px）。
- 因此 `samples[]`（承载 provenance 声明与自检证据的那一段）与实际生效的 WCS（含 SIP）**不是同一个映射**，且原点声明差 1 px。
- **科学产品是否受影响**：drizzle/HIPS 走 `wcs` 对象（含 SIP），**不经 samples**；故本项主要是 **provenance/可审计性缺陷**，但需独立确认。

### 3.3 建议

上呈负责人另立调查项（例如 `SDI-WCS-ZERO-POINT`）：判据 = 用 Gaia 直接拟合零点的绝对残差门（当前链内无此门），并核对 IPV `IpvWcsResult.crpix` 的坐标系定义与 `sdet` 输入的换算是否恰好差 0.5 px。

---

## 4 诚实声明与未决项

1. **③ 判定为「不是缺陷」**，证据是「独立高斯真值 + 全链消费点逐点审计 + 登记在册的契约与机器门」三重同向；**没有为凑数把它写成缺陷**。审计 003 的「恒定 0.5 px 偏移」实测**不存在**。
2. **② 判定为「确认不符」**，但**不主张立刻切换**：切换会改 5 个交付产品的数值与规模（1.6），属科学影响面变更，须负责人裁决 + 变更 claim + 重新验收。
3. **未决**：
   - 3.1 链内 WCS 零点偏移的**归因**（IPV crpix 口径 / sdet 输入换算 / 其他）未定；本分片只给出量级与空间结构。
   - Gaia 自行历元改正**不可得**（`gaia_client_cone_search` 按契约强制 pm=0，`gaia_client.c:2072`）；抽样核对显示该场影响 <=0.02 px，但未做全样本统计。
   - 注入-恢复用的是**高斯 PSF**（FWHM=3.48 px，取自 sdet 实测中位）；真实 PSF 更接近 Moffat4，SNR 5-10 段的召回数字对 PSF 形状敏感。
   - 本调查只跑了 **1 帧**做检测器对比（t2_m2_red 首帧）、**2 帧**做 WCS 绝对残差；未做全 12 瓦片统计。
4. **硬约束遵守**：未改任何生产代码/文档；零 git 写（仅只读 `git log/show/cat-file`）；未跑 `ninja`/`cmake`/`ctest`（探针程序为独立 `g++` 重链）；TMPDIR=`/dev/shm/astrocs_sdi` 已清理。
---

## 5 证据清单与复现

**脚本（`run/RELEASE-02/stardet-invest/`）**

| 文件 | 作用 |
|---|---|
| `sdi_gaia_dump.c` / `bin/sdi_gaia_dump` | 生产 `gaia_client` 锥形搜索导出（0.8 度，G<16，1,883 星） |
| `sdi_det_compare.cpp` / `bin/sdi_det_compare` | wrapper vs sdet 同帧对比探针（独立重链） |
| `dump_f32.py` | FITS -> raw float32（喂探针） |
| `sdi1_wcs_compare.py` / `sdi2_chain_origin.py` / `sdi2b.py` | 链内 WCS vs 外部 PinPoint；samples 原点口径 |
| `sdi3_gaia_resid.py` / `sdi4_centroid_verdict.py` | Gaia 绝对残差；三方质心口径判定 |
| `sdi5_grid_resid.py` / `sdi8_multiframe.py` / `sdi9_sdet_gaia.py` | 网格一致性；多帧 WCS 零点；sdet 对 Gaia 空间结构 |
| `sdi6_det_compare.py` / `sdi7_three_way.py` | 检测星表对比；三方质心对照 |
| `sdi10_inject.py` / `sdi11_purity.py` | 注入-恢复 + 纯噪声虚警；Gaia 外部纯度/完备性 |

**数据产物**：`det/wrap.csv`（345,960）、`det/sdet_m2000.csv` / `sdet_m0.csv`（1,473）、`det/*_meta.txt`、`gaia_m42.csv`（1,883）、`sdi4_stars.csv`（58 孤立亮星三方质心）、`inject/*`、`logs/*.log`

**关键复现命令**

```bash
export TMPDIR=/dev/shm/astrocs_sdi
# 1) 导出真实帧为 raw float32
python3 run/RELEASE-02/stardet-invest/dump_f32.py \
  "run/RELEASE-02/L4-rebuild/norm/t2_m2_red/cleaned_M42_M2_T2_flying_dutchman-20251212@020002-300S-Red.fts" \
  /dev/shm/astrocs_sdi/frame_t2m2_f0.f32 4096 4096
# 2) 两检测器同帧跑
./run/RELEASE-02/stardet-invest/bin/sdi_det_compare /dev/shm/astrocs_sdi/frame_t2m2_f0.f32 4096 4096 \
  run/RELEASE-02/stardet-invest/det 2000 both
# 3) 数值判定
python3 run/RELEASE-02/stardet-invest/sdi7_three_way.py     # ③ 三方质心口径
python3 run/RELEASE-02/stardet-invest/sdi6_det_compare.py   # ② 检测对比
python3 run/RELEASE-02/stardet-invest/sdi11_purity.py       # ② Gaia 纯度/完备性
```

**参考（只读）**：`docs/algorithms/STAR_DETECTION_ALGORITHMS.md`、`docs/science/STAR_DETECTION.md`、`docs/contracts/DATA_SEMANTICS.md`、`lib/infrastructure/pipeline/orchestrator/cpp/include/star_coord_contract.h`、`lib/algorithms/psf/tests/p1psf/p1psf_center_contract_gate.cpp`

---

## 6 快照与并发改动提示（可追溯性）

调查期间工作树正被**并行分片修改**（CONFORM-SWEEP-1 报告亦已声明同一情况）。本报告引用的行号对应**本分片实际读取的版本**；下表给出该版本与**当前工作树**的锚点对照，供复核。

| 文件 | 审计基线 md5 | 当前工作树 md5 | 说明 |
|---|---|---|---|
| `lib/algorithms/star_detection/src/sdet_api.cpp` | `db23020bf8d709325fecdc8f8bc8624e` | **同值（未变）** | 与 CONFORM-SWEEP-1 基线一致；本报告 sdet 侧结论对同一源成立 |
| `lib/algorithms/star_detection/wrapper_phase1/star_detector.cpp` | `ffce241f6707aa4b08e454e3271ec4ee`（177 行） | `6d90cf8ec0e22390da02f413bcac3b26` | 并行分片已改（+46/-2）：把硬编码 `peak>50000.0` 换成规范 `dynrange/minsatlevel/satrange` 双条件（CONFORM-SWEEP-1-005 修复）。**质心口径未动**：`const double px = nx, py = ny;` 仍在（当前 `:164`，原 `:146-152`），无 +0.5 |
| `lib/infrastructure/scheduler/src/module_adapters.cpp` | `35288396163311bf12a13d717270cd6e` | `0ffbe09dbd28a174e4474d10d77f22c3` | 并行分片已改（+394/-53）。**本报告涉及的锚点均未改语义**，仅行号位移 |

**行号对照（本报告引用 -> 当前工作树）**

| 锚 | 本报告 | 当前 |
|---|---|---|
| `kP1FitsPixelOrigin = 1.0` | `:374` | `:376` |
| `const astrocs::phase1::StarDetector det(5.0);` | `:2050` | `:2056` |
| `sdet_create(&sp)` | `:2812` | `:2818` |
| `ipv_set_detector_handle(ipv, ...)` | `:2831` | `:2837` |
| `wrapper ... star_detector.cpp` `px = nx, py = ny` | `:146-152` | `:164` |
| 测光消费点 `c.x = s.value("x", 0.0)` | `:3281-3282` | 语义不变（并行改动自带注释「位置口径不变（检测质心, 非本次改动）」） |

**对定案的影响**：
- **② 不受影响**：`sdet_api.cpp` 未变；wrapper 的并行改动只动饱和判定，**不改变检测星数与质心**，故 1.4 的数值对比仍然有效（探针链接的是 `build/libastrocs_phase1_stars.a`，即产出交付产品的同一实现）。
- **③ 不受影响**：质心口径（`px = nx`）与全部消费点（`pc::WcsTransform` / `WcsSip` / `dpsf` / `snr` / `kP1FitsPixelOrigin`）在两版之间**逐字未变**；并行改动反而在测光消费点留下「位置口径不变」的显式确认。
