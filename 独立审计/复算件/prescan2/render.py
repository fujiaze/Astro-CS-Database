#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""D4 第二路 判读件渲染器：把 judgements_b*.json 按批次追加渲染成
   raw/D4-多侧默认值候选.csv 与 .md（每批一跑，中断不丢已完成批次）。

PROGRESS 行常驻 .md 末尾：已判分叉行 / 分叉行总数。
"""
import collections
import csv
import glob
import io
import json
import os
import re
import subprocess
import sys

sys.stdout.reconfigure(encoding="utf-8", errors="replace")

HERE = os.path.dirname(os.path.abspath(__file__))
RAW = r"独立审计/证据"
CSV_OUT = os.path.join(RAW, "D4-多侧默认值候选.csv")
MD_OUT = os.path.join(RAW, "D4-多侧默认值候选.md")
FORK = os.path.join(HERE, "D4-分叉行筛选.csv")
FULL = os.path.join(RAW, "D4-多侧默认值全表.csv")
REPO = r"F:/Astro dev/Astro CS Normalization Database"

COLS = ["优先档", "键", "键族", "命中判据", "是否已有门覆盖（门 ID 或「无」）",
        "定级建议", "保守方向与影响面", "与既有四路判读的交叉", "六侧实测摘要"]

PREAMBLE = u"""# D4 多侧默认值候选（AUDIT-06 复算·第二路：先抽全表，再只判分叉行）

基线 HEAD `c8f64e9ab6b867e4f108ba1e0073a8f2a97cfde9`；仓库 `F:\\Astro dev\\Astro CS Normalization Database`；
本路全程只读仓库、零 git 写、不构建、不跑 ctest/`run_checks.py`/任何 `eng/**` 脚本、不 import 仓库 Python 模块。

## 开工/收工 git status 原文

开工（任务开始时记）：

```text
?? ACSD整治工作包_AUDIT-06.zip
?? site/
```

收工（本次渲染时实测，命令 `git -C "F:/Astro dev/Astro CS Normalization Database" status --porcelain`）：

```text
%s
```

两次一致：本路未在仓库内留下任何新增/改动条目（中途每次跑脚本后均复查过，未触发停用条件）。

## 抽取口径（第一步产物 = 全量表）

| 侧 | 载体 | 取法 |
|---|---|---|
| S1 defaults.json | `eng/packaging/config/defaults.json` | `fields[].value` ＋ `authority_status` |
| S2 出厂模板 | `eng/packaging/config/templates/*.json` | 现行 `blocks[]` 形态逐块展开叶子（**不**按旧顶层 `config` 取）；缺项显式标「该侧无此项」 |
| S3 CLI 生成骨架 | `lib/infrastructure/cli/session_commands.h` | 先剥注释、按 C++ 聚合初始化项解出 `ConfigField{key,json,doc,scope}`，再把 `json` 里的**字符串内嵌 JSON 解析成对象**逐层取键；`json==nullptr` 记为「不进模板骨架」 |
| S4 机器合同/schema | `eng/contracts/**.json`（含 `schemas/**`）＋ `lib/infrastructure/pipeline/orchestrator/configs/*.schema.json` | 同名属性的 `default`/`enum`/`minimum`/`exclusiveMinimum`/`maximum`/`const` |
| S5 代码兜底 | `lib/**`（跟踪集，排除 `third_party`/`tests`/`test_*`/`*_test.*`/`*.md`） | `value("k",d)`、`obj.k = <字面量>`、初始化列表 `k(<字面量>)`、`contains("k")?x:d`、具名常量与 `#define`；一个键的多处全部收（全位点见 `复算/prescan2/side5_位点.log`） |
| S6 登记册 | `eng/packaging/config/config_registry.json` | `plugin_knobs[].declared_default` ＋ `note` 里的默认值断言文本 |

