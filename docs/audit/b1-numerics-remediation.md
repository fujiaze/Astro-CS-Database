# B1｜共享严格数值原语：修复、证明与独立反例

> 状态：**实现已修改并由独立精确有理 Oracle 检验；这不是全链科学认证。**
> 基线：`rebuild/acsd-v2@e2df158bb`（2026-11-10 拉取后的 HEAD）。
> 变更文件：`src/numerics/interval.cpp`、`src/numerics/linalg.cpp`、`CMakeLists.txt`、`validation/lead/interval_oracle.cpp`。
> 依据：[NUMERICS 标准](../engineering/rebuild/standards/NUMERICS.md)；审查对象 [IMPLEMENTATION-STATUS](IMPLEMENTATION-STATUS.md) N-01..N-06。
> 本文只讨论数值原语。上游 WCS/geometry/PSF/measure 的证书是否成立，必须在其各自数据边上重新审查。

## 1. 修复前的实际缺陷（不是“测试不通过”）

### N-01 四则运算的“有向舍入”是空的

旧代码：

```cpp
double down(double value) { RoundingGuard guard(FE_DOWNWARD); return value; }
...
return Interval(down(a.lower + b.lower), up(a.upper + b.upper));
```

C++ 先求值实参 `a.lower + b.lower`，此时仍是 round-to-nearest；进入 `down()` 后只把已经舍入好的 `double` 原样返回。乘法先算四个乘积再 `down(min)` 同样无效。因此 `kDirected == true` 与 `interval_primitive_status() == directed_rounding` 是**假证书**。

反例（旧实现）：binary64 的 `0.1 + 0.2` 精确和是
`0.3000000000000000166533453693773481063544750213623046875`，
round-to-nearest 结果是 `0.3000000000000000444089209850062616169452667236328125`。
旧区间为 `[0.30000000000000004, 0.30000000000000004]`，不包含精确和。

### N-02 sin/cos 的极值判定只枚举 `k=-8..8`

`sin([1000,1002])` 的极大值 1 位于 `π/2 + 318·π ≈ 1001.57`，旧循环最多到 `k=8`，漏掉该极值。旧下界/上界只由端点 `sin(1000), sin(1002)` 给出，不含 1。

### N-03 一般幂只取交叉两个端点

旧实现对 `base.lower > 0` 只算 `pow(base.lower, exponent.upper)` 与 `pow(base.upper, exponent.lower)`。`[2,3]^[2,3]` 的真正范围含 `4` 与 `27`，但旧结果可能给出 `pow(2,3)=8, pow(3,2)=9` 的包络，漏掉两端。负底/零底/整数指数也没有定义域处理。

### N-04 atan2 四角采样漏负实轴切线

`atan2` 在负实轴上不连续。对 `y∈[-1,0]`、`x∈[-2,-0.5]`，角度可从 `-π` 任接近；四角只给出 `-153.4°` 到 `+180°`（正零给 +π），漏掉 `-π` 一侧。`x∈[-2,-0.5]`、`y∈[0,1]` 时上界为 +π，下界由角点给出，旧实现尚可；但只要 `y` 从下方跨 0 就失败。

### N-05 线性代数“证书”字段不是向外界的

旧 `jacobi_svd` 的 `delta_u/delta_v/residual/spectral_norm/sigma_error_bound` 全用普通 double 求；`classify_rank` 用点值 threshold 和点值 bound；`least_squares` 直接 `rank_certified = svd.converged`，完全不看奇异值区间是否跨阈值。一个零列使 `U` 不正交（`delta_u=1`），`sigma_error_bound=inf`，但旧代码仍可能给 `rank_certified`。这违反 N-05 的“点残差不能改名为 certified”。

### N-06 一个全局状态掩盖原语分族能力

`interval_primitive_status()` 只有一个布尔式状态；sin/cos/pow/atan2 的能力与四则混在一个“directed_rounding”名字下。本次修复把四则/sqrt 的定向舍入与超越函数的保守界分开陈述（见 `interval_primitive_note()`），不再用一个全局 OK 认证全部数学函数。

## 2. 修复后的数学合同与实现

### 2.1 四则与 sqrt：表达式在舍入模式内求值

