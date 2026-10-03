# 审稿-P1-EXP-shared-001

**片号**：`EXP-shared-001` ｜ **层**：`实验/shared` ｜ **交付件路径**：`run/GOVERN-08/审核包-R2/审稿-P1-EXP-shared-001.md`

---

## 0. 基线声明（与派单不符，须前台裁决）

派单给定基线为 `HEAD=cbddbb8d` + 工作树干净。本车道开工实测同样成立。**但审稿途中仓库被前台推进了一个提交**，实测：

```
git log --oneline -1   → db354e22 登记禁止修改清单：故障注入元测试全仓十个
git status --porcelain → M 实验/TAUTOLOGY_REGISTER.md
                         M 实验/absolute-snr/code/exp05/e5_gates.py
                         M 实验/additive-sky-seamless/code/audit_rework/GATE_DISCLOSURE.json
                         ?? run/GOVERN-08/审核包-R2/G08-05-整改-登记措辞与判据移动.md
```

核验本片是否受影响：

```
git diff --name-only cbddbb8d db354e22 -- <本片 46 个成员文件> | wc -l   → 0
```

`cbddbb8d..db354e22` 只新增两份 `run/GOVERN-08/**` 下的交付件。**本片 46 个成员文件在两个基线之间逐字节未变**，本审稿结论对两个基线同样成立。三个被改动的文件均不在本片内，属其他车道。

新增的 `禁止修改清单-故障注入元测试.md` 列出全仓 10 个受保护文件（`lib/**` 9 个 + `eng/**` 1 个）。**本片无任何文件在该清单内**，该清单不改变本车道红线。

---

## 1. 读完了吗（份数 / 行数 / 覆盖率）

### 1.1 口径

- **片实际行数**：`片清单-权威版.yaml:2603-2656` 记 `成员份数: 46`、`实际行数: 10856`。
- 独立实测：对 46 个成员文件逐个 `wc -l`，合计 **10856**，与清单**逐位相符**。清单与磁盘一致，划片无缺漏。

### 1.2 我本人逐行读完的部分

| 文件 | 行数 | 本人读到的区间 | 本人实读 |
|---|---|---|---|
| `synthetic/m16_sampling.py` | 1485 | 1–1485 | **读满** |
| `synthetic/noise_selftest.py` | 1323 | 1–1323 | **读满** |
| `synthetic/noise_model.py` | 520 | 1–520 | **读满** |
| `synthetic/gainlib.py` | 241 | 1–241 | **读满** |
| `data/synthetic/generate.py` | 105 | 1–105 | **读满** |
| `synthetic/run_selftests.sh` | 91 | 1–91 | **读满** |
| `synthetic/m16_scene.py` | 897 | 240–897 | 658（**缺 1–239**） |
| `synthetic/render.py` | 634 | 315–634 | 320（**缺 1–314**） |
| `synthetic/m16_mask.py` | 592 | 293–592 | 300（**缺 1–292**） |
| `synthetic/.gitkeep` | 0 | — | 0（空文件） |
| **小计** | **5886** | | **5043** |

**本人逐行覆盖率 = 5043 / 10856 = 46.4%**（按行）；**10 / 46 份 = 21.7%**（按份）。

### 1.3 未被本人读完的部分（必须列出，不得以「已覆盖」掩盖）

| 未读文件 | 行数 |
|---|---|
| `synthetic/m16_scene.py` 第 1–239 行 | 239 |
| `synthetic/render.py` 第 1–314 行 | 314 |
| `synthetic/m16_mask.py` 第 1–292 行 | 292 |
| `synthetic/synth_gain.py` | 386 |
| `references/REVERSE_VERIFY_BIBLIOGRAPHY.md` | 333 |
| `references/reverse_verify_bibliography.bib` | 501 |
| `REVERSE_VERIFY_MIGRATION.md` | 104 |
| `synthetic/README.md` | 120 |
| `data/README.md` | 84 |
| `data/real/README.md` | 117 |
| `data/synthetic/before_stackn32/CHANGES.md` | 44 |
| `data/synthetic/datasets.json` | 234 |
| `synthetic/scenes/*.json`（23 个） | ≈ 2 737 |
| `data/synthetic/before_stackn32/scenes/*.json`（4 个） | ≈ 396 |
| **合计** | **≈ 5 813** |

**诚实声明**：参考文献两份（`.bib` 501 + `BIBLIOGRAPHY.md` 333）与全部 27 个场景 JSON **本人一行未读**。因此本交付件中**任何关于伪引、`before_stackn32` 旧场景、场景 JSON 一致性的结论一律缺席**，不给出「无问题」的判断——未读即未判。`synth_gain.py` 本人未读，其下述发现全部来自子代理，本人**未独立复核**。

### 1.4 子代理补读后的并集覆盖

本车道派出 3 个子代理（详见 §7），**三个均已交付**：

- `673d4a30`：`m16_sampling.py` / `noise_selftest.py` / `m16_scene.py` 三份读满（3 705 行）；
- `4774e795`：`render.py` / `m16_mask.py` / `noise_model.py` / `synth_gain.py` / `gainlib.py` / `generate.py` / `run_selftests.sh` 七份读满（2 569 行）；
- `3f22ee01`：**36 份文档 + 数据集 + 场景 JSON 全部逐行读满，4 583 行，无未读项**（7 文档 1 303 + `datasets.json` 234 + `synthetic/scenes/` 23 个 2 650 + `before_stackn32/scenes/` 4 个 396 + `.gitkeep` 0）。

**并集覆盖 = 10 856 / 10 856 = 100%**（本片 46 份全部至少被一份逐行读完）。

**结论口径**：本片**并集 100% 覆盖**，但**本车道本人仅 46.4%**。§3.10（`synth_gain.py`）与 §3.11（全部文档 / 参考文献 / 场景 JSON）的发现**全部转引子代理**，本车道未复核其行号。**文档面与场景数据面的关键发现见 §3.11**，引用时按 §8.3 的强度限定对待。

---

## 2. 本片判定与最重 3 条

**判定：不通过。** 本片承载实验单元「一键复现」的公共前置步，其自检面存在**多条对真实缺陷零判别力的门**，且**入口层存在可制造绿色产物的路径**。「自检全绿 ⇒ 实验读数可作为证据」这条推论，在下列缺陷下不成立。

### 最重第 1 条 · `m16_scene.py:791/793` V3 掩膜传播门测的是测试自己写的那一行

`m16_scene.py:791` 在自检里写下 `adu = np.where(valid, fr.adu, 0.0)`，紧接着 `:793` 断言 `np.all(adu[~valid] == 0.0)`。**这个断言的第一合取项由 `:791` 自己构造，按定义恒真**——`np.where` 把 `~valid` 位置一律写成 `0.0`，断言只是把同一行代码又读了一遍。

被检验的生产实现在 `m16_scene.py:541`：

```python
# 无效像素 -> 0（与真实 drz 的零填充一致）；**必须写回 Frame**，
# 否则落盘的是未掩膜的帧（曾因此使 HDU MASK 与 SCI 不一致，交付自检抓出）。
frame.adu = np.where(valid, frame.adu, 0.0)
```

自检 `:789` 直接调 `NM.expose`，**从不调用 `render_m16_frame`**，故 `:541` 在 `--selftest` 路径上执行次数为 **0**。

**判别力归零的机理**：`:539-540` 的注释明写这一行是「交付自检抓出」的缺陷所对应的防线，但它所声称的防线与它所处的自检之间**没有任何调用边**。把 `:541` 整行删掉（渲染器不再写 0，落盘 HDU 的 MASK 与 SCI 不一致——正是注释所述的那类缺陷），`V3_mask_propagates` 仍为 True → `:867 all_pass` 仍 True → `:883` 退出码仍 0。

### 最重第 2 条 · `noise_selftest.py:534` 「加性负例」乘了字面量 0，是一条机制正确但从不执行的判据

`noise_selftest.py:534`：

```python
add_ratio = (v1 + (mm - 1.0) ** 2 * 0.0) / v1   # 加性常数不改方差 ⇒ 比值恒 1
```

`(mm - 1.0) ** 2` 被显式乘以 `0.0`，因此 `add_ratio` **对任何输入恒等于 `1.0`**。它在 `:543` 以 `additive_negative_control_ratio` 印进产物，读起来像一个真实测得的负例对照值。