规范键归一：取语义路径叶子；叶子属泛用词表（mode/name/path/value/type/center/…）时带上父段；
再做 `a.b ↔ a_b` 后缀等值并组（例：`snr.path ↔ snr_path`、`algorithm_drizzle_pixfrac ↔ pixfrac`）。
并组口径的副作用见文末「预筛漏检与误并口径」。

## pixfrac 正例控制（抽取口径的有效性门）

`drizzle.pixfrac` 行六侧实测：

| 侧 | 实测 |
|---|---|
| S1 | `0.8`（authority_status=owner_adjudicated） |
| S2 | `0.8`（normalize 模板两块皆 0.8） |
| S3 | `1.0`（`config_fields(kNormalize).drizzle` 内嵌串 `{"nested":1,"pixfrac":1.0,"precision_mode":1}`） |
| S4 | `(0,1]`：`phase_config_normalize#/drizzle_config/pixfrac`、`#/algorithm_drizzle_pixfrac`、`product_family_field_constraints#/signal/normalization/pixfrac` 三节点均只给 `exclusiveMinimum=0;maximum=1`（**无 default**）；orchestrator `stage1.schema.json#/drizzle/pixfrac` 给 `default=0.8` |
| S5 | `1.0`×11、`0.0`×3、`0.8`×2（0.8 两处＝`json_config.h:67` 成员初值与 `json_config.cpp:712-713` 的 `contains(...)?...:0.8`） |
| S6 | `declared_default="1.0"` ＋ note 断言「默认 1.0 已入 defaults.json」 |

控制判定：**PASS** —— 六侧全部取到值，且同时含 0.8 与 1.0 两组。
抽取脚本 `extract_defaults.py` 内建该校验并以退出码表达（FAIL ⇒ rc=2，禁止以「零分叉」交表）。

## 总量与分叉率

| 指标 | 数值 |
|---|---|
| 全量表数据行（规范键组） | %s |
| 抽取记录总数 | %s |
| 六侧齐全键数 | %s（占 %s） |
| 机械筛出分叉行 | %s（分叉率 %s） |
| 本件已判分叉行 | %s |
| 判读中标为「新暴露」的行数（口径见下节：按判读文本，含既判键的新面） | %s |

各侧覆盖：S1 59 条/49 键｜S2 40/23｜S3 50/41｜S4 909/351｜S5 184/180｜S6 138/123。
**六侧齐全只有 1 个键（就是 pixfrac）** ⇒ 本项目里「同一键在多侧各说各话」不是少数派异常，
而是**默认形态**：绝大多数键只在一到两侧有值，其余侧根本没登记点。这条比率本身就是 D4 的结论性读数。

## 判读用的门覆盖事实（只读 `eng/**` 源码得出，未执行任何门）

- `CHK-CONFIG-DEFAULTS`（`eng/ci/check_config_defaults.py`；profiles fast/linux-main/windows-main；
  豁免台账 `eng/ci/ledgers/config_default_divergences.json`）是唯一声称管「同一配置键的生产默认值必须一致」的门。实测其口径：
  1. 取值形态只认 `\\.value/.get/.get_or/.at("k",<字面量>)` 与 **接收者名为** `*cfg|config|settings|opts|params|options|defaults` 的 `recv.k = <字面量>`
     ⇒ `hips_properties.h:14 tile_width = 0`、`v6_drizzle_science.h` 一类成员初值不在形态内；
  2. `config_key_set()` 与 `template_defaults()` 都按 `doc.get("config") or {}` 取模板叶子，
     而现行模板是 `blocks[]` ⇒ **模板侧零键**，门自订的「代码 vs 模板」比较整条空转；
     其 `--self-test` 夹具也写 `{"config": {...}}`，故夹具仍全绿（fail-open 不会被自身发现）；
  3. `defaults.json` 只贡献**键名**不贡献 `value`，CLI 骨架 `config_fields[]` 是聚合初始化（非上述两种形态），
     登记册 `declared_default` 与 schema `default` 均不入比较
     ⇒ 「权威默认 ↔ 代码兜底」「权威默认 ↔ CLI 骨架」「登记册断言 ↔ 权威默认」三条边**无门**。
