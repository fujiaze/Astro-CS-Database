# 审稿 · G08-05 对抗审稿第 1 遍 · 片 `EXP-m42-realdata-001`

**审查片号**：`EXP-m42-realdata-001`
**层**：`实验/m42-realdata`
**基线**：`HEAD = f9650dd0`（`git log --oneline -1` 实测）
**片清单来源**：`run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml:2494-2512`
**划片依据**：SRS-1 层内 LPT 均衡装箱（n=ceil(层行数/11000)，严格不跨层）
**交付件**：`run/GOVERN-08/审核包-R2/审稿-P1-EXP-m42-realdata-001.md`（唯一）
**审查员动作**：只读。未 add / commit / checkout / reset / stash / `git rm --cached`；未编译、未跑 ctest/pytest/构建/实验脚本；未改任何仓内文件。唯一执行的是 `wc -l` / `git ls-files` / `git log` / `grep` / `sed -n` / `sha256sum -c` / `python3 -c`（纯解析与算术，不读产品、不跑实验脚本）。

---

## 1. 读完了吗

| 项 | 口径 | 值 |
|---|---|---|
| 成员份数（片清单声明） | 人工产物份数 | **11** |
| 实际读完份数 | 本人逐字 `read` | **11** |
| 成员总行数（片清单 `实际行数`） | 行 | **2814** |
| 实测成员总行数（`wc -l`） | 行 | **2814**（与清单一致，差 0） |
| 实际读了多少行 | 行 | **2814** |
| **覆盖率** | 行 | **100.0 %** |

**未读完的部分：无。** 11 份成员全部从第 1 行读到文件末行，无跳读、无片段读。

### 为取证额外读取的非成员文件（不计入覆盖率，但计入证据）

这些是**判据的唯一证据面**，被片清单划为「生成物」而排除在所有片之外（见 S-3），不读就无法核对任何红绿结论：

- `实验/m42-realdata/results/c1_photometry.json`、`c2_absolute_snr.json`、`c3_seam_additive.json`、`c4_leaf_allocation.json`
- `实验/m42-realdata/results/SNAPSHOT.sha256`
- 交叉核验用：`docs/ACSD_DESIGN.md`、`docs/science/algorithms/DRIZZLE_GEOMETRY.md`、`实验/healpix-polar/docs/EXP-07-POLAR.md`、`lib/infrastructure/scheduler/src/module_adapters.cpp`（定点 `sed -n`）、`.gitignore`、`run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml`

### 计数口径声明（任务第 9 条）

本片出现三套数，全部显式标注，**不可混用**：

| 口径 | 值 | 说明 |
|---|---|---|
| **门实例（运行时）** | **49** | c1=14 / c2=10 / c3=11 / c4=14。含 `%s` 循环按 block / arm 展开后的实例数 |
| **静态 `g.add(` 调用点** | **39** | c1=10 / c2=9 / c3=9 / c4=11。实测 `grep -c '^\s*g\.add\('` |
| **去重门** | **37**（子代理口径，另给出 39 的变体） | 同一判定逻辑被复制到多处算 1 个；口径差异来自 `C4-G5a/b/c` 是否按三个奇点分别计 |
| **整改分母（现行代码）** | **46 live / 3 degenerate** | c2 三条已标 `degenerate=True`（`c2:398/412/444`）；c1/c3/c4 各 0 条 |
| **整改分母（仓内归档）** | **49 / 0 degenerate** | 归档 `results/*.json` **无** `degenerate` 键 ⇒ 早于现行代码（见 B3） |

**本片文档与代码使用的都是「门实例」口径（49），而现行代码跑出来是 46，两者不一致。**

---

## 2. 本片判定

# 阻断

**理由**：本片同时存在「绿灯掩护同一产物上的红灯」「复现命令恒返回成功且其完整性锚会被复现动作自己刷新」「判据代码已改而归档未重跑」「阈值放宽把红灯改成绿灯而依据是不存在的引文」「论文级报告头条结论无证据面」五类失效。任一条单独即足以阻断本单元发布；五条叠加，本片的可判定结论整体不可采信。

### 最重的 3 条

**① 选择后判据：绿灯掩护红灯**（`c3_seam_additive.py:302-305`）
`C3-G1b` 先按 `consistent`（要求 4/4 行块同号，`c3:133`）筛出子集，再对子集取 `max|median|`。归档实测：**被判集只剩 14 条边界中的 2 条（列向 x=3584、行向 y=3584），且这 2 条是全图 excess 最小的两条**（1.55e-07 / 5.92e-07）；被剔除的 `y=2048` 块中位 **−2.7810e-06，是 5σ 阈值（1.4906e-06）的 1.87 倍**，且 3/4 块同号——差一票就被归入「天体结构」逐出判集。同一张图在 `C3-G1` 上是**红**（4.5013e-06 vs 1.4906e-06）。此外 `max(..., default=0.0)` 使「无一致边界」直接判绿（fail-open）。据此写出的结论「M42 导出图上没有可检出的加性天光接缝」写在 `README.md:161-162`、`CRITERIA.md:191-197`、`REPORT_paper.md:140-141`。

**② 自愈判据 + 退出码空门 + 输入不在仓**（`run_all.sh:14/22-23`；`m42_common.py:34-41`）
`README.md:203` 与 `REPORT_paper.md:212` 让读者跑 `sha256sum -c results/SNAPSHOT.sha256` 当「完整性锚」；而该归档由 `run_all.sh:22-23` 在四个步骤之后**无条件重写**，且 `-name '*.json'` 把 `results/c*.json` **自身**纳入指纹。实测当前 **11/15 不匹配**（含 c1/c2/c3/c4 四个脚本、`m42_common.py`、`CRITERIA.md`、`README.md`、`REPORT_paper.md`、c1/c3/c4 三个结果 JSON）。在缺输入的机器上跑一次：四脚本全崩 → JSON 不落盘（JSON 只在 `main()` 末尾写）→ `run_all.sh:22` 照常用陈旧 JSON 重算指纹 → 再校验**全绿**。这就是「第一跑红、第二跑转绿而缺陷仍在」。叠加：四个 `main()` 一律 `return 0`（`c1:426`/`c2:452`/`c3:379`/`c4:323`），`run_all.sh:17` 只看退出码 ⇒ **判红在退出码上完全不可见**。且唯一输入根 `run/RELEASE-05/vis/out` 目录不存在、被 `.gitignore:17` 排除、`git ls-files run/RELEASE-05` 为 0 份 ⇒ 本单元在当前仓状态下**不可复跑**。

**③ 阈值放宽把红灯改成绿灯，依据是不存在的引文**（`c4_leaf_allocation.py:10-11 / 150-166`）
归档实测 `dev = 8.754e-09`：**原拟门限 1e-9 会判红，现用 `GATE = 1e-6`（`c4:155`）判绿**。而 docstring 称依据「`docs/science/algorithms/DRIZZLE_GEOMETRY.md:236`（FP64 通量闭合 <1e-6）、冻结容差出处同文件 **:235（"不得放宽"）**」。实测该文件 `:235-236` 原文是「累加结构: 归约池中至多 `kScratchPoolCap` 个 scratch map（P22 前为 per-thread map）+ per-thread 计数器；行级顶点缓存 `thread_local`（`shared_vertices = (config.pixfrac == 1.0)`）」，**与容差、FP64/FP32、闭合门毫无关系**；引号里的「**不得放宽**」在该文件不存在（`docs/` 全仓仅 1 命中，在 `docs/engineering/HIPS_STORAGE_FORM_CONTRACT.md:107`，讲的是 `IO_003 §4`）。真实冻结容差在同文件 `:318-323`，其中**没有任何 1e-9**，且明写：**「求和型判据对逐叶错注入无判别力，只作辅判据，不作验收判据」**——`C4-G4` 恰是求和型（跨 order 求 Σ），`C4-N5` 恰是逐阶错注入。

---

## 3. 逐文件清单

