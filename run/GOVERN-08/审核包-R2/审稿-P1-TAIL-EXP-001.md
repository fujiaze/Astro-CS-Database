# 审稿 P1 · TAIL-EXP-001（G08-05 对抗审稿第 1 遍）

- 片号：`TAIL-EXP-001`
- 层：尾域合并（实验/ 下层行数 < 2000 的小层）
- 基线：仓库 `/workspace/Astro CS Database`，HEAD = `f9650dd0`
- 划片依据：SRS-1 层内 LPT 均衡装箱（n=ceil(层行数/11000)，严格不跨层）
- 纪律：零 git 写；未编译、未跑 ctest/pytest/构建/实验脚本；未改任何仓内文件；未读 `/tmp/acsd_g08/`

---

## 1 读完了吗

| 项 | 数值 |
|---|---|
| 成员份数（权威片清单） | **4** |
| 实读份数 | **4** |
| 成员总行数（权威片清单） | **1643** |
| `wc -l` 实测总行数 | **1643**（与清单逐位一致） |
| 实际读了多少行 | **1643** |
| **覆盖率** | **100.0%（4/4 份，1643/1643 行）** |

**未读完的部分：无。** 4 份成员文件全部逐行读完，无跳读、无抽样、无「顺便看看别的片」。

核验命令：
```
cd "/workspace/Astro CS Database"
wc -l 实验/cone-search-constants/REPORT_paper.md 实验/TAUTOLOGY_REGISTER.md 实验/裁决台账.md 实验/README.md
#  642 实验/cone-search-constants/REPORT_paper.md
#  547 实验/TAUTOLOGY_REGISTER.md
#  425 实验/裁决台账.md
#   29 实验/README.md
# 1643 总计
```

> 说明：`REPORT_paper.md` 的 `wc -l` 为 642，read 工具报 642 行；`实验/README.md` `wc -l` 为 29，read 工具末行编号 30（末行无换行符），二者以 `wc -l` 为准。

### 片清单原始条目（`分片清单/片清单-权威版.yaml:3599-3610`）
```yaml
- 片号: TAIL-EXP-001
  成员份数: 4
  目标行数: 11000
  实际行数: 1643
  成员文件:
    - "实验/cone-search-constants/REPORT_paper.md"
    - "实验/TAUTOLOGY_REGISTER.md"
    - "实验/裁决台账.md"
    - "实验/README.md"
```

### 为核验本片而读、但不属于本片的文件（只读取证，非本片成员）
`docs/ACSD_DESIGN.md`、`docs/engineering/DOCUMENT_GOVERNANCE.md`、`docs/science/PHOTOMETRY.md`、`docs/science/PHASE2_UPM.md`、`docs/engineering/MODULE_MAP.md`、`VERSION`、`lib/algorithms/photometry/cpp/src/frame_photometry_fit.cpp`、`实验/photometric-magnitude/code/{scia_common.py,step8_real_frame.py}`、`实验/photometric-magnitude/results/step8_real_frame.json`、`实验/healpix-polar/code/audit/route2/p3lib.py`、`实验/dense-snr-reconstruct/code/route2/exp_P4R2_06_criterion_arms.py`、`实验/shared/synthetic/noise_selftest.py`、`实验/additive-sky-seamless/{code/audit_rework/route3/exp10_weight_arms.py,results/audit_rework/route3/q10_weight_arms.json,results/audit_rework_summary.json}`、`实验/engineering-evidence/release-01/VIS-001/l4/{profile2.py,profile_and_crop.py}`。

---

## 2 本片判定

# 判定：**需修**（无「阻断」级新科学事故；但有 1 条**新伪引**，按伪引纪律须在下一批前处置）

### 最重的 3 条

**① 【新·最重】整改量最大的单元 `absolute-snr` **一条门都没登记**，且 §2.7 声称「已完成分类」的落点（§5）实为「维护规则」、内容不存在。**
登记表 `:3-5` 自称覆盖 absolute-snr，`:55` 给它全表最大的计数（窄实例 108 / 宽实例 368），`:72-82` 更**以它为唯一样板**推出全表口径，其整改分母 **36** 是全表最大单单元分母。但 §2 下的子单元节只有 §2.1–2.7 七节（`:117/:142/:229/:244/:268/:293/:316`）加 §2.8 的四个子小节（`:390/:414/:432/:450`），**没有任何 absolute-snr 小节**，12 处 `absolute-snr` 命中**全部**落在计数、生产绑定、归档脱钩节，无一条是门位登记。全仓第二个「登记表」`实验/dense-snr-reconstruct/docs/TAUTOLOGY_REGISTER.md` 已是 23 行存根。⇒ **36 这个数字目前没有任何逐条证据面支撑它。**
另：`:318` §2.7「见 §5 移交说明：核心 `c1`–`c7` 已完成分类」，但 `:374` 的 §5 是「**维护规则**」，无移交说明、无 c1–c7 分类 ⇒ **指针悬空，声称已完成的内容在本文件里不存在。**

**② 【新】`TAUTOLOGY_REGISTER.md:542` 的「文档 §16.1.3」是伪造章节锚，且实质描述与正本相反。**
`docs/science/PHASE2_UPM.md` 全 30 个标题中**只有 §16 / §16.1 / §16.2 / §16.3，没有 §16.1.3**（`docs/science/PHASE2_UPM.md:371,377,389,407`）。登记表写「与文档 §16.1.3 声明的排序不符」。而真实 §16.1 第 3 条（`:384-385`）只说「`control_ivar` 臂…三臂最小（`uniform` 与 `SNR²` 两臂更大）」—— **不含 uniform 与 SNR² 的相对次序，也不含任何数值**。实测 `q10_weight_arms.json` 三臂 `pollution_leak_bias`：ivar 2.6099 < snr2 3.0872 < uniform 6.4997，**与 §16.1 第 3 条完全一致**。该门判红的真实原因是「实现与**代码 docstring 里硬编码的** 0.0245/0.0651/0.3203 不符」，锚点源头是 `实验/additive-sky-seamless/code/audit_rework/route3/exp10_weight_arms.py:9`。
⇒ 登记表把一条**对代码注释**的核对，叙述成了**对正本条款**的核对。这是恒真门登记表自身的证据基础被伪造锚污染。

**③ 【新】`§2.8.1` / `§2.8.2` 标题声称条数与其表格实际行数不符，整改分母失准。**
`:390` 标题写「`data_matrix/`（12 条）」，其下表格第一列实际编号 **1→19 连续无缺、19 个数据行**；`:414` 标题写「`m16_scene/`（11 条）」，表格实际 **1→13、13 个数据行**。本登记表 §0.5.4（`:86-89`）自订硬规则：「任何对外引用本表数字的地方，必须写明口径；只写「共 N 处恒真门」而不写口径的，视为未完成登记」——这里连口径都写对了，**数仍是错的**，而这正是整改量的分母。

**④ 【新】登记表内部交叉引用错节号：`:20` 让 D 类「见 §4『生产实现绑定』」，而「生产实现绑定」是 **§3**（`:348`），§4 是「归档脱钩」（`:362`）。读者按 §4 查不到任何「不读真实对象」的绑定面。

**⑤ 【承前轮未闭环】`REPORT_paper.md:623`「**全阶段 DONE ✅**」与本文件 `:609` 的核实结论正面矛盾，且未被中和。**
`:609` 已写「下表 P1–P12 所列「证据文件」经逐一核实**全部不存在于仓库中**，故「✅DONE」不成立」，但 `:610-621` 十二行 `✅DONE` 标记与 `:623` 的总括「**全阶段 DONE ✅**」原样保留、无任何撤回标记。R2-S5 已于前轮把这一点列为**阻断**（`审稿-R2-S5-边界与诚实性面.md:37,202,512`），本轮仍在。
⇒ **正是本片 `TAUTOLOGY_REGISTER.md:204-215` 记录的项目已知失效模式「撤回声明没有落到结果行」，在 cone 单元原样重演。**

---

## 3 逐文件清单

### 3.1 `实验/README.md`（29 行 / 30 行尾）
**读了什么**：全文 29 行。

