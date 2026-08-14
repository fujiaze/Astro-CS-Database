# Round 0 — Contract / Scope Review（V17 True Final Freeze）

日期：2026-08-14 ｜ HEAD：03d5d96+ ｜ 控制包：AstroCS_TrueFinal_Freeze_Control_Package_V17
（SHA256 10ab2cddb534005fbf63b2ffe5e3695e1fbcc5976865df6e27b327bee29e30ed）

## 冻结合同（V16 → V17 变更）

1. **integration 最后 correctness（C01-C05）**：
   - 非 finite/非正 weight、非 finite support → `P2_INTEGRATE_INVALID_INPUT`
     （绝不 OK+NaN）；`p2_validate_candidate_weights` 在 SNR lookup 后统一
     校验；
   - rejection 仅 OK/UNDERDETERMINED 可继续；INVALID_INPUT/
     INVALID_CONFIGURATION/INVALID_METHOD/INTERNAL_ERROR 必须 hard fail；
   - output support 唯一 canonical reducer = max(accepted support)，
     Stage2/ACR 只消费 pr.support；
   - 显式状态枚举 OK/NO_CANDIDATES/ALL_REJECTED/ZERO_VALID_WEIGHT/
     INVALID_INPUT；
   - UPM 控制权重（upm.robust_control_weight.v1）与 stack 积分权重
     （stack.support_x_snr2.v1 / stack.equal.v1）分开命名。
2. **WBPP 版本化**：canonical = `wbpp_2_9_1`（group-level 一次解析）；
   `wbpp_current` 仅 migration alias；diagnostics/manifest 序列化版本化。
3. **rejection normalization 独立命名**：astrocs_median_center_v1 /
   astrocs_median_scale_v1（old 名仅 alias）。
4. **Large-Scale Rejection（用户要求 WBPP 类功能完整 → 实现）**：
   `astrocs.large_scale_rejection.v1`（8-连通分量 grow，min structure
   size，low/high 独立半径，默认关闭）；compact cosmic/星点不生长；
   PIXINSIGHT_EXACT=NOT_CLAIMED。
5. **受控 clean truth**：零离群合成 20 帧测 true FPR / 星点通量 / PSF /
   结构 / 噪声效率；注入 satellite/cosmic/streak 测 recall；真实 16 帧
   只报 observed_rejection_rate（不再叫 false reject）。
6. **legacy 多路径移除**：healpix_stack 移入 archive/legacy（不 build/
   link/load/call）；orchestrator legacy Stage2 wiring 删除；
   `no_legacy_production_reference` gate。
7. **旧 config aliases 删除**：low/high/max_iterations/min_samples 在
   parser 硬错误；提供 `tools/migrate_stage2_config.py`。
8. **Phase1 性能**：真实 16 帧分段 profile；冷/热分离；platesolve hint
   （上一帧 WCS 中心作初始指向，仍逐帧求解验证）；解释 65s vs 150s。
9. **docs/API/config machine 一致性**：PUBLIC_API 无已删 API；SCIENCE_
   FREEZE 更新；MinMax 无 max_iterations；新 machine check
   （api_doc_consistency.py）。
10. **Round0-6 增强**：Round5 至少 15 个攻击假设（V16 审计决定 K 列表）；
    Round6 clean-tree：全新构建 + 74/74 gate + 真实 16 帧 E2E + 受控
    truth + external browser + no_legacy。
11. **交付**：source/canonical_core（Phase1+Phase2+shared+Browser）+
    evidence/repo_source_manifest.csv（path/SHA256/classification/caller）。

## 允许 / 禁止

- 允许：上述 V17 修复；性能工具层优化（hint 传入、bench runner）；
  docs/reports/self_review 更新。
- 禁止：Phase1 冻结算法（Drizzle/platesolve 核心不改）；ACR 业务算法；
  testdata 写入；恢复 legacy aliases/多路径；把 observed 叫 false reject。

## 语义模糊点

无（V17 审计决定与 ACCEPTANCE_GATES.md 已逐项定义；两处可选项已按用户
要求选择：Large-Scale=Option A 实现；性能=platesolve hint 工具层）。

```text
ROUND0=PASS
```
