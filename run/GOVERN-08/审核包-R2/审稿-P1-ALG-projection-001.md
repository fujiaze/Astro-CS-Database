# 审稿-P1 · ALG-projection-001

> G08-05 对抗审稿 第 1 遍 · 生产源码片（`lib/algorithms/projection/`）
> 基线 HEAD = `850a9ede` · 负责人裁定口径：一遍 = 对该片材料的一次完整重读
> 本片判定：**需修**（无「必须整片停摆」级阻断，但存在 6 条阻断级缺陷，其中 3 条可由构造反例直接坐实）

---

## 1 读完了吗

| 口径 | 值 |
|---|---|
| 成员份数（权威片清单） | 17 |
| 实际读完份数 | **17 / 17（100%）** |
| 成员总行数（片清单 `实际行数`） | 4899 |
| 我实测总行数（`wc -l` 逐文件） | **4899（与清单逐位相符）** |
| 实际读入行数 | **4899** |
| 覆盖率 | **100%（0 行未读）** |

### 逐份行数（实测 `wc -l`，与权威清单核对一致）

| 文件 | 行数 |
|---|---|
| `p3_proj.cpp` | 781 |
| `tests/p3wcs/p3_projection_registry_test.cpp` | 719 |
| `p3_wcs.cpp` | 593 |
| `tests/p3wcs/p3_wcs_test.cpp` | 474 |
| `p3_projection.cpp` | 407 |
| `p3_wcs.h` | 373 |
| `p3_projection_registry.h` | 312 |
| `p3_proj.h` | 266 |
| `tests/p3wcs/test_p3_wcs.py` | 169 |
| `memory.md` | 168 |
| `module.yaml` | 152 |
| `README.md` | 133 |
| `p3_projection.h` | 116 |
| `tests/p3wcs/p3_projection_unsupported_cli.py` | 93 |
| `tests/p3wcs/p3_wcs_main.cpp` | 65 |
| `CMakeLists.txt` | 42 |
| `tests/p3wcs/CMakeLists.txt` | 36 |
| **合计** | **4899** |

**未读完的：无。** 全部 17 份、4899 行逐行读完，无抽样、无跳读、无「顺便看别的片」。

### 口径说明
- 「读」= `read` 工具全文返回（非 grep 片段、非摘要）。两个 CMakeLists 与 `p3_wcs_main.cpp` 分别经 `read` / `cat` 取全文。
- 行数口径：`wc -l`（换行计数），与片清单 `实际行数: 4899` 一致。
- 本人**未运行任何仓内二进制、未编译、未跑 ctest/pytest**。所有数值结论来自（a）我自己对 `p3_wcs.{h,cpp}` 公式的 1:1 转写重算（脚本落 `/tmp/p3_recheck/`，非仓内），（b）子代理的代数证明，两者在报告中逐条标注来源并互相复核。

---

## 2 本片判定与最重 3 条

**判定：需修。**

本片的 TAN 数学核心是**正确**的（我逐式重算验证了 gnomonic 正反映射，与 Calabretta & Greisen (2002) 一致；见 §5 CE-0）。问题不在公式，而在**判据**：本模块用于证明自己正确的机制，绝大多数是**自洽式断言**——被检量与期望量出自同一个定义式，因此无论实现对错都必然通过。

### 最重 3 条

**① 阻断 · 适用域门放过一个「自己的逆变换会拒绝」的 WCS**
`p3_wcs.cpp:105` 只守 `|centre_dec|≤85`，四角守卫 `:149-156` 只验半球；**从不检查四角映射后的 `|dec|≤85`**。而 `p3_wcs_world2pix:545` 对 `|dec|>85` 硬拒。
反例 CE-1（`/tmp/p3_recheck/probe1.py`，我的独立重算）：`|CRVAL2|=85°`、FOV 5°–19.9°（声明上界 20° 以内）时，`p3_wcs_make` 一律返回 `P3_WCS_OK`，帧内像素 `dec` 最大到 **86.32°**，喂回 `world2pix` 得 **status=1（拒绝）**，同时密集域扫描有 **29.8%–47.0% 的采样点在 `cpp:367` 被静默丢弃**，而门报 **GREEN**。
⇒ 模块会输出一张自己的逆函数读不回来的 FITS 卡，而所有门都看不见。

**② 阻断 · `NaN` 使往返门恒绿，且报告「误差 = 0.000000 px」**
`p3_wcs.cpp:105` 只守卫 `centre_dec_deg`，`:112` 直接抄 `centre_ra_deg`（**无 `isfinite`**——而同目录的 v6 内核 `p3_proj.cpp:461-462` 有）。`cpp:552` 的 `if (denom <= 0.0)` 对 `NaN` 为假，半球守卫落空；`cpp:372` 的 `if (e > worst)` 对 `NaN` 亦为假，**每个 NaN 采样点都被丢弃**。
反例 CE-2：`p3_wcs_make(NaN, 2.0, 0.001, 64,64,…)` → status 0；`crval_ra=NaN`；密集域 4096 点全计入、实测往返误差 **恰为 0.0**；`cpp:500` 判绿。
⇒ 这是**恒绿门**，比恒红更隐蔽：门亮着，缺陷在里面。

**③ 阻断 · 往返门对整个线性半边（WCS 的 CRPIX / 桥接 / CD / PA）在代数上完全失明**
`pix2world = N∘L`、`world2pix = L⁻¹∘N⁻¹` 用**同一个** `d` ⇒ 复合 = `I`，对**任何可逆的 L** 成立。`+1`/`−1`、`crpix`、`CD` 逐位相消。唯一能验它的两道门恰好是同义反复：
- `cpp:451` 手性 `det<0`：`cpp:139` `sgn_y=-sgn_x` ⇒ `det=-s²<0` **恒成立**；
- `cpp:466` CRPIX：`cpp:114` 用 `(W+1)/2.0` 算，`cpp:464` 用同一表达式比 ⇒ **恒等**。
⇒ 真正该防的 `CD[1][0]` 的 PA 符号写反（`cpp:145`，正是注释 `cpp:133-134` 记载曾出 P0 bug 的那一行）**没有任何门能发现**：`det` 变成 `-s²cos(2PA)`，PA=30° 时仍 <0 通过手性，往返又完全不变。

---

## 3 逐文件清单

