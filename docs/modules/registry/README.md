# registry

本目录存放 registry 模块的说明文档：按 module_id 组织的生产实现登记。页面事实以源码
descriptor、模块合同三件套（`lib/algorithms/<module>/`）与 SCI/ALG 正本为准；
生成页带生成锚，无锚页为人工内容。

## 职责边界

- 放：逐模块 registry 说明（生产实现落位、职责、入口与端口）、模块 ID 迁移基线，
  以及 descriptor 词汇与合同 module_id 的对齐登记。
- 不放：模块工作细节设计（在 docs/plugins/）；模块总映射（在上级 MODULE_MAP.yaml）；ABI 合同（在 docs/api/）。

## 内容

- `astrocs.phase1.*.md` —— normalize 阶段 11 个模块的 registry 说明（calibration、cosmetic、drizzle、hips-writer、noise-snr、photometry、session、star-detection、star-psf、wcs-platesolve、writer）。
- `astrocs.phase2.*.md` —— mosaic 阶段 9 个模块的 registry 说明（coverage、integrate、reject、resample、sample、session、upm-apply、upm-fit、write）。
- `astrocs.phase3.*.md` —— export 阶段 6 个模块的 registry 说明（properties、resample、resample2、verify、wcs、writer）。
- `module_id_migration_baseline.json` —— 模块 ID 迁移基线登记。

## 上游

上游：docs/ASTROCS_DESIGN.md §8.5（模块与 ABI）。
