# ACR Work-Domain Equivalence Science (SCI-ACR-EQUIV)

> 上游：ASTROCS_DESIGN.md §1.3（非目标）、§4（normalize）

> ID: SCI-ACR-EQUIV-001  状态: FROZEN  上游: SCI-SCOPE-001 + SCI-INT/REJ/UPM/DRIZZLE  下游 ALG: ALG-ACR-EQUIV-001..  模块: acr × phase2 (acr_kernels)

## 1 目的与非目标

- **目的**：定义 CPU / GPU / 混合分块的工作域 `equiv`、数值等价边界与失败回退语义，使 `phase2` 加权叠加在任意 `ACR` 调度下科学结果一致，允许的数值差异仅来自编译器/FMA/归约阶且预冻结冻结。
- **生产地位**：`ACR` 是隔离实验，生产不可达（`ASTROCS_DESIGN.md` §1.3/§8.1）；本文件定义其工作域等价边界，供隔离实验与回归使用。
- **非目标**：不决定 `ACR` 资源调度/性能优化（见 `PERFORMANCE_MODEL.md`）；不定义 `ACR` 通用硬件画像（聚焦 `phase2` 热点）；不改变 `phase2` 权重/排异科学（`acr_kernels.cpp: CPU reference 是权威 science semantics`）。

## 2 符号表

| 符号 | 含义 | 出现位置 |
|---|---|---|
| `kOpMosaicReject` | 合成 Operation `synthetic.mosaic_reject.fp64acc` | `acr_kernels.h:phase2` |
| `inv` | `KernelInvocation`（buffers+scalars） | `acr_kernels.cpp:mosaic_reject_legacy` |
| `px` | `pixel_count`（该子域像素数） | `scalars[0]` |
| `depth` | `stack_depth`（候选深度） | `scalars[sizeof_t]` |
| `p0` | tile 内偏移（`inv.scalars`） | `scalars+p0` |
| `wmode` | 实现标量槽（分支号 0/1/2 为实现事实；权重是阶段二按该天球像素对应帧集合现场算出的派生量） | `scalars+wmode` |
| `mosaic_reject_legacy` | CPU reference launcher（逐像素 `rejection+integrate`） | `acr_kernels.cpp` |
| 逐像素 ivar 权重 | 阶段二按该天球像素对应帧集合现场算出的派生量 | `stage2_common.cpp:469` |
| `ACR-IVAR-001` | ivar 时 ACR 块禁用 → CPU canonical | `stage2_common.cpp:512-529`（`p2_acr_block_eligible`） |

## 3 物理量和单位

- 量纲逐项（**信号标度**由调用方按缓冲元素类型声明，见 §4）：
  - `values, signal`：信号标度量（生产消费点为 `ADU`；`photo_scaled_adu` 标度与其同量纲、相差逐帧乘性因子 `α`）；
  - `support`：无量纲，值域 `[0,1]`；
  - `weights, ivar`：**信号⁻²**，信号取 `ADU` 时即 **`ADU⁻²`**（`ivar = 1/variance`、`variance` 单位 `ADU²`；量纲一致条款见 `docs/science/NOISE_MODEL.md` §7「量纲一致」）；
  - `pixel_count, depth`：无量纲整数计数（`std::size_t`）；`frame_id`：`uint64`；
  - 容差/误差单位：**信号标度**，其标度依赖性与定量后果见 §9。
- **适用域**：`weights/ivar` 槽只服务 `wmode=0`（legacy `support×snr²`）ablation/诊断口径；本仓生产的逐像素 ivar 科学权重路径**禁用** ACR 块（`ACR-IVAR-001`，§4），故本条单位声明**不覆盖任何生产加权路径**。

## 4 输入有效域

