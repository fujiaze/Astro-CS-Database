# G08-09 整改 · Windows 侧全量编译 + 警告收敛

**状态：未完成 —— Windows 构建一次都没有跑起来。**
本单要求「Windows 全量编译通过 + 逐条修正警告至零」。截至本件交付，
**构建目标数 = 0、编译耗时 = 0、真实 Windows 告警条数 = 未采集（不是 0 条，是没采到）**。
原因是验证主机 FATDUCK 在本单开工前约 2 分钟即掉线，且现行在线窗已过（下文 §1 附原始输出）。
按任务书《G08-09》边界条款「平台受限无法验证的项如实记录，不伪造绿灯」，
本件**不登记任何绿灯**；把「没跑」写成「跑过了」是本单明令禁止的。

**基线**：本件开工时仓库 HEAD = `7516d162`；交付时前台已推进到 `3259a95`。
`git diff --stat 7516d162 HEAD` = **1 个文件、57 行、纯 Markdown**，未触及任何
CMake / C / C++ 源文件 ⇒ 本件全部代码层结论对两个 HEAD 同样成立。
（`7516d162` 是 `3259a95` 的祖先，非并列。）

---

## 1. 构建是否成功

### 1.1 结论

**没有。构建从未开始。** 不是「编不过」，是**根本没连上机器**。

### 1.2 FATDUCK 掉线的原始证据

任务书给的地址 `10.0.26220` 是**非法 IPv4**。实测：

```
$ ssh -o BatchMode=yes 10.0.26220 hostname
ssh: connect to host 10.0.102.108 port 22: Connection timed out
EXIT=255
```

`glibc inet_aton` 把三段式 `10.0.26220` 解析为 `10.0.(26220>>8).(26220&255)`
= `10.0.102.108`。**照抄任务书的地址会连到一台无关主机上，并把失败误归因为「通道不存在」。**

真实地址取自已从索引移除的接入说明（`git show 2f20a99d:FATDUCK_ACCESS.md`）：**`100.104.10.71`**（Tailscale）。

连真实地址，实测：

```
$ tailscale status --json | 解析 Fatduck
HostName = Fatduck
Online = False
LastSeen = 2026-10-03T15:31:29.1Z        # UTC → 北京时间 23:31:29
CurAddr =                                # 空
Relay = nue
LastHandshakeTime = None

$ tailscale ping 100.104.10.71
ping "100.104.10.71" timed out      # ×5

$ ssh -o BatchMode=yes -o ConnectTimeout=10 fujia@100.104.10.71 hostname
ssh: connect to host 100.104.10.71 port 22: Connection timed out
EXIT=255
```

### 1.3 时间线（这台机器是**在线窗刚关就掉的**）

| 时刻（CST） | 事件 | 证据 |
|---|---|---|
| 23:30:19 | 前台提交 `3259a95`「更正：双平台编译不是平台受限，是从未做过」，文中记录工具链实测可用 | `git log -1 --format=%ci` |
| **23:31:29** | **FATDUCK 最后一次在线** | `tailscale LastSeen` |
| 23:33 | 本单开工 | 本会话时间戳 |
| 23:47 → 00:04 | 轮询器连续 14 次探测全部超时 | `/tmp/g08-09/fatduck_wait.log` |

**掉线发生在前台更正提交之后 70 秒**，且正好越过接入说明写明的在线窗上界
「每日 07:00（最早 06:30）～ 23:30 在线的确定性高」。

### 1.4 「当下离线」≠「不可行」

按 `FATDUCK_ACCESS.md` 的「Fatduck 离线不中止目标」条款，本单**没有**把这次失败写成永久结论：

- 已启动后台轮询器 `/tmp/g08-09/poll_fatduck.sh`（每 120s 探测，日志
  `/tmp/g08-09/fatduck_wait.log`，SSH 一旦成功即置 `/tmp/g08-09/FATDUCK_UP`）；
- 已写好可直接执行的构建脚本 `/tmp/g08-09/run_on_fatduck.sh`（`bash -n` 通过），
  窗口重开后无需再分析即可跑完整条链路；
- 下一次在线窗最早 **06:30**。

### 1.5 「Windows 侧从未构建过」是错的 —— 存在一次真实 MSVC 全量构建

前台更正件（`run/GOVERN-08/审核包-R2/前台更正-双平台编译非平台受限.md`）称
「Windows 侧工具链可用，**只是从未构建过**」。**这句不准确。**
仓内存在 **2026-09-29 FINAL-07 的真实 MSVC 冷全量构建报告**：

`run/FINAL-07/审核包/工程/Windows腿构建与警告面报告.md`

原文摘录（`:1-4`、`:24`）：

```
> 本报告只做**构建与静态面**：同步 → 冷配置 → 冷全量构建 → 告警/错误普查 → 全量 ctest。
> **不跑端到端**。Windows 验证树上**零提交**。
…
| 绑定 SHA | `13d019fd074add93b696a732f35469bb23d687f6`（= `origin/main`） |
| 验证树 | `/f/Astro dev/Astro CS Normalization Database`（Git Bash 语径） |
```

实测结果（`:5-9`）：

```
1. **冷配置 rc=0**（两棵独立构建树）；**冷全量构建 rc=1**（两棵都是），**122 error / 1274 warning**。
2. **构建只覆盖了 52.4% 的登记目标**（338 个 `.vcxproj` 中只尝试了 177 个，其中 145 成功、
   32 失败，**161 个因 MSBuild 遇错停止派发而从未被尝试**）。
3. 之前几轮「Windows 侧只剩少量告警」的印象**不成立**，原因是**构建工具遇错即停止派发后续工程**：
   日志短不是问题少，而是**没走到后面**。
```

⇒ **结论要改口**：不是「从未构建过」，而是**「构建过一次，失败了（122 error），且只覆盖了 52.4% 的目标」**。
这个区别很重要——它意味着**历史告警面已知**，但**从未有一次绿色的 Windows 全量构建**。

**该基线对当前 HEAD 不可直接沿用**（实测漂移）：

```
$ git rev-list --count 13d019fd..HEAD   → 296 个提交
$ git merge-base --is-ancestor 13d019fd HEAD → YES（祖先）
$ git diff --stat 13d019fd HEAD -- CMakeLists.txt CMakePresets.json 'lib/**' 'eng/**'
  2038 files changed, 16089 insertions(+), 385594 deletions(-)
```

296 个提交、2038 个文件、净删 37 万行 ⇒ **1274/122 只能当「本仓历史会产出哪些告警族」的线索，
不能当当前 HEAD 的预测值**。且该报告用的是**改名前**的 `astrocs_*` target 名。

历史报告里**对本单仍有价值**的告警族（`:275-290` 原文摘录）：

| 编号 | 条数 | 文件数 | 含义 |
|---|---|---|---|
| `C4130` | **1009** | 16 | 字符串常量地址的逻辑操作（报告 §6.2 归纳为**一件事**：10 个测试框架头里同一段宏的展开） |
| `C4127` | 86 | 14 | 条件表达式是常量 |
| `C4244` | 57 | 3 | 隐式窄化 |
| `C4189` | 42 | 9 | 局部变量已初始化但不引用 |
| `C4456` | 30 | 5 | 局部声明隐藏上一个局部声明 |
| `C4100` | 29 | 8 | 未引用的形参 |
| `C4457` | 5 | 2 | 声明隐藏了函数参数 |
| `C4477` | 3 | 3 | printf 格式串与实参不匹配 |
| `C4505` | 2 | 2 | 未引用的内部链接函数被删除 |
| **`C4005`** | **2** | 1 | **`NOMINMAX` 宏重定义（`sampler.cpp`）** ← 本件 §3.2 W3 的证据 |
| `C4267` | 2 | 1 | `size_t`→`int`（`zuncompress.c`，cfitsio 第三方） |
| `D9002` | 1 | 1 | 忽略未知选项 `-fopenmp`（`astrocs_p1_drizzle`） |
| `LNK4006` | 1 | 1 | 重复符号 `try_append_cuda_bridge_executors` |

子代理另行核实：`C4130` 那个测试宏写法**在当前树已不存在**（`grep 'faultname) != nullptr'`
在跟踪文件里只命中 `run/` 下的叙述文字），cfitsio 重复编译与 `-fopenmp` 漏旗**也已修复**
（`ACSD_CFITSIO_SOURCES` 现只在 `CMakeLists.txt:533` 一处使用）。
⇒ **不可按这份旧表估工作量**，但 C4005 那一行是本件 W3 的直接佐证。

### 1.6 同步口径（若直接照任务书做会踩的两个坑）

**(a) 只 scp 源码必然配置失败。** `CMakeLists.txt:80-90` 在**配置期** fail-closed：

```cmake
execute_process(COMMAND git rev-parse HEAD
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  OUTPUT_VARIABLE ACSD_GIT_COMMIT ...)
if(NOT ACSD_GIT_RC EQUAL 0 OR ACSD_GIT_COMMIT STREQUAL "")
  message(FATAL_ERROR
    "GIT_UNAVAILABLE: git rev-parse HEAD 失败（rc=${ACSD_GIT_RC}; ${ACSD_GIT_ERR}）"
    " —— 版本串的 +g<sha> 分量只能来自 git HEAD，退化为全零 SHA 会让版本门假绿。…")
```

非 git 工作树 ⇒ 直接 `FATAL_ERROR`。`CMakeLists.txt:115-124` 对 `git ls-files` 同样再判一次。
⇒ **必须 `git clone`（远端已有仓库），或连 `.git` 一起传。**

**(b) 「不要同步 run/」要分清 tracked 与 untracked。** `.gitignore` 放行了
`run/GOVERN-08/`（治理交付物本身入库），实测：

```
$ git ls-files | wc -l                    → 2986 个跟踪文件，合计 13 MB
$ git ls-files | awk -F/ '{print $1}' | sort | uniq -c | sort -rn
   1068 lib/    724 实验/    369 testdata/   304 eng/   291 run/   187 docs/ …
$ git count-objects -vH
   size-pack: 243.47 MiB
```

⇒ 被忽略、**不该同步**的是 `run/` 下未跟踪的历史运行产物（80+ GB）；
`run/GOVERN-08/` 的 291 个条目是**跟踪态**、随 `git clone` 自然带过去，
总量仅 13 MB。**用 `git clone` 就自动满足「不同步 run/ 的巨量副本」这条要求。**

---

## 2. 工具链口径冲突（构建前必须裁决，否则会用错方式冒充正式构建）

任务书给的工具链与仓内冻结合同**不一致**：

| 项 | 冻结合同（`CMakePresets.json` / `preset-contract.json`） | 任务书给的 FATDUCK 实况 |
|---|---|---|
| generator | `Visual Studio 17 2022` | （要求用 Ninja） |
| toolset | `v143,host=x64`，不固定 build 号 | MSVC **14.50.35717** |
| VS 实例 | VS 17 2022 | `Visual Studio\18\BuildTools`（VS **18**） |
| cmake | pin `3.31.12` | **4.1.2-msvc8** |

`eng/packaging/schemas/preset-contract.json:35-38` **明文把 `"Visual Studio 18"` 列入 `forbidden.vs2026`**，
`:43-46` 把 `Ninja` / `ninja` 列入 `forbidden.ninja_generator`。

**但这些禁令当前没有机器执行器**：`eng/cmake/toolchain/verify_toolchain.py`（364 行）
与 `.github/workflows/` 整个目录都在 `e5f589a6`「G08-01 物理删除旧门禁与 CI（344 件 / -136676 行）」
中被删。`CMakePresets.json:30` 的「Never edits this preset to another generator」
位于 JSON `description` 字段，**是注释不是约束**。

⇒ **直接 `cmake -G Ninja` 不会让仓内任何东西判红**，但它违反明文合同。
本单**不擅自用 Ninja 冒充正式构建**；机器可用后按 `win-msvc-17.14.39-x64`
（VS17 生成器）走，若该主机只装了 VS18 则如实登记为**工具链漂移**交负责人裁决。

**前台已就此单独裁决**（`45c9b40d`「裁决 Windows 工具链口径：按冻结合同用 VS17，
不接受前台给的 Ninja/VS18」，落在 `run/GOVERN-08/审核包-R2/前台裁决-Windows工具链口径.md`）：
一律按 preset 走 VS 17 2022；**不得用 Ninja、不得用 VS18**；
若**只有** VS18 ⇒ 记「工具链漂移」并写清生成器/工具集/CMake/SDK 四项差异，
交付构建记为「**因工具链漂移未完成**」，**不得把 VS18 产物当「双平台全量编译通过」交付**，
**不得为迁就本机改 preset 或改合同禁令**。

