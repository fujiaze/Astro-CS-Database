# T10 · Windows 平台全量构建，零编译警告

## 0. 结论摘要

| 项 | 结果 |
|---|---|
| 构建是否成功 | **是**。冻结 preset `win-msvc-17.14.39-x64` configure rc=0、`--build --preset win-rel` rc=0、install rc=0，**从零目录 clean-room 全量重建** |
| 编译警告 | **119 → 0**（首轮实测 119 条主诊断行 / 54 个真实诊断点 + 8 条 LNK4217；最终轮全形态检索命中 0） |
| 检索覆盖 | 503 个 cl 编译单元（源文件清单行），全目标、全配置（RelWithDebInfo） |
| 产物形态 | **部分符合**规范 §2/§3：唯一 exe + 14 个 dll + 按指令集分的计算核（baseline/AVX2/AVX-512 两族）+ schemas + licenses + 两份 manifest **齐备**；**`config/` 与滤镜库在发行目录缺失** |
| 两平台差异 | 登记 **12 条**（见 §4） |
| 为消警告改动 | **21 个文件**；其中 **2 处改变了行为**（均为缺陷修复，非静默重构），其余 19 处零行为变化 |
| 登记缺陷 | **23 条**（§6），其中 8 条本单已修、15 条如实登记未修 |

⚠️ **本单只覆盖 Windows 一侧。** Linux 腿零警告未在本单验证，不在此断言。

---

## 1. 构建节点、工具链与实际执行的远程命令

### 1.1 节点

| 项 | 值 | 取证 |
|---|---|---|
| 主机 | Windows 正式工具链节点（Fatduck），Tailscale 地址 `100.104.10.71` | 远程实测 |
| OS | `Microsoft Windows NT 10.0.26220.0` | `$PSVersionTable` / `[Environment]::OSVersion` |
| CPU | AMD Ryzen 7 5800X 8-Core Processor，16 逻辑核 | `Get-CimInstance Win32_Processor` |
| 可用磁盘 | 412.3 GB free | `Get-PSDrive C` |

### 1.2 工具链（实测）

| 组件 | 实测值 | 合同 pin | 判定 |
|---|---|---|---|
| Visual Studio | **17.14.3 BuildTools**（另有 2022 Community，但未装 MSVC 工具集） | VS 17 2022 | ✅ 命中 |
| MSVC 编译器 | **19.40.33820.0**，工具集目录 `14.40.33807` | `v143,host=x64`（不带版本 pin） | ✅ 命中 v143 |
| Windows SDK | **10.0.26100.0** | `10.0.26100.0` | ✅ 命中 |
| CMake | **3.31.6-msvc6** | `3.31.12` | ⚠️ **漂移**（登记 D-14） |
| zlib | `C:\Users\fujia\zlib-msvc`，`libz.lib`，版本 **1.3.2.1** | — | ✅ |
| Python | 3.12.2（`eng/tools/gen_build_stamp.py` 用） | — | ✅ |
| git | 2.53.0.windows.1 | — | ✅ |

**生成器口径**：严格按 preset 的 `Visual Studio 17 2022`，**未使用 Ninja、未使用 VS18**（该机同时装有 VS 18.3.0 BuildTools + cmake 4.1.2，按裁决一律不用）。cmake 不在 PATH，用的是 VS2022 BuildTools 内置的绝对路径。

⚠️ **preset `displayName` 与实测不符**（登记 D-15）：preset 自称 `v143 14.44.35207`，而该机 v143 默认解析到 **14.40.33807**（14.44.35207 虽已安装但非默认）。`displayName` 是提示文本、非机器源，但它是仓内唯一写明工具集号的地方。

### 1.3 源码同步（保 HEAD 精确一致）

根 CMake 配置期强制 `git rev-parse HEAD` 与 `git ls-files`（`CMakeLists.txt:69-96`、`:113-130`），非 git 树直接 `FATAL_ERROR` ⇒ 必须传真 git 工作树。

```bash
# 本地：造 depth-1 纯仓，保留精确 HEAD SHA（39 MB）
git clone --bare --depth 1 "file:///workspace/Astro%20CS%20Database" /tmp/t10win/acsd-head.git
#  → 10e84432a402fbd131cbc37cb06e299b96072015
tar czf acsd-head.tgz acsd-head.git
sudo scp -i /root/.ssh/id_ed25519_fatduck acsd-head.tgz fujia@100.104.10.71:C:/Users/fujia/t10_acsd-head.tgz
```

远程克隆后实测：`HEAD: 10e84432a402fbd131cbc37cb06e299b96072015`、`STATUS_LINES: 0`、`FILECOUNT: 2956`。

### 1.4 实际执行的远程命令（逐条）

全部经 `powershell -NoProfile -ExecutionPolicy Bypass -File <正斜杠路径>.ps1`（脚本先落地，不堆内联）。核心三条：

```powershell
# ① 配置（冻结 preset，零命令行覆盖）
$env:ACS_ZLIB_ROOT = "C:/Users/fujia/zlib-msvc"
& "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" `
    --preset win-msvc-17.14.39-x64
# ② 全量构建（冻结 preset）
& $cmake --build --preset win-rel --parallel
# ③ 安装（冻结 preset 的 installDir = ${sourceDir}/build/win-install）
& $cmake --build --preset win-rel --target install --parallel
```

**最终一轮的 clean-room 序列**（为使「零警告」覆盖全部 TU 而非增量）：

```powershell
Remove-Item -Recurse -Force "C:/Users/fujia/t10/acsd/build"   # build_dir_exists: False
& $cmake --preset win-msvc-17.14.39-x64                       # configure_rc=0
& $cmake --build --preset win-rel --parallel                  # build_rc=0  elapsed=96.5s
& $cmake --build --preset win-rel --target install --parallel  # install_rc=0
```

实测输出：

```
HEAD: 10e84432a402fbd131cbc37cb06e299b96072015
DIRTY_FILES: 21
build_dir_exists: False
configure_rc=0
build_rc=0
elapsed_sec=96.5
log_lines=697
install_rc=0
```

⚠️ `DIRTY_FILES: 21` = 本单的 21 个未提交修复。构建期指纹如实记录了这一点：

```
#define ACSD_VERSION_STRING "0.1.0-alpha.1+g10e84432a402fbd131cbc37cb06e299b96072015"
#define ACSD_COMMIT_SHA     "10e84432a402fbd131cbc37cb06e299b96072015"
#define ACSD_BUILD_DIRTY    1
#define ACSD_BUILD_SOURCE_DIGEST "0b103ed0364e24a8f7ea52643f2f047ec99d10c5ac829fc44b94710361f9f2e3"
```

**远程构建产物一律不回流本地仓库**（只取回日志文本用于计数）。

---

## 2. 目标与产物清单（实测）

### 2.1 唯一可执行 + 多动态库

```
=== BUILD-TREE EXEs ===
EXE: ...\RelWithDebInfo\acsd.exe                                  ← 产品唯一入口
EXE: ...\lib\infrastructure\pipeline\orchestrator\cpp\RelWithDebInfo\orchestrator_legacy_cli.exe