- `inv.buffers` 含 `buffer0=out signal`, `buffer1=values` 必备，否则 `throw runtime_error`；`buffer2/3/4/5/6` 可空（`support/snr/out_sup/out_rej/out_valid`）。
- `scalars` 须 `px>0 && depth>0`，否则 `throw runtime_error("mosaic_reject: missing scalars")`（`acr_kernels.cpp:81-83`）。
- **标量布局（正向约束，按 `append_scalar` 紧凑追加顺序、无填充）**：10 槽、共 **60 字节**，逐槽偏移 `px@0 / depth@8 / method@16 / und_n@20 / lo@24 / hi@32 / max_it@40 / p0@44 / wmode@52 / workers@56`。注册声明的 `args.scalar_bytes` **必须等于**该布局字节数：`validate_invocation` 按**精确相等**判定（`lib/infrastructure/acr/api/kernel_registry.cpp:21`），语义不是「不小于」。
- **缺省与符号约定**：`method=1u`、`underdetermined_n=2u`、`max_iterations=8`、`p0=0`、`wmode=0`、`workers=1`；`sigma_lower=-4.0`、`sigma_upper=3.0` 的**符号位不承载语义**——CPU launcher 取 `fabs` 后写入 `plan.sigma.lower_sigma/upper_sigma`（`acr_kernels.cpp:90-99`），CUDA launcher 以 `-fabs(lo)`/`+fabs(hi)` 传入（`acr_kernels.cpp:329-330`）；两侧默认量值等价（`|lower|=4.0`、`upper=3.0`）。
- 逐像素 ivar 权重时 `ACR` 块禁用，走 CPU `canonical p2_integrate_pixel` 路径（`ACR-IVAR-001`），不进入 `GPU/Mixed` 分块。
  - **可判定条件**：`p2_acr_block_eligible(...)`（`stage2_common.cpp:512-529`）在本仓**恒返回 `false`**——冻结口径下生产只剩「逐样本逆方差」一条权重路径，而该路径按 `ACR-IVAR-001` 必须走 CPU canonical。⇒ **ACR 块在生产不可达是构造性结论**，不存在任何合法配置能进入该块（`stage2.cpp:868-870, 1001`）；ACR 等价门只在隔离实验/回归域内有意义（`ASTROCS_DESIGN.md` §1.3/§8.1）。
- `acr_route∈{auto,cpu}`，其他取值在配置解析期显式拒绝（`stage2_common.cpp:469-471`）。

## 5 连续定义

```text
工作域等价 (SCI-ACR-EQUIV-001):
  ∀ tile 子域分块 {px_i, p0_i} 满足 Σ px_i = total_pixels 且 p0_i 连续不重叠
     ACR 调度前  cpu_only(inv)  ≡ gpu_only(inv)  ≡ mixed(inv, split={px_i})
  的科学输出 {signal, support, rejection_count, status} 在容差内等价；
  分块不改变 per-pixel 候选栈语义（frame_id 绑定不变，rejection/integrate 逐像素独立）。

分块合法域 (fail-closed，正向约束):
  Σ px_i == total_pixels  ∧  p0_i 严格递增  ∧  p0_i + px_i <= total_pixels  ∧  区间两两不交
  任一条件不成立 ⇒ 该 split 在进入 launcher 前被拒绝 (显式错误)，禁止执行；
  禁止以 §9 容差吸收越界或 p0 错位造成的差异（§10 同款条款）。
  判定点: 调度侧按 inv.domain=[begin,end) 与 scalars p0 的一致性校验（kernel_registry.cpp:26-28 的 empty-domain 拒绝为最低限度；越界/重叠由分块器保证，见 §11 分块不变量门）。

两阶段不变:
  CPU reference 为权威 science semantics；ACR 只加速热点
    (block calibration / rejection / weighted reduction) (acr_kernels.cpp:1-8 的 W9 契约块)
  合成 Operation synthetic.mosaic_reject.fp64acc 的**权威语义 = CPU launcher**：
    cpu / legacy_parallel 两个函数指针同指 mosaic_reject_legacy（acr_kernels.cpp:359-360）；
    cuda 指针指 mosaic_reject_cuda（acr_kernels.cpp:361，CUDA kernel acr_cuda_bridge_kernels.cu:166-272）。
  ⇒ 等价性在 GPU 域是**待验证主张**而非构造性结论；两侧已知语义差见下「设备域差异」。

设备域差异 (必须在等价门中显式覆盖，禁止静默):
  (a) 候选深度上限: CUDA host 侧拒绝 frame_count>64 并返回 rc!=0 (acr_cuda_bridge_host.cpp:910)
      ⇒ CPU 侧无该上限；depth>64 时 CUDA 路径抛错并由调用方回退 CPU (stage2.cpp:1137-1142)。
  (b) 小样本门槛: CPU 用 plan.minimum_n=3 与 underdetermined_n=2 双条件 (acr_kernels.cpp:96-97;
      rejection.cpp:2064-2082)，CUDA 只收 und_n 作为 min_samples (acr_kernels.cpp:331)。
      n=2 时 CPU 走 UNDERDETERMINED 全接受；CUDA 进 sigma-clip，两样本的 |z| 恒为
      1/1.482602218505602 = 0.6744897501960817，故在缺省 |lower|=4.0/upper=3.0 下数值同解；
      当 upper<0.674490 或 |lower|<0.674490 时两侧**分歧**。
  (c) 已删权重记号哨兵槽（该记号已按 §9.73 A44 作废；槽位保留且被钉为常量 2，故复活时失败关闭）: CPU 对 wmode==2 显式 throw (acr_kernels.cpp:141-144)；CUDA launcher 不读该槽，
      无同款 fail-closed ⇒ 若该槽被置 2，只有 CPU 侧拒绝。

失败回退 (不得改变科学语义):
  逐像素 ivar 权重路径 → 强制 CPU canonical (ACR-IVAR-001)
  无画像/无画像信任 (model_available≠model_trusted) → OpenMP fallback (acr memory.md: BDR Reviewed)
     适用域: mosaic_reject_legacy 的并行分支仅在 P2_ENABLE_OPENMP 编译宏定义时存在
     (acr_kernels.cpp:75-80,212-245)；默认构建 P2_ENABLE_OPENMP=OFF (coverage/CMakeLists.txt:28)，
     此时 workers 槽被读取但并行路径不参与编译，执行退化为串行。
  候选栈 non-finite / UNDERDETERMINED / ALL_REJECTED 等冻结语义在任意设备上一致
```

