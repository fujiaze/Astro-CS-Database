# 审稿-P1 — INF-hips_browser-001（G08-05 对抗审稿 第 1 遍）

- **片号**：`INF-hips_browser-001`
- **层**：`lib/infrastructure/hips_browser`
- **基线**：任务书写 HEAD=`850a9ede`；**实测 HEAD=`1fa477a7a05c315550df2ce2bafc3e64ed9bbd78`**（「移除实验域运行结果归档，报告迁入单元文档目录」）。审稿期间仓被他人并发推进，子代理先后观测到 `f4a2cf21`、`9a83f63a`；经核 `1fa477a7` 存在且落后 HEAD **2 个提交**，且**子代理已在 `1fa477a7`（`git show`/`git ls-tree`）与 `9a83f63a` 两侧分别复核**，本片全部结论在三个提交上**一致成立**，故不因提交漂移作废。**任务书写的 `850a9ede` 未被复现，建议前台核对该基线是否笔误。**
- **口径**：结论全部来自本人逐行 read 原文 + 子代理独立复核；**未编译、未运行任何二进制、未跑 ctest/pytest、未执行任何 git 写操作、未改任何仓内文件**（本文件为唯一交付件；`git status --porcelain` 收尾为空）。
- **计数口径**：「成员行数」以片清单声明值与本人 `wc -l` 实测值并列；覆盖率按**行**计，不按字节。

---

## 1. 读完了吗

| 项 | 数 |
|---|---|
| 成员份数（片清单声明） | 23 |
| 实际读了几份 | **23** |
| 成员总行数（片清单声明 `实际行数`） | 6038 |
| 实测总行数（`wc -l`） | **6039** |
| 实际读了多少行 | **6039** |
| 覆盖率 | **100%（23/23 份，6039/6039 行）** |

**未读完的：无。** 零遗漏。

差 1 行的成因：分母以 `splitlines()`/末尾换行约定计，`lib/infrastructure/hips_browser/README.md` 在权威 CSV 中记为 16 行、本人实测 17 行（末行无换行符）。属分母侧计数口径问题，非漏读。

逐份读法记录见第 3 节。`gl_renderer.cpp`（2062 行）与 `browser_backend.cpp`（1043 行）均分块读到文件末尾（末行分别为 `:2062 return 0;`、`:1043 }`）。

---

## 2. 本片判定

### **判定：阻断**

最重 3 条：

**B1（阻断·科学语义）** `BrowserBackend::ud_grade` 把 signal **求和**（`core/browser_backend.cpp:629`），其依据注释 `:619-621` 自称「signal = 累计通量（HISS 规范: 不除面积）」。权威正本 `docs/science/DATA_SEMANTICS.md` §12.2/§12.3 明文规定 **signal 单位 `ADU/sr`，`signal = flux_sum / covered_area`，是面亮度**。合并 4^k 个**面亮度**像素取和，结果被放大 **4^k 倍**，产出的是无物理意义的量。头文件 `browser_backend.h:124` 与 `tests/test_browser_backend.cpp:73,76` 写的都是「均值」——**头与测试是对的，实现是错的**。这是本片最重的一条：它同时违反科学正本、违反自身接口契约，且现有测试无法发现（见 B3）。

**B2（阻断·读侧契约，且有在盘实证）** `HipsBrowserBackend::read_tile_at_order` 手工拼 `Dir = tile_ipix/10000`、`Npix = tile_ipix%10000`（`core/hips_browser_backend.cpp:274-282`），`load_order_tiles:313-316` 以 `dirnum*10000 + npix` 反解 —— 二者正是 AIO 自己标注为**非标准**的 `tile_path_legacy`（`aio_hips_reader.cpp:49-55`），且本片**无 standard-first 回退**（对照 `tile_path_resolve:59-67` 有）。权威正本 `DATA_SEMANTICS.md` §12.3 冻结判据：「目录布局：`Norder{K}/Dir{(ipix/10000)*10000}/Npix{ipix}.fits`（IVOA REC-HIPS-1.0 §4.1：Dir = 万进制块**起始值**，Npix = 完整 tile 号；判据：**Dir/Npix 一律按标准式写**，读侧标准优先，**legacy 布局只读回退**）」。writer 五个调用点（`aio_hips_writer.cpp:1034,1413,1629,1771,2055`）全部走标准式 `tile_rel_path:229-235`。

**在盘实证**（子代理实读真实产品文件，**本人亦已亲自复验**）：`run/VAR-SEMANTICS-APPLY-01/probe/m42_probe_patched/hips/signal/Norder6/Dir10000/Npix11026.fits`（另有 `Npix11027.fits`；`probe_orig/` 同形）⇒ 真实 tile 号 11026，目录名 `Dir10000` 恰为块起始值。此为标准式的**直接物证**，非推演。

- **失效阈值**：order ≤ 4 时最大 ipix = `12·4⁴−1` = 3071 < 10000，两式恰好重合故看似正常；**自 order ≥ 5 起（最大 ipix 12287）必然发散**。故本片读 tile 对**任何 order ≥ 5 的 ACSD 产品一律失效**，而 order ≤ 4 的小样本无法暴露。
- **读侧**：`read_tile_at_order(6, 11026)` 去找 `Dir1/Npix1026.fits`，真实文件是 `Dir10000/Npix11026.fits` ⇒ `read_fits_image` 失败 ⇒ 返回 `-2`（tile 不存在）。
- **扫侧**：`load_order_tiles` 把 `Dir10000/Npix11026.fits` 反解为 `10000·10000+11026` = **100011026**，凭空造出幽灵 id。
- **波及生产链**：`core/hips_sky_view.cpp:188`(`has_tile_at_order`)、`:193`/`:364`(`read_tile_at_order`) 驱动 GUI LOD；`app/browser_cli.cpp:317,801-813` 驱动 CLI 参考渲染 ⇒ **order ≥ 5 时 LOD 渲染全空、参考渲染取不到样本**。
- **测试结构性失明**：唯一覆盖 `read_tile_at_order` 的 `tests/test_geometry_truth.cpp:209` 用的是 **order 0**（ipix 0..11），**结构上不可能发现此缺陷**。

**B3（阻断·判据不可信）** 本片测试体系**没有任何一道门能转红**：
- `tests/test_browser_backend.cpp:76` `assert(std::fabs(output.pixel[0] - 2.5f) < 0.01f)` 解引用 `output.pixel`，而 `BrowserBackend::ud_grade` 只写 `pixel_u8`（`:637,655`），`pixel` 恒为 `LeafData` 构造函数初值 `nullptr`（`browser_backend.h:50-51`）→ **空指针解引用**；
- 即使修好，`:73` 期望**均值 2.5** 而实现给**和 10.0**，再经 `[0,255]` 归一化钳到 **255**——期望值在任何读法下都不可达；
- `tests/test_browser_backend.cpp:22-26` 文件不存在时打印 `[SKIP]` 直接 `return`，**不算失败**；测试数据路径 `:15-18` 硬编码 Windows `f:/` 绝对路径，仓外必不存在；
- 同文件 `:38-41` 断言 `get_all_data()` 非空，但按需重构后 `all_ipix_/all_pixel_` **再无任何写入点**（唯一写入者 `build_hiss_leaf_index` 零调用者），该断言一旦真被执行必失败；
- `healpix_browser_qt/CMakeLists.txt:16-18` 缺省 `Release` ⇒ `-DNDEBUG` ⇒ **全部 `assert` 被编译掉**；
- 仓根 `CMakeLists` 从不 `add_subdirectory` 本模块，`BUILD_TESTS` 缺省 OFF，无 `.github/`，**无任何 CI**；
- `CMakeLists.txt:164` 注册 ctest 却不传 `<hips_root>`，而测试 `:45-48` 缺参即 `return 2` ⇒ 该 case 必红。