`src/numerics/interval.cpp:74` 起：

```cpp
double add_down(double a, double b) {
  RoundingGuard guard(FE_DOWNWARD);
  volatile double result = a + b;   // 求值发生在 FE_DOWNWARD 之下
  return result;
}
```

`+,-,*,/` 各有 down/up 版本；乘法/除法的 8 个候选各自在对应模式下求值，下界取四个 down 结果的最小值、上界取四个 up 结果的最大值。为什么这是包含：

- IEEE 754 binary64 的 `+,-,*,/` 结果是对精确实数的正确舍入；在 `FE_DOWNWARD` 下，结果 `r ≤ exact`；`FE_UPWARD` 下 `r ≥ exact`。
- 对乘/除，矩形上的极值必在四角取得（端点符号固定时双线性函数的性质）；对每个角分别向外舍入后取 min/max，得到整个矩形的外包围。多余的一侧只会更宽，不会变窄。
- 分母跨 0 返回整条实轴；NaN（如 `0·∞`、`∞-∞`）返回整条实轴，不伪造有限端点。
- 平台不支持定向模式时，`detect_directed_rounding()` 返回 false，退回“round-to-nearest 结果再各外扩 1 ulp”。一次正确舍入的误差 ≤ 0.5 ulp，外扩 1 ulp 仍包含真值；此时状态明确降为 `outward_ulp_expansion`。

编译条件：`-fno-fast-math -ffp-contract=off -frounding-math -mfpmath=sse`（仓库 CMake 已强制），并在源文件 `#error` 拒绝 `__FAST_MATH__`。这是“为什么实现满足定理”的一部分：若编译器把表达式重排/收缩，定向模式不再可观察。

### 2.2 sin/cos：全相位极值判定

`interval_sin`（`interval.cpp:357`）与 `interval_cos`：

1. 宽度 ≥ `2π`：直接返回 `[-1,1]`（包含所有值）。
2. 幅值 > `1e9`：相位无法在本精度判定，返回 `[-1,1]`，不伪造窄界。
3. 否则用 long double 端点值，并加 `1e-15·max(1,|x|)` 的保守边界（见 §4 的条件）。
4. 扫描所有可能落在区间内的极值相位：sin 的极大在 `π/2+2kπ`（即 `k·π/2, k≡1 mod 4`），极小在 `-π/2+2kπ`（`k≡3 mod 4`）；cos 的极大在 `2kπ`（`k≡0`），极小在 `π+2kπ`（`k≡2`）。k 用 `floor/ceil(x/(π/2))` 再外扩 3 个索引，吸收除法舍入；命中则把界推到 ±1。

`sin([1000,1002])` 现在返回上界 1，`sin([-1001,-999])` 返回下界 −1。

### 2.3 pow：正底统一为 `exp(Y·log(X))`

`interval_pow`（`interval.cpp:469`）：

- `base.lower > 0`：`exp(exponent · log(base))`，由已修复的 `interval_mul/log/exp` 组合；`[2,3]^[2,3]` 得 `[3.9999…, 27.0000…]`，包含 4 与 27。
- `base.lower ≥ 0` 且包含 0：`exponent.lower > 0` 时下界 0、上界 `pow(b,e_up)` 外扩；`exponent ≡ 0` 返回 1（含 0^0 约定）；负指数返回 `[0,∞)`。这些是显式声明的闭包，不是“交叉两端”。
- 负底只接受精确整数指数（|n| ≤ 2^53），按奇偶和 0 临界点处理；负指数且底含 0 判 invalid；其它返回 invalid/whole，不把实数域外结果伪装成有限。

### 2.4 atan2：原点、负实轴、四角单调段

`interval_atan2`（`interval.cpp:430`）：

- 矩形含原点（`x` 含 0 且 `y` 含 0）：方向可取遍 `(-π,π]`，返回 `[-π,π]`。
- `x.upper < 0` 且 `y` 跨 0：负实轴切线两侧分别趋于 ±π，返回 `[-π,π]`。
- `x.upper < 0` 且 `y.lower < 0 ≤ y.upper`：下界闭合到 −π（y 从下方趋 0 的极限）；`y.upper > 0` 或 `y.upper==0` 且 `y.lower<0` 时上界闭合到 +π（+0 端点给 +π）。
- 其它情形函数在矩形上连续；对固定象限 `atan2` 关于 y 单调、关于 x 单调，极值在四角取得，四角向外取值即可。