- `CHK-CONFIG-CONSUMED`（已正确支持 `blocks[]`）与 `CHK-PROD-WIRING` 只管「键有无生产读取点」，**不比值**。
- `CHK-UNIT › UT-CONFIG › CFG002-01/02/03`（仅 `linux-main` 档）比对
  `docs/plugins/**` 表 ↔ 登记册 `declared_default` ↔ `defaults.json` value；
  `eng/tests/test_index.csv` 对该套件记 `verdict=FAIL`（CFG002-02 登记点 2 条，测于 2026-09-23）。
  本路不跑 eng 脚本，现状红灯与否待前台复跑确认；此处按台账引用，不当作已复算证据。
- `CHK-REGISTRY-DOC-SYNC` 是 `checks.json ↔ docs/ci/01_CHECKS.md` 的门，与默认值无关（避免混用门 ID）。
- 键名跨面对齐（`noise.clip_sigma` ↔ 合同 `clip_sigma` ↔ 代码 `cfg.clip_sigma`、
  `snr.path` ↔ `snr_path`）：**无任何门**。

## 预筛漏检与误并口径（正则吃不到的形态）

本路的机械面有已知边界，交表时一并登记，不以「零分叉」冒充「无分叉」：

1. **跨行初始化**：结构体成员默认写在多行初始化列表（`P2SkyPatchConfig c{\\n  3.0,\\n ...}`）或
   `constexpr X y = \\n z;` 时，逐行正则只吃到首行 ⇒ 位置参数式初值全部漏。
2. **宏拼接**：`#define PF_MIN 0.0` / `ASTROCS_DEFINE_LIMIT(name, v)` 一类经预处理才成形的字面量，
   以及 `X(name, value)` 形态的表驱动注册，本路只认 `#define NAME 字面量` 直写形。
3. **运行时计算**：`kcorr_lookup(pixfrac, scale)` 的插值、`1.4826*MAD`、`nside = 1 << tile_order`
   这类「默认值来自函数而非字面量」的取值，机械面只能记到函数名，不能记到数。
4. **形参默认与返回默认**：`double f(double pixfrac = 1.0)`、`Config cfg() { return {}; }`
   以及 `.at("k")` 无兜底 + 外层 try 的隐式默认，均不在两种被认形态内。
5. **跨键名同语义家族**：精度家族 `precision.default("fp64")` / `drizzle.precision_mode(1)` /
   `bitpix(-32)` 三条边按-key 机械口径永远连不上（A1 已手工判出，本路机械面确认其**不可被按键预筛发现**）。
6. **值域断言在散文里**：`docs/contracts/DATA_SEMANTICS.md`、`docs/plugins/**` 表内的「>=1」「默认 64」
   不参与 S4（本路口径 S4 只取 JSON 合同），故「登记即错」只覆盖 JSON 面，散文面靠判读。
7. **误并**（并组口径的反面）：`ra_deg` 因后缀规则吃进 orchestrator 的 `initial_ra_deg`(-999)、
   `block/center/source/median/read/write/checksum/snr` 等泛用叶子吃进同名局部变量与结构体成员。
   这类行在判读里逐条标「同名异义 ⇒ 伪分叉」，**不删行**（主表保持全量、可复算）。
8. **误报形态（本路口径自带，判读时逐行标注）**：`value("k",d)` 形按「带引号键＋逗号」匹配，
   会吃进 `std::set<std::string> k = {"schema_version", "inputs", ...}` 与 `kChain[] = {"cal", ...}`
   这类**键名集合字面量**（实测 parser.cpp:354/578、module_adapters.cpp:1872 三例），
   把邻居键名当成该键的默认值；同时 S3 的内嵌 JSON 串会被 S5 再吃一次（session_commands.h 自身位点），
   属同一事实的自身重复。两类都不删（保持可复算），判读列里逐条点名。