⇒ 「检查通过」在本片是**结构性不可能**，任何据此的放行都不成立。

---

## 3. 逐文件清单（读了什么 → 看到什么 → 判定）

| # | 文件（相对 `lib/infrastructure/hips_browser/`） | 读了什么 | 看到什么（带 `文件:行`） | 判定 |
|---|---|---|---|---|
| 1 | `README.md` (17) | 全读 | `:12` 列举 `CMakeLists.txt`/`Makefile`/`deploy.ps1`/`run_healpix.bat` — 四者经 `find` 确认**均存在**；`:17` 指向 `docs/ACSD_DESIGN.md §8.4`，而 `PENDING.md:4` 指向 `7.1` — 两文件对同一节号说法不一致 | 须修（节号自相矛盾） |
| 2 | `PENDING.md` (8) | 全读 | `:3` 「**本目录为空，未含任何实现；不代表模块已实现或可用**」；实测本目录 46 文件 / 含完整 Qt+C++ 组件 → **事实性矛盾**。`:5,8` 指向 `eng/cmake/ARCH-001-migration-manifest.md` — 经核**存在** | 须修（虚假登记） |
| 3 | `healpix_browser_qt/README.md` (128) | 全读 | `:3` 「已归档至 `../archive/`」— **目录不存在**（全仓 `archive/` 均在 `lib/infrastructure/aio/healpix_db/` 或 `run/**`，无一在 hips_browser 下）。`:17` 「`archive/single_frame_view.*`」— `find -name "single_frame_view*"` **全仓零结果**，非归档而是删除。`:127` 指向 `docs/detail/healpix_browser_qt.md` — **不存在**。`:76-78` 的「5/5」「4/4」**计数本身准确**（`test_healpix_math.cpp` 恰 5 个测试函数、`test_stf_engine.cpp` 确存在且 4 个测试全覆盖所述四项、解释器确为 4 个）—— **此处对 README 公平：STF/HEALPix 两项的测试存在性与覆盖声明成立**；唯 `test_browser_backend` 的「4/4」实质是 **1 真 / 1 SKIP / 2 空转**（`:52-54` 只断言打不开一个不存在的文件）。`:20` 「4 滑块」已被 `stf_panel.h:7` 的 v3 三控制点取代。`:9-21` 架构段完全未提现 HiPS 路径。`:96` 判据 `grep -r "Q" core/` **无效**（实测 11 命中：8 处是「无 Qt 依赖」注释本身、3 处是 `kQuad*Shader`，**零个 Qt 类型** ⇒ 在正确代码上恒红，违反 AGENTS.md §8「判据须在正确实现下绿」） | 须修（悬空引用 + 无效判据 + 与源码脱节） |
| 4 | `healpix_browser_qt/memory.md` (330) | 全读 | `:9-11` 指向 `docs/superpowers/...` 3 文件 — **`docs/superpowers` 整目录不存在**（README:127 自承「已删」）。`:20,28` 「归档至 `widgets/archive/`」— **不存在**。`:265,278,243` 记「4/4 PASS」「5/5 PASS」为现行成绩，但依 B3 不可复现 ⇒ **自愈锚**。`:63`「FOV 范围 [0.5°, 50°]」与 `:112`「[0.5°, 170°]」**同文件内互斥**；`:39-43`（赤道仪=最终）与 `:85-93`（虚拟轨迹球=最终成功）**互斥**。`:48` 给出 `58.6` 的**正确**推导（√(π/3)/nside×180/π） | 须修（悬空引用 + 陈旧 PASS + 互斥结论） |
| 5 | `healpix_browser_qt/include/healpix_browser_core.h` (16) | 全读 | 纯聚合头，`:11-14` 四个 include 均存在。**但 `:6` 编译注释写 `-I../../astro_image_io/include`** —— 经核 `lib/infrastructure/astro_image_io/` **不存在**（真实位置 `lib/infrastructure/aio/`），全片共 8+ 处沿用此错路径 | 须修（悬空路径，承 §11.14） |
| 6 | `healpix_browser_qt/core/logger.h` (86) | 全读 | `:35` `localtime(&now)` 非线程安全且**未持锁**即 `strftime` → 并发日志时间戳可撕裂。`:54` `if (buf.size() < MAX_LOG_BUFFER)` 超限**静默丢弃**、无计数无告警。`:64-65` `fopen(path,"w")` — **截断**既有文件，且 `if (!fp) return;` 静默吞掉写失败（缓冲不落盘、调用方不知）。`:39-44` `header_len` 未校验即用于 `sizeof(msg)-header_len`（当前字面量下安全，属未加固路径） | 须修（静默降级 + 自覆写） |
| 7 | `healpix_browser_qt/core/browser_backend.h` (236) | 全读 | `:124` 「4^k 个相邻像素**求均值**合并」— 与 `:629` 实现（求和）**直接冲突**（B1）。`:204-207,211-212` `is_hiss_/nside_/n_pix_/nested_/all_ipix_/all_pixel_` **无初始化器**（构造函数 `:31-32` 确有补齐，指针类由 ctor 覆盖 → 实为安全，但头部自相矛盾）。`:109` 契约「限制最大 100 个」而 `:417` 实现**明确不限** → 契约失效。`:127-128` `ud_grade` 默认实参 `0.0f,1.0f` 对真实像素数据**静默错误** | 须修（契约冲突 + 契约失效） |
| 8 | `healpix_browser_qt/core/browser_backend.cpp` (1043) | 全读（分 4 块，末行 `:1043 }`） | `:629` 求和 vs 正本 ADU/sr（**B1**）；`:619-621` 注释所引「HISS 规范」与 `DATA_SEMANTICS.md` §12.2 冲突。`:452` 注释推式 `360/(θ√12)=58.6/θ` — **算术错**（360/√12=103.92，比值恰为 √π）；常量 58.6 本身**正确**（memory.md:48 给出正推导，gl_renderer.cpp:1387-1388 亦正确）⇒ **注释错、数值对**，属高危误导。`:625` `n_pix >> shift + 1` **缺括号**（`+` 优先于 `>>`）。`:655` `(uint8_t)(normalized+0.5)` 在 normalized=255.0 时为 `(uint8_t)255.5` = **UB**（钳位在加 0.5 之前）。`:715-728` bbox 用 `min_ra/max_ra` 直算，`center_ra=(min+max)/2`、`width=max-min`，**未处理 RA 环绕** ⇒ 跨 RA=0 的 patch 得 `center_ra=180°`、`width=359°`（对照本文件 `:969` `need_rebuild_mesh` 处**确已**处理环绕 ⇒ 作者知晓而此处漏）。`:782-788` `.hcsd` 返回**硬编码全天** bbox 且 `return 0` ⇒ 以「测量成功」形态交付占位值。`:52-146` `build_hiss_leaf_index` **零调用者**（死代码 95 行），且其中 `:89-90` `std::malloc` 与 `:138-139` `hio_free` 跨分配器（`hio_free` 经核为 `aio_hio_free` 宏，`aio_healpix_io.h:258`，同函数；MinGW 单 CRT 下实际兼容 → 降为建议）。`:223-226`/`:883-886` `if (tile_ipix_list && n_tiles>0)` — `n_tiles==0` 时**泄漏**；`assign(..., +n_tiles)` 未校验列表长度。`:818-840` `detect_fp64_from_meta` 纯字符串搜索、**默认 FP32**，`:827` `if (*p=='1')` 无法区分 `1` 与 `10/11/12`。`:889` `load_hiss` 覆盖 `file_path_` 却**不置 `is_hiss_`**，致 `hiss_header_loaded_=true` 而 `:376,:457` 的 `if (is_hiss_ && ...)` 永不成立 ⇒ 载入的 header 实际不被使用。`:483-484` 注明「read_tile_signal_f64 由 AIO 返回错误」但本地**未实现任何 dtype 守卫**，安全完全外包 | **阻断**（B1 在此） |
| 9 | `healpix_browser_qt/core/gl_renderer.cpp` (2062) | 全读（分 4 块，末行 `:2062 return 0;`） | `:1316` `render_sphere` **无条件 `return 0`**：无子叶、加载全失败、0 顶点、GL 出错，一律报成功。`:1179-1182` 空的 `load_leaf` 结果**无日志无计数**直接 `continue`（`:1158-1159` 的 `newly_loaded`/`reloaded` 写了从不读 —— 恰是能发现此事的仪表却已死）。`:1142-1155` `get_required_leaves` 返回空即**清空整个 leaf 缓存**并以 `no_data_value` 填满顶点 ⇒ 全黑帧仍 `return 0`。`:1444` 除以 `all.nside`，而 `:1389` 用 `use_nside` ⇒ 自动降采样触发后菱形半对角线**小 2^k 倍**（nside=8192→512 时小 16 倍，几何近 99.6% 空洞），而 `:1390` 的日志**恰恰用正确的 `use_nside` 打印「健康」像素尺寸** ⇒ 典型自愈锚。`:1399` `value <= 0.0f` 一律跳过，但 signal 是 ADU/sr 面亮度，**扣背景后可为负** ⇒ 该筛子**滤掉全部本底/负值信号**，并使 `:1409-1412` 的 bbox 只覆盖亮核 ⇒ 筛掉真信号。`:1245-1267` 逐级降 nside（`MAX_SHIFT=4`，达 nside/16）用**更粗邻像素**的值顶替缺失像素 ⇒ 制造信号。`:1251-1253` 使用 `lm.ipix` 而 `:1179` 只校验 `pixel/pixel_u8` **未校验 `ipix`** ⇒ 潜在空指针。`:1470-1473` 全部像素被跳过时 `min/max` 保持初值 ⇒ `hiss_width_deg_=-360`、`hiss_height_deg_=-180`（**负展度**）而 `:1503-1509` 仍 `hiss_mesh_valid_=true` 且 `return 0`。`:738,:888` 顶点查值**硬编码 `ang2pix_nest(64,...)`**，而后端已改用 `hiss_header_.tile_nside`（可 16/64/128/8192，`:454-458` 明记「不再硬编码 64」）⇒ tile_nside≠64 时 leaf id 算错。`:1581,1589-1593` uniform 位置**未做 `>=0` 判空**，与 `:1287-1305`、`:1763,1775-1779` 同一文件内不一致。`:961-963` 重建判据只看 `viewport_w` 从不看 `viewport_h`，而 `:824` 的 `theta_screen` 依赖 `viewport_h` ⇒ 纯纵向 resize 不重建网格。`:1613-1788` `render_single_frame`（175 行）+ `set_single_frame_bbox`/`upload_leaf_texture`/`evict_unused_leaves`/`get_loaded_leaves` **全部零调用者**，且**无退役注释**（违反 AGENTS §6）。`:1026-1052` `upload_leaf_texture` 齐备而 `leaf_textures_` 永无 `push_back` ⇒ `:515-518`、`:1059-1075` 遍历恒空、`:622-629` 恒返回空。`:1663-1666` 以 `(ra!=0 \|\| dec!=0)` 判定「视图已设置」，故**正中心 (RA=0,Dec=0)** 的合法视图被误判为未设置。`:1718` `while (ra_deg<0.0) ra_deg+=360.0` **无界循环**；`:1709` `asin` 入参可能越界 → NaN 未夹。`:441` 注释称析构「尝试释放」而 `:440-445` **只打 WARN 不释放**。`:1531` 注释「FOV 范围 [0.5°, 170°]」与 `:1533` 判据 `0.01` 相邻两行互斥；`:950` 注释「半个视场」而 `:971` 实为 `half_fov*0.5`（四分之一）。**无任何线程/线程池/OpenMP**（`thread|async|QThread|OpenMP` 全文件零命中）⇒ 私建线程池项**通过**；着色器 compile/link 状态**均有检查并输出 info log**（`:374-382`/`:402-410`）⇒ 该项**通过** | 须修（多处静默降级 + 死代码） |
| 10 | `healpix_browser_qt/core/hips_browser_backend.h` (91) | 全读 | 错误码设计良好（`0/-2/-3`，`:45`）。`:39` `get_tile_width()` 全仓**零消费者**。`:83-84` `order_/leaf_order_` 注释与 `:162,:169` `leaf_order_=order_+9` 一致 | 建议 |
| 11 | `healpix_browser_qt/core/hips_browser_backend.cpp` (359) | 全读 | `:274-282`/`:313-316` Dir/Npix 用 legacy 式，与 §12.3 判据及 writer 实际产出冲突（**B2**）。`:74,:80` `f.read(...)` **从不检查 `gcount()`/`good()`**，`out.resize(n)` 补零后仍 `return true` ⇒ **截断的 FITS 被当作完整零值数据上报**。`:285-286` flat 布局 `u.assign(s.size(), 1.0f)` **凭空捏造 support=满覆盖**并以真实读数形态返回（掩膜被伪造），且与 `hips_browser_backend.h:55` 的契约（缺失应返回 -3）相抵触。`:118-176` flat 下 `sig_` 必空（`aio_hips_open` 对 manifest 与 `hips_version` fail-closed），`open_product` 仍 `return 0` ⇒ **5 个公开 API + `is_open()` 全灭**（详见 §4-3）。`:346` `const int max = 100000` 硬编码，`aio_hips_reader.cpp:657` 以 `std::min` 截断且**无溢出信号**，后端 `:353-357` 亦无 ⇒ 无法区分「恰好 10 万」与「被截断」。`:159/:161/:170/:186` 写 `width_`，所有真实 tile 运算用硬编码 `kTileDim`(512)（`:220,243,248,260-261,284`）；`get_tile_width()` **全仓零消费者**；且 `:158-159` 的解析只在 flat 分支（`:148` 的 else，需 `sig_==nullptr`）进入 ⇒ **`hips_tile_width` 在正常 ACSD 嵌套布局下压根不被解析**。`:100-102`、`:166-168`、`:156,:158` 均为**子串搜索**（后者对**键名**也做子串 ⇒ `max_hips_order` 可误命中；循环内**文件序最后一个匹配胜出**）。`:198-209` `contains()` 对全部 tile 线性扫描 ⇒ O(n) 不可逐像素调用 | **阻断**（B2 在此） |
| 12 | `healpix_browser_qt/widgets/hips_view.h` (76) | 全读 | 成员**全部有初始化器**（`:67-73`）——与 `sphere_view.h` 形成鲜明对照，本文件较干净。`:34` 「唯一状态」措辞无对应约束 | 通过 |
| 13 | `healpix_browser_qt/widgets/hips_view.cpp` (210) | 全读 | `:105-108` `QImage(...).copy()` 深拷贝**正确**（未别名到 `rgba_`）。`:106` 之前 `sky_.rasterize(rgba_)` **无返回值/尺寸校验**，若 `rgba_.size() != w*h` 则按 `w*h` 读取 ⇒ 潜在越界读（本片内无护栏）。`:112` 在 `paintEvent` 路径内 `emit rendered()` ⇒ 重入风险。`:206-209` `save_snapshot` 失败仅返 `bool`，无错误码无日志。`:158` `has_cursor_` 门控正确 | 建议 |
| 14 | `healpix_browser_qt/widgets/sphere_view.h` (138) | 全读 | `:65-67` `center_ra_/center_dec_/fov_deg_`、`:91-99` `is_dragging_/last_mouse_x_/last_mouse_y_/is_touching_/last_touch_x_/last_touch_y_/last_touch_dist_` **全部无初始化器**（同文件 `:74-79` 却有）⇒ 构造遗漏即读未初始化值。`:102` 注释「最小FOV(最大放大, **去掉限制**)」而限值 `0.01` **确实存在** ⇒ 注释与码互斥。`:38` `set_render_mode` 内联且**不 `update()`** ⇒ 改模式不重绘。`:105-111` FOV_SPEED/DRAG_RATIO/ARROW_RATIO/FOV_STEP 四个魔数无出处未配置。`:5` 指向不存在的 `docs/detail/healpix_browser_qt.md` | 须修（未初始化成员 + 悬空引用） |
| 15 | `healpix_browser_qt/app/stf_bar.h` (72) | 全读 | `:53` `(v-view_lo_)/(view_hi_-view_lo_)` 除零可能 —— 经核对 `wheelEvent`（`stf_bar.cpp:183-186`）恒保持差值 ≥ `kMinWindow=0.05` ⇒ **不构成除零**，记为已加固。`:47,:52` `std::max/clamp` 用而未 `#include <algorithm>`（`stf_bar.cpp:8` 补上，头不自洽） | 通过 |
| 16 | `healpix_browser_qt/app/stf_bar.cpp` (204) | 全读 | `:16-17` 写 `dmin_/dmax_`，**全仓无任何读取点** ⇒ 死存储；而 `stf_panel.h:28-30` 明写 `set_data_range` 用途是「控制点 shadows/highlights 映射到 `[data_min,data_max]` 原始像素值」，`:41-48` `params()` 实际仍返回**归一化 [0,1]** ⇒ **文档承诺的映射不存在**（调用链 `MainWindow:562→STFPanel:54→STFBar:15` 走通但无效果）。`:53` `handle_at` 跳过窗口外控制点，而 `:143-153` 的最近点吸附**不跳过** ⇒ 可选中并拖动**当前不可见**的控制点。`:36-37` `enforce_window` 把 midtones 强行夹进 `[shadows,highlights]`，**静默覆盖用户设定的 midtones**，使 `:23`/`:138`/`:157`/`:168` 的 `kMidLo..kMidHi` 夹取大半失效 | 须修（死存储 + 文档承诺落空） |
| 17 | `healpix_browser_qt/app/stf_panel.h` (52) | 全读 | `:49` `QPushButton* auto_button_;` **无 `= nullptr`**（同行 `:46` 却有）⇒ 未初始化指针。`:30` `set_data_range` 的承诺落空（见上） | 须修 |
| 18 | `healpix_browser_qt/app/main.cpp` (152) | 全读 | `:100-107` 截图用 `QTimer::singleShot(1200,...)` **定时而非信号** —— `open_file_from_cli` 是 `:96-99` 的 QueuedConnection，大产品树 + 目录扫描常超 1.2 s ⇒ 截到空/半渲染窗口并退出，且**退出码 0**。`:109-127` 另用 950/1000 ms，`:138` 1400 ms，全部定时竞态；而 `hips_view.h:51` 已有 `rendered()` 信号本可替代。`:108` `--lod` 非法值**静默忽略**；`:104` `--layer` 非法值静默变 signal；`:130-131` `--view` 解析失败**静默跳过**且**不校验 ra/dec/fov 范围**（`fov` 可传 0 或负）。`:146-149` 日志仅在设 `BROWSER_LOG_FILE` 时落盘，未设则最多 50000 行缓冲（`logger.h:29`）**全部无声丢弃** | 须修（定时竞态 + 无效参数静默吞） |
| 19 | `healpix_browser_qt/tools/hips_tile_oracle.py` (250) | 全读 | `:250` `sys.exit(marker_oracle())` —— 而模块 docstring `:15-17` 描述的 CLI（`<ref_oracle_hips> [n_tiles] [n_points]`）对应的是 `main()`，**`main()`（`:58-161`，约 100 行）不可达死代码**。`:155`/`:238` 输出写死 `run/temp/p2_v11/evidence/hips_tile_oracle.json` —— 经核 **`run/temp/p2_v11` 不存在**，两函数还写**同一文件名**互相覆盖。`:39-40` `A=1_000_000.0`、`B=707_106.7811865475` 为**无出处魔数**，与文档所称「ref_oracle 的 float64 编码」无绑定（未从 ref 卡读取）。`:120` `k = argmin(|lin - v|)` **无拒绝半径** ⇒ 永不失败，编码不符也只会自信地给出最近邻结论。`:124` `got` 计算后**从不使用**；`:79-81` `expected_local` **从不调用**。`:13` 宣称可区分 transpose，但当 `col == 511-row`（反对角线）时 ACSD 候选与 transpose 候选**恒等**，而 `:107-108` 的 9 个定点探测中 `(0,511)`、`(511,0)` **恰在反对角线上** ⇒ 该两点被计入 `acsd_match`（`:127` 先匹配即计），抬高 ACSD 得分 | 须修（外部裁决工具本身失效） |
| 20 | `healpix_browser_qt/tests/gen_hips_browser_test.py` (139) | 全读 | `:34` `Path(__file__)`，但 `:10-19` 的 import 列表**无 `from pathlib import Path`**（`grep -c pathlib` = **0**）⇒ **模块级 `NameError`，脚本在任何机器上都无法启动**。`:88-90` `if t in tile_ipixs: continue` 去重但 `ti` 未用。`:93-94` `flux[:] = 3.0+0.5*len(...)`、`area[:] = (0.75+0.05*len(...))*a_cell` —— **每 tile 内空间常数** ⇒ 测试夹具本身**无法暴露任何 tile 内索引置换错误**。`:125` tile 全在 RA 10-22 ⇒ 也不覆盖 RA 环绕 | **阻断**（G3 夹具生成器完全不可用，见 B3） |
| 21 | `healpix_browser_qt/tests/test_browser_backend.cpp` (100) | 全读 | `:76` 空指针解引用；`:73` 均值 vs 实现求和；`:22-26` SKIP 不失败；`:15-18` 硬编码 `f:/`；`:38-41` 断言不可达；`:78-79` `release_leaf(input)` 因 `input.owned==false` **不释放测试自身 `malloc` 的内存** ⇒ 测试自身泄漏。README 宣称的「4/4」依此不可复现 | **阻断**（B3 在此） |
| 22 | `healpix_browser_qt/tests/test_healpix_math.cpp` (98) | 全读 | `:80` `assert(result.nside == 2)` 与被测函数内部赋值同源 ⇒ **自洽式断言**（被检量即期望量）。`:86` 期望均值 2.5 —— 对 `HealpixMath::ud_grade` **正确**（该实现确为求均值），但与 `BrowserBackend::ud_grade` 求和**语义相反**，两份同名函数物理语义冲突且后者无有效测试。`:17` `dec > 35 && dec < 45` 对已知 41.81° 给了 ±5° 松窗 ⇒ 弱断言。`:22` `printf(..., 41.81, dec)` 把**字面量 41.81 当测得值打印**，实参 `ra` 从未校验（`:24 (void)ra;`） | 须修（自洽断言 + 弱窗 + 假输出） |
| 23 | `healpix_browser_qt/tests/test_hips_browser_backend.cpp` (172) | 全读 | 文件头 `:4-10` 自称「硬门 (G3)」「与直接 AIO Reader 逐值比较, mismatch=0」。但 `:122` `nested_local_to_fits_index(z, 9u, 512u)` **与被测 `query_pixel` 内部 `hips_browser_backend.cpp:220` 同一函数同参数**；`:33-34` 的 `kTileDim=512`/`kTileMask=(1<<18)-1` 与 `.cpp:33-35` **逐字重复**；`:94` `(tile_ipix<<18)|z` 与 `.cpp:218-219` 同构 —— **被检映射与期望映射同一定义式，系统性错位自动抵消**，这正是本轮要找的最有价值形态。`:106` 注释甚至自承「同一映射」。叠加夹具空间常数（`gen_hips_browser_test.py:93`）⇒ **索引映射错误在本门完全不可见**。`:129` `no_data` 统计**不进 `:169` 的 pass 判据**。`:168` `snr_ok = (n_snr <= 0) || (...)` ⇒ **SNR 读取失败/缺失即整体跳过该检查**，与文件头「SNR catalogue 读取」承诺相反。`:169` `outside_ok >= 1`（64 次仅需 1 次）⇒ 门极松。`:92` `tiles[rng() % tiles.size()]` 未防 `tiles` 为空 ⇒ 除零崩溃。`:36-40` `struct RowResult` 死代码。`:111-117` AIO 读返回值全忽略，`tmp` 跨 tile 复用 ⇒ 前一 tile 读失败时「参考值」静默变前一 tile 数据 | 须修（自洽断言 + 恒真门） |