| # | 成员文件 | 读了多少 | 读了什么 | 看到什么（带 `文件:行`） | 判定 |
|---|---|---|---|---|---|
| 1 | `code/c2_absolute_snr.py` | 430/430 | SCI-B 全部 4 个判据 + 2 组 MC 控制 + 缺陷登记块 | `c2:377` 分箱内取 `np.median(W)` 全数组中位数 ⇒ 24 箱 `wbv` 恒为常数；`c2:148-150` `weight_efficiency` 对 w **0 次齐次** ⇒ `E(常数w) ≡ E(全1)`；已正确标 `degenerate=True`（`:398/412/444`）。但 `c2:25-26` 声明「p 的自助 CI 与 1 相容」，实执行的 `c2:309-310` 是两参数斜率 `±0.05`，`fit["p"]`/`ci`（`:294-295`）算了不入门；`EXP06_P_BAND`（`:46`）从不参与判决。归档 `pooled_fit.inv_g = −3.639e8`（**负增益，物理不可能**）、`p = 0.2`（网格下界）、`x_span = 0.362 dex`。`c2:23` 仍写「判据（全部可红）」而三条已降级。`c2:376` 自陈「本仓现成产物 run/RELEASE-05/vis/out/ 不在，无法实测复跑」 | 须修 |
| 2 | `code/c1_photometry.py` | 430/430 | SCI-A 全部 14 个门实例 | `c1:356` 与 `c1:416` 的 `ok` 参数**硬编码字面量 `True`**（恒绿）；`c1:403-407` `C1-G3b` 是 `median(r)` 与 `median(r+0.6)` 的代数恒等（实测差 1.11e-16），`r` 来自 `:396` 的 `rng3.normal`，从不触 `star_matcher.cpp`；三条均**未标** `degenerate=True`（全文件 `degenerate=True` 出现 0 次）。`c1:24-25` 声明 C1-G2 须「比值 ≥ 1.5 且约等于 log10(kmax/kmin)」——该判据**无任何门实现**，被 `:337` 的置换-k 对照顶掉，原命题降级为 `:343` 硬编码 `"UNDECIDABLE"`。`c1:285-292` `value` 报 `np.median(smag)` 而 `ok` 判 `np.all(...)`（报的不是判的）。`c1:229-246` MC 正负控制的 mag→dex 换算多出一个 `1/0.6745` 因子（见 R2）。`c1:12-14` 自述 `P1_PHOT_MAX_SPREAD_DEX`「只是 warning 参考、不是门禁」，`:45/:361/:366` 却拿它当门限 | 须修 |
| 3 | `code/c3_seam_additive.py` | 383/383 | SCI-C 全部 11 个门实例 | `c3:297-305` 选择后判据（见 §2-①）；`c3:362-367` `C3-N3` 只对 `C3-G3` 的同一读数做 `ratio − 1` 算术，**未对任何数组执行减法**，note `:367` 自陈；`c3:315-316` 注入点选「基线 |excess| 最小」而不看 `consistent` ⇒ 检测限 A\* 标定在一条被判为天体结构的边界上。`c3:229-230` 注释「天光保留」挂错位置且 `tab` 零使用；`c3:191-192,210` `nvalid`/`meds` 零使用；`c3:312-313` 空 for 循环。`:11` 引 `sci_c_common.py:356-411`，`:276/:289` 却引 `:377-411`，`:306` 引 `:356-374` —— **同一对象三个行号区间** | 阻断 |
| 4 | `code/c4_leaf_allocation.py` | 327/327 | 创新点四全部 14 个门实例 | `:30` docstring 写 `1e-9`，`:155` 代码是 `1e-6`（理由在 `:150-153` 但 docstring 未同步）；`:10-11` 的引文是伪引（见 §2-③）；`:33-34` docstring 列的负例（「把 candidates 替换为 geom_n、把 mask 取反」）**与实现 `:307-311` 不符**（实际是 nused+1 / cand//2 / cand→0 / nrej→0）；`:177-180` `C4-N5` 是主门读数的代数镜像（`tot5` 与 `ref` 出自同一累加循环）；`:244-250` `C4-G3b` 第二合取项是 `C4-G1` 零容差逐叶恒等式的**精确求和**（整数求和保恒 ⇒ 逻辑蕴含）；`:293-296` 门限 1.0° vs docstring `:32` 声明 36°；`:274` 的「到 z=0 线」算好但**无门**；`:101` 只查 `ctrl[:6000]`，而 `m42_common.py:12-13` 声称 33472 个控制点。死代码：`CORNERS14`(`:51-56`)、`bin_memmap`(`:69`)、`candidates_plane`(`:73`)、`n_by_ipix/off_by_ipix`(`:184-185`)、`ras/tiles_ra`(`:254,256`) | 阻断 |
| 5 | `docs/CRITERIA.md` | 314/314 | 判据台账全文 | `:5` 称「`code/run_all.sh` 可一键复跑」——**输入不在仓，不可复跑**。`:18` 总表「C2 6/10，判红项含 `C2-G3`、`C2-G3c`」与同文件 `:163`「已标 `degenerate` 并移出门计数；本判红**证据不足**」**自相矛盾**（现行代码应为 5/7、判红 `C2-G1` + `C2-G2-t3`）。`:308` 称「未放宽任何阈值；两处阈值调整（C4-G4 1e-9 → 1e-6、C1-G3b 0 → 1e-12）都附…**仍可红的负例**」——`C4-G4` 的负例是代数镜像（不成立）、`C1-G3b` **根本没有任何负例**。`:186` 把阈值写成 `1.496e-06`（= `mu+5σ`），`:199` 又写 `1.491e-06`（= `5σ`，门实际用的那个）。`:31/:34` 称 `0.057457` 是「band 上界」，而仓内 `run/AUTONOMOUS-01/self-resolved.md:887` 已指出它是**帧 B 的 σ_obs 观测值**、不存在可搬运的常数上界 | 阻断 |
| 6 | `code/m42_common.py` | 251/251 | 公共库全文 | `:11-13` 声称由 `p2_samples.json` 的 **33472** 个控制点做实测校验，但 `c4:101` 只查 `[:6000]`。`:239-251` `Gates.summary()` 的 degenerate 排除机制是本片唯一做对的地方，但 `degenerate` 由**脚本作者手工传**，c1/c3/c4 一条都没传。`:48` `A_CELL`、`:45` `HP_RES_ARCSEC`、`:44` `NSIDE_LEAF` 均为硬编码字面量 | 须修 |
| 7 | `README.md` | 248/248 | 八要素全文 | `:15` 把 H3 的判据直接写成 `C3-G1b`——**判红的 `C3-G1` 被后造的分类器门顶替**。`:66` 「49 条判据，43 绿 / 6 红」对齐陈旧归档。`:105-109`/`:157-158` 仍以「与**常数权重臂完全相同**（E 比 = 1.000000）」作**可判定结论 2**，而 `CRITERIA.md:154-168` 已**明确撤回**该推理。`:183` 「真实 E 可能比 1.1432 小，但**因为产品权重在信号维上是常数**」——被撤回的不可观测事实。`:174` 引 EXP-07「接缝地板 **1.4e-16 sr**」，而 `EXP-07-POLAR.md:35` 明写该值「已被同口径复测**否定**」、`:525` 现值是 `1.4e-10 sr`。`:193-204` 的「一键复现」在退出码语义上是空门 | 阻断 |
| 8 | `REPORT_paper.md` | 215/215 | 论文体报告全文 | `:18`/`:71` 称「**根因已定案**…修后同批 49 帧中位 **0.04505 mag** 回上界内」——本片四个归档中**无此数**（`c1_photometry.json` 仍是 0.44746 判红），全仓唯一语义命中在被 `.gitignore` 排除的 `run/PHOTOCURVE-ORCH-01/RECEIPT.md:177,182`，其中明写「**未验证**」「**本轮未复算**」；`run/FINAL-07-e2e/.../AUD-201-测光核验.md:354` 更直书该证据文件**已丢失**、「**需复测**」。`:183-186` §3 仍以「权重效率等价于不用权重」作为三条判红「**三者互相印证**」的支点，与同文件 `:129`「撤回，证据不足」对撞。`:15` 「6 条判红」同陈旧口径 | 阻断 |
| 9 | `code/seam_criterion.py` | 105/105 | 判据本体副本全文 | `:18-19` 自称「**本文件不新定义任何判据**：函数体与 `sci_c_common.py` 逐字一致」，`:6-8` 也只列了 `_step_at`/`seam_steps`/`BOUNDARIES` 三个被复制对象；但 `:76-99` 的 `steps_on_profile` 与 `:102-105` 的 `seam_steps_axis` 是**本文件新写**、无权威对应物，而 `C3-SELFTEST`（`c3:59-60`）**只比对 `seam_steps`** ⇒ **喂给阻断级 fail-open 门 `C3-G1b` 的正是未被逐位锁定的 `steps_on_profile`**。`:72` `nvalid=int(2*halfwin)` 与实际有限计数无关。`:25` `BOUNDARIES` 零消费者 | 须修 |
| 10 | `docs/DATA-SOURCES.md` | 60/60 | 产品布局清单全文 | 全文**无一条 URL / 校验和 / 可下载副本** ⇒ 输入不可得时无任何离线复现路径。`:13`/`:55` 的 0.9670115215027271″/px 与 `c1:48`/`c3:41` 的 `A_PIXEL_SR = 2.197925819099591e-11` **数值一致**（本人实测相对偏差 `0.000e+00`，见 R5），但两处硬编码、互不引用、也无一致性门 | 须修 |
| 11 | `code/run_all.sh` | 25/25 | 一键复现全文 | `:14` `/usr/bin/time -v` 硬编码无存在性检查（缺失时 bash 报 127、**四个 python 步骤根本不执行**，循环照常打印 `exit=127`、SNAPSHOT 照常重写）；`:22-23` 无条件重写 SNAPSHOT 且纳入 `results/*.json` 自身（见 §2-②）；`:17` 只看退出码而四个 `main()` 恒 `return 0`；`:22` 的重定向目标目录不存在时该步失败但 `exit $rc`（`:25`）用的是**重算前的旧值** | 阻断 |