| 文件 | 读了什么 | 看到什么 | 判定（`文件:行`） |
|---|---|---|---|
| **p3_wcs.h** (373) | 全文 | ① `h:64-90` 适用域声明表与其散文注释（1e-8/1e-6/0.9/128）；② `h:165-350` export-crop 几何（窗口、天球矩形、外扩包围盒）；③ `h:352-369` 裁剪帧 WCS 精确限制 | **需修**。`h:199` `P3_CROP_HEMISPHERE=2` **全模块无任何返回点**（`h:254-257` 的 `fail` 只产 `P3_CROP_PARAM`）＝死错误码；`h:274` 第三份 `85.0` 字面量（与 `cpp:20`、注释宣称的「单一条件」矛盾，因 `kMaxAbsDec` 在匿名命名空间、头内 inline 看不见）；`h:205` `kP3CropEdgeSamples=257` 的推导基准是 2° 画幅，而声明域是 20°（见 §4-M14，我**下调**了子代理在此处的指控）；`h:323-326` `floor()` 结果未做范围检查即 `static_cast<long>`；`h:359` `*out=*frame` 浅拷贝 `const char*` |
| **p3_wcs.cpp** (593) | 全文 | TAN 正反映射、请求校验、`make` tmp-then-commit、适用域门、双层往返门、密集域扫描、FITS 关键词 | **阻断**。CE-1（`:105/:112/:149-156` vs `:545`）、CE-2（`:112/:552/:372/:500`）、B3（`:451/:464` 同义反复 + `:500` 线性半边失明）、CE-4（`:526` 半球守卫用平面半径 `r≥π/2` ⇒ 角半径 57.52°，FOV≥115.04° 即误拒，而 `world2pix` 允许到 90°，正反守卫不对称）；`:291` `margin` 在 `envelope_px==0` 时给 `0.0`（看着像绿实为「算不出来」）；`:93`/`:161` 的 `perr`/`aerr` 构造后**从未被读**（OUT_OF_DOMAIN 的解释串在此丢弃）；`:254` 包络函数对非法输入返回 `0.0`，而 `0.0 ≤ 1e-6` 会让 `p3_wcs_roundtrip_gate:284` 判成 GLOBAL 门——**非法哨兵值冒充「零误差」通过保守性检验** |
| **p3_wcs.cpp** `:212-222` 常量表 | 冻结值来源 | `1e-8 / 1e-6 / 0.9 / 128.0` | **建议**。`128` 是对实测 max 78 的 1.64× 标定，注释（`cpp:197-208`）引用的证据 `evidence/kernel_roundtrip.json`、`run/GATE-DERIVE-01/REPORT.md`、`run/GATE-WCS-01` **三者均不存在**（`run/*` 被 gitignore，仅 `.gitkeep` 未忽略）⇒ 1e-8 的全部推导基础不可复现 |
| **p3_projection.h** (116) | 全文 | legacy v1 registry 的类型与 API 面，RETIRED 头注与 D1–D4 偏差表 | **须修**。`h:9` 冻结「偏差集合只减不增」，但 `p3_projection.cpp:183` 的 SIN 逆向 `sqrt(1-s²)` 灾难性消去**未登记**为第 5 条偏差；`h:43` 指向 `lib/phase3_session` 的 `P3WcsStatus`——该类型定义在 `p3_wcs.h:25`，路径悬空；`h:90-93` 宣称自检「行数=4」而实现（`cpp:303`）把 4 写成循环上界，从未与数组长度比对 |
| **p3_projection.cpp** (407) | 全文 | legacy v1 四投影冻结实现、registry 表、`make`、自检 | **须修**。`cpp:300-316` `p3_projection_registry_selfcheck` 的 8 条检查**全部把编译期常量与自身字面量比对**（`kP3ProjectionRegistryVersion != 1`（对 `h:33`）、`!kP3ProjectionRegistryRetired`（对 `h:34`）、`code==nullptr`、字符串非空、函数指针非空、`id==i`（对 `cpp:269-276` 的枚举字面量））⇒ **该函数恒返回 0，是一组永真门**；`:275` AIT `max_fov_deg=360.0` 是本仓 `p3_projection_registry_test.cpp:351-353` 已判为错误并在 v6 改成 180.0 的值；`:333` `parity=nullptr` 静默默认 `east_left`（头 `:96` 的校验序未披露）；`:216/255` `if (dra > 180.0)` 在 `:215` 归一化后恒为假＝死代码（`:253-255` AIT 同）；本文件 `isfinite` 出现 **0 次**（v6 内核 `p3_proj.cpp` 有 10 处） |
| **p3_projection_registry.h** (312) | 全文 | 产品声明注册表（冻结集 F / 声明集 D / 实现集 I）、请求面门、黑盒探针、注册表自检 | **阻断（B6）**。`:204` 注释白纸黑字「**探针不查声明表/实现表 —— …（判据独立性）**」，而 `:206` 的第一道门就是 `p3_wcs_validate_request`，它在 `p3_wcs.cpp:68` 调 `p3_proj_declare`，后者 `:167` 读 `kDeclared={"TAN"}`（`:122`）⇒ **对任何 ∉D 的码，探针在 `:209` 就返回，连内核都没跑** ⇒ `R ⊆ D` 恒成立，「D==I==R」的机器判据**在结构上不可能失败**；`:272-308` 自检里**没有 `I⊆D`，也没有 R**，故 `:20`「实现未声明 = 隐藏能力（禁止）」**无任何执行点**；`:196-197` 注释「本函数不硬编码数值」与 `:216` 的 `0.0005 / 64 / 64` 字面量自相矛盾；`:249/:263` `std::to_string(double)` 用 `%f`，`1e-8` 与 `2.4e-9` 一律印成 `0.000000`，探针的全部诊断量为零信息 |
| **p3_proj.h** (266) | 全文 | v6（kProjectionRegistryVersion=3）内核 API 面、`Plan`、逐像素立体角 Ω、R/S 归一 | **须修**。`h:101` 指向 `eng/tests/p3wcs/p3_projection_registry_test.cpp`——**该目录不存在**；`h:14/:20` 「`p3_proj.cpp:195 sin_world2pix`」——195 行在 `tan_world2pix` 内，被指表达式在 **:229**；`h:15` 「投影中心 theta→pi/2 时灾难性消去」——`1-s²` 在 `|s|→1` 即 **θ→0（投影中心）** 时才消去，θ=π/2 时 `s=0`、表达式为 `1-0=1`，**机制真但奇异集写反**（子代理独立发现，我确认）；`h:162` 声明 `fov_x_deg` 是「RA unwrap span」而 `cpp:657` 存的是**圆形** span，`raw_span` 算完丢弃；`h:169` `singularity_free` 承诺「离奇点有正 margin」而 `cpp:661` 直接 `= domain_valid`（从未算过奇点 margin）；`h:244` 「kParam 且输出零初始化」在循环中途失败路径（`cpp:722/737/753`）不成立 |
| **p3_proj.cpp** (781) | 全文 | v6 四投影内核、Ω 盈余法与微分法、`plan`、R/S 行/列归一 | **阻断（B4 CE-9）**。`cpp:186-190` 的 `tan_pix2world` **没有任何 dec 守卫**，而 `:196` 的 `tan_world2pix` 拒绝 `|dec|>85` ⇒ 反例 CE-9：`make(kTAN,150,−85,0.2,401,401)` 合法，其正向映射 `dec` 低至 **−89.40°**，采样到 42 个越界像素喂回即 `kParam`——**与 CE-1 同病，v6 内核同样中招**；`:628` `ra_samples[nra++] = ra` 在 `pix2world` 可能非 OK 而**不写出** `ra` 的情况下无条件读 ⇒ 不确定值读；`:629-631/:639-642` 先按 `st==kOk` 筛子集再取 dec/Ω 极值，**被筛掉的恰是越域即最差的那个**（而 `:636` 的 `margin_min` 又用相反口径，同一循环内两种策略）；`:660` `margin_min > 0.0` 使 SIN `ρ=1`、AIT `A=1` 这些**合法边界**判 `domain_valid=false`（与上游 §15.3「A=1 边界合法」矛盾）；`:664-668` 全部 Ω 采样失败时返回 `kOk` 且三项 Ω 全 0（`0/0` 呈现为 `0`，读起来像「恒定面积」）；`:613` 哨兵 `dec_min=90 / dec_max=−90` 在全失败路径外泄为 `fov_y_deg=−180.0`；`:426/:427` 又是 `constexpr` 与自身字面量比对＝永真门；`:66-69` 行锚在 `cpp` 里根本没有对应内容 |
| **tests/p3wcs/p3_wcs_test.cpp** (474) | 全文 | STD-F1 九宫格桥接锁 + 独立参考 + 负向注入 | **阻断（B5 的孪生）**。`:106-131` `fits_tan_forward_ref` 确是向量基路径（与生产球面三角式不同），但 `ref_pixel_offset:160-172` 把参考正向的结果喂给**生产逆变换** `p3_wcs_world2pix`，比较 `xr−x0`；代数上 `x_r = x0 + P − 1`（P=pixel_origin），故 `max_ref_px` **恒为 0**（P=1 时），`:310` 只能证明 `CD⁻¹CD=I` 与 `±1` 相消——**与 B3 同一条代数恒等式**；`:314-319` 六条断言（`max_nobridge_px==√2`、`min/max_*_axis==1.0`）是该线性代数的不动点，**可证恒真**；`:133-143` 的 `STD_F1_BRIDGE_FAULT` 注入机制本身有效（置 `nobridge` 会让 `:310` 转红），但**全仓无任何 CMake/CI/脚本设置该变量** ⇒ CI 从不武装这个负例；`:325-326` `CHECK(d.crpix_x == (kFrameW+1)/2.0)` 与 `p3_wcs.cpp:114` 同式同参 ⇒ **恒等**；`:324` `CHECK(det<0)`、`:327-328` `cd[0][0]±` 均由构造恒真；`:366` `fabs(cd[0][0]+0.0001389)<1e-9` 的期望值就是喂进去的输入字面量 |
| **tests/p3wcs/p3_projection_registry_test.cpp** (719) | 全文 | C1–C9 九组判据 + `--self-test` 负例 + `--matrix` 证据模式 | **阻断（自洽式断言集中地）**。`:557-566` 的 **C9②** 注释写「包络实现 == 独立复算式（**测试侧独立算, 不调生产函数**）」，但 `env_ref` 用的是与 `p3_wcs.cpp:263` **逐字相同**的公式，且 `c_env` 读自**同一个** `ap` 结构、`u=1.110223e-16` 与 `arcsec_per_rad=206264.806…` 与 `cpp:225/227` 同一字面量、`fov` 干脆调用**生产函数** `p3_wcs_fov_deg` ⇒ **被检量与期望量同式**，全域保守门的全部保守性论证在此**零独立证据**；`:503-509` 「存在 `nine<tol<dense` 的门值带」用 `tol_between=0.5*(nine+dense)`，两个条件都退化为 `nine ≤ dense`，「存在性」是实数的平凡性质；`:245-246` 「注入 1px 桥接偏差: 判据必红（非退化）」没有注入任何缺陷，只是把同一个 `(x,y)` 拿去和 `33.0` 比 ⇒ **恒真门**；`:172-174`/`:615-616` 「探针未实现码跑不通」因 §B6 的循环性而**恒真**；`:140`/`:313` 的 `n==8`/`nv==4` 与函数内 `*count=8`/`:407` 同为字面量；`:346-363` C5b 的 `!=360.0 && !=162.0569` 是对当前值的恒真回归锁；`:647-695` `--matrix`（自称用于裁决「文档说 1、代码说 4」）**未挂任何 ctest** |
| **tests/p3wcs/test_p3_wcs.py** (169) | 全文 | 6 个用例：roundtrip/RA wrap/pole guard/hemisphere/keywords/边界 | **阻断（B5）**。`:85` `xi, eta = gnomonic_vector(ra, dec, ra0, dec0)` 算出来后**立刻被丢弃**——`:86-89` 传给 `ungnomonic_vector` 的是由 `cd`/`crpix` 重算的平面坐标；即**唯一真正独立的向量法实现是死代码**。而 `:28-39` 的 `ungnomonic_vector` 用的 `atan2(1.0,r)`、`atan2(-xi,eta)`、`asin(st*sd+ct*cdv*cos(phi))`、`atan2(-ct*sin(phi), cdv*st-sd*ct*cos(phi))` 与 `p3_wcs.cpp:527-534` **逐字相同** ⇒ `:91-93` 的「独立参考一致」断言是**同式自洽断言**，无法发现 gnomonic 三角学里的任何错误。文档串 `:2` 与注释 `:84`「独立参考: 向量法」**均不成立**。另 `:47-52` 该用例在运行时 `g++` 编译生产源码；`:99-100` 的 roundtrip 断言（测试名自称「恒等」）本身就是恒等式 |
| **tests/p3wcs/p3_wcs_main.cpp** (65) | 全文 | CLI 探针（make/p2w/w2p/kw 四模式） | **须修**。`:21-22` `auto d5=[](double v){…}; (void)d5;` ——**一个定义后立刻被 `(void)` 丢弃的正确性谓词**（明显是某次检查的残留，被显式禁用）；`:1` 头注仍写 `eng/tests/backend/p3_wcs_main.cpp`（该文件不存在，真身在同目录）；**本文件未被任何 CMakeLists 编入**（`tests/p3wcs/CMakeLists.txt:8` 只编 `p3_wcs_test.cpp`）⇒ 65 行死代码 |
| **tests/p3wcs/p3_projection_unsupported_cli.py** (93) | 全文 | 端到端负例：7 个未实现码 + 1 未知码 ⇒ rc≠0 + 三段文本 | **须修（恒真门）**。N2 对照臂 `:77-82` **只检查输出里不含 `unsupported` 字样，完全不检查 `rc_tan`**，而 `:82` 的打印文案自己都写「后续失败与投影无关」；HiPS 夹具 `:34-37` 只有一个 `properties`（`hips_order = 3`），无任何像素数据 ⇒ TAN 这次运行几乎必然因无关原因失败 ⇒ **「证明拒绝由投影码引起」的对照臂在几乎任何情形下都通过**，是恒真门 |
| **CMakeLists.txt** (42) | 全文 | 模块 target 声明 | **建议**。`:11` 把 `p3_projection.cpp / p3_proj.cpp` 的 target 声明推给「P3-002 线」；`:41` 注释「模块 CMakeLists 求值早于根图注册 p3_wcs_test（`NOT TARGET` 恒真）」——**自陈恒真**；本片三个 `.cpp` 里 `std::thread/omp/mutex` 计数均为 **0**（模块不私建线程池，符合规范） |
| **tests/p3wcs/CMakeLists.txt** (36) | 全文 | 4 个 ctest 注册点 | **须修（孤儿源）**。`:8` 只编 `p3_wcs_test.cpp` ⇒ **本片的 `p3_wcs_main.cpp` 无 target**；`:16-19` 编 `p3_projection_registry_test.cpp` + `p3_proj.cpp`；全片只挂了 `p3_projection_unsupported_cli.py` ⇒ **本片的 `test_p3_wcs.py`（169 行）不被任何 ctest 执行** |
| **README.md** (133) | 全文 | 模块身份、合同链、生产源、偏差登记、产品声明面 | **须修（悬空引用集中地）**。`:130-133` 的「可执行面」整段指向 `eng/tests/p3wcs/…`——**该目录不存在**（真身 `lib/algorithms/projection/tests/p3wcs/`）；`:82` `eng/tests/p3wcs/p3_wcs_main.cpp` 同；`:55` `eng/tests/unit/p3_wcs_test.cpp`（不存在，且注「90 行」而真身 474 行）+ `eng/tests/backend/p3_wcs_main.cpp`（不存在）；`:128-129` 只写「往返 <1e-8 px」，**完全没提 1e-6 全域保守门与超域不判红**，与 `p3_wcs.h:73-78` 不一致；`:3-26` 是 24 行开头元信息块且含 4 个日期与 6 个任务流水号，违反 AGENTS.md §5「开头无元信息块；正文无日期、版本号、任务流水编号」 |
| **memory.md** (168) | 全文 | 冻结时的行号锚、偏差登记、验收记录 | **须修**。`:63-65`/`:96-99`/`:66-99` 通篇以 `lib/phase3_session/p3_wcs.{h,cpp}`（165 行版本）与 `eng/tests/…` 为锚；这些路径**全部不存在**（`lib/phase3_session/` 目录仍在，但已无 `p3_wcs.*`；三个 `eng/tests/*` 路径均缺）⇒ 开头 `:3-8` 声称「行号锚按新址复测」，**实际未复测**；`:121-168` 整节是带日期/控制包号/commit BASE 的历史流水叙事，直接违反 AGENTS.md §5 |
| **module.yaml** (152) | 全文 | 模块清单（schema/id/ports/contracts/source_symbols/threading） | **须修**。`:152` `known_defects: []` —— **与本模块自带的缺陷登记块直接冲突**：`p3_proj.h:14-22`、`p3_proj.cpp:14-22`、`p3_projection_registry.h:91-104` 都登记了一个「往返误差无上界」的**已知在案缺陷**，清单却写零缺陷；`:136-140` 的 `source_symbols` **仍把 5 个 legacy v1（RETIRED，头注写明「禁止新消费方引用」）符号列为在役符号**，与 `:57-59` 的退场登记自相矛盾；同时真正的 API 面（`p3_wcs_validate_request`/`p3_wcs_check_applicability`/`p3_wcs_roundtrip_*`/`p3_crop_*`/`p3_proj_*`）**一个都不在**；`:11` 把未编入任何 target 的 `p3_wcs_main.cpp` 称作「执行面探针」；`:143` 的 `TEST-P3-PROJ-REG-001` 在 README 的合同表（`:47-56`）中缺席 |

