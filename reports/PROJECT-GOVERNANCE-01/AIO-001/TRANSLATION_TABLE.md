# AIO-001 · 旧→新路径翻译表（ARCH-001 迁移映射）

> 依据：`cmake/ARCH-001-migration-manifest.md`（35 个旧 `lib/` 目录已迁；3 个 `*_session` DEFERRED）。
> 口径：清单里证据命令的旧路径**全部失效**；下表先翻译再复跑。所有翻译项均以 `test -e` 实测核对（日志 `run/PROJECT-GOVERNANCE-01/AIO-001/logs/07_path_translation_probe.log`）。

| 旧路径（清单/账本原文） | 新路径（当前树） | 状态 | 依据 / 备注 |
|---|---|---|---|
| `lib/astro_image_io/**` | `lib/infrastructure/aio/**` | DONE(ARCH-001 #13) | 7.1 infrastructure/aio（唯一 FITS/HiPS/manifest I/O 边界） |
| `lib/astro_image_io/src/hips/aio_hips_writer.cpp` | `lib/infrastructure/aio/src/hips/aio_hips_writer.cpp` | DONE | 同左；本轮改动 3 处在此外 |
| `lib/astro_image_io/src/hips/aio_hips_reader.cpp` | `lib/infrastructure/aio/src/hips/aio_hips_reader.cpp` | DONE | 同左 |
| `lib/astro_image_io/src/aio_fits.cpp` | `lib/infrastructure/aio/src/aio_fits.cpp` | DONE | 同左 |
| `lib/astro_image_io/src/aio_upm.cpp` | `lib/infrastructure/aio/src/aio_upm.cpp` | DONE | 同左 |
| `lib/astro_image_io/src/aio_pipeline.cpp` | `lib/infrastructure/aio/src/aio_pipeline.cpp` | DONE | 同左 |
| `lib/astro_image_io/src/healpix/aio_healpix_io.cpp` | `lib/infrastructure/aio/src/healpix/aio_healpix_io.cpp` | DONE | 同左 |
| `lib/astro_image_io/src/ahpx/aio_ahpx_reader.cpp` | `lib/infrastructure/aio/src/ahpx/aio_ahpx_reader.cpp` | DONE | 同左；**该文件不在构建图内（ahpx 已废弃）** |
| `lib/astro_image_io/src/astro_image_io.cpp` | `lib/infrastructure/aio/src/aio_api.cpp` | DONE(推断+实测) | 旧名未单列；按内容(公共 aio_* C ABI 实现)对应新树唯一候选 |
| `lib/astro_image_io/include/aio_hips.h` | `lib/infrastructure/aio/include/aio_hips.h` | DONE | 同左 |
| `lib/astro_image_io/src/aio_util.h` | `lib/infrastructure/aio/src/aio_util.h` | DONE | 同左 |
| `lib/astro_image_io/tests/hips_direct_smoke.py` | `lib/infrastructure/aio/tests/hips_direct_smoke.py` | DONE | 同左；**该镜像内仍硬编码旧 DLL 路径 lib\astro_image_io\astro_image_io.dll**（V11-N-01） |
| `lib/io/src/io_adapter.cpp` | `lib/infrastructure/aio/io/src/io_adapter.cpp` | DONE(ARCH-001 #14) | MODULE_MAP: aio legacy_paths 含 lib/io |
| `lib/hips/src/aio_publish.cpp` | `lib/algorithms/drizzle/hips/src/aio_publish.cpp` | DONE(ARCH-001 #10) | MODULE_MAP: drizzle legacy_paths 含 lib/hips |
| `lib/hips/src/module_entry.cpp` | `lib/algorithms/drizzle/hips/src/module_entry.cpp` | DONE | 同左 |
| `lib/hips/src/astro_sphere_sink.cpp` | `lib/algorithms/drizzle/healpix_drizzle/astro_sphere_sink.cpp` | DONE | 生产源在 healpix_drizzle 内（非 lib/hips） |
| `lib/hips/include/astrocs/hips/publish.h` | `lib/algorithms/drizzle/hips/include/astrocs/hips/publish.h` | DONE | 同左 |
| `lib/hips/CMakeLists.txt` | `lib/algorithms/drizzle/hips/CMakeLists.txt` | DONE | 仍在根 CMakeLists.txt:208 add_subdirectory |
| `lib/healpix_db/healpix_drizzle/v6_drizzle_science.h` | `lib/algorithms/drizzle/healpix_drizzle/v6_drizzle_science.h` | DONE(ARCH-001 #11) | 同左 |
| `lib/core/src/module_adapters.cpp` | `lib/infrastructure/scheduler/src/module_adapters.cpp` | DONE(ARCH-001 #34) | 7.1 infrastructure/scheduler |
| `lib/hips_p2` | `lib/algorithms/coverage/hips_p2` | DONE(ARCH-001 #33) | 同左；目录内仅 module.yaml/README/memory.md 三份文档 |
| `lib/hips/**（其余）` | `lib/algorithms/drizzle/hips/**` | DONE | 同左 |
| `00_COMMON_CONTRACTS.md` | `**无对应（全仓 0 命中）**` | GONE | W3-R2-009 的合同锚悬空：旧文档不在当前树，也无新替代 |
| `lib/phase3_session/p3_output.cpp` | `**不翻译（保留原位）**` | DEFERRED(ARCH-001) | 3 个 *_session 待 INT-001 删除；本任务不改 |
| `lib/phase3_session/p3_session.cpp` | `**不翻译（保留原位）**` | DEFERRED(ARCH-001) | 同上 |
| `runtime/io/fits_core.c` | `runtime/io/fits_core.c（未迁移）` | IN-PLACE | ARCH-001 未纳入 35 目录；W4-R2-08 / M9-G-4 / M9-G-5 锚点仍有效 |
| `runtime/io/hips_core.c` | `runtime/io/hips_core.c（未迁移）` | IN-PLACE | 同上；M2b-A-02 读侧锚点 |
| `modules/services/io/**` | `modules/services/io/**（未迁移）` | IN-PLACE | M2a-E-4 锚点仍有效 |
| `tests/unit/io_ownership_test.cpp` | `tests/unit/io_ownership_test.cpp（未迁移）` | IN-PLACE | M8-F-002 锚点仍有效 |
| `cli/commands.cpp、cli/parser.cpp` | `cli/commands.cpp、cli/parser.cpp（未迁移）` | IN-PLACE | W5-N-06 锚点仍有效；CLI-001 域，本任务只读 |
| `docs/algorithms/HIPS_WRITER.md` | `docs/algorithms/HIPS_WRITER.md` | IN-PLACE(只读) | ARCH-001 未同步锚；本任务只读，不改 |

## 无法翻译 / 需注意项

- `00_COMMON_CONTRACTS.md`：**全仓 0 命中**，W3-R2-009 引用的合同锚悬空（无新替代）⇒ 该条部分判据不可复跑，已登记。
- `lib/phase3_session/**`：ARCH-001 明确 DEFERRED（等 INT-001 删除），不翻译、不改。
- 行号：迁移不改变行号，但**迁移前即已漂移**（CONTINUATION §9.2 陷阱 3）。本轮一律按符号/重定位复跑，日志逐条留痕。
- `lib/astro_image_io/src/astro_image_io.cpp` 旧名未在 ARCH-001 表单列；新树唯一对应为 `src/aio_api.cpp`，属**推断项**，已标 `DONE(推断+实测)`。
- 新增（本任务）：`lib/infrastructure/aio/src/aio_atomic_file.h`（AIO 统一原子落盘原语，header-only）。