---

## 4. 发现清单

### 4.1 阻断（6 条）

---

**B-1 · 选择后判据：绿灯掩护同产物上的红灯**

- **位置**：`实验/m42-realdata/code/c3_seam_additive.py:297-305`（核心 `:302`）；判据本体 `c3:120`
- **现状**：`cons = [r for r in bc["per_boundary"] if r.get("consistent")]`（`c3:297`）先筛子集，再 `max_cons = max([abs(r["median"]) for r in cons + consT], default=0.0)`。`consistent` 要求 4/4 行块同号（`c3:133`）。阈值 `thr = 5·null_σ = 1.4906e-06`。
- **归档实测**（`results/c3_seam_additive.json`，`seam_block_consistency`）：

  | 轴 | 边界 | 块中位 excess | 同号块数 | 处置 |
  |---|---|---|---|---|
  | rows | **y=2048** | **−2.7810e-06（阈值的 1.87 倍）** | 3/4 | **剔除，不计** |
  | rows | y=1536 | −1.9726e-07（block3 = −6.862e-06） | 2/4 | 剔除 |
  | cols | x=1536 | +9.5075e-07 | 3/4 | 剔除 |
  | cols | x=2048 | −7.6081e-07（block2 = +7.189e-06） | 2/4 | 剔除 |
  | cols | x=3584 | +1.5549e-07 | 4/4 | **CONSISTENT** |
  | rows | y=3584 | +5.9157e-07 | 4/4 | **CONSISTENT** |

  判绿读数 **5.9157e-07** 只来自 **14 条中的 2 条**，且是全图 excess 最小的两条。同一张图在 `C3-G1`（`c3:286-291`）上是**红**：`max_abs_excess_dev = 4.5013e-06` vs 阈值 1.4906e-06。
- **应为**：判决对**全部 14 条边界**取 `max|excess − mu|`；块一致性降为**解释性标注**，不作筛选器；`default=0.0` 改 fail-closed。
- **证据**：`results/c3_seam_additive.json` → `seam_block_consistency.cols/rows.per_boundary[]`、`seam_p3.null`（σ=2.981297e-07）、`seam_p3.max_abs_excess_dev=4.50130667474381e-06`。复现命令见 §8-C2。
- **失效方向**：真实、较强但空间不均匀的接缝会降低跨行块同号一致性 → 被 desc(`c3:304`) 重分类为「天体结构」→ 掉出判集 → 门保持绿。**缺陷越严重越可能被筛掉。**
- **注**：`实验/TAUTOLOGY_REGISTER.md:239` 已登记该门为 fail-open，但只指出 `default=0.0`；本条**升级**为「筛选系统性地剔除了本单元自己认定的那个缺陷」。

---

**B-2 · 自愈型判据 + 退出码空门 + 输入不在仓**

- **位置**：`run_all.sh:14/17/22-23`；`m42_common.py:34-41`；`README.md:203`；`REPORT_paper.md:212`
- **现状**：① 复现动作 `run_all.sh:22-23` **无条件重写** `results/SNAPSHOT.sha256`，且 `find` 的 `-name '*.json'` 把 `results/c1..c4.json` **自身**纳入指纹；② 四个 `main()` 一律 `return 0`（`c1:426`/`c2:452`/`c3:379`/`c4:323`），`run_all.sh:17` 只看退出码 ⇒ **判红在退出码上不可见**；③ 唯一输入根 `run/RELEASE-05/vis/out` **不存在**，`.gitignore:17` 排除 `run/*`，`git ls-files run/RELEASE-05` = **0 份**。
- **证据**：`sha256sum -c 实验/m42-realdata/results/SNAPSHOT.sha256` → **11/15 不匹配**（失败：c1/c2/c3/c4 四个 `.py`、`m42_common.py`、`CRITERIA.md`、`README.md`、`REPORT_paper.md`、`results/c1.json`、`results/c3.json`、`results/c4.json`；成功：`run_all.sh`、`seam_criterion.py`、`DATA-SOURCES.md`、`results/c2.json`）。`ls run/RELEASE-05` → 只有 `evidence/`。
- **应为**：SNAPSHOT 拆成 `code+docs`（可自愈）与 `results/*.json`（**仅在四脚本 rc=0 时更新**）；`run_all.sh` 尾部加 `sha256sum -c` 自校验；四个 `main()` 改为 `return 1 if g.summary()["n_fail"] else 0`。
- **失效链（逐环节追踪）**：在缺输入机器上跑 `run_all.sh` → 四脚本在 `c1:54` 的 `M.read_json` 抛 `FileNotFoundError` 崩 → JSON 不落盘（`c1:423`/`c2:449`/`c3:376`/`c4:320` 只在 `main()` 末尾写）→ `run_all.sh:22` 照常用陈旧 JSON 重算指纹 → 再校验**全绿**。**第一跑红、第二跑转绿而缺陷仍在，在此精确成立。**
- **同一机制覆盖**：`timeout 3600` 超时、`/usr/bin/time` 缺失两种情形，只要 JSON 未被成功重写，锚都被刷新成「陈旧结果的合法指纹」。

---

**B-3 · 判据代码已改、归档未重跑（整改分母失效）**

- **位置**：`m42_common.py:236/246-250` vs `results/c{1,2,3,4}_*.json`；`CRITERIA.md:18`；`README.md:66`；`REPORT_paper.md:15/183`
- **现状**：现行 `Gates.add` 无条件写 `degenerate=bool(degenerate)`（`m42_common.py:236`），`Gates.summary` 无条件写 `n_degenerate`/`degenerate_ids`（`:249-250`）。**四个归档的 summary 键只有 `['n','n_pass','n_fail','rows']`，rows 一条都没有 `degenerate` 键**（本人实测）⇒ 归档全部早于现行代码。`c2` 归档仍把 `C2-G3c` 的 note 写成已撤回的「=1.0 表示权重在信号维上完全不变」，仍报 `n=10, n_pass=6, n_fail=4`。
- **应为**：按现行代码重跑并落盘。**重跑分母应为 `n=7, n_pass=5, n_fail=2, n_degenerate=3`**（c2）；本片合计 **46 live / 3 degenerate**。
- **连带**：三处汇总表仍按旧口径写「49 条判据，43 绿 / 6 红」/「C2 6/10，判红含 `C2-G3`、`C2-G3c`」，而 `CRITERIA.md:163` 同文件已写「已标 degenerate 并移出门计数；本判红证据不足」——**同文件内部矛盾**。
- **升级点**：`docs/science/algorithms/PHASE2_REJECTION.md:832` 把 `实验/m42-realdata/results/` 指定为**一级正本读数源**。按现状，任何人从该正本读到的是**本仓自己已撤回的判红**。

