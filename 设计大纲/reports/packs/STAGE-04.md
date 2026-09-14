# STAGE-04 V4–V5 发布控制期（V4_CPU_ADAPTIVE / 自造 REAUDIT_V4 / V5 单 CLI / V5 时代审核件）

对应组：G06、G07。对应任务一历史阶段：STAGE-05（seq 991–1260，08-28→08-30，reports/history/00_OVERVIEW.md §1 表行 5）。

## 1. 时间窗
- 全代压缩在 08-28 一天内起步：REAUDIT_V4 PACKNEW seq 1037（020cdc99，20:36）→ seq 1038（ae643541，20:38）→ seq 1039 b12305ed（20:54，PACKDEL 自造包 + PACKNEW CONTROL_V4，"删一归一"同提交，pack_events.md 事实 1）→ seq 1042 f99e80d8（RELEASE_V5/V5 包引入 + artifacts/prerelease_v5 目录）。收束：prerelease_v5/AUDIT_REVIEW 08-30 首现（lineage 表）；b7b2dea7（09-02）V4/V5 归档；AUDIT_PACKAGE_587fe0e341a7.zip 对应基线 587fe0e3（08-30 17:59）。
- 生命周期极端值：REAUDIT_V4 18 分钟（P-G07 事实 4）。

## 2. 包清单（7+3 身份，含 G06 3 身份）
V4 三形态（zip f41aacec / 解包件 / recovered@b12305ed，26 文件逐字节一致、54 行全 NOT_STARTED；CONTROL_V4 为外层容器身份零自有文件）；V3 zip 对照件（G05/G06 重叠）；REAUDIT_V4__v4_reaudit + 工程控制__REAUDIT_V4（同一删除包的容器/内包两身份，23 件一致，digest 差 175 B=两份 _PROVENANCE.json）；V5_SINGLE_CLI_AMD64 两身份（37 文件×多形态；ALPHA 仅存在于 zip 文件名，P-G07 事实 2）；prerelease_v5__AUDIT_REVIEW(34)/audit_src(33)（重叠 33 文件、同 MANIFEST package=AstroCS-audit-59649edc0480）；AUDIT_PACKAGE_587fe0e341a7（zip 1699 条目、SHA256SUMS 1698 条全过，P-G08 衔接段实测）。

## 3. 共同设计意图（转述）
- V4（00_READ_FIRST、01_SCOPE_AND_WORKFLOW）：完全替代 V3；取消 CP0–CP8 逐次外审停工→连续执行+异步胶囊（12_REVIEW_CAPSULE_AND_EXTERNAL_AUDIT）；ACR GPU/Mixed 取消记 DEFERRED、validate_control.py:63-69 禁 RUN-003..006/ACR 号入台账——方向反转（对照 V3 CP6 强制三路）。
- REAUDIT_V4（自造包）：98 原子任务 + C0..C9 检查点链 + 强校验器（020cdc99 消息自述）；被 b12305ed 按用户指令移除（消息自述"前会话自造包，非用户上传物料…历史不重写仅从当前树移除"）。
- V5（00 §0）："本包取代 V3/V4 的未完成门禁"（V6 无同类条款，P-G08 §六）；12_V3_V4_MIGRATION.md"只继承实现，不继承 PASS"+8 项主题级失效；14_V4_COVERAGE_MATRIX 21 行承接矩阵。
- 用户指令痕迹：b12305ed 消息内"按用户指示…归位用户上传的 V4_CPU_ADAPTIVE 原包…执行账本仍为 V5 98 任务"——V4 在实测层面从未作为执行台账使用（P-G06 事实 3/4）。

