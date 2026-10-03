# 取证 S5-A2：cone-search-constants 与 m42-realdata 两篇论文的对抗审稿

- 审稿分片：A2（实验单元论文）
- 仓库 HEAD：`ed33f57f15702d5d2b5efbc734f4d7c8e544a01b`
- 判定对象：`实验/cone-search-constants/REPORT_paper.md`、`实验/m42-realdata/REPORT_paper.md`
- 总判定：**阻断**（cone 论文 5 条阻断；m42 论文 0 阻断、8 条须修）

---

## 0 前置事实核验：这两个单元第一轮确实没被审过

```
git rev-parse HEAD                                    → ed33f57f15702d5d2b5efbc734f4d7c8e544a01b
git diff --name-only 1141939b^..HEAD | grep -c cone-search-constants  → 0
git diff --name-only 1141939b^..HEAD | grep -c m42-realdata           → 0
```

第一轮改动区间（`1141939b^..HEAD`，1595 files）对这两个目录**零命中**。两篇论文的结论均未经第一轮对抗审稿，本分片为首次审。

工作树核对（本人未做任何修改）：`git status --porcelain -- 实验/m42-realdata 实验/cone-search-constants` → 空。

---

## 1 读了哪些原文

| 文件 | 行数 | 读的范围 | 用途 |
|---|---|---|---|
| `实验/cone-search-constants/REPORT_paper.md` | 525 | **1–525 全部读完**（两段：1–270、270–525） | 审稿对象 A |
| `实验/m42-realdata/REPORT_paper.md` | 195 | **1–195 全部读完** | 审稿对象 B |
| `docs/science/PHOTOMETRY.md` | 407 | **1–407 全部读完**（两段：1–210、211–407） | 一级正本，口径比对基准 |
| `实验/m42-realdata/results/c1_photometry.json` | 1849 | `:1717-1849`（`gates` 块全文 14 行判据）；另 `:1-6`/`:6`/`:320` 结构定位 | 核对判据数、判红数、level 分组、逐条读数 |
| `实验/m42-realdata/results/c2_absolute_snr.json` | 629 | `:521-629`（`gates` 块全文 10 行判据） | 同上 |
| `实验/m42-realdata/results/c3_seam_additive.json` | 1315 | `:1210-1315`（`gates` 块全文 11 行判据）；另 `:30/:33/:1201-1206` | 同上 |
| `实验/m42-realdata/results/c4_leaf_allocation.json` | 682 | `:5-91`（面积守恒/层级块）、`:506-571`、`:536-682`（`gates` 块 14 行） | 同上 |
| `实验/photometric-magnitude/RESOLUTION_m42_curve_resolve.md` | 300 | `:95-154`（§3 受控对照表 9 变体、§4 判据带口径澄清） | m42 根因定案的读数正本 |
| `实验/photometric-magnitude/REPORT_paper.md` | — | `:145-150`、`:211-215` | 核对 0.057457 的真实语义 |
| `lib/algorithms/photometry/cpp/src/frame_photometry_fit.cpp` | 394 | `:163-184` | 核对 cone 论文 :30/:33-35 的代码锚 |
| `lib/algorithms/photometry/cpp/src/star_matcher.cpp` | 704 | `:148-165`（逐行读）；grep 定位 `:6/:21/:23/:373-376/:455/:545/:558/:567/:597/:612-627` | 核对 cone :344 与 m42 :60 的代码锚 |
| `lib/algorithms/photometry/cpp/src/spectrum_integrator.cpp` | — | `:87-91` | 核对 cone :343 的代码锚 |
| `实验/photometric-magnitude/code/scia_common.py` | — | `:245-249` | 核对 cone :342 的代码锚 |
| `实验/photometric-magnitude/code/step8_real_frame.py` | — | `:87-91` | 核对 cone :356 的代码锚 |
| `lib/infrastructure/scheduler/src/module_adapters.cpp` | — | `:4826-4845`（逐行读）；grep 定位 `:6187-6219`、`:6486-6487`、`:6544-6545` | 核对 c1 JSON 的 source 锚 |
| `实验/additive-sky-seamless/code/sci_c_common.py` | 480 | 函数定位 `:327/:356/:377/:414` | 核对 m42 :46 的 `:356-411` |

**目录枚举（含隐藏文件）**：

```
$ ls -la 实验/cone-search-constants/
total 28
drwxr-xr-x 2 dsh dsh 4096  9月26日 18:11 .
drwxr-xr-x 11 dsh dsh 4096 10月 2日 01:20 ..
-rw-r--r-- 1 dsh dsh 18697  9月27日 20:19 REPORT_paper.md      ← 全部内容，仅此一个文件

$ find 实验/cone-search-constants/ -mindepth 1
实验/cone-search-constants/REPORT_paper.md                     ← 无 code/、无 results/、无隐藏文件、无 README.md

$ git log --all --diff-filter=D --name-only -- 实验/cone-search-constants/   → 空输出
$ git log --oneline -- 实验/cone-search-constants/
19867470 chore: reports/ 目录退役收尾（组6批，四类处置）
0f97360e 实验: 补入 cone-search-constants 单元报告（锥形检索常数实验成稿）
```

→ 该目录**在全部 git 历史中从未存在过** `code/` 或 `results/`，也没有被删除过。

对照组 `实验/m42-realdata/` 结构完整：`code/`（7 个 .py + run_all.sh）、`docs/`（CRITERIA.md、DATA-SOURCES.md）、`results/`（4 个 json + SNAPSHOT.sha256）、README.md、REPORT_paper.md。

---

## 2 逐项检查清单

### ① 结论是否超出验证边界

**cone 论文 —— 严重超出。**

1. **把代码里的安全钳位读成科学成立域**。`实验/cone-search-constants/REPORT_paper.md:231-234`（§5.2）以「**下限 1.0°**」「**上限 10.0°**」陈述锥角半径的物理边界，`:257` 在 §6「成立条件」里再写一次 `[1.0°, 10.0°]`。该区间的真实来源是 `frame_photometry_fit.cpp:173-175` 的**输入域钳位**：

   ```cpp
   173:    if (fov_radius_deg <= 0.0 || fov_radius_deg >= 30.0) {
   174:        fov_radius_deg = std::min(std::max(fov_radius_deg, 1.0), 10.0);
   175:    }
   ```

   而 `fov_radius_deg` 本身（`:169-172`）是由帧几何**现场派生**的量：`pixel_scale_deg * sqrt(w²+h²)/2 * 1.2`，不是待标定常数。且钳位**只在 `(0, 30)` 之外触发**，区间内取值根本不经这段代码。论文自己在 `:38` 已认出「注意此处是**安全钳位**而非搜索优化」，却在 `:234`/`:257` 把同一段代码反读为「成立条件」。这是典型的「工程护栏 ⇒ 科学适用域」越界。
   正本定性明确：`docs/science/PHOTOMETRY.md:340`「FOV 三常数…为**项目约定**——文献腿查无出处、登记为约定」。

