# ALG-P1-001 证据与命令日志（Phase1 算法实施规格）

- 文档 ID：`ALG-P1-001-EVIDENCE`
- 任务：`工程控制/AstroCS_PARALLEL_SCIENCE_IMPLEMENTATION_V6_20260915/tasks/ALG-P1-001.md`
- write_scope：`docs/algorithms/v6/phase1/`（唯一；本任务未写任何其他路径）

## 0. 基线与并发集成说明

- 开工基线（任务卡 / 本任务 `git rev-parse HEAD` 复核）：`125bc0999363be1a42a1f2df3254601e0cc7b8fb`
- 报告时 HEAD：`28e0ac6ccecca0599639e4706263359a2779c26b`（分支 main）
- `git merge-base --is-ancestor 125bc099 HEAD` 为真：基线是当前 HEAD 的祖先。
- 期间控制器集成了并行 W3 兄弟任务 `1c4e5f92`（ALG-P2-PSFSW-001）与 `28e0ac6c`（ALG-P2-POINT-001），二者只改各自写域。
- `git diff --stat 125bc099..HEAD -- docs/science/v6 docs/science/UNIFIED_SCIENCE_MODEL.md docs/science/PSF_SIGNAL_WEIGHT.md docs/owner docs/design` 输出为空 —— 本规格引用的全部冻结/上位科学锚点在任务期间未变。
- 工作树预存回退（F1）为控制器级事项（域外 dirty 计数 ~178），本任务仅新增本目录下文件。

## 1. 产物清单（全部在 write_scope 内）

```text
docs/algorithms/v6/phase1/ALG_P1_001_PHASE1_ALGORITHM_SPEC.md
docs/algorithms/v6/phase1/alg_p1_001_spec.json
docs/algorithms/v6/phase1/alg_p1_001_test_matrix.json
docs/algorithms/v6/phase1/ALG_P1_001_TEST_MATRIX.md
docs/algorithms/v6/phase1/EVIDENCE.md
docs/algorithms/v6/phase1/README.md
docs/algorithms/v6/phase1/tools/verify_alg_p1_001.py
```

## 2. 命令与实测退出码

| # | 命令（cwd = `docs/algorithms/v6/phase1`） | 实测 rc | 结果 |
|---|---|---|---|
| 1 | `python3 tools/verify_alg_p1_001.py --selftest` | `0` | 38/38 基线门 PASS |
| 2 | `python3 tools/verify_alg_p1_001.py --all-mutations` | `0` | 39/39 负向 mutation CAUGHT |
| 3 | `python3 tools/verify_alg_p1_001.py --mutation M-D1` | `2` | CAUGHT（常量面亮度门红） |
| 4 | `python3 tools/verify_alg_p1_001.py --mutation M-P5` | `2` | CAUGHT（psfsw fail-closed 门红） |
| 5 | `python3 tools/verify_alg_p1_001.py --mutation M-W1` | `2` | CAUGHT（白噪声恒等门红） |
| 6 | `python3 tools/verify_alg_p1_001.py --mutation M-S1` | `2` | CAUGHT（冻结继承门红） |
| 7 | `python3 tools/verify_alg_p1_001.py --mutation M-S17` | `2` | CAUGHT（测试矩阵/规格门覆盖一致性门红） |
| 8 | `python3 tools/verify_alg_p1_001.py --mutation NO-SUCH` | `1` | FAIL: unknown mutation |
| 9 | `git status --porcelain docs/algorithms/v6/phase1/` | `0` | 仅新增本目录，无域外路径 |

退出码约定：`--selftest` 全绿 rc=0；`--all-mutations` 全捕获 rc=0；单 mutation 被捕获 rc=2，未捕获 rc=1。

## 3. 基线独立 Oracle / 结构一致性输出（38/38 PASS，rc=0）

```text
== ALG-P1-001 baseline (independent Oracle + structure) ==
  [PASS] G-STRUCT-FREEZE-INHERIT          required 19 freeze ids all inherited
  [PASS] G-STRUCT-UNIT-LAW                variance=signal^2, ivar=1/variance, W_info=ADU^-2, psfsw=1
  [PASS] G-STRUCT-FORBIDDEN-TOKENS        forbidden token set complete
  [PASS] G-STRUCT-ALG-AREAS               four algorithm areas present
  [PASS] G-STRUCT-GATES                   frozen gates present in gates set
  [PASS] G-MODE-PRODUCTION                production 3 modes; psf_snr_power deferred; legacy/auto forbidden
  [PASS] G-SO-REGISTERED                  SO-01..07 registered only, not signed
  [PASS] G-CTRL-REGISTERED                controller-level items registered only
  [PASS] G-REGISTRY-COVERS-MATRIX         test-matrix/spec gate ids subset of executed CHECKS (missing tm=[] spec=[])
  [PASS] CAL-COV-REPRESENTATION           shared master: 3 allowed representations
  [PASS] CAL-NO-CLIP                      no clipping / no pedestal
  [PASS] ADJ-OBS-01-SHARED                shared master detected: var_joint/var_naive=2.600000 > 1
  [PASS] CAL-COV-FORMULA                  cal per-pixel variance == J C J^T (same master folded as (1-alpha)^2)
  [PASS] CAL-UNIT                         variance unit == (signal unit)^2
  [PASS] FZ-FORMULA-WINFO                 F_hat=Q/W==GLS; Var=1/W=c^T C c; W(2a)/W(a)=4
  [PASS] FZ-COND-WHITENOISE               W=a^2 sum P^2/sigma^2 == a^2/(sigma^2 A_NEA); A_NEA=1/sum P^2
  [PASS] FZ-WINFO-DIAG-APPROX             approx diagonal must report c~^T C c~ and deviation
  [PASS] FZ-GATE-MEDIAN-SNR               W_info forbidden sources complete
  [PASS] FZ-DEGRADE-SCALAR                scalar downgrade needs dual gate + p05/p50/p95 + domain
  [PASS] FZ-GATE-CONST-SB                 S_p=B0 for pixfrac in {0.25,0.5,0.8,1.0}, |S/B0-1|<1e-3
  [PASS] FZ-FORMULA-DRIZZLE-VAR           variance_p=sum c_jp^2 v_j and alpha^2 scale law exact
  [PASS] FZ-COND-FLUX-CONSERV             Phi_out=sum_p S_p D_p = pixfrac^2 * sum_j x_j exact
  [PASS] FZ-DRZ-CORRELATION               Cov(S_p,S_q)=sum_j c_jp c_jq v_j with non-zero off-diagonal
  [PASS] FZ-DRZ-APERTURE                  aperture exact variance strictly greater than diagonal-only lower bound
  [PASS] FZ-FORMULA-COV-PROP              C_out = R C_in R^T: Cov(S_p,S_q)=sum_j c_jp c_jq v_j with non-zero off-diagonal | aperture exact variance strictly greater than diagonal-only lower bound
  [PASS] FZ-GATE-PARENT-VAR               parent variance: lower_bound + kernel + deficit gate
  [PASS] FZ-DRZ-PARENT-NUMERIC            parent Var is a lower bound; deficit=0.500000 (threshold pending SO-07)
  [PASS] FZ-DRZ-FLUX-PROV                 flux factor in provenance
  [PASS] FZ-DRZ-SB-DEF                    SB-preserving normalization declared
  [PASS] FZ-FIELD-PSFSW-4COMP             four distinct components + spatial summary
  [PASS] FZ-FIELD-PSFSW-UNIT              psfsw unit/scope/covariance boundary frozen
  [PASS] FZ-GATE-PSFSW-EPSF               effective PSF required
  [PASS] FZ-GATE-PSFSW-COV                psfsw covariance boundary: method / no weight-derived variance / epsf present
  [PASS] FZ-GATE-PSFSW-FAILCLOSED         fail-closed whitelist; valid=false => null weight
  [PASS] FZ-GATE-PSFSW-NOKEYS             psfsw forbidden keys present
  [PASS] FZ-PSFSW-COMMON-STAR-SET         common star set independent of frame measurement
  [PASS] FZ-FORMULA-PSFSW-COMPOSITE       group median=1, all positive, monotone S/Conc up and N/B down
  [PASS] FZ-PSFSW-RECORD                  psfsw record conforms (4 components, unit, scope, covariance, validity, no banned keys)
  -> 38/38 PASS
```