---

## 4 发现清单

### 4.1 阻断（6）

| # | 位置 | 缺陷 | 证据 |
|---|---|---|---|
| **B1** | `p3_wcs.cpp:105,112,149-156` vs `:545`；`p3_wcs.cpp:367` | `make` 从不检查角点映射后的 `\|dec\|≤85`，可造出**逆变换硬拒**的 WCS；且往返扫描把这类点静默丢弃 | CE-1（我的重算）：FOV 5–19.9°、\|CRVAL2\|=85° 下 `make`=OK，角点 `dec` 达 86.32°，`world2pix`=拒绝，**29.8–47.0% 采样点被丢弃**，门报 GREEN |
| **B2** | `p3_wcs.cpp:112,552,372,500` | `centre_ra_deg` 无 `isfinite`；`NaN` 击穿半球守卫与极值累加器 ⇒ 往返门**恒绿并报告「0.000000 px」** | CE-2；对照 `p3_proj.cpp:461-462` 有守卫、`p3_wcs.cpp:249` 也有守卫——**同模块内部不一致** |
| **B3** | `p3_wcs.cpp:451,464,500`（+`:139`,`:114`） | 往返门对线性半边（CRPIX/桥接/CD/PA）代数失明；本该防它的两道门是同义反复 ⇒ `CD[1][0]` 的 PA 符号写反无人能发现 | 代数：`N∘L∘L⁻¹∘N⁻¹ = I` 对任意可逆 L；`det` 写反后 = `-s²cos(2PA)`，PA=30° 仍 <0 |
| **B4** | `p3_proj.cpp:186-190` vs `:196`（同 `:211` vs `:221`） | v6 内核存在与 B1 同类的正/逆 `\|dec\|` 守卫不对称 | CE-9：`make(kTAN,150,−85,0.2,401,401)` 合法，正向达 `dec=−89.40°`，回灌即 `kParam` |
| **B5** | `test_p3_wcs.py:85`（+`:28-39`,`:91-93`） | 唯一的真独立向量参考 `gnomonic_vector` 算出即弃；实际断言比的是生产公式的 Python 重打 ⇒ **自洽式断言** | 逐行比对 `:31-37` 与 `p3_wcs.cpp:527-534` 逐字相同 |
| **B6** | `p3_projection_registry.h:204` vs `:206`→`p3_wcs.cpp:68`→`:167,122` | 「判据独立性」不成立：`R ⊆ D` 恒成立，`D==I==R` 结构上不可能失败；且 `selfcheck` 连 `I⊆D` 都不查 | 子代理独立发现并给出证明，我复核调用链成立 |

