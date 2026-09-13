# M7 · I_DOC_HYGIENE（文档卫生）· P2

> **档位声明**：本文件内全部条目的 **类别与优先级由本行标题承载**（协议 §3 的「一类别×优先级一档」）；条目正文只在**偏离本档级别**时显式标注改档及理由（如 M7-A-101 记 P0→P1、M7-A-201 记 P1→P2 并撤核心结论、M7-A-001 记 P1→P0、M7-I-202 记 P1→P2 且改类 A_SCI_DEF→I_DOC_HYGIENE）。逐条四态判定与编号映射见 \`问题扫描/_merge/M7.md\` §2；每条均含 位置/权威依据/证据或证据出处/问题说明/影响/建议处置/置信度/related/四态判定 九项。

口径同其它 M7 合档：锚为 `path::符号`，行号注「复核时 N」；四态判定见 `_merge/M7.md`。

## M7-I-201 SCI-SCOPE「默认 FP64 科学计算；FP32 仅显式等价路径」被 SCI-CAL §9 在文档层直接反驳（L19-028）
- 位置 `docs/science/SCIENCE_SCOPE.md::数值精度`（:53）对照 `docs/science/CALIBRATION.md::§9`（:82）
- 问题说明 宪章 §5.3 允许校准路径 float32 ⇒ 不是宪章违规，而是 SCI 之间的入口级矛盾（SCOPE 自列 CAL/PHOT/PSF 为其下游）；其举例 SparseEqualsDense 1e-12 是 UPM 专属门，不构成 CAL 的等价路径。
- 处置 SCOPE 改分层精度表（校准 FP32 合法、Drizzle/WCS 各按本合约）。
- **全层口径订正回执（父层 3 号移交）**：本条**不引用**旧表述「三处 FP32 累加」——该说法已被 M3 判一半不成立（`libasp` 的 APArray 本就 float 存储，读回是加宽非截断；真实截断在 `libaio/src/aio_api.cpp:372-380`）。全层口径改采「**五处 FP64 声明与实际累加域不符 + 两处真实 FP32→FP64 截断**」。
- related L04-001（M2a 定稿的累加域事实）、L11-011、M3 域、`_merge/M7.md` §订正表；判定 **仍成立**；置信度 高

## M7-I-202 hips_frame 字面值在合同与写侧措辞不一致（L21-009 **按前台裁决降级定稿**）
- 类别: I_DOC_HYGIENE · 优先级: **P2**（原 L21-009 定 A_SCI_DEF/P1 并称「双向锁死→产品互操作被锁」）
- 前台裁决原文（必须照此措辞）：L21 称「hips_frame 在 IO-002 必 equatorial 与 DATA-002/Phase3 校验器必 icrs 之间双向锁死 → 产品互操作被锁」**属过度表述**；实测**校验器两个值都收**（`lib/phase3_session/hips_properties.cpp:126`），且 `DATA_SEMANTICS.md:816`（值域 ∈{equatorial,icrs}→rc=1）与 :819（跨帧混用→rc=1 "hips_frame mismatch"，标 B2-A8）说明混合产品早已被显式拒绝，**不是静默锁死**；定性改为「**writer 恒写 equatorial（`HIPS_WRITER.md:174`）与两处合同/交换语义的措辞不一致**」，属 `C_DOC_CODE_GAP`/`I_DOC_HYGIENE` 的 P1/P2，**不得写成「产品互操作被锁死」**；L21 §6 自列的待复核项「hips_frame 运行时阻断」由该证据**判定为不存在**。
- 本代理按同法自查补充的两条事实（前台要求 M7 找实际校验点后定强弱）：①`runtime/io/hips_core.c` 的 properties 解析（复核时 :236/:240）文案是「缺 hips_frame」/「`hips_frame='%s' 非 equatorial (未知 frame 拒绝)`」⇒ **该处只收 equatorial、icrs 被拒**，与 Phase3 校验器（两值都收）**在值域上不一致**；②故真正的残留缺陷是「**三处运行时/合同面对 hips_frame 的合法值集不同**：Phase3 校验器 {equatorial,icrs}、runtime/io 读路径 {equatorial}、DATA-002 交换层 {equatorial,icrs}＋跨帧混用拒绝」，而非「互操作被锁死」。
- 处置 ①按前台定性把 IO_002/HIPS_WRITER/DATA-002 三处措辞统一为「写侧恒 equatorial；读侧允许两值但须同集」或「一律 equatorial」，择一由 owner 定；②把 `runtime/io/hips_core.c::hips_props 解析` 与 `lib/phase3_session/hips_properties.cpp::parse` 的值域差异登记为偏差（DISP-HIPS-*），因它是**行为差异**而非措辞差异（强于前台 P2 定性的那一半，仍不单立 P1，留移交）。
- related L21-007（交换面最小平面集，同族）、M2b（HiPS/AIO 域）、`40_OWNER_DECISIONS.md::B-12`（外部标准原文核验：HiPS 1.0 hips_frame 枚举行需外网核）；判定 **降级为 P2（照前台裁决）**＋**改述**（锁死→三处值域/措辞不一致）；置信度 高

## M7-I-203 DATA_SEMANTICS §24.4 给全整数的 coverage 结果标「UNIT=ADU/tile 口径透传」（L21-011）
- 位置 `docs/contracts/DATA_SEMANTICS.md::§24.4`（复核时 :1592 命中 ADU/tile）
- 问题说明 被标对象是整型计数（coverage），透传一个能级单位 ⇒ BUNIT 语义失真，下游按 ADU 解读即错一量纲。
- 处置 整型计数不写 ADU 单位或改 `无量纲（tile 计数）`。
- related L04-010、M7-A-122、M7-A-119；判定 **仍成立**；置信度 高

## M7-I-204 同文档内「reason u8」两套互斥值域（rejection 0..3 vs sampler 0..5）（L21-012）
- 位置 `docs/contracts/DATA_SEMANTICS.md::§/reason 行`（复核时 :471/:992/:1246/:1266 四处）
- 问题说明 同名 u8 字段在两节分别声明 4 值与 6 值，且 4..5 无定义 ⇒ 同一字节值在两通道语义不同，属「无单一事实源」簇。
- 处置 单一 reason 表 + 值域并集 + 未用值显式保留。
- related L21-002、M7-A-120；判定 **仍成立**；置信度 高

## M7-I-205 符号 K 在唯一词典内三义并存；electron 许可域标注与四文档现状互斥（L21-014）
- 位置 `docs/GLOSSARY.md::K 行`（复核时 :21/:25）、`docs/science/CALIBRATION.md::K=t_light/t_dark`、`docs/science/NOISE_MODEL.md::K`、`docs/contracts/DATA_SEMANTICS.md::electron`
- 问题说明 同一符号在词典内即三义（暗场缩放因子 / 卷积核 / K 因子），与「恰一个含义」的自述冲突；electron 的许可域在四文档标注不一（附录 A 第 8 行原样保留）。
- 处置 符号改名分列（K_dark、Kernel、K_corr 已占用）；electron 域由 owner 裁一次并同文四写。
- related M7-A-120、M7-A-119、附录 A 口径矩阵（`_merge/M7.md`）；判定 **仍成立**；置信度 高