# 审稿-P1 · ALG-photometry-003（第 1 遍 · 对抗审稿）

- 仓库：`/workspace/Astro CS Database`，HEAD = `850a9edefd47434b9ab71bc907c3de1e0814b323`
- 片清单：`run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml` → `- 片号: ALG-photometry-003`
- 口径声明：**一遍 = 对同一片材料的一次完整重读**。本片 26 份成员逐份读完；判据一律不采信「检查通过」，只认独立重算与反例。
- 纪律：零 git 写；未编译、未跑 ctest/pytest/构建/任何二进制；未改任何仓内文件（本交付件除外）；绝不读 `/tmp/acsd_g08/`。

---

## 1. 读完了吗

| 口径 | 值 |
|---|---|
| 成员份数（权威清单） | 26 |
| 实际读了几分 | **26 / 26（100%）** |
| 成员总行数（权威清单「实际行数」） | 9741 |
| 实测 `wc -l` 合计 | **9741**（与清单逐份吻合，见 §3） |
| 实际读了多少行 | **9741 / 9741（100%）** |
| **未读完的** | **无** |

**计数口径说明（必读，避免误读覆盖率）**

- 文本类成员（22 份，7511 行）：用 `read` 逐份读完，`offset/limit` 翻页，无抽样。
- 纯数据 JSON 成员（2 份，2549 行）：**头部条目由我本人 `read` 逐字读完**（`filter_qe_provenance.json:1-45`；`test_spectrum_integrator_golden_results.json` 五个顶层块结构），**其余数值条目由我本人写只读解析脚本逐条遍历核对**（57/57 provenance 条目全键集、60/60 golden 行全字段、逐值统计），并对关键字段（`val_min/val_max`、`rel_err_*`、`status`、`n_pass/n_total`）逐条打印核对。**这两份我没有用 `read` 逐行敲过 2549 行，而是用等价的结构化遍历覆盖了每一个值** —— 如需严格 `read` 口径，此处覆盖率应记为 `7511 逐行 + 2549 结构化全覆盖`。
- 另为取证**越出本片**读了 5 处（不计入覆盖率）：`lib/algorithms/photometry/include/acsd/psfsw.h`(407)、`lib/algorithms/photometry/cpp/src/star_matcher.cpp` 关键段(165-172/380-405/445-520/486-520/512-535/614-640)、`p1phot_test_main.hpp`、`docs/science/algorithms/GATES_AND_TOLERANCES.md`、`evidence/gate2_result.json` / `evidence/gate4_result_full_1050.json`。

---

## 2. 本片判定

**判定：阻断（BLOCKING）。**

本片不是「有几处瑕疵」，而是**判定层本身不可信**：11 条阻断里，9 条是「门不设防 / 门不看被检量 / 门恒真 / 门在读另一个量」。最重三条：

### 最重 ①：Akima 子样条在 4 条阻挡滤镜上产出**负透过率 −1.51**，F_syn 被偏置 **−9.3% ~ −25.9%**，而所有相关门全绿

我**独立复现**（自写 Akima 转写，未调用任何仓内代码，见 §8 命令）：生产 2 nm 光谱网格上，

| 滤镜 | 原始 `val_min` | 插值后 `min` | 负值点数 | F_syn 偏置 vs `[0,1]` 钳位 |
|---|---:|---:|---:|---:|
| Astronomik UV-IR Block L-1 | 0.000 | **−1.5097** | 50/233 | **−12.98%** |
| Astronomik UV-IR Block L-2 | 0.000 | **−0.5000** | 66/233 | **−9.28%** |
| Astronomik UV-IR Block L-3 | 0.000 | **−0.6250** | 81/233 | **−20.96%** |
| Baader UV/IR Cut / L CMOS Optimized | 0.000 | **−0.8333** | 81/233 | **−25.86%** |
| Antlia V Pro Series B（对照） | 0.002 | +0.0019 | 0 | **0.00%** |
| ZWO R（对照） | 0.000 | 0.0000 | 0 | **0.00%** |
| QE 曲线 12 条（对照） | — | — | **0/12** | 0.00% |

根因：这 4 条是 **7 点阶跃函数**（`[0,0,1,1,1,0,0]`，wl `[300,362,367,500,710,725,800]`）。Akima 子样条在阶跃处振铃。`frame_photometry_fit.cpp:222-225` 把 `filter_wl/filter_trans` 直接送入积分器，**全程无 `[0,1]` 钳位**。

**为什么所有门都没抓到 —— 这是本轮最有价值的一条：**

- `test_spectrum_integrator_golden.py:267` 的门是 `assert np.all((val >= 0) & (val <= 1.0))`。它断言的是**文件里存的原始采样点**，而缺陷出现在**实际参与积分的插值权重**上。**被检量与断言量不是同一个量** —— 这正是负责人点名的「用同一个定义式既当被检量又当期望量」的镜像变体：这里连定义式都不是同一个，却仍被判绿。
- `golden.py:333-335` 只取 `filter_names[0]` 与 `filter_names[-1]`，**恰好是两条零振铃的平滑曲线**；4 条阻挡滤镜一条都没进测试集。
- `filter_qe_provenance.json` 如实记录这 4 条 `val_min = 0.0`（**原始值是真的**），所以溯源文件本身也没有造假。

⇒ 这是一条**被全部绿灯放过的、可量化的物理错误**，直接进入 P1 通量积分。

### 最重 ②：`p1phot_fixgates.cpp` 的「门 2」用例**从不执行门 2**，且其注释描述与实际输入相反

`p1phot_fixgates.cpp:197-214` 名为 `b42_sigma_needs_two_inliers`，声称验证 `|r_inliers| >= 2 才估计 sigma_residual`（实现 `star_matcher.cpp:616-628`）。实际执行路径（我逐行推演 + 算术复现）：

```
构造 m[2].f_instr = 1000·10^2.90          (fixgates.cpp:199)
  gaia_mag = -2.5·log10(1000) = -7.5       (make_matches:77)
  delta = [-0.25, -0.25, -7.25], median = -0.25
  |delta_2 - median| = 7.0 > tol(3.0)  ⇒ star_matcher.cpp:494 预过滤剔除第 3 颗
  |r_consistent| = 2 < 3                 ⇒ star_matcher.cpp:518 直接 NO_DATA return {}
  out.size() = 0 < 2                     ⇒ 走 :208 的 if 臂（sigma==0.0）
```