### 4.2 须修（18）

| # | 位置 | 缺陷 |
|---|---|---|
| M1 | `p3_projection_registry_test.cpp:557-566` | **C9②「独立复算包络」是同式自洽断言**——全域保守门的保守性论证零独立证据 |
| M2 | `p3_wcs.cpp:512`（+`p3_projection.cpp:300-316`、`p3_proj.cpp:426-427`） | 三处 `registry_selfcheck` 的版本/退役/行数检查全部拿编译期常量与自身字面量比 ⇒ **永真门**；legacy 那份恒返回 0 |
| M3 | `p3_projection.cpp:275` | AIT `max_fov_deg=360.0` 是本仓 `p3_projection_registry_test.cpp:351-353` 判错、v6 已改成 180.0 的值，在 v1 里原样再冻 |
| M4 | `p3_projection.h:9` vs `p3_projection.cpp:183` | 「偏差集合只减不增」被违反：v1 SIN 逆向的灾难性消去未登记为第 5 条 |
| M5 | `tests/p3wcs/CMakeLists.txt:8` | `p3_wcs_main.cpp`（65 行）未被任何 target 编入；`test_p3_wcs.py`（169 行）不被任何 ctest 执行 |
| M6 | `p3_wcs_main.cpp:21-22` | 一个被 `(void)` 显式禁用的死谓词 |
| M7 | `p3_projection_unsupported_cli.py:77-82` | N2 对照臂不检查 `rc_tan` ⇒ **恒真门** |
| M8 | `p3_wcs.cpp:254` + `:284` | 包络函数对非法输入返回哨兵 `0.0`，而 `0.0 ≤ 1e-6` 会让门判成 GLOBAL —— **非法哨兵冒充「零误差」通过保守性检验** |
| M9 | `p3_wcs.cpp:476-491` | `OUT_OF_DOMAIN` 时 `check_applicability` 返回 `P3_WCS_OK`，与「全部适用域通过」对调用方不可区分；唯一机器可读证据（`:479-489` 的 `why`）在 `:161-163` 被构造后丢弃 |
| M10 | `p3_wcs.h:199` | `P3_CROP_HEMISPHERE=2` 全模块零返回点（死错误码）；`h:290-293` 还把所有非 OK 状态洗成一个布尔 |
| M11 | `p3_wcs.cpp:20` / `p3_wcs.h:274` / `p3_projection.cpp:40` / `p3_proj.cpp:52` | 「85° 单一条件」实为 4 份字面量；头注释 `h:273`「与 kMaxAbsDec 同一条件，不另设口径」因匿名命名空间不可见而**在结构上不可能成立** |
| M12 | `p3_proj.cpp:628` | `ra` 在 `pix2world` 可能非 OK 且不写出时被无条件读 ⇒ 不确定值读；`:637` 的 `om` 却正确初始化，对照明显 |
| M13 | `p3_proj.cpp:629-631,639-642` vs `:636` | dec/Ω 先筛 `st==kOk` 子集再取极值（被筛者恰是越域最差者），而 `margin_min` 在同一循环里用**相反**口径 |
| M14 | `p3_wcs.h:204-205` vs `p3_wcs.cpp:215` | `kP3CropEdgeSamples=257` 的推导基准是 2° 画幅，声明域却是 FOV≤20°。**实测**（CE-7）：域边处每子边矢高 5.85e-8 px，仅比 1° 基准大 101.5×，仍比 1 px 小 4 个量级 ⇒ **功能无碍，降为文档缺陷**（子代理称此处 ~2e-3 px「2× 超标」，见 §7 否决记录） |
| M15 | `p3_proj.cpp:107,155` vs `:168-169` | 正向 `asin` 入参**不夹紧**，而同一物理量的逆向孪生 `:168-169` 夹紧 ⇒ 是遗漏而非策略。（`asin` 溢出**发生率**我未能复现，见 §7） |
| M16 | 悬空引用 10 处 | `p3_wcs.cpp:44`→`eng/tests/unit/p3_wcs_test.cpp`(缺)；`:209`→`evidence/kernel_roundtrip.json`(缺)；`:211`/`:174`→`run/GATE-DERIVE-01/REPORT.md`、`run/GATE-WCS-01`(gitignore)；`p3_proj.h:101`→`eng/tests/p3wcs/`(缺)；`p3_proj.h:31`/`p3_proj.cpp:31`→`ENGINEERING_SPEC.md`(HEAD 无)；`p3_proj.h:29`→`eng/ci/checks.json`(缺)；`README.md:55,82,130-133`；`memory.md:63-65,96-99`；`module.yaml:11`(指向未编入 target 的文件) |
| M17 | `module.yaml:152` vs `p3_proj.h:14-22` | `known_defects: []` 与模块自带的在案缺陷登记块冲突；`:136-140` 把 5 个 RETIRED v1 符号列为在役 `source_symbols` |
| M18 | `p3_wcs.h:74-75` / `p3_wcs.cpp:188-189` | 散文宣称全域门保守性下界 `2.93e-3″/px`，实现（`cpp:263`，含 `sec²Δ`）给出 `3.02e-3″/px`（CE-5）⇒ 散文值是**省略了 sec²Δ 因子**的旧数。代码更保守，**文档偏松 3%** |

