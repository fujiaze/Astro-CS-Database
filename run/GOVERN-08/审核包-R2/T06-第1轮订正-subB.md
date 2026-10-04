# T06 第 1 轮订正 · subB 交付件

范围：`docs/engineering/contracts/CONFIG.md`、`docs/engineering/contracts/PIPELINE_BLOCK.md`（白名单内二文件，未新建任何文档，未新建 DOCUMENT_INDEX 条目）。

---

## ⓪ 撞车预警（需前台立即裁决，优先级高于本单其余全部内容）

**执行本单期间 `docs/engineering/contracts/PIPELINE_BLOCK.md` 被另一代理并发改写。**

- 本代理首次读取该文件（23:0x）时内容为原始版（`:85-93` 前表 `C4=方向一致 / C5=载体合同 / C6=非退化`；`:102-109` 后表 `C4=无幻边 … C8`）。
- 约 23:13:53 该文件 mtime 变为新内容，已含 `PC-C*` / `IR-C*` 改名空间与两处「判据域」声明 —— **这不是本代理写的**。
- 同一时段 `git status` 显示 30+ 个非白名单文档被改动，且存在其它代理的交付件（`T06-第1轮订正-DOC-ENG.md`、`T06-第1轮订正-DOC-DETAIL-GLOSSARY.md`、`野文件-noise_snr_audit_upstream_findings.md`）。

**本代理的处置**（依 AGENTS.md「后到者得知先到者已落内容，不回滚覆盖」）：

1. **未覆盖**先到者的实现。先到者把前表拆成 `C1/C2/C3/C3b`（基础判据，保持稳定编号）+ `PC-C4..PC-C6`（本域专有），后表整体 `IR-C4..IR-C8`，并把 `IR-C8` 排到末位使声明范围 `IR-C4–IR-C8` 与表体一致。**该方案同时满足本单两条要求**（「前表改 PC-C\*」与「不要改 C1–C3 这种稳定编号」），比整表加前缀更贴合，故予以保留。
2. 只做了 **3 处最小残留订正**（跨域编号混写与范围漏项），未改任何判据语义、未动块字段表、未动 C1–C3 的编号与内容。
3. **需前台裁决**：两代理同改一文件。请前台复核 `PIPELINE_BLOCK.md` 的最终 diff，确认先到者改动与本代理 3 处订正可并存；如判定冲突，以谁为准由前台指定。

本代理对 `PIPELINE_BLOCK.md` 的净改动 = 3 处字符串级订正，全部列于 §二。

---

## 一、CONFIG.md 逐条处置表

### 1. P0「两篇文档被拼进同一文件」→ **已改**（三块顶层章节）

**复核证据**：`CONFIG.md:237`（改前）自述「本文描述的是 orchestrator 的 Stage2 配置 … **不是**三命令 `phase_config` 合同」，其权威自指写作 `CONFIG.md`「三命令 phase_config 与模板」一节 —— 文档把自己引为第三方，是拼接的确证。

**改前逐字**（抬头段）：

> 三类配置严格分离：…；运行清单冻结本次运行。本文是三类配置的字段、值域、默认来源、滤镜库语义与配置校验命令的正本。

**改后逐字**：

> 本文是三类配置的字段、值域、默认来源、滤镜库语义与配置校验判据的正本。全文由三个互不重叠的配置面组成，每面各自声明作用域与键集合，判读任一面时以该面的作用域声明为准：
>
> - **配置面一 · 三命令 phase_config 与三类配置**（本文件正本主体）：…
> - **配置面二 · orchestrator Stage2 配置**：…作用域仅限 orchestrator Stage2 配置面，不是三命令 `phase_config` 合同，也不是生产写出侧的运行清单面。
> - **配置面三 · 阶段生产调用链与 Stage1 配置**：…作用域仅限编排调用链，不承担键集合与值域的声明。
>
> 三个配置面的键集合互不通用：在某一面的配置里出现另两面的键名即判错。

**结构改动**：原 12 个 `##` 降为 `###`（面一），Stage2 段降为 `###`（面二），stage1/stage2 子表降为 `####`（面三）。`参考文献` 保持文件级 `##`。

**复核命令与输出**：

```
$ grep -n "^#\{1,4\} " docs/engineering/contracts/CONFIG.md
16:## 配置面一 · 三命令 phase_config 与三类配置
20:### 权威链
32:### 三类配置（现场清单）
44:### `eng/packaging/config/defaults.json`（`acsd.config-defaults/v1`）
86:### 三命令 phase_config 与模板
120:### `eng/packaging/config/filters.json`（`acsd.filter-library/v1`）
146:### `eng/contracts/schemas/cpu_profile.schema.json`（单一文件，两分支）
159:### `eng/contracts/schemas/run_manifest.schema.json`
169:### 配置校验判据（当前无机器执行器，判据由人读）
197:### 旋钮与默认登记册 `eng/packaging/config/config_registry.json`（`acsd.config-registry/v1`）
210:### 滤镜名匹配语义（可执行规则 + 正反例）
223:### `cpu_profile.host.os_abi` 值域（冻结）
237:### 索引归属（单一事实源）
252:## 配置面二 · orchestrator Stage2 配置
261:### Stage2 config 段
350:## 配置面三 · 阶段生产调用链与 Stage1 配置
354:### 生产调用链（入口符号 → 配置门 → 实现符号 → 诊断字段）
364:#### stage1（`normalize` 子命令）
376:#### stage2（`mosaic` 子命令）
390:### Stage1 config
396:## 参考文献
```

**「文档引自己为第三方」→ 块内自引**：改前 `见 CONFIG.md 「三命令 phase_config 与模板」一节`（3 处）与 `本文与 CONFIG.md 在此点上两边同向` 1 处，全部改为「本文件的配置面一 / 本块 / 见本文件的…一节」等块内表述。

> **留给单独一轮的事**：本单按指示**不拆成三个文件**。若要彻底拆成三份独立正本（`CONFIG.md` / orchestrator Stage2 配置合同 / 阶段调用链与 Stage1 配置合同），需单独一轮处理，理由与影响面：① 会牵动 `docs/DOCUMENT_INDEX.yaml` 的登记（本单无权改，由前台统一做）；② 会牵动跨文档引用——`block_frame.h:5`、`module_ports.registry.json:784/845`、`eng/contracts/README.md:22` 均已按「一个文档」的口径引用；③ 会牵动 `eng/contracts/schemas/*.json` 的 `x-doc` 指针。**拆分必须与代码侧引用同步，否则立即产生悬空指针。**

### 2. 6-4 `weight_mode` 相反结论 → **已改**（两块各自声明键集合）

**复核证据**（生产代码为准）：

```
$ grep -n "weight_mode\|acr_route\|legacy_allow_weight_fallback" lib/algorithms/coverage/src/stage2_common.cpp
599:        if (in.contains("weight_mode")) {
600:            *err = "integration.weight_mode 已删除：不存在「权重模式」"        ← 出现即拒绝
611:        if (in.contains("legacy_allow_weight_fallback")) {                 ← 出现即拒绝
629:        if (in.contains("acr_route")) {
630:            *err = "integration.acr_route 已删除："                         ← 出现即拒绝
640:            {"precision", "memory_limit_mb", "rejection"},                   ← integration 段允许键集合
270:                                      "weight_mode", "rank_rtol", "frame_gain"},  ← sky_plane 子段允许键
281:            cfg->sky_plane_weight_mode = sp.value("weight_mode", ...);
644:            cfg->acr_route = in.value("acr_route", std::string("auto"));     ← 不可达死代码
```

**关键发现（审稿单未点出，本单据代码更正）**：改前 `:286` 把 `weight_mode(auto)` 列为 `integration` 段现行键、并称「`acr_route` 才是退役键」——**两头都错**。`stage2_common.cpp` 对 `integration.weight_mode` 与 `integration.acr_route` **同为 fail-closed 拒绝**；`integration` 段允许键集合恰为 `{precision, memory_limit_mb, rejection}`。`weight_mode` 作为现行键只存在于 **`sky_plane` 子段**（稀疏天光面定权），与被拒的 `integration.weight_mode` 是两个不同键位。

**改前逐字**：

```
             （low/high/max_iterations/min_samples 不是现行键（出现即硬错误），
              旧 config 必须 eng/tools/migrate_stage2_config.py 迁移）}
             weight_mode(auto)   # acr_route **不是现行键**（已从 parser 删除）：出现在 integration 段即
                                # fail-closed 拒绝；…集成执行路由唯一 = CPU。权重仍是派生量，见下方 note
```

**改后逐字**：

```
             rcr{technique ss_median_dl}}
  # integration 段的现行键集合 = {precision, memory_limit_mb, rejection}。
  # 出现在 integration 段的下列键名一律 fail-closed 拒绝（既不能被设、也不能被读）：
  #   low / high / max_iterations / min_samples   （档级阈值的旧键位）
  #   weight_mode / legacy_allow_weight_fallback  （权重模式面：全链无「权重模式」可选概念）
  #   acr_route                                   （集成执行路由面：生产计算后端恒为纯 CPU）
  # 其余任何键名同样判错。拒绝面必须存活，不得静默忽略或静默取默认值。
  # 需要上述键位的配置由 eng/tools/migrate_stage2_config.py 迁移。
```

