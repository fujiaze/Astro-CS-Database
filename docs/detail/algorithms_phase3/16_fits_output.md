# 插件文档：fits_output（流式 FITS 输出）

> 上游：ASTROCS_DESIGN.md §6.2（export 流程）

## 1. 职责与边界

- **职责**：把重采样后的平面产品流式写出为测量意义明确的 FITS 文件（PRIMARY + 扩展 HDU + WCS + provenance）。
- **不是**：不做重采样/投影（resample/projection）；失败/取消的产物面 = 无正式产品。

## 2. 权威依据

- 最高设计 `ASTROCS_DESIGN.md` §6.2（export 流程：投影到平面、WCS 直接计算生成）、§6.3（输出模式显式声明）、§10（I/O 与原子产品：输出不带权重、原子提交）
- `docs/detail/PHASE3_DETAILED_DESIGN.md` §5-§6
- FITS Standard v4.0（IAU FITS Working Group, 2016；Pence et al. 2010, A&A 524, A42）

## 3. 输入/输出数据合同

- **输入**：重采样产品（signal/统计、variance、correlation、coverage、validity、PSF、provenance 材料）、配置；输入 HiPS 产品的落盘形态由落盘名判定，裸/归档同义；输入合同**不设** `storage_form` 键，出现即 REJECT。
- **输出形态**：交付物为**裸 FITS，不压缩、不套壳**；Phase3 不产出 HiPS，不使用 `.hips` / `.hips.zst` 命名。
- **输出**：
  - **输出不需要带权重**——上游已完成叠加，这里只投影到平面并直接计算生成对应 WCS；
  - PRIMARY：所选科学 signal/flux/statistic；
  - 扩展 HDU：COVERAGE、VARIANCE/IVAR（语义择一且一致）；其余候选面（VALIDITY、SUPPORT、REJECTION、POINT_INFORMATION/W、PSF 表/图与 correlation 描述）为待实现项，落盘前须先在 `docs/science/DATA_SEMANTICS.md` §27 立输出行与 HDU 合同；
  - 标准 WCS（**直接计算生成**）、BUNIT（写端口单位 `UnitId::SURFACE_BRIGHTNESS`，落盘值 = `flux_sum / covered_area` = 面亮度，canonical 串 **`ADU/sr`**）、DATASUM/CHECKSUM；
    **BUNIT 语义**：主 HDU 的 `BUNIT` = 输入 HiPS `signal/properties#BUNIT` 声明的 canonical 串（canonical 值 `ADU/sr`）；缺声明时按 `docs/science/DATA_SEMANTICS.md` §31.2(b) 处理——`BUNIT = "ADU"` 要求 provenance 声明 `pixel_semantics = "surface_brightness"`；`VARIANCE`/`IVAR` 扩展 HDU 的 `BUNIT` = 主 HDU BUNIT 的平方 / 倒数（`FZ-P3-BUNIT-QUADRATIC`）。单位口径唯一权威 = `docs/science/DATA_SEMANTICS.md` §31.1a（单位定义）/ §31.2（BUNIT 语义）；
  - provenance：源 product/hash、软件完整 SHA、配置、投影、核、order、近似、生成时间。
- **不确定度可得性（fail-closed，唯一出口）**：输入 HiPS 不含 variance/ivar 子产品（或权重非纯逆方差、发生 fallback 等 §30 规则项）时 → **不写** VARIANCE/IVAR 扩展 HDU（禁静默丢弃、禁用常量 0 冒充）+ manifest 写 `uncertainty_available=false` + diagnostics 标红计数；**该键不是失败态**，是 unavailable 显式登记模式。正本：`docs/science/DATA_SEMANTICS.md:2733-2739`（规则）与 `:2837-2839`（显式登记，禁占位/静默缺键/空输出冒充）。
- 节点产物：`output_phase3.fits`、`p3_writer.json`（写侧自述）、`p3_verify.json`（独立复核面）；
- 所有 HDU shape/WCS 对齐。
- 参考：`docs/science/DATA_SEMANTICS.md` §27（DATA-P3-FITS：模块输入/输出数据合同正本）。

## 4. 算法与公式要点

- 写临时文件 → flush/close/fsync → 标准 checksum → 原子 rename → 重开独立验证；
- 流式：按行带/块执行，内存 `O(width×band_height + tile_cache)`，**常驻内存以该式为上界**；
- cache 只缓存，不改变 order/核/科学值；
- 线程预算来自 Runtime；并行输出与单线程科学结果一致。

## 5. 配置项

| 字段 | 默认 | 单位 | 说明 |
|---|---|---|---|
| `band_height` | —— | px | 输出行带高度（内存预算） |
| `tile_cache_mb` | —— | MB | tile 缓存上限 |
| `compression` | `none` | —— | 压缩选项（无/无损） |
| `mode` | `surface_brightness` | —— | surface_brightness/point_source_flux/visualization |

## 6. 接口/ABI

- entrypoint：重采样产品 → 原子 FITS 文件 + 验证记录；模块落点 = `lib/algorithms/fits_output/p3_output.cpp`（签名头 `lib/algorithms/fits_output/p3_output.h`，C ABI 入口 `p3_output_write_atomic` / `p3_output_write_atomic_ex` / `p3_output_verify`；模块注册 = `lib/infrastructure/pipeline/module_ports.registry.json` 的 `astrocs.phase3.writer`）；生产接线 = `lib/infrastructure/scheduler/src/module_adapters.cpp` 的 `p3_op_writer`；
- 复用 infrastructure/aio 的原子提交设施（临时文件隔离 + 校验 + fsync + 原子 rename；**单写者前提**：同一输出路径同一时刻只有一个写者，跨进程不取文件锁）。

## 7. 错误与边界

- 输出模式缺所需科学层 → 拒绝或明确 unavailable；
- BUNIT 与实际量纲不一致（`bunit="ADU"` vs `flux_sum/covered_area`）⇒ 必须显式失败或标注 unavailable，**声明值以实际量纲为准**（`"ADU"` 只在量纲一致时使用）；
- >2 GiB、长 UTF-8 路径、Windows CFITSIO、取消、缺 tile 必须正确处理；
- 取消 → 无可见半成品（临时文件隔离 + 原子 rename）。

## 8. 测试与 Oracle

- 标准 FITS 验证器（checksum/结构/WCS）；
- 重开独立验证内容与写入一致；
- 不同 block/cache/worker 输出科学值一致；
- 取消/失败无半成品测试；
- >2 GiB 与长路径测试。
