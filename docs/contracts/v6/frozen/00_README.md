> 上游：ASTROCS_DESIGN.md §3.1（数据对象）、§8.4（模块与 ABI）

> **⚠ 已按 §9.73 A44 作废**：本文件属历史/冻结层。其中「权重模式 / 权重档位 / mode0·mode1·mode2」这一整套概念**不存在**（负责人 2026-09-20 裁决，GAP_AUDIT.md §9.73 A44；ASTROCS_DESIGN.md §2.1）。本文件内容**保持历史原样**、仅作留痕，**不构成现行规范**；权重 = 阶段二按该天球像素对应帧集合**现场算出的派生量**。

> **DOC-001 溯源注记（2026-09-16）**：本文为 V6 产品族冻结/设计档案（上一轮治理产物），因仍被活动合同引用而保留在活动索引；文中 工程控制/旧 V6 控制包（ROOT-007 已删除）/** 等旧控制包路径为该轮任务溯源，该控制包已由 ROOT-007 删除，不作现状引用。文中「宪章 `ASTROCS-CONSTITUTION-001` §x.y」引用同属该轮历史溯源——该宪章（`ASTROCS_PROJECT_CONSTITUTION.md`）已废止（ROOT-007 删除），**不构成现行依据**；现行权威见 `ASTROCS_DESIGN.md` §0 权威链。

> 由 `reports/v6/contract-review/tools/gen_freeze.py` 机械渲染，与 `docs/contracts/v6/frozen/astrocs.v6.contract-freeze.v1.json` 同源；语义源 = `reports/v6/science-adjudication/adjudications.json` + W3 各规格；基线 HEAD = `ebefe00d3cb9018d61b7b3e8d3d7694191c1f333`。

## 0. 权威与用途

- 文档 ID：`CONTRACT-FREEZE-001-FROZEN-DATA`；机器可读冻结表 = `astrocs.v6.contract-freeze.v1.json`（唯一事实源）。
- 覆盖：信号/协方差/PSF/effective PSF/W_info/PSFSW/weight_mode/provenance 的单位、字段、枚举、适用域、fail-closed 与迁移规则。（已按 §9.73 A44 作废：该概念不存在）
- 上游：`docs/contracts/v6/data/**`（DATA-DESIGN-001 设计提案）与 `eng/contracts/proposals/v6/data/**`（schema proposal）；本目录为正式冻结层。
- schema 词表归一**已由** SCHEMA-INTEGRATE-001(W6) 完成（机器事实源 `eng/contracts/data/v6_weight_vocabulary_v1.json`，`status=PRODUCTION_INTEGRATED`；**2026-09-20 订正**，依据 `W6_SCHEMA_INTEGRATION.md:29,38,104`）：本目录只冻结语义与取值域，不写生产 schema。（原文「schema 词表归一**仍归** SCHEMA-INTEGRATE-001(W6)」所述「待归一」状态已结束，保留于此留痕。）

## 1. data 层冻结条款

| 条款 id | 主题 | 冻结值 | 适用域 | 来源锚 | 验证门 | fail-closed 语义 | 负向 mutation | 状态 | 签字 |
|---|---|---|---|---|---|---|---|---|---|
| FZ-BUNIT-SEMANTICS | BUNIT/pixel_area_power | BUNIT 必须量纲可判：显式 px 幂次 或 provenance pixel_semantics=surface_brightness+pixel_area_power=-2 | 全部产品写盘 | ADJ-F-OBS-01; DATA_SEMANTICS §30.4:2475 | 单位不可判 -> unavailable/REJECT | BUNIT=ADU 且无 pixel_semantics=surface_brightness + pixel_area_power=-2 → 单位不可判 → unavailable/REJECT | 删除 provenance 的 pixel_area_power 声明 → rc!=0 | PENDING_OWNER_SIGNOFF | SO-01 |
| FZ-FIELD-PSFSW-4COMP | PSFSW 四分量 | psfsw.signal / psfsw.concentration / psfsw.noise / psfsw.background (measurement_id 互异; p05/p50/p95; 有效覆盖) | Phase1/2 PSFSW | PSF_SIGNAL_WEIGHT §3; PSFW_FREEZE §4.2; ADJ-P2-03 | 四分量塌陷/缺分量即 REJECT | 四分量塌陷为三、缺分量、measurement_id 重复、p05>p50>p95 或 valid_area_fraction∉[0,1] → REJECT | concentration 直接复制 signal（measurement_id 重复）→ rc!=0 | FROZEN | - |
| FZ-FIELD-PSFSW-UNIT | PSFSW 单位/归一语义 | weight_kind=relative_dimensionless; weight_units=1; group_normalized=true; normalization.scope=group | Phase2 psfsw_robust | PSF_SIGNAL_WEIGHT §3; ADJ-P2-03 | units 含 flux^-2/ivar 即 REJECT | weight.units 含 flux^-2/ivar、group_normalized=false、scope=global、median_target≠1 → REJECT | group_normalized=false → rc!=0 | FROZEN | - |
| FZ-GATE-PSFSW-FAILCLOSED | PSFSW fail-closed | unavailable 原因属于 {no_common_star_set, background_nonpositive_undefined_transform, insufficient_valid_stars, selection_bias_gate_failed, spatial_nonuniformity_gate_failed}; valid=false 时 weight_value=null | Phase2 psfsw_robust | PSF_SIGNAL_WEIGHT §3:49; PSFW_FREEZE §5.1; ADJ-P2-03 | 回退 median SNR 即 REJECT | valid=false 时 weight_value 非 null，或 reason 不在 5 项白名单，或回退 median source SNR → REJECT | valid=false 仍写回 median source SNR 值 → rc!=0 | FROZEN | - |
| FZ-GATE-PSFSW-COV | PSFSW covariance 来源 | covariance.method=propagated_from_composite_coefficients; variance_from_weight=false; uses_relative_weight_as_ivar=false | Phase2 psfsw_robust | PSF_SIGNAL_WEIGHT §5; RULINGS.md #5; ADJ-P2-03 | variance=1/W_psfsw 即 REJECT | covariance.method≠propagated_from_composite_coefficients、variance_from_weight=true、uses_relative_weight_as_ivar=true，或命中 psfsw 禁止键 → REJECT | covariance.variance_from="psfsw_robust_weight"，或产物注入 ivar 键 → rc!=0 | FROZEN | - |
| FZ-GATE-PSFSW-EPSF | PSFSW effective PSF | effective_psf_id 非空; 实际组合算子脉冲响应; 归一约定按产品族声明; 只给 FWHM 标量不构成 effective PSF | Phase2 psfsw_robust | PSF_SIGNAL_WEIGHT §5; COVARIANCE_AND_EFFECTIVE_PSF §3; ADJ-P2-03 | 缺 effective PSF 即 REJECT | effective_psf_id 空，或只给 FWHM 标量，或未声明归一约定 → REJECT | effective_psf={"fwhm":4.71} → rc!=0 | FROZEN | - |
| FZ-FIELD-WEIGHTMODE | weight_mode 语义 | 显式三模式; legacy 整数 {0=support x snr^2,1=equal,2=ivar} 被取代; 0 不得进科学权重面; schema 词表归 W6 | Phase2 | ADJ-S1; C-004.3; UNIFIED §11 | schema 词表未归一前不得发明第三套 | legacy 整数 0 进科学权重面、未知模式、或发明第三套词表 → REJECT | weight_mode=0（support×snr²）→ rc!=0 | FROZEN | - |（已按 §9.73 A44 作废：该概念不存在）
| FZ-PROV-MINIMAL-SET | provenance 最小集 | schema/软件 SHA/run ID/输入+配置哈希/单位+pixel_area_power/frame/像素语义/算法 ID/provider/近似+降级原因/归一版本/相关核摘要/flux_conservation_factor/k_corr/时间/输出哈希 | 全部产品 | 宪章 §4.3; UNIFIED §9; DATA_SEMANTICS §30.3; ADJ-GEN-03 | 缺键/单位不可判/unavailable 无原因即 REJECT | provenance 缺最小集键、单位不可判、或 unavailable 无原因 → REJECT | 删除 flux_conservation_factor 或 k_corr 键 → rc!=0 | FROZEN | - |

## 2. 文件索引

| 文件 | 内容 |
|---|---|
| `astrocs.v6.contract-freeze.v1.json` | 机器冻结表（units/modes/forbidden/clauses/signoff/open/superseded/counts） |
| `01_DATA_CONTRACT_FREEZE.md` | 字段/单位/BUNIT/provenance 冻结明细 |
| `02_WEIGHT_MODE_VOCABULARY.md` | weight_mode 语义、禁止项、双词表映射建议 |（已按 §9.73 A44 作废：该概念不存在）
