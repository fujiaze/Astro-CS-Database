# AstroCS Public / Internal API

## C ABI（`extern "C"`，不跨边界抛 C++ exception）

- `lib/astro_image_io`：`aio_*`（image/HiPS I/O、writer/reader、pipeline）。
- `lib/phase2`：`p2_*`（coverage / sampler / upm / integrate / stage2 入口）。
- `p2_upm_build`（obs-only，兼容）与 `p2_upm_build_geo`（全几何节点，
  V13/V14）。
- `p2_sample_controls` / `p2_sample_controls_cached`（后者性能透传 `frame_id_cache` 避免二次 500MB payload 哈希；同数值语义）。
- V16/V17 rejection 接口（typing 单语义，版本化政策）：
  - `p2_reject_plan_resolve`（planning 层把 auto 解析为显式方法 +
    method-specific typed params；profile=wbpp_2_9_1 冻结版本或
    astrocs_adaptive 独立策略）；
  - `p2_eligibility_filter` / `p2_collect_candidate_stack`（V16 生产 strided
    collector：finite/valid/support/quality → CandidateStack；Stage2 CPU/ACR
    统一入口）；
  - `p2_validate_candidate_weights`（V17：SNR lookup 后统一非 finite/非正
    权重校验，禁止 Stage2 漏检）；
  - `p2_reject_stack_ex`（explicit plan kernel；per-sample reason +
    stack-level status 分离；V17 契约：仅 OK/UNDERDETERMINED 可继续，其余
    INVALID_*/INTERNAL_ERROR 必须 hard fail）；
  - `p2_large_scale_apply`（V17：astrocs.large_scale_rejection.v1，
    per-frame low/high rejection mask 的 connected-component grow）；
  - `p2_integrate_pixel`（V17：唯一 canonical support reducer=max(accepted
    support)；显式状态 OK/NO_CANDIDATES/ALL_REJECTED/ZERO_VALID_WEIGHT/
    INVALID_INPUT；非 finite weight/support 绝不返回 OK）；
  - `p2_reject_stack`（旧签名）为 COMPAT adapter，生产 Stage2 不再调用。
- 返回码：0=OK；非 0 具体语义见各头文件注释；`err` 缓冲只做日志，不承载
  状态机。

## 状态与错误所有权（V14 合同）

- **返回值所有权**：每个 C ABI 函数的返回码由该模块独占定义（各头文件注释
  为唯一权威），调用方只按 0/非 0 与头文件语义分支，禁止解析错误字符串。
- **错误缓冲 `err`**：仅承载人类可读日志文本，不参与状态机；为 `nullptr`
  时函数必须仍能正常执行并返回状态码。缓冲区所有权/容量/生命周期由各头
  文件声明，无隐式全局错误对象。
- **日志与状态分离**：日志写 `run/logs/<module>/`，返回状态只经返回值传递；
  模块内部日志级别不得影响控制流。
- **C ABI 不抛异常**：`extern "C"` 边界全部捕获并转换为返回码；`buffer
  ownership/lifetime/nullable/单位` 在头文件逐参数注释。
- **跨阶段**：Phase1 产物语义错误（非法 WCS/负 flux 等）必须在 Phase2 入口
  以非 0 返回码显式拒绝，禁止静默用默认值替代。

## C++ API

- `astrocs::healpix`（healpix_core：ang2pix/pix2ang/nested_local↔FITS index）。
- `astrocs::crypto`（SHA-256）。
- 命名空间建议：`astrocs::phase1 / phase2 / hips / acr`（不强制破坏现有
  `p2_*` ABI；C++ 层可逐步包装）。

## 工具/CLI

- `astrocs-stage2.exe <config.json>`（**V5 遗留, LEG-004 已退出生产**；当前生产入口 = `astrocs phase2 run` / `astrocs run --phases` preset）。
- `orchestrator.exe <stage1.json>`（**V5 遗留, LEG-002 已退出生产**；当前生产入口 = `astrocs phase1 run` / `astrocs run --phases` preset）。
- `healpix_browser_qt.exe`（HiPS 浏览器；`--hips/--standard-hips/--view/
  --screenshot/--lod/--exit`）。