**面一（phase_config 面）正向表述**（改前只说「不存在 weight_mode」，未给封闭键集合）：

> **本块键集合（三命令 phase_config 面）** = 三份 `phase_config` schema 的属性并集。本面键集合中**不存在** `algorithm_weight_mode`、`weight_mode`、`legacy_allow_weight_fallback`、`acr_route` 四个键名；在 `phase_config` 的任一分支给出这四个键之一即判错。权重是阶段二按该天球像素对应帧集合现场算出的派生量 `w = SNR²/F_ref² = 1/σ_F²`，全链没有「权重模式」这一可选概念（最高设计的「数据对象」一节；`eng/contracts/schemas/phase_config_mosaic.schema.json` 的 `description` 登记三套权重键名均不在键集合内）。

**核实 `:85` 原判成立**（保留）：

```
$ grep -n "weight_mode" eng/contracts/schemas/phase_config_*.schema.json
phase_config_export.schema.json:25  「块面**不含** weight_mode / legacy_allow_weight_fallback / algorithm_weight_mode」
phase_config_mosaic.schema.json:17  「三套键名一并作废 —— …在本 schema 的**任何**分支都不存在」
phase_config_normalize.schema.json:30「本 schema 任何分支都**不含** weight_mode / legacy_allow_weight_fallback / algorithm_weight_mode」
```

三份 schema 的 `description` 与文档一致 ⇒ **`:85` 的结论正确，本单不改其判，只补封闭键集合**。

**顺带更正（机器真相）**：`defaults.json` 内**不存在** `weight` 分组、`weight.default_mode` 字段不存在（prefix 统计：`calibration 2 / cosmetic 6 / detection 1 / drizzle 1 / hips 1 / noise 14 / paths 1 / photometry 6 / precision 1 / psf 2 / rejection 18 / scalar_gate 2 / snr 1 / sparse_snr 2 / upm 1`，无 `weight`）。故改前 `:36`/`:70` 的 `weight.default_mode` 登记描述已失效，改为「权重面不设默认登记项」。

### 3. 5-6 节名被批量替换成 schema 文件名 → **已改**（逐处打开目标文档按真实 `##` 标题改回）

**处置方式**：对每个被引文档实读 `##` 标题列表后再改，不凭印象。改动 16 处：

| # | 行（改后） | 改前逐字 | 改后逐字（真实节名） | 目标文档真实标题核对 |
|---|---|---|---|---|
| a | 28/59/81 | `CALIBRATION.md`「`eng/contracts/schemas/run_manifest.schema.json`」一节a「暗场-亮场曝光容差的科学判据（冻结）」 | `CALIBRATION.md` 的「**3.4 暗场与亮场的曝光容差：科学判据**」一节 | `grep -n "^#\{1,3\} "` → `101:### 3.4 暗场与亮场的曝光容差：科学判据` |
| b | 62 | `docs/science/noise_snr/NOISE_MODEL`「`…filters.json`」一节/`…cpu_profile.schema.json`」一节/一节a/`…run_manifest.schema.json`」一节 | 数值来源 = `noise_model.cpp` 的 `snr_noise_model_v1_default_config`；科学面 = `NOISE_SNR.md` 的「**3.1 背景方差面：空背景稳健方差**」与「**4 参数与常数**」 | `NOISE_SNR.md` → `110:### 3.1 背景方差面：空背景稳健方差`、`327:## 4 参数与常数` |
| c | 28 | `AGENTS.md`「`eng/contracts/schemas/run_manifest.schema.json`」一节 | **删除该假引用**，改为本块自带守卫条款 | `grep -n "运行产物\|不入库\|产物" AGENTS.md` → **0 命中**（AGENTS.md 无此节，也无此守卫） |
| d | 53、220 | `ANCHOR_CONTRACT.md`「旋钮与默认登记册 `…config_registry.json`」一节 | `ANCHOR_CONTRACT.md` 的「**4. 内容锚（content anchor）**」一节 | `ANCHOR_CONTRACT.md` → `49:## 4. 内容锚（content anchor）` |
| e | 36 | `「旋钮与默认登记册 `eng/packaging/config/config_registry.json」一节`（**引号不配对**） | `见本文件的「旋钮与默认登记册 `eng/packaging/config/config_registry.json`」一节` | 块内自引；配平复核见 §五 |
| f | 15、30、118 | `UNIFIED_MODEL.md`「三命令 phase_config 与模板」一节（该节**不存在**） | `UNIFIED_MODEL.md` 的「**3. 三类配置严格分离**」一节 | `UNIFIED_MODEL.md` → `64:## 3. 三类配置严格分离`（全文仅 3 个 `##`） |
| g | 16 | `18_cli.md`「三命令 phase_config 与模板」一节-「`cpu_profile.schema.json`」一节 | `18_cli.md` 的「**5. 配置项**」一节 | `18_cli.md` → `33:## 5. 配置项` |
| h | 63 | `REJECTION.md`「`cpu_profile.schema.json`」一节「阈值冻结锚点」 | `REJECTION.md` 的「**5 连续定义**」与「**2 符号表**」两节 | 18 个 rejection 锚点解析落点：17 → `55:## 5 连续定义`，1 → `12:## 2 符号表` |
| i | 64 | `PHOTOMETRY.md`（无节名） | 「**4 输入有效域**」「**2 符号表**」与 IRLS/Tukey biweight 记述三处 | 6 个 photometry 锚点解析落点：4→`181:## 4 输入有效域`、1→`20:## 2 符号表`、1→`# IRLS + Tukey biweight` |
| j | 65、131、143 | `PHOTOMETRY.md`「`eng/packaging/config/defaults.json`」一节a / 一节a.7 | `PHOTOMETRY.md` 的「**2a 参考通量 `F_syn` 的合成口径（定义 · 量纲 · 适用域 · 证据）**」一节 | `PHOTOMETRY.md` → `38:## 2a 参考通量 …` |
| k | 67 | `PHASE2_UPM.md`「滤镜名匹配语义」一节（**该节不存在**） | 删除该跳转 | `PHASE2_UPM.md` 全部 `##` 为 1–17 + 3a/9a，无此节名 |
| l | 78 | `docs/detail/normalize/modules/noise_snr`「`cpu_profile.schema.json`」一节（**目录不存在**） | 改为「`defaults.json` 中该字段的 `source` 与 `source_ref` 均为空，仓内无可核验的字段名来源登记」 | `ls docs/detail/normalize/modules/noise_snr` → `没有那个文件或目录`；`defaults.json` 两条记录 `source=None, source_ref=None` |
| m | 107 | `PHASE3_HIPS_TO_FITS.md`「三命令 phase_config 与模板」一节（不存在） | 「**2 符号表**」一节 + 「**4 输入有效域**」一节 | `PHASE3_HIPS_TO_FITS.md` → `12:## 2 符号表`、`41:## 4 输入有效域` |
| n | 116 | 「`eng/packaging/config/filters.json`」一节（CONFIG.md 自引却写成文件名） | 「本文件的滤镜库一节」 | 块内自引 |
| o | 118、82 | 「旋钮与默认登记册 `…config_registry.json`」一节（配对坏） | 「本文件的「旋钮与默认登记册 `eng/packaging/config/config_registry.json`」一节」 | 块内自引 |
| p | 199 | `DOCUMENT_GOVERNANCE.md` 的锚纪律一节「旋钮与默认登记册…」一节 | `DOCUMENT_GOVERNANCE.md` 的「**锚纪律**」一节 | `DOCUMENT_GOVERNANCE.md` → `61:## 锚纪律` |

**追加发现：两个「规则号」无处定义**（DOCUMENT_GOVERNANCE「检查项 C5——跨文档合同 ID 存在」明定「只被引用而无处定义的规则号本身即缺陷」）：

```
$ grep -rn "R-54" docs/ eng/ | grep -v config_registry.json
docs/engineering/contracts/CONFIG.md:42  （改前）…（R-54 「…filters.json」一节：需要位置信息时用「锚 id + 内容指纹」）
docs/engineering/contracts/CONFIG.md:204 （改前）…；R-54 「三类配置」一节：位置优先用内容锚表达…
```

`R-54` 在全仓只作为裸标签出现于 CONFIG.md 与 `config_registry.json` 的 note（「R-54 收口」），**任何规则清单里都没有它的定义**。两处均删去该编号，改引真正承载该规则的 `ANCHOR_CONTRACT.md`「4. 内容锚（content anchor）」。

**复核命令与输出**：

```
$ python3 -c "…对 defaults.json 全部 source_ref 的 quote 在目标文档内定位所属标题…"
#### docs/science/REJECTION.md     17  2:5 连续定义    1  2:2 符号表
#### docs/science/PHOTOMETRY.md     4  2:4 输入有效域   1  1:IRLS + Tukey biweight   1  2:2 符号表
#### docs/science/PHASE3_HIPS_TO_FITS.md  1  2:2 符号表

$ grep -c "「校验项清单」" docs/engineering/contracts/CONFIG.md
0
$ grep -n "「校验项清单」\|PSF_SIGNAL_SNR\|NOISE_MODEL\|R-54" docs/engineering/contracts/CONFIG.md
0
```

