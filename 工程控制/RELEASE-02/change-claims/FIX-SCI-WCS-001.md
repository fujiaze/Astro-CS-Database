# 变更 claim：FIX-SCI-WCS-001 — ASTROMETRY §5a 像素原点口径订正（标签错误）

- 控制包：RELEASE-02 / 任务 FIX-SCI（负责人已批准两项 P0 冻结科学文档订正）
- 变更对象：`docs/science/ASTROMETRY.md`（SCI-WCS-001，FROZEN）§5a、§14a、文件头变更注记
- 关联登记：finding WCS-003-F1（`run/RELEASE-01/science/SCI-S2-topics.md` §2 T3:62 / §3-2:179）；M1a-C-003（`reports/PROJECT-GOVERNANCE-01/root-scan/shards/C_DOC_CODE_GAP_P0.psv:4`）；`工程控制/RELEASE-01/GAP_AUDIT.md:109`；STD-F1 / 前台裁决 R-02 方案 b
- 日期：2026-09-18
- 依据条款：ENGINEERING_SPEC §3；AGENTS §8

## 1 问题描述（结论先行）

`ASTROMETRY.md` §5a 旧文把求解器口径描述为"**0-based 自洽约定**，`u = x − CRPIX`，参考像素在 0-based `x = CRPIX`，与标准口径相差**常量 1px 原点平移**，且该差异**不是数学内容差**"，并称导出边界 `+1` 桥接用于抵消它。

**该叙述是标签错误。** 求解器拟合自变量不是 0-based 整数数组下标，而是 **sdet 半整数像素中心 `det_x = i + 0.5`**。因此：

```
u = det_x − w/2 = (i+0.5) − w/2 = (i+1) − (w/2+0.5) = p − CRPIX = Paper I 的 q
```

求解器内部 `u` **就是** Paper I §2.1.1 的 `q`；迭代反演 `out.x = u + CRPIX = p` 返回的是 **1-based FITS 像素**，不是 0-based。**实现无 1px（也无 0.5px）原点错误。** 真正 0-based 的量只有 p1_sources 的整数数组下标与 p3 产品网格下标，由 astropy `origin=0` 语义 / `fits_pixel_1based` 各施加一次「下标→FITS」换算——与求解器内部口径无关。

旧文的错误有两层：(i) 把已是 1-based 的求解器输出标为"0-based"（`ipv_wcs.h:43,57-60,70-71` 同错）；(ii) 由该错标推出"存在 1px 内容差、须桥接抵消"的结论。**订正为 doc/comment-only，不改任何数值/容差/代码路径。**

## 2 证据

### 2.1 一手标准（FITS WCS Paper I）

- **Greisen, E. W. & Calabretta, M. R. 2002, A&A 395, 1061**（DOI `10.1051/0004-6361:20021326`；arXiv:astro-ph/0207407 §2.1.1 式(1)、§2.1.4 逐字核验）：`q_i = Σ_j m_ij (p_j − r_j)`，`r_j = CRPIX_j`；`p_j` 为 **1-based**，整数像素号=像素中心（首像素跨 0.5→1.5）；参考像素（`q=0`，world==CRVAL）在 `p = CRPIX`。

### 2.2 本仓实现（文件:行）

