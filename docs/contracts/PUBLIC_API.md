# AstroCS Public / Internal API

## C ABI（`extern "C"`，不跨边界抛 C++ exception）

- `lib/astro_image_io`：`aio_*`（image/HiPS I/O、writer/reader、pipeline）。
- `lib/phase2`：`p2_*`（coverage / sampler / upm / integrate / stage2 入口）。
  - `p2_upm_build`（obs-only，兼容）与 `p2_upm_build_geo`（全几何节点，
    V13/V14）。
  - `p2_sample_controls`（含 background-clean stats/geometry 输出，V13）。
- 返回码：0=OK；非 0 具体语义见各头文件注释；`err` 缓冲只做日志，不承载
  状态机。

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
