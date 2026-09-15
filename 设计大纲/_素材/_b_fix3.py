# -*- coding: utf-8 -*-
import re
P="大报告_历代控制包.md"
t=open(P,encoding="utf-8").read()

ADD = [
 ("钉死入口的上位约束逐代换人：REQUIRED 清单、V6.1 的 token 断言、V8.1 的一至一百字符机器断言（validate_control.py:255）、宪章期的槽位清单与路由链。",
  "钉死入口的上位约束逐代换人：REQUIRED 清单、V6.1 的 token 断言、V8.1 的一至一百字符机器断言（validate_control.py:255）、宪章期的槽位清单与路由链。
序列收束：00_START_HERE → AUTONOMOUS_ENTRY＋START_PROMPT → ＋README_INSTALLATION → 00_AGENT_START 双套 → 00_READ_FIRST＋START_PROMPT（V3 定标）→ V4 断档 → V5 恢复 → V6 至 V8.1 沿用 → V1 去提示词 → 救援回归双件 → 包规范只规定 READ_FIRST → RQS 00_README。失效点三处：C237｜a78f5430 删体、V3 定标改名、宪章期改由槽位清单裁定。"),
 ("架构权威在宪章期移出包体，V8.1 的冻结约束件被降级为历史参照（ACTIVITY_STATE.md L23，P-G15 §五）。",
  "架构权威在宪章期移出包体，V8.1 的冻结约束件被降级为历史参照（ACTIVITY_STATE.md L23，P-G15 §五）。
序列收束：根 18 份规格 → docs/＋contracts → contracts 全 schema 化 → 顶层 9 份 → 15 份＋schemas 独立 → 19 份＋fixtures → 99 文件 24 规格 validators 18 → 56 文件 16 规格＋baseline 内嵌 → 根 14＋tasks 57 的 JSON 图。失效点：schema 目录在 V8.1 取消、完整性担保在宪章期改由 git 与登记面承担。"),
 ("失效点：绝对值口径死于 V4，WAITING_WINDOWS 死于 V7 而在 V8.1 留痕以枚举外 FATDUCK_PENDING 复活（P-G10 缺口 12），包内数值死于 V8.1。",
  "失效点：绝对值口径死于 V4，WAITING_WINDOWS 死于 V7 而在 V8.1 留痕以枚举外 FATDUCK_PENDING 复活（P-G10 缺口 12），包内数值死于 V8.1。
序列收束：CPU≥150% 绝对值 → 0.80×min 相对带宽 → 同式加快速失败 → p50/mean 分位数 → 阈值入验收合同 → 均值 80% 加 10s<50% → 包内零数值 → 宪章 §10.5/§18.2 与代码 0.85/0.60 现行。真机口径的载体从包内规格换到仓库代码与登记件。"),
 ("ALPHA 字样可只在 zip 名而不在包内（P-G07 §一 1.2）。",
  "ALPHA 字样可只在 zip 名而不在包内（P-G07 §一 1.2）。
序列收束：无版本串 → 日期加特性长串 → 两行 → 语义化 1.3.0 → alpha 正则入校验器（V5）→ MANIFEST target_version（V6/V6.1）→ 包根 VERSION 与 product_version 同值（V7）→ VERSION 语义改包版本（V8.1）→ 包内无版本件（宪章期）。审核包命名同步演变为每 Gate 一包、任务胶囊、单包带 UTC 时间戳、宪章期 <hash> 式与救援期 sha256 前十二位（P-G05 §四、P-G06 §四、P-G07 §四、P-G08 §四、P-G10 §四、P-G13 §4.5、P-G14 §四）。"),
 ("现行口径：REVIEW_PENDING 被 tools/quality/validate_task_ledger.py 判非法、仅在 AGENTS.md 留历史映射；PASSED 亦不在其七值集内（口径注，P-G13 §四 4.3）。",
  "现行口径：REVIEW_PENDING 被 tools/quality/validate_task_ledger.py 判非法、仅在 AGENTS.md 留历史映射；PASSED 亦不在其七值集内（口径注，P-G13 §四 4.3）。
序列收束：三值 → 四值 → 五值 → 六值 → 四态加结论词 → 五值 → 七值 → 六值 → 六值（REVIEW_PENDING 判死）→ 三套并存 → 删列外置 → 列回归全 NOT_STARTED → PASSED → 无台账 → fix_state。载体随代次在包内清单、包外 JSON、仓库工具与登记件之间四次迁移。"),
 ("main-only 一条自 V3 起历代延续（文件名从 06 号演进到 V8.1 的 MAIN_ONLY_GIT_PROTOCOL，00_OVERVIEW §2.6）。",
  "main-only 一条自 V3 起历代延续（文件名从 06 号演进到 V8.1 的 MAIN_ONLY_GIT_PROTOCOL，00_OVERVIEW §2.6）。
序列收束：十步提交加每 Gate 一包（≤25MiB/5MiB）→ 仅最终一次加胶囊 → 胶囊索引 → 单一 audit zip 加四类提交相等 → 出口校验器十九类 → verdict 四值加自斥容量门 → 三件套对账加五步提交门 → 宪章六步冻结 → 终审计自产物加基线只减不增 → 常驻修复账本。失效点：胶囊死于 V6、包内顺序条款自此让位于宪章 §14.5。"),
 ("失效点：安装器派发随 C237｜a78f5430 删副本而终；队列派发死于 42/140 解除武装与 R-18 停用门框架（P-G13 缺口 17）；zip 密封让位于工作区入库——RQS 一代与\"zip 不入库成常态\"方向相反。",
  "失效点：安装器派发随 C237｜a78f5430 删副本而终；队列派发死于 42/140 解除武装与 R-18 停用门框架（P-G13 缺口 17）；zip 密封让位于工作区入库——RQS 一代与\"zip 不入库成常态\"方向相反。
序列收束：单代理总提示词 → 一句话提示词 → 安装器 → 启动动作定型（校验非零即停）→ 连续执行加异步回执 → 执行态外置加哈希绑定 → 队列加租约派发 → cprun 空白靠前台手册 → 模板与规范件定八字段派发 → 明文不依赖队列的前台交接 → 负责人直令加自建 git 工作区。"),
]
CUT = [
 ("；V8.1 因表值缺失改自算，三十六号对二千一百五十三份留痕与二千零四十五条提交，齐备十七、错位二、有账无据十七（P-G12 §七）",""),
 ("；同型事实在宪章期是 BASE-001 标 PASSED 而被引证据只存在于框架证据域（P-G13 缺口 9、P-G15 事实 3）",""),
 ("同一份清单在两形态给出两组数是登记面的常态：容器侧一百二十二比九比一、v1.3 复原件侧一百一十三比六比十三，P-G01 片尾二按口径冲突两记。",""),
 ("、G99 归并入 G13 的判定依据（coverage.md B 节）",""),
 ("、CP0 与 v3_cp0 同源却分两身份（P-G05 缺口 11）",""),
 ("它同时是代三同号噪声问题的放大版",""),
]
log=[]
for a,b in ADD:
    n=t.count(a)
    if n!=1: log.append("ADDmiss "+str(n)+" "+a[:20]); continue
    t=t.replace(a,b); log.append("add ok "+a[:16])
for a,b in CUT:
    n=t.count(a)
    if n!=1: log.append("CUTmiss "+str(n)+" "+a[:20]); continue
    t=t.replace(a,b); log.append("cut ok "+a[:16])
open(P,"w",encoding="utf-8").write(t)
han=len([c for c in t if 0x4E00<=ord(c)<=0x9FFF])
hb=len([c for c in t if (0x4E00<=ord(c)<=0x9FFF) or (0x3000<=ord(c)<=0x303F) or (0xFF00<=ord(c)<=0xFFEF) or ord(c) in (0x2014,0x2018,0x2019,0x201C,0x201D,0x2026)])
print("
".join(log))
print("TOTAL han",han,"han+punct",hb,"fences",t.count(chr(96)*3),"backticks",t.count(chr(96)))
PYEOF_END=None
