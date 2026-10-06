# T10 之一：Linux 平台全量构建，零编译警告

基线：仓库 `/workspace/Astro CS Database`，本单开工时 HEAD = `10e84432a402fbd131cbc37cb06e299b96072015`。
判据：`run/GOVERN-08/工作包-RECTIFY-09原件/standards/07_DYNAMIC_RUNTIME_AND_BUILD.md` §2（双平台构建）、§3:29（构建产物结构）、§1:5-12（动态运行）。

⚠️ **行号引用约定**：本文中「§N:M」= 该标准的第 N 节第 M 行。经复核修正：产物结构要求在 **§3:29**（不在 §2），引用已全量改正。

**一句话结论**：Linux 全量构建通过，编译警告实测为零（完整检索命令与输出见 §3）；但这个「零」只在**当前旗标覆盖面**下成立 —— 55 个 target 中有 **33 个从未接受过任何告警检查**，本单已如实登记（§5）。为消除警告共改 **4 处**（全部为同一缺陷类），**行为零变化**（逐条论证见 §4）。登记缺陷 **13 条**（§5），其中 **4 条是真实功能缺陷**而非告警问题。

⚠️ **基线漂移**：本单执行期间前台已把 HEAD 推进到 `1ef0849c`（Windows 侧车道提交），且该提交**已吸收本单的 4 处修复**。本单全部数据仍以派单指定的 `10e84432` 为基线；§5.1 D4 的 Clang 缺陷已额外在 `1ef0849c` 上复测，**仍然存在**。

---

## 1 构建命令与环境

### 1.1 工具链版本（实测）

```
$ cmake --version | head -1
cmake version 3.31.6
$ gcc --version | head -1
gcc (Debian 14.2.0-19) 14.2.0
$ g++ --version | head -1
g++ (Debian 14.2.0-19) 14.2.0
$ clang --version | head -1
Debian clang version 18.1.8 (18+b1)
$ ninja --version
1.12.1
$ make --version | head -1
GNU Make 4.4.1
$ uname -sr
Linux 6.12.107+deb13-amd64
$ nproc
16
$ grep -m1 'model name' /proc/cpuinfo | cut -d: -f2
 Genuine Intel(R) CPU 0000 @ 1.70GHz
$ grep -m1 '^flags' /proc/cpuinfo | tr ' ' '\n' | grep -cE '^avx512'
0
$ grep -m1 '^flags' /proc/cpuinfo | tr ' ' '\n' | grep -E '^(avx2|fma|avx)$' | tr '\n' ' '
avx avx2 fma
```

本机 CPU **有 AVX2+FMA、无 AVX-512**。这个事实对 §2 有直接意义：本单同时证明了
「AVX-512 计算核在无 AVX-512 的宿主上仍会被完整构建出来」（构建期无宿主 ISA 探测）。

### 1.2 构建目录纪律

派单要求「构建目录必须在仓库外或已被忽略的位置」。本单**两处都做到了**：

| 构建 | 源树 | 二进制目录 | 位置判定 |
|---|---|---|---|
| **A（权威证据）** | `/tmp/acsd-t10/pristine` | `/tmp/acsd-t10/pristine-build` | 仓库外 |
| B（旗标覆盖面探针） | 同上 | `/tmp/acsd-t10/build-probe` | 仓库外 |

仓库根的 `build/` 目录**未被本单使用**：该目录有 2026-09-29 的陈旧产物（`build/linux-control/acsd`，时间戳 `9月 29日 06:28`），复用它会产生「陈旧产物伪造构建成功」的风险，与派单点名的事故形态一致。本单拒绝复用。

`/build/` 确实已被忽略（`.gitignore:20` `/build/`，实测 `git check-ignore -v build/linux-control` → `.gitignore:20:/build/	build/linux-control`），因此即使按 `CMakePresets.json` 的 preset 落到仓库内也合规 —— 但本单仍选择仓库外，理由见 §7.3。

### 1.3 构建命令（逐字可复跑）

```bash
# ── 步骤 1：导出 HEAD 的纯净快照（只读 git 操作，不触碰仓库历史与索引）──
mkdir -p /tmp/acsd-t10/pristine
cd "/workspace/Astro CS Database"
git archive HEAD | tar -x -C /tmp/acsd-t10/pristine
cp -a .git /tmp/acsd-t10/pristine/.git     # CMake 对非 git 树 fail-closed（CMakeLists.txt:79-89）

# ── 步骤 2：施加本单的 4 处修复（§4）──
# ⚠️ 这一步不可省略：快照 = HEAD + 本单补丁，不是裸 HEAD。
#    裸 HEAD 的构建结果见 §3.1（恰好 1 条 -Wclass-memaccess）。
#    施加后核验：git -C /tmp/acsd-t10/pristine status --porcelain 应只列 3 个文件。

# ── 步骤 3：配置 + 构建（二进制目录在仓库外）──
cmake -S /tmp/acsd-t10/pristine -B /tmp/acsd-t10/pristine-build \
      -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release
cd /tmp/acsd-t10/pristine-build && make -j16 > /tmp/acsd-t10/pristine-build.log 2>&1
```

耗时：配置 2.6 s，编译 + 链接 **8 分 08 秒**（`11:21:29 → 11:29:37`，取自两个日志文件的 mtime）。

**为什么用仓库外快照而不是直接构建仓库**：本单执行期间，工作树被**并发车道**写入了 16 个与本单无关的源文件（`git status` 从开工时的干净变为 19 个 modified）。直接构建仓库会得到一份「混合了他人在途改动的树」，一旦出现/不出现某条告警就无法归因。快照法把归因问题消除。已核对快照内仅含本单的 4 处修改（§4）。

---

## 2 目标与产物清单（实测）

### 2.1 规模

| 项 | 数 | 取证命令 |
|---|---|---|
| 构建 target | **55**（有 `flags.make` 者） | `find . -name flags.make -path '*/CMakeFiles/*.dir/*' \| wc -l` |
| 编译单元（`.o`） | **503** | `find . -name '*.o' \| wc -l` |
| ├ 本项目自有 | 257 | `find . -name '*.o' ! -path '*third_party*' \| wc -l` |
| └ vendored 第三方 | 246 | `find . -name '*.o' -path '*third_party*' \| wc -l` |

编译单元数取自 `make` 日志的 `Building C/CXX object` 行（503）与产物树 `.o` 计数（503）两条独立路径，互相印证。

### 2.2 可执行文件

```
$ find . -type f -perm -u+x ! -name '*.so' ! -name '*.a' | while read f; do
    file -b "$f" | grep -q ELF && echo "$f"; done
./lib/infrastructure/pipeline/orchestrator/cpp/orchestrator_legacy_cli
./CMakeFiles/FindOpenMP/ompver_CXX.bin          ← CMake 探针，非产品
./CMakeFiles/FindOpenMP/ompver_C.bin            ← CMake 探针，非产品
./CMakeFiles/3.31.6/CompilerIdCXX/a.out         ← CMake 探针，非产品
./CMakeFiles/3.31.6/CompilerIdC/a.out           ← CMake 探针，非产品
./CMakeFiles/3.31.6/CMakeDetermineCompilerABI_CXX.bin  ← CMake 探针，非产品
./CMakeFiles/3.31.6/CMakeDetermineCompilerABI_C.bin    ← CMake 探针，非产品
./acsd
```

