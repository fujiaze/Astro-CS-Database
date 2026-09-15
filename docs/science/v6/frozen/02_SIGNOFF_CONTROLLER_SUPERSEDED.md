> 由 `reports/v6/contract-review/tools/gen_freeze.py` 机械渲染，与 `docs/contracts/v6/frozen/astrocs.v6.contract-freeze.v1.json` 同源；语义源 = `reports/v6/science-adjudication/adjudications.json` + W3 各规格；基线 HEAD = `ebefe00d3cb9018d61b7b3e8d3d7694191c1f333`。

## 1. 需负责人签字项（PENDING_OWNER_SIGNOFF，不得写成已冻结）

| ID | 事项 | 理由 | owner | 权威 | 状态 |
|---|---|---|---|---|---|
| SO-01 | F-OBS-01 DRIZZLE §3 术语/单位修正 | FROZEN SCI 术语修正 | 项目负责人 + CONTRACT-FREEZE-001 | 宪章 §1.2 | PENDING_OWNER_SIGNOFF |
| SO-02 | F-OBS-02 / S2 面亮度归一改为 (B) 面亮度保持 | FROZEN DRIZZLE.md §5/§7/§11 公式变更 + 重跑全部 Drizzle 不变量/variance 门 | 项目负责人 + CONTRACT-FREEZE-001 | 宪章 §1.2 | PENDING_OWNER_SIGNOFF |
| SO-03 | S3 常量场 Oracle 判据取代 DRIZZLE §11 | FROZEN 判据取代登记 | 项目负责人 + CONTRACT-FREEZE-001 | 宪章 §1.2 | PENDING_OWNER_SIGNOFF |
| SO-04 | AR-030/AR-031 协方差产品非目标声明取代 | FROZEN DRIZZLE §1/§9a 取代登记 | 项目负责人 + CONTRACT-FREEZE-001 | 宪章 §1.2 | PENDING_OWNER_SIGNOFF |
| SO-05 | AR-036/AR-019/AR-026 宪章 §10.5/§17.6 记录/裁决分离 | 宪章修订须负责人签字；不得由 Agent 放宽 | 项目负责人 | 宪章 §1.2/§18 | PENDING_OWNER_SIGNOFF |
| SO-06 | AR-032 非 v6 SCI 迁移/取代清单 | 跨文档权威裁定与 DOC-CONVERGE 收口 | 项目负责人 + CONTRACT-FREEZE-001 + DOC-CONVERGE-001 | PROJECT_SPEC §11 | PENDING_OWNER_SIGNOFF |
| SO-07 | F-OBS-03/F-OBS-04/F-OBS-05 的数值阈值/数据面/标定 | 误差门数值、低秩数据面、k_corr 标定脚本须 W3/W4 冻结并由负责人确认 | ALG-P1-001/ALG-P2-UPM-001/CONTRACT-FREEZE-001 + 负责人 | CSI 冻结流程 | PENDING_OWNER_SIGNOFF |

## 2. 控制器级事项（只登记不裁决）

| ID | 事项 | owner | 状态 |
|---|---|---|---|
| CTRL-F1 | F1 工作树不等于 HEAD (16 回退 + 10 删除) | 控制器 | registered_only |
| CTRL-AR033 | 根构建面 (CMakeLists) 无 V6 owner | 控制器 | registered_only |
| CTRL-AR034 | 7 项历史 CI 红 | 控制器/DOC-CONVERGE-001/RUNTIME-CI-001 | registered_only |
| CTRL-AR035 | 785 缺陷账本无销账任务 | 控制器 | registered_only |
| CTRL-AR036 | 宪章修订签字项 | 项目负责人 | registered_only |

## 3. 被取代的非 v6 SCI 段（AR-032/SO-06，只登记）

| 编号 | 文件 | 段落 | 行 | 取代条款 | 签字 |
|---|---|---|---|---|---|
| SUP-01 | docs/science/DRIZZLE.md | §3 物理量和单位 | 27 | FZ-UNIT-VAR-IN, FZ-UNIT-VAR-SB, FZ-UNIT-IVAR-SB, FZ-BUNIT-SEMANTICS | SO-01 |
| SUP-02 | docs/science/DRIZZLE.md | §5 权重与归一 / §7 不变量 | 38-51,79-84 | FZ-FORMULA-DRIZZLE-SB, FZ-FORMULA-DRIZZLE-VAR, FZ-COND-FLUX-CONSERV | SO-02 |
| SUP-03 | docs/science/DRIZZLE.md | §11 验收门 | 117 | FZ-GATE-CONST-SB | SO-03 |
| SUP-04 | docs/science/DRIZZLE.md | §1 目的与非目标 / §9a 专属问题 | 8,146 | FZ-GATE-PARENT-VAR, FZ-FORMULA-COV-PROP, FZ-PROV-SHARED-SYSTEMATIC | SO-04 |
| SUP-05 | docs/science/CONTROL_WEIGHT_SNR.md | § 像素级 SNR 权重（weight_mode=2） | 66,71 | FZ-MODE-PRODUCTION, FZ-FIELD-WEIGHTMODE, FZ-GATE-SUPPORT-COVERAGE, FZ-GATE-MEDIAN-SNR | SO-06 |
| SUP-06 | docs/science/ACR_EQUIVALENCE.md | §4 GPU 合同 | 33 | FZ-FIELD-WEIGHTMODE, FZ-MODE-PRODUCTION | SO-06 |
| SUP-07 | docs/science/INTEGRATION.md | §1 目的与非目标 | 8 | FZ-FORMULA-COV-PROP, FZ-GATE-PARENT-VAR | SO-04/SO-06 |
| SUP-08 | docs/science/CALIBRATION.md | §1 目的与非目标 / §9a 专属问题 | 5,91,95 | FZ-PROV-SHARED-SYSTEMATIC, FZ-FORMULA-COV-PROP | SO-06 |
| SUP-09 | docs/science/PHASE3_HIPS_TO_FITS.md | §1 范围与非目标 / §3 输出投影 | 20,100 | FZ-P3-MODES, FZ-P3-FAILCLOSED, FZ-UNIT-IVAR-SB | SO-06 |
| SUP-10 | docs/science/UNCERTAINTY_AND_COVARIANCE.md | §V19R3 control estimator 方差 | 21,30-48 | FZ-PROV-KCORR, FZ-GATE-PARENT-VAR | SO-06/SO-07 |

完整事实与建议措辞见 `reports/v6/contract-review/03_SUPERSEDED_SCI_SECTIONS.md`。本包不得改写非 v6 `docs/science/*.md`。