> ⚠️ **对下一棒最重要的一条情报**（同一裁决 `:20-21`）：
> 前台先前探测时看到 `…\Microsoft Visual Studio\` 目录下
> **「18」与「2022」两个条目同时存在**，**VS2022 很可能就在 FATDUCK 上**。
> ⇒ **机器恢复后第一件事请先跑 `vswhere -all -products * -format value -property displayName,installationVersion`**
> （我的脚本 `/tmp/g08-09/run_on_fatduck.sh` 第 1 步已包含）。
> **若 VS2022 确实在，四项版本差自然消失，本单可以直接走正式合同路径，不必记漂移。**

> **重要澄清（推翻一种误判）**：上述执行器缺位**不是治理缺陷**。
> 工作包 `standards/08_编译与CI重做规范.md` §2 明确要求「治理开始时先物理删除旧门禁与 CI」，
> §4 规定**在双平台编译（本单 G08-09）之后**才重建门禁。
> ⇒ 本单执行期间这些约束**只有合同文本、没有机器执行**，是**计划内的时序**。
> 子代理最初把它报成「治理缺陷」，经复核后推翻，特此记录以免误传。
> 副作用是：**本单若靠「关警告」达成零告警，不会有任何门禁拦下**，只能靠人守住。

---

## 3. 警告清单

### 3.1 真实 Windows 告警条数：**未采集**

`cl` 一次都没有被调用过。**本节不给出「N 条警告」的总数**——那是编造。
下列条目全部是**静态取证得到的、带 `file:line` 与原文的候选项**，
按「一旦机器可用即可验证」排序。**每条都标注了确定度，未实测的一律标 UNVERIFIED。**

### 3.2 已确证的第一方警告级缺陷（我在本机复核过原文）

#### ⚠️ 头号发现：约 48 个第一方 TU **在两个平台上都没有被告警检查**

这比任何一条具体告警都重要，因为它**动摇"零告警"这个验收口径本身**。

**机制**：`eng/cmake/cfitsio_platform.cmake:63-77` 的 `acsd_cfitsio_isolate_warnings()` 做两件事：
设 `ACSD_WARNINGS_OFF=1`（`CMakeLists.txt:53-54` 据此把 `/W4` 撤掉）
**并且**在 `:73` 加 `target_compile_options(${tgt} PRIVATE /W0)`。
`/W0` 是**整目标**告警档位 ⇒ **它静音的是该目标里每一个 TU，不只是 vendored cfitsio 的那些。**

三个目标把第一方源与 vendored cfitsio **混在同一个 `add_library`** 里，然后整体 `/W0`：

| 目标 | 第一方源 | cfitsio 追加 | `/W0` 施加处 |
|---|---|---|---|
| `acsd_phase2_integrate` | `:26-55`（约 30 个 .cpp） | `:56` | `:101` |
| `acsd_p1_drizzle` | `:40-61`（adapter + prod + 21 个 aio/shared） | `:72` | `:85` |
| `acsd_p1_hips_writer` | `:25-49`（7 个） | `:59` | `:73` |

去重后约 **48 个第一方 `.cpp`** 落在这个范围里，含 `lib/algorithms/coverage/src/{sampler,coverage,rejection,identifiability,upm}.cpp`、
`lib/algorithms/drizzle/healpix_drizzle/{drizzle_science,spherical_overlap,spherical_overlap_science}.cpp`、
以及 `lib/infrastructure/aio/src/` 的大部分。

**为什么严重 —— 这批代码两个平台都没查**：
- **Windows**：`/W0` ⇒ 静默。
- **Linux**：这三个目标**不在**严格旗标列表里。`CMakeLists.txt:1350-1357` 只列了
  `acsd_core acsd_common acsd_phase2 acsd_phase3_session acsd_cpu acsd_phase1_{wcs,phot,noise,stars,session}
   acsd_phase2_session acsd_module_adapters acsd_cli_runtime acsd_cpu_avx2 acsd_cpu_avx512
   acsd_cpu_avx2_kernels acsd_cpu_avx512_kernels`（另加 `:1362` 的 `acsd` 本身）。
  `acsd_phase2_integrate` / `acsd_p1_drizzle` / `acsd_p1_hips_writer` **均不在其中**。

⇒ **即使 MSVC `/W4` 全绿，也证明不了这 48 个 TU 是干净的——门根本照不到它们。**
（具体推论：下面 W3 的 `sampler.cpp` C4005 会在 `sampler.cpp` 确实处于 `/W4` 的
`acsd_coverage` 与 `acsd_phase2`(`CMakeLists.txt:686`) 里报出来，
但在 `acsd_phase2_integrate` 里看不见。）

**并且它违反本仓自己的标准**。`docs/engineering/CODE_STANDARD.md:48` 逐字：

> 禁止 `/w`、`/W0` 一类整目标或全局降级来掩盖本项目源码的告警。

当前 CMake 编排做的正是这件事——作为"隔离 cfitsio"的副作用，覆盖了 48 个第一方文件。

**建议修法方向（本轮未实施）**：不要把 `/W0` 放在混合目标上。两个选项：
- **(a) 把 vendored cfitsio 抽成独立的 static/object 库**，只让它带 `ACSD_WARNINGS_OFF`，
  三个模块目标改为链接它而不再重复编译 cfitsio。
  **一步同时解决三件事**：修好隔离、消除约 60 个 vendored TU 的 3 次重复编译、消除 include 面漂移
  （FINAL-07 报告记录该重复编译的手工维护至少漂移过两次）。
- **(b) 若必须保留重复编译**，则去掉这三个目标上的 `/W0`，接受 vendored cfitsio 的诊断进入告警面
  ——这正是 `CODE_STANDARD.md:48` 已经要求的口径。

#### 具体告警级缺陷

| # | 位置 | 类型 | 原文 | 性质 | 状态 |
|---|---|---|---|---|---|
| W1 | `lib/infrastructure/gaia_xpsd_client/CMakeLists.txt:38-39` | **D9025**（命令行）/ 级降级 | `target_compile_options(acsd_catalog_gaia PRIVATE`<br>`    /Zc:__cplusplus /W3)` | 把**第一方**目标钉在 `/W3`，而全局 `/W4` 仍生效（`CMakeLists.txt:53-54`）⇒ `cl` 额外报一条**关于降级手段本身**的警告，正是根 `CMakeLists.txt:50-52` 自己论证要避免的形态 | 违反本单「不得关警告」纪律 |
| W2 | `lib/infrastructure/gaia_xpsd_client/src/gaia_client.c:518-522`（`_WIN32` 分支） | **C4267** + **C4018** | `MEMORYSTATUSEX ms;`<br>`ms.dwLength = sizeof(ms);`<br>`return (ms.ullAvailPhys < MEMORY_PRESSURE_THRESHOLD) ? 1 : 0;` | **本单最干净的一个例证**。`:219` `#define MEMORY_PRESSURE_THRESHOLD (4ULL * 1024 * 1024 * 1024)`（unsigned long long）；`MEMORYSTATUSEX::ullAvailPhys` 的类型是 `DWORDLONG` = **有符号** `long long`（成员名里的 `ull` 有误导性）⇒ **有符号/无符号比较 ⇒ C4018**。而 POSIX 孪生分支 `:536` 比的是 `unsigned long mem_available * 1024`（`:529` 声明）**两侧同为无符号** ⇒ **GCC `-Wsign-compare` 静默，MSVC `/W4` 报 C4018**。另 `:520` 的 `sizeof(ms)`（size_t）→ `DWORD` 是 C4267。**这两条都落在被 `/W3` 降级的同一个 target 里** | **LIKELY，高置信** |
| W2b | `lib/infrastructure/cli/monitor.h:273` | C4100 | Windows 下 `read_thread_cpu` 空体 | 公共签名形参在 Windows 分支未引用；**仓内已有现成惯用法** `atomic_publish.cpp:155-156` 的 `(void)dir;`，此处缺 | LIKELY |
| **W4** | `gaia_client.c:1814,1949,2093` + `:1737`（递归点 `:1857,1860,1863,1866`） | **C4244 ×7，CERTAIN** | `int n = node->block_size / xf->star_stride;`（`block_size` 是 `uint32_t` `:233`、`star_stride` 是 `int` `:234` ⇒ `uint32_t/int` 提升为 `unsigned int` 再窄回 `int`）；以及 `:235` 的 `uint32_t child_nw/…` 传给 `search_recursive(..., int node_idx, …)`。**注意 `:1872`/`:2018` 两处同名递归正确地取 `uint32_t node_idx` ⇒ 签名本身自相矛盾** | **CERTAIN** |
| W5 | `lib/algorithms/coverage/src/sampler.cpp:190-197` | C4505（删 8 行即解） | `static int seh_filter(...)` 在 `namespace {}` 内且**全仓零引用**（唯一定义处即 `:191`）；另它也**不是合法的 `_except` 过滤器签名** | UNCERTAIN（取决于 `[[maybe_unused]]` 是否抑制 C4505） |

> **W4 是本单置信度最高的一组**：它**不受** `x.dwSize = sizeof(x)` 那一族的"常量表达式豁免"不确定性影响
> —— 那是**运行期值**的 `uint32_t→int` 窄化，且所在文件（`gaia_client.c`）确定在 `/W4` 构建内。
> **修掉这 7 条，`sizeof` 族是否需要证明豁免就不再挡在关键路径上。**

#### ❌ 已撤回的一条误报（记录下来免得有人去"修"它）

曾报「`gaia_client.c:2439-2442` C4456 ×4」。**该结论错误**：该处是
```c
    int f;
    #pragma omp parallel for schedule(dynamic) num_threads(gaia_omp_team_size())
    for (f = 0; f < nfiles; f++) {
```
循环是 `for (f = 0; …)` **纯赋值、无重声明**，`int f` 只声明一次 ⇒ **无遮蔽、无 C4456**。
该注释描述的"把索引变量提到 pragma 之前声明"**恰恰就是为避开 C3015 而做的正确修法**。
**不要按那条改。**

#### ⚠️ 又一批零告警的静默缺陷（比告警号更要紧）

- **`lib/infrastructure/benchmark/backend_host/cpu_features.cpp:65-109` 缺 `_M_X64` 分支**
  ⇒ MSVC x64 下 ISA mask 恒为 0 ⇒ `kernel_*_axpy_safe` 一类门（`sse.cpp:39` 等）全部返回 false
  ⇒ **静默退回标量路径**。同类根因见 `acr/topology/cpu_features.cpp:33`（只测 `__GNUC__`/`__x86_64__`，
  从不测 `_M_X64`，与该文件 `:51` 自己的注释「x86-64 必有 SSE2」直接矛盾）；
  `acr/qualification/benchmark_driver.cpp:789-798` 则在 MSVC 下恒报 `"baseline"`。
- **`hardware_inspect.cpp:126-128` 的 MSVC CPUID 解码漏了 `base_family == 0x6u`**（仓内孪生实现
  `capability_detect.c:205-207` 有）⇒ 两份实现对 family-6 部件给出**不同的 `model`** ⇒ 进 CPU 指纹/profile 缓存键。
- **`host_services.cpp:31-38`**：Windows 腿 `_aligned_malloc(size, align)` 返回 `size` 字节，
  POSIX 腿把 size **向上取整**到 `align` 的倍数 ⇒ **写那个尾部在 Windows 上越界**。
- **`gaia_client.c:1333-1335`**：`GetFileSizeEx` 失败时 Windows 侧 `mmap_size` **保持未初始化垃圾值**，
  POSIX 侧 `fstat` **fail-closed**。`/W4` 不做确定性赋值分析（只有 `/analyze` 的 C6001 能查）⇒ **零告警**。
- **`dll_loader.cpp:29-33` 建立 `kModuleLibSuffix`（`.dll`/`.so`）抽象，`:273` 与 `:301` 的 Windows 预载却硬写 `.dll`** ⇒ 240 行后自我否定。
- **`avx512_backend.cpp:27-32`**：门面 TU 用基线旗标（`/arch:AVX512` 只加在 `acsd_cpu_avx512_kernels` 上）
  ⇒ MSVC 那里永不定义 `__AVX512CD__` ⇒ `#else` **恒取**，声明的 feature 集漏 `AVX512CD`，
  与该文件 `:17-20` 自己的注释「必须含 CD，否则『用了却没声明』」矛盾
  ⇒ 无 CD 的宿主能过 preflight、加载 DLL，而计算 TU 撞 `#UD`。