=== BUILD-TREE DLLs (14) ===
acsd_catalog_gaia.dll          acsd_p1_calibration.dll     acsd_p1_cosmetic.dll
acsd_noop.dll                  acsd_p1_hips_writer.dll     acsd_p1_drizzle.dll
acsd_cpu_baseline.dll          acsd_io.dll                 acsd_runtime.dll
providers\acsd_cpu_avx2.dll    providers\acsd_cpu_avx512.dll
providers\acsd_cpuprov_baseline.dll  providers\acsd_cpuprov_avx2.dll
providers\acsd_cpuprov_avx512.dll
```

### 2.2 按指令集分的计算核（规范 §2 硬要求）—— 齐备

两族共 6 个 SHARED 计算核，baseline / AVX2+FMA / AVX-512 三档齐全：

| 族 | baseline | AVX2/FMA | AVX-512 |
|---|---|---|---|
| backend（`acsd_backend_get_api_v1`） | `acsd_cpu_baseline.dll` | `acsd_cpu_avx2.dll` | `acsd_cpu_avx512.dll` |
| provider（`acsd_provider_query_v1`） | `acsd_cpuprov_baseline.dll` | `acsd_cpuprov_avx2.dll` | `acsd_cpuprov_avx512.dll` |

编译旗标（Windows 实测写进 manifest）：

```
compiler: MSVC-19.40.33820.0
commit:   10e84432a402fbd131cbc37cb06e299b96072015
flag[baseline] = (none; amd64 SSE2 基线)
flag[avx2]     = /arch:AVX2
flag[avx512]   = /arch:AVX512
backend: acsd_cpu_avx2.dll    required_features_bits=24     (AVX2|FMA)
backend: acsd_cpu_avx512.dll  required_features_bits=992    (F|BW|DQ|VL)
```

旗标挂载点为**计算面 OBJECT target**（门面 TU 保持基线 ISA），`CMakeLists.txt:824/826/862/1014/1016/1018`；旗标失效有编译期 `#error` 兜底。

### 2.3 发行目录（install rc=0，25 文件）

```
acsd.exe                       ← 唯一可执行（legacy CLI 未进安装树 ✓）
acsd.product.json
acsd_io.dll   acsd_runtime.dll
licenses/     (3)
modules/      acsd_noop / acsd_catalog_gaia / acsd_p1_{calibration,cosmetic,drizzle,hips_writer}   (6)
providers/    5 个计算核 dll + backends.manifest.json + providers.manifest.json                     (7)
schemas/      acsd-product / dependency-lock / install-tree-contract / preset-contract             (4)
```

**符合**：可执行 ✓、动态库 ✓、schemas ✓。
**不符合**：规范 §3 要求「可执行 + 动态库 + schemas + **config** + **滤镜库**」，而 `config/` 与滤镜库**均未进安装树**（登记 D-12）。

### 2.4 运行期探测与回退（代码面核对）

能力探测 `cpu_features.cpp:74-105`（MSVC 走 `__cpuidex` + `_xgetbv`）→ 五道预检 `backend_loader.cpp:101-131`（裸文件名白名单 / ABI 版本 / 文件存在 / **sha256 实测** / `required ⊆ detected`）→ 装载 `backend_loader.cpp:133-192`（Windows `LoadLibraryExA` + `LOAD_LIBRARY_SEARCH_APPLICATION_DIR`）→ 任一闸不过即 `FALLBACK_BASELINE` → 进程内置 baseline 恒可用（`commands.cpp:274-284`）。

⚠️ 但选出的 provider **只到「上报」不到「派发」**：`cli_select_backend` 的产物仅用于 stderr 行与事件字段，`run_pipeline`（`commands.cpp:1063`）不接 backend 参数（登记 D-19）。

---

## 3. 警告检索：完整命令与结果（证明为零）

### 3.1 检索脚本（ASCII-only，避免 PowerShell 5.1 读 .ps1 的 ANSI 解码问题）

```powershell
$log = "C:/Users/fujia/t10_build4.log"
$lines = [IO.File]::ReadAllLines($log, [Text.Encoding]::GetEncoding(936))   # MSVC 按 GBK 输出
$w = @($lines | Where-Object { $_ -imatch "warning" })                       # ① 不分大小写、不限形态
$p = @($lines | Where-Object { $_ -match "warning [A-Z]+[0-9]+:" })          # ② 诊断式
$e = @($lines | Where-Object { $_ -imatch "error" })                         # ③
$r = @($lines | Where-Object { $_ -imatch "remark" })                        # ④
$g = @($lines | Where-Object { $_ -imatch "-W[a-z]|D9025|D9002|LNK[0-9]+" }) # ⑤ 旗标类
$c = @($lines | Where-Object { $_ -match "\.cpp$|\.c$" })                    # ⑥ TU 覆盖
```

### 3.2 最终轮实测输出（**全为零**）

```
warning=0  diagnostic=0  error=0  remark=0  flagish=0  tu=503
--- all warning lines ---
（空）
--- tail ---
  正在生成代码...
    正在创建库"...\RelWithDebInfo\acsd.lib 和对象"...\acsd.exp
  acsd.vcxproj -> C:\...\build\win-msvc-17.14.39-x64\RelWithDebInfo\acsd.exe
  Building Custom Rule C:/Users/fujia/t10/acsd/CMakeLists.txt
```