而 `:504` 的 docstring 声称：「另给负例：若把平场误当加性（+m 而非 ×m），比值不闭合。」——**该负例从未被构造**。

对照：同文件 `test_C4_source_flat_production` `:1077-1083` 的加性负例是**真做的**（不传 `flat`，再除以 `m`，断言散布 > 1e-3）。同一个仓里两处写法，一处真做一处乘 0，说明这是笔误而非设计。

**为什么它属于「恒真门第④型」**：不是谓词写错，而是**机制正确却从不执行**——负例臂的代码路径一次都没进入。该项还**不进入判词**（`:544` 的 verdict 只用 `np.median(rel) < 0.05`），属于「打印但不计入汇总」的变体：产物里有一个看起来像证据的数字，实际是常量。

### 最重第 3 条 · `generate.py:90` 无条件写 `status:"OK"`，一帧未生成也报 OK

`generate.py:88-90`：

```python
n_ok = sum(1 for f in man["frames"] if f.get("status") == "OK")
n_un = sum(1 for f in man["frames"] if f.get("status") == "UNAVAILABLE")
manifest["datasets"].append({"id": d["id"], "status": "OK", ...})
```

`n_ok` / `n_un` **被算出但既不参与 `status` 判定，也不影响 `rc`**；`:101` `return rc`，而 `rc` 在这条路径上恒为 0。

配套事实（本人亲读 `render.py:581-590`）：`render_dataset` 对每帧捕获 `FileNotFoundError`，写一条 `status: "UNAVAILABLE"` 记录后 `continue`，**不向上抛**。

**控制流**：真实底文件缺失 → `render.py:581` 捕获 → 全部帧记 `UNAVAILABLE` → `generate.py:81` 正常返回 → `:88` 得 `n_ok=0` → `:90` 写 `status: "OK"` → `:101` 返回 0。**manifest 声称 OK，一帧都没生成。**

这与本仓 `cbddbb8d` 提交（*根除一处「制造绿色产物」的失效*）所处理的失效**同型**：控制台逐帧打印 `UNAVAILABLE`，摘要却是一个绿色产物，且退出码为零。同型第二处：`--only <不存在的 id>`（`:63,:68-69`）→ 循环体一次不执行 → `{"datasets": []}`、rc=0、退出 0，**无「未匹配到任何 id」检查**。

---

## 3. 逐文件清单（读了什么 → 看到什么 → 判定）

> 判定取值：**阻断**（必须修，不修则结论不可信）/ **须修** / **建议** / **无问题（已主动排除误报）**

### 3.1 `synthetic/m16_sampling.py`（1485 行，读满）

- **V6 臂(c) `:1262` `real_sigma_used_equals_requested` 是恒真的配置回声**【须修·恒真门①】
  `sig_used` 取自 `:1248 ← v10["readings"]["smooth_sigma_px"] ← :1164 ← cmeta["smooth_sigma_px"]`，而 `cmeta["smooth_sigma_px"]` 在 `:279 ← :237` 就是 `sig_k = float(ccfg.get("smooth_sigma_px", 0.8))`——**即配置值本身**。`sig_req` 在 `:1210 ← :1168` 读**同一个 cfg 的同一个键**。两侧同源，任何 `load_canvas` 的实际行为改变都不会让它变红。
  *重要限定*：`:1263 real_fraction_matches_test_closed_form` **不是**恒真——`frac_real` 由 `_smooth_canvas` 用**实际传入的 σ** 算出，`frac_real_test` 用请求值，两者不等即判红，故 σ 传错这一整类缺陷由 `:1263` 兜住。`:1262` 本身是装饰性的，但**不是漏洞**。**降级为「须修（误导）」而非阻断。**
- **V6 臂(c) `:1266` `real_surviving_noise_below_raw` 构造恒等**【须修·恒真门①】
  判 `0.0 < resid_sig < raw_sig`，而 `:255` `resid_sigma_rate = raw_sigma_rate * math.sqrt(resid_frac_theory)`，且 `resid_frac_theory = 1/(4πσ_k²)` 在任何 σ_k > 0.282 时恒 < 1。**故第二合取项由第一合取项与 σ_k 的取值共同蕴含，不可能翻红。**
- **`unknown_band_rejected` 算了但不入 checks**【须修·打印不计入汇总】
  `:1085-1091` 正确地跑负例并置 `out["unknown_band_rejected"]`；但 `:1101-1124` 构造的 `checks` 字典**没有这个键**，`:1177-1178` 只对 `checks` 求值。而 `:1179-1181` 的判词文本明写「**且未知 band 被拒**」。
  *反例*：把 `:167-168` 的 `if band not in BANDS: raise ValueError` 改成宽松 `.get()` → `:1088` 置 `unknown_band_rejected=False`（会出现在 JSON 产物里），但 `verdict` 仍 True。**判词文本声称覆盖的一项在实现中不存在。**
- **`:43-45` 模块 docstring 与 `:625-626` 落盘元数据误述生成映射**【须修·注释与代码不符】
  `:44-45` 称「生成时按 像素 → 天球 → 画布像素 两步精确映射（**astropy**）」；`truth["wcs"]["generation_map"]` 在 `:625-626` 写成 `"pixel -> all_pix2world(frame WCS) -> all_world2pix(canvas WCS) -> cubic sample, exact=True"`，**并随 `meta.json` 落盘**。实际 `detector_to_canvas :470-479` 用的是**自写解析 TAN**（`tan_deproject :378-397` / `tan_project :400-411`），astropy 仅在 `:432` `wc.all_pix2world` 用过一次。该函数自身 docstring `:466-467` 写的是「**解析**两步映射」——**是诚实的**；不诚实的恰是对审计者作出承诺的模块头与落盘元数据。
- **V9 `:1420-1452` 覆盖面过窄**【建议】
  常数画布 ⇒ `ratio9 ≡ 0.37·s²`，整条门的信息量等于「`:578` 那行 `sig_rate = sig_rate * (s ** 2)` 是否存在」。对非常数场零覆盖。
- **V8 `:1018/:1034/:1035/:1419` 目标 seeing 3.2 硬编码四处**【建议·恒红门风险】
  `:1018` 配 3.2，`:1034-1035` 再字面写 3.2，`:1419` 用它判红。若把 `:1018` 改成 3.6，`fwhm_meas≈3.6` 而期望仍是 3.2 ⇒ `rel_err=0.125 > 0.10` **恒红**，而红的原因是过期常量不是缺陷。
- **V2 `:1307` 与 B1 同型（设计使然，非缺陷）**【无问题·已主动排除】
  `robust_sigma_adu` 是中位数型（`noise_model.py:368-380`），加常数后逐位不变 ⇒ `rel` 必为 0。这是**负例臂按设计恒真**（真值=无效应 ⇒ 度量必须归零），符合清单。登记以免后续车道误判为恒真门。
- **`:1152-1154` 宽 `except Exception` 正确置红**【无问题】——未把失败吞成绿。
- **`:1073-1081` / `:1212-1217` 模板缺失记红不跳过**【无问题，正面】——与仓内「不设 waiver 开关」政策一致。
- **死代码**：`:1066-1072` 第一个 band 负例永不触发（`band` 是合法字面量），与 `:1085-1091` 重复；`:560` `fwcs` 赋值后无引用。【建议】
- **`:1473` 退出码真实反映 `all_pass`**【无问题，正面】。

### 3.2 `synthetic/noise_selftest.py`（1323 行，读满）

- **`:534` 加性负例乘 `0.0`**【**阻断**】——见 §2 最重第 2 条。
- **`:880` 用 `assert` 当门，结构上不可能失败，且会摧毁失败产物**【须修】
  `assert dq.as_dict()["saturation_adu"] == dn.as_dict()["saturation_adu"]`——两臂只差 `quantize`，而 `saturation_adu` 由 `full_well/gain/bias` 决定（`noise_model.py:91-92`），与 `quantize` 无关 ⇒ **恒真**。更严重的是 `assert` 在 `-O` 下被消掉；一旦真炸，`AssertionError` 穿出 `:1290` 的循环 ⇒ `:1298-1300` 的统计与 `:1313-1317` 的 `--json` **全部不执行** ⇒ **红灯变成「无产物」而不是「红灯」**。
