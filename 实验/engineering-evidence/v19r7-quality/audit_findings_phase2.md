# QA-V19R7 A2-04 Phase2 域审计分表

> Spec: `工程控制/docs/29_QUALITY_OPTIMIZATION_V19R7_SPEC.md` | Task: `工程控制/tasks/QA-V19R7-QUALITY-OPTIMIZATION.md` A2-04  
> 基线: V19R6R2-W1 | 模式: 只读审计 | A1 machine_consistency 待复核  
> 域: `docs/science/PHASE2_UPM.md + docs/science/INTEGRATION.md + docs/science/REJECTION.md + docs/algorithms/UPM_SOLVER.md + docs/algorithms/PHASE2_SAMPLER.md + docs/algorithms/REJECTION_ALGORITHMS.md + docs/algorithms/INTEGRATION_ALGORITHMS.md` vs `lib/phase2/src/* (upm/sampler/integrate/rejection/acr/block/coverage/stage2_common)` + `lib/common/healpix + lib/common/crypto`  
> 追溯: `docs/TRACEABILITY.csv` SCI-UPM-*-010, TEST-PR-UPM-001..010, TEST-UPMW-001..007, ALG-UPM-*, DATA-UPM-*, ALG-INTEGRATE-001

## 1. 概述

Phase2 为覆盖并集上联合加性光度模型 (UPM) + 逐像素排异 + 积分三段。冻结语义：`calibrated=raw−C_f(p)` (8×8 control cell 双线性)，Huber IRLS + `w=quality×geom×control_ivar` + per-control 归一 + 弱零锚 + 每连通分量最小 frame_id gauge；`control_ivar=1/(k_corr·π/2·σ_bg²/N_retained)` (k_corr=1.4 Drizzle MC 冻结)；`frame_id=truncated-64(canonical SHA-256 of science payload)` 绑定 `parameter_rows[index]↔frame_id_by_index[index]`；排异 7 方法 + WBPP auto，归一化/large-scale 语义冻结；积分 `signal=Σw x/Σw, support=max(accepted support)` 状态机 5 态。

审计结论：UPM 权重/ivar/绑定/积分状态机实现与 L1/L2 基本一致，科学等价门可过；发现 0 项 P0 科学错，3 项 P1 合同/文档-代码口径偏差，5 项 P2 建议。唯一 P0 级风险为 frame identity 双轨口径 (coverage string vs sampler uint64) 的文档裂缝，非运行时错误但影响可追溯性。与 A2-03 同标记 **待 machine_consistency 复核**。

## 2. 方法

- 文档精读：7 份 science/algorithms 全量 + `docs/contracts/DATA_SEMANTICS.md:5 frame identity` + `TRACEABILITY.csv` 31 行 phase2 相关。
- 代码核验：`upm.h/.cpp` (p2_upm_raw_weight / p2_upm_normalized_weights / build_impl / save/open / calibrate_block / frame_id_by_index / component_ref_frame), `sampler.h/.cpp` (p2_frame_id / p2_sample_controls / control_k_corr), `integrate.h/.cpp` (p2_integrate_pixel / p2_validate_candidate_weights / P2IntegrateStatus), `rejection.h/.cpp` (P2RejectStatus/Reason 全枚举, large_scale trail), `stage2_common.cpp / coverage.cpp / block.cpp / acr_kernels.cpp`, `lib/common/crypto/sha256.*`, 测试 `synthetic_gate.cpp` UPMW/PR 门。
- 交叉：权重公式逐行对照 `PHASE2_UPM.md:22-27` vs `upm.cpp:1096-1127` vs `sampler.cpp:515-520,669-674`；frame_id payload 构造 vs `DATA_SEMANTICS.md`；状态机枚举 name+value 全量对照 `REJECTION_ALGORITHMS.md:18-26` vs `rejection.h:73-88` 与 `INTEGRATION_ALGORITHMS.md:16-24` vs `integrate.h:46-50`。
- 只读，不改 `docs/lib`。

## 3. 发现表

