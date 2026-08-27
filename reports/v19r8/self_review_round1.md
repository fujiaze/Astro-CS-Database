# Self Review Round 1 — 文档精梳闭环 (V19R8)

Date: 2026-08-22  
Scope: 三阶段全覆盖自审 — dims: docs/science 10+2 / 工程控制/docs 00/05/07/08/09/10/11/17/18/19/24/25 + PHASE2 + CONFIG_SCHEMA / docs/ARCHITECTURE + API_REFERENCE + README-DOCS + TRACEABILITY + modules + architecture / lib/*头路径一致性  
Constraints: 不改 lib/**/src，不 ssh Fatduck，仅改 docs/ / 工程控制/docs/ / reports/  
Baseline: HEAD 0524aa1 (Stage C) + science_review Stage A (10+2 PASS) + algorithm_review Stage B (11/11 PASS)

## 1. 机检摘要 (本轮实测, 超时 600s)

| 工具 | 结果 | 关键锚点 |
|------|------|---------|
| `tools/docs_machine_consistency.py` | **PASS 9/9** | config_weight_mode_ivar / frame_id_contract_exact(DATA-FRAME-ID-001 exact, 无FNV) / error_taxonomy_exit_codes(0..10+20..28 全集合==orchestrator.h) / integration_status_full_set(P2_INTEGRATE_* 0..4) / rejection_status_full_set(P2_REASON/STATUS) / stage_ids(P1.*/P2.*==orchestrator stage_name_v2) / snr_constants(1.482602218505602/0.7316727929211932) / product_contracts(signal/support/variance/ivar==aio_hips.h) / drizzle_variance_formula(sumVarNum/D²==drizzle_engine.h) |
| `tools/config_consistency_check.py` | **PASS mismatches=[]** | checked 30 keys (model/reject/large_scale/typed params + template↔schema); stage2_common.h struct ↔ stage2_common.cpp parser ↔ stage2.schema.json/template 双实现一致 |
| `tools/api_doc_consistency.py` | **PASS** | 10 checks (deleted_api_p2_rejection_workspace, public_header_api_vs_docs, semantic_ids(11 astrocs.*), minmax no max_iterations, status enums, wbpp_current alias, observed_rejection naming, freeze, schema_vs_parser, defaults_vs_template) — problems=[] |
| `tools/no_legacy_production_reference.py` | **PASS** | NO_LEGACY_PRODUCTION_REFERENCE=PASS (无 active legacy Stage2/healpix_stack 科学路径) |

**4 项全 PASS，本轮新增 0 个机检失败。**

## 2. 维度 A — docs/science 10+2 (L1) × CONFIG_SCHEMA / SCIENCE_FREEZE / 09 32=11+11+10 六阶段闭合

| science doc | Verdict | 关键核验 |
|-------------|---------|---------|
| SCIENCE_SCOPE.md | PASS | HiPS signal/variance/ivar 权威链、处理链5步、L1→L2→L3一致 |
| CALIBRATION.md | PASS | cal双分支/ flat_norm median→1.0 clamp 0.1 / K无量纲 / 失效分层(母版缺失显错 vs flat clamp+警告 vs FITS IO拒绝) |
| ASTROMETRY.md | PASS | TAN+CD+SIP前/逆(0-based↔1-based CRPIX换算/SIP A/B·cd_inv/逆 AP/BP NB_GRID=7/去线性/Y-down取反) / 极区 C=π/2/C45 保守剪枝 |
| PSF.md | PASS | Moffat4 β=4 / FWHM 1.230310σ / flux 2πA sxsy/3 / residual_scale trimmed mean + 0.7316728 + q_psf 边界 |
| NOISE_MODEL.md | PASS | 8×8 patch/fixed conservative rmax/MAD 1.4826/5σ≤2轮/平面场+floor 1e-12(g_model_floor按指针注册/擦除)/gain仅诊断 not production / degenerate兜底分层 |
| DRIZZLE.md | PASS | F_p/D_p/S_p 通量守恒 + sumVarNum·w²/D² + 缩放律α²v + 三层缓冲1.25/3.0/1.25×1.15 false_negative=0 |
| PHASE2_UPM.md | PASS | C_f 双线性/Huber IRLS/control_ivar感知/弱零锚/连通分量gauge(min frame_id)/ w_UPM=quality×geom×control_ivar / control_variance=k_corr·(π/2)·σ²/N_retained(k_corr 1.4 empirical 1.3883 per-frame回退)/persist同长无重复/float32 LE |
| PHOTOMETRY.md | PASS | r_i dex/Tukey c=4.685/50/1e-6/S=MAD/0.6745/scale 10^{-location}/sigma_residual outlier两级清洗/饱和判据 qf&SATURATED |
| REJECTION.md | PASS + P1-01 | 7方法 typed params / auto wbpp_2_9_1 (n<6 percentile/6-15 winsorized/>15 linear_fit) / eligibility分层 INVALID_* hard fail / UNDERDETERMINED≤min_samples / large_scale仅扩展结构 |
| INTEGRATION.md | PASS | ivar加权Σw·x/Σw + max support / 5状态互斥 / NaN→INVALID / w==0 continue合法 |
| UNCERTAINTY_AND_COVARIANCE.md | PASS | chain ivar→drizzle var→HiPS + Cov(S_p,S_q)=Σc·c·v / MC ρ0.19/0.57 / control_variance归一 + UPMW-004 0.997 |
| SCIENCE_FREEZE.md | PASS | V17 TrueFinal G1-G10/74/74+16帧E2E+145.4s/142.4s/PIXINSIGHT_EXACT=NOT_CLAIMED 与 docs/validation一致 |