---

## 4. 发现清单

### 阻断（3）

| ID | 发现 | 位置 |
|---|---|---|
| **B1** | `ud_grade` 对面亮度 signal 求和，违反 `DATA_SEMANTICS.md` §12.2/§12.3（signal=`ADU/sr`=`flux_sum/covered_area`），结果放大 4^k 倍；头文件与测试的「均值」契约反被实现违背 | `core/browser_backend.cpp:619-630`（对 `docs/science/DATA_SEMANTICS.md` §12.2、§12.3） |
| **B2** | Dir/Npix 采用 legacy 分解式（商+余数），与 writer 实际产出的 REC-HIPS-1.0 §4.1 标准式（块起始+完整号）不符，且**无 standard-first 回退**；违反 §12.3 冻结判据。**在盘实证**：`Norder6/Dir10000/Npix11026.fits` 等真实文件被读成 `Dir1/Npix1026.fits`（失败）并被扫成幽灵 id `100011026`；**order ≤ 4 两式巧合重合、order ≥ 5 必然发散** ⇒ GUI LOD 与 CLI 参考渲染对任何 order ≥ 5 产品全空；唯一覆盖测试 `test_geometry_truth.cpp:209` 用 order 0，**结构上不可能发现** | `core/hips_browser_backend.cpp:274-282`、`:313-316`（对 `docs/science/DATA_SEMANTICS.md` §12.3、`lib/infrastructure/aio/src/hips/aio_hips_writer.cpp:229-235`） |
| **B3** | 本片无任何可转红的门：`test_browser_backend.cpp:76` 空指针解引用 + 均值/求和/uint8 三重不可达；`:22-26` SKIP 不失败；`:38-41` 断言不可达；CMake 缺省 Release→`NDEBUG` 编译掉全部 assert；无 CI；ctest case 缺参必红；G3 夹具生成器 `NameError` 无法启动 | `tests/test_browser_backend.cpp:76,73,22-26,38-41`；`healpix_browser_qt/CMakeLists.txt:16-18,164`；`tests/gen_hips_browser_test.py:34` |

