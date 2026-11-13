# MAIN_PARITY_MATRIX｜main 已投产科学能力的 V2 等价审计矩阵（Issue #126 §2）

> **本文件是审计导航，不是自动生成的报告，也不是验收门禁。** 仓库明确禁止用 CI、自动文档脚本、覆盖率或自动 PASS 判决代替数学证明与独立复核（[AGENTS](../../../AGENTS.md) §2/§4、[IMPLEMENTATION-STATUS](../../audit/IMPLEMENTATION-STATUS.md)）。下表**不写任何 PASS**：有真实产物也只写“已测（范围）”并给出产物路径；只有代码没有实测写 `NOT_VERIFIED`；没有运行写 `NOT_RUN`。
>
> **源码身份**：分支 `rebuild/acsd-v2`，HEAD `c416f0be3`（自动选阶与有界累加器见 `1fa753068`，reviewer 轮次修复见 `c416f0be3`；`1fa753068` 之前为 `d7e5b0ba4`）。下文 `src/...:line` 指 `c416f0be3` 的内容。独立 reviewer（reviewer-126）对 `1fa753068` 的 PASS_SCOPED/FAIL/数据更正见 [issue126 审计记录](../../audit/issue126-2026-11-14.md) §10；本矩阵的"缺口"列已按修复后的状态更新，但**不把修复当 PASS**。
>
> **审计边界**：每行只引用 [00-BASELINE-ASSETS](migration/00-BASELINE-ASSETS.md) §2 判为“正式生产可达”的 main 资产（`git show main:<path>`，main `c5fee09f`）。`phase1_session/`、退役 UPM F1–F6、legacy AIO、`healpix_stack/` 等 ARCHIVE 行不出现在任何一行里——本矩阵的分类与 00 一致。
>
> **路径约定**：Markdown 链接是文档相对路径；其余一律 `仓库根相对路径:行号`。科学推导不在本文件复写：真球面 Drizzle 估计量见 [SPHERICAL_DRIZZLE](../../science/foundation/SPHERICAL_DRIZZLE.md)，天空/共加统计见 [STATISTICS_SKY_COADD](../../science/foundation/STATISTICS_SKY_COADD.md)，Gaia/响应见 [GAIA_RESPONSE](../../science/foundation/GAIA_RESPONSE.md)，各算子的步骤与反例见 [processes/README](processes/README.md)（例如 [geometry.drizzle](processes/geometry.drizzle.md)）。

## 1. 如何读这张矩阵 / How to read this matrix

- 每一行 = Issue #126 §2 要求的一个能力面。列义固定：**main 生产证据**（旧源码真正被 CLI 调用的文件:行/函数）、**V2 消费点**（当前唯一实现的位置）、**缺口**（缩水或未接线的事实，不是意见）、**同输入实测证据**（仓库里真实存在的产物/数字；没有就写 `NOT_RUN`）、**最小修复**、**独立复核要求**。
- 状态词只有三种：`已测(范围)` = 仓库存在可复核产物且本文件给出路径；`NOT_VERIFIED` = 有代码/入口但没有同输入实测；`NOT_RUN` = 没有运行过。**没有 PASS**；一次 exit 0、FITS 能打开、哈希自洽都不能升级状态（[07](migration/07-VERIFICATION.md) V-00/V-05）。
- 性能列一律 `NOT_RUN` 除非有同机同科学策略的 wall/RSS/IO 记录：[06](migration/06-PERFORMANCE.md) 规定没有这种记录就是 `BASELINE_PENDING`，本文件不会用“看起来快”填空。
- 旧 main 的已知错误（四角近似、Fisher→scene、饱和像元当信号、order13 固定阶）在本矩阵里**不是“退化”**：V2 用更严格数学修正它们是允许且被要求的（[MIGRATION](MIGRATION.md) §2、[02](migration/02-GEOMETRY.md) G-01/G-03）；但“更严格”本身也需要独立 oracle，不能由实现者自证。

## 2. 逐项矩阵（14 行 / 7 个要求面）