## 4. 负向 mutation 全量输出（39/39 CAUGHT，rc=0）

```text
== ALG-P1-001 baseline (independent Oracle + structure) ==
  [PASS] G-STRUCT-FREEZE-INHERIT          required 19 freeze ids all inherited
  [PASS] G-STRUCT-UNIT-LAW                variance=signal^2, ivar=1/variance, W_info=ADU^-2, psfsw=1
  [PASS] G-STRUCT-FORBIDDEN-TOKENS        forbidden token set complete
  [PASS] G-STRUCT-ALG-AREAS               four algorithm areas present
  [PASS] G-STRUCT-GATES                   frozen gates present in gates set
  [PASS] G-MODE-PRODUCTION                production 3 modes; psf_snr_power deferred; legacy/auto forbidden
  [PASS] G-SO-REGISTERED                  SO-01..07 registered only, not signed
  [PASS] G-CTRL-REGISTERED                controller-level items registered only
  [PASS] G-REGISTRY-COVERS-MATRIX         test-matrix/spec gate ids subset of executed CHECKS (missing tm=[] spec=[])
  [PASS] CAL-COV-REPRESENTATION           shared master: 3 allowed representations
  [PASS] CAL-NO-CLIP                      no clipping / no pedestal
  [PASS] ADJ-OBS-01-SHARED                shared master detected: var_joint/var_naive=2.600000 > 1
  [PASS] CAL-COV-FORMULA                  cal per-pixel variance == J C J^T (same master folded as (1-alpha)^2)
  [PASS] CAL-UNIT                         variance unit == (signal unit)^2
  [PASS] FZ-FORMULA-WINFO                 F_hat=Q/W==GLS; Var=1/W=c^T C c; W(2a)/W(a)=4
  [PASS] FZ-COND-WHITENOISE               W=a^2 sum P^2/sigma^2 == a^2/(sigma^2 A_NEA); A_NEA=1/sum P^2
  [PASS] FZ-WINFO-DIAG-APPROX             approx diagonal must report c~^T C c~ and deviation
  [PASS] FZ-GATE-MEDIAN-SNR               W_info forbidden sources complete
  [PASS] FZ-DEGRADE-SCALAR                scalar downgrade needs dual gate + p05/p50/p95 + domain
  [PASS] FZ-GATE-CONST-SB                 S_p=B0 for pixfrac in {0.25,0.5,0.8,1.0}, |S/B0-1|<1e-3
  [PASS] FZ-FORMULA-DRIZZLE-VAR           variance_p=sum c_jp^2 v_j and alpha^2 scale law exact
  [PASS] FZ-COND-FLUX-CONSERV             Phi_out=sum_p S_p D_p = pixfrac^2 * sum_j x_j exact
  [PASS] FZ-DRZ-CORRELATION               Cov(S_p,S_q)=sum_j c_jp c_jq v_j with non-zero off-diagonal
  [PASS] FZ-DRZ-APERTURE                  aperture exact variance strictly greater than diagonal-only lower bound
  [PASS] FZ-FORMULA-COV-PROP              C_out = R C_in R^T: Cov(S_p,S_q)=sum_j c_jp c_jq v_j with non-zero off-diagonal | aperture exact variance strictly greater than diagonal-only lower bound
  [PASS] FZ-GATE-PARENT-VAR               parent variance: lower_bound + kernel + deficit gate
  [PASS] FZ-DRZ-PARENT-NUMERIC            parent Var is a lower bound; deficit=0.500000 (threshold pending SO-07)
  [PASS] FZ-DRZ-FLUX-PROV                 flux factor in provenance
  [PASS] FZ-DRZ-SB-DEF                    SB-preserving normalization declared
  [PASS] FZ-FIELD-PSFSW-4COMP             four distinct components + spatial summary
  [PASS] FZ-FIELD-PSFSW-UNIT              psfsw unit/scope/covariance boundary frozen
  [PASS] FZ-GATE-PSFSW-EPSF               effective PSF required
  [PASS] FZ-GATE-PSFSW-COV                psfsw covariance boundary: method / no weight-derived variance / epsf present
  [PASS] FZ-GATE-PSFSW-FAILCLOSED         fail-closed whitelist; valid=false => null weight
  [PASS] FZ-GATE-PSFSW-NOKEYS             psfsw forbidden keys present
  [PASS] FZ-PSFSW-COMMON-STAR-SET         common star set independent of frame measurement
  [PASS] FZ-FORMULA-PSFSW-COMPOSITE       group median=1, all positive, monotone S/Conc up and N/B down
  [PASS] FZ-PSFSW-RECORD                  psfsw record conforms (4 components, unit, scope, covariance, validity, no banned keys)
  -> 38/38 PASS
== negative mutations ==
  [CAUGHT] M-S1   target=G-STRUCT-FREEZE-INHERIT      required 19 freeze ids all inherited
  [CAUGHT] M-S2   target=G-STRUCT-FORBIDDEN-TOKENS    forbidden token set complete
  [CAUGHT] M-S3   target=G-STRUCT-ALG-AREAS           four algorithm areas present
  [CAUGHT] M-S4   target=G-STRUCT-UNIT-LAW            variance=signal^2, ivar=1/variance, W_info=ADU^-2, psfsw=1
  [CAUGHT] M-S5   target=G-STRUCT-GATES               frozen gates present in gates set
  [CAUGHT] M-S6   target=G-MODE-PRODUCTION            production 3 modes; psf_snr_power deferred; legacy/auto forbidden
  [CAUGHT] M-S7   target=FZ-GATE-PSFSW-NOKEYS         psfsw forbidden keys present
  [CAUGHT] M-S8   target=FZ-GATE-PSFSW-EPSF           effective PSF required
  [CAUGHT] M-S9   target=FZ-GATE-PSFSW-FAILCLOSED     fail-closed whitelist; valid=false => null weight
  [CAUGHT] M-S10  target=FZ-PSFSW-COMMON-STAR-SET     common star set independent of frame measurement
  [CAUGHT] M-S11  target=FZ-GATE-PARENT-VAR           parent variance: lower_bound + kernel + deficit gate
  [CAUGHT] M-S12  target=FZ-GATE-MEDIAN-SNR           W_info forbidden sources complete
  [CAUGHT] M-S13  target=CAL-COV-REPRESENTATION       shared master: 3 allowed representations
  [CAUGHT] M-S14  target=CAL-NO-CLIP                  no clipping / no pedestal
  [CAUGHT] M-S15  target=FZ-WINFO-DIAG-APPROX         approx diagonal must report c~^T C c~ and deviation
  [CAUGHT] M-S16  target=FZ-DEGRADE-SCALAR            scalar downgrade needs dual gate + p05/p50/p95 + domain
  [CAUGHT] M-S17  target=G-REGISTRY-COVERS-MATRIX     test-matrix/spec gate ids subset of executed CHECKS (missing tm=['NO-SUCH-GATE'] spec=[])
  [CAUGHT] M-D1   target=FZ-GATE-CONST-SB             const-SB invariant failed at pixfrac=0.25
  [CAUGHT] M-D3   target=FZ-GATE-CONST-SB             const-SB invariant failed at pixfrac=0.25
  [CAUGHT] M-D2   target=FZ-FORMULA-DRIZZLE-VAR       variance formula != sum c^2 v at pixfrac=0.50
  [CAUGHT] M-D7   target=FZ-FORMULA-DRIZZLE-VAR       variance formula != sum c^2 v at pixfrac=0.50
  [CAUGHT] M-D4   target=FZ-COND-FLUX-CONSERV         flux conservation factor wrong at pixfrac=0.25 (got 8.25 exp 132)
  [CAUGHT] M-D5   target=FZ-DRZ-APERTURE              aperture exact variance not strictly above diagonal-only (0.0363 vs 0.0363)
  [CAUGHT] M-D6   target=FZ-DRZ-PARENT-NUMERIC        parent diagonal claimed exact while deficit=0.500000>0
  [CAUGHT] M-C1   target=ADJ-OBS-01-SHARED            shared master not detected: ratio=1 (must be >1)
  [CAUGHT] M-C2   target=CAL-COV-FORMULA              cal per-pixel variance != J C J^T (got 0.2492 ref 0.2372)
  [CAUGHT] M-C4   target=CAL-UNIT                     variance unit == (signal unit)^2
  [CAUGHT] M-W1   target=FZ-COND-WHITENOISE           white-noise identity failed (2.72978217139 vs 1.04627395912)
  [CAUGHT] M-W3   target=FZ-COND-WHITENOISE           white-noise identity failed (2.72978217139 vs 4.35765913837)
  [CAUGHT] M-W2   target=FZ-FORMULA-WINFO             GLS/Q-W mismatch (F_hat=5.46 1/W=0.376623513 cC c=0.289710395)
  [CAUGHT] M-P1   target=FZ-PSFSW-RECORD              psfsw measurement_id not distinct (component collapse)
  [CAUGHT] M-P2   target=FZ-FORMULA-PSFSW-COMPOSITE   psfsw group median != 1
  [CAUGHT] M-P3   target=FZ-PSFSW-RECORD              psfsw weight kind/units must be relative_dimensionless / 1
  [CAUGHT] M-P4   target=FZ-GATE-PSFSW-COV            psfsw covariance boundary: method / no weight-derived variance / epsf present
  [CAUGHT] M-P5   target=FZ-PSFSW-RECORD              psfsw invalid must have weight_value=None (no median-SNR fallback)
  [CAUGHT] M-P6   target=FZ-PSFSW-RECORD              psfsw four components incomplete
  [CAUGHT] M-P7   target=FZ-PSFSW-RECORD              psfsw effective PSF missing
  [CAUGHT] M-P8   target=FZ-PSFSW-RECORD              psfsw product contains forbidden key
  [CAUGHT] M-P9   target=FZ-PSFSW-RECORD              psfsw normalization must be group / median_target=1 / versioned
  -> 39/39 CAUGHT
```

