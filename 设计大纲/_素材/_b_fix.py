# -*- coding: utf-8 -*-
import re
P = "大报告_历代控制包.md"
t = open(P, encoding="utf-8").read()
R = [
 ["（整号 87.7%、限窗 86.0%，；救援包无号可核", "（整号 87.7%、限窗 86.0%）；救援包无号可核"],
 ["留痕：对齐包 57.9% 是正则低估",
  "废止已转为登记制：ACTIVITY_STATE §2 九行状态表（ACTIVE 1、ARCHIVED_SUPERSEDED 4、ARCHIVED_DISARMED 1、REFERENCE_TEMPLATE 3）加 §7 只读自检机器门，取代了文件与 SHA 链，负责人 09-13 指令经 C1850｜5a250999 写入（STAGE-07 §3、P-G14 §五）。留痕：对齐包 57.9% 是正则低估"],
 ["七节规定控制包七槽位（00_READ_FIRST.md 八项、TASK_LEDGER.csv 最低五字段加推荐四字段、OWNER_BINDINGS.yaml、tasks/ 每任务七项、schemas 与 validators（注明推荐不强制）、审核包树与禁装七类、四枚举最终结论并禁自宣发布）。",
  "七节定出七槽位（L16-25）：入口件八字段（L29-40）、台账五最低字段加四推荐（L42-61）、owner 件、每任务七项、schemas 与 validators（推荐不强制）、审核包树与禁装七类、四枚举结论并禁自宣发布。"],
 ["凡出现「首次／最后提交」，须注意 pack_inventory.csv 对早期身份存在方向倒置的已知缺陷（首列实为删除提交），P-G01 缺口 8、P-G02 缺口 2、P-G03 缺口 2 与 P-G13 缺口 1 已订正，以 provenance 与 git 实测为准。",
  "凡出现「首次／最后提交」，须注意 pack_inventory.csv 对早期身份方向倒置的已知缺陷（首列实为删除提交；P-G01 缺口 8、P-G13 缺口 1 已订正），以 provenance 与 git 实测为准。"],
 ["v1.1 至 v1.3 换 AUTONOMOUS_ENTRY.md 加 START_PROMPT.txt 双件，提示词六十一至六十六字符，且被 v1.2 的 tools/validate_pack.py 列为 REQUIRED 首两项（P-G01 §四/§六）",
  "v1.1 至 v1.3 换 AUTONOMOUS_ENTRY.md 加 START_PROMPT.txt 双件，提示词 61 至 66 字符，并被 v1.2 的 tools/validate_pack.py 列为 REQUIRED 首两项（P-G01 §四/§六）"],
 ["V8.1 包体回归定性条款，八十五与六十在包内全形态零命中，数值实体在宪章 §10.5 与 §18.2 及 run_monitored.py:453/455、resource_gate.h:108/111，其引入提交均晚于本包（P-G12 §四 4.6）",
  "V8.1 包体回归定性条款，85% 与 60% 在包内全形态零命中，数值实体在宪章 §10.5/§18.2 与 run_monitored.py:453/455、resource_gate.h:108/111，其引入提交均晚于本包（P-G12 §四 4.6）"],
 ["v1.0 无版本串仅 PACKAGE_SHA256.txt；v1.1 记\"日期加特性\"长串；v1.2 两行；v1.3 收敛为 1.3.0（六十六字节降到六字节）；v2.0 起产品目标版本进 MANIFEST 面",
  "v1.0 无版本串仅 PACKAGE_SHA256.txt；v1.1 记日期加特性长串；v1.2 两行；v1.3 收敛为 1.3.0（66 字节降到 6 字节）；v2.0 起产品目标版本进 MANIFEST 面"],
 ["命名规则另有三处可核对事实：日戳用于分辨同期并存的包（20260828 的 V4、V5、REAUDIT_V4 三包）；同一物可四名并存（V7 抬头、清单、zip 名、解包目录，P-G10 缺口 7）；ALPHA 字样可在 zip 名而不在包内（P-G07 §一 1.2），三者语义不可互代。",
  "另有三处命名事实：日戳用于分辨同期并存的包（20260828 的 V4、V5、REAUDIT_V4）；同一物可四名并存（V7 抬头、清单、zip 名、解包目录，P-G10 缺口 7）；ALPHA 字样可只在 zip 名而不在包内（P-G07 §一 1.2）。"],
 ["、G3 规模三种表述（P-G10 缺口 4）", ""],
 ["、引用不存在的规则编号（P-G05 缺口 7）", ""],
 ["按身份去重后七代相加恰为四十九、按实例相加为七十四（交叠两身份的四实例只计一次）。",
  "按身份去重后七代相加恰为 49、按实例相加为 74（交叠两身份的 4 个实例只计一次）。"],
]
for a,b in R:
    n = t.count(a)
    if n != 1:
        print("SKIP", n, a[:26]); continue
    ha = len([c for c in a if 0x4E00 <= ord(c) <= 0x9FFF])
    hb = len([c for c in b if 0x4E00 <= ord(c) <= 0x9FFF])
    t = t.replace(a,b)
    print("ok", ha-hb, a[:22])
open(P,"w",encoding="utf-8").write(t)
han = len([c for c in t if 0x4E00 <= ord(c) <= 0x9FFF])
print("TOTAL han", han)
print("fences", t.count(chr(96)*3), "backticks", t.count(chr(96)))