| # | 能力面 | main 生产证据（`git show main:`） | V2 消费点（工作树） | 缺口 / 缩水 | 同输入实测证据 | 最小修复 | 独立复核要求 |
|---|---|---|---|---|---|---|---|
| 1 | **自动 NSIDE**：选择、失败、clamp、metadata | `lib/algorithms/drizzle/healpix_drizzle/drizzle_engine.cpp:711` `compute_auto_nside_ex`（四叉树+N 维切平面 Jacobian，clamp `[16,4194304]`）；C 入口 `hp_drizzle_api.cpp:1385`；生产调用 `module_adapters.cpp:8527` `p1_op_drizzle` | `src/coordinates/resolution.cpp:219` `decide_leaf_order`（探测 `:160`，clamp `:233`–`:242`）；`apps/cli/commands.cpp:463`–`:486` 解码 `order:"auto"`/整数；`apps/cli/commands.cpp:1363`–`:1445` `plan_only` 预检；`src/workflows/normalize.cpp:385`–`:431` 单次调用取全帧 `max(order)` | 自动规划**只接受 `frame_options[*].supplied_wcs`**（`normalize.cpp:399`–`:406`）；同一命令本会现场做 `sources.astrometry`（`:613`–`:637`），因此“先 Gaia 解算再选阶”在一条 Normalize 内不可达。计划文档（`commands.cpp:670`–`:684`）缺 policy/算子哈希、缺 sampled/invalid 计数，auto 分支不填 `frames[]`（`normalize.cpp:425`–`:430`）。计划字段在旁文件（`drizzle_plan.json`）。过粗显式 order 现默认**拒绝**（无 `allow_coarser_order` 时 `domain` 具名失败，实测退出 4）；带 flag 的诊断产品在 Normalize 产品评估（`product.cpp` 合并 graph assessment）与导出产品（`export_workflow.cpp` 同时匹配 reason/impact）都带 `order_coarser_than_native_scale` 且 `science=conditional`（本轮重新跑 o13 并用 grep 验证）。计划读取入口 `order:"plan"` 已被一次真实 Normalize 消费（退出 0） | 已测(范围)：`run/issue126/preflight/f0/out/drizzle_plan.json`（`plan_only`，1 帧 M5 T2，finest 0.940477″, required_order 18，common 18，clamped=false）；`run/issue126/staging/m5_256_0_0/out/drizzle_plan.json`（normalize 写出的同一决策） | 把 WCS 求解与 Drizzle 拆成两个显式阶段（或允许 Normalize 解算后重算计划并要求全帧一致）；计划进产品并带 policy/code hash、采样数、失败原因 | 独立数值 reviewer 用**稠密** Jacobian 扫描（不是同一函数）在强 SIP/边缘/极区 WCS 上复算，报告采样近似与真最小值的差；并复现“无 supplied WCS”时的具名失败 |
| 2 | **真实 WCS/SIP 角尺度**与“最细尺度”可信度 | `drizzle_engine.cpp:692`–`:790`（同一决策块：自适应细分 + 3D Jacobian）；采样深度常量见 `:453` 注释（`MAX_DEPTH=5 → 1024 叶单元`） | `src/coordinates/resolution.cpp:59`–`:114`（5 点中心差分最小奇异值）；`:124`–`:156` 四叉树；常量 `:20`–`:23`（深度 5、比值 1.25、最小单元 4 px） | 与 main 同一启发式，**没有区间/全域证书**；[02](migration/02-GEOMETRY.md) G01-a.2 要求的“有限采样不是严格最小值证书”的验证在仓库里不存在（`NOT_VERIFIED`）。当前只在 0.94–0.95″/px 的圆对称 SIP 帧上跑过 | 已测(范围)：E1 单帧 finest 0.940477″；E4 两面板 0.952910″/0.953304″（`run/issue126/cross_panel/plan/plan/drizzle_plan.json`） | 增加可选的区间/自适应细分到给定容差，把 sampled/refined 状态写入计划；用解析 WCS（已知 Jacobian）做定量误差表 | 构造解析可算的强畸变 WCS，独立算真 min σ 与 V2 决策的偏差；覆盖 RA wrap、极点、边缘 |
| 3 | **一个全局 leaf order，在 Normalize 前规划** | main 每帧 `p1_op_drizzle` 各自读 auto 请求（`module_adapters.cpp:8527`）；旧 P2 的粗化保护在 mosaic 侧：`lib/algorithms/coverage/README.md:39` `target_order = min(逐帧 hips_order)` | `src/workflows/normalize.cpp:392`–`:431`（同一调用内 `common = max(decision.order)`）；`apps/cli/commands.cpp:1363`–`:1445`（`plan_only` 写计划） | 计划**可被消费且已实测**：CLI 支持 `drizzle.order="plan"` + `plan` 路径（`read_resolution_plan`），读入 `common_leaf_order` 并校验 clamped/high_resolution；本轮用 256² ROI 的冻结计划真实跑了一次 Normalize（退出 0）。跨面板 E4 的两次 Normalize 仍用 `auto` 各自决策（同为 18）；跨调用冻结尚未成为默认流程 | 已测(范围)：`run/issue126/cross_panel/plan/plan/drizzle_plan.json`（两帧同 18）与两次单帧 Normalize 各自的 `out/drizzle_plan.json`（E4） | 让 Normalize 接受计划对象/`--plan`，校验每帧 `required_order ≤ plan.order` 且 order 一致；不一致具名 `plan_mismatch` | 用两个故意不同尺度的裁剪帧（例如 0.6″ 与 1.5″/px）检查是否被拒；这是能判红的检查 |
| 4 | **Mosaic 不得拒绝混阶输入** | 见第 3 行的 coverage 粗化保护；`coverage.cpp:184` `p2_coverage_build` | `src/workflows/mosaic.cpp:129`–`:137`：order 不一致仍**硬失败** “the inputs do not share the leaf order” | 因为上游计划不跨调用（第 3 行），mosaic 仍可能因两次独立决策而拒绝；没有“先给出统一建议 order”或“读取计划并验证”的路径。硬拒绝避免了 main 的静默粗化，但未与统一计划联动 | `NOT_RUN`（没有构造混阶输入；E4/E5 两个输入同为 18，未被拒） | 打开输入时读计划身份；不一致时报 `order_mismatch` 并给出由 `min required_order` 算出的统一建议 | 独立构造 order17/18 混合产品，要求失败可判红且诊断可行动 |
| 5 | **像元→真球面 Drizzle**：pixfrac/面积/支持/守恒 | `drizzle_engine.cpp`（stripe 归约 + touched tile + 每线程几何 LRU）；旧公开 API 的**四角近似**见 `spherical_overlap.cpp:470` 注释与 `:511`/`:560` `corners[4]` | `src/geometry/drizzle_core.hpp:15`–`:18` 定义 `w=a/A, B=F/N, R=w/N`；`drizzle_core.cpp:71`–`:99` 面积区间→权重、`:56`–`:67` 叶立体角上界；pixfrac 收缩 `src/geometry/overlap.cpp:155`–`:166`；真曲边交叠在 `src/geometry/{overlap,refine,containment,exact_fraction}.cpp` | 数学更严格（真曲边/区间），但 **main 的性能核没有迁**：`src/geometry/` 里没有 stripe 唯一作者/固定归约、没有 `run_generation` 几何 LRU、没有 tile 并行（`grep "stripe|LRU|atomic|thread_local" src/geometry` 只命中 `profile.cpp` 的 thread_local 计数与 `drizzle_core.hpp` 注释）。`std::map` 分块累加是单线程结构 | 已测(范围)：E2 256² order18（`leaves=92880/109716`, `terms=521810/558797`，`run/issue126/staging/m5_256_0_0/dbg.txt`）；E4 1024² order18（`leaves=1484951/1733150`, `terms=8351003/9080010`，两面板 stderr）。**无**总通量/FWHM/MTF 指标 | 把 main 的 stripe 归约/touched tile/几何 LRU **语义**接进现有 accumulator（[02](migration/02-GEOMETRY.md) G-03、[05](migration/05-RUNTIME.md) R-03），并按 06 做 1/2/4/8 线程等价 | 独立真值（解析 Gaussian/脉冲场）在 order18 下量总通量、FWHM/椭圆率、MTF、背景低频；不接受仅 `ΣRx=signal` |
| 6 | **Drizzle 输出的不确定度** | `noise_snr/cpp/src/{sparse_reference,information_weight,noise_model}.cpp`、`integration/normalize_science/`（球面 reference completion） | `src/information/*`、`src/uncertainty/*`；产品侧对无根模型降级（E2/E4 的 `product.collect_signal` issue 原文） | 目前所有 order18 产品都是 `signal_only`、`noise=unknown`，没有 R 的噪声作用/共享根。旧的 Fisher→scene misuse 已不复用，但新的可发布 covariance 也**没有产出** | 已测(范围)：E2/E4 assessment `noise=unknown`，reason “the covariance model has no roots”；没有 SNR/N_eff 数字 | 发布 R 的噪声作用（共享根 + per-leaf ivar），或在 issue 中把 `signal_only` 明确标成未满足 §2 的 uncertainty 要求 | 独立核对双帧/多帧共享根（重复帧不得当独立）、`P C Pᵀ`、PSD 与零空间 |
| 7 | **PSF/点源通量/延展结构保真** | `psf/src/{pixel_integrated_psf,spatial_psf}.cpp`、`star_detection/src/*`、`p1_op_star_psf_impl` | `src/sources/psf*.cpp`、`src/geometry/drizzle*`；PSF **不进入** Drizzle 核（无 PSF 匹配/卷积步骤） | V2 的 R 只做面积加权（pixfrac=1 时即像元面积平均），没有把 PSF 差带入共加；没有 PSF/MTF 诊断。order18 产品没有任何质量度量；旧 order13 seam/低频数字不能搬用（`docs/audit/m42-s1-recovery-2026-11-12.md:376` 是 order13 的三帧记录） | `NOT_RUN`（order18 无 PSF/flux/MTF/seam 证据） | 按 [07](migration/07-VERIFICATION.md) V-03 增加独立 PSF/MTF/flux 探针；把 PSF 匹配作为 mosaic 的显式可选步骤并记录 | 独立 reviewer 用真实 M42 未参与 fit 的 Gaia 星量 FWHM/椭圆率/通量，与输入帧原生值对照 |
| 8 | **main P2 全流程**：coverage/sample/sky/UPM/reject/integrate/write → V2 单 Process | `coverage.cpp:184 p2_coverage_build`；`sampler.cpp:1738 p2_sample_controls_cached`；`sky_plane.cpp:3966 p2_sky_plane_build_joint`；`rejection.cpp:2103 p2_reject_stack_ex`；`integrate.cpp:20 p2_integrate_pixel`；适配器 `module_adapters.cpp:11101/11444/11813/12815/13723` | `src/workflows/mosaic.cpp:371`–`:425`（sky.controls→fit→apply）、`:501`–`:532`（select→weights→coadd）；实现分布 `src/sky/*`、`src/coadd/*` | (a) **没有 Coverage Process/MOC/footprint 并集**：`mosaic.cpp:102`/`:148` 读了输入已发布叶的并集但**从未使用**（只有 insert，没有任何读取），共加由 FramesProvider 按帧提供，没有面板邻接图、面积交集或空天区语义；(b) 整帧读入（`read_leaf_block` `:152`），没有 main 的动态 microchunk/有界队列；(c) 唯一跨面板跑批的 sky 基是常量 0 阶（E5），没有 stencil/连通分量实测；(d) 唯一跑批排异是 `dq_only`（E5）；V2 `RejectionPolicy::Kind` 只有 `{dq_only, single_max_loo}`（`include/acsd/model.hpp:417`），而 main 有 9 种方法（`rejection.cpp:987`–`:997`） | `NOT_VERIFIED`（E5 mosaic 未产出；E2 无 mosaic） | 加真实 footprint/MOC 的 coverage 计算与面板邻接；mosaic 改为按 tile 流式；按 [03](migration/03-MOSAIC.md) M-04 迁排异方法并显式声明小 N 策略 | 独立用 healpy/MOC 复算两真实相交面板的并集面积/叶数；注入坏点/卫星测 TPR/FPR |
| 9 | **全局 sky gauge**与夜间乘性标度 | `sky_plane.cpp:3966`（联合正规方程、Schur、IRLS、连通分量 gauge） | `src/sky/sky.cpp:14`–`:15`（每连通分量一个 gauge）；`mosaic.cpp:203`–`:290` 先用星等样本施加夜间乘性标度；`:371`–`:425` 再加性 sky fit/apply；`:541`+ 存 per-leaf offset；`:788`+ signed seam 诊断 | 夜间标度来自**未类型化 sidecar**：`mosaic.cpp:227` 直接拼 `photometry.json` 路径，缺文件静默 1.0（`:231`），不是产品闭包里的 PhotoSolution；`normalize.cpp:1417` 写该文件时无 schema 校验、manifest 不登记。先标度后加性的顺序在源码注释（`mosaic.cpp:203`–`:220`）里被明确写成对 S1 复审判定的修复；该顺序是否已足够正确仍须独立复核 | 已测(范围)：E4 产品 `relative_scales=null`（未请求 photometry）；E5 mosaic 未完成 → gauge 实测 `NOT_VERIFIED`；E2/E4 assessment 都 `science=conditional` | 把逐帧星等样本/scale 作为产品对象发布，mosaic 从 ProductProvider 读；缺失不得默认 1，应具名 `unavailable` | 注入已知 1.5× 乘性差，检查 mosaic 是否恢复为 1 并给出 p16/p84；检查断连分量不被强行拉平 |
| 10 | **Gaia/WCS**：留出星、夜间、强星云背景 | `platesolve/cpp/ipv/src/ipv_{solver,ransac,sip}.cpp`；生产 `module_adapters.cpp:4911 p1_op_wcs`；#92 AP/BP 负例 | `src/sources/astrometry_*.cpp`、`src/coordinates/wcs.cpp`、`tan_sip.cpp`、`wcs_process.cpp` | issue126 的 WCS **全部来自外部 Python 工具**（`run/issue126/tools/fit_poly_wcs.py` 头注释：independent Gaia TAN+SIP），以 `supplied_wcs` 注入（E4 scientific 的 `frame_options`）；V2 自己的 `sources.astrometry` 在这些产物里**没有被调用**。WCS 解码契约有独立证据，但盲解/留出路径没有 | 已测(范围)：M2 外部拟合 36,121 检测 / 523 Gaia in-frame / 116 匹配 / 85 训练+31 留出 / train 0.174″、holdout 0.204″（`run/issue126/astrometry/fit_20251212_M2.json`）；独立 M42 报告 holdout 0.3176 px=0.307″（`validation/m42/review/independent/REPORT.md:20`）；WCS JSON 对 astropy 1e-10″、逆闭合 2.5e-7 px（`validation/m42/astrometry/verify_20251212.json`） | 用 V2 `sources.astrometry` 重跑同一帧并与独立解对拍；把留出统计写进产品；对无 supplied WCS 的 30 帧执行真实盲解 | 独立用 astropy/WCSLIB 对拍 V2 解（不是同核 roundtrip）；检查 #92 型“先迭代后门限”的反例 |
| 11 | **测光/响应/校准**：不同相机与滤镜 | `photometry/cpp/src/{spectrum_integrator,frame_photometry_fit,spatial_gain,image_corrector}.cpp`；`module_adapters.cpp:5631 p1_op_photometry`；旧 Antlia/QE 错配反例 | `src/response/*`、`src/photometry/*`、`src/observation/calibration*` | issue126 **所有** scientific 都没有 `photometry` 请求（E2/E4），`allow_relative_uncalibrated=true` → 逐帧乘性标度未拟合、未施加；`response_ha.json` 是外部响应文件；校准只有 T2 `cal/`；没有 T3/R/G/B。mosaic 默认要求 response+kernel 身份相同（`mosaic.cpp:335`–`:365`），不同相机/滤镜只能靠手写 group 声明（E5 的 `assumption_kind=same_filter_relative_response_shape_different_kernel_conditional`） | 已测(范围但不是本次)：`validation/m42/science/metrics.json` 两夜 52 星，19.2% 星通量为负，SNR 中位数 0.75，零点 2.73e20±4.0e20（文件内无构建 SHA，只作量级参考，不作本次判决） | 对同批打开 photometry 请求并用 shared-Gaia latent 拟合；跨相机/滤镜要么给出响应+核匹配证据，要么按通道分组产品 | 独立用 Gaia XP 前向积分与留出星检查 scale/零点；不得把历史 0.045 mag 当本次结果 |
| 12 | **HiPS tile width/tile order vs leaf order**、多分辨率 | `aio_hips.h:104`–`:105`（tile 父单元 `K = log2(nside) − 9`，`leaf_order = log2(nside)`）；`hips_core.c:143`/`:255`（`nside = tile_width × 2^K`，K=`hips_order`）；`hips_properties.cpp:150` 强制 `hips_tile_width=512` | `src/workflows/fits_writer.cpp:491`–`:506`（`tile_log2_width` 6..9，`tile_order = order − log2w`）；`:602` 路径写 `Norder<leaf order>`；`:701`–`:707` `hips_order=<leaf order>`；`src/workflows/product.cpp:1437`（默认 9，可配） | 证据产物用 `tile_log2_width=6`（64×64）且写 `hips_order=18`。按 main/IVOA 语义 `hips_order` 是 **tile order**、`nside = tile_width × 2^order`，标准读取器会得到 64·2^18 而不是真实 2^18——路径/属性把 leaf order 当成了 tile order；64×64 也不是标准 512；只有单一 Norder，**没有多分辨率派生** | 已测(范围)：E2 `properties` 原文 + 64×64 tile 头（`signal/Norder18/Dir8780/Npix87808423.fits`，CD=1.7166e-4 deg=0.618″）；39 个 tile（256²）与 417/419 个 tile（1024²）。外部 HiPS 读取器 `NOT_VERIFIED` | 用 `tile order = leaf order − 9` + 512 tile + `hips_order=tile_order`，或在 properties 显式标非标准并同步修正读取代码；需要低阶层时按真实下采样派生 | 独立用 healpy/hips 客户端按 properties 解析 tile 地址与像素中心，抽查 tile 内像元的天空位置 |
| 13 | **Export 真实分辨率**与采样核 | `projection/p3_{projection,wcs}.cpp`、`resample/p3_{resample,rsmp_operator,rsmp_units}.cpp`、`p3_order_select`；旧文档承认“精确 area overlap 未全面接入 P3 生产”（[04](migration/04-EXPORT.md) E-01.3） | `src/projection/export_projection.cpp:382`（目标像素网格×真实 overlap，pixfrac=1）、`:231`–`:330` 系数/支持、`:751` 覆盖记录；`src/projection/export_process.cpp:209`–`:261` 计划 | (a) 目标 WCS 完全由调用者给（`export.scientific.json`），不按参与帧足迹规划；(b) 没有 `p3_order_select` 等价，也**没有“目标像元比 leaf 细”的拒绝**；当前工作树只在 Normalize 产品上加 `order_coarser_than_native_scale`，导出产品不继承（`export_workflow.cpp:165`–`:172`），而 E3 的 order13 产物产生于该改动前，导出评估与 order18 产品同形；(c) 只有面积核，没有 nearest/bilinear 对照，无 PSF/MTF、无噪声相关 | 已测(范围)：E2/E3 同 ROI 两个 order 的导出（117 vs 92880 leaves，均 65536 像元）；独立面积 oracle：`docs/audit/b5-export-review.md` §8（常场 rel 1.76e-11；2×2 旋转 CD 2.56e-7/2.45e-7/3.57e-8）。`NOT_VERIFIED`：E2/E3 的 FITS 无外部 astropy 复核 | build_export 内算源 leaf 尺度与目标尺度，目标更细即具名降级 `upsampled_beyond_source` 并写进评估；目标 WCS 由足迹派生或至少校验覆盖；加 nearest/bilinear 对照探针 | 独立 astropy 校验导出 FITS 的 WCS/形状/BUNIT；用已知 PSF 注入场景量 MTF；不得只看 shape 或内部恒等式 |
| 14 | **CLI/GUI 可达性、内存/IO/Task、只读源、缓存、resume、FITS 发布** | `lib/infrastructure/cli/commands.cpp` 三命令 `--json`+JSONL；GUI `infrastructure/hips_browser`（Qt） | `apps/cli/commands.cpp`（三命令/事件/exit code）；`apps/` 只有 `cli` 与 `contract` → **无 GUI**。资源：`src/workflows/normalize_runtime.hpp:343`–`:352` 与 `src/workflows/normalize.cpp` 的 `PoolConfig` 固定声明 `host_bytes=1 GiB`、`cpu_tokens=2`、`max_work=4e9`，但 `rt::make_services` 是**进程唯一账本**（`src/runtime/pool.cpp:1453`–`:1467`：第二次请求返回 CLI 已按用户 `Limits` 建立的同一 Services），因此 1 GiB 声明**不会**生效，真实预算仍是 CLI 的 12 GiB（S1 全幅 plan 的 `f_bytes=1.14 GiB > 1 GiB` 被 admit 即为证）。CLI 在 `apps/cli/commands.cpp:1451` 建 runtime 的语义因此是权威的；但固定声明本身仍是误导性的代码注释，且 CLI 的 `threads/work` 与 workflow 内部 pool 参数不一致，应清理。I/O：`drizzle_grid.cpp:80`–`:99` 每次调用读整幅 native+整幅 DQ；`product.cpp:1449` 发布时读整幅 signal | 双 runtime/双账本：配置的 `threads=4`/`bytes=12 GiB` 不作用于真正跑科学的 host；无跨调用的产品缓存复用与 resume（`grep -rn -e reuse -e resume src/workflows/` 无命中；唯一的缓存是产品重开时的进程内 memo `src/workflows/product.cpp:485`/`:614`），只有输入 `expected_hash` 校验；发布非流式；E2–E5 配置 `durable:false` → 无 fsync 语义；source 只读未见强制 | 已测(范围)：E2–E5 的 stdout/exit（0 与 4 都有）。**性能 `NOT_RUN`**：`run/issue126` 无任何 wall/RSS/IO 记录 | 让 workflow 使用 CLI 已建立的限额（或把限额传给 `open_workflow_runtime`）；drizzle/发布改窗口流式；加产品级 hash 缓存与 resume 记录；证据配置用 `durable:true` 并做 new-process reopen | 独立性能 reviewer 按 [06](migration/06-PERFORMANCE.md) 记录 1/2/4/8 线程、RSS、IO、缓存命中，并在 12 GiB 上限下复跑；独立做 manifest/objects 的 new-process reopen |

