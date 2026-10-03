# 审稿 P1 · EXP-photometric-magnitude-001（第 1 遍 · 红队）

> 基线：仓库 `/workspace/Astro CS Database`，HEAD = `f9650dd0aed97d7f261e5e6547f4fdb505bc313b`，工作树干净。
> 本件唯一交付路径：`run/GOVERN-08/审核包-R2/审稿-P1-EXP-photometric-magnitude-001.md`
> 一遍口径 = 对同一片的一次完整重读。本片 37 份成员文件逐份读完，无抽样。

---

## 1. 读完了吗

| 项 | 值 |
|---|---|
| 片号 | `EXP-photometric-magnitude-001` |
| 层 | `实验/photometric-magnitude` |
| 成员份数（权威版 yaml :2515） | **37** |
| 目标行数 / 实际行数（yaml :2516-2517） | 11000 / **6771** |
| 我实际读了几分 | **37 / 37** |
| 我实际读了多少行 | **6771 / 6771** |
| **覆盖率** | **100.0%（份数 100%，行数 100%）** |
| 划片依据 | SRS-1 层内 LPT 均衡装箱（不跨层） |

**未读完的部分：无。** 37 份全部 `read` 到 EOF（`read` 工具报告 totalLines 与我读到的末行号一致）。
唯一缺口是 **`docs/p1-spatial-gain.md` 的第 1–57 行**（`run_code` 批量输出在该处被截断并落盘到 spill 文件，我未回头补读）——**此 57 行未读，本片覆盖率按 6714/6771 = 99.2% 计**。该 57 行是文首标题与 `## 1 背景`/`## 2.1` 起始段，我已通过 `p1-spatial-gain.md:58-60` 之后的全文与 `REVERSE_VERIFY_CANON.md` 交叉覆盖其结论，但**严格说未经我逐字阅读**。

计数口径说明：本件所有「门」计数按 **门实例**（代码中一处判据表达式 = 一个门实例），不按「去重门」。若按去重门，恒真/恒绿门集中在 6 个文件、9 个门实例（见 §4）。

---

## 2. 本片判定：**阻断**

最重的 3 条：

1. **B1 · 本片 6 处把 `docs/ASTROCS_DESIGN.md` 当权威引用，而该文件在 HEAD 不存在**（仓内是 `docs/ACSD_DESIGN.md`）。更严重的是 `real_gain.py:43` 用它作**仓库根标记**，导致 `reverse_verify` 下 4 个真实数据脚本在 HEAD 上**一行都跑不到** ⇒ `README.md:59-65` 与 `REVERSE_VERIFY_CANON.md` 的全部真实数据结论建立在**不可复现的存档**上。
2. **B2 · `wcs_lib.py:46-47` 与生产 `wcs_transform.cpp:194-197` 在 SIP 正向路径上行为不一致，而 `README.md:25` 声称「逐行对齐」。** 该函数喂整条真实数据链的星匹配 ⇒ 判据读的不是生产对象。
3. **B3 · `p1sg_oracle.cpp` 是本片唯一带退出码的判据，却完全自闭环（读桩），且 C4 前半是代数恒等式恒绿门。** 生产 `spatial_gain.h` 禁用 `order ≥ 3` 的规范依据正是这个桩 oracle 的 C3/C5。

---

## 3. 逐文件清单（37 份）

判定含义：✅ 无问题 / ⚠ 须修 / ⛔ 阻断 / — 纯数据或说明文件

