# 审稿-P1-INF-aio-004 — G08-05 对抗审稿 第 1 遍

- 片号：`INF-aio-004`
- 层：`lib/infrastructure/aio`
- 基线：仓库 `/workspace/Astro CS Database`
- ⚠️ **HEAD 实测 = `bf25c085`，不是单据所述的 `850a9ede`。** 单据基线与实际工作树不一致，本片所有结论基于 `bf25c085`。
- 负责人裁定口径已采纳：本片结论**不采信任何「检查通过」**，全部由重读原文 + 构造反例独立得出。

---

## 1. 读完了吗（口径说明）

**口径**：只计「用 `read` 工具从第 1 行读到文件末行、每行都过眼」。子代理读过的不计入我的覆盖率（另列于 §7）。

| 项 | 数 |
|---|---|
| 清单成员份数 | **28** |
| 清单声明实际行数 | 10,631（`片清单-权威版.yaml:3022`，实测 `wc -l` 逐份一致） |
| 我**亲自读完**份数 | **21** |
| 我**亲自读完**行数 | **6,671** |
| 行覆盖率 | **6,671 / 10,631 = 62.7%** |
| 份数覆盖率 | 21 / 28 = 75.0% |

### ⚠️ 未读完的 7 份（如实列出，共 3,960 行）

| 文件 | 行数 |
|---|---|
| `lib/infrastructure/aio/tests/test_transform.cpp` | 999 |
| `lib/infrastructure/aio/src/hips/aio_hips_reader.cpp` | 695 |
| `lib/infrastructure/aio/src/ahpx/aio_ahpx_reader.cpp` | 635 |
| `lib/infrastructure/aio/src/aio_upm.cpp` | 586 |
| `lib/infrastructure/aio/healpix_db/archive/legacy/healpix_stack/tests/test_gradient_synthetic.py` | 495 |
| `lib/infrastructure/aio/tests/p1hips/p1hips_fixtures.hpp` | 298 |
| `lib/infrastructure/aio/tests/test_wph_cli_browser.cpp` | 252 |
| **合计** | **3,960** |

**这 7 份的原因与诚实声明**：上下文预算在本轮被 6 个子代理的长报告占满，我优先把「自己读完并独立推导」的资源集中到阻断面最大的文件上。上述 7 份**有子代理读过并给了结论（见 §7）**，但那些结论**不是我亲自复核过的**，本报告中凡引用之处一律标注 `[子代理结论·我未复核]`，不计入我的判定。

**其中两处我需要点名**，因为它们正是单据点名的重点而我未能亲读：
- `src/ahpx/aio_ahpx_reader.cpp` —— `aio_build_config.full.json:2` 自称「含已废弃 ahpx」且 `:5 enable_ahpx: true`，「退役对象仍有活调用者」这条线索正落在这里，**我未能亲自裁决**。
- `archive/legacy/healpix_stack/tests/test_gradient_synthetic.py` —— 归档目录测试文件，退役声明真伪未亲自裁决。

---

## 2. 本片判定：**阻断（BLOCKING）**

三条最重：

1. **`aio_fits.cpp` 写路径三重不一致 ⇒ 产出结构非法的 FITS，且常规往返即可触发。**
   `aio_fits.cpp:1158` 头卡写 `BITPIX=16`、`:1192` 实写 float32（4 B/px）、`:1211` 按 2 B/px 算补零。三式在 `float_sample==0 && bits_per_sample==32` 时互相矛盾——而这正是 `:963-964` 读 BITPIX=32 文件后必然产生的状态。**文件:行 `aio_fits.cpp:1158 / :1192 / :1211`，被检量与期望量分叉。**

2. **HISS occupancy 展开静默截断 + 默认无校验 ⇒ 整块 tile 以零值＋support 置位返回成功。**
   `hiss_reader.cpp:1039-1046`（及 `:1168`/`:1262`/`:1340` 四份拷贝）`if (compact_idx < signal_compact.size())` 之后仍 `compact_idx++`，越界部分静默留 `0.0f` 并 `return 0`；`:328` 的 checksum 在 `NONE` 时整段跳过。叠加 `:1048-1050` 打印的 `expanded=%zu` 恒等于 `n_leaf_per_tile`（`assign` 已定死），**这条看起来像验证的日志结构上不可能暴露该缺陷**。

3. **归档 `healpix_stack` 的「退役声明」查出来了，但结论与单据预设相反：调用者确实为零，**错的是**状态文档与检索可见性。** `lib/infrastructure/aio/healpix_db/README.md:18` 把这个只存在于 `archive/legacy/`、不在任何构建图里的模块标成「**活跃（独立仓库）**」；而根 `.gitignore:37` 的 `archive/` 规则使整棵归档树对默认 ripgrep/grep **不可见** ⇒ 任何审「零消费者」的审计都**结构性地**得到假阴性。`[子代理 S4 结论·我未亲读 README/.gitignore，未复核]`