**看到什么**：
- `:16-24` 列出 9 个实验子目录 → 我逐个 `ls -d 实验/*/` 比对，**9 个全部存在，无漏项、无虚构**。登记准确。
- `:28` 「上游：docs/ASTROCS_DESIGN.md §12.1（科学正确性与三重佐证）、§12.2（三类实验数据）、§12.3（创新点实验单元）」→ **文件名 `docs/ASTROCS_DESIGN.md` 在全仓不存在**（`git ls-files | grep -i ASTROCS_DESIGN` → 零命中）。真实文件是 `docs/ACSD_DESIGN.md`。但**三个节标题逐字正确**：`docs/ACSD_DESIGN.md:583/591/601` 正是「科学正确性与三重佐证」「三类实验数据」「创新点实验单元」。⇒ 死的是**文件名**，不是条款内容。
- `:30` 「佐证要求见 docs/DOCUMENT_GOVERNANCE.md §2」→ 该文件真实路径是 `docs/engineering/DOCUMENT_GOVERNANCE.md`（顶层 `docs/` 只有 ACSD_DESIGN.md / GLOSSARY.md / README.md）。**路径缺 `engineering/` 一级**。§2「各层准入判据」确实存在（`:46`）。

**判定**：**须修**（2 处死/错路径，均为上游指针，不影响科学结论）。

### 3.2 `实验/TAUTOLOGY_REGISTER.md`（547 行）
**读了什么**：全文 547 行，含 §0 / §0.5.1–0.5.5 / §1 / §2.1–2.8.4 / §3 / §4 / §5 / §6.1–6.3。

**看到什么（正面，抽查复核结论准确）**——我**独立打开原文**复核了 6 处高杠杆登记，**全部成立**：
| 登记条目 | 我打开的原文 | 结论 |
|---|---|---|
| `:272` `p3lib.py:82-84` `jacobian_chart` 「直接 `np.full_like(u, π/3)`，完全忽略 f,U,V」 | `实验/healpix-polar/code/audit/route2/p3lib.py:82-84`：`return np.full_like(np.asarray(u, dtype=float), np.pi / 3.0)`，形参 `f,u,v` 中 `f`/`v` 从未被引用 | ✅ **成立** |
| `:126` `exp_P4R2_06_criterion_arms.py` `src_true` 与 `S_src_hat` 是「同一次 moffat4 调用的逐字相同值」 | `code/route2/...:49` 与 `:52` 均为 `moffat4(N, N/2, N/2, alpha, A_peak, 0.0)`，实参全同 | ✅ **成立** |
| `:258` `noise_selftest.py:534` `additive_negative_control_ratio` 「乘 0 的项 ⇒ 字面就是 v1/v1，只可能等于 1.0」 | `:534` `(v1 + (mm - 1.0) ** 2 * 0.0) / v1`，同行注释自认「比值恒 1」 | ✅ **成立** |
| `:314` 「`fits_probe` 在全仓不存在」 | `git ls-files | grep fits_probe` → 零命中；而 `profile2.py:3` 与 `profile_and_crop.py:3` 均 `from fits_probe import read_fits` | ✅ **成立**（两脚本不可运行） |
| `:445-446` `closure/variance_closure.json` 与 `M16FIX/regression.json` **不存在** | `find 实验 -name variance_closure.json -o -name regression.json` → 零命中 | ✅ **成立** |
| `:139-140` 归档 `…exp_p4_04_brightness_forward.json → gates` 只有 **7** 门 | 实测 `gates` dict 长度 = **7** | ✅ **成立** |

**看到什么（算术自洽，正面）**：§0.5.2（`:53-63`）四列合计我逐项复算——窄实例 **333**、窄去重 **191**、宽实例 **968**、宽去重 **575**，**四列全部等于 `:63` 声称合计**。§0.5.3（`:74-79`）108/64=1.6875、64/36=1.7778、108/36=**3.000000 精确闭合**。⇒ 本表核心计数纪律是可靠的。

**看到什么（缺陷）**：
- `:542` **新伪引**（详见 §4 阻断 1）。
- `:390` / `:414` **标题条数与表格行数不符**（12→19、11→13，详见 §4 阻断 2）。
- `:454-458` §2.8.4 小结表与 §2.8.1/:2.8.2 逐行「类」列**对不上**（详见 §4 须修 1）。我**自己重新打印了 §2.8.2 全部 13 行的类列**逐行核对：C 类**只有 #4（P3b）1 行**，而小结 `:455` 写 m16_scene = **2**（点名的 `C2_pass` 即 #8，其类列原文是「真门但**容差自指**」，不是 C）；fail-open **只有 #5 1 行**，而小结 `:458` 写 m16_scene = **2**。
- `:457` D 行「全部 **10 条实验门 + 输入守卫**」+ m16_scene「7」= 声称 **17**。算术只有当「10 已含守卫」时成立，但**字面必然读成 11** ⇒ 应为 18。且 §2.8.3 `:437` 说「**整簇**为类 D」，D 在那里是**簇级标签**，§2.8.4 却把它当**逐门计数**混进同一张表。⇒ **粒度混用**。
- **章节顺序错乱**：实际出现顺序为 §0 → §0.5 → §1 → §2 → §3 → §4 → §5(:374) → **§2.8(:381)** → §6(:465)。§2.8 整节被塞在 §5「维护规则」之后、§6 之前，编号逆序。
- `:546` 「按规范 08 §5，**红色**不以 waiver 覆盖」——§5 原文是「**红灯**不以 waiver 覆盖」。规则真实存在（非伪引），但措辞漂移且未加引号（裸从句）。

**判定**：**需修**（技术分类可靠，计数与引用不可靠）。

### 3.3 `实验/裁决台账.md`（425 行）
**读了什么**：全文 425 行，含 §1 D-01…D-11、§2 GA-01…GA-08、§3 G0–G4 承接判定、§4 逐单元索引、§5 待补三表。

**看到什么（正面）**：§4 声称的逐单元条数我逐条核过，**全部正确**：
| 单元 | 台账 `:393-397` 声称 | 我实测 `docs/DISPUTES.md` 唯一 ID 数 / 最大编号 | |
|---|---|---|---|
| P1 | 13（A-P1-01…13） | 13 / 13 | ✅ |
| P2 | 11 | 11 / 11 | ✅ |
| P3 | 12 | 12 / 12 | ✅ |
| P4 | 7 | 7 / 07 | ✅ |
| P5 | 12 | 12 / 12 | ✅ |

`D-xx` 实测 **11** 条（D-01…D-11）✅；`GA-xx` 实测 **8** 条（GA-01…GA-08），与 `:383`「只有 §2 的 GA-01…GA-08 八条」一致 ✅。5 份 `docs/DISPUTES.md` 全部存在 ✅。`:89` 引用的 `实验/healpix-polar/refs.md` 存在 ✅。

**看到什么（缺陷）**：
- **`:256` GA-01 落点 `docs/ASTROCS_DESIGN.md` §9 —— 文件名死 + 节号也错。**
  真实 `docs/ACSD_DESIGN.md:546`（§10「I/O 与原子产品」）**逐字**写着：「统一 I/O 是文件级唯一边界：Phase1 读原始帧与校准帧、写 HiPS；Phase2 读 HiPS、写天球 HiPS；Phase3 读天球 HiPS、写 FITS。」——**与 GA-01 终裁三条逐条对应**。而 §9（`:530`）标题是「**CPU 后端与资源**」，通篇无 I/O 边界条款。
  ⇒ 不是「权威缺失」，是**指针指错**：正确落点是 `docs/ACSD_DESIGN.md` **§10**。
- **`:273` GA-02 落点 `docs/ASTROCS_DESIGN.md` §3.1** —— 文件名死，但节号**对**（`ACSD_DESIGN.md:177`「### 3.1 数据对象」）。⇒ 仅需改文件名。

**判定**：**需修**（2 处死文件名；其中 GA-01 附带节号错）。

### 3.4 `实验/cone-search-constants/REPORT_paper.md`（642 行）
**读了什么**：全文 642 行，含 §0 整改登记（0.1–0.4）、摘要、§1–§11 附录、PROGRESS 表、参考文献。

**看到什么（正面）**：§0.2 证据链核实表（`:72-83`）逐条**成立**，我独立复核了其中 7 项：
- `VERSION` 实为 `0.1.0-alpha.1` ✅（报告 `:18-19` 正确）
- `docs/science/PHOTOMETRY.md` 存在，全文「锥」字计数 = **0** ✅
- 其 §2 实际标题是「符号表」（`:18`）✅；无 §2.1 ✅
- `frame_photometry_fit.cpp:173-174` 确为 FOV 钳位（原标 172-173 偏移 1，**报告的偏移诊断正确**）✅
- `step8_real_frame.py:89` 确为 `from astropy.wcs import WCS` ✅
- `scia_common.py:597` 确为 `return "ABOVE_CEILING"` ✅；`docs/engineering/MODULE_MAP.md:142` 确为 `ABOVE_CEILING` 的 verdict_id 行 ✅
- `scia_common.py:476-481` 确为 `sigma_ceiling` 的 7 项预算求和 ✅
- 算术：2.6595、0.4850、0.044274 三数**全部复算正确** ✅

