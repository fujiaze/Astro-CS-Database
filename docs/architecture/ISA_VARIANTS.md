# ISA 变体与逐 kernel 选路

> 上游：`docs/ASTROCS_DESIGN.md` §9（CPU 后端与资源）、§8.5（模块与 ABI）

## 0 原则

- 先由 profile 证明热点，只为热点做变体；变体与 baseline 共享合同与源，禁复制漂移；
  逐 kernel 有 Oracle 与确定性判据。
- 无收益的档位**不登记变体**（`NOT_SHIPPED`），但必须留下完整测量记录（读入 `实验/` 证据面）。
- 变体一律以 **DSO**（Windows `.dll` / Linux `.so`）随安装树分发；调度器运行时检测 CPU +
  benchmark 选取，缺库回退基线；主程序保持基线指令集。

## 1 测量条件与决策

- 测量条件：Linux 开发节点（`vm-bj`，2 vCPU，AVX2+FMA+AVX512F）；
  基准程序 `eng/tests/backend/kernel_bench_main.cpp`，median-of-5 计时 × 多轮，
  baseline 取最优（对变体最保守）；变体整 TU 局部旗标，共享 `baseline_kernels_impl.inc`
  同一源（**零复制漂移**）。
- 变体实现（`lib/infrastructure/benchmark/backend_host/`）：`avx_backend.cpp`（`-mavx`，无 FMA）、
  `avx2_backend.cpp`（`-mavx2 -mfma`）、`avx512_backend.cpp`
  （`-mavx512f -mavx512bw -mavx512vl -mavx512dq`）。
- **SHIP 阈值**：受控热点增益 ≥ +10% 且方向稳定；落在 ±10% 带宽内的记 `REMEASURE`，
  交 benchmark 逐 kernel 选路。
- **现行测量口径 = AVX2+FMA 独立复测批**。各批逐 kernel ns 读数、增益、能力证明的反汇编计数
  与决策原表见测量归档 `实验/engineering-evidence/prerelease-v5/`（`ISA-001` / `ISA-002` /
  `ISA-003` 三批子目录）。本文件只承载结构决策。

| 档位 | 决策 | 依据 |
|---|---|---|
| AVX2+FMA（`avx2_backend.so`） | **SHIP** | 受控热点 calibration-pixel-transform 与 hips-bulk-transform 的增益均过阈值且方向稳定；能力证明 = 反汇编含 FMA 特征指令与 256-bit VEX，而 baseline 扫描零 VEX |
| AVX（无 FMA） | **NOT_SHIPPED** | AVX 是 AVX2+FMA 的严格指令集子集，受控热点的增益被 AVX2+FMA 严格主导 ⇒ 无独立收益 |
| AVX-512 | **NOT_SHIPPED** | 受控热点无超越 AVX2+FMA 的收益；且 AVX512F 存在已知 downclock / 功耗-频率风险 |
| drizzle-accumulate | **保持 baseline** | 该 kernel 上变体更慢（方向在两批复测中一致） |
| noise-snr-reductions / upm-spmv / integration-accumulate | **保持 baseline** | 排序型 / gather 型算子，ISA 变体收益不足 |
| BMI2 / POPCNT（整数 / 位操作） | **NOT_SHIPPED** | 无整数/位操作热点，见 `docs/architecture/ISA_BIT_MANIP_VARIANTS.md` |

- **边界（诚实登记）**：测量主机具备 AVX2，无法在「仅支持 AVX 的主机」上证明选路；
  该情形须在对应主机复测后再决定。Windows 侧能力复核由发行验证承担。

## 2 变体注册（capability）

- `avx2_backend.cpp` → `avx2_backend.so`（DSO；manifest `required = avx2 + fma`，
  sha256 入 `backends.manifest.json`）；预检保证**不支持该 ISA 的主机绝不加载/执行**。
- 逐 kernel 选路：calibration-pixel-transform / hips-bulk-transform → avx2 变体；
  drizzle-accumulate → **保持 baseline**；其余 → baseline。
  错误变体绝不入候选（hash / ABI / ISA 预检 + 逐 kernel Oracle）。
- variant Oracle：与 baseline 同公式同序（共享源）⇒ 输出只允许 FMA 舍入差
  （容差 2e-4 相对，与 Python 参考比对）；值语义不变。

## 3 ISA 污染防线

- 主 CLI / baseline TU：无 `-march` / `-mavx` 旗标（测试断言）；opcode scanner 禁 VEX/ymm/zmm
  （`eng/tests/backend/test_abi_kernels.py`）。
- 变体 TU：整 TU 局部旗标（`-mavx2 -mfma`）⇒ 反汇编必须含 VEX
  （`eng/tests/backend/test_isa_variants.py`，变体「真变体」证明）。
- Windows 变体（`/arch:AVX2`）随发行链同流程登记。

## 4 与模块边界 Oracle 的关系

- 12 kernel 的 Oracle（独立参考 + 确定性）已在模块边界合同建立；变体复用同一 Oracle
  （共享公式），变体特有仅舍入差 ⇒ 容差比对；确定性（预算 1 vs 4 逐位）对变体同样成立
  （同 impl 外层）。