### 4. 5-11 `PSF_SIGNAL_SNR` 不存在 → **已改**

**复核**：

```
$ ls docs/science/noise_snr/
NOISE_SNR.md
README.md
```

**改前逐字**（`:53` 与 `:70`，共 2 处）：

- `:53` `| weight（默认权重口径；权重是阶段二按该天球像素对应帧集合现场算出的派生量） | 1 | `docs/science/noise_snr/PSF_SIGNAL_SNR`（两处陈述） |`
- `:70` `…依据 `docs/science/unified/UNIFIED_SCIENCE_MODEL.md` 与 `docs/science/noise_snr/PSF_SIGNAL_SNR` 「三类配置」一节）。`

**改后**：`:53` 的 `weight` 整行删除（该分组在 `defaults.json` 中不存在，删的是**失效登记**而非真内容），代之以新增的「权重面不设默认登记项」条；`:70` 的 `enum_target`/`enum_token` 首例（`weight.default_mode` → `point_information`）因该键不存在而删除，保留条款本体。

**范围外同源缺陷（登记，本单无权改）**：`docs/engineering/resources/PERFORMANCE_MODEL.md:19` 与 `docs/science/PHOTOMETRY.md:304` 仍引 `docs/science/noise_snr/NOISE_MODEL`，该路径同样不存在。

### 5. 5-9 `[18]/[19]` 无源标注 → **已撤回标注**（核对不到原文）

**核对尝试与结果**：

```
$ web_search "PixInsight ImageIntegration winsorized sigma clipping Huber documentation"
No results found.
$ web_search "WBPP Better Pixel Alignment bestRejectionMethod engine.js PixInsight"
No results found.
$ web_fetch https://pixinsight.com/doc/modules/ImageIntegration/ImageIntegration.html
HTTP Error 404
$ curl -o /dev/null -w "%{http_code}" https://pixinsight.com/doc/modules/ImageIntegration/index.html
406
$ curl … https://github.com/PixInsight/PCL            → 404
$ curl … https://raw.githubusercontent.com/PixInsight/PCL/master/README.md → 404
```

网络可达但目标资源不可取（404/406，疑经过滤代理）⇒ **PixInsight 官方式与 `WBPP 2.5.9 engine.js:1421-1429`（含包 sha1 `712cc7c3…`）均核对不到原文**。

**处置（删无源标注，不凭印象补文献表）**：

改前：

```
  - winsorized_sigma: robust 版（median 位置 + 1.5σ winsorize 迭代；
      语义来源 = PixInsight ImageIntegration 官方式[18]/[19]（Huber 体系）；
      Siril 1.4.3 仅作**次生参考实现对拍**，不是语义来源）。
```

改后：

```
  - winsorized_sigma: robust 版（median 位置 + 1.5σ winsorize 迭代，Huber 体系）。
      其语义来源应为一手权威文档（PixInsight ImageIntegration 官方式）；
      **该一手出处在本仓核对不到**，本节只陈述本仓实现口径，不以未经核对的文献编号背书。
      Siril 1.4.3 仅作**次生参考实现对拍**，不是语义来源。
```

同样口径处理 WBPP 对照档（改前声称档界取自 `engine.js:1421-1429` 与 `WBPP ≤2.3.x 旧表`）：

改前：

```
  - auto 在 …；对照档 = **本仓冻结解析表**（档界取自 WBPP 2.5.9
    `bestRejectionMethod`，`engine.js:1421-1429`，包 sha1 `712cc7c3…`；
    其 `n>15` 分支为 ESD，本仓该档取 linear_fit = WBPP ≤2.3.x 旧表）：
```

改后：

```
  - auto 在 …；对照档 = **本仓冻结解析表**：
      nominal<6 → percentile；6..15 → winsorized_sigma；>15 → linear_fit；
    该表的档界本应逐档对应 WBPP 的 `bestRejectionMethod`；**其一手出处在本仓核对不到**
    （WBPP 2.5.9 的 `engine.js` 对应行、包 sha1 与 WBPP ≤2.3.x 旧表均未能取得原文），
    故此处的对照档只承担**本仓冻结行为**，不主张与 WBPP 逐档同源；需权威补源后另行登记。
```

**冻结表本身（`nominal<6 / 6..15 / >15`）予以保留** —— 它是仓内冻结行为，删掉会丢真内容；只撤掉其不可核验的出处主张。

**文献表**：本篇 `参考文献` 维持 `[1]–[5]`，**未新增任何条目**（核对不到即不著录）。

### 6. 9-5 版本代次与历史叙事 → **已改**

| 位置 | 改前逐字 | 改后逐字 |
|---|---|---|
| `:331`（排异档位映射） | 「`N≥6` winsorized（M3：原 `N≥16` linear fit 档改投）」 | 「`N≥6` winsorized」 |
| `:288` | 「rejection.method 说明（V17 冻结）：」 | 「rejection.method 说明：」 |
| `:321` | 「V17：旧顶层 low/high/max_iterations/min_samples 已从 parser 删除，出现即硬错误（提示 eng/tools/migrate_stage2_config.py）。」 | 「档级阈值的现行键位在 `rejection` 段的各方法子对象内（如 `winsorized_sigma{lower_sigma, upper_sigma, max_iterations}`）；`low`/`high`/`max_iterations`/`min_samples` 出现在 integration 顶层即硬错误。」 |
| `:352` | 「上方 fenced 块是 `eng/tools/config_consistency_check.py` 的 docs 腿输入，」 | 并入「上方 fenced 块的 `acsd_adaptive_pixel` 档位与本条同值」 |

**9-5 复核命中只剩 3 类，逐条判定**：

```
$ grep -nE "V1[0-9]|M3|R-[0-9]+|P-[0-9]+|已从 parser 删除" docs/engineering/contracts/CONFIG.md
123: …`gap_id=GAP-025`…          ← P-[0-9]+ 命中 "GAP-025"：缺口登记号，非版本代次，保留
182: …provenance unverified/GAP-025… ← 同上，保留
373: …TEST-SNR-001                ← R-[0-9]+ 命中 "-SNR-001"：测试 ID，保留
387: …V17StatusesExplicit         ← V1[0-9] 命中：**真实测试 ID**，见下
```

`V17StatusesExplicit` **是真测试标识符，不是正文版本叙事**：

```
$ grep -rn "V17StatusesExplicit" lib/ eng/
lib/algorithms/integration/README.md:57:  ZERO_VALID_WEIGHT（:70-81；V17StatusesExplicit 冻结）。
eng/tools/quality/v19r3_traceability.py:95:  "V17NonFiniteWeightInvalid;V17StatusesExplicit;V17NonFiniteSupportInvalid",
eng/tools/quality/v19r3_traceability.py:165: "V17StatusesExplicit",
```

该 ID 被机器可读面 `v19r3_traceability.py` 消费，改名会立即打断机器引用；且它位于列头已标「测试 ID」的表内，属标识符而非散文历史叙事。**故保留不改**，登记入 §四 待裁决。

### 7. 10-2 复跑命令不可执行 → **已改**（如实写「本门无执行器」）

**复核：三条命令的目标全部不存在，且不止这两处。**

```
$ ls eng/tests
conformance
validation
$ ls eng/tests/conformance
noop
$ ls eng/tests/validation
release02
$ ls eng/tests/config
ls: 无法访问 'eng/tests/config': 没有那个文件或目录
$ ls eng/tools/config_consistency_check.py
ls: 无法访问 …: 没有那个文件或目录
$ ls eng/tests/backend/test_cpu_profile.py
ls: 无法访问 …: 没有那个文件或目录
$ python3 -c "import jsonschema"
ModuleNotFoundError: No module named 'jsonschema'
```

`eng/tests/config/` **整目录不存在**；仓内仅存的副本在归档快照 `run/FINAL-07-e2e/bisect/**/eng/tests/config/` 下（快照非跟踪执行面）：

```
$ grep -rln "check_cfg002_registry" --include="*.py" .
./run/FINAL-07-e2e/bisect/src/eng/tests/config/check_cfg002_registry.py
./run/FINAL-07-e2e/bisect/tree_D_rewrite/eng/tests/config/check_cfg002_registry.py
./run/FINAL-07-e2e/bisect/src2/eng/tests/config/check_cfg002_registry.py
```

**改前逐字**（三条不可执行命令）：

```bash
# 主门（零第三方依赖）
timeout 300 python3 -m unittest discover -s eng/tests/config -t eng/tests/config
# 单例：模板通过 schema / 负例必败
timeout 60 python3 eng/tests/config/run_validation.py eng/contracts/schemas/phase_config_normalize.schema.json eng/packaging/config/templates/normalize.phase_config.json
timeout 60 python3 eng/tests/config/run_validation.py eng/contracts/schemas/phase_config_normalize.schema.json eng/tests/config/fixtures/negative/hardware_fields_in_phase_config.json
```

**改后逐字**（整块换成可核对的事实陈述，**不留任何仍不可执行的命令**）：