## 3. 当前证据 / Current evidence

**时间基准**：2026-11-14 约 01:00–01:10 CST，`rebuild/acsd-v2` 工作树（HEAD `d7e5b0ba4` + 上述未提交改动）。所有产物都在 `run/issue126/`（未跟踪）；`testdata/`、`gaia/` 为只读源。

### E1 自动选阶 probe（`run/issue126/preflight/f0/`）

`normalize.scientific.json` 的 `request.plan_only=true`、`drizzle.order="auto"`、`frame_options[0].supplied_wcs` 为 M5 T2 的认证相机模型；`out/drizzle_plan.json` 原文要点：

```
schema=acsd.drizzle_plan/1  source=auto-preflight  mode=auto
common_leaf_order=18  nside=262144  leaf_arcsec=0.8051921844482421  clamped=false
frames[0]: frame=M42_M5_T2_flying_dutchman-20251212@041842-600S-H-alpha
           probed=true finest_arcsec=0.9404774497439962 required_order=18 required_nside=262144
```

这条只证明 **CLI 预检入口存在且写出决策**；它没有处理任何帧，也不能证明该 order 下的产品。

### E2 256×256、order-18 已发布 ROI（`run/issue126/staging/m5_256_0_0/`）

- 输入：`frame.fits` 为真实 M42 M5 T2 20251228 051230 Hα 的 256×256 裁剪（`run/issue126/tools/make_crop_run126.py`，不复制原始 FITS）；`cal/` 有 bias/dark/flat；`catalogue.json` 为冻结 Gaia 目录；WCS 由外部独立解提供（第 10 行）。
- Normalize：`drizzle.order="auto"`，`out/drizzle_plan.json` 写 `common_leaf_order=18`、`frames[0].required_order=18`、`finest=0.9409237736725373`；`dbg.txt`：`drizzle_grid: order=18 pixels=65536 leaves=92880/109716 terms=521810/558797 leaf_ratio=1.176`。
- 发布：`out/M42_.../signal/Norder18/Dir8780/Npix*.fits`，共 **39 个 64×64 tile**，`properties` 写 `hips_order=18`、`hips_tile_width=64`、`hips_unit=ADU/SR`。
- Export：`export/signal.fits` 为 **256×256 float64**，TAN-SIP，`CD≈0.9666″/px`（相对 order18 leaf 0.8052″ 是 1.2× 下采样，不是伪分辨率），`BUNIT=ADU/SR`，`ACSUNIT` 保留 native 身份。评估原文：`science=conditional`、`validity=approximate`、`noise=unknown`、`included=["the target covers 65536 of 65536 pixels and touches 92880 leaves","support_approximation"]`、`saturated_pixel_count=8`、`normalizer_lower_bound_is_zero`。
- **没有**同输入外部 FITS 读回、PSF/MTF/通量或 SNR 数字。

