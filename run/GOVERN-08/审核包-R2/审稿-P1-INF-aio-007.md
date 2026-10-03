# 审稿-P1 · INF-aio-007 · G08-05 对抗审稿第 1 遍

- **片号**：`INF-aio-007`（层 `lib/infrastructure/aio`）
- **基线**：任务书给定 HEAD=`850a9ede`；**实测 HEAD=`1fa477a7a05c315550df2ce2bafc3e64ed9bbd78`**（领先 2 个提交：`bf25c085` 把文档权威层与「实验域不留运行结果归档」对齐、`1fa477a7` 移除实验域运行结果归档）。**本片 28 个成员文件在工作树中均无改动**（`git status --porcelain` 的 11 条改动全部在 `docs/science/*.md`，与本片无交集）。两提交均未触及本片文件，故按 `1fa477a7` 审读有效。
- **纪律**：零 git 写；未编译、未跑 ctest/pytest/未跑任何二进制；未改动仓内任何文件（唯一写入为本交付件）；中文路径一律 `git -c core.quotepath=false`。
- **计数口径**：见 §1，分「本人亲读」与「子代理代读」两档，**不合并粉饰**。

---

## 1. 读完了吗

**成员份数 28 / 成员总行数 10635**（与权威清单 `片清单-权威版.yaml:3128-3130` 的「成员份数: 28 / 实际行数: 10635」逐字相符；本人对 28 个文件逐个 `wc -l` 复核，合计 **10635**，与清单零偏差）。

### 1.1 诚实结论：**未达到「本人逐字读完 28/28」**

| 档位 | 份数 | 行数 | 占比 |
|---|---:|---:|---:|
| **本人完整逐行读完** | 7 | 2763 | 26.0% |
| **本人部分精读（定位式取证）** | 2 | ~191 / 1594 | 1.8% |
| **本人未读，仅子代理代读** | 19 | 6278 | 59.0% |
| 合计 | 28 | 10635 | 100% |

**本人完整逐行读完的 7 份**（`read` + offset/limit 逐行，无 grep-only 结论）：
`src/aio_pipeline.cpp`(1512)、`src/aio_pipeline_engine.cpp`(629)、`tests/abi/aio_abi_layout_probe.cpp`(211)、`tools/aio_abi_mirror.py`(229)、`include/aio_ahpx_format.h`(71)、`src/ahpx/DEPRECATED.md`(8)、`tests/pipeline_frame_contract_test.cpp`(103)。

**本人部分精读 2 份**：`io/hips_core.c`（读 890-1011，定位空指针解引用）、`tests/p1hips/p1hips_tests_properties.cpp`（读 100-139、228-257，定位断言语义）。

### 1.2 未读完的 19 份（如实列出）

`tests/hiss_experiments.cpp`(1495)、`tests/test_writer_integration.cpp`(810)、`tests/p2hips/p2hips_unc_prov_test.cpp`(669)、`healpix_db/archive/legacy/healpix_stack/tests/test_healpix_stack.py`(509)、`io/fits_verify.py`(431)、`include/aio_hips.h`(384)、`tests/v5_maptile_oracle.py`(313)、`runtime/artifact_store/phase_product_exchange_validator.py`(304)、`.../gradient/spherical_spline.cpp`(289)、`tests/results/performance_report.md`(261)、`.../gradient/spherical_spline.h`(173)、`.../hp_stack_api.h`(145)、`src/hiss_transform.h`(132)、`.../healpix_core.h`(98)、`product_io/include/astro/aio/bunit.h`(88)、`.../README.md`(79)、`product_io/include/astro/aio/product_io.h`(48)、`src/aio_util.h`(28)、`healpix_db/.gitignore`(22)。

### 1.3 分片整体覆盖（代读档，**可信度低于亲读档**）

6 个子代理各自对所派文件**自报 100%**（合计 2141×2 + 1293×2 + 4184 + 2505 = 13559 行次，含两组重复盲读）。**故 28/28 文件、10635/10635 行在「本人或受控子代理」口径下被读遍。**
⚠️ 但按纪律第 1 条「结论必须来自你自己读完原文」，**下述所有标【代读】的条目仅作线索登记，未经本人逐字复核**；本人逐字读过并亲自复算的条目标【亲验】。**最重 3 条与全部阻断/须修结论均为【亲验】**（见 §5）。

---

## 2. 本片判定

### 判定：**需修**（无「阻断」级生产事故；但存在 1 条进程级崩溃面与多条恒真/自指门）

> 定级说明：本片唯一「阻断」候选 `hips_core.c:904` 虽可从公开 C ABI 触发崩溃，但该 TU **未进任何生产 target**（见 §3 阻断候选 A 的诚实边界），故不上「阻断」。真正吃掉「阻断」的是**治理层断裂**：一整片生产源码引用不存在的设计条款与不存在的机器判据，使「生产调用点=0」这类**可追溯性主张全部失去强制力**——而这正是本项目已实测过一次的退役对象伪造模式的复刻。

### 最重 3 条