**看到什么（缺陷）**：
- `:623` 「**全阶段 DONE ✅**」未被撤回（阻断 3）。
- `:44-45` 「本报告全文 **`ABOVE_CEILING` 出现 0 次**」——**实测 6 次**（`:40,44,317,321,588,592`；§0 整改节 2 次、正文 4 次）。该计数被 R2 整改节**自身的写入**推翻。
- `:123` 仍写 `frame_photometry_fit.cpp:172-173`——§9 表（`:456`）已订正「偏移 1」，**§1.1 的正文引用没同步**。又是「撤回声明没有落到结果行」。
- **`:216-217` 双边界判据的 `n` 全文从未定义**。且由 `:52-53` 的 P1 分项反解：`σ_psfsys=0.013668` 占 ceiling² 72.3% ⇒ 预算 sqrt = 0.0160748 ⇒ `ρ_hi = 0.0205608/0.0160748 = 1.2791` ⇒ `1+3×1.166/√n = 1.2791` ⇒ **n ≈ 157.1**。而报告自报 `matched_stars` 是 **247**（`:269,547`）与 **183**（`:597`），**两个都对不上**（n=247 ⇒ ceiling 0.019653；n=183 ⇒ 0.020231，均 ≠ 记录的 0.020560838）。
- 附带：`:216` 的 `σ_floor = (1 − 3·1.166/√n)·σ_fit`，当 **n ≤ 12** 时系数 ≤ 0（n=12 时 −0.0098），下界检查 `σ_floor ≤ σ_obs` **空过**，「双边界」退化为单边。报告未声明 `n` 的定义域，也未声明该退化。

**判定**：**需修**（结论方向正确、核实表可信；但存在未撤的成功标记、被自身写入推翻的计数、未同步的行号、未定义且不自洽的门参数）。

---

## 4 发现清单

### 4.1 阻断

#### 【B-1 / 新】伪引：伪造的正本章节号 §16.1.3，且实质描述与正本相反
- **位置**：`实验/TAUTOLOGY_REGISTER.md:542`
- **现状**：「与文档 §16.1.3 声明的排序不符的真实主张门，报告仍当通过项引用」；并据此把该门列入 6 条 `open_red` 之一。
- **应为**：指向真实存在的条款，或改述为「与**代码 docstring 标称数值**排序不符」。
- **证据**：
  1. `docs/science/PHASE2_UPM.md` 全部标题只有 `## 16`(:371) / `### 16.1`(:377) / `### 16.2`(:389) / `### 16.3`(:407)，**无 §16.1.3**。
     ```
     grep -n "^#\{1,6\} " docs/science/PHASE2_UPM.md | grep 16
     grep -n "^#\{1,6\}.*16\.1\.3" -- docs 实验   # → 零命中
     ```
  2. 真实 §16.1 第 3 条（`docs/science/PHASE2_UPM.md:384-385`）原文：「**权重**：`control_ivar` 臂的伪影漏入与噪声 RMS 均为三臂最小（`uniform` 与 `SNR²` 两臂更大），与独立结论一致。」——**不规定 uniform 与 SNR² 的次序，也不含任何数值**。
  3. 实测三臂 `pollution_leak_bias`（`实验/additive-sky-seamless/results/audit_rework/route3/q10_weight_arms.json`）：ivar **2.6099** < snr2 **3.0872** < uniform **6.4997** ⇒ **与 §16.1 第 3 条完全一致**（ivar 最小、两臂更大）。
  4. 门判红的真实原因：`实验/additive-sky-seamless/code/audit_rework/route3/exp10_weight_arms.py:9` docstring 自造「PHASE2_UPM §16.1.3（实测伪影漏入: ivar 0.0245 < uniform 0.0651 < SNR^2 0.3203）」，而 `:118-120` 的门断言该硬编码次序 ⇒ 与实测的 uniform/snr2 互换 ⇒ `ordering_matches_doc_16.1.3 = false`。
  5. **被引数字根本不在正本**：`13×`、`0.0245`、`0.0651`、`0.3203` 在 `docs/science/PHASE2_UPM.md` 中**全部 NOT FOUND**（`grep` 零命中）。这些数字只活在脚本 docstring 里。
  6. **四处扩散、且证据面自我矛盾**（我逐个打开确认）：
     | 层 | 位置 | 内容 |
     |---|---|---|
     | 源头 | `实验/additive-sky-seamless/code/audit_rework/route3/exp10_weight_arms.py:9` | docstring 自造「PHASE2_UPM §16.1.3（ivar 0.0245 < uniform 0.0651 < SNR^2 0.3203）」 |
     | 门键名 | `实验/additive-sky-seamless/results/audit_rework/route3/q10_weight_arms.json:66` | `"ordering_matches_doc_16.1.3": false` |
     | 披露面 | `实验/additive-sky-seamless/code/audit_rework/GATE_DISCLOSURE.json:64,66` | 「**与文档 §16.1.3 声明的排序不符的真实主张门**」 |
     | 汇总面 | `实验/additive-sky-seamless/results/audit_rework_summary.json:90` | 「（**非正本 16.1.3** 的 13×）」 |
     | 报告面 | `实验/additive-sky-seamless/REPORT_paper.md:100` | 「（**非正本 16.1.3** 的 13×）」 |
     | **登记表** | `实验/TAUTOLOGY_REGISTER.md:542` | 「与文档 §16.1.3 声明的排序不符」 |
     
     ⇒ **关键矛盾**：同一批证据的**汇总面与报告面已经写明「非正本 16.1.3」**（即已承认它不是正本条款），而**登记表与披露面仍把它当「文档 §16.1.3」**引用。⇒ 这不是编号笔误，是**已知错误未被下游订正**——与本项目已固化的失效模式「代码改了、归档没重跑」同型（此处是「下游发现了、登记表没改」）。
- **相对前三轮**：`审稿-RR*` / `审稿-R2-*` / `审稿-R3-*` / `审稿-总账.md` 全域 grep `16\.1\.3` → **零命中** ⇒ **新问题**。

#### 【B-2 / 新】恒真门登记表的整改分母条数与自身表格不符
- **位置**：`实验/TAUTOLOGY_REGISTER.md:390`（称 12 条，表实 19 行）、`:414`（称 11 条，表实 13 行）；下游 `:457` D 行数字疑似继承 `:390` 的过时 12。
- **现状**：标题声称条数 ≠ 其下表格第一列连续编号的最大值。
- **应为**：标题与表格一致；并按 §0.5.4 注明口径 III。
- **证据**：
  ```
  sed -n '392,413p' 实验/TAUTOLOGY_REGISTER.md | grep -c '^| [0-9]'   # → 19（编号 1→19）
  sed -n '416,431p' 实验/TAUTOLOGY_REGISTER.md | grep -c '^| [0-9]'   # → 13（编号 1→13）
  ```

#### 【B-3 / 承前轮未闭环】撤回声明未落到结果行：cone 单元仍宣称「全阶段 DONE ✅」
- **位置**：`实验/cone-search-constants/REPORT_paper.md:609`（已注明证据文件全不存在）vs `:610-621`（12 个 `✅DONE`）vs `:623`（「**全阶段 DONE ✅**」）
- **现状**：注释加在表头行，**结果行的 ✅ 标记与总括全部保留**。
- **应为**：按本片 `TAUTOLOGY_REGISTER.md:204-215`「最高杠杆的一处：撤回声明没有落到结果行」的既定处置，撤回必须落到每一行。
- **证据**：`grep -n "全阶段 DONE" 实验/cone-search-constants/REPORT_paper.md` → `:623`；`:610-621` 逐行 `✅DONE`。
- **相对前三轮**：`审稿-R2-S5-边界与诚实性面.md:37,202,512` 已记为阻断 ⇒ **非新，但未闭环**。


#### 【B-4 / 新】登记表内部交叉引用错节号：D 类指向 §4，而「生产实现绑定」是 §3
- **位置**：`实验/TAUTOLOGY_REGISTER.md:20`
- **现状**：`| **D** | 不读真实对象：比较的是本地桩/重实现，不是生产实现 | 见 §4「生产实现绑定」 |`
  但实测：`实验/TAUTOLOGY_REGISTER.md:348` 是 `## 3 生产实现绑定（第 3 项的结论）`；`:362` 是 `## 4 归档脱钩（系统性）`。
  ⇒ **名称匹配 §3，节号写成 §4**；读者按 §4 去查会落在「归档脱钩」，查不到任何「不读真实对象」的绑定面。
