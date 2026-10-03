# 审稿-P1 · EXP-healpix-polar-002（第 1 遍）

- 仓库：`/workspace/Astro CS Database`，HEAD = `f9650dd0aed97d7f261e5e6547f4fdb505bc313b`
- 片：`EXP-healpix-polar-002`（层 `实验/healpix-polar`，权威片清单 `run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml:2456-2493`）
- 划片依据：SRS-1 层内 LPT 均衡装箱，严格不跨层
- 纪律：零 git 写；未编译、未跑 ctest/pytest/build、未跑任何实验脚本（含 `run_all.sh`）；未读 `/tmp/acsd_g08/`；未修改任何仓内文件（交付件本身除外）。终局 `git status --porcelain` 为空。
- 所有需要跑才能得的结论一律标注为「未实测」；本文所有读数均引自仓内已提交归档 JSON 与源码，逐条给 `文件:行`。

---

## 1. 读完了吗

| 项 | 值 |
|---|---|
| 成员份数（权威清单） | **30** |
| 实际读了 | **30**（30/30） |
| 成员总行数（权威清单） | **5526** |
| 磁盘实测行数（`wc -l` 逐份） | **5526**（与清单逐位吻合） |
| 实际读了多少行 | **5526** |
| **覆盖率** | **100.0%（30/30 份，5526/5526 行）** |
| 目标行数 | 11000（不超容量） |

**未读完的部分：无。** 本片 30 份成员文件我逐份用 `read` 从头到尾读完，无抽样、无跳读、无「顺便看看别的片」。

逐份行数（与权威清单 `实际行数: 5526` 逐位对齐）：

```
831 exp_sim01_m16_forward_conservation.py   506 polar_common.h      381 p1_rootcause.cpp
313 kcorr/mc_kcorr.py                       290 variants.h         262 REPORT_paper.md
250 route2/exp10_chain_usecase.py           236 route2/p3lib.py     229 kcorr/run_scan.py
216 route1/e4_flux_conservation.py          192 route2/exp03_flux_conservation.py
178 route1/e3_circumradius_scan.py          164 route1/e6_lhuilier_vos.py
147 route2/exp07_polar_sagitta_ladder.py    136 route3/exp01_leaf_area.py
121 route2/exp06_circumradius_margin.py     117 docs/DISPUTES.md    105 README.md
103 code/p4_hst3d.cpp                        96 route3/exp04_circumradius.py
 91 route3/exp07_quantization.py             85 results/audit/kcorr/tables.md
 83 route3/edge_geom.py                      80 route2/exp01_leaf_area.py
 73 run_all.sh                               64 docs/EXP-07-POLAR-摘要.md
 59 results/audit/KEY_RESULTS.md             53 code/p5_extent.cpp
 42 code/p7_wcs3d.cpp                        23 code/p0_selftest.cpp
```

**引用了但不在本片清单内、因而只做交叉核对未计入覆盖率的仓内文件**（均为只读）：
`results/audit/sim/exp_sim01_m16_forward_conservation.json`、`results/audit/route1/{e3,e4,e6}*.json`、`results/audit/route2/{exp03,exp10}*.json`、`results/audit/kcorr/*.json`、`results/p1_t4_pole_full.out`、`code/audit/kcorr/read_tables.py`、`code/audit/run_all.sh`、`lib/algorithms/drizzle/healpix_drizzle/{spherical_overlap.cpp,tests/control_median_mc_test.cpp}`、`docs/science/algorithms/DRIZZLE_GEOMETRY.md`、前三轮审稿件与 `实验/TAUTOLOGY_REGISTER.md`。

---

## 2. 本片判定

## **需修（其中 4 条达阻断级）**

最重的 3 条：

1. **【阻断·新】本片整个 `code/audit/` 树没有任何可失败路径** —— 零 `assert`、零 `sys.exit`、零只读 `open`。红灯只写进 JSON，进程恒 `exit 0`。仓内**已入库随发布的三条真红灯**：`route1/e6_lhuilier_vos.json` 的 `verdict = "FAILED"`、`route1/e3_circumradius_scan.json` 的 `monotone_increasing: false`、`e4_flux_conservation.json` 的 `inv_strict_bound_holds: false`（我亲自 `python3 -c` 读归档确认）。这使「恒真门」与「真红灯」产生完全相同的静默后果。根杠杆点：**在这条修好之前，任何单条恒真门的修复都不可见。**

2. **【阻断·新】`exp_sim01` 的 NC-A 负例代数恒真，且它自己写的判词是错的。** `exp_sim01_m16_forward_conservation.py:790-792`。`res_flat_p = E_p[om]/E_p[ap] − 1` 是**两个正量加权均值之比** ⇒ `Σw ≠ 1` 在比值里约掉、NC-B 的错分母 `w=a_jp/A_pixel` 同样约掉；唯一能破门的是**负权重**，而 `wg = a/|ad| ≥ 0`（`:606`、`:189`）结构上不可能。而归档 `judgement`（`:784`）逐字写「**漏归一化时立刻破**」——**这句话是假的**，且 `TAUTOLOGY_REGISTER.md` 未登记此条。

3. **【阻断】`run_all.sh:43` 实参错位，T4 极点扫描退化为 0 配置并读作恒绿，且 `:68` 会就地覆盖当前正确的归档。** `./p1 t4 -8 8 0.5` 的落位是 `argv[2]="-8" → csv`（`p1_rootcause.cpp:358`），`lo=8.0`、`hi=0.5`（`:366-367`），`:297-298` 双层循环**零次执行** ⇒ 打印 `配置数=0`、`破门=0/0`。归档 `results/p1_t4_pole_full.out:2-3` 明写 `[-8.0,8.0] step 0.50` / `配置数=1089` ⇒ **脚本已无法复现自己的归档**。叠加无 `set -e`（`:9`）、`:40-56` 17 个调用点全不检查 `$?`、`:380` 恒 `return 0`。

---

## 3. 逐文件清单（30 份全读）