### E3 同 ROI、order-13 对照（`run/issue126/staging/m5_256_0_0_o13/`）

同一输入、同一 256×256 目标网格，`drizzle.order=13`（显式整数）。`out/drizzle_plan.json`：`common_leaf_order=13`、`nside=8192`、`leaf_arcsec=25.76614990234375`、`high_resolution=false`、`coarser_than_native=true`、`frames[0].required_order=18`。产品发布在 `Norder13/Dir8/Npix85750.fits`（export 评估记 117 个 leaf）；导出 FITS 与 E2 **同形**（256×256、0.967″/px、同样的 issue 代码）。该产物**产生于**当前工作树加入 `order_coarser_than_native_scale` 评估之前，因此 Normalize 与 Export 两层都**没有**“粗于原生”的标记；当前代码只在 Normalize 产品上加该 issue，导出仍不继承。这是仓库里可复核的“固定低阶→细网格”对照：117 leaves 对 92880 leaves，同一目标网格。

### E4 跨面板 M2+M5 的 1024×1024 裁剪（`run/issue126/cross_panel/`）

- 预检：`plan/plan/drizzle_plan.json` 两帧 `M42_M2...021829`（finest 0.9529103487646423）与 `M42_M5...041842`（finest 0.9533040867211847），`common_leaf_order=18`、`leaf_arcsec=0.8051921844482421`、`clamped=false`、`source=auto-preflight`。
- Normalize：**M2_1024 与 M5_1024（探测器 0,0 裁剪）失败**，exit 4、`not_identifiable`，原文 “the design matrix is rank deficient; the singular values are … 0.000000 0.000000”（40 个参数、2 个零）——来自 `src/sources/psf_fit.cpp:567` 的 PSF 设计矩阵秩检查；两目录只有 `out/drizzle_plan.json`，无产品。
- **M2_north_1024 与 M5_north_1024 完成（exit 0）**：order18；`drizzle_grid` stderr 分别 `leaves=1484951/1733150 terms=8351003/9080010` 与 `leaves=1485677/1734052 terms=8350007/9083002`；`signal/Norder18/Dir9340/...` 分别 **417 / 419 个 tile**；`frame_science.json`：`science=conditional`、`validity=certified`、`complete=false`、`noise=unknown`、`relative_scales=null`，issue 仍是 `support_unresolved(normalizer_lower_bound_is_zero)`、饱和（M2：2 px/15 terms）、`signal_only`。
- `M5_scan_0` 也完成（419 tile，同一帧不同裁剪）；`M5_scan_512`、`M5_scan_1024` 只有计划或空目录 → 未完成。

