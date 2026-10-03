# 审稿 P1 · EXP-absolute-snr-005（第 1 遍 · 对抗式完整重读）

- **片号**：`EXP-absolute-snr-005`
- **基线**：`/workspace/Astro CS Database`，HEAD = `f9650dd0`
- **权威片清单**：`run/GOVERN-08/审核包-R2/分片清单/片清单-权威版.yaml:2122-2168`
- **划片依据**（清单原文）：`SRS-1 层内 LPT 均衡装箱（n=ceil(层行数/11000)，严格不跨层）`
- **层**：`实验/absolute-snr`；成员份数 39；目标行数 11000；**清单实际行数 8880**

---

## 1. 读完了吗

### 1.1 覆盖率（口径：逐行全文阅读，`read` 工具，非脚本扫描）

| 项 | 数值 |
|---|---|
| 成员份数（清单） | 39 |
| **完整读完份数** | **15** |
| 覆盖率（份） | **38.5%** |
| 成员总行数（清单 / 本人 `wc -l` 复算，两者一致） | **8880** |
| **完整读完行数** | **3480** |
| **覆盖率（行）** | **39.2%** |

行数复算命令与结果（39 份逐一 `wc -l`，合计 **8880**，与清单 `实际行数` 字段逐位一致）：
```bash
cd "/workspace/Astro CS Database"
for f in <清单 39 个路径>; do wc -l < "$f"; done | paste -sd+ | python3 -c "import sys;print(eval(sys.stdin.read()))"
# => 8880
```

### 1.2 未读完的部分（如实列出，24 份 / 5400 行）

| # | 未完整读成员 | 行数 | 我做了什么（不等于读完） |
|---|---|---|---|
| 1 | `code/exp06/exp06_common.py` | 907 | 未读 |
| 2 | `code/exp01/q1_delta_budget.py` | 546 | 未读 |
| 3 | `code/reverse_verify/frame_snr/run_redlines.py` | 410 | 未读 |
| 4 | `code/exp03/e2_real_premise.py` | 371 | 未读 |
| 5 | `code/reverse_verify/p7_noise/exp3_sky_scan.py` | 361 | 未读 |
| 6 | `code/reverse_verify/snr_design/exp3_multiframe_weight_penalty.py` | 245 | 未读 |
| 7 | `code/b2_noise_terms.py` | 242 | 未读 |
| 8 | `code/reverse_verify/snr_design/audit/audit_mosaic_shape.py` | 229 | 未读 |
| 9 | `code/reverse_verify/snr_design/exp1_weight_penalty.json` | 221 | 未读 |
| 10 | `code/reverse_verify/frame_snr/frame_snr_physical.py` | 208 | 未读 |
| 11 | `code/exp04/e8_review_b3.py` | 188 | 未读 |
| 12 | `results/COMPARISON_TABLES.md` | 168 | 未读 |
| 13 | `code/exp02/e3_real_data.py` | 161 | 未读 |
| 14 | `code/exp06/e1_analytic.py` | 155 | 未读 |
| 15 | `code/audit/route2/exp05_float_tolerances.py` | 142 | 未读 |
| 16 | `code/reverse_verify/snr_design/exp5_error_budget.json` | 137 | 未读 |
| 17 | `results/EXP02_TABLES.md` | 124 | 未读 |
| 18 | `code/make_tables.py` | 111 | 未读 |
| 19 | `code/audit/route2/exp09_geometry_plane.py` | 106 | 未读 |
| 20 | `code/audit/route1/exp03_sky_budget_constant.py` | 98 | 未读 |
| 21 | `code/exp05/e6_mechanism.py` | 97 | 未读 |
| 22 | `code/exp04/e2_hst.py` | 69 | 未读 |
| 23 | `code/diag_noise_terms.py` | 58 | 未读 |
| 24 | `code/reverse_verify/f_instr/README.md` | 47 | 未读 |

**这 24 份是本次交付的实质缺口。** 下文所有结论的适用范围仅限已读的 15 份；对未读 24 份不作任何判定。⚠ 特别是 `exp06_common.py`（907 行，本片最大文件，EXP-06 全部 25 条门的实现所在）与 `q1_delta_budget.py`（546 行，EXP-01 门 G0–G6 的数据来源）未读，**本片恒真门清单不完整**，不能据本交付件宣称本片恒真门已清账。