4. **`p1hips_tests_selfcheck.cpp` 的「必败自检」门是反的：子进程起不来它就绿，子进程正常工作它就红。**
   `:67` 硬编码 `execve("/proc/self/exe", …)`、`:68` 失败 `_exit(127)`、`:150`/`:164` 唯一判据是 `child_rc == 0`。fork 失败(127)、execve 失败(127)、被信号杀死(`:72` 返回 -1)、组名拼错(`:84` 返回 127)——**四条路径全部产出非 0，全部被判为「必败验证通过」**。且 `:6` 声称的判别证据「stderr 含 FAULT-INJECT 行」**代码从未检查**。

---

## 3. 逐文件清单（21 份亲读）

| 文件 | 行 | 读了什么 | 看到什么（`文件:行`） | 判定 |
|---|---|---|---|---|
| `src/hiss_reader.cpp` | 1790 | LE 编解码、内嵌 HEALPix NESTED ang2xy、TLV 头、tile 目录、校验和/codec/inverse-transform、occupancy 展开、radec 查询 | `:1039-1046` 静默截断；`:1071` 越界索引静默丢弃；`:1048-1050` 诊断恒真；`:1511-1516` 等 8 处 `return 0` 把失败变成零值；`:995-1008` `ratio*ratio` uint32 回绕且无 `HISS_MAX_N_LEAF`；`:388-419` `expected` 算了却从不校验 `restored.size()`；`:341`/`:368` 错误码 −5/−4 互换 | **阻断** |
| `src/aio_fits.cpp` | 1236 | 手写 FITS 解析 + CFITSIO fpack + 写路径 + detect | `:1158/1192/1211` 三重不一致；`:887-889` 短读仅 WARN 后继续；`:1187-1215` 全部 `fwrite` 返回值丢弃；`:1203` OpenMP 私建并行区；`:435-439` `catch(...)`+放行 NaN；`:787-793` cfitsio 路径 BSCALE/BZERO 算了从不读；`:661`/`:692` `c=1` 在 `if(gray)` 之外 | **阻断** |
| `runtime/artifact_store/provenance.py` | 769 | DATA-004 digest/revision/supersede/privacy | `:462-463` 脏输入静默放行；`:475` 版本裸串比较（模块自带 `parse_version` 却不用）；`:753-754` 字面 `pass` 的空检查；`:386-393` 可选字段 `None` vs `[]` 使「同输入同 digest」不成立；`:497`/`:499` 把任意 `/tmp`、任意 URL 判泄露 | **阻断** |
| `product_io/src/provenance.cpp` | 362 | FROZEN 最小集 + bunit/k_corr/降级门 | `:266` 通量守恒门被 `has_pixfrac` 前置条件关掉（而 `:264` 注释自称「与 pixfrac 无关」）；`:335-336` 缺失分位数默认 0.0 后 `0≤0≤0` 通过；`:176`/`:182` `signal_unit` 空则整条二次律跳过；`:83` 生产者伪造 `kernel_id="unspecified"` 再由门校验；`:340-343` 「双门」只查存在不取值 | **阻断** |
| `src/aio_api.cpp` | 441 | C ABI 异常屏障与访问器 | `:50-52` `aio_internal_is_fp64()` 读进程全局态决定像素 ABI；`:243-303` 每个 getter 的 `catch(...)` 恒不可能触发（无抛点）＝装饰性异常屏障；`:405-416` `free` 顺序未校验双缓冲互斥 | 须修 |
| `healpix_db/archive/legacy/healpix_stack/gradient/corrected_stacker.cpp` | 384 | 梯度校正 + SNR² 加权 sigma-clip | **`:196`/`:301`/`:350` `frames[f].snr[k]` 无长度校验 ⇒ 越界读**（`:66` 只校 ipix==pixel）；`:132-133` NaN/Inf 静默改 0.0（模型崩溃被当成「梯度为零」）；`:98`/`:142` 模型无效静默不校正且不置标志；`:127` `10×`/`+10 ADU` 硬编码无出处；`:112` `n_ctrl>=5` 否则整段保护静默跳过；`:373-378` 全被剔除 ⇒ mean 记 0.0 与真零通量不可分；`:326` `E[X²]−E[X]²` 灾难性抵消，负值被 `:327` 吞成 0 ⇒ 精度损失变「无离群」；`:225-227` 参数非法静默替换成 0.05/0.95 | **阻断** |
| `io/include/acsd/io/fits_stream_v1.h` | 227 | 对外 FITS 流式 C ABI v1 | **`:13` 声称错误码与 `acsd_status` 一致，实测 8/9 冲突**（`common_abi_v1.h:64-65` BUDGET=8/SELFTEST=9 vs 本头 `:58-59` TRUNCATED=8/BAD_HEADER=9）；`:100` `MAX_CARDS=1024` 内联 ≈157 KB 结构由调用方分配；`:144` chunk 读「返回实际读得元素数」与同族返回状态码冲突 ⇒ 截断读返回正数；`:204` 引用不存在的 `strict` 参数；`:6` 宣称「v1 不可变」却与实现漂移 | **阻断** |
| `product_io/include/astro/aio/fits.h` | 161 | 流式写出 + DATASUM/CHECKSUM + 重开验证声明 | `:78` `fits_data_size` 无溢出契约；`:150` 「expected 为空 ⇒ 只做自洽验证」＝自证式降级；`:61` 声明一个恒真的校验原语（见 §7 子代理） | 须修 |
| `product_io/src/bunit.cpp` | 269 | 冻结单位表 + 量纲代数 | `:163-177` 期望量与被检量**共用同一个 `parse_unit_exponents`** ⇒ 解析器同错则两侧同错，门无法失败；`:64-67` `px`≡`sr` 硬等价（像素立体角≠1 sr）无推导；`:242-255` vs `:159` 同文件对「未知量」一个静默一个报错 | 须修 |
| `product_io/src/product_io.cpp` | 93 | 顶层发布装配 | `:9-29` `expected_from_layers` 用**产出字节的同一批 `layers`** 造验证期望量；`:31-35` 不校验 `expected` 非空/与 layers 等长 ⇒ 传 `{}` 静默降级为只看结构；`:50-53` 空 data + NAXIS>0 静默跳过 ⇒ 全零 HDU 带合法 DATASUM 发布；`:82-88` 哈希字段只查 `.empty()`，从不绑定真实 sha256 | **阻断** |
| `src/aio_disk_full.h` | 117 | ENOSPC/EDQUOT 失败瞬间分类 | `:97` `statvfs` 失败静默 `return false`；`:95-100` `_WIN32` 下整条磁盘满判定不可达（Windows 恒 exit 7）；`:18-19` 头注声称「非满盘 I/O 失败不算磁盘满」，代码是无条件 OR，真满盘时 EACCES/EIO 也会被标成 disk_full；且 `aio_fits.cpp` 完全不引用本头 | 须修 |
| `tests/p1hips/p1hips_tests_selfcheck.cpp` | 206 | 注入自检 | `:67` 硬编码 `/proc/self/exe`；`:50-52` 子进程 env 被整体替换（无 TMPDIR/PATH）；`:150`/`:164` 唯一判据 `rc==0`；`:72` 信号死返回 -1 亦算通过；`:6` 声称的 stderr 判别从未实现 | **阻断** |
| `tests/p1hips/p1hips_tests_perf.cpp` | 157 | 性能哨兵 | `:48` `if (!ps) return;` 吞掉全部失败；`:53-61` 六个写/收尾返回值全弃；`:146-148` 三条判据在「均匀变慢」下比值不变 ⇒ 对绝对性能退化**结构上不可失败**；`:19-20` 头注自认「不设绝对阈值」；`:147` `parity4<4.0` 对完全线性无余量（1 核必红），而 `:13-14` 记录了它曾红（0.43/0.24）并以改测量协议消音 | **阻断** |
| `tests/hips_sanitize_driver.cpp` | 83 | ASan/UBSan 驱动 | `:67-79` 注释「读回验证」但**填了 buf 后一个数都不比**；`:62` `aio_hips_write_snr_points` 返回值全文件唯一被丢弃者；`:22` 用 `PRODUCT_ALL` 而非 `ALL_V19` ⇒ variance/ivar 除零路径零覆盖 | **阻断** |
| `tests/p2hips/CMakeLists.txt` | 51 | P2HIPS 注册 | `:22-23` 「基线(无注入)必 FAIL」注册在任何地方都没有；`:28-32` 列 4 种注入，只接了 `missing_key`，另三种是散文。**正面**：`:48-51` 注入下必 PASS + 基线必 FAIL 的双向结构是对的做法 | 须修 |
| `healpix_db/archive/legacy/healpix_stack/Makefile` | 53 | 遗留独立构建 | `:1-3` 头注明令「本文件不得自带 ISA/线程旗标」，`:19` 同一文件立刻硬写 `-O3 -ffast-math -fopenmp` —— **注释与本文件自身矛盾**；`:23` `AIO_DIR=../../astro_image_io` 实测**不存在**；`:28` SRCS 7 个中 `gradient/corrected_stacker.cpp` 不在其列；`:53` `clean` 用 Windows `del`，Linux 上 `make clean` 失败；`-ffast-math` 对科学模块直接破坏 NaN/Inf 语义 | **阻断** |
| `healpix_db/archive/legacy/healpix_stack/ahps_reader.h` | 84 | .ahps 读取器声明 | `:4` `#include "ahps_format.h"` 实测存在（我已核）；但 `AhpsReader` 全仓**无任何 `#include` 本头的活代码**（子代理已 grep），且实现 `ahps_reader.cpp` 不在本片。退役声明真伪**我未亲判** | 建议 |
| `product_io/src/sha256.cpp` | 123 | FIPS 180-4 SHA-256 | 实现正确：`digest()` 拷贝状态、padding 前先存 bitlen（`:80-88`）、可重复调用。**本文件内无比较器** ⇒ 不存在自比。唯一缺口：无比较原语（全仓比对都是 `==`） | 通过 |
| `product_io/include/astro/aio/sha256.h` | 43 | 声明 | 与实现一致，无缺陷 | 通过 |
| `tests/p1hips/p1hips_tests_selfcheck`（已并入上） | — | — | — | — |
| `aio_build_config.full.json` | 11 | 编译开关 | `:2` 自称「所有模块 ON」却 `:5 enable_ahpx:true` 启用其自身注释里的「已废弃 ahpx」；本片无人读这两个 json（子代理：仅 build.ps1 读，CMake 从不读 ⇒ 无法影响任何检查） | 须修 |
| `aio_build_config.minimal.json` | 11 | 编译开关 | 与 full 同构，无独立缺陷 | 建议 |
| `product_io/src/provenance.cpp`（已列） | — | — | — | — |