### E5 跨面板 Mosaic（`run/issue126/cross_panel/mosaic_run/`、`mosaic_run512/`）——真实失败，不是未跑到

- 1024² 对（M2/M5，leaves=1,484,951/1,485,677）：mosaic 单线程持续计算 37 min CPU、无产品，被 2400 s 超时杀死；按 S1 的 23.8K 叶 15 s 线性外推，3.0M 叶需 ~30–40 min，属性能问题而非死锁，但**没有产品**。
- 512² 对（leaves=370,094/370,265，真实二维重叠）：mosaic 在 1 min 内**确定性失败**，给出可复现反例：
  1. `apply_sky`：`the block selection cannot be built`（`src/sky/sky.cpp:2934`），调试输出 `joint_rows=740359 count=370094 row_offset=0 reason=the CSR offsets cannot be stored`；根因是 `plan_sky_apply`（`src/sky/sky_fit_process.cpp:452`）只声明 per-cohort scratch，而实现一次性构建整个信号的四个 CSR 载荷（~`count*96` B），progress arena 在 370K 叶耗尽。
  2. 修复该声明后，下一处同类失败是 coadd/select 的 `the leaf identities cannot be stored`——整信号载荷低声明在下一个 typed process 重复。
- **裁决**：跨面板 Mosaic 是 **FAIL（带反例）**，不是资源不足或"未跑到"；S1 三帧能过是因为三帧叶集合几乎相同、`apply_sky` 的"每帧是联合叶空间连续块"假设成立。修复按 `elements` 声明整信号 CSR/索引，或改为按 tile 流式；修好后才能谈接缝审核。