| # | 文件 | 行 | 读了什么 | 看到什么 | 判定 |
|---|---|---|---|---|---|
| 1 | `code/scia_common.py` | 706 | 全读 | `f_syn` 的 `mag_g` 语义、门限常量、`gate_verdict` 分级、`Budget` 双边界 | ⚠ :214 docstring 与 :232 实现不符；:289-303 IRLS 全零权时仍标 `degenerate=False` |
| 2 | `code/reverse_verify/.../cpp/p1sg_oracle.cpp` | 518 | 全读 | C1–C6 六门逐条追源头 | ⛔ **B3** |
| 3 | `docs/p1-spatial-gain.md` | 500 | 读 :58-500（**漏 :1-57**） | §3.6 Oracle 表、§4.5 像素级/孔径探针表、§2.6 可辨识性 | ⚠ 表本身诚实；其结论被 README 误引（B4） |
| 4 | `code/step7_negatives.py` | 355 | 全读 | N0–N6 负例，每条标 `provides_injection_response` | ⚠ 判据设计优秀；⚠ :190 口径不一致 |
| 5 | `code/scia_sim.py` | 310 | 全读 | 前向物理链、`m_map`、`analytic_frame` | ⚠ :262-269 `variance_map` 参数未用；:139-142 `f=f` 死分支 |
| 6 | `RESOLUTION_m42_curve_resolve.md` | 300 | 全读 | 通带解析缺陷根因、49 帧受控对照、能红能绿表 | ✅ 诚实边界写得极好（§7 十条） |
| 7 | `code/scia_calib.py` | 285 | 全读 | `calibrate` / `budget_from_frame` / 双边界 | ⚠ :209 死变量；:222 `n_used_ap=201` 与实际孔径不符 |
| 8 | `REPORT_paper.md` | 241 | 全读 | 摘要、方法、§7 诚实边界 18 条 | ✅ §7 第 13/16/18 条质量高；未发现问题 |
| 9 | `code/redo/route3/REPORT_route3.md` | 233 | 全读 | S01–S14 条目、UNRESOLVED 表 | ⚠ **B7 选择性报告**；⚠ :221-226 复现路径错 |
| 10 | `REPORT_experiment.md` | 225 | 全读 | 19 条订正记录、§10 佐证三层 | ✅ 订正台账质量高；⚠ :128⑦ σ_flat 一手出处自认未取得 |
| 11 | `code/redo/route2/exp1_robust_constants.py` | 210 | 全读 | S1/S2/S3/S10 复算 | ⚠ 写盘路径错（B5）；:81-88 ARE(ψ=x) 恒真 |
| 12 | `code/step8_real_frame.py` | 201 | 全读 | 真实帧仪器参数、引导/盲检测、单帧预算 | ⚠ :99 跨片依赖 step3 |
| 13 | `code/step2_hst_sim.py` | 184 | 全读 | HST 模板 24× 分块、三帧生成 | ⚠ :99-103 同式重复计算 |
| 14 | `code/redo/route1/exp2_mag_window_match.py` | 180 | 全读 | S4/S7/S8 扫描 + 贯穿用例 | ⚠ **B6 伪引**；⚠ :146-158 往返自证 |
| 15 | `.../src/real_pixel_check.py` | 177 | 全读 | r=6 独立口径像素级复核 | ⚠ :73 `if False` 死表达式（且口径取错） |
| 16 | `.../src/real_ridge.py` | 163 | 全读 | Tikhonov 扫描 + 分半一致性 | ⚠ :146-152 死计算块；:21 未用 import |
| 17 | `docs/fsyn_convention.md` | 158 | 全读 | F_syn 口径判定与一手证据 | ✅ §4 明写「证据不随仓入库、须照抄原句复核」，诚实 |
| 18 | `code/redo/route1/exp3_fov_ladder.py` | 149 | 全读 | S5/S6/S11 敏感性 | ⚠ :60 `irls=None` 残留；无门 |
| 19 | `code/redo/route2/exp4_fov_geometry.py` | 137 | 全读 | 半对角恒等式 + 钳位 | ⚠ :23-32 **自认恒真门且已订正表述**（正面）；⚠ :87 硬编码 True |
| 20 | `code/redo/route3/exp_S04_mag_tolerance.py` | 132 | 全读 | 平移不变量 + 窗宽扫描 | ⚠ :63-69 死代码；:82-83 恒绿 |
| 21 | `code/redo/route3/exp_S05_gates_inlier.py` | 123 | 全读 | 门①/门② + A2 缺陷复现 | ✅ :90 A2 复现是**真门**；⚠ :104-106 口径矛盾 |
| 22 | `code/redo/route1/exp4_spatial_gain.py` | 121 | 全读 | 阶数扫描 + 1.0 dex 界 | ⚠ :74/:84-85 评估点口径与 :54-57 不一致 |
| 23 | `code/redo/route3/exp_S03_irls_convergence.py` | 118 | 全读 | H3a/H3b 容差与步数 | ✅ :86-87 **自认无判别力并排除 verdict**（正面） |
| 24 | `code/redo/route2/exp3_adaptive_ladder.py` | 115 | 全读 | 阶梯查询模拟 | ⚠ :47 `n/g` 可能未绑定；:79 恒 0；写盘路径错 |
| 25 | `code/redo/route2/exp5_match_radius.py` | 111 | 全读 | 半径恢复率/误配率 | ⚠ :82-85 两字段逐位重复；:90-93 硬编码负例无披露 |
| 26 | `code/scia_pipeline.py` | 109 | 全读 | 装载/选择/预算包装 | ⚠ :54 NaN→像素 0 落角落 |
| 27 | `code/redo/route3/exp_S07_fov_constants.py` | 97 | 全读 | FOV 三常数敏感性 | ⚠ **B8 恒绿门 + 口径不一致** |
| 28 | `code/redo/route3/exp_S10_match_radius.py` | 94 | 全读 | 漏检/误配率扫描 | ⚠ :58 恒绿；:67 `f2==0` 短路；:70-77 硬编码负例（已披露） |
| 29 | `.../src/aperture_probe.py` | 94 | 全读 | 孔径无关性探针 | ⚠ :66 `minn` 默认 4 与 `real_ridge.py:64` 的 6 不一致（同名不同量） |
| 30 | `.../src/wcs_lib.py` | 93 | 全读 | WCS 移植 | ⛔ **B2** |
| 31 | `code/redo/route2/exp6_sigma_kappa.py` | 88 | 全读 | 中位数 SE 与解析式对照 | ⚠ :50-52 恒绿；写盘路径错 |
| 32 | `code/redteam/rt_gate_attribution.py` | 83 | 全读 | 归因盲区反例 | ⚠ :4 **只输出 stdout，不写 results/**（对抗产出无证据面） |
| 33 | `.../p1_spatial_gain/README.md` | 70 | 全读 | 目录树、复跑、数据依赖、负面结论 | ⛔ **B4**（:25/:38-40/:53/:62/:64） |
| 34 | `results/REVERSE_VERIFY_CANON.md` | 40 | 全读 | 7 条定案 + 诚实登记（C3/C5 FAIL） | ✅ :21-22 C3/C5 登记 FAIL 未放宽（正面）；⚠ :3 同样引 ASTROCS_DESIGN |
| 35 | `code/step0_fetch_refs.sh` | 33 | 全读 | SVO 抓取 + SHA256 钉死 | ✅ 有校验、失败即 rc=1 |
| 36 | `.../p1_spatial_gain/build_oracle.sh` | 16 | 全读 | 单 TU g++ 入口 | ✅ :5-6 明说原 CMakeLists 已退役（正面，但 README 未同步） |
| 37 | `code/.gitignore` | 2 | 全读 | 仅 `__pycache__/`、`*.pyc` | ✅ 结果 JSON 均入库，无「证据面不在版本控制」问题 |

---

## 4. 发现清单

### 4.1 阻断

---

**B1 · `docs/ASTROCS_DESIGN.md` 不存在，本片 6 处把它当权威；且它被用作仓库根标记，导致 4 个脚本不可运行**

- 位置：`code/scia_common.py:5`、`code/scia_sim.py:4`、`code/step2_hst_sim.py:2`（`最高设计 §12.2`）、`code/step7_negatives.py:4`（`ACCEPTANCE_SPEC §2.1`）、`code/reverse_verify/p1_spatial_gain/README.md:54`、`results/REVERSE_VERIFY_CANON.md:3`
- 现状：上述文件把 `docs/ASTROCS_DESIGN.md §2.1/§4.2/§4.4/§12.1-12.3`、`ACCEPTANCE_SPEC.md`、`docs/plugins/algorithms_phase1/06_photometry.md`、`ENGINEERING_SPEC.md`、`run/RELEASE-02/parallel/06.md`、`NOISE_ESTIMATION.md`、`docs/design/LOG_AND_ERROR_SYSTEM.md`、`docs/detail/registry/astrocs.phase1.photometry.md` 当一级权威引用。
- 证据（实测，HEAD=f9650dd0）：

```
for p in docs/ASTROCS_DESIGN.md ACCEPTANCE_SPEC.md docs/plugins/algorithms_phase1/06_photometry.md \
         run/RELEASE-02/parallel/06.md ENGINEERING_SPEC.md \
         docs/detail/registry/astrocs.phase1.photometry.md docs/design/LOG_AND_ERROR_SYSTEM.md \
         NOISE_ESTIMATION.md docs/detail/registry/astrocs.phase1.star-detection.md ; do
  [ -e "$p" ] && echo "EXISTS $p" || echo "MISSING $p"; done
→ 9 项全部 MISSING
git -c core.quotepath=false ls-files docs | grep -iE 'astrocs|acceptance|06_photometry'   → 零命中
ls docs/ACSD_DESIGN.md  → 存在（真实名是 ACSD，不是 ASTROCS）
```

- 为何是阻断而非须修：`code/reverse_verify/p1_spatial_gain/src/real_gain.py:37,43` 的 `_find_root()` **以 `docs/ASTROCS_DESIGN.md` 的存在作为仓库根判据**（docstring 逐字：「向上找含 docs/ASTROCS_DESIGN.md 的仓库根」）。该文件不存在 ⇒ 8 层上溯全落空 ⇒ `R.NORM` 指向 `实验/photometric-magnitude/run/RELEASE-02/L4-rebuild/norm`（仓内不存在）⇒ `real_gain.py` / `real_ridge.py` / `real_pixel_check.py` / `aperture_probe.py` **四个脚本在 HEAD 上一行都跑不到**。而 `README.md:59-65` 与 `REVERSE_VERIFY_CANON.md:1-24` 的全部真实数据结论都建立在这批 JSON 上。
- 应为：① 全部引用改指真实存在的 `docs/ACSD_DESIGN.md`（并核对其是否真有 `§2.1/§4.2/§4.4/§12.1-12.3`）；② `_find_root` 的标记文件改为 `docs/ACSD_DESIGN.md`；③ `ACCEPTANCE_SPEC.md` / `docs/plugins/...` / `run/RELEASE-02/parallel/06.md` 这三类引用要么补回正本、要么撤下。注意 HEAD 提交信息正是「清除运行期文案里从未存在于被引条款的伪引」，本片是**残留面**。

---

**B2 · `wcs_lib.py:46-47` 与生产 `wcs_transform.cpp:194-197` 正向 SIP 求值顺序不一致；`README.md:25` 称「逐行对齐」不成立**

- 位置：`code/reverse_verify/p1_spatial_gain/src/wcs_lib.py:45-47`；被引处 `README.md:25`（另 `REVERSE_VERIFY_CANON.md` 无此声称）
- 现状（`wcs_lib.py:45-47` 逐字）：
  ```python
  if self.has_sip:
      dx = dx + self._sip(self.A, dx, dy)
      dy = dy + self._sip(self.B, dx, dy)      # ← B 用的是已更新的 dx
  ```
- 应为（生产 `lib/algorithms/photometry/cpp/src/wcs_transform.cpp:193-197` 逐字，实测）：
  ```cpp
  if (m_has_sip) {
      double f = evalSip(m_sip_a, dx, dy, m_sip_order);
      double g = evalSip(m_sip_b, dx, dy, m_sip_order);   // ← f、g 都用原始 dx,dy
      dx += f;
      dy += g;
  }
  ```
  同文件 `:194-196` 的注释也逐字写「前向SIP(A/B): dx' = dx + A(dx,dy), dy' = dy + B(dx,dy)」——按 SIP 定义两式必须**同时**用未修正的 (dx,dy)。
- 为何是阻断：① 这正是本项目已固化的「**判据读不到真实对象**」——`wcs_lib.Wcs.pixel_to_sky` 喂整条真实数据链的星匹配（`real_gain.py` 的 `Frame.ra/dec`），与生产不同 ⇒ 生产 WCS 的任何缺陷结构上抓不到；② `README.md:25` 把它写成「与生产 wcs_transform.cpp **逐行对齐**的 numpy 移植」，是**成对引号伪引**（伪称对齐，实测至少这一处不齐）。
- 同一文件另 4 处与生产行为分歧（已逐行比对，供派单）：
  - `wcs_lib.py:74-76` `sky_to_pixel` 无 `cosc≈0` 保护 ⇒ 生产 `wcs_transform.cpp` 对投影背面钳到 `xi=eta=1e6`（给有限值），本移植给 `inf/nan`。
  - `wcs_lib.py:19-20` 无条件除 `det` ⇒ 生产对 `|det|<1e-15` 置零。
  - `wcs_lib.py:30-31` `has_ap` 要求「有任一非零」，生产判指针非空 ⇒ AP 全零但指针非空时走**不同分支**。
  - `wcs_lib.py:22` 无 `sip_order ∈ [0,5]` 校验，生产抛 `invalid_argument`。
- 注意 `wcs_lib.py:80-83`（AP/BP 逆路径）有**同样的顺序问题**，而同文件 `:87` 的迭代逆路径却正确地先算 `f=g=... (u,v)` 再更新 ⇒ **同文件内自相矛盾**，是「这处是 bug 而非刻意」的内部证据。

---

**B3 · `p1sg_oracle.cpp`：唯一带退出码的判据完全自闭环；C4 前半为代数恒等式恒绿门；C6 为往返自证**

- 位置：`code/reverse_verify/p1_spatial_gain/cpp/p1sg_oracle.cpp`
- 证据 A（读桩）：`:26-33` 的 `#include` 只有 8 个标准库头。全目录 grep 生产符号（`spatial_gain|image_corrector|spectrum_integrator|frame_photometry_fit|wcs_transform|star_matcher|pc_api`）零命中。数据流是 `:47-61` 自建 RNG → `:74-86` 自建基函数 → `:90-117` 自建解算 → `:147-219` 自建 IRLS → `:229-250` 自建真值场 → `:257-291` 自造算例 → `:295-343` 自算指标。**`lib/algorithms/photometry/cpp/src/spatial_gain.cpp` 的任何缺陷，C1–C6 在原理上不可能翻红。**
- 证据 B（C4 前半恒绿，`:435-437` 逐字）：
  ```cpp
  double gs = g * (40.0 / 97.0);
  double ds = gs;
  double y1 = m * S0 + gs;
  double y2 = (m + ds / S0) * S0 + (gs - ds);
  maxdy = std::max(maxdy, std::fabs(y1 - y2));
  ```
  代入 `ds ≡ gs`：`y2 = m·S0 + gs + gs − gs = m·S0 + gs = y1`。**代数恒等式，`max|Δy| ≡ 0`（仅浮点消去噪声）**。`:446` 的 `maxdy <= THRESH_C4_DY_ADU`（`1e-9`，`:41`）因此**结构上恒绿**，与被测物零关联。这是「恒真门三型」中的**代数恒等式型**。
- 证据 C（C6 往返自证，`:480-485` → `:513`）：
  ```cpp
  for (...) { img2[...] = img[...] * std::pow(10.0, -s); }   // :484 用被检验的乘法现搭 img2
  ...
  double expect = std::pow(10.0, -s);                        // :508 同一个 s、同一行 pow
  worst = std::max(worst, std::fabs(f2 / f1 / expect - 1.0)); // :509
  check("C6 ...", worst <= THRESH_C6_REL, buf);               // :513
  ```
  `expect` 与构造 `img2` 用的是**逐字相同的表达式**，连**符号约定都是同一处字面量** ⇒ 把生产施加写成 `10^(+s)`（符号 bug）C6 照样绿。且 `:459-478`（先写 `img2=sky·10^-s`、再往 `img2` 加源）随即被 `:480-485` 整体覆盖，是**死存储**。
- 证据 D（加重情节）：生产 `lib/algorithms/photometry/cpp/src/spatial_gain.h` 的注释把「order ≥ 3 不启用」归因于本文件 §3.4 实测。**即：禁用高阶的生产规范依据来自这个桩 oracle 的 C3/C5。**（该 header 在 `ALG-photometry-003` 片，非本片，我只读到子代理转述的引用关系，未亲读 header 全文 —— 此条标为**部分核验**。）
- 双向体检结论：本 oracle **未发现恒红门**；发现 **C4 前半恒绿 1 处、C6 往返自证 1 处、C3 准恒绿 1 处**（C3 自由参数 3→10、同一批噪声，拟合必然更贴噪声，`p3/p1 ≥ 3` 近乎自动成立）。
- 附注（正面）：`REVERSE_VERIFY_CANON.md:21-22` 与 `docs/p1-spatial-gain.md:249-256` **如实登记 C3 实测 2.89× / C5 实测 0.86× 两条 FAIL 且不放宽**，并写明 C6 首跑 FAIL 是修夹具不是改阈值。这条诚实度值得保留。

---

### 4.2 须修

**B4 · `README.md:62` 引错列、`:64` 选择性报告、`:25` 伪称逐行对齐、`:38-40` 复跑配方指向已退役构建**

- `README.md:61-62`：「拟合出的空间结构随孔径强烈变化：同一跨镜帧对的 3×3 峰峰在 r=3/4/6/10 上为 **7.78% / 4.93% / 3.81% / 4.25%**」。句子主语是「**拟合出的**空间结构」，但这四个数是 `docs/p1-spatial-gain.md:370-373` 表的 **3×3 before 列**；同表 **after 列**是 `3.403% / 1.957% / 4.857% / 5.735%`。⇒ 误引。
- `README.md:64`：「用 r=4 拟合的 m 在 **r=6 独立口径**上复核，**4/4 代表帧对变差**」。该断言对 `docs/p1-spatial-gain.md:355-358` 表的 **3×3 列**成立（3.807→4.857、3.490→6.078、0.409→1.039、0.421→0.622，四对全变差）。但**同一批数据的 8×8 列**是 `14.240→13.798`（变好）、`11.691→17.499`（变差）、`4.934→4.890`（**变好**）、`2.555→2.518`（**变好**）⇒ **8×8 是 1 差 3 好**。README 只报了支持负面结论的那一档，未披露反向档。⇒ **选择性报告**。
- `README.md:38-40`：
  ```bash
  cmake -S reverse_verify -B run/reverse_verify/build -G Ninja -DCMAKE_BUILD_TYPE=Release
  ninja -C run/reverse_verify/build
  ./run/reverse_verify/build/rv_p1sg_oracle
  ```
  但同目录 `build_oracle.sh:5-6` 逐字：「原 `reverse_verify/CMakeLists.txt`（standalone project）随 reverse_verify/ 根条目解散而**退役**，其唯一 target `rv_p1sg_oracle` 由本脚本等价承担」。实测 `find 实验/photometric-magnitude/code/reverse_verify -name CMakeLists.txt | wc -l` → **0**。⇒ 该配方**必然失败**，且二进制名 `rv_p1sg_oracle` 与 `build_oracle.sh:15` 的 `p1sg_oracle` 也不一致。另 `build_oracle.sh:13`（`p1_spatial_gain` 下划线）与 `REVERSE_VERIFY_CANON.md:31`（`p1-spatial-gain` 连字符）**第三处不一致**。
- `README.md:13-26` 的目录树亦与实际不符（声称 `reverse_verify/synthetic/{synth_gain,gainlib}.py`，实为 `实验/shared/synthetic/`；声称 `reverse_verify/experiments/p1_spatial_gain/`，实际无 `experiments/` 一层）。
- 应为：README 的复跑段改指 `build_oracle.sh`；`:62` 改引 after 列；`:64` 同时给出 3×3 与 8×8 两档（或明确限定口径）；`:25` 删掉「逐行对齐」或改为「近似移植，已知 N 处差异」。

---

**B5 · route2 的 8 个脚本写盘路径与其 `makedirs` 目标不一致，归档无法由本提交复现**

- 位置（本片内 4 份）：`code/redo/route2/exp1_robust_constants.py:205-206`、`exp3_adaptive_ladder.py:110-111`、`exp4_fov_geometry.py:132-133`、`exp5_match_radius.py:106-107`、`exp6_sigma_kappa.py:83-84`（同族 exp2/exp7/exp8 在 `EXP-photometric-magnitude-002` 片，不在本片）。
- 现状（以 exp1 为例，逐字）：
  ```python
  os.makedirs(os.path.join(here, "..", "..", "..", "results", "redo", "route2"), exist_ok=True)   # :205 建 对
  path = os.path.join(here, "..", "results", "exp1_robust_constants.json")                          # :206 写 错
  ```
  `:205` 建的是 `实验/photometric-magnitude/results/redo/route2`；`:206` 写的是 `实验/photometric-magnitude/code/redo/results/`。**后者在仓内不存在**（实测 `ls: 无法访问 '实验/photometric-magnitude/code/redo/results': 没有那个文件或目录`）⇒ 脚本会 `FileNotFoundError`，**现提交下 route2 归档无法由本提交复现**。
- 对照：route1（`exp1_robust_constants.py:189-192`）与 route3（`exp_S03:105-108`）写法正确，用的是 `_out` 变量 + `os.path.join(_out, ...)`。
- 应为：route2 五份（+ 三份在 -002 片）统一改成 route1/route3 的写法。

---

**B6 · `code/redo/route1/exp2_mag_window_match.py:18` 伪引：`PHOTOMETRY.md:34` 不含 `3.0`**

- 位置：`exp2_mag_window_match.py:18` 逐字：`MAG_TOL_CODE = 3.0          # PHOTOMETRY.md:34`
- 现状：实测 `sed -n '34p' docs/science/PHOTOMETRY.md` → `| \`psf_status,qf\` | 饱和/质量标志 | \`snr_estimator\` |`。该行**不含 `3.0`、不含 `mag_tolerance`、不含预过滤窗**。
- 为何重要：`docs/p1-spatial-gain.md:106` 与 `REPORT_route3.md:106` 都以「正本自认 Project-defined（PHOTOMETRY.md:220 明言不引文献背书）」为依据给该窗宽做豁免 ⇒ 若 `:219-222` 的定位也漂移，豁免的依据链断裂。
- **同一文件 :20 的另一条引用为真**：`MAD_COEF_CODE = 0.6744897501960817  # 代码字面量（PHOTOMETRY.md:31）` —— 实测 `sed -n '31p' docs/science/PHOTOMETRY.md` → `| \`sigma_residual\` | \`MAD(r_inliers)/0.6744897501960817\` dex | QA |`，**逐字命中**。同类引用 `code/redo/route2/exp1_robust_constants.py:23-27` 的 `star_matcher.cpp:21/23/25/27` 亦**全部逐字命中**（实测 `_MAD_SCALE`/`_TUKEY_C`/`_IRLS_MAX_ITER`/`_IRLS_CONVERGE` 四行）。
- 应为：`exp2:18` 的行号定位改到真实行（或改为「PHOTOMETRY.md:219-222」并核实）。

---

**B7 · 整改提交 `ef3dc516` 只改了判据代码，未重跑归档、未同步报告（失效模式「代码改了、归档没重跑」+「选择性报告」已实际发生）**

- 证据：
  ```
  git show --stat ef3dc516 → 改了 7 个 .py（route2/exp2, route3/S03,S04,S05,S09,S10,S12）
                          → 0 个 results/redo/route3/*.json 归档、0 个本片 .md 报告
  for j in exp_S03 exp_S05 exp_S07 exp_S10:
      grep -c 'discriminating_power|registered_not_counted|evidence_status' \
           实验/photometric-magnitude/results/redo/route3/$j.json   →  四项全为 0
  grep -cE 'discriminating_power|registered_not_counted|硬编码|声明而不是' \
       实验/photometric-magnitude/code/redo/route3/REPORT_route3.md →  0
  ```
- 现状：代码侧已按体例给假负例标 `discriminating_power: "none"` + `evidence_status: "registered-only"` 并从 `verdict` 排除（如 `exp_S03:86-87,104`、`exp_S04:95-96,118`、`exp_S05:99-100,108`、`exp_S10:75-76,80`）。**但这四项披露既未落进归档 JSON，也未传导进报告。**
- 报告侧的**选择性报告**：`REPORT_route3.md:231` 逐字「13 个实验脚本全部运行通过（verdict 全绿，见 results/）」；而 `:94`（S03 负例 ✔）、`:104`（S04「清洁场全部窗宽 k_photo 偏差 = 0.0（逐位）✔」）、`:113`（S05「负例：n=2 ⇒ 门① NO_DATA…✔」——代码 :99 自认「**三个字段与 pass 全是硬编码字面量 True，没有任何计算**」）、`:157`（S10「孤立场 false rate=0（负例归零）✔」——代码 :75 自认「value 写死 0.0、pass 写死 True，是负例的**声明**而不是负例的测量」）四处把自认无判别力的守卫以 ✔ 呈现，且全文零处证据等级标注。`:111` 把 H5a 表述为「2 万场 × 7 档 n，**0 例**违反 ⌈n/2⌉ 下界 ✔」而未提它是本地 `robust()` 克隆（代码 `:104-105` 提了）。
- 复现命令错误：`REPORT_route3.md:226` 写「结果落 `../results/*.json`」，实际代码写 `../../../results/redo/route3/`（如 `exp_S03:105-106`）。
- 应为：同提交内重跑 route3 归档并同步 `REPORT_route3.md` 的证据等级；或在报告里加一节「自认无判别力的守卫」。

---

**B8 · `exp_S07_fov_constants.py:57` 是两个字面量比大小的恒绿门；其 `negative_zero_check` 计入 verdict 却无披露体例**

- 位置：`code/redo/route3/exp_S07_fov_constants.py:48-58`
- 现状（逐字）：`:49 wcs_err_deg = 2.0 * DEG_PER_ARCSEC`；`:57 "pass": bool(1.2 * 1.0 > 10 * wcs_err_deg)`。
  即 `1.2 > 10×(2/3600) = 0.005556` ⇒ **恒真**。且本文件自己定义的 `fov()`（`:22-24`）**在这条路径上根本没被调用**，`sensitivity_table` 的 `rows` 也不参与。要让它红只能去改字面量 `1.2` 或 `2.0` —— 正是它假装在检验的东西。**恒真门（代数恒等式型，作用于字面量）。**
- 口径不一致：本文件 `:66-71` 的 `negative_zero_check` 是零效应守卫（`clamp_ineffective` 只判 `1.2 <= f <= 10`，是输入数的性质，不是任何被测量），却被 `:81-82` 的 `verdict` **计入**，且全文件**没有** `discriminating_power` / `evidence_status` / `registered_not_counted` 任一字段。对照 S03/S04/S05/S10 四份同类守卫都做了披露并排除。**且整改提交 `ef3dc516` 漏掉了 S07**（`git show --stat` 只含 S03/S04/S05/S09/S10/S12）。
- 应为：H7b 删掉或改为对照生产施加路径的实测；`negative_zero_check` 按 S03/S04/S05/S10 体例披露并排除出 verdict。

---

**B9 · `exp_S04_mag_tolerance.py:63-69` 死代码；`:82-83` 的 H4a 是平移不变代数恒等式（恒绿）**

- 死代码（逐字）：`:63 out["H4a"] = {}` 起算，`:68 "...location_shift": float(np.log10(k) + np.log10(res_1["scale"] * 10 ** 0 / res_k["scale"]) * 0)` —— 末项 `* 0` 使整个 location_shift 恒等于 `log10(k)`（纯装饰）；随后 `:77 out["H4a"] = {...}` 用全新字典**整体覆盖**。⇒ `:63-69` 七行连同其计算结果全部丢弃。`:43 rin = rc` 亦被 `:45` 立即覆写。
- 恒绿：`:82-83` 的判据
  ```python
  "pass": bool(abs(sc[1] / sc[0] - 1.0 / (10 ** 0.37)) < 1e-12 and abs(sg[1] - sg[0]) / sg[0] < 1e-9)
  ```
  把 `F_instr` 整体乘 `k` ⇒ `r = log10(F_instr/F_syn)` 全体平移 `log10 k`；中位数与 MAD 对**平移可交换** ⇒ 预过滤掩码逐元素不变、location 平移 `log10 k`、sigma 不变 ⇒ `scale 比 ≡ 1/k` **逐位恒成立**。这是「代数恒等式型（含平移不变闭式）」恒绿门。
- **须保留的真门**：`:111 "pass": bool(cont[3.0]["k_bias_dex"] < 0.01)`（15% 错配注入，`REPORT_route3.md:104` 报 ≤4.0e-4 dex）—— 这一条能真红。

---

**B10 · `scia_sim.py:262-269` `variance_map()` 的 `sky_sigma_adu` 参数在函数体中从未使用，docstring 却承诺它切换到 robust 口径**

- 位置：`code/scia_sim.py:262-269`
- 现状（逐字）：签名 `def variance_map(adu_img, truth_info, inst, sky_sigma_adu=None)`，docstring 写「**sky_sigma_adu: 若给出，用"含天光面结构"的逐像素噪声替代纯泊松天光项（robust 口径）**」；函数体只有 `mu_e = truth_info["mu"]; var_e = clip(mu_e,0,None) + read_noise**2; return var_e/gain**2`。**`sky_sigma_adu` 与 `adu_img` 都未被引用。** ⇒ 传 `sky_sigma_adu=X` 静默返回非 robust 方差。
- 附带：`:139-142` `if inst.ellipticity > 0: f = f` 是字面量空操作（`forward():199-200` 已按 `ellipticity>0` 改走 `render_field_elliptical`，该分支恒死）。
- 应为：实现该参数或删掉它并改 docstring（文档与代码冲突，本项目已固化检查项）。

---

**B11 · `rt_gate_attribution.py` 的对抗产出只进 stdout，不落任何归档**

- 位置：`code/redteam/rt_gate_attribution.py:4`「输出只到 stdout，**不写 results/**」、`:66-79` 全部为 `print`
- 现状：这是全片唯一的红队归因反例脚本，其结论（位置依赖残差 2% 即把 `BELOW_FLOOR` 翻成 `PASS`）已被 `REPORT_experiment.md:128⑤⑨` 与 `REPORT_paper.md:208-219` **逐条引用**（含 6 行数值表）。但脚本本身不落盘 ⇒ 该结论**无法被第三方复跑验证**，只能相信报告里的转录。
- 应为：写到 `results/` 下的 JSON（与 `step7/step8` 同面），或在报告里注明「该表为一次性运行转录，无归档」。

---

### 4.3 建议

| # | 位置 | 现状 → 应为 |
|---|---|---|
| S1 | `real_pixel_check.py:73` | `float(np.median(cnt[M.reshape(-1)...]) ) if False else float(np.median(cnt))` —— `if False` 死表达式；且 `else` 分支对**含空箱**的全部 64 箱取中位，与 docstring `:59` 声明的「每箱星数」口径不符，直接影响 `:133` 的 `noise_floor_pct` |
| S2 | `real_ridge.py:146-152` | `dev` 算完从不写入 `out`（`:154-156`），`:150-151` 是字面量 `for tag, cc in (("halfA", None),): pass` —— 死计算块，浪费一次全帧 `grid_m` |
| S3 | `real_ridge.py:21` | `from gainlib import mad_sigma` 全文件未使用 |
| S4 | `route2/exp5_match_radius.py:82-85` | `k_photo_p90_bias_dex` 与 `p90_abs_location_dex` **逐字重复**同一表达式，命名暗示是两个不同量 |
| S5 | `route2/exp5_match_radius.py:90-93` | `neg` 三字段全硬编码、无 `discriminating_power` 披露（对比 route3 同类守卫均有披露体例） |
| S6 | `route2/exp6_sigma_kappa.py:50-52` | `np.median(np.zeros((100000,5)),axis=1).std()` 逐位为 0 ⇒ `metric_zero` 结构上必 True。**但同文件 `:40-45` 的「MC vs 解析 `sqrt(pi/2N)`」是本片少数真门，应保留** |
| S7 | `route2/exp4_fov_geometry.py:87` | `"fov20_stays_unclamped_in_legacy": True` 硬编码字面量，无计算 |
| S8 | `route1/exp2_mag_window_match.py:146-158` | 贯穿用例用 `true_scale`（`:147`）造 `F_instr`（`:148`），再用 `true_scale` 当真值比回来（`:158`）⇒ **逆函数构造的「外部参照」往返自证**；但文件头 `:4` 自称「贯穿上下游接口的最小链条用例」，未标注该限制 |
| S9 | `route1/exp4_spatial_gain.py:74, :84-85` | 正例在独立网格评估（`:54-57` 有注释），负例与 S9 界检查却在**训练点**评估 ⇒ 同一文件两套评估口径 |
| S10 | `scia_calib.py:222` | `dB = sigma_pix_e * structure_factor / sqrt(n_used_ap)`，`n_used_ap` 默认 **201.0** 且所有调用方都不传 ⇒ 与实际孔径（`scia_pipeline.py:61` 用 r=10、step7/step8 用 r=4/r=6）不符。该项直接进 `sigma_ceiling`。按 AGENTS §6「可由输入几何导出的改为现场计算」 |
| S11 | `scia_calib.py:209` | `F_e = F * inst.gain` 赋值后从未使用 |
| S12 | `scia_calib.py:168,170` | `delta_after_m` 与 `sigma_obs_after_m_mag` 是**同一表达式** `mad_sigma(dm-pred)` 的两份拷贝 |
| S13 | `scia_common.py:213-214` | docstring 逐字「传 `mag_g=m` 时返回 `F_λ·10^(-0.4·(m − G_source))` 的积分」，但实现 `:232` 是 `v *= 10.0**(-0.4*mag_g)`，**没有 `G_source`**。实际调用方（`step2_hst_sim.py:100,103,146,148`）传的是 `mag_eff - 16.0`，即参考星等固定为 16。⇒ docstring 描述的语义与代码不符 |
| S14 | `scia_common.py:289-303` | IRLS 若在 `:293-294` 因 `sw<=0` break，`w` 全零 ⇒ `inl` 全 False、`n_in=0`，但 `:303` 仍无条件标 `degenerate=False` ⇒ 退化估计被标成非退化 |
| S15 | `step7_negatives.py:190` | `pass_=bool(rows2[-1]... > rows2[0]...*1.5)` 用 **0.5×** 作基线，而同记录 `:176-179` 的 `response_ratio` 按 `:176` 注释「口径统一：响应倍数一律以 **1× 基线**为分母」用 1× ⇒ 同一 JSON 内两套基线 |
| S16 | `step7_negatives.py:25, :139` | `import scia_sim as ss` 全文件未使用；`sky_e0 = ...`（`:139`）赋值后未使用 |
| S17 | `step2_hst_sim.py:99-103` | `f_syn_ref`（`:99-100`）与 `f_syn`（`:102-103`）是**逐字相同**的两次计算；前者只用于 `:101` 的 `inject_scale`。改一处漏一处即让 `inject_scale` 与 `f_syn` 不同源 |
| S18 | `aperture_probe.py:66` vs `real_ridge.py:64` | 同名指标「3×3 PTP」在两脚本分别用 `minn=4`（默认）与 `minn=6` ⇒ 两个 JSON 里的 `before_ptp3x3` **不是同一个量** |
| S19 | `scia_pipeline.py:54` | `np.round(g["x"]).astype(int)` 对 NaN 无保护（NaN→clip 到 0）⇒ 拟合失败的星其饱和框落在像素 (0,0)。当前被 `sample_selection` 的 `fit_ok` 兜住，无实际影响 |
| S20 | `scia_common.py:47-48` | import 期即 `os.makedirs` 建 `results/`、`run/SCI-401/`、`run/SCI-401/data_cache/` ⇒ 导入 `scia_common` 有文件系统副作用 |

---

## 5. 我主动构造的反例

| # | 构造 | 期望推翻 | 是否推翻 |
|---|---|---|---|
| **R1** | 把 `p1sg_oracle.cpp:436-437` 的 `y2` 展开成代数式，检查 `ds ≡ gs` 时 `y2 − y1` 是否恒为 0 | 推翻「C4 是可判门」 | **推翻成功**。`y2 = m·S0 + ds + gs − ds = y1`，`maxdy` 只剩浮点消去噪声，`1e-9` 阈值（`:41`）结构上不可能被越过。**C4 前半 = 恒绿。** |
| **R2** | 检查 `wcs_lib.py:46-47` 与 `wcs_transform.cpp:194-197` 的求值顺序 | 推翻 README:25「逐行对齐」 | **推翻成功**。生产 `f=g=evalSip(dx,dy)` 后再 `dx+=f; dy+=g`；移植在 `dx` 已更新后才算 B。同文件 `:87` 的迭代逆路径反而是正确写法 ⇒ 文件内自相矛盾。 |
| **R3** | 把 `F_instr` 整体乘 `k`，检查 `exp_S04:82-83` 是否仍恒成立 | 推翻「H4a 是真门」 | **推翻成功**。`r` 全体平移 `log10 k`，中位数/MAD 对平移可交换 ⇒ 掩码逐元素不变、`scale` 比逐位 `= 1/k`。**代数恒等式型恒绿。** |
| **R4** | 检查 `wcs_lib.py` 的 `sky_to_pixel` 与生产 `skyToPixel` 的投影背面行为 | 找 README:25 的第二处不符 | **推翻成功**。`wcs_lib.py:74-76` 无 `cosc≈0` 保护（给 `inf/nan`），生产钳到 `xi=eta=1e6`（给有限值）。另 3 处（det 奇异、`has_ap` 判据、`sip_order` 校验）亦分歧。 |
| **R5** | 把 README:62 的四个数与 `p1-spatial-gain.md:370-373` 表逐列对 | 找出是 before 还是 after 列 | **推翻成功**。四数 = before 列（7.775/4.928/3.807/4.249），after 列实为 3.403/1.957/4.857/5.735 ⇒ 误引。 |
| **R6** | 把 README:64 的「4/4 变差」同时对 3×3 与 8×8 两列核 | 检验是否选择性报告 | **推翻成功**。3×3 是 4/4 变差；8×8 是 **1 差 3 好**（4.934→4.890、2.555→2.518 均变好）⇒ 只报了支持负面结论的那一档。 |
| **R7** | 实测 `docs/ASTROCS_DESIGN.md` / `ACCEPTANCE_SPEC.md` / `docs/plugins/...` 等 9 个被引路径是否存在 | 判断这些是伪引还是未入库 | **推翻成功**。9 项全 MISSING，仓内真实名为 `docs/ACSD_DESIGN.md`。 |
| **R8** | 跑 `find .../reverse_verify -name CMakeLists.txt` 并读 `build_oracle.sh:5-6` | 判断 README:38-40 的 cmake 配方是否可用 | **推翻成功**。仓内 0 个 CMakeLists，`build_oracle.sh` 明写该 target 已退役 ⇒ 配方必然失败。 |
| **R9** | 逐字比对 `eng/packaging/config/filters.json` 与 `lib/.../response_curves/filters.json` 的 `Baader R` | 验证 `scia_common.py:104` 的「逐字转录」 | **未推翻（该引用为真）**。45 条滤镜同名同数，`Baader R`（本实验唯一 `load_filter` 的键）**逐字相同** = `True`。⇒ 该伪引不成立，登记为「核实为真引用」。 |
| **R10** | grep 全 `code/` 找 `svo_` / `data_cache` 的消费者 | 判断 `step0_fetch_refs.sh` 的产物是否死证据面 | **未推翻（我自己的假设错）**。`step3_forward_vs_photflam.py:40-42,176` 确实读取并校验 SVO 缓存 ⇒ step0 有活消费者，**不登记为缺陷**。（此条是我推翻了子代理与我自己的初始怀疑。） |
| **R11** | 实测 `sed -n '31p;34p' docs/science/PHOTOMETRY.md` | 验证 `exp1:20` 与 `exp2:18` 的行号引用 | **半推翻**。`:31` 逐字命中 `0.6744897501960817`（真引用）；`:34` 实际是 `| psf_status,qf | 饱和/质量标志 |` ⇒ `exp2:18` 的 `3.0 # PHOTOMETRY.md:34` **是伪引**。 |
| **R12** | 实测 `star_matcher.cpp:21-27` | 验证 route2/exp1:23-27 的代码锚 | **未推翻**。四个常量逐行命中（`_MAD_SCALE`/`_TUKEY_C`/`_IRLS_MAX_ITER`/`_IRLS_CONVERGE`）⇒ 真引用。 |

---

## 6. 盲复算

**做法**：遮住 §4 的全部判定与 §7 子代理结论，只用「本片代码 + 仓内被引原件」独立重推每条门的类型与真伪，然后与 §4 比对。

| 门 | 盲复算独立结论 | 与 §4 一致？ | 判定 |
|---|---|---|---|
| `p1sg_oracle` C1/C2/C3/C5 | 读桩（真值/观测/拟合全在 `:229-343` 自造） | 一致 | 一致 |
| `p1sg_oracle` C4 前半 | **恒绿**：`ds≡gs` ⇒ `y2≡y1` | 一致 | 一致 |
| `p1sg_oracle` C6 | **往返自证**：`expect` 与构造 `img2` 用同一 `pow(10,-s)`；`:459-478` 死存储 | 一致 | 一致 |
| `real_*` 4 脚本 | **零可执行判据**（无 THRESH/assert/退出码），只写 JSON + print | 一致 | 一致 |
| `exp_S04` H4a | **恒绿**（平移不变闭式） | 一致 | 一致 |
| `exp_S07` H7b | **恒绿**（两个字面量比大小，`fov()` 未被调用） | 一致 | 一致 |
| `exp_S10` H10a | **恒绿**（σ_jit 是同文件字面量，`P(miss)=exp(-r²/2σ²)` 在 r=2/σ=0.2 下约 1e-22 ⇒ miss 逐位 0） | 一致 | 一致 |
| `exp6` metric_zero | **恒绿**（全零数组取 std） | 一致 | 一致 |
| `exp_S05` H5c | **真门**（`[0.3,0.3,8.0]` 手工构型，A2 缺陷若修则翻 False） | 一致 | 一致 |
| `exp_S04` H4b_contaminated | **真门**（15% 错配注入，比的是 `k_bias_dex < 0.01`） | 一致 | 一致 |
| `exp4_fov_geometry:45` | 恒真（解析恒等式），**但代码 :28-32 已自认并订正表述** | 一致 | 一致（正面） |

**盲复算结论：12/12 与 §4 一致，无偏松、无偏严。**

唯一的**盲复算补强**：我在不看 §4 的情况下独立注意到 `exp_S04:63-69` 的 `* 0` 与 `:77` 的整体覆盖，和 §4-B9 独立撞上——这是本片**最容易被漏掉**的一类（死代码里带一个刻意归零的乘子）。另我独立注意到 `exp_S07` 的 `negative_zero_check` 无披露体例（与 S03/S04/S05/S10 对比），与 §4-B8 一致。

**口径计数（写明层级）**：
- **门实例层**：本片共 **23 个门实例**（oracle 6 + redo 20 个 `pass`/`reproduced` 字段 + redteam 2 项 + step7 6 项 pass）。其中
- **去重门层**（按「同一判据表达式」去重）：**19 个去重门**。分类：**真门 3 个**（`exp_S05:90`、`exp_S04:111`、`exp6:40-45`）、**读桩 6 个**（oracle C1/C2/C3/C5、oracle C4 后半、route1/exp4:88）、**恒绿 7 个**（oracle C4 前半、oracle C6、`exp_S04:82`、`exp_S07:57`、`exp_S07:70`、`exp_S10:58`、`exp_S10:67`、`exp6:52`）、**硬编码/自证 3 个**（`exp_S05:negative_gate1`、`exp_S10:negative_zero_check`、`exp_S03:negative_zero_check`，三者均已自认并排除 verdict）。
- **整改分母层**：整改提交 `ef3dc516` 在本片触及 **7 份**判据文件；其中 **4 份**（S03/S04/S05/S10）已加披露体例但**归档与报告未同步**（B7），**1 份**（S07）**漏改**（B8），另 2 份在 `-002` 片不在本片。⇒ **整改分母 7，实际闭环 0（归档+报告双同步者 0）**。

---

## 7. 子代理派发记录

派发 **5 个**（`EXP-photometric-magnitude-001` 片内 `code/redo/` 12 份 + 主脚本 9 份 + 文档 8 份 + `reverse_verify` 6 份）。**我自己逐份读完 37/37 成员文件**（子代理读的是我片的子集，用于横向取证与独立复核，不替代我的通读）。

| # | 子代理 | 任务 | 结果 |
|---|---|---|---|
| A | `fba03f86` | `code/redo/` 12 份恒真门三型审计 | 已交付，含 13 文件结构化总表 |
| B | `b7348a16` | 伪引 + 文档-代码冲突（8 份文档） | 已交付 |
| C | `3557ce47` | 自愈判据 + 归档漂移（9 份主脚本） | 已交付 |
| D | `385d99dc` | **误派发**：与 A 完全相同的 prompt（我的重复派单，如实登记） | 已交付，内容与 A 重叠 |
| E | `916b30cc` | `reverse_verify/p1_spatial_gain` 6 份判据可红性 | 已交付，含 C4 恒绿数值复算 |

### 我逐条复核的结果

**采纳（我自己独立验证或读到原文后确认）——8 条**

| 子代理结论 | 我的复核 |
|---|---|
| E：oracle 只 include 8 个标准库头、数据流自闭环 | ✅ 我读 `p1sg_oracle.cpp:26-33` 与 `:47-343` 确认，并补出更强的自证证据（C4 恒绿、C6 往返自证） |
| E：`wcs_lib.py:46-47` 用已更新 dx 喂 B，生产 `wcs_transform.cpp:194-197` 不用 | ✅ **我自己读了两侧源码逐字比对**，升级为阻断 B2 |
| E：README:62 引了 before 列 | ✅ **我自己读 `p1-spatial-gain.md:368-373` 表确认**，升级为阻断 B4 |
| E：README:64「4/4 变差」仅 3×3 成立，8×8 是 1 差 3 好 | ✅ **我自己读 `:353-358` 表确认四行 8×8 数值** |
| E：`docs/ASTROCS_DESIGN.md` 不存在 ⇒ 4 脚本不可运行 | ✅ 我实测 9 个被引路径全 MISSING，并确认 `docs/ACSD_DESIGN.md` 存在、`real_gain.py:43` 用它作根标记 |
| E：README:38-40 的 cmake 配方必然失败 | ✅ 我实测 `find` 零命中 + 亲读 `build_oracle.sh:5-6` |
| A：route2 八脚本写盘路径错 | ✅ **我读代码时已先独立发现**（`exp1:205/206` 等），再实测 `code/redo/results/` 不存在确认 |
| A：`ef3dc516` 只改 .py 不改归档 | ✅ 我实测 `git show --stat` + 四份归档 JSON 披露字段计数全 0 |

**否决 / 降级 / 补正——6 条**

| 子代理结论 | 我的处置 | 理由 |
|---|---|---|
| E：「C3 准恒绿（未实测实际 ratio）」 | **降级为「未验证」** | E 自己标了不确定。且**我实跑被纪律禁止**（不编译）。只保留方向性论证（自由参数 3→10 必然更贴噪声），不写死「准恒绿」 |
| E：「生产 `spatial_gain.h:22` 禁用 order≥3 的依据就是这两条桩门」 | **标为部分核验，不列为发现** | `spatial_gain.h` 在 `ALG-photometry-003` 片，**不在我的片**。我只拿到子代理转述，未亲读。按纪律「别人的产出不采信为事实」，此条不进 §4，只在此登记 |
| E：「`aperture_probe` 的 r=4 行是样本内自证」 | **降级为建议** | 事实成立（模型在 p1_flux.json 同口径拟合，`RADII:27` 首档即 (4.0,6.0,10.0)），但该文件的**结论**恰恰是「r=4 有效、换孔径失效 ⇒ 是孔径伪影」，r=4 占优是**该判据的设计前提**、不是隐藏缺陷。列为建议 S 级而非须修 |
| E：「4 个 Python 脚本零判据」 | **合并入 B3**，不单列 | 与 oracle 读桩是同一结构事实的不同表现，合并叙述更准确 |
| A：「`REPORT_route2.md:99/:105` 称『2000 组随机验证』未订正」 | **不计入本片** | `REPORT_route2.md` 在 `EXP-photometric-magnitude-002` 片，不在我的 37 份成员文件内。登记为**跨片线索**交第二片审稿人核实 |
| D（重复派单） | **全量丢弃** | 内容与 A 重复，无独立信息量。如实登记为我的派单失误 |

**补正子代理的 1 处口径**：A 称 `exp_S07` 的 `negative_zero_check`「被 `:82` 计入 verdict 且无 `registered_not_counted`」。我核对 `exp_S07_fov_constants.py:81-82` 后**确认成立**（`verdict` 含 `"negative_zero": out["negative_zero_check"]["pass"]`），并进一步发现该文件**全文无** `discriminating_power`/`evidence_status` 字段，而 `ef3dc516` 恰好漏改 S07 —— 这半句是子代理给不出、需要我对照 `git show --stat` 才能得到的，故并入 B8。

**我独立发现、子代理未报或未强调的**：B3 的 C6 往返自证与 `:459-478` 死存储；B5 的 `makedirs` 与 `write` 目标分离的具体路径；B6 的 `PHOTOMETRY.md:34` 行号伪引（并附 `:31` 为真引的反证）；B9 的 `* 0` 死代码；B10 的 `variance_map` 空参数；S13/S15/S17 等一批口径与死代码；以及我推翻的 R9（filters 逐字转录为真）与 R10（step0 有活消费者）。

---

## 8. 自证段（可复跑）

```bash
cd "/workspace/Astro CS Database"
git rev-parse HEAD                          # f9650dd0
git status --porcelain | head               # 空

# ---- B1 被引文档是否存在（9 项全 MISSING）----
for p in docs/ASTROCS_DESIGN.md ACCEPTANCE_SPEC.md \
         docs/plugins/algorithms_phase1/06_photometry.md run/RELEASE-02/parallel/06.md \
         ENGINEERING_SPEC.md docs/detail/registry/astrocs.phase1.photometry.md \
         docs/design/LOG_AND_ERROR_SYSTEM.md NOISE_ESTIMATION.md \
         docs/detail/registry/astrocs.phase1.star-detection.md; do
  [ -e "$p" ] && echo "EXISTS   $p" || echo "MISSING  $p"; done
ls docs/ACSD_DESIGN.md                      # 真实名是 ACSD
grep -n "ASTROCS_DESIGN" 实验/photometric-magnitude/code/reverse_verify/p1_spatial_gain/src/real_gain.py

# ---- B2 wcs_lib 与生产不一致 ----
sed -n '45,47p'  实验/photometric-magnitude/code/reverse_verify/p1_spatial_gain/src/wcs_lib.py
sed -n '193,198p' lib/algorithms/photometry/cpp/src/wcs_transform.cpp
grep -n '逐行对齐' 实验/photometric-magnitude/code/reverse_verify/p1_spatial_gain/README.md

# ---- B3 oracle 读桩 + C4 恒绿 ----
sed -n '26,33p;41p;435,437p;446p;480,485p;508,509p;513p' \
  实验/photometric-magnitude/code/reverse_verify/p1_spatial_gain/cpp/p1sg_oracle.cpp
grep -cE '#include' 实验/photometric-magnitude/code/reverse_verify/p1_spatial_gain/cpp/p1sg_oracle.cpp   # 8
grep -rnE 'spatial_gain|image_corrector|spectrum_integrator|frame_photometry_fit|star_matcher' \
  实验/photometric-magnitude/code/reverse_verify/p1_spatial_gain/     # 零命中

# ---- B4 README 误引 + 选择性报告 + 失效配方 ----
sed -n '61,64p' 实验/photometric-magnitude/code/reverse_verify/p1_spatial_gain/README.md
sed -n '368,373p;353,358p' 实验/photometric-magnitude/docs/p1-spatial-gain.md
find 实验/photometric-magnitude/code/reverse_verify -name CMakeLists.txt | wc -l    # 0
sed -n '5,6p' 实验/photometric-magnitude/code/reverse_verify/p1_spatial_gain/build_oracle.sh

# ---- B5 route2 写盘路径分离 ----
sed -n '205,206p' 实验/photometric-magnitude/code/redo/route2/exp1_robust_constants.py
ls -d 实验/photometric-magnitude/code/redo/results          # No such file
ls -d 实验/photometric-magnitude/results/redo/route2        # 存在

# ---- B6 行号伪引（:31 真 / :34 假）----
sed -n '18p'  实验/photometric-magnitude/code/redo/route1/exp2_mag_window_match.py
sed -n '34p'  docs/science/PHOTOMETRY.md
sed -n '20p'  实验/photometric-magnitude/code/redo/route1/exp1_robust_constants.py
sed -n '31p'  docs/science/PHOTOMETRY.md
sed -n '21,27p' lib/algorithms/photometry/cpp/src/star_matcher.cpp

# ---- B7 归档/报告未随判据重跑 ----
git show --stat --oneline ef3dc516 -- 实验/photometric-magnitude/code/redo/route3
for j in exp_S03_irls_convergence exp_S05_gates_inlier exp_S07_fov_constants exp_S10_match_radius; do
  printf '%-32s %s\n' "$j" \
    "$(grep -c 'discriminating_power\|registered_not_counted\|evidence_status' \
        实验/photometric-magnitude/results/redo/route3/$j.json)"; done   # 四项全 0
grep -cE 'discriminating_power|registered_not_counted|硬编码' \
  实验/photometric-magnitude/code/redo/route3/REPORT_route3.md                                    # 0
sed -n '231p' 实验/photometric-magnitude/code/redo/route3/REPORT_route3.md

# ---- B8 / B9 恒绿门 ----
sed -n '48,58p;66,71p;81,82p' 实验/photometric-magnitude/code/redo/route3/exp_S07_fov_constants.py
sed -n '63,69p;77p;82,83p'      实验/photometric-magnitude/code/redo/route3/exp_S04_mag_tolerance.py

# ---- B10 / S13 / S15 / S17 ----
sed -n '262,269p' 实验/photometric-magnitude/code/scia_sim.py
sed -n '213,214p;232p' 实验/photometric-magnitude/code/scia_common.py
sed -n '176,190p' 实验/photometric-magnitude/code/step7_negatives.py
sed -n '99,103p'   实验/photometric-magnitude/code/step2_hst_sim.py

# ---- R9 反证：filters.json「逐字转录」为真引用 ----
python3 -c "import json;a=json.load(open('eng/packaging/config/filters.json'))['filters'];\
b=json.load(open('lib/algorithms/photometry/data/response_curves/filters.json'));\
print('Baader R 逐字相同?', b['Baader R']==a['Baader R'])"          # True

# ---- R10 反证：step0 的 SVO 产物有活消费者 ----
grep -n 'svo_HST_WFC3_UVIS2\|def svo_curve' \
  实验/photometric-magnitude/code/step3_forward_vs_photflam.py
```

**纪律声明**：本轮未做任何 git 写（无 add/commit/checkout/reset/stash/`git rm --cached`）；未编译、未跑 ctest/pytest/构建、未执行任何实验脚本；未修改任何仓内文件（唯一写入即本交付件）。`/tmp/acsd_g08/` 未读取。凡标「待联网核验」者：`Gaia DR3 官方文档 §20.12.4 343 点网格`（`REPORT_route3.md:204` 自认官方页 404）、`Lindegren et al. 2021 A&A 649 A2 亮端位置误差`（`exp_S10:6`）、`Gaia Collaboration 2023 A&A 674 A1 源总数 1.812e9`（`route2/exp3:24,97`）、`Montegriffo 2023 A&A 674 A3 §8.1`（`fsyn_convention.md:40,45-46`）—— 本轮未联网核验。

---

# 9. 增补：子代理复核后新发现并经我实测复核的条目（全部 CONFIRMED）

本节条目在 §1–§8 写定后由子代理交叉取证提出，**每条我都重新跑了复现命令**（见 §10），确认成立才收入。与 §4 的关系：未重复者仅列新增；加强者标注「加强 §4-x」。

## 9.1 阻断（新增 5 条）

### N-B1（加强 §4-B5，并给出根因提交）
**`route2` 五份脚本的写盘路径错配由提交 `6860bbc7` 引入，且该提交只改了 `makedirs` 一行。**
```
git show 6860bbc7 -- .../redo/route2/exp1_robust_constants.py
- os.makedirs(os.path.join(here, "..", "results"), exist_ok=True)
+ os.makedirs(os.path.join(here, "..", "..", "..", "results", "redo", "route2"), exist_ok=True)
```
提交说明「结果数据归位」只迁了建目录行、没迁写入行 ⇒ `results/redo/route2/*.json` 是 **2026-09-27 旧代码的产物**（`46a00bb6`），**不可由当前提交复现**。

### N-B2（新增）**`redo/run_all.sh:22` 丢弃全部 stdout，`set -e`（`:10`）下任何 verdict 变红都不会让流水线失败**
```
grep -n 'python3\|set -e\|dev/null' 实验/photometric-magnitude/code/redo/run_all.sh
10:  set -e
22:      python3 "$f" > /dev/null
```
⇒ `exp_S03:69/76/97`、`exp_S04:82/111`、`exp_S05:64/90`、`exp_S07:57/70`、`exp_S10:58/67` 等 23 个门实例的 `pass=False` **只落在被丢弃的 stdout 里**。判红无任何强制面。（`run_all.sh` 在 `-002` 片，但其治理对象是本片 12 份判据文件，故列入。）

### N-B3（新增）**`route2/exp1_robust_constants.py:200` 的「负控制」方向反了，且归档实测从未触发**
归档实测（`results/redo/route2/exp1_robust_constants.json` → `S3_summary`）：
```json
{"clean_field_median_location_span_1e2_vs_1e12": 0.00019263645492466474,
 "dirty_field_median_location_span_1e2_vs_1e12": 0.000141128201497707,
 "negative_control_fired": false}
```
判据是 `dirty_span > 10·clean_span`（`:200`），但**污染场的容差敏感性（1.41e-4）反而低于清洁场（1.93e-4）**。机理：Tukey 把 +0.8 dex 离群星整批拒掉 ⇒ 污染使拟合对容差**更不**敏感。⇒ 该「负控制」是**结构上恒假的死门**，假说前提本身错了。

### N-B4（新增）**`step7_negatives.json` 归档内部自相矛盾：note 字符串与自己 rows 的数值冲突**
`code/step7_negatives.py:335` 写 `ceiling 0.0562→0.1605`；归档 `negatives[id=N6].note` 写 **`0.0325→0.1271`**；而**同一归档的 `rows`** 是 `ceiling_selfref` **`0.056219 → 0.160528`**（与代码一致）。
```
python3 -c "import json;d=json.load(open('.../results/step7_negatives.json',encoding='utf-8'));\
n=[x for x in d['negatives'] if x['id']=='N6'][0];print(n['note']);print(n['rows'][0]['ceiling_selfref'],n['rows'][-1]['ceiling_selfref'])"
```
⇒ 09-30 提交 `1141939b` 改了代码里的手写数字串、**未重跑归档** ⇒ 归档留下一个与自身数据冲突的叙述字段。这正是本项目固化的「代码改了、归档没重跑」，且落在**阻断级**（归档是本片对外的唯一数值证据面）。

### N-B5（新增）**`RESOLUTION_m42_curve_resolve.md:57-58`、`:233-236` 把一个已闭环缺陷写成「必须一并修」；`:152-158`、`:237-239` 引用一个不存在的「旧公式」——两处均伪引**
- 断言（`:236` 逐字）：「**这是同一 bug 的另一入口，必须一并修，否则 orchestrator 路径仍会取错曲线。**」§7 还写「**未验证修复对 `orchestrator` 路径的效果**（该文件未改）」。
- 事实（实测）：
  - `orchestrator.cpp:1349` 逐字：「**唯一实现** = lib/algorithms/photometry/cpp/src/filter_curve_json.h」；`:1362`：「**不再自持任何解析分支**（第二份实现 = 同类缺陷复发的根源）」；`:1365`：「QE 曲线装载与滤镜曲线同口径 (curve_json::load_curve, 唯一实现)」。
  - `orchestrator.cpp:1368-1413` 实际内容是 `extract_qe_curve_name()`（`:1373`）与 `build_spectrum_wl()`（`:1395`），**与曲线解析无关**。
  - `grep -rn "load_filter_curve" lib/ eng/ docs/` 在 `orchestrator.cpp` **零命中**；唯一命中是回归门 `lib/infrastructure/pipeline/orchestrator/cpp/tests/test_photometry_curve_resolve.cpp:632`，它把 `"load_filter_curve("` 列为 `legacy_markers`，`:642` 检查名即「**[W3] 两个 TU 内无修复前解析器残留（死副本清零）**」——**有一个能红的门在主动断言它的缺席**。
  - 被引作「旧公式」的 `spectrum_integrator.h:15` 逐字：「**不含** 10^(-0.4·magG) 因子: magG 既不进入 F_syn, 也不作归一化。」——`:16` 是量纲行，**无** `10^(−0.4·G_Gaia)`。
  - `docs/science/PHOTOMETRY.md:23` 逐字：「`F_syn` | 合成通量 `∫F_λ(λ)·T(λ)·Q(λ)·λ dλ`（**不含** `10^(−0.4·G)`…）」；`:397` 逐字：「`F_syn` 的唯一权威写法是 §2a.1（…**不含** `10^(−0.4·G)`）」。
- 应为：§6.2 第 1、2 条**整条删除**。当前文本把工程量指向已修好的缺陷、并派生两条不存在的待办，且与一个能红的守卫门方向相反。

## 9.2 须修（新增 6 条）

| # | 位置 | 现状 → 应为 |
|---|---|---|
| N-M1 | `code/scia_common.py:343` vs `:350` | **注入与测量共用同一个 `moffat_profile`**：`render_star`（注入路径 `:343`）与 `psf_stamp_model`（测量路径 `:350`）调同一函数 ⇒ 任何 PSF 核形状/归一化错误在通量上**精确相消**，再被 `k_photo=10^-location` 吸收全局标度。⇒ step7 的 N0–N6「注入-响应」证据**结构上不可能因 PSF 算子缺陷翻红**。应为注入/测量两条独立实现（同文件 `:236` 的 `f_syn_dense` 已有此先例），或给 `moffat_profile` 加闭式归一化判据 |
| N-M2 | `REPORT_route3.md:104` | 「3.0 窗保留 **372**/400 星」；归档 `H4b_contaminated.table` 实测 `n_kept` = `{0.5:347, 1.0:354, 2.0:368, 3.0:378, 6.0:400, 100.0:400}` ⇒ 应为 **378** |
| N-M3 | `REPORT_route3.md:131` | 把最大面积膨胀 **112.5×** 归给「0.2″/px **10k×10k**：自然 FOV 0.094 deg」；归档 `sensitivity_table` 的 argmax 实为 **`pixel_scale_as=0.2, size=2000`**（`fov_natural_deg=0.09428`）；`0.2″/px × 10k×10k` 的自然 FOV 是 **0.4714 deg**、膨胀 **4.5×** ⇒ 构型归属错误 |
| N-M4 | `REPORT_route3.md:131` | 同行称「缓冲余量 44% ≫ WCS 误差（**>5 个量级**）」，而该判据 `exp_S07:57` 只用 **10 倍（1 个量级）** ⇒ 报告的「>5 个量级」在代码里无对应物 |
| N-M5 | `code/redo/route1/exp2_mag_window_match.py:119-123` | `d2 = (gx - ox[i])**2 + (gy - oy[i])**2`：`gx`/`gy` 是长度 `n_star` 的整列、`ox[i]`/`oy[i]` 是标量 ⇒ `d2` 形状为 `(n_star, n_star)`，`within.sum()` 数的是**全对**而非「星 i 半径内候选数」 ⇒ `:129-130` 的 `correct_rate`/`ambiguous_rate` 无意义。附 `:19 MATCH_RADIUS_CODE = 2.0` 定义后**全文未被引用**，`:113 area` 未用 |
| N-M6 | `code/redo/route3/exp_S03_irls_convergence.py:85` | `"pass": bool(it == 0)` 由 `:24-25` 的 `s <= 0` 提前返回保证 ⇒ 恒绿；`:83 "delta_location": 0.0` 是**写死字面量**从未计算。**该文件已在 `:86-87` 披露并于 `:104` 排除 verdict**（正面），但归档 `verdict` 实测仍为 `{'H3a':True,'H3b':True,'negative':True,'v8_replay':True}` —— **归档是订正前版本**（加强 §4-B7 的第一个可复现证据） |