**【重1 · 亲验】伪引 + 悬空引用：整组「非生产/诊断」降级登记引用一段仓内不存在的设计条款**
`src/aio_pipeline.cpp:938,1167,1278,1320,1367,1388`（6 处）与 `include/aio_pipeline.h:24-31`（2 处）反复以「设计逐字」引用 `docs/ACSD_DESIGN §9「块↔文件的导出/缓存接口不是生产接口」……**禁止**任何阶段内节点用它们搬运数据」，并声明强制力来自「机器判据 `eng/ci/check_aio_io_boundary.py`」。
三重证伪（本人亲跑）：① `grep -n "^## " docs/ACSD_DESIGN.md` → `530: ## 9. CPU 后端与资源`、`545: ## 10. I/O 与原子产品`，**§9 与被引主题无关**；② 对 `搬运数据|缓存接口|导出/缓存|块↔文件|存成缓存文件再读回|非生产|DIAGNOSTIC-ONLY` 七个特征词在 `docs/ACSD_DESIGN.md` 内检索，**各 0 命中**；③ 全仓 `grep -rn "搬运数据"` **只命中引用它的那两个文件本身**（`aio_pipeline.h:26,28,225` + `aio_pipeline.cpp` 6 处），**任何文档目录 0 命中**。
④ 判据脚本 `eng/ci/check_aio_io_boundary.py` **不存在**：`ls -d eng/ci` → 无此目录（`eng/` 实为 build/cmake/contracts/packaging/tests/tools）；`find` 显示同名脚本仅存于 `run/FINAL-07-e2e/bisect/**` 的**历史工作副本**。
⇒ 这是本项目已在 `f9650dd0`「清除运行期文案里从未存在于被引条款的伪引」修过一次的**同类复发**，且这次被用来支撑「禁止搬运数据」这条**边界纪律**。
诚实记录：`docs/engineering/UNRESOLVED_REGISTER.md:1599-1600` **已登记**该判据不存在（GOV-ERR-4）；但**代码侧 6+2 处引用仍原样保留**，且登记表未记「伪引」这一半。分类本身是对的（6 个接口的非 aio 调用点确实只有测试，已亲验），**错的是引用**。

**【重2 · 亲验】同义反复的 ABI 门：三个 `sizeof` 既是被检量又是期望量（本轮最有价值的「自指式断言」）**
生产者 `aio_pipeline.cpp:228-230` 填 `(uint32_t)sizeof(PipelineFrame/AioBlock/AioKVEntry)`；消费者 `orchestrator.cpp:762-764` 比的**也是同一头文件的同一表达式**：
```cpp
abi->pipeline_frame_size == sizeof(PipelineFrame) &&
abi->aio_block_size      == sizeof(AioBlock) &&
abi->aio_kv_entry_size   == sizeof(AioKVEntry)
```
本人独立推导：往 `AioBlock` 加一个字段，两侧**同时**改变 ⇒ 该「结构不匹配硬失败」门在单一头文件构建下**构造上不可检出**，退化为 `sizeof(X)==sizeof(X)`。
更致命的是本该兜底的 `enum_fingerprint 0x5A1C0001u /* 冻结 */`（`aio_pipeline.cpp:227`）：本人亲跑 `git grep -rn "enum_fingerprint"` → **全仓仅 2 命中 = 声明 + 初始化，从未被任何人读取或比对**；`abi->struct_size` 亦只出现在 `orchestrator.cpp:772` 的日志字符串里，从不参与判定。即 5 个门字段里 4 个自指或未检，**仅 `abi_version == 1` 是真正的独立常量比对**。
诚实边界：跨版本陈旧 DLL 与 32/64 位指针宽度差异仍能挡住，故不是全无价值。

**【重3 · 亲验】公共 C ABI 在参数校验前解引用句柄 —— fail-closed 声明被自身的解引用击穿**
`io/hips_core.c:904` `int64_t tw = h->tile_width;` **执行于** `:910` `if (!h || !out || !out_got) return ACS_HIPS_ERR_PARAM;` **之前**。两个公开封装 `acsd_hips_read_tile_plane_f32_v1`(`:997`) / `_f64_v1`(`:1005`) 把 `h` 原样透传、零校验。
本人亲验：该文件其余 9 个入口（`:793,808,834,840,846,852,864,877`）**一律先查 `!h`**；本函数是唯一在初始化块里解引用句柄者。`:910` 的空指针守卫对 `h==NULL` **是死代码**。
反例（可直接构造，无需编译即可判定）：`acsd_hips_read_tile_plane_f32_v1(NULL, 0, buf, 128, &got, err, sizeof err)` ⇒ 在 `:904` 读 `NULL->tile_width` 段错误；按本文件自身契约应得 `ACS_HIPS_ERR_PARAM`。
诚实边界（必须写明）：`hips_core.c` **未进任何生产 target**，仅被 `eng/tests/unit/CMakeLists.txt:1727-1728` 以自测夹具直编（该处 `:1726` 自述「hips_core.c 未进任何生产目标」是 W1 族缺口）。故这是**潜伏崩溃面**而非线上事故，但它是公开 C ABI 上「每个失败都产生稳定错误码」这条纪律的**直接反例**，且修法是一行。

---

## 3. 逐文件清单（读了什么 / 看到什么 / 判定）

图例：**【亲验】**=本人逐字读完并亲自复算；**【亲验·定位】**=本人定位精读；**【代读】**=子代理结论，本人未逐字复核。