**两处任务提示的更正**：`LoadLibraryA`/`GetProcAddress`/`LoadLibraryExA` 是 **Win32 kernel32 API、不是 CRT**，
**C4996 不适用**；`localtime_s`/`gmtime_s` 的实参顺序在**全部 13 处都正确**
（MSVC 取 `(tm*, const time_t*)`，每个 Windows 分支都传 `(&tm, &t)`）——**不存在要修的换序隐患**。
| W3 | `lib/algorithms/coverage/src/sampler.cpp:35` | **C4005** | `#define NOMINMAX` | 与 `CMakeLists.txt:55` 的 `add_compile_definitions(NOMINMAX …)` → MSVC 的 `/DNOMINMAX` 重名。`cl` 把 `/DNOMINMAX` 当作 `#define NOMINMAX **1**`，而源码重定义为**空**替换列表 ⇒ **两者不同，触发 C4005**。⚠️ **我一度判其为良性（理由：替换列表相同）并推翻了子代理，该判断错误**，历史实测日志证明它确实发生（见 §1.6） | **LIKELY**，历史实测佐证 |

> **W3 的判定更正（我的自我推翻）**：我最初依据「C++ 标准允许相同定义的重定义」
> 判定 `sampler.cpp:35` 良性，并把它列为「推翻的既有判定」。
> 该判断**错在忽略了 `/D` 与源码 `#define` 的替换列表不同**（`/DNOMINMAX` ⇒ 值 `1`；源码 ⇒ 空）。
> FINAL-07 的真实 MSVC 日志明确记录了 `C4005 ×2，1 个文件，NOMINMAX 宏重定义（sampler.cpp）`
> （`run/FINAL-07/审核包/工程/Windows腿构建与警告面报告.md:284`）⇒ 该告警**真实存在过**。
> `sampler.cpp` 现被编入 3 个 target（`coverage/CMakeLists.txt:49`、
> `integration/phase2_integrate/CMakeLists.txt:34`、`CMakeLists.txt:686`）。
> **修法**（不加 epsilon、不改阈值、纯宏卫生）：把 `:35` 的裸 `#define` 改为 `#ifndef NOMINMAX` 包裹。
> **本轮未改**：无 Windows 产物可回归验证，且该改动应由前台统一提交。

### 3.3 违反「不得关警告」纪律的既有机制（须裁决，非我可单方面改）

| # | 位置 | 机制 | 证据与判定 |
|---|---|---|---|
| S1 | `CMakeLists.txt:53-54` | `ACSD_WARNINGS_OFF` 目标属性门控 `/W4` | 机制本身是**目标级**、非全局，设计正确。**但被过度应用到 5 个目标**：`acsd_cfitsio`、`acsd_orchestrator_jsv`（真第三方，合法）、`acsd_p1_drizzle`、`acsd_p1_hips_writer`、`acsd_phase2_integrate`（后三个**同时编译大量第一方 TU**）。⇒ **约 65 个第一方 TU 以 `/W0` 而非 `/W4` 编译**。调用点：`drizzle/CMakeLists.txt:85`、`drizzle/hips/CMakeLists.txt:73`、`integration/phase2_integrate/CMakeLists.txt:101` |
| S2 | `CMakeLists.txt:64` | `_CRT_SECURE_NO_WARNINGS` `_CRT_NONSTDC_NO_WARNINGS` | `add_compile_definitions` 是**目录级无条件**，等同全项目 `/wd4996`。子代理实测第一方受影响调用点 **332 处**（fopen 87 / strncpy 54 / getenv 52 / sscanf 40 / strerror 31 …），**仓内注释自称 206 处，数字对不上**。项目立场是「C4996 是平台口径差、同码 GCC 侧零告警」——工程上站得住，但**恰是本单纪律禁止的机制** ⇒ **需负责人明确裁决** |
| S3 | `eng/cmake/cfitsio_platform.cmake:69-76` | cfitsio 用 `/W0` 而非 `/w` | **判定为合法**：目标级、真第三方、有据（`/w` 与 MSBuild 默认 `/W1` 相撞报 D9025，故取 `/W0`） |
| S4 | `lib/algorithms/drizzle/healpix_drizzle/snr_evaluator.cpp:23-30` | `#pragma warning(push/disable:4324 4127/pop)` 包住 `#include "nanoflann.hpp"` | **判定为合法**：窄、push/pop 配对、include 点作用域、第三方触发、不降级别。但其安全网已失效：`drizzle/CMakeLists.txt:111-114` 把 nanoflann 当 `SYSTEM` include 的路径 `${CMAKE_SOURCE_DIR}/third_party/nanoflann/include` **不存在**（头文件与 .cpp 同目录）⇒ 若要彻底去掉该 pragma，须先修这条死分支 |

**分类统计**（已确证 2 条警告级 + 4 条抑制机制；**实测总数待机器可用后回填**）：
C4xxx 类 1 条（C4267）· 命令行类 1 条（D9025）· 抑制/降级机制 4 项，其中 2 项合法、2 项需裁决。

### 3.4 为零警告而无法达成的逐条理由

**本单无法给出该清单，因为零警告从未被尝试过。** 本单**不写**「某类警告修不掉」
——按纪律，**未取得「该条不改动的证据」前不得声称「无法修」**，而我手上没有任何一条 `cl` 输出。
唯一可如实登记的是**已确证的三处纪律冲突**（S1 过度应用、S2 全局降级、W1 `/W3`），
它们不是「修不掉的警告」，而是**用来说明达成零告警的既有手段本身不合规**。

### 3.5 「零告警」口径的一处结构性事实

`CMakeLists.txt:1350-1363` 的严格告警块是 `if(NOT MSVC)`；Windows 侧 `/W4` 全靠
`CMakeLists.txt:53-54` 的目录级 `add_compile_options`。同时 Linux 侧
`-Wall -Wextra -Wpedantic -Wconversion` 只加在一个**固定的 19 个 target 列表**上，
`acsd_aio` 等目标在 Linux 侧**根本没收这些旗标**。
⇒ **「Linux 零告警」与「Windows 零告警」覆盖面本就不同构**，任务书「Linux 全绿 ⇒ 这里也应全绿」的类比只在部分 target 上成立。

---

## 4. 动态库导出 / 入口 / 随产物配置核对结论

**性质：静态源码核对（产物不存在，无法做实测导出表比对）。**

| 核对项 | 结论 | 依据 |
|---|---|---|
| DLL 导出机制 | **基本成立**。`acsd_runtime` / `acsd_io` / `acsd_cpu_baseline` / `acsd_cpuprov_*` 走 `WINDOWS_EXPORT_ALL_SYMBOLS ON`（`CMakeLists.txt:264,278,395,906,1001`），CMake 自动生成 `.def` 导出全部符号；模块 DLL 走显式宏 | 原文见左 |
| 三态导出宏 | **两套并存**。`lib/include/acsd/abi/status_codes.h:56-64` 有完整三态（`ACSD_ABI_EXPORTS`→dllexport / 否则 dllimport / GCC→visibility / 否则空），规范 | — |
| **`AIO_EXPORT` 缺第三态** | ⚠️ `lib/infrastructure/aio/include/astro_image_io.h:8` 只有 `#ifdef _WIN32 → __declspec(dllexport) #else → visibility`，**无 dllimport 分支**。**当前未触发**：消费者 `orchestrator.cpp:3507,3600` 走运行期按名查找 `get_function(ModuleId::AIO, "aio_hiss_inspect")`，不产生链接期引用。但这是埋着的陷阱：将来任何 target 改为直接链接 `acsd_io` 就会踩 | 原文见左 |
| 入口 | **未发现缺陷**。主程序 `add_executable(acsd ...)`（`CMakeLists.txt:1307`）为控制台子系统，`lib/infrastructure/cli` 提供 `main`；DLL target 均未设 `WIN32` ⇒ 子系统与入口符号一致 | `CMakeLists.txt:1307,1334-1343` |
| 随产物配置 | **产品清单已声明 17 个 unit**（`eng/cmake/acsd.product.windows.json.in`）。⚠️ 其中 `acsd_runtime.dll` / `acsd_io.dll` 的 `status` 仍为 **`SKELETON`**，非 `IMPLEMENTED` —— 与 `CMakeLists.txt:241-245`「骨架，loader 接线由后续任务追加」自述一致。按 AGENTS.md §11，未验收阶段不得标 available ⇒ **如实保留 SKELETON 是对的**，但 Windows 侧**从未验证过安装树能否装出这 17 件** | manifest 原文 |
| 模块命名不一致 | ⚠️ **未定性为缺陷**。`dll_loader.cpp:54` 对 `ModuleId::AIO` 取名 `"astro_image_io" + kModuleLibSuffix`（Windows 下 `.dll`），而安装树与清单装的是 **`acsd_io.dll`**（`CMakeLists.txt:290` `OUTPUT_NAME "acsd_io"`、`install_layout.cmake:31`）。但这是**双平台同名问题**（Linux 同样找 `libastro_image_io.so`），非 Windows 独有；且主 `acsd` 可执行文件的链接面（`CMakeLists.txt:1334-1343`）**不含** `acsd_infra_orchestrator` ⇒ 该 loader 属 `orchestrator_legacy_cli` 旁路，**不在产品图内**。**需实跑确认，不在此断言缺陷** |

### 4.1 导出面：15 个 SHARED 目标全部有声明机制 —— **PASS（设计层面）**

三种机制并存，无一目标"无标注因而什么都不导出"：

| 组 | 机制 | 目标与出处 |
|---|---|---|
| A | `WINDOWS_EXPORT_ALL_SYMBOLS`（均在 `if(MSVC)` 内） | `acsd_runtime`(`CMakeLists.txt:260,264`)、`acsd_io`(`:273,278`)、`acsd_cpu_baseline`(`:387,395`)、`acsd_cpu_avx2/avx512`(`:835,:871` + `:903-906`)、`acsd_cpuprov_{baseline,avx2,avx512}`(`:989,:1000-1002`) |
| B | 纯 `__declspec(dllexport)` 宏，无 `.def` | `acsd_noop`(`eng/tests/conformance/noop/CMakeLists.txt:9,22-23`)、`acsd_catalog_gaia`(`gaia_xpsd_client/CMakeLists.txt:15,27-28`)；两者同时定义 `ACSD_ABI_SHARED=1` + `ACSD_ABI_EXPORTS=1` |
| C | `/DEF` 白名单，**只导出 `acsd_module_query_v1`** | `acsd_p1_drizzle`(`drizzle/CMakeLists.txt:150-153` + `src/acsd_p1_drizzle.def`)、`acsd_p1_calibration`、`acsd_p1_cosmetic`、`acsd_p1_hips_writer`、`acsd_p1_noise`；5 个 `.def` 内容一致 |

**两个必须记住的陷阱**（子代理取证）：
- 组 C 用的是 `target_link_options("/DEF:...")`（**链接选项**，不是 target source）。
  若日后给这些目标加 `WINDOWS_EXPORT_ALL_SYMBOLS`，CMake 会再生成一个 `.def` ⇒ `link.exe` 收到**两个 `/DEF`** ⇒ 冲突。
- `CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS` **不是** CMake 自动启用的，只是*默认值初始化器*；
  本仓未全局设置该变量，是每个目标显式 `set_property`。且该特性**只对 Windows 上的 MS 兼容工具链实现**——
  仓内用 `if(MSVC)` 守卫，比文档口径更窄（排除 MinGW）。当前声明平台只有 MSVC，**无缺口**；
  但若日后采纳 MinGW 就是缺口。

**三态导出宏正确**：`lib/algorithms/drizzle/healpix_drizzle/hp_drizzle_api.h:17-27` 是完整三态
（`HP_DRIZZLE_EXPORTS`→dllexport / `HP_DRIZZLE_STATIC`→空 / 否则 dllimport / GCC→visibility）。
逻辑自洽：同一批源既编进 STATIC `acsd_drizzle`(`CMakeLists.txt:725`) 又编进 SHARED `acsd_p1_drizzle`，
**`EXPORTS` 分支必须先判**，否则 SHARED 构建会落进 STATIC 分支丢掉 dllexport；
`acsd_drizzle` 因此把 `HP_DRIZZLE_STATIC` 声明为 PUBLIC(`:756`)。

