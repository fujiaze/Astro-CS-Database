# STAGE-05 REAUDIT_V3 收口、V4 更替与 V5 单一-CLI 发行控制包

## 时间窗与提交数
- seq 991–1260（共 270 条；merge 0，L 级 5 条）。日期 2026-08-28 至 2026-08-30（990→991 恰跨午夜，H-S19）。
- 对应分片：H-S19（尾）、H-S20、H-S21、H-S22、H-S23。全段单亲线性链、时间戳沿 seq 递增（跨午夜两处）。

## 该阶段要解决的问题（引自提交消息，标注自述）
1. REAUDIT_V3 审计线收口与打回：G3 科学冻结（seq 1004–1011 SCI-001..005 逐条 PASS + ORA-001）→ G4 机器检查器全绿（1012）→ G5 构建/测试枚举（467 通过/32 失败全部有外部依赖或平台预期证据，seq 1026/1034）→ G6 Fatduck 远程复验（1028）→ G7 真实数据矩阵（1030/1031）→ CON 系列收尾 → seq 1036 审核人"控制包打回重做、执行侧全部停止"。
2. 控制包世代更替：seq 1037 自造 V4 包（98 原子任务账本 + C0..C9 检查点链 + 强校验器）→ seq 1039 按用户指示整体删除自造包、归位用户上传 V4_CPU_ADAPTIVE 原包（feat(v4)→chore(v5)）→ seq 1042 RELEASE_V5 起点冻结（BASELINE.json 三 SHA 一致 + RISK_REGISTER 6 风险）——V5 纪元开启。
3. V5 的任务图："合同→机器门→胶囊"三件套逐域冻结：DOC/VER/TRACE/GOV → SCI-001..007 → ALG-001..007 → REV-001 → ARCH-001..005 → API-001..005 → CLI-001..003 → ABI-001..003 → BENCH-001..005 → ISA-001..005 → PAR/SYN/DOCCHK/LNX → REV-002 → WIN-001..009（H-S20/S21/S22/S23）。
4. 单一 astrocs CLI 从合同到发行形态：seq 1088 | 07bc18b9 cli/main.cpp 生产 C++ 首现 → seq 1090 统一 parser+JSONL 协议+cancel → seq 1094 include/astrocs 公共 ABI + lib/backend_host → p1/p2/p3 进程内 session 接入（seq 1113/1115/1125）→ run --phases 真编排 + "恰一用户 exe" install tree 机器门（seq 1128/1130）。