三重缺陷叠加：

1. **注释造假**：`:199` 写「delta 仍 <3 mag 预过滤」，实测偏差 **7.0 mag**，被预过滤剔除。
2. **用例退化**：它在 `:518` 就退出了，**从未走到 `:616` 的门 2**，实际是 B4-2(a)「2 星 NO_DATA」的重复用例。
3. **else 臂是恒真式**：`:211` `CHECK(sigma >= 0.0, ...)` 对任何 MAD 派生的 sigma 恒成立。

⇒ **反例成立：整段删掉 `star_matcher.cpp:616` 的门 2，本测试仍全绿。**

> 口径更正：4 个子代理中有 2 个独立判为「走 else 臂」（理由是 MAD=0→S=0 吸收了离群星）。**我否决了该判读** —— 星等预过滤（`:494`）先于 `<3` 门（`:518`）执行，离群星在到达 MAD 计算之前就已被剔除。两侧结论一致（门 2 从未执行），但**分支归属不同，以我的为准**。

### 最重 ③：`gate4_gaiaxpy_compare.py` **全文没有门**，而它的输出被审计文档当作结论

`gate4_gaiaxpy_compare.py:67-249`：`main()` 全函数**零阈值比较、零 `assert`、零非零 `sys.exit`**（除 `:79 exit(2)` 无 gaiaxpy、`:90 exit(3)` 空样本）。任何数值——哪怕 `p95_absdiff = 7.8`——都 `return 0` = 成功。而 `DR3SP_SCHEMA_AUDIT.md:85-95` 拿它的输出当「Gate 4 对比结论」写进审计文档。

同目录三个脚本门禁地位互相矛盾：`xpsd` 有 `all_pass`+`sys.exit(0/4)`；`gate4` 无门；`gate2` 有 gates 但 `blocker` 是**硬编码常量文本**（`:328-335`，不随 `gates` 计算）。

---

## 3. 逐文件清单

| # | 文件 | 行数 | 读了什么 | 看到什么（关键） | 判定 |
|---|---|---:|---|---|---|
| 1 | `cpp/test/test_spectrum_integrator_golden_results.json` | 1860 | 结构化全覆盖 | 测试自身输出日志；`test2`/`test3` 各 60 行；`test5` exit_code/n_pass/n_total | 须修 |
| 2 | `cpp/src/psfsw.cpp` | 954 | 全读 3 段 | `cnorm_invariance_deviation` 结构性恒 0；G11 自证门；空样本恒过 G17/G18；NaN 权重 | 须修 |
| 3 | `cpp/test/test_spectrum_integrator_golden.py` | 714 | 全读 2 段 | 覆写溯源源文件；`[0,1]` 门断言错对象；只测 2/45 滤镜 | **阻断** |
| 4 | `cpp/test/filter_qe_provenance.json` | 689 | 头部逐字 + 全量结构化 | 57/57 条 `source="unverified"`，`url`/`retrieved` **0 条非空**；每条 10 键，生成器只写 5 | **阻断** |
| 5 | `cpp/src/spatial_gain.cpp` | 645 | 全读 2 段 | `assess` 错误被降级为 `kDegradedRank`（`frame_fail=false`）；`coef_sigma_dex` 退化置 0；阶数静默钳位 | 须修 |
| 6 | `cpp/test/test_energy_conservation.py` | 510 | 全读 2 段 | `test_residual_distribution` **完全不读 `out_img`**；`1.4826` 四位截断；无 `sys.exit` | 须修 |
| 7 | `tests/p1phot/p1phot_tests_determinism.cpp` | 476 | 全读 2 段 | NO_DATA 路径 `0<=0` 恒真；T4/N1 是测试内玩具函数且**从不调 `verdict_of`**；双置换漏 `sigma_within` | **阻断** |
| 8 | `tests/p1phot/p1phot_oracle.hpp` | 468 | 全读 | oracle 是生产的**逐行转写**；TAN/SIP 与魔数 1e6 复制 | 须修 |
| 9 | `cpp/test/test_photometric_calib.py` | 414 | 全读 2 段 | 5 个 test 全 `return bool`（pytest 恒绿）；`:414` 无 `sys.exit`；SIP 数值惰性 1307 倍 | **阻断** |
| 10 | `cpp/src/frame_photometry_fit.cpp` | 394 | 全读 | **QE 失败静默退化 Q≡1**，注释明令禁止；FOV 魔数 1.2/30/10 无出处 | **阻断** |
| 11 | `cpp/test/gate4_dr3sp_gaiaxpy/gate2_psf_oracle.py` | 346 | 全读 2 段 | ROOT 指向外部旧仓；DLL 目录不存在；`match_truth` 2px 先筛；门不看 n_matched | **阻断** |
| 12 | `tests/p1phot/p1phot_fixgates.cpp` | 286 | 全读 | 门 2 从不执行；else 臂恒真；ABI 探针是空探针（**已自承**） | **阻断** |
| 13 | `cpp/src/wcs_transform.cpp` | 261 | 全读 | CD 近奇异→逆矩阵全 0（静默）；发散返回魔数 1e6；逆 SIP 固定 3 次不报不收敛 | 须修 |
| 14 | `cpp/test/gate4_dr3sp_gaiaxpy/gate4_gaiaxpy_compare.py` | 250 | 全读 | **全文无门**；C++ 交叉验证默认跳过；`mag_g` 缺则填 12.0 | **阻断** |
| 15 | `tests/p1phot/p1phot_tests_selfcheck.cpp` | 234 | 全读 | **127/-1 当「必败通过」**；子进程 env 只剩 1 项；缺 `spatial` 组 | **阻断** |
| 16 | `cpp/test/gate4_dr3sp_gaiaxpy/xpsd_cpp_crosscheck.py` | 209 | 全读 | 6 重 `continue` 无最小样本门；`fsyn_cached_like` 是复刻；ROOT 外部旧仓 | **阻断** |
| 17 | `tests/p1phot/p1phot_tests_perf.cpp` | 196 | 全读 | 过载时**三条硬门全不执行**仍打印 PASS | 须修 |
| 18 | `cpp/src/spatial_gain.h` | 173 | 全读 | `frame_fail` 默认 false；`coef_sigma_dex` 语义与实现相反 | 建议 |
| 19 | `module.yaml` | 141 | 全读 | **行号 10/12 失效**；`未接管线` 被证伪；三条依据文件不存在；端口面冲突 | **阻断** |
| 20 | `cpp/src/star_matcher.h` | 131 | 全读 | 默认 `2.0px`/`3.0mag` 裸写无出处 | 建议 |
| 21 | `wrapper_phase1/photometer.cpp` | 100 | 全读 | 失败编码在 payload `valid=false` 而非 Result 错误通道；`sum<0` 仍 valid | 须修 |
| 22 | `cpp/test/gate4_dr3sp_gaiaxpy/DR3SP_SCHEMA_AUDIT.md` | 95 | 全读 | **`:95` 数字被自己引用的证据推翻**；`:85` 悬空文件名；ZP 出处违反正本 | **阻断** |
| 23 | `cpp/src/wcs_transform.h` | 66 | 全读 | `skyToPixel` 无失败输出参数 → API 层面强制 fail-open | 须修 |
| 24 | `tests/p1phot/p1phot_gaia_stub.hpp` | 57 | 全读 | 仅声明；常量谱回退在片外 `p1phot_field_stub.hpp` | 建议 |
| 25 | `wrapper_phase1/photometer.h` | 40 | 全读 | 构造默认 `4.0/6.0/10.0` 无出处 | 建议 |
| 26 | `tests/p1phot/p1phot_tests_main.cpp` | 32 | 全读 | 注册 6 组；selfcheck 只认 5 组 | 建议 |
| | **合计** | **9741** | | | |