### 4.2 入口 —— ❌ **FAIL（我先前判 PASS，此处更正）**

产品可执行文件 `add_executable(acsd …)`（`CMakeLists.txt:1307`）**未设 `WIN32`** ⇒ `/SUBSYSTEM:CONSOLE`。
入口互斥分支本身写得对（`lib/infrastructure/cli/main.cpp:82-105`）：
`_WIN32` 下 `int wmain(int, wchar_t**)` 经 `WideCharToMultiByte(CP_UTF8 …)` 转发 `real_main`，
`#else` 下才是 `int main(int, char**)`。

**但缺了消费方。** MSVC 的默认 CRT 入口由 `_UNICODE` 决定：
未定义 ⇒ 默认 `mainCRTStartup`（调用 `main`）；定义 ⇒ `wmainCRTStartup`（调用 `wmain`）。
**而 `_UNICODE` / `UNICODE` 在整个产品构建里从未定义**——全树 grep 只命中
`build/acr/_deps/onetbb-src/cmake/compilers/MSVC.cmake:77`，那是 TBB 自带的 CMake，
属 ACR（已确认不在产品图内，见 §2 与 `UNRESOLVED_REGISTER.md:2627`「产品构建 0%」）。
`ENTRY` / `CRTStartup` / `SUBSYSTEM` 在仓内 CMake 里同样 **0 命中**。

⇒ **只定义 `wmain` 而不定义 `main`、也不定义 `_UNICODE` ⇒ 链接期找不到消费 `main` 的入口
⇒ 这是我判断中最可能让 Windows 构建**直接失败（不是告警）**的一条。**
⚠️ 该判断**未经实跑**（需要真实 VS17 链接才能定论），但它是我交给下一棒的**第一优先验证项**：
**首次 Windows 构建请先单独编译/链接 `acsd` 目标确认入口。**
修法（若确证）：在 `CMakeLists.txt` 的 MSVC 分支补
`add_compile_definitions(_UNICODE UNICODE)`（与既有 `NOMINMAX _USE_MATH_DEFINES` 同处），
或显式 `/ENTRY:wmainCRTStartup`。

> **我为何先前判 PASS**：另一位子代理把它记为「UNKNOWN，需实跑」。它指出了同一个风险点但未定性。
> 另一位（已给出 MS 文档依据）定为 FAIL：`link.exe` 无 `/ENTRY:wmainCRTStartup`，
> 而 MSVC 文档说明 `main`/`wmain` 的选择依赖 `_UNICODE`。
> 我复核了 `main.cpp` 原文与全树 grep，**采纳 FAIL**，并保留「未经实跑」的限定。

图内第二个可执行文件 `orchestrator_legacy_cli` 用普通 `main`、不设 WIN32 ⇒ PASS。

### 4.2.1 ⚠️ `.def` 不能把 `dllexport` 降级 —— 两个模块 DLL 的导出面超出白名单

`lib/algorithms/drizzle/src/module_entry.cpp:9-11` 与 `lib/algorithms/drizzle/hips/CMakeLists.txt:120`
都声称 `.def` 对遗留 `dllexport` 符号起「过滤 / 白名单」作用。
**但 MS 文档明说各导出方法可叠加使用**，`.def` 与 `__declspec(dllexport)` 是**并集**，不是覆盖：
- `acsd_p1_drizzle`：`hp_drizzle_api.cpp:5` `#define HP_DRIZZLE_EXPORTS` ⇒ 8 个 `HP_DRIZZLE_API` 函数
  都是 `__declspec(dllexport)`，而 `.def` 只列 `acsd_module_query_v1`
  ⇒ **实际导出 9 个，不是 1 个**。
- `acsd_p1_hips_writer`：`aio_hips.h:23-24` **没有 `#ifndef` 覆盖保护** ⇒ 9 个 `AIO_HIPS_EXPORT`
  函数无条件 `dllexport` ⇒ 同样泄漏。
- `acsd_p1_calibration` / `acsd_p1_cosmetic` **没问题**——它们在**编译期**就消解了
  （`astro_calibration.h:11` 的 `#ifndef AC_API`）⇒ 那里 `.def` 确实是白名单。

⚠️ 这**不阻断构建**（多余导出是安全的），但**违反 `.def` 文件自己声明的意图**，
且与「导出面逐条冻结」的口径不符。**未实测，列为待核实。**

### 4.3 ⚠️ Windows 路径分隔符 —— 一类 Linux 永远暴露不了的真缺陷

危险的不是"把 `/` 传给 Win32"（能容忍），而是**只按 `/` 切分/校验路径的字符串逻辑**——
在 Windows 上它们不是报错，而是**返回错误答案**。

| 级别 | 位置 | 缺陷 |
|---|---|---|
| **HIGH** | `lib/infrastructure/aio/product_io/src/atomic_publish.cpp:64-74`；`lib/infrastructure/scheduler/src/export_stream.cpp:34-37`；`lib/infrastructure/scheduler/src/mosaic_window.cpp:18-21` | `dirname_of`/`basename_of` **只找 `/`**。`"C:\out\x.fits"` 没有 `/` ⇒ dirname 得 `"."` ⇒ **产物静默写到当前工作目录** |
| **HIGH** | `atomic_publish.cpp:98-121` `make_parent_dirs` | 只按 `/` 切分、**丢弃 `_mkdir` 返回值**(`:113`) 且**无条件 `return true`**(`:121`) ⇒ 调用方拿到"成功"，失败延后表现为无关的建文件错误 |
| HIGH（**休眠**） | `lib/infrastructure/pipeline/module_loader/module_registry.c:584,625,717-718` | 拒绝任何不以 `/` 开头的 manifest 路径、只按 `/` 找末段、根边界检查要求 `canon[rl]=='/'` ⇒ Windows 上**每个合法路径都被判为越界**（安全检查恒定 fail-closed）。**但该文件不被任何 target 编译 ⇒ 属契约层/潜在，不是活路径** |
| MEDIUM | `dll_loader.cpp:80`（注释把"用正斜杠"写成未经验证的合同）、`module_adapters.cpp:11894-11899`（`if (rel[0] != '/')` 把 Windows 绝对路径当相对路径 ⇒ sparse SNR 层**静默加载为空**） | |

**冒号：零缺陷。** 无任何代码按 `:` 切路径；驱动器字母处理处处正确
（`aio_atomic_file.h:340,373`、`profile_store.cpp:108`、`backend_loader.cpp:32` 拒绝裸文件名里的 `:`）；
唯一的 `getenv("PATH")` 枚举（`dll_loader.cpp:192-217`）在 `#ifdef _WIN32` 内正确按 `;` 切分。

**仓内正确的参考实现**（修 HIGH 项时照抄）：`aio_atomic_file.h:339-357`（双前缀绝对路径 + make_dirs）、
`profile_store.cpp:104`（`find_last_of("/\\")`）。

**明确不是缺陷**：HiPS 家族的 `/` 是**声明式内部约定**，叶端归一化
（`hips_properties.cpp:46-48` 显式**拒绝**反斜杠；`aio_hips_writer.cpp:200-223` 归一并跳过盘符）。

**已确认是「活路径」而非休眠的一例**：`lib/algorithms/psf/src/dpsf_log.cpp:30-45` 与
`lib/algorithms/star_detection/src/sdet_log.cpp:31-44` 的 Windows 分支写的是
`"lib\\dynamic_psf\\logs"` / `"lib\\star_detector\\logs"`，
POSIX 分支是 `"lib/algorithms/psf/logs"` / `"lib/algorithms/star_detection/logs"`
—— **三处独立差异**（少 `algorithms` 段、`star_detector`≠`star_detection`、`dynamic_psf`≠`psf`），
**早于 `lib/algorithms/**` 迁移**，且是 CWD 相对路径 ⇒ **两个平台把日志写到不同地方**。
子代理已核实这两个文件**确实被编进已发布的 `acsd.exe`**（`CMakeLists.txt:1185-1188`、`:1218-1221`
→ `acsd_p1_dpsf`/`acsd_p1_sdet` → `:1394` `acsd_module_adapters` → `:1415-1416` `acsd_cli_runtime` → `:1335` `acsd`），
所以这是**今天就会跑的缺陷**，不是潜在问题。

### 4.4 ⚠️ 加载器：Windows 上模块 DLL **完全不可加载**

`lib/infrastructure/pipeline/module_loader/secure_loader.c:42-48` 在 `_WIN32` 下把
`ACS_LOADER_IMPL_PLATFORM` 置 0，`:411-415` 直接返回 `ACS_ERR_UNSUPPORTED`；
`#else` 分支(`:333-339`)只定义了 `elf64_ok`（返回 0），**没有** `path_canonical`/`read_file_all`/`resolve_symbol`。
文件自述（`:43-44`）：「Windows 落地属 WIN-* 任务…不包含 win32 LoadLibrary 代码路径」。
⇒ **ABI-006 的导出面建出来了，但 Windows 上一个模块 DLL 都加载不了。**

**范围界定（重要）**：`secure_loader.c` 与 `module_registry.c`
**不被任何跟踪的 CMake target 编译**（grep 验证；仅 `install_layout.cmake:123` 在注释里提到）。
两者是**休眠**状态，不是活路径缺陷。`acsd_infra_orchestrator` 虽被构建，但**未被链接进 `acsd` CLI 闭包**。

### 4.5 随产物配置：一处**双平台皆错**的功能缺陷

- **好消息**：`eng/packaging/config/` 下**没有**任何 JSON 需要贴着二进制放行，这**是设计正确**，不是 Windows 缺陷；
  安装规则平台中立（每个 target 同时给 `LIBRARY DESTINATION` 与 `RUNTIME DESTINATION`，
  而 Windows 把 `.dll` 归为 RUNTIME —— 正是对的，`install_layout.cmake:73-75,79-81,85-87,104-106,129-131,156-158`）；
  无任何 install 规则被 `if(UNIX)`/`if(NOT WIN32)` 门控。⇒ **Windows 安装面 PASS**。
- **F1（HIGH，双平台皆错）**：`acsd benchmark` 传错了目录。`lib/infrastructure/cli/commands.cpp:2628`
  `const std::string install_dir = cli_install_dir();` 随后 `:2638-2639`
  `generate_profile_v2("full", …, install_dir)`；但被调方形参就叫 `backends_dir`
  （`profile_gen_v2.cpp:411-414`），内部读 `backends_dir + "/backends.manifest.json"`(`:447`)
  并 `load_backend(backends_dir, …)`(`:454`)，而安装把该 manifest 写到 **`providers/`**
  （`install_layout.cmake:115-118`）⇒ 读的路径**永不存在** ⇒ **静默降级为 builtin-baseline-only，不报错**。
  对照 `doctor`（`commands.cpp:2569`）用的是正确的 `cli_providers_dir()`。
  ⚠️ `commands.cpp:2626-2627` 的注释还自称"与 cpu_profile/provider 解析同源"——**代码与自己的注释矛盾**。
- **F2/F3（文档漂移）**：`eng/packaging/README.md:14` 声明 `config/`、`launch/` 随安装树发布——**无对应 install 规则**；
  `install_layout.cmake:30-37` 列了 `pipelines/` 与 `README.txt`——`pipelines/` **仓内根本不存在**，`README.txt` 无规则（仓内是 `README.md`）。
- **F4（治理）**：`install_layout.cmake:41` 引 `verify_install_tree.py`、`:166` 引 `check_packaging_consistency.py`——
  **两者都不存在**（`eng/packaging/` 下 0 个 `.py`）。⇒ install 规则与合同文本的对应关系**无机器校验**，F3 的幽灵条目才会长期潜伏。

**路径解析本身 Windows 正确**：`commands.cpp:169-184` 在 `_WIN32` 下用 `GetModuleFileNameA`、
POSIX 下用 `/proc/self/exe`；`:187-190` `cli_providers_dir() = install_dir + "/providers"`。 原文见左 |

---

## 5. 数值等价核对

**结论：未做实测**（无 Windows 产物）。但**已确认优化级别确实不同**，故该核对**必须做**。

| 平台 | 配置 | 实际旗标 | 依据 |
|---|---|---|---|
| Linux 控制节点 | `Release` | **`-O3 -DNDEBUG`** | `build/linux-control/CMakeCache.txt` 实测 `CMAKE_CXX_FLAGS_RELEASE:STRING=-O3 -DNDEBUG`；`CMakeLists.txt:29-31` 单配置默认；`CMakePresets.json:75` |
| Windows 正式 | `RelWithDebInfo` | **`/MD /Zi /O2 /Ob1 /DNDEBUG`** | `CMakePresets.json:90`；旗标为 CMake `Windows-MSVC.cmake` 默认（仓内**无任何 per-config 覆写**） |

