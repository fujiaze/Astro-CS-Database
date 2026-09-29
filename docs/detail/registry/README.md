# registry

本目录存放 registry 模块的说明文档：按 module_id 组织的生产实现登记。页面事实以源码
descriptor、模块合同三件套（`lib/algorithms/<module>/`）与 SCI/ALG 正本为准；
生成页带生成锚，无锚页为人工内容。

## 职责边界

- 放：逐模块 registry 说明（生产实现落位、职责、入口与端口）、模块 ID 迁移基线，
  以及 descriptor 词汇与合同 module_id 的对齐登记。
- 不放：模块工作细节设计（在同级 `algorithms_phase1/`、`algorithms_phase2/`、`algorithms_phase3/` 与 `infrastructure/`）；模块总映射（在 `docs/modules/MODULE_MAP.yaml`）；ABI 合同（在 `docs/engineering/` 的 `API_STANDARD.md` 与 `C_ABI_STANDARD.md`）。

## 内容

- `astrocs.phase1.*.md` —— normalize 阶段 11 个模块的 registry 说明（calibration、cosmetic、drizzle、hips-writer、noise-snr、photometry、session、star-detection、star-psf、wcs-platesolve、writer）。
- `astrocs.phase2.*.md` —— mosaic 阶段 9 个模块的 registry 说明（coverage、integrate、reject、resample、sample、session、upm-apply、upm-fit、write）。
- `astrocs.phase3.*.md` —— export 阶段 6 个模块的 registry 说明（properties、resample、resample2、verify、wcs、writer）。
- `docs/modules/registry/module_id_migration_baseline.json` —— 模块 ID 迁移基线登记。**注意：该 JSON 未随本目录迁入，仍在 `docs/modules/registry/`**（两读法核实：`docs/detail/registry/` 下无此文件）。

## 上游

上游：docs/ASTROCS_DESIGN.md §8.5（模块与 ABI）。