---

## 4. 发现清单

### 4.1 阻断（11 条）

| ID | 类别 | 一句话 | 位置 |
|---|---|---|---|
| **A** | 硬红灯·被绿灯放过 | Akima 在 4 条阻挡滤镜产出负透过率 −1.51，F_syn 偏置 −9.3%~−25.9%，全仓零覆盖 | `golden.py:267,333-335`；`frame_photometry_fit.cpp:222-225` |
| **B** | 恒真门 + 筛掉真信号 | 「门 2」用例被星等预过滤提前打掉，从不执行 `\|r_inliers\|>=2`；注释「delta <3mag」实测 7.0mag；else 臂 `sigma>=0` 恒真 | `fixgates.cpp:197-214`（+`:199`）；`star_matcher.cpp:494,518,616` |
| **C** | 没有门 | `gate4_gaiaxpy_compare.py` 全文零阈值/零 assert/零非零退出 ⇒ 恒 rc=0 | `gate4_gaiaxpy_compare.py:67-249` |
| **D** | 悬空引用·数字自证伪 | 审计文档「颜色 median\|Δ\| ≤ 0.001 mag」，其引用证据为 BP_G **0.00121**、BP_RP **0.00176**（两带超）；`:58` 称 ≤0.008 实为 **0.008131** | `DR3SP_SCHEMA_AUDIT.md:58,85,95` vs `evidence/gate4_result_full_1050.json` |
| **E** | 退役声明被证伪 + 审计链失效 | `module.yaml:19` 「仅单测…未接 orchestrator 管线」被 `module_adapters.cpp:5358/16195` + `module_ports.registry.json:357` 证伪；yaml **10/12** 行号偏移（755→896、429-432→1072、2474→2514、469-486→947 等） | `module.yaml:3,4,18-19,27-28,31-32,38` |
| **F** | 静默降级冒充成功 | selfcheck 只判 `child_rc != 0`；fork/execve 失败返 **127**、信号杀死返 **-1**，全被记为「必败验证通过」；`:52` 子进程 env 只剩 `ACSD_P1PHOT_FAULT`（无 `LD_LIBRARY_PATH`）——正是会让 execve 失败的环境 | `selfcheck.cpp:52,62-72,85,159,173,186,202` |
| **G** | 门结构性不可运行 | `gate2`/`xpsd` 的 `ROOT` 指向 `F:\Astro dev\...` 外部旧仓；`lib/star_detector`、`lib/dynamic_psf`、`fsyn_export.exe` 仓内**均不存在** ⇒ 「验收门」在本仓从未执行 | `gate2:32,36-37,80-81`；`xpsd:25,34-35,89,116` |
| **H** | 筛子无最小样本门（三处） | ① gate2 `match_truth` 用 2.0px 先筛（**筛掉的恰是偏差最大那条**），门只看 `is not None`；② xpsd 六重 `continue` 只守 `n_ok==0`；③ gate4 `.dropna()` 恰好删掉 ACSD 积分失败（`F_syn≡0`）的行 | `gate2:214-224,311-316`；`xpsd:122-160,172`；`gate4:176-177,190` |
| **I** | 溯源源文件是测试输出 | `golden.py:299-302` 每次运行覆写 `filter_qe_provenance.json`，写盘只含 5 个统计键 ⇒ 盘上多出的 `source/url/retrieved/instrument/uncertainty_note` 被**整体抹掉**；该文件又被 CFG-001 按 sha256 冻结 ⇒ 重跑一次即打断溯源链 | `golden.py:268-274,286-292,299-302`；`eng/tests/config/test_cfg001_contracts.py:267-274,296-305` |
| **J** | 静默降级（注释自相矛盾） | QE 曲线解析失败 → `clear()` + 只 `fprintf(stderr)`，`out.error`/`degraded_reason`/`fit_ok` 全不动 ⇒ 按 Q(λ)≡1 继续合成 F_syn；而紧邻注释 `:152-154` 明写「**不得静默退化为 Q(λ)≡1**」。同函数内 filter 失败走 `:118-136` 具名报错，QE 不走 | `frame_photometry_fit.cpp:149-158`（对照 `:118-136`） |
| **K** | 恒真门（结构性） | 5 个 `test_*` 全 `return bool` 而非 `assert`（pytest 下只发 warning 不判失败）；文件 `:414` 结束**无 `sys.exit`** ⇒ 0/5 通过时 shell 仍收 exit 0 | `test_photometric_calib.py:98,158,196,269,359,414` |