- **应为**：改为「见 §3「生产实现绑定」」。
- **证据**：`sed -n '20p;348p;362p' 实验/TAUTOLOGY_REGISTER.md`。
- **注**：该「§4」不可能指规范 08 的 §4（规范 08 的 §4 标题为「CI 与门禁重建」，且无「生产实现绑定」一词），故纯属**登记表内部**节号错。

#### 【B-5 / 新·本片最重】整改量最大的单元 absolute-snr 一条门都没登记，且「已完成分类」的落点悬空
- **位置**：`实验/TAUTOLOGY_REGISTER.md:3-5`（覆盖面承诺）、`:55`/`:72-82`（absolute-snr 的计数与口径 III 分母）、`:316-319`（§2.7 的移交说明）
- **现状（三重矛盾）**：
  1. `:3-5` 自称「本文件是**全实验域**（`实验/*` 五个单元）的恒真门**唯一登记表**，覆盖 absolute-snr、…」——**明确把 absolute-snr 列入覆盖范围**。
  2. 但 §2 的小节实测只有：`:117` §2.1 dense-snr、`:142` §2.2 photometric-magnitude、`:229` §2.3 m42-realdata、`:244` §2.4 shared、`:268` §2.5 healpix-polar、`:293` §2.6 engineering-evidence、`:316` §2.7 additive-sky-seamless、`:390/:414` §2.8 data_matrix+m16_scene ———**没有任何 absolute-snr 小节**。`grep -n absolute-snr` 在本文件的 12 处命中**全部**落在 §0.5 计数、§3 生产绑定、§4 归档脱钩、`:173` 的说明句，**无一条是门位登记**。
  3. 而 absolute-snr 恰是**量级最大的那个单元**：`:55` 给它 窄实例 **108** / 去重 64 / 宽实例 **368** / 宽去重 **206**（四列均为全表第一）；`:72-82` 的 §0.5.3 更**以它为唯一worked example**推出全表的整改分母口径，其口径 III 分母 = **36**（`:78`），是全表最大的单单元分母。
  ⇒ **一个被写进覆盖承诺、又充当全表方法论样板、且整改分母最大的单元，其 36 条整改分母在全仓找不到任何逐条支撑表。**
- **「已完成分类」的落点悬空**：`:318-319` §2.7 写「见 §5 移交说明：核心 `c1`–`c7` **已完成分类**，`audit_rework/`、`reverse_verify/`、顶层 `aux` 三簇的完整逐行表在本单窗口内未完成，如实登记。」
  但实测 `:374` 的 **§5 标题是「维护规则」**，正文只有 4 条维护要求（`:376-379`），**既无「移交说明」，也无任何 `c1`–`c7` 的分类内容**。⇒ §2.7 的指针悬空，它声称「已完成」的那份分类**在本文件里不存在**。
- **应为**：为 absolute-snr 补 §2.x 逐条小节（或明确写「本单元未登记」并从 §0 的「唯一登记表」承诺中撤下）；把 §2.7 的「见 §5 移交说明」改为真实落点，或把 `c1–c7` 的分类结果真正落到 §2.7 内。
- **证据**：
  ```
  grep -n "^### 2\." 实验/TAUTOLOGY_REGISTER.md          # 11 个标题（2.1–2.7 + 2.8.x），其中无 absolute-snr
  grep -n "absolute-snr" 实验/TAUTOLOGY_REGISTER.md       # 12 处命中，全在计数/绑定/归档节
  sed -n '374,379p' 实验/TAUTOLOGY_REGISTER.md            # §5 = 维护规则，无移交说明
  git -c core.quotepath=false ls-files | grep -i TAUTOLOGY # 仅 2 个：本表 + dense-snr 的 23 行存根
  ```
- **相对前三轮**：`审稿-RR*` / `R2-*` / `R3-*` / `总账` 中「逐条登记表」「移交说明」「§5 移交」「absolute-snr 无」**全部零命中**。
  `审稿-R3-T2-文档跨面一致性.md:110,203` 的 **M3** 只抓到覆盖面**数目**矛盾（「五个单元」却列 8 个、漏 `cone-search-constants`），**未**抓到「absolute-snr 在名单内却零登记」这一层。⇒ **本条比 M3 更重且不重复**：M3 是账面笔误，本条是最大整改分母无支撑。
### 4.2 须修

#### 【S-1】§2.8.4 小结表与 §2.8.1/:2.8.2 逐行类标注不符（粒度混用）
- **位置**：`实验/TAUTOLOGY_REGISTER.md:454-458`
- **现状**（我自行逐行打印类列后核对）：
  | 类别 | 小结声称 dm/m16 | 表格实测 dm/m16 |
  |---|---|---|
  | C | 1 / **2** | 1 / **1**（`C2_pass` #8 类列原文是「真门但容差自指」） |
  | fail-open | 3 / **2** | 3 / **1** |
  | D | 10 / 7 | 表格类列显式标 D 的为 5 / 3；`:437` 又称「整簇为类 D」 |
  | A | 1 / 0 | 2 / 0（但按「计入≠否」过滤后为 1） |
- **应为**：先定一个口径（原始行数 or 口径 III 过滤行数；簇级 or 逐门），全表统一重算，并声明各行是否可重叠。
- **证据**：`实验/TAUTOLOGY_REGISTER.md:418-430` 逐行类列（我已独立打印复核）；`:437`「**整簇**为类 D」；`:457`「全部 10 条实验门 + 输入守卫」（字面 11）与合计 17 矛盾。
- **注**：`D` 与「真门」符号冲突（`:394/:396` 类列写「D（真门）」）已被 `审稿-R3-T4-收敛判定与计数核验.md:97` 记过 ⇒ **非新**；但 C/fail-open 两处不符与 D 行字面矛盾**未见于前三轮**。

#### 【S-2】GA-01 落点节号错：真实条款在 §10 而非 §9
- **位置**：`实验/裁决台账.md:256`
- **现状**：`docs/ASTROCS_DESIGN.md` §9（文件名死）；而 §9 在真实文件中是「CPU 后端与资源」。
- **应为**：`docs/ACSD_DESIGN.md` **§10**。
- **证据**：`docs/ACSD_DESIGN.md:546` 逐字「统一 I/O 是文件级唯一边界：Phase1 读原始帧与校准帧、写 HiPS；Phase2 读 HiPS、写天球 HiPS；Phase3 读天球 HiPS、写 FITS。」；`:530` §9 标题「CPU 后端与资源」。
- **附**：`实验/裁决台账.md:273` GA-02 同名文件死链，但 §3.1 节号正确（`ACSD_DESIGN.md:177`），只需改文件名。

#### 【S-3】`实验/README.md` 两处上游指针无效
- **位置**：`:28`（`docs/ASTROCS_DESIGN.md`，文件不存在）、`:30`（`docs/DOCUMENT_GOVERNANCE.md`，真实路径 `docs/engineering/DOCUMENT_GOVERNANCE.md`）
- **应为**：`:28` 改 `docs/ACSD_DESIGN.md`（§12.1/12.2/12.3 三个标题本身正确，无需改）；`:30` 补 `engineering/`。
- **证据**：`git -c core.quotepath=false ls-files | grep -i ASTROCS_DESIGN` → 零命中；`docs/ACSD_DESIGN.md:583/591/601`；`docs/engineering/DOCUMENT_GOVERNANCE.md:46`。
- **注**：`docs/engineering/UNRESOLVED_REGISTER.md:3965` 已登记「`ASTROCS_DESIGN.md` 已改名 `docs/ACSD_DESIGN.md`」⇒ 死链家族**已知**，但本片这 3 处（README:28、裁决台账:256、:273）**未随该登记一并订正**。全仓活面该串共 **179 处**（tracked），本片占 3 处。

#### 【S-4】REPORT_paper 自身的计数被自身写入推翻
- **位置**：`:44-45`「本报告全文 **`ABOVE_CEILING` 出现 0 次**」
- **现状**：实测 **6 次**（`:40,44,317,321,588,592`）。R2 整改节自身的写入（2 次）与内嵌订正（4 次）推翻了该计数。
- **应为**：改为「整改前正文出现 0 次；含本整改节共 6 次」或直接删去该易腐计数。
- **证据**：`grep -n ABOVE_CEILING 实验/cone-search-constants/REPORT_paper.md`。

#### 【S-5】REPORT_paper §1.1 行号未随 §9 订正同步
- **位置**：`:123` 仍写 `frame_photometry_fit.cpp:172-173`；`:456` §9 表已注明「行号原标 172-173 偏移 1」。
- **证据**：真实钳位在 `lib/algorithms/photometry/cpp/src/frame_photometry_fit.cpp:173-174`。
- **应为**：§1.1 直接写 173-174。（同 B-3 的「撤回未落到结果行」型。）