### 须修（21）

1. `read_fits_image` 吞掉读取失败：`:74,:80` 无 `gcount()`/`good()` 校验，截断文件补零后仍 `return true` — `core/hips_browser_backend.cpp:74,80`
2. flat 布局捏造 `support=1.0`（掩膜被伪造成满覆盖）且以真实读数形态返回 — `core/hips_browser_backend.cpp:285-286`
3. flat 布局下 `open_product` 返回 0 而 **`is_open()` 恒 false**，且**共 5 个公开 API 全死**：`contains()` 恒 false（`:199`）、`query_pixel()` 恒 -3（`:215`）、`read_tile()` 恒 -1（`:242`）、`read_support_tile()` 恒 -1（`:259`）、`read_snr_catalog()` 恒 -1（`:345`）⇒ **打开报成功而查询面全灭**。根因：`aio_hips_open` 对 `root/manifest.json` 与 `signal/properties` 的 `hips_version` 是 **fail-closed**（`aio_hips_reader.cpp:408-442`），flat（Hipsgen）布局二者皆无 ⇒ `sig_` 必空 — `core/hips_browser_backend.cpp:118,126-127,176`；`core/hips_browser_backend.h:35,42`（`is_flat_standard()` 亦零调用者）
4. `read_snr_catalog` 硬编码上限 100000 且无法区分「恰好」与「被截断」 — `core/hips_browser_backend.cpp:346,350-357`
5. `get_data_bbox` 未处理 RA 环绕 ⇒ 跨 RA=0 的 patch 初始视角跳到对跖点（width 359°）— `core/browser_backend.cpp:715-728`
6. `render_sphere` 无条件 `return 0`；空叶静默丢弃；空 required 清空全缓存 ⇒ 全黑帧报成功 — `core/gl_renderer.cpp:1316,1179-1182,1142-1155`
7. `:1444` 用 `all.nside` 而 `:1389` 用 `use_nside` ⇒ 菱形几何小 2^k 倍，而 `:1390` 日志按正确值报「健康」（自愈锚）— `core/gl_renderer.cpp:1389,1390,1444`
8. `value <= 0.0f` 筛掉全部本底/负值面亮度，并使 bbox 只覆盖亮核（筛掉真信号）— `core/gl_renderer.cpp:1399,1409-1412`
9. 顶点查值硬编码 `nside=64`，与后端已改用 `tile_nside` 冲突 — `core/gl_renderer.cpp:738,888`；`core/browser_backend.cpp:454-458`
10. 缺数据时以更粗像素顶替缺失像素（`MAX_SHIFT=4`，达 nside/16）制造信号 — `core/gl_renderer.cpp:1245-1267`
11. `lm.ipix` 未做空校验即参与 `lower_bound` — `core/gl_renderer.cpp:1179,1251-1253`
12. 全像素被跳过时产出**负展度** bbox 仍报成功 — `core/gl_renderer.cpp:1470-1473,1503,1509`
13. 约 370 行死代码无退役注释（`render_single_frame` + 4 方法）；`build_hiss_leaf_index` 95 行零调用者无退役注释 — `core/gl_renderer.cpp:1613-1788` 等；`core/browser_backend.cpp:52-146`
14. **`docs/detail/healpix_browser_qt.md` 被引用 13 处、横跨 9 个文件（本片内 6 个），全部落空** —— 本组件在权威链中**没有任何有效上游设计文档**，双向违反 AGENTS.md §3。本片内 6 处：`sphere_view.h:5`、`stf_panel.h:6`、`browser_backend.h:6`、`main.cpp:5`、`gl_renderer.cpp:9`、`healpix_browser_qt/README.md:127`。**修一处或删全部引用即可消掉最大一簇。**
15. 外部裁决工具自身失效：`main()` 不可达；输出/输入目录 `run/temp/p2_v11` 不存在；A/B 无出处魔数；`argmin` 无拒绝半径永不可证伪 — `tools/hips_tile_oracle.py:250,155,174,176,39-40,120`
16. `memory.md` 的 PASS 记录为**自愈锚**，且同文件内 FOV 范围与「最终方案」互斥 — `memory.md:63,112,39-43,85-93,243,265,278`。**更严重：`memory.md:146,147,148` 三条归档陈述均为假** —— archive/ 下只有 `legacy/healpix_stack/`，`healpix_browser_cpp/` 与 `healpix_browser_web/` 已全仓删除、`archive/README.md` 不存在，`:4` 仍以这两个已删前身为模块定位。根因见 §8 注：HEAD 提交 `f4a2cf21` 自承「清理科学正本对已删归档的依赖」，而 memory.md 与 README 未同步 ⇒ 归档叙事被留成悬空引用
17. `PENDING.md:3` 「本目录为空，未含任何实现」与实际 46 文件 / 12077 行 / 其中 10473 行 C++ 矛盾（`PENDING.md` 就躺在它自称空白的目录里，与同目录 `README.md` 互相打脸）；**`PENDING.md:7`「原因：旧路径仍在原处」同样自相矛盾** —— 组件已在目标路径；`PENDING.md:4` 引 `ACSD_DESIGN.md 7.1`（「命令树」，无关）而全文档仅 `docs/ACSD_DESIGN.md:497` 一处提及本目录、归属 **§8.4** ⇒ `README.md:17` 的 §8.4 正确、`PENDING.md:4` 错误；`README.md:96` 提出无效判据 `grep -r "Q" core/`（实测 11 命中：8 处是「无 Qt 依赖」注释本身、3 处是 `kQuad*Shader` 里的 Q，**零个 Qt 类型** ⇒ 该判据在正确代码上恒红，违反 AGENTS.md §8）
18. **`../../astro_image_io/` 错路径在片内 8+ 处沿用**（真实位置 `lib/infrastructure/aio/`）— `healpix_browser_qt/README.md:30,60,81`、`memory.md:141,142,192,220,264,302`、`include/healpix_browser_core.h:6`、`core/gl_renderer.cpp:7`、`tests/test_browser_backend.cpp:3,6`。连带片外 `Makefile:9` `AIO_DIR` ⇒ **Makefile 全链路失效**（`CMakeLists.txt:28` 写的是正确的 `../../aio`，两套构建系统互相矛盾）。**且 `astro_image_io.dll` 被 `.gitignore`（`lib/infrastructure/aio/.gitignore:1: *.dll`）排除、未被 git 跟踪 ⇒ 即使路径改对，指到的也是构建产物而非仓内内容**
19. `healpix_browser_qt/README.md:20` 称 `STFPanel` 为「4 滑块」，而 `app/stf_panel.h:7` 明写「v3：三控制点…替代 4 滑块」且 `stf_panel.cpp` 已无 `QSlider/QComboBox` ⇒ README 落后一个版本。README 架构段（`:9-21`）只描述旧三层 HEALPix 设计，**完全未提现已作为「正式 Browser 数据源」的 HiPS 路径**（`hips_browser_backend.h:4`）⇒ README 描述的是一个与自身源码不再匹配的组件
20. `healpix/healpix_core.h` 仓内**存在三份互异副本**（`lib/algorithms/shared/`、`lib/algorithms/drizzle/healpix_drizzle/`、`lib/infrastructure/aio/healpix_db/archive/legacy/healpix_stack/`），而 `hips_browser_backend.cpp:8` 称其为「共享 HEALPix core 标准映射」⇒ 分叉风险（本次未逐份 diff，登记待查）
21. `memory.md` 另 4 处悬空：`docs/superpowers/*`（整目录已删，`:9-11`）、`eng/tests/test_*.cpp`（实为 Python 测试树，`:234,254,275`）、`lib/include/healpix_browser_core.h`（路径错，`:205`）、`PROJECT_ARCHITECTURE.md`（不存在，`:312`）

