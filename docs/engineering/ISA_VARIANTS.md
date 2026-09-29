# ISA 变体与逐 kernel 选路

> 上游：`docs/ASTROCS_DESIGN.md` §9（CPU 后端与资源）、§8.5（模块与 ABI）

## 0 原则

- 先由 profile 证明热点，只为热点做变体；变体与 baseline 共享合同与源，禁复制漂移；
  逐 kernel 有 Oracle 与确定性判据。
- 无收益的档位**不登记变体**（`NOT_SHIPPED`），但必须留下完整测量记录（读入 `实验/` 证据面）。
- 变体一律以 **DSO**（Windows `.dll` / Linux `.so`）随安装树分发；调度器运行时检测 CPU +
  benchmark 选取，缺库回退基线；主程序保持基线指令集。

## 1 测量口径与决策

- 测量口径：基准程序 `eng/tests/backend/kernel_bench_main.cpp` 逐 kernel 计时，取多轮
  median-of-5 读数、baseline 取最优（对变体最保守）；变体整 TU 局部旗标，与 baseline 共享
  `baseline_kernels_impl.inc` 同一源（**零复制漂移**）。
- 变体实现（`lib/infrastructure/benchmark/backend_host/`）：`avx_backend.cpp`（`-mavx`，无 FMA）、
  `avx2_backend.cpp`（`-mavx2 -mfma`）、`avx512_backend.cpp`
  （`-mavx512f -mavx512bw -mavx512vl -mavx512dq`）。
- **SHIP 阈值**：受控热点增益 ≥ +10% 且方向稳定；落在 ±10% 带宽内的记 `REMEASURE`，
  交 benchmark 逐 kernel 选路。
- **现行测量口径 = AVX2+FMA 独立复测批**。各批逐 kernel ns 读数、增益、能力证明的反汇编计数
  与决策原表见测量归档 `实验/engineering-evidence/prerelease-v5/`（`ISA-001` / `ISA-002` /
  `ISA-003` 三批子目录），逐批读数与决策列落 `ISA-00x/MEASUREMENTS.csv`。本文件只承载结构决策。
- **批间数值差异（诚实登记）**：hips-bulk-transform 增益 `ISA-001/002` 批记为 **+28.2%/+28.3%**
  （两条腿；热点 profile 注释正本 = `lib/infrastructure/benchmark/cpu/avx2/include/astrocs/cpu/avx2_provider_v1.h`），
  现行 `ISA-003` 批记为 **+39.8%**（`ISA-003/MEASUREMENTS.csv`）。两者是**不同批次的实测值**，
  不是同一口径的两份读数；决策只取"过阈值且方向稳定"，不取具体数。

| 档位 | 决策 | 依据 |
|---|---|---|
| AVX2+FMA（`avx2_backend.so`） | **SHIP(avx2)** | 受控热点 calibration-pixel-transform 与 hips-bulk-transform 的增益均过阈值且方向稳定；能力证明 = 反汇编含 FMA 特征指令与 256-bit VEX，而 baseline 扫描零 VEX |
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

### 2.1 实现落点（本节登记实现态，§0/§1 为决策态）

**交付形态**：两个变体 target 为 SHARED，安装树 `providers/` 目录内落
`astrocs_cpu_avx2.so` / `astrocs_cpu_avx512.so`（Windows：同目录 `.dll`），与清单
`backends.manifest.json`（生成器 = `eng/tools/gen_backends_manifest.py`）**同目录**安装——
加载器语义要求清单与裸文件名同目录（见 `docs/architecture/CPU_BACKEND_ARCH.md` §3 信任边界）。

**能力位**：清单的 `required_features_bits` 由构建期实测填充；avx2 = `avx2|fma` = 24，
avx512 = `avx512f|avx512bw|avx512dq|avx512vl` = 928（声明集 ⊊ 编译所需集会放过 KNL 型主机，
故取编译期实际使用的四条）。位定义唯一源 = `cpu_features.h`。