#### 【S-6 / 新】§2.3 m42 七条门行号普遍漂移，其中一条指针根本不是门（差 207 行）
- **位置**：`实验/TAUTOLOGY_REGISTER.md:238` 声称 `code/c3_seam_additive.py:67` 为 `C3-SELFTEST-seam-copy`
- **现状**：我实测 `实验/m42-realdata/code/c3_seam_additive.py:67` 原文是
  `return dict(ok=bool(worst == 0.0), max_abs_diff=worst,` —— **是 `selftest_seam()` 的 return 语句，不是门**。
  该门的真实位置是 **`c3_seam_additive.py:274`**（`g.add("C3-SELFTEST-seam-copy", …)`）。**差 207 行。**
- **应为**：指针订正到 `:274`；并逐条复核 §2.3 七条门的行号。
- **证据**：
  ```
  sed -n '67p' 实验/m42-realdata/code/c3_seam_additive.py
  grep -n "C3-SELFTEST-seam-copy" 实验/m42-realdata/code/c3_seam_additive.py   # → 274
  ```
- **重要限定（红队自查）**：该指针指向的**不是门**，因此**按 `:67` 去核验这条登记会核到别的东西**；但**实质判定是对的**——`c3_seam_additive.py:274-278` 确有该门、且 `source=…sci_c_common.py:377-411`，「逐字副本只能抓副本漂移」成立。⇒ **这是指针缺陷，不是结论缺陷**。同批另 5 条（`:233/:234/:235/:236/:237`）行号有 ±1~14 的漂移，其中 `:233`、`:234` 落到相邻门的区间内。

#### 【S-7 / 新】§4 归档脱钩表：`:371` 说「三次」，表实为 4 行
- **位置**：`实验/TAUTOLOGY_REGISTER.md:371`
- **现状**：原文「⇒ **「代码改好了、归档没重跑」在本仓是系统性的**，三次都导致报告仍引用已被代码自己否定的证据。」
  但其上 `:364-369` 的表实为 **4 个数据行**（absolute-snr / photometric-magnitude / dense-snr-reconstruct / healpix-polar）。
- **应为**：改为「四处」，或明确说明「三次」另有所指。
- **证据**：`awk 'NR>=364&&NR<=370&&/^\| `/{n++}END{print n}' 实验/TAUTOLOGY_REGISTER.md` → **4**。

#### 【S-8 / 新】`:369` 把「无归档支撑」写成了「与落盘 JSON 冲突」（实情更重，且措辞与证据矛盾）
- **位置**：`实验/TAUTOLOGY_REGISTER.md:369`
- **现状**：原文「`healpix-polar` `read_tables.py:24` │ 整行硬编码 `"PASS"`，与落盘 JSON 冲突」。
- **应为**：改为「该行**无任何归档支撑**」——因为 JSON 里根本没有 N=5 记录，**无从「冲突」**。
- **证据**：
  1. `实验/healpix-polar/results/audit/kcorr/g0_sanity.json` 顶层键实测为
     `['group','identity_M','identity_geometry','N289','N25','reference_k_gauss','gate','written_utc']` —— **无 `N5`**；
     `gate` = `{'pass_identity_M': True, 'pass289': True, 'pass25': True}` —— **无 `pass5`**。
  2. `实验/healpix-polar/code/audit/kcorr/read_tables.py:22`/`:23` 用 `%s` 读 `g0['gate']['pass289']` / `pass25`；
     **`:24` 整行无任何 `%s` 插值**：`L.append("| 5 | 1.634 (3x40000: …) | 0.007 | 1.637 (直接定征 400k) | PASS(=k_gauss(5), 非 1) |")`
     ⇒ 同一张表里 N=289、N=25 **真读归档**，唯 N=5 这一行是字面量。
- **注**：登记表 §2.5 #15 已把它登记为**恒红门**（`run_extra.py:57` `gate_pass`），此处是**同一缺陷在报告面（`read_tables.py:24`）的第二处载体**，两处应合并登记。

#### 【S-9 / 新】`:368` 给出的门行区间只覆盖 14 门中的 4 门
- **位置**：`实验/TAUTOLOGY_REGISTER.md:139-140` 与 `:368`（「现行代码 `:795-803` 有 **14** 门」）
- **现状**：`:795-803` 实测只含 4 个门键（`absolute_scale_matches_frozen_first_hand` / `gain_matches_independent_archived_reading` / `gain_injection_is_detected_by_this_suite` / `reference_is_rewrite_proof`）；`res["gates"]` 字典实为 **`:731–:804`**。
- **应为**：区间订正为 `:731-804`。
- **证据**：归档 `gates` 实测 **7** 门（我独立复核，与登记表一致）；代码 14 门、交集 6 ⇒ script-only **8**，与登记表「8 条门没有任何实测记录」一致（**这一条登记表算对了**）。**错的只是行区间。**
#### 【S-10】双边界判据的 `n` 未定义，且与报告自报数据不自洽；`σ_floor` 在 n≤12 时空过
- **位置**：`实验/cone-search-constants/REPORT_paper.md:216-217`
- **现状**：`√n` 中的 `n` 全文只出现这两次，**从未定义**。反解 P1 的 `σ_ceiling`（`:38`）得 `ρ_hi ≈ 1.2791` ⇒ **n ≈ 157.1**；而报告自报 `matched_stars` 为 247（`:269,547`）与 183（`:597`），**均不匹配**。另：`1−3·1.166/√n ≤ 0` 当 **n ≤ 12**（n=12 时 −0.0098）⇒ 下界检查空过，「双边界」退化为单边。
- **应为**：定义 `n` 及其定义域，声明 `n ≤ 12` 时下界退化，并对该区间 fail-closed。
- **证据**：复算脚本见 §8。
- **注**：这使 §0.1「替换分母」的核心量化论证（`:59`「扣掉 1.166 包络因子后差距仍有 3.402 倍」）虽**可复现**，但必须经一个**报告从未披露的中间量 ρ_hi ≈ 1.2791**，而该中间量反推出的 n 与报告自报数据矛盾。

### 4.3 建议

- **N-1**：`实验/TAUTOLOGY_REGISTER.md` 章节顺序为 §0→§0.5→§1→§2→§3→§4→§5(:374)→**§2.8(:381)**→§6(:465)。§2.8 整节逆序夹在 §5 与 §6 之间，建议移入 §2 末尾。
- **N-2**：`实验/TAUTOLOGY_REGISTER.md:546`「**红色**不以 waiver 覆盖」→ §5 原文是「**红灯**不以 waiver 覆盖」。规则真实存在（非伪引），仅措辞漂移；建议补引号或改词。
- **N-3**：`实验/cone-search-constants/` 无 `README.md`（违反 AGENTS.md:77「每个文件夹内放一个极简 README」）——报告 `:82` 自己已指出。
- **N-4**：本登记表**依赖的被引规范文件不在 git 内**：`run/GOVERN-08/工作包-GOVERN-08原件/standards/08_编译与CI重做规范.md` 被 `.gitignore` 的 `run/*` 屏蔽（`git cat-file -e HEAD:<路径>` 报「在磁盘上，但不在 HEAD 中」）。登记表 `:10/:358/:513/:546` 四处引用它 ⇒ **任何 checkout 都复现不了这些引用**，与 AGENTS.md §4「证据须可复核」冲突。建议把被引条款原文内联或迁到受控目录。
- **N-5**：REPORT_paper 的治理形态问题（AGENTS.md:74「正文无日期、版本号、任务流水编号、commit」）：`:605-623` PROGRESS 表含 12 个 `2026-09-26` 日期列与 `P1–P12` 流水编号，`:640-642` 含 `Version: 0.11.0-alpha.3` 与 `Commit: d8495a65…`，全文 `G08-05` 出现 8 次。⇒ 建议按本片 `REPORT_paper.md` 自身的 §0 结论，把 PROGRESS 表整体撤除而非逐行标注。

---

## 5 我主动构造的反例