- **产品可执行 = 1 个**：`./acsd`（`ELF 64-bit LSB pie executable, x86-64`）。
- 构建图内另有 **1 个** `orchestrator_legacy_cli`（`lib/infrastructure/pipeline/orchestrator/cpp/CMakeLists.txt:100`，经 `CMakeLists.txt:529 add_subdirectory` 入图）。它**未被任何 `install()` 规则安装**，因此不进入发行目录 —— 但它确实被编译出来，是 §2:22「不设子入口」的字面违反，登记为 D9。

### 2.3 动态库（发行目录实测）

`cmake --install` 到 `DESTDIR=/tmp/acsd-t10/install`，实际落地的目录树：

```
/usr/local/acsd                                   ← 唯一可执行
/usr/local/acsd.product.json
/usr/local/libacsd_io.so
/usr/local/libacsd_runtime.so
/usr/local/licenses/{CFITSIO_LICENSE,LICENSE-INDEX,nlohmann_json.MIT}.txt
/usr/local/modules/acsd_catalog_gaia.so
/usr/local/modules/acsd_noop.so
/usr/local/modules/acsd_p1_calibration.so
/usr/local/modules/acsd_p1_cosmetic.so
/usr/local/modules/acsd_p1_drizzle.so
/usr/local/modules/acsd_p1_hips_writer.so
/usr/local/providers/acsd_cpu_baseline.so
/usr/local/providers/acsd_cpu_avx2.so
/usr/local/providers/acsd_cpu_avx512.so
/usr/local/providers/acsd_cpuprov_baseline.so
/usr/local/providers/acsd_cpuprov_avx2.so
/usr/local/providers/acsd_cpuprov_avx512.so
/usr/local/providers/backends.manifest.json
/usr/local/providers/providers.manifest.json
/usr/local/schemas/{acsd-product,dependency-lock,install-tree-contract,preset-contract}.schema.json
```

**发行目录 = 1 个可执行 + 14 个动态库 + schemas + licenses + 2 份 manifest。**

### 2.4 按指令集分的计算核（§2:23 的核心要求）

构建树中 6 个计算核动态库**全部存在**：

```
$ find . -name 'acsd_cpu_*_*.so' -o -name 'acsd_cpuprov_*.so' | sort
./acsd_cpu_baseline.so                    ← 注意：构建树根目录，不在 providers/
./providers/acsd_cpuprov_avx2.so
./providers/acsd_cpuprov_avx512.so
./providers/acsd_cpuprov_baseline.so
./providers/acsd_cpu_avx2.so
./providers/acsd_cpu_avx512.so
```

「分别编译」不是三个同名库的复制，而是**真的三份不同的机器码**。用 `objdump` 统计向量寄存器宽度：

```
$ for f in acsd_cpu_baseline.so providers/acsd_cpu_avx2.so providers/acsd_cpu_avx512.so \
           providers/acsd_cpuprov_baseline.so providers/acsd_cpuprov_avx2.so providers/acsd_cpuprov_avx512.so; do
    d=$(objdump -d "$f")
    printf "%-40s ymm=%-4s zmm=%-4s\n" "$f" "$(printf '%s' "$d" | grep -c '%ymm')" "$(printf '%s' "$d" | grep -c '%zmm')"
  done
acsd_cpu_baseline.so                 ymm=0   zmm=0     ← 纯标量基线
providers/acsd_cpu_avx2.so           ymm=53  zmm=0     ← 真 AVX2
providers/acsd_cpu_avx512.so         ymm=48  zmm=48    ← 真 AVX-512（zmm 出现）
providers/acsd_cpuprov_baseline.so   ymm=0   zmm=0
providers/acsd_cpuprov_avx2.so       ymm=6   zmm=0
providers/acsd_cpuprov_avx512.so     ymm=0   zmm=0     ← ★ 见 D3
```

- 第一族（backend ABI，`acsd_cpu_get_api_v1`）**三名齐全且真差异化**：基线 0 向量指令，AVX2 有 ymm，AVX-512 另有 zmm。
- 第二族（provider ABI，`acsd_provider_query_v1`）：基线与 AVX2 可分辨，**但 `acsd_cpuprov_avx512.so` 里一条 512 位指令都没有** —— 它带着 `-mavx512f -mavx512bw -mavx512vl -mavx512dq` 编译（`CMakeLists.txt:1020-1023`），但本 TU 集合上向量化器没选 512 位形式。该库**功能正确**（标量代码照跑），但「按指令集分别编译」在这一族只是名义成立。登记为 D3。

### 2.5 运行期选取与回退（只读源码核对）

规范 §2:23 要求「调度器运行时选取」、§1:11 要求「运行时检测 CPU 能力并结合画像选取，缺失时回退基线」。本单不跑端到端，只做源码与产物核对：

| 环节 | 实现 | 证据 |
|---|---|---|
| 构建期不探测宿主 ISA | `acsd_cpu_avx512` 无条件声明、无 `if(CMAKE_HOST_SYSTEM_PROCESSOR)` | `CMakeLists.txt:851-874`；本机无 AVX-512 而 `acsd_cpu_avx512.so` 确实产出（§2.4） |
| 运行期能力检测 | `__builtin_cpu_supports` + OSXSAVE/XGETBV | `lib/infrastructure/benchmark/backend_host/cpu_features.cpp:37-63` |
| 本机 ISA 不支持 → 回退 | `required_features_bits ⊄ detected` ⇒ `FALLBACK_BASELINE` | `lib/infrastructure/benchmark/backend_host/backend_loader.cpp:124-129`（实测该处代码为 `if ((detected_features & e.required_features) != e.required_features) return fail(LoadResult::FALLBACK_BASELINE, "unsupported ISA for " ...)`） |
| so 缺失 / 哈希不符 / `dlopen` 失败 / 符号缺失 / 握手失败 / self_test 失败 | 六条路径全部收敛 `FALLBACK_BASELINE` | `backend_loader.cpp:116-129`、`:163-188` |

回退终点是**静态编进可执行的进程内建基线**（`CMakeLists.txt:760` `acsd_cpu STATIC` 含 `baseline_backend.cpp`，`CMakeLists.txt:764`），实测 `nm -C acsd | grep -c acsd_backend` = **3** 个符号在主程序内。

**但**：读取侧存在一处断裂 —— `acsd_backend_api_v1 api` 在选核后被填充却未接入 `run_pipeline`，生产计算路径不走变体 `.so`（证据见 §5 D11）。这条不属「构建」范畴，但影响 §2:23 的实质满足度，必须登记。

---

## 3 警告检索的完整命令与结果（证明为零）

### 3.1 修复前的对照测量

在**未打任何补丁**的纯净 HEAD 上构建（目录 `/tmp/acsd-t10/build-make`，同一工具链、同一生成器、同一配置类型）：

```
$ grep -nE 'warning:|note:' /tmp/acsd-t10/build-make.log
/tmp/.../orchestrator.cpp:1578:16: warning: 'void* memset(void*, int, size_t)' clearing an object of
  non-trivial type 'struct SDetParams'; use assignment or value-initialization instead [-Wclass-memaccess]
/tmp/.../star_detector.h:29:16: note: 'struct SDetParams' declared here
```

**恰好 1 条告警**（+1 条 `note:` 指引行）。全树 503 个编译单元中仅此一处。

### 3.2 修复后的权威测量（证明为零）

