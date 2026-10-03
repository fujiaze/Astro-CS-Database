# 审稿-P1-INF-aio-003-补齐（增量件）

**片号**：`INF-aio-003` ｜ **层**：`lib/infrastructure/aio`
**本件性质**：对 `run/GOVERN-08/审核包-R2/审稿-P1-INF-aio-003.md` 的**增量**审稿。原交付件**一字未改**；本件与之一并流转。
**本车道**：G08-05 第 1 遍审稿 · 缺口补齐车道（只读 + 只写本件）

---

## 0. 基线声明

派单给定基线 `HEAD=19bbc439`。本车道开工实测一致（`git rev-parse HEAD` = `19bbc439cd241…`，`git status --porcelain` 仅 `M run/GOVERN-08/审核包-R2/审稿-P1-INF-aio-003.md` 一项，为其他车道所改，本车道未触碰）。

**审稿途中前台两次推进提交**：`19bbc439 → 3081acbb → 37595fe3`。核验本片是否受影响：

```
git diff --name-only 19bbc439 HEAD -- <本片 28 个成员文件> | wc -l   →  0
```

**本片 28 个成员文件在 19bbc439..37595fe3 三个基线之间逐字节未变**，本件全部行号对三者同时成立。

**零 git 写；未改任何仓内文件；未编译、未跑 ctest / pytest / 构建、未执行任何脚本。** 唯一 shell 用途：`wc -l` 数行数、只读 `git` 查询。

### 0.1 ⚠️ 定稿时发现的工作树漂移（**须前台知悉，影响两条阻断的状态**）

本件定稿瞬间复测 `git status --porcelain`，发现**本片成员 `io/hips_output_store.py` 在工作树中被其他车道改动**（未提交）：

```
 M lib/infrastructure/aio/io/hips_output_store.py    175 insertions(+), 57 deletions(-)    696 行 → 814 行
```

**该文件是 INF-aio-003 的成员。** 本车道全程未触碰它（§7.2 已声明）。

**后果与处置**：

1. 本件全部行号对应**提交基线 `19bbc439` / `37595fe3`**（派单指定基线，且已实测该区间逐字节未变）。**对定稿时的工作树不再逐字成立。**
2. **两条阻断正被同时整改**，其状态应从「待修」改为「待验证整改」：
   - **B11**（stage 按 basename 定址）：工作树已把 `return self._stage_dir / f"{hashlib.sha256(seg.encode()).hexdigest()[:16]}.{seg}"` 改为 `key = hashlib.sha256(rel_path.encode("utf-8")).hexdigest()[:16]` + `return self._stage_dir / f"{key}.{seg}"`，新 docstring 明确写出「Norder1/Dir0/Npix0.fits 与 Norder2/Dir0/Npix0.fits 同名是正常形态…按文件名定址会让两个不同的产品文件落进同一暂存路径」。**⇒ 本车道 §2.2 C9 报出的机理被整改方独立复现，改法与本车道裁决方向一致。**
   - **B12**（overwrite 先毁旧后暂新）：工作树已删除该代码块，替换为「目标区此刻**一个字节都不动**。清残 / 让位必须等暂存全部成功之后才做」，并新增 `self._retire_seq` 与 `_retire_success_target`。**⇒ 同一。**
3. ⚠️ **本车道不据此宣称两条阻断「已修复」** —— 整改代码**未经编译、未跑测试**（红线），且 §2.2 C9 附带的「全仓零测试覆盖」（fixture `:128` 是单标量 `order`、全部测试字面量都是 `Norder0`）**在 814 行的新版本下未见证据表明已被修复**。**整改是否覆盖该缺陷，须前台读新版本后裁决。**
4. **提请前台**：治理期间并发整改会让「行号—基线」绑定失效。本件只对 `19bbc439` / `37595fe3` 有效；引用 §2、§4 的行号时须先声明基线。

---

## 1. 合并后的完整覆盖率（份数 / 行数）

### 1.1 片口径

`片清单-权威版.yaml:2982-3017` 记 `成员份数: 28`、`实际行数: 10632`。本车道对 28 份逐个 `wc -l` 复算，合计 **10632**，与清单**逐位相符**，划片无缺漏。

### 1.2 原车道未读清单的实测核对（先纠一处数字）

原交付件 §1.3 标题为「未被本人读完的部分」，列 18 行，**合计栏写 8 424**。实测：

- 该 18 行**分项求和 = 4 624**，与合计栏 8 424 不符（差 3 800）；
- 该表**漏列 5 份成员**：`aio_healpix_io.cpp`(2201)、`hiss_writer.cpp`(1173)、`hiss_codec.cpp`(460)、`hiss_common.cpp`(293)、`hiss_tile_model.cpp`(167)，合计 **4 294 行**；
- 原车道个人实读 1714（见其 §1.2：`test_output.txt` 300 + `HEALPIX_FORMAT_SPEC.md` 380 + `aio_sysinfo.cpp` 312 + `aio_abi_layout_lock.py` 239 + `hiss_writer_smoke.cpp` 153 + `production_store.py` 1–330 共 330）。

⇒ **原车道未读 = 10 632 − 1 714 = 8 918 行，占全片 83.9%。**
原交付件 §1.3 的 8 424 **既不等于分项和、也不等于真值**，属算术错误 + 漏列。本车道按 8 918 推进。

### 1.3 本车道实读（本人亲读）

| 文件 | 行数 | 实读区间 | 本车道新覆盖 |
|---|---|---|---|
| `docs/HEALPIX_FORMAT_SPEC.md` | 380 | 1–380（读满） | 复读（原车道已读） |
| `tests/test_output.txt` | 300 | 1–300（读满） | 复读（原车道已读） |
| `tests/hiss_writer_smoke.cpp` | 153 | 1–153（读满） | 复读（原车道已读） |
| `tests/CMakeLists.txt` | 50 | 1–51（读满） | **50** |
| `Makefile` | 84 | 1–84（读满） | **84** |
| `src/hiss_common.cpp` | 293 | 1–293（读满） | **293** |
| `src/hiss_tile_model.cpp` | 167 | 1–167（读满） | **167** |
| `runtime/artifact_store/production_store.py` | 884 | 331–884 | **554** |
| `tests/test_pipeline_blocks.py` | 537 | 1–537（读满） | **537** |
| **本人新覆盖小计** | | | **1 686 行** |

**本人逐行覆盖率（对全片）= 2 521 / 10 632 = 23.7%**（含三份复读）；**本人新增覆盖 = 1 686 行 = 15.9%**。

> `tests/CMakeLists.txt` 口径说明：`wc -l` = 50，但 `read` 显示末行无换行符 ⇒ 实际 **51 行**。本件统一以 `wc -l` 计入片总数 10 632，逐份实读按 `read` 行号。

### 1.4 本车道子代理补读