### 反例 A：试图推翻「§0.1 的量化论证有误」
- **构造**：不信 §0.1 的 3.402，先假设它是错的，穷举能否由其自报的两个数（0.054682 / 0.020560838）导出。
- **期望推翻**：若任何运算都得不到 3.402，则 §0.1 的核心量化论证不可复现，R2 整改的核心论据被证伪。
- **是否推翻**：**部分推翻我的第一直觉，随后自我推翻。**
  - 我最初算出：`2.6595/1.166 = 2.2809`、`2.6595×1.166 = 3.1010`，**都不是 3.402**；反解所需因子为 1.2792，与 1.166 不合 ⇒ 我一度判定「3.402 不可复现」。
  - 再查：由 `σ_psfsys` 占 ceiling² 的 72.3% 反解，预算 sqrt = 0.0160748，`ρ_hi = 0.0205608/0.0160748 = 1.2791`，`cone/0.0160748 = 3.4017` ✅ **正是 3.402**。
  - ⇒ **3.402 是可复现的**，我的第一判断错误。**但**它必须经由一个报告从未披露的 ρ_hi，且该 ρ_hi 反推的 n≈157 与报告自报 matched_stars 247/183 矛盾。⇒ **攻击点转移**：不是「数错」，而是「中间量未披露 + 参数 n 未定义且不自洽」⇒ 转为发现 S-10。

### 反例 B：试图推翻「TAUTOLOGY_REGISTER 的技术分类不可信」
- **构造**：默认它是权威登记表，抽 6 条最容易被反驳的（类 D 的桩函数、同值两次调用、乘 0 项、不存在的模块、不存在的结果文件、归档门数），打开源码逐条对。
- **期望推翻**：任一条不成立，即证「登记表的技术判断不可靠，其计数与整改分母也不可信」。
- **是否推翻**：**未推翻，反而加强了登记表的信誉。** 6/6 全部成立（见 §3.2 表）。⇒ 结论：**本表的缺陷集中在计数、引用与结构，不在门分类**。整改时应信任其分类、只重算其计数。

### 反例 C：试图推翻「裁决台账 §4 的条数索引可信」
- **构造**：假设 13/11/12/7/12 是凑数，去 `docs/DISPUTES.md` 数真实唯一 ID。
- **期望推翻**：任一不符即证索引不可信。
- **是否推翻**：**未推翻**，5/5 全对；`D-xx`=11、`GA-xx`=8 亦全对。⇒ 裁决台账的**计数纪律显著优于**恒真门登记表，两者的缺陷面不同。

### 反例 D：试图推翻「REPORT_paper 的 §0.2 核实表可信」
- **构造**：不信 R2 整改，逐条打开被它引用的源文件。
- **期望推翻**：若 §0.2 有编造，则 cone 单元的「证据不足」结论本身不可信。
- **是否推翻**：**未推翻**。VERSION、PHOTOMETRY「锥」=0、§2=符号表、§2.1 不存在、frame_photometry_fit.cpp:173-174、step8_real_frame.py:89、scia_common.py:597 与 :476-481、MODULE_MAP.md:142 —— **8/8 全部成立**，2.6595/0.4850/0.044274 三数复算全对。⇒ **「证据不足」的结论成立且取证扎实**；其问题只在自身的形态与残留标记。

### 反例 E：试图推翻「§16.1.3 是伪引」
- **构造**：不直接采信 grep 结果，先问「登记表说的『文档』会不会是 PHASE2_UPM 之外的另一份？」——因此去打开 §16.1 全文与门脚本 docstring，核对**实质**而非仅编号。
- **期望推翻**：若 §16.1 某处确实规定了 uniform/SNR² 次序且与实测冲突，则登记表只是编号写错、不是伪引。
- **是否推翻**：**未推翻，伪引成立且比编号错更严重**——§16.1 第 3 条只说「ivar 最小、两臂更大」，**与实测 ivar 2.61 < snr2 3.09 < uniform 6.50 完全一致**。登记表把「对代码 docstring 硬编码数值的核对」写成了「对正本条款的核对」。

---

## 6 盲复算

**方法**：遮住我已形成的判定，先只用原文重新取证，再回来比对。

| 项 | 盲取证结论 | 与原判定比对 | 偏松/偏严 |
|---|---|---|---|
| §0.5.2 四列合计 | 333/191/968/575 全部自洽 | 一致 | — |
| §2.8.1/2.8.2 标题条数 | 12≠19、11≠13 | 一致 | — |
| §2.8.4 C / fail-open | 与类列对不上 | 一致 | — |
| 伪引 §16.1.3 | 章节不存在；且 §16.1 实质与实测**一致** | 一致（并加强） | — |
| GA-01 §9 | §9=CPU，条款在 §10 | 一致 | — |
| README 死链 | ASTROCS_DESIGN 不存在；DOCUMENT_GOVERNANCE 路径错 | 一致 | — |
| ABOVE_CEILING「0 次」 | 实测 6 次 | 一致 | — |
| 「3.402 倍」 | **可复现**（经 ρ_hi=1.2791） | **与我的第一判断相反** | **原判偏严 → 已修正为 S-10** |
| n 未定义 | n 确未定义；反解 n≈157 与 247/183 不符 | 一致（因 B 项修正而**加强**） | — |
| 全阶段 DONE | :623 未撤 | 一致 | — |
| §2 是否覆盖 absolute-snr | §2 的 11 个 `###` 标题中**没有** absolute-snr；§5 = 维护规则 | 一致（并升为本片最重） | **原判偏松** → 修正后已升为 ① |
| m42 `c3:67` 指针 | `:67` 是 return，真门在 `:274`（差 207 行） | 一致 | — |

**综合**：**偏严 1 处**（3.402，已自我修正为 S-10）、**偏松 1 处**（absolute-snr 的覆盖：我在完成 §3 逐文件通读、把 S4 回报的 L1 线索逐条自证后，才意识到登记表对它承诺覆盖却零登记 —— 已修正并升为本片最重一条 ①）。其余 11 项与盲取证一致。

**两点方法论自省**：
1. 我第一遍读 `TAUTOLOGY_REGISTER.md` 时，§2.8 的标题条数（12/11）一眼可见而 absolute-snr 的缺失**看不见**——因为该登记表**自己反复声称**它是「唯一登记表」，我把它当成了可信前提。⇒ **红队姿态要求我默认现行结论是错的；本次我在「登记表覆盖声明」这条上恰好默认它对了，这是本遍最实质的教训。**
2. 计数器本身也会骗人（S4 报告 `xargs` + 中文路径静默返回 0 命中）。任何「数出来是 0」的结论都必须换一条命令再验一次，否则「没登记」与「命令坏了」不可分辨——B-5 正是靠「标题清单 + 文件清单 + §5 原文」三路交叉才坐实的。

---

## 7 子代理派发记录

**派发 4 个**（要求 3–5 个），全部只读、零 git 写、禁读 `/tmp/acsd_g08/`。

| # | 任务 | 范围 | 状态 |
|---|---|---|---|
| S1 | 伪引逐字核验（规范 08 §4/§5、§16.1.3、`:358`） | TAUTOLOGY_REGISTER | 已回 |
| S2 | 计数表独立复算（§0.5.2/0.5.3、§2.8.1/2.8.2、§2.8.4） | TAUTOLOGY_REGISTER | 已回 |
| S3 | 死链与归档脱钩核验（ASTROCS_DESIGN、§4 归档表、m42 门行号、fits_probe、缺失结果文件） | 跨实验域 | 已回 |
| S4 | 恒红/恒真门定向抽验（8 组）+ 漏登抽查（AST 扫 346 个 .py） | 跨实验域 | 已回（末位） |

### 我如何逐条复核
**原则：结论一律回到原文自己打开。** 我对 S1/S2 的每一条结论都独立执行了验证命令或打开源文件，**不采信为事实**。

**S1（伪引）**
- ✅ **采信并已自行复核**：「§16.1.3 不存在」。我自己跑了 `grep -n "^#\{1,6\} " docs/science/PHASE2_UPM.md`（30 个标题，无 16.1.3）与 `git grep "^#\{1,6\}.*16\.1\.3" -- docs 实验`（零命中）。**已写入 B-1。**
- ✅ **采信并已自行复核**：实测三臂 ivar 2.6099 < snr2 3.0872 < uniform 6.4997，与 §16.1 第 3 条一致 ⇒ 登记表「与文档不符」的实质叙述错误。我自己用 python 读 JSON 打出三个值。
- ✅ **采信并已自行复核**：「被引规范文件不在 git 内」。我自己确认 `run/*` 在 `.gitignore` 中（写入 N-4）。
- ✅ **采信**：「规范 08 §4 引文逐字一致（44 字全等）」与「`:513` 精确子串」——我未逐字重跑比对脚本，但**间接确认**：这些引用**没有**进入我的 B-1/S 缺陷清单，方向上与我的独立核验一致。
- ⚠️ **部分采信**：S1 称 `:546`「红色≠红灯」属措辞漂移非伪引。我采信其**定性**（规则确实存在于 §5），已降级为建议 N-2 而非阻断。**降级理由**：核验目标是「是否从未存在于被引条款」，该规则确实在 §5，不满足伪引定义。