日志：`/tmp/acsd-t10/pristine-build.log`，616 行 / 70623 字节，退出码 `PRISTINE_EXIT=0`。

```
$ L=/tmp/acsd-t10/pristine-build.log

$ grep -ciE 'warning|警告|remark' $L
0

$ grep -nE 'warning:|Warning:|warning ' $L
（无输出；grep 退出码 1 = 零命中）

$ grep -nE '^[^ ]+:[0-9]+:[0-9]+:' $L
（无输出 —— 没有任何一行形如 <file>:<line>:<col>: 的 GCC/Clang 诊断）

$ grep -nE 'collect2|ld:|ar:|/usr/bin/ld' $L
（无输出 —— 链接器零告警）

$ grep -nE 'error:|Error [0-9]|FAILED|Stop\.' $L
（无输出 —— 零错误）

$ grep -nE '\bnote:' $L
（无输出 —— 零诊断备注）

$ grep -o 'PRISTINE_EXIT=[0-9]*' $L
PRISTINE_EXIT=0
```

### 3.3 为什么「grep 为 0」不是「没看到警告」

派单明确要求区分这两者。三条自查：

1. **GNU Make 会不会吞掉告警？** 不会。Make 只把子进程 stdout/stderr 原样转发到自身输出；编译器写告警到 stderr，Make 不解析、不过滤。本单的 1 条对照告警就是通过同一条链路（`make` 默认输出，非 `-s`）被抓到的 —— 链路本身已被证明有效。
2. **日志会不会被截断？** 616 行 / 70623 字节，`wc` 实测；`PRISTINE_EXIT=0` 行在日志**末尾**，说明 Make 正常收尾而非被 kill。
3. **编译单元有没有被整体跳过？** 503 个 `.o` 产物 + 日志里 503 条 `Building C/CXX object` 行，两条独立计数一致；`Built target` 行 57 条。
4. **有没有 target 因条件而根本没编？** 有：`acsd_probes`（`CMakeLists.txt:24` option `ACSD_PROBES` 默认 `OFF`，`:489` 声明）。因此本单结论的准确措辞是「**默认配置下被编的全部 target 的全部编译单元零告警**」，而不是「所有 target 零告警」。该条并入 D5 的覆盖面问题一并处置。

### 3.4 独立复现（第二名复核代理，不复用本单任何构建目录）

该代理用 `git archive HEAD` 与 `git clone` 两条**互相独立**的路径各自导出干净 HEAD（`git status --porcelain | wc -l` = 0），重新配置、重新编译，得到：

```
EXIT=0
$ grep -c 'warning:' /tmp/t10-verify/build.log
1
$ grep 'warning:' /tmp/t10-verify/build.log
…/orchestrator.cpp:1578:16: warning: 'void* memset(void*, int, size_t)' clearing an object of
  non-trivial type 'struct SDetParams'; use assignment or value-initialization instead [-Wclass-memaccess]
```

**与 §3.1 逐字一致**：裸 `10e84432` = 恰好 1 条告警，位置相同。两份独立导出的树、两个独立构建目录、同一结论 —— §3.1 的对照数据因此不是孤证。

该代理同时复跑了 Clang 交叉构建，并独立测出 §5.1 D4 与 §5.2 D5 的数字（`-w` 静默 65 单元 ⇒ 9 条；无 `-W` 旗标 121 单元 ⇒ 139 条）。它对本单证据提出的全部更正见 §6.2。

### 3.5 未找到推翻点

两名复核代理合计对「零告警」结论发起 7 项攻击（构建是否真跑完、Make 是否吞告警、编译单元是否被跳过、target 是否条件关闭、`-w` 盲区、`Clang` 交叉、构建入口合规性）。结果：

- **零告警本身**：未被推翻，且被独立复现加固。
- **4 处改动**：未被推翻，独立代理在干净树上复核了缺陷位置与 `-Wclass-memaccess` 诊断文本。
- **产物形态数字**（1 可执行 / 14 .so / 6 计算核 / ymm·zmm 分布）：未被推翻，独立代理逐条重测一致。
- **被推翻的是措辞与范围**（pristine 不是裸 HEAD、§3:29 而非 §2:29、60% 需并列 65.7% 口径），已全部改正并记入 §6.2。

---

## 4 为消除警告所做的改动

共 **4 处**，全部是同一缺陷类：**对非平凡类型做 `memset` 清零**（GCC `-Wclass-memaccess`）。**行为零变化**，逐条论证如下。

### 4.0 这类改动为什么是对的

`SDetParams`（`lib/algorithms/star_detection/include/star_detector.h:29-73`）的每个成员都带**默认成员初始化器（NSDMI）**，因此它的默认构造不是平凡的。对这类对象 `memset` 置零：

- 形式上越过类型自身的构造契约（对非平凡类型清零）；
- 编译器无法保证语义，GCC 直接判为 `-Wclass-memaccess` 并给出正解：赋值或值初始化。

本单采用的正解是**值初始化** `SDetParams x{};`。这不是「换个写法骗过编译器」，而是回到语言定义的构造路径。

### 4.1 逐条

**改动 1** — `lib/infrastructure/pipeline/orchestrator/cpp/src/orchestrator.cpp:1577`

```diff
-    SDetParams sdet_params;
-    std::memset(&sdet_params, 0, sizeof(sdet_params));
+    SDetParams sdet_params{};
     sdet_params.structureLayers = 5;
```

- **为什么对**：消除对非平凡类型的 `memset`，这是编译器指定的正解。
- **是否改变行为**：**否**。其后 9 个字段被逐字段显式赋值（`:1579-1590`），取值与原代码逐字相同。剩下 4 个未被赋值的字段 `psfFwhmLoRatio` / `psfFwhmHiRatio` / `maxPeakFraction` / `minQuarterMaxPixels`，其 NSDMI 恰好是 `0.0f / 0.0f / 0.0f / 0`（`star_detector.h:70-73`），与原 `memset` 得到的全 0 **逐字段相同**。故新旧两条路径产出的对象在 `sdet_create` 读到的每一个字节上等价。
- 这条修复消除了 §3.1 中**唯一**那条真实告警。

**改动 2、3、4** — `lib/infrastructure/scheduler/src/module_adapters.cpp:4031`、`:4918`、`:5084`

```diff
-        SDetParams sp;
-        std::memset(&sp, 0, sizeof(sp));
+        SDetParams sp{};
```
（另两处分别为 `SDetParams sp{};` 与 `SDetParams sp_s{};`）

- **为什么对**：与改动 1 同因。
- **是否改变行为**：**否**。三处也都是「先清零、再逐字段赋 9 个字段」，未被赋的仍是同样那 4 个 NSDMI 为 0 的字段。
- **为什么在「零告警」的构建里也要改**：这三处当前**不产生**告警 —— 因为 `acsd_module_adapters` 恰好没挂告警旗标（§5 D1）。若只改 orchestrator 那处，一旦有人修好 D1 的旗标缺失，这三处会立刻变成 3 条新告警。把同类缺陷一次清干净，是让 D1 的修复不至于「一修就红」的前提。派单要求「若某条警告需要改动代码或构建配置才能消除，照改」，此处正是该要求的适用面。

### 4.2 顺带同步的注释