### 建议（14）

`STFBar::dmin_/dmax_` 死存储致 `set_data_range` 文档承诺落空（`stf_bar.cpp:16-17` vs `stf_panel.h:30`）｜`stf_bar.cpp:36-37` 静默覆盖用户 midtones｜`stf_bar.cpp:143-153` 可拖动不可见控制点｜`sphere_view.h:65-67,91-99` 与 `stf_panel.h:49` 未初始化指针成员｜`sphere_view.h:38` 改模式不重绘｜`sphere_view.h:102` 注释与限值互斥｜`browser_backend.cpp:625` `>> shift + 1` 缺括号｜`browser_backend.cpp:655` `(uint8_t)255.5` UB｜`browser_backend.h:109` 「最多 100 个」契约已失效｜`browser_backend.cpp:818-840` `detect_fp64_from_meta` 纯子串匹配且默认 FP32｜`browser_backend.cpp:223-226,883-886` `n_tiles==0` 时泄漏｜`main.cpp:100-141` 定时竞态与无效参数静默吞｜`logger.h:35,54,64-65` `localtime` 非线程安全、超限静默丢、`fopen("w")` 截断｜`hips_view.cpp:106` `rasterize` 后无尺寸校验

---

## 5. 我主动构造的反例

| # | 构造 | 期望推翻 | 是否推翻 |
|---|---|---|---|
| E1 | 取 `signal ∈ ADU/sr` 且 `data_min=0,data_max=1000` 的 2×2 块（2x2 合并，ratio=4，k=1），四个值各 300：科学正本要求面亮度 ≈ 300，`ud_grade` 应给 300；实际 `:629` 求和得 1200，`:652` 归一化 `(1200-0)*255/1000=306` ⇒ `:654` 钳到 255 ⇒ 渲染值等同 `data_max`，**最亮区与次亮区全部压成同一个白** | 「signal=累计通量」注释成立 ⇒ 求和正确 | **推翻。** 权威 `DATA_SEMANTICS` §12.2/§12.3 规定 signal 是面亮度；求和使亮于 `data_max/4` 的块一律饱和 |
| E2 | 取标准布局文件 `Norder3/Dir10000/Npix10302.fits`（IVOA 标准式，writer 确实这样写）：`read_tile_at_order` 想找 `Dir1/Npix302.fits` ⇒ 不存在 ⇒ 返回 `-2`「tile 不存在」；同一文件被 `load_order_tiles` 扫到后反解为 `10000*10000+10302=100010302` | 「legacy 式只是扫描端容忍」 | **推翻。** 读路径与扫描路径双双错，且 `DATA_SEMANTICS.md` §12.3 判据把标准式定为唯一主式、legacy 仅作只读回退 |
| E3 | 让 `test_ud_grade` 在断言开启下运行 | 该测试能验证 `ud_grade` 语义 | **推翻。** `:76` 解引用恒为 `nullptr` 的 `output.pixel` ⇒ 空指针解引用；即便修好，期望的均值 2.5 在「求和→uint8 归一化」下不可达（实测语义为 255） |
| E4 | 构造跨 RA=0 的 patch（tile 在 RA 359.5 与 RA 0.5），调 `get_data_bbox` | 得到 `center_ra≈0°, width≈1°` | **推翻。** `:721-728` 得 `min_ra=0.5, max_ra=359.5` ⇒ `center_ra=180.0°`、`width_deg=359.0°` ⇒ 打开文件后视角落在数据对跖点。同文件 `:969` 已实现环绕 ⇒ 作者知晓而此处漏 |
| E5 | 构造 `fp64_=true` 且 support 为 FP64 的产品，调 `read_support_tile` | 返回垃圾（把 f64 当 f32 重解释） | **未推翻——我错了。** 子代理证明 `aio_hips_read_tile_f32/_f64` 是同一模板、按文件实际 BITPIX 转换（`aio_hips_reader.cpp:191-204,538-566`），实为 float32 收窄（约 6e-8 相对误差）。该条**从阻断降为建议**（一致性/精度债务，非正确性缺陷） |
| E6 | 构造文件名零填充（`Dir00000/Npix00000.fits`）假设 scan/read 不对称 | 读路径因缺零填充而失败 | **未推翻——我错了。** 子代理证明 writer 用裸 `%llu` 不填充（`aio_hips_writer.cpp:232-233`），全仓无 `setw/setfill/%05`；scan/read 对填充对称。**但同一次复核挖出更重且我原先没看到的缺陷**：真正错的是 Dir/Npix 的**语义**（商+余数 vs 块起始+完整号），且有在盘文件实证与 order≥5 的精确失效阈值 ⇒ 该假设作废，**代之以 B2**（见 §2） |
| E7 | 构造 properties 含 `max_hips_order=3` 在 `hips_order=9` 之前 | `find("hips_order=")` 命中前者、误解析为 3 | **未推翻——我错了。** 子代理证明 blob 由 `std::map` 序列化、键按字典序输出（`aio_hips_reader.cpp:491`），`hips_order` 字典序先于 `max_hips_order` ⇒ 解析为 9。反倒是 **flat 分支 `:156,:158` 对键名用子串匹配**，那里才会误命中 |
| E8 | 手工核算 `decide_target_nside` 的 58.6：注释称 `360/(θ√12)=58.6/θ` | 58.6 是笔误，应为 103.92 | **推翻（但方向相反）。** `360/√12=103.92≠58.6`，注释代数确错；然 58.6 = `√(π/3)×180/π = 58.6327` 是**正确的 HEALPix 像素边长系数**（`memory.md:48` 与 `gl_renderer.cpp:1387-1388` 均给出正确推导）。⇒ **数值对、注释错**，属高危误导而非缺陷本身 |