### E5bis 同 ROI order18 vs order13 细节对照（实施者自查）

`run/issue126/staging/m5_256_0_0{,_o13}` 同一 256² 输入、同一目标 WCS（0.9666″/px）：
二阶差分恰为 0 的比例 order18 **0.000** / order13 **0.794** / 原始 **0.015**；高通与输入帧相关 order18 **0.188** / order13 **−0.0003**；高频功率占比 order18 **0.608** / order13 **0.108** / 原始 **0.643**；内部像元比 18/13 中位 **0.995**（0.913..1.076）。这是固定 order13 把 25.8″ 叶放大到 0.967″ 像元的可量化指纹，也是"细节是否保住"的第一份同输入证据；它不是独立 PSF/MTF oracle，也没有全幅/两面板数据。

`mosaic.config.json` 输入为 E4 的 M2_north/M5_north 两个产品（带 `expected_hash`），`mosaic.scientific.json` 声明：一组两帧、`response_proof`+`spatial_kernel_proof`、`assumption_kind=same_filter_relative_response_shape_different_kernel_conditional`、sky `basis.kind=constant, degree=0`、`gauge.kind=reference_frame`、`rejection.kind=dq_only`、`weights=automatic`、`quantity=brightness`。

截至 **2026-11-14 01:10 CST**：`mosaic_stdout.jsonl` 只有 `run_started` 与 `frame_started` 两个事件（run_id `run-03ea9fa7bef899e8`），`mosaic_stderr.txt` 为空，**没有 mosaic 产品目录，没有 export**。因此跨面板 mosaic 的 `NOT_VERIFIED` 是事实状态；本文不能、也没有把它写成完成。该目录可能在本文写成之后继续变化——任何引用本节的人必须先用 `ls run/issue126/cross_panel/mosaic_run/` 与 `tail run/issue126/cross_panel/mosaic_run/mosaic_stdout.jsonl` 复核再改状态。