- **`:1217-1220` `dark_bit_exact` 是自证式锚**【须修·自证判据】
  比较 `f.provenance["dark_current_e_per_s_used"]` 与**输入配置** `det.dark_current_e_per_s`——provenance 由 `expose` **自己**写入（`noise_model.py:277`），参照量来自被检验量自身的输出字段。且 `:1201` 用 `T=T_ref`，使 `:1204` 的一阶矩闭合在数学上**无法区分任何温度律缺陷** ⇒ C6(c) 对「温度」零判别力。温度律本身由 A4 doubling 臂 `:464-469` 覆盖，**那条有效**。
- **T5 口径（`noise_model.py:482` 一并登记）**【须修·权重低于容差 15 倍】——见 §3.6。
- **未使用参数**：`:592` A8 的 `n_pix`（`:598` 硬写 512×512）；`:978` C4 的 `n_pix` 与 `det` **均未用**（`:993`/`:1043` 硬写 shape）；`:1092` C5 的 `n_pix`（`:1109` 硬写）；`:860` C2 的 `det`。对应调用点 `:1277/:1284/:1286/:1287` 白传。【须修·死参数】
- **`:612/:683-684/:721/:1150` 把 `NM.predicted_variance_adu2`（生产函数）当「解析预测」**【建议·期望量独立性打折】
  这些是**方法面**判据（A/B 系列）用生产解析式做期望，相对独立原语仍有独立性；但严格说期望量取自被测方。建议在 C 系列中至少标注哪些闭合仍吃该函数。C5(b) `:1150` 同。
- **正面**：C 系列 `:798-1243` 每条都**直接调用生产函数** `NM.expose` / `NM.sky_surface_e_per_s`，期望量由测试端按入参现场算出（`:1204`、`:1223`、`:850`、`:1121-1122`），**不取自生产返回值**。C5(a) `:1121-1125` 用真闭式比对 `NM.sky_surface_e_per_s` 的返回场，是本文件做得最好的一条。`:1235-1236` 白噪声探针亦然。**A/B/C 分层职责在注释里写清楚了（`:788-796`），这是本片最值得肯定的设计。**

### 3.3 `synthetic/noise_model.py`（520 行，读满）

- **自检从不经过 additive 臂**【须修·机制从不执行】
  docstring `:21` 与 `:45` 宣称自检含「纯加性 ⇒ 度量归零」的负例，`:312-316` 实现了 `MODE_ADDITIVE` 分支。但自检的对照臂在 `:463` `ctrl = ref + delta_adu`，而 `:454-459` 的 `draw()` 调 `expose()` 时**未传 `mode`、未传 `additive_offset_adu`** ⇒ `:312` 条件 `mode == MODE_ADDITIVE and additive_offset_adu` 两项皆假，**该分支在自检中一次都不进入**。
  *降级说明*：T3/T4 检验的 `robust_sigma_adu`（生产函数，`noise_model.py:368-380`）的平移不变性**是真门**，负例的存在性并未因此落空。故定为**须修**而非阻断——被浪费的是「additive 渲染路径」的覆盖，不是负例的证明力。
- **T5 的 1/12 项权重远低于容差**【须修·恒真门①（弱）】——本车道独立复算：
  B=30, t=1, D=0.02, g=1.5, σ_R=5 ⇒ `var_pred = 30.02/2.25 + 25/2.25 + 1/12 = 13.3422 + 11.1111 + 0.0833 = 24.5367`；删掉 1/12 项后为 24.4533，相对差 **0.34%**，而阈值是 **5%**（`:482`）。**把量化项整个删掉，门仍绿。** docstring `:422`「T5 增益量化：方差闭合含 1/12 ADU^2 项」名不副实。
  *缓解*：量化项由本仓 `noise_selftest.py` 的 **C2 `:860-899`** 精确检验（配对恒等式直接测 `Var(round(x)-x)` 对 1/12，容差 2%），**那一条是真门**。故 T5 是弱门而非缺口。
- **T7 半自证**【建议】：`:493-496` 把 `dark_current_at` 自身定义式（`:96`）代入，只能抓符号翻转/返回常数，抓不到 `dark_double_temp_c` 默认值错或单位错。
- **`Detector.hot_pixel_fraction` / `hot_pixel_dark_gain` 是永不被读的字段，且会进入产物**【须修·provenance 谎报】
  `:87-88` 定义，但 `expose` 只吃 `hot_map` 参数（`:264-265`），热图由 `render.py:466-468` / `m16_sampling.py:588-590` 从 `artifacts` 构造。场景若写 `detector:{hot_pixel_fraction:0.01}`，该值经 `det.as_dict()`（`:101`）进入 `truth["detector"]` 与 `prov["detector"]`（`:276`）**写进产物 JSON，而实际一个热像素都没生成**。
- **T1 不调 `expose`**【建议】：`:432` 直接 `rng.poisson`，验的是 numpy 而非本模块。
- **`MOFFAT4_SIGMA_OVER_FWHM`（`:61`）全 7 文件零引用，而 `render.py:139/:152` 硬写同一字面量 `1.230310`、`render.py:155` 却用 `NM.GAUSSIAN_FWHM_OVER_SIGMA`**【建议·同一物理量两种口径】。
- **主动排除误报**【无问题】：docstring `:10`「源/天光/暗电流各自独立」与 `:281` `poisson_terms` 对代码 `:296` 单次 `rng.poisson(lam_e)`——**这是对的**，独立 Poisson 之和 ≡ Poisson(和)。**不要改。**
- 伪引候选（本人无法裁决，标 **待核**）：`:5`「负责人 2026-09-19 强制要求，GAP_AUDIT §9.41/§9.47」、`:38`「§9.42：标定目标是测光坐标系，物理单位无意义」。**裸从句，无成对引号**，指向仓外文档 `GAP_AUDIT`，不在本片内。

### 3.4 `synthetic/m16_scene.py`（897 行；本人读 240–897）

- **`:791/:793` V3 掩膜传播门测测试自己那行**【**阻断**】——见 §2 最重第 1 条。
- **`:550` `base_transport_tol <= 0` 是静默 waiver 开关**【须修·与仓内政策矛盾】
  `if float(sc.get("base_transport_tol", 1e-9)) > 0:` —— 场景里写 `0` 即整条 V6 常驻守卫**静默跳过**，而 `:573` 仍把该 tol 写进 `truth["base_rate_transport"]` 当作「已检」证据。本仓 `m16_sampling.py:1056-1059`、`:1077-1078`、`:1205`、`:1454` 四处明写「不设 waiver 开关」，此处与之自相矛盾。
- **`:463-466` `sel` 全空时返回 `max_abs_rel_dev: 0.0`**【须修·未封堵的空过路径】
  于是 `:551` 的守卫空过。当前靠 selection floor（`:459`）实际难触发，但**一旦触发就是「0 ⇒ 通过」**，是未封堵的空过而非设计。
- **`:536-537` 透传 `variance_override_e2` / `additive_offset_adu` 两臂**【建议】：全仓自检零覆盖；`:622` 头里 MODE 明写 `...|variance_override`，即 `additive` 在 m16_scene 是**合法渲染臂**，与 `m16_sampling.py:31`「additive 仅作显式负例臂」的口径需要在正本层面对齐。
- **V6 `:824/:836-839` 是第③型往返自证的残余风险**【建议】
  `base_rate_transport_residual`（`:438-442`）按 `truth_e/(t·m) - sky - D/m` 反解，而 `truth_e` 由 `expose` 的 `lam_e = t*(src+sky)*m + t*D` 产出——**反解式与产生式是同一对式的正逆运算**。`:463-466` 的条件数选像素（`:444-459` 的大段注释）处理了数值条件数问题，但**没有解决「两侧同源」这一结构问题**。正例臂 `:837` 与负例臂 `:838` 互为对照，负例臂能证明判据有功效，故判为建议而非阻断。
- **`:331/:344/:345` 星表位置由 `rng` 决定，`seed` 已登记**【无问题】——`:562` `seed` 进 `truth`。
- **`:261-264` 掩膜缺失静默降级为全 valid，无门检查**【建议】——与 `m16_sampling.py:229-232` 同模式；且 `m16_sampling.py` 的 V10 `checks` 里也没有 `mask["used"]` 项。**掩膜是否存在，对判据而言不可见。**

