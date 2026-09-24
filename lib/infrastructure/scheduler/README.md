# lib/infrastructure/scheduler — 运行时内核与 Phase 节点适配层

本目录承载 Astro Celestial Sphere Database（ACSD） 的运行时内核（scheduler/executor/pipeline/artifact/runtime）
与三个 Phase 的**生产节点适配实现**。唯一真实节点实现集中在
`src/module_adapters.cpp`：CLI 只做薄入口，科学计算由这里的 `p1_op_*` /
`p2_op_*` / `p3_op_*` 委托到各模块库。

> 科学定义与合同权威在 `docs/science/`、`docs/contracts/`；本 README 只登记
> **编排层**（节点配置键、来源、审计字段）语义，不得反向改写合同。

## Phase1 WCS 节点：初始指向来源（`p1_op_wcs`）

权威合同见 `docs/contracts/DATA_SEMANTICS.md` §18.5。要点：

- 负责人裁定：**帧头 WCS 未授权**。`p1_op_wcs` 不得读取/使用帧头
  `CRVAL1/2`、`PLTSOLVD`、`CD`/`PC`、`SIP` 作为初始指向或任何解算输入。
- `wcs.init_source` 取值（枚举外一律 DATA fail-closed，禁 silent default）：
  - `header_pointing`（**默认首选**）：中心取帧自身 `OBJCTRA`/`OBJCTDEC`
    （六进制；RA 为小时，×15 转度），回退 `RA`/`DEC`；板尺度
    `s0 = 206.265·XPIXSZ/FOCALLEN`（FOCALLEN mm、XPIXSZ μm；常量与求解器
    `ipv_select.cpp:57 IPV_ARCSEC_PER_UM_PER_MM=206.265` 一致，不是 asec/rad
    的 206264.806——后者只在 XPIXSZ 记 mm 时成立）。
  - `config`：`config.wcs.ra0/dec0` + `focal_length_mm`/`pixel_size_um`。
  - `neighbor_crval`：`config.wcs.neighbor_ra0/neighbor_dec0`，来源必须是
    **我们自己已解出的产物**（本管线 `p1_wcs.json` 的 CRVAL），不是帧头 WCS。
- `gaia_data_dir` 仍是真实 ipv 求解链必需的数据参数，任何时候都必须显式给出。
- **审计登记（F-10）**：`p1_wcs.json` 与节点 manifest 逐帧登记
  `wcs_init_source`、`wcs_init_center_src`、`wcs_init_ra0_deg`、
  `wcs_init_dec0_deg`、`wcs_init_s0_arcsec_px`。
- 旧值 `header_crval` 已移除；传入即 DATA 拒绝。

## 构建

`lib/infrastructure/scheduler` 无独立 CMake 目标，随根 `CMakeLists.txt` 编入 `astrocs_core` /
`astrocs_module_adapters`。测试面见 `eng/tests/unit/`（`core_scheduler` 等）。