### E6 明确未运行（不得在 issue 回复里含糊其辞）

- **4096×4096 全帧**：`run/issue126` 下没有任何 4096² 产品（最大 1024×1024 裁剪）。`NOT_RUN`。
- **跨面板 mosaic/export 的完成态**：E5 仍在运行。`NOT_VERIFIED`。
- **多面板（≥2 个不同面板）mosaic 质量**：无接缝/低频/PSF 数字。`NOT_RUN`。
- **T3 / R / G / B / 不同相机或滤镜**：`NOT_RUN`（全部证据是 T2 + Hα）。
- **PSF/MTF/点源通量/延展结构保真（order18）**：`NOT_RUN`。
- **不确定度/SNR/N_eff/covariance**：全部产品 `noise=unknown`。`NOT_RUN`。
- **性能（wall/CPU/RSS/IO/cache）**：`run/issue126` 无记录，`BASELINE_PENDING`。
- **Windows/MSVC、GUI、外部 HiPS 客户端、new-process reopen**：`NOT_RUN`。

### E7 本批证据里可直接核对的产品级事实（供复审引用，不作 PASS）

- 证据跑批的发布配置全部 `durable:false`（E2 `export.config.json`、E4 各 `normalize.config.json`、E5 `mosaic.config.json`）。
- 证据跑批的 HiPS tile 配置全部 `tile_log2_width=6`（64×64），`hips_order` 写 leaf order（第 12 行）。
- 证据跑批的排异是 `dq_only`（E5）；没有统计排异被实际执行。
- 夜间标度的唯一通道是每帧输出目录里的 `photometry.json`（E2/E4 各产品都有），mosaic 直接按文件系统路径读（`src/workflows/mosaic.cpp:227`）。
- 所有完成产品的评估都是 `science=conditional`；没有一个达到“无条件”或“高分辨率正式科学产品”的声明。o13 的 Normalize 产品与导出产品在修复后明确带 `domain/order_coarser_than_native_scale`（E3）。

