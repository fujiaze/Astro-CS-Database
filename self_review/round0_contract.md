# Round 0 — Contract / Scope Review（V16 Final Closure AuditFix）

日期：2026-08-14 ｜ HEAD：37bebb9+ ｜ 控制包：AstroCS_FinalClosure_AuditFix_Control_Package_V16

## 冻结合同（相对 V15 的变更）

1. **profile 拆分**：
   - `wbpp_current`：integration-group 层一次解析（nominal = group active
     独立 exposure 数）；tile/pixel 不重选；局部候选不足 = UNDERDETERMINED；
   - `astrocs_adaptive`：AstroCS 自有策略，允许 tile nominal-depth 自适应；
     不冒充 WBPP exact。
2. **RejectionNormalizationPolicy**：none / median_center / median_scale；
   decision 作用于 working stack；accepted mask 应用回原始 calibrated
   science values；加权积分使用原始值。
3. **MinMax**：一次性固定 rank 删除（reject_low_count 个最低 +
   reject_high_count 个最高，一次；n−low−high ≥ min_kept）；删除
   max_iterations。
4. **percentile**：必须 median_center（负值科学域安全）；rcr 必须 none；
   违规 → INVALID_CONFIGURATION。
5. **Eligibility 单路径**：`p2_collect_candidate_stack`（frame-major strided）
   CPU/ACR/compat 共用；quality 为 control 级（像素级无 quality 数组，
   stage2 传 nullptr 并记录）。
6. **Oracle**：averaged_sigma 改名 `astrocs.averaged_sigma.v1`，IRAF exact
   = NOT_CLAIMED；oracle_matrix 不允许 NOT_RUN 同时 G4=PASS。
7. **diagnostics**：depth_0/depth_1/depth_ge_2 mutually exclusive。
8. **WBPP Light 默认参数**：linearFit 5.0/3.5、percentile 0.2/0.1、
   sigma 4/3、winsorizationCutoff 5.0、large-scale 默认 off（unsupported
   如实标注）。
9. **真实 E2E**：raw→Phase1 per-exposure HiPS→Phase2（normalization +
   WBPP profile + rejection + integration）→HiPS→external/browser。
10. **交付**：canonical_core 快照 + repo_source_manifest.csv（path/sha256/
    semantic classification/production caller）。

## 允许删 / 禁止动

- 允许：workspace 占位 API 删除（G9）；ScratchVec 固定 scratch；
  schema/template/parser 更新；oracle 改名。
- 禁止：Phase1 冻结算法（platesolve 饱和选星为外部限制，不改）；
  healpix_stack 冻结；testdata 写入；ACR 业务算法。

## 语义模糊点

无（V16 审计决定已逐项定义）。

```text
ROUND0=PASS
```