| # | 文件 | 读了什么 | 看到什么 | 判定 |
|---|---|---|---|---|
| 1 | `src/aio_pipeline.cpp` (1512) | 全文逐行 | ABI 自指门(228-230)、`enum_fingerprint` 无人读(227)、6 处伪引+悬空判据(938/1167/1278/1320/1367/1388)、FITS 导出 `bzero` 恒假分支(1409/1469/1501) + `fwrite` 返回值全忽略(1476/1488/1490/1495)、`validate` 不校 `count==∏dims`(194-217) | **须修**（含重1、重2） |
| 2 | `src/aio_pipeline_engine.cpp` (629) | 全文逐行 | **未注册阶段=返回成功**(195-201)、`run_batch` 返回 `n_success` 且 STACK 失败只记日志不递减(566-594)、`omp_set_num_threads` 进程级全局副作用(518)、硬编码 16 线程(503)、`mem_after-mem_before` size_t 下溢(213)、`debug_skip_pixels` 被吞(177)。**外加**：本人亲验本 TU **不在任何 CMakeLists/Makefile**（`git grep -rn "aio_pipeline_engine" -- '*CMakeLists.txt' '*.cmake' 'Makefile*'` → 0 命中），而 `aio_pipeline.cpp` 在 `CMakeLists.txt:624` | **建议（随孤儿 TU 处置）** |
| 3 | `io/hips_core.c` (1011) | 定位 890-1011 | `:904` 空指针解引用先于 `:910` 守卫 | **须修（重3）** |
| 4 | `tests/abi/aio_abi_layout_probe.cpp` (211) | 全文逐行 | `offsetof/sizeof` 全部由编译器求值、**非硬编码**，`static_assert` 锁 5 结构首部偏移 0/4 + 4 个版本常量 → **是真门不是恒真门**；但 `AIO_HIPS_VERIFY_REPORT_ABI_VERSION` 无对应 `static_assert`(39-42 只覆盖 4 个) | 通过（1 处建议） |
| 5 | `tools/aio_abi_mirror.py` (229) | 全文逐行 | ctypes 逐字段镜像，与探针构成**双向独立**（非自指）；`ACS_HIPS_MAX_FRAME_SCALE_ARCSEC=824.5167388361774`(:37) 为生产校验界(`aio_hips_writer.cpp:2190`)，**探针不发射该常量 ⇒ 不在 ABI 锁覆盖内**，C/Python 两份可静默分叉 | **建议** |
| 6 | `include/aio_ahpx_format.h` (71) | 全文逐行 | `HEADER_FIXED_SIZE=18` 与注释加法自洽(4+2+4+4+4)✔；`:21` 引「docs/ACSD_DESIGN §2.1 全程只有 SNR」——**本人亲验该表述实际在 `ACSD_DESIGN.md:182`（§3 区段），§2.1 是「P1 通量积分拟合」并未如此表述** ⇒ 又一处节号漂移；`hasJsonKey`(:37-50) 注释自陈「不匹配值中的同名子串」，但 JSON 值内 `\"weight\"` 转义串会假阳性 | **建议** |
| 7 | `src/ahpx/DEPRECATED.md` (8) | 全文逐行 | `:6` 指名的替代模块 `healpix_db/healpix_io/` 亲验**已归档、目录内只剩 `ARCHIVED.md`**，且符号名已改名 `hiss_write→aio_hiss_write`；**路径与符号名双悬空** | **须修（悬空引用）** |
| 8 | `tests/pipeline_frame_contract_test.cpp` (103) | 全文逐行 | `:36` `sizeof(AioKVEntry)==320`、`:38` 枚举值 —— **对硬编码字面量比对，是真门非恒真门**；`:79` 传 `count=1` 而 `dims=[4]`，校验器接受 ⇒ **形状元数据可以说谎**；`:64` 固定 `/tmp` 路径且从不 unlink ⇒ 非 hermetic；`:15` CHECK 不中止，`:70` 在 `:69` 失败后仍解引用 `gd` | **须修**（另见重1 之死门） |
| 9-28 | 其余 19 份 | 见 §1.2 | 见 §4【代读】条目 | 见 §4 |

**另亲验（不在本片、作为取证）**：`aio_pipeline.h:62-67`（`AIO_CACHE_MAX_DIMS=4` ⇒ 亲手**推翻**了「`:1077 int dims[4]` 栈溢出」假设，**不予立案**）、`tests/CMakeLists.txt:1-26`、`CMakeLists.txt:618/624`、`healpix_io/ARCHIVED.md`、`ACSD_DESIGN.md` 标题表与 `:182`、`aios_abi/CMakeLists.txt`（锁与 selfcheck **确已注册**）。

---

## 4. 发现清单

### 4.1 阻断

**无。** 唯一候选（`hips_core.c:904`）因该 TU 未进生产 target 而定为**须修**。理由与证据见 §2 重3、§3 第 3 行。

### 4.2 须修