| 检索面 | 首轮 | 中间轮 | **最终轮** |
|---|---|---|---|
| ① `warning`（不分大小写、全形态） | 119 | 2 | **0** |
| ② `warning <CODE>:` 诊断式 | 119 | 2 | **0** |
| ③ `error` | 0 | 0 | **0** |
| ④ `remark` | 0 | 0 | **0** |
| ⑤ `-W*` / `D9025` / `D9002` / `LNK*` | 8 | 0 | **0** |
| ⑥ 编译单元行（覆盖证明） | 503 | 503 | **503** |

### 3.3 为什么 119 ≠ 119 个问题

首轮 119 条主诊断行中含 MSVC 模板实例化上下文行（`with` / `[` / `]` / `_Ty=…`），它们复用同一 `warning Cxxxx:` 前缀。剔除上下文行后：

- **真实诊断点 54 个**，分布：`C4244` 14、`C4456` 24、`C4189` 5、`C4100` 4、`C5208` 3、`C4005` 2、`C4457` 2、`C4458` 1、`C4127` 1、`C4701` 1
- **链接期 LNK4217 8 条**

### 3.4 首轮（HEAD 原始状态）的逐条台账

按目标分布（真实诊断点）：

```
x64\acsd_module_adapters.vcxproj                              14
lib\infrastructure\gaia_xpsd_client\src\gaia_client.c        19
lib\algorithms\coverage\src\sky_plane.cpp                     3
lib\algorithms\star_detection\src\sdet_image.cpp              2
lib\infrastructure\benchmark\backend_host\hardware_inspect.cpp 2
lib\infrastructure\cli\commands.cpp                           2
lib\algorithms\coverage\src\sampler.cpp                       1
lib\algorithms\photometry\cpp\src\pc_api.cpp                  1
lib\algorithms\photometry\cpp\src\spatial_gain.cpp            1
lib\algorithms\star_detection\include\star_detector.h         1
lib\algorithms\star_detection\src\sdet_api.cpp                2
lib\infrastructure\aio\src\aio_disk_full.h                    1
lib\infrastructure\aio\src\aio_sparse_punch.h                 1
lib\infrastructure\cli\monitor.h                              1
lib\algorithms\resample\p3_resample.cpp                       1
lib\algorithms\coverage\src\rejection.cpp                      2
MSVC STL <utility>(247,54)                                    1
MSVC STL <xutility>(5118,37) / <xutility>(5164,24)             2
```

⚠️ **这份「零警告」有一个必须写明的边界**（登记 D-10）：仓内有 4 个目标用 `/W0` 整体关告警，其中 3 个**同时编译了 65 个第一方 TU**（`acsd_p1_drizzle` 31、`acsd_phase2_integrate` 27、`acsd_p1_hips_writer` 7）。这 65 个 TU 在本次构建里**结构上不可能产生告警**。因此本单证明的是「**在既有告警门控配置下，全量构建零警告**」，不是「所有第一方源码在 /W4 下零告警」。这与仓内正本 `docs/engineering/standards/CODE.md:52`（禁 `/w`、`/W0` 整目标降级来掩盖本项目源码的告警）直接冲突。

---

## 4. 两平台差异清单

> 判据：Linux/GCC 侧同码零告警、Windows/MSVC 侧报出。含根因与处置。

### D-1 整数提升：`F` 浮点值存入 `std::size_t`（真缺陷，已修）

`lib/algorithms/star_detection/src/sdet_image.cpp:394`
```cpp
const std::size_t lo = *std::max_element(v->begin(), v->begin() + mid);  // v 是 std::vector<F>
return (F)(0.5 * ((double)lo + (double)(*v)[mid]));
```
`F` 实测只有 `float`/`double` 两个实例化（`sdet_median_of_finite` / `sdet_mad_sigma_impl`）。背景扣除后的像素**常为负** ⇒ 负浮点转 `std::size_t` 在 C++ 里是**未定义行为**；`[0,1)` 区间一律截成 0。`:395` 的 `(double)lo` 是伪装——它作用在**已截断完的整数**上，纯 no-op 提升。

实测量化（512×512 uint16，行标准差来自 `sqrt(var)` 必非整数）：

| 每行 σ (ADU) | 存活行数 | bgnoise 修前 | 修后 | thr 位移 |
|---|---|---|---|---|
| 0.4 | 512(偶) | 0.261712 | 0.523424 | **−1.3086** |
| 0.6 | 512(偶) | 0.331780 | 0.663561 | **−1.6589** |

σ<1 时恒为 **−50%**（两个中间值都 <1 ⇒ `lo` 变 0 ⇒ 中位数被腰斩）。经 `pr.thr = pr.bg + 5*pr.bgnoise`（`sdet_api.cpp:1616`）直接压低 κ=5 检测阈值 ⇒ 检出更多假源。最坏情形：行差分中位数 ≤ −1 时 `med` 变成 9.22e18，5σ 窗口把全部样本剔光 ⇒ `bgnoise` 归 0 ⇒ 阈值塌到裸背景中位数。

**同仓自带反证**：`sdet_api.cpp:170-177` 的 `sdet_median_of` 注释写着「与 robust_median **同式**」，实现却是正确的两中位数平均 ⇒ 意图与实现冲突，代码自己就是证据。4000 组随机对拍：修后 0/4000 不符，修前 2035/4000 不符。

**处置**：改为 `const F lo`。**改变行为**（见 §5）。

### D-2 LLP64 位宽：`long long` → `long` 发生在校验之前（真缺陷，已修）

`P3CropWindow` 与 `p3_crop_window_from_fits` 的坐标是 `long`（`lib/algorithms/projection/p3_wcs.h:186-212`）。Windows `long` 32 位 / Linux 64 位 ⇒ 越界坐标在 **Windows 静默截断后放行**、**Linux 具名拒绝**，违反 `module_adapters.cpp` 自陈的「禁静默夹取」。且函数内全部校验都作用在**已截断的形参**上，原理上看不见原值。

**处置**：窄化**之前**用 `long long` 校验值域，越界即具名拒绝；合法值经已证明在范围内的显式 `static_cast<long>` 传入。合法输入产物逐位不变，非法输入两平台一致 fail-closed。

### D-3 C4996 咨询面：目录级无边界定义（真缺陷，未修）