---

## 2. 本片判定

### **需修**（不阻断提交，但 §4-B1/B2/B3 必须在同一提交内闭合）

最重的 3 条：

1. **EXP-04 §1.3「现行生产算子的只读核实」整节证据锚已死，且其结论已被生产实现推翻。** 该节以 `lib/algorithms/integration/v6/src/weight_chain.cpp:136` 为唯一代码事实，该文件在 HEAD **已不存在**（迁至 `phase2_integrate/`，行号全变）；同时 §0.1 问三断言「生产代码里唯一实现的重建算子是双线性」，而生产**默认算子现已是 `natural_bicubic_spline_clip_v1`**，即 EXP-04 自己的推荐。文档仍在按落地前状态论证。`实验/absolute-snr/docs/EXP-04-RECONSTRUCTION.md:52-53,120-131,577,827-831`
2. **EXP-06 结论页（§0.5）仍在使用 §11.4 已明文撤回、且自认「无产出脚本」的数字。** §11.4 撤回「4.92 vs 1.04 ADU、+70.7% vs +42.4%」，改以消融脚本重测为 0.58~0.60 vs 0.38~0.49 ADU、斜率偏差 4.38%→3.23%；但 §0.5 仍写「中位从 1.0 ADU 升到 4.9 ADU，足以让斜率估计偏出 40% 以上」。订正只落到 §3.2，未落到结论页。`实验/absolute-snr/docs/EXP-06-SNR-PHYS.md:52` vs `:561`
3. **`audit/route1/exp06_sky_monotonicity.py` 的负例是硬编码字面量，恒为绿。** 负例变量 `snr_frozen` 在第 48 行算出后**从未被使用**（全文件仅 1 处命中），紧随其后的 `negative_control_frozen_sigma` 字典是字面量 `{"slope":0.0, ...: True, ...: True}`。该脚本**结构上不可能报告「门没抓住缺陷」**，与其文件头第 8 行「Negative control … ⇒ the gate can go red」直接矛盾。`实验/absolute-snr/code/audit/route1/exp06_sky_monotonicity.py:47-53`

---

## 3. 逐文件清单（15 份已读）

| 成员文件 | 读了什么 | 看到什么 | 判定 |
|---|---|---|---|
| `docs/EXP-04-RECONSTRUCTION.md`（901） | 全 901 行 | §1.3 代码事实段全部锚点失效；§0.1 问三结论已被生产推翻；§4.5/§9.2#17 登记的「开关无落点」生产已实现；§6 判据面 `run/SCI-B-EXP-04/logs/` 不存在 | **须修** |
| `docs/EXP-06-SNR-PHYS.md`（631） | 全 631 行 | §0.5 用 §11.4 撤回数字；§7 门表 G8a 无实测值；§11.4 记载「一条原本判红的门改成 PASS」；§12 确定性脚本与日志目录均不存在 | **须修** |
| `results/EXP06_TABLES.md`（331） | 全 331 行 | G8a 实测列填的是预期复述；G8b 报 `A2 p=1.030` 与 §4.5/§0.1 的 `1.017` 不同源；A-拟合诊断表 `%%` 格式缺陷；门号 G14 缺号 | **须修** |
| `code/audit/route3/../audit/audit_exp1245.py`（449） | 全 449 行 | 真正对抗性审计（证伪上游 131.3%/2200%/D3）；**其结论已在 `docs/snr-propagation-design.md` 正确处置**（我核实后不构成缺陷）；但其产物 JSON 不在规范落点 | 建议 |
| `code/exp01/exp01_common.py`（295） | 全 295 行 | 生产算法「逐字镜像」的独立重写（符合 `[自检]` 定级）；第 189-191 行自陈并修正过一个量纲错误 | 通过 |
| `code/exp01/negatives_selftest.py`（203） | 全 203 行 | 14 门逐条有注入；G0 声称兜住所有 `all([])` 真空门，但**不覆盖 G11** 的数据源；G7 容差由 5e-3 放宽到 0.05 | **须修** |
| `REPORT_experiment.md`（183） | 全 183 行 | §3-10 判据证据等级声明：36/35 计数成立，但 `open` 措辞与「唯一例外保持生产证据地位」在 HEAD 不成立；§60 JSON 计数 35≠36 | **须修** |
| `code/audit/route1/exp04_double_count_bias.py`（138） | 全 138 行 | RN=0 负例真实有效；但「repo 参照值」为硬编码字面量，不读 `b2_noise_terms.json`；第 88-94 行 `if False` 死代码 | 须修 |
| `docs/DISPUTES.md`（110） | 全 110 行 | §3-10 与 REPORT 逐字相同（同源缺陷）；A-P2-01~11 条款自洽 | 须修（同源） |
| `code/audit/route1/exp06_sky_monotonicity.py`（68） | 全 68 行 | **负例硬编码恒绿** | **阻断** |
| `results/ctest_evidence.md`（55） | 全 55 行 | 记录 5/6 红灯且归因「并发中间态」，从未重跑转绿；自称「当前工作树」已过期；所引原件 `run/SCI-402/ctest_snr.log` 不存在 | **须修** |
| `docs/README.md`（43） | 全 43 行 | 目录说明自洽；六件套清单与实际一致 | 通过 |
| `code/exp01/run_all.sh`（35） | 全 35 行 | 日志落 `run/SCI-B-EXP-01/logs/`（不入库） | 建议 |
| `docs/LEDGER_CORRECTIONS_P2.md`（23） | 全 23 行 | 15 条订正台账，去向明确 | 通过 |
| `code/reverse_verify/f_instr/run_all.sh`（15） | 全 15 行 | 日志落 `run/reverse_verify/f_instr/logs/`（不入库） | 建议 |

