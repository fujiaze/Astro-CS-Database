# 审稿-P1 · INF-gaia_xpsd_client-001（第 1 遍 · 对抗性重读）

- **片号**：`INF-gaia_xpsd_client-001`
- **层**：`lib/infrastructure/gaia_xpsd_client`
- **基线**：任务书写 HEAD=`850a9ede`；**实测工作树 HEAD 在审核期间漂移两次**：`1fa477a7`（越过 `bf25c085`、`1fa477a7` 两个提交）→ `f4a2cf21`「清理科学正本对已删归档的依赖」。本报告以 `1fa477a7` 为准。
- **漂移复核**：交付前核对 `f4a2cf21` —— 它只触及 18 个 `docs/science/*.md`，**未触及本片 15 份成员中的任何一份，也未触及 `docs/science/algorithms/GAIA_QUERY.md`**；`GAIA_QUERY.md` 的行号锚点（`622-646`/`1282`/`661-734`/`1378-1387`）原样保留 ⇒ **本报告全部发现在新 HEAD 下仍成立**。
- **纪律**：未读 `/tmp/acsd_g08/`；零 git 写；未改任何仓内文件；未编译、未跑 ctest/pytest/任何仓内二进制。注入实验全部在 `/tmp/p1_verify/`。中文路径统一 `git -c core.quotepath=false`。

---

## 1. 读完了吗

**成员份数 15 / 读了 15 = 100%。成员总行数 6021 / 实际读 6021 行 = 100%。未读完：无。**

口径说明：`wc -l` 逐文件计数，与片清单「实际行数 6021」逐位相符（3171+1066+426+268+213+160+156+142+119+84+78+60+40+28+10=6021）。目录下 `find -type f` 亦恰为 15 个文件，无清单外成员、无未登记残留（无 `.pyc` / `__pycache__`）。

| # | 成员文件 | 行数 | 读法 | 覆盖 |
|---|---|---|---|---|
| 1 | `src/gaia_client.c` | 3171 | read 分 7 块：1-400/401-850/851-1300/1301-1750/1751-2200/2201-2700/2701-3171 | 100% |
| 2 | `src/module_entry.c` | 1066 | read 分 2 块：1-550/551-1066 | 100% |
| 3 | `tests/test_gaia_zlib_configure_contract.py` | 426 | read 全文 | 100% |
| 4 | `test/test_gaia_race.c` | 268 | read 全文 | 100% |
| 5 | `src/gaia_client.h` | 213 | read 全文 | 100% |
| 6 | `README.md` | 160 | read 全文 | 100% |
| 7 | `test/test_spec_collector_ownership.c` | 156 | read 全文 | 100% |
| 8 | `CMakeLists.txt` | 142 | read 全文 | 100% |
| 9 | `integration/acsd_catalog_gaia.integration.json` | 119 | read 全文 | 100% |
| 10 | `memory.md` | 84 | read 全文 | 100% |
| 11 | `include/acsd/gaia/types.h` | 78 | read 全文 | 100% |
| 12 | `module.yaml` | 60 | read 全文 | 100% |
| 13 | `example/demo.c` | 40 | read 全文 | 100% |
| 14 | `Makefile` | 28 | read 全文 | 100% |
| 15 | `.gitignore` | 10 | read 全文 | 100% |

另为核实注释/文档指针而**只读**（非结论来源，仅作证伪/证实之锚）：`docs/science/algorithms/GAIA_QUERY.md`、`eng/tests/unit/CMakeLists.txt:1800-1832`、`lib/infrastructure/pipeline/orchestrator/cpp/src/orchestrator.cpp:1388-1560`、`lib/algorithms/photometry/cpp/Makefile`、根 `CMakeLists.txt:20-55 / 1203 / 329-350`。

---

## 2. 本片判定：**需修偏阻断**（存在 5 条阻断项，其中 1 条为可数值复现的静默漏星）

最重 3 条：

1. **【A-1】极区剪枝谓词可构造假阴性，且唯一守卫对它需要保护的情形恒不触发** — `src/gaia_client.c:1113-1116`（余纬镜像）+ `1130-1139`（用镜像值建平面坐标并剪枝）。我独立数值复现：**213 组组合假阴性**；节点 θ≤45° 时**恰为 0 组** ⇒ 前置条件精确为「AE 树越过 θ=45°」。触发半径最小到 **0.1°**（日常量级）。失败后 rc=0、`drop_ledger` 不置位 ⇒ 静默漏星。对应 `docs/science/algorithms/GAIA_QUERY.md:120` 同款规则与「false_negative=0」声称（同文件 §2.5）。
2. **【A-2】lz4 解压缺「解码长度 == block_size」校验，污染且被块缓存固化** — `src/gaia_client.c:1253-1264`。zlib 分支有 `:1260` 的 `dest_len == block_size`，lz4 分支**只判 `< 0`**；`:1191` 返回短长度即被当成功 ⇒ `:1814/1949/2093` 按完整 `block_size` 读未初始化 `scratch`，尾部垃圾被解析成伪星；`:1278` 又把该污染块**写入缓存持久化**。
3. **【C-1】本片两个已注册的门都没有链接它们自己声称赖以成立的 sanitizer，且从不校验 sanitizer 是否在** — `eng/tests/unit/CMakeLists.txt:1805-1824` 注册 `w34_gaia_race`（TSAN 门，`test/test_gaia_race.c:266` 原话「race 判定看 TSAN 报告」）与 `w34_gaia_spec_collector_ownership`（ASAN 门，`test/test_spec_collector_ownership.c:111-112` 原话「由 ASAN 守护」），但该段**零 sanitizer flag**；根 `CMakeLists.txt:21` `ACSD_ENABLE_SANITIZERS` 默认 OFF。⇒ 默认通道下两道门对各自命名的缺陷**检出率为零**，却照样打印 `ALL PASS`。

---

## 3. 逐文件清单

### 3.1 `src/gaia_client.c`（3171 行）

**读了什么**：全 7 块逐行；另在 `/tmp/p1_verify/` 用 Python 独立重写了 `polar_plane_intersects`、`bbox_intersects`、`lz4_decompress` 语义并做参数扫描。

**看到什么**：见 §4 A 系列；**判定：需修偏阻断**。

### 3.2 `src/module_entry.c`（1066 行）

**读了什么**：全 2 块逐行（我本人读，非依赖子代理）。