> 说明：`hiss_reader.cpp` / `aio_fits.cpp` / `provenance.py` / `provenance.cpp` / `corrected_stacker.cpp` / `fits_stream_v1.h` / `product_io.cpp` / `p1hips_tests_selfcheck.cpp` / `p1hips_tests_perf.cpp` / `hips_sanitize_driver.cpp` / `Makefile` 共 11 份由两个子代理**独立复读**，与我结论重合处见 §7。

---

## 4. 发现清单

### 阻断（BLOCKER）— 14 条

| # | 位置 | 类别 | 缺陷 |
|---|---|---|---|
| A1 | `aio_fits.cpp:1158/1192/1211` | 静默降级＋硬编码 | 三式 bytes-per-pixel 不一致 ⇒ BITPIX=16 卡 + float32 数据 + 2B 补零，产出非法 FITS |
| A2 | `hiss_reader.cpp:1039-1046,1168,1262,1340` + `:328` | 静默降级 | occupancy/compact 长度不符静默截断；checksum 默认 NONE 无兜底；`return 0` |
| A3 | `p1hips_tests_selfcheck.cpp:67,72,150,164` | 恒红门反转 | 「必败自检」在子进程起不来时绿、正常时红 |
| A4 | `provenance.cpp:266` | 恒真门 | 通量守恒门被无关前置条件关闭；注释 `:264` 明说「与 pixfrac 无关」，代码相反 |
| A5 | `aio_fits.cpp:887-889` | 静默降级 | 短读仅 WARN，零填充尾部当合法天空，`return 0` |
| A6 | `corrected_stacker.cpp:196,301,350` | 数值稳定性 | `snr[k]` 无长度校验 ⇒ 越界读（`:66` 只校 ipix==pixel） |
| A7 | `corrected_stacker.cpp:132-133` | 静默降级 | NaN/Inf → 0.0，模型崩溃被表述为「梯度为零」 |
| A8 | `fits_stream_v1.h:13,58-59` vs `common_abi_v1.h:64-65` | 悬空/契约 | 冻结 ABI 错误码 8/9 冲突，「文件截断」被派发成「预算超限」 |
| A9 | `Makefile:1-3` vs `:19` | 注释与代码矛盾 | 头注禁用自带 ISA/线程旗标，同文件硬写 `-ffast-math -fopenmp` |
| A10 | `aio_fits.cpp:1203` | 私建线程池 | I/O 模块内 OpenMP 并行区（`lib/infrastructure/aio/Makefile:10` `-fopenmp` 为其存在） |
| A11 | `p1hips_tests_perf.cpp:48,53-61,146-148` | 恒真门 | 失败被吞 ⇒ 三条哨兵在 writer 100% 失效时仍全绿 |
| A12 | `hips_sanitize_driver.cpp:67-79` | 恒真门 | 注释「读回验证」实为不比对任何值 |
| A13 | `product_io.cpp:9-29` | 自洽式断言 | 验证期望量由产出字节的同一批 `layers` 派生 |
| A14 | `provenance.cpp:83` | 自愈锚 | 生产者伪造 `kernel_id="unspecified"`，门再校验它 |
| A15 | `healpix_db/README.md:18` `[未复核]` | 退役/悬空 | 把只存在于 `archive/legacy/`、不在任何构建图的模块标成「活跃（独立仓库）」；与 `HIPS_WRITER.md:397`「全仓零调用 已死代码化」**对同一对象给出相反结论** |
| A16 | 根 `.gitignore:37` `archive/` `[未复核]` | 悬空/审计可证伪性 | 规则使整棵归档树对默认 ripgrep/grep 不可见，其 `:36` 注释却自称「归档文件须受 Git 跟踪」⇒ **「零消费者」类审计结论在此结构性必然假阴性** |
| A17 | `corrected_stacker.cpp:122,132` vs `Makefile:19` | 静默降级 | 代码用 `std::isfinite` 防 NaN，同目录构建旗标 `-ffast-math`（隐含 `-ffinite-math-only`）可把它折成恒真 ⇒ 唯一防线在定义它的构建路径里失效 |
| A18 | `corrected_stacker.cpp:249-278` vs `:375` | 科学方法不一致 | winsorized 分支用**未加权**样本算剔除阈值，最终 mean 却用 **SNR² 加权** ⇒ 判据与被报告值是两个估计量（非 winsorized 分支 `:323-329` 才是对的） |
| A19 | `test_gradient_synthetic.py:42,48-49` `[未复核]` | 恒红门 | 依赖模块 `healpix_stack.py` 全仓不存在 ⇒ import 期即失败；即便修好，`GAIA_DATA_DIR` 解析到不存在的路径 ⇒ 三测试恒 SKIP、`:491` 退出码恒 1。**SKIP 与 FAIL 不可分** |
| A20 | `ahps_reader.h:42,46` `[未复核]` | 静默降级 | 返回裸 `vector`，无错误通道 ⇒ 「本 tile 零像素」与「chunk 解压失败」对调用方完全不可分 |