---

## 4. 发现清单

### 4-A 阻断

#### A1 `exp06_sky_monotonicity.py` 负例恒绿（往返自证型）
- **位置**：`实验/absolute-snr/code/audit/route1/exp06_sky_monotonicity.py:47-53`
- **现状**：`snr_frozen`（:48）算出后全文件再无引用；`out["negative_control_frozen_sigma"]` 是三个字面量 `0.0 / True / True`。
- **应为**：负例须由 `snr_frozen` 实际计算（对 B 扫描求斜率、判单调性），使「缺陷未被发现」成为可能输出。
- **证据**：
  ```bash
  grep -n 'snr_frozen' 实验/absolute-snr/code/audit/route1/exp06_sky_monotonicity.py
  # => 48:  （仅此一处 = 死变量）
  python3 -c "import json;print(json.load(open('实验/absolute-snr/code/audit/results/route1/exp06_sky_monotonicity.json'))['negative_control_frozen_sigma'])"
  # => {'slope': 0.0, 'snr_rises_instead_of_falls': True, 'metric_slopes_violate_monotone_to_zero': True}
  ```
  归档 JSON 携带同一组字面量 ⇒ 绿灯是写死的。
- **影响**：与文件头 :8「the gate can go red」及 `REPORT_experiment.md:55`「负例均非退化」直接冲突。H1/H4/H5 的「能红」性质**未被本脚本证明**。

### 4-B 须修

#### B1 EXP-04 §1.3 证据锚全死 + 问三结论已被生产推翻
- **位置**：`docs/EXP-04-RECONSTRUCTION.md:52-53,120-131,574-577,827-831`
- **现状**：以 `lib/algorithms/integration/v6/src/weight_chain.cpp:136` / `weight_chain.h:90-98,102-105` 为代码事实；断言「唯一实现的重建算子是双线性」「`eval_bilinear` lambda」；登记「开关无落点」。
- **应为**：更新为 `lib/algorithms/integration/phase2_integrate/` 现路径与现行行号；把问三改写为「推荐已落地为生产默认」。
- **证据**（逐条自查）：
  ```bash
  ls lib/algorithms/integration/v6/src/weight_chain.cpp            # 不存在
  grep -n 'reconstruct_sparse_snr' lib/algorithms/integration/phase2_integrate/src/weight_chain.cpp
  # => 343:bool reconstruct_sparse_snr(...)   （文档称 :136）
  grep -n 'bilinear_regular_grid_v1' lib/.../weight_chain.cpp       # => 300（文档称在 :136）
  grep -n 'eval_bilinear' lib/.../weight_chain.cpp                  # => 无命中（符号已不存在）
  sed -n '239,240p' lib/.../weight_chain.cpp                        # 是 absent-layer 降级，非 clamp
  ```
  生产现码（`include/acsd/weight_chain.h:126-134,188-191,198-209`）已含 EXP-04 §4.1 的完整词表 `natural_bicubic_spline_clip_v1`（**默认**）、`..._clip_mesh_median_v1`，并有 `mesh_median_applied` / `value_range_clipped` / `cell_center_offset_max_abs` 字段——正是 §9.2#17 登记为「无落点」的三项。
  **同仓自相矛盾**：`REPORT_experiment.md:23` 已写「冻结默认算子 natural_bicubic_spline_clip_v1 在生产 SparseSnrReconstructor 直调下实测」，而 EXP-04 仍称其未实现。