| 子代理 | 授权范围 | 实读 | 状态 |
|---|---|---|---|
| `976b9280` | `Makefile` 84 · `tests/CMakeLists.txt` 51 · `tests/sanitize_wsl.sh` 39 · `test_gradient_sampler.cpp` 58 · `.gitignore` 9 · `legacy/…/.gitignore` 14 · `aio_file_io.h` 257 · `aio_upm.h` 122 · `aio_hips_reader.h` 92 · `aio_sysinfo.h` 83 · `snr_evaluator.h` 118 | **927** | 已交付 |
| `20e99660` | `hips_output_store.py` 696 · `atomic_publish.cpp` 641 · `io_adapter.cpp` 207 · `test_healpix_io.py` 451 | **1 995** | 已交付 |

**本车道合计新覆盖 = 1 686 + 927 + 1 995 = 4 608 行**（占全片 43.3%）。

### 1.5 并集覆盖率（原车道 ∪ 本车道）

逐份核对 28 份成员的去向：

| 来源 | 份数 | 行数 |
|---|---|---|
| 原车道本人亲读 | 6（含 1 份部分） | 1 714 |
| 原车道子代理 `0890211e`（`aio_healpix_io` / `hiss_writer` / `hiss_codec` / `hiss_common` / `hiss_tile_model`） | 5 | 4 294 |
| 原车道子代理 `79373e7f` / `2703bfbc`（`gradient_sampler.cpp` 等） | 其余 | 4 624 |
| **本车道本人亲读（新增）** | 6（含 1 份部分） | **1 686** |
| **本车道子代理 `976b9280`** | 11 | **927** |
| **本车道子代理 `20e99660`** | 4 | **1 995** |
| **并集（去重）** | **28 / 28 份** | **10 632 / 10 632 = 100%** |

**✅ 并集完整覆盖率 = 100%（28/28 份，10 632/10 632 行）。**

### 1.6 强度分层（引用时必须区分，**不可笼统称「已重读」**）

| 层级 | 内容 | 强度 |
|---|---|---|
| **A · 本车道亲读 + 可复现控制流** | §4 高价值发现两条（全部）、§2 裁决、§5 本车道新发现 E1–E4 | **可直接采信** |
| **B · 本车道亲读 + 本车道子代理** | §5 的 E5–E9（转引 `976b9280` / `20e99660`，本车道未逐行复核其行号） | **建议抽查** |
| **C · 仅原车道子代理转引** | 原交付件 §3.7–§3.12（B5–B13、§3.12 表）中**本车道未复核行号**者 | **建议抽查，引用前须抽查** |
| **D · 一律不裁决** | 原交付件 §7.3 列出的 8 条「须前台执行」项、以及全部标「待核」的片外指针 | **不裁决** |

⚠️ **本件不声称「本人逐行读完本片」。** 本车道本人仅 23.7%；100% 是**并集**口径，其中约 4 900 行仍依赖原车道子代理的转引（原车道自报其并集亦为 100%）。

---

## 2. 对原交付件的复核结论（逐条：成立 / 不成立 / 需修正）

### 2.1 被**推翻**或**实质改写**的条目