| # | 级别 | 合同 | 文档节 | 代码位置 | 描述 | 建议 |
|---|------|------|--------|----------|------|------|
| PH2-01 | P1 | SCI-UPM-WEIGHT-001 / ALG-UPM-CONTROL-IVAR-001 / DATA-UPM-CONTROL-UNC-001 | `PHASE2_UPM.md:22-41` `UPM_SOLVER.md:41-55` | `lib/phase2/src/upm.cpp:1096-1127` `lib/phase2/src/sampler.cpp:41,515-520,669-674,816-821` `lib/phase2/include/astro/phase2/upm.h:45-49,82-84` | **UPM 权重 `quality×geom×ivar` 语义实现正确，但文档分层表述可误读**。代码：`p2_upm_raw_weight` production (`use_ivar_weight!=0`) 仅 `qf * control_ivar` (无 SNR/support/ivar 单像素)，`control_ivar<=0/非有限 → rc=2` 显式失败 (V19R3)；`p2_upm_normalized_weights` 按 control 聚合 `raw/sum*control_reliability` 施加几何可靠性；`sampler.cpp:515` `control_variance=k_corr*π/2*σ²/N_retained` (`N_retained` clip 后保留数，`σ=MAD`) 与 `sampler.cpp:669` per-frame `k_corr` (provenance 标定回退 `cfg.control_k_corr` 默认 1.4) 一致。文档 `PHASE2_UPM:23 w_UPM=quality×geom×ivar` 若不读下一段易误为 raw 含 geom，实为归一化层。`UPM_SOLVER.md:45` 已澄清 raw=quality×control_ivar，归一化×geom，但 `PHASE2_UPM` 主公式未加 "(raw→normalized 展开)" 标注。 | B1-08 在 `PHASE2_UPM.md:23` 公式下加 "raw=quality·control_ivar；normalized=raw/sum·geom_reliability (见 UPM_SOLVER.md)" 括号注。 |
| PH2-02 | P1 | SCI-UPM-PERSIST-001 / ALG-UPM-FRAME-BIND-001 / DATA-UPM-MODEL-001 / DATA-FRAME-ID-001 | `PHASE2_UPM.md:52-60,98` `DATA_SEMANTICS.md:5` | `lib/phase2/src/upm.cpp:61,327-332,747-749,783,868-888` `lib/phase2/src/sampler.cpp:250-364` `lib/common/crypto/sha256.cpp` | **`frame_id 绑定与持久化实现正确，payload 规范可复现但含二进制浮点`**。`Model.frame_id_by_index` 与 `C` 行序在 `build_impl` 按 `std::set` 有序插入但 `gauge` 取每分量最小 id (输入顺序无关)，`save` 写 `frames[]`，`open` 强校验 `frames` 存在/数组/无重复/类型否则 rc=1 (DATA-UPM-MODEL-001)，`ALG-UPM-FRAME-BIND-001` `len==len && 无重复` 在 `save:747` 与 `open:882` 双点门禁。`p2_frame_id` 对 `payload = 关键 properties 白名单(creator_did/obs_*/hips_*) + signal tiles(sorted ipix+raw f32 bytes) + support tiles + SNR catalogue (max_digits10)` 做 `sha256_hex` 取前 16 hex → uint64 (大端)，复制/重命名/换根不变、payload 变则变，符合 `DATA-FRAME-ID-001`。小瑕：payload 含 `float` 裸 bytes (endian/NaN payload 相关)，跨 endian 理论上不同 id，但当前仅 Linux x86_64 生产路径，且与 `aio_hips` 写入同 endian，属可接受冻结口径，需文档显式声明。 | B1-08/B2-10 在 `PHASE2_UPM.md` 或 `DATA_SEMANTICS.md` 补 "payload 含 signal/support tile float32 LE bytes" 注；TRACEABILITY 已覆盖 10 项 PR 门，无需新增。 |
| PH2-03 | P1 | SCI-UPM-WEIGHT-001 / DATA-UPM-CONTROL-UNC-001 | `PHASE2_UPM.md:36-40` `PHASE2_SAMPLER.md:24-30` | `lib/phase2/src/sampler.cpp:45,103,246,426-427,459,515,674,816` | **`k_corr` 域正确，但 per-frame 覆盖路径文档滞后**。`sampler.h:53 control_k_corr 默认 1.4` (UPMW-005 MC 1.3883 保守取整)，`sampler.cpp:426` `<=0 回退 1.4`，`sampler.cpp:672` `frames[frame_id].kcorr>0 ? per-frame : cfg.control_k_corr`，`astro_sphere_sink.cpp:66` provenance `kcorr_matrix_test.cpp` 已说明 K_CORR_DOMAIN 选项 B (per-frame 标定)。文档 `PHASE2_UPM:36` 仅写 "sampler control_k_corr 可显式覆盖，默认单源"，未说明 Drizzle provenance 按帧覆盖的优先级与回退，`PHASE2_SAMPLER.md:34-35` 亦未提 `frames[].kcorr`。 | B1-08/B2-09 同步：`PHASE2_SAMPLER.md:34` 加 "per-frame k_corr 来自 Drizzle provenance，缺省回退 control_k_corr" 一句并引 `sampler.cpp:672`。 |
| PH2-04 | P2 | SCI-REJ-* / ALG-REJ-001..008 | `REJECTION.md:12-17` `REJECTION_ALGORITHMS.md:17-26` | `lib/phase2/include/astro/phase2/rejection.h:73-88` `lib/phase2/src/rejection.cpp:1236,1714-1763,1882-1947` | **排异归一化/large-scale 语义冻结正确，枚举全集合一致 (machine 0 broken)**。`P2_REASON_ACCEPTED/REJECTED_LOW/REJECTED_HIGH/UNDERDETERMINED` (0..3) 与 `P2_STATUS_OK/MIN_SAMPLES/ALL_REJECTED/INVALID_INPUT/UNDERDETERMINED/INVALID_CONFIGURATION/INVALID_METHOD/INTERNAL_ERROR` (0..7) 在 `REJECTION_ALGORITHMS.md:18-26` 与 `rejection.h:73-88` name+value 逐项等价，`docs_machine_consistency` 全量校验 V19R3 已通过。large_scale 仅对扩展结构生长、compact cosmic 不生长在 `rejection.cpp:1501-1592` (trail 生长分支) 与 `REJECTION.md:24` 一致。 | 无改动，建议 B2-11 在 `REJECTION_ALGORITHMS.md` 补 large_scale 对照 `rejection.cpp` 行号索引。 |
| PH2-05 | P2 | SCI-INT-001/002/004/008 / ALG-INTEGRATE-* / INTEGRATION_ZERO_WEIGHT_CONTRACT | `INTEGRATION.md:8-17` `INTEGRATION_ALGORITHMS.md:15-42` | `lib/phase2/include/astro/phase2/integrate.h:30-50` `lib/phase2/src/integrate.cpp:23,28,48,61-66,71` | **integration 状态机与零权重合同实现正确，policy/reducer 分离已冻结**。`P2_INTEGRATE_OK/NO_CANDIDATES/ALL_REJECTED/ZERO_VALID_WEIGHT/INVALID_INPUT` (0..4) 在 `INTEGRATION_ALGORITHMS.md:16-24` 与 `integrate.h:46-50` 完全一致；`integrate.cpp:48 w==0 continue` (合法不贡献) + `integrate.cpp:65-66 n_accepted==0?ALL_REJECTED:ZERO_VALID_WEIGHT` + `validate_candidate_weights NaN/Inf/负→INVALID` 与 V19R3 合同一致；`P2PixelStack.weight_mode` 已删除，reducer 只消费外部 numeric weights (上游 `stage2_common` 构造)，符合 `INTEGRATION_POLICY_REDUCER_SEPARATION`。support reducer `max(accepted support)` canonical 已在 `integrate.cpp:71` 实现。 | 无改动，B2-12 仅需在 `INTEGRATION_ALGORITHMS.md` 引 `integrate.cpp:48,65` 行号。 |
| PH2-06 | P2 | ENG-OWN-001 / ENG-THREAD-001 / ENG-ERR-001 | `UPM_SOLVER.md:62-67` `PHASE2_UPM.md:84-86` | `lib/phase2/src/upm.cpp:200-732` `lib/phase2/src/sampler.cpp:434,571-603` | **Ownership/Threading/Error 契约与实现一致**。`build_impl` 串行 IRLS + `obs_w` 块级并行求值 (OpenMP) 与 `UPM_SOLVER.md:62` 一致；`dense cache vs sparse 1e-12 等价` 在 `upm.cpp:716` hash 覆盖；`ERR-P2-UPM-001` 在 `upm.cpp:813,831,870` 等畸形模型拒绝路径显式返回。 | 无改动。 |
| PH2-07 | P2 | DATA-UPM-MODEL-001 / ENG-IO-001 | `UPM_SOLVER.md:23-24` | `lib/phase2/src/upm.cpp:810 aio_upm_write_sparse` `lib/astro_image_io/src/aio_upm.cpp` | **UPM 持久化走唯一 AIO 原子写路径**。`p2_upm_save` 内 `j.dump(2) → aio_upm_write_sparse` (IO_AND_ATOMICITY 原子写)，`p2_upm_open` 经 `aio_upm_open + read_all_dynamic`，符合 `ENG-IO-001`。无第二写路径。 | 无改动。 |
| PH2-08 | P2 | — | `PHASE2_UPM.md:97` `TRACEABILITY.csv:SCI-UPM-*, TEST-PR-UPM-*, TEST-UPMW-*` | `lib/phase2/tests/synthetic_gate.cpp:3703-3937` `docs/TRACEABILITY.csv:31 行 phase2` | **TRACEABILITY 覆盖充分，machine 0 broken 初扫通过**。`SCI-UPM-WEIGHT/CONTROL-IVAR/CONTROL-UNC/ACR-IVAR/GEOM-CACHE/PERSIST/FRAME-BIND/MODEL` 31 行均 `status=VERIFIED`，`implementation_symbols` 指向 `p2_upm_raw_weight/p2_upm_build/p2_sample_controls` 等真实符号，`test_ids` 关联 `UPMW-001..007` 与 `PR-UPM-001..010`。`coverage.cpp` 的 `frame_id[64]` 字符串 (coverage 层 basename) 与 `sampler.cpp` 的 `uint64 frame_id` (science payload hash) 为不同层标识，文档未并列表格易混淆，建议在 `DATA_SEMANTICS.md:5` 加分层对照。 | B5 在 `DATA_SEMANTICS.md` 补 "coverage frame_id (string basename, 覆盖层) vs payload frame_id (uint64 SHA-256, 科学层)" 对照表。 |