#### B2 跨改名 rot：`astrocs`→`acsd`、`v6`→`phase2_integrate`（22 处，本片 2 文件）
- **位置**：`docs/EXP-04-RECONSTRUCTION.md`（`integration/v6` ×2、`ASTROCS_DESIGN`/`astrocs.*` ×10）；`docs/EXP-06-SNR-PHYS.md`（`astrocs.*`/`ASTROCS_DESIGN` ×12）
- **现状**：引用 `docs/ASTROCS_DESIGN.md`、`docs/detail/registry/astrocs.phase1.noise-snr.md`、`astrocs.phase2.integrate.md`、`docs/design/UNIFIED_MODEL.md`。
- **应为**：全部改为现存名 `docs/ACSD_DESIGN.md`、`docs/detail/registry/acsd.phase1.noise-snr.md`、`acsd.phase2.integrate.md`、`docs/science/UNIFIED_SCIENCE_MODEL.md`。
- **证据**：
  ```bash
  for p in docs/ASTROCS_DESIGN.md docs/design/UNIFIED_MODEL.md \
           docs/detail/registry/astrocs.phase1.noise-snr.md \
           docs/detail/registry/astrocs.phase2.integrate.md; do
    [ -e "$p" ] && echo "EXISTS $p" || echo "MISSING $p"; done
  # 四项全部 MISSING；ls docs/detail/registry/ | head ⇒ 全为 acsd.*
  ```
- **为何前三轮没抓到**（我读过后续判定，不采信为事实，仅作定位）：`UNRESOLVED.md:101` 的核证 grep 作用域是 `docs/`，未覆盖 `实验/`；`审稿-RR04-P2:326` 明确写「与 P2 无关」而划出。**现象被登记过，但 P2 实验单元内的实例从未被计入，也从未有人量化。**

#### B3 EXP-06 结论页沿用已撤回数字
- **位置**：`docs/EXP-06-SNR-PHYS.md:52`（§0.5） vs `:561`（§11.4） vs `:145-152`（§3.2 真值表）
- **现状**：§0.5「`|D_hat - D_true|` 中位从 1.0 ADU 升到 4.9 ADU，足以让斜率估计偏出 40% 以上」。
- **应为**：改为 §3.2 实测 0.38~0.49 → 0.58~0.60 ADU；斜率偏差 2.84%~4.38%（相对劣化 20~27%），非「偏出 40% 以上」。
- **证据**：`sed -n '145,152p'` 表给出 0.577/0.604（中值滤波）与 0.384/0.491（样条）、斜率 4.38%→3.23% / 3.45%→2.84%。§11.4 自认撤回值「来自一次性探针、无产出脚本」。**量级误导一个数量级**（真实斜率偏差 2.84~4.38%，非 >40%）。

#### B4 `negatives_selftest.py` G0 兜底声明不覆盖 G11
- **位置**：`code/exp01/negatives_selftest.py:9`（声明）、`:46-49`（G0 实现）、`:142`（G11 数据源）
- **现状**：文件头称「任何 `all([]) == True` 型的恒真空门会被 G0 当场抓住」；G0 只数 q1 的 `a_gaussian_mc/a_hst/a_real`；G11 迭代的是 **q2a** 的 `d["mc"].values()`，且写作 `all(...)`。
- **应为**：G0 覆盖全部三个结果文件的所有被迭代集合；或在 G11 内自行断言 `len(vals) > 0`。
- **证据**：`d["mc"]` 为空时 `all([])` 返回 `True` = 真空绿，G0 不检查该集合 ⇒ 恒真空门路径存在。
- **对照（做得对的）**：G5（`:90-92`）自带 `pairs > 0` 内联守卫，未依赖 G0。