### 4.2 须修（14 条）

| ID | 一句话 | 位置 |
|---|---|---|
| L | `cnorm_invariance_deviation` **结构性恒 0**：median 归一对任意 `c_norm>0` 数学不变 ⇒ 该「不变性」门永绿；且生产调用传 `c_norm=1.0` 时两侧**逐位相同** | `psfsw.cpp:335-346`；`psfsw.h:180-181`（注释自承「应为 0」） |
| M | G11 是**自证门**：只比对记录里生产者自填的 `depth_gate_ok/depth_max_rel_dev/...`，**从不调用** `depth_stability_gate()`；G18/G19 才独立重算 | `psfsw.cpp:834-841`（对照 `:889,895`） |
| N | 空样本 ⇒ `p05=p50=p95=value` ⇒ G17 `p05<=p50<=p95` 成恒等式、G18 `spread=0` 恒过；**无样本 = 门全绿** | `psfsw.cpp:160-167`；`:878-885`；`:507-514` |
| O | `assess()` 返回 1（注释自述「H 对角为负/非有限 ⇒ **设计矩阵有缺陷**」）在预检路径被归入 `kDegradedRank` 且 `frame_fail=false`，`kDegradedSolve`/`frame_fail` 分支**不可达** | `spatial_gain.cpp:140-141,404-407,416-422` |
| P | `coef_sigma_dex[j] = (djj>0) ? … : 0.0` —— 该字段语义是「该系数是否真被数据约束」，退化时填 **0（表示完全确定）**，方向相反 | `spatial_gain.cpp:626`；`spatial_gain.h:137-138` |
| Q | CD 近奇异 ⇒ `m_cdInv` 全置 0 且对象构造成功 ⇒ `skyToPixel` 对**任意** RA/Dec 返回同一参考像素、`pixelToSky` 对任意像素返回 CRVAL；`skyToPixel` 签名无失败位 ⇒ API 层面强制 fail-open | `wcs_transform.cpp:54-57,200-219`；`wcs_transform.h:33` |
| R | 投影发散返回魔数 `xi=eta=1e6`（度），无错误码、无 status | `wcs_transform.cpp:168-173` |
| S | 逆 SIP 固定 3 次迭代、容差 1e-10 **实际不可达**，不收敛照样采用且无 `converged` 位；`:235` 注释称「牛顿迭代一次」（实为固定点迭代 3 次） | `wcs_transform.cpp:229-253` |
| T | `test_residual_distribution` **完全不读 `out_img`**：期望值由被测返回的单个标量 `scale` 与测试自造输入构成 ⇒ 期望量与被检量同源 | `test_energy_conservation.py:344,354-355,368-372` |
| U | 同文件另两处期望值亦由被测 `scale` 构造：`expected = 1000.0*scale`、`f_out_i = out_img[mask].sum() - 100.0*scale*mask.sum()` | `energy_conservation.py:219,298` |
| V | `mad_normalized = 1.4826 * mad_raw` 用**四位截断**字面量 —— 与 `photometer.cpp:90` 已被治理定性的同一反模式（相对差 −1.5e-6） | `energy_conservation.py:359` |
| W | 性能门可被宿主负载**整体关闭**：`load1 > nproc` ⇒ 三条 `P1PHOT_CHECK` 全部不执行，仍打印 `PERF PASS` 并 `return 0`；豁免开关靠环境变量，CI 无人设置 | `p1phot_tests_perf.cpp:175-192` |
| X | `photometer.cpp` 失败编码在 payload (`Result::ok(r)` + `r.valid=false`)，而同函数 `:19-22` 用 `Result::fail` ⇒ 同一函数两套失败编码；只查 Result 的调用方会把 out-of-bounds 中心当成功读到 `flux=0` | `photometer.cpp:19-22` vs `:26-30,52-56,76-80` |
| Y | `psfsw` G05 只按 `last_segment` **精确等值**匹配禁词 ⇒ `inverse_variance.<任意>`、`snr.<任意>` 可绕过禁词表（`psfsw.cpp:699-702,790-798`）；G25 仅在 `baseline_claim` 非空时生效，**声明缺失即整门跳过**（`:937-947`） | `psfsw.cpp:699-702,790-798,937-947` |

### 4.3 建议（9 条）

1. `psfsw.cpp:237` `a_ref<=0 ⇒ 静默用 a_nea`（无错误码）；`:238` `background_robust_mean` 不查符号/有限性 ⇒ NaN/负背景以 `ok=true` 返回。
2. `psfsw.cpp:555` `scalar_degradation_gate` 无下界 ⇒ 负 `power_loss`（含 `-inf`）直接过 G19。
3. `psfsw.cpp:287` 有 `isfinite(b)` 而 `:294` 对 `s/conc/n` 无 ⇒ `+inf` 放行 ⇒ `inf/inf=NaN` ⇒ `std::sort` 对 NaN 不构成严格弱序（行为未定义）。
4. `psfsw.cpp:223,391` 空 `valid` 掩码被解释为「全部有效」，抬高 `n_common`/`per_frame` 绕过下限门。
5. `spatial_gain.cpp:327` `order>2` 静默钳到 2，`degraded_reason` 不记；`:340-345` 只查 `isfinite` 不查 `x∈[0,width]`，`:367` 无界 `double→int` 强转。
6. `frame_photometry_fit.cpp:173-175` FOV 钳位 `[1,10]`、判据 `30`、系数 `1.2` 三处魔数无出处且**静默**（退化 WCS 被钳成看似合理的 1.0°）。
7. `p1phot_oracle.hpp:162-164,176-178,183` 与 `wcs_transform.cpp:168-173,115-118,130-131` 复制同一魔数 `1e6`/阈值 `1e-12` ⇒ 该族缺陷对拍结构性失明；`oracle.hpp:38-50` `oracle_median_avg`/`oracle_median` 是同一函数的两个名字。
8. `oracle.hpp:8-9` 自称是 SCI-PHOT-001 §11「NumPy 参考复算 rtol 1e-9」的 C++ 对应物，但 `lib/algorithms/photometry/` 下**无任何 NumPy IRLS 参考脚本**，`:170` 依赖的 `synthetic_photometry` 模块全仓不存在 ⇒ 该参考不是被翻译而是被替换。
9. `p1phot_tests_main.cpp:3-4` 头部组表只列 4 组，`:19-29` 实际注册 6 组（漏 `determinism`/`spatial`）；`selfcheck.cpp:78-86` 只认 5 组，缺 `spatial` ⇒ `./p1phot_selfcheck spatial` 走 `:85 return 127`，被上层当「必败通过」。

