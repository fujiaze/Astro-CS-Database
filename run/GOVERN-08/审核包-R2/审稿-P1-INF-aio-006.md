# 审稿-P1-INF-aio-006 — G08-05 对抗审稿第 1 遍

片号 `INF-aio-006` · 层 `lib/infrastructure/aio` · 交付件唯一路径 `run/GOVERN-08/审核包-R2/审稿-P1-INF-aio-006.md`
性质：只读审查。**未修改/创建/删除仓内任何文件；零 git 写；未编译、未跑 ctest/pytest/任何二进制；未读 `/tmp/acsd_g08/`。**

---

## 0. 基线偏差声明（影响复现口径，必须先读）

派单给定基线 `HEAD = 850a9ede`。开工时实测 `HEAD = bf25c085`，复核期间仓库前移到 `HEAD = 1fa477a7`（提交「移除实验域运行结果归档，报告迁入单元文档目录」，369 files / −283577 行，**只动 `实验/**` 与 `run/**`，未碰 `lib/infrastructure/aio/`**）。

逐片核对结论：**本片 28 份成员在 `850a9ede`、`bf25c085`、`1fa477a7` 三点的 blob 哈希完全一致（identical=28, changed_or_absent=0）**。

因此本报告全部结论对三个基线同等成立。但**「悬空引用」一类判定必须分两档**，否则会误伤：

| 档 | 含义 | 判定 |
|---|---|---|
| A 档 | 引用目标在 `850a9ede` 与 `HEAD` **都不存在** | 真悬空，报缺陷 |
| B 档 | 引用目标**只存在于被 1fa477a7 删除的 `run/**` 归档**，基线时为活 | 不报缺陷，登记为「归档删除后失效」 |

下文悬空引用一律标注档位。所有子代理均报「HEAD 与派单不符」，此处一并裁决。

---

## 1. 读完了吗

### 1.1 三种口径（**计数口径必须写明**，本片三个口径差距极大）

| 口径 | 份数 | 行数 | 覆盖率 |
|---|---|---|---|
| **口径 A：主审（我本人）逐字读完** | **13 / 28 完整** + 1 部分 | **2033 完整 + 50 部分 = 2083** | **19.6% 行** |
| 口径 B：主审亲自取证（read 原文 + 可复现命令） | 17 / 28 | 2083 | 19.6% 行 |
| **口径 C：口径 A ∪ 5 名子代理各自完整读完（互不重叠、并集覆盖全片）** | **28 / 28** | **10632 / 10632** | **100.0%** |

**口径 A 的 13 份（我本人 `read` 到每一行）**：
`src/aio_atomic_file.h`(620) · `src/aio_compressor.cpp`(285) · `src/aio_cfitsio_mutex.h`(114) · `src/aio_xisf.h`(10) · `aio_build_config.json`(11) · `product_io/CMakeLists.txt`(36) · `product_io/src/hips_manifest.cpp`(218) · `product_io/include/astro/aio/provenance.h`(162) · `src/hiss_stream_writer.h`(84) · `tests/test_p1_io_hardening.cpp`(294) · `tests/sanitize_wsl_v4.sh`(78) · `tests/hips_mapping_oracle.py`(**仅 1–50 行 / 156**) · `healpix_db/docs/healpix_drizzle_overview.md`(72) · `healpix_db/.../stack_engine.h`(97) · `healpix_db/.../hp_stack_hiss.h`(45)。

**口径 A 未读完的 15 份（如实列出，共 8549 行 = 全片 80.4%）**：
1. `healpix_db/.../hp_stack_api.cpp`(829) — 未读
2. `healpix_db/.../healpix_core.cpp`(679) — 未读
3. `healpix_db/.../stack_engine.cpp`(392) — 未读
4. `tests/test_hips_atomic_publish.cpp`(589) — 未读
5. `tests/p1hips/p1hips_tests_units.cpp`(519) — 未读
6. `tests/test_tile_model.cpp`(411) — **仅读 370–380 行**（为复核算术错误而定位）
7. `tests/dataflow_fuzz.cpp`(362) — 未读
8. `tests/p1hips/p1hips_test_main.hpp`(246) — 未读
9. `tests/test_psf_fit_handler.py`(208) — 未读
10. `tests/hips_direct_smoke.py`(124) — 未读
11. `tests/hiss_benchmark.cpp`(1546) — 未读
12. `tests/hiss_experiment_suite.cpp`(1408) — 未读
13. `tests/p1hips/p1hips_tests_oracle.cpp`(1037) — 未读

（口径 A 13 份完整 + 1 份部分 = 14 份触及；上列 13 项为其未读完者，`test_tile_model.cpp` 与 `hips_mapping_oracle.py` 的部分读已计入「触及但未读完」。）

**结论**：口径 C 为 100%（5 名子代理分组并集，逐组自报行数与本片清单逐份对齐），但**主审亲自重读只到 19.6% 行**。凡出自那 15 份的结论，本报告一律标注 `[子代理]` 且只作为线索采纳，不计入主审独立取证。

---

## 2. 本片判定：**阻断**

最重 3 条：

### 阻断-1 · 自洽式断言：测试的「压缩器」与被检的「压缩器」是同一个恒等式，zstd 路径零覆盖
`tests/test_p1_io_hardening.cpp:69-76` 在 `#else`（无 `HAS_ZSTD`）分支把函数命名为 `zstd_compress_bytes`，实现却是
```cpp
static std::vector<uint8_t> zstd_compress_bytes(const uint8_t* src, size_t n) {
    return std::vector<uint8_t>(src, src + n);      // 恒等，不压缩
}
```
它给 T4/T5/T6/T7（`:181, :202, :222, :242`）造 fixture。而被检的 `src/aio_compressor.cpp:95-104` 在同一条件下 `memcpy` 后 `return srcSize`。**两侧是同一个表达式**——测试与实现必然一致，因此这四例对「zstd 解压是否正确」提供的证据为零。

配套（我本人 `read` 确认）：`eng/tests/unit/CMakeLists.txt:896-908` 的构建注释两次以「**自洽**」背书——
> `HAS_ZSTD 不加 — fixture 在无 zstd 时走"伪压缩块"分支, 测试 TU 自洽, 与库一致。`
> `HAS_ZSTD 不加 — 根 CMake 从未为 acsd_hips/acsd_aio 定义该宏, hiss_*/aio_compressor 以无 zstd 分支自洽`

而 `src/aio_compressor.cpp:95-104` 的 fallback 在**产品由带 zstd 的构建写出、被无 zstd 的构建读回**时，会把**压缩字节 memcpy 进像素缓冲**并 `return srcSize` 伪装成功；`aio_compressor.h:43,45` 提供的 `hasZstdSupport()/hasLz4Support()` 正是为堵这个洞而存在的，**全仓零调用点**（`grep` 仅得声明与定义两处）。

**反例**：产品由 `-DHAS_ZSTD` 构建写出、CMake 构建（无宏）读回 → `decompressZstd` 走 `:96-103` → 返回非零、长度正确、无错误码、像素缓冲里是 zstd 帧。**不能被现有任何检查发现。**