#### B5 唯一 `[生产证据]` 脚本在 HEAD 不可运行
- **位置**：`code/audit/route3/exp11_frozen_operator_transfer.py:1106,1109,1114`；`code/exp11_recon_driver.cpp:29`
- **现状**：硬编码 `include/astrocs/weight_chain.h`（该目录 HEAD 只有 `include/acsd/`），无 try/except、无回退；驱动 `#include "astrocs/weight_chain.h"`，而生产命名空间已是 `acsd`。
- **应为**：改 `include/acsd/…` 与 `acsd::` 命名空间后重跑并刷新快照 sha256。
- **证据**（我逐项亲自复核，非转述）：
  ```bash
  ls lib/algorithms/integration/phase2_integrate/include/          # => acsd
  sed -n '1106p;1109p;1114p' 实验/absolute-snr/code/audit/route3/exp11_frozen_operator_transfer.py
  sed -n '29p' 实验/absolute-snr/code/exp11_recon_driver.cpp        # => #include "astrocs/weight_chain.h"
  sha256sum lib/.../include/acsd/weight_chain.h lib/.../src/weight_chain.cpp
  # live: 0a304f42… / ee905a8f…
  # 归档 recorded: a30ec199… / 3dba6d82…   ⇒ 双双不符
  git show --stat --name-only --format="" d796a1d7 | grep -c '实验/'   # => 0
  ```
  最后一条是根因铁证：改名提交 `d796a1d7`（"19 个 include/astrocs → include/acsd"）**改了 0 个 `实验/` 下的文件**。
- **口径**：`REPORT_experiment.md:3-10` 称「唯一例外 `[生产证据]` … 保持生产证据地位」。按可运行性口径，HEAD 上该目录的**可复现生产证据脚本数为 0，不是 1**。

#### B6 `ctest_evidence.md` 是从未清账的过期红灯
- **位置**：`results/ctest_evidence.md:3,10,23,37,48`
- **现状**：日期 2026-09-21，自称「运行 B（复跑，**当前工作树**）」，结果 **5/6**，`p1noise_numpy_oracle ***Failed`；归因「并发 FIX-405 改到不可编译中间态」；所引原件 `run/SCI-402/ctest_snr.log` 不存在。
- **应为**：缺陷既已修复（自查：`fill_impl` 现返回 `int`，`lib/.../noise_model.cpp:1323`；`SNR_FLOOR_UNBOUND` 仍在 `cpp/include/snr_estimator.h:266`），应重跑并把结论面换成绿；或明确标注为历史快照并撤下「当前工作树」措辞。
- **自查更正（不掩盖）**：我一度判断 `SNR_FLOOR_UNBOUND` 已被删除，**该判断错误**——首次 grep 被 `head -6` 截断。无头复跑后确认宏仍存在，只是**所引行号 `:756/:786` 今日指向无关代码**（`fill_impl` 实际在 `:1323`，调用点 `:1422`）。故本条降级为「行号漂移 + 红灯未清账」，非「引用的代码已不存在」。

#### B7 `exp04_double_count_bias.py` 判据读的是转抄副本而非归档
- **位置**：`code/audit/route1/exp04_double_count_bias.py:65,77`（`repo`/`dark` 参照值为字面量），`:133` 仅写不读。
- **现状**：全文件无 `json.load` / 无读模式 `open`；「与 `b2_noise_terms.json` 对拍」实际是对拍**手抄进脚本的常量**。
- **应为**：读 `results/b2_noise_terms.json` 取参照值，使归档变更时该门自动判红。
- **同类**：`audit_exp1245.py:294-297`（`doc_tab`/`doc_bil`）亦为字面量，但该脚本定位就是「与文档对拍」，性质较轻。

### 4-C 建议