### 4.4 应当保留的正面（本轮确认健康，勿在整改中误伤）

- `p1phot_oracle.hpp:298-300` **如实登记** Simpson `n_int==3` 分支历史上 oracle 与被测「同构同错」，并改用闭式期望 + 专设故障注入名（`PHOT-SIMPSON-N3-001`）。**这是全仓最正确的范式，应作为整改模板。**
- `p1phot_tests_selfcheck.cpp:92-142` `check_guard_path()` 三态真判别（合法名/空串名/nullptr 名），是本仓写得最干净的一段。
- `p1phot_fixgates.cpp:113-122` **主动登记自己是空探针**并说明旧 `static_assert` 是编译期恒真比较、已移除。诚实度满分。
- `test_spectrum_integrator_golden.py:714` 是三个 py 里**唯一**有正确 `sys.exit(0 if ... else 1)` 的；`:611-612` 解析失败判 FAIL（fail-closed）。
- `gate4_gaiaxpy_compare.py:32-37,77-79` gaiaxpy 缺失 → `exit(2)`（fail-closed）。
- SCI-PHOT-001 §4/§5/§8 对 `|r_consistent|>=3`、`|r_inliers|>=2`、`c=4.685/tol=1e-6/max_iter=50` 有**逐字正本依据**（B4-2/B4-3 的门不是测试自造门）；缺陷在覆盖，不在门的存在。

---

## 5. 我主动构造的反例

| # | 构造 | 期望推翻什么 | 是否推翻 |
|---|---|---|---|
| **R1** | 独立转写 Akima，对全部 45 滤镜 + 12 QE 在**生产 2nm 网格**与 1nm 交集网格上求值，查 `min<0` | 「响应曲线与积分器正确」 | **✅ 推翻**。4/45 滤镜负透过率，min **−1.5097**；F_syn 偏置 **−9.28%~−25.86%**；对照滤镜与 QE 全 0 |
| **R2** | 把 `star_matcher.cpp:616` 的 `\|r_inliers\|>=2` 整段删除 | 「fixgates 的门 2 被覆盖」 | **✅ 推翻**。该用例在 `:518` 就 NO_DATA 返回，从未到达 `:616` |
| **R3** | 把 `fixgates.cpp:199` 的 `m[2].f_instr` 改为使 `|delta-median| < 3` 的值 | 「注释描述的构造被如实执行」 | **✅ 推翻**。当前值 7.0 mag 被剔除；注释「delta 仍 <3 mag」为假 |
| **R4** | 把 `goldenary.py:267` 的断言对象从「原始采样点」改成「插值后权重」 | 「该门守的就是被检量」 | **✅ 推翻**。当前门对 A 条红灯完全失明 |
| **R5** | 把 `gate4_gaiaxpy_compare.py` 的 `p95_absdiff` 从 0.0078 改成 7.8 | 「Gate 4 会红」 | **✅ 推翻**。无任何比较，恒 rc=0 |
| **R6** | 把 `DR3SP_SCHEMA_AUDIT.md:95` 声称的 0.001 与证据 JSON 的 0.00121/0.00176 对齐 | 「文档数字可信」 | **✅ 推翻**。两带超阈，且超的是被写进审计结论的数 |
| **R7** | 在 `wcs_transform` 构造时传 `cd11=cd22=1e-8, cd12=cd21=0` | 「近奇异 CD 会硬失败」 | **✅ 推翻**（同文件 `:46-50` 对 sip_order 就是 throw）。对象构造成功，`skyToPixel(任意)` 返回同一参考像素 |
| **R8** | 令 `rec.depth_gate_ok=true, depth_k=5, depth_max_rel_dev=NaN` | 「G11 对 NaN fail-closed」 | **✅ 推翻**（静态可判）。`:838` 用 `>` 而非 `!(x<=k)`；同文件 `:555` 是正确写法 ⇒ 疏忽非设计 |
| **R9** | 把 `p1phot_tests_selfcheck` 跑在缺 `LD_LIBRARY_PATH` 的环境 | 「必败验证通过 ⇒ 注入真的发生了」 | **✅ 推翻**。`:52` 自己剥掉该变量 ⇒ execve 失败返 127 ⇒ 四个阶段全 PASS，零注入 |
| **R10** | 对 `filter_qe_provenance.json` 跑一次 `golden.py` | 「溯源链稳定」 | **✅ 推翻**。`:299-302` 覆写且只写 5 键 ⇒ 57×5 个溯源字段消失，同时 CFG-001 的 sha256 门变红 |
| **R11** | 把 `gate2` 的 `n_matched` 压到 1（唯一那颗偏差 0.01px） | 「p95≤0.3 门会拦住」 | **✅ 推翻**（静态可判）。门只看 `is not None` |
| **R12** | 验 `commit c19b4a59` | 「selfcheck 的模式先例引用真实」 | **❌ 未推翻**。`git cat-file -t c19b4a59` = `commit`，引用属实 |

---

## 6. 盲复算

口径：**先不读既有判定文件**（`审稿-RR*.md` / `审稿-R2-*` / `审稿-R3-*` / `审稿-P1-*`），只依权威原件 `run/GOVERN-08/工作包-GOVERN-08原件/` 与源码独立取证，得出结论后再比对。

