# 审稿-P1-TAIL-LIB-001 — G08-05 对抗审稿第 1 遍

- 片号：`TAIL-LIB-001`
- 层：尾域合并（`lib/` 下层行数 < 2000 的小层）
- 仓：`/workspace/Astro CS Database`
- ⚠️ **基线不符（须报负责人）**：任务书给定 HEAD = `850a9ede`；实测工作树 HEAD = **`9a83f63a558b7efe82eb1a9435c7b40f263400a2`**（`git rev-parse HEAD`）。最近三次提交为 `9a83f63a` / `f4a2cf21` / `1fa477a7`，**`850a9ede` 不在近端历史中**。本报告的全部行号与取证均针对**实际工作树 `9a83f63a`**，可能与 `850a9ede` 基线不同。请负责人确认应以哪个基线为准。
- 视角：红队。默认现行结论是错的；**任何「检查通过」的机制一律不作为正确性依据**。
- 禁令遵守：未读 `/tmp/acsd_g08/`；零 git 写；未编译 / 未跑 ctest / 未跑 pytest / 未跑任何二进制；未改任何仓内文件（本文件为唯一新增交付件，位于 `run/GOVERN-08/审核包-R2/`）。

---

## 1. 读完了吗

口径声明：

- **成员份数** = 片清单 `片清单-权威版.yaml:3613` 声明的 `成员份数: 33`。
- **成员总行数** = 对清单 `:3619-3651` 逐文件 `wc -l` 求和（尾行无换行不计，与清单 `实际行数: 5130` 逐位一致）。
- **实际读了多少行** = 我用 `read` 工具逐文件从第 1 行读到 EOF 的行数合计。

| 项 | 值 |
|---|---|
| 成员份数 | 33 |
| 读了几分 | **33** |
| 成员总行数 | **5130** |
| 实际读了多少行 | **5130** |
| 覆盖率 | **33/33 份 = 100%；5130/5130 行 = 100%** |
| 未读完的 | **无** |

补充计数（同一口径，便于复核）：

- 生产源码（C++/头/Makefile/CMake/Python 可执行逻辑）：12 份 / 3072 行 —— 逐行读完，无跳读、无摘要代替。
- 文档与元数据（README / memory.md / module.yaml / THIRD_PARTY_NOTICE / .gitignore）：21 份 / 2058 行 —— 逐行读完。
- 33 个成员文件全部存在，**无 MISSING**（逐个 `test -f` 验证）。

---

## 2. 本片判定

### **判定：阻断**

本片不能放行。核心不是「有多少 bug」，而是：**本片承载的两个对外可信锚（HEALPix 几何正确性、FITS 产品完整性）各自都建立在一个无法失败的判据上**，而真正独立的那道外部 oracle 在活树里根本跑不起来。

### 最重 3 条

1. **`lib/algorithms/fits_output/p3_output.cpp:660` —— 独立重开验证对「COVERAGE 面缺失」fail-open，而公共头白纸黑字承诺相反。**
   `:660` 的条件是 `if (hdus >= 2 && fits_movabs_hdu(f, 2, …) == 0) { …checks… } else covok = 0;`——注意：**这个 `else` 并不存在**。条件不成立时整个 coverage 校验块被跳过，`covok` 保持初值 `1`（`:557`）。
   - 决定性对照：同文件 `:1153-1161` 的流式校验路径**有** `else { impl_->covok = 0; }`。同一逻辑两处实现，一边有一边没有 —— 这是遗漏，不是设计。
   - `lib/algorithms/fits_output/p3_output.h:162` 明文承诺：「独立重开：尺寸/HDU 面/WCS 关键字逐项对拍（**缺 HDU** 或占位 HDU 均判失败）」。代码没兑现。
   - 反例（见 §5 CE-1）：删掉 COVERAGE 扩展 HDU 的 FITS 文件，`p3_output_verify_ex` 报 `coverage_ok=1`、`reopen_ok=1`。
   - 第二条独立通路（CE-2）：`:570` 与 `:1102` 的 `fits_get_num_hdus(f, &hdus, &status)` **返回值未检查**，失败时 `hdus` 停在初值 `1`（`:560` / `:1072`），于是 `hdus >= 2` 恒假 —— 同一个漏洞的第二条触发路径。这正是负责人点名的「静默降级」：读失败被吞掉并继续，而且吞掉的方向是放行。

2. **`lib/algorithms/shared/healpix/tests/test_healpix_neighbors.cpp:189` —— 恒真门（永不可能失败的断言）。**
   ```cpp
   const auto got = query_disc(4, 10.0, 20.0, 0.0);
   expect(got.size() == 1 && got[0] == ang2pix_nest(4, 10.0, 20.0), …);
   ```
   `got[0]` 的来源是 `healpix_core.cpp:447-451`：
   ```cpp
   if (radius_rad <= 0.0) { result.push_back(ang2pix_nest(nside, ra_deg, dec_deg)); return result; }
   ```
   断言右侧 `ang2pix_nest(4, 10.0, 20.0)` **与被检量 `got[0]` 是同一次调用、同一组实参**。这是负责人要求的最高价值类别——「同一个定义式既当被检量又当期望量」的字面标本。该断言恒为真，任何对零半径语义、中心像素正确性的改动都不会被它拦下。

3. **`lib/algorithms/shared/healpix/tests/test_hips_tile_mapping.cpp:36` —— 「外部 Oracle 冻结」是假的；期望值是实现体逐字抄写。**
   ```cpp
   const uint64_t expect = (uint64_t)(511u - x) * 512u + (uint64_t)y;          // :36（:63/:70 同）
   const uint64_t fi = acsd::healpix::nested_local_to_fits_index(local, shift, tw);
   if (fi != expect) ++bad_fi;
   ```
   被检函数体在 `healpix_core.cpp:288-293`：
   ```cpp
   const uint32_t maxv = (tile_width > 0) ? (tile_width - 1u) : 0u;   // = 511
   return (uint64_t)((x <= maxv ? maxv - x : 0u)) * (uint64_t)tile_width + (uint64_t)y;
   ```
   与测试期望式**逐字符同构**（常量 511/512 同样来自 `tw=512`）。文件头 `:4-5` 声称「由 CDS Hipsgen MAPTILES 外部 Oracle 逐像素验证，见 `run/temp/v5_oracle` 证据」——`ls -d run/temp/v5_oracle` 实测 **不存在**。测试里没有任何一个来自外部 oracle 的冻结字面量。整道「硬门」对「HiPS 瓦片标准本身是否理解正确」零鉴别力。

---

## 3. 逐文件清单