### 须修（MUST-FIX）— 21 条（摘要）

`hiss_reader.cpp`：`:995-1008` `ratio*ratio` 回绕且无 `HISS_MAX_N_LEAF`；`:388-419` `expected` 算了从不校验 `restored.size()`；`:341`/`:368` 错误码 −5/−4 互换；`:598-602` ordering/radesys 解析后从不校验却按 NESTED+ICRS 硬算；`:1490-1516` 未 open 即查询返回 0/成功；`:1048/1080` 裸 `fprintf` 绕过 `HISS_VERBOSE`（与 `:55-62` 自述的 40s 修复自相矛盾）；`:1593-1603` SPARSE_LIST 二分假设升序但 `hiss_format.h:90` 未要求。
`aio_fits.cpp`：`:1187-1215` fwrite 返回值全弃（**且本片 `aio_disk_full.h` 就在同目录却从未被引用 ⇒ FITS 导出永不产生 `error_kind=disk_full`，恒 exit 7**）；`:435-439` `catch(...)` 放行 NaN（文件自己在 `:175-179` 写明了这条禁令，只应用到 BSCALE/BZERO）；`:787-793` cfitsio 路径 BSCALE 算了从不读；`:661`/`:692` `c=1` 在 `if(gray)` 之外（手写路径 `:920` 在里面 ⇒ 两路径分叉）；`:473` RADESYS 硬写 "ICRS" 忽略 EQUINOX。
`provenance.py`：`:462-463` 脏输入放行；`:475` 裸串比较；`:753-754` 空 `pass`；`:497`/`:499` 过度宽泛的泄露正则。
`provenance.cpp`：`:335-336` 缺分位数默认 0.0 通过；`:176` `signal_unit` 空则整门跳过；`:340-343` 双门只查存在。
`corrected_stacker.cpp`：`:373-378` 全剔除⇒mean=0.0；`:326` 灾难性抵消；`:127` 硬编码 10×/+10 ADU；`:98/142` 模型无效静默不校正；`:225-227` 参数静默替换；`:max_iter` 耗尽不报。
`fits_stream_v1.h`：`:100` ≈157 KB 内联结构；`:144` 截断读返回正数。
其余：`aio_disk_full.h:97`、`:95-100` Windows 不可达；`p2hips/CMakeLists.txt:22-32`。

