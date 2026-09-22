# GAP_AUDIT — RELEASE-05 差距审计

来源：RELEASE-04 ACCEPTANCE 登记偏差 D-1..D-14、负责人本轮裁决、最高设计新版要求。文件/行以执行时 main 为准复核。

## G0. 架构（最高设计 §8 重写，负责人已裁决路线）

| ID | 差距 | 任务 |
|---|---|---|
| G0-1（D-1） | 阶段内命名块内存管线未在生产执行：20 个生产节点仅 1 个触命名块 API 且节点内自建自毁；无相邻节点通过帧传块；RunContext 无帧成员 | ARCH-501/505 |
| G0-2 | 三阶段无独立调度器：现行整阶段 Session（lib/phase1_session、phase2_session、phase3_session）承担编排；目标态=三进程三调度器（normalize 异步工作流/mosaic 窗口并行/export 子块流式） | ARCH-502/503/504/505 |
| G0-3（D-2 B1） | pipeline/orchestrator 50 文件 18411 行未接入生产；命名块机制落地后按数值等价结果决定接入或删除 | ARCH-501/505、CLEAN-501 |
| G0-4 | 块生命周期未实现：raw/calibrated 等中间面无显式消耗，峰值内存不受在途块控制 | ARCH-501 |
| G0-5 | 性能探针不成体系：编排优化需节点墙钟/排队/块生命周期/RSS/worker 均衡/缓存命中事件流 | ARCH-502、PERF-501 |
| G0-6 | docs/owner/PIPELINE_OVERVIEW.md、ARCHITECTURE_OVERVIEW.md 描述现行 Session 架构，架构落地后随代码同提交重写 | ARCH-505（文档同提交） |

## G1. 科学正确性（创新点闭环）

| ID | 差距 | 任务 |
|---|---|---|
| G1-1（D-4 D1，FIX-407） | σ_sky 在经验总 rms 口径下与 (RN/g)² 双计读出噪声，σ_F 高估 +12.8%~+36.6%；噪声项口径需在科学文档冻结（天光+暗流散粒 vs 经验总 rms，二选一不双计） | DOC-502、SCI-501 |
| G1-2（D-4 D2） | EXP-205 常数 c_est=1.44 被当 dex 阈值，实际是相对标准误（差 ln10 倍），NOISE_MODEL 文档与实验口径订正 | DOC-502 |
| G1-3（SCI-C FIX-1） | UPM 绝对容差 tolerance=1e-6 在 ~300e⁻ 尺度永不收敛（300 次迭代 converged=0），改相对/按观测尺度归一 | SCI-502 |
| G1-4（SCI-C FIX-2） | out_converged 只有 0/1，缺 max_iter/stalled 档（科学文档要求 0/1/2/3） | SCI-502 |
| G1-5（SCI-C FIX-3） | 天光面正规方程真实样本 κ=3.16e7 近奇异，需粗糙度正则化或节点自适应，provenance 记录条件数 | SCI-502 |
| G1-6（SCI-C 边界） | 参考面表示能力边界（不可表示且沿向相干的 ≲2 节点间距分量，残余接缝 ≈0.8×RMS）需写入 UPM 科学文档并指导节点间距/平滑参数选择 | DOC-502 |
| G1-7（SCI-A N5） | n=5 低星数下 σ_floor 可变负；**现行口径（覆盖本条旧表述）**：星数不构成拒绝条件、不设适用域门槛，星少到 SCI-PHOT-001 §4 求解前提不成立即 NO_DATA 拟合失败（见 `OPEN_QUESTIONS.md` Q-5、`docs/science/PHOTOMETRY.md` §16.5） | DOC-502、SCI-503 |
| G1-8（D-? p1001） | ctest 唯一红 p1001_real_nodes 为产品行为缺陷 | SCI-506 |
| G1-9 | 三个实验单元缺面向负责人的论文式精读报告 | SCI-503/504/505 |

## G2. 门禁、测试与合同（B 线）