---

**B-4 · 阈值放宽把红灯改成绿灯，依据是伪引**

- **位置**：`c4_leaf_allocation.py:10-11`（引文）、`:150-155`（放宽）、`:161-166`（门）
- **现状**：归档 `dev = 8.753990145683033e-09`。`GATE=1e-6` ⇒ 绿；**原拟 1e-9 ⇒ 红**。docstring 称依据 `DRIZZLE_GEOMETRY.md:236` 与 `:235`（「不得放宽」）。
- **实测伪引**：`sed -n '233,237p' docs/science/algorithms/DRIZZLE_GEOMETRY.md` ⇒ `:235-236` 原文「累加结构: 归约池中至多 `kScratchPoolCap` 个 scratch map（P22 前为 per-thread map）+ per-thread 计数器；行级顶点缓存 `thread_local`（`shared_vertices = (config.pixfrac == 1.0)`）」——与容差/FP64/FP32/闭合门无关。`grep -rn "不得放宽" docs/` → **仅 1 命中，在 `docs/engineering/HIPS_STORAGE_FORM_CONTRACT.md:107`，讲的是 `IO_003 §4「发布清单必含 properties」`，不在 `DRIZZLE_GEOMETRY.md`**。真实冻结容差在同文件 `:318-323`，**其中没有任何 1e-9**。
- **正本的反向条款**（比伪引更致命）：`:322-323` 明写「**求和型判据对逐叶错注入无判别力，只作辅判据，不作验收判据**」。`C4-G4` 是求和型（`:146` 跨 order 累加 Σ），`C4-N5`（`:175`）是逐阶错注入 ⇒ **正本明文否定本单元用它当验收判据的用法**，而 `CRITERIA.md:271-272` 却写「并配负例…⇒ 判红（**证明阈值仍可红**）」。
- **应为**：撤回 `1e-9 → 1e-6` 放宽；或改引 `:318-320` 的真实容差并把主判据换成逐 leaf <1e-5（`:319`），同时把求和型降为辅判据。

---

**B-5 · 论文级报告的头条结论无证据面**

- **位置**：`REPORT_paper.md:18`（摘要）、`:71`（§2.1(a) 补记）
- **现状**：称「**根因已定案**为模型通带配置缺陷＋`Q≡1`…修后同批 49 帧中位 **0.04505 mag** 回上界内」，并据此把 `C1-G1` 的证据语义改为「通带身份缺陷的检出」、从 §3 三条判红中摘出。
- **证据**：本片四个归档中**无 0.04505**（`c1_photometry.json` 的 `sigma_obs` 仍是 0.44746 判红）。全仓 `grep -rn "0\.04505" --include=*.md --include=*.py` 的语义命中全在 `run/` 下（被 `.gitignore:17` 排除），其中：
  - `run/PHOTOCURVE-ORCH-01/RECEIPT.md:177`：「**未验证**的是『某份真实 stage1.json 指向转录版时端到端数值等于 0.04505 mag 量级』」；`:182`：「…差异来自前序任务的受控对照，**本轮未复算**」；
  - `run/FINAL-07-e2e/bisect/src/独立审计/证据/AUD-201-测光核验.md:354`：「证据文件 `run/SCI-PHOT-FORMULA-01/evidence/d1_zp_sigma_rederive.json` **已丢失**（AUD201-004）| **需复测**」；
  - `run/FINAL-07-e2e/.../论文1-回执.md:67`：「论文**一律不引**这些数字…0.42720→0.04505 mag…§6.5 整节改记『待复测』」。
- **应为**：删除该定案表述，或恢复 `run/SCI-PHOT-FORMULA-01/evidence/d1_zp_sigma_rederive.json` 并在 `results/` 落一份可复算证据后重写。**当前状态是：唯一声明此数未复算的证据不在版本控制内，而相反的强主张写在论文报告里。**

---

**B-6 · 全片引用不存在的文档，且被引句在替换目标里也不存在**

- **位置**（`docs/ASTROCS_DESIGN.md` 悬空引用，共 **12 处**）：
  - 代码 `source=` 字段：`c1:7`、`c1:340`（挂在活门 `C1-G2-k-response` 的证据出处上）、`c2:7`、`c3:7`、`c3:360`（挂在活门 `C3-G3-sky-retained` 上）、`c4:7`、`c4:164`（挂在活门 `C4-G4` 上）
  - 文档：`CRITERIA.md:28/103/220/241`、`README.md:3/214/221/223`、`REPORT_paper.md:43/61`
- **实测**：`test -f docs/ASTROCS_DESIGN.md` ⇒ **不存在**；`ls docs/` ⇒ `ACSD_DESIGN.md / detail / DOCUMENT_INDEX.yaml / engineering / GLOSSARY.md / README.md / science`。`AGENTS.md §2-3` 规定的最高设计是 `docs/ACSD_DESIGN.md`。
- **加重情节（映射后仍不成立）**：把 `ASTROCS_DESIGN` 映射到 `ACSD_DESIGN` 后，被引句在所标行**仍不存在**：

  | 引用 | 被引句 | `docs/ACSD_DESIGN.md` 该行实际内容 |
  |---|---|---|
  | `c1:7` / `CRITERIA.md:28` / `REPORT_paper.md:61`「`:120-128` k_photo 是线性乘性标度」 | k_photo 线性乘性标度 | `:120` = `### 2.2 P2 跨帧绝对信噪比`；`:126` = `frame_snr` / `sparse_snr_layer`；`:128` = 帧级信噪比对天光的单调性成立域 |
  | `c1:340` / `CRITERIA.md:241`「`:126` 构造闭合 / `:147-159`」 | — | `:126` = 信噪比承载对象；`:153` = dense/sparse_reconstruct/frame_reconstruct 三种重建方式 |
  | `c3:360`「`:143` 保留公共天光平面，多退少补」 | 保留公共天光平面 | `:143` = face 角点、极面折线、赤道带对角线与面缝是 chart 的分支奇点 |
  | `README.md:3`「§12.2」 | 三类数据腿 | `docs/ACSD_DESIGN.md:591` 确有 `### 12.2 三类实验数据`（**这一条是唯一能对上的**） |

- **附带（cpp 行号漂移）**：`c1:12` 称 `P1_PHOT_MAX_SPREAD_DEX` 在 `module_adapters.cpp:4377` ⇒ 实测 **:5536**（`constexpr double P1_PHOT_MAX_SPREAD_DEX = 0.02;`）；`c1:13`/`CRITERIA.md:71`/`README.md:216` 称的裁决 `:4826-4838` ⇒ `"photscale_spread_gate"` 字符串实际在 **:6487 / :6545 / :12788 / :12832**。
- **应为**：全片 `ASTROCS_DESIGN` → `ACSD_DESIGN` 并**逐条重新定位行号**；在判据 `source=` 字段上尤其不能只改文件名。

---

### 4.2 须修（16 条）

