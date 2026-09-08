# astrocs.p2.sampling — Phase2 控制点采样模块（P2-SAMP）

> P2-SAMP-DOC（2026-09-09，SA-P2-S20）新建模块页。合同三件套落位
> `lib/phase2_samp/`（README r1 + module.yaml + memory.md，CONTRACT_READY，
> entrypoint=MISSING）——迁移目标目录按 `lib/phase2_int/`（P2-INT-DOC，
> 其按 `lib/hips_p2/` P2-HIPS-DOC 先例）→ `lib/phase2_rej/`（P2-REJ-DOC）
> 先例新建；`lib/phase2/` 三件套已被 P2-COV（astrocs.p2.coverage）占用
> （lib/phase2/README.md r1，一目录一套 README/module.yaml/memory.md，
> 不可覆盖）。生产源 `lib/phase2/src/sampler.cpp`（1156 行，根
> CMakeLists astrocs_phase2 静态库成员 :337-346/:342）+ 唯一权威签名头
> `lib/phase2/include/astro/phase2/sampler.h`（136 行）；唯一生产消费方
> `lib/phase2/tools/stage2.cpp`（probe/fill 两遍调用 :279-311）。

## 身份与合同

- MOD ID：`MOD-astrocs-phase2-sample`（registry 行 ID 沿用
  `MOD-astrocs-phase2-sample`）；module_id：`astrocs.p2.sampling`
  （MODULE_MIGRATION_MATRIX P2-SAMP 行；descriptor 编排层占位
  astrocs.phase2.sample 由 P2-XX-INT 对齐，不反向作冻结依据）；
  dll_target：`astrocs_p2_sampling.dll`（合同值，尚未存在，迁移归
  P2-SAMP-IMPL）。
- owner SA-P2-S20；depends_on_int=P2-COV-INT;CPU-005；
  legacy_paths="lib/phase2 sampling sources"（均以
  MODULE_MIGRATION_MATRIX.csv P2-SAMP 行为权威）。
- 合同链：SCI-UPM-001（docs/science/PHASE2_UPM.md，FROZEN T106
  2026-08-23，集合 SCI-UPM-001..010 + SCI-UPM-WEIGHT-001 +
  SCI-UPM-PERSIST-001，页头明示模块 "phase2 (upm/sampler)"；descriptor
  占位 SCI-P2-SMP-001⇒SCI-UPM-001 映射声明于
  docs/algorithms/PHASE2_SAMPLER.md §11.4，占位 ID 不入合同）
  → ALG-P2-SMP-001（docs/algorithms/PHASE2_SAMPLER.md，兼承接
  ALG-UPM-CONTROL-IVAR-001 方差子面）
  → DATA-P2-SMP（DATA_SEMANTICS §23）/ API-P2-SMP-001
  （PUBLIC_API Phase2 sampling 公共消费面节）→
  TEST-P2-SMP-001（登记面=设计冻结 TEST-P2-SMP-DESIGN-001 VERIFIED，
  承载于 docs/modules/registry/astrocs.phase2.sample.md §独立 synthetic
  验证节 + ALG 文档 §11.3 F1-F9 容差；可执行测试 MISSING 归
  P2-SAMP-TEST，不冒认）。

## 职责（摘要，权威=ALG-P2-SMP-001 §4/§5）

- 三阶段 background-clean 控制点采样（BACKGROUND_SAMPLER_SPEC.md
  Stage A-E 映射，sampler.cpp:584-591 冻结注释）：Stage A 候选
  patch（cell 中心 ±background_patch_radius，默认 17×17）→
  Stage B 亮端迭代 sigma-clipping（median/MAD，保留负值）→
  Stage C DBE-like 局部 tolerance gate（同 tile 邻域基线）→
  Stage D contamination/retained 双门 → Stage E SNR catalogue veto。
- 控制点几何由 union 几何与目标角间距决定、**不由 SNR 决定**
  （sampler.h:6 语义冻结）：每 union tile 8×8 cell 网格，
  control_id=cells 索引，out_n_controls=n_union×G²（含空覆盖占位，
  sampler.h:118-119）。
- control estimator 方差权威：control_variance = k_corr × (π/2) ×
  sigma_bg² / N_retained（:840-842；ALG-UPM-CONTROL-IVAR-001）；
  k_corr 逐帧 Drizzle provenance 查标定表（:88-109/:547-555，选项 B），
  缺失回退冻结保守默认 1.4（:82；h:47-53）。
- frame_id 内容稳定身份：truncated-64 canonical SHA-256（9 properties
  + signal tiles + support tiles "S" 前缀 + SNR catalogue，:314-438；
  h:85-93 冻结）；与 UPM 域共享同一实现。
- 非职责：不做 UPM 拟合/权重归一（P2-UPM 域）、不做星点检测
  （SNR 纯查询，sampler.h:11-12）、不做 coverage union 计算
  （上游 P2-COV 域 DATA-P2-COV）、不产 per-pixel 科学场产品。

## 关键合同事实

- ≥2 帧 clean 观测才进入 UPM（:1013-1027；相对光度约束，单帧区
  由 UPM Laplacian 延拓，upm.h:93-99）。