⇒ **`-O3` vs `/O2 /Ob1` 是真实的优化级别差**（且 `/Ob1` 比 `/Ob2` 更保守）。
`/GL`+LTCG **不适用**（只在 IPO 下，而 `CMAKE_INTERPROCEDURAL_OPTIMIZATION` 仓内从未设置）。

**浮点收缩差异（全局，重要）**：`CMakeLists.txt:826-833` 只给**两个 AVX2 benchmark target**
加了 `/fp:contract`（理由见 `:811-814`：「Linux 腿 GCC 默认 `-ffp-contract=fast` 会收缩」）。
⇒ 其余代码 MSVC 走 `/fp:precise` **不收缩 FMA**，GCC 走 `-ffp-contract=fast` **收缩**
⇒ 全局存在逐点 FMA 差异。**这不是「优化级别不同」，是「编译器默认行为不同」，量级更大。**

**一条有利证据（我实测）**：`long double` 在 MSVC/x64 下退化为 64 位 `double`，是本项目最大的数值风险源。实测全仓跟踪源码：

```
$ git ls-files '*.cpp' '*.h' '*.hpp' '*.c' | xargs grep -c "long double"
lib/infrastructure/aio/third_party/cfitsio/cfortran.h:1
lib/third_party/nlohmann/json.hpp:3
实验/healpix-polar/code/p1_rootcause.cpp:10
实验/healpix-polar/code/p9_review.cpp:1
实验/healpix-polar/code/polar_common.h:4
```

⇒ **第一方产品图 `lib/` 零 `long double`**；命中项全在第三方与 `实验/`，
而根 `CMakeLists.txt` 的 **14 个生效 `add_subdirectory`**（19 处提及，其中 5 处在注释里）
**无一在 `实验/`**
⇒ **该差异不影响产品数值**。

⚠️ **「零告警」与「数值等价」两个口径都要加限定**（都是子代理取证、我复核过原文）：

**(a) 关于容差**：P2 的两个模块**冻结为逐位一致**，不是容差一致 ——
`docs/engineering/PUBLIC_API.md:1370-1372`（P2 rejection）「容差=bitwise（无 epsilon 门…）」、
`:1483-1485`（P2 sampler）「容差=bitwise」。
而 P2 的判据链路正好压在**特殊函数最敏感的地方**：`rejection.cpp:67-70` 用
`std::lgamma`/`std::log`/`std::exp` 算 Beta，`:100` 用 `std::erfc`，
`:841-852` 的 `rcr_erfc_custom` 手写有理近似（只用 `sqrt`+`pow`），
`:59` 循环里还有 `if (std::fabs(del - 1.0) < 1e-12) break;` ——
**`lgamma`/`erfc`/`pow` 的最后一位 ULP 在 glibc 与 UCRT 之间本就会差**，
而 `:169-170` 明写本实现「数值语义与 oracle **逐位对齐**以便 rejected-set 精确比对」。
⇒ **数值等价核对预计最先在这里变红，且那是合同失败、不是容差问题。**

**(b) 关于「归约」**：一个**有力的反面结论**——`lib/` 里**没有 `#pragma omp simd`**（0 命中），
且**所有 OpenMP `reduction(...)` 子句都是整数或布尔**
（`n_success`/`count`/`n_quick`/`mask_count`/`nonfinite`/`success_count`/`n_valid_fsyn` 等）
⇒ 整数/布尔归约**精确且与次序无关**，OpenMP 归约**不是**本项目的跨平台 FP 风险。
真正受归约次序影响的只有 P3 drizzle 的 tile 合并（`drizzle_engine.cpp:1484-1508`）。
⚠️ 但要注意 `drizzle_engine.cpp:1980-1996` 的 `push_leaf` 按 `touched` 序追加，
⇒ **drizzle trace JSONL 的行序会因 STL 实现不同而不同，即使每个像素值完全相同**——
**比对产物前必须先按 `ipix` 排序**，否则会误判成数值差异。
「未决面…Windows 侧浮点开关（`/fp:*`）、优化开关（`/O2` 与 `/GL`/LTCG/PGO 禁面）…
仓内（含 git 全历史）没有这些取值的机器锚或构建锚」。
已核实 `eng/packaging/schemas/preset-contract.json` **无任何 fp/opt 键**，
`forbidden` 只列 generator/toolset/arch/CRT ⇒ **仓内不存在可供比对的浮点合同**。

### 5.1 快速数学（fast-math）：产品构建里**不存在**，已核实

`-ffast-math` / `-Ofast` / `/fp:fast` 在 `CMakeLists.txt` 与 `CMakePresets.json` 中**零命中**。
仅有的出现都在**非产品通道**：一个遗留 `Makefile`，另一个还明写「不启用 -ffast-math」。
⇒ **`-O3` vs `/O2` 不是语义错配**（两边都不允许重结合）；但 `-O3` 比 `/O2` 更激进自动向量化，
**可能改变浮点求和的归约次序**。

### 5.2 真正的 FP 风险是 FMA 收缩，且**仓库自己写了**

`CMakeLists.txt:811-814` 明写：GCC 默认 `-ffp-contract=fast` 会收缩 FMA，
而 MSVC ≥ VS2022 的 `/fp:precise` 默认**不收缩**，项目用 `/fp:contract` 归一。
**但该旗标只加在四个 benchmark kernel OBJECT target 上**（`:829`、`:1019`），
**没有任何全局或逐科学目标的浮点旗标**。
⚠️ 且 AVX-512 侧**不对称**：`:865` `acsd_cpu_avx512_kernels /arch:AVX512` 与
`:1021` `acsd_cpuprov_avx512_kernels /arch:AVX512` **没有** `/fp:contract`，
而它们 AVX2 的孪生体（`:827-830`、`:1017-1020`）**有**。
基线 TU 按 `eng/tools/quality/isa_sites.json` 的 `baseline_flags=["-msse2"]` 走 SSE2，两边都无 FMA
⇒ 暴露面限于变体 kernel 与任何 FMA-capable 基线 ISA。

### 5.3 跨平台数值判定面：两条实测到的真实差异

**(a) 配置不可比 —— 内存上限把并发压成 1。**
`lib/infrastructure/scheduler/src/module_adapters.cpp:2310-2321` 在 Windows 下返回 0，
于是 `p1_memory_cap`(`:2325-2327`) 把 P1 帧并发强制为 **1**；Linux 则按 F=8/W_eff=16 伸缩。
⇒ **Windows 与 Linux 的比对不是在同一执行配置下做的**，结果不可直接对照。

**(b) 排序不稳定导致测天解本身可漂移（HIGH）。**
`lib/algorithms/platesolve/cpp/ipv/src/ipv_polygon.cpp:759-761` 用**无 tiebreaker 的比较器**
对 `double` 票数排序，而输入来自 `unordered_map<VoteKey,double,VoteKeyHash>` 的遍历(`:746-747`)。
`std::sort` **不稳定** ⇒ 并列票按哈希/桶序决出，**libstdc++ 与 MSVC STL 的桶序不同**。
并列很常见：`:405-409` 在整数票之上叠加了一个分数的 `alpha*consistency` 奖励。
胜者直接喂给 **PROSAC 采样** ⇒ **天测解本身可能跨编译器不同**。

**(c) P3 写出端把 FP64 也降成 float32（HIGH）。**
`lib/algorithms/drizzle/healpix_drizzle/astro_sphere_sink.cpp:503,542,544,549,560`
在 FP64 实例化（`:627 float`、`:630 double`）下仍硬接 float32 缓冲与 `AIO_HIPS_FLOAT32`
⇒ **发布的 flux/area/variance 列一律是 FP32**。
⇒ **P3 产物的任何跨平台核对，其相对精度上限被写出端本身压到约 6e-8**，与编译优化无关。

### 5.4 容差口径已定位，但**跨平台容差在仓内是空的**（需负责人裁决）

`docs/engineering/TEST_MATRIX.md:21-25` 给出阶梯（逐字）：

```
FP64 非归约: `rtol=1e-12, atol=1e-13×scale`
FP32 产品非归约: `rtol=5e-6, atol=1e-6×scale`
归约: γ_n = n·u/(1−n·u)，C≤4
并行等价（1/N worker、baseline/ISA）判据 = 容差等价，不是逐位一致
```

**关键缺口**：`TEST_MATRIX.md:54` 把「编译器/平台/ISA/后端 → 双平台等价矩阵」列为必需触发项，
但 `docs/engineering/SCHEDULER_CONTRACT.md` 写的是「跨后端等价=无此合同，判据面为空」。

**✅ 更正：并非完全为空 —— 仓内确有一条已冻结的跨编译器容差。**
`docs/science/algorithms/PHASE2_INTEGRATION.md:295-299` 逐字：

```
- 冻结容差汇总: F1/F1b/F2/F4/F5/F7 = bitwise/rtol 0；F6 = rtol 1e-12
  （NumPy 参考域）；无其他容差（本层禁引入 epsilon）。
  **适用域**：rtol 0 只在归约顺序完全一致时可达——F1/F1b 的期望值用同一 `i=0..count-1`
  固定序、同一 `vs/wsum` 双累加器复算；跨编译器/跨 FMA 契约的复算必须降到 rtol 1e-12
  并在报告中声明所用域。
```

⇒ **仓库自己早就认定 bitwise 跨编译器不可达，并把退档写成 rtol 1e-12**。
这与通用 FP64 阶梯（`TEST_MATRIX.md:21` `rtol=1e-12`）一致 ⇒ **FP64 面用 rtol 1e-12 有仓内依据**；
FP32 面仍按 `TEST_MATRIX.md:22` 的 `rtol=5e-6`；P3 Phase-1 HiPS 输出按写出端设计本就是 FP32（上限约 6e-8）。
**缺的只是**：这条退档从未被应用到 Windows `/fp:*` 面，也没有登记为通用跨平台规则。
⚠️ 该条款**作用域是 P2 integration 层**；P2/P3/P4/P5 的容差不在
`GATES_AND_TOLERANCES.md` 里（该表按 `DOCUMENT_INDEX.yaml:138-141` 只覆盖 P1），
而在各模块 ALG 文档 + `TEST_MATRIX.md`。

**⚠️ 顺带一处治理缺口**：`GATES_AND_TOLERANCES.md:5-6` 自述只覆盖星检测/PSF/WCS
（§3 全部 20 行都是 `G-P1-*` 前缀），**但它自己 `:20-21` 的 R1 规则要求"每条容差都要登记在此"**
⇒ **规则与登记表自相矛盾**：P2–P5 的容差事实上散落在各模块 ALG 文档，
没有单一可机检的容差登记面。查 P2–P5 的验收数**不要去 `GATES_AND_TOLERANCES.md` 找**。
当前已散落登记的数值举例：P5 接缝门 `≤1e-2`（`PHASE2_UPM.md:333-335`）、
P2 噪声 oracle `5%`/`10%`/`≤2%`（`NOISE_MODEL.md:288,345,347`）、
P3 逐叶 `<1e-5`（`DRIZZLE_GEOMETRY.md:321`）。

### 5.5 仓内自己的状态台账与本件结论一致

`docs/engineering/RELEASE_STATUS.md` 逐字：

```
:102 | Windows 工具链 preset | `CONTRACT_READY` | BLD-001 + eng/packaging/schemas/preset-contract.json |
:120 | Windows DLL 化发布安装树（Windows 侧复验） | `NOT_VERIFIED` | Linux 技术预览安装面已达 INSTALLED；Windows 侧未产出/复验 |
:121 | Windows MSVC 编译 + 测试（Win10 22H2 下限 / Win11 主验证） | `NOT_VERIFIED` | Fatduck 侧执行，未完成 |
:51  Windows x64 复验面: NOT_VERIFIED（VERIFIED 要求正式平台 + 真实数据验收通过）
```

⇒ **Windows 侧只到 `CONTRACT_READY`，从未有过 `VERIFIED`。**
所以本单产出的将**是第一次真实 MSVC 编译证据，不是对既有 Windows 基线的回归**。
（这也与 §1.5 的 FINAL-07 那次失败构建一致：**建过，但从未绿过。**）

---

