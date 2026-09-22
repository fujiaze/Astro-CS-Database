# P1-INTEGRATE-001 接线证据（Phase1 单帧产品链集成）

- 任务: `P1-INTEGRATE-001`（Wave 7），write_scope = `lib/phase1/`、`lib/phase1_session/`、`eng/tests/integration/v6_p1/`
- 基线 HEAD（派发时）: `95703e639eb0056d2702e407e01f453fd353010d`
- 复核 HEAD（结束时）: `a689eff232e8894471c944cd1149446144b89cb9`
  —— 控制器在本任务运行期间并行提交了 `P3-INTEGRATE-001`（`a689eff2`）；
  该提交未触及本任务写域（只含 `lib/phase3_session/`、`eng/tests/integration/v6_p3/`、台账）。
- 接线对象（Wave 5，只调用不修改）:
  - `lib/algorithms/calibration/include/astrocs/calibration/v6_calibration_covariance.h`
  - `lib/algorithms/psf/include/astrocs/v6/psf_information.h`、`lib/algorithms/noise_snr/include/astrocs/v6/information_weight.h`、`lib/algorithms/photometry/include/astrocs/v6/psfsw.h`
  - `lib/algorithms/drizzle/healpix_drizzle/{v6_drizzle_science.h,v6_spherical_overlap.h}`
  - `lib/infrastructure/aio/v6/include/astro/aio/*.h`（FITS/provenance/BUNIT/原子发布/HiPS manifest）

## 1. 交付文件（全部在 write_scope 内；无已归属底层模块改动）

| 文件 | 作用 |
|---|---|
| `lib/algorithms/integration/v6_phase1/include/astrocs/v6/phase1_product.h` | 单帧产品装配/重开/Phase2 消费面 API |
| `lib/algorithms/integration/v6_phase1/src/phase1_product.cpp` | 实现：校准→PSF/W_info→PSFSW→球面 Drizzle→FITS/记录→原子发布→重开验证 |
| `lib/algorithms/integration/v6_phase1/CMakeLists.txt` | 静态库 `astrocs_v6_phase1_product`（自注册，不改根/公共 CMake，C-004.4） |
| `lib/algorithms/integration/v6_phase1/README.md` | 接线契约与冻结口径 |
| `eng/tests/integration/v6_p1/CMakeLists.txt` | 自注册 ctest 目标（独立可配置） |
| `eng/tests/integration/v6_p1/v6_p1_integrate_test.cpp` | 端点测试：写盘/重开/组消费/24 条负向/无半成品 |
| `eng/tests/integration/v6_p1/oracle/v6_p1_reopen_oracle.py` | 独立重开 Oracle（生产 schema + 独立 SHA/BUNIT + 独立组归一 + 自检） |
| `eng/tests/integration/v6_p1/README.md`、`eng/tests/integration/v6_p1/EVIDENCE.md` | 说明与证据 |
| `run/v6/P1-INTEGRATE-001/logs/*.log` | 构建/测试/证据日志（run/ 为工作域） |

## 2. 接线要点与冻结节点