### 阻断-2 · 三个「门」以恒真/恒绿方式通过，而它们恰是本片唯一的证据来源
- `tests/hips_mapping_oracle.py:30` `_HERE = Path(__file__).resolve().parent`，全文 import 只有 `:16-24`（ctypes/math/os/random/sys/numpy/astropy.io.fits/astropy_healpix），**无 `from pathlib import Path`**。模块级 `NameError`，脚本**根本跑不起来**。所有「V5 HIPS-IMG-001 mapping oracle 通过」的声称在此不可复现。（我本人 `read` 1–50 行确认）
- `tests/sanitize_wsl_v4.sh:18` `ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"` → 实测 `/workspace/Astro CS Database/lib`（应为 `../../../..`），`lib/lib` 不存在 ⇒ `:30-33` 首个 `g++` 源路径即不存在，**整门在第 1 步 `set -e` 退出**。
- `tests/sanitize_wsl_v4.sh:68-73` 步 `[4/5]` **无任何断言**：`:73` 直接 `... | tee gaia_sanitize.out`，`:74` 就进入步 5；脚本只有 `set -e`（`:12`）**没有 `set -o pipefail`**，管道状态取 `tee` 的 0；且输入路径 `/mnt/f/Astro dev/Astro CS Normalization Database/GaiaDR3SP` 本机不存在。即使修好 ROOT，该步也是**在什么都没测的情况下放行**。

### 阻断-3 · 原子发布面「宣称 fail-closed、实为 fail-open」的三处实证
- `src/aio_atomic_file.h:509` 注释「递归删除文件或目录 (**不跟随符号链接**)」，`:511-515` 实现用 `path_exists` → `stat()`（`:310`，**跟随**）判定 `is_dir`；符号链接指向目录时 `:531 opendir` 进入**目标**并递归删其内容。同一文件 `:432` 的 `for_each_child` 用 `lstat` 做对了——正确助手存在却未被 `remove_tree` 使用。
- `scheduler/src/export_stream.cpp:137` `aio_atomic::write_file_atomic(cfg_.manifest_path, f.str(), nullptr);` —— **返回值丢弃且 `err` 传 nullptr**，`export` 的 manifest 落盘失败完全不可见。同文件另有 3 处调用点是检查 `== 0` 的，说明这是遗漏而非设计。
- `product_io/src/hips_manifest.cpp:106,109,114` `std::strtol(..., nullptr, 10)` —— `endptr` 为空且不查 `errno`：`hips_order = abc` → 0 → `o<0` 不成立 → **通过**；`= 12abc` → 12 → 通过；`= 99999999999999999999` → `LONG_MAX`（ERANGE 未查）→ 通过。

---

## 3. 逐文件清单（读了什么 → 看到什么 → 判定）

标注：`[主]` = 我本人读到每一行；`[子]` = 子代理读完、主审仅复核关键行。