2. **合成域/单帧域的读数外推为普遍成立**。全文唯一的定量证据是 1 个 HST 帧 + 1 个 testdata 帧（`:205-221`、`:409-485`），而 `:254`（§6）给出无条件断言：「**猜想成立**：锥形搜索四项常数可从星表引导的 Cone Search 中鲁棒恢复，相对误差<0.2%」；`:242` 进一步称「四项常数在锥域内可被**唯一识别**到亚角秒级精度」。「唯一识别」没有任何条件数、后验或完整相关矩阵支撑，全文只给了单个配对相关 `:229` `corr(R_c, θ_p) ≈ 0.12`。

3. **精度适用范围无声明**。`:192-195` 报 `R_c` 相对误差 0.013%、标准误差 0.0012°，`:266` 却把工程路径写成「最大似然细化至 **0.001°精度**」——后者比被验证的读数还细一个量级，且没有对应的失效边界。

**m42 论文 —— 基本守住，局部越界。**

4. `:27-29` 把「创新点四 14/14 全绿」写成摘要级卖点，其核心项 `C4-G1`（逐 leaf 覆盖重数守恒）的判据本体是 `nused + nrej == candidates`（`c4_leaf_allocation.json:557`），即**按构造成立的恒等式**。论文在 `:170`（§3）已如实披露「14/14 全绿且是**零容差恒等**」，属已披露的表述偏重，建议前移披露而非改判。
5. `:156-157`（§2.4(d)）结论写成「**天区远离奇点** ∧ **尺度满足**」的合取，并给出四类距离读数（极冠叶 0、最近极点 82.898°、最近 `|z|=2/3` 接缝圆 34.708°、到 `z=0` 线 3.736°）+ 尺度条件（`A_drop(0.967″/px) = 2.198e-11 sr ≤ 1.4e-10 sr`），与 `c4_leaf_allocation.json:512/:524/:528-533` 逐位一致。这是本分片见到的最规范的适用域写法。

**判定：cone 越界（①不通过）；m42 通过（附 1 条建议）。**

---

### ② 有没有选择性报告

**cone 论文 —— 有，且成规模。**

6. **测试矩阵承诺 5 条，§4 结果只给 3 类读数，T02/T03/T04 全无读数**。`:158-164` 的矩阵：

   | ID | 数据来源 | 预期结果 |
   |---|---|---|
   | T01 | HST 真值 | ✅PASS |
   | T02 | 解析合成 | ✅PASS |
   | T03 | 解析合成 | ❌FAIL(星数不足) |
   | T04 | 解析合成 | ❌FAIL(背景污染) |
   | T05 | Testdata | ✅PASS |

   §4（`:168-221`）只有 4.1 Phase1、4.2 Phase2、4.3 负例注入（N01–N03）、4.4 Testdata。**T02/T03/T04 的任何读数在全文不存在**。而 `:257-260`（§6）恰恰用 T03/T04 这两条未报告的臂去支撑「成立条件：锥角半径 [1°, 10°]」——即**判红臂消失、结论保留**。

7. **摘要的数据规模与结果规模对不上**。`:15` 称「三个物理前向仿真帧（HST M16 真实星云结构 + **两帧合成星场**）与一帧真实 testdata **全部 PASS**」；但 §3.1（`:130-148`）只列 **3 类数据源**（HST 真值模板、纯解析合成数据、Testdata 真实帧），§11 附录（`:409-485`）只给 **2 份 JSON**（`hst.json`、`testdata.json`），连 `:290` 自称的 `results/analytic.json` 产物都没有。`run_experiment.sh`（`:295`）用 `results/*.json` 通配，暗示第三份，但它既不在 §11 也不在仓内。

8. **摘要的误差数字与结果表互相矛盾**。`:15` 写「四点联合估计的相对误差分别为 **0.12%/0.08%/0.05% / —**」；`:190-195` 的最终估计表写 **0.013% / 0.0005% / 0.00006% / 0.003%**；`:431-436` 的 JSON 附录写 **0.013 / 0.0005 / 0.00006 / 0.003**。摘要那组数字在全文任何位置都找不到出处，量级也差约一个数量级。

9. **φ_p 的相对误差内部不自洽**。`:194` 记 `φ_p` 真值 310.000°、估计值 309.9998°、相对误差「0.00006%」，`:434` 记 `0.00006`。但 0.0002°/310° = 6.45e-7 = **0.0000645%**，此项尚可；问题在 `:15` 摘要把它换成了 0.05%。

**m42 论文 —— 未见选择性报告，反向证据很强。**

10. 判据总数与 level 分组**与 JSON 逐位对上**。论文 `:13-14` 称 49 条（`data` 18、`external-consistency` 3、`control` 2、`positive-control` 4、`negative-control` 17、`degenerate-control` 3、`honest-boundary` 2）。本人逐文件统计 `gates.rows`：

    | 文件 | n | n_pass | n_fail |
    |---|---:|---:|---:|
    | c1_photometry.json:1718-1720 | 14 | 13 | 1 |
    | c2_absolute_snr.json:522-524 | 10 | 6 | 4 |
    | c3_seam_additive.json:1211-1213 | 11 | 10 | 1 |
    | c4_leaf_allocation.json:542-544 | 14 | 14 | 0 |
    | **合计** | **49** ✓ | 43 | **6** ✓ |

    level 分组合计 `data=1+3+5+9=18` ✓、`external-consistency=1+2=3` ✓、`control=2` ✓、`positive-control=1+2+1=4` ✓、`negative-control=5+2+5+5=17` ✓、`degenerate-control=2+1=3` ✓、`honest-boundary=2` ✓。与论文完全一致。

11. **主动报告对自己不利的读数**（这些是"绿臂"论文最常藏的）：
    - `:99-100` 「E 需要真值方差场——EXP-06 的真实数据臂因此**没有算 E**（`exp06_e3_real.json` 中 `eff_loss` 出现 **0 次**）」——披露上游单元缺读数；
    - `:74-80`（§2.1(c)）跨帧乘性一致性直接登记 **UNDECIDABLE**：「**不宣称一致、也不宣称不一致**，登记"用本产物不可判定"」；
    - `:124-125` 主动报告渲染器 FAIL 的真实原因 `covered_but_nonfinite_px = 466515`（2.78%）；
    - `:81-86` 列出「**已排除的假设**」两条，含一条否定性结论「星云污染可经 `background` 辨认（**不成立**）」；
    - `:181` 「偏差方向已知：C2 的 `var_true` 代理把系统差计入噪声 ⇒ **E 被高估**」——主动报出对自己结论不利的偏差方向。

12. c4 的每一个 headline 数字都与 `c4_leaf_allocation.json` 逐位相符：137,101,312 叶（`:5/:12/:34`）、Σnused=1,141,247,607（`:37`）、Σnrej=17,329,578（`:15/:38`）、Σcand=1,158,577,185（`:39`）、8.754e-09 = 地板 7.3%（`:91`）、order0 面积 3.644293e-04 sr（`:571`）、掩码 50,136,578（`:506`）、几何 50,905,958（`:507`）、控制点 33,472（`:512`）、门 1e-6（`:89`）。**无选择性**。