与 `lib/algorithms/coverage/src/acr_kernels.cpp`（注册与三个 launcher）、`lib/algorithms/coverage/src/stage2_common.cpp:512-529`（`p2_acr_block_eligible`）、`lib/infrastructure/acr/backends/cuda/bridge/acr_cuda_bridge_kernels.cu:166-272`（CUDA kernel）、`lib/infrastructure/acr/memory.md: BDR Reviewed` 一致。

## 6 假设

- `phase2` 热点为逐像素独立栈（`rejection+integrate`），分块仅沿像素维切分，不跨像素依赖；
- `frame_id` 稳定绑定不受分块影响；
- `GPU` 浮点阶差异仅来自 FMA/并行归约阶，不含算法分支差异。

## 7 独立不变量

- **分块不变量**：**在 §5「分块合法域」成立的 split 上**（含 `1×total` 与 `N×1` 极端），`signal`/`support` 在 §9 容差内等价；合法域不成立的 split 不进入比较，按 §5 显式拒绝。
- **设备不变量**：`cpu_only` vs 单设备 `gpu_only` vs `mixed`（同 `total_pixels`）结果在 §9 容差内等价，且**必须覆盖 §5「设备域差异」(a)(b)(c) 三条**；未覆盖时该门对该设备域不成立。
- **回退不变量**：回退到 CPU 的结果与直接 CPU 一致，无相位内分支偷换。
- **常量场不变量**：常数 `values=C` 时 `signal=C` 在 §9 的浮点容差内成立，与设备/分块无关（`C` 为 FP32 时 `Σw·C/Σw` 不必逐位等于 `C`）。

## 8 极端/退化条件

| 条件 | 行为 | 证据 |
|---|---|---|
| `px==0` / `depth==0` | `throw runtime_error missing scalars` | `acr_kernels.cpp` |
| 缺 `buffer0/1` | `throw missing buffers` | 同上 |
| 逐像素 ivar 权重 | 禁 ACR，CPU canonical | `ACR-IVAR-001` |
| 无画像信任 | OpenMP fallback；适用域 = `P2_ENABLE_OPENMP` 编译宏已定义（默认构建未定义 ⇒ 实际执行串行） | `acr memory.md BDR`；`acr_kernels.cpp:75-80,212-245`；`coverage/CMakeLists.txt:28` |
| 非有限 candidate | `INVALID_INPUT` 一致 | `integrate.cpp` |

## 9 精度策略

- 容差**必须按信号标度声明**，禁止只给绝对量。设 `s` 为该域信号的特征量级（`signal` 缓冲的标度代表值）：
  - `signal`（FP32 承载面）：`max_abs ≤ 8·ulp_fp32(s)`，即**相对容差** `max_rel ≤ 1e-6`；
  - FP64 内部累积（`vs/wsum`，未落盘）：`max_rel ≤ 1e-12`；
  - 推导：FP32 机器精度 `~1.19e-7` × `depth≤32` 累积 × 独立像素数，实测相对包络 `≤1e-6`；禁止失败后增大。
