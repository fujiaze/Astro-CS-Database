# AUDIT_INDEX · 扩展审计总索引（ROOT-004 加派；第一波 10 维度轴 + 第二波 14 文件级轴）

- 统一纪律：只读、零修复、零 git 写；每轴产物=audit/[w2/]<AXIS>.md + run/PROJECT-GOVERNANCE-01/ROOT-004/logs/audit/[w2_]<AXIS>.log；发现全带「权威条款§节 + 当前树命令逐字证据 + 严重度 + 整改建议 + 文件域 + 验收门 + GAP/TASK_LIST/第一波/旧账本同源」。
- 基线政策：三 SHA 相等即开工（并行线高频提交，各轴记录开工/收工 SHA；涉 WIP 按 tracked/工作树双口径标注，锁点外的文件增量归 NA:moved-wip）。

## 第一波·维度轴（10/10 全交，共 107 条，P0×13）

| 轴 | 发现 | P0/P1/P2 | P0/主项判词（一句） | 报告 |
|---|---|---|---|---|
| PKG 打包/安装/版本 | 9 | 2/5/2 | CHK-PACKAGE 未注册且 10/10 unit sha256=null、verify 跳过 ⇒ 发布哈希/provenance 门空转；Win manifest 把 SKELETON 标 IMPLEMENTED | audit/PKG.md |
| ARCH 架构/源码根/ABI | 9 | 0/6/3 | 无 P0；P1：lib 平铺 31、SHARED 3/29、三 session 在图、注册表三轨、ABI 门语料空转（106 公共头仅 6 带 struct_size） | audit/ARCH.md |
| RT 调度/线程/资源 | 7 | 1/4/2 | 资源冻结门全树无红灯面（record-only + CHK-RESOURCE 未注册）⇒ exit-10 合同双失效（与订正表 M5a-G-003=OPEN 互证） | audit/RT.md |
| QA 测试矩阵 | 10 | 1/5/4 | API_CONTRACTS 382/382 裸铸 VERIFIED、356 行占位 TST-GEN-001 零宿主且消费门不校验 status ⇒ 合同台账自铸零守护 | audit/QA.md |
| GOV 治理/权威链 | 10 | 1/7/2 | FATDUCK 仍 tracked、.gitignore 被 tracked 豁免、DOCUMENT_INDEX 登记 ACTIVE_INFORMATIVE（打包面已由 DENY 闭合） | audit/GOV.md |
| GOV 治理/权威链 | 10 | 1/7/2 | HiPS 写出 remove+create 直写 0 rename/fsync ⇒ 中断留假完整产品、读侧不复核 DATASUM（M2b-C-01 同源未修毕） | audit/AIO.md |
| SCI1 Phase1 科学 | 11 | 1/6/4 | v6 star-psf 生产节点饱和位写死 peak>50000（非 SCI 冻结双条件、零权威定义）⇒ 14-bit 平台星漏判饱和直入测光零点【建议优先裁决】；另 SIP 7×7 vs 41²/81² 无裁决 | audit/SCI1.md |
| SCI2 Phase2/3 科学 | 8 | 1/4/3 | **CAR/AIT 的 CRVAL2 只进 FITS 头不进像素映射（θ₀ 恒等）**：二进制对拍实测 probe 在自身 CRPIX 解出 dec=0、astropy 解出 30.0 ⇒ registry 挂生产即产出整幅错位 \|CRVAL2\| 度产品且头部自证合法（现被三重挡在交付面外）；测试面零防线（dec0 恒 0 夹具+自证参照式） | audit/SCI2.md |
| CLI 命令树/合同 | 15 | 1/9/5 | **资源门禁退出码 10 在唯一用户命令面不可达**：新 command_tree.h 未登记 --strict-resource-gate 等三旗标（实测 unknown flag rc=2），RESOURCE(10) 只在旧 commands.cpp strict 分支返回 ⇒ §6.3 码 10+CHK-RESOURCE 失去可执行入口；另 --json 双义无单文档实现、output_dir 隐式 CWD 缺省 11 处、预检三级只落两级、§6.3 唯一源 exit_codes.h 不存在+第二份 11 数值表 | audit/CLI.md |
| CI 注册表/机器门 | 17 | 4/10/3 | 四 P0：update_audit_status 713/713 机械铸 VERIFIED（与 TOOL-P0①同一铸源双轴互证）；ISA 泄漏/产品可达/生产图三门 CI 只跑 --selftest 真判面零调用者；ACR-DORMANT 证据面绑死已消失二进制 if-exists 恒跳仍打 PASS；新执行器 run_checks.py 只认退出码、73 项声明 outputs 无人核验 | audit/CI.md |

## 第二波·文件级穷尽轴（已交 8/14，共 79 条，P0×4，P0×1）

- 计划表 _gen/coverage_plan.tsv：2398 文件 → 14 轴不相交划分；排除 问题扫描 1097（26 分片已逐条处理）/ reports 724（历史证据）/ 工程控制 54（隔壁治理线活动区）。收口标准：各轴覆盖清单与计划表双向差集为空。