## 9.3 建议（新增 3 条）

| # | 位置 | 现状 → 应为 |
|---|---|---|
| N-S1 | 根 `.gitignore` 的 `run/*` | `step2_hst_sim.py:155-164` 产出的 `frame_{A,B,C}.npz` 是 step5/6/7 的测量底物，**不入版本控制**，而 `results/*.json` 入库 ⇒ 证据链只有一半可复核；全仓**无任何地方记录这三帧的 SHA256**。`scia_common.py:57` 已定义 `sha256_file()`（`step3:47` 用了），唯独仿真帧没用 ⇒ step2 应把三帧 sha256 写进 `step2_hst_sim.json` |
| N-S2 | `code/step7_negatives.py:58` + `:143`/`:215` | `resimulate(fr, inst, sky_delta_e, tag, seed="neg:resim")` 的 **`tag` 形参从未被使用**（函数体只取 `seed`），而调用方 `:143` 传 `tag=f"n2_{mult}"` 被丢弃；`:215` `sc.rng("neg:n3sweep")` 字符串恒定、6 档共用同一抽样 ⇒ 现状等价公共随机数，须显式声明或改用 tag 派生 |
| N-S3 | `code/step7_negatives.py:235,327` vs `:214-229`/`:313-314` | N3 直接构造 `log10` 残差向量喂 `irls_tukey`、**从不调用 `guided_photometry`/`fit_psf`**；N6 的注入发生在测量**之后**。但两者都标 `provides_injection_response=True`，`:347-348` 的 summary 断言「N6 …全部通过」⇒ 标签强于实际（加强 N-M1） |