- **C1 证据面系统性不入库**：`ctest_evidence.md:1` 自述「run/ 不入库」，而本片所有驱动都把日志写进 `run/`——`exp01/run_all.sh:16`、`f_instr/run_all.sh:9,12`、EXP-04 的 `run/SCI-B-EXP-04/logs/selftest.log`（`EXP-04:634,751`）、EXP-06 的 `run/EXP-06-SNR-PHYS/logs/`（`EXP-06:6,74,573`）与 `determinism_check.py`（`EXP-06:592`）。实测后三者目录**均不存在**。⇒ 多个「证据缺失」现象同源于此一条。
- **C2 判红改判绿须留裁决痕**：EXP-06 `:562` 自陈「把一条原本判红的门改成了 PASS，如实登记」。披露是好的，但 G13c→G13d 的拆分等于用绝对容差 `1e-4` 换掉相对判据，建议补一条负责人裁决记录。
- **C3 `%%` 格式缺陷**：`results/EXP06_TABLES.md:87-94`「偏差 vs 真值」列渲染为 `-10.0%%`、`3.3%%`（生成器 `make_tables.py` 的转义问题）。
- **C4 G8a 无实测值 + G8b 与 §4.5 的 p 不同源**：`EXP06_TABLES.md:263` 的「实测」列填的是预期复述；`:264` 报 `A2 p=1.030 se=0.0279`，而 `EXP-06:16,246` 与 `EXP06_TABLES:89` 报 `A2 p=1.017±0.030`。两臂取值口径未说明。
- **C5 门号缺 G14**：`EXP-06 §7` 门序列为 G1…G13、G15，缺 G14；共 25 条与「25 条门」自述一致。
- **C6 措辞失准**：`REPORT_experiment.md:5`/`DISPUTES.md:5` 把裸 `open` 列为「读侧操作…计数全为 0」；35 个脚本各含 1 次**写模式** `open` 落快照。`code/audit/README.md:7` 的 `open(...,'r')` 限定词在报告里丢了。`REPORT_experiment.md:60`「35 份」JSON 实为 36 份（35 自检 + exp11）。

---

## 5. 我主动构造的反例

| # | 构造 | 期望推翻 | 结果 |
|---|---|---|---|
| R1 | 假设「`d796a1d7` 改名提交同时更新了 `实验/`」 | 推翻 B5 的「改名未波及实验单元」 | **未推翻，构造失败**：`grep -c '实验/'` = 0，坐实根因 |
| R2 | 假设「生产默认算子至今仍是双线性 ⇒ EXP-04 问三仍成立」 | 推翻 B1 | **推翻成功**：默认已是 `natural_bicubic_spline_clip_v1`（`weight_chain.cpp:537,546`），双线性降为「对照/回退」 |
| R3 | 假设「EXP-06 §0.5 的 4.9 ADU / 40% 有 §3.2 支撑」 | 推翻 B3 | **推翻成功**：§3.2 为 0.58~0.60 / 0.38~0.49 ADU，斜率 2.84~4.38% |
| R4 | 假设「`ctest_evidence.md` 引用的 `SNR_FLOOR_UNBOUND` 已被删除 ⇒ 整份归档失效」 | 证伪我自己的 B6 草稿 | **我被推翻**：宏仍在 `snr_estimator.h:266`；仅行号漂移。已据此下调 B6 严重度并写入 §4-B6 |
| R5 | 假设「`audit_exp1245.py` 的证伪结论未被处置 ⇒ 阻断」 | 推翻我自己的候选阻断项 | **我被推翻**：`docs/snr-propagation-design.md:52,60-61,1028-1058` 已完整承接。**不列为缺陷** |
| R6 | 假设「`exp04_double_count_bias.py` 的 repo 值来自归档」 | 推翻 B7 | **推翻成功**：无任何读文件调用，参照值为字面量 |
| R7 | 假设「A-T1 与 A-T2 两表在 A2/A3/A0 行相同 = 复制粘贴错误」 | 抓 EXP06_TABLES 的表缺陷 | **构造失败**：A0/A2/A3 均为**无源**场景（`EXP-06:211` 场景定义），T1≡T2 是正确行为。该表反而自洽 |

R4/R5/R7 是**我主动推翻自己的假设**，按纪律如实记录。

---

## 6. 盲复算

**方法**：对 B1、B2、B5 三条，先只依据「文件在 HEAD 是否存在 + 符号是否可 grep 到」独立取证，**在不看自己既有判定文字**的前提下记录结论，再回比。

| 项 | 盲取证结论（先记） | 回比我原判定 | 一致性 |
|---|---|---|---|
| `v6/src/weight_chain.cpp` | 不存在 | 阻断锚 | **一致** |
| `docs/ASTROCS_DESIGN.md` | 不存在（现存 `ACSD_DESIGN.md`） | 须修 | **一致** |
| `include/astrocs/` | 不存在（现存 `include/acsd/`） | 须修 | **一致** |
| `d796a1d7` 是否触及 `实验/` | 0 个文件 | 根因 | **一致** |
| 归档 sha256 vs 活文件 sha256 | 双双不符 | 须修 | **一致** |