**判定：cone 不通过（选择性报告成规模）；m42 通过。**

---

### ③ 「诚实边界」章节是否覆盖失效域 / 奇点邻域 / 覆盖缺口

**cone 论文 —— 章节存在，但覆盖不足且不可核。**

13. 章节存在：`:17`（摘要末）、`:240-248`（§5.4 诚实边界）、`:268-271` 与 `:360-405`（UNRESOLVED 三条 + 影响范围 + 优先级）。
14. **失效域**：`:242-247` 列了边缘畸变/光谱依赖/时间稳定性三条，但**均无读数**。其中 F01（`:366`）写「**实测**显示边缘像素的通量响应有 **3-5%** 的系统偏离」——全文没有任何数据、脚本、产物路径或文献支撑这个「实测」数字，而整篇论文号称有机器可读结果（见 ④/⑤ B1）。这是诚实边界里唯一带数字的条目，恰是不可核的。
15. **奇点邻域**：`:254` 的「唯一识别」与 `:362-375` 的 F01「引入二阶畸变参数 $(k_1,k_2)$」都预设了**锥域边缘是连续的**，未讨论锥域与 HEALPix 极冠/`|z|=2/3` 接缝的奇点邻域如何交互。对照 m42 论文 `:152-157` 的四类距离实测，cone 这块是空白。
16. **覆盖缺口**：全文未提星表覆盖、锥域边缘截断、通带/谱覆盖缺口。对照正本 `PHOTOMETRY.md:179-183`（输入有效域）与 `:241-244`（退化条件表），cone 未做对应交代。

**m42 论文 —— 章节存在且质量高，但漏收两处已知失效域。**

17. §4 诚实边界（`:176-184`）四段俱全：未独立验证清单、偏差方向、单一天区限制、本次未改动的声明。
18. **奇点邻域覆盖充分**：`:152-156` 逐项给出到极点/接缝圆/极冠叶/`z=0` 线的实测距离，并明确「极冠叶 **0** ⇒ RC1/RC2a **不适用**」「RC3 域 ≈ 40″ ⇒ **不适用**」——即明确写出「哪些门在本数据上不适用」，这正是失效域的写法。
19. **覆盖缺口未收编**：`covered_but_nonfinite_px = 466515`（2.78%，`c3_seam_additive.json:1201-1206`）是典型的覆盖缺口（覆盖到但非有限），论文只在 `:124-125` 把它当作「渲染器 FAIL 的原因」顺带一提，**未进入 §4 诚实边界的失效域清单**。同理 §2.2 判红的 variance 平面缺陷，其适用边界（哪些帧/哪些域会受影响）也未进 §4。

**判定：cone 部分覆盖（须修）；m42 须补 2 项。**

---

### ④ 凡引用恒真门读数的地方是否已标注降级

**cone 论文 —— 未标注。**

20. **双边界 PASS 未报 n 与 gate_scope**。`:216-220`：

    ```
    Double-boundary check:
      σ_obs = 0.0265 mag
      σ_floor = 0.0182 mag
      σ_ceiling = 0.0547 mag
      Result: 0.0182 ≤ 0.0265 ≤ 0.0547 → ✅ PASS
    ```

    正本 `docs/science/PHOTOMETRY.md:381-388` 对同一判据规定了强制报告义务与恒真门禁令：

    > `:383` 「…在 `n ≲ 12` 时为负、在 `n ≲ 22` 时已趋零 ⇒ 对"把样本裁剪到只剩同质星"**没有判别力**…」
    > `:385` 「`max(rho_lo, 0)·σ_fit` 形式的下界恒为 0，而 `σ_obs ≥ 0` 恒真 ⇒ **属恒真门、无证据资格**（`standards/01` §7），一律不用；」
    > `:386-388` 「判据形态 = 显式最小样本量规则：`rho_lo ≤ 0` ⟺ `n ≤ (3·1.166)² = 12.236` 时，判据作用域取 `upper_only`，状态词返回 **`LOWER_BOUND_UNDEFINED`**（**不记 PASS**），并随产品一并报出 **`n` 与 `gate_scope`**」

    论文报了 σ_obs/σ_floor/σ_ceiling 和 `passed: true`（`:474-480`），**既没报 n，也没报 gate_scope，也没检查 `n ≤ 12.236`**。按论文自报的星数（`:443` matched_stars=247、`:481` 183）算 `rho_lo = 1 − 3.498/√247 ≈ 0.777 > 0`、n=183 时 `≈ 0.741 > 0`，**下界确有定义、此处不构成恒真门**——这一点必须公平地说清楚。问题在于：**该报告义务完全未履行**，读者无法从论文判断 PASS 是否落在 `LOWER_BOUND_UNDEFINED` 区。

21. **§2.3 声明判据适用「每个参数」，实际只有一个 mag 域读数**。`:116-124`：「对每个参数的估计值 $(\hat\theta,\hat\sigma)$，要求 σ_floor ≤ σ̂ ≤ σ_ceiling」。但 `:190-195` 四个参数的标准误差单位是**度**（0.0012°/0.0008°/0.0011°）与 mag 混列，而 `:216-220` 与 `:474-480` 的双边界检查只有**一个** mag 域 σ_obs=0.0265。R_c/θ_p/φ_p 三个几何参数**没有任何门**。这是覆盖缺口，也是判据适用面被缩小。

22. **N01/N03 是输入域字面判定，未降级**。`:199-203`：

    | 测试 | 异常注入 | 预期行为 | 实际行为 | PASS? |
    |---|---|---|---|---|
    | N01 | $R_c=0$ | 抛出 ValueError | 抛出 ValueError | ✅ |
    | N03 | 随机极轴 (θ=360°) | 钳位到合法域 | 钳位到 180° | ✅ |

    这两条不检验任何科学量：任何带入参域检查的实现都能让它们通过，N03 甚至是**代码保证**的行为（`:33-35` 的 clamp）。属 AGENTS.md §8「判据须在正确实现下绿、注入缺陷时红」的弱化形态——注入的是**非法输入**而非**科学缺陷**。论文未标注降级。
    N02（`:202`）以「50 次迭代未收敛」判 PASS，但未报未收敛时 `location` 的落点，即未说明该门是「拒绝」还是「静默返回坏值」。`PHOTOMETRY.md:183` 冻结了 `max_iter=50`，故「50 次未收敛」本身不携带判别信息。

**m42 论文 —— 大部分已标注，一处未标注。**

23. **显式建立降级政策并执行**。`:43-44`：「**判据必须非退化**。凡"真值无效应时度量不归零"的度量…一律降级为 `degenerate-control` 并明确写"无证据资格"」。执行情况：
    - `c1_photometry.json:1840-1846` `C1-G3-invariance-degenerate`，level=`degenerate-control`，desc 明写「⇒ 对绝对窗口**无信息，不得作判据**」；论文 `:67-69`（§2.1(b)）如实转述为「**退化对照**」并给出数值演示（平移 0.6 dex 后 MAD 差 1.11e-16）。✓
    - `c2_absolute_snr.json:620-626` `C2-G3b-optimal-degenerate`，desc 明写「该臂**恒真，无证据资格**」；论文 `:107` 在 E 表中标为「（**退化对照**）」并给 −2.22e-16。✓
