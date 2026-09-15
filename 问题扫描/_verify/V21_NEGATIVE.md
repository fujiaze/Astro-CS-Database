# V21 判否清单（**不得立条**，供其它轴复用为反证）

## §6.3 红线复算：support/coverage 当 ivar 权重 —— **未发现新 P0**
- 脚本 `scripts_v21_c2w.py` 命中 13 条候选，逐条通读后**唯一语义命中站点**是 `lib/phase2/tools/stage2.cpp:1118,1120,1141`（及 `:1373,1395` 同族），**判定为不命中，两条依据**：
  1. `weight_mode==2` 的 support 降级受同文件 **`:565-578` 显式门约束**：ivar 产品缺失数 >0 且 `legacy_allow_weight_fallback=false` 时**直接 `return 7`**，代码原话「拒绝继续，防止在非逆方差语义下冒充 ivar coadd」——只有**显式开降级开关**才 `weights[s]=support_v[s]` 且 diagnostics 标红计数；
  2. `:1141` 的 `weights[s]=support_v[s]*snr_v*snr_v` 属 `weight_mode==0`（**legacy snr^2 语义通道**），**不是 ivar 通道**。
- ⇒ **若其它轴报该站点，以上述二条为判否依据**（开关显式 + 非 ivar 通道）。这不解除 `M7-A-002`/`A-02` 的**文档三语义未收敛**问题，只是不新增 §6.3 红线违规站点。

## 其它判否/负结果
- 全仓 `assertTrue(True)` 仅 1 处（`V18-N-12`）；`O_EXCL`/独占创建 0 命中（`W4-R2-08` 已立条，此处指反向）；`|| true` 恒真 CHECK 恰 2 处（`M8-F-007` 在册）。

## 前台待办（我自己）
- `V12-N-17` 经 RC2 复核判**不可复核**：宿主指针「V12.md §2.17」不存在、22.5/20.6/20.48 三级检索零命中 ⇒ **不得判 STILL**，须由 V12 原轴补宿主后重派或撤条（我写的条目，我来补）。
- `M3-E-001` 账本原标 FIXED，RC2 复算**降级为 FIXED-PARTIAL**（矩阵 JSON 已重锚，但 `docs/TRACEABILITY.csv:20` 仍指旧测试且无 SUPERSEDED 牌、`TEST_MATRIX.md:32` 原样、三域 §13 `TST-*` 非法 ID 在位、matrix CSV 仅 2/30 行带牌、`flux_calibrator`/`02_FROZEN`/`SNR_*` 幻影引用在位）。
- 锚漂移集中订正清单（RC2 提供）：`M1a-A-006`(:1829→:2072)、`M2a-A-3`(:2266→:2525)、`M6a-I-002`(DATA:1978→:2046)、`V1-N-04`(:2544→:2808)、`M5b-G-16`(:97-101→:100-106)、`V7-N-10`(:2892→:2967)、`M1a-C-004`(:46-64→`p3_wcs_validate_request:61-82`/`p3_wcs_make:83-140`、:4842→:5786)；`M3-C-005`/`M2a-C-12` 的 `cosmetic real_ingest.cpp` **路径不存在**（疑混 `lib/calibration`）⇒ 只改锚不撤条。