`lib/algorithms/star_detection/include/star_detector.h:26-28` 的原注释写着「对既有『先 memset(0) 再逐字段赋』的调用方**零影响**」。4 处调用点全部改完后，这句话已无指称对象，按 `AGENTS.md` §7「注释与代码同步」改为说明现行约定（值初始化；`memset` 因 `-Wclass-memaccess` 停用）。**纯注释，零行为变化。**

### 4.3 没有做的事（明确声明）

- **没有**新增任何 `-Wno-*`、`-w`、`#pragma GCC diagnostic ignored`。
- **没有**改动任何既有告警旗标（既没加也没减）。
- **没有**删除或放宽任何既有检查。
- **没有**改动 `CODE.md` 的 MSVC C4996 项目级豁免（那是 `CODE.md:50` 逐字要求的合规项）。
- **没有**跑测试、**没有**跑端到端。
- **没有**任何 git 写操作（`git archive` / `git rev-parse` / `git ls-files` / `git status` / `git diff` 全为只读；快照树的 `.git` 是 `cp -a` 的副本，本单未在其中执行任何 git 写命令）。

### 4.4 提请前台注意：并发车道改了同一批文件

本单执行期间工作树被并发车道持续写入（`git status` 从开工时的干净变为 21 个 modified，且仍在变动）。**`lib/infrastructure/scheduler/src/module_adapters.cpp` 与 `lib/algorithms/star_detection/include/star_detector.h` 被本单与并发车道同时修改**。已逐处核对本单的 4 处代码修改 + 1 段注释在最新工作树中**全部存活且未被覆盖**（`grep` 确认 `SDetParams sp{}` / `sp_s{}` / `sdet_params{}` 在位，`memset(&sdet_params` 已不存在）。

但因同文件有他人改动，`module_adapters.cpp` 的整体 diff 现已达 126 行，远超本单的 9 行。前台提交时应按 hunk 甄别，**不要整文件取用**。本单改动在文件中的定位：
- `module_adapters.cpp:4046` `SDetParams sp{};`（原 4031）
- `module_adapters.cpp:4935` `SDetParams sp{};`（原 4918）
- `module_adapters.cpp:5109` `SDetParams sp_s{};`（原 5084）
- `orchestrator/cpp/src/orchestrator.cpp:1577` `SDetParams sdet_params{};`
- `star_detection/include/star_detector.h:26-28` 注释段

---

## 5 如实登记的代码缺陷与未解决问题

共 **12 条**。前 3 条是**真实功能缺陷**，不是告警问题；其余为覆盖面/纪律/合规问题。

### 5.1 真实功能缺陷

**D1（严重）— `CMakeLists.txt:1348-1358` 声明顺序 bug，两个产品 target 的告警旗标被静默丢弃**

集中挂旗标的 `foreach` 名单里有 `acsd_module_adapters`（`:1351`）与 `acsd_cli_runtime`（`:1351`），循环体用 `if(TARGET ${tgt})` 守卫（`:1355`）；而两者的 `add_library` 分别在 **`:1363`** 与 **`:1407`**，都在循环之后。守卫取假，四个旗标一个都没挂，**且不产生任何错误**。

实测（`/tmp/acsd-t10/pristine-build`）：

```
$ find . -name flags.make -path '*acsd_module_adapters.dir/*' -exec grep '^CXX_FLAGS' {} \;
CXX_FLAGS = -O3 -DNDEBUG -std=gnu++17 -fopenmp
$ find . -name flags.make -path '*acsd_cli_runtime.dir/*' -exec grep '^CXX_FLAGS' {} \;
CXX_FLAGS = -O3 -DNDEBUG -std=gnu++17 -fopenmp
```

讽刺点：`:1352-1353` 的注释原文正是「少一个 target 就等于悄悄放松了 `-Wconversion` 的覆盖面」—— 而这条防线自己漏了两个 target。这两个 target 直接静态链进产品可执行（`CMakeLists.txt:1332` 链 `acsd_cli_runtime`，`:1413` 链 `acsd_module_adapters`），承载 Phase1/2/3 全部科学节点。**影响：`module_adapters.cpp`（16274 行产品最重的 TU）与 `runtime_client.cpp` 从未接受过任何告警检查。**

修法（建议，不在本单范围）：把 `foreach` 移到所有 `add_library` 之后，或改用 `target_compile_options` 集中在文件末尾统一施加。

**D2（严重）— `acsd_p1_ipv` 缺 `-fopenmp`，OpenMP 指令被编译器静默忽略**

`CMakeLists.txt:1141` 声明 `acsd_p1_ipv STATIC`，其源表含 `gaia_client.c`（`:1155`）与 `ipv_select.cpp` / `ipv_solver.cpp` / `ipv_triangle.cpp`。实测旗标：

```
$ find . -name flags.make -path '*acsd_p1_ipv.dir/*' -exec grep -E '^(C_FLAGS|CXX_FLAGS)' {} \;
C_FLAGS = -O3 -DNDEBUG
CXX_FLAGS = -O3 -DNDEBUG -std=gnu++17
```

而这些文件里有大量 OpenMP 指令：`gaia_client.c` **24 处** `#pragma omp`（含 `:419 omp critical`、`:1748/1751/1754 omp atomic`、多处 `omp parallel`），`ipv_select.cpp` / `ipv_solver.cpp` / `ipv_triangle.cpp` 各含 `omp parallel` / `omp for`。

实测编译器反应（用该 target 的真实旗标加 `-Wall -Wextra`，逐条重编）：

```
gaia_client.c:419:  warning: ignoring '#pragma omp critical'   [-Wunknown-pragmas]
gaia_client.c:1748: warning: ignoring '#pragma omp atomic'
...
（该 target 名下 36 条 -Wunknown-pragmas）
```

**后果**：这些 `parallel` / `for` / `atomic` 全部不生效，声明的并行化静默失效，代码实际串行运行。这直接违反 §1:6-9「调度器是真正的动态调度」。对照：`acsd_catalog_gaia` 编译同一个 `gaia_client.c` 时带 `-fopenmp`（实测 `C_FLAGS = ... -fopenmp`），同样旗标下 `-Wunknown-pragmas` 为 **0**，可排除「源码写法有问题」这一替代解释。

修法：给 `acsd_p1_ipv` 接上既有的 `acsd_openmp_link_if_unix()`。本单**未改** —— 改它会实质改变并行行为（从串行变并行），属行为变更，超出「消除告警」范围，须单独立项并评估数值影响。

**D3 — `acsd_cpuprov_avx512.so` 无任何 512 位指令**

见 §2.4。带 `-mavx512f -mavx512bw -mavx512vl -mavx512dq` 编译（`CMakeLists.txt:1020-1023`），产物 `ymm=0 zmm=0`，与 `acsd_cpuprov_baseline.so` 的向量指令分布无差异。第二族 provider 的「按指令集分别编译」目前只是名义成立。功能不受影响（标量代码正确），但性能承诺无着落。

**D4 — Clang 18.1.8 下整树编译失败（跨编译器缺陷，`1ef0849c` 上仍存在）**

`docs/engineering/build/BUILD_NODES.md` §2 规定「轻验证用 GCC，**调试与静态分析用 Clang**，两侧各走本平台的 preset」。实测用 Clang 走同一 preset 语义配置（`-DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++`），**构建直接失败**：

