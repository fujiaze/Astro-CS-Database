# 分片报告 A_SCI_DEF_P1a（ROOT-004 旧 bug 清单按最新权威订正）

- 分片名：`A_SCI_DEF_P1a`（类别 A_SCI_DEF / 优先级 P1 / 分配 38 条：M1a-A-004..M7-A-119）
- 产物行数：PSV 39 行 = 表头 1 + finding 38（每条恰一行，行序 = 分配表顺序）
- 四态计数：OPEN 35 / RESOLVED 3 / VOID 0 / UNVERIFIABLE 0
- 基线：实测 `git rev-parse HEAD` = 2c328348304d033aecfa81faf79d1c6cd802b30a（任务书写的 ecf6ad6f 之后已有 2 个提交：5f891080 ROOT-004 订正、2c328348 GAP/CFG 登记）；判定一律以当前树为准。

## ID 覆盖自证（命令 + 输出）
- 集合一致：`diff <(awk -F'\t' 'NR>1{print $1}' .../_assign/A_SCI_DEF_P1a.tsv | sort) <(awk -F'|' 'NR>1{print $1}' .../A_SCI_DEF_P1a.psv | sort) && echo ID_SET_IDENTICAL`
  → `ID_SET_IDENTICAL`（rc=0）
- 行序一致：`paste <(awk -F'\t' 'NR>1{print $1}' _assign.tsv) <(awk -F'|' 'NR>1{print $1}' psv) | awk '$1!=$2{print "ORDER_MISMATCH",NR,$1,$2}'` → 无输出（无错位）
- 结构校验：`python3` 逐行 `count("|")==9` → `lines 39 / bad_rows [] nbad 0 / ids 38 uniq 38 / cols_ok True`；列内零 `|`、零换行。

## 判定口径（10 列，逐字见 SHARD_BRIEF §3）
- 旧判据列保留原 finding 实际引用的旧文档/旧路径（宪章 §…、旧 SCI/ALG ID、非链上 docs/standards、工程控制旧模板等）；最新权威列一律改用链上条款（ASTROCS_DESIGN §0/§11.1/§11.3、ENGINEERING_SPEC §2/§3/§5.1、FITS Paper I/II、UNIFIED_MODEL §2）并已 read/grep 核对节号与原文。
- 每条证据列均为本轮真跑命令 + 逐字输出（管道内省略处用 `…`）；集合类（零命中）用 `echo rc=$?` 给出 rc=1。

## RESOLVED（3 条，均为「文本已删/缺陷对象不存在」）
- `M7-A-102`：`grep -n 'x²·ivar' CONTROL_WEIGHT_SNR.md` rc=1；P5-SNR 订正后 §1 为 `w_snr=snr_v²`、§2a 另立帧级基准。
- `M7-A-103`：`grep -n '1e12' NOISE_MODEL.md` rc=1；§8 已改单一决策表（MAD=0/无合格 patch/全帧 NaN 饱和均 degenerate，ivar=0,r=1）。
- `M7-A-109`：`grep -rn '1.134' docs/science` rc=1；该常数仅存于 ALG 实现锚 `PHASE2_REJECTION.md:248`，SCI §7「流量中性」只指方法集不拒真值。

## 异常（不影响判定，已落 PSV 备注）
1. 行号漂移普遍存在：gaia_client.c 2266→2525、noise_model.cpp 394→403、dpsf_psf.cpp 389→411、ASTROMETRY.md 锚 :328-331 已非 cd_inv 块、DATA_SEMANTICS §30.4 2374→2435/2441、§28.4 2189→2257 等；本轮一律按当前树重定位。
2. 原 finding 引用的节名已随文档改版消失：M7-A-113 的「§n=1/§帧级/小样本」、M7-A-006 的「§6 MISSING」句、M7-A-102 的 §4 恒等式；缺陷本体（无适用域/历元混用/定义链唯一性）仍在者判 OPEN 并在备注写明。
3. 原判前提被本轮读码推翻：M3b-A-07「A_fit 饱和星公式已消失」不确——现树并存 A_fit(:1548/:1608) 与 box_sum(:2348)，故按同一缺陷（定义链不唯一）维持 OPEN。
4. M2a-A-2 的 BUNIT 零命中为逐文件 grep rc=1（writer 全文件无 BUNIT），非工具失败。
5. M5a-A-001 涉及 ACR（DORMANT，ASTROCS_DESIGN §1.3/§7.1）：无运行值影响，按冻结文档缺陷维持 P1/OPEN。

## UNVERIFIABLE 清单
- 无（38 条全部取得当前树可复跑证据）。

## 纪律自证
- 仅写 3 个文件：`reports/PROJECT-GOVERNANCE-01/root-scan/shards/A_SCI_DEF_P1a.psv`、同目录 `.md`、`run/PROJECT-GOVERNANCE-01/ROOT-004/logs/shards/A_SCI_DEF_P1a.log`；零修复、零 git 写、未触 `lib/cli/tests/contracts/ci/tools/docs/AGENTS.md` 等禁改面，未读取 `FATDUCK_ACCESS.md`。
- 全部外部命令带 `timeout`；文档检索未排除 docs/lib/tests（取证对象），仅按需限定路径。
