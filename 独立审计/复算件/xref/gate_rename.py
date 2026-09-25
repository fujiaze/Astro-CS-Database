import sys
import re
from pathlib import Path

sys.stdout.reconfigure(encoding='utf-8')
BASE = Path(r'产出/')
GATE = BASE / '独立审计包' / '05_门禁'
OLD = GATE / '旧门禁处置.md'
NEW = GATE / '门禁现状与旧门禁处置.md'
KEEP = BASE / '复算' / 'xref' / '旧门禁处置.改名前副本.md'

txt = OLD.read_text(encoding='utf-8')
KEEP.write_text(txt, encoding='utf-8')

APPEND = """
---

## 8 现状读数与逐条结论的复核态

本件与《门禁清单.md》共用同一批现状读数。读数本身是机械普查，**逐条结论的判定另见 §1–§7 与其引用的复核件**；本节只登记"哪些数可直接引用、哪些还在第②层"。

### 8.1 注册表现值（基线 `c8f64e9a`）

| 量 | 值 | 复算来源 |
|---|---|---|
| 顶层注册项 | 150 | `eng/ci/checks.json` 的 `checks[]` 长度 |
| 执行单元（顶层单步项＋全部 step） | **370**（其中 step 291） | 同上 |
| profile 分布（单元可多档） | linux-main 345／fast 194／windows-main 168／prerelease 64／linux-deep 10／integration 9／fatduck 3 | 同上 |
| 按 `platform` 字段分布 | any 223／linux 143／windows 4 | 同上，**与上一行是两列，不得互相顶替** |
| `waivable` 分布（按单元） | false 360／true 8／字段缺失 2 | 同上 |
| 声明 `inputs` 的单元 | 10／370（2.7%） | 普查 |
| 声明 `optional_inputs` 的单元 | 1 | 普查 |
| 登记 `outputs` 为空的单元 | 157／370 | 普查 |
| `heavy` ⇒ `requires_monitor` | 13／13（无违例） | 普查 |
| 命令引用的脚本目标缺失 | 0 | 普查 |
| 单元 ID 重复 | 0 | 普查 |

复算脚本：`独立审计/工具脚本/AUD-501/census_registry.py`、`census2.py`、`census3.py`、`census5.py`。
**注册表在上游一旦漂移，本表全部读数作废重算**，§1–§7 里引用这些数的判据行同批复算。

### 8.2 逐条结论（AUD-501 的 28 条）复核态

| 范围 | 复核态 | 第②层件 |
|---|---|---|
| A-03、A-04、A-05、A-08、A-10 | **已判**：两条主链判"确认"，但含三处必须订正的读数（见 8.3） | `独立审计/证据/复核-门禁入口.md` |
| A-01、A-02、A-06、A-07、A-09、A-11…A-16 | 在途（11 条） | `独立审计/证据/复核-V501-A.md` |
| A-17…A-28 | 在途（12 条） | `独立审计/证据/复核-V501-B.md` |

**在这两份复核件落盘之前，本件 §1–§7 按"单层未复核"对待**：可以照做其中已复核的部分，不得据未复核条目派工，也不得把它的读数写进对外结论。本包的合入规则（总目录 §3）要求只有"确认／降级后仍成立"进实施任务书。

### 8.3 已复核两组的订正（引用现状数时以订正后为准）

第②层《复核-门禁入口》判了 W1、W2 两组，**两组都判"确认"，但各带一表订正**。
该件判定句写"三处／两处须订正"，而它自己的"与成稿差异"表**各列 6 行**——按原文表计，此处逐行照录（判定句少计的那几行恰恰含最要紧的"成稿未记"项）。

**W1（对 A-08：两条执行入口判定必然分叉，其中一条可零执行报绿）**

| # | 成稿 | 第②层 |
|---|---|---|
| 1 | "platform: linux 的单元共 345 个（占 93%）全部落在此面" | **数值口径错**：345 是 `profiles` 含 `linux-main` 的数；`platform: linux` 实为 **143（38.6%）**，"全部落在此面"不成立 |
| 2 | "`--all --profile linux-main` 在 Windows 上同理全 skip 全 PASS" | **推翻该子句**：该档选中 345 单元，Windows 上 137 跳、**208 真可执行**（会真跑并可能真红） |
| 3 | 权威依据引 `CI_SPEC §2.4 第 3 条（空选择判红）` | **引证过宽**：§2.4 第 3 条（改动集非空而选中数为 0）`run_checks.py:1058-1059` **已实现**；真实缺陷是 `explicit`/`full` 档在规范里**没有零有效执行口径** |
| 4 | "run.py 对同两种情形显式判 FAIL，CI 面成立" | **补两条限制**：`run.py:1248-1251` FATDUCK_PENDING → `:1514` rc=0 且 `executed` 为空；`validate_registry.py:383-386` R12 强制 71/71 聚合项经 agent 入口派发 |
| 5 | "置信 CONFIRMED；实际 rc 值 需复测" | rc=0 **读码即定**（`:1966→:2056` 无环境分支），"需复测"可缩到"现跑一次留 JSON 证据"一档 |
| 6 | 成稿未记 | `run.py:92` 把平台跳过**塌成 `SKIPPED(waivable)` 同一标记**（两因不可分）；`:1257` 注释错引 `SPEC §8`；`CHK-ALGO-WIRING` 是活分叉样本；`01_CHECKS.md` 命令列 **58 行走 agent 入口 / 0 行走 CI 入口** |

**W2（对 A-03／A-04：那道"全门禁 fail-closed 普查"测的是谁）**

| # | 成稿 | 第②层 |
|---|---|---|
| 1 | "普查表里 **213** 个『通过』不构成判别力证据" | **数字错口径**：213 = 声明了 outputs 的单元数（适用面），不是"通过"数；归档实际 **226/230**，现算 **366/370** ⇒ **订正后主张更强**（被当作已核的门多 71%） |
| 2 | A 面／C 面充要条件 | **确认**，并补第三条：B 面 = `requires_monitor` 且有任一 `.json` 输出，注入物全局同一份 `BAD_EVIDENCE` 字面量 |
| 3 | "阈值 50 属'把历史观测量抄成阈值'同一形态" | **降级该子句**：脚本与归档件同批落地，当时观测量 230 ≠ 阈值 50 ⇒ 是**无来源魔数**，不是抄来的测量值；不判别力结论不变（裁到 51 仍绿） |
| 4 | 取证命令"subprocess 类调用零命中" | **独立复现并加严**：换 `Popen\|os.system\|check_output\|run(` 仍零命中，且 import 面只有 argparse/json/pathlib/shutil/sys/tempfile |
| 5 | A-05 的 150／370／13 | **确认（独立复算同数）**，并补：4 条"无适用面"清单与文档逐字相同 ⇒ 漂移只在总量、不在结构 |
| 6 | 成稿未记 | ①普查不做字段继承 ⇒ **42 个单元的普查对象 ≠ runner 强制对象**；②注册命令只有 `--json-out` ⇒ **门本身在 370 单元上现算**，"只回放归档件"只适用于文档与跟踪件；③普查自测 N1 的注入点是共享函数本身，**未触及任何一条豁免分支** |

**两条定级**：W1、W2 同判 P0，第②层建议**合并为一条"门禁给的红绿均不具证据资格"的重建前提**（已落在 `06_实施/tasks/ACSD-T00`、`ACSD-T01`）。

**污染面（凡引用下列读数者须改写口径）**：本节点既往的"全量扫描红灯分档"实为 **agent 入口 · fast 档 · 194/370 单元（52.4%）**，另有 176 个单元从未在该扫描里跑过；其中的"平台跳过"档在整轮汇总面上是 `verdict=PASS`＋`rc=0`，即"判不了"从未被表达成一次失败。`ACCEPTANCE_SPEC.md` 两处"通过标准 = `run_checks.py --check … exit 0`"与 `docs/ci/03_GATES.md §5` 的"绿"字来源未限定入口 ⇒ 三条都须与"入口唯一化"同批改。**本节所有现状读数按 `c8f64e9a` 取，上游注册表一漂移即整表作废重算。**
"""

