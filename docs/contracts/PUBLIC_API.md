# AstroCS Public / Internal API

## C ABI（`extern "C"`，不跨边界抛 C++ exception）

- `lib/astro_image_io`：`aio_*`（image/HiPS I/O、writer/reader、pipeline）。
- `lib/phase2`：`p2_*`（coverage / sampler / upm / integrate / stage2 入口）。
- `p2_upm_build`（obs-only，兼容）与 `p2_upm_build_geo`（全几何节点，
  V13/V14）。
- `p2_sample_controls`（含 background-clean stats/geometry 输出，V13）。
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

- `astrocs-stage2.exe <config.json>`（Phase2 唯一生产入口）。
- `orchestrator.exe <stage1.json>`（Phase1 唯一生产入口）。
- `healpix_browser_qt.exe`（HiPS 浏览器；`--hips/--standard-hips/--view/
  --screenshot/--lod/--exit`）。
- `toolchain.ps1 check|build|run|review`（统一工程入口）。

## JSON schema（config）

- stage1: `lib/orchestrator/configs/stage1_*.json`。
- stage2: `lib/phase2/configs/stage2_*.json`（model/integration/output/
  diagnostics 四段；默认值唯一来源见 `CONFIG_SCHEMA.md`）。

## On-disk 格式

- HiPS：IVOA 1.4（signal/support/snr 产品，NESTED，512 tile）。
- UPM：`astrocs-upm-v2` JSON（sparse）+ dense cache（checksum 校验）。
- Manifest：`manifest.json` / `diagnostics.json` / `controls_accept.json`。

详见 `reports/api_inventory.md`（V14 交付中的完整分类清单）。
