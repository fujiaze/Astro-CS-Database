# Phase2 W0 — 当前 main 盘点（2026-08-10）

控制包：`AstroCS_Phase2_Implementation_Control_Package_V1.zip`
SHA256：`34A532A2451C8746BEF7B5DA05C3C4C7D15201D66A9D5F6AB5F8F291BE2EB308`

## 仓库状态

- 分支：main；HEAD：e862403（= origin/main）
- 工作区：仅未跟踪交付包（Phase1 归档包），无代码改动
- ACR：已并入 main（merge 198d69e，HEAD e949b30，131 提交）；无业务调用

## 模块与接口（已核实存在）

| 模块 | 路径 | 关键接口 |
| --- | --- | --- |
| ACR | lib/acr/ | include/astro/compute/acr.hpp、scheduler/dispatcher.hpp、routing/benchmark_route_estimator.hpp、examples/weighted_integration/ |
| AIO | lib/astro_image_io/ | include/aio_hips.h（writer）、include/aio_hips_reader.h（reader）、include/aio_healpix_io.h（shared HEALPix） |
| orchestrator | lib/orchestrator/cpp/ | include/orchestrator.h、json_config.h、configs/stage1.schema.json（v1.1） |
| photometric_calib | lib/photometric_calib/cpp/ | include/photometric_calib.h（PhotometricDiag、PcMatchRecord、pc_* C API）、src/spectrum_integrator.h、src/star_matcher.h |
| snr_estimator | lib/snr_estimator/cpp/ | include/snr_estimator.h |
| healpix_stack | lib/healpix_db/healpix_stack/ | hp_stack_api.h（历史 Stage2/HICS 代码，控制包声明"旧 HICS 代码标记 deprecated，不删除、新实现不得依赖"） |
| Wiki | AstroCS.wiki/ | Home.md、Project_Status.md 当前仍写"Phase2=HICS"（需按 W1 更新） |

## 控制包正式定义（覆盖旧 HICS 描述）

- 输入：N 个 Phase1 单帧 HiPS（signal/support/snr）
- coverage union Ω = MOC 并集；控制点布置于整个 Ω
- 一个 UnifiedPhotometricModel（UPM），全局联合求解（Huber IRLS、SNR-aware 权重、图平滑、弱零锚、连通分量）
- 动态分块：内存估算 + 块增长 + 真实 N_B + OOM 减块（不 swap）
- 每样本经 `upm_calibrate_block(model, frame_id, pos, in, out)` 校准
- 迭代排异（7 种：None/Sigma/Winsorized/AveragedSigma/LinearFit/ESD/RCR，CPU reference 优先 + Oracle）
- SNR/support/quality 加权叠加
- 输出：标准 IVOA HiPS `Mosaic/{signal,support,weight?,rejection_count?}/`（唯一 AIO）

## 撤销项

- HICS 动态数据库
- 每帧独立梯度 Gi
- 只在帧间重叠区建控制点
- Phase2 乘性 photometric scale
- 新 runtime I/O DLL

## 后续

W1 Wiki 同步 → W2 接口/schema 冻结 → W3 输入/coverage → W4 UPM → ... → W12 交付