```
$ cmake -S /tmp/acsd-t10/pristine -B /tmp/acsd-t10/clang-build \
      -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
$ make -j8 ; echo $?
make: *** [Makefile:136：all] 错误 2        # CLANG_EXIT=2

module_adapters.cpp:16215:23: error: capturing a structured binding is not yet supported in OpenMP
module_adapters.cpp:16215:26: error: capturing a structured binding is not yet supported in OpenMP
module_adapters.cpp:16239:23: error: capturing a structured binding is not yet supported in OpenMP
module_adapters.cpp:16239:26: error: capturing a structured binding is not yet supported in OpenMP
module_adapters.cpp:16264:23: error: capturing a structured binding is not yet supported in OpenMP
（错误数=6）
```

触发形态（`module_adapters.cpp:16268-16273`，P1 节点工厂注册）：

```cpp
for (const auto& [d, spec] : p1_nodes) {          // 结构化绑定
  auto rr = registry.register_module(d);
  ...
  auto ff = registry.register_factory(
      d.module_id, [d, spec]() { return make_p1_node_module(d, spec); });  // ← lambda 捕获结构化绑定
```

`acsd_module_adapters` 带 `-fopenmp`，Clang 的 OpenMP outlining 无法处理捕获结构化绑定的 lambda，直接报 `error`（不是 warning）。P2 节点链（`:16296`）与 P3 节点链（`:16321`）同型，共 3 处 ×2 列 = 6 个错误。GCC 14.2 接受同一份源码。

**已在当前 HEAD `1ef0849c` 上复测，缺陷仍在**：

```
$ clang++ <module_adapters 的真实 D/I/FLAGS> -fopenmp -fsyntax-only \
      "…/lib/infrastructure/scheduler/src/module_adapters.cpp" 2>&1 | grep -c 'error:'
6
（module_adapters.cpp:16272 / 16296 / 16321，行号随文件增长平移）
```

**后果**：`BUILD_NODES.md` §2 指定的 Clang 静态分析车道**当前不可用** —— 不是「有告警」，是「根本编不过」。Linux 侧连「第二个编译器交叉验证」这条兜底都不存在。

**Clang 侧另有 160 条警告**（GCC 侧为 0），类别分布：`143 [-Wsign-conversion]`、`4 [-Wnon-c-typedef-for-linkage]`、`3 [-Wunused-but-set-variable]`、`2 [-Wunused-const-variable]`、`2 [-Wformat-security]`。其中 `-Wsign-conversion` 批量出现源于 GCC 的 `-Wconversion` 与 Clang 的 `-Wconversion` 覆盖集合不同（Clang 把符号转换单列为 `-Wsign-conversion`）—— 这是**同一份 `-Wconversion` 旗标在两个编译器上覆盖不同**的证据，属 `CODE.md:45`「按两平台语义一致原则定口径」要处理的真实差异。`4 [-Wnon-c-typedef-for-linkage]`（`extern "C"` 块内用 C++ typedef）则与并发车道在 `1ef0849c` 中做的 `star_detector.h`「匿名 typedef → 具名 struct」修复同源，说明该问题在 Windows 侧已被发现，Linux 侧仍有残留。

本单**未修** Clang 缺陷：修它要改 `module_adapters.cpp` 三处 lambda 的捕获方式（把结构化绑定先解包到具名变量再捕获），属产品注册路径的代码改写，超出「消除告警」范围，须单独立项并验证注册语义不变。

### 5.2 覆盖面缺陷（使「零警告」的适用范围远小于表面）

**D5 — 55 个 target 中 33 个（60%）从未接受过任何告警检查**

实测分档（`/tmp/acsd-t10/pristine-build`，遍历全部 `flags.make`）：

| 档位 | 个数 | target |
|---|---|---|
| 满档 `-Wall -Wextra -Wpedantic -Wconversion` | **16** | `acsd`、`acsd_common`、`acsd_core`、`acsd_cpu`、`acsd_cpu_avx2(_kernels)`、`acsd_cpu_avx512(_kernels)`、`acsd_phase1_noise/phot/session/stars/wcs`、`acsd_phase2`、`acsd_phase2_session`、`acsd_phase3_session` |
| 部分档 `-Wall -Wextra`（缺 `-Wpedantic`/`-Wconversion`） | **5** | `acsd_cpu_baseline`、`acsd_io`、`acsd_noop`、`acsd_runtime`、`acsd_p3_projection_wcs` |
| 仅 `-Wall`（连 `-Wextra` 都没有） | **1** | `acsd_infra_orchestrator` |
| **完全无任何 `-W` 旗标** | **28** | `acsd_aio`、`acsd_calibration`、`acsd_catalog_gaia`、`acsd_cli_process`、`acsd_cli_runtime`、`acsd_cpuprov_{baseline,avx2,avx512}`、`acsd_cpuprov_{avx2,avx512}_kernels`、`acsd_cpuprov_common`、`acsd_drizzle`、`acsd_hips`、`acsd_hips_properties`、`acsd_identifiability`、`acsd_io_adapter`、`acsd_module_adapters`、`acsd_p1_calibration`、`acsd_p1_cosmetic`、`acsd_p1_dpsf`、`acsd_p1_ipv`、`acsd_p1_sdet`、`acsd_p3_fits_output`、`acsd_p3_rsmp`、`acsd_phase1_photcal`、`acsd_phase1_product`、`acsd_product_io`、`orchestrator_legacy_cli` |
| **整 target `-w` 静默** | **5** | `acsd_cfitsio`、`acsd_orchestrator_jsv`、`acsd_p1_drizzle`、`acsd_p1_hips_writer`、`acsd_phase2_integrate` |

**其中 12 个是随安装树分发的产品件**（`eng/cmake/install_layout.cmake` 白名单）：`acsd_cpuprov_{baseline,avx2,avx512}`、`acsd_catalog_gaia`、`acsd_p1_calibration`、`acsd_p1_cosmetic`、`acsd_p1_drizzle`、`acsd_p1_hips_writer`。

**量化「若强加 `-Wall -Wextra` 会怎样」**：另配一个把 `-Wall -Wextra` 用 `CMAKE_C{,XX}_FLAGS` 铺到全树的探针构建（`/tmp/acsd-t10/build-probe`），把产生告警的 11 个源文件 `touch` 后**串行**重编（`-j1`），确保归因可靠，得到：

```
串行重编告警总数: 52
  36  acsd_p1_ipv           （24 gaia_client.c 的 -Wunknown-pragmas + 12 ipv_* 的）
   7  acsd_module_adapters
   4  acsd_p1_sdet
   2  acsd_catalog_gaia
   1  acsd_infra_orchestrator
   1  acsd_p1_dpsf
   1  acsd_hips
类别分布: 33 [-Wunknown-pragmas]  4 [-Wunused-variable]  4 [-Wunused-function]
          4 [-Wformat-truncation=]  4 [-Wclass-memaccess]  1 [-Wunused-but-set-variable]
          1 [-Wrange-loop-construct]  1 [-Wmaybe-uninitialized]
```

**口径并列**：target 口径 33/55 = 60%；编译单元口径（对抗复核代理独立统计）**115/175 = 65.7%** 的本项目自有编译单元从未被严格告警编过。§2:23 的头条卖点 `acsd_cpuprov_*` 六件产物**全部落在零告警档**。

即：**只把 `-Wall -Wextra` 铺开，当前树就会出 52 条告警**，其中 36 条是 D2 的直接后果。本单修掉的 4 处 `-Wclass-memaccess` 中有 3 处正在这 52 条里。