## 具体工作内容（按模块）
- 科学算法：合同补全而非改语义（SCI-001..007：Frames/专属问题/Primary literature/Acceptance 逐份，seq 1047–1053）；ALG V5 重验删 GPU 路径改 CPU-only worker pool、固定序归约禁重结合（seq 1054–1064）；Phase3 重采样算法新建 PHASE3_RESAMPLE.md（seq 1064）。
- 管线与 CLI：见上第 4 点；ISA 决策审计闭环（seq 1132–1139：AVX/AVX512F NOT_SHIPPED、AVX2+FMA SHIP、位操作 NOT_APPLICABLE，四连裁决附 MEASUREMENTS.csv）；CPU 后端六查信任边界（seq 1072）；全局 ThreadBudget（seq 1074）；Phase3 四单元单向流（seq 1076）。
- 构建与 CI：VER-001 alpha 单一版本源 VERSION=0.9.0-alpha.1 + gen_version + schema + 一致性检查（seq 1044）；GOV-001 AGENTS 治理机器门 10/10 要素 + mutation 试金石（seq 1043 | 15720f5e）；TRACE-001 六层追溯 schema + 检查器（seq 1045）；BENCH-001..005 硬件画像→harness 顺序合同→候选生成→cpu_profile 生命周期→benchmark cpu/doctor CLI（seq 1103–1112，测试基线 131→154）；backend 加载安全化五查（seq 1096）；baseline 全生产 kernel 12 op + Python 独立 Oracle（seq 1098）。
- 控制包与治理：台账状态机违规治理（seq 1056 无台账行→1057 补正并披露 sed 错配；1058/1090 NOT_STARTED→PASS 单步翻转与自述不符，H-S20）；C1 检查点 14 胶囊聚合 zip（seq 1066/1067）；登记格式突变——seq 1153 起登记提交同携带二进制胶囊 zip 且 sha 写法 12→7 位（H-S22）；REVIEW_PENDING 台账字面量（seq 1196 REV-002，自证→待外部审阅，片内无回写，环节悬空）；V5 审核包/审计包 88 PASS 表格自述（seq 1260 前）。
- 文档与报告：docs/api 首现（seq 1078）；37 个 review 胶囊集中入库（seq 1101/1102）+ FATDUCK_ACCESS.md 纳管；FATDUCK 探测报告（seq 1198：克隆同 main SHA、testdata inventory hash 一致）。
- 测试：跨节点 MSVC 移植链 seq 1200–1210 十一连（全部"包与证据面=0"，本片唯一纯代码链；H-S22），其构建 PASS（1213）与登记（1214）落在下一片；WIN-002..005"修复→验证→胶囊注册"三连 → WIN-006/007 BLOCKED（缺生产 HIPS 产线/T4 数据）→ WIN-009 双平台 alpha 发布打包线（0.9.0-alpha.1，seq 1231 起）；PAR-002 全片唯一 BLOCKED（cfitsio 库级并发读 SIGSEGV exit 139 实证，seq 1152）与 PAR-003 PASS 收尾（seq 1154）。

## 交付与验证留痕
- BASELINE.json（三 SHA 一致锚恰为 seq 1041 的 SHA）；C1_SCI_ALG_REVIEW.zip + C1_MANIFEST.json + CHECKPOINTS.csv C1 行；MEASUREMENTS.csv 实测量表与 ISA_VARIANTS.md（seq 1100/1132–1139）；467/32 测试枚举 + Windows 233/0 交叉（自述，seq 1026/1034）；tar.zst 发布产物与 AUDIT_PACKAGE zip（seq 1236/1260——是否入库不可判，不在变更面）。

## 结构与规范变化
- 目录：工程控制/RELEASE_V5、artifacts/prerelease_v5（seq 1042）；cli/、include/astrocs、lib/backend_host、tests/backend、tests/api、tests/arch、docs/api、schemas/version.schema.json、tools/gen_version.py 等 V5 新面集中涌现（seq 1044–1098）。
- 编号族：REAUDIT G2–G7 → V4 C0-00x → V5 (TASK-ID) 括号族——全仓最强治理边界（H-S19 信号④/⑤）。
- 前会话遗留工作树归位：seq 1040 | 1c580eb9 把 QA-V19R7-A2-* 与 self_review/round0-6 按 AGENTS 目录规范移入 reports/（BASE-001 登记）。

## 与下一阶段的衔接
- seq 1261 | 4b1b948e CONTROL_V6 控制包 46 文件导入 + evidence/refactor/TASK_LEDGER.csv 建立（88 任务 G0 起点）直接开启 STAGE-06；"V6 tasks=88 与 V5 88 PASS 是否同一任务集未证实"（H-S23）。
- 带入项：REVIEW_PENDING 无回写；WIN-002 收口跨片；T4 数据到位方式无记录；FATDUCK 侧后续无证据。

## 证据缺口
- 全部 rc/通过数/耗时/RSS/sanitizer 计数为消息与报告自述，实跑产物 gitignore 未入库，无 CI 日志可交叉（H-S22 通则）；MON-004 等 12 位短 sha 胶囊 zip 从未入库（现为工作区未跟踪文件，seq 1147–1151）；1150 条目"新增/修改"判词与逐文件操作位存在一片不一致（H-S22 重叠核对判 1 不一致，H-S21 称 docs/architecture 新增、实核为修改 M +19/-0）；1182 SHA 基线不同一、1184/1190 计数口径差；zlib 链接方案三连改第三改在 seq 1211 片外。