### 4.3 建议（10）

1. `p3_proj.h:162` 声明 `fov_x_deg` 是 unwrap span，`cpp:657` 存 circular span，`raw_span` 算完丢弃（`p3_proj.h:169` `singularity_free` 同为 `domain_valid` 的别名，从未算过奇点 margin）。
2. `p3_proj.cpp:664-668` 全部 Ω 采样失败时返回 `kOk` 且三项 Ω 全 0；`:613` 哨兵在全失败路径外泄为 `fov_y_deg=−180.0`。
3. `p3_proj.cpp:660` `margin_min>0.0` 使 SIN `ρ=1`/AIT `A=1` 这些合法边界判 `domain_valid=false`（与上游 §15.3 矛盾）。
4. `p3_proj.cpp:717/722,732/737,747/753` 中途失败留下部分输出，违反 `p3_proj.h:244` 的「输出零初始化」。
5. `p3_proj.cpp:557` 中心差分步长 `h=1e-3` px 无出处；`:659` 的 `1e-9` 角度域 epsilon 无出处；`:91` 的 `1e-300` 是绝对阈值而量纲是 (deg/px)²。
6. `p3_proj.cpp:61-64` `normalize_ra` 对 `ra=−1e-17` 返回恰好 `360.0`，违反冻结的 `[0,360)`。
7. `p3_projection.cpp:216,255` 的 `if (dra > 180.0)` 在前一行归一化后恒为假（死代码）；`:333` `parity=nullptr` 静默默认。
8. `p3_wcs.cpp:573-589` FITS 卡片不补齐到 80 字节（头 `:61` 承诺「每行 80 字节内」）；`%.10f` 写入 `buf[128]` 无溢出检查。
9. `p3_projection_registry.h:115-117` `p3_proj_frozen_list()` 全仓零调用者且是第三份手抄冻结集；`:137` 「实现集 I（生产路径**真正有内核**的码）」措辞与 SIN/CAR/AIT 有 v6 内核的事实矛盾。
10. README/memory/module.yaml 三件套含大量日期、任务流水号、控制包名与开头元信息块，违反 AGENTS.md §5（本条为文档规范，不影响运行正确性）。

