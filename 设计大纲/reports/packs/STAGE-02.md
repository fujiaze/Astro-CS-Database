# STAGE-02 ACR 专项期（acr 支线包 + ACR_FOCUSED V1–V4）

对应组：G04。对应任务一历史阶段：STAGE-02（seq 272–550，08-02→08-10，reports/history/00_OVERVIEW.md §1 表行 2）。

## 1. 时间窗
- PACKNEW 锚点（structural_events.csv / pack_events.md）：acr 支线 spec 于 49ea5c1e（2026-08-02 侧），ACR_FOCUSED V1 seq 406（0030c3a5，08-05）、V2 seq 429（fe98df3f 窗）、V3 seq 438（38b85702，08-06）、V4 seq 460（38b85702→实为 a36c4825 窗，08-06）；五目录 marker 由 seq 550（198d69e0，08-10 merge）重复注册（pack_events.md 事实 3：PACKNEW 52 条中 10 条系该提交对 acr+ACR 五目录的重复登记，真实引入目录 26）。
- 收束：e1604f14（08-27 23:14:47）docs→archive 移档（84 条 R100）；b7b2dea7（09-02）以 134 条 R100（ACR 子集口径；容器全量 854，P-G11 事实 2）收入 legacy 容器。

## 2. 包清单（5 身份）
acr（2 实例：复原件仅 3 载荷文件取自 49ea5c1e^、容器件 50 文件系 12fb99f3 终态——两形态严重不一致，P-G04 事实 4）；ACR_FOCUSED_CONTROL_PACKAGE{,_V2,_V3,_V4}（各 3 实例：原始复原件/归档复原件/容器解包件，载荷逐字节一致，diff 仅 _PROVENANCE.json，P-G04 事实 2）。全部无 zip 实物（_control_packs 与 git 全史 0 命中），而包内自述固定包名 AstroCS_ACR_Control_Package.zip——发行口径与入库形态从未一致（P-G04 事实 1）。

## 3. 共同设计意图（转述）
- V1 00_READ_FIRST §4/§5、package_manifest.json：supersedes 仅覆盖"all earlier AstroCS ACR control packages and plans 20-26"（ACR 自家谱系）；spec.md §1.3 把 HISS 列为禁改算法对象。
- V1→V4 演进主线（P-G04 §五）：从"聚焦控制包"到 08-06 "freeze architecture and weighted integration"（38b85702 提交消息）；V4 冻结架构与加权集成合同。
- 留痕外置规则：7c67328a（08-04 "remove in-repo evidence (external single-head generation only)"）把 工程控制/evidence/acr/ 70 文件删净——包明文要求证据在仓外单头生成（P-G04 事实 5）。

## 4. 任务规模与状态字面量
五身份均无 TASK_LEDGER.csv/任务号族（pack_lineage_table.md 行 14–18 台账行 0）；编号只存在于 V1 09 号 §1"建议提交序列"（V1 5 条、V4 5 条）。状态口径不可与后续代际对接——pack_commit_link.csv 无本代行属"真无台账"（对照 G09/G12 的采集盲区成因不同）。

## 5. 门禁与验收
- 无机器门脚本入包；验收由 audits/*.md 引用的 HEAD 快照承担——V2/V3/V4 audits 引用 5 个 HEAD（f8cba99e/1c2ed0f5/e107061c/c82013ee/610d7b6a）全部真实存在（P-G04 事实 3）。
- 被审 Review zip 三件（ACRFocused_20260805/V2_20260806/V3_20260806）均未入库，SHA 无法复算（P-G04 缺口）。

## 6. 与上一代差异
- 与 STAGE-01 末对象 _agent_package：无直接继承/废止（包内零互引；supersedes 域仅 ACR 自家），唯一可核是 acr 支线 base commit 8f50519（07-31）落在 G03 包活跃当天（P-G04 衔接段）。
- 内部四代代际链 V1→V4：规格文件与 audits 递增（P-G04 §六按 V1→V4 逐项）。

## 7. 执行留痕概况
- V1 建议提交序列 5/5 精确命中 git（0030c3a5/3a405960/fe98df3f/380aae32/78101ae0，均 08-05）；V4 5 条命中 2 精确+2 近似+1 未命中（P-G04 事实 3）。
- 三方核对呈"有据无账"：留痕按包规则外置，仓内 evidence/ 无对应目录（P-G04 事实 5、七节）。

## 8. 与下一代衔接
V4 于 e1604f14（08-27 23:14:47）归档，与 G05 首事件 3703650d（23:08:54）相隔 3–6 分钟（时间相邻、锚点互不引用）；G05 三个锚点（b38b446e/83471979/535e7387）与 ACR V4 基线 047357818c 互不引用（P-G05 衔接段，P-G04 同向核对）。"ACR"字样在 V2/V3/V4 正文出现 13/19/13 次但全部指代码模块，ACR_FOCUSED 计数 0——两代无引用关系（P-G05 事实）。

## 9. 缺口（P-G04 共 8 条）
最关键：acr 身份两形态互斥（3 vs 50 文件）且 pack_inventory 对其记 first(08-04)>last(08-02) 日期倒序；V1 废止的"旧 23–26 号计划"全史无踪迹（仅存 20/21/22）；digest 的 SHA256SUMS JSON 解析口径与 CHECKLIST.md 大小写漂移（digest_verify 观察 3：acr_wt missing=1 实体即该漂移）。