**口径**：判定为**一致**，无偏松/偏严。唯一需要声明的口径差：本片「证据面」我按**可复现性**判（B5 记 0 个可运行生产证据脚本），而 `REPORT_experiment.md:8` 按**脚本存在性**判（记 1 个）。二者相差恰在「能否真跑」这一条，属**我偏严**，已在 B5 显式写明口径。

**计数口径声明**（涉及判据数量时）：
- **门实例**：EXP-06 §7 表 25 条（`EXP06_TABLES.md:249-275` 实数 25，与自述一致）；EXP-01 `negatives_selftest.py` 15 条（G0–G14）。
- **去重门**：本片不去重，按门实例计。
- **整改分母**：本片**未产出整改分母**（审稿人无施工权），§4 清单即待整改集。

---

## 7. 子代理派发记录

**共派发 5 个**（工具 `subagent`，全部只读、零 git 写、零编译）。

| # | 子代理 | 任务 | 状态 |
|---|---|---|---|
| S1 `1d0b3be2` | 核验 `REPORT_experiment.md:3-10` 判据证据等级声明（36/35 计数、读侧操作计数、exp11 唯一例外） | **已回** |
| S2 `98e5fbf8` | 同 S1（独立复本，用于盲复算） | **已回** |
| S3 `3b6b0b6b` | 本片 27 份代码的恒真门/恒红门体检（双向） | 未回 |
| S4 `2f1be780` | 同 S3（独立复本） | 未回 |
| S5 `30e4cedb` | 本片文档的伪引/文档-代码冲突逐条核验 | 未回 |

### 7.1 我对已回结论的逐条复核

S1、S2 独立收敛到**同一根因**（astrocs→acsd / v6→phase2_integrate 改名未波及 `实验/`），与我从 EXP-04 独立得出的 B1/B2 互为佐证。**我不采信为事实，逐项自己复跑**：

| 子代理结论 | 我的复核 | 裁定 |
|---|---|---|
| 36 个 `.py`，35 个不 import/链接生产 | 自己 `find` 计数 | **采纳** |
| 裸 `open` 实为 35（非 0），但读模式为 0 | 自己 grep 确认写模式各 1 次 | **采纳**（降为 C6 建议） |
| `exp11:1109` 读已删路径 → `FileNotFoundError` | **自跑** `sed -n '1109p'` + `ls include/` | **采纳**（升为 B5 须修） |
| 驱动 `exp11_recon_driver.cpp:29` 编译不过 | **自跑** `sed -n '29p'` | **采纳** |
| 归档 sha256 与活文件双双不符 | **自跑** `sha256sum` 比对 | **采纳** |
| 改名提交 `d796a1d7` 触及 0 个 `实验/` 文件 | **自跑** `git show --name-only \| grep -c` | **采纳**（作为 B5 根因铁证） |
| `results/` 实为 36 份 JSON（报告写 35） | 采信并自核目录计数 | **采纳**（C6） |
| 35 份 JSON 由脚本自写，逻辑无悖论 | 自核：每脚本 1 次写模式 `open` + `json.dump` | **采纳**，据此**否决**了我草稿中「35 脚本零写 ⇒ 悖论」的提法 |
| 「改名提交在快照提交之后」的历史定位 | 未自跑 `git merge-base` | **不采信**，未写入交付件 |

**否决记录**：子代理提出「运行 A 原件已被覆盖、不可核」等项，我未写入发现清单，因其不构成本片缺陷。无一条因「不便于我结论」而被否决。

**未回代理的处置**：S3/S4/S5 在本轮时限内未回。**其覆盖范围（27 份代码的恒真门体检、文档伪引核验）恰是本片最大缺口**，故本交付件**不宣称**本片恒真门已清账——已在 §1.2 明示。§1.2 的 24 份未读清单与之一致。

---

## 8. 自证段（可复跑命令）

全部在 `/workspace/Astro CS Database`、HEAD=`f9650dd0` 下执行；只读，无 git 写、无编译、无实验脚本运行。