| # | 位置 | 问题 | 依据 |
|---|---|---|---|
| S1 | `aio_pipeline.cpp` 6 处 + `aio_pipeline.h` 2 处 | 伪引（引仓内不存在的设计条款）+ 悬空机器判据 | §2 重1，4 条亲跑命令 |
| S2 | `aio_pipeline.cpp:228-230` × `orchestrator.cpp:762-764`；`:227` | ABI 门 `sizeof(X)==sizeof(X)` 同义反复；`enum_fingerprint` 全仓无人读 | §2 重2 |
| S3 | `hips_core.c:904` | 公开 C ABI 空指针解引用先于 `:910` 守卫 | §2 重3 |
| S4 | `src/ahpx/DEPRECATED.md:6` | 替代路径已归档、符号名已改名，路径与符号双悬空 | 亲验 |
| S5 | `aio_pipeline_engine.cpp:195-201` | handler 未注册 ⇒ **跳过并返回 0（成功）**；显式请求的 `[from,to]` 区间静默不执行 | 亲读 |
| S6 | `aio_pipeline_engine.cpp:566-594` | STACK 阶段失败只 `fprintf`、**不递减 `n_success`**；`run_batch` 返回 `n_success` ⇒ 按 `:84` 契约「全部成功==n_frames」，STACK 全失败仍被读成全成功 | 亲读 |
| S7 | `aio_pipeline_engine.cpp:503/518` | `omp_set_num_threads` 改**进程级全局**，硬编码 16 线程无 config/无推导（违 AGENTS §6「不私建线程池」） | 亲读 + `ACSD_DESIGN.md:535` |
| S8 | `aio_pipeline.cpp:1409-1410/1469/1501` | `bzero` 初始化为 0.0 后**再不赋值** ⇒ `if (bzero != 0.0)` 是**恒假门**，BZERO 卡永不写出（有符号 int32/int64 负值被读成巨无符号） | 亲读 |
| S9 | `aio_pipeline.cpp:1476/1488/1490/1495` | FITS 导出**忽略全部 `fwrite` 返回值却 `return 0`**；对照同文件 `:1301-1303` 的 XML 导出是检查的 ⇒ 磁盘写满仍报 ok | 亲读 |
| S10 | `tests/CMakeLists.txt:17` | `pipeline_frame_contract_test` 以「需 `aio_pipeline_frame_*` 符号」为由排除；**本人亲验该理由为假**——这些符号存在且 `aio_pipeline.cpp` 已编入 `acsd_aio`(`CMakeLists.txt:618,624`)。它是事务性失败加载/移动所有权/块名注册策略的唯一门 | 亲验 |
| S13 | `aio_pipeline.cpp:1140→1146→1153` | **块数据缓冲泄漏**：`:1140 blk->data = buf` 已接管堆缓冲；`:1146` 描述字段校验失败即 `break`，`:1148` 的 `n_blocks++` **未执行**；失败清理 `:1153 for (i=0; i<frame->n_blocks; ++i)` 覆盖 `[0, i)`，**恰好排除当前下标 i**，该缓冲永不回收。单次泄漏上限 `AIO_CACHE_MAX_BLOCK_BYTES`=4 GiB。对照同函数 KV 分支 `:1128` 显式 `std::free(entries)` 后再 break ⇒ 证明这是**非 KV 分支独有的疏漏**，非有意设计 | **亲验**（读 1139-1160） |
| S14 | `aio_pipeline_engine.cpp:506-507/539/545` | **零执行报成功**：`:507 pre_stack_end = has_stack ? (STAGE_STACK-1) : to_stage`；`:496` 的参数校验只查 `from_stage<=to_stage`，**未查 `from_stage <= pre_stack_end`**。反例：`run_batch(eng, frames, n, t, from_stage=4, to_stage=4)` ⇒ `pre_stack_end=3`，`:539` 循环 `s=4; s<=3` **一次不执行**，`ret` 保持初值 0 ⇒ 每帧 `n_success++`，逐帧打印「pre-stack success」，而**没有任何 handler 被调用过**；`:575` 的 DRIZZLE 闸门又把所有帧滤掉，整批「成功」而零工作 | **亲验**（读 482-604） |
| S11 | `p1hips_tests_properties.cpp:115-116` | **筛掉真信号**：`:115` 的 `break` 先于 `:116` 执行 ⇒ signal 一旦失配即跳出，`bit_sup` 永不变假，`:119` 的 `i7_hier_sup_bitwise` **无条件报 PASS** | **亲验** |
| S12 | `p1hips_tests_properties.cpp:244` | **自指式断言**：`aio_hips_writer.cpp:1846` 自陈 `fmt_sky_fraction` 是 moc_sky_fraction 的「**唯一**」格式化函数，`:2331` 算一次 `moc_frac`，`:1946`/`:2109`/`:2456` 两面同函数同变量 ⇒ `it->second == mlit` 即 `f(x)==f(x)`。测试注释 `:234-237` 自陈「唯一格式化函数 ⇒ 字面量逐字符相等」，**恰好记录了使其不可证伪的那次重构** | **亲验**（writer + test 两端均亲读） |

### 4.3 建议（择要；【代读】者未逐字复核，登记为线索）