---

## 5 我主动构造的反例

脚本全部落 `/tmp/p3_recheck/`（**非仓内**，未编译/未运行任何仓内二进制）。`recompute.py` 是 `p3_wcs.{h,cpp}` 公式的 1:1 转写，每处标注源码行号。

| 编号 | 构造什么 | 期望推翻什么 | 是否推翻 |
|---|---|---|---|
| **CE-0** | 把 `pix2world` 的 `θ=atan2(1,r)` 当作可疑的「补角」写法，怀疑 TAN 公式整体错误 | 推翻「核心数学正确」 | **未推翻（我方结论）**。代数验：`cos θ_code = r/√(1+r²)`、`sin θ_code = 1/√(1+r²)`，代入 `:532` 得 `(sin d₀ + η cos d₀)/√(1+r²)`，与向量法 `v = n₀ + ξ·e + η·n` 的精确结果**逐项相同**。核心数学正确 |
| **CE-1** | `\|CRVAL2\|=85°`、FOV 5/10/18/19.9°、1024²，逐点查 `make` 状态、角点 `\|dec\|`、`world2pix` 状态、密集域丢弃率 | 推翻「适用域门确实守住了『违反 ⇒ 拒绝』」 | **✅ 推翻**。`make`=OK；角点 `dec` 达 **86.32°**；回灌 `world2pix`=**拒绝**；**29.8–47.0% 采样点被 `cpp:367` 丢弃**；门 GREEN |
| **CE-1b** | 把 CE-1 的过滤器关掉，用同一批点算真实误差 | 证明被丢弃的恰是最差者 | **✅ 成立**。FOV=19.9°/1024²：保留子集最差 **8.49e-12 px**，丢弃子集最差 **6.14e-10 px**，**比值 72.3×**，帧真实最差 100% 落在被丢弃集里 |
| **CE-2** | `p3_wcs_make(NaN, 2.0, 0.001, 64,64,…)` | 推翻「`make` 会拒绝非法输入」 | **✅ 推翻**。status=**0**，`crval_ra=NaN`；`world2pix(NaN)` 靠 `NaN<=0.0` 为假而**穿过半球守卫返回 OK**；4096 点全计入、实测往返误差**恰 0.0**；`cpp:500` 判绿 |
| **CE-3** | 扫 8 档尺度找「过滤器藏起真红灯」的区间 | 证明该过滤器不只是口径瑕疵，而是会掩盖门越线 | **❌ 未推翻（我方主动下调）**。过滤器**系统性地把帧真实最差值低估 20–60×**（全尺度一致），但 `kMaxSide=20000` 挡住了构造小尺度大帧的路径，**在可达参数域内真实最差最大只到 2.5e-9 px（=容差的 25%），从未真正越线**。故 B1 的定性应是「**保证被证伪**（报出的最差值不是帧的最差值）」，而**不是**「藏起了一条已存在的红灯」——不夸大 |
| **CE-4** | 解 `cpp:526` 的 `r≥π/2` 与 `cpp:552` 的 `denom≤0` 各自的真实角半径 | 证明正/逆域守卫不是同一集合 | **✅ 成立**。正向在角半径 **57.518°（FOV≥115.04°）** 就报「跨半球」，逆向允许到 **90°**；声明域 FOV≤20° 使其仅为潜伏，但守卫**并未实现注释所说的「半球」** |
| **CE-5** | 用实现常量（`C_env=128`, `u=2⁻⁵³`, 含 `sec²Δ`）反解紧门/全域门的尺度下界，与散文对照 | 核对 `p3_wcs.h:74-75` 的保守性下界声明 | **✅ 文档偏松 3%**。实现值 `0.3020 / 3.0205e-3 ″/px`，散文值 `0.293 / 2.931e-3` 恰为**省略 `sec²Δ`** 的旧式；代码更保守 |
| **CE-6** | 修正我第一版驱动后重扫尺度带 | 同 CE-3 | **❌ 未推翻**（已并入 CE-3 的下调） |
| **CE-7** | 算 `kP3CropEdgeSamples=257` 在 1°/2.5°/5°/10° 半边下的每子边矢高 | 检验「257 只按 2° 画幅推导，声明域却是 20°」是否构成功能缺陷 | **❌ 未推翻功能缺陷**。10° 半边处 5.85e-8 px，比 1 px 小 **4 个量级** ⇒ 不足以切像素。**降级为文档缺陷（M14）** |
| **CE-8** | 扫 `p3_proj.cpp:107` 的 `asin` 入参 `A = sinθ sind₀ + cosθ cosd₀ cosφ` 是否能浮点溢出 1 | 检验子代理「36% 组合溢出」 | **❌ 未推翻**。4×10⁶ 次抽样（随机 θ/φ/d₀ + `θ=π/2` 精确）**零次** `A>1`。机制（正向不夹紧 vs `:168-169` 夹紧）真实存在，但**发生率未证** ⇒ 降级（M15） |
| **CE-9** | `v6::make(kTAN,150,−85,0.2,401,401)` 后逐点 `pix2world`，查 `\|dec\|>85` 的点数 | 证明 v6 内核也有 B1 同类病 | **✅ 推翻**。`dec ∈ [−89.40, −41.81]°`，**42 个采样点越界**，回灌 `world2pix` 即 `kParam` |

---

## 6 盲复算（遮住既有判定独立取证）

