# Science Freeze（V17 True Final Freeze）

> 上游：ASTROCS_DESIGN.md §12.1（科学正确性与三重佐证）、§12.5（状态阶梯）

> 冻结结论：`ACCEPTANCE_GATES.md` G1-G10 全部 PASS、known P0/P1 = 0，
> `ASTROCS_FOUNDATION_FINAL_FREEZE = PASS`。

## V17 冻结状态

```text
PHASE1_BASE_ALGORITHMS = FROZEN（冻结算法不因审计与性能工作改变）
PHASE2_BASE_ALGORITHMS = FROZEN
REJECTION_SEMANTICS    = FROZEN（canonical semantic IDs + typed params +
                        eligibility/rejection 分层 + per-sample reason +
                        RejectionNormalizationPolicy）
ASTROCS_REJECT_PROFILE = FROZEN（生产科学路由 = ASTROCS_DESIGN.md
                        §5.5 档位表（M3 裁决 2026-09-25 后为三档），N = 几何覆盖帧数：
                        1≤N≤3→none；4≤N≤5→percentile；N≥6→winsorized sigma
                        （原 N≥16→linear_fit 档改投 winsorized）；min/max 不用于生产）
WBPP_AUTO_POLICY       = FROZEN（对照档 wbpp_2_9_1 = 本仓冻结解析表；档界
                        取自 WBPP 2.5.9 bestRejectionMethod，engine.js:1421-1429，
                        包 sha1 712cc7c3…；nominal<6→percentile；
                         6..15→winsorized；>15→linear_fit（**该档 WBPP 2.4.0+ 为
                         ESD，本仓取 linear_fit = WBPP ≤2.3.x 旧表**）；
                         wbpp_current 为 alias，运行期解析并序列化为 wbpp_2_9_1）
WBPP_LARGE_SCALE       = SUPPORTED（astrocs.large_scale_rejection.v1：
                        connected-component grow，min structure size，
                        low/high 独立半径；默认关闭 = WBPP
                        largeScaleClipLow/High 默认一致；非 PixInsight
                        exact）
REJECTION_NORMALIZATION = FROZEN（astrocs_median_center_v1 默认；
                        astrocs_median_scale_v1；none）
SATELLITE_REJECTION_GATE = PASS（受控注入 recall=1.0；n<=2 →
                        REJECTION_UNDERDETERMINED，不宣称可剔除；真实
                        16 帧只报 observed_rejection_rate，不称
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
PERFORMANCE_BASELINE   = FINAL（真实 16 帧 Phase1：cold median 145.4s /
                        warm median 142.4s（platesolve hint）；Drizzle
                        主导且冻结；无 >5% 回归；Phase2 24.0-25.1s；
                        Browser pan p50 34.7ms）
FINALIZATION_SELF_REVIEW = PASS（含 clean-tree 74/74 gate + 真实 16 帧
                        E2E + 受控 truth + external browser + no_legacy）
ASTROCS_FOUNDATION_FINAL_FREEZE = PASS（G1-G10 全部满足）
```

> 冻结后若发现新 P0/P1，按变更流程（科学等价门）处理。

PIXINSIGHT_EXACT_COMPATIBILITY = NOT_CLAIMED（WBPP profile 仅提供 Auto
routing 政策与参数映射，不宣称与 PixInsight 内核 bit-exact）。

## 已冻结基线

- HiPS 几何/序列化/hierarchy：V11（外部 oracle）。
- background-clean sampler / standardized Huber / smooth global
  continuation：V13（ACCEPTED）。
- UPM component 语义：V14（data/geometry/unobserved 分开）。
- Phase2 rejection：V15-V17（typed params、normalization、large-scale、
  integration status/support 契约；74/74 synthetic gate）。

## 冻结后允许

- 业务扩展、GUI、新算法（不改基础定义）；
- 经科学等价门（C/M 逐位或数值等价 + 回归集）的性能/重构优化。

## 冻结后排除面

- 恢复 `low`/`high`/`max_iterations`/`min_samples` 等 config alias
  （这些键不存在于现行 parser；旧 config 必须经 eng/tools/migrate_stage2_config.py 迁移）；
- 重新引入 active Stage2/healpix_stack 科学路径
  （no_legacy_production_reference gate 必须持续 PASS）；
- 把真实 16 帧 observed rejection rate 命名为 false reject。