### 3.5 `synthetic/render.py`（634 行；本人读 315–634）

- **`:425` `canvas_cache` 跨帧键不含逐帧合并后的 `sc`**【须修·静默数据损坏】
  `key = ("src", id(scene), cshape)`，而 `:405` `sc = deep_merge(scene, ov)` 是逐帧合并的。`render_dataset :573-580` 对所有帧共用同一 `scene` 对象与同一 `cache`。⇒ 若 `scene["frames"][k]` 覆盖了 `nebula` 或 `stars`，**第 k 帧静默复用第 0 帧的画布**；`:430` 的 `cat` 也取自缓存，`:486` 的 `stars_in_frame` 随之自洽 ⇒ **元数据看不出任何异常**。
- **`:581-590` 捕获 `FileNotFoundError` 不抛出，产出 UNAVAILABLE 记录**【须修·须与 `generate.py:90` 合并修】
  单看这一层「如实登记」的注释是诚实的，但与 §2 最重第 3 条合并后构成绿色产物路径。**单独看无缺陷，合并看是阻断。**
- **`:408` `rng_scene` 决定被注入的真值星表，却不进 `truth`**【须修·provenance 缺项】
  对照 `:503` 登记了 `flat.seed`，`:466` 的 `hot_seed` 同样未登记。违反本模块自己的契约「把**用了什么**逐项写进产物」。
- **`:518-519` 饱和计数口径**【无问题·已主动核过】
  `frame.adu >= det.saturation_adu + add_off` 与 `noise_model.py:312-316`（加性偏移在**钳位之后**施加）**一致**。子代理提出过疑点，本车道核对后**否决**——此处无缺陷。

### 3.6 `synthetic/m16_mask.py`（592 行；本人读 293–592）

- **`:468-526` 自检从不调用 `build_frame_mask`**【**阻断**】
  自检在 `:498-505` **内联重抄**了生产的判据并对内存数组求值（`row0` 取默认 0、无分块、无 halo、无边框）。生产路径中结构性不可达的部分：分块统计对齐（`:214-218`）、`row0=y0` 流式上采样（`:233-234`）、`flags[y0:y1] = f`（`:253`）、EDGE 连通域标记（`:254-263`）、`valid` 计算（`:264`）、`max_rect_pixels`/`max_axis_aligned_rect`/`expand_rect_pixelwise`（`:269/:272/:366/:388/:399`）、`write_mask_products`/`load_valid_mask`——**全部零覆盖**。这正是清单第 ⑦ 项「判据读的是本地桩而非生产实现」。
  *反例*：把 `:233` 改成 `row0 = y0 + block`（生产流式上采样错块），`--selftest` 仍全绿退出 0。
- **`:507` 报出的分母与被门覆盖的分母不一致**【须修·计数口径】
  `injected_bad = len(spikes)+len(negs)+len(zeros) = 3+2+3 = 8`；但三个 **ZERO** 注入点（`:485-486`）**不被任何 verdict 覆盖**——`:519/:520/:521` 三条只管 SPIKE 召回、星点误报、EXTREME_NEG 召回。**报出 8，实际进门 5。** 零像素与 EDGE 判据整体零覆盖。
- **`:541-543` + `:588` 帧缺失记 MISSING 但不改退出码**【须修·伪绿】：三个波段全缺 ⇒ manifest 三条 MISSING + 退出码 0。
- **`:298` `prim = np.zeros(6)` 只用 5 槽**【建议】：`prim[5]` 恒 0。
- **`:174` `build_frame_mask` 的 `verbose` 参数在函数体内零引用**【建议】：`:544` 传入即丢弃。

### 3.7 `data/synthetic/generate.py`（105 行，读满）

- **`:90` 无条件 `status:"OK"`**【**阻断**】——见 §2 最重第 3 条。
- **`:63/:68-69` `--only` 未匹配静默成功**【须修】：`{"datasets": []}`、rc=0、退出 0。
- **`:82` 宽 `except Exception` 但记 FAIL 且 `rc=1`**【**无问题·已主动排除**】
  子代理正确指出它与 `render.py:581` 的差别是决定性的：这里会红，那里不会。**`:80-87` 的处理是本片内正确的错误处理范式。**
- **`:38` 顶格 `import m16_scene as M16`**【建议】：连 `--list`（`:59-62`）都硬依赖它可导入。
- **`:17-21` 注释里的不变量无门守护**【建议】：「两者现均为 32」在本文件既不可验证也不被断言；上游漂移到 16 本入口照样绿灯。**这是写在注释里的不变量，不是判据。**
- **裸从句伪引（待核）**：`:15`「判据与示范实验在 实验/additive-sky-seamless/code/reverse_verify/data_matrix/」、`:17`「STACKN32-001（A4 前台裁决，2026-09-20）」。后者带**裁决编号 + 日期**，与 AGENTS.md §5「正文无日期、版本号、任务流水编号、历史叙事」冲突——须前台裁定归属层。

### 3.8 `synthetic/run_selftests.sh`（91 行，读满）

- **`:41` 过滤分支 `return 0` 且不增任何计数器**【**阻断·可制造绿色产物**】
  `bash run_selftests.sh m16` 会过滤掉 `noise_selftest`（名字不含 `m16`）→ `:80` 打印「通过 3 / 红灯 0 / 缺失 0」→ `:90` 打印「物理链自检全绿，实验读数可作为证据」→ `:91` `exit 0`。
  **该措辞与四组件全绿完全相同，但脚本头 `:8-12` 自述承担「验方法」那一半证据的组件根本没跑。**
- **`:80` 汇总从不打印分母**【须修·计数口径】
  「通过 $pass / 红灯 $fail / 缺失 $missing」——读者无法知道本应跑几个。**应改为 `通过 $pass / 共 $((pass+fail+missing))`。**
- **脚本只调 4 个文件（`:62/:65/:68/:72`），`render.py` / `noise_model.py` / `synth_gain.py` / `gainlib.py` / `generate.py` 一个都没被调用**【须修】
  ⇒ `noise_model.py` 自有的 T1–T7 门（`:414-516`，有独立 `all_pass` 与退出码）**不在一键复现路径上**。**那些仅有的真门运行时谁都不看。**
- **`:45` 临时日志无 `trap`、不清理、路径无 PID/时间戳**【须修】：过滤运行时被跳过的组件会留下**上一轮的旧日志**，而 `:51` 明确指引读该路径 ⇒ 历史 RED 可被误读为本轮证据。
- **`:28` 无 `set -e`；`pipefail` 是装饰性的（全脚本零管道）**【建议】——当前被 `run_one` 的显式 `if` 抵消，但将来加命令即成静默失败源。
- **主动确认（无问题）**：`:48/:52` 的 `pass=$((pass+1))` 写在**当前 shell**（命令本身才在子 shell `( cd … && "$@" )` 内）⇒ **计数器正确累加**。这是常见陷阱，此处处理对了。`${1:-}` 防了 `set -u` 崩溃。退出码 0/1/2 三路均有明确出口。

### 3.9 `synthetic/gainlib.py`（241 行，读满）

- **`:213-214` `align_shape` 强制残差零均值**【建议】：`shape_error_pct.rms_pct`（`:226/:229`）因此**永远看不到系统偏置**；用法上必须强制与 `:231` 的 `bias_dex` 并读。
- **`:95-96` / `:102-103` 失败路径返回哨兵 `(None, 0.0, 0, 0)` 而非抛异常**【建议】：上游必须自己记得判。`synth_gain.py:219` 判了，但**静默丢弃**（见 §3.10）。
- **死码**：`:217-219` `shape_err_pair` 是 `:222` 的纯别名且无调用者；`:234-241` `robust_ptp_pct` 无调用者；`:159` `self.c0` 存了从不读。【建议】
- **裸从句伪引（待核）**：`:86`「sigma = MAD(加权残差)/0.6745（与生产 **`star_matcher.cpp:616-627`** 同口径）」——**带精确行号的跨模块引用**，`star_matcher.cpp` 不在本片内。
- **正面**：`:69-70` `MAD_SCALE = 0.6744897501960817`、`TUKEY_C = 4.685` 为标准值；`:26-33` `basis_terms` 与 docstring `:16-19` **逐条一致**；`:112` Tukey biweight 形式正确；`lstsq` 全程带 `rcond=None`，无隐式秩截断。