## 6. 我推翻的既有判定

| # | 被推翻的说法 | 实际 | 证据 |
|---|---|---|---|
| R1 | 任务书「FATDUCK（10.0.26220）」 | `10.0.26220` 非法 IPv4，被解析成 `10.0.102.108`；真实地址 **`100.104.10.71`** | `ssh` 报错原文 + `git show 2f20a99d:FATDUCK_ACCESS.md` |
| R2 | 子代理：「执行器缺位是治理缺陷」 | **是规范 08 §2 的计划内拆除**，§4 规定本单之后才重建。推翻 | `standards/08_编译与CI重做规范.md:10-18, 27-29` |
| R3 | 子代理：「`module_entry.c:60` 的 `__attribute__((unused))` 未保护，MSVC 会 C2061」 | **已有 `#if defined(__GNUC__)` / `#else` 保护**，MSVC 走 `#else`，不是缺陷 | 原文：`#if defined(__GNUC__)` … `#else` `static const char kLogComponent[] = …` `#endif` |
| R4 | 子代理甲：「只有 2 个目标带 `ACSD_WARNINGS_OFF`」 | **5 个**（`acsd_cfitsio`、`acsd_orchestrator_jsv`、`acsd_p1_drizzle`、`acsd_p1_hips_writer`、`acsd_phase2_integrate`）。两子代理结论冲突，**我逐条 grep 调用点后以 5 个为准** | `CMakeLists.txt:544,586`；`drizzle/CMakeLists.txt:85`；`drizzle/hips/CMakeLists.txt:73`；`integration/phase2_integrate/CMakeLists.txt:101`；`orchestrator/cpp/CMakeLists.txt:43` |
| R5 | 子代理乙：「`sampler.cpp:35` 触发 C4005」 | **判为良性**：与 `/D NOMINMAX` 替换列表逐字相同（均为空），标准允许相同定义重定义，MSVC C4005 仅在定义不同时触发 | `sampler.cpp:35` 与 `CMakeLists.txt:55` 原文 |
| R6 | 任务书隐含「Linux `-Wall -Wextra` 覆盖面 = Windows `/W4` 覆盖面」 | 两者**不同构**：Linux 的 `-Wall -Wextra -Wpedantic -Wconversion` 只加在固定 19 target 列表上（`CMakeLists.txt:1350-1363`），`acsd_aio` 等不收；MSVC `/W4` 是目录级、覆盖除 2 个隔离目标外的全部 | 两处原文 |

**另记两处仓内文档/代码不一致（非警告，但会误导后续执行）**：
- `CMakeLists.txt:538` 注释称 pthread shim 是「SRWLOCK 映射」，实际
  `eng/cmake/win32_pthread_shim/pthread.h:1-5` 明写「纯原子自旋锁…**不 include windows.h**」以避开 SRWLOCK 冲突。
- `CMakeLists.txt:557` `if(ENV{ACS_ZLIB_LIB})` 是**恒假死分支**（CMake 会把 env 值再当变量名解引用），
  仓内 `gaia_xpsd_client/CMakeLists.txt:79-84` 已自登记该 finding。
- `CMakePresets.json:4,14` 指向的 `eng/packaging/schemas/preset-contract.schema.json`
  **在任何提交里都不存在**（实际文件是 `preset-contract.json`）。

---

## 7. 自证段

### 7.1 本单做了什么 / 没做什么

- ✅ 核实 Windows 主机可达性（并**纠正了错误地址**）；失败原因定位到「主机掉线」而非「工具链缺失」
- ✅ 核实同步口径（git fail-closed、run/ tracked vs untracked）
- ✅ 核实冻结工具链合同 vs 主机实况的冲突面
- ✅ 按纪律派发 5 个子代理分头审计，逐条复核其结论，**并推翻其中 3 条**（R3/R4/R5）
- ✅ 静态核对导出宏 / 入口 / 产品清单 / 命名一致性
- ✅ 静态核对优化级别与浮点口径差，确认数值等价核对**必须做**
- ❌ **没有跑任何 Windows 构建**（主机离线）
- ❌ **没有采集到任何真实 `cl` 警告**（所以没有警告总数、没有"修到零"的结论）
- ❌ **没有改任何源文件**——在拿到真实告警清单前改代码＝盲改；且纪律要求「先取得证据再判定」
- ❌ **没有做任何 git 写操作**（全程仅 `log`/`show`/`ls-files`/`diff`/`rev-parse`/`cat-file`/`ls-tree`）

### 7.2 判定「修不掉」的自证

本单**未声称任何警告修不掉**，因为没有任何一条 `cl` 输出可供判定。
按纪律「未取得该条不改动的证据前，不得声称任何警告『无法修』」，
在无实测的情况下写「修不掉」与写「零警告」同样是编造。

### 7.3 复现命令

**A. 复现「主机离线」结论（Linux 侧，任何时候可跑）**

```bash
# 1) 纠正地址：证明 10.0.26220 被解析成 10.0.102.108
ssh -o BatchMode=yes -o ConnectTimeout=10 10.0.26220 hostname 2>&1 | head -2
#   期望：ssh: connect to host 10.0.102.108 port 22: ...

# 2) 真实节点状态
tailscale status --json 2>/dev/null | python3 -c "
import json,sys; d=json.load(sys.stdin)
for p in d.get('Peer',{}).values():
    if (p.get('HostName') or '').lower()=='fatduck':
        print({k:p.get(k) for k in ('HostName','Online','LastSeen','TailscaleIPs')})"

tailscale ping 100.104.10.71          # 期望：timed out（离线时）

# 3) 真实地址与接入说明来源
git show 2f20a99d:FATDUCK_ACCESS.md | grep -oE '([0-9]{1,3}\.){3}[0-9]{1,3}' | sort -u

# 4) 持续等待（窗口重开后自动接手）
nohup /tmp/g08-09/poll_fatduck.sh >/dev/null 2>&1 &
tail -f /tmp/g08-09/fatduck_wait.log
```

**B. 机器恢复后，跑完整构建（脚本已就绪，`bash -n` 通过）**

```bash
bash /tmp/g08-09/run_on_fatduck.sh 2>&1 | tee /tmp/g08-09/driver.log
```

该脚本依次做：可达性探测 → 采集工具链实况（vswhere / vcvars64 / cmake / ninja / git / python / 磁盘）
→ `git clone` 到 `C:/Users/fujia/astrocs-g08-09` → `cmake --preset win-msvc-17.14.39-x64`
→ `cmake --build --preset win-rel` → 拉回完整日志并按 `warning XXXX` 统计分类。

**手工等价命令**（前台可直接重跑）：

```bash
# —— 在 FATDUCK 上（pwsh）——
$env:ACS_ZLIB_ROOT='C:/Users/fujia/zlib-msvc'   # gaia_xpsd_client/CMakeLists.txt:96 find_package(ZLIB REQUIRED)
git clone git@github.com:fujiaze/Astro-CS-Database.git C:/Users/fujia/astrocs-g08-09
cd C:/Users/fujia/astrocs-g08-09
git log -1 --format='HEAD=%H %ci %s'            # 必须 == 3259a95
cmake --preset win-msvc-17.14.39-x64 2>&1 | Tee-Object C:/Users/fujia/g08-09-configure.log
cmake --build --preset win-rel 2>&1 | Tee-Object C:/Users/fujia/g08-09-build.log

# —— 拉回并统计全部告警（不看前若干条）——
scp fujia@100.104.10.71:C:/Users/fujia/g08-09-build.log .
grep -oE 'warning [A-Z]+[0-9]+' g08-09-build.log | sort | uniq -c | sort -rn
grep -cE 'warning [A-Z]+[0-9]+' g08-09-build.log
```

**C. 复现本件的静态结论**

```bash
cd "/workspace/Astro CS Database"

# W1  /W3 降级
sed -n '36,40p' lib/infrastructure/gaia_xpsd_client/CMakeLists.txt
# W2  C4267 候选（_WIN32 分支，Linux 不编译）
sed -n '516,522p' lib/infrastructure/gaia_xpsd_client/src/gaia_client.c
# R3  module_entry.c 的 __GNUC__ 保护（推翻子代理）
sed -n '57,63p' lib/infrastructure/gaia_xpsd_client/src/module_entry.c
# S1  5 个 ACSD_WARNINGS_OFF 目标
git grep -n 'acsd_cfitsio_isolate_warnings(\|ACSD_WARNINGS_OFF' -- CMakeLists.txt 'lib/**'
# R5  NOMINMAX 良性重定义
sed -n '35p;41,43p;57,59p' lib/algorithms/coverage/src/sampler.cpp
# §5  产品图零 long double
git ls-files -z '*.cpp' '*.h' '*.hpp' '*.c' | xargs -0 grep -c 'long double' | grep -v ':0$'
# §4  导出机制
grep -n 'WINDOWS_EXPORT_ALL_SYMBOLS\|OUTPUT_NAME "acsd_io"' CMakeLists.txt
sed -n '6,12p' lib/infrastructure/aio/include/astro_image_io.h
sed -n '54p' lib/infrastructure/pipeline/orchestrator/cpp/src/dll_loader.cpp
# §1  git fail-closed
sed -n '80,90p' CMakeLists.txt
```

### 5.6 ⚠️ 一条与告警无关、但比告警更危险的跨平台行为背离

**pthread shim 把「递归互斥」静默降级成非递归自旋锁。**
cfitsio 明确索要递归锁（`lib/infrastructure/aio/third_party/cfitsio/cfileio.c:83-96`）：

```c
    /* Init the main fitsio lock here since we need a a recursive lock */
    pthread_mutexattr_settype(&mutex_init, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&Fitsio_Lock, &mutex_init);
```

而 Windows 侧拿到的实现（`eng/cmake/win32_pthread_shim/pthread.h:35-46`）**把 attr 整个丢掉**：

```c
    static inline int pthread_mutex_init(pthread_mutex_t* m, const pthread_mutexattr_t* a) {
      (void)a;            // ← attr 被丢弃
      m->locked = 0;  return 0; }
    static inline int pthread_mutex_lock(pthread_mutex_t* m) {
      while (_InterlockedExchange(&m->locked, 1) != 0) { /* 忙等 */ }  return 0; }
```

`pthread.h:23` 自己写了注释：`#define PTHREAD_MUTEX_RECURSIVE 1  // …shim 近似非递归`。
且 `pthread_mutexattr_settype`(`:55-58`) 仍返回 0 ⇒ **调用方看不到任何失败**。

⇒ **Linux 侧递归锁 honored，Windows 侧静默不 honored**：
持有 `Fitsio_Lock` 时任何重入的 cfitsio 调用都会**在 Windows 上自死锁并烧满一个核**。
⚠️ **它不产生任何 /W4 告警** —— 这正是"光靠告警审计会漏掉"的那一类。
（另：`CMakeLists.txt:538` 把该垫片称作「SRWLOCK 映射」，但 SRWLOCK 同样非递归，
所以**声明的意图带同一个缺陷**。是否真存在可达的重入路径**未经运行时证明**，此处只登记契约背离。）

### 5.7 其余「静默给错答案」的跨平台行为差异（按建议优先级）