**运行期选路调用点**（生产）：`lib/infrastructure/cli/commands.cpp` 的运行路径
→ `backend_loader`（裸文件名/ABI/实测 sha256/能力位预检 → dlopen → handshake → self_test）
→ `cpu_routing`（逐 kernel 路由决策，profile 驱动）→ 选址；缺库/不支持/收益不足回退基线，
事件面 `backend_id` 记**实际选址结果**。选取与回退口径正本 = `CPU_BACKEND_ARCH.md` §6。

**avx512 的档位状态**：§1 表判 avx512 = NOT_SHIPPED（无超越 AVX2+FMA 的收益）。
「随构建交付为可测量 DSO」与「档位是否发布」是两件事：本节只登记前者，后者以 §1 决策为准。

### 2.2 两族 provider 的登记与清单校验（DYN-740 / C-03）

安装树的 `providers/` 目录内**并存两族** DSO，各有**独立冻结的 C ABI 与消费方**——不可混装、
不可互相代替（混装由机器门判红）：

| 族 | 文件 | 唯一入口符号 | ABI 正本 | 消费方 | 清单 | 构建产物名 |
|---|---|---|---|---|---|---|
| backend（逐 kernel 路由） | `astrocs_cpu_{baseline,avx2,avx512}.so` | `astrocs_backend_get_api_v1` | `lib/include/astrocs/common_abi_v1.h` | `backend_host/backend_loader.cpp` + `cpu_routing` | `backends.manifest.json` | `ASTROCS_BACKENDS_MANIFEST` |
| provider（能力探测/变体查询） | `astrocs_cpuprov_{baseline,avx2,avx512}.so` | `astrocs_provider_query_v1` | `lib/include/astrocs/abi/module_api_v1.h`（冻结） | `lib/infrastructure/pipeline/module_loader/secure_loader.c` | `providers.manifest.json` | `ASTROCS_PROVIDERS_MANIFEST` |

- **入图口径（C-03 判词）**：provider 族是设计文档指定的 provider 实现面
  （`docs/architecture/cpu/CPU_001_CAPABILITY_PROBE.md:96-98`、`CPU_003_AVX2_PROVIDER.md:13,88`），
  且已被 `UT-CPU-BASELINE / UT-CPU-AVX2 / UT-CPU-AVX512` 覆盖，此前只是**未进构建目标**
  （`CHK-RETIRED-CODE` R4 把它列为未引用）⇒ 按裁决「属安装树分发的 ISA provider 集则必须入图」，
  本轮补入构建目标 + 清单校验 + 安装树登记，**不退役**（退役会删掉活的 CI 覆盖面并与 CPU 文档冲突）。
- **校验**：`eng/ci/check_provider_manifests.py`（`CHK-PROVIDER-MANIFESTS`）逐条校验清单自洽、
  实测 sha256、声明的入口符号**确实可由 dlopen 解析**、`providers/*.so|*.dll` 与清单条目一一对应
  （游离或双登记判红）、能力位与 `features_defined` 一致；配 `--self-test`（5 红 1 绿）作负例面。
- **隔离**：两族都在 `acsd` 链接闭包之外（`check_isa_leak.py` 的报错面不变）；主程序保持基线指令集。

## 3 ISA 污染防线

- 主 CLI / baseline TU：无 `-march` / `-mavx` 旗标（测试断言）；opcode scanner 禁 VEX/ymm/zmm
  （`eng/tests/backend/test_abi_kernels.py`）。
- 变体 = **两个 TU**（R-60）：**门面 TU**（`astrocs_cpu_avx2` / `astrocs_cpu_avx512`：`*_backend.cpp`
  的 get_api / self_test / kernel 注册表）**零 ISA 旗标**，**计算面 TU**（`*_backend_kernels.cpp`，
  与 baseline 共用 `baseline_kernels_impl.inc`）是唯一带 ISA 旗标的 TU，两者经唯一跨 TU 符号
  `astrocs_variant_kernel_dispatch_v1`（`backend_variant_kernels.h`）相连。
  **为什么按源文件隔离**：MSVC 没有函数级指令集覆盖（无 `#pragma GCC target` 对应物），若自检/握手
  入口与计算面同 TU，则「能力预检不过 ⇒ 干净拒绝」会退化成「加载即撞非法指令」。
  机器判据：`eng/tools/quality/check_isa_same_source.py` 的 `tu_isolation`（S7，门面 TU 带旗标即红）
  + 站点登记 `eng/tools/quality/isa_sites.json`。