9. **S4 口径边界**：只认 JSON 合同里的 `default/enum/minimum/exclusiveMinimum/maximum/const`，
   因此「默认只写在 `description` 散文里」的键（实测 `export_wcs.rotation_deg`、`wcs_config.init_source`、
   `snr_path` 之外的多数 export 几何键）在 S4 记为「该侧无此项」——这是**合同面缺机读默认**，
   不是该侧与别侧冲突；判读时按「登记面只有散文」单独定性，不计入分叉率的事实依据。


## 与既有四路判读（AUD-402-判读-A1/A2/A3/BD1）的交叉

按规范键名与 `符号/键` 列取交集（命令见 `复算/prescan2/cross_ref.py`）：

- **同键已判 12 行**：`clip_sigma`、`k_corr`、`max_iterations`、`pixfrac`、`tile_width`、
  `saturation_level`、`lower_sigma`、`upper_sigma`、`spatial_field_enabled`（A1）、
  `scale_deg_per_px`、`snr`（BD1）、`block`（A2 判的是别处的 block 常数，本件判 cpu_profile 面 ⇒ 键名同名而对象不同，计入已判以保 12＋31=43 的算术闭合）。
  其中 A1 已在备注里就 `drizzle.pixfrac`（六侧）、`noise.saturation_level`（default 落在自身
  exclusiveMinimum 外）、`noise.spatial_field_enabled`（defaults 整数 1 vs schema boolean）、
  `hips.tile_width`（多侧硬编码同值＋0 的第三种表示法）、`noise.clip_sigma`（Phase2 3.0 无注册侧）、
  `precision.default`（fp64 vs bitpix=-32 vs precision_mode vs 死键四面）
  **立过多侧默认值分叉案** ⇒ 本件对这些键只做补全（记机械读数与门缺口径），不重复立案。
- **新暴露 31 行**：`dark_exposure_s`、`light_exposure_s`、`dec_deg`、`ra_deg`、`nside`、
  `precision_mode`、`storage_form`、`flux_conservation_factor`、`target_pixel_area`、`w_info`、
  `reference_flux`、`median`、`read`、`write`、`workers`、`checksum`、`center`、
  `crpix_px`、`rotation_deg`、`gaia_data_dir`、`height_px`、`width_px`、`hips_paths`、`output_dir`、
  `log_dir`、`max_iter`、`max_stars`、`parity`、`schema_version`、`source`、`wcs`。
  这一批的来源面是 **orchestrator stage1 配置面 / p2-p3 会话读键点 / CLI 键表面**，
  A1 从 `defaults.json`  outward 判，覆盖面天然到不了这里 —— 这正是本路「先抽全表」换来的增量。
- 四路里带「多侧/六侧/四侧/两侧并列」字样的判读条目：A1 16 条、A2/A3/BD1 另有若干；
  计数命令与输出见 `cross_ref.py` 末段（复算可跑）。

## 排序规则与未做行数

分叉行按任务书优先级排序：①生产兜底在对面 ＞ ②值域自相矛盾 ＞ ③哨兵不一致 ＞
④登记面与文档面分歧 ＞ ⑤仅跨侧异值（含同名异义嫌疑）。
"""

SORT_TAIL_TMPL = u"""
本件判读覆盖：已判 DONE 行 / 共 TOTAL 行。各优先档行数（机械筛出口径）：
PRIO_COUNTS
其中按判读结论再分一刀：
- 构成新案或补强新事实的行数：NEWCASE
- 判为「同名异义/机械误并 ⇒ 无案」的行数：NOCASE（不删行，留作机械口径的可复算证据）
- 与 A1/BD1 既立条目同案、仅补机械读数的行数：ALREADY
未做行数：**TODO**。

