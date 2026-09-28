> 由 `reports/v6/contract-review/tools/gen_freeze.py` 机械渲染，与 `docs/contracts/v6/frozen/astrocs.v6.contract-freeze.v1.json` 同源；语义源 = `reports/v6/science-adjudication/adjudications.json` + W3 各规格；基线 HEAD = `ebefe00d3cb9018d61b7b3e8d3d7694191c1f333`。

## 1. 开放项总表（DI / OI / PF / P3 / AR 缺口）

| ID | 主题 | 类别 | owner | 状态 |
|---|---|---|---|---|
| DI-01 | schema 词表单一化（SCI-PSFW 词表 vs SCI-P2 词表 → 单一 schema） | open_design | SCHEMA-INTEGRATE-001(W6) | OPEN |
| DI-02 | 数值阈值：surface_gls epsilon / HiPS deficit / PSFSW 指数与归一常数 / 共同星集深度稳定性阈值 | threshold_pending | ALG-P2-SURF-001 / ALG-P1-001 / ALG-P2-PSFSW-001 (W3) + 负责人 | OPEN |
| DI-03 | shared systematic 的低秩/相关核数据面实例化 | data_face | ALG-P1-001 / IMPL-P1-CAL-001 / SCHEMA-INTEGRATE-001 | OPEN |
| DI-04 | k_corr 标定脚本 + 固定种子 MC 复跑 | calibration | ALG-P2-UPM-001 / IMPL-P2-UPM-001 + 负责人 | OPEN |
| DI-05 | AR-048 参数生效证明（参数被记录 ≠ 生效） | runtime_gate | DATA-DESIGN-001(G-PARAMETER-EFFECTIVENESS) / RUNTIME-CI-001 | OPEN |
| DI-06 | 生产 schema plane 枚举扩展与 runtime validator 同一提交（F-UNC-003） | schema_runtime | SCHEMA-INTEGRATE-001(W6) / IMPL-AIO-001 | OPEN |
| DI-07 | weight_units 字面量 "1"(冻结) vs dimensionless_relative(SCI-P2 R5) 的 W6 双射落定 | vocabulary | SCHEMA-INTEGRATE-001(W6) | OPEN |
| OI-01 | Phase1 单帧无法形成帧组：四分量+未归一 Wt+归一契约归 Phase1，组内 median=1 归 Phase2；接口拆分须 W4 批准 | interface_ratification | CONTRACT-FREEZE-001(W4) | OPEN |
| OI-02 | DESIGN-P1 §4.2 的 V_b + α²V_b 与同 master_id 的 (1−α)² 合并关系 | finding | CONTRACT-FREEZE-001(W4)/负责人 | OPEN |
| OI-03 | SCI-CAL-001 §9a/§1『不传播 variance / 不建模 gain-readnoise』与 V6 目标冲突（取代登记） | supersede_registration | CONTRACT-FREEZE-001(W4) SO-06 | OPEN |
| OI-04 | docs/**/v6/** 未登记进 docs/DOCUMENT_INDEX.yaml；check_doc_index.py docs_fully_covered 现为红 | coverage_gap | DOC-CONVERGE-001(W12)/控制器 | OPEN |
| OI-05 | common star set n_common 下限、selection bias 深度稳定性阈值、四分量非均匀拆 tile 阈值未冻结 | threshold_pending | ALG-P2-PSFSW-001(W3)/CONTRACT-FREEZE-001(W4) SO-07 | OPEN |
| OPEN-P2S-01 | point_source 产品规范输出形态（map vs statistic） | domain_open | ALG-P2-POINT-001/控制器 | OPEN |
| OPEN-P2S-02 | UPM 乘法尺度 g_k 的生产数据面/schema 与空间模型表示 | data_face | DATA-DESIGN-001/SCHEMA-INTEGRATE-001 | OPEN |
| OPEN-P2S-03 | 跨帧完整联合 C 的低秩/相关核表示 | data_face | DATA-DESIGN-001/CONTRACT-FREEZE-001 | OPEN |
| P3-OPEN-EPSILON-CORR | Phase3 相关核近似误差阈值 epsilon_corr 具体数值 | threshold_pending | CONTRACT-FREEZE-001(W4)+负责人(SO-07) | OPEN |
| PF-01 | G-INJ-01 注入源 flux bias 容差（设计默认 2% 未冻结） | threshold_pending | ALG-P2-POINT-001 | OPEN |
| PF-02 | G-INJ-02 扩展源三量一致性容差（设计默认 3% 未冻结） | threshold_pending | ALG-P2-SURF-001 | OPEN |
| PF-03 | G-INJ-03 W_info 注入恢复容差（建议 5% 未冻结） | threshold_pending | ALG-P2-PSFSW-001 | OPEN |
| PF-04 | G-INJ-07 Phase3 flux 恢复容差（设计默认 2% 未冻结） | threshold_pending | ALG-P3-001 | OPEN |
| PF-05 | G-RD-01 真实数据 M42 接缝/噪声清单门（W10 预注册冻结） | threshold_pending | REAL-SCIENCE-001 | OPEN |
| PF-06 | G-RD-02 真实数据银心清单门（W10 预注册冻结） | threshold_pending | REAL-SCIENCE-001 | OPEN |
| PF-07 | G-BASE-03 psfsw 基线比较效应量与 CI（预注册冻结） | threshold_pending | ALG-P2-PSFSW-001/预注册 | OPEN |
| AR-032-GAP | 非 v6 docs/science/*.md 在 W1–W8 无 owner；正式取代归 DOC-CONVERGE-001 | coverage_gap | DOC-CONVERGE-001(W12)/SO-06 | OPEN |
| AR-034-GAP | 7 项历史 CI 红无 V6 逐名验收锚（建议 RUNTIME-CI-001 逐名登记终态） | ci_gap | 控制器/RUNTIME-CI-001 | OPEN |
| AR-035-GAP | 785 合并层缺陷账本无销账任务（只登记，处置权在控制器/负责人） | ledger_gap | 控制器/FINAL-AUDIT-001 | OPEN |
| AR-036-SIGNOFF | 宪章 §10.5/§17.6『记录 vs 自动判决』需负责人签字（V6 无承载） | governance_signoff | 项目负责人 | OPEN |

## 2. pending_freeze 7 条（度量/门冻结，数值未冻）

| 门 | 度量 | 设计默认值 | owner | 条款 id |
|---|---|---|---|---|
| G-INJ-01 | `|bias|/F_ref` | 0.02 | ALG-P2-POINT-001 | QF-G-INJ-01 |
| G-INJ-02 | 三量 max 偏差 | 0.03 | ALG-P2-SURF-001 | QF-G-INJ-02 |
| G-INJ-03 | `|W_info-W_info_ref|/W_info_ref` | 0.05 | ALG-P2-PSFSW-001 | QF-G-INJ-03 |
| G-INJ-07 | `|flux_rec/F_inj-1|` | 0.02 | ALG-P3-001 | QF-G-INJ-07 |
| G-RD-01 | M42 接缝/噪声清单失败数 | 0 | REAL-SCIENCE-001 | QF-G-RD-01 |
| G-RD-02 | 银心清单失败数 | 0 | REAL-SCIENCE-001 | QF-G-RD-02 |
| G-BASE-03 | psfsw 基线效应量 | >0 | ALG-P2-PSFSW-001/预注册 | QF-G-BASE-03 |

## 3. 需负责人签字（保持待签，不得写成已冻结）

| ID | 事项 | owner | 状态 |
|---|---|---|---|
| SO-01 | F-OBS-01 DRIZZLE §3 术语/单位修正 | 项目负责人 + CONTRACT-FREEZE-001 | PENDING_OWNER_SIGNOFF |
| SO-02 | F-OBS-02 / S2 面亮度归一改为 (B) 面亮度保持 | 项目负责人 + CONTRACT-FREEZE-001 | PENDING_OWNER_SIGNOFF |
| SO-03 | S3 常量场 Oracle 判据取代 DRIZZLE §11 | 项目负责人 + CONTRACT-FREEZE-001 | PENDING_OWNER_SIGNOFF |
| SO-04 | AR-030/AR-031 协方差产品非目标声明取代 | 项目负责人 + CONTRACT-FREEZE-001 | PENDING_OWNER_SIGNOFF |
| SO-05 | AR-036/AR-019/AR-026 宪章 §10.5/§17.6 记录/裁决分离 | 项目负责人 | PENDING_OWNER_SIGNOFF |
| SO-06 | AR-032 非 v6 SCI 迁移/取代清单 | 项目负责人 + CONTRACT-FREEZE-001 + DOC-CONVERGE-001 | PENDING_OWNER_SIGNOFF |
| SO-07 | F-OBS-03/F-OBS-04/F-OBS-05 的数值阈值/数据面/标定 | ALG-P1-001/ALG-P2-UPM-001/CONTRACT-FREEZE-001 + 负责人 | PENDING_OWNER_SIGNOFF |

## 4. 控制器级事项（只登记不裁决）

| ID | 事项 | owner | 状态 |
|---|---|---|---|
| CTRL-F1 | F1 工作树不等于 HEAD (16 回退 + 10 删除) | 控制器 | registered_only |
| CTRL-AR033 | 根构建面 (CMakeLists) 无 V6 owner | 控制器 | registered_only |
| CTRL-AR034 | 7 项历史 CI 红 | 控制器/DOC-CONVERGE-001/RUNTIME-CI-001 | registered_only |
| CTRL-AR035 | 785 缺陷账本无销账任务 | 控制器 | registered_only |
| CTRL-AR036 | 宪章修订签字项 | 项目负责人 | registered_only |