### 3.10 `synthetic/synth_gain.py`（386 行）——**本人未读，以下全部来自子代理，本车道未独立复核**

- `identifiability()` 的两条退化度量（`:318` `bg_max_abs_dy_adu`、`:322` `bg_degeneracy_max_abs_dy_adu`）疑为代数恒等式。【须修·待复核】
- `:228` `lr_corr = log_ratio_true + fitB.log_gain − fitA.log_gain` 用**真值场**构造「校正后」的场，再据此报 `:243/:245`；而 `:232` 的星级 `d_corr` 用观测量。**同一份输出里两个「校正后」不是同一个东西。**【须修·待复核】
- `:219-220` `if fitA is None or fitB is None: continue` 静默丢弃拟合失败，**无计数、无告警、退出码恒 0**。【须修·待复核】

### 3.11 文档 / 参考文献 / 场景 JSON —— `3f22ee01` 交付后补记（**全部转引，本车道未复核行号**）

**最重 3 条（并入 §4.1 阻断 B6–B8）**：

- **R1｜两个「负例」的真值逻辑相反，而登记在册的那个是恒真门**【**阻断**】
  `scenes/common_mode_overlap.json:4` 宣称「6 帧…**只有噪声实现不同** —— 真值为『无帧间空间形状差异』，任何形状差异度量**必须归零**」；但 `:61-80` 的 6 个 frame 条目**只有 `frame_id`**，无 per-frame seed/pointing/exposure/psf/sky ⇒ 6 帧是同一真值模型、同一几何的重复渲染，**任何帧间形状差异度量在此结构上只能由噪声涨落驱动，构造上不可能非零**（恒真门①结构恒等 + ③往返自证）。它被 `datasets.json:76-87`（`negative_control:true`）+ `data/README.md:49` 正式登记并绑定判据 `exp2_mosaic_shape_difference`（`datasets.json:84`）。
  对照 `scenes/m16_sampling_overlap_common.json:48` 同样宣称「必须归零」，但 `:88/100/112` 三帧带亚像素抖动（`dither_label: sub_0.50_0.25` 等）+ `:19 resample_order:3` ⇒ 对 β=4 Moffat 做亚像素相位三次样条重采样**必然**产生非零逐帧残差 ⇒ **这条真值在物理上不成立**；该场景 `matrix_cells:54` 同样挂 `negative_control:true` **但未登记进 datasets.json**。
  ⇒ **仓里两个负例，一个恒真、一个真值写错；恒真的恰是唯一被登记的。** `data/README.md:66` 的「每类数据都要能红能绿」在登记口径下不成立。
- **R2｜一条「全仓命中 0」的核对结论，被它自己声明的检索命令当场推翻**【**阻断**·伪证据】
  `references/REVERSE_VERIFY_BIBLIOGRAPHY.md:330`：「本工作区**无**（全仓 `find -iname '*hst*' / '*hubble*'` **命中 0**）⇒ 真实数据实验改用真实实拍 M42 帧。」
  实测 `find . -iname '*hst*' -o -iname '*hubble*' | grep -vc node_modules` = **296**，含本片子树内的 `scenes/hst_m16_realbase.json`（`:6` 自述「负责人上传的 3 帧哈勃 M16 作底」，`:41` `real_base.path` 直指 `testdata/HST_M16/*.fits`）。⇒ **意图可能成立，但检索方式与结果都错，且一条范围决策建立在从未按声明方式执行过的检索上。**
- **R3｜4 份被引的一手来源 / 上游文档在本仓不存在**【**阻断**·上游指针断裂】
  实测 MISSING：`docs/ASTROCS_DESIGN.md`（实际是 `docs/ACSD_DESIGN.md`）、`docs/ENGINEERING_SPEC.md`、`tools/doccheck/dangling_ledger.json`（`MIGRATION.md:76` 整段论证的支点）、`run/RELEASE-02/paper/data/STACKN32/before_stackn32/`（119 MB 前后对照）、`run/RELEASE-02/paper/data/M16FIX/src/a6_before_repro.py`（`CHANGES.md:35-37` 记录了对其的代码改动）。另 `BIBLIOGRAPHY.md:236-237` 的两条路径与 `.bib:386-387` 指向**另一条路径**。⇒ 违反 AGENTS.md §3「追溯不到上层的机制，先补上层要点」。

**反例 G（本车道未复算，采信子代理）· 「噪声底偏乐观 5.66×」的口径缺陷**
`scenes/m16_nebula_core.json:37` 断言「stack_n=1（3.1 e⁻）使**噪声底偏乐观 5.66x**」。复算：√32×3.1 = 17.536 e⁻，读出项比确为 5.657× ✅；但**总噪声底** = √(σ_R²+σ_dark²)，而 σ_dark = 0.002×9600 = **19.2 e⁻**（本就大于读出项）⇒ stack_n=1 总 σ = **19.45 e⁻**、stack_n=32 总 σ = **26.00 e⁻**，真实倍数 **1.337×，不是 5.66×**（差 4.2 倍）。该数字被 `datasets.json:3` 原样复述。**口径缺陷：句子写「噪声底」但数字只对读出项成立。**

**反例 H · 旧版仍在被引用，且旧版本身内部不自洽**
`datasets.json:3` 的 `$stack_n` **主动指向** `before_stackn32/`；旧版 `m16_nebula_core.json:19 "stack_n": 1` 与 `:30 "full_well_e": 2240000` 并存，而同文件 `:37 calibration.rule` 定义 `full_well_e = NDRIZIM*70000 = 32*70000` ⇒ **旧版拿 1 次叠加的读出噪声配 32 次叠加的饱和预算**，任何拿 before/after 比饱和像素或满阱的结论都无效，而 `CHANGES.md` 未披露。缓解已到位（目录名、`stack_n:1`、不登记），故判「须修说明」而非删文件。