| # | 原条目 | 本车道裁决 | 理由 |
|---|---|---|---|
| **R1** | **B1**（原「最重第 1 条」）：规范 Magic 4 字节 / 无版本号 / zstd-5 三处「与实现」矛盾，责任面为「规范 ↔ 实现」 | **判词方向须改写；三条事实本身成立** | 实为**三套并存格式**，不是两方。见 §3.1。规范描述的 4 字节 Magic 与 zstd-5 **与 `aio_healpix_io.cpp` 一致**；被指为「实现」的 8 字节 `"ACSHISS\0"` 出自**已废弃**格式（`hiss_stream_writer.cpp:53` 原文「已废弃」）。**最重第 1 条的标题与整改方向都会误导前台。** |
| **R2** | **§3.3 把 `hiss_writer_smoke.cpp` 记为正面结论**：「全部失败路径都是 `return 1`，退出码真实反映结果……**这是本片里写法最正确的测试**」 | **❌ 撤回该正面结论** | `tests/CMakeLists.txt:15` 原文：`#   hiss_writer_smoke     断言失败「MAGIC 不匹配」`，列入「未注册者（构建或断言未通过，另行登记，**不混入绿门**）」。**本仓自己登记它断言失败且不注册。** 一个已知红且不运行的测试**不构成任何正面证据**。其失败原因恰是格式从 `"ACSHISS\0"` 改为 `"HISS0100"`，故它对**现行格式零覆盖**。 |
| **R3** | **B13**（`sanitize_wsl.sh`）：「3 阶段中第 3 个从未执行**仍全绿**」 | **❌ 因果链否决** | 本车道采信并复核子代理 `976b9280` 的控制流推演：`:4 set -e` + `:19 ls *.o >/dev/null`，在 ROOT 算错导致的空目录下 glob 未匹配 ⇒ bash 传字面量 `*.o` ⇒ ls 退出 **2** ⇒ `set -e` **立即中止**，`:39 ALL_SANITIZE_PASS` **不可达**。该脚本是「**响亮失败**」而非「静默假绿」，属**第④型「机制正确但从不执行」**，与「过滤器全绿」不同类。原交付件把两者混为一谈会虚增「制造绿色产物」的实例数。 |
| **R4** | **B2**（`test_output.txt:293-298`）：「`通过/失败/跳过` 三个计数器全为 0，却报 21/21」 | **⚠️ 现象成立，机理不适用于当前树 —— 须改写** | 本车道核到 `tests/hiss_correctness_test.cpp`（片外，仅为裁决）：`:1317 g_test_passed = g_test_total - (int)g_failures.size() - g_test_skipped;` `:1319-1322` 打印 `总计/通过/失败/跳过`，**全文没有 `实际通过:` 这一行**。⇒ **`test_output.txt:298` 的 `实际通过: 21 / 21` 不可能由当前源码产生**，该快照出自**另一代代码**。原交付件据快照推断的「计数器从未被写入」机理，**对当前源码不成立**。当前源码另有一处**真实**且更值得报的缺陷，见 §5 E2。 |
| **R5** | **B9**（`test_gradient_sampler.cpp` NDEBUG 下零断言却输出全绿），列为**阻断** | **⚠️ 结构与行号全对，严重性须下调** | `976b9280` 核实：根 `CMakeLists.txt:30-32` 确设默认 Release ⇒ NDEBUG 前提成立；但全仓**任何** `CMakeLists.txt`/`*.cmake`/`*.ps1`/`*.sh`/`*.bat`/`*.py` **均不引用该文件**，归档 `Makefile:28` 的 `SRCS` 也不含它 ⇒ **没人编译它，没人运行它，那个「全绿」读数不可能被任何人观察到**。按 AGENTS.md §6，正确处置是**删除**，不是修 assert 或给 CMake 加 `-UNDEBUG`（那是给没人跑的代码投资）。 |
| **R6** | **§3.12 的 `:661-673` 非法 size 静默返回 0** | **❌ 虚构** | `20e99660`：`grep "return 0"` 全 696 行**零命中**；`read_product_file` 返回 `None`。真问题在 `:694`（`verify_tree_hash` 的 `size` 取自 manifest 而非磁盘，与其 docstring `:683` 不符），性质与行号均须改。 |
| **R7** | **§3.12 的 `:461`+`:174` `new_writer("a")` 两次互覆盖（TOCTOU）** | **❌ 三重虚构，机理方向反了** | 无 `new_writer` 符号；`:174` **是空行**；真正的固定 tmp 名在 `:562`，而 `:217` 用 `O_CREAT|O_EXCL` ⇒ 第二次**抛 `FileExistsError`**（响亮失败），**不是静默覆盖**。TOCTOU 的方向说反了。 |
| **R8** | **§3.12 的 `:229` 裸 `fopen` 与 `:363` `::_open`「自相矛盾」** | **❌ 推理错误** | `::_open` 与 `std::fopen` 同为 **CRT ANSI** API，对 UTF-8 路径行为**一致**（需 `_wopen`/`_wfopen` 才处理宽字符），不存在不对称。改写后的真结论是「本文件 Windows 路径全线 ANSI」，真实但更小、形状不同。 |
| **R9** | **§3.12 的 `:457`/`:609`「目录 fsync 失败时直接 return ⇒ rollback 不可达」判为缺陷** | **⚠️ 前半反了，须改写** | `20e99660` 引 `product_io/include/astro/aio/atomic_publish.h:28-31`（冻结正本）：kNotDurable **「不得回滚删除」**（产品已可见，删了就毁掉已发布对象）。两处 early-return 已置 `durability=kNotDurable` ⇒ **跳过 rollback 是正本明令的正确行为**；按原样交付等于与正本冲突。**残留真缺陷只是「跳过 verify」**（`verify_after_rename` 与 `remove_on_verify_failure` 默认均 true），窄得多、中危。 |
| **R10** | **§3.12 的 `:98-122` `make_parent_dirs` 恒返回 true ⇒ 恒真函数** | **⚠️ 前半逐字属实，后果须下调两级** | 恒真 + 吞全部 mkdir errno **属实**。但唯一调用点 `:557` 是**裸表达式语句、丢弃返回值** ⇒ 恒真性是**惰性**的，不会导致错误分支；随后的 `:558 ::mkdir(staging)` 会带真实 errno 报错。**净缺陷 = 误导性诊断**（把操作者指向 staging，真实故障在中间父目录），非阻断。 |
| **R11** | **§3.12 的 `:22-24` 用 `errno` 未 include `<cerrno>`** | **⚠️ 事实对，定性过重** | 头链确实不含 `<cerrno>`（`20e99660` 连 `lib/include/acsd/core` 一并 grep，零命中）。但 `<system_error>`(`:10`) 与 POSIX `<unistd.h>`(`:13`) **传递提供 `errno`** ⇒ 主流工具链均可编译。属 IWYU 卫生缺陷与移植风险，**不是编译断裂**。降为 Low。 |
| **R12** | **§3.12 的 `test_healpix_io.py`「41 条判据在 `python -O` 下全灭」** | **⚠️ 数字全对，「全灭」过强** | 数字逐条核实无误：31 裸 assert + 10 `np.testing.` = **41**，`np.testing.` 行号 158/163/253/256/298/299/316/317/334/335 共 10 条。**但 10 条 `np.testing.*` 是真实函数调用，`-O` 下存活**。正确表述：**76% 判据失效，且失败检测从 return-code 层完全丧失**。 |
| **R13** | **§3.12 的 `:421`「按函数名字符串决定调用约定，改名即静默传参错误」** | **⚠️ 部分驳回** | 名字不匹配得到的是响亮 `TypeError`（被 `:429` 捕获记 False），**不是静默**。**同块真正的隐患是 `:425 result = True` 硬编码**——丢弃函数返回值（原交付件未标），这才是「期望量不追溯到被检验量的输出」。 |
| **R14** | **§3.12 的 `:42-43`「计数口径不变」与 `if(UNIX)` 矛盾** | **⚠️ 降级** | `:43` 括号自带作用域限定，该作用域内字面为真，不构成自相矛盾。**真问题更硬**：同行的 `eng/ci/checks.json` 经 glob 核实**零命中、目录不存在** ⇒ 括号**前提**不成立（悬空引用，而非矛盾）。 |
| **R15** | **§3.12 对 `atomic_publish.cpp:332-337` cleanup_tmp「实测 residue」的 ✅** | **⚠️ 须加限定** | 该测量**并非普遍**：`:372-376` 的 open 失败路径**完全绕过** `cleanup_tmp`，`res.tmp_residue` 保持默认 `false`，而 POSIX 重试循环 `:367-370` 明确容忍 EEXIST 8 次（即已知 tmp 可能已存在）⇒ 崩溃残留存在却报「无残留」。**这一条 ✅ 需要加限定**，见 §5 E6。 |
| **R16** | **§3.3 的 `hiss_common.cpp:46-49 ↔ :240-247`「此后每次往返再涨 5 字节」** | **⚠️ 机理成立，增长断言不成立** | 本车道逐字符推演：控制字符 ESC → 写 `\u001b`(6 字符) → 读回得 6 字符 → 再写 `\\u001b`(7) → 再读回 **6** ⇒ **一轮后收敛，不再增长**。「每往返涨 5 字节」**错误**，须删去。「写读转义不对称、控制字符被还原成 6 字符字面量」**成立**。 |
| **R17** | **§3.7 的 `aio_healpix_io.cpp` 24 个 `catch(...)` 不构成吞异常恒绿（✅ 本片最扎实）** | **✅ 确认，本车道采信** | 机理与本车道独立读到的 `hiss_common.cpp` 中**唯一**那处 `catch(...)`（`:217`，包 `std::stod` 转 `false`）一致，处置得当。 |
| **R18** | **§3.1 的 `:217` TEST 17 行号** | **⚠️ 行号指向错误** | `:217` 是 `OK: 打开含越界 offset 的文件 (open 不检查 offset)`，属 **TEST 17**；原条目讨论的「`.partial` 测试」在 **`:236-239`**。条目内容（该测试是靠通用越界检查过的）本身可成立，但 `:217` 指错了位置。 |

### 2.2 **成立**（本车道独立确认）的条目