- snr_available 语义冻结：0=无局部星点（snr 回退整帧精确中位，
  :860），禁止以 1.0 伪装 unknown（upm.h:51-55）。
- obs.ivar 弃用仅诊断：单 leaf Phase1 ivar ≠ Var(control estimator)，
  科学权重一律 control_ivar；ivar 产品缺失 → o.ivar=0.0 如实降级
  （:1039-1056；upm.h:38-41）。
- 确定性：输出 obs 序列 bitwise 与 worker 数无关（1/N worker 等价，
  固定槽位写回 cells[idx] :870-872 + 第三遍单线程顺序扫描；
  sampler_parallel_consistency_test.cpp:29 承载）→ CON-006。
- 并发模型：worker 数唯一来源=Runtime lease（cfg.cpu_workers，
  stage2.cpp:273-274 由 ExecutionOptions 透传；模块无
  hardware_concurrency 自行开线程，:880-883）；cpu_workers>1 走
  std::thread 池（per-worker 独立 AIO 句柄 ：894），=1 串行
  reference（共享句柄经 g_aio_mu :161/:166 串行化）；默认参考构建
  P2_ENABLE_OPENMP=OFF（lib/phase2/CMakeLists.txt:28 hotfix 保留
  仅影响旧 target 编译面；实现唯一并行路径=std::thread :882-883，
  h:54-56 OpenMP 表述为历史状态，h:55 注释漂移未整改=注释面债）。
- 错误面：rc=0 成功 / rc=1 错误 + err 8KB 文本（frame_id 0、open
  failed、n_union>1e6、cells>2e8、resize OOM、首 tile 越界、
  exception）；out_obs/out_controls 容量不足按 capacity 截断拷贝、
  out_n_* 返回真实需求（probe/fill 协议，sampler.h:101-102）。
- 统计量共享：p2_stats_median/p2_stats_mad（1.4826 系数）与 UPM 域
  同一实现（sampler.h:96-99）；median 偶数 n 取 [begin,mid) 最大值
  均值（P0-01 修复，:196-203）。

## 验证

可执行 `TEST-P2-SMP-001` MISSING（P2-SAMP-TEST 建立，不冒认）；
登记面=TEST-P2-SMP-DESIGN-001 设计冻结 VERIFIED，承载于
docs/modules/registry/astrocs.phase2.sample.md §独立 synthetic
验证节 + ALG-P2-SMP-001 §11.3 F1-F9 容差（F1-F6/F8-F9
bitwise/解析/计数精确，F2 角点 exact 插值 rtol 1e-12、F3-F5
rtol 1e-12、F4 atol 1e-9 deg）。现状相邻证据（引用不冒认）：
lib/phase2/tests/synthetic_gate.cpp Phase2Sampler 组
（RealHipsControlSampling :3423、G6LocalSnrAvailabilityThreeZones
:3470、G1StatisticsCorrectness :3594、UPMW-004 MC :4001、cvar 公式
:4089）+ Phase2SamplerParallel.OneTvsTwoTDeterminism
（sampler_parallel_consistency_test.cpp:29）+ ivar_wiring_test.cpp
WireProductionStage2PerFrameIvar :223。

## 链接

- README/module.yaml/memory.md：`lib/phase2_samp/`（本目录）
- SCI：docs/science/PHASE2_UPM.md（SCI-UPM-001，FROZEN T106
  2026-08-23，零改动）
- ALG：docs/algorithms/PHASE2_SAMPLER.md（ALG-P2-SMP-001）
- DATA：docs/contracts/DATA_SEMANTICS.md §23（DATA-P2-SMP）
- API：docs/contracts/PUBLIC_API.md API-P2-SMP-001；
  API-P2-001（编排层既有）
- 模块页：docs/modules/registry/astrocs.phase2.sample.md
- 新模块页：docs/modules/phase2_samp.md

## 已知限制（DISP-P2SMP，登记不改码，整改归 P2-SAMP-IMPL/TEST）

| ID | 摘要 | 源锚 |
|---|---|---|
| DISP-P2SMP-001 | 配置修补 `<=0→默认` 吞显式 0（意图"禁用"的 0 被静默改写为默认值，如 background_clip_iters=0 想关 clipping 反得 3） | sampler.cpp:485-502 |
| DISP-P2SMP-003 | 诊断进度日志直写 stderr（17 处 fprintf/fflush），未走结构化日志通道 | sampler.cpp:641-672 等 |
| DISP-P2SMP-004 | catalog veto 阈值 10×frame_snr_med 与半径 0.012° 硬编码，未入 P2SamplerConfig | sampler.cpp:849-850 |
| DISP-P2SMP-005 | clipping 相对收敛阈值 1e-12×max(|m0|,1e-12) 在 m0≈0 时过严，退化为固定轮数全迭代（确定性无影响，性能观察级） | sampler.cpp:818 |
| DISP-P2SMP-002 | 第三遍 ：1022 对 reason==2 拒绝帧重复 `++rejected_insufficient_retained`（与 ：1006 双计数），统计面偏差（obs 输出不受影响） | sampler.cpp:1022 |