- `aio_pipeline_engine.cpp:213` `(double)(mem_after - mem_before)`：`size_t` 相减，阶段内释放内存即下溢成 ~1.8e19 打印。【亲验】
- `aio_pipeline_engine.cpp:177` `debug_skip_pixels` setter 存了但导出路径无此参 ⇒ 参数被吞。【亲验】
- `aio_pipeline_engine.cpp:196` 未注册阶段同时**不置 `stages_completed` 位**，返回码却绿。【亲验】
- `aio_ahpx_format.h:21` §2.1 节号漂移（实际表述在 `ACSD_DESIGN.md:182`）。【亲验】
- `tools/aio_abi_mirror.py:37` `ACS_HIPS_MAX_FRAME_SCALE_ARCSEC` 是生产校验界却不在 ABI 锁内，CP 两份可分叉。【亲验】
- `pipeline_frame_contract_test.cpp:79` `count` 与 `dims` 不一致被接受；`:64` 固定 `/tmp` 不清理。【亲验】
- 【代读·待复核】`fits_verify.py:396-398` CHECKSUM 含 `or computed==0/0xFFFFFFFF` 无条件放行；`:386` 畸形 CHECKSUM 卡静默跳过全部校验。
- 【代读·待复核】`phase_product_exchange_validator.py:121` `!= 1` 接受 Python `True`（`True == 1`），而同文件 `:133` 已显式排除 bool ⇒ 漏改。
- 【代读·待复核】`bunit.h:62` `quadratic_law_holds` 只比量纲幂次、不比数值；ivar 为空时跳过 ivar 半边。
- 【代读·待复核】`healpix_stack/tests/test_healpix_stack.py` 全 7 例因 `healpix_stack` 模块不存在而**永久 skip**（`find` 全仓无此文件）⇒ 恒绿门。
- 【代读·待复核】`v5_maptile_oracle.py` 零调用者；`:302` 的 `ok` 不含 `mismatch_tiles` ⇒ 全 tile 形状错也判 PASS；`:239` `np.empty` 未初始化即参与比较。
- 【代读·待复核】`p2hips_unc_prov_test.cpp:79-84` `read_file` 失败返回 `""` ⇒ `:631` 的 `detected` 因「文件整个不存在」而恒真。
- 【代读·待复核】`hiss_experiments.cpp` 未注册 ctest 且素材目录不存在，`main():1474` 必返 1。

---

## 5. 我主动构造的反例

| # | 构造 | 期望推翻 | 结果 |
|---|---|---|---|
| **CE-1** | 往 `AioBlock` 增删一个字段，重跑 ABI 握手 | 期望「结构不匹配硬失败门」转红 | **推翻成功**。两侧同读 `aio_pipeline.h`，`sizeof` 同变 ⇒ 门恒绿。**该门在单一头文件构建下对它自述要抓的缺陷构造上不可检出。** |
| **CE-2** | 令 `dlsym` 注册 5 个阶段中的 1 个返回 `nullptr`，调 `run_single(0, STAGE_STACK)` | 期望整条管线拒绝执行 | **推翻成功**。`aio_pipeline_engine.cpp:196-201` 打印「skipped (no handler)」后 **`return 0`**，`run_single:466` 再打印「success」。测光零点从未施加、返回码全绿。 |
| **CE-3** | 8 帧 pre-stack 全成功 + STACK handler 每帧返 `-1` | 期望 `run_batch` 报告失败 | **推翻成功**。`:580-582` 只 `fprintf`，`n_success` 不递减，`:594 return n_success` 仍 = 8 ⇒ 按 `aio_pipeline_engine.h:84`「全部成功则 == n_frames」被读成全成功，而产品从未生成。**并附**：pre-stack 全失败时返回 **0**，与同族 `run_single` 的 0=成功撞义。诚实边界：本人亲验 `run_batch` **当前零调用者**，故为潜伏面。 |
| **CE-4** | 调 `acsd_hips_read_tile_plane_f32_v1(NULL, …)` | 期望得 `ACS_HIPS_ERR_PARAM` | **推翻成功**（`:904` 解引用先于 `:910` 守卫 ⇒ 段错误）。 |
| **CE-5** | 把 `properties` 与 `manifest.json` 两个面的 `moc_sky_fraction` 改成由**不同**格式化函数产出 | 期望 `i6_sky_fraction_double_face` 转红 | **反例不成立——这正是问题所在**。因 writer:1846 起两点已共用 `fmt_sky_fraction` 唯一函数，**构造不出分叉**：该断言**可证伪性为零**。（对照组：`:249` 的 `i6_sky_fraction_roundtrip` 比的是字面量 `3.0/12.0`，是真门。） |
| **CE-6** | 只破坏 support 通道的 bit 值、不动 signal | 期望 `i7_hier_sup_bitwise` 转红 | **推翻成功**。但**先破坏 signal** 通道时 `:115` 的 `break` 先跳出，support 从此不再被检查，`:119` **无条件 PASS** ⇒ support 缺陷被 signal 缺陷掩护。 |
| **CE-7** | 在 `docs/` 下搜被引的「设计逐字」条款 | 期望能找到权威出处 | **推翻成功**。七个特征词在 `docs/ACSD_DESIGN.md` 各 0 命中；全仓「搬运数据」只出现在引用它的那两个源文件里 ⇒ **引用是伪造的**。 |
| **CE-8** | 核对 `deps` 中「生产调用点 = 0」是否有强制力 | 期望存在机器判据 | **推翻成功**。`eng/ci/` 目录不存在；同名脚本仅存于 `run/FINAL-07-e2e/bisect/**` 历史工作副本。**但分类本身为真**（6 个接口非 aio 调用点确实只有测试）。 |
| **CE-9（亲手推翻自己的假设）** | 假设 `load_cache_parse` 的 `int dims[4]` 会被 `n_dims>4` 撑爆成栈溢出 | 期望找到可写越界 | **假设被推翻，撤销该发现**。`aio_pipeline.h:63` `AIO_CACHE_MAX_DIMS=4`，与 `:1076` 上界、`:1077` 数组长度**精确吻合**；`:422` 的 `n_dims>4` 钳位是死代码而非静默截断。**不予立案。** |
| **CE-10（推翻子代理的一条发现）** | 独立核对子代理「上游 provenance URL 已死」 | —— | **采纳子代理的自我撤回**。该代理自查发现本沙箱对**任意** github.com 仓库（含必然存在的 `scikit-spatial`）一律返回**字节相同的 404 通用页**，故原结论无信息量，**已撤回**。本人复核其撤回逻辑成立。 |
| **CE-11** | 构造 `.aio`：最后一个块的 name/type/count/dims/数据全合法，唯独 `description` 写 200 字节（>127） | 期望 `load_cache` 干净失败且不泄漏 | **期望成立但实现有洞**。`:1146` 正确判失败并返回 3（fail-closed 对），但 `:1140` 已接管的堆缓冲因 `:1153` 清理循环 `i < n_blocks` 排除当前下标而**永久泄漏**。单次上限 4 GiB。 |
| **CE-12** | `run_batch(eng, frames, 4, 4, from_stage=STAGE_STACK, to_stage=STAGE_STACK)` | 期望要么真正执行 STACK，要么拒绝参数 | **推翻成功**。`:496` 只校验 `from<=to` 不校验 `from<=pre_stack_end` ⇒ `:539` 循环空转 ⇒ `ret` 保持 0 ⇒ 每帧计入 `n_success`，日志打「pre-stack success」，返回 4/4 全成功而**零 handler 被调用**。 |