`atan2(y∈[-1,0], x∈[-2,-0.5])` 现在返回 `[-π−ε, π+ε]`，包含 −π 一侧的所有极限值。

### 2.5 linalg：向外界的缺陷/残差/秩

`src/numerics/linalg.cpp`：

- `orthogonality_defect_upper`：`||QᵀQ−I||_F` 的每个点积、平方、求和、sqrt 全部走 interval 原语，取 upper。
- `reconstruction_residual_upper`：`||A−UΣVᵀ||_F` 逐元素用 interval 求 `u·σ·v` 的和，再减 `A_ij`、平方、求和、sqrt upper。
- `sigma_error_upper`：用 Weyl 型扰动界
  `e_R + e_U·||Σ||₂·sqrt(1+δ_V) + e_V·||Σ||₂`，
  `e_U=δ_U/(1+sqrt(1−δ_U))`（δ≥1 时为 ∞），`||Σ||₂` 用 `||A||_F` 的上界（`||A||₂ ≤ ||A||_F`）。所有量已是上界，组合仍向外。
- `spectral_norm` 仍报告点值用于诊断；`sigma_error_bound` 才是证书量。
- `classify_rank`：阈值本身有不确定性（谱范数 ± bound），因此
  `threshold_lower = rank_relative·max(0, σ_max−bound)`，`threshold_upper = rank_relative·(σ_max+bound)`；
  只有 `s−bound > threshold_upper` 才计入秩，`s+bound < threshold_lower` 才判为阈值下；否则 `rank_unresolved`。`certified` 还要求 `svd.converged`、bound 有限、bound < threshold_lower。
- 零奇异值列：一侧 Jacobi 得到 `σ_j=0` 时 U 的第 j 列没有定义。新增正交补全（对标准基做两遍 Gram–Schmidt，取残差最大的基向量），使 U 仍为基。否则 `δ_U=1`、`sigma_error_bound=inf`，任何秩亏矩阵都不可能认证。补全后 `dupcols` 例给 `rank=1, certified`，奇异值 `[2.449, 0]`。
- `least_squares`：残差用 interval 求上界（不是点值）；`rank/rank_certified/rank_error` 来自 `classify_rank`；多 RHS 的 rank 必须一致，否则拒证书。

## 3. 独立证据（实验，不是证明）

### 3.1 精确有理 Oracle

`validation/lead/interval_oracle.cpp`（构建目标 `acsd_interval_oracle`，不作为门禁）：
把每个 binary64 用 `frexp` 精确转成 `cpp_int` 有理数；`+,-,*,/` 用精确有理算术求真值；sqrt 用端点平方的精确有理比较。固定种子 `20261110`，20000 组随机输入，每组检查加、减、乘、除（非零）、正数 sqrt。

```
$ ./build/science/acsd_interval_oracle
interval oracle: 20000 checks, 0 failures
```

针对性反例同时检查：`0.1+0.2` 精确和包含；`pow([2,3],[2,3])` 含 4/27；`sin([1000,1002])` 含 1；`sin([-1001,-999])` 含 −1；`atan2(y[-1,0],x[-2,-0.5])` 含 ±π；`(-2,-1)^2` 含 [1,4]；负底非整数幂被拒。

Oracle 的独立性：它不复用 `interval.cpp` 的任何算术，只用整数/有理比较。它仍**不是**对所有输入的证明：随机样本只能作为反例搜索。真正的包含性论据是 §2 的逐运算理由 + IEEE 正确舍入 + 编译器标志。

### 3.2 既有数值自测

`./build/science/acsd_numerics_selftest` 在修改后仍全过（0 failures）：Kronrod/Gauss 面板、SVD 重建、最小二乘、伪逆、underdetermined 拒绝、dyadic 符号。该自测不检查 interval 包含性，因此不能替代 §3.1。

### 3.3 秩分类探针（手工实验）