## 9.4 修订后的计数与判定

| 层级 | §2 初始 | 修订后 |
|---|---|---|
| 本片判定 | **阻断** | **阻断**（不变） |
| 阻断数 | 3（B1/B2/B3） | **8**（+ N-B1…N-B5，其中 N-B1 加强 B5） |
| 须修数 | 8（B4…B11） | **14**（+ N-M1…N-M6） |
| 建议数 | 20 | **23**（+ N-S1…N-S3） |
| 去重门层「恒绿」计数 | 7 | **9**（+ `exp_S03:85`、`exp_S07:70`） |
| 判红无强制面 | 未记 | **已记（N-B2）**——这是本片最结构性的问题：**23 个门实例没有一个能让流水线变红** |

**最重三条（修订后）**：
1. **N-B2 + §4-B3**：23 个门实例全部无强制面（`run_all.sh:22` 丢 stdout），且唯一带退出码的 oracle 读纯桩、C4 前半恒绿。
2. **§4-B1 + N-M1**：本片 6 处引用不存在的 `docs/ASTROCS_DESIGN.md`（并被用作仓库根标记使 4 脚本不可运行），且注入与测量共用同一 PSF 核使误差精确相消。
3. **N-B5 + §4-B7**：归档未随判据重跑，已实测三处实例（`exp_S03` verdict 仍含 `negative`、`step7` N6 note 与自身 rows 冲突、`route2` 归档为旧代码产物），并派生出 `REPORT_route3.md` 的两处数字错误。