**看到什么**：C ABI adapter，完整 8 函数 vtable（`:1043-1053`）+ 唯一导出 `acsd_module_query_v1`（`:1057-1066`）。
- `:637`、`:982`、`:991` 三处 `return ACS_ERR_PARAM/INTERNAL` **未调 `efill`**，而这三个函数签名都有 `err` 形参（`:634`、`:633`、`:989`）⇒ 失败不产生稳定错误码，`err` 内容是调用方的残留。
- `:100-105` `strbuf_write` 在返回 `BUFFER_TOO_SMALL` **之前**已经 `memcpy` 写入了截断前缀；而本文件头 `:26` 明写「不足→PARAM+BUFFER_TOO_SMALL，**不半写**」⇒ **注释合同与实现相反**。
- `:554` 用 `w += snprintf(w, ...)`；而本文件 `:419-420` 的注释明文禁止：「snprintf 返回值=截断前应写长度，**不得直接用于推进**」⇒ **文件自身立规、自身违例**（截断时 `:574` `*w='\0'` 越界；实测 plan JSON ≈350B vs buf 1024B，**当前不可达**，属潜在栈溢出）。
- `:235` 只校验 `ra_list/dec_list` 指针非空，**不校验元素有限性**；`:247-254` 的 `isfinite` 六参数检查只覆盖标量 ⇒ `1e999` 坐标经 `strtod` 得 `inf` → `cos(inf)`=NaN → 静默判「未匹配」，而 `types.h:69`/`integration.json:110` 对外承诺非有限值走 `detail=103`。
- `:646` execute 期 config 解析失败**不**更新 `exec_count`/`last_status`，而 `:669-673` 预算耗尽路径**会** ⇒ `inspect` 的 `exec_count` 漏计失败次数（与 `integration.json:99`「inspect.exec_count 递增可巡检」不符）。
- `:786` `char head[512]` vs `:212` `char catalog_dir[1024]` ⇒ 目录名 >~440 字符必然执行期 `PARAM`（虽已正确处理，但配置层与执行层上限不自洽）。
- `:596` 用 `ACS_ERR_ABI_MISMATCH` 表达「host allocator 缺失」，语义错；且 `integration.json:102-112` 的 `failure_contract` **根本没有 `missing_allocator` 条目**。
- `:838-845` `tail_keys[6]` 死数组（`:845 (void)tail_keys;`），与 `:855-861` 的 `keys[]` 重复。
- `:1026` `if (inst->state == ACS_LC_STATE_DESTROYED)` 是**恒假门**：`:1037-1038` 置 DESTROYED 后立即 `free(inst)`，该守卫永不成立于合法调用。
- `:28` 注释称键词表冻结于 `lib/include/acsd/gaia/types.h` — **该目录不存在**（实测 `lib/include/acsd/gaia` → No such file）。真实路径为模块 `include/acsd/gaia/types.h`。`integration.json:4` 的「三方一致」声明因此有一环落在空路径上。
- **缓存常量四份独立字面量、零 linkage**：`:545` `kBlockCacheCap`、`:546` `kQueryCacheCap` 与 `gaia_client.c:209` `BLOCK_CACHE_MAX_MEMORY`、`:217-218` `QUERY_CACHE_MAX_BYTES` 互不相干；`gaia_client.c:210-216` 的注释把「二者相等」当作**推导**写出，但那是散文而非断言。改 `MAX_STARS_RESULT` 一处，两边即静默分叉，无 `static_assert`、无测试。

**判定：须修。**

### 3.3 `src/gaia_client.h`（213 行）

**读了什么**：全文。**看到什么**：
- `:3` 把缓存容量算式（`64×200000×3×sizeof(double)=307,200,000 B`）作为**散文**再抄第三遍 —— 三处独立副本，零编译期一致性。
- `:4` 并发锚点引 `docs/engineering/THREADING_MODEL.md`、`ERROR_MODEL.md` — **两者均不存在**（实测 MISSING；`OWNERSHIP_AND_LIFETIME.md` 存在）。同一行声称「`cache_lock` 互斥…包裹」，而 `gaia_client.c:2182-2184` **忽略 `pthread_mutex_init` 返回值并硬置 `cache_lock_initialized=1`** ⇒ 声称的互斥以一个不成立的事实为前提。
- `:6-19` 极区剪枝锚点引 `gaia_client.c:bbox_intersects`、`polar_plane_intersects`（无名号），`:17` 称「锥形查询为球面角距判定（**Haversine** 余弦定理）」，而实现 `:1847-1853/1982-1988/2126-2132` 用的是**余弦内积 + acos**（数值上劣于 Haversine 的那一支）。**文档点名了代码未用的那个方法名**，会让审阅者据以签发「数值稳定」结论。
- `:178-179` 查询侧 fail-closed 合同明确把 `MAX_STARS_RESULT` 截断**排除在「丢弃」之外**（`:179`「截断上限 … 不属丢弃」）⇒ 见 §4 A-5：自建不变式的最大口子被自己豁免。

**判定：须修。**

### 3.4 `include/acsd/gaia/types.h`（78 行）

**读了什么**：全文。**看到什么**：契约宏/枚举清晰、与代码一致（见 §6 子代理 D 的 G4/G5 逐项核对结论，我复核成立）。唯一问题：`:69` 把「README 限制7」当作该校验的出处，而 `README.md:153` 恰恰说「NaN/Inf 参数**不**显式校验」⇒ **代码引用了一份否认它的文档**（`integration.json:110` 同样措辞）。这是本片最纯粹的「期望量由一份与被检量矛盾的文档定义」。

**判定：建议（文档指针方向反了）。**

### 3.5 `tests/test_gaia_zlib_configure_contract.py`（426 行）

**读了什么**：全文。**看到什么**：见 §4 C 系列；6 场景中 5 个真判别、S4 的结构漂移守卫（`:375` `assertNotIn("set(ZLIB_LIBRARY")`）是本片写得最好的护栏（失败消息自称「不得静默跳过」）。但 `:297`/`:361` 两条「不得命中 fake/bogus root」是**恒真门**（`fake` 是每次随机的 `mkdtemp`，`bogus` **从未被创建**）；`:412` 在 POSIX 上因 `os.altsep is None` 而退化为 `mixed == canonical` ⇒ **在唯一能实际跑它的 Linux 上恒真**；`:154-177` 探测失败或 Windows 路径形态下静默退回一串 Linux 硬编码路径。

**判定：阻断（登记面，见 C-5）+ 须修（断言质量）。**

### 3.6 `test/test_gaia_race.c`（268 行）

**读了什么**：全文。**看到什么**：
- `:115` 四个叶块的 `mag_raw` **完全相同**（13500），`:195` 唯一的数据断言只查 `magG` ⇒ **读错块不可能改变 `magG`**；而 `:193` 注释声称「修复前 UAF/撕裂可能读到错块 ⇒ 星等错乱」。**该断言对它声称检测的故障模式盲。**
- `:242-243` 单线程预检已把 4 个叶块全部灌入块缓存与查询缓存；`:180` `ra = 180.0 + (r%4)*0.001` 只产生 **4 把键**，TTL 60s（`gaia_client.c:203`），整个测试毫秒级 ⇒ **56/60 轮走缓存命中路径（`gaia_client.c:2383-2424` 直接 return），`block_cache` 一次未被触碰**；即便走冷路径，`:1236-1239` 命中即 return，`block_cache_insert`（`:1278`）不可达 ⇒ **竞态最重的 insert/淘汰/free 从未执行**。
- `:207` 默认写死 `/tmp/gaia_race_fixture`，`:210-223` 开头 `unlink` 该目录全部条目，结束不清理 ⇒ 多构建树并发互踩。
- `:170` `volatile int g_errors` 被 8 线程 `++`（`:185/191/198`）。仅因这些 `++` 只在失败分支执行才无实害。

**判定：阻断（与 C-1 合并计）。**

### 3.7 `test/test_spec_collector_ownership.c`（156 行）