24. **主动为判据加抗恒真门条款**。`:90-91`：「判据取"斜率 = 1 ± 0.05 **且** 自助 CI 半宽 ≤ 0.05"（后半条用于排除"CI 宽到必然包含 1"的**恒真门**）」，并在 `c2_absolute_snr.json:528` 落进判据 desc。✓ 这是正确的做法。
25. **未标注的一处（须修）**：`:96` 报「T2 斜率 **0.97432**（CI [0.19037, 1.61583]）**判绿**」。
    - 该 CI 半宽 = (1.61583−0.19037)/2 = **0.7127**，是论文自己在 `:90` 设的 0.05 阈值的 **14.3 倍**，且 1 落在区间内 ⇒ 正是论文 `:90-91` 亲口定义为"恒真门"的情形。
    - `c2_absolute_snr.json:559` 显示该判据实际写作「log-log 斜率 = 1 **+- 0.15**」，**根本没有**半宽条款，level 也标为 `data`（非 `degenerate-control`）。
    - 论文 `:96` 既未披露实际门限是 ±0.15（比 (a) 的 ±0.05 松 3 倍），也未把 T2 标为退化/降级。
    - 附带对照：T3（`:97`）判红，而 T3 的 CI 半宽 2.08 也远超 0.05。**同一条「CI 过宽」事实，在 T2 上被忽略、在 T3 上成为判红理由之一**——适用不一致。
    - 该绿读数还在 `:166`（§3）被用作"三者互相印证"的证据之一。

**判定：cone 不通过（未标注 + 判据面被缩小）；m42 须修 1 处。**

---

### ⑤ 与 `docs/science/` 正本的口径是否一致

**PHOTOMETRY.md 实际结构（全部标题行 + 行号，grep 只用于定位、正文已逐行读完）**：

```
  1: # Photometry Science (SCI-PHOT)
  7: ## 1 目的与非目标
 18: ## 2 符号表          ← 论文 :4 声称此处是「锥形搜索与常数定义」
 36: ## 2a 参考通量 `F_syn` 的合成口径（定义 · 量纲 · 适用域 · 证据）
 40: ### 2a.1 定义式
 65: ### 2a.2 量纲逐项推导
 79: ### 2a.3 与官方定义的对应关系
 93: ### 2a.4 `Q(λ)` 的规范地位
112: ### 2a.5 通带失配的误差量级（为什么通带形状是地基）
120: ### 2a.6 判定证据汇总（四类）
132: ### 2a.7 通带身份核对（装配期 fail-closed；正向约束）
171: ## 3 物理量和单位
175: ## 3a 坐标 frame
179: ## 4 输入有效域
185: ## 5 连续定义
215: ## 6 假设
224: ## 7 独立不变量
231: ## 8 极端/退化条件
246: ## 9 精度策略
250: ## 9a 口径问答：孔径测光与 PSF 域的边界
257: ## 10 不可接受变化
265: ## 11 验证 Oracle
272: ## 12 关联 ALG ID
277: ## 13 追溯与测试
284: ## 14 Primary literature（引用定位声明）
290: ## 14a 参考文献与参考代码库（含许可证）
313: ## 15 Acceptance
322: ## 16 方法链与方法学（不改 §5 公式与常数）
327: ### 16.1 方法链（逐步对应最高设计 §4.2 的节点顺序）
342: ### 16.2 物理单位消除的论证
349: ### 16.3 物理闭合反推不成立的论证
357: ### 16.4 误差预算的构成与出处
376: ### 16.5 星数依赖与降级语义
402: ### 16.6 指针
```

**无 §2.1。** 无「锥形搜索与常数定义」小节。§2 符号表（`:20-34`）共 14 个符号，**不含** `cone_radius`/`polar_theta`/`polar_phi`/`constant_offset` 任何一项。

**cone 论文 —— 口径不一致 5 处：**

26. `:4` 引用正本节不存在（详见第 4 节候选核证）。
27. `:353` 证据锚 A-02「最小匹配星数 | `PHOTOMETRY.md §2.1` | ✅确认」——`PHOTOMETRY.md` **无 §2.1**，且"最小匹配星数"在正本中的落点是 `:182`（`|r_consistent|>=3` 才进 IRLS）与 `:236`，不是 §2.1。
28. **最小匹配星数 ≥50 / ≥100 与正本直接冲突**。cone `:260`：「最小匹配星数：≥50（建议≥100）」列为成立条件。正本 `PHOTOMETRY.md:381` 原文：

    > 「**星数不构成拒绝条件**：不存在星点少到无法测光的图；任何可解析帧都完成测光定标并出产品…**不设固定星数门槛**、也不套用他帧的精度口径。」

    `:382` 进一步区分：「`§4` 的冻结门「`|r_consistent| ≥ 3` 才进 IRLS」是**求解前提**…不作"星数够不够"的**准入判据**」。cone 把 ≥50 写成"成立条件"，与正本相反。
29. **MAD 常数写法两篇相反**。cone `:112` 用全精度 `0.6744897501960817` ✓ 与正本 `:287`（权威值 = 全精度写法）一致。m42 `:59` 写 `MAD(r)/0.6745` ✗（见发现 S17）。
30. **双边界公式形态** ✓ 正确。cone `:123-124` 的 `σ_floor = (1 − 3·1.166/√n)·σ_fit`、`σ_ceiling = (1 + 3·1.166/√n)·√(Σ budget_j²)` 与 `PHOTOMETRY.md:383` 及 `RESOLUTION_m42_curve_resolve.md:135-137` 的「帧自算」写法一致。**此处 cone 无误，应予肯定**；但见发现 S7（报告义务未履行）。

**m42 论文 —— 口径不一致 3 处（其中 2 处是硬错）：**

31. **0.04505 的归因与正本相反（硬错）**。m42 `:65`：「改用正确通带**并计入 QE**后，同批 49 帧的逐帧 `2.5·sigma_residual_dex` 中位降至 **0.04505 mag**」；摘要 `:18` 同（「修后同批 49 帧中位 **0.04505 mag** 回上界内」）。
    读数正本 `实验/photometric-magnitude/RESOLUTION_m42_curve_resolve.md:107-108` 的两行原文：

    | 变体 | 改动 | 49 帧中位 (mag) |
    |---|---|---|
    | **V1_band_fix** | **只把通带改成 Baader R（仍无 QE）** | **0.04505** |
    | **V2_band_qe_fix** | **再计入 KAF-16803 QE** | **0.05104** |

    且 `:124-125` 原文：「**QE 未配置使散度略升**（0.04505 → 0.05104，+13%）」。
    ⇒ **0.04505 是「通带改对、QE 仍未配（Q≡1）」那一臂的数；「通带改对并计入 QE」的数是 0.05104。** 论文把 0.04505 归给"并计入 QE"，同时又在同一句把「叠加 `Q≡1`（未配 QE）」列为根因之一，自相矛盾。
    **结论本身不受影响**：0.05104 仍低于 0.057457 参照值，"回上界内"成立（fail-closed 方向）。但数字归因反了，且是本分片唯一一处论文数字与一手读数正本**直接矛盾**。