### 建议（SUGGESTION）— 8 条（摘要）

`sha256.cpp` 无 `reset()`；`bunit.cpp:53` `±64` 无出处；`provenance.cpp:271/286` `1e-12` 无标定来源；`ahps_reader.h` 退役声明待裁；`full.json:2`「所有模块 ON」与 `:5` 自相矛盾；`hiss_reader.cpp:1528` 陈旧注释「tile_nside² × 12」；`product_io.cpp` 与 `p1hips/CMakeLists.txt` 的孤儿/未注册问题 `[子代理结论·我未复核]`。

---

## 5. 我主动构造的反例（全部由我读码推导，未运行任何二进制）

| # | 构造 | 期望推翻什么 | 是否推翻 |
|---|---|---|---|
| **X1** | 读一张 BITPIX=32 的 FITS，再交给 `fits_write_file` 原样写回 | 往返应无损 | **推翻**。`:963-964` 产出 `bits=32, float_sample=0`；`:1158` 写 BITPIX=16、`:1192` 写 float32、`:1211` 按 2B 算补零 ⇒ 头与载荷互相矛盾。（子代理独立算出总数据段 3080 B 非 2880 倍数，与我一致） |
| **X2** | occupancy 位图置位数 > signal 数组长度，checksum=NONE | 应报长度不符 | **推翻**。`:1039` 越界后仍 `compact_idx++`，像素留 `0.0f`、support 留 0，`:1090 return 0`。下游 SNR 会拿到「有效但零通量」 |
| **X3** | `:1048` 打印的 `expanded=%zu` 拿来当展开完整性证据 | 应当能暴露 X2 | **推翻**。`:1028` 的 `assign(n_leaf_per_tile,…)` 已把 `signal.size()` 定死，`:1050` 打印的必然是满 tile 数 ⇒ **这条日志结构上不可能报出 X2**，反而给出安心信号 |
| **X4** | 把 `qret` 设成任意非 0 整数喂给 `test_wph_cli_browser.cpp:193` 式的 `qret==0 \|\| qret<0` | 该断言应有判别力 | `[子代理结论·我未复核]` 报为恒真。我**未亲读该文件**，不背书 |
| **X5** | `history.replaced` 记 `"v1"`，新发布写 `revision.product="1"` | 旧版本应被拒 | **推翻**。`:475` 是裸串 `"v1"=="1"` → False ⇒ 放行；而模块自己的 `parse_version` 会把两者都解析成 `(1,)`，`:273-275` 自己就知道它们同版本 |
| **X6** | provenance 不声明 `sampling.pixfrac`，`flux_conservation_factor=0.87` | 守恒门应红 | **推翻**。`:257-266` 的 `has_pixfrac` 为 false ⇒ 整块跳过。且 `:264` 注释自称「与 pixfrac 无关」 |
| **X7** | degradation 记录不含 p05/p50/p95 | 分位数门应红 | **推翻**。`:335-336` 三个 `.value(k,0.0)` 全取 0.0 ⇒ `0≤0&&0≤0` 通过 |
| **X8** | 让 `make_tmp_dir` 彻底失败（tmpfs 满），跑 perf 哨兵 | 性能门应红 | `[子代理结论·我未复核]` 指向 `p1hips_tests_perf.cpp:48` 的 `if(!ps) return;` 与 `p1hips_oracle.hpp:382` 的空串返回，判定三哨兵全绿。**我未亲读 oracle.hpp，不背书**；但 `:48` 与 `:53-61` 我亲读确认确实吞掉全部返回值 |
| **X9** | `frames[f].snr` 非空但比 `ipix` 短 | 应被长度校验拦下 | **推翻**。`:66` 只校 `ipix.size()==pixel.size()`；`:184` 只判 `!snr.empty()`；`:196` 直接 `snr[k]` ⇒ 越界读 |
| **X10** | 让某 ipix 的全部帧被 sigma-clip 剔除 | 应有「无数据」态 | **推翻**。`:373-378` 把 mean 记 `0.0`，与真实零通量像素不可区分 |
| **X11** | 让 `E[X²]−E[X]²` 因抵消算出负值 | 应报数值失效 | **推翻**。`:327 if(var<0) var=0` 吞掉，`:342 std_arr<=0` 于是**跳过该 ipix 的离群扫描** ⇒ 精度损失被转成「无离群」 |
| **X12** | 在 macOS/无 `/proc` 环境跑 selfcheck | 应报「无法执行注入」 | **推翻**。`execve` ENOENT ⇒ `:68 _exit(127)` ⇒ `:150` 判「必败验证通过」⇒ 打印 `P1HIPS SELFCHECK PASS`。**门恰在什么都没验证时绿** |
| **X13** | 把 `HISS_ERR_*` 当冻结码做数值派发 | 应语义正确 | **推翻**。`common_abi_v1.h:64-65` BUDGET=8/SELFTEST=9 对本头 TRUNCATED=8/BAD_HEADER=9，我已亲自比对两份原文 |