## 5. 单 mutation 注入样例（rc=2 = 门变红）

- `M-D1`：
```text
== ALG-P1-001 baseline (independent Oracle + structure) ==
  [PASS] G-STRUCT-FREEZE-INHERIT          required 19 freeze ids all inherited
  [PASS] G-STRUCT-UNIT-LAW                variance=signal^2, ivar=1/variance, W_info=ADU^-2, psfsw=1
  [PASS] G-STRUCT-FORBIDDEN-TOKENS        forbidden token set complete
  [PASS] G-STRUCT-ALG-AREAS               four algorithm areas present
  [PASS] G-STRUCT-GATES                   frozen gates present in gates set
  [PASS] G-MODE-PRODUCTION                production 3 modes; psf_snr_power deferred; legacy/auto forbidden
  [PASS] G-SO-REGISTERED                  SO-01..07 registered only, not signed
  [PASS] G-CTRL-REGISTERED                controller-level items registered only
  [PASS] G-REGISTRY-COVERS-MATRIX         test-matrix/spec gate ids subset of executed CHECKS (missing tm=[] spec=[])
  [PASS] CAL-COV-REPRESENTATION           shared master: 3 allowed representations
  [PASS] CAL-NO-CLIP                      no clipping / no pedestal
  [PASS] ADJ-OBS-01-SHARED                shared master detected: var_joint/var_naive=2.600000 > 1
  [PASS] CAL-COV-FORMULA                  cal per-pixel variance == J C J^T (same master folded as (1-alpha)^2)
  [PASS] CAL-UNIT                         variance unit == (signal unit)^2
  [PASS] FZ-FORMULA-WINFO                 F_hat=Q/W==GLS; Var=1/W=c^T C c; W(2a)/W(a)=4
  [PASS] FZ-COND-WHITENOISE               W=a^2 sum P^2/sigma^2 == a^2/(sigma^2 A_NEA); A_NEA=1/sum P^2
  [PASS] FZ-WINFO-DIAG-APPROX             approx diagonal must report c~^T C c~ and deviation
  [PASS] FZ-GATE-MEDIAN-SNR               W_info forbidden sources complete
  [PASS] FZ-DEGRADE-SCALAR                scalar downgrade needs dual gate + p05/p50/p95 + domain
  [PASS] FZ-GATE-CONST-SB                 S_p=B0 for pixfrac in {0.25,0.5,0.8,1.0}, |S/B0-1|<1e-3
  [PASS] FZ-FORMULA-DRIZZLE-VAR           variance_p=sum c_jp^2 v_j and alpha^2 scale law exact
  [PASS] FZ-COND-FLUX-CONSERV             Phi_out=sum_p S_p D_p = pixfrac^2 * sum_j x_j exact
  [PASS] FZ-DRZ-CORRELATION               Cov(S_p,S_q)=sum_j c_jp c_jq v_j with non-zero off-diagonal
  [PASS] FZ-DRZ-APERTURE                  aperture exact variance strictly greater than diagonal-only lower bound
  [PASS] FZ-FORMULA-COV-PROP              C_out = R C_in R^T: Cov(S_p,S_q)=sum_j c_jp c_jq v_j with non-zero off-diagonal | aperture exact variance strictly greater than diagonal-only lower bound
  [PASS] FZ-GATE-PARENT-VAR               parent variance: lower_bound + kernel + deficit gate
  [PASS] FZ-DRZ-PARENT-NUMERIC            parent Var is a lower bound; deficit=0.500000 (threshold pending SO-07)
  [PASS] FZ-DRZ-FLUX-PROV                 flux factor in provenance
  [PASS] FZ-DRZ-SB-DEF                    SB-preserving normalization declared
  [PASS] FZ-FIELD-PSFSW-4COMP             four distinct components + spatial summary
  [PASS] FZ-FIELD-PSFSW-UNIT              psfsw unit/scope/covariance boundary frozen
  [PASS] FZ-GATE-PSFSW-EPSF               effective PSF required
  [PASS] FZ-GATE-PSFSW-COV                psfsw covariance boundary: method / no weight-derived variance / epsf present
  [PASS] FZ-GATE-PSFSW-FAILCLOSED         fail-closed whitelist; valid=false => null weight
  [PASS] FZ-GATE-PSFSW-NOKEYS             psfsw forbidden keys present
  [PASS] FZ-PSFSW-COMMON-STAR-SET         common star set independent of frame measurement
  [PASS] FZ-FORMULA-PSFSW-COMPOSITE       group median=1, all positive, monotone S/Conc up and N/B down
  [PASS] FZ-PSFSW-RECORD                  psfsw record conforms (4 components, unit, scope, covariance, validity, no banned keys)
  -> 38/38 PASS
  mutation M-D1 target=FZ-GATE-CONST-SB -> CAUGHT (const-SB invariant failed at pixfrac=0.25)
rc=2
```
- `M-P5`：
```text
== ALG-P1-001 baseline (independent Oracle + structure) ==
  [PASS] G-STRUCT-FREEZE-INHERIT          required 19 freeze ids all inherited
  [PASS] G-STRUCT-UNIT-LAW                variance=signal^2, ivar=1/variance, W_info=ADU^-2, psfsw=1
  [PASS] G-STRUCT-FORBIDDEN-TOKENS        forbidden token set complete
  [PASS] G-STRUCT-ALG-AREAS               four algorithm areas present
  [PASS] G-STRUCT-GATES                   frozen gates present in gates set
  [PASS] G-MODE-PRODUCTION                production 3 modes; psf_snr_power deferred; legacy/auto forbidden
  [PASS] G-SO-REGISTERED                  SO-01..07 registered only, not signed
  [PASS] G-CTRL-REGISTERED                controller-level items registered only
  [PASS] G-REGISTRY-COVERS-MATRIX         test-matrix/spec gate ids subset of executed CHECKS (missing tm=[] spec=[])
  [PASS] CAL-COV-REPRESENTATION           shared master: 3 allowed representations
  [PASS] CAL-NO-CLIP                      no clipping / no pedestal
  [PASS] ADJ-OBS-01-SHARED                shared master detected: var_joint/var_naive=2.600000 > 1
  [PASS] CAL-COV-FORMULA                  cal per-pixel variance == J C J^T (same master folded as (1-alpha)^2)
  [PASS] CAL-UNIT                         variance unit == (signal unit)^2
  [PASS] FZ-FORMULA-WINFO                 F_hat=Q/W==GLS; Var=1/W=c^T C c; W(2a)/W(a)=4
  [PASS] FZ-COND-WHITENOISE               W=a^2 sum P^2/sigma^2 == a^2/(sigma^2 A_NEA); A_NEA=1/sum P^2
  [PASS] FZ-WINFO-DIAG-APPROX             approx diagonal must report c~^T C c~ and deviation
  [PASS] FZ-GATE-MEDIAN-SNR               W_info forbidden sources complete
  [PASS] FZ-DEGRADE-SCALAR                scalar downgrade needs dual gate + p05/p50/p95 + domain
  [PASS] FZ-GATE-CONST-SB                 S_p=B0 for pixfrac in {0.25,0.5,0.8,1.0}, |S/B0-1|<1e-3
  [PASS] FZ-FORMULA-DRIZZLE-VAR           variance_p=sum c_jp^2 v_j and alpha^2 scale law exact
  [PASS] FZ-COND-FLUX-CONSERV             Phi_out=sum_p S_p D_p = pixfrac^2 * sum_j x_j exact
  [PASS] FZ-DRZ-CORRELATION               Cov(S_p,S_q)=sum_j c_jp c_jq v_j with non-zero off-diagonal
  [PASS] FZ-DRZ-APERTURE                  aperture exact variance strictly greater than diagonal-only lower bound
  [PASS] FZ-FORMULA-COV-PROP              C_out = R C_in R^T: Cov(S_p,S_q)=sum_j c_jp c_jq v_j with non-zero off-diagonal | aperture exact variance strictly greater than diagonal-only lower bound
  [PASS] FZ-GATE-PARENT-VAR               parent variance: lower_bound + kernel + deficit gate
  [PASS] FZ-DRZ-PARENT-NUMERIC            parent Var is a lower bound; deficit=0.500000 (threshold pending SO-07)
  [PASS] FZ-DRZ-FLUX-PROV                 flux factor in provenance
  [PASS] FZ-DRZ-SB-DEF                    SB-preserving normalization declared
  [PASS] FZ-FIELD-PSFSW-4COMP             four distinct components + spatial summary
  [PASS] FZ-FIELD-PSFSW-UNIT              psfsw unit/scope/covariance boundary frozen
  [PASS] FZ-GATE-PSFSW-EPSF               effective PSF required
  [PASS] FZ-GATE-PSFSW-COV                psfsw covariance boundary: method / no weight-derived variance / epsf present
  [PASS] FZ-GATE-PSFSW-FAILCLOSED         fail-closed whitelist; valid=false => null weight
  [PASS] FZ-GATE-PSFSW-NOKEYS             psfsw forbidden keys present
  [PASS] FZ-PSFSW-COMMON-STAR-SET         common star set independent of frame measurement
  [PASS] FZ-FORMULA-PSFSW-COMPOSITE       group median=1, all positive, monotone S/Conc up and N/B down
  [PASS] FZ-PSFSW-RECORD                  psfsw record conforms (4 components, unit, scope, covariance, validity, no banned keys)
  -> 38/38 PASS
  mutation M-P5 target=FZ-PSFSW-RECORD -> CAUGHT (psfsw invalid must have weight_value=None (no median-SNR fallback))
rc=2
```
- `M-W1`：
```text
== ALG-P1-001 baseline (independent Oracle + structure) ==
  [PASS] G-STRUCT-FREEZE-INHERIT          required 19 freeze ids all inherited
  [PASS] G-STRUCT-UNIT-LAW                variance=signal^2, ivar=1/variance, W_info=ADU^-2, psfsw=1
  [PASS] G-STRUCT-FORBIDDEN-TOKENS        forbidden token set complete
  [PASS] G-STRUCT-ALG-AREAS               four algorithm areas present
  [PASS] G-STRUCT-GATES                   frozen gates present in gates set
  [PASS] G-MODE-PRODUCTION                production 3 modes; psf_snr_power deferred; legacy/auto forbidden
  [PASS] G-SO-REGISTERED                  SO-01..07 registered only, not signed
  [PASS] G-CTRL-REGISTERED                controller-level items registered only
  [PASS] G-REGISTRY-COVERS-MATRIX         test-matrix/spec gate ids subset of executed CHECKS (missing tm=[] spec=[])
  [PASS] CAL-COV-REPRESENTATION           shared master: 3 allowed representations
  [PASS] CAL-NO-CLIP                      no clipping / no pedestal
  [PASS] ADJ-OBS-01-SHARED                shared master detected: var_joint/var_naive=2.600000 > 1
  [PASS] CAL-COV-FORMULA                  cal per-pixel variance == J C J^T (same master folded as (1-alpha)^2)
  [PASS] CAL-UNIT                         variance unit == (signal unit)^2
  [PASS] FZ-FORMULA-WINFO                 F_hat=Q/W==GLS; Var=1/W=c^T C c; W(2a)/W(a)=4
  [PASS] FZ-COND-WHITENOISE               W=a^2 sum P^2/sigma^2 == a^2/(sigma^2 A_NEA); A_NEA=1/sum P^2
  [PASS] FZ-WINFO-DIAG-APPROX             approx diagonal must report c~^T C c~ and deviation
  [PASS] FZ-GATE-MEDIAN-SNR               W_info forbidden sources complete
  [PASS] FZ-DEGRADE-SCALAR                scalar downgrade needs dual gate + p05/p50/p95 + domain
  [PASS] FZ-GATE-CONST-SB                 S_p=B0 for pixfrac in {0.25,0.5,0.8,1.0}, |S/B0-1|<1e-3
  [PASS] FZ-FORMULA-DRIZZLE-VAR           variance_p=sum c_jp^2 v_j and alpha^2 scale law exact
  [PASS] FZ-COND-FLUX-CONSERV             Phi_out=sum_p S_p D_p = pixfrac^2 * sum_j x_j exact
  [PASS] FZ-DRZ-CORRELATION               Cov(S_p,S_q)=sum_j c_jp c_jq v_j with non-zero off-diagonal
  [PASS] FZ-DRZ-APERTURE                  aperture exact variance strictly greater than diagonal-only lower bound
  [PASS] FZ-FORMULA-COV-PROP              C_out = R C_in R^T: Cov(S_p,S_q)=sum_j c_jp c_jq v_j with non-zero off-diagonal | aperture exact variance strictly greater than diagonal-only lower bound
  [PASS] FZ-GATE-PARENT-VAR               parent variance: lower_bound + kernel + deficit gate
  [PASS] FZ-DRZ-PARENT-NUMERIC            parent Var is a lower bound; deficit=0.500000 (threshold pending SO-07)
  [PASS] FZ-DRZ-FLUX-PROV                 flux factor in provenance
  [PASS] FZ-DRZ-SB-DEF                    SB-preserving normalization declared
  [PASS] FZ-FIELD-PSFSW-4COMP             four distinct components + spatial summary
  [PASS] FZ-FIELD-PSFSW-UNIT              psfsw unit/scope/covariance boundary frozen
  [PASS] FZ-GATE-PSFSW-EPSF               effective PSF required
  [PASS] FZ-GATE-PSFSW-COV                psfsw covariance boundary: method / no weight-derived variance / epsf present
  [PASS] FZ-GATE-PSFSW-FAILCLOSED         fail-closed whitelist; valid=false => null weight
  [PASS] FZ-GATE-PSFSW-NOKEYS             psfsw forbidden keys present
  [PASS] FZ-PSFSW-COMMON-STAR-SET         common star set independent of frame measurement
  [PASS] FZ-FORMULA-PSFSW-COMPOSITE       group median=1, all positive, monotone S/Conc up and N/B down
  [PASS] FZ-PSFSW-RECORD                  psfsw record conforms (4 components, unit, scope, covariance, validity, no banned keys)
  -> 38/38 PASS
  mutation M-W1 target=FZ-COND-WHITENOISE -> CAUGHT (white-noise identity failed (2.72978217139 vs 1.04627395912))
rc=2
```
- `M-S1`：
```text
== ALG-P1-001 baseline (independent Oracle + structure) ==
  [PASS] G-STRUCT-FREEZE-INHERIT          required 19 freeze ids all inherited
  [PASS] G-STRUCT-UNIT-LAW                variance=signal^2, ivar=1/variance, W_info=ADU^-2, psfsw=1
  [PASS] G-STRUCT-FORBIDDEN-TOKENS        forbidden token set complete
  [PASS] G-STRUCT-ALG-AREAS               four algorithm areas present
  [PASS] G-STRUCT-GATES                   frozen gates present in gates set
  [PASS] G-MODE-PRODUCTION                production 3 modes; psf_snr_power deferred; legacy/auto forbidden
  [PASS] G-SO-REGISTERED                  SO-01..07 registered only, not signed
  [PASS] G-CTRL-REGISTERED                controller-level items registered only
  [PASS] G-REGISTRY-COVERS-MATRIX         test-matrix/spec gate ids subset of executed CHECKS (missing tm=[] spec=[])
  [PASS] CAL-COV-REPRESENTATION           shared master: 3 allowed representations
  [PASS] CAL-NO-CLIP                      no clipping / no pedestal
  [PASS] ADJ-OBS-01-SHARED                shared master detected: var_joint/var_naive=2.600000 > 1
  [PASS] CAL-COV-FORMULA                  cal per-pixel variance == J C J^T (same master folded as (1-alpha)^2)
  [PASS] CAL-UNIT                         variance unit == (signal unit)^2
  [PASS] FZ-FORMULA-WINFO                 F_hat=Q/W==GLS; Var=1/W=c^T C c; W(2a)/W(a)=4
  [PASS] FZ-COND-WHITENOISE               W=a^2 sum P^2/sigma^2 == a^2/(sigma^2 A_NEA); A_NEA=1/sum P^2
  [PASS] FZ-WINFO-DIAG-APPROX             approx diagonal must report c~^T C c~ and deviation
  [PASS] FZ-GATE-MEDIAN-SNR               W_info forbidden sources complete
  [PASS] FZ-DEGRADE-SCALAR                scalar downgrade needs dual gate + p05/p50/p95 + domain
  [PASS] FZ-GATE-CONST-SB                 S_p=B0 for pixfrac in {0.25,0.5,0.8,1.0}, |S/B0-1|<1e-3
  [PASS] FZ-FORMULA-DRIZZLE-VAR           variance_p=sum c_jp^2 v_j and alpha^2 scale law exact
  [PASS] FZ-COND-FLUX-CONSERV             Phi_out=sum_p S_p D_p = pixfrac^2 * sum_j x_j exact
  [PASS] FZ-DRZ-CORRELATION               Cov(S_p,S_q)=sum_j c_jp c_jq v_j with non-zero off-diagonal
  [PASS] FZ-DRZ-APERTURE                  aperture exact variance strictly greater than diagonal-only lower bound
  [PASS] FZ-FORMULA-COV-PROP              C_out = R C_in R^T: Cov(S_p,S_q)=sum_j c_jp c_jq v_j with non-zero off-diagonal | aperture exact variance strictly greater than diagonal-only lower bound
  [PASS] FZ-GATE-PARENT-VAR               parent variance: lower_bound + kernel + deficit gate
  [PASS] FZ-DRZ-PARENT-NUMERIC            parent Var is a lower bound; deficit=0.500000 (threshold pending SO-07)
  [PASS] FZ-DRZ-FLUX-PROV                 flux factor in provenance
  [PASS] FZ-DRZ-SB-DEF                    SB-preserving normalization declared
  [PASS] FZ-FIELD-PSFSW-4COMP             four distinct components + spatial summary
  [PASS] FZ-FIELD-PSFSW-UNIT              psfsw unit/scope/covariance boundary frozen
  [PASS] FZ-GATE-PSFSW-EPSF               effective PSF required
  [PASS] FZ-GATE-PSFSW-COV                psfsw covariance boundary: method / no weight-derived variance / epsf present
  [PASS] FZ-GATE-PSFSW-FAILCLOSED         fail-closed whitelist; valid=false => null weight
  [PASS] FZ-GATE-PSFSW-NOKEYS             psfsw forbidden keys present
  [PASS] FZ-PSFSW-COMMON-STAR-SET         common star set independent of frame measurement
  [PASS] FZ-FORMULA-PSFSW-COMPOSITE       group median=1, all positive, monotone S/Conc up and N/B down
  [PASS] FZ-PSFSW-RECORD                  psfsw record conforms (4 components, unit, scope, covariance, validity, no banned keys)
  -> 38/38 PASS
  mutation M-S1 target=G-STRUCT-FREEZE-INHERIT -> CAUGHT (required 19 freeze ids all inherited)
rc=2
```
- `M-S17`：
```text
== ALG-P1-001 baseline (independent Oracle + structure) ==
  [PASS] G-STRUCT-FREEZE-INHERIT          required 19 freeze ids all inherited
  [PASS] G-STRUCT-UNIT-LAW                variance=signal^2, ivar=1/variance, W_info=ADU^-2, psfsw=1
  [PASS] G-STRUCT-FORBIDDEN-TOKENS        forbidden token set complete
  [PASS] G-STRUCT-ALG-AREAS               four algorithm areas present
  [PASS] G-STRUCT-GATES                   frozen gates present in gates set
  [PASS] G-MODE-PRODUCTION                production 3 modes; psf_snr_power deferred; legacy/auto forbidden
  [PASS] G-SO-REGISTERED                  SO-01..07 registered only, not signed
  [PASS] G-CTRL-REGISTERED                controller-level items registered only
  [PASS] G-REGISTRY-COVERS-MATRIX         test-matrix/spec gate ids subset of executed CHECKS (missing tm=[] spec=[])
  [PASS] CAL-COV-REPRESENTATION           shared master: 3 allowed representations
  [PASS] CAL-NO-CLIP                      no clipping / no pedestal
  [PASS] ADJ-OBS-01-SHARED                shared master detected: var_joint/var_naive=2.600000 > 1
  [PASS] CAL-COV-FORMULA                  cal per-pixel variance == J C J^T (same master folded as (1-alpha)^2)
  [PASS] CAL-UNIT                         variance unit == (signal unit)^2
  [PASS] FZ-FORMULA-WINFO                 F_hat=Q/W==GLS; Var=1/W=c^T C c; W(2a)/W(a)=4
  [PASS] FZ-COND-WHITENOISE               W=a^2 sum P^2/sigma^2 == a^2/(sigma^2 A_NEA); A_NEA=1/sum P^2
  [PASS] FZ-WINFO-DIAG-APPROX             approx diagonal must report c~^T C c~ and deviation
  [PASS] FZ-GATE-MEDIAN-SNR               W_info forbidden sources complete
  [PASS] FZ-DEGRADE-SCALAR                scalar downgrade needs dual gate + p05/p50/p95 + domain
  [PASS] FZ-GATE-CONST-SB                 S_p=B0 for pixfrac in {0.25,0.5,0.8,1.0}, |S/B0-1|<1e-3
  [PASS] FZ-FORMULA-DRIZZLE-VAR           variance_p=sum c_jp^2 v_j and alpha^2 scale law exact
  [PASS] FZ-COND-FLUX-CONSERV             Phi_out=sum_p S_p D_p = pixfrac^2 * sum_j x_j exact
  [PASS] FZ-DRZ-CORRELATION               Cov(S_p,S_q)=sum_j c_jp c_jq v_j with non-zero off-diagonal
  [PASS] FZ-DRZ-APERTURE                  aperture exact variance strictly greater than diagonal-only lower bound
  [PASS] FZ-FORMULA-COV-PROP              C_out = R C_in R^T: Cov(S_p,S_q)=sum_j c_jp c_jq v_j with non-zero off-diagonal | aperture exact variance strictly greater than diagonal-only lower bound
  [PASS] FZ-GATE-PARENT-VAR               parent variance: lower_bound + kernel + deficit gate
  [PASS] FZ-DRZ-PARENT-NUMERIC            parent Var is a lower bound; deficit=0.500000 (threshold pending SO-07)
  [PASS] FZ-DRZ-FLUX-PROV                 flux factor in provenance
  [PASS] FZ-DRZ-SB-DEF                    SB-preserving normalization declared
  [PASS] FZ-FIELD-PSFSW-4COMP             four distinct components + spatial summary
  [PASS] FZ-FIELD-PSFSW-UNIT              psfsw unit/scope/covariance boundary frozen
  [PASS] FZ-GATE-PSFSW-EPSF               effective PSF required
  [PASS] FZ-GATE-PSFSW-COV                psfsw covariance boundary: method / no weight-derived variance / epsf present
  [PASS] FZ-GATE-PSFSW-FAILCLOSED         fail-closed whitelist; valid=false => null weight
  [PASS] FZ-GATE-PSFSW-NOKEYS             psfsw forbidden keys present
  [PASS] FZ-PSFSW-COMMON-STAR-SET         common star set independent of frame measurement
  [PASS] FZ-FORMULA-PSFSW-COMPOSITE       group median=1, all positive, monotone S/Conc up and N/B down
  [PASS] FZ-PSFSW-RECORD                  psfsw record conforms (4 components, unit, scope, covariance, validity, no banned keys)
  -> 38/38 PASS
  mutation M-S17 target=G-REGISTRY-COVERS-MATRIX -> CAUGHT (test-matrix/spec gate ids subset of executed CHECKS (missing tm=['NO-SUCH-GATE'] spec=[]))
rc=2
```
- `NO-SUCH`：
```text
== ALG-P1-001 baseline (independent Oracle + structure) ==
  [PASS] G-STRUCT-FREEZE-INHERIT          required 19 freeze ids all inherited
  [PASS] G-STRUCT-UNIT-LAW                variance=signal^2, ivar=1/variance, W_info=ADU^-2, psfsw=1
  [PASS] G-STRUCT-FORBIDDEN-TOKENS        forbidden token set complete
  [PASS] G-STRUCT-ALG-AREAS               four algorithm areas present
  [PASS] G-STRUCT-GATES                   frozen gates present in gates set
  [PASS] G-MODE-PRODUCTION                production 3 modes; psf_snr_power deferred; legacy/auto forbidden
  [PASS] G-SO-REGISTERED                  SO-01..07 registered only, not signed
  [PASS] G-CTRL-REGISTERED                controller-level items registered only
  [PASS] G-REGISTRY-COVERS-MATRIX         test-matrix/spec gate ids subset of executed CHECKS (missing tm=[] spec=[])
  [PASS] CAL-COV-REPRESENTATION           shared master: 3 allowed representations
  [PASS] CAL-NO-CLIP                      no clipping / no pedestal
  [PASS] ADJ-OBS-01-SHARED                shared master detected: var_joint/var_naive=2.600000 > 1
  [PASS] CAL-COV-FORMULA                  cal per-pixel variance == J C J^T (same master folded as (1-alpha)^2)
  [PASS] CAL-UNIT                         variance unit == (signal unit)^2
  [PASS] FZ-FORMULA-WINFO                 F_hat=Q/W==GLS; Var=1/W=c^T C c; W(2a)/W(a)=4
  [PASS] FZ-COND-WHITENOISE               W=a^2 sum P^2/sigma^2 == a^2/(sigma^2 A_NEA); A_NEA=1/sum P^2
  [PASS] FZ-WINFO-DIAG-APPROX             approx diagonal must report c~^T C c~ and deviation
  [PASS] FZ-GATE-MEDIAN-SNR               W_info forbidden sources complete
  [PASS] FZ-DEGRADE-SCALAR                scalar downgrade needs dual gate + p05/p50/p95 + domain
  [PASS] FZ-GATE-CONST-SB                 S_p=B0 for pixfrac in {0.25,0.5,0.8,1.0}, |S/B0-1|<1e-3
  [PASS] FZ-FORMULA-DRIZZLE-VAR           variance_p=sum c_jp^2 v_j and alpha^2 scale law exact
  [PASS] FZ-COND-FLUX-CONSERV             Phi_out=sum_p S_p D_p = pixfrac^2 * sum_j x_j exact
  [PASS] FZ-DRZ-CORRELATION               Cov(S_p,S_q)=sum_j c_jp c_jq v_j with non-zero off-diagonal
  [PASS] FZ-DRZ-APERTURE                  aperture exact variance strictly greater than diagonal-only lower bound
  [PASS] FZ-FORMULA-COV-PROP              C_out = R C_in R^T: Cov(S_p,S_q)=sum_j c_jp c_jq v_j with non-zero off-diagonal | aperture exact variance strictly greater than diagonal-only lower bound
  [PASS] FZ-GATE-PARENT-VAR               parent variance: lower_bound + kernel + deficit gate
  [PASS] FZ-DRZ-PARENT-NUMERIC            parent Var is a lower bound; deficit=0.500000 (threshold pending SO-07)
  [PASS] FZ-DRZ-FLUX-PROV                 flux factor in provenance
  [PASS] FZ-DRZ-SB-DEF                    SB-preserving normalization declared
  [PASS] FZ-FIELD-PSFSW-4COMP             four distinct components + spatial summary
  [PASS] FZ-FIELD-PSFSW-UNIT              psfsw unit/scope/covariance boundary frozen
  [PASS] FZ-GATE-PSFSW-EPSF               effective PSF required
  [PASS] FZ-GATE-PSFSW-COV                psfsw covariance boundary: method / no weight-derived variance / epsf present
  [PASS] FZ-GATE-PSFSW-FAILCLOSED         fail-closed whitelist; valid=false => null weight
  [PASS] FZ-GATE-PSFSW-NOKEYS             psfsw forbidden keys present
  [PASS] FZ-PSFSW-COMMON-STAR-SET         common star set independent of frame measurement
  [PASS] FZ-FORMULA-PSFSW-COMPOSITE       group median=1, all positive, monotone S/Conc up and N/B down
  [PASS] FZ-PSFSW-RECORD                  psfsw record conforms (4 components, unit, scope, covariance, validity, no banned keys)
  -> 38/38 PASS
FAIL: unknown mutation NO-SUCH
rc=1
```

