# RELEASE-02 代码修复分片回执 — CI 门禁（防复发）

- 分片：**CI 门禁（防复发）**，执行面 = ci/ 检查器 + ci/checks.json 单一注册表 + 显式台账
- 日期：2026-09-19；零 git 写权限；未改 docs/**；未跑 ninja/cmake/ctest
- 产物：run/RELEASE-02/fix-gates/（脚本/日志/逐项 JSON/caught_problems.json）
- 依据：docs/ci/CI_SPEC.md §4/§7、docs/ci/01_CHECKS.md §1（能红能绿 / fail-closed / 可执行负例面）、ENGINEERING_SPEC.md §8

---

## 0 结论速览

| # | 检查项 | 门禁级 | 真仓结果 | 自证（--self-test） | 当前抓到 |
|---|---|---|---|---|---|
| 1 | CHK-ALGO-WIRING | P0 | PASS（20/20 op 绑定；67 符号入 DORMANT 台账） | 绿/红/绿 + fail-closed 均命中 | 67 个 module.yaml source_symbols 不在生产二进制 |
| 2 | CHK-REGISTRY-IR-PARITY | P0 | PASS（22 注册 ↔ 20 IR，2 差异入台账） | 绿/红×2/绿 + fail-closed | astrocs.phase2.resample、astrocs.phase3.resample 注册但不在管线 |
| 3 | CHK-CONFIG-CONSUMED | P0 | PASS（17 模板键，2 死键入台账） | 绿/红/绿 + fail-closed | wcs.center_deg、wcs.s_out_deg 生产零读取 |
| 4 | CHK-CONFIG-DEFAULTS | P0 | PASS（6 差异入台账） | 绿/红/绿 + fail-closed | smoothing_lambda 0.0 vs 0.1 等 6 处 |
| 5 | CHK-PROD-SCALE | P0 | PASS（control_ivar/control_variance 均有生产尺度用例） | 绿/红×2/绿 + fail-closed | 16 处 O(1) control_ivar/control_variance 合成用例 |
| 6 | CHK-PROVENANCE-CONSISTENCY | P0 | PASS（32 记录一致 + 生产/消费侧锚点） | 绿/红×2/绿 + fail-closed | 当前 0 违规（19 个真实 p1_phot.json 全部自洽） |
| 7 | CHK-REALDATA-E2E | P0（slow/heavy） | --plan/--verify-only PASS（3 writer + 3 verifier） | 绿/红/绿 + fail-closed | 当前 0 违规；E2E 已脚本化 |

- 注册表：ci/checks.json 由 46 → 53 项；新项插在 CHK-KNOWN-FAILURES-BASELINE **之前**（R10 末位约束）。
- ci/id_migration_map.json：7 个新 ID 登记为 KEPT + absorbed（R13 通过）。
- **唯一红项**：CHK-REGISTRY-DOC-SYNC（因 docs/ci/01_CHECKS.md §2 未登记 7 个新 ID）—— 见 §5，需前台裁决。
- 科学行为变更：**0 条**（本分片只加门禁与台账，未改任何科学公式/默认容差/SCI·ALG 冻结定义）。

---

## 1 执行面与硬约束遵守

- 只新增 ci/check_*.py、ci/gate_common.py、ci/ledgers/**、ci/fixtures/provenance/**；
  只修改 ci/checks.json、ci/id_migration_map.json（注册所必需）；**未改 docs/**、未改 lib/ 生产代码。
- 不跑 ninja/cmake/ctest；仅 python3 + nm（既有 check_prod_reachability.py 同款依赖）。
- 每个检查器：--self-test（tempfile 夹具，正例必绿 + 负例必红 + fail-closed 断言）；
  锚点缺失/不可解析/台账非法一律 rc=2，绝不静默降级（scanned==0 ⇒ rc!=0）。
- 台账不是后门：每条 entry 必须有 id/kind/reason/owner/exit_condition 五字段，缺一即 rc=2
  （ci/gate_common.py load_ledger）。空 reason / 无解除条件无法通过。

---

## 2 新增检查项清单

| ID | 名称 | 入口 | 关键判据 | 锚点 |
|---|---|---|---|---|
| CHK-ALGO-WIRING | 算法/关键 API 生产调用图可达性门 | ci/check_algo_wiring.py | W1 registry (module,op,entry) 必须在 register_phase_modules 绑定；W2 lib/**/module.yaml 函数型 source_symbols 必须在生产二进制 nm 可见 | module_ports.registry.json、module_adapters.cpp、build/astrocs、dormant_algorithms.json |
| CHK-REGISTRY-IR-PARITY | 注册表 ↔ Pipeline IR 双向一致门 | ci/check_registry_ir_parity.py | 注册 descriptor module_id 集合 == build_pipeline_ir 节点 module_id 集合（双向） | module_adapters.cpp、runtime_client.cpp、registry_ir_parity.json |
| CHK-CONFIG-CONSUMED | 生产配置键消费门 | ci/check_config_consumed.py | config/templates/*.json#config 每个叶子键必须在 lib/** 生产源码出现（或台账登记 consumed_as） | config/templates/*.json、lib/**、dead_config_keys.json |
| CHK-CONFIG-DEFAULTS | 生产默认值一致性门 | ci/check_config_defaults.py | 配置键集合（模板/defaults.json/schema/watch_keys）内同键 ≥2 个不同字面量缺省即红 | config/**、contracts/schemas/phase_config_*.schema.json、config_default_divergences.json |
| CHK-PROD-SCALE | 生产尺度参数门 | ci/check_prod_scale.py | 台账声明参数在生产源码的字面量赋值 |v/prod| 必须落在 [floor,ceil]，且必须有生产尺度用例 | prod_scale_params.json、lib/**/tests/**、tests/** |
| CHK-PROVENANCE-CONSISTENCY | 产品 provenance 自洽门 | ci/check_provenance_consistency.py | 生产/消费侧锚点 + 产品 JSON：photometry_applied=false ⇒ photscal==1.0 且 bunit != ASTROCS_RELATIVE_FLUX；=true ⇒ photscal 有限>0 | module_adapters.cpp、hiss_writer.cpp、产品 JSON、provenance_exceptions.json |
| CHK-REALDATA-E2E | 真实数据 E2E（slow） | ci/check_realdata_e2e.py | normalize×N → mosaic → p3_writer/p3_verify 断言 + FITS 有限像素占比 | run/RELEASE-01/e2e/l4/configs、run/RELEASE-02/L4-rebuild/mosaic_49.json、build/astrocs |

### 2.1 注册表与治理联动

- ci/checks.json：7 项均为顶层注册项（platform：ALGO-WIRING/REALDATA-E2E = linux，其余 any；
  REALDATA-E2E profiles=[linux-deep]、heavy=true、waivable=true）。
- ci/id_migration_map.json：targets + mappings(KEPT) + coverage.absorbed_entries 各 +7；
  source_entry_count 保持 157（191+7 - (34+7)），语义=基线后新增执行单元 absorbed。
- ci/validate_registry.py --registry ci/checks.json --strict → PASS（0 error）。
- CHK-IMPACT-MAP → PASS；CHK-EXIT-CONSISTENCY → PASS（scanned=104，findings=0）。

---

## 3 每项能红能绿证据

所有自证日志：run/RELEASE-02/fix-gates/logs/<ID>.selftest.log；汇总 evidence_summary.tsv。
自证夹具全部用生产尺度/真实结构，不用 O(1) 合成掩盖（PROD-SCALE 的绿例即生产尺度 5.6e-22）。

### CHK-REGISTRY-IR-PARITY（logs/CHK-REGISTRY-IR-PARITY.selftest.log）
- 绿：green_parity（注册 ⊆ IR 且 IR ⊆ 注册）。
- 红：red_registered_not_in_ir（注册 astrocs.phase2.resample 但 IR 无该节点）；
      red_ir_not_registered（IR 有 astrocs.phase2.ghost 但未注册）。
- 绿：green_ledgered（台账承载已知差异）。
- fail-closed：missing_anchor（IR 锚点缺失 ⇒ GateError）、bad_ledger（缺 reason ⇒ GateError）。
- 真仓：PASS registered=22 ir_nodes=20。

### CHK-ALGO-WIRING（logs/CHK-ALGO-WIRING.selftest.log）
- 绿：green_wired（registry op 已绑定 + 全部 source_symbols 在符号表）。
- 红：red_dormant_symbol（p2_coverage_free 不在符号表）；red_unbound_op（dead_op 未绑定）。
- 绿：green_ledgered；fail-closed：no_binary（无生产产物 ⇒ GateError）、bad_ledger。
- 真仓：PASS registry_ops=20 bound=20 dormant_symbols=67。

### CHK-CONFIG-CONSUMED（logs/CHK-CONFIG-CONSUMED.selftest.log）
- 绿：green_all_consumed；红：red_dead_keys（wcs.center_deg / wcs.s_out_deg 零命中）；
  绿：green_ledgered（consumed_as 别名）；fail-closed：missing_template。
- 真仓：PASS template_keys=17 dead_keys=2。

### CHK-CONFIG-DEFAULTS（logs/CHK-CONFIG-DEFAULTS.selftest.log）
- 绿：green_consistent（两文件同缺省）；红：red_divergent_default（0.1 vs 0.0）；
  绿：green_ledgered；fail-closed：zero_defaults。
- 真仓：PASS config_keys=58 scanned=24 divergent=6。

### CHK-PROD-SCALE（logs/CHK-PROD-SCALE.selftest.log）
- 绿：green_prod_scale（control_ivar=5.6e-22）；红：red_synthetic_scale（=1.0）；
  红：red_missing_prod_case（只有合成尺度、无生产尺度用例）；绿：green_ledgered；
  fail-closed：no_params。
- 真仓：PASS params=[control_ivar, control_variance] scan_files=392。

### CHK-PROVENANCE-CONSISTENCY（logs/CHK-PROVENANCE-CONSISTENCY.selftest.log）
- 绿：green_consistent；红：red_photscal_and_bunit（photscal=2.5 + bunit=RELATIVE_FLUX）；
  红：red_producer_neutral_photscal（生产中性分支写 2.5）；绿：green_ledgered；
  fail-closed：zero_records。
- 真仓：PASS records=32 files=128（含 19 个真实 run/RELEASE-01 p1_phot.json）。

### CHK-REALDATA-E2E（logs/CHK-REALDATA-E2E.selftest.log / .plan.log / .verify.log）
- 绿：green_e2e；红：red_e2e（covered_px=0 + coverage_ok=0 + canonical_match=false）；
  fail-closed：zero_products。
- 真仓：--plan 输出 21 条 normalize + 1 条 mosaic 命令（missing_prerequisites=[]）；
  --verify-only 对 run/RELEASE-02/L4-rebuild 校验 writers=3 verifiers=3 → PASS。
- --execute 为 slow 路径（heavy=true、linux-deep、timeout 20000s），本分片未跑真跑。

---

## 4 当前抓到的问题清单

机器可读：run/RELEASE-02/fix-gates/caught_problems.json（含每条 reason）。

### 4.1 CHK-REGISTRY-IR-PARITY（2）
- registered_not_in_ir:astrocs.phase2.resample —— module_adapters.cpp:7980-7985 注册
  phase2_descriptor()（P2 模板复制残留占位），phase2 IR 链无此节点 ⇒ 永不被调度。
- registered_not_in_ir:astrocs.phase3.resample —— module_adapters.cpp:7988-7992 注册
  phase3_descriptor()，phase3 IR 链无此节点（:8049-8050 注释自认占位）。

### 4.2 CHK-ALGO-WIRING（67 个 dormant symbol；58 dormant_uncompiled + 9 dormant_unlinked）
按模块（完整清单见 ci/ledgers/dormant_algorithms.json）：
- astrocs.p1.noise（7）：snr_noise_model_v1 / _f64 / _default_config / _fill / _free /
  snr_noise_scale_law / snr_noise_gain_variance —— 定义在 lib/algorithms/noise_snr/cpp/src/
  noise_model.cpp，但生产 target astrocs_phase1_noise 只编 wrapper_phase1/noise_model.cpp
  （CMakeLists.txt:588-591）⇒ **ALG-NOISE-001 实现面不在生产二进制**。
- astrocs.p1.photometry（6）：pc_calibrate_simple* 全系 —— 不在任何生产静态库/二进制。
- astrocs.p3.projection（12）：p3_wcs_make/pix2world/world2pix/fits_keywords +
  p3_projection_registry_table/find/find_id/selfcheck/make/pix2world/world2pix/fits_keywords。
- astrocs.p3.resample（8）：p3_order_select / p3_resample_check_mode / p3_sampler_* /
  p3_sample_nearest/bilinear。
- astrocs.p1.star_detection（7）：sdet_detect_impl / sdet_gauss_fit / sdet_lm_fit /
  reject_star / sdet_dedup_stars / sdet_sort_stars / sdet_compute_bgnoise。
- astrocs.p3.fits_writer（6）：p3_output_write_atomic / p3_output_verify / p3_wcs_*。
- astrocs.p1.cosmetic（5）：detect_hot_pixels / detect_cold_pixels /
  filter_by_structure_size / interpolate_pixels / correct_frame。
- astrocs.p1.hips_writer（4）：aio_publish_stage_create_v1 / discard_v1 / tree_fsync_v1 /
  promote_v1；astrocs.p2.hips_writer（4）：p2_stage2_parse_config / p2_stage2_make_upm_cfg /
  p2_acr_block_eligible / p2_block_plan。
- astrocs.phase1.session（6）：p1_session_create/validate/run/inspect/destroy/last_error
  （libastrocs_phase1_session.a 有定义但未链入生产二进制；生产 phase1 走 p1_op_* 节点）。
- astrocs.p1.psf（2）：dpsf_batch_dims_check / moffat4_fit_tmpl。
- 说明：9 个 dormant_unlinked 是「静态库有定义但未链入 build/astrocs」；58 个
  dormant_uncompiled 是「不在任何生产静态库」。二者都需前台裁决：接线 / 从 module.yaml
  移除声明 / 出具 DISP 编号。**其中 noise_snr 与 photometry 两组最需优先裁决**
  （可能是整块算法实现未编入生产 target）。

### 4.3 CHK-CONFIG-CONSUMED（2）
- dead_config_key:wcs.center_deg（config/templates/export.phase_config.json）——
  生产实际读 wcs.center.{ra_deg,dec_deg}（parser.cpp:304；module_adapters.cpp:6957-6967）。
- dead_config_key:wcs.s_out_deg（同文件）—— 生产实际读 wcs.scale_deg_per_px（parser.cpp:304）。
- 后果：用户按模板填 center_deg/s_out_deg 会被静默忽略。

### 4.4 CHK-CONFIG-DEFAULTS（6 个 divergent key）
- smoothing_lambda：stage2_common.cpp:140（"auto" → 0.1）vs upm.cpp:234（缺省 0.0），
  synthetic_gate.cpp:1641 注释称「生产语义 smoothing=0」⇒ 同一科学旋钮双口径。
- enabled / height_px / width_px / max_iterations / precision：已登记为「同名不同义/未初始化
  哨兵/示例值」，见台账 reason。

### 4.5 CHK-PROD-SCALE（16 处合成尺度用例）
- control_ivar=1.0（ratio 1.79e21）：synthetic_gate.cpp:73/86/4286 等 + sanitize_driver.cpp:44；
- control_variance=1.0（ratio 5.56e-22）：synthetic_gate.cpp:72/85/4285 + sanitize_driver.cpp:43。
- 已有生产尺度判别力回归 synthetic_gate.cpp:543-560（kProdIvar=5.6e-22 / kProdUnc=4.2e10）
  ⇒ 台账登记豁免，解除条件=既有合成用例迁移到生产尺度。

### 4.6 CHK-PROVENANCE-CONSISTENCY（当前 0）
- 19 个真实 p1_phot.json（run/RELEASE-01/e2e/evidence）全部 photometry_applied=false 且
  photscal=1.0；生产侧中性分支与 hiss_writer 拒绝守卫均在位。

### 4.7 CHK-REALDATA-E2E（当前 0）
- run/RELEASE-02/L4-rebuild：3 个 p3_writer.json（covered_px=12,516,801 ≤ total=16,777,216）
  + 3 个 p3_verify.json（coverage_ok=1、reopen_ok=1、canonical_match=true）全通过。

---

## 5 需前台裁决的文档同步项

**不改 docs/**（硬约束）。以下为必须由前台落 docs 的同步项：

### 5.1 docs/ci/01_CHECKS.md §2 增补 7 行（当前 CHK-REGISTRY-DOC-SYNC 红）
在 §2 表格 CHK-IMPACT-MAP 行之后、CHK-KNOWN-FAILURES-BASELINE 之前插入：

    | CHK-ALGO-WIRING | 治理 | 算法/关键 API 生产调用图可达性（DORMANT 台账） | python3 ci/check_algo_wiring.py --json-out run/ci/fix-gates/algo_wiring.json | P0 |
    | CHK-REGISTRY-IR-PARITY | 治理 | 生产注册表 ↔ Pipeline IR 双向一致 | python3 ci/check_registry_ir_parity.py | P0 |
    | CHK-CONFIG-CONSUMED | 治理 | 生产配置键消费（死键 no-op） | python3 ci/check_config_consumed.py | P0 |
    | CHK-CONFIG-DEFAULTS | 治理 | 生产默认值一致性 | python3 ci/check_config_defaults.py | P0 |
    | CHK-PROD-SCALE | 科学 | 关键算法生产尺度参数 | python3 ci/check_prod_scale.py | P0 |
    | CHK-PROVENANCE-CONSISTENCY | 科学 | 产品 provenance 自洽 | python3 ci/check_provenance_consistency.py | P0 |
    | CHK-REALDATA-E2E | 科学 | 真实数据 E2E（slow/heavy，linux-deep） | python3 ci/check_realdata_e2e.py --execute | P0 |

### 5.2 docs/ci/CI_SPEC.md §3/§4（建议）
- §3 检查项总表新增「治理/防复发」类：算法接线、注册表↔IR、配置消费/默认值、生产尺度、provenance 自洽、真实数据 E2E；
- §4 门禁：明确「真实数据 E2E 为 slow 门（linux-deep，waivable，前置缺失 SKIP 77）」，
  与「合成测试不等于真实数据 VERIFIED」并列。

### 5.3 台账的文档可见性（建议）
- ci/ledgers/*.json 是新的机器可审台账面；建议在 docs/ci/01_CHECKS.md §1 增一条
  「显式台账（DORMANT/dead-key/default-divergence/prod-scale/provenance）为唯一豁免载体，
  每条须带 reason/owner/exit_condition，缺字段即 runner error」。

### 5.4 与既有文档的冲突/提醒
- docs/algorithms/PHASE2_MOSAIC_WRITE.md / NOISE_ESTIMATION.md 等引用 ALG-NOISE-001
  snr_noise_model_v1 的实现位置，但该符号不在生产二进制（§4.2）；文档是否需要更新
  「生产实现面」表述，请前台裁决（本分片不改 docs）。

---

## 6 产物与复跑

    run/RELEASE-02/fix-gates/
      run_evidence.sh               # 一键复跑全部自证 + 真仓 + 联动门
      evidence_summary.tsv          # check/mode/rc 汇总
      logs/<ID>.selftest.log        # 能红能绿证据
      logs/<ID>.real.log            # 真仓运行
      logs/CHK-REALDATA-E2E.plan.log / .verify.log
      logs/validate_registry.log / registry_doc_sync.log / runner_new_checks.log
      caught_problems.json          # 当前抓到的问题（台账聚合）
      <check>.json                  # 逐项机器可读结果
      gen_dormant_ledger.py / gen_caught.py / register_checks.py / register_migration_map.py

复跑：

    export TMPDIR=/dev/shm/fix-gates
    bash run/RELEASE-02/fix-gates/run_evidence.sh
    python3 ci/run_checks.py --check CHK-ALGO-WIRING CHK-REGISTRY-IR-PARITY \
        CHK-CONFIG-CONSUMED CHK-CONFIG-DEFAULTS CHK-PROD-SCALE \
        CHK-PROVENANCE-CONSISTENCY --quiet

---

## 7 科学行为变更

**0 条。** 本分片只新增检查器/台账/注册项，未改任何科学公式、默认容差、SCI/ALG 冻结定义、
生产源码或 docs。唯一被触动的仓库文件是 ci/checks.json、ci/id_migration_map.json（注册所必需）
及新增的 ci/check_*.py、ci/gate_common.py、ci/ledgers/**、ci/fixtures/**。

### 附：并发提醒（给前台）
- 工作区为多分片共享：证据采集期间 lib/infrastructure/scheduler/src/module_adapters.cpp
  被 FIX-P1 分片修改（新增 apply_photometry 路径），本分片的 provenance 生产侧锚点已改为
  同时支持「字面中性对」与「条件式 X ? Y : 1.0」两种写法，避免被合理重构误判。
- CHK-ALGO-WIRING 的 DORMANT 台账基于当前 build/astrocs（2026-09-19）；若前台重建二进制，
  台账需按新 nm 重跑 gen_dormant_ledger.py 重新三方裁决（未接线项会重新变红，属预期 fail-closed）。