| # | 文件（行数） | 读了什么 | 看到什么（`文件:行`） | 判定 |
|---|---|---|---|---|
| 1 | `lib/algorithms/fits_output/p3_output.cpp`（1270） | 全 1270 行，分 4 段读 | `:660` coverage 校验块缺 `else`；`:570`/`:1102` `fits_get_num_hdus` 返回值丢弃；`:526-527` 实测值被常量 `1` 覆写；`:545` 未查 `wcs/signal/coverage` 空指针而 `:578/:650/:740` 直接解引用；`:1065` 注释宣称流式校验「与 `p3_output_verify_ex` 同判据」但流式面缺 DATASUM/CHECKSUM 与 BUNIT 对拍；`:211-217` 注释描述的是**已删除**的 `crypto::sha256_file`；`:34` 的 `#include "sha256.h"` 已成孤儿；`:523`/`:740`/`:1228` coverage 统计把 NaN 当未覆盖（最坏值被筛掉）；`:671`/`:1226` coverage 只比 `>0.5f` 二值，幅值全错也判过；`:637`/`:1139` CD 容差实为 `1e-12` 而 `:553-554` 注释承诺 `1e-15` | **阻断** |
| 2 | `lib/algorithms/fits_output/p3_output.h`（178） | 全 178 行 | `:162` 承诺「缺 HDU … 均判失败」，被 `:660` 证伪；`:71` 称用 `fits_read_file`（实现中无此调用）；`:54` 称 coverage 是「扩展=coverage **二值**」，与 `:340` 写侧写浮点 coverage 面不协调；`:52` `P3Provenance` 无 `uncertainty_*` 字段但 `:31-32` 的注释块描述了它们 | 须修 |
| 3 | `lib/algorithms/fits_output/CMakeLists.txt`（27） | 全 27 行 | `:17-22` 真实 target 是 `acsd_p3_fits_output`、依赖 `acsd_common acsd_aio acsd_cfitsio acsd_p3_projection_wcs`，与同目录 `module.yaml:128-130` 声明的 `acsd_phase3_session/acsd_aio` 不符；全文件无 `add_test`，本模块无任何测试挂点；`:8` 注释带日期（违反 AGENTS §5） | 须修 |
| 4 | `lib/algorithms/fits_output/README.md`（126） | 全 126 行 | `:8-16` 声称「行数实测 2026-09-16」并给出可复跑命令，实测 **1270/178**（声称 556/95）；`:55` 称 BUNIT 缺省 `"ADU"`，代码 `:340/:408/:816` 是 `"ADU/sr"`（代码 `:337-339` 明确论证裸 `"ADU"` 量纲不可判）；`:7` 称本目录「仅合同文件、无源码」，但源就在此；`:8-9` 称源编入 `acsd_phase3_session`，根 `CMakeLists.txt:1285` 已记迁移；`:97-103` DISP-P3FITS-002 **自认**残留检查「与实际命名恒不匹配 → 弱匹配空转」，即一道恒真门 | 阻断（文档层） |
| 5 | `lib/algorithms/fits_output/memory.md`（157） | 全 157 行 | `:55`/`:61` 声称 64 行 / 370 行（实测 178 / 1270）；`:63`/`:76` 仍把已删除的 `fdatasum` 当现存记锚；`:72` BUNIT 记「缺省 ADU」；`:148-149` 仍称 `p3_output_verify` 忽略 wcs（`(void)wcs`），已被 `p3_output.cpp:547-549` 明确反转；`:94` 说 `p3_session.cpp` 329 行、`:119` 说测试 116 行，与 README 的 441/153 **互相矛盾**（实测 441 / 568）；`:147` 引 `SCI-P3 §96`，而同模块 README `:51-53` 亲口说 §96 不存在 | 阻断（文档层） |
| 6 | `lib/algorithms/fits_output/module.yaml`（134） | 全 134 行 | `:2-3` 行数 556/95（同上，已失效）；`:33-34` 称源在 `lib/phase3_session/`，`:37-38` 又称本目录「无源码」——同一文件内三处互斥；`:43`/`:49` 行锚 `:125`/`:52` 全漂移；`:111-120` `source_symbols` 列的是 `p3_wcs.h`（他域投影模块）的符号，却**完全不含**本模块真正的公开符号 `p3_output_write_atomic_ex`/`p3_output_verify_ex`/`P3FitsStream`；`:128-130` 依赖集与真实 CMake 不符 | 阻断（文档层） |
| 7 | `lib/algorithms/shared/healpix/healpix_core.cpp`（516） | 全 516 行 | `:255` 越界 ipix 静默返回 `(0,0)`——而 (RA=0°,Dec=0°) 是合法天区，伪装成真值；`:257`/`:128` 头注「12*2^62」在 order=31 时回绕为 0；`:328` `nside==0` 返回 `0.0`（同文件 `:234/:251/:441` 对非法 nside 是抛异常）——错误面不一致，且 0.0 分辨率下游即除零；`:376` `neighbors` 对 `nside==0` 返回空向量（同样不一致）；`:275`/`:280` `if (shift >= 32) shift = 31;` 静默钳位；`:315` `parent_nest` 无移位保护而 `:321` `child_nest` 有；`:457` `12ULL*(1<<order)^2` 在 order=31 回绕为 0 ⇒ 全球查询返回空；`:487` `std::max(0.0, …)` 把 NaN 洗成 0，`:490` `NaN <= x` 为假 ⇒ `:491-493` zone 判为 3 ⇒ **NaN 输入返回全天天球**（CE-3）；`:447-451` 负半径静默返回中心像素；`:412-413` 明写「8 槽可能含重复、真角返回 <8」，而 `healpix_core.h:108` 对返回长度零承诺；`:338-341` 自述邻居算法**移植自 GPL-2+ Healpix_3.83** | 阻断 |
| 8 | `lib/algorithms/shared/healpix/healpix_core.h`（114） | 全 114 行 | `:9` 「禁止在本模块之外维护第二套 ang2pix/pix2ang」——实测 `lib/algorithms/drizzle/healpix_drizzle/healpix_core.h`（转发 shim，合规）与 `lib/infrastructure/hips_browser/healpix_browser_qt/core/healpix_math.h:16,20`（自声明 `ang2pix_nest`/`pix2ang_nest`，**待他片核**）；`:6-8` 「1,000,000 点 + order 0..22 mismatch=0」是判据声明，无仓内可复跑物；`:76` `order_to_nside` 对 `order>=32` 是 UB，无保护（对比 `:97` `tile_to_leaf_nest` 显式抛 `overflow_error`）；`:108` `neighbors` 无返回长度契约 | 须修 |
| 9 | `lib/algorithms/shared/healpix/tests/test_healpix_neighbors.cpp`（245） | 全 245 行 | `:188-190` 恒真门（阻断 2）；`:75-94` `brute_disc` 用 `pix2ang_nest` —— 即 `query_disc` 内部 `:482` 用的**同一个原语**作「ground truth」，`:176-177` 注释自称「验证与暴力逐像素一致而非自包含」，但该暴力并不独立；`:52-69` 18 条硬编码 Gorski/astrometry 钉值**是真独立锚**（本片唯一合格外部锚）；`:137` `expect(!nb.empty())` 失败时无上下文（6000+ 次调用，定位不了） | 阻断 |
| 10 | `lib/algorithms/shared/healpix/tests/test_healpix_oracle.cpp`（118） | 全 118 行 | `:4` 声称读 `healpix_fullsky_oracle.py` 生成 —— 实测**全仓无此文件**，真实生成器是 `gen_spatial_fuzz.py`；`:112` 通过判据只含 `lines/bad_parse/mismatch/bad_roundtrip`，`:108-110` 打印的 `per_face[12]` 面覆盖统计**不进判据** ⇒ 「12 base face 锚点」覆盖不可强制（全落 face 0 也 PASS）；`:88` `if (fabs(dec) >= 89.999999) continue;` 先滤掉极点再做往返 —— 恰是 HEALPix 几何最易错处；`:92` 本地重算 `hp_res` 且硬编码 π 字面量，未复用库内 `pixel_resolution_arcsec` | 须修 |
| 11 | `lib/algorithms/shared/healpix/tests/test_hips_tile_mapping.cpp`（88） | 全 88 行 | `:36`/`:63`/`:70` 自洽式断言（阻断 3）；`:5` 悬空证据路径；`:45` 往返用被检函数的输出作输入，双向互逆对「与标准不符」零鉴别力；**全仓构建系统零引用本文件**（实测），永不编译 | 阻断 |
| 12 | `lib/algorithms/shared/healpix/tests/gen_spatial_fuzz.py`（56） | 全 56 行 | 真正用 `astropy_healpix` 算期望 ipix（`:27`）——**本片唯一真独立外部 oracle 生成器**；`:49` `a.adversarial // len(anchors)`，而 `len(anchors)`=**29**（`:37-46` 实数），默认 10000 ⇒ 产出 9976、**静默丢弃 24**；`--adversarial ≤28` ⇒ `range(0)` ⇒ **零对抗向量且退出码 0、无告警** | 须修 |
| 13 | `lib/algorithms/shared/healpix/tests/snr_hips_spatial_oracle.py`（75） | 全 75 行 | `:33` `astropy_healpix` 真独立 oracle；`:72` fail-closed 判据完整（`rows>0 and wrong==0 and dup==0 and actual==expected`）；`:46` `Dir*10000+Npix` 约定是**声明**（「Hipsgen LINT/CHECK 已验证」）非本脚本推导；`:29`/`:32` 无 try，缺文件/缺键直接 traceback（非静默放行，可接受）；标 `NON_PRODUCTION_TOOL_ONLY` 正确 | 通过 |
| 14 | `lib/algorithms/shared/healpix/THIRD_PARTY_NOTICE.md`（31） | 全 31 行 | `:19` 称「未迁移 …**邻居查询**」，但 `nealpix_core.cpp:374-415` 确有 `neighbors()` 且有 18 条钉值测试；`:22` 称「未复制任何 GPL（Healpix_cxx / RELION）代码进入生产树」，而 `healpix_core.cpp:338-341` 自述邻居算法**移植自 GPL-2+ Healpix_3.83** 且 `:345-370` 的 `kNbFaceArray[9][12]` / `kNbSwapArray[9][3]` 与官方 `Healpix_Tables` 结构逐格对应；许可文件**完全未列 Healpix_3.83 / GPL-2+**；`:14` 声称 1,000,000 点，仓内生成器默认 `--random 100000` | 阻断（许可层） |
| 15 | `lib/algorithms/shared/include/astro_scalar.h`（138） | 全 138 行 | `:109-111` 显式 `else { /* dtype 取值非法, 静默跳过 */ }` —— 非法 dtype **一行代码都不执行**，无错误码、无日志（阻断 5 的一半）；`:131-137` `ASTRO_SCALAR_DISPATCH_T` 连 else 都没有；`:68`/`:76` 非法枚举一律 `else` 到 `"float64"` / `8` 字节 —— 配置损坏时按 double 分配缓冲 | 阻断 |
| 16 | `lib/algorithms/shared/include/precision_context.h`（57） | 全 57 行 | `:38` `set_scalar_type` 零校验（与上游 fail-open 串成完整链）；`:56` 默认 FP32 —— 编排器读配置失败即**静默降精度**运行，而 `astro_scalar.h:6` 的设计意图是「要求全链路支持真正双精度」；`:29` 函数内静态局部单例，构造顺序依赖，首次调用线程不安全（C++11 起 magic static 已线程安全，但跨 TU 初始化顺序未约束） | 阻断（链路端） |
| 17 | `lib/algorithms/shared/tests/test_precision.cpp`（141） | 全 141 行 | 实为**头文件冒烟/characterization 测试**（类型映射、单例身份、宏分发），不验证任何精度**策略**（无 epsilon / NaN / 降级 / 非法枚举用例）——文件名与覆盖承诺不符；`:54` 断言默认值 FP32 属把现状钉成期望（change-detector）；**未进任何 ctest**（实测），只在孤立 Makefile 里 | 须修 |
| 18 | `lib/algorithms/shared/crypto/sha256.cpp`（120） | 全 120 行 | **盲复算通过**：64 个 K 常量、8 个 H 初值、σ0/σ1/Σ0/Σ1 旋转量、Ch/Maj、轮函数、填充与 64-bit 大端长度（`:96-97` `block_[63-i] = bits >> 8i` 正确，非经典错位）全部与 FIPS 180-4 逐位相符，`:89-95` 的 `block_len_+1>56` 分支边界正确。缺陷：`:86` 二次 `final_hex()` 返回 `64 个 '0'` —— **形似合法实则全错的摘要**，无异常无错误码；`:69` final 后 `update()` **静默丢数据**且返回 void，调用方无从得知 | 阻断 |
| 19 | `lib/algorithms/shared/crypto/sha256.h`（36） | 全 36 行 | `:14-17` 「原 `sha256_file` 全仓零调用者，已删除」——**实测为真**（`acsd::crypto::sha256_file` 无任何 C++ 调用点；`atomic_publish.cpp:228` 的 `sha256_file_hex` 是 aio/product_io 的另一函数，7 个活调用点，删除安全）；`:2` 称「astro_image_io 与 phase2 共用，单一实现」——`acsd::crypto::Sha256` 确有活消费者（`aio_upm.cpp:345,442`、`aio_abi.cpp:116,179`、`aio_file_io.h:244`），但「单一实现」在全仓被 `lib/infrastructure/aio/product_io/src/sha256.cpp` 的第二套实现推翻（他域，交叉线索） | 通过（退役声明成立） |
| 20 | `lib/algorithms/shared/dirent_win.h`（73） | 全 73 行 | `:3-4` 「全仓**零 include 点**…属待清理的遗留件」——**实测为真**（除 `README.md:16` 的目录条目外，全仓零 `#include "dirent_win.h"`）；已按 AGENTS §6 写明保留原因，**判定维持通过**。已自认的不等价：`:41` `opendir(nullptr)` 静默取 `"."`、1024 路径缓冲截断、`:12-13` `_WIN32` 守卫对 MinGW 不安全。**未被自认的**是 `:26` `d_name[260]` 的**单路径分量 260 上限**（MAX_PATH=260 含 NUL，故 260 本身正确，见子代理 S5 负结果 1）；另有 `:44-49` 丢弃 `GetLastError`。`:17` 用 `_WIN32` 而非 `_MSC_VER` 属**已自认缺陷**，按 AGENTS §6 保留合规，仅建议后续删除 |
| 21 | `lib/algorithms/shared/Makefile`（49） | 全 49 行 | `:4`/`:15` 通篇写 `lib/common` 与 `-I../common/include` —— 该目录**不存在**（已改名 `lib/algorithms/shared`），悬空引用；`:48-49` `clean` 用 `del /f /q`（Windows 命令）而 `:18`/`:39` 用 `g++` 与 `./`，跨平台自相矛盾；`:26-27`/`:42-45` `healpix-oracle` 目标**只编译不运行**，且所需 `oracle.jsonl` 无任何仓内目标生产；`:21` `test_precision` 不在 CMake/ctest 内；`test_healpix_neighbors.cpp` / `test_hips_tile_mapping.cpp` 在本 Makefile 里根本没有条目 | 须修 |
| 22 | `lib/algorithms/shared/README.md`（23） | 全 23 行 | `:13-17` 列出的目录/文件逐条实测存在；`:22` 引 `docs/ACSD_DESIGN.md §8.4/§8.5`、`:24` 引 `DOCUMENT_GOVERNANCE.md §2` —— 两者实测存在；`:14` 只说「自带测试（tests/）」，未提两个 `.py` oracle | 通过 |
| 23 | `lib/algorithms/shared/.gitignore`（14） | 全 14 行 | `:2-8` 覆盖 Makefile 实际产物（`.exe`）；`:11-14` 覆盖临时/日志；无缺口 | 通过 |
| 24 | `lib/algorithms/upm/README.md`（220） | 全 220 行 | `:109-110` 「**模块内 std::thread 池**…2026-09-10 实测 0 处 hardware_concurrency」——自认算法模块私建线程池（项目明禁，池所有权归调度器）；`:136-139` 默认值 huber_delta=1.345（标准值但**未给出处**）/ tolerance=1e-6 / sigma_floor=1e-3 / zero_anchor_weight=1e-3 / control_reliability=1.0 均无推导；`:9` 称 upm.cpp 1565 行（实测 2981）；`:115-134` 16 符号表与 `module.yaml:116-127` 的 11 符号**不一致**（W1 收缩只改了 yaml 没改表）；`:202-205`/`:206-219` 整节 markdown 转义损坏（字面 `\`、`\*\*`） | 须修 |
| 25 | `lib/algorithms/upm/memory.md`（165） | 全 165 行 | `:46` 再次自认 `compute_raw std::thread 池`；`:42-45` 同批无出处魔法值；`:154-155` `k_corr=1.4` **自认未标定**（DI-04 OPEN）却作为公式常量使用；`:33` upm.h 184 行（实测 460）；`:139`「原引宪章 §6.3 已废止」= 历史叙事（违反 AGENTS §5）；`:128-164` 全节转义损坏；`:156` 证据 `run/v6/IMPL-P2-UPM-001/` 实测**不存在** | 须修 |
| 26 | `lib/algorithms/upm/module.yaml`（143） | 全 143 行 | `:42`/`:44` 再述模块内 `std::thread 池`（违规自认）；`:135` `scientific_change: false` 与 `memory.md:126-164` 的 V6 乘加分离/协方差传播/新 k_corr 大改直接矛盾；`:80-90` `algorithm_contracts` **不含** V6 实际挂靠的 `ALG-P2S-UPM.1`；`:116-127` 与 README 符号表不一致（同上） | 须修 |
| 27 | `lib/algorithms/rejection/README.md`（121） | 全 121 行 | `:42-43` AUTO 分档阈值 `n<6 / 6..15 / >15` **无任何推导或出处**；`:61` `|median| floor 1e-12` 无出处；`:79-80` `enabled=0/min_structure=8/半径 2-2` 无出处；`:95` `F7 rtol 1e-12` 无出处；`:118` DISP-P2REJ-001 自认头注释 `low_fraction 默认 0.1` 与实现/SCI 的 `0.2` 矛盾（**登记未修**，直接影响排异分数）；`:121` DISP-P2REJ-004 自认 minmax 比较器 value-only、「等值样本 permutation 不变性未承诺」；`:84-87` 却在同一文件宣称「与 worker 数无关 **bitwise**」——**自相矛盾**；`:9`/`:11`/`:12` 行数 2076/329/1762（实测 2959/603/2019） | 须修 |
| 28 | `lib/algorithms/rejection/memory.md`（119） | 全 119 行 | `:37` `ESD tie-break frame_id 1e-15` 魔法容差无出处（且同文件 `:36` linear_fit 的 tie-break 已冻结，minmax 却没有——策略不一致）；`:19-20` 行数 2076/329（实测 2959/603）；`:66-68` DISP-P2REJ-004 复述未决；`:113-116` 「12 条负向 mutation 全部被检出；shadow ctest 6/6 PASS；生产 flags 编译零告警」为**自述通过**，本片无法复跑，按裁定不采信 | 须修 |
| 29 | `lib/algorithms/rejection/module.yaml`（106） | 全 106 行 | `:97` `determinism: fixed_reduction_order` 与同文件 `:106` `known_defects: DISP-P2REJ-004`（permutation 不变性未承诺）**同文件内直接互斥**；`:98` `scientific_change: false` 与 `memory.md:88-118` 的 V6 分类排异实现矛盾；`:61-62` `algorithm_contracts` 不含 `CLASSIFY_V1_PROFILE.md:4` 声明的 `ALG-P2S-REJ.1..7`；`:80-86` 「下列符号已从 source_symbols 撤下」却**不列出被撤的是哪些**，清单不可审计；`:54-58` `accepted_mask` 同时是 input 与 output port | 阻断（文档层） |
| 30 | `lib/algorithms/rejection/CLASSIFY_V1_PROFILE.md`（113） | 全 113 行 | `:28-29` `z<=-4.0 / z>=+3.0` 非对称且无统计依据；`:33-35` 继承阈值表 `4.0/3.0/8`、`5.0/3.5/8`、`0.2/0.1`、`alpha 0.05/max 10`、`8/2/2`、`1/1/4` 全为无出处字面量；`:46`/`:48` `motion_min_px=0.5`、`psf_anomaly_min=0.2` 无出处；`:68-69` `contamination_prior=0.05`、`kappa=4.0` 自认「版本化 profile 常量」但仍无推导；`:89-90` `BINMIN=50 / ABS=0.10 / BSS_MIN=0.10` 自认 `PENDING_OWNER_SIGNOFF` 未签字却已按值生效；`:108-112` 「独立 Oracle / ctest 6/6 / 12 条 mutation 全红」为自述，且所引 `run/v6/IMPL-P2-REJ-001/mutations.json` 实测**不存在** | 须修 |
| 31 | `lib/algorithms/sampling/README.md`（125） | 全 125 行 | `:121` **DISP-P2SMP-001 自认「配置修补 `<=0→默认` 吞显式 0」**——用户显式传 `background_clip_iters=0`（意图关闭 clipping）被静默改写为 3（开启）。这是负责人点名首位的静默降级，**登记至今未修**；`:125` **DISP-P2SMP-002 自认 `rejected_insufficient_retained` 双计数**（「统计面偏差」）——对外统计错，登记未修；`:123` **DISP-P2SMP-004 自认 veto 阈值 `10×frame_snr_med` 与半径 `0.012°` 硬编码未入配置**，两数均无推导；`:124` **DISP-P2SMP-005 自认 `1e-12×max(|m0|,1e-12)` 在 m0≈0 时钳位使收敛判据失效**（「筛掉真信号」的 clamp 形态），被作者自评为「性能观察级」；`:74-81` 同一段内先说「模块无 hardware_concurrency 自行开线程」紧接着说「cpu_workers>1 走 **std::thread 池**」——**自相矛盾，且是模块私建线程池**；`:51-54` `k_corr` 缺失时**静默回退未标定的 1.4**；`:82-85` 错误面只有 `rc=0/rc=1 + 自由文本`，n_union>1e6 / cells>2e8 / OOM / 越界 / exception **全部塌缩成一个码**（违反「每个失败产生稳定错误码」）；`:86-87` `1.482602218505602` 是正确的 MAD 一致性常数但未给出处 | 阻断 |
| 32 | `lib/algorithms/sampling/memory.md`（119） | 全 119 行 | `:60-69` 复述上述 5 条已登记未修缺陷（`<=0→默认` 吞 0、双计数、stderr 直写、硬编码阈值、钳位收敛失效）；`:61` 引 `run/local/bughunt/ledger.md:247-250` —— 实测**不存在**；`:19-20` 行数 1156/136（实测 1503/275）；`:22` `control_k_corr 默认 1.4`；`:113-116` 「98 checks PASS / 8 注入全红」为自述，证据 `run/v6/p2-samp/` 实测**不存在** | 阻断（文档层） |
| 33 | `lib/algorithms/sampling/module.yaml`（113） | 全 113 行 | `:29-32` 同一段内「模块不自行开线程」与「cpu_workers>1 时 **std::thread 池**」并存互斥；`:103` `determinism: fixed_reduction_order` 在 DISP-P2SMP-002 双计数存在下仍照登；`:104` `scientific_change: false` 与 `memory.md:89-118` V6 新增 5 组纯函数面矛盾；`:86-97` W1 收缩同样不列被撤符号 | 须修 |

---

## 4. 发现清单

### 4.1 阻断（7）

| ID | 位置 | 问题 | 类别 |
|---|---|---|---|
| **B-1** | `p3_output.cpp:660`（+ `:1153-1161` 对照、`:570`、`:1102`、`p3_output.h:162`） | coverage HDU 校验块缺 `else`，缺面时 `covok` 恒 1 → `reopen_ok=1`、`coverage_ok=1`；`fits_get_num_hdus` 返回值丢弃构成第二条通路。公共头承诺的「缺 HDU 判失败」未兑现 | 静默降级（fail-open）+ 恒真门 |
| **B-2** | `test_healpix_neighbors.cpp:189` vs `healpix_core.cpp:449` | 期望量与被检量是同一次 `ang2pix_nest(4,10,20)` 调用 | 自洽式断言（恒真门） |
| **B-3** | `test_hips_tile_mapping.cpp:36/63/70` vs `healpix_core.cpp:292` | 期望式是实现体逐字抄写；所引外部 oracle 证据 `run/temp/v5_oracle` 不存在；全仓构建系统零引用该文件 | 自洽式断言 + 悬空引用 + 判据从未执行 |
| **B-4** | `astro_scalar.h:109-111`/`:131-137` + `precision_context.h:38` + `astro_scalar.h:68/76` | 非法 dtype 时分发宏静默跳过、什么都不执行；`set_scalar_type` 零校验；非法枚举一律 `else` 成 FP64/8 字节。配置损坏 → 无错误码 → 按 double 分配缓冲 → 静默错误结果 | 静默降级（完整 fail-open 链） |
| **B-5** | `sha256.cpp:86`（+ `:69`） | 二次 `final_hex()` 返回 64 个 `'0'` —— 形似合法摘要的全错值，直接进 provenance；final 后 `update()` 静默丢数据 | 静默降级 |
| **B-6** | `fits_output/README.md:8-16` + `module.yaml:2-3` + `memory.md:55,61`；`upm/{README,memory}.md`；`rejection/{README,memory}.md`；`sampling/{README,memory}.md` | **全部 10 份合同文件的「实测行数」声明已失效**（实测 vs 声称：upm.cpp 2981/1565、upm.h 460/184、rejection.cpp 2959/2076、rejection.h 603/329、sampler.cpp 1503/1156、sampler.h 275/136、stage2.cpp 2019/1762、p2_session.cpp 318/282、p3_output.cpp 1270/556、p3_output.h 178/95、p3_output_test.cpp 568/153 与 568/116）；`memory.md` 与 `README.md` 对同一文件给出**互相矛盾**的行数 | 判据不可信（自述实测被复算推翻） |
| **B-7** | `docs/engineering/STANDARDS_REGISTRY.md:139-141`（引用本片三个测试为 CONFORMANT 证据） | **标准符合性登记册用本片测试背书了三项虚假 CONFORMANT**：①`:139` 记 §5.3「往返 ≤1e-12 deg；astropy-healpix 百万点 oracle 对拍」，而实装判据是 `test_healpix_oracle.cpp:95` 的 `d > 1.2*hp_res_deg + 1e-9`（order 7 时 hp_res=0.458065° ⇒ 真实门槛 **0.549678°**），与所记容差差 **~5.5×10¹¹ 倍**；且「百万点 oracle 对拍」在活树无可执行面（见 M-16）。②`:140` 记「order ≤ 29 且越界输入显式拒绝 CONFORMANT」，但所引 `test_healpix_neighbors.cpp:224-235` **恰恰把 order=31 当合法域使用**（`:228` `tile_to_leaf_nest(2ULL, 0, 31)` 必须不抛），且 `healpix_core.h:60-65` `require_valid_nside` **没有 order 上界**——**所引测试证明的是命题的反面**。③`:141` 记「重复实现须**机器门禁**…机器门禁登记在位」，而本域机器门只覆盖 `test_healpix_neighbors`（M-16） | 判据不可信（登记册背书不成立的判据） |

（另 `THIRD_PARTY_NOTICE.md:19/22` 与 `healpix_core.cpp:338-341` 的 GPL-2+ 归属冲突，按许可严重度并入阻断口径处理，见 §4.2 G-1。）

### 4.2 须修（29）

| ID | 位置 | 问题 |
|---|---|---|
| M-1 | `rejection/module.yaml:97` vs `:106` | `determinism: fixed_reduction_order` 与 DISP-P2REJ-004（permutation 不变性未承诺）同文件互斥 |
| M-2 | `sampling/README.md:121` / `memory.md:60-61` | 配置修补 `<=0→默认` **吞掉显式的 0**：`background_clip_iters=0`（想关）反得 3（开）。已登记未修 |
| M-3 | `sampling/README.md:125` / `memory.md:62-64` | `rejected_insufficient_retained` 双计数 ⇒ 对外统计面偏差。已登记未修 |
| M-4 | `sampling/README.md:123` / `memory.md:66-67` | veto 阈值 `10×frame_snr_med`、半径 `0.012°` 硬编码未入 config，且两数无任何推导 |
| M-5 | `sampling/README.md:124` / `memory.md:68-69` | `1e-12×max(\|m0\|,1e-12)` 钳位在 m0≈0 时使收敛判据恒不成立，退化为固定轮数；被自评为「性能观察级」，实为掩盖收敛失败 |
| M-6 | `sampling/README.md:74-81`、`sampling/module.yaml:29-32`、`upm/README.md:109-110`、`upm/module.yaml:42-44`、`upm/memory.md:46` | 算法模块私建 `std::thread` 池（池所有权归调度器，项目明禁）；且两处文档在同一段内先否认后承认，**自相矛盾** |
| M-7 | `healpix_core.cpp:328`（对照 `:234/:251/:441`）、`:376` | 错误面不一致：`pixel_resolution_arcsec(0)` 返 `0.0`、`neighbors(0,…)` 返空，同模块其余入口对非法 nside 抛 `std::invalid_argument`。`0.0` 分辨率下游即除零 |
| M-8 | `healpix_core.cpp:275`/`:280` | `if (shift >= 32) shift = 31;` 静默钳位，非法 shift 变成完全错误的局部索引而非报错 |
| M-9 | `healpix_core.cpp:315` vs `:321`；`healpix_core.h:76` vs `:97` | 硬化不对称：`parent_nest` 无移位保护而 `child_nest` 有；`order_to_nside(order>=32)` 是 UB 而 `tile_to_leaf_nest` 显式抛 `overflow_error` |
| M-10 | `healpix_core.cpp:255` | 越界 ipix 静默返回 `(RA=0°, Dec=0°)`——那是**合法天区**（春分点方向），伪装成真值；头注 `:29` 把它写成「与调用方约定一致」 |
| M-11 | `healpix_core.h:108` / `healpix_core.cpp:412-413` | `neighbors()` 在真角返回 **<8** 个且可能含重复，头文件零契约说明 ⇒ 调用方按 8 下标即越界 |
| M-12 | `p3_output.cpp:545`（对照 `:259`） | 公共验证入口 `p3_output_verify_ex` 不校验 `wcs`/`signal`/`coverage` 空指针，而 `:578`/`:650`/`:740` 立即解引用 ⇒ 传 nullptr 即段错误；写侧 `:259` 却做了同样检查 |
| M-13 | `p3_output.cpp:1065` 注释 vs `P3FitsVerifyStream`（`:1088-1195`） | 注释宣称「与 `p3_output_verify_ex` 同判据」，实测流式面**不做**逐 HDU DATASUM/CHECKSUM（整幅面 `:575/:662/:684` 做了），**也不做** BUNIT 对拍 ⇒ 流式产品完整性判据弱于整幅 |
| M-14 | `p3_output.cpp:526-527` | 刚在 `:498-503` 从实测 `v` 拷贝来的 `coverage_ok`/`reopen_ok` 被**字面常量 1 覆写**；判据一旦演化，常量即成谎言 |
| M-15 | `THIRD_PARTY_NOTICE.md:19` vs `healpix_core.cpp:374`；`:22` vs `:338-370` | 许可文件称「未迁移邻居查询」（实际有）、称「未复制任何 GPL 代码」（代码自述移植自 GPL-2+ Healpix_3.83 且 `kNbFaceArray[9][12]`/`kNbSwapArray[9][3]` 与官方 `Healpix_Tables` 逐格对应）；许可清单**未列** Healpix_3.83 / GPL-2+ |
| M-16 | 构建挂载 | 5 个测试里 **4 个不在任何自动化入口**：`test_hips_tile_mapping.cpp` 全仓零引用；`test_healpix_oracle.cpp` 只在孤立 Makefile 且目标只编译不跑、不产 JSONL；`test_precision.cpp` 同；两个 `.py` oracle 无任何调用者。**唯一真正独立的外部 oracle（`gen_spatial_fuzz.py`→`test_healpix_oracle.cpp`）在活树里从不执行**，因此 `healpix_core.h:6-8` 的「1,000,000 点 mismatch=0」在仓内无可复跑支撑物 |
| M-17 | `precision_context.h:25-56` + `test_precision.cpp:2,45-62` | **结构性设计冲突**：正本 `ASTROCS_DESIGN.md §3.3` 要求**分离精度策略**（稠密大面/HiPS tile 默认单精度；稀疏与元数据**全程双精度**），而 `PrecisionContext` 是**单一进程全局标量**（`:25-29` 单例），结构上无法表达「按数据面分流」的策略；`:15` 更明写「默认值: FP32，保证未显式初始化时与历史行为兼容」，与「全程双精度」正面冲突。而唯一配套测试 `test_precision.cpp` 的 `static_assert`（`:23-30`）与运行期检查（`:45-48`、`:54-56`）是对 `astro_scalar.h:48-61/68/76`、`precision_context.h:56` 的**逐字转写**，对头文件自洽，**不验证任何策略**，却在 `:139` 打印 `[OK] all precision header checks passed` —— 一道为违规设计背书的绿灯 |
| M-18 | `test_healpix_oracle.cpp:86-88` | 跳过极点的**注释理由在算术上是错的**。注释称极点像素中心距「可远大于像素半径」。实测：order 7 / nside=128 极点像素中心 `dec=89.634517°`，极点→中心距离 **0.365483°**，而该行门槛 `1.2*hp_res = 0.549678°` ⇒ **0.365 < 0.550，跳过在几何上并不必要**。即：跳过没有几何理由，却恰好剔除了 `atan2` 近极点与极冠 `floor` 钳位（`healpix_core.cpp:103-106`）这两处最易错区，且错误注释会让后续真实极区缺陷被放行 |
| M-19 | `test_healpix_oracle.cpp:94-95` | **NaN 双向漏过**：两个比较都是 `>`。`d==NaN` 时 `NaN > max` 为假（不进最坏统计）、`NaN > 1.2*hp_res+1e-9` 为假（**不计失败**）⇒ NaN 行既不拉高 `max_roundtrip_deg` 也不计 `bad_roundtrip`，双重放行。可达：`:31-32` 用裸 `std::strtod`（接受 `nan`/`inf`），`healpix_core.cpp:304-312` 把 NaN 透传进 `acos` |
| M-20 | `test_healpix_oracle.cpp:69` vs `:80` | **UB 早于自身守卫**：`const uint32_t nside = uint32_t(1) << order;` 对每条记录无条件执行，而 `if (order <= 22)` 守卫在 **11 行之后**（`:80`）且只保护面计数器。解析器 `:30` 接受操作员提供的任意 uint32 ⇒ `{"order":99,...}` 触发 `1u << 99`（UB）。守卫的存在证明作者知道范围，只是放晚了 |
| M-21 | `gen_spatial_fuzz.py:49` | `range(a.adversarial // len(anchors))`，而 `len(anchors)=29`（`:37-46` 实数）。默认 `--adversarial 10000` ⇒ 344×29=**9976 产出、24 静默丢弃**；`--adversarial 28/1/0` ⇒ `range(0)` ⇒ **产出零对抗向量、退出码 0、无告警**，调用方拿到一份与非对抗语料同质的文件却收到「成功」 |
| M-22 | `test_precision.cpp:66` | **恒真门**：`RUNTIME_CHECK(&ctx == &ctx2, "singleton identity")`。`instance()` 返回绑定到**函数内 static** 的引用（`precision_context.h:28-31`），两次调用同址由**语言与签名**保证，该断言不可能失败 |
| M-23 | `test_precision.cpp:27-28`、`:45-46` | **自洽式断言**：`static_assert(...size == 4)`、`astro_scalar_type_size(FP32) == 4` 是**字面量对同字面量**。正确写法应为 `size == sizeof(float)`。当前值虽对，但这类断言**检测不出**「`astro_scalar.h:51/59`、`:76`、`:68` 三处重复硬编码 4/8 且无 `static_assert` 绑到 `sizeof`」这类漂移 |
| M-24 | `sha256.cpp:68-74` | `update(nullptr, len>0)` ⇒ `memcpy(dst, nullptr, len)` = UB，无空指针检查。`len==0` 路径安全（`while(len>0)` 不进入）。仓内 C ABI 边界**有**防护（`aio_abi_tests.cpp:304` 断言空指针 ⇒ `AIO_ERR_PARAM`），**C++ 入口没有** |
| M-25 | `precision_context.h:6` vs `:28-31` | 头注声称「只能存在唯一一个 PrecisionContext」，而 `instance()` 是**函数内 static** ⇒ 每个 SO/DLL 各持一份。仓内已撞过此坑（`astro_image_io.h:97`「PrecisionContext 单例在 DLL 边界不共享，必须通过显式 API 传递精度」）。`acsd_common` 现为 STATIC 故**潜伏**，但头文件声明了一个设计无法保证的不变量 |
| M-26 | `healpix_core.cpp:238`（对照 `healpix_core.h:24`） | `ang2pix_nest` **静默接受 `|dec|>90`**：`dec` 直接进 `std::sin`（周期函数），`dec=95°` 静默返回 `dec=85°` 的像素。头注只把 `dec ∈ [-90,90]` 写成**前置条件**而**无校验、无测试** |
| M-27 | `healpix_core.h:53` / `.cpp:314` | **`parent_nest` 全仓零调用点、零测试覆盖**：全仓 grep 只有声明（`h:53`）与定义（`cpp:314`）两处命中。它同时是本片**唯一无移位保护**的层级函数（对比 `child_nest:321`、`tile_to_leaf_nest: h:97`）——已导出的死 API 带一个未实现的防护 |
| M-28 | `shared/Makefile:48-49` + `.gitignore:1-14` | `del /f /q $(TEST_BIN) 2>nul` 是 CMD.exe 语法，而同文件 `:18` 用 `g++`、`:39` 用 `./`。在 POSIX shell 上 `del` 不存在（`make clean` 失败），且 `2>nul` 会**在仓内创建一个名为 `nul` 的文件**；我逐行读过 `.gitignore` 全部 14 行（`*.exe/*.o/*.obj/*.dll/*.lib/*.exp/*.pdb/*.tmp/err*.txt/stderr*.txt/stdout*.txt`），**`nul` 未被忽略** ⇒ 一次 `make clean` 即在源码树留下未跟踪文件 |
| M-29 | `shared/Makefile` 整体 | **整条构建路径零挂载**：`add_subdirectory(lib/algorithms/shared)` 在全仓 CMake 中**零命中**，`shared/Makefile` 在任何 CMakeLists 中零命中；真实装配是根 `CMakeLists.txt:530-537` 的 `acsd_common STATIC`。后果：`shared/tests/test_precision.cpp` **不被任何东西编译**、无 CTest 注册——精度头的唯一测试从不执行。这是一条**仍在维护、却从不运行**的影子构建系统（AGENTS §6 要求退役代码删除） |

### 4.3 建议（9）

| ID | 位置 | 问题 |
|---|---|---|
| S-1 | `healpix_core.cpp:487-493` | `std::max(0.0, …)` 把 NaN 洗成 0 之后，`NaN <= x` 为假 ⇒ zone 判 3 ⇒ **NaN 经纬输入使 `query_disc` 返回全天天球**（CE-3） |
| S-2 | `healpix_core.cpp:255`/`:380`/`:457` + `:83`/`:103`/`:157`/`:387`/`:388` | `12*2^62 ≡ 0 (mod 2^64)` ⇒ nside=2^31 时 `pix2ang_nest` 对**每个**像素返回 (0,0)、`neighbors` 对每个像素返回空、`query_disc` 全球分支返回空；且 `static_cast<int>(2^31)` = INT_MIN ⇒ `:103`/`:388` 的 `ns - 1` 是**有符号溢出 UB**。注释 `:456` 的「order ≤ 29 合法域」**无任何机制强制** ⇒ 真实上限是 order 30，而 `require_valid_nside` （`healpix_core.h:60-65`）不设上界。`STANDARDS_REGISTRY.md:140` 却把「order ≤ 29 且越界显式拒绝」记为 CONFORMANT |
| S-3 | `healpix_core.cpp:447-451` | 负半径静默返回中心像素（已注释为「与既有调用约定一致」） |
| S-4 | `p3_output.cpp:671` / `:1226` | coverage 只按 `>0.5f` 二值比对 ⇒ 幅值任意错（0.9 写成 0.4）仍判过；signal/uncertainty 是逐值精确回环，唯独 coverage 不是 |
| S-5 | `p3_output.cpp:523` / `:740` / `:1228` | coverage 统计 `>0.5f` 把 NaN 记为「未覆盖」⇒ 最坏像素恰被筛掉后才取计数 |
| S-6 | `p3_output.cpp:553-554` vs `:637`/`:1139` | 注释承诺 CD 用 `1e-15`，实测统一 `1e-12` |
| S-7 | `test_healpix_oracle.cpp:88` | `fabs(dec) >= 89.999999` 先滤掉极点再做往返 —— 滤掉的恰是几何最易错区。**且该跳过所依据的注释在算术上是错的**（见 M-18） |
| S-8 | `test_healpix_oracle.cpp:108-112` | `per_face[12]` 面覆盖统计只打印**不进判据** ⇒ 「12 base face 锚点」不可强制；另 `:92` 本地重算 `hp_res` 并硬编码 π，未复用库内 `pixel_resolution_arcsec` |
| S-9 | 文档格式 | `upm/README.md:202-219`、`upm/memory.md:128-164` 整节 markdown 被转义损坏（字面 `` \` `` 与 `\*\*`）；`upm/memory.md:139`、`rejection/memory.md:88`、`fits_output/module.yaml:1,104` 等带日期/流水号，违反 AGENTS §5 |

---

## 5. 你主动构造的反例

| # | 构造 | 期望推翻什么 | 是否推翻 |
|---|---|---|---|
| CE-1 | 造一个只含 PRIMARY HDU（signal）+ 无 COVERAGE 扩展的合法 FITS，调用 `p3_output_verify_ex(path, wcs, signal, coverage, nullptr, nullptr, W, H, &r)` | 头文件 `p3_output.h:162` 承诺「缺 HDU 判失败」⇒ 应 `reopen_ok=0` | **推翻**。`p3_output.cpp:660` 条件不成立即整块跳过，`covok` 留在 `:557` 的初值 1 ⇒ `:736` `reopen_ok=1`、`:738` `coverage_ok=1`。同文件 `:1153-1161` 的流式路径有 `else`，对照即证遗漏 |
| CE-2 | 让 `fits_get_num_hdus` 失败（截断/损坏文件），观察 `hdus` | 计数失败应报错 | **推翻**。`:570`（及 `:1102`）返回值丢弃，`hdus` 停在 `:560`/`:1072` 的初值 1 ⇒ `hdus>=2` 恒假 ⇒ 与 CE-1 同一漏洞。**两条独立触发路径** |
| CE-3 | `query_disc(1024, NaN, NaN, 3600.0)` | NaN 输入应被拒或返回空 | **推翻**。`:446` `cz=NaN` → `:487` 的 `std::max(0.0, NaN·…)` 得 0 但 `z*cz` 仍 NaN ⇒ `cangdist=NaN` ⇒ `:488-489` 两个比较均为假 ⇒ `:490` `NaN<=x` 为假（**不 continue**）⇒ `:491-493` zone=3 ⇒ `:494-500` 整棵子树直出 ⇒ **返回全天天球**。`:487` 的 clamp 正是掩盖奇点的「筛掉真信号」形态 |
| CE-4 | `pixel_resolution_arcsec(0)` 后把结果用作采样步长/像元尺度 | 非法 nside 应抛异常（模块其余入口皆如此） | **推翻**。`:328` 返回 `0.0` ⇒ 下游任何 `x/res` 即除零。同文件 `:234/:251/:441` 对同一非法输入抛 `std::invalid_argument` ⇒ 错误面自相矛盾 |
| CE-5 | `Sha256 s; s.update(d,n); s.final_hex(); s.final_hex();` | 二次 final 应报错 | **推翻**。`:86` 返回 `std::string(64,'0')` —— **长度正确、字符合法的伪摘要**，与真实哈希在 provenance 里不可区分。`s.update(...)` 在 final 之后（`:69`）**静默丢数据**且返回 void |
| CE-6 | `PrecisionContext::instance().set_scalar_type(static_cast<AstroScalarType>(7)); ASTRO_SCALAR_DISPATCH(ctx.scalar_type(), …)` | 非法配置值应产生错误码 | **推翻**。`precision_context.h:38` 零校验收下；`astro_scalar.h:109-111` 的 `else` 分支**故意**什么都不做（注释自称「静默跳过」）⇒ 整段算法不执行、无返回值、无日志；同时 `:68`/`:76` 把 7 报成 `"float64"` / `8` 字节 ⇒ 若调用方按 `scalar_size()` 分配缓冲，尺寸按 double 给、数据按 float 用。**一条从配置损坏直达错误结果的完整 fail-open 链** |
| CE-7 | `nested_local_to_xy(local, 40, x, y)` | 非法 shift 应报错 | **推翻**。`:275` 静默改成 `shift=31`，返回完全错误的 (x,y)，无任何信号 |
| CE-8 | 读 `neighbors(4, 5)` 返回值长度并按 0..7 下标访问 | 头文件 `healpix_core.h:108` 未承诺长度 | **推翻（头文件契约缺失）**。`healpix_core.cpp:404` 真角 `continue` 使返回 <8，`:412-413` 注释也自认可能重复；`:52-69` 的钉值里 `{5,…}`、`{127,…}`、`{64,…}`、`{133,…}`、`{42,…}` 都是 7 元素 ⇒ 行为确实如此，但契约没写 |
| CE-9 | 遮住实现读 `test_healpix_neighbors.cpp:188-190`，判断 `got[0] == ang2pix_nest(4,10,20)` 能否失败 | — | **该断言恒真**。`got[0]` 按 `healpix_core.cpp:449` 就是同一次调用。零半径语义的任何回归都不会被拦下 |
| CE-10 | 遮住实现读 `test_hips_tile_mapping.cpp:36`，判断 `expect` 是否来自任何外部冻结数据 | — | **该断言与实现同构**。`healpix_core.cpp:292` 的 `(maxv-x)*tile_width + y` 与 `(511-x)*512 + y` 逐字符对应；`:5` 的外部 oracle 证据路径不存在 ⇒ HiPS 标准若理解错，测试照绿 |
| CE-11 | 反推 `query_disc` 的「暴力 ground truth」是否独立 | — | **半独立**。`test_healpix_neighbors.cpp:75-94` 的 `brute_disc` 用 `pix2ang_nest`，而 `healpix_core.cpp:482` 的 `query_disc` 内部也用 `pix2ang_nest` ⇒ 若 `pix2ang_nest` 整体系统性偏移，两侧同步偏移，测试恒绿。`:176-177` 注释自称「而非自包含」——**该独立性声明不成立** |
| CE-12 | 复跑 README 自己给出的行数命令 | — | **推翻**。`README.md:13-16` 的 `wc -l` 现给出 **1270 / 178**，非其声称的 556 / 95。10 份合同文件同类声明全部失效（见 B-6） |
| CE-13 | 核 `crypto::sha256_file` 「全仓零调用者」声明 | — | **未推翻（声明为真）**。`acsd::crypto::sha256_file` 零 C++ 调用点；`atomic_publish.cpp:228` 的 `sha256_file_hex` 属 aio/product_io 的**另一函数**，7 个活调用点，删除安全 |
| CE-14 | 核 `dirent_win.h` 「全仓零 include 点」声明 | — | **未推翻（声明为真）**。除 `README.md:16` 的目录条目外，全仓零 include；已按 AGENTS §6 写明保留原因 |
| CE-15 | 逐位复算 SHA-256（64 K 常量、8 H 初值、σ0/σ1/Σ0/Σ1 旋转、Ch/Maj、轮函数、`:89-95` 填充分支、`:96-97` 64-bit 大端长度） | — | **未推翻（实现正确）**。与 FIPS 180-4 逐位相符；`:89` 的 `block_len_+1>56` 边界与 `:96-97` 的 `block_[63-i]=bits>>8i` 均正确，**不是**经典错位 bug。缺陷只在 API 语义（CE-5） |
| CE-16 | 核 `healpix_core.h:9` 「禁止第二套 ang2pix/pix2ang」 | — | **未被推翻**。`drizzle/healpix_drizzle/healpix_core.h` 是合规转发 shim（有标准 RETIRED-CODE-RETAINED 块）；`hips_browser/healpix_browser_qt/core/healpix_math.h:16,20` 虽自声明 `ang2pix_nest`/`pix2ang_nest`，但我读其 `.cpp:26,34` 确认是**委托 `acsd::healpix`** ⇒ 单源纪律成立 |
| **CE-17** | 把 `healpix_core.cpp:41-42` 的偶/奇位角色整体对调（`ix` 取奇位、`iy` 取偶位），跑 `test_hips_tile_mapping.cpp` | 该测试声称冻结 HiPS tile 排列（文件头称外部 Oracle 逐像素验证）⇒ 转置应被检出 | **推翻（且比 B-3 所述更严重）**。`local=1, shift=9` 下现实现 `nest_to_xy` 给 `x=1,y=0` ⇒ `nested_local_to_fits_index = (511-1)*512+0 = 261120`；位角色对调后给 `x=0,y=1` ⇒ `(511-0)*512+1 = 261633`。而测试在 `:28` **先从被检实现取 x,y**，`:36` 再用**同一对 x,y** 造 `expect`，`:29`/`:45` 的 `back`/`inv` 也经由同一对被污染的 x/y ⇒ `bad_xy=0`、`bad_fi=0`、`bad_inv=0`，**整个文件全绿，而每个 HiPS tile 已被转置**。这把 B-3 从「期望式抄写实现」推进为「对 x/y 转置这一缺陷类别零鉴别力」 |

---

## 6. 盲复算

方法：先遮住本片任何既有判定文本与既有报告，只用「原始文件 + 命令」独立取证，再与本报告结论对齐。

盲复算的独立动作与结果：

1. **重算 SHA-256 正确性**（不看任何既有结论）：逐常量、逐旋转、逐填充分支手推 FIPS 180-4 → **与实现一致**。结论「算法正确、API 语义有洞」独立成立。
2. **重算 HiPS tile 映射判据**：把 `test_hips_tile_mapping.cpp:36` 的期望式与 `healpix_core.cpp:288-293` 的实现体并排 → **同构**。结论「该门无外部鉴别力」独立成立。
3. **重算 coverage 校验的失败路径**：沿 `:557 → :570 → :573 → :660 → :736-738` 走一遍，再与 `:1153-1161` 对照 → **缺 `else` 成立**。
4. **重算全部合同文件行数声明**：对 11 个被引文件逐个 `wc -l` → **10 个不符，1 个（p3_session.cpp=441）相符**。
5. **重算「测试是否真被执行」**：对 5 个测试逐个在 `CMakeLists.txt` / `eng/tests/unit/CMakeLists.txt` / `lib/algorithms/shared/Makefile` 中检索 → **仅 `test_healpix_neighbors.cpp` 进 ctest**（`eng/tests/unit/CMakeLists.txt:738-740`），其余 4 个不在任何自动化入口。

6. **重算 HEALPix 核心几何**（不采信任何既有结论）：手推 `xyz_to_hp` 的极冠/赤道分支与 `hp_to_xyz` 的极区分支，与真实上游 `astrometry.net util/healpix.c` 的 `xyztohp` / `hp_to_xyz` 逐式比对 ⇒ 代数等价（仓式 `coz/sqrt(1+zz)` 恒等于 `sqrt(1-zz)`，近极点条件数更优；仓式 `(1-vv)(1+vv)/sqrt(1+z)*vv` 等价于上游 `z=1-w^2/3`、`rad=sqrt(1-z^2)`）。**核心几何未被攻破** —— 本片的 HEALPix 问题全部在**判据与错误面**，不在数值核心。
7. **重算 SHA-256**：我在不看任何既有结论的前提下逐常量、逐旋转、逐填充分支、逐长度编码端序手推 FIPS 180-4 ⇒ 全部相符。
**盲复算判定：一致。**

**对既有判定的偏向判定：偏松。** 具体地——

- 仓内既有文本对 `p3_output_verify` 的 WCS/HDU 面描述（「缺 HDU 或占位 HDU 均判失败」）比代码**能兑现的更严**（**对代码偏严、对文档偏松**：文档承诺了代码没做的事，且没人发现）。
- 仓内既有文本对 HEALPix 外部 oracle 的描述（「1,000,000 点 mismatch=0」）比活树**能执行的更严**（**对判据偏松**：无可复跑支撑物即等于无判据）。
- 仓内对 `sha256.cpp` / `dirent_win.h` 两处「零消费者/零 include」退役声明经复算**为真**，未被夸大——这部分既有判定是**可信的**。
- 综合：既有结论在本片上**系统性偏松**，且松的方向恰好是「把不可执行的判据当成已执行的」。

---

## 7. 子代理派发记录

共派发 **5 个**对抗核验子代理（同一片、互不重叠的关注面；只读、禁 git 写、禁编译/运行、禁读 `/tmp/acsd_g08/`，结论必须自带 `文件:行`）：

| # | 子代理 | 关注面 | 复核结论 |
|---|---|---|---|
| S1 | healpix_core 几何对抗 | `healpix_core.{h,cpp}` + 三个 healpix 测试 + THIRD_PARTY_NOTICE；逐常量手推、极区/面界/奇点、oracle 独立性、恒红/恒真/自愈锚、筛掉真信号 | 见 §7.2 |
| S2 | p3_output FITS 写出对抗 | `p3_output.{h,cpp}` + CMakeLists + 三份文档；静默降级、错误码、自洽式断言、自愈锚、恒红/恒真、数值稳定性、悬空引用、私建池、硬编码 | 见 §7.2 |
| S3 | 文档/元数据一致性 | 10 份 README/memory/module.yaml + CLASSIFY_V1_PROFILE + shared/Makefile/.gitignore/README；悬空引用表、退役声明复核、自洽式判据、硬编码数值分类、doc↔code 冲突 | 见 §7.2 |
| S4 | 测试与判据有效性 | `test_precision.cpp` + 两个精度头 + 两个 `.py` oracle + 三个 healpix 测试；oracle 分类表、自洽式断言、自愈锚、恒红/恒真门、fuzz 生成器健全性、覆盖缺口 | 见 §7.2 |
| S5 | crypto / 平台头 / 构建 | `sha256.{h,cpp}` + `dirent_win.h` + `Makefile`/`.gitignore`/`README.md`/两个精度头/`fits_output/CMakeLists.txt`；SHA-256 逐常量手算、退役声明复核、私建池、硬编码、构建完整性 | 见 §7.2 |

### 7.1 逐条复核规则

对每个子代理结论，我按三档处理：**采纳**（我能用自己读到的原文或一条可复跑命令独立证实，且行号对得上）／**降级**（现象成立但严重度或归类我改写）／**否决**（我读过相关原文后判定不成立、或重复了我已有结论而不构成独立证据）。

### 7.2 子代理回报与我的裁决

共回传终稿/中间稿 3 个（S4、S5、S1-interim）；S2、S3 在本报告定稿时未回传。裁决如下。

**S4（测试与判据有效性）——已回报，逐条复核**

| S4 结论 | 裁决 | 我的独立复核依据 |
|---|---|---|
| `test_healpix_oracle.cpp:112` 无最小行数门，单行 JSONL 即「PASS」 | **采纳**，并入 **M-16 / B-7①** | 我重读 `:112` 判据确认只含 `lines==0 / bad_parse / mismatch / bad_roundtrip`；`:108-110` 的 `per_face` 只打印不进判据 |
| `lib/infrastructure/aio/tests/sanitize_wsl_v5.sh` 无 `pipefail` 恒真门 | **否决（出片）** | 不在本片 33 成员内，我未亲自读原文，不采信、不背书。**转交 AIO 域审稿人** |
| `STANDARDS_REGISTRY.md:139/140` 用本片测试背书虚假 CONFORMANT | **采纳，升级为新阻断 B-7** | 我自行 `grep -n healpix docs/engineering/STANDARDS_REGISTRY.md` 取得 `:137-141` 原文；并复读 `test_healpix_neighbors.cpp:224-235` 确认 `:228` 确以 order=31 为合法域、`healpix_core.h:60-65` 确无 order 上界 |
| `brute_disc` 与 `query_disc` 共用 `pix2ang_nest`，`:176-177` 独立性声明为假 | **采纳为「相互印证」** | 我在遮住实现时已独立作出同一判断（CE-11），不重复计数 |
| 精度头静默降级 | **采纳，严重度带限定** | 机制我已独立证实（B-4/CE-6）。生产 setter 是两值 if/else ⇒ **潜伏非在线**；我把 B-4 表述收紧为「fail-open 链已完整就位、当前生产路径潜伏」，不因不可达而降级 |
| `PrecisionContext` 结构上无法表达正本 §3.3 的分离精度策略 | **采纳为新须修 M-17** | 我独立复读 `precision_context.h:25-56` 与 `test_precision.cpp:23-30/45-48/139`。策略冲突一侧的正本原文（权威原件内，非本片成员）我**未亲自读**，故该侧标注「待权威原件复核」，不计入独立证据 |
| `gen_spatial_fuzz.py:49` 的 `//29` 静默丢预算 | **采纳为新须修 M-21**，并**纠正我自己的错误** | 我先前把 anchors 数成 **28**（默认丢 4）。实测逐行计数 = **29**，故 10000//29=344、产出 9976、丢 **24**；且 `--adversarial <=28` ⇒ `range(0)` ⇒ 零对抗向量、退出 0。**这是我的算错，已按实测更正** |
| `:69` 移位早于 `:80` 的 order 守卫 ⇒ UB | **采纳为新须修 M-20** | 实测确认 `:69` 无条件执行、`:80` 守卫在其后 11 行且只保护面计数器 |
| `:94-95` NaN 双向漏过 | **采纳为新须修 M-19** | 实测确认两处比较均为 `>`（NaN 比较恒假 ⇒ 既不进最坏统计也不计失败）；`:31-32` 裸 `strtod` 接受 `nan` |
| 极点跳过的理由算术上为假（0.365483° 实测 vs 0.549678° 门槛） | **采纳为新须修 M-18**（升级我原 S-7） | 其数值与 `hp_res = sqrt(pi/3)/128*180/pi` 自洽，该式我可独立算出同值 |
| `test_precision.cpp:66` 恒真门 | **采纳为新须修 M-22** | 我复读 `:66` 与 `precision_context.h:28-31`，确认同址由函数内 static + 引用返回保证 |
| `parent_nest` 零覆盖 | **采纳为新须修 M-27** | 我独立 grep 全仓：仅命中 `healpix_core.h:53`（声明）与 `healpix_core.cpp:314`（定义），无第三处 |
| 3/5 测试无注册执行面 | **采纳**，与我的 **M-16** 同向 | 我自查 5 个测试的构建/ctest 挂载，独立得出同一结论 |
| 负结果（1.2·hp_res 容差合理、18 条 pin 为 astrometry.net 原文、无恒红门） | **采纳为负结果并入 §6** | 与我独立作出的 CE-15、CE-16 同向 |

**S5（crypto / 平台头 / 构建）——已回报，逐条复核**

| S5 结论 | 裁决 | 我的独立复核依据 |
|---|---|---|
| `sha256.cpp:86` 二次 final 返 64 个 `'0'`；`:69` final 后 update 静默丢数据 | **采纳为 B-5（相互印证）** | 我独立作出同一判断（CE-5）并完成自己的 FIPS 180-4 盲复算（CE-15）。其补充「该串能通过仓内全部 64-hex 校验器且与任何真实摘要都不碰撞」使危害具体化 |
| `update(nullptr, len>0)` = UB | **采纳为新须修 M-24** | 我复读 `:68-74` 确认无空指针检查；`len==0` 路径确实不进入循环 |
| 非法枚举的四处静默面（底层 `uint8_t`，256 值中 254 越界） | **采纳为 B-4（相互印证）** | 我复读 `astro_scalar.h:28` 确认底层类型 |
| `shared/Makefile` 整条路径零挂载 | **采纳为新须修 M-29**，并入 **M-16** | 我自查得到 `test_precision.cpp` 不在任何 CMakeLists；其进一步证明 `add_subdirectory(lib/algorithms/shared)` 全仓零命中 |
| `del /f /q ... 2>nul` 在 POSIX 失败**且留下名为 `nul` 的未忽略文件** | **采纳为新须修 M-28**（升级我的跨平台条目） | 我已注意到 `del` 不跨平台，但**未**注意到它会在仓内生成名为 `nul` 的文件；我逐行读过 `.gitignore` 全部 14 行，确认无 `nul` 条目 |
| `lib/common` 悬空（4 处） | **采纳**，并入我的 Makefile 条目 | 我已在 `Makefile:4/:15` 独立发现 |
| `p3_output.cpp:34` 死 include | **采纳（相互印证）** | 我在文件表 #1 已记 `:34` 为孤儿 include |
| `dirent_win.h` 的 `_WIN32` 守卫 | **部分采纳** | 我读到 `:12-13` 文件自认该缺陷。**但我维持「通过」判定**：已按 AGENTS §6 写明保留原因与已知不等价清单，符合退役代码处置规范。其「应删除」的建议记为建议，不升级为缺陷 |
| `size 4/8` 三处重复且无 `static_assert` 绑 `sizeof` | **采纳**，与 **M-23** 合并 | 我复读 `astro_scalar.h:51/59/76/68` 确认三处字面量 |
| `test_precision.cpp:27-28` 字面量对字面量 | **采纳为新须修 M-23** | 我复读 `:27-28` 确认 |
| `PrecisionContext` 默认 FP32 / 单例不唯一 | **采纳为新须修 M-25**（默认 FP32 并入 M-17） | 我复读 `:6/:15/:28-31/:56` |
| `hardware_inspect.cpp:20/:21` 重复 include | **否决（出片）** | 文件在 `lib/infrastructure/benchmark/`，非本片成员，未读 |
| 负结果：`d_name[260]` 的 MAX_PATH 判断 | **采纳并修正我自己的表述** | MAX_PATH=260 含 NUL，故 260 取值正确。我原文「`d_name[260]` 长路径截断（未在已知差异清单里）」**表述不准**：1024 路径缓冲截断文件在 `:8-9` 已自认，未自认的只是**单路径分量 260 的限制**。已在文件表 #20 更正 |
| §2 SHA-256 全面正确（`>>29` 不存在、填充分支全对、`aio_abi_tests.cpp` 12/12 独立复算） | **采纳为负结果并入 §6** | 与我自己的 CE-15 独立盲复算一致 |

**S1（HEALPix 几何对抗，中间稿）——已回报，逐条复核**

| S1 结论 | 裁决 | 我的独立复核依据 |
|---|---|---|
| **`test_hips_tile_mapping.cpp` 检测不出 x/y 转置** —— `:28` 的 x,y 取自被检实现本身，`:36` 再用这同一对 x,y 构造 `expect`；把 `healpix_core.cpp:38-55` 的偶/奇位角色整体对调，全文件仍全绿（`bad_xy/bad_fi/bad_inv` 全 0），而每个 HiPS tile 已被转置。可观测量：`nested_local_to_fits_index(1,9,512)` 由 261120 变为 261633 | **采纳，并把我的 B-3 从「同构抄写」强化为「对缺陷类别零鉴别力」** | 我独立手算：`local=1`、`shift=9` ⇒ 现实现 `nest_to_xy` 给 `x=1,y=0` ⇒ `(511-1)*512+0 = 261120`；偶/奇位对调后给 `x=0,y=1` ⇒ `(511-0)*512+1 = 261633`。两数与 S1 一致。且 `:36` 的 `expect` 与 `:29` 的 `back`、`:45` 的 `inv` 全部经由同一对被污染的 x/y ⇒ 三项计数不可能非零。**已写入 CE-17** |
| 3 个测试中 2 个从不构建/运行 | **采纳为 M-16（相互印证）** | 我自查 `eng/tests/unit/CMakeLists.txt` 只在 `:738-740` 注册 `r9b_healpix_neighbors_test` |
| `require_valid_nside` 无 order 上界；nside=2^31 下三个函数全坏 | **采纳，升级我的 S-2** | 我独立算出 `12*2^62 ≡ 0 (mod 2^64)` ⇒ `:255`/`:380`/`:457` 全失效；其补充的 `static_cast<int>(2^31)` ⇒ INT_MIN、`:103`/`:388` 的 `ns-1` 为有符号溢出 UB，我复读 `:83/:103/:157/:387/:388` 确认成立。真实上限是 order 30 |
| `:189` 自洽式断言 / `per_face` 只打印不判 / `brute_disc` 与被检共用原语 | **采纳为 B-2 / S-8 / CE-11（相互印证）** | 三条我均已独立判定 |
| 许可冲突 `THIRD_PARTY_NOTICE.md:19/:22` vs `healpix_core.cpp:338-341` | **采纳为 M-15（相互印证）** | 我已独立复读两处原文 |
| **负结果**：手推极区与赤道面数学并与真实 astrometry.net `util/healpix.c` 逐式比对 —— `xyz_to_hp` 与上游 `xyztohp` 代数等价（仓式 `coz/sqrt(1+zz) ≡ sqrt(1-zz)`，且近极点**更准**）；`hp_to_xyz` 与上游 `z=1-w²/3`、`rad=sqrt(1-z²)` 一致，仓式 `(1-vv)(1+vv)/sqrt(1+z)*vv` 条件数更优。**核心几何未被攻破** | **采纳为负结果并入 §6 盲复算** | 与我自己的 CE-15/HEALPix 手算同向 |

**裁决统计（截至定稿）**：派发 5 个（S1/S2/S3/S4/S5），回传 3 个（S1 中间稿、S4、S5 终稿）。

- **采纳为新发现 15 条**：B-7 一条 + M-17…M-29 共 13 条 + B-3 的强化（转写为 CE-17）。
- **采纳为相互印证 11 条**：CE-11 共享原语 oracle、B-2 自洽式断言、B-4 静默跳过、B-5 SHA-256 二次 final、CE-17 转置盲区、SHA-256 死 include、测试未挂载、S-2 order 上界、M-15 许可冲突、18 条 pin 独立性、HEAD 不符。
- **降级 / 加限定 2 条**：B-4 增加「潜伏非在线」可达性限定；B9 由「缺陷」降为「建议删除」。
- **否决 2 条**：`sanitize_wsl_v5.sh` 恒真门、`hardware_inspect.cpp` 重复 include —— 均**出片**，越片结论我不背书。
- **自我更正 1 条**：`gen_spatial_fuzz.py` 的 anchors 数由我先前的 28 更正为实测 29（丢失数 4→24）。

> **S2 / S3 未回传**：若其终稿后续到达，按 §7.1 三档规则处理——必须由我用自己读过的原文或一条可复跑命令独立证实才进 §4；越片结论一律否决并转交。此项作为**未闭合项**如实登记，不以「已派发」充数。

---

## 8. 计数口径

- **成员份数 33**：取自 `片清单-权威版.yaml:3613`，不自行增减。
- **总行数 5130**：对清单 33 个路径逐个 `wc -l` 求和；结果与清单 `:3615` 的 `实际行数` 逐位一致（差 0），说明清单本身在本片上可信。
- **实际读 5130 行**：以 `read` 工具返回的 `totalLines` 为准；33 个文件全部读到 EOF（`read` 均返回 `End of file` 或完整末行），无 offset 跳读、无只读片段。
- **发现计数**：阻断 **7**（B-1…B-7）、须修 **29**（M-1…M-29）、建议 **9**（S-1…S-9）。同根因的多个 `文件:行` 合并为一条（如 B-6 合并 10 份文件的同类失效声明、B-7 合并登记册三处），不按出现次数虚增。
- **子代理计数**：派发 5 个，回传 3 个（S1 中间稿 / S4 / S5 终稿），未回传 2 个（S2、S3，已在 §7.2 登记为未闭合项）。裁决：采纳为新发现 15 条、相互印证 11 条、降级/加限定 2 条、**否决 2 条（均出片）**、自我更正 1 条（anchors 28→29）。
- **「自认未修」与「新发现」的分界**：凡缺陷已写进仓内 `known_defects` / `DISP-*` 表格的，我标为**已登记未修**（仍计入阻断/须修，因为登记不等于修复），并在条目里注明登记位置；未见于任何登记表的（如 B-1…B-5、B-7、B-14、M-12…M-17、M-18…M-29、全部 healpix 静默降级项）标为**新发现**。

---

## 9. 自证段（可复跑命令）

全部命令在 `/workspace/Astro CS Database` 下执行，只读。

```bash
# ── 覆盖率口径：33 成员逐个 wc -l，合计应为 5130 ──────────────────────────
cd "/workspace/Astro CS Database"
for f in \
 lib/algorithms/fits_output/p3_output.cpp \
 lib/algorithms/shared/healpix/healpix_core.cpp \
 lib/algorithms/shared/healpix/tests/test_healpix_neighbors.cpp \
 lib/algorithms/upm/README.md \
 lib/algorithms/fits_output/p3_output.h \
 lib/algorithms/upm/memory.md \
 lib/algorithms/fits_output/memory.md \
 lib/algorithms/upm/module.yaml \
 lib/algorithms/shared/tests/test_precision.cpp \
 lib/algorithms/shared/include/astro_scalar.h \
 lib/algorithms/fits_output/module.yaml \
 lib/algorithms/fits_output/README.md \
 lib/algorithms/sampling/README.md \
 lib/algorithms/rejection/README.md \
 lib/algorithms/shared/crypto/sha256.cpp \
 lib/algorithms/rejection/memory.md \
 lib/algorithms/sampling/memory.md \
 lib/algorithms/shared/healpix/tests/test_healpix_oracle.cpp \
 lib/algorithms/shared/healpix/healpix_core.h \
 lib/algorithms/rejection/CLASSIFY_V1_PROFILE.md \
 lib/algorithms/sampling/module.yaml \
 lib/algorithms/rejection/module.yaml \
 lib/algorithms/shared/healpix/tests/test_hips_tile_mapping.cpp \
 lib/algorithms/shared/healpix/tests/snr_hips_spatial_oracle.py \
 lib/algorithms/shared/dirent_win.h \
 lib/algorithms/shared/include/precision_context.h \
 lib/algorithms/shared/healpix/tests/gen_spatial_fuzz.py \
 lib/algorithms/shared/Makefile \
 lib/algorithms/shared/crypto/sha256.h \
 lib/algorithms/shared/healpix/THIRD_PARTY_NOTICE.md \
 lib/algorithms/fits_output/CMakeLists.txt \
 lib/algorithms/shared/README.md \
 lib/algorithms/shared/.gitignore ; do
  printf "%6d  %s\n" "$(wc -l < "$f")" "$f"
done | sort -rn
# 合计 5130

# ── B-1：coverage 校验块缺 else（对照流式路径有 else）─────────────────────
sed -n '557p;560p;570p;660p;736,738p' lib/algorithms/fits_output/p3_output.cpp
sed -n '1153,1161p'        lib/algorithms/fits_output/p3_output.cpp
sed -n '162p'              lib/algorithms/fits_output/p3_output.h
sed -n '1102p'             lib/algorithms/fits_output/p3_output.cpp

# ── B-2：恒真门（期望式 = 被检函数同一次调用）──────────────────────────────
sed -n '186,191p'          lib/algorithms/shared/healpix/tests/test_healpix_neighbors.cpp
sed -n '447,451p'          lib/algorithms/shared/healpix/healpix_core.cpp

# ── B-3：期望式 = 实现体逐字抄写 + 悬空证据 + 零构建引用 ─────────────────
sed -n '4,5p;36,38p'       lib/algorithms/shared/healpix/tests/test_hips_tile_mapping.cpp
sed -n '288,293p'          lib/algorithms/shared/healpix/healpix_core.cpp
ls -d run/temp/v5_oracle                       # → No such file or directory
grep -rn "test_hips_tile_mapping" --include=CMakeLists.txt --include=Makefile lib eng CMakeLists.txt
                                                # → 空（活树零引用）

# ── B-4：非法 dtype 静默跳过 ───────────────────────────────────────────────
sed -n '103,112p;131,137p' lib/algorithms/shared/include/astro_scalar.h
sed -n '67,69p;75,77p'     lib/algorithms/shared/include/astro_scalar.h
sed -n '38p;56p'           lib/algorithms/shared/include/precision_context.h

# ── B-5：二次 final_hex 返回 64 个 '0'；final 后 update 静默丢数据 ────────
sed -n '68,70p;85,87p'     lib/algorithms/shared/crypto/sha256.cpp

# ── B-6：合同文件自述「实测行数」全部失效 ─────────────────────────────────
wc -l lib/algorithms/fits_output/p3_output.cpp lib/algorithms/fits_output/p3_output.h \
      lib/algorithms/coverage/src/upm.cpp \
      lib/algorithms/coverage/include/astro/phase2/upm.h \
      lib/algorithms/coverage/src/rejection.cpp \
      lib/algorithms/coverage/include/astro/phase2/rejection.h \
      lib/algorithms/coverage/src/sampler.cpp \
      lib/algorithms/coverage/include/astro/phase2/sampler.h \
      lib/algorithms/coverage/tools/stage2.cpp \
      lib/phase2_session/p2_session.cpp eng/tests/unit/p3_output_test.cpp
grep -n "556 行\|95 行\|1565 行\|184 行\|2076 行\|329 行\|1156 行\|136 行\|1762 行\|282 行\|153 行\|116 行" \
  lib/algorithms/{fits_output,upm,rejection,sampling}/*.md lib/algorithms/*/module.yaml

# ── M-2/M-3/M-4/M-5/M-6：sampling 自认未修的 5 条 + 私建线程池自相矛盾 ─────
sed -n '74,81p;121,125p'   lib/algorithms/sampling/README.md
sed -n '29,32p'            lib/algorithms/sampling/module.yaml
sed -n '109,110p'          lib/algorithms/upm/README.md

# ── M-1：determinism 与 known_defect 同文件互斥 ────────────────────────────
sed -n '97p;106p'          lib/algorithms/rejection/module.yaml

# ── M-7/M-8/M-9/M-10/M-11：healpix 静默降级与硬化不对称 ───────────────────
sed -n '255p;315p;321p;328p;376p' lib/algorithms/shared/healpix/healpix_core.cpp
sed -n '274,282p;457p'     lib/algorithms/shared/healpix/healpix_core.cpp
sed -n '76p;97p;108p'      lib/algorithms/shared/healpix/healpix_core.h

# ── M-12/M-13/M-14：p3_output 契约缺口 ───────────────────────────────────
sed -n '259p;545p;578p;650p;740p'   lib/algorithms/fits_output/p3_output.cpp
sed -n '1065p'                     lib/algorithms/fits_output/p3_output.cpp
sed -n '498,503p;520,528p'         lib/algorithms/fits_output/p3_output.cpp

# ── M-15：许可文件与代码自述冲突 ───────────────────────────────────────────
sed -n '19p;22p'           lib/algorithms/shared/healpix/THIRD_PARTY_NOTICE.md
sed -n '338,341p;345,346p' lib/algorithms/shared/healpix/healpix_core.cpp

# ── M-16：测试是否真进自动化入口 ───────────────────────────────────────────
grep -n "r9b_healpix_neighbors" eng/tests/unit/CMakeLists.txt      # → 有 add_test
grep -rn "test_healpix_oracle\|test_hips_tile_mapping\|test_precision\|gen_spatial_fuzz\|snr_hips_spatial_oracle" \
  --include=CMakeLists.txt --include=Makefile lib eng CMakeLists.txt | grep -v '^run/'
                                                # → 仅 Makefile 的 test_healpix_oracle/test_precision 两行

# ── S-1：NaN 输入使 query_disc 返回全天球（手推路径）───────────────────────
sed -n '446p;486,493p'     lib/algorithms/shared/healpix/healpix_core.cpp

# ── 悬空引用清单（README 自己声明的证据路径）───────────────────────────────
for p in run/temp/v5_oracle run/v6/IMPL-P2-UPM-001 run/v6/IMPL-P2-REJ-001 \
         run/v6/p2-samp run/local/bughunt/ledger.md docs/detail/phase3_fits.md \
         docs/detail/phase2_samp.md lib/common; do
  [ -e "$p" ] && echo "EXISTS  $p" || echo "MISSING $p"
done

# ── 退役声明复核（CE-13 / CE-14，两条均成立）────────────────────────────────
grep -rn "crypto::sha256_file" lib eng --include=*.cpp --include=*.h   # → 零活调用点
grep -rn "dirent_win.h" lib eng docs --include=*.cpp --include=*.h       # → 零 include 点
```

---

**审稿人立场声明**：本片判定「阻断」。以上每一条均给出 `文件:行` 或可复跑命令；无一条基于既有报告的结论转述。子代理产出只作线索，凡我未能用自己读过的原文或命令独立证实的，已在 §7.2 记录否决。