做法：先写下我自己对每条关键声明的独立裁决，再与子代理/既有判定对齐，**不一致处一律以我的重算为准并记录分歧**。

| 声明 | 我的盲复算结论 | 与既有判定对比 | 判 |
|---|---|---|---|
| TAN gnomonic 正反映射数学正确 | 逐式代数验证，**正确**（CE-0） | 一致 | 一致 |
| `make` 守住了 `\|CRVAL2\|≤85` 的完整后果 | **不成立**：`\|dec\|≤85` 只守中心，不守角点映射结果（CE-1） | 子代理未发现；**我方新发现** | **偏松（既有判定漏检）** |
| `NaN` 会被上游挡住 | **不成立**：`centre_ra_deg` 完全无守卫（CE-2） | 子代理 A 独立发现（我在 v1 侧也见到同类）；**双方独立收敛** | 一致 |
| 往返门能验出 CD/PA 写错 | **不能**，且守 CD 的两道门是同义反复（B3） | 子代理 A 独立推出同一结论 | 一致 |
| `kP3CropEdgeSamples=257` 在 20° 域边会超标 | **不成立**，实测比 1 px 小 4 个量级（CE-7） | **子代理称「~2e-3 px，2× 超标」** | **偏严（否决子代理，见 §7）** |
| `p3_proj.cpp:107` 的 `asin` 会以 36% 概率溢出 | **未能复现**（CE-8，4×10⁶ 抽样零命中） | 子代理称「60 860/170 001 = 36%」 | **偏严（下调子代理）** |
| 过滤器藏起了已存在的真红灯 | **不成立**（CE-3/6），但保证被证伪 | 子代理称「最坏条件的样本被筛掉」 | **部分偏松（我方主动收紧措辞）** |
| `p3_projection.h` RETIRED 声明属实 | **属实**：全仓 `p3_projection.cpp` 消费者仅 `eng/tests/unit/*`，无生产 target | 子代理亦核实；**不构成「退役仍有活调用者」** | 一致（未误报） |
| 本片私建线程池 | **零**：`p3_wcs.cpp`/`p3_proj.cpp`/`p3_projection.cpp` 的 `std::thread`/`omp`/`mutex` 计数均为 0 | 一致 | 一致 |

**总体：判偏松 3 处、偏严 2 处、一致 4 处。** 既有判定（含子代理报告）在「自洽式断言」类上命中率高，但在**量级上会夸大**（CE-7、CE-8），且**漏掉了 CE-1 这条最贴近产品路径的阻断**。

---

## 7 子代理派发记录

**派发 5 个**（`subagent`，全部只读、零仓内改动、零编译、零 git 写；均在同一工作目录，仅读取）。因首条派发参数键名笔误产生 1 个与第 1 条重复的实例，故有效分工为 **4 个**。

| # | 名称/ID | 范围 | 状态 |
|---|---|---|---|
| 1 | `915ecc5b…` | `p3_wcs.h` + `p3_wcs.cpp`（966 行） | 已交回 |
| 2 | `a937b48b…` | `p3_projection.h/.cpp` + `p3_projection_registry.h` | 已交回 |
| 3 | `da4b07b4…` | `p3_proj.h` + `p3_proj.cpp`（1047 行） | 已交回 |
| 4 | `e4dfd809…` | 测试面 + 文档面 + 两个 CMakeLists（1520 行） | **未交回**（本片 17 份我已 100% 自读，其范围已由我本人覆盖，故不阻塞） |

### 逐条复核与否决

**采纳（复核后成立）**
- 子代理 2 的 **B-3（探针循环性）** → 我复核调用链 `registry.h:206 → p3_wcs.cpp:68 → :167 → :122` 成立，**独立收敛于我的同一发现**，采纳为本片 B6。
- 子代理 3 的 **B3（`p3_proj.cpp:628` 未初始化读）**、**M-6 filtered-extremum**、**B4（通量守恒被假定而非计算，`A_ij=Ω_j∩Ω'_i` 由调用方给、模块内无人算守恒残差）** —— 我逐行核对成立，采纳。
- 子代理 3 的「`p3_proj.h:15` 奇异集写反」「`h:14/:20` 行锚 195→229」—— 我复核确认，采纳。
- 子代理 2 的 **B-4（v1 偏差表漏第 5 条）**、**B-6/B-7（`sin_roundtrip_gate.py` 未挂 CMake、`.json` 证据缺失）** —— 成立，采纳（M4、M16）。
- 子代理 2 的 **`p3_proj_probe` 独立性声明为假** —— 与其 B-3 同源，已合并。

**否决（复核后推翻）**
1. ⛔ **子代理 1 的 M1「`kP3CropEdgeSamples=257` 在 20° 域边约 2e-3 px，是注释所述上界的 2×，与 `h:247`「保证不切像素」矛盾」—— 驳回。** 其算式把角秒当像素、未除以像素角尺度。我独立重算（CE-7）：域边每子边矢高 **5.85e-8 px**，比 1 px 小 4 个量级，**功能上不可能切像素**。降级为文档缺陷 M14。
2. ⛔ **子代理 3 的 B1「`p3_proj.cpp:107/155` 的 `asin` 入参 36%（60 860/170 001）溢出 ⇒ NaN + `kOk`」—— 频率断言驳回。** 我扫 4×10⁶ 组合零命中（CE-8）；`A` 是两个单位向量的点积，需两次独立舍入同向巧合才可能越 1。保留「正向不夹紧 vs `:168-169` 夹紧」的**代码不一致**（降级 M15），删除其阻断定性。
3. ⛔ **子代理 1 对「本模块私建线程池」的 19 个算法/基础设施树统计** —— **超出本片文件域**，且其自陈「多为仅声明线程类型的头，未经逐行归属」。本片口径下重做：三个生产 `.cpp` 的 `std::thread`/`omp`/`mutex` 计数**均为 0**，本片**无**线程池违规（该 9 处违规属其它片，我不越界取证）。
4. ⛔ **子代理 1 的 M9「`%.10f` 写入 `buf[128]` 静默截断」—— 降级为建议。** `snprintf` 不溢出但会截断；我未找到可达的输入规模使其越过合同上界（`kMaxSide=20000` + FOV≤20° 共同约束），故不按阻断/须修记。
5. ⛔ **子代理 2 的 B-1「零初始化 descriptor 让 `pix2world` 返回 OK 且恒等于 CRVAL」** —— 机制成立（`p3_projection.h:80` 默认 `projection=TAN` + `cd` 全 0），但该文件是 **RETIRED 且仅被 `eng/tests/unit` 消费**，不构成生产路径缺陷，**降级为须修**，不进阻断。