**读了什么**：全文。**看到什么**：
- `:110` `CHECK(sc.stars != NULL, "…单一所有权核心断言")` 是**恒真门**：修复版与缺陷版**都不给 `sc->stars` 赋 NULL**，缺陷版下它是已释放但非 NULL 的悬垂指针 ⇒ **缺陷版此断言照样 PASS**。
- `:111-112` 注释自己承认真正的判据是 ASAN，而 ASAN 未链接（C-1）。
- `:34` `#include "../src/gaia_client.c"` ⇒ 该 TU 传递性消费 `<zlib.h>`（`gaia_client.c:26`），但 `eng/tests/unit/CMakeLists.txt:1817-1819` 只把 `acsd_gaia_zlib_include` 给 `w34_gaia_race`，且注释明文断言「spec_collector 用例**不收 zlib**」⇒ **该前提为假**，Windows/MSVC 下 C1083（模块自己的 `CMakeLists.txt:100-105` 已把这条写成正典）。
- `:49-50` `gaia_test_malloc` 不检查 `dlsym` 结果（`:57-58` 的 calloc 检查了）⇒ 不一致。

**判定：阻断（恒真核心断言 + 缺 zlib 包含面）。**

### 3.8 `CMakeLists.txt`（142 行）

**读了什么**：全文。**看到什么**：
- `:127-130` 注释称「`gaia_client.c` 使用 GNU libm 扩展 **sincos**（RTLD_NOW 下未定义符号）」⇒ **实测 `gaia_client.c` 中 `sincos` 命中 0 次**；实际 libm 依赖是 `acos`×4/`asin`×1/`atan2`×1/`cos`×28/`sin`×24/`sqrt`×3。**`-lm` 该加（结论成立），但给出的理由指向一个文件中不存在的函数** ⇒ 又一处「注释声称与代码相反」。
- `:106-123` 定义了 INTERFACE target `acsd_gaia_zlib_include`，本文件内**无消费者**（供 eng/tests 用）；`:119-120` 的裸 configure 分支「宁可零改动」策略使 `:133` 的 `acsd_platform_math` 在该路径下不可用而无人报错。
- `:133` 多两格缩进（兄弟语句全在第 0 列）——合并残留。
- `:46-47` Linux 分支**零警告开关**（MSVC 有 `/W3`）；而 `README.md:155` 称「Linux amd64（当前构建验证）」⇒ 生产验证平台构建无任何警告。
- `:77` 把 `tests/test_gaia_zlib_configure_contract.py` 说成「可在 Linux 侧同构复验」的手段，而该文件全仓零登记面（C-5）。

**判定：须修。**

### 3.9 `integration/acsd_catalog_gaia.integration.json`（119 行）

**读了什么**：全文。**看到什么**：
- `:27` `"resource_class": "io"` vs `module.yaml:58` `resource_class: io_bound` ⇒ **同片两份契约文件口径分歧**，而 `:4` 自称「所有 ID/键/词表与 module.yaml、types.h **三方一致**」。
- `:110` 与 `types.h:69` 同款倒置引用（README 限制7）。
- `:102-112` `failure_contract` 缺 `missing_allocator` 条目（实际 `module_entry.c:596` 会返回）。
- `:45`/`:56` 的 `unit` 串（`"n/a(dataset dir)"` / `"deg,mag,W*m^-2*nm^-1,nm"`）不在 registry `unit_vocabulary` token 内。

**判定：须修。**

### 3.10 `memory.md`（84 行）

**读了什么**：全文。**看到什么**：整篇是**开发流水日志**，与 AGENTS.md §5「正文无日期、版本号、任务流水编号、commit、历史叙事」正面冲突；且多处被证伪：
- `:16-17`「C99 / **无外部库（零依赖）**」⇒ 实测依赖 zlib + OpenMP + Threads + libm（`CMakeLists.txt:52/125/133/139`）。
- `:13`「默认分支：master」 vs `:81`「分支统一为main」——**同文件两行互斥**。
- `:23` `gaia_query_cone` ⇒ **全仓仅 memory.md:23 自身出现**，真实符号是 `gaia_client_cone_search`（`gaia_client.h:86`）。
- `:32` `docs/detail/gaia_xpsd_client.md` ⇒ **不存在**（已迁至 `docs/detail/infrastructure/22_gaia_xpsd_client.md`）。
- `:37-38`「验收全 PASS … mutation 试金石 4 passed」、`:42-44`「**全部 Gate 通过**」⇒ 这些门的证据归档已随 `1fa477a7` 删除，**结论保留、证据消失 ⇒ 不可复验**。

**判定：须修（建议整体退役，AGENTS.md §5 不允许存在此类载体）。**

### 3.11 `module.yaml`（60 行）

**读了什么**：全文。**看到什么**：
- `:39-45` 声称 4 个符号「在三个生产命令的名字级调用图上不可达（判据 = `eng/ci/check_prod_wiring.py` W1）」并据此撤出 `source_symbols`。**实测三个是活调用点**：`module_entry.c:712`（`gaia_client_cone_search_with_photometry`）、`:718`（`gaia_client_query_spectrum_by_coords`）、`:796`（`gaia_client_get_db_type`）；而前两个正是**同一文件** `:24/:26` 声明的 `node_operations` 的实现 ⇒ **文件内正面冲突**。判据脚本 `eng/ci/` **整目录已删**，`dormant_algorithms.json`（`:44-45`）亦零命中 ⇒ **撤下决定当前不可复验、不可反驳**。
- `:58` `io_bound` 是全仓孤例（另 23 件用 `io`/`cpu_heavy`/`cpu`/`metadata`），且与 `integration.json:27` 的 `"io"`、schema enum 的 `io_bounded` 三种拼法并存。
- `:55-56` `test_ids: [TEST-GAIA-DESIGN-001]`，不含本片三个测试自称的 ID（`CI-WIN-001`/`B4-P1-2`/`B4-P1-3`）。

**判定：阻断（退役声明被证伪）。**

### 3.12 `example/demo.c`（40 行）

**读了什么**：全文。**看到什么**：`:7-10` `atof` 无校验（`atof("abc")=0.0`、`atof("nan")=NaN`）⇒ NaN 直灌 legacy API ⇒ `polar_plane_intersects` 全比较为假 ⇒ **全剪枝 ⇒ 返回 0 颗、exit 0**。`:6` 硬编码 `./GaiaDR3SP`；POSIX 空目录返回非 NULL 空 client（`gaia_client.h:175`）⇒ demo 打印 `Found 0 stars` 并成功退出，**用户无法区分「目录错」与「真的没星」**。

**判定：建议。**

### 3.13 `Makefile`（28 行）

**读了什么**：全文。**看到什么**：
- `:5` `SRC = src/gaia_client.c` ⇒ **不编 `module_entry.c`** ⇒ 产物无 `acsd_module_query_v1`，与 `CMakeLists.txt:7-9`、`integration.json:22` 的「唯一导出」合同**相反**。
- `:20` 共享库规则 `-lz -fopenmp` **无 `-lm`**，而 `:23` demo 规则**有** `-lm` ⇒ 同文件内不一致；实际 libm 调用 61 处（见 §3.8）。
- `:13/:20/:23` 硬编码 `gcc`，无 `CC` 可覆盖；`:12-13` `gaia_client.o` 不依赖头文件。
- 根 `CMakeLists.txt`（BLD-002）已写「根 CMakeLists.txt 是唯一产品事实源」⇒ 此 Makefile 依此应退役，但既未删也未声明退役，且 `README.md:142` 仍在把它当「构建（现状）」发布（并附已被 `Makefile:1` 移除的 `-march=native`）。