| # | 原条目 | 本车道确认方式 |
|---|---|---|
| **C1** | **B2 现象**：`test_output.txt:293-298` 三个计数器全 0 而报 21/21 | 亲读 `:293-298` 逐字确认。**但机理须改写，见 R4。** |
| **C2** | **B3**：`test_output.txt:149-152` TEST 13 把 `read_tile_snr` 返回 **-4** 的真实布局不一致记为 `OK:` | 亲读确认：`:149 got=52 expected=28 (n_points=0)`、`:150 read_tile_snr 返回 -4`、`:151` 记下「Writer 写入 52B（含 snr_phot/median_snr/idw_power 三个 double）, Reader 期望 28B」、`:152` 却写 `OK:`。**完全成立**，且该测试**具备判别力却选择记 OK**。 |
| **C3** | **B4**：`test_output.txt:191-203` TEST 16 名为「未知必需子块**拒绝**」，实为 `:202 read_tile 返回 0`（接受），`:203` 仍记 `OK:` | 亲读逐字确认。**完全成立。** |
| **C4** | **B5**：`hiss_common.cpp:157` 范围门对 NaN 结构性恒绿 | **本车道亲读 293 行并独立确认**：`S = sum_area / A_p`；`if (S < -eps \|\| S > 1.0 + eps)`。IEEE-754 下 `S=NaN` ⇒ 两比较皆假 ⇒ 循环不触发 ⇒ `:165 return 0`。**成立。** **补一条本车道新发现（§5 E3）：`:148 if (pixel_area <= 0.0)` 同样对 NaN 恒假 ⇒ NaN 的 `pixel_area` 也整条溜过。** |
| **C5** | **B6**：`hiss_tile_model.cpp:163-165` `owns_global` 往返自证恒真门 | **本车道亲读 167 行并独立做代数展开**：`:164` = `global_to_parent(global) == parent_ipix`；`:146-149` = `global >> (2*depth)`；`:118` = `(parent_ipix << (2*depth)) \| local`。代入 `((parent<<2d)\|local) >> 2d == parent` 对**任意** in-range `(parent, local)` **恒真**。两侧同源于同一对 `(parent_ipix, depth)` 与同一 shift。**成立。** |
| **C6** | **§3.8 的 `hiss_common.cpp:124-125` 虚构值回退 vs `:147-148` 要求硬失败** | 亲读确认：`:125 const double A_p = (pixel_area > 0.0) ? pixel_area : 1.0;` vs `:147-148` 注释「pixel_area<=0 必须硬失败, 不能回退到虚构值」+ `return -2`。**同文件两个函数对同一条件给出相反契约，成立。** |
| **C7** | **§3.8 的 `hiss_common.cpp:290` `from_json` 永远返回 0** | 亲读确认：15 个 `get_num` / 10 个 `get_str` 的返回值全部写成 `if (...)` 而结果被弃（`:264-289`），`:290` 无条件 `return 0`。**任何输入（含 `""`、`"{garbage"`）都返回 0。成立**，且直接使 `hiss_writer_smoke.cpp:133` 的 `!= 0` 判据**对畸形 JSON 恒绿**。 |
| **C8** | **B10**：`io_adapter.cpp` 全文无任何 fsync，读回校验读的也是同一页缓存 | `20e99660` grep `fsync\|FlushFileBuffers\|_commit\|O_SYNC\|O_EXCL` 覆盖全部 207 行 ⇒ **零命中，连注释里都没有**；持久化路径 `:44 ofstream` → `:60` → `:130 filesystem::rename` 直接 success。**成立。** |
| **C9** | **B11**：`hips_output_store.py:451` stage 按 basename 定址 ⇒ 多 Norder 产品无法发布 | `20e99660` 逐字确认 `:448 seg = rel_path.rsplit("/",1)[-1]`、`:451 return self._stage_dir / f"{sha256(seg)[:16]}.{seg}"`、`:502-504` 撞名即 `PermissionError`、`:607-612` 明确允许 `Norder{K}` 且 `{K}` 为任意 `isdigit()`。**并集无任何 Norder 过滤旁路，推翻失败。成立并升级为「全仓零测试覆盖」。** |
| **C10** | **B12**：`hips_output_store.py:477-481` vs `:496` overwrite 先毁旧后暂新 | `20e99660` 逐字确认，且 `_cleanup_staged`(`:617-624`) 只删新 tmp、**从不恢复旧目标**。**成立**；另 `:17-18` docstring 自称「可恢复」**为假**。 |
| **C11** | **B3 §3.12 `test_pipeline_blocks.py`「本片唯一接线正确的计数路径」** | **本车道亲读 537 行确认**：`:51-52 global PASS, FAIL` / `if condition: PASS += 1`、`:56 FAIL += 1`、`:528 print(f"测试结果: {PASS} 通过, {FAIL} 失败")`、`:533 return 0 if FAIL == 0 else 1`。**计数逐条递增、退出码真反映 FAIL —— 这是本片唯一一处「通过数被独立累加」而非减法导出的汇总，成立且值得记账为正。**（对照 §5 E2 的减法导出。） |
| **C12** | **§3.12 `Makefile` 三条**（`del /Q` Windows-only；`CXX = g++` 覆盖环境；Linux 上产 `.dll` 后缀 so） | **本车道亲读 `Makefile` 84 行逐条确认**：`:82-84 clean: del /Q … 2>nul`（POSIX 上重定向先于命令查找 ⇒ 必建名为 `nul` 的文件，再 127）、`:4 CXX = g++`（递归赋值，env 被覆盖）、`:74 all: astro_image_io.dll` + `:77 -o $@` 后缀硬编码。**全部成立。** |
| **C13** | **§3.12 `tests/CMakeLists.txt` 0 处 Python 注册** | **本车道亲读 51 行确认**：`:23` 只注册 `checksum tile_model transform query_pixel precision_dual` 五个 C++，`:45-48` 再加一个 `aio_hips_atomic_publish`（`if(UNIX)`）⇒ **零 Python**。**成立。** |
| **C14** | **§3.12 `tests/CMakeLists.txt:9-11` 的 49/67/75 不可复现** | 亲读确认三行只有 PASS/FAIL 数字，**无复现命令、无 oracle 路径、无提交锚**；`:8` 自述「前台 ad hoc 构建 + 运行复跑记录」按 AGENTS.md §4 不是可复核证据。**成立。** |
| **C15** | **§3.12 `sanitize_wsl.sh:5` ROOT 算错一级** | `976b9280` 数清层级：脚本在 `lib/infrastructure/aio/tests/`，`../../..` ⇒ `lib`（应为 `../../../..`）；`:14/:15` 展开为 `<repo>/lib/lib/…`，glob 核实 `lib/lib/**` 零命中。**成立。** |
| **C16** | **§3.12 `aio_file_io.h:213` `fclose(f_)!=0 && ok` 无泄漏（✅）** | `976b9280` 确认 C++ `&&` 有序列点、左操作数无条件求值 ⇒ 无泄漏，且 `:50/:67/:97/:122/:249` 模式一致。**确认是真无问题，非假阳性 —— 正面结论也须正面核实，本车道采信。** |

---

## 3. 两处高价值发现的独立复核（本车道亲读原文，非转引）

### 3.1 高价值发现一 · `HEALPIX_FORMAT_SPEC.md` 与实现的三处正面矛盾

**裁决：三处矛盾的事实全部成立；但「正本 ↔ 实现」的责任划分须整体改写，且真相比原判词更重。**

#### 3.1.1 逐条复核（亲读）