32. **「band 上界」措辞已被兄弟单元明文判为不精确（硬错，订正未落实）**。m42 `:62`：「EXP-04 band 上界 0.057457 mag ⇒ **49/49 超界**」；`:65` 两处同；摘要 `:17` 更写「EXP-04 **已发表量级上界**（0.057457 mag）」。
    兄弟单元 `RESOLUTION_m42_curve_resolve.md:138-141` 原文：

    > 「2. **`0.057457 mag` 是 EXP-04 帧 B 的 `σ_obs`（观测值），不是带上界**（`REPORT_paper.md:147-149` 列的是三帧 `σ_obs`；三帧的 `σ_ceiling` 为 0.056830 / 0.103015 / 0.159899）。`实验/m42-realdata/code/c1_photometry.py:21-23` 把它写作"EXP-04 band 上界"是**不精确的表述**。」

    本人复核 `实验/photometric-magnitude/REPORT_paper.md:145` 原文：「**仿真腿**：三帧物理前向仿真全 PASS，σ_obs = 0.045344 / **0.057457** / 0.051718 mag」——确认 0.057457 是三帧仿真 σ_obs 之一（恰为三帧最大者），不是被定义的"上界"。`c1_photometry.py:21-23` 亦仍是旧措辞（「≤ 0.057457 mag（EXP-04 三帧物理前向仿真 band 的上界）」），说明该订正**只落在 RESOLUTION 文档，未回改代码注释与 m42 论文**。摘要"已发表"三字尤其不成立。
33. **`docs/ASTROCS_DESIGN.md` 文件名失效**。m42 `:37`（「`docs/ASTROCS_DESIGN.md` §12.2 要求三类数据齐备」）与 `:55`（「`docs/ASTROCS_DESIGN.md:120-128`」）指向的文件**不存在**：

    ```
    $ ls docs/
    ACSD_DESIGN.md  DOCUMENT_INDEX.yaml  GLOSSARY.md  README.md
    detail  engineering  science
    $ ls docs/ASTROCS_DESIGN.md
    ls: 无法访问 'docs/ASTROCS_DESIGN.md': 没有那个文件或目录
    ```

    顶点文件现名 `docs/ACSD_DESIGN.md`。同一失效名还散落在 `c1_photometry.json:1763/:1799` 与 `c3_seam_additive.json:1301`。