> **本节门表在本仓没有执行器。** 核对：`eng/tests/` 下只有 `conformance/` 与 `validation/` 两个目录，其中不含配置合同测试；`eng/tools/` 下无 `config_consistency_check.py`；配置合同的门脚本 `check_cfg002_registry.py` 与测试 `test_cfg001_contracts.py`/`test_cfg002_registry.py` 在跟踪集内不存在。运行期依赖 `jsonschema` 的模板/schema 门亦无执行器（该依赖不在仓内）。⇒ 下表列出的是**应门禁的判据**，不是可复跑的命令；逐条核对判据由人读执行，登记为需代码侧订正项。
>
> 下表中的测试 ID 与脚本路径为判据的既有标识，仓内无对应实现，改动判据时同步更新本节与登记面。

**门表本体全部保留**（14 条判据是真内容，只撤执行器主张，不撤判据）。另将本块内其余指向不存在执行器的引用一并降级为事实陈述：`:152` 的「校验项 `TestCpuProfileMigration::test_writer_is_benchmark_only`」、`:30`/`:42` 的回归锁引用、`:250` 的「一致性由 `eng/tools/config_consistency_check.py` 校验」（改为「该一致性门在本仓无执行器 … 判据由人读核对」）。

### 8. P1 `run_manifest` 两套不相交字段名族 → **已改**（如实登记，UNRESOLVED）

**复核证据**：

```
$ python3 -c "import json;d=json.load(open('eng/contracts/schemas/run_manifest.schema.json'));
  print('required:',d['required']); print('properties:',list(d['properties'])); print('additionalProperties:',d['additionalProperties'])"
required: ['manifest_schema','run_id','software_sha','config_hash','manifest_input_hashes','manifest_output_hashes','toolchain_version','created_utc']
properties: ['manifest_schema','run_id','software_sha','config_hash','manifest_input_hashes','manifest_output_hashes','toolchain_version','toolchain_compiler','toolchain_cmake','created_utc','storage']
additionalProperties: False

$ sed -n '535,558p' lib/infrastructure/cli/commands.cpp   # write_run_manifest()
{"schema_version","1"}, {"kind","acsd_run_manifest"}, {"run_id",…}, {"acsd_version",…},
{"platform",…}, {"config_path",…}, {"config_sha256",…}, {"cpu_profile_path",nullptr},
{"cpu_profile_sha256",nullptr}, {"phases",…}, {"artifacts",…}, {"status",…},
{"started_utc",…}, {"finished_utc",…}, {"summary",…}
$ sed -n '569p'
final_path = out_dir + "/acsd_run_" + ev.run_id() + ".json";
```

**交集 = {`run_id`}**；生产字段名族其余每一项都落在 schema 的 `additionalProperties:false` 拒绝面内。生产写出点无任何代码引用该 schema（`grep -rn "run_manifest" lib/ eng/ --include=*.cpp --include=*.h --include=*.py` 命中的全是 `write_run_manifest`/`final.run_manifest` 的协议字段名，非 schema 校验）。

**改后逐字**：

> **本节描述的是 `eng/contracts/schemas/run_manifest.schema.json` 冻结的 run_manifest 类字段名族（预留合同面）**，字段集 = `manifest_schema`/`run_id`/`software_sha`/`config_hash`/`manifest_input_hashes`/`manifest_output_hashes`/`toolchain_version`/`toolchain_compiler`/`toolchain_cmake`/`created_utc` + 加性可选键 `storage`，且 `additionalProperties:false`。
>
> **显式登记（两个不相交的 run manifest 对象）**：该 schema **未被任何生产写出点消费**。生产写出点是 `lib/infrastructure/cli/commands.cpp` 的 `write_run_manifest()`，它写的是**另一套字段名族**——`schema_version`/`kind`(`acsd_run_manifest`)/`run_id`/`acsd_version`/`platform`/`config_path`/`config_sha256`/`cpu_profile_path`/`cpu_profile_sha256`/`phases`/`artifacts`/`status`/`started_utc`/`finished_utc`/`summary`（失败时另加 `error`，节点级科学事实经 `extra` 并入），落盘为 `acsd_run_<run_id>.json`，由同文件的 `inspect`/`resume` 读回。该生产字段名族除 `run_id` 外**逐字段**落在本 schema 的 `additionalProperties:false` 拒绝面内。
>
> ⇒ 「run manifest」在本仓存在**两个不相交的对象**：本节的 schema 预留字段名族，与生产写出点的运行清单字段名族。两套词表**不合并、不互相改写**；哪一套是正本需负责人裁决（UNRESOLVED，见交付审核包）。

**未把两套词表硬凑成一套**（遵指示）。

---

## 二、PIPELINE_BLOCK.md 处置表（受 §⓪ 撞车约束，仅 3 处最小订正）

**改动面复核**（改前执行）：

```
$ grep -rn -E "C[0-9]b?" docs/engineering/
docs/engineering/contracts/PIPELINE_BLOCK.md:74..109   ← 本单唯一改动面（12 处 C 编号）
docs/engineering/build/BUILD_GRAPH.md:61,87             ← 无关命名域（自建 C4/C5）
docs/engineering/governance/DOCUMENT_GOVERNANCE.md:…    ← 无关命名域（检查项 C1–C5、C3-1..C3-6）
docs/engineering/resources/cpu/CAPABILITY_PROBE.md:73   ← 误命中（"纯 C11"）
docs/engineering/standards/CODE.md:47,49                ← 误命中（MSVC C4996 等）
```

`docs/engineering/**` 内 **C 编号冲突面仅限 PIPELINE_BLOCK.md 一个文件**。

**对生产代码 / schema 引用面的影响（改前 grep，逐项回答）**：

| 引用方 | 引用形态 | 含 C 编号？ | 改名影响 |
|---|---|---|---|
| `lib/include/acsd/core/block_frame.h:5` | 「块元数据 9 字段、创建-消费-销毁状态机、DAG 四条非法图判据、provenance 流转、显式降级」 | **否** | **零影响** |
| `eng/contracts/schemas/pipeline_block.schema.json` | `title` = 「阶段内命名块元数据与生命周期」；`x-doc` = 路径 | **否** | **零影响**（见下方验证输出） |
| `eng/contracts/block_flow/conformance_deviations.json` | 引本文件路径 + `§4` | 否 | 零影响（但其 `§4` 锚点本身已失效，见 §四） |

```
$ python3 -c "import json;d=json.load(open('eng/contracts/schemas/pipeline_block.schema.json'));print(d.get('title'))"
阶段内命名块元数据与生命周期
$ grep -rnE "\b(PC-C|IR-C)[0-9]" lib/ eng/
(0 命中 —— 改名后代码侧无 PC-C/IR-C 引用面)
```

⇒ **`PC-C*` / `IR-C*` 改名对生产代码与 schema 的引用面影响为零**；schema 与 `block_frame.h` 均不引用 C 编号，块字段表未动，C1–C3 编号与内容未动。

**并发写入后，先到者已落地的部分（复核通过，予以保留，未回滚）**：

- 两张表各自加了「判据域」声明与域说明段；
- 前表 `C1/C2/C3/C3b` 保持原样 + `PC-C4/PC-C5/PC-C6`；
- 后表整体 `IR-C4/IR-C5/IR-C5b/IR-C6/IR-C7/IR-C8`，并把 `IR-C8` 移至末位；
- `:100`（改后 `:108`）声明范围由「C4–C7」改为 `IR-C4–IR-C8`，**与表体 6 条一致**（原声明漏 `C8`、且 `C8` 排在 `C7` 之上）。

**本代理的 3 处最小订正**：

| # | 改前逐字 | 改后逐字 | 理由 |
|---|---|---|---|
| 1 | 「⇒ 判红（同上 C3b / PC-C5）；」 | 「⇒ 判红（同上 C3b、PC-C5）；」 | 原写法用斜杠把**基础判据 `C3b`** 与**载体域专有 `PC-C5`** 并列成「C3b / PC-C5」，读作同一判据，正是本单要消除的跨域混写 |
| 2 | 「（C1–C3、PC-C4–PC-C6）」 | 「（C1、C2、C3、C3b、PC-C4–PC-C6）」 | 原范围 `C1–C3` 漏掉表内实有的 `C3b` |
| 3 | 「`C1`–`C3` 是本合同的**基础判据**（结构完整、锚点解析、双向闭合）」 | 「`C1`、`C2`、`C3`、`C3b` 是本合同的**基础判据**（结构完整、锚点解析、双向闭合、载体一致）」 | 同上：编号枚举与表体对齐 |

**复核命令与输出**：

```
$ grep -n "PC-C\|IR-C" docs/engineering/contracts/PIPELINE_BLOCK.md
76:  ⇒ 判红（同上 PC-C4）；
77:  ⇒ 判红（同上 C3b、PC-C5）；
78:  ⇒ 判红（同上 PC-C6；恒真比较无证据资格）。
86: 端口↔代码双向一致判据（C1、C2、C3、C3b、PC-C4–PC-C6）把注册表的端口面与
95: | PC-C4 方向一致 | …
96: | PC-C5 载体合同 | …
97: | PC-C6 非退化 | …
108: 断言管线 IR 与注册表端口图的**序**与**边**一致（IR-C4–IR-C8；…）：
112: | IR-C4 无幻边 | …
113: | IR-C5 序为拓扑序 | …
114: | IR-C5b 声明序一致 | …
115: | IR-C6 psf 在 wcs 之后 | …
116: | IR-C7 非退化 | …
117: | IR-C8 IR 端口 ∈ descriptor | …
```