| 输入 | rank | certified | 说明 |
|---|---|---|---|
| `I₂` | 2 | yes | 奇异值 1,1，bound 2.2e−15 |
| `diag(1,1e−15)` | 1 | yes | 第二值可证低于 1e−12 阈值 |
| `diag(1,1e−12)` | 1 | **no** | 区间跨阈值 → `rank_unresolved` |
| 两列相同（3×2） | 1 | yes | 零列补全后 δ_U≈7.6e−16 |
| 2×4 宽矩阵 | 2 | yes | 转置分支正确 |

`diag(1,1e−12)` 不认证是正确行为：阈值处的谱间隙小于当前算术误差，不能凭点值宣布可估。

## 4. 尚存条件与不能声称的能力

1. **没有 MPFR/GMP 开发库**（本机只有运行时 `.so.6`，无头文件）。因此 NUMERICS 首期“MPFR 有向算术”路线未采用；四则/sqrt 走 binary64 硬件定向模式 + 运行时检测 + 编译标志，并在平台不支持时诚实降级。四则/sqrt 的包含性依赖 IEEE 754 正确舍入与 SSE 路径；已在 x86-64/glibc/GCC 14 验证，未在 Windows/MSVC 或其它架构验证。
2. **sin/cos/exp/log/atan/atan2/pow 不是定向舍入证书**。它们用 long double 计算并加 `1e-15·max(1,|x|)` 边界；该边界远大于 glibc 观测误差，但“平台 libm 误差 < 边界”是**条件假设**，不是证明。生产认证链当前只用到 `sqrt/+/-/*//`（`coordinates/tan_sip.cpp`、`coordinates/wcs.cpp`、`coordinates/chart.cpp`），未用这些超越函数；若将来上游使用它们，必须降为 conditional 或补有向多精度实现。
3. **巨大角、`|x|>1e9` 的 sin/cos 只给 `[-1,1]`**；这是有效但粗的包围，不能用于窄误差预算。
4. **`interval_intersect` 的空标记**仍是 `[1,−1]`；`sqrt` 非法域返回该标记。调用者必须检查 `lower > upper`，不能继续把反序区间当数值。
5. **谱误差界的出处尚未逐式核对**：`sigma_error_upper` 采用 Weyl/Wedin 型奇异值扰动界，Wedin 1972 (BIT 12, 99–111) 已定位但未读全文；当前引用状态是 `DERIVATION_TO_REVIEW`，源码注释已如此标注。它只用于“是否可证分离阈值”的判定，不作为已核对的文献定理。
6. **linalg 的谱误差界是保守的**：`||Σ||₂` 用 `||A||_F`，比真实谱范数大 `sqrt(min(m,n))` 量级，因此小奇异值/大条件数问题的 `rank_certified` 会比点值算法更保守，可能返回 `rank_unresolved`。这是有意的：宁可未决，不给假证书。
7. **未重跑全部依赖 case**。本次只跑了 numerics selftest、interval oracle、X01 三命令（进行中）与完整 science 构建。WCS/geometry/PSF 上游的既有 `certified` 结论必须按新原语重新审查；旧 PASS 不自动继承。

## 5. 对上游的影响（待逐项复核）

- `coordinates.wcs` 的 `Validity::certified` 现在真的依赖正确的四则/sqrt；此前该字段是空头承诺。需要重跑 WCS 正逆/导数/单射与 G01。
- `geometry.overlap` 的 dyadic ledger、`Ω/A` 端点、`sqrt` 面积界现在有正确底座；但“面积包含”仍需其自身算法证明，不是本文件自动授予。
- `sources.measure` 的 flux 区间使用 `sigma_error_bound`/`rank_certified`，现在会在秩亏/阈值未决时拒绝证书；这会把一些此前的 “certified” 变成 conditional，这是更正而不是退步。
- `psf`/`photometry` 的 QR/SVD 依赖同样适用。

## 6. 复现

```bash
cmake --build build/science -j16 --target acsd_numerics_selftest acsd_interval_oracle
./build/science/acsd_numerics_selftest
./build/science/acsd_interval_oracle
```

旧失败记录（N-01..N-06）保留在 [IMPLEMENTATION-STATUS](IMPLEMENTATION-STATUS.md)，不删除、不改写为通过。
