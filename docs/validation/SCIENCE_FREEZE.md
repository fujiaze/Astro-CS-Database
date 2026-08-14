# Science Freeze（V17 True Final Freeze 状态）

> 状态机：在 V17 Round0-6 clean-tree 终验完成前，本文件保持
> `ASTROCS_FOUNDATION_FINAL_FREEZE = CANDIDATE`；只有控制包
> ACCEPTANCE_GATES.md G1-G10 全部 PASS 且 known P0/P1 = 0 后，才可签
> `ASTROCS_FOUNDATION_FINAL_FREEZE = PASS`（由 final_status.md 更新）。

## V17 冻结状态（2026-08-14，V17 TrueFinal 控制包）

```text
PHASE1_BASE_ALGORITHMS = FROZEN（V14 审核通过后；V17 只审计与性能，
                        不改冻结算法）
PHASE2_BASE_ALGORITHMS = FROZEN
REJECTION_SEMANTICS    = FROZEN（canonical semantic IDs + typed params +
                        eligibility/rejection 分层 + per-sample reason +
                        RejectionNormalizationPolicy）
WBPP_AUTO_POLICY       = FROZEN（wbpp_2_9_1 = WBPP 2.9.1
                        bestRejectionMethod；nominal<6→percentile；
                         6..15→winsorized；>15→linear_fit；wbpp_current 仅
                         migration alias，运行期解析并序列化为 wbpp_2_9_1）
WBPP_LARGE_SCALE       = SUPPORTED（astrocs.large_scale_rejection.v1：
                        connected-component grow，min structure size，
                        low/high 独立半径；默认关闭 = WBPP
                        largeScaleClipLow/High 默认一致；非 PixInsight
                        exact）
REJECTION_NORMALIZATION = FROZEN（astrocs_median_center_v1 默认；
                        astrocs_median_scale_v1；none）
SATELLITE_REJECTION_GATE = PASS（受控注入 recall=1.0；n<=2 →
                        REJECTION_UNDERDETERMINED，不宣称可剔除；真实
                        16 帧只报 observed_rejection_rate，不再叫
                        false reject）
INTEGRATION_CONTRACT   = FROZEN（显式状态 OK/NO_CANDIDATES/ALL_REJECTED/
                        ZERO_VALID_WEIGHT/INVALID_INPUT；非 finite 权重/
                        support → INVALID_INPUT；support 唯一 canonical
                        reducer = max(accepted support)；UPM 控制权与
                        stack 积分权明确分开命名）
BASE_API_CONTRACT      = FROZEN（V17：rejection INVALID_* hard fail；
                        PUBLIC_API 与头文件 machine 一致）
CROSS_STAGE_CONTRACTS  = FROZEN
HIPS_BROWSER_BASE      = FROZEN
PERFORMANCE_BASELINE   = CANDIDATE（真实 16 帧 Phase1 ≈150 s/frame 已
                        profile；65s 历史差异已解释；V17 优化 + 3 runs
                        before/after 完成后由 final_status.md 更新）
FINALIZATION_SELF_REVIEW = RUNNING（V17 Round0-6 见 self_review/；
                        Round6 clean-tree 终验通过后置 PASS）
```

PIXINSIGHT_EXACT_COMPATIBILITY = NOT_CLAIMED（WBPP profile 仅提供 Auto
routing 政策与参数映射，不宣称与 PixInsight 内核 bit-exact）。

## 已冻结基线

- HiPS 几何/序列化/hierarchy：V11（外部 oracle）。
- background-clean sampler / standardized Huber / smooth global
  continuation：V13（用户 ACCEPTED）。
- UPM component 语义：V14（data/geometry/unobserved 分开）。
- Phase2 rejection：V15-V17（typed params、normalization、large-scale、
  integration status/support 契约；74/74 synthetic gate）。

## 冻结后允许

- 业务扩展、GUI、新算法（不改基础定义）；
- 经科学等价门（C/M 逐位或数值等价 + 回归集）的性能/重构优化。

## 冻结后不允许

- 恢复 legacy config aliases（low/high/max_iterations/min_samples 已删除，
  旧 config 必须经 tools/migrate_stage2_config.py 迁移）；
- 重新引入 active legacy Stage2/healpix_stack 科学路径
  （no_legacy_production_reference gate 必须持续 PASS）；
- 把真实 16 帧 observed rejection rate 命名为 false reject。