| ID | 差距 | 任务 |
|---|---|---|
| G2-1（D-10） | L2 性能门恒真：frozen_gate 四条判据全违规仍 verdict=pass（record_and_justify）；worker_balance utilization_pct 恒 50.00% | GATE-501 |
| G2-2（D-12） | requires_monitor 八声明六不生效（落 else PASS）；mutates_workspace 只是跳过 git 对比；语义需二选一落地（真强制 fail-closed 或改名 monitor_capable） | GATE-501、CONTRACT-501 |
| G2-3（D-14） | test_isa_variants.py:79 输出陈旧路径 artifacts/prerelease_v5（无人读），已跟踪证据在 artifacts/evidence/prerelease-v5；pack_audit_package.py:30 同源排查 | GATE-501 |
| G2-4（D-8） | eng/tests/api 5 条断言 CLI_PROTOCOL_V1 中不存在的措辞；eng/tests/config CFG002-04 依赖失效的 bader r 反例 | GATE-502 |
| G2-5（D-8/BLD） | linux-main profile 15 条内容面红未处置 | GATE-502 |
| G2-6（D-6） | FIX-404 66 条路径缺口 + 72 条能力缺口无归属任务：甄别为"待实现（立 A 线任务或登记 DEFERRED）"或"声明作废（清注册表）" | CLEAN-501、GATE-502 |
| G2-7 | 空断言/恒真测试普查（RELEASE-04 已发现 io_adapter 空断言一例），建立"断言必须有判别力"检查 | GATE-502 |
| G2-8 | docs/contracts（合同说明）与 eng/contracts（schema）双向对应无机器校验；双线文件域、块生命周期、调度器接口、性能判据、监控字段语义需先入合同 | CONTRACT-501、GATE-501 |
| G2-9（D-13 续） | CI 增量/并行化未完成项：并行 runner 等价性验证、g++ 预编译、SECRET-HYGIENE 并发读、RESOURCE-GATE 合并窗口、构建图反查、指纹缓存 | GATE-501 |

## G3. 代码整洁（A 线）

| ID | 差距 | 任务 |
|---|---|---|
| G3-1（D-2 B2/B3/B4/B5） | p3_v6_export.* + v6_p3 集成测试、v1 p3_projection.*、healpix_drizzle shim、psfsw/weight_mode 残留面（contracts 中 forbidden.weight_source_tokens/weight_modes 两段需退役） | ARCH-505、CLEAN-501、DOC-502 |
| G3-2（D-3） | CLEAN-402 12 组保留候选（注册表锁定的无引用文件）随注册表退役分批删除 | CLEAN-501 |
| G3-3（D-11） | 性能归因更新：cfitsio 锁非瓶颈（撤锁 339.5s/注回 333.5s）；真实瓶颈=阶段串行段 + 约 3.9GB/帧读放大；16 worker 仅 2.37×、均值 3.92/16 核 | PERF-501 |
| G3-4（D-5） | Windows CTRL_CLOSE 退出码 9 平台限制，发布前 Windows 节点复核 | BLD-501 登记 |

## G4. 验收与发布

| ID | 差距 | 任务 |
|---|---|---|
| G4-1 | 五一致性（科学-文档-合同-代码-测试）无独立验收；实现者自审不够，设独立验收与自迭代 | ACCEPT-501 |
| G4-2 | E2E/L4 本轮未跑（负责人裁决）；下包必跑：预检矩阵、三命令串行、M42+银心 R 通道成品帧与切块目检 | E2E-501、VIS-501 |
| G4-3 | 无法裁决项需成文（审核包 OPEN_QUESTIONS）随最终报告交付 | REPORT-501 |
| G4-4 | Windows/MSVC 腿未真跑（D-9），发布前补 | BLD-501 |
| G4-5 | FIN（README、0.0.1alpha、收口）仍待负责人认可成品帧后执行 | REPORT-501 之后 |

## 本包不做

GUI/HiPS Browser、GPU/ACR 生产化、ARM、R 以外通道、安装器。