- **为什么不能写成绝对 `ADU`**（标度依赖的定量后果；下表按 IEEE 754 单/双精度的 `nextafter` 间距直接计算，三档结论互不相同）：

  | 信号标度 `s` | `ulp_fp32(s)` | `1e-6 ADU / ulp` | 判定 |
  |---|---:|---:|---|
  | `1.1387e-17`（`photo_scaled_adu`，M42 实测下界） | `8.27e-25` | `1.21e18` | **恒真**：容差比可表示量子宽 18 个数量级，任何 FP32 差异都通过 |
  | `1.0`（`calibrated_adu` 归一一档） | `1.19e-7` | `8.39` | 有意义 |
  | `6.5535e4`（`raw_adu` 16-bit 满量程） | `3.91e-3` | `2.56e-4` | **不可达**：容差小于末位量子，任何一次末位差即判红 |

  同理 `1e-12 ADU` 在 FP64 上于 `s=6.5535e4` 处为 `0.137·ulp_fp64`（不可达）、于 `s=1.1387e-17` 处为 `6.49e20·ulp_fp64`（恒真）。⇒ 绝对容差在三个被本仓显式声明的标度上给出**三种互不相同**的结论，故绝对写法不具证据资格。
- **离散量按整数精确比较，连续量按相对容差比较（二者必须分开写）**：
  - `rejection_count`、`n_valid`（`buffer5`/`buffer6`）为整数计数 ⇒ `exact`（不可放宽）；
  - `support`（`buffer2`/`buffer4`）为 FP32 连续量、值域 `[0,1]` ⇒ `max_rel ≤ 1e-6`，**不适用** `exact`。

## 10 不可接受变化

- 在 `GPU/Mixed` 分块中改变 `rejection` 阈值/归一化/large_scale 语义；
- 在逐像素 ivar 权重时仍走 `GPU` 分块（违反 `ACR-IVAR-001`）；
- 将 `status` 的 `UNDERDETERMINED/INVALID` 语义改写为设备相关；
- 以放宽 §9 的相对容差掩盖分块越界或 `p0` 错位；
- 把 §9 的容差写成绝对 `ADU` 量而不声明信号标度；
- 把 `support`（FP32 连续量）与 `rejection_count`/`n_valid`（整数计数）混用同一判据；
- 在 `GPU/Mixed` 分块中不覆盖 §5「设备域差异」(a)(b)(c) 即宣称设备等价。

## 11 验证 Oracle

- **CPU/GPU/Mixed 等价门**：同 `total_pixels` 的 `cpu_only vs gpu_only vs mixed(2/4/8 splits)`，按 §9 的相对容差比较 `signal`，`rejection_count`/`n_valid` 精确相等，`support` 按 §9 相对容差。
  - **执行域（可判定）**：用例为 `phase2_synthetic_gate.Phase2Acr.*`（`lib/algorithms/coverage/tests/synthetic_gate.cpp:3355` 起）。CUDA bridge 不可用时 CUDA 侧用例以 `GTEST_SKIP` 退出（`synthetic_gate.cpp:3476`）⇒ **该门在无 GPU 环境下不构成对 GPU 路径的证据**，报告必须区分「通过」与「未行使」。
  - **夹具前置条件（必须显式声明，否则门恒真）**：构造 `KernelInvocation` 时必须按 §4 的 10 槽布局追加标量；`workers` 槽位于偏移 56，仅当 blob ≥ 60 字节时可读。夹具少于 60 字节时 `p0`/`wmode`/`workers` 三个槽一律读到 `nullopt`，落到缺省值 ⇒ 任何「多 worker 对照」两档取到同一配置，比较结果与配置无关。
- **分块不变量门**：`1×N` vs `N×1` 切分结果等价（合法域按 §5）。
- **回退门**：逐像素 ivar 权重的 `ACR` 强制 CPU 与纯 CPU `canonical` 等价（`ivar_wiring_test`）。
- **流量守恒门**：常数场 `C` 的 `signal` 与 `C` 在 §9 相对容差内相等，且与分块无关。
- **独立参考**：`eng/tests/api/test_reject_integration_oracle.py`（SYN-006）用**纯 Python 第一性原理**复算 `robust-MAD sigma-clip → accepted 集合 → 等权均值`，与生产 kernel 逐像素比对，判据为 `abs ≤ 0.02`（信号标度，非相对容差）。该参考不实现逐样本 ivar 加权，只覆盖等权（`weights=nullptr`）分支。

## 12 关联 ALG ID

- `ALG-ACR-EQUIV-001` 合成 Operation 分块等价（`mosaic_reject_legacy` → ACR 调度）
- `ALG-ACR-EQUIV-002` ivar 生产禁用与 OpenMP fallback 路由（`p2_acr_block_eligible` 恒 false，见 §4）