34. **MAD 常数 0.6745（硬错）**。m42 `:59` 写 `sigma_residual = MAD(r)/0.6745`。`PHOTOMETRY.md:287` 原文：「4 位截断写法 `0.6745` 与全精度值相对差 **+1.5196e-05**，**只允许出现在「≈」语境并标注该偏差**；权威值 = 全精度写法」。论文既未用 ≈、也未标注偏差。**且与本单元自己的代码矛盾**：`c1_photometry.py:16` 写「sigma_residual = MAD(r_inliers)/**0.6744897501960817**」，`:239/:241/:397/:399` 同为全精度。即代码对、论文错。

**判定：cone 不通过（5 处）；m42 须修 4 处。**

---

## 3 发现清单

### 3.1 阻断（5 条，全部在 cone 论文）

| ID | 位置 | 现状 | 应为 | 证据 |
|---|---|---|---|---|
| **B1** | `实验/cone-search-constants/REPORT_paper.md:5` | 抬头宣称「**机器可读结果**：`results/phase1_phase2.json`、`results/final_constants.json`」，两份文件均不存在；整个单元只有一个 md | 补入真实产物，或删除该行 | `ls -la`/`find` 见目录仅 1 文件；`git log --all --diff-filter=D -- 实验/cone-search-constants/` → 空（历史中从未有过 `results/`，也从未删过） |
| **B2** | 同上 `:7` | 宣称「含可复现脚本、三类实验数据…Oracle 核验、负例注入与**完整证据链**」，四类实体全部不存在 | 同上 | §7（`:275-318`）列的 12 个入口 `run_experiment.sh`/`build.sh`/`src/simulate_analytic.py`/`simulate_hst.py`/`process_testdata.py`/`collect_results.py`/`negative_injection.py`/`generate_report.py`/`src/cone_search.cpp`/`src/irsels_fit.cpp` 等**零命中**；`:301` 还宣称本报告由 `generate_report.py` 生成 |
| **B3** | 同上 `:4`、`:353` | 引用正本节「`docs/science/PHOTOMETRY.md`（…；§2「锥形搜索与常数定义」）」与「`PHOTOMETRY.md §2.1`」，两处目标节**均不存在** | 改指真实存在的节，或先补正本 | `PHOTOMETRY.md:18` = `## 2 符号表`；全文无 §2.1，无「锥形搜索与常数定义」。`polar_theta_deg`/`polar_phi_deg`/`constant_offset_mag` 在 `docs/ lib/ eng/ 实验/` **零命中**（`cone_radius_deg` 仅命中无关的 `lib/algorithms/platesolve/tools/diag_projection_plot.py:156`，且那里是 `fov_radius_deg*1.05` 的查询半径）。`docs/` 中「锥形搜索」指的是 `gaia_client_cone_search`（`docs/detail/infrastructure/22_gaia_xpsd_client.md:78-81`，参数只有 `ra`/`dec`/`radius_deg`，**无极轴方向**） |
| **B4** | 同上 `:354` | 证据锚 A-03「双边界公式 \| `PHOT-GATE-DROP-001` \| ✅确认」——该 claim ID 的真实语义是**被删除的门** | 改指正本双边界判据出处 | `PHOT-GATE-DROP-001` 全文语义 = 「跨帧一致性不是门禁」（`PHOTOMETRY.md:15-16`、`:263`；`docs/…/06_photometry.md:113-115` 记「人为加入跨帧 k/scale 一致性门 ⇒ **必须判红**」）。它认证的是**一个门被移除**，与「双边界公式存在」语义相反，用作该公式的「✅确认」是把 DROP 当 SUPPORT |
| **B5** | 同上 `:340-344` | §8.2「开源代码对照」三行全部不成立（路径、行号、内容） | 逐条重锚 | ① `:342` `/实验/shared/scia_common.py:247` —— 路径不存在（真实 `实验/photometric-magnitude/code/scia_common.py`），且 `:247` 是 `Sg = np.interp(xg, wl, S)`（谱插值），非「Gaia XP 光谱积分 `f_syn()`」；② `:343` `/lib/algorithms/spectroscopy/cpp/src/spectrum_integrator.cpp:89` —— 目录不存在（真实 `lib/algorithms/photometry/cpp/src/`），且 `:89` 是 `w1 = std::fabs(m_right - m_cur)`（Akima 斜率），非「复合 Simpson 求积」（`PHOTOMETRY.md:304` 正本口径 = Akima + Simpson）；③ `:344` `star_matcher.cpp:156` —— 逐行读为 `destroy(node->left);`（k-d 树析构），真实 Tukey 权重在 `:567` `w = tmp * tmp;  // Tukey biweight 权重 (1-u^2)^2`，常数 `_TUKEY_C` 在 `:23`、`_MAD_SCALE` 在 `:21`。三行均标「无差异」 |

### 3.2 须修（20 条）

**cone 论文（12 条）**

| ID | 位置 | 现状 | 应为 | 证据 |
|---|---|---|---|---|
| S1 | `:6`、`:416`、`:454`、`:524` | 抬头与 JSON 样例称 `` `VERSION` = 0.11.0-alpha.3 ``，实际 `VERSION` = **0.1.0-alpha.1**；`:6`/`:525` 称 HEAD = `d8495a65…`，实际 HEAD = `ed33f57f…` | 删除版本/commit/日期（AGENTS.md §5 禁入正文） | `cat VERSION` → `0.1.0-alpha.1`；`git rev-parse HEAD` → `ed33f57f15702d5d2b5efbc734f4d7c8e544a01b`（`d8495a65` 确在历史中，即非编造，是**过期**） |
| S2 | `:15` vs `:190-195`、`:431-436` | 摘要相对误差 0.12%/0.08%/0.05%/— 与结果表 0.013%/0.0005%/0.00006%/0.003% 矛盾，摘要那组数全文无出处 | 摘要与表对齐 | 逐行读 `:15`、`:190-195`、`:431-436` |
| S3 | `:158-164` vs `:168-221` | 矩阵 T03/T04（预期 FAIL）读数全缺，而 §6 成立条件正靠这两臂支撑 | 补 T02–T04 读数 | §4 全文（`:168-221`）无 T02/T03/T04 任何字样 |
| S4 | `:83-85` vs `:172-177`、`:264` | 伪代码 R∈10 档 × θ∈19 档 × φ∈72 档 = **13680** 个候选，`:173` 却报「360 candidate points」；`:264`「步长 0.5°~1.0°」与伪代码步长（1.0/10/5）矛盾 | 统一网格定义与报数 | 逐行读 `:83-85`、`:172-177`、`:264` |
| S5 | `:231-234`、`:257` | 把 `frame_photometry_fit.cpp:173-175` 的安全钳位当作科学成立域 [1°,10°] | 明确其为工程护栏；FOV 区间定性按正本登记为「项目约定」 | `frame_photometry_fit.cpp:169-175`（钳位仅在 `(0,30)` 外触发，值由 `:169-172` 几何派生）；`PHOTOMETRY.md:340`「FOV 三常数…为**项目约定**——文献腿查无出处」；论文自己 `:38` 已称其为「安全钳位」 |
| S6 | `:260` | 「最小匹配星数：≥50（建议≥100）」列为成立条件 | 改为求解前提表述 | `PHOTOMETRY.md:381`「**不设固定星数门槛**」、`:382`「`|r_consistent|≥3` 是**求解前提**…不作准入判据」、`:182` |
| S7 | `:216-220`、`:474-480` | 双边界 PASS 未报 n、未报 gate_scope、未查 `n ≤ 12.236` | 补报 n 与 gate_scope；命中 LOWER_BOUND_UNDEFINED 时不记 PASS | `PHOTOMETRY.md:385-388`（恒真门禁令 + `n ≤ (3·1.166)² = 12.236` 规则 + 「**不记 PASS**」+ 「随产品一并报出 `n` 与 `gate_scope`」） |
| S8 | `:30` | 「四项常数…在 `frame_photometry_fit.cpp:172-173` 中被**显式配置**」 | 删除或改为几何派生描述 | 行号错（实为 `:169-175`）；该处只有 `fov_radius_deg`，四项常数一个都不在（见 B3 全仓零命中） |
| S9 | `:199-203` | N01/N03 是入参域字面判定（ValueError / clamp），无科学判别力，未降级 | 标注为 `degenerate-control` 或替换为科学负例 | AGENTS.md §8「判据须在正确实现下绿、注入缺陷时红」；N03 的 clamp 行为由 `:33-35` 代码保证 |
| S10 | `:366` | F01「**实测**显示边缘像素的通量响应有 3-5% 的系统偏离」——全文无任何数据/脚本/产物/文献支撑 | 补一手出处，或降为待测假设 | 逐行读 `:360-375`；对照 `:5` 声称的机器可读结果不存在（B1） |
| S11 | 目录级 | 缺 `README.md`（7 个兄弟单元均有）；`docs/` 内零引用，单元无向上追溯 | 补 README；补正本追溯 | `ls 实验/*/README.md` → 7 个，缺 cone-search-constants；`grep -rn cone-search-constants docs/` → 空（仅 `实验/README.md:21` 在同层提及） |
| S12 | `:326-336` | §8.1 三条文献定位（Gaia DR3 §8.1.1 Fig. 27 / 表 B.3；ETC Manual §4.2 与附录 D 的"HLSP drz WCS 精度/系统偏移统计"）未经核验 | 核实并给永久链接 | **待联网核验**——本分片未联网，不下结论 |

**m42 论文（8 条）**

| ID | 位置 | 现状 | 应为 | 证据 |
|---|---|---|---|---|
| **S13** | `:65`（及摘要 `:18`） | 「改用正确通带**并计入 QE**后…降至 **0.04505 mag**」 | 该数是"仅改通带、QE 仍未配"臂；"并计入 QE"臂为 **0.05104** | `RESOLUTION_m42_curve_resolve.md:107` V1_band_fix「只把通带改成 Baader R（**仍无 QE**）」= 0.04505；`:108` V2_band_qe_fix「再计入 KAF-16803 QE」= **0.05104**；`:124`「QE 未配置使散度略升（0.04505 → 0.05104，+13%）」。结论（回上界内）不受影响，但归因反了，且与论文同句把 `Q≡1` 列为根因自相矛盾 |
| **S14** | `:17`、`:62`、`:65` | 「EXP-04 band 上界 / **已发表量级上界** 0.057457 mag」 | 应为「三帧仿真 σ_obs 的最大者，非定义的上界」 | `RESOLUTION_m42_curve_resolve.md:138-141` 已明文判「**不精确的表述**」，订正未回改到 m42 论文与 `c1_photometry.py:21-23`；`实验/photometric-magnitude/REPORT_paper.md:145` 原文为三帧 σ_obs 列举 |
| **S15** | `:96`（及 `:90-91`、`:166`） | T2「0.97432（CI [0.19037, 1.61583]）判绿」未降级；实际门限 ±0.15 且无 CI 半宽条款，未披露 | 标 `degenerate-control` 或披露真实门限与降级语义 | CI 半宽 0.713 = 论文 `:90` 自设 0.05 阈值的 14.3 倍，正落其自定义的恒真门；`c2_absolute_snr.json:559` 判据 desc = 「= 1 **+- 0.15**」，无半宽条款，level=`data` |
| S16 | `:163` | 「三条判红（C1-G1、C2-G1/C2-G3、C2-G2-t3）」只列 4 个 ID，与摘要「**6 条判红**」对不上；漏列 `C2-G3c-weights-exploit-noise-variation` 与 `C3-G1-seam-p3-export` | 补全 6 条 | `c2_absolute_snr.json:593-600`（ok:false）、`c3_seam_additive.json:1225-1232`（ok:false） |
| S17 | `:59` | `MAD(r)/0.6745` | 全精度 `0.6744897501960817` | `PHOTOMETRY.md:287`（4 位写法「只允许出现在「≈」语境并标注该偏差」）；且与本单元代码 `c1_photometry.py:16/:239/:241/:397/:399` 的全精度写法**矛盾** |
| S18 | `:23`、`:120`、`:121` | 「5σ = **1.496e-06**」 | 5 × 2.981297e-07 = **1.491e-06** | `c3_seam_additive.json:30` σ=2.981297309838216e-07、`:1249`「阈值 1.491e-06」。判红（4.501e-06 ≫）与判绿（5.916e-07 ≪）结论均不受影响 |
| S19 | `实验/m42-realdata/results/c1_photometry.json:1727`、`:1729` | source 指 `module_adapters.cpp:4839-4845` 与「同文件 `:4826-4838`：组间一致性不是门禁」 | 改为 `module_adapters.cpp:6187-6219` | 逐行读 `:4826-4845` 全为 WCS 产物写出/manifest 字段（`wcs_source`/`n_samples`/`max_roundtrip_px`/`wcs_artifact`），与散度/门禁无关；真实位置 `:6187-6219`，`docs/engineering/UNRESOLVED_REGISTER.md:3629` 同样记作 `:6187-6219`。`photscale_spread_gate = "none (owner ruling 9.49: frame-independent)"` 在 `:6487`/`:6545`（与论文 `:72` 一致 ✓） |
| S20 | `:176-184`（§4 诚实边界） | 未收编两处已知失效域：覆盖缺口 `covered_but_nonfinite_px=466515`（2.78%）；SCI-B variance 平面缺陷的适用边界 | 补入失效域清单 | `c3_seam_additive.json:1201-1206`；论文仅在 `:124-125` 顺带提及，未进诚实边界 |

### 3.3 建议（3 条）

| ID | 位置 | 说明 |
|---|---|---|
| G1 | m42 `:27-29` | 把构造性恒等（`nused + nrej == candidates`，`c4_leaf_allocation.json:557`）作为「14/14 全绿」摘要卖点。`:170` 已披露「是零容差恒等」，建议把披露前移到摘要，或摘要改述为「几何闭合为恒等式、判别力来自 :144-146 的三方交叉核对」 |
| G2 | m42 `:128-129` | 「算术平均 **1.159**、取最大 **9.194**、取最小 **1.607**」易被读成同一 16 tile 分布的均值/极值（那样 1.159 < 最小值 1.607 不成立）。经核，三者是**三条独立负例臂**（`c3_seam_additive.json:1270-1295`），数字无误，仅措辞需消歧。**此为我初判的疑似内部矛盾，核证后推翻** |
| G3 | m42 `:37`、`:55` | 顶点文件名 `docs/ASTROCS_DESIGN.md` 不存在（现名 `docs/ACSD_DESIGN.md`）；同一失效名另见 `c1_photometry.json:1763`/`:1799`、`c3_seam_additive.json:1301`。内容可核但锚失效 |

---

## 4 已知候选的核证结论

### 候选 1：「机器可读结果」与「可复现脚本…完整证据链」

**结论：真（失实陈述，可一条命令核伪）。**

逐字原文，`实验/cone-search-constants/REPORT_paper.md:5`：

> `> **机器可读结果**：`results/phase1_phase2.json`、`results/final_constants.json`（固定种子 20260926）`

`:7`：

> `> **报告性质**：standalone 实验报告。含可复现脚本、三类实验数据（HST 真值 + 纯解析合成 + testdata）、Oracle 核验、负例注入与完整证据链。`

复现命令与输出：

```
$ ls -la 实验/cone-search-constants/
total 28
drwxr-xr-x 2 dsh dsh 4096  9月26日 18:11 .
drwxr-xr-x 11 dsh dsh 4096 10月 2日 01:20 ..
-rw-r--r-- 1 dsh dsh 18697  9月27日 20:19 REPORT_paper.md