**S2（计数）**
- ✅ **采信并已自行复核**：「§2.8.1 标题 12 / 实测 19」、「§2.8.2 标题 11 / 实测 13」。**我在派发前已独立发现并验证过这两条**（`grep -c '^| [0-9]'`），子代理为独立复现。**已写入 B-2。**
- ✅ **采信并已自行复核**：「§0.5.2 四列合计全对」。我自己用 python 逐列求和得 333/191/968/575，与声称一致。**采信为正面结论。**
- ✅ **采信并已自行复核**：「§0.5.3 精确闭合 ÷3.00」。我自己复算 108/36=3.000000。
- ✅ **采信并已自行复核**：「m16_scene 的 C=2 应为 1、fail-open=2 应为 1」。**我未采信其数字，而是自己重新打印了 `:418-430` 全部 13 行的类列逐行核对**，确认 #4=**C**、#8=「真门但容差自指」、#5=B+fail-open ⇒ C=1、fail-open=1。**已写入 S-1。**
- ⚠️ **部分采信**：S2 称 §2.8.4 表格「混入原始行数与口径 III 过滤行数两种口径」。我采信其**现象**（A 行过滤后为 1、不过滤为 2；B 行过滤后为 7、不过滤为 9），但**未采纳其「两种口径混用」的成因归因**为已证结论——因为我无法排除它只是 S2 自身筛选脚本的产物。**降级理由**：成因未定，我只按可复现的「逐行不符」现象记缺陷，不替它写根因。
- ⚠️ **部分采信**：S2 称「10 条实验门无表格依据、疑似继承 `:390` 的过时 12」。我**未采纳**：这是动机推断，非可复现事实。**否决理由**：动机会推断不可作为缺陷依据；我只保留「`:457` 字面 11 与合计 17 矛盾」这一可复现事实。

**S3（死链与归档脱钩）**
- ✅ **采信并已自行复核**：`README:28` / `README:30` 两处指针失效（我自己 `git ls-files | grep -i astrocs_design` 零命中、并确认 `DOCUMENT_GOVERNANCE.md` 在 `docs/engineering/` 下）。**已写入 S-3。**
- ✅ **采信并已自行复核**：§4 归档脱钩四行的**存在性与实质全部成立**（`exp05_e5_gates.json` n_gates=28、GATES.md 记 G4=PASS、归档 7 门、缺失结果文件、`fits_probe` 不存在）。其中归档 7 门**我在 S3 回来之前就已自己复核过**。⇒ **登记表的技术判断继续得到支持。**
- ✅ **采信并已自行复核**（新）：`:371`「三次」vs §4 表实为 **4 行**——我自己 awk 数了。**已写入 S-7。**
- ✅ **采信并已自行复核**（新）：`:369`「与落盘 JSON 冲突」措辞不准——我自己打开 `g0_sanity.json` 确认**无 `N5` 键、`gate` 无 `pass5`**；并打开 `read_tables.py:22-24` 确认 `:22/:23` 用 `%s` 读归档而 **`:24` 整行字面量**。**已写入 S-8。**
- ✅ **采信并已自行复核**（新，最重）：`:238` 声称的 `c3_seam_additive.py:67` **不是门**——我自己 `sed -n '67p'` 看到的是 `return dict(ok=bool(worst == 0.0), …`，而 `grep -n C3-SELFTEST-seam-copy` 给出真实位置 **:274**，**差 207 行**。**已写入 S-6。**
- ⚠️ **部分采信**：S3 称「m42 七条门行号普遍漂移，7/7 全错」。我**只采纳我自己实测的那一条**（`c3:67→274`）与「同批另 5 条有漂移」的定性说法；**未逐条复算其余 5 条的漂移量**，故 S-6 中只把 `c3` 那条写成确证，其余标为「同批另 5 条建议复核」。**降级理由**：不采信未自证的精确漂移行数。
- ⚠️ **采纳但不计入本片**：S3 另报「登记表漏登 `m16_scene/README.md:11`/`:27` 引用的 `run/RELEASE-02/paper/data/M16FIX/M16-SCENE-FIX-001.md` 不存在」。该路径在 `run/**`（gitignore 区），与我片内 4 份成员文件无直接关系，**仅在此登记为待前台复核**。理由：扩到片外会违反「不要换焦点」。

**S4（恒红恒真门抽验 + 漏登）**
- ✅ **采信并已自行复核（最重）**：「absolute-snr 无逐条小节」。我自己 `grep -n "^### 2\." 实验/TAUTOLOGY_REGISTER.md`（11 个 `###` 标题 = §2.1–2.7 七个子单元节 + §2.8 的四个子小节，**其中无 absolute-snr**）、`grep -n absolute-snr`（12 处全在计数/绑定/归档节）、`sed -n '374,379p'`（§5 = 维护规则，无移交说明）、`git ls-files | grep -i TAUTOLOGY`（仅 2 个文件）**逐项自证**。**已写入 B-5，并提为本片最重一条。**
- ✅ **采信并已自行复核**：「§6.3 声称 `audit_rework/` 38 个脚本无一非零退出」**成立**。⇒ 登记表这条系统性判断继续得到支持。
- ✅ **采信并已自行复核**：`p3lib.jacobian_chart`、`noise_selftest.py:534`、`m16_scene.py:463-466` 的 fail-open ⇒ 这三条我在 S4 回来之前已**独立抽查过**（见 §3.2）。S4 的补充证据（`:550-556` 的 `raise AssertionError` 消费方、`rel_dev = 2.22e-16` 比门小 12 个量级）与我的一致。
- ⚠️ **降级**：S4 建议把 `noise_selftest.py:534` 从 B 降为「诊断读数」，理由是 `:546` 的 verdict 用的是 `rel`、`add_ratio` 只报不判。**我采信其定性但未采纳为降级**，理由：我未自行复算 `add_ratio` 的消费链，**降级会削弱一条对登记表有利的判定**，在未自证前不应下调（对称于我对 S2/S3 动机的否决）。
- ⚠️ **降级**：S4 称 §2.2 #6 的「`N_eff > 1` ⇒ 比值恒 < 1」推断不足，应为 `N_eff > PHOTON_MAG² = 1.1788`。**未采纳**（我只记为待核，未复算 `PHOTON_MAG`），因为它不影响「类 C」定性，且改动方向是**削弱**登记表的论证。
- ⚠️ **不计入本片**：S4 的 L2–L5 漏登（`exp_p4_03_plane_geometry.py:164` fail-open、`b7_absolute_snr_recon.py:956` 空过、`qa_oracle/assemble.py` 退出码 2 未实现、m42 `CRITERIA.md:18` 撤回未落到结果行）——**全部位于本片 4 份成员文件之外**。按「不要换焦点」纪律，**不写入本片发现清单**，仅在此登记为**待前台复核的漏登线索**（其中 m42 那条与 B-5 同属「撤回未落到结果行」型，可与 ⑤ 合并处置）。
- ✅ **采信其排除项**：`exp06_common.py:499/:550/:595` 的 `"ok": True` 是拟合状态位非门——**排除正确**，若采信会误报一条不存在的漏登。

**方法论警示（采信并采纳）**：S4 报告 `xargs grep` 直接吃中文路径的 `git ls-files` 结果会**静默返回 0 命中**，据此会误得「346 个 .py 一个 `sys.exit` 都没有」。**这正是「计数口径」类错误在工具层的形态**——与我 B-2/B-5 同源。已写入 §8 自证段提醒。

**派发失误（如实登记）**：S1 与 S5 因派发时参数笔误发出了**两份完全相同的伪引任务**，两者结论一致、互为复核，白耗一个名额。

**否决统计**：采信并自行复核 **21** 条；降级/部分采信 **5** 条（S1 的 `:546`→建议；S2 的口径成因→只采信现象；S3 的「7/7 漂移」→只采信自证的 1 条；S4 建议的两处**降级**（`:534` 降为诊断、N_eff 阈值）→未采纳，因二者都会**削弱**对登记表有利的判定而我未自证；对称于我对 S2/S3 动机的否决）；**明确否决 2 条**（S2 的「10 继承自 12」动机推断；S1 请求的「续核其余 85 处引号」——超出本片范围，按「不要换焦点」纪律不委派）；不计入本片 **2** 组（S3 的片外 `run/**` 漏登；S4 的 L2–L5 四条片外漏登——均报为待前台复核线索）。

---

## 8 自证段（可复跑命令）