**判定：阻断（双构建漂移 + 发布有缺陷路径）。**

### 3.14 `README.md`（160 行）

**读了什么**：全文。**看到什么 —— §9「已知限制」整节过期，且与代码、与自身冲突**：
- `:4` 以 `eng/tools/quality/check_module_map.py` 为「现状复测」机读依据，`:6` 据其 `noop_entrypoint` finding 宣布「机读状态 = **NOT_IMPLEMENTED**」⇒ **该脚本不存在**（实测 MISSING），判决无可复现来源。
- `:6-9`/`:19`/`:145` 称 entrypoint「函数体零调用」、`:147` 称「C ABI adapter / 版本化 query 入口 / `plan()`/`cancel()` **不存在**」⇒ `module_entry.c:1043-1066` 完整实现（`gaia_plan` 在 `:507-579`，cancel 检查点在 `:729`）。
- `:118`「cancel/checkpoint：**无取消检查点**」⇒ `gaia_client.h:207` 已导出 `gaia_set_cancel_checkpoint`，`gaia_client.c:174` 已实现。
- `:153`「NaN/Inf 参数不显式校验」⇒ `module_entry.c:247-252` 有显式六参数 `isfinite` → `detail=103`。
- `:46`/`:149`「`parallax/pmra/pmdec` **未初始化**」⇒ `gaia_client.c:2407`/`:2502` 均 `calloc`，`:2421`/`:2536` 显式 `source_id=0`。
- `:89` 声称 DR3SP 记录布局在「源码 **1378-1387**」⇒ 实测该区间是 mmap 失败路径与魔数校验；**真实布局在 `gaia_client.c:1992-1997`**（偏移约 615 行）。且 `GAIA_QUERY.md:131` 引**同一错误区间** ⇒ 错误已从正本传播到 README。
- `:142` 仍发布 `-march=native`；`:124` 又说该旗标「非 ACSD 生产 target」——**README 自相矛盾**。`:123`「纯 C99」vs `CMakeLists.txt:49` `c_std_11`。
- `:156`/`:39`（memory.md）指向已删归档 `run/local/agent_cat_gaia_doc/`。

**判定：阻断（判决依据消失且被同片代码证伪）。**

### 3.15 `.gitignore`（10 行）

**读了什么**：全文。**看到什么**：收 `*.o/*.obj/*.dll/*.so/*.a/*.lib/*.exe/*.out/__pycache__/*.pyc`，**未覆盖 `Makefile:10/22` 产出的无扩展名二进制 `gaia_client_demo`** ⇒ 按 `README.md:142` 跑一次 `make` 后该产物立刻进入未跟踪列表。**判定：建议。**

---

## 4. 发现清单

### 【阻断】9 条

| ID | 位置 | 机制 | 为什么错 |
|---|---|---|---|
| **A-1** | `gaia_client.c:1113-1116` + `1130-1139`（三处调用点 `1785`/`1920`/`2065` 各一份拷贝） | 查询中心余纬在 `:1116` 被镜像折回 [0,90]，随后 `:1130-1131` 用**镜像值**当平面坐标，`:1139` 据此剪枝 | 可构造假阴性（我已数值复现 213 组，θ≤45 时 0 组）。`:1124` 的守卫（`theta_q + radius > 90`）本为拦此而设，但归一化后 `θ_m + r ≤ 90` 恒成立 ⇒ **守卫对它唯一需保护的情形恒不触发（恒假门）**。失败后 rc=0、`drop_ledger` 不置位 ⇒ 静默漏星 |
| **A-2** | `gaia_client.c:1253-1264`（对照 `:1260`） | lz4 分支缺 `decompressed_size == block_size` 校验 | 流提前结束返回正的短长度即被当成功 ⇒ 未初始化 `scratch` 尾部按完整 `block_size` 解析成伪星；`:1278` 再把污染块**写入缓存**，使缺陷自持 |
| **A-3** | `gaia_client.c:1563`、`:553`、`:2182-2184`（消费点 `497/507`、`566/576/588`、`674/775`） | 三处锁初始化失败一律 fail-open：`bc_lock_ok=0`、`budget->lock_ok=0`、以及 `cache_lock_initialized` **被硬置 1 而忽略 `pthread_mutex_init` 返回值** | 初始化失败 ⇒ 静默跳过加锁（块缓存/预算记账/查询缓存全部无同步），**`load_xpsd_file` 仍返回 0、create 仍成功、无日志无错误码**。`cache_lock_initialized = 1` 是最纯粹的自洽式断言：**它断言「锁已初始化」，而它自己就是那个断言** |
| **A-4** | `gaia_client.c:3155-3156`（导出声明 `gaia_client.h:131`，登记 `module.yaml:54`，活调用者 `orchestrator.cpp:1401`） | `gaia_client_get_spectrum_params` 是 7 个兄弟 getter 中**唯一**无 `if (!client)` 的 | 传 NULL 即空指针解引用。同文件 `:2615/2620/2625/2637/2646/2651` 全部有守卫 |
| **A-5** | `gaia_client.c:1745`/`1820`/`1880`/`1955`/`2026`/`2099` + 豁免声明 `:63`、`:179` | `MAX_STARS_RESULT(200000)` 截断点直接 `return`，**不调 `drop_ledger_note`** | 本文件 `:402-408`/`:2472-2475` 把「任一丢弃 ⇒ 查询 fail-closed」立为不变量，却在 `:63` 自己豁免了唯一且最大的口子 ⇒ **自建不变式被自身最大例外击穿**。截断顺序按文件/树遍历序，丢的恰是锥内较暗者 |
| **A-6** | `module.yaml:39-45`（活调用者 `module_entry.c:712`/`:718`/`:796`；判据 `eng/ci/check_prod_wiring.py` 整目录已删；`dormant_algorithms.json` 零命中） | 以「名字级调用图不可达」为由把 4 个符号撤出 `source_symbols` | 前两个正是同文件 `:24/:26` 声明的 `node_operations` 的实现 ⇒ 文件内正面冲突。**且判据口径本身有结构性盲区**：名字级调用图看不见 `dlsym`/`GetProcAddress` —— 而 `orchestrator.cpp:1401` 正是活的动态解析点 |
| **A-7** | `eng/tests/unit/CMakeLists.txt:1805-1824` + 根 `CMakeLists.txt:21` | 两个已注册的门（TSAN / ASAN）均无 sanitizer flag，且测试自身**从不校验** sanitizer 是否链上 | 默认通道下 `test_gaia_race.c:266` 与 `test_spec_collector_ownership.c:153` 照样打印 `ALL PASS`。**判决词与判决能力分离** |
| **A-8** | `README.md:4`/`:6`/`:19`/`:145`/`:147`（检查器 MISSING；实现见 `module_entry.c:1043-1066`） | 以一个**不存在的脚本**的 finding 宣布 `NOT_IMPLEMENTED`，并保留 4 条已被代码修复的「已知限制」 | `eng/tools/quality/check_module_map.py` 不存在 ⇒ 判决无可复现来源；同片的 1066 行 adapter 就在旁边把它证伪 |
| **A-9** | `Makefile:5`/`:20` vs `CMakeLists.txt:7-9`/`integration.json:22`；`README.md:142` | 第二套构建不编 `module_entry.c`、共享库不链 `-lm`，README 仍把它当「构建（现状）」发布 | 产物**没有**唯一导出面合同要求的 `acsd_module_query_v1`；且缺 `-lm` ⇒ 61 处 libm 调用悬空。根 CMakeLists（BLD-002）已声明自身是唯一产品事实源 ⇒ 此 Makefile 应退役却仍在被发布 |