| 编号 | 位置 | 现状 | 应为 | 证据 |
|---|---|---|---|---|
| **M-1** | `c1:356`、`:416` | `ok` 参数**硬编码字面量 `True`**，恒绿 | 补 `degenerate=True`；或改为对 `estimator_disagreement`/真值实算 | 归档 c1 `n=14` 含这 3 条 |
| **M-2** | `c1:403-407` | `C1-G3b`：`median(\|r−med(r)\|)` 与 `median(\|(r+0.6)−med(r+0.6)\|)` 代数恒等，实测差 1.11e-16；`r` 来自 `:396` 合成高斯，**从不触 `star_matcher.cpp`**；未标 degenerate | 补 `degenerate=True`；或在真实 `sigma_residual` 的 r 样本上验证 | `CRITERIA.md:308` 却称其「附仍可红的负例」——**它没有负例** |
| **M-3** | `c3:362-367` | `C3-N3` 是 `C3-G3` 同一读数的算术减 1（`np.min(ratios) - 1.0 <= 0.3`），**代码未对任何数组执行减法**；归档 value = −0.475496 恰为 `min(ratios)=0.524504 − 1` | 真正构造一份全减背景的 corrected bin 并重跑 `C3-G3` 判据路径 | note `:367` 自陈「注入方式：ratio → ratio − 1」 |
| **M-4** | `c4:244-250` | `C4-G3b` 第二合取项 = `C4-G1` 零容差逐叶恒等式的**精确求和**（同一批数组，整数求和保恒）⇒ 逻辑蕴含，零信息量；第一合取项是 `C4-G2` 的严格弱化 | 删除或改判真正独立的量 | `c4:117-118` vs `c4:247-249` |
| **M-5** | `c4:177-180` | `C4-N5` 是主门读数的代数镜像（`tot5`/`ref` 出自同一累加循环 `:142-147` / `:171-174`），倍率 1e-5 写死为门限 1e-6 的 10 倍 ⇒ 必红 | 负例须触碰生产数据通路；正本 `:322-323` 已明文否定求和型判据配逐叶错注入 | `CRITERIA.md:271-272` 称「证明阈值仍可红」不成立 |
| **M-6** | `seam_criterion.py:76-99`、`:102-105` | 自称「不新定义任何判据」（`:18-19`），但 `steps_on_profile`/`seam_steps_axis` 是本文件新写、无权威对应物；`C3-SELFTEST`（`c3:59-60`）**只比对 `seam_steps`** ⇒ 喂给 B-1 那条门的代码从未被逐位锁定 | SELFTEST 扩展到这两个函数，或删除 | `seam_criterion.py:6-8` 的来源清单只列三项 |
| **M-7** | `README.md:105-109`、`:157-158`、`:183`；`REPORT_paper.md:183-186` | `CRITERIA.md:154-168` / `REPORT_paper.md:117-132` 已**撤回**「产品权重在信号维上是常数 ⇒ E 等于不用权重」，但 README 三处与 REPORT §3 仍以此作**可判定结论**与「三者互相印证」的支点 | 与撤回同步；`REPORT:183` 的「三者」须降为两者 | `CRITERIA.md:166-168`：「该『可观测的事实』不可观测，它是 `np.median(W)` 取全数组的代码产物」 |
| **M-8** | `c4:30` vs `:155`；`c4:33-34` vs `:307-311` | docstring 写 `1e-9`（代码 1e-6）；docstring 列的负例（含「把 mask 取反」「替换为 geom_n」）**与实现不符** | 同步 docstring | `c4:150-153` 已写明改门限理由，但 docstring 未改 |
| **M-9** | `c1:24-25` vs `c1:337-343` | docstring 声明 C1-G2 须「比值 ≥ 1.5 且约等于 `log10(kmax/kmin)`」——**无门实现**，被置换-k 对照顶掉；实测该比值 T2 = 0.0549/0.2631 = **0.21**（若实现必红） | 实现该判据，或撤回声明 | 归档 `cross_frame.t2.arms` |
| **M-10** | `c1:355` + `c1:356/343` | `C1-DIAG` 的 desc 称「估计器分歧 ≥ 待测效应 ⇒ 不可判定」，代码**从不评估该不等式**，ok 与结论都写死。归档实测分歧/效应 = **0.0090 / 0.1145 / 0.2452 / 0.3600，全部 < 1** ⇒ 按其自身规则属**可判定** | 让判决真的由该不等式驱动，或改写 desc | 归档 `cross_frame.{t2,t3}.estimator_disagreement` vs `arms` |
| **M-11** | `c2:25-26` vs `c2:294-295/302/309-310` | 声明「三参数 `Var = sigma0² + D^p/g`，要求 p 的自助 CI 与 1 相容」；实执行的 `fit["p"]`、`ci`（`:294-295`）**不进门**，门判的是 `c2:302` 的两参数斜率 ±0.05。`EXP06_P_BAND=(0.995,1.017)`（`:46`）**从不参与判决**，只被原样抄进输出 JSON（`:299`）。归档 `inv_g = −3.639e8`（负增益）、`p = 0.2`（网格下界）、`x_span = 0.362 dex` | 把门改到 `fit["p"]` + `ci`，或改 docstring；补 `inv_g > 0` 与 `x_span` 下限的非退化门 | `results/c2_absolute_snr.json → variance_model.pooled_fit / slope.x_span` |
| **M-12** | `c1:229-246` | MC 正/负控制的 mag→dex 换算多出一个 `1/0.6745` 因子（见 R2 实测）。后果：注入 0.026520 mag 的「正例」返回 **0.039**（应为 0.0265）；未接门的 `exp04_sim_top_mad` 臂注入 0.057457（正压门限）返回 **0.085077 ⇒ ok=false** | 补乘 `0.6744897501960817`，并复核该臂转绿后是否引入新问题 | 见 §8-C3 |
| **M-13** | `c1:426`/`c2:452`/`c3:379`/`c4:323` + `run_all.sh:17` | 四个 `main()` 恒 `return 0`，判红在退出码上不可见 | `return 1 if g.summary()["n_fail"] else 0` | 见 B-2 |
| **M-14** | `c1:12`；`c1:13`/`CRITERIA.md:71`/`README.md:216` | cpp 行号漂移：`P1_PHOT_MAX_SPREAD_DEX` 实际 `:5536`（称 :4377）；裁决字符串实际 `:6487/:6545/:12788/:12832`（称 :4826-4838） | 逐条重新定位 | `grep -n` 实测 |
| **M-15** | `README.md:174` | 引 EXP-07「接缝地板 **1.4e-16 sr**」。该值**已被同口径复测否定**：`EXP-07-POLAR.md:35`「原『绝对面积地板 ≈1.4e-16 sr』已被同口径复测否定」，`:830` 亦注「去掉『绝对地板 ≈1.4e-16 sr』」。现值是 `A_drop ≲ 1.4e-10 sr`（`:525`），与 `c4:57`/`DATA-SOURCES.md:55` 一致 | 改为 1.4e-10 sr，或删去该括注 | `grep -n "1\.4e-1[06]" 实验/healpix-polar/docs/EXP-07-POLAR.md` |
| **M-16** | `c4:31-32` vs `c4:283-284/274` | docstring 声明 C4-G5 为「距两极 ≥ 84 度、距接缝圆 ≥ 36 度、距 z=0 线 ≥ 4 度」；门用 **5.0** 与 **1.0**，第三项**无门**（`:274` 算好落盘但从不进 `g.add`）。归档实测 **82.898 / 34.708**——两项都**不满足**声明值，却在松门限下绿 | 门限改声明值或改声明；补「距 z=0 线」的门 | 归档 `defect_domain` 四个 `min_dist_*` |

---

### 4.3 建议（8 条）