---

## 6. 盲复算（遮住既有判定独立取证）

口径：先不看 `run/GOVERN-08/审核包-R2/审稿-R*.md`、`审稿-R2/R3/P1-*.md` 的任何结论，独立从原文重算本片判定，再回头比。

| 项 | 我的独立结论 | 与既有判定比 |
|---|---|---|
| 片级判定 | **阻断** | 一致 |
| `aio_fits.cpp` 写路径 | 阻断 | 一致（子代理独立同判） |
| `hiss_reader.cpp` 截断 | 阻断 | 一致 |
| selfcheck 门反转 | 阻断 | 一致（我与子代理**分别独立**得出，且我补出了子代理未列的 env 替换与 `:6` 声称未实现两条） |
| `bunit.cpp` px≡sr | 须修（硬编码等价） | **偏松**：子代理判为 BLOCKER。我读到 `:37-38` 的注释**明确把 px 写成有意保留的迁移别名**，属「已记录的有意决策」，我下调一档，但要求补出推导或出处 |
| 磁盘满分类缺失 | 须修 | **偏严**：我初判以为 FITS 写路径完全忽略 `aio_disk_full.h` 即可判阻断；子代理核实 `atomic_publish.cpp` 的发布链确有 fsync 与错误传播，我把该项下调为须修，仅保留「FITS 导出路径不产 disk_full」这一条 |
| `p2hips/CMakeLists` 的 `pass` | —— | 我**否决**了把它算作缺陷的直觉写法：该 CMake 的双向注入结构本身是对的，问题只在「基线必 FAIL」未注册 |

**盲复算结论：判一致。** 我唯一自查偏严处（磁盘满）已下调；唯一自查偏松处（px≡sr）已下调子代理的定级。

---

## 7. 子代理派发记录

派发 **6 个**（超单据要求的 3-5），互不重叠分片，互斥焦点：

| # | 分片 | 文件 | 状态 |
|---|---|---|---|
| S1 | A 组 | `hiss_reader.cpp` + `aio_fits.cpp` | 已交，42 findings |
| S2 | B 组 | `aio_hips_reader.cpp` + `aio_ahpx_reader.cpp` + `aio_upm.cpp` + `aio_api.cpp` | 已交，3 BLOCKER + 17 MUST-FIX |
| S3 | C 组 | product_io 八件套 | 已交，11 BLOCKER + 19 MUST-FIX |
| S4 | D 组 | 归档 `healpix_stack` 四件套 | 已交，28 findings（4 BLOCKER） |
| S5 | A 组复读 | 同 S1（独立第二读） | 已交，8 BLOCKER |
| S6 | E 组 | tests + configs + `fits_stream_v1.h` | 已交，7 BLOCKER |