| 优先级 | 位置 | 差异 |
|---|---|---|
| HIGH | `lib/infrastructure/cli/process.cpp:117-120` | Windows 用 `GetExitCodeProcess` **不掩码**，把完整 32 位映射进 `exit_code`；POSIX(`:198-205`) 用 `WEXITSTATUS`(0..255) 且信号死亡另作处理 ⇒ 一次 AV 崩溃在 Windows 上变成 `exit_code=-1073741819` 且 `exited=true`，按 `acsd_process.h:5-6` 的合同会被误判成「启动失败」 |
| HIGH | `process.cpp:70` (Windows, **父进程**) vs `:164` (POSIX, **仅子进程**) | `extra_env` 在 Windows 上写进**父进程**环境，泄漏到后续所有子进程；POSIX 才是预期行为 |
| HIGH | `process.cpp:95` | `cwd_utf8` 原样传给 `CreateProcessA` 的 **ANSI** `lpCurrentDirectory`，**未加宽**（与 `acsd_process.h:30` 注释「Windows 变宽字符后传」矛盾）⇒ 非 ASCII 安装路径在 Windows 上坏 |
| HIGH | `lib/phase3_session/p3_export.cpp:113-123` `mkdirs()` | **只按 `/` 切分**且**丢弃 `_mkdir` 返回值** ⇒ `D:\out\run1` 完全不切分、一次 `_mkdir` 失败被吞 ⇒ 无目录也无报错 |
| HIGH | `lib/infrastructure/gaia_xpsd_client/src/gaia_client.c:2194` vs `:2255` | Windows 用 `FindFirstFileA`（NTFS 字典序）、POSIX 用 `readdir`（任意序），而 `:2327-2328` 用 `files[0]` 判 `is_dr3sp` ⇒ **混合目录里自动选中的数据集由遍历序决定**。两侧都谈不上"正确"，应显式排序 |
| MED-HIGH | `lib/infrastructure/cli/cancel_token.h:22-38` | Windows 的 `SetConsoleCtrlHandler` **从不检查 `type`**，对**所有**事件返回 TRUE ⇒ CTRL_CLOSE/LOGOFF/SHUTDOWN 被吞、OS 默认终止被抑制；且该 handler 跑在新建线程上 |
| MED-HIGH | `process.cpp:105-114` vs `:195-196` | Windows 单次阻塞 `WaitForSingleObject` + 5s 有界回收后**继续往下走**（可能弃掉未回收子进程）；POSIX 10ms 轮询后**无限** `waitpid` |
| MED | `lib/infrastructure/cli/disk_gate.h:70` | `GetLastError()` 与 POSIX `errno` **是两个不相交的命名空间**，却都渲染成 `"errno=" + 值` ⇒ Windows 上 `errno=3` 是 `ERROR_PATH_NOT_FOUND`，Linux 上 errno 3 是 `ESRCH` |
| **HIGH** | `lib/infrastructure/scheduler/src/module_adapters.cpp:2310-2327` | Windows 把 P1 帧并发**硬钉为 1**，而遥测仍报真实核数（`monitor.h:452-460`、`cpu_budget.cpp:26-43`）⇒ 报表说多核、实际单线程。⚠️ **五路审计一致把它列为「第一功能级背离」**：`:2300-2306` 的注释记录该值是**实测标定**出来的（`P1-CONCURRENCY-CALIB-01`，`W_eff=16`，相对 `W_eff=4` 实测 `1.86×` 加速，峰值 RSS 13.940 GB）——**这套标定在正式平台上被整体丢弃**。而 P1 是五创新点之**根**（AGENTS.md §10）⇒ **在正式平台上静默废掉了根创新点的标定并发**。`module_adapters.cpp` 本身**在 `/W4` 构建内**，不是休眠代码 |

### 4.6 ⚠️ 又一批**在构建图内**的 Windows FAIL（未实测，静态取证）

| 位置 | 问题 |
|---|---|
| `lib/algorithms/platesolve/cpp/ipv/src/ipv_select.cpp:148,167,183` | `LoadLibraryA("astro_image_io.dll")` / `"star_detector.dll"` / `"gaia_client.dll"` —— **三个都不是 CMake target、也都没被安装**。但 `CMakeLists.txt:1144-1152` 把 `ipv_select.cpp` **和** `gaia_client.c` 直接编进静态 `acsd_p1_ipv`，该库在 `acsd.exe` 闭包内(`:1394`)。POSIX 分支静态绑定同一批符号 ⇒ **Linux 全然看不见，Windows 上 `GetProcAddress` 必失败** |
| `lib/infrastructure/aio/io/fits_core.c:1494` | `rename(wr->tmp, wr->target)` 无保护；MSVC 的 `rename`==`_rename` **拒绝替换已存在的目标** ⇒ 在 Windows 上**重复发布同一 FITS 产物必然失败**，Linux 成功。仓内正确修法已写在 `io_adapter.cpp:124-136` |
| `lib/infrastructure/aio/product_io/src/atomic_publish.cpp:431-434` | Windows 分支先 `remove(target)` 再 `rename` ⇒ **违反本仓自己冻结的禁令** `aio_atomic_file.h:17-19`「禁止先删目标再 rename」（该窗口内崩溃即丢产物） |
| `atomic_publish.cpp:242-246` | `remove_tree()` 在 `_WIN32` 下 `(void)path; return false;` **恒返回 false**，与 `atomic_publish.h:150-151` 的合同「路径不存在 ⇒ true」矛盾。正确实现已在 `aio_atomic_file.h:516-529` |
| `eng/cmake/win32_pthread_shim/../aio_atomic_file.h` 窄 ANSI I/O | `_stat64`(:306-310)、`FindFirstFileA`(:410,451,518)、**`MoveFileExA`(:594-596)**、`_mkdir`/`_open`/`_unlink`/`_rmdir` 等全用窄 API，而**同一头文件**的 `atomic_replace`(:97) 正确用了 `MoveFileExW`；头文件 `:20-21` 自己要求加宽，`widen_utf8` 已存在(:81)。同类：`aio_abi.cpp:177-178`（形参就叫 `path_utf8`）、`aio_hips_reader.cpp:93-96`（§10 完成门） |
| `lib/algorithms/coverage/hips_properties.cpp:46` | `path_is_safe()` 对用户路径**一律拒绝 `\`** ⇒ `"C:\work\hips"` 被拒。**全树唯一**把 `\` 当非法的地方（其余 basename 提取都用 `find_last_of("/\\")`）。在构建内(`CMakeLists.txt:710-711`)，经 `p3_session.cpp:149` 用户 JSON 可达 |

**已核实不是缺陷（记录下来免得被"顺手修坏"）**：`dll_loader.cpp:200` 在 `#ifdef _WIN32` 内按 `;` 切 `PATH`（正确，且是全树唯一）；`parser.cpp:192-214` 的 `sanitize_path()` 正确归一 `\`→`/` 并特判盘符；`backend_loader.cpp:27-42` 正确拒绝裸文件名里的 `/`、`\`、`:`；`profile_store.cpp:103-116` 有真实的 `_WIN32` 盘符分支。

### 5.8 补充：两处「f32 收窄」的科学面影响（与平台无关，但影响数值等价的解读）

**(a) P4 的 SNR 在输出边界丢掉 FP64，且仓内既有理由不适用。**
`snr_evaluator.cpp:306,317,379` 全部 `return static_cast<float>(snr_phot_ * snr_psf / median_snr_);`，
而头文件 `snr_evaluator.h:59-62` 明写「**FP64 控制点版本**：snr_psf 以 double 存储并参与评估」，
IDW 内部（`:294-295,355-356`）也确是 double，消费链 `hp_drizzle_api.cpp:658→691→692` 以 `std::vector<float>` 喂进
**FP64 的 P3 kernel**。
⚠️ 仓内为 f32 SNR 给的理由（`snr_estimator.h:413-414`）是「HISS SNR 子块格式已冻结为 float32…
**SNR 是诊断值不是科学累加值**，精度损失可接受」——**但在 P4 重建路径里 SNR 是积分输入、不是诊断值，
该理由不成立**。
**目前是潜伏的**：该图当前是未使用参数（`drizzle_engine.cpp:1200` 形参被注释成 `float /*snrValue*/`），
所以在 SNR 接进权重之前不产生代价。

**(b) P1 的 Simpson 积分硬编码等间距假设，且无运行期校验。**
`spectrum_integrator.cpp:134` `double h = (x[n_pts-1] - x[0]) / (n_pts-1); // 等间距假设`，
生产调用点 `:285` 与 `:412,:458` 传的是 XP 波长栅格。
**严重性有节制**：该假设是**写进合同的**——`spectrum_integrator.h:3` 明写「Simpson 1/3 复合…
**等间距**」，所以今天没有合同违反。
暴露点在于：`prepare_curve`(`:21-41`) 只强制严格单调、**不校验间距**；
栅格目前恰好均匀只因为它以整数 nm 合成 ⇒ **将来任何非均匀生产者会静默用错 h**——
不报错、不出 NaN、给出看起来合理的通量。同文件 `akima_interpolate`(`:46-125`) 做对了
（逐区间 dy/dx、局部 dx 缩放、显式 dx<=0 退出），说明正确范式在仓内已有。

**⚠️ 一处存疑字面量（登记为 UNKNOWN，不断言为缺陷）**：`rejection.cpp:195` 的 `kRcrSSUnity[1001]`
表，第 `[5]` 项 = `8.81255`，相对邻项 `[4]=2.15681`、`[6]=2.72685` 是 3 倍以上离群，
而该表其余部分落在 ~1.87–2.9 区间。或是忠实转录自其引用的 oracle（`rejection.cpp:162-163` 标注
`nickk124/robust-outlier-rejection` commit `a8a29a6`），或是转录错误——**oracle 不在仓内，出处无法核实**。
它进入硬分支 `rejection.cpp:969-976`。建议独立复核（成本低）。

**一条需要修正的"假前提"**：`drizzle_engine.cpp:1484-1485` 用「src 迭代序由构建序决定」解释
per-leaf 归约次序不变——但 `std::unordered_map` **不保证插入序**，该机制陈述是错的。
不变量本身成立（次序由 `merge_cursor` 强制的 stripe 升序左折叠决定），**但它被一条错误理由"保护"着**，
建议顺手改注释，避免后人据错误前提改动它。

### 5.9 aio 子系统补遗：告警 ID 更正 + 几处一行可修的静默缺陷

**⚠️ 一处告警 ID 更正**：`int → size_t` 的加宽族**不是 C4267**。
x64 上 `int`→`size_t` 是 32→64 位**加宽、不丢位**，唯一问题是符号；
MSVC `/W4` 对有符号→无符号失配的诊断是 **C4245**（level 4），C4365 是 `/Wall` 变体。
⇒ **预期 C4245，不是 C4267**；无论哪个都需要显式转换。
活站点（均在已编译 TU）：`aio_util.h:13,17`、`aio_healpix_io.cpp:73,77`、`aio_pipeline.cpp:83,88`、
`hiss_stream_writer.cpp:164,166`。
**仓内已证明正确写法**：`aio_atomic_file.h:85` `std::wstring ws((size_t)len, L'\0');` 与
`:87` `ws.resize((size_t)len - 1);` 已带转换，**被复制粘贴的那几处漏了**。

**几处一行可修的静默缺陷**（无告警，但都有仓内正确范式可抄）：

| 位置 | 缺陷 | 仓内已验证的正确范式 |
|---|---|---|
| `aio_disk_full.h:84-90` | `#ifdef EDQUOT` 被编译掉——**MSVC UCRT 的 `errno.h` 不定义 `EDQUOT`** ⇒ 配额耗尽在 Windows 上不可见 | **同一子系统的 `io/fits_core.c:43-48` 已写好 shim**：`#if defined(_WIN32) && !defined(EDQUOT) #define EDQUOT 122`。直接抄，一行 |
| `aio_log.cpp:39-48` | Windows 用 `CreateDirectoryA`（**单层**），POSIX 用 `std::filesystem::create_directories`（**递归**）⇒ 非仓根 CWD 下 Windows **静默不产生日志文件**。该文件 `:32-37` 的注释自称这类问题已修——**只修了吞返回值，没修单层/递归不一致** | — |
| `aio_sparse_punch.h:216-232` | `errno_is_unsupported` 在 Windows 恒返回 true ⇒ 每次 `DeviceIoControl` 失败都被报成"trim=skipped"降级而非 `PUNCH_IO_ERROR` ⇒ **fail-open，而 Linux 是 fail-closed**。该文件 `:32-33` 自认 Windows 分支**从未在真实硬件上跑过** | — |
| `aio_sparse_punch.h:71-75` | `kPunchBlockBytes` Windows 65536ULL vs Linux 4096ULL（`:68-70` 有说明，是有意的），但它改变 `PunchResult` 的 `zero_blocks/punched_bytes/holes` ⇒ **provenance 载荷跨 OS 不可比**（字节内容仍可比） | — |
| `aio_sysinfo.cpp:249-259` | `aio_process_tree_rss_bytes` 在 Windows **只测当前进程**，POSIX 测「本进程 + 全部后代」⇒ **违反其自己冻结的合同** `include/aio_sysinfo.h:60-63`「当前进程树（本进程 + 其全部后代进程）」；内存压力判据的分子在 Windows 上系统性偏小 | — |
| `atomic_publish.cpp:398` | `size = _filelengthi64(fd);` 未查返回值，失败返回 `-1`，而 `size` 是 `uint64_t` ⇒ 变成 `0xFFFFFFFFFFFFFFFF`（**真 bug，不只是告警**） | — |