---

## 6. 盲复算（遮住既有判定独立取证）

方法：先读 `docs/engineering/UNRESOLVED_REGISTER.md` 与既有审稿产物仅作**线索**，对每条关键结论**遮蔽其既有判定**，用本地命令从零取证；子代理结论一律**重新亲验**后才采信。

| 待验结论 | 既有判定 | 独立取证 | 判定 |
|---|---|---|---|
| `enum_fingerprint` 是有效 ABI 保护 | 注释自称「冻结」，读起来像在保护 | `git grep -rn "enum_fingerprint"` → 全仓 2 命中（声明+初始化），**无任何读取点** | **偏松**（既有判定过于乐观，我判更重） |
| ABI 握手能抓结构不匹配 | 自称「硬失败」 | 比较生产者 `:228-230` 与消费者 `:762-764` 的表达式来源，同一头 ⇒ 同义反复 | **偏松** |
| 「生产调用点 = 0」由机器判据强制 | 注释如此声明 | 判据脚本不存在；非 aio 调用点只有测试 | **一致**（分类对、引用错） |
| `AIO_CACHE_MAX_DIMS` 可能致栈溢出 | 我初始假设 | `aio_pipeline.h:63` = 4，与 `:1076`/`:1077` 精确吻合 | **我方偏严，已自行撤销** |
| `hips_core.c` 每入口先查 `!h` | 通读印象 | 逐入口核 `:793…877` 九处均先查，唯 `:904` 例外 | **偏松**（既有印象漏一处） |
| `read_str_with_len_strict` 会静默截断 | 名字含 strict，易疑 | `:907-914` 超长返回 -2 硬失败、调用方 `:1069` 转 `failed` ⇒ **真 fail-closed** | **一致（判其正确）** |
| `strncmp(...,64)` 是魔数 | 易疑为硬编码 | `AIO_BLOCK_NAME_MAX=63` + `name[64]` ⇒ 64 精确正确 | **一致（判其正确）** |
| `hiss_transform.h:46` 的 include 悬空 | 子代理初判疑悬空 | 经 `-I` 解析到在册的 `include/hiss_format.h` | **偏严（子代理已自行撤回）** |

**总体**：既有判定对本片**整体偏松**——三处「硬失败/强制力」的自述在独立重算后被推翻；而我方也**自行撤销 1 条**（栈溢出假设）与**采信 1 条子代理撤回**（provenance 404）。无「偏严」误伤。

---

## 7. 子代理派发记录

**派发 6 个（去重后 4 个独立任务 + 2 个刻意重复盲读）**。全部只读、零写入、零编译零运行。

| ID | 任务 | 覆盖 | 回报 |
|---|---|---|---|
| `14d1d351` | `aio_pipeline.cpp` + `aio_pipeline_engine.cpp` | 2141 行 100% | 17 条（B1–B17），含 ABI 自指门、**缓存块数据泄漏**、**零执行报成功** |
| `5ec14d80` | **同上（刻意重复盲读）** | 2141 行 100% | 独立复现同 14 条 |
| `a54b0687` | legacy healpix_stack 6 件 + h 因子 | 1293 行 100% | **主动撤回 1 条**（D7） |
| `97c0f8ee` | **同上（刻意重复盲读）** | 1293 行 100% | 同结论：h 因子**不适用** |
| `c9a3858e` | 7 件测试/预言机 | 4184 行 100% | 自指式断言 7、死门 4、筛信号 1 |
| `259e0914` | IO 核心 10 件 | 2505 行 100% | 1 阻断候选 + 8 须修 + 12 建议 |

### 逐条复核与**否决**记录