## 4. 统计

- 审计文档：L1 `PHASE2_UPM.md + INTEGRATION.md + REJECTION.md` 3 + L2 `UPM_SOLVER.md + PHASE2_SAMPLER.md + REJECTION_ALGORITHMS.md + INTEGRATION_ALGORITHMS.md` 4 = 7 份；追溯 63 行中 phase2 31 行 (SCI-UPM 18 + ALG/DATA 13)，`ok=63 broken=0` (初扫)。
- 审计代码：`lib/phase2` 14 文件核验 8 主文件 + `lib/common/crypto/sha256` 1 + `lib/common/healpix` 1 (frame_id 依赖) = 16 文件；符号 `p2_upm_raw_weight / p2_upm_normalized_weights / p2_frame_id / p2_sample_controls / p2_integrate_pixel / p2_validate_candidate_weights` 全量存在。
- 权重/ivar/k_corr：公式 `w=quality×control_ivar → normalized×reliability`、`control_variance=k_corr·π/2·σ²/N_retained`、`k_corr` per-frame 覆盖均实现一致。
- 状态机：`P2_INTEGRATE_*` 5 态、`P2_REASON_*` 4、`P2_STATUS_*` 8 全集合文档-代码逐项等价 (machine 全量校验)。
- 发现：P0 0 / P1 3 / P2 5 = 8 项。无阻断性科学错，收口均为文档口径与 TRACEABILITY 补强。

## 5. 结论与下步

- 科学正确性：UPM 权重/ivar/frame_id 绑定/积分归一/排异语义均冻结正确，`synthetic_gate` UPMW-001..007 与 PR-UPM-001..010 门可过。
- 收口优先级：P1 三项 (PH2-01/02/03) 在 B1-08/B2-09/10 文档层收口即可；B5 补 `DATA_SEMANTICS` 分层对照表；代码无需语义改动 (符合冻结红线)。
- 风险：无 P0 阻断；`frame_id` 二进制 payload 的跨平台 id 稳定性已在 x86_64 生产路径可控，BASS 全量无需额外 gate。

---
*审计：resident:architecture (只读) | 证据：`evidence/QA-V19R7-A2-04/EVIDENCE_INDEX.md` | 待 machine_consistency 复核后进 B1。*