前置：`cd "/workspace/Astro CS Database"`；HEAD 应为 `f9650dd0`。全部命令只读。

```bash
# 0) 片成员与行数（应得 1643）
wc -l 实验/cone-search-constants/REPORT_paper.md 实验/TAUTOLOGY_REGISTER.md 实验/裁决台账.md 实验/README.md

# B-1 伪引 §16.1.3：PHASE2_UPM 全部标题（应只有 16/16.1/16.2/16.3）
grep -n "^#\{1,6\} " docs/science/PHASE2_UPM.md | grep 16
git -c core.quotepath=false grep -n "^#\{1,6\}.*16\.1\.3" -- docs 实验 ; echo "零命中=伪引成立"

# B-1 实质：§16.1 第 3 条原文（不含 uniform/SNR² 次序与任何数值）
sed -n '384,385p' docs/science/PHASE2_UPM.md

# B-1 实质：实测三臂（应 ivar 2.6099 < snr2 3.0872 < uniform 6.4997 ⇒ 与 §16.1 第3条一致）
python3 -c "
import json;d=json.load(open('实验/additive-sky-seamless/results/audit_rework/route3/q10_weight_arms.json',encoding='utf-8'))
a=d['arms'];v={k:a[k]['pollution_leak_bias'] for k in a}
print(v); print('升序:',[k for k,_ in sorted(v.items(),key=lambda x:x[1])])"

# B-1 锚点源头：代码 docstring 自造 §16.1.3 与硬编码数值
sed -n '9p;118,120p' 实验/additive-sky-seamless/code/audit_rework/route3/exp10_weight_arms.py

# B-1 仓内自相矛盾：summary 自承「非正本 16.1.3」
grep -n "16\.1\.3" 实验/additive-sky-seamless/results/audit_rework_summary.md 实验/additive-sky-seamless/results/audit_rework_summary.json 2>/dev/null

# B-2 标题条数 vs 表格行数（应得 19 与 13）
sed -n '390p;414p' 实验/TAUTOLOGY_REGISTER.md
sed -n '392,413p' 实验/TAUTOLOGY_REGISTER.md | grep -c '^| [0-9]'
sed -n '416,431p' 实验/TAUTOLOGY_REGISTER.md | grep -c '^| [0-9]'

# [自证1] §2.8.2 逐行类列（独立复核 C=1、fail-open=1）
sed -n '418,430p' 实验/TAUTOLOGY_REGISTER.md | awk -F'|' 'BEGIN{OFS=" | "}{printf "%-4s %-46s %s\n",$2,$3,$6}'
sed -n '452,460p' 实验/TAUTOLOGY_REGISTER.md

# §0.5.2 四列合计（应得 333 191 968 575）
python3 -c "
n=[108,38,86,0,16,0,43,42];d=[64,32,47,0,16,0,21,11];w=[368,121,166,3,50,2,94,164];x=[206,84,101,1,36,0,54,93]
print(sum(n),sum(d),sum(w),sum(x)); print('108/36=',108/36)"

# [自证2] GA-01 落点节号：§9 是 CPU，条款在 §10
sed -n '530p;546p' docs/ACSD_DESIGN.md

# [自证3] 死链
git -c core.quotepath=false ls-files | grep -i ASTROCS_DESIGN ; echo "零命中=文件名死"
ls docs/ASTROCS_DESIGN.md docs/DOCUMENT_GOVERNANCE.md 2>&1 | tail -2
ls docs/engineering/DOCUMENT_GOVERNANCE.md

# [自证4] ABOVE_CEILING「0 次」（应得 6）
grep -c "ABOVE_CEILING" 实验/cone-search-constants/REPORT_paper.md
grep -n "ABOVE_CEILING" 实验/cone-search-constants/REPORT_paper.md

# B-3 全阶段 DONE（:623 未撤，:610-621 仍 ✅DONE）
sed -n '609p;623p' 实验/cone-search-constants/REPORT_paper.md
grep -c "✅DONE" 实验/cone-search-constants/REPORT_paper.md

# [自证5] 行号偏移（真钳位在 173-174）
sed -n '173,174p' lib/algorithms/photometry/cpp/src/frame_photometry_fit.cpp
grep -n "172-173" 实验/cone-search-constants/REPORT_paper.md

# [自证6] n 未定义 + 反解 rho_hi/n（应得 rho≈1.2791、n≈157，与 247/183 不符）
grep -n "sqrt{n}" 实验/cone-search-constants/REPORT_paper.md
python3 -c "
p1=0.020560838265260225;cone=0.054682;psf=0.013668277986773102
X=(psf**2/0.723)**0.5;rho=p1/X
print('budget sqrt=',round(X,7),'rho_hi=',round(rho,4),'cone/budget=',round(cone/X,4))
print('implied n=',round((3*1.166/(rho-1))**2,1))
for m in (247,183):
    r=1+3*1.166/m**0.5; print('n=%d -> ceiling=%.9f'%(m,r*X))
print('下界退化阈值 n<=',(3*1.166)**2)"

# N-4 被引规范不在 git 内
git check-ignore -v "run/GOVERN-08/工作包-GOVERN-08原件/standards/08_编译与CI重做规范.md"
git cat-file -e "HEAD:run/GOVERN-08/工作包-GOVERN-08原件/standards/08_编译与CI重做规范.md" 2>&1 | tail -1

# 章节逆序
grep -n "^## " 实验/TAUTOLOGY_REGISTER.md

# B-5 §2 的 8 个小节里没有 absolute-snr；§5 是「维护规则」而非「移交说明」
grep -n "^### 2\." 实验/TAUTOLOGY_REGISTER.md
grep -c "absolute-snr" 实验/TAUTOLOGY_REGISTER.md          # 应得 12，且全不在 §2
sed -n '374p' 实验/TAUTOLOGY_REGISTER.md                    # 应为「## 5 维护规则」
git -c core.quotepath=false ls-files | grep -i TAUTOLOGY   # 仅 2 个：本表 + dense-snr 存根

# B-4 登记表内部交叉引用错节号（:20 说 §4「生产实现绑定」，实为 §3）
sed -n '20p;348p;362p' 实验/TAUTOLOGY_REGISTER.md

# B-1 被引数字不在正本（应全部 NOT FOUND）
grep -n "13×\|13x\|0\.0245\|0\.0651\|0\.3203" docs/science/PHASE2_UPM.md || echo "全部 NOT FOUND"
# B-1 扩散面（汇总面/报告面自承「非正本 16.1.3」，登记表仍当文档引用）
grep -n "16\.1\.3" 实验/additive-sky-seamless/results/audit_rework_summary.json \
  实验/additive-sky-seamless/REPORT_paper.md \
  实验/additive-sky-seamless/code/audit_rework/GATE_DISCLOSURE.json \
  实验/additive-sky-seamless/results/audit_rework/route3/q10_weight_arms.json

# S-6 m42 行号漂移：:67 是 return 不是门，真门在 :274（差 207 行）
sed -n '67p' 实验/m42-realdata/code/c3_seam_additive.py
grep -n "C3-SELFTEST-seam-copy" 实验/m42-realdata/code/c3_seam_additive.py

# S-7 「三次」vs §4 表实 4 行（应得 4）
sed -n '371p' 实验/TAUTOLOGY_REGISTER.md
awk 'NR>=364 && NR<=370 && /^\| `/ {n++} END{print n" 行"}' 实验/TAUTOLOGY_REGISTER.md

# S-8 g0_sanity.json 无 N5 键、gate 无 pass5；read_tables.py:24 整行字面量
python3 -c "
import json;d=json.load(open('实验/healpix-polar/results/audit/kcorr/g0_sanity.json',encoding='utf-8'))
print('keys:',list(d.keys()));print('gate:',d['gate'])"
sed -n '22,24p' 实验/healpix-polar/code/audit/kcorr/read_tables.py

# S-9 归档 7 门（登记表这条算对了，错的是行区间 :795-803 → 实为 :731-804）
python3 -c "
import json;d=json.load(open('实验/dense-snr-reconstruct/results/route1/exp_p4_04_brightness_forward.json',encoding='utf-8'))
print('归档 gates:',len(d['gates']))"
grep -n 'res\["gates"\] = {' 实验/dense-snr-reconstruct/code/route1/exp_p4_04_brightness_forward.py
```

**取证方式声明**：以上命令只用于**核对我人读所得**（计数、求和、grep 定位、逐行打印）。所有判定均由本人逐行读完 4 份成员文件后推导，符合规范 04 §1「禁止以脚本扫描替代阅读」。全程零 git 写、未改任何仓内文件。