**本人逐条复验后采信（【亲验】）**：`hips_core.c:904` 空指针（读原文 890-1011 确认）、ABI 自指门（读生产者+消费者确认）、伪引/悬空判据（4 条命令确认）、S11 断言语义（读 `p1hips_tests_properties.cpp:100-139` 确认）、S12 自指断言（读 test `:228-257` + writer `:1846/1858/1946/2331/2456` 两端确认）、死门排除理由为假（读 `tests/CMakeLists.txt` + `CMakeLists.txt:618,624` 确认）、**S13 缓存泄漏**（读 `aio_pipeline.cpp:1139-1160` 逐行确认下标排除）、**S14 零执行报成功**（读 `:482-604` 确认循环边界）、S8 `bzero` 恒假门、S9 `fwrite` 全忽略（均在本人 1512 行全文阅读中直接观察）。

**否决/降级的子代理结论**：
1. **否决**「`int dims[4]` 栈溢出」类推论——`AIO_CACHE_MAX_DIMS=4`，我方自行撤销（CE-9）。
2. **否决**「`run_batch` 违反返回契约」——`aio_pipeline_engine.h:84` 明文规定返回成功帧数，**不是契约违反**，降为「同族语义冲突的潜伏面」。
3. **否决**把 `healpix_stack` 退役对象判为「退役声明有活调用者」——两个独立代理各自以 per-symbol `git grep` 证明**零活调用者**，声明为真。
4. **否决** h 因子缺陷——两个独立代理各自独立推导并以 `grep -E '/ *h|\* *h\b'` 零命中证明：`spherical_spline.cpp` 是**双调和球面样条（Wahba/SHM）**，根本无节点间距 `h`，该缺陷类**结构上不可能存在**；`850a9ede` 的修复只落在实验脚本上，**生产代码未被波及**。**明确不为本片开 h 因子条目。**
5. **采信** `a54b0687` 的**自我撤回**（provenance URL 404 无信息量）——其自查控制实验（对必然存在的仓库同样得 404）方法学正确。
6. **降级为线索** 全部【代读】条目（`fits_verify.py` / `phase_product_exchange_validator.py` / `bunit.h` / `v5_maptile_oracle.py` / `test_healpix_stack.py` / `p2hips_unc_prov_test.cpp` / `hiss_experiments.cpp` / `test_writer_integration.cpp`）——本人未逐字复核，按纪律不作为定论。

---

## 8. 自证段（可复跑）

> 全部在 `/workspace/Astro CS Database`，中文路径一律 `git -c core.quotepath=false`。**以下命令均为只读，不写仓、不编译、不运行被审二进制。**

