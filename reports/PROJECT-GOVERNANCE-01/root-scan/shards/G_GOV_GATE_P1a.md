# ROOT-004 分片执行报告 · G_GOV_GATE_P1a

- 分片名：G_GOV_GATE_P1a（类别 G_GOV_GATE，优先级 P1，分配 31 条）
- 产物：reports/PROJECT-GOVERNANCE-01/root-scan/shards/G_GOV_GATE_P1a.psv（表头 1 行 + 31 行，逐行 10 列）
- 行数：31；ID 区间 FD-G-001 … M5b-G-19，行序与 _assign/G_GOV_GATE_P1a.tsv 逐行一致
- 四态计数：OPEN 29 / RESOLVED 1 / VOID 1 / UNVERIFIABLE 0

## ID 覆盖自证（命令 + 逐字输出）
命令：timeout 60 python3 - <<'PYEOF'（比对 PSV 首列与分配表首列，含列数与四态统计）PYEOF

    psv_rows 31 assign_rows 31
    order_equal True set_equal True
    first_last FD-G-001 M5b-G-19
    states {'OPEN': 29, 'RESOLVED': 1, 'VOID': 1}
    unverifiable []
    cols_ok True

## 非 OPEN 结论
- RESOLVED｜FD-G-002：ci/known_failures.json 的 failures 现仅 1 条（p1_noise_adapter），UT-CLI 已在 removals 显式登记于 2026-09-14 移除，waivable=false 与 expected=fail 的矛盾对消失。
- VOID｜M5b-G-07：原判据依据旧文档 ASTROCS_PROJECT_CONSTITUTION.md §12.1 的 docs/owner 文件清单；最新替代为 ASTROCS_DESIGN.md §11.3/§12 与 docs/ci/01_CHECKS.md §2（27 个 CHK-* 无 L0 文档项），均不再要求该清单，故 check_l0_docs.py 校验 docs/review 不构成偏差。

## 异常
1. 基线漂移：派发提示词写 HEAD=main=ecf6ad6f，实测 HEAD=main=2c328348、origin/main=5f891080；本片一律按当前树取证。
2. 锚漂移：tools/check_domain_interfaces.py（M2a-G-1 原锚）、工程控制/AstroCS_V7_MODULAR_REFOUNDATION_CONTROL_20260902_FINAL3（FD-G-001 模板锚）已不存在；ci/checks.json 的 UT-CTEST 条目已消失；ci/known_failures.json 的 UT-CLI 条目已移除。
3. 检索陷阱：本仓 grep -r PATTERN .（点根递归）在 60s 内零输出（超时/管道缓冲丢输出），已改用显式目录列表或 git grep 重新取证；RADESYS、sampling_semantics、PHASE_OVERVIEW 三处结论均已按可靠口径复算。
4. 反证记录：pixel_sampling_semantics 仅出现在 lib/healpix_db/healpix_drizzle/v6_drizzle_science.h:325（非 phase3 产品面）；M5b-G-08 的「AGENTS.md 被门强制保留 REVIEW_PENDING」半句已不成立（新 AGENTS.md 0 命中），仍按检查器 REQUIRED 硬编码旧十组字符串、FORBIDDEN 空实现、当前 rc=1 判 OPEN。
5. FD-G-001 的对象在 run/**（被排除检索的巨目录），仅以 git ls-files 与 find -maxdepth 5 定界取证，未读取凭据文件。
6. 纪律：零修复、零 git 写（只用 git rev-parse/ls-files/log/grep 只读查询）；仅写本分片 .psv/.md 与 run/ 下日志；根条目未动。

## UNVERIFIABLE 清单
- 无：31 条均取得当前树可复跑命令证据（详见 .psv 第 6 列）。