| 编号 | 位置 | 内容 |
|---|---|---|
| **S-1** | `c3:229-230`、`c3:191-192/210`、`c3:312-313`、`c4:51-56/69/73/184-185/254/256`、`c2:45`、`seam_criterion.py:25/72` | 死代码：`tab`、`nvalid`、`meds`、空 for 循环、`CORNERS14`（注释引的 `experiment/healpix-polar/...` 路径还把 `实验/` 写成 `experiment/`）、`bin_memmap`、`candidates_plane`、`n_by_ipix`/`off_by_ipix`、`ras`/`tiles_ra`（同一表达式算两遍）、`TILE`、`BOUNDARIES`。`c4:49` 注释的 `experiment/` 路径写错，实体在 `实验/healpix-polar/results/t12_sweep.csv` |
| **S-2** | `c1:48`、`c3:41`、`c4:275/279` | 几何常数硬编码三处。**已核实数值本身一致**（`A_PIXEL_SR=2.197925819099591e-11` ⇔ 0.9670115215027271″/px，相对偏差 `0.000e+00`），但无一致性门：WCS 尺度若变，`C3-G3` 的 0.3 门限与 `C4-G5` 的 `scale_condition_met` 静默错标 |
| **S-3** | 片清单 `片清单-权威版.yaml:2494-2512` | **片划分盲区**：本片 11 份成员全是 code+md，`results/*.json` 与 `SNAPSHOT.sha256` 被判为「生成物」而**排除在全部 91 片之外**。这意味着本项目最典型的失效模式（判据↔归档一致性）**没有任何一片负责**。建议把判据单元的 `results/*.json` 强制并入其 code 所在片 |
| **S-4** | `run_all.sh:14` | `/usr/bin/time -v` 硬编码，无存在性检查无 fallback；缺失时 bash 报 127、四个 python 步骤根本不执行而循环照常打印 `exit=127`、SNAPSHOT 照常重写 |
| **S-5** | `c1:21`、`:42`；`CRITERIA.md:34`；`README.md:13/80` | 阈值口径：`0.057457` 被称为「EXP-04 三帧仿真 **band 上界**」，而仓内 `run/AUTONOMOUS-01/self-resolved.md:887` 已指出它是**帧 B 的 σ_obs 观测值**、判据带是「帧自算」的（三帧 0.056830/0.103015/0.159899）⇒ **不存在可搬运的常数上界**。`RESOLUTION_m42_curve_resolve.md:138-141` 同指 `c1_photometry.py:21-23` 表述不精确。**判红结论仍成立**（量级差 7.8 倍），但措辞须改 |
| **S-6** | `c3:81-92` vs `:106` | `null_threshold` 返回的 `threshold = mu + 5·sd`（**单边**）存进 `cal` 但判决从不使用（判决用 `5.0*cal["sigma"]`，**双边**）。两份「门限」并存，`CRITERIA.md:186` 引前者、`:199` 引后者 |
| **S-7** | `m42_common.py:44-50`、`c2:46` | 冻结常量（`NSIDE_LEAF`、`HP_RES_ARCSEC`、`A_CELL`、`K_MAD`、`EXP06_P_BAND`）全为字面量，无来源自校验。按 AGENTS.md §6「可由输入几何导出的改为现场计算」 |
| **S-8** | 依赖 | `numpy`（5 处）、`astropy.io.fits`（2 处）；仓内无 `requirements*.txt` / `pyproject.toml` / `environment*.yml`，**无版本钉**。`C3-SELFTEST` 断言 `worst == 0.0` 的逐位一致，对 numpy/astropy 版本敏感 |

---

## 5. 我主动构造的反例

### R-1 · 把 `C3-G1b` 改为对全部 14 条边界判决 —— **推翻**

- **构造**：不看门表达式，只用归档 `seam_block_consistency` 的 `median` 字段，对 `cols + rows` **全部 14 条**取 `max|median|`。
- **期望推翻**：若 C3-G1b 的绿只是筛选口径的产物，改口径后应由绿转红。
- **实测**：`max|median| = 2.7810e-06`（`y=2048`）> 阈值 `5·2.981297e-07 = 1.4906e-06`。**转红，倍数 1.87。**
- **结论**：**成立。** C3-G1b 的绿**完全**依赖「先筛后判」这一层筛选；被剔掉的恰是全图最大的台阶。这是最强反例。
- **口径说明**：本反例用**归档 JSON 的已落盘读数**做纯算术，未运行任何实验脚本、未读产品（产品已不在仓）。

### R-2 · `mc_positive_control` 的 mag→dex 单位换算 —— **推翻**

- **构造**：在 `/tmp` 复刻 `c1:233-245` 的循环（400 次 MC、n=157、seed 改用 11/12/13），与归档读数对账。
- **期望推翻**：若代码返回量纲正确，三臂应返回注入值本身。
- **实测**：

  | 臂 | 注入目标 (mag) | 复刻 MC 返回 | 比值 | 归档读数 | 归档比值 |
  |---|---|---|---|---|---|
  | `exp04_real_mad` | 0.026520 | 0.039477 | 1.4886 | 0.039036 | 1.4719 |
  | `exp04_sim_top_mad` | 0.057457 | 0.084680 | 1.4738 | 0.085077 | 1.4807 |
  | `4x_band` | 0.229828 | 0.338497 | 1.4728 | 0.337694 | 1.4693 |

  解析值 `1/0.6744897501960817 = 1.48260`。六个读数全部落在 1.47–1.49。
- **结论**：**成立。** `vals` 估的是 σ（= `mad_dex/0.6745`），再乘 2.5 ⇒ 系统性虚高 `1/0.6745`。后果之一是 `exp04_sim_top_mad` 臂（注入值恰好等于门限）**返回 0.085 ⇒ ok=false**，是一条被读成「估计器会把合格帧判红」的伪证据，实为换算缺陷的产物。
- **注意**：本条**不改变** `C1-G1c`/`C1-G1d` 的红绿结论（两种标定下结论相同），但使两条控制臂所声称的量纲陈述失真。

### R-3 · 完整性锚当前是否有效 —— **推翻**

- **构造**：直接跑 README 宣称的校验命令。
- **实测**：`sha256sum -c 实验/m42-realdata/results/SNAPSHOT.sha256` ⇒ **11/15 不匹配**，`sha256sum: 警告：11 个校验和不匹配`。
- **结论**：**成立。** README.md:203 与 REPORT_paper.md:212 宣称的「完整性锚」在当前提交上已经失效，且它失效本身正是「代码改了、归档没重跑」的直接证据。

### R-4 · `C3-G1b` 的 `default=0.0` fail-open 路径 —— **成立（静态）**

- **构造**：思考 `cons + consT` 为空的情形。
- **推论**：`max([], default=0.0) = 0.0`，`0.0 <= thr` 恒真 ⇒ **无任何一致边界时门判绿**（无证据 = 通过）。
- **现状**：归档中恰有 2 条一致边界，故本条**潜伏未发**——但 B-1 已经说明即使有 2 条，筛选本身就在掩盖缺陷。

### R-5 · `A_PIXEL_SR` 与 c4 硬编码像元尺度是否矛盾 —— **推翻（自我否决）**

- **构造**：我在初读时怀疑 `c1:48`/`c3:41` 的 `A_PIXEL_SR = 2.197925819099591e-11` 与 `c4:275`/`DATA-SOURCES.md:13` 的 `0.9670115215027271″/px` 不一致。
- **实测**：`sqrt(2.197925819099591e-11) × 206264.806247 = 0.9670115215027271`，相对偏差 **`0.000e+00`**。
- **结论**：**否决。** 两者逐位相同，不存在矛盾。我把该疑点降级为 S-2（硬编码无一致性门），不作为发现提交。**记录在此以示否决留痕。**

---

## 6. 盲复算

### 6.1 我遮住了什么

我在读完 11 份成员 + 4 份 `results/*.json` 归档、形成本交付件 §2–§4 的主要结论**之前，没有打开**任何一份 `审稿-RR*.md`、`审稿-R2-*.md`、`审稿-R3-*.md` 或 `实验/TAUTOLOGY_REGISTER.md`。§1–§4 的每一条都来自我自己对原文的通读与对外部被引条款的逐条开卷核对。

**局限（如实声明）**：因此我**不能**声称「本片独立发现了 TAUTOLOGY_REGISTER 未记的项」——我没有读它，无法判定。§7 记录子代理读到的登记表条目，并逐条标注它与我独立结论的关系。

### 6.2 独立取证后的比对

| # | 我的独立结论 | 与前三轮已记项的关系 | 判定 |
|---|---|---|---|
| 1 | B-1 C3-G1b 筛选剔除了最大台阶（`y=2048` = 1.87×阈值） | 子代理报告称 `TAUTOLOGY_REGISTER.md:239` 已登记该门 fail-open，但只指出 `default=0.0` | **相对已记项：偏严**（我把定性从 fail-open 升级为「绿灯掩护红灯」） |
| 2 | B-3 归档无 `degenerate` 键 | 子代理报告称登记表已记 C1 三条恒绿门未处置（`:235-236` 标 B 类未处置） | **一致**（同一现象，独立取证） |
| 3 | B-4 DRIZZLE_GEOMETRY 伪引 + 正本否定求和型判据 | 登记表未见此项（我未读，不能断言） | **我判为新** |
| 4 | B-5 0.04505 无证据面 | — | **我判为新** |
| 5 | B-6 `docs/ASTROCS_DESIGN.md` 不存在 + 映射后被引句仍不在该行 | 子代理独立报出同一问题（§5.2） | **一致** |
| 6 | B-2 SNAPSHOT 自愈 | 子代理独立报出并补出「四脚本恒 return 0」与 gitignore | **一致**（我先独立实测 11/15 失败） |
| 7 | M-11 C2-G1 门判两参数斜率、声明三参数不入门、`inv_g` 为负 | — | **我判为新** |
| 8 | M-12 MC 单位换算 1/0.6745 | 两个子代理均报（一个定性为「漏乘 0.6745」、一个定性为往返自证） | **一致**；我给的实测比值表为补强 |
| 9 | M-15 README:174 的 1.4e-16 sr 已被 EXP-07 否定 | — | **我判为新** |
| 10 | M-16 C4-G5 门限 5.0/1.0 vs 声明 84/36/4，实测 82.90/34.71 不满足声明 | 子代理独立报出同项 | **一致** |
| 11 | M-4 C4-G3b 是 C4-G1 的精确求和（逻辑蕴含） | 子代理独立报出同项 | **一致** |