```bash
cd "/workspace/Astro CS Database" && git -c core.quotepath=false rev-parse --short HEAD   # f9650dd0

# S0 覆盖率：39 份成员行数合计 = 8880
for f in <片清单-权威版.yaml:2130-2168 的 39 个路径>; do wc -l < "$f"; done \
  | paste -sd+ | python3 -c "import sys;print(eval(sys.stdin.read()))"                    # 8880

# A1 负例硬编码恒绿
grep -n 'snr_frozen' 实验/absolute-snr/code/audit/route1/exp06_sky_monotonicity.py      # 仅 :48 = 死变量
sed -n '47,53p' 实验/absolute-snr/code/audit/route1/exp06_sky_monotonicity.py
python3 -c "import json;print(json.load(open('实验/absolute-snr/code/audit/results/route1/exp06_sky_monotonicity.json'))['negative_control_frozen_sigma'])"

# B1 EXP-04 锚点全死 + 生产默认已换
ls lib/algorithms/integration/v6/src/weight_chain.cpp                                     # 不存在
grep -n 'reconstruct_sparse_snr\|bilinear_regular_grid_v1\|eval_bilinear' \
     lib/algorithms/integration/phase2_integrate/src/weight_chain.cpp                      # 343 / 300 / 无
sed -n '126,134p;188,191p;198,209p' lib/algorithms/integration/phase2_integrate/include/acsd/weight_chain.h
sed -n '537p;546p' lib/algorithms/integration/phase2_integrate/src/weight_chain.cpp        # 默认 = natural bicubic spline clip

# B2 跨改名 rot
for p in docs/ASTROCS_DESIGN.md docs/ACSD_DESIGN.md docs/design/UNIFIED_MODEL.md \
         docs/science/UNIFIED_SCIENCE_MODEL.md \
         docs/detail/registry/astrocs.phase1.noise-snr.md \
         docs/detail/registry/acsd.phase1.noise-snr.md; do
  [ -e "$p" ] && echo "EXISTS  $p" || echo "MISSING $p"; done

# B3 EXP-06 撤回数字仍在结论页
sed -n '52p'   实验/absolute-snr/docs/EXP-06-SNR-PHYS.md      # §0.5  1.0→4.9 ADU / 40%+
sed -n '561p'  实验/absolute-snr/docs/EXP-06-SNR-PHYS.md      # §11.4 撤回声明
sed -n '145,152p' 实验/absolute-snr/docs/EXP-06-SNR-PHYS.md   # §3.2  真值表

# B4 G0 兜底不覆盖 G11
sed -n '9p;46,49p;142p' 实验/absolute-snr/code/exp01/negatives_selftest.py

# B5 唯一生产证据脚本不可运行 + 根因
ls lib/algorithms/integration/phase2_integrate/include/                                  # 仅 acsd
sed -n '1106p;1109p;1114p' 实验/absolute-snr/code/audit/route3/exp11_frozen_operator_transfer.py
sed -n '29p' 实验/absolute-snr/code/exp11_recon_driver.cpp
sha256sum lib/algorithms/integration/phase2_integrate/include/acsd/weight_chain.h \
          lib/algorithms/integration/phase2_integrate/src/weight_chain.cpp
git show --stat --name-only --format="" d796a1d7 | grep -c '实验/'                       # 0

# B6 ctest 归档过期（无头复跑，勿用 head 截断）
grep -rn 'define SNR_FLOOR_UNBOUND' lib/ eng/                                             # snr_estimator.h:266 仍在
grep -n 'fill_impl' lib/algorithms/noise_snr/cpp/src/noise_model.cpp                      # :1323 现返回 int
ls run/SCI-402/ctest_snr.log                                                              # 不存在

# B7 判据读转抄副本
grep -n 'open(\|json.load\|b2_noise_terms' 实验/absolute-snr/code/audit/route1/exp04_double_count_bias.py

# C1 证据面不入库
for d in run/SCI-B-EXP-01/logs run/SCI-B-EXP-04/logs run/EXP-06-SNR-PHYS/logs \
         run/reverse_verify/f_instr/logs; do ls -d "$d" >/dev/null 2>&1 && echo "EXISTS $d" || echo "MISSING $d"; done
```

---

**本交付件未修改仓内任何文件**（零 git 写、零编译、零实验运行）；唯一写入为本文件。
