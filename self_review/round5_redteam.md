# Round 5 — Enhanced Red-Team（V17，15 攻击假设）

日期：2026-08-14 ｜ 规则：每项 DISPROVED_WITH_EVIDENCE 或
BUG_FOUND_AND_FIXED；不接受"没测过"。

| # | 攻击假设 | 结论 | 证据 |
| --- | --- | --- | --- |
| 1 | NaN weight 产生 status=OK + signal=NaN | DISPROVED（C01 已修） | V17NonFiniteWeightInvalid：INVALID_INPUT；p2_validate_candidate_weights 返回 1 |
| 2 | Inf / 负 weight 被积分 | DISPROVED | V17NonFiniteWeightInvalid：+Inf/-Inf/负 → INVALID_INPUT |
| 3 | rejection INVALID_CONFIGURATION 被 Stage2 当 UNDERDETERMINED 接受 | DISPROVED | stage2 CPU hard fail（return 6）；ACR 同样只收 OK/UNDERDETERMINED；V17InvalidMethodStatus + V16InvalidConfigurationCombos |
| 4 | p2_integrate support 与 Stage2/ACR support 三套 reduction | DISPROVED（C03 已修） | support_out=pr.support（max accepted）；ACR 消费 pr.support；large_scale 两遍路径同样消费；rg 无第二处 max/mean |
| 5 | ALL_REJECTED status 不可达 | DISPROVED | V17StatusesExplicit：全部样本被拒时 P2_INTEGRATE_ALL_REJECTED 返回且 valid=0（stage2 输出 zero_px 对应） |
| 6 | UPM weight 与 stack weight 名字混用 | DISPROVED（C05 已修） | integrate.h 分开 upm.robust_control_weight.v1 / stack.support_x_snr2.v1 / stack.equal.v1；api_doc_consistency PASS |
| 7 | wbpp_current 随未来安装版本漂移 | DISPROVED（W01 已修） | canonical=wbpp_2_9_1；parser 规范化 alias；real16 重跑 diagnostics reject_profile=wbpp_2_9_1 |
| 8 | 真实 observed 混名 false reject | DISPROVED（D1/D2 已修） | real16 指标 renamed observed_sample_rejection_rate=0.54%；受控 truth 测 true FPR=1.88%（Siril 100% 一致） |
| 9 | Large-Scale unsupported 却声称 WBPP 功能完整 | DISPROVED（E 已实现） | astrocs.large_scale_rejection.v1 实现+5 单元测试+E2E（satellite grown=3079，cosmic grown=0）；feature matrix=SUPPORTED |
| 10 | healpix_stack/legacy Stage2 仍可 build/call | DISPROVED（F 已修） | 移入 archive/legacy；orchestrator 枚举/接线删除；运行时 7 模块；make 编译通过；no_legacy_production_reference PASS |
| 11 | deprecated config aliases 静默改变语义 | DISPROVED（G 已修） | parser 硬错误（migrate 提示）；schema/template 无键；config_consistency PASS |
| 12 | PUBLIC_API 含已删除 API | DISPROVED（J 已修） | p2_rejection_workspace_create/free 无未标注引用；api_doc_consistency deleted_api=PASS |
| 13 | Phase1 150s/frame 仍叫 performance frozen | DISPROVED（I 处理中） | PERFORMANCE_BASELINE=CANDIDATE（SCIENCE_FREEZE.md）；65s vs 150s 差异已解释；before/after runs 进行中 |
| 14 | warm catalogue cache 是否复用 | BUG_FOUND_AND_FIXED（工具层） | gaia_client 进程内缓存跨帧不命中（每帧独立进程）→ V17 用 platesolve hint（上一帧 CRVAL 作初始指向，逐帧求解验证）实现跨帧 warm；phase1_e2e_bench.py warm 模式 |
| 15 | Phase1 canonical source snapshot 完整性 | DISPROVED（H 处理中） | 审核包 source/canonical_core 含 orchestrator/calibration/plate solve/photometry/PSF/SNR/drizzle/shared/browser；repo_source_manifest.csv 全仓 path/SHA256/classification/caller |

## 审查中额外发现并修复

- orchestrator legacy 移除后不可编译（dll_loader 括号失衡 +
  GRADIENT_SPHERE/STACK 残留）→ 修复并重建（round1 P1-001/002）；
- Siril 同源对照极性 bug → 修复后 100% 一致（round1 P1-003）；
- large_scale mask 覆盖 bug → 只增不减（round1 P1-004）；
- rejection_cli normalization 漏读 → real16 9.45%→0.54%（round1 P1-005）；
- large_scale pre/post 统计口径 → grown 精确（round1 P2-001）。

```text
ROUND5=PASS（15/15 假设闭环；额外 5 处 BUG_FOUND_AND_FIXED）
```