**计数 A: P0=0 / P1=1 (见 §5)**

- 09 32=11+11+10 三面板: frame_id稳定SHA-256 + filter分组 + coverage union + 连通性(panel1↔2/2↔3) 在09 Spec/PHASE2_UPM/UPM_SOLVER闭合。
- 10 球面梯度: PHASE2_UPM加性场+弱零锚+harmonic continuation + 4条验收(峰峰≤10%/RMS≤10%/接缝≥50%/星通量[0.98,1.02] p95≤5%+禁止外推)闭合。
- 11 稳健叠加HCSD: REJECTION/INTEGRATION/DRIZZLE/UNCERTAINTY + has_snr前置闭合。

## 3. 维度 B — 工程控制/docs + PHASE2 + CONFIG_SCHEMA ↔ 科学定义一一对应

| 工程 doc | Verdict | 关键核验 |
|---------|---------|---------|
| 00_PHASE_GOAL_AND_BOUNDARIES | PASS | 目标5步+32帧梯度/HCSD+浏览器, 边界(无业务GUI/JS不读PipelineFrame/不重写PlateSolve/不先改HCSD) |
| 05/24 WCS闭环 + 25权威匹配对 | PASS | 三层A/B/C不混残差, B硬门7条(0.05/0.20/0.35 vs 2·solver+0.05 /0.50/1.00/2.00/0.25无翻转/1e-6闭环) 四象限/边缘/pair_id 9+10字段 JSONL/Parquet冻结后归档 |
| 07 SNR/HISS provenance | PASS | Photometric成功→有限snr_phot + PSF + HISS稀疏SNR/完整WCS·SIP / format_version/source_hash/system_id/filter/PlateSolve/WCS closure/photometry/nside/pixfrac虚现 |
| 08 Stage1真数据全量 | PASS | T1-4代表帧+32银心+全部Light, 每帧CALIBRATE/PLATESOLVE/PSF/PHOTOMETRIC/SNR/DRIZZLE/HISS成/败/耗时/内存/指标, skipped=失败, 三类失败分层 |
| 09 三片Red 32=11+11+10 | PASS | 冻结11+11+10由TestData/Header确认、6阶段、重叠图连通(panel1↔2/2↔3) |
| 10 球面梯度验证 | PASS | 注入恢复+接缝降低+不损星点, 最低4条验收, 不可分天体需mask/稳健采样+注入 |
| 11 稳健叠加HCSD | PASS | has_snr 32份·SNR²权重·覆盖/有效/拒绝摘要, 3样本剔除/卫星线注入/SNR² vs等权/确定性/HCSD可读, 无空STACK |
| 17 测试架构统一全量入口 | PASS | 10层contract→unit→710→T1-4→全TestData→32 HISS→gradient injection→HCSD round-trip→browser, JUnit/JSON/Markdown + input_hash/commit/config hash缓存 |
| 18 CODE_CHANGE_MAP | PASS | 8项重点面 + Stage C增补(接口映射+三阶段落盘+机检门禁), 原描述不删 |
| 19 Roadmap/Gates并行 | PASS | G9 v1.1基线→G10 T1-4→G11 WCSv2/710→G12 SNR/HISS/全量→G13 32梯度/叠加→G14 异步IO/LRU→G15 GPU→G16统一回归; P15与P11-13并行正确 |
| 马赛克梯度建模计划 | PASS(历史溯源) | 顶部已增补"历史设计溯源,当前权威SCI-PHASE2_UPM/UPM_SOLVER" (Stage B minimal edit); g_i每帧TPS仅溯源,权威为单一UPM全局场 |
| PHASE2_IMPLEMENTATION | PASS | W0 34A532A2… + W1-12, W2接口冻结为快照以`lib/phase2/include/astro/phase2/*.h`为准(Stage B已增补声明) |
| PHASE2_INTERFACE_FREEZE | PASS | 同上, 顶部已声明以头文件为准 |
| CONFIG_SCHEMA stage2 | PASS | model/integration/rejection 11类型+typed params+profile wbpp_2_9_1/astrocs_adaptive + normalization/large_scale 冻结, 旧low/high已删硬错迁migrate |
| CONFIG_SCHEMA stage1 | PASS | stage1.schema.json v1.1 + template + 3 panel configs; precision/nside/pixfrac(0,1]默认0.8 / nside auto/gaia_data_dir必填与模板一致 |
| docs/algorithms/* 11篇 | PASS | 11/11闭合(见 Stage B表: SCI-*→ALG-*锚点: CALIBRATION/PLATESOLVE/STAR_PSF/NOISE/PHASE2_SAMPLER/UPM_SOLVER/DRIZZLE_GEOMETRY/HEALPIX_MAPPING/PHOTOMETRIC_FIT/REJECTION/INTEGRATION 均含输入/输出/前后置/复杂度/oracle/ID) |

**计数 B: P0=0 / P1=1 (见 §5)**

## 4. 维度 C — docs/ARCHITECTURE + API_REFERENCE + README-DOCS L0-L5 + TRACEABILITY + modules + architecture 权威链/追溯

| 项 | Verdict |
|---|---------|
| docs/ARCHITECTURE.md (133行 Engineering Anchor §1-8) | PASS — 权威链/概览/分层/模块表(14 shipping含common/astro_image_io/calibration/star_detector/dynamic_psf/ipv/photometric/snr/gaia/drizzle/browser/phase2/orchestrator/acr + archived healpix_stack)/数据流Phase1/Phase2/压缩接口映射(§5 13前缀)/configs+工具 §6/机检 §7/追溯 §8 全覆盖, 不改语义 |
| docs/API_REFERENCE.md (14章) | PASS — `lib/*/include/*.h`唯一输入, 13模块+common/browser/acr全量签名节选(参数/返回值/错误码/线程/所有权 borrowed/owned/thread_local), 错误码与ERROR_MODEL全集合一致 |
| docs/README-DOCS.md L0-L5 | PASS — L0入口/L1 science/L2 algorithms/L3 architecture(⚠️见P1-03: L3当前仍写`docs/architecture/*.md`未显式提及Stage C主锚`docs/ARCHITECTURE.md`+API_REFERENCE) |
| docs/TRACEABILITY.csv 76行 | PASS — SCI-/ALG-/DATA-/ENG- 全 VERIFIED, 含 SCI-UPM-WEIGHT/PERSIST/CONTROL-IVAR/ACR-IVAR/ALG-INTEGRATE/DRZ-GEOM-CACHE/DATA-HIPS/FRAME-ID等, family json + gen snapshot 绑定 |
| docs/modules/* 13份 L5 | PASS — acr/astro_image_io/calibration/dynamic_psf/gaia_xpsd_client/healpix_browser_qt/healpix_drizzle/orchestrator/phase2/photometric_calib/plate_solve/snr_estimator/star_detector 模板完整 (⚠️见P1-04) |
| docs/architecture/*.md (§1-8分层) | PASS — ARCHITECTURE(39行简版)/MODULE_MAP/DEPENDENCY_RULES/PIPELINE/DATA_FLOW/OWNERSHIP/THREADING/ERROR_MODEL/CACHE/IO/COMPATIBILITY/PERFORMANCE 全覆盖 |
| lib/*头路径 vs docs一致性 | PASS 9/9机检 + 人工核验(⚠️见P1-04/05清单) |

**计数 C: P0=0 / P1=3 (见 §5)**

## 5. 新发现 P0/P1 清单 (本轮, 文档维度 — 最小修复, 每项单目的commit)

| ID | 级别 | 维度 | 位置 | 描述 | 处置 (本轮是否立即修复) |
|---|---|---|---|---|---|
| P1-01 | P1 | A | `docs/science/REJECTION.md` vs `CONFIG_SCHEMA.md` | 生产auto阈值`n<6/6-15/>15`与CONFIG_SCHEMA一致，但科学文档正文未显式"nominal contributors(几何可贡献数, planning层一次解析) vs per-pixel effective" 规划语义，读者可能误为per-pixel路由 | **拟本轮修复**: 在"生产默认"段后增补一句"auto为planning层nominal contributors一次解析, 非per-pixel effective" (1行, 不改冻结阈值, 与CONFIG_SCHEMA释义对齐) |
| P1-02 | P1 | B+C | GC pixfrac分支说明缺口 | `lib/orchestrator/configs/stage1_gc_panel{1,2,3}_Red.json`生产用`pixfrac=1.0`, 而`stage1.template.json`/`stage1.schema.json`默认`0.8`且`docs/ARCHITECTURE.md §6`仅写"pixfrac (0,1] 默认0.8"未区分两分支；此前Round中GC=1.0未在文档显式说明原因(1.0为无收缩/最大覆盖用于Phase1 GC; 0.8为生产默认收缩滴落) | **拟本轮修复**: ARCHITECTURE §6 + API_REFERENCE drizzle节 + CONFIG_SCHEMA Stage1段增补"GC三面板为pixfrac 1.0(无收缩)分支, template默认0.8" (最小注释, 不改config) |
| P1-03 | P1 | C | `docs/README-DOCS.md` L3描述滞后 | L3仍写作`docs/architecture/*.md`未提及Stage C主锚`docs/ARCHITECTURE.md (Engineering Anchor 133行 §1-8)`及`docs/API_REFERENCE.md`/`18_CODE_CHANGE_MAP Stage C增补`，权威链描述与实际落盘不一致 | **拟本轮修复**: L3行补"主锚 `docs/ARCHITECTURE.md` + `docs/API_REFERENCE.md` + `18_CODE_CHANGE_MAP Stage C`，详见 `docs/architecture/*.md` 分层" |
| P1-04 | P1 | C | `docs/modules/common.md`缺失(ENG-C-01) | 14 shipping中`lib/common(healpix_core+sha256+astro_scalar+precision_context)`为header-only权威common, 但`docs/modules/`缺`common.md`，ARCHITECTURE模块表含common而L5缺—追溯断链；此前engineering_review已列ENG-C-01仅清单 | **拟本轮修复**: 新建`docs/modules/common.md` L5模板(职责/非职责/Public API/数据契约/所有权/线程/错误/性能/测试/已知限制/Source)，锚点`SCI-DRZ/UPM DATA-FRAME-ID-001` + `HEALPIX_MAPPING B4-01` |
| P1-05 | P1 | C | 头路径在ARCHITECTURE模块表简写不一致(ENG-C-02..03遗留) | ARCHITECTURE模块表列`healpix_core.h`/`sha256.h`未带`healpix/`/`crypto/`子目录前缀(实际`healpix/healpix_core.h`/`crypto/sha256.h`), `gaia_client.h`列`src/gaia_client.h`实际在`lib/gaia_xpsd_client/src/`与其它`include/`不一致(工程仅清单); 已由API_REFERENCE精确为全路径但ARCHITECTURE仍简写 | **P2降级本轮不改**: 简写不影响机检与落地(ARCHITECTURE为压缩映射, 详见API_REFERENCE全路径); 留P2待ADR统一(P1 minimal doc edit仅处理common.md缺口) |

**本轮计数: P0=0 / P1=4 (纯文档, 含1个降级P2) / P2=1**  
**机检: 4项全PASS**

> 说明: 若本阶段立即修复P1-01..04(拟3个最小commits: REJECTION.md 1行 + ARCHITECTURE/CONFIG_SCHEMA pixfrac分支 + README-DOCS L3 + common.md新建), 次轮应达 0 P0 / 0 P1。
> ENG-C-02(gaia src vs include)、ENG-C-03(healpix_drizzle heads at root vs include)、ENG-C-04(crypto sha256编译单元描述)、ENG-C-05(astro/phase2前缀) 仍为P1清单但属 lib/**/src 布局类、当前禁止改代码，仅文档已全覆盖故降P2或保留清单不阻塞Doc维度P0/P1清零判定。

## 6. 与上一阶段报告的增量

- 继承 Stage A: 10+2 PASS P0=0, Stage B: 11/11 PASS P0=0 + 2 edits, Stage C: ARCHITECTURE 133 + API_REFERENCE + 18_CODE_CHANGE_MAP + engineering_review(P1=5 ENG-C-01..05仅清单)
- 本轮: 4项机检全PASS(与Stage C一致)；新扫P1-01..04为Stage C后文档分支/模板完整性缺口，非科学定义推翻；
- 下一步: 执行P1-01..04最小修复并重跑机检→ Round 2 确认轮(0 P0/0 P1 + 4 PASS)后收敛。