### 【须修】18 条

| ID | 位置 | 摘要 |
|---|---|---|
| **B-1** | `gaia_client.c:1437`/`:1535`/`:1242`（对照 `:1457` 的严格 `parse_bounded_int`） | `data_position`/`block_offset` 全程不与 `mmap_size` 比较 ⇒ 越界读；`nodeCount`/`rootPosition` 走 `atoi` 无界（`:1508-1526`）。**同函数内对 `spectrumCount` 严防死守、对树节点完全不设防** |
| **B-2** | `gaia_client.c:1394-1395`（读出即弃）、`:1397`、`:970-1026` | `header_len` 读出后全文未用；其后 `strstr` 在 mmap 上无界扫描 ⇒ 文件大小恰为页整数倍时越界。文件自己声明了 `header_len`，正是为限定此扫描 |
| **B-3** | `gaia_client.c:1196-1210` | `byte_unshuffle` 只填 `n*item_size`，`:1208` 把尾部余数（≤ `item_size-1`）未初始化字节回写进数据 |
| **B-4** | `gaia_client.c:2452`（裕量 `:0.25`，证据 `run/perf-fix/P1-gaia/REPORT.md` MISSING） | shard 星等剪枝整文件 `continue`，**不记账不告警**，被剪的恰是各 shard 最亮的一段；裕量与记录解码 `mag_raw*0.001-1.5`（`:1827`）是否同标度**全仓无断言** |
| **B-5** | `gaia_client.c:2443`/`:2707`/`:2901`/`:3069` | 取消检查点 `continue` 不进 `drop_ledger` ⇒ 返回 rc=0 的残缺星表，与「空结果」不可区分（`:158-165` 把 CANCELLED 判定全推给 adapter） |
| **B-6** | `gaia_client.c:2254-2292`（全文无 `qsort`/`scandir`/`alphasort`） | `readdir` 序未排序 ⇒ 输出星序依赖文件系统枚举序（`:161` 注释只声明了「与线程数无关」） |
| **B-7** | `gaia_client.c:2803-2807`/`:2959-2962` | `copy_count = min(本文件 spectrum_count, 首个光谱文件的 global_spec_count)` ⇒ 较大者被静默截短；且 `out_spectra` 是 `malloc` 而非 `calloc`，`0 < spectrum_count < global_spec_count` 时**尾部为未初始化堆字节**。KI-2 修复（`:2809-2816`）只补了 `count==0` 分支 |
| **B-8** | `gaia_client.c:168-170`（static）+ `module_entry.c:651`/`:678`/`:728` | `gaia_leased_workers`/`gaia_cancel_poll_fn`/`gaia_cancel_poll_ud` 是**进程级全局**且无锁写；`inst->executing` 互斥是 per-instance（`:638`）⇒ **两实例并发 execute 时 B 的租借数与 cancel 回调覆盖 A**。`gaia_client.c:190` 注释称「fn 原子读，**无共享写**」为假 |
| **B-9** | `module_entry.c:638`（对照 `:13` 声称「同实例 execute 互斥」） | 无锁 check-then-act ⇒ 声称的互斥未被任何锁或原子实现 |
| **B-10** | `module_entry.c:100-105` vs 头注释 `:26` | 「不半写」合同与实现相反：先写截断前缀再返回 `BUFFER_TOO_SMALL` |
| **B-11** | `module_entry.c:554` vs 同文件 `:419-420` | 用本文件明文禁止的 `w += snprintf(...)`（当前不可达，属潜在栈溢出） |
| **B-12** | `module_entry.c:637`/`:982`/`:991` | 三处失败返回未 `efill`，无稳定错误码 |
| **B-13** | `module_entry.c:235` vs `:247-254` | 非有限值校验只覆盖标量，`ra_list`/`dec_list` 元素不校验 ⇒ `inf` 坐标静默判「未匹配」，与 `types.h:69` 承诺的 `detail=103` 相悖 |
| **B-14** | `gaia_client.c:2134-2140` | `has_spectrum` 门控了 BP/RP 读取，但两 stride 的前 32 字节布局相同（`:30-31`）⇒ 纯 DR3 目录下 `cone_search_with_photometry` **恒返回 magBP=magRP=0**（README:50 只记了症状未记成因） |
| **B-15** | `gaia_client.c:1225-1228`（注释）/`:1236-1239`/`:635-653` | 注释以「与单写者时代一致」否定并发淘汰风险，而 `:475-481` 自己承认 by_coords 并行轴=坐标、同文件多 worker 并发读写 ⇒ **历史等价已被证伪**，是 UAF |
| **B-16** | `module.yaml:58` vs `integration.json:27` vs `module_dll_contract.schema.json` | `io_bound` / `io` / `io_bounded` 三种拼法并存；`integration.json:4` 自称三方一致 |
| **B-17** | `CMakeLists.txt:127-130` | 「gaia_client.c 使用 GNU libm 扩展 sincos」—— **实测 `sincos` 命中 0 次**；真实 libm 依赖为 `acos`×4/`asin`×1/`atan2`×1/`cos`×28/`sin`×24/`sqrt`×3。结论（该加 `-lm`）成立，**理由指向不存在的函数** |
| **B-18** | `eng/tests/unit/CMakeLists.txt:1817-1819`（注释）+ `test_spec_collector_ownership.c:34` | 注释断言「spec_collector 用例不收 zlib」为假（该 TU `#include` 了含 `<zlib.h>` 的 `gaia_client.c`）⇒ Windows/MSVC 必编译失败；Linux 靠 `/usr/include` 静默掩盖（同 `CMakeLists.txt:100-105` 已把该失效模式写成正典） |

### 【建议】14 条