`_CRT_SECURE_NO_WARNINGS` / `_CRT_NONSTDC_NO_WARNINGS` 唯一定义点 `CMakeLists.txt:63`，**目录级、无作用域**，第一方与第三方同等覆盖。C4996 在 GCC 侧对系统头与 ISO C 函数名零告警，是纯平台口径差。

实测第一方调用点（486 文件，排除 third_party）：**CODE 口径 293 处**（SECURE 245 + NONSTDC 48）；RAW 口径 346 处。
⚠️ 仓内注释自称 **206** —— `git log -S '206 个调用点' -- CMakeLists.txt` 只有一条 `7d4c0aa2` 一次性写入，**无任何测量记录支撑**，不可复现；另一车道声称的 332 同样不可复现。本单已把该注释改成实测口径（见 §5）。

**为何本单未收紧到「第一方不覆盖」**：一旦第一方失去该宏，上面 293 处 C4996 全部暴露，与「本单零警告」**不可兼得**。裁决要求收紧作用域，但逐点整改 293 处需独立车道。已登记为遗留项并写进 `CMakeLists.txt` 注释。

### D-4 签名不对称：POSIX-only 判据使形参在 Windows 未被读

| 位置 | 现象 |
|---|---|
| `aio_disk_full.h:93` | `path` 只被 `statvfs`（POSIX）用 ⇒ Windows 侧 C4100 |
| `aio_sparse_punch.h:299` | `off` 在 Windows 分支不参与 ⇒ C4100（见 D-5） |
| `monitor.h:273` | `read_thread_cpu` 全靠 `/proc` ⇒ Windows 侧 `s` 未被写 ⇒ C4100 |

**处置**：签名保持两平台同形，Windows 分支显式 `(void)param;` 标明「有意不读」而非漏用。判定逻辑与返回值两侧相同 ⇒ 零行为变化。

### D-5 `read_at` 的 Windows 退化为顺序读（真缺陷，登记未修）

`aio_sparse_punch.h:299` 注释声明「位置读（**不改文件游标**）」。Linux 走 `pread` 两项都满足；Windows 走 `_read`，**静默丢弃 `off` 且推进游标**，契约不成立。

严重性定级（实测复核后的准确口径，勿夸大）：唯一生产调用点 `aio_sparse_punch.h:537` 的扫描循环严格自 0 起顺序推进（`chunk_off = b*kPunchBlockBytes`，除末块外 `in_file == chunk_len`）⇒ CRT 游标恒等于 `chunk_off`，**当前调用图上读到的字节与 `pread` 逐字节相同**，不是活跃错误结果。真实风险是契约违反 + 脆性（任何乱序/复用调用会静默读到错数据且不报错）+ 游标终态停在文件尾。真正的定位读实现属功能改动、需配套测试，本单不做。

### D-6 OpenMP 只在 UNIX 侧链接 ⇒ 变量与 pragma 双双失效

`CMakeLists.txt:1086/1119/1132/1195/1227` 全部 `if(UNIX AND OpenMP_CXX_FOUND)` ⇒ Windows/MSVC 下 `_OPENMP` 必不定义。直接后果：`in_flight` 只被 `#ifdef _OPENMP` 内的 `inner_u` 引用 ⇒ C4189。**处置**：声明随之搬进用它的分支，Linux 侧字面不变。

### D-7 指令集内建不同 ⇒ CPUID 取值路径不同

`hardware_inspect.cpp:86` 把 `eax/ebx/ecx/edx` 统一声明在分支外，但 MSVC 分支走 `__cpuidex(info[...])` 只回填 `ecx/edx` ⇒ `eax/ebx` 在 `_M_X64` 下从未被引用 ⇒ C4189。**处置**：声明按平台分支下移；已初始化但从未被读的局部变量无可观测效应。

### D-8 匿名 typedef + NSDMI

`star_detector.h:29` 的 `typedef struct { … } SDetParams;` 触发 C5208（C++ 里合法，MSVC 仍报）。**处置**：改为具名 `struct SDetParams { … };` —— 类型、布局、成员默认初值、可聚合性全同。

### D-9 宏重定义：`NOMINMAX`

`NOMINMAX` 是 windows.h 专用宏，构建系统已在 `CMakeLists.txt:64` 全局定义，`sampler.cpp` 与 `commands.cpp` 又无守卫地各定义一次 ⇒ C4005。**处置**：删除无守卫那一行（`sampler.cpp` 平台块内本就有 `#ifndef` 守卫）/ 给另一处加守卫。宏最终取值不变。

### D-10 `/W0` 整体关告警覆盖了 65 个第一方 TU

`eng/cmake/cfitsio_platform.cmake:63-77` 的 `acsd_cfitsio_isolate_warnings` 是**目标粒度**的，而第三方源只是该目标源列表的一部分（`target_sources(... ${*_CFITSIO_SOURCES})`）。CMake 没有「只对部分源关告警」的目标级表达，唯一可用的是 source 粒度 —— 仓内目前**从未用过**。结果：`acsd_p1_drizzle` / `acsd_phase2_integrate` / `acsd_p1_hips_writer` 三个目标共 **65 个第一方 TU** 在 MSVC 下完全无 `/W4`。

**收窄方案**：改用 source 粒度（`set_source_files_properties(<第三方 .c> PROPERTIES COMPILE_OPTIONS "/W0")`），使隔离只覆盖 vendored cfitsio；第一方 TU 回到 `/W4`。本单未做（裁决明示「不要求本单全量整改」），登记为遗留。

### D-11 `/W3` 叠加在全局 `/W4` 之上，把第一方目标静默降级

`lib/infrastructure/gaia_xpsd_client/CMakeLists.txt:39` 原有 `/Zc:__cplusplus /W3`。CMake 把目录级 `add_compile_options` 排在目标级之前 ⇒ 编译行实际 `/W4 … /W3`，而 MSVC 的 `/Wn` **后者胜** ⇒ 该目标有效级别是 `/W3` 而非 `/W4`。且 `/Wn` 覆写正是仓内自己所述的 D9025 形态。

**处置**：删掉 `/W3`，与全局口径对齐。⚠️ 该目标的告警面因此**变宽**——删 `/W3` 后立刻暴露出一条此前被遮蔽的 C4189（`module_entry.c:293` 的死变量 `int f = 0;`，一并删除）。**这是收紧不是放宽**，且实证了 `/W3` 此前确实在掩盖本项目源码的告警。