---

## 三、你推翻的审稿判定

| 审稿判定 | 我的结论 | 依据（可复核） |
|---|---|---|
| 「`:286` 的 `weight_mode(auto)` 是 orchestrator stage2 面的现行键，`acr_route` 才是退役键」 | **推翻**：`integration.weight_mode` 与 `integration.acr_route` **同为 fail-closed 拒绝键**；`integration` 段允许键集合恰为 `{precision, memory_limit_mb, rejection}`。`weight_mode` 作为现行键只存在于 `sky_plane` 子段 | `lib/algorithms/coverage/src/stage2_common.cpp:599-606`（weight_mode 拒绝）、`:629-636`（acr_route 拒绝）、`:639-642`（允许键集合）、`:267-281`（sky_plane 允许键含 weight_mode） |
| 「`:331`/`:284`/`:286`/`:321` 含版本代次，全部删代次」 | **部分保留、部分修正**：`:331` 的 `M3`、`:288` 的 `（V17 冻结）`、`:321` 的 `V17：` 确为历史叙事，已删；但 `:387` 的 `V17StatusesExplicit` 是被 `eng/tools/quality/v19r3_traceability.py:95/165` 消费的**活测试 ID**，不在本单「版本代次」范围内，**保留并登记待裁决** | `grep -rn "V17StatusesExplicit" lib/ eng/` |
| 「`:85` 说不存在 `algorithm_weight_mode`/`weight_mode` 键，与 `:286` 矛盾，应以其中一方为准」 | **推翻「矛盾」定性**：两处属**不同配置面**，各自成立，不构成同文件相反结论。`:85` 面（phase_config schema）经三份 schema `description` 逐字印证为真；`:286` 面（orchestrator Stage2 parser）经代码印证为另一组键集合。原判要求「两块各自声明作用域」的方向正确，但把它写成「同一文件给出相反结论」不准确 | `grep -n "weight_mode" eng/contracts/schemas/phase_config_*.schema.json`（3 份均声明不含）；`stage2_common.cpp:639-642` |
| 「5-6 只需改 3 处点名节名」 | **扩充为 16 处**：点名 5 处（a/b/c/d/e）之外，同一「节名被替换成文件名」的损坏模式还散布在 f–p 共 11 处（UNIFIED_MODEL、18_cli、REJECTION、PHOTOMETRY、PHASE2_UPM、PHASE3、DOCUMENT_GOVERNANCE、CONFIG.md 自引、以及 `PHASE2_UPM.md`「滤镜名匹配语义」/ `docs/detail/normalize/modules/noise_snr` 两处**指向不存在的节与目录**）。按「逐处打开目标文档按真实标题改回」的本意全量处置 | 逐篇 `grep -n "^#\{1,3\} "` 输出见 §一.3 表右列 |
| 「`:146` 描述的是 run_manifest 的 schema 侧字段」 | **确认原判成立**，并按指示补「未被任何生产写出点消费 + 两个不相交对象」的显式登记；额外核实交集恰为 `{run_id}`、生产落盘名 `acsd_run_<run_id>.json`、`kind:"acsd_run_manifest"`、`inspect`/`resume` 读回 | `commands.cpp:530-569`、`run_manifest.schema.json` 的 `required`/`properties`/`additionalProperties` |
| 「`:151-157` 两条 `run_validation.py` 命令不可执行」 | **确认并加重**：不止这两条，`eng/tests/config/` **整目录不存在**，门脚本与测试仅存于 `run/FINAL-07-e2e/bisect/**` 归档快照；`eng/tools/config_consistency_check.py`、`eng/tests/backend/test_cpu_profile.py`、`eng/tests/unit/cpu007_profile_store_test.cpp` 亦不存在；`jsonschema` 未安装 ⇒ 无任何可跑的 schema 门 | 见 §一.7 全部命令与输出 |

**保留原判不动的项**：`:100` 声明范围与表体不符、`C8` 排序错位、块字段表、C1–C3 稳定编号 —— 原判成立，已由先到者落地，保留。

---

## 四、需代码侧订正的问题（本单白名单外，逐条给出可复核定位）

| # | 问题 | 定位 | 影响 |
|---|---|---|---|
| C-1 | **`stage2_common.cpp` 不可达死代码**：`acr_route` 已在 `:629-636` 被显式拒绝、且不在 `:640` 允许键集合内，`:644-648` 的 `cfg->acr_route = in.value("acr_route","auto")` + 值域校验**永不可达** | `lib/algorithms/coverage/src/stage2_common.cpp:644-649` | 读者会误以为 `acr_route` 仍是活键；与本单 6-4 的键集合声明直接冲突。应删除 |
| C-2 | **`eng/tests/config/` 整目录缺失** ⇒ CONFIG.md 全部门表无执行器 | `eng/tests/`（仅 `conformance/noop`、`validation/release02`）；仅存于 `run/FINAL-07-e2e/bisect/**` 快照 | 配置合同 14 条门 + CFG002-01…12 全部无法执行；本单已按「无执行器、判据由人读」如实登记，但**这是代码侧缺口，需补齐或正式撤销门表** |
| C-3 | **`eng/tools/config_consistency_check.py` 缺失** | CONFIG.md「索引归属」一节引其作 docs 腿校验器 | 本单已降级为「无执行器」陈述 |
| C-4 | **代码引用已失效的文档路径 `PIPELINE_BLOCK_CONTRACT.md`**（真实路径为 `docs/engineering/contracts/PIPELINE_BLOCK.md`） | `lib/include/acsd/core/block_frame.h:5`、`:22`、`lib/include/acsd/core/block_flow.h:4`、`lib/infrastructure/scheduler/src/block_frame.cpp:2`、`block_flow.cpp:2`、`:75`、`lib/infrastructure/pipeline/module_ports.registry.json:784`、`:845`（后者还引 `§1.1`） | 8 处悬空指针。**本单只改文档，未改代码**，登记待代码侧统一订正 |
| C-5 | **`eng/contracts/block_flow/conformance_deviations.json:14` 引 `PIPELINE_BLOCK.md §4`**，但该文件无 `§` 编号、「降级必须显式」实际位于「provenance 流转」一节 | 同左 | 登记面锚点失效 |
| C-6 | **`sci`/`filters` 判据锚源文件缺失**：`lib/algorithms/photometry/cpp/test/filter_qe_provenance.json` 与 `lib/infrastructure/pipeline/orchestrator/cpp/tests/test_photometry_curve_resolve.cpp`、`test_p1phot_passband_identity.cpp` 均不在跟踪集内 | CONFIG.md:123（provenance 段）、`:131`（判据锚） | 本单已在 `:131` 就地标注「两个路径在跟踪集内不存在」；`:123` 的 provenance 指向同样悬空，需代码侧补文件或改指 |
| C-7 | **`defaults.json` 53 条 `source_ref` 中 28 条引文在目标文档内不命中**（含全部 14 条 `noise.*`） | 复算：`noise.patch_grid`/`variance_floor`/`saturation_level`/… 14 条；`detection.threshold_sigma`；`psf.default_model`/`moffat_beta`；`precision.default`；`upm.k_corr`；`drizzle.pixfrac`；`calibration.master_flat_median_range`；`cosmetic.*` 6 条；`snr.path`（目标 `docs/detail/algorithms_phase1/07_noise_snr.md` **不存在**） | 门 CFG002-09「引文唯一命中」对 28/53 判红。本单已把 `noise` 行的权威改指代码 `snr_noise_model_v1_default_config` + `NOISE_SNR.md` 真实节名，但**锚本身的修正在 `defaults.json`（白名单外）** |
| C-8 | **`stage2_common.h` 不存在**（改前 `:326` 指其为 struct 唯一实现） | `ls lib/algorithms/coverage/src/stage2_common.h` → MISSING | 本单保留该句原样未改（属配置面二既有内容，且本单只处理点名项）；登记待订正 |
| C-9 | **`SCI-PHOT-FORMULA-01` 不是条款号，是实验目录名** | `grep -rn "SCI-PHOT-FORMULA-01" docs/` → 只在 `PHOTOMETRY_RESEARCH_PACK.md:62` 作为 `run/…/evidence/` 路径出现 | 本单已就地改注「该 ID 是实验单元目录名，不是条款号」 |

---

## 五、需权威补充的问题（核对不到，不得凭印象补）