**已确认是死代码、不要为其分配修警预算**（三路独立 grep 确认零 target 引用）：
`lib/infrastructure/aio/healpix_db/archive/legacy/healpix_stack/**`（30 文件，且**未**登记在
`dependency-lock.json` 里，连 vendored 身份都没有）、`lib/infrastructure/aio/src/ahpx/**`（4 文件）、
`lib/infrastructure/aio/src/aio_pipeline_engine.cpp`、`lib/infrastructure/aio/io/hips_core.c`、
以及 `lib/infrastructure/pipeline/module_loader/{module_registry,secure_loader}.c`。
按 AGENTS.md §6 应**删除或显式标记退役**——目前它们是"带着真缺陷、无人负责、却看着像产品代码"的状态。

### 5.10 两个竞争性的 `<unistd.h>` 垫片——**没用的那个才是活的那个**

| | 内容 | 状态 |
|---|---|---|
| `lib/infrastructure/aio/third_party/cfitsio/win_compat/unistd.h` | **完整映射垫片**：`#include <io.h>/<process.h>/<direct.h>/<fcntl.h>`，并把 `getcwd→_getcwd`、`getpid→_getpid`、`read→_read`、`write→_write`、`close→_close`、`unlink→_unlink`、`access→_access`、`ftruncate→_chsize`、`fsync→_commit`、`isatty→_isatty` 全部 `#define`（有 `_WIN32` 与 `ACS_CFITSIO_UNISTD_H` 双守卫） | **死代码**：`git grep win_compat` 在所有 CMakeLists.txt / *.cmake 中**零命中**；vendored cfitsio 自己的 CMakeLists **根本没声明任何 `target_include_directories`**（根是从 `eng/cmake/cfitsio_sources.cmake` 的显式源列表编 cfitsio 的） |
| `eng/cmake/win32_pthread_shim/unistd.h` | **几乎全空**，只有注释 + `#pragma once` + `#ifndef _WIN32 #error` | **活的**：经 `acsd_cfitsio_apply_platform_shim`（`cfitsio_platform.cmake:102-115`）注入到**全部四个编译 cfitsio 的目标**的包含面（`CMakeLists.txt:547`、`drizzle/CMakeLists.txt:81`、`hips/CMakeLists.txt:69`、`phase2_integrate/CMakeLists.txt:97`） |

⚠️ 空垫片的注释断言 cfitsio 只是**声明** `unlink`/`access` 而从不真正调用那些分支——
**这一点静态无法核实**。若任何 cfitsio TU 真的以 POSIX 拼法调用 `read`/`write`/`close`/`open`，
在 MSVC 下就是 **C4013/C2065**（不是告警，是编译错）。
**处置建议（二选一，别留着两份）**：删掉死的 `win_compat/unistd.h`，或把完整的那个提上包含面、删掉空的。
**这已是本仓第 4 例「同一物的两份实现、其中一份是死的」**（另三例见 §4.4 的 `module_loader`、
§5.9 的 `healpix_stack`、以及 `CATCH` 的 legacy 目录）。

**⚠️ 隔离是双刃的（值得写明）**：`acsd_cfitsio` 在 `/W0` 下编译 ⇒
pthread 垫片里那些 `static inline` 函数产生的任何 C4505/C4514 **全被压掉**
⇒ **该垫片对 `/W4` 零告警贡献，而 §5.6 那个"递归互斥被静默降级"的缺陷恰恰就藏在里面。**
（已核实无冲突：手工声明的 `long _InterlockedExchange(volatile long*, long);`（`pthread.h:29`）
不与任何 cfitsio TU 冲突——没有 TU 包含 `intrin.h`；只有 `swapproc.c` 拉 `tmmintrin.h`/`emmintrin.h`，
二者都不声明该内建。）

**⚠️ 关键更正：当前源码树里 `ctest` 没有任何测试可跑。**
实测（`lib/` + `eng/` 全量，来源 CMakeLists.txt / *.cmake）：

```
$ grep -rn 'add_test'            --include=CMakeLists.txt --include='*.cmake' lib/ eng/   → 0
$ grep -rn 'enable_testing|include(CTest)|gtest_discover'  同上                          → 0
```

⇒ **HEAD 上不存在任何注册进 CTest 的测试**。
⚠️ 两个直接后果：
(1) `eng/build/toolchain.ps1` 的 `ctest --preset win-rel` **无事可跑**——它会"绿"，但那是**空绿**，
**不构成任何测试通过证据**。这一点必须写进 Windows 侧交付，否则极易被读成"测试全过"。
(2) **数值等价核对没有现成的载体**。任何"双平台数值等价"结论目前**没有可执行的判据面支撑**：
既没有测试套件，也没有跨平台容差（§5.4）。若要做，只能从 CLI 侧
（`acsd normalize|mosaic|export --config … --output …`，入口 `commands.cpp`，注册 `CMakeLists.txt:1290-1294`）
对同一小输入跑两端并 diff 产物——**但"用哪份当前真实存在的配置"尚未核实，记为 UNKNOWN**。
`testdata/` 在树内（BASS_DR3 日期 JSON 自 1965 B 起），可用作输入候选，但**未验证**哪份配置能驱动完整流水线。

> ⚠️ **易踩的坑**：仓内 `build/linux-control/**/CTestTestfile.cmake` 里**仍留着一份 455 条测试清单**，
> 但那棵构建树的 `CMakeCache.txt` mtime 是 **2026-09-29**，而源码此后已大改
> （HEAD 已推进 294 提交量级）。**照那份清单排计划会全部落空**——本件一度差点引用它，已自我排除。
> **复核任何"测试名"之前，先确认它出自当前源码而不是 `build/` 产物。**

---

## 8. 前台裁决与本车道响应

交付期间前台已提交 `ea8881c`「裁决 Windows 警告处置：收紧作用域而非逐点整改，/W3 必须修」，
**对本车道报出的五项升级全部作出裁决**：

| 本车道升级 | 前台裁决 | 本车道状态 |
|---|---|---|
| 生成器口径（Ninja vs VS17） | 按 preset 走 **VS 17 2022**；不用 Ninja、不用 VS18；只有 VS18 ⇒ 记「工具链漂移」，可跑一次只为暴露警告，**但不得把 VS18 产物当「双平台全量编译通过」交付**，不得为迁就本机改 preset 或合同 | 已遵循（§2） |
| `_CRT_SECURE_NO_WARNINGS` 332 处 | **不逐点整改**（真实 API 惯例差异，纯形式改写、行为风险不为零收益为零），但**必须收紧作用域**，复用仓内目标属性级门控；**并修正仓内自称 206 的错数字注释** | 待实施（需 Windows 构建回归） |
| `/W3` 降级 | **必须删**，与全局口径对齐 | 待实施 |
| 5 目标整体关警告 | **降级为「记录并逐目标收窄」**：逐目标列出关了哪些/为什么/影响多少第一方源文件，并给收窄方案；**不接受「沿用既有设置」** | §3.2 头号发现 + §3.3 已给出收窄方案 |
| 执行器缺位是否缺陷 | **采纳本车道推翻**：规范明文要求先删后建 ⇒ 计划内时序 | 已采纳（§2 注） |
| 拒绝盲改 | **认可，不催先改** | 已遵循：未改任何源文件 |

**本车道对裁决的两点补充**（供裁决者参考，均有一手证据）：

1. **`/W3` 不是孤例，问题是「整目标 `/W0` 静音第一方」。** 裁决三要求删 `/W3`（`acsd_catalog_gaia`），
   但同一形态还存在于另外三个目标（`acsd_phase2_integrate` / `acsd_p1_drizzle` / `acsd_p1_hips_writer`，
   经 `acsd_cfitsio_isolate_warnings()` 施加 `/W0`），合计让**约 48 个第一方 TU 在两个平台上都不受检查**
   ——且**违反本仓自己的** `docs/engineering/CODE_STANDARD.md:48`「禁止 `/w`、`/W0` 一类整目标或全局降级
   来掩盖本项目源码的告警」。**只删 `/W3` 不解决这一条**，建议按 §3.2 的方案 (a) 抽独立库一并处置。
2. **「零告警」这个验收口径需要加限定。** 在那 48 个 TU 恢复严格档之前，
   MSVC `/W4` 全绿**不能**作为「本项目源码零告警」的证据。建议 G08-10 重建门禁时，
   把"第一方 TU 不得处于 `/W0`"做成可机检判据。

| 导出 / 入口 / 配置核对 | **导出面 PASS（15 个 SHARED 全有声明机制）**；**入口 FAIL（`wmain` 缺消费方，最高优先验证项）**；Windows 安装面 PASS，但有 1 处双平台皆错的功能缺陷 + 一类路径分隔符真缺陷 |

---

## 8.5 ⚠️ 给下一棒的三条「按这个顺序验」

1. **先验入口**：`acsd` 目标只定义 `wmain`（`main.cpp:82-105`）而 `_UNICODE` 全树未定义
   ⇒ 疑似 `mainCRTStartup` 找不到 `main` ⇒ **链接失败**。这是最可能让构建直接挂的一条（§4.2）。
2. **再验 zlib**：`CMakePresets.json` 的 cacheVariables **不含 `ACS_ZLIB_ROOT`**，
   必须额外用 env 或 `-D` 喂；且 `gaia_xpsd_client/CMakeLists.txt:96 find_package(ZLIB REQUIRED)`
   **无 `if(MSVC)` 保护且注册得比根的 zlib 段早** ⇒ zlib 缺失会**先**死在 gaia，
   根 `CMakeLists.txt:567` 那条 `FATAL_ERROR` 在此场景**永远轮不到**。
   （连带：`dependency-lock.json:60` 声称「无 ACS_ZLIB_ROOT 时 cfitsio 降级」**对根产品图为假**。）
   ⚠️ 而 zlib 的**权威注入方式在仓内无从查证**——其合同来源
   `.github/workflows/ci-windows.yml:49` 已被删 ⇒ 只能靠 FATDUCK 上的既有安装位置。
3. **然后才是警告面**：按 §3.2 头号发现先修 `/W0` 作用域与 `/W3`，再逐条收敛真告警。

---

## 8.6 两条会影响"复现本件结论"的实操陷阱

**(a) `.gitignore` 的 `build/` 是未锚定模式，会连带屏蔽 `eng/build/`。**
`.gitignore` 的 `build/` 写作不带前导 `/`，按 gitignore 语义它匹配**任意层级**的 `build`，
因此 `eng/build/build.sh`、`eng/build/toolchain.ps1`、`eng/build/README.md` 一并落入忽略面。
⇒ 用默认 `rg` / `grep -r`（尊重 ignore）做审计时，会**静默跳过整个 Windows 构建入口脚本**。
复核本件任何结论时应加 `--no-ignore`（并显式排除真正的产物目录）。

**(b) `eng/build/build.sh` 已失效**（不在本单授权范围，仅登记）：
`:26` 指向 `cmake -S "$REPO/lib/phase2"`，而 `git ls-files lib/phase2` → **0**（目录不存在）；
`:27` 传的 `-DP2_ENABLE_OPENMP=ON` 对应的 option 位于**不可达**的 coverage 段。
⇒ 该 Linux 入口脚本指向一个不存在的目录。**注意这不影响本单的 Windows 结论**
（Windows 走 `toolchain.ps1` + preset，不经此脚本），但任何"照 Linux 脚本复现"的建议都会失败。

**另一条实用结论**：前台 `45c9b40d` 已裁定「**不恢复执行器**」，并明示
「工具链漂移不再会被任何机制自动拦住 ⇒ 只能靠人按合同核对」。
⇒ **机器恢复后，生成器/工具集/CMake/SDK 四项必须人工核对**，`toolchain.ps1 check`
在 VS18 + MSVC 14.50 + cmake 4.1.2 的机器上**会报绿**（它只查 PATH 存在性与 4 个文件存在性，不查版本）。

---

## 9. 交接（下一棒要做的）

1. **窗口重开后立刻跑 §7.3-B**，把真实告警条数、目标数、耗时回填本件 §1、§3.1。
2. **优先处置 W1**（删 `/W3` 只留 `/Zc:__cplusplus`）——它同时消掉一条 D9025 并解除一处纪律违规。
3. **S2（332 处 C4996）需负责人裁决**，不属我可单方面处置的范围。
4. **S1（5 目标 /W0）建议把 cfitsio 源拆进独立 OBJECT 库**，只让第三方 TU 退出 `/W4`。
5. **数值等价**：待 Windows 产物就绪后，按 §5 的口径差做只读比对；不得改判据、不得建常驻门。