```bash
# S0 基线与行数复核（合计应 = 10635）
git -c core.quotepath=false rev-parse HEAD
wc -l lib/infrastructure/aio/src/aio_pipeline.cpp lib/infrastructure/aio/src/aio_pipeline_engine.cpp \
      lib/infrastructure/aio/io/hips_core.c lib/infrastructure/aio/tests/hiss_experiments.cpp \
      lib/infrastructure/aio/tests/test_writer_integration.cpp \
      lib/infrastructure/aio/tests/p2hips/p2hips_unc_prov_test.cpp \
      lib/infrastructure/aio/tests/p1hips/p1hips_tests_properties.cpp \
      lib/infrastructure/aio/healpix_db/archive/legacy/healpix_stack/tests/test_healpix_stack.py \
      lib/infrastructure/aio/io/fits_verify.py lib/infrastructure/aio/include/aio_hips.h \
      lib/infrastructure/aio/tests/v5_maptile_oracle.py \
      lib/infrastructure/aio/runtime/artifact_store/phase_product_exchange_validator.py \
      lib/infrastructure/aio/healpix_db/archive/legacy/healpix_stack/gradient/spherical_spline.cpp \
      lib/infrastructure/aio/tests/results/performance_report.md \
      lib/infrastructure/aio/tools/aio_abi_mirror.py \
      lib/infrastructure/aio/tests/abi/aio_abi_layout_probe.cpp \
      lib/infrastructure/aio/healpix_db/archive/legacy/healpix_stack/gradient/spherical_spline.h \
      lib/infrastructure/aio/healpix_db/archive/legacy/healpix_stack/hp_stack_api.h \
      lib/infrastructure/aio/src/hiss_transform.h \
      lib/infrastructure/aio/tests/pipeline_frame_contract_test.cpp \
      lib/infrastructure/aio/healpix_db/archive/legacy/healpix_stack/healpix_core.h \
      lib/infrastructure/aio/product_io/include/astro/aio/bunit.h \
      lib/infrastructure/aio/healpix_db/archive/legacy/healpix_stack/README.md \
      lib/infrastructure/aio/include/aio_ahpx_format.h \
      lib/infrastructure/aio/product_io/include/astro/aio/product_io.h \
      lib/infrastructure/aio/src/aio_util.h lib/infrastructure/aio/healpix_db/.gitignore \
      lib/infrastructure/aio/src/ahpx/DEPRECATED.md

# S1 伪引 + 悬空机器判据（重1）
grep -n "^## " docs/ACSD_DESIGN.md | sed -n '9,11p'      # → §9 CPU 后端与资源 / §10 I/O 与原子产品
for p in 搬运数据 缓存接口 导出/缓存 块↔文件 存成缓存文件再读回 非生产 DIAGNOSTIC-ONLY; do
  printf '%s: %s\n' "$p" "$(grep -c "$p" docs/ACSD_DESIGN.md)"; done   # → 全部 0
git -c core.quotepath=false grep -rn "搬运数据" -- .        # → 仅 aio_pipeline.h / aio_pipeline.cpp
ls -d eng/ci                                                 # → 无此目录
find . -name '*aio_io_boundary*' -not -path './.git/*' | head  # → 全在 run/FINAL-07-e2e/bisect/**

# S2 ABI 自指门 + enum_fingerprint 无人读（重2）
sed -n '223,233p' lib/infrastructure/aio/src/aio_pipeline.cpp
sed -n '759,765p' lib/infrastructure/pipeline/orchestrator/cpp/src/orchestrator.cpp
git -c core.quotepath=false grep -rn "enum_fingerprint" -- '*.cpp' '*.h' '*.py'   # → 仅 2 命中

# S3 hips_core.c:904 空指针先于守卫（重3）
sed -n '896,912p' lib/infrastructure/aio/io/hips_core.c
sed -n '997,1011p' lib/infrastructure/aio/io/hips_core.c
grep -n "if (!h" lib/infrastructure/aio/io/hips_core.c          # → 九处先查 !h，904 独缺

# S4 死门：排除理由为假
sed -n '15,18p' lib/infrastructure/aio/tests/CMakeLists.txt    # → 「pipeline_frame_contract_test 需 …符号」
grep -n "aio_pipeline.cpp" CMakeLists.txt                      # → :624 编入 acsd_aio（:618 add_library）
git -c core.quotepath=false grep -n "aio_frame_add_block_move\|aio_frame_save_cache" \
     lib/infrastructure/aio/include/aio_pipeline.h lib/infrastructure/aio/src/aio_pipeline.cpp | head

# S5 孤儿 TU（本 TU 不在任何构建文件）
git -c core.quotepath=false grep -rn "aio_pipeline_engine" -- '*CMakeLists.txt' '*.cmake' 'Makefile*'  # → 0 命中

# S6 降级登记的 6 个接口确无生产调用者（分类为真，引用为假）
git -c core.quotepath=false grep -n "aio_frame_save_cache\|aio_frame_load_cache\|aio_frame_export_all_xml" \
     -- '*.cpp' '*.c' | grep -v "src/aio_pipeline.cpp"

# S7 DEPRECATED.md 指向已归档目录 + 已改名符号
ls lib/infrastructure/aio/healpix_db/healpix_io/            # → 仅 ARCHIVED.md
head -20 lib/infrastructure/aio/healpix_db/healpix_io/ARCHIVED.md   # → 归档日期 + API 改名表

# S8 亲手推翻自己：栈溢出假设不成立
grep -n "AIO_CACHE_MAX_DIMS" lib/infrastructure/aio/include/aio_pipeline.h   # → 4，与 :1076/:1077 吻合

# S9 拒绝为本片开 h 因子条目（两代理独立结论）
git -c core.quotepath=false grep -nE '/ *h|\* *h\b|h \*' \
     lib/infrastructure/aio/healpix_db/archive/legacy/healpix_stack/gradient/spherical_spline.cpp  # → 无命中
git -c core.quotepath=false grep -nE 'SphericalSpline|SplineParams' -- ':!*/archive/*'              # → 无命中

# S10 §2.1 节号漂移
grep -n "全程只有 SNR\|全链没有「权重模式」" docs/ACSD_DESIGN.md   # → :182（在 §3 区段，非 §2.1）

# S11/S12 断言语义（本人亲读）
sed -n '107,120p' lib/infrastructure/aio/tests/p1hips/p1hips_tests_properties.cpp   # → :115 break 掩护 :116
sed -n '238,251p' lib/infrastructure/aio/tests/p1hips/p1hips_tests_properties.cpp   # → :244 自指断言
grep -n "fmt_sky_fraction\|moc_sky_fraction" lib/infrastructure/aio/src/hips/aio_hips_writer.cpp
```

---

## 9. 给前台的处置建议（不含施工，仅建议）

1. **一行修**：`hips_core.c:904` 的解引用移到 `:910` 守卫之后（重3）。
2. **治理优先**：S1 的 6+2 处引用是**虚假可追溯性**。建议统一改指真实出处（`docs/engineering/io/IO_003_ATOMIC_OUTPUT_PUBLISH.md` 与 `ACSD_DESIGN.md` §10），并**二选一**：恢复那两个判据脚本，或删掉「机器判据」四字。当前状态是「声称有强制力而强制力不存在」，比无声明更危险。
3. **自指门专项**：S2 的 ABI 门应把**期望值**改为消费侧独立钉死的常量表（不要从被检头文件推导），否则它永远抓不到自述要抓的缺陷；`enum_fingerprint` 要么接进门、要么删字段。
4. **孤儿 TU 一并处置**：`aio_pipeline_engine.cpp` 629 行不在任何构建文件、9 个导出符号零生产调用，而公共头 `aio_pipeline.h:241-243` 仍称其「唯一在位的内部调用点」。按 AGENTS §6「退役代码从代码库删除，或保留统一注释块写明原因」处置，可一次性带走 S5/S6/S7 与 §4.3 多数条目。
5. **诚实边界**：本片 59% 行数（6278 行 / 19 份）本人未逐字读完，仅由子代理代读且未复核。§4.3 中【代读】条目**不构成定论**，若要据此派单收口，请指派复核。