> S5 是我在派发时误将 A 组重复派发的一次。**它反而成为本轮最有价值的一次派发**：S5 在我亲读同一对文件之外，独立报出 `aio_fits.cpp:661/692` 的 `c=1` 花括号嵌套问题。**我第一遍读错了**，以为 `c=1` 在 `if(gray)` 内；S1（读同一文件的另一路）也未发现。**我随后用 `read` 精确取回 645-664 与 678-695 行复核，确认 S5 正确、我错**，据此改判。（这也是我唯一一处被子代理纠正后回改的结论。）

### 我逐条复核后**采纳**的（子代理提出、我亲自验证成立）

- S5-B3 `aio_fits.cpp:661/692` `c=1` 在 `if(gray)` 之外，手写路径 `:920` 在里面 —— **已用精确行段复核，成立**（我原读错误，已改判）。
- S3-B2 `provenance.cpp:266` 通量守恒门在生产路径上是死门（生产 `has_pixfrac=false`）—— 我本就独立发现该门被无关前置条件关闭，S3 补出「生产侧恒 false」，成立。
- S3-F7 `bunit.cpp:163-177` 期望量与被检量共用同一解析器 —— 我独立看出同一形状，成立。
- S6-B8 错误码 8/9 与 `common_abi_v1.h` 冲突 —— **我亲自比对两份原文确认，成立**，已升为阻断 A8。
- S1/S5 关于 `aio_fits.cpp:1158/1192/1211` 与 `:887-889` —— 与我独立结论一致，双路互证。

### 我逐条复核后**否决/改判**的

- **否决 S6-B9/S1-B6 对 `test_wph_cli_browser.cpp` 的「13 PASS」签核指控** —— 该文件**不在我亲读范围**，我不以未复核的结论升为阻断；仅登记为待裁。
- **下调 S3-B7 `px≡sr` 由 BLOCKER→须修** —— 理由见 §6（`:37-38` 注释把它写成有意别名）。
- **下调「磁盘满分类」定级** —— 见 §6。
- **否决我自己的一个初判**：我曾怀疑 `publish_fits_product` 缺父目录 fsync；S3 核实 `atomic_publish.cpp:447` 有 `confirm_dir_durability`，**该假设不成立，撤回**。
- **撤回一个我自己算错的升级**：我最初打算把「未校验 BITPIX 可致 `size_t` 回绕 ⇒ 越界读」列为阻断。自己把 `n_pixels` 上界 `65535³ ≈ 2.81e14`、`bytes_per_pixel` 上界 `INT_MAX/8` 与 `2^64` 联合验算后确认：可达的回绕余数几乎都远大于 1 GB 上限，**够不到「小分配 + 大读取」的危险组合**。降为建议级（BITPIX 未按 FITS 合法集校验），**不作为阻断**。
- **改判「退役声明」这条线索的结论方向**：单据提示「已实测存在一例退役声明被证伪」，我据此预设本片归档代码应有活调用者。S4 实测**调用者确实为零**（`AhpsReader`/`CorrectedStacker` 的引用全在归档目录内部，live orchestrator 的 stage 8/9 函数体已在 `orchestrator.cpp:4876-4879` 删除）。**被证伪的不是调用者，而是状态文档与检索可见性**（A15/A16）。我据此把原先记为 UNRESOLVED 的第 1 条改为已裁决，并把它升级为阻断——**方向相反，但仍是新问题**。

### 子代理报出、我**未亲读因而未背书**的高价值项（登记待裁）

- **S2-B2 `aio_ahpx_reader.cpp:255-268`**：`if (decompressed != m_headerSize)` 在 `HeaderSize==0 ∧ HeaderCompSize>0` 时**两侧同为 0**，门必然通过；`parseHeader()` 随后对空串找不到 `blocks` 而 `return true` ⇒ `open()` 在无头文件上返回成功。**这是本轮最标准的「同一表达式既当被检量又当期望量」**，但该文件不在我亲读范围，我按未复核登记。
- **S2-B1 `aio_hips_reader.cpp:294-300`**：MOC order 被**假定**等于 tile order，而真正的 `MOCORDER` 就在文件里（`writer:420` 写过）却从不读；`moc_order < tile_order` 时静默丢弃大部分单元。
- **S6-B4**：`make_tmp_dir` 失败返回空串 + `perf.cpp:48 if(!ps) return;` ⇒ 三条性能哨兵在 writer 100% 失效时仍全绿。**与我亲读的 `:48`/`:53-61` 一致**，但 `p1hips_oracle.hpp:382` 我未亲读。
- **S4-#5/#7**：归档 Makefile 的两个前置依赖路径实测不存在（`make` 直接 `No rule to make target`）；`clean` 用 Windows `del` 在 Linux 上是空操作。与我亲读的 `:23`/`:53` 一致。

---

## 8. 自证段（可复跑命令）