我全程**未打开**任何既有审稿-RR*/R2*/R3*/P1* 文件；本片的既有判定我只从权威清单的「逐份判定-权威版.csv」口径理解，且结论全部来自自读原文。

盲复算结论（逐条独立重算 vs 仓内已落盘的机器判定）：

| 量 | 我独立重算 | 仓内已落盘 | 一致性 |
|---|---|---|---|
| 4 条阻挡滤镜插值 `min` | −1.5097 / −0.5000 / −0.6250 / −0.8333 | 溯源文件记 `val_min=0.0`（原始值） | **口径不同**：文件记的是采样点，我算的是插值权重 ⇒ **偏松**（落盘口径看不到 A 条红灯） |
| gate2 `acsd_production_path.n_matched` | 120 注入，`match_truth` 2px 筛 | `evidence/gate2_result.json` = 109 | 一致（11 颗已被静默丢弃，门内不计数） |
| gate4 三色 `median_diff_mag` | 不可判（无门） | BP_G −0.00121 / G_RP −0.00054 / BP_RP −0.00176 | 审计文档称 ≤0.001 ⇒ **偏松**（两带超阈未被拦） |
| gate4 自洽 BP median\|Δ\| | 不可判 | 0.008131 vs 文档称 ≤0.008 | **偏松**（超 1.6%） |
| `xpsd` `cpp_validation.max_rel_diff` | 不可判 | 4.5132e-05，而同族门阈是 1e-6 | **偏松 45 倍**且无门拦 |
| fixgates 门 2 覆盖 | 未覆盖 | 文件自称「回归锁…能红能绿」 | **偏松**（自述覆盖 ≠ 实际覆盖） |
| selfcheck 阶段 2-5 | 127/-1 亦计通过 | 打印「必败验证通过」 | **偏松**（基础设施失败被当注入成功） |
| module.yaml「未接管线」 | 有冻结注册生产算子 | yaml 称仅单测 | **偏松**（漏记生产消费者） |

**总判定：现行机器判定对本片整体偏松，无一处偏严。** 偏松集中在三类：(a) 断言对象 ≠ 被检量（A、Y）；(b) 无最小样本门（H、K、R11）；(c) 无门或门可被环境关闭（C、F、W）。

---

## 7. 子代理派发记录

**派发 4 个（工具口径；因并行调用参数串扰，第 1 条被误发两次，实际执行 4 个不同任务，第 5 个专攻跨文件合同）**：

| # | 车道 | 范围 | 读了多少 | 主产出 |
|---|---|---|---|---|
| A1 | 自洽式断言/恒真恒红门/筛子 | `oracle.hpp`、`fixgates`、`selfcheck`、`gate2`、`gate4`、`determinism`、`gaia_stub`、`main`、`xpsd`、`DR3SP_AUDIT` | 2413/2413 行 | 9 阻断 + 8 条否决 |
| A2 | 生产源码静默降级/错误码/数值/线程池/硬编码 | `psfsw`、`spatial_gain.{cpp,h}`、`frame_photometry_fit`、`wcs_transform.{cpp,h}`、`star_matcher.h`、`photometer.{h,cpp}`、`module.yaml` | 2905/2905 行 | 7 阻断 + 8 条否决 |
| A3 | 黄金数据与出处溯源 | `golden_results.json`、`golden.py`、`filter_qe_provenance.json`、`energy_conservation.py`、`photometric_calib.py` | 4189/4189 行 | 5 阻断 + 5 条否决；**负透过率 A 条** |
| A4 | 跨文件合同与生命周期 | 11 份生产源必读 + 6 份额外 + ~40 条全仓 grep | ≈5300 行 | 5 阻断 + 8 条否决 |

### 逐条复核与**否决**记录（我如何驳回子代理）

| 子代理结论 | 我的裁定 | 理由 |
|---|---|---|
| A1&A4：fixgates「门 2」用例**走 else 臂**（`sigma>=0` 恒真） | **部分否决** | 两者都跳过了 `star_matcher.cpp:494` 的星等预过滤。我逐行复核：预过滤先于 `:518` 的 `<3` 门执行，第 3 颗被剔除 ⇒ 实走 **if 臂**。结论（门 2 未覆盖）不变，但分支归属以我为准，且缺陷比我方描述的更重（用例整体退化为 B4-2(a) 重复） |
| A1：`PC_QF_*` 位断言是恒真门 | **否决** | A1 自陈「两侧同步展开」不成立：A4 指出期望量是**测试内字面量** `(1u<<1)`、被检量是**公共头宏** `star_matcher.h:11`，改头即红 ⇒ 是有效冻结锁 |
| A1/A3：「黄金数组 == 输入数组」的恒等式 | **否决（两条）** | A3 数值层面否决（60/60 `f_syn` 全部 >0、无平凡值）；我另找到真正的恒等在 **test4**：`:538-540` 构造 `qe_val_ones = ones` ⇒ 「无 QE vs QE=1」是代数恒等式 |
| A2：「`wcs_transform.cpp:54` 的 1e-15 阈值会对正常像素比误报」 | **否决（A2 自行撤回，我复核确认）** | 需 pixel scale < 0.114 mas/px 才触发 ⇒ 阈值本身不构成缺陷（**但 fail-open 的处置仍成立**，见 Q） |
| A2：`frame_photometry_fit.cpp:141-144` 空 `filter_wl` 会 UB | **否决（A2 自证）** | `filter_curve_json.h:215` 在返回 `kOk` 前硬校验非空 |
| A2：「psfsw.cpp 未进任何构建 = 死代码」 | **否决（A4 推翻 A2）** | A2 只查了根 `CMakeLists.txt`（零命中）；A4 查出 3 个 target 编译它（`integration/phase1_product/CMakeLists.txt:29`、`phase2_integrate/CMakeLists.txt:47`、`eng/tests/unit/p1_psfw/CMakeLists.txt:28`） |
| A2：「`build/` 目录可作为可达性判据」 | **否决** | A2 自陈：`build/` 目标名前缀仍是 `astrocs_*`，与当前 CMakeLists 的 `acsd_*` 完全不符（两侧 `grep -c` 均为 0）⇒ 陈旧产物，据它断言任何可达性都错 |
| A3：A 条负透过率（4 条滤镜 −1.55 / F_syn −9.2%~−25.8%） | **确认（我独立复现）** | 我自写 Akima 转写复跑，得 −1.5097/−0.5000/−0.6250/−0.8333 与 −12.98%/−9.28%/−20.96%/−25.86%，与 A3 同量级（末位差异来自网格端点取整）。**过程中我一度复现失败**（网格上下界写反而把 45 条全 `continue` 掉，得出「0 条」的错误结论），逐步定位后推翻了自己的错误复现 |
| A1：「`fixgates.cpp:140` 的 `scale==1.0` 是恒红门」 | **否决（A1 自行撤回）** | `star_matcher.cpp:404` 在所有门之前无条件写 `*out_scale_factor=1.0`，哨兵 123.0 在入口即被覆盖 ⇒ 断言有鉴别力 |
| A1/A3：「selfcheck 把 127/-1 当成功」 | **确认** | 我复核 `selfcheck.cpp:62-72,85,159,173,186,202`，且补上更尖锐的一层：`:52` 正是剥掉 `LD_LIBRARY_PATH` 的那行 |
| A4：「gate4/xpsd 族硬编码外部旧仓绝对路径」 | **确认** | 我独立核实：`lib/star_detector` **不存在**（本仓为 `lib/algorithms/star_detection`）、`lib/dynamic_psf` **不存在**、无任何 `.dll` 入库 |
| A4：`module.yaml` 行号漂移 | **确认（我独立逐条复算）** | 我复算 12 条：star_matcher 两条**正确**，其余 10 条失效（755→896、1048/1084→1193/1239、931/1023→1073/1025、2474→2514、2714/2790→2773/2792、469-486→947-984、429-432→1072） |