**明确不成立的既有判定（我方主动推翻，防止「检查通过」被当正确性）**
- `p3_projection_registry.h:204` 的「判据独立性」注释 —— **证伪**（B6）。
- `p3_proj.h:10-13` 的「内核-only、未接入生产」—— **部分证伪**：`lib/phase3_session/p3_export.h:86` include 本头且 `p3_export.cpp:476,493` 调用 `v6::plan`/`v6::solid_angle_grid`（潜伏依赖，非在役）；编译它的测试 target 是 **4 个**不是 3 个。
- `test_p3_wcs.py:2/:84` 的「独立参考（向量法）」—— **证伪**（B5）。
- `p3_wcs.cpp:197-211` 的往返门冻结值推导 —— **不可验证**（证据文件三处全缺）。

---

## 8 自证段（可复跑）

> 全部只读；不编译、不跑 ctest/pytest、不跑任何仓内二进制、不做任何 git 写。

```bash
# 0) 基线与本片清单（权威）
cd "/workspace/Astro CS Database"
git -c core.quotepath=false log --oneline -1          # => 850a9ede
sed -n '614,638p' "run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml"

# 1) 覆盖率自证：17 份成员逐份 wc -l，合计应为 4899
for f in lib/algorithms/projection/p3_proj.cpp \
         lib/algorithms/projection/tests/p3wcs/p3_projection_registry_test.cpp \
         lib/algorithms/projection/p3_wcs.cpp \
         lib/algorithms/projection/tests/p3wcs/p3_wcs_test.cpp \
         lib/algorithms/projection/p3_projection.cpp \
         lib/algorithms/projection/p3_wcs.h \
         lib/algorithms/projection/p3_projection_registry.h \
         lib/algorithms/projection/p3_proj.h \
         lib/algorithms/projection/tests/p3wcs/test_p3_wcs.py \
         lib/algorithms/projection/memory.md \
         lib/algorithms/projection/module.yaml \
         lib/algorithms/projection/README.md \
         lib/algorithms/projection/p3_projection.h \
         lib/algorithms/projection/tests/p3wcs/p3_projection_unsupported_cli.py \
         lib/algorithms/projection/tests/p3wcs/p3_wcs_main.cpp \
         lib/algorithms/projection/CMakeLists.txt \
         lib/algorithms/projection/tests/p3wcs/CMakeLists.txt ; do
  printf "%6s  %s\n" "$(wc -l < "$f")" "$f"; done | tee /tmp/p3_recheck/lines.txt
awk '{s+=$1} END{print "TOTAL =",s}' /tmp/p3_recheck/lines.txt      # => TOTAL = 4899

# 2) 悬空引用取证（M16）
for p in eng/tests/unit/p3_wcs_test.cpp eng/tests/p3wcs \
         eng/tests/backend/p3_wcs_main.cpp evidence \
         run/GATE-DERIVE-01 run/GATE-WCS-01 \
         eng/ci/checks.json lib/phase3_session/p3_wcs.cpp ; do
  [ -e "$p" ] && echo "EXISTS  $p" || echo "MISSING $p"; done
git -c core.quotepath=false ls-files | grep -c '^run/'   # => 0（run/* 全被忽略）

# 3) 孤儿源 / 未接线测试面（M5）
git -c core.quotepath=false grep -n "p3_wcs_main\|test_p3_wcs" -- '*/CMakeLists.txt'   # => 无命中
grep -n "add_executable\|add_test" lib/algorithms/projection/tests/p3wcs/CMakeLists.txt

# 4) 未武装的负向注入（p3_wcs_test.cpp:133-143）
git -c core.quotepath=false grep -n "STD_F1_BRIDGE_FAULT"      # => 只出现在测试自身，无任何设置方
git -c core.quotepath=false grep -n "STD_F1_BRIDGE_FAULT" -- '*.txt' '*.json' '*.yml' '*.yaml' '*.cmake'  # => 空

# 5) 线程池口径（仅本片三个生产 .cpp）
for f in p3_wcs.cpp p3_proj.cpp p3_projection.cpp; do
  printf "%-22s thread=%s omp=%s mutex=%s\n" "$f" \
    "$(grep -c std::thread lib/algorithms/projection/$f)" \
    "$(grep -c 'omp ' lib/algorithms/projection/$f)" \
    "$(grep -c std::mutex lib/algorithms/projection/$f)"; done   # => 全部 0

# 6) 自洽式断言取证（B3 / B5 / M1）
sed -n '525,536p;363,372p;450,452p;463,472p;139,140p;113,115p' lib/algorithms/projection/p3_wcs.cpp
sed -n '85,89p;31,37p'                lib/algorithms/projection/tests/p3wcs/test_p3_wcs.py
sed -n '557,566p'                    lib/algorithms/projection/tests/p3wcs/p3_projection_registry_test.cpp
sed -n '204,209p'                    lib/algorithms/projection/p3_projection_registry.h
sed -n '64,70p'                      lib/algorithms/projection/p3_wcs.cpp

# 7) 我的独立重算（不编译仓内代码；仅跑我自己的 Python 复算脚本）
ls -la /tmp/p3_recheck/
python3 /tmp/p3_recheck/probe1.py   # CE-1 : make=OK / 角点 |dec|>85 / world2pix 拒绝 / 丢弃率 / 门 GREEN
python3 /tmp/p3_recheck/probe2.py   # CE-2 NaN 恒绿 / CE-3 丢弃集误差 72.3x / CE-4 守卫不对称 / CE-5 常数核对
python3 /tmp/p3_recheck/probe4.py   # CE-6 修正后的尺度扫描（未推翻 concealment，故 M 未按藏红灯记）
# CE-7 / CE-8 / CE-9 见会话内脚本；要点：10° 半边矢高 5.85e-8 px；asin 4e6 抽样零溢出；
#   v6 make(kTAN,150,-85,0.2,401,401) 正向 dec 低至 -89.40 deg，42 点越界。

# 8) 零 git 写 / 零仓内改动的自证
git -c core.quotepath=false status --porcelain -- lib/algorithms/projection | wc -l   # => 0
git -c core.quotepath=false diff --stat HEAD -- lib/algorithms/projection            # => 空
```

**本审稿人交付时对仓库的唯一写入即本文件**：`run/GOVERN-08/审核包-R2/审稿-P1-ALG-projection-001.md`。

---

*覆盖率 17/17 份 · 4899/4899 行 · 100%；子代理派发 5（有效 4）· 否决 5 条 · 采纳 6 组；反例 11 个（CE-0…CE-9，其中 3 个推翻我方初判并已主动下调措辞）。*