new = txt.replace('# 旧门禁处置\n', '# 门禁现状与旧门禁处置\n', 1)
new = new.rstrip() + '\n' + APPEND
NEW.write_text(new, encoding='utf-8')
OLD.unlink()

PKG = BASE / '独立审计包'
fixed = []
for p in sorted(PKG.rglob('*.md')):
    if p.name == '门禁清单.md':
        pass
    t = p.read_text(encoding='utf-8')
    o = t
    t = re.sub(r'(?<!门禁现状与)《旧门禁处置(?=\.|》|》)', '《门禁现状与旧门禁处置', t)
    t = re.sub(r'(?<!门禁现状与)`旧门禁处置\.md`', '`门禁现状与旧门禁处置.md`', t)
    t = re.sub(r'(?<!门禁现状与)05_门禁/旧门禁处置\.md', '05_门禁/门禁现状与旧门禁处置.md', t)
    if t != o:
        p.write_text(t, encoding='utf-8')
        fixed.append(p.relative_to(PKG).as_posix())
print('改名完成；引用被改写的文件：', fixed)
print('新文件行数 %d（原 %d）' % (len(NEW.read_text(encoding="utf-8").splitlines()), len(txt.splitlines())))
for p in sorted(PKG.rglob('*.md')):
    for i, l in enumerate(p.read_text(encoding='utf-8').splitlines(), 1):
        if '旧门禁处置' in l and '门禁现状与旧门禁处置' not in l:
            print('残留引用: %s:%d %s' % (p.relative_to(PKG).as_posix(), i, l.strip()[:120]))