- `toolchain.ps1 check|build|run|review`（统一工程入口）。

## JSON schema（config）

- stage1: `lib/orchestrator/configs/stage1_*.json`。
- stage2: `lib/phase2/configs/stage2_*.json`（model/integration/output/
  diagnostics 四段；默认值唯一来源见 `CONFIG_SCHEMA.md`）。

## gaia_client C API（API-GAIA-001）

> ID: API-GAIA-001  状态: CONTRACT_READY（CAT-GAIA-DOC 冻结，2026-09-05）
> 头: lib/gaia_xpsd_client/src/gaia_client.h（唯一权威签名源，禁止手抄他版）
> SRC: lib/gaia_xpsd_client/src/gaia_client.c；ALG: ALG-GAIA-001；
> DATA: DATA-GAIA-001。纯 C（无 C++ 边界），`GAIA_EXPORT` 导出。

- 导出符号（12 个，全部当前真实存在）：`gaia_client_create`、
  `gaia_client_create_ex`、`gaia_client_destroy`、`gaia_client_cone_search`、
  `gaia_client_cone_search_for_solver`、`gaia_client_get_db_type`、
  `gaia_client_get_file_count`、`gaia_client_get_total_sources`、
  `gaia_client_cone_search_with_spectrum`、`gaia_client_query_spectrum_by_coords`、
  `gaia_client_cone_search_with_photometry`、`gaia_client_get_spectrum_params`。
- 返回码：搜索/查询族 `0`=成功（含 0 结果）、`-1`=参数错误/分配失败/内部错误；
  `get_spectrum_params` 返回 `1`=有光谱 / `0`=无；`get_db_type` 返回
  GaiaDbType（0/1/2，NULL 句柄返回 0）；`create/create_ex` 失败返回 NULL。
- 所有权：`client` 由 create 分配、destroy 释放；全部 `out_*` 数组由模块
  malloc、调用方 free（用同一 C 运行时 free）；`out_stars=NULL`/`out_count=0`
  表示空结果，不需要 free。
- 线程安全：同一 client 并发查询安全（文件级 OpenMP 并行 + 缓存互斥，
  ALG-GAIA-001 §4）；`destroy` 不得与在途查询并发；不同 client 互相独立。
- 参数有效域：ra∈[0,360)、dec∈[-90,90]、radius≥0、mag_low≤mag_high、参数
  有限（NaN/Inf 输入不显式校验，行为未定义——前置条件，负面测试覆盖）；
  `query_spectrum_by_coords` 的 `match_radius_arcsec` 单位角秒，其余半径均为度。
- 已登记现状缺陷（不得静默使用，CAT-GAIA-IMPL 处理）：
  `GaiaStar.parallax/pmra/pmdec` 输出未初始化；`source_id` 恒 0；空数据目录在
  Windows 返回 NULL 而 POSIX 返回 file_count=0 的空 client（平台差异）；
  单文件结果上限 200000 静默截断；无取消检查点。
- plan/execute/cancel/inspect 迁移语义见 ALG-GAIA-001 §3.1（astrocs.catalog.gaia，
  C ABI adapter 由 CAT-GAIA-IMPL 建立；本节描述现状 C API，不声明 DLL 化完成）。

## On-disk 格式

- HiPS：IVOA 1.4（signal/support/snr 产品，NESTED，512 tile）。
- UPM：`astrocs-upm-v2` JSON（sparse）+ dense cache（checksum 校验）。
- Manifest：`manifest.json` / `diagnostics.json` / `controls_accept.json`。

详见 `docs/architecture/api_inventory.csv`（API 机器单源清单，与 `check_api_contracts` 的
`API_CONTRACTS.csv` 一致；完整分类清单）。