---

## 6. 盲复算

**方法**：本人**刻意未读**任何既有产出（`审稿-RR*.md`、`审稿-R2-*.md`、`审稿-R3-*.md`、`审稿-P1-*.md`），全部结论先由本人 read 原文 + 子代理独立取证得出；成稿后才调取权威 `逐份判定-权威版.csv` 作对照。

**对照结果**：`分片清单/逐份判定-权威版.csv` 中本片 **23 份全部**为

```
tier=HUMAN, reason=默认保留（非产出面或非数据形态）
```

即：**先前判定对本片是「一律默认保留、未经实质审读」**。

**裁定：先前判定 偏松（且偏松到无效）。** 依据：

1. **偏松（最重）**：三份源码/脚本被打上「非产出面或非数据形态」而默认保留，其中却含 **B1（违反科学正本的 signal 语义）** 与 **B2（违反冻结判据的读侧契约）** 两条阻断级缺陷。「非产出面」的判断不成立 —— `hips_browser/README.md:3,8` 自述本组件「不进产品清单」，但 `:1-13` 与 `main.cpp:6-9` 显示它是一个可执行、带 CLI（`--hips/--preset/--screenshot/--view/--lod/--reset-stf/--lock-stf`）的完整程序；即便不进产品清单，**它读的正是产品集写出的 HiPS 数据**，B1/B2 一旦进入任何消费路径即为科学错误。
2. **偏松（判据层）**：`默认保留` 意味着这些文件**未经任何对抗性检查**。而本片恰恰存在本轮最忌讳的形态：B3 证明该片**没有任何一道门能转红** —— 「默认保留」的分母与「测试通过」的自证互为因果地互相掩护。
3. **未偏严**：本人在前四条子代理证伪后，主动下调/撤销了自己最初的 E5、E6、E7 三条结论（见第 5 节），并在 E8 上反向裁定「数值对、注释错」。故本片判定未因求严而膨胀。
4. **分母侧数据质量问题（非本片结论，附报）**：`README.md` 在权威 CSV 记 16 行、实测 17 行，解释了片清单 `实际行数:6038` 与实测 6039 的差 1。

