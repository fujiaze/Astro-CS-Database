# Normalize 真实生产调用链盘点（第二轮）

分支 `audit/acsd-science-docs-p5`；盘点基线 `4f7ecb6b8`。本文只记录**当前真实存在的代码事实**，
按 `docs/engineering/architecture/NORMALIZE_PIPELINE.md` §1–§3 的口径逐项对照；不引入新设计。

## 1. 唯一用户入口与调度面

- 入口：`lib/infrastructure/cli/commands.cpp` `run_phase1_block` → `acsd::cli::build_pipeline_ir`
  → scheduler `P1NodeModule`（`module_adapters.cpp:14730` 起）。
- IR 静态图：`lib/infrastructure/cli/runtime_client.cpp` `phase1_nodes()`（`mk(...)` 工厂）。
- 生产 C++ 实现全部在 `lib/infrastructure/scheduler/src/module_adapters.cpp` 的
  `p1_op_*` 函数；算法本体在各自 `lib/algorithms/*` 库。

## 2. 八节点：模块归属、输入输出对象、typed-port 依赖

| 节点 (IR id / operation / entry) | 生产实现 | 算法库 | 真实取数（当前） | descriptor 端口（module_adapters.cpp） | IR typed 边（runtime_client.cpp） |
|---|---|---|---|---|---|
| `cal` / `calibrate` / `acsd_phase1_calibrate_v1` | `p1_op_calibrate` (:2623) | `acsd_phase1_calibration` | `input_lights` + `master_bias/dark/flat` → `calibrated_<base>.fts` | 入 `calibrated`(DATA-P1-CAL, ADU) / 出 `calibrated` | 出 `calibrated→artifact:cal` |
| `cos` / `cosmetic_correct` / `acsd_phase1_cosmetic_v1` | `p1_op_cosmetic` (:3099) | `acsd_p1_cosmetic` | `calibrated_*` + 坏点源 → `cleaned_<base>.fts` + `badcol*` | 入 `calibrated` / 出 `cleaned`(DATA-P1-COSMETIC) | 入 `cos←artifact:cal`，出 `cleaned→artifact:cos` |
| `wcs` / `plate_solve` / `acsd_phase1_wcs_v1` | `p1_op_wcs` (:4889) | `lib/algorithms/platesolve/cpp/ipv`（`ipv_solve_from_memory_with_callback_d`）+ `sdet` | 读 `calibrated_*` 自行盲检 + Gaia + hint → `<frame>/p1_wcs.json` | 入 `calibrated` / 出 `wcs`(DATA-P1-WCS) | 入 `wcs←artifact:cal`，出 `wcs→artifact:p1_wcs` |
| `psf` / `detect_sources` / `acsd_phase1_starpsf_v1` | `p1_op_star_psf_impl` (:3951)，dispatch `p1_op_star_psf` (:4554) | `star_detection` + `psf`（`dpsf_psf.cpp`） | `cleaned_*` + 已解 `p1_wcs.json`（含 SIP）→ `p1_sources.json`（含 `psf_params[]`）+ `p1_psf.json` | 入 `cleaned`、`wcs` / 出 `sources`、`psf` | 入 `psf←{cos,p1_wcs}`；出 `sources→artifact:p1_sources`、`psf→artifact:p1_psf`（后者无消费者，见 §3） |
| `phot` / `measure_flux` / `acsd_phase1_photometry_v1` | `p1_op_photometry` (:5609) | `photometry`（`frame_photometry_fit`→`star_matcher`→`pc_calibrate_simple_*`） | `calibrated_*` + `p1_sources.json` + `p1_wcs.json` + 逐 occurrence `frame_overrides`（F07）→ `photoapplied_<base>.fts` + `p1_phot.json` | 入 `calibrated`、`sources`、`wcs` / 出 `fluxes`、`photprov`(DATA-P1-PHOTPROV-001) | 入 `phot←{cal,p1_sources,p1_wcs}`；出 `fluxes→artifact:p1_flux`、`photprov→artifact:p1_phot` |
| `snr` / noise-snr | `p1_op_noise` (:7500) | `noise_snr`（`snr_noise_model_v1`、`snr_frame_science`） | `cleaned_*` + `p1_sources.json` + `p1_phot.json` → `p1_snr.json` + `noise_model` / variance 面 | 入 `sources`、`photprov`、`cleaned`、`fluxes`(required=true) / 出 `snr` | 入 `snr←{p1_sources,p1_phot,cos}`；出 `snr→artifact:p1_snr` |
| `drz` / drizzle | `p1_op_drizzle` (:8312) | `hp_drizzle_run_phase1_hips`（`drizzle_engine`、`astro_sphere_sink`、`spherical_overlap`）+ **noise 库球面完成**（`normalize_precision.cpp` + `sparse_reference.cpp`） | `photoapplied_*` 或 `cleaned_*`（按 `frame_applied`）+ `p1_wcs.json` + `p1_sources.json` + `p1_snr.json` + variance 面 → 帧 HiPS（signal/support/variance/ivar）+ `frame_science.json` staging + `p1_stack.json` | 入 `calibrated`、`wcs`、`photprov`、`sources`、`snr` / 出 `stacked`(DATA-P1-STACK) | 入 `drz←{cal,p1_wcs,p1_phot,p1_sources,p1_snr}`；出 `stacked→artifact:p1_stack` |
| `wr` / writer | `p1_op_writer` (:9675) | `lib/infrastructure/aio`（atomic publish / manifest） | 帧 HiPS + `frame_science.json` staging + `p1_stack.json` + 模型文件 → 产品面 `frame_science.json`、`models/*`、`p1_final.json` | 入 `stacked`(required=true) / 出 `fits` | 入 `wr←{p1_stack}`；出 `fits→artifact:p1_hips` |