| # | 文件 | 读了什么 | 看到什么（`文件:行`） | 判定 |
|---|---|---|---|---|
| 1 | `code/audit/sim/exp_sim01_m16_forward_conservation.py`（831） | docstring `:1-65`、`sphere_to_chart` `:99-148`、`clip_rect` `:161-186`、`solid_angle_gnomonic` `:199-252`、`face_boundary_case` `:290-377`、`main` 全程 `:444-831` | `:508` 真计数 → `:514` `outside = 0` 裸覆盖 → `:746` 落档；`:510-513` `np.empty` 仅 `:519` 条件赋值，`:574`/`:594`/`:656-657` 仅值守卫；`:507` 丢弃像元自身 `single` 而 `:644` 用 drop 的 `drop_single` 当像元有效性；`:790-792` NC-A 代数恒真；`:725-726` 与 `:733-734` 同一表达式；`:22` 残留已推翻的 √(1+ρ²)−1 | 阻断 |
| 2 | `code/polar_common.h`（506） | 全部 | `rotate_to_z` `:84-96` 旋转轴方向正确；`vos_area_rotated` `:102-122`；`chart_uv_to_xyz` mode1 `:183-190` 的 `st = s·√(2/3)·√(1−s²/6)` **恒等于** `sin(2·asin(s/√6))`（我逐项代数验证）；`oracle_overlap` `:479-487` Richardson `(4A(2K)−A(K))/3` 正确；`leaf_area_K` `:490-494` 未外推 | 通过（含 1 条须修派生项，见 F12） |
| 3 | `code/p1_rootcause.cpp`（381） | 全部 | T1 `:45-108`、T2 `:130-162`、T3 `:167-199`、T4 `scan_at` `:282-353`、T7 `:227-280`、入口 `:356-381` | 须修 |
| 4 | `code/audit/kcorr/mc_kcorr.py`（313） | docstring `:1-28`、算子 `:70-133`、MC 核心 `:183-271`、F&H `:274-313` | `:13` docstring 称正本 `N_retained ~ 251`，而 `:85-121` 的构造实测 `n_out=16×16`、touched=**225**（`tables.md:16` 亦记 225）⇒ 与「唯一记录」不符且未对账；`:285` `half_drop` 死变量；`:27` 自述「不 import 仓库任何代码」 | 须修 |
| 5 | `code/variants.h`（290） | 全部 | `:190-193` 头注释声称「自适应折线（顶点严格在 C_k 上，**弦偏差 ≤ tol**）」，而 `clip_poly_by_curve` `:254` 是 `(void)tol; (void)max_depth;` —— **容差被丢弃**；`chart_curve_crossings` `:201-222` 全仓无调用者；`chart_overlap_leaf` `:265-266` 把裸索引 `i` 当 chart 坐标传给要求 `u,v∈[0,1]` 的 `chart_uv_to_xyz` ⇒ 变体 B 结构错误 | 阻断（另见 F17） |
| 6 | `REPORT_paper.md`（262） | 全部 | `:14` 摘要、`:26-28` 接口、`:40-48` §2.1、`:67-122` §3.1-3.6、`:183-234` §5 诚实边界 15 条、`:238-240` §6 结论、`:244-262` 文献 | 须修 |
| 7 | `code/audit/route2/exp10_chain_usecase.py`（250） | 全部 | `:90-93` IDW 短路 `vals[qi]=ctrl_vals[hit[0]]`，`:223` 用控制点自查 ⇒ `:239` 恒真；`:180-181` 使 `flux_closure` 成为 `:237` 权重门的严格蕴含别名；`:116-119` `sigma_pix=max(x,1.0)` 恒 1.0（x=9.4e-8）⇒ 归档 `reconstruction_range=[-0.685,+0.510]`（**负 SNR**）；`:199` 注释称「无噪参考」而 `:201` 用含噪 `Fp`；`:210-220` 造 100 个随机查询只取前 50 | 阻断 |
| 8 | `code/audit/route2/p3lib.py`（236） | 全部 | `:4` 自述不 import 仓内模块；`:82-84` `jacobian_chart` **返回字面量 π/3，不做任何微分**；`:74` 用稳定式 `sqrt(1−z²)` 而生产用 `acos(z)`（RC2a）⇒ route2 全片结构上看不见 RC2a；`:106/:111-112` `sample_edge`/`leaf_true_boundary` 文档称 “chart-straight (**true-curve**)” 用括号并列二者，误导；`:21` 死 import | 须修 |
| 9 | `code/audit/kcorr/run_scan.py`（229） | 全部 | `:69` `reference_k_gauss=dict(N25=1.0826)` **硬编码字面量**充当 oracle，`:73` 门 `abs(k-1.0826)<0.10` 对它判红；`:94-99` G1 写 `target_from_repo=1.3883` 但**从不与之比较**；`main` `:195-226` 无任何 exit/assert | 须修 |
| 10 | `code/audit/route1/e4_flux_conservation.py`（216） | 全部 | `:142`/`:143` 实参逐字相同、`:147` `(α²·X)/X ≡ 9`、`acc2` 死代码（归档 `ratio_min==ratio_max==9.0` 坐实）；`:124` `pixnorm_flux_ratio` 是字面量、H3(b) 判红臂从未执行；`:137-138` verdict 是硬编码字符串；`:49` `rng` 死、`:53-54` `round_half_away` 死、`:76` `h_in` 死、`:57` `pitch_out_ratio` 死、`:93` `cov` 死；归档 `inv_strict_bound_holds=false` | 阻断 |
| 11 | `code/audit/route2/exp03_flux_conservation.py`（192） | 全部 | `:81-83` `dz = 2/(3·NSIDE)` 只有真实叶 z 跨度 `4/(3N)` 的一半 ⇒ **「叶」不是 HEALPix 叶**而是被砍半的轴对齐矩形；`:31/:34/:86` ⇒ 叶 10126″ vs 源帧 16″ ⇒ 64 个 drop 全落一格，S-H 部分重叠分支一次未跑；`:171-174` `negative_injections.note` 声称 pf=1 已 checked 而 `PF` 是模块常量 `:32` 从未改 | 阻断 |
| 12 | `code/audit/route1/e3_circumradius_scan.py`（178） | 全部 | 归档 `monotone_increasing: false`、序列 1.13233→1.12839（**降**）、`worst_dec` 73.4°→88.97°（**极区**，非 docstring `:7` 的 |z|=2/3）、`max_measured=1.13233`（docstring `:6` 称 1.0442）、`lonlat_contrast={4:0.952, 8:1.142, 16:3.125}`（docstring `:9` 称 ~1.67；N=4 时对照格反而优于被测格）；`:72` 中心取相邻 ring 纬度中点；`:32` `np.random.seed` 无随机 | 阻断 |
| 13 | `code/audit/route1/e6_lhuilier_vos.py`（164） | 全部 | 归档 `verdict="FAILED"`（`lh_shift_max=2.98e-07` 未过 1e-12）；H1 **无门**且归档 `tiny`/`near_hemisphere` `max_rel_diff = 1.0`（**100% 失败**，docstring `:7-8` 称 ≤1e-12）；`:93` `ok` 掩码静默丢 17%/25% 样本；`:139` note 声称「≥π/2 必须显式失败(NaN)」而归档 `vos_finite_on_those=41529/41529`（该行为不存在）；`:159-160` H3「Quantified」是两句零数字的散文；`:129` `mx=np.zeros(50000)` 长度硬编码 | 须修 |
| 14 | `code/audit/route2/exp07_polar_sagitta_ladder.py`（147） | 全部 | `:133` 门拿 **rho₁ 单位**的 `rr` 比 8.094e-2，而 `:72-79` note 自认 hp_res 单位实测是 **6.39e-2**（换单位即红）；`:111` 结论串硬编码「measured factor > 3x」；`:106`/`:110` `claimed_*` 字面量入档不比较（`1.23` vs 实测 `3.147`）；docstring `:15-17` 称 NSIDE 1024 而 `:121-129` 演示的是 1 vs 2 | 须修 |
| 15 | `code/audit/route3/exp01_leaf_area.py`（136） | 全部 | `:99`/`:101`：`sum_res = ΣA/4π−1` 与 `mean_res = ΣA/(12N²)÷(π/3N²)−1` **恒等** ⇒ `:129`/`:130` 两条门是同一个数（归档八行逐位相同坐实）；`:128` 键名 `le_3p3e12` 而阈值 `4e-12`；`:105-118` 四个负控制实测全对但一个都没进 `:128-130`；`:78` `np.abs(带符号求和)` 而生产 `p1drz_geom.hpp:139` 是逐三角形 `fabs` | 须修 |
| 16 | `code/audit/route2/exp06_circumradius_margin.py`（121） | 全部 | `:109-112` 四条全真门；`:111` `margin_min > 1.2` 而归档 `1.2001933288902629` ⇒ **0.016% 擦线过**；`:70`/`:109`/`:111` 三处硬编码 1.25，**从不读**生产 `spherical_overlap.cpp:42`；`:103` 论证串里的实测值写死；`:71` `predicted_range` 记了从不参与判定 | 须修 |
| 17 | `docs/DISPUTES.md`（117） | 全部 | A-P3-01…12 十二条，格式合规（`:7`/`:8`/`:10` 明示 ID 不重排、无锚写「待补」、不写变更过程）；**反向发现**：`:28`/`:45` 与 `REPORT_paper.md:81` 仍把代码注释订正登记为「须订正」，而生产 `spherical_overlap.cpp:1009-1010` 已写成 `≈ −θ_max²/2 … −5.0e-7, 恒负`，lib/ 下 `4e-8` **0 命中** ⇒ **正本状态被四份文档低报** | 通过（含 1 条建议） |
| 18 | `README.md`（105） | 全部 | `:41` 正文残留 HTML 批注 `<!-- 订正: ... -->` 与「原表述（保留可追溯）」历史叙事，违 AGENTS §5；`:31` 「sag0 = 8.094e-2·ρ₁」引的是**声称值**，实测 `0.08012`（`tables.md:28`）；`:64` 宣称「fail-closed 剔除并**计数**」而代码计数被焊死；`:80-81` 主动披露 `tables.md` 重跑会覆盖手写批注 | 须修 |
| 19 | `code/p4_hst3d.cpp`（103） | 全部 | `:88` `int bad = 0;` + `:101` `(void)bad;`、`:44` `fsh` 未用、`:93-94` `HealpixCore hp((int)Ns); (void)hp;` ⇒ **门被摘掉后留下的脚手架**，`:102` 恒 `return 0`；`:75-77` 汇总行打印五组破门数但无 exit | 须修 |
| 20 | `code/audit/route3/exp04_circumradius.py`（96） | 全部 | `:86-90` 四条真门、归档 `max_ratio` 与 route2/exp06 **逐位相同**（1.0414988734821498）⇒ 两者被测多边形代码不同却数值重合，**不是两条独立腿**；`:87` 严格 `<` 与 e3 `:157` 的 `<=+1e-12` 口径不一致；`:11` `wrap_pi`、`:12` `pixel_corners` 死导入 | 建议 |
| 21 | `code/audit/route3/exp07_quantization.py`（91） | 全部 | `:84` 容差 `0.05` **大于**它要检的效应（归档 `\|pred\|`@S0=0.05 = 0.0381）⇒ **可证明恒绿**；归档实测 `+0.0079769` vs 预测 `−0.0380917`（**符号相反**）仍判绿；`:78` 严格界是舍入定义的重述（`|255S−q\|≤0.5`）；`:80` 对浮点用 `== -0.5` 精确相等（当前侥幸为绿）；`:74` `r_if_stored_clamped_to_1=0.3` 字面量 | 须修 |
| 22 | `results/audit/kcorr/tables.md`（85，全读） | 全部 | `:9` 整行硬编码 `PASS`（生成器 `read_tables.py:24` 不读 JSON）；`:22` **手插审稿批注**「见检查-跨文档冲突 红1」而 `:1` 声称「由 results/*.json 机器生成」；N=5 三个读数并存 `:9` 1.634 / `:30` 1.6620 / `:45` 1.6370 | 须修 |
| 23 | `code/audit/route3/edge_geom.py`（83） | 全部 | `:4` 「verified verbatim from arXiv:astro-ph/0409513」**仓内无核对记录**；`:63-64` `raise ValueError` 是全片唯一 fail-loud 出口（**值得推广**）；`:25` `za` 死变量、`:11` `import os` 死、`:74-83` `edge_sagitta_hp_res` 全仓无调用者 | 建议（fail-loud 记为正面） |
| 24 | `code/audit/route2/exp01_leaf_area.py`（80） | 全部 | `:33` 调 `P.jacobian_chart`，而该函数返回字面量 ⇒ 这一腿在**测一个常量**；`:69-72` 三条门；`:62` `A_wrong` 负例 0.25 真跑 | 须修 |
| 25 | `run_all.sh`（73） | 全部 | `:9` 仅 `set -u`；`:43` argv 错位；`:33-36` `return $rc` 但 `:40-56` 全不检查；`:68` `cp ... "$UNIT/results/"` 就地覆盖；`:69-70` **自写哈希再自校验**（恒绿）；`:73` 决定最终退出码 | 阻断 |
| 26 | `docs/EXP-07-POLAR-摘要.md`（64） | 全部 | `:20`/`:33`/`:63` 三处 HTML 批注残留；`:5` 「±8 px / 0.5 px 网格 1089 配置」与 `run_all.sh:43` 的退化调用矛盾；`:32` 对 REC-1 缺陷的登记**诚实度高**（明写「未逐叶证实」「不得作为推荐落地路径」） | 通过（含建议） |
| 27 | `results/audit/KEY_RESULTS.md`（59） | 全部 | **无 `sim/` 节**（16 个 M16 门从不进索引）；`:50` `g0b` 判红被隐去且用**第三个文件**的数字背书；`:14` 把代数恒真的「方差二次律比值 9.000000」记为核心读数并挂 A-P3-05/D-03；`:42` 伪引已注销的 `"4e-8"` 注释 | 阻断 |
| 28 | `code/p5_extent.cpp`（53） | 全部 | `:49-50` 列头与实参一一对齐（我核过）；`:53` 恒 `return 0` 无门；`:48` 中位数口径正确 | 建议 |
| 29 | `code/p7_wcs3d.cpp`（42） | 全部 | `:34` 表头「模型 2.2e-16/r²」与 `:40` 实现一致（无伪引）；`:42` 恒 `return 0` 无门 | 建议 |
| 30 | `code/p0_selftest.cpp`（23） | 全部 | `:14` 打印 `PASS`/`FAIL` 字样但 `:23` 恒 `return 0` ⇒ **字符串判词与退出码脱钩**；`:9` `rotate_to_z(c,c)` 是合法的自检（`polar_common.h:80-83` 给出了推导，非恒真） | 须修 |

---

## 4. 发现清单

### 4.1 阻断级（4 条）

**【B1 · 新】整个 `code/audit/` 树零可失败路径，三条真红灯已入库随发布**
- 位置：`实验/healpix-polar/code/audit/**`（全树）；消费点 `run_all.sh:31-37`、`:40-56`
- 现状：`assert` / `sys.exit` / `raise SystemExit` **0 处**；只读模式 `open(...)` **0 处**（全部 31 处为写模式 `"w"`）。所有门只 `print`/落 JSON，进程恒 `exit 0`。
- 实测三条活红灯（我亲自读仓内归档）：
  - `results/audit/route1/e6_lhuilier_vos.json` → `non_normalized_divergence.verdict = "FAILED"`
  - `results/audit/route1/e3_circumradius_scan.json` → `monotone_increasing: false`
  - `results/audit/route1/e4_flux_conservation.json` → `quantization.inv_strict_bound_holds: false`
- 应为：每个实验组至少一处 `assert` / `raise SystemExit(1)`，把 verdict 转成退出码；`code/audit/run_all.sh` 的 `RC` 累加（其 `:22/:28/:33/:86-88` 已具备，README `:78-79` 描述准确）应扩展覆盖全部腿。
- 证据：`python3 -c "import json;..."` 读三份归档（见 §8）。

**【B2 · 新】`exp_sim01` NC-A 负例代数恒真，其 `judgement` 判词为假**
- 位置：`exp_sim01_m16_forward_conservation.py:790-792`（门）、`:618-619`（分子分母）、`:646`（上界）、`:784`（假判词）
- 现状：门为 `max|res_flat| <= pix_area_ratio_max + floor`。但 `res_flat_p = Ff_p/Np_p − 1 = E_p[om]/E_p[ap] − 1`，是**两个正量加权均值之比**。任意权重下该比值必落在 `min_j(om_j/ap_j)` 与 `max_j(om_j/ap_j)` 之间，而上界恰是 `max|ap_j/om_j − 1|` ⇒ **恒成立**。`Σw ≠ 1` 在比值里约掉；NC-B 的错分母同为加权均值之比，也约掉。唯一能破门的是负权重，而 `wg = a/|ad|`（`:606`/`:189`）**结构上不可能为负**。
- 应为：要么改成直接检验 `Σ_p w_jp − 1`（NC-A 的目的是「平坦真值下不产生伪信号」，那就必须用可被破坏的量），要么把 `judgement` 的「漏归一化时立刻破」删掉。
- 证据：代数展开 + `shoelace_abs` 恒返 `abs(...)`（`:189-196`）。

**【B3】`run_all.sh:43` 实参错位 ⇒ T4 退化为 0 配置、读作恒绿、且覆盖正确归档**
- 位置：`run_all.sh:43`；`code/p1_rootcause.cpp:358`、`:366-368`、`:297-298`、`:345-350`、`:380`；覆写点 `run_all.sh:68`
- 现状：argv 落位 `csv="-8"`、`lo=8.0`、`hi=0.5`、`st=0.5` ⇒ `for(dx=8.0; dx<=0.5; …)` **零次执行** ⇒ `配置数=0`、`破门=0/0`。同时生成名为 `-8` 的垃圾文件，CSV 不落 `$LOGS`。
- 应为：`run p1_t4_pole_full ./p1 t4 "$LOGS/t4_pole.csv" -8 8 0.5`（对齐同文件 `:46` 的 p3_t12 约定）。
- 证据：归档 `results/p1_t4_pole_full.out:2-3` 仍写 `dx,dy in [-8.0,8.0] step 0.50 px` / `配置数=1089` ⇒ **脚本已无法复现自己的归档**。

**【B4】`e4_flux_conservation.py` 的「方差二次律」是 `(α²X)/X`，且文件内有一条真红灯无人报**
- 位置：`route1/e4_flux_conservation.py:142`、`:143`、`:144`、`:147`；归档 `results/audit/route1/e4_flux_conservation.json`
- 现状：两次 `drizzle_local(2.0, 0.8)` 实参逐字相同；函数体 `:69` `v_in = np.full(..., 4.0)` 无输入方差参数 ⇒ `acc2` 死代码；`:147` `(alpha**2 * acc1["Var"][m]) / acc1["Var"][m]` 逐像元恒等于 9。归档 `ratio_min == ratio_max == 9.0` 坐实。docstring `:19-20` 的 H4 **从未被检验**。
- 加重：同文件归档 `inv_strict_bound_holds = false`（真红灯），而 `KEY_RESULTS.md:14` 只报「方差二次律比值 9.000000」这条**恒等式**，把红灯静默掉。
- 应为：`:144` 的「重跑」要么真跑（给 `drizzle_local` 加方差入参），要么把该条从报告与台账证据中撤下。

### 4.2 须修级（16 条）

| ID | 位置 | 现状 → 应为 |
|---|---|---|
| **M1**·新 | `exp10_chain_usecase.py:116-119` | `A_pix=9.40e-11`、`x=9.40e-8`、`sigma_pix=max(x,1.0)` **恒为 1.0** ⇒ SNR=1.88e-6，归档 `reconstruction_range=[-0.685,+0.510]`（**负 SNR 控制点**）⇒「P1 标定帧 + SNR 控制点」在数值上不存在。修量级或删 SNR 语义主张 |
| **M2**·新 | `exp10:90-93` + `:223` + `:239` | IDW 精确性门被 `if len(hit): vals[qi]=ctrl_vals[hit[0]]` 短路，归档值精确 `0.0` ⇒ 结构恒真。docstring `:14-15` 称「插值精确性」被验证，实为「验证自己写了 return」 |
| **M3**·新 | `exp10:180-181` + `:238` | `F_tot = Σ x_j·sw_j` 使 `flux_closure` 成为 `:237` 权重门的**严格蕴含别名**，归档精确 `0.0`。docstring `:10` 把 (I1) 列为独立接口检查 |
| **M4**·新 | `exp03_flux_conservation.py:81-83` | `dz = 2/(3N)` 只有真实叶 z 跨度 `4/(3N)` 的一半 ⇒ 「叶」是被砍半的轴对齐矩形；全部门是**分割不变式** ⇒ 对格子错误免疫。正确叶格见同目录 `exp10:146-167` 的 (u,v) 裁剪 |
| **M5**·新 | `exp03:31`+`:34`+`:86` | 叶 10126″ vs 源帧 16″ ⇒ 64 个 drop 全落一格，S-H 部分重叠分支一次未跑。归档 `S_over_B0_wrongDp_mean` 与 `1/pf²` 只差 1.6e-11 正是该退化的数值指纹。修 NSIDE 或放大源帧 |
| **M6**·新 | `exp03:171-174` | `negative_injections.note` 逐字声称 pf=1 情形「(checked: sum_p w = 1 to float floor…)」而 `PF` 是 `:32` 的模块常量、**从未改过** ⇒ 把推导写成实测 |
| **M7**·新 | `e3_circumradius_scan.py` docstring `:5-10` | 四个声称数字全被自身归档推翻（monotone=false / 最坏在极点 / 1.1323 vs 1.0442 / lonlat 0.952·1.142·3.125 vs ~1.67）；且 e3 与 `exp06`/`exp04` 两个独立实现差 **8.7%**（二对一，e3 离群且无门） |
| **M8**·新 | `e6:93` + `:99-101` | H1 无门，且归档 `tiny`/`near_hemisphere` 的 `max_rel_diff = 1.0`（100% 失败）；`:93` 的 `ok` 掩码静默丢 17%/25% 样本 |
| **M9**·新 | `e6:139` | note 称「max_ang ≥ π/2−1e-12 必须显式失败(NaN)」，而 `vos_area` `:37-40` 无此守卫、归档 `vos_finite_on_those = 41529/41529` ⇒ **note 陈述了一条实现恰好违反的规范** |
| **M10**·新 | `exp07:133` | 门拿 rho₁ 臂过门，而 `:72-79` note 自认 hp_res 臂实测 6.39e-2（换单位即红）；`:111` 结论串硬编码「> 3x」；`:15-17` 称 NSIDE 1024 而代码演示 1 vs 2 |
| **M11**·新 | `route3/exp01:129-130` | `sum_res` 与 `mean_res` **恒等** ⇒ 两条门是同一个数（归档八行逐位相同）；`:105-118` 四个负控制实测全对却一个未进门；`:128` 键名 `le_3p3e12` 而阈值 4e-12 |
| **M12**·新 | `route3/exp07:84` | 容差 0.05 > 效应 \|pred\|=0.038 ⇒ **可证明恒绿**；归档实测 +0.0080 vs 预测 −0.0381（**符号相反**）仍判绿。docstring `:8` 的乘性模型不适用（量化加的是加性噪声） |
| **M13**·新 | `mc_kcorr.py:13` | docstring 称正本 `N_retained ~ 251`，而 `:85-121` 的构造实测 **225**（`tables.md:16` 亦记 225）⇒ 与自称的「唯一记录」不符，全链无对账 |
| **M14**·新 | `run_all.sh:69-70` | `sha256sum … > SNAPSHOT.sha256` 紧接 `sha256sum -c SNAPSHOT.sha256` ⇒ **自写哈希再自校验，恒绿**，不构成任何回归门 |
| **M15**·新 | `variants.h:190-193` vs `:254` | 头注释声称「弦偏差 ≤ tol 的自适应折线」，代码 `(void)tol; (void)max_depth;` **丢弃容差**；`chart_curve_crossings` `:201` 全仓无调用者；`chart_overlap_leaf:265` 把裸索引当 chart 坐标 ⇒ 变体 B 结构错误（好消息：全仓无调用者，未污染产物） |
| **M16**·新 | `p1_rootcause.cpp:167-199` + `polar_common.h:185` | T3 的 mode1「误差」是把同一恒等式（`st = s·√(2/3)·√(1−s²/6)` ≡ `sin(2·asin(s/√6))`，我逐项验证）在双精度 vs 长双精度下相比 ⇒ **自证型**，只能测舍入不能测公式。mode0（`:177` 用 `acos`）才是真门 |

### 4.3 建议级（10 条）

| ID | 位置 | 说明 |
|---|---|---|
| S1·新 | `exp_sim01:510-513`/`:519`/`:574`/`:594`/`:656` | `np.empty` + 条件赋值 ⇒ 非 `drop_single` 像元读未初始化值；当前 `drops_total=25600=CROP²` 故不触发。`:507` 丢弃像元自身 `single`，`:644` 却用 drop 的 `drop_single` 当像元有效性 |
| S2·新 | `exp_sim01:22` | 残留已推翻的「√(1+rho^2)-1 以下」原话，而同文件 `:45-48`/`:227-234` 逐句推翻它；已扩散到 `REPORT_experiment.md:152` |
| S3·新 | 8 处文本 | 「**随足迹缩小而单调下降**」方向与数据相反：归档 `max_abs_by_footprint_scale_px = {0.2:3.397e-9, 0.8:7.483e-10, 3.2:2.025e-10, 12.8:7.307e-11}` 是随足迹**变大而下降**。涉 `exp_sim01:22`、`REPORT_experiment.md:109/:130/:184`、`REPORT_paper.md:133`、`README.md:57` |
| S4·新 | `KEY_RESULTS.md`（全 59 行） | **无 `sim/` 节**：16 个 M16 门与 verdict 从不进索引，与 `:5` 自称「本文件是索引」不符 |
| S5·新 | `KEY_RESULTS.md:42` | 伪引已注销对象：逐字引生产注释 `"4e-8"`；我核 `git grep 4e-8 -- lib/` **0 命中**，`spherical_overlap.cpp:1009-1010` 现为 `≈ −θ_max²/2 … −5.0e-7`，正本 `DRIZZLE_GEOMETRY.md:385` 已记「撤换」 |
| S6·新 | `DISPUTES.md:28`/`:45`、`REPORT_paper.md:81` | 四处仍登记「注释须订正」，而代码已改 ⇒ **正本状态被低报**（AGENTS §3 文档权威更高，应订正文档而非代码） |
| S7·新 | `p3lib.py:82-84` | `jacobian_chart` 返回字面量 π/3，不做任何微分；`:24` `exp01_leaf_area.py:33` 却拿它当「数值积分」的输入 ⇒ 该腿在测一个常量 |
| S8·新 | `p3lib.py:106`/`:111-112` | `sample_edge`/`leaf_true_boundary` 文档把 “chart-straight” 与 “(true-curve)” 用括号并列，暗示二者同一；实测二者不同（真曲边模型在 `edge_geom.py:46/:71`） |
| S9·新 | `p1_rootcause.cpp:140` | 「极冠/赤道交界 z=2/3」采样的是叶 (Ns/2, Ns−1)，其四角 z∈[0.917, 0.999]，**不跨接缝**；真正的接缝叶是 (0, Ns−1) |
| S10·新 | `REPORT_paper.md:254-255`、`:45/:46/:71/:97` | 孤儿文献 2 条（Roukema 2007、Fernique MOC 2015）；正文用 `[文献 V1..V4]`、文献表用 `1.–7.`，**两套编号无映射**（AGENTS §5 要求正文用编号标注） |

---

## 5. 你主动构造的反例

### 5.1 构造「把 `exp_sim01` 的 `outside` 真值改成非零，看门会不会红」
- **期望推翻**：`drops_cross_face_excluded` 是有判别力的诊断量。
- **结果：推翻成功**。我逐行推出：`:508` 的值在 `:508-514` 之间**零消费**（死算），`:514` 无条件覆盖，落点 `:746`。而且——即使把 `:514` 修好，归档 JSON **逐字节不变**（真值确为 0，`:609` 的 `n_tot += 1` 在 `:585-586` 的 continue 之后，而 `drops_total = 25600 = 160²` 证明 `drop_single` 全 True）。⇒ 修它不产生任何可观察后果，**红灯不可诊断**。
- **否决的替代解释**：不是「`:514` 掩盖了真实缺口」——主腿确实一个 drop 都没剔（`REPORT_paper.md:147` 的声明为真）。被掩盖的是**计数能力**，不是计数结果。

### 5.2 构造「把 NC-A 的分子 `Ff` 换成 `Σw ≠ 1` 的权重，看门会不会红」
- **期望推翻**：NC-A 能抓「漏归一化」。
- **结果：推翻成功**。`res_flat = E_p[om]/E_p[ap] − 1` 是比值，`Σw` 约掉。归档 `:784` 的 `judgement` 写「漏归一化时立刻破」——**这句是假的**。这条是本片最干净的新恒真门。

### 5.3 构造「把 `solid_angle_gnomonic` 的指数从 3/2 改回 2，看 M2 门会不会红」
- **期望推翻**：`M2_slope_abs_spread_le_2e-3`（`:731`）守得住指数订正。
- **结果：推翻成功**。该门比的是**四个足迹尺度之间斜率的离散度**；指数误差 `Ω(2)/Ω(3/2) = (1+u)/2 ≈ 1 − ρ²/4` 与足迹尺度**无关**，四个尺度同步移动 ⇒ 离散度仍 ≈ 0 ⇒ **照样绿**。能抓它的 `slope_on_physical_crop`（`:720`，归档 7.5e-5）**无任何门**。这条是「本文件花大力气订正的缺陷，恰是它自己的门看不见的缺陷」。

### 5.4 构造「把 `M3_interior_bitwise_zero` 的几何改坏，看门会不会红」
- **期望推翻**：15126 个 drop 的逐位 0 是证据。
- **结果：推翻成功**。interior ⇒ 只命中 1 叶 ⇒ `clip_rect`（`:161-186`）四次半平面裁剪**原样 append 顶点**（无交点就不算 `t`）⇒ 返回与 `:520` 计算 `ad` **完全同一**的 4 点列表 ⇒ `a ≡ ad` ⇒ `wg = a/ad ≡ 1.0`（IEEE 自除恰 1.0）⇒ `|sw−1| ≡ 0.0`。**数学上不可能红**；15126 个 0.0 是恒等不是证据。

### 5.5 构造「把 `exp10` 的 `sigma_pix` 下限去掉，看门会不会红」
- **期望推翻**：`exp10` 的 SNR 控制点有物理内容。
- **结果：推翻成功**。`x = 1000×(9.6963e-6)² = 9.40e-8`，`max(x,1.0)` 恒取 1.0 ⇒ SNR = 1.88e-6。归档 `reconstruction_range = [−0.685, +0.510]` 里的**负 SNR** 就是「这帧全是噪声」的签名。门全绿——因为**所有门都与信号量级无关**。

### 5.6 构造「把 `run_all.sh:43` 的命令手工解析一遍」
- **期望推翻**：归档 `p1_t4_pole_full.out` 可由本脚本复现。
- **结果：推翻成功**。argv 落位 `csv="-8"`、`lo=8`、`hi=0.5`、`st=0.5` ⇒ 循环零次。而归档写 `[-8.0,8.0] step 0.50` / `配置数=1089`。**脚本已无法复现自己的归档**，且 `:68` 会用空结果覆盖它。

### 5.7 构造「用第一性原理重算 `exp06` 的余量门余量」
- **期望推翻**：裕量 ≥20% 是宽裕的。
- **结果：部分推翻**。归档 `margin_min = 1.2001933288902629`，门是 `> 1.2` ⇒ **实际只容忍 0.016% 余量**；任何几何修正都会破门。

---

## 6. 盲复算

方法：先只看源码（不看任何归档、不看前三轮结论）独立推导，再与仓内已提交归档和既有结论比对。

| # | 盲复算项 | 我的独立结论 | 与既有结论比对 | 判定 |
|---|---|---|---|---|
| B-1 | `exp_sim01:508/514/746` | 真实计数被裸赋值覆盖；死算；落点 `drops_cross_face_excluded` | 与 RR10 / `TAUTOLOGY_REGISTER:287` 一致 | **一致** |
| B-2 | `exp_sim01` M1 门能否判红 | **能红**（`x_tot` 用全网格，被剔像元的通量留分母） | RR10 称「结构上无法判红」 | **原结论偏严**，我修正为「能红但红后不可诊断」 |
| B-3 | `e4:147` 方差律 | `(α²·X)/X ≡ 9`，恒真 | 与 RR05 / R2-S6 一致 | **一致** |
| B-4 | `run_all.sh:43` | argv 错位 ⇒ lo=8>hi=0.5 ⇒ 零次执行 | 与 RR08 C-10 一致 | **一致**（并补出「覆盖正确归档」这一后果） |
| B-5 | `variants.h` REC-1 预算推导 `:150` | **正确**：`Σ(len_i)=edge_len` ⇒ 总亏损 ≤ budget/4，量纲自洽 | 前几轮未正面核过 | **一致（我确认无误，非缺陷）** |
| B-6 | `polar_common.h:185` 与 `:167` | `st` 与 `sin(2·asin(s/√6))` **逐项恒等** ⇒ T3 mode1 是自证 | 前几轮未核 | **新发现，见 M16** |
| B-7 | `exp10:239` IDW 精确性 | 被 `:92` 短路，归档精确 0.0 ⇒ 恒真 | 前几轮未核 | **新发现** |
| B-8 | `exp03:81-83` 的「叶」 | `dz` 只有真叶 z 跨度一半 ⇒ 不是 HEALPix 叶；门为分割不变式 | 前几轮未核 | **新发现** |
| B-9 | `exp10` 量级 | `sigma_pix` 恒 1.0 ⇒ 负 SNR | 前几轮未核 | **新发现** |
| B-10 | `run_scan.py:69/:73` | `1.0826` 是人工转抄的硬编码 oracle | 前几轮未核 | **新发现** |
| B-11 | `REPORT_paper.md:240` 域外声明 | 声明域与主腿覆盖**吻合**，跨叶有 10474 drop 真实覆盖，无夸大 | 与 RR10 的「诚实度高」一致 | **一致（判不虚张）** |
| B-12 | `DISPUTES.md` A-P3-09「已订正」 | 全仓 grep `211034.6` 在 5 个目标文件中 **0 命中** ⇒ 属实 | 前几轮未核 | **一致（我确认非缺陷）** |

**结论：本片既有判定对 `B-1/B-3/B-4` 一致；我在 `B-2` 上判定原结论**偏严**（把「能红但不可诊断」说成「结构上无法判红」），其余 8 项为新发现。**

---

## 7. 子代理派发记录

**派发 6 个 / 3 个独立任务**（`run_code` 不可用，改用 `subagent`；每个任务派发两次构成**盲复现对**，第二次不发前一代理的任何结论，用于检验我的裁决是否可独立重现）。

| 代理 | 任务 | 结论 |
|---|---|---|
| A（2 个，盲复现对） | `exp_sim01_m16_forward_conservation.py` 831 行 + 归档 JSON，7 问定向核验 | 独立重现我的 B-1/B-2/M16 判定；**新增并被我采纳**：`NC-A` 的代数证明与 `judgement` 假判词；`M2_slope_abs_spread` 对 ρ² 型缺陷的盲区证明；`:507`/`:644` 掩码口径不一致 |
| B（2 个，盲复现对） | route1/route2/route3 的 12 份脚本，恒真门三型 + 双向体检 | 独立重现我的 M7/M8/M9/M11/M12 判定；**新增并被我采纳**：`exp03` 的叶格非 HEALPix 叶；`exp10` 的量纲塌陷；`exp01_leaf_area` 的 `sum_res ≡ mean_res` |
| C（2 个，盲复现对） | kcorr 链 + 文档面（`tables.md`/`KEY_RESULTS.md`/`REPORT_paper.md`/`README.md`/`DISPUTES.md`/`摘要.md`/`run_all.sh`） | 独立重现我的 M13/M14 判定；**新增并被我采纳**：`g0b gate_pass:false` 的选择性报告（用第三个文件的数字背书）；`read_tables.py:24` 硬编码 PASS；`KEY_RESULTS.md:42` 伪引已注销的 `4e-8`；`run_all.sh:69-70` 快照自校验恒绿 |

**我逐条复核并采纳的（12 条）**：A 组的 NC-A 证明（我用 `shoelace_abs` 返回 `abs` 与 `wg=a/|ad|` 独立验证，确认负权重不可能 → 恒真成立）；B 组的 `exp03:81-83`（我用 `p3lib.py:12-13` 的权威 chart 口径独立算出真实 z 跨度 `4/(3N)`，确认 `2/(3N)` 是其一半）；B 组的 `exp10` 量级（我独立算 `A_pix=9.40e-11`、`x=9.40e-8`、SNR=1.88e-6，并用归档 `reconstruction_range=[-0.685,+0.510]` 交叉确认）；B 组的 `exp01_leaf_area` `sum_res ≡ mean_res`（我代数验证并核对归档八行逐位相同）；C 组的 `g0b gate_pass:false`（我自己 `python3` 读归档确认为 `False`，并确认 `KEY_RESULTS.md:50` 的两个数字都不来自 g0b）；C 组的 `4e-8` 伪引（我自己 `git grep -c 4e-8 -- lib/` 得 0 命中，并读了 `spherical_overlap.cpp:1009-1010` 现值）。

**我否决的（9 条）**：
1. **「`exp_sim01` 的 M1 门结构上无法判红」**—— 否决。`:629` `x_tot` 用**全网格无掩码**，单个被剔像元即偏离 ~3.9e-5 ≫ 1e-12 ⇒ **能红**。原结论偏严，我改判「能红但红后不可诊断」。
2. **「`e6:122` 的门因 l'Huilier 自行归一化而恒绿」**—— 否决。我读归档：`lhuilier_rel_shift_max = 2.98e-07` **未过 1e-12**，该门**确实判了红**。它是真门，问题在红灯不可见（B1），不在门本身。
3. **「`exp06:58` 的 `isfinite` 守卫静默丢弃极冠叶、使门无法判红」**—— 否决。归档 `worst_leaf_fij` 在 N=8..64 为 `[0,0,7]`/`[0,0,15]`/`[0,0,31]`/`[0,0,63]`，**face 0 的极冠角叶正是 argmax**，守卫没有掩盖最坏情形。
4. **「`p3lib` 的极冠公式与生产 `xyf2ang_replica` 不等价」**—— 否决（公式层）。代数推演后两个 φt 分支都化简为 `z = 1 − s²/3`，与 `p3lib:55` 一致。真正的缺陷是**数值路径**（`:74` 用 `sqrt` 稳定式 vs 生产 `acos(z)`），我另列为 M15/S7 相邻项，不重复计数。
5. **「`run_all.sh:43` 的扫描区间反向 ⇒ 扫描退化」的前一轮表述需精确化**—— 部分修正。退化成立，但归因要写全：`csv` 吞掉 `-8`（垃圾文件）、`lo=8>hi=0.5`（零次执行）、`st` 回落默认 0.5（恰好与归档一致故不易察觉）。
6. **「`tables.md` 混用两套 σ 而未标注」**—— 否决。`tables.md:24` **确实**标注了「MAD 尺度(正本估计器口径)」，未伪装。真实缺陷是输入 σ=10.0 从未披露 + `sigma`/`sigma_bg` 两符号全文无定义。
7. **「`README` 谎报退出码能力 / 破坏性未披露」**—— 否决（对 `code/audit/run_all.sh` 与 `run_all.sh` 分别处理）。`code/audit/run_all.sh:22/:28/:33/:86-88` 的 `set -eu` + RC 累加 + `exit $RC` 全部核实存在，README `:78-79` 描述准确；`run_all.sh` 的覆写也已在 README `:83-84` 明写。**只否决「未披露」，不否决「无保护」**（根入口确实无保护，见 B3/B1）。
8. **「`A-P3-02/04` 文档自称已解决、代码未改」**—— 否掉原假设，**反向成立**：代码已改（`spherical_overlap.cpp:1009-1010`），是四份文档仍登记为待改 ⇒ **正本状态被低报**（S6）。
9. **「`e6` 的 `ok` 掩码构成选择性隐藏」**—— 降级为建议。掩码确实丢掉病态样本，但 `:96` 的 `n_valid` 被报出并在 txt 打印，丢弃数可见，不构成「隐去判红臂」。

---

## 8. 自证段（可复跑命令）

```bash
cd "/workspace/Astro CS Database"
git rev-parse HEAD        # 期望 f9650dd0aed97d7f261e5e6547f4fdb505bc313b
git status --porcelain    # 期望空（审稿全程零 git 写）

# B1 三条活红灯（红灯已入库随发布，退出码却恒 0）
python3 -c "import json;d=json.load(open('实验/healpix-polar/results/audit/route1/e6_lhuilier_vos.json'));print(d['non_normalized_divergence']['verdict'], d['non_normalized_divergence']['lhuilier_rel_shift_max'])"
python3 -c "import json;d=json.load(open('实验/healpix-polar/results/audit/route1/e3_circumradius_scan.json'));print(d['monotone_increasing'], d['max_measured'], d['lonlat_contrast'])"
python3 -c "import json;d=json.load(open('实验/healpix-polar/results/audit/route1/e4_flux_conservation.json'));print(d['quantization']['inv_strict_bound_holds'], d['variance_law'])"
# 期望：FAILED 2.978908291985819e-07 / False 1.13233434346079 {'4':0.952…, '8':1.142…, '16':3.125…}
#      / False {'ratio_min': 9.0, 'ratio_max': 9.0, 'expected': 9.0}

# B1 零可失败路径
grep -rn "assert \|sys.exit\|raise SystemExit" 实验/healpix-polar/code/audit/ | grep -v '\.md:'   # 期望 0 命中
grep -rn "open(.*['\"]r" 实验/healpix-polar/code/audit/                                            # 期望 0 命中

# B2 NC-A 恒真 + 假判词
sed -n '618,619p;646p;784p;790,792p' 实验/healpix-polar/code/audit/sim/exp_sim01_m16_forward_conservation.py

# B3 run_all.sh:43 实参落位（手算 argv）
python3 - <<'EOF'
argv=["./p1","t4","-8","8","0.5"]
print("csv=",argv[2],"lo=",float(argv[3]),"hi=",float(argv[4]),"step=0.5(缺省)")
print("循环次数=", 0 if float(argv[3])>float(argv[4]) else -1)
EOF
sed -n '2,3p' 实验/healpix-polar/results/p1_t4_pole_full.out     # 期望 [-8.0,8.0] step 0.50 / 配置数=1089

# B4 e4 方差律恒等式
sed -n '69p;142,147p' 实验/healpix-polar/code/audit/route1/e4_flux_conservation.py

# M4 exp03 的「叶」不是 HEALPix 叶
sed -n '12,13p;81,83p' 实验/healpix-polar/code/audit/route2/p3lib.py 实验/healpix-polar/code/audit/route2/exp03_flux_conservation.py
python3 -c "
import numpy as np;d=json.load(open('实验/healpix-polar/results/audit/route2/exp03_flux_conservation.json'))
print('wrongDp-1/pf2 =', d['fixture']['S_over_B0_wrongDp_mean']-1.5625, '（退化指纹）')"

# M1 exp10 量纲塌陷
python3 -c "
import numpy as np,json
SCALE=2*np.pi/180/3600; A=SCALE**2; x=1000*A
print('A_pix',A,'x',x,'sigma_pix',max(x,1.0),'SNR',x/0.05)
d=json.load(open('实验/healpix-polar/results/audit/route2/exp10_chain_usecase.json'))
print('reconstruction_range',d['idw']['reconstruction_range'],'dev_at_ctrl',d['idw']['max_abs_dev_at_control_points'])"

# M2/M11/M12
sed -n '90,93p;223,226p;180,181p;237,241p' 实验/healpix-polar/code/audit/route2/exp10_chain_usecase.py
sed -n '99,101p;128,130p' 实验/healpix-polar/code/audit/route3/exp01_leaf_area.py
sed -n '84p' 实验/healpix-polar/code/audit/route3/exp07_quantization.py
python3 -c "import json;d=json.load(open('实验/healpix-polar/results/audit/route3/exp07_quantization.json'));print([(v['S0'],v['var_ratio_minus_1'],v['pred_2rsig_plus']) for v in d['variance']])"

# M13 kcorr N_retained
grep -n "N_retained ~ \|n_touched" 实验/healpix-polar/code/audit/kcorr/mc_kcorr.py | head -3
grep -n "N=225\|N=251" 实验/healpix-polar/results/audit/kcorr/tables.md 实验/healpix-polar/results/audit/KEY_RESULTS.md

# M14 run_all.sh 快照自校验
sed -n '69,70p' 实验/healpix-polar/run_all.sh

# M15 variants.h：容差被丢弃 + 变体 B 无调用者
sed -n '190,193p;254p;265,266p' 实验/healpix-polar/code/variants.h
grep -rn "chart_overlap_leaf\|chart_curve_crossings" 实验/healpix-polar/code/ | grep -v variants.h   # 期望 0 命中

# M16 polar_common.h:185 与 :167 恒等
sed -n '167,168p;185,186p' 实验/healpix-polar/code/polar_common.h

# S5 伪引 4e-8（已注销对象）
git -c core.quotepath=false grep -c "4e-8" -- lib/        # 期望 0
sed -n '1009,1010p' lib/algorithms/drizzle/healpix_drizzle/spherical_overlap.cpp

# S10 孤儿文献与编号断裂
grep -c "Roukema\|Fernique\|HPX\|Multi-Order" 实验/healpix-polar/REPORT_paper.md   # 期望 2（全在文献表）
grep -o "\[文献 V[0-9]\]" 实验/healpix-polar/REPORT_paper.md | sort -u              # 期望 V1..V4

# 覆盖率自检（30 份 / 5526 行）
awk 'NR>=2464 && NR<=2493' run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml
```

---

## 8bis. 补录：锚点行号伪引（第六个子代理在交付件写定后送达，独立复核成立，合并入本件）

这些直接命中本项目固化检查项「伪引：核实被引句在该条款里是否逐字存在」，且**含裸从句**（无成对引号，只扫引号会漏）。我已对其中最硬的三条自行复核。

| 位置 | 引用写法 | 实际 | 判定 |
|---|---|---|---|
| `mc_kcorr.py:8` | 「300"/px (deg_per_px = 300/3600, **测试源码 :56**)」 | `control_median_mc_test.cpp:56` 是 `K_BAND_LO/K_BAND_HI`；真实位置 **`:67`** `const double deg_per_px = 300.0/3600.0;` | 伪引，行号错 11 行 |
| `mc_kcorr.py:39` | 「`# 正本 control_median_mc_test.cpp:139 的基 seed`」 | `:139` 是 `const int NMC = 2000;`；真实种子在 **`:198`** `std::mt19937 rng((unsigned)(20260816 + r));` | 伪引，行号错 59 行 |
| `mc_kcorr.py:22` | 「正本测试 **`:115/:160`** 的取值口径」 | `:115` 是 judge 自测用例行，`:160` 是 `for (uint32_t local : tile.touched)` | 伪引，两处皆非 |
| `docs/DISPUTES.md:65`（A-P3-07） | 「`spherical_overlap.cpp:1239`（有效代码锚）」 | `:1239` 是 quick-reject acos 注释块；叶面积原语在 **`:1289`** `return Scalar(PI/(3.0*nside*nside));` | 伪引，会把后续 agent 引到错代码行 |
| `docs/DISPUTES.md:72/:74`（A-P3-08） | 「`spherical_overlap.cpp:857` 的 12 为行为事实源」 | 实际在 **`:866`** `WCS_ADAPTIVE_MAX_DEPTH = 12;`（`:899` 使用） | 伪引，行号错 9 行 |
| `REPORT_paper.md:46` | 「`docs/science/DRIZZLE.md` SCI-DRZ-001 **§5 canonical 行 `:224`**」 | `:224` 落在 `## 14 Primary literature`；§5 的 canonical 核在 **`:47-53`** | 伪引，行号错约 171 行 |
| `docs/EXP-07-POLAR-摘要.md:4` | 「门禁 `DRIZZLE_GEOMETRY.md:236`」 | `:236` 是线程合并/累加结构；门禁实际在 **`:349-351`**（τ_rel=1e-6、ε_abs=1e-15 sr） | 伪引，行号错约 113 行 |
| `run_extra.py:9` | 「正本公式 N=5 时渐近式**低估 8.5%**」 | `docs/science/PHASE2_UPM.md:88-89` 逐字为「**恒为高估**…高估 **+9.5%**」；兄弟单元 `additive-sky-seamless/REPORT_paper.md:17` 已按 D-07 正式订正方向 | **方向相反且 P3 侧未回改**；与本单元 `tables.md:45`（0.9113）、`KEY_RESULTS.md:54`（「高估 9.6%」）自相矛盾 |
| `run_scan.py:8` | 「iid Gaussian ratio 0.997, **PHASE2_SAMPLER.md F3(b)**」 | `PHASE2_SAMPLER.md:605` 的 F3 条无 "0.997"；0.997 实际在 `PHASE2_UPM.md:355`；`PHASE2_SAMPLER.md:225/:275` 的 0.9972 是另一个量 | 双重误指 |
| `REPORT_paper.md:42`（KEY_RESULTS 转述） | 同一句 `4e-8` 注释，KEY_RESULTS 给 **10×**、DISPUTES A-P3-02 给 **6.3×**、A-P3-04 给 **12.5×** | 三方互斥，无一处说明口径差异 | 内部不自洽（各自按自己的 θ 读数算，均自洽但不可比） |

**已核实为真、故不列为缺陷的伪引候选**（防止下轮重复指控）：
- `REPORT_paper.md:46` 引 F&H 原句 `"where a factor of s² is introduced to conserve surface intensity"` —— 在 [arXiv:astro-ph/9808087v2](https://arxiv.org/html/astro-ph/9808087v2) §2 式(3) 后**逐字存在**；§2 式(2)–(5)、§7 式(6)–(10)、ℛ=1.662 全对。
- `tables.md:82` 用 `1/(1−r/3)`（r=0.5822<1）确为 F&H **式(10)**（r≤1 支），引用正确。
- `p3lib.py:30` 称 `spherical_overlap.cpp:711-774` 为 `xyf2ang_replica`：`:718` 确为函数定义行，711–717 是其上方注释块，函数体至 774。**引用为真**（保真度未测是另一回事，见 S7）。

**另一条产物孤儿（新增）**：`results/audit/kcorr/g3b_n5_hiprec.json` **无任何脚本生成它**（`run_extra.py:87-91` 只跑 g0b/g3b/g7，`direct_char.py:43-44` 只写 `direct_char.json`），却被 `KEY_RESULTS.md:54` 归给「run_extra.py / direct_char.py」，且 `read_tables.py:53-55` 消费它产出 `tables.md:45` 的「直接定征 400k」三数。其 mtime `9月27 02:15`，同目录其余 11 个 JSON 全为 `9月30 21:29` ⇒ **唯一未随批次重生成的陈旧件**，即 N=5 那一档「精度最高」的证据是不可复现的孤儿。

---

## 9. 未实测 / 待联网核验 / 交接项

- **未实测**：本片全部门的行为结论均由**读码 + 读已提交归档**得出。按纪律 4，未编译、未跑 ctest/pytest、未跑 `run_all.sh`、未跑任何 `.py`。凡需注入缺陷才能确证的判别力结论（如「门能否红」的动态证明），已在 §5 给出**代数证明**替代，未标注为实测。
- **待联网核验**：`REPORT_paper.md:254`（Calabretta & Roukema 2007）与 `:255`（Fernique MOC 2015）两条孤儿文献的存在性与相关性，需联网核；`edge_geom.py:4` 声称「verified verbatim from arXiv:astro-ph/0409513」但**仓内无核对记录**，需一手核验。
- **交接给前台的待执行项**（我无权执行）：
  1. 修 `exp_sim01:514`（去掉裸赋值）—— 但**修前须先给 `drops_cross_face_excluded` 加门**，否则修了也不可见；
  2. 全树加 `assert`/`sys.exit`，把三条已入库的红灯变红；
  3. 修 `run_all.sh:43` 为 `./p1 t4 "$LOGS/t4_pole.csv" -8 8 0.5`；**在此之前不要重跑 `run_all.sh`**，否则会用 0 配置的空结果覆盖 `results/p1_t4_pole_full.out`（README `:83-84` 已警示覆写，但未警示与此缺陷的叠加后果）；
  4. `testdata/HST_M16/` 被 gitignore（子代理实测），`exp_sim01` 的物理输入无法在仓内复核；
  5. `code/audit/run_all.sh` 不在本片清单内，README `:78-79` 对其退出码能力的描述**经交叉核对准确**，我未亲自读该文件。

---

## 10. 计数口径声明

本片涉及的判据数量按任务要求写明口径，避免歧义：

| 口径 | 数量 | 说明 |
|---|---|---|
| **门实例**（`exp_sim01` 归档 `gates_total`） | **16** | 1(M1) + 3(M2) + 5(M3) + 1(M4) + 4(M5) + 1(NC-A) + 1(NC-B)；NC-C **无门**，从不参与 PASS/FAIL |
| **去重门**（`exp_sim01` 去掉同表达式重复发布） | **15** | `:725-726` 的 `chart_term_shrinks_with_footprint` 与 `:733-734` 的 `M2_chart_term_decreases_with_footprint` 是**逐字同一表达式**，以两个名字发布，其一不计入 `gates_total` |
| **整改分母**（本片全部判据实例，逐文件点数） | 至少 **~75** 个门实例 / **~25** 个去重门族 | 明细：exp_sim01 16(15)；e4 **0 门**（8 个落盘布尔，其中 1 个实测为红）；e3 **0 门**（3 个报告量）；e6 1（实测红）；exp01_leaf_area(route2) 3；exp03 5；exp06 4；exp07 7；exp10 5；route3/exp01 3(+4 负控制未进门)；route3/exp04 4；route3/exp07 7；run_scan 3；mc_kcorr 0（库）；edge_geom 0（库）；p3lib 0（库）；5 个 C++ 探针 **0 门**（全为 `return 0`） |
| 本片报告的**阻断/须修/建议**分母 | **4 / 16 / 10** | 只计本片成员文件内的新增或改判，跨片登记（如 `TAUTOLOGY_REGISTER.md:287`）只标注不重复计数 |

**未给出的数字**（按纪律不报）：本片不对「整改分母」给出覆盖 `code/audit/` 全树（29 脚本）的完整计数——该树有 12 份不在本片清单内，我只对 30 份成员文件负责。跨片总账以 `run/GOVERN-08/审核包-R2/审稿-总账.md` 为准。