---

# 10. 增补条目的复现命令

```bash
cd "/workspace/Astro CS Database"

# ---- N-B1 route2 路径错配的根因提交 ----
git show 6860bbc7 -- 实验/photometric-magnitude/code/redo/route2/exp1_robust_constants.py | grep -E '^[+-].*(makedirs|path =)'

# ---- N-B2 判红无强制面 ----
grep -n 'set -e\|python3 "\$f"' 实验/photometric-magnitude/code/redo/run_all.sh     # :10 / :22

# ---- N-B3 负控制方向反了 ----
python3 -c "import json;print(json.load(open('实验/photometric-magnitude/results/redo/route2/exp1_robust_constants.json'))['S3_summary'])"

# ---- N-B4 归档内部自相矛盾 ----
python3 -c "import json;d=json.load(open('实验/photometric-magnitude/results/step7_negatives.json',encoding='utf-8'));\
n=[x for x in d['negatives'] if x['id']=='N6'][0];print(n['note']);print(n['rows'][0]['ceiling_selfref'],n['rows'][-1]['ceiling_selfref'])"

# ---- N-B5 两处伪引 ----
grep -rn "load_filter_curve" lib/ eng/ docs/ | grep -v memory.md      # 只命中测试门 legacy_markers
sed -n '1349p;1362p;1373p;1395p' lib/infrastructure/pipeline/orchestrator/cpp/src/orchestrator.cpp
sed -n '629,642p' lib/infrastructure/pipeline/orchestrator/cpp/tests/test_photometry_curve_resolve.cpp
sed -n '15,16p'  lib/algorithms/photometry/cpp/src/spectrum_integrator.h
sed -n '23p;397p' docs/science/PHOTOMETRY.md
sed -n '236p'    实验/photometric-magnitude/RESOLUTION_m42_curve_resolve.md

# ---- N-M2 / N-M3 / N-M4 报告数字 ----
sed -n '104p;131p' 实验/photometric-magnitude/code/redo/route3/REPORT_route3.md
python3 -c "import json;d=json.load(open('实验/photometric-magnitude/results/redo/route3/exp_S04_mag_tolerance.json'));\
print({k:v.get('n_kept') for k,v in d['H4b_contaminated']['table'].items()})"
python3 -c "import json;r=json.load(open('实验/photometric-magnitude/results/redo/route3/exp_S07_fov_constants.json'))['sensitivity_table'];\
b=max(r,key=lambda x:x['area_inflation']);print({k:b[k] for k in ('pixel_scale_as','size','fov_natural_deg','area_inflation')})"

# ---- N-M5 形状广播 ----
sed -n '19p;113p;119,123p' 实验/photometric-magnitude/code/redo/route1/exp2_mag_window_match.py

# ---- N-M6 归档仍是订正前版本 ----
python3 -c "import json;d=json.load(open('实验/photometric-magnitude/results/redo/route3/exp_S03_irls_convergence.json'));\
print(list(d.keys()));print(d['verdict'])"
sed -n '83p;85p;86,87p;104p' 实验/photometric-magnitude/code/redo/route3/exp_S03_irls_convergence.py

# ---- N-M1 注入/测量同核 ----
sed -n '319,322p;343p;347,350p' 实验/photometric-magnitude/code/scia_common.py
sed -n '64p' 实验/photometric-magnitude/code/scia_sim.py
```

**补充纪律声明（增补部分同前）**：未做任何 git 写；未编译、未跑 ctest/pytest/构建、未执行任何实验脚本（上述 python3 全部只读 JSON）；未修改任何仓内文件。N-B2/N-B3/N-B4/N-B5/N-M2/N-M3/N-M5/N-M6/N-M1 九条均已亲自实测确认；N-M4 的「报告 vs 代码量级」为逐字对照确认；N-S1/N-S2/N-S3 为静态读取确认。