- 变体 TU：门面走基线旗标，计算面 TU 局部旗标（GCC/Clang：`-mavx2 -mfma` /
  `-mavx512f -mavx512bw -mavx512vl -mavx512dq`）。**产物级双向判据** =
  `eng/tools/quality/check_variant_isa_disasm.py`（工具无关，吃 objdump / llvm-objdump / dumpbin 文本）：
  计算面必须含该档宽指令（`--hit`），自检/握手入口必须**零 VEX/EVEX**（`--clean-symbol`），
  且每条**有使用证据的声明位**都要在产物里找到证据（`--require-feature`；
  `--declared-features` 逐位登记"许可面 vs 实际发射面"的差异，不得静默）。
- **非 GCC 平台（R-60）**：MSVC/clang-cl 走 `/arch:` 档位 —— `/arch:AVX2`（官方口径同时开 FMA）
  与 `/arch:AVX512`（许可面 = F+CD+BW+DQ+VL，**没有** F 子集档位）；未知 `/arch:` 值只报 D9002
  且 rc=0（静默忽略）⇒ **不得**用 `check_cxx_compiler_flag` 判支持，必须"版本门槛 + 预定义宏
  `#error` 自检 + 产物级反汇编断言"三处闭合，且 MSVC 腿需 `/fp:contract`（VS2022 起
  `/fp:precise` 不再默认收缩，GCC 默认 `-ffp-contract=fast` 会收缩）。
- Windows 变体（`/arch:AVX2` `/arch:AVX512`）随发行链同流程登记；avx512 的**声明面按平台精确值**
  取（`__AVX512CD__` 分支）：GCC 腿 = F|BW|DQ|VL = 928，MSVC 腿 = F|CD|BW|DQ|VL = 992，
  清单 `required_features_bits` 由 `eng/tools/gen_provider_manifests.py` 按 `--compiler` 同步
  （GNU 腿输出逐字节不变）。

### 3.3 第二族：CPU provider 变体族（astrocs_cpuprov_*）同配方隔离（R-60）

第一族的 TU 级隔离配方**已推广到第二族**（`lib/infrastructure/benchmark/cpu/{avx2,avx512}/`）：

| | 门面 TU（零 ISA 旗标） | 计算面 TU（唯一带旗标） | 跨 TU 桥 |
|---|---|---|---|
| avx2 | `avx2/src/avx2_provider.cpp`（`astrocs_provider_query_v1` / `acs_cpu_avx2_cap_gate` / `*_self_test` / kernel 注册表） | `avx2/src/avx2_kernels.cpp` | `astrocs_cpuprov_kernel_range_v1` |
| avx512 | `avx512/src/avx512_provider.cpp` | `avx512/src/avx512_kernels.cpp` | 同上 |

- **平台旗标形态**（唯一登记点 = 根 `CMakeLists.txt` 的 `astrocs_cpuprov_<v>_kernels`）：
  GCC/Clang = `-mavx2 -mfma` / `-mavx512f -mavx512cd -mavx512bw -mavx512dq -mavx512vl`；
  MSVC/clang-cl = `/arch:AVX2`（+ VS2022 起 `/fp:contract`）/ `/arch:AVX512`。
  **改前该族的旗标被 `if(NOT MSVC)` 门控 ⇒ Windows 腿压根没有 ISA 旗标**，两个变体库与基线
  同码却仍在清单里声明 ISA 能力（虚假能力声明）。
- **旗标失效不得静默**：计算面 TU 顶部有 `_MSC_VER && !defined(__AVX2__)` / `__AVX512F__`
  （及 AVX-512 的 CD/BW/DQ/VL 齐套）`#error` —— `/arch:` 取值不被识别只报 D9002 且 rc=0，
  不得以「基线同码产物」冒充变体。
