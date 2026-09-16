# AIO-001 · P0-OPEN 逐条结论（4 条）

> 清单 P0-OPEN：M2b-A-01 / M2b-A-02 / M2b-C-01 / V11-N-01。全部在本轮当前树（翻译后路径）复跑取证。
> 另：清单内 P0 共 5 条，第 5 条 M8-F-002 本轮判 **RESOLVED**（见 DISPOSITION_TABLE.md）。

---

## M2b-A-01 — HiPS 写入口不校验 nside 为 2 的幂 ⇒ 静默夹逼出与声明不一致的产品

- **翻译后路径**：`lib/astro_image_io/src/hips/aio_hips_writer.cpp` → `lib/infrastructure/aio/src/hips/aio_hips_writer.cpp`（生产调用点 `lib/core/src/module_adapters.cpp` → `lib/infrastructure/scheduler/src/module_adapters.cpp:4663`；`lib/hips/.../astro_sphere_sink.cpp` → `lib/algorithms/drizzle/healpix_drizzle/astro_sphere_sink.cpp:65/:247`）
- **复跑证据**（日志 `logs/01_M2b-A-01.log`、`01b_M2b-A-01_callsites.log`）：
  - `sed -n 493p lib/infrastructure/aio/src/hips/aio_hips_writer.cpp` → `        ps->leaf_order = ilog2_u64(nside);`
  - `grep -c "nside & (nside - 1)" lib/infrastructure/aio/src/hips/aio_hips_writer.cpp` → `0`（rc=1）
  - 同 grep `lib/algorithms/drizzle/hips/src/module_entry.cpp` → `1`（正解只在被绕过的模块面）
  - `ilog2_u64` 实现（`aio_hips_writer.cpp:105-109`）为**向下取整**：`while (v > 1) { v >>= 1; ++l; }`
- **结论：OPEN（本轮就地修复）**
  - 原判据在最新权威下仍成立：`docs/algorithms/HIPS_WRITER.md §1(1a)` 冻结「叶级 nside=2^K≥512」；非 2 的幂（如 600）被 `ilog2_u64` 静默夹逼到 2^9，据此写出的 NSIDE/A_cell/leaf_order 与调用方声明不一致 —— 属 `ASTROCS_DESIGN §9` 禁止的「看似完整产品」。
  - **改动**（改动 2）：`aio_hips_product_begin` 增加 `nside_is_pow2` 与上界 `nside <= 2^29` 校验，错误信息同步（`nside=2^K 且 512<=nside<=2^29`）。
  - **改前红**：`ctest -R p1hips_negative` → `[p1hips] negative: 6 check(s) failed`、`0% tests passed, 1 tests failed out of 1`（日志 `logs/43_nside_neg_red_proof.log`）
  - **改后绿**：`ctest -R p1hips_negative` → `Passed 0.37 sec`、`100% tests passed`（日志 `logs/44_nside_neg_green2.log`）
  - **未闭合面**：真正的唯一 AIO 收敛（把 `lib/algorithms/drizzle/hips` 的 `astrocs_p1_hips_writer` 退场、由 `lib/infrastructure/aio` 独占）属 INT-001 架构边界 ⇒ 登记（见 M2b-G-01）。

---

## M2b-A-02 — NSIDE 约定三源分歧（写侧 2^(k+9) / ALG 文档 2^k / 读侧 2^(k+9)）

- **翻译后路径**：同 M2b-A-01；`runtime/io/hips_core.c` 未迁移（IN-PLACE）。
- **复跑证据**（日志 `logs/02_M2b-A-02.log`）：
  - `sed -n 1069p aio_hips_writer.cpp` → `            const uint32_t nside_k = 1u << (k + 9);`（`1072: cards.push_back({"NSIDE", std::to_string(nside_k)});`）
  - `sed -n 146p docs/algorithms/HIPS_WRITER.md` → `  约定。FITS cards ORDERING=NESTED + NSIDE=2^k；三处 scatter 同 (2a) 式`
  - `sed -n 245p runtime/io/hips_core.c` → `  h->nside = 1ULL << ((uint64_t)order + 9u);`
  - `sed -n 630,637p runtime/io/hips_core.c` → `tile NSIDE=%lld 与 order %d (nside=%llu) 不符` 校验存在
- **结论：OPEN（不改，列入「需权威裁决」）**
  - 三源分歧逐字复现。**但本任务不能自行选口径**：
    - 写侧/读侧一致为 `2^(k+9)`，与 IVOA HiPS 1.0 的「叶级 NSIDE = tileWidth × 2^K」（tile_width=512=2^9）**一致**；
    - 偏差在 `docs/algorithms/HIPS_WRITER.md:146` 的 `NSIDE=2^k`。
  - `docs/algorithms/**` 为**只读权威**（AGENTS §1、任务卡「科学语义只读」），且该文件锚点正由 ARCH-001 同步（前台令：等 TEST-GREEN-001 交棒）⇒ 本任务**不改**。
  - **需裁决**：以写侧/读侧（2^(k+9)，合 IVOA）为准订正 ALG 文档第 146 行；或确认文档口径另有依据。

---

## M2b-C-01 — HiPS writer 无原子原语，且注释宣称「AIO-002 原子发布内建」

