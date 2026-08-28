# DOC-001 执行日志 (2026-08-28, vm-bj)

## 输入
03_TASK_DETAILS DOC-001 行; docs/science/(CALIBRATION/DRIZZLE/ASTROMETRY/CONTROL_WEIGHT_SNR 等); docs/contracts/DATA_SEMANTICS.md; lib/calibration/src/cosmetic_corrector.cpp(极性核实)。

## 动作
1. 盘点现有定义: DATA_SEMANTICS §1/§2/§4/§4a/§5(坐标/HEALPix/support/invalid/variance/ivar/frame_id), ASTROMETRY(0/1-based 与 CRPIX 不变量), CALIBRATION(单位表), DRIZZLE(flux vs surface_brightness 禁混淆), CONTROL_WEIGHT_SNR(帧质量权重), cosmetic_corrector.cpp:158-166(bad_mask 极性实测: 1=坏点)。
2. 冻结 docs/GLOSSARY.md 18 核心术语, 每词唯一含义+单位/极性/域+legacy alias→迁移+权威锚点: adu/electron/variance/ivar/pixel_weight/frame_quality_weight/support/invalid/nan/bad_mask/product_bit_flags/ra_dec/pixel_coordinate/healpix_ordering/frame_id/signal/surface_brightness/calibration_units。
3. 关键消歧: pixel_weight(=ivar) 与 frame_quality_weight(=support×snr_v²) 为两个独立术语; bad_mask(1=坏点) 与 support/invalid(HiPS 域) 分域冻结, 禁止裸用 "mask"; DN 列为被禁 alias。
4. 机器检查 tools/check_glossary.py: G1 必备术语齐备/ G2 唯一性/ G3 禁二义表述(TBD/待定/二选一)/ G4 alias 映射唯一/ G5 锚点文件存在/ G6 被禁 alias 在 science+contracts 域未迁移即 FAIL。
5. 测试 tests/glossary/ 5 用例(真实仓 PASS/重复术语/二义注入/alias 冲突/裸 DN mutation)。

## 验证
- GLOSSARY_PASS terms=18/18 alias映射=3, G6 迁移扫描 docs/science+docs/contracts 无裸 DN(迁移已闭合)。
- 全量回归 unittest discover **15/15 OK**(version 5+traceability 5+glossary 5); 三既有 checker 全绿。

## 产物
docs/GLOSSARY.md; tools/check_glossary.py; tests/glossary/; 本日志。

## PASS 判定
正式 glossary 冻结; 机器检索模糊/冲突定义成立(mutation 5/5); 核心术语无 TBD/二选一; legacy alias 已列迁移并机器执行。DOC-001 = PASS。
