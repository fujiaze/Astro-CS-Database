# ACCEPTANCE — RELEASE-05 执行状态登记（执行 agent 维护）

## 任务状态

| ID | 状态 | 提交/证据指针 | 偏差登记 |
|---|---|---|---|
| DOC-501 | PASS | 3f5265ff、bf1bc15b、4e8d476d；门 DOC-INDEX/DOC-INDEX-SELFTEST/CHK-STALE-DOC/DOC-L0/CHK-REGISTRY-DOC-SYNC 全 PASS；六份 cmp 为空 | 无 |
| DOC-502 | PASS | b85c79dd；7 条结论各有文档落点（07_noise_snr §4.2a、NOISE_MODEL §5a/§9a、PHASE2_UPM §5/§7a/§16.3、PHOTOMETRY §16.5、CONTROL_WEIGHT_SNR §8b、PSF_SIGNAL_WEIGHT §7a、合同 frozen 段）；doc-index PASS | 独立子代理审稿未做（见偏差 D-5） |
| SCI-501 | PASS | cdcc0975；保护测试 `p1snr_science_skysource`：正确口径 zA=1.27(≤3σ) 绿、双计臂 zB=25.6(>3σ) 红、legacy 逐位一致；ctest -R 'p1snr|p1noise|p1001' 全绿（run/RELEASE-05/logs/sci501_ctest.log） | 生产 C++↔Python 对拍 2.24e-14 量级未复跑（见偏差 D-6） |
| SCI-502 | PASS | d77fd11f；FIX-1 相对容差（scale_obs 观测尺度，非 max|M|）、FIX-2 四态 0/1/2/3 + stalled、FIX-3 κ 自适应 ×10 ≤6 次 + provenance；ctest -R 'p2|upm|sky|UPM' 59/59 PASS（run/RELEASE-05/logs/sci502_ctest.log） | M42 真实样本"有限迭代 converged=1 迭代数与残差表"未单独复跑（见偏差 D-7） |
| SCI-506 | PASS | 7716e16a；根因=测试期望过时（CLEAN-401 入口 fail-closed 合同），非产品缺陷；改有限尖峰 + 新增 NaN fail-closed 负例；p1001 RC=0（run/RELEASE-05/logs/p1001_fix2.log） | 无 |
| SCI-503 | RUNNING | 实验/photometric-magnitude/REPORT_paper.md 已产出（子代理 03df4379），待复核 | |
| SCI-504 | RUNNING | 子代理 608370fb | |
| SCI-505 | RUNNING | 子代理 bc90ae56 | |
| CONTRACT-501 | PASS | ff07b958；四份合同 + 五份 schema + 双向校验器（--self-test 4/4 红绿双向）；doc-index PASS | 校验器注册进 eng/ci/checks.json 属 B 线域，请求 GATE-501 登记（偏差 D-8） |
| ARCH-501 | NOT_RUN | | |
| ARCH-502 | NOT_RUN | | |
| ARCH-503 | NOT_RUN | | |
| ARCH-504 | NOT_RUN | | |
| ARCH-505 | NOT_RUN | | |
| CLEAN-501 | NOT_RUN | | |
| GATE-501 | RUNNING | 子代理 c97566c1 | |
| GATE-502 | RUNNING | 子代理 367515a2 | |
| PERF-501 | NOT_RUN | | |
| ACCEPT-501 | NOT_RUN | | |
| BLD-501 | NOT_RUN | | |
| E2E-501 | NOT_RUN | | |
| VIS-501 | NOT_RUN | | |
| REPORT-501 | NOT_RUN | | |

状态取值：NOT_RUN / RUNNING / PASS / PARTIAL / BLOCKED / NOT_RUN(负责人裁决)。

## 双线文件域冲突登记

| 时间 | 线 | 触及共享面 | 前台裁决 |
|---|---|---|---|
| RELEASE-05 开工 | 前台 | `eng/tools/doccheck/check_doc_index.py`（B 线域） | 现行控制包切 RELEASE-05 属控制包登记动作，前台直接改并单独提交（d8495a65）；RELEASE-04 入归档白名单（需自带 SUMMARY.md 收口证明，缺证明仍判红） |
| RELEASE-05 开工 | 前台 | `lib/algorithms/noise_snr/tests/p1noise/CMakeLists.txt`（eng/tests 域） | SCI-501 需新增 ctest 组，按 TASK_LIST 脚注"只改对应科学断言的测试文件"处理，本表登记 |

## 偏差与开放问题

- **D-1（架构线未启动）**：ARCH-501..505 / CLEAN-501 / PERF-501 未开始。目标态（三进程三调度器 + 命名块生命周期）**尚未在生产路径执行**；本包当前只完成阶段 1 科学闭环与 CONTRACT-501。这是本包最大的未完成面，如实登记，不冒充完成。
- **D-2（收尾线未启动）**：ACCEPT-501 / BLD-501 / E2E-501 / VIS-501 / REPORT-501 未开始。
- **D-3（RELEASE-04 物理出库）**：CONTROL_PACK_SPEC §9 要求收口即清理，但 RELEASE-04 是负责人交付物（AGENTS.md §10 禁止未经询问删除交付物）⇒ 暂以"归档白名单 + 必须自带 SUMMARY.md"方式保留并保持门禁有牙；**是否物理出库请负责人裁决**。
- **D-4（规范冲突，已按仓内权威处理）**：控制包 `tasks/SCI-502.md` 写"converged 0/1/2/3 = 收敛/未收敛/达 max_iter/stalled"，而仓内权威 `docs/plugins/algorithms_phase2/11_upm.md` §4.6 与 `docs/algorithms/PHASE2_UPM_IMPL.md` §496 均定义 `0=max_iter / 1=converged / 2=stalled / 3=invalid`。按 AGENTS.md §1.1「规范在别的文档去那一份读」采用仓内权威定义。
- **D-5（DOC-502 独立审稿缺失）**：DOC-502 验收门要求"独立子代理审稿：文档表述与实验 results/JSON 数值一致"，本会话未执行，登记为未完成项。
- **D-6（SCI-501 对拍缺失）**：验收门"生产 C++ 与 Python 镜像对拍恢复 2.24e-14 量级"未复跑；本次以 C++ 内置独立 long-double 参考 + 独立 Monte Carlo（zA=1.27）替代佐证，Python 镜像对拍登记为未完成项。
- **D-7（SCI-502 真实样本复跑缺失）**：验收门"M42 真实样本有限迭代 converged=1（给迭代数与残差表）"未单独复跑；κ=3.16e7 取自 SCI-C 已归档结果（`实验/additive-sky-seamless/results/c7_realdata.json`），本次修复后未重跑该真实样本。
- **D-8（新检查器注册）**：`eng/tools/contract_doc_sync.py` 需注册进 `eng/ci/checks.json`（B 线域）——请求 GATE-501 处置。
- **D-9（`artifacts/prerelease_v5/`）**：未跟踪陈旧输出目录，已转 GATE-501 清理。

## 发布门核对（收尾填写）

- [ ] 三篇论文式报告交付
- [ ] 五一致性独立验收通过
- [ ] fast + 全量 + Windows 全绿、零 waiver
- [ ] 两组成品帧 agent 自验通过、负责人目检认可
- [ ] OPEN_QUESTIONS 已交付
- [ ] FIN（README、0.0.1alpha）经负责人明确授权