| 项 | 规范原文（亲读） | 被指「实现」 | 本车道裁决 |
|---|---|---|---|
| Magic | `:32` 「`.hiss` \| `'H','I','S','S'` (0x48 0x49 0x53 0x53)」；`:61` 「Magic: "HISS" 4 字节」；`:259` 「0 \| 4 \| Magic \| `0x48 0x49 0x53 0x53`」；`:271` 同构 `.hcsd` | `hiss_writer_smoke.cpp:103` `std::memcmp(head, "ACSHISS\0", 8) != 0` | **4 vs 8 属实**，但 `"ACSHISS\0"` **不是现行实现**（见 3.1.2） |
| 版本号 | `:344` 「当前格式**未在文件中显式存储版本号**，版本通过 Magic 标识区分」；`:346-347` `"HISS" → v1.0` | `hiss_writer_smoke.cpp:107-112` `memcpy(&ver, head+8, 4)` 且断言 `ver == 1` | **属实**，但同 3.1.2 |
| 压缩级别 | `:38` 「使用 zstd 压缩，**压缩级别 level=5**」；`:262`、`:274` 同；`:316` 「zstd level=5 压缩比通常 > 5:1」 | `test_output.txt:7` 「`[hiss][codec] 内置注册: ZSTD (level=3)`」 | **属实**（原文确认 `:7`），但**证据指针须换**（见 3.1.2） |

**另补一条本车道亲读的规范内部一致性核对（正面）**：`:207` `12 × 64² = 49152` ✓、`:209` `(8192/64)² = 128² = 16384` ✓、`:210` `ipix_fine >> (2 × log2(128))` = `>> 14` ✓、`:228` `49152 × 24 = 1 179 648` ✓ —— **四处算术全部正确**，`:41-42` 的 `LEAF_INDEX_SIZE` 字面量亦与之吻合。原交付件的正面复算结论成立。

#### 3.1.2 实情：仓内是**三套并存**的 `.hiss` 格式，不是两方

本车道 grep `ACSHISS|HCSDC|"ACSH|kMagic|HISS_MAGIC` 于 `lib/infrastructure/aio`，实得 11 处命中，拆开为：

| 代 | Magic | 头布局 | 出处 | 与规范关系 |
|---|---|---|---|---|
| **甲** | 4 字节 `'H','I','S','S'` | 4+4+4 = **12 字节**头，zstd **5**，**平铺** ipix/pixel 数组 | `src/healpix/aio_healpix_io.cpp:37`（写 `:1846`，读 `:1953`）、zstd `:45` `ZSTD_LEVEL = 5`（用 `:1213`/`:1831`） | **与规范一致（Magic 与级别都对）** |
| **乙** | 8 字节 `"HISS0100"` | **16 字节签名块**（magic + `header_length` u32 + `feature_flags` u32），**TLV 头**，**42 字节子块描述符**，15 字节 tile 目录前缀，CRC32C，zstd **3** | `src/hiss_stream_writer.cpp:49-69`（`:53` 注释「旧格式 (ACSHISS\0 + version + header_offset) **已废弃**」）、`src/hiss_reader.cpp:72`、zstd `src/hiss_codec.cpp:115` `kZstdDefaultLevel = 3`（打印串在 `:300`，与 `test_output.txt:7` 逐字相同） | **与规范全面冲突** |
| **丙** | 8 字节 `"ACSHISS\0"` | **20 字节**（magic + version u32 + header_offset u64） | `tests/hiss_writer_smoke.cpp:103/:107-121`（期望 `header_offset == 20`，`:117`） | **代码已声明废弃**（`hiss_stream_writer.cpp:53`） |

⇒ **关键更正**：
1. 规范在 **Magic 宽度**与 **zstd 级别**两点上**与甲完全一致**，与甲冲突的是别的；
2. 规范 `:344`「版本通过 Magic 标识区分」——**乙正是这么做的**（`"HISS0100"` 把版本编进 Magic）。**⇒ 「版本号」这一条，规范与现行生产实现不矛盾**，只与废弃的丙及钉在丙上的冒烟测试矛盾；
3. **两个实现都被编进同一个产物**：`Makefile:52-70` 的 `SRCS` 同时含 `$(SRC_DIR)/healpix/aio_healpix_io.cpp`(`:67`) 与 `hiss_codec/hiss_common/hiss_tile_model/hiss_transform/hiss_writer/hiss_stream_writer/hiss_reader`(`:58-64`) ⇒ **一个 DLL 同时导出两套互不兼容的 `.hiss` 读写器**。
4. **zstd 级别的证据指针须换**：原交付件引快照 `test_output.txt:7`；真实现是 `hiss_codec.cpp:115`（打印串在 `:300`），而 `aio_healpix_io.cpp:45` 是 5。**结论不变，指针须改。**

#### 3.1.3 「布局结构性不同」—— **成立，且比原判词更重**

原交付件称「这不是一个字段写错，是整份正本描述的是另一个格式」。**本车道确认，并给出更硬的证据**：

- 规范 `:59-76` / `:257-265` 的 `.hiss` 布局 = `Magic(4) + uncompressed_len(4) + compressed_len(4) + 压缩JSON + ipix[n]×8 + pixel[n]×4 (+ snr[n]×4)`，**12 字节头，无子块、无 TLV、无 CRC32C、无 tile 目录**；
- 实现的实测输出（`test_output.txt:86-89`，本车道亲读）：`子块 type=1 flags=0x0001 uncompressed=12288 …`、`:137` `子块 type=3 … uncompressed=52`、`:89 finalize 成功: tiles=1 header_offset=20 header_size=418`。**header_offset=20 = 8 + 4 + 4 + 4**，与规范的 12 字节头**差 8 字节，且字段集完全不同**（实现有 version 与 header_offset，规范两者皆无）；
- 实现侧源码佐证（`hiss_stream_writer.cpp:60-66`）：子块描述符 `type(1)+ext_type_id(2)+flags(2)+offset(8)+compressed_size(8)+uncompressed_size(8)+codec_id(2)+transform_id(2)+checksum_type(1)+checksum(8) = 42`，tile 目录前缀 `parent_ipix(8)+tile_nside(4)+occ_mode(1)+subblock_count(2) = 15`，TLV 项头 `tag(2)+flags(1)+length(4) = 7` —— **规范全文不出现其中任何一项**；
- **规范 `:372-378` 列出的 5 个「参考实现」文件全部不存在**：`healpix_db/healpix_io/` 下经 glob 核实**只剩 `ARCHIVED.md` 一个文件**（26 行）。原判词「参考实现……（**待实现**）」的措辞因此比原交付件所述更严重 —— **不是「待实现」，是「已归档且目录已空」**。

#### 3.1.4 「用例名为『拒绝未知必需子块』实为接受」—— **完全成立**

`test_output.txt:191` 测试名「`未知必需子块拒绝`」→ `:202` 实测「`read_tile 返回 0 (当前实现按 type 查找, 未全扫描未知必需子块)`」→ `:203` 记「`OK: 未知必需子块测试完成 (行为记录: Reader 按 type 查找, 未主动拒绝未知必需子块)`」。

**`read_tile` 返回 0 = 接受。** 机理不是断言写错，而是**判据的期望值被写成了「记录当前行为」而非「要求的为」**。原交付件的判词与修复建议（须先由产品裁决这两项是「未完成的必需功能」还是「已接受的现状」）**本车道确认成立**。