## 4. 任务规模与状态字面量口径（前台实测已锁定）
- V4 54 行全 NOT_STARTED（7 值状态集含 DEFERRED/REVIEW_PENDING）→ V5 98 行（zip 全 NOT_STARTED → 归档终态 PASS 88/NOT_STARTED 7/IN_PROGRESS 1/BLOCKED 1/REVIEW_PENDING 1；6 值，DEFERRED 移除并在 00 §3 明文禁止）→ REAUDIT_V4 98 行（复原件 PASS 5/NOT_STARTED 93）。
- 台账表头 V4=V5 逐列相同（含 phase/preferred_host）；V5 多依赖"|"47 行 vs V4 9 行。
- V5 归档件两处 depends_on 改写（PAR-007 去 PAR-002、WIN-009→VER-001）与 52e9541d 提交消息自述一致（P-G07 事实 1）。

## 5. 门禁演进
- V5 校验器收紧：11 个必需短语、分支创建命令正则、SHA256SUMS 集合+逐条校验、任务数下限 85、alpha 版本正则、windows_32r_run_id（validate_control.py:96/107/109/123/61、validate_final_package.py:95/97/108）；资源门两代同式 0.80×min(workers,cpus)，V5 加前 10 秒快速失败（07 §3/§4）。
- 审核包：V4 仅最终一次 10/25 MiB → V5 capsule 体系（capsules/<task_id>_<commit12>.zip + REVIEW_CAPSULE_INDEX.csv）——V6 再收敛为单一 audit zip。
- 可复算性冲突：REAUDIT_V4 EVIDENCE_C0.md 自述 CONTROL_PASS sha 仅在台账 CRLF 态复现；V5 两个解包件跑自家校验均 CONTROL_FAIL（台账回写 vs 出厂 SHA256SUMS），只有 ALPHA zip CONTROL_PASS（4b441464…=BASELINE.json 的 zip_sha256，与 zip 本体 42a03c0b 两口径）（P-G07 事实 6）。

## 6. 与上一代差异
规模 62→54→98；停工模型 逐 Gate→连续执行；号族 18→14→23；"逐号点名废止"（V4 11 号）→"主题级废止"（V5 12 号）（P-G06/P-G07 §六全表）。

## 7. 执行留痕概况
- 命中率（pack_commit_link.csv）：V4/CONTROL_V4 96.3%（52/54）、REAUDIT_V4 两身份 0%、V5 两身份+AUDIT_REVIEW+audit_src 100%（379/337 提交口径）。
- 关键限定（P-G06 事实 4 / P-G07 事实 3）：V4 的 96.3% 系同号歧义（44 号与 V5 同号、24 号与 V6 同号；抽查 11 号无一可归属 V4 执行；evidence/artifacts 均无 V4 目录→"有账无据 54/54"）；REAUDIT_V4 的 0% 部分系抽取器不识别 C<d>-<ddd> 族（git log --all 字面 3/98，其中 2 条为包自身提交、1 条 F-C3-002 子串误命中）。
- V5 三方：一致 92（88 PASS 全有 capsule）、有账有提交零留痕 6（REL-001..004、REV-003、WIN-008）、有据无账 1（HOUSEKEEPING-V4-CORRECTION 胶囊）、盘上有胶囊未进 INDEX 2（BASE-001、GOV-001）（P-G07 事实 5）。

## 8. 与下一代衔接
V6 直接以 AUDIT_PACKAGE_587fe0e341a7 为基线（00:5、MANIFEST.baseline_commit、validate_control.py:93 三处硬绑），02_BASELINE_AUDIT 判其证据非单一快照（点名混入提交 59649ed，实测即该 zip 内 SUMMARY.json 的 commit，verdict RELEASE_NOT_READY_BLOCKED）→"打回"实物成立；但 V6 包内无"废止 V5"显式条款（P-G08 衔接段，记未证实）。

## 9. 缺口（P-G06 9 + P-G07 20 条）
最关键：_control_packs zip 全不入库→13 个 zip-only 身份无提交锚（pack_events.md 事实 4）；AUDIT_PACKAGE 的 code/ 内 2 文件与基线不一致（test_bench_cli.py 113 行版不在 git 全史任何对象）、MANIFEST total_bytes 差 697 B；dispatch_groups.json G07 条目 digests 数组重复列非 ALPHA digest（覆盖率核对时按盘面补齐）。