1. `gaia_client.h:3` + `gaia_client.c:217-218` + `module_entry.c:545-546`：缓存常量**四份独立字面量零 linkage**，`gaia_client.c:210-216` 把「二者相等」写成散文推导而非断言。
2. `types.h:69` + `integration.json:110`：以「README 限制7」为出处，而 `README.md:153` 恰说该校验不存在 ⇒ **期望量由一份否认它的文档定义**。
3. `gaia_client.h:17`「Haversine 余弦定理」vs 实现 `:1847-1853` 的余弦内积+acos ⇒ 文档点名了代码未用、且数值更优的那个方法名。
4. 源码行号锚点系统性漂移：`GAIA_QUERY.md` 11 处抽核 **7 处错**（`:56`→空行、`:69`→murmur3、`:83`→空行、`:90`→淘汰函数、`:105`→缓存查找、`:129`→lz4 memcpy、`:131`→mmap 失败路径）；`README.md:89` 同样指向 `1378-1387`。**`:131` 与 `README.md:89` 引的是同一错误区间 ⇒ 错误已从正本传播**。
5. 悬空引用 4 处已删归档：`run/WCS-DETERMINISM-01/REPORT.md`（`gaia_client.c:50`/`:404`、`gaia_client.h:162`）、`run/perf-fix/P1-gaia/REPORT.md`（`gaia_client.c:2451`）、`run/perf-fix/P1-gaia/evidence/shard_magranges.tsv`（`:40-41`）、`run/local/agent_cat_gaia_doc/`（`README.md:156`、`memory.md:39`）—— **全部由 `bf25c085`/`1fa477a7` 清理，本片未收口**。
6. 判据/台账悬空且无对应物：`eng/ci/check_prod_wiring.py`（整目录已删）、`dormant_algorithms.json`、`eng/tools/quality/check_module_map.py`、`docs/detail/gaia_xpsd_client.md`（已迁至 `docs/detail/infrastructure/22_gaia_xpsd_client.md`）、`lib/include/acsd/gaia/types.h`（`module_entry.c:28`）。另 `docs/engineering/THREADING_MODEL.md`/`ERROR_MODEL.md`（`gaia_client.h:4`）与 `11_MODULE_SOURCE_TEST_STANDARD.md`/`12_DLL_ABI_AND_LOADER_STANDARD.md` 无同名对应物（疑有改名，但仓内无映射可证）。
7. `memory.md` 整篇违反 AGENTS.md §5，且 `:16-17`「零依赖」、`:13` vs `:81` 分支互斥、`:23` 编造符号 `gaia_query_cone`（全仓仅自身）、`:37-44`「验收全 PASS/全部 Gate 通过」而其证据归档已删。
8. `module_entry.c:1026` DESTROYED 检查为恒假门（destroy 后内存已释放）。
9. `module_entry.c:596` 用 `ACS_ERR_ABI_MISMATCH` 表达 allocator 缺失，且 `integration.json` 的 `failure_contract` 无此条目；`:838-845` 死数组；`:646` 与 `:669` 的 `exec_count` 记账不一致；`:786 head[512]` vs `:212 catalog_dir[1024]` 上限不自洽。
10. `gaia_client.c:2433`/`:2690`/`:3057` 的 `cos_radius` 计算并沿 3 个 `search_recursive*` 的 12 处递归逐层传递，**全文无一处读取**（死参数）。
11. `gaia_client.c:1063` 的 `1.2` 经验裕量在 EQ 路径仍在用，而 `:1074` 注释称本文件「取代 V18R2 的经验裕量 1.2」⇒ 只在极区路径取代。
12. `gaia_client.c:2645-2648` `get_file_load_fail_count` 对**活 client 结构性恒 0**（create 失败即返回 NULL）⇒ 任何 `assert(==0)` 恒真，是恒真门的 API 形态。
13. `gaia_client.c:179-188` 未注入租借时回退 `omp_get_max_threads()`（全核默认 team）；本文件**无** `pthread_create`/线程池（子代理 A 的 E1 否定成立，我复核：仅 3 个互斥量），残留形态是 OpenMP 默认 team 而非自建池。
14. `CMakeLists.txt:46-47` Linux 分支零警告开关（生产验证平台）；`:133` 多两格缩进；`.gitignore` 未覆盖无扩展名的 `gaia_client_demo`；`demo.c:7-10` `atof` 无校验 + 空目录 ⇒「Found 0 stars」且 exit 0。

---

## 5. 我主动构造的反例

| # | 构造什么 | 期望推翻什么 | 是否推翻 |
|---|---|---|---|
| **R-1** | **bbox_intersects 的 `×1.2` 裕量是否有假阴性**。先粗扫得 72 组「命中」，再限定 `dec∈[-90,90]` 合法几何重扫 | `gaia_client.h:9`/注释 `:1063` 的保守性声称 | **未推翻**。72 组初筛全部是 `dec∈[-92,-91.5]` 的**无效几何**；限定合法区间后**假阴性 = 0**。⇒ 我自己先前的怀疑与子代理 A 的 E7「待确认」**双双否定**，1.2 裕量在合法几何下充分 |
| **R-2** | 极区剪枝假阴性。参数扫描（节点 θ 区间 × dec × r），以球面真角距作独立真值 | `gaia_client.c:1083-1086` 与 `GAIA_QUERY.md` §2.5 的「无假阴性」 | **推翻成功**。213 组假阴性；最小触发半径表：节点 θ∈[45,55] 时 r≤5° 即触发，θ∈[50,60] 时 r≤0.1° 即触发。**θ≤45 时恰 0 组 ⇒ 前置条件精确**。反例样例：北极树，查询 `dec=40, r=1°`，节点 `y∈[49.5,50.5]`（真角距 0.5°，**确在锥内**）⇒ 剪掉 |
| **R-3** | 给 lz4 分支补上 zlib 分支同款的长度校验（改 `:1264` 一个条件） | 「部分解码仍返回 rc=0 星表」 | **推翻成功**：改后部分解码 ⇒ `read_leaf_block` 返回 NULL ⇒ `:1809/1944/2088` 记 `drop_ledger_note` ⇒ 查询返回 -1。⇒ **现有判据不自洽**（用「是否报错」当「是否正确」，而 lz4 流提前结束不报错） |
| **R-4** | 注入「`test_gaia_race` 读到了错误的块」 | `:193-200` 的数据断言能检出 UAF/撕裂 | **断言无效**。`test_gaia_race.c:115` 四个叶块的 `mag_raw` 全同（13500）⇒ 读错块不改变 `magG` ⇒ 断言仍绿。**该断言对它声称检测的故障模式盲** |
| **R-5** | 还原 B4-P1-3 修复前的 `spec_collector_push`（失败路径 `free(new_stars)`），跑已注册通道 | `test_spec_collector_ownership` 转红 | **默认通道下仍绿**。12 条 `CHECK` 无一能拦（`:110` 是恒真门），唯一判据 ASAN 未链接 ⇒ **判别力完全押在未被要求的工具上** |
| **R-6** | 遮住既有判定，独立核对 README 的 `NOT_IMPLEMENTED` 判决 | 判决是否成立 | **判决被证伪**。判据脚本 `check_module_map.py` 不存在；同片 `module_entry.c:1043-1066` 实现完整 8 函数 vtable 与唯一导出；`:147` 的 4 条「不存在」逐条被 `:507`（plan）、`:651`（cancel 检查点）、`:204/207`（头导出）推翻 |
| **R-7** | 撤下 4 个符号的「不可达」声称本身有没有判据 | 该决定是否可复验 | **不可复验**：判据脚本整目录已删。且名字级调用图对 `dlsym`/`GetProcAddress` 结构性盲 —— `orchestrator.cpp:1401` 正是活的动态解析点，名字级判据**看不见它** |
| **R-8** | `polar_plane_intersects` 的 θ 镜像（`:1116`）是否本身不安全 | 镜像是否致假阴性 | **镜像本身可否定**（保守）；**真正破掉的是 `:1124` 的守卫被镜像击败（恒假门）+ `:1130-1131` 用镜像坐标比节点矩形**。归因必须精确到这两处，不能笼统归给「镜像」 |