$ find 实验/cone-search-constants/ -mindepth 1
实验/cone-search-constants/REPORT_paper.md

$ git log --all --diff-filter=D --name-only -- 实验/cone-search-constants/
（空 —— 从未有过 code/ 或 results/，也从未被删除）
```

⇒ 两句陈述均为**可核伪的失实陈述**：无机器可读结果、无可复现脚本、无完整证据链。§11「附录：JSON 结果样例」（`:409-485`）是**内联在论文里的文本**，不是产物；`:301` 更是宣称本报告由 `generate_report.py` 生成，而该脚本在任何地方都不存在。这构成 B1 + B2 两条阻断。

### 候选 2：`:4` 引用的正本节在 `docs/science/PHOTOMETRY.md` 中不存在

**结论：真。**

逐字原文，`实验/cone-search-constants/REPORT_paper.md:4`：

> `> **正本科学口径**：`docs/science/PHOTOMETRY.md`（SCI-PHOT-001，FROZEN；§2「锥形搜索与常数定义」）`

复现命令与输出：

```
$ grep -n "^#\{1,4\} " docs/science/PHOTOMETRY.md
 18:## 2 符号表          ← 论文称此处为「锥形搜索与常数定义」
 36:## 2a 参考通量 `F_syn` 的合成口径（定义 · 量纲 · 适用域 · 证据）
 40:### 2a.1 定义式
 ...（§2.1 不存在）...

$ grep -n "锥形\|cone_radius\|cone_search" docs/ | head
docs/detail/infrastructure/22_gaia_xpsd_client.md:78:| `op` | 操作词表：`cone_search` / ...
docs/detail/infrastructure/22_gaia_xpsd_client.md:81:| `ra` / `dec` / `radius_deg` | 锥形查询天区（deg） |
docs/engineering/PUBLIC_API.md:459:| pc_calibrate_simple_with_gaia | :153-183 | DLL 内锥形搜索+积分（直通封装） |