⚠️ 探针盲区已由独立复核代理补齐（其做法：按 target 取真实旗标，剥掉 `-w`、补 `-Wall -Wextra -Wpedantic -Wconversion`，逐单元重编）：

| 分组 | 单元数 | 实测告警行数 |
|---|---|---|
| 被 `-w` 静默（`SILENCED_by_-w`） | **65** | **9** |
| 完全无 `-W` 旗标（`NO_W_FLAG`） | **121**（其中 109 个成功重编） | **139** |
| 合计 | 186 | **≥ 148** |

被 `-w` 静默的那 65 个单元里已确证的 9 条，按 target 分布：`acsd_p1_drizzle` 1 条（`aio_hips_writer.cpp:239` `tile_rel_path_legacy` 未使用函数）、`acsd_p1_hips_writer` 1 条（`module_entry.cpp:982` `finalized_ok` 设置未使用）、`acsd_phase2_integrate` 4 条（`phase2_integrate.cpp:277`/`:294` `-Wmisleading-indentation`、`:1288` 未使用参数 `meta`、`:167` `nearest_rank_percentile` 未使用函数）、其余 3 条为 `aio_hips_writer.cpp:239` 在另两个 target 下的同一处重复。

⚠️ 复核代理的探针有 10 个单元因宏展开问题未编过（`<command-line>` 污染导致 `GAIA_EXPORT` / `AC_API` 展开失败，rc=1），故 148 是**下界**。真实值需更干净的探针环境才能给出。**结论方向不变：从严编译的视角看，当前树潜在告警量是三位数，不是 0。**

**D6 — 5 个 target 整 target `-w`，连带静默 65 个本项目编译单元**

实测按 `.o` 计数（排除 `third_party/`）：

| target | 本项目编译单元被静默 | vendored 编译单元 |
|---|---|---|
| `acsd_p1_drizzle` | **31** | 60 |
| `acsd_phase2_integrate` | **27** | 60 |
| `acsd_p1_hips_writer` | **7** | 60 |
| `acsd_cfitsio` | 0（合规，源清单纯第三方） | 60 |
| `acsd_orchestrator_jsv` | 0（合规，源清单纯第三方） | 6 |

`acsd_p1_drizzle` / `acsd_p1_hips_writer` / `acsd_phase2_integrate` 三者都**同时编入本项目生产源与 vendored cfitsio**，隔离粒度是整 target，于是 65 个本项目编译单元（占全树 257 个自有单元的 **25%**）从未被检查过。这违反 `CODE.md:58`「窄隔离不构成降级本项目源码告警口径的依据」。

**D7 — 全仓无 `-Werror`**

`grep -rn Werror` 在 tracked 的 CMake 面零命中。告警只被打开、不升级为错：新增告警不会让构建转红，正是 D1/D5/D6 能长期存在而不被发现的原因。

### 5.3 规范符合性缺陷

**D8 — 发行目录缺 `config` 与滤镜库（§3:29 明文要求）**

§3:29 要求「构建产物结构符合发行目录：可执行 + 动态库 + schemas + **config** + 滤镜库」。实测 §2.3 的安装树：

```
$ [ -d /tmp/acsd-t10/install/config ] && echo YES || echo NO
NO
$ find /tmp/acsd-t10/install -name 'filters.json' | wc -l
0
```

`eng/cmake/install_layout.cmake` 全文零 config 目标、零滤镜库规则。两份滤镜库（`eng/packaging/config/filters.json` 241 KB、`lib/algorithms/photometry/data/response_curves/filters.json` 183 KB）只存在于源码树，不随安装分发。运行期靠用户配置显式传路径，缺则 fail-closed（`lib/algorithms/photometry/cpp/src/frame_photometry_fit.cpp:87-91`）。**安装后开箱即用不可行。**

**D9 — 构建图内存在第二个可执行（§2:22「不设子入口」）**

`orchestrator_legacy_cli`（`lib/infrastructure/pipeline/orchestrator/cpp/CMakeLists.txt:100`）由 `CMakeLists.txt:529 add_subdirectory` 拉进产品构建图并被编译（§2.2 实测存在）。它确实未被 `install()`，发行目录干净，但 `cmake --build` 仍产出第二个 ELF。

**D10 — 三核数学等价性无可复跑的判据（§2:24）**

§2:24 要求「指令集核与基线数学等价」。仓内的承诺在文档层（`docs/engineering/resources/cpu/ISA_VARIANTS.md:53-54`、`:146-148`；`docs/ACSD_DESIGN.md:445-451`），但文档指名的判据程序在当前树中不存在：`eng/tests/backend/test_abi_kernels.py`、`eng/tests/backend/test_cpuprov_isa_variants.py`、`eng/tests/backend/test_cpuprov_manifest.py`、`eng/tests/cpu/avx2/run_provider_avx2_checks.py`、`eng/tests/backend/kernel_bench_main.cpp`（`git ls-files eng/tests` 只有 conformance/e2e/integration/module/synthetic/unit/validation 七类，无 backend、无 cpu）。配套质量门只剩字节码：`eng/tools/quality/check_isa_same_source.py`、`check_variant_isa_disasm.py`、`check_manifest_isa_artifact.py` 均只有 `.pyc`。**等价性目前不可复跑验证。**

**D11 — 运行期选出的 kernel 表未接入生产计算路径**

`acsd_backend_get_api_v1` 选核后填充的 `api` 未被 `run_pipeline` 接收；`backend_sel` 只打一行 stderr。kernel 表仅在 benchmark 与 doctor 自检路径被调用。调度器与模块适配层对 backend_host 零引用。**后果：normalize/mosaic/export 的实际计算全部走静态编进可执行的基线码，AVX2/AVX-512 动态库在生产路径上不会被加载。** §2:23「调度器运行时选取」在构建产物层面成立，在执行路径层面不成立。

**D12 — 机器画像落点与 §1:10 不符**

§1:10 要求「benchmark 生成机器画像写入 **config 目录**」。实现写入可执行同级的 `cpu_profile.json`（`lib/infrastructure/cli/commands.cpp:194-197`、`:2666-2690`），失败降级到 `XDG_DATA_HOME`/`LOCALAPPDATA`（`:202-204`）。因 D8 安装树无 config 目录，此偏差是 D8 的次生结果。

**D13 — 两处门禁失效、注释仍指向已删文件**

`CMakeLists.txt:77` 引用 `eng/tools/check_warning_suppression.py`（已删除，仅余 `__pycache__/*.pyc`）；`eng/cmake/cfitsio_platform.cmake:120` 与 `CMakeLists.txt:571` 引用 `.github/workflows/ci-windows.yml`（`.github` 目录不存在）。`CODE.md:95-96` 明确禁止把未在位的检查项记为门已生效。同型：`eng/tools/quality/check_budget_single_source.py` 也只剩 `.pyc`，其声称守护的「配置唯一源」因此无机器抓手。

---

## 6 推翻的既有判定