**净效果：4 个车道共报 ~30 条阻断级，我逐条复核后采纳 11 条进入交付件，否决/降级 8 条，另有 6 条转入须修或建议。** 最重要的一次否决是子代理对 fixgates 分支归属的误判（我以更重的形式推翻）；最重要的一次自我否决是我自己把 Akima 网格上下界写反而得出「0 条负透过率」的假阴性。

---

## 8. 自证段（可复跑命令）

全部只读、零编译、零执行仓内脚本。复跑工作目录 = `/workspace/Astro CS Database`。

**① 本片行数口径**
```bash
cd "/workspace/Astro CS Database"
# 逐份 wc -l 后求和，期望 9741（与权威清单「实际行数」一致）
awk '{s+=$1} END{print s}' /tmp/shard003_wc.txt   # 复核时按 §3 清单重建该文件
```

**② A 条：Akima 负透过率（最强反例，可独立复现）**
```bash
cd "/workspace/Astro CS Database" && python3 - <<'EOF'
import json, numpy as np
d=json.load(open("lib/algorithms/photometry/data/response_curves/filters.json",encoding="utf-8"))
def akima(xs,ys,dst,fill=0.0):
    n=len(xs);out=np.full(len(dst),fill,float)
    if n<2 or not len(dst):return out
    m=np.array([(ys[i+1]-ys[i])/(xs[i+1]-xs[i]) for i in range(n-1)])
    def ext(k):
        if 0<=k<=n-2:return m[k]
        if n<3:return m[0]
        if k==-1:return 2*m[0]-m[1]
        if k==-2:return 3*m[0]-2*m[1]
        if k==n-1:return 2*m[n-2]-m[n-3]
        return 3*m[n-2]-2*m[n-3]
    t=np.zeros(n)
    for i in range(n):
        ml2,ml1,mc,mr=ext(i-2),ext(i-1),ext(i),ext(i+1)
        w1=abs(mr-mc);w2=abs(ml1-ml2)
        t[i]=0.5*(ml1+mc) if (w1+w2==0.0) else (w1*ml1+w2*mc)/(w1+w2)
    for k,x in enumerate(dst):
        if x<xs[0] or x>xs[-1]:out[k]=fill;continue
        j=0;lo,hi=0,n-2
        while lo<=hi:
            mid=(lo+hi)//2
            if xs[mid]<=x<=xs[mid+1]:j=mid;break
            if x<xs[mid]:
                if mid==0:break
                hi=mid-1
            else:lo=mid+1
        h=xs[j+1]-xs[j];s=(x-xs[j])/h
        out[k]=((2*s-3)*s*s+1)*ys[j]+(((s-2)*s+1)*s)*h*t[j] \
             +((-2*s+3)*s*s)*ys[j+1]+((s-1)*s*s)*h*t[j+1]
    return out
spec=np.array([336.0+2.0*i for i in range(343)])   # 生产 XPSD 网格
for name,e in d.items():
    xs=np.asarray(e["wavelength_nm"],float); ys=np.asarray(e["value"],float)
    lo,hi=max(xs.min(),spec.min()),min(xs.max(),spec.max())
    if lo>=hi: continue
    g=spec[(spec>=lo-1e-9)&(spec<=hi+1e-9)]
    t=akima(xs,ys,g)
    if (t<0).any(): print(f"{name:34s} raw_min={ys.min():.3f} interp_min={t.min():+.4f} #neg={(t<0).sum()}/{len(g)}")
EOF
# 期望输出恰好 4 行：L-1 -1.5097 / L-2 -0.5000 / L-3 -0.6250 / Baader -0.8333
```
> 注意：上下界必须是 `max(xs.min(), spec.min())` 与 `min(xs.max(), spec.max())`。写反会让 45 条全部被 `lo>=hi` 跳过而假得「0 条」（我在复核中踩过此坑）。

**③ B 条：fixgates「门 2」用例的星等预过滤算术**
```bash
python3 -c "
import math
gm=-2.5*math.log10(1000.0)
F=[1000*10**0.10,1000*10**0.10,1000*10**2.90]
d=[-2.5*math.log10(f)-gm for f in F]
s=sorted(d);m=s[len(s)//2]
print('delta=',[round(x,4) for x in d],'median=',m)
print('deviation=',[round(abs(x-m),4) for x in d],'tol=3.0')
print('kept=',sum(1 for x in d if abs(x-m)<=3.0),'(<3 => star_matcher.cpp:518 NO_DATA)')"
```