**其他须修（摘要）**：
- `REVERSE_VERIFY_MIGRATION.md` **整篇是历史叙事**（日期、「退役」、20 行旧→新路径表），**直接违反 AGENTS.md §5**；`:96-98` 三行表格经 `realpath -m` 实测**全部塌缩到同一文件**而 `:92` 声称是「对应单元的」⇒ 表与前言矛盾，且 `../` 写法刻意掩盖塌缩；`:37` 声明 `.gitkeep`「退役」而它实际存在；`:82-85` 用「我方零 git 写权限」解释 DOC-INDEX 必红 ⇒ **流程性假红**，任何把它计入证据的门都会失真。
- `BIBLIOGRAPHY.md:73` vs `:156` 对**同一条 SExtractor「逐字核对」句给出两种文本**（`:73` 是 `:156` 的截断）⇒ **至多一个是逐字连续引用**。【待联网核验】
- `BIBLIOGRAPHY.md:251` Bosch 2018 逐字句「where b is the level of the background (before it is subtracted)」**裸引、无引号** —— 正是清单点名的形态。【待联网核验】
- `BIBLIOGRAPHY.md:315-318` PixInsight 引文**跨行拼接且语法断裂**，疑为两处非相邻文本合并。【待联网核验】
- `BIBLIOGRAPHY.md:331` 用「Confluence CQL 检索 `totalSize: 0`」论证某说法**不存在** ⇒ **检索未命中 ≠ 不存在**（CQL 非全文索引），论证不成立。
- `.bib` **5 对重复条目**（同一 DOI/arXiv 两套 key，BibTeX 工具会产出重复参考文献）；`:84` 把 Jähne 2010 / Borek 2023 塞进 note 当参考文献；`:380` 与 `.md:126` 同一模块两个「官方」URL（其一已 404）。
- `synthetic/README.md:106` `m16_sampling.py` 判据条数写「——」而 `:61` 已写 V1–V7 ⇒ **总数应为 32（6+7+7+12），表里只能算出 25**；`:118-120` 判别力证据指向 gitignore 的 `run/`，**支撑「已实测」的证据不在仓内**；`:99` 「全绿 144 s，noise_selftest 143 s」⇒ 另三个组件合计只剩 1 s，**算术可疑**。
- `data/README.md:14-23` 目录树**漏掉整个 `before_stackn32/`**；`:68-74` 判据表 5 条中 **3 条的「结果」列填的是分片名 `DATA-TYPE-MATRIX` 而非结论**；`exp_variance_closure` 被 datasets.json 引用数 = **0**（孤儿判据）。
- `data/real/README.md:50-51` V1 判据是**往返自证型**（同一模块同一公式两份实现互校，公式错或 PHOTZPT 错时两臂同错 ⇒ **V1 不可能红**）；`:32/:44-48/:71-81/:87/:95-98/:103` 实测数字**全部手抄一遍**，而 `:11` 说同一批也在 `m16_scene_index.json` ⇒ **双份手维护、无单一真值源**。
- 场景 JSON 共同项：**三套坐标约定**无单位标注（`pointing:{y,x,dy,dx}` / `pointing:{offset_px:[x,y]}` / 无标签数组），且 `pointing.x/y` 实为 0.04″/px 而 `pixel_scale_arcsec` 是 0.2″/px · `m16_starfield.json:45` 与 `m16_dark_lowsnr.json:45` 把 **F657N 的文案逐字复制到 F502N/F673N**（子曝光 300s vs 实际 500/450s），且同文件 `:44` 自己写 `saturated_pixels_after: "0 px"` ⇒ **同文件内自相矛盾，且该错误在 `before_stackn32/` 旧版同样存在 ⇒ 跨两次交付未被发现** · 4 个 `m16_*` 顶层 `seed`/`psf`/`sky` 被逐帧全覆盖 ⇒ **顶层死配置**（改顶层不生效的静默陷阱） · `sweep_exposure.json:4` 声称验「SNR ∝ √t」，复算得 SNR 随曝光**单调下降**（t=30→19.2、600→6.2）⇒ **描述物理错误**（目前 `criteria: []` 未成红灯） · `mosaic_diff_pointing_realbase.json` 声称「不同指向」但 frames **完全没有 `pointing` 键**，全靠 `glob_index` 去 glob gitignore 的 `run/` ⇒ **判别依据既不在仓内、也不由文件系统顺序钉住**。
- **10 条待联网核验**已在 `3f22ee01` 报告中逐条列明（含 SExtractor、Bosch、PixInsight、Andrae、Jones 年份、Jacob Montage 出处、PhotometricMosaic 权威 URL 等），本车道**不裁决**。

**正面（记账以正）**：`.md` ↔ `.bib` 的 DOI/年份/卷页**逐条一致**，70 条全对上，0 孤儿 0 悬空 · `CHANGES.md:18-20/:25` 的场景改动清单**经 `diff` 完全证实**（4 文件 × 恰好 3 处差异，无第四处） · `data/real/README.md:39-48` 光度定标数值**独立复算全部吻合**（F657N ZP_AB 算得 **22.63501**，与 `:50` 声称完全一致） · `m16_band_matrix.json:45` 的 141/1048576=0.01345%、2720/1048576=0.25941% 吻合 · `datasets.json:3`「非 m16_* 数据集不含 stack_n」逐个核实**属实** · 子代理**主动撤回了自己对 `:210` shift=14 的误判**（复算确认 8192/64=128=2⁷、2×7=14 正确）——**这种「先准备报错、复算后撤回」正是本项目要求的证据纪律，予以记账为正**。

---

## 4. 发现清单（按严重性）

### 4.1 阻断（8 条）

| # | 位置 | 一句话 | 机理类型 |
|---|---|---|---|
| B1 | `m16_scene.py:791/793` | V3 掩膜传播门测的是测试自己写的 `np.where`，生产 `:541` 零执行 | 恒真门②（结构对称）+ 判据脱离生产实现 |
| B2 | `m16_mask.py:468-526` | 3 条 verdict 无一行执行生产代码，全在 `:498-505` 内联重抄 | 判据读本地桩（清单⑦） |
| B3 | `noise_selftest.py:534` | 加性负例 `(mm-1.0)**2 * 0.0`，负例臂从未构造 | 恒真门④（机制从不执行） |
| B4 | `generate.py:88-90` + `render.py:581-590` | 全部帧 UNAVAILABLE 时 manifest 仍写 `status:"OK"`，退出码 0 | 打印不计入汇总 + 吞异常（合并路径） |
| B5 | `run_selftests.sh:41` + `:80` + `:90` | 过滤运行与全量运行输出**逐字相同**的「全绿」结论，且汇总从不打印分母 | 制造绿色产物 + 计数口径 |
| **B6** | `scenes/common_mode_overlap.json:4` + `:61-80` | 6 帧只有 `frame_id`、无任何逐帧变量，却宣称「形状差必须归零」并被 `datasets.json:76-87` 登记为 `negative_control:true` | 恒真门①结构恒等 + ③往返自证 |
| **B7** | `references/REVERSE_VERIFY_BIBLIOGRAPHY.md:330` | 「全仓 `find -iname '*hst*'` 命中 0」实测为 **296**，一条范围决策建立其上 | 伪证据（负面检索被推翻） |
| **B8** | `docs/ASTROCS_DESIGN.md` / `docs/ENGINEERING_SPEC.md` / `tools/doccheck/dangling_ledger.json` / `run/RELEASE-02/...` | 4 份被引上游文档/数据在本仓 **MISSING**，其中 `dangling_ledger.json` 是 `MIGRATION.md:76` 整段论证的支点 | 上游指针断裂（AGENTS §3） |

（B1–B5 为本车道亲读原文；**B6–B8 来自 `3f22ee01`，本车道未复核行号**。）

### 4.2 须修（18 条）

`m16_sampling.py`：`:1262` 配置回声恒真门① · `:1266` 构造恒等恒真门① · `:1085-1091` 不入 checks（判词文本声称覆盖）· `:43-45`+`:625-626` 落盘元数据误述 WCS 生成路径。
`noise_selftest.py`：`:880` `assert` 当门且摧毁失败产物 · `:1217-1220` 自证式 provenance 锚 · 未使用参数 5 处。
`noise_model.py`：`:454-459` 自检从不进入 additive 臂 · `:482` T5 的 1/12 项权重 0.34% vs 容差 5% · `:87-88` 永不读的字段却进 provenance。
`m16_scene.py`：`:550` `base_transport_tol<=0` 静默 waiver · `:463-466` `sel` 空时返回 0.0。
`render.py`：`:425` cache 跨帧键不含 `sc` · `:408` 星表种子不入 provenance。
`m16_mask.py`：`:507` 分母 8 vs 实际 5 · `:541-543`+`:588` 缺失不改退出码。
`generate.py`：`:63/:68-69` `--only` 未匹配静默成功 · `:38` 顶格 import。
`run_selftests.sh`：`:80` 不打印分母 · 只调 4/9 文件 · `:45` 旧日志可被误读为本轮证据。
`noise_model.py`：`:87-88` 同上（合并计数）。

### 4.3 建议（14 条）

V8 的 3.2 四处硬编码（恒红门风险）· V9 覆盖面窄 · `:1066-1072`/`:560` 死代码 · `:349` `sub` 在 `real_base.path` 为空时可能未定义 · T7 半自证 · T1 不调 `expose` · `MOFFAT4_SIGMA_OVER_FWHM` 死码 + 两套口径 · `:174` `verbose` 零引用 · `:298` `prim` 死槽 · `:38` 顶格 import · `:28` 无 `set -e` · `align_shape` 零均值约定 · `tukey_irls_fit` 哨兵返回。

### 4.4 主动排除的误报（重要，避免后续车道重复劳动）

1. `noise_model.py:10/:281` 「各自独立」对 `:296` 单次 `rng.poisson` —— **正确的**，独立 Poisson 之和 ≡ Poisson(和)。不要改。
2. `m16_sampling.py:1307`（V2）、`noise_selftest.py:658`（B1）、`m16_scene.py:810`（V5）、`noise_model.py:468/470`（T3/T4）的「加常数 ⇒ sigma 不变」为 0 —— **负例臂按设计恒真**，符合清单，不是恒真门缺陷。
3. `render.py:518-519` 饱和计数 `>= saturation_adu + add_off` 与 `noise_model.py:312-316` 一致 —— **无缺陷**（子代理曾疑点，本车道核对后否决）。
4. `run_selftests.sh:48/:52` 计数器写在当前 shell —— **处理正确**，是常见陷阱但此处没错。
5. `generate.py:82` 宽 `except` —— **会红**（记 FAIL + rc=1），与 `render.py:581` 的决定性差别。
6. `gainlib.py:26-33` 与 docstring `:16-19` —— **逐条一致**。
7. 全部随机种子固定且与进程调度解耦 —— **可复现，此项无缺陷**。

