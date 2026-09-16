> **DOC-001 溯源注记（2026-09-16）**：本文为 V6 产品族冻结/设计档案（上一轮治理产物），因仍被活动合同引用而保留在活动索引；文中 工程控制/旧 V6 控制包（ROOT-007 已删除）/** 等旧控制包路径为该轮任务溯源，该控制包已由 ROOT-007 删除，不作现状引用。文中「宪章 `ASTROCS-CONSTITUTION-001` §x.y」引用同属该轮历史溯源——该宪章（`ASTROCS_PROJECT_CONSTITUTION.md`）已废止（ROOT-007 删除），**不构成现行依据**；现行权威见 `ASTROCS_DESIGN.md` §0 权威链。

# 07 — provenance 对象 schema（最小集 / 共享系统项 / k_corr / 降级 / unavailable）

上位锚：`FZ-PROV-MINIMAL-SET`、`FZ-PROV-SHARED-SYSTEMATIC`、`FZ-PROV-KCORR`、`FZ-DEGRADE-SCALAR`、`FZ-COND-FLUX-CONSERV`、`FZ-BUNIT-SEMANTICS`；
宪章 §4.3:123-125；UNIFIED §9；DATA_SEMANTICS §30.3/§30.4；ADJ-GEN-03；DESIGN-P3 §5。
机器：`contracts/proposals/v6/data/astrocs.v6.provenance.v1.schema.json`（`provenance.v1`）；正例 `examples/provenance.example.json`。

## 1. 最小集（`FZ-PROV-MINIMAL-SET`，缺键即 REJECT）

| 字段 | 语义 | 锚 |
|---|---|---|
| `product.type_id` + `product.schema_version` | 产品类型与 schema 版本 | 宪章 §4.3 |
| `software_sha` | 软件完整 SHA（40 hex） | 宪章 §4.3 |
| `run_id` | run 标识 | 宪章 §4.3 |
| `input_product_hashes[]` | 输入产品哈希 | 宪章 §4.3 |
| `config_hash` | 科学配置哈希 | 宪章 §4.3；§4.2 |
| `units.{bunit,pixel_semantics,pixel_area_power,target_pixel_area}` | 单位 + pixel 语义 | `FZ-BUNIT-SEMANTICS` |
| `coordinate.frame` | 坐标 frame | 宪章 §4.3 |
| `pixel_semantics` / `sampling` | 像素/采样语义 | 宪章 §4.3；UNIFIED §9 |
| `algorithm_ids[]` | 算法 ID | 宪章 §4.3；§9 |
| `module` / `provider` | 模块 build ID / 实际 provider | 宪章 §4.3；§10.2 |
| `approximations[]` | 近似清单 | PROJECT_SPEC §3 |
| `degradations[]` | 降级记录（见 §4） | `FZ-DEGRADE-SCALAR` |
| `normalization_version` | Drizzle/psfsw 归一版本 | SC-ADJ-F02 |
| `weight_mode_version` | 权重模式版本 | `FZ-FIELD-WEIGHTMODE` |
| `correlation_summary` | 相关核/低秩/master 摘要 | `FZ-PROV-SHARED-SYSTEMATIC` |
| `flux_conservation_factor` | pixfrac² 条件项 | `FZ-COND-FLUX-CONSERV` |
| `k_corr` | 定义+适用域+值+标定 | `FZ-PROV-KCORR` |
| `generated_utc` + `output_hash` | 时间与输出哈希 | 宪章 §4.3 |

## 2. 共享系统项表示（`FZ-PROV-SHARED-SYSTEMATIC`，三选一）

| 形式 | 字段 | 含义 |
|---|---|---|
| (a) 低秩因子 | `correlation_summary.representation=low_rank_factors` + `low_rank_ref` | `C_shared = L L^T` |
| (b) 相关核 | `representation=correlation_kernel` + `kernel_id`/`scale`/`rho_summary` | `sigma/scale + kernel` |
| (c) 共同 master | `representation=common_master` + `master_id`/`alpha_m` | 共同 master ID + 强度参数 |

任一组装必须进入 covariance 传播链；无法表示 → `unavailable` 或系统误差预算（进 validity/quality）。
**禁止**按独立随机项处理（联合 vs 朴素方差比 > 1 须检出；实测 3.48×）。

## 3. k_corr（`FZ-PROV-KCORR`）

```text
k_corr = Var(median) / [ pi * sigma_bg^2 / (2 * N_retained) ]
适用域(必须显式): geometry / pixfrac / control patch size / 稳健估计器版本 / 是否球面
未复跑标定前: 只允许已声明域内取冻结值 1.4，禁止外推/内插到其他 pixfrac 或尺度
```

字段：`k_corr.definition`（const）、`k_corr.value`、`k_corr.domain{geometry,pixfrac,patch_size,estimator,spherical}`、
`k_corr.calibration{script,seed,calibration_run_id}`、`k_corr.lookup_table_ref`。
缺适用域、跨域外推、或令 `k_corr=1` 忽略相关 → REJECT。

## 4. 降级记录（`FZ-DEGRADE-SCALAR`）

空间量（W_info / 背景 / variance / photometric response / PSF）默认**空间模型**；压成帧级标量必须**同时**过：

1. 空间残差/趋势门；
2. 功率损失门（对最终 flux bias / variance / detection power 的损失低于阈值，阈值由 ALG 冻结）。

标量摘要必须带 `p05/p50/p95` + **最大系统偏差** + 采样覆盖 + 模型误差 + 适用域；否则存 map/model/control points。
任一不满足 → 不得先删科学信息再证明。psfsw 四分量显著空间非均匀时拆 region/tile 或拒绝标量模式。

## 5. unavailable 显式登记（`ADJ-GEN-03`；宪章 §18.3）

`unavailable.{flag,reason,scope}` 必填。`uncertainty_available=false` **不是**失败态，是显式降级；
禁止命令占位、静默缺键、空输出冒充完成。缺键/单位不可判/unavailable 无原因 → REJECT。

## 6. fail-closed 与负向门

- 最小集任一键缺失 → REJECT（`G-PROV-MINIMAL-SET`）；
- BUNIT=ADU 且无 pixel 语义声明 → REJECT（`G-BUNIT-SEMANTICS`）；
- 近似/降级未登记 → REJECT（`G-PROV-APPROX-DEGRADE`）；
- 只出对角 variance 而无 correlation_summary 表示 → REJECT（`G-SHARED-SYSTEMATIC`）；
- 缺 `flux_conservation_factor`（pixfrac<1）→ 不可用于绝对通量（`G-FLUX-CONSERV-FACTOR`）；
- k_corr 缺适用域/标定脚本/种子，或跨域外推 → REJECT（`G-KCORR-DOMAIN`/`G-KCORR-CALIBRATION`）。

## 7. 迁移建议

- 现有 Phase2 provenance 键 `ASTROCS_WEIGHT_MODE=0/1/2`（DATA_SEMANTICS §30.3）须迁移为显式 mode 字符串 + 版本；
  整数 0 一律 REJECT，1→equal、2→pixel_ivar 仅作基线对照（S1/AR-028/AR-029）。
- 现有 `ASTROCS_UNCERTAINTY_AVAILABLE` 保留为 unavailable 的落位，但须补 `reason/scope`。
- 相关核摘要与 `k_corr` 适用域在旧产品中普遍缺失：读取侧按 fail-closed 标 unavailable，不得静默补默认。
