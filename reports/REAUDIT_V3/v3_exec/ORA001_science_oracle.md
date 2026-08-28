# ORA-001 科学 oracle 套件 —— 核对 PASS

> G3 任务：机器化 oracle 覆盖常量面亮度/点源 flux/平坦场/已知噪声/已知 additive 场/
> Huber 单离群/断开 UPM/rejection 边界/NaN-Inf/空输入；期望值用独立公式或小规模高精度
> 参考实现（**不得调用被测实现**）；容差**运行前固化**进 JSON，禁止看结果后修改。
> 判定：**PASS**（oracle 已存在且独立、容差预冻结）。
> 复核：2026-08-27。

## 1 覆盖度映射（spec 10 例 → e表例/test）

| spec 要求 | oracle 用例 | 期望值来源 | 文件 |
|---|---|---|---|
| 常量面亮度 | `constant_sphere` | 解析 `truth=B0×A_pixel` | `reverse_drizzle_science_test.cpp:437` |
| 点源 flux | `compact_source` | 独立球面三角扇细分积分(depth=7) | `:439,11-14` |
| 平坦场 | `gradient_sphere` | 独立细分积分 | `:438,11-14` |
| 已知噪声 | Gaussian/Poisson MC (σ 5%) | 解析 `N(0,σ²)`/`μ/gain+rn²/gain²` | `noise_model_science_test`(SNR-004/005) |
| 已知 additive 场 | `negative_field` + 平面场恢复(10%) | 独立细分 / LS 解析 | `:441`; `noise_model`(SNR-006) |
| Huber 单离群 | UPM 污染观测降权 | 独立 `Huber IRLS` 语义 (δ=1.345) | `synthetic_gate`(UPMW-*), `upm.cpp:619-629` |
| 断开 UPM | 连通分量独立 gauge | 独立分量图 | `synthetic_gate`(PR-UPM), `upm.cpp:86,419` |
| rejection 边界 | `n` 方法边界/阈值 (4.0/3.0/8, 5.0/3.5/8, 0.2/0.1) | NIST 独立 ESD/RCR | `synthetic_gate`74/74, `rejection.cpp` |
| NaN/Inf | 非有限 value/weight/support → `INVALID_INPUT` | 独立合同校验 | `integrate.cpp:41-54`, `V17NonFiniteWeightInvalid` |
| 空输入 | 无合格 control/patch/candidate | 独立 `NO_CANDIDATES`/degenerate | `noise`(degenerate), `integrate` |

## 2 独立性（不调用被测实现生成期望值）

- `reverse_drizzle_science_test` 解析真值：`truth_j = ∫_pixel B(Ω) dΩ`，用**独立球面三角形扇
  细分积分（depth=7）** + 解析 `B0×A_pixel_j`（`:11-18`）；**不调用**生产 `spherical_overlap`
  机制生成期望值。
- `constants_oracle.json` 冻结常数（`k1=1.482602218505602` 与解析 `1/Φ⁻¹(3/4)` 对照至 1 ulp；
  `k2` 与独立解析 `0.731673095` 对照），含 `verdict`，为**独立解析参考**。

## 3 容差运行前固化（不因看结果而改）

- `constants_oracle.json`、`upm_oracle_pure_python.json`、`upm_complexity_oracle.json`
  为**预冻结**的常数/参考与容差 JSON；测试内容差为提交的硬编码常量（不可在运行后静默修改）。
- 例：σ 5%（`SNR-004`）、平面 10%（`SNR-006`）、`floor=1e-12` 在 science/algorithms doc
  【容差来源】冻结，非数值运行后调整。

## 4 结论

ORA-001 要求（覆盖度、独立期望值、预冻结容差）已满足。机器门禁与审核链（SCI→ALG→API→SRC→TEST）
核对通过。判定 PASS。