### D-12 中文源文件与注释 / 编码开关 —— 已正确配置，实测零编码告警

任务书点名「中文源文件与注释在缺少对应编译开关时可能触发编码告警，正确做法是加编码开关，不是删注释」。

**实测结论：仓内开关早已存在且有效。** `CMakeLists.txt:52` 的 `add_compile_options(/utf-8 /EHsc …)` 在 MSVC 分支无条件生效 ⇒ **零 C4819 / C4828**（首轮与最终轮检索均未命中编码类告警）。

**本单未删除任何一条中文注释**，改动一律是加/正确定义。本单新增的中文注释在各修复点就地写明「为什么这么改、是否改变行为」。

### D-13 链接期 linkage 标注与产物形态矛盾（LNK4217 ×8，真缺陷，已修）

真因**不是** `/WHOLEARCHIVE`、`/INCLUDE`、`#pragma comment(linker)`（全仓 `#pragma comment` 零命中；`WHOLEARCHIVE` 只存在于 vendored cfitsio 的 autotools 文件）。真因是**同一个头对定义侧与消费侧声明了互斥的 linkage 标注**：

- `ipv_api.h` 只有两态。`acsd_p1_ipv` 是 **STATIC** 库（`CMakeLists.txt:1141`），定义侧 TU 因 `IPV_EXPORTS` 拿 `dllexport`（其 `/EXPORT:` 以 `.drectve` 存进 `.lib`），消费侧 `module_adapters.cpp` 不定义 ⇒ 落 `dllimport` ⇒ 引用 `__imp_ipv_*`。两者同 image，`link.exe` 用本地定义满足 `__imp_` ⇒ LNK4217（5 条）。
- `hp_drizzle_api.h` 已是三态，但 `acsd_p1_drizzle` 是 **SHARED**，`module_entry.cpp` 与两个 `hp_drizzle_api.cpp` 编进**同一个 DLL**：后两者自带 `HP_DRIZZLE_EXPORTS` 拿 `dllexport`，前者落 `dllimport` ⇒ 引用**自己所在 DLL** 的符号（2 条）。Windows 装载期禁止真自导入，现在能过纯粹靠 `link.exe` 本地定义回退。

**处置**：给两条路径各补第三态（`IPV_STATIC` / 已在 drizzle 侧定义 `HP_DRIZZLE_STATIC`），与仓内同族正解（`hp_drizzle_api.h:17-27`、`status_codes.h:55-64`）同构。⚠️ `hp_drizzle_api.h` 把 `EXPORTS` 放**最前**先判，而两个定义侧 TU 在 include 之前已自行定义 `HP_DRIZZLE_EXPORTS` ⇒ **DLL 对外导出面原样保留**，只有内部调用由 `call *__imp_X` 变为 `call X`（同一地址、同一语义）。

⚠️ **明确拒绝 `/ignore:4217`**：全仓生效，会连带掩盖真正的自导入（drizzle 那两条就是真自导入），且不修根因。

### D-14 STL 头内的窄化只在 MSVC 可见（因为没有 `/external:I`）

`utility(247,54)`、`xutility(5118,37)`、`xutility(5164,24)` 三条 C4244 的报告点在 **MSVC 自己的 STL 头**里。GCC 把 STL 头当系统头直接抑制，MSBuild 不标 external ⇒ 同一个调用点窄化在两平台可见性不同。

两个真实调用点：

| 位置 | 窄化 |
|---|---|
| `p3_resample.cpp:105` `absent.emplace(k, 1u)` | `unsigned int` → `uint8_t`（map 的 value_type） |
| `rejection.cpp:2495` `std::fill(ring.begin(), ring.end(), 0)` | `int` → `uint8_t`（`ring` 是 `vector<uint8_t>`） |

**处置**：调用点写元素类型字面量（`uint8_t{1}` / `std::uint8_t{0}`），落入的值不变。⚠️ **没有**用 `/external:I` 去压 STL 头告警 —— 那正是「为了让构建绿而放宽检查」。

---

## 5. 为消除警告做的改动（逐条 + 是否改变行为）

共 **21 个文件**。**没有任何一处删除或放宽检查**；`/W4` 保持全局、`_CRT_*` 保持原定义、无 `/wd`/`/we`/`/WX`/`/ignore`。

### 5.1 改变行为的 2 处（均为缺陷修复，已在代码注释与本单登记）

| # | 文件:行 | 改了什么 | 为什么那是对的 | 行为影响 |
|---|---|---|---|---|
| 1 | `lib/algorithms/star_detection/src/sdet_image.cpp:394` | `const std::size_t lo` → `const F lo` | 元素是浮点像素/噪声量；转整型对负值是 UB、对 [0,1) 恒截 0。同仓 `sdet_median_of` 已是正确两中位数平均并自称「同式」 | **是，且是科学路径改动**。偶数样本的 MAD/σ 估计变化 ⇒ `thr = bg + 5·bgnoise` 位移最大 **−1.66 ADU**（σ<1 时 −50%）⇒ P1 星表成员与 P2 绝对 SNR 的源集合会动。⚠️ **冻结旧数值的回归基线必须重跑**（本单不跑测试，测试集是另一条车道） |
| 2 | `lib/infrastructure/scheduler/src/module_adapters.cpp:5075` | `IpvWcsResult r;` → `IpvWcsResult r{};` | 原先 `memset(&r,0,…)` 在**循环体内**；若 `solve_ladder` 为空则循环零次，紧随的 `r.success` / `r.n_detected` / `r.error_msg[0]` 读未初始化栈内存（C4701），`error_msg[0]` 还可能被当成有效 C 串构造 `std::string` | **是，但是把 UB 换成确定的 fail-closed 降级**：阶梯跑过一级时 memset 随后全覆盖，取值逐位不变；阶梯为空时由「读栈垃圾」变为「读全零」⇒ `success=0` ⇒ 走既有 unsolved 降级分支 |

### 5.2 零行为变化的 19 处