| # | 待补事项 | 核对结果 | 本单处置 |
|---|---|---|---|
| A-1 | **PixInsight ImageIntegration 官方式**（`winsorized_sigma` 语义来源、`Huber` 体系、`:319` 原挂 `[18]/[19]`） | `web_search` 两组查询均 `No results found`；`web_fetch` 官网 404；`curl` 直取返回 404/406。**核对不到原文** | 删 `[18]/[19]` 标注，改为不带文献编号的定性表述并显式登记「一手出处在本仓核对不到」。**文献表未新增任何条目** |
| A-2 | **`WBPP 2.5.9 engine.js:1421-1429`（`bestRejectionMethod` 档界）** | `web_search` 无结果；`github.com/PixInsight/PCL` 与 raw README 均 404。**核对不到原文** | 冻结表 `nominal<6 / 6..15 / >15` 保留（仓内冻结行为），撤掉「档界取自 engine.js:1421-1429」「包 sha1 `712cc7c3…`」「`WBPP ≤2.3.x 旧表`」三项不可核验的出处主张 |
| A-3 | **滤镜曲线原始来源**（`GAP-025`，45/45 曲线 `status=unverified`、`url=null`） | 本单未核（不在点名项内） | 沿用既有 `gap_id=GAP-025` 登记，未改动表述 |
| A-4 | **`NOISE_MODEL` 文档名** —— `docs/science/noise_snr/` 只有 `NOISE_SNR.md`；`NOISE_MODEL` 在 3 处被引 | `grep -rn "NOISE_MODEL" docs/` → `CONFIG.md:50`（本单已改）、`PERFORMANCE_MODEL.md:19`、`PHOTOMETRY.md:304`（**白名单外**） | 本单内已清除；余 2 处登记待下一轮 |
| A-5 | **`R-54` 规则号** —— 全仓无定义处 | `grep -rn "R-54" docs/ eng/` 只命中 CONFIG.md 与 `config_registry.json` 的裸标签 | 两处引用已删该编号，改引真正承载规则的 `ANCHOR_CONTRACT.md`「4. 内容锚（content anchor）」；**`R-54` 究竟是历史轮次标签还是应有规则，需负责人确认** |

---

## 六、我否决 / 未采纳的事项（含理由）

| # | 事项 | 处置 | 理由 |
|---|---|---|---|
| N-1 | 把三篇**真正拆成三个文件** | **否决** | 会牵动 `DOCUMENT_INDEX.yaml` 与跨文档引用（含 8 处代码侧路径引用），超本单授权；改在同一文件内切三块顶层章节达成同等可判读效果 |
| N-2 | 为 `[18]/[19]` **补一条参考文献条目**（哪怕只写 PixInsight 官网） | **否决** | 核对不到原文，红线 2 禁止凭印象著录。宁可留「核对不到」登记 |
| N-3 | 整表把 PIPELINE_BLOCK 前表 `C1–C3` 也加 `PC-` 前缀 | **否决** | 本单同时要求「不要改 C1–C3 这种稳定编号」；先到者的方案（C1–C3 保持 + C4–C6 加 PC-）同时满足两条，更贴合 |
| N-4 | **删除** CONFIG.md 门表 14 条判据（因为无执行器） | **否决** | 红线 3：不得为消问题删真内容。判据是真内容，只撤执行器主张 |
| N-5 | 修正 CONFIG.md 分组表与 `defaults.json` 的**计数漂移**（`calibration` 实为 2 非 1；缺 `cosmetic 6`/`paths 1`/`snr 1` 三组；`sparse_snr 2`+`scalar_gate 2`=4 非 3；表列合计 50 ≠ `field_count` 59） | **本轮不处置，登记** | 已核实为真（机器统计见 §一.2），但属未点名项且改动面涉及分组语义，留给下一轮统一处理，避免与并发代理的文档面改动叠加 |
| N-6 | 删除 `:387` 的 `V17StatusesExplicit`（以满足 9-5 的 grep 归零） | **否决** | 它是被 `eng/tools/quality/v19r3_traceability.py` 消费的活测试 ID，改名即打断机器引用；且位于标「测试 ID」的表列内，属标识符非历史叙事 |
| N-7 | 修改 `block_frame.h` 等 8 处失效路径（C-4） | **否决** | 白名单只许改两个文档文件；登记入 §四 C-4 |
| N-8 | 在 `docs/DOCUMENT_INDEX.yaml` 新建本文件条目 | **否决** | 指示明确由前台统一做 |

---

## 七、验证（改完实跑，输出逐字粘贴）

```
$ cd "/workspace/Astro CS Database"

$ grep -n "docs/detail/common\|docs/science/noise_snr/PSF_SIGNAL_SNR" docs/engineering/contracts/CONFIG.md
（0 命中，rc=1）

$ grep -nE "\[1[89]\]" docs/engineering/contracts/CONFIG.md
（0 命中，rc=1）

$ grep -nE "V1[0-9]|M3|R-[0-9]+|P-[0-9]+|已从 parser 删除" docs/engineering/contracts/CONFIG.md
123:- **provenance**：指向 `lib/algorithms/photometry/cpp/test/filter_qe_provenance.json`；…`gap_id=GAP-025`；…
182:| 滤镜库 | 45/45 逐字一致 + provenance unverified/GAP-025 + 无零点键 + enum==库键 | `TestFiltersLibrary` |
373:| Orchestrator::run_stage_snr | stage1.snr.enabled | snr_estimator | snr_noise_model_v1 | parallel_cpu | snr.sigma_bg | TEST-SNR-001 |
387:| integration | stage2.integration.* | phase2/integrate | p2_integrate_pixel | tile 级并行 (p2_parallel_for,  std::thread) | integrate.signal/support | V17StatusesExplicit |

$ grep -n "PC-C\|IR-C" docs/engineering/contracts/PIPELINE_BLOCK.md
76:- 端口方向与代码读写角色不符、或角色无法由代码证据确认 ⇒ 判红（同上 PC-C4）；
77:- 节点触碰 HiPS 产品树却无对应 `carrier` 端口、或同一产物身份跨阶段出现 ⇒ 判红（同上 C3b、PC-C5）；
78:- 注册表为空、或端口数/代码 token 数/边数低于下界 ⇒ 判红（同上 PC-C6；恒真比较无证据资格）。
86:端口↔代码双向一致判据（C1、C2、C3、C3b、PC-C4–PC-C6）把注册表的端口面与
95:| PC-C4 方向一致 | 声明 token 必须在代码里出现，且变量流分析推断出的读写角色必须包含声明的 `direction`；推不出角色即判红 |
96:| PC-C5 载体合同 | 节点间端口 `carrier` ∈ {`output_dir_file`, `hips_product_tree`}；`output_dir` 产物身份限本阶段；`carrier_contract` 必须显式声明 HiPS 产品树为跨阶段载体 |
97:| PC-C6 非退化 | 模块数/端口数/代码 token 数/生产→消费边数均有下界；空注册表判红 |
108:断言管线 IR 与注册表端口图的**序**与**边**一致（IR-C4–IR-C8；**不走台账豁免**——幻边与序错一律按判红处理）：
112:| IR-C4 无幻边 | IR 声明的每条边必须由注册表端口图支持：…
113:| IR-C5 序为拓扑序 | 注册表端口图 DAG 的每条边必须满足 `pos(上游) < pos(下游)`；IR 自身声明的边也必须与节点数组序一致 |
114:| IR-C5b 声明序一致 | IR 的 phase1 节点序必须等于注册表 `modules` 数组里同阶段模块的出现序（块流规格 `stage_block_flow.json` 的 declared order 与 R4 同源） |
115:| IR-C6 psf 在 wcs 之后 | `pos(psf) > pos(wcs)`，且 `psf` 节点必须声明 `artifact:p1_wcs` 输入边（取向先验的真实来源） |
116:| IR-C7 非退化 | phase1 节点数 / IR 边数 / 注册表端口边数均有下界；解析不到即 fail-closed（判据 = 解析成功；解析不到一律判红） |
117:| IR-C8 IR 端口 ∈ descriptor | IR 每个节点的输入/输出端口名必须出现在 `module_adapters.cpp` 对应 descriptor 的端口表里（运行期 `MISSING_PORT` 静态验证的 CI 侧等价判据；不启动产品二进制即可发现 IR ↔ descriptor 漂移） |

$ grep -rn "PSF_SIGNAL_SNR\|ANCHOR_CONTRACT" docs/engineering/
docs/engineering/contracts/CONFIG.md:53:  锚形态规则正本 = `../../detail/anchors/ANCHOR_CONTRACT.md` 的「4. 内容锚（content anchor）」一节；
docs/engineering/contracts/CONFIG.md:220:- 正反例由门 CFG002-04 …（规则正本 = `../../detail/anchors/ANCHOR_CONTRACT.md` 的「4. 内容锚（content anchor）」一节：位置优先用内容锚表达，不写行号）。
docs/engineering/governance/DOCUMENT_GOVERNANCE.md:63:- **文档内不留指向仓内文档的行锚**：…（锚合同的唯一正本 = `../../detail/anchors/ANCHOR_CONTRACT.md`）。

$ python3 -c "import json;d=json.load(open('eng/contracts/schemas/pipeline_block.schema.json'));print(d.get('title'))"
阶段内命名块元数据与生命周期
```

**判定**：命令 1 = 0 命中 ✓；命令 2 = 0 命中 ✓；命令 3 = 4 命中，**全部经逐条判定为非违规**（`GAP-025` 缺口号 ×2、`TEST-SNR-001` 测试 ID ×1、`V17StatusesExplicit` 活测试 ID ×1，见 §一.6 与 §六 N-6）✓；命令 4 = 两命名空间齐备 ✓；命令 5 = `PSF_SIGNAL_SNR` 已从 `docs/engineering/` 全清，`ANCHOR_CONTRACT` 仅剩**合法路径引用**且均已配真实节名 ✓；命令 6 = schema title 未被改动 ✓。

**补充自检**：