## 6. 一致性闭合（规格 / 测试矩阵 / 验证器）

门 `G-REGISTRY-COVERS-MATRIX` 机器校验：`alg_p1_001_test_matrix.json` 每行 `gate` 与 `alg_p1_001_spec.json` 的 gates 集合都必须是验证器已执行门集合的子集；
覆盖缺口（如规格声明了门但无 checker）由 `M-S17` 注入后必红。这样「规格—测试矩阵—验证器」三者一致本身成为一条门。

## 7. 独立性声明（防同实现自证）

- 数值 Oracle 由 `numpy` 按冻结公式独立重推导（常量面亮度、方差/缩放律、通量因子、相关二次型、J C Jᵀ、GLS 与 A_NEA、PSFSW 复合与归一）；
- 不 import、不链接、不执行任何生产 C/C++ 实现或生产测试二进制，不读取生产产物作为 expected；
- 用例数门：`CHECKS` = 38（≥ 20），`MUTATIONS` = 39；零用例 / skip-only 直接 FAIL。

## 8. 写域与证据落位说明

派发通用要求「机器可读规格/证据/命令日志写入 run/ 工作区」，但本任务卡与 TASK_MANIFEST 的 `write_scope` 仅为 `docs/algorithms/v6/phase1/`，纪律写明「只写 write_scope，禁止写任何其他路径」。因此未写 run/；机器可读规格、证据与命令日志均落在本目录内。如控制器要求 run/ 版本，请在集成时另行授权。

## 9. 声明

- 未 commit / push / git add；未建分支或 worktree；未 stash/reset/clean/rebase；git 仅只读使用；
- 仅写 `docs/algorithms/v6/phase1/`；未改 `docs/science/*.md`、`docs/owner/**`、`docs/design/**`、`docs/references/**`、生产源码、CI；
- 未修改任何冻结公式、容差或冻结门；SO-01..07 只登记不擅改；F1 / AR-033 / AR-034 只登记不裁决；
- 未派生子代理；未宣布任何发布或 VERIFIED 状态。