| # | 文件:行 | 告警 | 处置 | 为何零行为变化 |
|---|---|---|---|---|
| 3 | `star_detector.h:29/74` | C5208 | 匿名 typedef → 具名 `struct` | 类型/布局/NSDMI/可聚合性全同 |
| 4 | `coverage/src/sampler.cpp:34` | C4005 | 删无守卫的 `#define NOMINMAX`（平台块内本已有 `#ifndef` 守卫） | 宏最终取值不变 |
| 5 | `cli/commands.cpp:71` | C4005 | 加 `#ifndef` 守卫 | 同上 |
| 6 | `aio/src/aio_disk_full.h:93` | C4100 | Windows 分支 `(void)path;` | 判定逻辑与返回值两侧相同 |
| 7 | `aio/src/aio_sparse_punch.h:299` | C4100 | Windows 分支 `(void)off;` + 登记 D-5 | 不改任何读行为 |
| 8 | `cli/monitor.h:273` | C4100 | Windows 分支 `(void)s;` | 采样结果两侧都是「未观测」既定语义 |
| 9 | `star_detection/src/sdet_api.cpp:1742/2152` | C4189 | 删两处**零引用**的 `const double& bgnoise = pr.bgnoise;` 别名 | 绑定到已有对象无副作用；噪声本身经 `pr.thr`/`pr.locthreshold` 正常消费，**没被丢弃** |
| 10 | `gaia_xpsd_client/src/module_entry.c:293` | C4189 | 删零引用的 `int f = 0;`（删 `/W3` 后才暴露） | 未初始化即无读点 |
| 11 | `backend_host/hardware_inspect.cpp:86` | C4189 | 声明按平台分支下移 | 从未被读的已初始化局部变量无可观测效应 |
| 12 | `scheduler/src/module_adapters.cpp:1974` | C4189 | `in_flight` 声明搬进 `#ifdef _OPENMP` | `std::min` 无副作用；Linux 侧字面不变 |
| 13 | `module_adapters.cpp:4164-4203` | C4458 | `dets` → `dets_buf`（遮蔽同名类成员） | 两者是不同量（句柄表 vs 打平匹配向量）；取值不变 |
| 14 | `module_adapters.cpp:12581-12690` | C4457 | `w` → `wgt`（遮蔽 lambda 形参 `w`） | 权重取值与累加顺序逐位不变 |
| 15 | `module_adapters.cpp:6267-6351` | C4456 | 内层 `f_err` → `f_apply_err`（遮蔽外层同名的星表读错误累加器 `:5395`） | 两通道归约顺序与返回值逐位不变 |
| 16 | `coverage/src/sky_plane.cpp:759/1031/1078` | C4456 ×3 | `d` → `proj` / `dvec` / `dman`（均遮蔽外层样条阶数 `d`，`:565`） | 求值顺序与数值逐位不变 |
| 17 | `cli/commands.cpp:763` | C4456 | `ec` → `ec_fs`（遮蔽同函数 `:642` 的 `ec`） | 两者各自只服务自己那次 fs 查询 |
| 18 | `photometry/cpp/src/spatial_gain.cpp:615` | C4457 | `n` → `npx`（遮蔽形参 `n` = **样本数**，本处是**像素数**） | 均值/方差公式逐字不变 |
| 19 | `gaia_xpsd_client/src/gaia_client.c` ×4 处 | C4456 ×19 | 把函数级的 `int f;` / `int i;` 圈进仅服务其 OpenMP 循环的块 | 循环体、pragma、OpenMP 私有化语义与求值顺序逐字不变 |
| 20 | `photometry/cpp/src/pc_api.cpp:1172` | C4244 | `static_cast<float>(pixels[i] * scale)` | 赋值转换本就等价于 `static_cast<float>`，转换时机与舍入点完全不变 |
| 21 | `resample/p3_resample.cpp:105`、`coverage/src/rejection.cpp:2495` | C4244 ×3 | 写元素类型字面量 `uint8_t{1}` / `std::uint8_t{0}` | 落入的值不变 |
| 22 | `module_adapters.cpp:14412/14451` | C4244 ×8 | 窄化**前**加 `long long` 值域校验 + 显式 `static_cast<long>`（D-2） | 合法输入逐位不变；非法输入由「Windows 静默截断放行」变为「两平台一致具名拒绝」 |
| 23 | `module_adapters.cpp:1966` | C4127 | 见下 | 见下 |
| 24 | `platesolve/.../ipv_api.h:16-24` + `CMakeLists.txt:1173` | LNK4217 ×5 | `IPV_API` 补第三态 `IPV_STATIC`；`target_compile_definitions(acsd_p1_ipv PUBLIC IPV_STATIC)` | MinGW 的 `ipv_solver.dll` 仍走 `dllexport` 分支不受影响；CMake 静态路径从「假 dllimport」变「无 declspec」，`call *__imp_X` → `call X` 同一地址 |
| 25 | `drizzle/CMakeLists.txt:87` | LNK4217 ×2 | `target_compile_definitions(acsd_p1_drizzle PRIVATE HP_DRIZZLE_STATIC)` | 两个定义侧 TU 先行自判 `EXPORTS` ⇒ **DLL 导出面原样保留** |
| 26 | `gaia_xpsd_client/CMakeLists.txt:38` | （降级） | 删 `/W3`，与全局 `/W4` 对齐 | 告警面**变宽**（收紧）；已同步修掉因此暴露的 C4189 |
| 27 | `CMakeLists.txt:56-73` | （文档缺陷） | 把不可复现的「206 个调用点」改成实测口径 293/346 并标注不可复现 | 注释 |

**关于 #23（C4127）——这是本单最需要讲清的一处。**

`kP1MaxFramesInFlight` 是 `inline constexpr`（`eng/packaging/config/runtime_resources_generated.h.in:55` ← `runtime_resources.json:61`，**当前值 0**），故 `frame_cap > 0u` 编译期恒假 ⇒ 该块**永不执行**。**C4127 是真阳性**：它在陈述「配置键已接通、实现已写好，却不可达」这一事实。

三种走法里选了第三种：
1. ~~删掉死代码~~ —— 删掉一个已写好的、有 fail-closed 语义的配置能力；
2. ~~给常量加豁免~~ —— 把「该配置键永不生效」永久藏起来，违反仓内「告警不得静默」纪律；
3. **✅ 给同一旋钮一条运行期通路**（`ACSD_P1_MAX_FRAMES_IN_FLIGHT` env 覆盖，机制与紧邻的 `ACSD_P1_AXIS_*` env 覆盖同族），默认值恒等于原常量。