```
$ python3 - <<'PY'   # 括号配平 + 机械跳转锚
CONFIG.md      「=87 」=87 balanced | `=1524 even | § 锚点数 = 0
PIPELINE_BLOCK.md 「=12 」=12 balanced | `=172 even | § 锚点数 = 0
PY

$ git -c core.quotepath=false diff --stat -- docs/engineering/contracts/CONFIG.md docs/engineering/contracts/PIPELINE_BLOCK.md
 docs/engineering/contracts/CONFIG.md         | 212 ++++++++++++++-------------
 docs/engineering/contracts/PIPELINE_BLOCK.md |  42 +++---
 2 files changed, 139 insertions(+), 115 deletions(-)
```

---

## 八、自证段

**白名单遵守**：本代理只写 `docs/engineering/contracts/CONFIG.md` 与 `docs/engineering/contracts/PIPELINE_BLOCK.md` 两个文件，加本交付件 `run/GOVERN-08/审核包-R2/T06-第1轮订正-subB.md`。未新建任何文档、未新建 `DOCUMENT_INDEX.yaml` 条目、未触碰 `docs/science/**`、`docs/detail/**`、`AGENTS.md`、代码与 schema。

```
$ git -c core.quotepath=false diff --name-only
（本次仅含上述两个白名单文档；其余 M 项为并发代理在同期的改动，非本代理所为）

$ git -c core.quotepath=false status --short | grep -c "^ M"
30   ← 其中仅 2 项属本代理；其余 28 项在 §⓪ 已如实披露为并发代理改动
```

**git 只读**：全程只用 `git -c core.quotepath=false status/diff/diff --stat/log`；未执行任何 `add/commit/checkout/reset/stash/tag/push`，未改分支、未写索引。

**红线遵守自查**：

1. **禁编造** —— 所有公式、常数、键集合、节名均来自一手可复核来源：节名来自目标文档实读的 `##` 标题（`grep -n "^#\{1,3\} "` 逐篇输出已贴）；键集合来自 `stage2_common.cpp` 允许键字面量与三份 schema 的 `description`；run_manifest 字段来自 `commands.cpp:535-558` 与 schema 的 `required`/`properties`；noise 默认值来自 `noise_model.cpp` 的 `snr_noise_model_v1_default_config`。核对不到的一律写「核对不到」（A-1、A-2）。
2. **不删真内容** —— 门表 14 条判据、WBPP 冻结表、C1–C3、块字段表、`GAP-025`、活测试 ID 全部保留；删除的只有失效登记（不存在的 `weight` 分组、不存在的 `PSF_SIGNAL_SNR`、无处定义的 `R-54`）与不可执行的命令块（且以「无执行器 + 判据由人读」替代，未让读者误以为可复跑）。
3. **保留原判** —— `:85`、`class` 判定为真的部分（phase_config 面键集合、`:100` 范围错位、C8 排序、块字段表）在 §三 逐条记录依据后保留；推翻的 6 条判定均附可复核依据。
4. **文档规范** —— 抬头无元信息块；正文已无 `M3`、`（V17 冻结）`、`已从 parser 删除`、`作废`、行号锚；`§N` 机械跳转锚计数为 **0**；跨文档引用全部改为**真实节名**或论文式内容锚；本文件内引用一律「本文件/本块的…一节」块内自引，未设任何锚点；引号与反引号配平已核。

**诚实性声明**：本单有 **1 处未能按原计划自行完成**（PIPELINE_BLOCK 的 C 编号改名，因并发代理先落，改为只做 3 处残留订正并如实披露）、**1 处主动降级为待裁决**（`V17StatusesExplicit` 不改）、**1 处主动搁置并登记**（分组表计数漂移）、**9 项代码侧缺口**与 **5 项待权威补充**全部登记而未擅自填补。这些均不是「已解决」，请前台按 §四/§五 派单。

**未提交**：按 AGENTS.md 第 9 条，子代理不做 git 写操作；本单改动留在工作树，由前台独立复核后统一原子提交。

---

## 九、第二代理独立复核补遗（追加，不覆盖 §⓪–§八）

**背景更正（相对 §⓪ 的角色对调）**：本节由**另一代理**（下称 B）追加。B 与 §⓪–§八的作者（下称 A）是**同一派单的两次重复派发**，两人都在改这两个白名单文件：

- B 于本会话早期完成 **`PIPELINE_BLOCK.md` 的 `PC-C*` / `IR-C*` 改名空间、两处「判据域」声明、`:100` 声明范围 `C4–C7 → IR-C4–IR-C8`、`IR-C8` 移至末位**（即 §⓪ 所称「先到者」）；
- A 于同期完成 **`CONFIG.md` 的全部改动**，随后发现 B 已落 `PIPELINE_BLOCK.md`，未回滚、只做 3 处残留订正（即 §二 所记）；
- B 稳定采样确认 `CONFIG.md` 连续 5 分钟无变化后，在 A 的成果之上补了 3 处**新增**订正（见 §九.3），其余一律保留 A 的文本。

⇒ **两份成果互补、无相互覆盖**，前台可直接按 §七验证输出复核。

### §九.1 A 未登记的最高风险项：`CONFIG.md` 的 `##` 序号塌缩打乱 `eng/**` 的节号引用

A 的 P0 处置把 12 个 `##` 降为 `###`、只留 4 个 `##`（三个配置面 + 参考文献）。**`eng/**` 侧有 11 处按 `§N` 或 `文件:行` 引用 `CONFIG.md`，其中 2 处原本正确、现全部失效。** 这些文件**不在本单白名单**，A 的 §四未登记此项。

改前 vs 改后的 `##` 序号对照（`git show HEAD:…` 与当前文件实读）：

| 引用方（`eng/`，白名单外） | 引用形态 | HEAD 版 `§N` 实际指向 | HEAD 版是否正确 | 当前版是否正确 | 性质 |
|---|---|---|---|---|---|
| `packaging/config/config_registry.json:858`（`conflict.evidence[]`） | `CONFIG.md §2` | §2 = 三类配置（现场清单） | ✅ 正确 | ❌ §2 现为「配置面二 · orchestrator Stage2 配置」 | **本轮回归** |
| `packaging/config/filters.json:864`（`non_key_examples[].note`） | `CONFIG.md §10` | §10 = 滤镜名匹配语义 | ✅ 正确 | ❌ 当前无 §10 | **本轮回归** |
| `schemas/phase_config_{mosaic,normalize,export}.schema.json:32/27/33` | `CONFIG.md §3` | §3 = defaults.json（作者本意 = §4 三命令 phase_config 与模板） | ❌ 改前即错 | ❌ | 既有缺陷，非本轮 |
| `schemas/cpu_profile.schema.json:24` | `CONFIG.md §5` | §5 = filters.json（作者本意 = §6 cpu_profile） | ❌ 改前即错 | ❌ | 既有缺陷，非本轮 |
| `contracts/ledgers/dead_config_keys.json:118` | `CONFIG.md:82（mosaic 行）` | HEAD 行 82 = mosaic 行 | ✅ 正确 | ❌ 当前 mosaic 行在 :100 | **本轮回归（行号）** |
| `ledgers/dead_config_keys.json:36/51/110`、`schemas/phase_config_export.schema.json:368/497` | prose 内嵌 `CONFIG.md §3` / 行号 | — | 混杂 | 混杂 | 需逐条重锚 |

```
$ grep -n "^## " docs/engineering/contracts/CONFIG.md | cat -n
     1	16:## 配置面一 · 三命令 phase_config 与三类配置
     2	252:## 配置面二 · orchestrator Stage2 配置
     3	357:## 配置面三 · 阶段生产调用链与 Stage1 配置
     4	404:## 参考文献
```

**处置建议（需白名单外一轮）**：登记面与 schema `description` 一律**弃用 `§N`**，改用**真实节名**（如「三命令 phase_config 与模板」「cpu_profile.schema.json 一节」「滤镜名匹配语义」）。这与 `AGENTS.md` §5「不使用「见 §几」式的机械跳转锚」同向，且能一次性消除此类漂移。**本单未处置（白名单外），在此登记。**

### §九.2 `filters.json` 的 `where.quote` 内容锚**改前即已失效**（非本轮引入）

```
$ python3 - <<'PY'
import json
q=json.load(open('eng/packaging/config/filters.json'))['lookup']['non_key_examples'][0]['where']['quote']
print('HEAD 版命中次数 =', open('/tmp/cfg_head.md',encoding='utf-8').read().count(q))
print('当前版命中次数 =', open('docs/engineering/contracts/CONFIG.md',encoding='utf-8').read().count(q))
PY
HEAD 版命中次数 = 0
当前版命中次数 = 0
```

`where.quote` 存的是 `…唯一权威锚点 = 本文件 §10 负例表…`，而 `CONFIG.md` 自上一轮起就已改写成 `…本文件 「滤镜名匹配语义」一节 负例表…`。⇒ 门 CFG002-04「引文唯一命中」对这两条 `non_key_examples` **在本轮之前就已判红**，本轮**既未使其变好也未使其变坏**。`filters.json` 不在白名单，**登记待下一轮重锚**（重锚时应把 `quote` 同步为本文件当前逐字文本，并把 `note` 里的「第 198 行」「§10」一并改为节名）。

### §九.3 B 补入的三处 CONFIG.md 订正（A 未覆盖的证据缺口）