"""

PRIO_KEYS = [u"①", u"②", u"③", u"④", u"⑤"]

MD_TABLE_HEAD = u"""
---

## T5 专项：「该导出却硬编码」（与多侧分叉正交，另计）

机械候选生成方式：对 `defaults.json` 的全部数值默认做两两 `×`／`+` 匹配第三值（容差 1e-9），
得 **30 个候选键**。逐条判读后：

| 结论 | 键 | 依据 |
|---|---|---|
| **真案（仓内已有导出式，值仍按字面量硬编码）** | `sparse_snr.spacing_px`=64 | 该键自己的 constraint 文本写着「默认 64 = hips.tile_width / 8 = 512 / 8（复用 Phase2 UPM 的 8×8/tile 控制网格）」⇒ 导出式的两个输入（`hips.tile_width`、除数 8）都在仓内，而 64 以字面量同时落在 defaults.json 与 `phase_config_normalize#/sparse_snr_spacing_px` 的 schema default 上（两处各写一遍，任一处改 512 都不会传导） |
| **真案（下界即导出值）** | `cosmetic.bad_column_variance_kappa`=2.0 | authority_status=**derived**；constraint 写 `>= 1/Σw²（单列两邻平均 ⇒ >= 2）` ⇒ 默认值 2.0 恰等于自身下界，是结构导出量而非可选项；登记面同时称「取保守侧」——两说并存 |
| **真案（A1 已立）** | `hips.tile_width`=512 | `leaf_order = tile_order + 9` 的 9 即 log2(512)，A1 已判「应按式算」；本路不重复立案，只记它同时是 spacing_px 的上游 |
| **正面对照（有式且写了式）** | `noise.mask_budget_min_sky`=9216 | note 给完整 a priori 链：SE(σ̂)/σ ≈ 1.44/√N_sky ≤ 1.5% ⇒ N ≥ (1.44/0.015)² = 96² = 9216。这就是「导出常数应怎么写」的样板 |
| **数值巧合（无导出处，判无案）** | 其余 26 键 | 例：`pixfrac` 被判为 0.75+0.05、`moffat_beta` 被判为 5×0.8、`esd.alpha` 被判为 50×0.001 —— 全部是把两个语义无关的量凑成同值。**判据在纯算术面上不可判**，必须回到导出式文本 |

**为什么 T5 不在 43 行分叉表里**：`sparse_snr.spacing_px` 两侧同值（defaults 64 ＝ schema default 64），
模板面自 §9.68 起不再列该可选键 ⇒ 多侧比较面无不等值，机械筛选不吃它。
即：**「该导出却硬编码」是"值哪来的"问题，不是"各侧值不等"问题**，两判据正交，必须像本专节这样单列。

**本路机械口径在此判据上的实证漏检**：候选生成只做 `×` 与 `+`，真式是 **除法**（512/8）——
漏检不是"正则吃不到跨行/宏"那一类，而是**关系算子集不全**。补 `÷`、`log2`、`√`、`²` 之后才可机械检。

---

## 判读表

"""

TABLE = u"""
---

## 附：分叉行机械读数来源