---

## 5. 主动构造的反例

### 反例 A（对 B1）· 删掉生产掩膜写回，自检仍全绿

**注入**：删除 `m16_scene.py:541` 整行 `frame.adu = np.where(valid, frame.adu, 0.0)`。
**控制流**：`--selftest` → `selftest()` `:704` → V3 走 `:789 NM.expose(...)` → `:791` 测试端自己 `np.where` → `:793` 两个合取项均真 → `:867 all_pass=True` → `:883 return 0`。
**结果**：渲染器不再把无效像素写 0，落盘 HDU 的 `MASK` 与 `SCI` 不一致（`:539-540` 注释所述的正是该缺陷），**V3 对它零判别力**。

### 反例 B（对 B2）· 生产流式上采样错块，自检仍全绿

**注入**：把 `m16_mask.py:233` 改成 `row0 = y0 + block`。
**控制流**：`--selftest` → `selftest()` `:498` 调 `block_robust_stats(bad, block)` 得整图块统计 → `:499` 调 `upsample_blocks(m, block, (ny, nx))`，**第四参 `row0` 取默认 0**。生产循环 `:199-253` 与自检**无任何调用边**。
**结果**：三条 verdict 仍全 True，退出码 0。真实帧上的掩膜几何（旋转足迹、非矩形有效域）与自检里的 512×512 平场合成图毫无关系。

### 反例 C（对 B3）· 加性负例改成真做，产物数字才变

**注入**：删掉 `noise_selftest.py:534` 的 `* 0.0`，改为真正的加性臂（对 `bad` 做 `+m` 再测方差比）。
**现状**：`additive_negative_control_ratio` 恒为 `1.0`，因为 `:534` 字面乘了 0。**不论 `flat` 被误当加性还是乘性，这个读数都是 1.0。**

### 反例 D（对 B4）· 一帧未生成，manifest 报 OK

**注入**：把某场景的 `real_base.path` 指向不存在的文件。
**控制流**：`render.py:581` `except FileNotFoundError` → `man["frames"]` 全部 `status="UNAVAILABLE"` → `generate.py:81` **正常返回**（无异常） → `:88` `n_ok=0`、`:89` `n_un=N` → `:90` 写 `"status": "OK"` → `:101 return 0`。
**结果**：`generate_manifest.json` 里每个 dataset 都是 `status: OK`、`n_ok: 0`，shell 拿到退出码 0。

### 反例 E（对 B5）· 少跑一个组件，结论逐字不变

**注入**：`bash run_selftests.sh m16`。
**控制流**：`:41` `[[ "$name" != *"m16"* ]]` 对 `noise_selftest` 为真 → `return 0`，**不增 `pass`、不增 `fail`、不增 `missing`** → `:80` 打印「通过 3 / 红灯 0 / 缺失 0」→ `fail=0`、`missing=0` → `:90` 打印「物理链自检全绿，实验读数可作为证据」→ `:91 exit 0`。
**结果**：与四组件全绿的**输出逐字相同**（除计数数字外），读者无从分辨。

### 反例 F（对 `noise_model.py:482` T5）· 删掉量化项，门仍绿

**注入**：把 `predicted_variance_adu2`（`noise_model.py:344`）的 `+ QUANTIZATION_VARIANCE_ADU2` 删掉。
**控制流**：T5 `:477-478` 用同一个被改的函数算 `var_pred`，`:481` 比对，`:482` 判 `abs(rel_dev) < 0.05`。
**结果**：期望量与被检验量**同源改写**，偏差从 ~0 变成 **0.34%**，远低于 5% ⇒ **T5 仍绿**。
*注*：这不是 B 类的「真漏洞」，因为量化项由 `noise_selftest.py` C2 精确检验（容差 2%，配对恒等式）。但它说明**T5 名义上检验的那一项，它检验不出来**。

---

## 6. 盲复算记录

**口径一 · 顶层 verdict（`all_pass` / `n_pass` 真正聚合的单位）**

| 组件 | verdict 条目数 | 出处 |
|---|---|---|
| `m16_sampling.py` | **12** | `res["verdicts"]` V1–V10 + V3c 除外 |
| `noise_selftest.py` | **18** | `:1269-1289` 的 tests 表，A1–A8/B1–B4 = 12，C1–C6 = 6 |
| `noise_scene.py`（`m16_scene.py`） | **7** | V1–V7 |
| `noise_model.py` | **7** | T1–T7 |
| `m16_mask.py` | **3** | `:519/:520/:521` |
| **合计（顶层 verdict）** | **47** | |

- `noise_selftest.py:1266` 自报的「现存用例数：12（A/B）+ 6（C1–C6）= 18」—— **复算正确**。
- `m16_sampling.py:1328` 的 V3c 自述「如实登记量级，不作判据」，未计入 12 —— **复算正确**。

**口径二 · 去重后独立判别族**

| 族 | 合并项 | 独立族数 |
|---|---|---|
| `m16_sampling.py` | V10 与 V6 共享真实模板读数（V6 复用 V10 的 `cmeta`，不重复读 268MB FITS） | 12 − 0.5 ≈ 11.5 |
| `noise_selftest.py` | A2/A3/A8 各自独立；C5 与 A8 都测梯度但一条走生产一条走独立臂，**不可合并** | 18 |
| `m16_scene.py` | V1 与 V2(b) 都用 `zp`；V7 依赖 V2 的 `rate_to_ab_mag` | 7 |
| `noise_model.py` | **T5 ⊂ T2**（`sky_sigma_adu = sqrt(predicted_variance_adu2(src=0,…))`，且 T5 的 `clipped_std_adu²` 就是 T2 在 B=30 处的同一估计量、同一阈值）；**T4 ≡ T3**（换分母的同一代数式） | 7 → **5** |
| `m16_mask.py` | 3 条彼此独立 | 3 |
| **合计（去重族）** | | **≈ 44.5** |

**口径三 · 真正打在生产函数上的族**

- `m16_mask.py`：3 条 verdict **全部**不打在生产实现上（§3.6 B2）⇒ **0**。
- `noise_model.py`：T1 不调 `expose`（`:432`）；T3/T4 绕过 additive 臂；T7 只调 `dark_current_at`；T2、T5、T6 调 `expose` ⇒ 去重后 **2 族**（{T2,T5}、{T6}），而 T5 弱（见反例 F）。
- `m16_scene.py`：V3 不打生产（§2 B1）；V5 只打 `robust_sigma_adu` ⇒ 实际 **6**。
- **本片 47 条顶层 verdict 中，至少 5 条（B1、B2、B3 + T5 + `m16_sampling:1262`）对真实缺陷零或近零判别力。**

**口径四 · 其他计数（本车道亲自复算）**

- `m16_mask.py` 报出 `injected_bad = 8`（`:507`），实际被门覆盖 **5**（3 个 ZERO 注入点无 verdict）—— **分子分母口径不一致**。
- `run_selftests.sh:80` 汇总**从不打印分母**——本片唯一的 shell 汇总也没有分母口径。
- `run_selftests.sh` 覆盖 **4 / 9** 个本片代码文件。
- `synth_gain.py` **0 verdict**，且 `main()` 无 `return` ⇒ 退出码恒 0（来自子代理，**本车道未复核**）。

**引用纪律**：本片任何「N 条判据通过」的说法，**分母最多 47（顶层 verdict），有效分母 ≤ 42**。三个口径不可互换。

---

## 7. 子代理派发记录与否决情况

### 7.1 派发

派发 3 个子代理（工具对每次调用返回了重复实例，重复副本已 `interrupt_agent` 中止，**去重后实派 3 个**）：