## 13 追溯与测试

- 权威文件: `docs/science/ACR_EQUIVALENCE.md` (SCI-ACR-EQUIV-001)
- 实现: `lib/algorithms/coverage/src/acr_kernels.cpp` (`kOpMosaicReject, mosaic_reject_legacy`), `lib/algorithms/coverage/src/stage2_common.cpp` (`ACR-IVAR-001`), `lib/infrastructure/acr/scheduler/*` (Dispatcher/Profile)
- 公开 API: `register_phase2_acr_kernels, kOpMosaicReject`
- 测试: `TST-ACR-001` CPU/GPU等价、`TST-ACR-INV-001` 分块不变量、`TST-ACR-FAIL-001` 极端回退（新增/映射见 `docs/TRACEABILITY.csv`）。
- 用例锚（`lib/algorithms/coverage/tests/synthetic_gate.cpp`）：`Phase2Acr.LegacyLauncherEquivalent`(:3355)、`Phase2Acr.CudaEquivalent`(:3466)、`Phase2Acr.CudaWeightedSupportEquivalent`(:3513)、`Phase2Acr.G9CompactFrameSubset`(:3591)、`Phase2Acr.G9WinsorizedCpuRoute`(:3648)、`Phase2AcrParallel.LegacyCpuOneVsTwoTDetermine`(:3395)。
- **执行域（不得含糊）**：CUDA bridge 不可用时 CUDA 侧用例走 `GTEST_SKIP`（`synthetic_gate.cpp:3476`）；`LegacyCpuOneVsTwoTDetermine` 的并行分支依赖 `P2_ENABLE_OPENMP` 编译宏，未定义时两档都在串行路径上运行 ⇒ **报告必须区分「通过」与「该门未行使」**，不得把 SKIP 或未行使计为等价证据。

## 14 参考文献与参考代码库（含许可证）

> 本节只补出处与参考实现；§5 等价定义、§9 容差与 §11 判据形态以正文为准。

- **浮点语义与归约非结合**：IEEE 754-2019, IEEE Standard for Floating-Point Arithmetic；Goldberg, D. 1991, ACM Computing Surveys 23, 5（DOI 10.1145/103162.103163）。
- **归约误差界/确定性求和**：Higham, N. J. 2002, Accuracy and Stability of Numerical Algorithms, 2nd ed., SIAM（ISBN 0-89871-521-0）；可复现求和技术见 Demmel, J. & Nguyen, H. D. 2013, “Fast Reproducible Floating-Point Summation”, Proc. 21st IEEE Symp. Computer Arithmetic (ARITH)。
- **并行执行语义**：OpenMP Application Programming Interface（OpenMP ARB）——本模块 fallback 路径（§5）的语义基础；ACR 为隔离实验，生产不可达（`ASTROCS_DESIGN.md` §1.3/§8.1）。
- **CPU reference 为权威 science semantics**：Project-defined（§5）；GPU/Mixed 仅加速热点，不改变 rejection/integrate 语义。

参考代码库（含许可证；仅对照不复制 GPL 代码）：
- Astropy（BSD-3-Clause，https://github.com/astropy/astropy）：WCS/投影、统计、单位。
- photutils（BSD-3-Clause，https://github.com/astropy/photutils）：检测/质心、背景估计、PSF 与孔径测光。
- SExtractor（GPL-3.0，https://github.com/astromatic/sextractor）：背景网格、检测/去混叠、FLUXERR。
- ccdproc（BSD-3-Clause，https://github.com/astropy/ccdproc）与 LSST ip_isr（GPL-3.0，https://github.com/lsst/ip_isr）：母版约定与 ISR 顺序。
- SWarp（GPL-3.0，https://github.com/astromatic/swarp）/ SCAMP（GPL-3.0，https://github.com/astromatic/scamp）：马赛克背景与相对定标。
- DrizzlePac（BSD-3-Clause，https://github.com/spacetelescope/drizzlepac）：drizzle 与相关噪声。
- astropy-healpix（BSD-3-Clause，https://github.com/astropy/astropy-healpix）/ healpy（GPL-2.0，https://github.com/healpy/healpy）：HEALPix 几何。
- reproject（BSD-3-Clause，https://github.com/astropy/reproject）：WCS 重采样与方差传播。
- WCSLIB（LGPL-3.0）/ CFITSIO（宽松许可，NASA/HEASARC）：WCS 与 FITS 独立读取器。
- NumPy/SciPy（BSD-3-Clause）：独立 FP64 Python Oracle。