A 在 §五 A-1 / A-2 把 PixInsight 官方式与 WBPP `engine.js:1421-1429` 一律登记为「一手原文核对不到」并**撤掉了全部出处主张**。B 复核后认为这**过度纠正**：一手原文确实核对不到（B 独立复核：`web_search` 对两组查询均 `No results found`；`pixinsight.com/doc/modules/ImageIntegration/{index,ImageIntegration}.html`、`pixinsight.com/doc/index.html`、`pixinsight.com/documentation.html` 全部 HTTP 404；`pixinsight.com/` 根 200 但为 JS 壳、无正文；WBPP 为 PixInsight 付费更新包，不可公开取得）——**但仓内科学正本已逐字承载该出处登记**，撤掉属于删真内容（红线 3）。B 据此补回指向，未新增任何文献表条目。

| 处 | 改前逐字（A 的文本） | 改后逐字（B 的文本） |
|---|---|---|
| 对照档出处 | 「该表的档界本应逐档对应 WBPP 的 `bestRejectionMethod`；**其一手出处在本仓核对不到**（WBPP 2.5.9 的 `engine.js` 对应行、包 sha1 与 WBPP ≤2.3.x 旧表均未能取得原文），故此处的对照档只承担**本仓冻结行为**，不主张与 WBPP 逐档同源；需权威补源后另行登记。」 | 「…**其一手原文在本仓核对不到**（`pixinsight.com/doc/**` 各路径均返回 404，WBPP 为 PixInsight 付费更新包、不可公开取得），故此处的对照档只承担**本仓冻结行为**，不主张与 WBPP 逐档同源。**仓内权威登记存在**：`../../science/REJECTION.md` 的「14 Primary literature（引用定位声明）」第 3 条与「14a 参考文献与参考代码库（含许可证）」一节逐字登记了档界对照的可核验形式—— WBPP 2.5.9 的 `WeightedBatchPreProcessing-engine.js:1421-1429` `bestRejectionMethod()`、官方更新包 sha1 `712cc7c3fdb523643ad0e685104592d511996f82`、该版 `n > 15` 档为 `Rejection_ESD` 与 WBPP ≤2.3.x 该档为 `n < 25 → LinearFit`。本块以那两节的登记为准，需一手原文补源后另行登记。」 |
| `winsorized_sigma` 语义来源 | 「其语义来源应为一手权威文档（PixInsight ImageIntegration 官方式）；**该一手出处在本仓核对不到**，本节只陈述本仓实现口径，不以未经核对的文献编号背书。」 | 「**一手原文（PixInsight ImageIntegration 官方文档）在本仓核对不到**，故本块不以文献编号背书，只陈述本仓实现口径。仓内权威登记见 `../../science/REJECTION.md` 的「14 Primary literature（引用定位声明）」第 2 条：语义来源为 PixInsight ImageIntegration 官方文档中的相应公式（±1.5σ winsorize、常数 1.134、迭代限 5e-4），概念出处 = Huber & Ronchetti 2009, *Robust Statistics* 2nd ed.。」 |

**逐字来源**（`docs/science/REJECTION.md` §14 第 2/3 条与 §14a「档界对照（可核验形式）」条，B 已 `sed -n '264,320p'` 实读）：
- §14 第 2 条：「本层 `winsorized_sigma` 的**语义来源 = PixInsight ImageIntegration 官方文档式[18]/[19]**（±1.5σ winsorize、常数 1.134、迭代限 5e-4；概念出处 = Huber & Ronchetti 2009, *Robust Statistics* 2nd ed.）」
- §14a：「PixInsight WBPP **2.5.9** `WeightedBatchPreProcessing-engine.js:1421-1429` `bestRejectionMethod()`（官方更新包 sha1 `712cc7c3fdb523643ad0e685104592d511996f82`…）。该版 `n > 15` 档为 `Rejection_ESD`；WBPP ≤2.3.x 该档为 `n < 25 → LinearFit`」

⇒ **A 的 A-1 / A-2 结论「核对不到原文」成立且保留**；B 只补回「仓内已有权威登记」这一事实，**未新增文献、未新增常数、未新增公式**。两处原文均按 §七 命令 2 复核：`grep -nE "\[1[89]\]" docs/engineering/contracts/CONFIG.md` = **0 命中**（`式[18]/[19]` 这两个 token 已被 B 改写为「官方文档中的相应公式」，避免裸方括号编号被误读为本篇参考文献编号）。

B 另补 1 处纯格式订正：`### Stage1 config` 段末与 `## 参考文献` 之间补空行（CommonMark 下 `##` 不强制前置空行，但与本文件其余 `##` 的写法不一致）。

### §九.4 B 对 A 若干处置的独立复核结论（全部**支持**，附机器证据）

| A 的处置 | B 的复核 | 证据 |
|---|---|---|
| 删除 CONFIG.md 分组表的 `weight` 行（§一.2） | **确认不是「删真内容」**：`eng/packaging/config/defaults.json` 实测 `field_count=59`、实到 59 条，分组为 calibration 2/cosmetic 6/detection 1/drizzle 1/hips 1/noise 14/paths 1/photometry 6/precision 1/psf 2/rejection 18/scalar_gate 2/snr 1/sparse_snr 2/upm 1 ——**无 `weight` 分组**，`weight` 前缀键 0 个；`phase_config_mosaic.schema.json` 的 `description` 逐字登记「三套键名一并作废 … `eng/packaging/config/defaults.json#weight.default_mode` 组同批注销」 | `python3 -c "import json;…"` + `grep -n weight_mode eng/contracts/schemas/phase_config_mosaic.schema.json` |
| A 的 §三「`V17StatusesExplicit` 是活测试 ID，保留」（N-6） | **支持**：该 ID 被 `eng/tools/quality/v19r3_traceability.py:95` 与 `:165` 消费，并登记于 `docs/engineering/governance/TRACEABILITY.md:269/276`；改名即打断机器引用 | `grep -rn "V17StatusesExplicit" lib/ eng/ docs/`（5 处非 CONFIG.md 命中） |
| A 的 C-8「`stage2_common.h` 不存在」 | **支持**：`ls lib/algorithms/coverage/src/stage2_common.h` → No such file or directory | 同左 |
| A 引 `noise_model.cpp` 的 `snr_noise_model_v1_default_config` 为数值唯一源 | **支持**：该符号存在于 `lib/algorithms/noise_snr/cpp/src/noise_model.cpp`，并被 `lib/infrastructure/pipeline/orchestrator/cpp/src/orchestrator.cpp:4318` 与 `lib/infrastructure/scheduler/src/module_adapters.cpp:6766` 调用 | `grep -rn "snr_noise_model_v1_default_config" lib/` |
| A 在配置面二声明「`weight_mode` 作为现行键只出现在 `sky_plane` 子段」 | **支持**：`stage2_common.cpp:270` 的 `sky_plane` 允许键含 `weight_mode`，`:281` 读入；`:21` 与 `:594` 另注明 `integration.weight_mode` 为退役键 | `grep -n "weight_mode\|sky_plane\|acr_route" lib/algorithms/coverage/src/stage2_common.cpp` |

### §九.5 B 独立发现、需前台注意的两点

1. **§七 命令 3 的 4 条命中全部为正则误报或活标识符**，B 逐条复核结论与 A 一致：`GAP-025` ×2 命中的是 `P-[0-9]+`（`GAP-025` 尾部）、`TEST-SNR-001` 命中的是 `R-[0-9]+`、`V17StatusesExplicit` 是活测试 ID。**该 grep 不足以判 9-5 是否收敛**，建议前台在门里改用 `\bV17\b|\bM3\b|已从 parser 删除`（带词边界、排除测试 ID）以免长期假阳性。
2. **`§九.1` 的节号回归是本轮唯一未被任何人登记的 P0 副作用**，且落在 `eng/**`（机器消费面）而非文档面。建议前台在合并前先派一轮「登记面弃用 `§N` 改节名」的单，否则 `filters.json` 与 `config_registry.json` 的两处 `conflict`/`note` 会持续指错节。

### §九.6 B 的自证

- **白名单**：B 只写 `docs/engineering/contracts/CONFIG.md`、`docs/engineering/contracts/PIPELINE_BLOCK.md` 与本交付件；未新建文档、未动 `DOCUMENT_INDEX.yaml`、未触碰 `docs/science/**`、`docs/detail/**`、`AGENTS.md`、代码与 schema。
- **git 只读**：全程只用 `git -c core.quotepath=false status / diff / diff --stat / log / show HEAD:<path>`；**未执行任何写操作**（`add/commit/checkout/reset/stash/tag/push` 一律未用），未改分支、未写索引。
- **不覆盖**：B 未回滚 A 的任何 CONFIG.md 文本；`PIPELINE_BLOCK.md` 的改名空间与判据域声明由 B 先落、A 后补 3 处，两人在该文件上**无冲突、无相互覆盖**。
- **诚实性**：§九.1 的 3 处 `§N`/行号回归是**本轮 P0 处置引入的**（非既有缺陷），B 已在 §九.1 明确标注「本轮回归」并给出 HEAD 对照；§九.2 的内容锚失效经 HEAD 对照确认**改前即已存在**。两者均**只登记未擅自修复**（`eng/**` 不在白名单）。
