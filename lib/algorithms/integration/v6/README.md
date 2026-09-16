# lib/algorithms/integration/v6 — Phase2 三模式产品链集成层 (P2-INTEGRATE-001, Wave 8)

> 本目录是 V6 目标态 Phase2 **产品链集成**（接线）层，与同目录的 legacy 合同文件
> （`README.md`/`module.yaml`/`memory.md`，P2-INT 迁移合同）**共存不覆盖**。
> 写域：`lib/algorithms/integration/`。生产源被链接但不修改。

## 交付物

| 文件 | 说明 |
|---|---|
| `include/astrocs/v6/phase2_integrate.h` | 三模式路由 / 磁盘重开消费 / 组合 / 原子发布 / 重开校验 / UPM·REJ·SAMP 接线公共接口 |
| `src/phase2_integrate.cpp` | 上述实现（只接线，不含新科学公式） |
| `CMakeLists.txt` | 自包含静态库 `astrocs_v6_phase2_integrate`（可独立构建） |

## 接线来源（Wave 5/7 交付，本层只调用不修改）

- `lib/algorithms/integration/v6_phase1`（P1-INTEGRATE-001）：`open_phase1_product`、`consume_phase1_group_for_psfsw`
- `lib/algorithms/coverage/src/upm.cpp`（IMPL-P2-UPM-001）：`p2_upm_ma_*`、`p2_upm_control_variance`
- `lib/algorithms/coverage/src/rejection.cpp`（IMPL-P2-REJ-001）：`p2_reject_classify`、`p2_reject_plan_resolve`、
  `p2_reject_plan_thresholds_inherited`、`p2_rejection_weight_surface_guard`
- `lib/algorithms/coverage/src/{sampler,coverage}.cpp`（IMPL-P2-SAMP-001）：`p2_spatial_model_eval|summary`、
  `p2_scalar_degrade_gate`、`p2_coverage_support_classify`、`p2_weight_mode_check`、
  `p2_weight_source_token_reject`
- `lib/{dynamic_psf,snr_estimator,photometric_calib}`（IMPL-P1-PSFW-001）：
  `conventional_coadd`、`propagate_covariance`、`conventional_effective_psf`、
  `compute_psfsw_weights`、`validate_psfsw_record`
- `lib/infrastructure/aio/v6`（IMPL-AIO-001）：流式 FITS + DATASUM/CHECKSUM + 原子发布 + provenance + BUNIT

## 三模式（FZ-MODE-PRODUCTION）

| mode | 权威式 | covariance |
|---|---|---|
| `point_information` | `Q=ΣQ_k; W=ΣW_info,k; F=Q/W; Var=1/W`（相关帧 `W=A^T C^-1 A`） | `R C_in R^T` 实际系数传播 |
| `surface_gls` | `x=(A^T C^-1 A)^-1 A^T C^-1 d; Cov=(A^T C^-1 A)^-1` | GLS；pixel-ivar 近似过 `Var_approx/Var_GLS<=1+0.05` 门（硬上限 1.20） |
| `psfsw_robust` | 组内 median=1 + `alpha_k(p)=W_k v_k/Σ` conventional coadd | `C_out=R C_in R^T`；**禁** `1/W_psfsw` |

`psf_snr_power` 保持 DEFERRED，不进生产路由（FZ-MODE-DEFERRED / C-004.1）。

## 构建 / 测试

```bash
cmake -S lib/algorithms/integration/v6 -B <build> && cmake --build <build>
cmake -S tests/integration/v6_p2 -B <build_it> && cmake --build <build_it>
ctest --test-dir <build_it> --output-on-failure
```

根/公共 CMakeLists 的注册归 RUNTIME-CI-001/W9（C-004.4）。

## 边界声明

- 未改 `lib/algorithms/coverage/src/{upm,rejection,sampler,coverage}.cpp` 及其头、`lib/phase1/**`、
  `lib/infrastructure/aio/**`、`contracts/**`、`docs/**`、根/公共 CMakeLists。
- `lib/algorithms/coverage/src/integrate.cpp`/`block.cpp` 的 legacy 生产 API 未修改：V6 组合核心落在本目录，
  避免向根构建面注入新 `-I` 依赖（CMake 归属 W9）。
- 不发布、不改冻结公式/容差/门；49 条 PENDING 保持 fail-closed。