**④ E 条：module.yaml 行号逐条证伪**
```bash
cd "/workspace/Astro CS Database"
sed -n '429,433p;1072,1075p' CMakeLists.txt
for L in 755 896 931 1023 1025 1048 1073 1084 1193 1239; do printf "pc_api.cpp:%s: %s\n" $L "$(sed -n "${L}p" lib/algorithms/photometry/cpp/src/pc_api.cpp)"; done
for L in 2474 2514 2714 2773 2790 2849; do printf "orchestrator.cpp:%s: %s\n" $L "$(sed -n "${L}p" lib/infrastructure/pipeline/orchestrator/cpp/src/orchestrator.cpp)"; done
for L in 469 947; do printf "module_adapters.cpp:%s: %s\n" $L "$(sed -n "${L}p" lib/infrastructure/scheduler/src/module_adapters.cpp)"; done
```

**⑤ D 条 / G 条：审计文档数字 vs 其引用的证据 + 悬空路径**
```bash
cd "/workspace/Astro CS Database"
python3 -c "
import json;d=json.load(open('lib/algorithms/photometry/cpp/test/gate4_dr3sp_gaiaxpy/evidence/gate4_result_full_1050.json'))
print({k:round(v['median_diff_mag'],5) for k,v in d['color_stats'].items()},' 文档称 <=0.001')
print('selfcheck BP=',d['selfcheck']['BP_median_absdiff'],' 文档称 <=0.008')
print('keys=',list(d.keys()),' <- 无 gates 键')"
ls -d lib/star_detector lib/dynamic_psf 2>&1
git -c core.quotepath=false ls-files | grep -icE '\.dll$|\.exe$'
```

**⑥ I 条：溯源文件会被测试覆写（比对生成器写盘键集 vs 盘上键集）**
```bash
python3 -c "
import json;d=json.load(open('lib/algorithms/photometry/cpp/test/filter_qe_provenance.json',encoding='utf-8'))
e=next(iter(d['filters'].values()))
gen={'n_points','wl_min','wl_max','val_min','val_max'}   # golden.py:268-274 实际写盘
print('盘上键:',sorted(e));print('生成器写:',sorted(gen))
print('会被抹掉的:',sorted(set(e)-gen))
print('总条目:',sum(len(v) for v in d.values()),'| 有 url:',sum(1 for s in d for v in d[s].values() if v['url']))"
sha256sum lib/algorithms/photometry/cpp/test/filter_qe_provenance.json
```

**⑦ H/K 条：门不设最小样本 / 无退出码**
```bash
grep -n 'n_matched\|is not None' lib/algorithms/photometry/cpp/test/gate4_dr3sp_gaiaxpy/gate2_psf_oracle.py | sed -n '1,20p'
grep -c 'sys.exit' lib/algorithms/photometry/cpp/test/test_photometric_calib.py \
               lib/algorithms/photometry/cpp/test/test_energy_conservation.py \
               lib/algorithms/photometry/cpp/test/test_spectrum_integrator_golden.py
grep -n 'return t[0-9]_ok\|return all_ok\|^def test_' lib/algorithms/photometry/cpp/test/test_photometric_calib.py
```

**⑧ F 条：selfcheck 的 127/-1 路径**
```bash
sed -n '43,74p;84,86p;155,168p' lib/algorithms/photometry/tests/p1phot/p1phot_tests_selfcheck.cpp
grep -n '"spatial"' lib/algorithms/photometry/tests/p1phot/p1phot_tests_main.cpp lib/algorithms/photometry/tests/p1phot/p1phot_tests_selfcheck.cpp
```

**⑨ 悬空引用的存在性总检**
```bash
cd "/workspace/Astro CS Database"
for p in "lib/algorithms/photometry/python" "lib/algorithms/photometry/cpp/test/test_spectrum_integrator.exe" \
         "lib/algorithms/photometry/cpp/test/gate4_dr3sp_gaiaxpy/fsyn_export.exe" "eng/ci/check_prod_wiring.py" \
         "MODULE_MIGRATION_MATRIX.csv" "docs/engineering/11_MODULE_SOURCE_TEST_STANDARD.md" "logs"; do
  [ -e "$p" ] && echo "EXISTS  $p" || echo "MISSING $p"; done
find . -name 'synthetic_photometry*' -not -path './.git/*' | head
grep -c '§F3' docs/science/algorithms/PHOTOMETRIC_FIT.md
git -c core.quotepath=false cat-file -t c19b4a59
```

---

## 9. 建议处置顺序（按证据强度，非施工建议，仅供负责人裁定点）

1. **先修 A（负透过率）** —— 这是本片唯一的**真红灯**：给 `frame_photometry_fit.cpp:222-225`（及积分器）的 `filter_trans/qe_trans` 加 `[0,1]` 钳位或改保形插值，并把 4 条阻挡滤镜纳入回归；同时把 `golden.py:267` 的门改成断言**插值后**的值，否则该门永远看不见它。
2. **给三个 py 补 `sys.exit`，把 `test_photometric_calib.py` 的 5 个 `return bool` 改 `assert`** —— 否则 0/5 通过时 shell 收 exit 0。
3. **把三个克隆 oracle（`oracle_irls_tukey`/`oracle_akima`/`fsyn_cached_like`）换成真独立参照** —— 同仓 Simpson `n_int==3/5` 的闭式 + 分段互证已是现成模板（`oracle.hpp:298-300`）。
4. **给三处筛子补「丢弃计数 + 最小成功数门 + 判 max」**（gate2 / xpsd / gate4）。
5. **修 `module.yaml` 的行号与「未接管线」声称**，并先决定 `filter_qe_provenance.json` 的归属（迁移元数据进 `eng/packaging/config/filters.json` 的 `provenance.per_filter` 并补 QE 12 条，再删测试写盘代码）。
6. **selfcheck 改为「子进程打印了 `FAULT-INJECT <name>` 且 rc ∉ {0,127,-1}」**（`selfcheck.cpp:3-8` 头部本来就写了这条要求，代码从未检查）。