| # | 被推翻的判定 | 出处 | 实测反证 |
|---|---|---|---|
| 1 | 「遗留 aio/hips/drizzle/calibration 已 **target-local 隔离**（只加 `-fopenmp`）」 | `CMakeLists.txt:1343-1345` | 这四个 target 连 `-Wall` 都没有（§5.2 D5 表）；实测 `acsd_p1_drizzle` 的旗标串是 `-O3 -DNDEBUG -std=gnu++17 -w -fopenmp` —— **`-w` 真实存在且注释里根本没提**。其 target-local 处理只有 `acsd_openmp_link_if_unix()`（OpenMP 接线），**不是**告警隔离。注释把「整 target 降级」描述成了「只加 `-fopenmp`」。 |
| 2 | 「自有生产: core/common/phase2/phase3/cpu/**CLI** 全量 `-Wall -Wextra -Wpedantic -Wconversion`」 | `CMakeLists.txt:1346` | CLI 侧三个 target 全部无旗标：`acsd_cli_runtime`、`acsd_module_adapters`（D1 顺序 bug）、`acsd_cli_process`。 |
| 3 | 「少一个 target 就等于悄悄放松了 `-Wconversion` 的覆盖面」被当作**已生效的防线** | `CMakeLists.txt:1352-1353` | 这条防线自身因声明顺序漏掉 2 个 target（D1）。注释描述的风险正是它自己造成的。 |
| 4 | `CODE.md:112`「警告策略：`-Wall -Wextra` first-party 无新增警告」在 Linux 控制节点**成立** | `docs/engineering/standards/CODE.md:112` | 55 个 target 中 28 个连 `-W*` 都没有、5 个被 `-w` 静默；`CODE.md` 未把 `-Werror` 列为 MUST，但配合 33/55 的零覆盖面，「无新增警告」在 Linux 侧**无任何机器抓手**。 |
| 5 | §3:29「构建产物结构符合发行目录：可执行 + 动态库 + schemas + config + 滤镜库」**已满足** | `standards/07_DYNAMIC_RUNTIME_AND_BUILD.md:29` | 实测安装树无 `config/` 目录、无 `filters.json`（D8）。 |
| 6 | §2:23「计算核按指令集分别编译动态库，调度器运行时选取」**已端到端满足** | `standards/07_DYNAMIC_RUNTIME_AND_BUILD.md:23` | 构建产物层面满足（三族核 + 真 ymm/zmm 差异化）；执行路径层面不满足（D11：kernel 表未接入 pipeline）。 |
| 7 | `BUILD_NODES.md:28`「全仓无全域告警屏蔽」⇒ 告警口径是干净的 | `docs/engineering/build/BUILD_NODES.md:28` | 全局屏蔽确实没有，但 5 个 target 整 target `-w`（D6），其中 3 个连带静默 65 个本项目编译单元 —— 属「目标级降级」而非「全域屏蔽」，措辞成立而实质有洞。 |

### 6.1 独立复核的结论与否决

本单派发 4 名只读审计子代理 + 2 名复核子代理。**被复核否决的判定**：

| 审计子代理提出的判定 | 复核结果 | 否决理由 |
|---|---|---|
| 「`acsd_cpu_baseline.so` 是空壳，0 个动态符号、`.text` 为 0」 | **否决（我自己的中间结论）** | 起因是对该文件用错路径（它在构建树**根目录**，不在 `providers/`），`objdump`/`nm` 因 `2>/dev/null` 静默失败而返回 0。重测：`readelf -S` 显示 `.text` = 0x3983（14723 字节），`nm -D` 有 36 个符号、导出 `acsd_backend_get_api_v1` / `acsd_abi_boundary_probe` / `acsd_baseline_last_workers_used`。**该库内容完整，不是缺陷。** |
| 「`acsd_cfitsio` 含 3 个本项目自有源」 | **否决（我自己的中间结论）** | 那 3 个「非 third_party」条目是 CMake 自动生成的 `DependInfo.c` / `cmake_clean.c` / `cmake_clean_target.c`，不是项目源。实测 `find . -path '*acsd_cfitsio.dir*' -name '*.o' ! -path '*third_party*' | wc -l` = **0**。该 target 的 `-w` **合规**。 |
| 「探针日志里的 33 条 `-Wunknown-pragmas` 是并行 `make` 输出交错导致的误归因，不是真问题」 | **部分否决** | 归因确实不可靠（并行交错，已改用 `-j1` 串行重编重建归因），但**问题本身是真的**：串行归因把其中 36 条明确落到 `acsd_p1_ipv` 名下，且实测该 target 旗标无 `-fopenmp`（D2）。**这是真实功能缺陷，不是探针假象。** |
| 「`gaia_client.c` 的 `#pragma omp` 被忽略是本探针强加 `-Wall` 造成的假象（因为 `acsd_catalog_gaia` 带 `-fopenmp`）」 | **否决** | 该论证只覆盖 `acsd_catalog_gaia` 一个消费方。同一文件还被 `acsd_p1_ipv`（`CMakeLists.txt:1155`）编译，那里的旗标无 `-fopenmp`，实测 23–24 条 `-Wunknown-pragmas`；做「去掉 `-fopenmp` 再加 `-Wall`」的对照实验得 23 条，方向一致。**D2 成立。** |

**未找到推翻点的部分**：本单的零告警结论、4 处改动清单、产物形态实测数字，均未被任何复核代理推翻，并已由独立代理用自己的构建目录重新复现（详见 §3.4）。本单推翻的**是既有判定**共 7 条（见上表），不是自己的结论。

---

### 6.2 对抗复核代理提出的更正（本单已接受并改正）

| 复核代理的指控 | 本单裁定 | 处置 |
|---|---|---|
| 「`/tmp/acsd-t10/pristine` 不是裸 HEAD，是 HEAD + 3 个未提交修改，所以『10e84432 零警告』这句是假的」 | **成立**。措辞确实可能被误读 | §1.3 的命令块已补出「步骤 2：施加本单 4 处修复」并加警示；裸 HEAD 的对照数据（恰好 1 条告警）单列在 §3.1。**准确表述**：`*10e84432 + 4 处修复* 在既有旗标配置下零告警；裸 `10e84432` 有 1 条告警。 |
| 「`-w` 静默的 49 个 TU 含 P3 drizzle 全部 11 个计算面（drizzle_engine / drizzle_science / reverse_drizzle / spherical_overlap(_science) / hp_drizzle_api(_hips_api) / poly_clip / snr_evaluator / fits_reader / astro_sphere_sink / wcs_sip），即创新点 P3 守恒映射算子本体」 | **成立且比本单原述更严重** | 并入 D6：`-w` 的受害面不只是「65 个编译单元」这个数，而是**恰好罩住 P3 的计算面**。 |
| 「`CMakeLists.txt:1344` 注释说遗留目标『只加 `-fopenmp』，实测 `acsd_p1_drizzle` 是 `… -w -fopenmp`，`-w` 未在注释里声明」 | **成立** | 并入 §6 推翻表第 1 条：该注释不仅把「无旗标」说成「已隔离」，连 target 上真实存在的 `-w` 都没写出来。 |
| 「`-Wall -Wextra` 探针的 48/52 条是**下界**，因为 target 级 `-w` 在旗标串末位覆盖 `CMAKE_CXX_FLAGS`」 | **成立**。本单原本已自行标注该盲区 | §5.2 D5 保留盲区声明，并加注：`-w` 受害那批 TU 的告警数**至今未被量化**，须临时改 `eng/cmake/cfitsio_platform.cmake:75` 才能测，属仓库修改，另派单。 |
| 「`-DCMAKE_CXX_FLAGS` 覆盖不了 `-w`，所以那 49 个 TU 的告警数测不出来」 | **成立** | 同上，登记为未解决项。 |
| 「构建入口用 `-G`/`-B` 违反 `BUILD_NODES.md` §5，证据可采性受损」 | **成立，但本单保留裁定** | §7.3 已给出自我裁定：以派单的「不得留下会被检索到的构建目录/陈旧产物」禁令优先。两条规范在此直接对撞，**提请前台裁决**。 |
| 「安装树只有 1 个 ELF 可执行，`orchestrator_legacy_cli` 不在 `install_layout.cmake` 的 14 条规则里 ⇒ §2:22『唯一可执行』在发行形态上成立」 | **成立**，与本单一致 | §2.2/§2.3 保持原述并补注「发行形态成立、构建形态不成立」。 |
| 「config 目录与滤镜库：`§2` 原文未要求 ⇒ 该攻击不成立」 | **部分成立**：要求确实不在 §2，而在 **§3:29**；且任务书 T10 正文同样明列 | D8 保留，引用行号已由「§2:29」改正为「§3:29」，并补注 T10 任务书亦要求。 |
| 「`route_kernel_from_profile`（`cpu_routing.cpp:155` / `.h:145`）全仓零调用点，是死代码」 | **成立** | 并入 D11（kernel 选路链断裂）作为旁证。 |
| 「干净 `10e84432` 克隆构建 = 恰好 1 条警告，位置 `orchestrator.cpp:1578`」 | **与本单完全一致**（本单的对照构建走的是 `git archive` + 复制 `.git`，其走的是 `git clone`） | 两条独立路径互证，§3.1 结论加固。 |
| 「115/175 = 65.7% 的自有 TU 从未被严格告警检查」 | **口径不同，两个数都保留** | 本单用 target 口径（33/55 = 60%），复核用 TU 口径（115/175 = 65.7%）。§5.2 D5 两个口径并列，避免单一口径被质疑。 |

## 7 自证段

### 7.1 每个数字都能指回可复跑命令

| 数字 | 出处 |
|---|---|
| 503 编译单元 | `find /tmp/acsd-t10/pristine-build -name '*.o' \| wc -l`，并与 `grep -cE 'Building (C\|CXX) object' pristine-build.log` 互证 |
| 55 个 target | `find . -name flags.make -path '*/CMakeFiles/*.dir/*' \| wc -l` |
| 33 个 target 无有效检查 | 同上，逐个读 `C_FLAGS`/`CXX_FLAGS` 后分档（§5.2 表） |
| 65 个被 `-w` 静默的自有单元 | `find . -path '*<t>.dir*' -name '*.o' ! -path '*third_party*' \| wc -l` 逐 target 求和 |
| 52 条探针告警 | `/tmp/acsd-t10/probe-recheck.log`（`-j1` 串行重编，11 个源文件） |
| 1 个可执行 + 14 个 .so | `cmake --install` 后 `find /tmp/acsd-t10/install` |
| ymm/zmm 计数 | `objdump -d <so> \| grep -c '%ymm'` / `'%zmm'` |
| 24 处 `#pragma omp` | `grep -c '#pragma omp' lib/infrastructure/gaia_xpsd_client/src/gaia_client.c` |

### 7.2 本单的自限

- 没跑测试（测试集是另一条车道）。
- 没跑端到端（T11 的事）。
- 没做 Windows 构建（同一 T10 的另一半，由并发车道负责）。
- 没提交任何 git 变更（本单无 git 写权限，交付物由前台统一提交）。
- 改了 3 个源文件（共 4 处代码 + 1 段注释），**没有**碰任何构建脚本 —— 因为一旦动旗标就会引出 D1/D5/D6 的大面积告警，那是另一个规模的任务，且属于「收紧检查」而非「消除告警」，不应在单子里偷做。

### 7.3 一处规范冲突的自我裁定（供前台裁决）

`docs/engineering/build/BUILD_NODES.md` §5 规定「构建入口由 preset 合同给定……生成器、二进制目录与配置类型都取自 `CMakePresets.json`，**不得在命令行另指定**」。本单用的是 `cmake -S ... -B ... -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release`，按字面**违反**该条。

本单仍然这么做的理由：preset 的 `binaryDir` 是 `${sourceDir}/build/linux-control`（`CMakePresets.json:72`），而该目录存着 2026-09-29 的陈旧构建产物；派单又明确要求「不得留下会被检索到的构建目录」「不要把陈旧构建产物伪造出某目录存在的假象」。两个要求在此处直接对撞。

裁定：**以派单的具体禁令优先**，用命令行指定仓库外二进制目录。已核对 `/build/` 确实被 `.gitignore:20` 忽略，因此 preset 落点本身也合规 —— 真正的问题只是**陈旧产物**，不是位置。若前台认为必须走 preset，建议先清空 `build/linux-control` 再 `cmake --preset linux-control && cmake --build --preset linux`，本单的旗标面与告警面结论可直接迁移（已验证 preset 与命令行两条路的 `CMAKE_BUILD_TYPE=Release` + Unix Makefiles 等价）。

### 7.4 遗留与下一步建议（按优先级）

1. **D1**：`acsd_module_adapters` / `acsd_cli_runtime` 的旗标顺序 bug —— 单行级修复，收益最大。
2. **D2**：`acsd_p1_ipv` 接 `-fopenmp` —— 涉及行为变更（并行化生效），须评估数值影响后再动。
3. **D5/D6**：33 个 target 的告警覆盖面 —— 按「先产品件、后内部件」分批，每批配 `-Werror` 防回退。
4. **D7**：给 CI 加 `-Werror`（T12 的活），让上述修复不被静默回退。
5. **D8**：补 `config/` 与滤镜库的 install 规则，或修订 §3:29。
6. **D10/D11**：三核等价性判据程序与 kernel 表接线 —— 决定 §2:23/§2:24 是形式满足还是实质满足。

---

## 附：证据文件清单

| 路径 | 内容 |
|---|---|
| `/tmp/acsd-t10/pristine-build.log` | 权威构建日志（616 行，零告警，`PRISTINE_EXIT=0`） |
| `/tmp/acsd-t10/pristine-configure.log` | 配置日志（工具链识别、依赖探测、指纹面 925 个源文件） |
| `/tmp/acsd-t10/build-make.log` | **修复前**对照日志（恰好 1 条 `-Wclass-memaccess`） |
| `/tmp/acsd-t10/probe-build.log`、`probe-recheck.log` | 旗标覆盖面探针（52 条告警，串行归因） |
| `/tmp/acsd-t10/pristine/` | HEAD 纯净快照 + 本单 4 处修改 |
| `/tmp/acsd-t10/pristine-build/` | 构建树（503 `.o`、14 `.so`、2 ELF） |
| `/tmp/acsd-t10/install/` | `cmake --install` 落地树（发行目录实测） |
| `/tmp/acsd-t10/clang-build.log` | Clang 18.1.8 交叉验证日志（**失败**：6 个 `capturing a structured binding is not yet supported in OpenMP` error + 160 条警告） |
| `/tmp/acsd-t10/build-make.log` | 裸 HEAD 对照日志（1 条告警），与 §3.1 互证 |
| `/tmp/t10-verify/`、`/tmp/t10-adv/` | 复核代理的独立构建目录与完整报告（`/tmp/t10-adv/REPORT.md`） |