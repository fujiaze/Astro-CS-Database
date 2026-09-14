# STAGE-03 审计-复审期（用户上传 V2/V3 审计包 + REAUDIT_V3 三件套 + CP0）

对应组：G05（主体；重叠身份亦 G06）。对应任务一历史阶段：STAGE-04（seq 716–990，08-16→08-27，reports/history/00_OVERVIEW.md §1 表行 4）。注：任务一的 STAGE-03（Phase2 v6→v19 迭代，seq 551–715）在本包谱系中无对应控制包身份——该窗口由开发版本号前缀族驱动（v6..v19），QA-V19R7"控制包"（history seq 735）不在 49 个包身份的取证域内（其形态非 marker 包件，见 pack_events.md 与控制包域对照）。

## 1. 时间窗
- 首事件：AstroCS_MAIN_AUDIT_SUPPLEMENT_V2_20260826（zip，08-26，未入库无提交锚，P-G05 事实 1）；CP0 zip 复原引入 08-27；REAUDIT_V3 三件套 PACKNEW seq 978（3703650d，08-27 23:08:54，"将外部审计目录中的工程控制/审核记录移入"）；收束 ac2ced53（seq 979，23:11:52，R100×333）→ 工程控制/REAUDIT_V3 身份最后触及 6a947f51（08-28 19:29）。
- LEDGER 事件起点即本代：seq 978 起 289 条中的首批 02_TASK_LEDGER 提交（CON-008/009 PASS、CON-010 FAIL=53a1cc04 等，structural_events.csv seq 983–986；pack_events.md 事实 5 指明前 15 身份在窗口前已废止）。

## 2. 包清单（9 身份）
V2 zip（2 文件/34,912 B 无台账）；AstroCS_MAIN_PRERELEASE_REAUDIT_CONTROL_V3_20260827 zip（21 文件/51,229 B，62×NOT_STARTED 空白发行版，未入 git）；AstroCS_CP0（2 zip 实例同 sha c79f50d7）；REAUDIT_V3__v3_audit / v3_cp0 / v3_reaudit（各 2 实例）、AstroCS_REAUDIT_V3_REVIEWPACK_20260828T1126Z（zip 46 文件）、工程控制__REAUDIT_V3（复原 356 文件含留痕）。四个 _control_packs zip 全部未被 git 跟踪（check-ignore 空、status ??、mtime 同为 09-04 18:51，P-G05 事实 1）。

## 3. 共同设计意图（转述）
- V2：证据补充请求（§3–§15 段落式进度、§1.3 十字段证据、禁事后调容差行 42-47；verdict EVIDENCE_COMPLETE/INCOMPLETE，P-G05 §三/§四）。
- V3（00_READ_FIRST）：62 任务 G0–G8 逐 Gate 停工＋CP0–CP8 复选框审核制；5 值状态（NOT_STARTED/IN_PROGRESS/PASS/FAIL/BLOCKED，00_READ_FIRST:19）；CPU≥150%、1T/2T≥1.50 利用率门（05_NUMERICAL_AND_PERFORMANCE_GATES.md:19-24）；CP6 强制 CPU/GPU/Mixed 三路（03_TASK_SPECIFICATIONS.md:219-225）——执行期被审核人改判（10_REVIEWER_DEVIATIONS.md，负责人指令痕迹）。
- v3_reaudit 与 REVIEWPACK：审核留痕的固化（CHECKPOINT_RESULTS/COMMITS.csv/25 份报告，P-G05 §七）。

## 4. 任务规模与状态字面量口径
- 62 号 / 18 族（ACR/ALG/API/ARCH/BLD/CHK/CON/DOC/HIPS/ID/ORA/PKG/RUN/SEAM/SYN/TST/WIN/REL）；台账 gate+checkpoint 两列。
- 三形态台账哈希各异（P-G05 关键发现 / P-G06 事实 5）：zip 3e14c578 全 NOT_STARTED；history@3703650d 0693c1b0 PASS 10/IN_PROGRESS 1/NOT_STARTED 51；archive 33693b75 PASS 47+FAIL 1+IN_PROGRESS 3+BLOCKED 4+NOT_STARTED 7（前台实测同此）。
- SUMMARY.json 自报 {PASS:8,BLOCKED:1,NOT_STARTED:53} 与同包台账 {PASS:10,...} 不符，verdict 字面量 G1_PASS_CP2_PARTIAL_COND006_BLOCKED 越出 SUMMARY.schema.json 三值枚举——包内校验器复算即报"self-accept"与"mismatch task_counts"（P-G05 事实 4）。

## 5. 门禁与验收
- 机器门：validate_control.py / package_audit.py / validate_audit_package.py（V3）；每 Gate 一包、≤25 MiB/单文件≤5 MiB（07_AUDIT_PACKAGE_SPEC.md:5-7）。
- REVIEWPACK 自门复算：12 项 REQUIRED 全缺 + 2 成员超 5 MiB（19,017,197 B / 6,564,246 B，P-G05 事实 5）；README 自述 PASS 47 vs 包内台账实测 45（差 RUN-003/004，工作树版 6a947f51 已记 47）。
- 引文断链：09_CONTINUOUS_EXECUTION_ORDER.md:3 引"00_READ_FIRST 规则 #12、#32"，V3 00 实测仅 12 条规则，#32 不存在（P-G05 附加发现）。

## 6. 与上一代差异
与 ACR 期无引用关系（§8 见下）；对照 STAGE-01/02 的"包=开发指导"，本代起"包=外部审计+复审指令"属性：首次出现 用户上传 zip 不进 git、规格与台账分形态漂移、审核人指令文件（09/10 号）混入包体的模式（P-G11 子包表：REAUDIT_V3 容器内件为终态、多 09/10 两个审核人指令文件）。

## 7. 执行留痕概况
- pack_commit_link.csv：三件套+工程控制/REAUDIT_V3 均 62/55=88.7%（194 提交）。方法局限（P-G05 §七）：全史命中 55 中仅 48 落在执行窗 08-27..08-29，窗外命中系后续代际同号复用。
- 三方分类：一致 50、有账无据 5（CHK-002/003/004、ACR-002/003）、有据无账 1（RUN-005）、账具皆无 6（RUN-006、SEAM-001、HIPS-001、REL-001/002、PKG-001）（P-G05 事实 6）。
- CP0 双形态校验相反（zip 17 条全 ok vs 解包 ok=1/16 不符），系 CRLF↔LF+末尾换行（P-G05 事实 3；digest_verify O-2 同型确认）。

## 8. 与下一代衔接
V4_CPU_ADAPTIVE 由 b12305ed（seq 1039，08-28 20:54）解包归位并明文"完全替代 V3"（00_READ_FIRST）；11_V3_INHERITANCE_AND_INVALIDATION.md 逐号点名废止 CON-006/008/009 PASS、CON-010 豁免、CHK-006、SCI-001/004、WIN-002、RUN-001..006（P-G06 衔接段）——本代与 STAGE-04 是明文继承+废止关系（对照 G04→G05 的零引用）。

## 9. 缺口（P-G05 共 13 条）
最关键：4 个 zip 无提交锚（首末提交"本仓库无解"）；V3 三形态台账进度并存且无权威标注；V2 的输入件 AstroCS_MAIN_FULL_AUDIT_EVIDENCE_REQUEST_V1.md 为外部未跟踪文件（identity.json:16 指认）；pack_lineage/inventory 对 REAUDIT_V3 系的删除提交记录不全（ac2ced53 R100 与 6a947f51 触达未入 dels 列）。
