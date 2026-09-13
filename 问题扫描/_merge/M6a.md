# 合并验证 M6a — 注释质量横向（Phase1 侧 L13 + Phase2/3 侧 L14）

- 合并代理码: M6a（第二层合并验证）
- 输入: `_cache/L13.md`（20 条：P0:1/P1:13/P2:6）、`_cache/L14.md`（15 条：P0:2/P1:6/P2:7），共 **35 条**
- 基准证据: `_cache/F00_FRONT_SPOTCHECKS.md`（F00-03/04/05/06）、`_merge/00_COORDINATION.md`（L14-004 前台逐行复验记录）
- 判定基线（宪章原文亲读）: §4.1、§10.4、§10.5、§12.2、§12.3-1/-5/-8/-9/-10/-12/-13、§13.1、§14.2/§14.3、§16.2、§17、§17.6、§18.1/§18.2；RQS-PROTOCOL-001 §1/§2/§3；`AGENTS.md` 目录规范与执行纪律
- 取证方式: 全程只读（read/grep/glob），未使用 shell/构建/git；免报区（run/** build/** artifacts/** evidence/** reports/** 工程控制/** GaiaDR3* BASS* wiki）仅作旁证
- 定稿文件（12 个，每「类别×优先级」一件）:
  - `findings/G_GOV_GATE/p0/M6a_L13_L14.md`、`findings/G_GOV_GATE/p1/M6a_L13_L14.md`
  - `findings/C_DOC_CODE_GAP/p0|p1|p2/M6a_L13_L14.md`
  - `findings/D_COMMENT/p1|p2/M6a_L13_L14.md`
  - `findings/E_TRACE_BREAK/p1|p2/M6a_L13_L14.md`
  - `findings/I_DOC_HYGIENE/p1|p2/M6a_L13_L14.md`
  - `findings/B_STD_MISMATCH/p1/M6a_L13_L14.md`、`findings/A_SCI_DEF/p2/M6a_L13_L14.md`、`findings/F_TEST_GAP/p2/M6a_L13_L14.md`
- **定稿计数: 31 条（P0:2 / P1:15 / P2:14）**；35 条叶子 → 5 条整条已由他域定稿（不重复登记）、30 条在本域有定稿点（其中 L13-015 与 L14-003 并为同一条），另新增 2 条横向定稿（M6a-C-003 现状宣称三源对账、M6a-I-005 宣称形态量化）

## 1. 逐条处置表（四态：仍成立 / 已被修复 / 部分修复 / 无法判定）

| 来源 | 标题（简） | 四态 | 定稿编号 | 类别/优先级 | 处置理由 |
|---|---|---|---|---|---|
| L13-001 | SIP 1e-4 px 门注释自证不可达 | 仍成立（残余） | M6a-D-001 | D_COMMENT/P1 | 科学三点互锁已由 **M1a-C-001 定 P0**；本域只落注释层：裁决请求无落点 + 引用的 DISP-WCS-007/008 在 docs 登记面零命中（只在控制包/evidence 台账以「候选」存在，007 已翻锚亦不回写），故 P0→P1 |
| L13-002 | 注释以不存在文档为合同锚 | 已由他域定稿 | —（指 M6b-E-004） | E_TRACE/P1（M6b） | M6b-E-004 的 位置/来源 明列 02_FROZEN/00_COMMON_CONTRACTS/SNR_* 与 `snr_estimator.h:216-217`、`photometry_apply.h:7,12`（含 133 处计数）⇒ 同事实不重复登记；本域新形态另立 M6a-E-001 |
| L13-003 | 租约降级语义四适配器三口径 | 仍成立 | M6a-D-002 | D_COMMENT/P1 | CAL 头与其自身实现相反（硬证据），DRZ 真降级、COS/NOISE 引错先例 |
| L13-004 | StarRecord.mag 注释只给饱和星式 | 已由他域定稿 | —（指 M3b C_DOC_CODE_GAP/p1） | C_DOC/P1（M3b） | M3b 该条 位置 已含 `::StarRecord.mag 注释(:238)` 与 mag 分支双式 ⇒ 不重复登记 |
| L13-005 | PSF [N,9] 布局三方割裂 + A/B 倒置 | 已由他域定稿（本域核实一致） | —（指 M3b-C-04） | C_DOC/P1（M3b） | M3b-C-04 即「两份 L1 合同 A/B 标签相反 + 布局 A 两列无缓冲」，且其 位置 含 ALG §1.1/DATA §15.2/代码逐列赋值 ⇒ 本域会签确认（见 §3.4），不重复登记 |
| L13-006 | dpsf 文件头 7 锚与 ALG 934 行基准 | 已由他域定稿（本域出补证） | —（指 M3b-E-01） | E_TRACE/P1（M3b） | M3b-E-01 位置含 `dpsf_psf.cpp:1-6` 与 ALG/DATA/README 锚漂移；本域实测偏移表见 §2.3、移交 §9 |
| L13-007 | ipv_wcs 头仍写 NB_GRID=7 | 仍成立（残余） | M6a-D-003 | D_COMMENT/P1 | M1a-C-001 定 SCI/ALG/registry 层；同文件顶部注释块 :17/:238 未被其 位置 覆盖 |
| L13-008 | 模块私造 logger 写 logs/ | 仍成立 | M6a-G-002 | G_GOV/P1 | §11 统一日志零实现；并入 L14-011 的无条件 stderr 子事实 |
| L13-009 | astro_calibration.h「不影响 FP64 全链路精度」 | 仍成立 | M6a-D-004 | D_COMMENT/P1 | 降级已登记（DISP-COS-004），未登记的是总括结论；两条已接线 _f64 生产 op 推翻其前提 |
| L13-010 | snr C 头承诺 clamp variance_floor | 仍成立 | M6a-D-005 | D_COMMENT/P1 | 真实语义为进程级指针键侧表 + 未登记回退 1e-12；禁拷贝/线程约束仅在内部头 |
| L13-011 | SNR_QF_PSF_OK 含 status==3 | 仍成立（降级） | M6a-D-007 | D_COMMENT/P2 | P1→P2：生产 SNR 通道一律 `status!=0` 剔除，宽松口径只在遗留族与不在根图的 orchestrator；合同意图待裁（§10）；主事实移交 M3a |
| L13-012 | drizzle nested 三处口径 | **部分修复** | M6a-C-004 | C_DOC/P2 | `lib/drizzle/README.md:52` 已订正「仅 1=NESTED」⇒ 进已修复表；残余 = types.h 词表并列 + DLL 面缺省仍 0（fail-closed ⇒ P1→P2） |
| L13-013 | 双「唯一真实入口 / 唯一生产实现」 | 仍成立（注释层） | M6a-D-008 | D_COMMENT/P2 | 权威归属与根图解除的主事实移交 M3a（L05/L07）；本域只定稿注释互斥与测试目标名失真 |
| L13-014 | module_adapters 头委托总述陈旧 | 仍成立 | M6a-D-006 | D_COMMENT/P1 | 四处与实现/根 CMake 不符（含 drizzle 宿主库归属写反），同文件并存两代注释无取代标记 |
| L13-015 | §12.2 禁止形态充当注释主干 | 仍成立 | M6a-D-009（与 L14-003 并档） | D_COMMENT/P2 | 保留 P2；同时登记「该形态无机检防线」的因果（→ M6a-G-001） |
| L13-016 | cosmetic 8 邻居 vs 4 方向 IDW | 仍成立 | M6a-D-011 | D_COMMENT/P2 | 名实不符已由 DISP-COS-003 登记（M2a 域），新增错文是「8 邻居」 |
| L13-017 | sdet 断句残片 + 40 处悬空上游锚 | 仍成立（残余） | M6a-E-002 | E_TRACE/P2 | 断句/流水已由 **M3b-D-01** 定稿；本域只定稿 `star_finder.c`/`PSF.c` 共 40 处不可解析源锚 |
| L13-018 | 公共头前后置条件缺口簇 | 仍成立（残余） | M6a-D-010 | D_COMMENT/P1 | star_detector.h/dynamic_psf.h 零契约已由 M3b-D-01 定稿；本条落三要素量化 + photometry_apply 返回码缺 -5 + ac_calibrate_frame 无单位 |
| L13-019 | healpix oracle 百万点不可复现 | 仍成立 | M6a-F-001 | F_TEST/P2 | 生成器脚本与 jsonl 仓内均不存在、未挂 ctest/CI（三重零命中复算）；无错值证据 ⇒ 保 P2 |
| L13-020 | 遗留 cc_* 无 legacy 声明 + 库存清单 production=yes | 仍成立 | M6a-I-004 | I_DOC/P2 | 三源互斥（清单/README/根 CMake） |
| L14-001 | 三件套「实测」行数与现状陈述 | 仍成立 | M6a-I-001 | I_DOC/P1 | 7 个文件行数全部复算不符；phase3_proj 两条现状陈述被代码直接推翻（硬事实）；`docs/modules/*` 面归 M6b |
| L14-002 | 控制包章节号 / 截断哈希 / wiki | 仍成立（新形态） | M6a-E-001 | E_TRACE/P1 | 不存在文档族已由 M6b-E-004 定稿；本条只定稿「12 §x/15 §2/C §C6」章节号 + ARCHIVED 标准并列 + v19r4 工具正则对 8+8 截断形态失效 |
| L14-003 | Phase2/3 §12.2 堆积量化 | 仍成立（并档） | M6a-D-009 | D_COMMENT/P2 | 与 L13-015 同一违例模式 ⇒ 并为一条（六目录计数 257 实测）；Phase2 具体实例已由 M4-D-03 定稿，本条不重复其例 |
| L14-004 | 注释门 CON-COMMENTS 空壳 | 仍成立 | **M6a-G-001** | G_GOV/**P0** | 四条必核①；前台 7 子事实全部复算成立，另补 2 子事实（见 §3.1） |
| L14-005 | ORDERSEL 记录的不是实际层级 | 仍成立 | **M6a-C-001** | C_DOC/**P0** | 四条必核②；双通道 order_sel 全后继穷举（见 §3.2） |
| L14-006 | ARCH-P3 TileCache 共享读/LRU | 仍成立 | M6a-C-002 | C_DOC/P1 | 与实现（每 worker 无锁 FIFO）相反，且内存上界式不含 n_workers |
| L14-007 | 注释第二线程预算源 | 已由他域定稿 | —（指 M5a-G-006 + M5a-D-001） | G_GOV/P1（M5a） | M5a 两条已覆盖 execution_options「唯一来源」+ hardware_concurrency + sampler.h:54 半句 ⇒ 不重复登记 |
| L14-008 | coverage_ok 恒 1 与合同口径 | **部分修复** | M6a-I-002 | I_DOC/P1 | 会话与 writer 节点已停发该字段、verify 节点与 IR verify 通道已发真值 ⇒ 进已修复表；残余 = 写路径常量 + 头/DATA/ALG 口径 + 三处常量断言（**无有效回归测试** ⇒ 建议处置③） |
| L14-009 | AIT 合法域 1<A≤2 未拒 | 仍成立 | M6a-B-001 | B_STD/P1 | 恒等式与反例闭式可推（§2.4）；因 registry 未接生产 ⇒ 保 P1 不升 P0（M1a 已在其域定 oracle 共源 P0） |
| L14-010 | registry 守卫字段装饰性 | 仍成立 | M6a-D-012 | D_COMMENT/P2 | grep 复算：两守卫字段读取点为零（selfcheck 仅 `>0`） |
| L14-011 | 死代码被合同固化 + stderr 探针 | 仍成立 | M6a-D-013（stderr 子事实并 M6a-G-002） | D_COMMENT/P2 | `(void)y1` 压制实际使用变量；ALG §6.5 据以叙述 ⇒ 合同固化错实现 |
| L14-012 | kMaxOrder 出处虚假 | 仍成立 | M6a-A-001 | A_SCI/P2 | ARCH-P3 §3 实测无 order 上界条款；叶数式少 12 倍；三处口径互斥（properties 拒 / select 拒 / session 静默降） |
| L14-013 | 同文件并发注释互斥 | 仍成立 | M6a-D-014 | D_COMMENT/P2 | 文档面「并行路径不经此锁」已由 **M4-C-05** 定稿 ⇒ 本条缩为代码内注释互斥残余 |
| L14-014 | 36/36 PASS 与 run/local 指针 | 仍成立（升档） | M6a-I-003 | I_DOC/**P1** | P2→P1：新证 —— `tests/abi/test_secure_loader.py` 无 unittest 用例而 UT-ABI 门用 `unittest discover` ⇒ 该 36-check 编排在 CI 面零执行（见 §2.5） |
| L14-015 | 浏览器头 [y*512+x] 排列 | 仍成立 | M6a-D-015 | D_COMMENT/P2 | 叶子给的锚 `:140` 不存在（文件仅 91 行），证据实为 `:8-9` ⇒ 记入 §6 漂移处置，按符号定稿不剔除 |

## 2. 复算与原文对照（关键项）

### 2.1 行数复算（全部为 read 实测，用于 M6a-I-001）
| 文件 | 文档自称 | 实测 | 差 |
|---|---|---|---|
| `lib/phase2/src/coverage.cpp` | 239 行（phase2 README:10） | 280 | +41 |
| `lib/phase2/include/astro/phase2/coverage.h` | 59 行（:11） | 65 | +6 |
| `lib/phase3_session/p3_session.cpp` | 343 行（RSMP ALG:56） | 433 | +90 |
| `lib/phase3_session/p3_wcs.cpp` | 165 行（PROJ ALG:12） | 230 | +65 |
| `lib/phase3_session/p3_wcs.h` | 50 行（PROJ ALG:11） | 63 | +13 |
| `lib/phase3_session/p3_resample.cpp` | 239 行（RSMP ALG:55） | 366 | +127 |
| `lib/phase3_session/p3_resample.h` | 58 行（RSMP ALG:54） | 116 | +58 |

### 2.2 注释门判据复算（M6a-G-001）
- `STALE_PATTERNS`/`REQUIRE_ID_NEAR` 全仓引用数 = 仅定义行本身（grep 复算）⇒ 死常量；
- 唯一生效判据 `"V19R2" in c or "V19R3" in c` + 豁免 `"冻结" not in c`；在树豁免样本 2 处（`lib/phase2/tests/synthetic_gate.cpp`、`lib/healpix_db/healpix_drizzle/tests/test_spherical_overlap.cpp`）；
- 扫描面 `lib/**.{cpp,h,hpp}`（不含 `.c`；lib 下第一方 `.c` 实测 7 个），`cli/` 5 cpp、`runtime/` 5 `.c`、`include/` 23 `.h` 均不在面内；
- coverage 自报恒 `ratio=1.0, mode:"full"`（:52）；每文件命中即 break（:42/:48）；JUnit 单 testcase（:60-61）；
- CI 挂载：`ci/checks.json:772-797` id=CON-COMMENTS，profiles fast/linux-main/windows-main，`waivable:false`。

### 2.3 dpsf_psf.cpp 文件头锚点表偏移（L13-006 补证；该头在扫描期间被并发重写为「B4-14 数值契约」版，锚仍全漂 ⇒ 该条未修复）
现行 `dpsf_psf.cpp:3-5` 的守卫锚表（逐字）：

> 守卫：pivot 1e-30（数值奇异，45）、sx/sy≤0 哨兵 1e10（71-74）、Q<0 哨兵 1e10（89-91）、
>  h=max(|x|·1e-6,1e-8)（114）、sx/sy 下界 0.3（169-170,326）、FWHM>rect 拒绝（333-341）、
>  背景比 0.5（343-344）

| 锚 | 声明内容 | 本代理实测位置 | 偏移 |
|---|---|---|---|
| 45 | `max_val < 1e-30` 奇异判定 | :52 | +7 |
| 71-74 | `sx<=0\|\|sy<=0` → 1e10 哨兵 | :78-80 | +7 |
| 89-91 | `Q < 0` 分支 | :96 | +5~+7 |
| 114 | `h = max(\|x\|·1e-6, 1e-8)` | :121 | +7 |
| 169-170 | `x[4]/x[5] < 0.3` 下界钳制 | :176-177 | +7 |
| 326 | 第二处 0.3 判定（`sx<=0.3\|\|sy<=0.3`） | :364 | +38 |
| 333-341 | `FWHM exceeds rect` 拒绝 | :374-375 | +41 |
| 343-344 | 背景比 0.5 判定 | :382 | +38~+39 |
- 即：前四组偏 +7、后三组偏 +38~+41，**八处锚全部不符**；且 :1-2 仍自称「（B4-14，不改算法，仅文档锚点）…实测提取冻结」。
- 另：`docs/algorithms/STAR_PSF_ALGORITHMS.md:132` 自称「dpsf_psf.cpp 934 行实测基准」，本代理实测该文件 **1124 行**（+190）⇒ 基准行号已失效。
- 归属：M3b-E-01 已把「sdet/dpsf 面行锚成批漂移」定稿（其 位置 含 `lib/dynamic_psf/src/dpsf_psf.cpp:1-6`），本表作为该条的补证移交（§9）；本域不重复登记。

### 2.4 AIT 域守卫复算（M6a-B-001）
- 代码/ALG 自述恒等式：`A = X²/4 + Y² = 1 − cosθ·cos(φ/2)`。数值代入：θ=0,φ=π ⇒ A=1 ✓；θ=30°,φ=60° ⇒ A=0.22985 = 1−cos30°cos30° ✓ ⇒ 天球（|θ|≤π/2, |φ|≤π）像集恰为 **A≤1**。
- 实现判据是 `dsq = 2 − A > 0`（A<2）与 `|Y·dq| ≤ 1`。反例（代入实现式）：`X=0, Y=√1.5` ⇒ A=1.5、dq=0.7483、sinθ=0.9165（≤1）⇒ θ≈66.4°、`atan2(0, dsq−1=−0.5)=π` ⇒ φ=2π ⇒ RA 绕回中心，状态 OK；而正向 `(RA0,66.4°)` 给 Y=sinθ/√(1+cosθ)≈0.7745 ≠ 1.2247 ⇒ 往返不闭合且未被拒。四角守卫（:342-350）走同一 pix2world ⇒ 同样放行。
- 两侧 oracle 同式：`tests/unit/p3_projection_test.cpp:445-451`、`tests/backend/test_p3_projection_oracle.py:158-170`（域外取样点 1e9，落在 A≫2 的远域，必然通过）。

### 2.5 UT-ABI 复算（M6a-I-003）
- `runtime/module_loader/README.md:12/:39` 称「36 checks / 36/36 PASS」；`tests/abi/test_secure_loader.py` 中 `check()` 调用点实测 **36** 处（数量相符），但同文件 grep `unittest\|TestCase\|def test_` **零命中**；
- `ci/checks.json:1642-1658` 的 UT-ABI 命令 = `python3 -B -m unittest discover -s tests/abi -t tests/abi` ⇒ 该脚本不被收集为用例；
- 结论限于「CI 面零执行落点」；该轮检查历史上是否真跑过不在仓内可判（§10）。

### 2.6 与叶子档案的其它差异（不改判定，仅订正坐标）
- `DATA_SEMANTICS.md` 的 `result.coverage_ok` 行在 **:1978**（叶子写 :1977），`§27.3` 在 :1981；
- `p3_session.cpp` 现不再输出 `{"coverage_ok", …}`（改为 `coverage_stats`，:410），IR writer 节点同（:5120-5124），真值只在 verify 节点 :5209；
- `lib/phase2/README.md:94` 的 `p2_coverage_build` 实际定义在 coverage.cpp:173（README 写 :144）；
- 悬空权威实测：`12_DLL_ABI_AND_LOADER_STANDARD.md`/`15_CPU_PROVIDER_AND_RESOURCE_STANDARD.md`/`11_MODULE_SOURCE_TEST_STANDARD.md` 仅存在于 `工程控制/`（免报区），`docs/standards/` 实测 14 份无此三份；`AstroCS.wiki/` glob 无任何 `.md` 页。

## 3. 本域四条必核结论

### 3.1 L14-004 注释门空壳 → 定稿为**一条** G_GOV_GATE 根因（M6a-G-001，P0）
前台 7 个子事实**全部复核成立**（死常量、"冻结"永久豁免、扫描面只 lib 且不含 `.c`、每文件 break、coverage 恒 1.0/full、docstring 三项无实现、注释自述只修了截断），并补 2 个新子事实：⑧JUnit 汇总只写一条名为 hygiene 的 testcase（失败数取 P0/P1 计数，CI 看不到逐文件明细）；⑨被弃用的 `num_threads(16)` 正则本可命中 F00-03 的 `cli/pc_api.cpp`「16 线程」注释，但 `cli/` 不在扫描面内 ⇒ 门与本域主题擦肩而过（该注释事实本身已在 M5a 域定稿，此处只记机制）。其它机器门（THREAD-BUDGET/AST-API/ACR-DORMANT/PROD-REACH/UT-*）一律归 M5a/M5b，本域不越界。

### 3.2 L14-005 ORDERSEL 装饰性 → 定稿 M6a-C-001（P0），`related: L03-001`
- **唯一后继核实**：`p3_session.cpp:196-199` 计算的 `order_sel`，全部后继只有 :358 字符串化 → :371 `prov.order_sel_used` → :405 结果 JSON；IR 通道（`module_adapters.cpp:4816-4818`）同形，后继只有 :5000/`{"order_sel"}`、:5018 metadata、:5084-5088 provenance。**没有任何读路径分支、tile 定位、leaf_nside 计算或越界判定引用它**。
- 采样读路径恒按 hips_order：`p3_resample.cpp:135-136 s->order=p.order; s->leaf_nside=kTileWidth<<p.order;`，`read_leaf` 用 `s->order`；`aio_hips_reader.cpp:38-41` 注释称「层级路径由调用方给定，本 reader 不猜」而 :105/:155/:490 实为按 `d->hips_order` 组路径 ⇒ 无层级注入口。
- 合同冲突：`PHASE3_HIPS_TO_FITS.md:91`（§9a-5 冻结）要求 `leaf_nside=2^(order_sel+9)`，实现是 `512<<hips_order`，仅当两者相等时才一致。
- 分工：HiPS 产品/落盘可达性主定稿点 = **M2b（L03-001）**；本条只定稿「注释自述禁仅写 metadata 却只写 metadata + provenance 记的不是实际层级」这一侧。

### 3.3 L13-001 与 L01-004 同证 → 科学定档在 M1a（P0 已定），本域落注释层 M6a-D-001（P1，双向 related）
M1a 的 P0 定稿点（`C_DOC_CODE_GAP/p1` 与 `p0/M1a_L01_L02.md`）已把「SCI/ALG 7×7 vs 实现 41×41/81×81」「1e-4 px 门在一步逆上数学不可达」「该事实以注释形式挂起、未升为偏差登记」三点收口。本域据此**不重复登记科学面**，只补两点 M1a 未列的事实：①同文件顶部注释块 :17/:238 仍写整改前的 NB_GRID=7（M6a-D-003）；②注释与测试引用的偏差 ID **DISP-WCS-007/008 在 `docs/**` 全域零命中**（STANDARDS_REGISTRY 只登记 001/006，PLATESOLVE §11.3 亦无）；两 ID 只在 `工程控制/ACTIVITY_STATE.md:44`（免报区）与 `evidence/v8_1_ci_control/TASK_STATE.json:1563` 以「候选」形态存在，而 `tests/unit/p1wcs/p1wcs_tests_negative.cpp:255` 显示 007 已「翻锚」却仍无登记行 ⇒ 「未经定义即被引用」+「裁决/翻锚结论不回写权威登记面」（M6a-D-001）。

### 3.4 L13-005 PSF [N,9] 三方割裂 → 本域核实与 **M3b-C-04** 结论一致，故不重复登记（会签通过）
- 列序核实结果：批缓冲真实列序由 `lib/dynamic_psf/include/dynamic_psf.h:93-94`（`/* [7]=fwhm_x [8]=fwhm_y */`）与 `dpsf_psf.cpp:903-904`/`:1078-1079`（`out_row[7]=result.fwhm_x; out_row[8]=result.fwhm_y;`）钉死；消费侧 `orchestrator.cpp:2443-2458` 按 `[2]x0 [3]y0 [4]e_x [5]e_y [6]theta [7]fwhm_x [8]fwhm_y` 取；`module_adapters.cpp:2298-2299` 与 `p1_op_star_psf:1500` 注释同口径。⇒ **`DATA_SEMANTICS.md:514` §15.2 布局 B 与代码一致；`STAR_PSF_ALGORITHMS.md:14` §1.1 的「(B,A,x0,y0,sx,sy,θ,residual_scale,q_psf)」是错文**。
- 文档倒置核实：ALG §1.1:23 自称「布局 B（生产表顺序）」，而 DATA §15.2:513/:530 把「布局 A」定义为编排块、「布局 B」定义为批 API ⇒ 同名标签在两份 L1 文档中互为倒置。
- 结论：上述两点均已被 **M3b-C-04**（`C_DOC_CODE_GAP/p1/M3b_L06.md:24-39`）以 L06-015 为来源定稿，且其 问题说明 已覆盖「布局 A 两列无对应缓冲 / q_psf 无产出点 / 标签相反 / dynamic_psf.h:55-57 注释-接口错位」⇒ 按「同一事实只在一处定稿」，L13-005 记为**已由他域定稿（本域会签确认，核实结果一致）**。
- 本域仍新增一条未被 M3b 覆盖的实例：`lib/snr_estimator/cpp/include/snr_estimator.h:56-57/:196` 以「布局 A」指编排块（与 DATA 同、与 ALG §1.1 反），且 `q_psf` 被其 `SnrQfMask` 当在位字段消费（`SNR_QF_PSF_OK` 族）⇒ 已在 M6a-D-007 的 位置 面登记该口径互斥，科学侧随 M3b-C-04 整改。

## 4. 注释要素齐备性小结（20 个公共头/导出函数抽查；与 L28 互为校准，本域不抢定稿）

样本（12 个公共头，跨 Phase1/2/3 + 公共基建）：`astro_calibration.h`、`star_detector.h`、`dynamic_psf.h`、`ipv_api.h`、`PhotometryCalculator.h`、`drizzle/types.h`、`healpix_core.h`、`snr_estimator.h`、`common_abi_v1.h`、`phase2/{coverage,sampler,upm}.h`、`phase3_session/{p3_wcs,p3_resample,p3_output}.h`、`hips/aio_hips_reader.h`；抽 20 个符号（清单见 M6a-D-010 位置项）。判定采用「显式标注，不从 README 反推」：

| 要素 | 缺失 | 缺失率 | 说明 |
|---|---|---|---|
| A 算法来源（SCI/ALG/DISP/DATA 锚） | 3/20 | **15%** | 集中于 `astro_calibration.h::ac_calibrate_frame`、`ac_correct_frame`（同头另一符号 `ac_generate_master_dark_f64` 同）；`star_detector.h` 的 9 个导出函数整体缺 A/C |
| B 输入来源/域（单位与数据来源） | 2/20 | **10%** | `healpix_core.h::ang2pix_nest\|pix2ang_nest` 缺 deg/rad（同文件 :16 已声明「本头不解释天文语义」，属自觉例外但仍是消费方陷阱）；`drizzle/types.h::DrizzleConfig` 缺 scale 单位 |
| C 被消费者/生产者标识 | 11/20 | **55%** | 最重项；函数类尤甚（11 个函数中 10 个不写被谁调），头文件整体无「谁消费本头」段 |
| 三项全缺 | 2/20 | 10% | `ac_calibrate_frame`、`ac_correct_frame` |

- **正向结论（与 L28 校准用）**：抽样的 A 项 token（SCI-PSF-001/SCI-NOISE-001/SCI-CAL-001/ALG-STARDET-001/ALG-COS-003/ALG-DRZ-001/ALG-P3-002/DATA-P1-STAR 等）在 `docs/**` **零悬空**，即「写 ID 的都真存在」；失效率集中在**非 ID 形态**的锚：同 20 样本另有 6 组引用不可解析 —— 归档标准（`AstroCS_ENGINEERING_CONSTRAINTS.md §H.2`，:10/:12-13 自述 ARCHIVED 且宪章 :660 已替代）、控制包章节（`06 §2`）、上游已删源文件行锚（`Siril src/core/psfmatching.c:135`）、截断哈希 + wiki 页（`34A532A2...B2EB308 + wiki Phase2_Architecture`）、外部已删副本（`third_party/GPS 13.3/…sphere.c`，实测 third_party 无 GPS）、**ALG ID 交叉误标**（`p3_wcs.h:41` 引 ALG-P3-003，而该文件头 :1 自标 ALG-P3-002）。⇒ 建议 L28 的矩阵把「锚可解析性」与「锚存在性」分列两维。
- 边界：全仓三要素矩阵与缺失率总口径由 **L28（注释溯源轴）** 定稿；本域只交种子样本与判定口径，不与其抢定稿（M6a-D-010 已显式声明）。

## 5. 注释中的「现状宣称」对账（与注册表 / 节点表 / 根 CMake 三源）

定点对账 38 条含「当前实现 / 已接线 / 唯一生产入口 / 已验证 / 本版实测」措辞的注释与模块文档：**相符 15 / 错位 20 / 无法判定 3**（口径见 M6a-I-005）。错位者按归属分派如下（本域已定稿的只列编号，不重复正文）：

| 现状宣称（逐字要点） | 载体 | 对账结果 | 定稿归属 |
|---|---|---|---|
| 「AIO-002 原子发布原语**内建于 aio_hips 落盘路径**」 | `module_adapters.cpp:23-25`/`:2417-2421`/`:3911-3913` | 事务面 `aio_publish.cpp` 只编 `astrocs_p1_hips_writer`；节点直调的 `aio_hips_writer.cpp:185-186` 为 `std::remove` + cfitsio 直写；`DISP-HIPS-004` 自证相反 | **M6a-C-003**（表述侧）+ 移交 M2b（HiPS 落盘侧） |
| 「不影响 FP64 全链路精度」 | `astro_calibration.h:110-119` | 两条已接线 _f64 生产 op 推翻 | **M6a-D-004** |
| 「不接 dpsf_fit_batch」「drizzle→lib/drizzle 静态库」「writer→aio_write_fits」「委托 session adapter 一站式」 | `module_adapters.cpp:5-6/:11/:12/:524/:759` | 与同文件 :1485/:1933/:2476 与注册段 :5489-5522、根 :643-646 相反 | **M6a-D-006** |
| 「现状唯一生产实现 lib/snr_estimator/cpp/src/noise_model.cpp」＋测试目标 `astrocs_p1_noise_prod` | `lib/snr_estimator/{memory.md:35,README.md:5,tests/p1noise/CMakeLists.txt:37-40}` | 根 :210-215 解除子图；节点实调 `lib/phase1/noise/*`（根 :490-493）；目标实名为 `p1noise_under_test` | **M6a-D-008**（注释层）+ **M6a-C-003** |
| 「成功后 `manifest["status"]="complete"`」＋把 `check_prod_reachability.py:42` 引作生产锚 | `lib/phase1_session/README.md:89-111`/`:201-203` | `p1_session.cpp:442-444` 成功路径写 `"partial"` + availability；:42 实为 `BANNED_CLI_INCLUDES` | README 面 **M6a-C-003**；节点接线主事实移交 M3a/L05 |
| 「projection 硬编码 TAN」「P3_WCS_UNSUPPORTED 现无产生点」「PA 未接线（:160）」 | `lib/phase3_proj/{README.md:93-97,memory.md:100-102}` | `p3_wcs.cpp:66/:72` 有真实产生点、:104 显式拒非 TAN、:110 注释自述非硬编码 | **M6a-I-001** |
| 「唯一权威签名头（59 行）…239 行实测」 | `lib/phase2/README.md:10-11` 等 | §2.1 七项全部不符 | **M6a-I-001** |
| 「36 checks / 36/36 PASS」 | `runtime/module_loader/README.md:12/:39`、`ABI_003:83` | 数量相符但 CI 收集不到该脚本（§2.5） | **M6a-I-003** |
| 「正式 Browser 数据源…仅通过 astro_image_io.dll 的 AIO HiPS Reader API」 | `hips_browser_backend.h:2/:4-5` | 该目录为独立 project，根 CMake 零引用；根图对应 target 是 `astrocs_aio`/`astrocs_hips` STATIC + `astrocs_io` SHARED | **M6a-D-015**（注释错文）+ 表述归 **M6a-C-003** |
| 「coverage_ok 1=coverage 头/数据一致」 | `p3_output.h:33`、`DATA:1978`、`PHASE3_FITS_IMPL:142` | 写路径恒 1（:374），真值只在 verify（:491/:538/:5209） | **M6a-I-002** |
| 「TileCache 共享读+互斥加载/LRU；I/O 线程 1」 | `PHASE3_MODULE_ARCH.md:16/:24-25/:29-31` | 实现每 worker 私有无锁 FIFO、无预取线程、上界式不含 n_workers | **M6a-C-002** |
| 未被本域对账覆盖者（`docs/modules/*` 13 份「本版实测」、`ARCHITECTURE.md` 版本事实表） | docs 面 | 未逐条判定（边界） | **移交 M6b**（F00-05 同主题） |

## 6. 行号漂移处置（会话期间源码被并发修改）

- 事实：`lib/core/src/module_adapters.cpp` 在复核期间由 5541 → 5552 行（同一次 read 前后行数不同），多个并发任务在改源码。
- 处置原则（本域全程执行）：**锚一律写 path::符号，行号只作复核时点参考值，绝不以行号不符为剔除理由**；证据以「注释原句 / 代码片段」逐字复核，符号名可 grep 定位者即视为有效。
- 本域叶子锚错记（已按符号重取）：
  1. `hips_browser_backend.h` 叶子给 `:140` → 文件实仅 91 行，证据实在 :8-9（M6a-D-015）；
  2. `DATA_SEMANTICS.md` 叶子给 `:1977` → 实在 :1978；
  3. `p3_session.cpp` 叶子按旧版给 `:375-378 {"coverage_ok"…}` → 现该处为 `coverage_stats`，字段已停发（据此判 L14-008 为部分修复）；
  4. `dpsf_psf.cpp` 文件头 7 锚偏 +7/+38（§2.3，移交 M3b-E-01）；
  5. `lib/astro_image_io/tests/p1hips/CMakeLists.txt:37` 与 `lib/phase1_session/README.md:21-25` 引的根 CMake 锚偏 +55~+101（归 M6a-C-003 表述面）；
  6. `STAR_PSF_ALGORITHMS.md:132` 自称 934 行基准 vs 实测 1124（归 M3b/M6b 锚族）。
- 建议（跨域）：机器侧 DOC-LINE-ANCHORS 门只验「可解析 + 不越界」，不验符号是否在该行 ⇒ 提出「锚必须含符号名且符号在该 ±N 行内命中」的判据，由 M5b/M6b 落地。

## 7. 剔除与降级显式清单（不得静默丢弃）

**整条不重复登记（已由他域定稿，共 5 条）**
1. L13-002 → M6b-E-004（不存在文档被当权威，含本域 4 处实例与 133 处计数）。
2. L13-004 → M3b C_DOC_CODE_GAP/p1（mag 双式 + `StarRecord.mag 注释:238`）。
3. L13-005 → M3b-C-04（PSF [N,9] 两列无缓冲 + 两份 L1 A/B 标签相反）；本域 §3.4 会签。
4. L13-006 → M3b-E-01（sdet/dpsf 文档锚成批漂移）；本域出 §2.3 补证。
5. L14-007 → M5a-G-006 + M5a-D-001（execution_options 唯一来源、hardware_concurrency 第二源、sampler.h:54 半句）。

**部分剔除（主体他域定稿，本域只登记残余）**：L13-001（科学面→M1a）、L13-017（残句/流水→M3b-D-01）、L13-018（star_detector.h 零契约→M3b-D-01）、L14-002（不存在文档族→M6b-E-004；phase2 残断 token→M4-D-03）、L14-003（Phase2 实例→M4-D-03）、L14-013（文档「并行路径不经此锁」→M4-C-05）。

**降级（4 条，含理由）**
| 条目 | 原 | 新 | 理由 |
|---|---|---|---|
| L13-001 注释层 | P0 | P1 | 科学裁决 P0 已由 M1a 定稿；本域只剩注释载体与未登记 ID |
| L13-011 | P1 | P2 | 现行生产 SNR 通道一律 `status!=0` 剔除，宽松口径仅在遗留族与不在根图的 orchestrator ⇒ 无错值证据；合同意图待裁（§10） |
| L13-012 | P1 | P2 | README 已订正；残余为 fail-closed 的通道不可达（不产错值） |
| L13-005 / L13-006 / L13-002 / L14-002 的部分面 | — | — | 不是降级而是**不重复登记**（见上两组） |

**升档（2 条）**
| 条目 | 原 | 新 | 理由 |
|---|---|---|---|
| L14-014 | P2 | P1 | 新证：UT-ABI 门用 `unittest discover` 而该脚本无 unittest 用例 ⇒ README 的「36/36 PASS」在 CI 面零执行落点（不止是快照数字陈旧） |
| L14-001（拆出的现状陈述面） | D_COMMENT/P1 | I_DOC_HYGIENE/P1 | 类别改判：「硬编码 TAN」「无产生点」是与代码相反的文档陈述（非注释卫生），归文档准确性 |

**类别改判（3 条）**：L14-001 → I_DOC_HYGIENE/P1（原报 D_COMMENT）；L14-006 → C_DOC_CODE_GAP/P1（原报 D_COMMENT，因主体是 FROZEN 架构文档 vs 实现）；L14-008 → I_DOC_HYGIENE/P1（原报同）保留类别但改判为「部分修复 + 残余」。

## 8. 已修复（并发提交期间）表 —— 不进 findings/，另记回归测试

| 项 | 原条目 | 修复证据（复核时点） | 有无回归测试 | 后果 |
|---|---|---|---|---|
| HiPS 会话/写出节点不再发布恒真 `coverage_ok`，改发 `coverage_stats`；真值改由 verify 节点（`p3_output_verify_ex` → `vres.coverage_ok`）上报 | L14-008 | `p3_session.cpp:410`（只有 coverage_stats）、`module_adapters.cpp:5120-5124`（写路径不发）、`:5198-5212`（verify 发真值） | **无**：三处自测仍断言常量（`tests/unit/p3_output_test.cpp:75/:85`、`tests/unit/p3_assembly_test.cpp:121`、`tests/backend/test_p3_output.py:160`），verify 侧真值无「不一致 ⇒ covok=0」的负例 | 已在 M6a-I-002 的「建议处置③」写明须补负例（不另立 F_TEST_GAP，避免同事实双计） |
| `lib/drizzle/README.md` 已把 nested 订正为「仅 1=NESTED，0=RING 硬拒绝」 | L13-012 | `lib/drizzle/README.md:52` | 无（nested 缺省行为无测试断言） | 残余（types.h 词表 + 缺省 0）定稿于 M6a-C-004；缺省行为补测在其「建议处置」 |
| `p3_wcs.cpp` 已改为入口显式拒非 TAN（P3_WCS_UNSUPPORTED 有真实产生点），三件套未回写 | L14-001 的一项 | `p3_wcs.cpp:59-82/:89-94/:110` | 有（`p3_wcs_test` 中 `P3_WCS_UNSUPPORTED` 断言） | 故该项不算缺陷，只算**文档滞后**，归 M6a-I-001 |

本域**无「已被修复（整条）」**条目：35 条叶子中没有一条在复核时点完全消失；两条为部分修复（L13-012、L14-008），其余为仍成立或已由他域定稿。

## 9. 移交清单（跨域项，附已核实证据，不代签）

| 移交给 | 事项 | 已核实证据 |
|---|---|---|
| **M2b**（L03 HiPS） | ①AIO-002「原子发布原语内建」三处注释（`module_adapters.cpp:23-25/:2417-2421/:3911-3913`）的 HiPS 落盘侧后果；②`aio_hips_reader.cpp:38-41` 注释称不猜层级而 :105/:155/:490 按 hips_order 组路径；③`aio_hips_reader.cpp:52/:56` 允许 `hips_order∈[0,29]` 与 Phase3 侧 20 上限的可达链 | `aio_publish.cpp` 只编 `astrocs_p1_hips_writer`；`aio_hips_writer.cpp:185-186` remove+create；`DISP-HIPS-004` 已登记「无原子发布」 |
| **M2a / M1a** | `docs/modules/calibration.md` 的编译归属陈述与测试装配现状不符：:111 称 `test_photometry_apply.cpp`「未挂接 CMake 测试目标」，实际已挂（`tests/unit/CMakeLists.txt:744-755` 编译 `src/photometry_apply.cpp` 并 `add_test(NAME cal_photometry_apply …)`）；:118「未编译：dark_optimizer.cpp、photometry_apply.cpp」就生产库/模块 DLL 成立（`lib/calibration/CMakeLists.txt:20-25` 的 `CAL_PROD_SOURCES` 只含 4 TU），就测试目标不成立 ⇒ 措辞需按 target 区分；DISP-CAL-006（`docs/algorithms/CALIBRATION_ALGORITHMS.md:407`）与 `lib/calibration/{README.md:156,memory.md:20}` 同口径 | 本代理复核：`apply_photometry` 的生产直调点在 `lib/core/src/module_adapters.cpp` 中 **0 命中**（故「未接线」仍成立，只是「未编译」措辞过宽） |
| **M3a / L05** | `lib/phase1_session/README.md §3` 称成功写 `status="complete"`，实为 `p1_session.cpp:442-444` 写 `"partial"` + availability 八域；§7 把 `check_prod_reachability.py:42` 当生产锚（该行是 `BANNED_CLI_INCLUDES`） | 见 §5 表行 5 |
| **M5b**（门） | UT-ABI 门 `python3 -m unittest discover -s tests/abi` 收集不到 `test_secure_loader.py`（该文件无 TestCase）⇒ 36-check 编排 CI 零执行；另 DOC-LINE-ANCHORS 门不验符号位置 | §2.5 复算；`ci/checks.json:1642-1658` |
| **M6b**（文档体系） | `docs/algorithms` 三件套与 `docs/modules/*` 的「本版实测/实测 N 行」族（本域已复算 7 项行数）；ALG ID 交叉误标 `p3_wcs.h:41`（引 ALG-P3-003 vs 本文件 ALG-P3-002） | §2.1、§4 |
| **M3b**（已定稿域） | L13-006 的 7 锚偏移实测表（§2.3）与 `STAR_PSF_ALGORITHMS.md:132` 的 934→1124 行，建议并档进 M3b-E-01 的证据面 | §2.3 |
| **M1a**（已定稿域） | L14-009 的 AIT/CAR 域守卫复算（§2.4）与其 oracle 共源条并档；`spec->max_abs_crval_dec_deg` 无强制点（M6a-D-012）可作其 §7.3 整改的输入 | §2.4 |
| **前台** | ①「控制参数被计算、被记录、但不参与执行」主题在本域再添一例（M6a-C-001，与 L03-001/L05-013/L09-010/L11-001 同族）；②「机器门 fail-open」再添 CON-COMMENTS（P0）与 UT-ABI 两例；③跨域缺口：`DISP-WCS-007/008` 未登记（需 owner 裁决落登记面） | §2.5、§3.2、M6a-D-001 |

## 10. 待复核 / 无法判定（§6 遗留）

1. **SNR_QF_PSF_OK 含 status==3 是否刻意宽松**：`snr_estimator.h:337` 与 :77/:398 互斥，orchestrator:4392 实现「0 或 3」；仓内无文档裁定合同意图 ⇒ 归 M3a/L07 交叉裁定（本域已按注释层残余定 P2）。
2. **「36/36 PASS」这一轮是否真跑过**：脚本本身 `FAILURES` 非空即 exit 1（可跑），但历史日志在 `run/**`（免报区），且审计禁执行 ⇒ 只能判定「CI 面零执行落点」，不能判定「从未通过」。
3. **1e-4 px 门的绑定 fixture 集是否含 F2 强畸变场**：决定 M1a P0 的实际产品影响，需运行 p1wcs oracle（本域禁执行）⇒ 前台或 M1a 复验。
4. **AIT 域守卫的第三方基准**：宪章 §7.3 要求独立 Oracle；本域用闭式恒等式与数值代入判定（未执行任何程序），若需 `wcslib\|astropy CAA/CAR` 对拍须由有执行权限的一方补。
5. **`AstroCS.wiki` 是否属现行权威**：`AGENTS.md` 把它列为「用户/资料区，原地保留、gitignore」，而 `coverage.h:5`/`stage2.cpp:5` 等以「wiki Phase2_Architecture」为语义依据；本域按「wiki 不在仓内可核真源」定 E_TRACE（M6a-E-001），若负责人确认 wiki 属权威层，该条应改判为路径落位问题。
6. **`p3_wcs.h:41` 引 ALG-P3-003**：可能是有意的跨文档引用（投影合同另册），本域仅记为疑点交 M6b，不定性为错标。
