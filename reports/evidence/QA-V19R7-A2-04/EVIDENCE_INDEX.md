# EVIDENCE_INDEX — QA-V19R7-A2-04 (phase2 域)

> 任务: `工程控制/tasks/QA-V19R7-QUALITY-OPTIMIZATION.md` A2-04 | Spec: `工程控制/docs/29_QUALITY_OPTIMIZATION_V19R7_SPEC.md`  
> 审计分表: `reports/v19r7_quality/audit_findings_phase2.md` | 基线: V19R6R2-W1 | 只读

## 1. 输入

- Spec/Task: `工程控制/docs/29_QUALITY_OPTIMIZATION_V19R7_SPEC.md`, `工程控制/tasks/QA-V19R7-QUALITY-OPTIMIZATION.md`
- 科学: `docs/science/PHASE2_UPM.md` (SCI-UPM-*-010, SCI-UPM-PERSIST-001, SCI-UPM-WEIGHT-001), `docs/science/INTEGRATION.md` (SCI-INT-001/002/004/008), `docs/science/REJECTION.md` (SCI-REJ-*)
- 算法: `docs/algorithms/UPM_SOLVER.md` (ALG-UPM-FRAME-BIND-001), `docs/algorithms/PHASE2_SAMPLER.md` (ALG-P2-SAMPLE-*), `docs/algorithms/REJECTION_ALGORITHMS.md` (ALG-REJ-001..008), `docs/algorithms/INTEGRATION_ALGORITHMS.md` (ALG-INTEGRATE-*)
- 合同: `docs/contracts/DATA_SEMANTICS.md:5 DATA-FRAME-ID-001`
- 追溯: `docs/TRACEABILITY.csv` 31 行 phase2 (SCI-UPM 18 + ALG/DATA 13), `status=VERIFIED, broken=0` (初扫)
- 代码: `lib/phase2/src/upm.cpp:61,1096-1148` `sampler.cpp:250-364,515,669` `integrate.h:30-50` `integrate.cpp:23-71` `rejection.h:73-88` `rejection.cpp` `stage2_common.cpp` `coverage.cpp:110-115` `block.cpp` `acr_kernels.cpp` `lib/common/crypto/sha256.*`
- 测试: `lib/phase2/tests/synthetic_gate.cpp` (UPMW-001..007, PR-UPM-001..010)
- 机器初扫: `reports/v19r7_quality/machine_consistency_before.json` (9 checks 0 broken, 待复核)

## 2. 产出

- 本审计分表: `reports/v19r7_quality/audit_findings_phase2.md` (P0 0 / P1 3 / P2 5)
- 证据索引: 本文件

## 3. 方法 (只读)

L1↔L2↔TRACE↔CODE 全链：权重 `quality×geom×ivar`/`control_ivar`/`control_k_corr`/per-frame provenance、`frame_id` SHA-256 绑定持久化、排异归一化/large-scale、积分状态机 `P2_INTEGRATE_*` 全集合 `name+value` 对照、support reducer max。

## 4. 关键证据摘录

- **PH2-01 P1 权重**: `upm.cpp:1096 raw=qf*civ (civ<=0→2)`, `upm.cpp:1129-1147 normalized=raw/sum*reliability`, `sampler.cpp:515 var=k_corr·π/2·σ²/N_retained`, 文档主公式未标 raw→normalized 分层。
- **PH2-02 P1 绑定**: `upm.cpp:61 frame_id_by_index`, `327-332 有序插入`, `747 save 门禁`, `868-888 open 强校验+重复拒绝`, `sampler.cpp:250 p2_frame_id payload=properties白名单+signal/support f32 LE bytes+SNR catalogue → sha256_hex 前16hex`, `DATA_SEMANTICS.md:5 truncated-64`。
- **PH2-03 P1 k_corr**: `sampler.h:53 默认1.4`, `sampler.cpp:426 <=0回退1.4`, `672 per-frame kcorr>0?per-frame:cfg`, 文档未写 per-frame 优先级。
- **PH2-05 P2 积分**: `integrate.h:46-50 5态`, `integrate.cpp:48 w==0合法`, `65 ZERO_VALID_WEIGHT vs ALL_REJECTED`, weight_mode 已删 policy/reducer 分离。
- **PH2-08 P2 双 frame_id 口径**: `coverage.h:33 frame_id[64] basename` (覆盖层 string) vs `sampler.h:83 uint64 SHA-256` (科学层 payload hash)，文档未并表。

## 5. 缺口标注

- A1 machine_consistency 待 `tools/docs_machine_consistency.py` 复核后进 B5 (当前 0 broken 为初扫快照)。
- 无 P0 阻断；P1 三项仅文档口径收口，无需改语义 (冻结红线)。

## 6. 下步

B1-08/B2-09/10 补权重分层 + payload LE 注 + per-frame k_corr 句；B5 `DATA_SEMANTICS` 加 coverage vs payload frame_id 对照表；代码 0 改动。

---
*只读审计，未改 `docs/lib`。*