## 4. 与 main 对照时的三条硬边界

1. **旧错误不作为回归基准**：main 的四角近似（`spherical_overlap.cpp:470`）、把 65535 当信号积分、Fisher 权当 scene IVAR、固定 order13 都已在本分支被判为缺陷；V2 更严格的数学是**要求**而不是退化。但这些修正必须由独立 oracle 证明，不能由“新代码更严格”这句话代替。
2. **性能只在同科学策略下比较**：[06](migration/06-PERFORMANCE.md) P-00 要求同 WCS/order/pixfrac/响应/排异/目标 WCS；目前没有任何 main↔V2 的同输入性能表，故本矩阵所有性能格都是 `NOT_RUN`。用粗 order 或减少帧数换速度属于 P-02 红线，不是“优化”。
3. **产品闭包 ≠ 成品**：E2/E4 的产品闭包可重开、评估自洽，但 `noise=unknown`、`science=conditional`、无质量度量；[MIGRATION](MIGRATION.md) §4 的 “完整成品 READY_FOR_OWNER_REVIEW” 要求高阶+两面板+196 帧+接缝/PSF/SNR/外部 FITS，这些在本文件里逐项记为未运行。

## 5. 结论性观察（只写源码与产物能支撑的）

- **真正打通的是“自动选阶的决策与证据链”和“order18 的单帧/裁剪 ROI 发布”**：CLI 预检、Normalize 计划、order18 稀疏累计、256²/1024² 裁剪产品与导出都存在，并且 order13 对照被明确标成 `coarser_than_native`（E1–E4）。
- **真正没打通的是四件事**：① 计划已有读取入口（`order:"plan"`）但没有被任何真实跑批消费，跨面板的“同一 order”仍是两次独立决策恰好相同（第 3 行，`NOT_VERIFIED`）；② mosaic 仍拒绝混阶，且没有 coverage/MOC、没有分块流式、排异只剩 `dq_only`（第 4/8 行）；③ order18 产品没有不确定度与 PSF/通量质量证据（第 6/7 行）；④ HiPS 的 `hips_order`/tile width 与 main/IVOA 语义不一致，且导出不检查“目标比源细”（第 12/13 行）。
- **资源与发布**：`make_services` 的进程唯一账本使 workflow 的 1 GiB 声明实际不生效，真实预算仍是 CLI 按用户 `Limits` 建立的 12 GiB（`src/runtime/pool.cpp:1453`–`:1467`；S1 全幅 `f_bytes=1.14 GiB > 1 GiB` 被 admit 为证）——但该固定声明与 CLI 的 threads/work 参数不一致，属代码卫生与可读性缺陷，须清理；发布仍是非流式且本批 `durable:false`（第 14 行）。在 4096²/196 帧之前，这些必须先按 [05](migration/05-RUNTIME.md) R-01/R-04 和 [06](migration/06-PERFORMANCE.md) P-03 修正并实测。