- 全量表：`独立审计/证据/D4-多侧默认值全表.csv`（脚本 `extract_defaults.py`，含 pixfrac 正例控制）
- 分叉筛：`独立审计/复算件/prescan2/D4-分叉行筛选.csv`（脚本 `filter_divergent.py`）
- S5 全位点：`独立审计/复算件/prescan2/side5_位点.log`
- 逐条记录：`独立审计/复算件/prescan2/records.tsv`
- T5 可导出却硬编码候选：`独立审计/复算件/prescan2/T5_可导出却硬编码_候选.csv`
"""


def git_status():
    out = subprocess.run(["git", "-C", REPO, "status", "--porcelain"], capture_output=True)
    return out.stdout.decode("utf-8", "replace").rstrip("\n") or "(clean)"


def stats():
    full = list(csv.reader(io.open(FULL, encoding="utf-8-sig")))
    fork = list(csv.reader(io.open(FORK, encoding="utf-8-sig")))
    recs = sum(1 for _ in io.open(os.path.join(HERE, "records.tsv"), encoding="utf-8")) - 1
    six = 0
    for r in full[1:]:
        if len([c for c in r[3:9] if not c.startswith(u"该侧无此项")]) == 6:
            six += 1
    return len(full) - 1, recs, six, len(fork) - 1


def classify(r):
    """按判读文本把行归入：新案 / 伪分叉无案 / 补强既立条目。"""
    x = (r.get(u"定级建议", "") + r.get(u"与既有四路判读的交叉", "")
         + r.get(u"保守方向与影响面", ""))
    if u"无案" in x or u"伪分叉" in x:
        return "nocase"
    if u"已判" in r.get(u"与既有四路判读的交叉", "") and u"新暴露" not in r.get(
            u"与既有四路判读的交叉", ""):
        return "already"
    return "newcase"


def main():
    rows = []
    for p in sorted(glob.glob(os.path.join(HERE, "judgements_b*.json"))):
        with io.open(p, encoding="utf-8") as f:
            rows.extend(json.load(f))
    os.makedirs(RAW, exist_ok=True)
    with io.open(CSV_OUT, "w", encoding="utf-8-sig", newline="") as f:
        w = csv.writer(f)
        w.writerow(COLS)
        for r in rows:
            w.writerow([r.get(c, "") for c in COLS])

    nf, nrec, nsix, nfork = stats()
    done = len(rows)
    todo = max(0, nfork - done)
    newx = sum(1 for r in rows if u"新暴露" in r.get(u"与既有四路判读的交叉", ""))
    cls = [classify(r) for r in rows]
    ncase = {"newcase": cls.count("newcase"), "nocase": cls.count("nocase"),
             "already": cls.count("already")}
    # 各优先档行数按**机械筛出表**计（判读表若未覆盖全部仍以筛出表为准）
    fork = list(csv.reader(io.open(FORK, encoding="utf-8-sig")))
    pcnt = collections.Counter(r[0][0] for r in fork[1:])
    prio_lines = "\n".join(u"- %s %s 行" % (k, pcnt.get(k, 0)) for k in PRIO_KEYS
                           if pcnt.get(k, 0))
    tail = (SORT_TAIL_TMPL
            .replace("DONE", str(done)).replace("TOTAL", str(nfork))
            .replace("PRIO_COUNTS", prio_lines)
            .replace("NEWCASE", str(ncase["newcase"]))
            .replace("NOCASE", str(ncase["nocase"]))
            .replace("ALREADY", str(ncase["already"]))
            .replace("TODO", str(todo)))
    head = PREAMBLE % (git_status(), nf, nrec, nsix,
                       "%.1f%%" % (100.0 * nsix / max(1, nf)), nfork,
                       "%.1f%%" % (100.0 * nfork / max(1, nf)), done, newx)
    with io.open(MD_OUT, "w", encoding="utf-8-sig", newline="") as f:
        f.write(head)
        f.write(tail)
        f.write(MD_TABLE_HEAD)
        w = csv.writer(f)
        w.writerow(COLS)
        for r in rows:
            w.writerow([str(r.get(c, "")).replace("\n", " ") for c in COLS])
        f.write(TABLE)
        f.write(u"\n<!-- PROGRESS: 已判分叉行 %d / 分叉行总数 %d -->\n" % (done, nfork))
    print("已判 %d / 总数 %d ; 新案 %d 伪分叉 %d 补强 %d ; CSV %d 行 ; MD 已写"
          % (done, nfork, ncase["newcase"], ncase["nocase"], ncase["already"],
             len(rows) + 1))


if __name__ == "__main__":
    main()