---

## 7. 子代理派发记录

**派发实况（如实）**：共发起 **7 次** subagent 调用，内容为 **4 份任务书**（测试族 ×2、HiPS 后端 ×2、悬空引用 ×2、gl_renderer ×1）。其中前三份**被我误发了重复件**，故 7 次调用仅对应 4 个独立任务。**4 份任务书最终全部回执。** 全部子代理只读，禁编译/禁运行/禁 git 写/禁读 `/tmp/acsd_g08/`。逐条复核并**否决了本人 6 条结论**。

| 任务 | 状态 | 结论 | 否决/修正了本人什么 |
|---|---|---|---|
| A · 测试族（4 份测试 + Makefile/CMakeLists + 两份 `ud_grade`），派发 2 次结论一致 | ✅ 回执 ×2 | 8 条主张 **6-7 CONFIRMED / 2 SPLIT** | **否决「`get_all_data` 无非测试调用者」**——实为 5 个生产调用点（`abstract_view.cpp:93,219`、`browser_cli.cpp:1201`、`gl_renderer.cpp:1328,1617`），但因 B3 恒返空 ⇒ 应表述为「被调用但功能死亡」，比「无人调用」更糟。**修正 NDEBUG 归因**：Makefile **无** `-DNDEBUG`（断言存活），NDEBUG 只经 `CMakeLists.txt:16-18` 强制 `Release` 进入。**新增**：ctest case 缺 `<hips_root>` 必红；无 CI；`test_browser_backend` 的 4/4 实为 1 真/1 SKIP/2 空转；**两份 `ud_grade` 同时活在生产路径**（`gl_renderer.cpp:1365` 走均值、`browser_backend.cpp:548,582` 走求和）⇒ B1 由「注释错」升级为「科学契约分歧」 |
| B · `hips_browser_backend.{h,cpp}`，派发 2 次**均回执且结论一致** | ✅ 回执 ×2 | **2,3,4,5,6 CONFIRMED**；**1,7,8 部分否决** | **否决 E5**（f32/f64 同模板、按 BITPIX 转换，`read_tile_t` 对 `-64` 读 TDOUBLE 再 `(T)tmp[i]` 收窄 ⇒ 非垃圾，仅精度债）。**否决 E6**（writer 裸 `%llu` 不填充）。**否决 E7 的具体反例**（`std::map` 字典序使 `hips_order` 先于 `max_hips_order`；真正脆弱的是 flat 分支 `:156,:158` 对**键名**做子串匹配）。**新增并在第二份回执中补实证**：writer 走标准式 Dir/Npix、后端走 legacy 式且**无 standard-first 回退**（AIO 自己有 `tile_path_resolve`）；给出**在盘文件**（`Norder6/Dir10000/Npix11026.fits`）、**精确失效阈值**（order ≤ 4 重合、order ≥ 5 发散）、**生产波及面**（`hips_sky_view.cpp:188,193,364`、`browser_cli.cpp:317,801-813`）、**测试结构性失明**（`test_geometry_truth.cpp:209` 用 order 0）⇒ 升级为 B2；并把 flat 下「死 API」从 2 个扩到 **5 个 + `is_open()` 说谎**，把 `width_` 收紧为「ACSD 路径下压根不解析」 |
| C · `gl_renderer.cpp` 2062 行独立复审（C1-C9） | ✅ 回执 | **C2、C4 REFUTED；C1 部分；C3、C5-C9 CONFIRMED** | **否决「私建线程池」假设**（全文件 thread/async/QThread/OpenMP 零命中 ⇒ **通过**）；**否决「着色器状态未检查」假设**（`:374-382`/`:402-410` 均查状态并输出 info log ⇒ **通过**）；**修正本人 C1 方向**（gl_renderer 本身无第二处 nside 阈值，58.6 只以注释出现且推导正确）。**新增**：`render_sphere` 无条件 `return 0`、`get_hiss_bbox` 恒零使 `sphere_view.cpp:123` 成恒假门、`newly_loaded/reloaded` 死仪表、`mesh_viewport_h_` 死写、`:961-963` 忽略 viewport 高度 |
| D · 23 份成员全量悬空引用普查，派发 2 次结论一致 | ✅ 回执 ×2 | 枚举 **17 个**悬空目标、逐条给证否命令 | **未否决任何本人结论**，但**显著扩大了本人范围**：本片内 `docs/detail/healpix_browser_qt.md` 实为 **13 处 / 9 文件**（我原写 6）；新增 `../../astro_image_io` 簇（9 处）、`memory.md:146-148` 三条归档陈述**均为假**、`astro_image_io.dll` **被 gitignore**、`healpix_core.h` **三份副本**。**并推翻我一处「过苛」**：实测 `test_stf_engine.cpp` **存在且 4 项覆盖声明成立**、README 的 5/5 与 4/4 **计数准确** —— 本人已在 §3 表中据此改为对 README 公平表述 |

