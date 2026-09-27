# DATA_SEMANTICS 代码锚重锚登记（fix 单）

权威：本文档只登记 DATA_SEMANTICS.md 实现锚 (`file.ext:line` / `file.ext:line-line2`) 的行号重定位记录，
判据与语义零改动；锚的语义负载以 DATA_SEMANTICS.md 正文为准。

## 1. 方法

- 核证器：`eng/tests/contracts/check_data_semantics_anchors.py`（三档分类：
  old_still_valid / high_confidence / manual_review；复跑命令
  `python3 eng/tests/contracts/check_data_semantics_anchors.py --report`）。
- high_confidence 是候选行号；每条替换前按文档行语义与目标行内容逐条对照复核，
  复核驳回的候选（含中文注释行对位但 ASCII token 失配的盲区）不落笔。
- 区间锚按首行定位，区间长度保持原 span（若结构块整体扩张，端点按块界调整，
  见 §2 的 coverage.h 条目）。

## 2. 已核证重锚（22 处）

| DATA_SEMANTICS.md 行 | 锚 | 登记行号 | 现行行号 | 核证依据 |
|---|---|---|---|---|
| 372 | hp_drizzle_api.cpp:412-421 | 434-443 | 412-421 | CDELT 读取行 `aio_frame_kv_get_double(frame, "header", "CDELT1", 0.0)` |
| 374 | orchestrator.cpp:3367-3379 | 3313-3325 | 3367-3379 | precision_mode 写入注释 `"fp32" (默认) 或 "fp64", drizzle DLL 写入 HISS metadata` |
| 382 | hp_drizzle_api.h:55 | 39 | 55 | `weight_path: 可选权重 FITS 文件路径` 参数注释（文件通道权重面） |
| 772 | orchestrator.cpp:2411-2419 | 2403-2411 | 2411-2419 | `写入 star_measurements 权威块 (FLOAT64 [N,15], schema astrocs-star-measurements-1)` 注释 |
| 832 | p1_session.cpp:558-566 | 349-357 | 558-566 | `s->host->allocator.alloc(...)` host 分配行 |
| 836 | commands.cpp:910 | 668 | 910 | `g.granted_workers = astrocs::core::granted_worker_observation()` |
| 893 | orchestrator.cpp:1556-1575 | 1593-1612 | 1556-1575 | `using sdet_create_fn = StarDetectorHandle (*)(const SDetParams*)` |
| 907 | orchestrator.cpp:2252-2280 | 2218-2246 | 2252-2280 | `星点检测权威块 (FLOAT64 [N,6]: x,y,flux,mag,saturated,has_saturated...)` 注释 |
| 1105/1112 | coverage.h:32-44 | 32-38 | 32-44 | `P2HipsInputInfo` struct 域（B2-A8 注释与 hips_ordering 字段并入，起始行不变） |
| 1199/1288 | stage2.cpp:792-805 | 565-578 | 792-805 | `if (ivar_product_missing > 0)` fail-closed 段至 `return 7`（ivar 产品缺失显式科学错误） |
| 1717 | sampler.h:137-140 | 85-93 | 137-140 | `@param frame_ids 长度 n_inputs，与 hips_paths 同序；0 视为非法` 注释块 |
| 1717 | sampler.h:138 | 117 | 138 | 同上（0=非法哨兵冻结注释） |
| 2067 | upm.h:125-127 | 99-101 | 125-127 | `由全局平滑/Laplacian 延拓得到 C（harmonic continuation）` 注释块 |
| 2074 | p2_session.cpp:206 | 189 | 206 | `空间 UPM 必须显式 control leaf 层级(order=target+9); 取 coverage 实测值` 注释 |
| 2110/2127 | upm.cpp:316 | 256 | 316 | `m->info.precision = 1;  // fp64 reference` |
| 2135 | upm.cpp:65 | 56 | 65 | `double M{0.0};  // latent unified reference（待求）` |
| 2558 | p3_wcs.cpp:105 | 40 | 105 | `if (std::fabs(centre_dec_deg) > kMaxAbsDec) return P3_WCS_PARAM;` |
| 2574 | p3_wcs.h:14-23 | 12-20 | 14-23 | `struct P3WcsDescriptor` 域（crval/crpix/cd/width/height/projection 字段面） |
| 2379 | p3_output.h:20-29 | 15-24 | 20-29 | `struct P3Provenance` 起始行（8 字段） |

## 3. 保留原锚（old_still_valid，抽查代表性条目）

- p2_session.h:19（两处）：注释 `/* 纯读; 无 IO; 拒未知键/缺必需键(无 silent default) */` 仍精确对位
  「validate 纯读无 IO」与「拒未知键/缺必需键」两条引用。
- p3_resample.h:150-159 / 93-96：样本级掩膜（rule_id `NAN-SAMPLE-MASK-COVERAGE-NAN`）
  注释块仍对位。
- hp_drizzle_api.h:39 以外的 G3-5 注释块、hp_drizzle_api.cpp:427-447（CTYPE2 读取段）等
  由核证器归入 old_still_valid 的条目维持原行号。

## 4. 待人工核证（manual_review）

- 核证器报告 manual_review 约 180 条：弱匹配/多候选/无语义 token/
  basename 解析歧义四类。全量清单以
  `python3 eng/tests/contracts/check_data_semantics_anchors.py --report` 现场输出为准，
  本文档不复制该清单（避免与现场输出漂移）。
- 处置约定：逐条按 DATA_SEMANTICS.md 行语义在目标文件中定位同语义行；
  目标文件已重构且语义无法对应时，登记为语义级重锚需求（引用符号/函数名替代行号），
  走相应条目处理。

## 5. 边界

- 本文档登记的是行号指针修正；实现行为、判据数值、语义负载零改动。
- 核证器的 token 分类依赖 ASCII 标识符与引号串；中文注释行与英文行号锚的语义
  对位以人工对照为最终依据。