| ID | 授权范围 | 状态 |
|---|---|---|
| `673d4a30` | `m16_sampling.py` / `noise_selftest.py` / `m16_scene.py`（3 705 行） | 已交付，读满 |
| `4774e795` | `render.py` / `m16_mask.py` / `noise_model.py` / `synth_gain.py` / `gainlib.py` / `generate.py` / `run_selftests.sh`（2 569 行） | 已交付，读满 |
| `3f22ee01` | 7 份文档 + `datasets.json` + 27 个场景 JSON（≈4 403 行） | **审稿进行中**（截至本件定稿未交付） |

**并集去重覆盖 = 6 274 / 10 856 = 57.8%**，覆盖本片全部 9 个代码/脚本文件；文档面与场景数据面缺口未被本车道补上（§1.3 已列明）。

### 7.2 否决与降级记录（逐条）

| 子代理结论 | 本车道裁决 | 理由 |
|---|---|---|
| `673d4a30`：把 `m16_sampling.py:1262` 列为**阻断 2**，并给出反例「改 `:238` 传 `0.5*sig_k` ⇒ V6 九项全绿」 | **部分否决，降级为须修** | 该反例**不成立**：`:1263 real_fraction_matches_test_closed_form` 比较的 `frac_real` 由 `_smooth_canvas` 用**实际传入的 σ=0.4** 算出（≈0.497），而 `frac_real_test` 用请求值 0.8 算出（≈0.124）⇒ 偏差远超 `1e-12` 容差 ⇒ **`:1263` 会判红**。σ 传错这一整类缺陷由 `:1263` 有效覆盖。`:1262` 本身确为配置回声（本人独立确认），但它是装饰性而非漏洞。 |
| `4774e795`：把 `noise_model.py` additive 臂零覆盖列为**阻断 1** | **降级为须修** | T3/T4 检验的 `robust_sigma_adu` 是**生产函数**，其平移不变性是真门；负例的存在性未落空。被浪费的是 additive **渲染路径**的覆盖。本车道独立确认 `:454-459` 未传 `mode`、`:312` 分支零进入，但严重性按「覆盖缺口」而非「门失效」定。 |
| `4774e795`：`render.py:518-519` 饱和计数疑点 | **否决** | 本车道核对 `noise_model.py:312-316`（加性偏移在钳位之后施加）与 `:518-519` 的 `>= saturation_adu + add_off` **一致**，无缺陷。 |
| `4774e795`：T5 的 1/12 项权重 0.34% vs 容差 5% | **接受并独立复算** | 本车道用 `B=30, t=1, D=0.02, g=1.5, σ_R=5` 重算，得 24.5367 vs 24.4533 = 0.34%，与子代理一致。并补一条缓解：量化项由 `noise_selftest.py` C2（容差 2%）精确覆盖，故非缺口。 |
| `4774e795`：`gainlib.py` 种子/常量正面结论 | **接受** | 本车道已独立读满 `gainlib.py` 241 行，确认 `:69-70` 标准值、`:26-33` 与 docstring 逐条一致、`lstsq` 带 `rcond=None`。 |
| `4774e795`：所有文件随机种子固定、可复现 | **接受** | 本车道独立核对 `noise_model.py:426/:455`、`m16_scene.py:562`、`m16_sampling.py:124/:535-536`、`render.py:463/:466` —— 种子均固定且与逐帧序号解耦。 |
| `673d4a30`：`m16_sampling.py:1180` 主张判词文本声称「未知 band 被拒」但不在 checks | **接受并独立确认** | 本车道读 `:1101-1124` 逐键核对，`checks` 确无 `unknown_band_rejected`；`:1179-1181` 判词文本确含该句。 |
| `673d4a30` / `4774e795`：多处裸从句伪引（GAP_AUDIT §9.41/§9.42/§9.47、`star_matcher.cpp:616-627`、Q1/Q3 §3.4、`9.67 定案 7`、`.gitignore:167`、`P7-synthetic-noise.md §1`） | **登记为待核，本车道不裁决** | 被引文档均**不在本片内**，本人未读，无法核实被引句是否逐字存在。按纪律标「待核 / 待联网核验」，不下结论。 |

### 7.3 缺口已闭合（`3f22ee01` 交付后）

`3f22ee01` 已交付，声明 **36 份文档/数据集/场景 JSON 全部逐行读满、4 583 行、无未读项**，并给出 R1/R2/R3 三条阻断与大量须修（见 §3.11）。**本片并集覆盖已达 100%，§7.3 原记载的缺口（4 403 行、占 40.5%）已闭合。**

**残留的三项限制（非覆盖缺口，须随件流转）**：
1. **本人未复核 §3.10/§3.11 的行号**——见 §8.3 的强度分层。
2. **28 个场景 JSON 的「可解析性」未被验证**——`3f22ee01` 逐行读毕、括号配平，但**按纪律未跑解析器**（跑解析器会越过「不许执行脚本」的边界）。前台补跑：
   `cd "实验/shared" && for f in $(find . -name '*.json' | grep -v __pycache__); do python3 -m json.tool "$f" >/dev/null || echo "BAD $f"; done`
3. **10 条外部文献逐字核对项仍为「待联网核验」**，本车道与子代理均未裁决。

**建议前台**：本件可作为 EXP-shared-001 的重读证据使用，但引用 §3.10/§3.11 的具体行号前应抽查。

---

## 8. 自证段

### 8.1 本审稿自身的判别依据

本车道对每一条判据施加的唯一检验是：**期望量能否追回到被检验量自身的输出字段**。

- 「两侧用了同一条代数公式」**不**算独立验证 → 据此降级了 `m16_sampling.py:1262`。
- 「往返」**不**算独立验证 → 据此把 `m16_scene.py:836`（V6 正例臂）列为建议而非通过，把 `hiss_writer_smoke` 类往返断言列为第③型（见 INF 片）。
- 每一处「恒真」都必须能指出**是哪一个代数关系或哪一条控制流使其无法翻红**；本件 4.1 的 5 条阻断，每条都配了具体注入点与控制流（§5 反例 A–E）。

### 8.2 本审稿的可复现性声明

- 全部行号来自本人用 `read` 工具逐段读取的输出，未使用任何脚本扫描替代阅读（规范 04 §1）。
- 全部计数由本车道亲自 `wc -l` 复算，与 `片清单-权威版.yaml:2607` 的 `实际行数: 10856` 逐位相符。
- **未执行**：任何编译、ctest、pytest、任何脚本、未运行任何 selftest。
- **零 git 写**：未 add、未 commit、未 checkout、未 reset、未 stash。
- **未改任何仓内文件**：本车道只写了本文件与姊妹片交付件。
- 唯一在授权范围外做的动作：确认 `git log`/`git status`/`git diff --name-only`（只读），用于基线漂移核验。

### 8.3 未读到即未判（纪律声明 · 已按 §1.4 更新）

本片 46 份中，**本人逐行读完 6 份、部分读完 3 份、共 37 份未由本人读**（含参考文献两份与全部 27 个场景 JSON）。

**这 37 份已由 `3f22ee01`（文档/场景）与 `673d4a30`/`4774e795`（Python）逐行读完**，故本片**并集 100% 覆盖**，§3.11 与 §3.10 的条目**不是「未判」，而是「转引」**——它们有结论，但**本车道未亲自复核其行号**。

**强度分层**（引用时必须区分）：
- **可直接采信**：B1–B5 及 §3.1–§3.9 的全部发现（本车道亲读原文 + 可复现控制流）。
- **建议抽查**：B6–B8 及 §3.10/§3.11 的全部发现（转引子代理，本车道未复核行号）。
- **一律不裁决**：10 条「待联网核验」的外部文献逐字核对项。

### 8.4 结论的强度限定

本车道给出的 5 条阻断，**全部经本人独立复核控制流**，可直接采信。§3.10 的 3 条来自子代理、本人未读原文，**强度较弱**，引用前须复核。§7.2 列出的 7 条待核伪引**均未裁决**。

---

*本件由 G08-05 第 1 遍审稿补片车道产出。片清单核对结论：**91 片清单中 `EXP-shared-001` 与 `INF-aio-003` 此前确无交付件**（`审稿-P1-EXP-shared-001.md` 与 `审稿-P1-INF-aio-003.md` 在 `run/GOVERN-08/审核包-R2/` 下均不存在），前台先前「91/91 全部交付」的口头报出是按片清单核对出来的**错误**，实际为 89。本车道补上其中 1 片，另 1 片见姊妹件。*