- **ABI 零变化**：导出面仍是 provider ABI 白名单（`astrocs_provider_query_v1` + `acs_cpu_*_cap_gate`
  + `acs_cap_*` 探测面）；跨 TU 桥在非 MSVC 下 `hidden visibility` ⇒ **不进动态符号表**
  （Linux 侧导出面与改前逐条相同，实测 `nm -D` 集合一致）。
  Windows 侧 DSO 以 `WINDOWS_EXPORT_ALL_SYMBOLS` 构建，导出表会多一条桥符号，与第一族
  `astrocs_variant_kernel_dispatch_v1` 同款处置（加载器按名字取 `astrocs_provider_query_v1`，
  多一条不改变任何加载/选路语义）。
- **数值零变更**：kernel 实现从门面 TU **逐字符搬移**到计算面 TU（只去掉 `static`、改名为桥入口）；
  科学公式 / 项序 / 容差（2e-4 冻结）不动。实测 `CPU-003 AVX2 PASS`（两热点 oracle
  `max_rel` 4.47e-06 / 0、预算 1 vs 4 逐位相同）。
- **产物级判据落点**（本族此前无人承担）：
  - `eng/tests/backend/test_cpuprov_isa_variants.py` —— 正例 P1（计算面含本档档位证据、门面
    `query`/`cap_gate` 零 VEX/EVEX）+ 负例 N1（旗标挂回门面 ⇒ 判红）/ N2（计算面不编旗标 ⇒
    判红）/ N3（门面代码行出现旗标字面量 ⇒ 判红）+ S1（唯一桥、不复制实现）/ S2（导出面白名单、
    桥不进 `.dynsym`）/ S3（声明位 = 站点推导位，两族 × 两平台）。
  - `eng/tests/backend/test_cpuprov_manifest.py` —— 声明面端到端（真 cpuprov DSO + 真
    `gen_provider_manifests.py --family provider`，两腿位值/旗标逐条核对 + 站点缺失 rc=2 /
    旗标少一位 rc=5 两条负例）。
  - `eng/tests/cpu/avx2/run_provider_avx2_checks.py` 与
    `eng/tests/cpu/avx512/run_provider_avx512_checks.py` 改走**同一份**编译配方
    (`eng/tests/backend/variant_build.py`)，测试产物形态 = 发行产物形态。
  - `check_isa_same_source.py --fault-inject` 新增 4 条本族注入：
    `cpuprov-tu-isolation` / `cpuprov-kernels-unflagged` / `cpuprov-arch-flag-drift` /
    `cpuprov-declaration-cd`（逐条必红，见 `--fault-inject all`）。
- **已登记差异（产品缺陷，非判据缺陷）**：▭check_manifest_isa_artifact.py 的 M5 对本族判红一处，
  登记如下，不得用改判据消红：

  | 项 | 实测 | 判定 |
  |---|---|---|
  | astrocs_cpuprov_avx512 产物实测发射 FMA3（vfmadd132ss ×2 / vfmadd231ss ×1，VEX.FMA 编码），
    而清单声明 {avx512f, avx512cd, avx512bw, avx512dq, avx512vl}=992（**不含 FMA**） |
    本机 GCC 14.2 / astrocs_cpuprov_avx512.so 实测 3 条 | **用了却没声明**。根因：isa_sites.json 的
    flag_feature_map["-mavx512f"] = ACS_FEAT_AVX512F 把许可面记窄（实测 GCC 14.2 与 clang 18.1.8
    在 -mavx512f 下都会发射 FMA3）。**实际可达性低**（同时具备 F+CD+BW+DQ+VL 的商用 CPU 均同时具备 FMA3），
    但声明面确实少一位。随第一族同因的裁决一并处置；**未裁决前保持判红**。本轮未改 flag_feature_map
    （共享表，属第一族裁决范围）。 |

- **与第一族的产品差异（合法，判据只要求各自同源）**：本族确实带 `-mavx512cd` 编译且 DSO 自陈宏取
  `ACS_CAP_GROUP_AVX512_SUBSET`，故两平台清单位都是 `F|CD|BW|DQ|VL = 992`；第一族 GCC 腿是 928、
  MSVC 腿 992。

## 4 与模块边界 Oracle 的关系

- 12 kernel 的 Oracle（独立参考 + 确定性）已在模块边界合同建立；变体复用同一 Oracle
  （共享公式），变体特有仅舍入差 ⇒ 容差比对；确定性（预算 1 vs 4 逐位）对变体同样成立
  （同 impl 外层）。
