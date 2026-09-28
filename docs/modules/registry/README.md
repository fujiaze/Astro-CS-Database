# registry

本目录存放 registry 模块的说明文档：按模块 ID 组织的生产实现登记，由源码 descriptor 生成。

## 职责边界

- 放：逐模块 registry 说明（生产实现落位、职责、入口与端口）与 ID 迁移基线。
- 不放：模块工作细节设计（在 docs/plugins/）；模块总映射（在上级 MODULE_MAP.yaml）；ABI 合同（在 docs/api/）。

## 内容

- `astrocs.phase1.*.md` —— normalize 阶段 11 个模块的 registry 说明（calibration、cosmetic、drizzle、hips-writer、noise-snr、photometry、session、star-detection、star-psf、wcs-platesolve、writer）。
- `astrocs.phase2.*.md` —— mosaic 阶段 9 个模块的 registry 说明（coverage、integrate、reject、resample、sample、session、upm-apply、upm-fit、write）。
- `astrocs.phase3.*.md` —— export 阶段 6 个模块的 registry 说明（properties、resample、resample2、verify、wcs、writer）。
- `module_id_migration_baseline.json` —— 模块 ID 迁移基线登记。

## 上游

上游：docs/ASTROCS_DESIGN.md §8.5（模块与 ABI）。