```bash
cd "/workspace/Astro CS Database"

# 基线与片成员（HEAD 实测 bf25c085，非单据所述 850a9ede）
git log -1 --oneline
sed -n '3018,3053p' run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml
while IFS= read -r f; do printf "%7d  %s\n" "$(wc -l < "$f")" "$f"; done < <(
  sed -n '3026,3053p' run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml | sed 's/.*"\(.*\)".*/\1/')
# 合计应为 10631

# A1 三式不一致（自洽式：三个 bytes-per-pixel 派生）
grep -n 'bits_per_sample > 0 && !image->float_sample ? 16 : -32' lib/infrastructure/aio/src/aio_fits.cpp   # :1158
grep -n 'image->float_sample || image->bits_per_sample == 32' lib/infrastructure/aio/src/aio_fits.cpp        # :1192
grep -n 'data_written = n_pixels \* (image->float_sample ? 4 : 2)' lib/infrastructure/aio/src/aio_fits.cpp   # :1211
sed -n '960,966p' lib/infrastructure/aio/src/aio_fits.cpp   # 读 BITPIX=32 → bits=32,float_sample=0

# A1 修正依据：c=1 的花括号位置（S5 对、我原读错）
sed -n '649,662p' lib/infrastructure/aio/src/aio_fits.cpp
sed -n '909,922p' lib/infrastructure/aio/src/aio_fits.cpp

# A2 静默截断 + A3 恒真诊断
sed -n '1028,1050p' lib/infrastructure/aio/src/hiss_reader.cpp
grep -n 'checksum_type != ChecksumType::NONE' lib/infrastructure/aio/src/hiss_reader.cpp   # :328
grep -n 'HISS_DLOG(' lib/infrastructure/aio/src/hiss_reader.cpp | head -3

# A3 反转门：唯一判据是 rc==0
sed -n '60,74p'  lib/infrastructure/aio/tests/p1hips/p1hips_tests_selfcheck.cpp
sed -n '145,172p' lib/infrastructure/aio/tests/p1hips/p1hips_tests_selfcheck.cpp
grep -n 'FAULT-INJECT' lib/infrastructure/aio/tests/p1hips/p1hips_tests_selfcheck.cpp || echo "-> 声称的判别证据从未被检查"

# A4 守恒门被前置条件关闭
sed -n '254,276p' lib/infrastructure/aio/product_io/src/provenance.cpp

# A8 错误码冲突（两份原文对照）
sed -n '55,67p' lib/include/acsd/common_abi_v1.h
sed -n '48,66p' lib/infrastructure/aio/io/include/acsd/io/fits_stream_v1.h

# A9 Makefile 自相矛盾 + 悬空路径
sed -n '1,3p;19p;23p;28p' lib/infrastructure/aio/healpix_db/archive/legacy/healpix_stack/Makefile
ls lib/infrastructure/aio/healpix_db/astro_image_io 2>&1 | head -1   # 实测不存在

# A10 私建线程池
sed -n '1201,1209p' lib/infrastructure/aio/src/aio_fits.cpp
grep -n 'fopenmp' lib/infrastructure/aio/Makefile

# A12 读回不比对 + 返回值丢弃
sed -n '60,82p' lib/infrastructure/aio/tests/hips_sanitize_driver.cpp

# 跨文件：FITS 写路径从不引用同目录的磁盘满分类头
grep -rn 'aio_disk_full\|note_failure\|note_full' lib/infrastructure/aio/src/ | grep -v aio_disk_full.h
```

---

## 9. 交付与后续

- 本文件为本次唯一交付件，未改动仓内任何其他文件，未执行任何 git 写、未编译、未跑测试/二进制、未读 `/tmp/acsd_g08/`。
- **遗留 UNRESOLVED（需负责人裁定）**：
  1. ~~归档 `healpix_stack` 的退役声明真伪~~ —— **已裁决（S4）**：调用者为零，声明为真；**假的是 `healpix_db/README.md:18` 的「活跃」标注与 `.gitignore:37` 的检索屏蔽**。升级为阻断 A15/A16。
  2. `aio_build_config.full.json:2,5` 启用「已废弃 ahpx」是否构成退役违规 —— 需读 `aio_ahpx_reader.cpp`（我未亲读；但 S2 已报该文件另有 2 BLOCKER）。
  3. `px` 与 `sr` 的历史等价是否有据（`bunit.cpp:64`）—— 需 DATA-001 schema 历史或遗留产品样本，代码内无法裁决。
  4. `tests/CMakeLists.txt` 记「75 PASS / 2 SKIP」与 `test_report.md` 记「100 PASS / 0 SKIP」并存 `[子代理结论·我未复核]` —— 需负责人确认二者是否为不同构建。
  5. `HAS_ZSTD` 在全仓任何 CMakeLists 中均未定义 ⇒ `test_transform.cpp` 的 ZSTD 测试与唯一的 Writer/Reader 端到端变换测试被**编译期剔除** `[S6·未复核]`。若属实，这是本片覆盖率之外最重的一条：不是测试红，而是**测试从未存在过**。