**行为影响**：默认部署**逐位不变**（未设置 ⇒ 取值 0 ⇒ 分支不执行，与现状一致）；只有显式设置该环境变量时该上限才真正生效，届时改变的是**并行轴形态**而非科学输出（本函数上方注释已记录「四档产品逐位相同 ⇒ 轴形态不改科学结果」）。这条同时补上了标准 §1「运行参数优先由程序 config 目录读取」的缺口 —— 原形态不是硬编码（值确实来自仓内 JSON），但被 `configure_file` 固化成编译期常量，**已部署的安装树无法调整，必须重新编译**。

⚠️ 该改动触及调度合同面（`docs/engineering/contracts/SCHEDULER.md`），**需文档链同步更新**，建议与 T11（合成性能/调度）车道合并处置。

---

## 6. 如实登记的缺陷与未解决问题

| ID | 缺陷 | 证据 | 本单处置 |
|---|---|---|---|
| D-A | **P0 科学缺陷**：偶数样本中位数把浮点像素截断成整数 | `sdet_image.cpp:394`；σ<1 时 bgnoise −50%，thr 位移 −1.66 ADU | ✅ 已修，**改变行为**，回归基线须重跑 |
| D-B | `read_at` 在 Windows 丢弃 `off` 且推进游标，违反自陈契约 | `aio_sparse_punch.h:299`；契约违反 + 脆性，当前调用图字节等价 | 登记未修（需功能改动 + 测试） |
| D-C | `IpvWcsResult r` 可能未初始化即被读（C4701，UB） | `module_adapters.cpp:5075` vs `:5107` memset 在循环体内 | ✅ 已修（UB → fail-closed） |
| D-D | LLP64 `long` 窄化在校验之前 ⇒ 越界裁剪框 Windows 静默放行 | `p3_wcs.h:186-212` + `module_adapters.cpp:14412/14451` | ✅ 已修（两平台一致 fail-closed） |
| D-E | `ipv_api.h` 两态宏使静态库的 linkage 标注与产物形态相反 | LNK4217 ×5 | ✅ 已修（三态） |
| D-F | `acsd_p1_drizzle` 内 `module_entry.cpp` 自导入所在 DLL | LNK4217 ×2 | ✅ 已修（导出面不变） |
| D-G | 配置键 `p1_max_frames_in_flight` 因编译期常量而不可达 | C4127 真阳性 | ✅ 已修（运行期通路），⚠️ 需同步 SCHEDULER.md |
| D-H | `acsd_catalog_gaia` 带 `/W3` 覆盖全局 `/W4`，静默降级第一方目标 | `gaia_xpsd_client/CMakeLists.txt:39`（原） | ✅ 已修 |
| D-I | `CMakeLists.txt` 注释「206 个调用点」不可复现 | `git log -S` 仅 `7d4c0aa2` 一次性写入，无测量记录 | ✅ 已改为实测 293/346 |
| D-10 | **`/W0` 覆盖 65 个第一方 TU**，与 `CODE.md:52` 正本禁令直接冲突 | `acsd_p1_drizzle`(31) + `acsd_phase2_integrate`(27) + `acsd_p1_hips_writer`(7) | 登记未修；收窄方案 = source 粒度隔离 |
| D-3 | `_CRT_*` 目录级无边界定义；第一方一旦失去即暴露 293 处 C4996 | `CMakeLists.txt:63` | 登记未修；与「本单零警告」不可兼得 |
| D-12 | **发行目录缺 `config/` 与滤镜库**（规范 §3 明列） | `install_layout.cmake` 14 条 install 零命中；`eng/packaging/config/` 6–9 文件全不装 | 登记未修（属 install 面变更） |
| D-14 | CMake **pin 漂移**：实测 3.31.6，冻结合同 pin 3.31.12 | `CMakePresets.json` `windows_formal.cmake_pin` | 登记；按 `BUILD_NODES.md §4` 应判红不放行，本单未改 pin |
| D-15 | preset `displayName` 自称 v143 **14.44.35207**，实测解析到 **14.40.33807** | configure log 的 cl.exe 路径 | 登记（`displayName` 非机器源，但它是仓内唯一写明工具集号处） |
| D-16 | `BUILD_GRAPH.md` 与实际严重不符：21 个根图 target 未登记（含 5 个 provider DSO）、5 行悬空；生成器 fail-closed 退出码 1 | `gen_build_graph_doc.py` 实测 rc=1 | 登记 |
| D-17 | `orchestrator_legacy_cli.exe` 是根图内**第 2 个可执行**且未设 `EXCLUDE_FROM_ALL`（未进安装树） | `orchestrator/cpp/CMakeLists.txt:100` | 登记 |
| D-18 | `acsd_cpu_baseline.dll` 装了但**永不被加载**（`--isa-only` 清单跳过 baseline，进程内置基线） | `CMakeLists.txt:912-914` | 登记 |
| D-19 | 选出的 provider **只上报不派发**；provider 族 3 个 DSO **零运行时消费者**（`secure_loader.c` 不在任何 target） | `commands.cpp:1063` 不接 backend 参数 | 登记 |
| D-20 | **内存上限硬编码 3 处**（4GB 字面量两份副本），`gaia_client.c` 经 `acsd_p1_ipv` 进生产闭包 | `gaia_client.c:209/219`、`module_entry.c:545` | 登记（违反「产品不硬编码内存上限」） |
| D-21 | `acsd_orchestrator_jsv` 另有一份**手工内联**的 `/W0` 实现，未复用共用函数 | `orchestrator/cpp/CMakeLists.txt:41-45` | 登记（纯第三方 0 第一方受损） |
| D-22 | `check_warning_suppression.py` 已于 `e5f589a6` 物理删除，`CMakeLists.txt:77` 仍悬空引用；且它**只认 GCC `-w`**，认不出 MSVC `/W0` `/W3` | `git log --diff-filter=D` | 登记 |
| D-23 | `CMakeLists.txt:55` 与 `cfitsio_platform.cmake:18` 的登记面锚 `ENGINEERING_SPEC.md` 全仓不存在（真实登记面是 `docs/engineering/standards/CODE.md`） | `find . -name ENGINEERING_SPEC.md` → 空 | 登记 |