### 6.3 总体判词

**判：一致，局部偏严，局部偏松。**

- **偏严**（我把已登记项升级）：C3-G1b 由「fail-open 记为可修」升级为「**在当前归档数据上正在给已红产物发绿光**」，因为归档实测显示筛选恰好剔除了 `y=2048`（1.87× 阈值）——这不是潜在的空过，而是**已发生的掩盖**。
- **偏严**（我把可修项升级为阻断）：C4-G4 的阈值放宽，我按「归档 `dev=8.75e-9` 在原门限下本应判红 + 正本明文否定求和型判据」判为阻断，而非「阈值设定问题」可修。
- **偏松**：B-5（0.04505 无证据面）我判为**阻断**（它替换了本单元唯一经 MC 正负例证明的判据结论，且顶掉了论文摘要的头条结果）；若按前三轮口径可能只记为「须补证据」。这一条我维持阻断立场，理由是**它改变了结论的证据语义而不只是缺一个附件**。

---

## 7. 子代理派发记录

派出 **4 个**子代理（要求 3–5 个），全部只读、无 git 写、无编译/测试/实验脚本。**本人逐条复核，否决 3 条、修正 2 条、采纳 9 条。**

| # | 子代理 | 职责 | 复核结论 |
|---|---|---|---|
| 1 | `fa6803ce` | 恒真门三型**双向**体检（c1–c4 + seam_criterion） | ✅ 采纳为主证据源。**否决 1 条**：其称 `mc_positive_control` 的 `mad_dex = T/2.5`「漏掉了 0.6745」并据此说三臂读数应为 0.0265/0.0575/0.2298 —— 我初读时也以为如此，**复核后确认其结论方向正确但表述不准**：`vals` 估的是 σ 不是 MAD，故系统虚高 `1/0.6745`；我用 400 次 MC 实测比值 1.4728–1.4886、解析值 1.48260，据此**把该条改写为 M-12 并补上实测量级**（见 R-2）。另**修正**其对 `C1-SPREAD-RECOMPUTE` 的「恒绿」定性为「弱判别力门」（改生产公式仍会红）。**修正**其引 `DRIZZLE_GEOMETRY.md:323-324`，本人实测真实行号为 **:318-323**。 |
| 4 | `a116b1be` | 恒真门三型**双向**体检（同上，第二独立遍，用于交叉对账） | ✅ 与 #1 大幅收敛，**独立复现**了 C3-G1b 筛选掩盖、C2-G3/C2-N1 同一读数、B-2 归档过期。**否决 1 条**：其称「`C1-DIAG` desc 所述条件被自身数据证伪」并给出分歧/效应 = 0.009/0.114/0.25/0.36 —— 本人独立复算归档得 0.0090/0.1145/0.2452/0.3600，**采纳**（这是它最有价值的新增）。**修正**其「C2-G1 的 `inv_g=−3.64e8`」未指出这同时说明**三参数拟合已崩溃而门不检查**，我据此扩写 M-11。 |
| 3 | `bd885f71` | 复现链 + 自愈判据专项 | ✅ 采纳 B-2 的**完整自愈链路**（它比我先找到「`run_all.sh:22-23` 无条件重写 SNAPSHOT 且把 `results/*.json` 纳入指纹」这一环节，我原先只测出「11/15 失配」这一现象）。**修正 1 条**：它称「全仓无脚本产出 `vis/out`」，我保留该结论但按其自陈的核不到项降级措辞为「输入不在仓且被 gitignore 排除」，不断言「永远无法重跑」。**修正**其 `TILE = M.TILE_SPAN`（`c2:45`）零使用的判断——我只复核了 `g.add` 相关死代码，该条按其报告原样记入 S-1 但标注未独立复核。 |
| 2 | `f10c91ff` | 文档 ↔ 代码交叉面（数字/门名/伪引/控制流/选择性报告/源文） | ⚠️ **未回交报告**。该职责我已本人逐条完成（B-6 伪引、M-7/M-8/M-9/M-10/M-15/M-16 文档-代码冲突、CRITERIA 内部矛盾），不依赖其结果，故**本片未因该代理缺席而留下空白**。如实记录。 |

**复核方法**：对每个子代理结论，凡给出 `文件:行` 且我能在原文或归档中复算的，一律本人重跑一遍（`sed -n` / `grep -n` / `python3 -c` 解析 JSON）；凡与我的独立结论冲突的，逐条仲裁并在上表写明否决理由；凡属推测而无可复现证据的，降级为「核不到」或删除。

### 子代理与我结论的重合度

4 个子代理报出的**独立于我**的新增项共 8 条，我全部**独立复核后采纳**：`C3-G3b` 逻辑蕴含、`C1-DIAG` desc 被自身数据证伪、`C2-G1` 三参数拟合崩溃（`inv_g` 负、`p` 触界）、`C4-G5` 门限/声明不符、8 处 `ASTROCS_DESIGN` 悬空、3 处 cpp 行号漂移、归档 `results/` 与 `PHASE2_REJECTION.md:832` 正本指定的冲突、12 处死代码。

**我否决的 3 条**（保留在此以示否决留痕）：① 子代理 #1 对 MC 换算的量级表述（已改为 M-12 并补实测）；② 我自己在初读阶段怀疑的 `A_PIXEL_SR` 不一致（实测相对偏差 `0.000e+00`，见 R-5）；③ 子代理 #1 引 `DRIZZLE_GEOMETRY.md:323-324` 的行号（实测为 `:318-323`）。

---

## 8. 自证段

以下命令在 `/workspace/Astro CS Database` 根目录执行，全部只读，**不编译、不跑 ctest/pytest/构建/实验脚本**。