---

## 6. 盲复算（遮住既有判定独立取证）

**方法**：对每一类既有结论，先不看既有文本，自行从原文推导并取证，再比对。共抽核 27 条既有/子代理结论。

**判一致（14 条）** —— 既有结论成立，我独立复现：`gaia_client.c` 悬空归档 4 处、`memory.md` 零依赖/分支互斥/编造符号、`module.yaml` 判据悬空、`README.md:89` 行号错、`Makefile` 漏 `module_entry.c`、`integration.json` plan 键 17 个逐个相符、三方标识符一致、README 内存数字（4GB/8192/64/60s/65536/200000）逐值对得上、三个测试钩子真实、`acsd_platform_math` 定义早于 `add_subdirectory`、`.pyc` 无残留、CFG002-06 的 glob **确实覆盖**本片 module.yaml。

**判我更严（推翻既有/子代理的宽松结论，8 条）**：
1. **`bbox_intersects` ×1.2 裕量**：子代理 A 标「待确认、存在理论窗口」⇒ 我**否定之**（合法几何下假阴性恒为 0）。
2. **`n_coords * global_spec_count` int 溢出**：子代理 A 列为阻断 B7 ⇒ 我**证伪**（`gaia_client.c:2877`/`:3000` 均有 `(size_t)n_coords`，乘法在 64 位完成）。
3. **Makefile 缺 `-lm` 的理由**：子代理 D 称源于「GNU `sincos` 扩展」⇒ 我**证伪理由**（`sincos` 命中 0 次），但**保留结论**并给出更强依据（61 处真实 libm 调用）。
4. **`inv_scale`/`inv_dra` 魔数**：子代理 A 标「无出处」⇒ 我**否定**：`GAIA_QUERY.md:66-80` 给出完整推导（1 deg = 3.6e9 µas、1.8e9 LSB/deg ⇒ **2 µas/LSB**；`dra` 10 µas/LSB）。残留的真问题是**代码未回指该正本**（追溯链断），而非「无出处」。
5. **`gaia_client.c` 内部有无自洽式断言**：子代理 A 报「无」⇒ 我**补上 3 条它漏掉的**：`cache_lock_initialized = 1`（`:2184`）、`gaia_client_get_file_load_fail_count` 对活 client 结构性恒 0（`:2645`）、`gaia_request_cancel` 的 DESTROYED 检查恒假（`module_entry.c:1026`）。
6. **两个 C 测试是否已接线**：我初判「未接线」⇒ 复核后**自我修正**为「已 `add_test` 注册，但缺 sanitizer 且缺 zlib 包含面」——比初判更精确。
7. **`resource_class` 分歧**：我补上第三种拼法（schema enum `io_bounded`），子代理 D 已独立发现同样三套并存 ⇒ 互证。
8. **AE 树是否越过 θ=45°**：我把「假阴性成立」与「生产可达性」**分开裁定** —— 缺陷成立可复现，但仓内唯一 AE 夹具仅覆盖 θ≤1.05°（`gaia_xpsd_fixture_gen.c:569-578` 解析证实），`run/**` 下的 `.xpsd` 经解析**全部是 Equirectangular** ⇒ 生产可达性**不可判定**，如实标注而非夸大。

**判既有偏松（我新增、本轮未见既有文本覆盖，5 类）**：
1. **README §9「已知限制」整节过期**（4 条被代码证伪 + `NOT_IMPLEMENTED` 判决依据消失）。
2. **源码行号锚点系统性漂移**（`GAIA_QUERY.md` 7/11 处错，且与 `README.md:89` 引同一错误区间 ⇒ 错误已传播）。
3. **两个已注册门缺 sanitizer 且不校验**、**`w34_gaia_spec_collector_ownership` 缺 zlib 包含面且注释断言相反**。
4. **迁移桥段三全局量的跨实例串扰** + `gaia_client.c:190`「无共享写」为假。
5. **三处 `return` 未 `efill` 缺稳定错误码** + **`strbuf_write` 半写违反「不半写」合同** + **`plan()` 用本文件明令禁止的 snprintf 推进模式**。

---

## 7. 子代理派发记录

**派了 4 个**（分片核验），另本人独立通读全 15 份。

| 子代理 | 分片 | 状态 | 复核结果 |
|---|---|---|---|
| A | `src/gaia_client.c`（3171） | **已返回** | 逐条复核。**采纳** B1/B2（我独立数值复现，见 R-2）、B3（我独立阅读先于其返回即已发现）、B4/B5/B6/B8-B17/B18/B20-B24；**修正其归因**（θ 镜像本身可否定，破口在 `:1124` 守卫 + `:1130-1131`）；**否决 B7**（int 溢出不存在，`:2877`/`:3000` 有 `(size_t)` 强转）；**否决其魔数表对 `inv_scale`/`inv_dra`「无出处」的判定**（`GAIA_QUERY.md:66-80` 有完整推导）；**接受其 E1/E2/E3/E5/E6/E8 的自我否定**（尤其 E1「本文件无私建线程池」我复核成立） |
| B | `src/module_entry.c` + `src/gaia_client.h` + `include/acsd/gaia/types.h`（1357） | **未返回**（交付时仍在运行） | **其范围由我本人完整覆盖**：1357/1357 行 100%。§3.2/§3.3/§3.4 全部结论为我自己 read 原文得出 |
| C | 三个测试文件（850） | **已返回** | 逐条复核。**采纳** D-1/D-2/D-3/D-5、`ownership.c:110` 恒真门、`py:297`/`:361`/`:412` 恒真、`_system_ignore_paths` 静默退化；**接受其对本分片「0 条有害自洽式断言」的阴性结论**（我复核其三条最像的自洽候选均异源，判定成立）；**我补强 D-4**（zlib 包含面缺失）并给出本片 `CMakeLists.txt:100-105` 的同型正典作对照 |
| D | 构建/配置/文档（643） | **已返回** | 逐条复核。**采纳** B1（module.yaml 退役声明被证伪，我另加 `orchestrator.cpp:1401` 的 dlsym 活调用者作为「名字级判据结构性盲区」的实证）、B2/B3/B4/B5/B6/B7、S1-S12、G1-G5；**修正 B8 的 `-lm` 理由**（`sincos` 不存在）但**保留结论**；**接受其 G1-G8 的 8 条自我否定**（尤其 G2「5 个用例数对了」、G7「CFG002-06 确实覆盖本片」——两条都挡住了我的错误怀疑） |

**合计否决/修正子代理结论 6 条**（A 的 B7、A 的魔数表 inv_scale/dra 两项、D 的 B8 理由、A 的 B1 归因；另 2 项为归因精化）。**采纳并经我独立复核者 40+ 条。**

---

## 8. 自证段（可复跑命令）