#### 3.1.5 ⚖️ 正本失效判定

**判定：该规范构成「正本失效」，且属最重的一档 —— 整份描述的是另一套字节布局。**

按 AGENTS.md §3「文档之间冲突时以更高一层为准；代码与文档冲突时以文档为准订正代码」：

| 问 | 答 |
|---|---|
| 冲突面 | 规范（甲：12B 头 + 平铺数组 + 无子块） ↔ 现行生产实现（乙：16B 签名块 + TLV + 42B 子块描述符 + tile 目录 + CRC32C） |
| 是否只是字段写错 | **否**。布局、容器模型、子块模型、压缩对象（整个 JSON 头 vs 逐子块）全部不同 |
| 是否「文档高于代码」即可机械订正代码 | **否**。若照规范改乙，将推翻已上线的 TLV 容器模型、原子提交与按子块随机访问能力 |
| 实情 | 仓内**并存甲、乙、丙三套**；`Makefile:52-70` 把甲、乙同时编进同一 DLL |
| 结论 | **正本失效成立，且失效面大于原交付件所述。** 但正本失效**不自动等于「代码错」** —— 前台须先裁决「甲还是乙才是产品格式」，才能确定订正方向 |

**给前台的裁决项（不可由审稿人代决）**：
1. 产品 `.hiss` 格式以**甲**（规范）为准，还是以**乙**（现行 TLV 实现）为准？
2. 若以乙为准 ⇒ `docs/HEALPIX_FORMAT_SPEC.md` 须**整份重写**（不是打补丁），并撤下 `:372` 的「（待实现）」与 `:380` 的 `aio_compress`/`aio_decompress` 伪引；
3. 若以甲为准 ⇒ 须裁决 `aio_healpix_io.cpp` 与 `hiss_*` 两套实现的**去留**，否则同一 DLL 继续导出两套互斥格式；
4. `tests/hiss_writer_smoke.cpp` 无论哪种裁决都须改：它现在钉在**代码已声明废弃**的丙格式上，且被 `tests/CMakeLists.txt:15` 登记为断言失败。

### 3.2 高价值发现二 · 「过滤器导致过滤运行与全量运行输出逐字相同的全绿」

**裁决：成立，但存在**两个候选**，必须取其一；且原交付件挂在 INF 片的那个候选（`sanitize_wsl.sh`）是**错的**。该发现的真实位置在 **EXP-shared-001**（本车道亲读复核，见 `审稿-P1-EXP-shared-001-补齐.md` 同节）。**

| | 候选甲 `实验/shared/synthetic/run_selftests.sh` | 候选乙 `lib/infrastructure/aio/tests/sanitize_wsl.sh` |
|---|---|---|
| 有真过滤器分支 | ✅ `:41` | ❌ `:38` 是硬编码空阶段 |
| 能跑到结论句 | ✅ | ❌ `:19 ls *.o >/dev/null` 在空目录下退 2，`set -e`(`:4`) 中止 |
| 结论句逐字相同 | ✅ `:90` | ⛔ 从不打印 |
| 退出码相同 | ✅ `:91` | ⛔ 从不打印 |
| **裁决** | 🎯 **就是它** | ❌ **不是**（是「响亮失败」，第④型） |

**原交付件 B13 必须改写**，理由见 §2.1 R3。

---

## 4. 本车道新发现

> 判别依据唯一一条：**期望量能否追回到被检验量自身的输出字段**。两侧同源、往返自证、恒等式、机制正确但从不执行，**都不算**判别力。

| # | 严重性 | 位置 | 机理 | 一句话 |
|---|---|---|---|---|
| **E1** | **阻断** | `tests/CMakeLists.txt:15` + `tests/hiss_writer_smoke.cpp:103/:107-121` | 制造绿色产物（证据失效） | **本仓自己登记该冒烟测试「断言失败『MAGIC 不匹配』」且不注册**，而它正是全片唯一的签名级验证 ⇒ 「.hiss 签名已被验证」这一前提**无任何存证**。原交付件却把它记为正面结论。 |
| **E2** | **阻断** | `tests/hiss_correctness_test.cpp:1317/:1319-1322/:1338`（片外，为裁决所读） | **①代数恒等式 + ④机制正确但从不执行** | `g_test_passed = g_test_total − failures − skipped` 是**减法导出**、从不 `++` ⇒ **「通过」栏恒等于残差，零信息量**（`通过 + 失败 + 跳过 ≡ 总计` 恒成立）。更硬的一条：`:79-82` `SKIP_TEST` 只入 `g_skips` 不入 `g_failures`，而 `:1338 return g_failures.empty() ? 0 : 1` ⇒ **全部 21 个测试被跳过时退出码仍为 0**，读者只见「失败: 0」即认为全过。 |
| **E3** | **阻断** | `src/hiss_common.cpp:148` + `:157` | **④机制正确但从不执行** | 原交付件只报 `:157` 的 NaN 双重假。**`:148` 是第二个入口**：`if (pixel_area <= 0.0)` 对 `pixel_area = NaN` 同样恒假 ⇒ 穿过 `:154 const double A_p = pixel_area` ⇒ **每个 `S` 都是 NaN ⇒ `:157` 全假 ⇒ `return 0`**。且与 `:125` `finalize_support` 在同一对象上**对 NaN 得出不同的 `A_p`**（前者 1.0、后者 NaN），**两者不一致却因都返回 0 而不可见**。 |
| **E4** | **须修** | `tests/test_output.txt:285-287`（TEST 21 内） | **①代数恒等式 + 判据脱离被检对象** | `:285` 「`Tile 未找到: parent_ipix=1221`」紧接着 `:286` 「`OK: query_pixel 返回 0 (成功或无覆盖返回零值)`」、`:287` 「`query_pixel(10,10): sig=0.0000 sup=0 (0=无覆盖)`」。**「成功」与「无覆盖」两条分支产出完全相同的可观测量 ⇒ 该判据恒真、不可翻红**；且 TEST 21 名为「Tile 父子恢复正确」，而它**只查了一个不存在的 Tile 的零返回**，**写入的那个 Tile（`parent_ipix=5`）的像素级恢复从未被查询**。原交付件未报。 |
| **E5** | **须修** | `tests/hiss_writer_smoke.cpp:130-147` | **③往返自证（且更弱：不过文件）** | 原交付件已报「往返自证」，**本车道补强一层**：`:130 meta.to_json()` 序列化的是**测试自己的局部 `meta`**，`meta2.from_json(json)` 解析的也是**同一个内存串** ⇒ **全文件从未把写盘后的 JSON 头读回来解析过一次**（`:95-96` 只读了前 20 字节签名块）。故它连「writer 落盘的 JSON 是否可解析」都不覆盖。**修复建议应是「把 :20 的 `json` 换成从文件 offset `header_offset` 读回并解析」**，而不是继续加固 `meta2` 与 `meta` 的比对。 |
| **E6** | **须修** | `product_io/src/atomic_publish.cpp:372-376` + 头 `:112` | **恒真门①（不变量恒为默认值）** | `20e99660` 报告（转引，本车道未逐行复核）：open 失败路径**完全绕过** `cleanup_tmp`，`res.tmp_residue` 保持头文件注释「必须恒为 false」的默认值；而 `:367-370` 明确容忍 EEXIST 重试 8 次 ⇒ **已知 tmp 可能存在，却报「无残留」**。**这一条直接给原交付件对 `:332-337` 的 ✅ 加了限定。** |
| **E7** | **须修** | `tests/test_healpix_io.py:21/:34/:70/:79`（转引 `20e99660`） | 双重死亡 | 要求 `healpix_io.dll`，而 `Makefile` 唯一产品目标是 `astro_image_io.dll`（`grep healpix_io` 于 Makefile **零命中**）；**即便编出该 DLL**，`:34 _dll.hiss_write` / `:70 _dll.hcsd_read_leaf` / `:79 _dll.hio_free` 在 import 期即 `AttributeError` —— 导出名是 `aio_hiss_write` 等，带前缀；旧名**只以 C++ 预处理器宏存在**（`include/aio_healpix_io.h:251/257/258`），**宏不产生 DLL 导出**。⇒ **即使重编并放置，该测试仍无法通过。** |
| **E8** | **建议** | `runtime/artifact_store/production_store.py:857-864` | 持久化证据丢失 | `_fsync_dir` 文档说「Windows 无目录句柄 fsync 语义时静默跳过」，但 `:863 except OSError: pass` **吞掉全部 OSError**、**无任何平台判别** ⇒ 真实 EIO 与「无此语义」不可区分。5 个调用点（`:571/:572/:575/:577/:590`）一律把「没报错」当「已落盘」。另 `:571-572` 这两次 fsync 打在 rename **之前**，目录尚无变更 ⇒ **空转**。 |
| **E9** | **建议** | `runtime/artifact_store/production_store.py:616` | ④（生产代码用 assert 做控制流） | `assert self._source_commit is not None`；`-O` 下消失。当前由 `:548` 的 `if self._source_commit is not None` 守卫而侥幸不可达，**与 `hiss_common.cpp:125/:148` 属同一族「靠调用顺序侥幸」。** |