| 冻结节点 | 接线落点 |
|---|---|
| FZ-FORMULA-WINFO / Q / FHAT | `p1psfw::w_info_diagonal` + `combine_point_estimates`：实测 W_info=0.058889 ADU^-2、Q=58.888889 ADU^-1、flux=1000.0 ADU、Var(F)=16.981132 ADU^2，且 Var·W_info=1.0（rel<1e-9） |
| FZ-COND-WHITENOISE | 对角 C（`sigma2` ADU^2）且声明 ⇒ provenance 白噪近似 applied=true + 三 condition=true |
| FZ-FIELD-PSFSW-4COMP / UNIT / FZ-FORMULA-PSFSW-COMPOSITE | `extract_psfsw_components` 四分量（S/N/B=ADU、Conc=ADU/px^2）+ `compute_psfsw_weights` 未归一 Wt。**PSFSW-RETIRE-03 产品合同收口**：退役对象声明（`units.psfsw_robust_weight` / `psfsw.weight_mode="psfsw_robust"` / `psfsw.weight.kind="psfsw_robust_weight"`）已从产品 schema 的 required 移出 ⇒ 新写出的产品**不再携带**；旧产品仍携带时按退役 canonical 形状（`kind=psfsw_robust_weight`/`units="1"`/`group_normalized=true`/`scope=group`/`median_target=1.0`/**weight_value=null**）判定、登记并交消费面 fail-closed（`v6_p1_positive` 的 legacy 块锁定该路径） |
| OI-01（OPEN，fail-closed） | `phase1_extensions.group_normalization_deferred_to="phase2"` + `open_items=["OI-01"]`；单帧不产伪归一权重 |
| FZ-GATE-PSFSW-COV | `drizzle_covariance.representation=diagonal_variance_plus_correlation_kernel`（由真实算子非对角 Cov 计算 ρ）；`variance_from=actual_combination_coefficients`，禁 `weight/median_snr/support/coverage/fwhm` |
| FZ-GATE-PSFSW-EPSF | `conventional_effective_psf` 出 `effective_psf_id`+peak 归一+profile 值+per-frame kernel transfer；只给 FWHM 标量即 REJECT |
| FZ-FORMULA-DRIZZLE-SB/VAR | `build_operator_from_sources`（真实球面 overlap）+ `signal_sb/variance_sb/ivar_sb`；门 `gate_sb_definition/gate_variance_identity` 实测 pass |
| FZ-COND-FLUX-CONSERV | provenance `flux_conservation_factor=pixfrac^2`（pixfrac=0.8 ⇒ 0.64）；pixfrac<1 缺因子重开必红 |
| FZ-FORMULA-COV-PROP / FZ-GATE-PARENT-VAR | `gate_covariance_propagation` pass；`diagonal_approximation.is_lower_bound=true`/`use_for_aperture=false` + deficit=(exact-diag)/exact（记录值） |
| FZ-UNIT-* / FZ-BUNIT-SEMANTICS / FZ-P3-BUNIT-QUADRATIC | FITS 层 BUNIT=ADU/sr、px^2、ADU^2/sr^2、sr^2/ADU^2；重开逐 HDU 独立回读校验 + 二次律 |
| FZ-PROV-MINIMAL-SET / SHARED-SYSTEMATIC / KCORR | provenance 走 AIO `validate_provenance`；校准共享 bias master 走 `common_master` 通道（`representation=common_master`）；k_corr 域内冻结常数 1.4 并登记为继承（不重拟合） |
| 原子发布（ALG-P3-008 §7） | `atomic_publish_directory`（staging→fsync→rename→重开验证）；失败清 staging，目标不可见 |
| FZ-GATE-MEDIAN-SNR / C-004.2（P33 撤销） | 重开门递归扫描键 `snr_frame_coefficient/snr_coefficient/support_x_snr*/psf_snr_power/snr_frame*`，出现即红；**未引用** `lib/algorithms/noise_snr/wrapper_phase1/snr_frame_science.*` |

## 3. 构建 / 测试命令与实测（g++ 14.2.0 / cmake 3.31.6 / Linux amd64）

```bash
cd run/v6/P1-INTEGRATE-001
timeout 180 cmake -S ../../../tests/integration/v6_p1 -B build_clean -DCMAKE_BUILD_TYPE=Release   # rc=0
timeout 1200 cmake --build build_clean -j 8                                                      # rc=0（0 warning）
timeout 900 ctest --test-dir build_clean --output-on-failure                                     # rc=0
#  1/5 v6_p1_positive     Passed   POSITIVE PASS checks=64
#  2/5 v6_p1_group        Passed   GROUP PASS checks=6 w=[1.33333333333, 0.666666666667]
#  3/5 v6_p1_negative     Passed   NEGATIVE PASS detected=24/24 checks=25
#  4/5 v6_p1_halfproduct  Passed   HALFPRODUCT PASS checks=3
#  5/5 v6_p1_schema_oracle Passed  ORACLE_PASS checks=68 mutations_caught=6/6
#  100% tests passed, 0 tests failed out of 5
```

- `-Wall -Wextra` 复建：本任务源 0 warning，5/5 PASS。
- 确定性：独立两次写入 `frame_a.p1` 的 `science.fits` 与 `phase1_product.json` 逐字节一致
  （sha256 `0739f587…` / `80ea6410…`）。

## 4. 端到端产物与重开证据

磁盘结构：`<dir>/science.fits`（PRIMARY=SIGNAL + SUPPORT/VARIANCE/IVAR）+ `<dir>/phase1_product.json`。

- 重开校验：FITS CHECKSUM/DATASUM + SHA-256 与 manifest/provenance 相符 + 逐 HDU BUNIT + 单位冻结串 + 二次律 + psfsw 四分量 + provenance 最小集 + 禁诊断来源（**现行产品不携带退役对象声明**；携带者按退役/迁移情形登记）。
- Phase2 消费面（`consume_phase1_group_for_psfsw`）：**整体退役、无条件 fail-closed**（FZ-MODE-RETIRED + 迁移提示，不产出 w_psfsw）——该面的唯一产物就是退役对象 psfsw_robust_weight 的组内归一权重，`ASTROCS_DESIGN.md` §3.1 禁止 PSF 质量代理进入科学叠加权重。Oracle 侧保留独立复算作**非空洞守卫**（fixture 四分量本身能算出合法组内归一 ⇒ 拒绝是策略拒绝而非数据退化）。
- 无半成品：几何闭合不可能（closure tol=0）时发布失败，目标目录不存在且无 `.staging.tmp-` 残留。

## 5. 负向测试（24 条，全部红）

fits_bit_flip / weight_kind_ivar / group_normalized_false / median_target_2 /
signal_unit_adu / flux_factor_removed / provenance_output_hash_removed /
winfo_units_wrong / valid_false_weight_present / variance_from_weight /
diagonal_claims_exact / normalization_not_deferred / manifest_hash_changed /
concentration_units_legacy / third_vocabulary_token / correlation_rep_bogus /
component_p05_gt_p95 / component_count_3 / kcorr_one / software_sha_short /
manifest_file_hash_changed / bunit_card_changed / **p33_coefficient_reintroduced** /
**psf_snr_power_reenabled**。

每条断言：`open_phase1_product` 返回 `ok=false`；同套未篡改正控制必绿。

## 6. 未决风险与需控制器裁决事项

1. **生产 schema 未约束 `quantity.units`**（finding，非本任务写域）：
   `astrocs.v6.point-information.v1.schema.json#/$defs/quantity` 的 `units` 仅
   `type:string`，故 `W_info.units="ADU^-1"` 仍过 schema（Oracle 自检因此改为
   校验 `authoritative_formula` const）。冻结单位串一致性由本层 C++ 重开门补齐，
   但机器 schema 门有缺口，建议 W6/W12 收紧为 `const/enum`。
2. **校准 covariance 与 `covariance.v1` 的表示枚举张力**：
   `covariance.v1` 在 `representation="diagonal_variance"` 时强制
   `correlation_kernel` 或 `operator_descriptor`（`kind` 枚举仅
   drizzle_forward/phase2_combination/phase3_resample，无"校准 Jacobian"）。本任务以真实
   共享 master（`common_master`）落地规避；若校准无共享项（纯对角），需 owner 就
   `operator_descriptor.kind` 是否容纳校准算子给出裁决。
3. **构建面欠账（AR-033 / W9）**：`astrocs_v6_phase1_product` 直接把 Wave 5 源编入
   静态库（与 `eng/tests/unit/v6_*` 各自编译同源）。W9 将新生产源接入生产库时，请
   决定复用现有生产 target 还是保留本独立 target，避免重复编译/重复符号。
4. **`k_corr` 对本帧产品不适用但 schema 必填**：Phase1 无 UPM，
   `provenance.k_corr` 仍为生产 schema 必填项；本层按 FZ-PROV-KCORR
   "未复跑前域内用 1.4"登记继承冻结常数（并写入 `approximations`），未伪造 MC 标定。
   建议 owner 明确 Phase1 是否应豁免 `k_corr`。