- **翻译后路径**：`lib/astro_image_io/src/hips/aio_hips_writer.cpp` → `lib/infrastructure/aio/src/hips/aio_hips_writer.cpp`；`lib/core/src/module_adapters.cpp` → `lib/infrastructure/scheduler/src/module_adapters.cpp`。
- **复跑证据**（日志 `logs/03_M2b-C-01.log`）：
  - `grep -c -e rename -e fsync -e staging aio_hips_writer.cpp` → `0`（rc=1）
  - `grep -n std::remove aio_hips_writer.cpp` → `232:`、`301:`、`1030:`
  - `grep -rn 原子发布 lib/infrastructure/scheduler/src/module_adapters.cpp` → `:28`、`:4602`「AIO-002 原子发布原语内建」
  - `sed -n 313p docs/algorithms/HIPS_WRITER.md` → `| DISP-HIPS-004 | 高 | C++ 写出无原子发布：… writer 层不冒认已原子 |`
- **结论：OPEN（部分闭合）**
  - 原判据仍成立：writer 内 `rename/fsync/staging` 命中 0，`remove + create` 直写仍在；两处注释仍宣称原子发布内建，与 `DATA_SEMANTICS §12.5:383` 及 `DISP-HIPS-004` 直接冲突。
  - **本轮闭合的部分（M9-G-6）**：`properties`（IVOA+provenance 唯一文本载体）已改为**同目录临时文件 → fflush → fsync → 原子 rename**（新增 `lib/infrastructure/aio/src/aio_atomic_file.h` 作为 AIO 统一落盘原语），失败经子产品错误码 −3..−8 传播并清理临时文件。
  - **未闭合**：FITS/MOC/metadata.xml/manifest.json 各写点仍直写；目录级 `staging → 校验 → 原子 rename` 需 INT 层接线（`DISP-HIPS-004` 的「INT 层接线或 writer 内嵌事务」二选一）⇒ 属架构性边界，超出本任务最小改动面，登记后续。

---

## V11-N-01 — `AioHipsSnrPoint` C/Python 镜像字段不一致且无 struct_size/abi_version

- **翻译后路径**：`lib/astro_image_io/include/aio_hips.h` → `lib/infrastructure/aio/include/aio_hips.h`；`lib/astro_image_io/tests/hips_direct_smoke.py` → `lib/infrastructure/aio/tests/hips_direct_smoke.py`。
- **复跑证据**（日志 `logs/04_V11-N-01.log`）：
  - C 头字段数 → `C AioHipsSnrPoint field-count 6`（`ra_deg/dec_deg/snr/star_id/quality_flags/photometric_status`，`aio_hips.h:84-92`）
  - Python 镜像 → `_fields_` 仅 4 项：`ra_deg/dec_deg/snr/source_id`（`hips_direct_smoke.py:34-38`）
  - `grep -n snr.push_back aio_hips_writer.cpp` → `914:            ps->snr.push_back(pts[i]);`
  - `grep -n -e struct_size -e abi_version lib/infrastructure/aio/include/aio_hips.h` → **0 命中**
  - **附加（本轮新发现）**：`hips_direct_smoke.py:43` 仍硬编码旧 DLL 路径 `ROOT + r"\lib\astro_image_io\astro_image_io.dll"` ⇒ ARCH-001 迁移后该 smoke 已无法加载目标 DLL。
- **结论：OPEN（不改，列入「需权威裁决」）**
  - 原判据逐字复现：C 侧按 40 字节步长取元素，而按镜像分配的调用方只给 32 字节/元素 ⇒ 交付的 HiPS SNR 目录第 2/3 条仍为错位数据；结构体仍无 `struct_size`/`abi_version`。
  - **需裁决（`ASTROCS_DESIGN §7.3` ABI 兼容域）**：修法有三条互斥路线 ——（a）补 Python 镜像 2 字段并保持 40 字节布局；（b）改 C 结构布局（破坏 ABI/已冻结 `API-HIPS-001`）；（c）新增版本化结构 + `struct_size` 并按 ABI 版本分派。三者均改变**跨 DLL C ABI 面**，须负责人裁决后才能改。
  - `hips_direct_smoke.py` 的旧 DLL 路径属文档/脚本同步面，可随裁决一并订正（本任务不改，避免与 ABI 裁决在同一文件反复改）。

---

## 需权威裁决清单（本任务停下上报）

| # | 条目 | 要裁决什么 | 为什么不能自行改 |
|---|---|---|---|
| 1 | **M2b-A-02** | 以写侧/读侧 `2^(k+9)` 为准订正 `docs/algorithms/HIPS_WRITER.md:146`，还是文档口径另有依据 | `docs/algorithms/**` 只读权威；且其锚点正由 ARCH-001 同步 |
| 2 | **V11-N-01** | `AioHipsSnrPoint` ABI 修法（补镜像 / 改 C 布局 / 版本化结构） | 改变已冻结的跨 DLL C ABI（`API-HIPS-001`、`ASTROCS_DESIGN §7.3`） |
| 3 | **M2b-H-01** | `moc_sky_fraction` 字面精度统一到哪一侧（6 位 vs 8 位 vs 全精度），以及 `<1e-9` 容差与字面量化的关系 | 与冻结容差面（`HIPS_WRITER.md §9`）不自洽，且既有断言把 6 位小数值钉成期望 |
| 4 | **M2a-H-3** | support 钳制后层级通量归约公式，或 I2 不变量的口径 | 直接改科学公式/不变量 |
| 5 | **M9-F-3** | `drizzle_scale_arcsec` 的上界取值与是否接受 0 | 上界属科学域参数定义（冻结容差面） |
| 6 | **M2b-G-01 / M2b-C-01 剩余面** | 第二阶段独立 HiPS writer（`astrocs_p1_hips_writer`）退出路径与目录级原子发布接线归属 | 架构性边界（INT-001 域） |