---

## 5. 子代理派发与否决记录

### 5.1 派发

派发 2 个（本车道范围内；工具对每次调用返回了重复实例 `3343a431` / `e991858a` / `6b7a44b2`，重复副本已 `interrupt_agent` 中止并从 `list_agents` 确认 inactive）。

| ID | 授权范围 | 实读 | 状态 |
|---|---|---|---|
| `976b9280` | `Makefile` · `tests/CMakeLists.txt` · `tests/sanitize_wsl.sh` · `test_gradient_sampler.cpp` · 两个 `.gitignore` · `aio_file_io.h` · `aio_upm.h` · `aio_hips_reader.h` · `aio_sysinfo.h` · `snr_evaluator.h`（另加读 `run_selftests.sh`、归档 `Makefile`、根 `CMakeLists.txt` 定点） | **927 行（片内 100%）** | 已交付 |
| `20e99660` | `hips_output_store.py` · `atomic_publish.cpp` · `io_adapter.cpp` · `test_healpix_io.py` | **1 995 行（100%）** | 已交付 |

**两个子代理均自报已披露 HEAD 漂移并用 `git diff --quiet` 验证目标文件在 `19bbc439..HEAD` 区间逐字节未变** —— 本车道独立复核同一结论为 **0 行**，三方一致。

### 5.2 否决与改级（逐条）

| 子代理结论 | 本车道裁决 | 理由 |
|---|---|---|
| `976b9280`：`sanitize_wsl.sh` 的 `ALL_SANITIZE_PASS` **从不打印**，脚本在 `:19` 响亮中止 rc=2 ⇒ **原 B13 因果链否决** | **✅ 接受** | 本车道复核控制流：`set -e`(`:4`) + glob 未匹配 ⇒ `ls` 收字面量 `*.o` ⇒ 退 2。**这正是「机制正确但从不执行」第④型**，比「假绿」更贴近项目判别依据。 |
| `976b9280`：B9 结构成立但**严重性从阻断下调为死代码**（全仓零构建配方） | **✅ 接受** | 处置应为按 AGENTS.md §6 删除，而非给没人跑的代码加 `-UNDEBUG`。 |
| `976b9280`：`aio_file_io.h` 的 `f_` **7 处无锁访问**，头 `:131-132` 的并发合同不成立（原交付件记为「`:223` × `:164` 窄竞态」） | **✅ 接受为升级** | 原判词的定位不准：持锁的 `write_at` 根本不修改 `f_`，故 `:164`×`:223` 不构成冲突；真实缺陷面是 `flush_and_sync` 只把 `fflush` 放锁内、`:205/:207/:213/:217` 裸奔，可致 **use-after-free / double-close**。 |
| `976b9280`：`γ=0` 时是 **`inf/inf`** 而非原交付件写的 `0×inf` | **✅ 接受机理修正，结论不变** | `w_i = 1/γ^p` ⇒ γ=0 ⇒ `w=+inf` ⇒ `Σw·x/Σw` = `inf/inf`。 |
| `976b9280`：`snr_evaluator.h` 死 include 在 **`:15`** 不是 `:14` | **✅ 接受行号修正** | `:14` 是 `<cstdint>` 且在用。 |
| `20e99660`：`:661-673` / `:461`+`:174` / `:229`↔`:363` **三条虚构或推理错误** | **✅ 全部接受** | 见 §2.1 R6/R7/R8。**这三条若原样流转会虚增发现数**，须在原交付件中撤下。 |
| `20e99660`：`:457`/`:609`「跳过 rollback」是**正本明令的正确行为**，真缺陷只是「跳过 verify」 | **✅ 接受** | 引 `atomic_publish.h:28-31`。**按原交付件原样交付会与冻结正本冲突。** |
| `20e99660`：`make_parent_dirs` 恒真的后果**下调两级**（唯一调用点丢弃返回值） | **✅ 接受，但登记为条件性** | 若前台补上返回值检查，此条应立即上调回须修。**该条件性已写入本条裁决。** |
| `20e99660`：B11「全仓零测试覆盖」升级依据（fixture `:128` 是单标量 order，全部测试字面量都是 `Norder0`） | **✅ 接受为补强** | 期望与实测同源于「只有一个 Norder」的同一假设 ⇒ **零判别力**。 |
| `20e99660`：B10 的精确化限定（读回校验对短写/损坏**是**真判别力，只对持久化盲） | **✅ 接受** | `checksum_`(`:71`) 来自调用方 buffer、`got`(`:100`) 来自磁盘字节，两侧独立。**不精确化会把修复范围放大到把好代码也改掉。** |

### 5.3 本车道**未**否决任何子代理的阻断级结论（与原交付件 §7.2 的自陈一致）

---

## 6. 仍未读完的部分（如实登记，**不谎报 100%**）