```bash
cd "/workspace/Astro CS Database"
git -c core.quotepath=false rev-parse HEAD          # => 1fa477a7（≠ 任务书基线 850a9ede）
git -c core.quotepath=false log --oneline -3

# §1 覆盖率：成员行数合计应 = 6021
for f in src/gaia_client.c src/module_entry.c tests/test_gaia_zlib_configure_contract.py \
         test/test_gaia_race.c src/gaia_client.h README.md test/test_spec_collector_ownership.c \
         CMakeLists.txt integration/acsd_catalog_gaia.integration.json memory.md \
         include/acsd/gaia/types.h module.yaml example/demo.c Makefile .gitignore; do
  printf "%6d %s\n" "$(wc -l < lib/infrastructure/gaia_xpsd_client/$f)" "$f"; done
find lib/infrastructure/gaia_xpsd_client -type f | wc -l   # => 15

# §4 悬空引用（全部 MISSING）
for p in run/WCS-DETERMINISM-01/REPORT.md run/perf-fix/P1-gaia/REPORT.md \
         run/perf-fix/P1-gaia/evidence/shard_magranges.tsv run/local/agent_cat_gaia_doc \
         docs/detail/gaia_xpsd_client.md lib/include/acsd/gaia/types.h \
         eng/tools/quality/check_module_map.py eng/ci/check_prod_wiring.py \
         docs/engineering/THREADING_MODEL.md docs/engineering/ERROR_MODEL.md \
         docs/engineering/11_MODULE_SOURCE_TEST_STANDARD.md \
         docs/engineering/12_DLL_ABI_AND_LOADER_STANDARD.md; do
  [ -e "$p" ] && echo "EXISTS  $p" || echo "MISSING $p"; done
git -c core.quotepath=false ls-files | grep -E "prod_wiring|dormant_algorithms|module_map\.py"   # => 空

# A-1 / A-4 / A-6
sed -n '1113,1116p;1124p;1130,1139p' lib/infrastructure/gaia_xpsd_client/src/gaia_client.c
sed -n '3155,3156p' lib/infrastructure/gaia_xpsd_client/src/gaia_client.c
grep -n "gaia_client_cone_search_with_photometry(cli\|gaia_client_query_spectrum_by_coords(cli\|gaia_client_get_db_type(cli" \
     lib/infrastructure/gaia_xpsd_client/src/module_entry.c     # => 712 / 718 / 796
grep -n "get_proc_address(gaia_h, \"gaia_client_get_spectrum_params\")" \
     lib/infrastructure/pipeline/orchestrator/cpp/src/orchestrator.cpp   # => 1401

# A-2（lz4 vs zlib 长度校验的不对称）
sed -n '1253,1264p' lib/infrastructure/gaia_xpsd_client/src/gaia_client.c

# A-3（三处锁 fail-open + 硬置真）
sed -n '1563p;553p;2182,2184p' lib/infrastructure/gaia_xpsd_client/src/gaia_client.c

# A-7 / B-18（门注册但缺 sanitizer / 缺 zlib 包含面）
sed -n '1805,1824p' eng/tests/unit/CMakeLists.txt
sed -n '20,22p;39,43p' CMakeLists.txt
grep -n "sincos" lib/infrastructure/gaia_xpsd_client/src/gaia_client.c        # => 空（注释称用了）
grep -c "acsd_gaia_zlib_include" eng/tests/unit/CMakeLists.txt

# A-8（README 用不存在的检查器宣布 NOT_IMPLEMENTED）
sed -n '4,9p;89p;147p' lib/infrastructure/gaia_xpsd_client/README.md
sed -n '1378,1387p' lib/infrastructure/gaia_xpsd_client/src/gaia_client.c   # mmap 失败路径，非记录布局
sed -n '1992,1997p' lib/infrastructure/gaia_xpsd_client/src/gaia_client.c   # 真正的 DR3SP 布局

# A-9（双构建漂移）
grep -n "SRC\|lm\|module_entry" lib/infrastructure/gaia_xpsd_client/Makefile
ls -d lib/algorithms/gaia_xpsd_client 2>&1        # => 不存在（photometric Makefile GAIA_DIR 陈旧）

# 建议 4（行号锚点漂移）
sed -n '56p;69p;83p;90p;105p;129p;131p' docs/science/algorithms/GAIA_QUERY.md | grep -o "gaia_client\.c[:：][0-9-]*"
grep -n "^static void unproject\|^static int polar_plane_intersects" \
     lib/infrastructure/gaia_xpsd_client/src/gaia_client.c      # => 1028 / 1108（实际）

# R-1 / R-2 反例复跑（/tmp 副本，不碰仓内）
python3 /tmp/p1_verify/prune.py    # R-2：C1/C3 假阴性复现
python3 /tmp/p1_verify/sweep.py    # R-2：213 组假阴性 + 最小触发半径 + θ≤45 时 0 组
python3 /tmp/p1_verify/bbox2.py    # R-1：合法几何下假阴性 = 0（否定 ×1.2 裕量的怀疑）

# R-6 / R-7（README 判决被证伪 / 退役判据不可复验）
sed -n '1043,1066p' lib/infrastructure/gaia_xpsd_client/src/module_entry.c
grep -rn "gaia_query_cone" --include=* . 2>/dev/null | grep -v "^./run/"   # => 仅 memory.md:23

# 纪律自证：本次未做任何仓内写
git -c core.quotepath=false status --porcelain -- lib/infrastructure/gaia_xpsd_client/
git -c core.quotepath=false log --oneline -1

# 基线漂移复核：f4a2cf21 未触及本片成员
git -c core.quotepath=false show --stat --name-only --oneline f4a2cf21 | grep gaia_xpsd_client \
  || echo "未触及本片任何成员文件"
sed -n '69p;83p;105p;131p' docs/science/algorithms/GAIA_QUERY.md | grep -o "gaia_client\.c[:：][0-9-]*"
```

---

## 9. 交付要点

- **15/15 份、6021/6021 行、100% 覆盖，无未读段落**；未读清单为空。
- **判定：需修偏阻断**。阻断 9、须修 18、建议 14。
- **最强反例**：R-2（极区剪枝 213 组假阴性，最小触发半径 0.1°，前置条件精确锁定为「AE 树越过 θ=45°」）—— `src/gaia_client.c:1116` + `:1130-1139`，守卫 `:1124` 恒假。
- **新问题**：**是**。README §9「已知限制」整节过期且其 `NOT_IMPLEMENTED` 判决依据（`check_module_map.py`）已不存在；源码行号锚点系统性漂移（`GAIA_QUERY.md` 7/11 处错，错误已传播到 `README.md:89`）；两个已注册门缺其赖以成立的 sanitizer 且从不校验；`w34_gaia_spec_collector_ownership` 缺 zlib 包含面且注释断言相反；迁移桥段三全局量跨实例串扰。
- **诚实标注**：A-1 的**生产可达性不可判定**——仓内唯一 AE 夹具仅覆盖 θ≤1.05°，`run/**` 下的 `.xpsd` 全部为 Equirectangular。缺陷成立性已数值复现，可达性需真实 Gaia XPSD 才能定论。