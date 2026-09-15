# V6 Phase3 科学重采样传播实现（IMPL-P3-RSMP-001）

> 任务：IMPL-P3-RSMP-001（Wave 5，depends_on CONTRACT-FREEZE-001）
> 写域：`lib/phase3_rsmp/`、`tests/unit/v6_rsmp/`（本文件与生产源、共址测试同域）
> 基线 HEAD：`44e1cb65`（开工时 `git rev-parse HEAD` 复核；未 commit/push）
> 上位：`docs/algorithms/v6/phase3/ALG-P3-001_{SPEC,KERNEL_REGISTRY,VERIFICATION}.md`、
> `docs/contracts/v6/frozen/*`、`docs/algorithms/v6/frozen/*`、`docs/science/v6/phase3/PHASE3_PROPAGATION_REVIEW.md`

## 1. 生产面

| 文件 | 内容 |
|---|---|
| `p3_rsmp.h` | 公共声明：模式/BUNIT/采样核 registry/线性算子/covariance/QW/12 门 |
| `p3_rsmp_units.cpp` | 冻结单位表、BUNIT 二次律、`FZ-BUNIT-SEMANTICS` 可判性、mode 词表 |
| `p3_rsmp_kernel_registry.cpp` | 采样核 registry（nearest/bilinear_4quad/area_overlap_exact/bicubic/lanczos）+ 注册结构门 + 生产准入 |
| `p3_rsmp_operator.cpp` | `R_ij=a_ij/Omega'_i`（行归一）、`S_ij=a_ij/Omega_j`（列归一）、跨 tile 邻域生成、缺 tile fail-closed |
| `p3_rsmp_covariance.cpp` | `C_y = R C_x R^T`（唯一方差来源）、相关核、Cholesky（禁伪逆静默） |
| `p3_rsmp_propagation.cpp` | 三模式传播：`y=R x`、`f=S d`、`pi=S p`、`Q=a pi^T C_y^-1 f`、`W=a^2 pi^T C_y^-1 pi`（输出帧重算） |
| `p3_rsmp_failclosed.cpp` | 12 条模式门 + kernel/covariance/QW/epsf/provenance 扩展门、禁止词表 |

## 2. 冻结节点对应（逐条）

- `FZ-P3-MODES`：`parse_mode` 仅接受三模式；legacy `auto/support_x_snr2/0/1/2` 与 `psf_snr_power` → `Reject`（C-004.1）。
- `FZ-P3-FAILCLOSED`：`G-P3-SB-01..04`、`G-P3-PSF-01..06`、`G-P3-VIS-01`、`G-P3-GLB-01`（12 门）。
- `FZ-P3-QW-RECOMPUTE`：`pi=S p`；`Q/W` 在输出帧以完整 `C_y` 重算；禁 `input_qw_resampled`/`w_from_sum_input`/`upstream_w_recomputed`/对角化未审计。
- `FZ-P3-KERNEL-REGISTRY`：`bilinear_4quad` = registered_with_oracle（误差界 `(h²/8)(max|Fxx|+max|Fyy|)`、边界 MissingIsNaN、独立 Oracle 证据）；生产科学默认 = `bilinear_area_overlap_exact`；`nearest` 仅离散/诊断/显式选择；`bicubic/lanczos` requires_registration。
- `FZ-P3-BUNIT-QUADRATIC`：`var = signal²`、`ivar = 1/variance`；`G-P3-SB-03`。
- `FZ-FORMULA-COV-PROP`：唯一方差来源 `C_y=R C_x R^T`；无任何权重标量反推路径。
- `CF-T-P3-CORR-EPSILON`（OPEN / SO-07）：`GateConfig::epsilon_corr_ratified=false` 缺省；近似相关核面 → `Unavailable`；本实现不自行定值。
- `FZ-PROV-MINIMAL-SET`：`G-P3-PROV-01`（缺键/unavailable 无原因 → Reject）。

## 3. 单位（冻结表）

`signal_sb=ADU/px²`、`sb_variance_out=ADU²/px⁴`、`sb_ivar_out=px⁴/ADU²`、`W_info=ADU⁻²`、`Q=ADU⁻¹`、`flux=ADU`、`psfsw=1`、`phase3_var_out=(signal BUNIT)²`。

## 4. 独立 Oracle 与负向门

- Oracle：`tests/unit/v6_p3_rsmp/p3_rsmp_oracle.h`（仅标准库，不 include/不调用生产实现）：
  解析双线性误差界、显式稠密矩阵转写、固定种子 MC（`SEED=20260915`）、独立高斯消元解。
- 共址测试：`p3_rsmp_core_test`（101 checks）、`p3_rsmp_oracle_test`（36）、`p3_rsmp_gate_test`（127）。
- 负向 mutation 驱动：`tests/unit/v6_p3_rsmp/run_mutations.py` —— 对生产源影子副本注入 20 条违反冻结的实现，断言测试变红。

## 5. 构建与验证（不修改根构建面）

```text
bash tests/unit/v6_p3_rsmp/run_verification.sh      # 单一 rc；日志 run/v6/p3-rsmp/logs/
cmake -S tests/unit/v6_p3_rsmp -B run/v6/p3-rsmp/build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build run/v6/p3-rsmp/build -j 8
ctest --test-dir run/v6/p3-rsmp/build --output-on-failure
python3 tests/unit/v6_p3_rsmp/run_mutations.py
```

根 `CMakeLists.txt`/`tests/unit/CMakeLists.txt` 非本任务写域（C-004.4）；注册由控制器统一以独立集成提交完成。
`lib/phase3_rsmp/CMakeLists.txt` 与 `tests/unit/v6_p3_rsmp/CMakeLists.txt` 为本模块的 add_subdirectory 面。

## 6. 未决风险 / 需裁决

- `CF-T-P3-CORR-EPSILON`（SO-07） 数值未冻结：近似相关核 covariance 面保持 fail-closed（UNAVAILABLE）。
- `QF-G-INJ-07`（Phase3 flux 恢复门 2%） 为 OPEN：本任务不将其作为生产门，仅在测试中用统计 MC 容差（非冻结科学阈值）。
- `CTRL-AR033` 根构建面注册归控制器。