```bash
# ── C1 · 基线与片成员 ───────────────────────────────────────────────
git log --oneline -1                      # 期望: f9650dd0
wc -l 实验/m42-realdata/code/{c1_photometry,c2_absolute_snr,c3_seam_additive,\
c4_leaf_allocation,m42_common,seam_criterion}.py \
      实验/m42-realdata/code/run_all.sh \
      实验/m42-realdata/docs/{CRITERIA,DATA-SOURCES}.md \
      实验/m42-realdata/{README,REPORT_paper}.md
# 期望: 总计 2814（与片清单 实际行数 一致）

# ── C2 · B-1 阻断：归档里 14 条边界的筛选前后对比（纯 JSON 解析，不跑实验）──
python3 -c "
import json; d=json.load(open('实验/m42-realdata/results/c3_seam_additive.json'))
thr=5*d['seam_p3']['null']['sigma']
cons=[];allb=[]
for ax in ('cols','rows'):
  for r in d['seam_block_consistency'][ax]['per_boundary']:
    allb.append((ax,r['x'],abs(r['median']),r['consistent']))
    if r['consistent']: cons.append(abs(r['median']))
print('5*sigma =',thr)
print('门实际判决 max_cons =',max(cons),'  <- 判绿')
print('全 14 条 max|median| =',max(a for _,_,a,_ in allb),'  <- 改口径后判红')
print('最差边界 =',[t for t in allb if t[2]==max(a for _,_,a,_ in allb)])
print('同一图 C3-G1 max_abs_excess_dev =',d['seam_p3']['max_abs_excess_dev'])
"
# 期望: 5*sigma=1.4906e-06; max_cons=5.9157e-07; 全14条=2.7810e-06; C3-G1=4.5013e-06

# ── C3 · B-2 阻断：完整性锚当前状态 ─────────────────────────────────
sha256sum -c 实验/m42-realdata/results/SNAPSHOT.sha256 2>&1 | grep -v ': OK$'
# 期望: 11 个文件「失败」+ 「警告：11 个校验和不匹配」

# ── C4 · B-3 阻断：归档早于现行代码（degenerate 机制未落盘）────────────
python3 -c "
import json
for f in ('c1_photometry','c2_absolute_snr','c3_seam_additive','c4_leaf_allocation'):
    g=json.load(open('实验/m42-realdata/results/%s.json'%f))['gates']
    deg=[r['id'] for r in g['rows'] if 'degenerate' in r]
    print('%-20s summary_keys=%s rows=%d 带degenerate键=%d'
          %(f,sorted(g.keys()),len(g['rows']),len(deg)))
"
# 期望: 四行 summary_keys 均无 n_degenerate；带 degenerate 键 均为 0
# 而 m42_common.py:249-250 现在无条件写 n_degenerate/degenerate_ids ⇒ 归档陈旧

# ── C5 · B-4 阻断：DRIZZLE_GEOMETRY 引文是伪引 ───────────────────────
sed -n '233,237p' docs/science/algorithms/DRIZZLE_GEOMETRY.md   # :235-236 讲 scratch 归约池
grep -rn "不得放宽" docs/                                    # 唯一命中在 HIPS_STORAGE_FORM_CONTRACT.md:107
sed -n '318,323p' docs/science/algorithms/DRIZZLE_GEOMETRY.md # 真容差；:322-323 否定求和型判据
grep -n "P1_PHOT_MAX_SPREAD_DEX" lib/infrastructure/scheduler/src/module_adapters.cpp
# 期望: :5536（c1:12 称 :4377）

# ── C6 · B-6 阻断：被引文件不存在；映射后被引句也不在该行 ───────────────
test -f docs/ASTROCS_DESIGN.md && echo YES || echo "NO (不存在)"
ls docs/ | head
for L in 120 126 128 131 143 153; do printf '%4d: ' $L; sed -n "${L}p" docs/ACSD_DESIGN.md; done

# ── C7 · M-11 须修：C2-G1 门判的量与声明的量不是同一个 ────────────────
python3 -c "
import json; v=json.load(open('实验/m42-realdata/results/c2_absolute_snr.json'))['variance_model']
print('pooled_fit=',json.dumps(v['pooled_fit']))   # p=0.2 网格下界, inv_g 负
print('pooled_ci =',json.dumps(v['pooled_ci']))    # [0.2,3.0] 整网格无信息
print('slope     =',json.dumps(v['slope']))       # x_span 仅 0.362 dex
print('EXP06_P_BAND 仅被抄进输出，未参与任何判决 =',v['exp06_p_band'])
"

# ── C8 · M-10 须修：C1-DIAG 的 desc 条件被自身数据证伪 ────────────────
python3 -c "
import json; d=json.load(open('实验/m42-realdata/results/c1_photometry.json'))
for b in ('t2','t3'):
    a=d['cross_frame'][b]['arms']; dis=d['cross_frame'][b]['estimator_disagreement']
    eff=abs(a['uncalibrated_k1|la_ols']['spread']['max_abs_dev_dex']
           -a['calibrated|la_ols']['spread']['max_abs_dev_dex'])
    print(b,'分歧/效应 =',round(dis['calibrated']/eff,4),' (<1 ⇒ 按其自身规则属可判定)')
"

# ── C9 · M-12 须修：MC 单位换算的 1/0.6745 因子（/tmp 纯算术）────────────
python3 -c "
import numpy as np
S=0.6744897501960817
for T in (0.026520,0.057457,4*0.057457):
    r=np.random.default_rng(11).normal(0.0,(T/2.5)/S,size=157)
    print('注入 %.6f -> 返回 %.6f  比值 %.4f  (1/S=%.4f)'%(T,2.5*np.median(np.abs(r-np.median(r)))/S,
          (2.5*np.median(np.abs(r-np.median(r)))/S)/T,1/S))"

# ── C10 · R-5 自我否决：A_PIXEL_SR 与像元尺度逐位一致 ──────────────────
python3 -c "
import math; A=2.197925819099591e-11; ARC=180*3600/math.pi
print('A_PIXEL_SR 等价像元尺度 =',math.sqrt(A)*ARC,' 相对偏差 =',math.sqrt(A)*ARC/0.9670115215027271-1)"

# ── C11 · M-15 须修：README 引的 1.4e-16 sr 已被 EXP-07 否定 ───────────
sed -n '174p' 实验/m42-realdata/README.md
grep -n "1\.4e-1[06]" 实验/healpix-polar/docs/EXP-07-POLAR.md | head -5
sed -n '525p' 实验/healpix-polar/docs/EXP-07-POLAR.md

# ── C12 · B-5 阻断：0.04505 的证据面不在仓 ──────────────────────────
grep -rn "0\.04505" --include=*.md --include=*.py 实验/ docs/ lib/ eng/ | head   # 本片内 0 命中
sed -n '177p;182p' run/PHOTOCURVE-ORCH-01/RECEIPT.md               # 「未验证」「本轮未复算」
grep -n "run/" .gitignore | head -3                               # :17-18 排除 run/*

# ── C13 · S-3 建议：证据面被排除在所有片之外 ─────────────────────────
grep -n "m42-realdata" "run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml"
git -c core.quotepath=false ls-files 实验/m42-realdata
# 期望: 清单只列 11 份 code+md；git 跟踪 16 份（含 results/ 的 5 份）

# ── C14 · 计数口径复算 ───────────────────────────────────────────────
for f in 实验/m42-realdata/code/c*.py; do
  printf '%-24s g.add=%s degenerate=True=%s\n' "$(basename $f)" \
    "$(grep -c '^\s*g\.add(' $f)" "$(grep -c 'degenerate=True' $f)"
done
# 期望: c1=10/0  c2=9/3  c3=9/0  c4=11/0  ⇒ 静态调用点 39；运行时门实例 49（循环展开）
```

**不可复跑声明**：本片的**红绿读数本身无法重新产生**。`run/RELEASE-05/vis/out` 不存在且被 `.gitignore:17` 排除，`bash 实验/m42-realdata/code/run_all.sh` 在当前仓状态下四个脚本全部崩溃。上述命令因此只能核验**代码/文档/归档之间的一致性**，不能核验**判据在真实产品上的红绿**。凡涉及真实红绿的结论，本交付件一律标注其读数来源为 `results/*.json`（9月23 历史产物，且已被现行代码的 `degenerate` 机制作废，见 B-3）。

---

## 9. 未核到（如实列出，不编造）

1. `run/RELEASE-05/vis/out` 的真实内容 —— 目录不存在，`DATA-SOURCES.md` 所列全部字段/二进制尺寸**只有文档自述，无一手可核**。
2. 四个脚本在真实产品上的实际红绿 —— 按纪律未运行；且输入不在仓，即使运行也会崩。
3. 现行代码（含 `degenerate` 机制）下的真实 `n/n_pass/n_fail` —— 只能由代码静态推得（46 live / 3 degenerate），**未经实跑确认**。
4. `0.04505 mag` 的真值 —— 证据文件 `run/SCI-PHOT-FORMULA-01/evidence/d1_zp_sigma_rederive.json` 已被本仓审计记为丢失，需复测。
5. EXP-06 的五个 E 数值、EXP-07 的任何缺陷数值 —— 本单元自陈未独立复现（`README.md:172-175`），我亦未复现。
6. `run_all.sh:14` 的 `/usr/bin/time` 缺失分支 —— 本机存在该文件，未构造缺失场景，该分支为静态推理。
7. 外部文献（Gaia XP、滤过片曲线、astrometry.net healpix.c 的 BSD-3 归属）—— **待联网核验**。本片引用的外部一手来源有三处：`m42_common.py:12`（healpix.c）、`REPORT_paper.md:71`（滤过片曲线解析）、`CRITERIA.md:42-52`（EXP-04 选星口径），我均只核了**仓内陈述的一致性**，未核外部原文。