| 轴 | 域 | 覆盖 | 发现 | 亮点 |
|---|---|---|---|---|
| CI2 | ci/** + .github/** | 64/64（含 3 UNTRACKED） | 9（P0×1） | **fatduck-admin.yml inputs.cmd 原样注入 self-hosted FATDUCK shell:cmd ⇒ 任意命令执行+证据链可篡改【建议插队】；UT-GAIA-ZLIB 指向消失目录三 profile 恒红；365 元测试仅一族被消费（真跑 14 failed）** |
| ROOT | 根+scripts+run+third_party+modules | 39/39（补交 README/memory/HANDOVER/VERSION 后） | 17（P1×8） | ci/run_checks.py 被当入口从未入库；版本串编译进 5 处；toolchain.ps1 全旧架构；windows 工具链锁零命中仍被 6 处引用；config/ 在盘未入库=权威打架转裁决 |
| LIB5 | lib/algorithms+全仓 CMakeLists | 467/467 | 7（P1×3） | MODULE_MAP 18/23 target 不在图；13/16 module.yaml entrypoint:MISSING；hips_writer 安装单元反方向断链；干净面：install 四方 unit 一致 |
| DOC2 | docs/plugins 23 篇+INDEX | 24/24 | 6（P1×1） | 21 篇 §3 schema 引用全悬空（真合同链 16 份 WIP 无文档引用）；8 节模板 23/23 符合 |
| TOOL | tools/** 173/173（OK78/FINDING85/NA10） | 100% | 16（P0×3） | 铸造三源实锤：update_audit_status.py 机械铸 VERIFIED+hash-carry 写死空集（V13-N-03 的铸源本体）、v19r3_audit F03-F11 恒 True、check_traceability.py 实测 broken=26 仍无条件 rc=0 且 TRACEABILITY 门以其为保留依据；**新增治理红旗：tools/astro_toolkit+README 内置子 Agent git_add/commit/push 通道并自述「绕沙箱确认」——直接违 AGENTS §5/ESPEC §6 零 git 写禁令，建议负责人专项处置**；另 版本族门与 §12 冲突、空转恒绿族 6 门、49 契约夹具零消费者 | docs 其余全部 223/223 穷尽（OK96/FINDING96/NA31） | 100% | 11（P1×8） | 29 个 CHK-* 里 26 个不在注册表同一 ID 空间；DOCUMENT_INDEX 六条指已删文件+未登记 43；27 文件 45 处拿已删宪章当上游权威；八份 docs 根文档以 VERSION 0.11.0 为「产品版本唯一事实源」违 §12 Alpha 零版本；STANDARDS_REGISTRY 自称冻结四投影对抗 §5.3 八投影且自家 checker=FAIL |
| LIB2 | lib/phase2*+drizzle+hips_p2+healpix_db | 239/239（132 moved-wip 已映射新路径） | 5（P1×2） | stage2 仍暴露 support×SNR² 冒充 ivar 面且自设门零接线；README 端口锚出链旧 docs/contracts 绕 §4 唯一事实源；**并发移交：cli/CMakeLists.txt:151 与 healpix_db/README 仍指已迁旧路径，合入前不同批更新则 main 红** |

## 跨轴共性（给调度员的合并处置建议）
1. 「01_CHECKS 声明的 P0 门未注册进 ci/checks.json」同形四处（CHK-PACKAGE/RESOURCE/ORACLE/INVARIANT）⇒ CI-001 已把注册表重写为 35 聚合项（进行中），务必加「声明⇒注册」对账门。
2. 状态铸造家族（QA-1、M6b-G-001、V13-N-03、PKG-2、W2-CI2-5/6 事实豁免）⇒ 统一「VERIFIED 必须可复算锚」原则，CI-001+DATA-001+GOV-001 三向夹击。
3. 安全敞口两条需插队：FATDUCK git tracked 面（GOV-7/ROOT-006）+ fatduck-admin.yml 任意命令注入（W2-CI2-2）。
4. ARCH-001 迁移波高频移动 lib/*：ARCH/RT/LIB 系列行号锚已按锁点标注；迁移合入后请各轴验收门统一复跑（全只读）。
5. 门能红正样本（QA-2、W2-CI2-4）优先处置——它们同时是 CI-001 新聚合表的现成负例。

## 总量（终版）

- 24 轴全部交卷：第一波 10 轴 107 条（P0×13）+ 第二波 14 轴 137 条（P0×7）= **244 条发现、P0×20**；文件级穷尽 2398/2398 计划文件（含迁移双身与窗口新增 463 文件行，见 COVERAGE_MATRIX.md 终判）。
- 插队级：安全 3（fatduck-admin 命令注入 / FATDUCK git tracked+42 同型豁免缺口 / ACR 生产触达）+ 科学 3（SCI1-1 饱和位、SCI2-01/LIB3-2 CAR-AIT 恒等、LIB3-1 order_sel）+ 门失效群（CHK-PACKAGE/RESOURCE/ORACLE/INVARIANT 未注册、三 gate 只跑 selftest、run_checks 丢内容判决）。

- 与 ROOT-004 订正表交叉：本索引发现与 REBASE_TABLE（OPEN 728）逐条互标同源；读取顺序 SUMMARY → 本索引 → 各轴报告。