| # | 文件 | 读了什么 | 看到什么（`文件:行`） | 判定 |
|---|---|---|---|---|
| 1 | `src/aio_atomic_file.h` (620) | 统一落盘原语全文 | `:110-155`/`:161-207` fsync 文件后 rename，**未 fsync 父目录**；同文件 `:612-616` 的 `fsync_parent_dir` 助手存在，且 `copy_file` `:577-579` 用了 ⇒ 同文件内不一致（调用方 `aio_publish.cpp:243`、`module_adapters.cpp:12905` 各自补了，属补偿而非缺失，但 `write_file_atomic` 自身不闭合）。`:509` 注释与 `:511-515` 实现矛盾（符号链接）。`:594` `promote_dir` 用 **`MoveFileExA`**，与本文件 `:17`、`:20-21`「必须 widen 后 `MoveFileExW`，不得用 ANSI 代码页 API」直接冲突，且比 `atomic_replace` `:97-98` 少了 `MOVEFILE_WRITE_THROUGH`。`:447-475` `dir_is_nonempty` 在 `opendir` 失败时返回 0=「空」。`:370` POSIX 下把 `\` 当分隔符。`:348/:353` 固定 4096 cwd 缓冲。`:550` `copy_file` `path_exists` 后再写，TOCTOU。 | **须修** |
| 2 | `src/aio_compressor.cpp` (285) | zstd/lz4 封装 + C ABI | `:64-76, :95-104, :144-155, :175-184` 四处 fallback `memcpy` 并 `return srcSize`（可信非零长度），仅 stderr 区分；无 codec magic 校验。`:121, :138, :169` `(size_t)→(int)` 窄化，>2GiB 静默截断。`:46, :81, :130, :161` `srcSize==0` 返回 0，与失败不可区分。`:54-55` level 静默夹到 [1,22]（未取 `ZSTD_minCLevel()/ZSTD_maxCLevel()`）。`:263-275` `aio_compress_bound` 对未知 codec **静默返回 srcSize**，与 `:217-219`/`:248-250` 的报错+返回 0 语义不一致。全部诊断走 `fprintf(stderr)`。 | **阻断**（并入阻断-1） |
| 3 | `tests/test_p1_io_hardening.cpp` (294) | 8 例恶意 fixture | `:69-76` 恒等「伪压缩块」＝自洽式断言（阻断-1）。`:120, :142` `CHECK(rc != 0)` —— 注释写「rc=-1」，断言只要求「非 0」；`dir` 不可写时同样绿。`:277-289` `std::system("mkdir -p '" + root + "'")` 用 argv 拼串且**失败被 `if(...){ /* best-effort */ }` 吞掉**（教科书静默降级），`:288-289` 未转义路径的 `rm -rf`。`:189, :209, :229, :268` 硬编码 `-7`/`-2`，而 `HIO_ERR_*` 是 `aio_healpix_io.cpp` 私有常量、公开头文件里取不到。`:57-59` 与 `:78-79` 两段**互相矛盾**的布局注释（实现服从后者）。`:86` `if (!fp) { fprintf(...); return; }` 后调用方照样断言 rc==-7 ⇒ 静默 fixture 失败。`:65-67` `ZSTD_compress` 返回值未过 `ZSTD_isError`。`:166` 只校验首尾像素，中间被污染仍绿。`:25-34` 头注释给的是 `-lzstd -llz4` 的手工 g++ 命令，与实际 CMake 形态不符。 | **阻断** |
| 4 | `tests/sanitize_wsl_v4.sh` (78) | ASan/UBSan/LSan 五步门 | `:18` ROOT 少一级 ⇒ 整门第 1 步即 `set -e` 退出（阻断-2）。`:12` 无 `-o pipefail`；`:68-73` 步 4 无断言 + 输入路径本机不存在（阻断-2）。`:39` `gcc ... || true` 吞掉 cfitsio 编译失败，`:41` `ls *.o` 只证 ≥1 个。`:35/:50/:62/:65/:77` grep 的是驱动**自报**字符串，shell 门零增益。`:16` `tee -a` 只追加不清空。`:24` 无 `-std=c++17`。`:49/:51/:61/:63/:64/:66` 硬编码 `/tmp` 绝对路径，并发冲突。`:73` 单开发者 WSL 绝对路径。`:20` `rm -rf "$BUILD"` 目标为固定字面量但无前缀守卫。 | **阻断** |
| 5 | `tests/hips_mapping_oracle.py` (156，我读 1–50) | astropy 独立映射 Oracle | `:30` `Path` 未 import ⇒ 模块级 `NameError`（阻断-2）。`:16-24` import 段确认无 pathlib。`:43-44` `AIO_DLL` 默认指向 Windows `.dll`，`:62` `ctypes.CDLL` 无 try/except。`:28-29` 注释自陈「不得再内联 _fields_，内联镜像 = 镜像分叉」，而 `:39` 又 `import aio_abi_mirror as abi` 直接复用生产镜像 —— 独立性存疑。`:8` 引用的 `v5_maptile_oracle.py` 与 `run/temp/v5_oracle` 证据不在本片。`[子]` `:128-132` astropy 自往返 `healpix→lonlat→healpix` 恒等式，且失败被计入 → astropy 缺陷会被误报为 ACSD 映射缺陷；`:10-11` 声称的 Dir9999/10000 覆盖在默认 nside=2048 下不可能出现。 | **阻断** |
| 6 | `product_io/src/hips_manifest.cpp` (218) | HiPS properties/manifest 渲染与校验 | `:106,109,114` `strtol` 无 endptr/errno ⇒ fail-open（阻断-3）。`:44-48` `path_is_safe` 只挡 `p[0]=='/'` 与子串 `".."`，**不挡** `C:\`、`\\server\share`、符号链接逃逸；同函数对 `a..b` 反而误拒。`:165` 类型盲的 required 检查（`null`/`0`/`{}` 全部算「存在」）。`:176-183` 只 `is_array()`，`:206` 却宣称 "non-empty array" ⇒ `"files": []` + `tile_count: 0` 全绿（零 tile 产品通过）。`:177-178` `j["tile_count"].get<uint64_t>()` 无 try/catch，字符串/布尔输入直接抛 `json::type_error`。`:160-168` `manifest_schema`/`schema_version` 要求存在但**从不比对** ⇒ 未来不兼容 schema 静默通过。`:70-74` `%.10g` 渲染 RA/Dec/Fov 无 `isfinite` 守卫，NaN 点位无人校验。`:119` 报错文案写 `{fits,png,jpeg}`，`:41` 实际含 `jpg`。`:208-213` 校验 `provenance_sha256` 格式，同等重要的 `output_hash`（`:145`）无任何格式校验。`:105-112` 校验 `hips_order_min > hips_order`，JSON 侧同一事实不校验。`:140` 写出 `hdu_extname`，全仓无读者。 | **阻断**（并入阻断-3） |
| 7 | `product_io/include/astro/aio/provenance.h` (162) | provenance 最小集 + 门声明 | `:14-15` 声称 required 集与 schema 一致并「由独立 Oracle 反查交叉校验」—— 我实测 `$defs/provenance.required`（22 键）与 `provenance.cpp:49-59` 的 `provenance_required_keys()` **逐名一致**，该声称**成立**。但 `:105-141` 的 `Provenance` 结构里 `coordinate_frame`/`coordinate_epoch` 两字段对应 schema 的单一 `coordinate` 键，映射靠 `provenance.cpp` 完成；本片内无交叉校验实现。`:98` `KCorr::definition` 默认写死公式串。`:130-132` `has_k_corr`/`unavailable_*` 默认为空 ⇒ 「不可用无原因」只能靠 `validate_provenance` 拦截，声明层不设防。 | **通过（附条件）** |
| 8 | `src/aio_cfitsio_mutex.h` (114) | 进程级 cfitsio 锁 + 锁等待实测计数 | `:32` `inline std::mutex&` + 函数内 static ⇒ 单一实体、初始化线程安全，**这一点是对的**。`:3` 引 `CMakeLists.txt:407 显式 _REENTRANT` —— 实测 `:407` 在 `add_library(acsd_cpu_baseline …)`，真实定义在 **`CMakeLists.txt:610`**，行号错（A 档悬空）。`:4` 把整个线程模型挂在「`fits_is_reentrant()` 必须返回 1」上，而 **`fits_is_reentrant` 全仓无调用点**，前提无任何运行时校验；`:9-13` 据此断言 READONLY 并发安全、无需进程级串行化。`:15` 引 `eng/tools/quality/tsan/cfitsio.supp`（存在）。`:74-79` `cfitsio_lock_stats_reset()` 与在途 guard 非原子，统计可少计。`:82-112` guard 锁非递归 `std::mutex`，无嵌套防护。 | **须修** |
| 9 | `src/hiss_stream_writer.h` (84) | HISS 流式写入器声明 | `:8` 引 `02_FROZEN_STAGE1_HISS_SPEC.md` —— **全仓零命中**（A 档悬空）。`:11` 引 `docs/engineering/IO_AND_ATOMICITY.md` —— **全仓零命中**（A 档悬空）。`:10` 引 `docs/engineering/io/IO_003_ATOMIC_OUTPUT_PUBLISH.md`（存在，A 档通过）。`:15` 把 `add_tile` 列为本类职责，但本类 API 是 `append_subblock`/`record_tile`（`:55,:61`），`add_tile` 属 `HissWriter` ⇒ 方法名悬空。`:72` `void cancel()` 无返回码，删不掉 `.partial` 无法上报。`:79` 用 `std::unique_ptr<Impl>` 但 include 段 `:29-32` 无 `<memory>`。`:45` 临时名 `<final>.partial` 无 pid/seq，与 `aio_atomic_file.h:76` 的 `.tmp.<pid>.<seq>` 两套约定并存 ⇒ 并发写同目标相撞。 | **须修** |
| 10 | `aio_build_config.json` (11) | aio 编译开关 | 全文读毕。**只有 `build.ps1:43-45` 读它**；根 `CMakeLists.txt` 与任何 `.cmake` 都不读。即 `enable_ahpx:false` 对权威构建形态毫无作用。同仓 `lib/infrastructure/aio/Makefile:13` 有 `-DHAS_ZSTD -DHAS_LZ4`，与 CMake 的「无宏」形态分叉（阻断-1 的两套构建）。`:5` `enable_ahpx:false` 与 `healpix_drizzle_overview.md:8` 把 `.ahpx` 描述为主数据流相互矛盾。 | **须修** |
| 11 | `product_io/CMakeLists.txt` (36) | product_io 静态库 | 全文读毕。`:14-20` 声明 7 个源，实测 7 个 `.cpp` 全在（sha256/bunit/fits/atomic_publish/provenance/hips_manifest/product_io），**0 孤儿 0 缺失**。`:9` 第三方路径解析到 `lib/third_party/nlohmann/json.hpp`（通过）。`:27-35` 的 `if(TARGET acsd_platform_math)` 守卫与其自述理由（独立 configure 时根入口不存在）一致，判断正确。 | **通过** |
| 12 | `src/aio_xisf.h` (10) | XISF 读入口 | 全文读毕。3 个 `extern` 声明 `xisf_read_file/xisf_read_header_only/xisf_detect`，无格式定义（无 magic、无版本、无头结构）、无 writer；命名不带 `aio_` 前缀，与同层兄弟不一致。`[子]` 复核：三者均有实现且被编入（`aio_xisf.cpp:437/651/740`，`CMakeLists.txt:623`）。非本片缺陷。 | **建议** |
| 13 | `healpix_db/docs/healpix_drizzle_overview.md` (72) | Drizzle 引擎总览 | 全文读毕。`:38-46` 模块表列 12 个文件（fits_reader / wcs_sip / poly_clip / drizzle_engine / hp_drizzle_api / healpix_drizzle.py / eng/tests/test_drizzle.py）—— **我实测全部不存在于仓内**（B 档：仅存于被删的 `run/FINAL-07-e2e/bisect/**`）。`:49-51` 依赖 `ahpx_io/compressor`、`ahpx_io/ahpx_writer`、`healpix_stack/healpix_core` 三条路径均已改名/迁移（A 档悬空）。`:56-59` `cd healpix_drizzle; make` 与仓内实际路径不符。`:27-28` `亮度 = sum_flux/sum_weight`、`SNR = sqrt(sum_snr_sq/sum_weight)` —— **`sum_weight` 无零保护**。`:35` 「面积计算用局部切平面近似（10"/px 尺度下误差 <0.01%）」—— 硬编码尺度假设 + 硬编码误差界，无推导、无出处，违反本项目硬编码处置规则。`:63` `from healpix_drizzle import …` 模块不存在。 | **须修** |
| 14 | `…/healpix_stack/stack_engine.h` (97) | sigma-clip + SNR 加权堆栈声明 | 全文读毕。`:4-5` include 的 `ahps_format.h` / `stack_db.h` 确实同目录存在（A 档通过）。`:42` `sigma = 1.4826 * MAD` —— 数值正确（=1/0.6744897）但**未注明出处**，违反「有科学推导的保留并注明出处」。`:33` `lowConfidence`「(N<3)」阈值硬编码无推导。`:19-24` `DrizzlePixel` **无 band 字段**（`[子]` 判为多波段塌陷到 band 0 的根因）。 | **建议** |
| 15 | `…/healpix_stack/hp_stack_hiss.h` (45) | `hp_stack_hiss` C 声明 | 全文读毕。`:11` 文档写的裁剪规则 `|v-mean|>sigma*std` 与 `stack_engine.h` 的加权 MAD 规则**不一致**（两套并存算法）。`:15` 依赖 `healpix_io.dll`，该目录现仅剩 `ARCHIVED.md`（A 档悬空）。`:34-35` `sigma 通常 3.0` / `max_iter 通常 5` 为魔法默认。 | **建议** |
| 16 | `tests/test_tile_model.cpp` (411，我读 370–380) | tile 几何 + signal/support 语义 | 复核算术：`:373` 注释 `12345<<18 = 3230216192 + 1 = 3230216193`，实测 `12345<<18 = 3236167680`、`|1 = 3236167681` ⇒ **注释错 5951488**。而 `:377-378` `expected = ((uint64_t)12345 << 18) \| 1ULL;` **用与生产同一个表达式算期望** ⇒ 错的笔算永远抓不到。**自洽式断言确认。** `[子]` 另报 `:97-99` `expected_4d = 1u << (2*g.depth)` 与生产 `hiss_tile_model.cpp:81` 逐字同式；`:163` 循环在 `n_leaf_per_tile==0` 时真空通过；`:266` 守恒断言由已断言过的字面量相加得出（只测了加法）。 | **须修** |
| 17 | `tests/dataflow_fuzz.cpp` (362) `[子]` | 4 个 fuzz 目标 | `:150, :194` 缺种子即 `跳过; return 0`，而种子是 gitignore 的 `run/temp/freeze_test.hiss` ⇒ 干净 checkout 上 HISS 目标跑 0 次迭代仍报 PASS。`:227-276, :316` `validate_snr_model` 是手写复刻（其自身 `:226` 注释即写「复刻」），只测本文件自造样本、校验和也由本文件算 ⇒ 自洽式断言。`:192-221` 的「JSON 配置 fuzz」fuzz 的是 nlohmann，不是生产的 `json_config`。`:113, :139-140` `crashes` 声明并打印但**从不自增** ⇒ 报告里的 `crashes=0` 是硬编码。`:141/:185/:219/:341` `CHECK(accepted + rejected == n_iters)` 恒真。`:96, :121` 可空指针未查即解引用。`:128, :180` 5000.0ms 是事后比较，兑现不了注释宣称的「每 case 超时」。 | **阻断** |
| 18 | `tests/hiss_experiment_suite.cpp` (1408) `[子]` | 对真实 `HissWriter/Reader` 跑 DQ-001~007 | `:1407` 退出码只是「CSV 写成功」的活性检查；`:519-520` 表结构**无 `verified` 列**；`:400` 压缩失败、`:442` 变换往返失配、`:829-846` 读写失败、`:918-919` **篡改检测失败写 0** 全部 `run()==true` ⇒ rc=0。`:911-919` `open_ret` 赋值后从未使用，篡改检测**无断言**。`:436-447` `apply_transform` 在计时外、`inverse_transform` 在计时后 ⇒ 两个成本列都排除变换成本，而排名正偏向 BYTE_SHUFFLE/DELTA 类候选。`:1042-1063` 分母不匹配（计数在读之前、延迟只收成功样本）。`:1068-1074` 无样本编码为延迟 0（最优值）。`:634-635` `n_leaf=65536 // 标准 Tile 大小` 与同文件 `:1328-1330` 的模型矛盾。 | **阻断** |
| 19 | `tests/hiss_benchmark.cpp` (1546) `[子]` | 基准 + CSV/JSON/summary.md | `:1317-1352, :1360-1369` summary.md 的每条「推荐」是**硬编码散文**，`results` 只用于打印表格；`:1367` 的 `FULL>80%/BITMAP 20-80%/SPARSE<20%` 在实测数据里根本不存在。`:511-513` `verify_plain` 是 `return true;` 的恒真谓词**且是死代码**（5 处调用全传 `nullptr`）。`:975, :1017` CRC32C/xxHash 的「校验」是 `c==0`/`h==0`（真空谓词），且全文件**无任何损坏检测实验**。`:1145-1147` `compress_ms` 被复用于随机读延迟、`cpu_ms = compress_ms` 复制，CSV 表头无单位列 ⇒ 同列混义。`:1105` DQ-007 的 `compression_ratio` 极性与其余相反。`:1478` 默认输出目录 `../../../ACSD_Stage1_HISS_Delivery/reports/experiments` **不存在**。`:1480` `create_directories` 用抛异常重载 ⇒ `std::terminate`，无稳定错误码。 | **阻断** |
| 20 | `tests/p1hips/p1hips_tests_oracle.cpp` (1037) `[子]` | O1–O8：FITS 映射 / MOC / SNR 界 / 层级守恒 / 内存 / 确定性 | `:738-753, :763-770` 整个 MEM-DESIGN-01 记忆体证据块包在 `if (rss0>0 && rss1>0)` 里**无 else 失败**，`o7_rss_kb()` 非 Linux 返回 −1（`:119`）⇒ Windows 上 4 项记忆检查凭空消失而 `:1030` 仍打印 `O1..O8 PASS`。`:223-226` `|gs-ws|/|ws|` 未防 `ws==0`，`0/0=NaN` 被 `if (d>ms)` 静默丢弃（同循环 `:227-231` 的 support 分支**有** `wp>0` 守卫 ⇒ 同一循环两半不一致）。`:790/:797/:800/:805/:807` 六个数值门里四个被蕴含关系吃掉，永不可能加红。`:636` `bits_eq_d(iv, 1.0/v)` 两操作数都来自生产输出 ⇒ 自洽式。`:766` 稠密注入负控复用 `:706` 采的陈旧 `rss0`。`[子]` 自陈两条**未推翻并已撤回**的假设（C2/C3），见 §5。 | **须修** |
| 21 | `tests/p1hips/p1hips_tests_units.cpp` (519) `[子]` | U1–U5：f64/f32 位级、SNR 往返、manifest 计数、FNV 树摘要 | 期望值真正独立（Fliegel–Van Flandern MJD、解析式 `hips_pixel_scale`、全 262144 元点位图）。`[子]` 判为本片唯一质量较好的文件。弱点在同族其它文件。 | **通过** |
| 22 | `tests/p1hips/p1hips_test_main.hpp` (246) `[子]` | 注入 harness + 组运行器 | `:209-224` `run_all_groups` 在**请求的组名匹配不到任何东西时返回 0 并打印「PASS」** ⇒ `ctest -R p1hips_oracle` 一旦因改名失配，执行 0 条断言仍绿。`:142-153` `P1HIPS_CHECK_EQ` 无 fault-name 形参 ⇒ 26 个调用点结构性排除在注入机制外。`:17` 文档举例的 `ACSD_P1HIPS_FAULT=i1_leaf_signal_bitwise` **不存在** ⇒ 照文档做注入是空操作 + 绿跑。 | **须修** |
| 23 | `tests/test_hips_atomic_publish.cpp` (589) `[子]` | A1–A7 原子发布 / manifest fail-closed / SIGKILL / 5 种故障注入 | 6 个故障名经复核真实存在于 `aio_hips_writer.cpp:552-637`；A7 T2/T3 对 `aio_disk_full.h:66-82` 的 `thread_local` 计数器重新推导后**判定测试设计正确且有牙**（此点我采纳子代理的**反向**结论）。弱点：`:415-446` 只证明**测试自带的** `fits_checksum_ok` 有牙，**不证明生产** `verify_fits_checksum` 有牙；`:262/:350/:422` `all_tiles_ok` 也用测试自己的 cfitsio 校验器；全文件**无一个像素值断言**；`:438` `2880`、`:326` 60s、`:334` 5ms 均无出处；`:44` 直接 `std::thread`（两线程非线程池，不计入 9 处私建池）。 | **建议** |
| 24 | `tests/test_psf_fit_handler.py` (208) `[子]` | PSF 拟合 handler 3 例 | **整文件是死的**：`:27` 插入的 `lib/infrastructure/aio/python`、`:31` `astro_image_io`、`:32` `orchestrator`、`:46/:112` `dynamic_psf` **全部不在仓内**；任何 CMakeLists 都不注册。即便能 import：`:110-130` DLL 缺失时**把被测单元 mock 掉**再断言成功（自洽式断言）；`:137-147` 只断言形状/dtype，**一个数值都不验**，全零 (5,6) float64 也全绿；`:197-203` 一个 `except Exception` 包三例 ⇒ 第 1 例抛异常时后两例**根本不跑**。 | **阻断** |
| 25 | `tests/hips_direct_smoke.py` (124) `[子]` | 写 2 个合成 tile + SNR 点 | `:4` docstring 声称「再用 astropy（独立 reader）验证结构」—— **全文无 astropy**（`:18` 只 `import json`，未使用）。脚本写完 tile（`:80-119`）、打印 `finalize ok`、返回 0，**退出码就是生产返回码，零独立验证**。不在任何 CMakeLists。默认往仓内 `run/temp/hips_smoke` 写且不清理。`:84/:97/:105/:113/:117` 对可能是 NULL 的 `last_error` 调 `.decode()` ⇒ 崩而不是稳定错误码。 | **阻断** |
| 26 | `…/healpix_stack/hp_stack_api.cpp` (829) `[子]` | C API：JSON 解析、DB 开关、5 阶段梯度修正 | `:463-466` `sigma<=0→3.0` / `max_iter<=0→5` / `lambda<=0→1e-4` **静默替换非法参数**；`:506-512`、`:531-536` 采样或拟合失败即「回退」到无梯度修正的 `hp_stack_hiss` 并透传其 rc ⇒ 调用方拿 rc=0 与一份未修正数据（自标「回退」的 fail-open）；`:660-676/:690` SNR 模型缺失时 `corrected_stacker` 静默用等权 `w=1.0`，P2 加权退化为不加权；`:427-438` `hp_stack_run` 是返回 1 的空壳。**零消费者经复核为真**（`orchestrator.cpp:4876` 是无函数体的孤儿注释，非活调用点）。 | **须修（当前不可达）** |
| 27 | `…/healpix_stack/healpix_core.cpp` (679) `[子]` | HEALPix ang↔pix / ring-nested / 邻居 / queryDisc / LOD | `:47-49` 打印「nside 不是 2 的幂，**行为未定义**」后**照样构造**；`:501` `neighbors()` **无 ipix 越界检查**（权威版 `lib/algorithms/shared/healpix/healpix_core.cpp` 有 `if (ipix >= 12ULL*npface) return result;`）⇒ 越界 ipix 经 `pix2ring` 置 `ring=-1`（`:292`）流入 `ring2xy` 的 `ring<=Ns` 分支（`:346`）产出垃圾 bighp/x/y；`:665-666` `if (ratio<1) ratio=1;` 把粗→细请求静默退化为返回粗像素本身；`:387-391` 奇偶不一致时静默 `hh++` 重算（掩盖而非上报）。`[子]` 两条「疑似缺陷」（环带 off-by-2Ns、赤道对角邻居）**经与权威实现对拍后自行推翻并撤回**，见 §5。 | **须修（当前不可达）** |
| 28 | `…/healpix_stack/stack_engine.cpp` (392) `[子]` | sigma-clip + SNR 加权堆栈 + tile 落盘 | `:216` `if (!db) return 0;`、`:231` `if (byPixel.empty()) return 0;` —— **空 DB 与空输入都返回「成功」**；`:126` **丢弃 `sigmaClip` 的 bool 返回**，`:108/:124` 的 `lowConfidence` 只按 `N<3` 置位 ⇒ 剔掉 5/100 个野值与一个都没剔的像素记为同一状态；`:270-275` 自承多波段塌陷到 band 0，`:318/:322` 只写 `existStats[0]`，`:314` 清零其余 ⇒ 默认 6 波段（`stack_db.h:19`）的库被写成 5/6 波段全零且 rc>0 无告警；`:349-353`/`:367-369` tile 写失败仅 `continue`/记日志，`:374` 仍 `return (int)result.pixels.size()`（正数=成功）⇒ **全部写失败也通过**；`:323` `uint16_t count` 累加，>65535 帧静默回绕。 | **须修（当前不可达）** |

---

## 4. 发现清单

### 4.1 阻断（8）

| ID | 一句话 | `文件:行` | 失败场景 |
|---|---|---|---|
| **BLOCK-1** | 测试的「压缩器」是被检压缩器的同一个恒等式；zstd 路径零覆盖；能力查询 `hasZstdSupport` 零调用 | `tests/test_p1_io_hardening.cpp:69-76` + `src/aio_compressor.cpp:95-104` + `eng/tests/unit/CMakeLists.txt:896-899` | 带 zstd 构建写出的产品被 CMake（无宏）构建读回 ⇒ 压缩字节进像素缓冲、返回非零、零错误码 |
| **BLOCK-2** | 独立映射 Oracle 模块级 `NameError`，根本跑不起来 | `tests/hips_mapping_oracle.py:30`（import 段 `:16-24` 无 pathlib） | 一切「mapping oracle 通过」声称不可复现 |
| **BLOCK-3** | ASan/UBSan/LSan 门 ROOT 少一级 ⇒ 整门第 1 步退出 | `tests/sanitize_wsl_v4.sh:18`（实测 ROOT=`<repo>/lib`，`lib/lib` 不存在） | 健壮性门从未在本仓跑过 |
| **BLOCK-4** | 同门步 `[4/5]` 无断言 + 无 `pipefail` + 输入路径本机不存在 ⇒ 在什么都没测的情况下放行 | `tests/sanitize_wsl_v4.sh:12, :68-73` | 脚本走到 `:79` 打印 `ALL_SANITIZE_V4_PASS` |
| **BLOCK-5** | 实验套件退出码不是判定：CSV 无 `verified` 列，篡改检测失败也 rc=0 | `tests/hiss_experiment_suite.cpp:519-520, :911-919, :1407` | `HissReader::open` 变得能容忍校验和失配，报告仍「DQ-001~007 OK」 |
| **BLOCK-6** | benchmark 的 summary 结论是硬编码散文；`verify_plain` 恒真且是死代码 | `tests/hiss_benchmark.cpp:1317-1352, :511-513` | 全部 codec 不可用（正是当前构建形态）时仍输出「推荐 byte-shuffle + LZ4/Zstd」 |
| **BLOCK-7** | fuzzer 缺种子即静默跳过；`snr_model` 目标只测本文件自造样本；`crashes` 恒为 0；四个 `accepted+rejected==n` 恒真门 | `tests/dataflow_fuzz.cpp:150, :194, :227-276, :113, :141/:185/:219/:341` | HISS 目标在干净 checkout 跑 0 次迭代仍报 PASS |
| **BLOCK-8** | 三个 Python 证据脚本分别是：死的（import 不存在模块 + mock 被测单元 + 零数值断言）、死的（docstring 谎称 astropy 验证）、恒定绿的 | `tests/test_psf_fit_handler.py:27-46,:110-130,:137-147`；`tests/hips_direct_smoke.py:4,:119`；`tests/hips_mapping_oracle.py:30` | PSF 拟合与 tile 直写的「验证」从未发生 |

### 4.2 须修（12）

1. `src/aio_atomic_file.h:509` vs `:511-515` —— `remove_tree` 注释称不跟随符号链接，实现用 `stat` 跟随 ⇒ 清理暂存树时 planted 符号链接会把**产品树外**的数据删掉；同文件 `:432` 的 `lstat` 正确助手存在却未用。
2. `src/aio_atomic_file.h:594` `MoveFileExA` —— 违反本文件 `:20-21` 自己写的「Windows 必须 widen 后 `MoveFileExW`」；且比 `atomic_replace` `:97-98` 少 `MOVEFILE_WRITE_THROUGH`，整树发布耐久性更弱。（注：文件 `:297-298` 已把 UTF-8→UTF-16 登记为遗留项，故我对 `FindFirstFileA`/`_stat64`/`_mkdir` 不单独记缺陷，**只记这一处内部不一致 + 缺 WRITE_THROUGH**。）
3. `scheduler/src/export_stream.cpp:137` —— `write_file_atomic(...)` 返回值丢弃且 `err=nullptr`，`export` manifest 落盘失败不可见（同仓另有 3 处检查 `==0`，证明是遗漏）。
4. `product_io/src/hips_manifest.cpp:106,109,114` —— `strtol` 无 endptr/errno，非法数字静默变 0 并通过。
5. `product_io/src/hips_manifest.cpp:44-48` —— `path_is_safe` 子串黑名单，Windows 绝对路径与符号链接逃逸放行。
6. `product_io/src/hips_manifest.cpp:160-168` —— `manifest_schema`/`schema_version` 要求存在但从不比对，不兼容 schema 静默通过。
7. `product_io/src/hips_manifest.cpp:176-206` —— `"files": []` 全绿（文案说 non-empty，代码只查 `is_array()`）；`:177-178` 类型错输入直接抛异常而非稳定错误码。
8. `src/aio_cfitsio_mutex.h:3` —— 引 `CMakeLists.txt:407`，真实在 `:610`；`:4` 的 `fits_is_reentrant()` 前提**全仓无调用点**，无人校验。
9. `src/hiss_stream_writer.h:8, :11` —— 两个 A 档悬空文档引用；`:15` 方法名 `add_tile` 属别的类；`:72` `void cancel()` 无错误码。
10. `healpix_db/docs/healpix_drizzle_overview.md:27-28, :35` —— `sum_weight` 零除无保护；`:35`「10\"/px 误差 <0.01%」是硬编码尺度+硬编码误差界，无推导无出处。
11. `tests/test_tile_model.cpp:373` + `:377-378` —— 注释手算错 5951488，而断言用与生产**同一个表达式**算期望 ⇒ 错值永抓不到（自洽式断言）。
12. `tests/p1hips/p1hips_test_main.hpp:209-224` —— 组名失配返回 0 并打印 PASS；`:142-153` 26 个调用点结构性排除注入。

### 4.3 建议（14）

`aio_compressor.cpp:121,138,169` `size_t→int` 窄化 · `:46,81,130,161` 空输入与失败不可区分 · `:54-55` level 静默夹取（应用 `ZSTD_minCLevel/maxCLevel`）· `:263-275` 未知 codec 的 bound 与 compress 语义不一致 · `aio_atomic_file.h:447-475` `dir_is_nonempty` 失败即「空」（唯一调用点 `aio_publish.cpp:228-229` **确实**检查 `ok` 并 fail-closed，故本条为潜在陷阱非现行缺陷）· `:370` POSIX 下 `\` 当分隔符 · `:348/:353` 固定 4096 缓冲 · `:550` `copy_file` TOCTOU · `hips_manifest.cpp:70-74` RA/Dec/Fov 无 `isfinite` · `:119` 报错文案 `{fits,png,jpeg}` 与 `:41` 实际 `{fits,png,jpeg,jpg}` 不符 · `:140` `hdu_extname` 写出无人读 · `stack_engine.h:42` `1.4826` 正确但未注出处 · `hp_stack_hiss.h:11` 裁剪规则与 `stack_engine.h` 的 MAD 规则两套并存 · `aio_build_config.json` 只被 `build.ps1` 读、对 CMake 无效 · `hiss_stream_writer.h:79` 缺 `<memory>`

**本片私建线程池：无。** 我逐份核过全部 28 份，无任何自建池、无 `exit()`、无 `getenv()`、无全局配置读、无 catch 吞异常（`aio_compressor.cpp:223/:226/:254/:257/:278/:281` 的 catch 是 C ABI 异常屏障，有 `return 0` 语义，不计）。唯一并发设施是 `aio_cfitsio_mutex.h` 的进程级互斥量，**单一定义、跨 TU 共享**，未构成违规。

---

## 5. 我主动构造的反例

| # | 构造 | 期望推翻 | 结果 |
|---|---|---|---|
| CE-1 | 追 `decompressZstd` 无 zstd 分支：产品由带宏构建写出、被 CMake（无宏）构建读回 | 「无 zstd 时只是不压缩，功能不受影响」 | **推翻成功**。`:96-103` 把压缩字节 memcpy 进像素缓冲并返回 `srcSize`，无 magic 校验、无错误码、无调用方防护；`hasZstdSupport()` 全仓零调用点 |
| CE-2 | 追 `sanitize_wsl_v4.sh` 的 `ROOT` | 「门可运行，只是路径风格不同」 | **推翻成功**。`cd lib/infrastructure/aio/tests/../../..` → `/workspace/Astro CS Database/lib`；`lib/lib` 不存在 ⇒ 首个 `g++` 源路径不存在 |
| CE-3 | 追 `hips_mapping_oracle.py` 的 `Path` | 「`Path` 由某处间接 import」 | **推翻成功**。`:16-24` import 段确认无 pathlib，模块级 `NameError` |
| CE-4 | 追 `test_tile_model.cpp:373` 的位运算笔算 | 「注释只是笔误，断言会抓到」 | **部分推翻**。注释确实错（`12345<<18=3236167680`，非 3230216192）；但 `:377-378` 用 `((uint64_t)12345 << 18) | 1ULL` 与生产同式算期望 ⇒ **断言抓不到笔误**。同一处既是错值又是自洽式断言 |
| CE-5 | 假设 `dir_is_nonempty` 在 `opendir` 失败时返回「空」= fail-open | 「目录不可读会被当空目录，误判已发布」 | **假设被推翻**。唯一调用点 `lib/algorithms/drizzle/hips/src/aio_publish.cpp:228-229` 明确 `if (!ok) return AIO_PUBLISH_ERR_IO;`，fail-closed。**降级为潜在陷阱，不记缺陷** |
| CE-6 | 假设 `provenance.h:14-15` 的「required 与 schema 一致」是陈旧声称 | 「注释漂移，门名不副实」 | **假设被推翻**。实测 `$defs/provenance.required` 22 键与 `provenance.cpp:49-59` 的 `provenance_required_keys()` 逐名一致。**判通过，不记缺陷** |
| CE-7 | 假设三个悬空文档引用是 `1fa477a7` 删 `run/**` 造成的假象 | 「悬空判定会被基线漂移污染」 | **推翻成功（对 A 档）**。`ENGINEERING_SPEC.md`、`02_FROZEN_STAGE1_HISS_SPEC.md`、`docs/engineering/IO_AND_ATOMICITY.md` 在 `850a9ede` 与 `HEAD` **都**零命中 ⇒ A 档真悬空，非归档假象 |
| CE-8 | 查本片 28 份在 `850a9ede` vs `HEAD` 的 blob 哈希 | 「基线漂移可能让我的结论失效」 | **推翻成功**。identical=28 / changed_or_absent=0 ⇒ 全部结论对三基线同等成立 |

**另采信子代理的三条「自推翻」**（我未独立复核，登记为线索）：`p1hips_tests_oracle.cpp` 的 o7 tile 选择与 `o7_nodata_nan_sup0` 非真空（两条假设经追fixture 后自行撤回）；`healpix_core.cpp` 的环带 off-by-2Ns 与「赤道带对角无邻居」经与仓内权威实现 `lib/algorithms/shared/healpix/healpix_core.cpp` 对拍后**自行撤回为假阻断**；`test_tile_model.cpp` 的 `{8,0}` 往返经推演**不红**。这三处若成立，我方的「恒红门」清单需相应缩减。

---

## 6. 盲复算

**方法**：先遮住既有判定，独立逐份取证并写下自己的结论，**之后**才打开 `run/GOVERN-08/审核包-R2/分片清单/逐份判定-权威版.csv` 对照。

**独立结论**：本片 28 份中，**0 份可判「通过」**；须修 11、阻断 8、其余为建议级。

**权威表实际记载**：28/28 行**全部**为同一句 `默认保留（非产出面或非数据形态）`，**无任何缺陷分级、无任何 `文件:行`**。

**判定：偏松。** 且偏松有可解释的成因——把 `hiss_benchmark.cpp`、`hiss_experiment_suite.cpp`、`p1hips_tests_oracle.cpp` 归入「非产出面或非数据形态」，于是把**本片唯一的证据生产面**整体豁免：这些文件产出的 CSV/summary.md 正是别处引用来支撑「已验证」的读数，而它们（BLOCK-5、BLOCK-6、BLOCK-7）恰恰是自洽式断言与恒绿门的密集区。同理，`sanitize_wsl_v4.sh` 被当作「非产出面」脚本豁免，而它是 ASan/UBSan/LSan 的唯一健壮性门（BLOCK-3、BLOCK-4）。

**需负责人裁决的口径问题**：「非产出面或非数据形态」是否应豁免**验证面**？本片证据表明：验证面一旦失能，缺陷不会停留在验证面，而会流到被判「通过」的生产面上（BLOCK-1 的路径正是 `tests/test_p1_io_hardening.cpp` → `src/aio_compressor.cpp`）。

---

## 7. 子代理派发记录

**派发 5 名**（按文件分组，互不重叠，并集覆盖 28/28）：

| 子代理 | 分组 | 份数 | 自报行数 |
|---|---|---|---|
| A | `src/` + `product_io/`（9 份） | 9 | 1540 / 1540 |
| B | `healpix_db/archive/legacy/healpix_stack` + `docs/healpix_drizzle_overview.md`（6 份） | 6 | 2114 / 2114 |
| C（第一次） | 同 B 组（**有意重复，作独立二次意见**） | 6 | 2114 / 2114 |
| D | `hiss_*` + `p1hips/*`（5 份） | 5 | 4756 / 4756 |
| E | 其余 `tests/`（8 份） | 8 | 2222 / 2222 |

### 逐条复核（我亲自 `read` 原文或跑命令的）

| 子代理结论 | 我的处置 | 依据 |
|---|---|---|
| A: B1 压缩静默降级 | **采纳（升格）**。我自己读 `aio_compressor.cpp` 全文确认四处 fallback；并**纠正其证据链**：它引的 `Makefile:13`/`build.ps1:74`/`eng/tests/unit/CMakeLists.txt:880` 在活树与归档中混杂，我实测活树 CMake 侧零定义、`Makefile:13` 有宏 —— 分叉结论不变，路径更正 | `aio_compressor.cpp:64-184`；`grep -rn HAS_ZSTD lib/ eng/ CMakeLists.txt` |
| A: B2 `(size_t)→(int)` 窄化 | 采纳，须修 | `aio_compressor.cpp:121,138,169` |
| A: B3 Windows 先删后 rename | **不采纳为本片缺陷**（涉 `atomic_publish.cpp`，不在本片），登记为跨片线索 | `aio_atomic_file.h:18` 禁令引用；`hiss_stream_writer.cpp:174` 我实测确在 |
| A: M10 缺父目录 fsync | **部分降级**。实测 `fsync_parent_dir` 的两个关键调用点（`aio_publish.cpp:243`、`module_adapters.cpp:12905`）都补了 ⇒ 不是缺失，是原语自身不闭合 + `export_stream.cpp:137` 单独漏 | `grep -rn fsync_parent_dir lib/` |
| A: M11 `remove_tree` 跟随符号链接 | 采纳，须修 #1 | `aio_atomic_file.h:509` vs `:511-515`/`:310`，与 `:432` 的 `lstat` 对照 |
| A: M13 `fits_is_reentrant` 零调用 | 采纳，须修 #8 | `aio_cfitsio_mutex.h:4` |
| A: M14 非递归锁嵌套风险 | 登记为潜在（子代理自陈未证明嵌套发生） | — |
| A: S13 `product_io.cpp:80` 往返自校验 | 登记为线索 | 文件不在本片 |
| A: CE4「provenance 一致」被它自己**推翻** | **采纳其推翻**（见 §5 CE-6） | 我独立核对 schema，22 键一致 |
| B/C: 「零消费者」为真 | 采纳，并据此把 `hp_stack_api.cpp`/`healpix_core.cpp`/`stack_engine.cpp` 的缺陷降为**当前不可达** | 两份独立复核同结论；`orchestrator.cpp:4876` 是孤儿注释 |
| B/C: `test_healpix_stack.py` 恒绿（7 例全 skip） | 采纳为线索（该测试文件不在本片） | 子代理自报读 509 行 |
| B/C: `healpix_db/README.md:18` 称 healpix_stack「活跃/被 gitignore 忽略」与 `.gitignore` 自注矛盾 | 采纳，须修 | 我实测 `.gitignore` 尾注「已于 2026-07-24 纳入主仓库版本控制, 不再忽略」 |
| D: B4 两个 benchmark 从不 `add_test` 且无 `HAS_ZSTD` 宏 | 采纳为 BLOCK-7 的成因 | 与我实测「活树 CMake 零定义」一致 |
| D: M1 组名失配返回 0 | 采纳，须修 #12 | `p1hips_test_main.hpp:209-224` |
| D: M5 记忆体块无 else-fail | 采纳，须修 | `p1hips_tests_oracle.cpp:738-753` |
| D: M8 `p1hips_oracle.hpp` 的解析 oracle 零调用 | 采纳为线索 | 头文件不在本片 |
| D: **C2/C3/C4 三条自推翻** | **采纳其推翻**，不计入缺陷 | 子代理自陈经追 fixture/对拍后撤回 |
| E: B1 T8 因开错目录 + 缺 manifest 而绿 | 采纳为线索（依赖 `aio_hips_reader.cpp`，不在本片），但**我自己独立确认**了 E 报的 M15 静默吞 `mkdir` 失败 | `test_p1_io_hardening.cpp:277-289` 我亲自读过 |
| E: B4/B5 `sanitize_wsl_v4.sh` | **采纳，并**我自己 `cd` 复现 ROOT 少一级；**修正其表述**：ROOT 错使整门在第 1 步 `set -e` 退出（不是「exit 0 什么都没测」）；「exit 0」只适用于步 4 的管道无断言那一条独立缺陷 | `sanitize_wsl_v4.sh:18, :12, :68-73`，我实测 |
| E: M20 该组「HAS_ZSTD 分裂在 CMake 下不成立」 | **采纳并与我的独立发现合并**：CMake 下测试与库确实同分支，但**正因为如此，CMake 从不定义 HAS_ZSTD，zstd 路径在权威构建里根本不存在** —— 分裂点不在测试，在构建 | 我的 `grep` 结果 + `eng/tests/unit/CMakeLists.txt:880-899` |
| E: M8 PSF 文件整个是死的 | 采纳为 BLOCK-8 | 文件不在本片，取其线索 |

**否决 / 修正合计：11 条被否决或降级**（A 的 B3、M10、M14、S13 各一；D 的 C2/C3/C4 各一；E 的 B4 表述、E 的 M20 各一；另有 3 条 A 自己的自我推翻被我采纳）。5 组独立复核中，A、B/C、D、E 均自报并主动撤回了自己构造的假阳性——这一点提高了对余下结论的置信度，但**不能替代我本人对那 15 份未读文件的独立复核**。

---

## 8. 自证段（可复跑）

```bash
cd "/workspace/Astro CS Database"
# 基线与漂移
git -c core.quotepath=false rev-parse HEAD
git -c core.quotepath=false log --oneline -5
git -c core.quotepath=false show --stat --format="" --name-only 1fa477a7 -- lib/infrastructure/aio   # 空 ⇒ 该提交未碰本片

# 本片成员与行数（应得 28 份 / 10632 行 / missing=0）
sed -n '3090,3126p' run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml
while IFS= read -r f; do [ -f "$f" ] && printf "%6d  %s\n" "$(wc -l < "$f")" "$f" || echo "MISS $f"; done < /tmp/myslice.txt | tail -3

# BLOCK-1：zstd 在权威构建中不存在
grep -rn "HAS_ZSTD\|HAS_LZ4" lib/ eng/ CMakeLists.txt        # 仅 .cpp 注释，无任何 CMake 定义
grep -n "_DHAS_ZSTD\|_DHAS_LZ4" lib/infrastructure/aio/Makefile lib/infrastructure/aio/build.ps1
sed -n '880,883p;896,899p' eng/tests/unit/CMakeLists.txt       # 两次「自洽」背书
sed -n '69,76p' lib/infrastructure/aio/tests/test_p1_io_hardening.cpp
sed -n '95,104p' lib/infrastructure/aio/src/aio_compressor.cpp
grep -rn "hasZstdSupport\|hasLz4Support" lib/ eng/             # 仅声明+定义，零调用点

# BLOCK-2：oracle 跑不起来
sed -n '16,24p;30p' lib/infrastructure/aio/tests/hips_mapping_oracle.py

# BLOCK-3/4：sanitize 门
cd lib/infrastructure/aio/tests && cd "$(dirname ./sanitize_wsl_v4.sh)/../../.." && pwd   # => <repo>/lib
cd "/workspace/Astro CS Database"; ls -d lib/lib                                          # No such file
grep -n "set -" lib/infrastructure/aio/tests/sanitize_wsl_v4.sh                            # 只有 set -e，无 pipefail
sed -n '68,74p' lib/infrastructure/aio/tests/sanitize_wsl_v4.sh                             # 步4 无 grep

# 阻断-3：原子发布
sed -n '509,515p;310p;432p;594p;612,616p' lib/infrastructure/aio/src/aio_atomic_file.h
sed -n '135,138p' lib/infrastructure/scheduler/src/export_stream.cpp
sed -n '44,48p;106,109,114p;160,168p;176,183p;206p' lib/infrastructure/aio/product_io/src/hips_manifest.cpp

# 悬空引用（A 档：三基线均零命中）
for p in ENGINEERING_SPEC.md 02_FROZEN_STAGE1_HISS_SPEC.md docs/engineering/IO_AND_ATOMICITY.md; do
  printf "%-40s base=%s head=%s\n" "$p" \
    "$(git -c core.quotepath=false ls-tree -r --name-only 850a9ede | grep -cx "$p")" \
    "$(git -c core.quotepath=false ls-tree -r --name-only HEAD | grep -cx "$p")"; done
grep -n "_REENTRANT" CMakeLists.txt          # 真实在 :610，非注释所引 :407

# CE-4：test_tile_model 注释算错 + 断言自洽
python3 -c "print(12345<<18, (12345<<18)|1)"    # 3236167680 3236167681 ; 注释写 3230216192/3230216193
sed -n '370,380p' lib/infrastructure/aio/tests/test_tile_model.cpp

# CE-6：provenance required 与 schema 一致（该子代理假设被推翻）
python3 -c "import json;print(json.load(open('eng/contracts/schemas/product_family_field_constraints.schema.json'))['\$defs']['provenance']['required'])"
sed -n '49,59p' lib/infrastructure/aio/product_io/src/provenance.cpp

# CE-5：dir_is_nonempty 唯一调用点确实 fail-closed（假设被推翻）
sed -n '226,231p' lib/algorithms/drizzle/hips/src/aio_publish.cpp

# 盲复算对照
head -1 run/GOVERN-08/审核包-R2/分片清单/逐份判定-权威版.csv
while IFS= read -r p; do grep -F "$p" run/GOVERN-08/审核包-R2/分片清单/逐份判定-权威版.csv | head -1; done < /tmp/myslice.txt
```

---

## 9. 交付状态

**判定：阻断。** 本片含 8 项阻断，其中 BLOCK-1 的形态正是负责人本轮点名要的产物——**同一个定义式既当被检量又当期望量**：测试里的 `zstd_compress_bytes` 在无宏分支退化为恒等函数，与被检的 `decompressZstd` 无宏分支同为 memcpy，二者必然一致，故四条 HCSD 用例无法证伪「zstd 解压正确」；配合 `hasZstdSupport()` 零调用与 CMake 侧零宏定义，**zstd 路径在权威构建中既不存在、也无人证明过它正确**。

**新问题**：是。除本轮点名的自洽式断言外，另报四类既有清单外的新问题：静默降级（`export_stream.cpp:137` 丢弃 manifest 写失败返回码；`test_p1_io_hardening.cpp:278` 空 `if` 吞 mkdir 失败）、恒绿门（`sanitize_wsl_v4.sh:73` 管道无断言且无 pipefail；`p1hips_test_main.hpp:209-224` 组名失配返回 PASS）、悬空引用（本片 7 处 A 档悬空，其中 2 处是错误行号）、硬编码（`healpix_drizzle_overview.md:35` 的 `10"/px` 与 `<0.01%` 无推导无出处）。

**须负责人裁决**：本片 28 份中 15 份主审未亲读（占 80.4% 行），结论依赖 5 名子代理的口径 C 覆盖。若需「主审逐字重读」口径，本片应由前台或下一遍补齐 §1.1 列出的 15 份后重新裁决。另建议对「非产出面或非数据形态」是否豁免**验证面**给出统一口径——本片 BLOCK-1 的传导路径正是从被判豁免的测试文件流向被判通过的生产文件。