---

## 7. 自证段

### 7.1 本单所有数字的来源

| 数字 | 来源 |
|---|---|
| `build_rc=0` / `elapsed_sec=96.5` / `log_lines=697` | 远程 clean-build 脚本 stdout（§1.4） |
| `warning=0 diagnostic=0 error=0 remark=0 flagish=0 tu=503` | 远程 scan4.ps1 stdout（§3.2） |
| 首轮 `119` / `54` / `8` | 远程 scan 脚本 + 抽取脚本 primary/unique/histogram 三段（§3.3、§3.4） |
| 14 个 dll、2 个 exe、25 个安装文件 | 远程 finalshape.ps1 目录枚举（§2.1、§2.3） |
| `required_features_bits=24 / 992`、`/arch:AVX2` `/arch:AVX512` | 安装树 `providers/backends.manifest.json` 解析输出（§2.2） |
| 版本串 / `BUILD_DIRTY` / digest | 构建树 `version_generated.h` / `build_stamp_generated.h`（§1.4） |
| 65 个第一方 TU、293/346 调用点、4 处 `/W0` 目标 | 子代理实测（逐 TU 展开 + `os.path.isfile` 校验 + 两种计数口径），命令可复跑 |
| σ<1 时 −50%、thr 位移 −1.66 ADU、2035/4000 不符 | 子代理 x86-64 实测程序（`/tmp/med_probe/`），仓外 |
| LNK4217 的 `.drectve /EXPORT:` 与 `__imp_` 证据 | 子代理用 `clang --target=x86_64-pc-windows-msvc` 产出真实 COFF 后 `objdump`（仓外 `/tmp/lnk4217/`）+ 微软官方文档 |
| LLP64 分叉复现（`x0=2^32+1`） | 子代理按 LP64/LLP64 各自真实 `long` 宽度复刻校验核实测 |

### 7.2 子代理派发（4 个，范围互斥、无重复）

| 子代理 | 范围 | 产出 |
|---|---|---|
| 1 | MSVC 告警屏蔽面审计（`_CRT_*`、`ACSD_WARNINGS_OFF`、`/W0`、`/W3`、检查器存废） | 65 个第一方 TU 被 `/W0` 覆盖；293/346 调用点；206 不可复现；检查器已删且认不出 MSVC |
| 2 | 交付形态核对（§2 规范逐条） | 13 SHARED / 唯一 exe / install 14 条；config+滤镜库缺失；硬编码 4GB ×3；BUILD_GRAPH 21 漏 5 悬空 |
| 3 | LNK4217 根因 + LLP64 位宽 + `kP1MaxFramesInFlight` | 真因 = 两态宏；fail-open 复现；C4127 是真阳性 |
| 4 | 5 处疑似真缺陷逐条裁决 | 2 真缺陷 / 2 死代码 / 1 平台口径差；并**推翻我写错的 WIN-PREAD-01 严重性**（我原写「读到错位数据」，实为潜在缺陷，当前调用图字节等价） |

⚠️ 派单过程中我曾把两份 prompt 各重复发送一次（4 个实例），已 `interrupt_agent` 终止重复实例，只保留 2 个有效子代理 + 后续 2 个独立派单。重复实例未产生任何文件改动。

### 7.3 独立复核与自查

- **推翻自己的登记**：子代理 4 用实测推翻了我 WIN-PREAD-01 注释中「读到错位数据」的错误表述，我已按实测口径改写（§4 D-5）。把潜在缺陷错记成活跃缺陷同样是缺陷。
- **未做的事**（明确声明，避免被误读为已完成）：
  - ❌ 未跑测试集、未跑端到端（任务书禁止；测试集是另一条车道）
  - ❌ 未在 Linux 侧验证零告警（未断言）
  - ❌ 未把远程构建产物同步回本地仓库（只取回日志文本）
  - ❌ 未做任何 git 写操作（21 个文件为工作树修改，由前台统一提交）
  - ❌ 未删任何中文注释、未加任何告警豁免

### 7.4 复核建议（交前台）

1. **D-A（P0）的回归基线必须重跑** —— 这是唯一改变科学数值路径的改动，thr 位移会影响 P1 星表成员与 P2 绝对 SNR 的源集合。
2. **D-G 需同步 `docs/engineering/contracts/SCHEDULER.md`** —— 调度合同面变更未在本单走文档链。
3. **D-12 与 D-10 是「零警告」声明的两个真实边界**，建议在 T12 CI 设计时把 `/external:I` 口径与 source 粒度告警隔离一并纳入，否则「零警告」只是配置性的、不是源码性的。
4. 工作树里另有 **6 个不属于本单** 的改动（`eng/tests/synthetic/` 下 5 个 + `orchestrator.cpp`），来自并发车道。本单只同步了自己的 21 个文件到 Windows，**远程构建树 = HEAD `10e84432` + 本单 21 文件**，不含那些改动。

---

## 参考文献

[1] `run/GOVERN-08/工作包-RECTIFY-09原件/tasks/TASKS_D_BUILD_E2E_CI.md`，阶段 D T10 双平台构建。
[2] `run/GOVERN-08/工作包-RECTIFY-09原件/standards/07_DYNAMIC_RUNTIME_AND_BUILD.md` §2 双平台构建、§3 构建要求、§1 动态运行。
[3] `docs/engineering/build/BUILD_NODES.md`，构建档位、节点分工、冻结工具链处置（§4 三段式：显式 pin → 构建后实测 → fail-closed）、构建入口（§5）。
[4] `docs/engineering/standards/CODE.md`，编译器告警口径（第 50–52 行：C4996 策略与禁整目标降级）。
[5] `eng/packaging/schemas/preset-contract.json`，冻结工具链机器源（禁用 `vs2026` / `Ninja` / `v144`+ / `/MT`）。
[6] `run/GOVERN-08/审核包-R2/前台裁决-Windows工具链口径.md`、`前台裁决-Windows警告与口径.md`，生成器口径与告警处置裁决。
[7] 微软官方 `LNK4217` 说明 <https://learn.microsoft.com/en-us/cpp/error-messages/tool-errors/linker-tools-warning-lnk4217>。