$ grep -rn "cone_radius_deg\|polar_theta_deg\|polar_phi_deg\|constant_offset_mag" docs/ lib/ eng/ 实验/
lib/algorithms/platesolve/tools/diag_projection_plot.py:156:    cone_radius_deg = fov_radius_deg * 1.05
实验/cone-search-constants/REPORT_paper.md:30,420-469:（仅论文自身）
```

`docs/science/PHOTOMETRY.md` §2 是「**符号表**」（`:18`），§2.1 不存在，全文无「锥形搜索与常数定义」。更根本地：论文所标准化的一整套对象在正本与代码中**均不存在**——`polar_theta_deg`/`polar_phi_deg`/`constant_offset_mag` 全仓零命中；`cone_radius_deg` 唯一代码命中是 platesolve 一个诊断工具里的目录查询半径 `fov_radius_deg*1.05`（`lib/algorithms/platesolve/tools/diag_projection_plot.py:156`），与"待标定的物理常数"无关。`docs/` 中的「锥形搜索」统一指 `gaia_client_cone_search` 星表查询，参数只有 `ra`/`dec`/`radius_deg`，**根本没有极轴方向 θ_p/φ_p 这个自由度**。

⇒ 分类 **(a)：论文的引用确实指向不存在的节**，且不存在的是一个完整概念而非仅节号偏移。构成 B3 阻断；同源的 `:353`（A-02 指向 `PHOTOMETRY.md §2.1`）一并归入 B3。

### 明确推翻的候选

**G2**（m42 `:128-129` 的 1.159 / 9.194 / 1.607）：我初判为"均值小于最小值"的内部矛盾。核证后**推翻**——三者分属三条独立负例臂（`c3_seam_additive.json:1270-1277` C3-N2-arithmetic_mean-red = 1.1590129887121463、`:1279-1286` C3-N2-maximum-red = 9.19404434664433、`:1288-1295` C3-N2-minimum-red = 1.607279143144859），不是同一分布的统计量，数字无误。降为措辞建议。

**cone 双边界公式形态**（`:123-124`）：我初判可能与正本不一致。核证后**推翻**——与 `PHOTOMETRY.md:383` 及 `RESOLUTION_m42_curve_resolve.md:135-137` 的「帧自算」写法完全一致，cone 在此无错。问题只在报告义务（S7）与适用面（S8/见 ④第 21 条），不在公式。

---

## 5 疑问与不确定项

1. **cone 论文的数值是否来自别处**。`:458` 的 `initial_offset_arcsec: 1.62` 与真实读数 `实验/photometric-magnitude/results/step3_forward_vs_photflam.json:70/:86/:102`（`shift_arcsec` = 1.6218 / 1.6290 / 1.6283）吻合，`PHOTOMETRY.md:396` 也把 1.6″ WCS 平移的读数正本指向该文件。这提示 cone 的数字**可能是从 photometric-magnitude 单元移植后重新署名到不存在的脚本上**，但**无法证明**（`final_residual_px: 0.87`、`iterations: 2` 我未在源文件中定位到）。**未决**，需作者说明或前台调取该单元执行记录。
2. **cone 的 `analytic.json`（`:290`）是否曾存在过**。脚本清单暗示有第三类数据产物，§3.1 描述了"纯解析合成数据"生成方式，但无任何产物、无读数。**未决**。
3. **cone §8.1 三条文献定位**（Gaia DR3 §8.1.1 Fig. 27 / 表 B.3；STScI ETC Manual §4.2、附录 D 的"HLSP drz WCS 精度"与"系统偏移统计"）**待联网核验**。ETC Manual 是否含 HLSP drz WCS 精度章节存疑（HLSP 产品的 WCS 精度通常在数据发布说明 / DrizzlePac 文档），但本分片不联网，不下结论。
4. **cone 究竟是不是一个应当存在的实验单元**。`实验/README.md:21` 把它列为「锥形搜索四项常数的论文式精读报告单元」，但 `docs/` 内零引用、无 README、无代码、无产物、且其核心对象在正本与代码中不存在。三种可能：(a) 早期实验产出物在 `run/FINAL-07-e2e/bisect/` 归档过程中丢失；(b) 单元应整体退役（与 HEAD `ed33f57f`「reports/ 目录退役收尾」的处置方向一致）；(c) 需先补正本再重做。**这是处置决策，超出审稿权限，须前台/负责人裁决**。
5. **m42 `:120` 的「（x=2048 与 y=2560 超界）」未能独立复核**。我读到的 `c3_seam_additive.json:1240` 是**行块一致性分类**清单（不一致边界及各自 `|excess|`），不是 5σ 超界清单；这两类边界集合不是同一集合（该 note 里 x=2048 的 `|excess| = 7.6e-07` 本身并未超 1.49e-06）。逐边界 5σ 超界明细在我未读到的 JSON 区段，**未判**。
6. **m42 §2.3(b) 的 `seam_ratio = 1.00961`（门 1.5）**未在本次核验范围内定位到源读数，**未判**。
7. **正本自身存在死指针（超出本分片，但影响 ⑤ 的比对基准）**：`docs/detail/algorithms_phase1/06_photometry.md` **不存在**（实际 `docs/detail/` 下为 `infrastructure/`、`registry/`、`*.md`），而 `PHOTOMETRY.md:211`、`:332`、`:334`、`:359`、`:405` 五处引用它；`PHOTOMETRY.md:332` 引用的 `docs/research/PHOTOMETRY_RESEARCH_PACK.md` 亦不在 `docs/` 下。**建议转交负责 docs/ 的分片**，本分片不越界处置。
8. **`RESOLUTION_m42_curve_resolve.md:152-155` 记录的正本/实现口径分歧**（称 `PHOTOMETRY.md` 与 `spectrum_integrator.h:16` 写的是 `F_syn = ∫…×10^(−0.4·G_Gaia)` 旧式）——本人核对 HEAD 版 `PHOTOMETRY.md:43`/`:54` 已是**不含** `10^(−0.4·G)` 的正确式，该分歧**似已在上游订正**，但 `spectrum_integrator.h:16` 未核，**待确认**。
9. **cone `:229` 的 `corr(R_c, θ_p) ≈ 0.12`** 无原始产物可核，"唯一识别"（`:242`）因此缺条件数/后验支撑。若 B3 按方向 (a) 处置（补正本+重做），这一项应作为重做验收项。

---

## 6 处置建议（按优先级）

1. **cone 论文整体退回**。B1–B5 五条阻断中任意一条即足以退回；五条叠加意味着该单元目前**没有任何可核证据**，其 §6 结论（`:254`）与 §5 成立条件（`:256-260`）不具备证据资格。建议先做第 5 节疑问 4 的三选一裁决，再决定补做/退役。
2. 若决定**保留并补做**：须先在 `docs/science/` 建立四项常数的正本节（补 §2 的符号表条目与判据），再产出 `results/` 与 `code/`，并按 `PHOTOMETRY.md:385-388` 补 `n`/`gate_scope`。
3. **m42 论文**：S13、S14、S15 三条建议在同一轮修掉（均为口径/归因，硬错但不改结论）；S16–S20 可同批处理。m42 论文的整体证据质量在两篇中明显更高——判据总数、level 分组、每个 headline 数字均与 JSON 逐位吻合，且主动报告了 UNDECIDABLE、负例臂、对自己不利的偏差方向与上游单元缺读数，建议作为正面样板。
4. 疑问 7（`06_photometry.md` 死指针）转交 docs/ 分片。