**合计否决本人 6 条**（E5、E6、E7、「无生产调用者」、「着色器未校验 / 私建线程池」，以及对 README 测试声明的过苛推定），**并把 1 条（E8）反向裁定**。本片 3 条阻断全部经子代理独立复核后保留；**B2 是在 B 代理否决我两条假设之后新发现并升级的；B1 的严重性是在 A 代理指出「两份 ud_grade 同时活在生产路径」之后再次升级的。**

---

## 8. 自证段（可复跑）

```bash
cd "/workspace/Astro CS Database"

# 0) 基线（任务书写 850a9ede，实测不同；本片结论对二者一致，见子代理 diff 核验）
git -c core.quotepath=false log -1 --format='%H %s'

# 1) 覆盖率：本片成员行数实测 = 6039（片清单声明 6038，差 1 为 README.md 末行换行口径）
python3 - <<'PY'
import yaml
d=yaml.safe_load(open('run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml',encoding='utf-8'))
s=[x for x in d['片清单'] if x['片号']=='INF-hips_browser-001'][0]
tot=sum(sum(1 for _ in open(f,'rb')) for f in s['成员文件'])
print(f"份数={len(s['成员文件'])} 清单行数={s['实际行数']} 实测行数={tot}")
PY

# 2) B1：求和 vs 科学正本的面亮度语义
sed -n '619,630p' lib/infrastructure/hips_browser/healpix_browser_qt/core/browser_backend.cpp
sed -n '124,124p' lib/infrastructure/hips_browser/healpix_browser_qt/core/browser_backend.h
sed -n '466p;488,490p' docs/science/DATA_SEMANTICS.md      # signal = ADU/sr = flux_sum/covered_area

# 3) B2：Dir/Npix legacy 式 vs 冻结判据 + writer 实际产出
sed -n '274,282p;313,316p' lib/infrastructure/hips_browser/healpix_browser_qt/core/hips_browser_backend.cpp
sed -n '229,235p' lib/infrastructure/aio/src/hips/aio_hips_writer.cpp   # writer 走标准式（5 个调用点）
sed -n '49,67p'  lib/infrastructure/aio/src/hips/aio_hips_reader.cpp    # legacy 路径 + standard-first 解析器
grep -n "Dir{" docs/science/DATA_SEMANTICS.md                          # REC-HIPS-1.0 §4.1 判据
# 阈值验算：order≤4 最大 ipix=12*4^4-1=3071<10000（巧合重合）；order≥5 最大 ipix=12*4^5-1=12287（必发散）
python3 -c "print('o4',12*4**4-1,'o5',12*4**5-1); print('ghost',10000*10000+11026)"
# 在盘实证（若产品尚在）：
find run -path '*hips/signal/Norder6/Dir*/Npix*.fits' 2>/dev/null | head -3

# 4) B3-a：测试空指针解引用（pixel 恒 nullptr）
sed -n '72,79p' lib/infrastructure/hips_browser/healpix_browser_qt/tests/test_browser_backend.cpp
sed -n '636,637p;655p' lib/infrastructure/hips_browser/healpix_browser_qt/core/browser_backend.cpp
sed -n '50,51p' lib/infrastructure/hips_browser/healpix_browser_qt/core/browser_backend.h

# 5) B3-b：SKIP 不失败 + 硬编码 f:/ 路径
sed -n '15,18p;22,26p' lib/infrastructure/hips_browser/healpix_browser_qt/tests/test_browser_backend.cpp

# 6) B3-c：CMake 缺省 Release → NDEBUG；ctest 缺参；无 CI
sed -n '16,18p;126p;164p' lib/infrastructure/hips_browser/healpix_browser_qt/CMakeLists.txt
ls -d .github 2>&1                      # 无 CI
grep -rn "healpix_browser_qt" --include=CMakeLists.txt . | grep -v "^./lib/infrastructure/hips_browser/healpix_browser_qt/"   # 仓根从不 add_subdirectory

# 7) B3-d：G3 夹具生成器 NameError（Path 无 import）
sed -n '10,19p;34p' lib/infrastructure/hips_browser/healpix_browser_qt/tests/gen_hips_browser_test.py
grep -c pathlib lib/infrastructure/hips_browser/healpix_browser_qt/tests/gen_hips_browser_test.py   # = 0

# 8) 死代码：build_hiss_leaf_index 零调用者 / get_all_data 5 个生产调用点
grep -rn "build_hiss_leaf_index" --include=*.cpp --include=*.h lib/
grep -rn "get_all_data" --include=*.cpp lib/

# 9) gl_renderer 自愈锚：:1390 用 use_nside 报健康，:1444 用 all.nside 建几何
sed -n '1386,1391p;1442,1444p' lib/infrastructure/hips_browser/healpix_browser_qt/core/gl_renderer.cpp

# 10) 悬空引用（逐条判不存在）
for p in docs/detail/healpix_browser_qt.md docs/superpowers \
         lib/infrastructure/hips_browser/archive \
         lib/infrastructure/hips_browser/healpix_browser_qt/widgets/archive \
         run/temp/p2_v11 eng/cmake/ARCH-001-migration-manifest.md; do
  [ -e "$p" ] && echo "EXISTS   $p" || echo "DANGLING $p"
done

# 11) 私建线程池 / 着色器状态：本片两项**通过**的证否命令
grep -nE "std::thread|QThread|std::async|omp |QThreadPool" lib/infrastructure/hips_browser/healpix_browser_qt/core/gl_renderer.cpp   # 零命中
sed -n '374,382p;402,410p' lib/infrastructure/hips_browser/healpix_browser_qt/core/gl_renderer.cpp

# 12) oracle 主实现不可达 + 输出目录不存在
sed -n '249,250p;155p;173,176p' lib/infrastructure/hips_browser/healpix_browser_qt/tools/hips_tile_oracle.py

# 13) 盲复算对照：本片 23 份先前的统一判定
python3 - <<'PY'
import csv,io
rows=list(csv.DictReader(io.open('run/GOVERN-08/审核包-R2/分片清单/逐份判定-权威版.csv',encoding='utf-8-sig')))
t=[r for r in rows if r['slice']=='INF-hips_browser-001']
print(f"本片成员={len(t)}  统一判定={set((r['tier'],r['reason']) for r in t)}")
PY
```