## 3. typed-port 与正本 §4 的差异（本轮盘点结论，未改代码）

`NORMALIZE_DATA §4` 的端口表与实际 descriptor/IR 的偏差，逐条列明：

1. **drz 的像素输入端口名与物理单位不符**：descriptor/IR 都写 `calibrated`(DATA-P1-CAL, ADU)，
   但 `p1_op_drizzle` 实际消费的是 `photoapplied_<base>`（`frame_applied=1`，单位 = applied
   线性单位）或 `cleaned_<base>`。正本 §8 明确「当前 descriptor 的 calibrated 输入不能继续被
   当作 photoapplied 的物理单位」。需要一个**新 data_id**（如 DATA-P1-PHOTOAPPLIED）而不只是改名。
2. **snr 节点的 `fluxes`(DATA-P1-FLUX) 声明为 required=true，但生产 IR 不绑定**：
   descriptor `p1_noise_snr_descriptor` `{"fluxes","DATA-P1-FLUX",true,...}`，而
   `runtime_client.cpp` 注释明言「旧 IR 的 artifact:p1_flux 输入是**幻边**（本节点无
   p1_flux.json 读取点）, 已删」。这是 **descriptor 与 IR 不一致**：required 输入在生产图中
   不存在。要么把该端口降为 required=false 并在注册表标注 synthetic-only，要么让 phot 的
   `fluxes` 真正被消费。
3. **`psf`(DATA-P1-PSF) 端口生产链零消费者**：psf 节点输出，photometry descriptor 注释明确
   「该边在注册表里不存在」。Drizzle/噪声侧真正消费的 PSF 目前是
   `p1_sources.json#frames[].psf_params[].sx/sy`（经 `normalize_precision.cpp`
   `frame_psf_sigma_px`），`p1_psf.json` 无消费者。按正本 §8「PSF 从零消费者工作文件变成
   显式模型输入」尚未闭合。
4. **drz 缺三个正式输出端口**：`drizzle_model`、`sparse_reference`、`frame_reference`
   在正本 §4 有定义，descriptor 只有 `stacked`。球面精度结果目前经
   `frame_science.json` staging + writer 发布，**不经 typed 输出边**。
5. **wr 只声明 `stacked` 一个输入**：正本 §4 要求 identity、wcs、photprov、noise_model、
   stacked、drizzle_model、sparse_reference、frame_reference 八项。当前 writer 从
   output_dir 文件约定读取其余各项。
6. **实际取数仍是 output_dir JSON 热路径**：`p1_wcs.json`、`p1_sources.json`、
   `p1_phot.json`、`p1_snr.json`、`p1_stack.json` 仍被真实消费者按路径读写；typed 边只用于
   **排序**（保证上游先落盘），不搬运载荷。这与正本 §4「data_schema_id 表示载荷，不能只是
   安排节点先后」仍有距离。
7. **`p1_session.cpp` 仍是兼容 facade**（calibration/cosmetic/io_read/io_write），未在产品面
   冒充完整 Normalize；按 N03 处置。

## 4. 保留不动的算法归属

IPV PlateSolve（`lib/algorithms/platesolve/cpp/ipv`）、Gaia XP photometry
（`lib/algorithms/photometry`）、PSF（`lib/algorithms/psf`）、noise_snr（`lib/algorithms/noise_snr`）、
Drizzle（`lib/algorithms/drizzle`）的既有算法全部保留；本盘点未改动任何算法入口。