- sdet 半整数中心：`lib/algorithms/star_detection/src/sdet_api.cpp:546-549` `sp.dx = x + 0.5 − cx`（合成模型 `sdet_fp64_test.cpp:29-37` 同式）。
- 求解器 U 构造：`lib/algorithms/platesolve/cpp/ipv/src/ipv_select.cpp:943,947` `cx = img_w/2.0`；`U.x = det_x[idx] − cx`。
- CRPIX：`lib/algorithms/platesolve/cpp/ipv/src/ipv_wcs.cpp:158-162` `crpix = cx + 0.5`（注释"1-based FITS, frozen value"）。
- 迭代反演输出：`lib/algorithms/platesolve/cpp/ipv/src/ipv_wcs.cpp:942-946` `out.x = u + wcs.crpix[0]`（= `p`，1-based）。
- 旧标签错误处：`lib/algorithms/platesolve/cpp/ipv/include/ipv_wcs.h:43,57-60,70-71` 把 `x=u+CRPIX` 标为"0-based FITS 像素"。
- 真正 0-based→FITS 的单次换算：`lib/algorithms/projection/p3_wcs.cpp:31-57` `kFitsPixelOrigin=1.0`/`fits_pixel_1based`；`lib/infrastructure/scheduler/src/module_adapters.cpp:2487-2493`、`:2731-2733`（p1_wcs.json samples）+ `:2555-2557`、`:2811-2813`（`pixel_origin` 声明）。
- 闭环指标消费 0-based 数组下标：`tools/astrometry/closure_metric.py:223` `w.all_pix2world(xy, 0)`（无额外桥接；p1_sources 为整数下标）。
- 无生产消费方：`wcs_sky_to_pixel_iterative` 仅测试面调用（`tests/unit/p1wcs/p1wcs_tests_apbp.cpp`）。

### 2.3 独立数值/第三方复算（2026-09-18，只读）

- astropy 7.0.1：`all_pix2world([[512.5,512.5]], origin=1)` 与 `[[511.5,511.5]], origin=0` 同给 CRVAL；`origin=0` 语义 `p=x+1`。⇒ `out.x=u+CRPIX=512.5` 是 origin=1 的 1-based 对象，非 0-based。
- fixture 复算（`run/p1wcs_wcs003/wcs003_cross_input.json`，独立实现 TAN+SIP）：生产前向与 `u = x_f − crpix` 逐位一致；若改用 0-based 解释（`p=x+1`）则差 1.65–2.45e-4 deg。说明 fixture 的 `x_f` 与 `crpix=512.5` 配对即 Paper-I `q`（`x_f` 即 1-based 连续像素坐标）。
- E2E 外部闭环（`docs/algorithms/GATES_AND_TOLERANCES.md` §3，G-P1-WCS-CLOSURE）：T4 GC panel1 `s0=6.31″/px` 下 solved median **0.1077 px**、match 31.32%（任何 0.5px 平移会在 6.3″/px 下产生 ≥3.15″ 残差、match≈0）；T2/T3 solved−header ≈0.06 px。**排除 0.5px 与 1px 系统差。**
- 开源对照：astrometry.net（`util/anwcs.c` `CRPIX=W/2+0.5`、`util/sip.c` `u=px−crpix` 后 `px=U+crpix`）用同一约定；astropy `origin` 语义见其 legacy interface 文档；WCSLIB 为 Paper I 参考实现。

### 2.4 既有登记

- `run/RELEASE-01/science/SCI-S2-topics.md` §3-2（finding WCS-003-F1）自述："代码 `out.x=u+CRPIX` 在参考点给 `x=CRPIX`，即 FITS 1-based，与标准无 1px 差；同节 :80 又写 `x=CD⁻¹+CRPIX−1`，自相矛盾 … 当前流水线无实际 1px 错误 … 但冻结合同条款标签错误是潜在隐患。"（与本次结论一致。）
- `工程控制/RELEASE-01/GAP_AUDIT.md:109`（P0）与 M1a-C-003 记录了该冲突与"两门对原点平移零判别力"。

## 3 订正前/后 diff 摘要（`docs/science/ASTROMETRY.md`）