### 6.1 本车道本人未逐行读、且**未**由本车道子代理覆盖的成员

| 成员 | 行数 | 覆盖来源 | 本车道强度 |
|---|---|---|---|
| `src/healpix/aio_healpix_io.cpp` | 2 201 | **原车道子代理 `0890211e`** | C · 转引 |
| `src/hiss_writer.cpp` | 1 173 | 原车道子代理 | C · 转引 |
| `src/hiss_codec.cpp` | 460 | 原车道子代理 | C · 转引 |
| `healpix_db/…/gradient/gradient_sampler.cpp` | 612 | 原车道子代理 | C · 转引 |
| `tests/abi/aio_abi_layout_lock.py` | 239 | 原车道本人 | C · 转引 |
| `src/aio_sysinfo.cpp` | 312 | 原车道本人 | C · 转引 |
| `tests/CMakeLists.txt` | 50 | **本人已读满** | A |

**⇒ 本车道本人未逐行读全的成员共 5 份、4 846 行（占 45.6%），其行号依赖原车道子代理转引。**

### 6.2 为裁决而越片读取（**不构成对片外文件的覆盖率主张**）

`tests/hiss_correctness_test.cpp`（`:40-89`、`:1300-1339`）、`src/hiss_stream_writer.cpp:30-109`、`src/hiss_reader.cpp:72`、`src/hiss_common.cpp` 之外的 `hiss_format.h` 引用、`healpix_db/healpix_io/ARCHIVED.md`、`tests/abi/CMakeLists.txt`、`tests/test_healpix_io_py.py`、`eng/tests/io/hips_output_fixture.py`、`eng/tests/io/test_hips_output_contract.py:40-139`、`docs/engineering/io/` 对应册 —— 均**未通读**，已在裁决中逐条标注为「片外、仅定点」。

### 6.3 仍须前台执行的裁决项（本车道无权执行）

1. `tests/abi/CMakeLists.txt`（**不在本片**）注册的是 `--selfcheck` 还是真 ABI 锁 —— 若只注册 selfcheck，则全片无 ABI 门；
2. `healpix_io.dll` 是否真无生产者 —— 本车道只核实了本片 `Makefile` 零命中；`build.ps1:94` 与 `lib/algorithms/drizzle/healpix_drizzle/Makefile:16` **未读**，可能存在第二个悬空消费者；
3. 根 `CMakeLists.txt` 其余 1 554 行 —— 未核实是否在别处对特定 target 追加 `-UNDEBUG`；
4. **`test_output.txt` 与 `hiss_correctness_test.cpp` 的代际对应关系** —— 本车道已证明 `:298` 的 `实际通过:` 行**不在当前源码中**，但**未逐版追溯**该快照对应哪一 commit；
5. `docs/engineering/io/IO_003_ATOMIC_OUTPUT_PUBLISH.md` 等文档链上层 —— 本件全部裁决以代码 + `atomic_publish.h` 的正本注释为锚，**未与文档链上层比对**；
6. `aio_healpix_io.cpp` 的 JSON 浮点序列化实际输出格式 —— `test_healpix_io.py:172` 的脆性判据能否被推翻，落点在此文件与 `hiss_common.cpp:to_json`。

---

## 7. 自证段

### 7.1 本审稿自身的判别依据

- 唯一判别依据：**期望量能否追回到被检验量自身的输出字段**。据此：
  - **确认** B5（`hiss_common.cpp:157` NaN 双重假）、B6（`hiss_tile_model.cpp:164` shift 恒等）—— 并对每条都独立做了代数/IEEE 展开；
  - **新增** E4（`test_output.txt:286` 「成功或无覆盖返回零值」两条分支同可观测量）、E2（`:1317` 通过数减法导出）；
  - **升级** 原交付件 §3.3 对 `hiss_writer_smoke.cpp` 的正面结论 —— 「写法正确」不等于「有判别力」，一个已知红且不注册的测试**不产生任何证据**。
- **未把「往返」当独立验证**：据此把 `hiss_writer_smoke.cpp:130-147` 从「缓解」升级为 E5（并补出「不过文件」这一更强事实）。
- **未把「读回了同一个值」当验证**：`test_output.txt:149` 已把 Writer 52B / Reader 28B 的**具体字节数**记下，这恰恰证明该测试**具备判别力却选择记 OK**。
- **正面结论也须正面核实**：`aio_file_io.h:213` 的无泄漏、`aio_upm.h`/`aio_hips_reader.h`/`aio_sysinfo.h` 无 POD 结构体（`976b9280` 逐一核过含 `#ifdef` 检查）、`test_healpix_io.py:250-251` 的 10 对手推置换（`20e99660` 逐对核完，10/10 全对，并补了「不能区分 leaf-序与纯 ipix 序」的限定）—— **一律采信并记账为正**。

### 7.2 可复现性声明

- 全部行号来自本人或子代理用 `read` / `grep` 工具的带行号输出；**未使用任何脚本扫描替代阅读**。
- 全部计数由本车道亲自 `wc -l` 复算，与 `片清单-权威版.yaml:2986` 的 `实际行数: 10632` 逐位相符。
- **基线漂移**：已实测 `19bbc439 → 3081acbb → 37595fe3` 三次推进中，**本片 28 个成员文件 `git diff --name-only` = 0 行**，故本件行号对三个基线同时成立。两个子代理各自独立得出同一结论。
- **未执行**：任何编译、ctest、pytest、构建、任何脚本、未运行任何冒烟测试或自检、未 import 任何目标模块。`test_output.txt` 全程作为**文本**阅读，未当作任何运行的产物采信（并在 R4 中证明它与当前源码不同代）。
- **零 git 写**：未 add、未 commit、未 checkout、未 reset、未 stash、未改任何分支/标签/引用。
- **未改任何仓内文件**：本车道只写了本件与姊妹件 `审稿-P1-EXP-shared-001-补齐.md`。开工时 `git status` 中的 `M 审稿-P1-INF-aio-003.md` 非本车道所为，**全程未触碰**。

### 7.3 结论的强度限定（重申）

- **可直接采信**：§3 的两处高价值发现复核、§3.1.5 的正本失效判定、§4 的 E1–E5、§2.2 的 C1–C7 与 C11–C16。
- **建议抽查（转引子代理，本车道未复核行号）**：§4 的 E6、E7、§2.1 的 R5/R6–R14、§2.2 的 C8–C10、C12–C16 中依赖子代理的部分。
- **须先抽查再引用**：原交付件 §3.7–§3.12 中本车道未复核行号的全部条目（B5–B13、§3.12 表）。
- **一律不裁决**：§6.3 的 6 条「须前台执行」项，以及全部标「待核」的片外指针。

---

*本件由 G08-05 第 1 遍审稿 · 缺口补齐车道产出。**未读即未判**：本车道本人逐行覆盖 23.7%（2 521/10 632），本车道新覆盖 4 608 行，并集 100%（28/28 份）；约 4 846 行仍依赖原车道子代理转引，强度按 §1.6 分层对待。*