| 位置 | before | after |
|---|---|---|
| §5a 内部口径 | "0-based 自洽；`u=x−CRPIX`；参考像素 0-based `x=CRPIX`；与标准差常量 1px；**不是数学内容差**" | 求解器拟合自变量 = sdet 半整数中心 `det_x=i+0.5`；`u=det_x−w/2=p−CRPIX=q`（Paper I）；输出 `x=u+CRPIX=p` 是 1-based；**无 1px 差**；`ipv_wcs.h` 注释属标签错误 |
| §5a 导出/桥接 | 称 `+1` 用于抵消内部 1px；逆向写 `x=CD⁻¹+CRPIX−1` | 明确 `+1` 只是产品网格/数组下标→FITS 的单次换算（与求解器无关）；删去 `CRPIX−1` 逆向式；指出 `wcs_sky_to_pixel_iterative` 无生产消费方 |
| §5a 责任方 | "第三方消费原始 WCS 必须施加 +1 桥接" | 求解器输出已是 1-based，不得再叠加 `+1`；0-based→FITS 换算由 p3/p1_wcs.json 写出侧单点负责 |
| §14a WCS 引用条 | "求解器内部相对 Paper I 是常量 1px 原点偏离" | 改为"旧 §5a 的 0-based/1px 叙述是标签错误；实现与 Paper I 逐式一致" |
| 文件头 | 仅 §11a 变更注记 | 追加 §5a 订正注记（claim FIX-SCI-WCS-001） |

## 4 影响面

- **文档**：`docs/science/ASTROMETRY.md`（§5a/§14a/头）。§7 CRPIX 不变量、§11/§11a 门与容差**均不变**。
- **实现**：本次**未改代码**。建议的 comment-only 修正：`ipv_wcs.h:43,57-60,70-71` 的 "0-based FITS 像素" → "FITS 1-based 像素 `p=u+CRPIX`（拟合自变量 `u` 为 Paper-I `q`）"。**不改任何数值/容差/路径。**
- **合同/产品**：`p1_wcs.json` 已声明 `pixel_origin`/`fits_pixel_origin`，无需结构变更；未触碰三命令 / JSON 结构 / HiPS 数据模型。

## 5 一致性回归

- `python3 tools/science_contract_lint.py docs/science/ASTROMETRY.md` → `SCIENCE_CONTRACT_LINT_PASS kind=sci files=1 sections=15`（rc=0）。
- `python3 ci/run_checks.py --check CHK-SCI-REF --quiet` / `CHK-DANGLING` → 见 `reports/RELEASE-02/FIX-SCI-report.md` §5。
- **待前台构建后复跑（本次不构建）**：`ctest -R 'p1wcs|p3wcs'`（`p1wcs_closure_metric_gate`、`p1wcs_astropy_cross`、`p1wcs_std_f1_bridge_cross`）；真实帧闭环 `closure_metric.py` T2/T3/T4 逐轴均值偏移统计。

## 6 实现侧发现（未改，登记）

1. **注释标签错误（comment-only）**：`ipv_wcs.h:43,57-60,70-71` 把 `x=u+CRPIX` 标为 0-based；实为 1-based `p`。归 P1-WCS-IMPL。
2. **astropy 对拍是 no-op relabel（测试面）**：`tests/unit/p1wcs/p1wcs_astropy_cross.py:296-302` 对 `x_f`（已是 1-based `p`）用 `origin=0` + `crpix+1`；等价于 `origin=1` + 原 `crpix`。建议改为 `origin=1`、去掉 `crpix+1` 桥接并确认残差不变，消除可执行面里最后一份错误叙事（同 `p1wcs_std_f1_bridge_cross.py:59,310` 的 `BRIDGE_DECLARED` 命名）。
3. **真实风险 R2（中）——耦合未锁定**：链路正确依赖"ipv 拟合消费 sdet 半整数探测、而 p1_sources/闭环消费 phase1 整数下标"，两个 0.5 相消；无测试锁定该耦合。若未来把 phase1 整数探测喂给 ipv 拟合或改 `cx/CRPIX`，会静默出现 0.5px 系统差。建议：跨探测合成测试（同一天体分别过 `StarDetector` 与 `sdet_detect_ex`，断言 `x` 相差恰 0.5、重建 `p` 一致）+ 保留 T4 大尺度闭环帧。`run/std_f1_adj/STD_F1_ADJ_REPORT.md` §8.1 与 REAL-001 真实数据终验同指此事。

## 7 状态

- 文档订正：**已落地**（本 claim）。
- 实现订正：**未落地**（§6，均为 comment/测试面；无数值改动；归 P1-WCS-IMPL / INT-001）。
